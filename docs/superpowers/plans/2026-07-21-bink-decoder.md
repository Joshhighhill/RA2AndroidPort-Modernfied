# Bink1 Video Decoder + Menu CRT Animation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use `- [ ]` checkboxes.

**Goal:** From-scratch Bink1 (`BIKi` rev-'i') video decoder in `ra2assets`, then play `language.mix/ra2ts_s.bik` (the real RA2 menu animation: logo + rotating red radar sweep + Statue of Liberty) looping in the clone's main-menu CRT screen.

**Architecture:** New `ra2assets` module: `BinkFile` (container parser) + `BinkVideoDecoder` (bitstream → YUV420 planes → RGBA per frame). The menu (`engine/main.cpp`) loads `ra2ts_s.bik` from `language.mix`, decodes frames on a 15 fps clock in `ShellScene::update`, and blits the current frame into the CRT screen region (0,0,472,448 logical), replacing/over the static backdrop. Fully additive — no gate/decoder/world_hash touched.

**Tech Stack:** C++17, ra2assets (MixFile), SDL2/TextureCache (render side). Port target = `docs/re/bink_proof_decoder.py` (proven byte-exact vs ffmpeg). Authority = `docs/re/bink.md` (all tables + algorithm + exact-arithmetic traps).

## Global Constraints

- **Authority docs (read, use verbatim):** `docs/re/bink.md` (spec + tables) and `docs/re/bink_proof_decoder.py` (reference implementation — the C++ must match its logic). Do NOT re-derive tables; copy them from bink.md.
- All 18 byte-identity gates MUST stay bit-identical: combat=78, tank=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661, multihouse 39412ae996cce8ae, skirmish e9ad3f1fd88495f9, roster 333cd43d8b5ece6a.
- PURELY ADDITIVE: the Bink module is a NEW ra2assets TU (register in RA2_LIB_SOURCES + a new test_bink.cpp) — it must NOT modify any existing decoder (shp/pcx/fnt/wav/pal/csf/mix) or any `--demo-*`/`run_demo_*`/`simulate_*`/`world_hash`. Menu wiring is interactive-only (main.cpp menu region).
- **Exact-arithmetic (from bink.md — get these RIGHT or frames drift):** 32-bit *wrapping* multiplies in dequant and IDCT `MUL` (>>11 arithmetic shift); row munge `(x+127)>>8`; pixel adds wrap mod 256 (NO clamping); quant tables indexed by SCAN position while blocks store RASTER; quantizer index read AFTER the coefficients; DC is dequantized too; 32-bit alignment after each plane; video packet starts with a u32 (= Y-plane end offset) that is SKIPPED; plane decode order **Y, V, U**; COLORS bundle bytes used RAW (no <'i' sign-fold).
- **Test oracle:** ffmpeg (available) `ffmpeg -i ra2ts_s.bik -pix_fmt yuv420p -f rawvideo out.yuv` = byte-exact YUV reference; `bikframes/f%04d.png` = RGBA reference (±2 for BT.601 limited-range color conversion, swscale dithers). Real asset via `language.mix/ra2ts_s.bik` (guard tests with RA2_INSTALL_DIR like existing real-asset tests).
- No assets committed. Commit trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Build: `cmake --build build --target ra2assets_tests ra2engine`.

---

### Task 1: BinkFile container parser

**Files:** Create `include/ra2assets/bink.hpp`, `src/bink.cpp`; Test `tests/test_bink.cpp`. Modify the CMake source list (RA2_LIB_SOURCES) + tests list.

**Produces:** `struct BinkFile { int width, height, num_frames, fps_num, fps_den; std::vector<uint32_t> frame_offsets; std::vector<uint8_t> data; bool is_keyframe(int f); std::pair<const uint8_t*,size_t> frame(int f); static BinkFile parse(std::vector<uint8_t> bytes); };` (exact fields per bink.md container section).

Per bink.md: 44-byte LE header — magic "BIKi", filesize-8, num_frames, num_frames(2), largest_frame, ?, width, height, fps_num, fps_den, video_flags, num_audio_tracks(=0 here); then audio-track headers (none); then frame index = (num_frames+1) u32 absolute offsets (bit0 = keyframe flag, mask it off for the real offset). Frame f bytes = [offset[f]&~1, offset[f+1]).

- [ ] Step 1: Failing test — parse `language.mix/ra2ts_s.bik` (via MixFile): assert width==472, height==450, num_frames==431, fps==15/1, frame_offsets.size()==432, is_keyframe(0)==true, frame(0) non-empty, last offset == file size. (Guard: skip if RA2_INSTALL_DIR/language.mix absent.)
- [ ] Step 2: Run test → fails (no parser).
- [ ] Step 3: Implement `BinkFile::parse` per bink.md container layout (mask bit0 off offsets; keyframe = bit0 set).
- [ ] Step 4: Test passes.
- [ ] Step 5: Commit `feat(bink): BIKi container parser`.

---

### Task 2: Bit reader + entropy/bundle infrastructure

**Files:** Modify `src/bink.cpp` (+ internal headers). Test `tests/test_bink.cpp`.

**Produces (internal):** LSB-first `BitReader` (matches bink.md's exact bit order); Huffman tree build (`read_tree` / on-the-fly), the 16 bundle types with their RLE-length + Huffman decode, and the per-frame bundle-init + `read_bundle` machinery. Copy the 16 Huffman trees, 16 RUN scan patterns, and scan order verbatim from bink.md.

- [ ] Step 1: Failing unit test — feed a known bitstring, assert BitReader returns the documented bit sequence (LSB-first), and a small hand-built Huffman tree decodes a known symbol stream (values from bink.md / cross-check the proof decoder).
- [ ] Step 2: Run → fails.
- [ ] Step 3: Implement BitReader + Huffman + bundle read (port bink_proof_decoder.py's reader/bundle code; tables from bink.md).
- [ ] Step 4: Tests pass.
- [ ] Step 5: Commit `feat(bink): LSB bit reader + Huffman/bundle entropy layer`.

---

### Task 3: Frame decode — block types + IDCT + plane assembly (YUV)

**Files:** Modify `src/bink.cpp`, `include/ra2assets/bink.hpp`. Test `tests/test_bink.cpp`.

**Produces:** `BinkVideoDecoder` that decodes frame N into YUV420 planes (Y full res, V,U half res), decode order **Y, V, U**, holding the previous frame for INTER/MOTION. Implements ALL block types (SKIP, SCALED, MOTION, RUN, RESIDUE, INTRA, FILL, INTER, PATTERN, RAW) + the Bink integer IDCT + intra/inter dequant (tables from bink.md). Honor EVERY exact-arithmetic trap in Global Constraints.

**Interfaces:** `class BinkVideoDecoder { BinkVideoDecoder(const BinkFile&); const uint8_t* y_plane()/u_plane()/v_plane(); int y_stride()...; void decode_frame(int n); };` (decode is sequential; interframes need frame n-1 decoded first — the API may require decoding 0..n in order, document it).

- [ ] Step 1: Failing test — decode ra2ts_s.bik frame 0 (keyframe); dump YUV; compare **byte-exact** to ffmpeg `-pix_fmt yuv420p` frame 0 (generate the reference .yuv at test time via a committed helper OR pin an FNV hash of the Y/U/V planes computed from the proven decoder). Also a mid-stream interframe (e.g. frame 130) after sequential decode.
- [ ] Step 2: Run → fails.
- [ ] Step 3: Implement the frame decoder — port `bink_proof_decoder.py` block-by-block; tables from bink.md; exact-arithmetic traps enforced. Skip the leading u32 per packet; align 32-bit after each plane.
- [ ] Step 4: Test passes byte-exact on frame 0 + the interframe (and, in dev, run the full 431-frame compare against the ffmpeg .yuv to confirm no drift — note the result in the report).
- [ ] Step 5: Commit `feat(bink): Bink1 frame decoder (all block types + IDCT), YUV byte-exact vs ffmpeg`.

---

### Task 4: YUV→RGBA + public decode API

**Files:** Modify `src/bink.cpp`, `include/ra2assets/bink.hpp`. Test `tests/test_bink.cpp`.

**Produces:** `std::vector<uint8_t> BinkVideoDecoder::rgba();` — BT.601 **limited-range** YUV420→RGBA (w*h*4). Public convenience API for the engine (decode_frame(n) then rgba()).

- [ ] Step 1: Failing test — decode frame 0 → rgba(), compare to `bikframes/f0001.png` within ±2 per channel (swscale dithers; bink.md notes this). Assert size w*h*4.
- [ ] Step 2: Run → fails.
- [ ] Step 3: Implement BT.601 limited-range conversion (coefficients from bink.md), 4:2:0 chroma upsample as ffmpeg/spec does.
- [ ] Step 4: Test passes (±2).
- [ ] Step 5: Commit `feat(bink): YUV420→RGBA (BT.601 limited) + decode API`.

---

### Task 5: Play ra2ts_s.bik in the menu CRT

**Files:** Modify `engine/main.cpp` (MenuArt/load_menu_art + draw_shell + ShellScene::update).

Load `language.mix/ra2ts_s.bik` in load_menu_art (null-guarded); hold a `BinkVideoDecoder` + current frame index + accumulator in ShellScene. In `update(dt)`, advance at **15 fps** (accumulator; frame = (frame+1)%num_frames at 1/15s; sequential decode — decode next frame each tick, loop to 0 at end). In `draw_shell`, blit the current decoded RGBA frame (texture-from-pixels, updated each new frame) into the CRT screen rect (logical 0,0,472,448) — this replaces the static poster as the CRT content. Fall back to the static mnscrns backdrop if the .bik is absent (partial install). Keep the WARNING monitor + buttons unchanged.

- [ ] Step 1: Add ra2ts_s.bik load + BinkVideoDecoder to MenuArt/ShellScene; advance frame in update(dt) at 15fps looping.
- [ ] Step 2: Blit the current frame into the CRT rect in draw_shell (upload RGBA to a texture each new frame; guard if decoder absent → static backdrop).
- [ ] Step 3: Build ra2engine; run `--menu-shot <scratch>/bink_menu.png` (captures the current frame). Also capture 3 shots with a few update ticks between to confirm the frame advances (or reason it via the code).
- [ ] Step 4: VISUAL CHECK (controller): CRT shows the real RA2 logo+radar+sweep frame, animating; matches bikframes/. 
- [ ] Step 5: Commit `feat(menu): play real ra2ts_s.bik animation in the CRT`.

---

### Task 6: Finalize

**Files:** README.md, ledger.

- [ ] Step 1: `cmake --build build --target ra2assets_tests ra2engine`; run suite — bink tests pass, no regressions.
- [ ] Step 2: Re-verify all 18 byte-identity gates EXACT, one-per-call (~2min each; never batch; long timeouts). All match Global Constraints.
- [ ] Step 3: arm64 `android; ./gradlew.bat assembleDebug` — BUILD SUCCESSFUL; nm-verify the bink symbols link.
- [ ] Step 4: Final `--menu-shot` screenshots; controller eyeball = faithful animated CRT.
- [ ] Step 5: README "Bink CRT animation" section (from-scratch BIKi decoder; language.mix/ra2ts_s.bik; 15fps loop; ffmpeg-oracle tested) + ledger phase-complete. Commit `docs(bink): finalize + README + ledger`.

## Self-Review notes
- The proof decoder + ffmpeg oracle make correctness objective: Task 3 must be YUV byte-exact, Task 4 RGBA ±2.
- Biggest risk = the exact-arithmetic traps (wrapping mul, no-clamp adds, scan-vs-raster quant indexing, quantizer-after-coeffs). The reviewer must check each against bink.md.
- Memory: decode one frame at a time (prev + current), do NOT hold all 431 as textures.
