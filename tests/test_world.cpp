#include "doctest.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <tuple>
#include <vector>

#include "iso_project.hpp"   // cell_to_screen / screen_to_cell
#include "iso_renderer.hpp"  // pal_for_house
#include "occupancy.hpp"
#include "pathfind.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"  // Palette/Rgba (pal_for_house tests)
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

// Scan outward from (x0, y0) for the first offset (skipping skip_dx/skip_dy)
// that find_path -- the same algorithm order_move_selected uses -- reaches
// with a real (>1 cell) route. Pure probing: no World state is touched.
constexpr int kProbeDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                                  {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
bool find_reachable(const ra2engine::World& w, int x0, int y0, int skip_dx, int skip_dy,
                    int& ox, int& oy) {
    for (int r = 4; r <= 40; r += 2)
        for (const auto& d : kProbeDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            if (dx == skip_dx && dy == skip_dy) continue;
            auto p = ra2engine::find_path(w.passable(), w.grid_w(), w.grid_h(),
                                          {x0, y0}, {x0 + dx, y0 + dy});
            if (p.size() > 1) { ox = x0 + dx; oy = y0 + dy; return true; }
        }
    return false;
}

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

// GI (house 0, M60) / Conscript (house 1, M1Carbine) combat stats -- ground
// truth from docs/re/combat.md / the Combat Phase's extracted rulesmd values
// (both Strength=125, SA warhead), same numbers test_combat.cpp's
// make_soldier helper uses.
ra2engine::UnitCombat gi_stats() {
    ra2engine::UnitCombat c;
    c.strength = 125;
    c.armor = "none";
    c.image = "GI";
    c.damage = 15;
    c.rof = 20;
    c.range = 4.0;
    c.verses = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};  // kSAVerses
    return c;
}
ra2engine::UnitCombat cons_stats() {
    ra2engine::UnitCombat c;
    c.strength = 125;
    c.armor = "flak";
    c.image = "CONS";
    c.damage = 15;
    c.rof = 25;
    c.range = 4.0;
    c.verses = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};  // kSAVerses
    return c;
}

// Spawn `n` combat units of `house` at walkable+unoccupied cells scanning a
// small square from (base_x, base_y) -- same walkable()+unit_at_cell()<0
// scan the Input-test spawn helper (tests/test_input.cpp's `spawn`) and this
// file's own "group move" test use, just via add_combat_unit instead of
// add_unit. Returns the placed units' indices (fewer than n if the area is
// too crowded/blocked).
std::vector<int> spawn_combat_squad(ra2engine::World& world, int base_x, int base_y, int house,
                                    const ra2engine::UnitCombat& stats, int n) {
    std::vector<int> idx;
    for (int dy = 0; dy < 8 && static_cast<int>(idx.size()) < n; ++dy)
        for (int dx = 0; dx < 8 && static_cast<int>(idx.size()) < n; ++dx) {
            const int x = base_x + dx, y = base_y + dy;
            if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                world.add_combat_unit(x, y, house, stats);
                idx.push_back(static_cast<int>(world.units().size()) - 1);
            }
        }
    return idx;
}

// Spawn two 3-unit squads (GI house 0 / Conscript house 1) INTERLEAVED at
// walkable+unoccupied cells scanning outward from (base_x, base_y): the
// (placed % 2) alternation means every two spatially-adjacent placed units
// are opposite factions, so every unit's nearest enemy is close -- unlike a
// two-separate-blocks layout (each squad scanned from its own offset base),
// which leaves the far-end unit of EACH block as nobody's "nearest enemy"
// (it's the single farthest point from the other block): in a 120s
// full-battle run against the real map this produced two mutually-idle,
// undamaged, just-out-of-weapon-range survivors that never engaged --
// confirmed by instrumenting a run before landing on this interleaved
// layout. Interleaving keeps every unit within a cell or two of an opposing
// unit throughout the fight, so a unit whose original target dies is very
// likely to have another hostile already within auto-acquire range.
struct InterleavedSquads {
    std::vector<int> gi, cons;
};
InterleavedSquads spawn_interleaved_squads(ra2engine::World& world, int base_x, int base_y,
                                           const ra2engine::UnitCombat& gi_c,
                                           const ra2engine::UnitCombat& cons_c, int n_each) {
    InterleavedSquads sq;
    int placed = 0;
    for (int dy = 0; dy < 8 && placed < 2 * n_each; ++dy)
        for (int dx = 0; dx < 8 && placed < 2 * n_each; ++dx) {
            const int x = base_x + dx, y = base_y + dy;
            if (!world.walkable(x, y) || world.unit_at_cell(x, y) >= 0) continue;
            const bool is_gi = (placed % 2) == 0;
            world.add_combat_unit(x, y, is_gi ? 0 : 1, is_gi ? gi_c : cons_c);
            const int idx = static_cast<int>(world.units().size()) - 1;
            (is_gi ? sq.gi : sq.cons).push_back(idx);
            ++placed;
        }
    return sq;
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

TEST_CASE("World: order_move_selected mid-walk retarget keeps fx/fy (no backward snap)") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: no install"); return; }
    ra2engine::World world(map);
    world.set_move_speed(3.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_unit(c->x, c->y, "GI", 0);
    world.units()[0].selected = true;

    int t1x = 0, t1y = 0;
    REQUIRE(find_reachable(world, c->x, c->y, 999999, 999999, t1x, t1y));
    world.order_move_selected(t1x, t1y);
    REQUIRE(world.units()[0].moving());

    // Step partway: 0.05s at 3 cells/sec = 0.15 cells -- genuinely mid-segment.
    world.step(0.05);
    REQUIRE(world.units()[0].moving());
    const double fx0 = world.units()[0].fx, fy0 = world.units()[0].fy;
    REQUIRE((fx0 != (double)world.units()[0].cell_x || fy0 != (double)world.units()[0].cell_y));

    int t2x = 0, t2y = 0;
    REQUIRE(find_reachable(world, world.units()[0].cell_x, world.units()[0].cell_y,
                           t1x - world.units()[0].cell_x, t1y - world.units()[0].cell_y,
                           t2x, t2y));
    world.order_move_selected(t2x, t2y);

    // The retarget must NOT snap fx/fy backward to the committed cell.
    CHECK(world.units()[0].fx == fx0);
    CHECK(world.units()[0].fy == fy0);
    CHECK(world.units()[0].moving());
    REQUIRE_FALSE(world.units()[0].path.empty());
    CHECK(world.units()[0].path.back().x == t2x);   // formation slot for a lone
    CHECK(world.units()[0].path.back().y == t2y);   // unit is the goal itself
}

TEST_CASE("World: unreachable order mid-walk cancels and snaps fx/fy cleanly") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: no install"); return; }
    ra2engine::World world(map);
    world.set_move_speed(3.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_unit(c->x, c->y, "GI", 0);
    world.units()[0].selected = true;

    int t1x = 0, t1y = 0;
    REQUIRE(find_reachable(world, c->x, c->y, 999999, 999999, t1x, t1y));
    world.order_move_selected(t1x, t1y);
    world.step(0.05);
    REQUIRE(world.units()[0].moving());
    REQUIRE((world.units()[0].fx != (double)world.units()[0].cell_x ||
             world.units()[0].fy != (double)world.units()[0].cell_y));

    // A goal FAR off-grid: the formation ring search finds no free walkable
    // cell, the unit keeps its own cell as target, pathing to self cancels ->
    // fx/fy must snap onto the committed cell, not strand mid-segment.
    world.order_move_selected(world.grid_w() + 1000, world.grid_h() + 1000);
    CHECK_FALSE(world.units()[0].moving());
    CHECK(world.units()[0].path.empty());
    CHECK(world.units()[0].fx == (double)world.units()[0].cell_x);
    CHECK(world.units()[0].fy == (double)world.units()[0].cell_y);
}

// ---- Combat integration (Task 5: add_combat_unit / order_attack / step chaining / tombstones) ----

TEST_CASE("World: two squads fight to elimination deterministically (real map)") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }

    const ra2engine::UnitCombat gi = gi_stats();
    const ra2engine::UnitCombat cons = cons_stats();
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Runs the full scenario on a fresh World (same underlying map): spawn 3
    // GIs (house 0) and 3 Conscripts (house 1) interleaved (see
    // spawn_interleaved_squads -- close-range combat, ~1-3 cells apart), each
    // GI explicitly ordered (via order_attack, selecting one unit at a time)
    // to attack its own nearest hostile, each Conscript given an explicit
    // attack on its own nearest GI directly (hostiles have no selection path
    // -- order_attack is a house-0-only command), then simulates at 60Hz to a
    // 120s cap, checking per-frame invariants throughout. Returns the final
    // (hp, cell_x, cell_y, alive, dying) tuple per unit for the bit-
    // determinism comparison across two independent runs below.
    auto run_battle = [&](ra2engine::World& world) {
        world.set_move_speed(3.0);

        InterleavedSquads sq = spawn_interleaved_squads(world, c->x, c->y, gi, cons, 3);
        std::vector<int>& gi_idx = sq.gi;
        std::vector<int>& cons_idx = sq.cons;
        REQUIRE(gi_idx.size() == 3);
        REQUIRE(cons_idx.size() == 3);

        // Each GI attacks its OWN nearest hostile: select just that unit so
        // order_attack's "every selected unit" only ever touches it.
        for (int gi_i : gi_idx) {
            world.clear_selection();
            world.units()[gi_i].selected = true;
            int nearest = -1;
            double best_d2 = 0.0;
            for (int cons_i : cons_idx) {
                const double dx = world.units()[gi_i].cell_x - world.units()[cons_i].cell_x;
                const double dy = world.units()[gi_i].cell_y - world.units()[cons_i].cell_y;
                const double d2 = dx * dx + dy * dy;
                if (nearest == -1 || d2 < best_d2) {
                    nearest = cons_i;
                    best_d2 = d2;
                }
            }
            REQUIRE(nearest != -1);
            world.order_attack(nearest);
        }
        world.clear_selection();

        // Conscripts (hostile) have no selection path -- set target/
        // attack_order directly on each, aimed at its own nearest GI.
        for (int cons_i : cons_idx) {
            int nearest = -1;
            double best_d2 = 0.0;
            for (int gi_i : gi_idx) {
                const double dx = world.units()[cons_i].cell_x - world.units()[gi_i].cell_x;
                const double dy = world.units()[cons_i].cell_y - world.units()[gi_i].cell_y;
                const double d2 = dx * dx + dy * dy;
                if (nearest == -1 || d2 < best_d2) {
                    nearest = gi_i;
                    best_d2 = d2;
                }
            }
            REQUIRE(nearest != -1);
            world.units()[cons_i].target = nearest;
            world.units()[cons_i].attack_order = true;
        }

        const double dt = 1.0 / 60.0;
        double t = 0.0;
        bool ended = false;
        for (; t < 120.0; t += dt) {
            world.step(dt);
            const auto& us = world.units();

            // No two ALIVE-and-not-dying units ever share a cell (a dying
            // unit's occupancy was already released at the moment it started
            // dying, so it MAY coincide with a living unit's cell -- excluded
            // from this invariant by design).
            for (std::size_t i = 0; i < us.size(); ++i) {
                if (!us[i].alive || us[i].die_time >= 0.0) continue;
                for (std::size_t j = i + 1; j < us.size(); ++j) {
                    if (!us[j].alive || us[j].die_time >= 0.0) continue;
                    CHECK_FALSE((us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y));
                }
            }
            // No target ever dangles: -1, or a valid index into a unit that
            // is alive. (A target may point at a unit that is DYING but still
            // alive==true for one frame -- validation clears it on the NEXT
            // step_combat call -- so "alive", not "not dying", is the correct
            // per-frame invariant here.)
            for (const auto& u : us) {
                CHECK((u.target == -1 || (u.target >= 0 &&
                                          u.target < static_cast<int>(us.size()) &&
                                          us[u.target].alive)));
            }

            int gi_alive = 0, cons_alive = 0;
            for (int gi_i : gi_idx)
                if (us[gi_i].alive && us[gi_i].die_time < 0.0) ++gi_alive;
            for (int cons_i : cons_idx)
                if (us[cons_i].alive && us[cons_i].die_time < 0.0) ++cons_alive;
            if (gi_alive == 0 || cons_alive == 0) {
                ended = true;
                break;
            }
        }
        CHECK(ended);       // battle resolves before the 120s cap
        CHECK(t < 120.0);

        std::vector<std::tuple<int, int, int, bool, bool>> final_state;
        for (const auto& u : world.units())
            final_state.emplace_back(u.hp, u.cell_x, u.cell_y, u.alive, u.die_time >= 0.0);
        return final_state;
    };

    ra2engine::World world1(map);
    const auto final1 = run_battle(world1);

    ra2engine::World world2(map);
    const auto final2 = run_battle(world2);

    // Bit determinism: an identical run from a fresh World reaches the exact
    // same final state.
    REQUIRE(final1.size() == final2.size());
    CHECK(final1 == final2);
}

TEST_CASE("World: order_attack sets target+attack_order on selected units only") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    std::vector<int> gi_idx = spawn_combat_squad(world, c->x, c->y, 0, gi_stats(), 3);
    std::vector<int> cons_idx = spawn_combat_squad(world, c->x + 3, c->y, 1, cons_stats(), 1);
    REQUIRE(gi_idx.size() == 3);
    REQUIRE(cons_idx.size() == 1);
    const int hostile = cons_idx[0];

    // Select only the first two GIs.
    world.units()[gi_idx[0]].selected = true;
    world.units()[gi_idx[1]].selected = true;
    world.units()[gi_idx[2]].selected = false;

    world.order_attack(hostile);

    CHECK(world.units()[gi_idx[0]].target == hostile);
    CHECK(world.units()[gi_idx[0]].attack_order == true);
    CHECK(world.units()[gi_idx[1]].target == hostile);
    CHECK(world.units()[gi_idx[1]].attack_order == true);
    // Unselected GI untouched.
    CHECK(world.units()[gi_idx[2]].target == -1);
    CHECK(world.units()[gi_idx[2]].attack_order == false);

    world.clear_selection();
    world.units()[gi_idx[2]].selected = true;

    // No-op: out-of-range target index.
    world.order_attack(static_cast<int>(world.units().size()) + 5);
    CHECK(world.units()[gi_idx[2]].target == -1);
    CHECK(world.units()[gi_idx[2]].attack_order == false);

    // No-op: target is house 0 (friendly) -- can't order an attack on it.
    world.order_attack(gi_idx[0]);
    CHECK(world.units()[gi_idx[2]].target == -1);

    // No-op: dead target.
    world.units()[hostile].alive = false;
    world.order_attack(hostile);
    CHECK(world.units()[gi_idx[2]].target == -1);
    world.units()[hostile].alive = true;  // restore

    // No-op: dying target.
    world.units()[hostile].die_time = 0.2;
    world.order_attack(hostile);
    CHECK(world.units()[gi_idx[2]].target == -1);
    world.units()[hostile].die_time = -1.0;  // restore

    // A selected but dying unit does not get a target set either.
    world.units()[gi_idx[2]].die_time = 0.1;
    world.order_attack(hostile);
    CHECK(world.units()[gi_idx[2]].target == -1);
}

TEST_CASE("World: dead units are unpickable and non-occupying") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(3.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    std::vector<int> gi_idx = spawn_combat_squad(world, c->x, c->y, 0, gi_stats(), 1);
    std::vector<int> cons_idx = spawn_combat_squad(world, c->x + 3, c->y, 1, cons_stats(), 1);
    REQUIRE(gi_idx.size() == 1);
    REQUIRE(cons_idx.size() == 1);
    const int victim = cons_idx[0];
    world.units()[victim].w_damage = 0;  // disarm: isolate the GI's attack (no retaliation kill-race)

    world.units()[gi_idx[0]].selected = true;
    world.order_attack(victim);

    const int vx = world.units()[victim].cell_x;
    const int vy = world.units()[victim].cell_y;
    REQUIRE(world.unit_at_cell(vx, vy) == victim);  // alive: pickable, occupies its cell

    const double dt = 1.0 / 60.0;
    double t = 0.0;
    while (world.units()[victim].die_time < 0.0 && t < 30.0) {
        world.step(dt);
        t += dt;
    }
    REQUIRE(world.units()[victim].die_time >= 0.0);
    REQUIRE(world.units()[victim].alive);  // dying, not yet tombstoned

    // Dying: unpickable and non-occupying, but still rendered (die anim).
    CHECK(world.unit_at_cell(vx, vy) != victim);
    CHECK(world.occupancy().at(vx, vy) == ra2engine::Occupancy::kEmpty);
    {
        const auto order = ra2engine::unit_draw_order(world.units());
        bool found = false;
        for (int idx : order)
            if (idx == victim) found = true;
        CHECK(found);
    }

    // Run the death animation out to tombstone (alive = false).
    while (world.units()[victim].alive && t < 32.0) {
        world.step(dt);
        t += dt;
    }
    REQUIRE_FALSE(world.units()[victim].alive);

    CHECK(world.unit_at_cell(vx, vy) != victim);
    CHECK(world.occupancy().at(vx, vy) == ra2engine::Occupancy::kEmpty);
    {
        const auto order = ra2engine::unit_draw_order(world.units());
        bool found = false;
        for (int idx : order)
            if (idx == victim) found = true;
        CHECK_FALSE(found);  // tombstoned: excluded from draw order
    }
}

TEST_CASE("World: attack order halts the pursuer at weapon range (no melee overshoot)") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: no install"); return; }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // GI (house 0) at the start cell; a hostile Conscript ~6 cells away (well
    // beyond the 4-cell weapon range) at a reachable, unoccupied cell.
    ra2engine::UnitCombat gi{125, "none", "GI", 15, 20, 4.0,
        {100,80,80,50,25,25,75,50,25,100,100}};
    ra2engine::UnitCombat co{125, "flak", "CONS", 15, 25, 4.0,
        {100,80,80,50,25,25,75,50,25,100,100}};
    world.add_combat_unit(c->x, c->y, 0, gi);
    // find_reachable's nearest-first scan (starting at radius 4) can land
    // exactly on the 4-cell weapon range boundary rather than beyond it (on
    // this map, the only radius-4 reachable offsets from the start cell are
    // the cardinal (+-4,0) -- confirmed via debug trace, and every other
    // radius-4 direction is blocked by terrain). Rather than rely on
    // find_reachable's shared radius-4 start, scan directly here starting at
    // radius 6, which guarantees a strictly-out-of-range distance (>4)
    // whether the hit is cardinal (d=6) or diagonal (d~8.49), so the pursue
    // transition this test exercises is actually triggered.
    int ex = 0, ey = 0;
    double d0 = 0.0;
    bool found_far = false;
    for (int r = 6; r <= 40 && !found_far; r += 2) {
        for (const auto& d : kProbeDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            const int tx = c->x + dx, ty = c->y + dy;
            auto p = ra2engine::find_path(world.passable(), world.grid_w(), world.grid_h(),
                                          {c->x, c->y}, {tx, ty});
            if (p.size() > 1) {
                ex = tx; ey = ty;
                d0 = std::sqrt(double(dx * dx + dy * dy));
                found_far = true;
                break;
            }
        }
    }
    REQUIRE(found_far);
    REQUIRE(d0 > 4.0);
    world.add_combat_unit(ex, ey, 1, co);   // hostile, index 1

    // Select the GI and order the attack; the Conscript does NOT move or fire
    // (leave it idle: it will auto-acquire/return-fire, which is fine).
    world.units()[0].selected = true;
    world.order_attack(1);
    REQUIRE(world.units()[0].attack_order);

    const int hp0 = world.units()[1].hp;
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 20.0; t += dt) world.step(dt);

    // The GI closed to within weapon range and STOPPED there (did not march to
    // melee contact) and has fired at least once.
    const ra2engine::Unit& g = world.units()[0];
    const ra2engine::Unit& e = world.units()[1];
    const double df = std::sqrt((e.fx - g.fx) * (e.fx - g.fx) + (e.fy - g.fy) * (e.fy - g.fy));
    CHECK(df <= 4.0 + 1e-6);        // reached effective range
    CHECK(df >= 2.0);               // held at range -- did NOT close to melee (~1 cell)
    CHECK_FALSE(g.moving());        // holding, not still walking
    CHECK(e.hp < hp0);              // fired on the target while holding
}

// ---- Move-cancels-engagement regression (freeze seam: hold-at-range pass
// clearing the path of a moving unit that still carried an auto-acquired or
// retaliation target) ----

namespace {
// Scan a short list of close offsets from (bx, by) for the first walkable,
// unoccupied cell -- used by the two tests below to park a Conscript within
// weapon range (4.0 cells) of a GI's start cell via a direct add_combat_unit
// call (not the squad-scan helpers above, which aren't guaranteed to land
// close). Ordered nearest-ish first; the wide spread of directions/radii
// makes it very unlikely every candidate is blocked on this map (the same
// neighborhood other tests in this file already pack up to 6 units into).
bool find_close_free_cell(const ra2engine::World& w, int bx, int by, int& ox, int& oy) {
    struct Off { int dx, dy; };
    static const Off offs[] = {{3, 0},  {0, 3},  {-3, 0}, {0, -3}, {2, 2},  {2, -2},
                               {-2, 2}, {-2, -2}, {2, 0},  {0, 2},  {-2, 0}, {0, -2},
                               {1, 1},  {1, -1},  {-1, 1}, {-1, -1}, {1, 0}, {0, 1},
                               {-1, 0}, {0, -1}};
    for (const auto& o : offs) {
        const int x = bx + o.dx, y = by + o.dy;
        if (w.walkable(x, y) && w.unit_at_cell(x, y) < 0) { ox = x; oy = y; return true; }
    }
    return false;
}
}  // namespace

TEST_CASE("World: a group-move order cancels engagement and is never frozen by the hold-pass") {
    // Regression for the freeze seam this task fixes: a GI holding an
    // AUTO-ACQUIRED target (idle, in weapon range) that is then given a group
    // move order used to have its path repeatedly cleared by the hold-at-range
    // pass every single frame (the pass only checked "has a live target still
    // in range", not "is this an explicit attack order") -- frozen in place,
    // never reaching the ordered cell. order_move_selected must now cancel
    // the engagement outright, so the hold-at-range pass never touches it.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    world.add_combat_unit(c->x, c->y, 0, gi_stats());  // GI, index 0

    // Conscript (idle, house 1) within weapon range (4.0 cells) of the GI's
    // start cell.
    int conx = -1, cony = -1;
    REQUIRE(find_close_free_cell(world, c->x, c->y, conx, cony));
    const double d_init =
        std::sqrt(double((conx - c->x) * (conx - c->x) + (cony - c->y) * (cony - c->y)));
    REQUIRE(d_init <= 4.0 + 1e-6);  // in range
    world.add_combat_unit(conx, cony, 1, cons_stats());  // Conscript, index 1

    // One step: both units are idle and in range of each other -> the GI
    // auto-acquires the Conscript.
    world.step(1.0 / 60.0);
    REQUIRE(world.units()[0].target == 1);

    // A goal at least 5 cells away (well beyond weapon range). Reuses this
    // file's far-goal probe pattern (kProbeDirs at growing radius, confirmed
    // reachable via find_path) rather than the shared find_reachable helper:
    // the earlier "attack order halts pursuer" test found find_reachable's
    // radius-4 start can land exactly on this map's cardinal offsets from
    // this same start cell (distance == weapon range, not "well beyond" it).
    // Starting the scan at radius 6 guarantees a strictly ->=5-cell goal
    // regardless of which directions this map allows.
    int gx = 0, gy = 0;
    double dgoal = 0.0;
    bool found_far = false;
    for (int r = 6; r <= 40 && !found_far; r += 2) {
        for (const auto& d : kProbeDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            const int tx = c->x + dx, ty = c->y + dy;
            auto p = ra2engine::find_path(world.passable(), world.grid_w(), world.grid_h(),
                                          {c->x, c->y}, {tx, ty});
            if (p.size() > 1) {
                gx = tx;
                gy = ty;
                dgoal = std::sqrt(double(dx * dx + dy * dy));
                found_far = true;
                break;
            }
        }
    }
    REQUIRE(found_far);
    REQUIRE(dgoal >= 5.0);

    world.clear_selection();
    world.units()[0].selected = true;
    world.order_move_selected(gx, gy);

    // The move order must cancel engagement immediately -- load-bearing fix.
    CHECK(world.units()[0].target == -1);
    CHECK_FALSE(world.units()[0].attack_order);

    // Simulate: pre-fix, the hold-at-range pass would clear this path every
    // frame (the target was still in range at the moment of the order) and
    // the GI would never move at all. Post-fix it walks unimpeded to the
    // ordered cell.
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 30.0 && world.units()[0].moving(); t += dt) world.step(dt);

    REQUIRE_FALSE(world.units()[0].moving());
    REQUIRE(world.units()[0].cell_x == gx);
    REQUIRE(world.units()[0].cell_y == gy);
}

TEST_CASE("World: moving units do not auto-acquire or return fire (idle-only engagement)") {
    // Spec wording: "IDLE/guarding units auto-acquire hostiles in range and
    // return fire when hit" -- a unit executing a player move order is
    // neither idle nor guarding. GI ordered to walk away from an in-range
    // idle Conscript: the Conscript (idle) DOES auto-acquire and fires on the
    // GI (whose start cell is already within the Conscript's range), but the
    // GI must never acquire a target or retaliate while moving() is true --
    // it takes damage but keeps walking -- and must still arrive.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    world.add_combat_unit(c->x, c->y, 0, gi_stats());  // GI, index 0

    // Conscript (idle, house 1) within weapon range of the GI's start cell --
    // the GI's own starting position is already in range, so the Conscript
    // can acquire and fire the instant combat runs, regardless of which
    // direction the GI is then ordered to walk.
    int conx = -1, cony = -1;
    REQUIRE(find_close_free_cell(world, c->x, c->y, conx, cony));
    const double d_init =
        std::sqrt(double((conx - c->x) * (conx - c->x) + (cony - c->y) * (cony - c->y)));
    REQUIRE(d_init <= 4.0 + 1e-6);  // in range
    world.add_combat_unit(conx, cony, 1, cons_stats());  // Conscript, index 1

    // A far goal (direction is irrelevant to the engagement window -- the
    // Conscript is already in range of the GI's own start cell before it has
    // moved anywhere).
    int gx = 0, gy = 0;
    bool found_far = false;
    for (int r = 8; r <= 40 && !found_far; r += 2) {
        for (const auto& d : kProbeDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            const int tx = c->x + dx, ty = c->y + dy;
            auto p = ra2engine::find_path(world.passable(), world.grid_w(), world.grid_h(),
                                          {c->x, c->y}, {tx, ty});
            if (p.size() > 1) {
                gx = tx;
                gy = ty;
                found_far = true;
                break;
            }
        }
    }
    REQUIRE(found_far);

    world.units()[0].selected = true;
    world.order_move_selected(gx, gy);
    REQUIRE(world.units()[0].moving());
    REQUIRE(world.units()[0].target == -1);

    const double dt = 1.0 / 60.0;
    double t = 0.0;
    while (world.units()[0].moving() && t < 30.0) {
        CHECK(world.units()[0].target == -1);
        world.step(dt);
        t += dt;
    }

    REQUIRE_FALSE(world.units()[0].moving());  // arrived, not stuck/timed out
    REQUIRE(world.units()[0].cell_x == gx);
    REQUIRE(world.units()[0].cell_y == gy);
    CHECK(world.units()[0].hp < 125);  // it was shot en route
}

// ---- pal_for_house (Task 7: house palette remap) ----

namespace {
// Hand-built unittem.pal stand-in: index 5 is a sentinel OUTSIDE the 16..31
// remap ramp (verifies non-ramp entries are left alone); index 20 is the
// task brief's own worked example -- (60,90,150), brightness
// (60+90+150)/3 == 100 -> house-0 remap (100*2/5, 100*3/5, 100) == (40,60,100)
// -- with a non-255 alpha (200) to verify alpha is preserved, not
// clobbered; indices 16 and 31 are the ramp's inclusive boundaries.
// Uses Palette's mutable operator[] (added this task alongside the existing
// const overload -- pal_for_house needs write access to build a derived
// palette from a copy, which the const-only accessor didn't allow).
ra2assets::Palette make_test_pal() {
    uint8_t raw[768] = {0};
    ra2assets::Palette pal = ra2assets::Palette::load(raw, 768);  // all zero, alpha 255
    pal[5] = ra2assets::Rgba{11, 22, 33, 255};     // sentinel, OUTSIDE the ramp
    pal[16] = ra2assets::Rgba{200, 10, 10, 255};   // ramp lower boundary
    pal[20] = ra2assets::Rgba{60, 90, 150, 200};   // brief's worked example
    pal[31] = ra2assets::Rgba{90, 5, 5, 255};      // ramp upper boundary
    pal[32] = ra2assets::Rgba{7, 8, 9, 255};        // sentinel, just OUTSIDE the ramp
    return pal;
}
}  // namespace

TEST_CASE("pal_for_house: house 1 (Soviet) returns the palette unchanged (identity)") {
    const ra2assets::Palette src = make_test_pal();
    const ra2assets::Palette out = ra2engine::pal_for_house(src, 1);
    for (int i = 0; i < 256; ++i) {
        CHECK(out[i].r == src[i].r);
        CHECK(out[i].g == src[i].g);
        CHECK(out[i].b == src[i].b);
        CHECK(out[i].a == src[i].a);
    }
}

TEST_CASE("pal_for_house: house 0 (Allied) rescales ramp entries onto a blue ramp by original brightness") {
    const ra2assets::Palette src = make_test_pal();
    const ra2assets::Palette out = ra2engine::pal_for_house(src, 0);

    // Brief's worked example: entry 20 = (60,90,150) -> brightness 100 ->
    // (40, 60, 100), alpha (200) preserved verbatim.
    CHECK(out[20].r == 40);
    CHECK(out[20].g == 60);
    CHECK(out[20].b == 100);
    CHECK(out[20].a == 200);

    // Ramp boundary 16 = (200,10,10) -> brightness (200+10+10)/3 == 73 (int
    // division) -> (73*2/5, 73*3/5, 73) == (29, 43, 73).
    CHECK(out[16].r == 29);
    CHECK(out[16].g == 43);
    CHECK(out[16].b == 73);
    CHECK(out[16].a == 255);
}

TEST_CASE("pal_for_house: indices outside 16..31 are untouched by either house") {
    const ra2assets::Palette src = make_test_pal();
    const ra2assets::Palette out0 = ra2engine::pal_for_house(src, 0);
    const ra2assets::Palette out1 = ra2engine::pal_for_house(src, 1);
    for (const ra2assets::Palette* out : {&out0, &out1}) {
        CHECK((*out)[5].r == src[5].r);
        CHECK((*out)[5].g == src[5].g);
        CHECK((*out)[5].b == src[5].b);
        CHECK((*out)[5].a == src[5].a);
        CHECK((*out)[32].r == src[32].r);
        CHECK((*out)[32].g == src[32].g);
        CHECK((*out)[32].b == src[32].b);
    }
}

// ---- Fire/die anim-override pass (Task 7: World::step frame selection) ----

TEST_CASE("World::step anim-override: fire_flash > 0 selects the unit's fire frame range and visits all frames during flash window") {
    // No real map needed -- this exercises Unit::frame selection only, not
    // terrain/pathing, so a default-constructed (cell-less) MapFile is fine
    // (World tolerates an empty map: build_passability(empty) -> 0x0 grid,
    // Occupancy(0,0) no-ops all accesses -- see occupancy.hpp).
    ra2assets::MapFile map;
    ra2engine::World world(map);
    ra2engine::Unit& u = world.add_combat_unit(0, 0, 0, gi_stats());  // image "GI"
    const ra2engine::UnitAnimSeq& seq = ra2engine::anim_for_image("GI");

    // Mid-flash sanity check: frame selection is within range.
    u.fire_flash = 0.3;
    world.step(1.0 / 60.0);
    CHECK(u.frame >= seq.fire_first);
    CHECK(u.frame < seq.fire_first + seq.fire_count);

    // Exhaustiveness check: simulate a full flash window from fire_flash = 0.4
    // (kFireFlashSecs) down to 0, collecting all visited frames.
    ra2engine::World world2(map);
    ra2engine::Unit& u2 = world2.add_combat_unit(0, 0, 0, gi_stats());
    const ra2engine::UnitAnimSeq& seq2 = ra2engine::anim_for_image("GI");

    std::set<int> visited_frames;
    u2.fire_flash = 0.4;  // start of flash window
    const double dt = 1.0 / 60.0;

    while (u2.fire_flash > 0.0) {
        world2.step(dt);
        // u2.fire_flash is decremented by step_combat during step()
        // Record the frame after each step while still in flash window.
        if (u2.fire_flash >= 0.0) {
            visited_frames.insert(u2.frame);
        }
    }

    // After fire_flash reaches 0, verify all fire frames were visited.
    REQUIRE(seq2.fire_count > 0);
    for (int i = 0; i < seq2.fire_count; ++i) {
        const int expected_frame = seq2.fire_first + i;
        CHECK(visited_frames.count(expected_frame) > 0);
    }
}

TEST_CASE("World::step anim-override: die_time selects the die range and clamps at the last frame") {
    ra2assets::MapFile map;
    ra2engine::World world(map);
    ra2engine::Unit& u = world.add_combat_unit(0, 0, 1, cons_stats());  // image "CONS"
    const ra2engine::UnitAnimSeq& seq = ra2engine::anim_for_image("CONS");

    // Early in the death animation: not yet clamped.
    u.die_time = 0.05;
    world.step(1.0 / 60.0);
    REQUIRE(u.alive);  // still dying, not yet tombstoned
    CHECK(u.frame >= seq.die_first);
    CHECK(u.frame < seq.die_first + seq.die_count);

    // Late in the death animation, but still short of combat.cpp's
    // kDieAnimSecs (1.0s) -- world.cpp's die_count is deliberately capped so
    // the WHOLE die range is reachable before the unit tombstones (see the
    // derivation comment next to kGIAnim/kConsAnim) -- clamps at the LAST
    // die frame instead of running off the end of the table.
    u.die_time = 0.9;
    world.step(1.0 / 60.0);
    REQUIRE(u.alive);  // still dying at this die_time (< kDieAnimSecs)
    CHECK(u.frame == seq.die_first + seq.die_count - 1);
}
