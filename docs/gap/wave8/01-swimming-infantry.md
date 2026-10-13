# W8 interaction audit 01: swimming infantry × terrain/layer × state

Auditor: w8int-01. Clone: D:\ra2-android at master 34cb652, build\ra2engine.exe. Binary: gamemd.exe (YR), Ghidra on a private copy of the `bridge` project (deleted after the audit).

Evidence:
- Scripts are in `D:\ra2-re\w8int-01-scripts\play\` (t0..t6).
- Outputs, play.log and shots are in `D:\ra2-re\w8int-01-out\<script>\`.
- Decompiles are `D:\ra2-re\w8int-01-out\d1..d8.txt` and `v1.txt` (vtables).

## Summary

The unit family is every InfantryType whose art has a Swim sequence or whose MovementZone is amphibious. In YR rulesmd/artmd that is:

| Type | Image | MovementZone / SpeedType | Swim / Tread / WetAttack / WetDie1 | Enter/LeaveWaterSound | Crushable | NavalTargeting |
|---|---|---|---|---|---|---|
| GHOST (Navy SEAL) | SEAL | AmphibiousDestroyer / Amphibious | 388 / 340 / 436 / 514 | TanyaEntersWater / TanyaLeavesWater | yes | 4 |
| TANY (Tanya) | TANY | AmphibiousDestroyer / Amphibious | 506 / 410 / 554 / 322 | TanyaEntersWater / TanyaLeavesWater | no | 4 |
| YURIPR (Yuri Prime) | YURIX | AmphibiousDestroyer / Amphibious | 8 / 0 / 169 / 134 | none | no | default |

- **DNOB** (the "Bront" animal) is an InfantryType with MovementZone=Water and SpeedType=Float, but it has no Swim set and no stock map uses it. It is N-A.
- **JUMPJET and LUNR** fly. They are N-A.

The wet-set rules of the original are as follows [CONFIRMED-RE 0x51D4C0, PlayAnim]:
- The substitution runs only for MovementZone 3 (AmphibiousDestroyer).
- The cell's LandType (+0xEC) must be Water (2) or Beach (6), and the soldier must not be OnBridge (+0x8C).
- The substitutions are:
  - Walk/Crawl → Swim
  - Ready/Prone → Tread
  - Idle1/2 → WetIdle1/2
  - Die1/2 → WetDie1/2
  - FireUp/FireProne → WetAttack
- On the dry/wet edge (+0x6E8), PlayAnim itself plays the enter/leave sound.

**Result.** The wet animation, Beach, the deck (after 3a7c551), cliffs, rocks, naval targeting, transports and the Chronosphere all behave as in the original. The gaps cluster where the bridge layers meet the water. There are **7 gaps**: 4 major, 2 minor and 1 cosmetic.

1. **W8-01-01 (major).** A collapsing high bridge kills only ONE infantry per deck cell. The other soldiers on that cell are left standing in the river [PLAY].
2. **W8-01-02 (minor).** Swimmers (and ships) UNDER a collapsing high bridge survive. The original kills everything in the under-deck chain [RE 0x47DC80].
3. **W8-01-03 (minor).** Low-bridge collapse uses the LAND grid as its kill rule. A Tanya, SEAL or Robot Tank on a low bridge over water is killed; the original kills only objects that cannot enter the new cell [RE 0x487920].
4. **W8-01-04 (major).** An intact low bridge keeps the tile's LandType instead of the overlay's Road. On a bridge over Beach, Tanya treads and plays the water sounds (the user's bridge-swim bug, low-bridge variant) [PLAY]. On a bridge over Water, every unit on it counts as a naval target [RE 0x47D1C0].
5. **W8-01-05 (major).** The deck and the water under it share one occupancy layer:
   - Tanks parked on a deck block a SEAL swimming under it [PLAY].
   - Infantry spots are shared across the two layers.
   - A crusher on a deck can crush under-deck infantry [RE 0x7404B0].
6. **W8-01-06 (major, every ground unit).** A low-bridge section destroyed at runtime over Beach becomes Foot-passable: `world_depth.cpp` `blocks_ground` lacks Beach [PLAY, TRN02].
7. **W8-01-07 (cosmetic).** Boarding a transport from water plays LeaveWaterSound at the moment of boarding. The original's edge lives in PlayAnim, so it fires on the first animation after unloading [PLAY + RE].

Known wave-7 items that are not re-reported:
- **Already fixed** (verified here): W7-12-09 (Beach wet set), W7-01-12 (the SEAL's water sounds), W7-06-02 (naval target is decided by the cell) and W7-11-01 (swim speed).
- **Still owned by wave 7:** W7-12-35 (WetDie corpse), W7-01-43(b) (the IsABomb deep-water splash), W7-12-14 and W7-11-05.

## Matrix

Legend: OK = matches the original; GAP = see W8-01-NN; N-A = the state cannot occur. Tags: P = CONFIRMED-PLAY, R = CONFIRMED-RE, D = CONFIRMED-DATA.

| Terrain / layer | Reach | Anim idle / move / attack | Shot: prone, fear | Death (WetDie vs Die) | Crush | Targeted by naval / land | Enter/leave sound | Draw layer | Transport in / out | Chrono | Paradrop |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Clear / Road | OK | OK Ready/Walk/FireUp | OK Down→Prone | OK Die | OK (SEAL yes, TANY/YURIPR no; D) | OK land only (LandTargeting=1 refused, R 0x6FBD7B) | OK none | OK | OK unload on land (P t6) | OK dies (Organic, R 0x6CBDF0) | OK land cells |
| Beach (LandType 6) | OK amphib only, Foot refused (P t3, TRN02 sandbar) | OK Tread/Swim/WetAttack (P t3/t3b, W7-12-09 fixed) | OK Down then Tread (R 0x51FDE0: no water test) | OK WetDie | OK (no water test in Crush, R 0x7404B0) | OK naval cell (R 0x6FBC86) | OK enter/leave (P t3: SEAL too) | OK | OK | OK dies | N-A (lands on walkable land; original not traced) |
| Water (LandType 2; "shallow/deep" is not a LandType distinction) | OK | OK Tread/Swim/WetAttack (P t1) | OK as Beach | OK WetDie (corpse = W7-12-35, splash = W7-01-43b) | OK as Beach | OK naval: DEST primary, SUB/DLPH ok, SQD secondary vs Organic, AEGIS(6) refused | OK | OK | OK boards an LCRF afloat (P t6); no unload at sea (P t6) | OK dies | N-A |
| Rock-in-water sub-tiles (TMP byte 7/8/15 → Rock) | OK refused (all 590 ALL01 Rock cells amph=0; move-bridge-cliff (57,58)) | N-A | N-A | N-A | N-A | N-A | N-A | N-A | N-A | N-A | N-A |
| High-bridge deck, intact | OK (R-210, P t1 `udeck`=1) | OK Walk on the deck (3a7c551; P move-bridge-cliff §3) | OK dry | OK Die (dry) | **GAP 05** (cross-layer) | OK not naval (OnBridge, R 0x6FBC86) | OK none on the deck | OK deck layer (pres_layers.inc) | N-A | OK dies | N-A |
| High-bridge deck, collapsing with the unit on it | — | — | — | **GAP 01** (only 1 per cell dies; the rest stand in the river) | — | — | — | — | — | — | — |
| Under a high bridge (swimming) | OK passes when the deck is empty (P t1) / **GAP 05** when it is occupied (P t4) | OK Tread/Swim (P t1 `iseq(Tread)`) | OK | **GAP 02** survives a collapse | **GAP 05** | OK naval (on_deck=0) | OK no edge under the deck | OK drawn under the deck (shot t1 under_deck) | N-A | OK | N-A |
| Low bridge, intact (over Water or Beach) | OK passable | **GAP 04** Tread/WetAttack on a bridge over Beach (P t3b) | GAP 04 (Tread) | GAP 04 (WetDie) | OK | **GAP 04** naval cell over Water (R + code) | **GAP 04** enter/leave (P t3b) | OK | OK | OK | N-A |
| Low bridge, collapsing with the unit on it | OK Tanya survives over Beach (P t3b) | OK | — | **GAP 03** over Water: the clone kills an amphibious unit; **GAP 06** over Beach: Foot survives and can walk | — | — | — | — | — | — | — |
| Cliff top / foot / ramp | OK no 4-level drop, uses the stairs (P move-bridge-cliff §1, `cliff_steps==0`) | OK | OK | OK | OK | OK | OK | OK | N-A | OK | N-A |
| Shore transition (enter/leave water) | OK | OK Swim↔Walk | OK prone kept (Crawl↔Swim) | OK | OK | OK | OK walking (P t1/t3); **GAP 07** via a transport | OK | **GAP 07** | — | — |

## Gaps

### W8-01-01: a collapsing high bridge kills only the first infantry of each deck cell (MAJOR)

**Evidence: [CONFIRMED-PLAY t1, ALL01T east bridge (82,51..55)].**
1. After the hut repair, four GIs stand on the deck at (82,52)/(82,53), with `udeck`=1.
2. `cheat bridge_hit 82 53 100000` breaks the span: `bridge_destroyed` goes 4→9.
3. `units(0,E1)` drops only 15→14. Three GIs remain at (82,52)/(82,53) with `udeck`=0, standing on water cells in the Ready pose (`t1/after_break.png`).

**Original.** Destroying a high section [CONFIRMED-RE 0x5766D0 / 0x570FC0] calls FUN_0047DC80 on each cell of the 3-cell cross-section [CONFIRMED-RE 0x47DC80]:
- Every object in the AltObject (deck) chain (+0xE8) gets vt+0xEC = 0x5F3970. That sets IsFallingDown (+0x8D) and IsABomb (+0x8F) and clears OnBridge (+0x8C).
- On touchdown, ObjectClass 0x5F3680 deals `ReceiveDamage(Health, 0, Rules+0xFA8, 0, ignoreDefenses=1, …)` to any IsABomb object [CONFIRMED-RE 0x5F3680 @L80-83].
- So **every** object on the deck falls and dies, Tanya included.

**Clone.** `engine/world_depth.cpp:581-590` (`break_high_bridge`) kills `occ_.at(c.x, c.y)`. That is the vehicle, else only the FIRST infantry spot (`engine/occupancy.hpp:102-110`). The same single-occupant pattern is in `set_bridge_cell` (`world_depth.cpp:448-454`).

**Root cause.** The whole-cell occupancy API is used where every occupant is meant.

**Fix.**
- Iterate every unit whose cell is the deck cell and that has `on_deck` set. Use all three spots plus the owner, or scan `units_` the way `an_chrono` does.
- Optional: model the fall (+0x8D) for the visuals.

**Test.** World unit test: three infantry on one intact deck cell, then `break_high_bridge`. All three must be dead. Play: t1, asserting `units(0,E1)` drops by the number of GIs that had `udeck==1`.

### W8-01-02: units under a collapsing high bridge survive (MINOR)

**Evidence.**
- [CONFIRMED-RE 0x47DC80]: the first loop walks the FirstObject chain (+0xE4) and calls `vt+0x16C ReceiveDamage(&obj->Health, 0, Rules+0xFA8, 0, 1, 1, 0)`. That is lethal and ignores defences, for everything in the ground layer under the span.
  - FirstObject = ground layer and AltObject = deck layer, as the layer pick in UnitClass::Crush shows [CONFIRMED-RE 0x7404B0: crusher on a bridge → +0xE8].
  - This matches YRpp `CellClass::GetContent()`.
- [CONFIRMED-PLAY t1]: GHOST and TANY tread at (82,52) under the deck (`udeck`=0, `iseq(Tread)=2`). After the break both are alive with full HP (`uhp` 125 / 250, `units(0,GHOST)=1`, `units(0,TANY)=2`).

**Clone.** `world_depth.cpp:581-584`: the comment "an amphibious unit swimming under it lives" is the deliberate opposite of the original. A ship under the span also survives.

**Fix.** In `break_high_bridge`, also kill every non-airborne unit with `on_deck==0` on the cell, ships included, with the C4Warhead / ignoreDefenses semantics. This applies to the cross-section (the deck cell ± the BridgeBody side cells, if they are ever modelled).

**Test.** World test: a SEAL and a DEST under a deck cell, then break. Both die. Play: t1 assertion `units(0,GHOST)==0` after the hit.

### W8-01-03: low-bridge collapse kills by the land grid, not by "cannot enter this cell" (MINOR)

**Evidence.** [CONFIRMED-RE 0x57B720 → FUN_00487920 on the three rewritten cells (destroy branch, param 1)].
- 0x487920 walks the cell's objects. It deals lethal C4Warhead damage when `vt+0x1AC CanEnterCell(cell) == 7 (Move::No)`. With param 1 it also kills landed aircraft and JumpJet types.
- It also kills objects within ±2 cells whose locomotor is heading into the cell, under the same test.
- So a Tanya, SEAL, Yuri Prime or Robot Tank on a section that collapses into Water **survives**.

**Clone.** `world_depth.cpp:448-454` kills `occ_.at(x,y)` when `!is_naval && !is_aircraft && passable_[i]==0`. That is the land grid, so an amphibious non-naval unit over Water dies. Only the first infantry is considered (as in W8-01-01), and units heading into the cell are ignored.

**Not reproduced in play.** None of the maps tested has a low bridge over Water; TRN02's bridge is over Beach. That case is covered by W8-01-06. The claim rests on the RE contradiction.

**Fix.**
- Replace the predicate with `!unit_can_enter_cell(u, x, y)` for every occupant.
- Add the ±2 incoming-mover pass.

**Test.** World test: a low-bridge section over Water with a TANY and a GI on it, then destroy. The GI dies and Tanya lives, with WetDie not played.

### W8-01-04: an intact low-bridge cell keeps the tile's LandType instead of the overlay's Road (MAJOR)

**Evidence.**
- [CONFIRMED-RE 0x47D1C0, CellClass::RecalcAttributes L39-43]: with an overlay (+0x44 ≠ -1), `LandType = OverlayType.Land (+0x298)`. It returns early when Land is Wall/Railroad or `NoUseTileLandType` (+0x2AC) is set.
  - Intact LOBRDG/LOBRDB pieces have `Land=Road`, `NoUseTileLandType=true` [CONFIRMED-DATA rulesmd], so the cell is **Road**.
- [CONFIRMED-PLAY t3b, TRN02 bridge (54..56,41..46); row 43 is intact overlay 90 over a Beach tile]:
  1. Tanya stands on (55,43) with `uland(0,TANY,0)=6` and `iseq(Tread)=1`. At L18 she fired from the bridge with `iseq(WetAttack)=1`.
  2. `pres(enter_water)` 0→1 when she stepped onto the bridge, and `pres(leave_water)` 0→1 when she stepped off to (55,42).
  - Original: Road, so Ready/FireUp and no sounds. This is the user's "Tanya swims on a bridge" bug, low-bridge variant.
- [CONFIRMED-RE + code] Over a Water tile, `an_victim_naval_cell` (`world_an.cpp:78-87`) reads the same `speed_land_at` and returns true. Every unit on an intact low bridge over water is a naval-cell target:
  - SUB, DLPH, SQD and AEGIS, which have LandTargeting=1 and must be refused on land, can attack GIs and tanks on the bridge.
  - The NavalTargeting slot switch applies; for example, a SEAL picks Sapper against a tank on the bridge instead of its MP5.

**Clone.**
- `world.cpp:691-697` `speed_land_at` returns `speed_land_[i]`, the tile type. `set_bridge_cell` (`world_depth.cpp:438-446`) updates only `passable_` and `water_passable_`.
- Consumers that break:
  - the wet test, `world.cpp:6031`;
  - the sound edge, `pres_audio_events.inc:621-625`;
  - `an_victim_naval_cell`;
  - the `uland` probe.

**Root cause.** The bridge overlay's land type is not part of the clone's per-cell LandType.

**Fix.** In `speed_land_at` (or in a `speed_land_` patch done by `set_bridge_cell`), return Road (1) for an intact or damaged low-bridge overlay and the tile type for a destroyed one. Mirror 0x47D1C0's Wall/Railroad overlays too.

**Test.**
- t3b: assert `uland(0,TANY,0)==1`, `iseq(Tread)==0` and `pres(enter_water)` unchanged on (55,43).
- World test: a SUB with LandTargeting=1 cannot target a GI on an intact low bridge over water.

### W8-01-05: the deck and the water under it share one occupancy layer (MAJOR)

**Evidence.**
- [CONFIRMED-PLAY t4, ALL01T]:
  1. Three Grizzlies stand on the deck at (82,51), (82,52) and (82,53), all with `udeck`=1.
  2. A SEAL ordered from (78,52) to (88,52) along the river under the deck stops at (81,52): `umov=1`, `uwait`=49 s, and it never reaches (88,52) within 600 frames (`t4/seal_vs_tanks.png`).
  3. After the tanks leave the deck, the same SEAL crosses (L40 `ucell=88052`).
- [CONFIRMED-PLAY t1]: one GI on the deck plus GHOST and TANY under it filled the three spots of (82,52). The cell's spot pool is shared across the layers.
- [CONFIRMED-RE] The original keeps two content chains, FirstObject +0xE4 and AltObject +0xE8, and two occupation words, +0x124 and +0x128.
  - `IsClearToMove` reads the bridge word for a deck mover (movement-height.md §2).
  - `UnitClass::Crush` 0x7404B0 crushes only the chain of the crusher's own layer (OnBridge, or the previous cell 4 levels up → +0xE8, else +0xE4). The incoming warning uses +0x128 / +0x124 respectively.

**Clone.**
- `engine/occupancy.hpp` has one `owner_` / `spots_` grid.
- `world_loco.cpp:257-283`: the crusher's `reserve_cell` takes victims from `occ.at` plus the spots with no layer test. A crusher on the deck will squash a crushable SEAL under it, and the reverse.
- The crush across layers was not isolated in play: the enemy SEAL in t4 and t5 was shot by the fort first. It rests on the RE contradiction.

**Root cause.** The single-layer occupancy predates R-210.

**Fix.** Add a second occupancy plane for deck cells, keyed by `on_deck`, used by `can_enter`, `reserve_cell`, scatter and the crush victim list. As a cheaper stop-gap, ignore occupants whose `on_deck` differs from the mover's layer, both in the step test and in crush.

**Test.** World test: a vehicle on a deck cell, then an amphibious infantry path under it. The infantry passes, and a crusher on the deck leaves the under-deck SEAL alive. Play: t4, asserting `ucell(0,GHOST,0)==88052` within 600 frames.

### W8-01-06: a low-bridge section destroyed at runtime over Beach becomes passable to Foot, Track and Wheel (MAJOR; every ground unit)

**Evidence: [CONFIRMED-PLAY t3c, TRN02].**
- Before the hit, row 44, which was destroyed at load (overlay 101), reads `walk=0` over Beach.
- After `cheat bridge_hit 55 43 100000`, row 43 is overlay 101 over Beach too, but reads `walk=1`, `wat=0` (`trn02_after_hit.txt`).
- A GI standing on it is not killed, and GIs can now walk across the broken span.

**Original.** A destroyed piece has `NoUseTileLandType=false`, so the tile's LandType (Beach) applies. Beach is Foot/Track/Wheel 0% [CONFIRMED-DATA [Beach]]. 0x487920 then kills the ground units on it.

**Clone.**
- `world_depth.cpp:27-30` `blocks_ground` returns Water/Rock/Wall only. Its comment says "== world.cpp land_type_blocks_ground".
- But `world.cpp:1195-1203` (fv7c) also blocks Beach. The runtime path (`set_bridge_cell`) therefore disagrees with the load path.

**Fix.** Make `blocks_ground` call or alias `land_type_blocks_ground`, Beach included. With W8-01-03, kill by "cannot enter".

**Test.** World test: a low-bridge section over a Beach tile destroyed at runtime has `passable_==0`. Play: the t3c assertion `walk==0` on (55,43) after the hit.

### W8-01-07: boarding a transport from water plays LeaveWaterSound at boarding (COSMETIC)

**Evidence.**
- [CONFIRMED-PLAY t6, xbayopigs]: a SEAL and a Tanya tread at (172,162/163), then each boards an LCRF afloat at (174,165). At boarding `pres(leave_water)=2`, and `cargo_total(0)=2` with the LCRF still on water.
- [CONFIRMED-RE 0x51D4C0]: the enter/leave sound is played only inside PlayAnim, on the +0x6E8 edge. A passenger in limbo calls no PlayAnim, so its flag keeps "wet". The sound plays on the first PlayAnim after it is unloaded on land, at the unload point.

**Clone.** `pres_audio_events.inc:469-476` uses `w6_wet`, which is false for `cargo_of >= 0`, so the edge fires the moment it boards.

**Fix.**
- Keep the edge state on the soldier and evaluate it only while it is out of cargo: skip the update when `cargo_of >= 0`.
- Equivalently, drive the sound from the sequence machine's wet substitution, as the original does.

**Test.** Play t6: `pres(leave_water)` is unchanged after boarding and +1 after the unload on land.

## Verified OK

- **Family and data.** GHOST, TANY and YURIPR are the swimmers [CONFIRMED-DATA rulesmd/artmd].
  - The SEAL's EnterWaterSound now plays: `pres(enter_water)` +1 when the SEAL walked onto the TRN02 sandbar, and `leave_water` +1 when it walked off [CONFIRMED-PLAY t3]. W7-01-12 is fixed.
  - YURIPR has no water sounds in the data, and the clone stays silent.
- **Wet set.** The wet substitution table and the MovementZone gate match 0x51D4C0, with Beach included (W7-12-09 fixed).
  - Panic is suppressed in water, because Walk is already Swim when the fear test runs; this matches.
  - Die3-5 are not remapped, in both.
- **The deck.** On an intact high deck the soldier walks and plays no sounds (R-210 / 3a7c551, OnBridge exclusion as at 0x51D4C0 +0x8C) [CONFIRMED-PLAY move-bridge-cliff §3].
- **Going prone in water.** The original requests Down with no water test [CONFIRMED-RE 0x51FDE0] and PlayAnim does not remap Down, so a SEAL hit in water plays Down, then Tread. The clone is the same.
- **Speed.** There is no terrain or water speed term for Walk (W7-11 RE). The clone's `land_speed_mult` excludes infantry (`world_loco.cpp:144-150`).
- **Reachability.**
  - Rock-in-water sub-tiles are refused: every Rock cell on ALL01 has amph=0 (`all01_terrain.txt`).
  - Beach is amphibious-only.
  - The cliff gate holds (move-bridge-cliff §1).
  - Swimming under an empty deck works [CONFIRMED-PLAY t1: the SEAL went (78,52)→(88,52) with `udeck`=0 and Tread].
- **Naval targeting by cell.** Water or Beach, not on a bridge, not airborne (0x6FBC86 / 0x6F2B9F); `naval_cell` respects `on_deck`. The per-NavalTargeting slots are 0-6.
  - SUB, DLPH and SQD can attack a swimmer, and AEGIS (6) cannot.
- **Transports and the Chronosphere.**
  - A SEAL and a Tanya board an LCRF afloat [CONFIRMED-PLAY t6].
  - Unload is refused at sea and works on land (they appear at (178,165)/(178,167)).
  - Chronosphere: infantry are Organic by default, so they die at the source (0x6CBDF0 per `world_sw.cpp:456-461`). The destination's water, deck or cliff is moot.
- **Drawing.** Units under a deck are drawn under it (pres_layers.inc deck bias; `t1/under_deck.png`).
- **Crush.** In open water, a Crusher (SAPC) against a swimming SEAL: neither UnitClass::Crush 0x7404B0 nor PerCellProcess 0x739AC0 has a water test, so the clone's crush in water is consistent. Whether the Hover locomotor reaches Crush was not traced, so this is [SUSPECTED] consistent.
- **Not verified.** Paradrop onto water: the clone lands troopers only on walkable land, and the original's landing-cell choice was not traced. No finding is claimed.
