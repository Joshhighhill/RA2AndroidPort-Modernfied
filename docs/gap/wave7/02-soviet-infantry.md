# Wave 7 / 02: Soviet infantry behaviour gap analysis

- **Tree:** master `b3a509f`. Engine: `D:\ra2-android\build\ra2engine.exe` (built Oct 10 20:03; the commits after the build change scripts only).
- **Spec:** YR `rulesmd.ini` / `artmd.ini` / `aimd.ini`. The extracted copies used are `D:\ra2-re\out\shroud_evidence\rulesmd.ini`, `D:\ra2-re\out\roster2\artmd.ini` and `D:\ra2-re\w5-abil-out\aimd.ini`.
- **Disassembly:** `gamemd.exe` 1.000 in Ghidra 11.2.1, private copy `D:\ra2-re\w7-sinf`. Scripts are in `D:\ra2-re\w7-sinf-scripts`; raw output is in `D:\ra2-re\w7-sinf-out`.
- **Play evidence:** `docs/gap/wave7/02-evidence/` holds 4 `--play` scripts (`sinf1..4.play`) with their PASS/FAIL lines and the presentation sound logs (`*-pres.txt`).

**Scope:** every infantry that a Soviet house can build in YR `[InfantryTypes]`:
- E2 Conscript (image CONS);
- FLAKT Flak Trooper;
- SHK Tesla Trooper;
- IVAN Crazy Ivan;
- CIVAN Chrono Ivan (stolen tech);
- DESO Desolator (Iraq);
- TERROR Terrorist (Cuba; image TRST);
- BORIS;
- SENGINEER Soviet Engineer (image ENGINEER);
- DOG Attack Dog.

The following were checked and left out:
- **Out of reach for a Soviet house:** YURI needs YABRCK. YDOG has `RequiredHouses=YuriCountry`. LUNR has TechLevel 11, so it is never buildable.
- **Not trainable:** VLADIMIR, PENTGEN and SSRV are campaign or map objects with TechLevel -1.
- **Never built by the original AI:** BORIS appears in no `aimd.ini` TaskForce, so the original AI does not build him either.

**Prior work this report builds on:**
- `docs/gap/wave6/04-units.md` (part B), which measured Soviet weapon fidelity. Most of its items are FIXED and are not repeated here.
- `docs/re/abilities.md` §A–§G, `abilities-w5.md` §3/§8/§10 and `infantry-anim.md` §8–§13.
- `docs/gap/wave7/01-allied-infantry.md`. Shared root causes are cross-referenced below rather than duplicated.

## Summary

The Soviet infantry fight correctly at the weapon level. Bombs, suicide blasts, radiation, parasites, Burst, elite bounce and the airstrike call all work and match the RE.

What is missing is mostly the layer above the weapons: the player's commands, the AI's use of special units, and feedback.

**Major (4):**
1. **W7-02-01:** the Crazy Ivan still cannot bomb own or allied objects, and Ctrl force-fire refuses own units and buildings for every unit. W6-04-23 is STILL OPEN [CONFIRMED-PLAY, CONFIRMED-RE 0x6FF865].
2. **W7-02-02:** you cannot order a Tesla Trooper onto your own Tesla Coil. The click selects the coil and drops the troopers. The only way to charge a coil is to walk the trooper next to it and leave it idle [CONFIRMED-PLAY; weapon choice CONFIRMED-RE 0x6F2B7F].
3. **W7-02-03:** a deployed Desolator silently ignores move orders [CONFIRMED-PLAY].
4. **W7-02-04:** the AI never auto-deploys or undeploys its Desolators. `AIAutoDeployFrameDelay` has no reader [CONFIRMED-RE 0x521050].

**Minor (6):**
- W7-02-05: the Desolator eruption is silent and does not animate.
- W7-02-06: Boris fires his AKM at the designated building instead of holding the Flare, and his airstrike voice and cursor are missing.
- W7-02-07: Select Combatants sweeps up Engineers and Terrorists. This has the same root cause as W7-01-02.
- W7-02-08: dogs idle in Guard instead of Area Guard.
- W7-02-09: computer-owned infantry never scatter when hit.
- W7-02-10: a garrison cannot be given targets, never fires at buildings and draws no occupant muzzle flash.

**Cosmetic (1):**
- W7-02-11: an Ivan over an already-bombed target shows the attack cursor where the original shows the no-bomb cursor.

**Cross-slice (1):**
- W7-02-12: the Secret Lab is not implemented. TERROR and DESO are on its `SecretInfantry` list.

## Per-unit table

| Unit | Works (measured here or in wave 6, re-checked in code) | Gaps |
|---|---|---|
| **E2 Conscript** | <ul><li>M1Carbine 15/25/4 and elite M1CarbineE.</li><li>Prone / Crawl / FireProne (`Crawls=yes`) with ProneDamage 70% (SA).</li><li>Garrison entry with the Enter cursor, `OccupyWeapon` UCM1Carbine, the Occupy multipliers and range.</li><li>Paradrop (SovParaDropInf).</li><li>Voices.</li></ul> | <ul><li>W7-02-10: no player targeting from a garrison, no garrison fire at buildings, no UCCONS muzzle flash.</li><li>W7-02-09.</li><li>R-053 (Assaulter clearing) still open.</li></ul> |
| **FLAKT Flak Trooper** | <ul><li>AG FlakGuyGun / AA FlakGuyAAGun choice.</li><li>Elite Burst=2 (W6-04-01 fixed).</li><li>`Crawls=no`: never goes prone, so it takes no ProneDamage reduction.</li><li>Voices.</li></ul> | W7-02-09 |
| **SHK Tesla Trooper** | <ul><li>ElectricBolt 50.</li><li>Elite Electricbounce shrapnel (W6-04-02 fixed).</li><li>`Crushable=no`.</li><li>An idle trooper adjacent to an own Overpowerable coil charges it (R-045).</li><li>Select voice.</li></ul> | **W7-02-02**: cannot be ordered onto a coil. |
| **IVAN Crazy Ivan** | <ul><li>Bomb on an enemy unit or building, at plant + 451 frames.</li><li>`IvanDamage` 450 / IvanWH DamageArea per foundation cell (W6-04-03 fixed).</li><li>Chain detonation on target death.</li><li>The Ivan's own death does nothing (§E).</li><li>Bomb icon, `CrazyIvanAttack` attach sound and `CrazyIvanBombTick` loop [CONFIRMED-PLAY, sinf2 pres f=220].</li><li>Engineer defuse.</li></ul> | <ul><li>**W7-02-01**: no bombing of friendlies, no Ctrl force-fire on own objects.</li><li>W7-02-11.</li></ul> |
| **CIVAN Chrono Ivan** | <ul><li>Stolen-tech gate (R-042).</li><li>Teleport move.</li><li>Bombs as IVAN.</li></ul> | W7-02-01, W7-02-11 (same code paths as IVAN) |
| **DESO Desolator** | <ul><li>RadBeam 125 / elite 200.</li><li>Deploy cursor (ACTION_SELF) and D key.</li><li>Eruption every 60 frames builds a RadSite with spread 10 (§A) [CONFIRMED-PLAY: `rad_total` 0 → 57787].</li><li>`ImmuneToRadiation`, `Fearless` (no fear / prone), `SelfHealing`, beam drawn (W6-04-24).</li><li>Select and move voices.</li></ul> | <ul><li>**W7-02-03**: a deployed Desolator ignores move orders.</li><li>**W7-02-04**: the AI never auto-deploys.</li><li>W7-02-05: eruption silent, no DeployedFire.</li><li>W7-02-12.</li></ul> |
| **TERROR Terrorist** | <ul><li>Suicide TerrorBomb 225 / 2-cell blast via DamageArea.</li><li>`Explodes` death blast.</li><li>`CanPassiveAquire=no` and `CanRetaliate=no` are honoured (`combat.cpp:262,1142`).</li><li>`Crawls=no`.</li><li>Voices are keyed by id, so image TRST does not break them.</li></ul> | <ul><li>W7-02-07: picked up by Select Combatants.</li><li>W7-02-09: as a computer unit it cannot retaliate, so it should scatter when hit.</li><li>W7-02-12.</li></ul> |
| **BORIS** | <ul><li>AKM 65 / elite 90.</li><li>Airstrike: 2 BPLN, `AirstrikeRechargeTime` 100 [CONFIRMED-PLAY sinf4: `airstrikes=1` within 30 frames].</li><li>Flare line drawn at the call (W6-04-24).</li><li>Maverick3 Burst (W6-04-01).</li><li>`BuildLimit=1`, `CreateSound`.</li><li>`Crushable=no`.</li></ul> | **W7-02-06**: AKM fires at the building; `VoiceSecondaryWeaponAttack`, `MigAttackCursor` and the SecondaryFire sequence are missing. |
| **SENGINEER** | <ul><li>Capture, repair and defuse cursors.</li><li>Consumed on capture.</li><li>EngSov* voices (not the Allied engineer's) [CONFIRMED-PLAY sinf3: `EngSovSelect`, `EngSovMove`].</li></ul> | <ul><li>W7-02-07: picked up by Select Combatants.</li><li>W7-02-09: as a computer unit it never scatters when hit.</li></ul> |
| **DOG** | <ul><li>BadTeeth one-bite kill of infantry; refuses vehicles (Verses 0).</li><li>DetectDisguise.</li><li>Parasite release on death.</li></ul> | <ul><li>W7-02-08: `DefaultToGuardArea` is not read.</li><li>R-054 (the leap is not drawn) still open.</li></ul> |

## Gap list

### W7-02-01: The Crazy Ivan cannot bomb friendlies, and Ctrl force-fire refuses own objects (major; W6-04-23 STILL OPEN)

**Evidence:**
- **[CONFIRMED-PLAY]** `02-evidence/sinf1.play`:
  - T1: with the Ivan selected, the pointer over an own Rhino gives `cursor=1` (Select), not 6 (Attack).
  - T1b: `click unit 0:HTNK ctrl` leaves `bombs=0` 60 frames later.
- **[CONFIRMED-RE 0x6FF380]** TechnoClass::What_Action(object):
  - `AttackCursorOnFriendlies` is stored at TechnoType +0x6C1 (loader @0x714603).
  - It is read at **0x6FF865**: `type+0x6C0 || (type+0x6C1 && !...)` enables the attack action on friendly objects.
  - The force-fire keys (`DAT_00a8db60/64`, combined into a local flag) also return action 5 regardless of the target's alliance.

| | Behaviour |
|---|---|
| **Original** | <ul><li>An Ivan (or Chrono Ivan) right-click on an own or allied unit or building shows the bomb cursor and plants a bomb, the classic "Ivan-bomb your own APC" trick.</li><li>Ctrl force-fire works on own units and buildings for every armed unit.</li></ul> |
| **Clone** | <ul><li>`input_original.cpp:153-156`: the Ctrl branch only accepts `!controllable(house)` units and non-own buildings.</li><li>`:208`: an own unit resolves to Select.</li><li>`AttackCursorOnFriendlies` has 0 readers.</li></ul> |

**Fix:**
- In `Input::classify`, return Attack when Ctrl is held on any object outside the selection, own or not.
- Return Attack for an own or allied object when a selected type has `AttackCursorOnFriendlies=yes`. Parse it into the unit profile.
- `make_mega(Mission::Attack, own target)` must then pass the command layer's friend check for these two cases.
- `ability_fire_event` already plants on any target.

### W7-02-02: A Tesla Trooper cannot be ordered to charge an own Tesla Coil (major)

**Evidence:**
- **[CONFIRMED-PLAY]** `sinf2.play` T2, with SHK selected and the pointer over an own TESLA:
  - `cursor=3` (SelectBuilding);
  - after the click, `selected_type(SHK)=0` and `selected_building=63`. The troopers are deselected.
- **[CONFIRMED-RE 0x6F2890 @0x6F2B7F]** TechnoClass::SelectWeapon returns the Secondary when all of these hold:
  - the target is a building;
  - it is allied (`FUN_004f9910`);
  - the secondary warhead has `ElectricAssault` (+0x158);
  - the BuildingType has `Overpowerable` (+0x1575).

  This means an order onto an own coil is a valid AssaultBolt attack.
- The idle-charge path the clone already models is FootClass::Mission_Guard 0x4D4F90 (reads +0x158).
- The exact cursor glyph shown in the original is [SUSPECTED] (attack).

| | Behaviour |
|---|---|
| **Original** | The player selects Tesla Troopers and clicks the coil. They walk adjacent and fire AssaultBolt (`TeslaTroopRechargeCoil`) to overpower or power it during low power. |
| **Clone** | <ul><li>Charging happens only when an idle trooper happens to stand next to the coil (`world_w5.cpp:115-158`).</li><li>Clicking the coil selects it (`input_original.cpp:235`).</li></ul> |

**Fix:**
- In `classify`/`issue_order`: when the selection holds a unit whose Secondary warhead is `ElectricAssault` and the pointer is on an own or allied Overpowerable building, return Attack and emit `Mission::Attack(building)`.
- In the world, walk to an adjacent cell. On arrival, set `u.w5.charging = b` and keep the `target_building`. Step (3) must accept an attacking trooper whose target is that coil; it currently drops any unit with a target.

### W7-02-03: A deployed Desolator ignores move orders (major)

**Evidence:**
- **[CONFIRMED-PLAY]** `sinf2.play` T3:
  - after `key d` the radiation appears (`rad_total` 0 → 57787);
  - a ground click 6 cells east leaves `ucell` 69020 unchanged after 120 frames;
  - the move voice played (`DesolatorMove`) and nothing else happened.
- Code: `world.cpp:1205-1211`. `order_unit_move` returns on `u.frozen`. Only a `DeployFire` GI is un-deployed by a move order (G-45).

| | Behaviour |
|---|---|
| **Original** | **[SUSPECTED]**, consistent with the GI path. A move order undeploys the Desolator (Undeploy sequence) and walks him. The deployed mission is left through the undeploy mission (0x521050 queues mission 0x1F from the deployed state for computer houses). |
| **Clone** | The order is silently dropped. The player must press D again, and nothing tells him so. |

**Fix:** in `order_unit_move` (and attack orders), clear `deployed`/`frozen` for an `abil.deployer` infantry before pathing, as the `deploy_fire` branch already does, and emit `AbilityEvent::Deploy(0)`.

### W7-02-04: The AI never auto-deploys or undeploys Desolators (and GIs) (major, AI use)

**Evidence:**
- **[CONFIRMED-RE 0x521050]** This InfantryClass function is called from 0x51F330 / 0x51F350. For a non-human owner (`FUN_0050b550` false) whose type has `Deployer` (+0xEC8) and `DeployFire` (+0x6AC), it deploys when `Frame > lastFrame + Rules.AIAutoDeployFrameDelay[difficulty]`. The delays come from the Rules +0xE2C vector (loader @0x66FC72) and are read at **0x52128C**.
- From the deployed missions (0x1B–0x1E):
  - an ImmuneToRadiation type re-fires or undeploys depending on the cell's RadLevel against `Weapon[1].RadLevel/3`;
  - other types undeploy via mission 0x1F.
- **[CONFIRMED-DATA]** `[General] AIAutoDeployFrameDelay=15,25,100`.
- Code: 0 readers of `AIAutoDeploy` in `engine/`. The AI deploys only when a team script contains the Deploy action (`ai_team.cpp:34`, `world.cpp:4242`). The `aimd` TaskForces "0=2,DESO" and "0=1,DESO" are fielded as plain walkers.

| | Behaviour |
|---|---|
| **Original** | Computer Desolators deploy on their own and irradiate attackers. |
| **Clone** | Computer Desolators never deploy, so the Iraqi AI's signature unit is a slow rad-rifleman. |

**Fix:** port 0x521050 into the AI step:
- per computer-owned Deployer infantry, a per-difficulty frame delay;
- deploy when idle or engaged with no destination;
- the Desolator's undeploy rule (cell RadLevel < weapon RadLevel / 3 → re-erupt, else undeploy and move on).

The GI branch belongs to the Allied slice and is shared.

### W7-02-05: The Desolator eruption is silent and does not animate (minor)

**Evidence:**
- **[CONFIRMED-PLAY]** `sinf2-pres.txt`: across 120+ deployed frames (at least 2 eruptions at ROF 60), no `DesolatorDeploy` sound is logged and there is no deploy sound.
- **[CONFIRMED-DATA]**:
  - `[RadEruptionWeapon] Report=DesolatorDeploy`;
  - `DesoSequence DeployedFire=292,7,0`;
  - DESO has no `DeploySound`, so the weapon Report is the only deploy audio.
- Code:
  - `world_abilities.cpp:600-611` emits `AbilityEvent::Eruption` and sets no `fire_flash`;
  - `pres_audio_events.inc:157` consumes only `Kind::Deploy`;
  - the only consumer of `Eruption` is the `--demo-abilities` gate (`main.cpp:16975`).

| | Behaviour |
|---|---|
| **Original** | Every eruption plays the weapon Report through TechnoClass::Fire, and the infantry plays DeployedFire. |
| **Clone** | The Desolator stands on the static Deployed frame in silence. |

**Fix:** on the Eruption event, set `u.fire_flash` (inf_anim then plays DeployedFire) and play the deploy weapon's Report at the unit.

### W7-02-06: Boris fires his AKM at the designated building; airstrike voice, cursor and SecondaryFire are missing (minor)

**Evidence:**
- **[CONFIRMED-PLAY]** `sinf3-pres.txt`: Boris ordered onto an enemy Radar logs `BorisAttackCommand` and then `BorisAttack (report)` plus `PIFFPIFF` every ~21 frames for 200 frames. That is the AKM firing at the building (BORISWH 1% vs buildings). The airstrike itself is called (`sinf4`: `airstrikes=1`).
- **[CONFIRMED-RE 0x708520]** The attack-voice picker uses:
  - TechnoType +0x580 VoicePrimaryWeaponAttack;
  - +0x584 the elite variant of the primary voice;
  - **+0x588 VoiceSecondaryWeaponAttack** (loader @0x712AEA);
  - +0x58C the elite secondary voice;
  - else VoiceAttack.

  The choice is by the weapon index that SelectWeapon picks for the target. `VoiceSecondaryWeaponAttack` has 0 readers in the clone.
- **[CONFIRMED-RE 0x51E81E]** InfantryClass::What_Action: a weapon with `MigAttackCursor` (+0x13A) on a qualifying building returns action 0x47, the airstrike cursor.
- **[SUSPECTED]**:
  - the Flare, not the AKM, is the weapon Boris holds on a building in the original;
  - the art `BorisSequence SecondaryFire=401,1,1` / `SecondaryProne` is played while designating. `inf_anim` always plays FireUp/FireProne.

| | Behaviour |
|---|---|
| **Original** | "Airstrike" voice, airstrike cursor, laser held on the target while the MiGs come. |
| **Clone** | Rifle shots at a wall and the generic attack voice. |

**Fix:**
- `choose_weapon`: prefer the Secondary when its warhead has `Airstrike=yes` and the target is a building.
- Play `VoiceSecondaryWeaponAttack` when the order's chosen slot is 1.
- Map `MigAttackCursor` to the airstrike cursor.
- `inf_anim`: play SecondaryFire / SecondaryProne for slot-1 shots when the art defines them.

### W7-02-07: Select Combatants (P) sweeps up Soviet Engineers and Terrorists (minor; same root as W7-01-02)

**Evidence:**
- **[CONFIRMED-PLAY]** `sinf3.play`: after `key p`, `selected_type(SENGINEER)=1` and `selected_type(TERROR)=1`.
- **[CONFIRMED-RE 0x731350 / 0x7316A7]** The hotkey predicate reads TechnoType **+0xDBC `IsSelectableCombatant`** (loader @0x714B09; 0 in the ctor @0x7109EB).
- **[CONFIRMED-DATA]** SENGINEER has `IsSelectableCombatant=no`. TERROR has no key, so it defaults to no.
- Code: `game_scene_controls.inc:254-256` uses `w_damage > 0 && !is_harvester`. DefuseKit Damage=1 and TerrorBomb 225 both pass.

**Fix:** parse `IsSelectableCombatant` into the unit type and use it as the predicate. One fix covers W7-01-02.

### W7-02-08: Dogs idle in Guard instead of Area Guard (`DefaultToGuardArea` unread) (minor; also ADOG in W7-01-03)

**Evidence:**
- **[CONFIRMED-RE 0x51CB0E..0x51CB45]** InfantryClass Enter_Idle_Mode picks mission 0xB (Area_Guard) instead of 5 (Guard) when **TechnoType +0xD39 `DefaultToGuardArea`** (loader @0x7142F8) or the GUARD_AREA ability is set and the unit is not in a team (+0x5D4). The UnitClass twin is 0x737BB6.
- **[CONFIRMED-DATA]** `DOG DefaultToGuardArea=yes`.
- Code: `tg.area_guard` is set only by an explicit guard order or a map mission (`unit_orders.cpp:84`, `campaign_runtime.inc:1309`).

| | Behaviour |
|---|---|
| **Original** | An idle dog scans 2 × weapon range (3 cells, threat 2, `combat.cpp:99`) and returns to its post. |
| **Clone** | It scans floor(1.5) + 1 = 2 cells, so idle dogs react later to infantry walking past. |

**Fix:** read `DefaultToGuardArea`. On spawn or idle, for a unit not in a team, set `tg.area_guard = true`.

### W7-02-09: Computer-owned infantry never scatter when hit (minor)

**Evidence:** **[CONFIRMED-RE 0x700DA0 @0x701ECE..0x702196]** TechnoClass::ReceiveDamage behaves as follows:
- When the victim retaliates (0x707C40 true), it scatters only with `PlayerScatter` (Rules +0x17ED) or a veteran/elite SCATTER ability (type +0x29F / +0x2B1).
- When it does **not** retaliate (0x702089) and is not moving, has no target and is not an aircraft:
  - a **computer-owned** object always scatters (vtbl +0x174);
  - a human-owned one only with `PlayerScatter` or SCATTER.
- InfantryClass::Scatter 0x51CF9E repeats the human gate.
- **[CONFIRMED-DATA]** `PlayerScatter=no`.
- Code: `world_w5.cpp:177-191` implements only the SCATTER-ability branch, for every house.

| | Behaviour |
|---|---|
| **Original** | Shot-at AI engineers, terrorists (`CanRetaliate=no`), dogs hit by tanks and out-of-range infantry hop to a neighbouring cell. |
| **Clone** | They stand still. |

**Fix:** in the damage path, when no retaliation target was set and the victim is idle and not moving, call `order_scatter` for computer houses (`!controllable(house)`). Keep the SCATTER and PlayerScatter rule for humans.

### W7-02-10: Garrisons cannot be given targets, never fire at buildings, and draw no occupant muzzle flash (minor)

**Evidence:**
- Code: `world_depth.cpp:233-300` `step_garrison_combat` acquires only **units**: `engageable()` iterates `units_` and nearest wins. Nothing sets `b.garrison_target` from input. `command.cpp:283-287` gives a selected garrison only Deploy (unload).
- **[CONFIRMED-DATA]** `[UCM1Carbine] OccupantAnim=UCCONS`. `OccupantAnim` / `MuzzleFlash` have 0 readers. The UC weapons have no `Anim=`, so a firing garrison shows no flash (`pres_fx.inc:732` uses only `Anim=`).

| | Behaviour |
|---|---|
| **Original** | <ul><li>A garrisoned building can be selected and given an attack order like a defence: **[SUSPECTED]**, the BuildingClass What_Action for CanOccupyFire was not traced.</li><li>It engages structures in range: **[SUSPECTED]**.</li><li>Each shot shows the occupant's `OccupantAnim` at the building's `MuzzleFlashN` art points.</li></ul> |
| **Clone** | <ul><li>Conscripts in a civilian building auto-shoot the nearest unit only.</li><li>The player cannot focus fire.</li><li>There is no visual shot origin.</li></ul> |

**Fix:**
- Accept an attack Command whose actor is an own garrisoned building and store it in `garrison_target` (or a building target).
- Extend `engageable` to enemy buildings, following the W6-04-13 threat rules.
- Spawn `OccupantAnim` at `MuzzleFlash<firing_occupant>` on each garrison FireEvent.

This overlaps with W7-01 slice C (GI garrisons) and needs one fix.

### W7-02-11: An Ivan over an already-bombed or non-Bombable target shows the attack cursor (cosmetic)

**Evidence:** **[CONFIRMED-RE 0x51E89E]** InfantryClass::What_Action for an Ivan type (+0xEAE) returns:
- 0x35 (bomb cursor) only if the target's type is `Bombable` (+0x22E) and it has no AttachedBomb (+0x38);
- otherwise 0x36 (NoIvanBomb).

| | Behaviour |
|---|---|
| **Original** | The no-bomb cursor, and the click does nothing. |
| **Clone** | <ul><li>Attack is shown.</li><li>The Ivan walks up and the plant is a no-op (`world_abilities.cpp:428`, `bomb_frame >= 0`).</li><li>`FireOnce` ends his mission, so the walk is wasted.</li></ul> |

**Fix:** in `classify`, for an Ivan selection over a target with `bomb_frame >= 0`, return NoMove (or a NoIvanBomb action).

### W7-02-12: The Secret Lab is not implemented, so its SecretInfantry (TERROR, DESO) are never granted (minor; cross-slice, tech buildings)

**Evidence:**
- **[CONFIRMED-DATA]** `[CASLAB] SecretLab=yes`, `[General] SecretInfantry=SNIPE,TERROR,DESO,YURI`.
- Code: 0 readers of `Secret` in `engine/`.

| | Behaviour |
|---|---|
| **Original** | Capturing a Secret Lab grants a random secret unit. That is how a non-Cuban or non-Iraqi Soviet gets a Terrorist or Desolator. |
| **Clone** | Never. |

**Fix:** belongs to the tech-building owner. On capture of a `SecretLab=yes` building, pick from the Secret* lists for the capturing side and add a cameo.

### Still open from earlier waves (not renumbered)

- **R-053:** Assaulter clearing of garrisons and DistributedFire. This is relevant to Conscript and Flak garrisons; SHK and BORIS are `Assaulter=no`.
- **R-054:** the dog leap (`DOGJUMP` projectile) is not drawn.
- **W6-04-23:** see W7-02-01.

## AI use

- **Build:** the skirmish AI roster (`ai_skirmish.inc:26-27,45,50`) fields E2, DOG, FLAKT, SHK, TERROR, DESO, IVAN, SENGINEER and CIVAN through the stock `aimd` TaskForces. That matches the original. BORIS is in no TaskForce, so the original does not build him either.
- **Gaps:**
  - Desolators never deploy (W7-02-04).
  - AI infantry never scatter (W7-02-09).
  - AI dogs do not area-guard (W7-02-08).
  - AI garrisoning stays off (R-081 / W6-b, unchanged).
- **Not verified here:** whether AI engineer teams capture enemy structures as the original scripts direct. The clone sends idle engineers only to neutral tech buildings (`world_w5.cpp:508-528`, RE-FALLBACK).

## Verified OK (this pass)

**Measured in play:**
- **Ivan bomb plant:** an attack order on an enemy Rhino gives `CrazyIvanAttackCommand`, then the `CrazyIvanAttack` attach sound and the `CrazyIvanBombTick` loop [CONFIRMED-PLAY sinf2 pres f=211-220].
- **Desolator deploy** creates the RadSite (`rad_total` 0 → 57787) [CONFIRMED-PLAY sinf2].
- **Boris airstrike** is called within 30 frames of the order (`airstrikes=1`) [CONFIRMED-PLAY sinf4].
- **Voices:**
  - the Soviet Engineer uses its own `EngSovSelect` / `EngSovMove`, not the Allied engineer's, even though both share image ENGINEER;
  - `TeslaTroopSelect`, `DesolatorSelect` / `DesolatorMove`, `BorisSelect` / `BorisAttackCommand` and `CrazyIvanSelect` play [CONFIRMED-PLAY sinf2/sinf3].

**Code matches the RE (re-checked):**
- Fear, prone and ProneDamage, Panic above fear 199, idle fidgets, and death by `InfDeath`:
  - `inf_anim.cpp`, matching infantry-anim.md §11–§13;
  - `pres_game.inc:992-1009`;
  - `Crawls=no` (FLAKT, BORIS, TRST, DOG) never goes Down;
  - `Fearless` (DESO) never fears.
- **Terrorist:** `CanPassiveAquire=no` and `CanRetaliate=no` are honoured (`combat.cpp:262,1142`). Suicide is a self-kill followed by the death weapon (§F).
- **Ivan:** his own death fires nothing (§E). A bombed object's death detonates its bomb at once. `IvanDamage` / IvanWH run through the faithful DamageArea (W6-04-03).
- **Radiation:** creation, accumulation, decay and the 16-frame application match §A1–§A4. `ImmuneToRadiation` is read.
- **Wave-6 items whose fix is present on master:**
  - Burst (W6-04-01): elite Flak, Boris MiGs;
  - elite Tesla shrapnel (W6-04-02);
  - IsRadBeam / IsLine drawn (W6-04-24);
  - Ivan bomb icon and sounds (W6-04-11 + #5);
  - Terror Drone buildable (W6-04-10).
- `BuildLimit=1` for Boris (`realplay_scene.inc:289`). `CreateSound` BorisCreated is played on creation (`pres_audio_events.inc:571`).
- `VoiceFeedback` (the Fear lines) is played on the NowYellow edge at 30% for player units, per the RE in `pres_audio_events.inc`.

## Evidence index

| Item | What it holds |
|---|---|
| `02-evidence/sinf1.play` + `-result.txt` / `-pres.txt` | T1 Ivan over an own tank (`cursor=1`); Ctrl on an own tank (`bombs=0`) |
| `02-evidence/sinf2.play` | <ul><li>T2 Tesla Trooper over an own TESLA (`cursor=3`; the click deselects the troopers).</li><li>T3 deployed Desolator move ignored (`ucell` unchanged).</li><li>Eruption silent (pres log).</li></ul> |
| `02-evidence/sinf3.play` | <ul><li>T5 the P hotkey selects SENGINEER and TERROR.</li><li>T6 Boris AKM fire at a Radar plus the voice log.</li></ul> |
| `02-evidence/sinf4.play` | Boris airstrike called (PASS) |
| `02-evidence/{fs,offs,offs2}.py` | Ghidra helpers. Raw decompiles: `D:\ra2-re\w7-sinf-out\dec1.txt` (0x6FF380, 0x51E17C, 0x51CB2A, 0x51CF9E, 0x481580), `dec2.txt` (0x521050, 0x708520), `dec3.txt` (0x6F2890, 0x4D4F90), `dec4.txt` (0x731350), `dis2.txt` / `dis4.txt` (ReceiveDamage scatter block) |
