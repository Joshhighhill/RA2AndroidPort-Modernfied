# Native Android engine stability test 14

Based on RA2AndroidPort `android-touch`, commit `bccdd7e`. This patch changes the
native C++/SDL2 fork only; StillM8 RA2YRPort remains a separate application.

## Engine changes

- Terrain visibility considers the TMP cliff/extra image separately from the
  flat diamond. An offscreen diamond no longer hides an onscreen cliff.
- Fog dims opaque terrain pixels using texture modulation, including cliff
  extensions. Transparent diamond corners no longer darken neighboring pixels.
  Shared texture modulation is restored after each draw.
- Terrain movement rejects malformed X coordinates that previously aliased a
  different row when converted to a linear index, invalid layers and zero width.
  Existing one-level ramp and bridge rules remain in force. This is not a full
  vehicle tilt/suspension implementation or proof of vanilla slope parity.
- Missing unit SHP messages are emitted once per image instead of every frame,
  avoiding repeated synchronous disk flushes for persistently missing artwork.

## Precise local reports

Use the launcher **Share game report**, or pause a match and select **Game
Controls → Share game report**. Enter the unit/map, observed behavior and expected
behavior, then choose Drive or another destination in Android's share sheet.
Sharing from the pause menu keeps that menu open and records a fresh world probe.

Reports retain the existing archive/file allowlist and bounded local logs. No
automatic upload, retail archive or save-file attachment is added.

- `report.txt`: schema 2, UTC report timestamp, package/version, available memory,
  Android low-memory flag and optional user description (maximum 2,000 characters).
- `RENDERER`: SDL backend, flags, physical/logical sizes and maximum texture size.
- `UNIT`: miner/worker/Gatling and blocked-unit samples, IDs, coordinates, level,
  bridge layer, path progress, blocked duration, ore target/cargo, manager and
  combat/transport references. At most 24 unit samples per probe.
- `ROUTE`: next cell, ground heights and tile/subtile IDs for slope investigation.
- `STRUCTURE` / `OCCUPANT`: bunker/miner references, worker replacement countdown,
  occupant identity and cargo state. At most 16 structures and 8 occupants each.
- `SLAVE`: spawn, replacement, release, blocked spawning and ore-target decisions.
- `CHECK`: nonfatal observations for nonfinite positions, invalid path indices,
  invalid manager/occupant references. They do not reset units or abort gameplay.
- `TERRAIN_FALLBACK`: first 16 unresolved tile/subtile locations per terrain build.
- `PERFORMANCE`: five-second aggregate CPU average/maximum and stage call rate for
  world simulation and scene rendering. These are not GPU timings, temperatures,
  display-present FPS or a hardware profiling trace; background pauses affect rate.

Regular probes run every 150 simulation ticks. Unit reports are samples, not a
complete replay. Old crash traces retain their original timestamps and may belong
to a previous session. Large logs can rotate while a report is being prepared.

## Validation and limitations

The available native suite passes: 1,163 cases, 154,142 assertions. Six existing
tests require retail rules data unavailable here and were excluded; additional
asset-dependent cases return early with a skip message. The initial sandbox run
also failed filesystem/socket checks; those pass when run without that restriction.
The probe regression verifies saved simulation bytes are unchanged by observation.
Bounds, ramp/cliff, bridge and invalid-coordinate regression tests are included.

`tests/render_stability` exercises the actual terrain blit helper with SDL's
software renderer and synthetic pixels. Build with CMake, passing an existing
SDL 2.30.9 source directory as `SDL2_SRC_DIR`; then run `render_stability`.
It checks alpha corners, fog tint, restoration of shared texture tint and cliff
bounds without retail assets. Android build/lint and final package details are
recorded with the delivered APK.

The software pixel readback passes (fog RGB 99/49/24 from 200/100/50; background
20/40/180 unchanged). Android ARM64 assembly and lint execution complete. Full
lint reports 33 errors and 37 warnings in existing SDL bindings/resources; the
build has lint abort disabled. The changed app Java files have no lint findings.
This is not a clean full-lint result.

Package: `org.ra2.engine`, version code `900014`, version name
`0.9.0+gv0.9.0-19-gbccdd7e-dirty`. The debug signing certificate matches native
test 12 (code 900013), allowing an update of that installation. Native symbols
with the packaged library's matching Build ID are retained for future reports.

The target POCO has not been connected to this build environment. Retail map
appearance, heat, real-device frame pacing and a complete Yuri skirmish still
require phone testing. Existing bunker/worker bugs are not declared fixed merely
because assets load or a diagnostic reports a valid reference. The historical
native crash still needs matching-symbol investigation.

## Phone test

1. Install the native test APK over the native fork, preserving its game files.
2. Pan across a cliff at all four screen edges; compare fog enabled/disabled.
3. Move infantry and tanks uphill/downhill, around a cliff and across a bridge.
4. Reproduce one worker or bunker issue and pause immediately.
5. Share a report with a short description and, for visual defects, a recording.

Keep native-fork reports separate from StillM8 reports when comparing performance.
No GitHub push or claim of a stable release is implied by this local test patch.

## Changed files

- `android/app/src/main/java/org/ra2/engine/Diagnostics.java`
- `android/app/src/main/java/org/ra2/engine/RA2Activity.java`
- `engine/diagnostics` additions: `report_probe.hpp` (existing `diagnostics.hpp` unchanged)
- `engine/terrain_blit.hpp`, `engine/viewport_bounds.hpp`, `engine/iso_renderer.cpp`
- `engine/pathfind.cpp`, `engine/platform.cpp`, `engine/platform_os.cpp`, `engine/platform_os.hpp`
- `engine/main.cpp`, `engine/ingame_pages.inc`, `engine/harvest.cpp`, `engine/world_yuri.cpp`
- `tests/test_bridge_cliff.cpp`, `tests/test_projection.cpp`, `tests/test_yuri_core.cpp`
- `tests/render_stability/CMakeLists.txt`, `tests/render_stability/main.cpp`
- `tools/package_stability14.py`, `docs/ANDROID_STABILITY_14.md`
