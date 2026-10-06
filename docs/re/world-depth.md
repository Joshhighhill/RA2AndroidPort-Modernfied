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

Resets FiringOccupantIndex, finds one exit cell by walking outward from the
foundation edge, unlimbos every occupant there (an occupant that cannot be
placed is deleted), AI-owned occupants get mission Hunt, then the vector is
cleared. Callers: Mission_Unload `FUN_0044D880` (the Deploy order on the
building), the sell path `FUN_00449B70`, building destruction `FUN_004421B0`,
and the ownership sync above. When no exit cell exists the occupants are
killed (`FUN_004585B0` KillOccupants) [PROBABLE].
Clone: `World::unload_occupants` (Deploy on a selected own garrison = `D` or
the ACTION_SELF click; sell/death release). [RE-FALLBACK] the clone's one-unit
cells put each occupant on the nearest free cell to the foundation's bottom
corner instead of stacking sub-cells.

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
  Clone: `depth_set_terrain` + `World::set_bridge_cell`.
* Damage — `MapClass::DamageArea` `FUN_00489190`: an explosion in a low-bridge
  cell whose warhead has `Wall=yes` (WarheadType +0x144) breaks the section when
  the warhead is `IonCannonWarhead` (Rules +0xFF0) or
  `Random(1, BridgeStrength) < damage` (`BridgeStrength` = Rules +0x1740,
  1500). Wood range `0x49 < idx < 100`, concrete `0xCC < idx < 0xE7`
  (`FUN_00586BB0` dispatch). Gated by `DestroyableBridges` /
  `BridgeDestruction`. Clone: `World::bridge_explosion`, fed by projectile
  detonations and ground (force-fire) shots. [RE-FALLBACK] instant-hit weapons
  do not roll; the RNG is a dedicated xorshift, not the scenario RNG.
* Section — `FUN_0057B4D0` finds the centre of the 3-cell cross-section
  (looks one / two cells back along the cross axis); `FUN_0057B720` (cross
  axis y) and `FUN_0057BCE0` (cross axis x) rewrite the three cells:
  intact (< 0x50 / < 0x59) → damaged (0x50 / 0x59), damaged (0x50–0x52 /
  0x59–0x5B) → destroyed (100 / 0x65), end pieces 0x5C→0x5D, 0x5E→0x5F,
  0x60→0x61, 0x62→99. Concrete mirrors the wood set at +131 [PROBABLE: the
  concrete rewrite functions `FUN_0057C990`/`FUN_0057CF60` were not dumped;
  `FUN_0057C720`'s dispatch thresholds are exactly +131].
* A ground unit left on a collapsed section dies [PROBABLE: `FUN_00654C00`
  runs on the three cells only in the destroy branch].

### B3 High bridges — not modelled

High bridges are tile-based (bridge tile sets + `BRIDGE1/2` head overlays,
cell flags 0x100/0x400, units `OnBridge` at `BridgeHeight` = 416). Their
destruction (`FUN_005766D0`/`FUN_00570FC0`) and the on/under layer are left for
a later pass; their cells keep the tile land type.

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
