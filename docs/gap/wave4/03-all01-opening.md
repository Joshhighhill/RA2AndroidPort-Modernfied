# Wave 4 / 03: Allied mission 1 (ALL01 "Lone Guardian", New York), opening sequence

Read-only audit of `master` 4647b17, made on 2026-10-07. Nothing is committed and no engine source was changed.

| Item | Where |
|---|---|
| Worktree | `D:\ra2-wt\w4-all01`, branch `w4-all01`, Release build `build-rel` |
| Play scripts and outputs (local only) | `D:\ra2-wt\w4-all01\scratch\` |
| Ghidra project (private copy of `p4proj`) | `D:\ra2-re\w4-all01` |
| Ghidra scripts | `D:\ra2-re\scripts\w4a_1..4.py` |
| Ghidra outputs | `D:\ra2-re\w4-all01-out\` (`loco\f_00661d00.c` is RocketLocomotion Process) |
| Map census decoders (scratchpad) | `trig.py`, `teams.py` and `csf.py`, run over `all01t.map`, RA2 `rules.ini` and `ra2.csf` |

The sibling report `04-projectiles.md` pins the RocketLocomotion and homing-bullet flight models in depth. This report
uses those findings for ALL01 and adds the mission-specific causes.

## Fix status (wave-4 fix agent 3, branch `w4fix-proj`)

| ID | Status | Where / evidence |
|---|---|---|
| A1-01 opening shelling held | **FIXED** | `tag_guards_loss` skip removed (`campaign_runtime.inc`); `scripts/play/all01-opening.play` (real menu, 4 min, no input): DMISL launched (244), shot down by the Liberty Island Patriots (159), Statue alive (4742/5117), fleet intact, `RUNNING` |
| A1-02 DMISL explodes with V3WH | **FIXED** | `engine/rocket.cpp` per-type block + warhead (FUN_00662A70); `tests/test_rocket.cpp` pins DMISL vs CANEWY04 = 6, V3 = 100000 |
| A1-03 rocket flight model | **FIXED** | `rocket_step` ports FUN_00661D00 (pause, tilt, climb to the map's 1550, LazyCurve arc / level-off + dive, nose contact); hull drawn pitched |
| A1-04 Patriot missiles do not fly | **FIXED** | `ProjKind::Homing` (`integrate_homing`); RedEye2 flies with the DRAGON line trail and hits the rockets |
| A1-05 Statue DestroyAnim/rubble palette | **FIXED** | the ruin anim carries the building art `own_pal` (`spawn_hold`); `all01-v3-statue.play` shots `v3_destroy_anim` / `v3_rubble` |
| A1-06 "Break Bridges" | not this agent (mission-sequence agent) | – |
| A1-07 Lock/UnlockInput | not this agent (control agent) | – |
| A1-08 Elite damage / warhead | **FIXED** | `rocket_elite` from the launcher's rank at launch; EliteDamage + *EliteWarhead |
| A1-09 rocket kill shows no explosion | **FIXED** | a shot-down rocket plays its `Explosion=` anim at its height (Explodes=no: no warhead damage) |
| scripted V3 on the Statue | **kept working** | `scripts/play/all01-v3-statue.play` |

## 0. Headline: why no rockets fly

Three separate causes stack up. The first one alone explains the user report.

1. **The opening shelling is held.** Trigger "Mission Start" (t=0) creates the team "BG1 - Dreadnought 1" four
   times. The team recruits the four placed BadGuy2 Dreadnoughts and runs script "BG1 - Attack Statue of Liberty",
   which is `AttackWaypoint 0`. Waypoint 0 lies on the Statue, and the Statue's tag `0A1C2220` guards the loss
   ("Enable Failure 2" → "Mission Failed").
   - `campaign_runtime.inc:1525-1537` (`tag_guards_loss`) silently skips the order.
   - The Dreadnoughts carry `CanPassiveAquire=no`, so they never pick a target of their own.
   - Result: no DMISL is ever launched. `fx_v3_puffs=0` for all 3 minutes (run `out-menu`), and the screenshots show
     four idle ships.
2. **The hold is only needed because of a clone bug, not because of interception.** The clone explodes every rocket
   with the V3 warhead (`resolve_sw_params` → `p.missile_verses` from `V3Warhead`, used by `world_an.cpp:291`). The
   original picks the warhead per rocket type (CONFIRMED, §3.2).
   - ALL01 overrides `[V3WH] Verses` with **special_1 = 50000%**, so the scripted V3 one-shots the Statue.
   - ALL01 overrides `[DMISLWH] Verses` with **special_1 = 2%**, which keeps the Statue safe from the Dreadnoughts.
   - The Statue has `[CANEWY04] Armor=special_1 Strength=10000`.
   - So in the clone, one Dreadnought missile does 300×500 = 150000 damage and the mission is lost on the first hit
     (the P10 "fails in ~15 s"). In the original it does 300×0.02 = **6 damage**. About 1670 unintercepted missiles
     would be needed.
3. **What flies would still look wrong.** The flight model is a straight `cruise_step` at a hard-coded 768-lepton
   ceiling (`world_an.cpp:273-300`). It ignores the map's `[General] DMislAltitude=1550 / V3RocketAltitude=1550` and
   the Acceleration, TurnRate and LazyCurve keys. The Patriot's `RedEye2` (AAHeatSeeker, `ROT=80`) is still resolved
   instantly (P2 cut, `projectile.cpp:278`), so interceptions happen with no missile on screen.

Interception itself already works. In experiment B (§5.3), a Soviet-owned Patriot shot down a player V3 rocket in
flight: `units_lost(0)` went 0→1 and the target took `hp_lost=0`.

**Lifting the hold is therefore safe once cause 2 is fixed.** Cause 3 is about looks: it decides whether the user sees
rockets arc and get shot down.

## 1. Map census (`all01t.map`, RA2 `Maps01.mix`, URBAN, 50×64, `LocalSize=2,6,46,52`)

**Houses.** House-type indexes follow the RA2 `rules.ini [Countries]` list (13 entries), with the map's countries
appended after it.

| ht | House | Country / parent | Role | Allies |
|---|---|---|---|---|
| 13 | Player | Americans, TechLevel 3, credits 4000 | Tanya; later Fort Bradley | Civ, Other1-3 |
| 14 | BadGuy1 | Russians | Soviet supply base (89-106, 62-87), 3 V3s (Sleep), transports, paratroopers, cargo planes, V3 salvos | BG2, BG3, Civ, Other2 |
| 15 | BadGuy2 | Russians | **4 Dreadnoughts** at (61,91) (64,91) (58,90) (67,90), mission Attack | BG1, BG3, Civ, Other3 (one-sided) |
| 16 | BadGuy3 | Russians | Conscript paratroopers, barrels | as BG2 |
| 17 | Civilian1 | Neutral | city, cars, 3 MTNK (handed over by "Change House 4") | BG, Civ, Other1/2 |
| 18 | Other1 | Germans (Allied) | **Fort Bradley** (GACNST, GAREFN, 4 GAPOWR, 6 GAPILL, **3 NASAM**), **Statue of Liberty CANEWY04 (61,69), tag 0A1C2220** | Player, Civ, Other1-3 |
| 19 | Other2 | Germans | **2 NASAM on Liberty Island (59,83) (66,83)**, 3 GAPOWR | Player, Civ, Other1-3 |
| 20 | Other3 | Germans | Player GIs ("ButtNugget" AllChangeHouse) | Player, Civ, Other1-3 |

**Map rules overrides that matter here:**
- `[DMISL] Strength=75`. One RedEye2 hit (75 damage, SAMWH 100% vs special_2) kills a missile.
- `[DMISLWH] CellSpread=1.5 PercentAtMax=.25 Verses=…,28%,2%,0%` (special_1 = 2%).
- `[V3WH] Verses=…,50%,50000%,0%` (special_1 = 50000%).
- `[CANEWY04] Armor=special_1 Strength=10000`, placed at health 131/256 (5117 HP).
- `[General] DMislAltitude=1550 V3RocketAltitude=1550 StrayDistance=2 TeamDelays=1000,750,500`.
- `[NASAM] Name=Patriot Missile`; `[NAYARD] TechLevel=-1`; `[M1Carbine] Damage=10`.
- `[SpecialFlags] DestroyableBridges=no`. `[Basic] Intro=A01_f00e`; the movie MIX is a stub on this install.

**Scale.** 130 triggers, 128 tags, 49 TeamTypes, 0 malformed lines. The clone's own count agrees:
`[camp] … triggers=130 tags=128 teams=49 placed_b=190 placed_u=59 skipped=0`.

**Objectives** (`ra2.csf`):
- `MISSION:ALL01A` "Objective 1: Destroy all four Soviet DREADNOUGHTS".
- `ALL01B` "Objective 2: Rendezvous with FORT BRADLEY".
- `ALL01H` "Objective 3: Destroy the Soviet SUPPLY BASE".
- The briefing adds "Keep Tanya alive".
- `ALL01X` ("The STATUE OF LIBERTY is about to be destroyed - Hurry and destroy the Dreadnoughts!") is never referenced
  by a trigger.

**The rockets in the mission:**

| Launcher | Owner | Script / trigger | Target | Rocket | Warhead vs target |
|---|---|---|---|---|---|
| 4 × DRED (DredLauncher, Spawner, 2 DMISL, regen 80 frames, Range 25, MinRange 8) | BadGuy2 | "BG1 - Dreadnought 1" `AttackWaypoint 0`, from t=0 | **Statue** (wp0 = (62,71)) | DMISL (75 HP, Speed 20, ROT 4) | DMISLWH: Statue 2% → **6 per hit** |
| V3 "V3 - Liberty" (1, at wp82 (77,68)) | BadGuy1 | "V3 - Attacks Liberty", 7 s after the player enters cells (61-64, 84) | Statue (`AttackEnemyBuilding 131151` = type 79 CANEWY04) | V3ROCKET | V3WH 50000% → **Statue destroyed in one hit (scripted)** |
| V3 "V3 - Salvo" ×3, "V3 Salvo 2" ×2 | BadGuy1 | "Enable Salvos" (20 s after Objective 3) at 0/6/12/18/24 s | wp96 (46,90), wp52 (53,67), looping (`JumpToLine 1`) | V3ROCKET | V3WH |
| 3 placed V3 at (82-86,72) | BadGuy1 | Sleep | – | – | – |
| Patriots NASAM ×5 (RedEye2: 75 damage, ROF 55, Range 12, AAHeatSeeker ROT 80) | Other1 ×3, Other2 ×2 | base defence | any enemy rocket in range | – | SAMWH special_2 100% |

## 2. The original opening, as a timeline (from the triggers; t = scenario seconds, Frame/15)

| t | Trigger | What the original shows / does |
|---|---|---|
| 0 | Lock Input | Input locked; EVA "Establish battlefield control" |
| 0 | Mission Start 2 | Reveal around wp1 (the Dreadnoughts), 27, 49, 20; radar ping at the fleet |
| 0 | **Mission Start** | 4 Dreadnoughts start shelling the Statue (`AttackWaypoint 0`) |
| 0 | **Break Bridges** | 10 × Apply100Damage at wp80 (62,59) and wp79 (82,52). Each one is `FUN_006DFA00` = 5 × `DamageArea(100, C4Warhead)` with no anim of its own. The high bridges at both waypoints break (`Random(1,BridgeStrength) < 100` per hit; effects.md §5.2) |
| 0 | Ambient Sounds 1/2 | Harbour, ocean, birds, city murmur at 26 waypoints |
| 1 | Create Crates | 4 crates (wp8/5/21/7) |
| 3 | Mission Start 3 | `NukeSiren` |
| 5 | Initial Audio | EVA `Mis_A1_EvaProtectStatue` |
| ≈5.3 → | (DMISL flight) | Each DMISL waits on deck for `DMislPauseFrames=20`, tilts for `TiltFrames=60` (0°→45°), leaves the deck, then climbs at +0.8 lept/frame² to **1550** leptons. `LazyCurve=no`: it levels off by `TurnRate` 0.08 per frame, then dives at the Statue. The two Liberty-Island Patriots (Other2, 7 cells from the fleet) fire `RedEye2` (DRAGON missile, ROT 80); one hit kills a 75-HP DMISL. Survivors deal 6 damage to the Statue with a 1.5-cell DMISLWH splash. This repeats for the whole mission (regen 80 frames per missile) until Objective 1 is done or the fleet retreats |
| 14 | "Unlock Input" | `PlayIngameMovie 69`; arms Unlock Input 2 |
| 30 | Start Scroll | Camera pans to waypoint 18 (speed 1); arms "Start Transports" and "Enable Objective 1" |
| 30 | Enable Cargo 1 | A Soviet CARGOPLANE flies across (wp83 → wp84); planes 2-4 follow at 55/80/100 s and repeat every 125 s |
| 43 | Enable Objective 1 | Text "Objective 1: Destroy all four Soviet DREADNOUGHTS", EVA "New objective received", the fleet flashes; re-shown every 120 s |
| 44 | Start Transports | `NavalUnitEmerge`; 2 amphibious SAPC + 5 Conscripts each, at wp74 (50,96) and wp9 (76,96). They land at wp75/77, unload, set locals 0/1, attack. "Transport Retreat 1/2" then sails the empty APCs away |
| 50 | Unlock Input 2 | Tanya's team (move to wp36, guard) |
| 55 | Unlock Input 3 | "--Battle Control Online--", EVA `BattlefieldControlOnline`, **UnlockInput**, reveal wp50 |
| 56 | Unlock Input 4 | `TanyaAttackCommand` voice; Tanya flashes |
| player | Initial Paratroopers 1 (player enters (61-64,84)) | 3 × 7 Conscript paradrops (BadGuy3). 7 s later "Enable - Enable V3 - Liberty" **disables the Statue's loss trigger** and sends the V3 |
| +7…+16 | V3 - Attacks Liberty | V3ROCKET (lazy-curve arc to 1550) → **Statue destroyed** (V3WH 50000%), DestroyAnim `CANY04DM` (needs `liburb.pal`). "Play Statue Movie" (BuildingDoesNotExist 79), then Soviet movie 67, Harriers ×3 against the V3, "Enable Retreat"; 120 s later "Dreadnought Retreat" (the fleet sails to wp85 and is deleted, unless Tanya C4'd it first) |
| later | Objective 2 / 3 | Fort Bradley handed to the player ("Change House 2b"), Chrono Miner, then Objective 3 and the V3 salvos |

## 3. Original mechanics pinned in `gamemd.exe` (YR 1.001; RA2 `rules.ini` values are identical for these keys)

### 3.1 RocketLocomotion Process: `FUN_00661D00` (CONFIRMED; vtable slot `0x7EFB5C`; RocketLocomotion CLSID GUID at `0x7E8AD0`)

- **Block selection.** The `RocketStruct` comes from the owner's type: `== Rules+0x4E0 (V3RocketType)` → `Rules+0x4B0`;
  `== +0x548 (CMislType)` → `+0x518`; anything else → DMisl at `+0x4E4`.
- **Block layout.** Every field is string-anchored in `FUN_0066CF70` (`0x670C47…0x670F9B`): `+0 PauseFrames`,
  `+4 TiltFrames`, `+8 PitchInitial`, `+C PitchFinal`, `+10 TurnRate`, `+14 RaiseRate` (ftol), `+18 Acceleration`,
  `+1C Altitude`, `+20 Damage`, `+24 EliteDamage`, `+28 BodyLength`, `+2C LazyCurve`, `+30 Type`.
- **States** (`this+0x3C`):
  1. Pause timer = PauseFrames, with smoke anims every 0x18 frames.
  2. Tilt: pitch = lerp(PitchInitial, PitchFinal)·π/2 (`DAT_007E1820`) over TiltFrames; at the end it detaches from
     the launcher and spawns a launch anim.
  3. Climb: speed += Acceleration, capped at the type Speed (`+0x678`), until height ≥ Altitude; it then stores the
     initial horizontal distance.
  4. Cruise: `LazyCurve=no` decays pitch by TurnRate; `LazyCurve=yes` blends atan(dz/horiz) and PitchFinal by the
     remaining-distance fraction.
  5. Dive: turns toward atan(dz/horiz), limited to TurnRate per frame.
  6. CMisl only.
- **Detonation** (`FUN_00661B30`): pitch-projected distance ≤ the target height, or a bridge layer, or height < 1.
  `Health < 1` also detonates (the shot-down case, `0x662840`).
- **Explosion** (`FUN_00662A70`). **The warhead is per rocket type:** V3 `Rules+0xFB0` (elite `+0xFB8`), CMisl
  `+0xFC0`/`+0xFC4`, DMisl `+0xFB4` (elite `+0xFBC`). These are the `[CombatDamage] V3Warhead / DMislWarhead` reads
  in `FUN_0066B5F0` at `0x66BDDC/0x66BE1B`. Damage then goes through `FUN_00489190` (DamageArea) and the rocket is
  deleted.

### 3.2 Can Patriots shoot rockets down? Yes

- V3ROCKET and DMISL are `[AircraftTypes]` (RA2 indexes 4 and 6), `Armor=special_2`, `Strength=50` (75 for DMISL in
  ALL01).
- The `MissileSpawn` flag (TechnoType `+0xD68`, store at `0x7142D7`) has 8 readers. None of them is a
  targeting/threat filter: `FUN_0054E040` and `FUN_006B7230`/`6B7400` are spawn and transport bookkeeping,
  `FUN_004176F0` is the aircraft mission, `FUN_0041430D` is draw height and `FUN_007021D0` is the fire-coord owner.
- There is no `Interceptor` key string in `gamemd.exe`, so vanilla RA2/YR has no dedicated interceptor logic.
- **Interception is plain AA fire at an airborne AircraftClass.** NASAM `RedEye2` → `AAHeatSeeker` (`AA=yes`,
  `ROT=80`, `Arm=2`, `Ranged`), and SAMWH does 100% vs special_2. CONFIRMED by data; PROBABLE as code (a negative
  search). `04-projectiles.md` §1.3 reaches the same conclusion.

### 3.3 Homing bullet (the Patriot's missile)

The `BulletClass::Update` `ROT ≥ 1` branch: turn = ROT×(1..1.5) with a 15-frame `MissileROTVar` sine, speed ramps by
`Acceleration`, and it arrives at `dist ≤ 0.5·|V|`. It was pinned in `04-projectiles.md` §1.1 (it was PROBABLE in
`projectile.md` §6.4). The clone still maps `ROT>0` to Instant (`projectile.cpp:278`).

## 4. The clone's timeline, side by side (run `out-menu`: real menu input, no cheats)

| t (s) | Original | Clone (`camp_log` witness; frame/15) | Match |
|---|---|---|---|
| 0 | Input locked + EVA | "Lock Input" fires; `LockInput … (recorded)`. **Input is not locked**: the player can act during the 55-second intro | ✗ |
| 0 | Fleet shells the Statue | "Mission Start" → 4 × `CreateTeam … recruited=1` → script `AttackWaypoint 0` **held** (no log line). No missile all run: `fx_v3_puffs=0`, the 4 DRED never fire (`m045`/`m090`, `t000`-`t180`) | ✗ **(the user report)** |
| 0 | Bridges at wp79/80 collapse | "Break Bridges": **10 × `a63 … -> skipped`**. The clone damages only an object standing on the waypoint cell, and there is no high-bridge destruction (W2-GA-07). The bridge north of Liberty Island stays intact (`expA/a04.png`) | ✗ |
| 0 | Reveals, radar ping | reveals OK; radar event recorded | ~ |
| 0 | Ambient sounds | recorded (plays when audio is on) | ✓/~ |
| 1 | Crates | 4 × CreateCrate ok | ✓ |
| 3 | Nuke siren | f=45, recorded sound | ✓ |
| 5 | EVA "Protect the Statue" | f=75 | ✓ |
| 5→ | DMISL arcs, Patriot intercepts | **nothing** | ✗ |
| 14 | In-game movie 69 | f=210, recorded (movie MIX is a stub) | ~ |
| 30 | Camera pan | f=450; the view moves to the fleet (`m045.png`) | ✓ |
| 30 | Cargo plane 1 | f=450 placed=1 | ✓ |
| 43 | Objective 1 text + EVA + flash | f=645; text shown on screen (`m045.png`) | ✓ |
| 44 | 2 amphibious APCs land | f=660 placed=6+6 at wp74/wp9 (land at the far goal); "Transport Retreat 1" f=881 | ✓ |
| 50 | Tanya | f=750 recruited=1 | ✓ |
| 55 | Battle Control Online, UnlockInput | f=825 (text shown) | ✓ |
| 56 | Tanya voice + flash | f=840 | ✓ |
| 160/125 | Objective re-text / cargo planes 1-4 | f=2445 / 2325, 2700, 3075, 3375 | ✓ |

The mission is still `RUNNING` at 3 minutes with no input. Trigger timing is frame-exact for every trigger the
clone executes.

The clone's game speed runs the campaign at about 1.34 scenario frames per driver frame (301 per 225). Shot names in
`out-menu` are in driver seconds.

## 5. Missiles and rockets: what should fly, what flies, and why not

### 5.1 Should fly in the first 3 minutes (original)

- DMISL from the 4 Dreadnoughts, continuously.
- Patriot DRAGON missiles from the 2 Liberty-Island NASAMs, intercepting them.
- Puffs and explosions on the Statue and its plaza.
- Later, player-triggered: the V3 Statue strike, Harrier Mavericks and the V3 salvos.

### 5.2 Flies in the clone

- **Nothing in the first 3 minutes.** The DMISL launch is held (§0.1).
- **The scripted V3 strike does fly.** Experiment A (`w4-expA.play`: cheat `camp_fire "Enable - Enable V3 - Liberty"`)
  shows it working:
  - the V3 is placed (`units(1,V3)` 3→4) and a V3ROCKET is airborne at once (`airborne(1)=1`);
  - the trail draws (`fx_v3_puffs` 0→38);
  - the rocket hits after about 6.4 s and the Statue dies in one hit (`buildings(5,CANEWY04)` 1→0);
  - the chain follows: "Play Statue Movie" f=144, "Play Soviet Movie 2", "Create Harriers b", "Harriers Talk";
  - outcome stays `RUNNING`, which is correct because the loss trigger was disabled first.
- **But the strike looks wrong:**
  - The rocket flies a flat, low line (`expA/a04.png`) instead of a 1550-lepton lazy arc.
  - **The Statue's DestroyAnim `CANY04DM` and its rubble draw as magenta noise** (`expA/a08.png`, `a20.png`). This is
    the same `Palette=lib` (`liburb.pal`) gap that wave 3 fixed for the Statue body, but the destroy anim did not get
    it. **New finding.**

### 5.3 Interception (experiment B, `w4-expB.play`)

Set-up: a Soviet-owned NASAM is built next to the player's lamp post (`w2 build NASAM 1`), and a player V3 attacks
it (`camp_attackb 1 NASAM V3`).

Result: the rocket launches (`airborne(0)=1`, puffs 0→28) and dies in flight at about 4 s (`units_lost(0)` 0→1,
`airborne` 0). The Patriot takes no damage (`hp_lost(1)=0`). AA vs rockets therefore already works in the World.

What is missing:
- The Patriot shot is instant and nothing visible flies (`expB/b04.png`: only the rocket is visible).
- No explosion is drawn at the kill point.

### 5.4 Why not, in order of effect

1. **Held order** (`campaign_runtime.inc:1525-1537`). This is cause 1 in §0.
2. **Wrong warhead for DMISL** (`w2air_main.inc:119-123` reads `V3Warhead` only; `world_an.cpp:291` applies it to
   every missile). Elite warheads and EliteDamage are also ignored.
3. **Rocket flight model is a stand-in** (`world_an.cpp:273-300`):
   - hard-coded altitude 768 and climb rate;
   - no tilt pitch, Acceleration, TurnRate or LazyCurve;
   - it detonates only on 2-D arrival;
   - it does not read `[General]`, so it misses the map's 1550 altitude;
   - pause and tilt are hard-coded 80/60 frames (equal to the stock values, but not read from rules).
4. **Homing projectiles are instant** (`projectile.cpp:278`). The Patriot, Harrier Maverick (`AirToGroundMissile`)
   and IFV missiles never fly. See `04-projectiles.md`.
5. **Apply100Damage is not DamageArea**, and high bridges cannot be destroyed. The opening explosions and collapse
   are missing.

## 6. Gap table

| ID | Gap | Original | Clone (evidence) | Severity |
|---|---|---|---|---|
| A1-01 | Dreadnought opening shelling held | team `AttackWaypoint 0` on the Statue from t=0 | `tag_guards_loss` skips it; no DMISL ever (`out-menu`, `fx_v3_puffs=0`) | **MAJOR** (user report) |
| A1-02 | DMISL explodes with V3WH | per-type warhead `Rules+0xFB0/0xFB4/0xFB8/0xFBC` (`FUN_00662A70`) | one global `missile_verses` from V3Warhead; in ALL01, 150000 damage per DMISL on the Statue instead of 6 | **BLOCKER for lifting A1-01** |
| A1-03 | Rocket flight model | 6-state RocketLocomotion, Rules blocks, map `[General]` overrides (§3.1) | flat cruise at 768, hard-coded rates (`expA/a04.png`) | MAJOR (visual + timing + intercept window) |
| A1-04 | Patriot (homing ROT>0) missiles do not fly | `BulletClass` homing branch | Instant; nothing drawn for base defences (`expB/b04.png`) | MAJOR (visual) |
| A1-05 | Statue DestroyAnim/rubble palette | `CANY04DM` with `liburb.pal` | magenta noise (`expA/a08.png`, `a20.png`) | MAJOR (visual) |
| A1-06 | "Break Bridges" does nothing | 10 × (5 × DamageArea C4) → high bridges at wp79/80 collapse | `a63 … skipped` ×10; bridges intact | MAJOR (scripted set-piece; the "repair the bridge" hint chain then plays over an intact bridge) |
| A1-07 | LockInput / UnlockInput | input locked t=0-55 s | recorded only; the player can act during the intro | MINOR |
| A1-08 | DMISL Elite damage / warhead, `EliteDamage` | read per block | ignored | MINOR |
| A1-09 | Rocket kill shows no explosion | the rocket dies through the normal AircraftClass death (Explosion= anims) | it vanishes | MINOR |
| A1-10 | In-game movies 67/68/69 | `PlayIngameMovie` | recorded; `MOVIES01.MIX` is a 4-byte stub on this install | n/a (install) |
| A1-11 | `[SpecialFlags] DestroyableBridges=no` vs the scripted bridge break | Not traced. It probably gates only player and AI targeting of bridges, not trigger DamageArea (the triggers clearly expect the break: "Bridge Destroyed" hint, CABHUT repair) | – | RE item |

## 7. Prioritized fix list

1. **Per-rocket warhead (A1-02 + A1-08).**
   - `SwParams` gets `{v3, dmisl}` × `{normal, elite}` warhead verses, spread and PercentAtMax, read from
     `[CombatDamage] V3Warhead / DMislWarhead` (+ the elite keys) through the campaign rules overlay.
   - `AnType` records which block the rocket uses: owner type == `V3RocketType`, else DMisl.
   - Unit test: the ALL01 overlay gives DMISL vs CANEWY04 = 300×2% = 6 and V3 vs CANEWY04 = 100000.
   - Size: S. It changes no other mission's numbers; the stock DMISLWH and V3WH differ only in Verses and spread.
2. **Lift the opening hold (A1-01), gated.**
   - Delete the `tag_guards_loss` skip, or keep it behind a debug flag for one release.
   - New gate `w4-all01-shelling.play`: real menu, no input, 4 minutes.
   - Assert: DMISL launched (`fx_v3_puffs > 0`, `spawned_out(2)` seen > 0); at least one DMISL shot down
     (`units_lost(2)` grows by DMISL); `buildings(5,CANEWY04) == 1` and its HP still > 4000; outcome `RUNNING`.
   - Safety math with no interception at all: 4 ships × 1 missile per 80 frames ≈ 0.75 missiles/s × 6 HP ≈ 4.5 HP/s.
     The placed Statue (5117 HP) survives about 19 minutes; with the 2 Patriots (≤0.54 kills/s) it survives far
     longer.
   - The original's own timers (the Retreat 120 s after the Statue strike, and Objective 1) end the shelling first.
   - Also check: the DMISLWH 1.5-cell splash around wp0 hits only the Statue plaza. The Patriots are 12 cells away.
3. **RocketLocomotion (A1-03).**
   - Port `FUN_00661D00` states 1-5 with the Rules blocks (pause, tilt, climb with Acceleration up to Altitude, cruise
     with LazyCurve/TurnRate, dive, and the `FUN_00661B30` detonation tests). Read `[General]` through the overlay so
     ALL01 gets 1550.
   - This sets how long a missile spends inside Patriot range, so do it before tuning or asserting interception
     counts.
   - Size: M. `04-projectiles.md` §1.2 has the full spec.
4. **Homing ROT>0 bullets (A1-04).**
   - Implement the `BulletClass` homing branch for unit and building weapons (`04-projectiles.md` §1.1).
   - The Patriot DRAGON missile, the Harrier Maverick and the IFV missiles then fly visibly, and interception becomes
     a visible mid-air event.
   - Kill rule: an AA bullet whose target rocket is already dead follows `LoseTarget` (air target → climb to
     MissileSafetyAltitude and self-detonate).
   - Size: M-L.
5. **CANY04DM palette (A1-05).** Apply the art `Palette=` key (lib → `liburb.pal`) to building DestroyAnim and rubble
   frames, the same way as the body. Size: S.
6. **Apply100Damage → 5 × DamageArea(100, C4Warhead), plus high-bridge destruction (A1-06).**
   - Needs W2-GA-07 (the `FUN_005766D0`/`FUN_00570FC0` high-bridge break and the `FUN_0056FB80` hut repair).
   - Until then, keep the action a no-op, but log it as `held` instead of `skipped`, so the witness says why.
   - Size: L.
7. **Rocket death explosion (A1-09) and LockInput (A1-07).** Size: S each.

Steps 1 and 2 alone restore the user-visible opening: rockets leave the Dreadnoughts, Patriots kill them and the
Statue stands. Steps 3-4 make it look like the original.

## 8. Evidence index (local, `D:\ra2-wt\w4-all01\scratch\`)

- **`w4-menu.play` → `out-menu\`.** Real input: Single Player → New Campaign → Allied → Load → Start. Shots
  `m000..m180` are taken every 15 driver seconds; `out-menu.txt` holds the full `camp_log` witness.
  - `m000.png`: start view.
  - `m045.png`: camera on the fleet, Objective 1 text, the APC landed, the fleet idle.
  - `m090.png`: Tanya on the pier, the fleet still idle.
- **`w4-cheat.play` → `out-cheat\`.** `set campaign ALL01T.MAP GDI` + reveal, view on Liberty Island every 10 s
  (`t000..t180`). The Statue stands, both Patriots are idle, the ships are idle and no rocket appears in any frame.
- **`w4-expA.play` → `out-expA\`.** Scripted V3 strike: `a04` shows the rocket low and flat, `a08` the magenta
  destroy anim, `a20` the magenta rubble.
- **`w4-expB.play` → `out-expB\`.** A Soviet Patriot intercepts a player V3 rocket: `b04` shows the rocket in flight
  with no Patriot missile visible.
- **Ghidra outputs in `D:\ra2-re\w4-all01-out\`:**
  - `a1_gamemd_exe.log`: key strings and the CLSID;
  - `a2.log`: the warhead reads;
  - `loco\f_00661d00.c`, `f_00662a70.c`, `f_00661b30.c`: Process, Explode and the detonation test;
  - `ms\`: the MissileSpawn readers.
