> Appendix to `docs/gap/wave6/04-units.md`. Gap IDs in this part were remapped to the consolidated W6-04-nn numbering (duplicates across parts share one ID).

# W6-04 part C: Allied and Soviet aircraft, jumpjets and ships

Agent C, worktree `D:\ra2-wt\w6-units` @ e2b569e, release exe built by the lead.
- **Spec:** `D:\ra2-re\w5-abil-out\rulesmd.ini` (YR).
- **Ghidra:** private copy `D:\ra2-re\w6-units-C`. Scripts are in `D:\ra2-re\w6-units-C-scripts`, outputs in `D:\ra2-re\w6-units-C-out`.
- **Black Hawk:** YR has no separate Black Hawk unit. `[SHAD]` is `Name=BlackHawk Transport` with `UIName=Name:SHAD`, which is the **Night Hawk** in the sidebar ("Nighthawk Transport"). Night Hawk is audited below.

## Evidence

Scripts are in `docs/gap/wave6/04-evidence/scripts/`. Step logs (every click, cheat, PASS and FAIL with its frame) are in `04-evidence/C-logs/`.

| Script (out dir) | Map / human | What it does |
|---|---|---|
| `C-air.play` (`play-out/w6-C-air2`) | default map, Korea | Roster (BEAG yes / ORCA no); Black Eagle strike → return → rearm; Kirov bombing interval; Siege Chopper weapon and deploy |
| `C-air2.play` (`w6-C-air3`) | Korea | Rocketeer vs 3 Conscripts (selected while grounded); Night Hawk with 6 GIs |
| `C-air3.play` (`w6-C-air4`) | Korea | Rocketeer Stop; Night Hawk with exactly 5 GIs; Kirov killed over the own War Factory (crash damage) |
| `C-naval1.play` (`w6-C-naval1`) | Bay of Pigs, America vs France | Naval roster; Destroyer + Osprey vs Typhoon; Amphibious Transport with 5 Grizzlies; Sea Scorpion vs Harrier; Aegis vs Kirov |
| `C-naval2.play` (`w6-C-naval2`) | Bay of Pigs | Aegis vs Kirov sampled every 8 frames; enemy Squid on my Destroyer, then my Dolphin; Carrier Hornets vs a Tech Centre; Dreadnought vs a Radar |
| `C-naval3.play` (`w6-C-naval3`) | Bay of Pigs | My Squid vs an enemy Destroyer (R-049); my Typhoon vs an enemy Sea Scorpion, sampled every 5 frames (surfacing) |

`C-air.play` was also run once to `w6-C-air`. That first run is invalid: the spawned target building types already existed in the AI base.

- **Screenshots:** in `04-evidence/`: `C-schp-no-deploy.jpg`, `C-aegis-kirov.jpg`, `C-dolphin-squid.jpg`, `C-squid-grab.jpg`, `C-sub-fire.jpg`, `C-kirov-crash.jpg`, `C-beag-strike.jpg`, `C-carrier-hornet.jpg`, `C-dred-missiles.jpg`, `C-dest-osprey-sub.jpg`.
- **Timing:** below, "frames" are driver frames. A skirmish runs at game speed 1, which is **4 logic ticks per driver frame**. This was measured as `anim_tick` 156 → 1312 over 289 frames, and the Black Eagle rearm took 67-68 frames, which matches ReloadRate 270 ticks.

## 1. Matrix

- **OK** = measured in play. **OK?** = checked by reading code and rules only.
- IDs are the consolidated `W6-04-nn` (section 2). R-/W2-AN- IDs refer to existing rows.

| Unit | Move | Weapon(s)+ROF | Range (3-D/min) | Warhead Verses/splash | Projectile | Special/deploy | Vet/elite | Garrison/passengers | Sight/cloak/detect | Sounds |
|---|---|---|---|---|---|---|---|---|---|---|
| JUMPJET Rocketeer | OK. Jumpjet, lifts off on exit; flight 7.1 cps by code (JumpjetSpeed 30, bias; W6-04-36) | OK. 20mm 25/30. First Conscript killed 41 frames after the order; all 3 killed | OK? 5 | OK? SSA | OK? Inviso AA+AG | OK? Stop clears the flight goal (C1 inconclusive: it had already arrived); falls on death (Crashable) | OK? 20mmE ROF 5 | N/A | OK? 8 | MISSING: RocketeerMoveLoop, ImpactLandSound (W6-04-27) |
| ORCA Harrier | OK. Fly, 3.28 cps; built onto an AMRADR pad | OK. Maverick 150. HYD 400→250 in one pass; fires at 6 cells, then RTB | OK? 6 (elite 9) | OK. ORCAAP 100% vs heavy | OK? homing DRAGON (W4-PJ-01 fixed) | OK. Ammo 1 → RTB → rearm at 270 ticks (BEAG run). Attack-run geometry RE-FALLBACK (R-090) | OK? MaverickE 300 | N/A | OK? 8 | MISSING: AuxSound1/2 takeoff and landing, MoveLoop (W6-04-27) |
| BEAG Black Eagle (Korea) | OK. Korea: `cameo(BEAG)=1`, `cameo(ORCA)=0` | OK. Maverick2 200: GATECH 500→300 in one pass | OK? 6 (elite 9) | OK. ORCAAP | OK? homing | OK. Order f=248 → hit f=268 → docked f=274 → ammo back f=342 (68 frames = 272 ticks) | OK? | N/A | OK? 8 | MISSING (W6-04-27) |
| SHAD Night Hawk | OK? Jumpjet, lands to load (W2-AN-08) | OK? BlackHawkCannon 35/40 OmniFire | OK? 6 | OK? SA | OK? Inviso | OK? lands, then loads | OK? | OK? Group boarding: 2 of 5-6 GIs boarded in both runs. The rest stood still 2-3 cells away (terrain is suspected; not confirmed, see §3) | MISSING: RadarInvisible=yes unread (minor) | MISSING (W6-04-27) |
| SCHP Siege Chopper (YR) | OK. Flies (2.8 cps) | WRONG: only BlackHawkCannon is ever used. Secondary 160mm needs deploy (W6-04-17) | OK? 6 | OK? SA | OK? | **MISSING:** `d` does nothing; it stays at altitude 500 and never becomes SCHD artillery (W6-04-17) | OK? | N/A | OK? 7 | MISSING (W6-04-27) |
| ZEP Kirov | OK. Jumpjet 1.125 cps measured. RE says ~0.70 (W6-04-36, W2-AN-26 open) | OK. BlimpBomb 250, ROF 50: bomb interval 13 frames = 52 ticks | OK. 1.5 | OK. BlimpHE CellSpread 2. Refinery 1000→231 by one bomb (per-cell building splash, combat.md §8), dead on the 2nd | OK? Vertical ZBOMB voxel | OK. Crashable: falls 750 lepts in 15 frames (JumpjetCrash 12). Crash on own GAWEAP 1000→541 (−459, Strength/4 RE-FALLBACK, R-090) | OK? SelfHealing seen (+3) | N/A | OK? 8 | MISSING: KirovCreated (CreateSound), KirovMoveLoop, KirovCrash ImpactLandSound (W6-04-27) |
| ASW Osprey (spawned) | OK. From the Destroyer; kills a submerged Typhoon 600 HP in 837 frames | OK. ASWBomb 50 | OK? 3 | OK? APSplash | OK? DepthCharge homing | OK. Destroyer `Range=-2` launcher; docks and rearms (SpawnReloadRate 150) | N/A | N/A | OK? 2 | WRONG: every docked return counts as a unit lost (W6-04-26) |
| HORNET (spawned) | OK. From the Carrier | OK. HornetBomb 40: NATECH 500→460 | OK? 5 | OK? ORCAAP | OK? NormalBomb | PARTIAL: only one Hornet airborne at a time (`spawned_out` 1 over 150 frames). Not RE'd (§3) | OK? HornetBombE | N/A | OK? 2 | WRONG: lost-unit count (W6-04-26) |
| DMISL (spawned) | OK. Rocket loco (W4-PJ-04) | OK. NARADR 1000→612 from one 2-missile volley | OK? 25 / min 8 | OK? | OK. Pause, tilt, climb | OK. Interceptable (W4-PJ-10) | N/A | N/A | N/A | OK? DontScore |
| DEST Destroyer | OK. Water (W2-AN-09) | OK. 155mm 60/110 + ASWLauncher | OK? 8 | OK? ARTYHE | OK? Ballistic | OK. Sensors 8 sees a submerged sub at once (`sub_detected_by0=1` after 0 frames); ASW Osprey hunts it | OK? 155mmE Burst 2 (W6-04-01-type burst issue may apply) | N/A | OK. Sonar | OK? |
| AEGIS Aegis Cruiser | OK? Water, ROT 1 | **WRONG:** Medusa 100/15 fires only after the hull slowly turns. OmniFire ignored (W6-04-06) | WRONG: effective range ~4.5 cells instead of 12 (W6-04-06) | OK? SAMWH (0% vs ships/buildings) | OK? Homing MEDUSA | MISSING: RadialFireSegments / DistributedFire (minor) | OK? MedusaE Burst 2 | N/A | OK? 8 | OK? |
| CARRIER Aircraft Carrier | OK? Water | OK. HornetLauncher spawner. Attacks a land building (Tech Centre) | OK? 25 | N/A (Special) | N/A | OK. CanPassiveAquire=no honoured. Hull-turn delay before launch (W6-04-06) | OK? | N/A | OK? 7 | OK? |
| DLPH Dolphin | OK. SHP ship, submerges | PARTIAL: SonicZap 4 dmg plus ambient. IsSonic wave and AmbientDamage are not modelled | OK? 6 | MISSING: `Sonic=yes` warhead eject of a parasite (W6-04-09) | MISSING: sonic wave | **MISSING:** cannot free a ship held by a Squid (W6-04-09) | OK? | N/A | WRONG: surfaces on fire despite DecloakToFire=no (W6-04-25) | OK? |
| LCRF Amphibious Transport (Allied) | OK. Amphibious hover (W2-AN-10, naval.play) | N/A | N/A | N/A | N/A | OK. Beaches, unloads on land only | OK? | **WRONG:** took 5 Grizzlies (`cargo_total=5`); the original caps at 4 (size sum 12) (W6-04-08) | OK? 6 | OK? |
| SAPC Armored Transport (Soviet) | OK? as LCRF; Crusher=yes | N/A | N/A | N/A | N/A | OK? | N/A | WRONG: same capacity rule (W6-04-08) | OK? 6 | OK? |
| SUB Typhoon | OK? Water; submerged | OK. SubTorpedo 100/120: HYD 370→270, torpedo every ~30 frames = 120 ticks | OK? 7 | OK? APSplash | OK? Level torpedo SUBT | WRONG: surfaces for CloakDelay after every shot (`submerged(0)` 2→1 at f=460) (W6-04-25) | OK? SubTorpedoE Burst 2 | N/A | OK. Hidden unless sonar; Sensors 7 | OK? |
| HYD Sea Scorpion | OK? Water | OK? FlakTrackGun AG 25/25 + FlakWeapon AA 40/20 | OK? 5 / 12 | OK? FlakTWH / FlakWH | OK? | Not observed: it did not engage the Harrier's 1-pass strike (it kept its Destroyer target, `utgt=2`) | OK? | N/A | OK? 8 | OK? |
| SQD Giant Squid | OK. SHP ship | OK. **Grabs in play now:** mine auto-grabbed an enemy DEST (`parasited=1`, 498→279 over 120 frames), and an AI squid grabbed my DEST. R-049's "never fires" was not reproduced | OK. 1.5 | OK? ParasitePlus paralyse | MISSING: leap not drawn (R-054) | PARTIAL: Culling at 25% not observed. Eject by Sonic or heal is missing (W6-04-09) | OK? | N/A | WRONG: surfaces on fire (W6-04-25) | OK? |
| DRED Dreadnought | OK? Water | OK. DredLauncher 2×DMISL vs a radar 81 frames after the order | OK? 25 / min 8 (not measured) | OK? DMisl block | OK. RocketLocomotion | OK. CanPassiveAquire=no. Hull-turn before launch (W6-04-06) | OK? elite warhead | N/A | OK? 7 | OK? |
| GAYARD / NAYARD | — | — | — | — | — | OK? UnitRepair is free [RE-FALLBACK] (R-090). A healing hit does not eject a Squid (W6-04-09) | — | — | — | — |
| GAAIRC / AMRADR pads | — | — | — | — | — | OK. 4 pads; Harrier and Black Eagle dock and rearm | — | — | — | — |

## 2. Gap entries

### W6-04-06 — OmniFire ignored on ships: the Aegis turns its hull (ROT 1) before every engagement — NEW, MAJOR, effort S

**What the player sees.** A Kirov flies straight over an Aegis Cruiser and sinks it.
- The Aegis has the target from 11.9 cells away but launches its first Medusa only at about 4.5 cells, once its ROT=1 hull has turned to face it.
- In `C-naval2` R1 the AI Aegis (800 HP) lost to one Kirov, and the Kirov kept 1706 of its 2000 HP.
- In `C-naval1` N4 the Kirov kept 1117 HP after killing the Aegis.
- The Carrier, Dreadnought and Destroyer launchers (ROT 1/1/5) also wait for the hull before launching.

**Evidence.**

| Frame | Aegis target | Kirov distance | Missiles in flight | Kirov HP |
|---|---|---|---|---|
| 243 | Kirov | 11.9 cells | 0 | — |
| 251, 259 | Kirov | closing | 0 | — |
| 267 | Kirov | ~4.5 cells | first missile (`homing_live=1`) | 2000 |
| 283 | Kirov | — | — | 1703 |

- Sampled in `play-out/w6-C-naval2/play.log`.
- In code, `Unit::omni_fire` is set only for jumpjets (`engine/main.cpp:928`, inside `if (is_aircraft)`) and for Fly/Rocket types (`engine/w2air_main.inc:72`).
- The aim gate `u.is_vehicle && !u.omni_fire && !vehicle_aimed(...)` at `engine/combat.cpp:796/907/1104` therefore holds every ship's fire until it faces the target.
- The per-slot `WeaponSlot::omni_fire` is parsed (`engine/weapon_resolve.cpp:34`) but combat never reads it.

**Original.** `OmniFire=yes` → `WeaponType+0x12B` (`FUN_0077151B`) bypasses the turn-to-face aim gate (`docs/re/aircraft.md` §4). Medusa, ASWLauncher, HornetLauncher, DredLauncher and BlackHawkCannon all have `OmniFire=yes`.

**Fix.** In `activate_weapon_slot` / `resolve_combat_unit`, set `u.omni_fire` from the active slot's `omni_fire` for every unit, not only fliers. Alternatively, test the slot flag in the three combat.cpp gates.

### W6-04-08 — Transport capacity is a body count; the original's UnitClass gate is a Size sum — NEW, MAJOR, effort S

**What the player sees.** The Amphibious Transport (Passengers=12) takes 5 Grizzlies, and would take 12 tanks. In the original it takes 4 (Size 3 each) or 2 Apocalypses (Size 6).

**Evidence.** `C-naval1` N2: five MTNK selected, then a click on the LCRF → `cargo_total(0)=5` after 31 frames. `engine/cargo.cpp:15-44` `can_load` is `cargo.size() < passengers_max` plus `p.size <= size_limit`.

**Original (RE, this audit).** `UnitClass::ReceiveMessage` `FUN_00736500` (vtable ref `0x7F4E04`), cases `0xF` (can I load?) and `0xE` (load):
- `if (Type->Passengers(+0x5E0) < Passengers.GetTotalSize() + passenger->Type->Size(+0x380)) return 10 /*NEGATIVE*/;`
- `if (passenger->Size > Type->SizeLimit(+0x388)) return 10;`
- `GetTotalSize` `FUN_00473390` sums the Size over the passenger list. Its callers `0x736711/0x736871/0x736915/0x736A82` are all inside this function, so it is not display-only.
- `docs/re/transport-cargo.md` §1 analysed only the FootClass base `FUN_004190B0`, which UnitClass overrides. The BuildingClass garrison gate (`FUN_0043C250`) is a count, so garrisons stay as they are.

**Fix.**
- `can_load`: for a unit transport, require `cargo_total_size(units, t) + p.size <= t.passengers_max`; keep the count gate for buildings.
- Correct `transport-cargo.md` §1.
- This touches IFV/BFRT/HTK/SHAD only when Size>1 passengers board, because infantry are Size 1.

### W6-04-17 — Siege Chopper deploy (land, become SCHD artillery, fire 160mm) not modelled — NEW, MAJOR, effort M

**What the player sees.** The YR Soviet Siege Chopper is NOT buildable (`cameo(SCHP)=0` in B-soviet.play L45, R-201; lead correction) and, cheat-spawned, is only a gunship. Pressing `d` does nothing, and its 160mm artillery (Range 12, Secondary) is never used.

**Evidence.**
- `C-air.play` A5 (`w6-C-air2` dump L158): after `key s` then `key d` and 120 frames, `sel_altitude=500`, `sel_flight_state=2` (cruising), `sel_damage=35` (BlackHawkCannon), `weapon_range(SCHP)=6`.
- Screenshot `C-schp-no-deploy.jpg`.
- `git grep -i "IsSimpleDeployer\|DeployToLand"` has no reader. `weapon_resolve.cpp:100` binds DeployFire only with `Deployer=yes`.

**Original.** [RE-FALLBACK: from rules semantics, not traced in Ghidra] `[SCHP] DeployFire=yes IsSimpleDeployer=yes DeployToLand=yes DeployingAnim=SCHPDEPL UnloadingClass=SCHD`, `Secondary=160mm`:
1. On deploy it lands.
2. It plays SCHPDEPL and swaps to the SCHD image.
3. It fires the secondary from the ground.
4. On undeploy it lifts off.

**Fix.** `world_abilities.cpp` deploy dispatch: an IsSimpleDeployer jumpjet with DeployToLand descends to Grounded, swaps image to UnloadingClass, becomes immobile and uses weapon slot 1. Undeploy reverses this. UnloadingClass art is already loaded for harvesters (`main.cpp:7184-7200`).

### W6-04-09 — A Sonic hit (Dolphin) or a healing hit does not free a host from its parasite (Giant Squid, Terror Drone) — NEW, MAJOR, effort S

**What the player sees.** An enemy Giant Squid grabs a Destroyer and chews it. The player's Dolphin cannot help:
- the Squid is in limbo, so it cannot be clicked;
- attacking the held ship with the Dolphin does not make the Squid let go;
- the Naval Yard repair does not free it either.

The Dolphin's main job in the original (countering Squids) is lost.

**Evidence.**
- `C-naval2` R3: `parasited=1` from f=588 to f=835, my DEST 585→458.
- `click unit 1:SQD` → "no unit 1:SQD" (the Squid is limboed). The Dolphin never freed the ship.
- `engine/abilities.cpp:148` `suppression_frames` returns 0 for a negative (healing) hit, and no code ejects a parasite on heal.
- `git grep -i sonic engine` has no warhead flag.

**Original (RE, this audit).** `FootClass::ReceiveDamage` `FUN_004D7250` head:
- `if (warhead && warhead->Sonic(+0x14B) && this->ParasiteEatingMe(+0x694)) { ParasiteClass::ExitUnit (FUN_00629F40); ... }`.
- `+0x14B` = `Sonic` was confirmed by the WarheadType INI loader at `0x75C3B0..0x75C3C4` (string `"Sonic"` at `0x846D70`).
- The same function ejects on a negative-damage (healing) hit, with suppression 50 frames (abilities.md §B3, already documented but not implemented).
- `[SonicWarhead] Sonic=yes` (Dolphin SonicZap).

**Fix.**
- Parse `Sonic=` into the warhead profile. In the unit damage path (`deliver_unit_hit` / `damage_area` / `ability_damage_unit`), a Sonic hit on a host with `parasite_on_me >= 0` runs the existing drone-exit code (`world_abilities.cpp` ~585-608).
- Do the same when the host's HP rises (yard or depot repair).
- The Dolphin's IsSonic wave / AmbientDamage remains a separate MINOR item.

### W6-04-25 — Cloaked naval units surface on every shot; DecloakToFire=no is ignored — NEW, MINOR (PROBABLE), effort S

**What the player sees.** A Typhoon, Dolphin or Squid pops up for about 18 ticks after each torpedo, sonic zap or grab. During that window any nearby ship can shoot it without sonar.

**Evidence.**
- `C-naval3` S2: `submerged(0)` is 2 (SUB + SQD) in every 5-frame sample except f=460 (just after a shot), where it is 1.
- `engine/world_an.cpp:131` re-arms `surface_left = CloakDelay` on every `fire_flash`. The weapon key `DecloakToFire` is never parsed (`git grep -i decloak` finds nothing).

**Original (RE, this audit).**
- `DecloakToFire` → `WeaponType+0x133` (loader `0x770F42`; ctor default 1 at `0x770BC2`).
- Its only reader is `TechnoClass::GetFireError` `FUN_006FB5D0` @`0x6FBF4A`: `if (weapon->DecloakToFire && CloakState(+0x220) != 0 && ...) return FireError 9 /*CLOAKED*/`. With `=no` (SubTorpedo, SonicZap, SquidGrab) the unit fires while cloaked.
- The CloakDelay timer `+0x240` is armed from `Rules+0x1410` in the cloak state machine (`0x6FAEF7..0x6FAF2C`) whenever the unit uncloaks.
- Whether `Fire` itself forces an uncloak was **not traced** → PROBABLE.

**Fix.** Parse `DecloakToFire` per weapon slot. `world_an.cpp:131` re-arms the surface timer only for DecloakToFire=yes weapons. Confirm `TechnoClass::Fire`'s cloak handling first.

### W6-04-26 — Spawned aircraft that dock back on their carrier count as units lost — NEW, MINOR, effort S

**What the player sees.** The skirmish score screen's "units lost" grows by one for every Hornet or Osprey sortie that lands safely.

**Evidence.**
- `C-naval1`: `units_lost(0)` went 2 → 13 while only Ospreys flew and nothing of the player's died.
- `C-naval2` R4: 2 → 6 in 150 frames (Hornets).
- `engine/world_an.cpp:543-551` docks a spawn with `alive=false; die_time=1.0` and no `deployed_away`. `engine/gf_scene.inc:176-183` counts every `!alive` non-DontScore unit as lost or killed.

**Original.** A docked spawn re-enters the SpawnManager; it is not a kill. [RE-FALLBACK: not traced, but the stats would be absurd otherwise.]

**Fix.** Set `deployed_away = true` when a spawn docks (as the paradrop plane does, `world_an.cpp:528`).

### W6-04-27 — Aircraft-specific sounds never play — NEW, MINOR, effort M

**What the player sees.** Several aircraft sounds are silent:
- no "Kirov created" on build;
- no Kirov engine drone, Rocketeer jetpack loop or Harrier/Black Eagle engine loop while flying;
- no takeoff or landing sound (Harrier, Hornet, Osprey);
- no ground impact when a wreck hits (KirovCrash, GenAircraftCrash, RocketeerCrash).

**Evidence.**
- `git grep -i "ImpactLandSound\|AuxSound\|CreateSound"` finds nothing in engine.
- `pres_game.inc:1094` plays `MoveSound` once when a vehicle starts a path move. Fliers move without a path, so their `*MoveLoop` never starts.
- Agent A notes the same `CreateSound` gap (A.md: TanyaPrimeCreated).

**Original.** rules keys `CreateSound` (ZEP), `MoveSound=*MoveLoop`, `AuxSound1/2` (ORCA, HORNET, ASW takeoff/landing), `ImpactLandSound`. [RE-FALLBACK for the trigger points. W5-E traced only CrashingSound/VoiceCrashing.]

**Fix.** In `pres_audio_events.inc`:
- edge-detect a unit's creation (CreateSound);
- edge-detect the flight state, TakingOff → AuxSound1 and Landing → AuxSound2;
- hold a looping controller while airborne and moving (MoveSound);
- play ImpactLandSound when a crashing wreck reaches altitude 0 (`an_step_deaths`).

### W6-04-35 — Harness: `click unit` on an airborne unit clicks its shadow — NEW, MINOR (harness), effort S

**Effect.** A script cannot select or target a flying unit. Real players are not affected, because `Input::unit_under` raises the hit box.

**Evidence.**
- `C-air.play` run 2: `click unit 0:JUMPJET` on a just-built airborne Rocketeer → `selected_type(JUMPJET)=0`.
- `engine/game_scene_play.inc:49-52` computes the ground point with no `altitude_to_screen_dy`.
- **Consequence:** `scripts/play/air.play` §3 (W2-AN-07 "Rocketeer obeys Stop") passes vacuously, because the Rocketeer was never selected. The Night Hawk §5 works only because the unit is grounded at click time.

**Fix.** Subtract `altitude_to_screen_dy(u.altitude)` for `is_aircraft` units in the `unit` locator.

### W6-04-36 — Jumpjet and aircraft speed bias — STILL OPEN (W2-AN-26 / R-090), new evidence, MINOR

**Measured.** `uspeed` probe: Kirov 1.125 cells/s and Harrier 3.28 cps; Kirov positions agree, about 8 cells in 30 frames. **From code:** Rocketeer flight 7.1 cps. These are `speed_to_cps` with the ground `GameSpeedBias` 1.6 (`main.cpp:778-782, 922`; `w2air_main.inc:66`).

**Why it may be wrong.**
- `docs/re/aircraft.md` §5 derives 12 leptons/frame = **0.70 cells/s** for the Kirov, with no bias.
- `docs/re/abilities-w5.md` §12 (R-082) found **no reader** of the house Groundspeed/Airspeed handicap fields (+0x190/+0x198) into which AssignHandicap folds GameSpeedBias.
- So the 1.6× on flight speed has no RE support. If that holds, every flier is ~60% too fast; the Kirov crossing a map is the most visible case.

**Fix.** Trace `GetMaxSpeed` (obj vtbl `+0x538`) for a Jumpjet and a Fly unit (aircraft.md Q3), then drop or keep the bias for fliers.

### W6-04-37 — Kirov crash damage still guessed — STILL OPEN (R-090), MINOR

**Measured.** A Kirov killed over the own War Factory: GAWEAP 1000 → 541 (−459). The fall takes 15 frames for 750 leptons at JumpjetCrash 12.

**Clone.** `world_an.cpp:625-633` deals `max(10, Strength/4)` over 1 cell, 100% Verses, hitting each foundation cell [RE-FALLBACK]. The original crash payload is untraced. Keep it in the R-090 batch.

## 3. Verified OK in play (and not-confirmed notes)

**Verified OK:**
- **Roster:** Korea gets the Black Eagle and no Harrier. America has the Harrier and every Allied ship cameo (DEST, AEGIS, CARRIER, DLPH, LCRF). Dolphin and Squid exist as SHP ships.
- **Black Eagle sortie:** off the pad at once, hit at 20 frames (200 damage), docked 6 frames later, `plane_ammo` back to 1 after 68 frames = 272 ticks (ReloadRate .3 min = 270).
- **Harrier:** Maverick 150 on a Sea Scorpion (400→250) from 6 cells, then RTB and dock.
- **Rocketeer:** 20mm kills 3 Conscripts; the first dies 41 frames after the order, including the flight in.
- **Kirov bombing:**
  - drop interval 13 frames = 52 ticks (ROF 50);
  - 2-cell BlimpHE splash, which hits each building cell, per combat.md §8;
  - an enemy refinery dies to 2 bombs;
  - Crashable fall rate is JumpjetCrash 12 leptons/tick;
  - the Kirov self-heals.
- **Destroyer + Osprey vs Typhoon:**
  - sonar detects the submerged sub at once;
  - the Destroyer cannon never hits the submerged sub (correct);
  - Osprey sorties (50 dmg each) kill the 600-HP sub in 837 frames;
  - the Osprey docks and relaunches.
- **Carrier:** Hornets attack a land building (Tech Centre 500→460 per bomb); the launch range was not measured.
- **Dreadnought:** a 2-DMISL volley hits a Radar 81 frames after the order (1000→612).
- **Giant Squid:** grabs a Destroyer in play from both houses (`parasited=1`, the host chews down at 15/99 ticks plus other fire). **R-049's "the squid never fires at a ship" did not reproduce on e2b569e.** The explicit click order was not exercised, because the squid auto-grabbed first.
- **Typhoon:** torpedoes a Sea Scorpion every ~120 ticks (100 dmg), hidden from a non-sonar enemy.
- **Aegis:** does shoot the Kirov down once facing it: 100 per Medusa, roughly ROF.

**Not confirmed / for the lead:**
- **Night Hawk group boarding:** in both runs only 2 of 5 or 6 selected GIs boarded. The others stood 2-3 cells away with no path. The boarders were the ones on the x≥64 side, so the terrain around the spawn point is the likely cause. Not filed.
- **Carrier sorties:** only one Hornet is ever airborne at a time (each sortie is shorter than the 150-tick HornetLauncher ROF). Whether the original launches all 3 together was not RE'd.
- **Sea Scorpion vs Harrier:** the HYD did not fire AA at the Harrier during its single pass (it kept its Destroyer target). Too short to judge.
- **AI Typhoon passivity:** an AI Typhoon 6.7 cells from my Destroyer never torpedoed it (its Sight is 4 < Range 7, and Osprey hits do not give it a target). This is consistent with sight-gated acquisition; not filed.
- **Dreadnought MinimumRange 8** and **Squid Culling at 25%** were not measured.

## Summary

| Severity | Count | IDs |
|---|---|---|
| MAJOR | 4 | W6-04-06, W6-04-08, W6-04-17, W6-04-09 |
| MINOR | 6 | W6-04-25, W6-04-26, W6-04-27, W6-04-35, W6-04-36 (still open), W6-04-37 (still open) |

**Top 5:**
1. W6-04-06: OmniFire is ignored on ships, so the Aegis loses to a lone Kirov.
2. W6-04-08: transport capacity is a count, not a Size sum (5+ tanks in the Amphibious Transport).
3. W6-04-09: a Dolphin or a repair cannot free a Squid-held ship (Sonic eject, RE `FUN_004D7250`).
4. W6-04-17: the Siege Chopper cannot deploy.
5. W6-04-25: subs surface on every shot (DecloakToFire, PROBABLE).
