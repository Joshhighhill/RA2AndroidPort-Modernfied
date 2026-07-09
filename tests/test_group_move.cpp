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
            CHECK_FALSE((cells[i].x == cells[j].x && cells[i].y == cells[j].y));
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
            CHECK_FALSE((targets[i].x == targets[j].x && targets[i].y == targets[j].y));
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
            CHECK_FALSE((targets[i].x == targets[j].x && targets[i].y == targets[j].y));
}
