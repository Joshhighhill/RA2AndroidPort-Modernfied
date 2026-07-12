# AI-1 Base-Planning Phase Design — The Enemy Builds a Base

**Date:** 2026-07-12
**Status:** Approved (brainstorm sections 1–4 approved individually)
**Depends on:** Economy phase (complete; both houses accumulate income,
`--demo-econ` gate econ_hash=7fed96cb98b68017) and Unit Production phase
(complete; four concurrent queues + placement + exit/rally, `--demo-prod`
gate prod_hash=f0f4e1207ecd713c)

## Goal

Non-player houses get a base-planning brain. This is sub-phase **AI-1** of a
three-sub-phase AI effort: AI-1 is the base-planning brain, AI-2 adds
offensive teams via TaskForce/TeamType/ScriptType, and AI-3 adds the
AITriggerTypes weighted trigger engine. AI-1 is the foundation the other two
hang off. Today the enemy (house 1) already has a pre-built Soviet base and a
working economy but produces nothing and never acts — production and
placement machinery exists (four concurrent queues, placement validity,
exit/rally) but is wired only to house-0 player input. AI-1 gives every
non-player house a planner that drives that EXISTING machinery: it decides
what to build next, where to place it, and rebuilds what it loses.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Planner fidelity | **Full recovered BaseNode/Find_Build_Location planner** — the real what-to-build ordering and where-to-place scan, recovered from the binary |
| Roster | **Full Soviet base roster** for the AI house |
| Difficulty | **Per-difficulty economic handicaps, all three difficulties** (Easy/Medium/Hard rules groups) |
| Rebuild | **Yes** — destroyed structures in the base plan are rebuilt |
| Generality | **House-generalized** — any non-player house runs the AI, not a hardcoded house 1 |
| Verification | **New deterministic gate** `--demo-ai-base` (the 6th permanent byte-identity gate) |

## §1 What the Ghidra hunt must recover (→ `docs/re/ai-base.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001 at
D:\ra2-re, headless analyzeHeadless.bat, JAVA_HOME=C:\Program Files\Java\jdk-21)
plus the rulesmd `[AI]` and difficulty sections, producing `docs/re/ai-base.md`
— the calibration authority BEFORE any code, with a located-functions table, a
"deviations we take" section (mirroring `production.md`/`economy.md`), and
each item landing CONFIRMED / PROBABLE / [RE-FALLBACK]. Six items:

1. **House AI tick & build clock**: the per-frame `House::AI` entry point —
   the scheduler both later AI phases (AI-2/AI-3) hang off; the
   build-attempt cadence (BuildDelay/BaseBuildingDelay frames between
   structure starts) and the gate that says "start a building now."
2. **What-to-build decision (urgency ordering)**: RA2's next-building
   priority — the `[AI]` rules constants (PowerSurplus,
   PowerEmergencyFraction, BaseSizeAdd, refinery/harvester ratios,
   defense-count targets) and the ordering: power-emergency → economy
   (refinery/ore) → production prereqs → base defenses → expansion.
   Recovered as committable constants.
3. **Where-to-place — Find_Build_Location / BaseNode planner**: the
   adjacency-constrained placement scan outward from the ConYard base
   center (buildable + within Adjacent of an owned structure, no overlap,
   foundation-aware), and how the AI stores its planned base so it can be
   re-filled. Reuses the existing green/red placement validity.
4. **Per-difficulty handicaps**: the three rules difficulty groups'
   economic + behavioral multipliers (cost, build speed, build delay, IQ
   gates, starting credits) for Easy/Medium/Hard.
5. **Rebuild semantics**: how the base plan detects a destroyed node and
   reconstructs it — the rebuild vs fresh-expansion path, and any rebuild
   count/limit.
6. **Powered/prereq gating for AI builds**: confirm the AI obeys the same
   prerequisite + power rules as the player (the low-power build-speed
   clamp already exists) so nothing new diverges.

Calibration discipline (unchanged): every engine constant cites this doc.
**All numeric values in this spec are HUNT OUTPUTS, not guesses.** A
**RE-GATE** step (like Production's) lets the hunt's findings amend the plan
before implementation — e.g. if the real placement scan differs from an
assumed outward-spiral, the plan is amended and committed BEFORE dependent
dispatch. **[RE-FALLBACK]**: anything the hunt cannot pin becomes a
plan-amendment gate (present options to the human) — never an uncalibrated
guess. Numeric constants recovered from the binary are committable; GPL
sources at D:\ra2-re are read off-repo, prose conclusions only;
content-anchored hunts only (no trusted YRpp/Ares hardcoded addresses).

## §2 Architecture

Consistent with every prior phase: a pure SDL-free engine module + thin
World integration + a deterministic demo in main.cpp. Nothing player-facing
changes; the AI only drives non-player houses.

- **New pure module `engine/ai_base.{hpp,cpp}`** (no SDL, no World types;
  doctest-testable): the base-planning brain as free functions plus a small
  per-house `AiBaseState` struct (build clock, planned-node list,
  difficulty handle, rebuild bookkeeping). Pure inputs → decision outputs:
  - `next_build(state, world_view, rules, difficulty)` →
    `BuildDecision{type, or none}`
  - `find_build_location(base_view, type, rules)` → cell | none
  - `difficulty_mods(difficulty, rules)` →
    `{cost_mul, speed_mul, delay_mul, …}`

  The module consumes a READ-ONLY view of the house's buildings, credits,
  and power (a lightweight struct World fills in), never the World itself —
  keeping it unit-testable and out of the byte-identity surface.
- **World integration (`engine/world.cpp`)**: new
  `World::step_ai_houses()` called once per `step()`, AFTER the existing
  production/economy step, iterating houses flagged AI. For each: tick its
  `AiBaseState`, and when a decision fires, drive the EXISTING machinery —
  the same `ProductionQueues::start` (Structure/Defense queue), the same
  placement validity, the same building-placed path the player uses. NO new
  production or placement code. House 0 (the player) is never ticked.
- **House registry**: today `house` is just an int 0/1. AI-1 adds a minimal
  per-house record (`is_ai`, `difficulty`, `AiBaseState`) on the World,
  house-generalized so AI-2/AI-3 reuse it. The player house is simply the
  one not flagged AI.
- **main.cpp**: wires the enemy (house 1) as AI in the interactive path,
  expands the Soviet roster art loads (§3), and adds the `--demo-ai-base`
  gate.
- **Byte-identity firewall**: `step_ai_houses()` is a no-op when no house
  is AI-flagged, and the five existing demo scripts flag none — so
  `--demo-combat`/`tank`/`base`/`econ`/`prod` call into an early return and
  stay bit-identical. The new per-house AI fields are excluded from the
  existing `world_hash` (the same discipline as Production's rally fields);
  the new gate hashes them separately.

## §3 Base-planning algorithm

The recovered decision loop, run each build-clock tick for an AI house.

**Per-tick flow (`next_build`):**

1. **Gate on the build clock** — only attempt when the recovered BuildDelay
   (× the difficulty `delay_mul`) frames have elapsed since the last start,
   and the Structure queue slot is free (single-slot, as today).
2. **Urgency ordering** (recovered §1 item 2, evaluated top-down; the first
   satisfied need wins):
   - **Power emergency**: power output < demand × PowerEmergencyFraction →
     NAPOWR.
   - **Economy**: refineries/harvesters below the recovered ratio, or
     credits starved → NAREFN plus its free harvester (which the economy
     phase already spawns).
   - **Production prereqs**: ensure NAHAND then NAWEAP exist, in the
     recovered order, once power is met.
   - **Defenses**: base-defense count below the recovered per-difficulty
     target → NAPILL/tesla, placed toward the threat/base edge.
   - **Expansion**: else grow toward the recovered BaseSizeAdd cap.
3. **Affordability** — the chosen type must pass prerequisite + credit
   (× `cost_mul`) checks; if unaffordable the AI waits (banks income)
   rather than picking something cheaper, matching RA2.

**Placement (`find_build_location`)**: scan outward from the base center
(the ConYard cell) for the first cell where the type's foundation is fully
buildable AND within Adjacent of an owned structure — the exact validity the
player's green/red cursor already computes, reused verbatim. Deterministic
scan order (recovered; [RE-FALLBACK] to a defined nearest-first spiral if
the exact scan can't be pinned). Defenses bias toward the base perimeter
facing map center/enemy. The chosen node is recorded in the planned-base
list.

**Rebuild**: each planned node stores its intended type + cell. When a
node's building is gone (destroyed) and re-buildable, the planner re-queues
that exact type at that cell before considering new expansion — RA2's
re-fill-the-plan behavior, subject to any recovered rebuild limit.

**Difficulty** threads through as the recovered multipliers on
delay/cost/speed plus the defense-count and base-size targets, so Easy
visibly dawdles with a smaller base and Hard expands fast and fortifies.

All numeric values (PowerEmergencyFraction, ratios, targets, multipliers,
scan order) are HUNT OUTPUTS; the §1 RE-GATE fills/corrects them.

## §4 Determinism, the `--demo-ai-base` gate & testing

**Determinism**: the base planner is fully deterministic given world state —
a total urgency ordering, a defined placement scan order, constant
difficulty mods. Where RA2 would consult the lockstep game RNG (e.g. a node
tie-break), use the same engine-local xorshift32 the economy/production
phases adjudicated, as a documented [RE-FALLBACK] — never wall-clock or
global random, seeded deterministically so runs reproduce.

**Demo gate `--demo-ai-base`** (the 6th permanent gate): a headless scripted
run — spawn a lone Soviet ConYard (house 1, AI-flagged) at a fixed
difficulty on a fixed map seed, run the AI a fixed frame count, and let it
build out a base from scratch. Hash the result (`ai_base_hash` over the
sorted building list [type, cell, hp], credits, power, and the planned-node
list); assert two-run byte-stability plus a pinned literal committed to the
README (like prod_hash). Witness lines print the build sequence so a
controller can eyeball authentic order (power → refinery → barracks → …).
The controller also eyeballs a rendered screenshot for placement sanity (the
ore-palette-confetti lesson).

**The five existing gates stay green**: the demo scripts flag no AI house,
`step_ai_houses()` early-returns, and
`--demo-combat`/`tank`/`base`/`econ`/`prod` remain bit-identical — verified
unmoved at phase end.

**Unit tests** (doctest, both install modes, run FOREGROUND per the stall
lesson):

- `next_build` ordering per difficulty;
- `find_build_location` adjacency/foundation/overlap, including the
  boxed-in "no location" case;
- `difficulty_mods` values per the three rules groups;
- rebuild re-queues the exact destroyed node;
- affordability-wait (unaffordable → no pick, banks income).

arm64 NDK (D:\android-ndk-r27c) build green with the new symbols
nm-verified.

**Divergences carried (README)**: engine xorshift for AI tie-break vs the
lockstep RNG; single-slot Structure queue (as today) vs 29-deep; anything
unpinnable lands as an adjudicated [RE-FALLBACK].

## Global constraints (project-wide, unchanged)

- Five permanent byte-identity gates must stay green and are pinned:
  `--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
  hash=5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017,
  `--demo-prod` prod_hash=f0f4e1207ecd713c.
- Ghidra-ground-truth-first: recover from gamemd.exe YR 1.001 + rulesmd
  BEFORE implementing; content-anchored hunts only (YRpp/Ares hardcoded
  addresses untrusted); off-repo GPL references read prose-only.
- No game assets, gamemd.exe, or the Ghidra project are committed;
  recovered numeric constants ARE committable.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`.
- Tests skip cleanly without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`).
- Run executables from PowerShell, not Git Bash (Git Bash returns exit 127
  for these exes).
- Repo on master, solo, no remote; commits end
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- Model assignments: implementation → opus; specs/plans → fable; review →
  fable; easy/mechanical tasks → sonnet.

## Testing summary

| Layer | Tests |
|---|---|
| engine/ai_base | `next_build` urgency ordering per difficulty; `find_build_location` adjacency/foundation/overlap + boxed-in no-location; `difficulty_mods` per the three rules groups; rebuild re-queues the exact destroyed node; affordability-wait |
| integration | `--demo-ai-base` witnesses (build sequence) + `ai_base_hash` two-run byte-stability, pinned in README (new permanent gate); controller screenshot eyeball for placement sanity |
| regression | five existing demo gates byte-identical; full suite both modes; arm64 NDK build with new symbols nm-verified |
