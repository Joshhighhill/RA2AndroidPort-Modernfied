# 13b: combat suspects (w7sus-combat)

**Scope:** master 9bf5ccd, the existing `build/ra2engine.exe`, and retail YR 1.000 `gamemd.exe` (Ghidra, a copy of the fixG project, now deleted).

**Locations:**
- Scratch scripts, logs and the extracted maps: `D:\ra2-re\w7sus-combat-out\`. The extracted maps are `all04u.map` plus `maps\`.
- Ghidra dumps: `d1.txt` and `d2.txt`.
- Older dumps reused: `w7-shc-B-out/b5.txt` (FUN_00707500), `out/combat3_fp8_707C40.c` (CanRetaliateToAttacker), `out/p4/supp_4D7250.c` (FootClass::ReceiveDamage), `w7-shc-B-out/b6.txt` (BuildingClass::ReceiveDamage) and `w7-abs-B-out/dc2.txt` (Evaluate_Object).

**House indices in the clone's ALL04:**
- 0 = Player
- 1 = Russian1
- 2 = Russian2 (it owns NAPSYA at 43,82)
- 10 = PlayerAlt2 (`PlayerControl=yes`, IQ=0, allied to the Player)

| # | Suspect | Verdict |
|---|---|---|
| 1 | ALL04 PlayerAlt2 team GIs don't fight back | **CONFIRMED BUG.** The root cause is not retaliation. Houses with index ≥ 8 never auto-acquire a unit target, because of an 8-bit `seen_by_mask`. Retaliation itself is faithful. |
| 2 | Rocketeers do little damage to the Amplifier | **FAITHFUL.** 25 × 25% (SSA vs concrete) = 6 per shot. Damage applies; fire, range and altitude are correct. "No damage" is NOT REPRODUCIBLE. |
| 3 | ai_protect skips the GetFireError check | **CONFIRMED BUG** (minor). No byte-identity `--demo` gate can move. Only GameScene skirmish `--play` scripts can change. |
| 4 | ai_protect / call_for_help overlap | **CONFIRMED BUG** (gaps, not double counting). There are two diverging re-implementations of the original's single FUN_00707500. |

---

## Suspect 1: ALL04 PlayerAlt2 GIs "don't fight back"

### Verdict
CONFIRMED BUG. The symptom is real: the PlayerAlt2 GIs stand still while enemies two or three cells away shoot their squad-mates. The cause is a clone-only visibility mask that is capped at 8 houses, so every unit of house index ≥ 8 never auto-acquires a unit target.

The other mechanisms behave as follows:
- **Retaliation (CanRetaliateToAttacker):** works. It is faithful for a PlayerControl ("human") house.
- **Team membership:** not the cause. A campaign TeamClass::TookDamage reaction is missing (secondary, see below).

### Evidence (play)
`s1.play` / `s2.play` / `s4.play` start from the `m-all04` opening. The steps are:
1. Drive PlayerAlt2's LCRF 10:0 to (120,68).
2. Press `d` to unload. Five GIs land at (118,66..69) and (119,68); the MTNK stays aboard.
3. Spawn two house-1 Conscripts (`cheat spawn E2 1 cell 115 66` and `115 68`).

`s4/play.log`, sampled every 30 frames:
- At f=940, Conscript E2:17 (from 116,67) targets a GI.
- **GI E1:0, the one being shot, retaliates.** Its `utgt` changes from -1 to 186, the Conscript. The Conscript's HP drops 105 → 89 → … → 9, and it dies.
- E1:0 also dies at f≈1150 (HP 125 → 10).
- **GIs E1:1 to E1:4, standing one or two cells away, keep `utgt=-1` from f=910 to f=1360.** In that window, a live Conscript stands at (115,68), two or three cells away, and its own target is a GI. They never acquire it.

`s1.play` (no player input):
- PlayerAlt2's GIs stay inside the three LCRFs at sea (142,86..92) the whole time. Their team scripts LC2/3/4EnterMap are `3,wp` then `42,6` (Move, then Face), with no Unload.
- So they only fight if the player, who has PlayerControl, unloads them. That is the case above.

### Original
- **No per-house sight gate on auto-acquire.** Evaluate_Object FUN_006F71D0 (`w7-abs-B-out/dc2.txt`) only tests a cloaked target (+0x220 == 2) through FUN_00486FE0, the cell's sensors for the scanner's `HouseClass+0x30` ArrayIndex.
- **House masks are 32-bit.** FUN_004F98B0 IsAlliedWith tests `1 << ArrayIndex & house+0x5788`, so any house index up to 31 works.
- **A PlayerControl house counts as human.** FUN_0050B550 IsControlledByHuman returns `house+0x1EC || (SessionType==0 && house+0x1ED PlayerControl)`. Both the clone (`human_commanded`, world.hpp:1445) and the original therefore treat PlayerAlt2 as human. For a human:
  - **Scanning:** Guard and Area Guard still scan for targets (TechnoClass::AI 0x6F9BC7).
  - **Retaliation** (CanRetaliateToAttacker 0x707C40) needs all of these:
    - no TarCom;
    - Mission Control Retaliate;
    - `Rules+0x17EC` PlayerReturnFire (rulesmd `no`), or a building, or the mission is Guard 5 / Area Guard 0xB / Patrol 0x19;
    - not in a Suicide team (Team +0x5D4 → TeamType +0xAF).

  So the original's idle PlayerAlt2 GIs in Guard both acquire nearby enemies and retaliate.

### Clone
**The mask:**
- `world.hpp:288` declares `std::uint8_t seen_by_mask = 0xFF`.
- `world.cpp:3304-3310` stamps bits only for `h < 8` ("T3: … capped at 8").

**The gates that test `seen_by_mask & (1u << house)`:**
- `combat.cpp:346`: `w7_greatest_threat`, the W7 scan that the armed campaign runs.
- `combat.cpp:1659`: the legacy auto-acquire.
- `building_combat.cpp:274` and `world_depth.cpp:290`: base defences also can never fire on units for a house ≥ 8.
- Only `world_w5.cpp:265` guards it with `p.house < 8 &&`.

For house 10, `1u<<10` is 0x400, and 0x400 & any uint8 is 0. The scan rejects every unit candidate, whether shroud is on or off (the default 0xFF fails too).

**Retaliation** (`world_w7.cpp:121-163` can_retaliate plus `:342-358`) is reached, and it worked for E1:0.

### Root cause
`Unit::seen_by_mask` is 8 bits wide. Campaign maps have more houses:

| Map | Houses | Index ≥ 8 |
|---|---|---|
| ALL04 | 11 | CivilianTech, PlayerAlt, PlayerAlt2 |
| ALL06 | 9 | Elania |
| ALL08 | 10 | Elania, Reinforcers |
| SOV09 | 13 | Russians, GDI, Nod, Neutral, Special |

These lists use the map's `[Houses]` order. Every unit and every armed building of those houses never auto-acquires units. The ALL04 handover (ConYdBuilt → ChangePlayerAlt2House) moves the units to house 0, which is why they "start fighting" afterwards.

### Suggested fix
- **Proper fix:**
  - Widen `Unit::seen_by_mask` to `std::uint32_t` (default `0xFFFFFFFF`) in `world.hpp:288`.
  - Stamp `h < house_count() && h < 32` in `world.cpp:3306-3310`.
  - Keep the `1u << h` tests at combat.cpp:346/1659, building_combat.cpp:274, world_depth.cpp:290 and world_w5.cpp:265. Drop the now-redundant `p.house < 8` there.
  - Adapt `save_fields.hpp:1132`, which needs a format bump (savegame.hpp kFormatVersion), and the `%02x` diagnostics in main.cpp:14082-14088. `hud_pips.inc:337` only uses bit 0 and is unaffected.
- **Minimal alternative:** at the four gates, treat `house >= 8` as seen, the way world_w5.cpp:265 does.

### Test that proves the fix
- **Play test.** Copy `D:\ra2-re\w7sus-combat-out\s4.play` to `scripts/play/w7-all04-alt2-acquire.play`. After the two `cheat spawn E2 1 …` lines, add:
  ```
  until 90 utgt(10,E1,1) >= 0 and utgt(10,E1,2) >= 0 and utgt(10,E1,3) >= 0
  ```
  On 9bf5ccd this fails, with every value at -1.
- **Unit test.** Build a World with 11 houses. Put an idle armed unit of house 10 in Guard, with an enemy unit of house 1 three cells away. One targeting tick later it must hold `target >= 0`. Add the same case for an armed building of house 9.

### Secondary findings (same area, not the root cause)
- **(1a) TeamClass::TookDamage is not modelled.** FUN_006EA8E0 is called from FootClass::ReceiveDamage at 0x4D7373 for a team member. It applies when the result is non-zero, there is an attacker, and the TeamType is not Suicide (+0xAF):
  - **Team not moving (+0x7F == 0):** the team clears its target (+0x34) and sets +0x75 and +0x7B, which forces a re-evaluation.
  - **Team moving:** the team re-targets the attacker, unless it is already a member, the team's current target is in range, or the team leader is an aircraft (WhatAmI 2).

  No clone code reacts to damage on a campaign or AI team. RE status: the effect of the decompiled final call (a virtual call at vtable offset `+0x2C` on the function's third argument) is not fully unwound [RE-OPEN]. This is not needed for the ALL04 symptom.
- **(1b) Suicide teams in campaigns.** `tg.team_suicide` and `tg.team_aggressive` are only filled from `ai_houses_` (skirmish) teams (`world_w7.cpp:291-306`). Campaign TeamTypes with `Suicide=yes` therefore still retaliate, and `Aggressive=yes` campaign teams do not scan on the move. The original reads the TeamType through Team +0x5D4 for every house. ALL04's teams are `Suicide=no`, so this does not affect the suspect.

---

## Suspect 2: Rocketeers vs the Psychic Amplifier

### Verdict
FAITHFUL (low damage by data). "No damage" is NOT REPRODUCIBLE.

| Check | Result |
|---|---|
| Fire when already in range | yes |
| Altitude / airborne | yes |
| 3-D range | correct (original formula) |
| Damage applied | yes |

### Data
| Item | Value |
|---|---|
| `[JUMPJET]` | Primary=20mm, ElitePrimary=20mmE, JumpjetHeight=500, HoverAttack=yes, BalloonHover=yes. It is an **InfantryType** (`[InfantryTypes] 5=JUMPJET`). |
| `[20mm]` | Damage=25, ROF=30, Range=5, Warhead=SSA |
| `[SSA]` | Verses=100,100,100,60,40,40,75,50,**25**,100,100. The 9th entry is concrete. |
| `[NAPSYA]` | Strength=1000, Armor=concrete, Foundation=4x4. The ALL04 map override only sets Name, Capturable and WorkingSound. |

So 25 × 25% = 6.25, truncated to **6 per shot**:
- 4 Rocketeers do 24 per 30-frame volley, which takes about 42 volleys, roughly 1260 frames, to kill it.
- That is "little damage" by design.

### Evidence (play)
**`r2.play`** (two house-0 Rocketeers spawned at (49,85), defences and Soviet units removed, `cheat camp_attackb 2 NAPSYA JUMPJET`):
- The amplifier goes 1000 → 988 → 976 → 964 …
- That is **12 per volley = 2 × 6**, starting within 15 frames of the order, with no move (`umov=0`, `utgtb=3`).

**`r3.play`** (the real PlayerAlt2 Rocketeers):
- They fly to (49,85) with `camp_move`, all airborne (`airborne(10)=14`), then get the attack order once they are already in range.
- Damage starts within 15 frames: 1000 → 940 → 880 → 820 …
- That is **60 per volley = 10 × 6**. The amplifier dies at f≈1600, about 360 frames after the order.

**Range in 3-D.** The clone uses FUN_006F6750's rule at `combat.cpp:1224-1240`: 3-D distance to the building centre, range + (w+h)×64 = 1280 + 512 leptons. From (49,85) at height 500, the distance to the centre is about 1314 leptons, which is in range.

**Facing.** Infantry are exempt from the turret aim gate. HoverAttack lift-off is at `world_an.cpp:1640-1651`.

### Original
- **Damage:** WarheadTypeClass Verses × Damage, then ftol (6).
- **Range:** FUN_006F6750 (building centre, range + (w+h)×64).
- **Infantry type:** the Rocketeer is infantry (WhatAmI 0xF).

### Root cause / fix / test
- **Root cause:** none in the engine.
- **Fix:** none.
- **Optional regression:** keep `r3.play` as `scripts/play/w7-all04-rocketeer-amp.play`, asserting:
  ```
  until 30 bhp(2,NAPSYA) <= 940
  ```
  (one 10 × 6 volley).

**Unexplained (not a combat bug).** In r2, `bhp` later fell 72 per 15 frames with only two house-0 Rocketeers firing. That matches about 12 PlayerAlt2 Rocketeers attacking the amplifier on their own. PlayerAlt2's JUMPJET count also differs: 4 in s1 versus 10+ in r3 after `camp_killu`. These look like campaign team / trigger effects and were not chased.

---

## Suspect 3: ai_protect skips the "can fire on the attacker" check

### Verdict
CONFIRMED BUG (minor). The function's own RE comment (`ai_unit.cpp:112-124`) says the original requires "able to fire on the attacker (GetFireError != ILLEGAL)". The code does not do this.

### Original
FUN_00707500 (`w7-shc-B-out/b5.txt`) has two candidate loops: infantry at 0x707618.., then units. Each candidate needs all of these:
- **Owner and recruitment:**
  - the same Owner (vtbl+0x38);
  - no Team, or a Team whose TeamType has +0xF6 (IsBaseDefense);
  - flags +0x421 and +0x422;
  - IsArmed (vtbl+0x2AC);
  - MissionControl Recruitable (+6), **or SessionType != 0**: in skirmish the mission is not checked.
- **`vtbl+0x3BC(attacker, 0) != 5`.** This is **GetFireError with weapon index 0, the primary**, against ILLEGAL.
- **Movement zone:** reachable (FUN_0056CC30).
- **Threat value:** FUN_004D96B0(attacker) != 0. Its value is:
  - 0 for a non-base-defence team member or a unit in Harvest;
  - −cost if the unit's TarCom is already the attacker, which is subtracted from the budget;
  - otherwise cost<<10 / travel time.
- **The six chosen:** the top six by that value, within a budget of attacker cost × Rules+0xB18.

### Clone
`engine/ai_unit.cpp:156`:
```cpp
if (u.w_damage <= 0 || !u.w_ag) continue;   // must be able to fire on a ground attacker
```
It does not test the weapon's Verses against the attacker's armour (GetFireError ILLEGAL when the warhead's Verses is 0), or any of the GetFireError legality.

Example from the stock Allied and Soviet skirmish rosters:
- **DOG and ADOG:** BadTeeth/GoodTeeth → `[ParasiteDog]` Verses=100,100,100,**0,0,0**,0,0,0,0,0. They cannot affect light, medium or heavy armour.
- **What happens:** when a tank shoots an AI harvester, ai_protect can pick the nearest idle dog. It then calls `order_unit_attack` on the tank (`world.cpp:4422` ai_order_attack has no legality check). The dog uses one of the six slots, and the original would not pick it.
- **Other primaries with 0% cells:** TANY (vehicles), CLEG (special_2), and, if Yuri were ever a skirmish AI, BRUTE and TELE.

`call_for_help` (`world_w7.cpp:246`) does test `unit_weapon_for_unit(u, A) < 0`. That is closer, but it accepts any weapon slot, while the original tests slot 0 only.

### Would the gates move?
- **Byte-identity `--demo-*` gates: NO.**
  - ai_protect only runs for houses armed by `arm_skirmish_ai` (`roster_to_protect`, `ai_skirmish.inc:149-151`).
  - That is called only from GameScene: `main.cpp:6447`, `main.cpp:7430` and `game_scene_net.inc:175`.
  - The main.cpp comment at ~5866 says: "no gate constructs GameScene". `--demo-skirmish` uses `launch_skirmish` and never arms it.
- **`--play` skirmish scripts:**
  - `sk-1v3` and `sk-a-lose` have a passive human. The AIs never come under attack from the human, and in a plain skirmish the AIs are allied against the human (W6-a), so ai_protect never fires. I predict them **unchanged**.
  - `sk-a`, `sk-s` and `sk-walls` (any script whose player attacks AI miners or ships) could change `play_hash` and the timing whenever a dog is the nearest idle candidate. I do **not** expect their outcomes to change.
  - These predictions are by reasoning; I did not run them, per the load rules.

### Suggested fix
At `ai_unit.cpp:156`, replace the test with the primary's GetFireError-ILLEGAL equivalent against `au`. Use the same helper and Verses rule that `world_w7.cpp:121-139` uses, restricted to slot 0, for example `primary_view(u)` with a Verses-0 / AG/AA test.

Better still is the unification in suspect 4, which also adds:
- MissionControl Recruitable (campaign only);
- the threat-value order and budget;
- base-defence team members.

### Test that proves the fix
A unit test on a World with an AI house that has `roster_to_protect[HARV]=1`:
- **Setup:** an idle DOG one cell from the HARV and an idle E2 four cells away. An enemy MTNK hits the HARV.
- **On 9bf5ccd:** the DOG is ordered (target == MTNK).
- **After the fix:** the DOG has no target and the E2 is ordered.

---

## Suspect 4: overlap between ai_protect and call_for_help

### Verdict
CONFIRMED BUG. There is no double counting. The clone splits one original routine into two paths that disagree with each other and with the original, and it leaves gaps.

### Original (one mechanism, FUN_00707500)
**Callers:**
- **TechnoClass::ReceiveDamage 0x701C74 (`fixDout/techrecvdmg_00700da0.c:498-500`).** It calls when all of these hold:
  - the victim TechnoType has ToProtect (+0xC96), or the techno has +0x3CF;
  - the owner is not IsControlledByHuman (FUN_0050B550);
  - there is an attacker.
- **BuildingClass::ReceiveDamage 0x44223C.** It calls for every hit on a building that has an attacker, and stamps the owner's +0x54D8/+0x54DC (last attacked frame and attacker house).
- **FootClass::ReceiveDamage 0x4D73BA.** It calls for a team whose TeamType has +0xAC (Whiner). In YR 1.000 this path is dead: the branch above at 0x4D7373 returns first for any team member with a non-zero result.

**Head rejects** (`b5.txt`). The call is refused when any of these holds:
- the attacker is null or allied;
- the victim's owner is human (in campaign, PlayerControl counts as human);
- the attacker is in limbo (+0x81);
- the session is a campaign and the victim is armed;
- the attacker is neither Infantry (0xF) nor Unit (1);
- the victim type is Insignificant (+0x232);
- the attacker's +0x650 timer is running (foot attackers only).

**Effect:**
- The candidate rules are listed under suspect 3.
- Each recruit gets Mission_Set:
  - Area_Guard 0xB if rand(0,99) > 65 or it is in a base-defence team;
  - otherwise Rescue 0x15 (Mission 21).
- Each recruit is then given TarCom = the attacker (vtbl+0x3C8).
- The +0x650 cooldown starts only when the budget is spent.

### Clone
- **Units:** `World::ai_note_damage` (`world.cpp:4013-4016`) queues `ai_protect` (`ai_unit.cpp:125-168`) for skirmish AI houses (`ai_enabled`), and only for types in `roster_to_protect`.
- **Units, other computer houses:** `step_missions_w7` calls `call_for_help` for to_protect units of non-`ai_enabled` computer houses (`world_w7.cpp:367`).
- **Buildings:** `call_for_help` is called for every non-human house's buildings, including skirmish AI houses (`world_w7.cpp:308-316`).
- **No double counting:** a unit goes down exactly one path (`ai_enabled` versus `!ai_enabled`), and buildings only ever go through call_for_help.

### Gaps and divergences
| # | Aspect | Original | ai_protect (skirmish AI units) | call_for_help (other computer houses, all computer buildings) |
|---|---|---|---|---|
| G1 | Trigger type test | TechnoType ToProtect | only types in the AI's **roster** (`ai_unit_type_of` ≥ 0). A captured, crate, map-placed or off-roster ToProtect unit never calls (`world.cpp:4013`). | `v.tg.t.to_protect`, correct |
| G2 | Attacker | the real ReceiveDamage source | searched as "a unit of `attacker_house` whose `target == victim`" (`ai_unit.cpp:129-138`). Splash, area-fire, garrison or opportunity-fire hits, or a shooter targeting something else, get **no help**. | the real attacker, correct |
| G3 | Attacker class | rejects only non-Infantry/Unit (aircraft) | skips `is_aircraft`, which in the clone includes every Jumpjet-locomotor unit (`main.cpp:1029-1030`). So a **Rocketeer (InfantryType)** attacking a miner or a building gets no response. | same skip (`world_w7.cpp:220`) |
| G4 | Fire legality | GetFireError(attacker, slot 0) != ILLEGAL | missing (suspect 3) | any slot (`world_w7.cpp:246`); should be slot 0 |
| G5 | Team members | allowed only in a base-defence TeamType (+0xF6) | every team member is excluded, including base-defence ones | **no team exclusion** except Suicide. Skirmish AI team members (attack teams) get pulled off their missions by a building hit. |
| G6 | Mission recruitable | checked only when SessionType == 0 (campaign) | never checked; it excludes `moving()` units instead, which the original does not | always checked (`:248`), including skirmish building hits |
| G7 | Selection | threat FUN_004D96B0 (cost<<10 / travel time; Harvest → 0; already attacking → −cost) within a budget of attacker cost × Rules+0xB18 | nearest six, no budget | most expensive six, no budget |
| G8 | Cooldown | attacker +0x650, armed only once the budget is spent | "help already on its way" stand-in (`:143-148`) | 45 frames on every call [SUSPECTED] (`:233-236`) |
| G9 | Issued mission | Area_Guard (34%) / Rescue (66%) + TarCom | Attack order (`order_unit_attack`) | Override_Mission Attack (`w7_override_attack`), which also resumes the old mission afterwards. That is a retaliation semantic, not call-for-help. |
| G10 | Domain test | FUN_0056CC30 zone reachability | water/land domain heuristic | none |

### Suggested fix
1. Replace both with a single `World::call_for_help(victim_house, victim_unit, victim_building, attacker)` that follows FUN_00707500:
   - head rejects exactly as above;
   - the attacker-class test by type kind (infantry/vehicle versus aircraft), not `is_aircraft`;
   - candidates: same owner, (no team || base-defence team), armed, `primary GetFireError != ILLEGAL`, `(campaign ? recruitable mission : true)`, reachable zone, `threat != 0`;
   - pick by threat, within the budget of cost × `Rules+0xB18`;
   - Mission Rescue/Area Guard + target;
   - the cooldown only when the budget is spent.
2. Call it in two places:
   - from `step_missions_w7` for every computer to_protect unit, using the type flag and **the real `h.attacker`**;
   - for every computer building.
3. Delete the `ai_protect_reqs_` path (`world.cpp:4013-4016`, `ai_unit.cpp:102-110,125-168`), or keep `ai_note_damage` for the anger only.

### Test that proves the fix
**(a)** A World with a skirmish AI house (Soviet roster, `roster_to_protect`):
- **Setup:**
  - HARV hit by an enemy **Rocketeer** (JUMPJET) → expect at least one AA-capable idle unit ordered and no non-AA unit;
  - HARV hit by splash from an MTNK aimed at a neighbouring unit → expect help;
  - a captured (off-roster) ToProtect unit hit → expect help.
- **On 9bf5ccd:** all three give no help.

**(b)** A skirmish AI building hit while an AI attack team stands idle nearby → expect team members not recruited (unless the team is base-defence). On 9bf5ccd they are recruited.

---

## Files
| File | Content |
|---|---|
| `D:\ra2-re\w7sus-combat-out\s1.play` .. `s4.play` and the `s1\`..`s4\` folders | Suspect 1 repros (s4 is the key run) |
| `r1.play`..`r3.play` and the `r1\`..`r3\` folders | Suspect 2 repros (r2 house-0 spawned, r3 real PlayerAlt2 Rocketeers) |
| `d1.txt` | FUN_0050B550 IsControlledByHuman, FUN_004D96B0 call-for-help threat, FUN_004F98B0 IsAlliedWith |
| `d2.txt` | FUN_006EA8E0 TeamClass::TookDamage |
| `all04u.map`, `maps\*.map` | Extracted with `ra2extract --raw` (read-only) |
