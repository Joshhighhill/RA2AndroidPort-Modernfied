# Wave 7 / 11: Ground and water locomotion, pathfinding, unit speeds — gap analysis

**Tree:** master `b3a509f`, `build/ra2engine.exe` (not rebuilt).
**Scope:** the Walk, Drive, Hover, Ship and Teleport locomotors. The data has no Mech or Tunnel user: CAOS and DRON carry Mech only in a comment, and nothing uses Tunnel. Also in scope: pathfinding, blocked units, group moves, bridges and cliffs, shroud, entering transports and buildings, and speed calibration. The Fly and Jumpjet locomotors are covered by 05-aircraft.md and are not repeated here.
**Method:**
- **Data:** `rulesmd.ini` (`D:\ra2-re\out\shroud_evidence`), GroundType tables and the per-type Speed / ROT / Accelerates / SpeedType / MovementZone keys.
- **Prior docs:** gamespeed.md, movement-height.md, movement-zones.md, voxel-render.md §5, veterancy.md, REMAINING.md, and wave-7 reports 01–09.
- **Engine code:** `world.cpp` `step_units` / `order_unit_move` / `group_move` / `land_speed_mult`, `main.cpp` `speed_to_cps`, `pathfind.cpp`, `group_move.cpp`, `world_veterancy.cpp`, `harvest.cpp`.
- **Ghidra:** two opus sub-agents on private project copies, which are now deleted. Their reports and dumps:
  - `D:\ra2-re\w7-loco-out\REPORT.txt` covers Drive, Walk and the speed formula (copy: `11-evidence/re-drive-walk.txt`).
  - `D:\ra2-re\w7-loco2-out\REPORT.txt` covers Hover, Ship, Teleport, pathfinding and entering (copy: `11-evidence/re-hover-ship-teleport-path.txt`).
- **Play runs:** 9 `--play` runs, with scripts and results in `11-evidence/`. Skirmish was run with `slider:speed=2` (GameSpeed 4), so one driver frame is one logic frame. Positions were read from the `ulx`/`uly` probes, in leptons.
  - s1–s4, s6, s7, s9: the default skirmish map.
  - s8: Bay of Pigs (ships).
  - The R-210 gate `move-bridge-cliff.play` was re-run: PASS, 1404 frames.

## 1. Summary

The clone is built on one unverified premise from gamespeed.md §4: that `GameSpeedBias` 1.6 multiplies every ground unit's speed through `house+0x190`. RE shows that premise is wrong:
- `house+0x190` is written by AssignHandicap and **never read** [CONFIRMED-RE: whole-exe operand scan; only the 2 writes in `FUN_004F6DD0`].
- The "house speed" factor in `GetCurrentSpeed` `FUN_004DB0B0` is the **country's** SpeedUnitsMult / SpeedInfantryMult, which is 1.0 [CONFIRMED-RE `FUN_0050BE70`].
- Vehicles do end up about 1.6× faster than their `Speed=` leptons, but only because of the **Drive (and Ship) track tables**. Each track point costs 7 speed units and points are about 11.1 leptons apart: ×1.59 straight, ×1.67 diagonal [CONFIRMED-RE `FUN_004B0E50`, tables at `0x7E6A28` / `0x7E6B28`].
- The Walk and Hover locomotors move **raw leptons**.

So the clone's speeds for vehicles are right by coincidence. Every infantry unit (Walk) and every Hover unit moves **1.6× too fast**.

**Totals.** 17 gaps: 3 MAJOR, 9 MINOR, 5 COSMETIC / cross-reference.

**Top gaps:**
1. **W7-11-01 (MAJOR).** Infantry move 1.6× too fast. A GI covers 16 leptons/frame where the original covers 10; a dog 32 instead of 20; Tanya swimming 24 instead of 15 [CONFIRMED-PLAY s1/s8; CONFIRMED-RE Walk `0x75ADCC`].
2. **W7-11-02 (MAJOR).** The Hover locomotor (Robot Tank, LCRF, SAPC) is modelled as Drive with the 1.6 bias. The Robot Tank moves 40 leptons/frame where the original cruises at 25. The hover acceleration, braking, approach slow-down and drift turns are all missing [CONFIRMED-PLAY s3; CONFIRMED-RE `0x515CF0`].
3. **W7-11-03 (MAJOR).** Blocked units abandon their order after 2 s (`kGiveUpAfter`), even when far from the goal. The original never abandons because of units: it re-paths every 9 frames, switches to a path-around-traffic mode after 60 frames, stops only within CloseEnough (2.25 cells) of the goal, and otherwise orders the friendly blockers to scatter [CONFIRMED-RE `0x4B2550`, `0x4D3840`, `0x759CE0`].
4. **W7-11-04 (MINOR, gameplay-visible).** Vehicles stop and turn in place at every bend in the path. The original drives 45° and 90° bends as curved tracks without stopping, and turns in place only for 135°/180° bends or when starting off-axis [CONFIRMED-RE `FUN_004B2550`, TurnTrack `0x7E6B28`].
5. **W7-11-05.** Crawling (prone) infantry move at full speed. The original moves a Crawls=yes prone soldier at `s − s/3`, so 7 leptons/frame for a GI [CONFIRMED-RE `0x521AB0`].
6. **W7-11-06.** The damage slow-down threshold is ConditionRed (25%); the original uses **ConditionYellow (50%)**, and only for Drive and Ship [CONFIRMED-RE `0x4B2550`, Rules+0x1700].
7. **W7-11-07.** `Accelerates` / `AccelerationFactor` / `DeaccelerationFactor` / `SlowdownDistance` are ignored. That affects harvesters, MCVs, the Chrono Miner, V3, subs and dolphins, which accelerate and brake in the original.
8. **W7-11-08.** The initial A* ignores other units, and every step costs 10 or 14 (octile). The original A* charges a parked friendly 8, a moving block 1 or 4, a cloaked cell 1000, and a destroyable blocker 20 or 60, with uniform orthogonal and diagonal cost [CONFIRMED-RE `0x429A10` / `0x4297B0`].

## 2. Per-locomotor table (measured vs original)

The leptons/frame figures are at GameSpeed 4 (15 logic frames/s). The "Original" column comes from the RE formulas; nothing was captured from the retail game.

| Locomotor / unit | Rules | Clone measured | Original (RE) | Verdict |
|---|---|---|---|---|
| **Walk** E1 / E2 | Speed 4 → 10 lept | **16.0** lept/f (s1: +80 per 5 f) | **10** (raw leptons, no bias) [CONFIRMED-RE `0x75ADCC`, `0x4DB0B0`] | WRONG ×1.6 (01) |
| Walk DOG / ADOG | Speed 8 → 20 | **32.0** (s1) | **20** | WRONG (01) |
| Walk, prone crawl (E1) | Crawls=yes | 16 (no crawl term in `step_units`) | **7** (`10 − 10/3`) [CONFIRMED-RE `0x521AB0`] | WRONG (05) |
| Walk, swimming TANY | Speed 6 → 15 | **24.0** (s8, water) | **15** (no water term) [CONFIRMED-RE] | WRONG (01) |
| Walk, terrain (ore, weeds, rough) | Foot 90% / 50% | 1.0 | 1.0 (the Foot column only gates passability) [CONFIRMED-RE] | OK |
| **Drive** MTNK Grizzly | Speed 7 → 17 | **27.2** straight (s1) | 17 per frame through tracks ≈ **27.0** straight, **28.4** diagonal | OK (≤4%) |
| Drive HTNK Rhino | 6 → 15 | **24.0** (s2) | ≈ 23.9 | OK |
| Drive FV IFV | 10 → 25 | **40.0** (s2) | ≈ 39.8 | OK |
| Drive, ROT turn | ROT 5 | **1280** u16/frame (s2 `sel_body`: 18944 → 33024 in 11 f) | `min(ROT,127)<<8` = 1280 [CONFIRMED-RE `0x4C95A0`] | OK |
| Drive, bends in the path | — | stops and turns in place at every direction change | curved 45/90° tracks without stopping [CONFIRMED-RE] | WRONG (04) |
| Drive, damaged | — | ×0.75 at ≤25% (s2: 60/300 HP → 20.4) | ×0.75 at ≤**50%** (ConditionYellow), applied to the integer speed | WRONG threshold (06) |
| Drive, veteran MTNK | VeteranSpeed 1.2 | `8/7 × 27.2` = 31.1 | `ftol(17×1.2)` = 20 → ≈ 31.8 | OK (<3%) |
| Drive, acceleration (HARV, AMCV, CMIN on drive, V3) | Accelerates default yes | none, instant | +0.03/frame, slows within 500 lept to 0.3, decel `Speed×0.002` [CONFIRMED-RE `0x4B0E50`] | MISSING (07) |
| Drive, reverse | — | none | none (no reverse tracks) [CONFIRMED-RE] | OK |
| **Hover** ROBO | Speed 10 → 25 | **40.0** (s3, with powered GAROBO) | cruise **25**; 0.5 within 1 cell (0.75 with HoverBoost on a straight); accel 1/18 per frame, brake 1/27 [CONFIRMED-RE `0x515CF0`] | WRONG (02) |
| Hover, terrain | Hover Clear 50% | 1.0 | 1.0 (the table only gates passability) [CONFIRMED-RE scan `0x513000–0x518000`] | OK (by accident) |
| Hover LCRF / SAPC | 6 → 15 | not measured (`water_near_sel` gave no order in s8) | cruise 15 | WRONG by code (02) |
| **Ship** DEST | 6 → 15 | **24.0** (s8, diagonal 67.9/4 f per axis) | 15 per frame through the ship's own track tables (`0x7F1A40`); lept/frame ratio [SUSPECTED ≈1.6 like Drive] | OK? |
| Ship SUB | 4 → 10, Accelerates=yes | **16.0**, instant (s8) | ramps +AccelerationFactor/f; slow-down 0.3 near goal | MISSING accel (07) |
| Ship DLPH | 8 → 20, Accelerates=yes | **32.0** (s8) | as SUB | MISSING accel (07) |
| Ship, turning | ROT | turns in place, then straight | curved tracks; no motion while the hull rotates [CONFIRMED-RE `0x69F2A0`] | WRONG (04) |
| **Teleport** CLEG | ChronoDistanceFactor 48 | 10-cell jump done within 5 frames (s1), then frozen 53 f (abilities gate stage 4) | immediate jump, then `dist/48` frozen (min 16) [CONFIRMED-RE `0x7187B7`] | OK |
| Teleport CMIN Chrono Miner | Harvester=yes | warps both to ore and to the refinery | **drives** (piggybacked Drive loco) except when docking at a DockUnload refinery; delay 0, no freeze [CONFIRMED-RE `0x740780`] | WRONG (W6-04-34, see 13) |
| Mech / Tunnel | — | — | classes exist (vt `0x7ECB6C` / `0x7F4A24`); no stock user | N/A |

## 3. Gaps

### W7-11-01: Infantry (Walk locomotor) move 1.6× too fast
- **Severity / effort:** MAJOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] s1: E1 `ulx` 18080 → 18960 in 55 frames is 16.0 lept/frame; DOG covers 32.0. s8: swimming TANY covers 24.0.
  - [CONFIRMED-RE `0x75ADCC..0x75AEE6`] WalkLocomotion sets SpeedPercentage to 1.0, then `X += ftol(cos × GetCurrentSpeed())`, in raw leptons with no track geometry.
  - [CONFIRMED-RE `0x4DB0B0`] GetCurrentSpeed is `ftol(type+0x678 × countryMult × SpeedMultiplier)`, then ×VeteranSpeed for FASTER, then ×SpeedPercentage. No `house+0x190` or GameSpeedBias factor appears in it.
  - [CONFIRMED-RE] `house+0x190` has no reader anywhere in the exe.
- **Original vs clone:** the original GI moves 10 lept/frame (0.59 cells/s at 15 fps); the clone's moves 16 (0.94 cells/s).
- **Code:**
  - `engine/main.cpp:830-835` `speed_to_cps` applies `g_game_speed_bias` (1.6) to every unit.
  - The doc premise is gamespeed.md §4 [ANCHORED → refuted].
- **Fix:**
  - Apply the 1.6 track factor only to the Drive and Ship locomotors. Better: model the track geometry (161 units per straight cell, 217 per diagonal).
  - Walk, Hover and Teleport should use `trunc(Speed×256/100)` leptons/frame raw.
  - Correct gamespeed.md §4.
  - Note: every infantry-timed test and play script will shift.

### W7-11-02: The Hover locomotor is modelled as a biased Drive
- **Severity / effort:** MAJOR / M
- **Evidence:**
  - [CONFIRMED-PLAY] s3: ROBO (with GAPOWR ×2 + GAROBO) moves 40.0 lept/frame (120 per 3 frames) and starts at full speed.
  - [CONFIRMED-RE Process `0x514198`, `0x515CF0`]:
    - Each frame the unit moves `ftol(GetCurrentSpeed × cur)`.
    - `cur` ramps by `1/(HoverAcceleration×900)` = 1/18 per frame up, and `1/(HoverBrake×900)` = 1/27 per frame down.
    - The target is 0 when the facing error is over 45° (stop and turn), 0.5 within 1 cell of the goal (×HoverBoost 1.5 on a straightaway, clamped to 1.0), and 1.0 otherwise.
    - The unit moves along its *current* facing, so small turns drift in an arc.
    - The GroundType Hover column only gates passability.
- **Original vs clone:** the original Robot Tank cruises at 25 lept/frame, eases in and out, and slides around shallow bends. The clone's runs at 40 lept/frame, starts and stops instantly, and turns in place at every bend.
- **Code:**
  - `main.cpp:830` (bias).
  - `world.cpp:5609-5629` (turn-then-move gate for any `is_vehicle`).
  - `weapon_resolve.cpp:143-148` (`land_speed_loco` false for Hover; correct).
  - No `HoverAcceleration` / `HoverBrake` / `HoverBoost` reader exists.
- **Fix:** add a Hover branch in `step_units` (keyed on the `{4A582742}` CLSID): a raw-lepton speed with a `cur` ramp, the 45° stop rule, the 1-cell approach fraction, and motion along `body_facing`. HoverBob / HoverHeight are presentation only.

### W7-11-03: Blocked units abandon their move after 2 seconds
- **Severity / effort:** MAJOR / M
- **Evidence:**
  - [CONFIRMED-code] `world.cpp:166-171`: `kBlockedWait = 0.30 s` and `kGiveUpAfter = 2.0 s`. At `world.cpp:5792-5799`, any unit continuously blocked for 2 s clears its path, at any distance from its goal.
  - [CONFIRMED-RE Drive `FUN_004B2550`, Walk `0x759CE0`, Hover `0x514D90`, Ship `0x6A1310`, Basic_Path `0x4D3840`]:
    - **Moving blocker (Move 2):** stop. Re-path when the PathDelay timer expires (`ftol(0.01×900)` = 9 frames). After BlockagePathDelay (60 frames), re-path in mode 2, where moving units cost 1000 so the route goes around the jam.
    - **Parked friendly (Move 6):** re-plan first. On the next try, stop as "arrived" if within CloseEnough (576 lept), otherwise order the occupants to scatter (CellClass::ScatterContent `0x481580`, forced) and retry with a 10-count.
    - A move is abandoned only when **no terrain route exists**. Units never cause abandonment.
  - [CONFIRMED-PLAY] s7 and s9 show the clone resolves a clean 3-vs-3 head-on jam and a 12-tank pile. A long chokepoint was not tested, so whether the 2 s give-up fires in real traffic is [SUSPECTED].
- **Original vs clone:** in the original, units in a long queue (a bridge, a base gate) keep their order and arrive. In the clone, a unit stuck behind traffic for 30 frames stops where it is.
- **Fix:**
  - Replace `kGiveUpAfter` with the original's state machine:
    - a 9-frame re-path cadence;
    - a 60-frame blockage mode, using `passable_minus_units` only for *moving* units, or cost 1000;
    - a CloseEnough stop;
    - forced scatter of parked friendlies for vehicles as well as infantry (the clone only asks own-house idle infantry: `world.cpp:5762-5770`).
  - Keep the seniority stagger only if it is still needed for determinism.

### W7-11-04: Drive and Ship units stop and turn in place at every bend instead of following curved tracks
- **Severity / effort:** MINOR (visible movement feel, slower pathing around corners) / L
- **Evidence:**
  - [CONFIRMED-code] `world.cpp:5609-5629`: before each segment the vehicle turns until `|d| ≤ kAimToleranceU16`, with no movement while it turns.
  - [CONFIRMED-PLAY] s2 MTNKTURN: an order reversing a Grizzly that was mid-cell (`ulx` 18334.8) left it rotating in place mid-cell for 12+ frames.
  - [CONFIRMED-RE `FUN_004B2550`, TurnTrack `0x7E6B28` (72 × 12-byte entries, `cur×8 + next`), RawTrack `0x7E6A28`]:
    - The unit turns in place only when its facing is off the next cell's 8-way direction.
    - The track is then chosen by looking one step ahead, so a 45° or 90° direction change is driven as a curve spanning the cells, and a new curve can be joined at the track's jump index without stopping.
    - 135° and 180° bends use a straight track after a turn in place.
    - Ships do the same with their own tables (`0x7F1A40`, `0x7F1960`).
- **Fix:**
  - Port the TurnTrack/RawTrack tables. They are data, so they can be dumped from `0x7E6A28` / `0x7E6B28`; see `D:\ra2-re\w7-loco-out\tracks.txt`.
  - Drive `fx`/`fy`/`body_facing` along the track points, at 7 speed units per point.
  - This also yields the exact ×1.59 / ×1.67 speed geometry needed by W7-11-01.

### W7-11-05: Prone (crawling) infantry move at full speed
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-RE `0x521AB0`] InfantryClass::GetCurrentSpeed: if prone (`+0x6DB`) and the art key `Crawls=` is set, `s = s − trunc(s/3)`. Otherwise (prone with Crawls=no, reachable only by the "crippled" path `0x5F4DE2` [SUSPECTED]) `s = s + s/2`.
  - [CONFIRMED-code] `step_units` has no prone term; `inf_anim.cpp:108` only switches the sequence to Crawl.
  - This contradicts infantry-anim.md §11 ("crawling keeps walking speed").
- **Fix:** multiply the per-frame speed by the integer rule above when `InfAnimState::prone`. W7-01-17 (stand up on a move order) still applies on top.

### W7-11-06: The damage slow-down uses ConditionRed instead of ConditionYellow
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-RE `FUN_004B2550`, and Ship `0x6A1310`] `if (Health/Strength ≤ Rules+0x1700) p *= 0.75` (constant `0x7E6FC0`). `+0x1700` precedes ConditionRed `+0x1708` (G-65), so it is ConditionYellow (50%). The offset-to-key name is by adjacency.
  - The original factor applies to the Drive/Ship SpeedPercentage, which then goes through `ftol`. Walk and Hover have no damage slow-down.
  - [CONFIRMED-PLAY] s2: a Grizzly at 60/300 HP moved 20.4 lept/frame (×0.75). [CONFIRMED-code] `world.cpp:5562` compares with `c.condition_red` (`w5_combat_arm.inc:49`).
- **Fix:** read `[AudioVisual] ConditionYellow` for the threshold, and apply the factor to the integer speed (`ftol(17×0.75)` = 12) before the track geometry.

### W7-11-07: Acceleration and braking keys are ignored
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-RE Drive `FUN_004B0E50` lines 101-188; Ship `0x69FC80`; defaults in the ctor `FUN_0070FE90`]:
    - Accelerates (default **yes**).
    - AccelerationFactor (0.03 per frame).
    - DeaccelerationFactor (0.002 × max leptons per frame).
    - SlowdownDistance (500 lept): approaching the goal slows to a floor of 0.3.
    - Stop_Moving caps the speed at 0.3.
    - A CrusherAll unit crushing a wall or building is capped at 0.2.
  - [CONFIRMED-DATA] Combat tanks set `Accelerates=false`. HARV, AMCV, SMCV, CMIN, the civilian cars, V3 (AccelerationFactor 0.01), SUB, DLPH and SQD (0.01) accelerate.
  - [CONFIRMED-PLAY] s8: SUB and DLPH reach full speed on the first moving frame.
  - [CONFIRMED-code] No reader of `Accelerates` exists in `engine/`.
- **Fix:** keep a per-unit `speed_pct` in `step_units` with the ramp and slow-down rules above, for the Drive and Ship locomotors only.

### W7-11-08: The A* ignores units, and the step costs differ from the original
- **Severity / effort:** MINOR / M
- **Evidence:**
  - [CONFIRMED-code] `pathfind.cpp:17-23`: octile costs 10/14. `order_unit_move` (`world.cpp:1265-1272`) searches the terrain grid only, so units are invisible to the first path; only the blocked re-path removes them.
  - [CONFIRMED-RE A* `0x429A10`, cost `0x4297B0`, table `0x81770C`]: step cost depends on the Can_Enter result: OK 1, ClosedGate 1, Temp (parked friendly) 8, MovingBlock 1/4/1000, Destroyable 20, FriendlyDestroyable 60, Cloak 1000.
  - Further rules from the same RE:
    - Diagonals cost the same as orthogonals, plus a direction tie-break of .001–.008.
    - The heuristic is Euclidean.
    - A hierarchical zone pass runs first.
    - The path is stored 24 steps at a time.
    - Terrain speed is **not** in the cost, which is faithful in the clone.
- **Original vs clone:** the original routes around parked friendly blobs and through cells with crushable or destroyable obstacles at a price. The clone drives into the blob and only re-paths after it is blocked. Diagonal-heavy routes also differ.
- **Fix:** feed occupancy into the A* cost (parked friendly 8, moving 1 or 4) and use uniform step cost with the tie-break table.

### W7-11-09: Friendly vehicles and parked units are never told to move out of the way
- **Severity / effort:** MINOR / S (part of 03)
- **Evidence:**
  - [CONFIRMED-code] `world.cpp:5762-5770`: `scatter_out` is filled only for a vehicle mover blocked by **same-house, idle, target-less infantry**.
  - [CONFIRMED-RE `0x4B2550` Move 6 branch → ScatterContent(force=1) `0x481580`]: every interruptible occupant, vehicles included, gets Scatter. This applies to infantry and vehicle movers alike (Walk `0x75A6B1`), and regardless of PlayerScatter.
  - W7-01-20 covers the alliance and target filter. This item covers the **vehicle-blocker** and **infantry-mover** halves.
- **Fix:** route the Move-6 branch to `order_scatter` for each occupant of the blocked cell, whether vehicle or infantry, own or allied.

### W7-11-10: There is no CloseEnough arrival rule
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-RE `0x4B2550`, `0x4D3840`] when the next cell is held by a parked friendly and the unit is within CloseEnough (`Rules+0x1718` = 576 leptons) of its destination, with a height difference under 2 levels, it stops and counts as arrived. A Temp-held destination beyond CloseEnough is retargeted to `NearbyLocation`. The clone has no CloseEnough test for player moves; only `ai_team.cpp` uses it for teams.
- **Original vs clone:** in a pile-up the original's late arrivals stop up to 2 cells short. The clone's pre-assigned formation cells (`group_move.cpp`) make each unit drive to an exact distinct cell. In s9 the 12th tank arrived about 445 frames after the order, against an unobstructed 122 frames.
- **Fix:** add the CloseEnough stop in the blocked branch. The formation pre-assignment can stay as an engine deviation; see §4, note 1.

### W7-11-11: The re-path cadence is 0.3 s instead of PathDelay (9 frames)
- **Severity / effort:** COSMETIC / S
- **Evidence:** [CONFIRMED-code] `kBlockedWait = 0.30` s (4.5 frames), doubled for the senior unit. The original uses `ftol(PathDelay×900)` = 9 frames, and the BlockagePathDelay (60-frame) mode switch [CONFIRMED-RE disasm `0x4D3F42`].
- **Fix:** read `[General] PathDelay` / `BlockagePathDelay` (fold into 03).

### W7-11-12: Passengers walk to cells next to the transport
- **Severity / effort:** COSMETIC / S
- **Evidence:**
  - [CONFIRMED-code] `world.cpp:1630-1643` `group_enter_transport` assigns `assign_formation` cells around the transport (whole cells, land grid).
  - [CONFIRMED-RE Infantry `0x51BD70`, Unit `0x73DFA0`] in the original, Can_Enter returns OK for the NavCom transport or building's own cell under Enter / Capture missions (UnitAbsorb for vehicles), so movers path **into** that cell. The absorb then happens on arrival, by radio contact [SUSPECTED].
- **Impact:** the boarding point differs, and a cluster of infantry spreads over up to N cells instead of converging.
- **Fix:** path to the transport's cell, allowing it in the per-step test when `enter_target` is set.

### W7-11-13: The Chrono Miner warps to the ore field (cross-reference W6-04-34 / W7-03-13, now RE-resolved)
- **Severity / effort:** MINOR / M
- **Evidence:**
  - [CONFIRMED-RE UnitClass::SetDestination `0x740780`] For a Teleporter unit:
    - If the radio contact is a DockUnload refinery and the destination is a cell, it ends any piggyback and teleports.
    - **Otherwise** it co-creates a Drive locomotor (`{4A582741}`) and piggybacks, so it **drives**, both to ore and on player moves.
    - A Harvester=yes teleporter gets delay 0 and no freeze [CONFIRMED-RE `0x7187B7`].
  - [CONFIRMED-code] `harvest.cpp:572-576` warps back to the field; `world.cpp:1212` routes every Teleporter move through `teleport_unit`, the CMIN included.
- **Fix:** in `order_unit_move`, teleport only when `abil.teleporter && !is_harvester`, or when it is a harvester docking. Drive in every other case, using CMIN's Drive speed (Speed 4 → ≈15.9 lept/frame through tracks).

### W7-11-14: Track crushing details
- **Severity / effort:** COSMETIC / S
- **Evidence:** [CONFIRMED-RE `0x73DFA0`, `0x4B0E50` l.510-529]:
  - A Crusher (or the elite CRUSHER ability) enters Crushable wall overlays, and CrusherAll enters any wall.
  - A crush forces a straight track.
  - TiltsWhenCrushes pitches the hull by −0.05.
  - CrusherAll over a wall or building caps the speed at 0.2.
  - Non-crushable walls are shot down by Wall=yes warheads (Move 5).
  - The clone's `crush_grid()` / `crush_leak()` route crushers over crushable walls (P4), but it has no tilt, no 0.2 cap, and no "drive up and shoot the wall" (Destroyable) behaviour.
- **Fix:** fold into W7-11-04 (straight track) and W7-11-07 (0.2 cap). The Destroyable approach also covers movement-height.md's open `Move::Destroyable` gate deviation.

### W7-11-15: Waypoint (planning) mode is missing (cross-reference R-150)
- **Severity / effort:** MINOR / M
- **Evidence:**
  - [CONFIRMED-RE `0x508F10`] each house holds 12 waypoint paths; a waypoint is added while the count is under `MaxWaypointPathLength` (15) and the path is not closed.
  - [CONFIRMED-code] `input.cpp:286` "(PlanningMode ... not modeled)". The PlanningMode hotkey is parsed but inert.
  - The original has no Shift-queue for moves; Shift is the select toggle (faithful).
- **Fix:** see R-150 (open).

### W7-11-16: The infantry move order snaps to an 8-way facing and has no turn
- **Severity / effort:** COSMETIC
- **Evidence:** [CONFIRMED-RE Walk `0x759CE0`] the facing is set instantly with atan2 toward the next sub-cell point, and the unit snaps within 17 leptons. The clone's 8-way `facing_from_delta` is equivalent at draw resolution. **Not a gap**; recorded to close the question.

### W7-11-17: A vehicle re-ordered mid-cell turns in place in the middle of a cell
- **Severity / effort:** COSMETIC / S
- **Evidence:** [CONFIRMED-PLAY] s2 MTNKTURN: `ulx` stayed at 18334.8 (cell 71.62) for 12 frames while `sel_body` rotated. The original only re-evaluates at a track's end or jump point [CONFIRMED-RE `FUN_004B0E50`], so it finishes the current track before turning. The exact retarget-mid-track branch is [SUSPECTED].
- **Fix:** comes with W7-11-04.

## 4. Verified OK

- **Drive speeds** for the Grizzly, Rhino and IFV match the original's track-geometry result within 1% on straights [CONFIRMED-PLAY s1/s2 + RE]. Diagonals are 4% slow (27.2 vs 28.4).
- **ROT turn rate:** `min(ROT,127)<<8` per frame, measured exactly as 1280/frame for ROT 5 [CONFIRMED-PLAY s2, CONFIRMED-RE `0x4C95A0`].
- **No reversing** [CONFIRMED-RE: no reverse tracks].
- **Veteran speed** within 3% (the clone truncates the rules Speed rather than the lepton value). V8 is now resolved: FUN_004DB0B0 reads `type+0x678` (leptons) and the **country** mult [CONFIRMED-RE]. veterancy.md V8's worked example ("E1 4 → 4") should read leptons (10 → 12).
- **Terrain speed** (GroundType × SpeedType, slope factors, 0 → 0.5) for Drive/Ship, and its absence for Walk and Hover (passability only) [CONFIRMED-RE `0x4B2550`; CONFIRMED-code `land_speed_mult`].
- **Each unit moves at its own speed** in a group. No slowest-unit matching exists in the original either [CONFIRMED-PLAY s4/s9; RE: no formation code, no speed coordination found, [SUSPECTED] for absence]. Note 1: the clone pre-assigns distinct destination cells (`assign_formation`), where the original sends everyone to the clicked cell and spreads them with the NearbyLocation/CloseEnough rules. The end result is a similar blob; this is an accepted deviation.
- **Infantry sub-cells:** three per cell (spots 2–4), and squads pack three to a cell [CONFIRMED-PLAY s4 `uspot` 2/3/4; W5-B FindInfantrySubposition].
- **Head-on jam resolves** (3 vs 3) and a 12-tank group pile-up completes without anyone giving up [CONFIRMED-PLAY s7, s9].
- **Cliffs, slopes and high-bridge decks (R-210):** `move-bridge-cliff.play` PASS on this build [CONFIRMED-PLAY]. The height gate matches `FUN_004D9B70` [movement-height.md].
- **Shroud is pathable:** a Grizzly ordered into black shroud (95,41) got a path [CONFIRMED-PLAY s6 `upath=94040`]. The original A* and Can_Enter read no shroud [CONFIRMED-RE]. The MoveToShroud consumer is still untraced; see W7-01-26.
- **Teleport (CLEG):** the jump is immediate and the post-jump freeze is `dist/ChronoDistanceFactor`, with a minimum of ChronoMinimumDelay [CONFIRMED-PLAY s1 + abilities gate; CONFIRMED-RE `0x7187B7`]. If the destination is occupied, the clone's choice of a near free cell matches NearbyLocation in spirit.
- **Hover passability** over water and beach for ROBO, LCRF and SAPC (Amphibious* zones) [movement-zones.md; RE].
- **Ship domain:** water only; a low bridge blocks (06 report).
- **The PoweredUnit Robot Tank** does not move without a powered GAROBO [CONFIRMED-PLAY s1/s2 vs s3]. This is W6-04-18 behaviour, not a locomotion bug.
- **Crusher routing over crushable walls** (P4 `crush_grid`) [CONFIRMED-RE `0x73DFA0` rule matches].
- **Swimming and panic speeds:** the original has no water or fear term in speed [CONFIRMED-RE], so the clone's lack of one is faithful, apart from the ×1.6 of W7-11-01.

## 5. Open questions

1. The Ship lept/frame per track point: the ship tables at `0x7F1A40` were not measured, so DEST at 24 lept/frame is [SUSPECTED] right.
2. The flag that sets the Hover snap-facing (`loco+0x70`), and the Drive flag `Foot+0x3CD` (slow to 0.1).
3. The Mission_Enter radio handshake (where the absorb fires).
4. The MoveToShroud consumer (`type+0xC8D`).
5. Player "guard unit" / follow: `GuardModeStray` is read **only** by TeamClass [CONFIRMED-RE]. The player guard-object path was not traced, and the clone has no guard-a-unit order.
6. Whether a teleporting unit is damage-immune while frozen: ReceiveDamage checks only BeingWarpedOut [SUSPECTED: it is not immune].

## 6. Fix order

| # | Gap | Where | Effort |
|---|---|---|---|
| 1 | W7-11-01 Walk raw leptons (drop the 1.6 except for track locomotors) | `main.cpp:830` `speed_to_cps`, gamespeed.md §4 | S |
| 2 | W7-11-02 Hover locomotor | `world.cpp` `step_units` | M |
| 3 | W7-11-03 / 09 / 10 / 11 blocked-move state machine | `world.cpp:5755-5840` | M |
| 4 | W7-11-05 / 06 crawl and damage factors | `step_units`, `land_speed_mult` | S |
| 5 | W7-11-07 acceleration | `step_units` | S |
| 6 | W7-11-13 Chrono Miner drives | `world.cpp:1212`, `harvest.cpp` | M |
| 7 | W7-11-08 A* occupancy cost | `pathfind.cpp`, `order_unit_move` | M |
| 8 | W7-11-04 / 14 / 17 track tables | new `tracks.inc` from `0x7E6A28` / `0x7E6B28` / `0x7F1A40` | L |
