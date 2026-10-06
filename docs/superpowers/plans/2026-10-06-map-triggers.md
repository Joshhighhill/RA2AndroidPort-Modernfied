# P9 Map triggers — implementation plan

Spec: `docs/superpowers/specs/2026-10-06-map-triggers-design.md`.
RE: `docs/re/map-triggers.md`. Branch `p9-triggers` (worktree
`D:\ra2-wt\p9-triggers`, build dir `build/`).

**Test discipline (user rule):** build, then run ONLY
`build/ra2assets_tests.exe -tc="map_trigger*"` per task; the only gate run is
`--demo-triggers` (Task 4) to pin its hash. No full suite, no other gates.

## Task 1 — parser (`engine/map_trigger.{hpp,cpp}`, `tests/test_map_trigger.cpp`)

1. Tests first (synthetic INI): `[Triggers]` field rules (house lookup in the
   appended house-type space, `<none>`, Disabled, missing field ⇒ disabled,
   difficulty, linked trigger forward reference creates the type early);
   `[Events]` count + `Param==2` extra token + **reversed** storage;
   `[Actions]` 8-token groups, every ParamKind, `≤2-char` team index vs ID,
   waypoint letters (`A`, `Z`, `AA`, `BA`, non-alpha ⇒ −1), ParamKind 5/9/11
   last-token rules; `[Tags]`; TagType creation order (TeamType `Tag=` → action
   ParamKind 3 → `[Tags]`); `[VariableNames]` (100 cap, `name,value`);
   `[CellTags]` decode; unknown IDs ⇒ −1.
2. Implement `parse_map_triggers`, `waypoint_letters`, attach-flag tables
   (`event_flags`, `action_flags`, `tag_flags`).
3. Install-gated test: real `sov07tmd.map` parses to 147 triggers / matching
   counts, `_Lose` event 11 value 15 resolves to house type `Player`.
4. Wire into CMake (tests line + `cmake/ra2_sources.cmake`). Commit.

## Task 2 — runtime core (engine semantics)

1. Tests with a fake `TriggerHost`: ElapsedTime fires at exactly `Value·15`;
   RandomDelay range; ElapsedScenarioTime; AND of two events; volatile fires
   once and kills its sibling chain trigger; persistent re-arms its timer;
   LocalSet change sets the flag → a 36-only trigger fires the same frame only
   for later tags / next frame for earlier; variable change resets referencing
   timers; Enable resets timers and honours difficulty; Disable; DestroyTrigger;
   ForceTrigger (requires enabled); mission timer Set/Start/Stop/Extend/Shorten
   + event 14 single-frame; house loop every 8 frames, reversed order, kind 8
   AnyEvent; Win/Lose player-relative outcome; disabled trigger never fires.
2. Implement `MapTriggerSystem::{start,tick,raise_tag,raise_cell}`,
   RegisterEvent/HasOccurred/FireActions/Execute (engine classes), log,
   `state_hash`. Commit.

## Task 3 — host-backed events/actions

1. Tests: BuildingExists / DoesNotExist via host counts; DestroyedAll /
   UnitsAll / BuildingsAll with the "absent house ⇒ true" rule; null trigger
   house skips the house switch; TechTypeExists/DoesntExist (unknown type ⇒
   false); credits; power; object-kind events through `raise_tag` (kind match,
   EnteredBy owner, AttackedByHouse source); host effects emitted for
   4/5/7/16/17/80/107/125 with resolved team id / waypoint / house; recorded
   presentation actions; unimplemented actions logged.
2. Implement. Commit.

## Task 4 — `--demo-triggers` gate (main.cpp, additive)

1. `WorldTriggerHost` over `World` (+ roster tables), `simulate_triggers`,
   `run_demo_triggers` (rendered + headless self-compare, no pinned literal in
   code), dispatch line. Witness assertions per spec §6.
2. Build; run `ra2engine --demo-triggers` (PowerShell); pin the hash in the
   README. Commit.

## Task 5 — finalize

README section (status tables with counts, gate row, deviations), ledger entry
in `D:\ra2-android\.superpowers\sdd\progress.md`. Commit.
