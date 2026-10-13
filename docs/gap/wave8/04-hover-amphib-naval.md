# W8 interaction audit 04: hover, amphibious and naval units × water, beach, bridges and channels

**Auditor:** w8int-04 (read-only).
**Tree:** D:\ra2-android master `34cb652`, `build\ra2engine.exe`.
**Original:** gamemd.exe (YR 1.000), Ghidra on a private copy of the `bridge` project (`D:\ra2-re\w8int-04`, deleted after the audit).

**Evidence locations:**
- Decompiles and disassembly: `D:\ra2-re\w8int-04-out\` (`d1` DamageArea 0x489190 + HoverLoco Process 0x514130/0x515CF0, `d2` 0x5F3970, `d3` HoverLoco power functions, `d4` KamikazeTracker 0x54E040/0x54E160, `x1` overlay loader + ShipLoco wake, `x2` hover drop call, `v1`/`v2` vtables, `s1` strings).
- Play scripts and results: `D:\ra2-re\w8int-04-out\evidence\` (`p_lowbr.play`, `p_all01_deck.play`, `p_bop.play`, `*-result.txt`). Runs are in `D:\ra2-re\w8int-04-out\play\o_*`.
- Terrain dumps (`cheat map_terrain`): `play\all01_terrain.txt` and `play\xmp31s2_terrain.txt`.

**Family:** LCRF/SAPC (Hover, MovementZone=Amphibious), ROBO (Hover, AmphibiousDestroyer), DEST, AEGIS, CARRIER, DLPH, SUB, HYD, SQD, DRED. BSUB is N-A, because the clone has no Yuri side (U-5).

## 1. Summary

The naval core holds up when it meets the shore:
- Landing craft beach, load, refuse to unload at sea and unload on land. `naval.play` passes on 34cb652.
- A Typhoon may attack a Naval Yard; the Carrier launches its whole wing; yard repair is paid. `w7-airnav.play` passes.
- The fv7c ship scatter works in a two-cell river lane.
- The ASW Range=-2 rule, the AI water muster, Chronosphere and Iron Curtain all follow the RE.

The defects are where the water family meets **bridges**. Two of them are new and confirmed in play:

1. **W8-04-01 (MAJOR).** An intact low bridge keeps the LandType of the tile under it in the grid the clone uses for naval targeting, ground speed and the infantry wet test. The original makes the cell Road (`LOBRDG* Land=Road, NoUseTileLandType=true` → `FUN_0047D1C0`).
   - On May Day (xmp31s2), a Typhoon shows the attack cursor on a Grizzly standing on a repaired span and kills it. The original refuses: LandTargeting=1 applies off the water.
   - The Grizzly crosses the span at **half speed** (0.498 against 0.996 cells/s).
   - Tanya walking over the span plays **Swim** on the Beach-tile sections. This is the user's bridge bug, low-bridge variant.
2. **W8-04-02 (MINOR).** Explosions ignore the high-bridge layer. On ALL01, every 155mm shell a Destroyer fired at a landing craft *under* the bridge also hit our own MCV parked *on the deck* above: 1000 → 331 HP over five kills. The original's DamageArea reads one object list per explosion: the deck list or the ground list, chosen by the impact height (0x489190).

The other new gaps:
- **W8-04-03 (MINOR).** The high-bridge damage roll has no height window, and the projectile path runs it only on maps that also have a low bridge.
- **W8-04-04 (COSMETIC).** Hovercraft and Robot Tanks leave no wake on water [CONFIRMED-PLAY + RE].
- **W8-04-05 (MINOR).** A Robot Tank switched off over water floats on. The original spins it, then drops it into the water [play + RE for the trigger; the death outcome is PROBABLE].

Known items that were not re-reported: W7-06-14 (TooBigToFitUnderBridge), W7-12-09/11/14/35 (wet set on beach, sinking draw, water death anims), W7-06-10/11/13/15 remnants, and W7-11-02 (hover physics, fixed).

## 2. Matrix

Column legend:
- **Amph** = LCRF/SAPC.
- **ROBO** = Robot Tank.
- **Surf** = DEST/AEGIS/CARRIER/DRED/HYD.
- **Sub/Org** = SUB/DLPH/SQD.
- **BSUB** = Boomer.

Cell legend:
- **OK** = matches the original, measured in play or checked against code and RE.
- **GAP** = see the W8 or W7 item named.
- **N-A** = the unit can never be in that state.

| Terrain / layer × state | Amph | ROBO | Surf | Sub/Org | BSUB |
|---|---|---|---|---|---|
| Open water: reach, move, speed | OK (Hover column gates passability only, W7-11-02 fixed) | OK | OK (water grid only) | OK | N-A |
| Water: wake while moving | **GAP W8-04-04** | **GAP W8-04-04** | OK (17 wakes in 60 frames) | OK (none, W7-06-04 fixed) | N-A |
| Beach: reach / hold | OK (amph grid includes Beach, fv7c) | OK | OK (Float 0%) | OK | N-A |
| Shore transition, land ↔ water | OK (naval.play) | OK | N-A | N-A | N-A |
| Rock-in-water sub-tiles (TMP byte 15) | OK (Rock in both grids; e.g. May Day (72,122), (60..61,101..102)) | OK | OK | OK | N-A |
| Cliff water edge (CliffBack Rock) | OK (passability grid; the speed grid lacks CliffBack, but no unit can stand there) | OK | OK | OK | N-A |
| Under an intact high bridge: sail through | OK (ALL01 (82,52..54) water=1) | OK | OK | OK | N-A |
| Under a high bridge: hit by a shell (layer) | **GAP W8-04-02** | **GAP W8-04-02** | **GAP W8-04-02** | **GAP W8-04-02** | N-A |
| Under a high bridge: hit with a Wall=yes warhead (bridge roll) | **GAP W8-04-03** | ← | ← | ← | N-A |
| Under a high bridge: draw order | W7-06-14 (known) | — | W7-06-14 | W7-06-14 | N-A |
| Low bridge: ships blocked | N-A | N-A | OK (bridge cell water=0) | OK | N-A |
| Low bridge: hover/amph unit crossing on top | OK (Road passable) | OK | N-A | N-A | N-A |
| Low bridge: land unit on top is a naval target (attacked by Sub/Org) | — | — | — | **GAP W8-04-01** | N-A |
| Naval Yard docking and repair | OK (w7-airnav: paid) | N-A | OK | OK (RepairStep) | N-A |
| Narrow channel: head-on / scatter | OK? (code) | OK? | OK (May Day 2-cell lane: the 2v2 jam resolves, fv7c scatter_cell_ok) | OK? (code) | N-A |
| Load/unload on land / beach / water (fv7c height gate) | OK (naval.play; eject applies the passenger height gate) | N-A | N-A | N-A | N-A |
| Transport killed on water / beach: passengers | OK (drown unless they can stand there; 0x736FD1) | N-A | N-A | N-A | N-A |
| Ship sinking near shore: rule / passengers | OK (Weight ≥ 3; LCRF Weight 1 explodes) / OK | N-A | OK (draw is W7-12-11) | OK (subs explode) | N-A |
| Anti-sub Range=-2 (infinite, 9-cell scan) | N-A | N-A | OK (combat.cpp:97-103, :798; world_an.cpp:577) | N-A | N-A |
| Naval targeting by cell: target on water | OK (in_water / naval_cell) | OK | OK | OK (sub vs yard PASS) | N-A |
| … target on Beach (amphibious infantry, beached hover) | OK (Beach counts as naval) | OK | OK | OK | N-A |
| … target on a high-bridge deck | OK (on_deck → not naval) | OK | OK | OK | N-A |
| … target on a low bridge | — | — | — | **GAP W8-04-01** | N-A |
| … swimmer (Tanya / SEAL in water) | — | — | OK (AEGIS refuses: NavalTargeting 6) | OK (SQD punch, DLPH zap) | N-A |
| Squid / Dolphin vs hover on water | OK (grab: Parasiteable, not Organic) | OK | OK | — | N-A |
| Carrier Hornets near cliffs | N-A | N-A | OK (aircraft; wing PASS) | N-A | N-A |
| Dreadnought missile vs a target on a deck | — | — | **GAP W8-04-02/03** (2-D blast at the ground, no bridge roll) | — | N-A |
| Chronosphere onto land / water | OK (amph survives on either) | OK | OK (killed on land; code + W7-08-08 RE) | OK (Organic dies at source) | N-A |
| Iron Curtain | OK | OK | OK | OK (Organic dies, 0x4DE9F0) | N-A |
| Robot Control Center lost over water | N-A | **GAP W8-04-05** | N-A | N-A | N-A |
| AI naval muster near Rock-in-water | N-A | N-A | OK (flood fill on the water grid) | OK | N-A |

## 3. Gaps

### W8-04-01: A low bridge keeps the tile's LandType: naval units can attack land units on it, ground units cross at half speed, and Tanya swims on it
- **Severity:** MAJOR. It changes combat legality and speed on every map with river bridges (May Day, TRN02 after repair, …), and it is the low-bridge variant of the user's Tanya-on-a-bridge report.
- **Evidence** [CONFIRMED-PLAY `evidence\p_lowbr.play` → `o_lowbr-result.txt`, xmp31s2, after the hut repair of span x 71..74, y 117..119]:
  - An enemy MTNK on (73,118) gives `uland(1,MTNK,0)=2` (Water). A selected own SUB over it shows `cursor_name="attack"`, and after 300 frames the tank is at `uhpn=0` (dead).
  - Control: an MTNK on the intact section (55,93) over a **Rough** tile is never targeted (`utgt=-1`, HP unchanged).
  - An own Grizzly crossing measures `uspeed` 0.996 on (69,118) Rough, **0.498** on (73,118) Water tile, and 0.996 on (77,118).
  - Tanya crossing: `iseq(Swim)=1` with `uland=6` on (71,y) and (75,y). Those are intact low-bridge cells over Beach tiles.
- **Original:**
  - [CONFIRMED-DATA] `rulesmd [LOBRDG01]/[LOBRDB01] Land=Road NoUseTileLandType=true`. Destroyed pieces (LOBRDG27/28) have `NoUseTileLandType=false`.
  - [CONFIRMED-RE] `CellClass::Recalc FUN_0047D1C0`: with an overlay, `LandType(+0xEC) = OverlayType+0x298 (Land)`. When that is Wall/Railroad or **`+0x2AC` is set**, it returns at once. Otherwise the tile type is used.
  - [CONFIRMED-RE] `+0x2AC` is `NoUseTileLandType`: OverlayType loader `0x5FE148` reads it, and `0x5FE163` stores it.
  - So an intact low-bridge cell is **Road**: `GetFireError 0x6FBC86` treats it as off the water (LandTargeting=1 → ILLEGAL), Drive speed uses the Road row (Track 100%), and PlayAnim's wet test (Water/Beach, W7-12-09) is false.
- **Clone:**
  - `world_depth.inc:33` sets Road only in the passability `lt` vector.
  - `world_depth.cpp:438` `set_bridge_cell` updates `passable_` / `water_passable_` only.
  - The per-cell LandType that the rest of the engine reads, `World::speed_land_at` (`world.cpp:691`, `speed_land_` armed in `w5_combat_arm.inc:61`), is the raw TMP byte. Its readers:
    - `an_victim_naval_cell` (`world_an.cpp:78-87`), which feeds `naval_cell` (`world_an.cpp:146`);
    - `land_speed_mult` (`world_loco.cpp:144-171`). Water/Beach give Track=0%, which the `m == 0.0 → 0.5` fall-back turns into half speed;
    - the wet tests at `world.cpp:6031` and `pres_audio_events.inc:621-624`.
- **Root cause:** there are two LandType grids. The overlay override reaches only the passability grid.
- **Fix:**
  - Make `speed_land_at` return Road (1) for an intact low-bridge cell: `bridge_overlay_at(x,y) >= 0 && !low_bridge_destroyed(...)`. Alternatively, write `speed_land_[i]` in `depth_set_terrain` and in `set_bridge_cell` (tile land on destruction, Road on repair).
  - With the grid right, the `m==0 → 0.5` fall-back should become unreachable. Keep it as a guard only.
- **Test:**
  - Unit test: on a fixture with an intact LOBRDG01 over a Water tile, `speed_land_at == 1`. After `set_bridge_cell(destroyed)` it is 2, and after repair it is 1 again.
  - Play: `p_lowbr.play` sections 1, 2 and 1b must give `uland == 1`, `uspeed` 0.996 on (73,118), cursor `nomove` and the tank alive, and `iseq(Swim) == 0` on x 71/75.

### W8-04-02: Explosions ignore the high-bridge layer: a shell at a ship under the deck also damages units on the deck (and the reverse)
- **Severity:** MINOR (bridge cells only), but costly when it happens: a friendly MCV lost 67% HP.
- **Evidence** [CONFIRMED-PLAY `evidence\p_all01_deck.play` → `o_deck-result.txt`, ALL01T after the east bridge repair]:
  - The own AMCV is on the deck at (82,53): `udeck=1`, `uheight=6`.
  - An enemy LCRF sits under it in the same cell: `udeck(1,LCRF,0)=0`.
  - An own DEST at (86,52) is ordered to attack the LCRF. The AMCV goes **1000 → 844** after the first LCRF dies and **→ 331** after five. The LCRF is unarmed; the only damage source is the DEST's ARTYHE (CellSpread 1).
- **Original** [CONFIRMED-RE `MapClass::DamageArea FUN_00489190`, `d1.txt`]:
  - A bridge flag is set when the impact cell has the bridge bit (`Flags & 0x100`) **and** `impact Z > floor + BridgeHeight/2` (decompile lines 139-143).
  - The ring loop then reads **either** `cell+0xE4` (ground objects) **or** `cell+0xE8` (bridge-layer objects) for every cell (lines 168-172), never both.
  - A shell bursting at a ship's waterline never touches the deck list, and the reverse also holds.
- **Clone:**
  - `combat.cpp:911-950` `damage_area` scans every unit in the ring. `unit_lept_z` (`combat.cpp:734`) is `level*104 + altitude` and ignores `on_deck`, so deck and under-deck units sit at the same height.
  - `World::an_area_damage` (`world_sw.cpp:298-320`, rockets / superweapons) is 2-D for ground units.
  - Rockets aim at `z=0` (`rocket_target_point`, `world_an.cpp:238-245`), so a Dreadnought missile at a deck target also hits the ship below.
- **Fix:**
  - Give a unit on a deck `z = (level + 4) * 104` in `unit_lept_z`, using the clone's deck height as in R-210.
  - In `damage_area` and `an_area_damage`, decide the layer once from the impact cell (high-bridge cell and `iz > ground_z + bridge_height/2`). Then skip units whose `on_deck` does not match that layer.
  - Make `rocket_target_point` return the deck height for an `on_deck` target.
  - [SUSPECTED, not traced] Whether RocketLocomotion detonates at the deck for a deck target: `KamikazeTracker 0x54E040` stores `MapClass::Cell(target coords)` as the destination (`d4.txt`), and which height the cell gives back was not traced.
- **Test:**
  - Unit test: a deck unit and an under-deck unit in one cell. A shell burst at water level damages only the under-deck one; a burst at deck height damages only the deck one.
  - Play: `p_all01_deck.play` must keep `uhpn(0,AMCV,0) == 1000`.

### W8-04-03: The high-bridge damage roll has no height window and runs only on maps that also have a low bridge
- **Severity:** MINOR.
- **Evidence:**
  - [CONFIRMED-RE `FUN_00489190` @0x489E87 / 0x48A0xx]: for a high-bridge cell (`Flags & 0x100`), the bridge damage runs only when `(level-2)*LevelHeight + BridgeHeight < impact Z ≤ (level+1)*LevelHeight + BridgeHeight`. An explosion at the waterline (a ship under the span) never rolls the span; one at deck height does.
  - [CONFIRMED-code] `World::bridge_explosion` (`world_depth.cpp:463-474`) takes no height. Its projectile caller is gated on `!bridge_ovl_.empty()` (`world.cpp:6594`), and `bridge_ovl_` is only set when the map has a **low** bridge (`world_depth.inc:36`). Two consequences:
    - (a) On a map with both kinds, a 155mm shell at a ship under a high span rolls the span (code path).
    - (b) On ALL01T, which has high bridges only, no shell ever rolls a span, even at deck height, while a Ctrl-force-fire cell shot does (`step_depth` → `bridge_explosion`, `world_depth.cpp:726`).
  - Rockets (DMISLWH and V3WH are `Wall=yes`) never call `bridge_explosion` (`rocket_explode`, `world_an.cpp:265-294`).
  - Play cross-check: five LCRF kills under the ALL01 span left `bridge_destroyed` unchanged. That matches (b), not the original's reasoning.
- **Fix:**
  - Pass the impact Z to `bridge_explosion` and apply the 0x489E87 window for high-bridge cells.
  - Drop the `!bridge_ovl_.empty()` precondition. `bridge_explosion` already returns early when neither kind of bridge exists.
  - Call it from `rocket_explode` (end == 1) with the rocket's warhead `Wall` flag.
- **Test:**
  - Unit test: a high-bridge cell plus a Wall=yes burst at the water level → no roll. At deck height with damage > BridgeStrength (IonCannon-like) → the span breaks.
  - The same on a world with no low-bridge overlay.

### W8-04-04: Hover units (LCRF, SAPC, Robot Tank) leave no wake on water
- **Severity:** COSMETIC.
- **Evidence** [CONFIRMED-PLAY `evidence\p_bop.play` → `o_bop-result.txt`, Bay of Pigs]:
  - A DEST sailing 6 cells raises `fx_wakes` 0 → 17 in 60 frames.
  - An LCRF, then a ROBO, each sail about 6 cells on water (169,186 → 163,184 / 163,185), and `fx_wakes` stays at 17.
- **Original** [CONFIRMED-RE HoverLocomotionClass::Process `FUN_00514130` @0x514841, `d1.txt`]: while `Is_Moving_Now` (vt+0x80), every **10** frames (`Frame % 10 == 0`), when not OnBridge (+0x8C) and the cell LandType is **Water**, it spawns `Rules+0x94` (= `[General] Wake=WAKE1`) at the unit. The ship locomotor does the same every 8 frames (0x69F4C9).
- **Clone:** `pres_effects.inc:549` requires `u.is_naval`. `is_naval` is set only for the ShipLoco CLSID (`main.cpp:1105-1109`), so Hover-locomotor units never qualify.
- **Fix:** add a second branch for the Hover locomotor (`{4A582742…}`; LCRF/SAPC/ROBO/YHVR): `moving && !on_deck && cell LandType == Water && frame % 10 == 0`.
- **Test:** `p_bop.play` sections 2 and 3: `fx_wakes` must grow while the LCRF or ROBO sails.

### W8-04-05: A Robot Tank cut off from its Robot Control Center over water keeps floating
- **Severity:** MINOR.
- **Evidence:**
  - [CONFIRMED-PLAY `p_bop.play` section 3]: a ROBO on water at (163,185) (`sel_on_water=1`); GAROBO destroyed; 300 frames later `units(0,ROBO)=1`. `w6_step_powered_units` (`world.cpp:842-869`) only freezes it.
  - [CONFIRMED-RE trigger]:
    - HoverLocomotion vtable `0x7E9CFC`, `+0x5C` = `FUN_00516A10` (Power_Off): while still hovering, it sets `+0x64 = 1` and `+0x68 = ±Random(20,30)`.
    - Process (@0x514969..0x514A2C) counts `+0x68` down while spinning the unit's facing. When it ends, or when the unit is no longer powered or airborne, it clears `+0x64`. Then **if the cell LandType is Water and Z < floor + [0xA8E128]**, it calls `vtbl+0xEC` = `ObjectClass 0x5F3970` (sets +0x8D IsFallingDown, clears OnBridge): the unit is dropped.
  - [PROBABLE] The drop into water destroys it. This is the known in-game behaviour; the fall-landing code was not traced.
- **Fix:**
  - On the power-loss edge of a Hover-locomotor unit, run a 20–30 frame spin (sync RNG).
  - Then, if the cell is Water and the unit is not on a deck, kill it with the water-death path (W7-12-14 anims). The same spin happens on land, without the drop.
- **Test:** `p_bop.play` section 3 expects `units(0,ROBO) == 0` within 60 frames of losing GAROBO when on water, and a ROBO on land survives.

## 4. Verified OK (do not re-report)

- **LCRF/SAPC landing cycle** [CONFIRMED-PLAY `naval.play` PASS on 34cb652]: beach, board 2 GIs, no unload at sea (`cargo_total` stays), unload on land. Beach cells count as beached (`world.cpp:2091-2098`, fv7c). The eject applies the passenger's height gate (`world.cpp:2122-2132`).
- **Transport death:** passengers unlimbo on the transport's cell or drown if they cannot stand there. This includes Beach for Foot/Track units (`world.cpp:1934-1971`, RE 0x736FD1).
- **Naval targeting by cell:**
  - Water/Beach count as naval; a deck (`on_deck`) and the air do not (`world_an.cpp:146`).
  - Typhoon vs Naval Yard [`w7-airnav.play` PASS].
  - A land target on a Rough low-bridge cell is refused [p_lowbr control].
  - AEGIS refuses swimmers (NavalTargeting 6). Squid punch vs organic, grab vs a hover (Parasiteable).
- **Ship scatter / head-on (fv7c):** `scatter_cell_ok` is domain- and height-aware (`unit_orders.cpp:105-117`). In May Day's 2-cell river lane (x 72–73), four DESTs ordered through each other jam at y 111/112, ScatterContent pushes, two pass, and the other two get the scatter destination (`RA2_LOCO_TRACE`). There is no permanent deadlock [play].
- **ASW Range=-2:** kept raw; `range_infinite` in the in-range test, the threat scan (9 cells) and the spawner reset (`combat.cpp:97-103, 355, 377, 798`; `world_an.cpp:577`).
- **AI naval muster:** an all-ship team gathers at the nearest reachable water cell by flood fill over `water_passable_` (`world.cpp:4676-4697`). Rock-in-water sub-tiles are Rock in that grid (TMP byte 15; e.g. May Day (72,122), (60..61,101..102); ALL01 (57,58)).
- **High bridges:** ships pass under intact spans (ALL01 (82,52..54), (62,58..59): water=1). Sonar is a 2-D range, so a sub under a span is detected as anywhere else.
- **Wakes:** surface ships every 8 frames on water; underwater types none.
- **Chronosphere** (`world_sw.cpp:438-560`):
  - a ship sent onto land is killed;
  - an Organic Dolphin or Squid dies at the source;
  - a hover lands on either land or water;
  - a non-hover land vehicle onto water sinks.
- **Iron Curtain** (`world_sw.cpp:391-432`): ships are curtained, Organic naval units die, and a riding Squid is released.
- **Carrier wing** of 3 Hornets, 20 frames apart [`w7-airnav.play` PASS]. Cliffs do not matter to the aircraft.
- **Hover speed:** the Hover locomotor does not read the GroundType speed column (R-062 / W7-11-02), so land vs water hover speed is flat. Faithful.

## 5. Open questions

1. The detonation height of a RocketLocomotion missile aimed at a unit on a deck (KamikazeTracker stores a CellClass destination, 0x54E040). This decides whether the original's Dreadnought hits deck targets at all (W8-04-02).
2. The value of `[0xA8E128]` and the fall-landing path after `0x5F3970`, which fix the exact Robot Tank drop outcome (W8-04-05).
3. Whether an amphibious *passenger* (SEAL/Tanya) may be unloaded onto Beach or water cells when the transport is beached with no land neighbour. The clone's eject uses the land grid only (`world.cpp:1926-1930`); the original validates through the passenger's own `CanEnterCell`. No reproduction was found.
