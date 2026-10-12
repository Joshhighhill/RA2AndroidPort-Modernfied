# Wave 7 / 04: Soviet ground vehicles, behaviour gap analysis

**Tree:** master `b3a509f`, prebuilt `build/ra2engine.exe` (dated Oct 10). Read-only audit: no engine edits, no builds.

**Scope:** every Soviet-side `[VehicleTypes]` ground unit in YR `rulesmd.ini`:

| ID | Unit |
|---|---|
| HTNK | Rhino |
| APOC | Apocalypse |
| V3 | V3 Launcher, with its V3ROCKET |
| HTK | Flak Track |
| DRON | Terror Drone |
| DTRUCK | Demolition Truck (Libya) |
| TTNK | Tesla Tank (Russia) |
| HARV | War Miner, with HORV as its unloading class |
| SMCV | Soviet MCV |
| SAPC | Armored (amphibious) Transport |

**Out of scope, with notes:**
- The Siege Chopper (SCHP/SCHD, a JumpJet "AirPower" type) and the Kirov (ZEP) belong to the air slice. The SCHP deploy-to-artillery mode is still missing; this is carried over from W6-04-17 (PARTIAL), since `IsSimpleDeployer`, `DeployToLand` and `UnloadingClass=SCHD` are not read.
- The crusher fixes below also apply to the Allied vehicles.

**Method (calibrate-first).** Three opus sub-audits fed this report. Their evidence is in `docs/gap/wave7/04-evidence/`.

| Sub-audit | File | How it was done |
|---|---|---|
| RE-A: crush, targeting, guard, harvester, turret, MCV | `RE-A-crush-targeting.md` | Ghidra copy `D:\ra2-re\w7-sva`; raw output in `D:\ra2-re\w7-sva-out\a1..a16.txt` |
| RE-B: drone, Demo Truck, V3, self-heal, transports, Tesla | `RE-B-lifecycles.md` | Ghidra copy `D:\ra2-re\w7-svb`; raw output in `D:\ra2-re\w7-svb-out\b1..b17.txt` |
| PLAY | `PLAY-results.md` | 16 real-skirmish `--play` runs; scripts in `04-evidence/scripts/SV-*.play`, run one at a time through `04-evidence/run-play.ps1` (mutex). Speed slider 2, so 1 driver frame = 1 logic frame |

The audit also used:
- **Data:** `rulesmd.ini`, `artmd.ini` and `soundmd.ini` (extracted with `ra2extract`).
- **Prior reports:** `docs/re/*.md`, `docs/gap/wave6/04-units.md` and `REMAINING.md`. Items those reports mark FIXED are not re-reported: W6-04-01 Burst, -03 base-busting, -08 cargo Size, -09 Sonic/heal eject, -10 drone buildable, -14 OmniCrusher, R-050 crush steering, and others.

**Tags:**
- `[CONFIRMED-RE addr]`: read in `gamemd.exe` YR 1.000.
- `[CONFIRMED-DATA]`: read from the ini files or the clone source.
- `[CONFIRMED-PLAY script Lnn]`: measured in a play run.
- `[SUSPECTED]`: an inference that was not fully traced.

> **Tooling warning for other agents.** The shared Ghidra wrapper pattern `run.ps1` (`param($tag,$script,[string[]]$rest)`) binds only **one** address when it is started with `pwsh -File`. A batch decompile through it silently decompiles only the first function. Use `D:\ra2-re\w7-svb-scripts\run2.ps1` instead, which forwards `$args`.

## Summary

The basic combat numbers of the Soviet vehicles are right after the wave-6 fixes:
- Rhino shells, Tesla bolts, Terror Drone ticks (50 per 60 frames), Apocalypse self-heal (+1 HP per 14 frames, up to full), and the Demo Truck death blast all match.
- So do the V3's and Demo Truck's refusal to acquire targets on their own, and SMCV deploy and repack.

What is wrong is mostly lifecycle and rule-key behaviour around those numbers:

1. **Crushers squash allied infantry** (W7-04-01, the suspected bug, now CONFIRMED-RE). The original never crushes own or allied infantry; it makes them scatter. In the clone this affects co-op play and campaign allies.
2. **Passengers die with their transport** (W7-04-02). In the original, the infantry in a destroyed Flak Track, SAPC (on land), IFV or Battle Fortress climb out alive. `transport-cargo.md` §5 was wrong.
3. **`BuildTimeMultiplier` is ignored** (W7-04-03). The Rhino builds in 539 frames instead of 810, and the Tesla Tank in 755 instead of 972. The Grizzly and Lasher are also 1.5x too fast.
4. **`OpportunityFire` is ignored** (W7-04-04). Rhinos and War Miners never shoot while driving; a Rhino column drives past enemies without firing.
5. **V3 rockets home on moving targets** (W7-04-05). The original aims at the cell the target held at launch.
6. **You cannot send a vehicle to the Service Depot by clicking it** (W7-04-06). The only way to repair a vehicle, or to remove a Terror Drone, is to drive it onto a cell next to the depot by hand.

Below those are about 20 MINOR and COSMETIC items. The main ones:
- V3 minimum range is not applied to buildings.
- V3 reload cadence and per-rocket damage.
- Drone ejection by a heal, the Iron Curtain or the depot.
- AI behaviour: retaliating while moving, scattering when hit, `ToProtect` miner defence, the `AutoCrush` IQ gate.
- `DefaultToGuardArea`, acceleration, and a War Miner same-cell deadlock.
- Crush and drone visuals, `NoSpawnAlt` and `TargetLaser`.

## Per-unit matrix

Legend: OK (verified), WRONG, MISSING, PARTIAL, `–` (not applicable). Gap IDs are shown without the `W7-04-` prefix.

| Unit | Stats / keys | Weapons and targeting | Special behaviour | Player commands | AI use | Visuals and sounds |
|---|---|---|---|---|---|---|
| **HTNK** Rhino | WRONG: BuildTimeMultiplier 1.5 not read (03). Crusher OK | OK: 120mm; elite Burst 2 (W6 fix). WRONG: no fire on the move (04) | WRONG: crushes allied infantry (01). OK: own infantry scatter; enemy infantry crushed on contact; attack order on adjacent infantry shoots instead of crushing (PlayerAutoCrush=no) | MISSING: depot dock click (06) | OK: in AI roster. PARTIAL: crush gates (16), retaliate while moving (19), scatter (20) | PARTIAL: crush effects (25); turret idle-return hold (29) |
| **APOC** Apocalypse | OK: SelfHealing +1 HP per 14 frames up to full [CONFIRMED-RE 0x70B300, CONFIRMED-PLAY SV-FG/SV-G] | OK: 120mmx Burst 2; tusk is AA only (AG=no) [CONFIRMED-DATA]; landed aircraft are hit by the cannon. UNTESTED: tusk vs a flying aircraft (no airborne cheat spawn) | WRONG: crush (01) | MISSING: depot (06) | OK | MISSING: TargetLaser (28) |
| **V3** | OK: CanPassiveAquire=no [CONFIRMED-PLAY SV-C L45, SV-C2 L114]. MISSING: Accelerates/AccelerationFactor 0.01 (22) | WRONG: homing rocket (05); minimum range not applied vs buildings (07); reload cadence (08). SUSPECTED: damage 100 per rocket on wood (09). OK: shot down by a Patriot [CONFIRMED-PLAY SV-C L129]; rocket flies on if the launcher dies [CONFIRMED-RE 0x6B6780] | – | OK: attack order launches 13 frames later | OK | MISSING: NoSpawnAlt V3WO voxel, so two rockets are visible during a flight (27) |
| **HTK** Flak Track | OK: Passengers 5, SizeLimit 2 [CONFIRMED-PLAY SV-E] | OK: FlakTrackGun + AA gun (W6). UNTESTED: AA vs a flying aircraft | WRONG: passengers die with it (02) | OK: D unloads [CONFIRMED-PLAY SV-G L59]. COSMETIC: a full Flak Track still shows the enter cursor (31) | OK (Load/Unload team actions) | OK: Enter/Leave transport sounds |
| **DRON** Terror Drone | OK: buildable (269 frames). MISSING: DefaultToGuardArea (17) | OK: 50 dmg per 60 frames vs vehicles; instant kill vs infantry; host keeps moving; pops out when the host dies [CONFIRMED-RE 0x629A50/0x629D00, CONFIRMED-PLAY SV-B] | WRONG: a heal or depot eject places the drone alive (11); the Iron Curtain ignores it (12); no 3×ROF paralysis after an eject (13); suppression is measured after armour (14); can infest a unit that is being erased (15) | MISSING: depot dock (06) | OK | MISSING: host rock and sparks (26); jump not drawn (R-054, open). SUSPECTED: move-loop sound stuck at the start point (30). Depot kill plays a death anim (32) |
| **DTRUCK** Demolition Truck | OK: CanPassiveAquire=no / CanRetaliate=no [CONFIRMED-PLAY SV-D2 L42]. Accelerates (22) | OK: suicide on arrival (GAPOWR 750 → 13, consistent with the W6-04-03 per-foundation-cell DamageArea); death blast when killed [CONFIRMED-PLAY SV-D2 L60]; detonates even under the Iron Curtain; hits own units; no blast when erased [CONFIRMED-RE 0x6FD27C/0x700DA0/0x70CAC0] | OK: RadLevel site | OK | OK | OK: CreateSound (W6-04-27) |
| **TTNK** Tesla Tank | WRONG: BuildTimeMultiplier 1.3 (03) | OK: TankBolt; elite bounce (W6-04-02 fixed). Has no `OpportunityFire`, so 04 does not apply | WRONG: crush (01). OK: it does not charge Tesla Coils [CONFIRMED-DATA + RE 0x4693B0] | MISSING: depot (06) | OK | OK |
| **HARV** War Miner | MISSING: HarvesterTooFarDistance (23), ThreatPosed/ThreatAvoidanceCoefficient (24), ToProtect (18), Accelerates (22) | OK: a stationary, digging miner shoots a GI 3 cells away and keeps harvesting [CONFIRMED-PLAY SV-G L94-134]. WRONG: no fire while driving (04). WRONG: deadlock with crushable infantry in its own cell (10) | WRONG: AutoCrush only works through attack steering, so it never crushes while working (04/10) | OK | MISSING: AI call for help (18) | OK: EVA "ore miner under attack"; HORV unloading |
| **SMCV** | OK: OmniCrushResistant | – | OK: deploy facing gate, 36-cell relocation ring (yard shifted 2 cells when a Rhino blocks it, as in `FUN_00737D50`) [CONFIRMED-PLAY SV-H3, RE mcv-deploy.md §3]; EVA_CannotDeployHere; Sell repacks it with MCVRedeploys [CONFIRMED-PLAY SV-H2 L58] | OK | OK | OK: DeploySound |
| **SAPC** | OK: Passengers 12, SizeLimit 6, Size pool (W6-04-08) | – | WRONG: passengers die on a land kill (02). Original: they drown on water and escape on land | not re-tested in play | OK | OK |

## Gaps

### W7-04-01: Crushers squash ALLIED infantry, and allied infantry is never asked to scatter. MAJOR

- **Evidence.** [CONFIRMED-RE 0x5F650A, 0x5F6557, 0x73DFA0, 0x7405DE, 0x4B2550]
- **Original.**
  - `ObjectClass::IsCrushable` (`FUN_005F64C0`) refuses any target the crusher's owner is allied with. `HouseClass::IsAlliedWith` (`FUN_004F98B0`) counts the own house as allied, and the check is made in both the OmniCrusher branch and the Crushable branch.
  - `UnitClass::Can_Enter_Cell` (`FUN_0073DFA0`) returns `Temp` for a standing allied occupant and `MovingBlock` for a moving one.
  - The per-cell crush (`FUN_007404B0` @0x7405DE) skips allied occupants.
  - The Drive locomotor (`FUN_004B2550`) calls `CellClass::ScatterContent` for allied occupants, not just own-house ones.
- **Clone.**
  - `engine/world.cpp:5670` crushes whenever `can_crush(u, v) && v.house != u.house`.
  - `can_crush` (`engine/combat.cpp:557-566`) has no alliance test.
  - The scatter request (`world.cpp:5762-5770`) only covers `units[o].house == u.house`.
  - Own-house infantry are handled correctly [CONFIRMED-PLAY SV-A L42-53: 2 Conscripts scattered, 0 lost].
  - The allied case could not be measured, because the skirmish lobby has no team control.
- **Effect.** In co-op, and in campaigns with allied AI houses, the player's tanks kill allied infantry and allied AI tanks kill the player's infantry.
- **Fix.**
  - Pass an alliance predicate into `step_units`, for example via `MoveSpeedCtx` next to `scatter_out`.
  - Crush only `!allied(u.house, v.house)`.
  - Request a scatter for any allied idle infantry.
  - Give `can_crush` an `allied` parameter so the steering at `combat.cpp:1273` also refuses allies.
  - Optional: skip Iron-Curtained victims, as `IsCrushable` does through `vtbl+0x160`.

### W7-04-02: Passengers die with their transport; in the original they escape alive. MAJOR

- **Evidence.**
  - [CONFIRMED-RE 0x736FD1..0x7371D6] the KILLED branch of `UnitClass::ReceiveDamage` (`FUN_00736D60`).
  - [CONFIRMED-PLAY SV-E L117-118] the clone loses all 5 passengers: `units_lost` +6.
- **Original.** For a non-Crashable unit, every passenger is unlimboed onto the transport's cell and scattered. A passenger is killed only in these cases:
  - it cannot stand on that cell (a SAPC killed on water drowns its infantry);
  - Unlimbo fails;
  - a prevent-escape flag is set.

  The kill-all paths are separate and narrower: the Explodes/Suicide death (`0x701A97`), `KillPassengers` (`FUN_00707130`), and aircraft transports.
- **Clone.** `engine/world.cpp:1723-1751` (`step_cargo`) tombstones every carried unit, citing `transport-cargo.md` §5. That doc only read the two kill-all sites, so it is wrong for UnitClass.
- **Effect.** Every transport fight is out of balance: Flak Track, SAPC on a beach, IFV, and the Battle Fortress's 5 infantry.
- **Fix.**
  1. In the transport-death pass, eject each passenger, last in first out, onto the transport's cell or a free cell nearby (`eject_cell`), and scatter it.
  2. Kill a passenger only if its domain cannot use that cell, or if the transport `Explodes=yes`, died by suicide, or is an aircraft.
  3. Correct `docs/re/transport-cargo.md` §5.

### W7-04-03: `BuildTimeMultiplier` is ignored. MAJOR (balance)

- **Evidence.**
  - [CONFIRMED-RE 0x6F3D4E] `Time_To_Build FUN_006F3D00` multiplies by `type+0x608` (construction.md §7).
  - [CONFIRMED-DATA] `git grep -i BuildTimeMultiplier engine` finds 0 hits. The rules set HTNK 1.5, TTNK 1.3, and also MTNK 1.5, LTNK 1.5 and ROBO 1.3.
  - [CONFIRMED-PLAY SV-I L52/L54] measured from the cameo click, at full power with one factory.
- **Measured vs expected.** The expected value adds the clone's (RE'd, faithful) 54-step quantisation.

| Unit | Measured | Expected |
|---|---|---|
| Rhino | 539 frames | `54*floor(ftol(567*1.5)/54)` = 810 |
| Tesla Tank | 755 frames | 972 |

- **Clone.** `engine/production.cpp:8-13` `build_frames(cost, build_speed)` uses Cost only.
- **Fix.** In `production::start()` (and the AI's and the campaign's production paths), compute `t = ftol(ftol(base × countryMult) × BuildTimeMultiplier)` before dividing it into steps. Keep the ftol order. Read the key into the type definition (+save).

### W7-04-04: `OpportunityFire` is not read; vehicles never acquire targets while moving or harvesting. MAJOR

- **Evidence.** [CONFIRMED-RE 0x6F9BDE, 0x708710]
  - `TechnoClass::AI` runs the threat scan in the Move, Harvest and Guard missions when `CanAutoTarget` is true.
  - Without `OpportunityFire` (+0x6AF), only Guard passes. With it, Move and Harvest pass too.
  - The target becomes TarCom while the move continues, so the turret fires on the move.
- **Data.** [CONFIRMED-DATA] HTNK, HARV, MTNK, LTNK, UTNK, YTNK, ROBO, TELE, MIND, DISK, SMIN, DEST, AEGIS, CDEST.
- **Clone.** `engine/combat.cpp:1142` gives no auto-acquire when `u.moving()`. So:
  - a Rhino column on a move order drives past enemies without firing;
  - a War Miner driving between ore and the refinery never shoots;
  - with no target, the War Miner's `AutoCrush` steering never triggers while it works.
- **Fix.**
  - Read the key.
  - For those types, run the acquisition scan while moving or harvesting and set the target without clearing the path; turreted vehicles already aim and fire independently.
  - Skip the guard leash (`combat.cpp:1330-1341`) for opportunity targets, so it does not override the move or harvest path [SUSPECTED interaction].

### W7-04-05: V3 (and Dreadnought) rockets home on the live target; the original flies to the cell captured at launch. MAJOR

- **Evidence.** [CONFIRMED-RE 0x6B7176, 0x54E040, 0x54E160, 0x661D00]
  - `SpawnManagerClass::AI` hands each launched missile to `KamikazeTrackerClass::Add`.
  - `Add` stores `MapClass::Cell(Target->GetCoords())` at that moment.
  - The tracker update (every 30 frames) re-targets that **fixed cell**.
  - `RocketLocomotionClass::Process` steers at that cell.
- **Clone.** `engine/world_an.cpp:373-378` refreshes `f.tx/ty/tz` from `rocket_target_point` every frame, so the rocket follows a moving tank.
- **Effect.**
  - V3s and Dreadnoughts are much stronger against vehicles than in the original, where a moving tank dodges.
  - The original's side effect, where whatever now stands on the target cell takes the hit, is missing.
- **Fix.** Freeze the impact point at launch (cell centre `cell*256+128`, ground z) and drop the per-frame refresh. A dead target already keeps the last point.

### W7-04-06: No order to send a vehicle to the Service Depot (click on your own depot). MAJOR (player command)

- **Evidence.** [CONFIRMED-PLAY SV-B L86, SV-B2 L48/L70-160]
  - With a damaged Rhino selected, the pointer over your own NADEPT shows `select_building`, and the click only selects the depot.
  - A damaged Rhino stayed at 200/400 HP for 1500 frames.
  - A drone-infested Rhino was eaten while the player clicked the depot.
  - [CONFIRMED-DATA] `input_original.cpp`, `input.cpp`, `command.cpp` and `unit_orders.cpp` contain no depot or UnitRepair order.
  - The depot logic itself works [CONFIRMED-PLAY SV-B3]: a vehicle moved by hand to a cell next to the depot loses its drone and repairs to full.
- **Original.** With a damaged own vehicle selected, a `UnitRepair=yes` building shows the enter/dock cursor. The click gives a RADIO dock order (`TechnoClass::ReceiveCommand FUN_006F4010` case 0x1C repair steps). This is the standard RA2 way to remove a Terror Drone.
- **Fix.** In the original-scheme input, treat a left-click on an own `UnitRepair` building with own vehicles selected (vehicles only, not aircraft) as a dock order:
  - move to the pad, using `step_service_depots`' adjacency;
  - show the "enter" cursor for it;
  - give the order a voice.

  This applies to the Allied GADEPT as well.

### W7-04-07: V3 minimum range is not applied to building targets. MINOR

- **Evidence.**
  - [CONFIRMED-PLAY SV-C2 L48/L74] A V3 about 3.5 cells from a GAPOWR fired: 750 → 664 HP. It held position.
  - [CONFIRMED-DATA] `engine/combat.cpp:1264` computes `too_close` only in the unit-target branch.
- **Original.** `MinimumRange=5` on `V3Launcher` blocks the shot for any target (the weapon's fire error). The unit backs off or waits.
- **Fix.** Apply the same `min_range` test in the building-target fire path (and in the force-fire ground-cell path).

### W7-04-08: V3 reload cadence. MINOR (refines R-055)

- **Evidence.** [CONFIRMED-RE FUN_006B68B0, 0x6B72E0, 0x73FDE0]
  - `SpawnManagerClass::AI` ticks every 10 frames.
  - A missile node is held for `V3RocketPauseFrames + V3RocketTiltFrames` (0 + 60) after launch. It then goes to state 7, which waits `SpawnRegenRate` (400).
  - The next launch is therefore possible about 470-490 frames after the previous one, whatever happens to the rocket.
- **Clone.** `engine/world_an.cpp:263-283` starts the regen at launch (`out_held = 0`).
  - Measured launch-to-launch: 454 and 453 frames [CONFIRMED-PLAY SV-C L56/L60]. That is close to the original's band, but by a different mechanism, so the result is order-dependent.
  - Because of this RE, R-055's "slot never freed" theory is moot.
- **Fix.** Hold the node for Pause + Tilt of the rocket's block (V3 60, DMisl 80) before starting the regen timer. Optionally quantise both waits to 10-frame ticks.

### W7-04-09: V3 rocket damage vs a building looks halved. MINOR [SUSPECTED]

- **Evidence.**
  - [CONFIRMED-PLAY SV-C L52/L61] Each rocket took 100 HP off a GAPOWR. Nominal: `V3RocketDamage=200`; `[V3WH]` wood Verses is 100%, `CellSpread=1`, `PercentAtMax=.25`.
  - The rocket aims at the 2x2 footprint centre. Linear falloff over the distance from the impact point to the nearest cell centre could explain it, but the W6-04-03 per-foundation-cell DamageArea should hit each cell inside the 1-cell spread, which would give more damage, not less.
- **Fix.** RE-check how the clone resolves the V3 impact point and DamageArea for a building target, compared with `BulletClass::Detonate` and `DamageArea FUN_00489190`, then adjust.

### W7-04-10: War Miner deadlock with crushable infantry in its own cell. MINOR (edge case)

- **Evidence.** [CONFIRMED-PLAY SV-G L162-229] An enemy Conscript in the miner's cell kept 125 HP for 150 frames. The miner had it as its target and neither moved nor fired.
- **Clone.** `engine/combat.cpp:1279-1285`: when the crushable target is within `Crush` 1.8 cells, the crusher pushes a move to the target's cell and `continue`s past the fire step. If it already stands in that cell, the move is a no-op, so it neither crushes nor fires. It is unclear how often this happens in normal play, because the Conscript was cheat-spawned.
- **Original.** `Mission_Attack FUN_007402F0` crush steering drives over the cell, and the per-cell crush (`FUN_007404B0`) kills occupants of the cell being entered or occupied.
- **Fix.** If the target shares the crusher's cell, or the path request is empty, either crush it at once (it is on the crusher's cell) or fall through to the fire step.

### W7-04-11: A heal or Service Depot ejection should DESTROY a non-naval drone; the clone frees it. MINOR

- **Evidence.** [CONFIRMED-RE 0x4D7250, 0x629F40, 0x6F4010]
  - A heal hit (negative damage) or a depot repair step sets `Suppression = 50`, then `ExitUnit`.
  - For a non-Naval parasite (Terror Drone, dog), `ExitUnit` with suppression running sets Health to 0 and UnInits it. Only the Giant Squid is placed beside the host.
- **Clone.** `engine/world_abilities.cpp:651-668` places the drone on a free cell after a heal hit (IFV or engineer repair: `world_w5.cpp:447`, `world_an.cpp:184`). It sets `suppress_until` but never checks it.
- **Fix.** In the eject branch, if the parasite is not naval and `F < suppress_until`, kill it silently (no death anim). Otherwise place it.

### W7-04-12: The Iron Curtain on a drone-infested vehicle should destroy the drone. MINOR

- **Evidence.** [CONFIRMED-RE 0x4DE9F0] `FootClass::IronCurtain` does the following:
  - for an Organic unit, it applies C4 damage. This confirms the clone's RE-FALLBACK in `world_sw.cpp:397`.
  - for a host with a parasite, it sets suppression to 50, calls `ExitUnit` (so the drone dies, as in 11), clears the host's paralysis, then applies the curtain.
- **Clone.** `engine/world_sw.cpp:388-410` (`an_iron_curtain`) ignores `parasite_on_me`. The drone stays inside, harmless (its ticks are blocked by the IC check in `world_abilities.cpp:209`), and it resumes eating when the curtain ends.
- **Fix.** In `an_iron_curtain`, eject the parasite: kill it if it is non-naval, place it if it is a squid. Clear the host's paralysis. Update the comment to cite `FUN_004DE9F0`.

### W7-04-13: A drone ejected alive is not paralysed for 3×ROF. MINOR

- **Evidence.** [CONFIRMED-RE 0x62A2A0] The placement branch of `ExitUnit` sets the drone's ParalysisTimer (+0x6A0) to `weapon.ROF*3` (180 frames) and clears its mission, target and destination. This applies to a Chrono Legionnaire teleport (`0x7188DF`) and to a Sonic hit with no suppression. The host-death path (`FUN_00629D00`) does not paralyse.
- **Clone.** `world_abilities.cpp:141-152` (teleport) and `651-668` (Sonic) let the drone jump again at once.
- **Fix.** Freeze the drone for `3*w_rof` on those two paths only.

### W7-04-14: Drone suppression uses HP loss after armour instead of raw damage. MINOR

- **Evidence.** [CONFIRMED-RE 0x4D7250] On each hit from anything but the drone: `if (SuppressionThreshold < rawDamage) Suppression = 2*dmg - threshold`. The test runs before Verses or veterancy are applied.
- **Clone.** `world_abilities.cpp:671-675` uses the host's summed HP loss per frame after armour. This under-suppresses heavy hosts: a 90-damage Grizzly shell at 25% does 22.
- **Fix.** Record the pre-armour damage of each non-drone hit on a host in the combat damage path, as the `w6.sonic_hit` flag already does. Use the per-frame maximum.

### W7-04-15: A drone can infest a unit that a Chrono Legionnaire is erasing. MINOR

- **Evidence.** [CONFIRMED-RE 0x62A380] `CanInfect` also requires `+0x2E4 == 0` (no TemporalClass targeting the unit). If the infest fails, the drone is unlimboed nearby (`FUN_0062A420`).
- **Clone.** `world_abilities.cpp:405-412` does not check `warp_remaining`.
- **Fix.** Add `|| v.warp_remaining > 0` to the refusal.

### W7-04-16: AutoCrush steering misses the IQ, difficulty, ally and cloak gates. MINOR

- **Evidence.** [CONFIRMED-RE 0x742720; the IQ field name is SUSPECTED] `ShouldCrush` requires all of:
  - house IQ ≥ `[IQ] AutoCrush` (Rules+0x1448) for a computer house;
  - difficulty index ≠ 2;
  - `IsCrushable`, which excludes allied and invulnerable targets;
  - a target that is not cloaked from the owner (`vtbl+0xC8`).
- **Clone.** `engine/combat.cpp:1273-1276` checks only `house_is_computer || PlayerAutoCrush || AutoCrush`.
- **Fix.** Add those gates, together with 01.

### W7-04-17: `DefaultToGuardArea` is not read. MINOR

- **Evidence.** [CONFIRMED-RE 0x737BB6 (unit), 0x51CB2A (infantry)] When idle, the unit enters Area_Guard instead of Guard if the flag is set and it is not in a team. The flag is set on DRON, DOG, ADOG and others [CONFIRMED-DATA].
- **Clone.** `tg.area_guard` is set only by an explicit order or a map mission. An idle Terror Drone watches about 2 cells (⌊1.83⌋+1) instead of area-guarding.
- **Fix.** When a unit goes idle and is not in a team, set `tg.area_guard = type.default_to_guard_area`.

### W7-04-18: `ToProtect` is not read (the AI does not defend an attacked miner). MINOR (AI)

- **Evidence.** [CONFIRMED-RE 0x701C43, 0x707500] When a computer-owned `ToProtect` unit (HARV, CMIN, the navy) is damaged by a non-allied infantry or vehicle attacker, `FUN_00707500` recruits up to 6 idle same-house units to attack the attacker. Their budget is the attacker's threat × Rules+0xB18.
- **Clone.** Not implemented. The AI loses miners to harassment that the original answers.
- **Fix.** On damage to a computer `ToProtect` unit, order up to 6 nearby idle combat units of the same house to attack the attacker.

### W7-04-19: AI units do not return fire while moving. MINOR (AI)

- **Evidence.** [CONFIRMED-RE 0x707C40] `CanRetaliateToAttacker`:
  - A computer owner retaliates in any mission.
  - A human owner needs no TarCom and `PlayerReturnFire` (rulesmd `no`), or the Guard/AreaGuard/Patrol mission.
  - `CanPassiveAquire` and `CanRetaliate` themselves are honoured correctly (V3, DTRUCK) [CONFIRMED-PLAY SV-C, SV-D2].
- **Clone.** `combat.cpp:262/715` retaliates only when idle, for every house.
- **Fix.** Let computer houses retaliate while moving. Keep the idle rule for humans.

### W7-04-20: Computer units do not scatter when damaged; `PlayerScatter` is not read. MINOR (AI)

- **Evidence.** [CONFIRMED-RE 0x702006, 0x702123] A damaged Foot with no TarCom and no locked mission scatters when any of these holds:
  - the owner is a computer;
  - `PlayerScatter` is set;
  - it has the vet/elite SCATTER ability.
- **Clone.** `engine/world_w5.cpp:177-191` handles only the ability branch.
- **Fix.** Add the computer-owner and `PlayerScatter` branches.

### W7-04-21: Removed. The SMCV relocation is faithful; see Verified OK.

### W7-04-22: `Accelerates` / `AccelerationFactor` / `DeaccelerationFactor` are not read. MINOR

- **Evidence.**
  - [CONFIRMED-RE 0x4B0EA4, 0x4B10D3] With `Accelerates` (constructor default **yes**), the drive locomotor ramps the speed fraction by `AccelerationFactor` per frame and slows down near the destination.
  - [CONFIRMED-DATA] The Rhino, Apocalypse, Tesla Tank, Flak Track and Drone set `Accelerates=false`. HARV, SMCV, SAPC, DTRUCK and V3 keep the ramp. The V3 is notably sluggish (`AccelerationFactor=0.01`, about 100 frames to full speed).
- **Clone.** Every vehicle starts and stops at full speed.
- **Fix.** Ramp `speed_mult` for `Accelerates` types. The constructor defaults of the factors still need RE.

### W7-04-23: `HarvesterTooFarDistance` is not read. MINOR

- **Evidence.** [CONFIRMED-RE 0x73DB0E] A returning miner docks (radios the refinery) only within 5 cells. Otherwise it drives near the refinery (`FUN_0056D750`) and decides again.
- **Clone.** `engine/harvest.cpp:425-439` binds `dock_refinery` at any distance.
- **Fix.** Bind within 5 cells; otherwise path to near the refinery and re-pick on arrival.

### W7-04-24: `ThreatPosed` / `ThreatAvoidanceCoefficient` are not read. MINOR

- **Evidence.** [CONFIRMED-RE 0x707FC0, 0x4D720A; path-cost consumer `FUN_006A1310` SUSPECTED] HARV sets `ThreatPosed=0` and `ThreatAvoidanceCoefficient=.65`. Miners in the original weigh threat when pathing.
- **Clone.** Not implemented.
- **Fix.** Low priority; it needs a per-house threat map.

### W7-04-25: Crush effects. COSMETIC

- **Evidence.** [CONFIRMED-RE 0x7406A2-0x740764] The per-cell crush:
  - plays the VICTIM's `CrushSound` (InfantrySquish);
  - credits the kill and veterancy (the clone does too, `world.cpp:5674`);
  - removes the victim outright: no Die sequence, no corpse, no DieSound;
  - decloaks the crusher, and tilts it after crushing a vehicle.
- **Clone.**
  - The victim plays the normal death and leaves a `DeadBodies` corpse (`pres_game.inc:1023-1035`).
  - `pres_game.inc:1062-1064` also plays the crusher's own `CrushSound` (TankCrush, which is the sound of being crushed).
- **Fix.** Flag crush kills to vanish at once with no corpse, and drop the crusher-side sound.

### W7-04-26: No host rocking or sparks on drone ticks. COSMETIC

- **Evidence.** [CONFIRMED-RE 0x629A50] On each tick against a vehicle, the original creates a particle system (Rules+0x1020) at the host and rocks it (`vt+0x3D8`, 1.5f, random side). This is the visible "shaking" of an infested tank.
- **Clone.** Neither is done.
- **Fix.** Add a presentation-side body rock and the spark system on the ParasiteTick event.

### W7-04-27: `NoSpawnAlt` is not supported (V3 shows its rocket while it is in flight). COSMETIC

- **Evidence.** [CONFIRMED-RE loader 0x5F8C2E; draw consumer SUSPECTED] `ObjectType+0x1E8`. The rules comment says it uses the "xxxxWO" alternate voxel (V3WO) when out of spawns.
- **Clone.** `git grep NoSpawnAlt engine` finds 0 hits.
- **Fix.** Draw `V3WO` while `spawn_ready == 0`, after checking that the voxel exists in the YR archives.

### W7-04-28: `TargetLaser` is not drawn (Apocalypse). COSMETIC

- **Evidence.** [CONFIRMED-RE 0x6FE957] `TechnoClass::Fire` draws a target-laser line when `+0xC90` is set (APOC `TargetLaser=yes`).
- **Clone.** The key is not read.
- **Fix.** Draw a short laser line from the FLH to the target on each shot.

### W7-04-29: The turret returns to the hull at once. COSMETIC (known)

- **Evidence.** [CONFIRMED-RE FUN_00735A60, voxel-render.md §5] The original holds the turret for `ROF+5` frames after the last shot, then slews it to the body facing (or the move direction).
- **Clone.** `engine/world.cpp:105-145` (`step_turrets`) starts the slew at once. The rate is correct.
- **Fix.** Add the `ROF+5` hold.

### W7-04-30: The Terror Drone's move-loop sound is anchored where the move started. COSMETIC [SUSPECTED]

- **Evidence.**
  - [CONFIRMED-DATA] `TerrorDroneMoveLoop` is `Control=Random loop all attack decay` in soundmd.
  - `pres_game.inc:1112` plays `MoveSound` once on move start through `play_at`. The mixer loops a `loop` sound until it is stopped (`audio_mixer.cpp` start_), and `live_` re-attenuates it at the **captured start position** (`pres_audio.inc:363`, 410-418).
  - Nothing stops it when the drone halts. The loop therefore keeps playing at the spot where the drone started moving, until the camera leaves that spot.
  - Not heard in play: the harness runs with audio off.
- **Original.** Locomotor move sounds are owned by the object and stop on halt [SUSPECTED, not traced].
- **Fix.** Use `loop_at` and `update_loop` with the unit's live position for `loop` MoveSounds (the aircraft `w6_flight` pattern), and stop the loop when the unit stops.

### W7-04-31: A full transport still shows the "enter" cursor. COSMETIC

- **Evidence.** [CONFIRMED-PLAY SV-E L90] With a 6th Conscript selected over a full Flak Track (5/5), the cursor is `enter`. The click is correctly refused; the cargo stays at 5.
- **Original.** The no-enter cursor is shown.
- **Fix.** Check capacity (`cargo_can_take`) in the cursor resolver.

### W7-04-32: The Service Depot kills a riding drone on arrival, with a death animation. COSMETIC

- **Evidence.** [CONFIRMED-RE 0x6F4010 case 0x1C] The original removes the drone only on a paid repair step, and silently (UnInit).
- **Clone.** `world.cpp:2465-2470` calls `ability_kill_unit` on arrival, even with no money. The gameplay outcome is the same, apart from the no-money case.
- **Fix.** Fold this into 11: remove the drone on the first repair step, silently.

### Carried over (still open, not new)

- **R-054:** the drone, dog and squid jump projectiles are not drawn.
- **W6-04-17 (PARTIAL):** the Siege Chopper deploy-to-artillery mode.

## Verified OK

- **Rhino.**
  - Own infantry in its path scatter; none are crushed [CONFIRMED-PLAY SV-A L42-53].
  - An enemy Conscript in its path is crushed on contact at full HP [CONFIRMED-PLAY SV-A2 L51-66].
  - An attack order on an adjacent enemy Conscript makes it shoot without moving, per `PlayerAutoCrush=no` (22 per shot, kill in about 340 frames) [CONFIRMED-PLAY SV-A2 L70-89].
  - Idle, it auto-fires at adjacent infantry [CONFIRMED-PLAY SV-A2 L91-104].
- **Terror Drone.**
  - The cameo is present; it builds in 269 frames [CONFIRMED-PLAY SV-I L45/L58].
  - It latches 23 frames after the click and does 50 damage per 60 frames to a Grizzly (kill in 323 frames) [CONFIRMED-PLAY SV-B L48-61; CONFIRMED-RE 0x629A50].
  - It kills infantry instantly [CONFIRMED-PLAY SV-B; CONFIRMED-RE 0x629A50].
  - The host keeps moving [CONFIRMED-PLAY SV-B L58].
  - The drone survives the host's death [CONFIRMED-PLAY; CONFIRMED-RE 0x629D00].
  - The depot kills it once the host is adjacent, and the host then repairs to full [CONFIRMED-PLAY SV-B3].
  - It cannot target ships or aircraft, and an already-parasited host is refused [CONFIRMED-DATA].
  - It cannot be mind-controlled and never goes elite [CONFIRMED-DATA].
- **V3.**
  - It does not fire on its own (CanPassiveAquire=no) [CONFIRMED-PLAY SV-C L45, SV-C2 L114].
  - On an attack order it launches 13 frames later; the rocket lands about 244 frames after launch at 15 cells [CONFIRMED-PLAY SV-C L51-52].
  - A Patriot shoots the rocket down [CONFIRMED-PLAY SV-C L129].
  - The rocket flies on if the launcher dies [CONFIRMED-RE 0x6B6780].
- **Demolition Truck.**
  - It suicides on arrival (W6-04-03 DamageArea per foundation cell) [CONFIRMED-PLAY SV-D L58].
  - It fires its death weapon when killed en route [CONFIRMED-PLAY SV-D2 L56-61].
  - It is idle-passive [CONFIRMED-PLAY SV-D2 L42].
  - It still detonates under the Iron Curtain, hits its own side (AffectsAllies defaults to yes), gives no blast when erased, and leaves a RadLevel site [CONFIRMED-RE 0x6FD27C/0x700DA0/0x70CAC0; clone `world_abilities.cpp:386-389, 527-545`].
- **Flak Track.**
  - It takes 5 Conscripts and refuses a 6th [CONFIRMED-PLAY SV-E L53-93].
  - D unloads it [CONFIRMED-PLAY SV-G L55-60].
  - Size pool and SizeLimit (W6-04-08).
- **Apocalypse.**
  - SelfHealing gives +1 HP per `ftol(RepairRate*900)=14` frames, up to FULL, not 50% [CONFIRMED-RE 0x70B300; CONFIRMED-PLAY SV-FG/SV-G, 380 → 522].
  - The tusk is AA only [CONFIRMED-DATA].
- **Tesla Tank.** It does not charge Tesla Coils (no `ElectricAssault`) [CONFIRMED-DATA + abilities-w5 §3]. It kills a Grizzly in 3 bolts (W6).
- **War Miner.**
  - Stationary and digging, it shoots a GI 3 cells away and keeps harvesting [CONFIRMED-PLAY SV-G L94-134].
  - Idle, it shoots infantry 2 cells away and does not crush it beyond the 1.8-cell Crush radius [CONFIRMED-PLAY SV-H L44-82].
  - The EVA "ore miner under attack" plays (`pres_game.inc:1116`).
- **SMCV.**
  - A Rhino in the footprint makes the yard deploy one ring offset away, as in `FUN_00737D50`'s 36-offset ring [CONFIRMED-PLAY SV-H3; RE mcv-deploy.md §3].
  - Against a building there is no deploy [CONFIRMED-PLAY SV-H L135].
  - Sell repacks it with MCVRedeploys [CONFIRMED-PLAY SV-H2 L58].
  - The facing gate, the EVA, the NoDeploy cursor and OmniCrushResistant are present [CONFIRMED-DATA].
- **Keys read correctly** [CONFIRMED-DATA]: Strength, Armor, Speed, Turret, Primary, Secondary, ElitePrimary, EliteSecondary, Crusher, AutoCrush, Passengers, SizeLimit, Size, Harvester, Dock, Storage, UnloadingClass, DeploysInto, Spawns, SpawnsNumber, SpawnRegenRate, MissileSpawn, Explodes, DeathWeapon, SelfHealing, SelfHealUnitFrames, CanPassiveAquire, CanRetaliate, GuardRange, Parasiteable, SuppressionThreshold, ImmuneToRadiation, ImmuneToPsionics, OmniCrushResistant, RequiredHouses, DeployTime, MoveSound, CrushSound, CreateSound, DeploySound, VoiceHarvest, DebrisTypes, DamageParticleSystems, PrimaryFireFLH, TurretOffset, and the DRON WalkFrames and FiringFrames.
- **Keys that are harmless to ignore:**
  - `IsTilter` has no gameplay consumer in the original [CONFIRMED-RE].
  - `TooBigToFitUnderBridge` is draw-only [CONFIRMED-RE 0x73A0E0].
  - `ElitePrimaryFireFLH` (TTNK) has the same value as the normal FLH.
  - `PBarrelLength` (TRUCKA) is not used by the Demo Truck.

## Suggested fix order

| Order | Gaps | Area | Size |
|---|---|---|---|
| 1 | 01 + 16 | Crush alliance plus the ShouldCrush gates (`world.cpp` step, `combat.cpp` can_crush/steer) | S |
| 2 | 02 | Transport passenger escape (`world.cpp` step_cargo; correct transport-cargo.md §5) | S |
| 3 | 03 | BuildTimeMultiplier (production start: player, AI, campaign) | S |
| 4 | 06 | Service Depot dock order (input + cursor) | S-M |
| 5 | 04 | OpportunityFire (combat acquisition while moving or harvesting; leash exemption) | M |
| 6 | 05 + 08 | V3/DMISL fixed-cell impact and node hold (`world_an.cpp`) | S |
| 7 | 07, 10, 11-15 | V3 minimum range on buildings, miner deadlock, drone lifecycle | S each |
| 8 | 17-20, 22-24 | AI and rule-key polish | S-M |
| 9 | 25-32 | Cosmetics | S |
