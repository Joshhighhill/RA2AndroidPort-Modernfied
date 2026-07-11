# Unit Production Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The War Factory (GAWEAP) and Barracks (GAPILE) actually produce units — the sidebar Defense/Infantry/Vehicle tabs come alive, cameo clicks feed real concurrent per-category production queues, and finished units drive out of a recovered factory exit cell, path to a settable rally point, and become controllable.

**Architecture:** Task 1 recovers ground truth from gamemd.exe into `docs/re/production.md`; then the change lands additively, protecting all four byte-identity gates. ra2assets `rules.cpp` gains classification accessors; `engine/production` decouples the 54-step charge math into a scalar entry point and wraps it in a 3-queue `ProductionQueues` container (the existing `ProductionState` IS the BuildingType queue, untouched); a new pure `engine/factory` module recovers the exit cell, primary-factory selection, and rally; World spawns finished units at the exit and routes them to the rally; `main.cpp` brings the sidebar tabs alive, adds an Allied defense + unit roster, and wires a new `--demo-prod` gate. Spec: `docs/superpowers/specs/2026-07-12-production-design.md`.

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle), Ghidra headless (JAVA_HOME=`C:\Program Files\Java\jdk-21`), SDL2 (wiring layer only).

## Global Constraints

- **Calibration**: every gameplay/render constant cites `docs/re/production.md` (Task 1's output) or an existing `docs/re/*.md`. Values the hunt cannot pin are **[RE-FALLBACK]** plan-amendment gates — STOP, present options to the human, amend the plan, commit the amendment BEFORE any dependent dispatch. Never guess silently.
- **[RE-GATE] protocol**: Tasks 2–9 are written with best-known pre-hunt values, each consuming point marked `[RE-GATE APPLIED: production.md §N]` with its fallback inline. After Task 1, the controller diffs `docs/re/production.md` against every `[RE-GATE APPLIED]` block, amends the plan, and commits the amendment before dispatching the task that uses it.
- **Byte-identity hard gates (permanent)**: `--demo-combat` ends `deterministic=yes logs=78 lines`; `--demo-tank` ends `deterministic=yes logs=144 lines`; `--demo-base` prints `hash=5273b5e71ca009e1`; `--demo-econ` prints `econ_hash=7fed96cb98b68017` and is two-run byte-identical. Any change to their output is a defect in YOUR change.
- **Byte-identity hazards called out inline**: `production.cpp`'s `tick`/`cancel`/`placed` stay untouched; the BuildingType queue math stays bit-identical; the four gates stay green; `Building`'s new fields (`primary`, `rally_x`, `rally_y`, `has_rally`) are excluded from `world_hash`; adding GAPILL/GATOW to the roster must not alter `--demo-base` (`spawn_base` places nothing new — verify); `prod_hash` is a NEW function, `world_hash` itself unchanged.
- Run executables from **PowerShell**, never Git Bash. Run every build/test/demo **FOREGROUND, synchronously, blocking** — do NOT background test runs and wait; a prior phase lost subagents to that.
- `RA2_INSTALL_DIR` default `D:\Red Alert 2`; install-dependent tests skip cleanly (doctest `MESSAGE` + `return`) without it.
- No game assets, gamemd.exe, or Ghidra project committed. GPL sources (XCC/OpenRA/YRpp at `D:\ra2-re`) read off-repo, prose conclusions only; YRpp/Ares hardcoded addresses untrusted — content-anchored hunts only. **Recovered numeric constants ARE committable.**
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` (engine list) and `CMakeLists.txt` (test target); the Android jni build shares `cmake/ra2_sources.cmake`.
- Repo on master, solo, no remote. Commits end `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- Scratch files to the session scratchpad, never the repo.
- Desktop build: `cmake --build D:\ra2-android\build --target ra2assets_tests` then run `D:\ra2-android\build\ra2assets_tests.exe`. Engine: target `ra2engine`, exe `D:\ra2-android\build\ra2engine.exe`. doctest filters are name-based `--test-case="prefix:*"` (no suite globbing).

### Player side & scope (spec §Approved scope decisions)

- Player is **Allied** (GA\* side). Enemy Soviet base exists but does NOT produce this phase (player-only production; attack-wave AI is a future phase).
- **Both unit factories** produce: Infantry (GAPILE → GI etc.) and Vehicle (GAWEAP → Grizzly etc.), **plus Defense structures** (tab 1, placed like buildings).

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/production.md` (create, Task 1) | Calibration authority: queue model, primary/MultipleFactory, exit cell, rally, unit build economy, per-unit prereqs |
| `include/ra2assets/rules.hpp` + `src/rules.cpp` (modify, Task 2) | `build_category` / `is_infantry` / `is_vehicle` classification accessors |
| `engine/unit_type.hpp` (create, Task 3) | `UnitTypeDef` roster struct (production-relevant unit fields) |
| `engine/production.hpp` + `production.cpp` (modify, Task 3) | scalar `start`, `can_start_unit`, `ProductionQueues` 3-queue container |
| `engine/factory.hpp` + `factory.cpp` (create, Task 4) | `exit_cell` / `primary_factory` / `set_primary`; `Building` rally/primary fields |
| `engine/world.hpp` + `world.cpp` (modify, Task 5) | ready-to-exit spawn, rally routing, `set_rally`/`set_primary`, factory-died cancel |
| `engine/main.cpp` (modify, Tasks 6–8) | sidebar tabs alive, defense+unit roster+assets, exit handoff, `--demo-prod` |
| `engine/input.cpp`/`input.hpp` (modify, Task 7) | tab-click / cameo-click-in-tab / rally right-click / primary-set trigger |
| `tests/test_rules.cpp`, `tests/test_production.cpp`, `tests/test_factory.cpp` (create), `tests/test_world.cpp` | per-layer pins |

---

### Task 1: Ghidra hunt → docs/re/production.md

**Files:**
- Create: `docs/re/production.md`
- Scratch: hunt scripts under the session scratchpad (never committed)

**Interfaces:**
- Consumes: gamemd.exe YR 1.001 in the Ghidra project at `D:\ra2-re` (already analyzed in prior hunts; `analyzeHeadless.bat` + `-process` + `-postScript`, `JAVA_HOME="C:\Program Files\Java\jdk-21"`). Known anchors from prior hunts (docs/re/construction.md §located-functions): FactoryClass `FUN_004C9B90` (Set/Start, queue push), `FUN_004C9DC0` (rate update, `step_ticks`), `FUN_004C9A40` (AI per-step), `FUN_004C9F10` (Abandon/refund); `FUN_006F3D00` Time_To_Build (already contains the MultipleFactory loop `if MultipleFactory>0: repeat (n-1)x t = ftol(t x MultipleFactory)` and `FUN_00500730` house factory-count-per-product-class); rules `[General]` loader `FUN_0066CF70` (`MultipleFactory`→+0x57C). The binary loads INI keys by literal name — `"MultipleFactory"`, `"Factory"`, `"Weapon"` (VehicleType `Factory=WeaponsFactory`), `"Naval"`, `"WeaponsFactory"`, `"BarracksClass"`, `"DockUnload"`, `"Rally"`/`"SetRallyPoint"`, `"IsBaseDefense"`/`"BaseNormal"` are content anchors.
- Produces: `docs/re/production.md` §1 items 1–6, each with per-finding confidence (CONFIRMED / PROBABLE / [RE-FALLBACK]) and a located-functions table.

**Questions the doc MUST answer** (spec §1 items 1–6):

1. **Queue model / tab→queue mapping**: how many concurrent `FactoryClass` queues exist and how the 4 sidebar tabs (Buildings/Defenses/Infantry/Vehicles) map to them; whether Structures and Defenses share one BuildingType queue (expected: yes — you cannot build a power plant and a pillbox at once); the field that classifies a producible into a tab/queue (BuildingType-vs-UnitType + the base-defense flag + the infantry/vehicle split — locate `IsBaseDefense`/`BaseNormal`, and the `[VehicleTypes]`/`[InfantryTypes]` roster membership as the vehicle/infantry key).
2. **Primary factory & MultipleFactory**: which factory a completed item emerges from; how "primary" is chosen and changed (`Set_Primary` / the primary flag, right-click rule; the default when several are owned — first-built vs most-recently-built); the `MultipleFactory` multiplier value (rulesmd) and exactly how each extra same-type factory compounds BuildSpeed (confirm the `FUN_006F3D00` loop: `t = ftol(t x MultipleFactory)` repeated `n-1` times, MultipleFactory `.8`).
3. **Exit cell + leave-factory** (the subtlest item): where a finished vehicle/infantry first appears relative to the factory foundation (door/exit cell — is it a fixed foundation-relative offset, or the `[unit] Factory` exit-coord list?), and the leave-factory motion before free control.
4. **Rally point**: how a right-click with a factory selected sets the rally, and where completed units path (rally vs default exit-adjacent gather cell).
5. **Build economy for units**: confirm the same `build_frames(cost, speed)` + 54-step charge model applies to UnitTypes (FactoryClass is shared across BuildingType/Infantry/Vehicle); that units have **NO** FreeUnit charge subtraction (charge == cost); and immediate-exit-vs-ready-hold (structures wait for placement; vehicles/infantry exit on completion — confirm).
6. **Per-unit prereq/tech gating**: which prerequisite groups gate each producible unit (BARRACKS infantry, FACTORY vehicles), recovered as committable constants (e.g. GI `Prerequisite=GAPILE`, Grizzly `Prerequisite=GAWEAP`).

**Steps:**

- [ ] **Step 1: Re-read prior hunt method.** Read `docs/re/workflow.md`, `docs/re/construction.md` §located-functions + §10 (queue rules) + §7 (the Time_To_Build/MultipleFactory formula), and `docs/re/economy.md` §6 (FreeUnit spawn cell) for the anchor-based method and doc format. Do NOT trust YRpp addresses; use them only as prose hints for what functions look like.
- [ ] **Step 2: Anchor sweep.** Headless script: from the FactoryClass functions already located in construction.md, walk (a) the `FUN_006F3D00` MultipleFactory loop + `FUN_00500730` factory-count-per-class to pin the multiplier and compounding; (b) the string data refs for `"IsBaseDefense"`/`"BaseNormal"` and the VehicleType/InfantryType roster loaders to pin the tab classifier; (c) the completed-unit emergence path (FactoryClass completion → the building's "flush produced object" → cell placement) to pin the exit cell + leave-factory motion; (d) the Set_Primary / primary-flag writer and the rally-point setter. Dump decompiled candidates to scratchpad.
- [ ] **Step 3: Answer §1 items 1–6.** Iterate scripts until each item is CONFIRMED or PROBABLE with evidence (function address + decompile-excerpt reasoning in prose), or explicitly [RE-FALLBACK] with the adjudicated fallback stated.
- [ ] **Step 4: Write `docs/re/production.md`.** Sections mirroring §1 items 1–6; a per-finding confidence table; a located-functions table (name, address, one-line role); a "deviations we will take" section (anything discovered that the engine will diverge on, e.g. an unpinnable exit-motion animation replaced by an instant spawn-at-exit).
- [ ] **Step 5: Commit.**

```powershell
git -C "D:\ra2-android" add docs/re/production.md
git -C "D:\ra2-android" commit -m "docs(re): production hunt - queue model, primary/MultipleFactory, exit cell, rally, unit build economy, prereqs

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Expected: one commit, `docs/re/production.md` created.

- [ ] **Step 6 (CONTROLLER, not the implementer): RE-GATE review.** Diff `production.md` against every `[RE-GATE APPLIED]` block in Tasks 2–9; amend the plan; commit the amendment BEFORE dispatching Task 2. Specifically confirm/replace: the 3-queue model + tab→queue map (Task 3/6), the `MultipleFactory` value and compounding (Task 3), `exit_cell`'s foundation-relative offset (Task 4), the rally default gather cell (Task 5), and the per-unit prereq tokens (Task 6/7). If item 1 shows anything other than "3 queues, tabs 0+1 share the BuildingType queue", STOP and amend Task 3/6 before dispatch.

---

### Task 2: rules.cpp classification accessors — build_category / is_infantry / is_vehicle

**Files:**
- Modify: `include/ra2assets/rules.hpp`, `src/rules.cpp`
- Test: `tests/test_rules.cpp` (append)
- CMake: NOTHING new (rules.cpp already in `RA2_LIB_SOURCES`; test_rules.cpp already registered).

**Interfaces:**
- Consumes: the wrapped `Ini` (see `Rules`'s `ini_`), the existing `[VehicleTypes]`/`[InfantryTypes]`/`[BuildingTypes]` list sections, and `Ini::section(name)`.
- Produces (added to `class Rules`, following the existing thin-accessor pattern):

```cpp
    // Producible category [RE-GATE APPLIED: production.md §1 item 1]. Resolved
    // from INI list-section membership plus the recovered base-defense flag:
    //   [BuildingTypes] member + IsBaseDefense=yes -> Defense
    //   [BuildingTypes] member otherwise           -> Structure
    //   [InfantryTypes] member                     -> Infantry
    //   [VehicleTypes]  member                     -> Vehicle
    //   anything else (aircraft/ships/unknown)     -> None
    // Membership is by VALUE (the id, e.g. "GI"/"E1"): the list sections map
    // numbered keys ("0=E1") to ids, so we scan values, not keys.
    enum class BuildCategory { None, Structure, Defense, Infantry, Vehicle };
    BuildCategory build_category(const std::string& id) const;

    // [id] appears as a value in [InfantryTypes] / [VehicleTypes] respectively.
    bool is_infantry(const std::string& id) const;
    bool is_vehicle(const std::string& id) const;

    // [id] IsBaseDefense=, true iff "yes"/"true" (case-insensitive); false if
    // absent [RE-GATE APPLIED: production.md §1 item 1 — the structure-vs-
    // defense classifier key; [RE-FALLBACK] if the hunt names a different key
    // (e.g. BaseNormal=no), amend to read that key instead].
    bool is_base_defense(const std::string& id) const;
```

- [ ] **Step 1: Write the failing tests** (append to `tests/test_rules.cpp`, synthetic INI like the existing cases — no install needed):

```cpp
TEST_CASE("rules: build_category / is_infantry / is_vehicle classification") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[InfantryTypes]\n0=E1\n1=E2\n"
        "[VehicleTypes]\n0=MTNK\n1=HARV\n"
        "[BuildingTypes]\n0=GAPOWR\n1=GAWEAP\n2=GAPILL\n3=GATOW\n"
        "[E1]\nName=GI\n"
        "[MTNK]\nName=Grizzly\n"
        "[GAPOWR]\nName=Power Plant\n"
        "[GAWEAP]\nName=War Factory\n"
        "[GAPILL]\nName=Pillbox\nIsBaseDefense=yes\n"
        "[GATOW]\nName=Patriot\nIsBaseDefense=true\n");
    auto r = ra2assets::Rules::load(ini);
    using BC = ra2assets::Rules::BuildCategory;
    CHECK(r.build_category("E1") == BC::Infantry);
    CHECK(r.build_category("MTNK") == BC::Vehicle);
    CHECK(r.build_category("HARV") == BC::Vehicle);
    CHECK(r.build_category("GAPOWR") == BC::Structure);
    CHECK(r.build_category("GAWEAP") == BC::Structure);
    CHECK(r.build_category("GAPILL") == BC::Defense);
    CHECK(r.build_category("GATOW") == BC::Defense);
    CHECK(r.build_category("AMCV") == BC::None);   // in no list section
    CHECK(r.is_infantry("E1"));
    CHECK_FALSE(r.is_infantry("MTNK"));
    CHECK(r.is_vehicle("MTNK"));
    CHECK_FALSE(r.is_vehicle("E1"));
    CHECK(r.is_base_defense("GAPILL"));
    CHECK_FALSE(r.is_base_defense("GAWEAP"));
}
```

- [ ] **Step 2: Run to verify failure** (FOREGROUND):

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: compile FAIL — `build_category` / `BuildCategory` not a member of `Rules`.

- [ ] **Step 3: Implement.** In `rules.hpp` add the enum + three declarations shown in Interfaces (place them near the existing `[id]`-keyed getters). In `src/rules.cpp` add:

```cpp
namespace {
// True iff `id` (case-insensitive) is a VALUE in [section]'s key/value list.
bool in_list_section(const ra2assets::Ini& ini, const std::string& section,
                     const std::string& id) {
    for (const auto& kv : ini.section(section)) {
        // Case-insensitive compare of the value against id.
        if (kv.second.size() != id.size()) continue;
        bool eq = true;
        for (std::size_t i = 0; i < id.size(); ++i)
            if (std::toupper((unsigned char)kv.second[i]) != std::toupper((unsigned char)id[i])) {
                eq = false; break;
            }
        if (eq) return true;
    }
    return false;
}
}  // namespace

bool Rules::is_infantry(const std::string& id) const {
    return in_list_section(*ini_, "InfantryTypes", id);
}
bool Rules::is_vehicle(const std::string& id) const {
    return in_list_section(*ini_, "VehicleTypes", id);
}
bool Rules::is_base_defense(const std::string& id) const {
    // Reuse the existing yes/true bool-key pattern (see has_turret()).
    const std::string v = ini_->get(id, "IsBaseDefense", "");
    return v == "yes" || v == "true" || v == "Yes" || v == "True" || v == "YES" || v == "TRUE";
}
Rules::BuildCategory Rules::build_category(const std::string& id) const {
    if (is_infantry(id)) return BuildCategory::Infantry;
    if (is_vehicle(id)) return BuildCategory::Vehicle;
    if (in_list_section(*ini_, "BuildingTypes", id))
        return is_base_defense(id) ? BuildCategory::Defense : BuildCategory::Structure;
    return BuildCategory::None;
}
```

Add `#include <cctype>` to `src/rules.cpp` if not already present. (Confirm `is_base_defense`'s yes/true test matches the exact idiom `has_turret()` already uses in this file; reuse that helper if one exists rather than duplicating the literal list.)

- [ ] **Step 4: Run tests to verify pass** (FOREGROUND): rebuild, then `D:\ra2-android\build\ra2assets_tests.exe --test-case="rules: build_category*"`. Expected: PASS.

- [ ] **Step 5: Real-rules pins** (install-gated, mirror the existing `RA2_INSTALL_DIR` skip pattern used by the other `rules:` install tests — locate real rulesmd.ini from `ra2md.mix`/nested mixes exactly like the existing "locates real rulesmd.ini" case, then assert):

```cpp
TEST_CASE("rules: real rulesmd classification pins") {
    // ... same ra2md.mix -> rulesmd.ini location + skip as the existing
    //     "locates real rulesmd.ini" case; on miss MESSAGE + return ...
    using BC = ra2assets::Rules::BuildCategory;
    CHECK(rules.build_category("E1") == BC::Infantry);     // GI
    CHECK(rules.build_category("MTNK") == BC::Vehicle);    // Grizzly
    CHECK(rules.build_category("GAWEAP") == BC::Structure);// War Factory
    // [RE-GATE APPLIED: production.md §1 item 1] a base-defense structure:
    CHECK(rules.build_category("GAPILL") == BC::Defense);  // Pillbox
}
```

- [ ] **Step 6: Full suite FOREGROUND, both modes** (with and without `RA2_INSTALL_DIR`). Expected: all pass, install pins skip clean when unset.
- [ ] **Step 7: Commit** `feat(assets): rules classification accessors (build_category, is_infantry, is_vehicle)`.

---

### Task 3: engine/production multi-queue — UnitTypeDef, scalar start, ProductionQueues

**Files:**
- Create: `engine/unit_type.hpp`
- Modify: `engine/production.hpp`, `engine/production.cpp`
- Test: `tests/test_production.cpp` (append)
- CMake: add `${RA2_ROOT}/engine/unit_type.hpp` needs no source entry (header-only). `production.cpp` already registered in both files. **No new .cpp**, so no cmake edit is required for this task.

**Interfaces:**
- Consumes: `BuildingTypeDef`, `Building`, `prerequisites_met`, `build_frames`, `QueueSlot`, `ProductionState`, `tick`, `cancel`, `placed` (all existing in `production.hpp`); `Rules::BuildCategory` is NOT consumed here (engine stays free of ra2assets Rules — main.cpp resolves category into `UnitTypeDef::is_vehicle`).
- Produces — `engine/unit_type.hpp` (new, `namespace ra2engine`):

```cpp
#pragma once
// One producible unit type's production-relevant stats, resolved ONCE by the
// caller (main.cpp from Rules/Art, or a test by hand) — the UnitType mirror of
// BuildingTypeDef. Index into the unit roster is the queue's `type` key.
// Calibration: docs/re/production.md §5 (units have NO FreeUnit subtraction:
// charge == cost). No SDL/OS.
#include <string>
#include <vector>
#include "world.hpp"   // UnitCombat

namespace ra2engine {

struct UnitTypeDef {
    std::string id;                         // "E1" / "MTNK"
    int cost = 0;                           // Rules Cost=; charge == cost (§5)
    int tech_level = 0;                     // Rules TechLevel=; <=0 = never buildable
    bool is_vehicle = false;                // Infantry queue vs Vehicle queue
    bool chrono_miner = false;              // CMIN flag (add_harvester path); unused for combat units
    std::vector<std::string> prerequisite;  // uppercased tokens (BARRACKS/FACTORY groups or ids)
    std::string cameo;                      // Art Cameo= ("" = never in sidebar)
    UnitCombat combat;                      // resolved stats for World::add_combat_unit/add_harvester
};

}  // namespace ra2engine
```

- Produces — added to `engine/production.hpp` (`namespace ra2engine`):

```cpp
// Scalar production entry point [RE-GATE APPLIED: production.md §5 — the
// FactoryClass 54-step economy is shared by BuildingType/Infantry/Vehicle].
// Decouples the queue math from the BuildingType roster: total_frames =
// build_frames(cost, build_speed); step_ticks = clamp(total_frames/54, 1, 255);
// charge_basis = charge (caller passes cost when there is no FreeUnit — units
// always do). tick()/cancel()/placed() are reused verbatim. The existing
// start(ps, type, types, build_speed) becomes a one-line wrapper over this
// (types[type].cost, and charge = types[type].charge>0 ? types[type].charge :
// types[type].cost) so the BuildingType path stays BIT-IDENTICAL.
void start(ProductionState& ps, int cost, int charge, double build_speed);

// Can unit `unit_type` start in queue `ps`? factory_alive = an own alive Idle
// factory of this queue's category exists; slot idle; TechLevel > 0; prereqs
// met by scanning BUILDINGS (a unit's prereq is a BARRACKS/FACTORY building),
// reusing prerequisites_met. Affordability is NOT gated (RA2 starts at any
// credits and stalls at 0). `unit_roster`/`unit_type` name the UnitTypeDef.
bool can_start_unit(const ProductionState& ps, int unit_type,
                    const std::vector<UnitTypeDef>& unit_roster,
                    const std::vector<Building>& buildings,
                    const std::vector<BuildingTypeDef>& btypes, int house,
                    const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
                    bool factory_alive);

// The three concurrent production queues [RE-GATE APPLIED: production.md §1
// item 1 — 3 FactoryClass queues behind 4 tabs; tabs 0 (Structures) and 1
// (Defenses) BOTH view the Building queue]. Additive: the Building queue's
// ProductionState is stepped/started EXACTLY as before (demo-base/demo-econ
// untouched — those never construct a ProductionQueues). Infantry/Vehicle
// queues draw from a UnitTypeDef roster and, on completion, raise a
// ready-to-exit flag consumed by World (Task 5) instead of ready-to-place.
enum class QueueCat { Building, Infantry, Vehicle };

class ProductionQueues {
public:
    ProductionQueues() = default;

    ProductionState& building() { return q_[0]; }        // the existing single-queue economy
    const ProductionState& building() const { return q_[0]; }
    ProductionState& state(QueueCat c) { return q_[(int)c]; }
    const ProductionState& state(QueueCat c) const { return q_[(int)c]; }

    // Wire the credit pool: all three queues SHARE the player's credits, so
    // set the same value on every ProductionState each frame before tick()
    // (mirrors how ps.credits is the single money pool). Call once at setup +
    // whenever credits change outside tick (income, placement).
    void set_credits(double credits) { for (auto& q : q_) q.credits = credits; }
    double credits() const { return q_[0].credits; }

    // Advance every non-idle queue by dt. Each queue's speed = power_mult x its
    // MultipleFactory speedup (factory_speedup[cat], 1.0 for Building). The
    // three queues share ONE money pool, so tick SEQUENTIALLY (Building ->
    // Infantry -> Vehicle) carrying the running balance across queues — total
    // spend can never exceed the money, and the order is deterministic. Caller
    // sets all three credits equal via set_credits() first; credits() reads the
    // final pool balance after. [RE-GATE APPLIED: production.md §5 — shared
    // FactoryClass timing]
    void tick(double dt, double power_mult, const double factory_speedup[3],
              const std::vector<BuildingTypeDef>& btypes);

    // Reconcile the pool after a cancel(): a refund raises exactly one queue's
    // balance above the shared tail, so set all three to the max. Caller reads
    // credits() after. (No double-count: only the just-cancelled queue is high.)
    void reconcile_refund() {
        double m = q_[0].credits;
        for (int i = 1; i < 3; ++i) m = std::max(m, q_[i].credits);
        for (auto& q : q_) q.credits = m;
    }

    // Start a unit build in the Infantry/Vehicle queue (caller checked
    // can_start_unit). charge == cost (§5, no FreeUnit).
    void start_unit(QueueCat c, int unit_type, const std::vector<UnitTypeDef>& unit_roster,
                    double build_speed);

    void cancel(QueueCat c) { ra2engine::cancel(q_[(int)c]); }
    // Queue slot readiness for the unit queues (Task 5 consumes this).
    bool ready(QueueCat c) const { return q_[(int)c].slot.ready; }
    int producing(QueueCat c) const { return q_[(int)c].slot.type; }
    void placed(QueueCat c) { ra2engine::placed(q_[(int)c]); }

private:
    ProductionState q_[3];   // [0]=Building, [1]=Infantry, [2]=Vehicle
};
```

**Byte-identity discipline:** the existing `start(ps, type, types, build_speed)` MUST keep producing bit-identical `total_frames`/`step_ticks`/`charge_basis`. Refactor it to call the scalar `start`; verify `--demo-base` stays `hash=5273b5e71ca009e1` and `--demo-econ` stays `econ_hash=7fed96cb98b68017` (Step 5).

- [ ] **Step 1: Failing tests** (append to `tests/test_production.cpp`; reuse the existing `roster()`/`live()`/`kGroups` helpers). Add a small unit roster + a scalar-parity assertion:

```cpp
static std::vector<UnitTypeDef> unit_roster() {
    std::vector<UnitTypeDef> u(2);
    u[0].id = "E1";   u[0].cost = 200; u[0].tech_level = 1; u[0].is_vehicle = false;
    u[0].prerequisite = {"BARRACKS"};
    u[1].id = "MTNK"; u[1].cost = 2000; u[1].tech_level = 2; u[1].is_vehicle = true;
    u[1].prerequisite = {"FACTORY"};
    return u;
}
static const std::vector<std::pair<std::string, std::vector<std::string>>> kUGroups = {
    {"BARRACKS", {"GAPILE"}}, {"FACTORY", {"GAWEAP"}}, {"POWER", {"GAPOWR", "NAPOWR"}}};
static Building livet(int type, int house = 0) {  // like live() but arbitrary building type roster
    Building b; b.type = type; b.house = house; b.alive = true;
    b.state = Building::State::Idle; b.hp = b.max_hp = 100; return b;
}

TEST_CASE("production: scalar start matches the BuildingType wrapper bit-for-bit") {
    auto t = roster();  // GACNST/GAPOWR/GAPILE, no FreeUnit -> charge == cost
    ProductionState a, b;
    a.credits = b.credits = 10000;
    start(a, 1, t, 0.7);                                   // GAPOWR via wrapper
    start(b, t[1].cost, t[1].cost, 0.7);                   // scalar, charge==cost
    CHECK(a.slot.total_frames == b.slot.total_frames);
    CHECK(a.slot.step_ticks == b.slot.step_ticks);
    CHECK(a.slot.charge_basis == b.slot.charge_basis);
    // A FreeUnit type (charge < cost) still routes through the scalar correctly:
    std::vector<BuildingTypeDef> refn(1);
    refn[0].id = "GAREFN"; refn[0].cost = 2000; refn[0].charge = 600; refn[0].tech_level = 1;
    ProductionState c, d;
    c.credits = d.credits = 10000;
    start(c, 0, refn, 0.7);
    start(d, refn[0].cost, refn[0].charge, 0.7);
    CHECK(c.slot.total_frames == 1260);                    // raw cost 2000
    CHECK(c.slot.charge_basis == 600);                     // FreeUnit-subtracted
    CHECK(c.slot.total_frames == d.slot.total_frames);
    CHECK(c.slot.charge_basis == d.slot.charge_basis);
}

TEST_CASE("production: can_start_unit gates on TechLevel + building prereqs") {
    auto u = unit_roster();
    std::vector<BuildingTypeDef> bt(1); bt[0].id = "GAPILE";  // BARRACKS group member
    ProductionState ps; ps.credits = 5000;
    std::vector<Building> none;
    CHECK_FALSE(can_start_unit(ps, 0, u, none, bt, 0, kUGroups, /*factory_alive=*/true)); // no barracks
    std::vector<Building> barracks = {livet(0)};  // GAPILE idle
    CHECK(can_start_unit(ps, 0, u, barracks, bt, 0, kUGroups, true));
    CHECK_FALSE(can_start_unit(ps, 0, u, barracks, bt, 0, kUGroups, /*factory_alive=*/false));
    // GI prereq met but MTNK needs FACTORY (not present) -> blocked:
    CHECK_FALSE(can_start_unit(ps, 1, u, barracks, bt, 0, kUGroups, true));
}

TEST_CASE("production: infantry queue reproduces the 54-step economy for a unit") {
    auto u = unit_roster();
    ProductionQueues q;
    q.set_credits(10000);
    q.start_unit(QueueCat::Infantry, 0, u, 0.7);           // E1 cost 200
    const ProductionState& inf = q.state(QueueCat::Infantry);
    CHECK(inf.slot.total_frames == build_frames(200, 0.7));  // = 126
    CHECK(inf.slot.step_ticks == std::max(1, 126 / 54));     // clamp(2,1,255)=2
    const double speedup[3] = {1.0, 1.0, 1.0};
    std::vector<BuildingTypeDef> bt;
    // Drive to completion; charge_basis == cost (no FreeUnit).
    for (int i = 0; i < 10000 && !q.ready(QueueCat::Infantry); ++i)
        q.tick(1.0 / 15.0, 1.0, speedup, bt);
    CHECK(q.ready(QueueCat::Infantry));
    CHECK(inf.slot.spent == doctest::Approx(200.0));
}

TEST_CASE("production: three queues advance concurrently and independently") {
    auto u = unit_roster();
    ProductionQueues q;
    q.set_credits(100000);
    q.start_unit(QueueCat::Infantry, 0, u, 0.7);           // E1
    q.start_unit(QueueCat::Vehicle, 1, u, 0.7);            // MTNK
    // (Building queue left idle — its ProductionState is untouched.)
    const double speedup[3] = {1.0, 1.0, 1.0};
    std::vector<BuildingTypeDef> bt;
    for (int i = 0; i < 5; ++i) q.tick(1.0 / 15.0, 1.0, speedup, bt);
    CHECK(q.producing(QueueCat::Infantry) == 0);
    CHECK(q.producing(QueueCat::Vehicle) == 1);
    CHECK(q.producing(QueueCat::Building) == -1);
    // Infantry (cheap, fast) reaches ready long before the vehicle:
    for (int i = 0; i < 10000 && !q.ready(QueueCat::Infantry); ++i)
        q.tick(1.0 / 15.0, 1.0, speedup, bt);
    CHECK(q.ready(QueueCat::Infantry));
    CHECK_FALSE(q.ready(QueueCat::Vehicle));
}

TEST_CASE("production: MultipleFactory speedup scales the vehicle queue [RE-GATE §1 item 2]") {
    auto u = unit_roster();
    // [RE-GATE APPLIED: production.md §1 item 2] two war factories multiply the
    // effective build speed by MultipleFactory once (n-1 = 1); pre-hunt value
    // 0.8 -> a build with 2 factories completes in FEWER frames than with 1.
    // [RE-FALLBACK] if the recovered value/compounding differs, amend this
    // constant + the speedup a queue receives.
    ProductionQueues one, two;
    one.set_credits(1e9); two.set_credits(1e9);
    one.start_unit(QueueCat::Vehicle, 1, u, 0.7);
    two.start_unit(QueueCat::Vehicle, 1, u, 0.7);
    std::vector<BuildingTypeDef> bt;
    const double s1[3] = {1.0, 1.0, 1.0};
    const double s2[3] = {1.0, 1.0, 1.0 / 0.8};   // 2 factories: divide time by 0.8
    int f1 = 0, f2 = 0;
    while (!one.ready(QueueCat::Vehicle)) { one.tick(1.0/15.0, 1.0, s1, bt); ++f1; }
    while (!two.ready(QueueCat::Vehicle)) { two.tick(1.0/15.0, 1.0, s2, bt); ++f2; }
    CHECK(f2 < f1);
}

TEST_CASE("production: BuildingType queue path is unchanged by the multi-queue layer") {
    // The Building queue reached via ProductionQueues behaves EXACTLY like a
    // standalone ProductionState driven by start()+tick().
    auto t = roster();
    ProductionQueues q; q.set_credits(10000);
    start(q.building(), 1, t, 0.7);                 // GAPOWR through the shared start()
    ProductionState solo; solo.credits = 10000;
    start(solo, 1, t, 0.7);
    CHECK(q.building().slot.total_frames == solo.slot.total_frames);
    CHECK(q.building().slot.step_ticks == solo.slot.step_ticks);
    const double speedup[3] = {1.0, 1.0, 1.0};
    for (int i = 0; i < 10; ++i) {
        q.tick(1.0/15.0, 1.0, speedup, t);
        tick(solo, 1.0/15.0, 1.0, t);
    }
    CHECK(q.building().slot.stage == solo.slot.stage);
    CHECK(q.building().slot.spent == doctest::Approx(solo.slot.spent));
}
```

- [ ] **Step 2: Add the include + create the header, build FOREGROUND, verify failure.** Create `engine/unit_type.hpp` (above). Add `#include "unit_type.hpp"` to `engine/production.hpp` (after the existing `#include "building.hpp"`). Build the test target.

Expected: compile FAIL — `start(ps,int,int,double)` / `can_start_unit` / `ProductionQueues` not defined.

- [ ] **Step 3: Implement the scalar start + wrapper refactor** in `production.cpp`. Replace the existing `start(ProductionState&, int type, const std::vector<BuildingTypeDef>&, double)` body so it delegates, and add the scalar overload ABOVE it:

```cpp
void start(ProductionState& ps, int cost, int charge, double build_speed) {
    ps.slot = QueueSlot{};
    ps.slot.type = 0;   // caller (wrapper / queue) overwrites with the real roster index
    ps.slot.total_frames = std::max(1, build_frames(cost, build_speed));
    ps.slot.step_ticks = std::clamp(ps.slot.total_frames / kProductionSteps, 1, 255);
    ps.slot.charge_basis = charge > 0 ? charge : cost;   // charge==0 fallback -> cost
}

void start(ProductionState& ps, int type, const std::vector<BuildingTypeDef>& types,
           double build_speed) {
    const BuildingTypeDef& t = types[type];
    start(ps, t.cost, t.charge, build_speed);   // charge==0 -> scalar uses cost (BIT-IDENTICAL)
    ps.slot.type = type;                         // preserve the roster index the old code set
}
```

Rationale for bit-identity: the old code computed `total_frames = max(1, build_frames(t.cost, build_speed))`, `step_ticks = clamp(total_frames/54,1,255)`, `charge_basis = t.charge>0 ? t.charge : t.cost`, all after `ps.slot = QueueSlot{}` — the scalar reproduces every one of those operations in the same order, then the wrapper restores `ps.slot.type = type`. No arithmetic changed.

- [ ] **Step 4: Implement `can_start_unit` + `ProductionQueues`** in `production.cpp`:

```cpp
bool can_start_unit(const ProductionState& ps, int unit_type,
                    const std::vector<UnitTypeDef>& unit_roster,
                    const std::vector<Building>& buildings,
                    const std::vector<BuildingTypeDef>& btypes, int house,
                    const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
                    bool factory_alive) {
    if (unit_type < 0 || unit_type >= (int)unit_roster.size()) return false;
    if (!factory_alive || ps.slot.type != -1) return false;
    if (unit_roster[unit_type].tech_level <= 0) return false;
    // A unit's prereq tokens name BUILDINGS (BARRACKS/FACTORY) -> scan buildings.
    return prerequisites_met(unit_roster[unit_type].prerequisite, buildings, btypes, house, groups);
}

void ProductionQueues::start_unit(QueueCat c, int unit_type,
                                  const std::vector<UnitTypeDef>& unit_roster, double build_speed) {
    const UnitTypeDef& u = unit_roster[unit_type];
    start(q_[(int)c], u.cost, u.cost, build_speed);   // §5: charge == cost (no FreeUnit)
    q_[(int)c].slot.type = unit_type;                 // roster index into the UNIT roster
}

void ProductionQueues::tick(double dt, double power_mult, const double factory_speedup[3],
                            const std::vector<BuildingTypeDef>& btypes) {
    // Sequential single pool (deterministic Building->Infantry->Vehicle): carry
    // the running balance across queues so total spend never exceeds the money.
    double money = q_[0].credits;   // caller set all three equal via set_credits
    for (int i = 0; i < 3; ++i) {
        q_[i].credits = money;
        ra2engine::tick(q_[i], dt, power_mult * factory_speedup[i], btypes);
        money = q_[i].credits;      // this queue's spend reduces the shared pool
    }
    for (auto& q : q_) q.credits = money;   // final balance to all three
}
```

- [ ] **Step 5: Build + run FOREGROUND**, expected PASS:

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="production:*"
```

Then the CRITICAL byte-identity check — build the engine and run the two math-sensitive gates:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
```

Expected: `production:*` all PASS; `--demo-base` prints `hash=5273b5e71ca009e1`; `--demo-econ` prints `econ_hash=7fed96cb98b68017` and `deterministic=yes`. If either hash moved, the wrapper refactor broke bit-identity — revert Step 3 and re-derive.

- [ ] **Step 6: Full suite FOREGROUND both modes.** Expected: all pass, skips clean.
- [ ] **Step 7: Commit** `feat(engine): multi-queue production - scalar start, UnitTypeDef, ProductionQueues [RE: production.md]`.

---

### Task 4: engine/factory — exit cell, primary factory, set primary

**Files:**
- Create: `engine/factory.hpp`, `engine/factory.cpp`
- Modify: `engine/building.hpp` (four Building fields)
- Test: `tests/test_factory.cpp` (create)
- CMake: add `${RA2_ROOT}/engine/factory.cpp` to `RA2_ENGINE_SOURCES` in `cmake/ra2_sources.cmake`, AND `target_sources(ra2assets_tests PRIVATE tests/test_factory.cpp engine/factory.cpp)` in `CMakeLists.txt` (mirror the `test_ore`/`test_harvest` lines).

**Interfaces:**
- Consumes: `Building`, `BuildingTypeDef` (building.hpp), `Cellxy` (pathfind.hpp).
- Produces — added to `struct Building` in `engine/building.hpp` (after `blocking_released`):

```cpp
    // --- Production phase: factory rally/primary [RE: production.md §1] ---
    // NOT read by world_hash (which hashes only type/cx/cy/hp/state), so
    // --demo-base stays hash=5273b5e71ca009e1 (same discipline Economy used
    // adding fields to Unit). Default-initialized; only unit-factory buildings
    // ever set them.
    bool primary = false;            // this house's primary factory of its category
    int rally_x = 0, rally_y = 0;    // rally-point cell (valid iff has_rally)
    bool has_rally = false;          // a rally point was set for this factory
```

- Produces — `engine/factory.hpp` (new, `namespace ra2engine`):

```cpp
#pragma once
// Pure factory-exit / primary-factory / rally logic [RE: docs/re/production.md
// §1-§4], layered on Building like harvest.cpp is on Unit. No SDL/OS.
// `factory_types` is a category resolved to roster indices by the caller (the
// set of BuildingType indices that are factories for ONE queue category, e.g.
// the Vehicle queue's {GAWEAP}) — the same "resolve the category to a roster-
// index list" pattern harvest.cpp's refinery_types uses.
#include <vector>
#include "building.hpp"
#include "pathfind.hpp"  // Cellxy

namespace ra2engine {

// The cell a finished unit first appears in, foundation-relative
// [RE-GATE APPLIED: production.md §1 item 3]. [RE-FALLBACK adjudicated,
// mirroring the Economy refinery dock cell]: one row south of the foundation's
// center column — (cx + fw/2, cy + fh). If the hunt pins a real per-type exit
// coord (the [unit] Factory exit list), amend this to read it.
Cellxy exit_cell(const Building& factory, const BuildingTypeDef& def);

// Index of the factory a completed unit exits from: the alive, Idle, owned
// building whose type is in `factory_types` that is flagged `primary`; if none
// is flagged, the recovered default [RE-GATE APPLIED: production.md §1 item 2 —
// [RE-FALLBACK adjudicated]: the FIRST-built (lowest index) such factory].
// Returns -1 if the house owns no alive Idle factory of this category.
int primary_factory(const std::vector<Building>& buildings,
                    const std::vector<int>& factory_types, int house);

// Set building `b_idx` as its house's primary factory of its category: sets
// primary=true on b_idx and clears primary on EVERY OTHER building of the same
// house whose type is in `factory_types` (exactly one primary per category per
// house). No-op if b_idx is out of range or its type is not in factory_types.
void set_primary(std::vector<Building>& buildings, const std::vector<int>& factory_types,
                 int b_idx);

}  // namespace ra2engine
```

- [ ] **Step 1: Failing tests** (`tests/test_factory.cpp`) — hand-built vectors, no MapFile:

```cpp
#include "doctest.h"
#include "../engine/factory.hpp"
using namespace ra2engine;

namespace {
Building fac(int type, int cx, int cy, int fw, int fh, int house = 0) {
    Building b; b.type = type; b.cx = cx; b.cy = cy; b.fw = fw; b.fh = fh;
    b.house = house; b.alive = true; b.state = Building::State::Idle;
    b.hp = b.max_hp = 100; return b;
}
BuildingTypeDef def(const char* id, int fw, int fh) {
    BuildingTypeDef d; d.id = id; d.fw = fw; d.fh = fh; return d;
}
}  // namespace

TEST_CASE("factory: exit_cell is foundation-relative [RE-GATE §1 item 3]") {
    Building b = fac(3, 10, 20, 3, 2);           // GAWEAP 3x2 at (10,20)
    Cellxy e = exit_cell(b, def("GAWEAP", 3, 2));
    CHECK(e.x == 10 + 3 / 2);                     // cx + fw/2 = 11
    CHECK(e.y == 20 + 2);                         // cy + fh   = 22
}

TEST_CASE("factory: primary_factory prefers the flagged primary, else first-built") {
    std::vector<Building> bs = {fac(3, 0, 0, 3, 2), fac(3, 8, 0, 3, 2)};  // two GAWEAP
    std::vector<int> ftypes = {3};
    CHECK(primary_factory(bs, ftypes, 0) == 0);   // none flagged -> first-built
    bs[1].primary = true;
    CHECK(primary_factory(bs, ftypes, 0) == 1);   // flagged wins
    // A Buildup / dead / enemy factory is not eligible:
    bs[1].state = Building::State::Buildup;
    CHECK(primary_factory(bs, ftypes, 0) == 0);   // falls back to the Idle one
    std::vector<Building> none;
    CHECK(primary_factory(none, ftypes, 0) == -1);
    std::vector<Building> enemy = {fac(3, 0, 0, 3, 2, /*house=*/1)};
    CHECK(primary_factory(enemy, ftypes, 0) == -1);
}

TEST_CASE("factory: set_primary flags one, clears every other same-category same-house") {
    std::vector<Building> bs = {fac(3, 0, 0, 3, 2), fac(3, 8, 0, 3, 2), fac(3, 16, 0, 3, 2)};
    std::vector<int> ftypes = {3};
    bs[0].primary = true;
    set_primary(bs, ftypes, 2);
    CHECK_FALSE(bs[0].primary);
    CHECK_FALSE(bs[1].primary);
    CHECK(bs[2].primary);
    // Another house's factory of the same type is untouched:
    bs.push_back(fac(3, 24, 0, 3, 2, /*house=*/1));
    bs[3].primary = true;
    set_primary(bs, ftypes, 0);
    CHECK(bs[0].primary);
    CHECK(bs[3].primary);                          // house 1 kept its own primary
    // A non-factory type is not touched:
    set_primary(bs, ftypes, /*b_idx=*/999);        // out of range -> no crash, no change
    CHECK(bs[0].primary);
}
```

- [ ] **Step 2: Add to CMake, add the Building fields, create the header, build FOREGROUND, verify failure.** Edit `cmake/ra2_sources.cmake` and `CMakeLists.txt` as in **Files**; add the four `Building` fields; create `engine/factory.hpp`. Build the test target.

Expected: link/compile FAIL — `exit_cell`/`primary_factory`/`set_primary` undefined.

- [ ] **Step 3: Implement `engine/factory.cpp`:**

```cpp
#include "factory.hpp"
#include <algorithm>

namespace ra2engine {

Cellxy exit_cell(const Building& factory, const BuildingTypeDef& def) {
    // [RE-GATE APPLIED: production.md §1 item 3 — [RE-FALLBACK adjudicated]]
    return Cellxy{factory.cx + def.fw / 2, factory.cy + def.fh};
}

namespace {
bool is_cat(const std::vector<int>& factory_types, int type) {
    return std::find(factory_types.begin(), factory_types.end(), type) != factory_types.end();
}
}  // namespace

int primary_factory(const std::vector<Building>& buildings,
                    const std::vector<int>& factory_types, int house) {
    int fallback = -1;
    for (int i = 0; i < (int)buildings.size(); ++i) {
        const Building& b = buildings[i];
        if (!b.alive || b.state != Building::State::Idle || b.house != house) continue;
        if (!is_cat(factory_types, b.type)) continue;
        if (b.primary) return i;                 // flagged primary wins outright
        if (fallback < 0) fallback = i;          // else the first-built eligible one
    }
    return fallback;
}

void set_primary(std::vector<Building>& buildings, const std::vector<int>& factory_types,
                 int b_idx) {
    if (b_idx < 0 || b_idx >= (int)buildings.size()) return;
    if (!is_cat(factory_types, buildings[b_idx].type)) return;
    const int house = buildings[b_idx].house;
    for (int i = 0; i < (int)buildings.size(); ++i) {
        Building& b = buildings[i];
        if (b.house != house || !is_cat(factory_types, b.type)) continue;
        b.primary = (i == b_idx);
    }
}

}  // namespace ra2engine
```

- [ ] **Step 4: Build + run FOREGROUND**, expected PASS:

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="factory:*"
```

- [ ] **Step 5: Byte-identity check** — the new Building fields must not touch `world_hash`. Build + run the base gate FOREGROUND:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-base
```

Expected: `hash=5273b5e71ca009e1` (unchanged — `world_hash` hashes only `type/cx/cy/hp/state`; the new default-initialized fields are invisible to it). State this in the task report.

- [ ] **Step 6: Full suite FOREGROUND both modes.** Expected: all pass.
- [ ] **Step 7: Commit** `feat(engine): factory module - exit cell, primary factory, set_primary + Building rally fields [RE: production.md]`.

---

### Task 5: World integration — launch at exit, rally routing, set_rally/set_primary, factory-lost query

**Files:**
- Modify: `engine/world.hpp`, `engine/world.cpp`
- Test: `tests/test_world.cpp` (append)
- CMake: none (world.cpp already registered).

**Interfaces:**
- Consumes: `exit_cell`, `primary_factory`, `set_primary` (factory.hpp — add `#include "factory.hpp"` to `world.cpp`); `add_combat_unit`, `order_unit_move` (private), `walkable`, `unit_at_cell`, `occupancy` (World).
- Produces (added to `class World`, public — near the Economy-phase block):

```cpp
    // --- Production phase [RE: production.md §4] ---
    // Set factory `b_idx`'s rally cell (finished units path here after exiting).
    // No-op if b_idx is out of range or not an alive building.
    void set_rally(int b_idx, int x, int y);

    // Make `b_idx` its house's primary factory of its category (factory::
    // set_primary over `factory_types`, the roster-index list for ONE category).
    // No-op if b_idx is out of range.
    void set_primary(int b_idx, const std::vector<int>& factory_types);

    // Launch a finished COMBAT unit from `factory_idx`: if exit_cell(factory,
    // factory_def) is walkable AND unoccupied, add_combat_unit there (house =
    // factory's house, stats from `c`), then order it to the factory's rally
    // cell (has_rally) else the default gather cell (exit + kDefaultGather),
    // via order_unit_move (which cancels cleanly if unreachable). Returns the
    // new unit index; -1 if the exit was blocked (caller HOLDS the queue ready)
    // or factory_idx is invalid. Asset-free: the caller resolves c + factory_def.
    int launch_unit(int factory_idx, const BuildingTypeDef& factory_def, const UnitCombat& c);

    // True iff `house` owns NO alive Idle factory whose type is in
    // factory_types (the caller then cancels+refunds that queue; a survivor ->
    // false, the queue continues). Mirrors ps_tick's ConYard-died cancel gate.
    bool factory_category_lost(const std::vector<int>& factory_types, int house) const;
```

- Produces — a header-level constant in `world.hpp` (near the Economy constants) OR inline in `world.cpp`; the plan uses the offset directly:

```cpp
// Default gather offset from the exit cell when a factory has no rally point
// [RE-GATE APPLIED: production.md §4 — [RE-FALLBACK adjudicated]: two cells
// further out (south) of the exit so a launched unit clears the door for the
// next one]. If the hunt pins a real default (e.g. exit-adjacent), amend.
constexpr int kDefaultGatherDX = 0;
constexpr int kDefaultGatherDY = 2;
```

- [ ] **Step 1: Failing tests** (append to `tests/test_world.cpp`; reuse the file's real-map skip helper `load_real_map_w` + `pick_start_cell` — install-gated like every other World integration case). Build a UnitCombat by hand (mirror the existing combat-unit spawns in this file). Cases:

```cpp
TEST_CASE("World: launch_unit spawns at the exit when free and routes to rally") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // Place a 1x1 "factory" building on a walkable cell; its exit is (cx+0, cy+1).
    const int fi = world.add_building(/*type=*/0, c->x, c->y, /*fw=*/1, /*fh=*/1, /*house=*/0,
                                      /*strength=*/100, /*armor_ix=*/0, /*buildup=*/0.0,
                                      /*instant=*/true);
    ra2engine::BuildingTypeDef fdef; fdef.id = "GAWEAP"; fdef.fw = 1; fdef.fh = 1;
    const ra2engine::Cellxy ex = ra2engine::exit_cell(world.buildings()[fi], fdef);
    REQUIRE(world.walkable(ex.x, ex.y));
    // Set a rally a few cells away, then launch a GI.
    world.set_rally(fi, ex.x + 4, ex.y);
    ra2engine::UnitCombat gi; gi.image = "GI"; gi.strength = 100; gi.armor = "none";
    const int ui = world.launch_unit(fi, fdef, gi);
    REQUIRE(ui >= 0);
    CHECK(world.units()[ui].cell_x == ex.x);
    CHECK(world.units()[ui].cell_y == ex.y);
    CHECK(world.units()[ui].house == 0);
    CHECK(world.units()[ui].moving());               // has a path toward the rally
}

TEST_CASE("World: launch_unit HOLDS (returns -1) when the exit cell is occupied") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    const int fi = world.add_building(0, c->x, c->y, 1, 1, 0, 100, 0, 0.0, true);
    ra2engine::BuildingTypeDef fdef; fdef.id = "GAWEAP"; fdef.fw = 1; fdef.fh = 1;
    const ra2engine::Cellxy ex = ra2engine::exit_cell(world.buildings()[fi], fdef);
    REQUIRE(world.walkable(ex.x, ex.y));
    // Occupy the exit cell first.
    ra2engine::UnitCombat blocker; blocker.image = "GI"; blocker.strength = 100; blocker.armor = "none";
    world.add_combat_unit(ex.x, ex.y, 0, blocker);
    const int before = static_cast<int>(world.units().size());
    const int ui = world.launch_unit(fi, fdef, blocker);
    CHECK(ui == -1);                                  // blocked
    CHECK(static_cast<int>(world.units().size()) == before);  // nothing spawned
}

TEST_CASE("World: launch_unit gathers to the default cell when no rally is set") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    const int fi = world.add_building(0, c->x, c->y, 1, 1, 0, 100, 0, 0.0, true);
    ra2engine::BuildingTypeDef fdef; fdef.id = "GAWEAP"; fdef.fw = 1; fdef.fh = 1;
    ra2engine::UnitCombat gi; gi.image = "GI"; gi.strength = 100; gi.armor = "none";
    const int ui = world.launch_unit(fi, fdef, gi);   // no set_rally called
    REQUIRE(ui >= 0);
    CHECK_FALSE(world.buildings()[fi].has_rally);
    // Moving toward the default gather cell (or standing if it was unreachable):
    // the key contract is a spawn at the exit; a path is best-effort.
    CHECK(world.units()[ui].cell_x == world.buildings()[fi].cx + fdef.fw / 2);
}

TEST_CASE("World: set_primary routes primary_factory, factory_category_lost detects a wipe") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    const int a = world.add_building(3, c->x, c->y, 1, 1, 0, 100, 0, 0.0, true);
    const int b = world.add_building(3, c->x + 2, c->y, 1, 1, 0, 100, 0, 0.0, true);
    std::vector<int> ftypes = {3};
    CHECK(ra2engine::primary_factory(world.buildings(), ftypes, 0) == a);  // first-built default
    world.set_primary(b, ftypes);
    CHECK(ra2engine::primary_factory(world.buildings(), ftypes, 0) == b);
    CHECK_FALSE(world.factory_category_lost(ftypes, 0));
    ra2engine::kill_building(world.buildings()[a]);
    ra2engine::kill_building(world.buildings()[b]);
    CHECK(world.factory_category_lost(ftypes, 0));    // no alive Idle factory left
}
```

- [ ] **Step 2: Build FOREGROUND, verify failure.** Add `#include "factory.hpp"` to `world.cpp`; declare the four methods + the two constants. Build.

Expected: compile FAIL — `set_rally`/`launch_unit`/etc. undefined.

- [ ] **Step 3: Implement in `world.cpp`:**

```cpp
void World::set_rally(int b_idx, int x, int y) {
    if (b_idx < 0 || b_idx >= (int)buildings_.size() || !buildings_[b_idx].alive) return;
    buildings_[b_idx].rally_x = x;
    buildings_[b_idx].rally_y = y;
    buildings_[b_idx].has_rally = true;
}

void World::set_primary(int b_idx, const std::vector<int>& factory_types) {
    ra2engine::set_primary(buildings_, factory_types, b_idx);
}

int World::launch_unit(int factory_idx, const BuildingTypeDef& factory_def, const UnitCombat& c) {
    if (factory_idx < 0 || factory_idx >= (int)buildings_.size()) return -1;
    const Building& f = buildings_[factory_idx];   // copy exit coords before spawning
    const Cellxy ex = exit_cell(f, factory_def);
    if (!walkable(ex.x, ex.y) || unit_at_cell(ex.x, ex.y) >= 0) return -1;  // HOLD
    const int house = f.house;
    const bool has_rally = f.has_rally;
    const int rx = f.rally_x, ry = f.rally_y;
    add_combat_unit(ex.x, ex.y, house, c);
    const int ui = (int)units_.size() - 1;
    const int gx = has_rally ? rx : ex.x + kDefaultGatherDX;
    const int gy = has_rally ? ry : ex.y + kDefaultGatherDY;
    order_unit_move(ui, gx, gy);   // cancels cleanly if unreachable/self
    return ui;
}

bool World::factory_category_lost(const std::vector<int>& factory_types, int house) const {
    for (const Building& b : buildings_) {
        if (!b.alive || b.state != Building::State::Idle || b.house != house) continue;
        for (int t : factory_types) if (b.type == t) return false;   // a survivor
    }
    return true;
}
```

- [ ] **Step 4: Build + run FOREGROUND**, expected PASS (skips clean without the install):

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="World: launch_unit*,World: set_primary*"
```

- [ ] **Step 5: Byte-identity check** — World's new methods are never called by demo-base/econ (they construct no factory launch). Build + run both gates FOREGROUND:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
```

Expected: `hash=5273b5e71ca009e1`, `econ_hash=7fed96cb98b68017`, both `deterministic=yes`.

- [ ] **Step 6: Full suite FOREGROUND both modes.** Expected: all pass, skips clean.
- [ ] **Step 7: Commit** `feat(engine): World factory launch/rally/primary integration [RE: production.md]`.

---

### Task 6: main.cpp — sidebar tabs alive + Allied defense roster

**Files:**
- Modify: `engine/main.cpp`
- Test: none new (UI/render task; verified by the byte-identity gates + a screenshot eyeball). Pure logic added here is exercised by Task 8's `--demo-prod`.

**Interfaces:**
- Consumes: `BuildingAssets`, `sidebar_buildable`, `draw_sidebar`, `SidebarLayout::compute`, `lay.tab_at`, `Rules::is_base_defense` (Task 2), the `kRoster` load loop.
- Produces (main.cpp file-local helpers):

```cpp
std::vector<int> building_buildable_tab(const BuildingAssets& ba, bool defenses);
```

and a generalized `draw_sidebar` taking `int active_tab`.

**Byte-identity anchor:** `sidebar_buildable(ba)` MUST keep returning `{GAPOWR, GAPILE, GAWEAP}` (structures only) so `demo_buildable_closure` — which feeds `--demo-base`'s `buildable=GAPOWR,GAPILE` witness line — is unchanged. Adding GAPILL/GATOW to the roster appends indices 10/11; `spawn_base` places buildings by NAME and never places GAPILL/GATOW, so `--demo-base` `world_hash` is unchanged.

- [ ] **Step 1: Extend the building roster with GAPILL/GATOW.** In `load_building_assets`, change the roster table (currently 10 entries) to 12:

```cpp
    static const char* kRoster[12] = {"GACNST", "GAPOWR", "GAPILE", "GAWEAP",
                                       "NACNST", "NAPOWR", "NAHAND", "NAWEAP",
                                       "GAREFN", "NAREFN", "GAPILL", "GATOW"};
```

The existing `for (const char* id_c : kRoster)` loop now covers all 12. Bump the reservations (harmless): `ba.roster.reserve(12); ba.art.reserve(12);`. GAPILL (pillbox) / GATOW (patriot) are base defenses; missing SHP/cameo just leaves `art[i].ok=false` and drops them from the tab (existing guard), never a crash.

- [ ] **Step 2: Record the defense flag per roster entry.** Add a `std::vector<char> defense;` member to the `BuildingAssets` struct (near `roster`/`art`). In the roster loop, immediately after `ba.roster.push_back(std::move(d));`, append:

```cpp
        ba.defense.push_back(rules.is_base_defense(id) ? 1 : 0);
```

(Keep `defense` index-aligned with `roster`.)

- [ ] **Step 3: Split the buildable helper by tab.** Replace the body of `sidebar_buildable` and add the tab helper:

```cpp
// Building-roster indices for a building tab: GA* player buildings with a
// cameo, tech>0, art ok, split by the recovered base-defense flag
// [RE-GATE APPLIED: production.md §1 item 1]. tab 0 (Structures) = !defense,
// tab 1 (Defenses) = defense. Ordered by tech_level then roster order.
std::vector<int> building_buildable_tab(const BuildingAssets& ba, bool defenses) {
    std::vector<int> v;
    for (int i = 0; i < static_cast<int>(ba.roster.size()); ++i) {
        const ra2engine::BuildingTypeDef& d = ba.roster[i];
        if (!ba.art[i].ok || d.cameo.empty() || d.tech_level <= 0) continue;
        if (d.id.size() < 2 || d.id[0] != 'G' || d.id[1] != 'A') continue;  // Allied player side
        const bool def = i < static_cast<int>(ba.defense.size()) && ba.defense[i] != 0;
        if (def != defenses) continue;
        v.push_back(i);
    }
    std::stable_sort(v.begin(), v.end(),
                     [&](int a, int b) { return ba.roster[a].tech_level < ba.roster[b].tech_level; });
    return v;
}

// Structures tab (tab 0) — the DEMO-facing list; identical to the pre-defense
// output {GAPOWR,GAPILE,GAWEAP}, so demo_buildable_closure is unchanged.
std::vector<int> sidebar_buildable(const BuildingAssets& ba) {
    return building_buildable_tab(ba, /*defenses=*/false);
}
```

- [ ] **Step 4: Generalize `draw_sidebar` for the active tab.** Add an `int active_tab` parameter (append it; update the forward declaration near line 1558 and every call site). Change the tab-highlight loop so the ACTIVE tab is full-bright and the other three shaded:

```cpp
    for (int i = 0; i < 4; ++i) {
        blit_shp(ren, tc, "sb:tab" + std::to_string(i), ba.sb_tabs[i], 0, ba.sb_pal, lay.tabs[i].x,
                 lay.tabs[i].y);
        if (i != active_tab) shade_rect(ren, lay.tabs[i], 110);
    }
```

The `lay` passed in is already computed from the active tab's buildable list, and the `ps` passed is that tab's queue, so the existing cameo grid + overlay loop draws the correct strip unchanged. Update the three existing call sites: `run_screenshot` (pass `/*active_tab=*/0`), `run_interactive` (pass the live `active_tab`, Step 5), `simulate_base`/`simulate_econ` (pass `/*active_tab=*/0` — render-only, no determinism impact).

- [ ] **Step 5: Active-tab state + tab-click switching in `run_interactive`.** Add `int active_tab = 0;` before the loop. The `lay` for the active tab's list must be recomputed when the tab changes; replace the fixed `buildable`/`lay` with a small recompute helper driven by `active_tab`:

```cpp
    int active_tab = 0;
    auto tab_buildable = [&](int tab) -> std::vector<int> {
        if (tab == 0) return building_buildable_tab(ba, /*defenses=*/false);
        if (tab == 1) return building_buildable_tab(ba, /*defenses=*/true);
        return {};   // tabs 2 (Infantry) / 3 (Vehicle): unit roster wired in Task 7
    };
    std::vector<int> buildable = tab_buildable(active_tab);
    ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(platform.width(), platform.height(), buildable);
```

In the sidebar-consume branch (the `if (sidebar_up && ... PointerDown && lay.contains(...))` block), BEFORE the `cameo_at` handling, add tab-click switching:

```cpp
                if (ev.button == ra2engine::InputEvent::Left) {
                    const int tab = lay.tab_at(ev.x, ev.y);
                    if (tab >= 0 && tab != active_tab) {
                        active_tab = tab;
                        buildable = tab_buildable(active_tab);
                        lay = ra2engine::SidebarLayout::compute(platform.width(),
                                                                platform.height(), buildable);
                        continue;   // consumed: switched tab
                    }
                    // ... existing cameo_at start/cancel/place handling ...
                }
```

For tabs 0/1 both view the BUILDING queue (`ps`), so the existing cameo start/cancel/place code is unchanged for the Structures/Defenses tabs — a defense cameo starts a building in the same `ps` and follows the same ready→place flow (defenses ARE placed like buildings, per §1). Tabs 2/3 have an empty `buildable`, so `cameo_at` returns -1 and clicks are inert until Task 7. Pass `active_tab` to `draw_sidebar` at both draw sites in the loop.

- [ ] **Step 6: Build + FOREGROUND byte-identity + eyeball.**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --screenshot "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\prod_task6_sidebar.png"
```

Expected: `--demo-base` `hash=5273b5e71ca009e1` and `buildable=GAPOWR,GAPILE`; `--demo-econ` `econ_hash=7fed96cb98b68017`; the screenshot renders the sidebar with four tabs (tab 0 bright). (Interactively, clicking tab 1 would show GAPILL/GATOW cameos if their assets loaded; the screenshot's HUD is idle tab 0 — note in the report whether GAPILL/GATOW loaded `ok`.)

- [ ] **Step 7: Full suite FOREGROUND both modes.** Expected: all pass.
- [ ] **Step 8: Commit** `feat(engine): sidebar tabs alive + Allied defense roster (GAPILL/GATOW) [RE: production.md]`.

---

### Task 7: main.cpp unit roster + assets + exit handoff + input wiring

**Files:**
- Modify: `engine/main.cpp`, `engine/input.hpp`, `engine/input.cpp`
- Test: none new (integration; the deterministic proof is Task 8's `--demo-prod`).

**Interfaces:**
- Consumes: `UnitTypeDef` (unit_type.hpp), `ProductionQueues`/`QueueCat`/`can_start_unit`/`start_unit` (production.hpp), `World::launch_unit`/`set_rally`/`set_primary`/`factory_category_lost` (Task 5), `primary_factory` (factory.hpp), `load_vehicle_assets`, `resolve_combat_unit`, `build_speed_multiplier`, `power_totals`, `conyard_alive`, `roster_index`, `building_at_cell`.
- Produces (main.cpp file-local):

```cpp
struct UnitRoster {
    std::vector<ra2engine::UnitTypeDef> defs;   // index = the unit-queue slot type
    std::vector<VehicleAssets> vehicles;        // per vehicle def (aligned to defs where is_vehicle)
    // infantry SHP/anim reuse the existing UnitAssets pipeline; combat comes
    // from UnitTypeDef::combat, so rendering needs no per-def infantry store.
};
UnitRoster load_unit_roster(const std::string& dir, const ra2assets::MixFile& ra2,
                            const UnitAssets& ua, const Rules& rules, const Art& art);
std::vector<int> factory_types_for(const BuildingAssets& ba, const char* id);
double factory_speedup(const ra2engine::World& world, const std::vector<int>& ftypes, double mf);
void prod_tick(ra2engine::ProductionQueues& q, ra2engine::World& world, const BuildingAssets& ba,
               double dt, double low_mod, double mf, const std::vector<int>& inf_ft,
               const std::vector<int>& veh_ft);
```

**Roster** [RE-GATE APPLIED: production.md §6 prereqs]: infantry `E1` (GI) + one more (`GGI`); vehicles `MTNK` (Grizzly) + `FV` (IFV). Every entry opt-guarded — a def whose combat/cameo/assets fail to resolve is DROPPED from `defs` (never a crash). Prereq tokens: infantry `{BARRACKS}` (or the recovered `GAPILE`), vehicles `{FACTORY}` (or `GAWEAP`). [RE-FALLBACK] if the hunt pins per-unit ids, amend the tokens.

- [ ] **Step 1: Load the unit roster.** Add `load_unit_roster` near `load_miner_assets`:

```cpp
UnitRoster load_unit_roster(const std::string& dir, const ra2assets::MixFile& ra2,
                            const UnitAssets& ua, const Rules& rules, const Art& art) {
    UnitRoster ur;
    // Curated Allied producibles [RE-GATE APPLIED: production.md §1/§6].
    struct Ent { const char* id; bool vehicle; const char* group; };
    static const Ent kUnits[] = {
        {"E1",  false, "BARRACKS"}, {"GGI", false, "BARRACKS"},
        {"MTNK", true, "FACTORY"},  {"FV",  true, "FACTORY"},
    };
    for (const Ent& e : kUnits) {
        try {
            ra2engine::UnitTypeDef d;
            d.id = e.id;
            d.cost = rules.cost(e.id);
            d.tech_level = rules.tech_level(e.id);
            d.is_vehicle = e.vehicle;
            d.prerequisite = {e.group};
            d.cameo = art.cameo(e.id);
            if (d.cost <= 0 || d.tech_level <= 0 || d.cameo.empty()) continue;  // not producible
            if (e.vehicle) {
                VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, e.id);
                if (!va.ok) continue;                 // assets missing -> drop
                d.combat = va.combat;
                ur.vehicles.push_back(std::move(va));
            } else {
                const ResolvedUnit ru = resolve_combat_unit(rules, e.id);  // infantry combat + SHP image
                d.combat = ru.combat;
                d.combat.is_vehicle = false;
                d.combat.speed_cps = ru.speed * kCellsPerSecPerSpeed;
            }
            ur.defs.push_back(std::move(d));
        } catch (const std::exception& ex) {
            std::cerr << "[unit-roster] " << e.id << " unavailable (" << ex.what() << ")\n";
        }
    }
    std::cerr << "[unit-roster] producible=" << ur.defs.size() << "\n";
    return ur;
}
```

(Cameo SHPs for unit ids load from `cameo.mix` exactly like building cameos; if the unit-tab cameo blit needs a cache, add a `has_cameo`/`ShpFile` alongside — reuse the building cameo load path keyed by unit id.)

- [ ] **Step 2: Factory category + speedup helpers.**

```cpp
std::vector<int> factory_types_for(const BuildingAssets& ba, const char* id) {
    std::vector<int> v;
    const int t = roster_index(ba, id);
    if (t >= 0) v.push_back(t);
    return v;
}

// Alive Idle owned factories of this category compound the build speed
// [RE-GATE APPLIED: production.md §1 item 2]: time scales by mf^(n-1), so the
// tick SPEED multiplier is mf^-(n-1). n<=1 -> 1.0. [RE-FALLBACK] mf value.
double factory_speedup(const ra2engine::World& world, const std::vector<int>& ftypes, double mf) {
    int n = 0;
    for (const ra2engine::Building& b : world.buildings()) {
        if (!b.alive || b.state != ra2engine::Building::State::Idle || b.house != 0) continue;
        for (int t : ftypes) if (b.type == t) { ++n; break; }
    }
    if (n <= 1 || mf <= 0.0) return 1.0;
    return std::pow(1.0 / mf, n - 1);
}
```

- [ ] **Step 3: prod_tick.**

```cpp
void prod_tick(ra2engine::ProductionQueues& q, ra2engine::World& world, const BuildingAssets& ba,
               double dt, double low_mod, double mf, const std::vector<int>& inf_ft,
               const std::vector<int>& veh_ft) {
    const ra2engine::PowerTotals pt = ra2engine::power_totals(world.buildings(), ba.roster, 0);
    const double pmul = ra2engine::build_speed_multiplier(pt, low_mod);
    const double sp[3] = {1.0, factory_speedup(world, inf_ft, mf), factory_speedup(world, veh_ft, mf)};
    q.tick(dt, pmul, sp, ba.roster);
    // ConYard-died cancels the Building queue (existing rule, via conyard_alive).
    if (q.building().slot.type != -1 && !conyard_alive(world, ba, 0))
        ra2engine::cancel(q.building());
    // Factory-died cancels+refunds the matching unit queue [RE: production.md §4].
    if (q.producing(ra2engine::QueueCat::Infantry) != -1 && world.factory_category_lost(inf_ft, 0))
        q.cancel(ra2engine::QueueCat::Infantry);
    if (q.producing(ra2engine::QueueCat::Vehicle) != -1 && world.factory_category_lost(veh_ft, 0))
        q.cancel(ra2engine::QueueCat::Vehicle);
    q.reconcile_refund();   // a refund raised one queue above the pool tail
}
```

- [ ] **Step 4: Wire ProductionQueues into `run_interactive`.** Replace the bare `ProductionState ps` with `ProductionQueues queues;` where `queues.building()` IS the old building queue. Load the unit roster + factory type lists + MultipleFactory:

```cpp
    ra2engine::ProductionQueues queues;
    double player_credits = rules.mp_money();
    queues.set_credits(player_credits);
    double credits_shown = player_credits;
    UnitRoster ur = load_unit_roster(dir, ra2, ua, rules, art);
    const std::vector<int> inf_ft = factory_types_for(ba, "GAPILE");
    const std::vector<int> veh_ft = factory_types_for(ba, "GAWEAP");
    const double mf = rules.general_double("MultipleFactory", 0.8);  // [RE-GATE §1 item 2]
```

Per frame (replacing the old `ps_tick` + income block):

```cpp
        queues.set_credits(player_credits);
        prod_tick(queues, world, ba, dt, low_mod, mf, inf_ft, veh_ft);
        // Read the pool back (reconcile_refund set all three equal):
        player_credits = queues.building().credits;
        player_credits += world.take_income(0);      // harvest income (economy)

        // Exit handoff: any ready unit queue launches from its primary factory.
        for (ra2engine::QueueCat cat : {ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle}) {
            if (!queues.ready(cat)) continue;
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Infantry) ? inf_ft : veh_ft;
            const int fi = ra2engine::primary_factory(world.buildings(), ft, 0);
            if (fi < 0) continue;                    // no factory (queue was cancelled elsewhere)
            const int uslot = queues.producing(cat);
            if (uslot < 0 || uslot >= (int)ur.defs.size()) { queues.placed(cat); continue; }
            const ra2engine::BuildingTypeDef& fdef = ba.roster[world.buildings()[fi].type];
            const int ui = world.launch_unit(fi, fdef, ur.defs[uslot].combat);
            if (ui >= 0) {
                // register the unit's anim so infantry animate (vehicles ignore it)
                world.set_anim(ur.defs[uslot].combat.image,
                               ua.anims.count(ur.defs[uslot].combat.image)
                                   ? ua.anims.at(ur.defs[uslot].combat.image)
                                   : ra2engine::default_anim());
                queues.placed(cat);                  // slot cleared; unit is on the field
            }
            // else exit blocked -> HOLD ready this frame (retry next frame)
        }
```

The building-queue placement flow (cameo Ready→`begin_placement`→confirm→`add_building`→`placed`) now targets `queues.building()`: replace `ps` with `queues.building()` in the sidebar-consume branch and the placement-confirm branch, and after any building placement do `queues.set_credits(player_credits)` if that path mutated credits (it does not — the queue already spent during tick; placement only clears the slot). `draw_sidebar` now receives the visible tab's ProductionState: pass `queues.building()` for tabs 0/1 and `queues.state(cat)` for tabs 2/3.

- [ ] **Step 5: Unit-tab cameo start (main.cpp sidebar branch).** In the `tab_buildable` lambda, return the unit-roster indices for tabs 2/3:

```cpp
    auto tab_buildable = [&](int tab) -> std::vector<int> {
        if (tab == 0) return building_buildable_tab(ba, false);
        if (tab == 1) return building_buildable_tab(ba, true);
        std::vector<int> v;   // tabs 2 (Infantry) / 3 (Vehicle): unit-roster indices
        const bool want_veh = (tab == 3);
        for (int i = 0; i < (int)ur.defs.size(); ++i)
            if (ur.defs[i].is_vehicle == want_veh) v.push_back(i);
        return v;
    };
```

In the cameo-click handler, branch on the active tab: tabs 0/1 keep the existing building start/cancel/place; tabs 2/3 start a unit build in the matching queue:

```cpp
                    if (active_tab >= 2) {
                        const ra2engine::QueueCat cat = (active_tab == 3)
                            ? ra2engine::QueueCat::Vehicle : ra2engine::QueueCat::Infantry;
                        const int uslot = buildable[ci];   // unit-roster index
                        if (queues.producing(cat) == uslot) {
                            queues.cancel(cat);            // click the building cameo -> cancel
                            queues.reconcile_refund();
                            player_credits = queues.building().credits;
                        } else if (ra2engine::can_start_unit(
                                       queues.state(cat), uslot, ur.defs, world.buildings(),
                                       ba.roster, 0, ba.groups,
                                       !world.factory_category_lost(
                                           cat == ra2engine::QueueCat::Vehicle ? veh_ft : inf_ft, 0))) {
                            queues.set_credits(player_credits);
                            queues.start_unit(cat, uslot, ur.defs, build_speed);
                        }
                    } else {
                        // ... existing building tab 0/1 start/cancel/place on queues.building() ...
                    }
```

`SidebarLayout::compute` is re-run when switching to a unit tab so the cameo cells reference unit-roster indices — `compute` already takes whatever `types` list `tab_buildable` returns; the cameo blit for unit ids uses the unit cameo load. The `can_start`/overlay path in `draw_sidebar` keys on `ps.slot.type == type`; for unit tabs pass `queues.state(cat)` so the clock/ready overlay tracks the unit build. Guard the disabled-shade `can_start` call (which is building-roster-based) so it does not misfire on unit indices: since `active_tab` is already a `draw_sidebar` parameter, change the disabled branch from `else if (!can)` to `else if (active_tab < 2 && !can)` — unit tabs simply skip the disabled shade (cosmetic). No new parameter is needed.

- [ ] **Step 6: input.cpp — factory selection, rally, primary.** Add to `input.hpp` (private) `int selected_building_ = -1;` and (public) `int selected_building() const { return selected_building_; }`, plus the unit-factory type list setter `void set_unit_factory_types(std::vector<int> t) { unit_factory_types_ = std::move(t); }` with `std::vector<int> unit_factory_types_;`. In `input.cpp`:

- **Left-click** resolution (in `commit_click` / the left Down path): before the unit pick, if the click lands on an own (house 0) alive building whose type is in `unit_factory_types_` (`world_.building_at_cell` → type test), set `selected_building_ = that index` (and clear unit selection); a second left-click on the ALREADY-selected factory triggers `world_.set_primary(selected_building_, factory_types_of_that_category)` [RE-GATE APPLIED: production.md §1 item 2 — primary-set trigger; [RE-FALLBACK adjudicated]: re-click the selected factory]. A left-click elsewhere clears `selected_building_`.
- **Right-click** (the existing `InputEvent::Right` branch, BEFORE the harvester/attack resolution): if `selected_building_ >= 0` and the click resolves to own ground, call `world_.set_rally(selected_building_, cx, cy)` and consume [RE-GATE APPLIED: production.md §4 — rally set trigger; [RE-FALLBACK adjudicated]: right-click ground with a factory selected].

Because `set_primary` needs the category's factory-type list, store BOTH lists and pick by the selected building's type. Add `void set_infantry_factory_types(...)`/`set_vehicle_factory_types(...)` or a single combined list keyed by lookup — simplest: store `inf_ft`/`veh_ft` and in `set_primary`-trigger pass whichever contains the selected type. main.cpp calls `input.set_unit_factory_types(...)` with the union for selection, and the rally/primary handlers resolve the specific category from the selected building's type against the two stored lists.

- [ ] **Step 7: Render chrome (main.cpp).** After `draw_sidebar`, draw (all [RE-FALLBACK]/divergence-tagged like the READY text):
  - a primary-factory marker (small filled triangle) over each `primary` unit-factory of house 0 — iterate `world.buildings()`, `b.primary` → draw at the foundation screen center (reuse `draw_building`'s bbox-center math);
  - a rally marker (an X or diamond) at `(rally_x, rally_y)` for `input.selected_building()` when it has_rally, via `cell_to_screen` + `cam`.
  These are cosmetic; they never touch sim state.

- [ ] **Step 8: FOREGROUND build + byte-identity + interactive eyeball.**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --screenshot "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\prod_task7.png"
```

Expected: `--demo-base` `hash=5273b5e71ca009e1`, `--demo-econ` `econ_hash=7fed96cb98b68017` (the demos still use their own `ProductionState ps`; they never touch `ProductionQueues`/`UnitRoster`). Screenshot renders without crash. Report the `[unit-roster] producible=N` count.

- [ ] **Step 9: Full suite FOREGROUND both modes.** Expected: all pass.
- [ ] **Step 10: Commit** `feat(engine): unit roster + assets + exit handoff + rally/primary input [RE: production.md]`.

---

### Task 8: --demo-prod deterministic gate

**Files:**
- Modify: `engine/main.cpp`
- Test: none new (the demo IS the test; a permanent byte-identity gate).

**Interfaces:**
- Consumes: `simulate_econ`/`run_demo_econ` as the structural template; `ProductionQueues`, `UnitRoster`/`load_unit_roster`, `prod_tick`, `factory_types_for`, `primary_factory`, `World::launch_unit`/`set_rally`/`set_primary`, `world_hash`, `fnv_mix`, `spawn_base`, `place_player_building`.
- Produces: `uint64_t prod_hash(...)`, `DemoProdResult`, `simulate_prod(...)`, `int run_demo_prod()`; a `--demo-prod` arg + dispatch in `main()`.

**Byte-identity anchor:** `prod_hash` is a NEW function; `world_hash` is UNTOUCHED. `--demo-prod` constructs its own `ProductionQueues`; the four existing gates never do.

- [ ] **Step 1: prod_hash** (near `econ_hash`):

```cpp
// FNV extension of world_hash with each queue's state + spawned unit types +
// credits, for --demo-prod's two-run byte-identity gate. world_hash UNTOUCHED.
uint64_t prod_hash(const ra2engine::World& world, const ra2engine::ProductionQueues& q,
                   double credits) {
    uint64_t h = world_hash(world);
    for (ra2engine::QueueCat c : {ra2engine::QueueCat::Building, ra2engine::QueueCat::Infantry,
                                  ra2engine::QueueCat::Vehicle}) {
        const ra2engine::ProductionState& ps = q.state(c);
        h = fnv_mix(h, static_cast<uint64_t>(ps.slot.type));
        h = fnv_mix(h, static_cast<uint64_t>(ps.slot.stage));
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(ps.slot.spent)));
    }
    for (const ra2engine::Unit& u : world.units())   // spawned unit types (cells: world_hash)
        for (char ch : u.image) h = fnv_mix(h, static_cast<uint64_t>((unsigned char)ch));
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(credits)));
    return h;
}
```

- [ ] **Step 2: simulate_prod.** Mirror `simulate_econ` (fixed 15 Hz dt, frame-driven, rendered + headless byte-identical). Player Allied base + instant GAPOWR/GAPILE/GAWEAP (so both unit factories exist and PROC is met). Script + witnesses:

```cpp
struct DemoProdResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

DemoProdResult simulate_prod(Assets& a, UnitAssets& ua, BuildingAssets& ba, UnitRoster& ur,
                             const Rules& rules, double build_speed, double low_mod, int mp_money,
                             const std::vector<const VehicleAssets*>& vlist, const OreArt& ore_art,
                             double mf, bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                             const ra2engine::Camera* cam, int W, int H,
                             const std::string& start_png, const std::string& end_png) {
    DemoProdResult r;
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 6);

    const int gacnst = roster_index(ba, "GACNST");
    const int gapowr = roster_index(ba, "GAPOWR");
    const int gapile = roster_index(ba, "GAPILE");
    const int gaweap = roster_index(ba, "GAWEAP");
    const int cy_bi = find_building(world, gacnst, 0);
    const ra2engine::Building cy_b = cy_bi >= 0 ? world.buildings()[cy_bi] : ra2engine::Building{};
    { int ox = 0, oy = 0;
      if (!place_player_building(world, ba, gapowr, cy_b.cx + cy_b.fw, cy_b.cy, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gapile, cy_b.cx, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gaweap, cy_b.cx + cy_b.fw, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false; }

    const std::vector<int> inf_ft = factory_types_for(ba, "GAPILE");
    const std::vector<int> veh_ft = factory_types_for(ba, "GAWEAP");
    // Unit-roster slots for MTNK (Grizzly) and E1 (GI).
    int mtnk_slot = -1, gi_slot = -1;
    for (int i = 0; i < (int)ur.defs.size(); ++i) {
        if (ur.defs[i].id == "MTNK") mtnk_slot = i;
        if (ur.defs[i].id == "E1") gi_slot = i;
    }
    if (mtnk_slot < 0 || gi_slot < 0) { r.ok = false; r.witness.emplace_back("[prod] roster MISSING MTNK/E1"); }

    ra2engine::ProductionQueues q;
    double credits = mp_money;
    q.set_credits(credits);
    const ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(W, H, sidebar_buildable(ba));

    const double dt = 1.0 / 15.0, cap = 600.0;
    long frame = 0; double t = 0.0;
    auto step_one = [&]() {
        world.step(dt);
        q.set_credits(credits);
        prod_tick(q, world, ba, dt, low_mod, mf, inf_ft, veh_ft);
        credits = q.building().credits;
        credits += world.take_income(0);
        (void)world.take_harvest_events();
        // Exit handoff (same as run_interactive).
        for (ra2engine::QueueCat cat : {ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle}) {
            if (!q.ready(cat)) continue;
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Infantry) ? inf_ft : veh_ft;
            const int fi = ra2engine::primary_factory(world.buildings(), ft, 0);
            if (fi < 0) continue;
            const int uslot = q.producing(cat);
            if (uslot < 0 || uslot >= (int)ur.defs.size()) { q.placed(cat); continue; }
            const ra2engine::BuildingTypeDef& fdef = ba.roster[world.buildings()[fi].type];
            if (world.launch_unit(fi, fdef, ur.defs[uslot].combat) >= 0) q.placed(cat);
        }
        ++frame; t += dt;
        if (render) { SDL_SetRenderDrawColor(ren, 10, 20, 60, 255); SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
            if (ba.sidebar_ok) draw_sidebar(ren, *tc, ba, lay, q.building(), world,
                                            static_cast<int>(std::llround(credits)), t, 0); }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }

    // W1: queue a Grizzly in the Vehicle queue (frames from raw cost, charge==cost).
    while (frame < 15) step_one();
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Vehicle, mtnk_slot, ur.defs, build_speed);
    { char b[128]; std::snprintf(b, sizeof b, "[prod] start MTNK frames=%d step_ticks=%d charge=%d",
        q.state(ra2engine::QueueCat::Vehicle).slot.total_frames,
        q.state(ra2engine::QueueCat::Vehicle).slot.step_ticks,
        q.state(ra2engine::QueueCat::Vehicle).slot.charge_basis); r.witness.emplace_back(b); }

    // Set a rally for the war factory before the tank completes.
    const int weap_bi = find_building(world, gaweap, 0);
    if (weap_bi >= 0) {
        const ra2engine::Building wb = world.buildings()[weap_bi];
        world.set_rally(weap_bi, wb.cx + wb.fw + 5, wb.cy + wb.fh);
    }
    // Also queue a GI concurrently -> witness both queues advancing.
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Infantry, gi_slot, ur.defs, build_speed);
    { char b[96]; std::snprintf(b, sizeof b, "[prod] concurrent GI stage=%d MTNK stage=%d",
        q.state(ra2engine::QueueCat::Infantry).slot.stage,
        q.state(ra2engine::QueueCat::Vehicle).slot.stage); r.witness.emplace_back(b); }

    const int units_before = (int)world.units().size();
    while (q.producing(ra2engine::QueueCat::Vehicle) != -1 && t < cap) step_one();  // to exit
    // The freshly launched Grizzly is the last unit; witness its exit cell + rally path.
    if ((int)world.units().size() > units_before) {
        const ra2engine::Unit& tank = world.units().back();
        const ra2engine::Building wb = world.buildings()[weap_bi];
        const ra2engine::Cellxy ex = ra2engine::exit_cell(wb, ba.roster[wb.type]);
        char b[160]; std::snprintf(b, sizeof b,
            "[prod] MTNK exit cell=(%d,%d) expected=(%d,%d) moving=%d credits=%d",
            tank.cell_x, tank.cell_y, ex.x, ex.y, tank.moving() ? 1 : 0,
            static_cast<int>(std::llround(credits)));
        r.witness.emplace_back(b);
        if (tank.cell_x != ex.x || tank.cell_y != ex.y) r.ok = false;
    } else { r.witness.emplace_back("[prod] MTNK never exited"); r.ok = false; }

    // Second war factory + set_primary -> the NEXT tank exits from it.
    int gx = 0, gy = 0;
    place_player_building(world, ba, gaweap, cy_b.cx + cy_b.fw + 4, cy_b.cy + cy_b.fh, gx, gy);
    int w2 = -1;   // locate the SECOND war factory (any alive GAWEAP != weap_bi)
    for (int i = 0; i < (int)world.buildings().size(); ++i)
        if (world.buildings()[i].alive && world.buildings()[i].type == gaweap && i != weap_bi) w2 = i;
    if (w2 >= 0) world.set_primary(w2, veh_ft);
    { char b[96]; std::snprintf(b, sizeof b, "[prod] primary factory now idx=%d (2nd war factory=%d)",
        ra2engine::primary_factory(world.buildings(), veh_ft, 0), w2); r.witness.emplace_back(b); }
    const int u2_before = (int)world.units().size();
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Vehicle, mtnk_slot, ur.defs, build_speed);
    while (q.producing(ra2engine::QueueCat::Vehicle) != -1 && t < cap) step_one();
    if ((int)world.units().size() > u2_before) {
        const ra2engine::Unit& tank2 = world.units().back();
        const ra2engine::Building w2b = world.buildings()[w2 >= 0 ? w2 : weap_bi];
        const ra2engine::Cellxy ex2 = ra2engine::exit_cell(w2b, ba.roster[w2b.type]);
        char b[128]; std::snprintf(b, sizeof b, "[prod] 2nd MTNK exit cell=(%d,%d) from primary=(%d,%d)",
            tank2.cell_x, tank2.cell_y, ex2.x, ex2.y); r.witness.emplace_back(b);
    }

    // Finish the GI queue too, so both categories produced.
    while (q.producing(ra2engine::QueueCat::Infantry) != -1 && t < cap) step_one();
    for (int k = 0; k < 30; ++k) step_one();   // settle motion

    r.hash = prod_hash(world, q, credits);
    { char b[64]; std::snprintf(b, sizeof b, "[prod] prod_hash=%016llx",
        static_cast<unsigned long long>(r.hash)); r.witness.emplace_back(b); }
    if (t >= cap) r.ok = false;
    if (render) { step_one(); capture_png(ren, end_png, W, H); }
    return r;
}
```

- [ ] **Step 3: run_demo_prod.** Mirror `run_demo_econ`: load assets (fail clearly if `ba`/miners/unit roster unavailable — require MTNK + E1 producible), load `UnitRoster`, center the camera on the player base, run rendered (hidden window + start/end PNGs) then headless, byte-compare `r1.witness == r2.witness && r1.hash == r2.hash`, print each witness + `deterministic=yes/no lines=N`, exit 0 iff identical AND both `ok`. Add `mf = rules.general_double("MultipleFactory", 0.8)`.

- [ ] **Step 4: Dispatch.** Add `bool demo_prod = false;` + the `--demo-prod` arg parse + `if (demo_prod) return run_demo_prod();` in `main()` (place beside `--demo-econ`).

- [ ] **Step 5: FOREGROUND — build + run ALL FIVE demos, assert every pin.**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
```

Expected: `--demo-combat` `deterministic=yes logs=78`; `--demo-tank` `deterministic=yes logs=144`; `--demo-base` `hash=5273b5e71ca009e1`; `--demo-econ` `econ_hash=7fed96cb98b68017` `deterministic=yes`; `--demo-prod` `deterministic=yes` with the MTNK frames/charge, exit-cell, concurrent GI, second-factory primary, and `prod_hash` witnesses all present and `ok`. Record the fresh `prod_hash` in the task report. Run `--demo-prod` TWICE and confirm the same `prod_hash` both runs (stability).

- [ ] **Step 6: Commit** `feat(engine): --demo-prod deterministic gate + prod_hash [RE: production.md]`.

---

### Task 9: Finalize — full suite both modes, arm64 build, README

**Files:**
- Modify: `README.md`; `android/app/jni/CMakeLists.txt` ONLY if it lists engine sources beyond `cmake/ra2_sources.cmake` (verify — `factory.cpp` should flow in via the shared list; `unit_type.hpp` is header-only, no source entry needed).

- [ ] **Step 1: Full test suite, both modes, FOREGROUND.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: 0 failed in both; install-gated cases (`rules:` real pins, `World: launch_unit*`, etc.) skip clean when the dir is empty/absent.

- [ ] **Step 2: All five demos FOREGROUND, verify the four legacy pins byte-identical + record prod_hash.**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
```

Expected: `logs=78` / `logs=144` / `hash=5273b5e71ca009e1` / `econ_hash=7fed96cb98b68017` / `--demo-prod deterministic=yes`. Run `--demo-prod` a second time and confirm identical `prod_hash` (two-run stability). Record the pinned `prod_hash` for the README gate table.

- [ ] **Step 3: arm64 NDK build FOREGROUND.**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Then verify the new engine symbols linked into the arm64 `libmain.so` (nm-verify, mirroring the prior phases' check — confirm `exit_cell`, `primary_factory`, `set_primary`, `ProductionQueues`, `launch_unit`, `can_start_unit` are present):

```powershell
& "D:\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe" `
  (Get-ChildItem -Recurse -Filter libmain.so D:\ra2-android\android\app\build | Where-Object { $_.FullName -match "arm64" } | Select-Object -First 1).FullName `
  | Select-String -Pattern "primary_factory|exit_cell|ProductionQueues|launch_unit|can_start_unit"
```

Expected: at least one demangled/mangled match per symbol (nonempty output).

- [ ] **Step 4: README — production section.** Add a "Unit Production" section after the Economy section documenting: the four sidebar tabs coming alive, the 3-queue model (tabs 0/1 share the Building queue), the Allied defense (GAPILL/GATOW) + unit (GI/GGI/MTNK/FV, whichever loaded) roster, primary factory + MultipleFactory speedup, exit cell + rally. Add the new permanent gate line to the gate table:

```
| --demo-prod | prod_hash=<pinned value from Step 2> | deterministic=yes; MTNK frames/charge, exit cell, concurrent GI, 2nd-factory primary routing |
```

Add a "Deviations we will take" list capturing every `[RE-FALLBACK]` adjudication that survived the hunt: the `exit_cell` foundation-relative offset (if not a pinned per-type coord), the default gather cell, the `MultipleFactory` value/compounding if it differed, the rally-set + primary-set input triggers, and any leave-factory exit animation replaced by an instant spawn-at-exit.

- [ ] **Step 5: Commit** `docs: production phase README + --demo-prod gate pin`.

---

## Self-Review (performed at write time)

1. **Spec coverage:**
   - §1 (Ghidra hunt, all six items) → Task 1; each item cited as `[RE-GATE APPLIED: production.md §1 item N]` at its consuming task (classifier→Task 2/6; primary+MultipleFactory→Task 3/7; exit cell→Task 4; rally→Task 5; unit build economy→Task 3; per-unit prereqs→Task 7).
   - §2 (roster & rules: `build_category`/`is_infantry`/`is_vehicle`, `UnitTypeDef`) → Task 2 (accessors) + Task 3 (`UnitTypeDef`) + Task 7 (roster assembled by main.cpp).
   - §3 (multi-queue: scalar `start`, `can_start_unit`, `ProductionQueues`, 54-step economy, concurrency, speedup, prereq gating, BuildingType-path-unchanged) → Task 3 (all tests present).
   - §4 (factory exit/rally/primary, Building fields, World integration, factory-died cancel) → Task 4 (pure module + fields) + Task 5 (World).
   - §5 (sidebar tabs, roster+assets, rendering chrome, input, `--demo-prod`, four gates byte-identical, finalize) → Task 6 (tabs+defenses) + Task 7 (unit roster+assets+exit+input+chrome) + Task 8 (`--demo-prod`) + Task 9 (finalize/gates/arm64/README).
2. **Placeholder scan:** `[RE-GATE APPLIED]`/`[RE-FALLBACK]` markers are the phase's designed amendment gates with explicit adjudicated fallbacks, not TBDs. Task 8's `prod_hash` pin is an extract-then-pin by design (economy-gate precedent). No "add error handling"/"similar to Task N"/bare-TODO strings.
3. **Type/signature consistency across tasks:**
   - `UnitTypeDef` fields (`id`, `cost`, `tech_level`, `is_vehicle`, `chrono_miner`, `prerequisite`, `cameo`, `combat`) defined in Task 3, consumed identically in Tasks 7/8.
   - `ProductionQueues` API (`building()`, `state(QueueCat)`, `set_credits`, `credits()`, `tick(dt,power_mult,double[3],btypes)`, `reconcile_refund()`, `start_unit(cat,type,roster,speed)`, `cancel(cat)`, `ready(cat)`, `producing(cat)`, `placed(cat)`) defined in Task 3, called with matching signatures in Tasks 7/8. `QueueCat{Building,Infantry,Vehicle}` consistent.
   - `can_start_unit(ps, unit_type, unit_roster, buildings, btypes, house, groups, factory_alive)` — Task 3 definition matches Task 7 call.
   - `exit_cell(const Building&, const BuildingTypeDef&) -> Cellxy`, `primary_factory(buildings, factory_types, house) -> int`, `set_primary(buildings, factory_types, b_idx)` — Task 4 definitions match Tasks 5/7/8 calls (the abstract "category" param is realized as the roster-index list `factory_types`, documented in Task 4's header).
   - `World::set_rally(b_idx,x,y)`, `World::set_primary(b_idx, factory_types)`, `World::launch_unit(factory_idx, factory_def, combat) -> int`, `World::factory_category_lost(factory_types, house) -> bool` — Task 5 definitions match Tasks 7/8 calls.
   - Byte-identity: `production.cpp` `tick`/`cancel`/`placed` untouched (Task 3 only refactors `start` into scalar+wrapper, bit-identical); `Building`'s new fields excluded from `world_hash` (Task 4 Step 5 verifies); `sidebar_buildable` output preserved (Task 6 Step 3); demos keep their own `ProductionState ps` (Tasks 6–8); `prod_hash` is new, `world_hash` unchanged (Task 8 Step 1). All four legacy gates asserted in Tasks 3, 4, 5, 6, 7, 8, 9.

