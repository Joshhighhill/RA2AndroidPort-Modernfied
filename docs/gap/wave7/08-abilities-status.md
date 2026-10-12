# Wave 7 / 08: Special abilities and status effects (cross-unit mechanics)

- **Tree:** master `b3a509f`. Engine: `D:\ra2-android\build\ra2engine.exe` (not rebuilt).
- **Original:** retail YR `gamemd.exe` (image base 0x400000), analysed in Ghidra 11.2.1 on four private project copies. The decompiles stay in `D:\ra2-re\w7-abs-{A,B,C,D}-out\`.
- **Data:** `rulesmd.ini` / `artmd.ini` of this install (`D:\ra2-re\out\shroud_evidence\rulesmd.ini`).
- **Play evidence:** `08-evidence/*.play`, each with a `*-result.txt`. Every script is a skirmish at speed=2 (one driver frame = one logic frame), Easy AI, units=0.
- **Scope:**
  - deploy modes;
  - mind control;
  - chrono (Legionnaire, Chronosphere, Chrono Miner);
  - radiation;
  - disguise and detection;
  - IFV and Battle Fortress;
  - garrisons;
  - engineer capture and bridges;
  - spy infiltration;
  - parasites;
  - the Iron Curtain;
  - invulnerability, cloak and sensors;
  - self-heal, repair depots, Hospital and Machine Shop;
  - veteran effects of abilities.

  Yuri's side is a non-goal (U-5). Yuri-only content is noted only where the RA2 campaign can reach it.
- **De-duplication:** items already filed in wave 7 are cross-referenced and not renumbered:
  - W7-01-02/03/09/23/25/29/31/41/42;
  - W7-02-02/03/04;
  - W7-03-04/05/13;
  - W7-05-13;
  - W7-06-04/07/11/13;
  - W7-07-10;
  - W6-04-17.

Tags: [CONFIRMED-RE addr] = disassembled in gamemd.exe; [CONFIRMED-DATA] = rules/art; [CONFIRMED-PLAY] = measured with `--play`; [CONFIRMED-CODE] = read in the clone; [SUSPECTED] = not proven.

## Summary

The core numeric models are faithful, and play confirms them:
- radiation: creation, accumulation, falloff, the 16-frame application, Verses and decay;
- the Terror Drone's instant infantry kill;
- IFV modes;
- self-heal;
- Hospital and Machine Shop;
- the four spy infiltration effects;
- the Chronosphere's organic kill;
- the Iron Curtain's infantry kill and 750-frame invulnerability.

What is wrong is mostly the **status-effect interlocks** and a few **orders that cannot be given**:

1. **No way to send a vehicle to a Service Depot or Tech Outpost.** Clicking the depot with a damaged tank selected just selects the depot (W7-08-01, MAJOR, [CONFIRMED-PLAY]). Servicing itself works once the tank is parked next to it.
2. **The Iron Curtain protects only against damage.** In the clone an iron-curtained unit can still be erased by a Chrono Legionnaire, infested by a Terror Drone, crushed, mind-controlled, and moved by the Chronosphere. In the original every one of these is refused (W7-08-04, MAJOR, [CONFIRMED-RE 0x71A20A, 0x6FB5D0, 0x5F651E, 0x471C44, 0x6CBE61]).
3. **Garrisons never evacuate at ConditionRed.** The original walks all occupants out when the building drops to 25 % and refuses new occupants below it (W7-08-09, MAJOR, [CONFIRMED-RE 0x44012F → 0x4581F0 → 0x457DD0, 0x457CE0]).
4. **The garrison-clearing myth is closed.** No warhead flag (Fire, Radiation, Poison, PsychicDamage, Bullets) kills occupants in YR. Occupants leave only at ConditionRed, on building death or sale, through an Assaulter (no stock unit has `Assaulter=yes`), or by a Chrono erase (W7-01-29). The Assaulter half of R-053 is therefore moot for stock data (W7-08-10).
5. **Mind control (campaign only) lets a second controller steal an already-controlled unit.** The original refuses it (W7-08-11, MAJOR within the campaign).
6. **Iron Curtain vs C4: W7-01-23 needs a correction.** `BuildingClass::IronCurtain` **defuses** an armed C4 (W7-08-06, [CONFIRMED-RE 0x457C90]). Swapping the blast check, as W7-01-23 proposes, would let C4 kill a curtained building.
7. **Minor gaps:**
   - Chronosphere telefrag and sink rules (W7-08-07/08);
   - depot drone handling and phase (W7-08-02/03);
   - mind-control release paths and the AI "fate" roll (W7-08-12);
   - the deployed Yuri's PsiWave does nothing (W7-08-13);
   - organic vehicles get the Machine Shop rate (W7-08-16);
   - sensor disc boundary (W7-08-17);
   - drone tick uses the handicapped ROF (W7-08-18);
   - engineer bridge hut and neutral repair (W7-08-19/20);
   - infiltration veterancy applies only at factory exit (W7-08-21);
   - Desolator eruption ignores VeteranROF (W7-08-22);
   - Battle Fortress approach range and building fire (W7-08-23/24).

## Per-mechanic table

| Mechanic | Original (evidence) | Clone | Verdict |
|---|---|---|---|
| **GI / GGI deploy** | DeployFireWeapon slot 1 (Para / MissileLauncher, elite ParaE / MissileLauncherE); `Deploy=300,15,0` 15-frame anim; a move order undeploys and walks [CONFIRMED-RE 0x713B82, CONFIRMED-DATA artmd] | Toggle, deployed weapon used (deployed GI hits at 5 cells and never moves) [CONFIRMED-PLAY c10b]; a move undeploys and walks [CONFIRMED-PLAY c9-c10]; elite deploy weapon resolves | OK, except deploy is instant (W7-01-31) and the AI never deploys (W7-01-08) |
| **Desolator deploy** | Eruption every ROF (×VeteranROF), RadLevel 500, CellSpread 10, after the 15-frame Deploy anim | Eruption on the next frame, every raw ROF 60; a move order is ignored | W7-02-03, W7-02-04, W7-01-31 (extend), **W7-08-22** |
| **Siege Chopper deploy** | IsSimpleDeployer, UnloadingClass SCHD | Not implemented | W6-04-17 (open) |
| **MCV / ConYard** | mcv-deploy.md | Matches (sub-audit D, `world_mcv.cpp:190-216`) | OK |
| **Tesla coil charging** | abilities-w5 §3 | Matches (`world_w5.cpp` step 3) | OK; no order onto the coil (W7-02-02) |
| **Mind control** | CaptureManager: nodes = weapon Damage (Yuri 1, Psychic Tower 3, Mastermind infinite); Max==1 frees the old victim; CanCapture refuses own, ImmuneToPsionics, already-controlled, curtained; FreeAll on death / UnInit / erase start / unpowered building; Free on victim boarding; no range leash; AI fate roll [CONFIRMED-RE 0x6F34A0, 0x471BB0, 0x471C80, 0x471F20, 0x472070, 0x4722E0] | Campaign only (`campaign_runtime.inc:1612-1704`); 1 slot, release on controller death, full-slot acquire gate | Yuri OK; **W7-08-11/12/13/14** |
| **ImmuneToPsionics** | CanCapture + GetFireError 5 (refused at the cursor) [CONFIRMED-RE 0x6FC05F]; PsychicDamage blocked only by ImmuneToPsionicWeapons [CONFIRMED-RE 0x7010DA] | Explicit and auto-scan refuse MC on immune targets (`campaign_runtime.inc:1661`, `combat.cpp:1184`); ImmuneToPsionicWeapons unread | OK for MC; W7-08-13 for PsiWave |
| **Chrono Legionnaire erase** | `Strength*10` minus weapon Damage per frame; target immune to all damage (+0x270) [CONFIRMED-RE 0x700F51]; cannot start on a curtained target [CONFIRMED-RE 0x71A20A]; erase kills garrison occupants | Erase timing OK; target not immune (W7-01-09); erase step vet-modified (W7-01-25); starts on curtained targets (**W7-08-04**); occupants walk out (W7-01-29) | Partly wrong |
| **Chronosphere** | 3×3; skips airborne, curtained and already-warping units; Organic && !Teleporter killed (C4Warhead, ignoreDefenses); only UnitClass gets the Teleport loco; telefrag on the exact cell; water → sink, impassable → kill [CONFIRMED-RE 0x6CBD02-0x6CC268, 0x717600, 0x717B40] | GI dies, tank and CLEG arrive and freeze ~60 frames [CONFIRMED-PLAY c2]; tank on water destroyed, hovercraft survives [CONFIRMED-PLAY c2b] | Outcome OK; **W7-08-07/08** |
| **Chrono Miner** | Teleports home | Also warps back to the field (W7-03-13 / W6-04-34) | Open (cross-ref) |
| **Radiation** | abilities.md §A: Add accumulates; falloff; decay every 90 frames; application every 16 frames on foot units only, by `[RadSite] Verses` | GI −81 / tick, IFV (light) −40 (50 %), Rhino −8..−10 (10 %); rad_total +~50853 per eruption; linear decay after the Desolator died [CONFIRMED-PLAY c3] | OK |
| **Radiation vs garrisons / buildings** | Buildings never irradiated; Radiation=yes never clears occupants [CONFIRMED-RE 0x701093] | Same (own deployed Desolator beside a garrison: occupants stay 3) [CONFIRMED-PLAY c4b] | OK |
| **Spy disguise** | PermaDisguise → side E1/E2 in the spy's **own** house colours (protects nobody); click an enemy infantry → its type and house (copies a disguised target's disguise); fooled = not allied and as-house allied with the viewer; owner and allies see the spy; DetectDisguise scanners ignore the disguise in the normal threat scan; no reveal radius, never cleared [CONFIRMED-RE 0x5224B0, 0x522430, 0x522370, 0x522520, 0x6F71D0] | Spawned spy undisguised and shot dead [CONFIRMED-PLAY c7]; clicked disguise fools Conscripts [CONFIRMED-PLAY c7b]; dog never attacks [CONFIRMED-PLAY c7b] | W7-01-02, 03, 41, 42 (+ notes in the cross-reference section) |
| **Mirage** | Fake-blink, adjacency reveal | Reveals on every shot | W7-03-04 |
| **IFV** | IFVMode → weapon index (0..17), reset when empty; elite only HoverMissileE [CONFIRMED-RE 0x745250] | Empty 25/6/50, GI 25/6/15, Engineer −50/1.8/80, CLEG 5/6/120, Desolator 175/7/30 [CONFIRMED-PLAY c9] | OK (art and sound: W7-03-05/06) |
| **Battle Fortress** | Passengers fire their own vet weapon, range +2 cells (Rules+0xF5C, IsInRange 0x6F67F9), damage ×1.2 (Rules+0xF58, Fire 0x6FD91C); the transport's range = min(own, passengers) [CONFIRMED-RE 0x700760] | Bonus and multiplier OK (`world_w5.cpp:237,264`); approach uses own range only; passengers shoot units only | **W7-08-23/24** |
| **Garrison enter / fire** | OccupyWeapon / EliteOccupyWeapon per occupant, cycling shooter, ROF / count / 1.2, damage ×1.2, range = min(W,H)/2 + 5 [CONFIRMED-RE 0x452738, 0x6FC460, 0x6F6750] | Matches (`world_depth.cpp:212-311`); 3 GIs garrison and kill a Conscript in 32 frames [CONFIRMED-PLAY c4] | OK; no targeting or building fire (W7-02-10, W7-01-38) |
| **Garrison clearing** | ConditionRed eject + entry refusal; death / sale unload; erase = KillOccupants; Assaulter KillOccupants (no stock user) [CONFIRMED-RE] | Death / sale unload only | **W7-08-09**, W7-08-10, W7-01-29 |
| **Engineer** | Capture at any HP (EngineerCaptureLevel 1.0) [CONFIRMED-RE 0x51E17C]; own / allied repair to full; tech capture; hut only when a bridge needs repair; neutral CanBeOccupied repair | Capture 241 frames walk + instant [CONFIRMED-PLAY c5b]; own ConYard 400 → 1000 [CONFIRMED-PLAY c5b] | OK; **W7-08-19/20**; W7-01-16 |
| **Spy infiltration** | First-match dispatch Radar → Power → BuildTech → SW → Storage → Factory [CONFIRMED-RE 0x4571E0] | Power blackout 997 frames, refinery −50 %, barracks / factory → veteran [CONFIRMED-PLAY c7] | OK; **W7-08-21**; W7-01-14/15/40 |
| **Terror Drone** | Infantry killed on the first tick; vehicle `50×Verses` every raw weapon ROF 60; suppression; eject on IC / repair | GI dead in < 5 frames; Grizzly 300 → 250 at once, then −50 every ~75 frames [CONFIRMED-PLAY c6b] | Mostly OK; **W7-08-18**, W7-08-02, W7-08-04 |
| **Giant Squid** | ParasitePlus grab, Paralyzes, Culling | W6 fixed; organic (curtain kills it) | OK (W7-06-02 for targeting) |
| **Iron Curtain** | Infantry always killed; organic vehicles killed; others invulnerable for 750 frames; blocks C4, Temporal, parasite, MC, crush, Magnetron, Chronosphere; ejects a riding parasite; defuses C4; ignoreDefenses passes [CONFIRMED-RE 0x522330, 0x4DE9F0, 0x70D6E0, 0x457C90, 0x700EDF] | GIs die; tank and pillbox protected ~750 frames; tank damageable afterwards [CONFIRMED-PLAY c1, c1b] | Damage OK; **W7-08-04/05/06** |
| **Force Shield** | Allied buildings in radius | Not implemented | W6-04-20 (user decision) |
| **Cloak / sensors** | Sensor disc from SensorsSight (strict `<`), per house, not shared with allies [CONFIRMED-RE 0x4DE6C0]; cloaked targetable only if sensed by the scanner's house | Per-house bits OK (`world_an.cpp:148-162`); `<=` and gated on Sensors= | **W7-08-17**; W7-06-04/13 |
| **RadarInvisible** | SHAD hidden on radar | Unread | W7-05-13 |
| **Service Depot / Tech Outpost** | Radio dock, one client, URepairRate per-depot phase, RepairStep 8 HP, cost per step, Insufficient-funds EVA, rides off the pad; vehicles only; a drone is **ejected alive** [CONFIRMED-RE 0x44B780, 0x6F4010] | Cannot be ordered [CONFIRMED-PLAY c6, c5b]; adjacent parked vehicle +8 HP every 14 frames, ~2 credits per step; infantry not healed; drone killed [CONFIRMED-PLAY c6b] | **W7-08-01/02/03** |
| **Naval Yard repair** | — | — | W7-06-07 |
| **Self-healing** | `Frame % ftol(RepairRate*900)==0` +1 HP up to full; SELF_HEAL vet ability same path [CONFIRMED-RE 0x70B300] | HARV / DESO / TANY +10 per 140 frames, heal to full [CONFIRMED-PLAY c5-c11] | OK |
| **Hospital / Machine Shop** | Hospital: infantry **and organic UnitTypes** 20 HP / 50 frames per hospital; Machine Shop: other units 5 / 75 [CONFIRMED-RE 0x6F9380]; nobody enters (no InfantryAbsorb) [CONFIRMED-DATA] | GI +20 / 50, Grizzly +5 / 75 [CONFIRMED-PLAY c5-c11] | OK; **W7-08-16** |
| **Veteran effects of abilities** | Elite ParaE / MissileLauncherE / RadBeamWeaponE / NeutronRifleE (16) / IvanBomberE; Ivan blast uses Rules IvanDamage; Desolator eruption reload via the ROF getter ×VeteranROF [CONFIRMED-DATA; RE 0x6FC5FF, SUSPECTED for the eruption] | Elite deploy weapons resolve; Ivan OK | W7-08-22; W7-01-25 |

## Gaps

### MAJOR

#### W7-08-01: A vehicle cannot be ordered to a Service Depot or the Tech Outpost

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY `c6-c8-depot-drone`, `c6b-depot-drone`]: with a damaged own Grizzly selected, the cursor over the own GADEPT reads `select_building`. A click selects the depot; the tank never moves. In run 1 the tank stayed where it was and died.</li><li>[CONFIRMED-PLAY `c5b-tech`]: the same happens on a captured CAOUTP (UnitRepair=yes).</li><li>Moved by hand to cell 76,29 next to the depot, the tank is serviced: 100 → 300 HP in ~340 frames, credits 30000 → 29950.</li></ul> |
| **Original** | A damaged own vehicle over an own `UnitRepair=yes` building gets the Enter/repair action. It drives onto the pad, docks by radio and is repaired (`BuildingClass::Mission_Repair` 0x44B780) [CONFIRMED-RE]. |
| **Clone** | No action or cursor for UnitRepair buildings. `world.cpp:2433-2482` (`depot_client`, `step_service_depots`) services any stopped own vehicle adjacent to the depot. |
| **Fix** | <ol><li>In the command/cursor resolver (`command.cpp`, `input.cpp` / `input_original.cpp`), return an Enter/Repair action for an own or allied damaged vehicle (not infantry or aircraft) over an own UnitRepair building.</li><li>Order a move to the pad cell (the foundation centre); the existing service pass picks it up.</li><li>Add the touch path.</li></ol> |

#### W7-08-04: The Iron Curtain protects only against damage; every other effect still lands

| | |
|---|---|
| **Evidence (original)** | Every call site of `IsIronCurtained` (vt+0x160) [CONFIRMED-RE]: <ul><li>Temporal fire requires `Warpable && !IsIronCurtained` (0x71A20A).</li><li>A Parasite warhead gets GetFireError 5 against a curtained target (FUN_006FB5D0).</li><li>A drone already riding is ejected (ParasiteClass AI 0x629D62; FootClass::IronCurtain 0x4DEA48 sets suppression 50 and calls ExitUnit).</li><li>CaptureManager::CanCapture refuses (0x471C44).</li><li>Crush and OmniCrush refuse (FUN_005F64C0 0x5F651E / 0x5F656B).</li><li>Magnetron is refused (0x46970B).</li><li>Chronosphere does not move it (0x6CBE61).</li><li>A computer attacker will not fire at a curtained target (0x6FB75F; the house flag is [SUSPECTED] "not human").</li></ul> |
| **Evidence (clone)** | [CONFIRMED-CODE]: <ul><li>Temporal attach (`world_abilities.cpp:368` buildings, `:428` units), parasite infect (`:405-408`), the riding-parasite pass (`:640-690`), `can_crush` (`combat.cpp:557`) and the crush kill (`world.cpp:5670`, writes hp=0), `psionic_target_ok` (`world_w6.cpp:128`) and `an_chrono` (`world_sw.cpp:411`) never read `iron_left_frames`.</li><li>[CONFIRMED-PLAY `c4b`]: an ironed Desolator stayed curtained (`pres_iron` cheat); damage gating works.</li></ul> |
| **Player-visible** | A Chrono Legionnaire erases an iron-curtained Apocalypse; a Terror Drone infests and chews it once the curtain ends (or rides through it); a Battle Fortress OmniCrusher kills curtained vehicles; a curtained group is chronoshifted. |
| **Fix** | <ol><li>Add `iron_left_frames > 0` refusals to the Temporal attach, parasite infect, `can_crush`, `psionic_target_ok` (or `poll_mind_control`) and the `an_chrono` mover collection.</li><li>In `an_iron_curtain`, eject a riding parasite with the DroneExit path and a 50-frame suppression (as the W6-04-09 heal-hit eject does).</li><li>In the AI acquire scan, skip curtained targets for computer houses.</li></ol> |

#### W7-08-09: Garrisons never evacuate at ConditionRed, and entry is not refused below it

| | |
|---|---|
| **Original** | [CONFIRMED-RE]: the per-tick damage-state update FUN_0043FAA0 @0x44012F calls FUN_004581F0. At `health ratio ≤ ConditionRed` (Rules+0x1708 = 0.25 [CONFIRMED-DATA]) it calls `UnloadOccupants` FUN_00457DD0, and every occupant walks out alive. `CanBeOccupiedBy` FUN_00457CE0 refuses entry at or below ConditionRed. (The update sits behind a BuildingType+0x634 == −1 guard whose key is unidentified [SUSPECTED].) |
| **Clone** | [CONFIRMED-CODE]: `can_occupy` (`world_depth.cpp:46-58`) has no health test. The per-tick garrison block (`world_depth.cpp:626-642`) unloads only on death or sale. Occupants stay until the building dies. |
| **Fix** | <ol><li>In the garrison tick, call `unload_occupants(bi)` when `0 < hp ≤ max_hp × ConditionRed` (read `[AudioVisual] ConditionRed`).</li><li>Add `hp > max_hp × ConditionRed` to `can_occupy` and `campaign_can_occupy`.</li><li>The owner revert (FUN_004581F0 → neutral when empty) already exists (`world_depth.cpp:161-172`).</li></ol> |

#### W7-08-11: Mind control: a second controller can steal an already-controlled unit (campaign)

| | |
|---|---|
| **Original** | `CanCapture` FUN_00471BB0 refuses a target that `IsMindControlled` (FUN_0070F980: MindControlledBy ≠ 0 or the permanent byte +0x2C4). The same test is GetFireError 5 (0x6FC05F), so the cursor refuses it too [CONFIRMED-RE]. |
| **Clone** | `campaign_runtime.inc:1666` tests only `live`, `is_enemy` and `!is_aircraft`. The player's Yuri can take a unit an enemy Yuri holds, and the enemy's later `mc_release` flips it back again [CONFIRMED-CODE]. |
| **Fix** | In `poll_mind_control`, drop the target when `utrack_[vi].mc_orig >= 0` (already controlled). Also add the check to the attack-cursor validity for MC units. |

### MINOR

#### W7-08-02: The depot kills a riding Terror Drone; the original ejects it alive

- **Original** [CONFIRMED-RE `Receive_Message` case 0x1C @0x6F4010]: on each paid repair step, a parasite riding the unit gets SuppressionTimer = {Frame, 50} and `ParasiteClass::ExitUnit` FUN_00629F40. The drone pops out alive next to the pad (agrees with abilities.md §B3).
- **Clone** [CONFIRMED-CODE `world.cpp:2465-2469`; CONFIRMED-PLAY c6b: `units(1,DRON)` 1 → 0 on arrival]: `ability_kill_unit` every frame, unphased.
- **Fix:** on a paid step only, reuse the W6-04-09 heal-hit eject (free cell, `suppress_until = F + 50`, DroneExit event).

#### W7-08-03: Service Depot details

The clone uses:
- `RepairRate` on the global Frame % 14 phase;
- service for any adjacent stopped vehicle;
- a silent wait when funds run out;
- no ride-off at the end.

The original [CONFIRMED-RE 0x44B780 @0x44BD38, 0x44BFE5, 0x44BDC0]:
- uses `URepairRate` (Rules+0x16E8, also .016) with a **per-depot** accumulator (+0x620);
- serves one radio contact;
- plays `EVA_InsufficientFunds` and drops back to state 1 when broke;
- plays `EVA_UnitRepaired` and sends the unit off the pad (vt+0x480 to a free cell) when done.

`RepairStep=8` and the step cost match [CONFIRMED-PLAY c6b]. Vehicles only [SUSPECTED for aircraft; infantry not healed CONFIRMED-PLAY c6b].

**Fix:** read URepairRate, keep a per-depot timer, add the two EVA lines (check R-131's UnitRepaired) and an eject move. This lands together with W7-08-01's dock.

#### W7-08-05: Iron Curtain edge rules

Original [CONFIRMED-RE FUN_006CBA10 case 1 @0x6CC576, 0x522330, 0x4DE9F0, 0x700EDF]:
- The SW walks the cell ground and bridge lists, so **airborne aircraft are not curtained**. It also skips objects flagged chrono-warp-pending (+0x27C).
- InfantryClass::IronCurtain kills **every** infantry with C4Warhead and ignoreDefenses=1, regardless of Organic. Organic UnitTypes (SQD, DLPH, DNOA/B) are killed with ignoreDefenses=0.
- ReceiveDamage lets ignoreDefenses hits and heals through the curtain.

Clone (`world_sw.cpp:388-409`, `combat.cpp:248`, `world_abilities.cpp:209`) [CONFIRMED-CODE]:
- has no altitude or warp-pending filter;
- tests `organic`, which defaults to infantry (`w2air_main.inc:23`), so the kill set is the same for stock data;
- blocks **all** damage under the curtain, including C4 and Chronosphere kills (which use ignoreDefenses).

**Fix:**
- In `an_iron_curtain`, skip `is_aircraft && altitude > 0` and units with `an.chrono_left_frames > 0`.
- Kill infantry unconditionally.
- Add an `ignore_defenses` path to `ability_damage_unit` / `an_damage_unit` for the C4 / Chrono kill callers.

#### W7-08-06: The Iron Curtain defuses an armed C4 (correction to W7-01-23)

- **Original** [CONFIRMED-RE FUN_00457C90 = BuildingClass::IronCurtain, decompiled in `w7-abs-A-out\b13.txt`]: `if (C4Applied +0x6DF) { C4Applied = 0; C4Timer (+0x528/+0x530) reset; }`, then `TechnoClass::IronCurtain`. A curtain applied after planting **saves** the building, and the C4 is gone.
- **Clone:** under a curtain the C4 expiry is consumed and the damage returns early (`world_abilities.cpp:775` → `:217`). The outcome is the same today. W7-01-23's fix ("swap both checks") would make C4 kill a curtained building, which is wrong.
- **Fix:** keep W7-01-23's plant-side refusal (0x51A3F0). Replace its blast-side half with `b.c4_applied = false` in `an_iron_curtain` for a curtained building.

#### W7-08-07: The Chronosphere moves iron-curtained, erased and warping units, and teleports non-Unit objects instantly

- **Original** [CONFIRMED-RE 0x6CBD02-0x6CC268]:
  - curtained objects and those with +0x27C / +0x270 / +0x271 set are skipped;
  - a bunkered unit is unbunkered;
  - only `WhatAmI == Unit` gets the piggyback Teleport locomotor;
  - Teleporter infantry and landed aircraft only get a `SetDestination` and move by their own locomotor [SUSPECTED];
  - buildings are never moved (vt+0x480 FUN_00455D40 is empty).
- **Clone** [CONFIRMED-CODE `world_sw.cpp:411-470`; CONFIRMED-PLAY c2: the CLEG relocated with the tank]: every non-airborne unit in the 3×3 is relocated at once, curtained, warping and being-erased units included.
- **Fix:**
  - skip `iron_left_frames > 0`, `warp_remaining > 0` and `chrono_left_frames > 0`;
  - for non-vehicles (Teleporter infantry, grounded aircraft), issue a normal move (a CLEG teleports by itself) instead of relocating.

#### W7-08-08: Chronosphere landing: telefrag and sink rules

- **Original** [CONFIRMED-RE FUN_00717600, FUN_00717B40, 0x718DE0]:
  - The SW path uses the **exact** offset cell (no nearest-free search).
  - A non-curtained techno already on that spot is killed (C4Warhead, ignoreDefenses). For infantry it must be the same sub-cell.
  - If the occupant is curtained, the **arriving** unit dies.
  - A vehicle landing on water that is not Hover, not Naval and not on a bridge **sinks** (IsSinking +0x3CD; passengers handled).
  - A cell impassable for its movement zone sinks it on a shore tile, otherwise kills it.
  - A ship on land is destroyed.
- **Clone** [CONFIRMED-CODE `world_sw.cpp:440-460`, RE-FALLBACK; CONFIRMED-PLAY c2b: the Grizzly on water was destroyed, hovercraft LCRF survived]: wrong domain → `an_damage_unit(hp)` (explosion, no sink); an occupied cell slides to the nearest free cell within 2.
- **Fix:** port the telefrag and curtain rule; send water landings through the existing naval sink path (Weight / sinking state) instead of the normal death.

#### W7-08-10: R-053 "Assaulter clearing" is moot for stock data; garrisons have no warhead clearing

[CONFIRMED-RE] by an exhaustive operand search in `w7-abs-A-out\b15/b16`:
- Warhead Fire (+0x14C) is read only by a fire-error helper (0x6FFDDA).
- Radiation / PsychicDamage / Poison (+0x177 / +0x178 / +0x156) are read only by ReceiveDamage immunity checks.
- Bullets (+0x17A) is never read.
- No ReceiveDamage path walks the occupant array.

An Assaulter (InfantryType +0xEB5, default 0) ordered at an enemy garrison calls `KillOccupants` FUN_004585B0: all occupants die at once, the kill is credited and OccupantAnim plays per slot, and the Assaulter stays outside. No stock unit sets it: GHOST, TANY, SHK and BORIS say `Assaulter=no` [CONFIRMED-DATA].

**Action:**
- Mark R-053's Assaulter half "no stock effect". It is optional for mod parity: model it in `World::order_occupy` for an enemy-owned occupied building.
- DistributedFire is AEGIS-only and is filed as W7-06-11.
- Nothing to implement for flame or radiation clearing.

#### W7-08-12: Mind-control release paths and the AI fate roll

Original [CONFIRMED-RE]:
- (a) `FreeAll` runs when a Temporal shot **starts** on the controller (0x71A1A0), not when the erase completes.
- (b) A victim that boards a transport or building is freed first (0x519449, 0x739216, 0x739674), so it reverts to its original owner.
- (c) After Capture and after Free, `DecideUnitFate` FUN_004722E0 runs for a computer-owned new owner. It is a weighted roll (TeamType `MindControlDecision` overrides): join the controller's team, or Hunt (mission 0xF) by default.
- (d) `SetOwningHouse` deselects a unit the player loses [UNKNOWN whether the clone filters the selection].
- (e) The victim index can be reused after its death while `mc_victim` still holds it [SUSPECTED; check `sync_tracks`].

Clone (`campaign_runtime.inc:1620-1704`): releases only on controller death; the victim just stops and guards.

**Fix:**
- `mc_release` when the controller gets `warp_remaining > 0` and when the victim enters cargo or a garrison;
- put AI-owned captured or released units on Hunt (or into the controller's team);
- clear `mc_victim` when the victim dies.

#### W7-08-13: A deployed Yuri's PsiWave does nothing; PsychicDamage and ImmuneToPsionicWeapons are unread

- **Original** [CONFIRMED-DATA, CONFIRMED-RE 0x7010DA]: YURI is Deployer + DeployFire, with Secondary=PsiWave (PsiPulse: CellSpread 3, 100 % vs infantry, PsychicDamage=yes). PsychicDamage is blocked only by `ImmuneToPsionicWeapons` (+0xD36). BuildingType defaults it to yes; BORIS sets it to no.
- **Clone** [CONFIRMED-CODE `combat.cpp:145`]: `if (u.w_mind_control) return;` drops **every** shot of a mind-control type, the deployed PsiWave included [SUSPECTED in play; campaign only].
- **Fix:**
  - gate on the active weapon slot (`u.w_mind_control && active slot is the Controller weapon`);
  - parse `ImmuneToPsionicWeapons` (buildings default yes) and skip PsychicDamage hits on immune targets.

#### W7-08-14: Mind control exists only in the campaign runtime (INFO)

- [CONFIRMED-DATA] An Allied or Soviet skirmish reaches mind control only through `[YURI] PrerequisiteOverride=CARUS03` (owning a captured Kremlin Palace). `PrerequisiteOverride` is unread (W7-01-33), so it is unreachable today.
- PTROOP needs stolen Yuri tech; YAPSYT needs Yuri prerequisites; IFV slot 9 `CRMindControl` is PsiPulse, not MC.
- Multi-node controllers (Psychic Tower 3, Mastermind infinite + overload: bucket 3 / 6 / 10 / 50 → 0 / 50 / 100 / 500 every 60 frames) and the Psychic Dominator (permanent MC, FUN_0053ADB0) are Yuri-side and unmodelled.
- NAPSYB / NAPSYA are plain buildings in YR rules (no SuperWeapon), so the campaign effect comes from triggers. That matches the clone.
- **Fix:** if the PrerequisiteOverride path is added, move `poll_mind_control` into `World` so skirmish gets it.

#### W7-08-16: Organic vehicles (Dolphin, Squid) get the Machine Shop rate instead of the Hospital rate

- **Original** [CONFIRMED-RE TechnoClass::Update FUN_006F9380]: the Hospital branch applies to infantry **or** a UnitClass whose type is Organic (+0xD97). The Machine Shop branch covers the remaining UnitClass objects, ships included.
- **Clone** (`world_w5.cpp:84-114`): split by infantry / vehicle.
- **Fix:** `infantry_like = !is_aircraft && (!is_vehicle || an.t.organic)`.

#### W7-08-17: Sensor disc boundary and source flag

- **Original** [CONFIRMED-RE FootClass AddSensors @0x4DE6C0]:
  - The sensor disc is added whenever **SensorsSight ≠ 0**, not keyed on `Sensors=yes`.
  - It covers `dx² + dy² < SensorsSight²` in integer cell offsets (strict).
  - It counts per house in `cell+0x7C[house]`, **not shared with allies**. This answers W7-06 open question 6.
  - Sonar does not reveal spies; that is a separate array.
- [SUSPECTED, 0x4D84F0]: a cloaked foot unit entering a cell next to a non-allied Sensors object is force-decloaked.
- **Clone** (`world_an.cpp:148-162`): gates on `sensors` and uses `cdist <= sensors_sight`. Boundary cells differ.
- **Fix:** use the strict integer disc keyed on SensorsSight.

#### W7-08-18: The Terror Drone damage tick uses the handicapped and veteran ROF

- [CONFIRMED-PLAY `c6b-depot-drone`]: on a Grizzly, the first tick was immediate (300 → 250), then −50 roughly every 75 frames for an Easy-AI drone.
- The original re-arms `DamageDeliveryTimer = {Frame, weapon.ROF}` = 60 (abilities.md §B2 [CONFIRMED-RE 0x629A50]).
- The clone uses `d.w_rof` (`world_abilities.cpp:676`), which `refresh_vet_stats` / `rof_frames` scale by the house ROF handicap (60 × 1.25 = 75) [CONFIRMED-CODE `world_veterancy.cpp:58`]. Whether the original applies any multiplier on this timer is [SUSPECTED no].
- Same family as W7-01-25 (erase step).
- **Fix:** keep the raw weapon ROF in the ability profile and use it for `parasite_next_tick`.

#### W7-08-19: An engineer is wasted on the repair hut of an intact bridge

- **Original** [CONFIRMED-RE 0x51E17C L60-63]: over a `BridgeRepairHut`, What_Action calls FUN_00586E40, a ±2-cell bridge scan. It returns Enter (0x1D) only when a bridge needs repair, otherwise NoEnter (0x20).
- **Clone:**
  - `command.cpp:247-250` always orders the hut;
  - `world.cpp:2646-2652` consumes the engineer;
  - the restored count from `repair_bridge_near` (`world_depth.cpp:410`) is ignored.
- **Fix:** add a pre-check (factor the scan out of `repair_bridge_near`) for the cursor and the command, and consume only when `restored > 0`.

#### W7-08-20: An engineer cannot repair a damaged neutral garrisonable building

- **Original** [CONFIRMED-RE 0x51E17C L64-77, loader 0x4600BB]: the engineer repair branch accepts `IsAlliedWith(target) || (owner is the neutral/civilian house && BuildingType.CanBeOccupied)`.
- **Clone:** `command.cpp:249` drops such targets; `repair_building` (`world.cpp:2206`) requires `are_allied`.
- **Fix:** allow both for a neutral `can_be_occupied` building.
- **Doc note:** capture.md §A5's "action 3" is the injured-unit Hospital entry (`Hospital=` key, BuildingType+0x16C1, loader 0x460AD1). Engineer repair is 0x1D. Add this to W7-01-45.

#### W7-08-21: Infiltration veterancy is granted only at the factory exit

- **Original** [CONFIRMED-RE 0x517AE0 @0x517B4A, 0x745640 @0x74569A, 0x734490 @0x734717, 0x442BC0 @0x442C60]: the house flags +0x2BF / +0x2C0 are tested in the **Init** of every infantry (Trainable), non-Naval Trainable unit and MCV-deployed ConYard the house creates. That covers paradrops, reinforcements and crates.
- **Clone:** only the two production exits (`world.cpp:1512`, `:1569`).
- `Trainable` is honoured only under `vet_any_` (`world.cpp:1063`). The legacy `veteran_rank` firepower path (`weapon_unit.cpp:86`) ignores it for IVAN, TERROR, ENGINEER, SPY, DRON, AMCV, CMIN and ROBO [CONFIRMED-DATA Trainable=no].
- **Fix:** grant in `add_combat_unit` by class, gated on Trainable (with W7-01-15's naval exclusion). Optionally mark the ConYard.
- **Nit:** the AI spy order hardcodes 0.5 / 1000 (`world.cpp:4174`); use the rules values.

#### W7-08-22: The Desolator eruption ignores VeteranROF (and deploys instantly)

- **Data** [CONFIRMED-DATA]: DESO has ROF in both VeteranAbilities and EliteAbilities (VeteranROF 0.6 → 36 frames). The generic ROF getter multiplies by Rules+0x690 [CONFIRMED-RE 0x6FC5FF]; that the eruption reload uses that getter is [SUSPECTED].
- **Clone:** `world_abilities.cpp:605` uses the raw ROF 60.
- `deploy_next_fire = ability_frame_` (`:94`) skips the 15-frame `Deploy=299,15,0`. Extend W7-01-31 to the Desolator.
- **Fix:** `rof_frames(w.rof, has(Rof), vet_rules_.rof, …)`, with the first eruption after the deploy sequence.

#### W7-08-23: A Battle Fortress does not close to its passengers' range

- **Original** [CONFIRMED-RE 0x700760]: an OpenTopped transport's weapon range = `min(own range, min over passengers' weapon Range)`, without the +2 bonus. A BFRT carrying GIs approaches to 4 cells.
- **Clone:** approaches to its own 20mmRapid range of 5.5. Short-range passengers (GI M60 4, Tanya pistols 4) can be left out of reach even with the +2 bonus. (W7-08-15 is unused.)
- **Fix:** in the combat approach range for `w5.p.open_topped` transports, take the min with the live cargo `w_range`.

#### W7-08-24: Battle Fortress passengers fire only at units, and healing passengers are skipped (SUSPECTED)

- **Clone** [CONFIRMED-CODE `world_w5.cpp:236-256`]: scans `units_` only; `deliver_hit_from(…, -1, tgt)` is a unit hit; negative-damage passengers are skipped.
- **Original** [SUSPECTED; 0x6F9840-0x6F9960 validates passenger targets including buildings]: passengers hold an ordinary target that may be a building. Fire applies ×1.2 to negative damage too [CONFIRMED-RE 0x6FD91C].
- **Fix:** fire at `t.target_building` when it is in range + bonus; allow heal passengers on damaged allies.

## Cross-references (filed elsewhere; new evidence only)

- **W7-01-02 / 03 / 41 / 42 (spy disguise).** Sub-audit B adds [CONFIRMED-RE 0x5224B0, 0x522520]:
  - The PermaDisguise default is the side's E1/E2 in the **spy's own** house. It fools nobody until the spy "disguises" on an enemy infantry. A spawned spy is shot at once [CONFIRMED-PLAY c7].
  - Disguising on an already-disguised infantry copies **its** disguise.
  - DetectDisguiseRange is 0 except on NAPSIS (Yuri).
  - Sonar (`cell+0x7C`) does not reveal spies.
  - The dog never attacks a disguised spy [CONFIRMED-PLAY c7b].
- **W7-01-09.** Erase immunity also covers ignoreDefenses hits and heals, and is applied after the curtain test (0x700F51). The fix must sit before any ignore-defenses bypass added by W7-08-05.
- **W7-01-29.** Confirmed again: the erase calls KillOccupants (0x719CB7).
- **W7-02-03 / W7-01-31.** The deployed Desolator still ignores a move order [CONFIRMED-PLAY c9-c10]. A deployed GI or GGI undeploys and walks [CONFIRMED-PLAY].
- **W7-07-10 / W7-02-01.** Ctrl force-fire on own units is refused (`input_original.cpp:154`) [CONFIRMED-PLAY c1].
- **W7-05-13** RadarInvisible; **W7-06-07** Naval Yard repair; **W7-06-11** DistributedFire; **W7-03-13** Chrono Miner; **W6-04-17** Siege Chopper deploy; **W6-04-20** Force Shield.

## Verified OK

- **Radiation** (abilities.md §A):
  - eruption accumulation (+~50853 rad_total per 60 frames);
  - 10-cell spread;
  - 16-frame application by `[RadSite] Verses` (infantry 100 %, light 50 %, heavy 10 %);
  - linear decay after the source dies;
  - foot units only; garrisoned (limbo) infantry untouched [CONFIRMED-PLAY c3, c4b].
- **Iron Curtain:** infantry kill, ~750-frame vehicle and building invulnerability, damageable again after expiry [CONFIRMED-PLAY c1, c1b].
- **Chronosphere:** organic non-Teleporter kill, ChronoDelay freeze ~60 frames, tank lost on water, hovercraft survives on water [CONFIRMED-PLAY c2, c2b].
- **Terror Drone:** instant infantry kill; immediate first tick on vehicles; 50 × Verses per tick [CONFIRMED-PLAY c6b].
- **IFV:** all five modes measured, weapon by IFVMode [CONFIRMED-PLAY c9]. Elite only HoverMissileE [CONFIRMED-DATA].
- **Battle Fortress:** passenger range +2 and damage ×1.2 on the passenger's own vet-scaled damage [CONFIRMED-RE 0x6F67F9, 0x6FD91C]. Upgrades abilities-w5 §6 from PROBABLE to CONFIRMED (Rules+0xF58 / +0xF5C, firer flag +0x82).
- **Garrison fire:** OccupyWeapon / Elite, shooter cycling, ROF / count / 1.2, damage ×1.2, range rule, neutral owner sync [CONFIRMED-RE; CONFIRMED-PLAY c4].
- **No warhead clears a garrison in the original,** and the clone has none [CONFIRMED-RE].
- **Engineer:** capture at any HP, consumed; own building repaired to full (400 → 1000) [CONFIRMED-PLAY c5b]; IFV RepairBullet heals with raw damage [CONFIRMED-RE 0x6FD7EF].
- **Spy infiltration:**
  - power blackout 1000 (set, not added);
  - refinery 50 %;
  - radar re-shroud;
  - SW reset;
  - barracks / factory veterans, permanent and per house;
  - first-match dispatch order [CONFIRMED-RE 0x4571E0, 0x50BAB0; CONFIRMED-PLAY c7].
- **Self-heal** (type and SELF_HEAL ability) +1 HP every 14 frames up to full [CONFIRMED-RE 0x70B300; CONFIRMED-PLAY c5-c11].
- **Hospital** 20 HP / 50 frames per hospital, **Machine Shop** 5 / 75 [CONFIRMED-PLAY c5-c11]. Neither can be entered in YR [CONFIRMED-DATA].
- **Service Depot:** RepairStep 8, cost per step, one client, vehicles only, stops at full [CONFIRMED-PLAY c6b], once the vehicle is parked beside it.
- **Mind control** (campaign Yuri): capacity 1, a new capture frees the old, full slot stops auto-acquire (CannotControlAnyMore FUN_004721D0) [CONFIRMED-RE], release on controller death, ImmuneToPsionics refused for explicit and auto targets, no range leash.
- **MCV deploy / undeploy, Tesla coil charging, elite deploy weapons** (ParaE, MissileLauncherE), Ivan blast from Rules IvanDamage.

## Prioritized fix list

| # | Item | Where | Size |
|---|---|---|---|
| 1 | W7-08-01 depot / outpost order | `command.cpp`, `input*.cpp`, touch | S |
| 2 | W7-08-04 + 05 + 06 Iron Curtain interlocks | `world_abilities.cpp`, `combat.cpp` can_crush, `world_sw.cpp`, `world_w6.cpp` | M |
| 3 | W7-08-09 ConditionRed evacuation | `world_depth.cpp` | S |
| 4 | W7-08-11 + 12 + 13 mind control | `campaign_runtime.inc`, `combat.cpp:145` | S-M |
| 5 | W7-08-07 + 08 Chronosphere rules | `world_sw.cpp` an_chrono | M |
| 6 | W7-08-02 + 03 depot drone eject and phase | `world.cpp:2433-2482` | S |
| 7 | W7-08-18, 21, 22 ROF / veterancy fidelity | `world_abilities.cpp`, `world.cpp` add_combat_unit | S |
| 8 | W7-08-19, 20 engineer hut / neutral repair | `command.cpp`, `world.cpp`, `world_depth.cpp` | S |
| 9 | W7-08-16, 17, 23, 24 | `world_w5.cpp`, `world_an.cpp`, combat approach | S |

## Evidence

- `08-evidence/`:
  - `c1-iron`, `c1b-iron-enemy`;
  - `c2-chrono`, `c2b-chrono-water`, `c2-terrain`;
  - `c3-rad`;
  - `c4-garrison`, `c4b-garrison-ironed-deso`;
  - `c5-c11-heal`, `c5b-tech`;
  - `c6-c8-depot-drone`, `c6b-depot-drone`;
  - `c7-spy`, `c7b-spy-disguised`;
  - `c9-c10-ifv-deploy`, `c10b-deploy-range`;
  - each with `*-result.txt`, plus the runner `run-play.ps1`.
- RE decompiles:
  - `D:\ra2-re\w7-abs-A-out\b1..b32`: Iron Curtain, Chronosphere, garrisons, erase immunity;
  - `w7-abs-B-out\dc1..dc8`: depot 0x44B780, radio repair 0x6F4010, threat eval 0x6F71D0, Update 0x6F9380, disguise;
  - `w7-abs-C-out\cm1..cm8`: CaptureManager;
  - `w7-abs-D-out\dec1..dec9`, `dis1..dis17`: engineer, infiltration, OpenTopped, deploy.
