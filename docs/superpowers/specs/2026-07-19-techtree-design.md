# CB-5 Tech-Tree / Prerequisites Phase Design — Completing the Buildability Predicate with Full `Prerequisite=` Parsing and a Tech-Building Tier

**Date:** 2026-07-19
**Status:** Approved (controller-scoped, formalized autonomously)
**Depends on:** the complete AI arc, Unit Production (the four-queue
`ProductionQueues` + the EXISTING per-unit prerequisite check), Economy,
Buildings, combat, and CB-1/2/3/4 — the **twelve** permanent byte-identity
gates this phase must keep bit-identical: `--demo-combat` logs=78,
`--demo-tank` logs=144, `--demo-base` 5273b5e71ca009e1, `--demo-econ`
econ_hash=7fed96cb98b68017, `--demo-prod` prod_hash=f0f4e1207ecd713c,
`--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a, `--demo-ai-team`
ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc, `--demo-transport`
transport_hash=fe500f946e913645, `--demo-aircraft` aircraft_hash=a69642564cf64773,
`--demo-naval` naval_hash=6aee2c82bc024e86, `--demo-superweapon`
superweapon_hash=189496664f5eca68. **Reuses the EXISTING, already-shipping
buildability machinery verbatim** — `ra2engine::prerequisites_met`,
`can_start` (buildings), and `can_start_unit` (units) in `engine/production.cpp`
(`docs/re/production.md` §1.6, `docs/re/construction.md` §10), which already
implement the RA2 AND-semantics **including the prereq-group tokens**
(`POWER`/`FACTORY`/`BARRACKS`/`RADAR`/`TECH`/`PROC`), and the group membership
already loaded from the real rulesmd via `Rules::prerequisite`/
`Rules::prerequisite_group` into `BuildingAssets::groups`. This is the
lightest content-breadth phase since the prereq predicate is already correct
and battle-tested; CB-5 **completes** it rather than building it new.

## Goal

Complete the **tech-tree / prerequisite progression**: buildability gated by
the house's owned buildings through the real RA2 prerequisite system — with
(1) **full `Prerequisite=` parsing for units** (the last buildable category
still using a hardcoded single-token prereq), (2) a **tech building (the
Allied Battle Lab, `GATECH`)** belonging to the `TECH` prereq group that gates
an **advanced (tier-3) unit** (the Prism Tank `SREF`, `Prerequisite=GAWEAP,GATECH`),
and (3) **sidebar availability state for unit tabs** (a unit cameo shown
unavailable until its prereqs are met — the building tabs already do this;
the unit tabs do not yet). Demonstrated deterministically — the advanced unit
is **UNBUILDABLE** until its full prereq chain incl. the Battle Lab is
satisfied, then **BUILDABLE** — and gated by the **13th permanent byte-identity
gate `--demo-techtree`** with a `techtree_hash`. This is **CB-5**, the FINAL
content-breadth phase (CB-1 Transports ✓ → CB-2 Aircraft ✓ → CB-3 Naval ✓ →
CB-4 Superweapons ✓ → **CB-5 Tech-tree**).

Unlike prior CB phases, which each grafted a NEW subsystem beside the sim
(a locomotor, a per-house SW registry), CB-5 touches the **existing production
path** — the one surface the byte-identity discipline (§8) most cares about.
The saving grace, established by reading the code (§ Current-engine reality),
is that **`--demo-prod` bypasses the buildability predicate entirely** (it
calls `ProductionQueues::start_unit(...)` directly, never `can_start_unit`),
so extending unit-prereq DATA cannot move `prod_hash`; and the AI gates that
DO consult the predicate resolve the existing rosters to the identical
decision under the superset. So this phase's risk is not "a new hot path" but
"prove the extended prereq data is a byte-identical superset for every
existing roster, and only the new gate exercises the new tech-gating." One
spec→plan→execute cycle, ~7 tasks, the same spine as every prior phase.

## Recovered exemplar ground truth (committable — read from the real install)

The prereq **mechanism** and **group membership** are already recovered and
CONFIRMED in `docs/re/production.md` §1.6 and `docs/re/construction.md` §10
(`FUN_00505180`, the token-ANDing check; the `[General] Prerequisite<Group>`
lists parsed to six BuildingType vectors). Read from the real
`ra2md.mix → localmd.mix → rulesmd.ini` on this install with `ra2extract`
(committable; the INTERPRETATION — the exact `TECH`-group membership, the
Battle-Lab id, the advanced unit's full token list — is the §1 hunt's to
CONFIRM against the recovered data):

- **Prerequisite groups** (`[General]`, already recovered, `production.md` §1.6):
  - `PrerequisitePower   = GAPOWR,NAPOWR,NANRCT,YAPOWR`
  - `PrerequisiteFactory = GAWEAP,NAWEAP,YAWEAP`
  - `PrerequisiteBarracks= NAHAND,GAPILE,YABRCK`
  - `PrerequisiteRadar   = GAAIRC,NARADR,AMRADR,NAPSIS`
  - **`PrerequisiteTech    = GATECH,NATECH,YATECH`** — the TECH group; the
    Allied member is **`GATECH`** (the Allied Battle Lab / Tech Center).
  - `PrerequisiteProc    = GAREFN,NAREFN,YAREFN` ; `PrerequisiteProcAlternate=SMIN`
- **`[GATECH]`** (the Allied Battle Lab — the tech building this phase adds):
  a `BuildingType` with a `Prerequisite=` chain of its own (expected `POWER`
  + a factory/radar tier, e.g. `Prerequisite=GAWEAP,GARADR` or similar) and a
  `TechLevel>0`; it is the `TECH`-group member for the Allied side. **The exact
  `[GATECH] Prerequisite=` and `TechLevel=` are `ra2extract` reads the hunt
  quotes (§1 item 4).**
- **`[SREF]`** (Prism Tank — the advanced tier-3 unit exemplar, Allied):
  **`Prerequisite=GAWEAP,GATECH`** (`production.md` §1.6) — needs the War
  Factory AND the Battle Lab — with `TechLevel>0` and a `Cost`. This is the
  unit the demo proves UNBUILDABLE→BUILDABLE across the Battle-Lab placement.
- **Existing player-roster units** (`production.md` §1.6, the byte-identity
  anchors the full-parse must reproduce):
  `E1 (GI)=GAPILE`, `GGI=GAPILE`, `MTNK (Grizzly)=GAWEAP`, `FV (IFV)=GAWEAP`.
  The hunt CONFIRMS the real `[E1]/[GGI]/[MTNK]/[FV] Prerequisite=` values so
  the switch from the hardcoded single token to `rules.prerequisite(id)`
  resolves to the IDENTICAL buildable decision (§8).

> **Discrepancy the hunt resolves (item 4):** the controller brief names the
> Allied Battle Lab `GACTEK`; the already-recovered `production.md` §1.6
> ground truth (real rulesmd on this install) lists the `TECH`-group members
> as `GATECH,NATECH,YATECH` and `SREF=GAWEAP,GATECH`. This spec uses **`GATECH`**
> (the recovered id) and treats "GACTEK" as a brief typo; the §1 hunt +
> `ra2extract` confirm the exact id before any code.

**Design consequence — nearly the entire predicate already exists.** The
buildability decision is `prerequisites_met(tokens, buildings, types, house,
groups)` (`production.cpp:39`), a pure AND over tokens where a plain token = "own
a live, Idle building of that id" and a group token = "own any member of
`groups[token]`". `can_start`/`can_start_unit` wrap it with a `tech_level>0`
gate + a factory-alive gate. The `TECH` group is ALREADY in
`BuildingAssets::groups` (`main.cpp:1577`, `rules.prerequisite_group("Tech")`).
CB-5 adds NO new predicate; it (a) routes units onto the full-parse data the
predicate already consumes, (b) puts a `TECH`-group building (`GATECH`) into
the roster so the predicate has something to satisfy `TECH` with, and (c)
surfaces the predicate's unit-tab result in the sidebar chrome.

## Current-engine reality (verified by reading the code — drives §8)

**This is the load-bearing section: whether `--demo-prod` and the AI
base-planner go through the buildability predicate.**

- **The buildability predicate already exists, is correct, and handles group
  tokens.** `prerequisites_met` (`production.cpp:39–63`) ANDs every token,
  resolving group tokens against `groups`; `can_start` (buildings,
  `production.cpp:65–74`) and `can_start_unit` (units, `production.cpp:138–149`)
  wrap it with `tech_level>0` + factory-alive. The prereq groups
  (`POWER/BARRACKS/PROC/FACTORY/RADAR/TECH`) are loaded ONCE from the real
  rulesmd into `BuildingAssets::groups` (`main.cpp:1571–1578`). **No new
  buildability code is needed.**
- **Buildings already full-parse `Prerequisite=`; units DO NOT.** A
  `BuildingTypeDef` gets `d.prerequisite = rules.prerequisite(id)` (the full
  rulesmd token list, `main.cpp:1435`). A `UnitTypeDef` gets
  `d.prerequisite = {e.prereq}` — a **single hardcoded building id** from a
  static `UnitRosterEnt` table (`main.cpp:1151`, `1204–1207`:
  `{"E1","GAPILE"},{"GGI","GAPILE"},{"MTNK","GAWEAP"},{"FV","GAWEAP"}`).
  **Unit full-`Prerequisite=` parsing is the one genuine data gap CB-5 fills.**
- **`--demo-prod` BYPASSES the buildability predicate.** `simulate_prod`
  (`main.cpp:4685+`) starts every unit with `q.start_unit(QueueCat::Vehicle,
  mtnk_slot, …)` / `…Infantry, gi_slot, …` **directly** (`main.cpp:4756, 4782,
  4837`) — it **never calls `can_start_unit`**. It draws the sidebar with the
  Structure queue only (`draw_sidebar(..., q.structure(), …)`), and the
  unit-tab greyed-cameo path (`can_start_unit`) lives in `run_interactive`, not
  the demo. **Therefore extending unit-prereq DATA is inert for `prod_hash` by
  construction** — `prod_hash` folds world units + queue state, and no unit's
  buildable decision is ever consulted in the demo. This is the strongest
  possible byte-identity guarantee for the at-risk gate.
- **The AI base-planner DOES consult the predicate.** `World::step_ai_houses`
  gates each AI structure on `can_start(rec.queues.state(cat), d.type,
  cfg.roster, buildings_, h, cfg.groups, conyard)` (`world.cpp:1372`) — the
  SAME building predicate the player uses (`ai-base.md` §6). `--demo-ai-base`
  (house 1 Soviet) is therefore at-risk **only through building prereqs**,
  which CB-5 does NOT change (buildings already full-parse; their rulesmd
  `Prerequisite=` is untouched). Adding the Allied `GATECH` to the roster does
  not enter the Soviet AI catalog (`make_ai_config` names `NA*` ids
  explicitly, `main.cpp:1609+`), so no new Soviet AI node appears →
  `ai_base_hash` unchanged.
- **The AI team-planner DOES consult the unit predicate.** `step_ai_teams`
  gates unit production on `can_start_unit(rec.queues.state(cat), want,
  tc.unit_roster, buildings_, rec.cfg.roster, h, rec.cfg.groups, factory)`
  (`world.cpp:1912`) over the **Soviet AI unit roster**
  (`load_soviet_unit_roster`, `main.cpp:1221+`:
  `{E2,NAHAND},{HTNK,NAWEAP},{APOC,NAWEAP},{HTK,NAWEAP}`). **`APOC` carries a
  documented proxy prereq** — the table comment (`main.cpp:1216`) records that
  APOC's real prereq needs the tech path (`Prerequisite=NAWEAP,NATECH`,
  `production.md` §1.6) but is **proxied to `NAWEAP` pre-map [RE-FALLBACK]**.
  If CB-5 switched the Soviet AI roster to full-parse, `APOC` would gain a
  `NATECH` requirement that no roster building satisfies → `APOC` becomes
  UNBUILDABLE for the AI → **`ai_team_hash` would move (a defect).** So CB-5
  **deliberately keeps the Soviet AI unit roster on its existing proxy
  prereqs** (§8) — the new full-parse applies to the player roster + the new
  advanced unit only. This is exactly the "the AI keeps its existing prereq
  path unchanged" branch the brief anticipates.
- **The interactive sidebar greys BUILDING cameos only, not unit cameos.** The
  disabled-shade `can_start` call is guarded `!unit_tab && …` (`main.cpp:2595–
  2597`); the unit tabs (2/3) **skip the cosmetic disabled shade** entirely
  (`main.cpp:2584` comment). Starting a unit build DOES go through
  `can_start_unit` (`main.cpp:2956`), but the greyed/available VISUAL for units
  does not yet exist. **Extending the disabled shade to unit tabs is a genuine
  (small, render-only) CB-5 deliverable (item 4).** It is not hashed by any
  gate (no gate renders an interactive unit tab).
- **No tech building in the roster today.** The 18-building roster (`kRoster`,
  `main.cpp:1357–1361`: `GACNST/GAPOWR/GAPILE/GAWEAP/NA…/GAREFN/NAREFN/GAPILL/
  GATOW/NALASR/NABNKR/TESLA/NAFLAK/NAWALL/NARADR`) contains **no `GATECH`**.
  The `TECH` group is present in the DATA (`groups`) but has no member in the
  roster, so no unit's `TECH` requirement can ever be satisfied and no unit
  requires `TECH`. CB-5 adds `GATECH` (content-only roster growth, the
  `--demo-base roster=` precedent) + the `SREF` advanced unit that requires it.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Buildability predicate | **REUSE the existing `prerequisites_met`/`can_start`/`can_start_unit` verbatim** — they already implement the RA2 AND-semantics with group tokens (`production.cpp`, `docs/re/production.md` §1.6 / `construction.md` §10). CB-5 adds NO new predicate and NO new pure prereq module |
| Unit `Prerequisite=` parsing | **Switch the PLAYER unit roster (`kUnits`) from the hardcoded `{e.prereq}` single token to `d.prerequisite = rules.prerequisite(id)`** — the full rulesmd token list, exactly as buildings already do (`main.cpp:1435`). The `UnitRosterEnt.prereq` field is retired for the player table. Verified byte-identical: `E1/GGI=GAPILE`, `MTNK/FV=GAWEAP` (`production.md` §1.6) — and NO gate consults player-unit `can_start_unit` (§8) |
| Soviet AI unit roster | **UNCHANGED** — keeps its documented proxy prereqs (`APOC→NAWEAP` proxy, `main.cpp:1216` [RE-FALLBACK]) so `ai_team_hash`/`ai_base_hash` stay bit-identical. A future phase that adds the Soviet Battle Lab (`NATECH`) to the AI catalog would switch it (carry-forward, §Out of scope) |
| Tech building | **Add the Allied Battle Lab `GATECH` to the building roster** (`kRoster` grows 18→19). It is the `TECH`-group member; art-guarded (dropped if its SHP/cameo is absent, the `GATOW`/`NABNKR` precedent). Content-only roster growth: `--demo-base`'s `roster=` witness grows (hash unaffected — the documented precedent, README "content-only growth") |
| Advanced unit | **Add the Prism Tank `SREF` to the player unit roster** (`kUnits` grows 4→5, appended so 0..3 indices are stable), `Prerequisite=GAWEAP,GATECH` via full-parse, `TechLevel>0`. Art-guarded (dropped if art absent → the demo falls back to a synthetic advanced-unit def so the sim logic still runs, §6) |
| Sidebar unit availability | **Extend the existing disabled-shade to unit tabs** (`main.cpp:2582+`): compute `can_start_unit(...)` for a unit cameo and draw the SAME greyed shade the building tabs use when unmet. Reuse the EXISTING cameo chrome — no new font/UI systems. Render-only, art-guarded, not hashed by any gate |
| Grant of the tech tier | The demo **places** `GATECH` through the normal building path (like every base building) to flip `SREF`'s buildability — proving the real prereq chain, not a direct grant. The Battle-Lab-unlocks-SW / building-upgrade wiring stays a carry-forward |
| Verification | **New deterministic gate `--demo-techtree`** (the 13th) + `techtree_hash` (FNV over the buildable-set state across a scripted prereq-progression + world/unit/building state at fixed checkpoints), two-run compute-and-self-compare (NO pinned literal in code); **wedge guards (§6): `SREF` UNBUILDABLE before `GATECH` AND BUILDABLE after, else exit 1** |
| Byte-identity | **Strategy (a)+(superset)**: the predicate is reused unchanged; player-unit full-parse resolves identically AND is never consulted by a gate (`--demo-prod` bypasses `can_start_unit`); building prereqs are untouched; the Soviet AI unit roster is untouched; `GATECH`/`SREF` are appended (content-only) and only the new gate exercises the tech-gating. `world_hash`/`prod_hash`/`ai_*_hash` unchanged for existing demos; new tech-tree state hashed only by `techtree_hash`. Re-verify all 12 gates at the first buildability-touching commit AND at finalize (§8) |

## §1 What the Ghidra hunt must recover (→ `docs/re/techtree.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, `gamemd.exe` YR 1.001
Ghidra project at `D:\ra2-re`, headless `analyzeHeadless.bat`,
`JAVA_HOME="C:\Program Files\Java\jdk-21"`) plus the real rulesmd, producing
`docs/re/techtree.md` — the calibration authority BEFORE any code, with a
located-functions table, a "deviations we take" section, and each item landing
CONFIRMED / PROBABLE / [RE-FALLBACK] with function address + decompile
reasoning. **This hunt is largely CONFIRMATORY**: the prereq mechanism
(`FUN_00505180`) and the `[General] Prerequisite<Group>` membership are ALREADY
recovered and CONFIRMED in `docs/re/production.md` §1.6 and
`docs/re/construction.md` §10; the hunt re-anchors them for this phase and
fills the tech-tier specifics (Battle-Lab id, advanced-unit full prereq).
**Ares/YRpp addresses are UNTRUSTED prose hints only** — `combat.md`'s finding
stands (Ares' hardcoded YR-1.001 addresses do not transfer to this binary), so
content-anchor everything. Anchor on: the INI-key strings the binary loads
(`"Prerequisite"`, `"PrerequisitePower"`/`"…Tech"`/`…`, `"TechLevel"`); the
prereq-group `[General]` list reader (a `RulesClass::LoadFromINI` fragment, the
`combat.md` §3 precedent); and `FUN_00505180` (the token-ANDing predicate).
**Do NOT guess the token set/membership** — the `production.md`/`construction.md`
recoveries + `ra2extract` are the source. Five items:

1. **`Prerequisite=` syntax + the prereq-group tokens and their member sets +
   the satisfaction rule** (`FUN_00505180`, `construction.md` §10 CONFIRMED):
   every listed token ANDed; a plain token = own ≥1 live, placed building of
   that id; a group token (`POWER/FACTORY/BARRACKS/RADAR/TECH/PROC`) = own ≥1
   member of the corresponding `[General] Prerequisite<Group>` list. RE-anchor
   the token→group binding for this phase; confirm the engine's existing
   `prerequisites_met` matches the AND-semantics (it does). **Expected:
   CONFIRMED** (already `FUN_00505180`, member lists in `production.md` §1.6).
2. **How a building declares its group membership** — membership is a
   `[General] Prerequisite<Group>` LIST of building ids (NOT a per-BuildingType
   category flag), parsed to six RulesClass BuildingType vectors
   (`construction.md` §10 @+0x8C8…+0xA34). Confirm the engine's
   `BuildingAssets::groups`/`Rules::prerequisite_group` reads exactly these
   keys (it does). **Expected: CONFIRMED.**
3. **The buildability/`Can_Build` predicate consuming owned buildings** — how
   `FUN_00505180` (and the sidebar-side buildable logic, incl.
   `PrerequisiteProcAlternate=SMIN`) walks the house's owned-building-type list
   per token; the exact "buildable now" boolean; confirm no per-frame RNG.
   Confirm the engine's `can_start`/`can_start_unit` (`tech_level>0` +
   factory-alive + `prerequisites_met`) is faithful. **Expected: CONFIRMED**
   (`FUN_00505180` + loader).
4. **The Battle Lab (`GATECH` Allied / `NATECH` Soviet) + the advanced unit
   requiring `TECH`** — from `ra2extract`: the exact **`[GATECH] Prerequisite=`
   and `TechLevel=`**; the **`[SREF] Prerequisite=` (expected `GAWEAP,GATECH`)
   and `TechLevel=`**; confirm the `TECH` group members
   (`GATECH,NATECH,YATECH`). **Resolve the brief's `GACTEK`-vs-recovered-
   `GATECH` id discrepancy here.** Verify `GATECH`/`SREF` SHP + cameo art are
   present in the real mixes (a demo-art [RE-FALLBACK] with a synthetic
   fallback if absent — the sim is art-independent). **Expected: CONFIRMED
   (rulesmd values) + art-presence check.**
5. **Determinism + byte-identity** — confirm the buildability change does not
   alter what the existing demos build: `FUN_00505180` is pure over owned
   buildings (no RNG); the player-unit full-parse must resolve
   `E1/GGI=GAPILE`, `MTNK/FV=GAWEAP` identically to the hardcoded tokens
   (quote the real `[E1]/[GGI]/[MTNK]/[FV] Prerequisite=`); confirm the Soviet
   AI roster's `APOC` real prereq is `NAWEAP,NATECH` (justifying keeping the
   proxy to protect `ai_team_hash`). **Expected: CONFIRMED (no RNG) + the
   per-unit value quotes.**

Calibration discipline (unchanged from every prior phase): every engine
constant cites this doc. **All numeric/id semantics in this spec are pre-hunt
expectations except where cited to `production.md`/`construction.md` as already
recovered.** A controller **RE-GATE** amendment step precedes implementation —
the hunt's findings amend the plan and are committed BEFORE dependent dispatch,
with `[RE-GATE APPLIED]` markers threaded through every dependent task (the
CB-1/2/3/4 precedent). **[RE-FALLBACK]**: anything the hunt cannot pin becomes
a plan-amendment gate (present options to the human) — never an uncalibrated
guess. The most likely [RE-FALLBACK]s are the **Battle-Lab id/`Prerequisite=`
specifics (item 4)** and the **`GATECH`/`SREF` art presence**; the fallbacks
are documented (use the `production.md` §1.6 `GATECH,NATECH,YATECH` /
`SREF=GAWEAP,GATECH` recovery; synthetic advanced-unit def if art regresses).

## §2 Architecture & recovered data model

Consistent with every prior phase: a thin extension of the EXISTING pure
production module + roster wiring + a deterministic demo in `main.cpp`. **No
new pure engine module is required** — the buildability logic already lives in
the pure, already-registered `engine/production.{hpp,cpp}`. ~7 tasks:

- **Task 1**: Ghidra hunt → `docs/re/techtree.md` (RE-GATE before
  implementation; §1 above). Includes `ra2extract` of the real
  `[General] PrerequisiteTech`, `[GATECH]`, `[SREF]`, and the four existing
  player-unit `Prerequisite=` values, + art-verifying `GATECH`/`SREF` presence.
- **Task 2**: **Unit full-`Prerequisite=` parsing** — replace the player
  roster's `d.prerequisite = {e.prereq}` with `rules.prerequisite(id)`
  (`main.cpp` `load_unit_roster_table`/`kUnits`); retire the `UnitRosterEnt.prereq`
  field for the player table. Keep `load_soviet_unit_roster` UNCHANGED
  (documented proxy). Pure doctests: `prerequisites_met` over a multi-token
  unit prereq incl. a `TECH` group token (the function exists; add coverage for
  the group + multi-token AND path). **12-gate re-verify at this first
  buildability-touching commit** (§8).
- **Task 3**: **Tech building + advanced unit** — append `GATECH` to `kRoster`
  (art-guarded) and `SREF` to `kUnits` (art-guarded, synthetic fallback).
  Confirm `GATECH` stays out of the Soviet AI catalog. Content-only roster
  growth: update the `--demo-base roster=` witness expectation (hash
  unaffected). World/roster tests: `SREF` is UNBUILDABLE with `{GAWEAP}` owned,
  BUILDABLE with `{GAWEAP,GATECH}` owned.
- **Task 4**: **Sidebar unit availability** — extend the disabled-shade in
  `draw_sidebar` (`main.cpp:2582+`) to unit tabs via `can_start_unit`, reusing
  the existing greyed-cameo chrome (render-only, art-guarded).
- **Task 5**: **`--demo-techtree` gate** (the 13th) + `techtree_hash` +
  witnesses + wedge guards (§6).
- **Task 6**: finalize — suites both modes, 12 existing gates byte-identical,
  arm64 NDK `nm`-verify of the symbols, README (CB-5 section + 13th gate row +
  deviations note citing `docs/re/techtree.md`).

**Decomposition tripwire**: the phase is small (reuse the predicate + two
roster entries + a demo). IF Task 4 (unit-tab sidebar availability) fans out —
a full per-state cameo frame path, hit-test plumbing — it splits at the natural
seam: **(A) the prereq DATA + tech tier + gate** (Tasks 1–3, 5–6 with a
headless-first `--demo-techtree` that asserts buildability via `can_start_unit`
directly, no unit-tab sidebar shade) then **(B) the unit-tab sidebar
availability** (Task 4 + a rendered witness). Default is the single ~7-task
phase.

### Data model

Field shapes below are the approved design; the Task 1 hunt CONFIRMS the ids
and token values (RE-GATE), and code/tests are written against the amended form.

- **`UnitTypeDef`** (`unit_type.hpp`) — **UNCHANGED shape.** It already carries
  `std::vector<std::string> prerequisite` and `int tech_level`. CB-5 only
  changes what `main.cpp` STORES into `prerequisite` (the full rulesmd list,
  not a single token) for the player roster.
- **`UnitRosterEnt`** (`main.cpp:1123`) — the `const char* prereq` field is
  retired for the player table (`kUnits`); entries carry only `id` + `vehicle`,
  and `load_unit_roster_table` fills `d.prerequisite = rules.prerequisite(id)`.
  The Soviet table (`kSoviet`) KEEPS its `prereq` proxy field (unchanged path).
  [Simplest diff: keep the struct, load full-parse for the player caller, keep
  the hardcoded token for the Soviet caller — a per-caller flag or two loader
  variants; the plan picks the least-churn form.]
- **`BuildingTypeDef`** — **UNCHANGED.** Already carries `prerequisite`
  (full-parsed) + `tech_level`. `GATECH` is just a new roster entry.
- **`ProductionState`/`ProductionQueues`/`prerequisites_met`/`can_start`/
  `can_start_unit`** — **UNCHANGED.** Reused verbatim.
- **`World`/`Unit`/`world_hash`** — **UNCHANGED.** No new fields; the tech-tree
  state is derived (the buildable-set is computed from owned buildings on
  demand), hashed only by the new `techtree_hash`.
- **No new pure module.** `engine/production.{hpp,cpp}` (already in BOTH
  `cmake/ra2_sources.cmake` AND `CMakeLists.txt`) is the home of the reused
  logic. Every recovered constant cites `docs/re/techtree.md`.

## §3 Full `Prerequisite=` parsing for units

The one genuine data change — bring units onto the same full-parse path
buildings already use:

1. **Load**: in `load_unit_roster_table` (`main.cpp:1143+`), for the PLAYER
   table set `d.prerequisite = rules.prerequisite(e.id)` (splitting the real
   `[<id>] Prerequisite=` into the token list) instead of `{e.prereq}`. Tokens
   may be plain building ids OR group tokens (`BARRACKS`/`FACTORY`/`TECH`/…),
   resolved by the EXISTING `prerequisites_met` against `groups`.
2. **Byte-identity anchor**: the hunt (§1 item 5) confirms the real
   `[E1]/[GGI]/[MTNK]/[FV] Prerequisite=` values; `production.md` §1.6 already
   records `E1/GGI=GAPILE`, `MTNK/FV=GAWEAP`. If any real value is a group
   token or multi-token that resolves to the SAME decision given the current
   owned buildings, the buildable result is unchanged; if a real value would
   differ, it still cannot move a gate because **no gate consults player-unit
   `can_start_unit`** (`--demo-prod` bypasses it, §8) — but the design targets
   an identical resolution for interactive correctness regardless.
3. **Soviet AI roster untouched**: `load_soviet_unit_roster` keeps its
   hardcoded proxy prereqs (`APOC→NAWEAP`) — the `ai_team_hash` protection
   (§ Current-engine reality, §8).
4. **Determinism**: `prerequisites_met` is pure iteration over owned buildings;
   no RNG (§1 item 5).

## §4 The tech building + the advanced unit tier

1. **Battle Lab `GATECH`**: append to `kRoster` (`main.cpp:1357`). It loads
   through the identical `load_building_assets` path every building uses
   (`Prerequisite=`/`TechLevel=`/`Cost=`/art/cameo all from rulesmd), and is a
   member of the `TECH` group (already in `groups`). Art-guarded: if its SHP is
   absent it is dropped like `GATOW`/`NABNKR` (the demo then uses the synthetic
   path, §6). Adding it is **content-only roster growth** — no existing demo
   places or builds it; it does not enter the Soviet AI catalog (Allied `GA*`).
2. **Prism Tank `SREF`**: append to the player `kUnits` (`main.cpp:1204`) with
   `Prerequisite=GAWEAP,GATECH` (full-parse) + `TechLevel>0`. Appended at index
   4 so the existing MTNK/E1/GGI/FV indices (0..3) are stable → `--demo-prod`'s
   `mtnk_slot`/`gi_slot` lookups and `prod_hash` are unaffected. Art-guarded;
   if `SREF` art is absent the demo builds a synthetic advanced-unit def
   (`Prerequisite=GAWEAP,GATECH`, a placeholder combat stat) so the buildability
   progression is proven regardless of art.
3. **The tier gate**: with `{GACNST,GAPOWR,GAPILE,GAWEAP}` owned, `MTNK` is
   buildable but `SREF` is NOT (`GATECH` unmet); placing `GATECH` flips `SREF`
   to buildable. This IS the tech-tree progression the demo proves (§6).

## §5 Sidebar availability state for unit tabs

Reuses the EXISTING greyed-cameo chrome — no new font/UI systems:

- **Today**: building tabs draw a disabled shade over a cameo whose
  `can_start` is false (`main.cpp:2595–2597`), guarded `!unit_tab`; unit tabs
  skip it (`main.cpp:2584`). Starting a unit already checks `can_start_unit`
  (`main.cpp:2956`), so the LOGIC exists — only the VISUAL is missing.
- **CB-5**: in the unit-tab branch of the cameo loop, compute
  `can_start_unit(queues.state(cat), type, ur->defs, world.buildings(), ba.roster,
  0, ba.groups, factory_alive)` and draw the SAME shade when false. `SREF`'s
  cameo is greyed until `GATECH` is placed, then becomes available — the
  visible tech-tree feedback.
- **Guarded + not hashed**: art-guarded (a missing cameo just isn't drawn); no
  gate renders an interactive unit tab, so this is render-only and byte-identity
  neutral (§8).

## §6 The `--demo-techtree` gate (the 13th), `techtree_hash` & witnesses

**Script** (headless or rendered — bit-identical): on the temperate reference
map, build a fresh Allied base for house 0 via the existing `spawn_base` +
`place_player_building` path — `GACNST`, `GAPOWR`, `GAPILE`, `GAWEAP` (the
`--demo-prod` base-build precedent) — resolving `SREF` (advanced unit) and
`GATECH` (Battle Lab) from the roster (synthetic fallback if art absent, §4):

1. **BASELINE**: with `{GACNST,GAPOWR,GAPILE,GAWEAP}` owned, assert
   `can_start_unit(SREF)` is **FALSE** (witness `[techtree] SREF unbuildable
   (GATECH unmet) can_start=0`) AND `can_start_unit(MTNK)` is **TRUE** (the
   tier-2 control — proves the base is otherwise productive). Fold the
   buildable-set into the hash.
2. **UNLOCK**: place `GATECH` (Battle Lab) through the normal building path;
   step until it is Idle (fully built). Witness `[techtree] placed GATECH
   cell=(x,y) TECH group satisfied`.
3. **BUILDABLE**: assert `can_start_unit(SREF)` is now **TRUE** (witness
   `[techtree] SREF buildable (GATECH met) can_start=1`). Fold the flipped
   buildable-set into the hash. (Optionally `start_unit(SREF)` and step it to
   exit, confirming an advanced unit actually produces — the `--demo-prod`
   exit-cell precedent.)
4. **REGRESS (optional wedge)**: none required; the two wedges (unbuildable
   before, buildable after) are the core proof.

**`techtree_hash`**: FNV-1a 64 (the established `fnv_mix` helper) folded at
fixed checkpoints (baseline, post-unlock, end) over: every unit's/building's
`(cell_x, cell_y, alive, hp/state)` PLUS the **buildable-set snapshot** — the
sorted list of `(roster id, can_start/can_start_unit boolean)` for a fixed
probe set `{MTNK, SREF, GATECH, GAWEAP}` at each checkpoint. This is exactly
the tech-tree state the phase adds. Two-run compute-and-self-compare
(`deterministic=yes`), **NO pinned literal in code** (the `--demo-naval`/
`--demo-superweapon` precedent); README records the stable value in the gate
table once landed.

**Wedge guards** (exit 1, so the gate can never pass vacuously): `SREF` was
`can_start_unit==false` at baseline AND `can_start_unit==true` after `GATECH`
is placed — BOTH must hold, else exit 1. Witness log + hash are pure sim state
→ rendered and headless runs byte-compare identical.

## §7 Testing strategy

- **Pure doctests** (host-only, hand-built inputs, no install):
  `tests/test_production.cpp` (extend) — `prerequisites_met` over a multi-token
  unit prereq `{GAWEAP, TECH}` where `TECH` is a group: FALSE with only a War
  Factory owned, TRUE once a `TECH`-group member (`GATECH`) is Idle-owned;
  `can_start_unit` gates on `tech_level>0` + factory-alive + the group-token
  AND; a plain-token unit prereq (`{GAPILE}`) resolves exactly as before (the
  E1/GGI byte-identity anchor).
- **World-level tests** (the existing `test_world` pattern): a house owning
  `{GACNST,GAPOWR,GAPILE,GAWEAP}` has `SREF` unbuildable and `MTNK` buildable;
  adding an Idle `GATECH` flips `SREF` to buildable; removing/killing `GATECH`
  flips it back; the Soviet AI roster's `APOC` resolves identically to the
  pre-CB-5 proxy (regression guard for `ai_team_hash`).
- **Integration**: the `--demo-techtree` gate (§6) with wedge guards
  (unbuildable before AND buildable after) + two-run `techtree_hash`
  self-compare (no pinned literal); rendered ≡ headless.
- **Regression**: all 12 existing gates byte-identical (values in Depends-on),
  re-run at the first buildability-touching commit (Task 2) AND at finalize
  (Task 6); the `--demo-base roster=` witness growth (18→19) is an expected
  content-only change with the gated hash unchanged; full suite both modes;
  arm64 NDK build with the new symbols `nm`-verified. Suite stays scoped/fast
  (the shared-fixture discipline).

## §8 Byte-identity discipline

The **twelve existing permanent gates MUST stay bit-identical** (values in
Depends-on). This phase touches BUILDABILITY, so `--demo-prod`,
`--demo-ai-base`, and `--demo-ai-team` are the at-risk gates — the discipline,
justified from reading the code (§ Current-engine reality):

- **`--demo-prod` cannot move**: it starts units via `q.start_unit(...)`
  **directly** (`main.cpp:4756/4782/4837`) and **never calls `can_start_unit`**;
  it draws only the Structure-queue sidebar. Extending unit-prereq DATA is
  therefore inert for `prod_hash` by construction — the buildable decision it
  changes is never consulted in the demo. This is the strongest guarantee for
  the phase's most-exposed gate.
- **`--demo-ai-base` cannot move**: the AI building predicate `can_start`
  (`world.cpp:1372`) reads BUILDING prereqs, which CB-5 does NOT change
  (buildings already full-parse; their rulesmd `Prerequisite=` is untouched).
  The Allied `GATECH` added to the roster does not enter the Soviet AI catalog
  (`make_ai_config` names `NA*` ids), so no new AI node appears.
- **`--demo-ai-team` cannot move**: the AI unit predicate `can_start_unit`
  (`world.cpp:1912`) reads the **Soviet AI unit roster**, which CB-5 keeps
  UNCHANGED (the `APOC→NAWEAP` proxy is retained; switching it to full-parse
  would make `APOC` unbuildable via an unsatisfiable `NATECH` and MOVE the
  hash — a defect, deliberately avoided).
- **`--demo-base`/`--demo-econ`**: their `can_start` checks (GAWEAP blocked by
  `PROC`) read building prereqs (untouched). Adding `GATECH` grows the
  `roster=` WITNESS COUNT only (18→19), a documented content-only change; the
  gated world-state hash is unaffected (no `GATECH` instance is built/placed in
  those demos; `world_hash` iterates building INSTANCES, not roster entries).
- **`world_hash` untouched (strategy (a))**: no `Unit`/`World` field is added;
  the tech-tree buildable-set is derived on demand and hashed ONLY by
  `techtree_hash` in the new gate.
- **The predicate code is reused unchanged**: `prerequisites_met`/`can_start`/
  `can_start_unit` bodies are byte-for-byte identical — CB-5 changes only the
  DATA fed to them (player-unit prereq tokens) and the ROSTER contents, both
  proven above to be inert for every existing gate.
- **The unit-tab sidebar shade is render-only**: no gate renders an interactive
  unit tab, so it cannot perturb any hash.
- **The 12-gate re-verify runs at the first buildability-touching commit
  (Task 2) AND at finalize (Task 6)** — isolating any regression to the commit
  that introduced it (the CB-1/2/3/4 discipline).

## Out of scope / divergences (explicit)

- **Engineer / spy CAPTURE** (building ownership transfer, infiltration
  effects) — **DEFERRED; the biggest carry-forward and the natural next step.**
  A distinct ownership-transfer mechanic (a unit changes a building's `house`,
  with the infiltration side-effects), unrelated to the prereq/buildability
  gating CB-5 completes. Note prominently: with the tech-tree done, capture is
  the next major content mechanic.
- **Building UPGRADES** (`PowersUp`/`Upgrades=`, the war-factory/etc. upgrade
  slots) — deferred.
- **Neutral tech-structure capture** (oil derrick, hospital, machine shop) —
  deferred (part of capture).
- **`BuildLimit` / one-of / `Owner`-side restrictions beyond prereqs** —
  recover-and-document in the hunt; implement only the prereq gating.
  `BuildLimit` is a small add if the hunt shows it trivial; else deferred.
- **`Prerequisite.Negative` (can't-build-if-you-have-X)** — recover-and-
  document; deferred unless the hunt shows it trivial.
- **Soviet AI unit tech-tier via full-parse** — the Soviet AI roster keeps its
  `APOC→NAWEAP` proxy (byte-identity); switching it to full-parse (which needs
  the Soviet Battle Lab `NATECH` in the AI catalog) is a carry-forward.
- **Battle-Lab-unlocks-superweapon / Nuke-Silo-grants-SW wiring** — CB-4 grants
  SWs directly; wiring the tech tree to SW/unit unlocks beyond the direct
  buildability gate is a carry-forward.
- **Multi-side simultaneous tech trees / Yuri (`YA*`) roster** — CB-5 exercises
  the Allied player tech tree (the loaded roster); the Soviet/Yuri tech trees
  are data-present (groups include `NATECH`/`YATECH`) but only the Allied path
  is demonstrated.

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real rulesmd /
  prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE precede all
  dependent tasks; every recovered constant cites `docs/re/techtree.md`;
  [RE-FALLBACK] items become plan-amendment gates, never guesses.
- TWELVE byte-identity gates stay green (§8); `--demo-techtree` becomes the
  13th permanent gate.
- No new pure module is required, but if the plan factors any helper out it is
  registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the
  install. Long rendered demos auto-background on short timeouts; NEVER pipe a
  native exe through `Select-Object -First N` (stops draining stdout → false
  hang). The plan's implementer briefs must order FOREGROUND synchronous runs
  with generous timeouts (the recorded prior-subagent stall failure mode).
- No game assets / GPL source / Ghidra output committed; recovered numeric/id
  constants ARE committable (the `[General] Prerequisite<Group>` lists +
  `SREF=GAWEAP,GATECH` already are, via `production.md` §1.6).
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`

## Testing summary

| Layer | Tests |
|---|---|
| prereq predicate (pure) | `prerequisites_met` over a multi-token + `TECH`-group unit prereq (FALSE without a TECH member, TRUE with `GATECH` Idle-owned); `can_start_unit` gates on `tech_level>0` + factory-alive + group-token AND; a plain-token prereq (`{GAPILE}`) resolves exactly as pre-CB-5 (the E1/GGI byte-identity anchor) |
| World integration | `{GACNST,GAPOWR,GAPILE,GAWEAP}` → `SREF` unbuildable, `MTNK` buildable; adding Idle `GATECH` flips `SREF` buildable; killing `GATECH` flips it back; Soviet AI `APOC` resolves identically to the pre-CB-5 proxy (ai_team_hash regression guard) |
| integration | `--demo-techtree` witnesses (baseline unbuildable → unlock → buildable) + `techtree_hash` two-run self-compare (no pinned literal) + wedge guards (unbuildable before AND buildable after else exit 1); rendered ≡ headless |
| regression | 12 existing gates byte-identical (§8), re-verified at the first buildability-touching commit and at finalize; `--demo-base roster=` content-only growth (hash unchanged); full suite both modes; arm64 `nm`-verify of the symbols |
