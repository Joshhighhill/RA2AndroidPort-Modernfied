# AI-2 Offensive-Teams Phase Design — The Enemy Forms Squads and Attacks

**Date:** 2026-07-12
**Status:** Approved (brainstorm sections 1–4 approved individually)
**Depends on:** AI-1 base planning (complete; the enemy Soviet house builds
out a base from a lone ConYard, `--demo-ai-base` gate
ai_base_hash=cd1f7c6a29933f0a) plus the Economy and Unit Production phases
AI-1 already depends on (`--demo-econ` econ_hash=7fed96cb98b68017,
`--demo-prod` prod_hash=f0f4e1207ecd713c).

## Goal

Non-player houses get an offensive-team brain. This is sub-phase **AI-2** of
the three-sub-phase AI effort: AI-1 built the base-planning brain, AI-2 adds
offensive teams via the recovered TaskForce/TeamType/ScriptType trio from
aimd.ini, and AI-3 will add the AITriggerTypes weighted trigger engine. Today
the enemy builds a base and banks credits but never fields an army. AI-2
gives every AI house a team brain that drives the EXISTING machinery: it
parses the real aimd.ini, selects autocreate TeamTypes, recruits idle combat
units and produces the shortfall through the existing Infantry/Vehicle
production queues, gathers the squad near the base, and marches it at the
player under a ScriptType interpreter. One spec→plan→execute cycle, ~8–9
tasks, the same spine as AI-1.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Data model | **Full recovered TaskForce/ScriptType/TeamType structs** parsed from the real aimd.ini (index sections + per-entity numbered entries), layouts confirmed/replaced by the Task 1 hunt |
| Interpreter breadth | **Full opcode table parsed and RE-documented; every opcode expressible with existing machinery implemented functionally; the rest interpreted-to-inert** (logged skip, advance PC) |
| Team creation | **`autocreate` TeamTypes ONLY** — AITriggerTypes-driven creation is AI-3, out of scope here |
| Recruitment | **Two-source fill**: recruit-idle first, then produce the shortfall via the EXISTING ProductionQueues — no new production code |
| Generality | **House-generalized** — any AI house runs teams, extending AI-1's per-house registry |
| Verification | **New deterministic gate** `--demo-ai-team` (the 7th permanent byte-identity gate) |
| Ini substrate | **No Ini-layer changes** — `ra2assets::Ini::section()` already returns the ordered key/value list preserving file order, exactly what numbered `N=...` entries and index sections need; `has`/`get`/`get_int`/`section_names` cover the rest |

## §1 What the Ghidra hunt must recover (→ `docs/re/ai-team.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001
Ghidra project at D:\ra2-re, headless analyzeHeadless.bat,
JAVA_HOME="C:\Program Files\Java\jdk-21") plus the real aimd.ini, producing
`docs/re/ai-team.md` — the calibration authority BEFORE any code, with a
located-functions table, a "deviations we take" section (mirroring
`ai-base.md`/`production.md`), and each item landing CONFIRMED / PROBABLE /
[RE-FALLBACK] with function address + decompile reasoning. Anchor on the
literal aimd.ini key/section strings (`"TeamTypes"`, `"TaskForces"`,
`"ScriptTypes"`, `"Group"`, `"Priority"`, `"Max"`, `"Full"`, `"House"`,
`"Autocreate"`, `"Script"`, `"TaskForce"`, and the ScriptType action names).
YRpp/Ares addresses are UNTRUSTED — prose hints only. Six items:

1. **Struct layouts + file format**: the TeamType/TaskForce/ScriptType
   struct layouts and the exact aimd.ini section/key format — the
   `[TaskForces]`/`[ScriptTypes]`/`[TeamTypes]` index sections (index=id)
   and the per-entity numbered entries (`N=count,UNITID` for TaskForces,
   `N=action,argument` for ScriptTypes, the named keys for TeamTypes).
2. **Team lifecycle state machine**: create conditions (the autocreate
   path), the recruit-vs-produce decision, the gather behavior at
   AISafeDistance, the full/timeout rule (when does a below-strength team
   launch anyway), and disband semantics (script end, team wiped, survivor
   release).
3. **The FULL ScriptType opcode table**: action number → semantics for
   every opcode, each marked **implementable** (expressible with existing
   engine machinery) or **inert-required-subsystem-absent** (transport,
   aircraft, superweapons, chronosphere).
4. **TeamType selection/weighting**: the autocreate path's selection —
   house match, `Autocreate=`, `Priority=` ordering, `Max=` live-instance
   cap — explicitly noting that AITriggerTypes-driven creation is AI-3 and
   out of scope here, so the recovered doc marks the boundary the phases
   compose across.
5. **Quarry/target selection**: the quarry-kind → target-class mapping the
   attack opcodes use (what "attack quarry type N" resolves to and how a
   concrete target is chosen).
6. **Production/reserve interplay**: how teams consume production (factory
   reservation, one-at-a-time gating) and how InfantryReserve/CreditReserve
   gate team creation — confirming AI-2 reuses the existing player
   production path (can_start_unit/start_unit/launch_unit) rather than a
   parallel one.

Calibration discipline (unchanged): every engine constant cites this doc.
**All numeric values in this spec are HUNT OUTPUTS, not guesses.** A
controller **RE-GATE** amendment step precedes implementation — the hunt's
findings amend the plan and are committed BEFORE dependent dispatch, and
`[RE-GATE APPLIED]` markers are threaded through every dependent task (the
ai_base.hpp precedent). **[RE-FALLBACK]**: anything the hunt cannot pin
becomes a plan-amendment gate (present options to the human) — never an
uncalibrated guess.

## §2 Architecture & recovered data model

Consistent with every prior phase: a pure SDL-free engine module + thin
World integration + a deterministic demo in main.cpp. One spec→plan→execute
cycle, ~8–9 tasks, the same spine as AI-1:

- **Task 1**: Ghidra hunt → `docs/re/ai-team.md` (RE-GATE before
  implementation; §1 above).
- **Task 2**: aimd.ini parser producing TaskForce/ScriptType/TeamType
  structs (pure, testable).
- **Task 3**: pure team engine `engine/ai_team.{hpp,cpp}`: 4-state
  lifecycle + ScriptType interpreter.
- **Task 4**: recruitment + production intent (produce-to-fill +
  recruit-idle).
- **Task 5**: World integration: per-house team registry +
  `step_ai_teams()`.
- **Task 6**: main.cpp wiring: load+parse the real aimd.ini, resolve ids,
  Soviet combat roster art.
- **Task 7**: `--demo-ai-team` gate + `ai_team_hash` + witness lines.
- **Task 8**: finalize: suites both modes, 6 existing gates byte-identical,
  arm64, README.

**Decomposition tripwire**: if the plan exceeds ~10 tasks, the aimd.ini
parser (Task 2) splits into its own spec→plan→execute cycle and AI-2 proper
consumes its output.

### Recovered data model

Layouts below are the approved shape; the Task 1 hunt CONFIRMS or REPLACES
them (RE-GATE), and the parser/tests are written against the amended form.

- **TaskForce** — `name`, `group`, `vector<{count, unit_roster_idx}>`.
  Parsed from `[<tf-id>]`: `Name=`, `Group=`, numbered `N=count,UNITID`.
- **ScriptAction** `{int action; int argument;}`; **ScriptType** — `name`,
  `vector<ScriptAction>`. Parsed from `[<script-id>]`: `Name=`, numbered
  `N=action,argument`.
- **TeamType** — `name`, `taskforce_idx`, `script_idx`, `house`,
  `priority`, `max`, `full`, `autocreate`, plus whatever flags the hunt
  recovers as load-bearing. Parsed from `[<team-id>]`.
- **Index sections** `[TaskForces]`/`[ScriptTypes]`/`[TeamTypes]`
  (index=id) resolve id→struct; the parser walks the index, then each
  entity section, via the existing `Ini::section()` ordered key/value list.
  **No Ini-layer changes are needed** — order-preserving numbered keys are
  already what `section()` returns.
- **Art guard**: UNITID values missing from this install's art catalog are
  art-guard-dropped — the whole team is skipped, never a crash — exactly
  like AI-1's catalog gaps (`AiCatalog` -1 entries).

### Scope bound on the "broad interpreter"

Parse and RE-document the FULL opcode table. Functionally implement every
opcode expressible with existing machinery: move-to-cell/waypoint,
attack-quarry by type, guard/guard-area, timed wait, scatter, regroup,
jump/change-script — plus ONE cheap new primitive, a waypoint→cell table in
main.cpp. Opcodes requiring absent subsystems (transport load/unload,
aircraft, superweapons, chronosphere) are parsed, RE-documented, and
**interpreted-to-inert**: a logged skip that advances the program counter —
faithful to how AI-1 recorded PowerEmergency/InfantryReserve without
consuming them.

## §3 Team lifecycle & ScriptType interpreter (pure engine)

**New pure module `engine/ai_team.{hpp,cpp}`** (no SDL, no World types;
doctest-testable): consumes a read-only `TeamView` filled by the caller and
emits `TeamCommand`s the caller executes — exactly as `ai_base` consumes
`BaseView` and emits `BuildDecision`. Same conventions as ai_base.hpp:
every numeric constant is a hunt output cited to `docs/re/ai-team.md`; the
engine-local seeded xorshift32 `ai_rand` adjudicates any tie-break (NEVER
wall-clock); every piece is doctest-testable on hand-built views.

**Lifecycle: Recruiting → Gathering → Executing → Disbanding.**

- **Recruiting**: a TeamType has been selected; the team claims matching
  idle standing units and records the shortfall per TaskForce member. It
  stays here until member counts are met — or, when `full=false`, until the
  timeout rule the hunt pins fires and the team launches below strength.
  Newly produced units join the team here.
- **Gathering**: all members recruited → move members to a gather cell near
  the ConYard/base within AISafeDistance (`kAiSafeDistance` = 20, recovered
  in AI-1, [RE: ai-base.md §2]). Stays until members are within the gather
  radius; a member death drops the team back to Recruiting if it falls
  below full.
- **Executing**: run the ScriptType via a program counter `script_pc`
  walking the action list. Each action drives the team until its completion
  predicate fires, then `pc++`. Timed actions use a dt-accumulated
  deterministic `script_timer`.
- **Disbanding**: the script ran off the end, or the team was wiped →
  release survivors to idle (re-recruitable), destroy the team instance.

**Interpreter**: one `TeamCommand` per member per tick. Opcode classes →
command → what the World executes:

| Opcode class | TeamCommand | World execution |
|---|---|---|
| Move-to-waypoint/cell | `MoveTeam{cell}` | `order_unit_move` (formation via `group_move`) |
| Attack-quarry(type N) | `AttackQuarry{quarry_kind}` | `order_attack` / `order_attack_building` |
| Guard / guard-area | `GuardArea{cell,radius}` | guard anchors |
| Timed wait | `Wait{frames}` | interpreter timer |
| Scatter / regroup | (move commands) | `order_unit_move` |
| Jump / change-script | (none) | interpreter sets `pc` |
| Transport / air / SW | `Inert{opcode}` | logged skip, `pc++` |

**Determinism**: only the seeded `ai_rand` for tie-breaks. The quarry-kind →
target mapping is a Task-1 recovery item (§1 item 5), not a guess.
**Testability**: each opcode gets a doctest on a hand-built `TeamView`, like
`test_ai_base`.

## §4 TeamType selection, recruitment & production

Runs per tick per AI house, on a team clock (the hunt pins the cadence;
AI-1's `build_gate` is the model).

**Selection**: consider TeamTypes whose `house` matches and whose
`autocreate` is set. **AITriggerTypes-driven creation is AI-3 — AI-2 uses
autocreate ONLY**; this boundary is stated so the phases compose. Gate by
the recovered rules: never exceed `Max` live instances of a TeamType;
respect `Priority` ordering; pass a CreditReserve (`kAiCreditReserve` = 100)
/ InfantryReserve (`kAiInfantryReserve` — the AI-1 carry-forward AI-2 now
consumes) affordability check. Create at most ONE team per tick, highest
priority first, deterministically.

**Recruitment (two-source fill):**

1. **Recruit-idle first**: scan the house's own alive idle combat units
   (not in a team, not harvesting, not on a build order) matching an
   unfilled TaskForce member type; claim them via a deterministic ascending
   unit-index walk (mirrors AI-1's standing-building→node sync).
2. **Produce the shortfall**: drive the EXISTING ProductionQueues
   Infantry/Vehicle queues — `can_start_unit` → `start_unit` → on ready,
   `launch_unit` spawns at the factory exit → the unit is claimed into the
   recruiting team. **NO new production code** (the same discipline as AI-1
   reusing the Structure/Defense queues).

**Routing**: infantry members → Infantry queue (tab2), vehicle members →
Vehicle queue (tab3); one unit at a time per queue (the single-slot queue
maps the recovered one-at-a-time gate). The `build_slowdown` difficulty
handicap (AI-1 carry-forward, recorded-unused there) is consumed here as a
unit-production penalty.

**Member death / disband**: death is detected like AI-1's node death
(`!alive || Dead`); the slot reopens (team drops back to Recruiting if
below full); on disband, survivors return to idle and are re-recruitable.

## §5 World integration, the `--demo-ai-team` gate, testing & byte-identity

**World integration (`engine/world.cpp` + `world.hpp`)**, same firewall as
AI-1: extend `HouseAiRec` with an `AiTeamState` (teams + team clock +
production intent); the new fields are structurally excluded from
`world_hash` (main.cpp's whitelist covers only Unit/Building state).
`step_ai_teams(dt)` runs inside `if (ai_any_)` in `step()`, AFTER
`step_ai_houses`, in a fixed deterministic order (base tick then team tick);
it early-returns on a house with no eligible teams, so the six existing
gates stay bit-identical (the existing demos flag no AI team; only
`--demo-ai-team` and the interactive path do). `step_ai_teams` is the ONLY
impure glue: it fills `TeamView`, calls the pure engine, and translates
`TeamCommand`s into the existing `order_unit_move` / `order_attack` /
`order_attack_building` / `launch_unit` / `start_unit`. The recruit-idle
scan and death-detection live here.

**7th gate `--demo-ai-team` / `ai_team_hash`**: seed a Soviet AI house with
a base, run a fixed frame count, hash deterministically — FNV over the
sorted team state `{teamtype, state, pc, sorted member cells/hp}` + the
produced-unit list + credits — two-run byte-compare, **NO hardcoded
literal** (compute-and-self-compare, like `--demo-ai-base`). Witness lines:
team created → recruited N → gathered → executing script → first attack
order at the player.

**Six existing gates re-verified byte-identical in finalize:**
`--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a.

**Testing**: pure `test_ai_team.cpp` — the parser round-trips inline
real-format aimd fixtures; each opcode's emitted command; the full lifecycle
Recruiting→Disbanding on a hand-built `TeamView`; reserve gating;
recruit-idle claim order; death/disband. Integration is exercised via the
World loop in the gate demo. Tests skip cleanly without `RA2_INSTALL_DIR`
(the parser tests use inline fixtures, host-only).

**Interactive payoff**: main.cpp's existing enemy Soviet house gets
autocreate teams from the real aimd.ini — the enemy builds its base (AI-1)
AND forms squads and marches them at the player (AI-2).

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original RA2 behavior (Ghidra / real
  aimd.ini / prose-only GPL refs) BEFORE implementing; empirical guessing
  caused real bugs in prior phases. Every numeric AI constant is a hunt
  output, not a guess; tests assert against named constants.
- Six byte-identity gates must stay green (values above).
  `step_ai_teams()` early-returns when no AI house/team is flagged; new
  per-house AI fields are excluded from `world_hash`.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without
  the install.
- No game assets / GPL source / Ghidra project committed; recovered numeric
  constants ARE committable.
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode to design around: implementers stalling on
  backgrounded test/demo runs — the plan's briefs must order FOREGROUND
  synchronous runs with generous timeouts.

## Testing summary

| Layer | Tests |
|---|---|
| aimd parser (pure) | round-trips inline real-format aimd fixtures (index sections, numbered TaskForce/ScriptType entries, TeamType keys); art-guard-drop of unknown UNITIDs; host-only, no install needed |
| engine/ai_team (pure) | each opcode's emitted TeamCommand on a hand-built TeamView; full lifecycle Recruiting→Gathering→Executing→Disbanding; full=false timeout; reserve gating (CreditReserve/InfantryReserve); recruit-idle deterministic claim order; death/disband slot-reopen and survivor release |
| integration | `--demo-ai-team` witnesses (created → recruited → gathered → executing → first attack order) + `ai_team_hash` two-run byte-compare (compute-and-self-compare, no pinned literal) |
| regression | six existing demo gates byte-identical (values in §5); full suite both modes; arm64 NDK build with new symbols nm-verified |
