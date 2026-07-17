# AI-3 AITriggerTypes Weighted-Trigger Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Non-player houses get the AITriggerTypes weighted trigger engine (sub-phase AI-3, the last of the three-sub-phase AI effort): the AI parses the real `[AITriggerTypes]` section of aimd.ini, evaluates each trigger's recovered condition against the live battlefield, selects ONE eligible trigger by weighted-random over its CURRENT weight, creates the trigger's team through AI-2's EXACT team-creation path (unlocking the ~150 non-autocreate TeamTypes), and feeds the created team's outcome back into that trigger's weight (success → rises, failure → falls, clamped) — verified by a new 8th permanent byte-identity gate `--demo-ai-trigger`.

**Architecture:** Task 1 recovers ground truth from gamemd.exe + the real aimd.ini into `docs/re/ai-trigger.md` (RE-GATE before any code); then the change lands additively, protecting all seven byte-identity gates. `engine/aimd_parse.{hpp,cpp}` is EXTENDED (not a new module) with an `AITriggerType` table on `AimdData` and a `parse_ai_triggers` flat-line parser over the UNCHANGED `ra2assets::Ini::section()` list, resolving `condition_object` through caller resolvers to a `{kind, roster index}` pair with art-guard-drop; a new pure module `engine/ai_trigger.{hpp,cpp}` holds the trigger brain (condition dispatch over the FULL recovered ConditionType enum with inert→FALSE, the enable-filter matrix, weighted-random selection from the seeded `ai_rand`, and clamped adaptive weight dynamics) — read-only `TriggerView` in, `TriggerDecision` out, exactly the ai_base/ai_team pattern; `engine/world.{hpp,cpp}` adds a trigger-eval step INSIDE AI-2's existing TeamDelays check pass in `step_ai_teams`, beside `select_autocreate_team` — a SECOND creation path feeding the same `AiTeamState.teams`, with team→trigger attribution links reported back on disband, firewalled behind a `triggers_enabled` bool defaulting false; `main.cpp` parses the real `[AITriggerTypes]` house-scoped inside `make_ai_team_config`, enables triggers on the interactive enemy Soviet house, and wires the `--demo-ai-trigger` gate. **ZERO changes to the AI-2 lifecycle/interpreter.** Spec: `docs/superpowers/specs/2026-07-13-ai-trigger-design.md`.

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
- **[RE-GATE] protocol:** Tasks 2–6 are written with best-known pre-hunt values, each consuming point marked `[RE-GATE APPLIED: ai-trigger.md §N]` with its fallback inline. After Task 1, the controller diffs `docs/re/ai-trigger.md` against every `[RE-GATE APPLIED]` block in Tasks 2–6, amends the plan, and commits the amendment before dispatching the task that uses it. Values the hunt cannot pin are **[RE-FALLBACK]** plan-amendment gates — STOP, present options to the human, amend the plan, commit the amendment BEFORE any dependent dispatch. Never guess silently.
- Run every build/test/demo **FOREGROUND, synchronously, blocking** (generous timeout, e.g. 600000 ms) — do NOT background test runs and wait; a prior phase lost subagents to that (one even hung waiting on a monitor).
- Desktop build: `cmake --build D:\ra2-android\build --target ra2assets_tests` then run `D:\ra2-android\build\ra2assets_tests.exe`. Engine: target `ra2engine`, exe `D:\ra2-android\build\ra2engine.exe`. doctest filters are name-based `--test-case="prefix*"`.
- Tests skip cleanly without `RA2_INSTALL_DIR` — doctest `MESSAGE` + `return`. Parser/engine tests use INLINE fixtures and run host-only (no install needed).
- Scratch files (hunt scripts, extracted aimd.ini, screenshots) to the session scratchpad, never the repo.
- Model assignments: implementation → opus; specs/plans/review → fable; easy/mechanical tasks → sonnet.

### Approved scope decisions (spec §Approved scope decisions)

- **Full recovered AITriggerType struct** parsed from the real `[AITriggerTypes]` section — a FLAT list where each `<id>=Name,field,field,...` line is one complete trigger (NOT a per-id entity section), layouts confirmed/replaced by the Task 1 hunt.
- **FULL recovered ConditionType enum parsed and RE-documented**; every type testable against the TriggerView implemented; any type needing an absent subsystem is RE-documented and **evaluates FALSE** (inert, like AI-2's inert opcodes).
- **Weighted-random over CURRENT weight** among eligible triggers, drawn from the seeded engine `ai_rand` — deterministic, NEVER wall-clock.
- **Adaptive weight dynamics**: created-team outcome adjusts the source trigger's weight, clamped [min_weight, max_weight]; the EXACT adjustment rule and success definition are **Task 1 hunt recoveries, not guesses**.
- **A SECOND creation path BESIDE AI-2's `select_autocreate_team`** — both feed the same `AiTeamState.teams`; **ZERO changes to the AI-2 lifecycle/interpreter**; autocreate keeps running.
- **House-scoped** — any AI house can enable triggers, extending the per-house registry; wired on for the enemy Soviet house.
- **New deterministic gate `--demo-ai-trigger`** (the 8th permanent byte-identity gate).
- **No Ini-layer changes** — AI-3 extends `engine/aimd_parse.hpp` consuming the existing `ra2assets::Ini::section()` ordered key/value list.
- **Decomposition tripwire**: if the condition-type breadth pushes the plan past ~9 tasks, the parser (Task 2) splits into its own spec→plan→execute cycle. (This plan lands at 7 — the tripwire holds.)

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/ai-trigger.md` (create, Task 1) | Calibration authority: `[AITriggerTypes]` field layout, FULL ConditionType enum + comparator encoding, weighted-selection draw, weight dynamics + success rule, enable filters + cadence, composition/attribution |
| `engine/aimd_parse.hpp` + `aimd_parse.cpp` (modify, Task 2) | EXTENDED with `AITriggerType`/`TriggerObject`/`TriggerComparator` + `AimdData::triggers` + `decode_comparator` + `parse_ai_triggers` (flat-line walk, condition_object {kind,index} resolution, team1/team2 resolution, art-guard-drop) |
| `engine/ai_trigger.hpp` + `ai_trigger.cpp` (create, Task 3) | Pure trigger brain: recovered condition/comparator constants, TriggerView/TriggerDecision/AiTriggerState, `trigger_condition_true`/`trigger_filters_pass`/`trigger_eligible`/`select_trigger`/`adjust_weight`/`make_trigger_state` |
| `engine/world.hpp` + `world.cpp` (modify, Task 4) | `triggers_enabled` firewall + side/tech fields on `AiTeamConfig`, `AiTriggerState` on `HouseAiRec`, the trigger-eval step inside `step_ai_teams`'s TeamDelays check pass, disband outcome attribution, `ai_fill_trigger_view` |
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
// ScriptTypes/TeamTypes). Field order + comparator encoding are HUNT OUTPUTS
// [RE-GATE APPLIED: ai-trigger.md §1 — the pre-hunt 18-field order below is
// the spec's approved data model; the RE-GATE confirms or replaces it].

// The counted TechnoType may be a UNIT or a BUILDING [spec §2], so it resolves
// through the caller's two resolvers to a {kind, roster index} pair. None =
// no object (non-counting conditions carry "<none>").
enum class TechnoKind { None = 0, Unit = 1, Building = 2 };
struct TriggerObject {
    TechnoKind kind = TechnoKind::None;
    int index = -1;   // caller's unit- or building-roster index (by kind)
};

// Comparator = count threshold + operator, decoded from the on-disk hex blob
// [RE-GATE APPLIED: ai-trigger.md §2 — pre-hunt: first little-endian dword =
// value, second = operator; op semantics are ai_trigger.hpp's kComp*].
struct TriggerComparator { int value = 0; int op = 0; };

struct AITriggerType {
    std::string id;            // the flat-list key (canonical UPPER)
    std::string name;          // field 0
    int team1 = -1;            // field 1: TeamType id -> AimdData::teams index
                               // (REQUIRED — trigger dropped whole if missing)
    int team2 = -1;            // field 14: reciprocal team, -1 tolerated
                               // (recorded; team1 ONLY is created pre-hunt
                               // [RE-GATE APPLIED: ai-trigger.md §3])
    std::string owner_house;   // field 2 raw token; "<all>"/"<none>"/"" = any
    int tech_level = 0;        // field 3
    int condition_type = -1;   // field 4: recovered enum (ai_trigger.hpp kCond*)
    TriggerObject condition_object;  // field 5: TechnoType or <none>
    TriggerComparator comparator;    // field 6: hex blob, decode_comparator
    double starting_weight = 0.0;    // field 7 (floats in the file)
    double min_weight = 0.0;         // field 8
    double max_weight = 0.0;         // field 9
    bool is_for_skirmish = false;    // field 10
    // field 11 is recorded-only pre-hunt [RE-GATE APPLIED: ai-trigger.md §1]
    int side = 0;              // field 12: 0=any, 1=Allied, 2=Soviet
                               // [RE-GATE APPLIED: ai-trigger.md §5 — pre-hunt]
    bool base_defense = false; // field 13 [RE-GATE APPLIED: ai-trigger.md §1]
    bool enabled[3] = {false, false, false};  // fields 15/16/17: per-difficulty
                               // enables — index binding carries the SAME
                               // hardest<->easiest caveat as ai-base.md's count
                               // arrays [RE-GATE APPLIED: ai-trigger.md §5]
};

// Decode the on-disk comparator hex blob [RE-GATE APPLIED: ai-trigger.md §2 —
// pre-hunt: a fixed-width hex string whose first 8 chars are the count value
// (little-endian byte pairs) and next 8 the operator; the hunt pins the exact
// byte layout]. Returns false (caller drops the trigger) on a malformed blob.
bool decode_comparator(const std::string& hex, TriggerComparator& out);

// Parse [AITriggerTypes] into d.triggers [RE: ai-trigger.md §1]: one trigger
// per flat-list key, file order, via Ini::section(). d.teams must already be
// parsed (parse_aimd) — team1/team2 resolve against it, and a trigger whose
// team1 was dropped/missing is dropped WHOLE. condition_object resolves
// through `unit_index` first, then `building_index` (each canonical-UPPER
// token -> the caller's roster index, or -1 = this install cannot field it);
// an unresolvable non-"<none>" object ART-GUARD-DROPS the whole trigger —
// skipped, never a crash, exactly the parse_aimd discipline [spec §2].
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
// stock Name fields carry no embedded comma [RE-GATE APPLIED: ai-trigger.md §1]).
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
    // [RE-GATE APPLIED: ai-trigger.md §2 — pre-hunt on-disk encoding: dword 0
    // (hex chars 0-7, little-endian) = count value, dword 1 (chars 8-15) =
    // operator; the hunt pins the exact byte layout.]
    return hex_dword(hex, 0, out.value) && hex_dword(hex, 8, out.op);
}

void parse_ai_triggers(const ra2assets::Ini& ini, AimdData& d,
                       const std::function<int(const std::string&)>& unit_index,
                       const std::function<int(const std::string&)>& building_index) {
    std::unordered_map<std::string, int> team_ix;  // TeamType id -> teams index
    for (int i = 0; i < (int)d.teams.size(); ++i) team_ix[d.teams[i].id] = i;
    // FLAT list: every key is one full trigger, file order [RE-GATE APPLIED:
    // ai-trigger.md §1 — the pre-hunt 18-field comma order].
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
        // f[11] recorded-only pre-hunt [RE-GATE APPLIED: ai-trigger.md §1]
        t.side = std::atoi(f[12].c_str());
        t.base_defense = truthy(f[13]);
        const auto t2 = team_ix.find(upper(f[14]));
        t.team2 = t2 == team_ix.end() ? -1 : t2->second;  // recorded; -1 ok
        t.enabled[0] = truthy(f[15]);
        t.enabled[1] = truthy(f[16]);
        t.enabled[2] = truthy(f[17]);
        d.triggers.push_back(std::move(t));
    }
}
```

(`aimd_parse.cpp` also gains `#include <vector>` transitively via the header — no new includes needed beyond the existing `<cctype>`/`<cstdlib>`/`<unordered_map>`.)

- [ ] **Step 1: Failing tests.** Append to `tests/test_aimd.cpp` (inside/after the existing anonymous namespace — `test_unit_index` from the AI-2 fixtures is reused; host-only, no install needed):

```cpp
namespace {
// Real [AITriggerTypes] flat-line shapes [RE-GATE APPLIED: ai-trigger.md §1 —
// pre-hunt 18-field comma order; ids/values are fixture stand-ins in the
// recovered FORMAT; comparator blobs: value dword then op dword, little-endian].
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
    // Normal-only enable; not-for-skirmish; side any.
    "SOVT04=Owner Owns Tanks,02000010-G,<all>,5,1,HTNK,"
    "0200000000000000000000000000000000000000000000000000000000000000,"
    "30.000000,0.000000,60.000000,0,0,0,0,<none>,0,1,0\n";

// Building resolver: GAWEAP -> 7, everything else unknown (-1).
int test_building_index(const std::string& id) {
    return id == "GAWEAP" ? 7 : -1;
}
}  // namespace

TEST_CASE("aimd: decode_comparator unpacks the pre-hunt hex blob") {
    ra2engine::TriggerComparator c;
    REQUIRE(decode_comparator(
        "0300000003000000000000000000000000000000000000000000000000000000", c));
    CHECK(c.value == 3);
    CHECK(c.op == 3);
    REQUIRE(decode_comparator("0a00000005000000", c));   // 16 chars suffice
    CHECK(c.value == 10);
    CHECK(c.op == 5);
    CHECK_FALSE(decode_comparator("03000000", c));       // too short
    CHECK_FALSE(decode_comparator("zz00000003000000", c));  // non-hex
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
    CHECK(a.team2 == 0);                                 // resolved reciprocal
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
// PRE-HUNT standard RA2/YR values; the hunt CONFIRMS or REPLACES the full
// enum AND the implementable/inert split] ---
constexpr int kCondNone = -1;             // no condition -> always TRUE
constexpr int kCondEnemyOwns = 0;         // enemy owns <cmp> of condition_object
constexpr int kCondOwnerOwns = 1;         // owner owns <cmp> of condition_object
constexpr int kCondEnemyYellowPower = 2;  // enemy in yellow (low) power
constexpr int kCondEnemyRedPower = 3;     // enemy in red power
constexpr int kCondEnemyCredits = 4;      // enemy credits vs <cmp> — INERT
                                          // pre-hunt (World cannot see the
                                          // player's credits) [RE: §2 deviations]
constexpr int kCondIronCurtainReady = 5;  // INERT (superweapons absent)
constexpr int kCondChronoSphereReady = 6; // INERT (superweapons absent)
constexpr int kCondNeutralOwns = 7;       // INERT pre-hunt (no neutral house)

// --- Comparator operator encoding [RE-GATE APPLIED: ai-trigger.md §2 —
// pre-hunt standard packing] ---
constexpr int kCompLess = 0;
constexpr int kCompLessEq = 1;
constexpr int kCompEq = 2;
constexpr int kCompGreaterEq = 3;
constexpr int kCompGreater = 4;
constexpr int kCompNotEq = 5;

// --- Weight dynamics [RE-GATE APPLIED: ai-trigger.md §4 — pre-hunt ADDITIVE
// +/-3.0 per outcome, clamped [min_weight, max_weight]; the hunt pins the
// real rule (multiplicative vs additive) and the delta. NOT guessable —
// this constant is the phase's core RE-GATE item.] ---
constexpr double kAiTriggerWeightDelta = 3.0;

// Read-only battlefield snapshot, filled by the caller (World) each trigger
// pass. Count vectors are indexed by the caller's unit/building roster
// indices (the SAME indices parse_ai_triggers resolved condition_object to).
// The exact field set follows the recovered ConditionType enum — the RE-GATE
// extends it as the enum demands (elapsed time, health, ...) [spec §3].
struct TriggerView {
    std::vector<int> own_units;        // per unit-roster-index counts, owner house
    std::vector<int> own_buildings;    // per building-roster-index counts
    std::vector<int> enemy_units;      // enemy-owned counts (same indexing)
    std::vector<int> enemy_buildings;
    double credits = 0.0;              // owner house credits
    double enemy_credits = 0.0;        // kCondEnemyCredits — 0.0 pre-hunt (inert)
    int enemy_power_output = 0;        // kCondEnemyYellowPower/RedPower
    int enemy_power_drain = 0;
    double elapsed_min = 0.0;          // reserved for a recovered time condition
                                       // [RE-GATE APPLIED: ai-trigger.md §2]
    int difficulty = 1;                // AiDifficulty index for enabled[]
    bool is_skirmish = true;           // IsForSkirmish filter input
    std::string house_name;            // owner_house match ("<all>" = any)
    int side = 0;                      // 0=any, 1=Allied, 2=Soviet [RE: §5]
    int tech_level = 10;               // house tech ceiling for TechLevel gate
};

// select_trigger's answer: which trigger fired and which TeamType to create
// (team1 for the owner side — team2 recorded only, pre-hunt [RE-GATE APPLIED:
// ai-trigger.md §3]). trigger = -1: none eligible, create nothing.
struct TriggerDecision {
    int trigger = -1;   // AimdData::triggers index (weight attribution key)
    int teamtype = -1;  // AimdData::teams index to create
};

// Per-house runtime trigger state [spec §2]. The parsed AITriggerType table
// stays IMMUTABLE — only the current weights here move.
struct AiTriggerState {
    std::vector<double> weights;   // CURRENT weight per trigger, PARALLEL to
                                   // AimdData::triggers (init: starting_weight)
    std::vector<int> team_links;   // PARALLEL to the house's AiTeamState::teams:
                                   // triggers index that created teams[i];
                                   // -1 = autocreate (no attribution) [spec §4]
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

// Fresh per-house state: weights seeded from each trigger's starting_weight.
AiTriggerState make_trigger_state(const AimdData& aimd);

// Comparator test [RE-GATE APPLIED: ai-trigger.md §2 — pre-hunt operator
// semantics]. Unknown operator -> FALSE (inert, never a crash).
bool compare_count(int lhs, const TriggerComparator& c);

// Condition dispatch over the FULL recovered enum [RE-GATE APPLIED:
// ai-trigger.md §2]: kCondNone always TRUE; owner/enemy-owns test the counted
// object through the comparator; yellow/red power test the enemy power view
// (pre-hunt: yellow = drain > output; red = output == 0 || drain >= 2*output
// [RE-FALLBACK]); EVERY inert-required-subsystem-absent value (4/5/6/7) and
// every unknown value evaluates FALSE — exactly AI-2's inert-opcode
// discipline. An untestable counting condition (object None / index out of
// the view's range) is FALSE.
bool trigger_condition_true(const AITriggerType& t, const TriggerView& view);

// Enable-filter matrix [RE-GATE APPLIED: ai-trigger.md §5]: per-difficulty
// enabled[view.difficulty] AND (skirmish -> is_for_skirmish) AND side match
// (t.side 0 = any, view.side 0 = unset/any) AND owner_house match
// (""/"<all>"/"<none>" = any, else case-insensitive equal) AND
// t.tech_level <= view.tech_level.
bool trigger_filters_pass(const AITriggerType& t, const TriggerView& view);

// ELIGIBLE = condition TRUE && filters pass [spec §3].
bool trigger_eligible(const AITriggerType& t, const TriggerView& view);

// Weighted-random selection [RE-GATE APPLIED: ai-trigger.md §3 — pre-hunt
// draw: ONE seeded ai_rand draw scaled over the cumulative sum of the
// CURRENT weights of all eligible triggers; the hunt pins the real draw
// shape]. Eligibility additionally requires current weight > 0 and the
// team1 TeamType's live-instance Max cap (Max=0 = UNCAPPED, else count of
// live instances of team1 in `live` < Max) — the trigger create rides the
// SAME caps as autocreate pre-hunt [RE-GATE APPLIED: ai-trigger.md §6].
// `weights` is the house's AiTriggerState::weights (PARALLEL to
// aimd.triggers). Deterministic: same seed -> same pick sequence. Returns
// {-1,-1} when none eligible. ONE trigger per call — the caller enforces one
// per trigger pass. The caller checks TotalAITeamCap (a call-site gate).
TriggerDecision select_trigger(const AimdData& aimd,
                               const std::vector<double>& weights,
                               const std::vector<TeamInstance>& live,
                               const TriggerView& view, std::uint32_t& rng);

// Adaptive weight feedback [RE-GATE APPLIED: ai-trigger.md §4 — pre-hunt
// ADDITIVE +/- kAiTriggerWeightDelta]: success -> the trigger's current
// weight rises, failure -> falls, CLAMPED to [min_weight, max_weight] at
// BOTH bounds. Records the witness fields (adjustments/last_adjusted/
// last_success/last_old_weight/last_new_weight). Out-of-range `trigger`
// is a no-op (never a crash).
void adjust_weight(AiTriggerState& st, const AimdData& aimd, int trigger,
                   bool success);

}  // namespace ra2engine
```

- Produces — `engine/ai_trigger.cpp` (complete):

```cpp
#include "ai_trigger.hpp"
#include <cctype>

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

}  // namespace

AiTriggerState make_trigger_state(const AimdData& aimd) {
    AiTriggerState st;
    st.weights.reserve(aimd.triggers.size());
    for (const AITriggerType& t : aimd.triggers)
        st.weights.push_back(t.starting_weight);
    return st;
}

bool compare_count(int lhs, const TriggerComparator& c) {
    // [RE-GATE APPLIED: ai-trigger.md §2 — pre-hunt operator semantics.]
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
    // [RE-GATE APPLIED: ai-trigger.md §2 — the FULL recovered enum dispatch;
    // inert-required-subsystem-absent -> FALSE, the AI-2 inert discipline.]
    switch (t.condition_type) {
        case kCondNone:
            return true;
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
        case kCondEnemyYellowPower:
            // Pre-hunt: yellow = the enemy is in low power at all
            // [RE-FALLBACK: ai-trigger.md §2 — the hunt pins the threshold].
            return view.enemy_power_drain > view.enemy_power_output;
        case kCondEnemyRedPower:
            // Pre-hunt: red = no output, or drain at 2x output
            // [RE-FALLBACK: ai-trigger.md §2].
            return view.enemy_power_output == 0 ||
                   view.enemy_power_drain >= 2 * view.enemy_power_output;
        default:
            // kCondEnemyCredits / kCondIronCurtainReady /
            // kCondChronoSphereReady / kCondNeutralOwns + every unknown value:
            // INERT -> FALSE [RE: ai-trigger.md §2].
            return false;
    }
}

bool trigger_filters_pass(const AITriggerType& t, const TriggerView& view) {
    // [RE-GATE APPLIED: ai-trigger.md §5 — the enable-filter matrix.]
    if (view.difficulty < 0 || view.difficulty > 2 || !t.enabled[view.difficulty])
        return false;
    if (view.is_skirmish && !t.is_for_skirmish) return false;
    if (t.side != 0 && view.side != 0 && t.side != view.side) return false;
    if (!t.owner_house.empty() && !iequal(t.owner_house, "<all>") &&
        !iequal(t.owner_house, "<none>") &&
        !iequal(t.owner_house, view.house_name))
        return false;
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
    std::vector<int> elig;
    double total = 0.0;
    for (int i = 0; i < (int)aimd.triggers.size() && i < (int)weights.size(); ++i) {
        const AITriggerType& t = aimd.triggers[i];
        if (weights[i] <= 0.0) continue;             // no probability mass
        if (!trigger_eligible(t, view)) continue;
        // team1 live-instance Max cap (Max=0 = UNCAPPED) — the trigger create
        // rides the SAME cap as autocreate pre-hunt [RE-GATE APPLIED:
        // ai-trigger.md §6].
        const int max = aimd.teams[t.team1].max;
        if (max > 0) {
            int cnt = 0;
            for (const TeamInstance& ti : live)
                if (ti.teamtype == t.team1) ++cnt;
            if (cnt >= max) continue;
        }
        elig.push_back(i);
        total += weights[i];
    }
    if (elig.empty() || total <= 0.0) return d;
    // ONE seeded draw over the cumulative weight sum [RE-GATE APPLIED:
    // ai-trigger.md §3 — pre-hunt cumulative-sum walk; deterministic].
    const double pick = (double)ai_rand(rng) / 4294967296.0 * total;
    double acc = 0.0;
    for (std::size_t k = 0; k < elig.size(); ++k) {
        acc += weights[elig[k]];
        if (pick < acc || k + 1 == elig.size()) {
            d.trigger = elig[k];
            d.teamtype = aimd.triggers[elig[k]].team1;
            return d;
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
    // [RE-GATE APPLIED: ai-trigger.md §4 — pre-hunt ADDITIVE +/- delta,
    // clamped at BOTH bounds.]
    double w = old + (success ? kAiTriggerWeightDelta : -kAiTriggerWeightDelta);
    if (w < t.min_weight) w = t.min_weight;
    if (w > t.max_weight) w = t.max_weight;
    st.weights[trigger] = w;
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
    d.triggers.push_back(t);
    return d;
}
// A view where TRG0's condition is TRUE (enemy owns 3 of building 0).
TriggerView firing_view() {
    TriggerView v;
    v.own_units = {0}; v.enemy_units = {0};
    v.own_buildings = {0}; v.enemy_buildings = {3};
    v.difficulty = 1; v.is_skirmish = true;
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

TEST_CASE("ai_trigger: condition dispatch over the recovered enum") {
    AimdData d = one_trigger_aimd();
    TriggerView v = firing_view();
    AITriggerType t = d.triggers[0];
    // enemy-owns (building object): 3 >= 2 -> TRUE; 1 >= 2 -> FALSE.
    CHECK(trigger_condition_true(t, v));
    v.enemy_buildings[0] = 1;
    CHECK_FALSE(trigger_condition_true(t, v));
    // enemy-owns UNIT object.
    t.condition_object = TriggerObject{TechnoKind::Unit, 0};
    v.enemy_units[0] = 5;
    CHECK(trigger_condition_true(t, v));
    // owner-owns.
    t.condition_type = kCondOwnerOwns;
    v.own_units[0] = 2;
    CHECK(trigger_condition_true(t, v));
    v.own_units[0] = 1;
    CHECK_FALSE(trigger_condition_true(t, v));
    // kCondNone: always TRUE, no object consulted.
    t.condition_type = kCondNone;
    t.condition_object = TriggerObject{};
    CHECK(trigger_condition_true(t, v));
    // Yellow power: drain > output [RE-FALLBACK pre-hunt].
    t.condition_type = kCondEnemyYellowPower;
    v.enemy_power_output = 100; v.enemy_power_drain = 150;
    CHECK(trigger_condition_true(t, v));
    v.enemy_power_drain = 50;
    CHECK_FALSE(trigger_condition_true(t, v));
    // Red power: no output OR drain >= 2x output [RE-FALLBACK pre-hunt].
    t.condition_type = kCondEnemyRedPower;
    v.enemy_power_output = 0; v.enemy_power_drain = 0;
    CHECK(trigger_condition_true(t, v));
    v.enemy_power_output = 100; v.enemy_power_drain = 200;
    CHECK(trigger_condition_true(t, v));
    v.enemy_power_drain = 150;
    CHECK_FALSE(trigger_condition_true(t, v));
    // Inert conditions evaluate FALSE [RE: ai-trigger.md §2].
    for (int cond : {kCondEnemyCredits, kCondIronCurtainReady,
                     kCondChronoSphereReady, kCondNeutralOwns, 99}) {
        t.condition_type = cond;
        CHECK_FALSE(trigger_condition_true(t, v));
    }
    // Untestable counting condition (object None / out-of-range) -> FALSE.
    t.condition_type = kCondEnemyOwns;
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
    // Per-difficulty enable.
    t.enabled[1] = false;
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.enabled[1] = true;
    // IsForSkirmish: skirmish view requires the flag.
    t.is_for_skirmish = false;
    CHECK_FALSE(trigger_filters_pass(t, v));
    v.is_skirmish = false;
    CHECK(trigger_filters_pass(t, v));
    v.is_skirmish = true; t.is_for_skirmish = true;
    // Side: 0 = any on either end; mismatch fails.
    t.side = 1;                      // Allied trigger, Soviet house
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.side = 2;
    CHECK(trigger_filters_pass(t, v));
    t.side = 0;
    // Owner house: <all>/<none>/"" = any; case-insensitive name match.
    t.owner_house = "Russians";
    CHECK(trigger_filters_pass(t, v));
    t.owner_house = "RUSSIANS";
    CHECK(trigger_filters_pass(t, v));
    t.owner_house = "Americans";
    CHECK_FALSE(trigger_filters_pass(t, v));
    t.owner_house = "<none>";
    CHECK(trigger_filters_pass(t, v));
    t.owner_house = "<all>";
    // TechLevel gate.
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
    // Sole eligible trigger is picked with probability 1 across a seed sweep.
    for (std::uint32_t seed = 1; seed <= 100; ++seed) {
        std::uint32_t rng = seed;
        std::vector<double> w = {50.0};
        const TriggerDecision td = select_trigger(d, w, live, v, rng);
        CHECK(td.trigger == 0);
        CHECK(td.teamtype == 0);
    }
    // None eligible -> {-1,-1} (condition false).
    TriggerView cold = v;
    cold.enemy_buildings[0] = 0;
    std::uint32_t rng = 7u;
    CHECK(select_trigger(d, {50.0}, live, cold, rng).trigger == -1);
    // Zero current weight -> skipped (no probability mass).
    CHECK(select_trigger(d, {0.0}, live, v, rng).trigger == -1);
    // team1 Max cap: TT0 has max=1; one live instance blocks the trigger
    // [RE-GATE APPLIED: ai-trigger.md §6].
    TeamInstance ti; ti.teamtype = 0;
    live.push_back(ti);
    CHECK(select_trigger(d, {50.0}, live, v, rng).trigger == -1);
    live.clear();
    // Max=0 = UNCAPPED.
    d.teams[0].max = 0;
    live.push_back(ti); live.push_back(ti);
    CHECK(select_trigger(d, {50.0}, live, v, rng).trigger == 0);
}

TEST_CASE("ai_trigger: selection determinism + pick-convergence over weights") {
    // Two eligible triggers over the same team-less shapes.
    AimdData d = one_trigger_aimd();
    d.teams[0].max = 0;                        // uncapped for this case
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
    // Dominant weight: with weights 90 vs 10, the dominant trigger's
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
}

TEST_CASE("ai_trigger: weight dynamics rise/fall clamped at BOTH bounds") {
    AimdData d = one_trigger_aimd();
    AiTriggerState st = make_trigger_state(d);
    REQUIRE(st.weights.size() == 1);
    CHECK(st.weights[0] == 50.0);              // seeded from starting_weight
    // Success rises by the named delta [RE-GATE APPLIED: ai-trigger.md §4].
    adjust_weight(st, d, 0, /*success=*/true);
    CHECK(st.weights[0] == 50.0 + kAiTriggerWeightDelta);
    CHECK(st.adjustments == 1);
    CHECK(st.last_adjusted == 0);
    CHECK(st.last_success);
    CHECK(st.last_old_weight == 50.0);
    CHECK(st.last_new_weight == 50.0 + kAiTriggerWeightDelta);
    // Failure falls.
    adjust_weight(st, d, 0, /*success=*/false);
    CHECK(st.weights[0] == 50.0);
    CHECK_FALSE(st.last_success);
    // Clamp at max_weight (90).
    st.weights[0] = 89.0;
    adjust_weight(st, d, 0, true);
    CHECK(st.weights[0] == 90.0);
    adjust_weight(st, d, 0, true);
    CHECK(st.weights[0] == 90.0);              // stays clamped
    // Clamp at min_weight (10).
    st.weights[0] = 11.0;
    adjust_weight(st, d, 0, false);
    CHECK(st.weights[0] == 10.0);
    adjust_weight(st, d, 0, false);
    CHECK(st.weights[0] == 10.0);
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
- Modify: `engine/world.hpp`, `engine/world.cpp`
- Test: `tests/test_world.cpp` (append)
- CMake: none (world.cpp registered; ai_trigger.cpp registered in Task 3).

**Interfaces:**
- Consumes: `AiTriggerState`/`TriggerView`/`TriggerDecision`/`make_trigger_state`/`select_trigger`/`adjust_weight` (Task 3); `AITriggerType`/`TriggerObject`/`TechnoKind`/`AimdData::triggers` (Task 2); EXISTING machinery only: `step_ai_teams`'s TeamDelays check pass (the `st.check_clock >= check_period` block at world.cpp ~1290-1315 where `select_autocreate_team` is called), the disband erase point (~1439), `ai_unit_type_of`, `power_totals` (production.hpp), `units_`/`buildings_`, `HouseAiRec`, `enable_ai_house`. **NO new tick loop, no new firewall, ZERO changes to `engine/ai_team.{hpp,cpp}`.**
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
    int trigger_side = 0;           // house side for the Side filter (0=any,
                                    // 1=Allied, 2=Soviet) [RE-GATE APPLIED:
                                    // ai-trigger.md §5 — pre-hunt encoding]
    int trigger_tech_level = 10;    // house tech ceiling for the TechLevel
                                    // filter [RE-FALLBACK: skirmish ceiling]
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
// (TT1.autocreate=false) + ONE NON-autocreate TeamType TT2 reachable ONLY
// through the trigger path + ONE trigger keyed "enemy owns >= 1 of building
// roster type 0 (NACNST)". Script overridden to bare disband so the outcome
// lands fast. Fast test clocks carry over from ai2_cfg.
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
    cfg.team.aimd.triggers.push_back(tr);
    cfg.team.triggers_enabled = triggers_enabled;
    cfg.team.rules.total_team_cap = total_team_cap;
    return cfg;
}
}  // namespace

TEST_CASE("World: a trigger fires, creates a NON-autocreate team, and the outcome adjusts its weight") {
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
                                                 // autocreate was silenced
    REQUIRE(ts->adjustments == 1);
    CHECK(ts->last_adjusted == 0);
    CHECK(ts->last_success);                     // survivors at disband [RE-GATE:
                                                 // ai-trigger.md §4 pre-hunt rule]
    CHECK(ts->weights[0] == 50.0 + ra2engine::kAiTriggerWeightDelta);
    CHECK(ts->last_old_weight == 50.0);
    CHECK(ts->last_new_weight == 50.0 + ra2engine::kAiTriggerWeightDelta);
    CHECK(ts->team_links.empty());               // link erased with the team
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

- [ ] **Step 3: Implement in `engine/world.cpp`.**

(a) `enable_ai_house` gains ONE line after `rec.team_state = AiTeamState{};  // AI-2: fresh team state per enable`:

```cpp
    rec.trigger_state = make_trigger_state(rec.cfg.team.aimd);  // AI-3: current
                                    // weights seeded from starting_weight
```

(NB it must read `rec.cfg` — `cfg` was already moved from.)

(b) Append after the AI-2 block (accessor + view fill):

```cpp
// --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---------------------------

const AiTriggerState* World::ai_trigger_state(int house) const {
    return ai_enabled(house) ? &ai_houses_[house].trigger_state : nullptr;
}

void World::ai_fill_trigger_view(int house, const HouseAiRec& rec,
                                 TriggerView& v) const {
    const AiTeamConfig& tc = rec.cfg.team;
    // Per-TechnoType counts, own vs enemy. "Enemy" aggregates EVERY other
    // house (the ai_quarry_exists hostility convention) [RE-GATE APPLIED:
    // ai-trigger.md §5 — pre-hunt; the original consults the house's current
    // enemy]. Unit typing maps through the house's OWN roster
    // (ai_unit_type_of), so enemy-UNIT conditions count only types this
    // roster resolves; BUILDING conditions index the full shared building
    // roster directly (b.type), so enemy-owns-building conditions see the
    // player's base. No new state — a per-pass snapshot [spec §4].
    v.own_units.assign(tc.unit_roster.size(), 0);
    v.enemy_units.assign(tc.unit_roster.size(), 0);
    v.own_buildings.assign(rec.cfg.roster.size(), 0);
    v.enemy_buildings.assign(rec.cfg.roster.size(), 0);
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
    v.credits = rec.credits;
    v.enemy_credits = 0.0;  // player credits are invisible to World —
                            // kCondEnemyCredits stays INERT pre-hunt
                            // [RE: ai-trigger.md §2 deviations]
    // Enemy power = the PLAYER house (0) pre-hunt [RE-FALLBACK: the original
    // consults HouseClass::EnemyHouse; this engine's AI opposes house 0].
    if (house != 0) {
        const PowerTotals pt = power_totals(buildings_, rec.cfg.roster, 0);
        v.enemy_power_output = pt.output;
        v.enemy_power_drain = pt.drain;
    }
    v.elapsed_min = 0.0;    // no recovered time condition pre-hunt [RE-GATE:
                            // ai-trigger.md §2]
    v.difficulty = (int)rec.cfg.difficulty;
    v.is_skirmish = true;   // this engine runs skirmish only [RE: ai-trigger.md §5]
    v.house_name = tc.house_name;
    v.side = tc.trigger_side;
    v.tech_level = tc.trigger_tech_level;
}
```

(c) In `step_ai_teams`, the TeamDelays check-pass block (step 4, world.cpp ~1290) changes to (the autocreate spawn keeps its EXACT existing code; the trigger step is appended INSIDE the same `if (st.check_clock >= check_period)` — the shared-check-pass cadence [RE-GATE APPLIED: ai-trigger.md §5]; the autocreate push gains the guarded link line):

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
                    // AI-3: keep the team->trigger attribution links PARALLEL
                    // to st.teams; an autocreate team carries no link (-1)
                    // [RE: ai-trigger.md §6].
                    if (tc.triggers_enabled)
                        rec.trigger_state.team_links.push_back(-1);
                }
            }
            // AI-3 trigger-eval step [RE-GATE APPLIED: ai-trigger.md §5 —
            // pre-hunt cadence: triggers share AI-2's TeamDelays check pass;
            // the hunt confirms or pins the real AITrigger check period].
            // A SECOND creation path BESIDE autocreate: both feed st.teams
            // and reuse the whole AI-2 recruit/produce/gather/execute
            // machinery UNCHANGED [spec §4]. BOTH sources respect
            // TotalAITeamCap. FIREWALL: triggers_enabled defaults false, so
            // no existing gate ever reaches this block; the draw consumes
            // trigger_state.rng (a separate stream), never st.rng.
            if (tc.triggers_enabled && !tc.aimd.triggers.empty() &&
                (int)st.teams.size() < tc.rules.total_team_cap) {
                TriggerView tv;
                ai_fill_trigger_view(h, rec, tv);
                const TriggerDecision d =
                    select_trigger(tc.aimd, rec.trigger_state.weights, st.teams,
                                   tv, rec.trigger_state.rng);
                if (d.teamtype >= 0) {
                    TeamInstance t;
                    t.teamtype = d.teamtype;   // AI-2's EXACT create path: the
                    st.teams.push_back(t);     // team enters st.teams and
                    ++st.teams_created;        // lives the whole AI-2 lifecycle
                    rec.trigger_state.team_links.push_back(d.trigger);
                    ++rec.trigger_state.triggers_fired;
                    rec.trigger_state.last_fired = d.trigger;
                }
            }
        }
```

(d) In `step_ai_teams`'s team-stepping loop, the `if (t.state == TeamState::Disbanding)` branch gains the outcome attribution at its TOP (before the existing release-survivors code):

```cpp
            if (t.state == TeamState::Disbanding) {
                // AI-3 outcome attribution [RE-GATE APPLIED: ai-trigger.md §4
                // — pre-hunt success rule: SURVIVORS AT DISBAND = success, a
                // wiped team = failure; the hunt pins the real definition
                // (script completion vs survival vs condition-cleared)].
                // team_links is PARALLEL to st.teams; autocreate teams carry
                // -1 and report nothing [spec §4].
                if (tc.triggers_enabled &&
                    i < (int)rec.trigger_state.team_links.size()) {
                    const int trg = rec.trigger_state.team_links[i];
                    if (trg >= 0) {
                        bool survivor = false;
                        for (int m : t.members) {
                            if (m >= 0 && m < (int)units_.size() &&
                                units_[m].alive && units_[m].die_time < 0.0) {
                                survivor = true;
                                break;
                            }
                        }
                        adjust_weight(rec.trigger_state, tc.aimd, trg, survivor);
                    }
                    rec.trigger_state.team_links.erase(
                        rec.trigger_state.team_links.begin() + i);
                }
                // Release survivors: ... (existing code, UNCHANGED)
```

The new fields are NOT read by `world_hash` (main.cpp hashes only unit cell/hp/alive + building type/cell/hp/state) — the same discipline as AI-1/AI-2; nothing to change there, just do not touch `world_hash`.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="World*"
```

- [ ] **Step 5: Byte-identity — prove additive.** All SEVEN gates re-run FOREGROUND, blocking (the trigger step is fenced behind `triggers_enabled=false` everywhere; the AI-2 draws consume `st.rng`, untouched by the separate `trigger_state.rng` stream):

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
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "feat(engine): World AI-3 trigger-eval step in the TeamDelays pass - second create path beside autocreate, team->trigger attribution, disband outcome feedback, triggers_enabled firewall [RE: ai-trigger.md 4-6]

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
                                 // pre-hunt side encoding 0=any,1=Allied,2=Soviet]
    tc.trigger_tech_level = 10;  // skirmish tech ceiling [RE-FALLBACK]
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

**Script (spec §5):** the fixed demo map; the ai-team demo's base layout (full Soviet house-1 base + house-0 player base); house 1 AI-flagged at FIXED difficulty/credits with teams AND triggers enabled from the REAL parsed aimd.ini; the demo then hands the PLAYER a building count satisfying a stock enemy-owns-N-of-X trigger condition (chosen by a DETERMINISTIC runtime scan — no hardcoded trigger id) so a NON-autocreate team is created by the TRIGGER path specifically; fixed `kAiTriggerFrames = 13500` frames (15 min at 15 Hz — ~5 Normal TeamDelays check passes + team lifecycle + disband margin for the outcome). Two runs, byte-compared; **NO hardcoded hash literal** (compute-and-self-compare, the `--demo-ai-team` precedent).

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
// PLAYER a building count satisfying a stock enemy-owns trigger condition
// (deterministic runtime exemplar scan — NO hardcoded trigger id), and lets
// the trigger engine fire/create/attribute over kAiTriggerFrames. Witnesses:
// condition met -> trigger selected w/ weight -> team created (NON-autocreate)
// -> outcome -> weight adjusted old->new. ai_trigger_hash pins the end state.
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

    // Exemplar: the FIRST parsed trigger that (a) keys enemy-owns over a
    // BUILDING object, (b) creates a NON-autocreate team1 (the spec's proof
    // target), (c) passes the Normal/skirmish/side/owner filters this house
    // runs under, (d) fields a finishable squad (taskforce total <= 6).
    // Deterministic runtime scan — NO hardcoded trigger id, so a RE-GATE
    // field-layout amendment cannot orphan the demo [RE-GATE APPLIED:
    // ai-trigger.md §2 exemplars — Task 1's scratch records the stock lines;
    // the controller may tighten this filter from them].
    int exemplar = -1;
    for (int i = 0; i < (int)acfg.team.aimd.triggers.size(); ++i) {
        const ra2engine::AITriggerType& t = acfg.team.aimd.triggers[i];
        if (t.condition_type != ra2engine::kCondEnemyOwns) continue;
        if (t.condition_object.kind != ra2engine::TechnoKind::Building) continue;
        if (!t.is_for_skirmish || !t.enabled[1] || t.base_defense) continue;
        if (t.side != 0 && t.side != 2) continue;
        if (!(t.owner_house.empty() || t.owner_house == "<all>" ||
              t.owner_house == "Russians")) continue;
        const ra2engine::TeamType& t1 = acfg.team.aimd.teams[t.team1];
        if (t1.autocreate) continue;              // NON-autocreate proof target
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
    const long kAiTriggerFrames = 13500;  // 15 min at 15 Hz: ~5 Normal
                                          // TeamDelays passes (2500 frames) +
                                          // recruit/produce/gather/march +
                                          // disband margin for the outcome
    long frame = 0;
    int last_fired_seen = 0;
    int last_adjust_seen = 0;
    bool nonauto_created = false;

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
                std::snprintf(b, sizeof b,
                              "[ai-trigger] team created teamtype=%s autocreate=%s frame=%ld",
                              t1.id.c_str(), t1.autocreate ? "yes" : "no", frame);
                r.witness.emplace_back(b);
                if (!t1.autocreate) nonauto_created = true;
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
    // The witness chain the spec demands (spec §5). Wedge = NOT ok.
    if (last_fired_seen < 1) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO trigger fired");
    }
    if (!nonauto_created) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO non-autocreate team from the trigger path");
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

Expected: the seven legacy pins byte-identical; `--demo-ai-trigger` prints `deterministic=yes`, the witness chain reads exemplar → condition met/trigger selected (with weight) → team created `autocreate=no` → outcome → weight adjusted old→new, and the SAME `ai_trigger_hash` on both invocations (two-run + two-invocation stability). Record the fresh `ai_trigger_hash` value in the task report — do NOT write it into code; the pin is a controller step (Step 7). **HARD RULES for the implementer:** (a) run the demo FOREGROUND, synchronously — never backgrounded, never watched via a monitor; (b) do NOT pin a hash literal in code or tests; (c) if the demo exits nonzero, prints `deterministic=no`, or the witness chain never reaches the weight adjustment (trigger loop wedged — no fire, no non-autocreate create, or no outcome inside `kAiTriggerFrames`), **STOP and escalate to the controller — do NOT commit a red or wedged gate** (the AI-2 Task-7 precedent). A wedge is a design-level finding (wrong exemplar filter, too-short frame budget, or a real brain defect), not something to patch around silently.

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

- [ ] **Step 4: README — AI-3 section.** Add an "AI-3 — AITriggerTypes Weighted Triggers" section after the AI-2 section documenting: the recovered AITriggerType flat-line data model parsed from the REAL `[AITriggerTypes]` (`engine/aimd_parse` extension: one full trigger per `<id>=Name,field,...` key, condition_object resolved to a {unit|building, roster index} pair, comparator hex-blob decode, art-guard-drop of unresolvable objects / missing team1); the pure `engine/ai_trigger` brain (FULL recovered ConditionType dispatch with inert-subsystem-absent → FALSE, the enable-filter matrix, weighted-random selection over CURRENT weights from the seeded separate-stream xorshift32, adaptive weight feedback clamped [min,max]); the World trigger-eval step inside AI-2's TeamDelays check pass — a SECOND creation path beside autocreate feeding the same team list with ZERO AI-2 lifecycle changes, team→trigger attribution links reporting the disband outcome; the `triggers_enabled` firewall keeping the seven legacy gates bit-identical; and that the interactive enemy Soviet house now runs all three AI layers (base + autocreate teams + adaptive triggers). Add the gate row:

```
| --demo-ai-trigger | ai_trigger_hash=<pinned value from Task 6 Step 7> | deterministic=yes; the player is seeded with a building count satisfying a stock enemy-owns trigger condition; the Soviet AI house fires the trigger by weighted-random over current weights, creates a NON-autocreate team through the AI-2 path, and the team's disband outcome adjusts the trigger's weight, clamped (ai-trigger.md) |
```

Add to the "Deviations we take" list (so review does not "fix" them), each cited to ai-trigger.md's deviations section:
  - **Engine xorshift32 in a SEPARATE `AiTriggerState::rng` stream** for the weighted draw vs the original's lockstep game RNG — enabling triggers never perturbs the AI-2 draw sequence.
  - **Pre-hunt weight rule as amended by the RE-GATE** — the additive ±`kAiTriggerWeightDelta` fallback and the survivors-at-disband success rule stand only where ai-trigger.md §4 could not pin the recovered rule; cite the doc's final CONFIRMED/[RE-FALLBACK] status.
  - **Trigger cadence shares AI-2's TeamDelays check pass** (as amended by ai-trigger.md §5 — if the hunt pinned an own AITrigger period, the recovered clock replaced this).
  - **Enemy = all rival houses for counts, the player house (0) for power; enemy-credits condition INERT** (World cannot see the player's sidebar credits) [RE-FALLBACK].
  - **Unit-object conditions resolve within the AI house's own roster scope** (enemy-owns-UNIT triggers naming types outside it art-guard-drop); building objects use the full shared building roster.
  - **Skirmish-only engine**: `is_skirmish` is always true; IsForSkirmish=false triggers are never eligible — plus every remaining `[RE-FALLBACK]` recorded in ai-trigger.md, cited by §.
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
   - Spec §2 (task spine + data model) → the 7-task spine mirrors §2's list exactly (hunt → parser → pure engine → World → main.cpp → gate → finalize); the decomposition tripwire holds (7 ≤ ~9). The approved AITriggerType shape (name, team1/team2 resolved to `AimdData::teams` indices with team1-required drop, owner_house with `<all>`, tech_level, condition_type, condition_object as a {kind, roster index} pair through TWO caller resolvers with art-guard-drop, comparator, starting/min/max weights, enabled[3], is_for_skirmish, side, base_defense) lands in Task 2 with `[RE-GATE APPLIED]` markers; the flat-line format is parsed via the UNCHANGED `Ini::section()` (no Ini-layer changes); `AiTriggerState` (current weights initialized from starting_weight with the parsed table immutable, seeded rng, attribution links) lands in Task 3; the SECOND-path composition with ZERO AI-2 lifecycle changes lands in Task 4.
   - Spec §3 (pure engine) → Task 3: `engine/ai_trigger.{hpp,cpp}`, TriggerView in / TriggerDecision out; FULL enum dispatch with inert→FALSE; the enable-filter matrix (enabled[diff] × is_for_skirmish × side/owner_house × tech_level); weighted-random selection over CURRENT weight from the seeded `ai_rand` (never wall-clock) emitting the team1 TeamType + the trigger id for attribution, none-eligible → nothing; clamped adaptive `adjust_weight` with the delta and success rule as named RE-GATE constants; per-condition doctests on hand-built views, the filter matrix, selection determinism (same seed → identical picks) + pick-convergence (sole-eligible probability 1 across a seed sweep; 90/10 dominant-weight frequency bound), clamping at BOTH bounds. Registered in BOTH cmake files.
   - Spec §4 (World integration) → Task 4: trigger-eval step INSIDE the existing TeamDelays check pass beside `select_autocreate_team` (no new tick loop/firewall); TriggerView filled from `buildings_`/`units_` as a per-pass snapshot (no new state); the create goes through AI-2's EXACT path (a `TeamInstance` pushed to `st.teams` + `teams_created`, zero lifecycle code); attribution links PARALLEL to `st.teams` (autocreate = -1, reports nothing); disband reports success/failure per the pre-hunt survivors rule [RE-GATE]; BOTH sources respect TotalAITeamCap (autocreate inside `select_autocreate_team`, the trigger create at its call-site gate + the team1 Max check inside `select_trigger`); fixed deterministic ordering preserved (base tick → team lifecycle/autocreate → trigger eval); new fields excluded from `world_hash`; the trigger step early-returns via `triggers_enabled=false` (per-house, default false) so the seven gates stay bit-identical — proven by the Task 4 firewall test + all-gates re-run. World tests drive the loop end-to-end: condition fires → NON-autocreate team created → outcome adjusts the weight; plus the cap-skip case (spec §5's "trigger create skipped at TotalAITeamCap").
   - Spec §5 (gate + testing) → Task 6: `--demo-ai-trigger` seeds the player with a count satisfying a stock enemy-owns condition (deterministic runtime exemplar scan, no hardcoded id) so a NON-autocreate team is created by the TRIGGER path specifically (witnessed via `autocreate=no` + the `nonauto_created` ok-gate); fixed frames; `ai_trigger_hash` = FNV over per-trigger current weights (trigger-table order) + the created-team list {teamtype, state, members} + credits; two-run byte-compare, compute-and-self-compare, NO pinned literal; the full witness chain (condition met → trigger selected w/ weight → team created → outcome → weight adjusted old→new); implementer STOP-on-wedge hard rule + controller pin/eyeball step. Task 7: full suite both modes, the SEVEN legacy gate values re-asserted explicitly, arm64 + nm-verify of the new ai_trigger symbols, README AI-3 section + 8th gate row + deviations. Tests skip cleanly without `RA2_INSTALL_DIR` (Task 2/3 fixtures inline host-only; Task 4 world cases behind `load_real_map_w`).
   - Spec §Interactive payoff → Task 5 wires the house-scoped parse into `make_ai_team_config` (the same resolver scoping) and enables triggers on the interactive enemy Soviet house, so it runs all three AI layers.
2. **Placeholder scan:** no TODO/TBD/"similar to Task N" prose in the operative steps; every step shows real code or exact commands + expected output. Two deliberate references to in-tree code remain and are legitimate per the AI-1/AI-2 precedent: Task 6 Step 3 mirrors `run_demo_ai_team` (in-tree at a named location) with every DIFFERENCE shown as real code, and Task 2's install-gated additions append INSIDE an existing named TEST_CASE. All numerics are NAMED constants tagged `[RE-GATE APPLIED: ai-trigger.md §N]` or `[RE-FALLBACK]` with tests asserting the names — pre-hunt values reconciled at the controller RE-GATE. The ai_trigger_hash pin is extract-then-pin by the controller, deliberately never a literal in code.
3. **Type consistency (checked across tasks):**
   - `parse_ai_triggers(const ra2assets::Ini&, AimdData&, const std::function<int(const std::string&)>&, const std::function<int(const std::string&)>&) -> void` and `decode_comparator(const std::string&, TriggerComparator&) -> bool` — defined Task 2, consumed Task 2 tests + Task 5 (`make_ai_team_config`) with exactly these argument lists; Task 3/4/6 consume the Task 2 structs (`AITriggerType` field names `id/name/team1/team2/owner_house/tech_level/condition_type/condition_object/comparator/starting_weight/min_weight/max_weight/is_for_skirmish/side/base_defense/enabled[3]`, `TriggerObject{kind,index}`, `TriggerComparator{value,op}`, `TechnoKind::{None,Unit,Building}`, `AimdData::triggers`) verbatim.
   - `make_trigger_state(const AimdData&) -> AiTriggerState`, `compare_count(int, const TriggerComparator&) -> bool`, `trigger_condition_true/trigger_filters_pass/trigger_eligible(const AITriggerType&, const TriggerView&) -> bool`, `select_trigger(const AimdData&, const std::vector<double>&, const std::vector<TeamInstance>&, const TriggerView&, std::uint32_t&) -> TriggerDecision`, `adjust_weight(AiTriggerState&, const AimdData&, int, bool) -> void` — declared+defined Task 3, consumed Task 4 (`enable_ai_house`/`step_ai_teams`) and the Task 3/4 tests with exactly these argument lists; `AiTriggerState` field names (`weights/team_links/rng/triggers_fired/last_fired/adjustments/last_adjusted/last_success/last_old_weight/last_new_weight`) used identically in Task 4's World code, Task 4's tests, and Task 6's witnesses/hash.
   - `AiTeamConfig::{triggers_enabled,trigger_side,trigger_tech_level}` + `HouseAiRec::trigger_state` + `World::ai_trigger_state(int) -> const AiTriggerState*` + private `ai_fill_trigger_view(int, const HouseAiRec&, TriggerView&) const` — defined Task 4, consumed by Task 4 tests, Task 5 (config fill), Task 6 (hash/witnesses).
   - `make_ai_team_config(const BuildingAssets&, const UnitRoster&, const Rules&, const ra2assets::Ini&, ra2engine::AiDifficulty, int, int, bool enable_triggers = false)` — extended Task 5, consumed Task 6 with `enable_triggers=true`; the two pre-existing call sites compile unchanged via the default.
   - Existing signatures used verbatim from the current tree: `ai_rand(std::uint32_t&)` + `AiDifficulty` (ai_base.hpp), `TeamInstance/AiTeamState/TeamType/TaskForce/TaskForceMember/ScriptType/ScriptAction/kOpDisband` (ai_team.hpp/aimd_parse.hpp), `select_autocreate_team(..., std::uint32_t&)` (the AI-2 CONTRACT-F form at the world.cpp call site), `power_totals(const std::vector<Building>&, const std::vector<BuildingTypeDef>&, int) -> PowerTotals` (production.hpp), `World::add_building(type, cx, cy, fw, fh, house, strength, armor_ix, buildup_total, instant)`, `enable_ai_house(int, AiHouseConfig)`, `ai_enabled/ai_credits/ai_team_state`, `BuildingTypeDef::{fw,fh,strength,armor_ix,id}`, `Ini::section()`, `roster_index(ba, id)`, `fnv_mix`, the `ai2_cfg(bool, bool=false)` fixture at tests/test_world.cpp:2004, `load_real_map_w`/`pick_start_cell`/`add_combat_unit`, `spawn_base`/`wire_economy`/`spawn_free_units_on_open`/`build_world`/`make_ai_config`/`load_soviet_unit_roster`/`load_ini_from_ra2md`/`capture_png`/`render_world`.
   - Byte-identity: Tasks 2–3 add dead pure code (gates re-run); Task 4's only live-path changes are inside `step_ai_teams` behind `tc.triggers_enabled` (default false) + a fresh-state reset in `enable_ai_house` (not hashed) — all seven gates asserted; Task 5 parses triggers everywhere but enables them ONLY interactively (cerr-only output; the trigger rng is a separate stream so `--demo-ai-team`'s draws are untouched — `ai_team_hash=312e92ed862a5e74` explicitly re-checked); Task 6's `ai_trigger_hash` is new, `world_hash` untouched (all eight demos asserted); Task 7 re-asserts everything + arm64.
4. **FOREGROUND discipline:** every build/test/demo step in every task explicitly orders a foreground, synchronous, blocking run with a generous timeout (the prior stalled-implementer failure mode — including the monitor-wait variant), and exes run from PowerShell, never Git Bash.
