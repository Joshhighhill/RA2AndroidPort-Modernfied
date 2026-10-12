# Wave 7 / 09: Civilians, neutrals and campaign characters

**Tree:** master `b3a509f`, engine `D:\ra2-android\build\ra2engine.exe`.

**Spec sources:**
- YR `rulesmd.ini` / `artmd.ini` (extracted copies in `D:\ra2-re\w5-abil-out\`).
- The 24 RA2 campaign maps (Maps01.mix / MAPS02.MIX), the 14 YR campaign maps (mapsmd03.mix) and 197 stock MP maps (multimd.mix, MULTI.MIX).
- `gamemd.exe` 1.000 in Ghidra. Addresses in this build are shifted from YRpp. For example, the InfantryClass vtable is `0x7EA058`, the UnitClass vtable is `0x7F4C70`, and IsControlledByHuman is `0x50B550`.

**Evidence** is in `09-evidence/`:
- `lead/`: `snd.play` and `fear.play`, with their results.
- `A/`: RE dumps for civilian infantry and animals, plus `A1-crush-civie.play`.
- `B/`: map scans (`scan.txt`, `ev_obj.txt`), RE decompiles in `re/`, and the scanners in `py/`.
- `C/`: the census (`census-table.txt`), 11 play scripts, `run-excerpts.txt`, shots and tools.

RE text outputs are in `D:\ra2-re\w7-civ-A-out` and `D:\ra2-re\w7-civ-B-out`. The Ghidra project copies have been deleted.

**Scope notes** [CONFIRMED-DATA]:
- YR has **no trains**. `[VehicleTypes]` has no TRAIN or LOCO entry, "Train Tracks" exists only as terrain or overlay, and action 97 is used by no map.
- There are no dolphin neutrals (DLPH is an Allied unit) and no llama.
- The alligator is `ALL` and the chimp is `JOSH`.
- **Einstein is `EINS`**, not EINSTEIN.
- There is no Lt. Eva unit.

## Summary

On the 24 RA2 campaign maps, every civilian, animal, character and civilian vehicle **spawns at the right count and draws** with the correct palette and remap [CONFIRMED-PLAY C1-C5, C7].

The behaviour around those units has three serious problems:
- **Civilians are silent** (W7-09-01). Every map-only type has an empty voice set: there is no scream, no moo, no Einstein line and no car death sound. Clicking a civilian also never selects it or plays its voice (W7-09-10).
- **The player's own tanks kill friendly civilians** (W7-09-04). A house the player lists as an ally (Civie1 in ALL01) is crushable in the clone. The original refuses to crush any allied victim. The fix proposed in W7-03-01 does not cover this case.
- **Skirmish maps lose all their map units** (W7-09-05). About 1,700 cows, civilians and cars on the 197 stock maps never spawn. The YR campaign maps cannot be started at all (W7-09-06).

The mission and trigger layer works for what the maps use:
- Sleep, Guard, Area Guard, Hunt and Harvest are honoured.
- The Panic and Unpanic script actions match the RE.
- The ChangeHouse and MakeAlly actions behind the Psychic Beacon and Amplifier work.

Harmless differs from the original in three ways:
- Harmless units can still be targeted (W7-09-07).
- They do not wake when hit (W7-09-08).
- An ownership change does not reset a dormant unit to Guard (W7-09-09).

The fear model's constants match the RE, but these behaviours are missing or wrong:
- the hit-time flee away from the attacker (W7-09-11);
- the human-owner exemption is wrong (W7-09-12);
- sleeping civilians fidget and wander (W7-09-13);
- CIV3 never flees because of an image-keyed table (W7-09-02);
- the cow's wander bias is missing (W7-09-14).

None of the 24 maps has a VIP or Einstein rescue objective, so there is nothing to fix there. Event 18 "Civilians evacuated" and action 15 AllowWin are used by no map [CONFIRMED-DATA].

## Per-unit table

| Type | Where used [CONFIRMED-DATA, C census] | Original behaviour | Clone |
|---|---|---|---|
| CIV1, CIV2 | 36 + 38 placed (ALL03, ALL06-09, SOV01 ×10, SOV04, SOV08); Guard | Fraidycat. Fidgets and wanders in Guard. Steps away from the attacker when hit, then flees in panic. Never auto-targeted (Insignificant). | Spawn and draw OK. Flees when hit (Panic) [CONFIRMED-PLAY]. Gaps: 01, 04, 10, 11, 13 |
| CIV3 | 32 placed, Guard | Same as CIV1 | **02:** never flees [CONFIRMED-PLAY lead + C]. Also 01, 04 |
| CIVBF, CIVBFM, CIVBTM | 23 / 16 / 19 placed, Guard | Same | Draw OK. CIVBFM `Image=BeInvisibleHack` in ALL04U is dropped (15). Also 01 |
| CIVSF, CIVSFM, CIVSTM | 17 / 16 / 15 placed (snow maps) | Same | Draw OK in the snow theater. Also 01 |
| CIVA, CIVB, CIVC, CIVBBP | 1 placed (SOV06); the rest are TaskForce-only | Fraidycat with Pistola | Spawn and draw OK (C7). Also 01 |
| CTECH | 27 placed (ALL08 ×17, ALL10, SOV08) plus TaskForces; `Image=CIV3`, Fraidycat=no | Pistola; goes prone instead of fleeing | OK, but it overwrites CIV3's flags (02). The probe reports it as CIV3 (17). Also 01 |
| PRES, SSRV | ALL03U intro teams only (board-heli script) | Unarmed civilian / Pistola soldier; behaviour is entirely map-driven | Script actions 3/11/14/42 implemented. Draw OK (C7). Also 01 |
| EINS, RMNV, PENTGEN | Not in any of the 24 maps (EINS: YR ALL04DMD only) | No special flags | Draw OK when spawned (C7). Unreachable because YR maps cannot load (06). Also 01 |
| VLADIMIR | SOV08U AI team | Pistola | OK. Also 01 |
| YURI, YURIPR | SOV11 ×4 placed Area Guard plus teams; SOV03/SOV10 teams. YURIPR is YR only | Mind control | Generic mind-control path, not re-audited here. Draw OK |
| CLNT, ARND, STLN, MUMY, WWLF, CIVAN | YR maps only (WWLF unused) | — | Draw OK when spawned (C7). Unreachable (06) |
| COW | 62 placed (ALL05, SOV04, SOV06, SOV10, SOV11), Guard; 218 on MP maps | Unarmed; Crushable=no; moos on click; wanders often (5/11 bias); flees when hit | Grazes and shuffles. `CowAmbient` plays. Gaps: **14** (bias), 01 (CowDie silent, no moo on click), 05 (MP cows missing) |
| ALL (alligator), POLARB | 28 placed (ALL09, ALL11 ×18, SOV02); 7 placed (SOV04, SOV12) | Armed Guard / Area Guard units; normal alliance rules; Crushable=no | Draw OK. Acquire follows the clone's alliance rules. Also 01 |
| JOSH, CAML, DNOA, DNOB | TaskForce (ALL04U JOSH ×5) / YR / MP only | Armed animals (CAML unarmed) | Draw OK. The DNOB silhouette is plausibly correct [SUSPECTED OK]. Also 05 and 06 |
| Civilian cars (CAR, PICK, PTRUCK, STANG, SUVB, SUVW, EUROC, TAXI, LIMO, COP, CONA, TRUCKA, TRUCKB) | About 260 placed, mostly Sleep, some Guard or Sticky; tagged PICK trucks in ALL02, SOV01 and SOV02 | No traffic AI: an unarmed unit idles in Guard and stands still. No flee. Debris TIRE, GenVehicleDie / PoliceCarDie. Only an OmniCrusher crushes them | Parked and drawn OK. Destroying a tagged PICK raises event 48, which creates the crate. Gaps: 01 (GenVehicleDie silent) and 18 (Sticky) |
| BUS | 9 placed (ALL07, ALL11, SOV03 ×7), Sleep | Passengers=5; boarding requires an allied house | `can_load` gates on alliance. OK |
| CRUISE, TUG | ALL11, SOV01 | Insignificant ships | OK (see also 06-naval) |
| JEEP, AMBU, FTRK, DOLY, BCAB, YCAB, DDBX, WINI, CIVP, CBLC | YR maps and MP maps only | — | Not drawn when created by `camp_spawn` (16). Unreachable (05, 06). JEEP's voxel size is suspect (19) |
| Trains | — | Not present in YR | N/A |

## Gaps

### W7-09-01: Map-only types have no voices or death sounds (MAJOR)

**Evidence:**
- [CONFIRMED-PLAY `lead/snd.play`, ALL01T] We spawned and killed E1, CIV1, CIV3, COW, CTECH, EINS and CAR. Only `snd(GIDie)` was played. CivAllFemDie, CivAllDie, CowDie, CivFatDie, EinsteinDie and GenVehicleDie never played.
- `play_at` logs every non-empty key before any visibility check (`pres_audio.inc:355-357`). The keys were therefore empty, not culled.

**Original:**
- Every TechnoType has its own VoiceSelect / DieSound (FUN_00711510).
- `TechnoClass::Select` (0x6FB4C0) also plays VoiceSelect for a unit whose owner's country is MultiplayPassive [CONFIRMED-RE]. Clicking a civilian, cow or Einstein speaks.

**Clone:**
- `Presentation::init` (`engine/pres_game.inc:33-35`) calls `add_voices` only for `ur_.defs` and `ur_ai_.defs` (`main.cpp:6784`).
- Types resolved by the campaign `Types::unit` (`campaign_runtime.inc:146`) are never registered.
- `audio_->voices[u.image]` then default-constructs an empty set.
- The table is also keyed by **image** (`pres_game.inc:768`). After registration, CTECH would collide with CIV3 (CivFatDie vs CivAllDie) and CLNT with CIVC.

**Fix:**
- Store the rules id on `Unit`. This is also the fix for W7-01-01, W7-09-17 and C-5.
- Key `voices` by that id.
- Register lazily, the first time `unit_fx_tick` sees an id, or when `Types::unit` resolves a new type.

### W7-09-02: CIV3 civilians never flee because CTECH overwrites the CIV3 fear table (MINOR)

**Evidence:**
- [CONFIRMED-PLAY `lead/fear.play`] A GI shot CIV3, taking it from 35 to 20 HP. CIV3 stayed `umov=0` and never entered Panic. A CIV1 shot the same way had `umov=1` and Panic within 2 frames.
- Sub-audit C saw CIV3 never move on SOV01T, SOV04S or SOV08U while CIV1 and CIV2 moved.

**Original:** Fraidycat, Fearless and NotHuman are per-InfantryType fields at +0xEBF, +0xEBC and +0xEAD [CONFIRMED-RE LoadFromINI 0x5240E0..0x5243F0].

**Clone:**
- `enable_live_infantry_anims` (`engine/main.cpp:1470-1501`) calls `set_inf_table(image, t)` per image.
- CTECH (`Image=CIV3`, `Fraidycat=no`) comes after CIV3 in the list, so it overwrites CIV3's table.
- `world.cpp:5395-5401` arms each unit from `inf_table(u.image)`.
- CIV3/CTECH is the only shared image whose flags differ [CONFIRMED-DATA].

**Fix:** keep the sequence table per image, and keep the fearless / fraidycat / not_human flags per rules id.

### W7-09-03: The Insignificant auto-target gate is campaign-only (MINOR, latent)

**Original:** [CONFIRMED-RE Evaluate_Object 0x6F7894..0x6F790E]
- A target that is Insignificant and not mind-controlled (+0x2C0 MindControlledBy / +0x2C4) is rejected **in every GameMode** if it is not a building.
- An Insignificant building is rejected when its owner's country is MultiplayPassive. The exception is the infantry occupy/capture path.
- A separate block at 0x6F779E rejects **any** object of a passive house when GameMode ≠ 0. The same exception applies.

**Clone:**
- `engine/world_w6.cpp:32` rejects only when `playfield_.campaign`, and `:43` does the same for buildings.
- A mind-controlled Insignificant unit is wrongly rejected in the campaign.
- Today this is latent in skirmish, because the civilian house is never an enemy (W7-09-10).

**Fix:**
- Drop the campaign condition and add a `!mind_controlled` exemption.
- Apply the building rule in every mode.
- Add the passive-house rejection outside the campaign.

### W7-09-04: The player's crushers kill civilians of allied houses (MAJOR; refines W7-03-01)

**Evidence:**
- [CONFIRMED-PLAY `A/A1-crush-civie.play`, ALL01T] A house-0 MTNK was driven onto a CIV1 and then a CIV2 of house 4 (Civie1, team 104). Both died (`units_lost(4)` 0→1→2).
- [CONFIRMED-DATA] In ALL01T, the Allies lists of [Player House] and all three BadGuy houses include Civie1.

**Original:** `IsCrushable` (0x5F64C0) refuses any victim the crusher's owner is allied with. This holds in both the OmniCrusher branch and the Crushable branch, via `IsAlliedWith` 0x4F98B0 (0x5F650A / 0x5F6557) [CONFIRMED-RE]. Nobody can crush Civie1 civilians in ALL01.

**Clone:**
- `world.cpp:5670` checks only `v.house != u.house`.
- The W7-03-01 fix (`!are_allied`) is not enough on its own. `World::are_allied` (`world.cpp:490`) is false for every non-combatant, and the campaign makes a Neutral house the player lists as an ally a non-combatant (`campaign_runtime.inc:745-747`).

**Fix:**
- Keep each house's map Allies view, including Neutral and Civilian houses, separate from the `combatant` flag.
- Use it in `can_crush` (`combat.cpp:557`), `world.cpp:5670`, and the crush-scatter at `world.cpp:5767`.

### W7-09-05: Skirmish and MP maps never spawn their placed units (MAJOR)

**Evidence:**
- [CONFIRMED-PLAY `C/C8-sk-hillbtwn.play`] hillbtwn places 77 Neutral vehicles. At t0 the probe reads `country(2)="Civilian"` and `units(2)=0`, and `art_dump` lists only GI, GTNK and MCV.
- Neutral structures do spawn (`neutral-world=61`).

**Scale** [CONFIRMED-DATA]:
- 197 stock maps lose about 1,700 civilians, animals and cars: COW 218, CAR 157, PTRUCK 139, and about 190 on XHAILMARY alone.
- The co-op maps C1A01MD and C1A02MD also lose their tagged Neutral combat units.

**Original:** the scenario loader places `[Units]`, `[Infantry]` and `[Aircraft]` in every game mode.

**Clone:** `launch_skirmish` (`main.cpp:13913` onwards) places only the start units and bases. A placer exists only in the trigger-demo path (`main.cpp:15247`) and in the campaign path.

**Fix:**
- After the house setup, run the campaign placer over the map.
- Map Neutral / Special / Civilian to the skirmish Civilian house, and resolve types through `Types`.
- Feed the vehicle assets into `vlist_` (see W7-09-16).
- Honour the placed missions.
- Spawn deterministically, because netplay needs it.

### W7-09-06: YR campaign maps cannot be started (MAJOR if the YR campaign is in scope)

**Evidence:** [CONFIRMED-PLAY `C/C0-yr-all02umd.play`] Loading fails with `map entry not found in Maps01.mix: all02umd.map`.

**Clone:** `campaign_runtime.inc:59-63` searches only MAPS02.MIX (SOV*) and Maps01.mix. It never searches `mapsmd03.mix`. Every YR-only character and car is therefore unreachable: CLNT, ARND, STLN, DNOA/B, CAML, EINS, RMNV, PENTGEN, YURIPR, JEEP, AMBU and others.

**Fix:** map `*md.map` names to mapsmd03.mix, and add the YR sides and mission-select entries.

### W7-09-07: Harmless objects can still be auto-targeted (MAJOR; the fix is cheap)

**Original:** [CONFIRMED-RE 0x6F7331 in Evaluate_Object] A candidate whose current mission has MissionControl NoThreat (+4) is rejected. rulesmd `[Harmless]` sets `NoThreat=yes`.

**Clone:**
- `legal_threat_unit` (`world_w6.cpp:22-34`) never checks the mission.
- `set_dormant` (`campaign_runtime.inc:1310-1313`, `1333-1345`) only stops the unit from shooting.

**Impact** [CONFIRMED-DATA]:
- The only Harmless objects are in ALL09: 2 MTNK and 6 E1 of Other2 House.
- BadGuy3 is not allied with Other2, and it can kill these prisoners before they are freed.

**Fix:**
- Add a no-threat flag, set while the Track mission is Harmless and cleared on wake.
- Reject flagged units in `legal_threat_unit`.

### W7-09-08: A Harmless unit hit by an attacker does not wake (MINOR)

**Original:** [CONFIRMED-RE FootClass::ReceiveDamage 0x4D7250] The unit wakes when all of these hold:
- it was damaged but not destroyed;
- the damage has a source;
- the mission has NoThreat;
- the mission does not have Zombie.

It then goes to `Enter_Idle_Mode` (vt+0x484): Guard or Area Guard, chosen by IQ. Sleep is Zombie, so a Sleep unit never wakes from damage.

**Clone:** only `wake_ordered` and `wake()` (`campaign_runtime.inc:1356`, `1367`) clear the Track mission.

**Fix:** in the damage path, when `mission==2 && attacker>=0 && alive`, clear the mission and call `set_dormant(idx,false)`.

### W7-09-09: ChangeHouse, AllChangeHouse and mind control do not reset a dormant mission (MINOR)

**Original:** [CONFIRMED-RE]
- Action 14 (FUN_006E0010) and action 36 (FUN_006E00D0) call vt+0x3D4.
- That runs 0x7451D0, then 0x4DBDE0, then `SetOwningHouse` 0x700940.
- `SetOwningHouse` clears the target and destination and calls `Assign_Mission(Guard)`. The exceptions are a selling unit and a harvester in Unload.

**Clone:**
- `set_unit_owner` (`unit_orders.cpp:70-75`) only stops the unit.
- Actions 14 and 36 (`campaign_runtime.inc:3032-3064`) keep the dormant flags.

**Impact:**
- The ALL09 POWs freed by action 14 stay passive until the player orders them.
- Armed Sleep units the player mind-controls also stay dormant: ALL01 V3 ×3, SOV05 HTK, SOV10 DRED and PTROOP.

**Fix:** on actions 14 and 36 and on mind-control capture, set `t.mission=0` and call `set_dormant(i,false)`.

### W7-09-10: Neutral and civilian units cannot be clicked: no attack, no select, no voice (MINOR)

**Original:** [CONFIRMED-RE `What_Action` 0x6FF380, 0x6FF87D..0x6FF8FE; `Select` 0x6FB4C0]
- A plain click gives ATTACK when the target is not allied, is visible and is a LegalTarget. Insignificant is not checked here.
- An allied selectable target gives SELECT.
- Any discovered object can be selected. VoiceSelect plays for the player's own units and for passive-country owners.
- In skirmish the civilian house is not allied with the players [SUSPECTED: the passive-house branch in Evaluate_Object exists for exactly this case]. A click on a skirmish civilian, cow or car is therefore an attack. In the campaign, an allied civilian is selected and speaks.

**Clone:**
- `world.cpp:531` makes a neutral house never an enemy.
- `input_original.cpp:140` / `:209` only select controllable units.
- A click on a neutral unit is a ground move. Only Ctrl force-fire attacks.

**Fix:**
- Resolve the click through the per-house Allies view (W7-09-04): not allied means Attack, allied means a view-only selection.
- Play VoiceSelect when the owner is passive. This needs W7-09-01 first.

### W7-09-11: No flee away from the attacker on damage (MINOR)

**Original:** [CONFIRMED-RE 0x518A6C in FUN_00517DB5]
- Every hit with an attacker calls `Scatter(attackerCoord, 0, 0)` before the fear update.
- The unit moves away from the attacker, with random jitter, to the first free cell of 8 rotations. It then queues Move, which wakes a sleeper.
- The scatter is blocked when the mission's Scatter flag is off (Sleep, Attack, Sticky). It is also blocked for a non-Fraidycat unit that has a target, and for an unforced call when PlayerScatter=no.

**Clone:**
- There is no hit-time scatter.
- `order_scatter` (`unit_orders.cpp:87-111`) always moves in a random direction.

**Fix:** add `order_scatter_from(idx, x, y)` and call it from the damage paths (`combat.cpp:252`, `:359`, `:710`), with the mission and flag gates above.

### W7-09-12: The fear-AI scatter wrongly exempts human-owned units, and "human" ignores PlayerControl (MINOR)

**Original:** [CONFIRMED-RE FUN_0051FDE0 0x51FF0C]
- The Fraidycat fear scatter has no owner test. Its only extra gate is `!IsFallingDown`.
- The idle and prone tests use `IsControlledByHuman` (0x50B550), which includes PlayerControl in the campaign.

**Clone:**
- `inf_anim.cpp:181-182` adds `!c.human`.
- `world.cpp:5407` takes `c.human` from `is_human`.
- `inf_anim.cpp:174` also excludes Cheer, which the original does not.

**Fix:**
- Drop `!c.human` from the fear scatter.
- Use `human_commanded(u.house)`.

### W7-09-13: Sleeping infantry fidget and wander, and the wander wakes them (MINOR)

**Original:** [CONFIRMED-RE] The idle routine FUN_0051CB80 is called only from three missions:
- Mission_Guard 0x4D4F90;
- Mission_AreaGuard 0x4D69C0;
- Mission_Hunt 0x4D5270.

A unit placed with Mission=Sleep stands still until it is hit.

**Clone:**
- The idle machine runs in every mission.
- The idle scatter becomes `order_unit_move`.
- `wake_ordered` (`campaign_runtime.inc:1356-1364`) then wakes the dormant unit.

**Fix:**
- Add `idle_allowed` to `InfTickCtx`. It is true only when the unit is not dormant.
- Gate `idle_action` on it, and keep the fear scatter ungated.

### W7-09-14: The cow's wander bias is missing (COSMETIC)

**Original:** [CONFIRMED-RE 0x51CC6A..0x51CC98] For type ID "COW", idle roll 8 (turn and scatter, any owner) is forced with probability 5/11.

**Clone:** `inf_anim.cpp:127-146` has no such case.

**Fix:** add a per-type `is_cow` flag to the infantry table.

### W7-09-15: The ALL04 `Image=BeInvisibleHack` civilian is dropped (MINOR; R-035 confirmed)

**Evidence:** [CONFIRMED-PLAY `C/C6-all04u-r035.play`] The log shows `unit type CIVBFM unavailable (... BeInvisibleHack.shp)`. In the original this is the TaskForce "1BeachMaleHack" team (script BeAsleep), reinforced by PlayerLoss1a (action 80).

**Fix:** register the type as a non-rendered object instead of throwing (`campaign_runtime.inc:151-163`).

### W7-09-16: Vehicle types resolved after setup are never drawn (MINOR; harness-only today)

**Evidence:** [CONFIRMED-PLAY `C/C4*`]
- `[render] no SHP loaded for image=JEEP|AMBU|...`.
- TRUCKB draws when the map places it, but not when it is created with `camp_spawn`.

**Clone:**
- `vlist_` is copied once from `campaign_->vehicle_assets()` (`main.cpp:7667`).
- `Types::unit` (`campaign_runtime.inc:201-210`) keeps adding to `vas_` afterwards.
- Real missions are safe because TaskForce types are pre-resolved (`campaign_runtime.inc:330-332`). W7-09-05 would hit this.

**Fix:** refresh `vlist_` when `Types` grows.

### W7-09-17: The probe and the AI team type are keyed by image (MINOR)

**Evidence:**
- [CONFIRMED-PLAY C3/C5] `play_unit_id` (`main.cpp:7400`) reports CTECH as CIV3 and the civilian TRUCKA as DTRUCK.
- `World::ai_unit_type_of` (`world.cpp:3813`) uses the same mapping. An AI-owned civilian TRUCKA could therefore be recruited as a Demolition Truck team member [SUSPECTED].

**Fix:** use the rules id on `Unit` (see W7-09-01).

### W7-09-18: Mission=Sticky is treated as Guard (COSMETIC)

**Original:**
- [CONFIRMED-DATA] `[Sticky]` sets `Paralyzed=yes`, `Scatter=no` and `Recruitable=no`.
- [CONFIRMED-RE] Enter_Idle_Mode 0x737990 and Scatter 0x742880 honour these flags.

**Clone:** `place_unit` (`campaign_runtime.inc:1306-1325`) ignores Sticky.

**Impact:** only unarmed cars of non-recruiting civilian houses use Sticky (ALL04, SOV06, SOV08), so there is no effect today.

**Fix:** add a Sticky track mission that is excluded from recruiting.

### W7-09-19: The JEEP voxel loads at 7×3×3 (COSMETIC) [SUSPECTED]

Every other car is about 40×18 or larger, so this may be a wrong section. JEEP cannot be checked in play until W7-09-05 or W7-09-06 is fixed.

## Verified OK

**Spawn, draw and idle behaviour** [CONFIRMED-PLAY C1-C5, C7]:
- All 24 RA2 campaign maps resolve every placed type. Every type exists in YR rulesmd [CONFIRMED-DATA], and the only drop is W7-09-15.
- Placed counts equal live counts per house on SOV01T, ALL07T, SOV04S, ALL11T and SOV08U.
- Every civilian SHP and voxel draws with the correct palette, remap and theater, snow and urban included.
- Every campaign character, every YR infantry type and the animals draw when spawned.
- Parked cars stay put. Civilians play their Idle1 and Idle2 fidgets. Cows graze. `CowAmbient` and `PoliceSirenLoop` play.

**Fear model** [CONFIRMED-RE FUN_00517DB5 / FUN_0051FDE0 / 0x51D6CA]. The constants match `inf_anim.cpp`:
- The first hit sets fear to 100, or to 300 for a Fraidycat unit.
- Later hits add 50, 25 or 12 according to the condition, capped at 300.
- Fear drops by 1 per frame unless the unit is Fearless.
- A non-Fraidycat unit goes prone above 49 and stands up below 50.
- A Fraidycat unit scatters above 50.
- The Panic walk starts at fear 200 or more.
- Apart from the cow case, the idle roll mapping and the idle gate (FUN_0051CB80 / 0x521400) also match.
- Fraidycat CIV1 flees in Panic when hit [CONFIRMED-PLAY lead/fear.play].

**What triggers fear** [CONFIRMED-RE: full scan of the writes to +0x6D4]. Fear is set only by damage, an empty magazine, and Panic / Unpanic. Seeing an enemy or a near-miss explosion causes no fear, so the clone is correct not to model them.

**`Civilian=` flag** [CONFIRMED-RE: its three reads are 0x413CF9, 0x41934F and 0x6C749A]:
- It is used only by the evacuation-aircraft logic and a per-house stats count.
- It has no combat, scatter or score effect, so the clone not reading it is harmless.
- The evacuation event (18) is used by no map.

**Script actions** [CONFIRMED-RE 0x6ED2C0 / 0x6ED1D0, 0x521940 / 0x521970]: Panic 19 and Unpanic 41 match `campaign_runtime.inc:2696-2705`.

**NotHuman deaths** [CONFIRMED-RE 0x518317 / 0x520972]: a NotHuman unit always plays Die1 and leaves no generic corpse. Humans leave a random `[General] DeadBodies` corpse.

**Animals** [CONFIRMED-DATA]: Crushable=no is read with the infantry default of true (`weapon_resolve.cpp:154`).

**Civilian vehicles:**
- [CONFIRMED-RE 0x737990] There is no traffic or wander AI. An unarmed unit idles in Guard and stands still.
- [CONFIRMED-RE 0x736D60 / 0x4D7250 / 0x700DA0] A damaged vehicle never flees.
- [CONFIRMED-DATA] Debris and DieSound are handled generically (but see W7-09-01).
- Only an OmniCrusher crushes them.
- BUS boarding is gated on alliance (`cargo.cpp:31-33`).
- The tagged PICK trucks raise event 48, which runs action 108 CreateCrate.

**Placed missions:** Sleep (no acquire, no retaliate, no wake on damage), Guard, Area Guard (W6-02-11), Hunt and Harvest are honoured. The wake actions 81-83 exist.

**Psychic Beacon and Amplifier** [CONFIRMED-DATA]:
- NAPSYB and NAPSYA have no weapon and no superweapon.
- Their "mind control" is done by the map triggers with actions 14, 36 and 37 (ALL03, ALL04, ALL08, SOV08), and the clone implements those actions (`campaign_runtime.inc:3032-3086`).
- The only gap is W7-09-09.

**VIP and escort** [CONFIRMED-DATA]:
- PRES and SSRV appear only in the ALL03 intro teams, using script actions 3, 11, 14 and 42, which are implemented.
- RMNV, EINS and PENTGEN appear in none of the 24 maps.
- Event 18 and action 15 are used by no map.
- Every civilian-related event and action that the maps do use is handled.

**Insignificant civilian buildings** of passive houses are not targets in the campaign (W6-02-07).

**Trains:** not present in YR, so there is nothing to implement.
