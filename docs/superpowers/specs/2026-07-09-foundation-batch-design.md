# Foundation Batch (Phase 2.5) — Design

**Status:** Approved for planning (2026-07-09)

## Goal

Build, in parallel, a set of **independent, self-contained foundation pieces**
that later gameplay phases of the RA2 engine will consume — without touching
the coupled engine core (World / renderer / main loop). Three are new
byte-format **decoders** whose exact logic is recovered from the game's own
loader in `gamemd.exe` via Ghidra; the fourth is a self-contained **startup
performance** change. Each is small enough to be one plan built by one agent
on its own git worktree, then merged back conflict-free (new files + one CMake
source-list line).

## Context / where the project is

- **Phase 1 (complete):** `ra2assets` — portable C++17 library decoding RA2
  MIX/PAL/SHP + Blowfish/key-derivation, byte-oracle-gated against XCC.
- **Phase 2 (complete):** `ra2engine` — SDL2 front-end rendering one real
  temperate map (terrain + cliffs) with one tap-to-move GI infantry unit (A\*
  pathfinding, walk animation). Runs on desktop and as an arm64/x86_64 Android
  APK.
- The road to a playable game is a sequence of **coupled** gameplay phases
  (render-real-map → multi-unit → combat → vehicles → economy → fog → AI →
  HUD → menus) that share the engine core and must be built sequentially.
- This batch is the **parallel-safe layer beneath them**: the pieces have no
  dependencies on each other and are consumed by far-future phases, so they can
  be banked now, concurrently, at zero merge risk.

## Global constraints (bind every piece)

- **Language / purity:** C++17, no compiler extensions. All three decoders
  live in the pure `ra2assets` library (`include/ra2assets/*`, `src/*`) and
  contain **no** SDL/Win32/OS/DirectX calls — pure functions over byte spans /
  `std::istream`, exactly like the Phase-1 decoders.
- **Ghidra-exact logic:** each decoder's algorithm is recovered by decompiling
  the loader in the pinned `gamemd.exe` (SHA1
  `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`, YR 1.001) in the existing
  `D:\ra2-re` Ghidra project (`ra2`) from Phase 1. Ghidra access is
  **read-only** (decompilation export), so parallel agents never contend.
- **No third-party GPL code committed.** OpenRA / XCC / references may be read
  off-repo (`D:\ra2-re\reference`) only to cross-check; the shipped code is an
  independent reimplementation of the game's own logic.
- **No game assets, no game binary, no Ghidra project committed.** Decoders
  read the user's own files at test time via `RA2_INSTALL_DIR` (default
  `D:\Red Alert 2`) and **skip cleanly** when assets are absent, so asset-free
  CI still builds and passes.
- **Toolchain / running tests:** CMake + Ninja + MinGW-w64 (host); doctest.
  Test executables are run from **PowerShell**, not Git Bash (known
  environment issue). Each new source is added to `cmake/ra2_sources.cmake`
  (the single source list shared by desktop + Android builds) so both builds
  pick it up with no drift.
- **RE docs:** each decoder appends a short `docs/re/<piece>.md` — the located
  function(s), address(es), and a High/Med/Low confidence rating — extending
  the Phase-1 `docs/re/` methodology.

## Sub-project 1 — VXL + HVA voxel model decoder

**Consumed by:** Phase 6 (vehicles/tanks/aircraft — RA2 vehicles are voxel
models, not SHP). **Decode only — no rendering** (voxel rasterization is
Phase-6 engine work).

**Format:** a `.vxl` holds a file header, a 256-entry palette, then per-section
{header, body of voxel spans, tailer with a 3×4 transform, min/max bounds,
scale, and a normals-encoding mode (TS=2 / RA2=4)}. Each present voxel carries
a color index + a normal index. A sibling `.hva` holds, per animation frame,
a 3×4 transform matrix per section.

**Ghidra target:** the VXL model loader and HVA loader in `gamemd.exe` — voxel
span parsing, the normals table selection, and the transform-matrix layout.

**Interface (exact field set confirmed against the game loader):**
```cpp
namespace ra2assets {
struct VxlVoxel { uint8_t color; uint8_t normal; bool present; };
struct VxlSection {
    std::string name;
    int size_x, size_y, size_z;
    float scale;
    float transform[3][4];          // section transform (tailer)
    float min_bounds[3], max_bounds[3];
    uint8_t normals_mode;           // 2 (TS) | 4 (RA2)
    std::vector<VxlVoxel> voxels;   // size_x*size_y*size_z, present=false if empty
};
struct VxlModel {
    Rgba palette[256];
    std::vector<VxlSection> sections;
};
VxlModel load_vxl(const uint8_t* data, size_t len);

struct HvaSectionXform { float transform[3][4]; };
struct HvaModel {
    int frame_count = 0;
    std::vector<std::string> section_names;
    std::vector<std::vector<HvaSectionXform>> frames;  // [frame][section]
};
HvaModel load_hva(const uint8_t* data, size_t len);
}
```

**Files:** `include/ra2assets/vxl.hpp`, `src/vxl.cpp`, `include/ra2assets/hva.hpp`,
`src/hva.cpp`, `tests/test_vxl.cpp`, `docs/re/vxl.md`.

**Verification:** (1) Ghidra-exact span/normal/transform logic. (2) Structural
oracle on a real vehicle model from the install (e.g. a tank `.vxl`/`.hva`
resolved from `ra2.mix`): section count, per-section dimensions, voxel counts,
palette, and per-frame transform counts cross-checked against a reference
decoder's dump kept off-repo. Skips cleanly without assets.

## Sub-project 2 — CSF string-table reader

**Consumed by:** Phases 10/11 (HUD + menu text). **Produces label→text only** —
no UI.

**Format:** ` FSC`/`CSF ` header, then `LBL ` label records each pointing at
one or more ` RTS`/`WRTS` value records. Values are stored as **UTF-16LE with
every 16-bit unit bitwise-inverted** (`~`).

**Ghidra target:** the CSF loader in `gamemd.exe` — header validation, the
label/value record walk, and the exact string bit-inversion + any label
casing/hashing.

**Interface:**
```cpp
namespace ra2assets {
class CsfStrings {
public:
    static CsfStrings load(const uint8_t* data, size_t len);
    const std::u16string* find(const std::string& label) const;  // nullptr if absent
    size_t count() const;
private:
    // label -> first value (extra/secondary values ignored for this milestone)
    std::unordered_map<std::string, std::u16string> table_;
};
}
```

**Files:** `include/ra2assets/csf.hpp`, `src/csf.cpp`, `tests/test_csf.cpp`,
`docs/re/csf.md`.

**Verification:** (1) Ghidra-exact record walk + inversion. (2) Oracle against
`ra2.csf` in the install: `count() > 0` and a handful of known
label→text pairs decode correctly (fixed vectors read from the real file).
Skips cleanly without assets.

## Sub-project 3 — AUD audio decoder

**Consumed by:** the audio phase (EVA / unit responses / SFX / music). **Decode
to PCM only** — no playback.

**Format:** Westwood `.aud` header (sample rate u16, compressed size u32,
uncompressed size u32, flags u8 [stereo/16-bit], type u8 [1 = WW-ADPCM,
99 = IMA-ADPCM]); then chunks each `{compressed u16, uncompressed u16, id
0x0000DEAF u32}` of IMA-ADPCM nibbles.

**Ghidra target:** the AUD decoder in `gamemd.exe` — the exact IMA step table,
index-adjust table, predictor accumulation, and sample clamping.

**Interface:**
```cpp
namespace ra2assets {
struct AudInfo { int sample_rate = 0; int channels = 1; int bits = 16; };
// Decodes an AUD to interleaved little-endian 16-bit PCM.
std::vector<int16_t> decode_aud(const uint8_t* data, size_t len, AudInfo& out_info);
}
```

**Files:** `include/ra2assets/aud.hpp`, `src/aud.cpp`, `tests/test_aud.cpp`,
`docs/re/aud.md`.

**Verification:** (1) Ghidra-exact tables + clamping. (2) Oracle: decode a real
`.aud` from the install; assert the decoded sample count matches the header's
uncompressed size and the PCM matches a reference decoder's output for that
file (reference kept off-repo). Skips cleanly without assets.

## Sub-project 4 — Startup performance (lazy/cached tile decode)

**Consumed by:** everything — the ~2-minute cold start (measured on the Android
emulator and reported on-device) makes iterating painful and worsens as later
phases load more. This is the one **non-RE** piece: it is our own optimization,
not game logic, so it has **no Ghidra step** — its correctness gate is that
decoded output is byte-identical to today's.

**Change:** today `Theater`/asset load eagerly decodes every theater template up
front. Make tile decoding **lazy + memoized**: keep the raw TMP bytes, decode a
`(tile, subtile)` on first request, cache the decoded indexed pixels; the SDL
`TextureCache` already memoizes the GPU texture on top. `load_assets()` stops
pre-decoding the full template set. Touches `engine/theater.cpp`,
`engine/theater.hpp`, and possibly `engine/main.cpp`'s load path only.

**Files (edit, not new):** `engine/theater.{hpp,cpp}` (+ `engine/main.cpp` load
path); `tests/test_theater.cpp` (add a lazy-decode-equivalence + memoization
test).

**Verification:** (1) The existing theater/oracle tests still pass — every
decoded tile is byte-identical to the eager path (the primary gate). (2) A new
test asserts a tile decoded lazily equals the eager decode and that a second
request is served from cache (no re-decode). (3) Measured: time-to-first-frame
drops from minutes to seconds (report the before/after; not a unit assertion).

## Shared verification methodology

Every decoder (pieces 1–3) is gated by **two independent checks**, both
grounded in the user's own files:

1. **Ghidra-exact:** the shipped algorithm is the decompiled logic of the
   game's own loader (located, decompiled, reimplemented, confidence-rated in
   `docs/re/<piece>.md`) — the authoritative source of truth, and GPL-clean.
2. **Real-asset oracle:** decode a real asset and check output (structural for
   VXL, known-vector for CSF, PCM-equality for AUD), skipping cleanly when
   `RA2_INSTALL_DIR` assets are absent, so asset-free CI still builds/links.

TDD throughout (failing test first). Test output must be pristine.

## Parallel execution model

- **One git worktree + one implementer agent per piece**, run concurrently
  (fanned out by a workflow). Pieces 1–3 only **add** files + one
  `cmake/ra2_sources.cmake` line each; piece 4 edits `engine/theater.*` only —
  disjoint file sets, so merge-back is conflict-free.
- Each piece follows the Phase-1 review loop: implement (TDD) → independent
  spec+quality review → fix Critical/Important → re-review → merge.
- After all four merge, a **light integration pass**: the full suite builds and
  stays green (host), and the Android APK still builds (the new library sources
  compile under the NDK).
- Ghidra runs **read-only** against the existing analyzed `D:\ra2-re` `ra2`
  project — agents export decompilation concurrently without locking.

## Out of scope (explicitly deferred to later phases)

- **Rendering** voxels (Phase 6), **playing** audio (audio phase), and **using**
  CSF strings in any UI (Phases 10/11). This batch decodes/produces data only.
- Secondary/extra CSF values per label (first value suffices for now).
- WW-ADPCM type-1 AUD if the located assets are all IMA type-99 — implement the
  type the real assets use; record if type-1 is unneeded.
- Any change to the coupled engine core beyond piece 4's self-contained load
  path.

## Success criteria

1. Three new decoders (`vxl`+`hva`, `csf`, `aud`) in the pure library, each
   Ghidra-exact and real-asset-oracle-verified, tests skip cleanly without
   assets.
2. Startup: map opens in seconds, with decoded tiles byte-identical to before.
3. `docs/re/{vxl,csf,aud}.md` document the located game loaders with confidence
   ratings.
4. Full host suite green; Android APK still builds with the new sources.
5. No game assets / binary / Ghidra project / GPL source committed.
