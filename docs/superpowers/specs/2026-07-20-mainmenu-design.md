# Faithful Main Menu — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the Skirmish shell phase (`docs/superpowers/specs/2026-07-20-skirmish-design.md` — provides `MainMenuScene`/`SkirmishSetupScene`, the `GameScene`, and the `run_menu` scene driver reached at the two non-gate `main()` seams) + the base ra2assets loader (SHP `ShpFile`, PAL, CSF, MIX nested extraction, mono IMA-ADPCM `wav.cpp`) + the base engine render path (`blit_shp`, `TextureCache`, `draw_text`/rectfont, `Platform`/SDL).
**Calibration authority:** `docs/re/mainmenu.md` (real RA2 base-game front-end composition + `ra2extract` asset-presence probe of this exact install — ALREADY COMPLETE; every menu art id CONFIRMED present in `D:\Red Alert 2`; the only blockers are decoder gaps in our pipeline — PCX, FNT, stereo-IMA WAV — not missing art). This spec is built on that recovered ground truth + the controller's binding design decisions (D1–D6), not design guesswork. Scope = original Red Alert 2, **Allied vs Soviet, NO Yuri**.

## Goal

The Phase-3 main menu is a functional **rect-font placeholder** — `MainMenuScene::render` (`engine/main.cpp:3535`) draws the title as a 5×7 bitmap-font string and six flat SDL-filled rectangles for buttons. It reads as the RA2 menu *shape* but is not a visual clone. This phase makes it **faithful** using the REAL assets already confirmed present in this install:

- **SHP background** `mnscrnl.shp` (632×568 hi-res, single frame) scaled/letterboxed into the window, palette-calibrated.
- **3-state SHP buttons** `mnbttn.shp` (126×25, frames = normal / hover / pressed), hit-tested via `SbRect::contains`, wired to the EXISTING Phase-3 menu actions.
- **PCX logo/title** `logo.pcx` / `title.pcx` — via a NEW PCX decoder added to ra2assets.
- **Westwood FNT menu text** (`12metfnt.fnt` / `metal12.fnt`) — via a NEW FNT decoder + a `draw_fnt_text` helper.
- **Looping menu music** `grinder.wav` (theme.mix, `[INTRO] Sound=Grinder Repeat=yes`) — via a NEW stereo IMA-ADPCM branch in `wav.cpp`, started on menu-scene enter and stopped on entering the game.
- **Dialog chrome** (`bkgdlg.shp` + `sidec01/02` `optbtn`/`tabs`/`button00`) styling the `SkirmishSetupScene` so the setup screen matches RA2's dialog look.

All of this is a **visual reskin over the working Phase-3 scene logic** — every existing action/flow (`Skirmish → SkirmishSetupScene → GameScene`, `Quit`, greyed stubs) is preserved.

**Byte-identity promise.** The menu is **INTERACTIVE-ONLY**: `MainMenuScene`/`SkirmishSetupScene` are reached only through the `run_menu` scene driver at the two non-gate `main()` seams (`engine/main.cpp:9356`, `:9469`); **no `--demo-*` gate renders the menu**. So all menu work is PURELY ADDITIVE and cannot move a gate hash. The three new asset decoders (PCX, FNT, stereo-IMA WAV) are ADDITIVE to the ra2assets library — they add NEW code paths and are host-tested; they must NOT change the existing SHP/PAL/CSF/mono-IMA-WAV decode paths that the gates' asset loading depends on. `world_hash` is untouched. All **18 existing byte-identity gates stay bit-identical**, re-verified after each decoder lands (the gates load assets, so we PROVE the new decoders did not perturb SHP/PAL/WAV decode). No new determinism gate is added — the menu is visual/interactive (covered by a screenshot), and the decoders are covered by host tests.

## Global Constraints

Copied VERBATIM from the controller's design direction. These bind every task.

- All 18 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661, multihouse 39412ae996cce8ae, skirmish e9ad3f1fd88495f9, roster 333cd43d8b5ece6a.
- The menu is INTERACTIVE-ONLY (MainMenuScene/SkirmishSetupScene, reached only via the run_menu scene driver at the two non-gate main() seams). NO --demo-* gate renders the menu. So all menu work is PURELY ADDITIVE and cannot move a gate hash. world_hash untouched.
- The NEW asset decoders (PCX, FNT, stereo-IMA WAV) are ADDITIVE to the ra2assets library: they add NEW code paths + are host-tested; they must NOT change the existing SHP/PAL/CSF/mono-IMA-WAV decode paths (which the gates' asset loading depends on). Verify the gates stay byte-identical after each decoder lands.
- No assets/GPL committed. Recovered asset ids/coords/palette names ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the asset contract)

Every value below is tagged as in `docs/re/mainmenu.md` ([CONFIRMED] = probed present in *this* install via `ra2extract`; [ANCHORED] = from the XCC name DB / documented RA2 / SHP headers; [RE-FALLBACK] = a port design decision resolved by Task 1). **Nothing menu-related is missing from this install** — every front-end art id resolved (`mainmenu.md §2, §6`). The limits are decoder gaps, not absent assets.

### Front-end asset table (`mainmenu.md §1, §2`)

| Element | Asset | Container (in `ra2.mix`) | Format | Dims / frames | Present |
|---|---|---|---|---|:--:|
| Menu background (hi-res) | `mnscrnl.shp` | `neutral.mix` | SHP | 632×568, 1 frame | [CONFIRMED] 359008 B |
| Menu background (lo-res) | `mnscrns.shp` | `neutral.mix` | SHP | 472×448, 1 frame | [CONFIRMED] 211488 B |
| Button strip (3-state) | `mnbttn.shp` | `local.mix` | SHP | 126×25, 3 frames (up/down/hover) | [CONFIRMED] 9536 B |
| RA2 logo | `logo.pcx` | `local.mix` | PCX | 163858 B | [CONFIRMED] |
| Title image | `title.pcx` | `local.mix` | PCX | 76413 B | [CONFIRMED] |
| Dialog chrome (Allied) | `bkgdlg.shp` (856×736), `optbtn`, `tabs`, `button00`, `diplobtn` | `sidec01.mix` | SHP | bkgdlg 630048 B | [CONFIRMED] |
| Dialog chrome (Soviet) | same names | `sidec02.mix` | SHP | bkgdlg 630048 B | [CONFIRMED] |
| Loadscreen (Allied) | `aloadlg.pcx` | `neutral.mix` | PCX | 659243 B | [CONFIRMED] (carry-forward) |
| Loadscreen (Soviet) | `rloadlg.pcx` | `neutral.mix` | PCX | 980802 B | [CONFIRMED] (carry-forward) |
| Palettes | `fsscrn.pal` (neutral.mix), `uibkgd.pal`/`sidebar.pal` (sidec01/02), `palette.pal`/`cameo.pal` (cache.mix) | — | PAL | 768 B each | [CONFIRMED] |
| Shell/menu font | `12metfnt.fnt` (cache.mix, 10637 B), `metal12.fnt` (local.mix, 7698 B) | — | Westwood FNT | variable-width bitmap | [CONFIRMED] |
| Menu music | `grinder.wav` | `theme.mix` (loose) | RIFF WAV, IMA-ADPCM, **2ch/22050 Hz/4-bit** | 3264572 B | [CONFIRMED] |

**Theming (`mainmenu.md §2`):** front-end **backgrounds are faction-neutral** (`neutral.mix`). Allied vs Soviet theming lives in the **dialog/in-game chrome**: `sidec01.mix` = Allied, `sidec02.mix` = Soviet (each with its own `optbtn`/`diplobtn`/`uibkgd`/`sidebar`), plus the Allied `aloadlg.pcx` vs Soviet `rloadlg.pcx` loadscreens. [CONFIRMED]

### Button-label list — from `ra2.csf` (our loader supports CSF) (`mainmenu.md §1`)

Real RA2 base top-level `GUI:` keys: **Single Player** (`GUI:SinglePlayer` → `GUI:NewCampaign` → Allied/Soviet/Tutorial, `GUI:Skirmish`, `GUI:LoadSavedGame`), **Network**/LAN (`GUI:Network`), **Internet**/WOL (`GUI:Internet`), **Options** (`GUI:Options`), **Sneak Peeks** (`GUI:SneakPeeks` → Play Movies / View Credits), **Exit Game** (`GUI:ExitGame`). [CONFIRMED — strings present in `D:\Red Alert 2\ra2.csf`]

Live vs greyed for this phase (matches Phase-3 behavior, D5): **Skirmish + Exit/Quit are LIVE**; Campaign, Load, Network, Internet, Options, Sneak Peeks are **greyed stubs**.

### Decoder gaps (the only blockers) (`mainmenu.md §3, §4, §6`)

Loader supports today: **SHP ✅, PAL ✅, CSF ✅, WAV (IMA mono) ✅**. It does NOT support:
- **PCX ❌** — no `pcx.hpp` in `include/ra2assets/`, no PCX code in `src/`. Needed for `logo.pcx`/`title.pcx` (8-bit RLE PCX, palette in trailer). [CONFIRMED gap]
- **FNT ❌** — no `fnt.hpp`, no font code. Needed for authentic bevelled menu text (Westwood variable-width bitmap font: per-glyph width table + palettized glyph rows). [CONFIRMED gap]
- **Stereo-IMA WAV ❌** — `src/wav.cpp:41` hard-rejects `channels != 1` (`"wav: only mono IMA supported"`). The menu track `grinder.wav` is stereo → not playable as-is; needs an interleaved-L/R nibble-decode branch. [CONFIRMED gap]

## Current-engine reality (what we build on)

From the survey — `engine/main.cpp` unless noted. None is a sim invariant this phase alters; these are the seams the reskin + decoders attach to.

- **`MainMenuScene` (`main.cpp:3504`)** — the placeholder to reskin. `render` (`:3535`) draws title `"RED ALERT 2"` via `draw_text` (5×7 rect-font) and six flat SDL rectangles via a local `button()` lambda. `layout()` (`:3517`) computes six `SbRect`s (bw=380, bh=50) at window 1280×720 (`kWinW`/`kWinH`, `:3514`), used identically in `render` and `handle` (deterministic hit-test). Buttons: `skirmish`(live), `campaign`, `mp`, `load`, `options`(greyed), `quit`(live). `handle` is out-of-line (constructs a `SkirmishSetupScene`).
- **`SkirmishSetupScene` (`main.cpp:3574`)** — the setup dialog to reskin with chrome. Renders (`:3702`) via `draw_text` + flat boxes; `global_rect`/`slot_rect`/`start_rect`/`back_rect` (`:3589`–`3601`) define control rects. `Start → GameScene(launch_skirmish(cfg_))`, `Back → MainMenuScene`.
- **`run_menu(bool no_audio)` (`main.cpp:3825`)** — the scene driver; the only path that instantiates the menu scenes. Called at the two non-gate `main()` seams (`:9356`, `:9469`). No gate reaches it.
- **Render helpers** — `blit_shp` (`main.cpp:2642`) palettizes+blits a SHP frame through `TextureCache`; `draw_text` (`:2624`) is the 5×7 rectfont; `SbRect` (with `contains`) is the hit-test primitive; `TextureCache`/`Platform` (SDL) own the GPU textures. A PCX/FNT decoder produces pixels the same path can blit (add a texture-from-pixels helper if `TextureCache` lacks one).
- **ra2assets loader** — `include/ra2assets/`: `shp`, `pal`, `csf`, `mix`, `wav`, `art`, `rules`, `ini`, … (22 headers). **No `pcx.hpp`, no `fnt.hpp`.** `src/wav.cpp` `load_wav_ima` decodes IMA-ADPCM sharing `ima_adpcm.hpp` with the BAG path, but rejects stereo at `:41` and force-sets `out_info.channels = 1` (`:60`).
- **Gate asset dependency** — the 18 `--demo-*` gates load real assets (SHP/PAL/CSF/mono-WAV) through this same loader. The new decoders MUST leave those existing entry points byte-for-byte unchanged; the gates are re-verified after each decoder task to prove it.

## Architecture

Each component states what it does, its interface, its dependencies, and — critically — HOW it stays additive / byte-identical (menu is interactive-only; each decoder is a NEW code path; the existing SHP/PAL/CSF/mono-WAV paths are untouched; `world_hash` and all 18 gate hashes are unperturbed).

### A. Real SHP background + 3-state buttons + palette calibration (D1)

**What:** replace the placeholder `MainMenuScene::render` body. Draw `mnscrnl.shp` (hi-res, frame 0) scaled/letterboxed into the 1280×720 window as the backdrop, palette-calibrated (resolve the dark-render — see Behavior). Draw `mnbttn.shp` per menu item using its 3 state-frames (0 normal / 1 hover / 2 pressed), placed at RE-FALLBACK-approximate real coordinates, hit-tested via `SbRect::contains` and wired to the EXISTING Phase-3 actions (Skirmish → `SkirmishSetupScene`, Quit → `wants_quit`, others greyed). Highest visual win with NO new decoder — SHP/PAL already decode.

**Interface:** unchanged `MainMenuScene` public surface (`handle`/`render`/`next`/`wants_quit`); internally `layout()` returns real-coord button rects; render blits via `blit_shp` + the calibrated menu palette.

**Dependencies:** `ShpFile`/PAL decode, `blit_shp`, `TextureCache`, `SbRect::contains`, the CSF labels, the resolved menu palette.

**Additive/byte-identity:** touches only `MainMenuScene` (an interactive scene reached via `run_menu`, never a gate). Uses the EXISTING SHP/PAL decode paths unchanged. No gate renders the menu → all 18 hashes and `world_hash` unaffected.

### B. PCX decoder + logo/title (D2)

**What:** add a PCX decoder to ra2assets (standard 8-bit RLE PCX; palette in the 769-byte trailer, marker `0x0C`) producing an indexed-or-RGBA surface. Render `logo.pcx` / `title.pcx` on the menu (D5 places them).

**Interface:** new `include/ra2assets/pcx.hpp` + `src/pcx.cpp`, e.g. `PcxImage decode_pcx(const uint8_t* data, size_t len)` returning `{ int w; int h; std::vector<uint8_t> indices; std::array<PalEntry,256> palette; }` (RGBA surface derivable from indices+palette). A texture-from-pixels blit helper feeds the existing render path.

**Dependencies:** `fileio`/MIX extraction to get the PCX bytes; the render path for blitting.

**Additive/byte-identity:** a brand-new decoder file + a new header — no existing ra2assets translation unit changes. Consumed only by the interactive menu. Existing SHP/PAL/CSF/WAV paths untouched → gates byte-identical (re-verify after this task).

### C. FNT decoder + `draw_fnt_text` (D3)

**What:** add a Westwood `.fnt` bitmap-font decoder to ra2assets (variable-width glyphs: per-glyph width table + palettized glyph-row bitmaps). Add a `draw_fnt_text` render helper. Use it for authentic menu labels; the rectfont `draw_text` stays as the HUD font + fallback.

**Interface:** new `include/ra2assets/fnt.hpp` + `src/fnt.cpp`, e.g. `FntFont decode_fnt(const uint8_t* data, size_t len)` exposing glyph count, per-glyph width, and each glyph's bitmap; `draw_fnt_text(ren, font, text, x, y, color…)` in the engine layer measures via the width table and blits glyph bitmaps.

**Dependencies:** MIX extraction for the `.fnt` bytes; the menu palette (glyph indices map through a UI palette); the render path.

**Additive/byte-identity:** new decoder file + header; new `draw_fnt_text` is a NEW engine helper (the existing `draw_text` rectfont is unchanged and still used by the HUD/gates' non-menu rendering). Menu-only consumer → gates byte-identical (re-verify).

### D. Stereo IMA-ADPCM WAV + looping menu music (D4)

**What:** extend `wav.cpp` with a stereo IMA-ADPCM branch (interleaved L/R nibble decode: IMA blocks alternate 4-byte per-channel headers then interleaved nibble data per the block layout) — a NEW branch; the mono path (`channels == 1`) is byte-unchanged. Loop `grinder.wav` (extracted from `theme.mix`) as the menu music via the existing audio path when a menu scene is active; stop it on entering the game (`GameScene`).

**Interface:** `load_wav_ima` gains a `channels == 2` path (mono keeps the exact `:46`–`:57` loop; stereo decodes both channels and sets `out_info.channels = 2`). The menu scene starts/stops the loop through the existing audio playback API; honors the `no_audio` flag already threaded into `MainMenuScene`/`run_menu`.

**Dependencies:** `ima_adpcm.hpp` (shared nibble decode), MIX/loose-file extraction of `grinder.wav`, the engine audio path, `theme.ini` `[INTRO] Sound=Grinder Repeat=yes`.

**Additive/byte-identity:** the stereo branch is guarded by `channels == 2`; the `channels == 1` decode and `out_info.channels = 1` assignment are unchanged, so every existing mono-WAV consumer (including any gate audio path) decodes byte-identically. Menu music playback is interactive-only. Re-verify gates after this task (it edits a shared TU).

### E. Faithful `MainMenuScene` + `SkirmishSetupScene` render (D5)

**What:** compose the faithful menu — SHP background (A) + PCX logo/title (B) + FNT labels (C) + 3-state SHP buttons (A) at RE-FALLBACK real coords + looping menu music (D). Style `SkirmishSetupScene` with the dialog chrome (`bkgdlg.shp` + `sidec01/02` `tabs`/`optbtn`/`button00`) so the setup screen matches RA2's dialog look. Keep ALL existing menu ACTIONS/flow intact (visual reskin only). Button set per `ra2.csf`: Campaign/Load greyed stubs; Skirmish + Quit live; Network/Internet/Options/Sneak-Peeks greyed.

**Interface:** the reskinned `MainMenuScene::render`/`SkirmishSetupScene::render`; unchanged `handle`/`next` action wiring (`Skirmish → SkirmishSetupScene → GameScene`, `Back`, `Quit`).

**Dependencies:** A–D, `blit_shp`, `draw_fnt_text`, the CSF label lookup, the chrome SHPs.

**Additive/byte-identity:** both scenes are interactive-only (via `run_menu`); no gate path is touched. Chrome uses the existing SHP decode. `world_hash`/18 gates unaffected.

### F. Byte-identity re-verify + finalize (D6)

**What:** because B/C/D touch ra2assets and the gates load assets, re-verify all 18 gates byte-identical after the decoder tasks (PROVE the new decoders didn't perturb SHP/PAL/WAV decode). Capture a faithful-menu SCREENSHOT for visual confirmation (the menu isn't headless-gateable — via a manual screenshot hook / temporary capture). arm64 build. README "Faithful Main Menu" note.

**Interface:** the 18-gate sweep (one gate per call — gate-runtime discipline); a screenshot artifact; README section.

**Additive/byte-identity:** verification-only; no sim/gate code changes. NO new determinism gate (the menu is visual/interactive; decoders covered by host tests).

## Data model (new decoder interfaces — none touch `world_hash` or the gates)

| Addition | On | Shape | In `world_hash` / a gate? | Why additive/inert |
|---|---|---|---|---|
| `decode_pcx(...) → PcxImage` | new `pcx.hpp`/`pcx.cpp` | `{ w, h, indices[w*h], palette[256] }` (8-bit RLE PCX, trailer palette) | No | New TU + header; consumed only by the interactive menu; existing decoders unchanged. |
| `decode_fnt(...) → FntFont` + `draw_fnt_text` | new `fnt.hpp`/`fnt.cpp` (+ engine helper) | glyph count, per-glyph width table, per-glyph palettized bitmap | No | New TU + header; rectfont `draw_text` unchanged and still used elsewhere. |
| Stereo IMA branch | `src/wav.cpp` `load_wav_ima` | `channels == 2` interleaved L/R decode; `out_info.channels = 2` | No | Guarded branch; the `channels == 1` mono path + `out_info.channels = 1` are byte-unchanged. |

**No gate renders the menu and no gate uses the new decoders.** The 18 gate hashes and `world_hash` fold sim state loaded through the EXISTING (unchanged) SHP/PAL/CSF/mono-WAV paths. The three additions are new code paths exercised only by host tests + the interactive menu.

## Behavior details (each tied to the recovered §)

- **Background scale/letterbox** (`mainmenu.md §1, §5`): `mnscrnl.shp` frame 0 (632×568) is the full backdrop, drawn centered and uniformly scaled to fit 1280×720 with letterbox bars filled by the palette background color (the RA2 shell itself fills the border with the palette bg). Hi-res `mnscrnl` is preferred; `mnscrns` (472×448) is the lo-res fallback. [ANCHORED]
- **Palette choice — resolve the dark render** (`mainmenu.md §2, §6`): all UI palettes are present but test-renders of `mnscrnl.shp` came out dark/noisy with candidate pals. `fsscrn.pal` (co-located in `neutral.mix` with the background) is the best candidate; `uibkgd.pal` (sidec01/02) is the alternate. Task 1 visually calibrates which `.pal` makes `mnscrnl.shp` look right. This is a calibration step, not a missing asset. [RE-FALLBACK → Task 1]
- **3-state button hover/press** (`mainmenu.md §1`): each menu item is a `mnbttn.shp` (126×25) drawing frame **0 = normal**, **1 = down/pressed**, **2 = hover** (up/down/hover per §1 header). The scene selects the frame from pointer state over the button's `SbRect` (`contains` hit-test) and the mouse-down state. [CONFIRMED frames]
- **Logo/title placement** (`mainmenu.md §1, §5`): `logo.pcx` (RA2 logo) sits above the button column; `title.pcx` is the title graphic. Exact offsets are approximated from the SHP/backdrop layout (button column is a vertical stack in the lower/right region, logo above). [ANCHORED layout; RE-FALLBACK exact coords → Ghidra carry-forward]
- **FNT label rendering** (`mainmenu.md §3`): button labels come from `ra2.csf` and are drawn with `draw_fnt_text` using the shell metallic font (`12metfnt.fnt` / `metal12.fnt`) — the bevelled tech look — replacing rectfont in the menu. rectfont remains the fallback + HUD font. [CONFIRMED font ids]
- **Music loop start/stop** (`mainmenu.md §4`): on menu-scene enter, start `grinder.wav` (theme.mix) looping (`[INTRO] Sound=Grinder Repeat=yes, Normal=no`); stop it on entering the game (`GameScene`). Honors `no_audio`. Requires the stereo-IMA branch (track is 2ch/22050 Hz). [CONFIRMED track + loop]
- **Dialog chrome for setup** (`mainmenu.md §1, §5`): `SkirmishSetupScene` uses `bkgdlg.shp` (856×736) framed panels with `optbtn`/`tabs`/`button00` controls from `sidec01.mix` (Allied) / `sidec02.mix` (Soviet), matching RA2's in-game dialog look. [ANCHORED]

## Scope / non-goals (carry-forwards — document, do NOT implement)

- **Pixel-exact button coordinates** — hard-coded in `gamemd.exe` (not data-driven); OpenRA has no RA2 mod to borrow. This phase approximates coords visually from the SHP sizes + documented layout; a **Ghidra pass on the shell dialog init** to pin exact per-button rects is a carry-forward. (`mainmenu.md §5`)
- **Campaign / single-player, Network / Internet MP, Load / Save, Options persistence, Sneak Peeks / Play Movies / View Credits, Map editor** — shown as **greyed stubs** (Phase-3 behavior); wiring them is out of scope.
- **Loadscreens** (`aloadlg.pcx` Allied / `rloadlg.pcx` Soviet) between menu and game, the **intro movie**, and **animated menu elements** — carry-forward (the background is a static single frame).
- **Yuri faction** — EXCLUDED per user scope (`ra2md.mix`/`thememd.mix`/`langmd.mix` deliberately ignored). Allied vs Soviet only.
- **The 4-phase carry-forwards** — unit abilities, skirmish `allied_full` wiring, naval/aircraft depth, etc. — remain deferred.

## Testing strategy

- **Pure host tests** (doctests, no install required where a synthetic fixture suffices; real-asset assertions where the install is available) for each new decoder:
  - **PCX** (D2): decode `title.pcx` (or a synthetic 8-bit RLE PCX) → assert dimensions + a sampled pixel index/RGBA value against the trailer palette.
  - **FNT** (D3): decode a real `.fnt` (`12metfnt`/`metal12`) → assert glyph count, a couple of per-glyph widths, and a glyph bitmap sample.
  - **Stereo-IMA WAV** (D4): decode a stereo-IMA WAV → assert `channels == 2` and the decoded sample count; assert the mono path still decodes `channels == 1` (behavior-preserving).
- **The 18 gates stay byte-identical** — additive work, re-verified after each ra2assets-touching task: after **T2** (PCX), **T3** (FNT), **T4** (stereo-IMA WAV, edits a shared TU), and the full sweep at **T6**. The gates load assets, so this PROVES the new decoders did not perturb SHP/PAL/CSF/mono-WAV decode. **One gate per call** (each AI gate ≈2 min; batching exceeds the tool timeout — gate-runtime discipline).
- **Faithful-menu SCREENSHOT** — the menu is NOT headless-gateable (interactive/visual, reached only via `run_menu`). Capture a screenshot via a manual hook / temporary capture for visual confirmation of the SHP background + 3-state buttons + PCX logo + FNT labels + chrome.
- **NO new determinism gate** — the menu is visual/interactive (screenshot-verified); the decoders are covered by the host tests above. Adding a gate would contradict the interactive-only, additive constraint.

## [RE-FALLBACK] items for Task 1 to adjudicate

Task 1 is a **confirmatory RE-GATE calibration** (re-probe via `ra2extract` + visual render; flip `[RE-FALLBACK]`/`[ANCHORED]` → adjudicated), NOT a fresh hunt. Close these before the reskin code:

1. **The correct menu PALETTE — resolve the dark render** (`§2, §6`): determine which `.pal` makes `mnscrnl.shp` render faithfully (start `fsscrn.pal` from `neutral.mix`; alternate `uibkgd.pal` from sidec01/02). Test-renders were dark/noisy with candidates — pin the right one. This is a calibration step, not a missing asset.
2. **Approximate button coordinates / layout** (`§5`): fix the vertical button-column placement + logo/title offsets from the backdrop art (lower/right stack, logo above). Exact per-button rects are a **Ghidra carry-forward** — this phase pins a faithful-*looking* approximation.
3. **The exact menu-item list + live vs greyed** (`§1`): confirm the `ra2.csf` `GUI:` top-level set (Single Player → Campaign/Skirmish/Load, Network, Internet, Options, Sneak Peeks, Exit) and which are **live** (Skirmish + Quit) vs **greyed** (the rest) this phase.
4. **Front-end music track + loop** (`§4`): confirm `grinder.wav` (theme.mix) is the front-end theme and that `theme.ini` `[INTRO] Sound=Grinder Repeat=yes, Normal=no` → loop-on-menu, stop-on-game. Confirm it is stereo IMA-ADPCM (2ch/22050 Hz) requiring the D4 branch.
