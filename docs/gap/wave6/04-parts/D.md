> Appendix to `docs/gap/wave6/04-units.md`. Gap IDs in this part were remapped to the consolidated W6-04-nn numbering (duplicates across parts share one ID).

# W6-04 part D: Allied and Soviet structures, defences, walls and superweapons

Auditor: agent D. Tree: `D:\ra2-wt\w6-units` (w6-units @ e2b569e). Spec: YR `rulesmd.ini`/`artmd.ini` (`D:\ra2-re\w5-abil-out`).
Ghidra: private copy `D:\ra2-re\w6-units-D`, output in `D:\ra2-re\w6-units-D-out` (`d1.txt`, `d\dis_70cac0.txt`, `d\dis_489480.txt`).

**Time base.** The skirmish lobby's default Game Speed is 1, which runs **4 logic frames per `--play` driver frame** (`gamespeed.hpp`, `main.cpp:6132`). Every driver-frame count below has been converted to logic frames (x4) before comparing it with a rules value.

**Play runs.** All four were run with `docs/gap/wave6/04-evidence/run-play.ps1`, and the scripts are in `04-evidence/scripts/`:

| Script | Side / map | What it covers | Output |
|---|---|---|---|
| `D-defences.play` | Allied, default map | sidebar census, walls, first-pass defence tests | `play-out/w6-D-defences` |
| `D-defences2.play` | Allied | isolated Sentry Gun, Grand Cannon, Patriot vs Kirov and reactor-death measurements | `play-out/w6-D-defences2` |
| `D-superweapons.play` | Soviet, Bay of Pigs | Soviet census, Iron Curtain, Chronosphere into water | `play-out/w6-D-superweapons` |
| `D-sw2.play` | Soviet, default map | Nuke, Lightning Storm | `play-out/w6-D-sw2` |

In `D-superweapons.play`, the Easy AI razed the unguarded base after about 8000 logic frames, so its nuke and storm sections are void. `D-sw2` re-ran them with guards.

**Screenshots** are in `04-evidence/`: `D-wall-grizzly.jpg`, `D-patriot-kirov.jpg`, `D-iron-curtain.jpg`, `D-chrono-water.jpg`, `D-nuke.jpg`, `D-storm.jpg`.

**Out of scope under YR data, confirmed in play:**
- NAPSIS (Psychic Sensor) and NACLON (Cloning Vats) have `Prerequisite=YACNST` / `YATECH`. They are Yuri-only in YR, and `cameo=0` for both the Allied and the Soviet player.
- The Psychic Beacon and Psychic Dominator (`YAPPET`), NATBNK (Yuri Tank Bunker) and GAFWLL (Citadel Wall) are Yuri.
- YR has no gates.

## 1. Matrix

Key:
- **OK** = measured in play; **OK?** = checked by code reading only.
- *Move* is N/A for every structure, so the Move column is dropped.
- *Vet/elite* is N/A for structures: YR building veterancy is limited to garrison occupants, and is covered by row NABNKR and by R-053.

| Structure (rules ID) | Weapon(s) + ROF | Range (3-D / min) | Warhead Verses / splash | Projectile | Special | Garrison | Sight / detect | Sounds |
|---|---|---|---|---|---|---|---|---|
| GACNST / NACNST Construction Yards | N/A | N/A | N/A | N/A | OK (MCV deploy, Factory=Building) | N/A | OK? Sight 8 | OK? |
| GAPOWR Power Plant (+200) / NAPOWR Tesla Reactor (+150) | N/A | N/A | N/A | N/A | OK (`power_out` 2300 = 2000 + 2x150 measured) | N/A | OK? | OK? |
| NANRCT Nuclear Reactor (+2000) | DeathWeapon NukePayload | N/A | **WRONG**: death blast at full 600, not x0.5 (W6-04-12) | N/A | **MISSING**: no RadLevel 500 fallout on death (W6-04-12) | N/A | OK? | OK? |
| GAREFN / NAREFN Refineries | N/A | N/A | N/A | N/A | OK? (Soylent 300) | N/A | OK? | OK? |
| GAPILE / NAHAND Barracks | N/A | N/A | N/A | N/A | OK? (spy: veteran infantry) | N/A | OK? | OK? |
| GAWEAP / NAWEAP War Factories | N/A | N/A | N/A | N/A | OK? (spy: veteran vehicles) | N/A | OK? | OK? |
| GAYARD / NAYARD Naval Yards | N/A | N/A | N/A | N/A | OK? (UnitRepair; naval.play) | N/A | OK? | OK? |
| GAAIRC Airforce Command / AMRADR (Americans) | N/A | N/A | N/A | N/A | OK (GAAIRC forbidden to Americans, AMRADR present; AmerParaDrop granted, sw_count 1) | N/A | OK? Radar | OK? |
| NARADR Soviet Radar | N/A | N/A | N/A | N/A | **MISSING**: YR SpyPlaneSpecial not granted, sw_count 0 (W6-04-19) | N/A | OK? Radar | N/A |
| GADEPT / NADEPT Service Depots | N/A | N/A | N/A | N/A | OK? (UnitRepair) | N/A | OK? | OK? |
| GATECH / NATECH Battle Labs | N/A | N/A | N/A | N/A | **MISSING**: YR ForceShieldSpecial not granted (W6-04-20); stolen tech OK? (R-042) | N/A | OK? | N/A |
| GAOREP Ore Purifier | N/A | N/A | N/A | N/A | OK? (OrePurifier read, `world.cpp:4661`) | N/A | OK? | OK? |
| NAINDP Industrial Plant | N/A | N/A | N/A | N/A | **MISSING**: cameo 0 (R-201); FactoryPlant 0.75 not applied (R-066) | N/A | – | – |
| GASPYSAT Spy Satellite Uplink | N/A | N/A | N/A | N/A | OK? (whole-map reveal while powered; play abil-gap) | N/A | OK? | – |
| GAROBO Robot Control Center | N/A | N/A | N/A | N/A | **MISSING**: not in the building roster, so the Robot Tank cannot be built (W6-04-18) | N/A | – | – |
| GAGAP Gap Generator | N/A | N/A | N/A | N/A | OK? (radius 10, powered; R-044 play abil-gap) | N/A | OK? | MISSING radial indicator (W6-04-27) |
| GAPILL Pillbox | OK? Vulcan2 50, ROF 26 | **WRONG**: 2-D, no elevation bonus (W6-04-22) | OK? SA, ProneDamage 70% | OK? InvisibleLow | – | OK (not garrisonable) | OK? Sight 7 | OK? PillboxAttack |
| NALASR Sentry Gun | OK: Vulcan 50, hits every 24-36 logic frames | **WRONG**: 2-D (W6-04-22) | OK: 40 vs flak, then 28 prone (125>85>57>29>1>dead) | OK? | OK (Power 0: never offline) | N/A | OK? | OK? SentryGunAttack |
| NASAM Patriot Missile | OK: RedEye2, one hit about every 60 logic frames vs a Kirov | **WRONG**: 2-D vs air (the Kirov is at 750 leptons) (W6-04-22) | OK? SAMWH (70-73 per hit measured vs 75, see §3) | OK? AAHeatSeeker homing | OK? AA only (LandTargeting=1) | N/A | OK? Sight 10 | OK? PatriotAttack |
| NAFLAK Flak Cannon | OK? FlakWeapon 40, ROF 20 | **WRONG**: 2-D (W6-04-22) | OK? FlakWH 1.0 spread | OK? FlakScatter (R-057) | OK? AA only | N/A | OK? | OK? |
| ATESLA Prism Tower | OK? PrismShot 120, ROF 45 | **WRONG**: 2-D (W6-04-22) | OK? | OK? laser | OK? chain (R-043, play abil-prism-chain) | N/A | OK? | OK? |
| TESLA Tesla Coil | OK? CoilBolt 200 / OPCoilBolt 300 | **WRONG**: 2-D (W6-04-22) | OK? Electric | OK? bolt | OK? Overpowerable (R-045, play abil-tesla-charge) | N/A | OK? | OK? |
| GTGCAN Grand Cannon (France) | OK? 150, ROF 120 | **WRONG**: MinimumRange 3 ignored, killed a Conscript 1.5 cells out (W6-04-22) | OK? GrandCannonWH 2 spread | OK? arcing ball | OK (RequiredHouses: Americans cannot build) | N/A | OK? Sight 10 | OK? TurretRotateSound |
| NABNKR Battle Bunker | occupants' weapons | – | – | – | **MISSING**: cameo 0 (R-201) | **MISSING** (R-201; garrison combat R-053) | – | – |
| GAWALL / NAWALL Walls | N/A | N/A | **WRONG**: any warhead damages a built wall; a GI took 6 HP off (W6-04-04) | N/A | OK (Grizzly AP levelled one) | N/A | N/A | OK? |
| Map wall overlays | N/A | N/A | **MISSING**: no warhead damages them (P4-5, W6-04-05) | N/A | OK? crushable (P4-4) | N/A | N/A | N/A |
| NAMISL Nuke (RechargeTime 10) | OK: charge 8988 logic frames (9000) | OK? (Range 7 cosmetic) | OK: GAPOWR 750 left at 390 (600 x wood 60%); fallout OK (rad_total 57787); walls **WRONG** (W6-04-04) | ~400-frame flight (documented deviation) | OK | N/A | N/A | OK? |
| NAIRON Iron Curtain (5) | OK: charge about 4480 (4500) | 3x3 | OK: 2 Conscripts killed, the tank and a building protected, about 752 frames (750) | N/A | OK | N/A | N/A | OK? |
| GACSPH Chronosphere (7) | OK: charge about 6284 (6300) | 3x3 | OK: a Rhino warped onto water died | N/A | OK? (infantry chrono-killed) | N/A | N/A | OK? |
| GAWEAT Weather Control (10) | OK: charge 9000 | 10x10 scatter | OK: first bolt about 256 frames (Deferment 250), storm ends about 432 (250+180), target GAGAP destroyed | N/A | OK | N/A | N/A | OK? |
| Paradrops (AMRADR AmerParaDrop, CAAIRP ParaDrop) | – | – | – | – | OK? (sw.play; R-056 FIXED) | – | – | OK? |
| SpyPlaneSpecial (NARADR), ForceShieldSpecial (Battle Labs) | – | – | – | – | **MISSING** (W6-04-19, W6-04-20) | – | – | – |

## 2. Gap entries

### W6-04-04: Built walls take damage from any warhead, and walls ignore Wall/WallAbsoluteDestroyer (NEW)

- **What the player sees.**
  - A GI force-firing an enemy Allied Wall chips it. Measured in `D-defences.play` L66: `bhp(1,GAWALL)` 300 -> 294 after 1200 logic frames.
  - In the original, an M60 (`SA`, no `Wall=`) can never hurt a wall.
  - The reverse also holds. A Nuke (`[NUKE] WallAbsoluteDestroyer=yes`) only scratches built walls: concrete 8% of 600, at most 48 of 300 HP. In the original every wall in the blast rings is erased.
- **Evidence.**
  - Walls are 1x1 `Building`s with HP (R-195; `main.cpp:2539`).
  - The building damage paths never read the warhead's `Wall` flag: `combat.cpp` `damage_area` (building loop from l.586) and `deliver_building_hit`; `world_sw.cpp` `an_area_damage`; `world_abilities.cpp:236` `ability_area_damage`.
  - The warhead `wall` field (`weapon_resolve.cpp:35`) is used only by bridges (`world_depth.cpp:570`, `world.cpp:5615`).
- **Original.**
  - `DamageArea` FUN_00489190 runs this per ring cell when the cell's overlay is a wall (OverlayType+0x2A8), @0x489581..0x4895BD:
    - `WallAbsoluteDestroyer` (WH+0x145) -> `Reduce_Wall(-1)` (destroy).
    - else `Wall` (WH+0x144) -> `Reduce_Wall(damage)`.
    - else `Wood` (WH+0x147) on an overlay with Land=Wood -> `Reduce_Wall(damage)`.
    - Any other warhead does nothing.
  - `Reduce_Wall` FUN_00480BC0 (d1.txt) has no HP. When `damage < OverlayType.Strength` (+0x2A4), it advances a damage stage only if `Random(0,Strength) < damage`. At `DamageLevels-1` the wall is removed.
  - Placed walls become overlays (roster2.md TL;DR 1, FUN_0043F100).
- **Severity / effort.** MAJOR / M.
- **Fix.** Gate the building-damage paths on a `wall` building type:
  - Damage only when the warhead has `Wall` (or `Wood` for wood walls).
  - Destroy outright on `WallAbsoluteDestroyer`.
  - Use the probabilistic stage step instead of HP (`combat.cpp` damage_area/deliver_building_hit, `world_sw.cpp` an_area_damage, `world_abilities.cpp` ability_area_damage). The proper overlay model is R-195.
- **Related.** P4-5, R-195.

### W6-04-05: Map wall overlays cannot be damaged by any weapon (STILL OPEN, P4-5)

- **What the player sees.** Force-firing a map wall (sandbags, concrete, fences) with a Grizzly or a Rhino does nothing. Only crushing removes one (P4-4).
- **Evidence.** Code only: `world.hpp:821-846` and `world.cpp:695-726` hold crush state alone, and no combat path touches overlays.
  - Not measured: the stock skirmish map has no wall overlay near the start.
- **Original.** Same path as W6-04-04: FUN_00489190 -> FUN_00480BC0.
- **Severity / effort.** MAJOR (campaign maps are walled) / M.
- **Fix.** Give the overlay grid a damage stage (OverlayData high nibble) and call it from `damage_area` and `bridge_explosion`-style ground hits, with the W6-04-04 gate.

### W6-04-12: Nuclear Reactor death explosion is twice as strong and leaves no fallout (NEW)

- **What the player sees.** A destroyed NANRCT's blast does 600-based damage, not 300.
- **Evidence.** `D-defences2.play` L160: an adjacent enemy NAPOWR (wood 750) went 750 -> 488, a loss of 262.
  - At the same falloff the original value is 131, so the plant would be left at about 619.
  - `rad_total` stayed 0 (L161).
- **Code.**
  - `main.cpp:1240` resolves `DeathWeapon` but never reads `DeathWeaponDamageModifier`.
  - `world_abilities.cpp:516-519` passes `tw.death.damage` straight to `ability_area_damage`, which has no radiation and no anim.
- **Original.** FireDeathWeapon FUN_0070CAC0 @0x70CB1B: `FILD [weapon+0xA4 Damage]; FMUL [type+0xD1C DeathWeaponDamageModifier]; _ftol`, then a real bullet `Detonate` FUN_004690A0.
  - That bullet carries the `NukePayload` weapon (FUN_0046B180), so `RadLevel=500` makes a radiation site and NUKE's `AnimList=NUKEANIM` plays.
  - YRpp `TechnoTypeClass.h:442-443` confirms the field pair.
- **Severity / effort.** MAJOR (the reactor going up wrecks your own base at double strength) / S.
- **Fix.** Read `DeathWeaponDamageModifier` into `BldTypeW5` (`main.cpp` wire_building_deaths). Scale the damage, and call `rad_.detonate` when `death.rad_level > 0` (`world_abilities.cpp` death branch).

### W6-04-22: Base defences ignore MinimumRange and use 2-D range with no elevation bonus (STILL OPEN, R-063 residue)

- **What the player sees.** A Conscript hugging a Grand Cannon dies.
  - `D-defences2.play` L56-L76: spawned 1.5 cells from the GTGCAN centre, alive at driver frame 71, dead by 91.
  - In the original the cannon cannot fire inside `MinimumRange=3`, and that dead zone is the standard counter.
- **Other effects.**
  - Defences on hills get no `[ElevationModel]` bonus.
  - AA defences measure an airborne target in 2-D. For a Kirov at 750 leptons the Patriot's horizontal reach is 12 cells, where 3-D gives about 11.6. Against Harriers at FlightLevel 1500 it is 12 vs about 10.5.
- **Evidence.**
  - `building_combat.cpp:184-188` `in_range` is `std::hypot` in 2-D, with no MinimumRange and no Z.
  - `git grep MinimumRange engine` hits only the unit paths (`combat.cpp:1068-1071`, `w2air_main.inc:32`).
- **Original.** combat.md §9: the TechnoClass in-range test FUN_006F6750 (buildings are TechnoClass)
  - is 3-D (dx²+dy²+dz²);
  - adds the SubjectToElevation bonus FUN_006F6490;
  - refuses `distance < MinimumRange` (+0xB8).
- **Severity / effort.** MINOR (MAJOR for French players) / S.
- **Fix.** In `step_building_combat`, call `weapon_in_range_from` with the building centre (z = cell level x 104) and the shooter's weapon (`defense_shooter`) instead of the local lambda.
- **Note.** R-063 is marked FIXED, but the building path was left out.

### W6-04-13: Base defences never attack enemy structures (NEW, needs RE)

- **What the player sees.** A French player cannot creep Grand Cannons, and a Prism Tower never fires at an enemy structure, garrisoned civilian buildings included. Defences engage only units.
- **Evidence.**
  - `building_combat.cpp:189-215`: `engageable` scans `units_` only.
  - defenses.md §5: "Defenses do not target buildings".
- **Original.** [RE-FALLBACK: community knowledge of the original game ("Grand Cannon creep"). `Target_Something_Nearby` / `Greatest_Threat` was not decompiled (defenses.md §3 ANCHORED).]
- **Severity / effort.** MAJOR / M.
- **Fix.**
  - First decompile the TechnoClass threat scan for buildings (`Greatest_Threat`, the ThreatPosed/ThreatType filter).
  - Then add enemy buildings (distance to centre + (w+h)*64, combat.md §9) to the `step_building_combat` acquisition.

### W6-04-19: YR Spy Plane (Soviet Radar Tower) is missing (NEW; conflicts with W2-AN-19 under U-1)

- **What the player sees.** A Soviet player with a Radar Tower has no Spy Plane button. Measured in `D-superweapons.play` L54: `sw_count(0)=0` with NARADR + NATECH built.
- **Evidence.** `realplay_economy.inc:75-87` `rp_classic_sw_type` admits only the 7 classic RA2 types. It deliberately drops `[NARADR] SuperWeapon=SpyPlaneSpecial` (W2-AN-19 "classic RA2"). `main.cpp:2742` excludes NARADR from the AI SW list.
- **Original.** YR rulesmd `[NARADR] SuperWeapon=SpyPlaneSpecial` (RechargeTime 4, Type=SpyPlane). The type is index 8 in the binary enum (superweapon.md §4, table 0x841560).
  - The engine plays YR rules (U-1/R-193). The spec is therefore YR, and this is a YR Soviet addition.
- **Severity / effort.** MAJOR / M (fly-over reveal: SPYP aircraft, `SpyPlaneCamera` frames).
- **Fix.** Admit `SpyPlane` in `rp_classic_sw_type`, then add a `SuperweaponType::SpyPlane` branch in `World::fire_superweapon_at` (world_sw.cpp:198-262). It should fly an `SPYP` from the edge to the target and reveal along the path. RE the reveal radius first.

### W6-04-20: YR Force Shield (all Battle Labs) is missing (NEW; conflicts with W2-AN-19 under U-1)

- **What the player sees.** A Battle Lab gives no Force Shield. Measured in `D-defences.play` L51: `sw_count(0)=1`, AmerParaDrop only, with GATECH built.
- **Original (YR rulesmd).**
  - `[GATECH]/[NATECH] SuperWeapon=ForceShieldSpecial`, with RechargeTime 5 and Range 3.4.
  - `ForceShieldRadius=4`, `ForceShieldDuration=500` frames of invulnerability for own buildings, then `ForceShieldBlackoutDuration=1000` frames of the house's power blackout.
  - The AI uses it via `AISuperDefenseProbability`.
  - Type=ForceShield is index 10 in the enum (superweapon.md §4).
- **Evidence.** The same filter as W6-04-19; `superweapon.cpp:30` says "only id 1 (Nuke) is active", and `default: return false` (`world_sw.cpp:260`).
- **Severity / effort.** MAJOR / M.
- **Fix.** Admit the type. The fire branch can reuse the Iron Curtain building flag (`iron_left`) inside radius 4 for 500 frames, plus `blackout_frames_[house]=1000` (the spy power-plant blackout path, `world.cpp` infiltrate PowerPlant). RE the `ObjectClass::IronCurtain(..., ForceShield=true)` variant (YRpp `ObjectClass.h:156`) for whether units are covered.

### W6-04-18: The Robot Control Center is not in the building roster, so the Robot Tank cannot be built (NEW)

- **What the player sees.** No GAROBO cameo (`D-defences.play` L39: `cameo(GAROBO)=0`), so no Robot Tank either: `[ROBO] Prerequisite=GAWEAP,GAROBO`, and no ROBO cameo appears.
  - Robots also never deactivate when the RCC is lost or unpowered: `[ROBO] PoweredUnit=yes`, `[GAROBO] PowersUnit=ROBO`.
- **Evidence.**
  - GAROBO is absent from `kRoster` (`main.cpp:2305-2338`).
  - `main.cpp:2068` comments "GAROBO art absent". That is unverified: the same comment block calls GAWEAT art absent, but GAWEAT now has a cameo (L44 PASS).
  - `git grep PoweredUnit\|PowersUnit engine` returns nothing.
- **Original.** The rules above. [RE-FALLBACK for the deactivation mechanics: TechnoClass "Deactivated" when no live, powered PowersUnit building exists.]
- **Severity / effort.** MAJOR (a YR Allied unit is unbuildable) / M.
- **Fix.**
  - Append GAROBO to `kRoster` (append-only, index 40), and check that its art resolves through the YR mixes.
  - Add `powered_unit` to the unit type, with a per-house "has a live powered PowersUnit building" check in `World::step` that freezes such units.
- **Related.** R-201 (NABNKR, NAINDP are the same class).

### W6-04-21: NABNKR Battle Bunker and NAINDP Industrial Plant are still unbuildable (STILL OPEN, R-201 / R-066)

- **Measured.** `D-superweapons.play` L43/L44: `cameo(NABNKR)=0`, `cameo(NAINDP)=0` for a Soviet player with NATECH.
- **Effect.** Soviets lose their YR garrison defence (a 5-occupant bunker, also in `SovietBaseDefenses` for the AI) and the 25% vehicle discount.
- **Suggestion.** Re-run the R-201 art census, since GAWEAT's "absent" art now loads.
- **Severity.** MAJOR in play, DATA-LIMIT per R-201.

### W6-04-27: Powered-defence and indicator presentation keys are not read (NEW)

- **Missing keys.**
  - `WorkingSound=PowerOn` / `NotWorkingSound=PowerOff` on NASAM/NAFLAK/TESLA/ATESLA/GTGCAN (no sound when a defence goes offline or online).
  - `HasRadialIndicator=true` (the Gap Generator radius circle when selected).
- **Evidence.** `git grep "WorkingSound\|HasRadialIndicator" engine` returns nothing.
- **Severity / effort.** MINOR (cosmetic) / S.
- **Fix.** `pres_audio_events.inc` on the `def_offline` edge; a radial draw in the selection overlay.

## 3. Verified OK in play (this audit)

**Sidebar census.**
- Allied (Americans): GAPILL, NASAM, ATESLA, GAWALL, GAGAP, GASPYSAT, GAOREP, GADEPT, GAYARD, GACSPH, GAWEAT and AMRADR are present. GAAIRC is hidden (ForbiddenHouses), GTGCAN is hidden (RequiredHouses=French), and NAPSIS, NACLON and NALASR are absent.
- Soviet (Russia): NALASR, NAFLAK, TESLA, NAWALL, NAIRON, NAMISL, NANRCT, NADEPT and NAYARD are present. NAPSIS, NACLON and GAPILL are absent.
- AMRADR grants AmerParaDrop.

**Power.** NANRCT gives +2000 (`power_out` 2300 with 2 NAPOWR).

**Sentry Gun vs a Conscript.** 40 on the first hit, then 28 per hit once prone (ProneDamage 70%), 5 hits, about 24-36 logic frames apart (ROF 26 + jitter).

**Patriot vs Kirov.**
- The Patriot engages the airborne Kirov and hits about every 60 logic frames, and over a long run it kills the Kirov.
- Per-hit damage measured 70-73, not the expected 75 (SAMWH medium 100%, PercentAtMax 1). The cause is not attributed: possibly a house Armor multiplier in `vet_receive`. Passed to the combat-core owner, not filed here.

**Wall.** A Grizzly (AP Wall=yes) levels an enemy GAWALL.

**Iron Curtain.**
- Charge about 4480 logic frames (4500).
- It kills the Conscripts in the 3x3 (`units(0)` 16 -> 14) and protects the vehicle and the building (`iron_units=1`, `iron_buildings=1`).
- It lasts about 752 logic frames (IronCurtainDuration 750).

**Chronosphere.** Charge about 6284 (6300). A Rhino warped onto a water cell is destroyed (`units_lost` +1).

**Nuke.** Charge 8988 (9000), and the blast leaves the target GAPOWR at exactly 390 HP (600 x NUKE wood 60%). Radiation is laid (`rad_total` 57787).

**Lightning Storm.**
- Charge 9000. The first bolt comes about 256 frames after the click (LightningDeferment 250), and the storm ends about 432 frames after it (250 + 180).
- The targeted Gap Generator (600 wood) is destroyed, and a second storm is refused while one is active (code).
