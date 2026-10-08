> Appendix to `docs/gap/wave6/04-units.md`. Gap IDs in this part were remapped to the consolidated W6-04-nn numbering (duplicates across parts share one ID).

# W6-04 part B: Soviet infantry and ground vehicles

**Scope.** This part covers every Soviet infantry unit and ground vehicle a player can build under YR `rulesmd.ini`, including the country specials and the YR additions.

**Method.**
- The worktree read was `D:\ra2-wt\w6-units` at `e2b569e`.
- The spec was `D:\ra2-re\w5-abil-out\rulesmd.ini` and `artmd.ini`.
- Four play runs were made with real clicks, using `click unit 0:X` then `click unit 1:Y`:
  - `docs/gap/wave6/04-evidence/scripts/B-soviet.play` ran at the default lobby speed. The UI slider was 5, which is 60 logic fps, so 1 driver frame is about 4 logic frames.
  - `B-soviet2.play`, `B-soviet3.play` and `B-dogstall.play` ran with the slider at 2. That is GameSpeed 4 = 15 logic fps (docs/re/gamespeed.md §2), so 1 driver frame is 1 logic frame. **All frame numbers below come from these three runs unless marked.**
- Logs are under `D:\ra2-wt\w6-units\play-out\w6-B-soviet{,2,3}\play.log` and `play-out\w6-B-dogstall\`.

Legend: OK = measured in play. OK? = checked by code reading only. WRONG / MISSING = a gap (ID in the cell). N/A = the item does not apply.

## 1. Matrix

| Unit | Move (speed/loco/terrain/crush) | Weapon(s)+ROF | Range (3-D/min) | Warhead Verses/splash | Projectile | Special/deploy | Vet/elite | Garrison/passengers | Sight/cloak/detect | Sounds |
|---|---|---|---|---|---|---|---|---|---|---|
| E2 Conscript | OK? Speed 4 Walk, crushable | OK? M1Carbine 15/25 | OK? 4 (R-063 3-D) | OK? SA | OK? InvisibleLow | N/A | OK? elite M1CarbineE (range 5) | OK? Occupier, UCM1Carbine | OK? 5 | OK? (R-130) |
| SENGINEER Soviet Engineer | OK? Speed 4 | OK? DefuseKit (BombDisarm) | OK? 1.5 | OK? | OK? | OK? capture/repair/defuse (R-046 FIXED; image ENGINEER drives the gestures) | N/A Trainable=no | N/A | OK? 4 | OK? |
| DOG Attack Dog | OK Speed 8 | OK BadTeeth ParasiteDog: GI killed 19-26 frames after the click in 3 geometries | OK 1.5 | OK infantry only (vehicle Verses 0%) | MISSING leap not drawn (R-054 STILL OPEN) | OK DetectDisguise | OK? | N/A | OK? Sight 9 | OK? |
| FLAKT Flak Trooper | OK? Speed 4 | OK? FlakGuyGun AG + FlakGuyAAGun AA (choose_weapon) | OK? 5 / AA 8 | OK? FlakTWH 1-cell, FlakGuyWH | OK? FlakProj scatter (R-057 PARTIAL) | N/A | WRONG elite Burst=2 not fired (W6-04-01) | N/A | OK? | OK? |
| SHK Tesla Trooper | OK? Speed 4, Crushable=no honoured (weapon_resolve.cpp:125) | OK ElectricBolt 50 per shot, Grizzly 250→200 | OK 3 | OK Shock heavy 100% | OK? bolt drawn (IsElectricBolt) | OK? coil charge (R-045 FIXED) | WRONG elite bounce missing (W6-04-02) | N/A | OK? 6 | OK? |
| IVAN Crazy Ivan | OK? Speed 4 | OK bomb plant; blast 452 frames after the click (timer 451, §E) | OK 1.5 | WRONG building blast far too weak (W6-04-03) | N/A | MISSING bomb on own/allied objects (W6-04-23); defuse OK? (R-046) | OK? IvanDamage is used, not the elite Damage (§E) | N/A | OK? 6 | MISSING attach/tick sounds, bomb icon (W6-04-11) |
| CIVAN Chrono Ivan (stolen tech) | OK? Teleport loco (§C1) | OK? IvanBomber | OK? | WRONG as IVAN (W6-04-03) | N/A | OK? stolen-tech gate (R-042 FIXED) | OK? | N/A | OK? 8 | as IVAN (W6-04-11) |
| BORIS Boris | OK? Speed 5, Crushable=no | OK? AKM 65/20; Flare designates buildings | OK? 7 / Flare 12 | OK? BORISWH | MISSING Flare `IsLine` laser not drawn (W6-04-24) | OK airstrike: called at once from 10 cells, 2 BPLN, enemy Radar 1000→0 | WRONG MiG `Maverick3 Burst=2` fires 1 (W6-04-01) | N/A | OK? 9 | OK? |
| TERROR Terrorist (Cuba) | OK? Speed 6 | OK? Suicide TerrorBomb | OK? 1.5 | WRONG building blast (W6-04-03) | N/A | OK? death weapon (§F) | N/A | N/A | OK? 9 | OK? |
| DESO Desolator (Iraq) | OK? Speed 4, Crushable=no | OK? RadBeam 125/50 | OK? 6 (elite 8) | OK? RadBeamWarhead | MISSING `IsRadBeam` beam not drawn (W6-04-24) | OK deploy kills the adjacent GIs; after undeploy the field decays 261198→208959 in 450 frames (accumulating site, §A1-A3); glow drawn | OK? SelfHealing | N/A | OK? 6 | OK? DeploySound |
| HTNK Rhino | OK? Speed 6, Crusher | OK? 120mm 90/65 | OK? 5.75 | OK? AP .3 | OK? Cannon (R-PJ fixed) | N/A | WRONG elite 120mmE Burst=2 (W6-04-01) | N/A | OK? 8 | OK? |
| HTK Flak Track | OK? Speed 8, Crusher | OK? FlakTrackGun + AA gun | OK? 5 / AA 10 | OK? | OK? | N/A | WRONG elite Burst=2 (W6-04-01) | OK? 5 passengers, SizeLimit 2 (not OpenTopped in YR, so they cannot fire out) | OK? 8 | OK? |
| APOC Apocalypse | OK? Speed 4, Crusher | WRONG one 100-dmg shell per ROF instead of a 2-shell volley (W6-04-01) | OK? 5.75 / tusk 8 | OK? ApocAP | OK? | OK SelfHealing: +10 HP in 140 frames (500→510, +1/14 frames, V11) | WRONG 120mmxE Burst=4 (W6-04-01) | N/A | OK? 6 | OK? |
| V3 V3 Launcher | OK? Speed 4 | OK? spawner rocket 200 (elite 400) | OK? 18 / min 5 holds (combat.cpp:1068) | OK? V3WH | OK? RocketLoco (W4-PJ-04), interceptable (W4-PJ-10) | N/A | OK? V3EliteWarhead | N/A | OK? 7 | OK? |
| DTRUCK Demolition Truck (Libya) | OK? Speed 5 | OK suicide on arrival (71 frames from 5 cells) | OK 1 | WRONG Battle Lab only 500→291 (W6-04-03) | N/A | OK radiation left (rad_total 0→7572) | N/A | N/A | OK? 5 | OK? |
| TTNK Tesla Tank (Russia) | OK? Speed 6, Crusher | OK TankBolt kills a Grizzly in 3 bolts | OK? 4 (elite 6) | OK? Electric | OK? bolt | N/A | WRONG elite bounce (W6-04-02) | N/A | OK? 8 | OK? |
| DRON Terror Drone | MISSING not buildable (W6-04-10) | OK (cheat-spawned) Grizzly infected at once, 300 HP chewed, drone pops out; a GI dies in 12 frames | OK 1.83 | OK Parasite | MISSING leap (R-054) | OK? Service Depot kills the drone (world.cpp:2240) | N/A | N/A | OK? 4 | OK? |
| HARV War Miner | OK? Speed 4, Crusher | OK? 20mmRapid | OK? 5.5 | OK? HARVWH | OK? | OK? harvest (R-190 RE-FALLBACK) | OK? SelfHealing | N/A | OK? 4 | OK? |
| SMCV | OK? Speed 4 | N/A | N/A | N/A | N/A | OK? deploys into NACNST (play: real deploy each run) | N/A | N/A | OK? | OK? |
| SAPC Amphibious Transport | OK? Hover + Amphibious zone (R-001) | N/A | N/A | N/A | N/A | N/A | N/A | OK? 12 passengers, SizeLimit 6 (body-count capacity per transport-cargo.md §1, not re-verified) | OK? 6 | OK? |
| SCHP Siege Chopper | MISSING not buildable (R-201 STILL OPEN: voxel) | flight: agent C | – | – | – | MISSING deploy-to-artillery 160mm (W6-04-17) | WRONG 160mmE Burst=2 (W6-04-01) | N/A | – | – |
| YURI / PSIC | N/A: Yuri needs YABRCK; PSIC does not exist in rulesmd (the Psi-Corps Trooper is PTROOP, Allied stolen tech) | | | | | | | | | |

The sidebar was measured for an Iraq human with NAHAND, NAWEAP, NARADR and NATECH up:
- DESO, BORIS, APOC, IVAN, SHK, FLAKT, V3, HTK, HTNK, HARV, DOG and SENGINEER show.
- TTNK, DTRUCK and TERROR are correctly absent (the specials of other countries).
- **DRON and SCHP are absent.**

## 2. Gaps

### W6-04-01 — `Burst=` is not modelled: every weapon fires one shot per ROF cycle — NEW — MAJOR — effort M
- **What the player sees:**
  - The Apocalypse fires one shell where it should fire a two-shell volley (four when elite), so its ground DPS is halved.
  - Its anti-air tusks fire one missile instead of two.
  - Elite Rhinos, elite Flak Troopers and elite Flak Tracks, the elite Siege Chopper and Boris's MiGs (`Maverick3 Burst=2`, 750 per missile) do half their intended damage.
  - In run 1 an Apocalypse lost a duel to a single Rhino.
  - 50 weapons in rulesmd carry `Burst>1`. On the Allied side these include 105mmE, HoverMissile (IFV), Dragon, 90mmE and the AAGattling family, so agent A/C rows are affected too.
- **Evidence:**
  - `git grep -n Burst engine` finds no reader; `UnitCombat` and `WeaponSlot` have no burst field.
  - `combat.cpp:103` `try_fire` sets `rof_cool = rearm_secs(w_rof)` for every shot.
  - Play `B-soviet2.play` block B: an Apocalypse fired on a static enemy Radar.
    - Its first hit took the Radar from 1000 to 900, and it was 908 six frames later (AI repair, +8).
    - The next drop came about 80 frames later.
    - That is one 100-damage shell per ROF; two shells (1000→800) were expected.
- **Original:**
  - `TechnoClass::Fire` FUN_006FD210 advances `CurrentBurstIndex` (+0x3B8) and wraps it modulo `Weapon.Burst` (+0x9C) (decompile `D:\ra2-re\w5-abil3-out\b3\f_006fd210.c:690-700`; docs/re/combat.md §5).
  - `GetROF` FUN_006FC460 returns `Random(3,5)` frames while `CurrentBurstIndex < Burst` (or the vehicle type's per-burst delay at +0xE44 when that is set), and only then the full ROF (`D:\ra2-re\fixGout\h3_006fc460.c`).
  - So `Burst=N` gives N shots 3-5 frames apart, then the ROF.
- **Fix:**
  - Resolve `Burst` into `WeaponSlot`/`WeaponStats`, including the elite slots.
  - Add a `burst_index` to `Unit`, and in `try_fire`/`rearm_secs` use Random(3,5) until the index wraps.
  - Building weapons are in `building_combat.cpp`, and aircraft ammo handling (Maverick3 Ammo=1 Burst=2) is in `world_an.cpp`.

### W6-04-03 — Ability blasts bypass the DamageArea port, so buildings take a fraction of the damage — NEW (R-060 fixed only the weapon path) — MAJOR — effort S/M
- **What the player sees:**
  - A Crazy Ivan bomb leaves a Battle Lab standing at 50/500 HP, where the original destroys it.
  - A Demolition Truck driven into a Battle Lab leaves it at 291/500.
  - Terrorists, death weapons (Apocalypse `Explodes`, the Demo Truck) and the Desolator eruption damage use the same simplified path.
  - Base-busting with Ivans, Terrorists or Demo Trucks, the signature Soviet tactic, does not work.
- **Evidence:**
  - `world_abilities.cpp:236 World::ability_area_damage` applies 2-D linear falloff and hits each building **once**, at its nearest foundation cell.
  - It skips Iron Curtain, AffectsAllies, veterancy armour, ProneDamage and the min-1 floor.
  - The faithful `combat.cpp:505 damage_area` (FUN_00489190) hits a building **once per foundation cell in the ring set** (the victim list is not de-duplicated, AddContent FUN_0047E7B0).
  - Play `B-soviet2.play` F: an Ivan on NATECH (3x3, wood) took it from 500 to 50, a single 450 hit.
  - Play `B-soviet3.play` E: a DTRUCK on NATECH took it from 500 to 291, a single ~209 hit (300 × falloff × wood 80%). The truck died and rad_total rose (OK). Screenshot: `04-evidence/B-dtruck.jpg`.
- **Original:**
  - BombClass::Detonate 0x4386A0 is `DamageArea(target.coord, IvanDamage, ..., IvanWarhead)`, and FireDeathWeapon detonates a bullet in place (abilities.md §E/§F). Both run through FUN_00489190.
  - The clone's own port of that function gives the Ivan-on-NATECH case 450 + 3×291 = 1323 (CellSpread 1.5, PercentAtMax .25), so the building is destroyed. This figure is computed from `modify_damage`/`damage_area`, not measured.
- **Fix:** route `ability_area_damage` through `damage_area`, using a synthetic shooter with the ability warhead's damage, verses, CellSpread and PercentAtMax, and with `src_unit` set. Keep the eruption's RadSite creation as it is.

### W6-04-10 — The Terror Drone cannot be built (and the AI roster loses it too) — NEW — MAJOR — effort S
- **What the player sees:** no Terror Drone cameo with a War Factory up. One of the four basic Soviet vehicles is missing.
- **Evidence:**
  - Play `B-soviet.play` L47: `cameo(DRON)=0` with NAWEAP, NARADR and NATECH built, while V3, HTK and APOC show.
  - `artmd.ini [DRON] Voxel=no` (an SHP vehicle). `load_unit_roster_table` (`main.cpp` ~1934) takes the SHP branch only for the naval locomotor `2bea74e1` (DLPH/SQD). DRON, with the Drive locomotor, goes to `load_vehicle_assets` (voxel), which has no `.vxl`, so `if (!va.ok) continue;` drops it.
  - `ai_skirmish.inc:29` lists DRON for the AI, and it goes through the same loader [inference, not run].
  - The ability itself works on a cheat-spawned drone: a Grizzly was infected and killed, and a GI died in 12 frames.
- **Original:** DRON is `TechLevel=4 Prerequisite=NAWEAP` (rulesmd).
- **Fix:** in `load_unit_roster_table`, use the SHP-vehicle branch for any vehicle with `!art.voxel(image)`, not only naval ones, and make sure the SHP body is loaded by the scene. Then check that it renders.

### W6-04-23 — The Crazy Ivan cannot bomb own or allied units or buildings, and Ctrl force-fire refuses own objects — NEW (abilities.md "cut" 4) — MINOR — effort S
- **What the player sees:**
  - Clicking an Ivan on your own Rhino selects the Rhino.
  - Ctrl+click does nothing.
  - The classic "bomb your own tank or drone and drive it into the enemy base" is impossible.
- **Evidence:**
  - Play `B-soviet.play` T3b: `bombs=0` after a plain click and after a ctrl click (L92/L97 FAIL).
  - `input_original.cpp:134-136`: Ctrl attack requires `!controllable(house)` for a unit and `house != 0` for a building.
  - `input_original.cpp:155-170`: an own unit resolves to Select or Deploy.
- **Original:**
  - `[IVAN]` and `[CIVAN]` set `AttackCursorOnFriendlies=yes`; the rulesmd comment reads "Accept a command to attack, but don't consider in threat scan".
  - Ctrl force-fire on own units is standard RA2 behaviour.
  - [RE-FALLBACK: the TechnoType flag reader and the cursor path were not traced.]
- **Fix:**
  - In `Input::classify`, give an Attack action on own or allied objects when every selected unit's type has AttackCursorOnFriendlies, and allow own targets under Ctrl.
  - `World::order_attack`, `valid_target` and the IvanBomb plant path must accept a non-enemy target for an explicit order.

### W6-04-11 — Ivan bombs (and other ability events) have no presentation — NEW — MAJOR — effort S/M
- **What the player sees:**
  - Nothing marks a bombed unit or building: no flickering bomb icon, no `CrazyIvanAttack` attach sound, no `CrazyIvanBombTick` ticking.
  - The player cannot see which of his units carry a bomb, so a Soviet Engineer can only defuse by guesswork.
- **Evidence:**
  - `git grep` for `bomb_frame`, `BombTicking` and `BombAttach` finds no presentation reader.
  - `pres_audio_events.inc:141` consumes only `AbilityEvent::Kind::Deploy`. BombPlant, BombBlast, Infect, Suicide, Eruption and Teleport events are taken (`main.cpp:6303`) and dropped.
  - Screenshot `04-evidence/B-ivan-bomb.jpg` (40 frames after the plant): no marker on the Battle Lab.
- **Original:**
  - `[AudioVisual] BombAttachSound=CrazyIvanAttack BombTickingSound=CrazyIvanBombTick`, `[CombatDamage] IvanIconFlickerRate=8` (BombClass, abilities.md §E).
  - [RE-FALLBACK: the icon SHP name and the draw offset were not traced.]
- **Fix:** in `pres_audio_events.inc on_world_events`, play the attach and tick sounds, and draw the bomb icon over objects with `bomb_frame >= 0` in pres_game.

### W6-04-24 — The Desolator rad beam (`IsRadBeam`) and Boris's Flare line (`IsLine`) are not drawn — NEW — MINOR — effort S
- **What the player sees:**
  - A Desolator kills with no visible green beam.
  - Boris designates a building with no laser line.
- **Evidence:** `pres_fx.inc:170 fill_beam` reads only IsElectricBolt and IsLaser; `git grep IsRadBeam` and `git grep IsLine` find nothing in the engine.
- **Original:** `[RadBeamWeapon] IsRadBeam=yes`, `[Flare] IsLine=true IsHouseColor=true` (rulesmd). [RE-FALLBACK: the beam colour and duration constants were not traced.]
- **Fix:** add a rad-beam and line kind to `fill_beam` and to the beam drawer. The fire events exist: `world_w5.cpp:567` pushes `"Flare"`.

### W6-04-02 — Elite Tesla bounce (`Electricbounce` shrapnel) is missing; the shrapnel keys are read from the wrong section — NEW — MINOR (Soviet) / possibly MAJOR for the Prism Tank (agent A to confirm) — effort S
- **What the player sees:** an elite Tesla Trooper or elite Tesla Tank bolt does not arc on to two more targets.
- **Evidence:**
  - `main.cpp:854` reads `ShrapnelWeapon`/`ShrapnelCount` from the **weapon** section, and only for the primary.
  - In rulesmd these keys sit on the **projectile**: `[Electricbounce]` (ElectricBoltE, TankBoltE) and `[LargeCometP]` (the Prism Tank's Comet).
  - So `shrapnel_count` is always 0 for these weapons, and the elite weapon swap carries no shrapnel either.
- **Original:** `[Electricbounce] ShrapnelWeapon=TeslaFragment ShrapnelCount=2` (BulletType keys).
- **Fix:** read the keys from `rules.weapon_projectile(weapon)`, and resolve them per slot, elite included.

### W6-04-17 — Siege Chopper: unbuildable, and its deploy-to-artillery mode does not exist — STILL OPEN (R-201) + NEW (deploy) — MINOR while it is unbuildable — effort M
- **Evidence:**
  - `cameo(SCHP)=0` (`B-soviet.play` L45), because the voxel is absent under the clone's naming (R-201).
  - `IsSimpleDeployer=yes`, `UnloadingClass=SCHD` and `DeployFire` on a non-`Deployer` type are never read: `git grep IsSimpleDeployer` finds nothing, and `weapon_resolve.cpp:101` requires `Deployer && DeployFire`.
  - So even once it is buildable, the chopper could not land and fire `160mm` (Range 12, SCHOPWH).
- **Original:** rulesmd [SCHP] (deploy anim SCHPDEPL, deployed image SCHD). [RE-FALLBACK: the simple-deployer path was not traced.]
- **Fix:** the deploy state in `World::order_deploy` for IsSimpleDeployer aircraft (land, swap image, Secondary active). Agent C owns the flight half.

**Already-known items confirmed still open in this scope:**
- R-054: dog and drone leaps are not drawn.
- R-057: flak is PARTIAL.
- R-201: SCHP art.

## 3. Verified OK in play

| Check | Result | Run |
|---|---|---|
| Country specials gate | DESO yes; TTNK, DTRUCK and TERROR no | B-soviet L36-41 |
| Base Soviet roster cameos | APOC, BORIS, IVAN, SHK, FLAKT, V3, HTK, HTNK, HARV, DOG, SENGINEER all show | B-soviet L36-46 |
| Attack Dog vs GI | One bite kills; the GI died 19-26 frames after the click (3 cells straight, 6 cells diagonal, 3 cells elsewhere) | B-soviet3 |
| Idle dog | Auto-acquires a GI 2 cells away and kills it in 9 frames | B-soviet2 A2 |

The run-1/2 dog "stall" was a test artifact. The GI had been spawned inside a fully fenced yard (`play-out\w6-B-dogstall\map.txt`, `04-evidence/B-dog-fenced.jpg`), so it was unreachable on foot.

| Check | Result | Run |
|---|---|---|
| Tesla Trooper vs Grizzly | 50 per bolt (250→200) | B-soviet T2 |
| Crazy Ivan timer | Blast on an enemy Battle Lab 452 frames after the click, adjacent start (451 per §E). The blast damage itself is wrong (W6-04-03) | B-soviet2 F |
| Terror Drone | Infects a Grizzly at once; the host dies and the drone pops out | B-soviet T4 |
| Terror Drone vs GI | GI killed 12 frames after the click | B-soviet3 G |
| Desolator deploy | Kills the GIs beside it; after undeploy the field decays about 20% in 450 frames (accumulated site, §A); glow drawn | B-soviet2 D, `04-evidence/B-deso-deployed.jpg` |
| Apocalypse SelfHealing | +1 HP per 14 frames (500→510 in 140 frames, V11) | B-soviet2 C |
| Tesla Tank | Kills a Grizzly in 3 bolts | B-soviet T7 |
| Boris airstrike | Called immediately from 10 cells; 2 MiGs; enemy Radar 1000→0 | B-soviet T8 |
| Demolition Truck | Drives in, suicides and leaves radiation (the damage is wrong, W6-04-03) | B-soviet3 E |
| SMCV | Deploys to NACNST by a real click and the D key in every run | all runs |

**Counts:** 0 BLOCKER, 4 MAJOR (W6-04-01, W6-04-03, W6-04-10, W6-04-11), 4 MINOR (W6-04-23, W6-04-24, W6-04-02, W6-04-17). W6-04-02 is MINOR for the Soviet scope; it becomes MAJOR if agent A confirms the Prism Tank split is broken by the same bug.
