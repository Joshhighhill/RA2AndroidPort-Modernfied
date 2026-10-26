# Yuri's Revenge fidelity delta audit — 26 October 2026

Baseline: `android-touch` **3f6d33cf75df4bcad1dbb2a40623ec4b7234ce62**, verified
against the user's remote before editing. Native C++/SDL engine; this is not
the separate StillM8 WebView project. This document adds evidence and corrects
earlier crosswalks rather than reopening accepted work.

**User acceptance retained:** Tank Bunker movement/positioning, building Force
Fire and Citadel walls work on the phone. No corresponding gameplay, bunker
rendering, wall resolver or touch code is changed here. Nuclear visuals remain
**UNVERIFIED**. Source and automated tests cannot confer phone acceptance.

## New evidence and its limits

The new Drive policy puts diagnostics in `ra2yrlogs`
(`1PSZq6qc75qS51ZlVPKnDf-XPIU3l0kuS`) and APKs only in `ra2yr`.
Newest diagnostic: `1qTO7B54f8Uiih_vRGhmxvoJj-aGZArWJ`, 215,728-byte ZIP,
created 2026-10-26 00:14:47 UTC. Metadata identifies build **900042**, version
`0.9.0+gv0.9.0-88-g3f6d33c`, Android 16 / Xiaomi 2412DPC0AG, no description,
`low_memory=false`, available memory 4,346,654,720 bytes.

All ordinary text members were read: current engine 100,825 bytes / 1,150 lines,
previous engine 87,905 / 995; skirmish 24,207 / 604; report, device, exit history,
running marker and README. Both asset logs are empty. Neither engine log has a
fatal/assertion/signal marker. Current log stops at frame 675 before-world;
absence of a shutdown line is not proof of a crash. Exit history records
LockScreenClean, normal_mem_pressure and SwipeUpClean; do not recast these as
a new native crash. Two binary tombstone `.trace` attachments identify historic
SIGSEGV/null dereferences dated October 22 (Batterynotifier) and October 23
(dex_preload_t); they remain un-symbolicated. These dates precede build 900042;
no claim of a new crash root cause is made.

Short timing windows: render 59.39 and 60.92 calls/s, mean CPU 2.541 and
3.220 ms, maxima 104.680 and 53.040 ms; world-step mean 1.797 and 1.885 ms,
maxima 2.944 and 2.800 ms. These are CPU scope summaries, not GPU timings,
screen FPS, p95/p99, battery or sustained thermal benchmarks. Sampled world
checks report zero invalid objects and zero waits over three seconds, but
the report does **not** exercise a live YAGGUN firing sequence or establish
that traffic is solved. GATOW art missing is an optional roster warning;
map dressing reports zero missing art. No newer MP4/image was found than
the references already inspected for build 900042; they were not relabeled
as new evidence. ADB had no attached devices.

## Pinned cross-project evidence

All public trees below were enumerated at their actual commit with no API
truncation. Foreign files are private ignored research references, not compiled,
linked or copied into the engine.

* [User baseline](https://github.com/shietgrupe-cpu13/RA2AndroidPort/tree/3f6d33cf75df4bcad1dbb2a40623ec4b7234ce62).
* [Omar upstream](https://github.com/omarelarab/ra2-engine/tree/a8cfdccfef8592c13a6517c3fb76d834b1534027):
  upstream `pres_game.inc::defense_fire_tick` uses per-shot `play_at`, not this
  fork's owned Gatling loops. It provides no drop-in correction for the reported
  stationary-loop regression. Upstream history includes a GPL relicensing
  commit followed by a 0BSD commit; preserve this fork's existing licensed
  snapshot and attribution and review provenance before future cherry-picks.
* [Gagarin](https://github.com/r-melvin/gagarin/tree/945a29e262dbb364bbe41605c42fc5acde5a2918):
  exact `scripts/data/match_manifest.json` rows were checked. Its STATUS snapshot
  reports 10,268 matched / 743 partial functions, 18.3% matched function bytes;
  this is not a playable whole-game engine. GPLv3 research only.
* [StillM8](https://github.com/StillM8/RedAlert2-Engine/tree/f34dd487809869a46db38cba4ef8ccdb2234e59f):
  actual `GattlingTrait.ts` uses a fixed 54-tick stage constant, whereas this
  fork reads retail 200/400/600 thresholds. `MagnetizedTrait.ts` uses a 40-tick
  hold and 15% strength drop damage; this is an alternative approximation, not
  evidence of retail locomotor parity. `SubmergibleTrait.ts` models cooldown
  and emergence; `AresPrerequisites.ts` models expression trees and has dedicated
  tests. These are design comparisons, not patches to transplant. README
  explicitly calls Mental Omega compatibility work in progress. GPLv3.
* [Mustafa](https://github.com/Mustafa-MS/RedAlert2-Android/tree/b332c3f78ef4cff46ce4f5b6a93703b970beb3a2):
  inspected Kotlin MainActivity, AssetSchemeHandler and ThermalMonitor. The
  host uses WebView; thermal/power notifications are useful design references.
  Its asset handler opens packaged GameRes/WebDist with `Cache-Control=no-cache`;
  this is not evidence of superior native loading or a safe retail-asset
  distribution pattern. GPLv3; no shell code reused.
* [Chrono Divide SDK](https://github.com/chronodivide/mod-sdk/tree/5943c4ae6c19897929d348a417d6d2f1481b75fd):
  README explicitly excludes YR mods and says HVA playback uses only the first
  frame. Use visual captures and documented override concepts, not client code
  or Yuri-specific behavior claims. No new controlled FPS comparison was made;
  the user's stability observation is not a cross-engine benchmark.

Primary game evidence is the user's unmodified retail `rulesmd.ini`,
`artmd.ini`, `soundmd.ini` read from their owned installation, plus the existing
PC reference footage. Configuration establishes values/flags, not every
mission or animation transition. Original manual descriptive behavior is
available in this [archived Westwood manual](https://manuals.plus/m/6272c40b5b024d58eb23722bcdcfc61902ffe7b6d76c2b33249827b105be1718.pdf)
(third-party host of original documentation, not a new PC experiment).
ModEnc is specialist documentation; distinguish vanilla notes from Ares/Phobos
extensions. No original executable was run or newly disassembled in this audit.

## Ranked findings

Difficulty S/M/L is relative engineering scope, not a time promise. IDs below
are audit-local to avoid colliding with existing R/X tracker numbers.

| Finding | Impact / priority | Root confidence | Difficulty | Status |
|---|---|---|---|---|
| A43-01 Stationary Gatling report lifetime | Audio / first patch | Confirmed cell-order bug; general silence unconfirmed | S | Corrected + instrumented |
| A43-02 Floating Disc draining | Core gameplay / highest next | Confirmed absent native handler | L | Design + stronger original crosswalk |
| A43-03 Magnetron lift/pull/drop | Core combat / high | Confirmed missing native lift | L | Research; no fake damage |
| A43-04 Unavailable superweapon effects | Core gameplay / high | Confirmed launch-table gap | L each | Research |
| A43-05 Slave navigation/site choice | Economy / high | Existing partial behavior; candidate weaknesses | M | Plateau regression added; no speculative rewrite |
| A43-06 Global traffic/deadlocks | Movement / high | No fresh failing trace | M/L | Reproduction/profiling plan |
| A43-07 Boomer/sub targeting and launch | Naval combat / high | Existing runtime; exact PC parity partial | M/L | Research |
| A43-08 MCV and prerequisites | Core gameplay / retain | Existing implementation confirmed | S/M | Preserve; crosswalk corrected |
| A43-09 Slopes/interpolation/depth | Presentation / medium | Existing implementation; exact parity unproven | M | Research |
| A43-10 SHP/HVA/factory/beam/audio timing | Presentation / medium | Existing partial event systems | M | Per-object acceptance plan |
| A43-11 Android frame pacing/memory | Performance / medium | Timing spikes observed; cause unknown | M | Measure before changing architecture |
| A43-12 AI economy/expansion | Strategy / medium | Bounded plan confirmed; scenario parity unproven | M/L | Research |
| A43-13 Data mods/Ares compatibility | Optional / later | Vanilla parser vs extension gap confirmed | L | Separate capability roadmap |

### A43-01 — Gatling Cannon firing audio

**Original:** YAGGUN has no Primary entry; it uses six numbered ground/air
weapons. Ground stages AGGattling/2/3 are shared with YTNK; tower AA stages are
AAGattCann/2/3 instead of the tank's AAGattling/2/3. All use reports
GattlingGunAttackLoop1/2/3. Retail sound controls are attack/loop/random/all/decay,
Limit=2, volumes 60/65/75. RateUp=1, RateDown=50 and thresholds 200/400/600
are shared. These assets are not shown missing by the new logs; the working
tank also argues against a blanket missing-file diagnosis.

**Gagarin:** `match/combat/Techno.cpp::GattlingRateDown` **0x70E000**, matched
100%, controls sound release and stage decrease. Header exposes two original
sound controllers. `SetGattlingValue` 0x70DE00 is matched. This does not prove
the entire firing/audio startup path is reconstructed.
[Pinned method header](https://github.com/r-melvin/gagarin/blob/945a29e262dbb364bbe41605c42fc5acde5a2918/match/combat/techno_class_gattling_rate_methods.h).
[Gatling-system documentation](https://modenc.renegadeprojects.com/IsGattling).

**Fork:** `weapon_resolve.cpp::resolve_gattling`, `building_combat.cpp`
`defense_weapon/step_ordered_building_fire/step_building_combat`,
`pres_game.inc::defense_fire_tick`, `weapon_sound_loop.hpp` and
`pres_audio.inc::loop_at/update_loop`. Unlike the tank, the building loop
considered only unit/building pointers as a target. A ground cell Force Fire
has neither pointer, so the new loop was immediately stopped in the same tick.
The loop also did not consult `def_offline` when power failed.

**Small fix implemented:** recognize cell attack kind 3 in the presentation
lifetime gate and stop on `def_offline`; use the existing report-list picker
for building reports. No combat rules, stage values, damage, ROF, event wire
format, simulation state or shell code changed. New WEAPON_AUDIO records only
stage/report/start-result changes and actual loop stops. They expose weapon,
slot, charge, report, mixer enable state, variant count and handle, so the next
phone report can distinguish no event, failed clips, limits and cancellation.
No unconditional per-shot stderr flush or MIX tracing added.

**Expected improvement:** audible sustained cell Force Fire and proper low-power
stop, plus actionable evidence for ordinary tower silence. **Do not claim the
user's general stationary-turret sound complaint is resolved yet.**
**Tests:** actual defense event -> report -> owned loop -> real software mixer
with synthetic PCM, ground/AA and all three stages, cell Force Fire, Stop,
power gate and unchanged saved world under presentation updates. Fixture uses
short thresholds for speed, not changed retail parameters. Phone acceptance:
one turret alone, then tank+turret (Limit=2), ground/AA, retarget, three stages,
Stop, power loss/recovery; attach report containing WEAPON_AUDIO.

### A43-02 — Floating Disc drain is a gameplay gap

**Original:** DiskDrain is DrainWeapon=yes; it operates above a Drainable
structure. Power plants interrupt the victim house's power; a drained powered
defense stops; ResourceDestination buildings transfer configured funds. Owned
rules: amount 30, frame delay 30, DISKRAY animation. This is separate from
DiskLaser direct attacks and their already-corrected rim geometry.
[DrainWeapon](https://modenc.renegadeprojects.com/DrainWeapon),
[Drainable](https://modenc.renegadeprojects.com/Drainable).

**New Gagarin evidence:** `Techno.cpp::Drainer::BeginDraining` **0x70FD70** is
matched 100%; it checks the occupying cell's building, establishes reciprocal
links, flags the owner, creates/attaches a drain animation and refreshes the
display. `IsDraining` 0x70FEB0, `IsBeingDrained` 0x70FEC0 and drainable predicate
0x70FED0 are matched 100%. The old report's predicate-only crosswalk was
incomplete. These do not by themselves establish periodic transfer and every
cleanup path. Do not confuse `drain_and_unlink_methods.cpp` 0x401FC0 (container
memory cleanup) with Floating Disc behavior.
[Begin-drain layout](https://github.com/r-melvin/gagarin/blob/945a29e262dbb364bbe41605c42fc5acde5a2918/match/movement/locomotion_begin_drain_methods.h).

**Fork/root confirmed:** no DrainWeapon/Drainable runtime state in native
weapon resolver, world or save visitor. Generic secondary selection/damage
cannot implement reciprocal draining or DISKRAY ownership. `production.cpp`
power_totals and existing blackout mechanisms must be integrated, not replaced.
**Smallest correct next implementation:** parsed flags plus saved source/target
links and one drain controller; legal above-building approach; tick-driven
transfer; dedicated drained power state distinct from manual `power_off` and
spy blackout; cleanup on move/Stop/death/capture/sale/target loss; DISKRAY and
charge/report lifetime. Requires save schema review/bump, not a cosmetic beam.
**Acceptance:** plant/defense/refinery/non-drainable, own/enemy/captured targets,
multiple Discs, depleted funds, no duplication/negative credits, Stop/death and
capture recovery, deterministic replay and save in every phase. L scope;
unimplemented here rather than shipping a partial economy corruption risk.

### A43-03 — Magnetron vehicle lift, pull and drop

**Original:** IsLocomotor weapon moves valid vehicles; the secondary attacks
structures. TELE has CanPassiveAquire=no. Damage=5000 on the lift weapon is
not ordinary cannon damage. Original manual describes vehicle lifting and
ineffectiveness against infantry; exact pull/drop/surface collision needs the
original locomotor implementation or controlled PC footage.
**Gagarin:** Techno.cpp `ReleaseLocomotor` **0x70FEE0**, matched 100%, is now
verified as a specific release/crash/ownership transition lead. Full acquisition,
magnetic locomotion, falling damage and stacking behavior are not established.
**Fork/root confirmed:** `weapon_resolve.cpp` marks magnetic;
`weapon_unit.cpp::unit_weapon_for_building` selects secondary; `combat.cpp`
explicitly suppresses magnetic primary damage until the locomotor exists.
**Small fix path:** add a dedicated saved lift/pull/drop controller and legal
victim gates, then emit beam events anchored to FLH/altitude. Do not remove the
damage suppression or copy StillM8's fixed 15% damage approximation.
**Benefit:** functional vehicle combat without instakill regressions.
**Tests:** infantry, ground/naval/hover/air, buildings, enemy/friendly control,
bridges, release over water/obstacles, source/target death and save/load. L/high.

### A43-04 — Superweapon names do not equal implemented effects

**Original:** Spy Plane, Force Shield, Genetic Mutator and Psychic Dominator
have distinct prerequisites, targeting, charge/power, immunity and effects;
read their retail INI/art definitions and original behavior individually.
**Fork/root confirmed:** enum names exist in `superweapon.cpp`, but
`realplay_economy.inc::rp_sw_type_of` does not map SpyPlane, ForceShield,
GeneticConverter or PsychicDominator. The newest log explicitly emits
CAPABILITY_UNSUPPORTED for NARADR, all three labs, YAGNTC and YAPPET.
This is a confirmed user-facing grant gap, not evidence that adding enum
switch cases alone supplies working mechanics.
**Gagarin:** superweapon vector/RTTI/helper matches are not effect handlers;
no complete corresponding effect routine was verified in this audit.
**Next fix:** one complete effect end-to-end, starting with authoritative rules,
grant/capability gating, legal command application, deterministic saved state,
presentation and cleanup. Retain current unsupported diagnostics meanwhile.
**Tests:** prerequisites, charge, power loss, legal/illegal cells and targets,
invisibility/immunity/control interactions, interruption and replay/save.
Highest gameplay backlog, L per effect; nuclear launch visuals remain untested.

### A43-05 — Slave coordination exists; original timing/parity is partial

**Original:** linked slaves use SLAV HarvestRate=150 and miner
SlaveReloadRate=25, replacement delay 500, five workers in owned rules. They
have a managed mining/follow lifecycle, not ordinary player orders until freed.
**Fork checked:** loader reads those values; `harvest.cpp` applies rates and
manager-centered ore preference/reserved targets; `world_yuri.cpp`
step_yuri_workers replaces workers, follows a packed manager, transfers freed
workers and performs ore relocation. `slave_passage.hpp` supports own-manager
passage. These are not absent features.
**Gagarin:** LostSlave **0x6B0A20** and ShouldWakeUpNow **0x6B1020** matched
100%; Deploy2 **0x6B0D60** partial 84.6%. They do not prove complete follower
pathing. See pinned `match/combat/slave_manager_class_*` files/manifest.
**Suspected weaknesses:** mobile follow uses stable 30-frame polling and
1–2-cell perimeter enumeration, miner wake/relocate uses 150 frames, ore
reachability is a 4-connected terrain flood rather than the full layered
locomotor path. Site routing accepts any nonempty nearest fallback path before
stamping a deployment goal. These are candidates, not newly reproduced bugs.
**Verification added:** isolated three-level plateau next to ore still obtains
an exact legal deployment route and deploys across save/load with equal state;
it PASSED on the pre-change gameplay code. No speculative movement patch made.
**Small correction if reproduced:** require exact legal site completion, then
try next candidate without losing worker linkage; trigger follow re-evaluation
on stale goals/blocked paths with deterministic bounded cadence. Preserve Stop.
**Acceptance:** blocked/mobile/deployed managers, 1/5/multiple sets of slaves,
exhausted ore, cargo return, replacement/death/capture, slopes/bridges and repeat
seeded arrival/blocked-time measurements. M; no fresh stalled-worker trace.

### A43-06 — Traffic and pathfinding need measured failure scenarios

**Original:** missions, occupancy classes, terrain layers and locomotors jointly
determine movement; parked friendly yielding and blocked-path replanning differ
from allowing overlap. **Fork:** `pathfind.cpp` already has deterministic cost
and tie-break order; `world_loco.cpp` has scatter, replanning budgets, ownership
and stale claim handling; World movement has nearest-reachable fallback.
Do not replace A* because of an old sparse log.
**Gagarin:** MapClass::Update_Pathfinding_2 **0x586990**, matched 100%, refreshes
invalidated subzones; it is not the whole pathfinder. The reviewed drive/ship
trees have exact lock/interface/persistence helpers; full movement method
parity was not demonstrated by those manifest rows.
**Root:** no current deadlock reproduced. Layer transition, stale claims,
target-chase versus explicit-move and retry-budget overload remain hypotheses.
**Small next action:** record a blocked unit's mission, source/destination/layer,
next-cell owner and retry budget alongside existing TRAFFIC_RECOVERY logs;
reproduce that case in a deterministic map fixture, then correct one state gate.
**Acceptance/benefit:** 1/10/100-unit seeded corridors, opposing traffic, mobile
blockers, intact/destroyed/repaired bridges and navy; bound waits and illegal
crossings, compare save hashes independent of render refresh rate. M/L.

### A43-07 — Boomer behavior is partially implemented, not wholly absent

**Original evidence:** BSUB has BoomerTorpedo, CruiseLauncher, two CMISL spawns,
NavalTargeting=7 and LandTargeting=2; prerequisites are YAYARD,RADAR, **not** a
Battle Lab. LandTargeting=2 selects the secondary against land; it does not
alone establish a passive-acquisition prohibition. Original launch/surface
pose must come from art and actual PC capture, not an invented opening hull.
**Fork:** `world_an.cpp::an_naval_slot` implements land-secondary, naval gates,
submerge/sonar and spawners; `rocket.cpp` implements rocket phases. Existing
tests actually launch V3/Boomer rockets. `combat.cpp` has passive acquisition,
guard scan ranges and explicit-target handling. NavalTargeting=7 currently
falls through the generic primary case; exact YR semantics need checking.
**Gagarin:** Ship constructor **0x69EC50**, Load **0x69EE90** and piggyback
helpers are matched; those are not proof of a complete surfacing/launch renderer.
**Root:** exact surfacing, guard pursuit and dual launch alignment remain
unverified, rather than treating missiles as missing wholesale.
**Small next fix:** compare one normal torpedo and two spawned missile launches
with timestamped weapon/slot/FLH/spawn/altitude/pitch events, patch only the
incorrect transition. **Tests:** naval/land/air victims, Stop/guard/attack-move,
explicit orders, reload, two offsets, target loss and AA interception. M/L.

### A43-08 — Preserve working MCV and prerequisite behavior

**Original/rules:** AMCV->GACNST, SMCV->NACNST, PCV->YACNST and reverse
UndeploysInto links are present. Game MCV Redeploy setting matters. BSUB radar
requirement is deliberate. **Fork:** deployment state/timer and try_undeploy
exist; `world.cpp` packs valid conyards rather than issuing a sale refund;
existing MCV regression and user acceptance stand. `production.cpp`
prerequisites_met counts owned placed buildings and generic groups; mobile
Slave Miner PROC alternate support already has tests.
**Research correction:** Gagarin **0x41B890** manifest identifies
AircraftClass_0041B890, matched 100%; it is NOT an MCV deployment reference.
Factory balance/queue helpers do not establish the full build-availability gate.
**Root:** no new MCV/stock prerequisite bug reproduced. Mod-specific negatives,
alternates and fourth-side rules are a separate compatibility gap.
**Small action:** table-driven stock roster acceptance tests before changing
requirements; never add a Battle Lab requirement to Boomer. **Tests:** all
three factions, capture, build-up, lost prerequisites, redeploy on/off, queues,
blocked exits and save/load. S/M; no gameplay rewrite here.

### A43-09 — Existing slope/interpolation implementation needs exact calibration

**Original:** vehicle voxel/HVA transforms and tile elevation affect vehicle
pose; infantry remain directional SHP sprites rather than mechanically tilting
their flat image like a tank. **Fork:** UnitPresentation has previous/current
XY, facing, altitude and ground history, reset gates for teleport/cargo/deck;
terrain_tilt has ramp normals, quantized rigid transitions and an explicit
upright approximation for saddle ramps 17–20. Depth/bridge layers already exist.
**Gagarin:** Drive DrawMatrix 0x4AFF60 and Process 0x4B0500 remain source leads;
this pinned manifest search did not verify their complete matched routines.
**Root confirmed:** approximation exists; exact retail visual error for every
ramp is not measured. Chrono's smoothness is a benchmark, not matrix authority.
**Small next fix:** paired frame captures on each ramp, calibrate transform and
ground anchor independently, preserving accepted bunker offsets and legal
simulation layers. **Tests:** all headings on flat/edge/corner/saddle ramps,
bridge entry/middle/exit/underpass, depth/occlusion and unchanged sim hashes at
30/60/120-Hz rendering. M; no speculative global tilt rewrite.

### A43-10 — Missing animation must be classified before adding assets

**Fork checked:** inf_anim, pres_bld_anim, pres_walk/HVA frame selection, muzzle
FLH, beam and missile event systems exist. voxel_raster uses selected HVA
frames. Shadows intentionally use the original ShadowIndex/frame-zero path
described in docs/re/voxel-render.md; this is not automatically a bug just
because animated models have more frames. Prior Brute idle/secondary corrections
and direct Disc rim laser remain intact. Factory completion, egress and door
timing require per-art sequence checks, not a new generic animation layer.
**Original/Gagarin:** relevant Infantry finish routine **0x520810** and individual
factory/anim state functions need per-case reconstruction verification; no
whole animation implementation proven in this audit. Retail Anim/Trailer/FLH
keys establish resources but not all triggers. Chrono SDK explicitly lacks HVA
playback, so it cannot be used to remove native multi-frame behavior.
**Root:** object-specific trigger/anchor/timing suspects; newest asset logs are
empty, no proof of missing stock files. **Small action:** log four outcomes:
missing file, wrong archive/palette, absent event, correct event/wrong draw pose.
Trace weapon/target/FLH/Z/animation/sound on the fixed logic tick. **Tests:**
one unit per family, factory idle/produce/exit, turret/body directions, moving
Disc and air target, nuclear launch/ascent/descent/impact. Preserve shells. M.

### A43-11 — CPU/GPU optimization must follow a profile

**Observed:** short CPU render spikes with low average world-step time; no
GPU trace. TextureCache already has a 384-MB soft LRU budget and pins terrain
pointers; VoxelAnimationCache has an 8-MB per-cache soft budget retaining the
current/previous working set. Aggregate caches can exceed one cache's budget.
Both raster prewarm and display-only interpolation already exist.
**Suspected causes:** lazy voxel/palette/ramp rasterization, asset decoding,
upload, allocator activity or OS scheduling; no one cause confirmed.
**Small next improvement:** add frame-time percentiles and cache hit/miss/
raster-duration/eviction counters, then A/B a fixed replay with Perfetto CPU
and GPU data. Measure aggregate RSS/PSS and trim behavior. Prewarm only measured
hot variants and keep eviction at safe frame boundaries. Do not indiscriminately
bake giant atlases, alter simulation cadence, or move mutable World state into
workers on the basis of these two windows.
**Reference:** Mustafa's ThermalMonitor provides Android thermal callbacks as
an architecture idea; it is not proof of lower native costs. Gagarin x86 timing
and browser fps are not Android performance baselines. **Acceptance:** sustained
same-map/replay 10/50/100-unit runs, p50/p95/p99, jank, cache residency, thermals,
power, loading and equal simulation hashes. No device connected; no trace run.

### A43-12 — AI expansion needs a scenario comparison

**Fork checked:** ai_base.cpp has prerequisite-sorted plans, power preemption,
affordability, ratios/limits, rival-base-size cap and economy expansion.
It explicitly notes that the static plan often fills refinery_limit already,
making the post-plan refinery append branch unreachable. This is a confirmed
structural limit, not proof that the Soviet AI fails every economy scenario.
Combat recruitment and guard policies are separate from base planning.
**Original/Gagarin:** upstream docs/re/ai-base.md traces **0x5052D0** plan sorting
and **0x4FDB30** expansion; this Gagarin manifest query did not prove full matched
implementations at those two addresses. Do not substitute unrelated House/AI
helper matches. **Small fix path:** compare owned retail ai(md)/rules values,
initial plan versus destroyed/depleted refinery recovery and construction range;
correct duplicate/limit/rebuild decision only if a deterministic scenario fails.
**Benefit/tests:** more reliable economy without cheating resources; seeded
Allied/Soviet/Yuri 10-minute builds, ore exhaustion, lost factories/refineries,
power shortage and enemy pressure, with spend/order/rebuild traces. M/L.

### A43-13 — Incremental mod support, isolated from vanilla

**Current:** MIX/INI/SHP/VXL/HVA decoding is not full mod execution. Main loader
still binds stock rules/art rosters and known mechanics. Existing Mental Omega
read-only audit remains valid: fourth-side/custom armor/AttachEffect/Convert
features and x86 Ares/Syringe are not implemented by this native ARM64 program.
No mod or retail folder was changed. [Official MO requirements](https://mentalomega.com/index.php?page=install).
**Comparisons:** StillM8 AresPrerequisites implements explicit expression nodes
and dedicated tests, a useful separation pattern; its README does not claim
complete MO compatibility. Chrono's SDK excludes YR mods. Gagarin original
function matching does not supply Ares extensions or an Android DLL interface.
**Small correct roadmap:** opt-in non-destructive profile + unified archive/loose
file precedence, manifest hashes and unsupported-key report; prove a small
data-only RA2/YR pack first, then add capabilities individually. UI, AI and
production must share one availability evaluator. Do not silently accept keys
then advertise playable compatibility. Keep vanilla resolver defaults intact.
**Tests:** profile off/on/revert, stock hash unchanged, precedence collisions,
malformed paths/archives, unknown extensions, save/profile identity and multiplayer
profile mismatch rejection. L; mod activation is not part of this patch.

## Implementation scope and verification

Changed runtime: presentation-only building sound lifetime/report dispatch and
bounded diagnostic state. Added tests: mixer integration, power/cell gates and
existing miner plateau/save behavior. No new saved fields; save schema stays 55.
No claim of implemented Disc drain, Magnetron lift, new superweapons, universal
pathing fixes, generic mod support or improved measured phone FPS.

Native full suite: **1,275 cases / 277,597 assertions passed**, zero failures;
one optional missing retail landmark fixture excluded. Some other optional
oracle/data tests can early-return without their fixtures; this is not a claim
that every retail campaign oracle executed. New audio subset: three cases /
245 assertions passed, using synthetic PCM. Android NDK main.cpp syntax passed
with only the existing waypoint_cell missing-override warning. No real device,
emulator, new Perfetto trace, GPU/thermal test or speaker check was run.

Final Android/APK/push results and source SHA are recorded in the existing
Google Drive tracker after verification, with the packaged build metadata.
The phone checklist is docs/TOUCH-TEST43.txt. This audit preserves Omar's
attribution, Bryle13 credits and dependency notices. No Gagarin/GPL/proprietary
implementation or EA/Mental Omega assets are included.
