# Wave 7 / 03: Allied ground vehicles: behaviour gap matrix

**Tree:** master `b3a509f` (read-only audit; no engine edits; `D:\ra2-android\build\ra2engine.exe` as built).

**Scope:** every Allied-owned `[VehicleTypes]` entry in YR `rulesmd.ini` that drives on land:

| Unit | Rules id |
|---|---|
| Grizzly | MTNK |
| IFV | FV |
| Mirage Tank | MGTK |
| Prism Tank | SREF |
| Tank Destroyer | TNKD |
| Robot Tank | ROBO |
| Battle Fortress | BFRT |
| Chrono Miner | CMIN (unloading class CMON) |
| Allied MCV | AMCV |

Three more Allied vehicle types are covered only briefly, because wave 6 part C owns them:
- the Allied amphibious transport (`[LCRF]`, "Landing Craft", a hover VehicleType);
- the Night Hawk (`[SHAD]`, a jumpjet VehicleType);
- the ships.

The non-buildable `HOWI`, `TRUCKA/B`, `CDEST` and `XCOMET` are out of scope (TechLevel -1).

**Method (calibrate-first).** Evidence came from four sources:
- **Data:**
  - `rulesmd.ini` (`D:\ra2-re\out\shroud_evidence\rulesmd.ini`);
  - `artmd.ini` (`D:\ra2-re\out\roster2\artmd.ini`);
  - `aimd.ini`;
  - a voxel census of `ra2.mix/local.mix` and `ra2md.mix/localmd.mix`, done with `build\ra2extract.exe --raw`.
- **RE:** Ghidra 11.2.1 on retail `gamemd.exe` (YR 1.000). I used a private copy of the w6fix-units project, `D:\ra2-re\w7-av`. Its scripts are in `D:\ra2-re\w7-av-scripts` (dec, dis, ctx, strref, opscan) and its raw output is in `D:\ra2-re\w7-av-out`. Key listings are copied to `03-evidence/ghidra-*.txt`.
- **Code:** reading `engine/*`.
- **Play:** two `--play` runs from PowerShell, using my own scripts in `03-evidence/`:
  - `W7-03-crush-all01.play`: campaign ALL01T, real houses and alliances;
  - `W7-03-skirmish.play`: skirmish at `slider:speed=2`, so one driver frame is one logic frame.

  The step dumps are `03-evidence/*-result.txt`.

**Prior reports.** I read wave 6 `04-units.md` (with parts A to D and the fix-status table) and `REMAINING.md`. Items they record as FIXED are not reported again. They are listed under "Verified OK" or cross-referenced only when this audit re-checked them.

**Tags:**
- `[CONFIRMED-RE <addr>]`: read in the `gamemd.exe` disassembly or decompile.
- `[CONFIRMED-DATA]`: read from `rulesmd`, `artmd`, `aimd` or the MIX archives.
- `[CONFIRMED-PLAY]`: measured in a `--play` run on b3a509f.
- `[CONFIRMED-CODE]`: read in the clone source. This is the only evidence for some clone-side claims.
- `[SUSPECTED]`: not proven.

## Summary

**13 gaps, none a BLOCKER:** 4 MAJOR, 7 MINOR (one is MINOR/RE-needed) and 2 COSMETIC. Two wave-6 items are still open; they are cross-referenced below with no new ID.

1. **Crushers squash allied infantry** (W7-03-01, MAJOR). This is the user-reported bug.
   - In play, a player Grizzly drove onto an Engineer of house 5 and an Engineer of house 7. Both houses are team 0, allied to the player. Both Engineers died (`units_lost(5)=1`, `units_lost(7)=1`). `[CONFIRMED-PLAY]`
   - The victim test is `v.house != u.house` (`engine/world.cpp:5670`). `[CONFIRMED-CODE]`
   - The original's crushability check is `ObjectClass::IsCrushable` FUN_005F64C0. It calls `HouseClass::IsAlliedWith` (FUN_004F98B0) with `ECX = crusher->Owner` (0x5F650A / 0x5F6557) and refuses allied victims. `[CONFIRMED-RE]`
   - Allied infantry are also never asked to scatter out of the way (`world.cpp:5767`, own house only).
2. **Turretless vehicles fire without turning to face the target** (W7-03-02, MAJOR). This affects the Tank Destroyer and the Mirage Tank.
   - The sim slews an invisible `turret_facing` and fires once it is aimed. `body_facing` never moves.
   - In play, the TNKD killed a Rhino 90° off its nose with `sel_body=32768` throughout. `[CONFIRMED-PLAY]`
3. **`OpportunityFire=yes` is ignored** (W7-03-03, MAJOR). The Grizzly and the Robot Tank never shoot while driving.
   - The clone has a hard "no auto-acquire while moving" rule (`combat.cpp:1142`).
   - In the original, TechnoType +0x6AF bypasses the Guard-mission-only scan gate in FUN_00708710 @0x70885F. `[CONFIRMED-RE]`
4. **The Mirage Tank breaks its disguise on every shot** (W7-03-04, MAJOR).
   - The original never undisguises on firing. UnitClass::Fire FUN_00740150 only starts a `DisguiseFakeBlinkTime` (15, elite 5) blink timer when the type has DisguiseWhenStill (+0xD32). An undisguise happens on movement, and when a non-allied infantry stands in an adjacent cell (UpdateDisguise FUN_007456F0, `InfantryBlinkDisguiseTime`=20). `[CONFIRMED-RE]`
   - In play, the clone was fully visible and auto-targetable for about 16 frames after each shot. `[CONFIRMED-PLAY]`
5. **Presentation gaps:**
   - The IFV never swaps its turret voxel: `fvtur1..3` exist, and only `fvtur` is loaded (W7-03-05).
   - IFV passenger modes and elite weapons play the image primary's Report sound and muzzle anim (W7-03-06).
   - The Prism Tank charge turret (`IsChargeTurret`, `sreftur1..3`) is not drawn (W7-03-07).
6. **The skirmish AI never builds the Robot Tank or the Battle Fortress** (W7-03-08). Both are absent from `kAiUnits`, while stock `aimd.ini` TaskForces name BFRT ×4 and ROBO ×2.
7. **Still open from wave 6:**
   - **W6-04-34:** the Chrono Miner still warps back to the ore field after unloading (`harvest.cpp:572-576`).
   - **W6-04-31:** the Mirage `EliteSecondary` quirk.
   - **Robot Tank sounds** (W6-04-27 residue, now W7-03-10). Activate, Deactivate and `VoiceSelectDeactivated` are still not played. They are now unblocked, because `Unit::w6.unpowered` exists.

## Per-unit matrix

`OK` = verified (tag given). `OK?` = code and data reading only. `WRONG` / `MISSING` = a gap.

| Unit | Stats / keys read | Targeting / acquire / guard | Movement / crush | Turret / facing | Signature mechanic | Transport | AI use | Presentation / sound |
|---|---|---|---|---|---|---|---|---|
| **MTNK** Grizzly | OK? Strength 300, heavy, Speed 7, ROT 5 (turret slews at hull ROT, voxel-render.md §5), 105mm / 105mmE (Burst 2 fixed W6-04-01), VeteranAbilities, Elite SELF_HEAL `[CONFIRMED-CODE world_veterancy.cpp:194-201]` | **WRONG:** `OpportunityFire=yes` not read; no fire on the move (W7-03-03) | **WRONG:** crushes allied infantry (W7-03-01) `[CONFIRMED-PLAY]`. Crusher, AutoCrush steering (R-050) OK? | OK? Turret slew at ROT, aim gate. Idle return is immediate (W7-03-11, cosmetic). No recoil is correct: `TurretRecoil` is set only on GTGCAN / CAEAST02 `[CONFIRMED-DATA]` | N/A | N/A | OK (`kAiUnits`) | OK? Voices, GrizzlyTankMoveStart, TankCrush. The elite VTMUZZLE / VTEXPLOD are not shown (W7-03-06) |
| **FV** IFV | OK? Strength 200, Speed 10, Crusher=no, Passengers 1 / SizeLimit 1, AirRangeBonus 4 (fixed W6-04-16), HoverMissile Burst 2 / elite 4 (fixed). DeathWeapon=CRNuke is correctly inert (Explodes not set; `world_abilities.cpp:475`) | OK. All IFVMode swaps (W6 measured 7; mapping weapon index = IFVMode, `abilities.cpp:176`, matches `[FV] WeaponN`) `[CONFIRMED-DATA]` | OK? Not a crusher | **WRONG (art):** turret voxel never changes per mode (W7-03-05) | Gunner swap OK; veterancy with a passenger fixed (W6-04-28) | OK. A GI loads (`in_cargo=1`, `cargo_total(0)=1`) `[CONFIRMED-PLAY]` | OK | **WRONG:** passenger-mode Report and muzzle come from HoverMissile (W7-03-06). IFVTransform enter / leave OK? |
| **MGTK** Mirage Tank | OK? Strength 200, light, Speed 7, Turret=no, CanApproachTarget=no (fixed W6-04-30) | OK? A disguised Mirage is skipped by enemy auto-acquire (`combat.cpp:1182`) | Crusher; same W7-03-01 bug | **WRONG:** Turret=no hull never turns to the target (W7-03-02) | **WRONG:** reveals on every shot; no fake-blink, no adjacent-infantry reveal, no DisabledDisguiseDetectionPercent (W7-03-04) `[CONFIRMED-RE + PLAY]`. Tree draw for enemies fixed (R-047). Elite quirk still open (W6-04-31) | N/A | OK (`kAiUnits`) | OK? MirageTankAttack |
| **SREF** Prism Tank | OK? Strength 150, Speed 4, MovementZone Destroyer, Weapon1 Comet / EliteWeapon1 SuperComet (TurretCount 4) | OK? Range 10. Shrapnel ×5 on the projectile (fixed W6-04-02; fragment targeting still W2-SK-22 RE-FALLBACK) | Crusher; same W7-03-01 bug | OK? Turret slew | **MISSING (art):** IsChargeTurret charge turrets (W7-03-07) | N/A | OK | OK? House-colour laser `[CONFIRMED-CODE pres_fx.inc:189-193]` |
| **TNKD** Tank Destroyer (Germans) | OK? Strength 400, heavy, Speed 5, Turret=no, SABOT / SABOTE (Burst fixed), RequiredHouses=Germans `[CONFIRMED-CODE ai_side_catalog.inc:84]` | OK. Kills a Rhino `[CONFIRMED-PLAY]` | Crusher; same W7-03-01 bug | **WRONG:** fires with the hull 90° off (W7-03-02) `[CONFIRMED-PLAY]` | N/A | N/A | OK | OK? |
| **ROBO** Robot Tank | OK? Strength 180, Speed 10, Hover + AmphibiousDestroyer (amphibious domain), PoweredUnit stop (fixed W6-04-18, RE-FALLBACK), ImmuneToPsionics (mind-control path `campaign_runtime.inc:1661`, auto-scan `combat.cpp:1184`), ImmuneToRadiation | **WRONG:** OpportunityFire=yes ignored (W7-03-03) | Crusher; same W7-03-01 bug | OK? | PoweredUnit: OK? stop / freeze while no powered GAROBO or low power | N/A | **MISSING:** not in `kAiUnits` (W7-03-08) | **MISSING:** ActivateSound / DeactivateSound / VoiceSelectDeactivated (W7-03-10) |
| **BFRT** Battle Fortress | OK? Strength 600, Speed 4, Passengers 5 / SizeLimit 2, OpenTopped, OmniCrusher + OmniCrushResistant (fixed W6-04-14), OpenTransportWeapon (fixed W6-04-15) | OK (W6 measured passengers firing out) | OmniCrusher crushes vehicles; same alliance bug (W7-03-01; the original OmniCrusher branch is ally-gated too) | N/A (no turret) | Passengers fire out OK | OK (size sum fixed W6-04-08) | **MISSING:** not in `kAiUnits` (W7-03-08) | **MISSING:** AlternateFLH0..4 gun ports unread (W7-03-12, cosmetic) |
| **CMIN** Chrono Miner | OK? Strength 1000, Storage 20, Teleporter, AutoCrush, SelfHealing, UnloadingClass CMON | N/A (Primary none) | AutoCrush steering applies to the player's CMIN (R-050) OK?; same alliance bug (W7-03-01) | N/A | **STILL OPEN W6-04-34:** warps back to the field after unloading (`harvest.cpp:572-576`, `617-638`) `[CONFIRMED-CODE]`; the original drives to ore `[SUSPECTED]` | N/A | OK | OK? ChronoMinerTeleport, VoiceHarvest / VoiceEnter |
| **AMCV** | OK? Strength 1000, Speed 4, DeploysInto GACNST, OmniCrushResistant | N/A | Crusher; same alliance bug | N/A | Deploy / undeploy OK (mcv-deploy.md, FUN_007383E0 / FUN_00737D50 / FUN_0044F5C0) | N/A | OK (redeploy) | OK? PlaceBuilding |
| LCRF / SHAD (brief) | See W6 part C | – | – | – | – | Capacity by size sum fixed (W6-04-08) | OK | – |

## Gap entries

### W7-03-01: Crushers kill allied infantry, and allied infantry never scatter for a friendly vehicle (MAJOR)

| Field | Details |
|---|---|
| **Player** | <ul><li>A Grizzly (or any Crusher: IFV excepted, Prism Tank, Tank Destroyer, Mirage, Robot Tank, Battle Fortress, Chrono Miner, MCV) driving through allied infantry kills it.</li><li>This happens in co-op and allied skirmish, and in campaign maps where friendly forces belong to an allied house (ALL01's `Other3` GIs, house 7; the police and dogs of house 5). This is the user's "Grizzly crushed allied engineers".</li><li>The Battle Fortress's OmniCrusher also squashes allied *vehicles*.</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-PLAY]` `03-evidence/W7-03-crush-all01.play` on ALL01T: houses 5 and 7 have `team=0`, the same team as the player. One `MTNK` of house 0 was moved onto an `ENGINEER` of house 0, house 5 and house 7 in turn. Result: the house-0 engineer survived; house 5 went `units_lost 0 → 1`; house 7 went `0 → 1` (`W7-03-crush-all01-result.txt`).</li><li>`[CONFIRMED-CODE]` `engine/world.cpp:5662-5680`: the step-into-cell crush kills `if (can_crush(u, v) && v.house != u.house && ...)`. `engine/combat.cpp:557-566` `can_crush` has no alliance test. `world.cpp:5762-5770` asks only `units[o].house == u.house` infantry to scatter.</li></ul> |
| **Original** | <ul><li>`[CONFIRMED-RE 0x5F64C0]` `ObjectClass::IsCrushable(TechnoClass* crusher)`:<ol><li>OmniCrusher branch: crusher type +0xD29 OmniCrusher, victim type +0xD2A OmniCrushResistant == 0, victim not a building (WhatAmI != 6).</li><li>Normal branch: victim ObjectType +0x22D Crushable, and victim +0x2A4 == 0.</li></ol>**Both** branches then require `!crusher->Owner->IsAlliedWith(victim)`: `MOV ECX,[EDI+0x21C]; PUSH ESI; CALL 0x4F98B0` at 0x5F650A and 0x5F6557.</li><li>`[CONFIRMED-RE 0x4F98B0]` `HouseClass::IsAlliedWith(Techno)` is true for:<ul><li>the same house;</li><li>the same ArrayIndex;</li><li>a bit set in the house's `Allies` mask +0x5788.</li></ul>This is the crusher's own view, so it can be one-sided.</li><li>IsCrushable is called by:<ul><li>ShouldCrush FUN_00742720;</li><li>the attack mission FUN_007402F0;</li><li>the drive locomotor FUN_004B0E50 (0x4B1841);</li><li>the unit cell-entry / action code FUN_0073DFA0 (0x73E98C, 0x73EB5C) and FUN_0073EB90.</li></ul>An allied infantry is therefore neither crushed nor auto-crush-targeted. The vehicle waits for it to move, and in the TS-lineage Can_Enter_Cell the occupant is asked to scatter `[SUSPECTED for the scatter half; the clone's own comment at world.cpp:5760 cites the same rule]`.</li><li>Neutral (civilian) infantry is *not* allied, so it stays crushable. The fix must not use `is_enemy`, which treats neutrals as never-enemy.</li></ul> |
| **Fix** | <ul><li>`world.cpp:5670`: replace `v.house != u.house` with `!are_allied(u.house, v.house)`. `are_allied` already reads the per-pair view matrix (`set_ally_view`), which is the crusher-side `IsAlliedWith`.</li><li>Do the same in the OmniCrusher path, which uses the same call site.</li><li>`world.cpp:5767`: use `are_allied(u.house, units[o].house)`, so that allied idle infantry scatter like own infantry.</li><li>Optionally move the alliance test into `can_crush` (pass `World&`) so every caller gets it. The R-050 steering in `combat.cpp:1273-1276` targets enemies only, so it is safe today.</li></ul> |
| **Effort** | S |

### W7-03-02: `Turret=no` vehicles fire without rotating the hull to the target (MAJOR)

| Field | Details |
|---|---|
| **Player** | <ul><li>A Tank Destroyer or Mirage Tank shoots sideways or backwards. It never turns to face what it kills.</li><li>It also never pays the turn time the original charges: a turretless unit must rotate its whole hull at ROT 5 before firing. That makes it stronger than the original against flankers.</li><li>The same code affects the Soviet V3 launcher (`Turret=no`), which is out of this slice.</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-PLAY]` `W7-03-skirmish.play` T1: TNKD at (75,28), Rhino spawned at (79,28) (east). `sel_turret` went 32768 → 26368 → 18688 → 16384 while `sel_body` stayed 32768 for the whole fight. The Rhino went 400 → 250 → 100 and died (`W7-03-skirmish-result.txt`, `03-evidence/t1-tnkd.png`).</li><li>`[CONFIRMED-CODE]`:<ul><li>`world.cpp:105-145` `step_turrets` slews `turret_facing` for **every** vehicle.</li><li>The aim gate `combat.cpp:1301` (and `961`, `1072`) tests `turret_facing`.</li><li>Nothing turns `body_facing` toward a target when the unit is stationary. The only `body_facing` writers are movement (`world.cpp:5624`), eject, flight and MCV.</li><li>The renderer draws a turretless hull at `body_facing` (`main.cpp:3466-3470`), so the "turret" that aimed is invisible.</li></ul></li></ul> |
| **Original** | <ul><li>`[CONFIRMED-RE 0x735A60]` The UnitClass turret AI FUN_00735A60 branches on TechnoType `Turret` +0xCA1 (0x735A9C).</li><li>For `Turret=0`, with a target, no Destination (+0x5A4) and a locomotor that is not moving, it sets the desired facing to the target direction (`FUN_004C9140`, FacingClass::Set_Desired). The `Turret=1` path instead drives the turret facing.</li><li>The fire-facing check then uses the facing that exists. That is the hull for a turretless unit (the voxel-render.md §5 FireError::FACING rule).</li><li>`[SUSPECTED]` which FacingClass (Primary +0x388 / Secondary +0x3A0) the `Turret=0` branch writes: ECX was not recovered in the decompile. The visible behaviour (the TD and Mirage turn to fire) is long-standing RA2 behaviour.</li></ul> |
| **Fix** | <ul><li>Add `Unit::has_turret` (rules `Turret=`, already resolved as `va.combat.turret` in `main.cpp:1642`).</li><li>In `step_turrets`, for `!has_turret` units that are stationary and have a target, turn `body_facing` with `turn_toward(..., rot_u16_per_sec*dt)` and keep `turret_facing = body_facing`.</li><li>Leave the aim gate as it is: it then waits for the hull.</li><li>Moving turretless units keep their path facing, so they fire only when the path happens to face the target. That is the original Drive-locomotor behaviour [SUSPECTED].</li></ul> |
| **Effort** | S |

### W7-03-03: `OpportunityFire=yes` is not read; no unit fires while driving (MAJOR)

| Field | Details |
|---|---|
| **Player** | <ul><li>A Grizzly column driving past enemy infantry or tanks holds fire until it stops. The original Grizzly (and the Rhino, Robot Tank and War Miner) shoots on the move.</li><li>This blunts the Allies' main battle tank in every engagement that starts while moving, and changes how harass and retreat micro plays.</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-CODE]` `combat.cpp:1131-1143`: "No auto-acquire while under a move order … `if (u.moving() \|\| …) { /* no auto-acquire */ }`".</li><li>`git grep OpportunityFire engine` finds 0 readers.</li><li>`[CONFIRMED-DATA]` OpportunityFire=yes is set on: MTNK, ROBO (Allied); HTNK, HARV (Soviet). It is not set on FV, MGTK, SREF, TNKD, BFRT, CMIN, AMCV.</li></ul> |
| **Original** | <ul><li>`[CONFIRMED-RE 0x713BDD]` TechnoTypeClass::LoadFromINI FUN_00711510 stores `OpportunityFire` at type +0x6AF.</li><li>`[CONFIRMED-RE 0x70885F]` In TechnoClass FUN_00708710 (the target-scan permission), if `type->OpportunityFire` is set it takes the "may scan" path (0x7088CF). Otherwise it returns false unless `CurrentMission (+0xAC) == 5` (Guard).</li><li>So a non-OpportunityFire unit scans only while guarding (the clone's current rule), and an OpportunityFire unit also scans under Move.</li></ul> |
| **Fix** | <ul><li>Resolve `OpportunityFire` into `UnitCombat` / `Unit`.</li><li>In `combat.cpp` (e), allow the auto-acquire scan while `u.moving()` when the flag is set.</li><li>Such a target must not replace the move: keep the path and fire only when in range and aimed. This needs a "fire-at-will" target that does not trigger the approach or guard-leash branches (`combat.cpp:1305-1349`). The turret already slews independently.</li></ul> |
| **Effort** | M |

### W7-03-04: The Mirage Tank's disguise is dropped on every shot; the original fake-blinks (MAJOR)

| Field | Details |
|---|---|
| **Player** | <ul><li>A Mirage ambush gives itself away completely. After each shot it is a plain tank for about 16 frames, and every enemy in range auto-targets it.</li><li>In the original it stays a tree. It only "fake-blinks" (shows the tank for `DisguiseFakeBlinkTime` = 15 frames, 5 when elite). AI units get a small per-unit chance (`DisabledDisguiseDetectionPercent=15,5,2` by difficulty) to notice the blink.</li><li>Conversely, the clone never reveals a Mirage when enemy infantry walks up next to it. The original does, for `InfantryBlinkDisguiseTime=20` frames ("a logic blink so others will join in the shooting").</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-PLAY]` `W7-03-skirmish.play` T2:<ul><li>The Mirage was disguised (`udisguised(0,MGTK)=1`) after standing still.</li><li>An enemy Conscript was spawned about 5 cells away, and the Mirage shot it (125 → 25).</li><li>`udisguised` read 0 at f=135, 139, 143 and 147, and 1 again at f=151, at a 1:1 speed.</li></ul></li><li>`[CONFIRMED-CODE]` `world.cpp:2490-2517` `step_disguises`: `if (u.moving() \|\| u.fire_flash > 0.0 …) { … u.disguised = false; }` and re-disguise `if (++u.still_frames >= 15 …)` (RE-FALLBACK 15).</li><li>`DisguiseFakeBlinkTime`, `RevealOnFire`, `InfantryBlinkDisguiseTime` and `DisabledDisguiseDetectionPercent` have 0 readers.</li></ul> |
| **Original** | <ul><li>`[CONFIRMED-RE 0x740150]` UnitClass::Fire wrapper FUN_00740150: after a shot, if the type has `DisguiseWhenStill` (+0xD32), it sets a timer at unit +0x1EC..+0x1F4 to the weapon's `DisguiseFakeBlinkTime` (WeaponType +0x13C; loader 0x7710A0). It does **not** clear the disguise.</li><li>`[CONFIRMED-RE 0x7456F0]` UpdateDisguise FUN_007456F0:<ul><li>a moving locomotor clears the disguise (vt+0x470);</li><li>every 8 frames it looks in the 8 adjacent cells for an infantry (CellClass::FindInfantry FUN_0047EB50, WhatAmI 0xF) that is **not allied** (FUN_004F98B0). If one is found it clears the disguise and starts a re-disguise delay at +0x1E0..+0x1E8 = Rules +0x1014 = `InfantryBlinkDisguiseTime` (loader 0x6717C9);</li><li>when still, DisguiseWhenStill and the delay has expired, it disguises **at once** as `DefaultMirageDisguises[Random(0, n-1)]` (Rules +0xFFC / +0x1008).</li></ul></li><li>`[CONFIRMED-RE 0x6FD3C7]` `RevealOnFire` (WeaponType +0x137, default yes, ctor 0x770BDB) only gates a MapClass::Reveal around the shooter. It is a shroud reveal, not a disguise rule. MirageGun has `RevealOnFire=no`, so the Mirage also does not reveal shroud when it fires.</li><li>`[SUSPECTED]` The consumer of `DisabledDisguiseDetectionPercent` (Rules list, loader 0x66FC4C) and the `FUN_0065A770(0)==0` precondition were not traced.</li></ul> |
| **Fix** | <ul><li>Replace the `fire_flash` clause with a `fake_blink_until` frame (weapon DisguiseFakeBlinkTime, elite-aware). The presentation draws the real tank for enemies while it runs; auto-acquire stays blocked.</li><li>Add the every-8-frames adjacent non-allied-infantry reveal, with an `InfantryBlinkDisguiseTime` re-disguise delay.</li><li>Drop the 15-frame still delay: disguise immediately when the delay timer is clear.</li><li>Optionally, the AI detection roll during a blink (after RE of the detection-percent consumer).</li></ul> |
| **Effort** | M |

### W7-03-05: The IFV never changes its turret model (MINOR)

| Field | Details |
|---|---|
| **Player** | An IFV with an Engineer (repair arm), a GI (machine gun) or a Tesla Trooper looks exactly like an empty missile IFV, so the player cannot read the mode from the field. |
| **Evidence** | <ul><li>`[CONFIRMED-DATA]` `ra2.mix/local.mix` contains `fvtur.vxl`, `fvtur1.vxl`, `fvtur2.vxl` and `fvtur3.vxl` (`ra2extract --raw` probe). `[FV] TurretCount=4` and `WeaponCount=17`, with `*TurretIndex=0..3` per mode (`RepairTurretIndex=2`, `MachineGunTurretIndex=1`, `ShockTurretIndex=3`, …).</li><li>`[CONFIRMED-CODE]` `main.cpp:1702-1712` loads only `<base>tur` (plus `barl`). The draw `main.cpp:3471-3488` has one turret set.</li></ul> |
| **Original** | <ul><li>A multi-turret type draws the turret voxel of the current turret index: `fvtur` for index 0, `fvtur<N>` for N>0 `[CONFIRMED-DATA naming]`.</li><li>`[SUSPECTED]` Exactly how the gunner weapon index maps to the turret index (the `*TurretIndex` keys versus a per-weapon table) was not traced.</li></ul> |
| **Fix** | <ul><li>Load `tur`, `tur1`..`tur{TurretCount-1}` into `VehicleAssets`.</li><li>Expose the current turret index from the sim: the IFV mode via the `*TurretIndex` keys, and the SREF charge stage (W7-03-07).</li><li>Select the facing set by index in `draw_vehicle_unit`.</li></ul> |
| **Effort** | S-M |

### W7-03-06: The shot's Report sound and muzzle anim come from the image's primary, not the weapon that fired (MINOR)

| Field | Details |
|---|---|
| **Player** | <ul><li>Every IFV mode plays `SeawolfAttack` (the missile launch). An Engineer IFV repairing, a GI IFV firing its MG (`IFVAttackGround` with 8-way MGUN muzzle anims), and Sniper, Tesla, Desolator and Chrono IFVs all sound and flash like a missile launcher.</li><li>Elite Grizzly and Tank Destroyer shots show the rookie `GUNFIRE` muzzle instead of `VTMUZZLE`.</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-CODE]` `pres_game.inc:774-779`: `v.report` comes from `Primary` (or `Weapon1`) once per image. It is played at `pres_game.inc:1052`.</li><li>`fire_fx` (`pres_game.inc:671`) uses `fx_.weapon(u.image)`, built once per image from the primary (`pres_fx.inc:158`, `173`).</li><li>`[CONFIRMED-DATA]` `[FV] Weapon2..17` Report: IFVRepair, IFVAttackGround, FlakTrackAttackGround, SealAttack, SniperAttack, TeslaTankAttack, …</li></ul> |
| **Original** | Report and Anim are WeaponType fields of the weapon actually fired (TechnoClass::Fire FUN_006FD210 → the fired WeaponType) `[SUSPECTED: standard, not re-traced]`. |
| **Fix** | <ul><li>Key the FX and Report lookup on the active weapon id: for example, the sim exposes `Unit::w_active_id` or the slot's weapon id.</li><li>Cache `FxWeapon` per weapon id instead of per image.</li></ul> |
| **Effort** | S |

### W7-03-07: The Prism Tank charge turret (`IsChargeTurret`) is not drawn (MINOR)

| Field | Details |
|---|---|
| **Player** | The Prism Tank's turret does not light up through its charge stages before a shot. |
| **Evidence** | <ul><li>`[CONFIRMED-DATA]` `[SREF] IsChargeTurret=true TurretCount=4`. `sreftur.vxl` and `sreftur1..3.vxl` exist in `local.mix`.</li><li>`[CONFIRMED-CODE]` `IsChargeTurret` appears only in a comment (`weapon_resolve.cpp:103`), and one turret model is loaded (see W7-03-05).</li></ul> |
| **Original** | The turret index follows the reload or charge progress `[SUSPECTED: formula not traced; the string "IsChargeTurret" is in gamemd.exe]`. |
| **Fix** | RE the IsChargeTurret consumer (find TechnoType's flag offset with `strref.py IsChargeTurret`). Then drive the turret index from `rearm_frames / rof`, reusing W7-03-05's multi-turret loading. |
| **Effort** | S, after a short RE |

### W7-03-08: The skirmish AI never builds the Robot Tank or the Battle Fortress (MINOR)

| Field | Details |
|---|---|
| **Player** | Allied AI armies never contain Robot Tanks or Battle Fortresses. Its late-game composition is weaker and less varied than the original's. |
| **Evidence** | <ul><li>`[CONFIRMED-CODE]` `ai_skirmish.inc:24-55` `kAiUnits` lists MTNK, FV, SREF, MGTK, TNKD, CMIN and AMCV. It does not list ROBO or BFRT.</li><li>`[CONFIRMED-DATA]` In stock `aimd.ini`, TaskForces name BFRT 4 times and ROBO 2 times.</li><li>AITriggers "Allied Robo Tank - Easy / M_H" are conditioned on owning GAROBO.</li><li>Teams with an unbuildable member are skipped (`ai_trigger.cpp:181-186`, `ai_team.cpp:393-434`). Those teams and triggers are therefore dead.</li></ul> |
| **Original** | The AI builds any TaskForce member its house can build. |
| **Fix** | <ul><li>Append `{"ROBO", true, "GAWEAP"}` and `{"BFRT", true, "GAWEAP"}` to `kAiUnits` (append-only).</li><li>Check that the AI base planner can build GAROBO. `[SUSPECTED]` It may not, in which case ROBO stays gated.</li></ul> |
| **Effort** | S |

### W7-03-09: AI auto-crush ignores the house IQ gate (MINOR)

| Field | Details |
|---|---|
| **Player** | Low-IQ campaign AI houses steer tanks onto infantry. In the original only houses with IQ ≥ `[IQ] AutoCrush=2` do. |
| **Evidence** | `[CONFIRMED-CODE]` `combat.cpp:1276` `(world.house_is_computer(u.house) \|\| …)`. `house_is_computer` is `ai_enabled(h)` (`world.hpp:843`), with no IQ test. |
| **Original** | `[CONFIRMED-RE 0x742720]` ShouldCrush:<ul><li>`(!IsHuman \|\| PlayerAutoCrush (Rules+0x17EB) \|\| type AutoCrush (+0xD2D))`;</li><li>`&& (IsHuman \|\| Rules+0x1448 <= house+0x24C)`. House +0x24C is the house IQ used by the other IQ gates (`world.hpp:709-710`), and Rules+0x1448 is `[IQ] AutoCrush` `[SUSPECTED: offset-to-key not traced; the sibling IQ gates use the same +0x24C]`.</li></ul> |
| **Fix** | Gate the computer branch on `house IQ >= iq.auto_crush`. Skirmish AI houses run at MaxIQLevels, so they are unchanged. |
| **Effort** | S |

### W7-03-10: Robot Tank activate and deactivate feedback is missing (MINOR; W6-04-27 residue, now unblocked)

| Field | Details |
|---|---|
| **Player** | <ul><li>Losing or de-powering the Robot Control Center silently freezes the robots: no "RobotTankOffline", no "RobotTankOnline" on restore.</li><li>Selecting a dead robot plays the normal select voice instead of `RobotTankSelectDeactivated`.</li></ul> |
| **Evidence** | <ul><li>`[CONFIRMED-CODE]` `ActivateSound`, `DeactivateSound` and `VoiceSelectDeactivated` have 0 readers.</li><li>REMAINING.md:467 deferred them for "no sim state". The sim state now exists: `Unit::w6.unpowered`, set in `world.cpp:825-851`.</li></ul> |
| **Original** | `[CONFIRMED-DATA]` `[ROBO] ActivateSound=RobotTankOnline DeactivateSound=RobotTankOffline VoiceSelectDeactivated=RobotTankSelectDeactivated`. |
| **Fix** | <ul><li>Emit an event on the `unpowered` edge in `w6_step_powered_units`.</li><li>Play the sounds in `pres_audio_events.inc`.</li><li>Use the deactivated select voice when `unpowered`.</li></ul> |
| **Effort** | S |

### W7-03-11: The idle turret snaps back toward the hull immediately (COSMETIC)

| Field | Details |
|---|---|
| **Evidence** | `[CONFIRMED-CODE]` `world.cpp:108` `want = u.body_facing` as soon as there is no target. |
| **Original** | `[CONFIRMED-RE 0x735A60]` (also voxel-render.md §5). The idle return waits until `frame - LastFireFrame (+0x120) >= Rules+0xE04 + 5`. While a Destination exists, the turret aims at the move direction. |
| **Fix** | Hold the turret for that delay after the last shot. |
| **Effort** | S |

### W7-03-12: Battle Fortress `AlternateFLH0..4` gun ports are unread (COSMETIC)

| Field | Details |
|---|---|
| **Evidence** | <ul><li>`[CONFIRMED-DATA]` `artmd [BFRT] AlternateFLH0..4` define the five gun ports.</li><li>`[CONFIRMED-CODE]` `git grep AlternateFLH engine` finds 0. Passenger shots leave from the hull FLH.</li></ul> |
| **Original** | Passenger N fires from `AlternateFLH<N>` `[SUSPECTED: indexing not traced]`. |
| **Effort** | S |

### W7-03-13: Chrono Miner return-to-field warp (cross-reference; STILL OPEN W6-04-34; MINOR, RE needed)

| Field | Details |
|---|---|
| **Evidence** | <ul><li>`[CONFIRMED-CODE]` `harvest.cpp:572-576` and `617-638`: after unloading, `ChronoOut`, then a `teleport_unit(... harv_cell ...)`.</li><li>`[CONFIRMED-RE 0x713389]` `Teleporter` is TechnoType +0xCD4. Its consumers are 0x444C2B (BuildingClass), 0x4D9312, 0x4DE381 (FootClass path / move setup FUN_004DE0E0), 0x6CBE04, 0x716A89, 0x73D5DE, 0x73F759 and 0x7411E3. Within budget, none of them showed whether the outbound leg teleports.</li></ul> |
| **Original** | The miner drives to the ore and chronos only home `[SUSPECTED, community knowledge]`. |
| **Fix** | Keep W6-04-34. The next step is to decompile UnitClass FUN_0073F620 (the 0x73F759 branch) and the harvest mission FUN_0073C560. |

**Also still open, not re-filed:**
- **W6-04-31:** the Mirage `EliteSecondary=MirageGunE` quirk.
- **R-148:** the disguise detect radius.
- **W2-SK-22:** Prism fragment target choice.

## Verified OK on b3a509f

- **Default crushability.** InfantryType Crushable defaults to **yes**: the InfantryTypeClass ctor FUN_005233D0 writes `+0x22D = 1` at 0x523612 `[CONFIRMED-RE]`. The ObjectType default is 0 (FUN_005F6880 @0x5F696C). This matches `weapon_resolve.cpp:154`, so Engineers, Conscripts and Spies are crushable and Tanya, Tesla Troopers and Desolators are not.
- **OmniCrusher semantics.** The OmniCrusher branch (+0xD29, victim OmniCrushResistant +0xD2A, buildings excluded) matches `can_crush` (`combat.cpp:561`) `[CONFIRMED-RE 0x5F64C0]`. Only the alliance gate is missing (W7-03-01).
- **Own infantry are not crushed.** In the ALL01 run, the house-0 engineer survived: the Grizzly stopped beside it `[CONFIRMED-PLAY]`.
- **Turret slew rate = hull ROT** for the Grizzly, IFV, Prism and Robot Tank (UnitClass ctor FUN_00734490 Set_ROT on both facings; voxel-render.md §5). The clone uses `rot_u16_per_sec` for both. `TurretROT` is correctly not a RA2/YR key.
- **No barrel recoil is correct for Allied vehicles.** `TurretRecoil=yes` appears only on GTGCAN and CAEAST02 `[CONFIRMED-DATA]`.
- **No reverse gear and no acceleration.** All nine units have `Accelerates=false` `[CONFIRMED-DATA]`, and the clone has no reversing or acceleration for ground vehicles `[CONFIRMED-CODE]`.
- **The Tank Destroyer's UltraAP weapon works:** a TNKD killed a Rhino, 400 → 250 → 100 → dead, at 150 a shot `[CONFIRMED-PLAY]`.
- **IFV:**
  - a GI loads (`in_cargo=1`) `[CONFIRMED-PLAY]`;
  - the passenger-to-weapon map, weapon index = IFVMode, matches all 17 `[FV] WeaponN` slots `[CONFIRMED-DATA]`;
  - `DeathWeapon=CRNuke` is inert without `Explodes` (`world_abilities.cpp:475`), which is correct;
  - AirRangeBonus, Burst and veterancy are fixed (W6).
- **Mirage:**
  - it disguises when still `[CONFIRMED-PLAY]`;
  - enemy auto-acquire skips a disguised Mirage (`combat.cpp:1182`);
  - the tree is drawn for non-owners (R-047);
  - `CanApproachTarget=no` is honoured (W6-04-30).
- **Robot Tank:**
  - PoweredUnit freeze when no powered GAROBO or low power (`world.cpp:825-851`, RE-FALLBACK);
  - ImmuneToPsionics on the mind-control path (`campaign_runtime.inc:1661`) and the auto-scan (`combat.cpp:1184`);
  - amphibious hover domain (`main.cpp:1040-1043`).
- **Battle Fortress** OmniCrusher, OpenTransportWeapon and passengers firing out (W6 fixes and measurements). Note that its art `Image=SREF` key is ignored; that is correct, because the voxel is chosen by rules `Image`.
- **Allied MCV:** deploy ring, facing gate, undeploy-on-sell, and OmniCrushResistant (mcv-deploy.md, all RE-tagged).
- **Tank Destroyer production:** limited to Germany via `RequiredHouses` (`ai_side_catalog.inc:84`).
- **Veterancy:** STRONGER, FIREPOWER, SIGHT, FASTER, ROF and SELF_HEAL are applied (`world_veterancy.cpp:56-114`, `194-201`).

## Prioritized fix list (one fix agent)

| # | ID | Where | Effort |
|---|---|---|---|
| 1 | W7-03-01 alliance crush and scatter | `world.cpp:5670`, `5767` (+ `can_crush`) | S |
| 2 | W7-03-02 turretless hull facing | `world.cpp` `step_turrets` | S |
| 3 | W7-03-04 Mirage fake-blink / adjacency reveal | `world.cpp` `step_disguises`, `combat.cpp` fire path, presentation | M |
| 4 | W7-03-03 OpportunityFire | `combat.cpp` (e) + resolve | M |
| 5 | W7-03-08 AI roster ROBO/BFRT | `ai_skirmish.inc` | S |
| 6 | W7-03-06 per-weapon Report / muzzle | `pres_game.inc`, `pres_fx.inc` | S |
| 7 | W7-03-05 / 07 multi-turret art | `main.cpp` vehicle assets + draw | S-M |
| 8 | W7-03-10, 09, 11, 12, W6-04-34 | as listed | S each |

**Regression scripts to keep:** `03-evidence/W7-03-crush-all01.play` and `03-evidence/W7-03-skirmish.play`. After the fixes:
- **W7-03-01:** `units_lost(5)` and `units_lost(7)` stay 0.
- **W7-03-02:** `sel_body` follows `sel_turret` for the TNKD.
- **W7-03-04:** `udisguised(0,MGTK)` stays 1 across the shot.
