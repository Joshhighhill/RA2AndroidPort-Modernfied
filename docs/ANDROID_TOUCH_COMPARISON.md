# Android source comparison and touch implementation

Inspected 21 October 2026. This is a source audit and a tested gesture-layer
change, not a claim that either full game or an APK was built here.

**Follow-up:** both checkouts now have a local
`codex/generals-touch-unification` branch. The web implementation and expanded
validation are recorded in the follow-up section at the end of this report.
The comparison below describes the pinned upstream snapshots and initial
native pass, not the subsequent web changes.

## Source snapshots and result

| Repository | Inspected revision | Role |
|---|---|---|
| [omarelarab/ra2-engine](https://github.com/omarelarab/ra2-engine/tree/a8cfdccfef8592c13a6517c3fb76d834b1534027) | `a8cfdccfef8592c13a6517c3fb76d834b1534027` | Selected implementation base |
| [Mustafa-MS/RedAlert2-Android](https://github.com/Mustafa-MS/RedAlert2-Android/tree/b332c3f78ef4cff46ce4f5b6a93703b970beb3a2) | `b332c3f78ef4cff46ce4f5b6a93703b970beb3a2` | Initial comparison; follow-up implementation now available |
| [MYSOREZ/GeneralsZH-Android-Port](https://github.com/MYSOREZ/GeneralsZH-Android-Port/tree/e707c1dafe597cfb92e75f942ae320741d59e047) | `e707c1dafe597cfb92e75f942ae320741d59e047` | Control-design reference, not copied code |

The exact MYSOREZ project is **GeneralsZH-Android-Port**, a fork in the
GeneralsX lineage, rather than a Red Alert engine. Source inspection is a
better basis than calling every mobile RTS gesture “Generals-style.”

Use Omar's engine for a native Android direction and adapt interaction
semantics rather than merging engines. Mustafa's WebView engine is an
alternative implementation, not a UI layer that can be dropped onto Omar's
simulation. Neither source audit establishes which is more faithful or faster
on a real phone. Their README gameplay claims were not independently tested.

## File-level comparison

Paths below are relative to their respective repositories. The source links
above pin the full trees so these findings remain reviewable.

| Area | Omar | Mustafa | Consequence |
|---|---|---|---|
| Android entry | `android/app/src/main/java/org/ra2/engine/SetupActivity.java`, `RA2Activity.java`, `org/libsdl/app/SDLActivity.java` | `android/app/src/main/java/com/ammaar/ra2android/MainActivity.kt` | Omar launches SDL and native libraries after import. Mustafa configures an immersive WebView, JS bootstrap, logging, power/thermal notifications and renderer-loss recovery. |
| Build | `android/app/build.gradle`, `android/app/jni/CMakeLists.txt`, `cmake/ra2_sources.cmake` | `android/app/build.gradle.kts`, `scripts/build-android.ps1`, `redalert2/package.json` | Omar compiles C++17/SDL2 with NDK r27c; minSdk 21. Mustafa packages a web bundle plus game resources, minSdk 24, JDK 17/Kotlin. These are build configuration facts, not compatibility test results. |
| Rendering | `engine/platform.cpp`, `engine/iso_renderer.cpp`, `engine/iso_project.cpp`, `engine/facing_cache.cpp` | `redalert2/src/engine/gfx/Renderer.ts`, `TextureAtlas.ts` | Omar uses SDL accelerated 2D rendering with software fallback, isometric projection and cached voxel-facing sprites. Mustafa uses Three.js WebGLRenderer, texture atlases and browser graphics lifecycle. Sharing a renderer is not practical. |
| Assets | `src/asset_fs.cpp`, `engine/asset_search.inc`, `engine/data_dir.inc`, `SetupActivity.java` | `AssetSchemeHandler.kt`, `redalert2/src/shell/nativeSeed.ts`, `redalert2/src/engine/gameRes/GameResImporter.ts`, `scripts/prepare-gameres.ts` | Omar reads original/nested MIX archives in explicit patch/theater order. Android folder picking copies supported files into app storage. Mustafa serves APK resources from a virtual HTTPS domain and seeds OPFS, with build-time extraction/transcoding. |
| Gesture recognition | `engine/touch_gestures.hpp/.cpp`, `engine/platform.cpp` | `redalert2/src/gui/PointerEvents.ts` | Omar has a pure C++ state machine, logical pixel slop scaled for Android, timestamped events and SDL synthetic mouse filtering. Mustafa uses DOM touch listeners, a 50 ms initial down buffer, tracked single-finger ID, synthetic mouse events and wheel steps. |
| Gesture meaning | `engine/input_original.cpp`, `engine/touch_ui.inc`, `engine/touch_bar.inc` | `redalert2/src/gui/screen/game/worldInteraction/WorldInteraction.ts`, `CameraPanHandler.ts`, `MapScrollHandler.ts` | Omar already maps one-finger drag to pan and hold-drag to selection, with UI/radar routing. Mustafa maps one-finger drag to left-drag/selection and two fingers to right-drag/pinch, so matching the common scheme needs more than changing a label. |
| Touch buttons | `engine/touch_bar.inc`, `engine/touch_ui.inc` | `redalert2/src/gui/MobileTouchControls.ts` | Omar has CMD, BOX, additive selection, force fire/move, stop, guard, scatter, deploy, ten teams over two pages, views and cancellation. Mustafa's L/R selector file has no callers found by source search; changing it alone would not change active gestures. |

Mustafa's `nativeSeed.ts` skips existing files by **size**, not a content hash;
same-size changes can remain stale. The APK and OPFS can hold duplicate copies.
Its asset handler returns CORS/cache/MIME headers but no COOP/COEP headers;
a virtual HTTPS origin alone does not establish SharedArrayBuffer isolation.
Neither observation was tested in Android WebView here.

Mustafa's cancellation handling deserves care: `WorldInteraction` explicitly
suppresses orders from a cancelled mouseup, but `PointerEvents.onMouseEvent`
also dispatches separate `click` notifications after mouseup without checking
the cancelled flag. Any rewrite must audit UI click consumers as well as world
orders. This is a source-level risk, not a reproduced device bug.

## Exact Generals reference

The current implementation lives in
[`Core/GameEngineDevice/Source/SDL3GameEngine.cpp`](https://github.com/MYSOREZ/GeneralsZH-Android-Port/blob/e707c1dafe597cfb92e75f942ae320741d59e047/Core/GameEngineDevice/Source/SDL3GameEngine.cpp)
and
[`Core/GameEngineDevice/Source/SDL3Device/GameClient/TouchInput.cpp`](https://github.com/MYSOREZ/GeneralsZH-Android-Port/blob/e707c1dafe597cfb92e75f942ae320741d59e047/Core/GameEngineDevice/Source/SDL3Device/GameClient/TouchInput.cpp).
Some upstream documentation still names older `GeneralsMD/...` paths; the
tree was checked to locate the actual files under `Core/...`.

The gesture source separates **250 ms selection hold** from **600 ms stationary
long press**, with a 16 px drag dead zone and 350 ms/40 px double-tap checks.
It pans and zooms through camera operations; battlefield taps use engine picking
and context evaluation in `TouchInput`, with UI ownership checked first.
Own-unit selection yields to meaningful interactions such as entering a transport
or repairing. Double tap selects matching units; cancel/deselect has its own path.
Armed abilities support aiming and cancellation. UI taps remain ordinary UI clicks.
Pending building placement and radar have special handling, so the README's
gesture table is not a complete specification of every context.

See the project's
[control documentation](https://github.com/MYSOREZ/GeneralsZH-Android-Port/blob/e707c1dafe597cfb92e75f942ae320741d59e047/docs/port/TOUCH_CONTROLS.md)
and [README](https://github.com/MYSOREZ/GeneralsZH-Android-Port/blob/e707c1dafe597cfb92e75f942ae320741d59e047/README.md).
This work independently implements small gesture rules; it does not import
Generals engine code or assets.

## Unified control contract and implementation status

| Interaction | Common target | Omar branch now | Mustafa follow-up |
|---|---|---|---|
| Tap | Select or use the engine's context action | Existing behavior retained | Preserve engine default-action evaluation; normalize touch independently of optional right-click-move settings |
| Double tap | Select same type on screen | Existing behavior retained | Existing world double-click detection needs real touch validation |
| One-finger drag | Pan after movement slop | Existing behavior retained | Classify world gestures before starting left-drag; keep UI/list/minimap ownership separate |
| Hold then drag | Box selection after 250 ms | **Implemented shorter independent selection threshold** | Implement equivalent classifier; do not convert UI drags into camera motion |
| Stationary hold | Cancel armed mode; optional deselect when idle | Retains 450 ms cancel-only behavior, plus cameo tooltips/team assignment | Replace current long-press force-attack meaning only through an explicit touch policy; current world handler uses a 500 ms hold as force attack |
| Two-finger tap | Cancel mode, otherwise deselect | Existing behavior, **pan upgrade no longer becomes cancel**; a stationary hold can now upgrade | Make cancellation independent of right-click-move/scroll preferences; suppress UI click fall-through |
| Two-finger drag | Pan without a command on release | Existing behavior, protected after one-finger pan | Existing synthetic right-drag; remaining-finger inert state already present |
| Pinch | Bounded zoom about the gesture anchor | **Not implemented**; no gameplay zoom event/camera scale path found | Existing stepped wheel zoom (40 px pinch steps); continuous anchor zoom requires camera changes |
| UI/production | UI owns touch; hold gives explanation/actions | Existing routing retained | Preserve raycast and DOM UI ownership; audit cancelled clicks |
| Command column | Visible groups, add selection, force fire/move, stop/deploy | Existing column retained | Bind an actual HUD to existing keyboard/world command APIs; the unused L/R module is insufficient |
| Building/ability targeting | Preview under finger, explicit commit/cancel | Existing RA2 placement/confirmation retained | Keep existing mode semantics until preview/commit is tested; no Generals rotation copied into fixed-footprint RA2 buildings |

The native branch is a **partial implementation of the unified contract**,
not a complete reproduction of Generals X controls. The initial pass changed
only this native engine; the follow-up below adds web changes. Keeping Omar's
cancel-only stationary hold avoids silently changing
production holds and existing selection behavior. Zoom, continuous aiming,
new army-category buttons and inertial camera motion remain future work.

The original recommendation for a later web implementation was to introduce a pure gesture classifier with
tap/pan/box/cancel/zoom intents, then adapt those intents to WorldInteraction's
existing handlers. First resolve UI ownership, then gesture intent, then engine
context actions. Test normal and right-click-move settings, minimap boundaries,
placement, production buttons and third-finger transitions. Share the contract
and event-trace tests across languages, rather than engine code. The follow-up
instead adapts the existing world interaction handlers, which keeps UI gesture
ownership separate without replacing the entire DOM input layer.

## Modified files

- `engine/touch_gestures.hpp`: adds configurable `selection_hold_ms = 250`,
  documents the split thresholds, tracks whether a two-finger tap is eligible.
- `engine/touch_gestures.cpp`: classifies holds using incoming event timestamps
  as well as idle ticks; starts box selection at the shorter threshold; allows
  a second finger during a stationary hold; preserves consumed-pan status when
  the finger count changes, including the three-finger diagnostic path.
- `engine/platform.cpp`: corrects an obsolete gesture comment; no SDL routing
  or rendering code was changed.
- `tests/test_touch_gestures.cpp`: six new cases for short hold, threshold
  boundary, queued events, pan upgrade, opposing motion and hold upgrade.
- `docs/INSTALL.md`: updates the player control table and timing explanation.
- This report: comparison, decisions, limitations and reproducible validation.

## Assets and practical APK feasibility

Both paths require user-supplied game data; an APK wrapper cannot reconstruct
the art, maps, sounds or rules. Omar's `engine/data_dir.inc` requires
`ra2.mix`, `ra2md.mix`, `language.mix`, `multimd.mix`; its importer supports
additional patches, text, music and movies. Mustafa's preparation script copies
`ra2.mix`, `language.mix`, `multi.mix`, extracts English strings, unconditionally
opens `theme.mix` for music conversion, then handles optional YR additions.
Consequently the pipelines do not accept an interchangeable minimal file set.

Omar's source declares BSD-0-Clause. Mustafa declares GPL-3.0, but its own
`THIRD-PARTY-NOTICES.md` explicitly records unresolved engine provenance and
an unlicensed bot dependency. Those are the repository's disclosures, not a
rights audit performed here. Do not merge source on the assumption that both
projects have the same provenance. No retail assets were imported or packaged.

## Validation actually performed

The local branch is `codex/generals-touch-unification` in `work/ra2-engine`.
The synced `sources/` directory was untouched. At the end of the initial pass,
Mustafa's checkout was unchanged.

The new regression suite was first compiled against the original gesture
implementation: 13 total cases, 3 failures (queued hold, pan upgrade, hold
upgrade), establishing that those tests detect existing behavior. Two more
cases were then added for the new independent selection threshold.

Final focused compile/run from the native checkout:

```powershell
& C:/msys64/ucrt64/bin/g++.exe -std=c++17 -Wall -Wextra -pedantic -Ithird_party -Iengine tests/test_main.cpp tests/test_touch_gestures.cpp engine/touch_gestures.cpp -o build-touch/touch-tests.exe
& ./build-touch/touch-tests.exe
```

Result: **15 cases passed, 101 assertions passed, 0 failed, 0 skipped**.
This includes the harness sanity case and the existing gesture tests. It
compiles production gesture code without SDL; it does not compile the full
game, Android activity, renderer or command system. `git diff --check` also ran.

An initial Gradle version probe failed because its default cache was outside
the writable workspace. An `assembleDebug --no-daemon` attempt with a workspace
`GRADLE_USER_HOME` then failed downloading Gradle 8.9 with
`UnknownHostException: services.gradle.org`, before Android compilation.
No Android SDK environment variables or SDK at the usual LocalAppData path
were found. At that stage no APK, device/emulator run, full CMake test suite,
TypeScript build, WebView interaction test, asset import, gameplay test,
multiplayer test or performance measurement had been completed. The expanded
follow-up results below supersede those initial validation limits.

## Follow-up implementation and verification

Both local repositories now use branch `codex/generals-touch-unification`.
The native gesture implementation remains the tested change in `b0941ad`;
the follow-up adds a web implementation without copying engine code between
projects. See `work/RedAlert2-Android/docs/ANDROID_TOUCH_CONTROLS.md` for its
file-level changes, player controls and reproducible test commands.

The web branch now provides one-finger pan, 250 ms hold-then-box selection,
600 ms stationary cancel/deselect, preference-independent two-finger cancel,
pinch/pan release protection, and suppression of separate UI click callbacks
after cancellation or a drag. It preserves existing context actions, double
tap, minimap routing, explicit keyboard modifiers and mouse box selection.
The final camera delta is applied on touch release, including a drag completed
between rendering frames, without adding a movement step to desktop velocity
panning. The existing ActionFilter enum was isolated into a small module so
input tests do not load unrelated circular dependencies in the order/task graph.

Web validation:

- **30 regression tests passed, 100 assertions**, across three explicit test
  files: 23 touch/camera cases and seven existing performance helper cases.
- **Vite production build succeeded**, exit 0. Bundle/dependency warnings remain.
- Type checking reports **42 errors**, exactly matching the unmodified upstream
  checkout using the same dependencies after path normalization. The touch
  changes introduce no additional diagnostics; the type check is not green.
- These are production-handler tests with mocked services, not browser, GPU,
  WebView, real asset or multiplayer tests.

Native validation was expanded by installing build tools inside this task's
workspace and compiling the repository's full `ra2assets_tests` target with
CMake/Ninja/GNU C++14.2 (`RA2_BUILD_ENGINE=OFF`). Compilation and linking succeeded.
This compiles the library, simulation/input sources and test executable, not
the SDL game application or Android libraries.

The dedicated touch filter passed **14 cases / 100 assertions**. Its 1121
reported skips are other tests excluded by that filter. The earlier standalone
15-case total included the test harness's extra sanity case.

The full native suite was also run. The initial sandboxed run reported ten
failed cases. Repeating it with working workspace temporary storage and local
loopback sockets permitted cleared four failures. The final result is **1129
reported passes, six failed cases, 140309 passing assertions**. All six remaining
cases throw `mix: too small` in real-game-rules fixtures. The log contains 276
`skipped:` messages from tests returning early without a retail install, which
the test framework misleadingly counts as passed rather than skipped. This
is not a full gameplay pass and the six fixture exceptions are not repaired.

The Android build was retried with network access and a workspace Gradle cache.
Gradle 8.9 downloaded and project configuration ran, then `assembleDebug` failed
with **SDK location not found**. No SDK/NDK installation was provisioned here,
and no APK or device run was produced. This replaces the earlier DNS failure
as the confirmed remaining Android build blocker.

Remaining scope is explicit: no native pinch zoom, new web touch command column,
continuous anchored pinch, redesigned ability targeting, inertial camera or
Generals building rotation. Web long hold deselects when idle; native long hold
retains its 450 ms cancel-only behavior and existing tooltip/team semantics.
Both need real-device testing with the user's own game assets before release.
All changes are local; no repository was pushed, published or merged.
