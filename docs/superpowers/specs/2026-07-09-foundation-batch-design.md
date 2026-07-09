# Foundation Batch (Phase 2.5) — Design

**Status:** Approved for planning (2026-07-09). **Amended same day** after
verifying every piece against the real install (see "Ground-truth
corrections" below): the audio piece targets the formats this game actually
ships (BAG/IDX + IMA-ADPCM, not headered `.aud`), the voxel oracle assets
live in `ra2.mix/local.mix`, and the startup fix targets the measured
bottleneck (byte-wise MIX slurping in the engine load path — tile decode is
already lazy).

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

## Ground-truth corrections (verified against the install, 2026-07-09)

Every claim below was verified by extracting and inspecting the real files
with `ra2extract` before planning:

1. **Voxel models live in `ra2.mix/local.mix`** (not `conquer.mix`): 84
   `.vxl`/`.hva` files resolved by name there, including `htnk.vxl` (34,550
   bytes) + `htnk.hva` (88 bytes) — the Rhino tank, with separate
   `htnktur`/`htnkbarl` turret/barrel models. The VXL layout is confirmed by
   arithmetic: 802-byte file header (16 magic `"Voxel Animation"` + 5 u32 +
   2 remap bytes + 768-byte palette) + 28-byte section header + body
   (`bodysize` = 33,628) + 92-byte section tailer = 34,550 exactly.
2. **This install contains NO headered `.aud` files.** The game's audio is:
   * SFX + unit voices: `language.mix → audio.mix → audio.idx` (`GABA`
     magic, version 2, 1,153 entries; 12-byte header + 1,153 × 36-byte
     entries = 41,520 bytes exactly) + `audio.bag` (**Westwood IMA-ADPCM**
     clip data, chunked: `chunk_size`-framed blocks each led by a small
     predictor/index header, per XCC's own BAG decode path; entries with
     flag bit 1 are raw 16-bit PCM instead — every real PCM entry has
     `chunk_size` 0, every ADPCM entry 512/1024).
   * EVA speech: `audio.mix → <name>.wav` (459 files, e.g. `ceva001.wav`) —
     genuine RIFF WAVE, `fmt` tag 0x0011 (IMA ADPCM), mono 22,050 Hz.
   * Music: `theme.mix → <Sound>.wav` (16 RIFF WAVE files named by
     `theme.ini` `Sound=` keys, e.g. `Grinder.WAV`).
   Sub-project 3 therefore decodes **IDX/BAG + IMA-ADPCM** (the codec is the
   same IMA core the spec's `.aud` type-99 description named; only the
   container changed). Headered-AUD support is dropped as asset-less here.
3. **CSF:** `ra2.csf` sits **loose** in the install root (491,826 bytes,
   magic `" FSC"`, version 3, 4,562 label records — 4,558 distinct labels
   after case-insensitive normalization: exactly four duplicate pairs exist,
   `txt_defense_tab`, `txt_defense_tab_desc`, `txt_structure_tab`,
   `txt_structure_tab_desc` each appearing in both cases — direct real-file
   evidence for case-insensitive lookup). Known-good vectors decoded from it:
   `GUI:Cancel` → "Cancel", `Name:HTNK` → "Rhino Heavy Tank", `Name:E1` →
   "GI", `GUI:LoadGame` → "Load Game". Values are UTF-16LE with every byte
   XOR 0xFF (equivalent to `~` per 16-bit unit).
4. **Tile decode is ALREADY lazy** (`Theater::tmp_for_tile` memoizes
   templates; `draw_terrain` decodes a subtile only on texture-cache miss).
   The real startup cost — **measured 79 s on the desktop host** for
   `ra2engine --screenshot` — is the engine's `slurp()`
   (`std::istreambuf_iterator`, byte-at-a-time, no reserve) reading
   `ra2.mix` (268 MB) **twice** plus `ra2md.mix` (195 MB) once ≈ 730 MB of
   byte-wise reads. Sub-project 4 replaces that load path (block reads +
   open `ra2.mix` once); its byte-identical gate is unchanged.

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

**Verification:** (1) Ghidra-exact span/normal/transform logic. (2) Oracle on
a real vehicle model from the install — `htnk.vxl` + `htnk.hva` (Rhino tank)
resolved from `ra2.mix/local.mix` — section count, per-section dimensions,
per-voxel (position, color, normal) bytes, palette, and per-frame transform
counts cross-checked against a reference decoder's dump kept off-repo
(XCC's own VXL code built into a standalone dump tool under `D:\ra2-re\`,
the Task-9 pattern). Skips cleanly without assets.

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
the loose `ra2.csf` in the install root: `count() > 0` and the known
label→text vectors listed in "Ground-truth corrections" decode correctly.
Skips cleanly without assets.

## Sub-project 3 — Audio decoder (BAG/IDX + Westwood IMA-ADPCM)

**Consumed by:** the audio phase (EVA / unit responses / SFX / music). **Decode
to PCM only** — no playback.

**Format (as shipped in THIS install — see Ground-truth corrections):**
* `audio.idx` (in `language.mix → audio.mix`): header `{char magic[4] =
  "GABA"; u32 version = 2; u32 count}`, then `count` × 36-byte entries
  `{char name[16]; u32 offset; u32 size; u32 sample_rate; u32 flags;
  u32 chunk_size}` (entry layout to be confirmed exactly by Ghidra).
* `audio.bag`: at each entry's `offset`/`size`, the clip payload. ADPCM
  entries are **chunked**: consecutive `chunk_size`-byte blocks, each led by
  a 4-byte header `{i16 predictor; u8 step_index; u8 reserved}` that restarts
  the IMA state (XCC's own BAG decode path; confirmed against the real data —
  ADPCM entries all have `chunk_size` 512/1024). Entries with flag bit 1 are
  raw 16-bit LE PCM (`chunk_size` 0). Exact flag-bit semantics
  (stereo/16-bit/PCM) pinned by the Ghidra task.
* The EVA/music RIFF WAVs use the same IMA-ADPCM codec inside standard WAV
  `fmt 0x0011` framing — out of scope for this piece (any WAV reader can
  frame them later; the codec is what matters).

**Ghidra target:** the BAG/IDX loader and ADPCM decoder in `gamemd.exe` — the
exact IMA step table, index-adjust table, predictor accumulation, sample
clamping, and the idx entry layout/flag bits. Anchors: the 89-entry IMA step
table constants and the `"audio.idx"`/`"audio.bag"` strings.

**Interface:**
```cpp
namespace ra2assets {
struct BagClip {
    std::string name;               // idx name field (no extension)
    uint32_t offset = 0, size = 0;  // clip bytes within audio.bag
    uint32_t sample_rate = 0, flags = 0, chunk_size = 0;
};
class AudioIdx {
public:
    static AudioIdx load(const uint8_t* data, size_t len);
    const BagClip* find(const std::string& name) const;  // nullptr if absent
    const std::vector<BagClip>& clips() const;
};
struct AudioClipInfo { int sample_rate = 0; int channels = 1; int bits = 16; };
// Decode one clip's bytes (from audio.bag) to interleaved 16-bit PCM.
std::vector<int16_t> decode_bag_clip(const uint8_t* bag_data, size_t bag_len,
                                     const BagClip& clip, AudioClipInfo& out_info);
}
```

**Files:** `include/ra2assets/audio_bag.hpp`, `src/audio_bag.cpp`,
`tests/test_audio.cpp`, `docs/re/audio.md`.

**Verification:** (1) Ghidra-exact tables + clamping + idx layout. (2) Oracle:
decode a real clip from the install's `audio.bag`; assert the PCM matches a
reference decoder's output for that clip (XCC's own ADPCM code built into a
standalone dump tool off-repo, the Task-9 pattern) and the sample count
matches the clip size implied by the format. Skips cleanly without assets.

## Sub-project 4 — Startup performance (fast MIX loading)

**Consumed by:** everything — the ~2-minute cold start on device (79 s
measured on the desktop host) makes iterating painful and worsens as later
phases load more. This is the one **non-RE** piece: it is our own
optimization, not game logic, so it has **no Ghidra step** — its correctness
gate is that loaded/decoded output is byte-identical to today's.

**Change (targets the measured bottleneck — see Ground-truth corrections):**
1. Add `ra2assets::read_file(path)` — a block-reading file slurp (seek to
   end, size, single `read()`) replacing the engine's per-byte
   `istreambuf_iterator` slurp.
2. Open `ra2.mix` **once** per run and pass the open `MixFile` to both
   `load_assets()` and `load_unit_assets()` (today each slurps the 268 MB
   file independently).
Tile decoding is already lazy + memoized and is **not** touched.

**Files:** Create `include/ra2assets/fileio.hpp`, `src/fileio.cpp`,
`tests/test_fileio.cpp`; modify `engine/main.cpp` (load path only) and
`tools/ra2extract.cpp` (same slurp swap).

**Verification:** (1) The full existing suite still passes (the load path
produces the same bytes; everything downstream is untouched). (2) A new test
asserts `read_file` returns byte-identical content to the old slurp and
throws on a missing file. (3) Measured: `ra2engine --screenshot` wall time
drops from 79 s to seconds on the host (report before/after; not a unit
assertion).

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
  `cmake/ra2_sources.cmake` line each; piece 4 adds `fileio.*` and is the
  only piece that edits `engine/main.cpp` / `tools/ra2extract.cpp` — disjoint
  file sets, so merge-back is conflict-free. (All four also append one
  `target_sources` line to the test executable in `CMakeLists.txt` and their
  library line to `cmake/ra2_sources.cmake`; these are one-line additive
  edits at a marked list end — trivial to merge.)
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
- Headered `.aud` files and WW-ADPCM "type 1": this install ships neither
  (verified — see Ground-truth corrections); implement the formats the real
  assets use and record the decision in `docs/re/audio.md`.
- RIFF WAV framing (EVA/music): same codec, trivial container, added later by
  the audio phase when playback lands.
- Any change to the coupled engine core beyond piece 4's self-contained load
  path.

## Success criteria

1. Three new decoders (`vxl`+`hva`, `csf`, `audio_bag`) in the pure library,
   each Ghidra-exact and real-asset-oracle-verified, tests skip cleanly
   without assets.
2. Startup: map opens in seconds (host `--screenshot` from 79 s to single
   digits), with loaded data byte-identical to before.
3. `docs/re/{vxl,csf,audio}.md` document the located game loaders with
   confidence ratings.
4. Full host suite green; Android APK still builds with the new sources.
5. No game assets / binary / Ghidra project / GPL source committed.
