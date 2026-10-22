# Android test 12: local crash diagnostics

## Purpose and phone procedure

The reported crash happened during a USA/Allied skirmish after a Chrono Miner teleport. Its cause has NOT been identified. This change records evidence; it does not claim to repair that crash or finish Yuri abilities.

1. Install this APK over test 11, keeping imported files. Same package/signing key, ARM64, versionCode 900013. Save format remains 51.
2. The file/setup launcher now appears on every launch. Tap Start Game and reproduce the USA skirmish crash, preferably with the same map/settings.
3. Reopen the app. BEFORE starting another game, tap **Share game report** and save/send the ZIP, then attach it in this chat. The Android chooser only sends it when you choose a destination.
4. Describe the last action and approximately how long the match ran. A screenshot of the selected map/settings helps when the crash occurs before the skirmish snapshot exists.

No game data re-import is required for an update. The report button is in the Android file/setup launcher, not the in-game menu.

## Files and captured evidence

- `engine/diagnostics.hpp`: process-lifetime, mutex-protected recorder. Current/previous engine logs are each capped at 1 MiB; writes flush C stdio immediately. Asset files have the same two generations, with up to 8,192 distinct records and approximately 1 MiB each. Only skirmish.txt can be written as a snapshot, capped at 64 KiB. Logging write failures do not throw into gameplay. Logs are enabled automatically in the Android application; ordinary desktop/native tests leave them off.
- `engine/main.cpp`: redirects existing C++ streams to the local recorder and copies SDL logs while preserving their previous output. Records version, map bytes/FNV-1a identity, INI bytes/FNV-1a identities and explicit INI archive sources, periodic simulation/presentation progress, actual session seed and local house. `skirmish.txt` contains map/mode, seed/settings, country/side/AI/team slots, final merged CMIN-related rule/art sections and General/CombatDamage/MultiplayerDialogSettings. FNV is a noncryptographic identity hint, not a security hash.
- `engine/command.cpp`: records commands and actor state, targets/destinations, IDs and cells, excluding selection notifications. `COMMAND end` means the handler returned; it is not proof the order was accepted or completed.
- `engine/harvest.cpp`: records warp entry (unit, image, health, position, requested destination, harvest state, refinery and cargo), resolved landing and completion after occupancy update. It does not alter teleport logic.
- `include/ra2assets/asset_trace.hpp`, `src/asset_fs.cpp`, `src/mix.cpp`: opt-in callback records archive-open attempts, named file hits and successful asset resolution/source/size. Numeric-ID-only reads are not all named; this is a bounded resolution list, not a complete filesystem audit. It may stop recording new names at its limit.
- `Diagnostics.java`, `RA2Activity.java`: device/app version/start marker, Java exception stack, Android historical exit reasons and up to two available native/ANR traces. All stored privately on the phone. No remote reporting service.
- `SetupActivity.java`: always-accessible launcher, sharing button and reminder to share before restart.
- `ReportProvider.java`, manifest: nonexported, grant-protected, read-only ZIP provider. Only generated report filenames can be opened. Retail archives/saves are not copied. Logs may contain paths, map names, match details and existing engine output. Cached report ZIPs retained up to three; old local crash text may survive newer runs, so compare timestamps/version.
- `android/app/jni/CMakeLists.txt`: main library line tables retained in the separate matching symbols artifact; packaged release libraries remain stripped.

## Crash-report limits

A native crash cannot safely be recorded by ordinary C++ exception handling. This implementation keeps Android's native signal handling intact. On API 30+, exit history can identify crashes, ANRs, low-memory/system kills and other exit reasons. On API 31+, native trace streams may contain tombstone protobufs, not plain text. Android may return no trace or overwrite it; two traces, each capped at 2 MiB, are collected. Earlier Android versions get local logs/Java exceptions only.

Flushing improves survival of process crashes but does not guarantee survival of device power loss/storage failure. A crash before native logger startup can lack engine logs. Current/previous rotation bounds disk use and loses older history; share immediately after the crash. A five-second simulation heartbeat narrows the last completed stage but is not a full instruction trace. Logging can add I/O overhead and can change timing. No full automatic command replay is provided. No native signal-handler crash injection was run.

Android's primary API documentation: https://developer.android.com/reference/android/app/ApplicationExitInfo and https://developer.android.com/reference/android/app/ActivityManager#setProcessStateSummary(byte%5B%5D).

## Validation and limitations

- Native CMake rebuild succeeded. Targeted doctest run: 194 selected cases reported passed, 2,640 assertions, zero failures, 972 filtered out. 47 selected asset-dependent cases returned early with skip messages (doctest counts these as passed). The prior known retail-only `w7units: rulesmd type flags*` case remains explicitly excluded. See NATIVE_TEST_RESULTS.txt.
- New synthetic diagnostics test verifies bounded rotation, immediate flush visibility, snapshot cap/path restriction, actual named MIX lookup tracing/deduplication, asset count/size limits and matching miner landing/harvest/occupancy/event-count results with logging enabled and disabled. This is not proof of complete match determinism.
- Standalone `tests/diagnostics_abrupt_exit.cpp` exited with `_Exit(17)` without destructors; its last flushed line remained readable. This tests abrupt process exit, not an Android native crash/tombstone.
- Android NDK main.cpp syntax check succeeded with two existing warnings (shift precedence and missing override).
- APK build and packaging results are recorded in the accompanying BUILD_INFO.json and INSTALL_CHECK.json. These checks do not establish successful installation or runtime behavior on the POCO.
- No POCO/device, Android share-sheet runtime test, real game-data run, crash reproduction, retail-art or sound playback test was available. Android system trace availability is not guaranteed. Existing gameplay limitations from test 11 remain, including incomplete Magnetron lift/pull/drop and other Yuri features.

## Credits

Native engine: omarelarab/ra2-engine (0BSD), original history/license retained. MYSOREZ Generals X is a touch-design reference; Mustafa-MS/RedAlert2-Android is a research reference. No code copied from those reference projects. See CREDITS.md, LICENSE and THIRD_PARTY_NOTICES.md. No retail assets bundled.
