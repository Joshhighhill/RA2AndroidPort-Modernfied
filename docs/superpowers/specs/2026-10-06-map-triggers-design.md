# P9 — Map triggers / scripting (design)

**Calibration authority:** `docs/re/map-triggers.md` (every rule below cites a
section there). Roadmap row P9; gate `--demo-triggers`.

## 1. Goal and scope

The campaign substrate: parse a map's `[Triggers] [Events] [Actions] [Tags]
[CellTags] [VariableNames]` (plus the `[TeamTypes]` index for ParamKind-1/5
team references) and run them with the original's semantics:

- the **logic loop** every frame and the **house loop** every 8th frame, in the
  original order and with the original short-circuit (RE §3.2);
- AND-of-events with the Persistent-only latch, the three persistence classes,
  tag death on a volatile fire (RE §3.3, §3.4);
- per-trigger timers (ElapsedTime / RandomDelay), ResetTimers on construction,
  Enable, Persistent re-arm and variable change (RE §3.5, §3.6);
- 50 globals / 100 locals with the variables-changed flag (RE §1.5, §3.6);
- the mission timer and its actions/event (RE §3.2, §5);
- difficulty gating (RE §3.7); house-type index space (RE §6).

Non-goals this phase (RE §7): placing campaign objects; wiring object/cell events
to world objects; running map TeamTypes through a TeamClass/script; rendering or
playing presentation actions.

## 2. Event / action coverage (ranked by the 44-map census, RE §4/§5)

**Status classes.** *Engine* = evaluated/executed entirely inside the trigger
system. *Host* = the trigger system resolves it and asks the `TriggerHost`
(a query for events, an effect for actions); the gate's host implements it on
`World`. *API* = semantics implemented and unit-tested, entered through
`raise_tag`/`raise_cell`, but not raised by the world yet (needs P10 objects).
*Recorded* = presentation-only; logged with its parameters, no simulation state.
*Unimplemented* = logged as `unimplemented` when executed / evaluates false.

Events: Engine — 8, 13, 14, 27, 28, 36, 37, 47, 51. Host — 9, 10, 11, 12, 15,
16, 17, 19, 20, 21, 22, 30, 32, 52, 57, 58, 60, 61. API — 1, 2, 4, 6, 7, 24, 25,
26, 29, 31, 33, 38–44, 48, 49, 50, 59 (object kinds: kind match + the EnteredBy
owner check + AttackedByHouse source check). Unimplemented — 3, 5, 18, 23, 34,
35, 45, 46, 53–56 (together 9 of 4,857 uses).

Actions: Engine — 0, 1, 2, 3, 6*, 12, 13, 22, 23, 24, 25, 26, 27, 28, 29, 53, 54,
56, 57, 67, 68, 74, 75 (*house flags recorded in engine state). Host — 4, 5, 7,
16, 17, 80, 107, 125. Recorded — 10, 11, 19, 20, 21, 41, 46, 47, 48, 55, 99, 100,
103, 104, 112, 113, 114, 115, 116, 131. Unimplemented — everything else
(counts tabled in the README section).

## 3. Module layout (firewall)

- **NEW** `engine/map_trigger.hpp/.cpp` — pure C++17 (no SDL, no `World`):
  `parse_map_triggers(map Ini, rules [Countries], rules [BuildingTypes])` →
  `MapTriggerData`; `MapTriggerSystem` (runtime); `TriggerHost` (abstract
  queries + effects). Compiled into `ra2assets_tests` and `ra2engine` (via
  `cmake/ra2_sources.cmake`).
- **NEW** `tests/test_map_trigger.cpp` — synthetic-INI unit tests with a fake
  host, plus one install-gated parse of the real `sov07tmd.map`.
- **main.cpp** — additive only: a `WorldTriggerHost` adapter, `simulate_triggers`,
  `run_demo_triggers`, one `--demo-triggers` dispatch line.
- **Untouched:** `World`, `MapFile`, `load_assets`, every existing gate path.
  The trigger system exists only inside `run_demo_triggers`; no other mode
  constructs one, so all 19 existing byte-identity gates are unaffected by
  construction.

## 4. Data model

`MapTriggerData`: `house_types` (rules countries then map countries appended,
RE §6), `building_types` (rules `[BuildingTypes]` then map additions),
`triggers` (array order = first reference, RE §1.8; each with `house_type`,
`linked`, `enabled`, `difficulty[3]`, `events` **in memory order (reversed)**,
`actions` in file order), `tags` (creation order: TeamType `Tag=` → action
ParamKind-3 refs → `[Tags]`), `team_ids` (`[TeamTypes]` index order), locals
(name + initial value, 100 slots), `cell_tags` (decoded `x=v%1000, y=v/1000`).
Unknown IDs resolve to −1 (never a crash).

## 5. Runtime (`MapTriggerSystem`)

`start(host)` — scenario start (RE §3.2): globals cleared, locals initialised;
for each TagType in array order with flags & 4 / 0x10 / 8, `GetInstance` and
append to the map / logic / house(first trigger's house, if it exists) list.
`GetInstance` constructs the tag's TriggerClasses (chain reversed), each
running ResetTimers (RandomDelay draws in this order).

`tick(host)` — one logic frame: the logic loop (RE §3.2 steps 1–6; crate and
ambient flags are never set by this engine), then clear the variables-changed
flag and stop an expired mission timer, then if `frame % 8 == 0` each existing
house's list from last to first with kind 8, then delete pending tags, then
`++frame`.

`raise(tag, kind, ctx)` / `RegisterEvent` / `HasOccurred` / `FireActions` /
`Execute` as RE §3.3–§5. RNG: a seeded xorshift32 drawn inclusive `[0, v]`
(RE §7 item 1). Every fired trigger and every executed action appends a log
line `f=<frame> <trigger id> "<name>" a<kind> ...`; host-level actions call
`host.apply(effect)`; `state_hash()` folds frame, variables, mission timer,
every trigger instance (enabled/destroyed/bits/timer), outcome and the log.

## 6. The gate — `--demo-triggers` (20th byte-identity gate)

Default map (`sov07tmd.map`, the gates' existing map — no MapSelection change),
re-read as INI for the trigger sections. Houses: the map `[Houses]` list order →
engine house ids (house 0 = `Player House`); a Soviet base for house 0 so the
player owns objects. Difficulty Normal. Host: buildings/units counted from
`World` by owner; building-type index → roster id via `building_types`;
techno counts by id (buildings by roster id, units by `Unit::image` via the
loaded rosters); reveal via `World::reveal_source`/`reveal_all_house` with
rules `RevealTriggerRadius`; Reinforcement/CreateTeam place TaskForce members at
the team waypoint when the type is a loaded roster unit (else recorded as
skipped); CreateBuilding places a roster building at the waypoint. Run
`kTriggerFrames` frames rendered then headless from a fresh world; byte-compare
the trigger log + `trigger_hash = fold(world_hash(world), system.state_hash())`;
print the log, the per-status counts, and the hash. Exit 0 iff identical and the
witness shows ≥1 logic-loop fire, ≥1 variable-driven fire and ≥1 house-loop
fire.

## 7. Testing

Scoped TDD per task (`ra2assets_tests -tc="map_trigger*"`); the only gate run is
`--demo-triggers` (pin its hash). Full suite + all gates are the controller's
final verification.
