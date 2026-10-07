# Wave-2 gap 03: a full skirmish match, end to end

**What this covers.** Ground units, buildings, the economy and the AI, played as a player would play them.
- Air, naval and superweapons belong to `02-air-naval-sw.md`.
- UI and shell belong to `04-ui-shell.md`.
- Graphics, audio and render performance belong to `05-gfx-audio-perf.md`.

**How it was audited.**
- The tree is master `dfff02a`, built in its own worktree (`D:\ra2-wt\w2-skirmish`, build dir `D:\ra2-wt\w2-skirmish\build`).
- The game was driven through the real `--play` shell → lobby → `GameScene` path, plus `--ai-soak`.
- The original was checked against `rulesmd.ini` (`D:\ra2-re\out\shroud_evidence\rulesmd.ini`), the maps extracted from `MULTI.MIX`, and a private Ghidra copy (`D:\ra2-re\w2-skirmish`).
- Heavy runs were made one at a time. Peak RSS was ≤ 761 MB on every run.

**Reproducing.** Every scratch script, the runner and the dump summariser are in `docs/gap/wave2/03-evidence/scripts/`:
- `gen_allied.sh` / `gen_soviet.sh` generate the active-player matches.
- `run.ps1` runs one executable and records wall time and peak RSS.
- `dumps.py` summarises the `dump` blocks in `play.log`.

**Not re-reported.** These wave-1 items were re-verified as fixed:
- economy wiring, the AI's three layers and the opcode renumbering;
- MCV start and deploy, the full roster, defences firing, crush, garrisons, bridges, and the WON/LOST paths.

---

## 1. Executive summary

The early game is now faithful: build order, 54-step build times, power, prerequisites, AI start and first attacks all check out.

**Every long match fails the same way: the economy dies.**
- Mined-out ore never comes back, because the clone ignores **ore mines** (`SpawnsTiberium` terrain, TIBTRE01-03). All four audited maps rely on them (2-12 mines each).
- Harvesters only scan 32 cells. They then sit in `SeekOre` or `MoveToOre` forever.
- A miner left with no ore parks on its refinery's dock cell. This blocks every full miner behind it in `ReturnToRef`.

**Measured result:**
- Every house's income stops between game minute 8 and 19.
- From then on, AI production halts.
- 1v1 AI-vs-AI Brutal never produces a winner in 40 minutes.
- A 1v3 Brutal match on Arena **freezes completely** from minute ~13 to minute 38: no unit is built or lost by anyone.

**The AI is also much weaker than the original.**
- A Brutal AI fields about 25 units in 20 minutes.
- A Brutal AI has not killed a passive player's lone ConYard after 20 minutes.
- It never uses engineers, spies, transports, deployers or garrisons.
- It cannot recover from losing its ConYard.
- In 3+ player games it has no single target house.

**Several units still do not do their signature job:**
- V3 deals 1 damage per shot and has no rocket.
- Demolition Truck never explodes.
- Dogs need 5 bites.
- Service Depot does nothing.
- Spy cash theft yields ~0.
- Oil derricks never pay.
- Mirage, Prism split, Tesla charge and Boris airstrike are missing.

**Stability is good.**
- No crash, hang or desync in about 4.5 hours of simulated play.
- The same script twice gives the same `play_hash`.
- Memory stays flat.

**Performance** is acceptable on desktop: about 28 ms per logic frame with 226 tanks fighting, against 11 ms idle. But dead units are never removed from `World` vectors, and acquisition is O(U²).

---

## 2. Match logs

"min" is game minutes at 900 logic frames per minute (`--ai-soak`). In `--play` runs, f is the driver frame. The Skirmish slider was at 3 (GameSpeed 3, 20 logic fps, 1.333 logic steps per driver frame; gamespeed.md). The lobby's default map was `hillbtwn.map` (A Hill Between, URBAN) unless another map is named.

### 2.1 Run table

| # | Run | Setup | Length | Wall / peak RSS | Outcome |
|---|---|---|---|---|---|
| M1 | `--ai-soak --ai-vs-ai --diff brutal` | xmp31s2 May Day (SNOW), AI(Allied) vs AI(Soviet, Brutal) | 40 min | 191 s / 634 MB | **No winner.** AI-1 credits frozen at 367 from min 17 to 40; units built frozen at 25 from min 18 to 40; buildings 24 frozen; 17 live teams (most with 0 members); human-slot base 13886/14300 HP |
| M2 | `--play passive_brutal` (x2) | Allied human passive vs Brutal Soviet | 28 min | 151 s / 755 MB | Human LOST at f=12895 (~14.3 min, the passive start force fought back). AI miners: 7, from min 6 all in MoveToOre/SeekOre with 0 bales. Same `play_hash=f71be36061c6a572` both runs |
| M3 | `--play a1v1_brutal` | active Allied (2 refineries, 9 MTNK waves) vs Brutal Soviet | 40 min | 290 s / 700 MB | RUNNING at the end. AI left with 4 buildings and 0 units; player 29 units. Player income: 8.5k → 76k (f=4500-17100, ~4.4k/min with 2 Chrono Miners on gems), then **0** for 22 minutes (both CMIN in SeekOre). AI's 7 War Miners in MoveToOre/SeekOre throughout |
| M4 | `--play s1v1_brutal` | active Soviet (APOC waves) vs Brutal **Allied** | 27 min | 226 s / 693 MB | RUNNING. From min 12 the Allied AI has **7 CMIN and zero combat units**, 4 refineries on 1 power plant, no defences. Its miners never harvest again (5 MoveToOre / 2 SeekOre) |
| M5 | `--play a1v3_brutal_arena` | active Allied vs **3 Brutal** AIs, xarena (URBAN, 4 players, 12 ore mines) | 38 min | 660 s / 739 MB | **Frozen from min 13 to 38.** Player credits 3.70 from f=8100 on. House 1: 6 War Miners full in ReturnToRef + 2 empty in SeekOre for 25 min. Houses 2/3: all miners MoveToOre/SeekOre. Loss counters unchanged from min 18. Player lost only 6 units and 2 buildings to 3 Brutal AIs |
| M6 | `--ai-soak --ai-vs-ai`, hillbtwn | AI vs AI Brutal | 12 min | 107 s / 653 MB | AI-1 credits 1 from min 10 with 4 refineries and 7 miners. The PNG of house 0's base shows no ore anywhere on screen (`03-evidence/aivai_hillbtwn_min12_no_ore.png`) |
| M7 | `--ai-soak` passive, xmp31s2, Easy | lone ConYard vs Soviet Easy | 20 min | 81 s | ConYard 619/1000. AI: 13 units built, builds NATECH + NAIRON + 6 isolated NAWALL posts |
| M8 | same, Normal | — | 20 min | 81 s | ConYard 270/1000. 23 units built |
| M9 | same, Brutal | — | 20 min | 89 s | ConYard **83/1000**, still alive. 25 units built. Defences: NAFLAK×4, NALASR×5, TESLA×4. No Battle Lab |
| M10 | same, Brutal, AI Allied | Soviet human vs Allied AI | 20 min | ~85 s | ConYard 766/1000. AI built 5 Prism Towers (ATESLA), 5 Pillboxes, 5 Patriots, 13 miners, 22 units; no GATECH |
| M11 | `march.play` | 24 MTNK + 5 SREF ordered across hillbtwn to the shrouded enemy base | 4.5k f | 57 s | All 29 arrive in ~1400 f with no stuck unit. They kill 1 building and lose 6, then stand idle beside the enemy base (`march_29_units_arrived_idle.png`) |
| M12 | `perf_fight.play` / `perf_base.play` | 113 MTNK vs 113 HTNK spawned adjacent | 3000 f | 99 s vs 47 s | ~28 ms vs ~11 ms per driver frame (startup ~15 s subtracted; the machine was at ~64 % CPU from other agents). Only 77 of 226 die in 200 s of game time: the fight stalls once the front ranks die (`perf_226_tanks_fight_stalls.png`) |
| M13 | `unitprobe/dog/dtruck/v3.play` | per-unit weapon and ability probes | short | ~40 s each | See §3 |

### 2.2 Timings that match the original

Build times reproduce construction.md §7 exactly, including the 54-step truncation:
- Formula: `t = ftol(Cost × 0.7 × 0.9)`, then 54 steps of `ftol(t/54)`, divided by 1.333 logic steps per driver frame at GameSpeed 3.
- GAPOWR (800): predicted 364 driver frames, observed **364**.
- GAREFN/NAREFN (2000): predicted 931, observed **931**.
- GAPILE/NAHAND (500): predicted 202, observed **202**.
- NAPOWR (600): predicted 282, observed **282**.

Costs, power and prerequisites are read straight from rulesmd. Two prerequisite chains were confirmed in play:
- GATECH is only offered after the country radar (AMRADR for Americans).
- AMCV needs GADEPT.

### 2.3 What long play reveals (timeline, M1 / M3 / M5)

| Game min | What happens |
|---|---|
| 0-3 | Faithful. MCV deploy, power → refinery → barracks → war factory. The AI's first team forms at min 3. |
| 4-9 | AIs expand to 4 refineries and 7-8 miners, and spend down to < 700 credits on base and defences. |
| 6-10 | The fields near the AI bases are mined out (hillbtwn: AI miners at 0 bales from min 6). |
| 9-19 | Income stops house by house: AI-1 hillbtwn at min 10; AI-1 May Day at min 17; player May Day / hillbtwn at min 19; every house on Arena at min 8-9. |
| after | Teams keep being *created* from triggers but never fill (no money). Executing teams hold 2-3 leftover units. Nothing changes for the rest of the match. |

---

## 3. Per-unit behaviour matrix (real play)

**Sources.**
- "Probe" = measured in M13 (`weapon_damage/range/rof` probe keys, units spawned with `cheat spawn` and ordered with real clicks).
- "Code" = traced in the source on dfff02a.
- ✅ works · ◐ partial · ❌ missing/wrong.

### Allied infantry

| Unit | Weapon (probe: dmg / range / ROF) | Ability in real play | Status | Clone evidence | Original |
|---|---|---|---|---|---|
| GI E1 | M60 15 / 4 / 20 | D deploys (sandbag Para), a move order undeploys; the AI never deploys | ✅ (player) | world_abilities.cpp:75-90; world.cpp:799-802 | rulesmd [E1] Deployer/DeployFire |
| Engineer | DefuseKit 1 / 1.5 / 20 | Capture: ◐ ignores `Capturable=` (GAPILL, TESLA, ATESLA, NAMISL, GACSPH, GAGAP are `Capturable=false`). Repair own building ✅. Bridge hut ✅. Defuse Ivan bomb ❌. The AI never builds or uses it ❌ | ◐ | world.cpp:1932-1995 (no `Capturable` read anywhere in engine/); ai_skirmish.inc:22-35 roster | capture.md §A3/§A5; [DefuseKit] Warhead=BombDisarm |
| Attack Dog ADOG / DOG | GoodTeeth 30 / 1.5 / 30 | Kills E2 (125 HP) in about 5 bites (dog.play: 3 kills within 900 f). There is no ParasiteDog instant kill. DetectDisguise is gate-only | ❌ | main.cpp:1062 (no ability profile for ADOG/DOG) | rulesmd ParasiteDog warhead; abilities.md §B2 one-bite kill |
| Spy | MakeupKit | Infiltration: see §3.1. Disguise ❌ (`set_disguise` only from a demo, main.cpp:11432), so enemies shoot spies on sight | ◐ | world.cpp:1579-1611 | capture.md §B/§C |
| Tanya TANY | 125 / 6 / 5 | C4 on buildings ✅. Swim ❌ (no Amphibious domain) | ◐ | world_abilities.cpp:282-296; unit_combat.hpp:21 | [TANY] C4=yes, SpeedType=Amphibious |
| SEAL GHOST | 125 / 6 / 10 | Same as Tanya | ◐ | same | [GHOST] |
| Chrono Legionnaire CLEG | 8 / 5 / 120 | Erase units and buildings ✅. Teleport move ✅ | ✅ | world_abilities.cpp:116-163, 555-615 | abilities.md §C |
| Sniper SNIPE (British) | 125 / 14 / 150 | Plain weapon. Garrison N/A (no `Occupier=` in this rulesmd) | ✅ | world_depth.cpp:45-58 | — |
| Rocketeer JUMPJET | 25 / 5 / 30 | Flies, and is the only skirmish air target | ✅ (note) | main.cpp:797-820 | air agent |

### Allied vehicles

| Unit | Weapon (probe: dmg / range / ROF) | Ability in real play | Status | Clone evidence | Original |
|---|---|---|---|---|---|
| Grizzly MTNK | 65 / 5 / 60 | Crush ✅ | ✅ | world.cpp:3887-3900 | — |
| IFV FV | 25 / 6 / 50 | Passenger swap ◐: only dmg/ROF/range swap (the projectile and AA/AG flags stay HoverMissile). A promotion or crate resets it to HoverMissile. An Engineer passenger's RepairBullet (-50) targets *enemies* | ◐ | world_abilities.cpp:489-504; world_veterancy.cpp:42-60 | abilities.md §G |
| Mirage MGTK | 100 / 7 / 70 | Disguise as tree ❌ (`CanDisguise` never read) | ❌ | no hit in engine/ | [MGTK] CanDisguise=yes |
| Prism Tank SREF | 100 / 10 / 100 | Fires ✅. Beam split ❌ (`ShrapnelWeapon/Count` not parsed) | ◐ | no "Shrapnel" in engine/ or src/ | [LargeCometP] ShrapnelCount=5 |
| Tank Destroyer TNKD (Germans) | 150 / 5 / 70 | Plain | ✅ | — | — |
| Chrono Miner CMIN | — | Teleports to the refinery ✅. Scan radius, parking and lack of ore mines starve it after ~19 min (§4 W2-SK-01..03) | ◐ | harvest.cpp:334-349 | economy.md §3 |
| AMCV | — | Deploy ✅. The AI never builds or redeploys one | ◐ | world.cpp:2523 | — |

### Soviet infantry

| Unit | Weapon (probe: dmg / range / ROF) | Ability in real play | Status | Clone evidence | Original |
|---|---|---|---|---|---|
| Conscript E2 | 15 / 4 / 25 | Garrison ✅ (player only; the AI never garrisons) | ✅ | world_depth.cpp:59-270 | world-depth.md §A |
| Flak Trooper FLAKT | 20 / 5 / 20 | AA/AG weapon choice ✅ | ✅ | weapon_unit.cpp:29-36 | — |
| Tesla Trooper SHK | 50 / 3 / 60 | Charging Tesla Coils (Overpowerable) ❌ | ◐ | defenses.md §2; no "Overpower" in engine/ | [TESLA] Overpowerable=true |
| Crazy Ivan IVAN | 400 / 1.5 / 50 | Bomb units and buildings, timer, chain on death ✅. No input path to bomb own units. Defuse ❌ | ◐ | world_abilities.cpp:305-315, 617-634 | abilities.md §E |
| Desolator DESO (Iraq) | 125 / 6 / 50 | Deploy + radiation ✅ for the player. The AI never deploys ❌. Every rad tick pushes an event that is never drained (W2-SK-14) | ◐ | world_abilities.cpp:91-113, 643-655 | abilities.md §A |
| Boris (YR data) | 65 / 7 / 20 | Airstrike ❌ | ◐ | no "Airstrike" in engine/ | [BORIS] AirstrikeTeam |

### Soviet vehicles

| Unit | Weapon (probe: dmg / range / ROF) | Ability in real play | Status | Clone evidence | Original |
|---|---|---|---|---|---|
| Terror Drone DRON | 50 / 1.83 / 60 | Latch + damage over time ✅. Removal by Service Depot ❌ | ◐ | world_abilities.cpp:335-357, 526-552 | abilities.md §B |
| Rhino HTNK | 90 / 5.75 / 65 | Crush ✅ | ✅ | — | — |
| Flak Track HTK | 25 / 5 / 25 | 5 passengers ✅ | ✅ | cargo.cpp | transport-cargo.md |
| **V3** | **1** / 18 / 150 | **Probe: could not kill one HTNK in 1800 f** (v3.play). `Spawner/Spawns=V3ROCKET` is never read, so there is no rocket and nothing to shoot down | ❌ | no "Spawner" in engine/ or src/ | [V3] Spawns=V3ROCKET |
| Apocalypse APOC | 100 / 5.75 / 80 | Self-heal ✅, AA ✅. Death explosion (`Explodes=yes`) ❌ | ◐ | world_veterancy.cpp:150-159 | veterancy.md V11 |
| Tesla Tank TTNK (Russia) | 135 / 4 / 75 | Plain | ✅ | — | — |
| **Demolition Truck DTRUCK** (Libya) | 300 / 1 / 80 | **Probe: ordered onto 2 HTNK, it is still alive after 400 f.** It fires a 300-dmg "gun" every 80 f and never self-destructs. The AI roster fields it | ❌ | main.cpp:1062 (not in the ability list) | [DTRUCK] Suicide, DeathWeapon=Demobomb |
| War Miner HARV | 20mmRapid | Harvest ✅. Starves (W2-SK-01..03) | ◐ | harvest.cpp | economy.md |
| SMCV | — | As AMCV | ◐ | — | — |

### 3.1 Spy infiltration by building (`infiltrate_purposes_for`, main.cpp:1753-1774)

| Building | Clone | Original | Status |
|---|---|---|---|
| Power plant | 1000-frame blackout; defences go offline. Victim index must be < 3 | blackout | ◐ (houses ≥ 3 cannot be blacked out) |
| Refinery | Takes 50 % of `house_credits_[victim]`. That pool is swept every frame into the player and AI wallets, so the theft is about 0. Only houses 0-1 (world.cpp:1589) | steal half the victim's money | ❌ |
| Barracks / War Factory | A house-wide latch makes **every** later unit a veteran (world.cpp:1602) | veteran units from that factory type only | ❌ |
| Radar | Reshrouds the victim | reshroud | ✅ |
| Battle Lab | Treated as radar. Stolen-tech units are filtered out of the roster (realplay_economy.inc:131-135) | unlocks stolen tech | ❌ |
| Superweapon | Resets the charge | reset | ✅ |
| Non-spyable building | Returns true: the spy is consumed for nothing (`Spyable=` unchecked) | refused | ❌ |

### 3.2 Systems

| System | Status | Note |
|---|---|---|
| Veterancy (XP, ranks, elite weapon, self-heal) | ✅ | world_veterancy.cpp |
| Crates | ✅ | money/unit/heal/reveal/armor/firepower/speed/veteran (crate.hpp) |
| Garrisons | ✅ player / ❌ AI | — |
| Bridges | ✅ | world-depth play script passes |
| Repair / sell | ✅ | — |
| **Service Depot** (GADEPT/NADEPT) | ❌ | Built and acts as a prerequisite, but `UnitRepair` is never read: it does not repair, and does not remove Terror Drones |
| Ore growth + spread | ◐ | Spread only from cells already at max density (ore.cpp step). Growth is RE-FALLBACK |
| **Ore mines (TIBTRE01-03)** | ❌ | W2-SK-01 |
| Oil derricks / tech buildings | ❌ | `register_tech_income` is only called from a demo (main.cpp:11282). The +1000 capture bonus is keyed to house index 2 (`kNeutralHouse`, capture.hpp:15), which is an *AI* in 3+ player games |
| Walls | ◐ | Line-fill and 3× build time ✅. Any warhead damages them. The AI places isolated posts (M7: NAWALL×6) |
| Prism Tower chaining / Tesla overcharge / Gap Generator | ❌ | defenses.md §1-§2; `GapGenerator` never read |

---

## 4. Gap table

**Status values.**
- BROKEN: exists but fails in play.
- WRONG: behaves differently from the original.
- MISSING: not there at all.
- PARTIAL: some of the behaviour is there.
- RISK: a long-run or scale hazard.

**Severity values.**
- BLOCKER: a match cannot be played to a normal end.
- MAJOR: a core mechanic differs from the original.
- MINOR: a detail.

**Effort:** S/M/L.

| ID | Issue | Original (cite) | Clone (file:line / play evidence) | Status | Sev | Fix sketch | Effort | Depends on |
|---|---|---|---|---|---|---|---|---|
| W2-SK-01 | **Ore mines never spawn ore.** Mined-out fields never recover, so every economy dies at min 8-19 | rulesmd `[TIBTRE01..03] SpawnsTiberium=yes` ("Does it spawn growth of Tiberium around it", rulesmd l.27586). Maps carry them: hillbtwn 6, xarena 12, xmp29u2 4, xmp31s2 2 TIBTRE (extracted from MULTI.MIX). gamemd: TerrainType+0x2B1 loaded by `FUN_0071d140`; CellClass `FUN_004837f0` (spread eligibility) skips mine cells. The spawner tick itself is not pinned (RE-Q1) | No `SpawnsTiberium` anywhere in engine/ or src/. Map [Terrain] is dressing only (pres_map.inc:32-56). Play: M1, M3, M5, M6 income → 0 | MISSING | BLOCKER | Parse `SpawnsTiberium` on TerrainTypes and register each mine cell with `OreField`. On the growth clock (or a per-mine timer) seed or raise ore in a random free adjacent cell (radius 1-2), skipping the mine's own cell (FUN_004837f0). The mine must stay impassable. Re-pin econ gates | M | RE-Q1 for exact cadence |
| W2-SK-02 | Harvester long scan is 32 cells instead of 48 | `[General] TiberiumLongScan=48` (rulesmd l.312). Rules+0x177C, loader `FUN_0066cf70`; 0x2000 leptons = 32 cells is only the *default* (`FUN_00665090`) | harvest.hpp:19 `kOreScanRadius = 32`, a constant | WRONG | MAJOR | Read TiberiumLongScan (and TiberiumShortScan=6 for the in-patch dig) from rules | S | — |
| W2-SK-03 | **Dock-cell deadlock.** An empty miner that finds no ore parks on its refinery's dock cell; full miners behind it wait in ReturnToRef forever | economy.md §2: the miner docks on the refinery pad (part of the building), then leaves | harvest.cpp:465-494: after unloading it goes to SeekOre *in place*. The dock is the passable cell south of the foundation (harvest.hpp:39-40), and SeekOre with no target never moves (harvest.cpp:221-237). M5 house 1: 6 full War Miners in ReturnToRef plus 2 empty in SeekOre for 25 min | BROKEN | BLOCKER | After unloading (or when SeekOre fails), move off the dock cell to a free cell near the refinery. Make WaitDock require the dock cell itself to be free. Alternatively make the dock cell part of the foundation footprint | S | — |
| W2-SK-04 | Unreachable nearest-ore livelock | The original's TiberiumLongScan search is path-aware (RE-Q2) | harvest.cpp:230-259 picks the nearest *Chebyshev* ore cell regardless of reachability; world.cpp:847-855 drops a move with no path. The miner returns to SeekOre and picks the same cell every 3 s. M2-M5: 4-6 AI miners per house permanently in MoveToOre/SeekOre | BROKEN | MAJOR | Blacklist the cell for N rescans when the path is empty. Also skip cells occupied by another harvester. Better: flood-fill from the miner on the land grid and take the first ore cell reached | S | — |
| W2-SK-05 | Ordering a ground unit onto an impassable cell does nothing | The original walks to the nearest reachable cell | pathfind.cpp:54 returns {} when the goal is impassable; world.cpp:849 "stay put" | WRONG | MAJOR | Relax the goal to the nearest passable, reachable cell (spiral search on the domain grid) before A* | S | — |
| W2-SK-06 | AI economy and production too weak: Brutal fields ~25 units in 20 min and cannot kill a passive lone ConYard in 20 min | ai-team.md §6: produce for every team shortfall in parallel factories; MultipleFactory applies to all houses (construction.md §7) | One unit intent per frame from the oldest team (ai_team.cpp:428-437). AI `sp={1,1,1,1}`, so no MultipleFactory (world.cpp:2480). Up to 25 defence nodes planned in a row (ai_base.cpp:183-193). M7-M10 | WRONG | MAJOR | Scan all teams per idle factory category. Count the AI's own factories. Interleave defence nodes with the tech tail. Then re-measure against an original-game capture (RE-Q5) | M | W2-SK-01/03 (money) |
| W2-SK-07 | AI cannot recover from losing its ConYard | The original AI rebuys an MCV and redeploys (expected; RE-Q4) | world.cpp:2523 `if (!conyard) continue;`. AMCV/SMCV are absent from the AI roster (ai_skirmish.inc:22-35) | MISSING | MAJOR | Add the MCV to the AI roster. When there is no ConYard and a war factory exists, queue one, then `order_deploy_mcv` near the old base centre | M | RE-Q4 |
| W2-SK-08 | No single EnemyHouse per AI: in 1v3/FFA the gather ring and threat centroid average all enemies | ai-trigger.md deviations (house+0x5600 TargetHouse) | world.cpp:2277-2290, 3404-3416. All waypoints are anchored on the human base (ai_skirmish.inc:136-151). M5: 3 Brutal AIs inflict 6 unit and 2 building losses on the player in 38 min | WRONG | MAJOR | Add `HouseAiRec::enemy_house` (nearest living enemy, re-picked on defeat). Use it for the base view, trigger counts, gather and waypoints | M | — |
| W2-SK-09 | AI never uses engineers, spies or transports. Script actions 8/9/14/43/63/55 are inert. Capture-tech triggers (ct=7) are always false | ai-team.md §3 census; ai-trigger.md §2 | ai_skirmish.inc:15-37 (no ENGINEER/SPY). `parse_aimd` drops whole TaskForces that name them. world.cpp:3397-3403 clears the neutral counts | MISSING | MAJOR | Add them to the roster. Implement 14/43 load/unload on cargo.cpp, 8 unload, 9 deploy (DESO/GI). Engineer capture via `order_capture` for quarry 11 | L | W2-SK-12 |
| W2-SK-10 | AI never deploys (GI/Desolator), never garrisons, has no IsBaseDefense teams | aimd TeamTypes IsBaseDefense; script 9 Deploy | ai_trigger.cpp:117 and ai_team.cpp:357 reject base defence. Script 9 is inert | MISSING | MAJOR | Implement defensive triggers and teams (enemy within N cells of the base) and script 9 | M | — |
| W2-SK-11 | Team targeting splits the team: each member picks its own nearest quarry | ai-team.md §5: one target from the team anchor, path-checked | world.cpp:2971-2976 | WRONG | MINOR | Choose once per team, assign to all members | S | — |
| W2-SK-12 | Engineer captures buildings marked `Capturable=false` | rulesmd GAPILL/TESLA/ATESLA/NAMISL/GACSPH/GAGAP `Capturable=false`; capture.md §A3 | world.cpp:1932-1995 (no Capturable read) | WRONG | MAJOR | Parse `Capturable`; refuse with the no-entry cursor | S | — |
| W2-SK-13 | V3 does 1 damage and has no rocket | `[V3] Spawns=V3ROCKET`, `[V3Launcher] Spawner=yes`; the rocket is an interceptable missile unit | v3.play: weapon_damage(V3)=1; could not kill one HTNK in 1800 f | BROKEN | MAJOR | Spawner: fire launches a V3ROCKET missile (its own HP, MissileSpawn locomotion) carrying [V3Warhead]. Reuse the projectile system (p2) with a killable target. Coordinate with the air/SW agent (Dreadnought/Boomer share Spawner) | M | air agent |
| W2-SK-14 | Ability and garrison event vectors grow without bound in real play | — | `ability_events_` pushes per Desolator rad tick and per Terror Drone tick (world_abilities.cpp:548, 652). `take_ability_events` and `take_garrison_events` are only called by probes and gates (main.cpp:14837+), never by GameScene. They are also serialised into saves (save_fields.hpp:173, 188) | RISK | MAJOR | Drain both every frame in `GameScene::update` (feed audio and FX from them), or cap them. Remove them from the save, or clear them before saving | S | — |
| W2-SK-15 | Dead units and buildings are never removed from `World` vectors; O(U²) acquisition and splash | — | No `units_.erase` / `buildings_.erase` / compaction (world*.cpp). combat.cpp:515 auto-acquire, :240 splash. M12: ~28 ms vs ~11 ms per driver frame at 226 vs 8 units | RISK | MAJOR (Android) | Spatial hash (cell buckets) for acquisition and splash. Generation index + free list for dead slots | M | — |
| W2-SK-16 | Unbounded A*: whole-grid flood on unreachable goals, 3 full-grid allocations per call, blocked repath copies the grid every 0.3 s | — | pathfind.cpp:41-140; world.cpp:3999-4001 | RISK | MAJOR (Android) | Expansion budget, preallocated scratch arrays with a generation stamp, goal relaxation (W2-SK-05), shared occupancy-aware grid per frame | M | W2-SK-05 |
| W2-SK-17 | Demolition Truck never explodes | `[DTRUCK] Suicide=yes`, `DeathWeapon=Demobomb`, `Explodes=yes` | dtruck.play: alive after 400 f next to 2 HTNK. main.cpp:1062: DTRUCK is not in the ability list | BROKEN | MAJOR | Add DTRUCK (and APOC `Explodes`) to `wire_abilities`; Suicide on fire, DeathWeapon blast | S | — |
| W2-SK-18 | Attack Dog is not a one-bite infantry killer | ParasiteDog warhead (abilities.md §B2) | dog.play: 5 bites per E2. main.cpp:1062 has no ADOG/DOG | WRONG | MAJOR | Register ADOG/DOG; Parasite instant-kill vs infantry, cannot target vehicles | S | — |
| W2-SK-19 | Service Depot does nothing | `[GADEPT]/[NADEPT] UnitRepair=yes`; depot repair also kills an attached Terror Drone | no `UnitRepair` read in engine/ | MISSING | MAJOR | Units ordered onto the depot dock repair at RepairStep/RepairRate cost; eject drone parasites | M | — |
| W2-SK-20 | Spy refinery theft yields ~0 and works only for houses 0-1; blackout only for victims < 3; factory spy makes *all* units veteran; Battle Lab = reshroud; non-spyable targets consume the spy | capture.md §B/§C | world.cpp:1588-1611; main.cpp:1753-1774 | BROKEN | MAJOR | Steal from the real wallet (`ai_credits` / player credits) for any house; per-category veteran flag; Battle Lab → stolen-tech flags and allow `RequiresStolen*Tech` units; check `Spyable=` | M | — |
| W2-SK-21 | Oil derricks never pay; capture bonus keyed to house index 2 | `[CAOILD]` ProduceCashAmount/Delay; capture.md | `register_tech_income` only in a demo (main.cpp:11282). `kNeutralHouse=2` (capture.hpp:15), but the lobby appends the neutral house last (skirmish_lobby.cpp:369) | BROKEN | MAJOR | Register every map CAOILD / tech building at launch. Replace `kNeutralHouse` with `world.neutral_house()` everywhere (spy, capture, credits) | S | — |
| W2-SK-22 | Missing signature mechanics: Mirage disguise, Prism Tank split, Prism Tower chain, Tesla Coil charging, Boris airstrike, Gap Generator, Tanya/SEAL swim, Engineer defuse, Spy disguise | rulesmd keys `CanDisguise`, `ShrapnelWeapon/Count`, `Overpowerable`, `AirstrikeTeam`, `GapGenerator`, `SpeedType=Amphibious`, `BombDisarm` | none of these keys is read in engine/ | MISSING | MAJOR (Mirage, Prism, Spy) / MINOR (rest) | One feature each; start with Spy disguise (code exists, only needs wiring), Prism split, Mirage | M-L | — |
| W2-SK-23 | AI walls are isolated posts | ai-base.md §3 `FUN_0050c160` rings each defence's perimeter | ai_base.cpp:188-189 spacing probe applies to Wall=yes. M7: NAWALL×6 | WRONG | MINOR | Place walls on the defence's perimeter ring, without spacing | S | — |
| W2-SK-24 | Teams created from triggers never fill when money is out, and pile up in `teams_live` (17 at min 40) | ai-team.md §2 teams time out / disband | M1 `[ai-soak] team ... state=0 members=0` ×15 | PARTIAL | MINOR | Disband Forming teams after a timeout; do not count them toward TotalAITeamCap | S | — |
| W2-SK-25 | Large fights stall: back ranks never close in | RE-Q3 (guard-mode acquisition range) | M12: 77 of 226 die in 200 s at point-blank spawn, then the two blocks idle out of range | WRONG? | MINOR | After RE-Q3: acquire within the guard range (Sight/GuardRange), and react to friendly units under fire | S | RE-Q3 |
| W2-SK-26 | IFV passenger weapon reverts on promotion or crate; Engineer RepairBullet targets enemies; projectile and AA flags don't swap | abilities.md §G | world_abilities.cpp:489-504; world_veterancy.cpp:42-60 | BROKEN | MINOR | Re-apply `ifv_current` after `refresh_vet_stats`; negative-damage weapons target friendlies | S | — |
| W2-SK-27 | Harness: `--ai-soak` `first_human_damage_min` is always -1, because the base HP baseline is taken before the MCV deploys | — | ai_soak.inc:69 (`hp0` at frame 0) | BROKEN (tool) | MINOR | Take the baseline after the ConYard is built | S | — |
| W2-SK-28 | Hotkeys `CombatantSelect` (P), `Follow` (F), `VeterancyNav`, `PlaceBeacon`, `AllToCheer`, `PlanningMode` are bound but unhandled (seen while scripting army selection) | KeyboardMD.ini [Hotkey] | game_scene_controls.inc:144-177 has no case for them | MISSING | MINOR | Cross-ref the UI agent (04) | S | — |

---

## 5. Prioritized fix plan

1. **Make long matches possible (BLOCKERs, ~1 phase).**
   - W2-SK-03 dock deadlock: harvest.cpp 465-494, 221-237, plus WaitDock.
   - W2-SK-02 scan radius: harvest.hpp:19.
   - W2-SK-04 unreachable-ore blacklist: harvest.cpp 230-259.
   - W2-SK-01 ore mines: new `OreField` mine list; parse in `wire_economy` / pres_map.inc; step in ore.cpp.
   - Acceptance: `--ai-soak --ai-vs-ai --minutes 40` on xmp31s2 and hillbtwn with AI credits never stuck for > 3 min; the 1v3 Arena script ends with a winner, or at least with income every minute.
   - Gates `econ`, `skirmish`, `maps` will move: re-pin with these IDs.
2. **Signature-unit fixes (S each, parallel).**
   - W2-SK-17 DTRUCK, W2-SK-18 dogs, W2-SK-21 derricks + neutral index, W2-SK-12 Capturable, W2-SK-26 IFV.
   - Hotspots: `wire_abilities` main.cpp:1060-1073, world_abilities.cpp, capture.hpp, world.cpp:1588-1611 / 1932-1995.
3. **Leaks and scale.**
   - W2-SK-14 event drains: GameScene::update.
   - W2-SK-05 + W2-SK-16 pathfinding: pathfind.cpp, world.cpp:847, 3999.
   - W2-SK-15 spatial hash: combat.cpp:240, 515; then a dead-slot compaction pass.
   - Measure with `perf_fight.play` and a 300-unit variant; target < 10 ms per frame on desktop.
4. **AI strength.**
   - W2-SK-08 EnemyHouse.
   - W2-SK-06 production parallelism + MultipleFactory: ai_team.cpp:389-439, world.cpp:2480, 3174-3256; ai_base.cpp:183-193.
   - W2-SK-07 MCV rebuy.
   - W2-SK-10 defence teams + deploy.
   - W2-SK-24 team timeout.
   - Then W2-SK-09 engineers/transports (largest).
   - Hotspots: world.cpp 2227-3470, ai_team.cpp, ai_trigger.cpp, ai_skirmish.inc.
5. **Missing mechanics.**
   - W2-SK-13 V3 rocket (with the air agent's Spawner work).
   - W2-SK-19 Service Depot.
   - W2-SK-20 spy effects.
   - W2-SK-22, in this order: Spy disguise, Prism split, Mirage, Prism Tower chain, Tesla charge, Gap Generator, swim, defuse, Boris.
6. **Polish:** W2-SK-23 walls, W2-SK-25 (after RE-Q3), W2-SK-27 / 28.

---

## 6. Open RE questions

| # | Question | Where to look |
|---|---|---|
| RE-Q1 | The ore-mine spawner: which tick reads TerrainType+0x2B1 (`SpawnsTiberium`) to seed ore, its cadence and radius, and whether it is tied to `AnimationProbability`/animation end. | The readers found are `FUN_004837f0` (cell spread eligibility, excludes mine cells), `FUN_00489190`, `FUN_005f46d0`, `FUN_00700da0`, `FUN_0075e150` (calls the empty `FUN_00489180`). The TerrainClass AI vtable slot (TerrainType ctor `FUN_0071cd20`, vtable `PTR_FUN_007f4458`) was not decompiled. `0x71ad75` (not in a defined function) also calls `FUN_00489180`. Scripts: `03-evidence/scripts/w2sk_ore.py`, `w2sk_dec.py` |
| RE-Q2 | Harvester ore search: is the TiberiumLongScan pick path-aware or reachability-filtered, and what happens when nothing is found (return to the refinery and idle off-pad?). | Readers of Rules+0x177C/0x1778 use non-ESI addressing (only the loader and the defaults writer were found); search MissionClass Harvest (UnitClass::Mission_Harvest) |
| RE-Q3 | Guard-mode acquisition range for idle ground units (weapon range vs Sight vs GuardRange), and whether units react to nearby friendlies under fire. | TechnoClass::Greatest_Threat / Mission_Guard |
| RE-Q4 | Does the skirmish AI rebuy and redeploy an MCV after losing its ConYard (IQ-gated?), and where. | HouseClass AI tick `FUN_004fd320` region, base planner `FUN_0050a3e0` |
| RE-Q5 | Reference pacing: AI units fielded per minute and the time to kill a passive player, per difficulty, measured in the original (needs a capture on a stock map such as xmp31s2). | gamemd run plus screen capture; no decompile needed |
| RE-Q6 | Refinery dock geometry: GAREFN/NAREFN have no DockingOffset; is the dock cell inside the foundation (so a parked miner never blocks)? | `FUN_00740780` drive-to-dock (economy.md §2) |
| RE-Q7 | ParasiteDog: is a bite an instant kill for every infantry, and is it immune vs vehicles? | Warhead `ParasiteDog` / InfantryClass bite |
| RE-Q8 | `Spawner` (V3, Dreadnought, Boomer): spawn manager, rocket HP and interception. | Shared with the air/SW agent |

---

## 7. Fix status (branch `w2fix-skirmish`)

The rows below were fixed on `w2fix-skirmish` (merged with master up to the
w2fix-android merge). Save format **v12**. The evidence is real play: the new
`scripts/play/economy-60min.play` and `scripts/play/unit-specials.play`, plus
`--ai-soak` runs (the soak gained `--kill-ai-conyard-at MIN`, per-house
unit/building/wallet/miner columns, ore/mine counters, special-infantry and
tech-capture witnesses, and a team dump for every AI house).

### 7.1 RE questions resolved

| # | Answer | Evidence |
|---|---|---|
| RE-Q1 | **Pinned.** `TerrainClass::AI` (0x71B9D0): a mine that is not animating starts its animation when `abs(Random)%1000000 * 1e-6 < AnimationProbability` (0.003), advances one stage every `AnimationRate` (3) frames, and when the stage reaches half the SHP frame count stops and calls `CellClass` forced spread `FUN_00483690(cell, 1)`. That picks a random start direction 0..7 and gives the first of the 8 neighbours (DAT_0089E5F8 order) that can hold tiberium (`FUN_004837F0`: land allows it, no overlay, not a mine, no building) the mine cell's tiberium type (none → type 0) at OverlayData **3** (`FUN_004870A0(tib, 3)`). The regular growth/spread clocks were pinned at the same time: `FUN_00721EE0`/`FUN_007221A0` grow `abs(Random) % clamp(ftol(H*GrowthPercentage),5,50) + 1` cells every `Growth=` frames (2200; ×0.3 under a Rules flag that is off), `FUN_00721450`/`FUN_007216E0` spread `abs(Random) % clamp(ftol(H*SpreadPercentage),5,25) + 1` heap cells every `Spread=` frames, each seeding one neighbour at density 3 (`FUN_004835A0`: density > tiberium index / 2, no object on the cell). | Ghidra on gamemd (private copy `D:\ra2-re\w2-skirmish`, scripts `D:\ra2-re\scripts\w2fix_dec.py` / `w2fix_dis.py`); constants 0x7E4138 = 0.3, 0x7EE918 = 1e-6 read from the exe |
| RE-Q2 | [RE-FALLBACK] RA1 `FootClass::Goto_Tiberium` / `Mission_Harvest`: short scan, then long scan, cells in the miner's movement zone with no other techno on them; nothing found + a load → return to the refinery. Implemented as a zone flood + occupant filter. | — |
| RE-Q3 | [RE-FALLBACK] Idle units acquire within max(weapon range, Sight) and close in inside a Sight-based leash. The shroud test in the auto-acquire scan was kept (shroud.md §6, `--demo-shroud` witness); the acquisition radius never exceeds the unit's own Sight, so it is moot in practice. The gfx agent reads `FUN_006F71D0` as having no shroud test; that conflict with shroud.md §6 is left open. | perf_fight: 164 of 226 tanks die (was 77) |
| RE-Q4 | **Answered from the data.** The MCVs need a Service Depot (`[AMCV]`/`[SMCV] Prerequisite=…,GADEPT/NADEPT`), and both depots are `AIBuildThis=no`. So the stock AI cannot rebuy an MCV unless it has captured a depot. The clone now rebuys and redeploys exactly when the prerequisites stand. | `--ai-soak --kill-ai-conyard-at 8`: no rebuy, as in the original |
| RE-Q6 | [RE-FALLBACK] The dock stays south of the foundation. The deadlock is solved by the artmd `QueueingCell=4,1` queue (below). | — |
| RE-Q7 | Confirmed by the existing P4 parasite code: `ParasiteDog` (Parasite=yes, Verses 0% vs vehicles) instant-kills any infantry. | unit-specials.play |
| RE-Q5, RE-Q8 | Open (RE-Q5 needs an original-game capture; RE-Q8 is shared with the air agent). | — |

### 7.2 Row status

| ID | Status | What changed | Evidence |
|---|---|---|---|
| W2-SK-01 | **FIXED** | `OreField` ore mines (registered from the map `[Terrain]`, SHP frames/2), the original growth/spread clocks, save v12 | economy-60min.play (hillbtwn, 6 mines): `mine_spawned` 4 → 86 → 227 → 369 → 499 at min 0/15/30/45/60, ore never runs out (`ore_cells` 24 / 35 at min 30 / 60), AI refinery income 61.6k / 84.8k / 92.6k / 99.8k at min 15/30/45/60 |
| W2-SK-02 | **FIXED** | `TiberiumLongScan=48` / `TiberiumShortScan=6` from rules | unit tests + soaks |
| W2-SK-03 | **FIXED** | Returning miners queue at the QueueingCell while the pad is taken; an empty idle miner leaves the dock area | hillbtwn AI-vs-AI: before, 6 full miners stuck in ReturnToRef behind an empty miner on the pad (M5 pattern); after, no stuck queue in 40 min |
| W2-SK-04 | **FIXED** | Zone- and occupant-filtered ore search, failed-cell skip, a part-loaded miner banks its load | test_harvest |
| W2-SK-05 | **FIXED** | `find_path_nearest`: an impassable goal is relaxed to the nearest passable cell; an unreachable goal ends at the nearest reached cell (Land domain; ships keep the exact search) | test_pathfind |
| W2-SK-06 | **FIXED** (RE-Q5 pacing still open) | Every idle factory category produces the type most owed by all Recruiting/Gathering teams; MultipleFactory for the AI; core-structure money hold; teams muster at the own base until filled or 60 s; gather radius scales with team size | Brutal Soviet vs passive (xmp31s2): before, ConYard 83/1000 alive at min 20 with 25 units built; after, **player defeated at min 15**, 45 units built, NATECH + Iron Curtain + Nuke built and fired by min 20. Allied Brutal: 19 → 69 units in 20 min |
| W2-SK-07 | **FIXED** (data-faithful) | MCVs in the AI roster; with no ConYard and a war factory it builds and unfolds one when it can; dead plan nodes re-link | RE-Q4 |
| W2-SK-08 | **FIXED** | `HouseAiRec::enemy_house` (nearest living enemy, re-picked on defeat) drives the threat view, trigger counts (plus the real neutral house for ct=7) and quarry picks | 1v3 Arena (below) |
| W2-SK-09 | **FIXED** | ENGINEER / SENGINEER / SPY in the AI roster; engineers capture and spies infiltrate the 46/47 target; actions 14 / 43 / 8 (load, wait-loaded, unload) | `--ai-soak --map hillbtwn.map`: the AI captures 3 oil derricks by min 20 |
| W2-SK-10 | **PARTIAL** | Action 9 deploy; base-defence response (idle un-teamed units engage enemies within 12 cells of the base). The AI still never garrisons | — |
| W2-SK-11 | **FIXED** | One AttackQuarry target per team, picked from the lead member | — |
| W2-SK-12 | **FIXED** | `Capturable=` honoured; the click on an enemy Capturable=false structure does nothing | unit-specials.play: NAPOWR captured; Gap Generator refused, engineer kept |
| W2-SK-13 | **PARTIAL** | The V3 shot carries `V3RocketDamage=200` on `V3Warhead`. There is no interceptable 50-HP rocket and no MinimumRange | unit-specials.play: a Rhino killed in 229 frames (before: 1 damage per shot, alive after 1800 frames) |
| W2-SK-14 | **FIXED** | GameScene drains the ability and garrison events every frame | — |
| W2-SK-15 | **FIXED** (slots kept by design) | 8×8-cell buckets for auto-acquire (same tie rule: perf_fight `play_hash` unchanged by the bucketing alone); dead slots release their path/cargo storage. Indices stay stable because every target, team member, cargo slot, parasite and projectile owner stores a unit index | perf_fight 226 tanks: 101 s → 59 s wall (perf_base 61 s) before the pursuit fix; with pursuit (W2-SK-25), 63 s vs perf_base 41 s for 3.5× more combat |
| W2-SK-16 | **FIXED** | Generation-stamped A* scratch (no per-call grid allocations); blocked re-path budget of 4000 expansions; nearest-search budget of 20000 | — |
| W2-SK-17 | **FIXED** | DTRUCK (Suicide Demobomb + DeathWeapon) and APOC (Explodes) profiles | unit-specials.play |
| W2-SK-18 | **FIXED** | DOG/ADOG profiles: one bite kills any infantry | unit-specials.play: E2 dead 14 frames after the order |
| W2-SK-19 | **FIXED** | Service Depot: a stopped damaged vehicle beside an own UnitRepair depot heals RepairStep on the repair phase at repair_step_cost; a riding drone dies | unit-specials.play: 200 → 400 HP |
| W2-SK-20 | **PARTIAL** | Theft from the real wallet of any house; blackout for any house; factory veterancy per factory category; Spyable=no refuses the spy. Battle Lab stolen tech is still not modelled (needs roster/sidebar plumbing) | unit-specials.play: player credits 10450 → 14564 |
| W2-SK-21 | **FIXED** | Map tech buildings register their income; the real neutral house pays nobody; any capturing house gets the startup bonus | soak: AI-owned CAOILD count rises |
| W2-SK-22 | **PARTIAL** | Prism Tank split (5 CometFragments); Mirage disguise-when-still (no enemy auto-acquire; it is still drawn as the tank); Spy disguise (player MakeupKit order, AI spies launch disguised). Still open: Prism Tower chain, Tesla charging, Gap Generator, Boris airstrike, swim, defuse | unit-specials.play |
| W2-SK-23 | **FIXED** | AI walls take the perimeter ring of an own defence | — |
| W2-SK-24 | **FIXED** | Skirmish teams dissolve unfilled after DissolveUnfilledTeamDelay (skirmish is a multiplayer-type session) | — |
| W2-SK-25 | **FIXED** (RE-FALLBACK) | Guard-range acquisition and pursuit | perf_fight resolves |
| W2-SK-26 | **PARTIAL** | The IFV passenger weapon survives promotion and crates. RepairBullet targeting and the AA/projectile swap are open | — |
| W2-SK-27 | **FIXED** | The soak's base-HP baseline is re-based while the base grows | `first_human_damage_min` now reports (7.3–8.5) |
| W2-SK-28 | Not mine | UI agent | — |

### 7.3 Long-match results after the fix

| Run | Before (§2) | After |
|---|---|---|
| AI-vs-AI Brutal, hillbtwn, 40 min | M6: AI-1 at 1 credit from min 10, no ore on screen | **Winner at ~min 22** (house 0 defeated); income all match |
| AI-vs-AI, May Day (2 mines), 40 min | M1: no winner, credits frozen from min 17 | No winner: both bases are heavily walled and defended. Mines keep spawning (140 cells by min 40), but 2 mines cannot feed 16 miners |
| 1v3 Brutal, Arena, scripted active player | M5: frozen from min 13 to 38; the player lost 6 units | The player is overrun and **LOST at ~min 14** |
| Passive vs Brutal Soviet, xmp31s2, 20 min | M9: ConYard 83/1000 alive | Defeated at min 15 |
