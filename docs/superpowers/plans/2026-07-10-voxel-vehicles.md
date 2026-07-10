# Voxel Vehicles (Rhino Full Unit) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Rhino heavy tank as a complete unit on both sides — hull + turret + barrel rendered from the real voxel assets via a pure rasterizer with load-time facing caching, driven with the original's ROT turning, fighting with the original 120mm math.

**Architecture:** Task 1 recovers the original's voxel projection/lighting and Drive-locomotor semantics from gamemd.exe (Ghidra, content-anchored). Then: a pure `ra2assets::Vpl` loader → a pure `engine/voxel_raster` module (all recovered math) → `engine/facing_cache` (N facings × 3 parts × 2 houses, built once at load) → vehicle state in `world`/`combat` (continuous body/turret facing, ROT turning, turret-aim fire gating) → `main.cpp` wiring + `--demo-tank`.

**Tech Stack:** C++17, CMake+Ninja+MinGW (host), doctest, SDL2 (engine only), Ghidra headless at `D:\ra2-re`.

## Global Constraints

- GPL-clean: XCC/OpenRA/YRpp/ModEnc are read OFF-REPO (`D:\ra2-re\reference`) and only prose conclusions may be committed. No GPL code in the repo.
- No game assets, no `gamemd.exe`, no Ghidra project files committed. RE docs are prose + short decompiler excerpts. Numeric tables extracted from the binary (normal tables, projection constants) are factual data and MAY be committed as C++ arrays (precedent: R250 constants, damage formula).
- Tests must skip cleanly (doctest `MESSAGE` + `return`) when `RA2_INSTALL_DIR` is unset AND the default `D:\Red Alert 2` is absent. Guarded tests use the existing `tests/install_dir.hpp` helper pattern (see `tests/test_vxl.cpp`).
- Pure modules (no SDL/OS) are compiled into BOTH `ra2assets_tests` and `ra2engine`; library sources go in `cmake/ra2_sources.cmake` `RA2_LIB_SOURCES`, engine-pure sources are added to both targets in `CMakeLists.txt` (pattern: `engine/combat.cpp`).
- Run test/demo executables from **PowerShell**, never Git Bash (exit-code/console issues — see repo README).
- Work directly on `master`; commit after every task step-pair (test+impl); every commit message ends with `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- `--demo-combat` byte-identity must remain unchanged by every task (no tanks spawn there; no changes to infantry code paths' behavior).
- Sim determinism: NOTHING in this phase introduces RNG into the sim. The rasterizer/caches are deterministic; locomotion math is closed-form.
- Build dir: `D:\ra2-android\build` (already configured). Rebuild: `cmake --build build` from repo root. Full suite: `.\build\ra2assets_tests.exe` from PowerShell.
- Scratch files (screenshots, oracle dumps) go to the session scratchpad, never the repo.

## [RE-GATE] Plan amendment gate (same mechanism as the audio phase)

Tasks 4, 5, and 6 contain constants and semantic choices marked
`[RE: voxel-render.md §N]`. They are written from pre-hunt assumptions so the
plan is complete, but **the controller MUST reconcile them against Task 1's
`docs/re/voxel-render.md` and commit plan amendments BEFORE dispatching each
affected task**. If the hunt contradicts an assumption, the plan text is
amended (and the amendment committed) first; the implementer never sees the
stale version. Items the hunt cannot resolve get an `[RE-FALLBACK]` tag in
the RE doc naming the off-repo source used.

---

### Task 1: Ghidra voxel-render + locomotion hunt → docs/re/voxel-render.md

**Files:**
- Create: `docs/re/voxel-render.md` (the ONLY repo deliverable)
- Create (off-repo, not committed): `D:\ra2-re\scripts\voxel_render_hunt*.py`

**Interfaces:**
- Produces: `docs/re/voxel-render.md` with numbered sections §1–§7 (below), a
  confidence table, a **"Decisions for ra2engine"** section resolving every
  open choice in Tasks 4–6, and the two normal tables (§3) as copy-pasteable
  C++ float arrays.

**Context.** `D:\ra2-re` holds a Ghidra 11 project (`ra2.gpr`) over YR 1.001
`gamemd.exe`, with `analyzeHeadless.bat` workflow scripts from prior hunts in
`D:\ra2-re\scripts` (see `audio_hunt*.py` for the run pattern: block-read
string search → `getReferencesTo` → targeted `decompile` of candidates →
dump to `D:\ra2-re\out`). Content-anchor ONLY — YRpp/Ares hardcoded addresses
have been proven unreliable on this binary. Known-good anchors from
`docs/re/vxl.md` (read it first): the VXL loader `FUN_00754bd0`, HVA loader
`FUN_005bcff0`, the untraced matrix repack helper `FUN_005ae010` (reused by
both loaders — almost certainly feeds the renderer), the VoxLib bootstrap
`FUN_005313c0` and its caller `FUN_0052b790` (which loads `voxels.vpl` — the
filename string is an anchor), and the per-object filename builders
(`FUN_005f7280`/`FUN_005f75a0` build `<Image>TUR.VXL`/`<Image>BARL.VXL`).

Local ground truth to verify against (already extracted this session):
htnk.vxl 46×34×12 normals_mode=2, htnktur.vxl 28×18×9 normals_mode=2,
htnkbarl.vxl 19×3×3 **normals_mode=4** (so BOTH normal tables are needed),
voxels.vpl = 8,976 B = 16 header + 768 palette + 32×256 table bytes.
rulesmd: [HTNK] Speed=6 ROT=5 Turret=yes; [120mm] Range=5.75 ROF=65
Damage=90 Warhead=AP Speed=40; [AP] CellSpread=.3 PercentAtMax=.5;
artmd: [HTNK] Voxel=yes Remapable=yes PrimaryFireFLH=150,0,100.

**Recover, with a confidence row per item:**

- **§1 Projection.** The voxel→screen transform: the fixed camera/view matrix
  the voxel rasterizer uses, how section `scale` (tailer det_scale), tailer
  `transform[3][4]`, `min_bounds/max_bounds`, and the HVA matrix compose into
  a voxel-center world position, and the world→screen scale relating model
  units to pixels at the game's 60×30 cell metric (leptons-per-voxel-unit and
  px-per-lepton, or the equivalent combined constants). Deliverable: exact
  formulas `screen_x = f(model x,y,z)`, `screen_y = g(model x,y,z)` plus a
  depth ordering rule. Start from `FUN_005ae010` (the shared matrix repack)
  and the VoxLib singleton's draw path; the rasterizer also holds the span
  `0xFFFFFFFF` empty-column test docs/re/vxl.md could not locate — finding
  that walk confirms you're in the right function.
- **§2 Facing quantization.** How many pre-rendered orientations a vehicle
  body gets, and a turret (expected 32; find the actual constant — look for
  the facing→index quantization near the draw path or the cache that stores
  per-facing renders), plus the facing index → rotation angle mapping and
  which index is which on-screen direction (calibration rule like
  infantry-anim.md §4).
- **§3 Normal tables + lighting.** The mode-2 (TS, expected 36 entries) and
  mode-4 (RA2, expected 244 entries) normal vector tables as float triples —
  dump the static data blocks and INCLUDE BOTH TABLES in the doc as C++
  arrays (`constexpr float kNormals2[36][3]`, `kNormals4[244][3]`). Then the
  lighting rule: how a voxel's normal index + the scene light direction
  produce a vpl table row (the dot-product → level scaling, its clamp, which
  of the 32 vpl sections is "unlit"/"full").
- **§4 voxels.vpl + palette + house remap.** The 16-byte header field
  meanings (expected: u32 remap_start, remap_end, section_count, unused);
  whether the embedded 768-byte vpl palette or a theater unit palette
  (`unittem.pal`) is what the final indices index at draw time; WHERE
  `unittem.pal` lives (which mix — it is NOT in ra2.mix/local.mix; check
  cache.mix — the engine already loads it from `ra2.mix/cache.mix` for SHP
  units, confirm voxels use the same); and whether voxel house remap is the
  SHP scheme (palette indices 16–31 swapped per house) or driven by the vpl
  remap_start/remap_end range.
- **§5 Drive locomotion.** `Speed=6` → leptons-per-frame (the UnitTypeClass
  speed conversion — find the table/formula; deliver cells/sec at 15 Hz);
  `ROT=5` semantics (FacingClass turn rate — expected: ROT is scaled into
  16-bit facing units per frame; deliver facing-units-per-frame and thus
  degrees/sec); whether Drive turns the hull in place before moving and the
  alignment tolerance for starting to roll; turret slew rate (same ROT? a
  separate constant?) and idle return-to-body behavior.
- **§6 FLH fire point.** `PrimaryFireFLH=F,L,H` semantics (leptons,
  forward/lateral/height in TURRET space rotated by turret facing) → screen
  offset for the muzzle flash/Report origin.
- **§7 Projectile + splash deltas.** `Projectile=Cannon` `Speed=40` flight
  model (leptons/frame? straight-line?) and `[AP] CellSpread=.3
  PercentAtMax=.5` damage falloff (which cells a 0.3-cell spread actually
  reaches). End §7 with an explicit recommendation: implement or document-
  as-divergence for this phase (a 0.3-cell spread that only ever reaches the
  target cell supports keeping single-target damage).

**"Decisions for ra2engine"** section must explicitly resolve each
`[RE: voxel-render.md §N]` marker in Tasks 4–6 (grep this plan for the
marker before writing the doc).

- [ ] **Step 1:** Read `docs/re/vxl.md`, `docs/re/workflow.md`, and two prior hunt scripts in `D:\ra2-re\scripts` for the headless pattern.
- [ ] **Step 2:** Write + run the hunt scripts (iterate as needed; dump decompiled candidates to `D:\ra2-re\out`). Verify §3's table sizes against the local ground truth above (36/244 entries, unit-length vectors).
- [ ] **Step 3:** Write `docs/re/voxel-render.md` (§1–§7 + confidence table + Decisions for ra2engine + both normal tables).
- [ ] **Step 4:** Commit:

```bash
git add docs/re/voxel-render.md
git commit -m "docs(re): voxel rendering + drive locomotion semantics from gamemd.exe

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: ra2assets::Vpl (voxels.vpl loader)

**Files:**
- Create: `include/ra2assets/vpl.hpp`, `src/vpl.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `src/vpl.cpp` to `RA2_LIB_SOURCES`), `CMakeLists.txt` (register `tests/test_vpl.cpp`)
- Test: `tests/test_vpl.cpp`

**Interfaces:**
- Consumes: `ra2assets::Palette` (`pal.hpp`), `docs/re/voxel-render.md` §4 (header layout).
- Produces: `struct Vpl { uint8_t remap_start, remap_end; uint32_t section_count; Palette palette; std::vector<std::array<uint8_t,256>> tables; static Vpl load(const uint8_t*, size_t); }` — Task 4 consumes `tables`, Task 6 loads the real file.

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/test_vpl.cpp
#include "doctest.h"
#include "ra2assets/vpl.hpp"
#include "install_dir.hpp"
#include <cstdint>
#include <vector>
// (plus the mix-chain includes used by test_vxl.cpp's real-asset test)

namespace {
// 16-byte header (remap_start, remap_end, section_count, unused as u32 LE),
// 768 palette bytes, then n*256 table bytes.
std::vector<uint8_t> synth_vpl(uint32_t sections) {
    std::vector<uint8_t> v(16 + 768 + sections * 256, 0);
    auto put32 = [&](size_t at, uint32_t x) {
        v[at] = x & 0xff; v[at+1] = (x >> 8) & 0xff;
        v[at+2] = (x >> 16) & 0xff; v[at+3] = (x >> 24) & 0xff;
    };
    put32(0, 16);   // remap_start
    put32(4, 31);   // remap_end
    put32(8, sections);
    // palette: entry 1 = raw (10, 20, 30) -> <<2 = (40, 80, 120)
    v[16 + 3] = 10; v[16 + 4] = 20; v[16 + 5] = 30;
    // table 0 entry 5 = 42; last table's entry 255 = 7
    v[16 + 768 + 5] = 42;
    v[16 + 768 + (sections - 1) * 256 + 255] = 7;
    return v;
}
}  // namespace

TEST_CASE("vpl: synthetic parse") {
    auto v = synth_vpl(32);
    ra2assets::Vpl p = ra2assets::Vpl::load(v.data(), v.size());
    CHECK(p.remap_start == 16);
    CHECK(p.remap_end == 31);
    CHECK(p.section_count == 32);
    CHECK(p.tables.size() == 32);
    CHECK(p.palette[1].r == 40);   // 6-bit <<2 expansion, like .pal
    CHECK(p.palette[1].g == 80);
    CHECK(p.palette[1].b == 120);
    CHECK(p.tables[0][5] == 42);
    CHECK(p.tables[31][255] == 7);
}

TEST_CASE("vpl: truncation and bad counts throw") {
    auto v = synth_vpl(32);
    CHECK_THROWS_AS(ra2assets::Vpl::load(v.data(), 15), std::runtime_error);       // no header
    CHECK_THROWS_AS(ra2assets::Vpl::load(v.data(), v.size() - 1), std::runtime_error);  // short table
    auto z = synth_vpl(0);   // section_count == 0
    CHECK_THROWS_AS(ra2assets::Vpl::load(z.data(), z.size()), std::runtime_error);
    auto big = synth_vpl(2);  // header claims 2 but hand it a huge count
    big[8] = 0xff; big[9] = 0xff; big[10] = 0xff; big[11] = 0xff;
    CHECK_THROWS_AS(ra2assets::Vpl::load(big.data(), big.size()), std::runtime_error);
}

TEST_CASE("vpl: real voxels.vpl pins") {
    // Same guarded pattern as test_vxl.cpp's real-asset test: resolve the
    // install dir, MESSAGE+return if absent. voxels.vpl lives in
    // ra2.mix -> local.mix (verified in-session: 8,976 bytes).
    const std::string dir = ra2test::install_dir();
    if (dir.empty()) { MESSAGE("RA2_INSTALL_DIR not set and default install absent; skipping"); return; }
    // open ra2.mix -> local.mix -> voxels.vpl exactly as test_vxl.cpp opens htnk.vxl
    // (copy its mix-chain boilerplate), then:
    //   ra2assets::Vpl p = ra2assets::Vpl::load(bytes.data(), bytes.size());
    // and pin:
    //   CHECK(bytes.size() == 16 + 768 + p.section_count * 256);
    //   CHECK(p.section_count == 32);
    //   CHECK(p.tables.size() == 32);
    // Identity spot-check (voxel-render.md §3, CORRECTED post-Task-2 vs the
    // real file): ROW 8 is the base/unmodified row ((d+s)=0.5), 199 fixed
    // points measured; CHECK(fixed points of tables[8] > 150). Row 16 is NOT
    // neutral (13 fixed points). [Task 2 already landed exactly this pin.]
}
```

(The real-asset test's mix-chain boilerplate is intentionally described, not
duplicated — copy it verbatim from `tests/test_vxl.cpp`, which the implementer
must read anyway for the guarded-skip pattern. Everything asserted is spelled
out above.)

- [ ] **Step 2: Run to verify failure**

Run (PowerShell): `cmake --build build --target ra2assets_tests; .\build\ra2assets_tests.exe -tc="vpl:*"`
Expected: compile FAILURE (`ra2assets/vpl.hpp` not found).

- [ ] **Step 3: Implement**

```cpp
// include/ra2assets/vpl.hpp
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "ra2assets/pal.hpp"

namespace ra2assets {

// Decoded voxels.vpl: the shared voxel lighting remap the game applies at
// draw time (docs/re/voxel-render.md §4). Layout (byte-verified against the
// real 8,976-byte file): u32 remap_start, u32 remap_end, u32 section_count,
// u32 unused; 768 bytes of 6-bit palette (expanded <<2 like .pal); then
// section_count 256-byte lookup tables (table[level][color_index] ->
// final palette index).
struct Vpl {
    uint8_t remap_start = 0;
    uint8_t remap_end = 0;
    uint32_t section_count = 0;
    Palette palette;
    std::vector<std::array<uint8_t, 256>> tables;

    // Throws std::runtime_error (with context) on: len < 16, section_count
    // of 0 or > 256, or len != 16 + 768 + section_count*256 exactly.
    static Vpl load(const uint8_t* data, size_t len);
};

}  // namespace ra2assets
```

```cpp
// src/vpl.cpp
#include "ra2assets/vpl.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace ra2assets {

namespace {
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

Vpl Vpl::load(const uint8_t* data, size_t len) {
    if (len < 16) throw std::runtime_error("vpl: truncated header (" + std::to_string(len) + " bytes)");
    Vpl v;
    const uint32_t rs = rd32(data), re = rd32(data + 4), sc = rd32(data + 8);
    if (sc == 0 || sc > 256)
        throw std::runtime_error("vpl: implausible section_count " + std::to_string(sc));
    const size_t want = 16 + 768 + static_cast<size_t>(sc) * 256;
    if (len != want)
        throw std::runtime_error("vpl: size mismatch: have " + std::to_string(len) +
                                 ", layout wants " + std::to_string(want));
    v.remap_start = static_cast<uint8_t>(rs);
    v.remap_end = static_cast<uint8_t>(re);
    v.section_count = sc;
    v.palette = Palette::load(data + 16, 768);   // same 6-bit <<2 path as .pal
    v.tables.resize(sc);
    for (uint32_t i = 0; i < sc; ++i)
        std::memcpy(v.tables[i].data(), data + 16 + 768 + static_cast<size_t>(i) * 256, 256);
    return v;
}

}  // namespace ra2assets
```

Add `${RA2_ROOT}/src/vpl.cpp` to `RA2_LIB_SOURCES` in `cmake/ra2_sources.cmake`
(after `src/vxl.cpp`); add `target_sources(ra2assets_tests PRIVATE tests/test_vpl.cpp)`
in `CMakeLists.txt` (after the `test_vxl.cpp` line).

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target ra2assets_tests; .\build\ra2assets_tests.exe -tc="vpl:*"`
Expected: all `vpl:` cases PASS (real-file case passes with the install present, skips cleanly without).

- [ ] **Step 5: Commit**

```bash
git add include/ra2assets/vpl.hpp src/vpl.cpp tests/test_vpl.cpp cmake/ra2_sources.cmake CMakeLists.txt
git commit -m "feat(assets): voxels.vpl loader (header + palette + 32 lighting tables)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: Rules/Art getters for vehicles

**Files:**
- Modify: `include/ra2assets/rules.hpp`, `src/rules.cpp`, `include/ra2assets/art.hpp`, `src/art.cpp`
- Test: `tests/test_rules.cpp`, `tests/test_art.cpp` (extend)

**Interfaces:**
- Consumes: existing `Rules`/`Art` (thin Ini wrappers).
- Produces (Task 6 consumes all of these):
  - `double Rules::weapon_range(const std::string&) const` — **CHANGED from int to double** ([120mm] Range=5.75 truncates today). Callers already store into `UnitCombat::range` (a double); update the two int pins in tests.
  - `int Rules::rot(const std::string& unit_id) const` — `[unit] ROT=`, 0 if absent.
  - `bool Rules::has_turret(const std::string& unit_id) const` — `[unit] Turret=yes` (case-insensitive "yes"/"true"), false if absent.
  - `std::string Rules::explosion_first(const std::string& unit_id) const` — first comma-separated entry of `[unit] Explosion=`, "" if absent.
  - `bool Art::voxel(const std::string& image) const` — `[image] Voxel=yes`, false if absent.
  - `bool Art::remapable(const std::string& image) const` — `[image] Remapable=yes`, false if absent.
  - `bool Art::fire_flh(const std::string& image, int flh[3]) const` — `[image] PrimaryFireFLH=F,L,H` into flh, false (flh untouched) if absent/malformed.

- [ ] **Step 1: Write the failing tests** — extend the existing synthetic-Ini test cases in `tests/test_rules.cpp` / `tests/test_art.cpp` (follow their local style):

```cpp
// tests/test_rules.cpp (add)
TEST_CASE("rules: vehicle getters") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[HTNK]\nROT=5\nTurret=yes\nExplosion=TWLT070,S_BANG48\n"
        "[UTNK]\nTurret=no\n"
        "[120mm]\nRange=5.75\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.rot("HTNK") == 5);
    CHECK(r.rot("NOPE") == 0);
    CHECK(r.has_turret("HTNK"));
    CHECK_FALSE(r.has_turret("UTNK"));
    CHECK_FALSE(r.has_turret("NOPE"));
    CHECK(r.explosion_first("HTNK") == "TWLT070");
    CHECK(r.explosion_first("NOPE") == "");
    CHECK(r.weapon_range("120mm") == doctest::Approx(5.75));
}
```

```cpp
// tests/test_art.cpp (add)
TEST_CASE("art: voxel getters") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[HTNK]\nVoxel=yes\nRemapable=yes\nPrimaryFireFLH=150,0,100\n"
        "[GI]\nCameo=GIICON\n");
    ra2assets::Art a = ra2assets::Art::load(ini);
    CHECK(a.voxel("HTNK"));
    CHECK_FALSE(a.voxel("GI"));
    CHECK(a.remapable("HTNK"));
    int flh[3] = {-1, -1, -1};
    CHECK(a.fire_flh("HTNK", flh));
    CHECK(flh[0] == 150); CHECK(flh[1] == 0); CHECK(flh[2] == 100);
    int flh2[3] = {-1, -1, -1};
    CHECK_FALSE(a.fire_flh("GI", flh2));
    CHECK(flh2[0] == -1);
}
```

Also extend the existing REAL-install pin cases (both files have one) with:
`CHECK(r.rot("HTNK") == 5); CHECK(r.has_turret("HTNK")); CHECK(r.explosion_first("HTNK") == "TWLT070"); CHECK(r.weapon_range("120mm") == doctest::Approx(5.75));`
and `CHECK(a.voxel("HTNK")); CHECK(a.remapable("HTNK")); /* fire_flh pins 150,0,100 */`.

- [ ] **Step 2: Run to verify failure** — `cmake --build build --target ra2assets_tests` → compile error (`rot` not a member).

- [ ] **Step 3: Implement.** In `src/rules.cpp`, follow the existing getter style exactly (they all call the same private Ini-get helpers). `weapon_range`: change return type to `double`, parse with `std::strtod` instead of the int path; fix its two test pins (`tests/test_rules.cpp` — M60 Range=4 becomes `doctest::Approx(4.0)`). `has_turret`/`voxel`/`remapable`: lowercase the value, true iff `"yes"` or `"true"`. `explosion_first`: take the substring before the first `','`, trimmed. `fire_flh`: split on commas, require exactly 3 int-parseable fields. Update the doc comments in both headers (mirroring the existing per-getter comment style).

- [ ] **Step 4: Run to verify pass** — `.\build\ra2assets_tests.exe -tc="rules:*,art:*"` → PASS (with and without install).

- [ ] **Step 5: Commit**

```bash
git add include/ra2assets/rules.hpp src/rules.cpp include/ra2assets/art.hpp src/art.cpp tests/test_rules.cpp tests/test_art.cpp
git commit -m "feat(assets): Rules/Art vehicle getters (ROT, Turret, Explosion, Voxel, FLH; Range now double)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: engine/voxel_raster — the pure rasterizer

**[RE-GATE: APPLIED 2026-07-10]** — every marker below has been reconciled
against Task 1's docs/re/voxel-render.md "Decisions for ra2engine" and the
code blocks amended (projection basis + axis swap, corner-anchored voxels,
depth rule, 2×1 footprint, kNormals4[245], diffuse+specular lighting with a
yaw-rotated world light + level clamp, index_to_yaw offset). Remaining
`[RE: §N]` strings are citations, not open items.

**Files:**
- Create: `engine/voxel_raster.hpp`, `engine/voxel_raster.cpp`, `engine/voxel_normals.inc`
- Modify: `CMakeLists.txt` (compile `engine/voxel_raster.cpp` into both targets; register `tests/test_voxel_raster.cpp`)
- Test: `tests/test_voxel_raster.cpp`

**Interfaces:**
- Consumes: `ra2assets::VxlModel/VxlSection` (vxl.hpp), `ra2assets::HvaModel` (hva.hpp), `ra2assets::Vpl` (Task 2), `ra2assets::Palette`.
- Produces (Tasks 5–6 consume):
  - `struct VoxelSprite { int w = 0, h = 0; int anchor_x = 0, anchor_y = 0; std::vector<uint8_t> rgba; }` — anchor is the model-origin ("ground center") pixel within the sprite.
  - `VoxelSprite rasterize_model(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva, double yaw_rad, const ra2assets::Vpl& vpl, const ra2assets::Palette& pal)` — renders ALL sections of one model (frame 0) rotated by `yaw_rad` about the model Z axis.
  - `int facing_to_index(double facing_u16, int n)` and `double index_to_yaw(int i, int n)` — the facing-quantization pair `[RE: voxel-render.md §2]`.
  - `engine/voxel_normals.inc` defining `kNormals2[36][3]` and `kNormals4[245][3]` (copied verbatim from voxel-render.md §3 — mode 4 has 245 entries, not 244; entries 240–244 are identical padding).

**The math (AMENDED post-Task-1 from docs/re/voxel-render.md "Decisions for
ra2engine" — these are the recovered values, no longer assumptions):**

1. Voxel (ix,iy,iz) position in section-local units — CORNER-ANCHORED, no
   half-voxel offset (voxel-render.md §1):
   `local = min_bounds + i * (max_bounds - min_bounds) / size` per axis.
2. Section transform: `world = hva_matrix_3x4 * local`, with the HVA
   TRANSLATION column pre-multiplied by the section `scale` (det_scale)
   (voxel-render.md §1, CONFIRMED — `FUN_005BD160` applies it at load; doing
   it per call is equivalent).
3. Yaw: rotate `world` about +Z by `yaw_rad`.
4. Screen (voxel-render.md §1, recovered basis Rx(−60°)·Rz(−45°); note the
   axis roles vs the old placeholder are SWAPPED):
   `sx = (wx + wy) * kIsoX; sy = (wx - wy) * kIsoY - wz * kIsoZ;`
   with `kIsoX = 0.70711, kIsoY = 0.35355, kIsoZ = 0.86603`.
   1 voxel unit = 1 px; do NOT scale by cell metrics inside the rasterizer.
5. Depth (voxel-render.md §1, third basis row):
   `depth = 0.61237 * (wx - wy) + 0.5 * wz`, larger = nearer,
   strictly-greater z-buffer test.
6. Lighting (voxel-render.md §3, recovered formula — diffuse + specular):
   with `L_model = Rz(-yaw) * (-0.7071, -0.7071, 0)` (the light is
   WORLD-fixed, so it rotates into model space per rasterize call — it is
   NOT a global constant dotted with unrotated normals) and
   `H_model = normalize(L_model + (0,0,1))`:
   `d = max(0, dot(n, L_model)); h = max(0, dot(n, H_model)); s = h / (3 - 2*h);`
   `level = clamp(ftol((d + s) * 16.0), 0, vpl.section_count - 1)`
   (the clamp is REQUIRED: d+s can reach ~2.0 → 32 without it — reviewer
   finding on the RE doc). Normal indices 253–255 are forced to level 16
   (a hard-wired marker level; row 8 — not 16 — is the base/identity row per
   the corrected §3). `final_index = vpl.tables[level][v.color]`,
   RGBA = `pal[final_index]`, alpha 255. Normal table per section by
   `normals_mode` (2 → kNormals2[36], 4 → kNormals4[245]; any other mode →
   treat as 4). Out-of-range normal indices clamp to the table's last entry.
7. Sprite bounds: project all 8 corners of each section's transformed
   bounding box, take the pixel AABB over all sections plus a 1px pad (+1px
   right for the footprint below); `anchor = -aabb_min` (model origin lands
   at (anchor_x, anchor_y)).
8. Footprint (voxel-render.md §1, RESOLVED): each voxel plots exactly
   2 horizontal pixels (2×1 — px and px+1 on one row), matching vanilla's
   pinhole behavior. No `ceil(kIsoX)` square.

**House remap is NOT a rasterizer parameter:** callers pass an
already-remapped `Palette` (reuse `pal_for_house` from `iso_renderer.hpp` —
it remaps indices 16–31, matching artmd `Remapable=yes`)
(voxel-render.md §4, CONFIRMED: draw-time colors index the unit palette
through the same per-house 16–31 ramp as SHP; the vpl's embedded palette and
the VXL's embedded palette are NOT consulted at draw time, and vpl
remap_start/remap_end drive nothing).

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/test_voxel_raster.cpp
#include "doctest.h"
#include "voxel_raster.hpp"
#include "ra2assets/vxl.hpp"
#include "ra2assets/hva.hpp"
#include "ra2assets/vpl.hpp"
#include <cmath>

using ra2assets::VxlModel;
using ra2assets::VxlSection;
using ra2assets::HvaModel;
using ra2assets::HvaSectionXform;

namespace {
// One 1x1x1-section model holding a single voxel with the given color/normal,
// identity transform, bounds centered on the origin (so the voxel center IS
// the model origin), normals_mode 2.
VxlModel single_voxel_model(uint8_t color, uint8_t normal) {
    VxlModel m;
    VxlSection s;
    s.name = "T";
    s.size_x = s.size_y = s.size_z = 1;
    s.scale = 1.0f;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) s.transform[r][c] = (r == c) ? 1.f : 0.f;
    for (int a = 0; a < 3; ++a) { s.min_bounds[a] = -0.5f; s.max_bounds[a] = 0.5f; }
    s.normals_mode = 2;
    s.voxels = { {color, normal, true} };
    m.sections.push_back(s);
    return m;
}
HvaModel identity_hva() {
    HvaModel h;
    h.frame_count = 1;
    h.section_names = {"T"};
    HvaSectionXform x{};
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) x.transform[r][c] = (r == c) ? 1.f : 0.f;
    h.frames = {{x}};
    return h;
}
// Identity-ish vpl: every table row is the identity map, palette gray ramp.
ra2assets::Vpl identity_vpl() {
    std::vector<uint8_t> raw(16 + 768 + 32 * 256, 0);
    raw[8] = 32;  // section_count
    for (int i = 0; i < 256; ++i) {
        raw[16 + i * 3 + 0] = raw[16 + i * 3 + 1] = raw[16 + i * 3 + 2] =
            static_cast<uint8_t>(i / 4);  // 6-bit gray ramp
        for (int t = 0; t < 32; ++t) raw[16 + 768 + t * 256 + i] = static_cast<uint8_t>(i);
    }
    return ra2assets::Vpl::load(raw.data(), raw.size());
}
ra2assets::Palette gray_pal() {
    // Palette::load expands <<2; feed the same gray ramp for consistency.
    uint8_t raw[768];
    for (int i = 0; i < 256; ++i) raw[i*3] = raw[i*3+1] = raw[i*3+2] = static_cast<uint8_t>(i / 4);
    return ra2assets::Palette::load(raw, 768);
}
int opaque_count(const ra2engine::VoxelSprite& s) {
    int n = 0;
    for (size_t i = 3; i < s.rgba.size(); i += 4) if (s.rgba[i] != 0) ++n;
    return n;
}
}  // namespace

TEST_CASE("voxel_raster: single voxel lands on the anchor") {
    auto spr = ra2engine::rasterize_model(single_voxel_model(100, 0), identity_hva(),
                                          0.0, identity_vpl(), gray_pal());
    REQUIRE(spr.w > 0);
    REQUIRE(spr.h > 0);
    CHECK(opaque_count(spr) >= 1);
    // The voxel center is the model origin, so its pixels must cover the
    // anchor point (within the 1-voxel footprint).
    bool anchor_covered = false;
    for (int dy = -2; dy <= 2 && !anchor_covered; ++dy)
        for (int dx = -2; dx <= 2 && !anchor_covered; ++dx) {
            const int x = spr.anchor_x + dx, y = spr.anchor_y + dy;
            if (x < 0 || y < 0 || x >= spr.w || y >= spr.h) continue;
            if (spr.rgba[(static_cast<size_t>(y) * spr.w + x) * 4 + 3] != 0) anchor_covered = true;
        }
    CHECK(anchor_covered);
}

TEST_CASE("voxel_raster: yaw rotation moves an off-axis voxel as hand-computed") {
    // Two-voxel column along model +X: bounds [0,2] x [0,1] x [0,1], 2x1x1.
    // Corner-anchored positions (voxel-render.md §1): voxel (1,0,0) sits at
    // local (1, 0, 0) exactly -- pure +X, so its projected offset from the
    // anchor is sign-definite under the recovered basis (no y/z bleed).
    VxlModel m = single_voxel_model(100, 0);
    m.sections[0].size_x = 4;
    m.sections[0].min_bounds[0] = 0.0f; m.sections[0].max_bounds[0] = 4.0f;
    m.sections[0].min_bounds[1] = 0.0f; m.sections[0].max_bounds[1] = 1.0f;
    m.sections[0].min_bounds[2] = 0.0f; m.sections[0].max_bounds[2] = 1.0f;
    m.sections[0].voxels = { {0, 0, false}, {0, 0, false}, {0, 0, false},
                             {100, 0, true} };  // only x=3 present: local (3, 0, 0)
    auto at0 = ra2engine::rasterize_model(m, identity_hva(), 0.0, identity_vpl(), gray_pal());
    auto at180 = ra2engine::rasterize_model(m, identity_hva(), M_PI, identity_vpl(), gray_pal());
    // At yaw 0 the voxel is at world (3,0,0) -> sx = +2.12, sy = +1.06 px
    // (down-right, screen East -- whole-pixel offsets survive rounding); at
    // yaw pi it is (-3,0,0) -> sx = -2.12, sy = -1.06. Find each sprite's
    // opaque centroid relative to its anchor and check the sign flip.
    auto centroid_rel = [](const ra2engine::VoxelSprite& s, double& cx, double& cy) {
        double sx = 0, sy = 0; int n = 0;
        for (int y = 0; y < s.h; ++y)
            for (int x = 0; x < s.w; ++x)
                if (s.rgba[(static_cast<size_t>(y) * s.w + x) * 4 + 3] != 0) { sx += x; sy += y; ++n; }
        REQUIRE(n > 0);
        cx = sx / n - s.anchor_x;
        cy = sy / n - s.anchor_y;
    };
    double x0, y0, x1, y1;
    centroid_rel(at0, x0, y0);
    centroid_rel(at180, x1, y1);
    CHECK(x0 > 0.0);       // +X model axis projects right-and-down (diamond shear)
    CHECK(y0 > 0.0);
    CHECK(x1 < 0.0);       // opposite yaw mirrors both
    CHECK(y1 < 0.0);
}

TEST_CASE("voxel_raster: normal index selects the vpl row") {
    // A vpl whose table t maps EVERY color to t (so the output pixel value
    // reveals which lighting level was chosen).
    std::vector<uint8_t> raw(16 + 768 + 32 * 256, 0);
    raw[8] = 32;
    for (int i = 0; i < 256; ++i) {
        raw[16 + i * 3] = raw[16 + i * 3 + 1] = raw[16 + i * 3 + 2] = static_cast<uint8_t>(i / 4);
        for (int t = 0; t < 32; ++t) raw[16 + 768 + t * 256 + i] = static_cast<uint8_t>(t * 8);
    }
    auto vpl = ra2assets::Vpl::load(raw.data(), raw.size());
    // Normals 0 and 18 of kNormals2 point in visibly different directions
    // [RE: voxel-render.md §3 -- pick two indices the doc's table shows have
    // opposite-signed dot products with kLightDir, and hand-compute both
    // expected levels + resulting gray values here].
    auto a = ra2engine::rasterize_model(single_voxel_model(1, 0), identity_hva(), 0.0, vpl, gray_pal());
    auto b = ra2engine::rasterize_model(single_voxel_model(1, 18), identity_hva(), 0.0, vpl, gray_pal());
    auto first_opaque_r = [](const ra2engine::VoxelSprite& s) -> int {
        for (size_t i = 3; i < s.rgba.size(); i += 4) if (s.rgba[i] != 0) return s.rgba[i - 3];
        return -1;
    };
    const int ra = first_opaque_r(a), rb = first_opaque_r(b);
    REQUIRE(ra >= 0);
    REQUIRE(rb >= 0);
    CHECK(ra != rb);  // different normals -> different lighting rows
}

TEST_CASE("voxel_raster: facing quantization round-trips") {
    const int n = 32;  // [RE: voxel-render.md §2]
    CHECK(ra2engine::facing_to_index(0.0, n) == 0);
    CHECK(ra2engine::facing_to_index(65536.0 / n * 5, n) == 5);
    CHECK(ra2engine::facing_to_index(65536.0 - 1.0, n) == 0);  // wraps to nearest
    for (int i = 0; i < n; ++i)
        CHECK(ra2engine::facing_to_index(65536.0 * i / n, n) == i);
}

TEST_CASE("voxel_raster: real htnk hull renders deterministically") {
    // Guarded (tests/install_dir.hpp pattern). Load htnk.vxl + htnk.hva +
    // voxels.vpl + unittem.pal via the same chains test_vxl.cpp uses, then:
    //   auto s1 = rasterize_model(m, hva, 0.0, vpl, pal);
    //   auto s2 = rasterize_model(m, hva, 0.0, vpl, pal);
    //   CHECK(s1.rgba == s2.rgba);          // byte-deterministic
    //   CHECK(opaque_count(s1) > 200);      // a 46x34x12 tank is not a speck
    //   CHECK(s1.w < 200); CHECK(s1.h < 200);  // and not exploded
    // Then FNV-1a hash s1.rgba and MESSAGE() it. After the Task 6 visual
    // check confirms the render matches the original game, the fix round
    // pins the hash here as a CHECK (regression lock). Until then keep the
    // MESSAGE only.
}
```

- [ ] **Step 2: Run to verify failure** — compile error (no `voxel_raster.hpp`).

- [ ] **Step 3: Implement `engine/voxel_raster.{hpp,cpp}` + `engine/voxel_normals.inc`**

```cpp
// engine/voxel_raster.hpp
#pragma once
#include <cstdint>
#include <vector>
#include "ra2assets/hva.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/vpl.hpp"
#include "ra2assets/vxl.hpp"

namespace ra2engine {

// One pre-rendered voxel-model sprite. `anchor_x/anchor_y` is the pixel the
// MODEL ORIGIN projects to -- the renderer places that pixel on the unit's
// ground point (tile center), exactly like the SHP canvas-center convention
// in draw_unit (iso_renderer.hpp).
struct VoxelSprite {
    int w = 0, h = 0;
    int anchor_x = 0, anchor_y = 0;
    std::vector<uint8_t> rgba;  // w*h*4, alpha 0 = empty
};

// Projection constants (docs/re/voxel-render.md §1, recovered basis
// Rx(-60 deg)*Rz(-45 deg)): screen px per rotated-model unit. 1 voxel
// unit = 1 px -- no cell-metric scaling inside the rasterizer.
constexpr double kIsoX = 0.70711;   // sx = (wx + wy) * kIsoX
constexpr double kIsoY = 0.35355;   // sy = (wx - wy) * kIsoY - wz * kIsoZ
constexpr double kIsoZ = 0.86603;
// World-fixed light direction (voxel-render.md §3): rotated into MODEL
// space per rasterize call (L_model = Rz(-yaw) * kLightWorld), never dotted
// with normals directly as a constant.
constexpr double kLightWorld[3] = {-0.7071, -0.7071, 0.0};

// Facing quantization (docs/re/voxel-render.md §2): `facing_u16` is a
// facing in the original's 16-bit space [0, 65536); n is the cached facing
// count. Nearest-index with wraparound.
int facing_to_index(double facing_u16, int n);
// The yaw angle (radians about model +Z) to rasterize facing index i at.
double index_to_yaw(int i, int n);

// Rasterize every section of `m` (using HVA frame 0's per-section matrices,
// translation scaled by each section's `scale`) rotated by yaw_rad about +Z,
// lit via the normal tables + `vpl`, colored through `pal` (pass a
// pal_for_house-remapped palette for house colors). Deterministic; pure.
VoxelSprite rasterize_model(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                            double yaw_rad, const ra2assets::Vpl& vpl,
                            const ra2assets::Palette& pal);

}  // namespace ra2engine
```

```cpp
// engine/voxel_raster.cpp
#include "voxel_raster.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ra2engine {
namespace {
#include "voxel_normals.inc"   // kNormals2[36][3], kNormals4[244][3] (voxel-render.md §3)

struct V3 { double x, y, z; };

// hva 3x4 (rows) applied to p, translation column pre-scaled by `scale`
// [RE: voxel-render.md §1].
V3 apply_xform(const float t[3][4], float scale, const V3& p) {
    return {
        t[0][0] * p.x + t[0][1] * p.y + t[0][2] * p.z + t[0][3] * scale,
        t[1][0] * p.x + t[1][1] * p.y + t[1][2] * p.z + t[1][3] * scale,
        t[2][0] * p.x + t[2][1] * p.y + t[2][2] * p.z + t[2][3] * scale,
    };
}

V3 yaw_rotate(const V3& p, double c, double s) {
    return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
}

void project(const V3& w, double& sx, double& sy, double& depth) {
    // voxel-render.md §1: basis Rx(-60)*Rz(-45); depth = third row.
    sx = (w.x + w.y) * kIsoX;
    sy = (w.x - w.y) * kIsoY - w.z * kIsoZ;
    depth = 0.61237 * (w.x - w.y) + 0.5 * w.z;
}

// voxel-render.md §3: level = ftol((d + s) * 16), d = diffuse, s = specular
// h/(3-2h); L/H are the light dir already rotated into MODEL space by the
// caller (world-fixed light). Indices 253-255 -> hard-wired level 16 (slightly brightened; row 8 is base). Clamp to
// section_count-1 (d+s can reach ~2.0).
int light_level(uint8_t normal_ix, uint8_t mode, uint32_t levels,
                const double L[3], const double H[3]) {
    if (normal_ix >= 253) return std::min<int>(16, static_cast<int>(levels) - 1);
    const float(*tab)[3] = (mode == 2) ? kNormals2 : kNormals4;
    const int tab_n = (mode == 2) ? 36 : 245;
    const int ix = std::min<int>(normal_ix, tab_n - 1);
    const double d = std::max(0.0, tab[ix][0] * L[0] + tab[ix][1] * L[1] + tab[ix][2] * L[2]);
    const double h = std::max(0.0, tab[ix][0] * H[0] + tab[ix][1] * H[1] + tab[ix][2] * H[2]);
    const double s = h / (3.0 - 2.0 * h);
    const int lv = static_cast<int>((d + s) * 16.0);  // ftol = truncate
    return std::clamp(lv, 0, static_cast<int>(levels) - 1);
}
}  // namespace

int facing_to_index(double facing_u16, int n) {
    const double per = 65536.0 / n;
    const int i = static_cast<int>(std::floor(facing_u16 / per + 0.5));
    return ((i % n) + n) % n;
}

double index_to_yaw(int i, int n) {
    // voxel-render.md §2 (AMENDED): yaw(i) = (i - n/4) * (-2*pi/n).
    // Index n/4 (facing 0x4000, screen East) is the unrotated model (+X
    // nose points down-right); increasing facing rotates CLOCKWISE on
    // screen. For n=32: yaw(8)=0, yaw(0)=+pi/2 (North).
    return (i - n / 4) * (-2.0 * M_PI / n);
}

VoxelSprite rasterize_model(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                            double yaw_rad, const ra2assets::Vpl& vpl,
                            const ra2assets::Palette& pal) {
    const double c = std::cos(yaw_rad), s = std::sin(yaw_rad);

    // Pass 1: bounds. Project each section's 8 transformed bbox corners.
    double mnx = 1e9, mny = 1e9, mxx = -1e9, mxy = -1e9;
    for (size_t si = 0; si < m.sections.size(); ++si) {
        const auto& sec = m.sections[si];
        const auto& xf = hva.frames.at(0).at(std::min(si, hva.frames[0].size() - 1));
        for (int corner = 0; corner < 8; ++corner) {
            const V3 lo = {sec.min_bounds[0], sec.min_bounds[1], sec.min_bounds[2]};
            const V3 hi = {sec.max_bounds[0], sec.max_bounds[1], sec.max_bounds[2]};
            const V3 p = {(corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y,
                          (corner & 4) ? hi.z : lo.z};
            double sx, sy, d;
            project(yaw_rotate(apply_xform(xf.transform, sec.scale, p), c, s), sx, sy, d);
            mnx = std::min(mnx, sx); mxx = std::max(mxx, sx);
            mny = std::min(mny, sy); mxy = std::max(mxy, sy);
        }
    }
    VoxelSprite out;
    if (m.sections.empty() || mnx > mxx) return out;

    const int pad = 2;  // 1px pad + the voxel footprint below
    out.w = static_cast<int>(std::ceil(mxx - mnx)) + 2 * pad + 1;
    out.h = static_cast<int>(std::ceil(mxy - mny)) + 2 * pad + 1;
    out.anchor_x = static_cast<int>(std::round(-mnx)) + pad;
    out.anchor_y = static_cast<int>(std::round(-mny)) + pad;
    out.rgba.assign(static_cast<size_t>(out.w) * out.h * 4, 0);
    std::vector<double> zbuf(static_cast<size_t>(out.w) * out.h,
                             -std::numeric_limits<double>::infinity());

    // Voxel footprint (voxel-render.md §1, RESOLVED): the original plots
    // exactly 2 horizontal pixels per voxel (px and px+1, one row).
    // Light dirs rotated into model space ONCE per call (world-fixed light):
    const double lc = std::cos(-yaw_rad), ls = std::sin(-yaw_rad);
    const double L[3] = {kLightWorld[0] * lc - kLightWorld[1] * ls,
                         kLightWorld[0] * ls + kLightWorld[1] * lc, kLightWorld[2]};
    double H[3] = {L[0], L[1], L[2] + 1.0};
    const double hn = std::sqrt(H[0] * H[0] + H[1] * H[1] + H[2] * H[2]);
    H[0] /= hn; H[1] /= hn; H[2] /= hn;

    // Pass 2: voxels.
    for (size_t si = 0; si < m.sections.size(); ++si) {
        const auto& sec = m.sections[si];
        const auto& xf = hva.frames.at(0).at(std::min(si, hva.frames[0].size() - 1));
        const double stepx = (sec.max_bounds[0] - sec.min_bounds[0]) / sec.size_x;
        const double stepy = (sec.max_bounds[1] - sec.min_bounds[1]) / sec.size_y;
        const double stepz = (sec.max_bounds[2] - sec.min_bounds[2]) / sec.size_z;
        for (int z = 0; z < sec.size_z; ++z)
            for (int y = 0; y < sec.size_y; ++y)
                for (int x = 0; x < sec.size_x; ++x) {
                    const auto& v = sec.voxels[x + static_cast<size_t>(sec.size_x) *
                                                       (y + static_cast<size_t>(sec.size_y) * z)];
                    if (!v.present) continue;
                    // Corner-anchored (voxel-render.md §1): no +0.5.
                    const V3 local = {sec.min_bounds[0] + x * stepx,
                                      sec.min_bounds[1] + y * stepy,
                                      sec.min_bounds[2] + z * stepz};
                    double sx, sy, depth;
                    project(yaw_rotate(apply_xform(xf.transform, sec.scale, local), c, s),
                            sx, sy, depth);
                    const int px = static_cast<int>(std::round(sx)) + out.anchor_x;
                    const int py = static_cast<int>(std::round(sy)) + out.anchor_y;
                    const int lv = light_level(v.normal, sec.normals_mode, vpl.section_count, L, H);
                    const ra2assets::Rgba col = pal[vpl.tables[lv][v.color]];
                    // 2x1 footprint (voxel-render.md §1).
                    for (int dx = 0; dx < 2; ++dx) {
                        const int qx = px + dx, qy = py;
                        if (qx < 0 || qy < 0 || qx >= out.w || qy >= out.h) continue;
                        const size_t at = static_cast<size_t>(qy) * out.w + qx;
                        if (depth <= zbuf[at]) continue;
                        zbuf[at] = depth;
                        out.rgba[at * 4 + 0] = col.r;
                        out.rgba[at * 4 + 1] = col.g;
                        out.rgba[at * 4 + 2] = col.b;
                        out.rgba[at * 4 + 3] = 255;
                    }
                }
    }
    return out;
}

}  // namespace ra2engine
```

`engine/voxel_normals.inc`: the two arrays copied VERBATIM from
`docs/re/voxel-render.md` §3 (`static constexpr float kNormals2[36][3] = {...};`
`static constexpr float kNormals4[245][3] = {...};` — 245 entries). Included
inside the anonymous namespace of voxel_raster.cpp only.

CMakeLists.txt: add `target_sources(ra2assets_tests PRIVATE tests/test_voxel_raster.cpp engine/voxel_raster.cpp)`
(pattern/comment style of the `engine/combat.cpp` block); add
`engine/voxel_raster.cpp` to `RA2_ENGINE_SOURCES` in `cmake/ra2_sources.cmake`.

- [ ] **Step 4: Run to verify pass** — `.\build\ra2assets_tests.exe -tc="voxel_raster:*"` → PASS both with and without install.

- [ ] **Step 5: Commit**

```bash
git add engine/voxel_raster.hpp engine/voxel_raster.cpp engine/voxel_normals.inc tests/test_voxel_raster.cpp CMakeLists.txt cmake/ra2_sources.cmake
git commit -m "feat(engine): pure voxel rasterizer (projection, vpl lighting, facing quantization)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: Vehicle state — ROT turning, per-unit speed, turret slew, aim-gated fire

**[RE-GATE: APPLIED 2026-07-10]** — §5 markers reconciled: aim tolerance
AMENDED to 0 (rotation-complete), kRotU16PerRotPerSec CONFIRMED, spawn-facing
constant CONFIRMED, speed-scale controller decision recorded in Task 6 item 6.

**Files:**
- Modify: `engine/world.hpp`, `engine/world.cpp`, `engine/combat.cpp`
- Test: `tests/test_vehicle.cpp` (new; register in CMakeLists.txt after the test_movement.cpp line — no new sources needed, world.cpp/combat.cpp are already compiled in)

**Interfaces:**
- Consumes: `Unit`, `step_units`, `step_combat` (existing).
- Produces (Task 6 consumes):
  - `Unit` gains: `bool is_vehicle = false; double body_facing = 32768.0; double turret_facing = 32768.0; double rot_u16_per_sec = 0.0; double speed_cps = 0.0;`
    (facings in the original 16-bit space [0,65536); 32768 = the same
    screen-South spawn direction infantry uses — CONFIRMED by
    voxel-render.md §2: facing 32768 is index 16 = screen South; keep it).
  - `double facing_u16_from_delta(double dx, double dy)` — continuous
    facing from a cell-space direction, consistent with `facing_from_delta`'s
    8-block convention (block k spans facing `k*8192 ± 4096`).
  - `double turn_toward(double facing, double target, double max_step)` —
    shortest-arc step in u16 space with wraparound, clamped; returns the new
    facing (== target when within max_step).
  - `bool vehicle_aimed(const Unit& u, double target_facing)` — true when
    `|shortest_arc(u.turret_facing, target_facing)| <= kAimToleranceU16`.
  - `constexpr double kAimToleranceU16 = 0.0;` — AMENDED per voxel-render.md
    §5: vanilla holds fire until the timed turret rotation COMPLETES (exact
    facing, tolerance 0, via Is_Rotating). `turn_toward` clamp-lands exactly
    on the target value, and `step_turrets`/`step_combat` compute `want`
    from the same positions in the same frame, so exact double comparison is
    well-defined (`<= 0.0` on the shortest-arc magnitude). Same rule for the
    hull turn-then-move gate.
  - `UnitCombat` gains `int rot = 0; bool turret = false; double speed_cps = 0.0; bool is_vehicle = false;` and `World::add_combat_unit` copies them onto the Unit (`rot_u16_per_sec = rot * kRotU16PerRotPerSec`).
  - `constexpr double kRotU16PerRotPerSec = 256.0 * 15.0;` — CONFIRMED by
    voxel-render.md §5 (`Set_ROT` = ROT·256 u16/frame at 15 Hz): ROT=5 →
    19,200 u16/sec → full circle in ~3.4 s.

**Behavior changes (all vehicle-gated — infantry paths byte-identical):**

1. `step_units`: per-unit speed — the effective cells/sec for a unit is
   `u.speed_cps > 0.0 ? u.speed_cps : move_cps` (one local where `move_cps`
   is used today). Infantry units keep `speed_cps = 0`, so nothing changes
   for them.
2. `step_units`: when a vehicle selects/advances along a walk segment, compute
   `want = facing_u16_from_delta(dx, dy)`; if
   `|shortest_arc(body_facing, want)| > kAimToleranceU16`, the vehicle turns
   in place (`turn_toward` at `rot_u16_per_sec * dt`) and does NOT advance
   this frame `[RE: voxel-render.md §5 — turn-then-move gating]`; otherwise
   it snaps `body_facing = turn_toward(...)` toward exact and advances.
   Vehicles skip the 8-way `Unit::facing` update (facing stays whatever it
   was; renderers must not use it for vehicles).
3. New `void step_turrets(std::vector<Unit>& units, double dt)` (world.cpp,
   declared in world.hpp): for each alive, non-dying vehicle — if it has a
   live target, slew `turret_facing` via `turn_toward` at `rot_u16_per_sec *
   dt` toward `facing_u16_from_delta(target.fx - u.fx, target.fy - u.fy)`;
   else toward `body_facing`. Non-turreted vehicles (`turret==false` in
   combat resolution → Task 6 never builds a turret set) still run this (it
   is harmless — facing tracks body). Called from `World::step` between
   `step_units` and `step_combat`.
4. `engine/combat.cpp` `step_combat`: in the fire branch (target in range,
   rof_cool expired), vehicles additionally require
   `vehicle_aimed(u, facing_u16_from_delta(tdx, tdy))`; when not aimed the
   unit holds fire that frame (rof_cool untouched). Infantry (`!is_vehicle`)
   is exempt — zero behavior change `[RE: voxel-render.md §5 —
   FireError::FACING]`.

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/test_vehicle.cpp
#include "doctest.h"
#include "world.hpp"
#include "combat.hpp"
#include <cmath>

using ra2engine::Unit;
using ra2engine::turn_toward;
using ra2engine::facing_u16_from_delta;

TEST_CASE("vehicle: turn_toward takes the shortest arc with wraparound") {
    CHECK(turn_toward(0.0, 100.0, 300.0) == doctest::Approx(100.0));     // within step: clamp at target
    CHECK(turn_toward(0.0, 1000.0, 300.0) == doctest::Approx(300.0));    // step toward, positive arc
    CHECK(turn_toward(1000.0, 0.0, 300.0) == doctest::Approx(700.0));    // step back, negative arc
    // wraparound: 65000 -> 200 is a +736 arc (through 0), not -64800
    CHECK(turn_toward(65000.0, 200.0, 300.0) == doctest::Approx(65300.0));  // steps toward the wrap
    CHECK(turn_toward(65400.0, 200.0, 400.0) == doctest::Approx(200.0));    // arc 336 <= step 400: reaches target through 0
    // result always normalized to [0, 65536)
    CHECK(turn_toward(65500.0, 300.0, 100.0) == doctest::Approx(64.0));  // 65500 + 100 = 65600 -> 64
}

TEST_CASE("vehicle: facing_u16_from_delta agrees with the 8-block convention") {
    // facing_from_delta (infantry) maps each direction to block 0..7
    // (N=0 counterclockwise-on-screen to NE=7, spawn S=4). The continuous
    // facing must land inside block*8192 +/- 4096 of the SAME block.
    const double dirs[8][2] = {
        // the same 8 cell-space rays test_world.cpp uses for facing_from_delta
        {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1},
    };
    for (int k = 0; k < 8; ++k) {
        const int block = ra2engine::facing_from_delta(dirs[k][0], dirs[k][1]);
        REQUIRE(block >= 0);
        const double f = facing_u16_from_delta(dirs[k][0], dirs[k][1]);
        const double center = block * 8192.0;
        double d = std::fmod(f - center + 65536.0 * 2, 65536.0);
        if (d > 32768.0) d -= 65536.0;
        CHECK(std::abs(d) <= 4096.0);
    }
}

namespace {
std::vector<std::uint8_t> open_grid(int w, int h) {
    return std::vector<std::uint8_t>(static_cast<size_t>(w) * h, 1);
}
Unit tank_at(int x, int y) {
    Unit u;
    u.cell_x = x; u.cell_y = y; u.fx = x; u.fy = y;
    u.is_vehicle = true;
    u.rot_u16_per_sec = 5 * ra2engine::kRotU16PerRotPerSec;  // ROT=5
    u.speed_cps = 4.5;
    return u;
}
}  // namespace

TEST_CASE("vehicle: turns in place before rolling, then moves at its own speed") {
    std::vector<Unit> units = {tank_at(5, 5)};
    // Spawn faces S (32768); path due N-ish: goal (4,4) -> want facing 0-block.
    units[0].path = {{5, 5}, {4, 4}};
    units[0].path_idx = 1;
    ra2engine::Occupancy occ(16, 16);
    occ.set(5, 5, 0);
    auto grid = open_grid(16, 16);
    const ra2engine::AnimTable anims;

    // First small step: facing is ~180 degrees off -> must rotate, not move.
    ra2engine::step_units(units, grid, 16, 16, occ, 1.0 / 60.0, 3.0, anims);
    CHECK(units[0].fx == doctest::Approx(5.0));
    CHECK(units[0].fy == doctest::Approx(5.0));
    CHECK(units[0].body_facing != doctest::Approx(32768.0));  // it turned

    // Run 2 sim-seconds: ROT=5 covers 180 degrees in ~1.7s, then it rolls.
    for (int i = 0; i < 120; ++i)
        ra2engine::step_units(units, grid, 16, 16, occ, 1.0 / 60.0, 3.0, anims);
    CHECK((units[0].fx != doctest::Approx(5.0) || units[0].fy != doctest::Approx(5.0)));

    // Per-unit speed: a fresh tank on a long straight path outruns move_cps.
    std::vector<Unit> fast = {tank_at(1, 8)}, slow = {tank_at(1, 8)};
    fast[0].body_facing = facing_u16_from_delta(1.0, 0.0);   // pre-aligned
    slow[0].body_facing = facing_u16_from_delta(1.0, 0.0);
    slow[0].is_vehicle = false; slow[0].speed_cps = 0.0;     // infantry control
    for (auto* v : {&fast, &slow}) {
        (*v)[0].path.clear();
        for (int x = 1; x <= 14; ++x) (*v)[0].path.push_back({x, 8});
        (*v)[0].path_idx = 1;
    }
    ra2engine::Occupancy o2(16, 16), o3(16, 16);
    o2.set(1, 8, 0); o3.set(1, 8, 0);
    for (int i = 0; i < 60; ++i) {
        ra2engine::step_units(fast, grid, 16, 16, o2, 1.0 / 60.0, 3.0, anims);
        ra2engine::step_units(slow, grid, 16, 16, o3, 1.0 / 60.0, 3.0, anims);
    }
    CHECK(fast[0].fx > slow[0].fx);  // 4.5 cps vs 3.0 cps after 1s
}

TEST_CASE("vehicle: turret slews to target and returns to body when disengaged") {
    std::vector<Unit> units = {tank_at(5, 5), tank_at(9, 9)};
    units[1].is_vehicle = false;  // any target works
    units[0].target = 1;
    units[1].hp = units[1].max_hp = 100;
    units[0].hp = units[0].max_hp = 100;
    const double want = facing_u16_from_delta(units[1].fx - units[0].fx,
                                              units[1].fy - units[0].fy);
    for (int i = 0; i < 300; ++i) ra2engine::step_turrets(units, 1.0 / 60.0);
    CHECK(units[0].turret_facing == doctest::Approx(want));
    units[0].target = -1;
    for (int i = 0; i < 300; ++i) ra2engine::step_turrets(units, 1.0 / 60.0);
    CHECK(units[0].turret_facing == doctest::Approx(units[0].body_facing));
}

TEST_CASE("vehicle: fire waits for turret aim; infantry does not") {
    // Two hostile units in range of each other. The vehicle's turret starts
    // pointing away; step_combat alone (no step_turrets) must never let it
    // fire, then after slewing it fires on the next combat step.
    std::vector<Unit> units = {tank_at(5, 5), tank_at(7, 5)};
    for (auto& u : units) { u.hp = u.max_hp = 400; u.w_damage = 90; u.w_rof = 65;
                            u.w_range = 5.75; u.w_verses = {100}; u.alive = true; }
    units[0].house = 0; units[1].house = 1;
    units[0].target = 1; units[0].attack_order = true;
    units[1].is_vehicle = false;   // control: infantry-rules unit fires immediately
    units[1].target = 0; units[1].attack_order = true;
    units[0].turret_facing = turn_toward(facing_u16_from_delta(1.0, 0.0), 0.0, 20000.0);  // way off
    ra2engine::Occupancy occ(16, 16);
    occ.set(5, 5, 0); occ.set(7, 5, 1);

    ra2engine::step_combat(units, occ, 1.0 / 15.0);
    CHECK(units[1].hp == 400);          // vehicle held fire (not aimed)
    CHECK(units[0].hp < 400);           // infantry-rules unit fired

    // Aim EXACTLY (tolerance 0): use the same delta step_combat computes,
    // (t.fx - u.fx, t.fy - u.fy) = (2, 0), so the doubles compare equal.
    units[0].turret_facing = facing_u16_from_delta(2.0, 0.0);
    ra2engine::step_combat(units, occ, 1.0 / 15.0);
    CHECK(units[1].hp < 400);           // vehicle fired once aimed
}
```

- [ ] **Step 2: Run to verify failure** — compile error (`turn_toward` undeclared).

- [ ] **Step 3: Implement.**

world.hpp — add to `Unit` (after the combat block, with a comment citing voxel-render.md §5):

```cpp
    // --- Voxel Vehicles phase: continuous-facing vehicle state ---
    // Facings live in the original's 16-bit space [0, 65536); 32768 faces
    // screen-South like the infantry spawn (docs/re/voxel-render.md §2/§5).
    bool is_vehicle = false;
    double body_facing = 32768.0;
    double turret_facing = 32768.0;
    double rot_u16_per_sec = 0.0;  // ROT * kRotU16PerRotPerSec
    double speed_cps = 0.0;        // per-unit speed; 0 = use World move_cps
```

world.hpp — free functions + constants (near facing_from_delta) and the
`UnitCombat` fields, exactly as the Interfaces block above specifies; declare
`void step_turrets(std::vector<Unit>& units, double dt);`.

world.cpp:

```cpp
double facing_u16_from_delta(double dx, double dy) {
    // Same screen-space convention as facing_from_delta: block k of 8 sits
    // at k*8192. atan2(dx+dy, dx-dy) is the on-screen angle (see
    // docs/re/infantry-anim.md §5); scale to u16 with block 0 = N.
    const double a = std::atan2(dx + dy, dx - dy);  // [-pi, pi], 0 = screen E
    // screen E is block 6 (infantry-anim.md §4) -> offset so N lands at 0.
    double f = 6.0 * 8192.0 - a * (65536.0 / (2.0 * M_PI));
    f = std::fmod(f, 65536.0);
    if (f < 0) f += 65536.0;
    return f;
}

double turn_toward(double facing, double target, double max_step) {
    double d = std::fmod(target - facing, 65536.0);
    if (d > 32768.0) d -= 65536.0;
    if (d < -32768.0) d += 65536.0;
    double out;
    if (std::abs(d) <= max_step) out = target;
    else out = facing + (d > 0 ? max_step : -max_step);
    out = std::fmod(out, 65536.0);
    if (out < 0) out += 65536.0;
    return out;
}

bool vehicle_aimed(const Unit& u, double target_facing) {
    double d = std::fmod(target_facing - u.turret_facing, 65536.0);
    if (d > 32768.0) d -= 65536.0;
    if (d < -32768.0) d += 65536.0;
    return std::abs(d) <= kAimToleranceU16;
}

void step_turrets(std::vector<Unit>& units, double dt) {
    for (auto& u : units) {
        if (!u.is_vehicle || !u.alive || u.die_time >= 0.0) continue;
        double want = u.body_facing;
        if (u.target >= 0 && u.target < static_cast<int>(units.size())) {
            const Unit& t = units[u.target];
            if (t.alive && t.die_time < 0.0)
                want = facing_u16_from_delta(t.fx - u.fx, t.fy - u.fy);
        }
        u.turret_facing = turn_toward(u.turret_facing, want, u.rot_u16_per_sec * dt);
    }
}
```

step_units integration (inside the existing per-unit walk-advance code, where
the segment direction and speed are used — the implementer reads the
surrounding code first):
- effective speed local: `const double cps = u.speed_cps > 0.0 ? u.speed_cps : move_cps;` used in place of `move_cps`.
- vehicle turn gate, immediately before the advance for a unit with a
  selected segment:

```cpp
        if (u.is_vehicle) {
            const double want = facing_u16_from_delta(seg_dx, seg_dy);
            u.body_facing = turn_toward(u.body_facing, want, u.rot_u16_per_sec * dt);
            double d = std::fmod(want - u.body_facing, 65536.0);
            if (d > 32768.0) d -= 65536.0;
            if (d < -32768.0) d += 65536.0;
            if (std::abs(d) > kAimToleranceU16) continue;  // still turning in place
        }
```

(`seg_dx/seg_dy` = the same delta the existing code feeds `facing_from_delta`;
vehicles skip that 8-way update — guard it with `if (!u.is_vehicle)`.)

`World::step`: call `step_turrets(units_, dt);` between `step_units(...)` and
`step_combat(...)`.

`World::add_combat_unit`: after the existing field copies —

```cpp
    u.is_vehicle = c.is_vehicle;
    u.speed_cps = c.speed_cps;
    u.rot_u16_per_sec = c.rot * kRotU16PerRotPerSec;
```

combat.cpp fire branch (locate the in-range, rof-expired fire code; insert
before damage is applied):

```cpp
        if (u.is_vehicle) {
            const Unit& t = units[u.target];
            if (!vehicle_aimed(u, facing_u16_from_delta(t.fx - u.fx, t.fy - u.fy)))
                continue;  // FireError::FACING -- hold fire, keep rof_cool at 0
        }
```

- [ ] **Step 4: Run the new + full movement/combat suites** — `.\build\ra2assets_tests.exe -tc="vehicle:*"` PASS, then the FULL suite (`.\build\ra2assets_tests.exe`) green — proves infantry movement/combat pins unchanged.

- [ ] **Step 5: Run the --demo-combat identity gate** (vehicles must not perturb it):

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; .\build\ra2engine.exe --demo-combat
```
Expected: exit 0, `deterministic=yes`, same survivor line as the ledger records (GI 4-0).

- [ ] **Step 6: Commit**

```bash
git add engine/world.hpp engine/world.cpp engine/combat.cpp tests/test_vehicle.cpp CMakeLists.txt
git commit -m "feat(engine): vehicle locomotion (ROT turn-in-place, per-unit speed, turret slew, aim-gated fire)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: main.cpp wiring — assets, facing cache, draw, death explosion, voices, --demo-tank

**[RE-GATE: APPLIED 2026-07-10]** — §2 facings/spawn CONFIRMED, §4 palette +
remap CONFIRMED (unittem.pal via pal_for_house), §6 FLH RESOLVED with code,
§5 speed-scale decision recorded (keep 0.75·Speed, document divergence), §7
projectile instant-hit + single-target damage kept per the doc's
recommendation (documented divergences).

**Files:**
- Create: `engine/facing_cache.hpp`, `engine/facing_cache.cpp`
- Modify: `engine/main.cpp`, `engine/iso_renderer.hpp`, `engine/iso_renderer.cpp`, `CMakeLists.txt`, `cmake/ra2_sources.cmake`
- Test: `tests/test_facing_cache.cpp`

**Interfaces:**
- Consumes: `rasterize_model`/`index_to_yaw`/`facing_to_index` (Task 4), `Vpl` (Task 2), Rules/Art getters (Task 3), vehicle Unit fields (Task 5), `pal_for_house`, `TextureCache`, `AudioSystem` (existing).
- Produces:
  - `struct FacingSet { std::vector<VoxelSprite> sprites; }` (`facing_cache.hpp`) and
    `FacingSet build_facing_set(const ra2assets::VxlModel&, const ra2assets::HvaModel&, int n_facings, const ra2assets::Vpl&, const ra2assets::Palette& pal)` — pure, `sprites[i] = rasterize_model(..., index_to_yaw(i, n), ...)`.
  - `constexpr int kVehicleFacings = 32;` `[RE: voxel-render.md §2]` (facing_cache.hpp).
  - iso_renderer: `bool draw_voxel_sprite(void* sdl_renderer, TextureCache& tc, const std::string& key, const VoxelSprite& s, double fx, double fy, int level, const Camera& cam, int view_w, int view_h)` — projects (fx,fy,level) with `cell_to_screen`-equivalent math like draw_unit, places `s` so its anchor lands on the tile center, uploads via `tc.get(key, ...)`. NOTE: TextureCache::get takes INDEXED input today — add a `get_rgba(key, rgba, w, h)` overload to texture_cache.{hpp,cpp} that uploads an RGBA buffer directly (trivial variant of the existing body).
  - main.cpp: `struct VehicleAssets { FacingSet hull[2], turret[2], barrel[2]; bool has_turret = false, has_barrel = false; ra2engine::UnitCombat combat; int flh[3] = {0,0,0}; std::string explosion_image; bool ok = false; }` (index = house), `VehicleAssets load_vehicle_assets(dir, ra2, rules, art, "HTNK")`, `spawn_tank_squads(world, va, n_per_side)`, `--demo-tank` mode.

**Load chain (all individually guarded — any failure logs once and leaves `va.ok = false`, game continues infantry-only):**
1. `htnk.vxl`/`htnk.hva`/`htnktur.vxl`/`htnktur.hva`/`htnkbarl.vxl`/`htnkbarl.hva` from `ra2.mix → local.mix` (same nested-open pattern as `temperat.ini` in `load_assets`). Turret/barrel absence is NOT an error for non-turreted vehicles (`Rules::has_turret` gates the attempt).
2. `voxels.vpl` from `ra2.mix → local.mix`.
3. Palette: `unittem.pal` from `ra2.mix → cache.mix` (already loaded as `ua.pal`) — CONFIRMED by voxel-render.md §4.
4. Explosion: `Rules::explosion_first("HTNK")` (= "TWLT070") SHP from `ra2.mix → conquer.mix` via the existing `load_unit_shp`, drawn with `anim.pal` from `ra2.mix → cache.mix` if present, else `ua.pal`.
5. Build 6 FacingSets: hull/turret/barrel × house 0/1, palette per house via `pal_for_house(pal, house)`. Skip turret/barrel sets when absent.
6. Combat stats: `resolve_combat_unit(rules, "HTNK")` + `va.combat.is_vehicle = true; va.combat.rot = rules.rot("HTNK"); va.combat.turret = rules.has_turret("HTNK"); va.combat.speed_cps = speed * kCellsPerSecPerSpeed;` (Speed=6 → 4.5 cps). CONTROLLER DECISION per voxel-render.md §5: vanilla's absolute conversion is 0.15·Speed (≈5× slower); the engine keeps its established 0.75·Speed scale for ALL units (the tank:GI 6:4 ratio — the vanilla-faithful part — is preserved) as a KNOWN, documented divergence inherited from the infantry phases. Task 7's README lists it; a whole-engine time-base calibration pass is a recorded future-phase candidate. Do NOT change the scale piecemeal for tanks only.
7. Audio: extend `init_audio`'s unit-id loop `{"E1", "E2"}` → `{"E1", "E2", "HTNK"}` — nothing else; the existing per-image `UnitVoices` machinery handles the rest (image "HTNK").

**Draw (in render_world's back-to-front loop):** units with `u.image == "HTNK"` (or generally: an image with a VehicleAssets entry) draw via voxel sprites instead of SHP:

```cpp
    const int bi = ra2engine::facing_to_index(u.body_facing, ra2engine::kVehicleFacings);
    draw_voxel_sprite(ren, tc, "vxl:HTNK:h:" + std::to_string(u.house) + ":" + std::to_string(bi),
                      va.hull[u.house].sprites[bi], u.fx, u.fy, u.level, cam, W, H);
    if (va.has_turret) {
        const int ti = ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings);
        draw_voxel_sprite(ren, tc, "vxl:HTNK:t:" + std::to_string(u.house) + ":" + std::to_string(ti),
                          va.turret[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H);
        if (va.has_barrel)
            draw_voxel_sprite(ren, tc, "vxl:HTNK:b:" + std::to_string(u.house) + ":" + std::to_string(ti),
                              va.barrel[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H);
    }
```

**Dying vehicles:** when `u.die_time >= 0` and the unit is a vehicle, draw the
explosion SHP instead of the tank: `frame = min(int(u.die_time * 15.0), shp.count()-1)`
(same 15 fps as infantry deaths, kDieAnimSecs=1.0 unchanged), anchored like
draw_unit. Fire flash offset: keep the existing fire_flash mechanism; the flash
anchor for vehicles adds the FLH screen offset — RESOLVED by voxel-render.md
§6 (High): `PrimaryFireFLH=F,L,H` is leptons in TURRET space; muzzle offset in
cells = `Rz(turret yaw) * (F, L) / 256.0` (H likewise /256), then reuse the
unit-anchor projection basis:

```cpp
    // FLH (voxel-render.md §6): rotate (F, L) by the QUANTIZED turret yaw,
    // convert leptons -> cells (/256), project with the same 60x30 diamond
    // shear draw_unit uses; height contributes via the level_px-equivalent
    // z term (100 leptons of H ~= 0.39 cells * 15 px/level-ish -- use
    // -H/256.0 * 30.0 px, the cell_to_screen level convention scaled to a
    // half-cell rise, and record the visual anchor in the report).
    const double yaw = ra2engine::index_to_yaw(
        ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings),
        ra2engine::kVehicleFacings);
    const double fc = va.flh[0] / 256.0, lc = va.flh[1] / 256.0;
    const double mx = fc * std::cos(yaw) - lc * std::sin(yaw);   // cells, model X
    const double my = fc * std::sin(yaw) + lc * std::cos(yaw);   // cells, model Y
    const int flash_dx = static_cast<int>(std::round((mx + my) * 30.0));
    const int flash_dy = static_cast<int>(std::round((mx - my) * 15.0 - (va.flh[2] / 256.0) * 30.0));
```

(the (mx+my)/(mx−my) axis pairing matches the rasterizer's recovered basis —
model +X at turret yaw 0 points screen East; Burst=1 so L never flips).

**Spawning:** `spawn_tank_squads(world, va, n)` — reuse `spawn_squad` verbatim
with `va.combat`, origins offset from the infantry squads (house 0 tanks 4
cells west of the GI origin, house 1 tanks 4 cells east of the CONS origin).
Called from `run_interactive`, `run_screenshot`, and `run_demo_tank` ONLY when
`va.ok` — never from `run_demo_combat`/`--demo-move`/`--demo-retarget`/
`--demo-group` (byte-identity + existing demos untouched).

**--demo-tank** (new mode, pattern-copy of `run_demo_combat`): builds the
world with 5 GI + 3 Rhinos (house 0) vs 5 CONS + 3 Rhinos (house 1), orders
house-0 to attack the nearest hostile, re-targets idle units exactly like
`simulate_combat` does, runs rendered + headless, byte-compares the two logs,
requires ≥1 death, no overlap, no dangling targets, AND at least one
Rhino-vs-Rhino hit visible in the log (a victim hp drop of exactly 90 —
trunc(90×100%) heavy) plus a Rhino-vs-infantry hit of exactly 22
(trunc(90×25%) none). Exit 0 only if all hold. Start/end screenshots to the
scratchpad. Register `--demo-tank` in main()'s arg loop, dispatched before
`--demo-combat`.

- [ ] **Step 1: Write the failing facing-cache test**

```cpp
// tests/test_facing_cache.cpp
#include "doctest.h"
#include "facing_cache.hpp"
// reuse the synthetic single-voxel model/vpl/palette helpers by including the
// same construction inline (copy the three helper functions from
// tests/test_voxel_raster.cpp's anonymous namespace -- they are file-local).

TEST_CASE("facing_cache: N sprites, each equal to a direct rasterize call") {
    // build single_voxel_model(100, 0), identity_hva(), identity_vpl(), gray_pal()
    // (helpers copied per the note above)
    auto set = ra2engine::build_facing_set(model, hva, 32, vpl, pal);
    REQUIRE(set.sprites.size() == 32);
    for (int i = 0; i < 32; ++i) {
        CHECK(set.sprites[i].w > 0);
        auto direct = ra2engine::rasterize_model(model, hva, ra2engine::index_to_yaw(i, 32), vpl, pal);
        CHECK(set.sprites[i].rgba == direct.rgba);       // no drift cache vs direct
        CHECK(set.sprites[i].anchor_x == direct.anchor_x);
        CHECK(set.sprites[i].anchor_y == direct.anchor_y);
    }
}

TEST_CASE("facing_cache: real Rhino parts all render non-empty at every facing") {
    // Guarded (tests/install_dir.hpp pattern; MESSAGE+return without install).
    // Load htnk.vxl/.hva, htnktur.vxl/.hva, htnkbarl.vxl/.hva from
    // ra2.mix -> local.mix, voxels.vpl likewise, unittem.pal from cache.mix
    // (same chains as test_vxl.cpp / the Task 2 real-vpl test). For each of
    // the 3 parts x pal_for_house(pal, 0) and pal_for_house(pal, 1):
    //   auto set = ra2engine::build_facing_set(model, hva, ra2engine::kVehicleFacings, vpl, house_pal);
    //   REQUIRE(set.sprites.size() == ra2engine::kVehicleFacings);
    //   for (auto& s : set.sprites) { CHECK(s.w > 0); CHECK(opaque-pixel count > 0); }
    // (spec: "N facings x 3 parts x 2 houses all non-empty")
}
```

- [ ] **Step 2: Run to verify failure** — compile error (no facing_cache.hpp).

- [ ] **Step 3: Implement facing_cache (pure)**

```cpp
// engine/facing_cache.hpp
#pragma once
#include <vector>
#include "voxel_raster.hpp"

namespace ra2engine {

// Cached-facings renderer strategy (spec: "cached facings"): the original
// quantizes vehicle orientation to kVehicleFacings pre-rendered views
// (docs/re/voxel-render.md §2); we rasterize each once at load and blit.
constexpr int kVehicleFacings = 32;  // [RE: voxel-render.md §2]

struct FacingSet {
    std::vector<VoxelSprite> sprites;  // size n_facings; index = facing_to_index()
};

// Rasterizes model at every quantized facing. Pure; deterministic.
FacingSet build_facing_set(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                           int n_facings, const ra2assets::Vpl& vpl,
                           const ra2assets::Palette& pal);

}  // namespace ra2engine
```

```cpp
// engine/facing_cache.cpp
#include "facing_cache.hpp"

namespace ra2engine {

FacingSet build_facing_set(const ra2assets::VxlModel& m, const ra2assets::HvaModel& hva,
                           int n_facings, const ra2assets::Vpl& vpl,
                           const ra2assets::Palette& pal) {
    FacingSet out;
    out.sprites.reserve(n_facings);
    for (int i = 0; i < n_facings; ++i)
        out.sprites.push_back(rasterize_model(m, hva, index_to_yaw(i, n_facings), vpl, pal));
    return out;
}

}  // namespace ra2engine
```

CMake: `tests/test_facing_cache.cpp engine/facing_cache.cpp` into
ra2assets_tests; `engine/facing_cache.cpp` into `RA2_ENGINE_SOURCES`. Run
`-tc="facing_cache:*"` → PASS. Commit
(`feat(engine): load-time voxel facing cache`, with trailer).

- [ ] **Step 4: texture_cache.get_rgba + draw_voxel_sprite + main.cpp wiring** — implement everything in the Interfaces/Load/Draw/Spawn blocks above. `draw_voxel_sprite` body mirrors `draw_unit`'s projection (read iso_renderer.cpp first): tile-center anchor = `cell_to_screen(fx, fy, level, 60, 30, 15) + cam + (30, 15)`, blit rect at `(center.x - s.anchor_x, center.y - s.anchor_y, s.w, s.h)`, off-screen cull, `tc.get_rgba(key, ...)` for the texture. Death-explosion draw + FLH flash + `init_audio` id-list change + `spawn_tank_squads` + `--demo-tank` per the blocks above. Startup log line: `[vehicle] image=HTNK facings=32 hull=46x34x12 turret=yes barrel=yes speed_cps=4.5 rot=5` (from the loaded data, not hardcoded).

Selection/HP-bar sizing (spec: "sized from the cached sprite bounds"): the
tank's hull sprite is wider than an infantry SHP, so `draw_hp_bar`'s fixed
24px bar and `draw_selection`'s tile diamond read undersized on it. Minimal
conforming change: give both functions an optional trailing
`int width_px = 0` parameter (0 = today's defaults, callers unchanged);
render_world passes `va.hull[u.house].sprites[bi].w` for vehicle units so the
bar spans the hull and the selection diamond scales to it.

- [ ] **Step 5: Visual verification (the calibration gate).** Build; run
`$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; .\build\ra2engine.exe --screenshot <scratchpad>\tank.png`
and READ the screenshot: blue and red Rhinos visible, correct iso proportions
(tank ≈ one cell footprint), turret on hull, distinguishable facings. Compare
against an original-game Rhino screenshot (the controller supplies one from
the install docs or the user). If the projection looks wrong (stretched,
rotated, mis-lit), STOP and report BLOCKED with the screenshot — do not
hand-tweak constants; the fix goes through voxel-render.md.

- [ ] **Step 6: Run --demo-tank + the identity gates**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
.\build\ra2engine.exe --demo-tank        # exit 0, deterministic=yes, 90/22 hits present
.\build\ra2engine.exe --demo-combat      # unchanged: exit 0, deterministic=yes, GI 4-0
.\build\ra2assets_tests.exe              # full suite green
```

- [ ] **Step 7: Pin the htnk render hash** — with Step 5 visually confirmed, convert Task 4's MESSAGE-only FNV-1a hash in `tests/test_voxel_raster.cpp` ("real htnk hull") into a `CHECK` against the observed value (comment: "pinned after visual calibration vs the original — re-pin only with a new visual check").

- [ ] **Step 8: Commit**

```bash
git add engine/facing_cache.hpp engine/facing_cache.cpp engine/texture_cache.hpp engine/texture_cache.cpp engine/iso_renderer.hpp engine/iso_renderer.cpp engine/main.cpp tests/test_facing_cache.cpp tests/test_voxel_raster.cpp CMakeLists.txt cmake/ra2_sources.cmake
git commit -m "feat(engine): Rhino tank on screen -- voxel facing caches, draw path, death explosion, voices, --demo-tank

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: Finalize — arm64, README, full gates, ledger

**Files:**
- Modify: `android/app/jni/CMakeLists.txt` (if the new engine sources are not already picked up via `RA2_ENGINE_SOURCES` — verify; `audio_sdl.cpp` needed manual addition there, the new pure engine files should flow through `ra2_sources.cmake` automatically), `README.md`

**Steps:**

- [ ] **Step 1: arm64 gate.** Cross-compile the Android target exactly as the audio phase did (NDK r27c; see `.superpowers/sdd/progress.md` for the invocation) and verify `libmain.so` contains the new symbols: `llvm-nm` for `rasterize_model`, `build_facing_set`, `Vpl::load`, `step_turrets`.
- [ ] **Step 2: README.** Add a "Voxel vehicles" paragraph after the audio one: what renders (hull/turret/barrel, 32 cached facings, vpl lighting), the locomotion model (ROT turn-in-place, per-unit speed), and the PRECISE divergence list from the spec: no slope tilt (cached flat facings), no crush, no debris, projectile instant-hit + single-target damage (voxel-render.md §7: vanilla shells fly 102 leptons/frame and CellSpread .3 reaches only the direct target — state both halves), absolute speed scale kept at the engine's established 0.75·Speed vs vanilla's 0.15·Speed (ratios vanilla-faithful; whole-engine time-base calibration is a recorded future candidate), turret ROF+5-frame idle-return hold omitted, MoveSound deferred.
- [ ] **Step 3: Full gates, both modes.** PowerShell: full suite WITH install (expect all-pass), then `Remove-Item Env:RA2_INSTALL_DIR` + rename-guard is NOT possible (default dir exists on this box) — instead run the suite and confirm the guarded tests report their real-asset cases (the without-install skip path is exercised on the arm64/CI side; note it in the report as the audio phase did). Run `--demo-tank`, `--demo-combat`, `--demo-audio`, `--screenshot`.
- [ ] **Step 4: Commit + ledger.**

```bash
git add README.md android/app/jni/CMakeLists.txt
git commit -m "docs+build: voxel vehicles phase finalized (arm64, README divergences)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Append the phase-complete line to `.superpowers/sdd/progress.md` (controller does this).

---

## Self-review notes (spec coverage)

- Spec "Task 1 Ghidra hunt §1–§7" → Task 1. "Vpl" → Task 2. "rasterizer" → Task 4. "facing_cache" → Task 6 Steps 1–3 (+kVehicleFacings). "vehicle state/ROT/turret/fire gate" → Task 5. "main wiring/death/voices/--demo-tank/error handling" → Task 6. "arm64/README/divergences/gates" → Task 7. Rules/Art getters (implied by wiring; Range=5.75 double bug found during planning) → Task 3.
- Combat pins (22/90 damage, 5 shots, ROF cadence) live in --demo-tank's log assertions (Task 6) — the pure damage math is already pinned by test_combat.cpp; no duplicate pure test added (DRY).
- CellSpread/projectile: recovered in §7, decision deferred to the amendment gate (spec-conformant).
