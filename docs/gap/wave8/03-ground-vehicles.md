# W8 interaction audit 03: ground vehicles (Track / Wheel) × terrain and layers × states

Auditor: w8int-03 · master 34cb652 · build `D:\ra2-android\build\ra2engine.exe` · original: gamemd.exe YR 1.000 (Ghidra, private copy since deleted).
Evidence: `D:\ra2-re\w8int-03-out\` (Ghidra dumps `d1..d11.txt`, play scripts and runs under `play\t1..t14`, terrain dumps `play\*_terrain.txt`).

## Summary

Family: Grizzly, Rhino, Apocalypse, IFV, Flak Track, Prism, Mirage, Tesla Tank, V3, War Miner, Chrono Miner, MCVs, Terror Drone, Demolition Truck, civilian cars, plus the Robot Tank (Hover) for comparison.

Driving on clear, road and rough ground, onto and off high-bridge decks, the height gate on ramps and cliffs, Beach blocking, harvesting on a coastal map, scatter respecting the deck, and transport unload on a deck all match the original. The gaps cluster in four areas:

1. **Cell legality ignores overlays.** The original's per-cell land recalculation (`FUN_0047D1C0`) and build test (`FUN_0047C530`) both read the cell's overlay. The clone reads only the tile, except for intact low-bridge pieces. As a result:
   - an MCV deploys, and any building is placed, on ore, on slopes and on bridge cells (W8-03-01, [CONFIRMED-PLAY]: a ConYard unfolded in the middle of an ore field);
   - SROCK, TROCK and FENCE overlays (`Land=Rock`, `NoUseTileLandType=true`) can be driven through (W8-03-02);
   - every vehicle crosses the water span of an intact low bridge at half speed, because the land under it reads as Water instead of Road (W8-03-03, [CONFIRMED-PLAY] 0.498 vs 0.996).
2. **One occupancy layer.** A tank driving on a high-bridge deck scatters the ship or swimmer below it (W8-03-04, [CONFIRMED-PLAY] twice). In the original the deck and the ground under it have separate object lists.
3. **Bridge collapse uses the wrong victim rules.**
   - High bridge: units under the falling span live, though the original kills them. Units on the deck die at once instead of falling. Passengers escape (a SEAL swims away from a dead IFV), though the original sets PreventPassengerEscape (W8-03-05/06, [CONFIRMED-PLAY]).
   - Low bridge: the Hover Robot Tank dies even though it can float, and only one of three infantry in a cell dies (W8-03-07, [CONFIRMED-PLAY]).
4. **The 3-D range ignores the deck.** The elevation bonus and the cliff line-of-fire tracer skip the original's +4 levels for bridge cells, and a deck unit's z is its water-level cell (W8-03-08). Shell hits on a high deck never roll the bridge break on maps that have no low bridge (W8-03-09).

Severity counts: MAJOR 2 (01, 02), MINOR 6 (03-09, with 05/06 split), SUSPECTED 1 (10).

## Matrix

Unit groups:
- **TK** = turreted/armed tanks: MTNK, HTNK, APOC, SREF, MGTK, TTNK.
- **TR** = transports: FV, HTK.
- **HV** = harvesters: HARV, CMIN.
- **MCV** = AMCV, SMCV.
- **SP** = DRON, DTRUCK, V3.
- **CIV** = civilian cars.
- **HOV** = ROBO (comparison only).

| Terrain / layer / state | TK | TR | HV | MCV | SP | CIV | HOV | Notes |
|---|---|---|---|---|---|---|---|---|
| Clear / road / rough: reach, speed | OK | OK | OK | OK | OK | OK | OK | t9 `uspeed` 0.996 on Rough and Road |
| Ore field speed (Track 70% / Wheel 50%) | OK | OK | OK | OK | OK | OK | OK | `speed_land_at` maps ore to Tiberium (`world.cpp:691`) |
| Beach blocks Track/Wheel | OK | OK | OK | OK | OK | OK | OK (Hover crosses) | 4 maps: no walkable Beach except bridge Road; Bay of Pigs has no Beach cells at all |
| Harvest routes / AI on a coastal map | n/a | n/a | OK | n/a | n/a | n/a | n/a | t12 Bay of Pigs: AI miner harvests and returns (bales 18 → 6) |
| Rock overlays (SROCK / TROCK / FENCE) | **GAP 02** | **GAP 02** | **GAP 02** | **GAP 02** | **GAP 02** | **GAP 02** | **GAP 02** | |
| Low bridge: drive on/off | OK | OK | OK | OK | OK | OK | OK | world-depth.play, t9 |
| Low bridge: speed over the water span | **GAP 03** | **GAP 03** | **GAP 03** | **GAP 03** | **GAP 03** | **GAP 03** | n/a (Walk/Hover do not read it) | |
| Low bridge destroyed under the unit | OK (dies) | OK (dies) | OK | OK | OK | OK | **GAP 07** | and infantry: GAP 07 |
| Reaching a destroyed span; repair; re-path | OK | OK | OK | OK | OK | OK | OK | t9 (hut repair), move-bridge-cliff |
| High bridge deck: on/off, height | OK | OK | OK | OK | OK | OK | OK | t1/t2 `udeck=1`, `uheight=6` |
| Under a high bridge (another unit at the same cell) | **GAP 04** | **GAP 04** | **GAP 04** | **GAP 04** | **GAP 04** | **GAP 04** | **GAP 04** | t1 ship, t13 SEAL scattered |
| High bridge destroyed: deck units | OK (die), cosmetic: no fall | **GAP 06** (passengers) | OK | OK | OK | OK | OK | |
| High bridge destroyed: units under the span | **GAP 05** | **GAP 05** | **GAP 05** | **GAP 05** | **GAP 05** | **GAP 05** | **GAP 05** | ships and swimmers too |
| Ramps / slopes / voxel tilt | OK | OK | OK | OK | OK | OK | OK | W7-12-34 known, no new gameplay gap |
| Cliffs (height gate) | OK | OK | OK | OK | OK | OK | OK | move-bridge-cliff; scatter respects the gate (`unit_orders.cpp:116`) |
| Walls | OK | OK | OK | OK | OK | OK | OK | W6 wall model, not re-audited |
| Turret 3-D range deck ↔ ground | **GAP 08** | **GAP 08** | n/a | n/a | **GAP 08** (V3) | n/a | **GAP 08** | |
| Crushing infantry on a deck | **GAP 04** | n/a | **GAP 04** | **GAP 04** | n/a | n/a | **GAP 04** | the crush/scatter test is single-layer |
| Crushing infantry on a slope / beach | OK | n/a | OK | OK | n/a | n/a | OK | same occupancy path as flat ground |
| Head-on push (w7r2) / scatter on a deck | OK | OK | OK | OK | OK | OK | OK | scatter cells pass `path_step_layer`; no push into water |
| MCV deploy on ore / slope / bridge | n/a | n/a | n/a | **GAP 01** | n/a | n/a | n/a | t8 |
| MCV deploy near a beach | n/a | n/a | n/a | OK | n/a | n/a | n/a | Beach is closed to the passable grid |
| Harvester docking near water | n/a | n/a | OK | n/a | n/a | n/a | n/a | `dock_approach_cell` fallback (`harvest.cpp`) |
| Chrono Miner teleport destination | n/a | n/a | OK | n/a | n/a | n/a | n/a | passable + free, ring deflect ≤ 8; a deck cell syncs `on_deck` |
| Chronosphere → water / cliff | known W7-08-08 | | | | | | | |
| Chronosphere → deck | OK | OK | OK | OK | OK | OK | OK | deck-only cell → `on_deck=1` (`sync_deck_layer`) |
| Iron Curtain on a vehicle when the bridge goes | OK | OK | OK | OK | OK | OK | OK | the original's fall kill uses ignoreDefenses=1, so the curtain does not save it; the clone's `ability_kill_unit` matches |
| Terror Drone infesting a vehicle on a deck | n/a | n/a | n/a | n/a | not reproduced | | | ALL01T reinforcements killed the drone; pop-out cell: W8-03-10 [SUSPECTED] |
| Death on a deck (layer, explosion) | OK | OK | OK | OK | OK | OK | OK | `on_deck` kept while dying; the water-death branch skips deck units (`world_an.cpp:146`) |
| Passengers: transport dies on an intact deck | n/a | OK | n/a | n/a | n/a | n/a | n/a | t2: E2 at (82,51), `udeck=1`, h6 |
| Passengers: transport dies on water / collapse | n/a | **GAP 06** | n/a | n/a | n/a | n/a | n/a | |
| Passengers: transport dies at a cliff edge | n/a | SUSPECTED 10 | | | | | | no cliff-top/foot pair on ALL01T to reproduce |
| Demolition Truck blast on a bridge | n/a | n/a | n/a | n/a | OK | n/a | n/a | DemobombWH has no Wall=yes; the original's bridge roll needs Wall (0x489E2x), and so does the clone |
| Deck hit by shells (bridge roll) | **GAP 09** | | | | | | | |

## Gaps

### W8-03-01: MCV deploy and building placement accept ore, slopes and bridge cells (MAJOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_0047C530` CellClass::CanThisExistHere, reached from the MCV deploy legality `FUN_007154F0` (caller 0x7155A9, `mcv-deploy.md` §2.3)]. For a building (speed type -1) the cell is refused when:
    - it carries any overlay. When the cell's overlay +0x44 is not -1, the only exits are the wall/tiberium special cases for flagged types. During play (`DAT_00a8dcd3 == 0`) the function then `return 0`;
    - its flags +0x140 have 0x100 (a bridge) or 0x400 set;
    - its slope index +0x11C is not 0;
    - or the LandType's `Buildable` (the table at `DAT_0089D9D0`, stride 0x24) is false.
  - [CONFIRMED-PLAY `play/t8`, May Day]: `cheat spawn AMCV 0 cell 112 95` in the middle of the ore field, then `key d`. Result: `built(0,GACNST)=1` after 7 frames, in place. `t8/mcv2_before.png` and `mcv2_after.png` show the ConYard sitting on ore.
- **Original vs clone**
  - Original: "Cannot deploy here". The player MCV hears EVA_CannotDeployHere; the 36-offset search runs only from the try-to-deploy order.
  - Clone: `placement_check` (`engine/building.cpp:35-36`) tests only `passable[...] && occ.is_free`. Callers: `World::can_deploy_at` (`world_mcv.cpp:55-72`), the sidebar placement (`main.cpp:5428`) and the AI (`world.cpp:3481`).
- **Root cause:** `construction.md` §9 recorded the per-cell predicate as RE-FALLBACK ("passability is the analogue"). Ore, Road-over-bridge and ramp cells are passable, so they pass.
- **Fix:** add a `buildable_` grid built from the same inputs as `passable_`:
  - Buildable[LandType] from rulesmd `[Clear]..[Weeds] Buildable=`;
  - no overlay (ore included; keep the wall exception for wall types);
  - `ramp == 0`;
  - not a high- or low-bridge cell.

  Then AND it into `placement_check`, keeping one predicate for deploy, sidebar and AI.
- **Test:** --play on May Day: spawn an AMCV at (112,95) on ore, press `d`. Expect `built(0,GACNST)==0`, and the MCV still exists after 60 frames. Unit test: `placement_check` over a 2×2 with one ore, one ramp or one bridge cell fails.

### W8-03-02: Rock and fence overlays (`Land=Rock`, `NoUseTileLandType=true`) are passable (MAJOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_0047D1C0`, overlay branch]. The cell takes `LandType = OverlayType+0x298 (Land)`. When Land is Wall (4) or Railroad (9), or `+0x2AC` (NoUseTileLandType) is set, it keeps that type and returns before the tile land is read.
  - [CONFIRMED-DATA rulesmd]: SROCK01-05, TROCK01-05 and FENCE01-22 have `Land=Rock`, `NoUseTileLandType=true`, and [Rock] is 0% for every SpeedType. CRAT/DRUM/PALET have `Land=Rock` without NoUseTileLandType, so the tile wins and they stay passable.
  - [CONFIRMED-PLAY `play/t11`, SOV10T]: `view_key` puts SROCK05 at (125,64), TROCK01 at (128,58) and TROCK02 at (133,57). `map_terrain` shows all three with `walkable=1`, land Clear.
- **Original vs clone**
  - Original: the rock cells are impassable.
  - Clone: only low-bridge overlays override the land (`engine/world_depth.inc:24-35`). `resolve_land_types` says "the overlay branch of FUN_0047D1C0 ... is not modelled" (`main.cpp:362-363`). Every ground unit paths through the rocks on SOV02/04/06/07/10/11/12, and the cells are also buildable.
- **Fix:** in `depth_set_terrain`, for every overlay whose type has `NoUseTileLandType=true`, or `Land=Wall`/`Railroad`, set `lt[i]` to the overlay's Land before `set_land_terrain`. Do the same in `w5_arm_combat`'s speed-land grid, so Railroad speeds apply too.
- **Test:** --play SOV10T: `cheat camp_spawn HTNK 0 124 64`, `camp_move HTNK 126 64`. Expect it never to stand on (125,64), and `map_terrain` to show `walkable=0` there.

### W8-03-03: Intact low-bridge cells keep the tile's Water land for speed; vehicles cross at half speed (MINOR)

- **Evidence**
  - [CONFIRMED-PLAY `play/t9`, May Day after the hut repair]: a Grizzly on bridge cells over Rough has `uland=7`, `uspeed=0.996`. Over the river span (73,118) it has `uland=2` (Water), `uspeed=0.498`. Back on Road, 0.996.
  - [CONFIRMED-RE `FUN_0047D1C0` + DATA]: LOBRDG01-26 / LOBRDB have `Land=Road`, `NoUseTileLandType=true`, so the cell's LandType is Road (Track/Wheel 100%).
- **Clone:** `w5_arm_combat` fills `speed_land_` from the TMP byte only (`engine/w5_combat_arm.inc:50-63`). [Water] Track is 0, which `land_speed_mult` turns into its 0.5 fallback (`engine/world_loco.cpp:171`).
- **Root cause:** the low-bridge Road override is applied to `passable_` (`world_depth.inc:33`) but not to `speed_land_`. `set_bridge_cell` (`world_depth.cpp:438-445`) does not update it either when a section is broken or repaired.
- **Fix:** have `speed_land_at` return Road (1) for an intact low-bridge cell (`bridge_overlay_at >= 0 && !low_bridge_destroyed`), and the tile type for a destroyed one. Do the same for an `on_deck` unit on a high deck, where the original stands on the bridge, not on the water's land type.
- **Test:** t9-style. After the repair, `uspeed(0,MTNK,0)` at `ucx==73` must equal its value at `ucx==68`.

### W8-03-04: A high-bridge deck and the ground under it share one occupancy layer (MINOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_0047E7B0` / `FUN_0047E9A0`]: CellClass add and remove an object on list +0xE4 (ground) or +0xE8 (bridge) chosen by the object's OnBridge byte +0x8C (the caller `FUN_00567EF0` passes `obj+0x8C`). IsCellOccupied and crush checks walk one list.
  - [CONFIRMED-PLAY `play/t1`]: an own Destroyer parked under the deck at (82,53). A Grizzly ordered over the bridge to (82,58) displaced it to (83,54) through the W7-11-09 friendly scatter.
  - [CONFIRMED-PLAY `play/t13`]: an own SEAL swimming at (82,53) under the deck was pushed to (81,53) when a Grizzly drove over the deck.
- **Clone:** `Occupancy` holds one owner per cell (`engine/occupancy.hpp:102-110`). `reserve_cell` and the scatter request (`world_loco.cpp:239`, `485-506`) see the unit below as a blocker. Crush tests (`can_crush`) see the infantry below as a victim.
- **Effect:**
  - Deck traffic shoves boats and swimmers out from under bridges.
  - A non-allied unit below stops deck traffic.
  - Over a deck on land (`hbridge_` value 2), a tank on the deck can run over infantry walking under it.
- **Fix:** add a second occupancy plane (or a layer bit per owner/spot) keyed by `on_deck`. `reserve_cell`, `can_enter`, the scatter request and the crush target must consult only the mover's layer for the destination cell.
- **Test:** t13. With the deck intact, `ucell(0,GHOST,0)` stays 82053 while `ucell(0,MTNK,0)` passes 82053 to 82058.

### W8-03-05: High-bridge collapse spares units under the span; deck units vanish instead of falling (MINOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_0047DC80`, called on every section cell by `FUN_00570FC0` / `FUN_0056EA80` / `FUN_0056EE20` / `FUN_0047E380`]:
    1. Every object on the cell's ground list (+0xE4, the units UNDER the bridge) gets `ReceiveDamage(&Health, 0, Rules+0xFA8 warhead, 0, ignoreDefenses=1, preventPassengerEscape=1, 0)`, which kills it.
    2. Every object on the bridge list (+0xE8) gets vt+0xEC = `FUN_005F3970`. That function sets IsFallingDown (+0x8D) and a fall-kill flag (+0x8F), clears OnBridge and re-files the object on the ground list.
    3. Then debris anims are spawned (Rules+0x15C/+0x168).
  - `ObjectClass` AI `FUN_005F3680`: a falling object drops under gravity (Rules+0x7B8/0x7BC). On reaching the ground with +0x8F set, it gets the same lethal `ReceiveDamage(..., 1, 1, 0)`.
  - [CONFIRMED-PLAY `play/t4`]: a Destroyer spawned under the deck at (82,54) and a Grizzly on the deck at (82,52). `bridge_hit 82 53 100000`: the Grizzly dies, but `units(0,DEST)=1`.
- **Clone:** `break_high_bridge` (`engine/world_depth.cpp:579-591`) kills only `occ_.at(cell)` when `on_deck != 0`. Its comment says "an amphibious unit swimming under it lives". Deck units get `ability_kill_unit` at deck height, with no fall.
- **Fix:**
  - Kill every unit in the span's cells whose `on_deck == 0` (ships, swimmers, ground units under a deck over land).
  - For `on_deck == 1` units, start a fall: clear `on_deck`, animate z from deck height to ground, and kill on landing.
  - Iterate all occupants (infantry spots), not `occ_.at`.
- **Test:** t4, plus `assert units(0,DEST) == 0` after the collapse.

### W8-03-06: Passengers escape a transport killed by a bridge collapse (MINOR)

- **Evidence**
  - [CONFIRMED-RE]: both collapse kills, `FUN_00487920` (low) and `FUN_0047DC80` / `FUN_005F3680` (high), pass `preventPassengerEscape=1` (6th ReceiveDamage argument; UnitClass::ReceiveDamage is vt+0x16C = `FUN_00736D60`).
  - [CONFIRMED-PLAY `play/t3`]: an IFV carrying a SEAL on the deck at (82,52) when the span collapses. The IFV dies, the SEAL survives (`ucell(0,GHOST,0)=82052`, `udeck=0`, `uheight=2`, swimming).
- **Clone:** `step_cargo` (`engine/world.cpp:1947-1955`) applies the W7-04-02 escape rule to every transport death. The only kill-all cases are aircraft, Explodes, Suicide and sinking. Non-amphibious passengers die only by accident (they cannot enter the water cell).
- **Fix:** add a `prevent_passenger_escape` flag on the unit, set by the collapse kills (`set_bridge_cell`, `break_high_bridge`, the future fall landing). Include it in `kill_all`.
- **Test:** t3, with `assert units(0,GHOST) == 0`.

### W8-03-07: Low-bridge collapse kills the wrong set: Hover dies, extra infantry and jumpjets survive (MINOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_00487920` (called with destroy=1 from `FUN_0057B720` / `FUN_0057BCE0` and the other section rewrites)]. For every object on the cell:
    - when its `IsCellOccupied` (vt+0x1AC, for a unit `FUN_0073DFA0`) on the now-recalculated cell returns 7 (no), it is killed with ignoreDefenses and preventPassengerEscape;
    - in destroy mode, a techno whose type has `JumpJet` (+0xD94, read as "JumpJet" by `FUN_00711510`) is also killed;
    - a 5×5 scan kills movers whose locomotor is heading into the cell and who could not stand there.

    A Hover unit (ROBO, `Hover` on [Water] 100%) is not killed.
  - [CONFIRMED-PLAY `play/t10`]: a ROBO at (73,117) and a Prism at (73,119) on one river section. Three `bridge_hit` later, both are dead (`units(0,ROBO)=0`).
  - [CONFIRMED-PLAY `play/t14`]: three Conscripts in (73,118). After the collapse 2 of them are alive in spots 3 and 4, standing on Water (`uland=2`).
- **Clone:** `set_bridge_cell` (`engine/world_depth.cpp:448-455`) kills only `occ_.at(x,y)` (the first spot), and only if `!is_naval && passable_[i]==0`, which is the Land grid, so it ignores Hover/amphibious domains. It also ignores movers heading into the cell.
- **Fix:**
  - Iterate every occupant (vehicle owner and the 3 infantry spots), plus units whose next path cell is this one.
  - Kill when `!unit_can_enter_cell(u, x, y)` or the unit is a JumpJet on the ground.
  - Pass the passenger rule from W8-03-06.
- **Test:** t10 expects `units(0,ROBO)==1` and `units(0,SREF)==0`; t14 expects `units(0,E2)==0`.

### W8-03-08: Elevation bonus, cliff line-of-fire and unit z ignore the bridge height (MINOR)

- **Evidence** [CONFIRMED-RE `FUN_006F6490` (elevation bonus) and `FUN_004CC280` / `FUN_004CC5A0` (cliff tracer)]: both read the cell height through `FUN_00487C60` = `Level + 4 * ((flags +0x140 >> 7) & 1)`. A bridge cell counts 4 levels higher. Unit coordinates on a deck carry BridgeHeight (416) in z.
- **Clone** `engine/combat.cpp`:
  - `elevation_bonus` is called with `cell_level(shooter) - cell_level(target)` (`:801-802`);
  - `cliff_blocks` uses `cell_level` (`:760`);
  - `unit_lept_z` = `u.level * 104` (`:734-735`). No term uses `on_deck` or the high-bridge cells.
- **Effect:**
  - A tank, Prism or V3 on a deck firing at targets below loses the +ElevationIncrementBonus (2 cells, capped) the original grants for 4 levels.
  - Shooters below the bridge are not blocked by the deck in the cliff tracer (the original's +4 step blocks a shot at > 3 levels).
  - The deck ↔ water-level 3-D distance omits about 416 leptons of height.
- **Fix:**
  - Use `cell_level + (high_bridge_cell && !destroyed ? 4 : 0)` in both the bonus and the cliff tracer, which is the per-cell rule of 0x487C60.
  - Use `u.level + 4*on_deck` in `unit_lept_z` (and in the projectile impact z).
- **Test:** unit test on a World with a high-bridge deck: `weapon_in_range` from a deck unit to a water target at range + 1.5 cells succeeds once the bonus is applied. --play on ALL01T: an HTNK on (82,53) engages a ship at about 7.5 cells.

### W8-03-09: Shell hits on a high-bridge deck never roll the bridge break when the map has no low bridge; no deck-height band (MINOR)

- **Evidence**
  - [CONFIRMED-RE `FUN_00489190` @0x489E87..0x48A0DD]: a Wall=yes warhead (with the DestroyableBridges option bit `*DAT_00a8a198 & 0x8000`) exploding in a bridge cell (flag 0x100, deck overlay 0x18/0x19/0xED/0xEE) rolls `Random(1, BridgeStrength) < damage`. That only happens when the explosion z is inside `[(Level-2)*CellHeight + BridgeHeight, (Level+1)*CellHeight + BridgeHeight]`, i.e. at deck height.
  - Clone: `engine/world.cpp:6594` gates the projectile-path call on `!bridge_ovl_.empty()`, which only holds on maps with a LOW bridge. On ALL01T (high bridges only) a Grizzly/AP shell that kills a unit on the deck never rolls; only the force-fire cell path (`world_depth.cpp:726`) does. `bridge_explosion` (`world_depth.cpp:463-475`) has no z test, so on maps that do roll, a shell at water level under the deck can break it.
- **Fix:** gate on `!bridge_ovl_.empty() || !hbridge_.empty()`. Pass `impact_z`, and apply the deck band for high-bridge cells.
- **Test:** unit test: `bridge_explosion` on a high cell with a ground-level z returns 0; at deck z with BridgeStrength 1 it returns > 0. --play ALL01T: a projectile kill on the deck with a forced RNG breaks the span.

### W8-03-10: A drone and transport passengers popping out of a dead host have no height gate [SUSPECTED] (MINOR)

- **Code:**
  - the drone pop-out uses `nearest_free_cells({u.cell_x,u.cell_y}, 1, passable_, ...)` (`engine/world_abilities.cpp:572`);
  - the transport-death eject uses `nearest_free_cells(... grid_for(domain) ...)` (`world.cpp:1955-1956`).

  Neither applies `path_step_layer`. The fv7c fix (13a #1) gated only the unload eject (`world.cpp:2131`). The original unlimbos at the host's own coordinate.
- **Status:** not reproduced. ALL01T and May Day have no walkable cliff-top cell next to a walkable cell 2+ levels lower (scan of `play/all01t_terrain.txt`). The drone run on the ALL01T deck was killed by mission reinforcements before it infested.
- **Fix:** reuse the `free_eject` lambda (height gate from the host's cell and layer) for both.
- **Test:** a World unit test with a 4-level step next to the host, expecting no placement below.

## Verified OK (this audit)

- **Deck traversal.** Ground units climb onto and leave the ALL01T high deck. A deck unit reads `udeck=1` and `uheight=6` (t1, t2, t3, t4). Scatter destinations respect the height gate and the deck (`unit_orders.cpp:116`), so no vehicle is scattered off a deck into water.
- **Transport death on an intact deck.** The passenger is placed on an adjacent deck cell at deck height (t2: E2 at (82,51), h6, `udeck=1`).
- **Beach blocking.** Beach (TMP land 6) is closed to Track/Wheel on ALL01T, SOV10T and May Day; the only walkable Beach cells are intact low-bridge Road. Harvesting works on Bay of Pigs (AI miner cycles; the map has no Beach cells). Ore on flat cells reads as Tiberium for speed.
- **Low bridge mechanics.**
  - Repair at the hut, crossing and re-path work (t9).
  - Ground vehicles on a collapsing low section die (t10, Prism).
  - DTRUCK's Demobomb (no Wall=yes) does not touch bridges, as in the original (`FUN_00489190` requires the warhead's Wall flag for every bridge branch).
- **Iron Curtain and the bridge.** The original's collapse and fall kills use ignoreDefenses=1, so the curtain does not save a vehicle on a destroyed bridge. The clone's `ability_kill_unit` also ignores the curtain.
- **Other.**
  - Chrono Miner warp: passable plus free target, ring deflect, and a deck cell syncs `on_deck`.
  - Dying on a deck keeps the deck layer, and the water-death branch skips deck units (`world_an.cpp:146`).
  - Known and not re-reported: W7-08-08 (Chronosphere water/cliff), W7-06-14 (TooBigToFitUnderBridge draw), W7-12-34 (ramp tilt), W7-04-02 (passenger escape, implemented), W7-11-09 (scatter), W7-12-13/14.
