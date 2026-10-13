# w7sus-loco: locomotion / naval suspects from the m-all04-real re-tune

**Auditor:** w7sus-loco (read-only).
**Tree:** D:\ra2-android master 9bf5ccd. The existing build\ra2engine.exe was used.
**Original:** gamemd.exe (YR 1.000, Sep 2001), Ghidra on a private copy of the `bridge` project. That copy has since been deleted.

**Evidence locations:**
- Outputs: `D:\ra2-re\w7sus-loco-out\`
  - `d1`–`d5`, `d3`, `d4`: decompiles
  - `s1`: string search
  - `x1`: disassembly
  - `s1/` … `s9/`: --play runs, with `*.err.txt` holding the loco traces
  - `fullgrid.txt`: the ALL04 land-type and level map
- Scripts: `D:\ra2-re\w7sus-loco-scripts\`
  - `terr.py`: decodes IsoMapPack5 (LZO1X) and reads the TMP image headers
  - `tab.py`: tabulates the dumps

| # | Suspect | Verdict |
|---|---|---|
| 1 | LCRF can't climb from the "beach" (120,68) to (119,68) | **FAITHFUL that it can't.** But (120,68) is not a beach: the clone is wrong in the other direction. **CONFIRMED BUG** (land type of the YR cliff sets, and the eject ignores height), plus a **PRE-EXISTING SCRIPT ISSUE** (m-all04 lands on a cliff face). |
| 2 | Destroyers wander after a move, chasing targets | **CONFIRMED BUG.** The clone replaces ASWLauncher `Range=-2` with 30 cells, which inflates the Guard threat scan from 9 cells to 31. Reproduced. |
| 3 | Naval blocked path / ScatterContent / head-on push | **CONFIRMED BUG** at code level: the scatter-cell search uses the LAND grid, so a ship can never be scattered. A deadlock is **NOT REPRODUCIBLE on ALL04**, which has no one-cell water lanes (0 found). |

---

## Suspect 1: hovercraft (LCRF) can't climb from (120,68) to (119,68)

### Map data
The ALL04u.MAP terrain was decoded by `terr.py`. That script reads IsoMapPack5 and then the TMP image header of each cell: byte 0x29 is the terrain type, byte 0x2A is the ramp.

| cell | Level | tile / set | TMP terrain type → LandType (DAT_008278F4) | ramp |
|---|---|---|---|---|
| (120,68) | **0** | 829 sub 1, set 78 "CliffA/Water pieces" (WClifA06) | 15 → **Rock (3)** | 0 |
| (119,68) | **4** | 829 sub 0, set 78 (WClifA06) | 15 → **Rock (3)** | 0 |
| (119..120, 64..67) | 4 / 0 | set 15 "Cliff/Water pieces" | 15 → Rock | 0 |

(120,68) is the foot of a 4-level water cliff (Chicago's seawall). It is not a beach.

The real ALL04 beach is a strip of Level-0 `Beach` cells (TMP type 10) at x≈85–95, y≈83–165. That strip is where the reinforcement scripts land: `Reinforce1..4` go to waypoints 56–59 = (90,127..133).

The whole east coast of the plateau, x≈119–121, is a cliff (see `fullgrid.txt`).

### Original
1. **The LandType comes from the TMP byte.** `CellClass` recalc `FUN_0047D1C0` calls `FUN_00544910`, which returns `DAT_008278F4[tmp_image+0x29]` into `CellClass+0xEC`.
   - The table bytes were read from the exe: `{0,8,8,8,8,10,9,3,3,2,6,1,1,0,7,3}`. Terrain type 15 is Rock.
   - An overlay land type (`+0x44`) overrides it.
2. **The cliff-back rule.** `FUN_0047D1C0` also checks `Rules+0x664`, which is `CliffBackImpassability`: disassembly `0x66EC08..0x66EC23` stores the INI value there, and rulesmd sets it to 2.
   - When the value is 2 and one of six back neighbours is 4+ levels higher, a Clear/Water/Beach/Ice cell becomes **Rock (3)**.
   - For (120,68), the neighbour (119,68) is 4 levels higher, so the cell is Rock by both rules.
3. **Passability and the height gate.**
   - Passability `Ground[Rock][Hover] = 0%` (rulesmd [Rock]). The step test `IsClearToMove FUN_004833B0` refuses it.
   - The height gate `FUN_004D9B70` (UnitClass vtbl+0x1B0, read from the exe at 0x7F4E20) refuses a |diff| = 4 step when there is no deck.
   - **Result: no unit can enter (120,68) or (119,68), and no locomotor climbs 0→4 there.** An LCRF in the original cannot beach at (120,68) at all. It beaches on the x≈85–95 strip.

### Clone
- **The passability grid uses a tile-SET classification, not the TMP byte.**
  - `engine/main.cpp:345` `resolve_land_types` calls `Theater::land_type_of_cell`, which calls `src/land_type.cpp:8-17` `land_type_from_tile_set`.
  - Only the [General] sets are known: WaterSet 21, ShorePieces 12, CliffSet 10, WaterCliffs 15, Rough 13 and Clear 0.
  - **YR's `WaterCliffAPieces = 78`** (urbanmd.ini L106) and every other set fall to **Clear**. So (120,68) and (119,68) are open land in `passable_`, and in the amphibious grid `land || water`.
  - `engine/world.cpp:1192-1211` `land_type_blocks_ground` / `set_land_terrain` then blocks only Water/Rock/Wall.
  - The header comment in `land_type.hpp` says that "the .tmp TerrainType byte was FALSIFIED for LandType". That claim is wrong: `FUN_00544910` reads exactly that byte through the table. The clone's own speed grid already does this correctly (`engine/w5_combat_arm.inc` `w5_land_of_terrain_type` + `hdr[41]`), so the two grids disagree.
  - Whole-map diff for ALL04, counting only the passable vs blocked flip and ignoring the cliff-back rule: **109 cells**.

    | Count | Set | Original | Clone |
    |---|---|---|---|
    | 44 | 25 "Slope Set Pieces" | Rock | Clear |
    | 26 | 19 "Bridges" | Rock | Clear |
    | 13 | 12 "Shore Pieces" | Water | Beach |
    | 12 | 85 "CliffC" | Rock | Clear |
    | 8 | 78 "CliffA/Water" | Rock | Clear |
    | 6 | 35 "Farm Crops" | Rock | Clear |

    The bridge cells must keep the overlay override. `CliffBackImpassability` is not modelled at all.
- **The height gate itself is right.** `engine/pathfind.cpp:470-493` `path_step_layer` and `engine/world_loco.cpp:296-306` refuse 0→4. In run `s1`, `click cell 119 68` with the LCRF selected gave no path and no move: it stayed at (120,68) for 300 frames, with no `[r210] cliff step`.
- **The "infantry can climb" observation is an eject artefact.**
  - `engine/world.cpp:2085-2125`: the unload pass uses `eject_cell(...)` with `free_cell = walkable && occ free` (`world.cpp:1922-1926`), and there is **no height gate**.
  - Run `s1`: the LCRF sits at (120,68) at height 0 (`uland`=3, Rock, in the speed grid). Its AMCV is ejected straight onto **(119,68) at height 4** (`uheight(0,AMCV,0)=4`). No step ever climbs; the passenger is teleported up the cliff.
  - The eject documented for the original (`FUN_00415C60`, transport-cargo.md §4) validates through the passenger's `vtbl+0x1AC` (IsCellOccupied, which reaches the height gate). [SUSPECTED for the UnitClass/LCRF unload path: `FUN_00415C60` lies in the AircraftClass range and passes dir = -1, so whether the gate is applied there was not re-traced.]

### Root cause
1. The passability LandType is classified per tile-set instead of per tile from the TMP terrain-type byte through `DAT_008278F4`, and there is no `CliffBackImpassability`.
   - As a result, the YR cliff set 78, the slope pieces and others are walkable.
   - The LCRF beaches on a cliff face, and passengers are ejected onto the plateau 4 levels up.
2. The m-all04 / m-all04-real opening (`click cell 120 68`, `mk_all04.py:37`) depends on (1).

### Suggested fix
1. In `engine/main.cpp:345` `resolve_land_types`, derive the LandType from the TMP header: `w5_land_of_terrain_type(hdr[41])` (reuse the `w5_combat_arm.inc` table).
   - Let a bridge or overlay land type override it, as `FUN_0047D1C0` does through `+0x44`.
   - Then apply `CliffBackImpassability == 2`: a Clear/Water/Beach/Ice cell becomes Rock when one of the neighbours (x,y-1), (x-1,y), (x+2,y+2), (x+1,y+1), (x+1,y-1) or (x-1,y+1) has a level ≥ own + 4 (the neighbour order from the decompile).
   - Retire `land_type_from_tile_set` for passability. Its own comment says Beach blocking was skipped "because ShorePieces mixes land and water sub-tiles", and the per-tile byte resolves exactly that.
   - Expect ripple effects: infantry lose `Beach` (Foot=0%), and several mission scripts may route differently. Re-run the m-*-real set.
2. In the unload eject (`world.cpp:1922` `free_cell` and `eject_cell`), add the passenger's height gate: `path_step_layer(terrain, w, t.cell_x, t.cell_y, t.on_deck, x, y) >= 0`.
3. Scripts: `scripts/play/gen/mk_all04.py` should beach the MCV craft on the real beach. For example, steer it to about (88,127) next to `wp56` (90,127). Beach cells there are x=85–86. Then drive the MCV inland over the x≈76–84 strip, which is Level 0 with Lake Shore Drive at x 76–78.

### Test
- **Unit test:** an ALL04 terrain fixture (or a hand-built TMP header grid) asserting:
  - `land_type(120,68) == Rock` and `land_type(119,68) == Rock`;
  - `!walkable(120,68)`;
  - `!unit_can_enter_cell(LCRF,120,68)`;
  - `(85,127)` is Beach: passable for the LCRF and not for Foot.
- **--play:** `s1`-style. `click unit 0:LCRF` + `click cell 120 68` must leave `ucell(0,LCRF,0) != 120068`. A beach at (86,127) must allow `key d` to unload with `uheight(0,AMCV,0) == 0`.
- **Regression:** `assert cliff_steps == 0`, plus a new eject-height probe: no passenger may appear more than 1 level from its transport.

---

## Suspect 2: destroyers wander off after a move

### Original
1. **Mission after the move.** `Enter_Idle_Mode` `0x737990` puts a human DEST into **Guard**: IQ 0, no DefaultToGuardArea and no GUARD_AREA ability. The clone matches (`world_w7.cpp:101-115`, `world_w7.cpp:52-58`).
2. **Threat-scan radius.** `FUN_006F8320` uses `FUN_007072E0(0)`, which returns GuardRange (0 for DEST). It then falls back (`0x6F8520..`) to:

   `(max(GetWeaponRange(1), GetWeaponRange(0)) >> 8) + 1 + (AirRangeBonus >> 8)`

   `GetWeaponRange` is UnitClass vtbl+0x168 = `FUN_00700760`. It returns the **raw `Weapon+0xB4`**, so ASWLauncher is **-0x200**. For DEST that is max(-512, 155mm 2048) → **9 cells**.
3. **In-range test.** `FUN_006F6750` begins with `if (weapon->Range == -0x200) return true;` (decompile `d2`). The ASW secondary is always in range. A DEST that has acquired a submerged sub never approaches it; it just launches the Osprey.
4. **Guard approach.** A plain-Guard unit approaches an out-of-range target with no leash (W7-07-07, 0x4D55B0). Because the scan is 9 cells and the 155mm reach is 8, a DEST only ever moves about one cell to engage.

### Clone
- **The 30-cell stand-in.** `engine/w2air_main.inc:46-48` (and `:89`) turns `Range=-2` into **30.0 cells** for the combat secondary and for the spawner.
- **The scan.** `engine/combat.cpp:97-103` `acquire_radius` = `floor(max(slot_range(0), slot_range(1))) + 1` = **31 cells**. Area guard would use `min(2×30, 16)` = 16.
- **The range check.** `engine/combat.cpp:788-809` `weapon_in_range_from` has no `-0x200 → true` case. It relies on the 30-cell substitute.
- **The chase.** A primary target found in that 31-cell scan (a land unit on the shore, a ship) is beyond the 155mm reach of 8. The W7 Guard chase (`combat.cpp:1834-1838`) then approaches it with no leash. The per-tick drop (`combat.cpp:1497-1502`) clears the target, the 31-cell rescan picks it up again, and the chase continues. That is the "wander".

### Reproduction
- **Run `s7`.**
  - DEST:0 is sent to (126,47) as a sonar spotter.
  - DEST:1 is sent to (139,60). From (139,60) it acquires the sub at (123,45), **21.9 cells away** (`utgt(0,DEST,1)=10` at T19), and launches its Osprey from there.
  - The original's 9-cell scan would never acquire that sub.
- **Run `s9`.** All six DESTs were ordered to (100,92).
  - They arrive, then pick targets 15–25 cells away: HTK at (85,106), V3 at (81,109)/(81,122), E2s and SUB 9.
  - They drive off to (86,104), (87,104), (86,108) and (86,118–119) on the beach line, and all six are sunk by f≈3300.
  - The arrival point (100,92) has nothing within 9 cells.
- **Runs `s3`, `s5` and `s6`.** With subs within 8 cells, the DESTs hold position and fire, which is faithful. That shows the wander needs targets beyond the 9-cell original radius.

### Root cause
The 30-cell stand-in for ASWLauncher `Range=-2`. It feeds the threat-scan radius (31 instead of 9) and the in-range test, instead of treating -0x200 as the special "always in range" value that only `FUN_006F6750` honours.

### Suggested fix
1. Keep the raw range: drop `w2air_main.inc:48`. Keep -2 in `WeaponSlot.range`, and keep `spawner_range` as a flag.
2. In `weapon_in_range_from` (`combat.cpp:798`), `if (u.w_range == -2.0) return true;`. Do the same in any building/spawner range test, such as the `world_an.cpp:443` `sp_range` use: treat it as infinite.
3. Make `acquire_radius` and `area_guard_leash_cells` (`combat.cpp:97-103`, `:139-145`) take `max(slot_range(0), slot_range(1))` on the raw values, so that -2 loses to 8. The DEST scan then becomes 9, as in `FUN_006F8320`.
4. Check `w7_greatest_threat` (`combat.cpp:353`) `rng` for `in_range_only` with -2: use the -2 → infinite rule there.

### Test
- **Unit test:** DEST with primary 8 and secondary -2 gives `acquire_radius == 9`, and `weapon_in_range` with ASW active is true at 25 cells.
- **--play:** `s9` re-run.
  - After the arrival (`until 900 moving(0)==0`), assert every `ucell(0,DEST,k)` stays within 2 cells of (100,92) for 900 frames.
  - `s7`: `utgt(0,DEST,1) == -1` while the sub is 21.9 cells away.

---

## Suspect 3: naval blocked path, ScatterContent and the head-on push (w7r2)

### Original
- `UnitClass::Scatter` `FUN_00742880` (decompile `d5`) has two branches, and both use the unit's **own** movement rules:
  - **Empty-coord (nokidding / ScatterContent) branch:** `MapClass::NearbyLocation FUN_0056D750(cell, SpeedType = type+0x67C, zone -1, MovementZone = this+0x8C, ...)`.
  - **Directional branch:** tries the 8 `DAT_0089E5F8` neighbours. It accepts one only if the unit's own `vtbl+0x1AC` (IsCellOccupied, which includes the height gate `FUN_004D9B70`) returns 0.
- So a ship is scattered to a free **water** cell, and a land unit is never scattered across a cliff.

### Clone
- **The scatter-cell test.** `engine/unit_orders.cpp:105-138` `order_scatter` picks its cell with `if (!walkable(x, y) || taken || ...)` at `:130`.
  - `walkable` is the **LAND** grid (`world.cpp:746-749`), whatever the unit's `movement_domain`.
  - So a Water-domain ship (DEST, AEGIS, SUB, DRED) **can never be scattered**: every open-water neighbour fails `walkable`.
  - A ship by the shore can be handed a land cell. `order_unit_move` then cannot reach it for a ship. [Not observed on ALL04.]
  - An amphibious unit (LCRF/SAPC) can only scatter onto land.
- **The ScatterContent push.** The w7r2 push (`world_loco.cpp:483-508` → `incoming_scatters(.., nokidding=true)` → `order_scatter(.., cell_push)`) therefore never moves a ship.
  - A blocked ship only re-paths (`repath(e,u,self,1)`), which works in open water.
  - Two ships meeting head-on in a one-cell lane would wait on each other indefinitely: the original deadlock that w7r2 fixed for land vehicles.
- **No height gate.** `order_scatter` has no height gate for land units, but it routes through `order_unit_move`, so the A* height gate still applies (low impact).

### Reproduction
- ALL04 has **no** one-cell water lanes and no water cell with ≤ 2 water neighbours that is not a dead end (search over `fullgrid.txt`: 0 lanes). So a naval deadlock cannot be reproduced on this map.
- In `s5` (six DESTs to one cell) and `s9` (six DESTs into the bay), every ship arrived or re-pathed. There was no deadlock and no wrong shove.
- **Verdict:** a code-level CONFIRMED BUG, low impact on ALL04.

### Suggested fix
1. In `order_scatter` (`unit_orders.cpp:130`), replace `walkable(x, y)` with `unit_can_enter_cell(u, x, y)` (`world.cpp:897-907`, domain-aware).
2. Add the height gate: `path_step_layer(*terrain_for(u,..), grid_w_, u.cell_x, u.cell_y, u.on_deck, x, y) >= 0`.
3. Keep the `taken` / `building_at_cell` tests.

### Test
- **Unit test:** a World with a one-cell water lane and two DESTs ordered through it head-on. Both reach their goals within N frames, and the parked one is scattered to a water cell (`!walkable(dest) && water_passable(dest)`).
- **Unit test:** a ship next to a land cell is never given a land scatter cell.

---

## Notes for the controller
- Fixing suspect 1 makes the current `m-all04*` openings (`click cell 120 68`) invalid. The generator must land on the x≈85–95 beach, as the map's own reinforcement scripts do (waypoints 56–59).
- Suspect 2's fix lowers DEST/Osprey reach against subs found by other sonar units (31 cells → 9). That is faithful, but naval scripts may need re-tuning.
