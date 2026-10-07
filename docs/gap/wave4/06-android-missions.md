# Wave-4 gap analysis 06: playing the campaign on Android (touch)

This is a read-only audit of master `4647b17` (2026-10-07). I built it in the worktree
`D:\ra2-wt\w4-android` (branch `w4-android`, nothing committed, no engine source changed) and played it
**with touch only** on the `ra2x86` emulator. The goal was to reproduce what the user sees on their phone:
"I can't control the mission", "the Statue of Liberty is not positioned well" and "no rockets flying in
the first mission".

Evidence is in `06-evidence/`:

- emulator screenshots, downscaled to JPEG;
- the raw kernel-touch tools `mkev.py`, `kplay.sh` and `ktap.sh`.

Scratch outputs (full-size PNGs, `logcat_all01.txt`, `logcat_sov01.txt`, SurfaceFlinger dumps) are in
`D:\ra2-wt\w4-android\_scratch\` and are not committed.

## 1. Environment

| Item | Used |
|---|---|
| APK | Built in my worktree: `gradlew assembleDebug --max-workers=3`, JDK 21.0.12, NDK r27c, `RA2_SDL2_SRC_DIR=D:\ra2-android\build\_deps\sdl2-src`. Debug = RelWithDebInfo `-O2`. The build took **28 min**, at below-normal priority and sharing the PC with other agents. The APK is 5.5 MB with arm64-v8a and x86_64 libraries, `versionCode 776`, `versionName 0.9.0`. That is **identical** to `D:\RA2-phone\RA2-Engine.apk` (rebuilt 16:10), so two builds cannot be told apart from the version (W4-AM-12). |
| Device | AVD `ra2x86`: Android 11 / API 30, x86_64, `-gpu swiftshader_indirect`, 3 GB RAM, 4 vCPU, audio on, headless. The panel was **1920x1080 at 420 dpi (16:9)**, then **2400x1080 (20:9, typical of current phones)** via `wm size 1080x2400`. |
| Game files | A fresh `adb uninstall` then `install`. The 11 files were in `/sdcard/Download/RA2` and were imported **through the in-app importer** (§2). |
| Input | `adb shell input tap/swipe` for single-finger gestures. For multi-touch, raw `input_event` structs written to `/dev/input/event2` (`virtio_input_multi_touch_1`, the only touch device in direct mode, `SurfaceOrientation 1`) after `adb root`. Landscape (x, y) maps to portrait (1080−y, x), scaled to 0..32767, with `ABS_MT_PRESSURE` > 0. See `06-evidence/mkev.py` and `kplay.sh`. |
| Canvas | Logcat `Window size: 1920x1080`. `game_logical_size()` (app_robust.inc:25) gives a **860x484** game canvas on 16:9 (tactical view 692x484, scale 2.23) and **1076x484** on 20:9. The shell stays 1280x720 (scale 1.5), pillarboxed by 240 px per side on 20:9. |

Only one heavy job ran at a time: Gradle first, then the emulator.

## 2. Did the in-app importer work? **Yes, with real touch. Copy is fast. Two usability traps.**

These are the exact steps on a fresh install with the files in `Download/RA2`:

1. Launch. `SetupActivity` shows "Red Alert 2 game files needed" and lists the 4 required and 7 optional
   files (`01_setup.jpg`).
2. Tap **Import game files**. The DocumentsUI picker opens, in **portrait** (the app is landscape).
3. At the storage root and in `Download` itself the picker shows **"Can't use this folder – To protect
   your privacy, choose another folder"** (`05_picker_download_cant_use.jpg`). This is the Android 11+ rule
   for `ACTION_OPEN_DOCUMENT_TREE`. The user must open the `RA2` sub-folder (`06_picker_ra2.jpg`).
4. Tap **USE THIS FOLDER**, then **ALLOW**. The copy starts with a progress bar (`08_importing.jpg`).
   Logcat:

   ```
   RA2Setup: imported multimd.mix (31264268 bytes) ... imported MAPS02.MIX (3170324 bytes)
   RA2Setup: import done: 11 files, 746956882 bytes          (16:31:50 -> 16:32:18, ~29 s)
   SDL     : Window size: 1920x1080
   SDL/APP : [assets] android asset dir = /storage/emulated/0/Android/data/org.ra2.engine/files/ra2/
   ```

   The game then starts automatically and the main menu appears about 2 s later.

**The wave-2 open item is closed:** the SAF copy path works end to end, with case-insensitive names
(`MAPS02.MIX`, `Maps01.mix`), size checks, then rename. Wave 2 failed to drive the picker because DocumentsUI
**ignores `adb shell input tap` on its file grid** (its toolbar accepts them). Kernel-level touches work.
That limitation affects test automation only, not users.

Traps found:

- **The phone package has no instructions** (W4-AM-10). `D:\RA2-phone` holds the APK and an `RA2\` folder.
  If the user copies the 11 files loose into `Download` (not into a sub-folder), the picker refuses
  `Download`, and nothing in the app explains why.
- **The setup screen has no guard during the copy.** `Exit` stays enabled while copying
  (`SetupActivity.java:144,173-174`), and the copy thread is unowned (`:192`). Leaving mid-copy can leave
  `*.part` files behind. A retry recovers from that. Minor.

## 3. ALL01 by touch: what happened

The flow was: Main menu, **Single Player** (`input tap 1500 493`), **New Campaign**, Allied (the list row
is 38 px, about 14 dp, but it was preselected), then **Load**. The LS800 screen came up and the game loaded:
`[load] LOADING done in 5928 ms (67 checkpoints)`, `[audio] game handle ok: sdl_devices=1 handles=2`.
There were **0 SDL assertions** in either session and no crash.

| Step | Result | Evidence |
|---|---|---|
| Opening | The camera starts on the office block at the start cell (the same view as desktop `m-all01/start.png`). The `MoveCameraToWaypoint` pan then slides to the 4 Dreadnoughts. On the phone canvas **the Dreadnoughts are not on screen at mission start** (desktop shows them top-right) and no player unit is visible. | `13_load_6.jpg`, `15_after_pan.jpg` |
| Tap the unit (Tanya) on the pier | Selected (health pips). | `16_tap_gi.jpg` |
| Tap a Dreadnought | An order is issued and Tanya walks to the pier end. | `17b` (scratch) |
| **One-finger drag to scroll** | **The map does not scroll.** A band box is drawn and the **selection is cleared** (empty box). | `18_onefinger_drag.jpg` |
| Two-finger drag (raw multi-touch) | The map pans 1:1. It works, but nothing on screen says this is the gesture. | `19_twofinger_pan.jpg` |
| Pan to Liberty Island | The Statue renders (green, `Palette=lib`). The figure stands on the star plinth exactly as on desktop (`art-all01/statue.png`). The GIs around it are not the player's: tapping them does nothing, which is expected. | `22_statue.jpg` |
| Band box around those GIs | Nothing selected (not the player's units). | `23` (scratch) |
| Hold 450 ms | Treated as a tap: a move order. | `26` (scratch) |
| Hold 750 ms | Treated as a right click: deselect. The 500 ms threshold works as coded. | pixel probe |
| Long-press **1**, deselect, tap **1** | Team created and recalled. | `30` (scratch) |
| Tap **$** (Sell) on the sidebar, then tap Tanya | **She cannot be selected.** Sell mode stays armed. The only cue is the lit `$` and a "Sell Mode" tooltip stuck by the sidebar. Nothing changes on the map. Tapping `$` again disarms it, and then she selects. | `27_sell_armed.jpg`, `28_sellmode_tap_unit.jpg` |
| Back key | The Game Options page opens (paused). Abort, then Abort, returns to the main menu. | `31_back_menu.jpg` |
| Minimap | ALL01 has no radar, so the eagle logo is inert. Tapping the radar recentres (code: `game_scene_controls.inc:411-421`). There is **no drag-to-scroll on the radar**. | — |
| Dreadnoughts over about 6 game minutes | **They never fired.** No V3 rockets flew in any screenshot. This matches the user's "no rockets" report. It is not Android-specific: the same scene on desktop is covered by `03-all01-opening.md` and `04-projectiles.md`. | `15`, `25` |

## 4. SOV01 by touch: what happened

| Step | Result | Evidence |
|---|---|---|
| Start SOV01 **in the same process** after aborting ALL01 | **The sidebar is the Allied (eagle, blue) art** while the layout uses the Soviet constants (tab and button rects shifted). The menu art is Soviet. | `42_sov_deployed.jpg`, `44_build_ready.jpg` |
| Start SOV01 in a **fresh process** (`am force-stop`) | Soviet sidebar (hammer and sickle), correct. This confirms a cross-session texture-cache leak (W4-AM-04). | `53_fresh_sov01.jpg` |
| Tap the MCV, then **DPLY** | Deploys. The camera trigger then pans and the Construction Yard ends up **under the touch bar**. | `42_sov_deployed.jpg` |
| Tap the Tesla Reactor cameo | Builds. A **"Tesla Reactor $600" tooltip stays over the cameo** for the whole build, because the hover timer runs on the last tap point. | `44_build_ready.jpg` |
| Tap the cameo (Ready), tap the ground, tap again | Green footprint on the 1st tap, placed on the 2nd (W2-AR-16c works). | `46_place_preview.jpg`, `47_placed.jpg` |
| Switch the panel to 20:9 **mid-game** | The canvas stays 860x484 and is pillarboxed. A new mission gets 1076x484, full width. | `50_209_ingame.jpg`, `51_209_all01.jpg` |

## 5. Specific checks

- **Touch-to-click transform.** Correct. SDL's renderer event watch remaps finger coordinates into the
  logical viewport (`SDL_render.c:816-850`). `Platform::poll` multiplies them by the current logical size
  (`platform.cpp:276-293`). The driver sets the scene's logical size before polling (`main.cpp:4797-4802`).
  The checks:
  - every tap landed on the intended target: the shell buttons at both 1.5x pillarboxed layouts, the touch
    bar, cameos, `$`, units, and the ground;
  - the kernel tap at landscape (180, 1008) toggled `+SEL` exactly as `input tap` did.

  There is no hit-test offset bug. One remaining edge: SDL **clamps** touches in the shell's pillarbox bars
  to the viewport edge (`SDL_render.c:825-830` FIXME), so a touch in the black bar acts as an edge touch.
  It is harmless today because there are no edge buttons.
- **Command-bar modes getting stuck.**
  - `FIRE` and `MOVE` are one-shot and clear on the next left tap (`touch_bar.inc:325-328`). Arming FIRE and
    then tapping the sidebar keeps it armed until the next map tap, which is acceptable.
  - `+SEL` is **sticky by design** (`:258`). While it is on, every tap *toggles* selection and taps on own
    buildings do nothing (`input_original.cpp:178`). The red button is the only cue.
  - The worse "stuck" state is the **sidebar Repair/Sell mode** (W4-AM-03). Placement and super-weapon
    targeting have the same shape: right-click cancel exists only as a long-press or two-finger tap.
- **Long-press timing.** It is 500 ms with 12 logical px of slop (`touch_gestures.hpp:27-28`), about 14 dp
  at a 484-px canvas. Verified: 450 ms acts as a tap, 750 ms as a right click. Two side effects:
  - any **slow tap** (500 ms or more) becomes **deselect / cancel**;
  - **long-press on the map has no visual feedback** until release.

  Double-tap select-by-type uses `kDblSlopO = 6` logical px (`input_original.cpp:31`), about 13 physical px
  or 5 dp, so finger double-taps often miss (W4-AM-09).
- **Sidebar at phone scale (484 canvas, 0.85 dp/px).**

  | Element | Logical size | dp | Meets 48 dp? |
  |---|---|---|---|
  | Cameos | 60x48 | 51x41 | OK |
  | Tabs | 28x27 | **24x23** | no |
  | Repair/Sell | 64x31 | 54x26 | height short |
  | Strip scroll | 46x25 | **39x21** | no |
  | Touch-bar buttons | 50x56 | 42x48 | OK |

  4 cameo rows are visible (`sidebar.cpp:42`). Tabs and the strip arrows stay below the 48 dp guideline,
  and the Allied list rows on the campaign page are 14 dp.
- **Performance (emulator, SwiftShader software GL, so a floor only).** SurfaceFlinger: **15.4 fps** in the
  ALL01 opening at 1920x1080 and **19.6 fps** in SOV01 at 2400x1080. The engine thread used 96% of one
  vCPU. Load time is about 6 s per mission. PSS is 300 MB and RSS 405 MB in game. A real phone with GLES
  will be faster; it still needs a real-device measurement.
- **Audio.** One AudioFlinger track from the game pid (22050 Hz stereo, **0 underruns**, `Active yes`), one
  SDL device (`sdl_devices=1 handles=2`), no AAudio assertions. The track gain of −33 dB is just the AVD's
  media volume (5/15). I could not judge audibility headless. Audio focus is still not requested (W2-AR-19).
- **Statue position, Android against desktop.** **No platform difference.** The Statue and its plinth
  render in the same world-space relation (`22_statue.jpg` against desktop `art-all01/statue.png`). What
  differs is **how much map fits**: the phone tactical view is **692x484 logical** against desktop
  **1112x720**, i.e. **42% of the area** on 16:9 and 65% on 20:9, with the bottom 60 px under the touch bar.
  So the opening framing ("the mission opens on the player's first unit", `main.cpp:6565-6570`, then the
  trigger pan) shows a building and black shroud, and the Statue/harbour is off screen. If the Statue looks
  misplaced *relative to its pedestal*, that is a shared renderer issue (see `03-all01-opening.md`), not an
  Android one.

## 6. Gap table

Severity values: BLOCKER, MAJOR, MINOR.

| ID | Issue | Expected | Actual (screenshot / logcat) | Root cause (file:line) | Severity | Fix sketch |
|---|---|---|---|---|---|---|
| W4-AM-01 | **One-finger drag does not scroll the map. It band-boxes and clears the selection.** This is the most likely cause of "I can't control the mission". | Mobile RTS norm: one-finger drag on empty ground pans. Selection is by tap, or by a band box behind a mode or a long-press-drag. | `18_onefinger_drag.jpg`: no pan, selection lost. Panning needs a **two-finger** drag (`19_twofinger_pan.jpg`), which nothing on screen teaches. | `touch_gestures.hpp:10-16`, `touch_gestures.cpp:144-150` (drag maps to a left band press). `input_original.cpp:374-376` (`commit_box` with an empty box deselects). | **BLOCKER** (phone) | One-finger drag emits `Pan`. Band select becomes long-press-then-drag, or a `BOX` toggle on the touch bar. Keep two-finger pan. Never deselect on an empty touch box. Show a first-mission hint ("drag to scroll · tap to select · hold for menu"). |
| W4-AM-02 | **The phone canvas shows 42% of the desktop map area. The ALL01 opening shows no player unit, no Statue and no ships.** | Phone players must see their units and the objective at start. The original's "home" view centres the player's force. | `13_load_6.jpg` against desktop `m-all01/start.png`: same centre, much smaller view. The Dreadnoughts appear only after the trigger pan, and the Statue is off screen (`22_statue.jpg` needed about 9 pans). | `app_robust.inc:25-46` picks H = 484 (0.85 dp/px). Opening camera: `main.cpp:6565-6570` (first unit with house 0, not a unit-group centroid). | MAJOR | Centre the opening on the centroid of the player's units, falling back to the start waypoint. Consider about 540–600 logical height on phones (a little zoomed out) with larger touch-specific chrome. Add a "jump to my units" button (repeated group tap already centres). |
| W4-AM-03 | **Repair/Sell (and placement / super-weapon) modes are invisible on touch and block selection.** | An armed mode must be obvious and easy to leave. | `27_sell_armed.jpg`: only the lit `$` and a tooltip. `28_sellmode_tap_unit.jpg`: tapping Tanya does nothing. Leaving needs a 500 ms long-press, a two-finger tap, or a re-tap of `$`. | The mode cursor is drawn only for a real mouse: `main.cpp:5961-5967` (W2-AR-16a). Cancel exists only as a right click: `input_original.cpp:337-341`. | MAJOR | When `!mouse_present_` and a mode is armed, show a banner ("SELL MODE – tap a building · ✕") with a big cancel button in the touch bar. Draw the mode cursor at the last tap point. Optionally cancel the mode when a tap selects an own unit. |
| W4-AM-04 | **The second mission in a session shows the previous side's sidebar art** (Allied chrome in SOV01). | The sidebar follows the player's side. | `42_sov_deployed.jpg` (SOV01 after ALL01, eagle art, Soviet layout rects) against `53_fresh_sov01.jpg` (fresh process, correct). It affects desktop too. It is more visible on Android, where the process lives on. | TextureCache keys `"sb:top"`, `"sb:side1"`, `"sb:tab*"`, `"sb:rep"`, … are side-agnostic (`main.cpp:4319-4403`). The cache lives for the whole `run_menu` (`main.cpp:7736`/`7778`). `g_sidebar_mix` changes at `main.cpp:5295` but cached textures win. | MAJOR | Prefix the keys with `g_sidebar_mix` (e.g. `"sb2:"`), or `tc.clear()` when a GameScene is built with a different side. Audit other side-dependent keys (`hud_text.inc:42`, the menu art). |
| W4-AM-05 | **The touch bar covers the bottom 60 logical px (12% of the tactical view, 676 of 692 px wide on 16:9).** | On-screen controls should not hide gameplay, or should be collapsible to a corner. | SOV01: the Construction Yard is under the bar after the opening pan (`42_sov_deployed.jpg`). Taps and box-starts there are swallowed. | `touch_bar.inc:242-254` (full strip, `kWinH-60`). The bar is open by default (`:358`). | MAJOR | A vertical strip on the left edge, or a collapsed default (`CMD` only) that auto-collapses after a command. Shrink the input map/camera clamp by the bar height so the map can scroll its bottom row above it. |
| W4-AM-06 | **Hover tooltips appear and stay after taps** (e.g. "Tesla Reactor $600" over the cameo during the whole build). | Touch has no hover. Tooltips only on long-press. | `44_build_ready.jpg`, `27_sell_armed.jpg` ("Sell Mode"). | `hud_tooltip.inc:18-26` hover timer on `mx_/my_`; `track_pointer` stores tap points (`game_scene_play.inc:10-18`); `draw_tooltip` has no `mouse_present_` gate (`hud_tooltip.inc:99-103`). | MINOR (it hides the cameo clock) | Return `""` from `tooltip_now()` when `!mouse_present_`, or show the tooltip only while a long-press is held on the cameo. |
| W4-AM-07 | **Hold-to-cancel has no feedback, and any slow tap becomes deselect.** | A long-press ring/haptic at about 400 ms. A tap is anything shorter than the long-press. | Verified: 450 ms acts as a tap, 750 ms deselects (pixel probe, `26`). | `touch_gestures.cpp:204-209` fires the right click silently. | MINOR | Draw a progress ring at the finger after 150 ms. Raise the threshold to about 600 ms. Optional haptic (`SDL_HapticRumble` or JNI). |
| W4-AM-08 | **The sidebar tabs, strip arrows and campaign list rows are below touch size.** | 48 dp. | Tabs 24x23 dp, strip arrows 39x21 dp, Repair/Sell 54x26 dp, campaign/load list rows about 14 dp (`12_newcamp`, scratch). | `sidebar.hpp:31,40` (original pixel sizes kept). Shell list rows at 1280x720 scaled 1.5. | MINOR | Expand hit rects beyond the art on touch (e.g. ±10 px), or a swipe on the cameo strip to scroll. Taller hit rows for shell lists on Android. |
| W4-AM-09 | Double-tap select-by-type rarely triggers with a finger. | A slop of about 24 dp for touch. | `kDblSlopO = 6` logical px, about 5 dp at the phone canvas. | `input_original.cpp:29-31` | MINOR | Use a larger slop (and `kDragThresholdO`) when `ev.touch`. |
| W4-AM-10 | **The phone package has no install instructions, and the picker refuses `Download` itself.** | One page: "copy the RA2 folder to Download/RA2 → Import → open RA2 → Use this folder → Allow". | `05_picker_download_cant_use.jpg`. `D:\RA2-phone` has no README. | SAF policy on Android 11+. `SetupActivity` gives no hint (`strings.xml setup_how`). | MINOR | Put the folder rule in `setup_how` and `INSTALL.md`. Pass `DocumentsContract.EXTRA_INITIAL_URI` for `Download/RA2`. Recurse one level so picking the parent of the RA2 folder also works (where the picker allows it). |
| W4-AM-11 | No touch path to the mission briefing (F1) or the radar drag. | Every keyboard affordance is reachable by touch. | The campaign options menu has no Briefing entry (`31_back_menu.jpg`). The radar is tap-only. | `touch_bar.inc` (no briefing). `game_scene_controls.inc:411-421` (PointerDown only). | MINOR | Add a `BRF` touch-bar button in campaign. Drag on the radar pans continuously. |
| W4-AM-12 | Builds cannot be told apart. | The version identifies the commit. | Both the user's APK and mine report `versionCode 776`, `versionName 0.9.0`. The `+g<hash>` is only in the logcat banner (`Red Alert 2 engine 0.9.0+g4647b17`). | `app/build.gradle` versionName from `VERSION` only. | MINOR | Append `+g<hash>` to `versionName` and draw `RA2_VERSION` on the main menu. |
| W4-AM-13 | A mid-game panel resize (foldables, split-screen) keeps the old canvas, pillarboxed. | Re-layout on resize. | `50_209_ingame.jpg` | The canvas is fixed at GameScene build (`campaign_scenes.inc:134`, `kWinW/kWinH` static). | MINOR | Handle `SDL_WINDOWEVENT_SIZE_CHANGED`: recompute `game_logical_size`, then recompute `lay_` and the playfield. |
| W4-AM-14 | ALL01 Dreadnoughts never fire (no V3 rockets). | Dreadnoughts bombard Manhattan in the opening. | `15_after_pan.jpg`, `25` (scratch): idle for about 6 minutes. | Not Android-specific (gameplay/projectiles). See `03-all01-opening.md` and `04-projectiles.md`. | (owned elsewhere) | — |
| W4-AM-15 | OK, for the record. | — | Importer copy of 747 MB in about 29 s. Tap select and order. Two-finger pan. Long-press cancel. Team long-press create and tap recall. DPLY. Cameo build. Two-tap placement. Back to options, then Abort. 0 SDL assertions. A single audio device with 0 underruns. Load about 6 s. The touch-to-logical transform is correct at 16:9 and 20:9. | — | OK | — |

## 7. Prioritized fix list

1. **W4-AM-01: make one-finger drag scroll** (BLOCKER on phones). This is the likely root of "can't
   control". Changes go in `touch_gestures.cpp` (emit `Pan` for a one-finger drag), with band select as
   long-press-drag or a touch-bar `BOX` toggle. Also: never clear the selection on an empty touch band box,
   and add a first-mission gesture hint.
2. **W4-AM-03: visible, escapable cursor modes on touch.** Add a mode banner plus a cancel button, and draw
   the mode cursor at the last tap. Changes go in `main.cpp:5961` and `touch_bar.inc`.
3. **W4-AM-02 + W4-AM-05: what the phone can see.** Centre the opening on the player's force
   (`main.cpp:6565`). Move or collapse the touch bar so it does not cover the bottom of the playfield.
   Consider a slightly larger logical height on phones.
4. **W4-AM-04: side-keyed sidebar textures.** A one-line key prefix or a cache clear. It also fixes desktop
   sessions that switch sides.
5. **W4-AM-06 / -07: touch-only polish.** No hover tooltips without a mouse. A long-press progress ring.
6. **W4-AM-08 / -09 / -11: hit-target expansion.** Tabs, strip arrows, list rows, double-tap slop, a
   briefing button, radar drag.
7. **W4-AM-10 / -12 / -13: packaging and lifecycle.** A README in the phone package and better setup text
   (with `EXTRA_INITIAL_URI`). The commit hash in `versionName`. Re-layout on resize.
8. **Re-measure fps on a real arm64 phone.** The emulator gives 15–20 fps under SwiftShader, a floor only.

## 8. Repro scripts

- Single-finger gestures (landscape screen px on a 1920x1080 panel):
  - `adb shell input tap X Y`;
  - `adb shell input swipe x0 y0 x1 y1 600` (this one band-boxes, W4-AM-01).
- Two-finger pan: `adb root`, then `bash 06-evidence/kplay.sh "down:0:21:700:400 down:1:22:900:400" 0.05
  "move:0:700:440 move:1:900:440" 0.03 … "up:0 up:1" 0.4`. The ops are written to `/dev/input/event2`.
  Check which device is in direct mode first with `dumpsys input`.
- The DocumentsUI picker needs kernel taps: `bash 06-evidence/ktap.sh X Y`, with portrait coordinates.
- FPS: `dumpsys SurfaceFlinger --latency "SurfaceView - org.ra2.engine/org.ra2.engine.RA2Activity#0"`
  over 5 s.
