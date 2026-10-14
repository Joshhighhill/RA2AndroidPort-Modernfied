# Wave-2 gap analysis 06: Android, robustness, release readiness

This is a read-only audit of master `dfff02a` (2026-10-07), built in the worktree
`D:\ra2-wt\w2-android` (branch `w2-android`, nothing committed). Wave-1 items G-70, G-71
and G-72 (touch layer, synthetic-mouse duplicates, Android file list) were re-verified on
a running emulator and are not re-reported.

Every finding is backed by one of three kinds of evidence:

- a live emulator run (screenshots in `06-evidence/`, logcat quoted inline);
- a desktop run of the real game through `--play`, `--ai-soak` or a fuzzed save;
- static code reading, with file:line references.

Scratch scripts and outputs are under `D:\ra2-wt\w2-android\_scratch\`. That folder is
not committed.

## 1. Environment found

| Item | Found / used |
|---|---|
| Physical device | **None.** `adb devices` was empty. |
| Emulator | **Yes.** AVD `ra2x86`: Android 11 (API 30), x86_64, WHPX-accelerated, `-gpu swiftshader_indirect`, run headless with 3 GB RAM, 1080x1920 at 420 dpi (landscape 1920x1080). AVD `ra2test` (arm64 image) exists but cannot run on an x86 host. |
| APK | `gradlew assembleDebug` succeeded in 5 m 53 s. Inputs: JDK 21.0.12, AGP 8.5.2 / Gradle 8.9, NDK r27c, SDL 2.30.9 sources taken from the main checkout through `RA2_SDL2_SRC_DIR`. Output `app-debug.apk` is 6.56 MB and contains arm64-v8a and x86_64 `libmain.so` / `libSDL2.so` / `libc++_shared.so`. The Java build produced 3 warnings (`source/target 8 obsolete`). |
| Desktop build | `build/` in the worktree, configured like the main build (MinGW-w64 GCC from WinLibs, Ninja, empty `CMAKE_BUILD_TYPE`), with `-Wall -Wextra` added only to count warnings. The `ra2engine` build took 566 s at `-j4`. |
| Sanitizers | MinGW GCC has no ASan on Windows and there is no host clang. I built a **UBSan trap-mode** engine and test binary in `build-ubsan/` (`-O1 -g -fsanitize=undefined -fsanitize-undefined-trap-on-error -fno-sanitize=vptr`). It was clean on every run (§3.4). |
| Game data | `D:\Red Alert 2`. The 11 files the README lists (~745 MB) were pushed to the emulator. |

The heavy jobs ran one at a time: desktop build, Gradle, missing-file matrix, emulator
session, 60-minute soak, save/load cycles, fuzz, UBSan build, UBSan runs.

## 2. What was exercised on the emulator

The emulator session followed this flow:

- first launch with no files;
- push the files and launch;
- Main menu, then Single Player, then Back;
- Skirmish lobby, editing the name with the soft keyboard;
- Start;
- in game: select the MCV and deploy it, build a Power Plant and place it, drag a band box, tap to move;
- Home, then resume;
- Back to open the in-game menu, then save (keyboard plus IME Enter), then load;
- Abort, then Exit, then relaunch.

**What worked:**

- Tap and drag gestures.
- Back as Esc.
- Text entry (SDL_TEXTINPUT).
- Save and load.
- Pause and resume, with textures intact.
- Relaunch in the same process.

**What failed or degraded** is covered by W2-AR-01 to W2-AR-22.

| Evidence | What it shows |
|---|---|
| `emu_nofiles` (logcat) | `fatal: cannot open file: .../ra2md.mix`, then `Finished main function`, then the activity is destroyed about 1 s after launch. Nothing appears on screen. |
| `emu_menu.png` | The main menu after about 40 s. The status bar and navigation bar stay visible (SDL reports `Window size: 1794x1017` on a 1920x1080 device). |
| `emu_name.png` | The soft keyboard covers the lower half of the lobby. |
| `emu_poll.png` | **SDL assertion** `aaudio_OpenDevice ... (audioDevice == NULL) \|\| iscapture` on Start Game, about 9 minutes after the tap. |
| `emu_deploy.png`, `emu_placed2.png`, `emu_box.png` | Touch select, deploy, place and band box all work. |
| `emu_save.png` | The keyboard hides the save description field and the SAVE/BACK buttons. |
| `emu_poll2.png` | The same AAudio assertion fires again (counter "2 times") when a save is loaded. |
| `emu_relaunch.png` | **SDL assertion** `aaudio_CloseDevice ... audioDevice == this` on Exit Game. |
| SurfaceFlinger latency | 38 frames in 4.95 s, i.e. **7.5 fps** in game. This was a `-O0` build on SwiftShader, so it is not representative of a phone. It is listed only as a floor. |
| `dumpsys` / `top` | RSS in a skirmish was 0.75 to 0.92 GB, 1.3 to 1.4 GB while loading a save, and 157 MB back at the menu. |

## 3. Desktop robustness runs

### 3.1 Missing-file matrix

Each run removed one file, used hard-linked partial install directories, and drove the real
shell with `skirm.play` or `camp.play`:

| Missing | Skirmish Start | Allied campaign Start |
|---|---|---|
| none | PASS | PASS |
| `ra2md.mix` (or any of the files read at boot) | process exits before the window opens (`fatal: cannot open file`) | same |
| `language.mix` | **process dies on clicking Start** (exception, exit 3) | **dies on Start** |
| `Maps01.mix` | PASS | **dies on Start** |
| `multimd.mix` | "PASS", but the lobby shows `maps=0` and the game silently starts on the default *campaign* map `sov07tmd` (`mapsmd03.mix`) | PASS |
| `theme.mix`, `langmd.mix`, `expand01.mix`, `ra2.csf`, `mapsmd03.mix` | PASS (degrades silently) | PASS |

### 3.2 Soak and cycle runs

| Run | Result |
|---|---|
| `--ai-soak --minutes 60 --ai-vs-ai --diff brutal` (54,000 frames) | No crash, exit 0. 341 s wall. Working set flat at **514 MB** from minute 1 to minute 60. Anomalies: AI credits frozen at 367 and `ai_units_built` frozen at 25 from minute 18 to 60; `teams_live` grows monotonically 4 to 26, all with 0 members (see W2-AR-27). |
| `saveload25.play`: 25 save, then load, then resume cycles | PASS, 0 failures. The working set oscillates 570 to **1,250 MB** (the peak occurs during each load) with no upward drift. |
| `sessions12.play`: 12 rounds of lobby, game, Abort, menu | PASS. Working set 390 to 760 MB with no drift. No leak across sessions. |
| 8-player Brutal skirmish (`p8b.play`, `xmp27t8`) | PASS. Peak 681 MB. Throughput is low (§3.5, W2-AR-29). |

### 3.3 Corrupt and edge inputs

| Input | Result |
|---|---|
| Save truncated at 50% | Refused with "COULD NOT LOAD THAT SAVED GAME"; stays in the dialog. Good. |
| Empty save, or valid magic followed by garbage | Not listed in the Load dialog (0 rows). Silent. |
| 300 flipped body bytes, or 40 flipped header-area bytes | `[saveload] load FAILED`: a "LOAD FAILED" banner, then a *fresh game of the same scenario* starts. The player is not returned to the dialog. |
| **20 saves with 1 to 4 random bytes changed** (`fuzz/`, loaded through the real dialog, then 900 frames) | No crash in 20/20. **18/20 report `load ok`** and play on with silently altered state. There is no checksum. |
| Empty install directory | Exits with `cannot open file ...ra2md.mix` on stderr only. |
| Corrupt or extreme `RA2.INI` | Handled (§3.5). |

### 3.4 UBSan (trap mode, `build-ubsan/`)

| Run | Result |
|---|---|
| `ra2assets_tests` (full suite, real assets) | **775/775 pass, 350,157 assertions, no trap**. 27 s. |
| `--play` scripts `shell-smoke`, `sk-a`, `sk-s`, `m-all01`, `controls`, `world-depth` | All PASS with 0 failures and exit 0 (no trap). |
| `--ai-soak --minutes 20 --ai-vs-ai --diff brutal` | Exit 0, no trap. |
| The 20 fuzzed saves from §3.3, loaded and then 900 frames | 20/20 exit 0, no trap (18 load `ok`, 2 `FAILED`). |

UBSan finds no undefined behaviour on these paths. The silent acceptance of corrupt saves
(W2-AR-20) is therefore a data-integrity problem, not a memory-safety one, at least for
the 20 samples. ASan was not available. Running the APK under NDK ASan (`wrap.sh`) is a
recommended follow-up.

### 3.5 Eight players, corrupt RA2.INI, SDL assertions

| Run | Result |
|---|---|
| **8 players**: `p8b.play`, `xmp27t8` with 7 Brutal AIs, 9,000 frames (10 game minutes) | PASS, exit 0, peak working set **681 MB**. **Throughput: 4,500 frames in 272 s (16.5 logic frames/s), then 4,500 in 190 s (23.7/s)**, headless with sparse rendering on the `-O0` desktop build. The 2-player soak runs at about 158 frames/s. The 8-player simulation is roughly 7 to 10 times more expensive and **below real time** at the faster GameSpeeds, which need 20 to 60 logic frames/s (`gamespeed.hpp:12-16`). See W2-AR-29. |
| `RA2.INI` with 20 KB of random bytes | Treated as empty, so defaults are used, and the file is rewritten on save. PASS. |
| `RA2.INI` with extreme values (`ScrollRate=999`, `ScreenWidth=100000`, `GameSpeed=-99`, `VoiceVolume=-5`, `ScoreVolume=nan`, `[Skirmish] ScenIndex=99999 UnitCount=-4 Credits=999999999 GameSpeed=42`) | PASS: lobby, game and 600 frames. Values are read unclamped (`scroll_rate=999 voice=-5 score=nan`) but clamped at use (`settings.cpp:325,333,339`), and the lobby clamps speed to 6 and units to 0. `ScenIndex` is range-checked (`skirmish_page.inc:206`). `volume_gain_permille` feeds `NaN` to `std::lround` (`settings.cpp:338`), which is implementation-defined (0 on arm64, `LONG_MIN` then clamped to 0 on x86). Harmless, but worth a `std::isfinite` guard. |
| `RA2.INI` of 3 MB of `[` characters | PASS. |
| **Unrelated hang** during the first binary-INI run | The process blocked for over 400 s in **SDL's assertion handler**: `Assertion failure at WIN_AddDisplay (SDL_windowsmodes.c:380) 'index == *display_index'`. It fired on a display-change event and waited on the console for Abort/Retry/Ignore. It did not reproduce. See W2-AR-30. |

## 4. Gap table

Status values: **BROKEN**, **WRONG**, **MISSING**, **PARTIAL**, **OK** (verified, listed for the record).
Severity values: **BLOCKER**, **MAJOR**, **MINOR**.
Effort values: S (≤ 1 day), M (2 to 4 days), L (≥ 1 week).

| ID | Issue | Expected (original / Android norm) | Clone (file:line / logcat / evidence) | Status | Severity | Fix sketch | Effort | Depends on |
|---|---|---|---|---|---|---|---|---|
| W2-AR-01 | **Two SDL audio devices are open at once.** The menu's device stays open for the whole `run_menu`, and `GameScene` opens a second one. SDL's AAudio backend supports only one playback device. | One output device for the life of the process (the original has one DirectSound device). | Menu device: `engine/main.cpp:6075` (`ma->sys.out.open()`, `MenuArtStore` lives in `run_menu` `:7116`). Game device: `main.cpp:693` via `:4861`. The debug APK raises an SDL assertion dialog at `SDL_aaudio.c:81` on **every game start and every load** (`emu_poll.png`, `emu_poll2.png`), and `SDL_aaudio.c:195 audioDevice == this` on Exit (`emu_relaunch.png`). In a release SDL (asserts off) the backend's single `audioDevice` global is overwritten, so behaviour is undefined (game audio is lost or closes the wrong stream). | BROKEN | BLOCKER (Android) | Make one `AudioOutput` owned by `run_menu` (or a process singleton) and let scenes swap the `Mixer` it pumps. Minimal version: close the menu device before `GameScene` opens its own and reopen it when returning to the shell. | S–M | — |
| W2-AR-02 | **Starting or loading a game takes minutes on Android, and the screen freezes with no feedback.** | The original shows a loading screen with a progress bar. A mobile game should start in roughly 10 to 30 s. | Emulator: Start Game took about **9 min** and Load about **10 min**. The last lobby or dialog frame stays on screen the whole time and nothing pumps events (`GameScene` is built synchronously inside `scene->next()`, `main.cpp:4310-4313`). Contributors: (a) the native code is `-O0` (W2-AR-13); (b) MIX files are read **whole into RAM, repeatedly**: 35 `read_file(dir+"ra2.mix"/"ra2md.mix")` sites in `main.cpp`, against 281 MB and 204 MB files on Android 11's FUSE-backed `/storage/emulated`; (c) the voxel facing caches are built at load. The desktop takes about 15 to 20 s at `-O0`. | BROKEN | BLOCKER (Android) | (1) Build native code optimized (W2-AR-13). (2) Open each top-level MIX once per process, either cached in a shared `MixFile` or `mmap`-ed, instead of `read_file` per loader. (3) Add a real loading scene that pumps events and draws progress (`LOADING` art), or construct `GameScene` in steps across frames. Measure on a real arm64 phone. | M | W2-AR-13 |
| W2-AR-03 | **First run without game files: the app vanishes in about 1 s.** | Show "game files not found at <path>, copy these files…" and stay open. Ideally the app also creates the folder. | `main.cpp:16115-16124` calls `run_menu`, which throws, which is caught at `:16290` and only `SDL_LogError`-ed. Logcat: `fatal: cannot open file: /storage/emulated/0/Android/data/org.ra2.engine/files/ra2/ra2md.mix` then `Finished main function` then `onDestroy`. Nothing appears on screen. The `ra2/` sub-folder is never created. | MISSING | MAJOR | At startup, check all required files and list the missing ones with `SDL_ShowSimpleMessageBox` (it works on Android) or a minimal in-engine error page. `mkdir` `files/ra2/` so it appears over MTP. | S | — |
| W2-AR-04 | **The documented way to install assets does not work on Android 11+.** | Users must be able to supply their own files: an in-app import via the Storage Access Framework (SAF), or a documented MTP flow. | README:2969-2980 says to push as the non-root shell user. On API 30, `adb shell mkdir -p /sdcard/Android/data/org.ra2.engine/files/ra2` returns **Permission denied** (as does `ls`), because `Android/data` is protected from shell and file-manager apps on 11+ and fully blocked on 13+. Only MTP from a PC, or root plus a manual `chown`/`chcon` (what this audit had to do), works. A stale, wrongly labelled `storage_file` copy from July was still on the AVD. | BROKEN | MAJOR | Add an "Import game files" flow: `ACTION_OPEN_DOCUMENT_TREE` on the user's RA2 folder (or a `.zip`), copy the 11 files into `getExternalFilesDir()/ra2` or internal storage, and verify sizes. Document MTP as the fallback. Fix the README. | M | W2-AR-03 |
| W2-AR-05 | **A missing data file in the middle of a session kills the process.** The `multimd.mix` case falls back silently to the wrong map. | The original refuses at startup (its MIX check), or shows an error and returns to the menu. | §3.1: without `language.mix`, Skirmish/Campaign Start throws and the process exits (`--play` exit 3). Without `Maps01.mix`, the Allied campaign Start does the same. Without `multimd.mix`, the lobby has `maps=0 max=8` and the game launches on the default `mapsmd03` campaign map. Every `GameScene`/campaign constructor exception propagates up to `main()` (`main.cpp:16288-16296`). | BROKEN | MAJOR | Run a single preflight (W2-AR-03) of every file the shell can reach. Wrap scene construction in `run_scene_driver` (`main.cpp:4310`) in try/catch, return to the main menu, and show the message there. | S | W2-AR-03 |
| W2-AR-06 | **Not fullscreen on Android, and letterboxed to 16:9.** | Immersive fullscreen. Use the whole 19.5:9 to 20:9 panel; the original supports any resolution, with the tactical view growing and the sidebar anchored right. | `platform.cpp:28-32` creates the window `SDL_WINDOW_SHOWN`. SDL only enters immersive mode for fullscreen windows (`SDLActivity.java:776-785`). Logcat: `Window size: 1794x1017` on 1920x1080, with the status and navigation bars visible (`emu_menu.png`). `SDL_RenderSetLogicalSize(1280,720)` (`platform.cpp:66`) then pillarboxes inside that, which wastes about 25% of a 20:9 phone. | WRONG | MAJOR | On `__ANDROID__`, create the window with `SDL_WINDOW_FULLSCREEN_DESKTOP` and set `layoutInDisplayCutoutMode=shortEdges`. Pick a logical width from the display aspect (e.g. `720*w/h`) and let the tactical view widen, as the original does for widescreen `ScreenWidth`. | S (fullscreen) / M (aspect) | — |
| W2-AR-07 | **Touch targets are far below Android's minimum.** | Android guideline: 48 dp targets. | At 1280x720 logical on a 6" 1080p phone (scale 1.40, 420 dpi), measured on screenshots: a cameo is about 80x60 px, i.e. **30x23 dp**; sidebar tabs and the repair/sell buttons are about 40 px (**15 dp**); infantry sprites are about 3 mm. Selecting a single GI by tap is unreliable. | PARTIAL | MAJOR | Add a touch UI scale: a larger sidebar or cameo strip when the last input was touch. Pick units by nearest-within-radius (e.g. 24 dp) instead of exact hit. Make tabs taller. | M | W2-AR-06 |
| W2-AR-08 | **Commands that only exist as hotkeys cannot be reached by touch.** | Every action needs an on-screen affordance. | The G-70 gesture layer maps tap, drag, long-press and pan only (`touch_gestures.hpp:10-16`). Unreachable without a keyboard: team create/select (Ctrl+n, n, `game_scene_controls.inc:151-154`), Shift add-to-selection (only the band box adds), Stop, Guard, Scatter, force-fire (Ctrl), force-move (Alt), waypoints, View bookmarks (`:66`), and F1 briefing in campaigns. Double-tap does select-all-of-type. | MISSING | MAJOR | Add a collapsible touch command bar: modifier toggles (add / force-fire / force-move), Stop / Guard / Deploy, 5 group slots (tap = select, long-press = assign), and a Briefing button in campaigns. | M | — |
| W2-AR-09 | **No autosave or state persistence when backgrounded.** Lifecycle events are ignored. | Mobile norm: save on `onPause`/`WILLENTERBACKGROUND`, offer to resume after the process dies, and pause the game on return. | `grep SDL_APP_ engine/` returns nothing. `Platform::poll` drops `SDL_APP_WILLENTERBACKGROUND/LOWMEMORY/TERMINATING` (`platform.cpp:254`). SDL blocks the thread while paused, and the game **resumes instantly, unpaused** (`emu_resume.png`). With RSS 0.75 to 1.4 GB, the low-memory killer will kill backgrounded games and the session is lost. `am send-trim-memory RUNNING_CRITICAL` produced no reaction. | MISSING | MAJOR | On `SDL_APP_WILLENTERBACKGROUND`, write `Saved Games/AUTOSAVE.SAV` (the P7 path already exists) and open the in-game options menu, so the game resumes paused. At startup, offer "Resume" when an autosave is newer than the last exit. On `SDL_APP_LOWMEMORY`, drop texture caches. | M | — |
| W2-AR-10 | **Peak memory doubles while loading.** The old `GameScene` stays alive while the new one is built. | Tear down, then build. A mid-range phone app should stay well under 1 GB. | Desktop `saveload25`: 570 MB then **1,250 MB** on each load. Emulator: 0.9 GB then **1.4 GB**. `load_game` (`main.cpp:5458-5467`) does `new GameScene(...)` while `this` is alive, and the driver only destroys the old scene after `next()` (`:4310-4313`). The whole MIX files sit in `std::vector` (W2-AR-02b). | WRONG | MAJOR | Change the driver protocol so a scene requests a transition and the driver destroys the current scene before constructing the next (pass a factory, not an instance). `mmap` the MIX files. | M | — |
| W2-AR-11 | **The release blockers for Google Play are not met.** | Since Nov 2025, apps targeting Android 15+ must ship 16 KB-page-aligned `.so` files. The required `targetSdk` is 35. A release build needs signing. | `llvm-readelf -l libmain.so` shows `LOAD ... align 0x1000` (4 KB) for every lib (NDK r27c default), so the libs fail to load on 16 KB-page devices. `targetSdk 34` (`app/build.gradle:39`). No `signingConfig`. `versionCode 1` / `versionName "1.0"` are duplicated in gradle (`:40-41`) and the manifest (`AndroidManifest.xml:6-7`). | MISSING | MAJOR (release) | Pass `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` (r27) or move to NDK r28+, and verify with `llvm-readelf` that alignment is `0x4000`. Set `targetSdk 35` (this forces edge-to-edge, which goes with W2-AR-06). Add a release `signingConfig` from `keystore.properties`. Keep a single version source. | S | — |
| W2-AR-12 | **APK identity and packaging are placeholders.** | A proper app name and icon, and a no-assets notice. | `strings.xml:2` sets `app_name` to "RA2 Engine". The icons are SDL's default `ic_launcher.png`. `applicationId org.ra2.engine`. The manifest comments are still the SDL template's ("Replace com.test.game…", `AndroidManifest.xml:2-4`). There is an unused `VIBRATE` permission (`:52`). `allowBackup="true"` includes saves of about 0.7 MB each in auto-backup (25 MB cap). The orientation is locked to `landscape` (`:76`), so the device cannot be flipped 180°. | PARTIAL | MINOR | Rename and add an icon. Use `sensorLandscape`. Drop `VIBRATE`. Add `android:appCategory="game"`. Decide the backup rules (`fullBackupContent`/`dataExtractionRules`). | S | — |
| W2-AR-13 | **Native code is unoptimized.** The debug APK is `-O0` with SDL asserts on; the desktop default build is `-O0` too. | The shipped binaries should be optimized. | The APK's `compile_commands.json` has no `-O` flag (`-g -fno-limit-debug-info` only) and `CMAKE_BUILD_TYPE=Debug`. The README only documents `assembleDebug` (README:2899-2911). Desktop: `CMakeLists.txt` sets no default `CMAKE_BUILD_TYPE`, and the main `build/CMakeCache.txt` has it empty, i.e. no `-O`. The emulator measured 7.5 fps in game. | WRONG | MAJOR | Add `arguments "-DCMAKE_BUILD_TYPE=Release"` (or `RelWithDebInfo`) to the jni CMake for both variants, and set `ndk.debugSymbolLevel`. Default the desktop `CMAKE_BUILD_TYPE` to `RelWithDebInfo`. **Re-verify every determinism gate at `-O2`** (`-ffp-contract` matters on arm64 clang, see Q3). | S (+ gate re-run) | — |
| W2-AR-14 | Back on the root main menu does nothing. | Android norm: Back at the root leaves the app (with a confirmation in a game). | `KEYCODE_BACK` maps to `SDLK_AC_BACK`, which becomes Esc (`platform.cpp:120,156`). The main menu ignores Esc (G-01 deliberately keeps it alive), so the user is stuck and must use Home or the Exit button. Back elsewhere works (page Back, in-game menu). | WRONG | MINOR | On `PAGE_MAIN`, map Esc from AC_BACK only to the existing "ARE YOU SURE YOU WANT TO EXIT" dialog. | S | — |
| W2-AR-15 | **The soft keyboard covers the field being edited and the dialog buttons.** | The edited field and its confirm button stay visible (`adjustPan`, or `SDL_SetTextInputRect`). | `SDL_StartTextInput()` is called with no `SDL_SetTextInputRect` (`main.cpp:4406`, `skirmish_page.inc:440`). In the Save dialog the description edit and SAVE/BACK are hidden (`emu_save.png`). In the lobby the Start button is hidden. IME Enter does save, so the flow is not blocked. Typing appends to the prefilled "Skirmish" (saved as "SKIRMISHEMUSAVE1"). | PARTIAL | MINOR | Call `SDL_SetTextInputRect` with the edit rectangle. While text input is active, draw the dialog in the top half. Select the prefilled text on focus. | S | — |
| W2-AR-16 | Visual leftovers of touch input. | A clean touch UI. | (a) After a tap the software mouse cursor is drawn at the tap point (`emu_game.png`, `emu_placed.png`). (b) A green 1-px focus outline frames the surface after the IME closes (`emu_kbback.png`, Android default focus highlight). (c) Placement has no hover preview, so the first tap both previews and tries, which makes placing trial and error (`emu_placed.png`, red footprint). | PARTIAL | MINOR | Hide the cursor while `!mouse_present_` (already tracked, `game_scene_play.inc:17`). Set `android:defaultFocusHighlightEnabled=false` in the theme. For touch placement: first tap previews, a second tap on the same footprint confirms. | S | — |
| W2-AR-17 | The process lives on after Exit, and the next launch re-enters `main()` with stale statics. | SDL's README: call `exit()` at the end of `main` on Android, or reset all globals. | The pid was the same before Exit, after Exit and after relaunch (2339). The relaunch worked, but globals such as `g_art_chain`, `g_sidebar_mix` (`main.cpp:131,136`) and function-local statics persist. Wave-1's `run_interactive` comment in `RA2Activity.java:9-14` is stale. | PARTIAL | MINOR | On Android, `exit(rc)` after `run_menu` returns. Update the Java comment. | S | — |
| W2-AR-18 | A gesture cancelled by the system becomes a click. | A cancelled touch should do nothing. | `SDLSurface.java:278-289` converts `ACTION_CANCEL` into `ACTION_UP`, so `TouchGestures::finger_up` in the Pending state emits a full tap (`touch_gestures.cpp:84-88`). A system back-swipe or notification pull can therefore deselect units or issue an order. | WRONG | MINOR | Patch `SDLSurface` to send a distinct cancel (e.g. pressure −1) and make `TouchGestures` drop the gesture on it. | S | — |
| W2-AR-19 | Android audio: about 200 ms queued latency and no audio-focus handling. | Low latency (≤ 100 ms), and respect audio focus (calls, other apps). | `audio_sdl.hpp:15` (`kTargetQueuedMs = 200`), and the queue is only pumped from the main loop, which also stops during long loads (W2-AR-02). | PARTIAL | MINOR | Use 60 to 100 ms on Android, or switch to an SDL callback device fed by the mixer. Pause on `SDL_APP_WILLENTERBACKGROUND`. | S | W2-AR-01 |
| W2-AR-20 | **Saves have no integrity check.** Corrupted saves load as valid. | Detect corruption and refuse. (The original's saves are compressed streams that fail loudly when truncated or corrupt.) | `savegame.cpp:75-108` checks magic, version and length framing only. In §3.3, **18 of 20** saves with 1 to 4 random bytes changed load `ok` and run with silently altered state. Bounds come only from the length framing (`savegame.hpp` `io_count`). Indices inside `load_world` are not validated, so a flipped index can become an out-of-range access (see UBSan, §3.5). | MISSING | MAJOR | Add a CRC32 per section (format v9), refused with the existing "COULD NOT LOAD" message. Range-check every index in `load_world` (unit/building/target/team ids). | S–M | — |
| W2-AR-21 | Load failures are handled inconsistently. | The original shows an error and stays in the dialog. | Empty or garbage saves are silently left out of the list. A body that fails to decode **starts a fresh game of the same scenario** with a 2.5 s "LOAD FAILED" banner (`main.cpp:5440-5453`), and the player's current game is gone. | WRONG | MINOR | List unreadable files as "(damaged)". On a body failure, return to the dialog with a message instead of starting a fresh game. | S | W2-AR-20 |
| W2-AR-22 | Desktop: fatal errors only reach a console, and the install directory only comes from an env var. | The player must be able to launch by double-click. The original reads its install path from the registry and shows message boxes. | The exe is the console subsystem (`objdump`: `Subsystem 3 (Windows CUI)`), and `fatal:` goes to stderr only (`main.cpp:16290-16295`). `env_install_dir()` uses `RA2_INSTALL_DIR` or the hard-coded `D:\Red Alert 2` (`main.cpp:117-120`). There is no CLI flag, no registry lookup (`HKLM\SOFTWARE\Westwood\Red Alert 2\InstallPath`), no lookup next to the exe, and no folder picker. There is no `--help` or `--version`. | MISSING | MAJOR (release) | Search, in order: CLI `--data <dir>`, env var, the exe directory, the registry, `RA2.INI` `[Engine] DataDir`. On failure show `SDL_ShowSimpleMessageBox` listing the missing files. Ship a `WIN32_EXECUTABLE` (GUI) build for release. Add `--version` and `--help`. | S | W2-AR-03 |
| W2-AR-23 | **No version, license or notices.** | Release hygiene: a version string, a LICENSE, third-party notices, and a disclaimer that the game is user-supplied and not affiliated with EA. | No `LICENSE` or `NOTICE` in the repo (`git ls-files`). There are no notices for SDL2 (zlib), doctest (MIT) or stb (PD/MIT). The README only says "your own legally-owned game files" (`README:2383`) and "never bundled (they belong to EA)". It has no trademark or affiliation disclaimer, while the app shows EA's logo art from the user's files. The menu shows the original art's "v1.006". The engine has no version anywhere (window title "RA2 Engine", APK 1.0). | MISSING | MAJOR (release) | Add `LICENSE` and `THIRD_PARTY_NOTICES.md`. Add a README "Legal" section (trademarks, user-supplied data only, no assets distributed). Inject `git describe` into a `RA2_VERSION` define and show it in the window title, the logcat banner and `--version`. | S | — |
| W2-AR-24 | **The README is inaccurate for installing and running, and has no player quick start.** | A short, correct "install and run" for both desktop and Android. | 3,330 lines of development log. The intro's controls are stale: "right-click ground to move… Esc/close quits" (`README:2410-2416`) contradicts the original scheme in use (`:3115+`). The Android section has four errors: (1) it says "arm64-v8a only" (`:2911`) while the APK ships x86_64 too; (2) its `local.properties` example uses `ndk.dir` (`:2896`), which `app/build.gradle:11-14` explicitly says causes a `[CXX1100]` conflict (the key is `ra2.ndkPath`); (3) the non-root push fails (W2-AR-04); (4) it never says where RA2.INI and saves live on Android (`/data/data/org.ra2.engine/files/`, reachable only through `run-as`). | WRONG | MAJOR (release) | Add a `docs/INSTALL.md` (or a README top section) covering desktop install and run, the Android install (MTP or import), controls (mouse and touch) and the data-file list, plus troubleshooting. Fix the four Android errors. Move the dev log to `docs/`. | S | W2-AR-04 |
| W2-AR-25 | Build warnings are not enabled and dead code is left over. | `-Wall -Wextra` clean, with warnings tracked. | The project sets no warning flags (`CMakeLists.txt`). With `-Wall -Wextra` the engine produces **16 warnings** (plus 24 in vendored stb): unused `run_interactive` (`main.cpp:5885`), `blit_pcx`/`blit_pcx_scaled` (`:3599,:3614`), `camp::side_of` (`campaign_runtime.inc:118`), unused parameters, `-Wsubobject-linkage` (`controls_selftest.inc:13,17`), `-Wmissing-field-initializers` (`main.cpp:16015`, `platform.cpp:69`), `-Wrange-loop-construct` (`pres_map.inc:236`). Java: `source/target 8 obsolete`. | PARTIAL | MINOR | Add `-Wall -Wextra` to the project, mark third-party includes as `SYSTEM`, delete the dead code, and set `compileOptions` to Java 17. | S | — |
| W2-AR-26 | The texture cache is never evicted, uses one GL texture per sprite frame, and is never rebuilt after GL context loss. | Bounded GPU memory. Survive `SDL_RENDER_DEVICE_RESET` on Android. | `texture_cache.cpp:22-61`: an `unordered_map<string, SDL_Texture*>` that lives for the whole `run_menu` (`main.cpp:7113`) with no `clear()`/LRU, and one texture per frame, i.e. a draw call and texture bind per sprite. `SDL_RENDER_DEVICE_RESET`/`TARGETS_RESET` are not handled (grep), so on devices that lose the EGL context on background every cached texture is invalid. The emulator kept its context; no growth was seen in the 12 desktop sessions (software renderer). | PARTIAL | MINOR (MAJOR on low-end devices) | Add `TextureCache::clear()` on scene change, on `SDL_APP_LOWMEMORY` and on `SDL_RENDER_DEVICE_RESET` (entries rebuild lazily). Longer term: atlas the SHP frames per palette. | S / L (atlas) | — |
| W2-AR-27 | Long-game growth: entity vectors never shrink, and empty AI teams pile up. | Counts stay bounded over hour-long games. | `units_` is append-only (`world.cpp:548`, no erase), so every per-tick loop scans every unit ever built. In the 60-minute soak, `teams_live` climbs 4 to 26 with `members=0` and is never disbanded. The economy freezes at 367 credits from minute 18 (cross-ref to the AI/economy owners). Memory stayed flat at 514 MB because only 25 units were built. | PARTIAL | MINOR | Reuse dead slots through a free list with a generation counter (keeps handles stable). Disband teams with 0 members after their script finishes. Hand the economy stall to the AI and economy agents. | M | — |
| W2-AR-28 | Robustness confirmed (for the record). | — | No crash or leak in: 60-min brutal AI-vs-AI soak (flat 514 MB); 25 save/load cycles; 12 session cycles; 20 fuzzed saves; truncated, empty and garbage saves; corrupt and extreme `RA2.INI`; an 8-player Brutal game; `HOME` and resume; relaunch in the same process. **UBSan is clean** across the 775 tests, 6 play scripts, a 20-min soak and 20 fuzzed saves (§3.4). No `assert(` in `engine/` or `src/`. | OK | — | — | — | — |
| W2-AR-29 | **8-player simulation throughput.** A 7-AI Brutal game manages only about 16 to 24 logic frames/s (desktop, `-O0`, headless), which is about 7 to 10 times the cost of a 2-player game. | Real time at the user's GameSpeed: 15 logic frames/s at speed 4 and up to 60 at speed 0 (`gamespeed.hpp:12-16`), with headroom for rendering, on a phone. | §3.5: 4,500 frames in 272 s, then 4,500 in 190 s. Excess time is dropped beyond `kMaxLogicSteps` (`main.cpp:5081`), so on slower hardware the game simply runs slow. Likely contributors: append-only entity vectors scanned every tick (W2-AR-27), per-house AI, and A* pathfinding. None of this has been profiled. | PARTIAL | MAJOR (Android) | First build optimized (W2-AR-13) and re-measure. Then profile an 8-player game (`perf`, or Very Sleepy on Windows), cap AI or pathfinding work per tick, and reuse dead slots. Add a `--ai-soak --players 8` option so this becomes a tracked benchmark. | M | W2-AR-13, W2-AR-27 |
| W2-AR-30 | **SDL is built with assertions on, and the default handler blocks.** An SDL-internal assert hangs the game (console prompt on desktop) or pops a dialog (Android). | Release builds must not stop on library assertions. | Desktop: `SDL_windowsmodes.c:380` asserted on a display change and the process waited forever on stdin for Abort/Retry/Ignore (§3.5). Android: the AAudio asserts in W2-AR-01 show modal dialogs. Both builds compile SDL without `NDEBUG` (desktop `CMAKE_BUILD_TYPE` empty; Android `Debug`). The engine installs no `SDL_SetAssertionHandler`. | WRONG | MAJOR (release) | Build SDL Release (`SDL_ASSERTIONS=release`/level 1) for shipping. Install an assertion handler that logs and returns `SDL_ASSERTION_IGNORE` in release. Keep asserts on in dev builds so W2-AR-01-type bugs stay visible. | S | W2-AR-13 |

## 5. Prioritized fix plan

1. **Audio device ownership (W2-AR-01).** BLOCKER on Android: every game start and load trips SDL. Hotspots are `main.cpp:529-533` (`AudioSystem`), `:693`, `:6075`, `:7116`, and `audio_sdl.cpp`.
2. **Optimized native builds plus a loading screen (W2-AR-13, W2-AR-02).** Make Release/RelWithDebInfo the default in `android/app/build.gradle` (`externalNativeBuild.cmake.arguments`) and the top-level `CMakeLists.txt`. Re-run the determinism gates at `-O2` on x86-64 and arm64. Add a loading scene in `run_scene_driver` (`main.cpp:4285-4322`). **Gate:** a real arm64 phone reaches the game in under 30 s.
3. **Shared MIX handles and tear-down-before-build (W2-AR-02b, W2-AR-10).** Hotspots: the 35 `read_file(dir + "ra2*.mix")` sites in `main.cpp`, `ra2assets::MixFile` (`include/ra2assets/mix.hpp:18`), and the scene driver transition protocol. **Gate:** peak RSS under 800 MB through `saveload25`.
4. **First-run and missing-file handling (W2-AR-03, -05, -22).** One `preflight_data_files(dir)` call used by desktop and Android, a message box, scene construction wrapped in try/catch, and desktop data-dir discovery. Hotspots: `env_install_dir` (`main.cpp:96-121`), `main()` (`:16112-16296`), `run_scene_driver`.
5. **Android asset import (W2-AR-04).** A Java `ImportActivity` using SAF, which copies into `getExternalFilesDir("ra2")`. Hotspots: `RA2Activity.java` and the manifest.
6. **Fullscreen and aspect (W2-AR-06), then touch ergonomics (W2-AR-07, -08, -16, -15, -14).** Hotspots: `platform.cpp:28-66`, sidebar layout (`sidebar.*`, `draw_sidebar`), `touch_gestures.*`, `game_scene_controls.inc`, `SaveLoadDialog` (`main.cpp:4386+`), `skirmish_page.inc:440`.
7. **Lifecycle (W2-AR-09, -26, -17).** Handle `SDL_APP_*` and `SDL_RENDER_DEVICE_RESET` in `Platform::poll` (`platform.cpp:169-259`) through new `InputEvent` kinds. Autosave from `GameScene`. Call `exit()` on Android.
8. **Save integrity (W2-AR-20, -21).** Hotspots: `savegame.cpp` encode/decode (per-section CRC, format v9), index checks in `load_world`, and `GameScene::apply_loaded` (`main.cpp:5418-5457`).
9. **Release packaging (W2-AR-11, -12, -23, -24, -25).** 16 KB alignment, targetSdk 35, signing, identity, LICENSE/NOTICE/version, an INSTALL doc, warnings.
10. **Long-game hygiene and 8-player performance (W2-AR-27, W2-AR-29).** Slot reuse and team disbanding (coordinated with the AI owner). Profile an 8-player Brutal game after step 2 and add it as a tracked benchmark.
11. **SDL assertions in release (W2-AR-30).** Do this together with step 2: Release SDL plus a non-blocking `SDL_SetAssertionHandler`.

## 6. Open questions

- **Q1 (data set).** The engine reads Yuri's Revenge data: `rulesmd.ini` (`main.cpp:7110`), `ra2md.mix`, `langmd.mix`, the `multimd.mix` maps with `missionsmd.pkt` (`maplist.cpp:62-68`), and `mapsmd03.mix`. The target is RA2 Allied/Soviet only. Should the shipping data list be RA2-only (`MULTI.MIX` + `missions.pkt`, `rules.ini`)? That would also cut the Android payload (currently about 745 MB) by about 290 MB. This question is for the owners of fidelity.
- **Q2 (real-device performance).** The 9-minute load and 7.5 fps are emulator numbers from a `-O0` build on SwiftShader with FUSE storage. They need re-measuring on a real arm64 phone with an optimized build before sizing the work in W2-AR-02, -07 and -26.
- **Q3 (determinism across builds).** Do the byte-identity gates and save portability survive `-O2` and arm64 clang's default `-ffp-contract=on` (FMA)? Saves are probably meant to move between desktop and Android. If floating-point paths feed `world_hash`, add `-ffp-contract=off` to both builds.
- **Q4 (auto-pause on resume).** The original's single-player behaviour on focus loss has not been traced. The Android recommendation (open the options menu on resume) is a platform norm, not a fidelity claim.
- **Q5 (shipping channel).** Is the APK meant for Play Store distribution? If so, W2-AR-11 is a hard blocker. If it is side-loaded only, it is MINOR, except for 16 KB alignment on Android 15+ devices.

## 7. Fix status (branch `w2fix-android`)

These fixes were verified on the `ra2x86` emulator (Android 11, x86_64,
SwiftShader, 3 GB, 4 cores) and on the desktop. Evidence is in
`06-fix-evidence/`: `emu/*.png` are emulator screenshots; `desk_*` are
desktop captures made through the `RA2_AUTOSHOT_DIR` hook. The `[load]`,
`[audio]` and `[lifecycle]` lines below are quoted from logcat.

**Emulator load time (Start Game → first game frame, skirmish 1v1):**

| Build | Time |
|---|---|
| Audit, old master, `-O0` debug APK | about 9 min (Load: about 10 min) |
| Master with W2-GA-P5 file-backed MIX, `-O0` (`-Pra2.nativeBuildType=Debug`) | 30.5 s |
| This branch, debug APK at RelWithDebInfo `-O2` (the new default) | **4.5 s** |
| This branch, load a save | 2.3–2.6 s |

The menu now appears about 2 s after launch.

**Memory (emulator):** in-game RSS was 411 MB, against 0.75–0.92 GB in the
audit. Loading a save from inside a game peaked at **405 MB**, sampled every
0.2 s, against 1.4 GB in the audit. The old game is freed first: the minimum
during the load was 259 MB.

| ID | Status | What changed / evidence |
|---|---|---|
| W2-AR-01 | **FIXED** | `AudioOutput` is now a refcounted handle on ONE process-wide SDL device (`audio_sdl.cpp`); whichever scene is on screen supplies the mixer. This keeps W2-GA-23's stereo callback mixer and the streamed music. Logcat: `menu handle ok: sdl_devices=1 handles=1`, then `game handle ok: sdl_devices=1 handles=2`. There were 0 SDL assertions across game start, two loads and Exit, on a debug APK built with `SDL_ASSERTIONS=enabled`. |
| W2-AR-02 | **FIXED** | The native code is optimised in both APK types (see the timings above). `DeferredScene` + `build_deferred` (`app_robust.inc`) draw a loading screen with 49 `load_progress()` checkpoints (`emu/loading_saved.png`, `desk_loading/*.png`). A campaign keeps its LS800 screen as the backdrop (`desk_loading/loading_campaign_mid.png`). The build still runs on the main thread: true async loading is not needed at 2–5 s. |
| W2-AR-03 | **FIXED** | Desktop: a first-run **GAME FILES NEEDED** page (`desk_nofiles/message_page.png`) lists the required and optional files and the folder searched, with RETRY / BROWSE / EXIT. Android: the `SetupActivity` launcher lists the missing files (`emu/setup_firstrun.png`) and creates `files/ra2/`. The native page also has IMPORT. |
| W2-AR-04 | **FIXED (code), PARTIAL (evidence)** | `SetupActivity` imports through SAF (`ACTION_OPEN_DOCUMENT_TREE`): it matches names case-insensitively, copies to `.part` files, checks sizes and renames. The picker opens from the app (`emu/saf_picker.png`). On API 30, shell writes to `Android/data` fail as the audit said (`touch: ... Permission denied`); the README and INSTALL now say to push to `Download/RA2` and import. Selecting the folder could not be driven headless: DocumentsUI ignores injected `input tap` on this AVD, although keys reach it. The copy path still needs a manual run on a device. |
| W2-AR-05 | **FIXED** | A scene build that throws becomes an error page that names the missing file and returns to the main menu (`desk_error_missing_maps01.png`, `scripts/play/robust-missing.play` PASS). `multimd.mix` is now required, so no more silent fallback to the campaign map. |
| W2-AR-06 | **FIXED** | The Android window is `FULLSCREEN_DESKTOP` (immersive; logcat shows `Window size: 1920x1080`, against 1794x1017 in the audit; `emu/menu.png`). The cutout mode is `shortEdges`. The game canvas width follows the panel aspect (16:9 up to 21:9). The shell stays 1280x720, centred. The in-game pages centre on the current canvas (`ui_xform`). |
| W2-AR-07 | **FIXED** | Touch canvas: on Android the game canvas height is chosen so that 1 px ≈ 0.85 dp (480 on a 1080p 420 dpi phone), so cameos are ~53x42 dp and the sidebar buttons ~1.5x bigger (`emu/game_deployed.png`). The desktop stays 720. |
| W2-AR-08 | **FIXED** | A touch command bar (`touch_bar.inc`): +SEL / FIRE / MOVE modifier toggles, STOP / GRD / SCAT / DPLY, and teams 1–5 (tap selects, long-press creates). It calls the same handlers as the hotkeys. `emu/game_touchbar.png`; DPLY deployed the MCV (`emu/game_deployed.png`). Briefing (F1) and the View bookmarks remain keyboard-only. |
| W2-AR-09 | **FIXED** | `SDL_APP_WILLENTERBACKGROUND` opens the options menu (paused) and writes `AUTOSAVE.SAV`: `[lifecycle] background: paused, autosave ok (20 ms)`. After `am kill`, the next launch offers **RESUME GAME?** (`emu/resume_prompt.png`), which loads the save (`[lifecycle] autosave found (valid): resumed`). A normal end of the game deletes the autosave. |
| W2-AR-10 | **FIXED** | Teardown before build (see Memory above). The `saveload25` desktop soak was not re-run. |
| W2-AR-11 | **FIXED** | `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`: every `LOAD` segment is `align 0x4000` (`release_apk_check.txt`). compileSdk/targetSdk are 35. Release signing comes from `keystore.properties`, falling back to the debug key. versionName/versionCode come from `VERSION` + the commit count; the manifest no longer repeats them. |
| W2-AR-12 | **PARTIAL** | `sensorLandscape`; no VIBRATE; `appCategory=game`; backup rules exclude the game files; the SDL template comments are gone. The name is still "RA2 Engine" and the icon is still SDL's: a trademark-safe name and icon are a decision for the owner. |
| W2-AR-13 | **FIXED** | Debug APK = RelWithDebInfo, release = Release (`-O3`). The desktop default RelWithDebInfo came with W2-GA-P2. `-ffp-contract=off` is set on both builds (Q3). |
| W2-AR-14 | **FIXED** | Android Back on the root menu opens the existing Exit confirm (`emu/back_exit_confirm.png`). Esc stays inert there. |
| W2-AR-15 | **PARTIAL** | `SDL_SetTextInputRect` is set on the save-description and lobby-name edits, with `adjustPan`. The IME now pans the edit into view (`emu/save_dialog_ime.png`), and IME Enter saves. The SAVE button itself is above the visible area while typing. Selecting the prefilled text on focus is not done. |
| W2-AR-16 | **FIXED** (a, b, c) | (a) No software cursor is drawn unless a real mouse is present. (b) `defaultFocusHighlightEnabled=false`. (c) Placement by touch: the first tap previews the footprint, a second tap on the same cell places. |
| W2-AR-17 | **FIXED** | `main()` returns. `RA2Activity.onDestroy()` then calls `killProcess` once SDL has torn down. An `exit()` from the SDL thread SIGSEGV'd on the emulator at quit; that was found and replaced in this branch. |
| W2-AR-18 | **FIXED** | `SDLSurface` marks `ACTION_CANCEL` with pressure −1, and `TouchGestures::finger_cancel` drops the gesture (unit test `touch: a system-cancelled gesture never clicks`). |
| W2-AR-19 | **FIXED** (with W2-GA-23) | A callback device with a ~93 ms buffer, not fed from the main loop. SDL pauses AAudio in the background. Audio focus is not requested. |
| W2-AR-20 | **FIXED** | Save **v11** = v10 (W2-AN) plus a CRC-32 trailer over the whole file. The unit test flips every bit (`0x01`/`0x80`) of every byte after the version and refuses all of them; truncation and extra bytes are refused too. The `--demo-saveload` hash is unchanged (`8291b85b9957b34a`). Range checks on indices in `load_world` are not added; the CRC makes them unreachable for random corruption. |
| W2-AR-21 | **FIXED** | `list_saves` lists unreadable or CRC-failing `*.SAV` files as **Damaged**. They cannot be loaded. |
| W2-AR-22 | **FIXED** | `--data`, `RA2_INSTALL_DIR`, `RA2.INI [Engine] DataDir`, the exe folder, the registry `InstallPath`, then `D:\Red Alert 2`. The BROWSE folder picker writes `DataDir`. Fatal errors also show a message box. `--version` and `--help` were added. The exe is still the console subsystem (`--play` needs stdout); `os_attach_parent_console()` is ready for a GUI build. |
| W2-AR-23 | **FIXED** | `LICENSE` (MIT for the engine code; it grants nothing over the game data), `THIRD_PARTY_NOTICES.md` (SDL2 zlib, stb MIT, doctest MIT, libc++), a README "Legal" block, and the `RA2_VERSION` string (`0.9.0+g<hash>`) in the window title, the logcat banner and `--version`. The license choice should be confirmed by the owner. |
| W2-AR-24 | **FIXED** | `docs/INSTALL.md` (players). The README Android section is corrected: `ra2.ndkPath`, both ABIs, the import flow, where RA2.INI and saves live, and the build types. |
| W2-AR-25 | **FIXED** | `-Wall -Wextra` on the engine with **0 warnings** (24 before, counting the new code); stb is included as a SYSTEM include. Java is set to 17. |
| W2-AR-26 | **FIXED** | `TextureCache::clear()` on `SDL_RENDER_DEVICE_RESET` / `TARGETS_RESET` (takes a new `instance_id` so the terrain list rebuilds), and `trim()` on `SDL_APP_LOWMEMORY`. The LRU budget came with W2-GA-P5. |
| W2-AR-27 | FIXED on w2fix-skirmish (see 03-skirmish-match.md §7.4) | 60-min AI-vs-AI soak: teams_live peaks at 11 and ends at 0; credits and production move all hour. Entity slots stay append-only by design (stable indices); dead slots release their storage. |
| W2-AR-28 | OK | — |
| W2-AR-29 | IMPROVED on w2fix-skirmish (see 03-skirmish-match.md §7.4) | 7-AI Brutal xmp27t8, -O0 headless: about 290-400 logic frames/s (was 16.5-23.7). Not yet re-measured on a phone. |
| W2-AR-30 | **FIXED** | `SDL_SetAssertionHandler` logs `SDL ASSERTION (ignored)` and returns `ALWAYS_IGNORE`, so it never prompts. `SDL_ASSERTIONS` is `release` in optimised builds and in the Android release APK, and `enabled` in Debug and the Android debug APK. |

### Q1 decision (for the owner, not changed here)

The engine loads the **Yuri's Revenge** data set: `rulesmd.ini`,
`artmd.ini`, `soundmd.ini` and `evamd.ini` from `ra2md.mix`; the YR
`multimd.mix` skirmish maps with `missionsmd.pkt`; `langmd.mix` map names;
and `mapsmd03.mix`. It also loads the RA2 base archives `ra2.mix`,
`language.mix` and the campaign `Maps01.mix` / `MAPS02.MIX` / `expand01.mix`.

So an RA2-only install (no `ra2md.mix` / `multimd.mix`) **cannot start**. The
first-run page lists those two files as required.

Switching to the RA2-only list (`rules.ini`, `art.ini`, `sound.ini`,
`multi.mix` + `missions.pkt`) would:

- drop about 290 MB from the Android payload;
- change every rules-driven gate;
- need the YR-only units and buildings removed from the rosters.

It is a fidelity and owner decision. The data list in `engine/data_dir.inc`
(`kDataFiles`) and `docs/INSTALL.md` is the single place to change.
