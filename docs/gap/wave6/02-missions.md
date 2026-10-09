# Wave-6 gap analysis 02: the missions not yet won (ALL09, ALL12, SOV08, SOV09, SOV10, Boot Camp)

Base: master `e2b569e`. Branch `w6-missions`, worktree `D:\ra2-wt\w6-missions`, Release build `build-rel/`.

This is a read-only audit: no engine code was changed.

**RE sources.** Ghidra headless was run on private copies of `p4proj` (`D:\ra2-re\w6-missions{,-a,-b,-c,-d}`). The decompiles and scan scripts are in the matching `-out` directories.

**Evidence.** Probe scripts, log excerpts and screenshots are in `docs/gap/wave6/02-evidence/{a,b,c,d}/`.

**Method.** Four opus sub-agents did the RE and probe work: (a) difficulty, (b) SOV08/SOV09, (c) ALL12/SOV10 and the superweapon AI, (d) ALL09/ALL05/Boot Camp. Every gap below was then spot-checked against the cited clone line. The difficulty binding was confirmed independently a second time (see W6-02-01).

## Fix status (w6fix-missions, wave-6 fix agent #2)

| ID | Status | Where |
|---|---|---|
| W6-02-07 | **FIXED.** `World::legal_threat_unit/_building` (LegalTarget, IsInPlayfield, campaign Insignificant unit / Insignificant building of a MultiplayPassive house), used by the idle auto-acquire (unit + building branch), campaign `nearest_enemy` (team Attack / Patrol) and the AI team quarry pickers. krem2 probe: Civie1 keeps all 141 buildings (was 8 at f20260). | `engine/world_w6.cpp`, `combat.cpp`, `campaign_runtime.inc`, `world.cpp` |
| W6-02-08 | **FIXED.** Per-object IsInPlayfield (`Unit/Building::tg.in_playfield`): set from IsWithinUsableArea at placement, gained when a unit moves in, recomputed for every object at scenario start and on action 40 ResizeView (`World::set_playfield`). The w6fix-ai SW picker scores 0 outside the usable area (`cell_in_playfield`). | `world_w6.cpp`, `campaign_runtime.inc` `apply_playfield` |
| W6-02-01/02 | **FIXED.** Per-house handicap set for every campaign house at scenario start (player + PlayerControl = slider class, others 2 - slider), existing units refreshed; `refresh_vet_stats` reads it. SOV09 Easy probe: enemy E1 ROF 24, GHOST 12, MTNK 72 frames. | `World::set_house_handicap`, `setup_houses` |
| W6-02-03 | **FIXED.** Defence rearm (incl. #4's Burst and defence-vs-building rearms) = ftol(ROF x owner handicap). | `building_combat.cpp` (ROF multiplier only) |
| W6-02-04 | **FIXED.** `build_time_mul` = 1.0 (BuildTime/BuildDelay are CRC-only). R-082 closed as faithful. | `ai_base.cpp`, `docs/re/ai-base.md` |
| W6-02-05 | **FIXED (campaign).** An AI house armed later starts its TeamDelays clock as if it had run since frame 0, + HouseArray index x 175. Skirmish arming is #3's area and unchanged. | `arm_production_houses` |
| W6-02-09 / 10 | Done by #3 (w6fix-ai); the playfield TODO in its picker is wired. | |
| W6-02-11 | **FIXED.** Threat radius = GuardRange, else larger weapon range + 1 cell (+AirRangeBonus); area guard (map Mission=Area Guard, player Guard order) 2x, max 16 cells; Sight never read (also the leash). | `combat.cpp` `acquire_radius` |
| W6-02-12 | **FIXED.** ImmuneToPsionics (map overrides merged, also for voxel types) blocks mind-control acquisition and capture. SOV09 probe: no Yuri holds a car at f1500 (was f1156). | `combat.cpp`, `poll_mind_control` |
| W6-02-13 | Left to #4 (building_combat owner); #4 merged a 3-D defence range. | |
| W6-02-14 | **FIXED.** The TeamType Tag is attached to recruited members (AttachTrigger replace semantics). ALL05 probe: 'Tanya Dies' fires, LOST. R-P3-4 closed. | `adopt` |
| W6-02-15 | **FIXED.** Units carry their rules id (`tg.t.rules_id`); the campaign tracks built units by it. TRN02: '6Complete(GIBuilt)' fires on the 6th GI. | `sync_tracks`, `resolve_unit_w6` |
| W6-02-16 | **FIXED.** A team starts its script only at full strength (never dropped while waiting); Reinforce=yes teams refill and regroup at <= total/3. RE-FALLBACK: the regroup movement is not modelled. | `step_teams`, `team_total` |
| W6-02-17 | **FIXED (partial).** Recruit skips Sleep/Harmless/Hunt/Harvest/Capture and mind-controlled units; one per TaskForce entry per call, nearest the team anchor. Foreign-group penalty / RecruitableA/B not modelled. | `recruit` |
| W6-02-18 | **FIXED.** Script 16 Patrol is an attack-move (lead-member scan, weapon range + 1, every 15 frames; disguised spies skipped). | `patrol_tick` |
| W6-02-19 | **ANSWERED.** CAAIRP grants ParaDropSpecial (Type=ParaDrop): an Allied owner drops AllyParaDropNum=6 E1, not 8 (wave4/05 corrected). | |
| FUN_006862D0 | **APPLIED.** MultiplayerAICM start money also applies to campaign houses (no GameMode gate): +400 % for class 0 (computer houses on Hard, PlayerControl houses on Easy). | `setup_houses` |

### Mission scripts after the fixes (merged with master incl. w6fix-units, one at a time)

Temporary env toggles (removed before hand-back) bisected each regression to one fix.

| Script | Result | Cause / change |
|---|---|---|
| m-all03/04/07-real, m-all09-opening, m-sov02/04/11-real | PASS unchanged | |
| m-sov01-real | PASS after a generator change | W6-02-11: the engineer walks inside the Conscript group (it trailed it and died). |
| m-sov07-real, m-sov12-real | PASS on Easy | W6-02-11/16; on Normal the defence no longer holds. Easy = player ROF x0.8, computers x1.2. |
| m-all12-real (opening) | PASS after a script change | W6-02-11 area guard: BadGuy1's Area Guard tanks (2 x range) attack the Con Yard; the IFVs are sent at them. |
| **m-trn02-real (new)** | **WON** (real orders, `mk_trn02.py`) | needs W6-02-15. |
| m-all05-real | FAIL | W6-02-11 (toggle 1 alone restores the win): the first disguised spy no longer reaches its power plant, so no blackout; Tanya then dies waiting. Not re-tuned. |
| m-all10-real | FAIL (RUNNING at the end) | W6-02-16 / W6-02-18 (either toggle restores the win): BadGuy2's waiting full-strength team (14 units) stays at its Con Yard and the broke army cannot clear it. |
| m-all11-real | FAIL | W6-02-18: hydrofoil / Dreadnought patrols attack-move through the IFV staging area. With it off it still ends RUNNING (w6fix-units transport Size, see the coordinator note on the GHOST:8 / FV~:8 pairs). |
| m-sov09-real | FAIL | the opening's timing (SOV09 is now Easy-calibrated: no Yuri wastes its slot on a car, SEALs engage at range + 1); not re-tuned. |
| m-all06-real | FAIL (RUNNING at f140096) | long sweep; not bisected (802 s per run). |
| SOV08 V3 siege attempt | not won | the V3 cameo clicks queued nothing (only one V3 built, f10385); the Vlad2 flak circles also cover every 18-cell V3 site west of the White House except (84,22)/(84,23) (path within 9.7 of a flak). Generator not committed. |
| ALL12 / SOV09 / ALL09 / SOV10 full wins | not attempted | time spent on the regressions above. |

## Headline

The earlier agents concluded "tactics, not engine". For four of the five missions that conclusion does not hold.

1. **Difficulty is miscalibrated in every campaign mission.** In the original, on Easy:
   - the player gets the `[Easy]` handicap: ROF 0.8, so 1.25x damage per second;
   - every computer house gets `[Difficult]`: ROF 1.2, including all its defence buildings.

   The clone gives the player nothing. It also gives nothing to the defences and to computer houses that are never AI-armed. So the clone's Easy plays roughly like the original's Normal, and every "not won on Easy" proof ran about 1.5x harder in damage per second than the original's Easy (W6-02-01..03).
2. **ALL12 is lost through an engine bug, not through the Lightning Storm** (W6-02-07; closes R-P1-2). The clone lets BadGuy1's teams and units attack Insignificant civilian buildings of a MultiplayPassive house. They raze Moscow (141 to 8 Civie1 buildings) and eventually the Kremlin, which fires `Kremlin is Destroyed`. The original's `Evaluate_Object` rejects those targets in campaign mode.
3. **SOV09 is harder than the original because of two engine differences** (W6-02-11, W6-02-12):
   - units auto-acquire at their **Sight** instead of the guard range;
   - `ImmuneToPsionics` is ignored, so the Yuris waste their single control slot on cars.
4. **SOV08 never needed a capture.** Destroying the White House (DestroyedByAnything, trigger `WHDestroyed`) wins in the original and in the clone. The P2b push already had it at 800/1000. So SOV08 is a tactics and planning gap, not an engine gap.
5. **SOV10 has a win the original does not allow** (W6-02-08). Objects outside `LocalSize` are not "in playfield" in the original and cannot be targeted, so the clone's "Harriers bomb the WCD and win the mission" is wrong. Without that shortcut, SOV10 is genuine tactical difficulty.
6. **Boot Camp.** TRN01 is WON by real clicks. TRN02 breaks at objective 6 (W6-02-15): a factory-built unit is tracked under its art `Image=` (GI) instead of its rules id (E1), so "Build GIs" never completes. The same bug breaks build events in ALL02, SOV01, SOV03, SOV04 and ALL11.
7. **ALL05's missing Tanya-death loss** is W6-02-14. A TeamType `Tag=` is never attached to *recruited* members, and in the original the tag is on every member.

## Gap table

Severity: BLOCKER / MAJOR / MINOR. Effort: S / M / L. "Fix at" is the suggested location.

### Difficulty (all missions)

#### W6-02-01 — The human player (and PlayerControl houses) get no difficulty handicap

MAJOR, effort S.

- **What the player experiences:** on Easy, the player's units and defences reload in 1.0x the time instead of 0.8x, which is 20% less damage per second than the original. On Hard they fire faster than they should (1.0 instead of 1.2). Example: a Grizzly (ROF 60) reloads in about 60 frames, against 48-50 in the original.
- **Clone evidence:**
  - `engine/world_veterancy.cpp:50-56`: the house FirePower/ROF is read only `if (ai_enabled(u.house))`.
  - `engine/campaign_runtime.inc:700-731`: nothing sets a handicap for house 0 or for PlayerControl houses.
- **Original:**
  - `Start_Scenario` `FUN_00686560` (0x6865B2-0x6865D5), campaign branch: Scen+0x60C = slider and Scen+0x610 = 2 - slider.
  - `FUN_005007d0` (0x5008E7-0x5008FE): a house with IsHuman (+0x1EC) or IsInPlayerControl (+0x1ED) gets `AssignHandicap(Scen+0x60C)`. Every other house gets `AssignHandicap(Scen+0x610)`.
  - `AssignHandicap` `FUN_004F6DD0` copies Rules+0x1538 + d x 0x50 (`[Easy]` / `[Normal]` / `[Difficult]`).
  - The ROF is consumed in `TechnoClass::GetROF` `FUN_006FC460` @0x6FC585: `FMUL [owner+0x1A8]`, giving r = ftol(ROF x houseROF + rand(0,2)).
  - Confirmed twice: by sub-agent a and by an independent caller scan (`D:\ra2-re\w6-missions-out\caller_005007d0.c`, `w_00686560.c`).
- **Fix:** keep a per-house handicap in World and set it in the campaign `set_house` loop. Player and PlayerControl houses get group[launch.difficulty] (FirePower, ROF, RepairDelay). `refresh_vet_stats` reads that handicap instead of the `ai_enabled` gate.

#### W6-02-02 — Computer houses that are never AI-armed get no handicap; armed houses only cover new units

MAJOR, effort S.

- **What the player experiences:** on Easy, enemy forces of houses with no ProductionBegins or AITriggersBegin fire 1.2x faster than in the original. The affected houses:
  - SOV09: all of them (Americans, French, Germans, British are never armed);
  - ALL09: BadGuy2, BadGuy3 and Balabos;
  - ALL12: BadGuy2 and BadGuy3.

  Units of armed houses that already exist when the house is armed keep 1.0 until they are promoted.
- **Clone evidence:**
  - `world_veterancy.cpp:52`: only armed houses are read.
  - `campaign_runtime.inc:826`: the mods arrive only through `make_ai_config` at arming time.
  - `refresh_vet_stats` runs only at spawn and on a rank change.
- **Original:** `FUN_005007d0` handicaps every non-human house with 2 - d at scenario start (as in W6-02-01).
- **Fix:** the same per-house table, filled at scenario start for every house. Re-run `refresh_vet_stats` over a house's units when its handicap changes.

#### W6-02-03 — Defence-building weapons ignore the house ROF

MAJOR, effort S.

- **What the player experiences:** enemy Tesla Coils, Prism towers, pillboxes, sentry guns and flak fire at base ROF on every difficulty. A CoilBolt (ROF 80) should take 96-98 frames to reload on Easy but takes about 80. Defences are the core of SOV08 (the Tesla gate), SOV09 (the Prism compound), SOV10 (the Prism island) and ALL12.
- **Clone evidence:** `engine/building_combat.cpp:47` (`s.w_rof = w.rof`), `:289` and `:315` (`rearm_secs(w.rof, ...)` with no owner multiplier).
- **Original:** the Building vtable 0x7E2EBC, slot +0x318, is `GetROF` 0x6FC460, the same function as for units. `Fire` 0x6FD210 is at slot +0x3CC.
- **Fix:** multiply in the owner's handicap ROF before `rearm_secs`, in the ftol(ROF x mult + rand(0,2)) form.

#### W6-02-04 — The clone applies a BuildTime handicap the original never reads; BuildDelay is unread too

MINOR, effort S.

- **What the player experiences:** on campaign Hard, and in skirmish on Brutal (the AI gets `[Easy]` BuildTime .8), AI buildings finish 25% faster than in the original.
- **Clone evidence:** `engine/world.cpp:3145` (`cfg.build_speed * cfg.mods.build_time_mul`); `engine/ai_base.cpp:24`.
- **Original:**
  - house+0x1B8 (BuildTime) is written only at 0x4F6FFA and read only by the house CRC (0x502C31). `Time_To_Build` `FUN_006F3D00` uses only the country multiplier `FUN_0050BEC0`.
  - +0x1C8 (BuildDelay) is CRC-only (0x502C5B).
  - An FPU-operand scan of the whole binary found no reader of Groundspeed/Airspeed/Armor/Cost (+0x190/+0x198/+0x1A0/+0x1B0). R-082 can therefore be closed as faithful.
- **Fix:** set `build_time_mul` to 1.0. Correct ai-base.md §1/§4.

#### W6-02-06 — (number unused; the BuildDelay note is folded into W6-02-04)

#### W6-02-05 — The first AI team check after arming waits a whole TeamDelays period

MINOR, and it makes the clone easier; effort S.

- **What the player experiences:** after AITriggersBegin the clone always waits the full delay (3500 frames, about 233 s, on Easy) before the first team pass. The original's timer has been running since scenario start.
- **Clone evidence:** `world.cpp:3941` (`check_clock` starts at 0 on arming).
- **Original:** `AssignHandicap` seeds house+0x5798/+0x57A0 = now + TeamDelays[d] + houseIndex x 175. `FUN_00500960` @0x500F3F does the same. The timer is checked and reset in `FUN_004f8350`.
- **Fix:** seed the clock as if it had run since frame 0, with the +175 x index offset.

Verified faithful, do not re-report:
- the slider maps 0..2 with no remap;
- the map-trigger difficulty gate (plain Scen+0x60C, `FUN_00725240`, action 53 `FUN_006e2060`);
- the campaign AI-trigger enable (`FUN_0041e740` campaign branch, plain index; the clone's double inversion nets to the same result);
- armed AI houses use class 2 - d, which is now RE-backed (the `campaign_runtime.inc:768` RE-FALLBACK can be lifted);
- TeamDelays, BuildSlowdown and the base-defence counts per class;
- the IQ comes from the map only;
- CampaignMoneyDelta.

Which triggers differ by difficulty, per mission, is listed under each mission below.

### Targeting legality (ALL12, SOV10, every campaign map)

#### W6-02-07 — The AI attacks Insignificant buildings of MultiplayPassive houses; this is the real cause of R-P1-2 (ALL12 "Kremlin lost")

BLOCKER for ALL12; effort M.

- **What the player experiences:** after `Begin Prod: Bad Guy 1`, BadGuy1's Apocalypses and Conscripts raze Moscow's civilian buildings: Civie1 goes 141 to 111 (f7156), to 24 (f18858), to 8 (f20260). When the Kremlin (CARUS03, index 160) falls, `Kremlin is Destroyed` (BuildingDoesNotExist 160) loses the mission. The Lightning Storm was a coincidence: one storm costs the Kremlin about 80 HP (IonWH does 2% against concrete, about 5 per bolt), and the Black Guard never targeted it.
- **Evidence:**
  - `02-evidence/c/all12-civilian-razing.txt`, `krem2.play` (BadGuy1 target buildings 159/197 = CAPARK02/CAFARM06).
  - `campaign_runtime.inc:1982` `nearest_enemy` (any enemy building).
  - `engine/combat.cpp:1028-1031` (R-064: computer units may auto-acquire any enemy building).
- **Original:** `Evaluate_Object` `FUN_006f71d0` rejects, before any scoring:
  - a type with LegalTarget (+0x231) off;
  - an object with IsInPlayfield (+0x3D5) off;
  - in a campaign (`DAT_00a8a1a0==0`), an Insignificant (+0x232) object, or a building whose owner's country is MultiplayPassive.

  Civilian1 is MultiplayPassive, and 31 of the 32 Civie1 types in ALL12 are Insignificant, the Kremlin included.
- **Fix:** add one `World::legal_threat(scanner_house, target)` predicate mirroring these rejects. Use it in `nearest_enemy` (team Attack/46/47), the combat.cpp auto-acquire building branch and the AI superweapon picker. Close R-P1-2 under this ID.

#### W6-02-08 — IsInPlayfield is not modelled: the AI can target objects outside LocalSize (SOV10 Harrier win)

MAJOR (an unfaithful easy win); effort S-M.

- **What the player experiences:** in SOV10, BadGuy2's `Harassment Team_5` (4 Harriers) can bomb the Weather Control Device (GAWEAT, Other3, at (17,95)) while it is still outside `[Map] LocalSize`. That fires `WCD Destroyed`, and the player wins without ever capturing the Battle Lab.
- **Clone evidence:** IsWithinUsableArea is only used for input and the camera (`engine/main.cpp:7278-7319`, `engine/campaign.cpp:204`). None of the target pickers check it.
- **Original:**
  - `TechnoClass::Unlimbo` `FUN_006f61d0` sets IsInPlayfield = IsWithinUsableArea(cell) (`FUN_00577e90`).
  - Action 40 ResizeView recomputes it for every object (`FUN_00566e60`).
  - A unit's flag only goes false to true as it moves in (`FUN_006f45f0`).
  - `Evaluate_Object` rejects targets that are not in the playfield.
  - Other3 is hostile to BadGuy2 by Allies=, but the WCD becomes a legal target only after `BL Captured`.
- **Fix:**
  - add a per-object in-playfield flag;
  - set it at placement and when a unit moves in;
  - recompute it on ResizeView (`campaign_runtime` action 40);
  - check it in `legal_threat` (W6-02-07).

#### W6-02-11 — Idle units auto-acquire at their Sight range instead of the guard range (extends R-064)

MAJOR (SOV09 and every guard-heavy map); effort S.

- **What the player experiences:**
  - The SOV09 American SEALs (Sight 8) engage at 8 cells. That equals the Yuris' mind-control range, and one MP5 shot (125) kills a Yuri (100 HP), so a Yuri cannot take a SEAL "from outside its reach".
  - Enemy dogs (Sight 9, bite range 1.5) charge from 9 cells, and Shock Troopers react from 6 cells (range 3).
  - The player's own Yuris (Sight 12) wander up to 12 cells to grab cars.
- **Clone evidence:** `engine/combat.cpp:960-998`: `rmax = max(range0, range1, sight) + 1`, and the per-candidate range is `max(weapon range, sight)`. This is marked RE-FALLBACK W2-SK-25.
- **Original:**
  - The threat scan `FUN_006f8320` (which contains YRpp 0x6F8DF0) takes its radius from the guard-range virtual `vtbl+0x31c` = `FUN_007072e0`:
    - threat=1 (plain guard): the type's GuardRange if set, else the larger weapon range, + 1 cell + AirRangeBonus (+0x68C);
    - threat=2 (area guard): double that, clamped to 0x700..0x1000 leptons.
  - Sight (+0x5E8) is never used.
  - Infantry and unit idle scans pass 1 (@0x51F092, `FUN_004d4ce0` @0x4D4D5E). Area guard passes 2 (@0x4D4234).
  - The rescan interval is NormalTargetingDelay (27 frames).
- **Fix:** in the combat.cpp auto-acquire, use `GuardRange>0 ? GuardRange : max(range0, range1)` plus 1 cell, doubled for area guard, and drop Sight.

#### W6-02-12 — `ImmuneToPsionics` is ignored by mind control

MAJOR (SOV09); effort S.

- **What the player experiences:** at campaign frame 1156, before any input, a Yuri already controls a neutral CAR (`units(6,CAR)=1`) and has used up its single control slot. The SOV09 map sets `[CAR]` and `[DOG] ImmuneToPsionics=yes` precisely to stop this. The Yuris also walk off toward cars, into patrols.
- **Evidence:**
  - `02-evidence/b/sov09-yuri-controls-immune-car.txt`, `sov09-t77.png`.
  - `grep -ri psionic engine/` finds nothing.
  - Mind control is `campaign_runtime.inc:1497-1553` `poll_mind_control`.
- **Original:** [RE-FALLBACK: standard YR behaviour documented by YRpp/ModEnc; the map override only makes sense if it is honoured] a mind-control warhead cannot target or capture an ImmuneToPsionics object.
- **Fix:** honour the flag (with map overrides merged) in `poll_mind_control` and in auto-acquire for `MindControl=yes` warheads.

#### W6-02-13 — Defence range is measured 2-D from the building centre

MINOR, unverified in play; effort S.

- **What the player experiences:** on raised ground, Prism towers and Tesla coils reach slightly less or more than in the original.
- **Clone evidence:** `engine/building_combat.cpp:191-195`.
- **Original:** `FUN_006F6750` uses a 3-D range plus the weapon elevation bonus.
- **Fix:** use `unit_in_weapon_range`, as P2b did for mind control.

### Superweapon AI (ALL12)

#### W6-02-09 — The AI superweapon picker scores the wrong target set

MINOR; effort S.

- **What the player experiences:** the BadGuy1 nuke can target any hostile house, not just its designated enemy. The ALL12 outcome is the same, because a ConYard hit at about 10.5 min is one of the faithful tie-breaks.
- **Clone evidence:** `engine/world.cpp:3205` `ai_try_fire_superweapon`. It scores every enemy house, needs no Enemy, gives no zero score outside the usable area, has no cloak randomisation, and scores every non-harvester unit 2.
- **Original:** `FUN_0050ca10`, called from `FUN_00509710` (nuke) and `FUN_00509c20` (storm):
  - **Target set:** only objects of `HouseArray[house+0x5600]` (the Enemy), on the Ground layer, alive and not in limbo.
  - **Building scores:** ConYard / WarFactory (not naval) / Power / BaseDefense / tech centre values from `AIIonCannon*Value[house+0x184]`; any other building scores 4.
  - **Unit scores:** Harvester, MCV list and Passengers>0 get their own values; other vehicles 2; Engineer and Thief values; other infantry 2; aircraft 1.
  - **Adjustments:** a cell outside the usable area scores 0; a cloaked object scores rand(0, best+10).
  - **Enemy requirement:** the nuke needs Enemy != -1. In a campaign it is set only by MakeEnemy (`FUN_004f9db0`) or by damage anger (`FUN_005045b0` from `ReceiveDamage` `FUN_00700da0`).
- **Fix:** add the Enemy/anger field, use that target set and these scores, and do nothing when there is no Enemy.

#### W6-02-10 — The AI auto-fires the Iron Curtain and Chronosphere, which the original never does

MAJOR (ALL12: BadGuy1 owns NAIRON); effort S.

- **What the player experiences:** ALL12's Apocalypse groups get extra invulnerability windows on every AI tick.
- **Clone evidence:** `engine/world.cpp:3270-3302` (W2-AN-21, RE-FALLBACK) curtains or chronoshifts its own vehicle clusters.
- **Original:** the switch in `FUN_00509710` has cases 0 (nuke), 2 (storm), 5/6/8/0xB, 7, 9 and 10. There is no case for 1 (IronCurtain), 3 (ChronoSphere) or 4 (ChronoWarp). Those are used only through script action 55 (IronCurtainMe) and AITriggers.
- **Fix:** delete those branches and keep action 55.

### Teams and triggers

#### W6-02-14 — A TeamType `Tag=` is not attached to recruited members (root cause of R-P3-4)

MAJOR; effort S.

- **What the player experiences:** in ALL05, Tanya dies and the mission keeps RUNNING (`Tanya Dies` never fires). In other maps, scripted losses and conversions are missing.
- **Evidence:**
  - `02-evidence/d/all05-tanya.play`: Tanya is killed at f567 after `Enable Tanya Dies`, and the mission is still RUNNING 450 frames later.
  - Tanya reaches her tag only through the CreateTeam `Tanya (2)` (`Tag=088361A0`).
  - `campaign_runtime.inc:1678-1710` `recruit()` and `:1971-1975` never set the tag; only reinforcements pass `team_tag` (`:1910`, `:1942`).
- **Original:**
  - The team constructor `FUN_006e8000` creates the TagClass (`FUN_006e4350`) from TeamType+0xD0.
  - `AddMember` `FUN_006e9a70` calls `ObjectClass::AttachTrigger` `FUN_005f5360` on every member (skipped only for OnTransOnly non-transports).
  - A change of owner keeps obj+0x34, so the tag survives.
- **Also affected:** ALL02 (`1 Give French Back`, `Rocketeers Destroyed`), ALL03/ALL04 `BridgeEngDied`, ALL04 `PlayerLoss(MCVsunk)`/`CreateNewPatrol3`, SOV03 `Allied Conversion` (7 teams), SOV05 `Suicide Dies`, TRN02 `RedTeamA/B-Destroyed`.
- **Fix:** in `recruit()` and `adopt()`, set `utrack_[i].tag = data_.find_tag(tt.tag)` and call `sys_->attach_tag`.

#### W6-02-15 — Factory-built units are tracked under their art `Image=`, not their rules id

MAJOR (it breaks TRN02, among others); effort S.

- **What the player experiences:**
  - In Boot Camp 2 the player trains 6 GIs and objective 6 "Build a squad of GIs" never completes. The Red Team waves never come, objective 7 is never given, and `DiscoverBarn` never fires.
  - The same thing happens with build events in ALL02 (`GI/MTank/Grizzly/Rocketeer Check`), ALL11 (`Zone Defenders Go 2`), SOV01 (`Sofia Line 1`; `Hint: Engineer` resolves to the wrong index), SOV03 (`Rocketeer Detected`) and SOV04 (`End Intruder Harrassing`).
  - Produced CONS/GI/GTNK units also never match TaskForce `E2/E1/MTNK`, so scripted teams cannot be refilled from production.
- **Evidence:**
  - `02-evidence/d/trn02-real.play` (real orders): `units(0,E1)=6` at f2664, and `6Complete(GIBuilt)` never fires over 4000 more frames.
  - `campaign_runtime.inc:1014` `t.type = camp::up(u.image)`. YR rulesmd has E1 Image=GI, E2 CONS, MTNK GTNK, GHOST SEAL, JUMPJET ROCK, ORCA FALC, AMCV MCV.
- **Original:** the build events compare the produced TechnoType index [map-triggers.md events 19-22].
- **Fix:** resolve the rules id at `:1014` (the same lookup as `play_unit_id`, `main.cpp:6995`).

#### W6-02-16 — Teams start their scripts before reaching full strength, and `Reinforce=` teams never refill

MAJOR in general (team timing in every mission); effort M.

- **What the player experiences:** scripted and AI teams set out with one member and arrive piecemeal. `Reinforce=yes` teams never regroup or refill.
- **Clone evidence:** `campaign_runtime.inc:1973`, `:2093-2096`. A team runs as soon as it has one member and is dropped after 900 member-less frames.
- **Original:**
  - TAction 4 only constructs the team (`FUN_006eff20`); `Force_Active` `FUN_006e95f0` has no callers.
  - `TeamClass::AI` `FUN_006e86b0` moves the team only when IsFullStrength (+0x79, set by `FUN_006e9950` when TotalObjects equals the TaskForce total) or IsForcedActive is set. Until then it regroups (`FUN_006eb030`) and recruits.
  - `Reinforce=yes` teams that drop to `count <= total/3` go back to regroup and refill (`FUN_006e9640`).
  - `Full=` (+0xA5) is not read by team code; it only affects reinforcement loading.
- **Fix:** gate the script start on full strength, and implement the Reinforce re-recruit and regroup.

#### W6-02-17 — Recruit filters the clone does not apply

MINOR (no ALL09 effect); effort S.

- **What the player experiences:** teams can steal units that are Sleeping, Harmless, Hunting, Harvesting, Capturing and so on, units of another group, mind-controlled units and loaded transports.
- **Clone evidence:** `recruit()` only checks liveness, house, cargo, priority and the recruitable flag, and walks units in index order.
- **Original:** `FUN_006e9ff0` and `Can_Add` `FUN_006e9b80` check:
  - the mission is recruitable (`FUN_005b3110`);
  - the group matches for `Recruiter=no` teams;
  - RecruitableA/B (+0x421/+0x422) by Autocreate;
  - the unit is not mind-controlled and carries no passengers.

  Units are taken nearest-first to the team anchor (+0x3200 penalty for a foreign group), one per TaskForce entry per tick.
- **Fix:** apply these filters in `recruit()`.

#### W6-02-18 — Script action 16 Patrol is a plain move

MINOR (it makes the clone easier); effort S.

- **What the player experiences:** patrols and response teams walk past targets instead of engaging en route.
- **Clone evidence:** `engine/campaign.cpp:439-451` maps 16 to MoveWaypoint.
- **Original:** `FUN_006ec240` scans for threats from the strongest member every N frames, retargets (`FUN_006e85c0`), attacks (`FUN_006ea9f0`), otherwise regroups and moves.
- **Fix:** implement Patrol as an attack-move that scans with the team's threat rule. This supersedes the R-027 "patrol = move" fallback.

#### W6-02-19 — Open question: ALL09 Tech Airport paradrop size

MINOR; effort S.

- wave4/05 says "8 GIs every 4 min". For an Allied owner of a `Type=ParaDrop` the original drops `AllyParaDropInf=E1` x `AllyParaDropNum=6`. The clone reads both pairs (`engine/w2air_main.inc:115-117`).
- Next step: verify in play which pair the captured CAAIRP uses, and correct the wave4 doc if the clone drops 6.

## Per mission

### ALL09 (Yucatan)

**Engine blockers:** none that make it harder than the original.
- W6-02-01/02/03 apply: the Easy runs fought 1.2x-ROF defences of BadGuy2/BadGuy3/Balabos without the player's 0.8 ROF.
- W6-02-16/18 make the clone somewhat *easier*: no Reinforce refill, no attack-move.

**The overrun is original design.** The 'Attk Response' chain is faithful:
- Both tags are one-shot (volatile).
- Response 1 is on 2 HTNK, 2 SHK and the Teslas at (65,112)/(69,108). Response 2 is on 2 HTNK, 1 HTK and 2 NALASR.
- Each trigger disables the other; `Attacked Radar` is unattached.
- One fire creates 8 teams once, bypassing `Max`.
- All BadGuy1 objects are recruitable in the original too. Priority 16 takes from the Perimeter (7) and Beach (10) patrols; the Dog (16) and Shock @Y/Z (20) patrols are protected.
- Probe `02-evidence/d/all09-response.play` fills the same 42 units (4 HTNK, 3 HTK, 5x6 SHK/E2, 5 DOG) as the original.

**Difficulty differences (map data; the clone enables the right set):**
- Easy gets 2-SEAL + engineer reinforcements after each pyramid; Normal gets 1 SEAL.
- Easy has no Flak Patrol, and adds an Easy-only `Destroy Vehicles`.
- The BadGuy1 map AI attacks are Normal/Hard only and `=no` in `[AITriggerTypesEnable]` anyway.

**Winning plan:**
1. Play on Easy (after W6-02-01..03).
2. SEALs clear the village and free the POWs and Grizzlies.
3. Engineers capture the Tech Airport, then build up over several paradrop cycles at the village choke.
4. Do **not** touch the tagged Teslas, HTNKs or laser turrets, and do not capture the ConYard yet.
5. Provoke the response from range and fight the 42 units at the village with deployed GIs and Grizzlies. They come along wp1, 21, 31, 32 and then Hunt.
6. The response empties BadGuy1's base. Then capture the ConYard, Barracks and War Factory, C4 or capture the pyramids (objective 1), and level BadGuy1.

### ALL12 (Moscow)

**Engine blockers:**
- **W6-02-07** (BLOCKER): civilian razing, then the Kremlin falls, then the mission is lost.
- **W6-02-10** (MAJOR): the AI Iron Curtain protects the Apocalypses.
- W6-02-09 (MINOR); W6-02-01..03.

**Faithful:**
- **SEALs are buildable:** the map sets `[GHOST] TechLevel=5`, with GAPILE + AMRADR. Probe `02-evidence/c/ghost.play` gives `can_build(GHOST)=1`. The WIP generator had lost its AMRADR.
- **Kirov:** `Kirov Medium` is Normal/Hard only, so the ~6.5-min Kirov on Normal is faithful.
- **AI triggers by difficulty:** Easy enables 10, Normal 17, Hard 20. Easy lacks Occupy, Apocs 2, V3s 1, Defense 2, Backdoor L1/R1 and Kirov Medium.
- **Lightning Storm:** about 80 HP per storm against the Kremlin.

**Winning plan (after W6-02-07):**
1. SEALs C4 the BadGuy2 Tesla Reactor through the fence gap (`m-all12-real`).
2. Deploy the Chrono MCV and build power, refinery, barracks, war factory, Airforce Command, Battle Lab, Chronosphere and Weather Control, plus anti-air on Normal.
3. Chronoshift the eastern Black Guard Apocalypse pair into the lake.
4. Lightning Storm on (118,122) for the western pair and the Tesla coil.
5. Kill the remaining flak and sentry guns with SEALs carried in IFVs and chronoshifted into the yard, or with a second storm. The storm is safe for the Kremlin.
6. `Kremlin Secured`, Tanya's IFV, win.

### SOV08 (Washington; the White House)

**Engine blockers:** none.
- W6-02-11 explains why Shock Troopers and dogs feel over-aggressive.
- W6-02-03: the four Tesla Coils fire 20% too fast on Easy.

**Faithful:**
- **V3 interception:** V3s are 50-HP aircraft, FlakWH does 100%, and Flak Cannon, Flak Track and Patriot can all kill them.
- **Tesla Coil range:** 7, or 8 when charged. The earlier "range 10" was wrong.
- **Shock Troopers vs heavy armour:** 100%. ApocAP does 25% against Plate.
- **Moving units:** a unit on a move order does not stop to fight, in the original as in the clone.

**Key fact:** the White House's tag `WhiteHouseTaken1` fires `WHDestroyed` (DestroyedByAnything), which enables `WhiteHouseTaken2`, which wins 9 s later. `02-evidence/b/sov08-whdestroyed-win.txt`: a cheat kill at campaign frame 150 gave WON about 11 s later. No engineers are needed.

**Difficulty differences:** `!SendTanks` and the paratroopers are Normal/Hard only; EliteApocGuard is Hard only.

**Winning plan:**
1. Deploy the MCV at (59,97).
2. Build two sentry guns before the refinery to stop the paradrops.
3. Build V3s and Apocalypses with Flak Track escorts.
4. Siege the White House (1000 HP, concrete; V3 = 200 x 50% = 100, so about 10 hits) from its west or north side. Stay at least 12 cells from Vlad2's flak at (94,28) and (106,28); the White House at (98,14) is outside their circles.
5. Or bring the Apocalypse ball to range about 5.75 *with anti-infantry escorts and attack orders*, not a move order.

### SOV09 (Mexico; mind-control the President)

**Engine blockers:**
- **W6-02-12:** immune cars soak up the Yuris.
- **W6-02-11:** SEALs out-acquire the Yuris at 8 cells.
- W6-02-01..03: no house is ever armed, so the whole enemy force fires 1.2x faster on Easy than in the original.

**Faithful:**
- **R-202 confirmed NOT A GAP:**
  - CanBuild `FUN_004f7780`/0x4F7870 tests `Owner & (1 << HouseType+0xB8)` ORed with the four +0x2C4..+0x2D0 fields.
  - A re-scan including the indexed `[reg+reg*4+0x2a0..0x2d4]` forms and read-modify-write OR/BTS/AND/XOR/ADD found only the constructor as writer.
  - The map's "spies" hint has no mechanism, and the German spy team recruits from nothing.
- **Power cut-off:** output/drain < 1 (`FUN_004555c0`), and output scales with health.
- **Prism support:** 150% / 8 / 45.
- **SEAL stats:** MP5 125 / ROF 10 / range 6; the SEALs guard, except one patrol between waypoints 30 and 31.
- **R-023 "Switch Power House":** a no-op in the original (unattached tag, same house).

**Difficulty differences:**
- The ping and spy-team triggers are Easy/Normal only.
- The northern IFV patrol and the 6-SEAL group are Normal/Hard only.
- The Yuri Drop has Easy, Normal/Hard and Hard variants.

**Trap:** the 36-cell "Player in Base" ring turns every American to Hunt when any Africans unit steps on it, mind-controlled units included. The cells:
- north gates (58-62,53) and (66-67,53);
- x=56, y 57-60;
- x=73-74;
- south (59-61,61-64) and (66-67,66).

**Winning plan (after W6-02-11/12):**
1. At 65 s pull the Yuris to about (36,74).
2. Mind-control the British engineer from the west and capture the Alliance barracks. You get dogs; they belong to Confederation and do not trip the ring.
3. Take the north patrol SEAL from 8 cells and have it C4 the north plant (64,52) from the north, without stepping on (62-66,53). Output goes 1000 to 800.
4. Get output below 575: three plants dead, or two dead plus one more below 87.5% health (for example the (68,66) plant from (67,73) with a controlled IFV).
5. When the Prisms go dark, a Yuri takes the President on his walk (61,65), (63,60), (64,61). `Pres is Captured` wins.

### SOV10 (Weather Control Device)

**Engine:** W6-02-08 is an unfaithful *easy* win: Harriers kill the WCD outside the playfield. With it fixed, the mission is genuine tactical difficulty.
- The east beach (182,117) is inside the 8-cell range of the Prisms at (178,119), (177,110) and (188,112): PrismShot 120 plus support, against an LCRF with 300 HP, light armour.
- There are also 8 pillboxes and 9 Patriots.
- There is no other landing route. The designers' hint is "Build a NUCLEAR MISSILE SILO".

**Difficulty:** the landing-craft harassment (5 waves) and the player's `Kill the Nuke!` Nuke Buster are Normal/Hard only. Easy lacks 2 Intruders and the BG1/BG3 Carrier.

**Winning plan:**
1. Build up behind the opening transports.
2. Thin the east Prisms and pillboxes from the sea (Dreadnoughts outrange them) or with Kirovs.
3. Land Apocalypses plus an engineer wave at (182,117) and capture the Battle Lab (163,97). That runs ResizeView and starts a 300 countdown before the WCD fires and goes to BadGuy3.
4. Have a charged Nuclear Silo ready before the capture and nuke the WCD at (17,95), or capture it with an engineer.
5. Expect the Nuke Buster team at wp95 once the silo exists.

### ALL05 (Tanya death)

**Engine:** W6-02-14. The mission is currently *easier* (no loss when Tanya dies). The winning plan is unchanged (`m-all05-real`).

### Boot Camp (TRN01, TRN02)

- **Progression is faithful:** the TUT1 entry leads to `trn01t.MAP`, then `trn02t.MAP`, then `ALL01t.MAP`. mapsel `[GDI] 1=Trn01, 2=Trn02`, and `FUN_005cf300` reads the numbered stage keys. Probes `trn01-next.play` and `trn02-next.play` show `next_map` and the next loading screen.
- **TRN01:** WON by real clicks in 5598 frames (`02-evidence/d/trn01-next.play`, `trn01_result.png`). The whole Drill Sergeant chain fires.
- **TRN02:** objectives 1-5 work, then W6-02-15 stalls objective 6, and W6-02-14 also breaks the `RedTeamA/B-Destroyed` objective-7 path. It can still be won by destroying the RedTeam base (`PlayerWin` = DestroyedBuildingsAll; proven only with `kill_house`).
- **Winning plan after the fix:** build Power, Refinery, Barracks, then 6 GIs; deploy them at the base entrance; beat the two red waves; destroy the red base.

## REMAINING.md updates suggested

- **R-P1-2:** cause found; becomes W6-02-07.
- **R-P3-4:** cause found; becomes W6-02-14.
- **R-082:** close as FAITHFUL (no Armor/Groundspeed readers; W6-02-04).
- **R-028:** the campaign difficulty binding is now RE-backed (W6-02-01 RE).
- **R-206:**
  - SOV08: "win by destroying the White House".
  - SOV09: split into W6-02-11/12.
- **R-064:** superseded by the W6-02-11 RE.
- **R-023:** no SOV09 impact.
- **R-026:** Boot Camp TRN01 is WON by real orders; TRN02 is blocked by W6-02-15.

## Prioritized fix list (one fix agent)

1. **W6-02-07 + W6-02-08: one `legal_threat` predicate** (LegalTarget, IsInPlayfield, campaign Insignificant/passive-building rejects, per-object playfield flag recomputed on ResizeView). Wire it into `nearest_enemy`, the combat auto-acquire and the SW picker. Unblocks ALL12 and removes the SOV10 shortcut. (M)
2. **W6-02-01 + 02 + 03: per-house handicap table** filled at scenario start: player and PlayerControl houses d, others 2-d. Use it for unit and building ROF and FirePower, and refresh existing units. (S, a few dozen lines.) Then re-run the Easy proofs of ALL09, ALL12, SOV08, SOV09 and SOV10.
3. **W6-02-15: rules id instead of Image in `sync_tracks`.** Unblocks TRN02 and the build events in ALL02, SOV01, SOV03, SOV04 and ALL11, plus team refill from production. (S)
4. **W6-02-14: attach the TeamType tag on recruit/adopt** (ALL05 loss, ALL02, ALL03, ALL04, SOV03, SOV05, TRN02). (S)
5. **W6-02-11 + W6-02-12: guard-range acquisition radius and ImmuneToPsionics.** Unblocks SOV09 by real play. (S each)
6. **W6-02-10: remove the AI Iron Curtain/Chronosphere auto-fire.** (S)
7. **W6-02-16: team full-strength start and Reinforce refill.** (M)
8. Batch: W6-02-09 (SW target set), W6-02-04 (drop build_time_mul), W6-02-05 (team clock seed), W6-02-18 (Patrol attack-move), W6-02-17 (recruit filters), W6-02-13 (3-D defence range), W6-02-19 (airport paradrop check). (S each)

**Acceptance.** Real-order proofs on Easy:
- ALL12 holds the Kremlin with no player action for 25 min after `Begin Prod`;
- SOV10 no-input: the WCD survives until `BL Captured`;
- TRN02 `6Complete(GIBuilt)` fires on the 6th GI;
- ALL05 is LOST when Tanya dies;
- SOV09 SEAL acquisition happens at ≤ 7 cells;
- SOV08 is won by V3 siege per the plan.
