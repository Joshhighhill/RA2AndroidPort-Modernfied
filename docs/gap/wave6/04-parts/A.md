> Appendix to `docs/gap/wave6/04-units.md`. Gap IDs in this part were remapped to the consolidated W6-04-nn numbering (duplicates across parts share one ID).

# W6-04 part A: Allied infantry and Allied ground vehicles

Agent A, worktree `D:\ra2-wt\w6-units` @ e2b569e, release exe built by the lead.
Spec: `D:\ra2-re\w5-abil-out\rulesmd.ini` (YR). The Rocketeer (JUMPJET) is agent C's.

## Evidence

Scripts are in `docs/gap/wave6/04-evidence/scripts/`:

| Script | What it does | Result |
|---|---|---|
| `A-land.play` | First broad pass | Contaminated: the skirmish start force kept killing the spawned test enemies. Fixed with `slider:units=0` on the second run. |
| `A-probe.play` | Dog, Guardian GI, enemy Mirage, Prism, GI deploy timing, Chrono Legionnaire erase | Run clean |
| `A-dog.play` | Dog bite and dog pursuit | Run clean |
| `A-probe2.play` | Tanya and SEAL vs a Rhino, Tank Destroyer vs Conscript, Robot Tank with no Control Center, Battle Fortress with a GI | Run clean |
| `A-water.play`, `A-water2.play` | Bay of Pigs: Tanya swims, Tanya and SEAL vs ships | Run clean (`A-water2`) |

- **Logs:** `play-out/w6-A-*/play.log`. Results are copied to `04-evidence/A-*-result.txt`.
- **Screenshots:** `04-evidence/A-tanya-swim.jpg` and `04-evidence/A-mirage-own.jpg`.
- **Timing:** times below are logic frames (`anim_tick` deltas). The skirmish runs at game speed 1, which is 4 logic ticks per driver frame.

## 1. Matrix

- **OK** = measured in play. **OK?** = checked by reading code and rules only.
- Gap IDs below are `W6-04-nn` unless they carry another prefix.

| Unit | Move | Weapon(s)+ROF | Range (3-D/min) | Warhead Verses/splash | Projectile | Special/deploy | Vet/elite | Garrison/passengers | Sight/cloak/detect | Sounds |
|---|---|---|---|---|---|---|---|---|---|---|
| E1 GI | OK? Speed 4, foot, Crushable | OK. M60 15/20; deployed Para 25/15 measured | OK. 4, deployed 5 measured | OK? SA/SSA | OK? InvisibleLow | OK. Deploy measured: kills a Conscript in 88 frames deployed vs 304 undeployed. The deploy itself is instant, with no deploy-sequence delay | OK? M60E/ParaE | OK? Occupier UCPara/UCElitePara (R-053 open) | OK? Sight 5 | OK? Deploy/Undeploy sound (R-130) |
| GGI Guardian GI | OK? Speed 3 | OK. Deployed MissileLauncher 40/40 measured vs a Rhino (−40 per hit) | OK. 8 / min 1 | OK. GUARDWH 100% vs heavy | OK? AAHeatSeeker2 AA+AG; undeployed M60 has no AA | WRONG: DeployedCrushable=no not read (W6-04-29) | OK? MissileLauncherE | N/A (Occupier=no) | OK? 6 | OK? |
| ENGINEER | OK? | N/A (DefuseKit, R-046) | N/A | N/A | N/A | OK? Capture, defuse and IFV repair (R-048) | N/A (Trainable=no) | IFVMode 1 → RepairBullet (R-048 play) | OK? 4 | OK? |
| ADOG Allied dog | OK? Speed 8 | OK. Bite kills a Conscript at 3 cells; refuses a Rhino (0%) | WRONG: pursuit stalls at 2.2 cells (W6-04-38) | OK? ParasiteDog 100% vs inf, 0% vs vehicles | MISSING: jump not drawn (R-054) | OK? DetectDisguise (R-148 radius guessed) | OK? | IFVMode 0 → HoverMissile (OK) | OK? 9, DetectDisguise | OK? |
| SPY | OK? | OK? MakeupKit disguise (W2-SK-22 play) | N/A | N/A | N/A | OK? Infiltrate dispatch per abilities-w5 §1 (R-042) | N/A | IFVMode 2 → CRM60 | OK? PermaDisguise | OK? |
| TANY Tanya | OK. Swims (amphibious, measured); Crushable=no | OK. Pistols 125/5; C4 Sapper vs Rhino on land measured (kill in ~68 frames) | OK. 6 / C4 1.5 | OK? HollowPoint2 | OK? | WRONG: never reaches a ship to plant C4 (W6-04-07) | OK? DoublePistolsE; SelfHealing | IFVMode 4 → CRMP5 125/7/10 (OK) | OK? 8 | MISSING: EnterWater/LeaveWater/Create sounds (W6-04-27) |
| SNIPE Sniper (UK) | OK? Speed 4 | OK. AWP one-shots a GI at 7 cells (auto-acquired, GI dead within 2 driver frames); ROF 150 | OK. 14 measured | OK? HollowPoint 200% vs none | OK? | N/A | OK. Elite ROF 36 = AWPE 60 × VeteranROF 0.6 | N/A (not an Occupier in rulesmd); IFV → AWPE 125/14/60 (OK) | OK? 8 (< range, needs a spotter) | OK? |
| GHOST Navy SEAL | OK. Swims (amphibious code path) | OK? MP5 125/10 | OK. 6 / C4 1.5 | OK? | OK? | PARTIAL: C4'd a Sea Scorpion in one hit when it came adjacent, but cannot path onto water to reach a Destroyer (W6-04-07); vs a Rhino on land it uses MP5 at 1% (W6-04-32) | OK? MP5E | IFVMode 4 → CRMP5 | OK? 8 | MISSING: EnterWater/LeaveWater (W6-04-27) |
| CLEG Chrono Legionnaire | OK. Teleport move measured | OK. NeutronRifle erase: GI in 160 frames (spec 1250/8 = 157) | OK. 5 | OK? Temporal | N/A | OK. Teleport plus erase | OK? NeutronRifleE 16 | IFVMode 10 → CRNeutronRifle 5/6/120 (OK) | OK? 8 | MISSING: ChronoIn/OutSound (W6-04-27) |
| CCOMAND Chrono Commando | OK? Teleporter (bound via its IFVMode) | OK? ChronoMP5; FakeC4 as a normal 5000 hit | OK? | OK? | OK? | OK? Stolen-tech gate (R-042) | OK? | IFVMode 4 | OK? | MISSING: Create/ChronoIn/Out (W6-04-27) |
| MTNK Grizzly | OK? Speed 7, Crusher | OK? 105mm 65/60 | OK? 5 | OK? AP .3/.5 | OK? Cannon | N/A | WRONG: elite 105mmE Burst=2 fires once (W6-04-01) | N/A | OK? 8 | OK? |
| FV IFV | OK? Speed 10, Crusher=no | WRONG: HoverMissile Burst 2 (elite 4) fires once (W6-04-01) | WRONG: AirRangeBonus=4 missing (W6-04-16) | OK? HE .5/.5 | OK? | OK. Modes measured: empty 25/6/50, GI CRM60 25/6/15, GGI 30/8/40, Sniper 125/14/60, Tanya 125/7/10, CLEG 5/6/120, dog 25/6/50 | OK. Elite empty 88/ROF 30. Passenger modes ignore FIREPOWER (W6-04-28) | OK. Passengers=1 | OK? 8 | WRONG: IFVTransform per-type sound (W6-04-27) |
| SREF Prism Tank | OK? Speed 4, Crusher | WRONG: Comet hits the target only; no fragments (W6-04-02) | OK. 10 | OK? CometWH | WRONG: ShrapnelWeapon is on the projectile (W6-04-02) | N/A | WRONG: SuperCometFragment unused (W6-04-02) | N/A | OK? 8 | OK? |
| MGTK Mirage | OK? Speed 7 | OK? MirageGun 100/70 | OK? 7 | OK? | OK? | OK. Disguises when still, own and enemy measured. Enemy tree draw not visible in the shot (shroud): R-047 not re-measured. CanApproachTarget missing (W6-04-30) | ? MirageGunE (EliteSecondary quirk, W6-04-31) | N/A | OK? 9 | OK? |
| TNKD Tank Destroyer (Germany) | OK? Speed 5, Turret=no | OK. SABOT kills a Rhino; Conscript takes 3 per shot (150 × 2%) | OK. 5 | OK. UltraAP 2% inf / 100% heavy | OK? | N/A | WRONG: SABOTE Burst=2 (W6-04-01) | N/A | OK? 8 | OK? |
| BFRT Battle Fortress | WRONG: OmniCrusher missing (W6-04-14) | OK? 20mmRapid 30/20 | OK? 5.5 | OK? HARVWH | OK? | WRONG: passengers fire primary, not OpenTransportWeapon (W6-04-15) | OK? | OK. 5 passengers fire out (passenger_shots=3), +2 range, ×1.2 damage | OK? 6 | WRONG: EnterTransport per-type (W6-04-27) |
| ROBO Robot Tank | OK? Hover amphibious (code) | OK? Robogun | OK? | OK? | OK? | WRONG: PoweredUnit not modelled; it moves with no Control Center (W6-04-18). Not buildable on this install (GAROBO art absent) | OK? | N/A | OK? 6 | MISSING: Activate/Deactivate (W6-04-27) |
| CMIN Chrono Miner | OK? Drives (the teleporter profile is not bound for CMIN) | N/A | N/A | N/A | N/A | WRONG?: warps back to the field after unloading (W6-04-34) | N/A | N/A | OK? 4 | OK? ChronoMinerTeleport |
| AMCV | OK? | N/A | N/A | N/A | N/A | OK? Deploys to GACNST (gF) | N/A | N/A | OK? 6 | OK? PlaceBuilding |

## 2. Gaps

### W6-04-07: Tanya and SEALs never reach a ship to plant C4 (NEW; related to R-001 swim (fixed) and R-206)

**What the player sees.** Tanya is ordered from the water onto an enemy Destroyer 3.6 cells away. She shows the attack cursor and stays "moving" in place for about 450 frames. She never closes to C4 range, and the Destroyer shoots her down (200 → 45 HP). A SEAL standing on the shore never enters the water to reach a Destroyer.

**Evidence.**
- `A-water2.play` W2-samples: TANY stays at (176,164), DEST at (174,161), DEST HP 600 throughout, `sel_target=3`, `moving=1`.
- Cause: `engine/world.cpp:4626-4637`. Approach requests from non-naval units go to `assign_formation(..., passable_, ...)`, which is the land grid. The water cells around a ship are never firing cells. The naval branch at 4602 uses `grid_for(movement_domain)`, but amphibious units do not get it.
- A Sea Scorpion that sailed next to the SEAL was C4'd in one hit, so the weapon itself works.

**Original.** The attack approach uses the unit's own MovementZone (AmphibiousDestroyer). Sources: naval.md §1/§4; abilities.md §D for C4.

**Severity / effort.** MAJOR (SEAL missions ALL09/SOV09, any naval C4). S.

**Fix.** `World::step` MoveRequest dispatch. Route `MovementDomain::Amphibious` approaches like the naval branch (`nearest_free_cells` on `grid_for(domain)`), or pass `grid_for(u.movement_domain)` to `assign_formation`.

### W6-04-02: The Prism Tank beam never splits (NEW; regression of W2-SK-22)

**What the player sees.** A Prism Tank shot into 4 adjacent Conscripts hurts only the one it hits.

**Evidence.**
- `A-probe.play` P4: summed Conscript HP drops 625 → 525, exactly the 100 Comet damage, then 500 after the next shot. No CometFragment damage at all.
- Cause: `engine/main.cpp:854-855` reads `ShrapnelWeapon` / `ShrapnelCount` from the weapon section (`[Comet]`). In rulesmd they live on the projectile: `[LargeCometP] ShrapnelWeapon=CometFragment ShrapnelCount=5`.
- The elite `[SuperCometP] SuperCometFragment` (50 damage, range 5) is never resolved either, because only the primary is read.

**Original.** BulletType ShrapnelWeapon / ShrapnelCount (rulesmd; projectile.md).

**Severity / effort.** MAJOR (the Prism Tank's main trait). S.

**Fix.** `resolve_combat_unit` in `main.cpp`. Read the keys from `rules.weapon_projectile(primary)`, and swap in the elite set with the elite weapon in `refresh_vet_stats`.

### W6-04-01: `Burst=` is ignored by every weapon (NEW, cross-cutting)

**What the player sees.** Several weapons fire one shot per ROF cycle instead of a burst:
- IFV: HoverMissile Burst=2, elite Burst=4.
- Elite Grizzly: 105mmE Burst=2.
- Elite Tank Destroyer: SABOTE Burst=2.
- Guardian-GI IFV: CRMissileLauncher Burst=2.

For these units, DPS is ½ to ¼ of the original.

**Evidence.** `git grep -i burst engine` has no reader. wave4/04-projectiles.md §… lists "route Burst shots" only as a to-do.

**Original.** `TechnoClass::Fire` FUN_006FD210 advances the burst index (+0xEE) (combat.md §5). GetROF FUN_006FC460 uses a between-burst delay of Random(3,5) and the full ROF only after the last shot (veterancy.md §6.1).

**Severity / effort.** MAJOR. M.

**Fix.** `combat.cpp` `try_fire` / `rearm_secs`: a per-unit burst counter.

### W6-04-14: The Battle Fortress cannot crush vehicles (`OmniCrusher=yes`) (NEW)

**What the player sees.** A Battle Fortress drives around tanks instead of over them.

**Evidence.** `git grep -i omni engine` has only OmniFire. `c.crushable` is false for every vehicle (`weapon_resolve.cpp:117`). `OmniCrushResistant` (BFRT, AMCV, SMCV, DNOA, PCV, SMIN) is not read either.

**Original.** [RE-FALLBACK, ModEnc semantics] An OmniCrusher crushes any non-OmniCrushResistant ground unit. MovementZone=CrusherAll.

**Severity / effort.** MAJOR (a signature YR mechanic). M.

**Fix.** The `crusher` / `crushable` tests in `world.cpp` (crush path) and `weapon_resolve.cpp`.

### W6-04-15: Battle Fortress passengers fire their primary, not `OpenTransportWeapon` (NEW)

**What the player sees.** A GI in a Battle Fortress fires the M60: 15 damage (×1.2 = 18), range 4+2, ROF 20. The original has it fire Para (OpenTransportWeapon=1): 25 damage, range 5+2, ROF 15. That is about 2.2× the DPS.

**Evidence.**
- `A-probe2.play` Q5: Conscript HP steps of −12 (18 × 70% prone), never the −24 that Para would give.
- Code: `engine/world_w5.cpp:220-260` uses `p.w_damage / p.w_range / p.w_rof`.
- `OpenTransportWeapon` has no reader. TANY, GHOST and CCOMAND set it to 0, which is their primary anyway.

**Original.** [RE-FALLBACK, rules key semantics] abilities-w5.md §6, OpenTopped (PROBABLE).

**Severity / effort.** MAJOR. S.

**Fix.** The world_w5 OpenTopped pass: select the passenger's weapon slot `OpenTransportWeapon` (default −1 = normal selection).

### W6-04-38: A dog sent at a GI stalls out of bite range and dies (NEW, cause not pinned)

**What the player sees.** A dog is clicked onto an enemy GI about 10 cells away. It runs up, then stays "walking" at (69,37) while the GI is at (71,38), 2.24 cells away against a bite range of 1.5. It never leaps, and the GI shoots it down (85 → 10 HP).

**Evidence.** `A-dog.play` D2-samples: `sel_target=5`, `sel_moving=1`, position frozen. A dog that starts 3 cells from a Conscript bites and kills it at once.

**Original.** Approach until in range (infantry-anim.md §7). The dog's jump closes the last cell.

**Severity / effort.** MAJOR (the dog is the Allied anti-infantry and spy counter). S–M.

**Fix.** Debug the `assign_formation` firing-cell choice and the hold-at-range pass in `world.cpp:4631-4720`, together with the sub-cell occupancy (R-061). The assigned cell is likely occupied or unreachable and is never re-requested while `moving()` stays true. This needs one debug run.

### W6-04-16: IFV `AirRangeBonus=4` is not applied (NEW)

**What the player sees.** An IFV engages aircraft at 6 cells instead of 10.

**Evidence.** `git grep AirRangeBonus engine` returns 0 hits. [FV] has AirRangeBonus=4.

**Original.** [RE-FALLBACK] TechnoType AirRangeBonus is added to the weapon range against airborne targets.

**Severity / effort.** MAJOR (the IFV is the Allied mobile AA). S.

**Fix.** Read it in `resolve_combat_unit`, and add it in `unit_in_weapon_range` when the target is airborne.

### W6-04-18: Robot Tank `PoweredUnit` is not modelled; the Robot Control Center is unbuildable (NEW, plus an install note)

**What the player sees.**
- A Robot Tank with no Robot Control Center moves and fights. `A-probe2.play` Q4: ordered 6 cells, it moved (32 → 38).
- In real skirmish the ROBO and GAROBO cameos never appear (`A-land` ROSTER-DUMP). The GAROBO art is absent on this install (roster.md §5), so the Robot Tank is unbuildable.

**Evidence.** `git grep PoweredUnit/PowersUnit engine` returns 0 hits.

**Original.** [RE-FALLBACK] A PoweredUnit is deactivated (no move, no fire, DeactivateSound) unless its house has a powered building with `PowersUnit=ROBO`.

**Severity / effort.** MINOR here, because only map or cheat units are affected. S.

**Fix.** A world pass that disables units with a `powered_unit` flag. Related: U-1, R-193.

### W6-04-34: The Chrono Miner warps back to the ore field (NEW, RE needed)

**What the player sees.** After unloading, the CMIN teleports back to the field. In the original it drives out and only chronoshifts home when full.

**Evidence.** `engine/harvest.cpp:616-638`, "warp back after unload". economy.md §3 marks this itself as [RE-FALLBACK].

**Original.** [RE-FALLBACK, common player knowledge: drives to ore, teleports to the refinery]. Confirm in FUN_0073C560.

**Severity / effort.** MINOR (economy speed). S.

**Fix.** `harvest.cpp`: after unload, go to SeekOre/MoveToOre by driving.

### W6-04-29: `DeployedCrushable=no` (GGI) is not read (NEW)

**What the player sees.** A deployed Guardian GI can be run over.

**Evidence.** `git grep DeployedCrushable` returns 0 hits. `crushable` comes only from `Crushable` (`weapon_resolve.cpp:117`).

**Original.** [RE-FALLBACK, key semantics].

**Severity / effort.** MINOR. S.

**Fix.** The crush test in `world.cpp`: use DeployedCrushable while `deployed`.

### W6-04-27: Several unit sounds are never played (NEW; complements R-130)

**What the player sees.** These sounds are silent or generic:
- Chrono Legionnaire and Chrono Commando teleports are silent. `ChronoInSound`/`ChronoOutSound` are read only for CMIN as a global (`main.cpp:714`), and AbilityEvent::Teleport has no audio consumer (`pres_audio_events.inc:140` handles Deploy only).
- Tanya and SEAL `EnterWaterSound` / `LeaveWaterSound` are not read.
- `CreateSound` is not read: TanyaPrimeCreated, ChronoCommandoCreated.
- Robot Tank `ActivateSound` / `DeactivateSound` are not read.
- Per-type `EnterTransportSound` / `LeaveTransportSound` are not used: IFV IFVTransform, BFRT EnterTransport. The engine takes [HTK]'s or the global one (`main.cpp:719-724`).

**Evidence.** The greps above.

**Original.** TechnoType keys (pres_audio_events.inc RE header).

**Severity / effort.** MINOR. S.

**Fix.** `pres_audio_events.inc` `on_world_events` (Teleport events); a water-entry edge on `an.in_water`; create sound at `rp_launch_ready_units`.

### W6-04-28: An IFV with a passenger ignores veterancy (NEW)

**What the player sees.** A veteran or elite IFV carrying a passenger does base damage and ROF (no FIREPOWER/ROF), and passenger modes never use `EliteWeapon<k>`.

**Evidence.** `engine/world_abilities.cpp:296-303` (`ability_apply_weapon_fields` copies raw damage). `world_veterancy.cpp:109-112` re-applies the raw passenger weapon. Measured: an empty elite IFV is correct (88 / 30).

**Original.** SetCurrentWeapon selects the slot; GetROF/FirePower veterancy still applies (veterancy.md V7/V10).

**Severity / effort.** MINOR. S.

**Fix.** Route the IFV swap through `fire_damage` / `rof_frames`, and build `ifv_weapons` from `EliteWeapon<k>` when elite.

### W6-04-30: Mirage `CanApproachTarget=no` is not read (NEW)

**What the player sees.** A Mirage chases targets out of range instead of holding.

**Evidence.** `git grep CanApproachTarget` returns 0 hits.

**Severity / effort.** MINOR. S.

**Fix.** The approach branch at `combat.cpp:1109`.

### W6-04-31: Mirage elite weapon quirk and disguise delay (NEW, RE needed)

- [MGTK] sets `EliteSecondary=MirageGunE` with no Secondary. The engine never uses MirageGunE, because `w2` is invalid (`weapon_unit.cpp:31-36`). Whether the original's elite SelectWeapon picks it is unknown.
- The 15-frame "still" delay before the disguise (`world.cpp:2271`) is [RE-FALLBACK].

**Severity / effort.** MINOR. S, after RE of UpdateDisguise FUN_007456F0 and SelectWeapon.

### W6-04-32: SEAL vs a vehicle on land uses MP5 (1%), while Tanya plants C4 (STILL OPEN, R-191 SelectWeapon RE-FALLBACK)

**Evidence.** `A-probe2.play`:
- Q1: Tanya switches to Sapper (2500 / 1.5) and kills a Rhino. This matches YR, where Tanya can C4 vehicles.
- Q2: a SEAL shoots MP5 at the Rhino for 3 per hit (−22 per Rhino shell to the SEAL). `choose_weapon` prefers a primary with Verses > 0.

**Original.** Unverified. It needs `TechnoClass::SelectWeapon`.

**Severity / effort.** MINOR. S, after RE.

### W6-04-33: C4 on ships is an instant 2500 hit and the `ship_c4` probe is dead (NEW, low)

**Evidence.** `abil_w5.hpp:147` `ship_c4` is never incremented. Sapper against a unit takes the plain damage path, with no C4 plant event, timer, anim or "SealPlaceBomb" plant sequence. abilities.md:313 lists "C4 on ships" as cut.

**Severity / effort.** MINOR (the outcome is right: the ship dies in one charge). S.

## 3. Verified OK in play

**Infantry**
- GI deploy: weapon swaps to Para 25 / range 5 / ROF 15, and a deployed GI kills a Conscript 3.5× faster.
- Guardian GI deployed: MissileLauncher hits a Rhino for 40.
- Sniper: range 14, ROF 150; elite ROF 36 (60 × 0.6). One-shots a GI at 7 cells.
- Attack dog: a one-bite kill on a Conscript when it starts within about 3 cells. It refuses a Rhino.
- Chrono Legionnaire: erases a GI in 160 frames (spec 157). The teleport move works.
- Tanya swims onto water (`A-tanya-swim.jpg`). She C4s a Rhino on land, which matches YR.
- SEAL C4s a Sea Scorpion in one hit.
- Own Mirage disguises when still. An enemy Mirage reports as disguised.

**Vehicles**
- IFV passenger modes: empty, GI, Guardian GI, Sniper, Tanya, Chrono Legionnaire and dog all resolve the correct `Weapon<k>` (damage, range, ROF). An elite empty IFV gets HoverMissileE 88 / ROF 30.
- Tank Destroyer: kills a Rhino; does 3 damage per shot to a Conscript (UltraAP 2%).
- Battle Fortress: passengers fire out (`passenger_shots`).

**Roster**
- An Americans player with Barracks, War Factory, Airforce Command and Battle Lab gets ADOG, BFRT, CLEG, CMIN, E1, ENGINEER, FV, GGI, MGTK, MTNK, SPY, SREF, TANY.
- SNIPE (British only) and TNKD (Germans only) are correctly absent for Americans.
- ROBO is absent (W6-04-18).
