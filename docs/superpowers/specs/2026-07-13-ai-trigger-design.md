# AI-3 AITriggerTypes Weighted-Trigger Phase Design — The Enemy Reads the Battlefield and Responds

**Date:** 2026-07-13
**Status:** Approved (brainstorm sections 1–4 approved individually)
**Depends on:** AI-2 offensive teams (complete; the enemy Soviet house parses
the real aimd.ini, selects autocreate TeamTypes, recruits/produces/gathers and
marches squads under the ScriptType interpreter; `--demo-ai-team`
ai_team_hash=312e92ed862a5e74), AI-1 base planning (`--demo-ai-base`
ai_base_hash=cd1f7c6a29933f0a), plus the Economy and Unit Production phases
those depend on (`--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c).

## Goal

Non-player houses get the AITriggerTypes weighted trigger engine. This is
sub-phase **AI-3**, the last of the three-sub-phase AI effort: AI-1 built the
base-planning brain, AI-2 added autocreate offensive teams, and AI-3 adds
condition-driven team creation with adaptive weighting. Today the enemy fields
only `Autocreate=yes` squads on a timer; the ~150 non-autocreate TeamTypes in
the real aimd.ini sit unreachable. AI-3 parses the real `[AITriggerTypes]`
section, evaluates each trigger's recovered condition against the live
battlefield (owner/enemy per-TechnoType counts, credits, time, ...), selects
one eligible trigger by weighted-random over its CURRENT weight, creates the
trigger's team through AI-2's EXACT team-creation path, and feeds the created
team's outcome back into that trigger's weight (success → rises, failure →
falls, clamped) so the AI adapts its mix over the match. The AI-2 hunt set
this boundary explicitly (`docs/re/ai-team.md` §4): "AI-3 composes on top by
adding trigger-gated CreateTeam calls against the same
TeamType/TaskForce/ScriptType structures" — AI-3 delivers exactly that. One
spec→plan→execute cycle, ~7–8 tasks, the same spine as AI-1/AI-2.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Data model | **Full recovered AITriggerType struct** parsed from the real `[AITriggerTypes]` section — a FLAT list where each `<id>=Name,field,field,...` line is one complete trigger (NOT a per-id entity section), layouts confirmed/replaced by the Task 1 hunt |
| Condition breadth | **FULL recovered ConditionType enum parsed and RE-documented**; every type testable against the TriggerView implemented; any type needing an absent subsystem is RE-documented and **evaluates FALSE** (inert, like AI-2's inert opcodes) |
| Selection | **Weighted-random over CURRENT weight** among eligible triggers, drawn from the seeded engine `ai_rand` — deterministic, NEVER wall-clock |
| Weight dynamics | **Adaptive**: created-team outcome adjusts the source trigger's weight, clamped [min_weight, max_weight]; the EXACT adjustment rule and success definition are **Task 1 hunt recoveries, not guesses** |
| Composition | **A SECOND creation path BESIDE AI-2's `select_autocreate_team`** — both feed the same `AiTeamState.teams`; **ZERO changes to the AI-2 lifecycle/interpreter**; autocreate keeps running |
| Generality | **House-scoped** — any AI house can enable triggers, extending the per-house registry; wired on for the enemy Soviet house |
| Verification | **New deterministic gate `--demo-ai-trigger`** (the 8th permanent byte-identity gate) |
| Ini substrate | **No Ini-layer changes** — AI-3 extends `engine/aimd_parse.hpp` (an AITriggerType table added to `AimdData`) consuming the existing `ra2assets::Ini::section()` ordered key/value list |

## §1 What the Ghidra hunt must recover (→ `docs/re/ai-trigger.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001
Ghidra project at D:\ra2-re, headless analyzeHeadless.bat,
JAVA_HOME="C:\Program Files\Java\jdk-21") plus the real aimd.ini, producing
`docs/re/ai-trigger.md` — the calibration authority BEFORE any code, with a
located-functions table, a "deviations we take" section (mirroring
`ai-team.md`/`ai-base.md`), and each item landing CONFIRMED / PROBABLE /
[RE-FALLBACK] with function address + decompile reasoning. Anchor on the
literal `"AITriggerTypes"` section string, `"AITriggerTypesEnable"`, and the
AITrigger field strings, plus the condition/weight consult sites reachable
from the AI-2 House-AI tick chain (the located-functions table in
`docs/re/ai-team.md` is the entry map). YRpp/Ares addresses are UNTRUSTED —
prose hints only. Six items:

1. **`[AITriggerTypes]` section format + field layout**: the exact
   comma-field order and count of the `<id>=Name,field,field,...` line —
   which positions hold team1/team2, owner house, tech level, condition
   type/object/comparator, the starting/min/max weights, the per-difficulty
   enables, IsForSkirmish, side, base-defense — and which fields are
   load-bearing for the runtime vs recorded-only.
2. **The FULL ConditionType enum**: every value → what it tests (owner house
   owns ≥/≤ N of a TechnoType, enemy owns N, credits threshold, elapsed
   time, health thresholds, ...) plus the comparator encoding — how the
   operator and count threshold are packed in the trigger line (prose refs
   hint at a fixed-width hex blob; the hunt pins the exact on-disk encoding
   and byte layout). Each value marked **implementable** (testable against
   the TriggerView) or **inert-required-subsystem-absent** (evaluates FALSE,
   RE-documented).
3. **The weighted-selection mechanism**: how eligible triggers are gathered
   per pass and how the weighted-random draw over current weights actually
   works (the exact draw — cumulative-sum walk vs other); and which of
   team1/team2 is created for the owning side (pre-hunt: team1 for the
   owner, team2 the reciprocal — the hunt confirms whether team2 is ever
   consulted and by whom).
4. **Weight dynamics**: the starting/min/max weight semantics; the EXACT
   success/failure adjustment rule (multiplicative vs additive, and the
   delta); and what counts as success — script completion vs team survival
   vs condition-cleared. This is the phase's core calibration item and is
   NOT guessable.
5. **Enable filters + cadence**: the per-difficulty enabled flags (and their
   index→difficulty binding — the same hardest↔easiest caveat as
   ai-base.md/ai-team.md's count arrays), IsForSkirmish, Side/OwnerHouse
   matching (including the `<all>` token), TechLevel gating, and whatever
   `AITriggerTypesEnable` globally gates; plus the evaluation cadence — does
   trigger evaluation share AI-2's TeamDelays check pass, or run on its own
   AITrigger check period?
6. **Composition**: confirm triggers create teams via the SAME
   CreateTeam/TeamType path AI-2 uses (the ai-team.md §4 boundary); the
   TotalAITeamCap interaction (is a trigger create capped by the same
   per-house cap?); and how the original attributes a created team back to
   its source trigger for the weight feedback.

Calibration discipline (unchanged): every engine constant cites this doc.
**All numeric values in this spec are HUNT OUTPUTS, not guesses.** A
controller **RE-GATE** amendment step precedes implementation — the hunt's
findings amend the plan and are committed BEFORE dependent dispatch, and
`[RE-GATE APPLIED]` markers are threaded through every dependent task (the
ai_base.hpp/ai_team.hpp precedent). **[RE-FALLBACK]**: anything the hunt
cannot pin becomes a plan-amendment gate (present options to the human) —
never an uncalibrated guess.

## §2 Architecture & recovered data model

Consistent with every prior phase: a pure SDL-free engine module + thin
World integration + a deterministic demo in main.cpp. One spec→plan→execute
cycle, ~7–8 tasks, mirroring AI-2's spine:

- **Task 1**: Ghidra hunt → `docs/re/ai-trigger.md` (RE-GATE before
  implementation; §1 above).
- **Task 2**: aimd parser extension — parse `[AITriggerTypes]` → the
  AITriggerType table added to `AimdData` (pure, testable).
- **Task 3**: pure engine `engine/ai_trigger.{hpp,cpp}` — condition eval +
  weighted selection + weight dynamics.
- **Task 4**: World integration — TriggerView fill + trigger-driven create
  composing with autocreate + weight attribution.
- **Task 5**: main.cpp wiring — parse the real `[AITriggerTypes]`
  house-scoped, enable on the enemy Soviet house.
- **Task 6**: `--demo-ai-trigger` gate (the 8th) + `ai_trigger_hash` +
  witness lines.
- **Task 7**: finalize — suites both modes, 7 existing gates byte-identical,
  arm64, README.

**Decomposition tripwire**: if the condition-type breadth pushes the plan
past ~9 tasks, the parser (Task 2) splits into its own spec→plan→execute
cycle and AI-3 proper consumes its output.

### Recovered data model

Layouts below are the approved shape; the Task 1 hunt CONFIRMS or REPLACES
them (RE-GATE), and the parser/tests are written against the amended form.

- **AITriggerType** — `name`; `team1`/`team2` (TeamType ids resolved to
  `AimdData::teams` indices — team1 is the team created for the owner side,
  team2 the reciprocal; whether team2 is ever consulted is §1 item 3, and
  pre-hunt AI-3 creates team1 ONLY, recording team2); `owner_house`
  (side/country token, `<all>` = any); `tech_level`; `condition_type`
  (recovered enum); `condition_object` (the TechnoType being counted,
  resolved to a roster index, art-guard-dropped like AI-2); `comparator`
  (operator + count threshold, decoded per §1 item 2);
  `starting_weight`/`min_weight`/`max_weight`; `enabled[3]`
  (per-difficulty), `is_for_skirmish`, `side`, `base_defense`.
- **Flat-line format**: parsed from the `[AITriggerTypes]` section — a FLAT
  list where each `<id>=Name,field,field,...` line is one full trigger (NOT
  a per-id entity section like TaskForces/ScriptTypes/TeamTypes), via the
  existing `Ini::section()` ordered key/value list. **No Ini-layer changes.**
- **condition_object resolution (explicit)**: the counted TechnoType may be
  a unit OR a building, so it resolves through a caller-provided resolver to
  a `{kind: unit|building, roster index}` pair; a token this install cannot
  resolve (art missing / not loaded) **art-guard-drops the whole trigger**
  — skipped, never a crash, exactly the AI-2 `parse_aimd` discipline. A
  trigger naming a dropped/missing team1 is likewise dropped whole.
- **Per-house runtime `AiTriggerState`**: the mutable CURRENT weight per
  trigger (initialized from `starting_weight` — the parsed table itself
  stays immutable), the seeded rng stream, and the team→trigger attribution
  links (a created team's outcome adjusts the RIGHT trigger's weight).
- **Composition**: AI-3 adds a SECOND creation path BESIDE AI-2's
  `select_autocreate_team`; both feed the same `AiTeamState.teams` and reuse
  AI-2's entire recruit→produce→gather→execute machinery unchanged.
  Autocreate keeps running; triggers unlock the ~150 non-autocreate teams on
  real conditions. **NO changes to the AI-2 lifecycle/interpreter.**

## §3 The pure trigger engine (condition eval + weighted selection + weight dynamics)

**New pure module `engine/ai_trigger.{hpp,cpp}`** (no SDL, no World types;
doctest-testable): View-in/Decision-out like ai_base/ai_team — consumes a
read-only `TriggerView` the caller (World) fills and emits a
`TriggerDecision` the caller executes. Same conventions as
ai_base.hpp/ai_team.hpp: every numeric constant is a hunt output cited to
`docs/re/ai-trigger.md`; the engine-local seeded xorshift32 `ai_rand`
adjudicates ALL draws (NEVER wall-clock); every piece is doctest-testable on
hand-built views. Runs once per management cycle — the same TeamDelays check
pass AI-2 ticks (pre-hunt cadence assumption; §1 item 5 confirms or pins the
real AITrigger check period, RE-GATE amends). Inputs: the AITriggerType
table + the house's `AiTriggerState` (current weights, rng, attribution
links) + the `TriggerView` — per-TechnoType counts owned by the owner house
and by the enemy, credits, elapsed time, difficulty index; the exact field
set follows the recovered ConditionType enum (RE-GATE extends the view as
the enum demands). Three responsibilities:

1. **Condition evaluation** — resolve each trigger's `condition_type`
   against the TriggerView and test it through the comparator (operator +
   count threshold). The FULL recovered ConditionType set is dispatched
   (owner/enemy owns ≥/≤ N of a type, credits, time, health thresholds,
   ...); any type needing an absent subsystem is RE-documented and
   **evaluates FALSE** — inert, exactly like AI-2's inert opcodes. A trigger
   is ELIGIBLE only if its condition is TRUE **and** it passes the enable
   filters: per-difficulty `enabled[diff]` (indexed by the house's own
   AiDifficulty), `is_for_skirmish`, side/`owner_house` match, and
   `tech_level`.
2. **Weighted selection** — among eligible triggers, pick ONE by
   weighted-random over the CURRENT weight (drawn from `ai_rand`),
   deterministically; emit a `TriggerDecision` naming the TeamType to create
   (team1 for the owner side) and the trigger id to attribute the team to.
   None eligible → create nothing.
3. **Weight dynamics (adaptive)** — the created team's outcome feeds back:
   success → the trigger's current weight rises, failure → it falls, clamped
   to [min_weight, max_weight] at both bounds. The EXACT adjustment rule
   (multiplicative vs additive + the delta) and what counts as success
   (script completion vs survival vs condition-cleared) is a Task-1 hunt
   recovery (§1 item 4), NOT a guess. Attribution flows through the
   `AiTriggerState` link stored at create; the World reports the outcome on
   disband.

**Determinism**: only the seeded `ai_rand` — same seed, same pick sequence.
**Testability (pure doctests)**: each condition type evaluated on a
hand-built TriggerView; the enable-filter matrix; weighted-selection
determinism (same seed → identical picks) plus pick-convergence across
seeds — a sole eligible trigger is picked with probability 1, and across a
seed sweep a dominant-weight trigger's empirical pick frequency tracks the
weight ratios; weight rise/fall clamping exercised at BOTH bounds.

## §4 World integration, outcome attribution & composition with autocreate

**Hooks in `engine/world.cpp`** inside the existing `ai_any_` firewall: AI-3
adds a trigger-evaluation step to AI-2's EXISTING TeamDelays management
pass, beside `select_autocreate_team` — **no new tick loop, no new
firewall**. Per management cycle:

1. **Fill the TriggerView** — the World counts per-TechnoType owner- and
   enemy-owned buildings + units, credits/elapsed-time/difficulty, reading
   the same `buildings_`/`units_` the AI already walks. No new state — a
   per-pass snapshot.
2. **Call the pure engine** → `TriggerDecision`; if it names a TeamType,
   create it through AI-2's EXACT team-creation path (the same create the
   autocreate path uses: the team enters `AiTeamState.teams` and
   recruits/produces/gathers/executes with **ZERO new lifecycle code**);
   store the team→trigger attribution link in `AiTriggerState`.
3. **Autocreate still runs in the same pass** — two independent creation
   sources, one shared team list; BOTH respect `TotalAITeamCap` (a trigger
   create is skipped when the house is at cap).

**Outcome attribution**: when a trigger-spawned team disbands (the AI-2
lifecycle already reaches Disbanding), `step_ai_teams` reports
success/failure — per the hunt-recovered rule (§1 item 4) — back to the pure
engine, which adjusts that trigger's current weight via the stored link. A
team with no trigger link (autocreate) reports nothing.

**Byte-identity**: all new `AiTriggerState` fields are EXCLUDED from
`world_hash`; the trigger step early-returns when no AI house has triggers
enabled, so the SEVEN existing gates stay bit-identical — only
`--demo-ai-trigger` and the interactive path enable triggers. **Ordering**
is fixed and deterministic: base tick (AI-1) → team lifecycle/autocreate
(AI-2) → trigger eval/attribution (AI-3).

## §5 The `--demo-ai-trigger` gate, testing & byte-identity

**8th gate `--demo-ai-trigger` / `ai_trigger_hash`**: seed a Soviet AI house
in a condition that fires a trigger — e.g. give the player a building/unit
count satisfying a stock enemy-owns-N-of-X condition — so a NON-autocreate
team is created by the TRIGGER path specifically (proving the new path, not
AI-2 autocreate); run fixed frames; hash deterministically — FNV over the
trigger state: per-trigger current weights (trigger-table order) + the
created-team list `{teamtype, state, members}` (creation order) + credits —
two-run byte-compare, **NO pinned literal** (compute-and-self-compare, like
`--demo-ai-team`). Witness lines: condition met → trigger selected (with its
weight) → team created (teamtype id) → team outcome → weight adjusted
(old→new, clamped).

**Seven existing gates re-verified byte-identical in finalize:**
`--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a,
`--demo-ai-team` ai_team_hash=312e92ed862a5e74.

**Testing**: pure `test_ai_trigger.cpp` — each condition type on a
hand-built TriggerView; the enable-filter matrix; weighted-selection
determinism + pick-convergence (§3); weight clamping at both bounds; the
parser round-trips inline real-format `[AITriggerTypes]` fixtures including
a condition_object art-guard-drop case. Integration is exercised via the
World loop in the gate demo. Tests skip cleanly without `RA2_INSTALL_DIR`
(inline fixtures, host-only).

**Interactive payoff**: the enemy Soviet house runs all three AI layers —
builds its base (AI-1), fields autocreate squads (AI-2), AND spawns
condition-appropriate response teams whose weighting adapts over the match
(AI-3). arm64 NDK build with the new ai_trigger symbols nm-verified; README
gets the AI-3 section, the 8th gate row, and a deviations note citing
`docs/re/ai-trigger.md`.

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original RA2 behavior (Ghidra / real
  aimd.ini/rulesmd / prose-only GPL refs) BEFORE implementing; empirical
  guessing caused real bugs in prior phases. Every numeric AI constant is a
  hunt output; tests assert against named constants.
- SEVEN byte-identity gates must stay green (values above). The trigger step
  early-returns when no AI house has triggers enabled; new per-house
  AI-trigger fields excluded from `world_hash`.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the
  install.
- No game assets / GPL source / Ghidra project committed; recovered numeric
  constants ARE committable.
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs —
  the plan's briefs must order FOREGROUND synchronous runs with generous
  timeouts.

## Testing summary

| Layer | Tests |
|---|---|
| aimd parser extension (pure) | round-trips inline real-format `[AITriggerTypes]` fixtures (flat `<id>=Name,field,...` comma-field lines); condition_object art-guard-drop (unresolvable TechnoType → trigger dropped whole); dropped/missing team1 drops the trigger; host-only, no install needed |
| engine/ai_trigger (pure) | each recovered condition type on a hand-built TriggerView (incl. inert-subsystem-absent → FALSE); the enable-filter matrix (enabled[diff] × is_for_skirmish × side/owner_house × tech_level); weighted-selection determinism (same seed → identical picks) + pick-convergence across seeds (sole eligible → probability 1; dominant weight → frequency tracks ratios); weight rise/fall with clamping at BOTH bounds |
| integration | `--demo-ai-trigger` witnesses (condition met → trigger selected w/ weight → team created → outcome → weight adjusted old→new) + `ai_trigger_hash` two-run byte-compare (compute-and-self-compare, no pinned literal); trigger create skipped at TotalAITeamCap |
| regression | seven existing demo gates byte-identical (values in §5); full suite both modes; arm64 NDK build with new ai_trigger symbols nm-verified |
