# Build and install this source update

Open the **android** directory in Android Studio, keeping the parent ra2-engine directory intact. The native engine is in ../engine and libraries in ../src; opening only android/app is insufficient.

The saved **RA2 Phone** run/debug configuration launches the default activity and builds before launch. The project-local Gradle JDK is set to the installed JDK 17 through .gradle/config.properties and .idea/gradle.xml. Debug signing reuses this installation's configured test keystore so it can update test15 without deleting app data. Missing keystore.properties falls back to normal Android debug signing. No signing passwords are stored in the run configuration.

1. Let Gradle sync. Use Gradle JDK 17. This project uses Android SDK 35, NDK 27.2.12479018 and CMake 3.22.1. Its existing local.properties points to the downloaded SDK in build-tools/android-sdk; on another PC, select that PC's SDK instead.
2. Select the app module and debug build variant. Native debug builds are optimized (RelWithDebInfo).
3. Enable Developer options and USB debugging on the phone. Connect USB, approve the phone's debugging prompt and choose the phone in Studio.
4. Press Run. The current project build is 900020 (arm64-v8a); it can update earlier local builds without uninstalling.
5. Retain your installed app/data. If Studio reports a signing-key mismatch, use the same existing Android debug keystore used for test15; do not uninstall just to clear the error without preserving saves/settings first.
6. Import or retain the user's retail assets through the app. They are read at runtime; do not add MIX archives to the APK.

The local SDL source copy is automatically detected so Studio does not download it into a long Windows build path. SDK packages may still need installation on a new machine. Do not move only the android directory away from the engine source.

## Source update after test15

### Build 900020 unload and presentation fixes

UnloadingClass voxel assets now use shared ownership and atomically published scene lookups, preventing a replaced scene from leaving stale raw pointers. Tank bunker drawing uses the bunker cell/layer and composes its configured SpecialAnim front/back layers around the occupant, with the SpecialAnimThree/Four transition reversed when it exits. Slave Miner undeploy shows a mirrored YAREFNMK pack animation as a render-only effect; simulation still packs and routes immediately, so the original timed state machine is not claimed.

Real rules/art checks were run against `D:\Games\Command.&.Conquer.Red.Alert.II\game`: HARV→HORV, CMIN→CMON, YAREFN buildup/idle art and NATBNK special-animation entries. 1179 native regression cases passed (7 excluded, including the optional legacy ecache patch-cache fixture absent from this install). Android debug APK build succeeded. The new visuals and the Chrono Miner crash fix still need a full device regression on repeated scene changes, a complete collect/warp/unload cycle, bunker enter/exit, and Slave Miner relocation.

### Build 900019 shared ground-vehicle slope orientation

Ground voxel vehicles now apply terrain orientation after facing/HVA transforms. The common rasterizer transforms bounds and voxels together, and inversely transforms the lighting direction. Hull, turret, barrel, alternate turret and no-spawn hull caches distinguish ramp type. TurretOffset and muzzle FLH positions follow the same rotation. Infantry sprites remain upright; airborne, hovering, naval and bridge-deck units are excluded from ground tilt. This covers factions through the shared renderer rather than per-unit overrides. The first 32 tilted sprite cache misses add TERRAIN_TILT report records.

The orientation convention is independently implemented with rigid rotations, using the public TS/RA2 slope numbering and angle reference in OpenRA MapGrid.cs: https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Game/Map/MapGrid.cs . No OpenRA implementation is copied. This is not a claim of exact original binary matrix parity: transitions currently switch at cell boundaries, saddle ramps 17..20 keep an upright average, and shadows retain the existing ground projection. Animation, facing and house colors remain part of the cache keys. First encounters with new ramp/facing combinations still rasterize on demand and can cause a small first-use cost. No simulation/save format changes or retail assets are included.

Native slope/raster checks: eight cases, 154 assertions passed (one existing retail fixture returned early). Tests cover opposite slopes, rigid length/inverse rotation, flat-render equivalence and terrain rotation after facing. Reinstall from Android Studio, confirm version_code=900019, then drive a Lasher/Rhino/Grizzly up and down the same ramp, stop halfway and rotate turret independently. Share a report and a view of the slope if direction or alignment is wrong. Device visual verification remains pending.

### Build 900018 firing follow-up

Presentation observes unit FireEvents as well as the legacy flash edge, so rapid consecutive shots that stamp identical fire_flash values still create effects. Non-IFV visual weapon selection now uses the combat weapon resolver, with elite fallback, and numbered Gatling anti-air selection/FLH follows the active slot. Base-defense eight-direction muzzle animation lists now follow turret facing instead of always using the first animation. Building FLH positioning and Gatling heat stages are still pending. These changes do not add missing art or synthesize visible bullets for invisible projectile types.

Read the full user-supplied engine database as a behavior checklist. It is not authoritative configuration: numerous IDs differ from the imported rules (e.g. Gatling Tank YTNK, defense YAGGUN). Terrain audit confirms ground voxel rendering currently lacks the original ramp matrix; broken tile pixels and occlusion are separate issues requiring a reproducible view. No terrain fix is claimed in 900018. Targeted existing combat/projectile/projection regression: 45 cases, 8090 assertions passed with the local retail reference path configured. These tests do not validate the SDL presentation visually. Phone testing remains required.

### Build 900017 menu follow-up (supersedes the older layout limitations below)

The original large YR shell now applies through the shared ShellXform/ShellPainter to main-menu pages, options and resolution controls, skirmish, multiplayer pages, load/save and shell dialogs. Existing 640x480 control templates adapt to the 800x600 composition: the content panel grows to 632x568, the sidebar keeps its native width and the bottom Back/Exit control moves to the final row. Mouse/touch coordinates use the matching inverse transform. Classic assets retain the previous layout fallback.

The shared metallic font palette now treats glyph index 1 as the bright face, 2 as the upper bevel and 3 as the dark shadow. This corrects the previous dark, inverted lettering on menu buttons and submenu controls. No retail assets are bundled or modified.

Files for this menu follow-up: engine/main.cpp, engine/shell_layout.hpp, engine/shell_pages.inc, engine/skirmish_page.inc, engine/mp_pages.inc, tests/test_projection.cpp and this document. Native regression: 1178 cases, 154260 assertions passed; six retail-dependent cases excluded. Android main translation-unit syntax check passed with one existing missing-override warning. The updated menu follow-up still requires installation and visual/touch checks on the phone; the earlier 900017 screenshot predates these shared submenu/font changes. Press Run again in Android Studio using the android project. Check main menu, Options, resolution chooser, skirmish, load/save, confirmation dialogs and Back at two resolution presets. These are adapted templates, not a claim of pixel-perfect parity for every original dialog.

- Deployed Slave Miners placed through building production can pack and seek distant ore when no ore remains within eight cells of the foundation. Relocation checks run every 150 simulation frames, staggered by building ID. Loaded workers finish banking cargo first. If no distant reachable ore is found or packing is blocked, the building stays put. Nearby ore on inaccessible terrain can still prevent relocation; this is a known remaining limitation.
- Garrison ownership transfers from any noncombatant/passive owner, rather than only the first neutral-house ID. This fixes one reproducible reason a civilian building remains outside player control after entry. The report itself does not prove the exact cause of the user's pink pixels.
- Entry and evacuation now log GARRISON events, and periodic reports include occupied civilian buildings as well as bunkers/miners.
- Prefer the user's original langmd.mix ra2ts_l.bik (then ra2ts_s.bik) over the classic language.mix menu movie when the expansion movie parses. Missing/invalid movies retain classic fallback. No retail art bundled.
- Enable main-menu Options > Set Game Resolution on Android. Auto (touch) restores density/aspect adaptation; fixed presets apply a saved logical canvas to the main/options menu, skirmish lobby and new games. Rendering/input use the same transform. Other ancillary shell pages retain their prior canvas. In-match switching is not provided here.

Changing the logical resolution is not the same as changing Android's physical framebuffer or adding an offscreen low-resolution renderer. It is not a verified GPU/thermal optimization. Menu artwork still uses the old 640x480 composition; selecting an 800x600 output canvas does not recreate the original YR layout coordinates.

## Validation

1172 native cases, 154223 assertions passed; six retail-rules cases explicitly excluded because the original installation is not available. Some other retail-dependent cases return early. Android main translation unit syntax checked; one existing missing-override warning. No new APK built for this update, no Studio installation/device test performed here.

Test the new source first in a fresh Yuri skirmish: place a Slave Miner far from ore, wait at least ten simulation seconds, verify packing/travel/deployment and workers; test a civilian building with infantry, select it and press UNLOAD. Select 800x600 and 1280x720 in main Options, return to skirmish and verify taps align. Share a report immediately after any failure.

## Menu research

YR shares much of the red shell artwork; a red backdrop alone is not evidence of an incorrect menu. The original Westwood manual states menus always appear at 800x600. This port still composes its shell at 640x480 before scaling. A modder's first-hand instructions identify langmd.mix/ra2ts_l.bik as the expansion movie and explain that overriding it with the RA2 version restores the RA2 menu title.

https://oldgamesdownload.com/manual/command-conquer-yuri-s-revenge-windows-manual-english/
https://xhp.xwis.net/p/13836
https://ppmforums.com/topic-42316/red-alert-2-yuris-revenge-red-alert-2-mod/

Exact changes after test15: android/gradle.properties, android/app/jni/CMakeLists.txt, engine/world.hpp, engine/world_yuri.cpp, engine/world_mcv.cpp, engine/world_depth.cpp, engine/report_probe.hpp, engine/main.cpp, engine/app_robust.inc, engine/skirmish_page.inc, tests/test_yuri_core.cpp and this document. Changes are local and uncommitted; GitHub has not been updated.
