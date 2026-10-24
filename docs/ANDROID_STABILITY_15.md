# Native Android stability 15

Foundation: shietgrupe-cpu13/RA2AndroidPort android-touch, bccdd7e; uncommitted changes. StillM8 is a separate project and was not modified.

## Report evidence

Newest visible Drive report: 17pVtI5hQoAQ3ZWwcjHKed6q9u-Sw54Oi, uploaded 2026-10-23 08:48:29 UTC. All ten extracted files were scanned; analysis is preserved in deliverables/RA2YRPort/reports/native-test14/analysis.json. Current session exits normally. The included crash trace is byte-identical to an older report and does not demonstrate a new crash. Workers selected the same ore cell; one transport remained blocked for 486 simulation seconds. Disc voxel assets were found, so missing retail files do not explain its absent spinning animation. Unsupported powers were silently filtered.

The reported scene-render CPU average was about 1.95 ms and world-step average about 1.12 ms. These are neither GPU timings nor measured phone FPS/temperature.

## Changes since stability 14

- Read TurretSpins and animate the turret presentation using simulation ticks and ROT, without changing weapon aim or RNG. Exact original rotation speed remains unverified.
- Scope generated voxel sprite caches to model lifetime; give GPU cache entries a unique model identity to prevent stale sprites after a new scene/model load.
- Give slave workers distinct ore reservations and prefer ore near their own manager. Allow unloading at free cells around the manager instead of using a refinery's single vehicle dock.
- Wake idle mobile Slave Miners on a staggered cadence to seek ore and deploy near it. Explicit Stop/Guard and ground-fire orders retain priority. Uses vanilla default scan values; not full Ares custom-tag compatibility. Deployed miners do not yet automatically pack when a field is exhausted.
- After a sustained traffic stall, attempt a bounded alternative route around stationary units on the same layer. Preserve the current route if no alternative exists. This is an engine recovery policy, not verified original-engine parity.
- Put granted supported powers in the defense tab, including charging/ready display and targeting. Remove the invisible legacy radar click targets in real play. Power IDs cannot enter production queues or their pause/cancel menu.
- Implement Psychic Reveal with configured radius: terrain remains explored; live visibility still follows fog of war. Log unsupported power types instead of presenting them as working. Psychic Dominator, Genetic Mutator, Spy Plane and Force Shield are not implemented by this release.
- Add ART_BIND, POWER_ART, POWER, MINER and TRAFFIC_RECOVERY diagnostics alongside stability 14's share-report and rendering assertions.

## Exact source files changed since stability 14

engine/main.cpp, engine/vehicle_animation.hpp, engine/power_cameo.hpp,
engine/harvest.cpp, engine/harvest.hpp, engine/world_yuri.cpp,
engine/unit_orders.cpp, engine/world.cpp, engine/world_loco.cpp,
engine/world_sw.cpp, engine/realplay_economy.inc, engine/realplay_scene.inc,
engine/game_scene_controls.inc, engine/game_scene_play_probe.inc,
engine/hud_tooltip.inc, engine/report_probe.hpp,
tests/test_projection.cpp, tests/test_harvest.cpp,
tests/test_locomotion.cpp, tests/test_yuri_core.cpp,
docs/ANDROID_STABILITY_15.md, tools/package_stability15.py.

## Validation and limits

Native suite: 1170 cases, 154207 assertions passed; six retail-rules cases explicitly excluded because the original installation is unavailable. Additional asset-dependent tests can return early without testing retail behavior. Initial sandbox-only socket/temp-file failures passed with the required access.

Android main translation unit passed syntax checking (one existing missing-override warning). Android release build, signature/asset checks and matching crash symbols are recorded beside the APK. Lint retains 33 errors and 37 warnings in pre-existing Android/SDL resources and bindings; it is not a clean lint result.

No phone attached and no retail asset installation available here. Tank Bunker visibility/evacuation, Gatling Tank visuals, terrain slope behavior, all factions and thermal performance still need real-game verification. This release does not claim to fix every animation or unit behavior. No retail assets, Windows DLLs, Ares or Phobos source were bundled.

## Research boundary

Ares and Phobos extend the original Windows game using DLL injection; their DLLs cannot execute inside this native ARM engine. Winlator runs the original executable through Wine/Box86/Box64. cnc-ddraw replaces DirectDraw presentation, not game logic. Their documentation informs targeted behavior checks; this patch does not claim equivalent compatibility.

https://ares-developers.github.io/Ares-docs/new/harvesters.html
https://phobos.readthedocs.io/en/latest/Fixed-or-Improved-Logics.html
https://github.com/brunodev85/winlator
https://github.com/FunkyFr3sh/cnc-ddraw

## Phone verification

Install over stability 14. Start a fresh Yuri skirmish first; deploy two Slave Miners, destroy a worker, move a miner near another field and test a blocked approach. Observe the Flying Disc turret, Psychic Reveal charging/targeting, bunker entry/fire/evacuation and a large group moving through a narrow opening. Then test an older save. Share an in-game report immediately after a failure, describing unit, command and location. Compare the same map/unit count on stability 14 and 15 for heat/FPS before drawing performance conclusions.
