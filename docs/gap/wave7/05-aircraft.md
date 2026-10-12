# Wave 7 / 05: Aircraft and air units — gap analysis

**Tree:** master `b3a509f`, `build/ra2engine.exe` (built at `86a928f`; later commits are scripts only).
**Scope:** every `[AircraftTypes]` entry (ORCA, BEAG, HORNET, ASW, PDPLANE, CARGOPLANE, BPLN, SPYP, V3ROCKET, DMISL, CMISL; APACHE has no section) and the Jumpjet air units (ZEP Kirov, SHAD Night Hawk, SCHP Siege Chopper, JUMPJET Rocketeer). Yuri-side air (DISK, BSUB/CMISL launcher) is out of scope, because the clone has no Yuri side.
**Method:** I read `rulesmd.ini` (`D:\ra2-re\out\shroud_evidence\rulesmd.ini`), `docs/re/aircraft.md`, `roster2.md` §C and the earlier gap reports (wave2/02, wave6/04 and wave6/05, REMAINING.md), then the engine code (`world_an.cpp`, `w2air_main.inc`, `flight.cpp`, `world.cpp` step_flight, `combat.cpp`, `pres_audio_events.inc` and `pres_layers.inc`). I also made 7 `--play` runs, whose scripts are in `05-evidence/`.
**Ghidra:** decompiles of retail gamemd.exe were made from scratch project copies, which have since been deleted. The raw dumps are kept in `D:\ra2-re\w7-air-out`, `w7-air-a-out` and `w7-air-b-out`. The airfield-loss, Carrier-cadence, pad-repair and crash-damage items are resolved with addresses (§3). The questions still open are listed in §5.

**Time base.** The default lobby speed runs 4 logic frames per `--play` driver frame (wave6/04). Every timing below is in logic frames.

## 1. Summary

**Totals.** 15 rows: 2 WITHDRAWN as faithful after RE (W7-05-01, W7-05-06), 5 MAJOR, 8 MINOR or COSMETIC. W7-05-09 resolves W6-04-37 with RE. Several earlier items are still open and are not re-filed here: W6-04-17 (SCHP deploy), W6-04-19 (Spy Plane), W6-04-36 (flier speed bias) and R-110 (altitude-to-pixel factor).

**Top gaps:**
1. **W7-05-02.** The fixed-wing flight model is wrong. Planes reverse on the spot instead of looping (ROT 3), climb and land 5x too fast (the original climbs 20 leptons/frame, the clone 100), and fly no repeat passes [CONFIRMED-RE FlyLoco `FUN_004CD520`, Mission_Attack `FUN_00417FE0`].
2. **W7-05-01 / W7-05-06 withdrawn.** A dropped attack order on an empty plane, and a return to base after a move order, both match the original [CONFIRMED-RE].
3. **W7-05-03.** The Harrier and the Rocketeer never play their flight loop or their take-off and landing sounds. The audio looks up the rules section by *image* (`FALC` / `ROCK`), and neither section exists.
4. **W7-05-05.** The Carrier launches only one Hornet per HornetLauncher ROF (150), so at most one Hornet is in the air. The original launches every ready spawn, one every 20 frames (missiles one every 9) [CONFIRMED-RE SpawnManager AI, switch @0x6B691B].
5. **W7-05-09.** Crash damage is wrong. The original fires FireDeathWeapon `FUN_0070CAC0` (DeathWeapon, else the Primary weapon) on impact. That makes a Kirov crash a 250-damage BlimpHE 2-cell blast; the clone deals Strength/4 = 500 over one cell [CONFIRMED-RE @0x4CD729 planes, @0x745001 Kirov].
6. **W7-05-15.** The jumpjet locomotor was calibrated against the Hover locomotor (aircraft.md is wrong). The original moves JumpjetSpeed leptons per frame raw, with no bias and with JumpJetTurnRate turns, so the Kirov should cover about 0.29 cells/s; the clone does 1.125 [CONFIRMED-RE `FUN_0054CDA0`].
7. **W7-05-04** (MINOR). The skirmish crash on airfield loss is faithful [CONFIRMED-RE `FUN_004176F0` → vtbl+0x3DC = `FUN_004DEAC0`]. The campaign's fly-off-the-map path is the deviation.

## 2. Per-unit table

| Unit | Rules keys read / ignored | Flight / behaviour | Combat / AA | Verdict |
|---|---|---|---|---|
| ORCA Harrier | Read: Ammo, Strength, Speed, ROT (x2, floored at 1 rev/s), Dock, CanPassiveAquire, CanRetaliate. Ignored: PipScale=Ammo (no ammo pips), Fighter, PreventAttackMove (the clone has no attack-move), FlyBy, per-type Reload | Built onto a pad, sorties, Ammo 1, returns to base, rearms in 270 frames [CONFIRMED-PLAY via BEAG]. Instant reversal (W7-05-02). A move order flies to the point and returns: faithful (W7-05-06 withdrawn). Empty attack order dropped: faithful (W7-05-01 withdrawn) | Maverick homing [CONFIRMED-PLAY]. A docked plane is a ground target: GIs took 200→186 [CONFIRMED-PLAY] | WRONG: 02/03/04/07 |
| BEAG Black Eagle | as ORCA (image BEAG, so its sounds work) | as ORCA. BlackEagleMoveLoop plays [CONFIRMED-PLAY] | Maverick2 killed an E1 in one pass [CONFIRMED-PLAY] | WRONG: 02/04/07 |
| HORNET | Spawned, Ammo 1, MovementRestrictedTo | one airborne at a time (W7-05-05). Crashes when its Carrier dies [CONFIRMED-PLAY t3]; faithful (KamikazeTracker `FUN_0054E040` → crash for non-MissileSpawn) | HornetBomb: 32 per hit vs GAPOWR [CONFIRMED-PLAY] | PARTIAL: 05 |
| ASW Osprey | as HORNET; NavalTargeting=2 | not re-tested (W6 verified the ASW vs sub) | — | OK? |
| PDPLANE | Ammo 100, Speed 15; drop radius 1024 | Enters from the nearest x edge [RE-FALLBACK R-038]. One trooper per frame. Shootable (Strength 400) | — | OK? (R-038 open) |
| CARGOPLANE | not referenced by the engine | — | — | N/A (unused in skirmish) |
| BPLN (Boris MiG) | AirstrikeTeam path (world_w5) | Spawns at the map edge, kOutbound, exits | Maverick3 Burst 2 [W6-04-01 fixed]. DeathWeapon=BlimpBomb x0.1 ignored for the crash (W7-05-09) | MINOR |
| SPYP | — | **MISSING** (W6-04-19, user decision) | — | OPEN |
| V3ROCKET / DMISL | RocketLocomotion blocks (W4) | pause, tilt, climb, dive; interceptable | V3WH / DMISL warheads | OK (W6 verified) |
| ZEP Kirov | Read: JumpjetSpeed, Climb, Crash, Height, ROT, BalloonHover, Crashable, SelfHealing, CanPassiveAquire. Ignored: JumpJetAccel (W7-05-10), CellRangefinding=yes (W7-05-11) | Speed bias W6-04-36 (still open). Passive acquire killed a Rhino 1 cell away [CONFIRMED-PLAY t2] | BlimpBomb 2-cell splash OK. Crash damage is Strength/4; the original is the BlimpBomb death blast (W7-05-09) | WRONG: 09/10/11 |
| SHAD Night Hawk | Landable, Passengers 5, CanPassiveAquire=no. Ignored: RadarInvisible=yes, HoverAttack | Lands at its move goal [CONFIRMED-PLAY t2c: alt 420→0] | BlackHawkCannon | OK? (W6 boarding note) |
| SCHP Siege Chopper | No Landable key, so it hovers forever [CONFIRMED-PLAY t2c alt 500 for 130 frames]. IsSimpleDeployer / DeployToLand ignored | Deploy missing (W6-04-17 open). Not in the AI roster although aimd has `0=2,SCHP` (W7-05-12) | 160mm never used | WRONG (open + 12) |
| JUMPJET Rocketeer | BalloonHover, Crashable, JumpjetNoWobbles=yes, but the wobble draw reads `[ROCK]` (W7-05-08) | Hovers. Ignores fire while on a move order [code: combat.cpp:262]. Conscripts cannot hit it (InvisibleLow has no AA) [CONFIRMED-DATA/PLAY] | 20mm | MINOR: 03/08 |

## 3. Gaps

### W7-05-01: WITHDRAWN (faithful). An attack order on an empty docked plane is dropped
- **Evidence:**
  - [CONFIRMED-PLAY] t5: the order is lost, and the plane rearms and stays docked.
  - [CONFIRMED-RE] Mission_Attack `FUN_00417FE0` does the same:
    - Status 0 goes to 1, and status 1 with Ammo==0 goes to status 10.
    - Status 10 calls Assign_Target(0) for a human-owned plane (or a loaner), then Enter_Idle_Mode `FUN_004176F0`.
    - For a landed plane, that clears the target and sets Guard.
  - The original drops the order too. **No fix.**

### W7-05-02: Fixed-wing flight model: planes reverse on the spot, take off and land too fast, and fly no repeat passes
- **Severity / effort:** MAJOR (visual and behaviour) / M
- **Evidence:**
  - [CONFIRMED-PLAY] t1b: the BEAG flew 74,22 → 74,38, fired, and five driver frames later was at 74,34. That is a 180° reversal with no loop.
  - [CONFIRMED-code] `world_an.cpp:598-600` moves the plane straight at its goal regardless of facing, with the turn floored at `max(flight_turn, 65536)` u16/s.
  - Take-off and landing use `kFlyClimbLps` = 100 leptons/frame (`world_an.cpp:23`, RE-FALLBACK).
- **Original** [CONFIRMED-RE, FlyLocomotionClass ILocomotion vtable 0x7E79F4; Process 4CCA60; movement step `FUN_004CD520`]:
  - **Climb** (@0x4CDD84-0x4CDDBD): at most 20 leptons per frame, or 10 when the plane carries passengers. That is 75+ frames to reach FlightLevel 1500, against the clone's 15.
  - **Descent toward the cruise height:** clamp(d/20, 20, 50) per frame.
  - **Landing:** at most 16 leptons per frame. Take-off is d/20 + 10, capped at 48.
  - **Mission_Attack `FUN_00417FE0` geometry:**
    - Status 3: within 0x200 leptons the plane turns to face the target; within 0xF it fires (status 4, Burst shots).
    - **Strafing weapons** (Is_Strafe `41B820`: projectile ROT ≤ 1) fly three more passes (statuses 6-9).
    - **Fighter=yes** (AircraftType +0xE0E, Is_Fighter `41B870`) re-attacks while it has ammo.
    - **Any other plane** turns and fires again (status 5) until Ammo is 0.
  - The horizontal speed formula and the ROT turn were not traced (`FUN_004CD520`, Apparent_Speed 4CFD40).
- **Fix:**
  - Move the plane along `body_facing` at ROT×2 with no floor, so it overflies and loops back.
  - Use the climb, landing and take-off rates above.
  - Implement the status 3/4/5 pass loop, including Fighter re-attack and Strafe passes. This matters for Ammo>1 planes such as PDPLANE and SPYP (Ammo 100).

### W7-05-03: Harrier and Rocketeer flight sounds never play (lookup by image, not by rules id)
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] t4 (America ORCA + JUMPJET): `pres.log` has IntruderSelect, IntruderAttackCommand and IntruderAttack, but no IntruderMoveLoop, IntruderTakeOff or IntruderLanding, and no RocketeerMoveLoop. The BEAG run (t1b) logs BlackEagleMoveLoop.
  - [CONFIRMED-DATA] `[ORCA] Image=FALC` and `[JUMPJET] Image=ROCK`. rulesmd has no `[FALC]` or `[ROCK]` section.
- **Code:** `pres_audio_events.inc:525-534` (w6_flight), `:354` (ImpactLandSound), `:571` (CreateSound) all call `w5_bld_key(u.image, …)`.
- **Fix:** Key these lookups by the unit's rules id. The voices map already resolves ids.

### W7-05-04: Airfield lost while airborne: the skirmish crash is faithful, the campaign fly-off is not
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] t1 phase A: in skirmish, the plane crashes 2 frames after its airfield dies (`crashing(0)=1`).
  - [CONFIRMED-RE] AircraftClass Enter_Idle_Mode `FUN_004176F0`:
    - It searches for a dock through vtbl+0x528 = `FUN_0041BC00`. For an AirportBound plane that re-uses its last dock; otherwise it calls `FUN_004DEF50`.
    - When the search finds no dock and `AirportBound` (+0xE0D) is set, it calls vtbl+0x3DC(0) = `FUN_004DEAC0` and returns.
    - `FUN_004DEAC0` sets the crashing flag (+0x425=1), queues mission 3 and draws a random pitch and roll (R250 `FUN_0065C220`). That is the crash [call CONFIRMED; the name "Crash" is PROBABLE].
    - The check runs whenever the plane enters idle, both with ammo and empty (the @0x417AD4 block).
- **Original vs clone:**
  - The skirmish crash (`world_an.cpp:553-559`) is faithful, except for timing: the original crashes when the plane goes idle and finds no free dock, not on the frame its home dies.
  - The original also crashes a plane whose airfield is merely **full**; the clone instead "claims a free pad" on another airfield and otherwise crashes.
  - The campaign's `order_leave_map` (`world_an.cpp:744-780`, "RE-FALLBACK: leave over the nearest map edge") is **not** what this path does for an ordinary AirportBound plane. A *loaner* (reinforcement, techno +0x3D4, set only by the reinforcement code at 0x65D320-0x65E4F0) is the exception: it takes Mission Retreat (`FUN_00415A50`) and is deleted off-map by `FUN_004CD430`, so the leave-map path is faithful for campaign reinforcements only [CONFIRMED-RE `FUN_004176F0`].
- **Fix:**
  - Drop the leave-map fallback for AirportBound planes. Keep it only for non-AirportBound team transports, which take Mission Retreat.
  - Trigger the crash on "idle and no dock found".

### W7-05-05: The Carrier keeps only one Hornet in the air (the original launches the whole wing)
- **Severity / effort:** MAJOR (Carrier DPS) / M
- **Evidence:**
  - [CONFIRMED-PLAY] t3: `spawned_out(0)` never went above 1 in 160 frames. One sortie lands every ~150 logic frames, which is the HornetLauncher ROF.
  - [CONFIRMED-RE] SpawnManagerClass:
    - The ctor `FUN_006B6310` builds SpawnsNumber nodes; its vtable is `0x7F2650`.
    - The per-frame AI is a state switch @0x6B691B, in the function that contains 0x6B6CB7. Dump: `D:\ra2-re\w7-air-out\sp1.txt`.
    - For a node in state 0 (docked and ready), while the manager holds a target (+0x68) and its launch timer (+0x5C start, +0x64 length) has run out, the AI launches that spawn.
    - It then re-arms the timer with **0x14 = 20 frames, or 9 frames when the spawn type is MissileSpawn (TechnoType +0xD68)**, sets the node to state 2 and gives the spawn the target with mission 2.
    - So every ready node goes out in turn. The weapon ROF only arms the manager's target, through `FUN_006B7210` from TechnoClass::Fire `FUN_006FD210`.
  - On the owner's death (`FUN_006B6780`), docked nodes are deleted. Airborne spawns go to KamikazeTracker `FUN_0054E040`, which crashes any non-MissileSpawn type (vtbl+0x3DC). So the clone's "Hornets die with the Carrier" is faithful [CONFIRMED-PLAY t3].
- **Code:** `world_an.cpp:291-334` (`count = missile ? spawn_ready : 1`, one launch per weapon shot).
- **Fix:**
  - While the spawner has a target, launch every ready spawn, one every 20 frames.
  - Missiles (DMISL, V3) go one every 9 frames, a staggered volley instead of all in one frame.

### W7-05-06: WITHDRAWN (faithful). After a move order a fixed-wing plane returns to its airfield
- **Evidence:**
  - [CONFIRMED-PLAY] t1b: the plane reached 74,32, then docked again.
  - [CONFIRMED-RE] AircraftClass Mission_Move `FUN_004166C0` (vtable 0x7E12A4 +0x22C) calls Move_To. Status 3 waits for the locomotor to stop, then calls `Enter_Idle_Mode(0,1)`, and an airborne plane goes back to its dock (`FUN_004176F0`). It does not hover or land at the cell.
  - The clone's RE-FALLBACK comment at `world_an.cpp:499` can be upgraded to RE. **No fix.**

### W7-05-07: No ammo pips, and the plane may not be repaired on its pad
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-code] `PipScale` has 0 readers; `main.cpp:4112` draws only passenger pips.
  - [CONFIRMED-PLAY t1] a docked BEAG at 86/200 HP stayed at 86 for 900 frames.
- **Original, pad repair: UNCONFIRMED.** The two sources conflict:
  - [CONFIRMED-RE] For it: the airfield's UnitReload loop in `FUN_0044B780` (@0x44C84x) sends radio 0x1D to each docked plane. The plane is released to Guard only when Ammo is full **and Strength == the type's Strength**. Otherwise the loop sends 0x13 (RELOAD) and then 0x1F/0x1C, which are probably repair messages. That points to repair on the pad [PROBABLE].
  - [CONFIRMED-DATA] Against it: `[GAAIRC]`/`[AMRADR]` have `;UnitRepair=yes` commented out, with the note "(Repair code is oogly, only Reload got rewritten for four ports.)".
  - The repair rate and cost were not pinned.
- **Fix:**
  - Draw an ammo pip row for `PipScale=Ammo`.
  - Before adding pad repair, trace the 0x1F/0x1C handlers (Foot `FUN_004D8ED0`).

### W7-05-08: The Rocketeer wobbles although it has JumpjetNoWobbles=yes
- **Severity / effort:** COSMETIC / S
- **Evidence:** [CONFIRMED-code] `pres_layers.inc:105-126` reads `JumpjetWobbles`, `JumpjetDeviation` and `JumpjetNoWobbles` from `[u.image]`, which is `ROCK`. That section does not exist, so the defaults apply (0.15/s, 40 leptons ≈ 6 px bob).
- **Fix:** Look the keys up by rules id.

### W7-05-09: Crash damage should be the FireDeathWeapon blast (DeathWeapon, else Primary), not Strength/4
- **Severity / effort:** MAJOR / S (resolves W6-04-37)
- **Evidence:**
  - [CONFIRMED-code] `world_an.cpp:648-657` deals `max(10, Strength/4)` at 100% verses over one cell. W6 measured a Kirov crash taking an own War Factory from 1000 to 541.
  - **[CONFIRMED-RE] Fixed-wing (FlyLocomotion crash in `FUN_004CD520`), when height < 1:**
    - a plane carrying passengers (+0x6C) applies a DamageArea with Rules+0xFA8;
    - any other plane calls **FireDeathWeapon `FUN_0070CAC0` @0x4CD729**, plays ImpactWater/ImpactLandSound (type +0x53C/+0x540, else Rules +0x200/+0x204) and is destroyed (vtbl+0xF8).
  - **[CONFIRMED-RE] Jumpjets (unit message handler `FUN_00744F30`, message 0x117C with Crashable +0xD95):**
    - a **BalloonHover (+0xD6A)** unit such as the Kirov calls FireDeathWeapon(0) @0x745001, then is destroyed;
    - a non-BalloonHover unit (Night Hawk, Siege Chopper) calls `FUN_007376C0`, its ordinary explosion, instead.
  - **[CONFIRMED-RE] FireDeathWeapon `FUN_0070CAC0`:**
    - It picks the type's DeathWeapon (+0xD18), else the techno's weapon (vtbl+0x3F4, the primary), else Rules+0xFDC.
    - Damage is `ftol(Damage x DeathWeaponDamageModifier)` (W6-04-12, @0x70CB1B).
    - It is fired as a bullet with the weapon's warhead (+0xAC) through `FUN_0046AF70`, then detonated.
  - [CONFIRMED-DATA] The rulesmd `[SPYP]` comment agrees: "needs a death weapon or it will do one laser blast's worth of crash damage".
- **Original vs clone:**
  - **Kirov:** BlimpBomb, 250 x BlimpHE verses, CellSpread 2, PercentAtMax .5. The clone deals 500 flat over one cell.
  - **Harrier:** Maverick, 150 x ORCAAP. The clone deals 37.
  - **BPLN / SPYP:** BlimpBomb x0.1 = 25.
  - **Hornet:** HornetBomb, 40.
- **Fix:**
  - On impact, fire the death weapon (DeathWeapon, else Primary, x modifier) through DamageArea with its warhead.
  - Crashable non-BalloonHover jumpjets take the normal unit explosion.

### W7-05-10: JumpJetAccel is ignored (instant cruise speed). See W7-05-15
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-code] `world.cpp:1925-1965`: `flight_speed` is applied from the first frame. `[ZEP] JumpJetAccel=10`, `[SHAD] 12`.
- **Original:** aircraft.md §2 names it the horizontal acceleration [PROBABLE].
- **Fix:** Ramp the speed by JumpJetAccel per frame.

### W7-05-11: Kirov `BlimpBomb CellRangefinding=yes` is not applied
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-code] the only reader is the Sapper C4 test (`combat.cpp:943`); [CONFIRMED-DATA] `[BlimpBomb]`.
- **Fix:** Compute range cell-centre to cell-centre for weapons with this flag.

### W7-05-12: The Siege Chopper is missing from the AI roster
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-DATA] aimd TaskForce `0=2,SCHP`; [CONFIRMED-code] `ai_skirmish.inc:36-54` has no SCHP.
- **Fix:** Add `{"SCHP", true, "NAWEAP"}` once W6-04-17 (the deploy) lands.

### W7-05-13: Night Hawk `RadarInvisible=yes` and `HoverAttack` are unread
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-code] 0 readers; [CONFIRMED-DATA] `[SHAD]`.
- **Fix:** Hide it on the enemy radar. HoverAttack: consumer not traced (§5).

### W7-05-14: Fixed-wing speed shares the ground GameSpeedBias (extends W6-04-36)
- **Severity / effort:** MINOR / S
- **Evidence:** [CONFIRMED-code] `w2air_main.inc:74` uses `speed_to_cps`, which carries the 1.6 bias. [CONFIRMED-PLAY] t1b: about 64 leptons per logic frame, against 35 from `Speed=14·256/100`.
- **Original:** Not traced (§5; W6-04-36).
- **Fix:** Use the RE'd air speed (likely no bias).

### W7-05-15: The jumpjet locomotor is calibrated to the wrong class. Speed, turn rate and acceleration are all wrong (Kirov about 4x too fast)
- **Severity / effort:** MAJOR / M. This supersedes W6-04-36 for jumpjets and absorbs W7-05-10.
- **Correction to `docs/re/aircraft.md`** [CONFIRMED-RE, from RTTI]:
  - Vtable 0x7E9CFC (Process `FUN_00514130`, Link `FUN_00513AD0`, wobble `FUN_00513B40`) is **HoverLocomotionClass**, not Jumpjet.
  - The real **JumpjetLocomotionClass** has ILocomotion vtable 0x7EBD68 and IPiggyback vtable 0x7EBD44.
  - Its functions:
    - Link `FUN_0054AA60`, Process `FUN_0054ABF0`, Move_To `FUN_0054AEF0`;
    - integrator `FUN_0054CDA0`;
    - state handlers: Grounded `0054B6B0`, Ascending `0054B760`, Hovering `0054BA60`, Cruising `0054BD20`, Descending `0054C280`, Crashing `0054C7C0`.
- **Original:**
  - **Turn rate:** JumpJetTurnRate (`FUN_004C9100(TurnRate)` in Link), not ROT×2.
  - **Speed:** JumpjetSpeed is copied raw, with no 256/100 conversion and no GameSpeed bias. The step is `ftol(CurSpeed)` leptons per frame along the facing (@0x54D1FB).
  - **Acceleration:** CurSpeed rises by JumpJetAccel per frame up to JumpjetSpeed and falls by 1.5×Accel.
  - **Approach slow-down while Cruising:** Speed/8 within one Speed of the goal, Speed/4 within 2×Speed, Speed/2 within 50×Speed/TurnRate.
  - **Resulting speeds:**

    | Unit | Leptons/frame | Cells/s at 15 fps | Clone (W6 measured) |
    |---|---|---|---|
    | Kirov | 5 | ~0.29 | 1.125 |
    | Night Hawk, Siege Chopper, Rocketeer | 30 | ~1.76 | — |

  - **Crash (state 5):**
    - Z drops by JumpjetCrash per frame while the unit spins (desired facing = current + 0x2D00) and keeps drifting along its facing.
    - The landing cell crushes infantry. Other occupants take full-health damage with Rules+0xFAC.
    - On reaching the ground it sends message 0x117C (see W7-05-09).
- **Clone:**
  - `main.cpp:977-1012` uses `speed_to_cps(JumpjetSpeed)` (×256/100×1.6) and `flight_turn = ROT×2`.
  - `world.cpp:1925-1965` has no acceleration and no approach slow-down.
- **Evidence:**
  - [CONFIRMED-RE] the dumps in `D:\ra2-re\w7-air-b-out` (r1.txt RTTI, d2/d3, dis7).
  - [CONFIRMED-PLAY] W6: Kirov at 1.125 cells/s.
- **Fix:**
  - `flight_speed` = JumpjetSpeed leptons/frame × 15 / 256 cells/s.
  - `flight_turn` from JumpJetTurnRate.
  - Add the acceleration ramp and the approach slow-down to `step_flight`.
  - Add the spin and drift while crashing.
  - Re-derive aircraft.md §1/§5.

## 4. Verified OK

- **BEAG sortie** [CONFIRMED-PLAY t1b/t5]: it builds onto a pad, takes off, fires one Maverick at ~6-7 cells, kills an E1 (125 HP), returns and lands. It rearms in about 270 logic frames ([General] ReloadRate=.3).
- **Docked planes as targets** [CONFIRMED-PLAY t1]: a docked plane is a ground target for GIs. CanRetaliate=no is honoured, and the plane did not launch at them.
- **Hornets** [CONFIRMED-PLAY t3]: Hornet bombs damage buildings, and an airborne Hornet dies with its Carrier. W6-04-26 (docked spawns not counted as lost) holds: `units_lost` rose only for the carrier and the airborne Hornet.
- **Kirov** [CONFIRMED-PLAY t2]: it passively acquires and kills an adjacent Rhino.
- **Rocketeer** [CONFIRMED-PLAY t2]: on a move order it flew over 3 conscripts without stopping, and conscripts cannot hit it. The move-order no-retaliation rule is in code: `combat.cpp:262,714,1142`.
- **Night Hawk** [CONFIRMED-PLAY t2c]: it lands at its goal.
- **AA legality** [CONFIRMED-code `combat.cpp:61-75`, `weapon_unit.hpp:12`]: only AA projectiles hit an airborne target (altitude > 0), and landed aircraft count as ground targets.
- **Already fixed, from earlier waves** (not re-tested):
  - BlackEagleMoveLoop and the Kirov sounds;
  - Patriot 3-D range;
  - the V3 and Dreadnought rocket locomotor and interception;
  - Night Hawk land-to-load;
  - the Kirov crash fall;
  - the AI fielding ORCA, BEAG, ZEP and SHAD (R-080).

## 5. Open RE questions

These were not resolved in this audit. Raw dumps: `D:\ra2-re\w7-air-out`, `w7-air-a-out`, `w7-air-b-out`.

- **Fixed-wing behaviour:** the FlyLocomotion horizontal speed (W7-05-14) and the ROT turn (W7-05-02).
- **Fighter:** resolved (Is_Fighter `41B870`, see W7-05-02).
- **PreventAttackMove:** TechnoType +0x6C8, read only by `FUN_00711230` (CanAttackMove); the clone has no attack-move, so N/A.
- **HoverAttack** (+0x390): FootClass Mission_Attack `FUN_004D4CE0` and Guard `FUN_004D4F90` lift a grounded unit to fight while hovering. The clone does not model this (W7-05-13).
- **Pad repair:** W7-05-07.
- **AA/AG airborne threshold:** not traced.
- **Idle landing** of jumpjets that are not BalloonHover, and the **SCHP deploy** (W6-04-17).
- **Spy Plane:** answered [PROBABLE-RE]. SW case 8 in `FUN_006CBA10` spawns the plane at the map edge (`FUN_0065E4F0`). Approach `FUN_004155F0` reveals a radius of SpyCameraWeapon Damage = 6 cells once the target is within Range 20, then Overfly `FUN_004157C0` exits the map. Feed this into W6-04-19.
- **SCHP deploy:** offsets only (IsSimpleDeployer UnitType+0xE13, DeployToLand +0x6AD); the deploy path is in `FUN_0073A3A0`/`0073C560`/`0073EB90`, not analysed.
