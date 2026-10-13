# W8 interaction audit 06: special effects/states × terrain/layer

Auditor: w8int-06. Clone: D:\ra2-android master 34cb652, build\ra2engine.exe. Original: gamemd.exe (YR 1.000), Ghidra on a private copy of the root `ra2` project. That copy has now been deleted.
Evidence:
- Ghidra dumps: `D:\ra2-re\w8int-06-out\*.txt`.
- --play scripts: `D:\ra2-re\w8int-06-out\play\*.play`.
- Runs: `D:\ra2-re\w8int-06-out\run_{a,b,c,d}\play.log` and `run_*_err.txt`.

## Summary

The audit crossed 14 effects and states with water, beach, deck, under-deck, cliff and occupied cells, for six unit families. It checked effect code paths against the original and ran four play reproductions on ALL01T (the east high bridge) and the default skirmish map.

Six new gaps (W8-06-01..06). None of them appears in wave 7.

1. **BLOCKER: a save made while a paratrooper hangs under its chute cannot be loaded.**
   - The save validator rejects `cargo_of == kParachuteCargo` as an out-of-range unit reference.
   - In skirmish the load shows "Error loading game!".
   - In the ALL01T campaign the game then crashes (0xC0000005).
   - [CONFIRMED-PLAY run_b, run_c]
2. **MAJOR: a collapsing high-bridge span kills only one object per deck cell.**
   - With three GIs on one deck cell, two survive. They are left standing in the river (on_deck 0, height 2).
   - [CONFIRMED-PLAY run_a; CONFIRMED-RE 0x47DC80]
3. **MAJOR: a collapsing span never kills what is under it.**
   - In the original, every object in the cell's ground list (FirstObject) gets lethal C4Warhead damage with ignoreDefenses: swimmers, ships and hovercraft under the deck.
   - In the clone, a SEAL swimming under the deck survives.
   - [CONFIRMED-PLAY run_d; CONFIRMED-RE 0x47DC80]
4. **MAJOR: Lightning Storm bolts never damage bridges.**
   - In the original, each bolt is a MapClass::DamageArea at the bridge-height coordinate with IonWH (Wall=yes, 250 damage). That gives a Random(1,1500) < 250 roll, about 17 %, on a deck or low-bridge cell.
   - The clone's storm bolt has no bridge path.
   - [CONFIRMED-RE 0x53A030 → 0x489190 @0x489DCB-0x489F1E]
5. **MINOR: Chronosphere arrival on a bridge cell is layer-blind.**
   - The clone telefrags whatever shares the cell, including a ship or swimmer under the deck.
   - On a bridge over land, the clone keeps the unit's previous layer. The original telefrags only the deck (AltObject) list and lands on the deck.
   - [CONFIRMED-RE 0x717600]
6. **MINOR: the Chronosphere does not eject a riding Terror Drone or squid.**
   - The original Teleport locomotor calls ParasiteClass::ExitUnit (0x7188D9), as the clone already does for CLEG and Chrono-tank self-teleport.
   - [CONFIRMED-RE (the clone's own citation) + CONFIRMED-CODE]

A seventh item is a **minor RE contradiction (W8-06-07):** garrison exit cells. The original scans only the building's perimeter cells and kills the occupants when none is enterable. The clone searches outward with no limit.

Everything else in the matrix matches the original or is already a wave-7 item; see the verified-OK list.

## The matrix

Legend: OK, GAP (W8-06-NN, or the wave-7 ID when already known), N-A (does not apply in stock data or to that family), "–" not exercised.

Column groups: Inf = infantry (GI), Swim = swimmer (SEAL / Tanya / Yuri Prime), Veh = vehicle (Grizzly / Mirage), Ship (Destroyer / LCRF), Air (Harrier / Kirov), Bld = building.

| Effect / state | Water | Beach | Deck (over water) | Under deck | Cliff | Occupied cell |
|---|---|---|---|---|---|---|
| Chronosphere destination | Veh: sink path W7-08-08; Inf killed (Organic) OK; Ship OK | Veh OK (land) | Veh lands on deck OK (water deck → on_deck 1); telefrag hits under-deck units **GAP 05** | **GAP 05** (bridge over land: layer kept) | impassable → kill W7-08-08 | telefrag W7-08-08; curtained occupant kills arrival OK |
| Chronosphere vs drone rider | – | – | **GAP 06** | **GAP 06** | – | – |
| Chrono Legionnaire erase | Swim OK (removed, no anim) | OK | OK | OK | N-A | N-A |
| Iron Curtain | Inf/Swim killed OK; Ship curtained OK; Air airborne skipped OK | OK | OK | OK | OK | N-A |
| IC'd unit on a destroyed bridge | – | – | OK (dies; the original drops it as a bomb) | **GAP 03** (never killed) | – | – |
| Mind control (campaign) | OK (layer-agnostic both sides) | OK | OK | OK | OK | W7-08-11 |
| Garrison exit | **GAP 07** (search not perimeter-bounded) | OK | – | – | **GAP 07** | OK (spread vs one cell: known RE-FALLBACK) |
| Terror Drone infest/eject | eject on water: stays riding when no cell, OK | OK | OK | OK | N-A | OK |
| Radiation | OK (cell-based both) | OK | OK | OK | OK | OK |
| Lightning Storm | units OK | OK | **GAP 04** (bridge never damaged) | **GAP 04** | OK | OK |
| Nuke | OK (NUKE warhead has no Wall=, so no bridge damage in the original either) | OK | OK | OK | OK | OK |
| Bridge collapse vs deck units | – | – | **GAP 02** (1 of N per cell) | **GAP 03** | – | **GAP 02** |
| Veteran self-heal | Swim OK (no water gate in either) | OK | OK | OK | OK | N-A |
| Mirage disguise | N-A | OK | OK (no terrain gate in the original 0x7456F0) | OK | OK | OK |
| Gap Generator shroud | OK (terrain-agnostic) | OK | OK | OK | OK | N-A |
| Spy / SEAL infiltration | SEAL: N-A (no Infiltrate=, no CanDisguise); Spy cannot swim | OK | – | – | – | – |
| Engineer via amphibious transport | – (unload on land only, W6 OK; capture not re-run) | – | – | – | – | – |
| Deploy (GI / DESO / YURIPR) | YURIPR swimming deploy: – (no RE) | OK | OK (no gate in either known) | – | – | – |
| Save/load | units in water OK [PLAY] | – | deck units + curtain OK [PLAY] | swimmer under deck OK [PLAY] | – | – |
| Save/load mid-paradrop | **GAP 01** (all families: unloadable save; campaign crash) | | | | | |
| Save/load mid-chronoshift | OK (code: chrono_left_frames, frozen, warp_remaining all serialised) | | | | | |

## Gaps

### W8-06-01: A save made during a paradrop cannot be loaded (campaign: crash) — BLOCKER

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY `play\b-parasave.play`, skirmish]: `cheat pres_para E1 0 3 3 3000`, then save. The banner says "Mission Saved". On load: `[saveload] load_world: index out of range: unit 94 unit reference`, `load FAILED`, banner "Error loading game!".</li><li>[CONFIRMED-PLAY `play\c-crash.play`, ALL01T]: the same failure (`unit 113 unit reference`), then the process dies with 0xC0000005.</li><li>`a-deck.play` crashed the same way.</li></ul> |
| **Original** | A paratrooper is an ordinary object in the air: IsFallingDown with a parachute anim. Saving and loading it is unremarkable. |
| **Clone** | <ul><li>`engine/savegame.cpp:306` accepts `cargo_of` only when it is in `[-1, nu)` or equal to `kGarrisonCargo`.</li><li>`kParachuteCargo` (0x3FFFFFFE, `world.hpp:427`) is set by every real drop: `World::para_start` (`world_an.cpp:1325`, the SW / W7-01-07 path) and `an_drop_passenger` (`:1343`).</li><li>The sentinel was simply never added to the validator.</li></ul> |
| **Root cause** | A missing sentinel in `load_world`'s reference check. The crash after the failed campaign load is a second defect on the failure path (SUSPECTED: a half-restored world). |
| **Player-visible** | <ul><li>Any save taken while American Paratroopers, the AmerParaDrop / ParaDrop SW, or a campaign reinforcement drop is in the air is lost.</li><li>In a campaign, loading it crashes the game.</li></ul> |
| **Fix** | <ol><li>Accept `u.cargo_of == kParachuteCargo` next to `kGarrisonCargo` at `savegame.cpp:306`.</li><li>Separately, make a failed campaign load fall back cleanly. Find the crash with the c-crash script under a debugger.</li></ol> |
| **Test** | `c-crash.play` / `b-parasave.play` must reach `banner != "Error loading game!"`, and the paratrooper must land after the load (`units(0,E1)` +1). |

**Status: FIXED (w8fix-save, save v42).** `valid_world` accepts `kParachuteCargo`; the validator now also range-checks the cargo / occupant lists, spawn nodes and air links. The crash was a use-after-free: the failure path reset the campaign while `vlist_` still held its vehicle-art pointers, and `setup_campaign` → `load_unloading_classes` read them (0xFEEEFEEE string). A refused load now throws out of the load scene into the error page instead of rebuilding in place. Tests: `tests/test_save_states.cpp`, `scripts/play/save-paradrop.play`, `save-paradrop-camp.play`, `save-load-fail-camp.play`.

### W8-06-02: A collapsing high-bridge deck kills only one unit per cell (also low bridges) — MAJOR

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY `play\a-deck.play`, run_a]:<ul><li>ALL01T east bridge, repaired by the engineer.</li><li>Three E1 on deck cell (82,54) (`max_cell_inf(0)==3`, all `udeck 1`), then `bridge_hit 82 54 100000`.</li><li>Result: `bridge_destroyed=9`, `units(0,E1)=2`.</li><li>The survivors stand at 82,54 with `udeck 0` and `uheight 2`, a GI standing in the river.</li></ul></li><li>The Grizzly on deck cell 82,53 (curtained) died, which is correct.</li></ul> |
| **Original** | [CONFIRMED-RE FUN_0047DC80, the cell kill called by the high-bridge destroy walks FUN_00570FC0 / FUN_005766D0 / FUN_0056EA80 / FUN_0047E380]: it iterates the **whole** object lists of the cell. The ground list (+0xE4, FirstObject) gets `ReceiveDamage(&Health, 0, Rules C4Warhead, 0, ignoreDefenses=1, 1, 0)`. Every object in the deck list (+0xE8, AltObject) gets vt+0xEC. |
| **Clone** | `world_depth.cpp:586`: `const int ou = occ_.at(c.x, c.y);` returns the **first** occupant only. With sub-cells, `Occupancy::at` returns one spot holder, so up to two more infantry live. The low-bridge collapse has the same pattern at `world_depth.cpp:450` [SUSPECTED for the low-bridge original, FUN_00654C00 not traced]. |
| **Fix** | Iterate every unit whose `cell_x/cell_y` is the section cell, or the three `spot_unit` entries plus the cell owner, instead of `occ_.at`. |
| **Test** | a-deck.play: `assert units(0,E1) == 0` after the break. |

Two notes on the original's deck list:
- The list membership comes from AddContent FUN_0047E7B0: `alt` puts the object on +0xE8, otherwise +0xE4.
- vt+0xEC is DropAsBomb by YRpp's ObjectClass virtual order, which is consistent with +0xE0 RegisterDestruction and +0xF8 UnInit as seen in TemporalClass::Update 0x719A70. The deck units fall and die on impact.
- The clone's instant kill of deck units is an acceptable stand-in. Only the "first occupant only" part is wrong.

### W8-06-03: Units under a high bridge survive its collapse — MAJOR

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY `play\d-under.play`, run_d]: a GHOST (SEAL) spawned at 82,51 under the repaired deck (`udeck 0`, `amphib_on_water(0)=1`) and an E1 on the deck at 82,53.</li><li>`bridge_hit 82 52 100000` gives `bridge_destroyed=9`. The E1 dies; the SEAL lives (`units(0,GHOST)=1`, still at 82,51).</li></ul> |
| **Original** | [CONFIRMED-RE FUN_0047DC80 first loop]: every object in the cell's FirstObject list (the ground layer: ships, swimmers, hovercraft under the span) takes lethal damage with C4Warhead, ignoreDefenses=1 and preventPassengerEscape=1. Iron Curtain does not save it, and passengers die. |
| **Clone** | `world_depth.cpp:583-591` deliberately spares them ("an amphibious unit swimming under it lives") and skips `is_naval`. |
| **Fix** | In `break_high_bridge`, kill every non-airborne unit in the cell whose `on_deck == 0` as well, ships included, with an ignore-curtain kill. Kill its cargo too (no escape). |
| **Test** | d-under.play: `assert units(0,GHOST) == 0`. Add a Destroyer under the span. |

### W8-06-04: Lightning Storm never damages bridges — MAJOR

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE FUN_0053A030, the storm bolt]: the coordinate is the cell centre with Z = level height + BridgeHeight when the cell has the bridge flag (+0x140 & 0x100), i.e. GetCoordsWithBridge. It then calls `FUN_00489190(coords, Rules LightningDamage, …)` (DamageArea) with LightningWarhead=IonWH.</li><li>DamageArea's bridge block @0x489DCB-0x489F1E (high) and @0x489FB5-0x48A0C6 (low) requires warhead +0x144 Wall=yes. IonWH has `Wall=yes` [CONFIRMED-DATA]. It also requires the explosion Z inside the deck window `[(level-2)·h, (level+1)·h]`, then `Random(1, BridgeStrength=1500) < 250`. That is about a 16.6 % chance per bolt to call the destroy dispatch FUN_00586BB0.</li><li>The Nuke's warhead `NUKE` has no `Wall=`, so a nuke never breaks a bridge [CONFIRMED-DATA]. The clone matches that.</li></ul> |
| **Clone** | `world_sw.cpp:583` `an_storm_bolt` → `an_area_damage` (`:298-350`): units, buildings and wall overlays only. `bridge_explosion` is never called. |
| **Fix** | In `an_storm_bolt`, call `bridge_explosion(x, y, an_.sw.storm_damage, storm warhead Wall=)`. Read the flag from the LightningWarhead section rather than hard-coding it. |
| **Test** | A play script with a campaign storm trigger (action 90/91, `campaign_runtime.inc:3408`), or a test cheat, over a high-bridge cell for N bolts, seeded: `bridge_destroyed` rises. A unit test on `World::an_storm_bolt` with BridgeStrength=1. |

### W8-06-05: Chronosphere arrival on a bridge cell ignores the layers — MINOR

| | |
|---|---|
| **Original** | [CONFIRMED-RE FUN_00717600, the Teleport-locomotor destination check]: if the destination cell has the bridge flag (+0x140 & 0x100), the occupant scan walks **AltObject (+0xE8)**, the deck list; otherwise FirstObject. The arriving unit is placed on the deck, and only deck occupants are telefragged. FUN_00717B40, the landing: the "water → sink" branch is skipped on a bridge cell, so a vehicle chronoshifted onto an intact deck over water lands on the deck alive. |
| **Clone** | <ul><li>`world_sw.cpp:503-516` collects telefrag victims by `cell_x/cell_y` only, so a ship or swimmer **under** the deck dies when a tank lands on the deck.</li><li>`:534-548` never sets `u.on_deck`. `sync_deck_layer` (`world_depth.cpp:835`) forces deck only for a deck over water. On a bridge over land (deck value 1) the unit keeps its old layer, so a unit chronoshifted from open ground lands **under** the bridge. An amphibious unit (deck_amph value 1) also stays at its origin layer.</li><li>A landing on an intact water deck works: `passable_` is 1 and `sync_deck_layer` sets on_deck 1, which is correct.</li></ul> |
| **Fix** | In `an_chrono`, when `high_bridge_cell(lx,ly) && !high_bridge_destroyed(lx,ly)`, set `u.on_deck = 1` and restrict the frag/curtain scan to units with `on_deck == 1`. Otherwise restrict it to `on_deck == 0`. |
| **Test** | A unit test: a ship under deck cell C, then a tank chronoshifted onto C. The ship lives and the tank has `on_deck == 1`. |

### W8-06-06: The Chronosphere does not eject a riding parasite — MINOR

| | |
|---|---|
| **Original** | The Chronosphere gives every moved UnitClass the piggyback Teleport locomotor (0x6CBF1D-0x6CC226, W7-08-07). That locomotor's teleport step calls ParasiteClass::ExitUnit (0x7188D9), the same call the clone cites for CLEG / Chrono-tank self-teleport (`world_abilities.cpp:180-183`). |
| **Clone** | `an_chrono` (`world_sw.cpp:438-551`) relocates the host with `parasite_on_me` intact. The Terror Drone or squid travels with it and keeps chewing at the destination. |
| **Fix** | Call `parasite_exit(i, /*suppressed=*/false)` before relocating each mover, as `world_abilities.cpp:183` does. |
| **Test** | Infest a Grizzly with a drone and chronoshift it. Expect `parasited == 0`, and the drone left at the source, paralysed. |

### W8-06-07: Garrison exit cells are not bounded to the building's perimeter — MINOR

| | |
|---|---|
| **Original** | [CONFIRMED-RE BuildingClass::UnloadOccupants FUN_00457DD0]: <ul><li>It walks the four edges of the foundation's surrounding ring, testing each cell with the first occupant's vt+0x1AC (CanEnterCell: water and cliff faces are refused for Foot), and takes the first enterable cell.</li><li>Every occupant Unlimbos there (vt+0xD8). A failed Unlimbo means UnInit.</li><li>If no ring cell is enterable, it calls **KillOccupants** (FUN_004585B0) unless the caller flag is set.</li></ul> |
| **Clone** | `world_depth.cpp:195-198` calls `nearest_free_cells(corner, n, passable_, …)` with no distance bound and no height test. Occupants of a building ringed by water, cliff or other units are placed on cells further out: across a river, or on a different level below a cliff, possibly a deck cell (`passable_` includes intact water decks). They should die. |
| **Fix** | Restrict the search to the ring `cx-1..cx+fw, cy-1..cy+fh`, at the building's level (height gate), and kill the occupants when the ring is full. |
| **Test** | A unit test: a garrisoned building whose ring is all water or all occupied. The occupants die on unload and are not teleported. |

## Verified OK

The tags state the evidence for each item.

- **Save/load with units on a high-bridge deck, swimming under it, and Iron-Curtained** [CONFIRMED-PLAY run_a]: after the load, `udeck(0,E1,*)=1`, `udeck(0,MTNK,0)=1`, `udeck(0,GHOST,0)=0`, `iron_units(0)=1`, the same cell and 3 infantry in one cell. `on_deck` (v30), `iron_left_frames`, `chrono_left_frames`, `frozen`, `warp_remaining`, `hbridge_` and `para_alt` are all serialised (`save_fields.hpp:414-423, 1147-1257`). Only the parachute sentinel breaks the load (W8-06-01).
- **Iron-Curtained vehicle on a collapsing deck dies** [CONFIRMED-PLAY run_a]. The deck kill uses `ability_kill_unit`, which ignores the curtain, consistent with the original's ignoreDefenses / drop.
- **Iron Curtain kills infantry, including swimmers, and skips airborne aircraft and chrono-warping units** (`world_sw.cpp:395-420`, already the W7-08-05 fix).
- **Nuke does not break bridges** [CONFIRMED-DATA `[NUKE]` has no `Wall=`; RE bridge block requires +0x144].
- **Mirage disguise on decks and beaches:** the original UpdateDisguise FUN_007456F0 has no terrain or layer gate on disguising [CONFIRMED-RE], and neither does the clone (`world.cpp:2915-2965`). Its 8-neighbour infantry scan uses a per-layer list in the original (FUN_0047EB50(alt), alt = FUN_00486660 tile-set test). The difference is marginal and not reported.
- **Veteran / SelfHealing heal while swimming:** no water or layer gate in the clone (`world_veterancy.cpp:221-229`); garrisoned and carried units are skipped, as the original's limbo early-out.
- **Gap Generator shroud is terrain-agnostic** (`world_w5.cpp:337-370`).
- **Terror Drone on a host at sea:**
  - Host death with no land cell within 1 means the drone is lost (`world_abilities.cpp:691-716`).
  - Iron Curtain and depot ejects destroy it via suppression (`world.cpp:2854-2870`).
  - A Sonic or heal exit uses the parasite's own domain grid.
- **SEAL cannot infiltrate or disguise** [CONFIRMED-DATA: `[GHOST]` has neither Infiltrate= nor CanDisguise=]. The spy (MovementZone=Infantry) cannot reach water; N-A.
- **Radiation is cell-based** in both games, so deck and under-deck units in an irradiated cell are both affected. No layer gap.
- **Chronosphere landing on an intact high-bridge deck over water:** the unit is not sunk and ends `on_deck 1` (the `passable_` deck cell plus `sync_deck_layer` value 2), as FUN_00717B40 skips the sink on a bridge cell.

## Not exercised

- Engineer capture after an amphibious-transport unload (unload on land only is W6-verified).
- Yuri Prime deploying while swimming (YURIPR: SpeedType=Amphibious, Deployer=yes; no RE taken of the swimming-deploy gate).
- A mid-chronoshift save in play (code-verified only).
