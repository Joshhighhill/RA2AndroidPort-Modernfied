# AI-3 AITriggerTypes Weighted-Trigger Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Non-player houses get the AITriggerTypes weighted trigger engine (sub-phase AI-3, the last of the three-sub-phase AI effort): the AI parses the real `[AITriggerTypes]` (+ `[AITriggerTypesEnable]`) sections of aimd.ini, evaluates each trigger's recovered condition against the live battlefield, selects ONE eligible trigger by a cumulative-sum weighted-random draw over `round(CURRENT weight)` with the 5000 mandatory tier [RE-GATE APPLIED: ai-trigger.md §3], creates the trigger's Team1 AND Team2 through AI-2's EXACT team-creation path (the trigger pool IS the autocreate pool — all 163 stock TeamTypes are `Autocreate=yes` [RE-GATE APPLIED: ai-trigger.md §6]; AI-3's value is condition-driven, Team1+Team2, adaptive-weight creation beside autocreate's timer-driven draw), and feeds each created team's disband outcome back into every trigger sharing its Team1 (additive +20 success / −50 failure with the track-record term, clamped [RE-GATE APPLIED: ai-trigger.md §4]) — verified by a new 8th permanent byte-identity gate `--demo-ai-trigger`.

**Architecture:** Task 1 recovers ground truth from gamemd.exe + the real aimd.ini into `docs/re/ai-trigger.md` (RE-GATE before any code); then the change lands additively, protecting all seven byte-identity gates. `engine/aimd_parse.{hpp,cpp}` is EXTENDED (not a new module) with an `AITriggerType` table on `AimdData` and a `parse_ai_triggers` flat-line parser over the UNCHANGED `ra2assets::Ini::section()` list, resolving `condition_object` through caller resolvers to a `{kind, roster index}` pair with art-guard-drop; a new pure module `engine/ai_trigger.{hpp,cpp}` holds the trigger brain (condition dispatch over the FULL recovered ConditionType enum with inert→FALSE, the enable-filter matrix, weighted-random selection from the seeded `ai_rand`, and clamped adaptive weight dynamics) — read-only `TriggerView` in, `TriggerDecision` out, exactly the ai_base/ai_team pattern; `engine/world.{hpp,cpp}` adds a trigger-eval step INSIDE AI-2's existing TeamDelays check pass in `step_ai_teams`, beside `select_autocreate_team` — a SECOND creation path feeding the same `AiTeamState.teams` (creating BOTH Team1 and Team2 [RE-GATE APPLIED: ai-trigger.md §3]), with disband outcomes attributed back by Team1-identity scan (NO team→trigger back-pointer [RE-GATE APPLIED: ai-trigger.md §6]), firewalled behind a `triggers_enabled` bool defaulting false; `main.cpp` parses the real `[AITriggerTypes]` house-scoped inside `make_ai_team_config`, enables triggers on the interactive enemy Soviet house, and wires the `--demo-ai-trigger` gate. **ONE deliberate ADDITIVE AI-2 touch [RE-GATE APPLIED: ai-trigger.md §4]: `TeamInstance` gains a `completed_success` flag set where `step_team` executes `kOpDisband` (success = script completion, NOT survivors); every lifecycle transition is UNCHANGED.** Spec: `docs/superpowers/specs/2026-07-13-ai-trigger-design.md`.

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle, NDK `D:\android-ndk-r27c`), Ghidra headless (`analyzeHeadless.bat`, JAVA_HOME=`C:\Program Files\Java\jdk-21`, project at `D:\ra2-re`), SDL2 (wiring layer only), `ra2assets::Ini` (unchanged) as the aimd substrate.

## Global Constraints

(Copied verbatim from spec §Global constraints (project-wide) — every task's requirements implicitly include this section.)

- Ghidra-ground-truth-first: recover original RA2 behavior (Ghidra / real aimd.ini/rulesmd / prose-only GPL refs) BEFORE implementing; empirical guessing caused real bugs in prior phases. Every numeric AI constant is a hunt output; tests assert against named constants.
- SEVEN byte-identity gates must stay green (values above). The trigger step early-returns when no AI house has triggers enabled; new per-house AI-trigger fields excluded from `world_hash`.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127). `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the install.
- No game assets / GPL source / Ghidra project committed; recovered numeric constants ARE committable.
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs — the plan's briefs must order FOREGROUND synchronous runs with generous timeouts.

**Plan-operational addenda (AI-1/AI-2 precedent, unchanged):**

- **The seven byte-identity hard gates (permanent, all must stay green and pinned):** `--demo-combat` ends `deterministic=yes logs=78 lines`; `--demo-tank` ends `deterministic=yes logs=144 lines`; `--demo-base` prints `hash=5273b5e71ca009e1`; `--demo-econ` prints `econ_hash=7fed96cb98b68017`; `--demo-prod` prints `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` prints `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` prints `ai_team_hash=312e92ed862a5e74`. Any change to their output is a defect in YOUR change.
- **[RE-GATE] protocol:** Tasks 2–6 are written with best-known pre-hunt values, each consuming point marked `[RE-GATE APPLIED: ai-trigger.md §N]` with its fallback inline. After Task 1, the controller diffs `docs/re/ai-trigger.md` against every `[RE-GATE APPLIED]` block in Tasks 2–6, amends the plan, and commits the amendment before dispatching the task that uses it. Values the hunt cannot pin are **[RE-FALLBACK]** plan-amendment gates — STOP, present options to the human, amend the plan, commit the amendment BEFORE any dependent dispatch. Never guess silently. **STATUS: APPLIED 2026-07-18 — the Task 1 hunt is complete and Tasks 2–6 below carry the RECONCILED recovered values (value-first comparator blob, ConditionType −1..7 with ct=4 HouseCredits, round(weight) cumulative draw + 5000 tier, BOTH Team1+Team2 created, additive +20/−50 track-record weight rule, success = script-completion flag, inverted per-difficulty enable, Team1-identity attribution); each `[RE-GATE APPLIED]` tag now marks a CONFIRMED value unless it says [RE-FALLBACK].**
- Run every build/test/demo **FOREGROUND, synchronously, blocking** (generous timeout, e.g. 600000 ms) — do NOT background test runs and wait; a prior phase lost subagents to that (one even hung waiting on a monitor).
- Desktop build: `cmake --build D:\ra2-android\build --target ra2assets_tests` then run `D:\ra2-android\build\ra2assets_tests.exe`. Engine: target `ra2engine`, exe `D:\ra2-android\build\ra2engine.exe`. doctest filters are name-based `--test-case="prefix*"`.
- Tests skip cleanly without `RA2_INSTALL_DIR` — doctest `MESSAGE` + `return`. Parser/engine tests use INLINE fixtures and run host-only (no install needed).
- Scratch files (hunt scripts, extracted aimd.ini, screenshots) to the session scratchpad, never the repo.
- Model assignments: implementation → opus; specs/plans/review → fable; easy/mechanical tasks → sonnet.

### Approved scope decisions (spec §Approved scope decisions)

- **Full recovered AITriggerType struct** parsed from the real `[AITriggerTypes]` section — a FLAT list where each `<id>=Name,field,field,...` line is one complete trigger (NOT a per-id entity section), layouts confirmed/replaced by the Task 1 hunt.
- **FULL recovered ConditionType enum parsed and RE-documented**; every type testable against the TriggerView implemented; any type needing an absent subsystem is RE-documented and **evaluates FALSE** (inert, like AI-2's inert opcodes).
- **Weighted-random over CURRENT weight** among eligible triggers, drawn from the seeded engine `ai_rand` — deterministic, NEVER wall-clock. Recovered draw shape: cumulative-sum walk over `round(Weight_Current)` via `rand(1,Σw)`, with the 5000 mandatory tier [RE-GATE APPLIED: ai-trigger.md §3].
- **Adaptive weight dynamics**: created-team outcome adjusts the trigger weights, clamped [min_weight, max_weight]; the EXACT adjustment rule and success definition are **Task 1 hunt recoveries, not guesses** — recovered: additive `+20` success / `−50` failure + track-record ratio term, at disband only; success = the team ran its script to the success/disband action [RE-GATE APPLIED: ai-trigger.md §4].
- **A SECOND creation path BESIDE AI-2's `select_autocreate_team`** — both feed the same `AiTeamState.teams`; the spec's "zero AI-2 changes" is deliberately relaxed by the RE-GATE to **ONE additive success flag on `TeamInstance` (`completed_success`, set at `kOpDisband`) [RE-GATE APPLIED: ai-trigger.md §4]** — lifecycle/interpreter transitions unchanged; autocreate keeps running.
- **House-scoped** — any AI house can enable triggers, extending the per-house registry; wired on for the enemy Soviet house.
- **New deterministic gate `--demo-ai-trigger`** (the 8th permanent byte-identity gate).
- **No Ini-layer changes** — AI-3 extends `engine/aimd_parse.hpp` consuming the existing `ra2assets::Ini::section()` ordered key/value list.
- **Decomposition tripwire**: if the condition-type breadth pushes the plan past ~9 tasks, the parser (Task 2) splits into its own spec→plan→execute cycle. (This plan lands at 7 — the tripwire holds.)

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/ai-trigger.md` (create, Task 1) | Calibration authority: `[AITriggerTypes]` field layout, FULL ConditionType enum + comparator encoding, weighted-selection draw, weight dynamics + success rule, enable filters + cadence, composition/attribution |
| `engine/aimd_parse.hpp` + `aimd_parse.cpp` (modify, Task 2) | EXTENDED with `AITriggerType`/`TriggerObject`/`TriggerComparator` + `AimdData::triggers` + `decode_comparator` (value-first `Conditions[4]` blob) + `parse_ai_triggers` (flat-line walk, condition_object {kind,index} resolution, team1/team2 resolution, art-guard-drop, `[AITriggerTypesEnable]` per-trigger `is_enabled`) |
| `engine/ai_trigger.hpp` + `ai_trigger.cpp` (create, Task 3) | Pure trigger brain: recovered condition/comparator/weight-rule constants, TriggerView/TriggerDecision/AiTriggerState, `trigger_condition_true`/`trigger_filters_pass`/`trigger_eligible`/`select_trigger`/`adjust_weight`/`make_trigger_state` |
| `engine/ai_team.hpp` + `ai_team.cpp` (modify, Task 4) | The ONE additive AI-2 touch [RE-GATE APPLIED: ai-trigger.md §4]: `TeamInstance::completed_success` flag, set where `step_team` executes `kOpDisband` — lifecycle transitions unchanged |
| `engine/world.hpp` + `world.cpp` (modify, Task 4) | `triggers_enabled` firewall + side/tech fields on `AiTeamConfig`, `AiTriggerState` on `HouseAiRec`, the trigger-eval step inside `step_ai_teams`'s TeamDelays check pass (creates Team1 AND Team2), Team1-identity disband attribution, `ai_fill_trigger_view` |
| `engine/main.cpp` (modify, Tasks 5–6) | `make_ai_team_config` parses `[AITriggerTypes]` house-scoped + `enable_triggers` param; interactive enemy-house trigger enable; `ai_trigger_hash` + `simulate_ai_trigger` + `run_demo_ai_trigger` + `--demo-ai-trigger` |
| `tests/test_aimd.cpp` (append, Task 2), `tests/test_ai_trigger.cpp` (create, Task 3), `tests/test_world.cpp` (append, Task 4) | per-layer pins |
| `cmake/ra2_sources.cmake` + `CMakeLists.txt` (modify, Task 3) | register `engine/ai_trigger.cpp` + `tests/test_ai_trigger.cpp` in BOTH builds (Task 2 needs NO cmake change — aimd_parse/test_aimd already registered) |
| `README.md` (modify, Task 7) | AI-3 section + `--demo-ai-trigger` gate row + deviations |

**Module-placement decision:** the `[AITriggerTypes]` parser EXTENDS `engine/aimd_parse.{hpp,cpp}` (it is aimd.ini format code and consumes `ra2assets::Ini`, and the trigger table cross-references `AimdData::teams` built in the same pass), while the decision engine is its OWN new pure module `engine/ai_trigger.{hpp,cpp}` — the parser changes with the FILE FORMAT (§1 item 1) and the engine changes with BEHAVIOR (§1 items 2–6); split by responsibility, exactly the AI-2 aimd_parse/ai_team rationale. `ai_trigger.hpp` includes `aimd_parse.hpp` (data structs) and `ai_team.hpp` (`TeamInstance` for the live-instance Max cap check).

---

### Task 1: Ghidra hunt → docs/re/ai-trigger.md

**Files:**
- Create: `docs/re/ai-trigger.md`
- Scratch: extracted aimd.ini + hunt scripts + decompile dumps under the session scratchpad (never committed; nothing off-repo committed)

**Interfaces:**
- Consumes: gamemd.exe YR 1.001 in the Ghidra project at `D:\ra2-re` (already analyzed in prior hunts; `analyzeHeadless.bat` + `-process` + `-postScript`, `JAVA_HOME="C:\Program Files\Java\jdk-21"`, FOREGROUND, generous timeouts — headless runs take minutes), plus the real aimd.ini extracted with `D:\ra2-android\build\ra2extract.exe` from the install (`RA2_INSTALL_DIR`, default `D:\Red Alert 2`) into the scratchpad.
- Known content anchors to seed the hunt — the **literal strings** the binary loads: `"AITriggerTypes"` (the section name), `"AITriggerTypesEnable"`, plus whatever AITrigger field strings the WriteINI/ReadINI pair references. YRpp/Ares addresses are UNTRUSTED — prose hints only. Prior located functions to xref-walk from (the entry map is `docs/re/ai-team.md` §located-functions): `FUN_004fd320` (House-AI tick chain — where the trigger consult must hang), `FUN_0063f940` (AutocreateTime global scheduler), `FUN_0066cf70`/`FUN_00672520` (RulesClass `[General]`/`[AI]` loaders — check them for the `AITriggerTypesEnable` consult), `FUN_006f0ab0` (TeamTypeClass::WriteINI — the struct-offset-map precedent), `FUN_0065c220`/`FUN_007c4d00` (the lockstep RNG pair every weighted draw must consult).
- Produces: `docs/re/ai-trigger.md` — sections mirroring §1 items 1–6 below, each finding **CONFIRMED / PROBABLE / [RE-FALLBACK]** with function address + decompile-excerpt reasoning, a per-finding confidence table, a located-functions table (name, address, one-line role), and a "deviations we take" section (mirroring `ai-team.md`/`ai-base.md`).

**Questions the doc MUST answer** (spec §1 items 1–6):

1. **`[AITriggerTypes]` section format + field layout:** the exact comma-field order and count of the `<id>=Name,field,field,...` line — which positions hold team1/team2, owner house, tech level, condition type/object/comparator, the starting/min/max weights, the per-difficulty enables, IsForSkirmish, side, base-defense — and which fields are load-bearing for the runtime vs recorded-only.
2. **The FULL ConditionType enum:** every value → what it tests (owner house owns ≥/≤ N of a TechnoType, enemy owns N, credits threshold, elapsed time, health thresholds, ...) plus the comparator encoding — how the operator and count threshold are packed in the trigger line (prose refs hint at a fixed-width hex blob; the hunt pins the exact on-disk encoding and byte layout). Each value marked **implementable** (testable against the TriggerView) or **inert-required-subsystem-absent** (evaluates FALSE, RE-documented).
3. **The weighted-selection mechanism:** how eligible triggers are gathered per pass and how the weighted-random draw over current weights actually works (the exact draw — cumulative-sum walk vs other); and which of team1/team2 is created for the owning side (pre-hunt: team1 for the owner, team2 the reciprocal — the hunt confirms whether team2 is ever consulted and by whom).
4. **Weight dynamics:** the starting/min/max weight semantics; the EXACT success/failure adjustment rule (multiplicative vs additive, and the delta); and what counts as success — script completion vs team survival vs condition-cleared. This is the phase's core calibration item and is NOT guessable.
5. **Enable filters + cadence:** the per-difficulty enabled flags (and their index→difficulty binding — the same hardest↔easiest caveat as ai-base.md/ai-team.md's count arrays), IsForSkirmish, Side/OwnerHouse matching (including the `<all>` token), TechLevel gating, and whatever `AITriggerTypesEnable` globally gates; plus the evaluation cadence — does trigger evaluation share AI-2's TeamDelays check pass, or run on its own AITrigger check period?
6. **Composition:** confirm triggers create teams via the SAME CreateTeam/TeamType path AI-2 uses (the ai-team.md §4 boundary: "AI-3 composes on top by adding trigger-gated CreateTeam calls against the same TeamType/TaskForce/ScriptType structures"); the TotalAITeamCap interaction (is a trigger create capped by the same per-house cap?); and how the original attributes a created team back to its source trigger for the weight feedback.

**Steps:**

- [ ] **Step 1: Re-read prior hunt method.** Read `docs/re/workflow.md` (the headless recipe), `docs/re/ai-team.md` (§located-functions — the House-AI tick chain this hunt hangs off — and the doc format, especially the `FUN_006f0ab0` WriteINI struct-offset-map technique that also applies to AITriggerTypeClass), and `docs/re/ai-base.md` (§located-functions + the count-array index→difficulty caveat). Do NOT trust YRpp/Ares addresses; use them only as prose hints for what functions look like.

- [ ] **Step 2: Extract the real aimd.ini + record stock trigger lines.** Build the extractor if stale, then pull aimd.ini from the install mixes into the scratchpad (FOREGROUND, blocking):

```powershell
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/aimd.ini "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\aimd.ini"
```

If `localmd.mix/aimd.ini` misses, walk the same nested candidates `tests/test_rules.cpp`'s real-rulesmd case uses (`expandmd01..05.mix`, `expandmd99.mix`, `cachemd.mix`, `conqmd.mix`, then top level). Record IN SCRATCH NOTES, verbatim: the full `[AITriggerTypes]` section header + at least 10 representative stock trigger lines — including at least 3 with an enemy-owns condition over a BUILDING object whose team1 is a NON-autocreate Soviet-fieldable TeamType (these become Task 2's fixture shapes and Task 6's exemplar candidates), at least 1 with a UNIT condition object, and at least 1 per DISTINCT condition-type value observed. Also record each referenced team1/team2 TeamType id and whether that TeamType has `Autocreate=yes`. The committable findings (field order, condition values, comparator encoding, weight numbers) land in ai-trigger.md as prose + numbers only; the raw ini stays in scratch.

- [ ] **Step 3: Anchor sweep (headless scripts, FOREGROUND, generous timeouts — do NOT background).** Per workflow.md's recipe (`JAVA_HOME="C:\Program Files\Java\jdk-21"`, `analyzeHeadless.bat D:\ra2-re <project> -process gamemd.exe -noanalysis -postScript <script>.py`), from the string data refs for `"AITriggerTypes"` and `"AITriggerTypesEnable"` locate the AITriggerTypeClass loader (Read_INI — the flat-line comma-field parse loop pins §1 item 1's field order) and its WriteINI twin (the struct-offset map, the `FUN_006f0ab0` technique); xref-walk to the consumers: (a) the condition-evaluation dispatch — the switch/if-chain over the condition-type field (dump the FULL case set for §1 item 2) and the comparator decode (the hex-blob unpack); (b) the per-pass eligible-trigger gather + the weighted-random draw (the RNG consult via `FUN_0065c220`/`FUN_007c4d00` inside the trigger cluster — §1 item 3) and the team1/team2 consult; (c) the weight-adjustment sites (writes to the current-weight field — who calls them, on what team event — §1 item 4); (d) the enable-filter reads (difficulty flags, IsForSkirmish, Side/OwnerHouse, TechLevel, the `AITriggerTypesEnable` global — §1 item 5) and the check-period/timer that gates the whole pass (reachable from the `FUN_004fd320` House-AI tick chain — §1 item 5 cadence); (e) the CreateTeam call the fired trigger makes + the TotalAITeamCap consult + the team→trigger back-pointer (§1 item 6). Dump decompiled candidates to the scratchpad.

- [ ] **Step 4: Answer §1 items 1–6.** Iterate scripts until each item is CONFIRMED or PROBABLE with evidence (function address + decompile-excerpt reasoning in prose), or explicitly [RE-FALLBACK] with the adjudicated fallback stated (e.g. "adjustment delta unpinnable → additive ±3.0, Task 3's fallback"). The ConditionType table must enumerate EVERY value the dispatch handles, each tagged implementable vs inert-required-subsystem-absent.

- [ ] **Step 5: Write `docs/re/ai-trigger.md`.** Sections §1–§6 mirroring the items above; the per-finding confidence table; the located-functions table; the "deviations we take" section (e.g. engine-local xorshift32 in a SEPARATE `AiTriggerState::rng` stream for the weighted draw; enemy aggregated as all-rival-houses for counts and house 0 for power; enemy-credits condition inert while World cannot see player credits; skirmish-only engine).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add docs/re/ai-trigger.md
git -C "D:\ra2-android" commit -m "docs(re): ai-trigger hunt - AITriggerTypes field layout, full ConditionType enum + comparator encoding, weighted selection, weight dynamics, enable filters + cadence, composition/attribution

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Expected: one commit, `docs/re/ai-trigger.md` created, no scripts/assets/ini dumps committed.

- [ ] **Step 7 (CONTROLLER, not the implementer): RE-GATE review.** Diff `ai-trigger.md` against every `[RE-GATE APPLIED]` block in Tasks 2–6; amend the plan; commit the amendment BEFORE dispatching Task 2. Specifically confirm/replace:
  - the **field layout + comma-order** — the pre-hunt 18-field order in Task 2's `parse_ai_triggers` (which positions hold team1/team2/owner/tech/condition/object/comparator/weights/skirmish/side/base-defense/enables), the comparator hex-blob byte layout in `decode_comparator`, and the fixture lines (Task 2 struct + parser + fixtures — replace fixture field order with the recovered one);
  - the **FULL ConditionType enum + comparator encoding + implementable/inert split** — replace Task 3's pre-hunt `kCond*` set and the `trigger_condition_true` dispatch with the recovered values; extend `TriggerView` with whatever fields the recovered enum demands (elapsed time, health, ...) and re-split implementable vs inert (Task 3 condition dispatcher + TriggerView fields + per-condition tests);
  - the **weighted-selection draw + team1/team2** — replace the pre-hunt cumulative-sum walk in `select_trigger` with the recovered draw shape, and confirm team1-only creation / whether team2 is ever consulted (Task 3 `select_trigger` + `TriggerDecision` + selection tests);
  - the **weight-adjustment rule + success definition** — replace `kAiTriggerWeightDelta` (pre-hunt additive ±3.0) with the recovered rule (multiplicative vs additive + delta) in Task 3's `adjust_weight`, and replace the pre-hunt survivors-at-disband success rule in Task 4's disband attribution with the recovered definition (script completion vs survival vs condition-cleared);
  - the **enable filters + evaluation cadence** — confirm/replace the per-difficulty `enabled[]` index binding (hardest↔easiest caveat), the IsForSkirmish/Side/OwnerHouse/`<all>`/TechLevel semantics in Task 3's `trigger_filters_pass`, what `AITriggerTypesEnable` gates, and the Task 4 hook point (shared TeamDelays check pass vs an own AITrigger check period — if own-period, Task 4 gains the recovered clock);
  - the **composition/attribution + TotalAITeamCap interaction** — confirm the trigger create is capped by the same per-house cap and per-TeamType Max (Task 4's call-site cap + Task 3 `select_trigger`'s Max check), and the attribution mechanism (Task 4's parallel `team_links` vs whatever the original stores);
  - update Task 6's exemplar-scan filter (and `kAiTriggerFrames` budget if needed) from the verbatim stock trigger lines + team scripts recorded in the Step 2 scratch notes.

---

### Task 2: aimd_parse extension — the [AITriggerTypes] flat-line parser

**Files:**
- Modify: `engine/aimd_parse.hpp`, `engine/aimd_parse.cpp`
- Test: `tests/test_aimd.cpp` (append)
- CMake: **none** — `engine/aimd_parse.cpp` and `tests/test_aimd.cpp` are ALREADY registered in both `cmake/ra2_sources.cmake` (line `${RA2_ROOT}/engine/aimd_parse.cpp`) and `CMakeLists.txt` (`target_sources(ra2assets_tests PRIVATE tests/test_aimd.cpp engine/aimd_parse.cpp)`); confirm both lines exist, change nothing.

**Interfaces:**
- Consumes: `ra2assets::Ini::section()` (NO Ini-layer changes — the flat `[AITriggerTypes]` list is exactly an ordered key/value walk, keys canonicalized UPPER, values untouched); the existing `AimdData`/`TeamType` tables + the existing file-local helpers `upper`/`truthy` in `aimd_parse.cpp`.
- Produces — appended to `engine/aimd_parse.hpp` (immediately after `struct AimdData`'s current members and the two existing free functions; `AimdData` itself gains ONE field):

```cpp
// --- AI-3 AITriggerTypes [RE: docs/re/ai-trigger.md §1] ----------------------
// The [AITriggerTypes] section is a FLAT list: each <id>=Name,field,field,...
// key is ONE complete trigger (NOT a per-id entity section like TaskForces/
// ScriptTypes/TeamTypes) — 165 stock lines. Field order is CONFIRMED
// [RE-GATE APPLIED: ai-trigger.md §1]: Name, Team1, OwnerHouse, TechLevel,
// ConditionType, ConditionObject, Conditions(blob), StartWeight, MinWeight,
// MaxWeight, IsForSkirmish, 0(unused), SideIndex, IsForBaseDefense, Team2,
// Enabled_Easy, Enabled_Normal, Enabled_Hard (the FormatForSaving comma
// order, matched 1:1 by the real aimd.ini data).

// The counted TechnoType may be a UNIT or a BUILDING [spec §2], so it resolves
// through the caller's two resolvers to a {kind, roster index} pair. None =
// no object (non-counting conditions carry "<none>").
enum class TechnoKind { None = 0, Unit = 1, Building = 2 };
struct TriggerObject {
    TechnoKind kind = TechnoKind::None;
    int index = -1;   // caller's unit- or building-roster index (by kind)
};

// Comparator = count threshold + operator, decoded from the on-disk blob
// [RE-GATE APPLIED: ai-trigger.md §2 — CONFIRMED VALUE-FIRST packing:
// dword0 = count/value N (struct +0xe4), dword1 = operator 0..5 (+0xe8);
// the on-disk field is AITriggerConditionComparator Conditions[4] = 32 bytes,
// only slot [0] meaningful]. Operators (ai_trigger.hpp kComp*): 0=< 1=<=
// 2=== 3=>= 4=> 5=!=, tested as `actualCount OP value`.
struct TriggerComparator { int value = 0; int op = 0; };

struct AITriggerType {
    std::string id;            // the flat-list key (canonical UPPER)
    std::string name;          // field 0
    int team1 = -1;            // field 1: TeamType id -> AimdData::teams index
                               // (REQUIRED — trigger dropped whole if missing;
                               // the Team1-identity attribution key [RE-GATE
                               // APPLIED: ai-trigger.md §6])
    int team2 = -1;            // field 14: second team ALSO created for the
                               // owner alongside team1 (may equal team1 -> two
                               // instances) [RE-GATE APPLIED: ai-trigger.md §3
                               // — CONFIRMED, NOT a reciprocal]; "<none>"
                               // (116/165 stock) or a dropped id -> -1
                               // tolerated (Team2 is OPTIONAL)
    std::string owner_house;   // field 2 raw token; "<all>"=Any, country name
                               // =match, "<none>"/""=reject at filter time
                               // [RE-GATE APPLIED: ai-trigger.md §5]
    int tech_level = 0;        // field 3: house tech level must be >= this
    int condition_type = -1;   // field 4: recovered enum −1..7 (ai_trigger.hpp
                               // kCond*) [RE-GATE APPLIED: ai-trigger.md §2]
    TriggerObject condition_object;  // field 5: TechnoType or <none>
    TriggerComparator comparator;    // field 6: 64-hex-char Conditions[4]
                                     // blob, decode_comparator
    double starting_weight = 0.0;    // field 7 (floats in the file)
    double min_weight = 0.0;         // field 8
    double max_weight = 0.0;         // field 9
    bool is_for_skirmish = false;    // field 10 (=1 for ALL stock)
    // field 11 is a literal 0, recorded-only [RE-GATE APPLIED: ai-trigger.md
    // §1 — CONFIRMED unused, not stored by the original either]
    int side = 0;              // field 12: SideIndex 0=any, 1=Allied,
                               // 2=Soviet, 3=Yuri [RE-GATE APPLIED:
                               // ai-trigger.md §5 — CONFIRMED]
    bool base_defense = false; // field 13: IsForBaseDefense (=0 for ALL stock)
                               // [RE-GATE APPLIED: ai-trigger.md §1]
    bool enabled[3] = {false, false, false};  // fields 15/16/17 parse in
                               // Easy,Normal,Hard ON-DISK order — enabled[0]=
                               // Easy, [1]=Normal, [2]=Hard; the RUNTIME
                               // consult is INVERTED (house difficulty index
                               // 0 = HARDEST -> enabled[2]) — ai_trigger.hpp's
                               // trigger_filters_pass owns the inversion
                               // [RE-GATE APPLIED: ai-trigger.md §5 — CONFIRMED]
    bool is_enabled = true;    // [AITriggerTypesEnable] per-trigger flag —
                               // default ENABLED when the id is unlisted
                               // [RE-GATE APPLIED: ai-trigger.md §5]
};

// Decode the on-disk comparator blob [RE-GATE APPLIED: ai-trigger.md §2 —
// CONFIRMED]: 64 hex chars = the 32-byte `Conditions[4]` field = 8 little-
// endian dwords, VALUE-FIRST — dword [0] = count/value N, dword [1] =
// operator (0..5); dwords [2..7] (Conditions[1..3]) are reserved, always 0
// in all 165 stock lines, parsed for hex validity but IGNORED for meaning.
// Returns false (caller drops the trigger) on a malformed blob (length != 64
// or non-hex anywhere).
bool decode_comparator(const std::string& hex, TriggerComparator& out);

// Parse [AITriggerTypes] into d.triggers [RE: ai-trigger.md §1]: one trigger
// per flat-list key, file order, via Ini::section(). d.teams must already be
// parsed (parse_aimd) — team1/team2 resolve against it, and a trigger whose
// team1 was dropped/missing is dropped WHOLE (team2 is optional: "<none>" or
// a dropped id -> -1). condition_object resolves through `unit_index` first,
// then `building_index` (each canonical-UPPER token -> the caller's roster
// index, or -1 = this install cannot field it); an unresolvable non-"<none>"
// object ART-GUARD-DROPS the whole trigger — skipped, never a crash, exactly
// the parse_aimd discipline [spec §2]. Then applies the SEPARATE
// [AITriggerTypesEnable] section (<trigger-id>=<bool>) to each matching
// trigger's is_enabled — a per-trigger on/off list, NOT a subsystem switch;
// unlisted ids stay enabled [RE-GATE APPLIED: ai-trigger.md §5 — CONFIRMED].
// Deterministic: output order = section file order.
void parse_ai_triggers(const ra2assets::Ini& ini, AimdData& d,
                       const std::function<int(const std::string&)>& unit_index,
                       const std::function<int(const std::string&)>& building_index);
```

`struct AimdData` gains ONE field at its tail: `std::vector<AITriggerType> triggers;  // AI-3 [RE: ai-trigger.md §1]`.

- Produces — appended to `engine/aimd_parse.cpp` (the new anonymous-namespace helpers go inside the EXISTING anonymous namespace, after `split2`; the two functions after `parse_aimd`):

```cpp
// (inside the existing anonymous namespace, after split2)
std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}
// Split the WHOLE value on every comma (trigger lines are pure comma fields;
// stock Name fields carry no embedded comma — the FormatForSaving format
// string writes pure comma fields [RE-GATE APPLIED: ai-trigger.md §1 —
// CONFIRMED]).
std::vector<std::string> split_commas(const std::string& v) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = v.find(',', start);
        if (comma == std::string::npos) { out.push_back(trim(v.substr(start))); break; }
        out.push_back(trim(v.substr(start, comma - start)));
        start = comma + 1;
    }
    return out;
}
int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// Little-endian dword from 8 hex chars (byte pairs low-to-high).
bool hex_dword(const std::string& s, std::size_t off, int& out) {
    if (off + 8 > s.size()) return false;
    unsigned v = 0;
    for (int b = 0; b < 4; ++b) {
        const int hi = hex_nibble(s[off + b * 2]);
        const int lo = hex_nibble(s[off + b * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        v |= (unsigned)((hi << 4) | lo) << (8 * b);
    }
    out = (int)v;
    return true;
}
```

```cpp
// (after parse_aimd, at namespace ra2engine scope)
bool decode_comparator(const std::string& hex, TriggerComparator& out) {
    // [RE-GATE APPLIED: ai-trigger.md §2 — CONFIRMED]: the blob is
    // AITriggerConditionComparator Conditions[4] = 32 bytes = 64 hex chars =
    // 8 LE dwords, VALUE-FIRST: dword [0] (chars 0-7) = the count/value N
    // (+0xe4), dword [1] (chars 8-15) = the operator 0..5 (+0xe8); dwords
    // [2..7] (Conditions[1..3]) are reserved — always 0 in all 165 stock
    // lines — validated as hex, IGNORED for meaning.
    if (hex.size() != 64) return false;
    if (!hex_dword(hex, 0, out.value) || !hex_dword(hex, 8, out.op))
        return false;
    int reserved = 0;                        // slots [2..7]: validity only
    for (std::size_t dw = 2; dw < 8; ++dw)
        if (!hex_dword(hex, dw * 8, reserved)) return false;
    return true;
}

void parse_ai_triggers(const ra2assets::Ini& ini, AimdData& d,
                       const std::function<int(const std::string&)>& unit_index,
                       const std::function<int(const std::string&)>& building_index) {
    std::unordered_map<std::string, int> team_ix;  // TeamType id -> teams index
    for (int i = 0; i < (int)d.teams.size(); ++i) team_ix[d.teams[i].id] = i;
    // FLAT list: every key is one full trigger, file order [RE-GATE APPLIED:
    // ai-trigger.md §1 — the 18-field comma order is CONFIRMED].
    for (const auto& kv : ini.section("AITriggerTypes")) {
        const std::vector<std::string> f = split_commas(kv.second);
        if (f.size() < 18) continue;                 // malformed line skipped
        AITriggerType t;
        t.id = kv.first;                             // key is canonical UPPER
        t.name = f[0];
        const auto t1 = team_ix.find(upper(f[1]));
        if (t1 == team_ix.end()) continue;           // team1 dropped/missing ->
        t.team1 = t1->second;                        // drop the trigger WHOLE
        t.owner_house = f[2];
        t.tech_level = std::atoi(f[3].c_str());
        t.condition_type = std::atoi(f[4].c_str());
        const std::string obj = upper(f[5]);
        if (!obj.empty() && obj != "<NONE>") {
            const int u = unit_index(obj);
            const int b = u < 0 ? building_index(obj) : -1;
            if (u >= 0)      t.condition_object = TriggerObject{TechnoKind::Unit, u};
            else if (b >= 0) t.condition_object = TriggerObject{TechnoKind::Building, b};
            else continue;                           // art-guard-drop: object
        }                                            // unresolvable -> whole
        if (!decode_comparator(f[6], t.comparator)) continue;  // malformed blob
        t.starting_weight = std::atof(f[7].c_str());
        t.min_weight = std::atof(f[8].c_str());
        t.max_weight = std::atof(f[9].c_str());
        t.is_for_skirmish = truthy(f[10]);
        // f[11] is a literal 0, recorded-only [RE-GATE APPLIED: ai-trigger.md
        // §1 — CONFIRMED unused]
        t.side = std::atoi(f[12].c_str());   // SideIndex 0=any/1=Allied/
                                             // 2=Soviet/3=Yuri [RE: §5]
        t.base_defense = truthy(f[13]);
        const auto t2 = team_ix.find(upper(f[14]));
        t.team2 = t2 == team_ix.end() ? -1 : t2->second;  // Team2 OPTIONAL
                                             // ("<none>" on 116/165) — ALSO
                                             // created when set [RE: §3]
        t.enabled[0] = truthy(f[15]);        // Easy,Normal,Hard on-disk order;
        t.enabled[1] = truthy(f[16]);        // the consult inverts at filter
        t.enabled[2] = truthy(f[17]);        // time [RE: §5]
        d.triggers.push_back(std::move(t));
    }
    // [AITriggerTypesEnable] — a SEPARATE per-trigger on/off section of
    // <trigger-id>=<bool> lines; sets is_enabled on the matching trigger.
    // Unlisted ids stay ENABLED (the struct default); ids naming dropped or
    // unknown triggers are ignored (never a crash). It gates individual
    // triggers, NOT the whole subsystem [RE-GATE APPLIED: ai-trigger.md §5 —
    // CONFIRMED: FUN_0041f300 applies the list to IsEnabled(+0xa4)].
    for (const auto& kv : ini.section("AITriggerTypesEnable")) {
        for (AITriggerType& t : d.triggers)
            if (t.id == kv.first) { t.is_enabled = truthy(kv.second); break; }
    }
}
```

(`aimd_parse.cpp` also gains `#include <vector>` transitively via the header — no new includes needed beyond the existing `<cctype>`/`<cstdlib>`/`<unordered_map>`.)

- [ ] **Step 1: Failing tests.** Append to `tests/test_aimd.cpp` (inside/after the existing anonymous namespace — `test_unit_index` from the AI-2 fixtures is reused; host-only, no install needed):

```cpp
namespace {
// Real [AITriggerTypes] flat-line shapes [RE-GATE APPLIED: ai-trigger.md §1 —
// the CONFIRMED 18-field comma order (Name,Team1,OwnerHouse,TechLevel,
// ConditionType,ConditionObject,Conditions,StartWeight,MinWeight,MaxWeight,
// IsForSkirmish,0,SideIndex,IsForBaseDefense,Team2,eE,eN,eH); ids/values are
// fixture stand-ins in the recovered FORMAT; comparator blobs are the 64-hex
// Conditions[4]: VALUE dword first, then the operator dword, LE, slots 1..3
// zero [RE: ai-trigger.md §2]].
const char* kTriggerFixture =
    "[TaskForces]\n0=01000050-G\n"
    "[ScriptTypes]\n0=03000015-G\n"
    "[TeamTypes]\n0=02000010-G\n"
    "[01000050-G]\nName=Rhino Pair\n0=2,HTNK\n"
    "[03000015-G]\nName=March\n0=3,2\n1=49,0\n"
    "[02000010-G]\nName=Sov Response\nHouse=<none>\nTaskForce=01000050-G\n"
    "Script=03000015-G\nAutocreate=no\n"
    "[AITriggerTypes]\n"
    // Kept: enemy owns >= 3 GAWEAP -> create 02000010-G (building object).
    "SOVT01=Sov Rhino Response,02000010-G,Russians,5,0,GAWEAP,"
    "0300000003000000000000000000000000000000000000000000000000000000,"
    "50.000000,10.000000,90.000000,1,0,2,0,02000010-G,1,1,1\n"
    // Dropped: condition object ZZNOART resolves in NEITHER roster.
    "SOVT02=Ghost Object,02000010-G,Russians,5,0,ZZNOART,"
    "0100000003000000000000000000000000000000000000000000000000000000,"
    "50.000000,10.000000,90.000000,1,0,2,0,<none>,1,1,1\n"
    // Dropped: team1 09999999-G does not exist in AimdData::teams.
    "SOVT03=Ghost Team,09999999-G,Russians,5,1,HTNK,"
    "0100000003000000000000000000000000000000000000000000000000000000,"
    "50.000000,10.000000,90.000000,1,0,2,0,<none>,1,1,1\n"
    // Kept: owner owns < 2 HTNK (unit object); team2 <none> -> -1;
    // Normal-only enable fields; not-for-skirmish; side any.
    "SOVT04=Owner Owns Tanks,02000010-G,<all>,5,1,HTNK,"
    "0200000000000000000000000000000000000000000000000000000000000000,"
    "30.000000,0.000000,60.000000,0,0,0,0,<none>,0,1,0\n"
    // [AITriggerTypesEnable]: SOVT04 disabled per-trigger; SOVT03 names a
    // DROPPED trigger -> ignored, no crash; SOVT01 unlisted -> stays enabled
    // [RE-GATE APPLIED: ai-trigger.md §5].
    "[AITriggerTypesEnable]\nSOVT04=no\nSOVT03=yes\n";

// Building resolver: GAWEAP -> 7, everything else unknown (-1).
int test_building_index(const std::string& id) {
    return id == "GAWEAP" ? 7 : -1;
}
}  // namespace

TEST_CASE("aimd: decode_comparator unpacks the value-first Conditions[4] blob") {
    ra2engine::TriggerComparator c;
    // VALUE-FIRST [RE-GATE APPLIED: ai-trigger.md §2 — CONFIRMED]: dword0 =
    // count/value N, dword1 = operator; 3 (>=) is the stock default op.
    REQUIRE(decode_comparator(
        "0300000003000000000000000000000000000000000000000000000000000000", c));
    CHECK(c.value == 3);
    CHECK(c.op == 3);                                    // 3 = >=
    REQUIRE(decode_comparator(
        "0a00000005000000000000000000000000000000000000000000000000000000", c));
    CHECK(c.value == 10);
    CHECK(c.op == 5);                                    // 5 = !=
    // Reserved dwords [2..7] are hex-validated but IGNORED for meaning.
    REQUIRE(decode_comparator(
        "0100000000000000ffffffff000000000000000000000000000000000000ffff", c));
    CHECK(c.value == 1);
    CHECK(c.op == 0);                                    // 0 = <
    CHECK_FALSE(decode_comparator("0a00000005000000", c));  // 16 chars: NOT a
                                                     // full 32-byte blob
    CHECK_FALSE(decode_comparator("03000000", c));       // too short
    CHECK_FALSE(decode_comparator(                       // non-hex
        "zz00000003000000000000000000000000000000000000000000000000000000", c));
}

TEST_CASE("aimd: parse_ai_triggers walks the flat list with art-guard-drop") {
    ra2assets::Ini ini = ra2assets::Ini::parse(kTriggerFixture);
    ra2engine::AimdData d = parse_aimd(ini, test_unit_index);
    REQUIRE(d.teams.size() == 1);                        // 02000010-G survived
    parse_ai_triggers(ini, d, test_unit_index, test_building_index);
    // SOVT02 (unresolvable object) and SOVT03 (missing team1) dropped WHOLE.
    REQUIRE(d.triggers.size() == 2);
    const ra2engine::AITriggerType& a = d.triggers[0];
    CHECK(a.id == "SOVT01");
    CHECK(a.name == "Sov Rhino Response");
    CHECK(a.team1 == 0);
    CHECK(a.team2 == 0);                                 // second team, ALSO
                                                         // created [RE: §3]
    CHECK(a.owner_house == "Russians");
    CHECK(a.tech_level == 5);
    CHECK(a.condition_type == 0);                        // enemy-owns
    CHECK(a.condition_object.kind == ra2engine::TechnoKind::Building);
    CHECK(a.condition_object.index == 7);                // GAWEAP
    CHECK(a.comparator.value == 3);
    CHECK(a.comparator.op == 3);
    CHECK(a.starting_weight == 50.0);
    CHECK(a.min_weight == 10.0);
    CHECK(a.max_weight == 90.0);
    CHECK(a.is_for_skirmish);
    CHECK(a.side == 2);
    CHECK_FALSE(a.base_defense);
    CHECK(a.enabled[0]); CHECK(a.enabled[1]); CHECK(a.enabled[2]);
    CHECK(a.is_enabled);                                 // unlisted in the
                                                         // Enable section ->
                                                         // default enabled
    const ra2engine::AITriggerType& b = d.triggers[1];
    CHECK(b.id == "SOVT04");
    CHECK(b.condition_type == 1);                        // owner-owns
    CHECK(b.condition_object.kind == ra2engine::TechnoKind::Unit);
    CHECK(b.condition_object.index == 0);                // HTNK
    CHECK(b.comparator.value == 2);
    CHECK(b.comparator.op == 0);                         // <
    CHECK(b.team2 == -1);                                // <none> tolerated
    CHECK_FALSE(b.is_for_skirmish);
    CHECK(b.side == 0);
    CHECK_FALSE(b.enabled[0]); CHECK(b.enabled[1]); CHECK_FALSE(b.enabled[2]);
    CHECK_FALSE(b.is_enabled);                           // [AITriggerTypesEnable]
                                                         // SOVT04=no applied
                                                         // [RE: ai-trigger.md §5]
}

TEST_CASE("aimd: parse_ai_triggers is deterministic") {
    ra2assets::Ini ini = ra2assets::Ini::parse(kTriggerFixture);
    ra2engine::AimdData a = parse_aimd(ini, test_unit_index);
    ra2engine::AimdData b = parse_aimd(ini, test_unit_index);
    parse_ai_triggers(ini, a, test_unit_index, test_building_index);
    parse_ai_triggers(ini, b, test_unit_index, test_building_index);
    REQUIRE(a.triggers.size() == b.triggers.size());
    for (std::size_t i = 0; i < a.triggers.size(); ++i)
        CHECK(a.triggers[i].id == b.triggers[i].id);
}
```

Also append INSIDE the existing install-gated `TEST_CASE("aimd: real aimd.ini parses with resolved cross-references")` (after its current checks — the case already has `ini` parsed and skips clean without the install, so no new preamble):

```cpp
    // AI-3: the real [AITriggerTypes] parses under permissive resolvers.
    ra2engine::AimdData dt = d;
    parse_ai_triggers(ini, dt, [](const std::string&) { return 0; },
                      [](const std::string&) { return 0; });
    CHECK(dt.triggers.size() > 50);   // the real file carries hundreds
    for (const ra2engine::AITriggerType& t : dt.triggers) {
        REQUIRE(t.team1 >= 0);
        REQUIRE(t.team1 < (int)dt.teams.size());
        REQUIRE(t.max_weight >= t.min_weight);
    }
    MESSAGE("real aimd: triggers=" << dt.triggers.size());
```

- [ ] **Step 2: Declarations + build FOREGROUND, verify failure.** Add the header additions (structs + `AimdData::triggers` + the two declarations) to `engine/aimd_parse.hpp` ONLY (no .cpp changes yet). Build FOREGROUND, blocking (timeout 600000 ms):

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: link FAIL (`decode_comparator`/`parse_ai_triggers` undefined) — the required red run.

- [ ] **Step 3: Implement.** Add the helpers + both function definitions to `engine/aimd_parse.cpp` exactly as shown in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="aimd*"
```

- [ ] **Step 5: Byte-identity + full suite FOREGROUND, blocking.** The extension is dead code nothing calls yet — no gate may move:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: `deterministic=yes logs=78`, `deterministic=yes logs=144`, `hash=5273b5e71ca009e1`, `econ_hash=7fed96cb98b68017`, `prod_hash=f0f4e1207ecd713c`, `ai_base_hash=cd1f7c6a29933f0a`, `ai_team_hash=312e92ed862a5e74`; full suite passes both modes (the real-aimd case skips clean without the install).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/aimd_parse.hpp engine/aimd_parse.cpp tests/test_aimd.cpp
git -C "D:\ra2-android" commit -m "feat(engine): aimd_parse AITriggerTypes extension - flat-line trigger table, comparator decode, condition-object {kind,index} resolution, art-guard-drop [RE: ai-trigger.md 1]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: engine/ai_trigger — pure condition eval + weighted selection + weight dynamics

**Files:**
- Create: `engine/ai_trigger.hpp`, `engine/ai_trigger.cpp`, `tests/test_ai_trigger.cpp`
- CMake (BOTH files): add `${RA2_ROOT}/engine/ai_trigger.cpp` to `RA2_ENGINE_SOURCES` in `cmake/ra2_sources.cmake` (after the `ai_team.cpp` line), AND in `CMakeLists.txt` (after the `test_ai_team.cpp` block, same comment style):

```cmake
# ai_trigger.cpp is pure C++17 (no SDL): the AI-3 AITriggerTypes weighted
# trigger engine -- condition eval + enable filters + weighted-random selection
# + adaptive weight dynamics over a read-only TriggerView (docs/re/ai-trigger.md).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as ai_team.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_ai_trigger.cpp engine/ai_trigger.cpp)
```

**Interfaces:**
- Consumes: `AimdData`/`AITriggerType`/`TriggerObject`/`TriggerComparator`/`TechnoKind` (aimd_parse.hpp, Task 2); `ai_rand`, `AiDifficulty` (ai_base.hpp — reused, NOT redefined); `TeamInstance` (ai_team.hpp — the live-instance Max cap check).
- Produces — `engine/ai_trigger.hpp` (new, complete):

```cpp
#pragma once
// Pure AITriggerTypes weighted trigger engine [RE: docs/re/ai-trigger.md].
// No SDL, no World — consumes a read-only TriggerView the caller (World)
// fills, plus per-house AiTriggerState. doctest-testable. EVERY numeric
// constant below is a HUNT OUTPUT recorded in docs/re/ai-trigger.md; none is
// a guess — tests assert against the named constants so a RE-GATE amendment
// updates constant, doc, and test together. All draws come from the seeded
// ai_rand xorshift32 (ai_base.hpp) over AiTriggerState::rng — a SEPARATE
// stream from AiTeamState::rng, so enabling triggers never perturbs the AI-2
// draw sequence. NEVER wall-clock.
#include <cstdint>
#include <string>
#include <vector>
#include "ai_base.hpp"     // ai_rand, AiDifficulty
#include "aimd_parse.hpp"  // AimdData / AITriggerType / TriggerObject / TriggerComparator
#include "ai_team.hpp"     // TeamInstance (live-instance Max cap check)

namespace ra2engine {

// --- Recovered ConditionType enum [RE-GATE APPLIED: ai-trigger.md §2 —
// CONFIRMED: the dispatch is switch(ConditionType+1) in FUN_0041e740, values
// −1..7, >= 8 rejected. Implementable core: −1 (always true), 0
// (EnemyHouseOwns), 1 (OwnerHouseOwns), 4 (HouseCredits — a REAL
// credits-OP-N compare; ALL 8 stock uses are N=0/op>= so it is effectively
// always-true on stock data, but our economy exposes credits), 7
// (CivilianHouseOwns — FALSE unless a neutral house exists).
// Inert-required-subsystem-absent (evaluate FALSE): 5/6 (superweapons).
// Inert/unused (evaluate FALSE): 2/3 (inline house-metric, 0 stock uses).] ---
constexpr int kCondNone = -1;               // no condition -> always TRUE
constexpr int kCondEnemyOwns = 0;           // EnemyHouseOwns: enemy owns
                                            // [op] N of condition_object
constexpr int kCondOwnerOwns = 1;           // OwnerHouseOwns: owner owns [op] N
constexpr int kCondHouseMetric2 = 2;        // inline house-metric — UNUSED in
                                            // stock, inert -> FALSE
constexpr int kCondHouseMetric3 = 3;        // inline house-metric — UNUSED in
                                            // stock, inert -> FALSE
constexpr int kCondHouseCredits = 4;        // HouseCredits: OWNER house
                                            // credits [op] N (NOT "enemy
                                            // credits") [RE: §2]
constexpr int kCondIronCurtainCharged = 5;  // inert -> FALSE (superweapons absent)
constexpr int kCondChronoSphereCharged = 6; // inert -> FALSE (superweapons absent)
constexpr int kCondCivilianHouseOwns = 7;   // neutral/civilian house owns
                                            // [op] N — FALSE without a
                                            // neutral house [RE: §2]

// --- Comparator operator encoding [RE-GATE APPLIED: ai-trigger.md §2 —
// CONFIRMED: identical switch in every condition slave; tested as
// `actualCount OP value`; 3 (>=) is the stock default (148/165 lines)] ---
constexpr int kCompLess = 0;
constexpr int kCompLessEq = 1;
constexpr int kCompEq = 2;
constexpr int kCompGreaterEq = 3;
constexpr int kCompGreater = 4;
constexpr int kCompNotEq = 5;

// --- Weight dynamics [RE-GATE APPLIED: ai-trigger.md §4 — CONFIRMED; this
// REPLACES the pre-hunt additive ±3.0. rulesmd [General]:
// AITriggerSuccessWeightDelta=20 (Rules+0xc0), AITriggerFailureWeightDelta=
// -50 (Rules+0xc8), AITriggerTrackRecordCoefficient=1 (Rules+0xd0). The rule
// is ADDITIVE and ASYMMETRIC, applied ONLY at team disband, plus a
// track-record ratio term, clamped [min_weight, max_weight] — see
// adjust_weight.] ---
constexpr double kAiTriggerSuccessWeightDelta = 20.0;
constexpr double kAiTriggerFailureWeightDelta = -50.0;
constexpr double kAiTriggerTrackRecordCoefficient = 1.0;

// Read-only battlefield snapshot, filled by the caller (World) each trigger
// pass. Count vectors are indexed by the caller's unit/building roster
// indices (the SAME indices parse_ai_triggers resolved condition_object to).
// The field set follows the RECOVERED enum [RE-GATE APPLIED: ai-trigger.md
// §2]: per-house owned-count-by-TechnoType for owner + enemy (+ neutral for
// ct=7) and the OWNER house credits — there is NO enemy-credits scalar, no
// power condition, and no time condition in the recovered enum.
struct TriggerView {
    std::vector<int> own_units;        // per unit-roster-index counts, owner house
    std::vector<int> own_buildings;    // per building-roster-index counts
    std::vector<int> enemy_units;      // enemy-owned counts (same indexing)
    std::vector<int> enemy_buildings;
    std::vector<int> neutral_units;    // neutral/civilian-house counts for
    std::vector<int> neutral_buildings;// kCondCivilianHouseOwns — leave EMPTY
                                       // when no neutral house exists (ct=7
                                       // then evaluates FALSE) [RE: §2]
    double credits = 0.0;              // OWNER house credits — kCondHouseCredits
                                       // tests credits OP N [RE-GATE APPLIED:
                                       // ai-trigger.md §2 — ct=4 is
                                       // HouseCredits, NOT enemy credits]
    int difficulty = 1;                // house difficulty index (0 = HARDEST);
                                       // the enable consult INVERTS onto
                                       // enabled[] [RE-GATE APPLIED: §5]
    std::string house_name;            // owner_house match ("<all>" = any)
    int side = 0;                      // the house's side, 1=Allied, 2=Soviet,
                                       // 3=Yuri (side-specific triggers only
                                       // match an equal house side; trigger
                                       // side 0 = any) [RE: §5]
    int tech_level = 10;               // house tech level for the TechLevel gate
};

// select_trigger's answer: which trigger fired and which TeamType(s) to
// create. BOTH Team1 AND Team2 are created for the OWNING house — Team2 is
// NOT a reciprocal team, and may equal Team1 (-> two instances) [RE-GATE
// APPLIED: ai-trigger.md §3 — CONFIRMED]. trigger = -1: none eligible,
// create nothing.
struct TriggerDecision {
    int trigger = -1;    // AimdData::triggers index (witness/weight key)
    int teamtype1 = -1;  // AimdData::teams index — Team1, always created
    int teamtype2 = -1;  // Team2 — ALSO created when >= 0 (-1 = "<none>")
};

// Per-house runtime trigger state [spec §2]. The parsed AITriggerType table
// stays IMMUTABLE — only the state here moves. Attribution needs NO
// team->trigger link: the original scans the trigger array at disband for
// Team1 == the disbanding team's TeamType [RE-GATE APPLIED: ai-trigger.md §6].
struct AiTriggerState {
    std::vector<double> weights;      // CURRENT weight per trigger, PARALLEL to
                                      // AimdData::triggers (init: starting_weight)
    std::vector<int> times_completed; // C: per-trigger success count (the
                                      // original's +0x104), PARALLEL to
                                      // triggers [RE: §4]
    std::vector<int> times_executed;  // E: per-trigger total-attempt count
                                      // (+0x108) — C/E is the track-record
                                      // ratio [RE: §4]
    std::uint32_t rng = 0x2545F491u;  // deterministic fixed seed — a SEPARATE
                                      // stream from AiTeamState::rng
    int triggers_fired = 0;        // lifetime count (witness + hash)
    int last_fired = -1;           // triggers index of the latest fire (witness)
    int adjustments = 0;           // lifetime weight adjustments (witness + hash)
    int last_adjusted = -1;        // triggers index of the latest adjust (witness)
    bool last_success = false;     // latest outcome (witness)
    double last_old_weight = 0.0;  // witness old->new
    double last_new_weight = 0.0;
};

// Fresh per-house state: weights seeded from each trigger's starting_weight;
// times_completed/times_executed zeroed [RE: ai-trigger.md §4].
AiTriggerState make_trigger_state(const AimdData& aimd);

// Comparator test [RE-GATE APPLIED: ai-trigger.md §2 — CONFIRMED operator
// semantics, applied as `lhs OP c.value`]. Unknown operator -> FALSE (inert,
// never a crash).
bool compare_count(int lhs, const TriggerComparator& c);

// Condition dispatch over the FULL recovered −1..7 enum [RE-GATE APPLIED:
// ai-trigger.md §2 — CONFIRMED]: kCondNone always TRUE; kCondEnemyOwns/
// kCondOwnerOwns/kCondCivilianHouseOwns count the condition_object in the
// enemy/own/neutral view slices through the comparator; kCondHouseCredits
// tests the OWNER house credits through the comparator (stock is always
// N=0/op>= i.e. always-true, but the REAL compare is implemented — our
// economy exposes credits [RE: §2 deviations]); kCondHouseMetric2/3
// (unused in stock) and kCondIronCurtainCharged/kCondChronoSphereCharged
// (superweapons absent) and every unknown value evaluate FALSE — exactly
// AI-2's inert-opcode discipline. An untestable counting condition (object
// None / index out of the view's range / empty neutral slices) is FALSE.
bool trigger_condition_true(const AITriggerType& t, const TriggerView& view);

// Enable-filter matrix [RE-GATE APPLIED: ai-trigger.md §5 — ConditionMet's
// CONFIRMED filter set, applied BEFORE the condition]: (1) base-defense
// split — base_defense triggers rejected (offensive scope; [RE-FALLBACK]:
// the recovered split consults the house's defensive-team need, an input
// this engine does not track; ALL stock triggers are IsForBaseDefense=0);
// (2) IsGlobal — passes vacuously (aimd-loaded triggers are never Global in
// the original); (3) is_enabled from [AITriggerTypesEnable]; (4)
// IsForSkirmish must be 1 (skirmish-only engine); (5) per-difficulty enable
// INVERTED — house difficulty index 0 (=HARDEST) -> enabled[2] (Hard), 1 ->
// enabled[1], 2 -> enabled[0] (Easy); (6) OwnerHouse — "<all>" pass, country
// name match, "<none>"/"" reject; (7) SideIndex — 0=any, else must equal
// view.side (1=Allied/2=Soviet/3=Yuri); (8) TechLevel — require
// view.tech_level >= t.tech_level.
bool trigger_filters_pass(const AITriggerType& t, const TriggerView& view);

// ELIGIBLE = condition TRUE && filters pass [spec §3].
bool trigger_eligible(const AITriggerType& t, const TriggerView& view);

// Weighted-random selection [RE-GATE APPLIED: ai-trigger.md §3 — CONFIRMED
// draw shape: gather eligible triggers with w = round(Weight_Current) (int);
// if ANY eligible w == 5000, reset the candidate set to ONLY 5000-weight
// triggers (the mandatory tier); then r = rand(1, Σw) and a cumulative-sum
// walk — the first entry with r <= cum wins. The seeded xorshift32 ai_rand
// substitutes the original's lockstep FUN_0065c220(1,Σw) [RE: deviations]].
// Eligibility additionally requires round(weight) > 0 and the per-TeamType
// Max cap for BOTH Team1 AND Team2 (Max < 0 = UNCAPPED, else live instances
// < Max) — enforced INSIDE eligibility, before the draw, as the original
// does at ConditionMet's tail [RE-GATE APPLIED: ai-trigger.md §5/§6]; the
// original's team-buildable predicate (FUN_0041ff00) is deferred to AI-2's
// recruit/produce machinery downstream [RE-FALLBACK: an unfillable team
// idles and disbands as a failure]. `weights` is the house's
// AiTriggerState::weights (PARALLEL to aimd.triggers). Deterministic: same
// seed -> same pick sequence. Returns {-1,-1,-1} when none eligible. ONE
// trigger per call — the caller enforces one per trigger pass. The caller
// checks TotalAITeamCap (a call-site gate).
TriggerDecision select_trigger(const AimdData& aimd,
                               const std::vector<double>& weights,
                               const std::vector<TeamInstance>& live,
                               const TriggerView& view, std::uint32_t& rng);

// Adaptive weight feedback [RE-GATE APPLIED: ai-trigger.md §4 — the
// CONFIRMED additive asymmetric track-record rule, applied ONLY at team
// disband; REPLACES the pre-hunt ±3.0]. With E = times_executed[trigger]
// (pre-increment), C = times_completed[trigger], K =
// kAiTriggerTrackRecordCoefficient:
//   success: d = (E > 0) ? max(0.0, (C/E - 0.5) * E) : 0;
//            weight += kAiTriggerSuccessWeightDelta + d;  then ++C, ++E
//   failure: d = (E > 0) ? min(0.0, (C/E - 0.5) * K * E) : 0;
//            weight += kAiTriggerFailureWeightDelta + d;  then ++E only
//            (C is NOT incremented)
// then CLAMP to [min_weight, max_weight] at BOTH bounds — a trigger with
// start == min == max never moves (most stock). Records the witness fields
// (adjustments/last_adjusted/last_success/last_old_weight/last_new_weight).
// Out-of-range `trigger` is a no-op (never a crash).
void adjust_weight(AiTriggerState& st, const AimdData& aimd, int trigger,
                   bool success);

}  // namespace ra2engine
```

- Produces — `engine/ai_trigger.cpp` (complete):

```cpp
#include "ai_trigger.hpp"
#include <cctype>
#include <cmath>   // std::lround — the round(Weight_Current) draw [RE: §3]

namespace ra2engine {
namespace {

bool iequal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
            return false;
    return true;
}

// Count of the condition object in the given view slices; -1 = untestable
// (no object / index out of the view's range) -> the condition is FALSE.
int object_count(const TriggerObject& o, const std::vector<int>& units,
                 const std::vector<int>& blds) {
    if (o.kind == TechnoKind::Unit)
        return (o.index >= 0 && o.index < (int)units.size()) ? units[o.index] : -1;
    if (o.kind == TechnoKind::Building)
        return (o.index >= 0 && o.index < (int)blds.size()) ? blds[o.index] : -1;
    return -1;
}

// Live-instance count vs the per-TeamType Max (Max < 0 = UNCAPPED) — the
// caps sit INSIDE eligibility, before the draw, for BOTH Team1 and Team2
// [RE-GATE APPLIED: ai-trigger.md §5/§6 — CONFIRMED: ConditionMet checks
// cntInstances < Max at its tail].
bool team_capped(const AimdData& aimd, const std::vector<TeamInstance>& live,
                 int teamtype) {
    const int max = aimd.teams[teamtype].max;
    if (max < 0) return false;                   // Max < 0 = uncapped
    int cnt = 0;
    for (const TeamInstance& ti : live)
        if (ti.teamtype == teamtype) ++cnt;
    return cnt >= max;
}

}  // namespace

AiTriggerState make_trigger_state(const AimdData& aimd) {
    AiTriggerState st;
    st.weights.reserve(aimd.triggers.size());
    for (const AITriggerType& t : aimd.triggers)
        st.weights.push_back(t.starting_weight);
    st.times_completed.assign(aimd.triggers.size(), 0);   // C [RE: §4]
    st.times_executed.assign(aimd.triggers.size(), 0);    // E [RE: §4]
    return st;
}

bool compare_count(int lhs, const TriggerComparator& c) {
    // [RE-GATE APPLIED: ai-trigger.md §2 — CONFIRMED operator semantics,
    // `lhs OP c.value`.]
    switch (c.op) {
        case kCompLess:      return lhs < c.value;
        case kCompLessEq:    return lhs <= c.value;
        case kCompEq:        return lhs == c.value;
        case kCompGreaterEq: return lhs >= c.value;
        case kCompGreater:   return lhs > c.value;
        case kCompNotEq:     return lhs != c.value;
        default:             return false;   // unknown operator -> inert FALSE
    }
}

bool trigger_condition_true(const AITriggerType& t, const TriggerView& view) {
    // [RE-GATE APPLIED: ai-trigger.md §2 — the CONFIRMED −1..7 dispatch
    // (switch(ConditionType+1) in FUN_0041e740); inert values -> FALSE, the
    // AI-2 inert discipline; >= 8 / unknown -> FALSE (the original rejects).]
    switch (t.condition_type) {
        case kCondNone:
            return true;   // no condition -> always TRUE
        case kCondEnemyOwns: {
            const int n = object_count(t.condition_object, view.enemy_units,
                                       view.enemy_buildings);
            return n >= 0 && compare_count(n, t.comparator);
        }
        case kCondOwnerOwns: {
            const int n = object_count(t.condition_object, view.own_units,
                                       view.own_buildings);
            return n >= 0 && compare_count(n, t.comparator);
        }
        case kCondHouseCredits:
            // ct=4 HouseCredits [RE-GATE APPLIED: ai-trigger.md §2 — the
            // OWNER house money scalar through the comparator, NOT enemy
            // credits. Stock is always N=0/op>= (effectively always-true);
            // the REAL compare is implemented — our economy exposes credits
            // [RE: §2 deviations].]
            return compare_count((int)view.credits, t.comparator);
        case kCondCivilianHouseOwns: {
            // ct=7: counts the NEUTRAL/civilian house; FALSE unless a
            // neutral house exists (empty neutral slices -> -1) [RE: §2].
            const int n = object_count(t.condition_object, view.neutral_units,
                                       view.neutral_buildings);
            return n >= 0 && compare_count(n, t.comparator);
        }
        default:
            // kCondHouseMetric2 / kCondHouseMetric3 (unused in stock) +
            // kCondIronCurtainCharged / kCondChronoSphereCharged
            // (superweapons absent) + every unknown value: INERT -> FALSE
            // [RE: ai-trigger.md §2].
            return false;
    }
}

bool trigger_filters_pass(const AITriggerType& t, const TriggerView& view) {
    // [RE-GATE APPLIED: ai-trigger.md §5 — ConditionMet's CONFIRMED filter
    // set, in the recovered order. Filter #2 (IsGlobal) passes vacuously:
    // aimd-loaded triggers are never Global in the original (IsGlobal=0),
    // so the field is not parsed or checked.]
    // #1 base-defense vs offense split: AI-3's scope is OFFENSIVE triggers;
    // ALL stock triggers are IsForBaseDefense=0. [RE-FALLBACK: the recovered
    // split consults the house's defensive-team need (Rules+0x1394[diff] vs
    // house+0x566c), inputs this engine does not track — base-defense
    // triggers are rejected until a defensive-need input exists.]
    if (t.base_defense) return false;
    // #3 per-trigger [AITriggerTypesEnable] flag [RE: §5].
    if (!t.is_enabled) return false;
    // #4 IsForSkirmish must be 1 — skirmish-only engine [RE: §5].
    if (!t.is_for_skirmish) return false;
    // #5 per-difficulty enable, INVERTED [RE-GATE APPLIED: ai-trigger.md §5
    // — CONFIRMED]: house difficulty index 0 (=HARDEST) consults
    // Enabled_Hard = enabled[2], 1 -> enabled[1], 2 -> Enabled_Easy =
    // enabled[0] — the ai-base/ai-team hardest<->easiest inversion.
    if (view.difficulty < 0 || view.difficulty > 2 ||
        !t.enabled[2 - view.difficulty])
        return false;
    // #6 OwnerHouse [RE: §5 — CONFIRMED]: "<all>" (Any) -> pass; a country
    // name -> case-insensitive match; "<none>"/"" (OwnerHouseType 0) -> REJECT.
    if (!iequal(t.owner_house, "<all>")) {
        if (t.owner_house.empty() || iequal(t.owner_house, "<none>"))
            return false;
        if (!iequal(t.owner_house, view.house_name)) return false;
    }
    // #7 SideIndex [RE: §5 — CONFIRMED]: 0=any, else must equal the house
    // side (1=Allied, 2=Soviet, 3=Yuri).
    if (t.side != 0 && t.side != view.side) return false;
    // #8 TechLevel [RE: §5 — CONFIRMED]: require house tech level >= field.
    if (t.tech_level > view.tech_level) return false;
    return true;
}

bool trigger_eligible(const AITriggerType& t, const TriggerView& view) {
    return trigger_condition_true(t, view) && trigger_filters_pass(t, view);
}

TriggerDecision select_trigger(const AimdData& aimd,
                               const std::vector<double>& weights,
                               const std::vector<TeamInstance>& live,
                               const TriggerView& view, std::uint32_t& rng) {
    TriggerDecision d;
    // Gather eligible triggers with ROUNDED integer weights [RE-GATE APPLIED:
    // ai-trigger.md §3 — CONFIRMED: the driver draws over
    // round(Weight_Current) (FUN_007c4d00) via rand(1, Σw)].
    std::vector<int> elig;
    std::vector<long> wts;
    long total = 0;
    bool tier5000 = false;
    for (int i = 0; i < (int)aimd.triggers.size() && i < (int)weights.size(); ++i) {
        const AITriggerType& t = aimd.triggers[i];
        const long w = std::lround(weights[i]);
        if (w <= 0) continue;                        // no probability mass
        if (!trigger_eligible(t, view)) continue;
        // Per-TeamType Max cap for BOTH Team1 and Team2, INSIDE eligibility,
        // before the draw (Max < 0 = UNCAPPED) [RE-GATE APPLIED:
        // ai-trigger.md §5/§6 — CONFIRMED at ConditionMet's tail]. The
        // buildable predicate (FUN_0041ff00) is deferred to AI-2's
        // recruit/produce machinery [RE-FALLBACK].
        if (team_capped(aimd, live, t.team1)) continue;
        if (t.team2 >= 0 && team_capped(aimd, live, t.team2)) continue;
        // The 5000 MANDATORY tier [RE-GATE APPLIED: ai-trigger.md §3 —
        // CONFIRMED]: if ANY eligible trigger's rounded weight == 5000, the
        // candidate set resets to ONLY 5000-weight triggers.
        if (w == 5000 && !tier5000) {
            tier5000 = true;
            elig.clear();
            wts.clear();
            total = 0;
        }
        if (tier5000 && w != 5000) continue;
        elig.push_back(i);
        wts.push_back(w);
        total += w;
    }
    if (elig.empty() || total <= 0) return d;
    // r = rand(1, Σw), then the cumulative-sum walk: the FIRST entry with
    // r <= cum wins [RE-GATE APPLIED: ai-trigger.md §3 — CONFIRMED; the
    // seeded xorshift32 substitutes the lockstep FUN_0065c220(1, Σw)
    // (deviation, see ai-trigger.md deviations)].
    const long r = 1 + (long)(ai_rand(rng) % (std::uint32_t)total);
    long cum = 0;
    for (std::size_t k = 0; k < elig.size(); ++k) {
        cum += wts[k];
        if (r <= cum) {
            d.trigger = elig[k];
            d.teamtype1 = aimd.triggers[elig[k]].team1;
            d.teamtype2 = aimd.triggers[elig[k]].team2;  // BOTH created for
            return d;                                    // the owner [RE: §3]
        }
    }
    return d;
}

void adjust_weight(AiTriggerState& st, const AimdData& aimd, int trigger,
                   bool success) {
    if (trigger < 0 || trigger >= (int)st.weights.size() ||
        trigger >= (int)aimd.triggers.size())
        return;
    const AITriggerType& t = aimd.triggers[trigger];
    const double old = st.weights[trigger];
    // [RE-GATE APPLIED: ai-trigger.md §4 — the CONFIRMED additive asymmetric
    // track-record rule (RegisterSuccess FUN_0041fd80 / RegisterFailure
    // FUN_0041fe40), clamped at BOTH bounds; E/C pre-increment.]
    const int E = st.times_executed[trigger];
    const int C = st.times_completed[trigger];
    double delta = 0.0;
    double w = old;
    if (success) {
        if (E > 0) {
            delta = ((double)C / (double)E - 0.5) * (double)E;
            if (delta < 0.0) delta = 0.0;            // bonus >= 0
        }
        w += kAiTriggerSuccessWeightDelta + delta;
    } else {
        if (E > 0) {
            delta = ((double)C / (double)E - 0.5) *
                    kAiTriggerTrackRecordCoefficient * (double)E;
            if (delta > 0.0) delta = 0.0;            // penalty <= 0
        }
        w += kAiTriggerFailureWeightDelta + delta;
    }
    if (w < t.min_weight) w = t.min_weight;
    if (w > t.max_weight) w = t.max_weight;
    st.weights[trigger] = w;
    if (success) ++st.times_completed[trigger];      // ++C on success ONLY
    ++st.times_executed[trigger];                    // ++E on every outcome
    ++st.adjustments;
    st.last_adjusted = trigger;
    st.last_success = success;
    st.last_old_weight = old;
    st.last_new_weight = w;
}

}  // namespace ra2engine
```

- [ ] **Step 1: Failing tests.** Create `tests/test_ai_trigger.cpp` (host-only, inline fixtures, no install needed):

```cpp
#include "doctest.h"
#include "../engine/ai_trigger.hpp"
using namespace ra2engine;

namespace {
// One-trigger AimdData: TT0 = a Max=1 team; TRG0 = enemy owns >= 2 of
// building 0, weights 50/[10,90], all difficulties, skirmish, any side/owner.
AimdData one_trigger_aimd() {
    AimdData d;
    TaskForce tf; tf.id = "TF"; tf.members = {{1, 0}};
    d.taskforces.push_back(tf);
    ScriptType sc; sc.id = "SC"; sc.actions = {{49, 0}};
    d.scripts.push_back(sc);
    TeamType tt; tt.id = "TT0"; tt.taskforce = 0; tt.script = 0; tt.max = 1;
    d.teams.push_back(tt);
    AITriggerType t;
    t.id = "TRG0"; t.name = "enemy owns 2 bld0";
    t.team1 = 0; t.team2 = -1;
    t.owner_house = "<all>"; t.tech_level = 0;
    t.condition_type = kCondEnemyOwns;
    t.condition_object = TriggerObject{TechnoKind::Building, 0};
    t.comparator = TriggerComparator{2, kCompGreaterEq};
    t.starting_weight = 50.0; t.min_weight = 10.0; t.max_weight = 90.0;
    t.is_for_skirmish = true; t.side = 0; t.base_defense = false;
    t.enabled[0] = t.enabled[1] = t.enabled[2] = true;
    t.is_enabled = true;    // [AITriggerTypesEnable] default [RE: §5]
    d.triggers.push_back(t);
    return d;
}
// A view where TRG0's condition is TRUE (enemy owns 3 of building 0).
// Soviet house (side 2), Normal difficulty index 1, no neutral house.
TriggerView firing_view() {
    TriggerView v;
    v.own_units = {0}; v.enemy_units = {0};
    v.own_buildings = {0}; v.enemy_buildings = {3};
    v.difficulty = 1;
    v.house_name = "Russians"; v.side = 2; v.tech_level = 10;
    return v;
}
}  // namespace

TEST_CASE("ai_trigger: compare_count implements all six operators") {
    CHECK(compare_count(1, {2, kCompLess}));
    CHECK_FALSE(compare_count(2, {2, kCompLess}));
    CHECK(compare_count(2, {2, kCompLessEq}));
    CHECK(compare_count(2, {2, kCompEq}));
    CHECK_FALSE(compare_count(3, {2, kCompEq}));
    CHECK(compare_count(2, {2, kCompGreaterEq}));
    CHECK_FALSE(compare_count(1, {2, kCompGreaterEq}));
    CHECK(compare_count(3, {2, kCompGreater}));
    CHECK_FALSE(compare_count(2, {2, kCompGreater}));
    CHECK(compare_count(3, {2, kCompNotEq}));
    CHECK_FALSE(compare_count(2, {2, kCompNotEq}));
    CHECK_FALSE(compare_count(2, {2, 99}));   // unknown operator -> FALSE
}

TEST_CASE("ai_trigger: condition dispatch over the recovered -1..7 enum") {
    AimdData d = one_trigger_aimd();
    TriggerView v = firing_view();
    AITriggerType t = d.triggers[0];
    // ct=0 EnemyHouseOwns (building object): 3 >= 2 -> TRUE; 1 >= 2 -> FALSE.
    CHECK(trigger_condition_true(t, v));
    v.enemy_buildings[0] = 1;
    CHECK_FALSE(trigger_condition_true(t, v));
    // ct=0 over a UNIT object.
    t.condition_object = TriggerObject{TechnoKind::Unit, 0};
    v.enemy_units[0] = 5;
    CHECK(trigger_condition_true(t, v));
    // ct=1 OwnerHouseOwns.
    t.condition_type = kCondOwnerOwns;
    v.own_units[0] = 2;
    CHECK(trigger_condition_true(t, v));
    v.own_units[0] = 1;
    CHECK_FALSE(trigger_condition_true(t, v));
    // ct=-1 kCondNone: always TRUE, no object consulted.
    t.condition_type = kCondNone;
    t.condition_object = TriggerObject{};
    CHECK(trigger_condition_true(t, v));
    // ct=4 HouseCredits: OWNER credits OP N — a REAL compare [RE-GATE
    // APPLIED: ai-trigger.md §2 — NOT enemy credits]. Stock shape
    // (N=0, op >=) is always true.
    t.condition_type = kCondHouseCredits;
    t.comparator = TriggerComparator{0, kCompGreaterEq};
    v.credits = 0.0;
    CHECK(trigger_condition_true(t, v));       // the stock always-true shape
    t.comparator = TriggerComparator{1000, kCompGreaterEq};
    v.credits = 999.0;
    CHECK_FALSE(trigger_condition_true(t, v));
    v.credits = 1000.0;
    CHECK(trigger_condition_true(t, v));
    // ct=7 CivilianHouseOwns: counts the NEUTRAL house; FALSE while no
    // neutral house exists (empty neutral slices) [RE: ai-trigger.md §2].
    t.condition_type = kCondCivilianHouseOwns;
    t.condition_object = TriggerObject{TechnoKind::Building, 0};
    t.comparator = TriggerComparator{1, kCompGreaterEq};
    CHECK_FALSE(trigger_condition_true(t, v));  // no neutral house
    v.neutral_buildings = {2};
    CHECK(trigger_condition_true(t, v));
    v.neutral_buildings = {0};
    CHECK_FALSE(trigger_condition_true(t, v));
    // Inert values evaluate FALSE [RE: ai-trigger.md §2]: ct=2/3 (unused
    // inline house-metric), ct=5/6 (superweapons absent), >= 8 (rejected).
    v.neutral_buildings = {5};
    for (int cond : {kCondHouseMetric2, kCondHouseMetric3,
                     kCondIronCurtainCharged, kCondChronoSphereCharged,
                     8, 99}) {
        t.condition_type = cond;
        CHECK_FALSE(trigger_condition_true(t, v));
    }
    // Untestable counting condition (object None / out-of-range) -> FALSE.
    t.condition_type = kCondEnemyOwns;
    t.comparator = TriggerComparator{2, kCompGreaterEq};
    t.condition_object = TriggerObject{};
    CHECK_FALSE(trigger_condition_true(t, v));
    t.condition_object = TriggerObject{TechnoKind::Building, 99};
    CHECK_FALSE(trigger_condition_true(t, v));
}

TEST_CASE("ai_trigger: the enable-filter matrix") {
    AimdData d = one_trigger_aimd();
    TriggerView v = firing_view();
    AITriggerType t = d.triggers[0];
    CHECK(trigger_filters_pass(t, v));
    // #1 base-defense split: offensive scope -> base-defense triggers
    // rejected [RE-FALLBACK: no defensive-need input; all stock are
    // IsForBaseDefense=0].
    t.base_defense = true;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.base_defense = false;
    // #3 [AITriggerTypesEnable] per-trigger flag [RE: ai-trigger.md §5].
    t.is_enabled = false;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.is_enabled = true;
    // #4 IsForSkirmish must be 1 (skirmish-only engine) [RE: §5].
    t.is_for_skirmish = false;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.is_for_skirmish = true;
    // #5 per-difficulty enable INVERTED [RE-GATE APPLIED: ai-trigger.md §5 —
    // house difficulty index 0 = HARDEST -> Enabled_Hard = enabled[2];
    // 1 -> enabled[1]; 2 -> Enabled_Easy = enabled[0]].
    t.enabled[1] = false;            // Normal slot; view difficulty 1 ->
    CHECK_FALSE(trigger_filters_pass(t, v));  // enabled[2-1] (inversion-
    t.enabled[1] = true;                      // neutral middle)
    v.difficulty = 0;                // HARDEST -> consults enabled[2] (Hard)
    t.enabled[2] = false; t.enabled[0] = true;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.enabled[2] = true; t.enabled[0] = false;
    CHECK(trigger_filters_pass(t, v));  // Easy flag off does NOT gate hardest
    v.difficulty = 2;                // EASIEST -> consults enabled[0] (Easy)
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.enabled[0] = true;
    CHECK(trigger_filters_pass(t, v));
    v.difficulty = 1;
    // #7 SideIndex: 0=any, 1=Allied, 2=Soviet, 3=Yuri; the view is a Soviet
    // house (side 2) [RE: §5 — CONFIRMED 1-based, 0=any].
    t.side = 1;                      // Allied trigger, Soviet house
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.side = 3;                      // Yuri trigger, Soviet house
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.side = 2;
    CHECK(trigger_filters_pass(t, v));
    t.side = 0;
    // #6 OwnerHouse: <all> = Any -> pass; country name -> case-insensitive
    // match; <none>/"" -> REJECT (OwnerHouseType 0) [RE-GATE APPLIED:
    // ai-trigger.md §5 — <none> is NOT "any"].
    t.owner_house = "Russians";
    CHECK(trigger_filters_pass(t, v));
    t.owner_house = "RUSSIANS";
    CHECK(trigger_filters_pass(t, v));
    t.owner_house = "Americans";
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.owner_house = "<none>";
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.owner_house = "";
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.owner_house = "<all>";
    // #8 TechLevel gate: house tech level >= field.
    t.tech_level = 11;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.tech_level = 10;
    CHECK(trigger_filters_pass(t, v));
    // Eligibility = condition && filters.
    CHECK(trigger_eligible(t, v));
    v.enemy_buildings[0] = 0;
    CHECK_FALSE(trigger_eligible(t, v));
}

TEST_CASE("ai_trigger: selection - sole eligible, determinism, caps, none") {
    AimdData d = one_trigger_aimd();
    TriggerView v = firing_view();
    std::vector<TeamInstance> live;
    // Sole eligible trigger is picked with probability 1 across a seed sweep
    // (r = rand(1, 50) always lands inside its cumulative span).
    for (std::uint32_t seed = 1; seed <= 100; ++seed) {
        std::uint32_t rng = seed;
        std::vector<double> w = {50.0};
        const TriggerDecision td = select_trigger(d, w, live, v, rng);
        CHECK(td.trigger == 0);
        CHECK(td.teamtype1 == 0);
        CHECK(td.teamtype2 == -1);    // Team2 "<none>" -> no second create
    }
    // None eligible -> {-1,-1,-1} (condition false).
    TriggerView cold = v;
    cold.enemy_buildings[0] = 0;
    std::uint32_t rng = 7u;
    CHECK(select_trigger(d, {50.0}, live, cold, rng).trigger == -1);
    // round(weight) <= 0 -> skipped (no probability mass) [RE: §3 — the
    // draw runs over round(Weight_Current)].
    CHECK(select_trigger(d, {0.0}, live, v, rng).trigger == -1);
    CHECK(select_trigger(d, {0.4}, live, v, rng).trigger == -1);  // rounds to 0
    // Team1 Max cap: TT0 has max=1; one live instance blocks the trigger —
    // the cap sits INSIDE eligibility, before the draw [RE-GATE APPLIED:
    // ai-trigger.md §5/§6].
    TeamInstance ti; ti.teamtype = 0;
    live.push_back(ti);
    CHECK(select_trigger(d, {50.0}, live, v, rng).trigger == -1);
    live.clear();
    // Max < 0 = UNCAPPED [RE-GATE APPLIED: ai-trigger.md §5/§6 — CONFIRMED].
    d.teams[0].max = -1;
    live.push_back(ti); live.push_back(ti);
    CHECK(select_trigger(d, {50.0}, live, v, rng).trigger == 0);
    // Team2 rides the SAME cap: a Team2 at its Max blocks the whole trigger;
    // under the cap, BOTH team indices come back [RE: §3/§6].
    live.clear();
    TeamType tt1 = d.teams[0]; tt1.id = "TT1"; tt1.max = 1;
    d.teams.push_back(tt1);
    d.triggers[0].team2 = 1;
    TeamInstance t2i; t2i.teamtype = 1;
    live.push_back(t2i);
    CHECK(select_trigger(d, {50.0}, live, v, rng).trigger == -1);
    live.clear();
    const TriggerDecision both = select_trigger(d, {50.0}, live, v, rng);
    CHECK(both.trigger == 0);
    CHECK(both.teamtype1 == 0);
    CHECK(both.teamtype2 == 1);       // BOTH Team1 and Team2 created [RE: §3]
}

TEST_CASE("ai_trigger: selection determinism, weight convergence, 5000 tier") {
    // Two eligible triggers over the same team-less shapes.
    AimdData d = one_trigger_aimd();
    d.teams[0].max = -1;                       // uncapped for this case
    AITriggerType second = d.triggers[0];
    second.id = "TRG1";
    d.triggers.push_back(second);
    TriggerView v = firing_view();
    std::vector<TeamInstance> live;
    // Same seed -> identical pick sequence.
    std::vector<int> seq1, seq2;
    std::uint32_t r1 = 0x2545F491u, r2 = 0x2545F491u;
    for (int i = 0; i < 10; ++i) {
        seq1.push_back(select_trigger(d, {50.0, 50.0}, live, v, r1).trigger);
        seq2.push_back(select_trigger(d, {50.0, 50.0}, live, v, r2).trigger);
    }
    CHECK(seq1 == seq2);
    // Dominant weight: with rounded weights 90 vs 10, the dominant trigger's
    // empirical pick frequency tracks the ratio across a seed sweep
    // (>= 70% is a safe bound for a 90/10 split over 500 seeds).
    int dominant = 0;
    for (std::uint32_t seed = 1; seed <= 500; ++seed) {
        std::uint32_t rng = seed;
        if (select_trigger(d, {90.0, 10.0}, live, v, rng).trigger == 0)
            ++dominant;
    }
    CHECK(dominant >= 350);
    CHECK(dominant < 500);   // the low-weight trigger IS reachable
    // The 5000 MANDATORY tier [RE-GATE APPLIED: ai-trigger.md §3 —
    // CONFIRMED]: an eligible trigger whose ROUNDED weight == 5000 resets
    // the candidate set to the 5000 tier only — the 90-weight trigger is
    // NEVER picked, any seed.
    for (std::uint32_t seed = 1; seed <= 200; ++seed) {
        std::uint32_t rng = seed;
        CHECK(select_trigger(d, {90.0, 5000.0}, live, v, rng).trigger == 1);
    }
    // Rounding feeds the tier check: 4999.6 rounds to 5000.
    std::uint32_t rr = 3u;
    CHECK(select_trigger(d, {90.0, 4999.6}, live, v, rr).trigger == 1);
}

TEST_CASE("ai_trigger: weight dynamics - additive +20/-50 track-record rule, clamped") {
    AimdData d = one_trigger_aimd();               // 50 / [10, 90]
    AiTriggerState st = make_trigger_state(d);
    REQUIRE(st.weights.size() == 1);
    REQUIRE(st.times_completed.size() == 1);
    REQUIRE(st.times_executed.size() == 1);
    CHECK(st.weights[0] == 50.0);              // seeded from starting_weight
    CHECK(st.times_completed[0] == 0);
    CHECK(st.times_executed[0] == 0);
    // Success #1 [RE-GATE APPLIED: ai-trigger.md §4 — CONFIRMED formula]:
    // E=0 -> d=0 -> 50 + 20 = 70; then ++C, ++E.
    adjust_weight(st, d, 0, /*success=*/true);
    CHECK(st.weights[0] == 50.0 + kAiTriggerSuccessWeightDelta);
    CHECK(st.times_completed[0] == 1);
    CHECK(st.times_executed[0] == 1);
    CHECK(st.adjustments == 1);
    CHECK(st.last_adjusted == 0);
    CHECK(st.last_success);
    CHECK(st.last_old_weight == 50.0);
    CHECK(st.last_new_weight == 70.0);
    // Success #2: E=1, C=1 -> d = max(0, (1/1 - 0.5)*1) = 0.5 ->
    // 70 + 20 + 0.5 = 90.5 -> CLAMPED to max_weight 90.
    adjust_weight(st, d, 0, true);
    CHECK(st.weights[0] == 90.0);
    CHECK(st.times_completed[0] == 2);
    CHECK(st.times_executed[0] == 2);
    // Failure #1: E=2, C=2 -> d = min(0, (2/2 - 0.5)*1*2) = 0 ->
    // 90 - 50 = 40; C NOT incremented [RE: §4].
    adjust_weight(st, d, 0, /*success=*/false);
    CHECK(st.weights[0] == 40.0);
    CHECK(st.times_completed[0] == 2);
    CHECK(st.times_executed[0] == 3);
    CHECK_FALSE(st.last_success);
    // Failure #2: E=3, C=2 -> ratio > 0.5 -> penalty term clamps to 0 ->
    // 40 - 50 = -10 -> CLAMPED to min_weight 10.
    adjust_weight(st, d, 0, false);
    CHECK(st.weights[0] == 10.0);
    CHECK(st.times_executed[0] == 4);
    // The track-record term never brightens a failure or dims a success:
    // fresh state, two failures (-> E=2, C=0), then a success — the bonus is
    // max(0, (0 - 0.5)*2) = 0, so 10 + 20 = 30.
    AiTriggerState st2 = make_trigger_state(d);
    adjust_weight(st2, d, 0, false);               // E=0: 50-50 -> clamp 10
    CHECK(st2.weights[0] == 10.0);
    adjust_weight(st2, d, 0, false);               // E=1,C=0: d=-0.5 -> clamp 10
    CHECK(st2.weights[0] == 10.0);
    adjust_weight(st2, d, 0, true);                // E=2,C=0: bonus 0 -> 30
    CHECK(st2.weights[0] == 30.0);
    CHECK(st2.times_completed[0] == 1);
    CHECK(st2.times_executed[0] == 3);
    // start == min == max PINS the weight — most stock triggers never move
    // [RE: ai-trigger.md §4].
    d.triggers[0].starting_weight = 70.0;
    d.triggers[0].min_weight = 70.0;
    d.triggers[0].max_weight = 70.0;
    AiTriggerState st3 = make_trigger_state(d);
    adjust_weight(st3, d, 0, true);
    CHECK(st3.weights[0] == 70.0);
    adjust_weight(st3, d, 0, false);
    CHECK(st3.weights[0] == 70.0);
    // Out-of-range trigger: no-op, no crash.
    const int before = st.adjustments;
    adjust_weight(st, d, 5, true);
    CHECK(st.adjustments == before);
}
```

- [ ] **Step 2: Register + build FOREGROUND, verify failure.** Edit `cmake/ra2_sources.cmake` (append `${RA2_ROOT}/engine/ai_trigger.cpp` after the `ai_team.cpp` line) and `CMakeLists.txt` (the `target_sources` block above, after the `test_ai_team.cpp` block). Create `engine/ai_trigger.hpp` as shown, and `engine/ai_trigger.cpp` with ONLY the include + empty `namespace ra2engine {}` (so CMake finds the listed file). Build FOREGROUND, blocking (timeout 600000 ms):

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: link FAIL (`select_trigger`/`adjust_weight`/`trigger_condition_true`/... undefined) — the required red run.

- [ ] **Step 3: Implement.** Fill `engine/ai_trigger.cpp` exactly as shown in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="ai_trigger*"
```

- [ ] **Step 5: Byte-identity + full suite FOREGROUND, blocking.** `ai_trigger.cpp` is a brand-new pure TU nothing calls yet — no gate may move:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: `deterministic=yes logs=78`, `deterministic=yes logs=144`, `hash=5273b5e71ca009e1`, `econ_hash=7fed96cb98b68017`, `prod_hash=f0f4e1207ecd713c`, `ai_base_hash=cd1f7c6a29933f0a`, `ai_team_hash=312e92ed862a5e74`; full suite passes both modes.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/ai_trigger.hpp engine/ai_trigger.cpp tests/test_ai_trigger.cpp cmake/ra2_sources.cmake CMakeLists.txt
git -C "D:\ra2-android" commit -m "feat(engine): ai_trigger - pure AITriggerTypes condition eval + enable filters + weighted selection + clamped weight dynamics [RE: ai-trigger.md 2-5]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: World integration — trigger-eval step in step_ai_teams + outcome attribution

**Files:**
- Modify: `engine/world.hpp`, `engine/world.cpp`, `engine/ai_team.hpp`, `engine/ai_team.cpp` (the ONE additive `completed_success` flag — see Step 3(a))
- Test: `tests/test_world.cpp` (append)
- CMake: none (world.cpp/ai_team.cpp registered; ai_trigger.cpp registered in Task 3).

**Interfaces:**
- Consumes: `AiTriggerState`/`TriggerView`/`TriggerDecision`/`make_trigger_state`/`select_trigger`/`adjust_weight` (Task 3); `AITriggerType`/`TriggerObject`/`TechnoKind`/`AimdData::triggers` (Task 2); EXISTING machinery: `step_ai_teams`'s TeamDelays check pass (the `st.check_clock >= check_period` block at world.cpp ~1290-1315 where `select_autocreate_team` is called), the disband erase point (~1439), `ai_unit_type_of`, `units_`/`buildings_`, `HouseAiRec`, `enable_ai_house`. **NO new tick loop, no new firewall. ONE deliberate ADDITIVE AI-2 touch [RE-GATE APPLIED: ai-trigger.md §4 — the spec's "zero AI-2 changes" is relaxed to this]: `TeamInstance` gains a `completed_success` bool set where `step_team` executes `kOpDisband`; every lifecycle transition is UNCHANGED.**
- Produces — `engine/world.hpp` changes:

`world.hpp` gains `#include "ai_trigger.hpp"` immediately after the existing `#include "ai_team.hpp"` line, with the comment `// AI-3 trigger engine: AiTriggerState/TriggerView/TriggerDecision`.

`struct AiTeamConfig` gains THREE fields at its tail:

```cpp
    // --- AI-3 AITriggerTypes [RE: ai-trigger.md] -----------------------------
    // BYTE-IDENTITY FIREWALL: triggers_enabled defaults FALSE — every existing
    // caller (including --demo-ai-team) gets a trigger-less brain, the trigger
    // step never runs, and all SEVEN gates stay bit-identical. Only the
    // interactive path and --demo-ai-trigger set it true.
    bool triggers_enabled = false;  // the per-house trigger firewall
    int trigger_side = 0;           // the house's side for the SideIndex
                                    // filter (1=Allied, 2=Soviet, 3=Yuri;
                                    // triggers with side 0 match any house)
                                    // [RE-GATE APPLIED: ai-trigger.md §5 —
                                    // CONFIRMED 1-based encoding]
    int trigger_tech_level = 10;    // house tech level for the TechLevel
                                    // filter (house >= field passes)
                                    // [RE-FALLBACK: skirmish ceiling — no
                                    // recovered per-house tech input]
```

`struct HouseAiRec` gains ONE field: `AiTriggerState trigger_state;  // AI-3 [RE: ai-trigger.md]`.

`class World` public (after `ai_unit_producing`):

```cpp
    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    // Read-only trigger state (nullptr when `house` is not AI-flagged). NOT
    // read by world_hash — the 8th gate hashes it separately (ai_trigger_hash).
    const AiTriggerState* ai_trigger_state(int house) const;
```

`class World` private (after `ai_launch_unit_ready`):

```cpp
    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    void ai_fill_trigger_view(int house, const HouseAiRec& rec, TriggerView& v) const;
```

- [ ] **Step 1: Failing tests** (append to `tests/test_world.cpp` after the AI-2 team cases; reuses the file's `load_real_map_w` skip helper and the existing `ai2_cfg` fixture from the AI-2 block):

```cpp
// ---- AI-3 AITriggerTypes phase [RE: ai-trigger.md] : trigger-eval step ----

namespace {
// AI-3 trigger cfg: ai2_cfg(true) with the autocreate path SILENCED
// (autocreate=false on every fixture TeamType — a TEST-ISOLATION device so
// only the trigger path can create; ALL 163 stock TeamTypes are
// Autocreate=yes and the trigger pool == the autocreate pool [RE-GATE
// APPLIED: ai-trigger.md §6]) + ONE TeamType TT2 reachable ONLY through the
// trigger path + ONE trigger keyed "enemy owns >= 1 of building roster type
// 0 (NACNST)" with Team2 = <none>. Script overridden to bare disband so the
// outcome lands fast (kOpDisband sets completed_success -> RegisterSuccess
// [RE: ai-trigger.md §4]). Fast test clocks carry over from ai2_cfg.
ra2engine::AiHouseConfig ai3_cfg(bool triggers_enabled, int total_team_cap = 30) {
    ra2engine::AiHouseConfig cfg = ai2_cfg(/*teams_enabled=*/true);
    cfg.team.aimd.teams[0].autocreate = false;   // silence AI-2 autocreate
    cfg.team.aimd.scripts[0].actions =
        {ra2engine::ScriptAction{ra2engine::kOpDisband, 0}};
    ra2engine::TeamType tt;
    tt.id = "TT2";
    tt.house = "Russians";
    tt.taskforce = 0;     // reuse TF1 (1 x type0)
    tt.script = 0;        // reuse SC1 (now bare disband)
    tt.priority = 5;
    tt.max = 1;
    tt.autocreate = false;                        // NON-autocreate: trigger-only
    cfg.team.aimd.teams.push_back(tt);
    ra2engine::AITriggerType tr;
    tr.id = "TRG1";
    tr.name = "player conyard seen";
    tr.team1 = 1;                                 // TT2
    tr.team2 = -1;
    tr.owner_house = "<all>";
    tr.tech_level = 0;
    tr.condition_type = ra2engine::kCondEnemyOwns;
    tr.condition_object =
        ra2engine::TriggerObject{ra2engine::TechnoKind::Building, 0};
    tr.comparator = ra2engine::TriggerComparator{1, ra2engine::kCompGreaterEq};
    tr.starting_weight = 50.0; tr.min_weight = 10.0; tr.max_weight = 90.0;
    tr.is_for_skirmish = true;
    tr.side = 0;
    tr.base_defense = false;
    tr.enabled[0] = tr.enabled[1] = tr.enabled[2] = true;
    tr.is_enabled = true;    // [AITriggerTypesEnable] default [RE: §5]
    cfg.team.aimd.triggers.push_back(tr);
    cfg.team.triggers_enabled = triggers_enabled;
    cfg.team.rules.total_team_cap = total_team_cap;
    return cfg;
}
}  // namespace

TEST_CASE("World: a trigger fires, creates its team via the trigger path, and the disband outcome adjusts its weight") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // AI base (house 1) + the PLAYER conyard (house 0) that makes the
    // enemy-owns condition TRUE, well outside auto-acquire reach.
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(0, c->x + 20, c->y + 2, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    // One IDLE house-1 combat unit so recruit-idle fills TT2 instantly.
    ra2engine::UnitCombat uc;
    uc.strength = 100;
    uc.image = "AITEST";
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(c->x + 3, c->y + 2, /*house=*/1, uc);
    world.enable_ai_house(1, ai3_cfg(/*triggers_enabled=*/true));
    REQUIRE(world.ai_trigger_state(1) != nullptr);
    REQUIRE(world.ai_trigger_state(1)->weights.size() == 1);
    CHECK(world.ai_trigger_state(1)->weights[0] == 50.0);   // starting_weight
    // Step until the created team disbands and reports its outcome: fire on
    // the first 1 s check pass -> recruit the idle unit -> gather -> execute
    // bare disband -> Disbanding -> weight adjusted.
    for (int i = 0; i < 4500 && world.ai_trigger_state(1)->adjustments < 1; ++i)
        world.step(1.0 / 15.0);
    const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
    REQUIRE(ts != nullptr);
    CHECK(ts->triggers_fired == 1);              // fired once, then Max=1-capped
    CHECK(ts->last_fired == 0);
    REQUIRE(world.ai_team_state(1) != nullptr);
    CHECK(world.ai_team_state(1)->teams_created == 1);   // the TRIGGER create —
                                                 // autocreate silenced; Team2
                                                 // is <none> -> ONE create
    REQUIRE(ts->adjustments == 1);
    CHECK(ts->last_adjusted == 0);               // Team1-identity scan found
                                                 // TRG1 (Team1 == TT2)
                                                 // [RE-GATE APPLIED:
                                                 // ai-trigger.md §6]
    CHECK(ts->last_success);                     // the bare-disband script ran
                                                 // to kOpDisband -> the team's
                                                 // completed_success flag; NOT
                                                 // a survivors rule [RE-GATE
                                                 // APPLIED: ai-trigger.md §4]
    CHECK(ts->weights[0] == 50.0 + ra2engine::kAiTriggerSuccessWeightDelta);
    CHECK(ts->last_old_weight == 50.0);          // first outcome: E=0 -> no
                                                 // track-record term -> 50+20
    CHECK(ts->last_new_weight ==
          50.0 + ra2engine::kAiTriggerSuccessWeightDelta);
    CHECK(ts->times_completed[0] == 1);          // C incremented on success
    CHECK(ts->times_executed[0] == 1);           // E incremented every outcome
}

TEST_CASE("World: trigger firewall - triggers_enabled=false means no trigger ever fires") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(0, c->x + 20, c->y + 2, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    world.enable_ai_house(1, ai3_cfg(/*triggers_enabled=*/false));
    for (int i = 0; i < 600; ++i) world.step(1.0 / 15.0);
    const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
    REQUIRE(ts != nullptr);
    CHECK(ts->triggers_fired == 0);              // firewall held
    CHECK(ts->adjustments == 0);
    CHECK(ts->weights[0] == 50.0);               // weight untouched
    CHECK(world.ai_team_state(1)->teams_created == 0);  // autocreate silenced,
                                                 // trigger path fenced off
}

TEST_CASE("World: a trigger create is skipped at TotalAITeamCap") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(0, c->x + 20, c->y + 2, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    world.enable_ai_house(1, ai3_cfg(/*triggers_enabled=*/true, /*total_team_cap=*/0));
    for (int i = 0; i < 600; ++i) world.step(1.0 / 15.0);
    const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
    REQUIRE(ts != nullptr);
    CHECK(ts->triggers_fired == 0);              // at cap -> create skipped
    CHECK(world.ai_team_state(1)->teams_created == 0);
}
```

- [ ] **Step 2: Declarations, build FOREGROUND, verify failure.** Add the world.hpp include + the three `AiTeamConfig` fields + `HouseAiRec::trigger_state` + the public/private declarations. Build the test target FOREGROUND, blocking (timeout 600000 ms). Expected: link FAIL (`ai_trigger_state`/`ai_fill_trigger_view` undefined).

- [ ] **Step 3: Implement.**

(a) The ONE additive AI-2 touch [RE-GATE APPLIED: ai-trigger.md §4 — success = the created team ran its ScriptType to the success/disband action (the original's `team+0x84` flag, set by script action `0x31`/49), NOT survivors-at-disband]. In `engine/ai_team.hpp`, `struct TeamInstance` gains ONE field at its tail:

```cpp
    bool completed_success = false;  // AI-3 [RE: ai-trigger.md §4]: set when
                                     // step_team executes kOpDisband (script
                                     // action 49 "achieved success") — the
                                     // original's team+0x84 flag; a team wiped
                                     // to zero members reaches Disbanding with
                                     // this still FALSE (-> failure)
```

In `engine/ai_team.cpp`, inside `step_team`'s EXISTING `kOpDisband` opcode handler (the branch that transitions the team to `TeamState::Disbanding` on script completion), add ONE line setting the flag on the handler's local `TeamInstance` reference:

```cpp
            t.completed_success = true;  // AI-3 success witness [RE: ai-trigger.md §4]
```

NO other ai_team change: the zero-member wipe path keeps the flag false, and every lifecycle transition is UNCHANGED. This is the spec's "zero AI-2 changes" deliberately relaxed by the RE-GATE to "one additive success flag" — the pre-hunt survivors-at-disband success rule it replaces was WRONG (ai-trigger.md §4). The flag is written on gate paths (`--demo-ai-team` scripts execute `kOpDisband`) but read ONLY by the AI-3 attribution scan behind `triggers_enabled`, so all seven gates stay byte-identical.

(b) In `engine/world.cpp`, `enable_ai_house` gains ONE line after `rec.team_state = AiTeamState{};  // AI-2: fresh team state per enable`:

```cpp
    rec.trigger_state = make_trigger_state(rec.cfg.team.aimd);  // AI-3: current
                                    // weights seeded from starting_weight
```

(NB it must read `rec.cfg` — `cfg` was already moved from.)

(c) Append after the AI-2 block (accessor + view fill):

```cpp
// --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---------------------------

const AiTriggerState* World::ai_trigger_state(int house) const {
    return ai_enabled(house) ? &ai_houses_[house].trigger_state : nullptr;
}

void World::ai_fill_trigger_view(int house, const HouseAiRec& rec,
                                 TriggerView& v) const {
    const AiTeamConfig& tc = rec.cfg.team;
    // Per-TechnoType counts, own vs enemy. "Enemy" aggregates EVERY other
    // house [RE-GATE APPLIED: ai-trigger.md §2/deviations — the original's
    // ConditionMet receives the house's ONE current enemy (house+0x5600);
    // our World aggregates all rival houses, the ai-team.md convention].
    // Unit typing maps through the house's OWN roster (ai_unit_type_of), so
    // enemy-UNIT conditions count only types this roster resolves; BUILDING
    // conditions index the full shared building roster directly (b.type), so
    // enemy-owns-building conditions see the player's base. No new state —
    // a per-pass snapshot [spec §4].
    v.own_units.assign(tc.unit_roster.size(), 0);
    v.enemy_units.assign(tc.unit_roster.size(), 0);
    v.own_buildings.assign(rec.cfg.roster.size(), 0);
    v.enemy_buildings.assign(rec.cfg.roster.size(), 0);
    // No neutral/civilian house exists in this engine, so the neutral slices
    // stay EMPTY and ct=7 (CivilianHouseOwns) evaluates FALSE [RE-GATE
    // APPLIED: ai-trigger.md §2 — implementable-if-neutral-house; see the
    // deviations list].
    v.neutral_units.clear();
    v.neutral_buildings.clear();
    for (const Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0) continue;
        const int t = ai_unit_type_of(rec, u);
        if (t < 0) continue;
        if (u.house == house) ++v.own_units[t];
        else ++v.enemy_units[t];
    }
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.type < 0 || b.type >= (int)rec.cfg.roster.size()) continue;
        if (b.house == house) ++v.own_buildings[b.type];
        else ++v.enemy_buildings[b.type];
    }
    v.credits = rec.credits;  // OWNER house credits — ct=4 HouseCredits reads
                              // the CALLING house's money scalar [RE-GATE
                              // APPLIED: ai-trigger.md §2 — NOT enemy credits]
    v.difficulty = (int)rec.cfg.difficulty;   // index 0 = HARDEST; the enable
                                              // inversion lives in
                                              // trigger_filters_pass [RE: §5]
    v.house_name = tc.house_name;
    v.side = tc.trigger_side;                 // 1=Allied/2=Soviet/3=Yuri [RE: §5]
    v.tech_level = tc.trigger_tech_level;
}
```

(d) In `step_ai_teams`, the TeamDelays check-pass block (step 4, world.cpp ~1290) changes to (the autocreate spawn keeps its EXACT existing code, completely untouched; the trigger step is appended INSIDE the same `if (st.check_clock >= check_period)` — [RE-GATE APPLIED: ai-trigger.md §5 — CONFIRMED cadence: there is NO separate AITrigger clock; the original evaluates triggers inside the per-house team-management pass `FUN_006f0010`, the same pass that enforces TotalAITeamCap, gated by an always-true-by-default probability `rand(1,100) <= house+0x565c` with `house+0x565c` seeded 100 — modeled as always-run inside AI-2's TeamDelays check pass; do NOT add a standalone trigger clock]):

```cpp
        st.check_clock += dt;
        const double check_period = (double)tc.rules.team_delay_frames / 15.0;
        if (st.check_clock >= check_period) {
            st.check_clock = 0.0;
            st.spawn_clock += check_period;
            const double spawn_period = tc.rules.autocreate_time_min * 60.0;  // min->s
            if (st.spawn_clock >= spawn_period) {
                st.spawn_clock = 0.0;
                // CONTRACT F: select_autocreate_team takes a trailing seeded rng
                // (the house's AiTeamState::rng — deterministic, never wall-clock).
                const int pick = select_autocreate_team(tc.aimd, tc.house_name,
                                                        st.teams, rec.credits,
                                                        tc.rules, st.rng);
                if (pick >= 0) {
                    TeamInstance t;
                    t.teamtype = pick;
                    st.teams.push_back(t);
                    ++st.teams_created;
                }
            }
            // AI-3 trigger-eval step [RE-GATE APPLIED: ai-trigger.md §5 —
            // CONFIRMED: trigger eval lives INSIDE this team-management
            // pass; no separate AITrigger clock exists]. A SECOND creation
            // path BESIDE autocreate: both feed st.teams and reuse the whole
            // AI-2 recruit/produce/gather/execute machinery (lifecycle
            // transitions UNCHANGED) [spec §4]. BOTH sources respect
            // TotalAITeamCap. NO team->trigger link is stored — attribution
            // at disband is a Team1-identity scan [RE-GATE APPLIED:
            // ai-trigger.md §6]. FIREWALL: triggers_enabled defaults false,
            // so no existing gate ever reaches this block; the draw consumes
            // trigger_state.rng (a separate stream), never st.rng.
            if (tc.triggers_enabled && !tc.aimd.triggers.empty() &&
                (int)st.teams.size() < tc.rules.total_team_cap) {
                TriggerView tv;
                ai_fill_trigger_view(h, rec, tv);
                const TriggerDecision d =
                    select_trigger(tc.aimd, rec.trigger_state.weights, st.teams,
                                   tv, rec.trigger_state.rng);
                if (d.teamtype1 >= 0) {
                    // BOTH Team1 AND Team2 are created for the OWNING house
                    // [RE-GATE APPLIED: ai-trigger.md §3 — CONFIRMED; Team2 is
                    // NOT a reciprocal and may equal Team1 -> two instances].
                    // Each create still respects TotalAITeamCap + the
                    // per-TeamType Max (select_trigger pre-checked both; the
                    // Team2 push re-checks against the just-pushed Team1).
                    TeamInstance t1;
                    t1.teamtype = d.teamtype1;  // AI-2's EXACT create path: the
                    st.teams.push_back(t1);     // team enters st.teams and
                    ++st.teams_created;         // lives the whole AI-2 lifecycle
                    if (d.teamtype2 >= 0 &&
                        (int)st.teams.size() < tc.rules.total_team_cap) {
                        const int max2 = tc.aimd.teams[d.teamtype2].max;
                        int cnt2 = 0;
                        for (const TeamInstance& ti : st.teams)
                            if (ti.teamtype == d.teamtype2) ++cnt2;
                        if (max2 < 0 || cnt2 < max2) {   // Max<0 = uncapped
                            TeamInstance t2;
                            t2.teamtype = d.teamtype2;
                            st.teams.push_back(t2);
                            ++st.teams_created;
                        }
                    }
                    ++rec.trigger_state.triggers_fired;
                    rec.trigger_state.last_fired = d.trigger;
                }
            }
        }
```

(e) In `step_ai_teams`'s team-stepping loop, the `if (t.state == TeamState::Disbanding)` branch gains the outcome attribution at its TOP (before the existing release-survivors code):

```cpp
            if (t.state == TeamState::Disbanding) {
                // AI-3 outcome attribution [RE-GATE APPLIED: ai-trigger.md
                // §4/§6 — CONFIRMED]: NO team->trigger back-pointer exists;
                // the original's TeamClass dtor scans the WHOLE trigger array
                // and feeds EVERY trigger whose Team1 == the disbanding
                // team's TeamType. So: a shared Team1 feeds ALL its triggers;
                // a team created as some trigger's Team2 gives NO feedback
                // (only Team1 matches); and autocreate-created teams sharing
                // a trigger's Team1 ALSO feed it — exactly the original.
                // Success = the team ran its script to kOpDisband
                // (completed_success, the team+0x84 flag), NOT survivors
                // [RE: §4]. Adjustment happens ONLY here, at disband.
                if (tc.triggers_enabled) {
                    for (int j = 0; j < (int)tc.aimd.triggers.size(); ++j)
                        if (tc.aimd.triggers[j].team1 == t.teamtype)
                            adjust_weight(rec.trigger_state, tc.aimd, j,
                                          t.completed_success);
                }
                // Release survivors: ... (existing code, UNCHANGED)
```

The new fields are NOT read by `world_hash` (main.cpp hashes only unit cell/hp/alive + building type/cell/hp/state) — the same discipline as AI-1/AI-2; nothing to change there, just do not touch `world_hash`.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="World*"
```

- [ ] **Step 5: Byte-identity — prove additive.** All SEVEN gates re-run FOREGROUND, blocking (the trigger step is fenced behind `triggers_enabled=false` everywhere; the AI-2 draws consume `st.rng`, untouched by the separate `trigger_state.rng` stream; the additive `completed_success` flag is WRITTEN on gate paths at `kOpDisband` but READ only inside the `triggers_enabled` fence, so no gate output can move):

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: `deterministic=yes logs=78`, `deterministic=yes logs=144`, `hash=5273b5e71ca009e1`, `econ_hash=7fed96cb98b68017`, `prod_hash=f0f4e1207ecd713c`, `ai_base_hash=cd1f7c6a29933f0a`, `ai_team_hash=312e92ed862a5e74`; full suite passes both modes (the new `World:` trigger cases skip clean without the install).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp engine/ai_team.hpp engine/ai_team.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "feat(engine): World AI-3 trigger-eval step in the TeamDelays pass - second create path beside autocreate creating Team1+Team2, Team1-identity disband attribution keyed on the additive completed_success flag, triggers_enabled firewall [RE: ai-trigger.md 3-6]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: main.cpp wiring — house-scoped [AITriggerTypes] parse + interactive enable

**Files:**
- Modify: `engine/main.cpp` (two sites: `make_ai_team_config` at ~line 1940, the interactive enemy-house config at ~line 2426)
- Test: none new — the wiring is verified by the seven gates + the full suite (Task 6's demo is the integration test).

**Interfaces:**
- Consumes: `parse_ai_triggers` (Task 2); `AiTeamConfig::{triggers_enabled,trigger_side,trigger_tech_level}` (Task 4); EXISTING main.cpp helpers: `roster_index(ba, id)`, the `unit_ix` lambda already inside `make_ai_team_config`, `load_soviet_unit_roster`, `make_ai_config`.
- Produces: `make_ai_team_config` gains a trailing `bool enable_triggers = false` parameter (defaulted — the two existing call sites `--demo-ai-team` (~line 4834) and the interactive path compile unchanged; only the interactive site opts in) and always fills `AimdData::triggers` house-scoped.

- [ ] **Step 1: Extend `make_ai_team_config`.** Change the signature (line ~1940):

```cpp
ra2engine::AiTeamConfig make_ai_team_config(const BuildingAssets& ba, const UnitRoster& ur_ai,
                                            const Rules& rules, const ra2assets::Ini& aimd_ini,
                                            ra2engine::AiDifficulty difficulty,
                                            int player_x, int player_y,
                                            bool enable_triggers = false) {
```

and insert immediately after the existing `tc.aimd = ra2engine::parse_aimd(aimd_ini, unit_ix);` line:

```cpp
    // AI-3 [RE: ai-trigger.md §1]: parse [AITriggerTypes] HOUSE-SCOPED through
    // the SAME resolver scoping as the teams above — UNIT condition objects
    // resolve in this house's (Soviet) unit roster, BUILDING objects in the
    // full shared building roster; an unresolvable object or a dropped/missing
    // team1 art-guard-drops the trigger whole, so only fieldable/resolvable
    // triggers survive. Parsed unconditionally (cheap, deterministic, cerr
    // only); the FIREWALL is triggers_enabled below.
    auto bld_ix = [&](const std::string& id) -> int {
        const int i = roster_index(ba, id);
        return (i >= 0 && i < (int)ba.art.size() && ba.art[i].ok) ? i : -1;
    };
    ra2engine::parse_ai_triggers(aimd_ini, tc.aimd, unit_ix, bld_ix);
    tc.triggers_enabled = enable_triggers;
    tc.trigger_side = 2;         // Soviet [RE-GATE APPLIED: ai-trigger.md §5 —
                                 // CONFIRMED SideIndex encoding 0=any,
                                 // 1=Allied, 2=Soviet, 3=Yuri]
    tc.trigger_tech_level = 10;  // skirmish tech ceiling [RE-FALLBACK: no
                                 // recovered per-house tech input; TechLevel
                                 // gates pass at 10]
    std::cerr << "[ai-trigger] aimd triggers parsed=" << tc.aimd.triggers.size()
              << " enabled=" << (enable_triggers ? "yes" : "no") << "\n";
```

- [ ] **Step 2: Enable triggers on the interactive enemy Soviet house.** At the interactive config site (~line 2426), change the call:

```cpp
            acfg.team = make_ai_team_config(ba, ur_ai, rules, aimd_ini,
                                            ra2engine::AiDifficulty::Normal,
                                            /*player base:*/ sc->x, sc->y + 4,
                                            /*enable_triggers=*/true);
```

The `--demo-ai-team` call site (~line 4834) is NOT touched — its defaulted `enable_triggers=false` keeps the trigger step fenced and `ai_team_hash` byte-identical (parsing triggers writes cerr only, never a witness line, and the trigger draw stream `trigger_state.rng` is never consumed when fenced).

- [ ] **Step 3: Build + all SEVEN gates + full suite FOREGROUND, blocking (timeout 600000 ms each):**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: all seven pins byte-identical (`deterministic=yes logs=78`, `deterministic=yes logs=144`, `hash=5273b5e71ca009e1`, `econ_hash=7fed96cb98b68017`, `prod_hash=f0f4e1207ecd713c`, `ai_base_hash=cd1f7c6a29933f0a`, and explicitly re-check `ai_team_hash=312e92ed862a5e74` — this task touches its config path); suite passes both modes; the interactive path is NOT launched (no unattended interactive run — Task 6's gate exercises the enabled path deterministically).

- [ ] **Step 4: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "feat(engine): parse real AITriggerTypes house-scoped in make_ai_team_config, enable triggers on the interactive enemy Soviet house [RE: ai-trigger.md 1/5]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: --demo-ai-trigger — the 8th permanent byte-identity gate

**Files:**
- Modify: `engine/main.cpp`
- Test: none new (the demo IS the test; a permanent gate).

**Interfaces:**
- Consumes: `simulate_ai_team`/`run_demo_ai_team` as the structural template (fixed 15 Hz dt, rendered-hidden Run 1 + headless Run 2 from a FRESH World, byte-compare); `fnv_mix`; `spawn_base`; `make_ai_config` + `make_ai_team_config(..., enable_triggers=true)` (Task 5) + `load_soviet_unit_roster`; `World::ai_trigger_state` (Task 4) + `ai_team_state`/`ai_credits`/`add_building`; `AITriggerType`/`TriggerObject`/`TriggerComparator`/`TechnoKind` + `kCondEnemyOwns`/`kComp*` (Tasks 2–3); `wire_economy`, `spawn_free_units_on_open`, `load_miner_assets`, `load_ini_from_ra2md`, `pick_start_cell`.
- Produces: `uint64_t ai_trigger_hash(...)`, `DemoAiTriggerResult`, `simulate_ai_trigger(...)`, `int run_demo_ai_trigger()`, a `--demo-ai-trigger` arg + dispatch in `main()` (beside `--demo-ai-team`).

**Script (spec §5):** the fixed demo map; the ai-team demo's base layout (full Soviet house-1 base + house-0 player base); house 1 AI-flagged at FIXED difficulty/credits with teams AND triggers enabled from the REAL parsed aimd.ini; the demo then hands the PLAYER a building count satisfying a stock EnemyHouseOwns-N-of-X trigger condition (chosen by a DETERMINISTIC runtime scan — no hardcoded trigger id) so the TRIGGER path is exercised and PROVEN BY THE WITNESS CHAIN (condition met → trigger selected w/ weight → team(s) created → outcome → weight adjusted) — there is NO autocreate-flag distinction to lean on: ALL 163 stock TeamTypes are `Autocreate=yes` and the trigger pool IS the autocreate pool [RE-GATE APPLIED: ai-trigger.md §6 — this replaces the pre-hunt "non-autocreate team" proof target]; fixed `kAiTriggerFrames = 13500` frames (15 min at 15 Hz — ~5 Normal TeamDelays check passes + team lifecycle + disband margin for the outcome; a DEMO frame budget, NOT an engine cadence — trigger eval runs inside the AI-2 team pass [ai-trigger.md §5]). Two runs, byte-compared; **NO hardcoded hash literal** (compute-and-self-compare, the `--demo-ai-team` precedent).

- [ ] **Step 1: ai_trigger_hash** (near `ai_team_hash`; complete):

```cpp
// FNV hash of the AI-3 end state for --demo-ai-trigger's two-run byte-identity
// gate [RE: ai-trigger.md; spec §5]: per-trigger CURRENT weights
// (trigger-table order, x1000 fixed-point) + triggers_fired/adjustments + the
// created-team list {teamtype, state, member count} (creation/live order) +
// credits. A NEW function; world_hash/econ_hash/prod_hash/ai_base_hash/
// ai_team_hash UNTOUCHED.
uint64_t ai_trigger_hash(const ra2engine::World& world, int house) {
    uint64_t h = 1469598103934665603ULL;  // FNV offset basis
    if (const ra2engine::AiTriggerState* ts = world.ai_trigger_state(house)) {
        for (double w : ts->weights)
            h = fnv_mix(h, static_cast<uint64_t>(std::llround(w * 1000.0)));
        h = fnv_mix(h, static_cast<uint64_t>(ts->triggers_fired));
        h = fnv_mix(h, static_cast<uint64_t>(ts->adjustments));
    }
    if (const ra2engine::AiTeamState* st = world.ai_team_state(house)) {
        h = fnv_mix(h, static_cast<uint64_t>(st->teams_created));
        for (const ra2engine::TeamInstance& t : st->teams) {
            h = fnv_mix(h, static_cast<uint64_t>(t.teamtype));
            h = fnv_mix(h, static_cast<uint64_t>((int)t.state));
            h = fnv_mix(h, static_cast<uint64_t>(t.members.size()));
        }
    }
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(world.ai_credits(house))));
    return h;
}
```

- [ ] **Step 2: simulate_ai_trigger** (complete; mirrors `simulate_ai_team`'s shape — same parameter list, fixed 15 Hz dt, rendered + headless byte-identical, the FIX-3 no-extra-step end-capture rule):

```cpp
struct DemoAiTriggerResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the AI-trigger demo. Spawns the ai-team demo's base
// layout, enables teams + TRIGGERS on house 1 from the REAL parsed aimd.ini
// (the same make_ai_team_config path the interactive session uses), hands the
// PLAYER a building count satisfying a stock EnemyHouseOwns trigger condition
// (deterministic runtime exemplar scan — NO hardcoded trigger id), and lets
// the trigger engine fire/create/attribute over kAiTriggerFrames. Witnesses:
// condition met -> trigger selected w/ weight -> team(s) created (Team1 +
// Team2 when set) -> outcome -> weight adjusted old->new. Attribution is by
// Team1 identity [RE: ai-trigger.md §6], so autocreate-created teams sharing
// a trigger's Team1 also feed the weights — exactly the original's disband
// scan; every such outcome is witnessed deterministically.
// ai_trigger_hash pins the end state.
DemoAiTriggerResult simulate_ai_trigger(Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                        MinerAssets& miners, const Rules& rules,
                                        const std::vector<const VehicleAssets*>& vlist,
                                        const OreArt& ore_art, const ra2assets::Ini& aimd_ini,
                                        const UnitRoster& ur_ai, bool render, SDL_Renderer* ren,
                                        ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                        int W, int H, const std::string& start_png,
                                        const std::string& end_png) {
    DemoAiTriggerResult r;
    ra2engine::World world = build_world(a, ua, 0);
    wire_economy(world, ba, rules);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 4);
    spawn_base(world, ba, /*house=*/1, pc->x + 14, pc->y, /*with_refinery=*/true,
               miners.harv.ok ? &miners.harv : nullptr, /*harv_chrono=*/false,
               miners.harv_storage);
    ra2engine::AiHouseConfig acfg =
        make_ai_config(ba, rules, ra2engine::AiDifficulty::Normal,
                       (double)rules.mp_money());
    acfg.team = make_ai_team_config(ba, ur_ai, rules, aimd_ini,
                                    ra2engine::AiDifficulty::Normal,
                                    pc->x, pc->y + 4, /*enable_triggers=*/true);
    if (acfg.team.aimd.triggers.empty()) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO triggers parsed from aimd.ini");
        return r;
    }

    // Exemplar: the FIRST parsed trigger that (a) keys EnemyHouseOwns (ct=0)
    // over a BUILDING object, (b) passes the Normal/skirmish/side/owner/
    // enable filters this house runs under, (c) fields a finishable squad
    // (taskforce total <= 6). NO autocreate filter — ALL 163 stock TeamTypes
    // are Autocreate=yes; the TRIGGER path is proven by the witness chain,
    // not an autocreate-flag distinction [RE-GATE APPLIED: ai-trigger.md §6
    // — CONFIRMED, replaces the pre-hunt non-autocreate exemplar filter].
    // Deterministic runtime scan — NO hardcoded trigger id. Recovered stock
    // Soviet candidates this scan lands on (the player seeding below feeds
    // an enemy-owns-building condition, e.g. GACNST >= 1):
    //   `Soviet Engineer vs Ally med [0D53430C-G]` (ct=0 GACNST>=1),
    //   `Soviet Ivan vs Allies [0C87B45C-G]` (ct=0 GACNST>=1);
    // unit-object variant: `Soviet Terror D vs Allies [0A452E5C-G]`
    //   (ct=0 MTNK>=8);
    // owner-owns: `Soviet Apoc Medium [0D2C45DC-G]` (ct=1 NATECH>=1),
    //   `Soviet Miners [0C8C2C3C-G]` (ct=1 HARV<=3);
    // adaptive-weight showcase (weight 70/40/70, ct=4 always-true):
    //   `Soviet Armor Hard [0C8C56FC-G]` — ideal for demonstrating weight
    //   movement across create->disband [ai-trigger.md §6/DIVERGENCES].
    int exemplar = -1;
    for (int i = 0; i < (int)acfg.team.aimd.triggers.size(); ++i) {
        const ra2engine::AITriggerType& t = acfg.team.aimd.triggers[i];
        if (t.condition_type != ra2engine::kCondEnemyOwns) continue;
        if (t.condition_object.kind != ra2engine::TechnoKind::Building) continue;
        if (!t.is_for_skirmish || !t.is_enabled || t.base_defense) continue;
        if (!t.enabled[1]) continue;   // Normal house (difficulty index 1)
                                       // consults enabled[2-1]=enabled[1] —
                                       // the inversion-neutral middle
                                       // [RE: ai-trigger.md §5]
        if (t.side != 0 && t.side != 2) continue;   // Soviet house
        if (!(t.owner_house == "<all>" || t.owner_house == "Russians"))
            continue;                  // "<none>"/"" would be filter-rejected
        const ra2engine::TeamType& t1 = acfg.team.aimd.teams[t.team1];
        int total = 0;
        for (const ra2engine::TaskForceMember& m :
             acfg.team.aimd.taskforces[t1.taskforce].members)
            total += m.count;
        if (total > 6) continue;                  // finishable within the run
        exemplar = i;
        break;
    }
    if (exemplar < 0) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO suitable stock enemy-owns trigger");
        return r;
    }
    const ra2engine::AITriggerType& ex = acfg.team.aimd.triggers[exemplar];
    // Hand the PLAYER a count satisfying the exemplar's comparator so its
    // condition is TRUE from the first check pass.
    int need = 0;
    if (ex.comparator.op == ra2engine::kCompGreaterEq ||
        ex.comparator.op == ra2engine::kCompEq)
        need = ex.comparator.value;
    else if (ex.comparator.op == ra2engine::kCompGreater)
        need = ex.comparator.value + 1;
    else if (ex.comparator.op == ra2engine::kCompNotEq && ex.comparator.value == 0)
        need = 1;   // kCompLess/kCompLessEq: 0 owned already satisfies
    const int bt = ex.condition_object.index;
    const ra2engine::BuildingTypeDef& bd = ba.roster[bt];
    for (int k = 0; k < need; ++k)
        world.add_building(bt, pc->x + 1 + k * (bd.fw + 1), pc->y + 9,
                           bd.fw, bd.fh, /*house=*/0, std::max(1, bd.strength),
                           bd.armor_ix, 0.0, /*instant=*/true);
    world.enable_ai_house(1, acfg);
    {
        char b[240];
        std::snprintf(b, sizeof b,
                      "[ai-trigger] exemplar=%s object=%s need=%d start_weight=%.1f",
                      ex.id.c_str(), bd.id.c_str(), need, ex.starting_weight);
        r.witness.emplace_back(b);
    }

    std::vector<int> bstates;  // FreeUnit grand-opening tracker
    const double dt = 1.0 / 15.0;
    const long kAiTriggerFrames = 13500;  // DEMO frame budget (NOT an engine
                                          // cadence — trigger eval runs inside
                                          // the AI-2 team pass, no standalone
                                          // clock [RE: ai-trigger.md §5]):
                                          // 15 min at 15 Hz: ~5 Normal
                                          // TeamDelays passes (2500 frames) +
                                          // recruit/produce/gather/march +
                                          // disband margin for the outcome
    long frame = 0;
    int last_fired_seen = 0;
    int last_adjust_seen = 0;

    auto step_one = [&]() {
        world.step(dt);
        spawn_free_units_on_open(world, ba, rules, miners, bstates);
        const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
        if (ts) {
            if (ts->triggers_fired > last_fired_seen && ts->last_fired >= 0) {
                const ra2engine::AITriggerType& t =
                    acfg.team.aimd.triggers[ts->last_fired];
                const ra2engine::TeamType& t1 = acfg.team.aimd.teams[t.team1];
                char b[240];
                std::snprintf(b, sizeof b,
                              "[ai-trigger] condition met -> trigger=%s selected weight=%.1f frame=%ld",
                              t.id.c_str(), ts->weights[ts->last_fired], frame);
                r.witness.emplace_back(b);
                // BOTH Team1 and Team2 are created for the owner [RE-GATE
                // APPLIED: ai-trigger.md §3]; the second create may be
                // cap-skipped, so the FINAL teams_created line is the count
                // of record.
                std::snprintf(b, sizeof b,
                              "[ai-trigger] team(s) created team1=%s team2=%s frame=%ld",
                              t1.id.c_str(),
                              t.team2 >= 0
                                  ? acfg.team.aimd.teams[t.team2].id.c_str()
                                  : "<none>",
                              frame);
                r.witness.emplace_back(b);
                last_fired_seen = ts->triggers_fired;
            }
            if (ts->adjustments > last_adjust_seen && ts->last_adjusted >= 0) {
                char b[240];
                std::snprintf(b, sizeof b,
                              "[ai-trigger] outcome=%s trigger=%s weight %.1f -> %.1f frame=%ld",
                              ts->last_success ? "success" : "failure",
                              acfg.team.aimd.triggers[ts->last_adjusted].id.c_str(),
                              ts->last_old_weight, ts->last_new_weight, frame);
                r.witness.emplace_back(b);
                last_adjust_seen = ts->adjustments;
            }
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
        }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < kAiTriggerFrames) step_one();

    {
        const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
        const ra2engine::AiTeamState* st = world.ai_team_state(1);
        char b[240];
        std::snprintf(b, sizeof b,
                      "[ai-trigger] final fired=%d adjustments=%d teams_created=%d live=%d credits=%d",
                      ts ? ts->triggers_fired : -1, ts ? ts->adjustments : -1,
                      st ? st->teams_created : -1, st ? (int)st->teams.size() : -1,
                      (int)std::llround(world.ai_credits(1)));
        r.witness.emplace_back(b);
    }
    r.hash = ai_trigger_hash(world, 1);
    {
        char b[80];
        std::snprintf(b, sizeof b, "[ai-trigger] ai_trigger_hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(b);
    }
    // The witness chain the spec demands (spec §5, as amended by
    // ai-trigger.md §6: the TRIGGER path is proven by fire -> create ->
    // outcome -> adjust, NOT by an autocreate-flag distinction). Wedge = NOT ok.
    if (last_fired_seen < 1) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO trigger fired");
    }
    if (last_adjust_seen < 1) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO outcome/weight adjustment -- trigger loop wedged");
    }
    // Capture the ALREADY-rendered final frame WITHOUT stepping/witnessing
    // again (the --demo-ai-team FIX-3 rule: an extra rendered-only step could
    // append a witness line and false-RED the byte-compare).
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}
```

- [ ] **Step 3: run_demo_ai_trigger.** Mirror `run_demo_ai_team` exactly (same asset loads including `MinerAssets` + the render vlist with `ur_ai.vehicles`; same NACNST guard; aimd.ini load is a HARD failure for this gate; camera math identical), with the names swapped: scratchpad PNGs `demo_ai_trigger_start.png`/`demo_ai_trigger_end.png`; Run 1 rendered (hidden window), Run 2 headless from a FRESH World via `simulate_ai_trigger`; byte-compare `r1.witness == r2.witness && r1.hash == r2.hash`; print every witness line, the PNG paths, and the final line via:

```cpp
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d ai_trigger_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
```

where `const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);` and `const bool bad = !identical || !r1.ok || !r2.ok;` — exit 0 iff identical AND both `ok`.

- [ ] **Step 4: Dispatch.** `bool demo_ai_trigger = false;` + `else if (arg == "--demo-ai-trigger") { demo_ai_trigger = true; }` in the arg parse + `if (demo_ai_trigger) return run_demo_ai_trigger();` beside the `--demo-ai-team` dispatch.

- [ ] **Step 5: FOREGROUND — build + run ALL EIGHT demos, blocking (timeout 600000 ms each), assert every pin:**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
```

Expected: the seven legacy pins byte-identical; `--demo-ai-trigger` prints `deterministic=yes`, the witness chain reads exemplar → condition met/trigger selected (with weight) → team(s) created (`team1=`/`team2=` ids) → outcome → weight adjusted old→new, and the SAME `ai_trigger_hash` on both invocations (two-run + two-invocation stability). Record the fresh `ai_trigger_hash` value in the task report — do NOT write it into code; the pin is a controller step (Step 7). **HARD RULES for the implementer:** (a) run the demo FOREGROUND, synchronously — never backgrounded, never watched via a monitor; (b) do NOT pin a hash literal in code or tests; (c) if the demo exits nonzero, prints `deterministic=no`, or the witness chain never reaches the weight adjustment (trigger loop wedged — no fire, no create, or no outcome inside `kAiTriggerFrames`), **STOP and escalate to the controller — do NOT commit a red or wedged gate** (the AI-2 Task-7 precedent). A wedge is a design-level finding (wrong exemplar filter, too-short frame budget, or a real brain defect), not something to patch around silently.

- [ ] **Step 6: Full suite FOREGROUND both modes, blocking.** Expected: all pass.
- [ ] **Step 7 (CONTROLLER, not the implementer): pin + screenshot eyeball.** The controller (a) eyeballs `demo_ai_trigger_start.png`/`demo_ai_trigger_end.png` — the seeded player buildings visible at start, a Soviet squad formed/advanced by the end frame, no render garbage; (b) takes the reported `ai_trigger_hash` from Step 5 and pins it in the Task 7 README gate row (and in the Global Constraints of any FUTURE plan). If the witness order contradicts the spec chain (condition met before selected before created before outcome before adjusted), STOP — that is a brain defect, not a pin.
- [ ] **Step 8: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "feat(engine): --demo-ai-trigger deterministic gate + ai_trigger_hash (8th permanent gate) [RE: ai-trigger.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: Finalize — full suite both modes, seven legacy gates, arm64 build, README

**Files:**
- Modify: `README.md`; `android/app/jni/CMakeLists.txt` ONLY if it lists engine sources beyond `cmake/ra2_sources.cmake` (verify — `ai_trigger.cpp` should flow in via the shared list).

- [ ] **Step 1: Full test suite, both modes, FOREGROUND, blocking (timeout 600000 ms):**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: 0 failed in both; install-gated cases (real-aimd trigger smoke, the `World:` trigger cases) skip clean when the dir is absent.

- [ ] **Step 2: ALL EIGHT demos FOREGROUND, blocking — the seven legacy pins byte-identical + the new pin stable:**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
```

Expected, explicitly all seven legacy: `--demo-combat` `deterministic=yes logs=78`; `--demo-tank` `deterministic=yes logs=144`; `--demo-base` `hash=5273b5e71ca009e1`; `--demo-econ` `econ_hash=7fed96cb98b68017`; `--demo-prod` `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` `ai_team_hash=312e92ed862a5e74`; plus `--demo-ai-trigger` `deterministic=yes` with the ai_trigger_hash equal to Task 6's controller-pinned value.

- [ ] **Step 3: arm64 NDK build FOREGROUND, blocking (timeout 600000 ms):**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Then nm-verify the new ai_trigger symbols in the arm64 `libmain.so`:

```powershell
& "D:\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe" `
  (Get-ChildItem -Recurse -Filter libmain.so D:\ra2-android\android\app\build | Where-Object { $_.FullName -match "arm64" } | Select-Object -First 1).FullName `
  | Select-String -Pattern "parse_ai_triggers|select_trigger|adjust_weight|trigger_eligible|ai_fill_trigger_view"
```

Expected: at least one mangled/demangled match per symbol (nonempty output for all five).

- [ ] **Step 4: README — AI-3 section.** Add an "AI-3 — AITriggerTypes Weighted Triggers" section after the AI-2 section documenting: the recovered AITriggerType flat-line data model parsed from the REAL `[AITriggerTypes]` + `[AITriggerTypesEnable]` (`engine/aimd_parse` extension: one full trigger per `<id>=Name,field,...` key in the CONFIRMED 18-field order, condition_object resolved to a {unit|building, roster index} pair, value-first `Conditions[4]` comparator-blob decode, art-guard-drop of unresolvable objects / missing team1, per-trigger `is_enabled`); the pure `engine/ai_trigger` brain (the FULL recovered ConditionType −1..7 dispatch with inert values → FALSE, the recovered enable-filter matrix incl. the INVERTED per-difficulty enable, the cumulative-sum draw over `round(current weight)` with the 5000 mandatory tier from the seeded separate-stream xorshift32, and the additive +20/−50 track-record weight rule clamped [min,max] at disband); the World trigger-eval step inside AI-2's TeamDelays check pass — a SECOND creation path beside autocreate creating BOTH Team1 and Team2 into the same team list, with ONE additive AI-2 change (`TeamInstance::completed_success`, set at `kOpDisband` — success = script completion) and disband outcomes attributed by Team1-identity scan (no back-pointer; a shared Team1 feeds all its triggers; Team2-created teams feed nothing); the `triggers_enabled` firewall keeping the seven legacy gates bit-identical; and that the interactive enemy Soviet house now runs all three AI layers (base + autocreate teams + adaptive triggers). Add the gate row:

```
| --demo-ai-trigger | ai_trigger_hash=<pinned value from Task 6 Step 7> | deterministic=yes; the player is seeded with a building count satisfying a stock EnemyHouseOwns trigger condition; the Soviet AI house fires the trigger by the recovered cumulative-sum draw over round(current weight), creates its Team1 (and Team2 when set) through the AI-2 path, and the created teams' disband outcomes adjust every trigger sharing that Team1 by the +20/-50 track-record rule, clamped (ai-trigger.md) |
```

Add to the "Deviations we take" list (so review does not "fix" them), each cited to ai-trigger.md's deviations section [RE-GATE APPLIED — this list mirrors ai-trigger.md's "Deviations we take"]:
  - **Engine xorshift32 in a SEPARATE, dedicated `AiTriggerState::rng` stream** for the weighted draw vs the original's lockstep game RNG (`FUN_0065c220(1,Σw)`) — same distribution, not bit-identical to the netgame sequence; enabling triggers never perturbs the AI-2 draw sequence (protects `ai_team_hash`).
  - **ct=4 HouseCredits implemented as a REAL `credits OP N` compare** — every stock use is `N=0, op≥` (effectively always-true), but our economy exposes the owner house credits, so a mod-supplied non-zero N works [ai-trigger.md §2/deviations].
  - **Inert conditions**: ct=5 (IronCurtainCharged) and ct=6 (ChronoSphereCharged) evaluate FALSE until a superweapon subsystem exists; ct=2/ct=3 are recovered-but-unused inline house-metrics left unimplemented (FALSE); ct=7 (CivilianHouseOwns) evaluates FALSE unless a neutral/civilian house exists (none in this engine) [ai-trigger.md §2/deviations].
  - **Enemy aggregation**: the original's ConditionMet receives the house's ONE current enemy (`house+0x5600`); our World aggregates ALL rival houses for the enemy-owns count (the ai-team.md convention) [ai-trigger.md deviations].
  - **One additive AI-2 flag**: the spec's "zero AI-2 changes" is relaxed to `TeamInstance::completed_success` (set at `kOpDisband`) — required because the recovered success definition is script-completion (`team+0x84`), not survivors-at-disband [ai-trigger.md §4].
  - **Trigger cadence CONFIRMED shared**: no separate AITrigger clock exists — eval runs inside AI-2's team-management (TeamDelays) pass; the original's per-house probability gate (`house+0x565c`=100) is always-true by default and modeled as always-run [ai-trigger.md §5].
  - **Base-defense split simplified [RE-FALLBACK]**: base-defense triggers are rejected (offensive scope; ALL stock triggers are IsForBaseDefense=0) because the recovered defensive-need inputs (`Rules+0x1394[diff]`, `house+0x566c`) are not tracked [ai-trigger.md §5].
  - **Team-buildable predicate deferred [RE-FALLBACK]**: the original's `FUN_0041ff00` buildable check is deferred to AI-2's recruit/produce machinery — an unfillable team idles and disbands as a failure [ai-trigger.md §5/§6].
  - **Unit-object conditions resolve within the AI house's own roster scope** (enemy-owns-UNIT triggers naming types outside it art-guard-drop); building objects use the full shared building roster.
  - **Skirmish-only engine**: triggers with IsForSkirmish=0 are never eligible (all stock are 1); the campaign enable path (non-inverted) is not implemented — plus every remaining `[RE-FALLBACK]` recorded in ai-trigger.md, cited by §.
- [ ] **Step 5: Commit.**

```powershell
git -C "D:\ra2-android" add README.md
git -C "D:\ra2-android" commit -m "docs: AI-3 AITriggerTypes README + --demo-ai-trigger gate pin

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review (performed at write time)

1. **Spec coverage:**
   - Spec §1 (Ghidra hunt, six items) → Task 1 questions 1–6 verbatim, each landing CONFIRMED/PROBABLE/[RE-FALLBACK] with a located-functions table + confidence table + deviations section; the REAL aimd.ini extracted via ra2extract with ≥10 verbatim stock trigger lines recorded in scratch (Task 2 fixture shapes + Task 6 exemplar candidates); content-anchored headless sweep on `"AITriggerTypes"`/`"AITriggerTypesEnable"`/field strings + the condition/weight consult sites xref-walked from the AI-2 located functions (`FUN_004fd320`/`FUN_0063f940`/`FUN_0066cf70`/`FUN_00672520`/`FUN_006f0ab0`); Step 7 is the controller RE-GATE with the explicit confirm/replace list (field layout, full ConditionType enum + comparator + inert split, selection draw + team1/team2, weight rule + success definition, filters + cadence, composition/caps/attribution, exemplar filter) committed BEFORE Task 2 dispatch.
   - Spec §2 (task spine + data model) → the 7-task spine mirrors §2's list exactly (hunt → parser → pure engine → World → main.cpp → gate → finalize); the decomposition tripwire holds (7 ≤ ~9). The approved AITriggerType shape (name, team1/team2 resolved to `AimdData::teams` indices with team1-required drop, owner_house with `<all>`, tech_level, condition_type, condition_object as a {kind, roster index} pair through TWO caller resolvers with art-guard-drop, comparator, starting/min/max weights, enabled[3] + the [AITriggerTypesEnable] is_enabled, is_for_skirmish, side, base_defense) lands in Task 2 with `[RE-GATE APPLIED]` markers; the flat-line format is parsed via the UNCHANGED `Ini::section()` (no Ini-layer changes); `AiTriggerState` (current weights initialized from starting_weight with the parsed table immutable, C/E track-record counters, seeded rng — NO attribution links, per the recovered Team1-identity scan) lands in Task 3; the SECOND-path composition with only the ONE additive AI-2 flag (`completed_success`) lands in Task 4.
   - Spec §3 (pure engine) → Task 3: `engine/ai_trigger.{hpp,cpp}`, TriggerView in / TriggerDecision out; the FULL recovered −1..7 enum dispatch with inert→FALSE (ct=4 HouseCredits a real compare, ct=7 neutral-house counts); the recovered enable-filter matrix (base-defense reject × is_enabled × is_for_skirmish × INVERTED enabled[2-diff] × owner_house × side × tech_level); the cumulative-sum draw over `round(CURRENT weight)` + the 5000 mandatory tier from the seeded `ai_rand` (never wall-clock) emitting BOTH team indices (Team1+Team2) + the trigger id, none-eligible → nothing; the additive +20/−50 track-record `adjust_weight` (C/E counters in state) clamped at BOTH bounds, all as named RE-GATE constants; per-condition doctests on hand-built views, the filter matrix incl. the inversion, selection determinism (same seed → identical picks) + pick-convergence (sole-eligible probability 1 across a seed sweep; 90/10 dominant-weight frequency bound; 5000-tier exclusivity), the weight-rule arithmetic incl. pinned start==min==max. Registered in BOTH cmake files.
   - Spec §4 (World integration) → Task 4: trigger-eval step INSIDE the existing TeamDelays check pass beside `select_autocreate_team` (no new tick loop/firewall — CONFIRMED cadence, no standalone AITrigger clock [ai-trigger.md §5]); TriggerView filled from `buildings_`/`units_` as a per-pass snapshot (no new state); the creates go through AI-2's EXACT path (TeamInstances pushed to `st.teams` + `teams_created` for Team1 AND Team2, no lifecycle changes beyond the ONE additive `TeamInstance::completed_success` flag [ai-trigger.md §4]); attribution is the recovered Team1-identity scan at disband (NO back-pointer; a shared Team1 feeds all its triggers, Team2-created teams feed nothing [ai-trigger.md §6]) keyed on `completed_success` (success = script completion, NOT survivors); BOTH sources respect TotalAITeamCap (autocreate inside `select_autocreate_team`, the trigger create at its call-site gate + the Team1/Team2 Max checks inside `select_trigger` + the Team2-push re-check); fixed deterministic ordering preserved (base tick → team lifecycle/autocreate → trigger eval); new fields excluded from `world_hash`; the trigger step early-returns via `triggers_enabled=false` (per-house, default false) so the seven gates stay bit-identical — proven by the Task 4 firewall test + all-gates re-run. World tests drive the loop end-to-end: condition fires → team created via the trigger path (autocreate silenced as test isolation) → the script-completion outcome adjusts the weight by +20 with C/E counters; plus the cap-skip case (spec §5's "trigger create skipped at TotalAITeamCap").
   - Spec §5 (gate + testing) → Task 6: `--demo-ai-trigger` seeds the player with a count satisfying a stock EnemyHouseOwns condition (deterministic runtime exemplar scan over the real recovered Soviet exemplars, no hardcoded id) so the TRIGGER path is proven by the witness chain — all 163 stock TeamTypes are Autocreate=yes, so there is no autocreate-flag proof target [ai-trigger.md §6]; fixed frames; `ai_trigger_hash` = FNV over per-trigger current weights (trigger-table order) + fired/adjustment counts + the created-team list {teamtype, state, members} + credits; two-run byte-compare, compute-and-self-compare, NO pinned literal; the full witness chain (condition met → trigger selected w/ weight → team(s) created team1/team2 → outcome → weight adjusted old→new); implementer STOP-on-wedge hard rule + controller pin/eyeball step. Task 7: full suite both modes, the SEVEN legacy gate values re-asserted explicitly, arm64 + nm-verify of the new ai_trigger symbols, README AI-3 section + 8th gate row + deviations. Tests skip cleanly without `RA2_INSTALL_DIR` (Task 2/3 fixtures inline host-only; Task 4 world cases behind `load_real_map_w`).
   - Spec §Interactive payoff → Task 5 wires the house-scoped parse into `make_ai_team_config` (the same resolver scoping) and enables triggers on the interactive enemy Soviet house, so it runs all three AI layers.
2. **Placeholder scan:** no TODO/TBD/"similar to Task N" prose in the operative steps; every step shows real code or exact commands + expected output. Two deliberate references to in-tree code remain and are legitimate per the AI-1/AI-2 precedent: Task 6 Step 3 mirrors `run_demo_ai_team` (in-tree at a named location) with every DIFFERENCE shown as real code, and Task 2's install-gated additions append INSIDE an existing named TEST_CASE. All numerics are NAMED constants tagged `[RE-GATE APPLIED: ai-trigger.md §N]` or `[RE-FALLBACK]` with tests asserting the names — pre-hunt values reconciled at the controller RE-GATE. The ai_trigger_hash pin is extract-then-pin by the controller, deliberately never a literal in code.
3. **Type consistency (checked across tasks):**
   - `parse_ai_triggers(const ra2assets::Ini&, AimdData&, const std::function<int(const std::string&)>&, const std::function<int(const std::string&)>&) -> void` and `decode_comparator(const std::string&, TriggerComparator&) -> bool` — defined Task 2, consumed Task 2 tests + Task 5 (`make_ai_team_config`) with exactly these argument lists; Task 3/4/6 consume the Task 2 structs (`AITriggerType` field names `id/name/team1/team2/owner_house/tech_level/condition_type/condition_object/comparator/starting_weight/min_weight/max_weight/is_for_skirmish/side/base_defense/enabled[3]/is_enabled`, `TriggerObject{kind,index}`, `TriggerComparator{value,op}`, `TechnoKind::{None,Unit,Building}`, `AimdData::triggers`) verbatim.
   - `make_trigger_state(const AimdData&) -> AiTriggerState`, `compare_count(int, const TriggerComparator&) -> bool`, `trigger_condition_true/trigger_filters_pass/trigger_eligible(const AITriggerType&, const TriggerView&) -> bool`, `select_trigger(const AimdData&, const std::vector<double>&, const std::vector<TeamInstance>&, const TriggerView&, std::uint32_t&) -> TriggerDecision` (returning `{trigger, teamtype1, teamtype2}`), `adjust_weight(AiTriggerState&, const AimdData&, int, bool) -> void` — declared+defined Task 3, consumed Task 4 (`enable_ai_house`/`step_ai_teams`) and the Task 3/4 tests with exactly these argument lists; `AiTriggerState` field names (`weights/times_completed/times_executed/rng/triggers_fired/last_fired/adjustments/last_adjusted/last_success/last_old_weight/last_new_weight`) used identically in Task 4's World code, Task 4's tests, and Task 6's witnesses/hash; `TeamInstance::completed_success` (the Task 4 additive AI-2 flag) written in `step_team`'s `kOpDisband` handler and read ONLY by Task 4's disband attribution scan.
   - `AiTeamConfig::{triggers_enabled,trigger_side,trigger_tech_level}` + `HouseAiRec::trigger_state` + `World::ai_trigger_state(int) -> const AiTriggerState*` + private `ai_fill_trigger_view(int, const HouseAiRec&, TriggerView&) const` — defined Task 4, consumed by Task 4 tests, Task 5 (config fill), Task 6 (hash/witnesses).
   - `make_ai_team_config(const BuildingAssets&, const UnitRoster&, const Rules&, const ra2assets::Ini&, ra2engine::AiDifficulty, int, int, bool enable_triggers = false)` — extended Task 5, consumed Task 6 with `enable_triggers=true`; the two pre-existing call sites compile unchanged via the default.
   - Existing signatures used verbatim from the current tree: `ai_rand(std::uint32_t&)` + `AiDifficulty` (ai_base.hpp), `TeamInstance/AiTeamState/TeamType/TaskForce/TaskForceMember/ScriptType/ScriptAction/kOpDisband` (ai_team.hpp/aimd_parse.hpp), `select_autocreate_team(..., std::uint32_t&)` (the AI-2 CONTRACT-F form at the world.cpp call site), `World::add_building(type, cx, cy, fw, fh, house, strength, armor_ix, buildup_total, instant)`, `enable_ai_house(int, AiHouseConfig)`, `ai_enabled/ai_credits/ai_team_state`, `BuildingTypeDef::{fw,fh,strength,armor_ix,id}`, `Ini::section()`, `roster_index(ba, id)`, `fnv_mix`, the `ai2_cfg(bool, bool=false)` fixture at tests/test_world.cpp:2004, `load_real_map_w`/`pick_start_cell`/`add_combat_unit`, `spawn_base`/`wire_economy`/`spawn_free_units_on_open`/`build_world`/`make_ai_config`/`load_soviet_unit_roster`/`load_ini_from_ra2md`/`capture_png`/`render_world`. (The pre-amendment `power_totals` consumption is GONE — the recovered enum has no power condition.)
   - Byte-identity: Tasks 2–3 add dead pure code (gates re-run); Task 4's only live-path changes are inside `step_ai_teams` behind `tc.triggers_enabled` (default false) + a fresh-state reset in `enable_ai_house` (not hashed) + the additive `completed_success` write in `step_team`'s `kOpDisband` handler (WRITTEN on gate paths, READ only inside the `triggers_enabled` fence, hashed nowhere) — all seven gates asserted; Task 5 parses triggers everywhere but enables them ONLY interactively (cerr-only output; the trigger rng is a separate stream so `--demo-ai-team`'s draws are untouched — `ai_team_hash=312e92ed862a5e74` explicitly re-checked); Task 6's `ai_trigger_hash` is new, `world_hash` untouched (all eight demos asserted); Task 7 re-asserts everything + arm64.
4. **FOREGROUND discipline:** every build/test/demo step in every task explicitly orders a foreground, synchronous, blocking run with a generous timeout (the prior stalled-implementer failure mode — including the monitor-wait variant), and exes run from PowerShell, never Git Bash.
