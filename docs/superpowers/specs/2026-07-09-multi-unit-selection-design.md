# Phase A: Multi-Unit Selection & Group Movement — Design

**Date:** 2026-07-09
**Status:** Design approved; pending implementation plan.
**Depends on:** Phase 2 vertical slice (single GI unit, tap-select + tap-move,
A\* pathfinding, iso projection, terrain + unit rendering, camera).

## Goal

Turn the engine's single controllable unit into a controllable *army*: multiple
units, RA2-classic selection, and group movement with collision avoidance. This
is the prerequisite for the later combat phase. Combat itself is **out of
scope** — nothing here shoots, takes damage, or dies.

## Scope decisions (settled during brainstorming)

1. **Feature scope: full RTS control layer.** Box-select, Shift add/remove,
   double-click select-all-of-type, formation movement (units spread to distinct
   cells), and unit–unit collision avoidance. Units are all GI infantry (reusing
   the existing `GI.SHP` path); no new unit type and no voxel rendering are
   pulled in.
2. **Control model: RA2-classic mouse.** Left-click select, left-drag box-select,
   Shift+left-click add/remove, double-click select-all-of-type, right-click
   move. Camera pans via arrow keys + screen-edge scroll; left-drag no longer
   pans on desktop. A touch-adapted scheme is explicitly deferred.
3. **Movement model: grid-occupancy + reservation.** One unit per cell. A\* treats
   other units' cells as blocked; a unit reserves the next cell before entering
   it, waits when blocked, and re-paths if still blocked. Group move assigns each
   unit a distinct free cell spiralling out from the click. Deterministic and
   unit-testable; units never overlap.

## Architecture (Approach 1: extend in place)

The existing "thin `World` + pure free-function modules + SDL confined to
renderer/platform" structure is preserved. The two genuinely tricky algorithms
are factored into their own **pure** modules (like `find_path`, `iso_project`,
`build_passability`) so they are exhaustively testable without a window or a
real map. `Unit` stays a plain data struct and movement/selection/collision stay
pure functions over unit data — so the later combat phase can migrate to an ECS
mechanically rather than via a rewrite.

| File | Change |
|---|---|
| `engine/platform.hpp/.cpp` | Extend `InputEvent`: add `button` (Left/Right) on Down/Up, a `shift` modifier flag, a `t_ms` timestamp, and keyboard events for arrow-pan. `Platform::poll` populates them from SDL. |
| `engine/world.hpp/.cpp` | `Unit unit_` → `std::vector<Unit> units_`. Selection helpers (query/clear/add/toggle), a cell-occupancy grid, group `order_move(goal)` over the selection, per-unit `step` honoring reservations. |
| **new** `engine/group_move.hpp/.cpp` | **Pure**: `assign_formation(goal, selected_unit_cells, passable, occupancy)` → a target cell per unit, via nearest-first spiral + greedy nearest-unit assignment. |
| **new** `engine/occupancy.hpp/.cpp` | **Pure**: cell-occupancy + next-cell reservation helpers (cell owner, can-enter, reserve/release). |
| `engine/input.hpp/.cpp` | Gesture state machine implementing the RA2-classic model; exposes the live selection-box rect for the renderer. |
| `engine/iso_renderer.*` | Render loop iterates `units_` (marker + sprite per unit, depth-sorted among units); new `draw_selection_box`. |
| `engine/main.cpp` | `build_world` spawns a small GI squad near the start cell; render/step loops iterate units; new headless `--demo-group` verification mode. |

## Migration of existing code

Changing `World`'s single `Unit unit_` to `std::vector<Unit> units_` and
replacing the tap/drag-pan gesture model are **breaking** changes to existing
callers, which the plan must migrate as part of the work (existing tests must
stay green, not be deleted):

- `engine/main.cpp` uses `world.unit()` throughout the `--demo-move` and
  `--demo-retarget` modes and the render helper. These migrate to operate on the
  spawned squad / the current selection (e.g. the first unit, or an explicit
  index) so the existing headless demos still run.
- The Phase-2 `Input` tests assert the **old** model (tap = select/move, drag =
  pan). They are rewritten to assert the new RA2-classic model; the intent
  (non-vacuous gesture coverage) is preserved, the expected actions change.
- Any `World` test referencing `unit()` is updated to the multi-unit API.

## Selection

Selection is a per-unit `bool selected` (already on `Unit`). Pointer
hit-testing is **cell-based** — the unit occupying `screen_to_cell(pointer)` —
which is unambiguous precisely because occupancy guarantees one unit per cell.
Box-select membership is screen-space: a unit is boxed if its projected
ground-point falls inside the drag rectangle.

| Gesture | Action |
|---|---|
| Left-click a unit | Select only that unit (clear others) |
| Left-click empty ground | Clear selection |
| Left-drag | Box-select (replaces current selection) |
| Shift + left-click a unit | Toggle that unit in/out of the selection |
| Shift + left-drag | Add the boxed units to the selection |
| Double-click a unit | Select all on-screen units of the same type (`image`) |
| Right-click | Group-move order to the clicked cell |

Double-click is detected inside the **pure** `Input` class by comparing event
`t_ms` timestamps (with a distance threshold), so no clock is read in engine
logic and the behavior is testable from synthetic events.

## Group movement — formation assignment

Pure function `assign_formation`. From the clicked goal cell, walk a
**nearest-first spiral** outward, collecting the first N cells that are (a)
statically walkable and (b) not owned by a *non-selected* unit. Assign greedily:
each free cell in spiral order goes to the nearest not-yet-assigned selected
unit, which minimizes path-crossing. If the area is crowded (fewer free cells
than units), leftover units target the nearest reachable free cell. Each unit
then runs A\* to *its own* assigned target over a passability grid equal to the
static passability minus cells owned by other units.

## Collision — occupancy & reservation

Pure `occupancy` module, driven by `World::step`:

- An occupancy grid maps each cell → its owning unit (or none). A unit owns its
  current integer cell.
- Before advancing from cell A into the next path cell B, a unit must
  **reserve** B. If B is owned or reserved by another unit, the unit **waits**
  briefly, then **re-paths** (A\* with occupied cells blocked). If no path
  exists, it **stops** cleanly (clears its path — no spinning).
- On fully entering B, the unit releases A; ownership becomes B.
- Units are stepped in **fixed index order** every frame, so reservation
  resolution is deterministic (and therefore testable).

Explicitly-handled hard cases:

- **Head-on / mutual block** (A→B while B→A): both units wait, then re-path in
  fixed index order over a grid that treats other units' cells as blocked. In
  open ground they route around each other; in a sealed one-cell corridor both
  stop cleanly. The guarantee is **no overlap + guaranteed termination**
  (deterministic by index order) — not that a specific unit always wins the
  contested cell.
- **Boxed-in** unit: re-path fails → the unit stops.
- **One-cell-gap funnel**: units queue single-file via wait-then-proceed.

## Step loop

`World::step(dt)`: for each moving unit (index order) interpolate `fx/fy` toward
the next path cell at `move_speed`; on a cell-crossing, attempt reservation →
commit or wait/re-path; cycle the walk-frame animation; on arrival clear the
path and snap `fx/fy` onto the committed cell. Safe to call every frame
regardless of unit state (matches the existing single-unit contract).

## Rendering

The render loop iterates `units_`:

- Units are **depth-sorted among themselves** by screen `py` (`fx+fy`) so nearer
  units draw over farther ones.
- Each selected unit's green marker draws directly under its own sprite;
  selected units' path lines draw beneath all sprites.
- `draw_selection_box` (new) draws a green screen-space rectangle outline last,
  as a HUD overlay, while a left-drag is in progress.
- `draw_unit`/`draw_selection`/`draw_path` are unchanged (they already take one
  `Unit`); they are simply called per-unit in the loop.

## Testing

The pure modules carry the correctness weight — no SDL, hand-built inputs:

- **`assign_formation`**: spiral correctness, crowded/edge cases, deterministic
  greedy assignment, distinct targets.
- **`occupancy` / reservation**: reserve/release, head-on-swap tie-break,
  boxed-in stop, funnel queue.
- **`Input` gesture machine**: feed synthetic `InputEvent`s (button/shift/`t_ms`)
  and assert the resulting selection set and issued orders; click-vs-drag-vs-
  double-click thresholds; box-membership.
- **`World` group order + step**: a small hand-built map + squad, with the
  **one-unit-per-cell invariant asserted on every step**, and group arrival at
  distinct cells.
- **Headless `--demo-group`**: spawn a squad, box-select all, issue a group
  move, step to arrival, write before/after screenshots + print per-unit arrival
  cells (mirrors Task 12's `--demo-move` gate for visual confirmation).

The full suite stays green; real-asset/oracle tests still skip cleanly without
the install; the library sources still cross-compile to arm64.

## Success criteria

1. A GI squad (≈5 units) spawns on the real temperate map.
2. All six selection gestures work (unit-tested + visually confirmed via demo).
3. Right-click issues a group move; units path to **distinct** cells; **no two
   units ever occupy the same cell** (invariant asserted in tests).
4. Head-on, boxed-in, and funnel collision cases resolve with no deadlock or
   overlap (unit-tested).
5. Camera pans via arrow keys + screen-edge scroll; left-drag boxes (no longer
   pans).
6. Desktop playable; `--demo-group` before/after screenshots confirm the
   behavior; full suite green; arm64 cross-compile still succeeds.

## Non-goals (deferred to later phases)

Combat, unit health/death, a second unit type, voxel rendering, touch-control
adaptation, minimap, and HUD/production. Boundaries are kept clean
(`Unit`-as-data, pure movement/selection/collision functions, `World` free of
render/input concerns) so the combat phase can adopt an ECS mechanically.
