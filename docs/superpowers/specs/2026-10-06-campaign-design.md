# P10 Campaign missions — design

RE: `docs/re/campaign.md` (progression, object formats, win/lose flow, index
spaces, script anchors) and `docs/re/map-triggers.md` (the P9 substrate).

## Goal

Play the RA2 **Allied** and **Soviet** campaigns: main menu → side select →
loading screen (mission art + briefing) → the mission (map objects, map
triggers, map teams, objectives) → win/lose → result screen → next mission (or
retry / menu). First target: Allied 1 (`all01t`) and Soviet 1 (`sov01t`)
playable start → win; then the remaining 22 missions load and run through the
same path.

## Components

1. **`engine/campaign.{hpp,cpp}` (pure, World-free, unit-tested)**
   - `CampaignTables`: `battle.ini` (`[Battles]`), `mission.ini` (per-map
     briefing/LS keys), `mapsel.ini` (side stage lists + stage `Scenario=` /
     next-stage keys). `first_scenario(battle)`, `mission(map)`,
     `next_scenario(side, current)` = the RE §3.5 stage-chain rule.
   - `ScenarioBasic parse_scenario_basic(map)` — `[Basic]` fields (`Player`,
     movies, `EndOfGame`, `SkipScore`, `OneTimeOnly`, `Theme`, `Briefing`).
   - Map object records (`[Structures]/[Units]/[Infantry]/[Aircraft]`) with the
     RE §4 field orders; `campaign_strength(type_strength, health256)` (§4 snap
     rule).
   - `MissionEnd` — the BorrowedTime state machine: first Win/Lose flags the
     outcome at frame F, the mission **ends** at `F + round(SavourDelay·900)`.
   - **Map-team interpreter** `step_map_team` over the map's own
     `[ScriptTypes]` with the RE §7 action numbering (move to waypoint/cell,
     attack quarry, attack waypoint, patrol, guard-timer, jump (1-based), win,
     lose, set/clear global/local, change house, scatter, delete members, do
     mission (hunt/guard/sleep), attack/move enemy building, gather) → one
     command per tick; everything else logged and skipped.
2. **`MapTriggerSystem` campaign mode** (`set_campaign_mode(true)`, default
   off → every P9 behaviour/log line unchanged): object actions `14`
   ChangeHouse, `32` DestroyAttachedObject, `36` AllChangeHouse, `37/38`
   MakeAlly/MakeEnemy, `63` Apply100Damage routed to the host with the firing
   trigger; tag attach/detach (`InstanceCount`) and a non-creating
   `raise_object` (SemiPersistant detach rule, RE map-triggers §3.4).
3. **`CampaignRuntime` (`engine/campaign_runtime.inc`, included by `main.cpp`)**
   — built on the real `World`:
   houses from `[Houses]` (player first), RA2 `rules.ini` index spaces, map
   objects placed with tags, extra building/unit types resolved generically
   from rules/art, the trigger host (counters incl. destroyed/last-built),
   per-frame event polling (cell entry 1/59, destroyed 48/7, attacked 6,
   discovered 4, selected 33), teams (`CreateTeam`, reinforcements at the team
   waypoint, scripts), hunt mission, mission end. Ticks at 15 Hz from the
   interactive variable dt (accumulator) and per frame in the gate.
4. **Shell** (`main.cpp`, interactive only): main-menu Campaign entry →
   `CampaignSelectScene` (Allied / Soviet / difficulty) →
   `CampaignLoadScene` (LS800 SHP + mission name + `LSLoadBriefing` text from
   CSF; click/key to start) → `GameScene` in campaign mode (objective text
   overlay = the last `TextTrigger` lines; briefing on F1/Tab) →
   `CampaignResultScene` (won/lost; Continue → next mission's load scene,
   Retry, Menu). Movies: none on this install (RE §8) — skipped.
5. **Gate `--demo-campaign`** — Allied 1 and Soviet 1, each run for a fixed
   frame budget headless + rendered, with triggers, teams and polling live;
   witness = trigger log + team/runtime lines; `campaign_hash`.

## Firewall

Nothing constructs `CampaignRuntime` except campaign `GameScene` and
`--demo-campaign`. `MapTriggerSystem` campaign mode is off by default (the
`--demo-triggers` gate never sets it). New `World` API is two additive
per-unit order wrappers. `load_assets`, roster loaders, `launch_skirmish`,
every existing gate path: untouched.

## Cuts (justified)

- Campaign movies: absent from this install (RE §8).
- Score-screen layout, map-select globe: not recovered / forced choice.
- Enemy base AI (ProductionBegins / AI triggers for map houses): the map
  houses are scripted; base build-out needs IQ/base-node data the AI-1 planner
  doesn't consume. Logged.
- Transport delivery / paradrop / unload / deploy script actions, bridges
  (`AllBridgesDestroyed`, Apply100Damage on bridges), crates from triggers,
  ambient sounds/speech/movies (presentation actions are logged).
- Save/load of a mission in progress (P7 is concurrent; not depended on).
