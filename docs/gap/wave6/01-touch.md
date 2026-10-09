# Wave-6 gap analysis 01: real phone play (touch)

This is a read-only audit of master `e2b569e`, done in the worktree `D:\ra2-wt\w6-touch` (branch `w6-touch`). No engine code
was changed. It covers the touch experience from start to finish, the way the user plays on an Android phone. It
reports only gaps that are new or still open after `w4fix-ctl` / `w5-android`, which are both merged. Existing rows are
cited where they apply: R-006..R-010 (fixed), R-150, R-151, R-170, R-P3-5, and W4-AM-* in
`docs/gap/wave4/06-android-missions.md`.

Evidence is in `01-evidence/`:

- `emu_*.jpg`: emulator screenshots;
- `desk_*.jpg`: shots from the desktop touch harness;
- `hitmap_<TYPE>.png`: finger-tap hit maps;
- `hitmap.out.txt` and `touch-actions.out.txt`: raw outputs;
- `scripts/`: `touch-actions.play`, `gen_hitmap.ps1` + `hitmap_report.py`, and `save-all03.play`.

## 1. Setup

| Item | Used |
|---|---|
| Desktop | `build-w6` (Ninja Release, MinGW). The `--play` harness drives the real `TouchGestures` layer with `touch tap/dtap/hold/drag/boxdrag`. The harness canvas is 1280x720, not the phone canvas (W6-01-15). |
| APK | `gradlew assembleDebug -Pandroid.injected.build.abi=x86_64` (JDK 21.0.12, NDK r27c), which took 32 min. The result is `versionName 0.9.0+ge2b569e`, `versionCode 1020`, installed with `adb install -r -t`. |
| Device | AVD `ra2x86`: Android 11, 1920x1080 landscape, 420 dpi, SwiftShader. The game canvas is **974x548**, so 1 logical px is about 1.97 physical px, or 0.75 dp. |
| Input | `adb shell input tap/swipe/draganddrop` for single fingers. For holds, hold+drag and edge swipes, raw multi-touch events go to `/dev/input/event2` using the wave-4 `kplay.sh`/`mkev.py`. |
| Busy mission | I made an ALL03 opening save on desktop (`save-all03.play`), pushed it into `files/Saved Games/` and loaded it through the phone's Load Saved Game page. The save was deleted afterwards. |

### What already works on the phone (re-checked, no action needed)

- Tap to select and to order.
- One-finger drag scrolls.
- Two-finger tap deselects.
- Hold for 600 ms plus drag gives a band box: `emu_11_hold_drag_box.jpg` shows about 45 units selected.
- The hold ring.
- The CMD column: `emu_18_cmd_open.jpg` shows 14 buttons of 50x56 logical px, about 48 dp.
- Sell, repair and placement banners with the [X] button.
- Two-tap building placement.
- Back opens the Game Options page: the buttons are big, Load and Save work, and the IME behaves (R-152).
- Radar drag.
- Swiping the cameo strip.
- The three-finger perf overlay.
- Loading ALL03 from a save takes 5.7 s.

## 2. Gap table

Severity values: BLOCKER, MAJOR, MINOR. Effort values: S, M, L.

| ID | What a human player experiences | Evidence | What the original does | Sev | Eff | Fix location |
|---|---|---|---|---|---|---|
| **W6-01-01** | **Buildings can only be picked by their ground footprint.** On a phone this is much worse than R-P3-5 suggests. Tapping a tower's head, a lab's dome or a weather device's dish does nothing. A 1x1 defence offers a 60x30 logical diamond (about 45x23 dp) at its foot. Any own infantry standing next to it captures that diamond, because the unit box is 65x63 px and units win (`input_original.cpp:115-122`). Next to a GI, eight taps around a Prism Tower selected the GI or nothing, never the tower; the harness's own `touch tap building 0:ATESLA` also failed. Every building action uses the same pick: select, sell, repair, engineer capture or repair, spy, C4 or attack, garrison, set primary, rally. | **Hit maps** (10 px finger grid, `hitmap.out.txt`, `hitmap_*.png`; green = selects, red = art pixel that does not). The table below gives the share of each building's art pixels that select it. `desk_prism_tower_tap_selects_gi.jpg`. Code: `input.cpp:234-238` `building_under` and `input_original.cpp:117-118` both use `world_.building_at_cell(resolve_ground_cell(px,py))`. | Picking is by screen point. `FUN_00691d00` → `FUN_006d98f0(screen point)` first scans the tactical's list of drawn objects (`DAT_00b0be34`, {obj, x, y}) and keeps the nearest within a distance bound. Only then does it fall back to the cell's first object (`cell+0xE4`) [RE: `D:\ra2-re\w2-gfxout\q6\f_006d98f0.c`, `fixBout\pix2cell_00691d00.c`]. Which objects enter that list was not traced, so whether a building's upper art is pickable is [RE-FALLBACK]. A touch pick must reach the art either way. | **MAJOR** (touch) | M | `engine/input.cpp` `building_under` + `classify`: test the building's drawn art rect/mask (the renderer knows it) in draw order, before the cell fallback. On touch, prefer a building over a unit when the tap is inside the building's art and outside the unit's body. |
| **W6-01-02** | **In sell mode, a tap on a building's upper art sells the building behind it, instantly.** A tap on the Weather Device's upper-left dome sold the Construction Yard behind it. The game was then lost (no ConYard). There is no confirmation and no highlight on touch. | `touch-actions.out.txt` L88-L90: `built(0,GACNST)=0`, `built(0,GAWEAT)=1`. Shot before the tap: `desk_d_sell_weather_sold_conyard_before.jpg`. Code: `input_original.cpp:391-404` sells `building_under(ev.x, ev.y)` on the first tap. | With a mouse, the hovered object shows the sell / no-sell cursor before the click [RE: classify chain FUN_00692010]. The click then sells. | **MAJOR** | S | Fix W6-01-01. On touch, sell and repair become two-tap: the first tap highlights the building and shows "TAP AGAIN TO SELL <name>", the way placement already works (`touch_bar.inc:112-133`). |
| **W6-01-03** | **Reading a cameo's name and price by holding it pauses or cancels production.** A hold is the only way to see a tooltip on touch. When the finger lifts, the hold is sent as a right click: the first hold puts the build On Hold, a second hold cancels it with a refund. | `touch-actions.out.txt` L44-L50: `queue_stage` frozen, `evareq(EVA_OnHold)=1`, then `queue(structure)=""`. Code: `touch_gestures.cpp:163-166` (hold → right click); `main.cpp:6037-6042`; `game_scene_controls.inc:401-415,444-452`. | Mouse hover shows the tooltip with no side effect [RE: FUN_0067F580]. A right click suspends or abandons [RE: FUN_006AA390]. | MINOR | S | `touch_ui.inc`: on touch, a hold on a cameo shows only the tooltip. Hold and abandon move to a small 2-button popup next to the cameo, or need a hold on a cameo that is already showing its tooltip. |
| W6-01-04 | **The touch tooltip is drawn under the finger.** It is anchored at the hold point (bottom-left of the fingertip), so the thumb covers it. | `emu_17_cameo_hold_tooltip.jpg` (Power Plant $800 under the ring). Code: `hud_tooltip.inc:96-103` uses the hold point. | No touch in the original. | MINOR | S | `hud_tooltip.inc` `draw_tooltip`: on touch, anchor the box left of the sidebar at the cameo's row, or above the finger. |
| W6-01-05 | **Double-tapping a GI deploys it instead of selecting all GIs.** The on-screen hint promises "DOUBLE TAP: ALL OF TYPE". This affects every unit that deploys or unloads when clicked while selected: GI, Guardian GI, MCV, IFV/transport with cargo, Siege Chopper. | `touch-actions.out.txt` L29-30: `selected=1` after `touch dtap unit 0:E1`. `desk_a_dtap_gi_deploys.jpg` (the GI in sandbags). Code: the second tap sees the unit already selected → `Action::Deploy` (`input_original.cpp:159-163`), so `commit_double` (`:215-217`) is never reached. | Left_Up runs type-select only for ACTION_SELECT/TOGGLE (7/8 → `FUN_00731dd0`/`FUN_007318a0`) [RE: `fixBout\leftup_004ab8c0.c`]. A mouse double-click on a GI probably deploys it in the original too [RE-FALLBACK: not traced to the end]. | MINOR | S | `input_original.cpp:left_click_original`: on touch, a Deploy whose tap is a double tap of a Select (inside `kDblMsTouch`/`kDblSlopTouch`) becomes `commit_double`. Alternatively, make the hint truthful and add a TYPE button (W6-01-09). |
| **W6-01-06** | **In a crowd, a tap meant as a move re-selects one soldier and drops the army.** Each own unit owns a 65x63 logical box, about 49x47 dp, so the gaps in an ALL03-sized blob are all "select" (`emu_06_tap_gap_selects_gi.jpg`). A mouse player sees the select cursor and adjusts; a finger has no preview. The only workaround is CMD → MOVE → tap (3 taps per order). | `emu_06` (the health bar jumped to another GI; nothing moved). Code: `input.cpp:24-26` `kHitHalfW 32 / kHitUp 46 / kHitDown 16`; `input_original.cpp:156-173`. | The cursor shows Select before the click [RE: FUN_00692010 action → cursor]. | **MAJOR** (busy missions) | S-M | `input.cpp:unit_under`: on touch with ≥2 own units selected, a tap on an own unit that is **already in the selection** is a move to that cell, not a re-select. A deploy needs a lone selection or DPLY. Alternatively, shrink the touch box to the unit's body. |
| W6-01-07 | **A superweapon fires on the first tap, with no preview or confirm.** A finger miss on a 0.75 dp/px canvas drops a Nuke, Iron Curtain or Chronosphere on the wrong cell; a pan does not fire, but any stray tap does. Placement already has a 2-tap confirm. | Code: `input_original.cpp:374-382` (`sw_fire_confirmed` on the first left up); `touch_ui.inc:49-50` banner "TARGET - TAP THE MAP". | The mouse cursor shows the target cursor (and range) before the click. | MINOR | S | `touch_bar.inc:112-133`: reuse the placement two-tap path for `sw_targeting()`. The first tap draws the SW cursor and area at the cell; the second tap on the same cell fires. |
| W6-01-08 | **Tapping the radar to look somewhere sends the selected army there.** This is original behaviour: a radar click with a selection is a move. A phone player uses the radar as the main way to look around, and nothing warns them. | `touch-actions.out.txt` L112-113: `sel_moving=1`, `cam_x` unchanged after `touch tap button:radar` with a tank selected. Code: `game_scene_controls.inc:558-568`. | [RE: G-09, RadarClass action]: a click with a selection is a move, otherwise it centres. | MINOR | S | `touch_ui.inc` (engine affordance): on touch, a radar tap always centres. A radar move order needs MOVE armed, or a hold on the radar. Record it as a touch-only deviation. |
| W6-01-09 | **Several original commands are hotkey-only and have no touch path** (full list in §3): TypeSelect (T), teams 6-0, add-to-team (Shift+n), CenterBase (H), CenterOnRadarEvent (Space), Next/Prev object, Follow, View bookmarks, CombatantSelect, Health/Veterancy nav, Cheer. Shift+right-click on a unit cameo (abandon all) also has no path, because the sidebar ignores touch modifiers. | Code: `touch_bar.inc:30-43` (13 buttons); `game_scene_controls.inc:150-210` (the full hotkey set); `touch_bar.inc:134-135` (no modifiers on the sidebar). | Every one is in `KeyboardMD.ini` [RE: the CommandClass execs cited in `game_scene_controls.inc`]. | MINOR | M | `touch_bar.inc`: a second page in the column (`MORE`) with TYPE, HOME, EVT, NEXT, FOLW, 6-0 and VIEW1-4 (tap = jump, hold = set). Let +SEL reach the sidebar so +SEL plus a cameo hold abandons all. Partly overlaps R-150 and R-151 (view bookmarks). |
| W6-01-10 | **A system configuration change kills the running mission, and the next launch does not offer Resume.** Switching the phone to gesture navigation did this; other overlay or theme changes do the same, for example a Material You wallpaper colour change on Android 12+ or a font/display-size change. The background autosave was written and then deleted. | `emu_13_config_change_killed_game.jpg` (home screen), logcat `Config changes=20000500 … onDestroy … Finished main function … has died`; `emu_14_relaunch_no_resume.jpg` (main menu, no resume prompt). Code: `game_scene_lifecycle.inc:48-55` deletes `AUTOSAVE.SAV` in the scene destructor even when the activity is not finishing; `AndroidManifest.xml:68` configChanges lacks `density\|fontScale\|colorMode\|…`. | — (platform). | MINOR | S | `RA2Activity.onDestroy`: when `!isFinishing()`, tell native code to keep the autosave (a JNI flag checked in `lifecycle_teardown`). Add `density\|fontScale\|colorMode\|touchscreen\|mcc\|mnc` to configChanges. |
| W6-01-11 | **Box select is slow and limited.** Every box needs a 600 ms still hold, longer than Android's own 400-500 ms long-press, so a long-press-drag at the system timing scrolls instead (`emu_07`). The box cannot scroll the map while open and covers only the 974x548 view. | `emu_07_system_longpress_drag_scrolls.jpg` (`input draganddrop` = system long-press then drag → scrolled); `emu_11` (works at ≥950 ms). Code: `touch_gestures.hpp:35`, `touch_gestures.cpp:108-113`. | A mouse drag boxes immediately; edge scrolling during a drag. | MINOR | S | A sticky `BOX` toggle in the CMD column: while it is on, a one-finger drag boxes with no hold. Optional: edge auto-scroll while band-dragging on touch. |
| W6-01-12 | **HUD text is small on a phone.** EVA and objective lines are about 22 physical px tall, about 8.4 dp. Cameo captions ("POWER PLANT") are about 12 px, about 4.6 dp, below Android's 12 sp guideline. | `emu_04_all03_perf.jpg` ("Objective 1: Destroy the Psychic Beacon"; cameo caption). Code: `hud_text.inc` (game.fnt 13-row cells at 1:1 on the 548-px canvas). | 1:1 at 800x600 on a 15" monitor. | MINOR | S | `hud_text.inc` already has a `scale` parameter. On touch canvases, draw messages and tooltips at 1.5x. Cameo captions are baked into the art, so leave them. |
| W6-01-13 | **The sidebar Options button is 72x18 logical (about 54x13.5 dp)** and is not in the touch near-miss snap list. Back works with 3-button navigation. With gesture navigation in immersive mode, the first edge swipe only reveals the bars (W6-01-14), so the small button is the visible way in. | Code: `sidebar.cpp:32`; `touch_ui.inc:128-143` (snap covers tabs, strip arrows, repair and sell only). | Mouse target. | MINOR | S | Add `lay_.opt_btn` (and `diplo_btn` if it is ever live) to `touch_snap_sidebar`'s `consider(...)`. |
| W6-01-14 | **With gesture navigation, a drag from the screen edge reveals the system bars over the game:** the status bar covers the EVA/objective line and the gesture handle covers the bottom edge. The drag also scrolls the map. | `emu_16_gesture_nav_edge_swipe.jpg`. No `setSystemGestureExclusionRects` and no `layoutInDisplayCutoutMode` in `RA2Activity.java`. | — (platform). | MINOR | S | `RA2Activity`: exclusion rects for the left CMD column (≤200 dp) and the sidebar strip. `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES` and a safe inset for the CMD column on notched phones. |
| W6-01-15 | **Tooling: `--play` cannot run on the phone canvas**, so touch scripts check 1280x720 geometry; phone sizes and fits need the emulator. A second trap: a mouse `click … right` at y ≥ 696 parks the pointer in the edge-scroll band and the camera drifts, which broke my first hit-map runs. | `app_robust.inc:27-48` (`game_logical_size` is 1280x720 under `--play`); `main.cpp:6081-6087` (edge band, `mouse_present_`). | — | MINOR | S | `set canvas WxH` in the harness (passes through `game_logical_size`). Document the edge-band trap in `scripts/play/README.md`. |
| W6-01-16 | Performance, re-measured in a busy mission (still **R-170**, no real arm64 numbers). ALL03 opening (59 Russians plus the White House crowd), emulator at 974x548: **16-20 fps**. Breakdown: present 44-52 ms (the SwiftShader GL pipe), update 4-7 ms, render 1.5 ms, worst 65-76 ms, logic 19-20 ticks/s. Loading the save took 5.7 s. While the overlay is on, it covers the CMD column's bottom row. | `emu_04_all03_perf.jpg`; logcat `[perf] fps=19.6 … present=44.41ms … canvas=974x548`. | — | (R-170) | — | No engine work indicated by these numbers (render and update are small). A real arm64 phone run is still required. Optionally move the overlay to the top edge. |

### W6-01-01 hit-map numbers

Method: each building was placed by cheat at cell (75,25) with a fixed camera, on the default lobby map. Finger taps
went on a 10 px grid over 300x320 px around it. "Art samples" are grid points where `hit_<T>.png` differs from the
same view without the building, shadow excluded.

| Building | Art samples | Art samples that select it | Off-art samples that select it (bare footprint) |
|---|---|---|---|
| Prism Tower (ATESLA) | 22 | **7 (32 %)** | 1 |
| Tesla Coil (TESLA) | 11 | **4 (36 %)** | 4 |
| Battle Lab (GATECH) | 102 | **32 (31 %)** | 22 |
| Power Plant (GAPOWR) | 36 | 21 (58 %) | 15 |
| Air Force Command (GAAIRC) | 66 | 41 (62 %) | 13 |
| Barracks (GAPILE) | 61 | 43 (70 %) | 11 |
| Weather Device (GAWEAT) | 75 | 56 (75 %) | 24 |
| War Factory (GAWEAP) | 122 | 96 (79 %) | 38 |
| Pillbox (GAPILL) | 7 | 6 (86 %) | 2 |

Tall buildings, which include every 1x1 defence and the tech buildings, are about 2/3 untappable. In these runs no unit
stood next to the building. With one GI at its foot, the Prism Tower became 0 % selectable (`desk_prism_tower_tap_selects_gi.jpg`).

## 3. Original mouse actions, and how each is done on touch

| Original (mouse / keyboard) | Touch today | Verdict |
|---|---|---|
| Left-click select / order | tap | OK |
| Shift+click add/remove | CMD → +SEL (sticky) → taps | awkward (2 extra taps) |
| Left-drag band box | hold 600 ms + drag | awkward (W6-01-11) |
| Double-click = type on screen | double tap | broken for deployers (W6-01-05) |
| T / TT type select (screen / map) | — | **impossible** (W6-01-09) |
| Ctrl+click force fire / Alt+click force move | CMD → FIRE / MOVE → tap (one shot) | awkward |
| Cursor preview of the action (select / move / no-move / attack / enter / capture / sell…) | — (no hover) | **impossible**; causes W6-01-02, W6-01-06, W6-01-07, W6-01-08 |
| Click the selected unit = deploy; D | tap the selected unit; CMD → DPLY | OK |
| Right-click deselect / cancel mode | two-finger tap; hold = cancel mode; banner [X] | OK |
| Right-drag scroll; edge scroll; arrow keys | one-finger or two-finger drag | OK (no fling) |
| Radar click: centre / move order; radar drag | tap (= move with a selection); drag | surprising (W6-01-08) |
| Building pick by its art | footprint only | broken for tall art (W6-01-01) |
| Sell / repair click | banner + tap | dangerous (W6-01-02) |
| Placement click | two taps on the same cell | OK |
| Superweapon click | one tap | risky (W6-01-07) |
| Rally (factory selected + click), set primary (re-click) | same by tap | OK (a factory selection swallows every ground tap as a rally until a two-finger tap) |
| Garrison / enter transport / engineer / spy / C4 by click | tap | OK for units; for buildings see W6-01-01 |
| Cameo left: build / queue / resume / place | tap | OK |
| Cameo right: hold, then abandon | hold (also = tooltip) | conflicting (W6-01-03) |
| Shift+right cameo: abandon all | — | **impossible** (W6-01-09) |
| Hover tooltips (cameos, power bar, buttons) | hold | OK but under the finger (W6-01-04) |
| Tabs / strip arrows / repair / sell buttons | tap (near-miss snap) | OK; tab width ~21 dp (R-151) |
| Options button / Esc | Back key; 13.5 dp button | OK with 3-button nav (W6-01-13) |
| Ctrl+1..0 create, 1..0 select, Alt/double = centre | hold 1-5 / tap 1-5 / double tap | teams 6-0 **impossible** (W6-01-09) |
| Shift+1..0 add team to selection | — | **impossible** (W6-01-09) |
| H centre base, Space last event, P/N next/prev, F follow, Q combatant, health/veteran nav, C cheer, F-key view bookmarks | — | **impossible** (W6-01-09; bookmarks R-151) |
| Z / planning mode (waypoints), beacons | — | not implemented on any platform (R-150) |
| F1 briefing | CMD → BRF | OK |
| Save / Load / Delete | Back → Game Options | OK (list rows ~14 dp, R-151) |

## 4. Prioritized fix list (sized for one fix agent)

1. **W6-01-01 + W6-01-02: art-based building pick, and a safe sell/repair on touch** (M). A shared `pick_building(px,py)` in `engine/input.cpp` tests each building's drawn art rect/mask in draw order (front first), then the cell. `building_under`, `classify` and `commit_factory_click` use it. On touch, sell and repair become tap-to-highlight, tap-again-to-commit. Acceptance: re-run `gen_hitmap.ps1` / `hitmap_report.py`; the art share should be ≥ 90 % for every type. Extend `touch-actions.play` D: the ConYard survives.
2. **W6-01-06: an army tap in a crowd is a move** (S-M). With ≥2 own units selected on touch, a tap on a unit of the selection moves there. Acceptance: a new ALL03-save play step.
3. **W6-01-03 + W6-01-04: cameo hold = tooltip only, anchored off the finger** (S). Hold and abandon move to a popup.
4. **W6-01-10: keep the autosave when Android destroys the activity without finishing**, plus wider `configChanges` (S). Acceptance: on the emulator, switch nav mode mid-game, relaunch, get the Resume prompt.
5. **W6-01-07: superweapon two-tap confirm** (S). Reuse the placement path.
6. **W6-01-05: double tap on a deployer selects the type on touch** (S).
7. **W6-01-09 + W6-01-11: CMD column page 2** (TYPE, HOME, EVT, NEXT, FOLW, 6-0, VIEW1-4) **and a sticky BOX toggle**; let +SEL reach the sidebar (M).
8. **W6-01-12/13/14: 1.5x HUD message and tooltip text on touch canvases, Options-button snap, gesture-exclusion rects and a cutout mode** (S).
9. **W6-01-08** (radar tap centres on touch) and **W6-01-15** (`set canvas WxH` in `--play`) (S).

The real-phone fps and load numbers (R-170 / W6-01-16) still need the user's phone. INSTALL.md §5 has the steps.

## 5. Fix status (branch `w6fix-touch`)

| ID | Status | Fix |
|---|---|---|
| W6-01-01 | FIXED | `Input::pick_building`: mouse = the original (drawn-object hotspot `dx^2 + 0.5 dy^2 < 200` at the origin cell's centre, then the cell; `docs/re/object-pick.md`); touch = the drawn art first (main frame + ActiveAnim/IdleAnim/SuperAnim slots), and a unit's selection box only wins on its body. Hit maps in `01-evidence-fix/`: Prism 100 %, Tesla 100 %, Battle Lab 100 %, every type >= 94 %; the Prism head selects the tower with GIs at its foot (`ctl-touch-w6`). |
| W6-01-02 | FIXED | Sell / repair on touch: tap = mark (outline + "TAP AGAIN TO SELL <NAME>"), second tap on the same building = commit. The dome tap no longer sells the ConYard (`ctl-touch-w6`). |
| W6-01-03 | FIXED | A cameo hold shows the tooltip only; a popup beside the cameo has PAUSE / RESUME, CANCEL, ALL. |
| W6-01-04 | FIXED | The touch tooltip sits above and left of the finger, at 1.5x. |
| W6-01-05 | FIXED | A touch double tap = type select, never a deploy (a single tap on the lone selected deployer deploys after the 500 ms window). |
| W6-01-06 | FIXED | With >= 2 selected, a tap on a unit of the selection is a move; a tap beside an own unit (box, not body) is ground; a tap on an unselected unit's body selects it. |
| W6-01-07 | FIXED | Superweapon on touch: tap = target area preview (3x3 / CellSpread ellipse), second tap on the same cell = fire. |
| W6-01-08 | FIXED | A touch radar tap always centres; a radar move needs MOVE armed. |
| W6-01-09 | FIXED (part) | Command column page 2 (MORE): TYPE, HOME, EVT, NEXT, PREV, FOLW, teams 6-0, views V1-V3 (hold = SetView, tap = View). The popup's ALL replaces Shift+right on a cameo. View 4, CombatantSelect, Health/Veterancy nav and Cheer still have no touch path. |
| W6-01-10 | not this branch | The release agent (#6). |
| W6-01-11 | FIXED | Hold 600 -> 450 ms; sticky BOX toggle (one-finger drag boxes at once). Edge auto-scroll while band-dragging: not done. |
| W6-01-12 | FIXED | Messages, objectives, tooltips 1.5x on touch canvases. |
| W6-01-13 | FIXED | The Options button joins the sidebar near-miss snap (20 px). |
| W6-01-14 | FIXED (needs a gesture-nav phone) | `setSystemGestureExclusionRects` for the left command-column strip and the right cameo strip (200 dp each, Android 10+). The cutout mode stays the default (no inset needed). |
| W6-01-15 | FIXED | `--play` `set canvas WxH`; the edge-band trap is in `scripts/play/README.md`. |
| W6-01-16 | PARTIAL | The perf overlay moved right of the command column; real arm64 numbers are still R-170. |
