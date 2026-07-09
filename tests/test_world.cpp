#include "doctest.h"

#include <cstdlib>
#include <fstream>
#include <vector>

#include "iso_project.hpp"  // cell_to_screen / screen_to_cell
#include "occupancy.hpp"
#include "pathfind.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "world.hpp"

using ra2assets::MapCell;
using ra2engine::build_passability;

namespace {

MapCell cell(int x, int y) {
    MapCell c;
    c.x = x;
    c.y = y;
    c.tile = 0;
    return c;
}

int gidx(int w, int x, int y) { return y * w + x; }

}  // namespace

TEST_CASE("build_passability: empty cells -> empty grid, zero dims") {
    std::vector<MapCell> cells;
    int gw = -1, gh = -1;
    auto grid = build_passability(cells, gw, gh);
    CHECK(gw == 0);
    CHECK(gh == 0);
    CHECK(grid.empty());
}

TEST_CASE("build_passability: grid sized to max+1, marks present cells passable") {
    // Cells at (1,2), (5,7), (3,3). Max x = 5, max y = 7 -> grid 6x8.
    std::vector<MapCell> cells = {cell(1, 2), cell(5, 7), cell(3, 3)};
    int gw = 0, gh = 0;
    auto grid = build_passability(cells, gw, gh);

    REQUIRE(gw == 6);
    REQUIRE(gh == 8);
    REQUIRE(grid.size() == static_cast<size_t>(gw) * gh);

    // Present cells are passable...
    CHECK(grid[gidx(gw, 1, 2)] == 1);
    CHECK(grid[gidx(gw, 5, 7)] == 1);
    CHECK(grid[gidx(gw, 3, 3)] == 1);
    // ...every other coordinate is impassable (off-map / no record).
    CHECK(grid[gidx(gw, 0, 0)] == 0);
    CHECK(grid[gidx(gw, 2, 2)] == 0);
    CHECK(grid[gidx(gw, 4, 7)] == 0);

    // Exactly the three materialized cells are passable.
    int passable_count = 0;
    for (auto v : grid) passable_count += (v != 0);
    CHECK(passable_count == 3);
}

// The subtle coordinate reconciliation this task depends on: MapCell.x/y, the
// passability grid index, and iso_project's cell_to_screen/screen_to_cell all
// live in the SAME raw iso space. So projecting a materialized cell to screen
// and inverting must return that same cell, and that cell must be passable in
// the grid built from the same cells -- i.e. a click that lands on a mapped
// tile resolves to an in-grid, walkable cell the unit can be pathed to.
TEST_CASE("coordinate reconciliation: cell -> screen -> cell round-trips into the passable grid") {
    std::vector<MapCell> cells = {cell(10, 12), cell(40, 55), cell(110, 112), cell(2, 200)};
    int gw = 0, gh = 0;
    auto grid = build_passability(cells, gw, gh);

    for (const auto& c : cells) {
        const ra2engine::ScreenPt p = ra2engine::cell_to_screen(c.x, c.y, 0, 60, 30, 15);
        int rx = -1, ry = -1;
        ra2engine::screen_to_cell(p.x, p.y, 60, 30, rx, ry);
        CHECK(rx == c.x);
        CHECK(ry == c.y);
        REQUIRE(rx >= 0);
        REQUIRE(ry >= 0);
        REQUIRE(rx < gw);
        REQUIRE(ry < gh);
        CHECK(grid[gidx(gw, rx, ry)] == 1);  // the inverted cell is walkable
    }
}

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
                CHECK_FALSE((us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y));
    }
    // Distinct final cells.
    const auto& us = world.units();
    for (size_t i = 0; i < us.size(); ++i)
        for (size_t j = i + 1; j < us.size(); ++j)
            CHECK_FALSE((us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y));
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
