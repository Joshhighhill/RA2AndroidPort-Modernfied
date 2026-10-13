# World depth: garrisons (G-48) and bridges (G-49)

Agent G, 2026-10-06. Source: gamemd.exe (YR 1.001) decompiled with a private
Ghidra copy (`D:\ra2-re\fixG`, scripts `D:\ra2-re\scripts\fixG_*.py`, output
`D:\ra2-re\fixGout`), YRpp headers for offsets and names, rulesmd.ini data.
Code: `engine/world_depth.cpp` (World members), `engine/bridges.{hpp,cpp}`
(pure, tested by `tests/test_bridges.cpp`), `engine/world_depth.inc`
(real-game arming), probe/scripts: `scripts/play/world-depth.play`.

Confidence tags: **CONFIRMED** (read in the decompile/disassembly),
**PROBABLE** (structure read, one detail inferred), **[RE-FALLBACK]**
(documented stand-in, not traced).

## §A Garrisonable buildings (Urban Combat)

### A1 Keys and offsets — CONFIRMED

| Key | Where | Offset | Loader |
|---|---|---|---|
| `CanBeOccupied=` | BuildingType | +0x157B | `FUN_0045FE40` |
| `CanOccupyFire=` | BuildingType | +0x157C | `FUN_0045FE40` |
| `MaxNumberOccupants=` | BuildingType | +0x1580 | `FUN_0045FE40` |
| `Occupier=` | InfantryType | +0xEB4 | `FUN_00523??` (string user, gap) |
| `OccupyWeapon=` / `EliteOccupyWeapon=` | InfantryType | +0xE04 / +0xE20 | (read via `FUN_004526E0`) |
| `[CombatDamage] OccupyDamageMultiplier` | Rules | +0xF40 (float) | `FUN_0066B5F0` |
| `[CombatDamage] OccupyROFMultiplier` | Rules | +0xF44 (float) | `FUN_0066B5F0` |
| `[CombatDamage] OccupyWeaponRange` | Rules | +0xF48 (int, cells) | `FUN_0066B5F0` |
| Occupants / count / FiringOccupantIndex | BuildingClass | +0x688 / +0x694 / +0x69C | YRpp `BuildingClass.h` |

rulesmd: 163 types carry `CanBeOccupied=yes` (civilian CA\* buildings and the
Soviet NABNKR); civilian ones also `CanOccupyFire=yes`, mostly
`MaxNumberOccupants=10`. GI/Conscript/… have `Occupier=yes` and an
`OccupyWeapon` (`UCPara`, `UCM1Carbine`, …).

### A2 Who may enter — `BuildingClass::CanBeOccupiedBy` `FUN_00457CE0` — CONFIRMED

True iff: the building type has CanBeOccupied; its mission is not 0x12
(Construction) / 0x13 (Selling); the infantry is an Occupier; the building is
owned by the infantry's house **or** by a house whose HouseType has the
neutral/passive flag (+0x1A6); the occupant count differs from
MaxNumberOccupants; and neither iron-curtain/drain predicate holds. (The
`Assaulter` +0xEB5 branch — enter an *enemy* garrison and kill its occupants via
`KillOccupants` — is YR-only and not modelled.) Clone: `World::can_occupy`.

The order is the normal enter mission: `InfantryClass Mission_Enter`
(`FUN_00519449`, mission 8 = Enter) re-checks CanBeOccupiedBy on arrival and
calls `InfantryClass::Occupy` (`FUN_00522640`), which limbos the infantry and
appends it to Occupants; the first occupant raises the "structure garrisoned"
EVA/sound for the owner. Clone: `World::order_occupy` (walk adjacent) +
`occupy_now` (Chebyshev-1 arrival, the step_capture analogue). A garrisoned
infantry is off the map like a passenger (`cargo_of = kGarrisonCargo`).

### A3 Ownership — `FUN_004581F0` (per tick from `FUN_0043FAA0`) — CONFIRMED

For CanBeOccupied types: no occupants and owner != the neutral house → owner
becomes the neutral house (EVA "structure abandoned" for the old owner);
occupants and owner == neutral → owner becomes the FIRST occupant's house.
Clone: `World::sync_garrison_owner` (neutral = the first non-combatant house).

### A4 Firing out — CONFIRMED

* Weapon — `BuildingClass::GetWeapon` `FUN_004526E0`: when garrison-firing
  (vtable +0x400 = `FUN_00458DC0`: CanBeOccupied && CanOccupyFire && count > 0)
  and FiringOccupantIndex < count, the weapon is that occupant's
  EliteOccupyWeapon (if the occupant is elite and it is set) else
  OccupyWeapon, else the occupant's weapon 0.
* ROF — `TechnoClass::GetROF` `FUN_006FC460`: `rof = rof / OccupantCount`
  (integer division), then if `OccupyROFMultiplier > 1.0`:
  `rof = ftol(rof / OccupyROFMultiplier)` (FDIV @0x6FC660).
* Damage — `TechnoClass::Fire` `FUN_006FD210`: `damage = ftol(damage *
  OccupyDamageMultiplier)` (FMUL @0x6FD8BA).
* Range — `TechnoClass::GetWeaponRange` `FUN_006F6750`: the weapon's Range is
  REPLACED by `(vtable +0x404 + OccupyWeaponRange) << 8` where +0x404 =
  `FUN_00458DF0` = `min(foundation width, height) / 2`.
* Rotation — after every shot `FiringOccupantIndex = (index + 1) % count`
  (@0x6FE525).

Clone: `World::step_garrison_combat` (acquire nearest visible enemy in range).
[RE-FALLBACK] UC weapons fly invisible projectiles; the clone delivers the hit
instantly. `DistributedFire=` (retarget after every shot) is commented out on
the stock civilian types and not modelled.

### A5 Leaving — `BuildingClass::UnloadOccupants` `FUN_00457DD0` — CONFIRMED (structure)

Resets FiringOccupantIndex and finds ONE exit cell on the ring around the
foundation (origin (x0,y0), size w x h), testing each with the first
occupant's IsCellOccupied (vtbl+0x1AC: land type, the FootClass height gate
`FUN_004D9B70`, room) in this order (wave 8, disasm 0x457DD0..0x458050):
right column (x0+w, y0+h .. y0-1), bottom row (x0+w-1 .. x0-1, y0+h), top row
(x0 .. x0+w, y0-1), left column (x0-1, y0 .. y0+h). Every occupant (last
first) is force-unlimboed (DAT_00A8D714++) at that cell (a failed Unlimbo
deletes the unit), the scatter flag (arg 1) queues mission 0xF, then the
vector is cleared. No enterable cell: KillOccupants `FUN_004585B0(0)` UnInits
every occupant (no death) -- CONFIRMED -- unless arg 2 is set, in which case
they unlimbo on the foundation's last cell (x0+w-1, y0+h-1). Callers and
args: Mission_Unload `FUN_0044D880` (0,0), sell `FUN_00449B70` @0x44A4FC
(0,1), destruction `FUN_004421B0` @0x4425BB (0,0), the sync `FUN_004581F0`
(0,0), `FUN_006DCE20` (0,0), `FUN_00501329` (1,0).
Clone: `World::unload_occupants(bi, place_inside)` -- the ring walk, height
gate, kill / place-inside rules; the infantry fill the exit cell's spots and
the rest spill to the nearest enterable cells around it [RE-FALLBACK: the
forced stacking + scatter]. (Before wave 8: an unbounded nearest-free-cell
search with no height gate -- W8-06-07 / W8-02-04.)

## §B Bridges

### B1 Overlay runtime indices — CONFIRMED

`[OverlayPack]` stores, and the binary compares, the **order of appearance** in
rulesmd `[OverlayTypes]`, not the ini's own numbers (which start at 1 and skip
40, 41 and 183). Anchors: `BRIDGE1/2` = 0x18/0x19 (`FUN_00586BB0`,
`FUN_00489190`), `LOBRDG01` = 0x4A (`0x49 < idx` tests), `LOBRDB01` = 0xCD,
`BRIDGEB1/2` = 0xED/0xEE. The map dressing (`engine/pres_map.inc`) keyed its
art by the ini number and drew every overlay after #0 with the wrong art; fixed.

### B2 Low bridges (wood LOBRDG, concrete LOBRDB) — CONFIRMED / PROBABLE

* Passability: intact/damaged pieces are `Land=Road` with
  `NoUseTileLandType=true` (rulesmd) — ground units cross the river, ships do
  not; the destroyed pieces `LOBRDG27/28` (0x64/0x65) have
  `NoUseTileLandType=false` — the tile's land type (water) applies again.
  This is the cell's LandType itself (`CellClass::RecalcAttributes`
  `FUN_0047D1C0` L39-43: with an overlay, LandType = OverlayType.Land +0x298,
  returning at once when NoUseTileLandType +0x2AC is set; loader `0x5FE148`),
  so an intact low-bridge cell is Road for EVERY reader: Drive speed, the
  naval-target test (GetFireError `0x6FBC86`), PlayAnim's wet set and the
  enter/leave water sounds [CONFIRMED, W8-01-04]. Clone: `depth_set_terrain` +
  `World::set_bridge_cell` (grids, Beach blocks Foot/Track/Wheel as at load,
  W8-01-06) + `World::speed_land_at` / `MoveSpeedCtx::bridge_ovl` (LandType).
* Damage — `MapClass::DamageArea` `FUN_00489190`: an explosion in a low-bridge
  cell whose warhead has `Wall=yes` (WarheadType +0x144) breaks the section when
  the warhead is `IonCannonWarhead` (Rules +0xFF0) or
  `Random(1, BridgeStrength) < damage` (`BridgeStrength` = Rules +0x1740,
  1500). Wood range `0x49 < idx < 100`, concrete `0xCC < idx < 0xE7`
  (`FUN_00586BB0` dispatch). Gated by `DestroyableBridges` /
  `BridgeDestruction`. The roll lives in DamageArea itself, so EVERY
  DamageArea caller rolls it: shells (`FUN_004690A0`), V3 / Dreadnought
  rockets (`FUN_00662A70` @0x662D07), death and crash weapons
  (`FUN_0070CAC0` -> bullet -> @0x4699A0), Ivan bombs, the Lightning Storm
  bolt (`FUN_0053A030`, Z = GetCoordsWithBridge, IonWH Wall=yes, 250 vs 1500:
  ~16.6% per bolt) [CONFIRMED, W8-05-04 / W8-06-04]. On a cell with the bridge
  flag (+0x140 & 0x100, a high deck) the roll also needs the explosion Z in
  `((Level-2)*104 + BridgeHeight, (Level+1)*104 + BridgeHeight]`
  (@0x489E87 concrete / @0x48A012 wood) [CONFIRMED, W8-04-03]. Clone:
  `World::bridge_explosion(.., z_abs)` / `bridge_area_roll`, fed by projectile
  detonations (any map with a bridge, low or high), ground (force-fire) shots,
  rockets, `ability_area_damage` and the storm bolt. [RE-FALLBACK] instant-hit
  weapons do not roll; the RNG is a dedicated xorshift, not the scenario RNG.
* Section — `FUN_0057B4D0` finds the centre of the 3-cell cross-section
  (looks one / two cells back along the cross axis); `FUN_0057B720` (cross
  axis y) and `FUN_0057BCE0` (cross axis x) rewrite the three cells:
  intact (< 0x50 / < 0x59) → damaged (0x50 / 0x59), damaged (0x50–0x52 /
  0x59–0x5B) → destroyed (100 / 0x65), end pieces 0x5C→0x5D, 0x5E→0x5F,
  0x60→0x61, 0x62→99. Concrete mirrors the wood set at +131 [PROBABLE: the
  concrete rewrite functions `FUN_0057C990`/`FUN_0057CF60` were not dumped;
  `FUN_0057C720`'s dispatch thresholds are exactly +131].
* The section rewrite then calls `FUN_00487920` on the three cells (after
  `FUN_0047D1C0`) [CONFIRMED, W8-01-03 / W8-03-07]: every object in the cell's
  list whose CanEnterCell (vt+0x1AC) answers Move::No (7) takes
  `ReceiveDamage(Health, C4Warhead, ignoreDefenses=1, preventPassengerEscape=1)`;
  a landed AircraftType dies too, and in the destroy pass a JumpJet type
  (+0xD94); then a 5x5 scan kills movers heading into the cell (locomotor
  vt+0xA0) that cannot enter it. A Tanya / SEAL / Yuri Prime / Robot Tank on
  a section collapsing into water lives. (`FUN_00654C00` only queues the cell.)
  Clone: `World::low_bridge_cell_kill`.

### B3 High bridges — destruction modelled (W4-FIX-SEQ), on/under layer not

High bridges are the deck overlays `BRIDGE1/2` (0x18/0x19, concrete) and
`BRIDGEB1/B2` (0xED/0xEE, wood), one per deck cell over the river, plus the
ramp tiles at both ends (temperat.ini `BridgeSet=19` "Ovrps",
`WoodBridgeSet=80` "Ovrpsb"); cell flags 0x100/0x80 mark bridge cells, units
cross `OnBridge` at `BridgeHeight` = 416.
* Destroy dispatch `FUN_00586BB0` (cell): low wood `0x49 < ovl < 100` ->
  `FUN_0057B4D0`, low concrete `0xCC < ovl < 0xE7` -> `FUN_0057C720`; a bridge
  cell (+0x140 & 0x100) whose owner cell holds 0x18/0x19 -> `FUN_005766D0`
  (concrete); BridgeSet/WoodBridgeSet ramp tiles or a 0xED/0xEE owner ->
  `FUN_00570FC0` (wood). The high functions walk the span both ways
  (`FUN_00571D60/00571E60/00571F70/00572310`), place broken ramp tiles
  (`FUN_0056E6B0`), and run `FUN_0047DC80` on each cell [CONFIRMED structure;
  the per-section walk is RE-FALLBACK in the clone]. `FUN_0047DC80` kills
  EVERY object in the ground list (+0xE4: ships, swimmers, hover and ground
  units under the span) with `ReceiveDamage(Health, C4Warhead,
  ignoreDefenses=1, preventPassengerEscape=1)` and drops every object in the
  deck list (+0xE8) with vt+0xEC `FUN_005F3970` (IsFallingDown, die on
  landing: `FUN_005F3680` deals the same lethal hit on touchdown)
  [CONFIRMED, W8-01-01/02]. Clone: `World::high_bridge_cell_kill` kills every
  non-airborne unit of the cell at once, with its passengers (no fall anim).
* A crashing jumpjet that was above an intact deck stops on it and fires its
  death weapon at the deck's Z (`FUN_0054C7C0`) [CONFIRMED, W8-05-04].
* Trigger 63 Apply100Damage (`FUN_006DFA00`) calls `FUN_00586BB0` on its
  waypoint cell up to 4 times (no BridgeStrength roll, no DestroyableBridges
  gate) [CONFIRMED].
* Clone (`World::set_high_bridges / break_high_bridge / repair_high_bridge_near
  / force_bridge_break`): deck cells over water are ground-passable while
  intact (they were blocked before -- no unit could cross a high bridge), a
  break takes the whole connected deck at once, kills every unit on it and
  under it and hides the deck overlays; the ramps keep their art and stay passable; a
  BridgeRepairHut within 4 cells restores the span; weapon hits on a deck use
  the low-bridge Wall=/Random(1,BridgeStrength) roll [RE-FALLBACK]. Units on a
  deck are drawn at ground level (no OnBridge layer).

### B4 Repair — CONFIRMED (entry) / [RE-FALLBACK] (walk)

`Mission_Enter` `FUN_00519449`: an Engineer (InfantryType +0xEC3) entering a
building with `BridgeRepairHut=yes` (+0x16B6, CABHUT): EVA (player), plays
`RepairBridgeSound` (Rules +0x248) at the hut, scans ±2 cells around for the
bridge kind, calls the low (`FUN_00573070` → `FUN_0057EE70` for concrete) or
high (`FUN_0056FB80`) repair, notifies every house, and then the engineer is
deleted (the common tail `vtable +0xF8`) — **the engineer is consumed**, the hut
is never captured. Clone: `World::repair_bridge_near` restores every damaged /
destroyed low section of the bridge run reached from a low-bridge cell within
±2 of the hut to an intact piece (`low_bridge_repaired`) [RE-FALLBACK: the
piece-by-piece restore walk is not traced].

### B5 Force-fire at the ground (Ctrl+click)

Needed to demolish a bridge on purpose. The clone adds a ground target to the
unit (`order_attack_cell`): pursue into weapon range, slew the turret, fire at
ROF; the shot lands on the cell (unit/building there + the bridge roll).
[RE-FALLBACK] the shot is resolved at the ROF tick without a projectile flight.

## §C Evidence

`scripts/play/world-depth.play` (real shell → lobby → GameScene) PASSES:
3 GIs garrison a civilian building (owner flips to the player), shots fire out
and kill two Conscripts, Deploy evicts them (owner back to neutral); on May Day
(crossings shipped blown) an engineer repairs the bridge at the hut (consumed),
a Grizzly drives across, force-fire collapses a section, a second engineer
repairs it. `--depth-survey` lists every stock MP map's bridges/huts/garrisons.
