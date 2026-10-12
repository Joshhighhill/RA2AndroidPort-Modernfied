# Wave 7 / 01: Allied infantry behaviour gap analysis

**Tree:** master `b3a509f`, engine `D:\ra2-android\build\ra2engine.exe`.

**Spec sources:**
- `rulesmd.ini`, `artmd.ini` and `aimd.ini` (YR; extracted copies are in `D:\ra2-re\w5-abil-out\`).
- `gamemd.exe` 1.000, decompiled with Ghidra 11.2.1.

**Scope:** E1 GI, GGI, ENGINEER, SPY, JUMPJET (Rocketeer), GHOST (SEAL), CLEG, SNIPE, TANY and ADOG; superweapon paratroopers; and the stolen-tech CCOMAND and PTROOP.

**Method.** A lead (L) and four opus sub-audits each covered one slice:
- **A:** Spy, Engineer, dog.
- **B:** common infantry behaviour (prone, scatter, fear, death, sequences, voices).
- **C:** deploy, garrison, transports, paratroopers, commands.
- **D:** Tanya, SEAL, Chrono Legionnaire, Sniper, Rocketeer, CCOMAND and PTROOP.

Each slice was checked against the data first, then the RE docs, the engine code and Ghidra, then targeted `--play` runs. Wave 6 (`docs/gap/wave6/04-units.md` and its part A) already measured weapon-level fidelity, and most of it is fixed; it is not repeated here.

**Tags:**
- `[CONFIRMED-RE <addr>]`: the gamemd.exe decompile shows it.
- `[CONFIRMED-DATA]`: the rules, art or aimd keys show it.
- `[CONFIRMED-PLAY <script>]`: measured with the engine.
- `[SUSPECTED]`: not verified.

**Evidence (not committed):**
- Play scripts are in the audit session scratchpad: `play\{t0,L-snd,A-spydog,A-multi,B-ret,B-dog,B-prone,B-c4ret,C-deploy,D-cleg-jj}.play`. Their outputs are in `out\<name>\play.log`.
- Ghidra outputs are in `D:\ra2-re\w7-ainf-out\` and `D:\ra2-re\w7-ainf-{A,B,C,D}-out\`.
- Ghidra scripts are in `D:\ra2-re\w7-ainf-scripts\`.

## Summary

There are **45 gaps**: 0 blockers, 11 major, 27 minor and 7 cosmetic. Two wave-6 open items are closed as faithful (see "Resolved").

What a player notices first:
1. **Dogs bite tanks** (W7-01-01). A dog auto-acquires a vehicle and latches on, through its never-used `VirtualScanner` secondary weapon.
2. **Spies are not spies** (W7-01-02, W7-01-03, W7-01-04).
   - A new spy comes out undisguised.
   - Dogs never see through a disguise.
   - A spy or engineer in a group, or two of them together, cannot infiltrate or capture.
3. **GI and Guardian GI deploy** (W7-01-05, W7-01-06, W7-01-08).
   - A deployed GI walks off on any move click.
   - An undeployed Guardian GI shoots at aircraft.
   - The AI never deploys its GIs.
4. **The Chrono Legionnaire's erase is incomplete** (W7-01-09, W7-01-10, W7-01-11).
   - A target being erased can still be killed normally, so a Demo Truck being erased explodes.
   - Erasing never earns veterancy.
   - A power plant or defence being erased keeps working.
5. **Superweapon paratroopers appear on the ground** with no parachute fall (W7-01-07).

## Per-unit table

| Unit | Works (measured in wave 6 or re-verified here) | Gaps |
|---|---|---|
| E1 GI | M60 / Para weapons, deploy weapon swap, DeploySound; garrison fire formula and EVA; prone/fear machine; voices; elite SELF_HEAL | 05, 08, 17, 18, 19, 31, 35, 39 |
| GGI | Deployed MissileLauncher does 40 vs heavy; DeployedCrushable is read; voices | 05, **06**, 08, 17, 19, 31 |
| ENGINEER | Capture, repair, allied repair, bridge hut, defuse, IFV repair; refuses Capturable=no; ProduceCashStartup; voices and EVA | **04**, 13, 16, 33, 36, 45 |
| SPY | Infiltrate dispatch order and every per-class effect; stolen tech; EVA; no reveal when it fires | **02**, **03**, **04**, 13, 14, 15, 33, 34, 40, 41, 42 |
| GHOST SEAL | MP5; MP5 vs vehicles on land (faithful, RE); C4 on buildings and ships; swims | 12 (water sounds), 22, 23, 24, 43b |
| TANY | Pistols; Sapper vs vehicles; C4 plant with a 27-frame fuse; swims; BuildLimit 1; ImmuneToPsionics; Crushable=no; SelfHealing; create and enter-water sounds | 22, 23, 43b |
| CLEG | Erase budget Strength×10, stacking, reset on let-go; teleport delay and freeze; warp effects; teleport sounds | **09**, **10**, **11**, 25, 26, 29, 44 |
| SNIPE | AWP range 14, ROF 150, elite 60×0.6; British only; needs a spotter | none found |
| JUMPJET | Jumpjet flight at 500; BalloonHover; 20mm AA+AG; crash fall; voices | 12 (move loop and impact sound), 21, 27 |
| ADOG | Ordered bite kills infantry in one hit; Sight 9; NotHuman → Die1; ImmuneToPsionics | **01**, **02**, 32; R-054 (jump not drawn) still open |
| CCOMAND / PTROOP | Stolen-tech gate, teleporter, FakeC4 as a weapon (faithful), MindControl | 26, 28 |
| Paratroopers | Types and counts read from `[General]`; team drops fall under PARACH | **07**, 38 |

## Gap list

### MAJOR

#### W7-01-01: Dogs auto-acquire vehicles and latch onto them (`NeverUse=` is not read) (B)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY B-dog.play]: a player ADOG with no orders, next to an enemy Rhino, reads `parasited=1` within 1 frame. The Rhino then loses 20 HP per sample, 399 → 119.</li><li>[CONFIRMED-DATA]: `[ADOG] Secondary=VirtualScanner` has `NeverUse=yes`. ParasiteDog does 0% against every vehicle armour.</li><li>`git grep NeverUse engine` finds 0 readers.</li></ul> |
| **Original** | <ul><li>A `NeverUse` weapon only widens the scan; it never fires.</li><li>GoodTeeth at 0% cannot target vehicles.</li><li>Retaliation refuses a 0%-Verses weapon [CONFIRMED-RE 0x707C40].</li><li>Wave 6 saw an ordered dog refuse a tank. This gap is the auto-acquire path.</li></ul> |
| **Clone** | `weapon_unit.cpp:31-38` (`choose_weapon`) falls back to slot 1 (VirtualScanner, SA, 25% vs heavy). `world_abilities.cpp:405-424` then attaches the parasite. |
| **Fix** | <ul><li>Parse `NeverUse`.</li><li>Exclude those slots from `choose_weapon`, `slot_can_target` and `unit_weapon_for_building`.</li><li>Use their range only to widen the acquire radius.</li><li>Gate the Parasite ability on the warhead's Verses.</li></ul> |

#### W7-01-02: `DetectDisguise` is never read, and the detection mechanism is invented (A)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY A-spydog.play L33-66]: an enemy dog 2 cells from a disguised spy kept `utgt=-1` for 120 frames.</li><li>[CONFIRMED-PLAY A-multi.play L18-30]: control run; the same dog killed an undisguised spy within 15 frames.</li><li>`Unit::detect_disguise` (`world.hpp:305`) is only set by `--demo-capture`.</li><li>`world.cpp:2780-2793` clears `disguised` for any unit within 6 cells of a detector (`capture.hpp:62-66`).</li></ul> |
| **Original** | <ul><li>[CONFIRMED-RE 0x6F71D0 L455-467]: the threat scan skips a disguised object unless the scanning type has DetectDisguise (+0xD31).</li><li>There is no range and no flag clear.</li><li>`DetectDisguiseRange` applies to buildings only (see W7-01-34).</li></ul> |
| **Fix** | <ul><li>Read the key.</li><li>Gate the disguised-target skip on it in `combat.cpp:1182`, `building_combat.cpp:257`, `world_depth.cpp:256` and `world_w5.cpp:241`.</li><li>Delete the reveal loop.</li><li>Correct `capture.md` §C4.</li></ul> |

#### W7-01-03: `PermaDisguise` is not applied at creation, and AI spies use an invented look (A)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY A-spydog.play L26-29]: a spy trained from GAPILE comes out with `udisguised=0`.</li><li>`world.cpp:4372-4380` disguises only AI spies, as the enemy's first infantry image, in the enemy's colour.</li></ul> |
| **Original** | [CONFIRMED-RE 0x6F34A0 L597-612, called from 0x517AE0]: a `CanDisguise && PermaDisguise` unit starts disguised as `[General] AlliedDisguise` / `SovietDisguise` / `ThirdDisguise` (E1 / E2 / INIT) by side, in its own house. |
| **Clone** | Every enemy and every defence auto-targets a new player spy until the spy "attacks" an enemy infantry unit. |
| **Fix** | Give the side's default disguise in `add_combat_unit`, and drop the AI special case. |

#### W7-01-04: Spy and engineer orders only work for a lone unit (A)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY A-multi.play L33-58]: two engineers clicked on an enemy GAPOWR give `cursor_name="attack"`, `utgtb=-1`, and no capture after 150 frames.</li><li>`lone_selected_with_image` (`input.cpp:42-70`, `input_original.cpp:229-234`); `command.cpp:234/255`.</li></ul> |
| **Original** | [CONFIRMED-RE 0x4AE660, called from 0x4AB8C0]: each selected object computes its own `What_Action` (vtbl+0x74) and `Active_Click_With` (vtbl+0x144). |
| **Fix** | Classify per unit: spies and engineers get Sabotage/Capture, the rest of the group gets Attack. |

#### W7-01-05: A deployed GI or Guardian GI packs up instantly and silently on a move order (C)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY C-deploy.play L22-32]: after `key d` (`snd(GIDeploy)=1`), a ground click moved the unit 71030 → 74032 within 60 frames. `snd(GIUndeploy)` was never counted.</li><li>Cause: `world.cpp:1210-1211`.</li></ul> |
| **Original** | <ul><li>[CONFIRMED-RE 0x51F510]: for a deployed Deployer infantry, a player's ground click becomes NoMove. Force-fire on a cell is allowed only within the deploy weapon's range.</li><li>[CONFIRMED-RE 0x51F370]: Mission_Move on a human-owned unit clears the destination and stays put. A computer-owned unit plays Undeploy first.</li></ul> |
| **Fix** | <ul><li>Refuse the move in `order_unit_move`.</li><li>Return NoMove in `classify` (per unit for a mixed selection).</li><li>For computer houses, undeploy through the Deploy event so the animation and sound play.</li></ul> |

#### W7-01-06: An undeployed Guardian GI fires its MissileLauncher at aircraft (C)

| | |
|---|---|
| **Evidence** | `weapon_unit.cpp:31-37` → `choose_weapon` (`weapon.cpp:19-24`) falls back to slot 1, the anti-air MissileLauncher (AAHeatSeeker2). The same happens at `weapon_unit.cpp:68-70`. Not played, because cheat-spawned aircraft sit on the ground. |
| **Original** | [CONFIRMED-RE 0x521610] `InfantryClass::SelectWeapon`:<ul><li>in the deploy group, it uses DeployFireWeapon;</li><li>in an open-topped transport, OpenTransportWeapon;</li><li>otherwise always weapon 0.</li></ul>So an undeployed GGI has only its M60 and cannot engage aircraft. |
| **Fix** | For `deploy_fire && !deployed`, return the primary or nothing, never the secondary. |

#### W7-01-07: Superweapon paratroopers appear on the ground with no fall or parachute (C)

| | |
|---|---|
| **Evidence** | `world_an.cpp:527-541`, and the fallback at `:688-700`, call `an_spawn` on a free ground cell. Neither sets `cargo_of = kParachuteCargo` or `para_alt`. The R-122 parachute only covers team drops. |
| **Original** | [CONFIRMED-DATA]: `ParadropRadius=1024`, `ParachuteMaxFallRate=-3`, PARACH, and the art `Paradrop=` sequence. This is the same model as the clone's `an_drop_passenger`. |
| **Fix** | Route superweapon drops through `an_drop_passenger`. |

#### W7-01-08: The AI's GIs and Guardian GIs never deploy or undeploy by themselves (C)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x521050]: in Guard or Area Guard, a computer-owned Deployer+DeployFire infantry auto-deploys when it has no target, stands on its archived cell, and a per-difficulty Rules timer (Rules+0xE30[diff]) has expired. ImmuneToRadiation types are excluded.</li><li>It undeploys:<ul><li>in Hunt with no target [0x51F250];</li><li>on a move [0x51F370];</li><li>when its deploy weapon cannot reach its target [0x522070].</li></ul></li></ul> |
| **Clone** | The only computer deploy is the team-script Deploy line (`world.cpp:4241-4246`), which toggles. |
| **Fix** | A computer-house pass applying these rules. The timer index needs one more RE hop [RE-FALLBACK until then]. |

#### W7-01-09: A unit being erased still takes damage (D)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY D-cleg-jj.play L21-64]: two Grizzlies took a Rhino that was being erased from 400 to 10 HP.</li><li>Neither `ability_damage_unit` (`world_abilities.cpp:207`) nor the `combat.cpp` paths check `warp_remaining`.</li></ul> |
| **Original** | [CONFIRMED-RE 0x700DA0, 0x70BA30]: when the target is BeingWarpedOut (+0x270, set by TemporalClass::Fire 0x71A1A0), damage is 0 and the function returns. |
| **Clone** | The target can be killed mid-erase, the killer gets the credit, and its death weapon fires (Demo Truck, Apocalypse). |
| **Fix** | Return 0 damage for `warp_remaining > 0` (units and buildings) at the shared damage entry points. |

#### W7-01-10: Erasing gives no veterancy (D)

| | |
|---|---|
| **Original** | [CONFIRMED-RE 0x719A70]: when the erase completes, `VeterancyStruct::Add` (FUN_0074ED70) is called with the cost ratio if the owner type is Trainable (veterancy.md V2/V14). |
| **Clone** | `world_abilities.cpp:165-200` and `:737-749` never set `killed_by`, so the credit pass (`world_veterancy.cpp:160-174`) credits nobody, or whoever last damaged the target. |
| **Fix** | Set `killed_by` to the primary legionnaire, or credit V2 directly. |

#### W7-01-11: A building being erased keeps its power and keeps working (D)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x71A1A0, 0x4521B0, 0x452200]: TemporalClass::Fire on a building sets the house's power-recheck flags, sets HasPower=0 and pauses its animations. LetGo restores them.</li><li>So a power plant loses its output, and Powered defences go offline.</li><li>Whether unpowered defences also stop is [SUSPECTED].</li></ul> |
| **Clone** | `b.warp_remaining` is only read by the erase loop (`world_abilities.cpp:726-750`). |
| **Fix** | Treat the building as unpowered in the power sum and in the defence gates, and pause its animations. |

### MINOR

#### W7-01-12: Per-type sound and art keys are looked up by the SHP image name, not the rules id (L; B 9e)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY L-snd.play] on Bay of Pigs:<ul><li>Tanya entering water plays `TanyaEntersWater`.</li><li>A SEAL entering water plays nothing.</li><li>A flying Rocketeer never starts `RocketeerMoveLoop`.</li><li>On death only `RocketeerDie` (crashing) plays; `RocketeerCrash` (impact) never does.</li></ul></li><li>`pres_audio_events.inc` uses `w5_bld_key(u.image, …)` for Enter/LeaveWaterSound (~l.366), Impact*Sound (~l.354), AuxSound1/2 and MoveSound (~l.520), CreateSound (~l.571), ChronoIn/Out (~l.153, ~l.500) and Enter/LeaveTransportSound (~l.482). `DeadBodies=` (`pres_game.inc:1025`) is looked up the same way.</li><li>`Unit::image` is the SHP name (GHOST → SEAL, JUMPJET → ROCK, E1 → GI), and rulesmd has no `[SEAL]`, `[ROCK]` or `[GI]` section [CONFIRMED-DATA].</li></ul> |
| **Note** | Sub-audit D listed these sounds as wired by e24699f. They are wired, but they miss for any type whose `Image=` differs from its id. |
| **Fix** | Carry the rules id on `Unit` (or keep an image→id map) and key these lookups by id, as `add_w5_voices` already does. |

#### W7-01-13: Select Combatants (P) ignores `IsSelectableCombatant` (L, C)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-RE 0x731690]: the filter is `WhatAmI != building && Type+0xDBC && vtbl+0x13C`.</li><li>[CONFIRMED-RE loader 0x711510]: the type flag lives at +0xDBC.</li><li>[CONFIRMED-DATA]: ENGINEER and SPY are `no`, but DefuseKit and MakeupKit both have Damage 1.</li><li>The clone's `game_scene_controls.inc:253-256` uses `w_damage > 0`, so P sweeps engineers and spies into the army.</li></ul> |
| **Fix** | Parse the key and use it. |

#### W7-01-14: Infiltrate is offered on non-Spyable and allied buildings (A)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-PLAY A-spydog.play L83-88]: the infiltrate cursor shows over an enemy GAPILL (Spyable=no).</li><li>`input_original.cpp:229` tests only the house. `command.cpp:261` and `world.cpp:2132` allow allies, so a spy can steal 50% of an ally's money.</li></ul> |
| **Original** | [CONFIRMED-RE 0x51E17C L981-988]: the target must be a building, not allied, and `Spyable` (+0x1576). |
| **Fix** | Gate both the cursor and the command. |

#### W7-01-15: A spied factory makes ships and aircraft veteran (A)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x734717]: a spied War Factory makes vehicles veteran only if they are `!Naval` and Trainable.</li><li>[CONFIRMED-RE 0x517B4A]: a spied Barracks makes infantry veteran.</li><li>No reader exists for aircraft.</li></ul> |
| **Clone** | `world.cpp:2180` includes ships. `world.cpp:1535` makes aircraft veteran from any bit (a Kirov from NAWEAP). |
| **Fix** | Restrict the bit to non-naval vehicles. Also correct abilities-w5.md §1: naval yards are not Spyable [CONFIRMED-DATA]. |

#### W7-01-16: Engineer capture edge cases (A)

| | |
|---|---|
| **(a) Capture cursor on non-Capturable buildings** | <ul><li>[CONFIRMED-PLAY A-spydog.play L76-82]: the capture cursor shows over GAPILL, and the order is then silently dropped (`command.cpp:249`).</li><li>Original [CONFIRMED-RE 0x51E17C L709-715]: no capture action is offered.</li></ul> |
| **(b) Building being sold** | <ul><li>Original [CONFIRMED-RE 0x519449 L1279]: an engineer cannot capture a building in the Selling mission; it scatters instead.</li><li>Clone: `capture_building` (`world.cpp:2089`) has no check.</li></ul> |

#### W7-01-17: A prone soldier given a move order crawls instead of standing up (B)

| | |
|---|---|
| **Evidence** | [CONFIRMED-PLAY B-prone.play MV2-MV23]: Crawl plays for more than 21 frames (`inf_anim.cpp:202`). |
| **Original** | [CONFIRMED-RE 0x51A830]: SetDestination on a human-owned prone infantry plays Up. FUN_0051FDE0 does not re-drop a soldier that has a destination. |
| **Impact** | Visual only. |
| **Fix** | Queue Up when the destination is set. |

#### W7-01-18: The SCATTER ability is invented, and scatter from incoming fire is missing (B)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x517DB5, 0x51CEA0]: when hit, only Fraidycat types scatter.</li><li>[CONFIRMED-RE 0x481580, 0x5203E0]: SCATTER, `[IQ] Scatter=2`, PlayerScatter and "elite in the cell" only gate `CellClass::ScatterContent`.</li><li>ScatterContent is called when a vehicle enters a cell, when a building is placed, and when infantry fires a primary weapon with Speed < `Incoming=10` (Ivan bomb, TerrorBomb).</li></ul> |
| **Clone** | `world_w5.cpp:177-192` (pass 3b, RE-FALLBACK) makes any idle hurt unit step aside. Nothing reads `Incoming`. |
| **Fix** | Delete pass 3b and implement ScatterContent with the original gates. |

#### W7-01-19: Scatter (X) moves a deployed GI and overrides moving units (B)

| | |
|---|---|
| **Evidence** | [CONFIRMED-PLAY B-prone.play DEP→XSC]: the GI goes back to Ready and moves 57020 → 58020 (`unit_orders.cpp:87-112` → `world.cpp:1211`). |
| **Original** | [CONFIRMED-RE 0x4C6BD0 → 0x51CEA0]: X is ignored for human-owned deployed units, for moving non-Fraidycat units, and for units in a non-interruptible sequence. |
| **Fix** | Skip those three cases in `order_scatter`. |

#### W7-01-20: Friendly-vehicle scatter is too narrow, and crushers kill allied infantry (B)

| | |
|---|---|
| **Original** | [CONFIRMED-RE 0x4B2CDB, 0x75A6AA, 0x449576, 0x742720]:<ul><li>ScatterContent(1,1) moves every interruptible occupant of the cell, including units with a target; deployed units undeploy.</li><li>Auto-crush excludes allies.</li></ul> |
| **Clone** | <ul><li>`world.cpp:5762-5769` scatters only same-house, idle infantry with no target.</li><li>`world.cpp:5670` crushes anyone of another house (`!= house`), allies included. That the crush step itself spares allies is [SUSPECTED].</li></ul> |

#### W7-01-21: The Rocketeer never uses its flight sequences (B; still open from wave 3, RC-7)

| | |
|---|---|
| **Evidence** | <ul><li>`world.cpp:5394` runs `inf_anim` only for `!is_aircraft`. A jumpjet is `is_aircraft` (`main.cpp:987-990`), so it uses a 4-entry table (`main.cpp:1441`).</li><li>[CONFIRMED-DATA] `[RocketeerSequence]` defines Fly, Hover, FireFly, Tumble, AirDeathStart/Falling/Finish and Cheer.</li></ul> |
| **Clone** | The Rocketeer is drawn in ground poses and dies with a ground death. It never idles or cheers. |
| **Fix** | Run `inf_anim` with the airborne substitutions, after RE of PlayAnim FUN_0051D4C0. |

#### W7-01-22: `CanC4=no` is not wired (D)

| | |
|---|---|
| **Evidence** | <ul><li>[CONFIRMED-DATA]: CAMISC01/02/06 and AMMOCRAT have `CanC4=no`.</li><li>`World::set_no_c4_types` (`world.hpp:1947`) has no caller, so Tanya and SEALs can C4 explosive barrels.</li></ul> |
| **Fix** | Call the setter from `main.cpp` with the parsed types. |

#### W7-01-23: C4 and the Iron Curtain interact the wrong way round (D)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x51A3F0]: no C4 can be planted on an Iron-Curtained building.</li><li>[CONFIRMED-RE 0x700DA0]: the C4 detonates with ignoreDefenses=1, so a Curtain applied after planting does not save the building.</li></ul> |
| **Clone** | <ul><li>Planting (`world_abilities.cpp:357`) has no Curtain check.</li><li>The blast (`:775` → `:217`) returns early under a Curtain.</li></ul> |
| **Fix** | Swap both checks to match the original. |

#### W7-01-24: The SEAL's naval weapon choice ignores Hover targets and Beach cells (D)

| | |
|---|---|
| **Original** | <ul><li>[CONFIRMED-RE 0x6F2890]: the naval chooser applies on Water **or Beach** cells.</li><li>[CONFIRMED-RE 0x6F2D80], case 4: the secondary is used only against non-Hover, non-Organic targets. So the SEAL uses its MP5 against LCRF, SAPC and ROBO.</li></ul> |
| **Clone** | `world_an.cpp:56-58` and `:118-122`: no SpeedType test, and water cells only. |

#### W7-01-25: The erase step uses veterancy- and handicap-modified damage (D)

- [CONFIRMED-RE 0x719A70]: the original uses the raw `Weapon->Damage`.
- `world_abilities.cpp:709` sums `w_damage`, which already includes FIREPOWER and the handicap.

#### W7-01-26: `MoveToShroud=no` is not read (D, L)

- [CONFIRMED-DATA]: set on CLEG and CCOMAND.
- `teleport_unit` (`world_abilities.cpp:108`) lets a legionnaire blink into unexplored map.
- The original semantics are [RE-FALLBACK]: the readers of the loader string at 0x843464 were not traced.

#### W7-01-27: A Rocketeer can board an IFV or transport (D)

- [CONFIRMED-PLAY D-cleg-jj.play L71-90]: `cargo_total` goes 0 → 1.
- `can_load` (`cargo.cpp:16-51`) has no jumpjet test.
- That the original forbids it is [SUSPECTED]; RE `What_Action` before fixing.

#### W7-01-28: A Psi-Corps Trooper that is mind-controlling something can enter a transport (D)

- [CONFIRMED-RE 0x736500, case 0xF]: the original refuses entry while the passenger is controlling a unit, and refuses entry into a transport that is being warped out.
- `can_load` checks neither.

#### W7-01-29: Occupants of an erased garrison walk out (C)

- [CONFIRMED-RE 0x719C02 → 0x4585B0]: the original calls KillOccupants.
- In the clone, `world_abilities.cpp:737-742` is followed by `world_depth.cpp:636-641` `unload_occupants`, so the occupants survive.

#### W7-01-30: The Enter cursor shows on a full or too-small transport (C)

- [CONFIRMED-RE 0x51E17C]: when `ReceiveMessage(0xF)` refuses, the original shows NoEnter (0x1F).
- `input_original.cpp:165-171` and `:197-206` never test `can_load`.

#### W7-01-31: Deploy and undeploy are instant (C)

- [CONFIRMED-RE 0x522280 / 0x521050] and [CONFIRMED-DATA `Deploy=300,15,0`, `Undeploy=276,2,2`]: in the original, deploying takes 15 frames.
- In the clone, `world_abilities.cpp:67-81` flips the state immediately, and the deploy weapon is live on the same tick.
- Whether the original can fire during those 15 frames is [SUSPECTED].

#### W7-01-32: `DefaultToGuardArea=yes` (dogs) is not read (A, L)

- [CONFIRMED-RE 0x51CB2A; loader 0x7142F8, +0xD39]: an idle dog with no team enters Area Guard.
- The clone uses Guard instead, so the dog scans 2 cells instead of 3 and does not pursue.

#### W7-01-33: More behaviour keys with no reader (L, A, C)

[CONFIRMED-DATA; loader offsets CONFIRMED-RE 0x711510]

| Key | Units | Consequence |
|---|---|---|
| `PrerequisiteOverride` (+0x654) | GHOST (CAWA2A-D), CLEG (CAWASH16) | Owning these campaign buildings never unlocks the SEAL or Chrono Legionnaire. That this is the original semantics is [SUSPECTED]. |
| `ThreatPosed` / `SpecialThreatValue` | all | The clone's threat scoring ignores the per-type values [SUSPECTED]. |
| `StupidHunt` | SPY | An AI spy in Hunt behaves like a normal hunter. |
| `PreventAttackMove` (+0x6C8) | ENGINEER, SPY, CCOMAND | Inert until planning mode exists (R-150). No reader was found in the infantry or locomotor ranges. |
| `Agent` / `Infiltrate` / `Engineer` | SPY, ENGINEER | The clone identifies them by image name (`input.cpp:52,70`; `input_original.cpp:162,229`; `world.cpp:1410,3130,3759,4164,4372`; `world_w5.cpp:526`). Stock data is safe, but a modded type loses the ability. The `"SENGINEER"` test at `world.cpp:3759` is dead code. |

#### W7-01-34: Buildings with `DetectDisguise` (Psychic Sensor) never expose spies (A)

- [CONFIRMED-RE 0x445EE0 → 0x455981]: the original marks every cell within `DetectDisguiseRange` of the building.
- [CONFIRMED-DATA]: NAPSIS has a range of 15.
- The clone has nothing. What exactly the mark does to targeting and drawing is [SUSPECTED].

#### W7-01-35: The art's `FireUp=` / `FireProne=` release frame is not read (B; still open, infantry-anim.md §6)

- [CONFIRMED-RE 0x5203E0]: the shot is released on that frame: GI 2, SEAL 3, TANY 3, SNIPE 5, ADOG 6.
- The clone applies damage at the start of the animation.

#### W7-01-36: AI infantry fine points (B, C)

| | |
|---|---|
| **(a)** | [CONFIRMED-RE 0x517DB5]: a computer-owned engineer that is damaged while guarding switches to Hunt. The clone does not. |
| **(b)** | [CONFIRMED-RE 0x707C40]: a computer-owned unit switches to a new attacker whose threat is at least equal to its current target's. The clone only retaliates when it has no target (`combat.cpp:262`). |
| **(c)** | [CONFIRMED-RE 0x51F0F0 / 0x51F250]: a computer-owned Occupier in Attack or Hunt that targets an occupiable building gets Mission Enter. This is relevant to R-081 / W6-b. |

#### W7-01-37: `ScatterSound` never plays (B; SUSPECTED)

- `ScatterSound=CommandBar` has 0 readers, and [CONFIRMED-PLAY B-prone] counted no `snd(CommandBar)`.
- FUN_007300B0's sound argument was lost in the decompile.

#### W7-01-38: Suspected garrison and paradrop leftovers (C; SUSPECTED)

| | |
|---|---|
| **(a)** | A garrisoned building cannot be ordered to attack, and does not target structures (`world_depth.cpp:254-269`). |
| **(b)** | Clicking your own garrison does not evacuate it. world-depth.md §A5 says it should. |
| **(c)** | Falling paratroopers cannot be targeted. |
| **(d)** | The mission superweapon paratroopers get on landing has not been traced. |

### COSMETIC

#### W7-01-39: Garrison shots and occupant pips (C)

- Garrison shots use MGUN at the building's centre instead of `OccupantAnim=UCFLASH` at the MuzzleFlash ports, and there are no `OccupyPip` pips.
- Code: `pres_game.inc:967-985`; 0 readers for those keys [CONFIRMED-DATA].

#### W7-01-40: No veteran AltCameo after a factory is infiltrated (A)

[CONFIRMED-RE 0x7113E0]: `AltCameo` has 0 readers.

#### W7-01-41: A disguise is drawn for every viewer, the owner included (A; SUSPECTED)

- Code: `main.cpp:3938`, `iso_renderer.cpp:549`.
- The rules comment says the spy appears differently "on other people's computers".

#### W7-01-42: The disguise target filter is too broad (A)

- [CONFIRMED-RE 0x522430]: the original only accepts infantry, and copies the target's own disguise if it has one.
- The clone (`world.cpp:1411`) accepts any non-vehicle and copies `tgt.image`, which then feeds the trigger disguise checks.

#### W7-01-43: Infantry death presentation details (B)

| | |
|---|---|
| **(a)** | FLAMEGUY's `IsFlamingGuy` / `RunningFrames` are not read, so the burning infantry does not run. |
| **(b)** | Deep water: the original shows a Wake and splash and removes the body [CONFIRMED-RE 0x517DB5]. The clone plays WetDie on any water cell (`inf_anim.cpp:52`). |
| **(c)** | A death during a paradrop is forced to InfDeath 3 in the original. |
| **(d)** | InfDeath 9 (Mutate) should turn the victim into a Brute for the attacker. The clone plays Die2. |

#### W7-01-44: No WARPAWAY animation when an erased target vanishes (D)

- [CONFIRMED-RE 0x719A70]: the original plays the anim at Rules+0x340; the key name is unverified.
- `Ev::Kind::Erase` has no presentation consumer.

#### W7-01-45: Doc errors to fix (A)

- **capture.md §A2:** the original does have a damage-instead-of-capture branch. It is gated on the MultiEngineer session option, which is off by default [CONFIRMED-RE 0x519449 L1275-1311].
- **capture.md §C4** and **capture.hpp:62-66:** see W7-01-02.
- **abilities-w5.md §1:** see W7-01-15.

## AI use of Allied infantry (lead check)

[CONFIRMED-DATA aimd.ini] The stock TaskForces use every Allied infantry type:
- GI and Guardian GI attack and base-guard teams;
- dog guards;
- engineer oil and airport captures;
- six "spy vs X" teams;
- Tanya with GIs in a Night Hawk;
- Chrono Legionnaires vs the Construction Yard;
- Rocketeer harvester hunts and anti-Grand-Cannon teams;
- British snipers (one with an IFV);
- Battle Fortress teams carrying a SEAL or Chrono Legionnaires;
- Chrono Commandos.

All 14 script actions these teams use are defined (`script_actions.hpp`). The engine sends engineers to capture and spies to infiltrate (`world.cpp:4164`), and every one of these types is in `kAiUnits` (`ai_skirmish.inc:24-55`).

AI-specific gaps are W7-01-03, W7-01-08 and W7-01-36.

## Resolved / not a gap

- **W6-04-32, faithful:** in the original, a SEAL also uses its MP5 on a land vehicle. [CONFIRMED-RE 0x6F2890] keeps the primary when both warheads have non-zero Verses, and HollowPoint is 1%.
- **W6-04-33, faithful:** C4 planting is building-only [CONFIRMED-RE 0x519449 @0x51A3F0], and Sapper against a unit is a plain shot. Only the dead `ship_c4` probe remains.
- **R-053 can be narrowed:**
  - Assaulter=no on every stock type that sets the key;
  - DistributedFire is set on the Aegis only;
  - building damage never hurts the occupants [CONFIRMED-RE 0x4421B0].

## Verified OK

- **Fear and prone machine** [CONFIRMED-RE 0x517DB5, 0x51FDE0, 0x51D4C0; CONFIRMED-PLAY B-ret, B-prone]: values, decay, the Down/Up thresholds, the Crawls gate and ProneDamage all match.
- **Death anims:** InfDeath 1-10 map to Die1/Die2 and to InfantryExplode, FLAMEGUY, ELECTRO, HeadPop, Nuked, Virus and Brute. Corpses appear for humans only.
- **Veterancy:**
  - each type's own VeteranAbilities/EliteAbilities lists are used;
  - E1 and GGI get SELF_HEAL when elite;
  - Trainable=no is honoured.
- **Voices** for every Allied infantry type: Select, Move and Attack; Feedback at 30% on dropping to yellow (0x700DA0); Die or CrushSound; Cheer.
- **Retaliation:**
  - human-owned units retaliate only when idle;
  - a player's Tanya does not walk out to C4 a Sentry Gun that is shooting her [CONFIRMED-PLAY B-c4ret];
  - a weaponless Engineer never chases [CONFIRMED-PLAY B-ret].
- **Spy:**
  - infiltrate order Radar → Power → BuildTech → SuperWeapon → Storage → Factory [CONFIRMED-RE 0x4571E0];
  - every per-class effect, stolen tech by side, the spy being consumed, EVA;
  - no reveal when it fires.
- **Engineer:**
  - captures at any health;
  - refuses Capturable=no;
  - ProduceCashStartup only when capturing from neutral;
  - own and allied repair, bridge hut, defuse, IFV repair.
- **Garrisons:**
  - E1 is the only Allied Occupier;
  - the weapon, ROF, damage and range formulas match;
  - occupants unload when the building dies or is sold;
  - EVA plays;
  - passengers die with their transport.
- **Deploy:**
  - D or a click on the unit itself deploys, and DeploySound plays;
  - DeployFireWeapon is used when deployed, and OpenTransportWeapon in open-topped transports (0x521610);
  - a deployed human-owned unit holds its post against an out-of-range attack order.
- **Tanya:**
  - C4 plant with a 27-frame fuse;
  - no plant while the target is selling;
  - BuildLimit, ImmuneToPsionics, Crushable=no and SelfHealing;
  - `TanyaEntersWater` plays [CONFIRMED-PLAY L-snd].
- **Chrono Legionnaire:**
  - erase budget, stacking, LetGo hand-off;
  - teleport delay `max(dist/48, 16)` and the freeze;
  - the Fire gates (0x71A1A0).
- **Sniper:** AWP; British only; YR data has no garrison sniping, and the clone matches.
- **Rocketeer:** flies at 500; BalloonHover; AA+AG; `RocketeerDie` plays on a crash [CONFIRMED-PLAY L-snd].
- **Dog:** an ordered bite kills infantry; dog vs dog works.
- **Paradrop:** types and counts are read from `[General]`.

## Suggested fix order

1. **Small, data-driven fixes:**
   - 01 (`NeverUse`);
   - 02 and 03 (`DetectDisguise`, `PermaDisguise`);
   - 06 (SelectWeapon);
   - 09, 10 and 11 (Temporal guards);
   - 22 and 23 (C4);
   - 12 (look up sounds by rules id);
   - 13 (Select Combatants).
2. **Input:** 04 (per-unit classify), 05 (NoMove when deployed), 14, 16, 19, 30.
3. **Simulation:**
   - 07 (superweapon paradrop fall);
   - 08 (AI auto-deploy, after the Rules+0xE30 RE hop);
   - 18 and 20 (ScatterContent);
   - 31 (deploy timing).
4. **Presentation:** 21 (Rocketeer sequences, after the PlayAnim RE), 17, and 39-44.
5. **RE first:** 27, 33, 34, 38.
