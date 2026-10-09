# Movement across heights, bridges and gates (R-210)

User report (ALL01, phone build from master 11ea069): "none of the troops can
walk on a bridge; even Tanya is able to jump off the cliff and swim".
Recovered from `gamemd.exe` (YR 1.001) with Ghidra headless on a private copy
of the project (`D:\ra2-re\bridge`). Addresses are this binary's; YRpp names
are hints only (their vtable addresses drift: the class vtables were located
through the MSVC RTTI type descriptors instead).

## 1. Where the original decides a step

| What | Address | Notes |
|---|---|---|
| A* inner search (`AStarClass` regular pass) | `FUN_00429A10` | called from `FUN_0042C880` (FindPath) after the hierarchical pass; keeps a ground and a bridge ("alt") plane of visit marks and distances |
| per-neighbour test | `(*foot->vtable + 0x1AC)(cell, dir, level, cur_cell, alt)` | `InfantryClass` `FUN_0051BD70`, `UnitClass` `FUN_0073DFA0` (vtables `0x7EA058`, `0x7F4C70` by RTTI); returns a `Move` (0 OK .. 7 No); the A* accepts `< 7` |
| height gate | `+0x1B0` = `FUN_004D9B70` (FootClass, shared by both) | below |
| cell passability | `CellClass::IsClearToMove` `FUN_004833B0` | level / bridge layer + `Ground[LandType][SpeedType] == 0` |
| locomotor hook | `ILocomotion::Can_Enter_Cell` (+0x1C) | `0x55A880` returns 0 for Drive / Walk / Ship / Hover / Mech: every locomotor relies on the FootClass gate above |

CellClass fields used: `+0x11A` Height, `+0x11B` Level, `+0x11C` SlopeIndex
(the TMP ramp type), `+0xEC` LandType, `+0x140` flags (`0x100` the cell
carries a high-bridge deck, `0x200` a deck end that can be entered from the
high ground). `CellClass::GetLevel` adds 4 (`BridgeLevels`) on a bridge cell.

## 2. The height gate `FUN_004D9B70(dest, dir, &level, &on_bridge, src)`

`src` defaults to the neighbour of `dest` in the opposite direction. With
`srcEff = (src has 0x100) ? src.Level : level` (the mover's height) and
`diff = srcEff - dest.Level`:

* `diff == 0`: allowed, unless the mover is at a different height than the
  cell (`level != dest.Level`) and this is not a deck-to-deck step
  (`dest 0x100 && dest 0x200 && src 0x100`) -- a unit on a deck cannot drop
  to the ground of the next cell, a unit under a bridge stays under it.
* `|diff| == 1`: allowed only when the LOWER cell is a slope (going up:
  `src.SlopeIndex != 0`; going down: `dest.SlopeIndex != 0`).
* `|diff| == 4`: the bridge ends -- `src.Level == dest.Level - 4` (stepping
  off a deck onto high ground) needs the mover at `dest.Level` on a bridge
  cell; `dest.Level == src.Level - 4` (high ground onto a deck) needs
  `dest 0x100 && 0x200` and sets `on_bridge`.
* any other difference (2, 3, 5+): `Move::No` -- a cliff.

The same call gates the A* neighbours and every cell a locomotor enters, so
no locomotor can step down a cliff: foot, wheel/track, amphibious, hover and
ship units all go through `IsCellOccupied` -> `FUN_004D9B70`.

`IsClearToMove` (`FUN_004833B0`): with a level given, the cell must be at
that level, or carry a deck at `Level + 4`; on the deck the occupation flags
of the bridge layer (`+0x128`) are read and the `Ground[LandType][SpeedType]
== 0` test is skipped (the deck is always passable to ground units), while
the ground under it keeps its land type (water: ships pass under).

## 3. Gates (`InfantryClass FUN_0051BD70`, `UnitClass FUN_0073DFA0`)

A building in the target cell whose type has `+0x16B7` (`Gate=yes`):
open (`FUN_004525E0`) -> passable; else `gate_owner->IsAlliedWith(mover_owner)`
(`FUN_004F9870`: same house, or the owner's `+0x5788` Allies bit; the call is
`ECX = building->Owner (+0x21C)`, argument the mover's owner) ->
`Move::ClosedGate` (3, passable, the gate opens); else an armed mover
(`+0x2AC` -> `FUN_007005C0`, weapon 0 present) -> `Move::Destroyable` (5),
an unarmed one `Move::No`. `HouseClass::MakeAlly` (`FUN_004F9990`) sets only
the caller's own bit (alliances are one-sided).

## 4. ALL01 findings

* Both high bridges (`(62,57..60)` Liberty Island, `(82,51..55)` Fort
  Bradley) are broken at mission start by the map's own `Break Bridges`
  trigger (elapsed time 0, `Apply100Damage` x5 at waypoints 80 / 79), as in
  the original (`Mis_A1_EvaBridgeDestroyed`, `MISSION:ALL01f`/`p`: build a
  barracks, send an ENGINEER into a repair hut). The clone did that already.
* After the hut repair the GIs still could not reach the bridge: the
  fort's exit onto it is a `GAGATE_A` (Civie1, tagged `Change House 2b`, so
  the player's after the rendezvous) and the clone treated every gate as a
  solid building. **Fixed:** allied gates open (rule above).
* On the repaired deck the clone's search was 2-D; it now runs on the deck
  layer (below).
* Tanya (and enemy conscripts) stepped between level 6 and level 2 cells at
  the island's seawall / the bridge abutments (`(61,62) -> (60,61)`): the 2-D
  search connected any two passable neighbours. **Fixed:** the height gate.
* Picking: a click on a cliff-top cell resolved to the river cell behind it
  (both project onto the same pixel). **Fixed** (front-most surface, below).

ALL01 terrain census (passable neighbour pairs, land grid): 11421 flat, 191
one-level steps whose lower cell has a ramp type, 4 whose higher cell has it,
none without a ramp, 64 four-level and 28 two/three-level steps. The ramp
rule therefore matches the map data (SlopeIndex = the TMP ramp byte 42).

## 5. The clone

* `engine/pathfind.{hpp,cpp}`: `PathTerrain` (level, slope = ramp type,
  per-cell deck availability 0 none / 1 deck + ground / 2 deck only, a
  version keying the memo, the start layer, the open gate cells);
  `path_step_layer` is the gate (same height, or one level over the lower
  cell's slope; a deck at Level + 4); `find_path*_t` search (cell, layer)
  states. A null terrain is the old search byte for byte.
* `World` (`engine/world_depth.cpp`): `deck_grid(domain)` from the high-bridge
  state (Land: over water = deck only, over land = both; Amphibious: both;
  Water: none), `open_gates_for(house)` (the owner's `are_allied` view),
  `terrain_for(unit)`, `sync_deck_layer`; `Unit::on_deck` (save v30).
  `step_units` refuses a step the gate refuses (the blocked re-path then finds
  a legal way) and keeps `on_deck`. A broken deck kills the units ON it; an
  amphibious unit swimming under it lives.
* Armed with the map heights (`arm_land_speed`, the real game only); every
  unit-test / gate World keeps the flat search.
* `Input::resolve_ground_cell` scans heights top-down and takes the first
  cell standing at that height (Level, + 4 on an intact deck) -- the surface
  drawn in front [RE-FALLBACK: the TS-lineage click-cell calculation was not
  traced].

Deviations / fallbacks:
* `Move::Destroyable` (an armed non-ally walking into a closed gate shoots it
  down) is not modelled: such a gate stays closed to that mover.
* The bridge deck is one cell wide (the overlay cells), as before; the
  original's side cells (`0x400` BridgeBody) are not separate.
* The corner-cutting rule for diagonal steps is unchanged (2-D).

Evidence: `scripts/play/move-bridge-cliff.play`; unit tests
`tests/test_bridge_cliff.cpp`, `tests/test_high_bridges.cpp` (R-210 case).
