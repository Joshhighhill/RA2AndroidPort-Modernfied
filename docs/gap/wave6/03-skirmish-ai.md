# Wave-6 gap 03: skirmish against the AI, as a human plays it

**Scope.** Full skirmish games against the computer: base building, economy, attack waves, defence, superweapons, naval and air use, capture, repair and rebuild, difficulty, AI-vs-AI, and the end of the game (win, loss, score screen). Only new or still-open gaps are listed. Rows that are already tracked are cited by ID (REMAINING.md `R-`, wave2 `W2-SK-`).

**How it was audited.**
- Tree: `e2b569e`, in worktree `D:\ra2-wt\w6-skirmish-ai`, with its own Release build (`build/`).
- Driven through the real scene tree:
  - `--ai-soak` (the real `GameScene` with a passive human, 1v1);
  - `--play` scripts through shell → lobby → `GameScene`, for 1v1 and 1v3 with a passive or scripted "simple" human.
- Original game: a private Ghidra copy `D:\ra2-re\w6-skirmish-ai` (gamemd.exe), plus `rulesmd.ini` and `aimd.ini` from the install.
- Heavy runs were made one at a time. Peak RSS was ≤ 340 MB.

**Reproducing.** Everything is in `03-evidence/`:
- `scripts/`: the generators `gen_passive.py` and `gen_active.py`, the generated `.play` files, `run.ps1` (wall time and peak RSS), `dumps.py`, `walls_alive.play`, and the Ghidra scripts `w6sk_str.py` / `w6sk_disp.py`.
- `logs/`: soak logs, per-run dump summaries, superweapon EVA timelines, and Ghidra decompiles (`ghidra_dec*.txt`).
- `shots/`: screenshots.

**Time base.** Soak "min" means 900 frames, i.e. game minutes at 15 logic fps. `--play` "f" means driver frames at GameSpeed 3. The `*_dumps.txt` "min" is f/900.

---

## 1. Executive summary

The AI now builds a base, harvests, attacks, captures oil, fires superweapons and kills a passive player. Its early game looks right. Long games show four classes of problem.

1. **The game does not end properly.**
   - **Walls count as buildings for victory.** An AI left with one wall segment is never defeated (W6-03-01, probe `walls_alive`). A Brutal Allied AI builds 25 of them.
   - **1v3 games stall for 12–25+ minutes.** The human ends up with 1–4 buildings and no units, and three Brutal AIs never finish them or each other (W6-03-02). The player can neither win nor lose and has to quit.
2. **The AI picks and keeps the wrong enemy.**
   - It takes the nearest house and keeps it until that house is defeated. There is no "anger" from being attacked, so it never retaliates against the player who is hitting it (W6-03-04).
   - Superweapons ignore the designated enemy altogether (W6-03-05).
3. **Money is wasted.**
   - Allied AIs bank 13–94k credits mid-game while fielding about 25 mostly-infantry units (W6-03-07).
   - The Brutal/Medium income bonus `AIVirtualPurifiers` (+100%/+50%) is missing (W6-03-03).
   - No AI builds a single base defence before minute 18–21 (W6-03-06).
4. **Superweapon use is not the original's.**
   - Iron Curtain and Chronosphere are fired by an invented house-level rule. In the original they are fired only by team scripts (55/57), which are inert in skirmish, and by triggers weighted 5000, which never fire (W6-03-05).

Stability is fine: about 7 hours of simulated play produced no crash, hang or runaway memory.

---

## 2. Match logs

| # | Run | Setup | Length (wall / RSS) | Outcome |
|---|---|---|---|---|
| S1 | `--ai-soak --diff brutal --map xmp31s2` | passive Allied vs Brutal Soviet, May Day | 30 min (71 s / 245 MB) | Human dead at min 17. First damage 6.4. 56 units built. The start infantry are garrisoned from min 1 (`ai_aboard=9`, later 15). Iron Curtain (min 16) and nuke (min 20) fired. First base defence (NALASR) at **min 20** |
| S2 | same, `--ai-side allied` | passive Soviet vs Brutal Allied | 30 min | Human dead at min 14.5. Credits **13.6k at min 12, 18k at min 15, 42k at min 21**. 1 GAWEAP and 1 GAPILE all game. 8 Chrono Miners all in SeekOre from min 21 |
| S3 | `--diff easy --map hillbtwn` | passive vs Easy Soviet | 30 min | Human dead at min 15, first damage 6.7. 28 units. First defence at min 21 |
| S4–S7 | Easy/Normal/Brutal × hillbtwn/May Day, 20 min | passive vs Soviet | 20 min each | See the §4 difficulty table: Easy kills faster than Normal on hillbtwn |
| S8 | `--ai-vs-ai --diff brutal --map hillbtwn` | Allied Normal (slot 0) vs Soviet Brutal | 40 min | Soviet ConYard lost at about min 20; Allied wins by min 39. Allied banked **24–39k credits from min 12** with 9–17 units and up to 64 buildings |
| S9 | `--diff brutal --map xmp10s4` (Depth Charge) | passive vs Brutal Soviet | 30 min | Naval Yard at min 9, first ships at min 24 (3). Human dead at min 17 |
| P1 | `p1v3_brutal_arena.play` | passive Allied vs 3 Brutal AIs, Arena | 30 min (124 s) | Human LOST at f=15551 (~17 min). The AIs lost 23–33 units each, mostly to each other |
| P2 | `a1v1_medium.play` (default map) | "simple" Allied (12-building base, Grizzly/Prism waves) vs Medium Soviet | 28 min (237 s) | Human LOST at about min 23. A nuke at f=19603 killed 24 massed units; Iron Curtain ×3. Score screen shot |
| P3 | `a1v3_brutal_arena.play` | the same simple Allied vs 3 Brutal AIs | 38 min (251 s) | **Stall from min ~10 to 38:** the human had 3 → 1 buildings and 0 units, never defeated. AI 1 and AI 2 lost their ConYard and factories; **AI 2 banked 50k → 94k**. AI 3 had 19–31 combat units and never finished anyone |
| P4 | `a1v3_brutal_arena_reveal.play` | P3 plus `cheat reveal` and AI-base shots | 28 min (307 s) | Same picture: the human has 4 buildings and 0 units from min 16 to 28. The Allied AI has 41k credits, 25 GAWALL, 8 ATESLA, 9 GAPILL, 8 NASAM, 10 GAPOWR, and an army of 13 GI, 8 GGI and 1 Grizzly |
| P5 | `walls_alive.play` | kill house 1, leave it one NAWALL | 1.5k f (12 s) | `buildings(1)==1`, `units(1)==0`, `world_outcome==RUNNING`, `defeated(1)==0`. **Not won** |

---

## 3. Gap table

**Severity:**
- BLOCKER: a match cannot reach a normal end.
- MAJOR: a core behaviour differs from the original and a player notices.
- MINOR: a detail.

**Effort:** S/M/L.

### W6-03-01 — Walls (and other insignificant structures) keep a defeated house alive
- **What the player sees.**
  - The player razes every real AI building and nothing happens: no "You are victorious".
  - The AI stays "alive" until every wall segment is found and shot. A Brutal Allied AI plans 25 GAWALL (P4); Easy builds 6 NAWALL (S3).
  - The same applies the other way round: a human with one wall left is not defeated.
- **Evidence.**
  - `walls_alive.play` → `logs/walls_alive_result.txt`: `world_outcome="RUNNING"`, `defeated(1)=0` with only one NAWALL left; shot `shots/walls_alive_wall_only.jpg`.
  - `engine/world.cpp:535-537,553` counts every live `Building`, walls included.
  - The same "wall is a building" view leaks into AI targeting. `quarry_building_match` (`engine/world.cpp:3327-3338`) returns true for any building on quarry 1/2, and `ai_quarry_exists` (`engine/world.cpp:3389-3398`) keeps an "attack buildings" script line alive while a wall stands. AI teams therefore grind on walls (contributes to W6-03-02).
- **Original.**
  - Wall-type buildings are overlays once placed: `[OverlayTypes] 3=GAWALL`, `27=NAWALL` in rulesmd. They are not BuildingClass objects, so the ShortGame "last building" count and the TeamClass quarry scans never see them.
  - `[NAWALL]`/`[GAWALL]` also carry `Insignificant=yes`.
- **Severity / effort.** MAJOR (end-of-game) / S.
- **Fix location.**
  - `World::step_victory`: skip `Wall=yes`, and also `Insignificant=yes` types (the campaign already reads that flag, `engine/campaign_runtime.inc:961`).
  - `quarry_building_match` / `ai_quarry_exists` / the superweapon scorer: skip walls.
- **RE-OPEN.** Whether a garrisoned civilian building, which becomes the occupier's, keeps a house alive in the original. The AI garrisons civilian buildings from minute 1 (W6-03-09), so after the fix this is the next thing that could keep a match running.

### W6-03-02 — 1v3 end-game stall: nobody finishes anybody
- **What the player sees (P3).**
  - By min 10 the simple player has lost its army and its ConYard.
  - For the next **28 minutes** the screen shows 1–3 surviving buildings (a power plant at 492 HP for the last 12 minutes), 0 units and 10,800 frozen credits.
  - The game never ends; the player must quit.
  - P4 reproduces it (4 buildings, 0 units, min 16 → 28).
- **State at the end of P3** (`logs/a1v3_brutal_arena_final_dump.txt`):

  | House | Buildings | Factories | Credits | Army |
  |---|---|---|---|---|
  | AI 1 | 16 | no ConYard, no factory | 12.8k | 9 E2 |
  | AI 2 | 25 | no ConYard, no factory | **93.8k** | 10 GI, 2 GGI, 6 Chrono Miners |
  | AI 3 | 38 (NACNST, NAWEAP, NAHAND, NAMISL) | intact | 1.3k | 31 combat units, 10 miners |

  - Building losses over the last 16 minutes:
    - human 7 → 9;
    - AI 1 3 → 3;
    - AI 2 1 → 4;
    - AI 3 5 → 5.
  - Throughout, AI 3 fires its nuke and Iron Curtain (`logs/a1v3_brutal_arena_sw_eva.txt`).
- **Causes**, each traced to code or RE:
  - (a) Enemy choice is nearest-and-sticky and never switches on damage (W6-03-04). An AI whose "enemy" is a fortified neighbour never turns on the crippled human.
  - (b) Teams keep "attack buildings" lines alive on walls and grind on 25-wall fortresses (W6-03-01).
  - (c) Two of the three AIs are left without ConYard or factories, with money but nothing to build. The MCV re-buy is data-blocked: `GADEPT` has `AIBuildThis=no` (W2-SK-07/RE-Q4).
- **Original.**
  - Anger-driven enemy retargeting. `TechnoClass::ReceiveDamage` (`FUN_00700da0` @0x7012FD, `logs/ghidra_dec5.txt`) adds anger toward the attacker's house through `FUN_005045b0`, and the enemy is then the house with the highest anger (house+0x5600).
  - [RE-OPEN] `Paranoid=yes` → `FUN_00501460` allies all computer houses against the humans (see RE-Q W6-a). If that is active in skirmish, the three AIs would not spend the game fighting each other at all.
- **Severity / effort.** MAJOR (bordering on BLOCKER for 1v3 / FFA) / M, mostly delivered by W6-03-01 + W6-03-04.
- **Acceptance.** A 1v3 Brutal match with the simple scripted player ends WON or LOST before min 40. This needs a new soak or play gate with an `outcome != RUNNING` assert.

### W6-03-03 — `AIVirtualPurifiers` (the difficulty income bonus) is not applied
- **What the player sees.**
  - A Brutal AI should earn **double**, and a Medium AI 1.5×, from every ore unload in skirmish. In the clone it earns exactly what a human earns.
  - This is the single biggest economic handicap the original gives the AI, and it is absent. It contributes to the flat difficulty curve in §4.
- **Evidence.**
  - `grep -i purifier engine/` finds only the real Ore-Purifier path: `engine/world.cpp:4656-4670` counts own `OrePurifier=` buildings, and `engine/harvest.cpp:608-611` adds `purifiers*PurifierBonus*value`.
  - rulesmd `[General] AIVirtualPurifiers=4,2,0 ; h,m,e ... controls harvested money bonus except in Campaign`.
- **Original.**
  - Loader: `FUN_0066cf70` reads `AIVirtualPurifiers` into Rules+0x1320 (items at +0x1324); see `logs/ghidra_str1.txt`.
  - Readers (`logs/ghidra_disp1.txt`): `FUN_00522a80` (decompile in `logs/ghidra_dec2.txt`) and `FUN_0073c560` @0x73D30B (UnitClass vtable 0x7F4EAC):
    ```
    n = house+0x538c (own purifiers);
    if (!house->IsHuman(+0x1ec) && session != campaign)
        n += Rules.AIVirtualPurifiers[house+0x184];
    bonus = n * Rules+0xF3C (PurifierBonus .25) * unload_value
    ```
- **Severity / effort.** MAJOR / S.
- **Fix location.** The purifier vector in `World` step (world.cpp:4657): `purifiers[h] += AIVirtualPurifiers[difficulty_class_index(diff)]` for skirmish AI houses. Parse it in `make_ai_slot_config` next to `HarvestersPerRefinery` (`engine/ai_skirmish.inc:144-151`). Campaign is excluded by the original.

### W6-03-04 — Enemy selection: nearest-forever, no anger, no hate delay
- **What the player sees.**
  - In 1v3 an AI being hammered by the player keeps sending its waves at its nearest neighbour.
  - Attacking an AI never draws its retaliation.
  - Each AI sticks to one enemy until that house is completely defeated (walls included, W6-03-01).
- **Evidence.**
  - `World::ai_pick_enemy_house` (`engine/world.cpp:3170-3199`) re-picks only when the current enemy is defeated, and uses distance only.
  - No damage-to-anger path exists (`grep -n anger engine/` returns nothing).
  - `AIHateDelays` is unread.
  - `ai_find_typed_building` (actions 46/47, `engine/world.cpp:3449-3478`) ignores the enemy house altogether, so a "Tech Oil Capture" or "attack enemy War Factory" team may go for any enemy.
- **Original** (decompiles in `logs/ghidra_dec4.txt`).
  - `HouseClass` keeps an anger list (house+0x5608, pairs of house and anger; count +0x5614).
  - `FUN_005045b0(delta, house)` adds anger, then sets house+0x5600 = the living, non-allied house with the highest anger.
  - `ReceiveDamage` (`FUN_00700da0` @0x7012FD) feeds it damage from the attacker's house.
  - In the house AI (`FUN_004fd320`, lines 20-90 of the decompile), once the per-difficulty `AIHateDelays=30,50,70` frame timer (Rules+0x1178, armed in `FUN_006862d0`) runs out with no enemy, the AI adds 1 anger to the **nearest** non-defeated house.
  - The enemy is cleared on defeat (`FUN_004fd320`) and on alliance (`FUN_004f9990`).
- **Severity / effort.** MAJOR / M.
- **Fix location.**
  - `HouseAiRec`: add an anger vector.
  - `World::apply_damage` call sites: add anger for building and unit damage when the attacker's house is known.
  - Replace `ai_pick_enemy_house` with the arg-max rule and the hate-delay seed.
  - Make `ai_find_typed_building` prefer `rec.enemy_house`, as the unit and building pickers already do.
  - Save the anger table (save format bump).

### W6-03-05 — Superweapon AI: wrong target set; IC/Chrono fired by an invented rule while the original's script/trigger path is inert
- **What the player sees.**
  - In 1v3 a nuke or Lightning Storm can go at whichever enemy (another AI) holds the highest-value object, not at the house the AI is fighting.
  - Iron Curtain fires on any own vehicle clump, often at home: 7 Iron Curtains in P3, 3 in P2.
  - The Chronosphere teleports a vehicle clump "next to the highest-value enemy building".
  - In the original, a Soviet AI with a charged Iron Curtain forms a "Soviet Iron Curtain" team that gathers outside the enemy base, curtains itself and attacks factories. An Allied AI chronoshifts its "Chrono-Sphere Unit" team onto enemy power.
- **Evidence.**
  - `World::ai_try_fire_superweapon` (`engine/world.cpp:3206-3306`) scores `is_enemy(house, b.house)` objects (lines 3241, 3264), not `rec.enemy_house`. The comment at line 3213 still says "the engine keeps no single house+0x5600 enemy", which stopped being true with W2-SK-08.
  - Lines 3275-3302 fire Iron Curtain and Chrono.
  - `classify_op` (`engine/ai_team.cpp:34`) maps 55 IronCurtainMe and 57 ChronoPrepAQ to `Inert` in skirmish (campaign has 55: `engine/campaign_runtime.inc:2197-2203`).
  - `trigger_condition_true` (`engine/ai_trigger.cpp:98-104`) returns false for ct=5/6.
  - aimd triggers made dead by this: `Soviet Iron Curtain` (ct=5, weight 5000), `Soviet Iron Curtain Easy`, `Allied C-Sphere Easy/Medium/Hard` (ct=6, weight 5000). Their teams `Soviet Iron Curtain Team` / `Allied Chrono-Sphere Unit` use scripts `0C31EA4C-G` (53 gather enemy, 5, **55**, 49, 0,7…) and `0CAC55CC-G` (54, **57,9**, 5, 49, 0,6…).
  - rulesmd `AIMinorSuperReadyPercent=.7` ("when a Curtain or Sphere is this charged, the AI will consider it in its AITriggers") is unread in skirmish.
- **Original** (`logs/ghidra_dec1.txt`).
  - `FUN_00509710` (the house superweapon AI) switches on the type:
    - 0 Nuke → `FUN_0050ca10`;
    - 2 Lightning Storm → `FUN_00509c20` → `FUN_0050ca10`;
    - 5/6/8/11 → `FUN_00509af0`;
    - 7, 9, 10.
  - **There is no case for 1 (IronCurtain) or 3/4 (Chronosphere/ChronoWarp)**: the house AI never fires them.
  - Nuke requires house+0x5600 ≠ −1.
  - `FUN_0050ca10` begins `enemy = HouseArray[house+0x5600]` and scores only objects with `owner == enemy`.
- **Severity / effort.** MAJOR / M.
- **Fix location.**
  - `ai_try_fire_superweapon`: restrict to `rec.enemy_house`, return when it is −1, and drop the IronCurtain/Chrono branch.
  - `ai_trigger.cpp`: implement ct=5/6 as "own IC/Chrono charge ≥ AIMinorSuperReadyPercent".
  - `ai_team.cpp` + `World::ai_execute_team_command`: implement 55 (reuse the campaign implementation) and 56/57 (chronoshift the team to the target-type building or quarry).

### W6-03-06 — No base defences for the first 18–21 minutes
- **What the player sees.** An early tank rush meets an AI base with **zero** defences:

  | Run | First base defence |
  |---|---|
  | S1 Brutal Soviet | NALASR at min 20 |
  | S2 Brutal Allied | GAPILL at about min 18 |
  | S3 Easy Soviet | NALASR at min 21 |
  | P4 | none for AI 3 (Soviet) even at min 28 (`shots/a1v3_soviet_ai_base_min28.jpg`) |

  Before that, the AI has a Nuclear Silo or Chronosphere (Allied at min 10 in P1).
- **Evidence.**
  - `build_base_plan` (`engine/ai_base.cpp:181-199`) emits barracks, war factory, radar, 4 refineries, naval yard, tech centre, superweapons, then all `defense_target` defences in one block, then all walls.
  - W2-AN-21 moved the tech centre and superweapons ahead of the defences (the comment at lines 185-190).
  - `AIPickWallDefensePercent=50,25,10` is unread.
- **Original.**
  - ai-base.md §2 step 7: the defence placeholders are "RNG-spaced through the tail" (`FUN_005052d0`, insert positions from `rand(lo,hi)`).
  - The per-difficulty insertion % is at Rules+0xdd8, and the wall/gun choice comes from `AIPickWallDefensePercent` (§3).
  - So the original interleaves defences with the rest of the build-out from the first refinery on.
- **Severity / effort.** MAJOR / S (the RNG-spaced insert already has an RE description; tie-break [RE-FALLBACK]).
- **Fix location.** `engine/ai_base.cpp` `build_base_plan`: insert each defence node at a seeded random position after the first refinery, not as a tail block.

### W6-03-07 — The AI hoards money and fields a small army
- **What the player sees.**
  - The Allied AI sits on huge sums while sending small, mostly-infantry waves:
    - S2: 13.6k at min 12, 42k at min 21;
    - S8: 24–39k from min 12;
    - P3: 50–94k;
    - P4: 41k with an army of 13 GI, 8 GGI, 2 dogs, 1 Grizzly and 3 Harriers at min 28 (`shots/a1v3_allied_ai_fortress_min28.jpg`).
  - The Medium Soviet in P2 had 12.8k at min 16.
  - Every AI keeps **one war factory and one barracks all game**: GAWEAP×1 / NAWEAP×1 in every soak at min 30 (`logs/soak_*.log`).
- **Evidence.**
  - Unit production is only ever for team shortfalls (`engine/world.cpp:4072-4107`). Trigger teams come one per TeamDelays pass (2000–3500 frames).
  - The second war factory is only appended by `next_build`'s expansion after the whole static plan is satisfied (`engine/ai_base.cpp:287-331`), i.e. after 22–25 defences and 22–25 walls. A second barracks is never planned (BarracksLimit=2 unread).
  - `InfantryBaseMult` and `FillEarliestTeamProbability` are unread.
- **Original.** [RE-OPEN, W6-c]
  - `FUN_004fdb30` is not a "plan satisfied" expansion. It is the raise-money / missing-refinery-harvester-war-factory recovery (W6-03-08).
  - Where the original adds the second war factory and barracks, and whether it spends surplus credits outside team demand, is not pinned.
  - The behaviour clearly differs: a Brutal original AI with 4 oil derricks does not keep 90k idle.
- **Severity / effort.** MAJOR / M (needs RE-c first).
- **Fix location.** `engine/ai_base.cpp:181-199` and `287-331`, plus `World::step_ai_teams` production.

### W6-03-08 — No "raise money" economy recovery
- **What the player sees.** An AI that loses its harvesters or refinery while short of cash never recovers. It cannot afford the miner, and nothing frees money; the only sale rule needs a building below ConditionRed.
- **Evidence.**
  - `engine/world.cpp:3033-3050`: the AI sells only when credits < CreditReserve **and** the building is red, with probability `rand(0,50) < TechLevel`.
  - P3/P4 show AIs at 260–1,900 credits for long stretches. A full harvester wipe was not caught in the runs, so this row rests on the code and the RE.
- **Original** (`logs/ghidra_dec9.txt`, `ghidra_dec8.txt`).
  - `FUN_004fd320` rates urgencies through `FUN_004fd7c0` for a computer house in MP/skirmish. With no refinery (house+0x15c < 1) the target is the refinery (`BuildRefinery`), else the war factory; with refineries the target is a `HarvesterUnit` (Rules+0xb40).
  - On top urgency `FUN_004fdb30` walks the base nodes backwards and **sells buildings** (vtable +0x1a0, refund via +700) until credits plus refunds cover that cost. It then sets house+0x564c/0x5650 to build it.
- **Severity / effort.** MINOR (no run showed a dead economy, but it is the original's anti-stall mechanism) / M.
- **Fix location.** A new step in `World::step_ai_houses` after 6b.

### W6-03-09 — The AI locks its starting infantry inside civilian buildings for the whole game
- **What the player sees.**
  - At min 1 the AI's start force is already garrisoned in the nearest civilian buildings: S1 `ai_aboard=9` at min 1, 15 from min 21, and CARUS04/CARUS06 listed as AI buildings.
  - They never take part in an attack.
  - Each garrisoned building becomes an AI structure the player may have to clear to win (W6-03-01 RE-open).
- **Evidence.** `World::w5_ai_pass` (`engine/world_w5.cpp:450-490`): every 30 s, idle team-less Occupier infantry go into the nearest empty neutral garrison within 20 cells. Tagged [RE-FALLBACK]: "the stock aimd scripts never name a civilian building".
- **Original.** No garrison behaviour was found in `aimd.ini`, and this wave did not trace it (R-081 marked it FIXED with an invented rule).
- **Severity / effort.** MINOR / S.
- **Fix location.**
  - At least leave the start force out (it is the first team's free recruit pool).
  - Better: gate the rule off until RE-W6-b finds the original trigger.

### W6-03-10 — Difficulty levels barely differ, and not in order

| 20-min soak, passive human, Soviet AI | First damage (min) | Human dead by | Units built @10 / @15 |
|---|---|---|---|
| hillbtwn Easy | 6.7 | min 15 | 11 / 17 |
| hillbtwn Normal | 6.8 | min 19 | 13 / 26 |
| hillbtwn Brutal | 6.2 | min 7 | 18 / 26 |
| May Day Easy | 7.9 | min 20 | 11 / 17 |
| May Day Normal | 7.0 | min 17 | 12 / 25 |
| May Day Brutal | 6.4 | min 17 | 26 / 40 |

- **What the player sees.**
  - The first attack lands at minute 6–8 on every difficulty.
  - On A Hill Between, Easy razed the passive base 4 minutes faster than Normal.
- **Missing handicaps** in the original's toolkit:
  - W6-03-03 (income ×2/×1.5);
  - Armor (R-082, still RE-FALLBACK);
  - `AIHateDelays` (W6-03-04);
  - the per-difficulty defence-insertion % (W6-03-06).
- **Severity / effort.** MINOR (new data for R-082/RE-Q5; most of it is fixed by W6-03-03/04/06) / S after those.
- **Acceptance.** Re-run the six soaks and expect a monotonic order in "dead by" and "built @15".

### W6-03-11 — The score screen game time is wrong at GameSpeed 3 [RE-OPEN]
- **What the player sees.** P2 ended about 23.3 wall-minutes after start, at f=20954 driver frames at GameSpeed 3. The Skirmish Score page shows **Time 00:31:01** (`shots/a1v1_medium_score.jpg`).
- **Evidence.** `engine/gf_result.inc:46-47` formats `frames_/15` with logic frames. At GameSpeed 3 there are 20 logic frames per second (gamespeed.md §2).
- **Original.** Not traced. The MP score page (`MPScore.cpp` `FUN_005AAAE0`) has not been decoded for its time source.
- **Severity / effort.** MINOR / S once traced.

### Already tracked rows, re-observed

| Row | Re-observation in this wave |
|---|---|
| W2-SK-07 / RE-Q4 (no MCV re-buy, data-faithful) | Two Brutal AIs in P3 spent 28 minutes without ConYard or factories, one holding 94k credits. Faithful to the data, but it feeds the W6-03-02 stall |
| R-084 (May Day AI-vs-AI no winner) | May Day miners sit in SeekOre from min 21 (S2, 8/8). Not re-run at 40 min |
| R-082 (Armor/Groundspeed handicap) | Re-scanned: house+0x1a0 / +0x190 are only written, in `FUN_004f6dd0` @0x4F6E3B/0x4F6E71/0x4F6F39/0x4F6F65 (`logs/ghidra_disp3.txt`, displacement scan). No new reader; R-082 stands |
| R-083 (target picker proxy, value=1) | Seen as teams picking the nearest building, walls included (W6-03-01) |
| Naval use | Ships appear only after minute 24 on Depth Charge, and never in 30 minutes on May Day despite a yard from min 9–11. Stock naval triggers are ct=4 (always true) with weights 20/40, so this may be faithful. Recorded, not filed |
| AI capture / repair | Oil captured on every map (`ai_tech` up to 8). Repair runs under attack (S8 `repair_frames=5406`). No gap |

---

## 4. Open RE questions

| # | Question | Where to look |
|---|---|---|
| W6-a | `Paranoid=yes` (Rules+0x17e0). `FUN_00501460` makes every non-defeated computer house ally with every other computer and declare war on every human. Its gate is `Rules+0x14b5` OR a session-object virtual (`*DAT_00a8a1a4`+4), AND `GameOptions+0x11e0 == 0`. It is called at AI base init (`FUN_0050a3e0` @0x50A5D4, `FUN_0050c030`, `FUN_00504fa0`), on an AI defeat (`FUN_004fbed0`) and when a human allies (`FUN_004f9990`). **Is it live in a plain skirmish?** If so, 1v3 AIs never fight each other in the original, and the clone's AI-vs-AI warfare in P1/P3/P4 is wrong | Identify Rules+0x14b5 (loader write not found by the displacement scan) and the vtable at `DAT_00a8a1a4`. Decompiles: `logs/ghidra_dec3.txt`, `ghidra_dec6.txt` |
| W6-b | Does the original skirmish AI garrison civilian buildings at all, and does an occupied civilian building keep its owner alive under ShortGame? | `BuildingClass` occupier/owner change; the defeat counter in `HouseClass` (the per-house building count read by the ShortGame check, near `FUN_004f8350`) |
| W6-c | Where the original adds the second war factory and second barracks (`WarLimit=2`, `BarracksLimit=2`), and whether surplus credits are spent outside team demand | `FUN_005052d0` (plan build) and `FUN_004fe200`. Check whether `[AI] *Ratio` is applied at runtime |
| W6-d | The time source of the MP/skirmish score page | `FUN_005AAAE0` / `FUN_005C92C0` |
| W6-e | AI refinery placement. Does the original bias refineries toward ore fields? The clone uses the ConYard-out Chebyshev scan (ai-base.md §3 [RE-FALLBACK]). No reader of BuildingType+0x16BB (Refinery) was found in the 0x50xxxx house code | The AI placement routine called when a structure finishes for a computer house |

---

## 5. Prioritized fix list (one fix agent)

Ordered by player impact over effort. Items 1–4 are small and independent; do them first and re-run P3 and P5.

1. **W6-03-01 walls/insignificant (S).**
   - `World::step_victory` ignores `Wall=yes` and `Insignificant=yes` types.
   - The AI quarry match and quarry-exists and the superweapon scorer skip walls.
   - Gate: `walls_alive.play` must end WON.
2. **W6-03-03 AIVirtualPurifiers (S).**
   - Per-house virtual purifier count for skirmish AI houses (`engine/world.cpp:4657`, `engine/ai_skirmish.inc`).
   - Unit test on `step_harvest` income: Brutal = 2× human.
3. **W6-03-05 superweapon target set (S part).**
   - Score only `rec.enemy_house` objects; no fire without an enemy; drop the house-level IC/Chrono branch (`engine/world.cpp:3206-3306`).
4. **W6-03-06 interleaved defences (S).**
   - Seeded random insertion of the defence nodes after the first refinery (`engine/ai_base.cpp:181-199`).
   - Re-pin `ai-base` gates if their plan order is asserted.
5. **W6-03-04 anger-based enemy (M).**
   - Anger table, damage feed, `AIHateDelays` seed, arg-max pick, `ai_find_typed_building` preference, and the save field.
6. **W6-03-05 rest (M).**
   - Trigger ct=5/6 with `AIMinorSuperReadyPercent`; script 55 (port from campaign) and 56/57 in skirmish teams.
7. **W6-03-02 acceptance gate.**
   - After 1+5, a 1v3 Brutal Arena run with the simple scripted player (`a1v3_brutal_arena.play`) must reach WON/LOST before min 40, or show the AIs finishing the crippled human.
   - If it still stalls, take W6-a (Paranoid) next.
8. **W6-03-09 (S).** Exclude the start force from the invented garrison rule, or gate the rule off pending W6-b.
9. **W6-03-08 raise-money recovery (M).** Only if time remains.

Then re-run the six difficulty soaks (W6-03-10) and record the new table.

Leave W6-03-07 (needs RE W6-c) and W6-03-11 (needs RE W6-d) for an RE pass.

---

## 6. Fix status (fix agent #3, branch `w6fix-ai`)

RE for every row: `docs/re/ai-base.md` §7 (private Ghidra copy `D:\ra2-re\w6fix-ai`, outputs in
`D:\ra2-re\w6fix-ai-out`). Save format **v25**. Gate hashes: all six AI gates unchanged
(`--demo-ai-base` `b61f283f7fd56db9`, `--demo-ai-team` `fdd60988623ca3ef`, `--demo-ai-trigger`
`287a87c0aefdfec7`, `--demo-skirmish` `fde3f587e747f765`, `--demo-multihouse` `39412ae996cce8ae`,
`--demo-superweapon` `e6969fed3e772bd3`); no re-pin.

| Row | Status | What changed |
|---|---|---|
| W6-03-01 | **FIXED** | `Wall=`/`Insignificant=` types never count for victory or `ai_house_alive`; walls are never an AI quarry or SW target. `scripts/play/sk-walls.play` ends WON. |
| W6-03-02 | **FIXED** | 1v3 Brutal Arena with the simple scripted player (`a1v3_brutal_arena.play`): LOST at f≈9450 (min ≈ 10.5; was a 28-min stall). Fast form `scripts/play/sk-1v3.play` (passive player, LOST at f=1664). Main causes removed: W6-a (the AIs ally against the human), walls, anger. |
| W6-03-03 | **FIXED** | `AIVirtualPurifiers` 4,2,0 → Brutal 2×, Medium 1.5× harvest income (unit test). |
| W6-03-04 | **FIXED** | Anger table + damage feed + `AIHateDelays` seed + arg-max Enemy + MakeAlly/MakeEnemy; `ai_find_typed_building` ranks the Enemy (and neutral) first. Saved. |
| W6-03-05 | **FIXED** | SW picker scores only the Enemy's ground objects with the original order/values; no fire without an Enemy; paradrops to the Enemy base; the invented IC/Chrono auto-fire is gone. ct=5/6 AITriggers and script actions 55/56/57 work. |
| W6-03-06 | **FIXED** | `FUN_005052d0` plan tail: 1 + `AIExtraRefineries` refineries, defence placeholders at rand(3,n-1)+1, `AIPickWallDefensePercent` walls. Brutal Soviet first defence at min 5 (was 20). |
| W6-03-07 | **RE done, no code** | W6-c: `WarLimit`/`BarracksLimit` have no runtime reader; the plan holds one war factory and one barracks; no surplus-spending path outside team demand was found. Hoarding remains (Allied Normal AI-vs-AI: 46–60k from min 20). |
| W6-03-08 | **FIXED** | Raise money: sell plan buildings from the tail to rebuild a refinery / miner (`--ai-soak --ruin-ai-at`). |
| W6-03-09 | **FIXED** | The invented civilian garrison is off; the start force stays in the recruit pool. |
| W6-03-10 | **PARTIAL** | New handicaps: `MultiplayerAICM` (Brutal starts with 5× money), virtual purifiers, hate delays, defence %. Table below. |
| W6-03-11 | **RE done, owned by agent #6** | W6-d: the score page shows `Scenario+0x614` elapsed time on the `timeGetTime()>>4` clock / 60 (wall-clock play time ≈ ms/960), not game frames. |
| W6-a | **ANSWERED: yes, implemented** | `FUN_00501460` + `MPBattleMD.ini AlliesAllowed=yes`: in a skirmish with no lobby teams every AI allies with every other AI against the human. |
| W6-b | Partly | An occupied civilian building is `Insignificant=yes`, so it never keeps a house alive (W6-03-01). Whether the original AI garrisons at all is still open. |

**AI-vs-AI** (`--ai-soak --ai-vs-ai --diff brutal --map hillbtwn.map --minutes 40`): Soviet Brutal wins,
the Allied Normal house is defeated at min ≈ 35.

**Difficulty soaks** (20 min, passive human, Soviet AI):

| Run | Human dead by (min) | Units built @10 / @15 | Max AI units alive |
|---|---|---|---|
| hillbtwn Easy | 11 | 8 / 16 | 19 |
| hillbtwn Normal | 18 | 12 / 19 | 20 |
| hillbtwn Brutal | 8 | 22 / 44 | 51 |
| May Day Easy | alive at 20 (base 268/1000) | 6 / 8 | 16 |
| May Day Normal | 12 | 13 / 16 | 15 |
| May Day Brutal | 16 | 22 / 44 | 50 |

Strength (units built, army size) is now distinct and ordered on both maps. Time-to-kill against a
passive player is still not monotonic: the Brutal May Day AI razed the base to one ConYard at
188 HP by min 7, then its "attack anything" teams chased the human's 5 start infantry for 8 minutes
(the nearest-unit quarry pick, R-083); Easy on hillbtwn plans only 6 defences and spends the rest on
units.
