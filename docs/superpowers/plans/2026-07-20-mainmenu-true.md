# RA2 True Main Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current (mis-designed) main menu with the faithful RA2 shell recovered from `game.exe` — dark-red radar CRT backdrop + full metal sidebar chrome + real 3-state buttons + gold Westwood font + multi-page navigation.

**Architecture:** The shell composes on a **640×480 logical canvas** (scaled to the device window), matching RA2's authored layout. A reusable `draw_shell(...)` helper paints the composition; each menu "page" is a list of labeled buttons routed through the same helper + hit-test. All work is in `engine/main.cpp` (menu region) + `MenuArt`/`load_menu_art`. Purely additive/interactive — no `--demo-*` gate, `simulate_*`, `run_demo_*`, or `world_hash` code is touched.

**Tech Stack:** C++17, SDL2, ra2assets (ShpFile, PcxImage, Palette, Font/`load_fnt`, `draw_fnt_text`), existing TextureCache/Platform/Scene framework.

**Design authority (READ FIRST, use verbatim values):** `docs/re/mainmenu_true.md` — §1 composition table + button pixel rects, §3 page tree, §6 clone guidance. Do NOT re-derive; the assets, palettes, rects, and flow are all pinned there.

## Global Constraints

- All 18 byte-identity gates MUST stay bit-identical: combat=78, tank=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661, multihouse 39412ae996cce8ae, skirmish e9ad3f1fd88495f9, roster 333cd43d8b5ece6a.
- The menu is reached ONLY via `run_menu`/`run_menu_shot` (the non-gate `main()` seams). NO gate renders it. So this is PURELY ADDITIVE — cannot move a gate hash. Do NOT touch `world_hash`, any `run_demo_*`, `simulate_*`, or the `--demo-*` dispatch.
- Do NOT modify any ra2assets decoder (`shp/pcx/fnt/wav/pal/csf/mix`) — they are correct and gate-load-critical. This task only CONSUMES them.
- Scope = Allied vs Soviet RA2 (NO Yuri).
- No assets/GPL committed. Recovered asset ids/rects/palette names ARE committable.
- Commit trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- After each task that changes menu code: build `cmake --build build --target ra2assets_tests ra2engine`, and confirm the whole thing still compiles + `--menu-shot` runs.

---

### Task 1: Load the true-design chrome assets + palettes into MenuArt

**Files:**
- Modify: `engine/main.cpp` — `struct MenuArt` (~3601), `struct MenuArtStore` (~3625), `load_menu_art` (~3683).

**Interfaces:**
- Produces: `MenuArt` gains fields the render task consumes — `ShpFile* bg_s` (mnscrns.shp), `ShpFile* lwscrn` (lwscrns.shp), `ShpFile* sdtp`, `ShpFile* sdbtm`, `ShpFile* sdbtnbkgd`; `Palette shell_pal`, `Palette mainbttn_pal`, `bool shell_pal_ok`. Keep existing `btn` (mnbttn.shp), `logo`, `font`, `font_pal`, `audio`, `bg` (leave existing mnscrnl for now).

**What to do:** In `load_menu_art`, additionally decode from `neutral.mix`: `mnscrns.shp`, `lwscrns.shp`, `sdtp.shp`, `sdbtm.shp`, `sdbtnbkgd.shp` (all via the existing `try_shp` helper). Load `cache.mix/shell.pal` and `local.mix/mainbttn.pal` (via `Palette::load` on the mix bytes, guard with a bool). Store into the new `MenuArt` fields. All best-effort/null-guarded exactly like the existing loads (partial install must not crash). Assets confirmed present: mnscrns.shp(211488), lwscrns.shp(15136), sdtp.shp(66920, 2 frames), sdbtm.shp(10952), sdbtnbkgd.shp(7088), mnbttn.shp(9536, 3 fr), shell.pal(768), mainbttn.pal(768).

- [ ] **Step 1:** Add the new fields to `MenuArt` and (owning storage) `MenuArtStore`, mirroring how `bg`/`btn` are owned.
- [ ] **Step 2:** In `load_menu_art`, decode the 5 new SHPs (neutral.mix) + 2 palettes (cache.mix/shell.pal, local.mix/mainbttn.pal), null-guarded, and wire into `store.art`.
- [ ] **Step 3:** Build `cmake --build build --target ra2engine` — compiles clean.
- [ ] **Step 4:** Run `build/ra2engine.exe --menu-shot <scratch>/t1.png` (PowerShell) — still renders (old composition; new fields just loaded), no crash.
- [ ] **Step 5:** Commit `feat(menu): load true-design shell chrome assets + shell/mainbttn palettes`.

---

### Task 2: `draw_shell` helper + rewrite MainMenuScene to the true composition

**Files:**
- Modify: `engine/main.cpp` — add a `draw_shell(...)` free helper near `draw_fnt_text` (~2750); rewrite `MainMenuScene::render` (~3848), `MainMenuScene::layout()` (~3811), and `MainMenuScene::handle` (~4232) to the true 6-button design.

**Interfaces:**
- Consumes: `MenuArt` fields from Task 1.
- Produces: `draw_shell(plat, tc, art, canvas_scale, buttons, hover_ix, pressed_ix, header, version)` that paints one shell page on the 640×480 logical canvas scaled into the window; and a `struct ShellButton { SbRect rect; std::string label; bool enabled; int action; }`. `layout()` returns the 6 button rects in LOGICAL 640×480 space; render + hit-test both derive device rects from the SAME logical rects × scale (click rects == draw rects — a hard requirement, see the render-polish precedent).

**Composition (from mainmenu_true.md §1, 640×480 logical, draw order):**
1. Backdrop: `bg_s` (mnscrns.shp) under `shell_pal` at (0,0,472,448). Scale the 472×448 SHP to fill that logical region.
2. Bottom bar: `lwscrn` (lwscrns.shp) under shell_pal at (0,448,472,32).
3. Sidebar top: `sdtp` (sdtp.shp frame 0) under shell_pal at (472,0,168,199).
4. Sidebar bottom: `sdbtm` under shell_pal at (472,415,168,65).
5. Per-button metal slice: `sdbtnbkgd` (168×42) under shell_pal at x=472, y=btnY−2 for each button.
6. Buttons: `btn` (mnbttn.shp, frame 0=up / 1=hover / 2=down) under `mainbttn_pal`, stretched to 162×37 at the six rects.
7. Logo emblem: crop the RA2 logo from `logo` (logo.pcx) and composite over the CRT screen area (upper-left of the 472×448 region) as the static stand-in for the missing VQA. Sky-key/darken acceptable; keep it tasteful over the red radar map.
8. Gold labels via `draw_fnt_text` (12metfnt) centered in each button rect; "Main Menu" header at (477,2,162,16); version string bottom-right (477,461).

**Button rects (LOGICAL 640×480, from §1) + labels:**
| Label | rect (x,y,w,h) | action |
|---|---|---|
| Single Player | (476,198,162,37) | PAGE_SINGLE |
| Internet | (476,242,162,37) | (greyed stub) |
| Network | (476,286,162,37) | (greyed stub) |
| Movies & Credits | (476,330,162,37) | PAGE_MOVIES |
| Options | (477,374,162,37) | PAGE_OPTIONS |
| Exit Game | (477,418,162,37) | ACT_QUIT |

The scale = min(winW/640, winH/480); letterbox-center the 640×480 canvas (fill bars with black or the shell metal tone). Track `hover_ix` from mouse-move (add pointer-move handling if the InputEvent carries it; else hover on the button under the last cursor) and `pressed_ix` on mouse-down.

- [ ] **Step 1:** Add `ShellButton` + `draw_shell(...)`; implement the 8-layer composition above using the existing blit helpers (`blit_shp`, texture-from-pixels for PCX). Remove the old olive-LUT/right-align path from MainMenuScene.
- [ ] **Step 2:** Rewrite `MainMenuScene::layout()` to return the 6 logical rects above; `render` calls `draw_shell` with the 6 buttons + Single Player as default hover; `handle` hit-tests the same logical rects (scaled) and routes Skirmish-era actions (Single Player → Task 3 page; Exit → quit).
- [ ] **Step 3:** Build `ra2engine`; run `--menu-shot <scratch>/t2.png`.
- [ ] **Step 4:** VISUAL CHECK (controller eyeballs t2.png): dark-red radar backdrop, metal sidebar with SDTP monitor + SDBTM base, six red buttons with gold labels at the true rects, "Main Menu" header, version line. Compare to `scratchpad/menu_TRUEDESIGN.png`.
- [ ] **Step 5:** Commit `feat(menu): faithful RA2 shell composition (true backdrop+chrome+buttons+labels)`.

---

### Task 3: Multi-page navigation (Single Player / Movies & Credits / Options), Skirmish live

**Files:**
- Modify: `engine/main.cpp` — generalize the shell scene into a page-driven `ShellScene` (or add sub-scenes) using `draw_shell`; wire the page tree + Back nav; keep `SkirmishSetupScene`/`GameScene` as the Skirmish target.

**Interfaces:**
- Consumes: `draw_shell` + `ShellButton` from Task 2.
- Produces: page transitions per mainmenu_true.md §3.

**Page tree (from §3):**
- **Main (0xE2):** Single Player→SP page; Internet/Network→greyed; Movies & Credits→Movies page; Options→Options page; Exit Game→quit.
- **Single Player (0x100):** New Campaign→Campaign page (greyed stub OK), Skirmish→`SkirmishSetupScene` (LIVE), Load Saved Game→greyed, Back→Main.
- **Movies & Credits (0x101):** Sneak Peeks/Play Movies/View Credits→greyed stubs, Back→Main.
- **Options (0xD5):** faithful stub page (sliders not required this task; show the labeled buttons greyed) + Back→Main.

Each page reuses `draw_shell` with its own button list; the header text changes ("Main Menu" / "Single Player" / "Movies & Credits" / "Options"). Back button always present (bottom row). Only **Skirmish** launches the game (existing `SkirmishSetupScene`→`GameScene`). Greyed buttons render via mnbttn frame 0 dimmed + non-interactive.

- [ ] **Step 1:** Introduce a `ShellScene` that holds a page id + its `ShellButton` list, renders via `draw_shell`, and on click either transitions to another `ShellScene` page, launches `SkirmishSetupScene`, or quits. Preserve the existing `run_scene_driver` flow.
- [ ] **Step 2:** Define the page button lists per §3; wire MainMenuScene as the `PAGE_MAIN` ShellScene (or keep MainMenuScene as the entry that constructs PAGE_MAIN).
- [ ] **Step 3:** Build; run `--menu-shot` for each page if the shot hook supports a page arg (else just main). Manually verify nav compiles + Skirmish still reaches GameScene.
- [ ] **Step 4:** VISUAL CHECK: navigate Main→Single Player→(Skirmish launches) and Back paths are correct (controller eyeballs shots / reasons through handle()).
- [ ] **Step 5:** Commit `feat(menu): multi-page shell navigation (SinglePlayer/Movies/Options), Skirmish live`.

---

### Task 4: Finalize — gate re-verify, screenshots, arm64, README, ledger

**Files:**
- Modify: `README.md` (menu section), `.superpowers/sdd/progress.md` (ledger).

- [ ] **Step 1:** Build `cmake --build build --target ra2assets_tests ra2engine`; run the test suite (scoped) — all pass, no menu-related regressions.
- [ ] **Step 2:** Re-verify all 18 byte-identity gates EXACT, ONE per foreground call (per gate-runtime discipline — never batch; ~2min/3GB each). Confirm each hash matches the Global Constraints values.
- [ ] **Step 3:** arm64 `cd android; ./gradlew.bat assembleDebug` — BUILD SUCCESSFUL; nm-verify the menu symbols still link.
- [ ] **Step 4:** Capture final `--menu-shot` screenshots (main + a sub-page); controller eyeballs = faithful RA2 shell.
- [ ] **Step 5:** Update README "Faithful Main Menu" section (true design: mnscrns+shell.pal backdrop, full sidebar chrome, mainbttn.pal buttons at recovered rects, multi-page tree, VQA-absent static stand-in). Update the ledger with the per-task record + phase-complete block. Commit `docs(menu): true-design main menu finalize + README + ledger`.

## Self-Review notes
- Spec coverage: T1=assets, T2=composition+buttons, T3=pages, T4=verify/doc. Full design in mainmenu_true.md.
- The single hardest correctness point (repeated from the render-polish precedent): **render rects and hit-test rects must come from ONE `layout()` in logical space** — never compute them twice.
- Byte-identity is sound by construction (menu is additive/interactive) but T4 still re-verifies all 18 gates because the diff touches main.cpp.
