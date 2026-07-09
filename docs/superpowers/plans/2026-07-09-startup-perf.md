# Startup Performance (Fast MIX Loading) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut the engine's measured 79 s cold start to single-digit seconds by block-reading MIX files and opening `ra2.mix` once per run, with loaded bytes provably identical to today's.

**Architecture:** A new pure-library function `ra2assets::read_file` (in `include/ra2assets/fileio.hpp` + `src/fileio.cpp`) replaces the engine's and `ra2extract`'s per-byte `std::istreambuf_iterator` slurps with a single seek-to-end-size-then-`read()` block read. `engine/main.cpp`'s load path is restructured so each mode entry opens `ra2.mix` (268 MB) exactly once and passes the open `MixFile` to both `load_assets()` and `load_unit_assets()` (today each opens it independently). Nothing downstream changes: MIX parsing, palettes, SHP/TMP decode, and the already-lazy tile decode (`Theater::tmp_for_tile` memoizes; `draw_terrain` decodes only on `TextureCache` miss) are untouched.

**Tech Stack:** C++17, CMake + Ninja + MinGW-w64 (host build at `D:\ra2-android\build`), doctest, PowerShell for all test/timing runs.

## Global Constraints

- C++17, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Pure library: no SDL/Win32/OS/DirectX includes anywhere under `include/ra2assets` or `src/` — `<fstream>`/`<filesystem>` (standard C++) are fine.
- No game assets, no game binary, no Ghidra project, no GPL code committed.
- Real-asset tests skip cleanly (doctest `MESSAGE` + `return`) when assets are absent. (This sub-project's unit tests need NO game assets at all — they use temp files.)
- Test executables MUST be run from PowerShell, not Git Bash (known environment issue).
- Every new `src/*.cpp` goes in `cmake/ra2_sources.cmake` `RA2_LIB_SOURCES`; every new `tests/*.cpp` goes in `CMakeLists.txt` `target_sources(ra2assets_tests ...)`.
- TDD: write the failing test first, watch it fail, then implement. (Scope:
  this applies to library code — Task 1. Tasks 2 and 3 change `engine/main.cpp`
  and `tools/ra2extract.cpp`, which are not compiled into `ra2assets_tests`;
  per the spec's sub-project-4 verification list they are gated behaviorally
  instead: full suite green + `--screenshot` scene check + measured timing.)
- Pristine test output — no stray prints, no warnings introduced.
- Commit after every task.

## Context for the implementer (measured ground truth)

- This is the one **non-RE** piece of the Foundation Batch (spec: `docs/superpowers/specs/2026-07-09-foundation-batch-design.md`, sub-project 4). There is **no Ghidra task**. The correctness gate is: identical bytes loaded, full suite stays green.
- Measured on the desktop host: `ra2engine --screenshot` full run = **79 s**. Cause: `engine/main.cpp`'s `slurp()` (lines ~49–53) reads files via `std::istreambuf_iterator` — byte-at-a-time with no reserve — and the load path reads `ra2.mix` (268 MB) **twice** (`load_assets` and `load_unit_assets` each call `slurp(dir + "ra2.mix")`) plus `ra2md.mix` (195 MB) once in `resolve_unit_rules` ≈ 730 MB of byte-wise reads.
- Tile decoding is **already lazy** (`Theater::tmp_for_tile` memoizes; `draw_terrain` decodes only on `TextureCache` miss) — **do not touch it**.
- The error message `"cannot open file: "` in the new `read_file` must match `engine/main.cpp`'s current slurp message **exactly** — the README's Android logcat diagnostics section (README.md ~line 215) documents that phrasing.
- After Task 2 the open `ra2.mix` `MixFile` lives for the duration of a mode run (~268 MB resident). This is an accepted tradeoff per the approved spec (previously the same bytes were held transiently — twice); on Android the same sources build into the APK via `cmake/ra2_sources.cmake`, no jni changes are needed, and `<fstream>` works under the NDK.
- All commands below run from `D:\ra2-android` in **PowerShell**. If `build/` is stale or missing, configure once first: `cmake -S . -B build -G Ninja`.

---

### Task 1: `ra2assets::read_file` — block-reading file slurp (TDD)

**Files:**
- Create: `D:\ra2-android\include\ra2assets\fileio.hpp`
- Create: `D:\ra2-android\src\fileio.cpp`
- Test: `D:\ra2-android\tests\test_fileio.cpp` (new)
- Modify: `D:\ra2-android\cmake\ra2_sources.cmake` (one line), `D:\ra2-android\CMakeLists.txt` (one line)

**Interfaces:**
- Consumes: nothing from the existing library — only standard `<fstream>`, `<filesystem>` (test only), `<stdexcept>`.
- Produces: `std::vector<uint8_t> ra2assets::read_file(const std::string& path)` — throws `std::runtime_error("cannot open file: " + path)` on open failure, `std::runtime_error("cannot stat file: " + path)` if `tellg()` fails, `std::runtime_error("short read: " + path)` on truncated read. Tasks 2 and 3 depend on this exact signature and the exact `"cannot open file: "` message prefix.

- [ ] **Step 1: Write the failing test** — create `D:\ra2-android\tests\test_fileio.cpp` with exactly this content:

```cpp
#include "doctest.h"
#include "ra2assets/fileio.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ra2assets;

// Writes `bytes` to a fresh binary file under the system temp dir and returns
// its path. Truncates any stale file left by a previous crashed run. The name
// gets a per-process random suffix so two concurrently running test suites
// (parallel CI jobs, two dev shells) cannot race each other on the same path.
static fs::path write_tmp_file(const std::string& name,
                               const std::vector<uint8_t>& bytes) {
    static const std::string suffix =
        "." + std::to_string(std::random_device{}());
    fs::path p = fs::temp_directory_path() / (name + suffix);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    REQUIRE(f.good());
    if (!bytes.empty())
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    f.close();
    REQUIRE(f.good());
    return p;
}

TEST_CASE("read_file: binary round-trip including DOS-EOF and CR/LF bytes") {
    // 0x1A (DOS EOF) truncates text-mode reads on Windows, and 0x0D 0x0A would
    // be collapsed by newline translation -- surviving them proves the file is
    // opened in binary mode.
    const std::vector<uint8_t> want = {0x00, 0x1A, 0xFF, 0x0D, 0x0A,
                                       0x1A, 0x00, 0x7F, 0x80, 0x0A,
                                       0x0D, 0x1A, 0xFE, 0x01, 0x1A};
    fs::path p = write_tmp_file("ra2_fileio_roundtrip.bin", want);
    std::vector<uint8_t> got = read_file(p.string());
    CHECK(got == want);
    fs::remove(p);
}

TEST_CASE("read_file: equals the old istreambuf_iterator slurp byte-for-byte") {
    // 4 KiB + an odd tail so the content is not block-aligned.
    std::vector<uint8_t> want(4096 + 37);
    for (size_t i = 0; i < want.size(); ++i)
        want[i] = static_cast<uint8_t>(i * 7 + 3);
    fs::path p = write_tmp_file("ra2_fileio_equiv.bin", want);

    // The OLD engine slurp, inline: byte-at-a-time via istreambuf_iterator.
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.good());
    std::vector<uint8_t> old_way((std::istreambuf_iterator<char>(f)), {});

    std::vector<uint8_t> new_way = read_file(p.string());
    CHECK(new_way == old_way);
    CHECK(new_way == want);
    fs::remove(p);
}

TEST_CASE("read_file: missing file throws runtime_error naming the path") {
    try {
        read_file("Z:\\no\\such\\file.bin");
        FAIL("read_file did not throw for a missing file");
    } catch (const std::runtime_error& ex) {
        // Message prefix is load-bearing: engine/main.cpp's old slurp threw
        // exactly "cannot open file: <path>" and README's Android logcat
        // diagnostics document that phrasing.
        CHECK(std::string(ex.what()).find("cannot open file") != std::string::npos);
        CHECK(std::string(ex.what()).find("file.bin") != std::string::npos);
    }
}

TEST_CASE("read_file: empty file returns an empty vector, no throw") {
    fs::path p = write_tmp_file("ra2_fileio_empty.bin", {});
    std::vector<uint8_t> got = read_file(p.string());
    CHECK(got.empty());
    fs::remove(p);
}
```

- [ ] **Step 2: Register the test in CMakeLists.txt** — in `D:\ra2-android\CMakeLists.txt`, apply this exact edit (old → new):

Old:
```cmake
target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)
```

New:
```cmake
target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_fileio.cpp)
```

- [ ] **Step 3: Run the build to verify the test fails** — the header does not exist yet, so the failing state is a compile error naming it:

```powershell
cmake --build build --target ra2assets_tests
```

Expected failure output contains:
```
tests/test_fileio.cpp:2:10: fatal error: ra2assets/fileio.hpp: No such file or directory
```
(non-zero exit code; Ninja reports `build stopped: subcommand failed.`)

- [ ] **Step 4: Write the header** — create `D:\ra2-android\include\ra2assets\fileio.hpp` with exactly this content:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ra2assets {
// Reads an entire binary file with a single block read (seek-to-end to
// size, one read()). Throws std::runtime_error("cannot open file: " + path)
// if the file cannot be opened, and ("short read: " + path) if fewer bytes
// arrive than the file reported. Replaces per-byte istreambuf_iterator
// slurps (measured ~79 s startup -> seconds; see docs/superpowers/specs/
// 2026-07-09-foundation-batch-design.md, sub-project 4).
std::vector<uint8_t> read_file(const std::string& path);
}
```

- [ ] **Step 5: Write the implementation** — create `D:\ra2-android\src\fileio.cpp` with exactly this content:

```cpp
#include "ra2assets/fileio.hpp"
#include <fstream>
#include <stdexcept>

namespace ra2assets {

// Single block read. The old engine slurp
// (std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {})) pulled one
// byte per iterator increment with no reserve; on the 268 MB ra2.mix (read
// TWICE) + 195 MB ra2md.mix that byte-wise path measured 79 s for
// `ra2engine --screenshot` on the host. Seek-to-end + one read() makes the
// same load I/O-bound (seconds). The "cannot open file: " message prefix
// matches the old engine slurp exactly -- README's Android logcat
// diagnostics depend on that phrasing.
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open file: " + path);
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) throw std::runtime_error("cannot stat file: " + path);
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(buf.data()), n);
        if (f.gcount() != n) throw std::runtime_error("short read: " + path);
    }
    return buf;
}

}  // namespace ra2assets
```

- [ ] **Step 6: Register the source in the shared source list** — in `D:\ra2-android\cmake\ra2_sources.cmake`, apply this exact edit (old → new):

Old:
```cmake
    ${RA2_ROOT}/src/theater.cpp
)
```

New:
```cmake
    ${RA2_ROOT}/src/theater.cpp
    ${RA2_ROOT}/src/fileio.cpp
)
```

(This single list feeds both the desktop build and the Android jni build — no other registration is needed for the APK.)

- [ ] **Step 7: Build and run the new tests to verify they pass**:

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="read_file*"
```

Expected: build succeeds (exit 0, no warnings from the new files), and the test run ends with:
```
[doctest] test cases:  4 |  4 passed | 0 failed | <n> skipped
[doctest] Status: SUCCESS!
```
(`<n>` = the other, filtered-out test cases. All 4 `read_file` cases pass; no `MESSAGE` noise.)

- [ ] **Step 8: Run the full suite to confirm nothing regressed**:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected final line:
```
[doctest] Status: SUCCESS!
```

- [ ] **Step 9: Commit**:

```powershell
git add include/ra2assets/fileio.hpp src/fileio.cpp tests/test_fileio.cpp cmake/ra2_sources.cmake CMakeLists.txt
git commit -m "feat: add ra2assets::read_file block-reading file slurp (TDD; replaces per-byte istreambuf reads)"
```

---

### Task 2: engine/main.cpp restructure — open ra2.mix once, block-read everything

**Files:**
- Modify: `D:\ra2-android\engine\main.cpp` (load path only)

**Interfaces:**
- Consumes: `std::vector<uint8_t> ra2assets::read_file(const std::string&)` (Task 1); `static MixFile MixFile::open(std::vector<uint8_t> bytes)`; `MixFile MixFile::open_nested(const std::string& name) const`; `const MixEntry* MixFile::find(uint32_t) const` / `const MixEntry* MixFile::find(const std::string&) const`; `std::vector<uint8_t> MixFile::read(const MixEntry&) const`; `uint32_t mix_id(const std::string&)`; `static Palette Palette::load(const uint8_t* data, size_t len)` (all from `include/ra2assets/mix.hpp` / `pal.hpp`, unchanged).
- Produces (engine-internal, used only inside `main.cpp`): `Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2)` and `UnitAssets load_unit_assets(const std::string& dir, const ra2assets::MixFile& ra2, const std::string& unit_id)`. No later task depends on these.

**Gate (state explicitly): this task has NO unit test** — `engine/main.cpp` is not compiled into the test binary. Its gate is: (a) `ra2engine` builds, (b) `--screenshot` renders the identical scene, (c) the AFTER wall time is single-digit seconds (BEFORE = 79 s, recorded above). All edits below are exact old → new text replacements; whitespace/indentation (4 spaces) must match exactly. Read `engine/main.cpp` fully before starting so each old block is located unambiguously.

- [ ] **Step 1: Capture the BEFORE screenshot and timing (baseline, pre-edit)** — one run of today's code (~79 s; give the command a 300000 ms timeout):

```powershell
cmake --build build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; Measure-Command { & .\build\ra2engine.exe --screenshot "$env:TEMP\perf_before.png" } | Select-Object TotalSeconds
```

Expected: stderr prints `[assets] theater=TEMPERATE size=103x110 cells=...`, `[unit] id=E1 image=GI ...`, `[screenshot] wrote ...perf_before.png (1280x720)`; `TotalSeconds` ≈ 75–85 (the recorded measurement is 79 s). Keep `perf_before.png` — Step 8 compares against it.

- [ ] **Step 2: Add the fileio include and drop the now-unused `<fstream>`** — two exact edits in `D:\ra2-android\engine\main.cpp`:

Edit 2a — old:
```cpp
#include "ra2assets/ini.hpp"
```
new:
```cpp
#include "ra2assets/fileio.hpp"
#include "ra2assets/ini.hpp"
```

Edit 2b — old:
```cpp
#include <cstdlib>
#include <fstream>
#include <iostream>
```
new:
```cpp
#include <cstdlib>
#include <iostream>
```
(`<fstream>` was only used by `slurp()`, deleted next. Nothing else in `main.cpp` uses it.)

- [ ] **Step 3: Delete the local `slurp()` helper** — exact edit:

Old:
```cpp
std::vector<uint8_t> slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open file: " + p);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

std::string env_install_dir() {
```

New:
```cpp
std::string env_install_dir() {
```

(`ra2assets::read_file` throws the same `"cannot open file: " + path` message, so the README's logcat diagnostics stay accurate.)

- [ ] **Step 4: Replace `load_assets` in full** — exact edit (the old block is the entire current function):

Old:
```cpp
Assets load_assets() {
    const std::string dir = env_install_dir();

    // Map: read the temperate reference map INI out of mapsmd03.mix.
    MixFile maps = MixFile::open(slurp(dir + "mapsmd03.mix"));
    const MixEntry* me = maps.find(0x133a0cb2u);
    if (!me) throw std::runtime_error("map entry 0x133a0cb2 not found in mapsmd03.mix");
    auto map_raw = maps.read(*me);
    MapFile map = MapFile::parse(std::string(map_raw.begin(), map_raw.end()));

    const std::string theater = map.theater();
    if (theater != "TEMPERATE") {
        // Milestone 1 wires temperate only; other theaters map the same way
        // (isosnow.mix/snow.ini/.sno, isourb.mix/urban.ini/.urb).
        throw std::runtime_error("unsupported theater (only TEMPERATE wired): " + theater);
    }

    // ra2.mix holds the nested theater/config/palette mixes.
    MixFile ra2 = MixFile::open(slurp(dir + "ra2.mix"));

    // Palette: isotem.pal lives in cache.mix.
    MixFile cache = ra2.open_nested("cache.mix");
```

New:
```cpp
// `ra2` is the already-open ra2.mix (holds the nested theater/config/palette
// mixes) -- opened ONCE by each mode entry point and shared with
// load_unit_assets(), instead of each loader slurping the 268 MB file
// independently (was 79 s cold start; see docs/superpowers/specs/
// 2026-07-09-foundation-batch-design.md, sub-project 4).
Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2) {
    // Map: read the temperate reference map INI out of mapsmd03.mix.
    MixFile maps = MixFile::open(ra2assets::read_file(dir + "mapsmd03.mix"));
    const MixEntry* me = maps.find(0x133a0cb2u);
    if (!me) throw std::runtime_error("map entry 0x133a0cb2 not found in mapsmd03.mix");
    auto map_raw = maps.read(*me);
    MapFile map = MapFile::parse(std::string(map_raw.begin(), map_raw.end()));

    const std::string theater = map.theater();
    if (theater != "TEMPERATE") {
        // Milestone 1 wires temperate only; other theaters map the same way
        // (isosnow.mix/snow.ini/.sno, isourb.mix/urban.ini/.urb).
        throw std::runtime_error("unsupported theater (only TEMPERATE wired): " + theater);
    }

    // Palette: isotem.pal lives in cache.mix.
    MixFile cache = ra2.open_nested("cache.mix");
```

(The rest of the function body — `isotem.pal` lookup, `local.mix`/`temperat.ini`, `isotemp.mix`, the `Theater` construction, the `[assets]` stderr line, and the `return Assets{...};` — is byte-for-byte unchanged; it already refers to `ra2`, `cache`, `local`, `isotemp` by name. For reference, the complete resulting function is the new block above followed by the unchanged remainder:)

```cpp
    const MixEntry* pe = cache.find(std::string("isotem.pal"));
    if (!pe) throw std::runtime_error("isotem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    Palette pal = Palette::load(pal_raw.data(), pal_raw.size());

    // Tile-set control INI: temperat.ini lives in local.mix.
    MixFile local = ra2.open_nested("local.mix");
    const MixEntry* ce = local.find(std::string("temperat.ini"));
    if (!ce) throw std::runtime_error("temperat.ini not found in ra2.mix/local.mix");
    auto ini_raw = local.read(*ce);
    std::string ini_text(ini_raw.begin(), ini_raw.end());

    // Tiles: the .tem templates live in isotemp.mix.
    MixFile isotemp = ra2.open_nested("isotemp.mix");

    ra2engine::Theater th(std::move(isotemp), ini_text, "tem");

    std::cerr << "[assets] theater=" << theater << " size=" << map.width() << "x"
              << map.height() << " cells=" << map.cells().size()
              << " templates=" << th.template_count() << "\n";

    return Assets{std::move(map), pal, std::move(th)};
}
```

- [ ] **Step 5: Swap `resolve_unit_rules`'s slurp (signature unchanged — it reads ra2md.mix, a separate file)** — exact edit:

Old:
```cpp
    MixFile top = MixFile::open(slurp(dir + "ra2md.mix"));
```

New:
```cpp
    MixFile top = MixFile::open(ra2assets::read_file(dir + "ra2md.mix"));
```

- [ ] **Step 6: Replace `load_unit_assets`'s prologue (takes the open ra2.mix)** — exact edit:

Old:
```cpp
UnitAssets load_unit_assets(const std::string& dir, const std::string& unit_id) {
    const UnitRules ur = resolve_unit_rules(dir, unit_id);
    const std::string image = ur.image;

    MixFile ra2 = MixFile::open(slurp(dir + "ra2.mix"));

    MixFile conquer = ra2.open_nested("conquer.mix");
```

New:
```cpp
// `ra2` is the already-open ra2.mix shared with load_assets() (opened once by
// the mode entry points -- previously this function slurped the 268 MB file a
// second time).
UnitAssets load_unit_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                            const std::string& unit_id) {
    const UnitRules ur = resolve_unit_rules(dir, unit_id);
    const std::string image = ur.image;

    MixFile conquer = ra2.open_nested("conquer.mix");
```

(The rest of `load_unit_assets` — the lowercase `.shp` fallback, `ShpFile::load`, `unittem.pal` from `cache.mix`, the `[unit]` stderr line, and `return UnitAssets{...};` — is byte-for-byte unchanged. The complete resulting function is the new block above followed by:)

```cpp
    std::string shp_name = image + ".shp";
    const MixEntry* se = conquer.find(shp_name);
    if (!se) {
        // conquer.mix's stock entry names are lowercase (e.g. "gi.shp"); try
        // as-is first (in case a mod/layout differs), then lowercased.
        std::string lower = shp_name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        se = conquer.find(lower);
        shp_name = lower;
    }
    if (!se)
        throw std::runtime_error("unit SHP not found in ra2.mix/conquer.mix: " + image + ".shp");
    ShpFile shp = ShpFile::load(conquer.read(*se));

    MixFile cache = ra2.open_nested("cache.mix");
    const MixEntry* pe = cache.find(std::string("unittem.pal"));
    if (!pe) throw std::runtime_error("unittem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    Palette pal = Palette::load(pal_raw.data(), pal_raw.size());

    std::cerr << "[unit] id=" << unit_id << " image=" << image << " shp=" << shp_name
              << " frames=" << shp.count() << " canvas=" << shp.width() << "x" << shp.height()
              << " speed=" << ur.speed << "\n";

    return UnitAssets{std::move(shp), pal, image, ur.speed};
}
```

- [ ] **Step 7: Update all four mode entry prologues (open ra2.mix once; call `env_install_dir()` once — on Android it logs the asset dir each call, and once is correct)** — four exact edits. Note the three-line `Assets a = load_assets(); ...` pattern appears in FOUR functions, so each old block below includes preceding lines that make it unique.

Edit 7a — `run_screenshot`, old:
```cpp
int run_screenshot(const std::string& out_path) {
    const int W = 1280, H = 720;
    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);
```
new:
```cpp
int run_screenshot(const std::string& out_path) {
    const int W = 1280, H = 720;
    // ra2.mix opened ONCE and shared by both loaders; the buffer stays
    // resident for the run (accepted tradeoff, see the load_assets comment).
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua);
```

Edit 7b — `run_interactive`, old:
```cpp
int run_interactive() {
    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);
```
new:
```cpp
int run_interactive() {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua);
```

Edit 7c — `run_demo_move`, old:
```cpp
    const std::string end_png = scratch + "task12_end.png";

    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);
```
new:
```cpp
    const std::string end_png = scratch + "task12_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua);
```

Edit 7d — `run_demo_retarget`, old:
```cpp
int run_demo_retarget(int x1, int y1, int x2, int y2) {
    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);
```
new:
```cpp
int run_demo_retarget(int x1, int y1, int x2, int y2) {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua);
```

- [ ] **Step 8: Build the engine, capture the AFTER screenshot + timing, and verify the identical scene**:

```powershell
cmake --build build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; Measure-Command { & .\build\ra2engine.exe --screenshot "$env:TEMP\perf_after.png" } | Select-Object TotalSeconds
(Get-Item "$env:TEMP\perf_after.png").Length
(Get-FileHash "$env:TEMP\perf_before.png").Hash -eq (Get-FileHash "$env:TEMP\perf_after.png").Hash
```

Expected:
- Build succeeds (exit 0; no new warnings).
- The run prints the SAME stderr lines as Step 1 (`[assets] theater=TEMPERATE size=103x110 ...`, `[unit] id=E1 image=GI ...`, `[screenshot] wrote ...`) — same counts, since only the load I/O changed.
- `TotalSeconds` is **single-digit** (expect roughly 2–8 s).
- PNG `Length` is nonzero and comparable to `perf_before.png` (hundreds of KB — a blank frame would compress to a few KB).
- The hash comparison prints `True` (identical bytes loaded → identical render). **Fallback:** if it prints `False` (GPU rasterization can in principle differ run-to-run), open BOTH PNGs with the Read tool and confirm they show the same isometric temperate map with the GI unit — visual identity is the gate then; record which gate was used.
- **Record the exact BEFORE (Step 1) and AFTER TotalSeconds numbers** — they go in this task's commit message, the README note in Task 3, and the plan-execution report.

- [ ] **Step 9: Run the full test suite (library untouched, must stay green)**:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected final line: `[doctest] Status: SUCCESS!`

- [ ] **Step 10: Commit** (replace `<AFTER>` with the measured Step-8 whole seconds, e.g. `4.2`):

```powershell
git add engine/main.cpp
git commit -m "perf: open ra2.mix once and block-read MIX files in engine load path

--screenshot cold start on host: 79 s before -> <AFTER> s after
(Measure-Command). Scene verified identical (PNG hash / visual compare).
load_assets/load_unit_assets now take the open MixFile; tile decode
(already lazy) untouched."
```

---

### Task 3: ra2extract slurp swap, README startup note, final verification

**Files:**
- Modify: `D:\ra2-android\tools\ra2extract.cpp`
- Modify: `D:\ra2-android\README.md` (one short paragraph in "Engine (desktop)")

**Interfaces:**
- Consumes: `std::vector<uint8_t> ra2assets::read_file(const std::string&)` (Task 1).
- Produces: nothing new — behavior note: a missing input mix now surfaces as `error: cannot open file: <path>` (thrown by `read_file`, caught by `main`'s existing handler) instead of the old `cannot read <path>`; exit code stays 1 either way. An existing-but-empty input still hits the `bytes.empty()` → `cannot read` path.

- [ ] **Step 1: Swap ra2extract's slurp for read_file** — three exact edits in `D:\ra2-android\tools\ra2extract.cpp` (keep `#include <fstream>` — `std::ofstream` still writes `--raw` output):

Edit 1a — old:
```cpp
#include "ra2assets/mix.hpp"
```
new:
```cpp
#include "ra2assets/fileio.hpp"
#include "ra2assets/mix.hpp"
```

Edit 1b — old:
```cpp
static std::vector<uint8_t> slurp(const std::string& p){
    std::ifstream f(p,std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});
}

// Descend a nested-mix chain: "cache.mix" or "a.mix/b.mix".
```
new:
```cpp
// Descend a nested-mix chain: "cache.mix" or "a.mix/b.mix".
```

Edit 1c — old:
```cpp
        auto bytes=slurp(argv[1]);
```
new:
```cpp
        auto bytes=read_file(argv[1]);
```
(`using namespace ra2assets;` at the top of the file makes the unqualified call resolve to `ra2assets::read_file`.)

- [ ] **Step 2: Build ra2extract and verify it still works (and is now fast on ra2.mix)**:

```powershell
cmake --build build --target ra2extract
& .\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --list
& .\build\ra2extract.exe "Z:\no\such\file.mix" --list
```

Expected:
- Build succeeds (exit 0).
- The `--list` of `ra2.mix` prints `entries: <N>` (N > 0) followed by entry lines, completing in a couple of seconds (previously each invocation spent ~20 s just slurping the 268 MB file byte-wise — the tool is now usable for batched oracle extraction).
- The missing-file invocation prints `error: cannot open file: Z:\no\such\file.mix` and exits with code 1 (check with `$LASTEXITCODE` → `1`).

- [ ] **Step 3: Add the startup note to the README** — exact edit in `D:\ra2-android\README.md`, replacing `<AFTER>` with the AFTER whole-seconds number measured in Task 2 Step 8 (also recorded in Task 2's commit message) before saving:

Old:
```markdown
Interactive controls: tap/click the unit to select it (green marker); click a
```
New:
```markdown
Startup: MIX files are block-read (`ra2assets::read_file`) and `ra2.mix` is
opened once per run; cold start dropped from ~79 s to ~<AFTER> s on the host
(measured via `Measure-Command { ra2engine --screenshot }`). The same sources
build into the Android APK via `cmake/ra2_sources.cmake` — no jni changes.

Interactive controls: tap/click the unit to select it (green marker); click a
```

- [ ] **Step 3b: Update the README's logcat example filename** — after the Task 2
restructure, a fully missing Android asset dir surfaces first as
`cannot open file: .../ra2.mix` (the mode entries open `ra2.mix` before
`load_assets` reads `mapsmd03.mix`), so make the README's example match what
users will actually see. Exact edit in `D:\ra2-android\README.md` (the
"Diagnosing failures" paragraph in the Android section):

Old:
```markdown
   and any fatal error is logged as `fatal: <reason>` (e.g.
   `cannot open file: .../mapsmd03.mix`) before it exits — so
```
New:
```markdown
   and any fatal error is logged as `fatal: <reason>` (e.g.
   `cannot open file: .../ra2.mix`) before it exits — so
```

- [ ] **Step 4: Final full-suite verification (all three deliverables in place)**:

```powershell
cmake --build build --target ra2assets_tests
cmake --build build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected: both targets build with exit 0; the suite's final line is `[doctest] Status: SUCCESS!` with zero failures and pristine output.

- [ ] **Step 5: Commit**:

```powershell
git add tools/ra2extract.cpp README.md
git commit -m "perf: block-read ra2extract input via ra2assets::read_file; document startup speedup in README"
```

---

## Done criteria (whole plan)

- `ra2assets::read_file` exists in the pure library (no OS includes), fully unit-tested with synthetic temp files (round-trip with 0x1A/CR/LF bytes, old-slurp equivalence, missing-file throw containing `cannot open file`, empty-file empty vector), tests self-clean their temp files.
- `engine/main.cpp` opens `ra2.mix` exactly once per mode run and contains no `istreambuf_iterator` slurp; `tools/ra2extract.cpp` likewise.
- `--screenshot` renders the identical scene (PNG hash or visual compare vs the pre-change baseline) in single-digit seconds; BEFORE (79 s) / AFTER numbers recorded in the Task 2 commit message, the README, and the plan-execution report.
- Full suite green; `ra2engine` and `ra2extract` build; no game assets, binaries, or GPL code committed; three commits, one per task.
