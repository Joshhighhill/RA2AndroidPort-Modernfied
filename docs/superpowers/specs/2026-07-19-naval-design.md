# CB-3 Naval & Water-Movement Phase Design — Ships Path and Fight on Water

**Date:** 2026-07-19
**Status:** Approved (controller-scoped, formalized autonomously)
**Depends on:** the complete AI arc, Unit Production, Economy, Buildings,
combat, the voxel-vehicle render pipeline, CB-1 Transports/Cargo, and CB-2
Aircraft/Flight — the **ten** permanent byte-identity gates this phase must keep
bit-identical: `--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a,
`--demo-ai-team` ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc, `--demo-transport` transport_hash=fe500f946e913645,
`--demo-aircraft` aircraft_hash=a69642564cf64773. Reuses the voxel raster +
`facing_cache` (`docs/re/voxel-render.md`) for ship art, the
`commit_attack_target`/`step_combat`/`try_fire` path (`docs/re/combat.md`) for
naval fire, the existing A* + occupancy (`engine/pathfind.cpp`, `engine/occupancy.*`)
for water pathing, and CB-2's inert-field / added-clause byte-identity discipline
(`aircraft-design.md` §8) as the model for adding a CORE-SIM movement domain
without perturbing ground units.

## Goal

Introduce the **WATER movement domain**: naval units that **pathfind and move on
WATER cells via the existing A\*** (cell-to-cell, one-ship-per-cell occupancy —
the SAME locomotion shape as ground units, unlike CB-2's straight-line flight),
are **BLOCKED from land** (the inverse of ground units), and **acquire and attack
a target from the water**, composing on the existing combat path. Demonstrated
deterministically on one real Soviet surface warship — the **Sea Scorpion
(`[HYD]`)** — and gated by the **11th permanent byte-identity gate `--demo-naval`**.
This is **CB-3**, the third content-breadth phase (CB-1 Transports ✓ → CB-2
Aircraft ✓ → **CB-3 Naval** → CB-4 Superweapons → CB-5 Tech-tree) and the second
of the two new-locomotor phases: it establishes the **water/land passability
dimension** the engine currently lacks.

Unlike flight — which is a wholly separate step (straight-line, no A*, no
occupancy) that could be firewalled behind `aircraft_any_` — naval movement
**reuses the ground movement machinery** (A*, `occ_`, `step_units`) but on a
DIFFERENT passability grid. So this phase's byte-identity constraint (§8) lands
squarely on the PATHFINDER itself, the hardest surface touched to date: the
walkability test that feeds A* must be **parameterized by a movement domain** such
that ground/Drive units resolve to the EXACT existing grid, byte-for-byte, while
naval units resolve to a new water grid. One spec→plan→execute cycle, ~8 tasks,
the same spine as every prior phase.

## Recovered exemplar ground truth (committable — read from the real install)

Read from the real `ra2md.mix → localmd.mix → rulesmd.ini`/`artmd.ini` on this
install (committable ground truth; their INTERPRETATION — the locomotor model,
the passability semantics, the speed/damage FORMULAS — is the §1 hunt's):

- **`[HYD]` (Sea Scorpion, `[VehicleTypes]` id 39, Soviet):** `Name=Sea Scorpion`,
  `Naval=yes`, `IsSelectableCombatant=yes`, `Strength=400`, `Armor=heavy`,
  `Speed=8`, `ROT=6`, `Sight=8`, `Cost=600`, `Weight=2`, `TechLevel=6`,
  `Prerequisite=NAYARD,NARADR`, `Owner=Russians,Confederation,Africans,Arabs`,
  `Primary=FlakTrackGun`, `Secondary=FlakWeapon`, `Turret=no`,
  `MovementRestrictedTo=Water`, **`SpeedType=Float`**, **`MovementZone=Water`**,
  **`Locomotor={2BEA74E1-7CCA-11d3-BE14-00104B62A16C}`** (a DISTINCT naval
  LocomotionClass CLSID — NOT the ground Drive loco; the trailing
  `;{4A582741-9839-11d1-B709-00A024DDAFD1}` on the line is an INI comment, i.e.
  commented-out). No `Spawns=`, no `Underwater=`/cloak → a plain surface
  combatant with a direct-fire weapon (the reason it is chosen over the
  spawner ships and the cloaking sub — see Out of scope).
- **`[FlakTrackGun]`** (its Primary, "Anti-surface gun"): `Damage=25`, `ROF=25`,
  `Range=5`, `Projectile=FlakTProj`, `Speed=50`, `Warhead=FlakTWH`, `Anim=GUNFIRE`.
- **`artmd.ini [HYD]`**: `Voxel=yes`, `Remapable=yes`, `Cameo=HOVRICON`,
  `PrimaryFireFLH=65,0,180`, `SecondaryFireFLH=65,0,180`. Verified PRESENT in
  `ra2.mix → local.mix`: `hyd.vxl` (25,984 B) and `hyd.hva` (88 B) — renderable
  through the existing voxel_raster/facing_cache pipeline exactly like the Kirov
  (`zep.vxl`) and Rhino.
- **The land-type × SpeedType speed table** (rulesmd.ini `[Clear]`/`[Water]`/
  `[Beach]`/`[Rock]`/… sections — the passability matrix ground truth; a value of
  **0% = impassable** for that SpeedType, > 0% = passable):

  | Land type | Foot | Track | Wheel | Float | FloatBeach | Hover | Amphibious |
  |---|---|---|---|---|---|---|---|
  | **Clear**  | 100 | 100 | 100 | **0**   | 0   | 50 | 80 |
  | **Water**  | **0** | **0** | **0** | **100** | 100 | 100 | 100 |
  | **Beach**  | 0   | 0   | 0   | **0**   | 100 | 75 | 60 |
  | **Rock**   | 0   | 0   | 0   | 0       | 0   | 0  | 0  |
  | **Ice**    | 50  | 80  | 50  | 0       | 0   | 100 | 50 |
  | **Road**   | (≥100 — read at hunt) | … | … | 0 | 0 | … | … |

  A ground unit (Foot/Track/Wheel) has **0% on Water** → blocked from the lake;
  a **Float** naval unit has **100% on Water, 0% on Clear/Beach/Rock** → confined
  to open water, blocked from land AND beach. This is the exact matrix the engine
  must parameterize walkability by (§4). Note `SpeedType` (Foot/Track/Wheel/Hover/
  Float/FloatBeach/Amphibious) indexes THIS table; `MovementZone` (Normal/Water/
  Amphibious/AmphibiousDestroyer/WaterBeach/AmphibiousCrusher/Fly) is the
  pathfinding ZONE connectivity dimension — both are recovered (§1).

**Design consequence — the exemplar is a surface Float warship on a distinct naval
locomotor, moving cell-to-cell by A\* on water.** CB-3 models the **Float
water-movement domain** as its first naval locomotor: pathed A* like ground, but
over a water passability grid, blocked from land. The cloaking submarine (`[SUB]`
Typhoon, `Underwater=yes`) and the spawner ships (`[DRED]` Dreadnought
`Spawns=DMISL`, `[DEST]` Destroyer `Spawns=ASW`) are DEFERRED (§ Out of scope); the
hunt maps the sub's second CLSID + `Underwater` submerge/surface mechanism and the
`Spawns`/carrier delivery so later phases start from a mapped point.

## Current-engine reality — the water foundation does NOT yet exist (verified)

The brief's premise that "water cells are currently impassable-to-ground" is
**not accurate for the current code**, and this is the single most important design
input. Verified:

- **`build_passability`** (`engine/world.cpp:207–229`; contract in
  `engine/world.hpp:345–357`) marks a cell walkable (1) **iff a `MapCell` record
  was materialized at that (x,y)** — with **NO land-type distinction whatsoever**.
  Its own comment says so verbatim: *"For this vertical slice ALL materialized
  ground cells are treated walkable (water / cliffs / land-type refinement comes
  later)."*
- **`MapCell`** (`include/ra2assets/map.hpp:20–25`) carries `tile`, `subtile`,
  `level` — **no land-type / TerrainType field**.
- **`TmpFile`** (`include/ra2assets/theater.hpp`) decodes tile pixels + the
  extra-image/cliff geometry but **does NOT parse the per-tile TerrainType /
  RampType** bytes of the `.tmp` image header.
- **`Theater`** (`engine/iso_renderer.hpp:33–69`) walks `[TileSetNNNN]`
  FileName/TilesInSet to map a global tile index → `.tmp` file + local index, but
  attaches **no land-type category** to a tile set.

**Consequences for the design (both favorable and load-bearing):**

1. There is currently **no mechanism that blocks a ground unit from water** by
   land type. The "lake" the CB-1 transport and CB-2 aircraft demos reference is a
   VISUAL/narrative feature of the rendered temperate reference map (loaded from
   `mapsmd03.mix` entry `0x133a0cb2`, `engine/main.cpp:136–142`); a ground unit is
   held off those cells today only by non-materialization or occupancy, not by a
   water land-type. **CB-3 must BUILD water-cell identification** — the phase's
   net-new foundation (Task 2), larger than the brief assumed.
2. This makes the ground byte-identity argument **structurally trivial**: because
   `passable_` has no water concept to preserve, we keep `build_passability` and
   the `passable_` grid **literally unchanged** (the Land grid) and add a **fresh,
   separate water grid**. Ground A* reads the identical grid object; nothing about
   ground walkability changes (§8). We do NOT retrofit "block ground from water"
   onto the Land grid (that would perturb the ten gates); the water/land inverse
   is expressed by the SEPARATE water grid the naval domain reads.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Water movement model | **Pathed A\* over a WATER passability grid** (cell-to-cell, `occ_` one-ship-per-cell) — the SAME machinery ground units use, NOT CB-2's straight-line flight. The core change is **parameterizing walkability by a movement domain**: ground reads the existing `passable_` (Land) grid unchanged; naval reads a new `water_passable_` grid. The exemplar's locomotor is a distinct naval CLSID (§ ground truth); whether it is a standalone `ShipLocomotionClass` or Drive-gated-by-SpeedType, and any water-specific pathing (turn-in-place, no corner-cut differences), is §1 item 1 |
| Walkability parameterization | A `MovementDomain { Land, Water }` selector. `World` holds two grids: `passable_` (Land, built by the UNCHANGED `build_passability`) and a NEW `water_passable_`. A `grid_for(domain)` accessor returns the Land grid for `Land` (the DEFAULT — every existing call site unchanged) and the water grid for `Water`. Extensible to `Amphibious` later (§ Out of scope). Structural byte-identity: default Land ⇒ identical grid object (§4, §8) |
| Water-cell identification | NEW. A per-cell **LandType** resolved from the map's tiles. Source is §1 item 2 — the two candidates the hunt decides between: **(a)** the per-tile `TerrainType`/`RampType` byte in the `.tmp` image header (extend `TmpFile`), or **(b)** classifying a tile's `[TileSetNNNN]` as Water/Shore via the theater control INI (extend `Theater`). `main.cpp` (which owns the `Theater`) resolves per-cell LandType and hands `World` a land-type array; `World` builds `water_passable_` from it (`World` stays asset-free). Empty by default ⇒ no water pathing unless the naval path opts in (§8) |
| Passability rule | A cell is passable for a SpeedType iff its land-type speed % is **> 0** (the recovered table). For CB-3's Float exemplar: `water_passable_[cell] = 1` iff `LandType==Water`. The full speed-% → cost model (does A* weight cells by speed %, or is it binary passable/blocked?) is §1 item 2 — CB-3 uses **binary** passability (matching today's binary `passable_`); speed-% terrain cost is a documented carry-forward |
| Occupancy | Naval units claim `occ_` exactly like ground (one ship per cell). Water and land cells are DISJOINT sets, so naval `occ_` never interacts with ground `occ_` in practice; the shared `occ_` grid and its API are UNCHANGED (§4) |
| Exemplar | **Sea Scorpion `[HYD]`** (Soviet, image `HYD`, art PRESENT). Art-guarded; if `hyd.vxl`/`hyd.hva` ever regress, the guard drops to another present surface ship (`DRED`/`DEST` art verified in the mix) with the demo script adjusted, but the naval logic is exemplar-independent and covered by pure tests regardless |
| Naval combat | The Sea Scorpion acquires a target and fires `FlakTrackGun` (Damage=25, ROF=25, Range=5) through the EXISTING `commit_attack_target`/`step_combat`/`try_fire` path. Its combat-approach `MoveRequest` routes to a WATER A* path (the one naval-aware branch in the dispatch). Target = a hostile unit within range (a second naval unit on the lake, or a shore ground target — FlakTrackGun is "Anti-surface", it hits both; the demo picks a deterministic target). Instant single-target hit (the established projectile simplification, `combat.md`); FlakTProj ballistics deferred |
| Spawn | CB-3 **spawns the exemplar directly** (like CB-1/CB-2); no Naval Yard build-from-sidebar. Naval-yard-on-water placement is a carry-forward |
| Verification | **New deterministic gate `--demo-naval`** (the 11th) + `naval_hash` (FNV over the ship's water-path cells + position + attack state, two-run compute-and-self-compare, NO pinned literal) + **wedge guards (§7): the ship must actually MOVE on ≥1 water cell AND be BLOCKED from a land cell AND land ≥1 hit, else exit 1** |
| Byte-identity | **Strategy (a)**: naval fields excluded from `world_hash` (untouched); the Land grid `passable_` kept literally unchanged; every domain-selection site is an added clause provably `Land` for all existing units. Ground-unit A*, occupancy, and movement stay bit-identical (§8) |

## §1 What the Ghidra hunt must recover (→ `docs/re/naval.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001 Ghidra
project at `D:\ra2-re`, headless `analyzeHeadless.bat`,
`JAVA_HOME="C:\Program Files\Java\jdk-21"`) plus the real rulesmd/artmd,
producing `docs/re/naval.md` — the calibration authority BEFORE any code, with a
located-functions table, a "deviations we take" section (mirroring
`voxel-render.md`/`aircraft.md`), and each item landing CONFIRMED / PROBABLE /
[RE-FALLBACK] with function address + decompile reasoning. Anchor on: the naval
**`LocomotionClass` CLSID `{2BEA74E1-7CCA-11d3-BE14-00104B62A16C}`** (CONFIRMED
from `[HYD]`/`[DEST]`/`[SUB]`/`[DRED] Locomotor=` — the primary vtable/`Process`
entry map) and the sub's second CLSID `{4A582741-9839-11d1-B709-00A024DDAFD1}`
(commented out in rules — the submerge/cloak loco, mapped for the deferred sub);
`ShipClass`/`FootClass`/`TechnoTypeClass`; the INI-key strings the binary loads
(`"SpeedType"`, `"MovementZone"`, `"MovementRestrictedTo"`, `"Naval"`,
`"Underwater"`, `"Locomotor"`, plus the already-pinned `"Speed"`/`"ROT"` from
`voxel-render.md` §5); the **`CellClass::LandType`** field and the function that
computes a cell's land type from its tile; and the **ground-speed lookup**
(`GroundClass`/`Ground[]` — the `[Clear]`/`[Water]`/`[Beach]`… speed table above,
indexed by LandType × SpeedType, where 0% gates passability). YRpp/Ares addresses
are UNTRUSTED prose hints only — content-anchor everything. **Do NOT guess numeric
constants** (speed→cells/sec, ROT→u16/frame, range→cells) — they are hunt outputs.
Six items:

1. **The naval locomotor & movement shape**: is water movement a standalone
   `ShipLocomotionClass` (the distinct CLSID) or the Drive loco gated by
   `SpeedType`/`MovementZone`? CONFIRM it is **pathed A\* cell-to-cell** (not
   straight-line) and reserves ONE `occ_` cell like ground (expected: yes — ships
   queue and collide). Any water-specific pathing (turn-in-place before moving via
   `ROT`, wake, no diagonal corner-cut past shore) vs identical to the ground A*.
   Which locomotor `[HYD]` uses (the CLSID above — CONFIRMED from rules; pin the
   vtable/`Process`).
2. **The LandType → passability model (THE foundation)**: how the game derives a
   **cell's land type** — is it the per-tile `TerrainType` byte in the `.tmp`
   image header, the `[TileSetNNNN]` category, or a `CellClass` field stamped at
   map load? Then the **passability decision**: confirm it is the `[Clear]`/
   `[Water]`/… speed table indexed by `LandType × SpeedType`, with **0% ⇒ blocked**
   and **> 0% ⇒ passable** (so a Float unit is on Water only; a Track unit is off
   Water). Is A* cost BINARY (passable/blocked) or **speed-weighted** by the %? And
   the `MovementZone` role in zone-connectivity pathing vs the SpeedType speed
   table. CONFIRM the water/land/beach boundary (Beach is 0% for plain Float — a
   pure ship cannot beach). This is the exact gating `passable_`/A* must be
   parameterized by.
3. **Water occupancy & pathing rules**: one-ship-per-cell (expected: yes, shared
   `occ_` semantics); any naval-specific spacing, and how a blocked ship re-paths
   (the ground `passable_minus_units` repath at `world.cpp:2363` analogue on the
   water grid). Confirm land and water cell sets are disjoint (a cell is Water XOR
   land) so the two grids never overlap.
4. **Naval combat**: `[HYD] Primary=FlakTrackGun` (Damage=25, ROF=25, Range=5,
   Warhead=FlakTWH) target acquisition **from water** — can a ship auto-acquire a
   land/shore target and vice-versa (`NavalTargeting`/`LandTargeting`/
   `CanPassiveAquire` keys — `[HYD]` sets none of the restrictive ones)? Range in
   cells, ROF→cooldown, `FlakTWH` verses (→ `combat.md` damage math which already
   handles warheads); the `PrimaryFireFLH=65,0,180` muzzle offset through the
   existing FLH render path. Any naval-specific delivery (depth charge vs sub — OUT).
5. **Exemplar stats interpretation** (`[HYD]`/`[FlakTrackGun]` VALUES above are
   ground truth already): the FORMULAS that turn them into engine constants —
   `Speed=8`/`ROT=6` → cells/sec and u16/frame via `voxel-render.md` §5;
   `Range=5`/`ROF=25`/`Damage=25`/`FlakTWH` → `combat.md` constants; `Strength=400`
   → hp. Reuse the established conversions; the hunt pins any naval-specific ones.
6. **Determinism + byte-identity**: how naval units integrate into `step_units`/
   `step_combat` WITHOUT changing ground-unit pathing (expected: the domain-
   selected grid — Land for all existing units — see §8); deterministic ordering
   (naval units iterate in the same unit-vector order; no new step reordering); and
   confirmation that **no RNG** (scatter/wake) is on the naval critical path
   (`FlakTProj` fixed, no `BallisticScatter` on the path — pin it).

Calibration discipline (unchanged): every engine constant cites this doc. **All
numeric semantics in this spec are pre-hunt expectations, not ground truth** (the
rulesmd/artmd VALUES quoted above were read from the real files on this install
and are committable; their INTERPRETATION is the hunt's). A controller **RE-GATE**
amendment step precedes implementation — the hunt's findings amend the plan and
are committed BEFORE dependent dispatch, with `[RE-GATE APPLIED]` markers threaded
through every dependent task (the CB-1/CB-2 precedent). **[RE-FALLBACK]**: anything
the hunt cannot pin becomes a plan-amendment gate (present options to the human) —
never an uncalibrated guess. The **LandType source (§1 item 2)** and the
**binary-vs-speed-weighted cost** question are the most likely [RE-FALLBACK]s; the
fallbacks are documented (LandType via `.tmp TerrainType`; binary passability).

## §2 Architecture & recovered data model

Consistent with every prior phase: pure SDL-free engine/asset modules + thin
World integration + a deterministic demo in main.cpp. ~8 tasks:

- **Task 1**: Ghidra hunt → `docs/re/naval.md` (RE-GATE before implementation;
  §1 above). Includes verifying the temperate reference map has a contiguous
  materialized WATER region (the "lake") once LandType decoding exists — if it does
  not, the demo selects a suitable water region/map (a demo-map [RE-FALLBACK]).
- **Task 2**: LandType decoding (net-new foundation) — extend `ra2assets`
  (`TmpFile` per-tile `TerrainType`/`RampType`, OR `Theater` tile-set
  classification, per §1 item 2) to yield a **per-cell LandType**; a pure
  `land_type_of_cell(...)` helper, doctest-covered on hand-built tiles/tile-sets,
  no SDL. Registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt` if a
  new TU is added.
- **Task 3**: pure engine water passability — `MovementDomain` enum,
  `build_water_passability(land_types, grid_w, grid_h)` (Water-land-type ⇒ 1),
  over plain arrays; doctest-covered (a Water cell is passable, a Clear/Beach/Rock
  cell is not; the Land grid from `build_passability` is byte-identical to today).
- **Task 4**: World integration — `Unit`/`UnitCombat` naval fields (all INERT
  defaults), `water_passable_` member + `grid_for(domain)`, a `set_water_terrain`
  opt-in setter, and threading the domain through the THREE A* sites
  (`order_unit_move`, the `step_units` repath, the combat-approach `MoveRequest`
  dispatch) as added clauses defaulting to Land (§4). **Ten-gate re-verify at this
  first additive commit.**
- **Task 5**: naval combat — the Sea Scorpion acquires a target and fires
  `FlakTrackGun` through the EXISTING `step_combat`/`try_fire`; its approach
  request routes to a WATER A* path (§6).
- **Task 6**: render + rules — the `HYD` roster/art load (art-guarded), rules
  resolution of the naval stats (SpeedType/MovementZone/Speed/ROT/weapon) into
  `UnitCombat`. Reuses `draw_vehicle_unit`/`draw_voxel_sprite` (ships are just
  voxels at altitude 0 — no new render work).
- **Task 7**: `--demo-naval` gate (the 11th) + `naval_hash` + witness lines +
  wedge guards (§7).
- **Task 8**: finalize — suites both modes, ten existing gates byte-identical,
  arm64 NDK nm-verify of the new naval/land-type symbols, README (CB-3 section +
  11th gate row + deviations note citing `docs/re/naval.md`).

**Decomposition tripwire**: the LandType-decoding foundation (Task 2) is net-new
`ra2assets` work whose size depends on the §1 item-2 recovery (a per-tile byte is
small; a tile-set classification with theater-INI parsing is larger). If Task 2
fans out, or the pathfinder parameterization touches more sites than the three
enumerated, the phase splits at the natural seam — **(A) water-movement foundation**
(Tasks 1–4, 6; a `--demo-naval-move` gate that only paths a ship across the lake
and proves land-blocking, no combat) then **(B) the naval-combat exemplar**
(Tasks 5, 7). Default is the single coherent ~8-task phase below.

### Data model

Field shapes below are the approved design; the Task 1 hunt CONFIRMS or REPLACES
the semantics (RE-GATE), and code/tests are written against the amended form.

- **`UnitCombat` extension** (resolved once from Rules by main.cpp, like every
  existing stat; ALL defaults INERT): `is_naval = false`,
  `movement_domain = MovementDomain::Land`. Speed/ROT/weapon reuse the EXISTING
  `speed_cps`/`rot`/`damage`/`rof`/`range`/`verses` fields — a ship is a voxel
  vehicle, so `is_vehicle=true` already applies; naval adds only the domain flags.
- **`Unit` extension** (all defaults INERT — §8): `is_naval = false`,
  `movement_domain = MovementDomain::Land` (copied from `UnitCombat` at spawn). No
  new movement state is needed — a naval unit reuses `path`/`path_idx`/`fx`/`fy`/
  `cell_x`/`cell_y`/`occ_` exactly like a ground vehicle; only the GRID its A*
  reads differs.
- **`MovementDomain`** (new enum, `world.hpp`): `Land = 0`, `Water = 1` (Land = 0
  so a value-initialized/legacy Unit is Land). Reserved `Amphibious` for later.
- **`World` members**: `std::vector<std::uint8_t> water_passable_` (empty by
  default), `bool naval_any_ = false` (opens on the first naval order — a
  diagnostics/guard flag; naval movement has no separate step to firewall, unlike
  flight). `grid_for(MovementDomain d) const` returns `passable_` for Land (the
  SAME object every existing site already uses) and `water_passable_` for Water.
- **`World::set_water_terrain(const std::vector<std::uint8_t>& land_is_water)`**:
  opt-in setter main.cpp calls (only in the naval demo) to build `water_passable_`
  from the resolved per-cell water mask. Never called by any existing demo ⇒
  `water_passable_` stays empty ⇒ no behavior change (§8).
- **Pure modules** (registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`): `build_water_passability(...)` alongside the existing
  `build_passability` in `world.cpp`; the LandType helper in `ra2assets` (Task 2).
  All doctest-testable on hand-built inputs; every recovered constant cites
  `docs/re/naval.md`.

## §3 Water movement — pathed A* on the water grid

Naval movement is the SAME machinery as ground movement, differing ONLY in which
passability grid the A* reads. There is **no new movement step** (unlike CB-2's
`step_flight`): naval units flow through `step_units` and `find_path` like ground
vehicles.

1. **Order**: `World::order_unit_move(idx, gx, gy)` on a naval unit computes its A*
   path over **`grid_for(u.movement_domain)`** (= `water_passable_`) instead of
   `passable_`. This is the ONE naval-aware selection in `order_unit_move`; ground
   units (`movement_domain==Land` in every existing demo) read the exact existing
   `passable_` object — byte-identical. `naval_any_` is set here for diagnostics.
2. **Walk**: `step_units` interpolates `fx/fy` along `u.path` and reserves the next
   `occ_` cell exactly as for a ground vehicle — unchanged. When it re-paths a
   blocked unit (the `passable_minus_units` + `find_path` at `world.cpp:2350–2369`),
   it selects the domain grid: `const auto& g = grid_for(u.movement_domain);` — Land
   for all existing units ⇒ identical.
3. **Occupancy**: a naval unit `occ_.set`s its water cell like a ground unit; since
   water and land cells are disjoint, no ground unit ever contends for a water cell
   and vice-versa. `occ_` API and semantics unchanged (§4).
4. **Blocked from land**: because `water_passable_` is 0 on every non-Water cell,
   `find_path` returns empty (or a stay-put single-cell path) if a naval unit is
   ordered onto land — the ship refuses the order, the direct analogue of a ground
   unit ordered onto an impassable cell today. The `--demo-naval` wedge guard
   asserts this land-blocking actually happens (§7).
5. **Determinism**: no new step, no reordering; naval units iterate in the same
   unit-vector order inside `step_units`. `find_path` is already fully deterministic
   (`pathfind.cpp` heap tie-break by f, then h, then cell index). No RNG on the
   naval path.

## §4 Water passability & the byte-identity of ground pathfinding

This is the phase's hardest constraint: the walkability test feeds A*, which every
gate's ground units depend on. The rule is uniform and structural:

- **The Land grid is untouched.** `build_passability` and the `passable_` member
  are unchanged, bit-for-bit. Ground `find_path`/`assign_formation`/`step_units`/
  `step_harvest`/`ore_.step` all keep passing `passable_` exactly as today — no
  edit to those call sites' grid argument. `walkable(x,y)` (`world.cpp:299–302`)
  is unchanged (it reads `passable_`; it is a Land-domain query and no existing
  caller is naval).
- **The Water grid is new and default-empty.** `water_passable_` is built ONLY when
  `set_water_terrain` is called (the naval demo). No existing demo calls it, so the
  grid is empty and no naval path is ever computed in any existing demo.
- **Domain selection is an added clause.** The three A* sites gain
  `grid_for(u.movement_domain)` (or a `u.is_naval ? water_passable_ : passable_`
  ternary). For every unit in every existing demo `movement_domain==Land`
  (default), so the selection resolves to the SAME `passable_` object — control
  flow and data are identical instruction-for-instruction. The three sites:
  1. `order_unit_move` initial path (`world.cpp:481–482`);
  2. the `step_units` blocked re-path (`world.cpp:2362–2364`) — `step_units` gains
     a `water_passable` parameter (all callers pass it; read only when a unit's
     domain is Water) and per-unit grid selection;
  3. the combat-approach `MoveRequest` dispatch (`world.cpp:1994–2024`) — a naval
     unit's approach request routes through `order_unit_move` directly (the naval
     domain selects the water grid there), bypassing the `assign_formation`
     firing-cell batching, mirroring CB-2's aircraft branch at the same site.
     Multi-ship firing-formation fanout is a carry-forward.
- **`assign_formation` is unchanged.** It keeps taking `passable_` (Land) at its
  existing ground/harvest/group-move call sites; the naval path does not route
  through it in CB-3 (see the dispatch branch above), so its hot signature is
  untouched.
- **Occupancy is unchanged.** Naval units use `occ_.set`/`release_all_of`/
  `can_enter` identically; water/land disjointness means no cross-domain
  contention. No `occ_` API or storage change.

## §5 Naval unit rendering

Ships are **voxel vehicles at altitude 0** — the render pipeline needs no new work:

- The Sea Scorpion draws through the EXISTING `draw_vehicle_unit`/
  `draw_voxel_sprite` + `facing_cache` path exactly like the Rhino/Kirov, using
  `hyd.vxl`/`hyd.hva` and `body_facing` from `ROT`-driven turning. No turret
  (`Turret=no`). `PrimaryFireFLH=65,0,180` uses the existing FLH render path.
- **Art-guarded**: the `HYD` roster/art load is guarded; if `hyd.vxl`/`hyd.hva`
  regress, the guard drops to a present surface ship (`DRED`/`DEST`) with the demo
  script adjusted. The naval sim logic is exemplar-independent (pure tests).
- **HUD reuse only**: selection diamond / HP bar size to the hull sprite width
  (`veh_width`) exactly as ground vehicles. No new font/UI systems, no shadow, no
  altitude offset (a ship sits on the water surface at altitude 0).
- **Guarded**: no existing demo spawns a naval unit, so no existing screenshot
  changes (§8).

## §6 Naval combat

Composes on the existing combat path with no new damage math:

1. **Acquire**: `commit_attack_target` on the Sea Scorpion sets `target` (a hostile
   unit) or `target_building`, exactly as a ground vehicle. `[HYD]` sets no
   restrictive `CanPassiveAquire=no`, so idle auto-acquire is allowed; the exact
   naval/land targeting permission (`NavalTargeting`/`LandTargeting`) is §1 item 4 —
   the demo uses an explicit attack order so acquisition is deterministic
   regardless.
2. **Approach**: when out of range, `step_combat` emits its usual approach
   `MoveRequest`; the naval-aware branch in the dispatch (§4) routes it to a WATER
   A* path toward a firing position within `FlakTrackGun Range=5`.
3. **Fire**: once within `w_range`, `try_fire` applies damage through the EXISTING
   deterministic path (`combat.md` Damage=25 × Verses(`FlakTWH`) × ROF=25). No
   turret aim gate (`Turret=no`). Fire from the water surface; the muzzle FLH uses
   the recovered `PrimaryFireFLH=65,0,180`.
4. **Weapon delivery**: the established **instant single-target hit** (the
   simplification ground vehicles already ship, `combat.md`). The real `FlakTProj`
   projectile (Speed=50) + `FlakTWH` splash is a documented carry-forward.
5. **Target**: the demo's target is a hostile unit within range — a second Sea
   Scorpion on the lake (a naval duel) or a shore ground unit (naval-to-shore).
   `FlakTrackGun` ("Anti-surface") hits both; the demo picks a deterministic,
   in-range target. A stationary target keeps the run reproducible.

## §7 The `--demo-naval` gate (the 11th), `naval_hash` & witnesses

**Script** (headless or rendered — bit-identical): on the temperate reference map,
resolve the per-cell water mask (`set_water_terrain`), spawn one Sea Scorpion
`[HYD]` (art-guarded) for house 0 on a lake cell and one hostile target for house 1
(a second ship on water, or a shore unit within range), all scripted (no player
input):

1. **FLOAT/MOVE**: order the ship to a distant WATER cell across the lake; it paths
   over water via A* and arrives (witness `[naval] moved cells=N path_water=yes
   arrived=(x,y)`); assert every path cell is a Water cell (folded into the hash).
2. **LAND-BLOCK**: order the ship to a LAND cell; it refuses (empty/stay-put path)
   — the inverse of a ground unit (witness `[naval] land-block ordered=(lx,ly)
   moved=no`). This proves the water/land inverse.
3. **ATTACK**: order it to attack the hostile target; it approaches to within
   `Range=5` over water and fires; the target takes damage and dies (`[naval] hit
   target=N dmg=25`, `[naval] target_dead=N`).
4. **RESUME**: run a fixed window; the ship holds station on the water.

**`naval_hash`**: FNV-1a 64 (the established `fnv_mix` helper) folded at fixed
checkpoints (post-move, post-land-block, post-attack, end) over: every unit's
`(cell_x, cell_y, alive, hp)` PLUS each naval unit's `(movement_domain,
path.size(), target)` and the count of water cells traversed — exactly the state
the phase adds. Two-run compute-and-self-compare (`deterministic=yes`), NO pinned
literal in code (the `--demo-transport`/`--demo-aircraft` precedent); README
records the stable value in the gate table once landed.

**Wedge guards** (exit 1, so the gate can never pass vacuously): the ship actually
MOVED across ≥1 water cell AND was BLOCKED from a land cell (the land-block order
produced no movement) AND at least one hit landed on the target — all three must
hold, else exit 1. Witness log + hash are pure sim state → rendered and headless
runs byte-compare identical.

## §8 Byte-identity discipline (core-sim — the phase's hardest constraint)

The **ten existing permanent gates MUST stay bit-identical** (values in
Depends-on). Naval parameterizes the pathfinder, so the discipline is stricter than
CB-2's:

- **The Land grid is literally unchanged.** `build_passability` is untouched;
  `passable_` is the same object every existing A*/formation/harvest/ore site reads.
  Because the engine has NO water concept today (verified above), there is nothing
  in `passable_` to preserve or alter — the ground walkability is identical by
  construction. This is the structural no-change argument: the naval domain is
  ADDED beside ground, never carved out of it.
- **All new Unit/UnitCombat fields default INERT**: `is_naval=false`,
  `movement_domain=MovementDomain::Land` (= 0). No existing demo spawns a naval unit
  and no existing spawn path sets `is_naval=true`, so every field holds its default
  in every existing demo.
- **`world_hash` strategy (a) — EXCLUDE, chosen**: `world_hash`
  (`main.cpp:3779–3792`) reads exactly `(cell_x, cell_y, hp, alive)` per unit +
  building fields and is NOT touched; the new naval fields are hashed ONLY by
  `naval_hash` in the new gate. `cell_x/cell_y` ARE hashed and ARE live for a naval
  unit — but no existing demo has a naval unit, so those coords are default-driven
  exactly as before in every existing demo.
- **Domain selection is an added clause** at each of the three A* sites
  (`grid_for(domain)` / `u.is_naval ? water : passable`), provably `Land`/`passable_`
  for every unit in every existing demo (default `movement_domain=Land`) — so grid
  choice, control flow, and A* output are identical where it matters. `step_units`
  gains a `water_passable` parameter read only when a unit's domain is Water; every
  existing caller passes the (empty) grid and no existing unit reads it.
- **`water_passable_` is default-empty and opt-in**: built only by
  `set_water_terrain`, which no existing demo calls. `naval_any_` is set only by a
  naval order, which no existing demo issues.
- **No new step, no reordering**: naval movement reuses `step_units`; `World::step`
  is unchanged in structure (no `step_naval` pass). `assign_formation`, `occ_`, and
  `walkable()` signatures/behavior are unchanged.
- **The render/combat paths are guarded** on `is_naval`; no existing demo spawns a
  naval unit, so no existing screenshot or combat outcome changes.
- **The ten-gate re-verify runs at the first additive commit (Task 4) AND at
  finalize (Task 8)**, isolating any regression to the commit that introduced it —
  the CB-1/CB-2 discipline.

## §9 Testing strategy

- **Pure doctests** (host-only, hand-built inputs, no install):
  `tests/test_land_type.cpp` (or folded into `test_theater.cpp`) — a Water tile/
  tile-set classifies as Water, a Clear/Beach/Rock tile does not (§1 item 2 fixture);
  `build_water_passability` — a Water cell is passable, Clear/Beach/Rock are not;
  the Land grid from `build_passability` is byte-identical to the pre-change output
  on the same cells (a regression pin). `grid_for(Land)` returns the same object as
  `passable()`.
- **World-level tests** (the existing `test_world` pattern): a naval unit paths
  across a hand-built water region via `find_path` on `water_passable_`; a naval
  unit ordered onto land gets an empty/stay-put path (land-blocked); a ground unit
  and a naval unit coexist (ground on land, ship on adjacent water) without
  occupancy contention; a ground unit's A* over the SAME hand-built grid is
  byte-identical with and without the naval unit present (the parameterization does
  not perturb ground); naval fire damages/kills a target via `try_fire`.
- **Integration**: the `--demo-naval` gate (§7) with wedge guards (moved-on-water
  AND land-blocked AND ≥1 hit) + two-run `naval_hash` self-compare (no pinned
  literal); rendered vs headless byte-identical.
- **Regression**: all ten existing gates byte-identical (values in Depends-on),
  re-run at the first additive commit AND at finalize; full suite both modes; arm64
  NDK build with the new naval/land-type symbols nm-verified. Suite stays
  scoped/fast (the shared-fixture discipline).

## Out of scope / divergences (explicit)

- **Submarine cloak / submerge-surface** (`[SUB]` Typhoon `Underwater=yes`, the
  second Locomotor CLSID `{4A582741-…}`) — DEFERRED; CB-3 picks the surface Sea
  Scorpion. The §1 hunt maps the submerge/cloak mechanism so a later phase starts
  from a mapped point. CB-3 ships no cloaking.
- **Naval Yard production / build-from-sidebar / naval-yard-on-water placement** —
  CB-3 spawns the exemplar directly (like CB-1/CB-2). Production-queue and
  water-placement wiring are carry-forwards.
- **Amphibious / hovercraft land↔water transition** (`SpeedType=Hover`/
  `Amphibious`, `MovementZone=Amphibious`/`AmphibiousDestroyer`/`WaterBeach`, and
  the CB-1 amphibious transport deferred here) — the `MovementDomain` enum reserves
  `Amphibious`, but the land↔water transition and Beach/FloatBeach crossing are
  DEFERRED; CB-3 is pure-water ships. The hunt recovers the amphibious zones.
- **Spawner naval delivery** (`[DRED]` Dreadnought `Spawns=DMISL`, `[DEST]`
  Destroyer `Spawns=ASW`, aircraft carrier launching CB-2 aircraft) — out; the hunt
  maps `Spawns`/carrier delivery for later.
- **Speed-weighted terrain cost** — CB-3 uses BINARY passability (matching today's
  binary `passable_`); the `[Clear]`/`[Water]`/… speed-% → A* cost model is a
  documented carry-forward (§ scope table, §1 item 2).
- **Exact projectile ballistics** (`FlakTProj` Speed=50 flight + `FlakTWH` splash)
  — a simplified instant single-target hit is used (§6); the falling/travelling
  projectile is a carry-forward.
- **Naval-specific combat nuances** (depth charges vs subs, ASW, naval
  superweapons, the full naval roster, sinking-animation physics) — out.
- **AI use of naval units** — the AI does not field ships in CB-3 (carry-forward);
  naval-related ScriptType opcodes stay inert in the AI interpreter.
- **Ground-from-water blocking retrofit** — CB-3 does NOT change the Land grid to
  block ground units from water (that would perturb the ten gates and is
  unnecessary since no existing demo paths a ground unit through the lake). The
  water/land inverse is expressed by the separate water grid the naval domain reads.
  A future phase can make the Land grid land-type-aware once its byte-identity
  budget allows.

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real rulesmd /
  prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE precede all
  dependent tasks; every recovered constant cites `docs/re/naval.md`;
  [RE-FALLBACK] items become plan-amendment gates, never guesses.
- TEN byte-identity gates stay green (§8); `--demo-naval` becomes the 11th
  permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the
  install. Long rendered demos auto-background on short timeouts; NEVER pipe a
  native exe through `Select-Object -First N` (stops draining stdout → false hang).
- No game assets / GPL source / Ghidra output committed; recovered numeric
  constants ARE committable (the `[HYD]`/`[FlakTrackGun]` VALUES and the land-type
  speed table above already are).
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs — the
  plan's briefs must order FOREGROUND synchronous runs with generous timeouts.

## Testing summary

| Layer | Tests |
|---|---|
| ra2assets land-type (pure) | a Water tile/tile-set classifies as Water; Clear/Beach/Rock do not; per-cell LandType resolution on a hand-built map (§1 item 2 fixture) |
| engine water passability (pure) | `build_water_passability` (Water⇒1, Clear/Beach/Rock⇒0); `build_passability` output byte-identical to today on the same cells; `grid_for(Land)` == `passable()` object |
| World integration | naval unit paths across water via A*; naval unit land-blocked (empty/stay-put path); ground + naval coexist with no occupancy contention; a ground unit's A* is byte-identical with/without a naval unit present (parameterization does not perturb ground); naval fire damages/kills target via try_fire |
| integration | `--demo-naval` witnesses (move-on-water → land-block → attack-hit → resume) + naval_hash two-run self-compare (no pinned literal) + wedge guards (moved-on-water AND land-blocked AND ≥1 hit else exit 1); rendered ≡ headless |
| regression | ten existing gates byte-identical (§8), re-verified at the first additive commit and at finalize; full suite both modes; arm64 nm-verify of naval/land-type symbols |
