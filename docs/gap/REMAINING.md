# What is still not implemented (consolidated, verified)

Baseline: **master `f3f1ea1`** (after W4-FIX-POS and W4-FIX-PROJ were merged), 2026-10-07.
Unmerged at the time of writing: **`w4fix-ctl`** (12 commits) and **`w4fix-seq`** (6 commits).
Items those branches cover are marked **IN PROGRESS (branch, commit)**. Where a commit exists it is named. Where the item is in the branch's brief but has no commit yet, the row says "no commit yet".

## Sources and method

**Sources merged:**
- `docs/superpowers/ROADMAP-completion.md` and `ROADMAP-playable.md`
- `docs/gap/01..07`, `GAP-ANALYSIS.md`, `PLAYTEST-STATUS.md`
- `docs/gap/wave2/01..06`, `wave3/01..03`, `wave4/01..06`
- the README's per-phase carry-forward, cut and still-open lists
- the 232 `[RE-FALLBACK]` markers in `engine/`, `src/` and `include/` (76 files)

**How items were checked:** Every item was checked against the code on master and the cited file:line. An old doc's "open" was not trusted when a later wave closed the item, and a "fixed" claim was not trusted for the big items. The ROADMAP-completion phases P1–P10 are all delivered. Their non-goals (Yuri faction, netcode, map editor, mods) are not counted here.

**Spot checks** used my own Release build (worktree `D:\ra2-wt\remaining`, branch `remaining`, `build-rel/`). Play scripts run on master:

| Script | Result | Note |
|---|---|---|
| `m-all01`, `m-sov01`, `sk-a` | PASS | These scripts are shallow. `m-all01` does **not** move Tanya by real orders, so it does not catch R-001. |
| `probe-bridge` | FAIL | Stale script: it uses the old `map:+` lobby target. |
| `probe-flow` | FAIL | Stale script: it expects a ConYard at start. |
| `robust-missing` | FAIL | Needs `RA2_INSTALL_DIR` pointed at a stripped copy of the install. Not an engine bug. |

Code checks confirmed these on master:
- Amphibious matching is exact (`engine/main.cpp:915-919`).
- There is no `LocalSize` reader.
- Nothing in the engine reads PrismSupport, GapGenerator, Overpowerable, AirstrikeTeam or BombDisarm.
- Splash damage skips buildings and own units (`engine/combat.cpp:271-285`).
- Audio focus is never requested.
- The exe is not a WIN32 GUI application.

## Summary

The game is **feature-complete in breadth**: shell, skirmish against up to 7 AIs, all 24 Allied and Soviet missions loading and scripted, save/load, options, and an Android build. What remains is mostly **depth and fidelity**:

- **Mission-sequence plumbing.** Transports, paradrops, input lock, garrison scripts and sleeping units are almost all on the two running branches. Some trigger actions and events are still unimplemented.
- **The phone control scheme.** One-finger scroll, phone view and visible modes are on `w4fix-ctl`.
- **Signature unit mechanics:**
  - The Prism Tower chain, Gap Generator, Tesla Trooper charging and stolen tech are missing.
  - Infantry stand one per cell; the original packs three.
  - Splash damage never hits buildings or friendly units.
- **Audio feedback.** Many unit voices and sounds, and about 12 EVA lines, are never played.
- **About 230 `[RE-FALLBACK]` guesses**, mostly cosmetic or small timing differences.

**Blockers.** There are 3 BLOCKER rows, and all three are IN PROGRESS:
- R-001, ALL01: Tanya cannot swim.
- R-006, phone: a one-finger drag draws a band box instead of scrolling.
- R-012, SOV02: transport cargo is never loaded. The same fix covers the ALL05 spy loop.

No BLOCKER is open outside the two running branches.

**Mission playability on master** (wave-4 survey):

| Verdict | Count | Missions |
|---|---|---|
| YES | 12 | |
| PROBABLE | 9 | trigger chain proven only with cheats |
| DOUBTFUL | 1 | SOV09 |
| NO | 1 | SOV02 |
| BROKEN | 1 | ALL01 |

All three problem missions are targeted by the running branches.

### Counts (rows in the table below, after de-duplication)

| Severity \ Status | OPEN | PARTIAL | IN PROGRESS | RE-FALLBACK | DATA-LIMIT | USER-DECISION | **Total** |
|---|---|---|---|---|---|---|---|
| BLOCKER | 0 | 0 | 3 | 0 | 0 | 0 | **3** |
| MAJOR | 9 | 1 | 11 | 1 | 0 | 1 | **23** |
| MINOR | 41 | 20 | 11 | 14 | 1 | 2 | **89** |
| COSMETIC | 8 | 2 | 0 | 7 | 1 | 0 | **18** |
| (none) | 0 | 0 | 0 | 0 | 0 | 2 | **2** |
| **Total** | **58** | **23** | **25** | **22** | **2** | **5** | **135** |

| Category | Rows | | Category | Rows |
|---|---|---|---|---|
| Campaign | 29 | | Graphics | 12 |
| Units & abilities | 17 | | UI/Shell | 11 |
| Gameplay/sim | 9 | | Controls/Touch | 9 |
| AI | 7 | | Robustness/Release | 7 |
| Fidelity-guess | 7 | | Android/Platform | 5 |
| Animation/FX | 5 | | Audio | 5 |
| User decisions | 5 | | Performance | 4 |
| Data-limits | 2 | | Air/Naval/SW | 1 |

**RE needed:** 57 rows need some reverse engineering of `gamemd.exe` or `game.exe`. The other 78 can be done from data and code alone.

**Effort** for the 128 R-rows that have an estimate (R-200 and R-201 are data limits):

| Effort | Rows | Notes |
|---|---|---|
| S | 75 | |
| M | 47 | |
| L | 6 | R-020 high bridges, R-026 mission play-throughs, R-028 campaign AI houses, R-061 infantry sub-cells, R-083 AI fallbacks, R-100 bridge deck layer |

**Without the two running branches**, open work is 0 BLOCKER and 12 MAJOR rows:
- Campaign: R-026, R-027, R-028
- Units & abilities: R-042, R-043, R-044
- Gameplay/sim: R-060, R-061
- Audio: R-130
- Performance: R-170, R-171
- User decisions: U-1

## What a player will notice first (top 15)

Ordered by how soon a player is likely to hit each problem. **[IP]** means it is IN PROGRESS on a running branch.

1. **Phone: dragging a finger draws a selection box instead of scrolling.** The touch bar also covers the bottom of the battlefield, and sell/repair/placement modes are invisible. The game feels broken on a phone until `w4fix-ctl` lands. [IP] (R-006, R-007, R-008)
2. **The very first Allied mission (ALL01) cannot be finished by playing it.** Tanya will not enter water, so the GIs are never handed over. The scripted bridge collapse does not happen either. [IP] (R-001, R-020)
3. **Scripted intros do not lock input, and the opening camera is wrong.** The view starts on the first unit instead of the map's HomeCell, and the black off-map border and staged units are visible and clickable. [IP] (R-002, R-003)
4. **Reinforcements in the campaign just appear on the ground.** No transports unload, no paratroopers drop, and SOV02 cannot be won. [IP] (R-012, R-013)
5. **No movies anywhere.** The install's MOVIES0x.MIX files are 4-byte stubs, so there are no intro, briefing or victory videos. Briefings are a text overlay (F1). (R-200, R-034)
6. **Infantry stand one per cell.** In the original three share a cell. Squads sprawl, jam in chokepoints and look sparse. (R-061)
7. **Units are quiet.** There is no feedback voice on many actions, and no sounds for entering, capturing, deploying or undeploying, sinking, crashing, crates, credit ticks or turret rotation. About 12 EVA lines never play, among them "Primary building selected", "New rally point" and "Unable to comply". (R-130, R-131)
8. **Signature defences are missing:**
   - Prism Towers do not chain.
   - The Gap Generator does nothing.
   - Tesla Troopers cannot charge coils.
   - Spying on a Battle Lab gives no stolen tech, so the Chrono Commando, Psi Commando and Chrono Ivan can never be built.

   (R-040 to R-044)
9. **No friendly fire, and splash damage never hits buildings.** Desolators, V3 rockets and artillery behave differently against bases and clustered units. (R-060)
10. **The second mission in a session can show the previous side's sidebar art.** [IP] (R-009)
11. **High bridges cannot be destroyed, and units do not visibly cross over or under them.** Scripted bridge breaks in ALL01 and SOV01 do not happen. Low bridges work. [IP for destruction] (R-020, R-100)
12. **Placeholder base-building visuals:**
    - The placement grid is 1-pixel outlines, not PLACE.SHP.
    - The rally point is a cyan X.
    - Defence turrets snap to their target instead of rotating.
    - There is no Iron Curtain tint and no chrono warp effect on the teleported units.

    (R-102, R-146, R-121, R-120)
13. **The shell looks soft and blocky**, and the lobby offers maps that look wrong:
    - The menus are the 640x480 "S" asset set scaled up to 1280x720.
    - The lobby offers 27 YR desert and new-urban maps, drawn with temperate art.
    - The unit roster is the YR rulesmd one (Boris, Guardian GI, Battle Fortress, Robot Tank and so on).

    (R-106, R-105, U-1)
14. **The skirmish AI feels thin.** It never uses Harriers or Black Eagles, never garrisons buildings, and its difficulty pacing is uncalibrated (Brutal gets no armor or speed bonus). Campaign enemy houses use guessed difficulty and team rules. (R-080 to R-082, R-028)
15. **Phone performance is unknown, and some missions are heavy.**
    - Only emulator numbers exist (15–20 fps under SwiftShader).
    - The ALL03 opening runs at about 8.6 logic frames/s even on desktop.

    (R-170, R-171)

## Full table

**Columns:**
- **Sev:** player-facing severity.
- **Status:**
  - OPEN: not done.
  - PARTIAL: some of it is done.
  - IN PROGRESS: on a running branch.
  - RE-FALLBACK: implemented with a guessed value or behaviour.
  - DATA-LIMIT: the install lacks the data.
  - USER-DECISION: the owner must choose.
- **Eff:** effort, S/M/L.
- **RE:** whether reverse engineering is needed.
- **Source IDs:** the original gap IDs this row de-duplicates.

### In progress on `w4fix-ctl`

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-001 | Every Amphibious* MovementZone and SpeedType=Amphibious is treated as land, so Tanya, SEALs, Commando, JFISH, CPHAL and APC cannot swim. ALL01 is unplayable by real orders, and the ALL09/ALL12 SEALs are affected (W4-MC-01, G-51, W2-SK-22 swim) | Units & abilities | BLOCKER | IN PROGRESS (w4fix-ctl 8778652) | `engine/main.cpp:915-919` exact `=="amphibious"` | S | N |
| R-002 | LockInput/UnlockInput (actions 46/47) are only recorded, so the player can act during scripted intros (W4-MC-02 = W4-SQ-09 = A1-07) | Campaign | MAJOR | IN PROGRESS (w4fix-ctl df2e241) | `engine/map_trigger.cpp:101-102` (label only) | S | N |
| R-003 | The opening view comes from the first unit, not Waypoint[HomeCell]/wp98. `[Map] LocalSize` is ignored, so the black border and the staged off-map units are visible and clickable (W4-MC-03/04, W4-AM-02, R9) | Campaign | MAJOR | IN PROGRESS (w4fix-ctl ab86d75) | no `LocalSize` reader on master (`git grep`) | M | N |
| R-004 | Scripted camera pan (action 48) fights the player's scrolling (W4-MC-05) | Controls/Touch | MINOR | IN PROGRESS (w4fix-ctl 2c0c3f3, RE-FALLBACK rule) | `engine/campaign_visuals.inc` pan loop | S | N |
| R-005 | No control-level play scripts that issue real clicks and touches (W4-MC-07) | Robustness/Release | MINOR | IN PROGRESS (w4fix-ctl 2c0c3f3) | no `scripts/play/ctl-*` on master | M | N |
| R-006 | One-finger drag draws a band box and clears the selection instead of scrolling (W4-AM-01) | Controls/Touch | BLOCKER | IN PROGRESS (w4fix-ctl a51efdf) | `engine/touch_gestures.cpp:77,144-150` | S | N |
| R-007 | Phone view: the canvas is too small (~480 px), and the touch bar covers the bottom 60 px of the playfield (W4-AM-02/05, G-89, 06#37) | Controls/Touch | MAJOR | IN PROGRESS (w4fix-ctl a51efdf) | `engine/app_robust.inc:25-46`, `engine/touch_bar.inc:242-254` | M | N |
| R-008 | Sell, repair, placement and superweapon modes are invisible on touch and block selection (W4-AM-03) | Controls/Touch | MAJOR | IN PROGRESS (w4fix-ctl a51efdf) | `engine/main.cpp:5961-5967` | S | N |
| R-009 | Sidebar textures are not keyed by side, so the 2nd mission in a session shows the previous side's art (desktop too) (W4-AM-04) | UI/Shell | MAJOR | IN PROGRESS (w4fix-ctl c92888a) | `engine/main.cpp:4375` key `"sb:top"` | S | N |
| R-010 | Touch ergonomics. Tooltips stick after taps, hold-to-cancel gives no feedback, and the double-tap slop is about 5 dp. There is no touch path to the briefing (F1) or to radar drag (W4-AM-06/07/09/11, W2-AR-07) | Controls/Touch | MINOR | IN PROGRESS (w4fix-ctl a51efdf) | `engine/hud_tooltip.inc:93-103`, `engine/input_original.cpp:31`, `engine/touch_bar.inc:22-23` | S | N |
| R-011 | The APK versionName carries no commit, so builds cannot be told apart. The main menu shows no engine version (W4-AM-12, G-72, W2-UI-22 part) | Android/Platform | MINOR | IN PROGRESS (w4fix-ctl 567d898) | `android/app/build.gradle:26,64` | S | N |

### In progress on `w4fix-seq`

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-012 | Transport cargo is never loaded, and Load/WaitLoaded (script 14/43) are skipped. ChangeHouse/DeleteMembers then hit the landed force: **SOV02 cannot be won**, and the ALL05 spy-delivery loop makes the spy objective impossible (W4-SQ-01, W2-CA-08, W2-CA-17 part, G-73, 06#25) | Campaign | BLOCKER | IN PROGRESS (w4fix-seq f01cfb9) | `engine/campaign_runtime.inc:1345` `[RE-FALLBACK §9.3]`; `engine/campaign.cpp:446-448` Skip | M | N |
| R-013 | Droppod and paradrop planes are missing: units appear on the ground, and the landing-zone tags fire late (ALL08/09/11, SOV10) (W4-SQ-06/07) | Campaign | MAJOR | IN PROGRESS (w4fix-seq f01cfb9, 958c6a7) | `engine/campaign_runtime.inc:1343-1346` | M | N |
| R-014 | SOV09 is lost at about 107 s with no input, because both Yuris die. Covered by the one-sided alliances and the m-sov09-real script (W4-MC-08) | Campaign | MAJOR | PARTIAL (w4fix-seq 958c6a7, f01cfb9; p2-missions 73a35b1: `m-sov09-real` proves the opening through the first power-plant blackout, not the win -- see R-206) | `docs/gap/wave4/05-mission-sequences.md` SOV09 row, "P2 status" | S | N |
| R-015 | Scripts 46/47 ignore the BuildingTypes argument and selector, so map garrison teams never garrison (ALL03/04/06, SOV03/08) (W4-SQ-02) | Campaign | MAJOR | IN PROGRESS (w4fix-seq 5701807) | `engine/campaign_runtime.inc:1496,1540` | S | N |
| R-016 | Placed objects' `Mission=Sleep/Harmless` is ignored, so the ALL12 prisoners and their guards fight at frame 0 (ALL05 too) (W4-SQ-05 = W2-CA-18 = G-77) | Campaign | MAJOR | IN PROGRESS (w4fix-seq bd46bbc) | `engine/campaign_runtime.inc:940` `place_unit` honours only Hunt | S | N |
| R-017 | TeamType `VeteranLevel` is parsed but never applied (ALL09 elite SEALs) (W4-SQ-04) | Campaign | MINOR | IN PROGRESS (w4fix-seq f01cfb9) | `engine/campaign.cpp:357` read only | S | N |
| R-018 | DestroyTeam is skipped, and finished fixed-wing aircraft loiter. The branch exits them over the map edge as an RE-FALLBACK (W4-SQ-03/08) | Campaign | MINOR | IN PROGRESS (w4fix-seq d8038c9) | `engine/campaign_runtime.inc:1776-1786` | S | Y |
| R-019 | A player order does not release a unit from its script team. RE-W4-MC-2 is still untraced (W4-MC-06) | Campaign | MINOR | IN PROGRESS (w4fix-seq f01cfb9, RE-FALLBACK) | `engine/campaign_runtime.inc` `step_teams` | S | Y |
| R-020 | High bridges cannot be destroyed. Apply100Damage (action 63) only hits the waypoint cell, with no DamageArea, so the ALL01 "Break Bridges" and SOV01 "Break Bridge" set-pieces never happen. The `DestroyableBridges=no` interaction is untraced (W4-SQ-10, A1-06, A1-11, G-49, W2-GA-07 part) | Campaign | MAJOR | IN PROGRESS (w4fix-seq, no commit yet; uncommitted work in its worktree) | `engine/campaign_runtime.inc:1932-1940` `[DEVIATION §9.6]`; `engine/world_depth.cpp:290-360` low bridges only | L | Y |
| R-021 | Trigger actions not implemented: 18 RevealZone (ALL07), 35 PreferredTarget (ALL11 nuke) and 61 TurnOff (ALL10 Chronosphere stays powered) (W4-SQ-11a-c, W2-CA-13 part, G-75 part) | Campaign | MINOR | DONE (w5-camp: 35/61/62; 18 by w4fix-seq) | `engine/map_trigger.cpp:1007` "unimplemented" | S | N |
| R-022 | Events 5 HouseDiscovered (SOV10 "Bad Guy 3 Startup" never runs), 23 TeamLeavesMap and 34 ComesNearWaypoint are always false (W4-SQ-11d, W2-CA-14 part, G-74 part) | Campaign | MINOR | DONE (w5-camp: 23/34; 5 by w4fix-seq) | `engine/map_trigger.cpp:77-84` | S | N |
| R-023 | ChangeHouse (action 14) on a tag-attached building changes nothing (SOV09 "Switch Power House") (W4-SQ-11e) | Campaign | MINOR | IN PROGRESS (w4fix-seq scope, no commit yet) | `engine/campaign_runtime.inc:1796`; `engine/map_trigger.cpp:162` | S | N |
| R-024 | A CreateTeam for a computer house with no stock recruits 0 units and is dropped after 900 frames. Recruiter, Prebuild and Autocreate are not modelled (SOV03 opening harass, SOV12 BG1 rocketeers) (W4-SQ-12, RE-W4-SQ-2) | AI | MAJOR | DONE (w5-camp: Can_Add priority + loaded-transport rule; w4fix-seq stand-in) | `engine/campaign_runtime.inc:~1446` | M | Y |
| R-025 | Campaign alliances are forced symmetric; the original `Allies=` is one-way (06#30) | Campaign | MINOR | IN PROGRESS (w4fix-seq f01cfb9 one-sided alliances) | `engine/campaign_runtime.inc:638-647` | M | N |

### Campaign (open)

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-026 | 9 missions are only PROBABLE: ALL03/04/05/07/09/11/12 and SOV07/12. Their trigger chains were proven with cheat moves, and no real-order play-through exists. Boot Camp (TUT1/TRN) was never verified in play (wave4/05 survey, W2-CA missions, G-78) | Campaign | MAJOR | PARTIAL (w5-camp: SOV01/ALL03 WON by real orders, ALL12 opening; p2-missions 8dfb918: SOV07 WON by real orders (`m-sov07-real`); rest split to new agents; p5-missions: SOV11 + SOV12 WON by real orders (`m-sov11-real`, `m-sov12-real`), SOV10 not won -- tactical blocker, see wave4/05 "P5 status"; p3-missions: ALL05, ALL06, ALL07 WON by real orders (`m-all05-real`, `m-all06-real`, `m-all07-real`), see wave4/05 "P3 status"; p4-missions: SOV04 + ALL10 WON by real orders (`m-sov04-real`, `m-all10-real`), ALL09 not won -- tactical, engine side now complete (barrel chains, crushable walls, Harmless wake, Insignificant counts), see wave4/05 "P4 status") | `docs/gap/wave4/05-mission-sequences.md:74`; `engine/campaign_gate.inc:284` | L | N |
| R-027 | Map-team script actions are still skipped: 55 IronCurtainMe (ALL12, SOV11 ×14), 42 ForceFacing (×42), 10 follow, 19/41 panic, 50, 56/57 and 59. Patrol is a plain move, so patrols do not loop (W2-CA-17 rest, G-76, 04#19) | Campaign | MAJOR | DONE (w5-camp: 10/18/19/41/42/50/55; 56/57/59-64 unused by the 24 maps) | `engine/campaign.cpp:446` `default: Skip`; `engine/ai_team.cpp:18` `[RE-FALLBACK: patrol = move]` | M | Y |
| R-028 | Campaign computer houses: the difficulty binding, map AITriggers versus aimd precedence and the enable semantics are all guessed (G-69, RE-A5, W2-CA-07) | AI | MAJOR | PARTIAL (w5-camp: IQ SW gate, aimd merge per IgnoreGlobalAITriggers, difficulty binding confirmed) | `engine/campaign_runtime.inc:690,723` | L | Y |
| R-029 | Trigger actions still unimplemented: 40, 44, 51 (ALL08), 77/78 and 117 (W2-CA-13 rest, G-75 rest) | Campaign | MINOR | DONE (w5-camp: 40/44/51/70/77/78/89/117) | `engine/map_trigger.cpp:1007` | S | Y |
| R-030 | FireSale (action 9) sells everything at once instead of switching to the original AI state 4, and was logged "skipped" ×3 in ALL10 (W4-SQ-11f) | Campaign | MINOR | DONE (w5-camp: house AI state 4) | `engine/campaign_runtime.inc:1893-1904` | S | Y |
| R-031 | Events 3, 18, 35, 45/46 and 53/54 are hard-coded false. Event 31 AllBridgesDestroyed is never raised; it is used ×7 in ALL03/04/06/10/11 (G-74, W2-CA-14 part) | Campaign | MINOR | DONE (w5-camp: 31/23/34/53/54; 3/18/35/45/46 unused) | `engine/map_trigger.cpp:77-84`; `engine/map_trigger.hpp:35` (enum only) | M | Y |
| R-032 | The ALL01 GI hand-over fires only on a 4-cell strip (61..64,84). Tanya ordered to the west dock swims around it (observed on w4fix-ctl) | Campaign | MINOR | FAITHFUL (w5-camp: the map's own [CellTags] put tag 07BA863C 'Initial Paratroopers 1' on exactly (61..64,84); no RE change) | `docs/gap/wave4/02-mission-control.md` (w4fix-ctl copy) | S | Y |
| R-033 | Placed map objects ignore facing and per-object veterancy (G-77 part) | Campaign | MINOR | DONE (w5-camp: facing + veterancy; SubCell by w5-combat) | `engine/campaign_runtime.inc:940` (`place_unit` has no facing/vet) | S | N |
| R-034 | There is no pre-mission briefing page, only the in-game F1 text overlay. There is no campaign movie path either: Bink is used only by the main menu (wave3/03 §4) | UI/Shell | MINOR | FIXED (w5-audio a8b2068): Restate page 0x10C + Intro/Brief/Win/Lose/trigger movie path (skips stubs) | `engine/campaign_scenes.inc:31-42`; `engine/main.cpp:5009-5016` | M | N |
| R-035 | The ALL04 `Image=BeInvisibleHack` civilian has no SHP, so the unit is dropped and its tag is lost (wave3/01 §2) | Campaign | MINOR | OPEN | `engine/campaign_runtime.inc:151-163` | S | N |
| R-036 | Mind control is guessed: capacity and release rules (W2-CA-15, RE-W2-5) | Campaign | MINOR | PARTIAL (w5-camp: capacity-1 slot rule) | `engine/campaign_runtime.inc:1148-1175` | M | Y |
| R-037 | Guessed trigger plumbing: the SpiedBy raise site (raised alongside EnteredBy), damage-threshold event ratios (38-43), which map-INI rules sections are merged, and the AttackedByHouse attacker (RE-W2-2/3/4) | Campaign | MINOR | RE-FALLBACK | `engine/campaign_runtime.inc:1071,1107,1135`; `engine/campaign_rules.inc:38` | S | Y |
| R-038 | Reinforcement details are guessed: the placement spiral, the Unload argument modes, the paradrop plane type per house (the branch uses PDPLANE) and the entry edge (RE-W4-SQ-1, RE-C1 rest) | Campaign | MINOR | RE-FALLBACK | `engine/world_an.cpp:639`; `engine/campaign_runtime.inc:883,947` | S | Y |
| R-039 | FlashCameo (action 115): the sidebar cameo flash is never drawn (RC-8 115) | Campaign | COSMETIC | FIXED (w5-pres 5d84724) | `engine/campaign_visuals.inc:145` | S | N |
| R-040 | DoExplosionAt (action 42): the WeaponTypes index order is guessed (RC-8 42) | Campaign | MINOR | RE-FALLBACK | `engine/campaign_runtime.inc:522` | S | Y |
| R-041 | SOV05 has no visual for the "energized" Paris Tower (CAPARS01). The objective itself fires (wave3/02 SOV05) | Campaign | COSMETIC | OPEN | `git grep CAPARS01 engine` returns 0 | S | Y |

### Units & abilities

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-042 | Spying on a Battle Lab gives no stolen tech, so the Chrono Commando, Psi-Corps Trooper and Chrono Ivan can never be built (W2-SK-20 part) | Units & abilities | MAJOR | FIXED (w5-abil 06e4827; RE FUN_004571E0; play abil-stolen-tech) | `engine/realplay_economy.inc:146-149`; `engine/world.hpp:1210` (no TechLab infiltrate purpose) | M | N |
| R-043 | Prism Tower chaining (PrismSupport, PrismForwarding) is not modelled (W2-SK-22) | Units & abilities | MAJOR | FIXED (w5-abil faad5ac; RE Mission_Attack/PrismUpdate; play abil-prism-chain) | `git grep PrismSupport engine` returns 0 | M | Y |
| R-044 | The Gap Generator does nothing: no shroud field (G-27, W2-SK-22) | Units & abilities | MAJOR | FIXED (w5-abil faad5ac; RE CreateGap + 120-frame re-shroud; play abil-gap) | `git grep GapGenerator engine` returns 0 | M | Y |
| R-045 | Tesla Troopers cannot charge Tesla Coils (Overpowerable) (W2-SK-22) | Units & abilities | MINOR | FIXED (w5-abil faad5ac; RE Overpower/IsPowerOnline; play abil-tesla-charge) | `git grep Overpowerable engine` returns 0 | M | N |
| R-046 | No Boris airstrike (AirstrikeTeam), and Engineers cannot defuse Ivan bombs (W2-SK-22) | Units & abilities | MINOR | FIXED (w5-abil 53e50e4 defuse, a4ac30c Boris; plays abil-defuse, abil-boris; plane path RE-FALLBACK) | `git grep AirstrikeTeam engine` and `git grep BombDisarm engine` both return 0 | M | Y |
| R-047 | A disguised Mirage is still drawn as the tank, not as a tree (W2-SK-22) | Units & abilities | MINOR | FIXED (w5-abil 53e50e4 sim tree pick + w5-pres 66dc750 tree draw for non-owners) | `engine/world.cpp:1983-2000` | M | N |
| R-048 | The IFV passenger swap changes only damage, ROF, range and Verses; the projectile and AA/AG flags stay. The Engineer IFV RepairBullet still targets enemies (W2-SK-26 part) | Units & abilities | MINOR | FIXED (w5-abil 53e50e4: AA/AG/projectile follow the swap; RepairBullet heals allies; play abil-ifv-repair) | `engine/world_abilities.cpp:261-269` | S | N |
| R-049 | The Giant Squid grab (ParasitePlus) is not modelled (W2-AN-15 part) | Units & abilities | MINOR | PARTIAL (w5-abil 9aefae3: ParasitePlus paralysis/culling on the P4 parasite; the squid never fires at a ship in play -- combat.cpp aim gate, agent B) | `docs/gap/wave2/02-air-naval-sw.md` §5 row 15 | M | Y |
| R-050 | `Crush=1.8`, AutoCrush and PlayerAutoCrush steering are missing; vehicles crush only what they drive into (G-47 part, RE-W6) | Units & abilities | MINOR | FIXED (W5-B) | `engine/combat.cpp` crush steering (FUN_00742720/FUN_007402F0), friendly scatter; `scripts/play/w5-crush.play` | M | Y |
| R-051 | Transport `DeployTime` delay not modelled (03 transports) | Units & abilities | MINOR | FIXED (w5-abil 53e50e4; RE FUN_004A5150 DeployTime*900) | `engine/world.cpp:1423` | S | N |
| R-052 | Allies cannot board each other's transports | Units & abilities | MINOR | FIXED (w5-abil 06e4827; can_load takes IsAlliedWith; both control schemes) | `engine/cargo.cpp:31`, `engine/cargo.hpp:36` | S | N |
| R-053 | Garrison combat: Assaulter clearing of garrisons and DistributedFire are missing. Garrison shots are instant hits (README:3385) | Units & abilities | MINOR | OPEN | README:3385; `engine/world_depth.cpp:262` `[RE-FALLBACK]` | S | Y |
| R-054 | Jump projectiles are not drawn (Terror Drone, dog, Squid leap); they stay scripted abilities (W4-PJ-14) | Units & abilities | MINOR | OPEN | `engine/projectile.hpp:35` (no jump kind) | M | N |
| R-055 | V3 single-shot: the root cause (spawn slot not freed) is only "probable". 1edba43 also changed spawn regen (W4-PJ-12) | Units & abilities | MINOR | PARTIAL | `docs/gap/wave4/04-projectiles.md` status table | S | N |
| R-056 | The Tech Airport (CAAIRP) paradrop is missing. The generic ParaDrop effect and the American paradrop work (W2-AN-17) | Units & abilities | MINOR | FIXED (w5-abil faad5ac; Capturable+NeedsEngineer map structures are World buildings; play abil-tech-buildings) | `git grep CAAIRP engine` returns 0 | S | N |
| R-057 | FlakProj impact scatter and the Inviso cliff/wall trace are not modelled (no SubjectToCliffs/Walls). Flak MinimumRange is also missing (W4-PJ-15) | Units & abilities | MINOR | PARTIAL (W5-B) | FlakProj impact scatter + cliff trace done (combat.md §9/§10); wall trace not modelled | S | Y |

### Gameplay / simulation

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-060 | Splash damage hits enemy units only: never buildings, never own units (no friendly fire), with a linear falloff (G-50 splash, RE-W7) | Gameplay/sim | MAJOR | FIXED (W5-B) | `engine/combat.cpp` damage_area/modify_damage (FUN_00489190/FUN_00489090, docs/re/combat.md §8); `scripts/play/w5-splash.play` | S | Y |
| R-061 | One infantry per cell, where the original packs 3 sub-cell spots. `[Infantry] SubCell` is parsed but unused, and the spot table has not been found (W4-POS P3a, 03 infantry walk) | Gameplay/sim | MAJOR | FIXED (W5-B) | `engine/occupancy.hpp` spots (FUN_00481090, DAT_0089D960); Unit::sub_x/sub_y for render; `scripts/play/w5-subcell.play` | L | Y |
| R-062 | No `[LandType]` speed multipliers (road, rough, beach). Shore passability works at tile-set granularity (03 terrain speed) | Gameplay/sim | MINOR | FIXED (W5-B) | `engine/world.cpp` land_speed_mult (FUN_00673A40/FUN_004B2550); `scripts/play/w5-terrain.play` | S | N |
| R-063 | Range and splash use 2-D distance, not 3-D; there is no shared Z scale (03 range) | Gameplay/sim | MINOR | FIXED (W5-B) | `engine/combat.cpp` weapon_in_range (FUN_006F6750/FUN_006F6490/FUN_004CC020, combat.md §9); walls not traced | M | N |
| R-064 | Idle units engage only armed structures. The guard acquisition radius and leash (`kGuardStray`) are guessed (G-46, RE-W4/W5, W2-SK-25 Q3) | Gameplay/sim | MINOR | PARTIAL (W5-B) | Evaluate_Object FUN_006F71D0: computer units may pick unarmed buildings, human only armed; guard radius/leash still RE-FALLBACK | S | Y |
| R-065 | A prerequisite counts only once the building is Idle; in the original it counts once placed (G-31, RE-E1) | Gameplay/sim | MINOR | FIXED (W5-B) | `engine/production.cpp` counts from placement (Unlimbo FUN_006F61D0 -> RegisterGain FUN_005028A0) | S | Y |
| R-066 | Country and FactoryPlant cost multipliers are fixed at 1.0. NAINDP is YR-only and has no art (G-16/G-27) | Gameplay/sim | MINOR | OPEN | `engine/world.cpp:1822`; `engine/repair_sell.cpp:39` | S | Y |
| R-067 | Trees and terrain objects are inert: they cannot be shot, burn or fall (RC-11) | Gameplay/sim | MINOR | OPEN | `engine/pres_map.inc:20-28` (dressing only) | M | Y |
| R-068 | The `[Map] LocalSize` camera clamp is not applied in skirmish; the ctl branch does campaign only (W4-MC-04b) | Gameplay/sim | MINOR | OPEN | `docs/gap/wave4/02-mission-control.md` status (ctl) | S | N |

### AI

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-080 | The AI never fields fixed-wing aircraft (Harrier, Black Eagle) (W2-AN still-open, 04#17) | AI | MINOR | FIXED (w5-abil 7818961; ORCA/BEAG in the AI roster, built onto airfield pads; play abil-ai-air) | `engine/world.cpp:3047` skips `fixed_wing`; `engine/ai_skirmish.inc:32` | M | N |
| R-081 | The skirmish AI never garrisons buildings. Deploy and base-defence responses work (W2-SK-10) | AI | MINOR | FIXED (w5-abil 7818961, RE-FALLBACK choice rule; play abil-ai-air) | no AI caller of `order_occupy` | M | N |
| R-082 | Difficulty pacing is not calibrated against an original capture. The Armor and Groundspeed handicaps are not applied, so Brutal units are not tougher or faster (RE-Q5, G-63 part) | AI | MINOR | RE-FALLBACK (w5-abil: AssignHandicap FUN_004F6DD0 traced; ReceiveDamage divides by the COUNTRY armor mult, no reader of house Armor/Groundspeed found -- left unapplied, docs/re/abilities-w5.md §12) | `engine/ai_base.hpp:101-105` | M | Y |
| R-083 | AI behaviour fallbacks (about 55 markers): xorshift RNG instead of the lockstep RNG, wall ring and placement scan, team tie-break, base-defence triggers dropped, target picker proxy, moves abandoned after 10 s, 60 s boarding cap (RE-A4 and others) | AI | MINOR | RE-FALLBACK | `engine/ai_base.cpp:9,48,176`; `engine/ai_team.cpp:157,309,385`; `engine/ai_trigger.cpp:113`; `engine/world.cpp:2587,3093,3919` | L | Y |
| R-084 | On May Day (2 ore mines) AI-vs-AI ends with no winner after 40 min. This may be faithful | AI | MINOR | OPEN | `docs/gap/wave2/03-skirmish-match.md` §7.3 | S | Y |

### Air / Naval / Superweapons

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-090 | Air, naval and superweapon behaviour is guessed in several places: (W2-AN-02/04/07/11/13/21/26, Q3-Q8, Q10) | Air/Naval/SW | MINOR | RE-FALLBACK | `engine/world_sw.cpp:376,414,515`; `engine/world_an.cpp:21,161,478,607`; `engine/main.cpp:757` | M | Y |
|  | Superweapons: the Iron Curtain kills infantry, the Chronosphere wrong-domain kill, the Lightning Storm bolt scheduler, and SW cameo order | | | | | | |
|  | Aircraft: fixed-wing climb, move-order flight and attack runs, crash damage of Strength/4, jumpjet speed using the ground bias | | | | | | |
|  | Naval: the Naval Yard repairs for free on a guessed cadence; sub cloak and sonar are guessed | | | | | | |

### Graphics

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-100 | High-bridge decks draw in the ground pass, and there is no bridge layer for units on or under a deck. Rockets have no bridge layer either (W2-GA-07 layer, G-49) | Graphics | MINOR | FIXED (w5-pres add7b84: deck layer in the depth sort, on-deck lift; RE-FALLBACK for the Z-buffer) | `engine/bridges.hpp`; `engine/rocket.cpp:143` | L | Y |
| R-101 | There is no render layer system: aircraft and Kirovs are mixed into the ground depth sort, walls are in the ground pass, ground FX are not depth-sorted, and the building-anim ZAdjust/YSort are unused (W2-GA-17, W2-GA-14, RE-Q2/Q8/Q10/Q13) | Graphics | MINOR | PARTIAL (w5-pres add7b84: air layer; building-anim YSort still with the building) | `engine/main.cpp:3545-3590`; `engine/pres_fx.inc:290,548-551` | M | Y |
| R-102 | The placement grid is 1-px SDL diamond outlines, not `PLACE.SHP` (W2-GA-18, 01#19) | Graphics | MINOR | FIXED (w5-pres 6e2e616) | `engine/main.cpp:4559-4590` | S | N |
| R-103 | The house remap is a heuristic HSV ramp from 8 hard-coded YR colour triples (W2-GA-20) | Graphics | MINOR | FIXED (w5-pres 7deb00d) | `engine/world.cpp:327-340` | M | Y |
| R-104 | Lighting is incomplete: no ion-storm or Dominator lighting, no per-height brightening on objects, `Powered` lamps not checked, and light clamps at 1.0 (W2-GA-05) | Graphics | MINOR | PARTIAL (w5-pres 658663c: >1.0 brightening, per-height object light; ion storm / dominator lighting open) | `engine/pres_light.inc:18-25` | M | Y |
| R-105 | The lobby offers 27 YR DESERT/NEWURBAN maps drawn with temperate art, and 3 lobby maps carry SCHP with no voxel (W2-GA-09, wave3/03 §1.1) | Graphics | MINOR | FIXED (w5-pres d31b548) | no theater filter in `engine/maplist.cpp`; `engine/main.cpp:139-141` | S | N |
| R-106 | The shell is the 640x480 "S" asset set scaled to 1280x720, so it looks blocky. There are no 800x600 "L" assets (MNSCRNL/RA2TS_L) and no 9-row grid (W2-UI-28) | Graphics | MINOR | OPEN | `engine/main.cpp:4795`; no MNSCRNL references | M | Y |
| R-107 | Fog of war is off by default (`IsLocationFogged` is a stub), the BlendedFog compose is untraced, and explored cliff extras are not clipped against neighbouring shroud (G-84 part, W2-GA-02) | Graphics | MINOR | RE-FALLBACK | `engine/world.hpp:1431-1442,1956`; `engine/pres_shroud.inc:15` | M | Y |
| R-108 | Shadows are guessed: the voxel shadow is a z-flattened 50% silhouette with no light offset, the infantry shadow is index 12 at 50%, and the aircraft shadow is a dark diamond (W2-GA-12, RE-Q9) | Graphics | COSMETIC | PARTIAL (w5-pres: airborne voxel silhouette shadow; light offset open) | `engine/voxel_raster.hpp:52`; `engine/iso_renderer.hpp:194` | S | Y |
| R-109 | Building sprites that cross into unexplored black cells draw over the black (wave3/01 §3, not re-checked since the shroud post-pass) | Graphics | MINOR | FIXED (verified on w5-pres: the shroud post-pass covers building sprites over unexplored cells, play-out pres-place) | `engine/main.cpp:3616` | S | N |
| R-110 | Positioning leftovers: units step whole height levels on ramps (no continuous z), the tile-anim (waterfall) origin and the aircraft altitude-to-pixel constant are untraced, and the high-bridge Y offset uses the BRIDGE* overlay family as a proxy (W4-POS P3b/c, ramps, RE-Q6) | Graphics | COSMETIC | RE-FALLBACK | `engine/flight.cpp:36-40`; `engine/pres_map.inc:134-140,272` | M | Y |
| R-111 | The theater name munge goes by name pattern, not `NewTheater` (1 lobby NAPSIS object, possibly flags and guard shacks) (RC-1c, RE-W3-1) | Graphics | COSMETIC | OPEN | `src/theater_name.cpp:19-31` | S | Y |

### Animation / FX

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-120 | Missing state effects: no Iron Curtain tint on protected objects, no chrono warp on the teleported units (only the ChronoBlast/Dest anims), and no elite promotion flash (the sound does play) (W2-GA-15, 05 promotion) | Animation/FX | MINOR | FIXED (w5-pres 0fbc9a4) | `engine/pres_game.inc:161,524-531`; `iron_left` read only in `engine/world_an.cpp:136,143` | M | Y |
| R-121 | Defence turrets snap to the target (no turret ROT), and TurretRotateSound is not played (W2-GA-13) | Animation/FX | MINOR | FIXED (w5-pres 5c32f44; TurretRotateSound to the audio owner) | `engine/pres_turret.inc:10-11` | S | Y |
| R-122 | The paratrooper `PARACH` parachute anim is never drawn; the w4fix-seq branch adds only the fall (wave3 §3.3) | Animation/FX | MINOR | FIXED (w5-pres b4db729) | `git grep -i parach engine` returns 0 | S | N |
| R-123 | AnimClass keys not read: PingPong, Reverse, Spawns/SpawnCount, IsMeteor, MakeInfantry, Damage and Normalized (wave3/03 §3.6) | Animation/FX | MINOR | PARTIAL (w5-pres d9974b6: PingPong/Reverse/Normalized/RandomRate/IsMeteor/Spawns; MakeInfantry has no RA2 user; anim Damage cosmetic) | `engine/pres_fx.inc:450-490` `fill_type` | S | N |
| R-124 | Small guessed animation rules (RE-Q7/Q13, W3-B, W4-PJ-13): | Animation/FX | COSMETIC | RE-FALLBACK | `engine/pres_bldanim.inc:227-228,359-363` | S | Y |
|  | Building damage fires start from index 0; the SuperAnim state order and GateCloseDelay timing are guessed | | | | | | |
|  | The refinery storage level follows the money share; the ring mark on the launch frame is unexplained | | | | | | |
|  | Gas particle damage and debris damage are deliberately cosmetic | | | | | | |

### Audio

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-130 | Many unit and world sounds are never played (W2-GA-25, G-81 part): | Audio | MAJOR | FIXED (w5-audio 29c08e0): RE-traced triggers for every listed sound; `audio-events.play` | grep counts in `docs/gap/wave2/05-gfx-audio-perf.md`; `engine/gf_scene.inc:122` | M | Y |
|  | Voices: VoiceFeedback, VoiceEnter, VoiceCapture, VoiceHarvest, VoiceSpecialAttack | | | | | | |
|  | Unit and building sounds: Undeploy, DeploySound (only the AMCV has one), Sinking, Crashing, TurretRotate, BuildingRepaired, BuildingGarrisoned | | | | | | |
|  | Other: Crate* pickups, CreditTicks | | | | | | |
| R-131 | About 12 EVA lines never fire: PrimaryBuildingSelected, NewRallyPointEstablished, BuildingOffLine/OnLine, UnitRepaired, TechBuildingCaptured/Lost, BridgeRepaired, UnableToComply, the Armor/Firepower/Speed upgrades and BuildingInfiltrated* (W2-GA-26, G-80 part) | Audio | MINOR | FIXED (w5-audio 29c08e0; w5-audio2 7e0d1ed BuildingOffLine/OnLine via the power-toggle mode, a7a6018 BuildingInfiltrated* per FUN_004571E0) | the 35-name EVA set contains none of them | S | N |
| R-132 | Sound Control `Delay`/PREDELAY, `AMBIENT` and `Type=SHROUD/LOCAL` are not implemented (W2-GA-24) | Audio | MINOR | FIXED (w5-audio 29c08e0): Delay/PREDELAY/AMBIENT, Type LOCAL/SHROUD, on-screen dead zone | `engine/audio_mixer.*`, `engine/pres_audio.inc` | M | N |
| R-133 | The (frame, sound) tail of infantry sequences is not parsed (wave3 §3.3) | Audio | MINOR | FIXED (w5-audio2 ffec644: `<Seq>Sounds=` per FUN_00523A30; only the YR T-Rex uses it) | `engine/inf_anim.cpp` (no sound handling) | S | N |
| R-134 | Stereo IMA-ADPCM BAG entries are unsupported (only uslide1/2 are affected). Bink audio tracks are not parsed (the menu CRT is silent) | Audio | COSMETIC | FIXED (w5-audio2 7ec5796: stereo ADPCM + Bink Audio RDFT/DCT, checked against ffmpeg; the install's CRT .bik has no audio track) | `src/audio_bag.cpp:137`; README:1647 | S | N |

### UI / Shell

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-140 | No hotkey remap page (Options 0xA3). The Keyboard button is greyed on the shell and in-game pages; the bindings themselves are live from KeyboardMD.ini (W2-UI-27, 06#16/#34) | UI/Shell | MINOR | FIXED (w5-audio 43e3fe9): Keyboard page 0xA3, `keyboard.play` | `engine/main.cpp:7283-7288`; `engine/ingame_pages.inc:24-25` | M | N |
| R-141 | Score page: the Score formula is untraced, and the campaign Results page (0xBC7) has no graph well and no first/second place panel (W2-UI-20, G-02, RE-I8) | UI/Shell | MINOR | FIXED (w5-audio 339c780): original Score + 0xBC7 graph [RE-FALLBACK: graph art], `score-pages.play` | `engine/gf_result.inc:5-11`; `engine/campaign_scenes.inc:223-228` | M | Y |
| R-142 | Sidebar polish is missing or guessed (W2-UI-05/06/10, 01#14, 02#9): | UI/Shell | COSMETIC | PARTIAL (w5-pres 49d53b4: credits roll/colour, tab flash; CreditTicks sound to the audio owner) | `engine/main.cpp:4365-4372,4427-4460,5786-5792` | S | Y |
|  | No DARKEN flash on a new cameo, and no tab flash (`tab0N` frames 3/4) | | | | | | |
|  | The credits counter colour, roll rate and tick sound are guesses | | | | | | |
| R-143 | No shell page *leave* transition (SDBTNANM 11..16) and no main-menu header shimmer (W2-UI-24/25, SK-33) | UI/Shell | COSMETIC | PARTIAL (w5-audio2 eaeea68: leave band 5..10 / 11..16 per FUN_00606980; header shimmer still open) | `engine/main.cpp:7382-7386` | S | N |
| R-144 | The rally point and primary-building marker is a placeholder cyan X and triangle (W2-UI-30, 01#32) | UI/Shell | COSMETIC | FIXED (w5-pres 6e2e616, a7e43ec) | `engine/main.cpp:4677-4690` | S | Y |
| R-145 | Guessed UI details (W2-UI-07/11/18, SK-16/30/32, Q3/Q4/Q7/Q8/Q9): | UI/Shell | COSMETIC | RE-FALLBACK | `engine/hud_tooltip.inc:11`; `engine/hud_pips.inc:21`; `engine/campaign_page.inc:16`; `docs/gap/07-skirmish-page.md` §8 | S | Y |
|  | HUD: tooltip delay and box art, building pip-line lift | | | | | | |
|  | Campaign page: emblem hover art | | | | | | |
|  | Lobby: dropdown art drawn as primitives, no slider tick sound, preview scaling, ScenIndex mapping | | | | | | |
| R-146 | DetailLevel is saved but nothing uses it, and the resolution combobox is inert (06#16) | UI/Shell | MINOR | FIXED (w5-audio: DetailLevel + resolution combobox; w5-audio2 9d1efa2: See Hidden Objects draws the Behind= anim per FUN_0070E600 / FUN_00423AE0) | `engine/settings.cpp:240`; `engine/main.cpp:7147` | S | N |
| R-147 | Enemy units cannot be selected for info (HP); the original behaviour is not confirmed (01#6) | UI/Shell | COSMETIC | OPEN | `engine/input.cpp:295` | S | Y |
| R-148 | A disguised object's real owner: the Spy and Mirage detect radius is guessed | UI/Shell | COSMETIC | RE-FALLBACK | `engine/capture.hpp:62` | S | Y |

### Controls / Touch

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-150 | Hotkeys that are parsed but do nothing: CombatantSelect, Follow, VeterancyNav, PlaceBeacon, AllToCheer and PlanningMode. The PowerToggle, Planning and Beacon modes are not modelled; the beacon, alliance and taunt keys are swallowed (W2-SK-28) | Controls/Touch | MINOR | PARTIAL (w5-audio 43e3fe9): Follow/CombatantSelect/Health+VeterancyNav/AllToCheer/CenterOnRadarEvent/ScreenCapture done; w5-audio2 7e0d1ed: PowerToggle mode (no UI arms it in YR 1.001; --play cheat + trigger hook); PlanningMode open; beacons/alliances/taunts network-only (RE) | `engine/hotkeys.cpp:14-22`; `engine/input.cpp:225-227` | M | N |
| R-151 | Touch targets on phone: shell campaign and load list rows are about 14 dp, and the sidebar tabs about 21 dp (an art limit). View bookmarks have no touch path (W4-AM-08 rest, W2-AR-07 part) | Controls/Touch | MINOR | PARTIAL (w5-android: campaign list snaps a finger to the nearest row; sidebar near-miss snap from w4fix-ctl; save/load list rows and the ~21 dp tab width remain) | `engine/sidebar.hpp:31,40` | S | N |
| R-152 | IME: the SAVE button sits above the visible area while typing, and prefilled text is not selected on focus (W2-AR-15) | Controls/Touch | MINOR | FIXED (w5-android: IME rect spans SAVE..edit, SAVE stays above the keyboard on the emulator; prefilled text selected, typing/Backspace replace it) | `engine/app_robust.inc:177-188`; `engine/ingame_pages.inc:67` | S | N |
| R-153 | SDL clamps touches in the shell's pillarbox bars to the viewport edge (harmless today) | Controls/Touch | COSMETIC | OPEN | SDL `SDL_render.c:825-830` | S | N |

### Android / Platform

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-160 | Setup UX: the text never says to pick a sub-folder (the picker refuses Download itself), there is no `EXTRA_INITIAL_URI`, and the phone package has no README (W4-AM-10) | Android/Platform | MINOR | FIXED (w5-android: setup text names the RA2 sub-folder rule, EXTRA_INITIAL_URI opens Download/RA2, picking the parent folder also works; docs/PHONE-README.txt for the package) | `android/app/src/main/res/values/strings.xml:9` | S | N |
| R-161 | Exit stays enabled during the import copy, which runs on an unowned thread and can leave `*.part` files (W4-AM-10b) | Android/Platform | MINOR | FIXED (w5-android: Exit/Back disabled while copying, owned copy thread cancelled on destroy, *.part cleaned at start and on failure) | `SetupActivity.java:144,173-174` | S | N |
| R-162 | A mid-game panel resize (foldables, split-screen) keeps the old canvas, pillarboxed; there is no SIZE_CHANGED re-layout (W4-AM-13) | Android/Platform | MINOR | FIXED (w5-android: SDL_WINDOWEVENT_SIZE_CHANGED -> GameScene::relayout_canvas; emulator 974x548 -> 1218x548 mid-game) | no `SIZE_CHANGED` handler in `engine/` | M | N |
| R-163 | Audio focus is never requested, so calls and other apps do not duck or pause the game (W2-AR-19) | Android/Platform | MINOR | FIXED (w5-android: AUDIOFOCUS_GAIN, USAGE_GAME; loss/transient -> mute, can-duck -> 20 %; emulator call: gain 0.00 then 1.00) | no `requestAudioFocus` in `android/` or `engine/` | S | N |

### Performance

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-170 | Phone performance is unknown: there are no real arm64 fps or load-time numbers. The emulator floor is 15.4 fps on ALL01 at 1080p and 19.6 fps on SOV01, with RSS 405 MB and about 6 s per mission load (W2-AR-29/Q2, W4-AM perf) | Performance | MAJOR | PARTIAL (w5-android: perf overlay (3-finger tap / Ctrl+Shift+F / debug.ra2.perf) + [perf] log + INSTALL.md section 5 "how to measure"; emulator ALL01 14.0 -> ~17 fps; still no real arm64 phone numbers) | `docs/gap/wave4/06-android-missions.md:139-142,195` | S | N |
| R-171 | The ALL03 opening runs at about 8.6 logic frames/s (many idle infantry) and has never been profiled (W4-SQ-14) | Performance | MAJOR | FIXED (w5-android: profiled with RA2_PROFILE -- 86 % in A* for unreachable approach re-requests; content-validated path memo: ALL03 ~11 -> ~100 logic frames/s headless, play_hash identical) | `docs/gap/wave4/05-mission-sequences.md:94` | M | N |
| R-172 | 15 Hz fixed logic with no render interpolation. 8-player desktop logic runs 290-400 frames/s headless (W2-GA-P3) | Performance | MINOR | RE-FALLBACK (w5-android: not interpolated -- the original renders once per logic frame (gamespeed.md); render cost cut instead: light map cached, 62 % of an emulator frame) | `engine/main.cpp:5702-5710` | M | N |
| R-173 | Bink decodes on the main thread, does not pause on focus loss, and does not restart the CRT per page (README:1647) | Performance | COSMETIC | FIXED (w5-audio 3021f9e: pauses on focus loss, continues across pages; w5-audio2 2acfe10: decoding on a worker thread, engine/bink_async.hpp) | `src/bink.cpp` | M | N |

### Robustness / Release

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-180 | The production backlog and hold state are not written to save games | Robustness/Release | MINOR | FIXED (w5-android: QBKL save section, format v17) | `engine/game_scene_controls.inc:238` `[RE-FALLBACK]` | S | N |
| R-181 | `load_world` has no index range checks; the CRC-32 trailer catches random corruption only (W2-AR-20 residual) | Robustness/Release | MINOR | FIXED (w5-android: SaveAccess::valid_world range-checks unit/building/house/type/grid indices after load) | `engine/savegame.cpp:56-67` | S | N |
| R-182 | The Windows exe is a console-subsystem app (no `WIN32_EXECUTABLE`), so a double-click opens a console (W2-AR-22 residual) | Robustness/Release | MINOR | FIXED (w5-android: ra2game.exe WIN32_EXECUTABLE from the same objects; log to %APPDATA%/ra2engine/RA2/ra2engine.log) | `CMakeLists.txt:368-372` | S | N |
| R-183 | Stale play scripts. `probe-bridge` uses the removed `map:+` lobby target and `probe-flow` expects a ConYard at start; both fail (re-checked in this audit). `robust-missing` needs a stripped install via `RA2_INSTALL_DIR` | Robustness/Release | MINOR | FIXED (w5-android: probe-bridge/probe-flow pass; robust-missing skips on a full install, passes on a stripped one; --controls-selftest retired) | `scripts/play/probe-bridge.play:9`, `scripts/play/probe-flow.play:6`; README:3789 | S | N |
| R-184 | No raw SDL input record/replay (`RA2_INPUT_RECORD`/`REPLAY`) for reproducing player bugs (W4-MC-09) | Robustness/Release | MINOR | FIXED (w5-android: --record/--replay [--hidden], RA2_RECORD; replay of ctl-sov01 reproduces probe_hash d621403ea1f8ffb0) | no hits in `engine/` or `src/` | M | N |
| R-185 | Map dressing with no art is dropped and its cell left unblocked (0 cases on RA2 maps today) | Robustness/Release | MINOR | PARTIAL | `engine/pres_map.inc:66,93,118,191` | S | N |

### Fidelity guesses (cross-cutting `[RE-FALLBACK]` groups not already listed)

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-190 | Economy and harvest timing (about 20 markers): harvester scan order and rescan cadence, gulp and unload timing, the QueueingCell dock geometry, ore spread order. Income and miner pathing differ slightly (W2-SK-03/04 Q2/Q6) | Fidelity-guess | MINOR | RE-FALLBACK | `engine/harvest.cpp` (12 markers), `engine/ore.cpp`, `engine/realplay_economy.inc:58` | M | Y |
| R-191 | Combat and weapon edge rules: SelectWeapon body, BulletType AA/AG defaults, shrapnel targets, muzzle heights, rocket intercept cone, torpedo level, 8-way muzzle rounding (W4-PJ, W3) | Fidelity-guess | MINOR | RE-FALLBACK | `engine/weapon.hpp:45`; `engine/weapon_resolve.cpp:25,110`; `engine/combat.cpp:139`; `engine/rocket.cpp:77,200` | M | Y |
| R-192 | Skirmish start positions use a map-seeded LCG, not the session RNG. Lobby-slot-to-start randomisation and the first-run America/Gold defaults are guessed (RE-C2) | Fidelity-guess | MINOR | RE-FALLBACK | `engine/gf_start.inc:19`; `engine/skirmish.hpp:39` | S | Y |
| R-193 | Unit art sequences come from `artmd.ini` and stats from `rulesmd.ini` (YR), not RA2's `art.ini`/`rules.ini`. The roster therefore carries YR content: Guardian GI, Boris, Battle Fortress, Robot Tank and Siege Chopper exist, and RA2-only details differ. Depends on U-1 (wave3/03 §2, P10 cuts) | Fidelity-guess | MINOR | OPEN | `engine/main.cpp:1315,1364,1903-1915` | M | N |
| R-194 | Controls timing: right-drag scroll at 1:1, scroll acceleration reset, and GameSpeed 0 capped at 60 fps (the original is uncapped). The touch mapping is invented, since the original has none | Fidelity-guess | COSMETIC | RE-FALLBACK | `engine/input_original.cpp:312`; `engine/gamespeed.hpp:11`; `engine/touch_gestures.hpp:7` | S | Y |
| R-195 | World misc: walls are 1x1 buildings, `NumberImpassableRows` is not modelled, the unit exit facing, crate drop cell and bridge restore rule are guessed (P6 deviations) | Fidelity-guess | MINOR | RE-FALLBACK | `engine/roster2.hpp:38`; `engine/world.hpp:741`; `engine/world_veterancy.cpp:195`; `engine/world_depth.cpp:359,416` | M | Y |
| R-196 | Asset search: YR `ra2md` archives are searched as a fallback tail so that YR-only art appears; SNOW GENERIC.MIX is skipped | Fidelity-guess | COSMETIC | RE-FALLBACK | `src/asset_fs.cpp:24,53,58` | S | Y |

### Data limits

| ID | Item (source IDs) | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-200 | No movies. MOVIES01/02.MIX on this install are 4-byte stubs, so there are no intro, briefing or win videos and no in-game movies (actions 10/100/116/117; ALL01 67/68/69). Play Movies and Sneak Peeks are greyed. Overlaps U-5 (W2-UI-26, A1-10, W4-SQ-13, 06#15, R8) | Data-limits | MINOR | DATA-LIMIT | `engine/main.cpp:7278`; `docs/gap/wave4/03-all01-opening.md:274` | – | N |
| R-201 | YR-only art is absent from the RA2 archives: NABNKR and NAINDP are unbuildable, SCHP has no voxel under the clone's naming, and the CASIN01E/S/W stop signs have no file (the original draws nothing either) | Data-limits | COSMETIC | DATA-LIMIT | `src/asset_fs.cpp:58`; `docs/gap/wave3/03-asset-anim-systems.md` census | – | N |

### Mission-proof agent P3 (branch `p3-missions`)

| ID | Item | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-P3-1 | A C4 sapper (Tanya/SEAL) measured plant range to the nearest foundation cell and approached a cell next to the foundation cell nearest itself: behind a fence it stood still forever, and a wall-ringed building (ALL05 silos) was unreachable. The original measures any weapon vs a building to its centre + (w+h)*64 (FUN_006F6750), Sapper CellRangefinding from the cell | Units & abilities | BLOCKER (ALL05) | FIXED (p3-missions 89ed113) | `engine/combat.cpp` building branch (c4_reach_from) | S | N |
| R-P3-2 | Destroyed*All events counted Insignificant/DontScore objects (flags, lamps, gates, guard shacks), so ALL06's BadGuy1 could never be "all destroyed"; RegisterGain/Loss FUN_004FF520/004FF370 skip them | Campaign | BLOCKER (ALL06) | FIXED (p3-missions 4a51b4b) | `engine/campaign_runtime.inc` tracked() | S | Y (done) |
| R-P3-3 | A parasite (dog, terror drone) whose host left the world by a path bypassing the ability death pass stayed carried forever, unkillable and counted for its house | Units & abilities | MAJOR | FIXED (p3-missions e2d2cc4) | `engine/world_abilities.cpp` parasite pass | S | N |
| R-P3-4 | ALL05: in failed real-order attempts Tanya died and the mission stayed RUNNING (the map loses via `Tanya Dies` -> `Mission Failed`); the tag hand-over when `Change Tanya House` re-creates her was not checked | Campaign | MINOR | OPEN | scratch runs, wave4/05 "P3 status" | S | N |
| R-P3-5 | The clone's building pick is ground-based (footprint cells only); clicking a building's upper art does not target it, and a big army standing round a building covers its footprint so a click selects an own tank. The play harness works round it (`building ...:near`); a player would hit the same wall | Controls | MINOR | OPEN | `engine/input.cpp` building_under | M | Y |

### Mission-proof agent P2 (branch `p2-missions`)

| ID | Item | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| R-202 | "Capturing an enemy factory unlocks that side's units" (the W5-A SOV09 blocker). `CanBuild` FUN_004F7780 tests HouseClass +0x2C4..+0x2D0 against the Owner mask, but the only writer in `gamemd.exe` (and of +0x208..+0x214 in RA2 `game.exe`) is the HouseClass constructor (zero) -- the bits are never set, so a captured factory never unlocks foreign Owner= types. The clone's country-filtered roster already matches | Campaign | – | NOT A GAP (verified by RE) | `docs/gap/wave4/05-mission-sequences.md` "P2 status" RE note | – | Y |
| R-203 | A damaged power plant delivered full output (the original scales by health, `ftol(Health/Strength*PowerBonus)`, FUN_0044E7B0) | Economy | MAJOR | FIXED (p2-missions b0cd134) | `engine/production.cpp` power_totals; `tests/test_production.cpp` | S | Y |
| R-204 | A unit mind-controlled by the player kept running its old team script (FootClass::SetOwningHouse FUN_004DBDE0 removes it from the team) | Campaign | MAJOR | FIXED (p2-missions bac04a4) | `engine/campaign_runtime.inc` poll_mind_control | S | Y |
| R-205 | A Yuri ordered to mind-control a target about 8.5 cells away did nothing. Root cause (p2b): the script re-clicked the ALREADY-selected Yuri, which deploys him (deploy cursor), and a deployed DeployFire unit holds its post and never closes in -- the original does the same for a click on a selected deployable unit. Separately, the capture test (2-D <= range + 0.5) disagreed with the fire test near the edge (elevation bonus); it now uses the same FUN_006F6750 3-D range rule (the capture rides the shot: CaptureManager FUN_00471C80 is called only from the bullet detonation @0x4692C0). Play test `scripts/play/mc-range.play` | Campaign | MINOR | FIXED (p2b-missions) | `engine/campaign_runtime.inc` poll_mind_control | S | Y |
| R-206 | SOV08 and SOV09 have no real-order win (tactics, no engine blocker found). SOV08: the base holds (sentries before the refinery stop the paradrops) but ~16 Apocalypse tanks only take the White House to ~800/1000 at the Tesla-covered gate (`scripts/play/gen/mk_sov08_attempt.py`). SOV09: power reaches 742/575 (north plant by SEAL C4, (68,66) damaged by the IFV); the other plants are inside the Prism compound and the roaming SEALs kill every escort | Campaign | MAJOR | OPEN | `docs/gap/wave4/05-mission-sequences.md` "P2b status" | L | N |

### User decisions

| ID | Item | Category | Sev | Status | Evidence | Eff | RE |
|---|---|---|---|---|---|---|---|
| U-1 | **Data set.** The engine requires the YR files (`rulesmd.ini`, `ra2md.mix`, `multimd.mix`), so an RA2-only install cannot start. It also plays YR's Allied/Soviet rules (see R-193). The Android payload is about 745 MB, of which about 290 MB could be saved. Choose between: support RA2-only, RA2 rules on YR data, or keep YR as a requirement (W2-AR Q1) | User decisions | MAJOR | USER-DECISION | `engine/data_dir.inc` kDataFiles; `docs/INSTALL.md:32-33` | L | N |
| U-2 | **App name, package and icon.** Currently "RA2 Engine", `org.ra2.engine`, and SDL's default icon. Needs a branding and trademark choice before any distribution (W2-AR-12) | User decisions | MINOR | USER-DECISION | `android/app/src/main/res/values/strings.xml:2`; `android/app/build.gradle:59` | S | N |
| U-3 | **License.** MIT, "Copyright (c) 2026 Omar El Arab", covering the engine only. Confirm the owner or entity name. THIRD_PARTY_NOTICES covers SDL 2.30.9, stb_image_write, doctest and libc++ (W2-AR-23) | User decisions | MINOR | USER-DECISION | `LICENSE:3`; `THIRD_PARTY_NOTICES.md` | S | N |
| U-4 | **Release.** Signing keystore, timing of 0.9.0 → 1.0, and the shipping channel (Play Store or side-load) (W2-AR Q5) | User decisions | – | USER-DECISION | `VERSION`; README:3626-3630 | S | N |
| U-5 | **Movies and scope.** Either accept speech-only cinematics or support user-supplied movie files, which needs the campaign Bink path from R-034. Also confirm the scope cuts: random map generator (button greyed, SK-19), Yuri faction, LAN/online, map editor | User decisions | – | USER-DECISION | `engine/skirmish_page.inc:13`; README:1566,1583,3292 | – | N |

## Wave-5 agent F status (branch `w5-android`)

Rows R-151, R-152, R-160..R-163, R-170..R-172 and R-180..R-184 carry their `w5-android` status above. Evidence: `docs/gap/wave5/06-evidence/` (emulator). Perf: ALL03 opening ~11 -> ~100 logic frames/s (desktop, headless); emulator ALL01 14.0 -> ~17 fps (SwiftShader; 85 % of the remaining frame is the host GL pipe). Save format v17.

## Proposed next fix wave (wave 5, at most 6 parallel agents)

**Precondition:** merge `w4fix-ctl` and `w4fix-seq` first. A trial `git merge-tree` showed they merge cleanly with each other.

Then re-check the items `w4fix-seq` had in scope but had not committed when this was written: R-020, R-021, R-022, R-023 and R-024. Any that did not land go to agent A.

Agents are split by code area so their diffs do not overlap:

| Agent | Area (files it owns) | Items |
|---|---|---|
| **A: Campaign scripting and mission proof** (fable RE, then opus) | `engine/map_trigger.*`, `engine/campaign.cpp`, `engine/campaign_runtime.inc`, `engine/campaign_rules.inc`, `scripts/play/m-*` | R-026 (real-order play-through scripts for the 9 PROBABLE missions plus Boot Camp), R-027, R-029, R-030, R-031, R-032, R-033, R-035, R-036, R-037, R-038, R-040, plus any seq leftovers among R-020 to R-024 |
| **B: Combat and movement core** (opus; RE for the sub-cell table) | `engine/combat.cpp`, `engine/occupancy.*`, `engine/pathfind.*`, the movement and crush parts of `engine/world.cpp`, `engine/production.cpp` | R-060 (splash on buildings and own units), R-061 (3 infantry per cell, the L item), R-062, R-063, R-064, R-065, R-066, R-050, R-057, R-068, R-191 |
| **C: Unit and building abilities, plus AI usage** (fable RE for Prism/Gap, then opus) | `engine/world_abilities.cpp`, `engine/realplay_economy.inc`, `engine/world_sw.cpp`, `engine/building_combat.cpp`, `engine/cargo.*`, `engine/world_depth.cpp` (garrison), `engine/ai_*`, `engine/ai_skirmish.inc` | R-042 (stolen tech), R-043 (Prism chain), R-044 (Gap Generator; coordinate the shroud draw with D), R-045, R-046, R-047 (sim side), R-048, R-049, R-051, R-052, R-053, R-055, R-056, R-080, R-081, R-082 |
| **D: Presentation** (opus) | `engine/pres_*.inc`, the draw paths in `engine/main.cpp`, `engine/hud_*.inc`, `engine/campaign_visuals.inc` | R-100 (bridge deck layer), R-101, R-102, R-104, R-105 (theater filter in `engine/maplist.cpp`), R-109, R-120, R-121, R-122, R-123, R-054 (jump projectile draw), R-047 (Mirage tree draw), R-039, R-041, R-142, R-144 |
| **E: Audio and shell UI** (opus; sonnet for the mechanical parts) | `engine/audio_mixer.*`, `engine/pres_audio.inc`, `engine/inf_anim.cpp` (sound tail only), `engine/ingame_pages.inc`, `engine/gf_result.inc`, `engine/campaign_scenes.inc`, `engine/shell_widgets.inc`, `engine/hotkeys.cpp`, `engine/input.cpp` | R-130, R-131, R-132, R-133, R-134, R-140, R-141, R-143, R-146, R-150, R-034 (briefing page and campaign Bink path, ready for U-5), R-106 (if U-1/U-5 allow) |
| **F: Android, performance and robustness** (opus; needs a real phone) | `android/`, `engine/app_robust.inc`, `engine/touch_*`, `engine/savegame.cpp`, `engine/game_scene_controls.inc` (save backlog), `CMakeLists.txt`, `scripts/play/probe-*` | R-170 (measure on a real arm64 phone), R-171 (profile ALL03 idle infantry), R-172, R-160, R-161, R-162, R-163, R-151, R-152, R-180, R-181, R-182, R-183, R-184 |

**Not in any agent:**
- The pure `[RE-FALLBACK]` refinements R-083, R-090, R-107, R-108, R-110, R-124, R-145, R-190, R-192, R-194, R-195 and R-196. They are cosmetic or small timing differences; batch them into a later RE-calibration wave, using the hunt → spec → fix cycle.
- U-1 to U-5, which wait on the owner. U-1 should be decided before R-193 or R-106 are touched.

## P4 additions (mission-proof agent P4, branch `p4-missions`)

Fixed on the way to the ALL09/ALL10/SOV04 proofs (details and RE citations: `docs/gap/wave4/05-mission-sequences.md`
"P4 status"):

| ID | Item | Status |
|---|---|---|
| P4-1 | Buildings with Explodes=yes never fired their DeathWeapon; DelayKill (CausesDelayKill / EligibleForDelayKill) missing -- barrel and ammo-crate chains did not happen (ALL10 villages, ALL09 base) | FIXED (`2a3d27f`, save v23) |
| P4-2 | DestroyedAll/DestroyedBuildingsAll/DestroyedUnitsAll counted Insignificant= and DontScore= objects (lamps, barrels, civilian props, spent rockets) | FIXED (`fe23895`; P3 fixed it in parallel -- the merge keeps P3's `tracked()` implementation) |
| P4-3 | A Sleep/Harmless object stayed dormant after the player (or its team) ordered it | FIXED (`81b6570`, RE-FALLBACK) |
| P4-4 | Crushable= wall overlays (sandbags, prison fences) could not be crushed by Crusher vehicles | FIXED (`629b1e3`, RE-FALLBACK, save v23) |
| P4-5 | Wall=yes warheads do not damage wall overlays (force-firing a wall does nothing); only crushing removes one | OPEN (not needed by the P4 proofs) |
| P4-6 | ALL09 not won by real orders: `m-all09-opening` proves the opening to the airport + paradrop phase (pre-P3-merge runs reached the BadGuy1 ConYard capture), then BadGuy1's Attk Response teams overrun the force; needs a finer micro script | OPEN (tactical) |

