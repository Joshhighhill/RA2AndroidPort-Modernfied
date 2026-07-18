# CB-2 Aircraft & Flight-Locomotor Phase Design — Units Fly at Altitude

**Date:** 2026-07-18
**Status:** Approved (controller-scoped, formalized autonomously)
**Depends on:** the complete AI arc, Unit Production, Economy, Buildings,
combat, the voxel-vehicle render pipeline, and CB-1 Transports/Cargo — the
nine permanent byte-identity gates this phase must keep bit-identical:
`--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a,
`--demo-ai-team` ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc, `--demo-transport`
transport_hash=fe500f946e913645. Reuses the voxel raster + `facing_cache`
(`docs/re/voxel-render.md`) for art, the `commit_attack_target`/`step_combat`
path (`docs/re/combat.md`) for air-to-ground fire, and CB-1's off-map/inert-
field byte-identity discipline (`transport-cargo.md` §8) as the model for
adding a CORE-SIM movement mode without perturbing ground units.

## Goal

Introduce **FLIGHT**: a new movement mode where a unit travels in a straight
line at a cruise **altitude**, ignoring ground pathing / terrain-walkability /
cell-occupancy (it flies over everything and reserves no ground cell while
airborne), turning its facing toward the travel direction; **takes off**
(ascends from the ground to cruise altitude) and **descends**; is drawn
**raised** on screen (a vertical offset proportional to altitude) with a
**ground shadow** at its ground cell; and **acquires and attacks a ground
target from the air**, composing on the existing combat path. Demonstrated
deterministically on one real flying combat unit — the **Kirov Airship
(`[ZEP]`)** — and gated by the **10th permanent byte-identity gate
`--demo-aircraft`**. This is **CB-2**, the second content-breadth phase (CB-1
Transports/Cargo → **CB-2 Aircraft** → CB-3 Naval → CB-4 Superweapons → CB-5
Tech-tree) and the first of the two new-locomotor phases (CB-3 Naval is the
other): it establishes the **altitude dimension** the engine currently lacks.
Like cargo, flight lives in the CORE unit sim — it cannot hide behind the
`ai_any_` firewall — so the byte-identity discipline (§8) is itself a
first-class design deliverable. One spec→plan→execute cycle, ~7 tasks, the
same spine as every prior phase.

## Recovered exemplar ground truth (committable — read from the real install)

The Kirov's rules id is **`[ZEP]`** (NOT "kirov"); its art id is **`ZEP`**
(`artmd.ini [ZEP] Voxel=yes`, `PrimaryFireFLH=-50,0,-140`). Verified on this
install: `zep.vxl` (177,637 B) and `zep.hva` both PRESENT in
`ra2.mix → local.mix` — renderable through the existing voxel_raster/
facing_cache pipeline exactly like the Rhino. Values read from the real
`localmd.mix → rulesmd.ini`/`artmd.ini` (committable ground truth; their
INTERPRETATION — the locomotor model, the altitude/speed/shadow FORMULAS — is
the §1 hunt's):

- **`[ZEP]`**: `Strength=2000`, `Armor=medium`, `Speed=5`, `ROT=10`,
  `SpeedType=Hover`, `MovementZone=Fly`,
  `Locomotor={92612C46-F71F-11d1-AC9F-006008055BB5}` (**Jumpjet**),
  `JumpjetSpeed=5`, `JumpjetClimb=6`, `JumpjetCrash=12`, `JumpJetAccel=10`,
  `JumpJetTurnRate=2`, `JumpjetHeight=750`, `JumpjetNoWobbles=yes`,
  `BalloonHover=yes` ("ie never land"), `ConsideredAircraft=yes`,
  `Crashable=yes`, `PitchSpeed=.9`, `PitchAngle=0`, `Size=50`, `Cost=2000`,
  `Primary=BlimpBomb`, `ElitePrimary=BlimpBombE`, `Owner=Russians,…`.
  **No `Ammo=` key** → continuously armed, no airfield/rearm cycle (the reason
  the controller chose it — see Out of scope).
- **`[BlimpBomb]`** (its Primary): `Damage=250`, `ROF=50`, `Range=1.5`,
  `Burst=1`, `Projectile=BlimpBombP`, `Speed=20`, `Warhead=BlimpHE`,
  `OmniFire=yes` ("Don't need to turn even though I have no turret"),
  `CellRangefinding=yes`.
- **Jumpjet `[General]` defaults** (used when a `[ZEP]`-style block omits a
  key; ZEP overrides all of them): `JumpjetSpeed=30`, `JumpjetClimb=20`,
  `JumpjetCrash=25`, `JumpJetAccel=10`, `JumpJetTurnRate=10`,
  `JumpjetHeight=500`.

**Design consequence — the exemplar is a JUMPJET voxel vehicle, not a
`[AircraftTypes]` fixed-wing flyer.** The Kirov is a VehicleType carrying the
JumpjetLocomotionClass, hovering at `JumpjetHeight` and never landing
(`BalloonHover=yes`), firing a downward-FLH bomb (`PrimaryFireFLH=-50,0,-140`,
H negative = below the hull) while stationary over the target (`OmniFire` — no
turn required). CB-2 therefore models the **Jumpjet flight model** as its
first flight locomotor. The `[AircraftTypes]` proper (`APACHE`, `ORCA`,
`BEAG`, …) use the sibling **FlyLocomotionClass** with `Ammo=`/airfield rearm
and are DEFERRED (§ Out of scope); the hunt maps that sibling so a later phase
starts from a mapped point.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Flight model | **Straight-line travel at cruise altitude ignoring ground pathing/occupancy** (no A*, no `passable_` test, no `occ_` reservation while airborne). Facing turns toward the travel direction at the type's turn rate. The exemplar uses the **Jumpjet locomotor**; the exact per-frame movement + turn model (JumpjetSpeed vs Speed, `JumpJetTurnRate` vs `ROT`, straight-line integration) is §1 item 1 |
| Altitude | An **`altitude` field on the `Unit`** (leptons). **Takeoff** ascends ground→cruise at `JumpjetClimb`; **descend** toward a landing/target at `JumpjetCrash` (or a bounded descent for attack). Cruise = `JumpjetHeight=750` leptons for ZEP. Rates/behavior are §1 item 2 — pre-hunt expectations |
| Altitude render | The flyer is drawn **raised** by a screen y-offset proportional to `altitude`, reusing the voxel raster + the existing `level*level_px` vertical-offset mechanism; a **ground shadow** is drawn at the ground cell (altitude 0). Offset formula + shadow convention are §1 item 3 (fall back to the documented §5 approximation if unpinnable) |
| Air-to-ground | The flyer acquires a GROUND target, flies to within weapon `Range` (BlimpBomb `Range=1.5`), and fires from cruise via the EXISTING `commit_attack_target`/`try_fire` path (OmniFire → no turret aim gate). Fire-from-cruise vs descend-to-attack + weapon delivery are §1 item 4; a **simplified direct air-to-ground weapon** (instant single-target damage, as ground vehicles already use) is acceptable — exact bomb ballistics deferred |
| Exemplar | **Kirov Airship `[ZEP]`** (Soviet, image `ZEP`, art PRESENT). Art-guarded; if ZEP art ever regresses, the guard drops to another present flyer (`ORCA`/`BEAG` art verified present) with the demo script adjusted, but the flight logic is exemplar-independent and covered by pure tests regardless |
| Spawn | CB-2 **spawns the exemplar directly** (like CB-1 spawned the HTK); no airfield build-from-sidebar. Production-queue wiring is a carry-forward |
| Verification | **New deterministic gate `--demo-aircraft`** (the 10th) + `aircraft_hash` (FNV over flight path + altitude + attack state across takeoff→fly→attack→(descend), two-run compute-and-self-compare, NO pinned literal) + **wedge guards (§7): a takeoff AND airborne flight AND an air-to-ground hit must actually happen, else exit 1** |
| Byte-identity | **Strategy (a)**: flight fields excluded from `world_hash` (untouched); a new `step_flight` early-returns behind an `aircraft_any_` firewall; every new guard in existing movement/combat loops is an added clause provably false in every existing demo (§8). Ground-unit movement/occupancy stays bit-identical |

## §1 What the Ghidra hunt must recover (→ `docs/re/aircraft.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001
Ghidra project at `D:\ra2-re`, headless `analyzeHeadless.bat`,
`JAVA_HOME="C:\Program Files\Java\jdk-21"`) plus the real rulesmd/artmd,
producing `docs/re/aircraft.md` — the calibration authority BEFORE any code,
with a located-functions table, a "deviations we take" section (mirroring
`voxel-render.md`/`transport-cargo.md`), and each item landing CONFIRMED /
PROBABLE / [RE-FALLBACK] with function address + decompile reasoning. Anchor
on: `JumpjetLocomotionClass` / `FlyLocomotionClass` and the locomotor CLSID
vtables (the **Jumpjet CLSID `{92612C46-F71F-11d1-AC9F-006008055BB5}`** is
CONFIRMED from `[ZEP] Locomotor=` and is the primary entry map; recover the
Fly CLSID for the deferred sibling), `AircraftClass`/`AircraftTypeClass`, the
INI-key strings the binary loads (`"JumpjetHeight"`, `"JumpjetSpeed"`,
`"JumpjetClimb"`, `"JumpjetCrash"`, `"JumpJetAccel"`, `"JumpJetTurnRate"`,
`"BalloonHover"`, `"MovementZone"`, `"SpeedType"`, `"Ammo"`, plus the already-
pinned `"Speed"`/`"ROT"` from `voxel-render.md` §5), the height/altitude field
on the object, the air-to-ground mission/attack path, and the ground-shadow
draw (`voxel-render.md` names the shadow renderer `FUN_00755680` — "plot color
1, no z-test" — and the flush `FUN_00753110`'s "draw shadow blobs" step as the
entry map). YRpp/Ares addresses are UNTRUSTED prose hints only — content-
anchor everything. **Do NOT guess numeric constants (altitude→pixel offset,
cruise speed, turn rate, shadow offset)** — they are hunt outputs. Six items:

1. **Flight movement model**: straight-line to target vs pathed (expected:
   straight-line, no A*/`passable_`); the per-frame cruise speed and its
   conversion (does the Jumpjet use `JumpjetSpeed` or the base `Speed`? the
   `voxel-render.md` §5 `min(255, clamp(v,0,100)·256/100)` leptons/frame
   conversion, or a jumpjet-specific one?); the turn/heading model
   (`JumpJetTurnRate` vs `ROT`, and whether heading must complete before
   translating — the `BalloonHover`/`OmniFire` case may not turn at all); and
   the CONFIRMATION that an airborne unit reserves NO ground cell (holds no
   `occ_` slot, is not on the pathing grid). Which locomotor the exemplar uses
   (Jumpjet — CONFIRMED from rules; pin the vtable/Process function).
2. **Altitude**: the cruise-altitude constant(s) and how `JumpjetHeight=750`
   is stored/consumed (leptons? a z-coord on the object?); **takeoff** ascend
   rate (`JumpjetClimb`) and **descend** rate (`JumpjetCrash`); `JumpJetAccel`/
   `PitchSpeed` roles; where altitude lives on the object; and the
   `BalloonHover` "never land" branch (the exemplar takes off once and holds
   cruise — confirm there is no land cycle for it).
3. **Altitude→screen render offset + ground-shadow convention**: the height→
   pixel factor (pre-hunt fallback from `voxel-render.md` §6: the up/z term is
   ≈ **36.74 px per cell-of-z**, so `altitude_px ≈ (altitude/256)·36.74` →
   ~107 px raised at `JumpjetHeight=750`); how/where the shadow is drawn
   (`FUN_00755680` plots the model silhouette in color 1 at the un-raised
   ground projection with no z-test), and the shadow's cell/offset anchor.
4. **Air-to-ground attack**: fire-from-cruise vs descend-to-attack (expected
   fire-from-cruise given `BalloonHover`+`OmniFire`+downward FLH); the weapon
   FLH/delivery (`[ZEP] PrimaryFireFLH=-50,0,-140`, H below the hull → the
   bomb falls); `BlimpBomb Projectile=BlimpBombP Speed=20` (falling projectile
   — CB-2 may model it as an instant single-target hit, the established
   projectile simplification, `voxel-render.md` §7); and CONFIRM the exemplar
   needs no `Ammo`/airfield dock (rules already show no `Ammo=` key — pin the
   `Ammo` default = unlimited so the "must land to rearm" branch is provably
   not taken).
5. **Exemplar stats interpretation**: the `[ZEP]`/`[BlimpBomb]` VALUES above
   are ground truth already; the hunt pins the FORMULAS that turn them into
   engine constants (speed→leptons/frame, ROT/`JumpJetTurnRate`→u16/frame per
   `voxel-render.md` §5, `JumpjetHeight`→altitude leptons→pixels, `Damage`/
   `Warhead`→`combat.md` damage math which already handles this).
6. **Determinism + byte-identity**: how airborne units integrate into the unit
   step WITHOUT touching ground-unit movement (expected: a separate
   `step_flight` pass gated by `is_aircraft`, mirroring CB-1's `step_cargo`);
   any deterministic ordering the sim relies on for aircraft (step order vs
   `step_combat`); and confirmation that no RNG (wobble/scatter) is on the
   critical path (`[ZEP] JumpjetNoWobbles=yes` removes the sinusoidal wobble —
   good for determinism; `BallisticScatter`/`JumpjetWobbles` stay out).

Calibration discipline (unchanged): every engine constant cites this doc.
**All numeric semantics in this spec are pre-hunt expectations, not ground
truth** (the rulesmd/artmd VALUES quoted above were read from the real files
on this install and are committable; their INTERPRETATION is the hunt's). A
controller **RE-GATE** amendment step precedes implementation — the hunt's
findings amend the plan and are committed BEFORE dependent dispatch, with
`[RE-GATE APPLIED]` markers threaded through every dependent task (the CB-1
`transport-cargo.md` precedent). **[RE-FALLBACK]**: anything the hunt cannot
pin becomes a plan-amendment gate (present options to the human) — never an
uncalibrated guess. The altitude→pixel and shadow conventions are the most
likely [RE-FALLBACK]s; §5 documents the fallback approximations to use.

## §2 Architecture & recovered data model

Consistent with every prior phase: a pure SDL-free engine module + thin World
integration + a deterministic demo in main.cpp. ~7 tasks:

- **Task 1**: Ghidra hunt → `docs/re/aircraft.md` (RE-GATE before
  implementation; §1 above).
- **Task 2**: pure engine `engine/flight.{hpp,cpp}` — the straight-line
  cruise step, the altitude climb/descend integrator, and the
  heading-toward-travel helper — over plain `Unit` fields, doctest-covered on
  hand-built units, no World/SDL.
- **Task 3**: World integration — `Unit` flight fields, `order_move` for an
  aircraft routing to `step_flight` (not A*), the takeoff→cruise→descend
  state machine, the airborne no-occupancy guards (§4), and the added
  `|| u.is_aircraft` clause on the ground-movement sites.
- **Task 4**: air-to-ground combat — an aircraft with a ground target flies to
  within `Range` via `step_flight` and fires through the EXISTING
  `step_combat`/`try_fire` path (§6); the `commit_attack_target` acquire path
  grows one aircraft-aware clause.
- **Task 5**: render — the altitude screen offset + the ground shadow (§5),
  the `ZEP` roster/art load (art-guarded), rules resolution of the flight
  stats. Reuses `draw_vehicle_unit`/`draw_voxel_sprite`.
- **Task 6**: `--demo-aircraft` gate (the 10th) + `aircraft_hash` + witness
  lines + wedge guards (§7).
- **Task 7**: finalize — suites both modes, 9 existing gates byte-identical,
  arm64 NDK nm-verify of the new flight symbols, README (CB-2 section + 10th
  gate row + deviations note citing `docs/re/aircraft.md`).

**Decomposition tripwire**: if the altitude-render/shadow work (§5) or the
takeoff/descend state machine fans out past the enumerated sites and pushes
the plan past ~9 tasks, the phase splits at the natural seam — **(A) flight-
locomotor + altitude + render foundation** (Tasks 1–3, 5; a gate that only
takes off, cruises, and lands, no combat) then **(B) the air-to-ground combat
exemplar** (Tasks 4, 6). Default is the single coherent phase below.

### Data model

Field shapes below are the approved design; the Task 1 hunt CONFIRMS or
REPLACES the semantics (RE-GATE), and code/tests are written against the
amended form.

- **`UnitCombat` extension** (resolved once from Rules by main.cpp, like every
  existing stat): `is_aircraft` (true for a flight-locomotor type),
  `cruise_altitude` (leptons; `JumpjetHeight`), `climb_rate` / `descend_rate`
  (leptons/frame; `JumpjetClimb`/`JumpjetCrash`), `flight_speed` (leptons/
  frame; the recovered Jumpjet speed), `flight_turn` (u16/frame; the recovered
  turn rate), `balloon_hover` (never lands). Recovered defaults when a key is
  absent are §1 item 2.
- **`Unit` extension** (all defaults INERT — §8): `is_aircraft = false`,
  `altitude = 0.0` (leptons; 0 = on the ground), `flight_state = Grounded`
  (enum `Grounded / TakingOff / Cruising / Descending`), `cruise_altitude = 0`,
  `climb_rate = 0`, `descend_rate = 0`, `flight_speed = 0.0`,
  `flight_turn = 0.0`, `balloon_hover = false` (copied from UnitCombat at
  spawn). No stored screen-offset — it is DERIVED from `altitude` at draw time.
- **Pure module `engine/flight.{hpp,cpp}`** (registered in BOTH
  `cmake/ra2_sources.cmake` AND `CMakeLists.txt`): `step_altitude(...)`
  (integrate `altitude` toward the state's target at climb/descend rate,
  clamped), `cruise_step(...)` (advance `fx/fy` in a straight line toward the
  goal at `flight_speed`, returning arrival), `heading_toward(...)` (turn
  `body_facing` toward the travel/target direction at `flight_turn`), and
  `altitude_to_screen_dy(altitude)` (the §5 pixel offset). All doctest-testable
  on hand-built Units; every recovered constant cites `docs/re/aircraft.md`.
- **Unit indices as links**: `target`/`target_building` already store indices;
  flight adds no new cross-unit links.

## §3 Flight movement — straight-line at altitude

Airborne movement is a SEPARATE mode from ground movement; the two never share
a step. An aircraft never enters A*, never tests `passable_`, and never
reserves an `occ_` cell while airborne.

1. **Order**: `World::order_unit_move(idx, gx, gy)` on an `is_aircraft` unit
   sets a flight goal (goal cell + `flight_state = TakingOff` if `Grounded`,
   else `Cruising`) instead of computing an A* path — the ONE aircraft-aware
   branch in `order_unit_move`. Ground units take the exact existing A* path
   (branch not taken).
2. **Takeoff** (`step_flight`, state `TakingOff`): release the unit's ground
   `occ_` cell (once), ascend `altitude` toward `cruise_altitude` at
   `climb_rate`; on reaching cruise, transition to `Cruising`. The unit does
   not translate horizontally until airborne (pre-hunt; §1 item 2 confirms
   whether climb and travel overlap).
3. **Cruise** (state `Cruising`): each frame, turn `body_facing` toward the
   goal direction at `flight_turn` (skipped when `OmniFire`/no-turn — §1 item
   1), then advance `fx/fy` in a straight line toward the goal at
   `flight_speed` (no cell reservation, no terrain test). `cell_x/cell_y` are
   updated to `round(fx)/round(fy)` for the ground-shadow anchor and for
   combat range/targeting, but claim NO occupancy. On arrival, hold position
   at cruise (the exemplar hovers — `BalloonHover`).
4. **Descend** (state `Descending`): only reached by an explicit land order or
   a descend-to-attack model (§6). For the `BalloonHover` exemplar this is
   effectively unused (it never lands); it exists so the state machine is
   complete and the recovered non-hover behavior (§1 item 2) has a home. CB-2
   ships the recovered DEFAULT; a full land-cycle is a carry-forward.
5. **Determinism**: `step_flight` runs as a NEW pass in `World::step`,
   inserted right after `step_cargo` (before `step_turrets`/`step_combat`) so
   the aircraft's post-move altitude/position is visible to combat targeting
   the same frame — a pure INSERTION, no existing step reordered. Gated by
   `if (!aircraft_any_) return;` (§8). `JumpjetNoWobbles=yes` removes the
   sinusoidal wobble, so no RNG is on the flight path.

## §4 Altitude & no-occupancy — the byte-identity of ground units

An airborne unit (`is_aircraft && altitude > 0` — more precisely
`flight_state != Grounded`) is OFF the ground grid. The rule is uniform and
mirrors CB-1's off-map carry guard: **every ground-movement site that already
skips corpses/carried units grows ONE clause for airborne units**, provably
false for every unit in every existing demo (§8). Enumerated sites (the plan's
Task 3 checklist; the implementer sweeps for stragglers):

- `step_units` — the corpse-skip at world.cpp:2107
  (`if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;`) grows
  `|| u.is_aircraft` so airborne units never run ground movement/occupancy;
- `occ_` — an airborne unit holds no cell: takeoff releases it, and
  `step_flight` never calls `occ.set` for an airborne unit;
- pathing / `passable_` — never consulted for flight (no A* is computed);
- `step_combat` targeting BOTH directions — an aircraft CAN be a target only if
  ground AA is modeled (out of scope, §Out); pre-hunt, ground units do NOT
  auto-acquire the aircraft (an added `|| !is_aircraft` clause on the
  auto-acquire candidate test), and the aircraft attacks only ground targets
  (§6) — this keeps air-to-air out and keeps every existing demo's targeting
  bit-identical (no aircraft present there);
- `unit_at_cell` / `Input::unit_under` — an airborne unit is pickable at its
  RAISED screen position, not its ground cell; selection uses the raised
  hitbox (a render/input concern, not sim);
- harvester/AI recruitment pickers — an `is_aircraft` unit is never a
  harvester and (CB-2) never AI-fielded, so these are data-unreachable, guarded
  anyway.

**`cell_x/cell_y` while airborne track the ground projection** of `fx/fy`
(round), used ONLY for the shadow anchor and combat range — never for
occupancy. This is the deliberate analogue of CB-1's "coords stale while
carried": here coords are LIVE (the shadow must follow) but claim no cell.

## §5 Altitude rendering + ground shadow

- **Raised sprite**: the flyer's voxel sprite is drawn at its normal ground
  screen anchor MINUS a vertical offset `altitude_to_screen_dy(altitude)`,
  reusing `draw_vehicle_unit`/`draw_voxel_sprite` and the SAME
  `level*level_px` up-shift mechanism the terrain-height path already uses
  (`iso_project.cpp` subtracts `level*level_px` from screen y). Pre-hunt
  offset (fall back to this if §1 item 3 is [RE-FALLBACK]): the voxel up/z term
  is ≈ **36.74 px per cell-of-z** (`voxel-render.md` §6), so
  `dy = (altitude/256) · 36.74` → ~107 px raised at `JumpjetHeight=750`.
- **Ground shadow**: drawn at the un-raised ground cell (altitude 0), BELOW all
  objects (in the same pass ordering as the sprite's ground key), as the
  flyer's voxel silhouette in a dark/translucent color — the CB-2 approximation
  of the original's `FUN_00755680` shadow-blob (which plots the model's column
  silhouette in color 1 with no z-test). Recover the exact offset/color from
  the hunt; the approximation is a filled silhouette (or the hull sprite
  darkened) at the ground projection. This is NEW render work (the engine's
  `voxel_raster` does not yet emit shadows).
- **Guarded**: the raised-draw + shadow path is taken ONLY for
  `is_aircraft && altitude > 0` units. No existing demo spawns an aircraft, so
  no existing screenshot draws either path (§8).
- **HUD reuse only**: selection diamond / HP bar size to the hull sprite width
  exactly as ground vehicles (`veh_width`), positioned at the raised anchor. No
  new font/UI systems.

## §6 Air-to-ground combat

Composes on the existing combat path with no new damage math:

1. **Acquire**: `commit_attack_target` on an aircraft sets `target` (a ground
   unit) or `target_building`, exactly as a ground vehicle. Auto-acquire of
   ground targets by an idle airborne unit is allowed; auto-acquire OF the
   aircraft by ground units is suppressed (§4) — air-to-air is out.
2. **Approach**: `step_combat` emits its usual approach `MoveRequest` when the
   attacker is out of range. In `World::step`, an approach request for an
   `is_aircraft` unit routes to the flight goal (sets the cruise goal to the
   target's cell) instead of `order_unit_move`'s A* — the one aircraft-aware
   branch in the MoveRequest dispatch. The flyer cruises straight to within
   `Range` (BlimpBomb `Range=1.5` — it flies nearly over the target).
3. **Fire**: once `cell_x/cell_y` is within `w_range` of the target,
   `try_fire` applies damage through the EXISTING deterministic path
   (`combat.md` Damage×Verses×ROF). `OmniFire=yes` means no turret aim gate
   (ZEP has no turret) — the aircraft fires without turning. Fire is
   **from cruise** (pre-hunt; §1 item 4); the muzzle/bomb FLH uses the
   recovered `PrimaryFireFLH=-50,0,-140` (H below the hull) through the
   existing FLH render path.
4. **Weapon delivery**: CB-2 uses the established **instant single-target
   hit** (the same simplification ground vehicles already ship,
   `voxel-render.md` §7 — projectile flight time skipped). The real
   `BlimpBombP` falling bomb + `BlimpHE CellSpread` is a documented
   carry-forward, not CB-2 code.

## §7 The `--demo-aircraft` gate (the 10th), `aircraft_hash` & witnesses

**Script** (headless or rendered — bit-identical): on the standard demo map,
spawn one Kirov `[ZEP]` (art-guarded; flyer-fallback documented) for house 0
and one hostile ground target (a Conscript or a Soviet building) for house 1,
all scripted (no player input):

1. **TAKEOFF**: the Kirov starts `Grounded`; issue a move → it releases its
   ground cell, ascends to `cruise_altitude` (witness per phase:
   `[air] takeoff alt=A/750 cell released`), transitions to `Cruising`.
2. **FLY**: order it to a distant cell; it cruises in a straight line at
   altitude, reserving no cell, passing OVER an obstacle a ground unit could
   not cross (`[air] cruise arrived=(x,y) alt=750 occ_held=0`); assert no
   `occ_` cell is held while airborne (occupancy audit folds into the hash).
3. **ATTACK**: order it to attack the ground target; it flies to within
   `Range` and fires; the target takes damage and dies (`[air] hit target=N
   dmg=250`, `[air] target_dead=N`).
4. **RESUME/(DESCEND)**: run a fixed window; the Kirov holds cruise
   (`BalloonHover`) — or, if the recovered model lands, descends and the
   descent is witnessed.

**`aircraft_hash`**: FNV-1a 64 (the established `fnv_mix` helper) folded at
fixed checkpoints (post-takeoff, post-fly, post-attack, end) over: every
unit's `(cell_x, cell_y, alive, hp)` PLUS each aircraft's
`(round(altitude), flight_state, target)` and the occupancy-held count for
airborne units (must be 0) — exactly the state the phase adds. Two-run
compute-and-self-compare (`deterministic=yes`), NO pinned literal in code (the
`--demo-ai-team`/`--demo-transport` precedent); README records the stable
value in the gate table once landed.

**Wedge guards** (exit 1, so the gate can never pass vacuously): takeoff
observed (altitude went 0 → cruise) AND airborne cruise observed (a frame with
`altitude > 0` and zero `occ_` cells held) AND at least one air-to-ground hit
landed on a ground target — all three must hold, else exit 1. Witness log +
hash are pure sim state → rendered and headless runs byte-compare identical.

## §8 Byte-identity discipline (core-sim — the phase's hardest constraint)

The **9 existing permanent gates MUST stay bit-identical** (values in
Depends-on). Flight cannot hide behind `ai_any_` — the discipline instead:

- **All new Unit fields default INERT**: `is_aircraft = false`,
  `altitude = 0.0`, `flight_state = Grounded`, all flight rates 0,
  `balloon_hover = false`. No existing demo spawns an aircraft and no existing
  spawn path sets `is_aircraft = true`, so every field holds its default in
  every existing demo.
- **`world_hash` strategy (a) — EXCLUDE, chosen**: `world_hash` reads exactly
  `(cell_x, cell_y, hp, alive)` per unit + the building fields (main.cpp
  world_hash) and is NOT touched by this phase; the new flight fields are
  hashed ONLY by `aircraft_hash` in the new gate. Justification over strategy
  (b): (a) requires zero edits to a function five gates hash through, making
  the no-change argument structural. Note: `cell_x/cell_y` ARE hashed and ARE
  live for an airborne unit — but only aircraft have `altitude>0`, and no
  existing demo has an aircraft, so those coords are default-driven exactly as
  before in every existing demo.
- **`step_flight` is firewalled**: a World-level `aircraft_any_` bool, set the
  first time an aircraft is spawned/ordered, gates the entire function —
  `if (!aircraft_any_) return;` is the only cost every existing demo pays. It
  is inserted as a NEW call between `step_cargo` and `step_turrets` in
  `World::step` (§3) — a pure insertion; **no existing unit-step logic is
  reordered**.
- **Every §4 guard is an added clause** (`|| u.is_aircraft`, or
  `|| !is_aircraft` on an auto-acquire candidate) on an existing corpse/carry-
  skip condition — provably false (or the negation provably true) for every
  unit in every existing demo (default `is_aircraft=false`), so control flow is
  identical instruction-for-instruction where it matters.
- **The §5 render paths are guarded** on `is_aircraft && altitude > 0`; no
  existing demo spawns an aircraft, so no existing screenshot changes.
- **The 9-gate re-verify runs at the first additive commit (Task 3) AND at
  finalize** (Task 7), isolating any regression to the commit that introduced
  it — the CB-1 discipline.

## §9 Testing strategy

- **Pure doctests `tests/test_flight.cpp`** (host-only, hand-built Units, no
  install): `step_altitude` — ascends at `climb_rate` to exactly
  `cruise_altitude` and stops (no overshoot), descends at `descend_rate` to 0;
  `cruise_step` — straight-line advance toward a goal at `flight_speed`,
  arrival exact, NEVER consulting `passable_`/`occ_` (the function does not
  take them); `heading_toward` — shortest-arc turn at `flight_turn`, no-turn
  when OmniFire; `altitude_to_screen_dy` — monotone, 0 at altitude 0, the
  recovered factor at cruise; the flight state machine —
  Grounded→TakingOff→Cruising transitions on the altitude/arrival predicates.
- **World-level tests** (the existing test_world pattern): takeoff releases the
  ground `occ_` cell and the one-unit-per-cell audit stays green with the
  aircraft airborne; an airborne unit holds NO cell and a ground unit may
  occupy the cell beneath it; cruise crosses a `passable_=0` / occupied region
  a ground unit cannot; an aircraft is skipped by `step_units` ground movement;
  air-to-ground fire damages/kills a ground target via `try_fire`; ground units
  do NOT auto-acquire the aircraft.
- **Integration**: the `--demo-aircraft` gate (§7) with wedge guards + two-run
  self-compare; rendered vs headless byte-identical.
- **Regression**: all 9 existing gates byte-identical (values in Depends-on),
  re-run at the first additive commit AND at finalize; full suite both modes;
  arm64 NDK build with the new flight symbols nm-verified. Suite stays
  scoped/fast (the shared-fixture discipline).

## Out of scope / divergences (explicit)

- **Airfield/airpad dock + Ammo rearm cycle** (fixed-wing `[AircraftTypes]`
  like Harrier/Black Eagle/`ORCA`/`BEAG` that must land to reload) — DEFERRED.
  The exemplar (`[ZEP]`, no `Ammo=`, `BalloonHover=yes`) needs no rearm; the §1
  hunt maps the `FlyLocomotionClass`/`Ammo`/airfield-dock mechanism so a later
  phase starts from a mapped point. CB-2 ships no rearm.
- **Air-to-air combat / aircraft targeting other aircraft, and anti-air
  weapons vs air** — out. Ground AA vs air is a documented carry-forward (§4
  suppresses ground auto-acquire of the aircraft rather than building AA).
- **Jumpjet infantry** (Rocketeer/`Floating Disc`), **paradrop/transport
  aircraft**, **carryall/chrono-tank-lift** — out. (Note: the exemplar itself
  is a Jumpjet VEHICLE; the deferral is jumpjet INFANTRY and delivery craft.)
- **Exact bomb ballistics / falling-projectile physics** — a simplified direct
  instant-hit air-to-ground weapon is used (§6); the real `BlimpBombP` falling
  bomb + `BlimpHE CellSpread` is a documented carry-forward.
- **Aircraft production from an airfield / build-from-sidebar** — CB-2 spawns
  the exemplar directly (like CB-1's transport); production-queue wiring is a
  carry-forward.
- **AI use of aircraft** — the AI does not field aircraft in CB-2
  (carry-forward); the aircraft-related ScriptType opcodes stay inert in the
  AI-2 interpreter.
- **Full land-cycle / hover wobble** — `BalloonHover=yes` + `JumpjetNoWobbles=
  yes` mean the exemplar takes off once and holds cruise; the descend/land
  state exists but is unused by the exemplar (§3.4), and the sinusoidal
  jumpjet wobble is intentionally omitted (determinism).
- **Slope/pitch tilt of the airborne sprite** (`PitchSpeed`/`PitchAngle`) —
  like the ground voxel "no slope tilt" divergence (`voxel-render.md` §5), the
  flyer renders level; pitch is a carry-forward.

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real
  rulesmd / prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE
  precede all dependent tasks; every recovered constant cites
  `docs/re/aircraft.md`; [RE-FALLBACK] items become plan-amendment gates,
  never guesses.
- NINE byte-identity gates stay green (§8); `--demo-aircraft` becomes the 10th
  permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the
  install. Long rendered demos auto-background on short timeouts; NEVER pipe a
  native exe through `Select-Object -First N` (stops draining stdout → false
  hang).
- No game assets / GPL source / Ghidra output committed; recovered numeric
  constants ARE committable (the `[ZEP]`/`[BlimpBomb]` VALUES above already
  are).
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs —
  the plan's briefs must order FOREGROUND synchronous runs with generous
  timeouts.

## Testing summary

| Layer | Tests |
|---|---|
| engine/flight (pure) | `step_altitude` (ascend/descend to exact bound, no overshoot); `cruise_step` (straight-line, arrival exact, never touches passable_/occ_); `heading_toward` (shortest-arc turn, no-turn on OmniFire); `altitude_to_screen_dy` (monotone, 0 at 0, recovered factor at cruise); state machine Grounded→TakingOff→Cruising |
| World integration | takeoff releases ground occ_ cell (audit green); airborne unit holds no cell, ground unit may sit beneath; cruise crosses impassable/occupied region; aircraft skipped by step_units ground movement; air-to-ground hit damages/kills ground target; ground units do NOT auto-acquire the aircraft |
| integration | `--demo-aircraft` witnesses (takeoff → cruise-with-occ_held=0 → air-to-ground hit → resume/descend) + aircraft_hash two-run self-compare (no pinned literal) + wedge guards (takeoff AND airborne flight AND ≥1 air-to-ground hit else exit 1); rendered ≡ headless |
| regression | nine existing gates byte-identical (§8), re-verified at the first additive commit and at finalize; full suite both modes; arm64 nm-verify of flight symbols |
