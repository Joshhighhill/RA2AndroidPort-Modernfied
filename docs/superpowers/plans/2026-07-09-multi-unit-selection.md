# Phase A: Multi-Unit Selection & Group Movement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the engine's single controllable unit into a controllable army: multiple GI units, RA2-classic selection (click / box / Shift / double-click), and right-click group movement with grid-occupancy collision avoidance.

**Architecture:** Approach 1 — extend the existing engine classes in place. The two tricky algorithms (formation target assignment, and the per-step movement + collision "reservation" stepper) are factored into **pure free-function / plain-class modules** compiled into *both* `ra2assets_tests` and `ra2engine` from a single source of truth, exactly like `engine/iso_project.cpp`, `engine/pathfind.cpp`, and `engine/world.cpp`. `Unit` stays a plain data struct; movement/selection/collision are pure functions over unit data, so a future combat phase can migrate to an ECS mechanically.

**Tech Stack:** C++17, CMake + Ninja + MinGW-w64 (host), doctest, SDL2 (engine only). Android arm64 via NDK r27c reuses `cmake/ra2_sources.cmake` with no jni changes.

## Global Constraints

- **Run test executables from PowerShell, not Git Bash** — Git's `/mingw64` DLLs shadow the WinLibs runtime and the exe exits 127. Build command: `cmake --build D:\ra2-android\build --target ra2assets_tests`. Run: `& D:\ra2-android\build\ra2assets_tests.exe`.
- **Pure engine modules live under `engine/`** and are added to BOTH `RA2_ENGINE_SOURCES` in `cmake/ra2_sources.cmake` (feeds `ra2engine` + the Android `libmain.so`) AND `ra2assets_tests` in the top-level `CMakeLists.txt` (with their `test_*.cpp`), so they cannot drift and stay unit-tested without SDL. No SDL/OS headers in these modules.
- **No game assets, screenshots, or binaries are committed.** Real-asset tests are guarded by `RA2_INSTALL_DIR` (default `D:\Red Alert 2`) and must **skip cleanly** (print a `MESSAGE` and `return`) when the install is absent — never fail. Demo screenshots go only to the scratchpad.
- **Determinism:** units are stepped and re-pathed in fixed ascending index order; A\* tie-breaks are already deterministic (`engine/pathfind.cpp`). No wall-clock or RNG in engine logic; double-click timing uses the event-carried `t_ms` timestamp.
- **Hard invariant:** no two units ever occupy the same cell. Reservation of the next cell before entry enforces this; tests assert it every step.
- **Tile geometry constants** (`kTileW=60`, `kTileH=30`, `kLevelPx=15`) are duplicated across `iso_renderer.cpp` / `input.cpp` / `world.cpp` today (a known Minor); match the existing values, do not add a new copy pattern.
- **Scope:** GI infantry only. No combat, health/death, second unit type, voxel rendering, touch-control adaptation, minimap, or HUD (spec §Non-goals).

---

### Task 1: `Occupancy` cell-ownership grid (pure module)

**Files:**
- Create: `engine/occupancy.hpp`
- Create: `engine/occupancy.cpp`
- Create: `tests/test_occupancy.cpp`
- Modify: `CMakeLists.txt` (add test + source), `cmake/ra2_sources.cmake` (add engine source)

**Interfaces:**
- Produces: `class ra2engine::Occupancy` with `kEmpty = -1`; ctor `Occupancy(int width, int height)`; `int width()`, `int height()`; `int at(int x,int y) const` (owner unit index or `kEmpty`, `kEmpty` off-grid); `bool is_free(int x,int y) const`; `void set(int x,int y,int unit)`; `void clear(int x,int y)`; `void release(int x,int y,int unit)` (clears only if currently owned by `unit`); `void release_all_of(int unit, int keep_x, int keep_y)` (clears every cell owned by `unit` except the kept one — stale-reservation cleanup on re-order); `bool can_enter(int x,int y,int unit) const` (in-grid AND (free OR owned by `unit`)).

- [ ] **Step 1: Write the failing test** — create `tests/test_occupancy.cpp`:

```cpp
#include "doctest.h"

#include "occupancy.hpp"

using ra2engine::Occupancy;

TEST_CASE("Occupancy: default grid is all empty and off-grid reads are empty") {
    Occupancy occ(4, 3);
    CHECK(occ.width() == 4);
    CHECK(occ.height() == 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x) {
            CHECK(occ.at(x, y) == Occupancy::kEmpty);
            CHECK(occ.is_free(x, y));
        }
    // Off-grid is never owned and never enterable.
    CHECK(occ.at(-1, 0) == Occupancy::kEmpty);
    CHECK(occ.at(4, 0) == Occupancy::kEmpty);
    CHECK_FALSE(occ.is_free(-1, 0));   // off-grid is not "free" to enter
    CHECK_FALSE(occ.can_enter(4, 0, 7));
}

TEST_CASE("Occupancy: set / at / is_free / can_enter") {
    Occupancy occ(4, 3);
    occ.set(1, 1, 5);
    CHECK(occ.at(1, 1) == 5);
    CHECK_FALSE(occ.is_free(1, 1));
    CHECK(occ.is_free(2, 1));
    // Owner can re-enter its own cell; a different unit cannot.
    CHECK(occ.can_enter(1, 1, 5));
    CHECK_FALSE(occ.can_enter(1, 1, 6));
    CHECK(occ.can_enter(2, 1, 6));       // free cell
    CHECK_FALSE(occ.can_enter(-1, 1, 5));  // off-grid
}

TEST_CASE("Occupancy: release only clears the matching owner; clear is unconditional") {
    Occupancy occ(4, 3);
    occ.set(2, 2, 8);
    occ.release(2, 2, 9);            // wrong owner -> no-op
    CHECK(occ.at(2, 2) == 8);
    occ.release(2, 2, 8);            // right owner -> cleared
    CHECK(occ.at(2, 2) == Occupancy::kEmpty);
    occ.set(0, 0, 3);
    occ.clear(0, 0);                // unconditional
    CHECK(occ.at(0, 0) == Occupancy::kEmpty);
    // Off-grid set/clear/release are silent no-ops (no crash).
    occ.set(99, 99, 1);
    occ.clear(99, 99);
    occ.release(99, 99, 1);
}

TEST_CASE("Occupancy: release_all_of clears every cell of a unit except the kept one") {
    Occupancy occ(4, 3);
    occ.set(1, 1, 5);   // current cell
    occ.set(2, 1, 5);   // stale next-cell reservation
    occ.set(3, 2, 6);   // another unit
    occ.release_all_of(5, 1, 1);
    CHECK(occ.at(1, 1) == 5);                  // kept
    CHECK(occ.at(2, 1) == Occupancy::kEmpty);  // stale reservation cleared
    CHECK(occ.at(3, 2) == 6);                  // other units untouched
}
```

- [ ] **Step 2: Add build wiring, then run to verify it fails to compile/link.**

In `CMakeLists.txt`, after the `test_input.cpp` line (currently line 78), add:

```cmake
# occupancy.cpp is a pure C++17 cell-ownership grid (no SDL) used by the
# multi-unit movement/collision logic; compiled into both this test binary and
# ra2engine from one source of truth (same pattern as pathfind.cpp/world.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_occupancy.cpp engine/occupancy.cpp)
```

In `cmake/ra2_sources.cmake`, add `${RA2_ROOT}/engine/occupancy.cpp` to the `RA2_ENGINE_SOURCES` list (after `engine/input.cpp`).

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests`
Expected: FAIL — `occupancy.hpp` / `engine/occupancy.cpp` do not exist yet.

- [ ] **Step 3: Create the header** `engine/occupancy.hpp`:

```cpp
#pragma once
#include <vector>

namespace ra2engine {

// A width*height grid mapping each cell to the index of the unit that owns it
// (its current cell) or is reserving it (the next cell it is stepping into),
// or kEmpty. Pure: no SDL/OS deps, so it is unit-testable and compiled into
// both ra2assets_tests and ra2engine (see CMakeLists.txt). Off-grid
// coordinates read as kEmpty and are never enterable; all mutators no-op
// off-grid.
class Occupancy {
public:
    static constexpr int kEmpty = -1;

    Occupancy() = default;
    Occupancy(int width, int height)
        : w_(width < 0 ? 0 : width), h_(height < 0 ? 0 : height),
          owner_(static_cast<std::size_t>(w_) * h_, kEmpty) {}

    int width() const { return w_; }
    int height() const { return h_; }

    int at(int x, int y) const {
        if (!in(x, y)) return kEmpty;
        return owner_[idx(x, y)];
    }
    bool is_free(int x, int y) const { return in(x, y) && owner_[idx(x, y)] == kEmpty; }

    // unit may enter (x,y) iff it is in-grid AND (free OR already owned by unit).
    bool can_enter(int x, int y, int unit) const {
        if (!in(x, y)) return false;
        const int o = owner_[idx(x, y)];
        return o == kEmpty || o == unit;
    }

    void set(int x, int y, int unit) {
        if (in(x, y)) owner_[idx(x, y)] = unit;
    }
    void clear(int x, int y) {
        if (in(x, y)) owner_[idx(x, y)] = kEmpty;
    }
    // Clears (x,y) only if it is currently owned by `unit` (so a unit releasing
    // its old cell cannot stomp a claim another unit already made there).
    void release(int x, int y, int unit) {
        if (in(x, y) && owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
    }
    // Clear EVERY cell owned by `unit` except (keep_x, keep_y). Used when a
    // unit is re-ordered mid-walk: it may hold a reservation on a next cell it
    // will now never enter, which would otherwise stay owned forever and block
    // other units. O(w*h) scan -- called only on user orders, not per frame.
    void release_all_of(int unit, int keep_x, int keep_y) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x)
                if (owner_[idx(x, y)] == unit && !(x == keep_x && y == keep_y))
                    owner_[idx(x, y)] = kEmpty;
    }

private:
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * w_ + x;
    }
    int w_ = 0, h_ = 0;
    std::vector<int> owner_;
};

}  // namespace ra2engine
```

- [ ] **Step 4: Create the (empty) translation unit** `engine/occupancy.cpp` (the class is header-only; a TU keeps the CMake source entry valid and gives a place for future non-inline members):

```cpp
#include "occupancy.hpp"
// Occupancy is currently header-only (all members are trivial inline
// accessors). This translation unit exists so the class has a compiled home in
// both the test binary and ra2engine (see CMakeLists.txt / ra2_sources.cmake)
// and to hold any future out-of-line members.
namespace ra2engine {}  // namespace ra2engine
```

- [ ] **Step 5: Run the tests to verify they pass.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests` then `& D:\ra2-android\build\ra2assets_tests.exe -tc="Occupancy*"`
Expected: PASS — `test cases: 4 | 4 passed`.

- [ ] **Step 6: Commit.**

```bash
git -C D:\ra2-android add engine/occupancy.hpp engine/occupancy.cpp tests/test_occupancy.cpp CMakeLists.txt cmake/ra2_sources.cmake
git -C D:\ra2-android commit -m "engine: add pure Occupancy cell-ownership grid (Phase A)"
```

---

### Task 2: `group_move` formation assignment (pure module)

**Files:**
- Create: `engine/group_move.hpp`
- Create: `engine/group_move.cpp`
- Create: `tests/test_group_move.cpp`
- Modify: `CMakeLists.txt`, `cmake/ra2_sources.cmake`

**Interfaces:**
- Consumes: `ra2engine::Occupancy` (Task 1), `ra2engine::Cellxy` (from `engine/pathfind.hpp`).
- Produces:
  - `std::vector<Cellxy> nearest_free_cells(Cellxy center, int count, const std::vector<uint8_t>& passable, int w, int h, const Occupancy& occ)` — up to `count` distinct cells nearest-first (expanding square rings around `center`), each **walkable** (`passable[y*w+x]!=0`) and **free** (`occ.is_free`). Deterministic scan order.
  - `std::vector<Cellxy> assign_formation(Cellxy goal, const std::vector<Cellxy>& mover_cells, const std::vector<uint8_t>& passable, int w, int h, const Occupancy& occ)` — returns exactly `mover_cells.size()` target cells: collects free walkable cells spiralling from `goal` (treating the movers' own current cells as free), greedily assigns each collected cell to the nearest unassigned mover; any mover left without a collected cell keeps its own current cell.

- [ ] **Step 1: Write the failing test** — create `tests/test_group_move.cpp`:

```cpp
#include "doctest.h"

#include <algorithm>
#include <vector>

#include "group_move.hpp"
#include "occupancy.hpp"
#include "pathfind.hpp"  // Cellxy

using ra2engine::assign_formation;
using ra2engine::Cellxy;
using ra2engine::nearest_free_cells;
using ra2engine::Occupancy;

namespace {
// Fully-passable w*h grid.
std::vector<uint8_t> open_grid(int w, int h) { return std::vector<uint8_t>((size_t)w * h, 1); }
bool has_cell(const std::vector<Cellxy>& v, int x, int y) {
    return std::any_of(v.begin(), v.end(), [&](Cellxy c) { return c.x == x && c.y == y; });
}
}  // namespace

TEST_CASE("nearest_free_cells: returns the goal first, then nearest free walkable cells") {
    auto p = open_grid(10, 10);
    Occupancy occ(10, 10);
    auto cells = nearest_free_cells({5, 5}, 5, p, 10, 10, occ);
    REQUIRE(cells.size() == 5);
    CHECK(cells[0].x == 5);
    CHECK(cells[0].y == 5);                 // the center itself is nearest
    // All distinct.
    for (size_t i = 0; i < cells.size(); ++i)
        for (size_t j = i + 1; j < cells.size(); ++j)
            CHECK_FALSE(cells[i].x == cells[j].x && cells[i].y == cells[j].y);
    // All within one ring of the center (Chebyshev distance <= 1 for 5 cells).
    for (auto c : cells) {
        CHECK(std::abs(c.x - 5) <= 1);
        CHECK(std::abs(c.y - 5) <= 1);
    }
}

TEST_CASE("nearest_free_cells: skips impassable and occupied cells") {
    auto p = open_grid(5, 5);
    p[2 * 5 + 2] = 0;              // block the center (2,2)
    Occupancy occ(5, 5);
    occ.set(3, 2, 0);             // occupy (3,2)
    auto cells = nearest_free_cells({2, 2}, 3, p, 5, 5, occ);
    REQUIRE(cells.size() == 3);
    CHECK_FALSE(has_cell(cells, 2, 2));   // impassable center excluded
    CHECK_FALSE(has_cell(cells, 3, 2));   // occupied cell excluded
}

TEST_CASE("nearest_free_cells: returns fewer than requested when the area is crowded") {
    // 3x1 strip, two of three cells occupied -> only 1 free walkable cell.
    auto p = open_grid(3, 1);
    Occupancy occ(3, 1);
    occ.set(0, 0, 0);
    occ.set(2, 0, 1);
    auto cells = nearest_free_cells({1, 0}, 5, p, 3, 1, occ);
    REQUIRE(cells.size() == 1);
    CHECK(cells[0].x == 1);
}

TEST_CASE("assign_formation: one target per mover, all distinct, mover cells count as free") {
    auto p = open_grid(10, 10);
    Occupancy occ(10, 10);
    // Three movers standing near the goal; their own cells are occupied by
    // themselves but must be treated as free candidate targets.
    std::vector<Cellxy> movers = {{5, 5}, {6, 5}, {5, 6}};
    occ.set(5, 5, 0);
    occ.set(6, 5, 1);
    occ.set(5, 6, 2);
    auto targets = assign_formation({5, 5}, movers, p, 10, 10, occ);
    REQUIRE(targets.size() == 3);
    // Distinct targets.
    for (size_t i = 0; i < targets.size(); ++i)
        for (size_t j = i + 1; j < targets.size(); ++j)
            CHECK_FALSE(targets[i].x == targets[j].x && targets[i].y == targets[j].y);
    // The goal cell (currently owned by mover 0) is a valid target.
    CHECK(has_cell(targets, 5, 5));
}

TEST_CASE("assign_formation: crowded area leaves overflow movers on their own cell") {
    // 3x1 strip: goal region has room for only the 3 strip cells, but 3 movers
    // already sit on all 3. A 4th mover far away keeps its own cell.
    auto p = open_grid(3, 1);
    Occupancy occ(3, 1);
    std::vector<Cellxy> movers = {{0, 0}, {1, 0}, {2, 0}};
    occ.set(0, 0, 0);
    occ.set(1, 0, 1);
    occ.set(2, 0, 2);
    auto targets = assign_formation({1, 0}, movers, p, 3, 1, occ);
    REQUIRE(targets.size() == 3);
    // Every mover cell is free (they are the movers), so all 3 get assigned
    // distinct cells within the strip.
    for (size_t i = 0; i < targets.size(); ++i)
        for (size_t j = i + 1; j < targets.size(); ++j)
            CHECK_FALSE(targets[i].x == targets[j].x && targets[i].y == targets[j].y);
}
```

- [ ] **Step 2: Add build wiring and run to verify it fails.**

In `CMakeLists.txt`, after the Task 1 line, add:

```cmake
# group_move.cpp is pure C++17 (no SDL): formation target assignment for a
# group move order. Compiled into both binaries from one source of truth.
target_sources(ra2assets_tests PRIVATE tests/test_group_move.cpp engine/group_move.cpp)
```

In `cmake/ra2_sources.cmake`, add `${RA2_ROOT}/engine/group_move.cpp` to `RA2_ENGINE_SOURCES`.

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests`
Expected: FAIL — `group_move.hpp` not found.

- [ ] **Step 3: Create the header** `engine/group_move.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "occupancy.hpp"
#include "pathfind.hpp"  // Cellxy

namespace ra2engine {

// Up to `count` distinct cells nearest-first (by expanding square rings) around
// `center`, each walkable (passable[y*w+x] != 0) and free (occ.is_free). The
// center itself is the first candidate. Fewer than `count` are returned if the
// nearby area is too crowded/blocked (bounded ring search). Pure.
std::vector<Cellxy> nearest_free_cells(Cellxy center, int count,
                                       const std::vector<uint8_t>& passable, int w, int h,
                                       const Occupancy& occ);

// Assign each of `mover_cells` a distinct destination cell for a group move to
// `goal`. Collects free walkable cells spiralling out from `goal` -- treating
// each mover's OWN current cell as free (they are about to vacate) -- then
// greedily gives each collected cell (nearest-to-goal first) to the nearest
// not-yet-assigned mover. Any mover left unassigned (crowded) keeps its own
// current cell. Returns exactly mover_cells.size() targets. Pure.
std::vector<Cellxy> assign_formation(Cellxy goal, const std::vector<Cellxy>& mover_cells,
                                     const std::vector<uint8_t>& passable, int w, int h,
                                     const Occupancy& occ);

}  // namespace ra2engine
```

- [ ] **Step 4: Create the implementation** `engine/group_move.cpp`:

```cpp
#include "group_move.hpp"

#include <cmath>

namespace ra2engine {

std::vector<Cellxy> nearest_free_cells(Cellxy center, int count,
                                       const std::vector<uint8_t>& passable, int w, int h,
                                       const Occupancy& occ) {
    std::vector<Cellxy> out;
    if (count <= 0 || w <= 0 || h <= 0) return out;

    auto walkable = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h &&
               passable[static_cast<std::size_t>(y) * w + x] != 0;
    };
    auto consider = [&](int x, int y) {
        if ((int)out.size() >= count) return;
        if (!walkable(x, y) || !occ.is_free(x, y)) return;
        out.push_back({x, y});
    };

    // Ring 0 is the center; ring r is the square border at Chebyshev distance r.
    // The map's iso extent is ~213 cells, so this is a hard upper bound on r.
    const int kMaxRing = w + h;
    consider(center.x, center.y);
    for (int r = 1; r <= kMaxRing && (int)out.size() < count; ++r) {
        // Top and bottom edges of the ring.
        for (int x = center.x - r; x <= center.x + r; ++x) {
            consider(x, center.y - r);
            consider(x, center.y + r);
        }
        // Left and right edges (excluding the corners already done above).
        for (int y = center.y - r + 1; y <= center.y + r - 1; ++y) {
            consider(center.x - r, y);
            consider(center.x + r, y);
        }
    }
    return out;
}

std::vector<Cellxy> assign_formation(Cellxy goal, const std::vector<Cellxy>& mover_cells,
                                     const std::vector<uint8_t>& passable, int w, int h,
                                     const Occupancy& occ) {
    const std::size_t n = mover_cells.size();
    std::vector<Cellxy> targets(mover_cells);  // default: each mover keeps its own cell

    if (n == 0) return targets;

    // Treat the movers' own cells as free (they are about to leave), so a tight
    // group can fill the goal region even though they currently occupy it.
    Occupancy tmp = occ;
    for (const Cellxy& c : mover_cells) tmp.clear(c.x, c.y);

    std::vector<Cellxy> slots = nearest_free_cells(goal, static_cast<int>(n), passable, w, h, tmp);

    // Greedy: for each slot (nearest-to-goal first) pick the nearest unassigned
    // mover. Deterministic tie-break by mover index (first wins).
    std::vector<bool> taken(n, false);
    for (const Cellxy& slot : slots) {
        int best = -1;
        double best_d2 = 0.0;
        for (std::size_t m = 0; m < n; ++m) {
            if (taken[m]) continue;
            const double dx = mover_cells[m].x - slot.x;
            const double dy = mover_cells[m].y - slot.y;
            const double d2 = dx * dx + dy * dy;
            if (best < 0 || d2 < best_d2) {
                best = static_cast<int>(m);
                best_d2 = d2;
            }
        }
        if (best < 0) break;
        targets[best] = slot;
        taken[best] = true;
    }
    return targets;
}

}  // namespace ra2engine
```

- [ ] **Step 5: Run the tests to verify they pass.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests` then `& D:\ra2-android\build\ra2assets_tests.exe -tc="nearest_free_cells*,assign_formation*"`
Expected: PASS — 5 cases pass.

- [ ] **Step 6: Commit.**

```bash
git -C D:\ra2-android add engine/group_move.hpp engine/group_move.cpp tests/test_group_move.cpp CMakeLists.txt cmake/ra2_sources.cmake
git -C D:\ra2-android commit -m "engine: add pure formation assignment (nearest_free_cells/assign_formation)"
```

---

### Task 3: `Unit` movement fields + `step_units` collision stepper (pure)

**Files:**
- Modify: `engine/world.hpp` (add `Unit::wait_time`; declare `step_units`; include `occupancy.hpp`)
- Modify: `engine/world.cpp` (define `step_units` + its `passable_minus_units` helper)
- Create: `tests/test_movement.cpp`
- Modify: `CMakeLists.txt` (add `tests/test_movement.cpp`)

**Interfaces:**
- Consumes: `Occupancy` (Task 1), `find_path` (`engine/pathfind.hpp`), `Unit`/`Cellxy` (`engine/world.hpp`).
- Produces: field `double Unit::wait_time = 0.0;` and free function
  `void step_units(std::vector<Unit>& units, const std::vector<uint8_t>& passable, int w, int h, Occupancy& occ, double dt, double move_cps)` — advances every moving unit toward its `path` at `move_cps` cells/sec, reserving the next cell before entering it. A blocked unit waits (resuming the instant the cell frees), attempts a re-path around occupied cells on a seniority-staggered cadence (`kBlockedWait` = 0.30 s if blocked by a lower-index unit, one interval later if blocked by a higher-index one — this deterministic index-order asymmetry is what prevents two facing units from livelocking on mirror-image detours), and abandons its order only after `kGiveUpAfter` (2.0 s) of continuous block. Idle units re-assert ownership of their current cell and stand. Does **not** touch `Unit::level` (rendering height is patched by `World::step` from the map). Stepping order is ascending index (deterministic).

- [ ] **Step 1: Write the failing test** — create `tests/test_movement.cpp`:

```cpp
#include "doctest.h"

#include <cmath>
#include <vector>

#include "occupancy.hpp"
#include "pathfind.hpp"
#include "world.hpp"

using ra2engine::Cellxy;
using ra2engine::Occupancy;
using ra2engine::step_units;
using ra2engine::Unit;

namespace {

std::vector<uint8_t> open_grid(int w, int h) { return std::vector<uint8_t>((size_t)w * h, 1); }

// Build a unit at (x,y) with a straight-line intent toward `goal` computed by
// find_path over `passable`; seeds occupancy ownership of its start cell.
Unit make_mover(int x, int y, Cellxy goal, const std::vector<uint8_t>& p, int w, int h,
                Occupancy& occ, int idx) {
    Unit u;
    u.cell_x = x;
    u.cell_y = y;
    u.fx = x;
    u.fy = y;
    u.path = ra2engine::find_path(p, w, h, {x, y}, goal);
    u.path_idx = u.path.size() > 1 ? 1 : 0;
    occ.set(x, y, idx);
    return u;
}

// Step the whole set for `seconds` at 60 Hz.
void run(std::vector<Unit>& units, const std::vector<uint8_t>& p, int w, int h, Occupancy& occ,
         double seconds, double cps = 4.0) {
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < seconds; t += dt) step_units(units, p, w, h, occ, dt, cps);
}

// Assert no two units share an integer cell.
void check_no_overlap(const std::vector<Unit>& units) {
    for (size_t i = 0; i < units.size(); ++i)
        for (size_t j = i + 1; j < units.size(); ++j)
            CHECK_FALSE(units[i].cell_x == units[j].cell_x &&
                        units[i].cell_y == units[j].cell_y);
}

}  // namespace

TEST_CASE("step_units: a single unit walks its path to the goal") {
    auto p = open_grid(10, 1);
    Occupancy occ(10, 1);
    std::vector<Unit> units;
    units.push_back(make_mover(0, 0, {9, 0}, p, 10, 1, occ, 0));
    REQUIRE(units[0].moving());
    run(units, p, 10, 1, occ, 10.0);
    CHECK_FALSE(units[0].moving());
    CHECK(units[0].cell_x == 9);
    CHECK(units[0].cell_y == 0);
    CHECK(units[0].fx == 9.0);
}

TEST_CASE("step_units: reservation prevents two units ever sharing a cell (funnel)") {
    // Two rooms (x<=2 and x>=4 on y=0..2) joined only by the single cell (3,1).
    const int w = 7, h = 3;
    std::vector<uint8_t> p((size_t)w * h, 0);
    auto open = [&](int x, int y) { p[(size_t)y * w + x] = 1; };
    for (int y = 0; y < 3; ++y) { open(0, y); open(1, y); open(2, y); }  // left room
    open(3, 1);                                                          // the gap
    for (int y = 0; y < 3; ++y) { open(4, y); open(5, y); open(6, y); }  // right room
    Occupancy occ(w, h);
    std::vector<Unit> units;
    units.push_back(make_mover(0, 0, {6, 0}, p, w, h, occ, 0));
    units.push_back(make_mover(0, 1, {6, 1}, p, w, h, occ, 1));
    units.push_back(make_mover(0, 2, {6, 2}, p, w, h, occ, 2));
    REQUIRE(units[0].moving());
    // Step and assert the no-overlap invariant on EVERY frame.
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 20.0; t += dt) {
        step_units(units, p, w, h, occ, dt, 4.0);
        check_no_overlap(units);
    }
    // All three funnel through and reach the right room at distinct cells.
    for (const auto& u : units) CHECK_FALSE(u.moving());
    check_no_overlap(units);
    for (const auto& u : units) CHECK(u.cell_x == 6);
}

TEST_CASE("step_units: boxed-in unit cannot move and stops cleanly (no spin)") {
    auto p = open_grid(3, 3);
    Occupancy occ(3, 3);
    std::vector<Unit> units;
    // Center unit surrounded on all 8 neighbours by parked units.
    int idx = 0;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            if (!(x == 1 && y == 1)) {
                Unit u;
                u.cell_x = x; u.cell_y = y; u.fx = x; u.fy = y;
                occ.set(x, y, idx);
                units.push_back(u);
                ++idx;
            }
    // The center unit is index 8; order it toward a far cell.
    Unit c;
    c.cell_x = 1; c.cell_y = 1; c.fx = 1; c.fy = 1;
    c.path = ra2engine::find_path(p, 3, 3, {1, 1}, {2, 2});
    c.path_idx = c.path.size() > 1 ? 1 : 0;
    occ.set(1, 1, idx);
    units.push_back(c);

    run(units, p, 3, 3, occ, 5.0);
    // It never left its cell and it is no longer "moving" (gave up cleanly).
    CHECK(units.back().cell_x == 1);
    CHECK(units.back().cell_y == 1);
    CHECK_FALSE(units.back().moving());
    check_no_overlap(units);
}

TEST_CASE("step_units: head-on in a 1-wide corridor -> no overlap, both terminate") {
    auto p = open_grid(4, 1);  // cells (0..3, 0)
    Occupancy occ(4, 1);
    std::vector<Unit> units;
    units.push_back(make_mover(0, 0, {3, 0}, p, 4, 1, occ, 0));  // wants to go right
    units.push_back(make_mover(3, 0, {0, 0}, p, 4, 1, occ, 1));  // wants to go left
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 10.0; t += dt) {
        step_units(units, p, 4, 1, occ, dt, 4.0);
        check_no_overlap(units);
    }
    // Deadlock resolves by both stopping; neither overlaps.
    CHECK_FALSE(units[0].moving());
    CHECK_FALSE(units[1].moving());
    check_no_overlap(units);
}

TEST_CASE("step_units: head-on with room routes both around to their goals") {
    auto p = open_grid(4, 2);  // 2 rows give a detour lane
    Occupancy occ(4, 2);
    std::vector<Unit> units;
    units.push_back(make_mover(0, 0, {3, 0}, p, 4, 2, occ, 0));
    units.push_back(make_mover(3, 0, {0, 0}, p, 4, 2, occ, 1));
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 20.0; t += dt) {
        step_units(units, p, 4, 2, occ, dt, 4.0);
        check_no_overlap(units);
    }
    CHECK(units[0].cell_x == 3);
    CHECK(units[0].cell_y == 0);
    CHECK(units[1].cell_x == 0);
    CHECK(units[1].cell_y == 0);
}
```

- [ ] **Step 2: Add build wiring and run to verify it fails.**

In `CMakeLists.txt`, after the Task 2 line, add:

```cmake
# test_movement.cpp exercises the pure step_units collision stepper defined in
# engine/world.cpp (already a source above) against hand-built grids/units --
# no MapFile needed, so the head-on/boxed-in/funnel cases are deterministic.
target_sources(ra2assets_tests PRIVATE tests/test_movement.cpp)
```

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests`
Expected: FAIL — `step_units` undeclared and `Unit::wait_time` missing.

- [ ] **Step 3: Add the `Unit` field and the declaration** in `engine/world.hpp`.

Add `#include "occupancy.hpp"` next to the existing includes (after `#include "ra2assets/map.hpp"`).

In `struct Unit`, after the `double anim_time = 0.0;` line, add:

```cpp
    double wait_time = 0.0;     // seconds blocked at current cell (collision re-path timer)
```

After the `struct Unit { ... };` block (before `pick_start_cell`), add the declaration:

```cpp
// Advance every moving unit toward its `path` by `dt` seconds at `move_cps`
// cells/sec, using `occ` to enforce one-unit-per-cell: a unit reserves the next
// path cell before entering it. While that cell is held by another unit the
// unit waits (resuming the instant the cell frees) and attempts a re-path
// around occupied cells on a seniority-staggered cadence -- a unit blocked by
// a LOWER-index unit tries at kBlockedWait, one blocked by a HIGHER-index unit
// one interval later (breaks the mirror-symmetric re-path livelock); it
// abandons the order only after kGiveUpAfter seconds of continuous block
// (boxed-in / sealed corridor).
// Idle units re-assert ownership of their current cell and stand. Units are
// processed in ascending index order (deterministic). Does NOT modify
// Unit::level -- rendering height is patched separately from the map by
// World::step. Pure: takes a passability grid + occupancy, no MapFile/SDL.
void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
                Occupancy& occ, double dt, double move_cps);
```

- [ ] **Step 4: Implement `step_units`** in `engine/world.cpp`.

Add `#include "pathfind.hpp"` if not already present (it is, transitively via world.hpp, but include explicitly). At the top of the `ra2engine` namespace, near the existing `kStandFrame`/`kWalkFirst` constants, add:

```cpp
// Seconds between re-path attempts while blocked (next cell held by another
// unit). Short enough to feel responsive, long enough that a unit briefly
// waiting for one ahead to step off doesn't thrash A* every frame.
static constexpr double kBlockedWait = 0.30;
// Seconds of CONTINUOUS block after which a unit abandons its order (boxed-in
// or sealed corridor). Transient jams (a funnel cell mid-crossing) clear well
// inside this: the per-frame can_enter check resumes the walk the instant the
// blocker steps off, and failed re-paths retry every kBlockedWait until then.
static constexpr double kGiveUpAfter = 2.0;
```

Then add the helper and the function (place after `World::step`'s current definition, or anywhere in the namespace):

```cpp
// Copy `passable` but mark every cell OWNED/RESERVED by a unit other than
// `self` impassable, so a re-path routes around the current traffic. The
// unit's own start cell is forced walkable (find_path needs a passable start).
static std::vector<std::uint8_t> passable_minus_units(const std::vector<std::uint8_t>& passable,
                                                      int w, int h, const Occupancy& occ, int self,
                                                      int start_x, int start_y) {
    std::vector<std::uint8_t> out = passable;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int o = occ.at(x, y);
            if (o != Occupancy::kEmpty && o != self)
                out[static_cast<std::size_t>(y) * w + x] = 0;
        }
    if (start_x >= 0 && start_y >= 0 && start_x < w && start_y < h)
        out[static_cast<std::size_t>(start_y) * w + start_x] = 1;
    return out;
}

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
                Occupancy& occ, double dt, double move_cps) {
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& u = units[i];
        const int self = static_cast<int>(i);

        if (!u.moving()) {
            occ.set(u.cell_x, u.cell_y, self);  // idle units own their cell
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.frame = kStandFrame;
            u.wait_time = 0.0;
            continue;
        }

        double budget = move_cps * dt;
        bool blocked = false;
        while (budget > 0.0 && u.path_idx < u.path.size()) {
            const Cellxy tgt = u.path[u.path_idx];
            if (tgt.x == u.cell_x && tgt.y == u.cell_y) {  // already here (e.g. path[0])
                ++u.path_idx;
                continue;
            }
            // Reserve the next cell before stepping into it.
            if (occ.at(tgt.x, tgt.y) != self) {
                if (occ.can_enter(tgt.x, tgt.y, self)) {
                    occ.set(tgt.x, tgt.y, self);  // reserve (now owns current + next)
                } else {
                    blocked = true;
                    break;
                }
            }
            const double dx = tgt.x - u.fx;
            const double dy = tgt.y - u.fy;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 1e-9 || budget >= dist) {
                occ.release(u.cell_x, u.cell_y, self);  // leave the old cell
                u.fx = tgt.x;
                u.fy = tgt.y;
                u.cell_x = tgt.x;
                u.cell_y = tgt.y;  // occ already owns this cell (reserved above)
                budget -= (dist <= 1e-9 ? 0.0 : dist);
                ++u.path_idx;
                u.wait_time = 0.0;
            } else {
                const double t = budget / dist;
                u.fx += dx * t;
                u.fy += dy * t;
                budget = 0.0;
            }
        }

        if (u.path_idx >= u.path.size()) {  // arrived
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.path.clear();
            u.path_idx = 0;
            u.anim_time = 0.0;
            u.frame = kStandFrame;
            u.wait_time = 0.0;
            occ.set(u.cell_x, u.cell_y, self);
            continue;
        }

        if (blocked) {
            const double prev_wait = u.wait_time;
            u.wait_time += dt;
            u.frame = kStandFrame;  // stand while waiting (reads as "holding")
            // Seniority stagger (breaks the mirror-symmetric re-path
            // livelock): if the unit blocking our next cell has a LOWER index,
            // it holds course and WE detour early (first attempt at
            // kBlockedWait); if it has a HIGHER index, it is expected to
            // detour first, so our first attempt is delayed one extra
            // interval. Without this, two facing units cross the re-path
            // threshold on the same frame every cycle and -- with symmetric
            // occupancy and deterministic A* tie-breaks -- each computes the
            // mirror image of the other's detour, oscillating between rows
            // forever (a stable 2-cycle: observed in a 200s trace, not
            // hypothetical).
            const Cellxy blk = u.path[u.path_idx];
            const int blocker = occ.at(blk.x, blk.y);
            const double first_try =
                (blocker != Occupancy::kEmpty && blocker < self)
                    ? kBlockedWait
                    : 2.0 * kBlockedWait;
            if (u.wait_time >= kGiveUpAfter) {
                // Continuously blocked with no route found for the whole
                // timeout (boxed-in / sealed corridor) -> give up cleanly.
                u.path.clear();
                u.path_idx = 0;
                u.fx = u.cell_x;
                u.fy = u.cell_y;
                u.wait_time = 0.0;
            } else if (u.wait_time >= first_try &&
                       (prev_wait < first_try ||
                        static_cast<int>((prev_wait - first_try) / kBlockedWait) !=
                            static_cast<int>((u.wait_time - first_try) / kBlockedWait))) {
                // On a re-path attempt boundary (first_try, then every
                // kBlockedWait after) -> attempt a re-path around the current
                // traffic. On FAILURE keep the original path and keep waiting:
                // transient jams (another unit mid-crossing of a funnel cell)
                // clear on their own, and the per-frame can_enter check above
                // resumes the walk the moment the blocker steps off. Only the
                // kGiveUpAfter timeout abandons the order.
                const Cellxy goal = u.path.back();
                std::vector<std::uint8_t> p2 =
                    passable_minus_units(passable, w, h, occ, self, u.cell_x, u.cell_y);
                std::vector<Cellxy> np = find_path(p2, w, h, {u.cell_x, u.cell_y}, goal);
                if (np.size() > 1) {
                    u.path = std::move(np);
                    u.path_idx = 1;
                    u.wait_time = 0.0;
                }
            }
            continue;
        }

        // Still walking: cycle the walk frames.
        u.anim_time += dt;
        const int phase = static_cast<int>(u.anim_time * kWalkFps) % kWalkCount;
        u.frame = kWalkFirst + phase;
    }
}
```

- [ ] **Step 5: Run the tests to verify they pass.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests` then `& D:\ra2-android\build\ra2assets_tests.exe -tc="step_units*"`
Expected: PASS — 6 cases pass. (If the funnel test hangs conceptually, note the loop is time-bounded to 20 s of sim.)

- [ ] **Step 6: Commit.**

```bash
git -C D:\ra2-android add engine/world.hpp engine/world.cpp tests/test_movement.cpp CMakeLists.txt
git -C D:\ra2-android commit -m "engine: add step_units collision stepper (reserve/wait/re-path) + Unit.wait_time"
```

---

> **EXECUTION NOTE (controller):** Tasks 4 and 5 are dispatched to a single
> implementer and reviewed as one unit. Task 4 removes `World`'s single-unit API
> (`unit()`, `set_unit_image`, `order_move`), which the OLD `engine/input.cpp`
> and `tests/test_input.cpp` (both rewritten in Task 5) still call —
> `ra2assets_tests` does not compile between the two tasks, so they must land
> together. Run Task 4's test step after Task 5's changes are in; commit once.

### Task 4: `World` multi-unit API (units vector, selection, group order, step)

**Files:**
- Modify: `engine/world.hpp` (replace single-unit API with multi-unit API)
- Modify: `engine/world.cpp` (constructor, add_unit, selection helpers, order_move_selected, step, unit_draw_order; keep pick_start_cell/build_passability/terrain_screen_bounds)
- Modify: `tests/test_world.cpp` (add guarded real-map integration tests; existing free-function tests are unchanged)

**Interfaces:**
- Consumes: `assign_formation` (Task 2), `step_units` (Task 3), `Occupancy` (Task 1), `find_path`.
- Produces on `class World`:
  - `std::vector<Unit>& units()` / `const std::vector<Unit>& units() const`
  - `Unit& add_unit(int cell_x, int cell_y, const std::string& image, int frame = 0)` — appends a unit, seeds occupancy + level from the map, returns it.
  - `void clear_selection()`; `int unit_at_cell(int x, int y) const` (index whose current cell == (x,y), else -1).
  - `void order_move_selected(int goal_x, int goal_y)` — formation-assign + path every selected unit.
  - `void step(double dt)` — `step_units(...)` then patch each unit's `level` from the map.
  - `const Occupancy& occupancy() const`; `int grid_w()/grid_h()`, `bool walkable(int,int)`, `set_move_speed`/`move_speed` (unchanged).
  - Free: `std::vector<int> unit_draw_order(const std::vector<Unit>& units)` — indices sorted back-to-front by screen depth (`fx+fy` asc, tie-break `fx` then index).
- Removed: `Unit& unit()`, `const Unit& unit() const`, `set_unit_image`, `order_move(int,int)` (callers migrate to the new API in Tasks 5–6).

- [ ] **Step 1: Write the failing test** — append to `tests/test_world.cpp` (add `#include "occupancy.hpp"` and `#include "pathfind.hpp"` and `#include "ra2assets/mix.hpp"`, `#include <cstdlib>`, `#include <fstream>` at the top):

```cpp
// ---- Multi-unit World integration (guarded real map) ----
namespace {
std::vector<uint8_t> slurp_ra2(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
bool load_real_map_w(ra2assets::MapFile& out) {
    const char* d = std::getenv("RA2_INSTALL_DIR");
    std::string dir = d ? d : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    auto b = slurp_ra2(dir + "mapsmd03.mix");
    if (b.empty()) return false;
    ra2assets::MixFile mix = ra2assets::MixFile::open(std::move(b));
    const ra2assets::MixEntry* e = mix.find(0x133a0cb2u);
    if (!e) return false;
    auto raw = mix.read(*e);
    out = ra2assets::MapFile::parse(std::string(raw.begin(), raw.end()));
    return true;
}
}  // namespace

TEST_CASE("World: add_unit places units, occupancy and unit_at_cell agree") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    ra2engine::Unit& u0 = world.add_unit(c->x, c->y, "GI", 0);
    CHECK(world.units().size() == 1);
    CHECK(world.unit_at_cell(c->x, c->y) == 0);
    CHECK(world.occupancy().at(c->x, c->y) == 0);
    (void)u0;
}

TEST_CASE("World: group move sends selected units to distinct cells, never overlapping") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // Spawn a small cluster around the start cell (skip any non-walkable slot).
    int placed = 0;
    for (int dy = 0; dy < 3 && placed < 4; ++dy)
        for (int dx = 0; dx < 3 && placed < 4; ++dx)
            if (world.walkable(c->x + dx, c->y + dy) &&
                world.unit_at_cell(c->x + dx, c->y + dy) < 0) {
                world.add_unit(c->x + dx, c->y + dy, "GI", 0);
                ++placed;
            }
    REQUIRE(placed >= 2);
    for (auto& u : world.units()) u.selected = true;

    // Order them to a reachable cell ~12 south-east and run the sim.
    world.order_move_selected(c->x + 12, c->y + 12);
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 20.0; t += dt) {
        world.step(dt);
        // no-overlap invariant every frame
        const auto& us = world.units();
        for (size_t i = 0; i < us.size(); ++i)
            for (size_t j = i + 1; j < us.size(); ++j)
                CHECK_FALSE(us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y);
    }
    // Distinct final cells.
    const auto& us = world.units();
    for (size_t i = 0; i < us.size(); ++i)
        for (size_t j = i + 1; j < us.size(); ++j)
            CHECK_FALSE(us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y);
}

TEST_CASE("unit_draw_order: sorts back-to-front by fx+fy") {
    std::vector<ra2engine::Unit> us(3);
    us[0].fx = 5; us[0].fy = 5;   // depth 10 (middle)
    us[1].fx = 1; us[1].fy = 1;   // depth 2  (farthest -> first)
    us[2].fx = 9; us[2].fy = 9;   // depth 18 (nearest -> last)
    auto order = ra2engine::unit_draw_order(us);
    REQUIRE(order.size() == 3);
    CHECK(order[0] == 1);
    CHECK(order[1] == 0);
    CHECK(order[2] == 2);
}
```

- [ ] **Step 2: Run to verify it fails.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests`
Expected: FAIL — `add_unit`, `units()`, `unit_at_cell`, `order_move_selected`, `occupancy()`, `unit_draw_order` do not exist.

- [ ] **Step 3: Replace the `World` class declaration** in `engine/world.hpp`.

Replace the entire `class World { ... };` block with:

```cpp
// Holds a reference to the (caller-owned, long-lived) MapFile plus the placed
// Units, the passability grid, and a cell-occupancy grid derived from the
// map's cells. Does not copy the map's cell data -- the caller must keep the
// MapFile alive at least as long as the World. Pure engine-layer logic; no SDL.
class World {
public:
    explicit World(const ra2assets::MapFile& map);

    const ra2assets::MapFile& map() const { return *map_; }

    std::vector<Unit>& units() { return units_; }
    const std::vector<Unit>& units() const { return units_; }

    // Append a unit at iso cell (cell_x, cell_y) with SHP image name + frame;
    // seeds its level from the map and claims its cell in the occupancy grid.
    // Returns a reference to the new unit (valid until units() is reallocated).
    Unit& add_unit(int cell_x, int cell_y, const std::string& image, int frame = 0);

    // Deselect every unit.
    void clear_selection();
    // Index of the unit whose CURRENT cell is (x, y), or -1 if none.
    int unit_at_cell(int x, int y) const;

    // Passability grid accessors (iso space; see build_passability).
    int grid_w() const { return grid_w_; }
    int grid_h() const { return grid_h_; }
    const std::vector<std::uint8_t>& passable() const { return passable_; }
    bool walkable(int x, int y) const;
    const Occupancy& occupancy() const { return occ_; }

    // Movement speed in iso cells/sec (see main.cpp for the Rules-derived value).
    void set_move_speed(double cells_per_sec) { move_cps_ = cells_per_sec; }
    double move_speed() const { return move_cps_; }

    // Order every SELECTED unit to walk toward (goal_x, goal_y): assigns each a
    // distinct formation cell spiralling out from the goal, then paths it there.
    // Units retargeted mid-walk keep their interpolated fx/fy (no backward
    // snap). No-op if nothing is selected.
    void order_move_selected(int goal_x, int goal_y);

    // Advance all units by dt seconds (collision-aware, via step_units) and
    // patch each unit's terrain level from the map for rendering height.
    void step(double dt);

private:
    const ra2assets::MapFile* map_;
    std::vector<Unit> units_;
    std::vector<std::uint8_t> passable_;
    Occupancy occ_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;
};

// Indices of `units` sorted back-to-front for painter's-order drawing: by
// screen depth (fx + fy) ascending, tie-break fx ascending then index. Pure.
std::vector<int> unit_draw_order(const std::vector<Unit>& units);
```

- [ ] **Step 4: Rewrite the `World` members** in `engine/world.cpp`.

Replace the constructor and the removed single-unit methods (`World::World`, `World::set_unit_image`, `World::order_move`, `World::step`) with:

```cpp
World::World(const ra2assets::MapFile& map) : map_(&map) {
    passable_ = build_passability(map.cells(), grid_w_, grid_h_);
    occ_ = Occupancy(grid_w_, grid_h_);
}

bool World::walkable(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return passable_[static_cast<size_t>(y) * grid_w_ + x] != 0;
}

Unit& World::add_unit(int cell_x, int cell_y, const std::string& image, int frame) {
    Unit u;
    u.cell_x = cell_x;
    u.cell_y = cell_y;
    u.fx = cell_x;
    u.fy = cell_y;
    u.image = image;
    u.frame = frame;
    if (const ra2assets::MapCell* mc = map_->at(cell_x, cell_y)) u.level = mc->level;
    units_.push_back(std::move(u));
    const int idx = static_cast<int>(units_.size()) - 1;
    occ_.set(cell_x, cell_y, idx);
    return units_.back();
}

void World::clear_selection() {
    for (Unit& u : units_) u.selected = false;
}

int World::unit_at_cell(int x, int y) const {
    for (std::size_t i = 0; i < units_.size(); ++i)
        if (units_[i].cell_x == x && units_[i].cell_y == y) return static_cast<int>(i);
    return -1;
}

void World::order_move_selected(int goal_x, int goal_y) {
    std::vector<int> sel;
    std::vector<Cellxy> cur;
    for (std::size_t i = 0; i < units_.size(); ++i)
        if (units_[i].selected) {
            sel.push_back(static_cast<int>(i));
            cur.push_back({units_[i].cell_x, units_[i].cell_y});
        }
    if (sel.empty()) return;

    std::vector<Cellxy> targets =
        assign_formation({goal_x, goal_y}, cur, passable_, grid_w_, grid_h_, occ_);

    for (std::size_t k = 0; k < sel.size(); ++k) {
        Unit& u = units_[sel[k]];
        // Drop any stale next-cell reservation from an in-flight order (the
        // occupancy may still mark a cell this unit reserved but will now
        // never enter, which would block other units forever); re-assert only
        // the current cell.
        occ_.release_all_of(sel[k], u.cell_x, u.cell_y);
        occ_.set(u.cell_x, u.cell_y, sel[k]);
        const Cellxy tgt = targets[k];
        std::vector<Cellxy> path =
            find_path(passable_, grid_w_, grid_h_, {u.cell_x, u.cell_y}, tgt);
        if (path.size() <= 1) {
            // Assigned its own cell / unreachable: stay put, snap cleanly
            // (mirrors the single-unit Finding-1 cancel branch).
            u.path.clear();
            u.path_idx = 0;
            u.anim_time = 0.0;
            u.wait_time = 0.0;
            u.frame = kStandFrame;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
        } else {
            u.path = std::move(path);
            u.path_idx = 1;  // path[0] is the current cell
            u.anim_time = 0.0;
            u.wait_time = 0.0;
            // Do NOT reset fx/fy: preserves an in-flight walk across a reorder
            // (Task 12 Finding 1); step_units interpolates from any fx/fy.
        }
    }
}

void World::step(double dt) {
    step_units(units_, passable_, grid_w_, grid_h_, occ_, dt, move_cps_);
    // Patch terrain height for rendering (step_units is map-agnostic).
    for (Unit& u : units_)
        if (const ra2assets::MapCell* mc = map_->at(u.cell_x, u.cell_y)) u.level = mc->level;
}

std::vector<int> unit_draw_order(const std::vector<Unit>& units) {
    std::vector<int> order(units.size());
    for (int i = 0; i < static_cast<int>(units.size()); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const double da = units[a].fx + units[a].fy;
        const double db = units[b].fx + units[b].fy;
        if (da != db) return da < db;
        if (units[a].fx != units[b].fx) return units[a].fx < units[b].fx;
        return a < b;
    });
    return order;
}
```

Add `#include "group_move.hpp"` to `engine/world.cpp`'s includes (for `assign_formation`). `<algorithm>` is already included (for `std::sort`).

- [ ] **Step 5: Run the tests to verify they pass.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests` then (from PowerShell, with the install present) `$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & D:\ra2-android\build\ra2assets_tests.exe -tc="World*,unit_draw_order*"`
Expected: PASS — the `unit_draw_order` case always passes; the two `World:` cases pass (or skip cleanly with a MESSAGE if the install is absent).

- [ ] **Step 6: Commit.**

```bash
git -C D:\ra2-android add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C D:\ra2-android commit -m "engine: World holds a unit vector + selection/group-move/step API"
```

---

### Task 5: `InputEvent`/Platform extension + RA2-classic `Input` gesture machine

**Files:**
- Modify: `engine/platform.hpp` (extend `InputEvent`)
- Modify: `engine/platform.cpp` (`poll` fills button/shift/timestamp)
- Modify: `engine/input.hpp` (new gesture API + state)
- Modify: `engine/input.cpp` (rewrite `handle`; add `pan`, `selection_box`, gesture commits)
- Rewrite: `tests/test_input.cpp` (assert the new model from synthetic events)

**Interfaces:**
- Produces on `struct InputEvent`: `enum Button { Left, Right } button = Left;` `bool shift = false;` `uint32_t t_ms = 0;` (added after `x, y`; the existing `{Kind, x, y}` positional aggregate init still compiles).
- Produces on `class Input`: `void pan(int dx, int dy)` (camera pan + clamp — replaces drag-pan); `bool selection_box(int& x0, int& y0, int& x1, int& y1) const` (true + normalized rect while a left-drag box is active). `handle` now implements: left-click select, left-drag box, Shift+left-click toggle, Shift+left-drag add, double-click select-all-of-type, right-click group-move.
- Removed behavior: left-drag no longer pans (main.cpp drives pan via `pan()`).

- [ ] **Step 1: Write the failing test** — replace the entire contents of `tests/test_input.cpp` with:

```cpp
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "input.hpp"
#include "iso_project.hpp"
#include "world.hpp"

#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"

using namespace ra2assets;
using ra2engine::Camera;
using ra2engine::Input;
using ra2engine::InputEvent;
using ra2engine::World;

namespace {

std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
bool load_real_map(MapFile& out) {
    const char* d = std::getenv("RA2_INSTALL_DIR");
    std::string dir = d ? d : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    auto b = slurp(dir + "mapsmd03.mix");
    if (b.empty()) return false;
    MixFile mix = MixFile::open(std::move(b));
    const MixEntry* e = mix.find(0x133a0cb2u);
    if (!e) return false;
    auto raw = mix.read(*e);
    out = MapFile::parse(std::string(raw.begin(), raw.end()));
    return true;
}

// Screen pixel over the cell (cx, cy) ground point at identity camera, level 0.
void px_for_cell(int cx, int cy, int& px, int& py) {
    const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cx, cy, 0, 60, 30, 15);
    px = p.x + 30;  // + tile_w/2 (ground point x)
    py = p.y + 15;  // + tile_h/2 (ground point y)
}

// Left click helper (down then up, no move) with modifiers + timestamp.
void left_click(Input& in, int px, int py, bool shift, uint32_t t_ms) {
    InputEvent d;
    d.kind = InputEvent::PointerDown; d.button = InputEvent::Left; d.x = px; d.y = py;
    d.shift = shift; d.t_ms = t_ms;
    InputEvent u = d;
    u.kind = InputEvent::PointerUp;
    in.handle(d);
    in.handle(u);
}

// Spawn `n` GI units at consecutive walkable cells near the map's start.
struct Fixture {
    MapFile map;
    bool ok = false;
    std::vector<std::pair<int, int>> cells;
};
Fixture spawn(World& world, int n) {
    Fixture fx;
    const MapCell* c = ra2engine::pick_start_cell(world.map());
    int placed = 0;
    for (int dx = 0; dx < 8 && placed < n; ++dx) {
        const int x = c->x + dx, y = c->y;
        if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
            world.add_unit(x, y, "GI", 0);
            fx.cells.push_back({x, y});
            ++placed;
        }
    }
    fx.ok = placed == n;
    return fx;
}

}  // namespace

TEST_CASE("Input: left-click a unit selects only it; empty click clears") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped: could not place 3 units"); return; }

    int px, py;
    px_for_cell(fx.cells[1].first, fx.cells[1].second, px, py);
    left_click(input, px, py, /*shift=*/false, /*t=*/1000);
    CHECK(world.units()[1].selected);
    CHECK_FALSE(world.units()[0].selected);
    CHECK_FALSE(world.units()[2].selected);

    // Click far-away empty ground clears selection.
    left_click(input, px + 900, py + 400, false, 2000);
    for (const auto& u : world.units()) CHECK_FALSE(u.selected);
}

TEST_CASE("Input: Shift+left-click toggles units in and out of the selection") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    int px0, py0, px2, py2;
    px_for_cell(fx.cells[0].first, fx.cells[0].second, px0, py0);
    px_for_cell(fx.cells[2].first, fx.cells[2].second, px2, py2);
    left_click(input, px0, py0, false, 1000);   // select unit 0
    left_click(input, px2, py2, true, 2000);    // shift-add unit 2
    CHECK(world.units()[0].selected);
    CHECK(world.units()[2].selected);
    left_click(input, px0, py0, true, 3000);    // shift-remove unit 0
    CHECK_FALSE(world.units()[0].selected);
    CHECK(world.units()[2].selected);
}

TEST_CASE("Input: right-click issues a group move to the selected units") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    world.set_move_speed(4.0);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 2);
    if (!fx.ok) { MESSAGE("skipped"); return; }
    for (auto& u : world.units()) u.selected = true;

    // Right-click a cell 10 south of the first unit.
    int px, py;
    px_for_cell(fx.cells[0].first, fx.cells[0].second + 10, px, py);
    InputEvent rc;
    rc.kind = InputEvent::PointerDown; rc.button = InputEvent::Right; rc.x = px; rc.y = py;
    rc.t_ms = 5000;
    input.handle(rc);
    InputEvent ru = rc; ru.kind = InputEvent::PointerUp;
    input.handle(ru);

    // At least one selected unit now has an active path (some may already be
    // adjacent to their assigned cell).
    int moving = 0;
    for (const auto& u : world.units()) moving += u.moving() ? 1 : 0;
    CHECK(moving >= 1);
}

TEST_CASE("Input: left-drag exposes a selection box and box-selects covered units") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    // Compute the ground pixels of all three units, then drag a box covering them.
    int minx = 1000000000, miny = 1000000000, maxx = -1000000000, maxy = -1000000000;
    for (auto& cxy : fx.cells) {
        int px, py; px_for_cell(cxy.first, cxy.second, px, py);
        minx = std::min(minx, px); maxx = std::max(maxx, px);
        miny = std::min(miny, py); maxy = std::max(maxy, py);
    }
    InputEvent d; d.kind = InputEvent::PointerDown; d.button = InputEvent::Left;
    d.x = minx - 20; d.y = miny - 20; d.t_ms = 1000;
    input.handle(d);
    InputEvent m; m.kind = InputEvent::PointerMove; m.x = maxx + 20; m.y = maxy + 20; m.t_ms = 1050;
    input.handle(m);
    int x0, y0, x1, y1;
    CHECK(input.selection_box(x0, y0, x1, y1));  // box is live during the drag
    InputEvent u; u.kind = InputEvent::PointerUp; u.button = InputEvent::Left;
    u.x = maxx + 20; u.y = maxy + 20; u.t_ms = 1100;
    input.handle(u);
    CHECK_FALSE(input.selection_box(x0, y0, x1, y1));  // box gone after release
    for (const auto& un : world.units()) CHECK(un.selected);
}

TEST_CASE("Input: double-click selects all on-screen units of the same type") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    // Center the camera on unit 0 so all three (adjacent cells) are on-screen
    // -- commit_double only selects units whose ground point lies inside the
    // viewport. Input holds cam by reference, so mutating it here is live.
    int gx0, gy0;
    px_for_cell(fx.cells[0].first, fx.cells[0].second, gx0, gy0);
    cam.x = 1280 / 2 - gx0;
    cam.y = 720 / 2 - gy0;

    // Two quick clicks on unit 0's cell (window px = ground px + camera).
    const int px = gx0 + cam.x, py = gy0 + cam.y;
    left_click(input, px, py, false, 1000);
    CHECK(world.units()[0].selected);           // first click: single select
    CHECK_FALSE(world.units()[1].selected);
    left_click(input, px, py, false, 1200);     // 200ms later, same spot: double
    for (const auto& u : world.units()) CHECK(u.selected);  // all GI on screen
}

TEST_CASE("Input: pan moves the camera (clamped) and never fires a select") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    int mnx, mny, mxx, mxy;
    REQUIRE(ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy));
    cam.x = 1280 / 2 - (mnx + mxx) / 2;
    cam.y = 720 / 2 - (mny + mxy) / 2;
    Input input(world, cam, 1280, 720);
    const int x0 = cam.x, y0 = cam.y;
    input.pan(-40, -25);
    CHECK(cam.x == x0 - 40);
    CHECK(cam.y == y0 - 25);
}
```

- [ ] **Step 2: Run to verify it fails.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests`
Expected: FAIL — `InputEvent::button/shift/t_ms`, `Input::pan`, `Input::selection_box` do not exist.

- [ ] **Step 3: Extend `InputEvent`** in `engine/platform.hpp`. Replace the `struct InputEvent { ... };` with:

```cpp
struct InputEvent {
    enum Kind { None, Quit, PointerDown, PointerUp, PointerMove } kind = None;
    int x = 0, y = 0;                       // pointer position (window pixels)
    enum Button { Left, Right } button = Left;  // valid on PointerDown/PointerUp
    bool shift = false;                     // Shift held at event time
    uint32_t t_ms = 0;                      // event timestamp (ms) for double-click
};
```

Add `#include <cstdint>` at the top of `platform.hpp`.

- [ ] **Step 4: Fill the new fields** in `engine/platform.cpp`'s `poll`. Replace the mouse/finger cases:

```cpp
        case SDL_MOUSEBUTTONDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = e.button.x;
            ev.y = e.button.y;
            ev.button = (e.button.button == SDL_BUTTON_RIGHT) ? InputEvent::Right
                                                              : InputEvent::Left;
            ev.shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            ev.t_ms = e.button.timestamp;
            break;
        case SDL_MOUSEBUTTONUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = e.button.x;
            ev.y = e.button.y;
            ev.button = (e.button.button == SDL_BUTTON_RIGHT) ? InputEvent::Right
                                                              : InputEvent::Left;
            ev.shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            ev.t_ms = e.button.timestamp;
            break;
        case SDL_MOUSEMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = e.motion.x;
            ev.y = e.motion.y;
            ev.t_ms = e.motion.timestamp;
            break;
        case SDL_FINGERDOWN:
            ev.kind = InputEvent::PointerDown;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.button = InputEvent::Left;  // touch maps to the left/primary button
            ev.t_ms = e.tfinger.timestamp;
            break;
        case SDL_FINGERUP:
            ev.kind = InputEvent::PointerUp;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.button = InputEvent::Left;
            ev.t_ms = e.tfinger.timestamp;
            break;
        case SDL_FINGERMOTION:
            ev.kind = InputEvent::PointerMove;
            ev.x = static_cast<int>(e.tfinger.x * p_->w);
            ev.y = static_cast<int>(e.tfinger.y * p_->h);
            ev.t_ms = e.tfinger.timestamp;
            break;
```

- [ ] **Step 5: Rewrite `Input`'s declaration** in `engine/input.hpp`. Replace the `class Input { ... };` body with:

```cpp
class Input {
public:
    Input(World& world, Camera& camera, int view_w, int view_h);

    // Feed one polled event (RA2-classic model, see input.cpp).
    void handle(const InputEvent& ev);

    // Pan the camera by (dx, dy) window pixels, clamped to the map bounds.
    // Called by main.cpp for arrow-key + screen-edge scrolling (left-drag no
    // longer pans -- it draws the selection box).
    void pan(int dx, int dy);

    // If a left-drag selection box is active, fill its normalized rectangle
    // (x0<=x1, y0<=y1) in window pixels and return true; else return false.
    bool selection_box(int& x0, int& y0, int& x1, int& y1) const;

    bool wants_quit() const { return quit_; }

private:
    void clamp_camera();
    int unit_under(int px, int py) const;    // screen pixel -> unit index or -1
    void unit_ground_px(const Unit& u, int& gx, int& gy) const;  // ground point in window px
    void commit_click(int px, int py, bool shift);
    void commit_box(bool shift);
    void commit_double(int px, int py);

    World& world_;
    Camera& cam_;
    bool quit_ = false;

    // Left-button gesture state.
    bool l_down_ = false;
    bool dragging_ = false;
    bool down_shift_ = false;
    int down_x_ = 0, down_y_ = 0, cur_x_ = 0, cur_y_ = 0;

    // Double-click detection (uses event t_ms; no clock read here).
    uint32_t last_click_t_ = 0;
    int last_click_x_ = 0, last_click_y_ = 0;

    // Camera clamp bounds (computed once from the map's screen bbox + viewport;
    // same rule as the previous drag clamp -- see input.cpp).
    int view_w_ = 0, view_h_ = 0;
    int min_cam_x_ = 0, max_cam_x_ = 0, min_cam_y_ = 0, max_cam_y_ = 0;
};
```

Add `#include <cstdint>` to `input.hpp`.

- [ ] **Step 6: Rewrite `engine/input.cpp`.** Replace the whole file with:

```cpp
#include "input.hpp"

#include <climits>
#include <cmath>
#include <cstdlib>

#include "iso_project.hpp"  // screen_to_cell

namespace ra2engine {

static constexpr int kTileW = 60;
static constexpr int kTileH = 30;
static constexpr int kLevelPx = 15;

// Pointer travel (window px) past which a left press becomes a selection-box
// drag rather than a click.
static constexpr int kDragThreshold = 6;
// Double-click window (ms) and max pixel slop between the two clicks.
static constexpr uint32_t kDblMs = 400;
static constexpr int kDblSlop = 6;

Input::Input(World& world, Camera& camera, int view_w, int view_h)
    : world_(world), cam_(camera), view_w_(view_w), view_h_(view_h) {
    int mnx, mny, mxx, mxy;
    if (terrain_screen_bounds(world_.map(), mnx, mny, mxx, mxy)) {
        const int margin_x = view_w_ / 2;
        const int margin_y = view_h_ / 2;
        min_cam_x_ = -margin_x - mxx;
        max_cam_x_ = view_w_ + margin_x - mnx;
        min_cam_y_ = -margin_y - mxy;
        max_cam_y_ = view_h_ + margin_y - mny;
    } else {
        min_cam_x_ = INT_MIN; max_cam_x_ = INT_MAX;
        min_cam_y_ = INT_MIN; max_cam_y_ = INT_MAX;
    }
}

void Input::clamp_camera() {
    if (cam_.x < min_cam_x_) cam_.x = min_cam_x_;
    else if (cam_.x > max_cam_x_) cam_.x = max_cam_x_;
    if (cam_.y < min_cam_y_) cam_.y = min_cam_y_;
    else if (cam_.y > max_cam_y_) cam_.y = max_cam_y_;
}

void Input::pan(int dx, int dy) {
    cam_.x += dx;
    cam_.y += dy;
    clamp_camera();
}

void Input::unit_ground_px(const Unit& u, int& gx, int& gy) const {
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    gx = static_cast<int>(std::lround((u.fx - u.fy) * hw)) + kTileW / 2 + cam_.x;
    gy = static_cast<int>(std::lround((u.fx + u.fy) * hh - u.level * kLevelPx)) + kTileH / 2 +
         cam_.y;
}

int Input::unit_under(int px, int py) const {
    // Cell-based pick: invert the pointer (undo camera) to an iso cell, then ask
    // the World which unit stands there. Unambiguous because occupancy keeps at
    // most one unit per cell.
    int cx = 0, cy = 0;
    screen_to_cell(px - cam_.x, py - cam_.y, kTileW, kTileH, cx, cy);
    return world_.unit_at_cell(cx, cy);
}

bool Input::selection_box(int& x0, int& y0, int& x1, int& y1) const {
    if (!(l_down_ && dragging_)) return false;
    x0 = std::min(down_x_, cur_x_);
    x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_);
    y1 = std::max(down_y_, cur_y_);
    return true;
}

void Input::commit_click(int px, int py, bool shift) {
    const int idx = unit_under(px, py);
    if (idx >= 0) {
        if (shift) {
            world_.units()[idx].selected = !world_.units()[idx].selected;  // toggle
        } else {
            world_.clear_selection();
            world_.units()[idx].selected = true;
        }
    } else if (!shift) {
        world_.clear_selection();  // click on empty ground clears (Shift+empty = no-op)
    }
}

void Input::commit_box(bool shift) {
    int x0, y0, x1, y1;
    x0 = std::min(down_x_, cur_x_); x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_); y1 = std::max(down_y_, cur_y_);
    if (!shift) world_.clear_selection();  // plain box replaces; Shift+box adds
    for (Unit& u : world_.units()) {
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= x0 && gx <= x1 && gy >= y0 && gy <= y1) u.selected = true;
    }
}

void Input::commit_double(int px, int py) {
    const int idx = unit_under(px, py);
    if (idx < 0) return;
    const std::string type = world_.units()[idx].image;
    world_.clear_selection();
    for (Unit& u : world_.units()) {
        if (u.image != type) continue;
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= 0 && gx < view_w_ && gy >= 0 && gy < view_h_) u.selected = true;  // on-screen
    }
}

void Input::handle(const InputEvent& ev) {
    switch (ev.kind) {
        case InputEvent::Quit:
            quit_ = true;
            break;

        case InputEvent::PointerDown:
            if (ev.button == InputEvent::Right) {
                // Right-click = group-move the current selection (issued on down).
                int cx = 0, cy = 0;
                screen_to_cell(ev.x - cam_.x, ev.y - cam_.y, kTileW, kTileH, cx, cy);
                world_.order_move_selected(cx, cy);
            } else {
                l_down_ = true;
                dragging_ = false;
                down_shift_ = ev.shift;
                down_x_ = cur_x_ = ev.x;
                down_y_ = cur_y_ = ev.y;
            }
            break;

        case InputEvent::PointerMove:
            cur_x_ = ev.x;
            cur_y_ = ev.y;
            if (l_down_ && !dragging_ &&
                (std::abs(ev.x - down_x_) > kDragThreshold ||
                 std::abs(ev.y - down_y_) > kDragThreshold)) {
                dragging_ = true;
            }
            break;

        case InputEvent::PointerUp:
            if (ev.button == InputEvent::Right) break;
            if (l_down_) {
                if (dragging_) {
                    commit_box(down_shift_);
                } else {
                    const bool is_double =
                        (ev.t_ms - last_click_t_) <= kDblMs &&
                        std::abs(ev.x - last_click_x_) <= kDblSlop &&
                        std::abs(ev.y - last_click_y_) <= kDblSlop;
                    if (is_double) {
                        commit_double(ev.x, ev.y);
                        last_click_t_ = 0;  // consume, so a triple-click isn't a 2nd double
                    } else {
                        commit_click(ev.x, ev.y, down_shift_);
                        last_click_t_ = ev.t_ms;
                        last_click_x_ = ev.x;
                        last_click_y_ = ev.y;
                    }
                }
            }
            l_down_ = false;
            dragging_ = false;
            break;

        default:
            break;
    }
}

}  // namespace ra2engine
```

Add `#include <algorithm>` to `input.cpp` (for `std::min`/`std::max`).

- [ ] **Step 7: Run the tests to verify they pass.**

Run: `cmake --build D:\ra2-android\build --target ra2assets_tests` then `$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & D:\ra2-android\build\ra2assets_tests.exe -tc="Input*"`
Expected: PASS — 7 cases pass (or skip cleanly without the install).

- [ ] **Step 8: Commit.**

```bash
git -C D:\ra2-android add engine/platform.hpp engine/platform.cpp engine/input.hpp engine/input.cpp tests/test_input.cpp
git -C D:\ra2-android commit -m "engine: RA2-classic Input gestures (click/box/shift/double/right-move) + InputEvent button/shift/t_ms"
```

---

### Task 6: main.cpp integration — squad spawn, multi-unit render, box overlay, edge/arrow pan, `--demo-group`

**Files:**
- Modify: `engine/iso_renderer.hpp` / `engine/iso_renderer.cpp` (add `draw_selection_box`)
- Modify: `engine/main.cpp` (spawn squad; render over `unit_draw_order`; box overlay; pan via `input.pan`; migrate `--demo-move`/`--demo-retarget`; add `--demo-group`)

**Interfaces:**
- Consumes: everything from Tasks 4–5 (`World::units/add_unit/order_move_selected/step`, `unit_draw_order`, `Input::pan/selection_box`).
- Produces: `void draw_selection_box(void* sdl_renderer, int x0, int y0, int x1, int y1)`; a `--demo-group <n> <cellX> <cellY>` mode.

- [ ] **Step 1: Add `draw_selection_box`** to `engine/iso_renderer.hpp` (after `draw_path`):

```cpp
// Draw the live left-drag selection rectangle outline (green) in window-pixel
// space. Coordinates are the raw drag endpoints (any order). HUD overlay: call
// LAST, after terrain + units.
void draw_selection_box(void* sdl_renderer, int x0, int y0, int x1, int y1);
```

- [ ] **Step 2: Implement it** in `engine/iso_renderer.cpp` (after `draw_path`):

```cpp
void draw_selection_box(void* sdl_renderer, int x0, int y0, int x1, int y1) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    SDL_Rect r;
    r.x = std::min(x0, x1);
    r.y = std::min(y0, y1);
    r.w = std::abs(x1 - x0);
    r.h = std::abs(y1 - y0);
    SDL_SetRenderDrawColor(renderer, 60, 220, 90, 255);
    SDL_RenderDrawRect(renderer, &r);
}
```

(`<algorithm>` and `<cmath>`/`<cstdlib>` are already included in iso_renderer.cpp; add `#include <cstdlib>` for `std::abs` if the build warns.)

- [ ] **Step 3: Update `render_world`** in `engine/main.cpp` to loop over all units back-to-front and draw the selection box. Replace the current `render_world` body:

```cpp
void render_world(SDL_Renderer* ren, ra2engine::TextureCache& tc, Assets& a,
                  UnitAssets& ua, ra2engine::World& world, const ra2engine::Camera& cam,
                  int W, int H, const ra2engine::Input* input = nullptr) {
    ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H);
    const std::vector<ra2engine::Unit>& us = world.units();
    // Paths + selection markers first (under the sprites).
    for (const ra2engine::Unit& u : us) {
        ra2engine::draw_path(ren, u, cam, W, H);
        if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H);
    }
    // Sprites back-to-front so nearer units overlap farther ones.
    for (int idx : ra2engine::unit_draw_order(us))
        ra2engine::draw_unit(ren, tc, us[idx], ua.shp, ua.pal, cam, W, H);
    // Live selection box on top (interactive only).
    if (input) {
        int x0, y0, x1, y1;
        if (input->selection_box(x0, y0, x1, y1))
            ra2engine::draw_selection_box(ren, x0, y0, x1, y1);
    }
}
```

- [ ] **Step 4: Replace `build_world`** in `engine/main.cpp` to spawn a squad of `count` GI units clustered near the start cell:

```cpp
// Spawn `count` GI units on distinct walkable cells spiralling out from the
// map's start cell (pick_start_cell). count==1 reproduces the old single-unit
// slice (used by the single-unit demos); the interactive/screenshot/group
// modes pass a small squad.
ra2engine::World build_world(const Assets& a, const UnitAssets& ua, int count) {
    ra2engine::World world(a.map);
    if (ua.speed > 0) world.set_move_speed(ua.speed * kCellsPerSecPerSpeed);

    const ra2assets::MapCell* c = ra2engine::pick_start_cell(a.map);
    int placed = 0;
    for (int r = 0; r <= 12 && placed < count; ++r)
        for (int dy = -r; dy <= r && placed < count; ++dy)
            for (int dx = -r; dx <= r && placed < count; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // ring border only
                const int x = c->x + dx, y = c->y + dy;
                if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                    world.add_unit(x, y, ua.image, 0);
                    ++placed;
                }
            }
    std::cerr << "[world] spawned " << world.units().size() << " unit(s) near (" << c->x << ","
              << c->y << ") grid=" << world.grid_w() << "x" << world.grid_h()
              << " move_cps=" << world.move_speed() << "\n";
    return world;
}
```

- [ ] **Step 5: Update the call sites** in `engine/main.cpp`:

- In `run_screenshot`: `ra2engine::World world = build_world(a, ua, 5);`
- In `run_interactive`: `ra2engine::World world = build_world(a, ua, 5);`
- In `run_demo_move`: `ra2engine::World world = build_world(a, ua, 1);` then replace the select+order block:

```cpp
    const int start_x = world.units()[0].cell_x;
    const int start_y = world.units()[0].cell_y;
    world.units()[0].selected = true;
    world.order_move_selected(goal_x, goal_y);
    const std::size_t path_len = world.units()[0].path.size();
```

  and replace subsequent `world.unit()` references with `world.units()[0]` (the `moving()`/`cell_x`/`cell_y` reads in the sim loop and final prints).

- In `run_demo_retarget`: `ra2engine::World world = build_world(a, ua, 1);`, `world.units()[0].selected = true;`, replace `world.order_move(x1,y1)` with `world.order_move_selected(x1, y1)` and read `len1 = world.units()[0].path.size();` (0 if not moving); same for the retarget call; replace all `world.unit()` reads with `world.units()[0]`.

- In `run_interactive`, replace the drag-pan-era arrow block so panning goes through `input.pan` (keeps the clamp), and add screen-edge scrolling:

```cpp
        // Camera pan: arrow keys + screen-edge scroll (left-drag now box-selects).
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int pan_dx = 0, pan_dy = 0;
        if (ks[SDL_SCANCODE_LEFT]) pan_dx += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) pan_dx -= kPan;
        if (ks[SDL_SCANCODE_UP]) pan_dy += kPan;
        if (ks[SDL_SCANCODE_DOWN]) pan_dy -= kPan;
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        const int kEdge = 24;  // px band at the viewport edge that scrolls
        if (mx <= kEdge) pan_dx += kPan;
        else if (mx >= platform.width() - kEdge) pan_dx -= kPan;
        if (my <= kEdge) pan_dy += kPan;
        else if (my >= platform.height() - kEdge) pan_dy -= kPan;
        if (pan_dx || pan_dy) input.pan(pan_dx, pan_dy);
```

  and update the `render_world(...)` call in `run_interactive` to pass `&input` as the last argument.

- [ ] **Step 6: Add the `--demo-group` mode.** Add this function (after `run_demo_retarget`, before the closing `}  // namespace`):

```cpp
// Non-interactive verification of the multi-unit pipeline: spawn `n` GI units,
// select them all, group-move to (gx, gy), run the sim to arrival (or a cap),
// and write before/after screenshots. Asserts the no-overlap invariant every
// step and prints per-unit arrival cells. Screenshots -> scratchpad only.
int run_demo_group(int n, int gx, int gy) {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_group_start.png";
    const std::string end_png = scratch + "demo_group_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, n);
    for (auto& u : world.units()) u.selected = true;
    world.order_move_selected(gx, gy);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-group)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    bool overlap_seen = false;
    double sim_time = 0.0;
    {
        ra2engine::TextureCache tc(ren);
        ra2engine::Camera cam;
        {
            const ra2engine::ScreenPt mid = ra2engine::cell_to_screen(
                gx, gy, 0, 60, 30, 15);
            cam.x = W / 2 - mid.x;
            cam.y = H / 2 - mid.y;
        }
        auto draw = [&]() {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, tc, a, ua, world, cam, W, H);
        };
        auto any_moving = [&]() {
            for (const auto& u : world.units()) if (u.moving()) return true;
            return false;
        };
        auto check_overlap = [&]() {
            const auto& us = world.units();
            for (size_t i = 0; i < us.size(); ++i)
                for (size_t j = i + 1; j < us.size(); ++j)
                    if (us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y)
                        overlap_seen = true;
        };

        draw();
        capture_png(ren, start_png, W, H);

        const double dt = 1.0 / 60.0;
        const double kMaxSim = 30.0;
        while (any_moving() && sim_time < kMaxSim) {
            world.step(dt);
            check_overlap();
            sim_time += dt;
            draw();
        }
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cout << "units=" << world.units().size() << " target=(" << gx << "," << gy
              << ") sim_time=" << sim_time << "s overlap_seen=" << (overlap_seen ? "YES" : "no")
              << "\n";
    for (size_t i = 0; i < world.units().size(); ++i)
        std::cout << "  unit " << i << " final=(" << world.units()[i].cell_x << ","
                  << world.units()[i].cell_y << ")\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    return overlap_seen ? 1 : 0;  // fail if the invariant was ever violated
}
```

- [ ] **Step 7: Parse `--demo-group`** in `main()`. In the arg loop add:

```cpp
            } else if (arg == "--demo-group" && i + 3 < argc) {
                demo_group = true;
                group_n = std::atoi(argv[++i]);
                group_x = std::atoi(argv[++i]);
                group_y = std::atoi(argv[++i]);
```

Declare `bool demo_group = false; int group_n = 5, group_x = 0, group_y = 0;` next to the other demo flags, and before the other dispatch `if`s add:

```cpp
        if (demo_group) return run_demo_group(group_n, group_x, group_y);
```

- [ ] **Step 8: Build the engine and run the group demo.**

Run: `cmake --build D:\ra2-android\build --target ra2engine`
Expected: links cleanly (SDL2.dll copied next to the exe by the existing POST_BUILD step).

Run (PowerShell): `$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & D:\ra2-android\build\ra2engine.exe --demo-group 5 118 120`
Expected: exit 0, stdout shows `overlap_seen=no`, five distinct `final=(...)` cells, and two screenshots in the scratchpad. Open `demo_group_end.png` and confirm five GI sprites at distinct nearby cells (visual gate).

- [ ] **Step 9: Sanity-run the migrated single-unit demos + screenshot.**

Run: `& D:\ra2-android\build\ra2engine.exe --demo-move 118 122` (expect a non-zero path and arrival), `& D:\ra2-android\build\ra2engine.exe --screenshot "$env:TEMP\phaseA_shot.png"` (expect a frame with a squad rendered). Delete the temp PNG afterward (not committed).

- [ ] **Step 10: Commit.**

```bash
git -C D:\ra2-android add engine/iso_renderer.hpp engine/iso_renderer.cpp engine/main.cpp
git -C D:\ra2-android commit -m "engine: multi-unit render + selection box + edge/arrow pan + --demo-group; migrate demos"
```

---

### Task 7: Finalize — full suite, arm64 cross-compile, README

**Files:**
- Modify: `README.md` (engine controls + Phase A features)
- No code changes.

- [ ] **Step 1: Run the FULL test suite with assets.**

Run (PowerShell): `cmake --build D:\ra2-android\build --target ra2assets_tests` then `$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & D:\ra2-android\build\ra2assets_tests.exe`
Expected: `[doctest] Status: SUCCESS!`, 0 failed, 0 skipped (all real-asset tests ran). Note the new case/assertion totals (previous baseline: 123 cases / 99300 assertions).

- [ ] **Step 2: Verify the suite still passes WITHOUT the install (clean skips).**

Run: `$env:RA2_INSTALL_DIR = "Z:\nope"; & D:\ra2-android\build\ra2assets_tests.exe`
Expected: `SUCCESS!`, the guarded World/Input/oracle tests print skip MESSAGEs, 0 failed. (Restore `RA2_INSTALL_DIR` afterward.)

- [ ] **Step 3: Cross-compile the library + engine sources for arm64** (integration gate — the new engine modules must build under the NDK, reused by the Android app).

Run (PowerShell):
```powershell
$env:ANDROID_NDK_HOME = "D:\android-ndk-r27c"
cmake -S D:\ra2-android -B D:\ra2-android\build-android -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 `
  -DRA2_BUILD_ENGINE=OFF -DRA2_BUILD_SHARED=ON
cmake --build D:\ra2-android\build-android --target ra2assets ra2assets_so
```
Expected: builds clean. (The new `engine/occupancy.cpp` + `engine/group_move.cpp` are pure and in `RA2_ENGINE_SOURCES`; this target set builds the library, but the Android jni build — which compiles `RA2_ENGINE_SOURCES` into `libmain.so` — now includes them via `cmake/ra2_sources.cmake` with no jni edits. Confirm the two files appear in the shared source list.)

- [ ] **Step 4: Update `README.md`.** In the intro paragraph, change the `ra2engine` description to mention multiple units + RA2 controls; e.g. replace:

```
select and move a unit (A\* pathfinding + walk animation).
```

with:

```
select and command a squad of units (RA2-classic controls: left-click / box /
Shift to select, right-click to group-move) with A\* pathfinding, grid-occupancy
collision avoidance, and walk animation.
```

In the "Engine (desktop)" controls list, replace the interactive-controls line with:

```
Interactive controls: left-click a unit to select it; left-drag a box to select
many; Shift+left-click to add/remove; double-click a unit to select all of its
type on screen; right-click to move the selection (units fan out to distinct
cells and avoid each other); pan with the arrow keys or by pushing the pointer
to a screen edge; Esc/close quits.
```

And add `--demo-group <n> <cellX> <cellY>` to the headless-modes list:

```
- `ra2engine.exe --demo-group <n> <cellX> <cellY>` — spawn `n` GI units, select
  all, group-move to the cell, run to arrival; writes `demo_group_start.png`/
  `demo_group_end.png` to the scratchpad and exits non-zero if any two units
  ever shared a cell.
```

- [ ] **Step 5: Commit.**

```bash
git -C D:\ra2-android add README.md
git -C D:\ra2-android commit -m "docs: README — Phase A multi-unit controls + --demo-group"
```

---

## Self-Review

**1. Spec coverage:**
- Full RTS control layer (box, Shift, double-click, right-click move) → Task 5. ✅
- RA2-classic mouse + arrow/edge pan (no left-drag-pan) → Task 5 (`pan`, gestures) + Task 6 (edge/arrow). ✅
- Grid-occupancy + reservation movement, formation assignment → Tasks 1–4. ✅
- Hard cases (head-on, boxed-in, funnel) → Task 3 tests. ✅
- Migration of `world.unit()`/old Input tests/demos → Tasks 4 (World API), 5 (test_input rewrite), 6 (demos). ✅
- Rendering: loop over units, depth sort, selection box → Task 4 (`unit_draw_order`) + Task 6. ✅
- Testing: pure module tests + guarded World/Input + `--demo-group` → Tasks 1–6. ✅
- Success criteria (squad spawn, 6 gestures, distinct cells + no-overlap invariant, collision cases, pan, desktop + arm64) → covered across Tasks 3–7. ✅
- Non-goals honored (no combat/health/2nd type/voxel/touch/HUD). ✅

**2. Placeholder scan:** No TBD/TODO; every code + test step shows complete content; commands have expected output. ✅

**3. Type consistency:** `Occupancy` (kEmpty/at/is_free/set/clear/release/can_enter) consistent across Tasks 1→3→4. `assign_formation`/`nearest_free_cells` signatures match Task 2 def → Task 4 use. `step_units` signature matches Task 3 def → Task 4 use. `Unit::wait_time` added Task 3, used Task 3+4. `unit_draw_order` def Task 4 → use Task 6. `Input::pan/selection_box`/`InputEvent::button/shift/t_ms` def Task 5 → use Task 6. `draw_selection_box` def Task 6 Step 1 → use Task 6 Step 3. ✅

**Known simplification (documented, not a gap):** click selection is cell-based (`screen_to_cell` → `unit_at_cell`), so a click must land on the unit's tile, not merely its sprite pixels — chosen for unambiguity under the one-unit-per-cell model (spec §Selection). Acceptable for this phase.
