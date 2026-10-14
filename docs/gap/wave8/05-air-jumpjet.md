# Wave 8 interaction audit 05: aircraft, jumpjets, missiles and paratroopers across terrain layers

**Auditor:** w8int-05. **Tree:** master `34cb652`, `build/ra2engine.exe`.
**Original:** gamemd.exe YR 1.000, examined in Ghidra on a private copy of the `bridge` project. The copy was deleted after the audit.
**Evidence:** everything is in `D:\ra2-re\w8int-05-out\`:

| Folder | Contents |
|---|---|
| `re\b1..b12.txt` | Decompiles and disassembly |
| `play\e0..e3.play` | Play scripts, with their `play.log` outputs and shots |
| `play\all01_terrain*.txt` | `map_terrain` dumps of ALL01T |

**Known items not re-filed:** the wave 7 items W7-05-02..15, W7-01-21/27/38, W7-12-04/20/44, W6-04-17 (Siege Chopper deploy) and R-110.

## Summary

The aircraft code has no notion of the high-bridge deck and no notion of the ground height a flier crosses. Each flier's altitude counts from the level of the cell it is over. The clone also lands its jumpjets by a rule the original does not have: take any walkable free cell, and otherwise fly to one. Seven gaps, all reproduced or contradicted by RE:

1. **W8-05-01 (MAJOR): Night Hawk landing rules.** The original refuses to land on Water or Beach, and that includes a high-bridge deck, which keeps the Water land type of the tile under it. It also does not land on a cell it cannot enter, and it never relocates: it hovers. The clone does two things wrong:
   - It lands on the deck over the river.
   - Ordered onto water, a cliff face or an occupied cell, it flies to the nearest free ground cell within 4 cells and lands there. That cell can be on top of a cliff.

   As a result, an unload order over water ferries the troops up onto the cliff top.
2. **W8-05-02 (MINOR):** a jumpjet that has landed on a deck is drawn at water level, under the deck sprite, and cannot be seen.
3. **W8-05-03 (MINOR):** flier height ignores the deck and snaps over cliffs.
   - Jumpjets fly 500 leptons above the water under a deck. That is 84 leptons above the deck, where the original flies 500 above it.
   - Crossing a cliff edge, the flier jumps 60 px in one frame. The original climbs at JumpjetClimb and looks one cell ahead.
   - The 3-D range Z carries both errors and has no deck term at all.
4. **W8-05-04 (MAJOR):** V3 and Dreadnought rockets and the crash death weapons (the Kirov's BlimpBomb) can never damage a bridge. In the original every one of them rolls a bridge hit through DamageArea.
5. **W8-05-05 (MINOR):** the clone rolls a high-bridge hit for any explosion in a deck cell, whatever its height. A Kirov bombing a ship under the bridge can therefore break the bridge. The original only rolls when the blast's Z is within -2..+1 levels of the deck.
6. **W8-05-06 (MINOR):** a blast on the deck also hits the ship under it, and the other way round. The original's DamageArea picks either the deck's object list or the ground list by the blast's Z.
7. **W8-05-07 (MINOR):** paratroopers are placed differently.
   - The original drops one trooper every 5 frames at the plane's position, to alternating sides, into free sub-cell spots, so several share a cell. It loops back over the target to finish the drop.
   - The clone drops one trooper per frame, one per whole cell, in rings around the target.

## Matrix

Legend: **OK** = matches the original, **GAP** = a W8-05 item (number in brackets), **N-A** = the state cannot happen, **known** = an existing item.

### Jumpjets

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| Night Hawk (SHAD) | clear / road / ore | OK (lands; Hover can enter) | OK | OK | OK |
| Night Hawk | water | GAP (01): relocates to land; the original hovers | OK | OK | OK (death explosion; the land type picks ImpactWater) |
| Night Hawk | beach | GAP (01): relocates; the original hovers | OK | OK | OK |
| Night Hawk | cliff face (Rock) | GAP (01): relocates (E3: to 75,47); the original stays | GAP (03) snap | GAP (03) | OK |
| Night Hawk | high-bridge deck over water | GAP (01): lands (E1-A); the original refuses (LandType Water) | GAP (02): invisible under the deck | GAP (03) | GAP (04/05) |
| Night Hawk | deck over land | GAP (01/02): lands under the deck (`on_deck` stays 0); the original lands on it (OnBridge) [code + RE] | GAP (02) | GAP (03) | GAP (04) |
| Night Hawk | building / occupied cell | GAP (01): relocates (E3: 83,46 → 82,45); the original does not land | OK | OK | OK |
| Night Hawk unload | over water / cliff | GAP (01): ferried to the nearest land, possibly a cliff top (E1-C: to 81,50 at L6) | OK | n/a | n/a |
| Rocketeer / Kirov | any (BalloonHover) | OK (never land, in both) | GAP (03): deck / cliff | GAP (03) | GAP (04) for the Kirov crash on a deck |
| Siege Chopper | any | OK (hovers in both: DeployToLand; deploy is W6-04-17) | GAP (03) | GAP (03) | OK |

### Fixed-wing planes

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| Harrier, Black Eagle, Hornet, PDPLANE | over a deck or a cliff | N-A (they land only on pads or the carrier) | GAP (03) (FlyLoco also measures height over the deck) | GAP (03) | crash damage known (W7-05-09); a crash on a deck GAP (04) |
| Airfield pads | — | known (W7-05-04/07) | OK | OK | OK |
| Carrier with Hornets near cliffs | — | OK (straight-line flight) | GAP (03) cosmetic | OK | OK |

### Missiles

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| V3 / DMISL | target on a deck | OK | the rocket dives to water level (03) | OK | GAP (04): no bridge roll; GAP (06): hits both layers |
| V3 / DMISL | target under a deck | OK | OK | OK | OK (no roll in either); GAP (06) |

### Kirov bombs

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| Kirov bomb | target on a deck | OK | n/a | OK | OK (rolls in both); GAP (06) |
| Kirov bomb | target under a deck | OK | n/a | OK | GAP (05): rolls; GAP (06) |

### Paratroopers

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| Paratrooper | water / cliff / building | OK (refused in both; nobody drowns) | OK | known (W7-01-38c) | n/a |
| Paratrooper | deck over water | OK (on the deck in both) | OK | n/a | n/a |
| Paratrooper | deck over land | GAP (07): ground level; the original sets OnBridge [PROBABLE] | — | — | — |
| Paratrooper | sub-cell spot and cadence | GAP (07) | — | — | — |

### Shadows and anti-air

| Unit family | Terrain / layer | Reach / landing | Height / draw | Targeting / 3-D range | Damage / crash / bridge |
|---|---|---|---|---|---|
| Shadows | over cliffs and decks | — | OK (the jumpjet shadow sits on the terrain floor under a deck in both: Shadow_Point `0x55A550`) | — | — |
| Ground anti-air on a deck vs aircraft | deck | — | — | GAP (03): the shooter's Z has no deck term | FlakWH has no `Wall=`, so no bridge hit in either: OK |

## Gaps

### W8-05-01: Night Hawk landing: lands on a deck over water, and relocates instead of hovering where it cannot land (MAJOR)

**Evidence**

[CONFIRMED-PLAY] `play\e1` (ALL01T, east bridge repaired by the engineer):
- **A.** A Night Hawk ordered onto the deck cell (82,53) over the river lands there: f757 `sel_altitude=0`, `sel_flight_state=0`, `udeck(0,SHAD,0)=1`.
- **C.** A Night Hawk carrying a GI is ordered over open water (78,53), where it hovers at altitude 500. After `key d` (unload) it flies to (81,50), which is on top of the level-6 cliff. It lands there and unloads: f1438 `sel_cargo=0`.

[CONFIRMED-PLAY] `play\e3`, an empty Night Hawk:

| Ordered to | Where it landed | Frame |
|---|---|---|
| Water (78,52) | relocated to (81,49) on the cliff top and landed | f761 `airborne(0)=0` |
| Cliff-face Rock cell (78,50) | (75,47) | — |
| Cell held by a GI (83,46) | (82,45) | — |

**Original** [CONFIRMED-RE] JumpjetLocomotionClass:
- **Cruising `FUN_0054BD20`.** On arrival (dist < 0x14), a unit that is not BalloonHover and has no NavCom gets target height 0 and goes to state 4 (Descending). A Siege Chopper (IsSimpleDeployer + DeployToLand) hovers instead.
- **Descending `FUN_0054C280`, the land-type check (@0x54C2AA..0x54C38E).** It applies when the cell's LandType (`+0xEC`) is Water (2) or Beach (6) and the mission is not Enter (7):
  - If another jumpjet holds the cell (`FUN_004135A0`), the Night Hawk moves to a random adjacent cell (`FUN_0065C220(0,7)`, `FUN_00481720`).
  - Otherwise it claims the cell (`FUN_00487C80`), goes to state 2 (Hovering) and restores the full height.
  - It never lands there, and it never looks for land.
- **High-bridge decks keep the tile's land type.** `[BRIDGE1]`, `[BRIDGE2]` and `[BRIDGEB1]` have `NoUseTileLandType=false` and no `Land=` [CONFIRMED-DATA], so a deck over the river is Water and is refused.
- **Descending, the other cells (@0x54C38F..0x54C44A):**
  - `Can_Enter_Cell` (vtbl+0x1AC) and the sub-cell spot (`FUN_00480FB0` / `FUN_00481040`) decide.
  - If the spot is not free, or the move result is > 2, it calls `ILocomotion::Stop_Moving` (+0x48) and does not land.
  - A bridge cell (flag 0x100) skips the move-result test. At height 0 it then sets OnBridge (`+0x8C`) when Z is above the floor (@0x54C5A9..0x54C5E9), so a deck over land is a legal landing **on the deck**.
  - SpeedType=Hover cannot enter Rock, so it does not land on a cliff face.
- **Relocation.** No jumpjet state (Grounded `0054B6B0`, Ascending `0054B760`, Hovering `0054BA60`, Cruising, Descending) searches for another landing cell.

**Clone**
- `world_an.cpp:1653-1677`: `pad_ok = walkable && occ free && no building`, where `walkable` is `passable_`. `set_high_bridges` makes the deck cells walkable (`world_depth.cpp:542-548`).
  - If the spot fails, the Night Hawk flies to the first `pad_ok` cell within 4 rings, with no height gate, and lands there.
- `world.cpp:2083-2086`: an unload order clears the goal, so the same landing (and relocation) runs.

**Root cause.** The landing test is the ground-unit passability grid plus an invented relocation. It is not the Water/Beach refusal of the jumpjet locomotor followed by the Hover `Can_Enter_Cell` test.

**Fix**
- In the landing pass, refuse the landing when the cell's per-tile LandType is Water or Beach, which includes high-bridge deck cells over water. The Night Hawk then hovers at full height.
- Otherwise land only when the Hover `Can_Enter_Cell` test passes and the spot is free. If it does not pass, stay hovering.
- Remove the 4-ring relocation.
- Landing on a bridge cell sets `on_deck = 1`.
- Note: the unload mission of a jumpjet transport (UnitClass Mission_Unload) was not traced. If it turns out to pick a land cell itself, put the relocation there, not in the locomotor.

**Proposed test.** Extend `e3.play` into `scripts/play/w8-shad-landing.play`:

| Order | Assert |
|---|---|
| To (78,52) | after 300 frames `airborne(0)==1 && ucell(0,SHAD,0)==78052` |
| To (82,53) after the bridge repair | `airborne(0)==1` |
| To (82,45), free road | `airborne(0)==0` |

### W8-05-02: A jumpjet that has landed on a high-bridge deck is drawn under the deck (MINOR)

**Evidence**

[CONFIRMED-PLAY]
- `play\e1\shad_on_deck.png` and the zoomed copy `shad_on_deck_zoom.png`: the landed Night Hawk at (82,53) cannot be seen. Only its health bar and a few green pixels show at the deck edge.
- `shad_unload_water.png`: the second Night Hawk, landed on land at (81,50), draws normally.

**Clone**
- `pres_layers.inc:25-27` `pres_on_high_deck` returns false for `u.is_aircraft`. A landed jumpjet with `on_deck == 1` therefore gets no `kPresBridgeLevels` lift and no `kPresOnDeckBias` sort.
- It is drawn at the water level, and the deck sprite sorts over it.

**Original.** A landed jumpjet is an ordinary unit with OnBridge set (W8-05-01), drawn at the deck height.

**Root cause.** The deck-layer draw excludes every aircraft, landed ones included.

**Fix.** Exclude only airborne aircraft, `pres_airborne(u)`. A grounded jumpjet with `on_deck` gets the deck lift and sort. This is needed even after W8-05-01, for decks over land.

**Proposed test.** After a Night Hawk lands on a deck over land: assert a `pres(...)` probe of the draw lift equals 4 levels, or do a pixel check that the hull is visible in a shot.

### W8-05-03: Flier height ignores the deck and the terrain ahead, and the 3-D range Z has no deck term (MINOR)

**Evidence**

[CONFIRMED-PLAY] `play\e2`:
- **Over the deck.** A Rocketeer over the deck cell (82,52): `udeck=1`, altitude 500 above the level-2 river. In `rock_deck_zoom.png` it stands just above the deck's railing: 72 px of altitude minus the 60 px deck lift leaves about 12 px.
- **Over the cliff edge.** The Rocketeer crosses the cliff edge (79,49) L6 → (79,50) L2. `cliff_abc.png` shows three shots at f864, f868 and f870. Between a and b its screen y drops 73 px while x moves 15 px, so 60 px of that drop is the 4-level snap. Between b and c it moves only 2 px down.

[CONFIRMED-RE] The original's floor, in the JumpjetLoco integrator `FUN_0054CDA0`:
- **The floor.** The floor is `FUN_00577AB0` (the cell floor height). It gains `+ BridgeHeight` (`DAT_00ABB544`, 416) over a bridge cell when Z ≥ floor + 4 levels.
- **While cruising or ascending.** The target floor is `FUN_0054D4B0`:
  - the current cell's height `+ BridgeHeight` on any bridge cell;
  - when the unit is moving, the cell one step ahead along the facing; the result is the max of the two when the cell ahead is higher, else their average.
- **Climb rate.** Z moves toward floor + height by at most JumpjetClimb (`+0x24`) per frame, both up and down.
  - The Kirov (Climb 6) needs about 70 frames to rise 4 levels.
  - The Rocketeer (Climb 20) needs about 21.
- **Ascending and Descending.** Ascending `FUN_0054B760` and Descending `FUN_0054C280` subtract BridgeHeight from the height over a bridge cell when the unit is not OnBridge. The height is therefore counted above the deck.
- **Planes.** The FlyLocomotion step (`D:\ra2-re\w7-air-a-out\fly_move.c`, the `DAT_008B2C14` bridge block) does the same for planes.
- **3-D range.** The 3-D range test (`FUN_006F6750`, W5-B R-063) uses absolute coords. A unit OnBridge stands at floor + BridgeHeight (Descending @0x54C5A9).

**Clone**
- `world.cpp:2174-2251` `step_flight`: the altitude is a scalar above "the ground".
- `world.cpp:6094-6097`: `u.level` is re-patched to the cell's level every frame.
- The draw is `level*15 + altitude_to_screen_dy(altitude)` (`main.cpp:3615`). It has no deck term and snaps when the cell changes.
- `combat.cpp:734-736` `unit_lept_z = level*104 + altitude` for both the shooter and the target. It has no `+416` for a deck shooter or target, and no lag over cliffs.
- `world.cpp:6165` `unit_lz` (the projectile aim Z) is the altitude only, so a V3 aimed at a tank on a deck dives to the water.

**Impact**
- The visuals are wrong (the Rocketeer "standing" on a bridge, a 60 px pop at every cliff edge).
- The AA reach is wrong by up to 416 leptons of dz (about 0.6 cell at range 8) whenever a deck or a cliff is between the shooter and the target. That covers ground units on a deck firing at aircraft, and AA firing at jumpjets hovering over a deck.

**Fix**
- Keep an absolute flier Z, or a floor that follows `FUN_0054D4B0`: deck +416 on bridge cells, the one-cell lookahead, and change by at most JumpjetClimb per frame (the FlyLoco climb rate for planes).
- Draw from that Z.
- Add `on_deck ? 416 : 0` to `unit_lept_z` and `unit_lz` for ground units on a deck.

**Proposed test.** A Rocketeer crossing (79,49) → (79,50) on ALL01T: a probe of absolute Z per frame (new key `uz(h,ID,n)`) changes by at most 20 per logic frame. Over (82,52) after the repair: `uz == 6*104 + 500`.

### W8-05-04: Rockets (V3, DMISL) and crash death weapons never damage bridges; a Kirov crash falls through the deck (MAJOR)

**Evidence**

[CONFIRMED-RE] The original rolls a bridge hit on all three paths:
- **Bridge damage lives in DamageArea `FUN_00489190`** (@0x489E87..0x48A1D4). A `Wall=yes` warhead, `DestroyableBridges`, and a roll of `Random(1, BridgeStrength) < damage` (or IonCannonWarhead) drive `FUN_00586BB0` on the high-bridge owner cell (0x18/0x19, 0xED/0xEE, or the ramp tiles), and `FUN_0057B4D0` / `FUN_0057C720` for low bridges.
- **The V3 / DMISL rocket detonation** `FUN_00662A70` calls DamageArea (call site 0x662D07).
- **FireDeathWeapon `FUN_0070CAC0`** (the Kirov crash @0x745001, plane crashes @0x4CD729) fires a bullet that detonates through `FUN_004690A0`, which calls DamageArea @0x4699A0.
- **The jumpjet Crashing handler `FUN_0054C7C0`** stops the fall on the deck. It flags a deck hit when the cell has the bridge flag, the old Z ≥ floor + BridgeHeight and the new Z drops below it. It then sends 0x117C, which fires the death weapon at the deck's Z.

[CONFIRMED-DATA] `[V3WH]`, `[DMISLWH]` and `[BlimpHE]` all have `Wall=yes`, and BridgeStrength is 1500. The roll odds per hit are therefore:

| Weapon | Damage | Odds of breaking the bridge |
|---|---|---|
| V3 rocket | 200 | 13 % |
| Kirov death weapon | 250 | 16.6 % |

[CONFIRMED-PLAY, weak] `play\e2`: a Kirov killed over the deck (82,52) fell, and `bridge_destroyed` / `bridge_version` stayed unchanged. One sample, so this is consistent with the code but not a proof by itself.

**Clone**
- `bridge_explosion` is called only from the projectile impact (`world.cpp:6593-6598`) and from a ground cell shot (`world_depth.cpp:726`).
- `rocket_explode` (`world_an.cpp:265-293`) goes through `an_area_damage` (`world_sw.cpp:298-353`), which has no bridge code.
- The crash death weapon (`world_an.cpp:1128-1160`) goes through `ability_area_damage` (`world_abilities.cpp:274-293`) → `damage_area`, which also has no bridge code.
- The crash altitude counts down to the cell's ground level, so the Kirov falls through the deck to the river.

**Root cause.** The bridge roll was attached to two call sites instead of to the area-damage applicator, which is where DamageArea does it.

**Fix**
- Move the `bridge_explosion` roll into the shared area-damage path (`damage_area` and `an_area_damage`), with the Z gate of W8-05-05.
- Stop a crashing jumpjet at the deck when it was above it.

**Proposed test.** A deterministic RNG seed and V3s firing at a tank parked on the ALL01T east deck: within 10 rockets assert `bridge_version` increases. Also a unit test: `an_area_damage` at a deck cell with `wall_flags` and damage 1501 always breaks the bridge.

### W8-05-05: A high-bridge hit is rolled for explosions at any height (MINOR)

**Evidence**

[CONFIRMED-RE] `FUN_00489190` @0x489E87 and @0x489FB5 (both the concrete and wood high-bridge branches). For a bridge cell, the roll is skipped unless:

```
(Level - 2) * 104 + BridgeHeight  <  Z  <=  (Level + 1) * 104 + BridgeHeight
```

where Level is the cell's `+0x11B` and `DAT_0089D7E0` = 104. The roll is skipped for:
- a shell landing on a ship or unit under the deck (Z = the water level);
- an air burst more than one level above the deck.

**Clone.** `world_depth.cpp:463-476` `bridge_explosion(cx, cy, damage, wall)` has no Z parameter. Every Wall= projectile impact in a deck cell rolls (`world.cpp:6597`), including a Kirov bombing a destroyer under the bridge. In `play\e2` it does exactly that: about four BlimpBomb hits at (82,53), each one a 16.6 % roll. That run did not break the bridge, so the evidence is the RE and the code.

**Root cause.** The Z gate of DamageArea was not carried over.

**Fix.** Pass the impact Z, which is absolute, deck included (W8-05-03), and roll only inside the window above.

**Proposed test.** A unit test: `bridge_explosion` with `z = level*104` (under the deck) never changes `bridge_version`, for damage 1501. With `z = level*104 + 416` it always does.

### W8-05-06: Explosions on or under a deck hit both layers (MINOR)

**Evidence**

[CONFIRMED-RE] `FUN_00489190`, after the air scan:
- If the cell has the bridge flag and Z > floor + `DAT_0089D7D4 / 2` (half the bridge height), it sets a flag in byte 2 of a local (stack) flags dword.
- The per-cell object loop then walks `cell+0xE8` (the objects on the bridge) instead of `cell+0xE4` (the ground or water objects).
- A blast on the deck therefore never hurts the ship under it, and the reverse.

**Clone.** `damage_area` (`combat.cpp:911-960`) and `an_area_damage` (`world_sw.cpp:298-320`) measure every non-airborne unit in one plane. `unit_lept_z` has no deck term, so a Kirov bomb or V3 aimed at a GI on the deck also damages a destroyer under it, and the reverse.

[CONFIRMED-PLAY] was attempted in `play\e2`. The deck GI was killed by the fort's own defenders before the Kirov arrived, so the run is inconclusive. The finding rests on RE and code.

This overlaps any ground or naval auditor's deck findings. It is filed here for the Kirov, V3 and DMISL cases.

**Fix.** In both applicators, for a target cell with an intact deck, skip units whose `on_deck` differs from `impact_z > level*104 + 208`.

**Proposed test.** A unit test: a blast at deck Z over (82,53) with a DEST (`on_deck=0`) and an E1 (`on_deck=1`) in the cell damages only the E1. A blast at water Z damages only the DEST.

### W8-05-07: Paratrooper drop cadence and sub-cell placement (MINOR; refines the RE-FALLBACK of R-038 and `world_an.cpp:1317`)

**Evidence**

[CONFIRMED-RE] AircraftClass:
- **Mission_ParadropApproach `FUN_004158E0`** (vtbl+0x260).
- **Mission_ParadropOverfly `FUN_00415960`** (vtbl+0x264) returns 5, so it runs every 5 frames.
  - While it is within ParadropRadius (Rules+0x54C) of the target, it calls `FUN_00415C60`.
  - Out of radius, if passes remain (`+0x6D3`), it re-enters Approach (mission 0x1A) and loops back over the target.
- **`FUN_00415C60` drops one passenger:**
  - The drop point is the plane's own coords, offset ±90° from its facing; the side alternates with the passenger count.
  - It refuses the drop when the trooper's `Can_Enter_Cell` (vtbl+0x1AC) fails (water, Rock, buildings) or when `FUN_00481090` finds no free sub-cell spot. The trooper goes back aboard and is retried 5 frames later.
  - On success it calls SpawnParachuted.
- **SpawnParachuted `FUN_005F5150`** sets OnBridge (`+0x8C`) on a bridge cell (flag 0x100). It refuses the drop when flag 0x200 is clear; the meaning of 0x200 is not pinned [PROBABLE].

**Clone**
- `world_an.cpp:944-975` (superweapon plane) and `an_drop_passenger` `world_an.cpp:1330-1357` (team plane) pick the first cell in rings of 0-4 (or 0-6) around the **target** that is walkable, has no occupant and has no building.
- They drop one trooper per frame, one trooper per whole cell. A trooper landing on a deck over land stays on the ground layer (`sync_deck_layer` keeps 0 where `deck_land_ == 1`).

**What matches (OK).** In both, a trooper never lands in water, on a cliff face or on a building, and nobody drowns.

**Fix.** Drop at the plane's position every 5 frames, offset to alternating sides, onto the closest free sub-cell spot (several per cell). Pass over the target again (up to 5 passes) while passengers remain. On any bridge cell, set `on_deck = 1`.

**Proposed test.** `sw.play` American paradrop: assert that at least 2 E1 share a cell (`ucell` duplicates), and that the drop spans about 6 × 5 frames.

**Status: FIXED (w8fix-save).** `World::an_para_jump` + the Approach/Overfly state (`AnUnit::para_passes/para_overfly/para_timer`, save v42) implement FUN_004158E0 / 00415960 / 00415C60 for both the superweapon and the team planes; the R-038 ring fallback is gone. The jump point is 128 leptons off the plane's track (DAT_007E1808). Tests: `tests/test_save_states.cpp` (W8-05-07 cases).

## Verified OK

- **Paratroopers.** They are never dropped into water, onto a cliff face or onto a building. Both sides refuse such cells, the clone through `walkable`, the original through `Can_Enter_Cell` in `FUN_00415C60`. Superweapon troopers now fall under the parachute (`para_start`; W7-01-07 is fixed).
- **BalloonHover jumpjets never land.** That covers the Rocketeer and the Kirov [RE `FUN_0054BA60` / `0054BD20`; code `world_an.cpp:1653`].
- **The Siege Chopper hovers.** It hovers in the original too, because DeployToLand + IsSimpleDeployer make Cruising keep it at full height [RE `FUN_0054BD20`]. The clone gets the same result through `Landable` absent. The deploy itself is W6-04-17.
- **Night Hawk landing on clear, road and ore.** It lands where a Hover unit can enter [RE / E3].
- **Jumpjet shadow.** JumpjetLocomotion uses the base Shadow_Point `0x55A550`, which offsets by `GetHeight`, the height above the terrain floor. Over a deck the shadow falls on the floor beneath, as in the clone (`main.cpp:3618-3628` at `u.level`). FlyLocomotion Shadow_Point `0x4CF860` is (0,0).
- **Crash impact sound.** The sound over water vs land is chosen by the cell (FlyLoco `0x4CD75x`; clone `pres_audio_events.inc:459-463`).
- **Kirov bombing a target on a deck.** The projectile path rolls the bridge hit, as the original does at deck Z.
- **AA bursts never break bridges.** `[FlakWH]` has no `Wall=` [DATA], so in both games anti-air fire at a jumpjet over a deck cannot damage the bridge.
- **Airborne units and deck cells.** An airborne unit over a deck cell gets `on_deck=1` (E2) but holds no occupancy, so it never blocks ground traffic on the deck.

## Run notes

- `play\e1`, `play\e2` and `play\e3` use `assert X == -999` to log values; their FAIL counts are expected. Each is about 1,700 frames on ALL01T, run one at a time.
- Probe keys used: `udeck`, `uheight`, `ucell`, `sel_altitude`, `sel_flight_state`, `cargo_total`, `bridge_destroyed`, `bridge_version`, `crashing`.
- The east bridge (deck x=82, y 51-55) is repaired with the engineer and hut sequence from `move-bridge-cliff.play`.
