# Unit Production Phase Design — Factories, Queues, Sidebar Tabs

**Date:** 2026-07-12
**Status:** Approved (brainstorm sections 1–5 approved individually)
**Depends on:** Economy phase (complete; ore field, miner FSM, credits,
`--demo-econ` gate econ_hash=7fed96cb98b68017)

## Goal

The War Factory (GAWEAP) and Barracks (GAPILE) actually produce units: the
sidebar Defense/Infantry/Vehicle tabs come alive, cameo clicks feed real
concurrent production queues, and finished units drive out of a recovered
factory exit cell, path to a settable rally point, and become controllable.
Follows the completed Economy phase.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Factories | **Both unit factories** — Infantry (Barracks → GI etc.) and Vehicle (War Factory → tanks) — **plus Defense structures** (tab 1; defensive structures placed like buildings) |
| Unit exit | **Real exit cell + settable rally point** (recovered from Ghidra) — right-click ground with a factory selected sets the rally; a finished unit drives out the exit cell then paths to rally |
| Multiple factories of one type | **Full real behavior** — primary-building selection (right-click / recovered trigger to set primary) + multi-factory production speedup (rulesmd `MultipleFactory`) |
| Enemy production | **Player-only this phase** — enemy Soviet units would need attack-wave AI (a distinct future phase); enemy factories exist but do not produce |
| Player side | **Allied** (GA\* side) |
| Architecture | **Approach A** — generalize into per-category concurrent queues, Ghidra-first, additive; protects all four byte-identity gates by keeping the existing BuildingType queue math untouched |

## §1 Architecture & calibration authority

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001 at
D:\ra2-re, headless analyzeHeadless.bat, JAVA_HOME=C:\Program Files\Java\jdk-21)
producing `docs/re/production.md` with confidence tables and [RE-FALLBACK]
plan-amendment gates. It must pin:

1. **Queue model / tab mapping**: how many concurrent FactoryClass queues
   exist and how the 4 sidebar tabs (Buildings/Defenses/Infantry/Vehicles)
   map to them; whether Structures and Defenses share one queue; the field
   that classifies a producible into a tab/queue (BuildingType-vs-UnitType
   + defense flag + infantry/vehicle split).
2. **Factory selection & primary**: which factory a completed item emerges
   from; how "primary" is chosen and changed (Set_Primary / right-click
   rule; default when several owned); multi-factory speedup
   (`MultipleFactory` multiplier and how each extra same-type factory
   compounds BuildSpeed).
3. **Exit cell + leave-factory**: where a finished vehicle/infantry first
   appears relative to the factory foundation (door/exit cell), and the
   leave-factory motion before free control (the subtlest item).
4. **Rally point**: how a right-click with a factory selected sets the
   rally, and where completed units path (rally vs default exit-adjacent).
5. **Build economy for units**: confirm the same
   `build_frames(cost, speed)` + 54-step charge model applies to UnitTypes
   (FactoryClass shared); that units have NO FreeUnit charge subtraction;
   and immediate-exit-vs-ready-hold (structures wait for placement;
   vehicles exit on completion — confirm).
6. **Per-unit prereq/tech gating**: which prerequisite groups gate each
   producible unit (BARRACKS infantry, FACTORY vehicles), recovered as
   committable constants.

Calibration discipline (unchanged): every engine constant cites this doc.
**[RE-FALLBACK]**: anything the hunt cannot pin becomes a plan-amendment
gate (present options to the human; amend the plan and commit BEFORE
dependent dispatch) — never an uncalibrated guess. Numeric constants
recovered from the binary are committable; GPL sources at D:\ra2-re are
read off-repo, prose conclusions only; content-anchored hunts only (no
trusted YRpp/Ares hardcoded addresses).

**Layering** (unchanged from prior phases):

```
ra2assets (pure decode, no SDL)      engine (pure sim, doctest)            main.cpp (SDL wiring)
──────────────────────────────      ──────────────────────────            ─────────────────────
rules.cpp += unit-roster /           production.hpp MULTI-queue container   sidebar tab state + switching
  classification accessors             over N existing ProductionState      unit cameos per tab
                                       (54-step charge math UNTOUCHED)      factory-exit render/anim
                                     factory.hpp exit-cell + rally +        rally-point right-click
                                       primary + multi-factory speedup      --demo-prod gate + hash
                                     UnitTypeDef roster mirroring
                                       BuildingTypeDef
```

## §2 Roster & rules: the producible descriptors

Most per-id stats already have `rules.hpp` accessors (`cost`, `tech_level`,
`prerequisite`, `prerequisite_group`, `strength`, `armor`, `primary`,
`weapon_*`, `verses`, `rot`, `has_turret`, `speed`); cameos come from
`ra2assets::Art`. What's missing is **classification**.

**Two new rules.cpp accessors:**

- `build_category(id)` → `{Structure, Defense, Infantry, Vehicle, None}`,
  resolved from INI list membership (`[BuildingTypes]` / `[InfantryTypes]` /
  `[VehicleTypes]`; aircraft/ships → None) plus the recovered
  structure-vs-defense classifier from §1.
- `is_infantry(id)` / `is_vehicle(id)` membership helpers if the hunt shows
  the category needs them separately (fold into `build_category` otherwise).

**New engine roster struct `UnitTypeDef`** mirroring `BuildingTypeDef`'s
production-relevant fields: id, cost (charge == cost — units have no
FreeUnit subtraction), tech_level, category, prerequisite tokens, cameo,
resolved UnitCombat, is_vehicle, chrono_miner. Assembled once by main.cpp
from existing accessors + Art.

Structures and defenses keep using the existing `BuildingTypeDef` roster
(extended only if §1 shows a defense needs a new field); the two rosters
stay separate; the multi-queue layer references whichever roster a queue's
category points at.

**Tests:** pin real rulesmd values for a representative producible per
category (GI Infantry, a tank Vehicle, a pillbox-class Defense, GAWEAP
Structure) — cost/tech_level/prerequisite/category; skip cleanly
(MESSAGE + return) without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`).

## §3 Multi-queue production model

**Expected queue model (CONFIRM via §1):** RA2 has one concurrent queue per
factory building-category while sidebar tabs are filtered views —
canonically 3 concurrent queues (BuildingType/ConYard, Infantry/Barracks,
Vehicle/War Factory) behind 4 tabs, where tabs 0 (Structures) and 1
(Defenses) both view the single BuildingType queue (you cannot build a
power plant and a pillbox at once). [RE-FALLBACK] if §1 shows otherwise.
This makes the change purely additive: the existing `ProductionState` IS
the BuildingType queue and `--demo-base` keeps touching only it.

**Decouple the economy WITHOUT changing its math:** add
`start(ps, cost, charge, build_speed)` scalar entry point; the existing
`start(ps, type, types, build_speed)` becomes a one-line wrapper calling it
with `types[type].cost`/`.charge` (bit-identical operations →
`--demo-base` verified byte-identical). `tick`/`cancel`/`placed` are reused
verbatim by all queues (already roster-agnostic — `tick` does
`(void)types`, charge_basis baked into the slot by `start`). Add
`can_start_unit(ps, unit_type, unit_roster, buildings, house, groups,
factory_alive)` reading the `UnitTypeDef`'s tech_level/prerequisite but
scanning BUILDINGS for satisfaction (a unit's prereq is BARRACKS/FACTORY,
which are buildings), reusing `prerequisites_met`.

**`ProductionQueues` container** owns the three `ProductionState`s and per
queue tracks:

- its category + which roster it draws from;
- the factory-alive gate (ConYard / BARRACKS group / FACTORY group must
  have an alive Idle owned building);
- the multi-factory speedup (build_speed scaled by count of alive
  same-type factories per the recovered `MultipleFactory` rule, §1).

API: `can_start(cat, type)`, `start(cat, type)`, `tick(dt, power_mult)`
(advances every queue, each with power_mult × factory-speedup),
`cancel(cat)`, per-queue completion state. On completion: BuildingType
queue → existing ready-to-place; Infantry/Vehicle → a "ready-to-exit" flag
consumed by §4.

**Tests:** infantry & vehicle queues reproduce the 54-step charge economy
(reuse construction pins); three queues run concurrently without
interfering; multi-factory speedup matches the recovered multiplier; unit
prereq gating (GI blocked until a BARRACKS-group building is alive Idle);
explicit assertion that the BuildingType queue path is unchanged.

## §4 Factory exit, rally & primary (RE-critical handoff)

**New pure module `factory.hpp`** (layered on Building/occupancy/pathfinding
like `harvest.cpp`):

- `exit_cell(factory, def)` → recovered door/exit cell,
  foundation-relative (per §1 item 3; [RE-FALLBACK] adjacent-cell rule like
  the Economy refinery dock cell).
- `primary_factory(buildings, types, category, house)` → index of the
  factory a completed unit exits from (flagged primary, else the recovered
  default per §1 — likely most-recently or first-built).
- `set_primary(buildings, types, b_idx)` → sets the primary flag on
  `b_idx`, clears it on every other same-category same-house factory
  (exactly one primary per category per house).

**`Building` gains four default-initialized fields** — `bool primary`,
`int rally_x`/`rally_y`, `bool has_rally` — NOT read by `world_hash`, so
`--demo-base` stays byte-identical (same discipline Economy used adding
fields to `Unit`).

**World integration** (world.cpp / thin driver): each frame, for each unit
queue reporting ready-to-exit, find `primary_factory`; if its `exit_cell`
is free in occupancy, spawn the unit there (`add_combat_unit` /
`add_harvester`; house + stats from `UnitTypeDef`), give it a
leave-factory → rally move order (path to
`has_rally ? (rally_x, rally_y) :` recovered default gather cell), then
clear the slot (`placed`). If the exit is blocked, the queue HOLDS
"ready" — a blocked exit stalls that factory, matching RA2 (deterministic).
`World::set_rally(b_idx, x, y)` and `World::set_primary(b_idx)` for the
input layer. Factory destroyed mid-build: if no alive Idle factory of that
category remains for the house, cancel + refund that queue (mirrors
today's `ps_tick` ConYard-died cancel); if another same-type factory
survives, the queue continues and exits from it.

**Tests:** `exit_cell` matches the recovered offset; a ready unit spawns at
the exit when free and HOLDS when occupied; a spawned unit gets a rally
path (and default gather path when no rally); `primary_factory` routes the
exit to the flagged primary; `set_primary` clears others; a destroyed sole
factory cancels + refunds its queue; determinism.

## §5 Wiring, sidebar tabs, rendering, demo gate

- **Sidebar tabs come alive** (sidebar.hpp + main.cpp): track active tab
  0–3; clicking a tab switches the visible cameo strip and which queue's
  build-clock/ready overlays show. Buildable lists per tab: tab 0
  (Structures) & tab 1 (Defenses) filter the building roster by
  `build_category`; tabs 2 (Infantry) & 3 (Vehicle) list the unit roster.
  Active tab full-bright, rest shaded (existing shading generalized).
  Cameo click starts a build in that tab's queue — structures/defenses
  keep the existing ready→place flow; infantry/vehicles feed the §4 exit
  handoff.
- **Roster & assets:** extend the building roster with a couple of Allied
  defense structures (e.g. pillbox GAPILL, patriot GATOW) so tab 1 is
  non-empty (SHP/cameo loaded + opt-guarded like existing chrome). Add a
  curated Allied unit roster — infantry GI (+ one more) and 2–3 vehicles
  including the Grizzly MTNK (the harvester already comes via FreeUnit) —
  combat/voxel assets through the proven Rhino/SHP pipelines, cameos like
  building cameos. Missing assets drop the producible from its tab (never
  a crash). Player Allied; the enemy Soviet base does NOT produce
  (player-only).
- **Rendering:** exiting units draw through the existing unit path — no
  new unit draw. New chrome (all [RE-FALLBACK]/divergence-tagged like the
  READY text): a primary-factory marker over the primary factory, and a
  rally marker at the rally cell while that factory is selected.
- **Input** (input.cpp): tab click → switch tab; cameo click → start in
  the tab's queue; right-click own ground with a factory selected →
  `World::set_rally`; the primary-set trigger per §1 (e.g. clicking an
  already-built factory → `World::set_primary`). Structure/defense
  placement and all existing right-click resolution unchanged.
- **Demo gate `--demo-prod`** (new permanent byte-identity gate):
  deterministic scripted run — queue a Grizzly in GAWEAP → witness
  `frames=… charge=…`; unit exits at the recovered exit cell → witness
  exit cell + credits; set a rally, build another → witness it paths to
  rally; queue GI in the barracks concurrently → witness two queues
  advancing together; add a second war factory + `set_primary` → witness
  which factory the next unit exits from; final `prod_hash` (FNV over
  `world_hash` + each queue's state + spawned unit cells/types), two-run
  byte-compare; screenshots to scratchpad. Cap the run with ok=false
  diagnostics on timeout.
- **Existing gates stay byte-identical:** `--demo-combat` logs=78,
  `--demo-tank` logs=144, `--demo-base` hash=5273b5e71ca009e1,
  `--demo-econ` econ_hash=7fed96cb98b68017.
- **Finalize:** full suite both modes (with/without RA2_INSTALL_DIR),
  arm64 NDK build (NDK D:\android-ndk-r27c), README updated with
  adjudicated deviations.

## Constraints (project-wide, unchanged)

- Run executables from PowerShell, never Git Bash.
- No game assets, gamemd.exe, or Ghidra project committed; GPL sources
  read off-repo, prose conclusions only; recovered numeric constants are
  committable.
- Pure modules registered in `cmake/ra2_sources.cmake` (and
  `CMakeLists.txt`).
- Repo on master, solo, no remote; commits end
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- Scratch files to the session scratchpad.
- Tests skip cleanly (MESSAGE + return) without `RA2_INSTALL_DIR`
  (default `D:\Red Alert 2`).

## Testing summary

| Layer | Tests |
|---|---|
| ra2assets | classification accessors vs real-map/rules pins; skip-clean without install |
| engine/production | multi-queue 54-step economy, concurrency, multi-factory speedup, unit prereq gating, BuildingType-path-unchanged assertion |
| engine/factory | exit cell, ready/hold, rally path, primary selection/clear, factory-died cancel, determinism |
| integration | `--demo-prod` witnesses + `prod_hash` (new permanent gate) |
| regression | four existing demo gates byte-identical; full suite both modes; arm64 build |
