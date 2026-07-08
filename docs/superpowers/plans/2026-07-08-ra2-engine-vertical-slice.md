# RA2 Engine Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render a real stock Red Alert 2 temperate map with real theater terrain tiles and one infantry unit the player can tap to select and tap to move (A*-pathfound, walk-animated) — running on desktop and installable as an Android `.apk`.

**Architecture:** Extend the existing pure `ra2assets` library with four new dependency-free parsers (LCW/Format80 decompressor, map-pack wrapper, `Rules`/INI, `MapFile`, `Theater`/TMP). Add a new `engine/` layer that links SDL2 for window/input/audio and does all rendering/simulation in our own code (`SDL_Renderer` 2D, isometric projection, `TextureCache`, `World` + A*). Develop desktop-first; package the same native code as an `arm64-v8a` APK last.

**Tech Stack:** C++17, CMake + Ninja, MinGW-w64 (host) / Android NDK r27c (`arm64-v8a`), SDL2 (engine platform layer only), doctest, the existing `ra2assets` library (Blowfish/MIX/PAL/SHP). LCW and format parsers reimplemented in-house; cross-checked against XCC/OpenRA and byte-oracled.

---

## Global Constraints

Every task's requirements implicitly include this section. Values are copied from the approved spec.

- **Two layers, one repo.** Everything extends `D:\ra2-android`. The `ra2assets` library target (`src/blowfish.cpp`, `mix*.cpp`, `pal.cpp`, `shp.cpp` + the new parsers below) stays **pure C++17 with NO SDL/OS/third-party dependencies**. Only the new `engine/` target links SDL2.
- **New parsers are original implementations**, reimplemented from public format documentation and the standard algorithms — **not copied from XCC/OpenRA (both GPLv3)**. XCC/OpenRA (cloned read-only under `D:\ra2-re\reference`) may be consulted for algorithm understanding and used as **byte-for-byte test oracles**, never transcribed. This mirrors the Phase-1 clean-room outcome for `mix_keyderiv.cpp`.
- **No game assets, game binary, oracle output, or GPL code committed.** Assets resolve at runtime via `RA2_INSTALL_DIR` (default `D:\Red Alert 2`). Oracle tools live outside the repo under `D:\ra2-re`. `.gitignore` already blocks `/oracle/`, `*.mix/*.shp/*.pal/*.exe`, build dirs.
- **Rendering = `SDL_Renderer` 2D** (hardware-accelerated textures). Sprites/tiles palettized to RGBA on CPU via `ra2assets` (`Palette` + `ShpFile::palettize`); index 0 transparent. No OpenGL ES/shaders this milestone.
- **Target asset set:** temperate theater (`temperat.mix` → `.tmp` tiles + `isotem.pal`), one small stock skirmish map, one infantry unit (GI: `gi.shp`, proven to decode in Phase 1). Rules from `rulesmd.ini` (extracted from the MIX). Exact filenames verified via `ra2extract --list`/`find` and substituted if the install differs (Phase-1 precedent: `e1.shp`→`gi.shp`); document any substitution.
- **Palette scaling** stays the Phase-1 decision: `expand6(v) = v<<2` (game-accurate), already in `src/pal.cpp`. Do not change it.
- **Windows/tooling:** build with `cmake --build <dir>`; **run all `.exe`s (tests, engine) from PowerShell, NOT Git Bash** (Git Bash's bundled DLLs break exe launch — exit 127). NDK r27c at `D:\android-ndk-r27c`.
- **Bounds-validate every new parser** (truncation, oversized counts, out-of-range indices) and throw `std::runtime_error` with context, matching the hardening standard Phase 1 settled on. Asset-dependent tests skip gracefully when `RA2_INSTALL_DIR` is absent; oracle tests assert a non-vacuous comparison count.

---

## File Structure

```
D:\ra2-android\
├── CMakeLists.txt                     # MODIFY: add engine option + SDL2, new lib sources
├── include\ra2assets\
│   ├── lcw.hpp                        # Task 1  — Format80/LCW decompressor
│   ├── mappack.hpp                    # Task 2  — base64 + Format5 chunk wrapper
│   ├── ini.hpp                        # Task 3  — minimal INI reader
│   ├── rules.hpp                      # Task 4  — unit rules lookup
│   ├── map.hpp                        # Task 5  — MapFile (cells + start)
│   └── theater.hpp                    # Task 6  — Theater/TMP tile decoder
├── src\
│   ├── lcw.cpp  mappack.cpp  ini.cpp  rules.cpp  map.cpp  theater.cpp
├── engine\                            # NEW — links SDL2 (Tasks 7-13)
│   ├── main.cpp                       # entry: init, load, loop
│   ├── platform.hpp / .cpp            # SDL init, window, event pump, timing
│   ├── texture_cache.hpp / .cpp       # indexed+palette -> SDL_Texture
│   ├── iso_renderer.hpp / .cpp        # projection + terrain/sprite draw + camera
│   ├── world.hpp / .cpp               # cell grid, unit, sim step
│   ├── pathfind.hpp / .cpp            # A* over passability
│   └── input.hpp / .cpp               # mouse/touch -> select/move/scroll
├── tests\
│   ├── test_lcw.cpp  test_mappack.cpp  test_ini.cpp  test_rules.cpp
│   ├── test_map.cpp  test_theater.cpp
│   ├── test_projection.cpp            # iso projection round-trips
│   └── test_pathfind.cpp             # A* on synthetic grids
├── android\                           # Task 14 — SDL2 Android project -> APK
└── docs\re\theater.md                 # notes: TMP/map-pack findings + oracle results
```

Oracle tools (outside repo): `D:\ra2-re\oracle-tool\xcc_lcw_dump`, `xcc_tmp_dump` (Task 1/6 oracles), built like the Phase-1 `xcc_shp_dump`.

Task order builds bottom-up: decompression → map-pack → INI → rules → map → theater → (engine) texture → projection → terrain render → unit render → pathfinding → input/sim → Android.

---

## Prerequisites

- `ra2assets` Phase 1 complete (present). Suite baseline: 20 cases / 4347 assertions green.
- SDL2 acquired for desktop via CMake `FetchContent` (pinned `release-2.30.9`) — builds from source with MinGW. For Android, SDL2's own `android-project` template + CMake (Task 14).
- References cloned: `D:\ra2-re\reference\xcc` (@6f91bf8), `D:\ra2-re\reference\OpenRA`. XCC LCW oracle: `misc/shp_decode.cpp` `decode80`. XCC TMP: `misc/tmp_ts_file.cpp` `decode`/`decode_tile`, structs in `misc/cc_structures.h` (`t_tmp_ts_header`, `t_tmp_image_header`). OpenRA map packs: `Format5`/`Format80`, TS map/tileset readers.

---

## Task 0: Engine scaffold — SDL2 window on desktop

**Files:**
- Modify: `CMakeLists.txt`
- Create: `engine/main.cpp`, `engine/platform.hpp`, `engine/platform.cpp`

**Interfaces:**
- Produces: an `ra2engine` executable that opens an SDL2 window, clears it to a solid color, and exits on window-close/Esc. `platform.hpp` exposes:
  ```cpp
  namespace ra2engine {
    struct Platform {
        static Platform create(const char* title, int w, int h); // throws on SDL failure
        bool poll(/*out*/ struct InputEvent& ev);   // false when no more events this frame
        void begin_frame();                          // clear
        void present();
        void* renderer() const;                      // SDL_Renderer* (opaque to keep SDL out of headers where possible)
        ~Platform();
    };
  }
  ```
- Consumes: nothing (SDL2 only).

- [ ] **Step 1: Add SDL2 + engine target to CMake.** Append to `CMakeLists.txt`:
```cmake
option(RA2_BUILD_ENGINE "Build the SDL2 engine/app (needs SDL2)" ON)
if (RA2_BUILD_ENGINE)
  include(FetchContent)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL2)
  add_executable(ra2engine
    engine/main.cpp engine/platform.cpp)
  target_include_directories(ra2engine PRIVATE include engine)
  target_link_libraries(ra2engine PRIVATE ra2assets SDL2::SDL2 SDL2::SDL2main)
endif()
```

- [ ] **Step 2: Write `engine/platform.hpp`** — the interface above, plus a plain `InputEvent`:
```cpp
#pragma once
namespace ra2engine {
struct InputEvent {
    enum Kind { None, Quit, PointerDown, PointerUp, PointerMove } kind = None;
    int x = 0, y = 0;            // pointer position (window pixels)
};
class Platform {
public:
    static Platform create(const char* title, int w, int h);
    Platform(Platform&&) noexcept; Platform& operator=(Platform&&) noexcept;
    ~Platform();
    bool poll(InputEvent& ev);
    void begin_frame();
    void present();
    void* renderer() const;   // SDL_Renderer*
    int width() const; int height() const;
private:
    Platform() = default;
    struct Impl; Impl* p_ = nullptr;
};
}
```

- [ ] **Step 3: Implement `engine/platform.cpp`** — `SDL_Init(SDL_INIT_VIDEO)`, `SDL_CreateWindow`, `SDL_CreateRenderer(...,SDL_RENDERER_ACCELERATED)`; `poll` translates `SDL_QUIT`, `SDL_MOUSEBUTTONDOWN/UP`, `SDL_MOUSEMOTION`, `SDL_FINGERDOWN/UP/MOTION` into `InputEvent`; `begin_frame` = `SDL_SetRenderDrawColor` + `SDL_RenderClear`; `present` = `SDL_RenderPresent`. Throw `std::runtime_error(SDL_GetError())` on failures.

- [ ] **Step 4: Write `engine/main.cpp`** — create a 1024×768 window, loop: pump events (quit on `Quit`), `begin_frame()` (dark blue), `present()`, ~60fps via `SDL_Delay`. Exit cleanly.

- [ ] **Step 5: Configure + build.**
```bash
cmake -S . -B build -G Ninja
cmake --build build --target ra2engine
```
Expected: SDL2 fetched and built once, `ra2engine.exe` links.

- [ ] **Step 6: Run (PowerShell) and verify a window appears.**
```powershell
D:\ra2-android\build\ra2engine.exe
```
Expected: a 1024×768 window filled dark blue; closes on Esc/close. (Manual visual check — note it in the report.)

- [ ] **Step 7: Commit** `feat(engine): SDL2 platform layer + blank window (desktop)`.

---

## Task 1: LCW / Format80 decompressor (pure lib)

The map-pack chunks (Task 2) and some assets are LCW-compressed. LCW ("Format80") is a public, well-documented Westwood RLE/back-reference scheme. Reimplement it fresh; oracle against XCC `decode80`.

**Files:** Create `include/ra2assets/lcw.hpp`, `src/lcw.cpp`, `tests/test_lcw.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    // Decompress LCW/Format80. Writes up to out_len bytes. Returns bytes written.
    size_t lcw_decompress(const uint8_t* src, size_t src_len, uint8_t* out, size_t out_len);
  }
  ```

- [ ] **Step 1: Write failing tests** (`tests/test_lcw.cpp`) with hand-built streams covering each command:
```cpp
#include "doctest.h"
#include "ra2assets/lcw.hpp"
#include <vector>
#include <cstdint>
using ra2assets::lcw_decompress;
static std::vector<uint8_t> dec(std::vector<uint8_t> s, size_t out){
    std::vector<uint8_t> o(out,0); size_t n=lcw_decompress(s.data(),s.size(),o.data(),o.size()); o.resize(n); return o; }

TEST_CASE("lcw: literal copy (0x80|count) then end (0x80)") {
    // 0x83 => copy 3 literal bytes 0A 0B 0C ; 0x80 => end
    auto o = dec({0x83,0x0A,0x0B,0x0C,0x80}, 8);
    REQUIRE(o.size()==3); CHECK(o[0]==0x0A); CHECK(o[1]==0x0B); CHECK(o[2]==0x0C);
}
TEST_CASE("lcw: short relative back-copy (cmd MSB=0)") {
    // literal 'AB' then relative copy count=3 offset=2 -> repeats "ABA"
    // literal: 0x82 41 42 ; rel: cmd=0x20|0x00=0x20? count=((0x20&0x70)>>4)+3=3, off hi nibble=0, next=0x02
    auto o = dec({0x82,0x41,0x42, 0x20,0x02, 0x80}, 16);
    REQUIRE(o.size()==5); CHECK(o[0]=='A');CHECK(o[1]=='B');CHECK(o[2]=='A');CHECK(o[3]=='B');CHECK(o[4]=='A');
}
TEST_CASE("lcw: run fill (0xFE count value)") {
    // 0xFE, count=5 (LE u16), value=0x77
    auto o = dec({0xFE,0x05,0x00,0x77,0x80}, 16);
    REQUIRE(o.size()==5); for(auto b:o) CHECK(b==0x77);
}
TEST_CASE("lcw: large absolute copy (0xFF count pos)") {
    // literal 'XYZ'; 0xFF count=2 pos=0 -> copies dest[0..1]="XY"
    auto o = dec({0x83,'X','Y','Z', 0xFF,0x02,0x00,0x00,0x00, 0x80}, 16);
    REQUIRE(o.size()==5); CHECK(o[3]=='X'); CHECK(o[4]=='Y');
}
TEST_CASE("lcw: medium absolute copy (0xC0..0xFD, count=(cmd&0x3F)+3, pos u16)") {
    // literal 'XYZ'; cmd=0xC0 -> count=3, pos=0 -> "XYZ"
    auto o = dec({0x83,'X','Y','Z', 0xC0,0x00,0x00, 0x80}, 16);
    REQUIRE(o.size()==6); CHECK(o[3]=='X');CHECK(o[4]=='Y');CHECK(o[5]=='Z');
}
```

- [ ] **Step 2: Run — expect FAIL (header missing).**
```bash
cmake --build build && (PowerShell) D:\ra2-android\build\ra2assets_tests.exe -tc="lcw*"
```

- [ ] **Step 3: Header** (`include/ra2assets/lcw.hpp`):
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
namespace ra2assets { size_t lcw_decompress(const uint8_t* src, size_t src_len, uint8_t* out, size_t out_len); }
```

- [ ] **Step 4: Implement** (`src/lcw.cpp`) — the canonical Format80 command set, with strict bounds checks (never read past `src_len`, never write past `out_len`, back-refs must stay within already-written output):
```cpp
#include "ra2assets/lcw.hpp"
#include <stdexcept>
namespace ra2assets {
size_t lcw_decompress(const uint8_t* s, size_t sl, uint8_t* d, size_t dl) {
    const uint8_t* s0=s; const uint8_t* se=s+sl; uint8_t* d0=d; uint8_t* de=d+dl;
    auto need=[&](const uint8_t* p,size_t n){ if(p+n>se) throw std::runtime_error("lcw: src overrun"); };
    auto put=[&](uint8_t b){ if(d>=de) throw std::runtime_error("lcw: dst overrun"); *d++=b; };
    while (s<se) {
        uint8_t cmd=*s++;
        if (!(cmd&0x80)) {                                  // 0x00-0x7F: short relative copy
            need(s,1); size_t count=((cmd&0x70)>>4)+3; size_t off=((size_t)(cmd&0x0F)<<8)|*s++;
            if (off==0 || (size_t)(d-d0)<off) throw std::runtime_error("lcw: bad rel off");
            const uint8_t* r=d-off; for(size_t i=0;i<count;i++) put(*r++);
        } else if (!(cmd&0x40)) {                            // 0x80-0xBF
            size_t count=cmd&0x3F; if(count==0) break;       // 0x80 = end
            need(s,count); for(size_t i=0;i<count;i++) put(*s++);   // literal copy
        } else if (cmd!=0xFE && cmd!=0xFF) {                 // 0xC0-0xFD: medium absolute copy
            size_t count=(cmd&0x3F)+3; need(s,2); size_t pos=s[0]|(s[1]<<8); s+=2;
            if (pos+count>(size_t)(d-d0)+count && pos>(size_t)(d-d0)) {/*fallthrough check below*/}
            for(size_t i=0;i<count;i++){ if(pos>=(size_t)(d-d0)) throw std::runtime_error("lcw: bad abs pos"); put(d0[pos++]); }
        } else if (cmd==0xFE) {                              // run fill
            need(s,3); size_t count=s[0]|(s[1]<<8); uint8_t v=s[2]; s+=3;
            for(size_t i=0;i<count;i++) put(v);
        } else {                                             // 0xFF large absolute copy
            need(s,4); size_t count=s[0]|(s[1]<<8); size_t pos=s[2]|(s[3]<<8); s+=4;
            for(size_t i=0;i<count;i++){ if(pos>=(size_t)(d-d0)) throw std::runtime_error("lcw: bad abs pos"); put(d0[pos++]); }
        }
    }
    (void)s0; return (size_t)(d-d0);
}
}
```

- [ ] **Step 5: Wire CMake** (`target_sources(ra2assets PRIVATE src/lcw.cpp)`, `target_sources(ra2assets_tests PRIVATE tests/test_lcw.cpp)`), build, run `-tc="lcw*"` — expect PASS.

- [ ] **Step 6: Oracle gate (outside repo).** Build `D:\ra2-re\oracle-tool\xcc_lcw_dump` embedding XCC's `decode80` verbatim (GPL, stays outside repo). Feed it a handful of real LCW blocks (extract via a temporary harness) and confirm our `lcw_decompress` output is byte-identical. Record in `docs/re/theater.md`. (This proves our fresh implementation matches the reference on real data, exactly like the Phase-1 SHP oracle.)

- [ ] **Step 7: Commit** `feat(lcw): in-house Format80/LCW decompressor (oracle-verified)`.

---

## Task 2: Map-pack wrapper — base64 + Format5 chunks (pure lib)

**Files:** Create `include/ra2assets/mappack.hpp`, `src/mappack.cpp`, `tests/test_mappack.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `lcw_decompress` (Task 1).
- Produces:
  ```cpp
  namespace ra2assets {
    std::vector<uint8_t> base64_decode(const std::string& s);        // standard alphabet, ignores whitespace
    // Decompress a concatenated "Format5" pack: repeated [u16 cmpSize][u16 uncmpSize][LCW data].
    std::vector<uint8_t> decompress_format5(const uint8_t* data, size_t len);
  }
  ```

- [ ] **Step 1: Failing tests** — (a) base64 round-trip of known vectors (`"TWFu"`→`"Man"`, `"cGxlYXN1cmUu"`→`"pleasure."`); (b) a synthetic Format5 stream: build one chunk = `[u16 cmpSize][u16 uncmpSize] + lcw(literal "HELLO")`, assert `decompress_format5` yields `"HELLO"`; (c) two chunks concatenate.
```cpp
#include "doctest.h"
#include "ra2assets/mappack.hpp"
using namespace ra2assets;
TEST_CASE("base64 known vectors"){ auto a=base64_decode("TWFu"); CHECK(std::string(a.begin(),a.end())=="Man");
  auto b=base64_decode("cGxlYXN1cmUu"); CHECK(std::string(b.begin(),b.end())=="pleasure."); }
TEST_CASE("format5 single + double chunk"){
  // lcw("HELLO") = 0x85 'H' 'E' 'L' 'L' 'O' 0x80  (literal-5 then end) => 7 bytes, uncmp=5
  std::vector<uint8_t> lcw={0x85,'H','E','L','L','O',0x80};
  auto chunk=[&](std::vector<uint8_t> body,uint16_t u){ std::vector<uint8_t> c;
    c.push_back(body.size()&0xff);c.push_back(body.size()>>8);c.push_back(u&0xff);c.push_back(u>>8);
    for(auto x:body)c.push_back(x); return c; };
  auto one=chunk(lcw,5); auto out=decompress_format5(one.data(),one.size());
  CHECK(std::string(out.begin(),out.end())=="HELLO");
  auto two=one; for(auto x:chunk(lcw,5)) two.push_back(x);
  auto out2=decompress_format5(two.data(),two.size()); CHECK(std::string(out2.begin(),out2.end())=="HELLOHELLO");
}
```

- [ ] **Step 2: Run — expect FAIL.**

- [ ] **Step 3: Implement** `base64_decode` (standard alphabet, `=` padding, skip whitespace) and `decompress_format5` (loop: read `u16 cmp`, `u16 uncmp`, `lcw_decompress(src, cmp, tmp, uncmp)`, append; stop at end; bounds-check each chunk header and body against `len`). Header (`mappack.hpp`) declares both + includes `<vector>`, `<string>`.

- [ ] **Step 4: Wire CMake, build, run `-tc="base64*" -tc="format5*"` — PASS.**

- [ ] **Step 5: Commit** `feat(mappack): base64 + Format5 chunk decompression`.

---

## Task 3: Minimal INI reader (pure lib)

**Files:** Create `include/ra2assets/ini.hpp`, `src/ini.cpp`, `tests/test_ini.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    class Ini {
    public:
      static Ini parse(const std::string& text);           // also accepts bytes via string(begin,end)
      bool has(const std::string& section) const;
      std::string get(const std::string& section, const std::string& key, const std::string& def="") const;
      int get_int(const std::string& section, const std::string& key, int def=0) const;
      // Ordered key/value list for a section (needed for IsoMapPack5 numbered keys "1=,2=,...").
      const std::vector<std::pair<std::string,std::string>>& section(const std::string& name) const;
      std::vector<std::string> section_names() const;
    };
  }
  ```

- [ ] **Step 1: Failing tests** — parse a small INI (sections, `key=value`, `;`/`#` comments, whitespace trimming, duplicate-key ordering preserved), assert `get`, `get_int`, and ordered `section()` iteration. Include a case with a `[IsoMapPack5]`-style numbered-key section to prove concatenation order.

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** a straightforward line parser: track current `[section]`; split first `=`; trim; ignore lines starting with `;`/`#`; store per-section ordered `vector<pair<string,string>>` plus a `map<string,size_t>` index. `get` returns the **last** value for a key (INI override semantics); `section()` returns the ordered list. Case-insensitive section/key compare (RA2 INIs are case-insensitive) — normalize to a canonical case on store+lookup.

- [ ] **Step 4: Build, run `-tc="ini*"` — PASS.**

- [ ] **Step 5: Commit** `feat(ini): minimal case-insensitive INI reader`.

---

## Task 4: `Rules` — unit lookups from rulesmd.ini (pure lib)

**Files:** Create `include/ra2assets/rules.hpp`, `src/rules.cpp`, `tests/test_rules.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `Ini` (Task 3), `MixFile` (Phase 1).
- Produces:
  ```cpp
  namespace ra2assets {
    class Rules {
    public:
      static Rules load(const Ini& ini);
      // Art image (SHP base name) for a unit id; falls back to the id itself.
      std::string image(const std::string& unit_id) const;   // e.g. "GI" -> "GI" (then GI.SHP)
      int speed(const std::string& unit_id) const;            // [unit] Speed=, default 0
    };
  }
  ```

- [ ] **Step 1: Failing test** — synthetic `Ini` with `[GI]` section (`Image=GI`, `Speed=4`); assert `image("GI")=="GI"` and `speed("GI")==4`; unknown id → `image` returns the id, `speed` returns default. Plus a **guarded real-asset test**: locate `rulesmd.ini` in the install (descend `ra2md.mix`; try `find(mix_id("rulesmd.ini"))` at top level and within nested `localmd.mix`/`expandmd*.mix`), parse it, and assert `[GI]` exists and has a positive `Speed`. Skip gracefully if not found; if found, this proves the reader on real data.

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** — thin wrapper over `Ini`: `image` = `ini.get(unit_id,"Image",unit_id)`; `speed` = `ini.get_int(unit_id,"Speed",0)`. (Keep minimal — YAGNI; more fields come when the sim needs them.)

- [ ] **Step 4: Build, run `-tc="rules*"` (PowerShell, RA2_INSTALL_DIR set) — PASS.** Record the resolved `rulesmd.ini` location + GI speed in the report.

- [ ] **Step 5: Commit** `feat(rules): unit image/speed lookup over rulesmd.ini`.

---

## Task 5: `MapFile` — cells + start from a real map (pure lib)

**Files:** Create `include/ra2assets/map.hpp`, `src/map.cpp`, `tests/test_map.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `Ini`, `base64_decode`, `decompress_format5`.
- Produces:
  ```cpp
  namespace ra2assets {
    struct MapCell { int x, y; uint32_t tile; uint8_t subtile; uint8_t level; };
    class MapFile {
    public:
      static MapFile parse(const std::string& ini_text);
      int width() const; int height() const;           // from [Map] Size / LocalSize
      const std::vector<MapCell>& cells() const;        // decoded from IsoMapPack5
      std::string theater() const;                      // [Map] Theater= (e.g. "TEMPERATE")
      const MapCell* at(int x, int y) const;            // nullptr if none
    };
  }
  ```

- [ ] **Step 1: Failing test** — (a) synthetic: build an INI with `[Map] Theater=TEMPERATE`, `Size=0,0,10,10`, and an `[IsoMapPack5]` whose concatenated base64 decodes (via Task 2) to two 11-byte cell records `{x,y,tile,subtile,level}`; assert `cells().size()==2` and field values. (b) **Guarded real-map test**: extract a stock temperate `.map`/`.mpr` (list candidates from the maps mix via `--list`; pick one whose `[Map] Theater=TEMPERATE`), parse, assert `width()/height()>0`, `cells()` non-empty, and `theater()=="TEMPERATE"`. Document the chosen map filename.

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** — parse `[Map] Theater`, `Size`/`LocalSize` for dimensions. For `[IsoMapPack5]`: concatenate the ordered values from `Ini::section("IsoMapPack5")`, `base64_decode`, `decompress_format5`, then walk **11-byte** records: `int16 X, int16 Y, int32 Tile, uint8 SubTile, uint8 Level, uint8 (ignored)`. Skip records with `Tile==0xFFFFFFFF`/out-of-range as empty. Bounds-check. Confirm the 11-byte layout against OpenRA's `ReadTiles` / ModEnc during implementation; if the real-map test yields implausible coordinates, re-check record size/endianness (the tripwire).

- [ ] **Step 4: Build, run `-tc="map*"` (PowerShell, RA2_INSTALL_DIR) — PASS.** Report cell count + dimensions for the chosen map.

- [ ] **Step 5: Commit** `feat(map): parse .map/.mpr IsoMapPack5 into a cell grid`.

---

## Task 6: `Theater` / TMP tile decoder (pure lib)

Decodes `.tmp` isometric templates from `temperat.mix` into indexed tile images. Header layout is known (`t_tmp_ts_header`, `t_tmp_image_header` in `cc_structures.h`); the diamond pixel unpack is verified against an XCC oracle.

**Files:** Create `include/ra2assets/theater.hpp`, `src/theater.cpp`, `tests/test_theater.cpp`. Modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `MixFile`, `Palette` (Phase 1).
- Produces:
  ```cpp
  namespace ra2assets {
    struct TileImage { int w, h; std::vector<uint8_t> indices; };   // w=cx (48/60), h=cy (24/30)
    class TmpFile {
    public:
      static TmpFile load(std::vector<uint8_t> bytes);
      int count() const;                 // cblocks_x*cblocks_y
      int tile_w() const; int tile_h() const;  // cx, cy
      TileImage decode(int i) const;     // frame-local indexed diamond -> w*h buffer (0 = transparent margins)
    };
  }
  ```

- [ ] **Step 1: Failing tests** — (a) synthetic: hand-build a minimal 1-tile `.tmp` (header `cblocks_x=1,cblocks_y=1,cx=48,cy=24`; index[0]=offset; one `t_tmp_image_header` with `x=0,y=0,height=0`, flags 0; then the 576-byte diamond of a constant index), decode, assert `w==48,h==24` and the diamond region carries the value while corners are 0. (b) **Guarded real-tile oracle test**: build `xcc_tmp_dump` (outside repo, XCC `decode`/`decode_tile` verbatim) to dump reference indexed tiles for a real temperate `.tmp`; assert our `decode(i)` matches byte-for-byte over all tiles. Skip if assets absent; assert `tiles_checked>0`.

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** — read `t_tmp_ts_header` (4×int32); the `int32[count]` index table follows; each nonzero entry is a byte offset to a `t_tmp_image_header` (fields per `cc_structures.h`: `x,y,extra_ofs,z_ofs,extra_z_ofs,x_extra,y_extra,cx_extra,cy_extra`, then a bitfield byte, `height`, `terrain_type`, `ramp_type`, radar colors, pad — total documented size). Tile pixels start immediately after the image header (`= header + 1`), stored as a **diamond** of `cx*cy/2` bytes. Unpack the diamond into a `cx*cy` buffer (rows widen then narrow, centered) — implement per the TS iso-tile row-width pattern and **verify against the xcc_tmp_dump oracle** (do not guess the row widths; the oracle is the arbiter, matching the Phase-1 SHP approach). Empty index entries → all-zero tile. Bounds-check every offset.

- [ ] **Step 4: Build, run `-tc="tmp*" -tc="theater*"` (PowerShell) — PASS incl. oracle.** Record tiles compared + the `.tmp`/theater used in `docs/re/theater.md`.

- [ ] **Step 5: Commit** `feat(theater): TS TMP tile decoder (oracle-verified)`.

---

## Task 7: `TextureCache` — indexed+palette → SDL_Texture (engine)

**Files:** Create `engine/texture_cache.hpp`, `engine/texture_cache.cpp`. Modify `CMakeLists.txt` (add to `ra2engine`). Test: `tests/test_texture_cache.cpp` is **not** added (SDL texture creation needs a renderer/GPU; verified via the render tasks instead — note this in the task).

**Interfaces:**
- Consumes: `Palette`, `ShpFile::palettize`/inline palettize, `Platform::renderer()`.
- Produces:
  ```cpp
  namespace ra2engine {
    class TextureCache {
    public:
      explicit TextureCache(void* sdl_renderer);
      ~TextureCache();
      // Palettize indexed w*h -> RGBA (index 0 transparent) -> cached SDL_Texture. key identifies the asset+frame.
      void* get(const std::string& key, const uint8_t* indices, int w, int h, const ra2assets::Palette& pal);
    };
  }
  ```

- [ ] **Step 1: Implement** — maintain `std::unordered_map<std::string, SDL_Texture*>`. On miss: build an RGBA buffer (`ra2assets::Rgba`, index 0 → `{0,0,0,0}`, else `pal[i]`), `SDL_CreateTexture(STREAMING or STATIC, SDL_PIXELFORMAT_ABGR8888, w,h)`, `SDL_UpdateTexture`, set `SDL_BLENDMODE_BLEND`, store. Destructor destroys all. (No unit test; exercised by Tasks 9-10.)

- [ ] **Step 2: Build `ra2engine` — compiles/links.**

- [ ] **Step 3: Commit** `feat(engine): texture cache (indexed+palette -> SDL_Texture)`.

---

## Task 8: Isometric projection (engine, pure math — unit-tested)

**Files:** Create `engine/iso_renderer.hpp` (projection declared here), `engine/iso_renderer.cpp`. Test: `tests/test_projection.cpp`. Modify `CMakeLists.txt` — add `tests/test_projection.cpp` to `ra2assets_tests` (the projection functions are pure and can live testable without SDL: put them in a small `iso_project.hpp/.cpp` compiled into BOTH `ra2assets_tests` and `ra2engine`, or make them header-only inline in `iso_renderer.hpp` and include from the test).

**Interfaces:**
- Produces (pure, no SDL):
  ```cpp
  namespace ra2engine {
    struct ScreenPt { int x, y; };
    // RA2 diamond iso: tile cx=60,cy=30 (or 48/24). cell (cx,cy,level) -> screen pixel (top-left of tile diamond).
    ScreenPt cell_to_screen(int cell_x, int cell_y, int level, int tile_w, int tile_h, int level_px);
    // Inverse: screen pixel -> nearest cell (ignoring level).
    void screen_to_cell(int px, int py, int tile_w, int tile_h, int& cell_x, int& cell_y);
  }
  ```

- [ ] **Step 1: Failing tests** (`tests/test_projection.cpp`) — round-trip: for a grid of cells, `cell_to_screen` then `screen_to_cell` returns the original cell (level 0); assert the diamond stagger (odd rows offset by half a tile) matches RA2's projection: cell (x,y) → `px=(x - y)*tile_w/2`, `py=(x + y)*tile_h/2 - level*level_px` (confirm sign/orientation against OpenRA's TS `Map` projection during implementation).

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** the two functions per the formula above; `screen_to_cell` inverts (`x=(px/(tw/2)+py/(th/2))/2`, etc.) with rounding.

- [ ] **Step 4: Build, run `-tc="proj*"` — PASS.**

- [ ] **Step 5: Commit** `feat(engine): isometric cell<->screen projection (tested)`.

---

## Task 9: Terrain render — real map on screen (engine) — VISUAL MILESTONE 1

**Files:** Modify `engine/iso_renderer.*` (add `draw_terrain`), `engine/main.cpp` (load real assets + draw). Modify `CMakeLists.txt` if new sources.

**Interfaces:**
- Consumes: `MapFile`, `TmpFile`/`Theater`, `Palette`, `TextureCache`, projection (Task 8).
- Produces: `IsoRenderer::draw_terrain(const MapFile&, Theater&, camera)` that draws each cell's tile texture at its projected screen position, in back-to-front order.

- [ ] **Step 1: Asset loading in `main.cpp`** — via `ra2assets`: open `ra2.mix`/`ra2md.mix` from `RA2_INSTALL_DIR`; extract the chosen `.map` (Task 5), `temperat.mix`'s tiles + `isotem.pal`, build a `Theater` mapping `MapCell.tile`→`TmpFile`+subtile. (A `Theater` helper that, given a tile id, returns the right `TmpFile` and sub-tile image; theater tile→file mapping comes from the theater's tile set — for the slice, load the tiles referenced by the map's cells; document how tile ids map to `.tmp` entries, cross-checked with OpenRA.)

- [ ] **Step 2: Implement `draw_terrain`** — for each cell (iterate in `y` then `x` for correct overlap), compute `cell_to_screen`, offset by camera, get/roll the tile texture via `TextureCache`, `SDL_RenderCopy` at the tile rect. Clip to the window.

- [ ] **Step 3: Wire the frame loop** — `begin_frame()` → `draw_terrain(...)` → `present()`.

- [ ] **Step 4: Build + run (PowerShell, RA2_INSTALL_DIR).** Expected: the window shows the real map's isometric temperate terrain. Capture a screenshot into the scratchpad (outside the repo) and confirm it looks like RA2 terrain (not garbage/misaligned). Record in the report. If tiles are offset/overlapping wrong, re-check projection sign + draw order + tile anchor (`t_tmp_image_header.x/y`).

- [ ] **Step 5: Commit** `feat(engine): render real temperate map terrain (desktop)`.

---

## Task 10: Unit sprite render (engine)

**Files:** Modify `engine/iso_renderer.*` (add `draw_unit`), `engine/world.hpp/.cpp` (introduce `Unit`), `engine/main.cpp`.

**Interfaces:**
- Consumes: `ShpFile` (Phase 1), `Rules`, projection, `TextureCache`.
- Produces: a `Unit { int cell_x, cell_y; int frame; std::string image; }` and `IsoRenderer::draw_unit(const Unit&, ShpFile&, Palette&, camera)` drawing the unit's current SHP frame centered on its cell.

- [ ] **Step 1: Introduce `World`/`Unit`** (`engine/world.hpp`): a `World` holding the `MapFile` reference, a passability grid (Task 11 fills it), and one `Unit` placed at a chosen passable start cell (from map start or a hard-picked passable cell; document). `main.cpp` loads `gi.shp` + its palette (Phase-1 proven) and constructs the unit.

- [ ] **Step 2: Implement `draw_unit`** — project the unit's cell to screen, offset by camera and by the sprite's frame origin so the infantry stands centered on the cell; `TextureCache` the current frame; `SDL_RenderCopy`. Draw after terrain.

- [ ] **Step 3: Build + run.** Expected: a GI sprite stands on the terrain at its cell. Screenshot + report.

- [ ] **Step 4: Commit** `feat(engine): draw a unit sprite on the map`.

---

## Task 11: A* pathfinding (engine, pure — unit-tested)

**Files:** Create `engine/pathfind.hpp`, `engine/pathfind.cpp`. Test: `tests/test_pathfind.cpp` (pure — compile into `ra2assets_tests`). Modify `CMakeLists.txt`.

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2engine {
    struct Cellxy { int x, y; };
    // A* on a width*height passability grid (passable[y*width+x]!=0). Returns cell path incl. start+goal, or empty if none.
    std::vector<Cellxy> find_path(const std::vector<uint8_t>& passable, int width, int height,
                                  Cellxy start, Cellxy goal);
  }
  ```

- [ ] **Step 1: Failing tests** — open grid straight-line length; a wall forcing a detour; fully-blocked goal → empty; start==goal → single cell. 8-directional movement, octile heuristic.

- [ ] **Step 2: Run — FAIL.**

- [ ] **Step 3: Implement** standard A* (binary-heap open set, `g`/`f` maps, 8 neighbors, octile heuristic, block diagonal corner-cutting through impassable cells). Deterministic tie-breaking.

- [ ] **Step 4: Build, run `-tc="path*"` — PASS.**

- [ ] **Step 5: Commit** `feat(engine): A* pathfinding over passability grid`.

---

## Task 12: Input, selection, movement + animation (engine) — VISUAL MILESTONE 2

**Files:** Create `engine/input.hpp/.cpp`. Modify `engine/world.*` (movement/animation step), `engine/main.cpp` (wire input→sim→render). Modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `InputEvent` (Task 0), `find_path` (Task 11), `Rules::speed`, projection inverse.
- Produces: `World::step(double dt)` advancing the unit along its path and cycling walk frames; `Input` mapping pointer events to select/move; camera drag.

- [ ] **Step 1: Passability grid** — fill `World.passable` from `MapFile` cells (passable where a tile exists and its land type is walkable; for the slice, treat all mapped ground cells passable and off-map/empty impassable — refine later; document). 

- [ ] **Step 2: Selection + move** — pointer-down on the unit's cell selects it (highlight, e.g. tint or outline); pointer-down elsewhere while selected → `screen_to_cell` (minus camera) → `find_path` from the unit's cell → store path on the unit.

- [ ] **Step 3: Movement + animation in `World::step(dt)`** — advance the unit toward the next path cell at `Rules::speed`-derived pixels/sec (interpolate screen position between cells for smoothness); cycle SHP walk frames on a timer; pop cells as reached; stop at path end.

- [ ] **Step 4: Camera drag** — pointer-drag (empty space, or a modifier) scrolls the camera; clamp to map bounds.

- [ ] **Step 5: Wire loop** — pump `InputEvent`s → `Input` updates selection/target → `World::step(dt)` → render terrain, unit (at interpolated pos), selection marker.

- [ ] **Step 6: Build + run (desktop).** Expected: tap the GI to select; tap a destination; it walks there around impassable cells, animated; drag scrolls the map. Screenshots of before/after move + report. THIS IS THE DESKTOP-PLAYABLE GATE.

- [ ] **Step 7: Commit** `feat(engine): tap-select, tap-move with pathfinding + walk animation`.

---

## Task 13: Consolidate + desktop acceptance pass

**Files:** Modify as needed; add `README.md` engine section (desktop build/run).

- [ ] **Step 1:** Run the FULL test suite (PowerShell, RA2_INSTALL_DIR) — all pure-lib + projection + pathfinding tests green; oracle tests (lcw, tmp) green; Phase-1 suite still green. Record counts.
- [ ] **Step 2:** Full desktop acceptance: launch `ra2engine`, verify Success Criteria 1-2 (real terrain renders; select + pathfound animated move; camera scroll). Capture screenshots.
- [ ] **Step 3:** `README.md` — document desktop build (`cmake -B build`, `--target ra2engine`) and run (PowerShell, `RA2_INSTALL_DIR`), and the chosen map/unit asset names.
- [ ] **Step 4: Commit** `docs(engine): desktop build/run instructions + acceptance notes`.

---

## Task 14: Android arm64 APK — VISUAL MILESTONE 3 (on device)

**Files:** Create `android/` (SDL2 Android project: `build.gradle`, `AndroidManifest.xml`, a thin `SDLActivity` subclass, `jni/`/CMake linking `ra2engine` sources + `ra2assets` for `arm64-v8a`). Modify `README.md` (Android build).

**Interfaces:** Produces an installable `.apk` launching into the scene with tap-to-select/tap-to-move.

- [ ] **Step 1: Scaffold the SDL2 Android project** from SDL2's `android-project` template (SDL 2.30.9). Point its native build at our `engine/` sources + the `ra2assets` library, compiled for `arm64-v8a` via NDK r27c (reuse the Phase-1 NDK toolchain knowledge). App is `SDLActivity`-based; no menus.

- [ ] **Step 2: Asset access on device** — decide asset delivery: for the slice, push the needed game files to app-external storage (e.g. `/sdcard/Android/data/<pkg>/files/ra2/`) and set `RA2_INSTALL_DIR` (or an engine config path) to it; document the exact `adb push` of `ra2.mix`/`ra2md.mix` (+ whatever the loader needs). Do NOT bundle game assets into the APK (copyright).

- [ ] **Step 3: Touch input** — confirm `SDL_FINGER*` events drive the same select/move code as mouse (Task 0 already maps them). Scale finger (normalized) coords to window pixels.

- [ ] **Step 4: Build the APK.**
```bash
cd android && ./gradlew assembleDebug   # produces app/build/outputs/apk/debug/*.apk
```
Expected: an `arm64-v8a` debug APK builds.

- [ ] **Step 5: Install + run on device (ADB).** `adb install -r <apk>`; push assets (Step 2); launch. Expected: the app opens into the real map; tap-select and tap-move work with touch. Capture a device screenshot. THIS IS THE APK GATE (Success Criterion 3).

- [ ] **Step 6:** `README.md` — Android build/install/asset-push instructions.

- [ ] **Step 7: Commit** `build(android): SDL2 arm64 APK running the vertical slice on device`.

---

## Self-Review Notes (author checklist)

- **Spec coverage:** map-pack decompress (Tasks 1-2) ✓; `MapFile` (Task 5) ✓; `Theater`/TMP (Task 6) ✓; `Rules` (Task 4) ✓; SDL platform (Task 0) ✓; `TextureCache` (Task 7) ✓; `IsoRenderer` projection+terrain+unit (Tasks 8-10) ✓; `World`+A*+input+anim (Tasks 11-12) ✓; Android APK (Task 14) ✓; all four Success Criteria mapped (terrain=T9, interactive=T12, APK=T14, tests/purity=T1-8,11,13). Non-goals stay out.
- **Purity:** all new parsers (lcw, mappack, ini, rules, map, theater) live in `ra2assets` with no SDL/OS includes; SDL confined to `engine/` (Tasks 0,7,9,10,12) and `android/` (Task 14).
- **Clean-room:** lcw/tmp/map-pack reimplemented from public specs + standard algorithms, oracle-verified against XCC (outside repo), never transcribed — per Global Constraints and the Phase-1 licensing outcome.
- **Type consistency:** `MapCell`, `MapFile`, `TileImage`, `TmpFile`, `Rules`, `Ini`, `TextureCache`, `Unit`, `World`, `Cellxy`, `ScreenPt`, `find_path`, `cell_to_screen` used consistently across tasks. `Palette`/`Rgba`/`ShpFile`/`MixFile`/`mix_id` reused from Phase 1.
- **Reference-gated bodies (intentional, not placeholders):** the TMP diamond unpack (Task 6) and LCW real-data parity (Task 1) are verified against XCC oracles rather than hand-derived, matching the Phase-1 method; each has a byte-oracle gate as its correctness proof.
- **Known planning-time confirmations** (flagged in-task, not deferred vaguely): exact IsoMapPack5 record size/endianness (Task 5, tripwire = implausible coords), TMP diamond row widths (Task 6, oracle-arbitrated), projection sign/orientation (Task 8, vs OpenRA), chosen stock map + tile→.tmp mapping (Tasks 5,9, documented on selection).
