# Wave-2 gap 04: UI and shell visual/UX fidelity

This is a read-only audit of master `dfff02a`, done on 2026-10-07 in worktree `D:\ra2-wt\w2-ui` (branch `w2-ui`, own `build/`). No engine source was changed and nothing was committed.

**Scope.** It covers:
- the in-game HUD (sidebar, cameos, power, credits, radar, buttons, tooltips, selection, health bars, messages, the in-game options flow, the result screens);
- every shell page except the Skirmish lobby, which `docs/gap/07` covers.

**Original.** The original is RA2 `game.exe` 1.006 (not YR). Earlier docs pinned the sidebar geometry from `gamemd.exe`. The RA2 strip-draw code read below matches those docs.

## Evidence used

| Source | Where |
|---|---|
| Clone screenshots taken with the `--play` harness on this tree | `D:\ra2-wt\w2-ui\play-w2\out-ui\*.png` (script `play-w2\ui-tour.play`, 26 shots: shell pages, lobby → game, cameo states, F10 menu, save dialog, abort, victory, result) and `play-w2\out-camp\*.png` (`camp-tour.play`: loading screen, objectives, F1 briefing, mission result). Both scripts ran to the end. The only `PLAY FAIL` is L19, a button label: the Single Player page has no "Main Menu" button, which is itself gap W2-UI-14. |
| Original art rendered to PNG with `build\ra2extract.exe` | `play-w2\pal_compare.png` (sidebar chrome under `sidec01.mix/sidebar.pal` on the left vs `cache.mix/cameo.pal` on the right, as the clone draws it). `play-w2\tab_frames.png` (`tab00.shp` frames 0–4). `play-w2\pudlgbgs.png` (popup dialog background, 451×326). Plus the scratchpad `art\` folder. |
| Dialog templates from `game.exe` `.rsrc` | `D:\ra2-re\out\opt_all_dialogs.txt` (MapDialogRect 6×13 base units, so px = x·6/4, y·13/8). The 640×480 pixel rects are quoted below. |
| CSF strings | `D:\Red Alert 2\ra2.csf`, parsed with `play-w2\csf.py` |
| Ghidra on `game.exe` | Private project `D:\ra2-re\w2-ui` (a copy of `p4proj`). Scripts `D:\ra2-re\scripts\w2ui_1..3.py`. Outputs in `D:\ra2-re\w2-uiout\` (`w2ui_1.log`, `w2ui_2.log`, `w2ui_str_TXT_HOLD_0067f800.c`, `w2ui_str_TXT_MONEY_FORMAT_1_0067f580.c`, `w2ui3_4dd8a0.c`, `w2ui3_4ddf90.c`). |

## New RE findings (game.exe)

These findings are **[CONFIRMED]** unless they are tagged otherwise.

1. **The sidebar palette is `sidec0N.mix/sidebar.pal`, not `cache.mix/cameo.pal`.**
   - `cache.mix` has no `sidebar.pal`. `sidec01.mix` and `sidec02.mix` each carry their own, and the two differ.
   - `SIDEBAR.PAL` is a string in `game.exe` (`0x7F8AF8`, in the shell asset-name registry).
   - `sidebar.pal` and `cameo.pal` differ in 253 of 256 entries.
   - The rendered result is unambiguous. Under `sidebar.pal` the chrome is blue/steel with cyan lamps and a blue eagle. Under `cameo.pal` it is a pink and white speckled noise (`pal_compare.png`).
   - Cameos still use `cameo.pal`.
2. **StripClass cameo draw, `FUN_0067F800`.** All offsets are relative to the 60×48 cell at (x, y).
   - The cameo SHP is drawn at (x, y) with flags 0x400.
   - When the item cannot be built, `DARKEN.SHP` is drawn at (x, y) with flags 0x401.
   - A "new item" flash draws `DARKEN` with flags 0x404 when `(frame & 15) > 8`.
   - While an item is in production, `GCLOCK2.SHP` frame `stage+1` is drawn with flags **0x404 (translucent)**. There is no separate shade fill under the clock.
   - The quantity is drawn with `"%d"` at **(x+60, y+1)**, flags 0x242 (right-aligned). It is shown only when the count is > 1, or when the count is 1 and the item is not the current product.
   - The status text is CSF **`TXT_READY` "Ready" / `TXT_HOLD` "On Hold"**, picked by `FUN_0069D770`. It is drawn at **(x+30, y+1)** with flags 0x142 (centred). When a count is also shown, it moves to **(x+2, y+1)** with flags 0x42 (left-aligned).
   - Text colour comes from the UI colour pair at `0xAC1028`. The font is selected by `FUN_005FEB60(0, 0xAF)`; the face is not traced.
3. **Cameo tooltip text, `FUN_0067F580` (StripClass help text).**
   - It is gated on `[Options] ToolTips`.
   - A superweapon cameo shows the SW's UIName.
   - Other cameos use `swprintf(TXT_MONEY_FORMAT_2 "%s $%d", UIName, Cost_Of(house))`, or `TXT_MONEY_FORMAT_1 "$%d"` in the alternate mode (global `0x8373C0`). [ANCHORED: which branch uses which key is inferred from the line numbers.]
   - **Every space is then replaced with a newline.** So the tooltip reads "Power⏎Plant⏎$800".
4. **The in-game options dialog depends on the game mode, `FUN_004DD7D0`.**
   - `Session.GameMode` is at `0xA3D298`.
   - Mode 0 (campaign) and mode 5 (skirmish) get **0xB5**: Game Controls, Load, Save, Delete, Abort, Resume.
   - Every other mode gets **0xBBA**: Game Controls, Abort, Resume.
   - So the clone's skirmish Save/Load is faithful.
   - Proc `0x4DD8A0`:
     - 0x521 Game Controls opens 0xBBB.
     - 0x522 Abort leads to the confirm page 0xB6.
     - 0x51E / 0x51F / 0x520 open the Load / Save / Delete pages (0xB7 / 0x2B4 / 0x2B5, built in `FUN_0053B770`).
     - The GameSpeed slider maps `GameSpeed = 6 − pos`.
5. **The Abort flow is page 0xB6, proc `0x4DDF90`.**
   - The page text is `GUI:AskAbortMission` "What would you like to do?".
   - Button **0x6C9** returns 2, i.e. Abort. Its label is fetched at runtime [PROBABLE `GUI:Abort` "Abort"].
   - Button **0x712**:
     - Campaign (mode 0): label `GUI:Restart` "Restart", code 5 (restart the mission). The `GUI:Restart` string is referenced at `0x4DE037`, inside this proc.
     - Skirmish (mode 5): the button is **hidden**.
     - MP with more than one live player: a third label, code 4 (surrender/observe; `STT:ConfirmExitButtonObserve`).
   - 0x686 "Resume Mission" returns 1.
6. **Exit Game confirm is popup 0x120, `FUN_005B46B0`.**
   - The popup is 300×200 DLU, i.e. 450×325 px. The background `local.mix/pudlgbgs.shp` / `pudlgbgn.shp` is exactly **451×326** under `local.mix/dialog.pal`.
   - Text static 0x5B0 is at (60, 65, 330, 81).
   - OK is 0x5AE at (310, 251, 124, 24). Cancel is 0x002 at (310, 284, 124, 24).
   - Buttons are drawn with class 3 (`MNBTTN.SHP` under `MAINBTTN.PAL`, per `mainmenu_true.md` §7.2).
7. **Dialog 0x73 is the Diplomacy screen**, not a score screen. It is opened by `FUN_00634A00` from the sidebar Diplomacy button. Its columns are Player / Allies / Chat / Kills, plus a "Taunts On" check, and it has a Return button.
8. **`credits.txt` is present** in `ra2.mix/local.mix` (15,350 B), so View Credits can be implemented. Play Movies and Sneak Peeks have no content in this install: `MOVIES0x.MIX` are 4-byte stubs, and the only BIKs in `language.mix` are `RA2TS_S` and `RA2TS_L`.
9. **The original uses RA2 key bindings.** RA2 `game.exe` reads `Keyboard.ini`, and that file differs from `KeyboardMD.ini`, which is what the clone loads:
   - `SidebarUp/Down` = 2086 / 2088, plus `SidebarPageUp/Down` 2085 / 2087 (codes ≥ 2048, i.e. mouse-wheel class).
   - `LeftSidebarUp/Down`, `RightSidebarUp/Down` exist.
   - `HealthNav=85`.
   - `ScreenCapture=579`.

## Gap table

Status values: WRONG / MISSING / PARTIAL / WORKS. Severity values: MAJOR / MINOR (none of these block play). Effort: S < 1 day, M 1–3 days, L > 3 days. All clone `file:line` references are on master `dfff02a`. Screenshots are in `play-w2\out-ui\` (`out-camp\` where noted).

| ID | Element | Original (cite) | Clone (file:line, screenshot) | Status | Sev | Fix sketch | Effort | Depends on |
|---|---|---|---|---|---|---|---|---|
| W2-UI-01 | **Sidebar chrome palette** | Every `sidec0N.mix` chrome SHP (credits, top, radar, side1/2/3, addon, tabs, repair/sell, r-up/r-dn, powerp, gclock2, optbtn/diplobtn) draws under that mix's own `sidebar.pal`: blue/steel chrome with cyan lamps (RE #1, `pal_compare.png` left) | `ba.sb_pal = ba.cameo_pal`, with the override looking only in `cache.mix`, so it never fires (`main.cpp:1980-1986`). The whole sidebar renders in the `cameo.pal` pink/white noise (`g03_building_clock.png`, `sb_strip.png`). | WRONG | **MAJOR** (the most visible HUD defect) | Load `sidebar.pal` from `g_sidebar_mix` (the same `MixFile sb`, `main.cpp:2158`). Keep `cameo_pal` for cameos only. | S | — |
| W2-UI-02 | Build clock | `GCLOCK2` frame `stage+1`, **translucent** (0x404), with no shade under it (RE #2) | 90-alpha black shade, then gclock2 opaque, under the wrong palette, so it reads as a solid grey wipe (`main.cpp:4004-4010`, `g05_queue_count.png`) | WRONG | MAJOR | Drop the `shade_rect`. Blit the frame with 50% alpha (or the darken blend) under `sidebar.pal`. | S | 01 |
| W2-UI-03 | "Ready" / "On Hold" text | CSF `TXT_READY` "Ready" / `TXT_HOLD` "On Hold" centred at (x+30, y+1). With a count shown, left-aligned at (x+2, y+1). UI text colour (RE #2). | READY = a 2 Hz green tint with no text (`main.cpp:4000-4003`, `g04_ready.png`). Hold = the rect-font string "HOLD" at (x+18, y+2) (`game_scene_controls.inc:349-366`, `g06_on_hold.png`). | WRONG | MAJOR | Draw the CSF text with the game font at the recovered anchors and flags. Remove the green flash. | S | 12 |
| W2-UI-04 | Queue count | `"%d"` right-aligned at (x+60, y+1), flags 0x242, game font. Shown when count > 1, or when the type is queued but not current (RE #2). | 5×7 rect font, white, at (x+60−w−1, y+2) (`game_scene_controls.inc:357-361`) | PARTIAL | MINOR | Game font, exact anchor, and the "queued but not current" case. | S | 12 |
| W2-UI-05 | Unavailable / new-item cameo | `DARKEN.SHP` overlay (0x401) when unbuildable. A flashing `DARKEN` (0x404, `(frame&15)>8`) while a newly added cameo is "fresh" (RE #2). | `shade_rect` alpha 140 (`main.cpp:4011-4012`). No new-item flash. | PARTIAL | MINOR | Load `DARKEN.SHP` (the conquer/sidec mix; the asset ref is in `FUN_0067B890`) and draw it with its blend. Add a per-cell flash timer set on `EVA_NewConstructionOptions`. | S | 01 |
| W2-UI-06 | Tab buttons | `tab0N.shp` has 5 frames (`tab_frames.png`): 0 = normal icon, 1 = lit, 2 = blank/empty, 3 = dark/pressed, 4 = alt. Tabs with nothing buildable show the blank frame. A tab flashes when a new cameo arrives in it. [Frame→state map: RE-OPEN Q2.] | Frame 0 for every tab, with a black alpha-110 shade over the inactive ones (`main.cpp:3897-3902`, all screenshots) | WRONG | MINOR | Per-state frame from TabClass. A blank frame for empty strips. Tab flash on new cameo. | S | Q2 |
| W2-UI-07 | **Cameo tooltip** | With `ToolTips=yes`, hovering a cameo shows "UIName $Cost" with every space turned into a newline ("Power⏎Plant⏎$800"); a SW cameo shows its UIName (RE #3). Sidebar buttons show their tooltips too (Q3). | None. No tooltip anywhere in `GameScene` (`grep -i tooltip engine/*.inc` finds only the skirmish page). `g07_cameo_hover.png`, `g09_repair_hover.png`. G-11 is still open. | MISSING | **MAJOR** | Hover timer over `lay_.cameos`, then `swprintf(TXT_MONEY_FORMAT_2)` with space→newline, drawn as a boxed text near the cursor in the game font (box art/colour/delay: Q3). Honour `g_user_settings->tooltips`. | S | 12, Q3 |
| W2-UI-08 | Soviet sidebar geometry | sidec02 constants: repair x33 / sell x85 at y165; tabs 20 + 32·i; pips x0; r-up at 84; side3 height 18 (`construction.md` §11a) | `g_sidebar_mix` switches the art only (`main.cpp:4745-4748`). `SidebarLayout::compute` always uses the Allied constants (`sidebar.cpp:7-66`, `sidebar.hpp:21-55`). | PARTIAL | MINOR | A `Style` parameter on `compute` with the two constant sets. | S | 01 |
| W2-UI-09 | Radar boot / minimap rect | Boot animation = `radar.shp` frames 1–31 at a **4-tick** frame timer (≈ 8 s), reversed on power-down. Frame 32 is the open backdrop. Minimap viewport **140×108 at bar (16, 49)** (`construction.md` §11a). | One boot frame per game frame (≈ 2 s; `pres_radar.inc:54-59`, "boot rate not read"). No power-down animation. Minimap drawn into (x+1, 48, 153, 110) (`main.cpp:3870-3877`). | PARTIAL | MINOR | `f = 1 + ticks/4`. Play 31→1 on power-down. Use the 140×108 @ (16, 49) rect for both draw and hit-test (`radar_cell_at`). | S | — |
| W2-UI-10 | Credits counter | Tally `"%d"` centred at (84, 2), style 0x4108, UI colour (`construction.md` §11a). Roll rate and tick sound not traced (Q4). | `12metfnt` pale-yellow ramp at (84, 1) ("[RE-FALLBACK] HUD text colour not read", `pres_game.inc:58-66`). Roll = max(500/s, Δ/0.5 s) (`main.cpp:5154-5161`). | PARTIAL | MINOR | Pin the colour and roll once Q4 is answered. Position is fine. | S | Q4 |
| W2-UI-11 | Health bar / selection | Units and buildings use `PIPBRD.SHP` (2 frames) + `PIPS.SHP` (19 frames) / `PIPS2.SHP`, loaded in `FUN_005D5E00`. Building bars are pip rows along the foundation edge. Colours follow `ConditionYellow`/`ConditionRed`. | Units: a flat green diamond + 2 ticks + a solid rect bar (`iso_renderer.cpp:382, 473`; `g02_mcv_selected.png`, `sel_zoom.png`). Buildings: white corner ticks + a horizontal rect bar (`game_scene_controls.inc:14-52`, "[RE-FALLBACK] … pips.shp … agent E's art"; `g08_building_selected.png`). | WRONG | MAJOR (it is shown on every selection) | Draw the pip border + per-pip frames. Pip count = ceil(hp ratio × N); width per type class (Q1). | M | Q1 |
| W2-UI-12 | HUD / sidebar font | Game text font `GAME.FNT` (loaded by `FUN_00432290`) or the `0xAF` face chosen by `FUN_005FEB60` (Q5) | `12metfnt` for credits and banners (`pres_game.inc:58-66`). The 5×7 rect font for HOLD/count, objectives, briefing, timer, end banners and every in-game dialog (`main.cpp:5836-5874`, `gf_scene.inc:191-199`). | WRONG | MAJOR (with 13–16) | One `HudFont` (decoded `game.fnt` + UI colour) used by every in-game text path. Retire `draw_text` in-game. | S | Q5 |
| W2-UI-13 | **In-game options menu** | **0xB5 is a full-screen shell page** (426×295 DLU skeleton). Header `GUI:GameOptions` "Game Options". Right-column buttons on the 156×42 grid (`mainmenu_true.md` §7.2): row 0 Game Controls (0x521), 1 Load Game (0x51E), 2 Save Game (0x51F), 3 Delete Game (0x520), 4 Abort Mission (0x522), 5 Resume Mission (0x686). Status line 0x695 shows `STT:IGSingleOpt*`. MP uses 0xBBA (RE #4). | A 340×380 dark popup over the game, rect font "OPTIONS", 5 boxes, **no Game Controls** (`main.cpp:4661-4720`, `g11_ingame_menu.png`) | WRONG | **MAJOR** | Reuse `draw_shell` (static MNSCRNS screen, no 0x71C monitor on 0xB5) and the `ShellButton` grid. Keep the `menu:*` probe aliases. | M | 14 |
| W2-UI-14 | Game Controls page 0xBBB and Sound page 0xB8 | **0xBBB**: Game Speed slider 0x529 (130, 141, 192, 21), label "Game Speed" right-aligned at (6, 139, 117, 24), "Faster" at (331, 139); Scroll Rate 0x52A (130, 191, 192, 21); Visual Details (hidden, style 0x48…); checkboxes Target Lines 0x601 (48, 313), Tooltips 0x602 (235, 313), See Hidden 0x604 (48, 342); buttons Sound (row 0, 0x52D), Keyboard (row 1, 0x52C), Back (row 5). Speed = 6 − pos (RE #4). **0xB8**: Music / Sound / Voice sliders at (157, 45 / 81 / 117, 262, 21), score listbox (157, 157, 262, 160), Shuffle (43, 251), Play (43, 347, 124, 24), Stop (295, 347, 124, 24), Back. | Absent. Game speed can only be set in the lobby. Volumes can only be set on the main-menu Options page. (G-13 / G-14 "Controls" remain open.) | MISSING | MAJOR | Two `ShellScene`-style overlay pages reusing `OptionsPageState` widgets (`main.cpp:6534-6631`). Speed applies live to the G-13 accumulator. Jukebox over the G-82 theme player. | M | 13 |
| W2-UI-15 | **Abort flow** | Page **0xB6** (full shell page): text "What would you like to do?" at (66, 222, 345, 30). Row 0 = 0x6C9 Abort (code 2), row 1 = 0x712 **Restart** (campaign, code 5; hidden in skirmish), row 5 Resume Mission (RE #5). | `GfConfirm` popup "WHAT WOULD YOU LIKE TO DO" (no '?'), buttons ABORT / RESUME MISSION; **no Restart in campaign** (`gf_start.inc:282-370`, `g13_abort_confirm.png`) | PARTIAL | MAJOR | Shell page with the mode-dependent buttons. Restart = `gf_make_restart_scene` (already exists). | S | 13 |
| W2-UI-16 | Load / Save / Delete pages | Shell pages **0xB7** "Load Mission", **0x2B4** "Save Mission", **0x2B5** "Delete Mission". Label "Select Mission" / "Save Mission" / "Delete Mission" at (36, 26, 399, 39). Listbox at (36, 81, 399, 303); on Save it is (36, 81, 399, 255) + edit (37, 362, 399, 22). Action button at row 0 (`GUI:Load` / `GUI:Save` / `GUI:Delete`), Back at row 5. Confirms are 0x120-style popups. Saved banner = `TXT_GAME_WAS_SAVED` "Mission Saved". | `SaveLoadDialog` = a 660×540 popup with rect font, "LOAD SAVED GAME", "<NEW SAVED GAME>", "GAME SAVED" banner (`main.cpp:4389-4657, 5485`; `s05_load.png`, `g12_save_dialog.png`). Reached from both the main menu and in-game. | WRONG | MAJOR | Re-skin as a shell page: listbox skin `0x5F4BA0`-style red lines + yellow text (`options.md` §4), 12metfnt. Keep the logic and probe names. Use the CSF banner. | M | 13 |
| W2-UI-17 | Popup confirm dialogs (Exit, overwrite, delete, validation) | 0x120 / 0x121: `pudlgbg?.shp` 451×326 under `dialog.pal`, text static (60, 65, 330, 81), MNBTTN buttons 124×24 at (310, 251) / (310, 284) (RE #6). Mixed-case CSF text: "Are you sure you want to exit Red Alert 2?", "Ok", "Cancel". | Flat dark rect 600×400, uppercase 5×7 text with the "?" lost, flat red boxes (`gf_start.inc:302-370`, `s08_exit_confirm.png`) | WRONG | MINOR | `PopupDialog` widget: centred 451×326 bg, MNBTTN states 0 / 2 / 1 (normal / hover / pressed), 12metfnt text. | S | — |
| W2-UI-18 | **Campaign select page 0x94** | Header `GUI:CampaignMenu` "Campaign". Load button row 0. Campaign **listbox** 0x455 in the sidebar column (477, 247, 160, 159). **Difficulty slider** 0x50F (135, 406, 210, 21), labels "Difficulty" (135, 380) / "Harder" (232, 380). Three animated emblem wells (24, 16, 426, 115) / (61, 138, 348, 91) / (78, 235, 318, 115) with ASCR* / SSCR* art (`mainmenu_true.md` §8.2; tooltips `STT:CampaignAnimAllied/Soviet/Tutorial`). Back at y 411. Monitor 0x71C. | "New Campaign" header and two buttons Allied / Soviet Campaign over the main-menu CRT movie (`main.cpp:6750-6755`, `s04_campaign.png`). Difficulty is picked on the loading screen instead (`campaign_scenes.inc:64-185`). | WRONG | MAJOR | New `CampaignPage` scene: click an emblem well → highlight + Load; slider seeded from `[Options] Difficulty` (closes G-78); listbox Allied / Soviet / Boot Camp. | M | — |
| W2-UI-19 | Campaign loading screen | LS800 art full-screen with the briefing in the art's text area and a progress bar; mission starts automatically (`campaign.md` §3) | Art at (240, 60) + a dark panel + rect-font text + EASY / NORMAL / HARD + START / BACK buttons (`campaign_scenes.inc:103-128`, `out-camp\c01_loading.png`) | PARTIAL | MINOR | After W2-UI-18 moves the difficulty to 0x94: full-screen art, game-font text, a progress bar, auto start. | S | 18, 12 |
| W2-UI-20 | Result / score screens | Skirmish: `TXT_VICTORIOUS` "You Are Victorious!" / `TXT_LOST` "Game Over" during BorrowedTime, then the MP score dialog **0xBC6** (columns Player (16, 173), Kills (118, 173), Losses (181, 173), Built (256, 173), Score (331, 173); Continue at (475, 180, 163, 37)). Campaign: **0xBC7** "Campaign Results" (graph well (24, 136, 423, 229), First / Second Place, Continue at (477, 211)). Which one is used and when: Q6. | Black screen with 5×7 uppercase text and REMATCH / MAIN MENU or CONTINUE / RETRY / MAIN MENU rect buttons (`gf_result.inc`, `campaign_scenes.inc:203-290`; `g15_result.png`, `out-camp\c05_result.png`). The end banner is rect font, upper case (`gf_scene.inc:191-199`, `g14_victory_text.png`). | WRONG | MAJOR | Shell-page result scenes on the templates with the World tallies (already collected in `GfSkirmishStats`). Banner in the game font, mixed case. | M | 12, Q6 |
| W2-UI-21 | Message list / objectives / timer | Messages (TXT lines, objectives, `TXT_LOW_POWER` "Low Power - Production Slowed", "X has been defeated") in the top-left message list, game font, with timeouts (`MessageDelay`). Briefing via the 0xB5 path [ANCHORED]. | Objectives = green rect font on black bars (`main.cpp:5836-5847`). "TIMER mm:ss" rect font. "F1 BRIEFING" hint drawn permanently. Low-power text never posted. No chat line. (`out-camp\c02`, `c03`.) | PARTIAL | MINOR | A `MessageList` (N lines, per-line expiry, house colour) in the game font; feed it TXT_LOW_POWER, defeated players, objectives. Drop the hint. Chat is out of scope (single player). | S | 12 |
| W2-UI-22 | Shell page labels | 0x100 bottom button `GUI:MainMenu` "Main Menu" (STT "Return to the Main Menu.") and 0x101 bottom "Main Menu" (template). Campaign header "Campaign". Version 0x71D runtime "%s %s" from `GUI:Version` "V6.8 / " + build (Q7). | "Back" on Single Player and Movies (`main.cpp:6732, 6739`), "New Campaign" header (`:6709`), "v1.006" (`:6941`). PLAY FAIL L19 shows it: no `button:Main Menu` on Single Player. | WRONG | MINOR | Pull every label and header from the template's CSF key (a `csf("GUI:…")` table). Keep `button:Back` as a probe alias. | S | — |
| W2-UI-23 | Shell tooltips + click sound | Status line 0x695 (15, 458, 454, 19) shows `STT:MainButton*`, `STT:SingleButton*`, `STT:OptionsButton*`, `STT:MainOpt*`, `STT:Campaign*`, `STT:LoadButton*` (`FUN_005E22B0`). `MenuClick` on every button. | Only the Skirmish page has either (`skirmish_page.inc:505, 539-622, 823-825`). Main / Single / Movies / Options / Campaign pages are silent with an empty status line (`s01`–`s07`). | MISSING | MINOR | Move `tooltip_at` + `play_ui` into `ShellScene` with a (page, control) → STT table. | S | — |
| W2-UI-24 | Page transition (SK-33) | `FUN_005E5150`: rows power on top to bottom (SDBTNANM frames 5..10 on enter, 11..16 on leave, 1-row stagger) + SDWRNTMP monitor, 30 ms/frame (`mainmenu_true.md` §7.2) | Instant swap (`main.cpp:7095`) | MISSING | MINOR | Transition state in `ShellScene` (enter/leave counters) drawn by `draw_shell`. `FUN_005E5150` is decompiled in `w2-uiout\w2ui3_5e5150.c`. | M | — |
| W2-UI-25 | Header shimmer / default hover | The "Main Menu" header is an animated type-1 control (30 ms, 8 phases, `mainmenu_true.md` §7.3). Whether the first button is lit with no hover is not traced (Q7). | Static header. The first enabled button is always drawn hovered (`main.cpp:6926-6931`, `s01_main.png`). | PARTIAL | MINOR | Shimmer ramp. Drop the default hover unless Q7 confirms keyboard focus. | S | Q7 |
| W2-UI-26 | Movies & Credits | View Credits plays `local.mix/credits.txt` (present, RE #8) as a scroller [ANCHORED: CREDITS theme]. Play Movies and Sneak Peeks have no content on this install. | All three greyed (`main.cpp:6734-6740`, `s06_movies.png`) | MISSING | MINOR | A `CreditsScene` (parse the credits.txt markup, scroll in 12metfnt over MNSCRNS, CREDITS theme). Keep movies greyed. | M | — |
| W2-UI-27 | Options Keyboard page 0xA3 | Category combo (12, 81, 207), commands listbox (231, 79, 219, 178), description (12, 105, 207, 154), hotkey edit (13, 344, 186, 22), Assign (231, 344, 124, 24), Reset All, Back (`options.md` §6). Writes `Keyboard.ini`. | Greyed button (`main.cpp:6746`). The live bindings come from **`KeyboardMD.ini`** (YR), not RA2 `Keyboard.ini` (`game_scene_controls.inc:60`; RE #9). | MISSING / WRONG file | MINOR | Read `Keyboard.ini` first (RA2 sidebar wheel/page codes, `HealthNav`), then build the page over `HotkeyTable`. | M | — |
| W2-UI-28 | Shell resolution / sharpness | At 800×600 and above the original uses the "L" assets (MNSCRNL 632×568, `RA2TS_L.BIK` 632×570, LWSCRNL) with 9 button rows (`mainmenu_true.md` §1/§7.2) | A 640×480 canvas with the "S" assets, scaled ×1.5 into 1280×720 (`main.cpp:6326-6344, 6389`). Glyphs and art are blocky (`s01_main.png`). | PARTIAL | MINOR | An 800×600 logical shell (×1.2) with the L assets and the 9-row grid. The row distribution is RE-FALLBACK (`mainmenu_true.md` §1). | M | — |
| W2-UI-29 | Diplomacy button | Opens 0x73 Diplomacy (Player / Allies / Chat / Kills, Taunts On, Return) (RE #7) | Drawn at frame 0, inert (`main.cpp:3854-3857`) | MISSING | MINOR | A read-only shell page listing houses, allies and kills (skirmish). | S | 13 |
| W2-UI-30 | Rally point marker | Not traced (Q8) | Cyan X + yellow primary triangle (`main.cpp:4118-4157`) | PARTIAL | MINOR | After Q8. | S | Q8 |

These items are verified as **WORKING** and should not be re-reported:
- The main-menu composition (SDBTNANM buttons, SDWRNANM monitor, Bink CRT) matches §7–§9.
- Options page 0xD5 matches its template rects and skins.
- Repair/sell pressed frames and cursors work.
- Strip scrolling works (G-12).
- The power-pip model works.
- Radar click and minimap content work (G-09).
- Veterancy chevrons work.
- Esc routing works (G-01).
- The pause-while-menu-open behaviour works.
- Skirmish Save/Load is faithful to the original (RE #4).

## Exact layout specs for the pages that are wrong

All coordinates are 640×480 shell pixels (the template DLU rects pass through MapDialogRect). Buttons snap to the §7.2 grid: x = 484, 156×42, row y = 199 + 42k.

- **0xB5 Game Options (in game, campaign and skirmish).**
  - Header "Game Options" at (477, 1, 162, 16).
  - Rows:

    | Row | Button | Control id |
    |---|---|---|
    | 0 | Game Controls | 0x521 |
    | 1 | Load Game | 0x51E |
    | 2 | Save Game | 0x51F |
    | 3 | Delete Game | 0x520 |
    | 4 | Abort Mission | 0x522 |
    | 5 | Resume Mission | 0x686 |

  - Status line 0x695 at (15, 458, 454, 19).
  - No 0x71A movie and no 0x71C monitor, so the left screen is MNSCRNS and SDTP frame 0 is drawn static.
  - Esc = Resume.
- **0xBBA (MP only).** Game Controls / Abort Mission / Resume Mission at rows 0 / 1 / 5.
- **0xB6 Abort.**
  - Text "What would you like to do?" centred in (66, 222, 345, 30).
  - Row 0: Abort.
  - Row 1: Restart (campaign only).
  - Row 5: Resume Mission.
- **0xBBB Game Controls.**
  - Header "Game Options".
  - Sliders: Game Speed (130, 141, 192, 21) 0..6, Scroll Rate (130, 191, 192, 21) 0..6.
  - Labels: right-aligned labels at x = 6, w = 117; "Faster" labels at x = 331.
  - Checks: Target Lines (48, 313), Tooltips (235, 313), See Hidden (48, 342).
  - Buttons: row 0 Sound, row 1 Keyboard, row 5 Back.
  - Tooltips: `STT:GameOptSliderSpeed`, `STT:GameOptSliderScroll`, `STT:GameOptCBox*`, `STT:GameOptButtonBack`.
- **0xB8 Sound Options.**
  - Labels (13, 43 / 79 / 115, 135, 24): Music / Sound / Voice Volume.
  - Sliders (157, 45 / 81 / 117, 262, 21), 0..10 with readout.
  - Shuffle check (43, 251, 105, 22).
  - Score listbox (157, 157, 262, 160).
  - Play (43, 347, 124, 24), Stop (295, 347, 124, 24).
  - Back at row 5.
  - Tooltips: `STT:SoundOpt*`.
- **0xB7 / 0x2B4 / 0x2B5.**

  | Element | Rect |
  |---|---|
  | Title static | (36, 26, 399, 39) |
  | Listbox (Load, Delete) | (36, 81, 399, 303) |
  | Listbox (Save) | (36, 81, 399, 255) |
  | Edit (Save only) | (37, 362, 399, 22) |
  | Action button | row 0, h = 35 |
  | Back | row 5 |

- **0x94 Campaign.** See W2-UI-18. The tutorial well 0x6EC is RA2's Boot Camp, with tooltip "Play the Tutorial Campaign with 3 hidden missions!".
- **0x120 / 0x121 popups.**
  - Size 451×326, centred on the 640×480 canvas, i.e. (95, 77).
  - Text: (60, 65, 330, 81) relative to the popup.
  - OK: (310, 251, 124, 24). 0x121 has a third button at (310, 219).
  - Cancel: (310, 284, 124, 24).
- **Sidebar cameo cell** (RE #2): cameo (0, 0); darken (0, 0); clock (0, 0) translucent; count (60, 1) right-aligned; status (30, 1) centred or (2, 1) left-aligned; the tooltip anchors to the cell (Q3).

## Prioritised fix plan

1. **W2-UI-01 sidebar palette.** One-line class fix, the biggest visual win.
   - Then W2-UI-02 (clock), W2-UI-05 (darken), W2-UI-03 / 04 (Ready / On Hold / count text in the game font, W2-UI-12).
   - Hotspots: `main.cpp:1980-1986, 3835-4021`, `game_scene_controls.inc:349-366`, `pres_game.inc:58-66`.
2. **W2-UI-07 cameo tooltips.**
   - Hotspots: `game_scene_controls.inc` (hover state) and `main.cpp:5271-5335` (render).
3. **W2-UI-12 one HUD font,** then the text consumers W2-UI-21 and the banners.
4. **The shell-page framework for in-game dialogs:** W2-UI-13 → 15 → 16 → 14 → 29.
   - Generalise `ShellScene` / `draw_shell` (`main.cpp:6353-6521, 6649-7100`) so `GameScene` can push a shell page as a modal overlay. `InGameMenu` / `SaveLoadDialog` / `GfConfirm` then become thin page definitions.
   - Keep the `menu:*`, `dialog:*` and `confirm:*` probe target names stable for the 12 play scripts.
5. **W2-UI-11 pip health bars** (needs Q1).
6. **W2-UI-18 campaign page,** then W2-UI-19 loading screen, then W2-UI-20 result / score pages (`campaign_scenes.inc`, `gf_result.inc`).
7. **Polish:** W2-UI-22, 23, 17, 06, 08, 09, 10, 24, 25, 26, 27, 28, 30.

## Open RE questions

- **Q1 Health pips.** For `FUN_005D5E00` and its draw consumers: the `PIPBRD` frame per class, pip frames (green / yellow / red), pip count and spacing, and the building bar orientation and anchor. Also the unit selection bracket art, i.e. whether it is lines or a SHP.
- **Q2 TabClass.** The frame meaning for `tab0N.shp` (5 frames), the flash timing on a new cameo, and the disabled rule.
- **Q3 Tooltip widget.** Hover delay, box art and colour, position relative to the cursor or cell, and the sidebar-button help text (repair/sell/tabs).
- **Q4 Credits tally.** Roll speed per frame, the tick sound (if any), and the colour pair `0xB0E98C/D`.
- **Q5 Font.** What face `FUN_005FEB60(0, 0xAF)` selects (GAME.FNT vs 12metfnt vs a resource font) and the UI colour at `0xAC1028`.
- **Q6 Score screens.** Who creates 0xBC6 / 0xBC7 (no `MOV ECX` immediate was found; probably a table or `PUSH`). Their art and graphs, and whether skirmish shows 0xBC6 before the shell.
- **Q7 Shell details.** The runtime version string, whether the first button is lit with no hover, and Esc on the main page.
- **Q8 Rally point marker.** The waypoint/rally line art when a factory is selected.
- **Q9 Popup background choice.** Which `pudlgbg` variant (`s` = Soviet emblem vs `n`) is used, keyed by side or by screen.
