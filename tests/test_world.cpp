#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <tuple>
#include <vector>

#include "ai_team.hpp"        // AI-2 offensive-teams: AiTeamState/TeamInstance/opcodes
#include "aimd_parse.hpp"     // AI-2: AimdData/TaskForce/ScriptType/TeamType
#include "building.hpp"
#include "cargo.hpp"          // CB-1: eject_cell -- the eject-cell witness
#include "factory.hpp"        // Production phase: exit_cell/primary_factory/set_primary
#include "harvest.hpp"        // Economy phase: HarvState/HarvestEvent/OreValues/kUnloadSecsPerBale
#include "iso_project.hpp"   // cell_to_screen / screen_to_cell
#include "iso_renderer.hpp"  // pal_for_house
#include "occupancy.hpp"
#include "pathfind.hpp"
#include "production.hpp"     // Production phase: ProductionQueues cancel/reconcile_refund
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"  // Palette/Rgba (pal_for_house tests)
#include "unit_type.hpp"      // Production phase: UnitTypeDef (refund test)
#include "world.hpp"
#include "asset_fixture.hpp"  // shared load-once real map (mapsmd03.mix)

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

// Count of walkable Chebyshev-ring-1 (immediate 8-neighbor) cells around
// (x, y). Task 3: an attack_order target is no longer approached directly --
// World::step now resolves each attacker a DISTINCT free firing cell near the
// target via assign_formation, so (unlike the old direct-pursuit code) the
// terrain immediately around a test's chosen target now matters: a spot
// boxed in by cliffs/water/other terrain leaves assign_formation nothing
// close to hand out, and the pursuer can only reach whatever free cell the
// ring search finds -- possibly far outside weapon range. Tests placing a
// pursuit target below use this to reject overly-enclosed candidates.
int open_neighbor_count(const ra2engine::World& w, int x, int y) {
    static const int nb[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                                 {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    int n = 0;
    for (const auto& o : nb)
        if (w.walkable(x + o[0], y + o[1])) ++n;
    return n;
}

// Like find_reachable's kProbeDirs ring scan, but ALSO requires at least
// `min_open` free (open_neighbor_count) immediate neighbors around the
// candidate before accepting it -- see open_neighbor_count above. Returns the
// found cell's (ox, oy) and its Euclidean distance from (x0, y0) in odist.
bool find_reachable_open(const ra2engine::World& w, int x0, int y0, int start_r, int min_open,
                         int& ox, int& oy, double& odist) {
    for (int r = start_r; r <= 60; r += 2)
        for (const auto& d : kProbeDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            const int tx = x0 + dx, ty = y0 + dy;
            if (open_neighbor_count(w, tx, ty) < min_open) continue;
            auto p = ra2engine::find_path(w.passable(), w.grid_w(), w.grid_h(), {x0, y0}, {tx, ty});
            if (p.size() > 1) {
                ox = tx;
                oy = ty;
                odist = std::sqrt(double(dx * dx + dy * dy));
                return true;
            }
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
bool load_real_map_w(ra2assets::MapFile& out) {
    // Borrow the shared, load-once mapsmd03.mix map (asset_fixture) instead of
    // re-slurping + re-parsing the same archive in every integration case.
    // Same ok/false contract as before (archive present AND map entry found);
    // the parsed map is copied into `out` so callers keep their own instance.
    if (!ra2fixture::mapsmd03_available()) return false;
    out = ra2fixture::mapsmd03_map();
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
    // transition this test exercises is actually triggered. ALSO requires
    // open ground around the candidate (open_neighbor_count) -- Task 3's
    // approach logic resolves a firing position NEAR the target via
    // assign_formation rather than pathing directly to it, so a target
    // boxed in by terrain would leave the GI no in-range cell to walk to at
    // all (confirmed via debug trace: a boxed-in candidate this scan used to
    // accept left the GI stuck 4.12 cells out -- just outside range).
    int ex = 0, ey = 0;
    double d0 = 0.0;
    bool found_far = find_reachable_open(world, c->x, c->y, 6, 4, ex, ey, d0);
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

TEST_CASE("combat: five attackers fan out and ALL fire (no single-file queue)") {
    // Regression for the reported bug: a group attack order used to send
    // EVERY attacker to the target's own occupied cell (combat.cpp's old §f
    // attack_order branch emitted {i, v.cell_x, v.cell_y} for every unit), so
    // they queued single-file behind whoever got in range first and never
    // fanned out to distinct firing positions -- only ONE attacker ever fired
    // (the others sat blocked, out of weapon range, forever). docs/re/
    // infantry-anim.md §7: the original approaches until in weapon range and
    // never paths to the victim's own cell; World::step now batch-assigns
    // each attacker its own free firing cell via assign_formation.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Tough, disarmed victim (house 1) far enough away (> the 4-cell weapon
    // range) that every attacker must actually approach it. Huge hp so the
    // victim survives the whole 12s window; damage=0 so no attacker dies
    // mid-test (a death would vacate a cell and could mask the single-file
    // symptom by letting the next queued attacker shuffle forward).
    ra2engine::UnitCombat victim_stats = cons_stats();
    victim_stats.strength = 100000;
    victim_stats.damage = 0;

    // Needs at least 5 open (walkable) immediate neighbors around the victim
    // cell -- see open_neighbor_count -- so assign_formation actually has 5
    // distinct, in-range firing positions to hand out (a candidate near a
    // map edge/cliff/water could otherwise leave it no choice but to hand out
    // cells beyond weapon range, which isn't the bug this test targets).
    int vx = 0, vy = 0;
    double dv = 0.0;
    bool found_far = find_reachable_open(world, c->x, c->y, 8, 5, vx, vy, dv);
    REQUIRE(found_far);
    REQUIRE(dv > 4.0);
    world.add_combat_unit(vx, vy, 1, victim_stats);
    const int victim_idx = static_cast<int>(world.units().size()) - 1;

    // Five house-0 attackers CLUSTERED tightly near the start cell -- the
    // "single-file-prone" formation from the user's bug report (spawn_combat_
    // squad packs them into adjacent free cells scanning outward from base).
    std::vector<int> attackers = spawn_combat_squad(world, c->x, c->y, 0, gi_stats(), 5);
    REQUIRE(attackers.size() == 5);

    world.clear_selection();
    for (int a : attackers) world.units()[a].selected = true;
    world.order_attack(victim_idx);
    for (int a : attackers) REQUIRE(world.units()[a].attack_order);

    bool has_fired[5] = {false, false, false, false, false};
    const double dt = 1.0 / 60.0;
    for (double t = 0; t < 12.0; t += dt) {
        world.step(dt);
        for (std::size_t k = 0; k < attackers.size(); ++k)
            if (world.units()[attackers[k]].fire_flash > 0.0) has_fired[k] = true;
    }

    for (std::size_t k = 0; k < attackers.size(); ++k) CHECK(has_fired[k]);

    // Every attacker ended within weapon range of the victim.
    const ra2engine::Unit& v = world.units()[victim_idx];
    for (int a : attackers) {
        const ra2engine::Unit& u = world.units()[a];
        const double dx = v.fx - u.fx, dy = v.fy - u.fy;
        CHECK(std::sqrt(dx * dx + dy * dy) <= u.w_range + 1e-6);
    }
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

// ---- Fire/die anim-override pass (Task 7 introduced; Task 2 recalibrated
// the frame values + added facing -- docs/re/infantry-anim.md) ----

TEST_CASE("World::step anim-override: fire_flash > 0 selects the unit's fire frame range and visits all frames during flash window") {
    // No real map needed -- this exercises Unit::frame selection only, not
    // terrain/pathing, so a default-constructed (cell-less) MapFile is fine
    // (World tolerates an empty map: build_passability(empty) -> 0x0 grid,
    // Occupancy(0,0) no-ops all accesses -- see occupancy.hpp).
    ra2assets::MapFile map;
    ra2engine::World world(map);
    ra2engine::Unit& u = world.add_combat_unit(0, 0, 0, gi_stats());  // image "GI"
    const ra2engine::UnitAnimSeq& seq = ra2engine::default_anim();

    // Mid-flash sanity check: frame selection is within range, at whatever
    // facing the unit currently has (no target set -> stays the spawn
    // default, but the assertion is written facing-relative on purpose).
    u.fire_flash = 0.3;
    world.step(1.0 / 60.0);
    const int base = seq.fire.first + u.facing * seq.fire.fmult;
    CHECK(u.frame >= base);
    CHECK(u.frame < base + seq.fire.count);

    // Exhaustiveness check: simulate a full flash window from fire_flash = 0.4
    // (kFireFlashSecs) down to 0, collecting all visited frames. Facing stays
    // fixed throughout (no target -> the aim-facing pass never touches it),
    // proving all 6 fire frames are reachable at that one fixed facing.
    ra2engine::World world2(map);
    ra2engine::Unit& u2 = world2.add_combat_unit(0, 0, 0, gi_stats());
    const ra2engine::UnitAnimSeq& seq2 = ra2engine::default_anim();

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
    REQUIRE(seq2.fire.count > 0);
    for (int i = 0; i < seq2.fire.count; ++i) {
        const int expected_frame = ra2engine::seq_frame(seq2.fire, u2.facing, i);
        CHECK(visited_frames.count(expected_frame) > 0);
    }
}

TEST_CASE("World::step anim-override: die_time selects the die range and reaches the last frame before tombstone") {
    ra2assets::MapFile map;
    ra2engine::World world(map);
    ra2engine::Unit& u = world.add_combat_unit(0, 0, 1, cons_stats());  // image "CONS"
    const ra2engine::UnitAnimSeq& seq = ra2engine::default_anim();

    // Early in the death animation: not yet at the end.
    u.die_time = 0.05;
    world.step(1.0 / 60.0);
    REQUIRE(u.alive);  // still dying, not yet tombstoned
    CHECK(u.frame >= seq.die.first);
    CHECK(u.frame < seq.die.first + seq.die.count);

    // Late in the death animation, but still short of combat.cpp's
    // kDieAnimSecs (1.0s) -- default_anim's die entry (134,15,0) is EXACTLY
    // 15 frames at kDieAnimFps (15Hz, world.cpp) == kDieAnimSecs (1.0s), so
    // the WHOLE die range (134..148) is reachable before the unit tombstones
    // (docs/re/infantry-anim.md §2/§3): this reaches the LAST die frame (148)
    // while still alive, proving frame 148 is actually displayed (the Task 7
    // bug this replaces never reached its analogous last frame at all).
    u.die_time = 0.95;
    world.step(1.0 / 60.0);
    REQUIRE(u.alive);  // still dying at this die_time (< kDieAnimSecs)
    CHECK(u.frame == seq.die.first + seq.die.count - 1);
    CHECK(u.frame == 148);
}

// ---- Task 2: facing + artmd-calibrated animation table (docs/re/infantry-anim.md) ----

TEST_CASE("facing: 8-way cell-direction table (docs/re/infantry-anim.md §5)") {
    using ra2engine::facing_from_delta;
    CHECK(facing_from_delta(0, -1) == 7);   // screen NE
    CHECK(facing_from_delta(1, -1) == 6);   // E
    CHECK(facing_from_delta(1, 0) == 5);    // SE
    CHECK(facing_from_delta(1, 1) == 4);    // S
    CHECK(facing_from_delta(0, 1) == 3);    // SW
    CHECK(facing_from_delta(-1, 1) == 2);   // W
    CHECK(facing_from_delta(-1, 0) == 1);   // NW
    CHECK(facing_from_delta(-1, -1) == 0);  // N
    CHECK(facing_from_delta(0, 0) == -1);   // no direction -> keep current
    CHECK(facing_from_delta(5.0, 5.0) == 4);      // continuous, same ray as (1,1)
    CHECK(facing_from_delta(0.1, -0.1) == 6);     // continuous E ray
}

TEST_CASE("facing: seq_frame formula and real default table") {
    using namespace ra2engine;
    const UnitAnimSeq& a = default_anim();
    CHECK(a.stand.first == 0);   CHECK(a.stand.count == 1);  CHECK(a.stand.fmult == 1);
    CHECK(a.walk.first == 8);    CHECK(a.walk.count == 6);   CHECK(a.walk.fmult == 6);
    CHECK(a.fire.first == 164);  CHECK(a.fire.count == 6);   CHECK(a.fire.fmult == 6);
    CHECK(a.die.first == 134);   CHECK(a.die.count == 15);   CHECK(a.die.fmult == 0);
    CHECK(seq_frame(a.stand, 3, 0) == 3);          // stand facing SW -> frame 3
    CHECK(seq_frame(a.fire, 6, 2) == 164 + 36 + 2); // fire E phase 2 -> 202
    CHECK(seq_frame(a.die, 6, 4) == 138);           // die ignores facing (fmult 0)
    CHECK(seq_frame(a.fire, 0, 99) == 169);         // phase clamps to count-1
    AnimTable t;
    UnitAnimSeq custom = a; custom.fire.first = 500;
    t["ZZZ"] = custom;
    CHECK(anim_for_image(t, "ZZZ").fire.first == 500);
    CHECK(anim_for_image(t, "GI").fire.first == 164);  // miss -> default
}

TEST_CASE("Unit: spawns facing S (facing == 4, docs/re/infantry-anim.md §6)") {
    ra2engine::Unit u;
    CHECK(u.facing == 4);
}

TEST_CASE("World::step: walking unit faces its segment (docs/re/infantry-anim.md §5)") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(3.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Find a walkable start cell whose east neighbor is ALSO walkable and
    // unoccupied (pick_start_cell's centroid cell alone isn't guaranteed to
    // have a walkable east neighbor at a map edge/obstacle).
    int sx = -1, sy = -1;
    for (int dy = 0; dy < 8 && sx < 0; ++dy)
        for (int dx = 0; dx < 8 && sx < 0; ++dx) {
            const int x = c->x + dx, y = c->y + dy;
            if (world.walkable(x, y) && world.walkable(x + 1, y) &&
                world.unit_at_cell(x, y) < 0 && world.unit_at_cell(x + 1, y) < 0) {
                sx = x;
                sy = y;
            }
        }
    REQUIRE(sx >= 0);

    world.add_unit(sx, sy, "GI", 0);
    REQUIRE(world.units()[0].facing == 4);  // spawn facing S
    world.units()[0].selected = true;
    world.order_move_selected(sx + 1, sy);  // one cell east
    REQUIRE(world.units()[0].moving());

    world.step(1.0 / 60.0);  // small dt at 3 cells/sec -> still mid-segment
    CHECK(world.units()[0].facing == 5);  // faces the segment: delta (1,0) -> SE
    CHECK(world.units()[0].frame >= 8 + 5 * 6);
    CHECK(world.units()[0].frame < 8 + 5 * 6 + 6);
}

TEST_CASE("World::step anim-override: firing unit faces its target (docs/re/infantry-anim.md §5)") {
    // No real map needed (same minimal fixture as the fire/die override tests
    // above): two stationary combat units, attacker due WEST of victim so
    // the attacker's delta-to-victim is (1,0) -> facing 5 (SE).
    ra2assets::MapFile map;
    ra2engine::World world(map);
    world.add_combat_unit(0, 0, 0, gi_stats());    // index 0: attacker (GI, house 0)
    world.add_combat_unit(1, 0, 1, cons_stats());  // index 1: victim (CONS, house 1), due east

    // Idle proximity: step_combat auto-acquires the nearest hostile in range
    // and fires immediately (rof_cool starts at 0) -- no explicit order needed.
    world.step(1.0 / 60.0);

    const ra2engine::Unit& attacker = world.units()[0];
    REQUIRE(attacker.target == 1);
    CHECK(attacker.facing == 5);  // faces the victim even between/during shots
    REQUIRE(attacker.fire_flash > 0.0);
    const ra2engine::UnitAnimSeq& seq = ra2engine::default_anim();
    const int base = seq.fire.first + attacker.facing * seq.fire.fmult;
    CHECK(attacker.frame >= base);
    CHECK(attacker.frame < base + seq.fire.count);
}

// ---- Buildings & Construction Task 4: World integration (blocking, attack) ----
// Real-map fixture (same load_real_map_w/pick_start_cell precedent as every
// other World integration test in this file) -- MapFile has no public
// cell-injecting constructor, so an "in-memory" fixture would mean hand-
// synthesizing IsoMapPack5 LZO1X bytes, which test_lzo1x.cpp documents as
// impractical; the real-map-or-skip pattern is this file's established way
// of getting real walkable terrain into a World.

TEST_CASE("world: add_building blocks passability, death restores it") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World w(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // A 2x2 walkable block with a 5th walkable cell 2 cells east of its
    // origin (outside the foundation, to prove only the foundation itself
    // gets blocked).
    int bx = -1, by = -1;
    for (int dy = 0; dy < 10 && bx < 0; ++dy)
        for (int dx = 0; dx < 10 && bx < 0; ++dx) {
            const int x = c->x + dx, y = c->y + dy;
            if (w.walkable(x, y) && w.walkable(x + 1, y) && w.walkable(x, y + 1) &&
                w.walkable(x + 1, y + 1) && w.walkable(x + 2, y)) {
                bx = x;
                by = y;
            }
        }
    REQUIRE(bx >= 0);
    REQUIRE(w.walkable(bx, by));

    const int bi = w.add_building(/*type*/ 0, bx, by, /*fw*/ 2, /*fh*/ 2, /*house*/ 1,
                                  /*strength*/ 100, /*armor_ix*/ 0, /*buildup_total*/ 0.0,
                                  /*instant*/ true);
    CHECK_FALSE(w.walkable(bx, by));
    CHECK_FALSE(w.walkable(bx + 1, by + 1));
    CHECK(w.walkable(bx + 2, by));
    CHECK(w.building_at_cell(bx + 1, by) == bi);
    CHECK(w.building_at_cell(bx + 2, by) == -1);

    ra2engine::kill_building(w.buildings()[bi]);
    w.step(0.001);  // death detected -> passability restored
    CHECK(w.walkable(bx, by));
    CHECK(w.building_at_cell(bx, by) == -1);

    w.step(ra2engine::kBuildingDieSecs + 0.1);
    CHECK_FALSE(w.buildings()[bi].alive);  // tombstoned, never erased
}

TEST_CASE("world: order_attack_building sets exclusive target") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World w(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    w.add_combat_unit(c->x, c->y, 0, gi_stats());  // house-0 combat unit, index 0
    w.units()[0].selected = true;

    // Friendly (house 0) building: ordering an attack on it must be a no-op.
    int fx = -1, fy = -1;
    REQUIRE(find_close_free_cell(w, c->x, c->y, fx, fy));
    const int bi_friendly = w.add_building(0, fx, fy, 1, 1, /*house*/ 0, 100, 0, 0.0, true);
    w.order_attack_building(bi_friendly);
    CHECK(w.units()[0].target_building == -1);

    // Hostile (house 1) building elsewhere.
    int ex = -1, ey = -1;
    REQUIRE(find_close_free_cell(w, c->x + 3, c->y + 3, ex, ey));
    const int bi_enemy = w.add_building(0, ex, ey, 1, 1, /*house*/ 1, 100, 0, 0.0, true);
    w.order_attack_building(bi_enemy);
    CHECK(w.units()[0].target_building == bi_enemy);
    CHECK(w.units()[0].target == -1);
    CHECK(w.units()[0].attack_order);

    // A subsequent order_attack(<enemy unit>) clears target_building (the two
    // target kinds are mutually exclusive).
    int hx = -1, hy = -1;
    REQUIRE(find_close_free_cell(w, c->x - 3, c->y - 3, hx, hy));
    w.add_combat_unit(hx, hy, 1, cons_stats());  // hostile unit, index 1
    w.order_attack(1);
    CHECK(w.units()[0].target_building == -1);
    CHECK(w.units()[0].target == 1);
    CHECK(w.units()[0].attack_order);
}

// ---- Economy phase Task 6: World integration (ore ownership, harvest step,
// income, orders) -- same real-map-or-skip fixture as every other World
// integration test in this file (see the comment above "world: add_building
// blocks passability..." for why: MapFile has no cell-injecting constructor).

TEST_CASE("World: add_harvester sets harvester fields + occupancy") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    ra2engine::Unit& u = world.add_harvester(c->x, c->y, 0, gi_stats(), /*chrono*/ false,
                                             /*storage*/ 40);
    CHECK(world.units().size() == 1);
    CHECK(u.is_harvester);
    CHECK_FALSE(u.chrono_miner);
    CHECK(u.storage_bales == 40);
    CHECK(u.harv == ra2engine::HarvState::SeekOre);   // starts hunting immediately
    CHECK(world.unit_at_cell(c->x, c->y) == 0);
    CHECK(world.occupancy().at(c->x, c->y) == 0);

    // CMIN variant: chrono flag + a different storage figure both carry through.
    int cx = -1, cy = -1;
    REQUIRE(find_close_free_cell(world, c->x, c->y, cx, cy));
    ra2engine::Unit& cmin = world.add_harvester(cx, cy, 1, cons_stats(), /*chrono*/ true,
                                                /*storage*/ 20);
    CHECK(cmin.chrono_miner);
    CHECK(cmin.storage_bales == 20);
    CHECK(cmin.house == 1);
}

TEST_CASE("World: order_harvest paths a selected harvester to ore and it gulps on arrival") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    int ox = -1, oy = -1;
    REQUIRE(find_close_free_cell(world, c->x, c->y, ox, oy));
    world.ore().set(ox, oy, ra2engine::OreKind::Ore, ra2engine::kMaxOreDensity, 0);

    ra2engine::Unit& u =
        world.add_harvester(c->x, c->y, 0, gi_stats(), /*chrono*/ false, /*storage*/ 40);
    u.selected = true;
    world.order_harvest(ox, oy);
    CHECK(u.harv == ra2engine::HarvState::MoveToOre);
    CHECK(u.harv_cell_x == ox);
    CHECK(u.harv_cell_y == oy);

    // step_harvest only runs once economy is configured -- growth/spread
    // parked at a huge interval so this short run never fires them (keeps
    // the test focused purely on the seek->arrive->gulp path).
    std::vector<ra2engine::BuildingTypeDef> types(1);
    types[0].id = "NAREFN";
    world.set_economy(types, {0}, ra2engine::OreValues{}, /*growth_interval*/ 1000.0,
                      /*growth_pct*/ 0.0, /*spread_pct*/ 0.0);

    bool gulped = false;
    for (int i = 0; i < 400 && !gulped; ++i) {
        world.step(0.1);
        if (u.bales_ore > 0.0 || world.ore().density_at(ox, oy) < ra2engine::kMaxOreDensity)
            gulped = true;
    }
    CHECK(gulped);
}

TEST_CASE("World: take_income drains harvest income once, then reads zero") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Find a 4x3 refinery-shaped footprint (NAREFN) with its dock cell
    // (origin + (fw/2, fh), per dock_cell_x/y) also walkable.
    int bx = -1, by = -1;
    for (int dy = 0; dy < 16 && bx < 0; ++dy)
        for (int dx = 0; dx < 16 && bx < 0; ++dx) {
            const int x = c->x + dx, y = c->y + dy;
            bool ok = true;
            for (int fy = 0; fy < 3 && ok; ++fy)
                for (int fx = 0; fx < 4 && ok; ++fx)
                    if (!world.walkable(x + fx, y + fy)) ok = false;
            if (ok && world.walkable(x + 2, y + 3)) { bx = x; by = y; }
        }
    REQUIRE(bx >= 0);

    const int ref_idx = world.add_building(/*type*/ 0, bx, by, /*fw*/ 4, /*fh*/ 3, /*house*/ 0,
                                           /*strength*/ 1000, /*armor_ix*/ 0,
                                           /*buildup_total*/ 0.0, /*instant*/ true);
    const int dock_x = bx + 2, dock_y = by + 3;   // dock_cell_x/y, harvest.hpp

    ra2engine::Unit& u = world.add_harvester(dock_x, dock_y, 0, gi_stats(), /*chrono*/ false,
                                             /*storage*/ 40);
    // Drive the mini-cycle directly from Unloading (drives the same
    // per-bale drain step_harvest's own unload test exercises) rather than a
    // full seek/gulp/return walk -- this test is about take_income's
    // drain-once-then-zero contract, not the whole FSM.
    u.harv = ra2engine::HarvState::Unloading;
    u.dock_refinery = ref_idx;
    u.bales_ore = 2;    // 2 * OreValues::ore (25) = 50 credits

    std::vector<ra2engine::BuildingTypeDef> types(1);
    types[0].id = "NAREFN";
    world.set_economy(types, {0}, ra2engine::OreValues{}, /*growth_interval*/ 1000.0,
                      /*growth_pct*/ 0.0, /*spread_pct*/ 0.0);

    for (int i = 0; i < 50 && (u.bales_ore > 0.0 || u.bales_gems > 0.0); ++i)
        world.step(ra2engine::kUnloadSecsPerBale);
    CHECK(u.bales_ore == 0.0);

    const double income = world.take_income(0);
    CHECK(income == doctest::Approx(50.0));
    CHECK(world.take_income(0) == doctest::Approx(0.0));   // drained once -> zero after
    CHECK(world.take_income(1) == doctest::Approx(0.0));   // house 1 untouched
}

TEST_CASE("World: step() without set_economy runs zero ore/harvest activity "
         "(byte-identity gate)") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Even with an ore cell manually placed and a harvester mid-mission,
    // set_economy was never called -> types_ is empty -> step() must not
    // touch ore growth/spread OR the harvester's mission state at all.
    world.ore().set(c->x, c->y, ra2engine::OreKind::Ore, ra2engine::kMaxOreDensity, 0);
    ra2engine::Unit& u =
        world.add_harvester(c->x, c->y, 0, gi_stats(), /*chrono*/ false, /*storage*/ 40);
    u.harv = ra2engine::HarvState::Harvesting;
    u.harv_timer = 0.0;

    for (int i = 0; i < 50; ++i) world.step(1.0);

    CHECK(world.ore().timer() == 0.0);
    CHECK(world.ore().density_at(c->x, c->y) == ra2engine::kMaxOreDensity);   // never gulped
    CHECK(u.bales_ore == 0.0);
    CHECK(u.harv == ra2engine::HarvState::Harvesting);   // FSM frozen, never advanced
    CHECK(world.take_income(0) == doctest::Approx(0.0));
}

// --- Task 5: World factory launch / rally / primary integration ---

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
    auto free_cell = [&](int x, int y) {
        return world.walkable(x, y) && world.unit_at_cell(x, y) < 0;
    };
    const ra2engine::Cellxy ex = ra2engine::exit_cell(world.buildings()[fi], fdef, free_cell);
    REQUIRE(ex.x >= 0);                               // ring found a free door cell
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

TEST_CASE("World: launch_unit ring-falls-back past an occupied door cell, HOLDs when boxed in") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    const int fi = world.add_building(0, c->x, c->y, 1, 1, 0, 100, 0, 0.0, true);
    ra2engine::BuildingTypeDef fdef; fdef.id = "GAWEAP"; fdef.fw = 1; fdef.fh = 1;
    auto free_cell = [&](int x, int y) {
        return world.walkable(x, y) && world.unit_at_cell(x, y) < 0;
    };
    const ra2engine::Cellxy ex = ra2engine::exit_cell(world.buildings()[fi], fdef, free_cell);
    REQUIRE(ex.x >= 0);
    // Occupy the door cell: the 8-ring walk falls back to the NEXT free cell
    // [RE-GATE APPLIED: production.md §1.3 — first-free-in-ring, not a fixed cell]:
    ra2engine::UnitCombat blocker; blocker.image = "GI"; blocker.strength = 100; blocker.armor = "none";
    world.add_combat_unit(ex.x, ex.y, 0, blocker);
    const int ui = world.launch_unit(fi, fdef, blocker);
    REQUIRE(ui >= 0);                                 // spawned on a ring cell...
    CHECK((world.units()[ui].cell_x != ex.x || world.units()[ui].cell_y != ex.y));  // ...not the door
    // Now box the factory in: occupy EVERY remaining free ring cell.
    while (true) {
        const ra2engine::Cellxy g = ra2engine::exit_cell(world.buildings()[fi], fdef, free_cell);
        if (g.x < 0) break;
        world.add_combat_unit(g.x, g.y, 0, blocker);
    }
    const int before = static_cast<int>(world.units().size());
    CHECK(world.launch_unit(fi, fdef, blocker) == -1);        // fully blocked -> HOLD
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
    // Default destination = the exit-adjacent gather cell — the nearest free
    // cell from the building's own coordinate [RE-GATE APPLIED: production.md
    // §1.4, FUN_00500020] — NOT the map center. The unit spawns at the first
    // free ring cell (south door for a 1x1) and best-effort moves one ring
    // cell over to clear the door; the key contract is the spawn cell.
    CHECK(world.units()[ui].cell_x == world.buildings()[fi].cx + fdef.fw / 2);
    CHECK(world.units()[ui].cell_y == world.buildings()[fi].cy + fdef.fh);
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

// Carry-forward from the Task 3 review: the ProductionQueues cancel +
// reconcile_refund path (the sink for THIS task's factory-category-lost
// cancel) had NO test coverage. Drive a real cancel and assert the exact
// credit total lands back in the shared pool via reconcile_refund. This is
// the refund path a caller invokes when factory_category_lost() returns true.
TEST_CASE("World/production: factory-lost cancel refunds spend into the shared pool exactly") {
    std::vector<ra2engine::UnitTypeDef> u(1);
    u[0].id = "MTNK"; u[0].cost = 2000; u[0].tech_level = 2; u[0].is_vehicle = true;

    ra2engine::ProductionQueues q;
    q.set_credits(5000.0);
    // Build a MTNK in the Vehicle queue; tick a while so real credits are spent.
    q.start_unit(ra2engine::QueueCat::Vehicle, 0, u, /*build_speed=*/0.7);
    std::vector<ra2engine::BuildingTypeDef> btypes;  // unused by unit tick
    const double speedups[4] = {1.0, 1.0, 1.0, 1.0};
    for (int i = 0; i < 30; ++i) q.tick(1.0, 1.0, speedups, btypes);
    // Credits were drawn down but the build is not yet ready/placed.
    CHECK(q.credits() < 5000.0);
    CHECK_FALSE(q.ready(ra2engine::QueueCat::Vehicle));

    // factory_category_lost -> cancel that queue + reconcile the shared pool.
    q.cancel(ra2engine::QueueCat::Vehicle);
    q.reconcile_refund();
    // Cancel refunds ALL spend, so the pool returns to exactly the start value.
    CHECK(q.credits() == doctest::Approx(5000.0));
    CHECK(q.producing(ra2engine::QueueCat::Vehicle) == -1);  // slot cleared
}

// --- AI-1 base-planning phase [RE: ai-base.md] ------------------------------

TEST_CASE("World: step_ai_houses firewall - no AI house means a byte-identical step") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 100, 0, 0.0, true);
    CHECK_FALSE(world.ai_enabled(1));
    CHECK(world.ai_state(1) == nullptr);
    const std::size_t nb = world.buildings().size();
    const std::size_t nu = world.units().size();
    for (int i = 0; i < 60; ++i) world.step(1.0 / 15.0);
    // No AI flagged -> the planner never runs: nothing new appears.
    CHECK(world.buildings().size() == nb);
    CHECK(world.units().size() == nu);
}

TEST_CASE("World: an AI house builds out through the EXISTING queue+placement machinery") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // Hand roster: [0]=conyard 1x1, [1]=power 1x1 (test values; ids Soviet-shaped).
    ra2engine::AiHouseConfig cfg;
    cfg.roster.resize(2);
    cfg.roster[0].id = "NACNST"; cfg.roster[0].conyard = true;
    cfg.roster[0].cost = 3000; cfg.roster[0].tech_level = 1;
    cfg.roster[0].strength = 1000; cfg.roster[0].adjacent = 2;
    cfg.roster[1].id = "NAPOWR"; cfg.roster[1].cost = 600;
    cfg.roster[1].tech_level = 1; cfg.roster[1].strength = 750;
    cfg.roster[1].power = 150; cfg.roster[1].adjacent = 2;
    cfg.buildup_totals = {0.0, 0.0};
    cfg.catalog.power = 1;  // only the power plant is in the build set ->
                            // build_base_plan yields exactly ONE node
                            // (catalog gaps skipped) [RE: ai-base.md §2]
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.difficulty = ra2engine::AiDifficulty::Normal;
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Normal, g);
    cfg.build_speed = 0.1;      // fast builds for the test (54 frames for 600c)
    cfg.start_credits = 10000;
    // Seed the ConYard, then flag the house.
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.enable_ai_house(1, cfg);
    CHECK(world.ai_enabled(1));
    REQUIRE(world.ai_state(1) != nullptr);
    CHECK(world.ai_credits(1) == doctest::Approx(10000.0));
    // Step: the plan walk reaches its Power node -> the AI starts + places
    // NAPOWR via the EXISTING ProductionQueues + placement_check +
    // add_building path (the ~7 s tick + BuildDelay cadence paces the
    // attempts [RE: ai-base.md §1] — 3000 frames covers dozens of passes).
    const std::size_t nb = world.buildings().size();
    for (int i = 0; i < 3000 && world.buildings().size() < nb + 1; ++i)
        world.step(1.0 / 15.0);
    REQUIRE(world.buildings().size() >= nb + 1);
    const ra2engine::Building& nb1 = world.buildings()[nb];
    CHECK(nb1.type == 1);
    CHECK(nb1.house == 1);
    // Placement is nearest-first from the ConYard. NOTE the AI BYPASSES the
    // Adjacent= proximity check [RE-GATE APPLIED: ai-base.md §3]; locality
    // comes from the Chebyshev ring scan + the AIBaseSpacing(1) gap.
    CHECK(std::max(std::abs(nb1.cx - c->x), std::abs(nb1.cy - c->y)) <= 4);
    // The planned-base list recorded the node [RE: ai-base.md §3].
    REQUIRE(world.ai_state(1)->plan.size() >= 1);
    CHECK(world.ai_state(1)->plan[0].type == 1);
    CHECK(world.ai_state(1)->plan[0].building == (int)nb);
    // Credits were charged through the queue economy (600 x cost_mul).
    CHECK(world.ai_credits(1) <
          10000.0 - 0.5 * 600.0 * ra2engine::kDifficultyCost[1]);
    // Plan satisfied + no refinery/war factory in this catalog -> economy
    // expansion has nothing to add [RE: ai-base.md §2 — expansion appends
    // refineries/war factories only]: no further growth.
    const std::size_t done = world.buildings().size();
    for (int i = 0; i < 600; ++i) world.step(1.0 / 15.0);
    CHECK(world.buildings().size() == done);
}

TEST_CASE("World: AI rebuild re-queues the destroyed planned node at its cell") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    ra2engine::AiHouseConfig cfg;  // same tiny 2-type cfg as the case above
    cfg.roster.resize(2);
    cfg.roster[0].id = "NACNST"; cfg.roster[0].conyard = true;
    cfg.roster[0].cost = 3000; cfg.roster[0].tech_level = 1;
    cfg.roster[0].strength = 1000; cfg.roster[0].adjacent = 2;
    cfg.roster[1].id = "NAPOWR"; cfg.roster[1].cost = 600;
    cfg.roster[1].tech_level = 1; cfg.roster[1].strength = 750;
    cfg.roster[1].power = 150; cfg.roster[1].adjacent = 2;
    cfg.buildup_totals = {0.0, 0.0};
    cfg.catalog.power = 1;  // one-entry catalog -> the plan has exactly ONE
                            // node, so the rebuild target is unambiguous
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.difficulty = ra2engine::AiDifficulty::Normal;
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Normal, g);
    cfg.build_speed = 0.1;
    cfg.start_credits = 10000;
    world.add_building(0, c->x, c->y, 1, 1, 1, 1000, 0, 0.0, true);
    world.enable_ai_house(1, cfg);
    const std::size_t nb = world.buildings().size();
    for (int i = 0; i < 3000 && world.buildings().size() < nb + 1; ++i)
        world.step(1.0 / 15.0);
    REQUIRE(world.buildings().size() == nb + 1);
    const int node_cx = world.ai_state(1)->plan[0].cx;
    const int node_cy = world.ai_state(1)->plan[0].cy;
    // Kill the placed power plant; the planner re-queues THAT node.
    ra2engine::kill_building(world.buildings()[nb]);
    for (int i = 0; i < 4000 && !(world.ai_state(1)->plan[0].building > (int)nb); ++i)
        world.step(1.0 / 15.0);
    REQUIRE(world.ai_state(1)->plan.size() == 1);       // NOT a new expansion node
    const ra2engine::PlannedNode& n = world.ai_state(1)->plan[0];
    CHECK(n.type == 1);
    CHECK_FALSE(n.dead);
    REQUIRE(n.building > (int)nb);                      // a NEW building index
    CHECK(world.buildings()[n.building].type == 1);
    CHECK(world.buildings()[n.building].alive);
    // Rebuilt at the exact stored cell [RE-GATE APPLIED: ai-base.md §5]
    // (the dead building's rubble releases its cells, so the cell is free):
    CHECK(world.buildings()[n.building].cx == node_cx);
    CHECK(world.buildings()[n.building].cy == node_cy);
}

TEST_CASE("World: AI can_place rejects a partially-off-grid or partially-occupied foundation") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // Contract #1: the AI's placement validity (placement_check with
    // bypass_adjacency=true, over the FULL foundation) must reject a location
    // unless the ENTIRE foundation is on-grid, buildable, and unoccupied
    // [RE-GATE APPLIED: ai-base.md §3]. A 2x2 foundation is used so a single
    // off-grid or occupied cell makes the whole placement fail.
    const int fw = 2, fh = 2;
    // A fully-on-grid, empty interior cell passes (baseline sanity).
    CHECK(ra2engine::placement_check(c->x, c->y, fw, fh, /*house=*/1, /*adjacent=*/2,
                                     world.passable(), world.grid_w(), world.grid_h(),
                                     world.occupancy(), world.buildings(),
                                     /*bypass_adjacency=*/true).ok);
    // Partial-off-grid: origin at the far bottom-right corner so the +1 cells
    // of the 2x2 foundation fall off the grid -> at least one cell off-grid ->
    // the whole placement fails (NOT ok).
    CHECK_FALSE(ra2engine::placement_check(world.grid_w() - 1, world.grid_h() - 1, fw, fh,
                                           /*house=*/1, /*adjacent=*/2, world.passable(),
                                           world.grid_w(), world.grid_h(), world.occupancy(),
                                           world.buildings(), /*bypass_adjacency=*/true).ok);
    // Partial-occupied: drop a 1x1 building on ONE cell of the 2x2 footprint;
    // the overlap must make the full-foundation placement fail.
    world.add_building(0, c->x + 1, c->y + 1, 1, 1, /*house=*/1, 100, 0, 0.0, true);
    CHECK_FALSE(ra2engine::placement_check(c->x, c->y, fw, fh, /*house=*/1, /*adjacent=*/2,
                                           world.passable(), world.grid_w(), world.grid_h(),
                                           world.occupancy(), world.buildings(),
                                           /*bypass_adjacency=*/true).ok);

    // AIBaseSpacing(1) gap through the bound closure [RE-GATE APPLIED:
    // ai-base.md §3]: the AI's can_place probes the foundation EXPANDED by
    // +2*spacing (origin - s, size + 2*s), so a building placed one cell away
    // from a candidate must BLOCK that candidate even though the candidate's
    // RAW foundation is itself free. This exercises the spacing gap, not just
    // the raw footprint (review Minor #1). Uses a fresh 1x1 candidate clear of
    // the buildings placed above.
    const int s = ra2engine::kAiBaseSpacing;  // == 1
    const int gx = c->x + 3, gy = c->y;       // candidate origin (1x1)
    auto raw_ok = [&](int x, int y) {
        return ra2engine::placement_check(x, y, 1, 1, /*house=*/1, /*adjacent=*/2,
                                          world.passable(), world.grid_w(), world.grid_h(),
                                          world.occupancy(), world.buildings(),
                                          /*bypass_adjacency=*/true).ok;
    };
    auto spaced_ok = [&](int x, int y) {  // exactly what ai_place_ready binds
        return ra2engine::placement_check(x - s, y - s, 1 + 2 * s, 1 + 2 * s,
                                          /*house=*/1, /*adjacent=*/2,
                                          world.passable(), world.grid_w(), world.grid_h(),
                                          world.occupancy(), world.buildings(),
                                          /*bypass_adjacency=*/true).ok;
    };
    // Baseline: candidate is buildable and its spacing halo is clear.
    REQUIRE(raw_ok(gx, gy));
    REQUIRE(spaced_ok(gx, gy));
    // Drop a 1x1 building exactly one cell east of the candidate — OUTSIDE the
    // raw 1x1 footprint but INSIDE the +2*spacing expanded foundation.
    world.add_building(0, gx + 1, gy, 1, 1, /*house=*/1, 100, 0, 0.0, true);
    CHECK(raw_ok(gx, gy));          // raw footprint still free -> raw check accepts
    CHECK_FALSE(spaced_ok(gx, gy)); // but the 1-cell gap now blocks the candidate
}

TEST_CASE("World: a power-preemption insert never corrupts a pending queue's node "
          "(stable node identity)") {
    // REGRESSION [fix wave 1]: step_ai_houses stored, per queue, the RAW plan
    // index of the node in production (rec.state.pending[q]). next_build can
    // INSERT a power-preemption node at the first-unsatisfied index; an insert
    // at an index <= a stored pending index shifts the real node by one, but
    // the stored raw index was NOT updated -> when that queue completed,
    // ai_place_ready placed the WRONG (shifted) node: wrong type, wrong cell.
    // The fix gives every PlannedNode a stable id and stores/resolves pending
    // by id. This test reproduces the shift: a defense is mid-production
    // (pending[Defense] -> the defense node) when an earlier radar node dies
    // with power headroom tight, so next_build inserts a Power node AHEAD of
    // it. The in-production defense must still land at ITS OWN node (defense
    // type, its own stored cell), NOT the shifted radar node.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);

    // Roster: [0]conyard [1]power(+50) [2]radar(-40 drainer) [3]defense
    // [4]power2(+100, NOT in the catalog -> the AI never manages it; it only
    // supplies output so radar's ORIGINAL build clears the preemption gate,
    // then we kill it to make the radar REBUILD tight).
    ra2engine::AiHouseConfig cfg;
    cfg.roster.resize(5);
    auto& R = cfg.roster;
    R[0].id = "NACNST"; R[0].conyard = true; R[0].cost = 3000; R[0].tech_level = 1;
    R[0].strength = 1000; R[0].adjacent = 2;
    R[1].id = "NAPOWR"; R[1].cost = 600; R[1].tech_level = 1; R[1].strength = 750;
    R[1].power = 50; R[1].adjacent = 2;
    R[2].id = "NARADR"; R[2].cost = 1000; R[2].tech_level = 1; R[2].strength = 750;
    R[2].power = -40; R[2].adjacent = 2;   // drainer -> a preemption target when dead
    R[3].id = "NALASR"; R[3].cost = 5000; R[3].tech_level = 1; R[3].strength = 1000;
    R[3].power = 0; R[3].adjacent = 2;     // slow (5000c) so it stays in production
    R[4].id = "NAPOW2"; R[4].cost = 600; R[4].tech_level = 1; R[4].strength = 750;
    R[4].power = 100; R[4].adjacent = 2;   // extra output (not a catalog entry)
    cfg.buildup_totals = {0.0, 0.0, 0.0, 0.0, 0.0};
    cfg.catalog.conyard = 0; cfg.catalog.power = 1; cfg.catalog.radar = 2;
    cfg.catalog.defenses = {3};            // one defense type
    cfg.ai.defense_limit = 1;              // -> exactly ONE defense node in the plan
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.difficulty = ra2engine::AiDifficulty::Normal;
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Normal, g);
    cfg.build_speed = 0.1;
    cfg.start_credits = 100000;

    // Seed the ConYard and the extra (un-managed) power source, then flag AI.
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    const int power2_idx =
        world.add_building(4, c->x + 3, c->y, 1, 1, /*house=*/1, 750, 0, 0.0, true);
    REQUIRE(power2_idx >= 0);
    world.enable_ai_house(1, cfg);

    const int kRadar = 2, kDefense = 3;
    auto count_type = [&](int type) {
        int n = 0;
        for (const auto& b : world.buildings())
            if (b.alive && b.state != ra2engine::Building::State::Dead &&
                b.house == 1 && b.type == type) ++n;
        return n;
    };
    auto find_alive = [&](int type) {
        for (int i = 0; i < (int)world.buildings().size(); ++i) {
            const auto& b = world.buildings()[i];
            if (b.alive && b.state != ra2engine::Building::State::Dead &&
                b.house == 1 && b.type == type) return i;
        }
        return -1;
    };

    // Phase 1: let the AI build its power node, then the radar (output 150 from
    // power2+power keeps radar's build under the preemption gate).
    int steps = 0;
    for (; steps < 8000 && count_type(kRadar) < 1; ++steps) world.step(1.0 / 15.0);
    REQUIRE(count_type(kRadar) == 1);

    // Phase 2: step until the AI starts the (slow) defense in the Defense queue.
    for (; steps < 12000 && world.ai_producing(1, 1) != kDefense; ++steps)
        world.step(1.0 / 15.0);
    REQUIRE(world.ai_producing(1, 1) == kDefense);

    // Record the defense node's OWN stored cell (chosen at start, before any
    // insert) — the place the defense MUST land.
    const ra2engine::AiBaseState* st = world.ai_state(1);
    REQUIRE(st != nullptr);
    int dcx = -1, dcy = -1;
    for (const ra2engine::PlannedNode& n : st->plan)
        if (n.type == kDefense) { dcx = n.cx; dcy = n.cy; break; }
    REQUIRE(dcx != 0);  // a real cell was chosen (not the unset (0,0) sentinel)

    // Kill the extra power source (drops output 150 -> 50, plan power node still
    // satisfied) AND the radar (its node goes dead). Now rebuilding radar would
    // push drain past output - PowerSurplus(50), so the NEXT next_build pass
    // INSERTS a Power node ahead of the radar node — shifting the pending
    // defense node by one.
    ra2engine::kill_building(world.buildings()[power2_idx]);
    const int radar_idx = find_alive(kRadar);
    REQUIRE(radar_idx >= 0);
    ra2engine::kill_building(world.buildings()[radar_idx]);

    // Phase 3: run long enough for the insert to fire AND the defense to finish.
    for (int i = 0; i < 3000 && world.ai_producing(1, 1) == kDefense; ++i)
        world.step(1.0 / 15.0);
    REQUIRE(world.ai_producing(1, 1) != kDefense);   // defense completed + placed

    // The defense must have been placed at ITS OWN node: a building of the
    // DEFENSE type, at the defense node's stored cell. Before the fix the
    // Defense queue placed the shifted RADAR node instead (wrong type/cell),
    // so no defense-type building exists here.
    const int def_idx = find_alive(kDefense);
    REQUIRE(def_idx >= 0);                            // FAILS before the fix
    CHECK(world.buildings()[def_idx].cx == dcx);
    CHECK(world.buildings()[def_idx].cy == dcy);
}

TEST_CASE("World: a PRE-BUILT building is synced into its plan node and rebuilt "
          "IN PLACE when destroyed") {
    // REGRESSION [fix wave — final review IMPORTANT-1]: ai-base.md §3 —
    // FUN_0050a3e0 builds the base plan AND walks the house's already-placed
    // buildings, recording each live building's cell + World index into its
    // matching plan node. Our engine skipped that sync, so a node covering a
    // PRE-BUILT building (spawn_base, the seeded demo ConYard) stayed
    // cx=cy=0/building=-1. When such a pre-built building was destroyed,
    // node-death detection (n.building==-1) never fired, the node looked FRESH
    // (cx==0&&cy==0) and the AI rebuilt at a NEW nearest-ring cell instead of
    // IN PLACE at the retained cell — breaking the §3/§5 rebuild-in-place
    // contract on the exact Goal scenario (the pre-built enemy base rebuilding
    // what it loses). This test FAILS before the sync (node never linked;
    // rebuild lands at the wrong cell) and PASSES after.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // [0]=conyard 1x1, [1]=power 1x1; BOTH in the catalog so the plan has a
    // node for each (so the pre-built NAPOWR has a node to be synced into).
    ra2engine::AiHouseConfig cfg;
    cfg.roster.resize(2);
    cfg.roster[0].id = "NACNST"; cfg.roster[0].conyard = true;
    cfg.roster[0].cost = 3000; cfg.roster[0].tech_level = 1;
    cfg.roster[0].strength = 1000; cfg.roster[0].adjacent = 2;
    cfg.roster[1].id = "NAPOWR"; cfg.roster[1].cost = 600;
    cfg.roster[1].tech_level = 1; cfg.roster[1].strength = 750;
    cfg.roster[1].power = 150; cfg.roster[1].adjacent = 2;
    cfg.buildup_totals = {0.0, 0.0};
    cfg.catalog.conyard = 0;
    cfg.catalog.power = 1;
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.difficulty = ra2engine::AiDifficulty::Normal;
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Normal, g);
    cfg.build_speed = 0.1;
    cfg.start_credits = 10000;

    // Pre-build BOTH the ConYard and a NAPOWR at KNOWN cells (like spawn_base),
    // BEFORE flagging the house. The NAPOWR sits far (+6,+6) from the ConYard
    // so a FRESH nearest-ring rebuild (the pre-fix bug) would land somewhere
    // else entirely — making the stored-cell check discriminating.
    const int cy_idx = world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    const int napowr_cx = c->x + 6, napowr_cy = c->y + 6;
    const int pw_idx =
        world.add_building(1, napowr_cx, napowr_cy, 1, 1, /*house=*/1, 750, 0, 0.0, true);
    REQUIRE(cy_idx == 0);
    REQUIRE(pw_idx == 1);
    world.enable_ai_house(1, cfg);

    // One tick: the plan builds + the pre-built buildings sync into their nodes.
    world.step(1.0 / 15.0);
    const ra2engine::AiBaseState* st = world.ai_state(1);
    REQUIRE(st != nullptr);
    REQUIRE(st->plan.size() == 2);        // conyard node + power node
    const ra2engine::PlannedNode& pnode = st->plan[1];
    REQUIRE(pnode.type == 1);             // the power node
    // The sync recorded the pre-built NAPOWR's cell + World index into its node.
    CHECK(pnode.building == pw_idx);      // FAILS before the fix (stays -1)
    CHECK(pnode.cx == napowr_cx);         // FAILS before the fix (stays 0)
    CHECK(pnode.cy == napowr_cy);         // FAILS before the fix (stays 0)

    // Destroy the pre-built NAPOWR. Because its node is now LINKED, node-death
    // detection flips the node dead and the AI rebuilds it at the RETAINED cell.
    ra2engine::kill_building(world.buildings()[pw_idx]);
    for (int i = 0; i < 5000 && !(world.ai_state(1)->plan[1].building > pw_idx); ++i)
        world.step(1.0 / 15.0);
    const ra2engine::PlannedNode& rebuilt = world.ai_state(1)->plan[1];
    REQUIRE(rebuilt.building > pw_idx);   // a NEW building index (rebuilt)
    CHECK_FALSE(rebuilt.dead);
    CHECK(world.buildings()[rebuilt.building].type == 1);
    CHECK(world.buildings()[rebuilt.building].alive);
    // Rebuilt IN PLACE at the pre-built cell — NOT a fresh nearest-ring cell
    // (the pre-fix bug placed it near the ConYard center instead).
    CHECK(world.buildings()[rebuilt.building].cx == napowr_cx);
    CHECK(world.buildings()[rebuilt.building].cy == napowr_cy);
}

// ---- AI-2 offensive-teams phase [RE: ai-team.md] : step_ai_teams integration ----

namespace {
// Tiny AI-2 world config: buildings [0]=NACNST conyard, [1]=NAHAND barracks;
// unit roster [0] = one infantry type (image "AITEST"). aimd: ONE autocreate
// TeamType needing 1 x type0, script = attack-anything then success.
ra2engine::AiHouseConfig ai2_cfg(bool teams_enabled, bool unreachable_move = false) {
    ra2engine::AiHouseConfig cfg;
    cfg.roster.resize(2);
    cfg.roster[0].id = "NACNST"; cfg.roster[0].conyard = true;
    cfg.roster[0].cost = 3000; cfg.roster[0].tech_level = 1;
    cfg.roster[0].strength = 1000; cfg.roster[0].adjacent = 2;
    cfg.roster[1].id = "NAHAND"; cfg.roster[1].cost = 500;
    cfg.roster[1].tech_level = 1; cfg.roster[1].strength = 500;
    cfg.roster[1].adjacent = 2;
    cfg.buildup_totals = {0.0, 0.0};
    // Empty AiCatalog -> the AI-1 base brain plans nothing (all -1 skipped);
    // this test exercises ONLY the team brain.
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Normal, g);
    cfg.build_speed = 0.05;    // 200c infantry -> 9 frames -> fast test builds
    cfg.start_credits = 10000;
    if (!teams_enabled) return cfg;
    cfg.team.enabled = true;
    cfg.team.house_name = "Russians";
    cfg.team.inf_factory_types = {1};   // NAHAND
    cfg.team.veh_factory_types = {};
    // Fast test clocks (the REAL cadences are the Task 3 constants
    // [RE: ai-team.md §4]): TeamDelays check pass every 1 s, AutocreateTime spawn
    // every ~1 s, so an autocreate team lands quickly in the short test window.
    cfg.team.rules.team_delay_frames = 15;         // 1 s management check pass
    cfg.team.rules.autocreate_time_min = 1.0 / 60.0;  // ~1 s spawn cadence
    ra2engine::UnitTypeDef u;
    u.id = "E2TEST";
    u.cost = 200;
    u.tech_level = 1;
    u.is_vehicle = false;
    u.prerequisite = {"NAHAND"};
    u.combat.strength = 100;
    u.combat.image = "AITEST";
    cfg.team.unit_roster.push_back(u);
    ra2engine::TaskForce tf;
    tf.id = "TF1";
    tf.members = {{1, 0}};              // 1 x type0
    cfg.team.aimd.taskforces.push_back(tf);
    ra2engine::ScriptType sc;
    sc.id = "SC1";
    if (unreachable_move) {
        // Move to a mapped waypoint that is OFF-MAP: every snap candidate is out
        // of bounds so the snap fails, order_unit_move cancels, and the member
        // can never satisfy the arrival predicate. The generalized backstop must
        // re-issue then advance the pc so the team disbands (never wedges) — the
        // FIX-1 wedge-freedom guarantee exercised end-to-end through step_ai_teams.
        cfg.team.waypoints = {ra2engine::Cellxy{-100, -100}};
        sc.actions = {{ra2engine::kOpMoveToWaypoint, 0},
                      {ra2engine::kOpDisband, 0}};
    } else {
        sc.actions = {{ra2engine::kOpAttackQuarry, ra2engine::kQuarryAnything},
                      {ra2engine::kOpDisband, 0}};
    }
    cfg.team.aimd.scripts.push_back(sc);
    ra2engine::TeamType tt;
    tt.id = "TT1";
    tt.house = "Russians";
    tt.taskforce = 0;
    tt.script = 0;
    tt.priority = 7;
    tt.max = 1;
    tt.autocreate = true;
    cfg.team.aimd.teams.push_back(tt);
    return cfg;
}
}  // namespace

TEST_CASE("World: step_ai_teams firewall - team.enabled=false means no teams ever") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.enable_ai_house(1, ai2_cfg(/*teams_enabled=*/false));
    const std::size_t nu = world.units().size();
    for (int i = 0; i < 300; ++i) world.step(1.0 / 15.0);
    REQUIRE(world.ai_team_state(1) != nullptr);
    CHECK(world.ai_team_state(1)->teams.empty());        // firewall held
    CHECK(world.ai_team_state(1)->teams_created == 0);
    CHECK(world.units().size() == nu);                   // nothing produced
    CHECK(world.ai_unit_producing(1, 0) == -1);
}

TEST_CASE("World: a team recruits an IDLE unit first and attacks the player") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    // An IDLE house-1 combat unit of the roster type (image match) + a
    // house-0 target unit placed WELL OUTSIDE auto-acquire reach (20 cells,
    // weapon range 4) so the AI unit stays genuinely idle/recruitable until
    // the TEAM issues the attack order — the recruit-idle premise under test.
    ra2engine::UnitCombat uc;
    uc.strength = 100;
    uc.image = "AITEST";
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(c->x + 3, c->y + 2, /*house=*/1, uc);
    ra2engine::UnitCombat pc0 = uc;
    pc0.image = "PLTEST";
    world.add_combat_unit(c->x + 20, c->y + 2, /*house=*/0, pc0);
    world.enable_ai_house(1, ai2_cfg(/*teams_enabled=*/true));
    const std::size_t nu = world.units().size();
    // Step: autocreate fires (1 s test clock), recruit-idle claims the
    // standing unit (NO production needed), the team gathers then executes
    // attack-anything at the house-0 unit.
    bool attacked = false;
    for (int i = 0; i < 1200 && !attacked; ++i) {
        world.step(1.0 / 15.0);
        const ra2engine::Unit& u = world.units()[0];   // the AI unit
        attacked = u.attack_order && (u.target >= 0 || u.target_building >= 0);
    }
    REQUIRE(world.ai_team_state(1) != nullptr);
    CHECK(world.ai_team_state(1)->teams_created == 1);   // Max=1 respected
    CHECK(world.units().size() == nu);                   // recruit-idle, NOT produce
    CHECK(world.ai_unit_producing(1, 0) == -1);          // Infantry queue never ran
    REQUIRE(attacked);
    CHECK(world.units()[0].target == 1);                 // the house-0 unit
}

TEST_CASE("World: a team produces its shortfall through the EXISTING Infantry queue") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(1, c->x + 3, c->y, 1, 1, /*house=*/1, 500, 0, 0.0, true);  // NAHAND
    world.enable_ai_house(1, ai2_cfg(/*teams_enabled=*/true));
    const std::size_t nu = world.units().size();
    const double credits0 = world.ai_credits(1);
    // No idle unit exists -> next_produce_type drives the Infantry queue
    // (tab2) -> launch_unit spawns at the barracks exit -> the unit is
    // claimed into the recruiting team [RE: ai-team.md §2/§6].
    for (int i = 0; i < 1200 && world.units().size() < nu + 1; ++i)
        world.step(1.0 / 15.0);
    REQUIRE(world.units().size() == nu + 1);
    const ra2engine::Unit& made = world.units()[nu];
    CHECK(made.house == 1);
    CHECK(made.image == "AITEST");
    CHECK(world.ai_credits(1) < credits0);               // charged through the queue
    REQUIRE(world.ai_team_state(1) != nullptr);
    REQUIRE(world.ai_team_state(1)->teams.size() >= 1);
    const ra2engine::TeamInstance& t = world.ai_team_state(1)->teams[0];
    REQUIRE(t.members.size() == 1);
    CHECK(t.members[0] == (int)nu);                      // claimed on launch
    CHECK(t.member_types[0] == 0);
    // The single-slot queue held ONE unit at a time (Max=1 team of 1):
    for (int i = 0; i < 300; ++i) world.step(1.0 / 15.0);
    CHECK(world.units().size() == nu + 1);               // no runaway production
}

TEST_CASE("World: an Executing move whose ideal cell is unreachable does NOT wedge") {
    // [FIX: Executing-move wedge] Before the fix, a move opcode whose target's
    // CloseEnough neighborhood is blocked/unreachable ordered members onto an
    // unreachable cell; order_unit_move cancelled, arrival vs the ideal was
    // false forever, move_issued stayed true, and the team wedged in Executing
    // PERMANENTLY holding its Max/TotalAITeamCap slot — the AI eventually stops
    // attacking. With the generalized backstop the team re-issues then advances
    // past the dead move, disbands, and FREES its cap slot so a fresh team spawns.
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);  // AI conyard
    // An idle recruitable AI unit (fills the team with no production wait) and a
    // far house-0 unit so the enemy-threat centroid (gather-ring centre) is
    // well-defined — the gather succeeds and the team reaches Executing, exactly
    // as in the recruit-idle test above.
    ra2engine::UnitCombat uc;
    uc.strength = 100; uc.image = "AITEST";
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(c->x + 3, c->y + 2, /*house=*/1, uc);
    ra2engine::UnitCombat pc0 = uc; pc0.image = "PLTEST";
    world.add_combat_unit(c->x + 20, c->y + 2, /*house=*/0, pc0);
    world.enable_ai_house(1, ai2_cfg(/*teams_enabled=*/true, /*unreachable_move=*/true));

    // Observe the lifecycle: the team must actually REACH Executing (non-vacuous
    // — the move opcode is genuinely exercised) AND the cap slot must free (a
    // second team is created once the first disbands past the dead move). Under
    // the bug the first team wedges in Executing forever -> teams_created pegs at 1.
    bool saw_executing = false;
    int max_created = 0;
    for (int i = 0; i < 4000; ++i) {
        world.step(1.0 / 15.0);
        const ra2engine::AiTeamState* st = world.ai_team_state(1);
        REQUIRE(st != nullptr);
        max_created = std::max(max_created, st->teams_created);
        for (const ra2engine::TeamInstance& t : st->teams)
            if (t.state == ra2engine::TeamState::Executing) saw_executing = true;
        if (saw_executing && max_created >= 2) break;
    }
    CHECK(saw_executing);           // the Executing move opcode was reached
    CHECK(max_created >= 2);        // team disbanded past the dead move + cap freed
    // Max=1 held throughout: teams recycle (create -> wedge-free disband ->
    // create) rather than accumulating a leaked wedged instance.
    CHECK((int)world.ai_team_state(1)->teams.size() <= 1);
}

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

// AI-3 non-Normal difficulty fixture [RE-GATE STOP: the 2-difficulty edge].
// Drives the house at AiDifficulty::Difficult (engine enum 2, 0 = EASIEST) with
// TWO triggers that differ ONLY in their per-difficulty enable slot, sharing
// one always-satisfiable enemy-owns condition and equal weight so the enable
// slot alone decides eligibility:
//   TRG_HARD (index 0) -> team TT2, enabled = {easy=0, normal=0, hard=1}
//   TRG_EASY (index 1) -> team TT3, enabled = {easy=1, normal=0, hard=0}
// The World converts the enum to the pure engine's 0 = HARDEST convention, so a
// Difficult AI consults the HARD flag: TRG_HARD is eligible/fires, TRG_EASY is
// NOT. Under the pre-fix non-inverted fill the two SWAP (Easy fires, Hard is
// excluded), so this pins the world<->engine difficulty-convention seam.
ra2engine::AiHouseConfig ai3_cfg_difficult() {
    ra2engine::AiHouseConfig cfg = ai3_cfg(/*triggers_enabled=*/true);
    cfg.difficulty = ra2engine::AiDifficulty::Difficult;   // enum 2 (0 = EASIEST)
    ra2engine::DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = ra2engine::kDifficultyCost[i];
        g[i].build_time = ra2engine::kDifficultyBuildTime[i];
        g[i].build_delay = ra2engine::kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];
    }
    cfg.mods = ra2engine::difficulty_mods(ra2engine::AiDifficulty::Difficult, g);
    // Second trigger-only team TT3 (teams index 2), reachable ONLY via TRG_EASY.
    ra2engine::TeamType tt3;
    tt3.id = "TT3"; tt3.house = "Russians";
    tt3.taskforce = 0; tt3.script = 0; tt3.priority = 5; tt3.max = 1;
    tt3.autocreate = false;
    cfg.team.aimd.teams.push_back(tt3);   // index 2
    // Rebuild the trigger pool as exactly the two enable-slot variants off the
    // ai3_cfg TRG1 template (same condition/comparator/weights/side/tech).
    const ra2engine::AITriggerType base = cfg.team.aimd.triggers[0];
    cfg.team.aimd.triggers.clear();
    ra2engine::AITriggerType hard = base;
    hard.id = "TRG_HARD"; hard.team1 = 1;   // TT2
    hard.enabled[0] = false; hard.enabled[1] = false; hard.enabled[2] = true;
    ra2engine::AITriggerType easy = base;
    easy.id = "TRG_EASY"; easy.team1 = 2;   // TT3
    easy.enabled[0] = true; easy.enabled[1] = false; easy.enabled[2] = false;
    cfg.team.aimd.triggers.push_back(hard);   // index 0
    cfg.team.aimd.triggers.push_back(easy);   // index 1
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

// The world<->engine difficulty-convention seam [RE-GATE STOP: non-inverted
// difficulty]. A Difficult AI (engine enum 2) MUST consult the HARD enable
// slot, not Easy: the World converts the enum (0 = EASIEST) to the pure
// engine's 0 = HARDEST convention, which trigger_filters_pass re-inverts as
// enabled[2-difficulty]. The Hard-slot-only trigger fires; the Easy-slot-only
// trigger is INELIGIBLE and never fires. Under the pre-fix non-inverted fill
// these SWAP, so this fails without the world.cpp `2 - (int)difficulty` fix.
TEST_CASE("World: a Difficult AI consults the HARD enable slot, not Easy [RE-GATE: 2-difficulty edge]") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(0, c->x + 20, c->y + 2, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    // One IDLE house-1 unit so the fired team fills instantly and runs to disband.
    ra2engine::UnitCombat uc;
    uc.strength = 100; uc.image = "AITEST";
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(c->x + 3, c->y + 2, /*house=*/1, uc);
    world.enable_ai_house(1, ai3_cfg_difficult());
    const ra2engine::AiTriggerState* ts0 = world.ai_trigger_state(1);
    REQUIRE(ts0 != nullptr);
    REQUIRE(ts0->weights.size() == 2);
    const int kHard = 0, kEasy = 1;
    CHECK(ts0->weights[kHard] == 50.0);
    CHECK(ts0->weights[kEasy] == 50.0);
    for (int i = 0; i < 4500 && world.ai_trigger_state(1)->triggers_fired < 1; ++i)
        world.step(1.0 / 15.0);
    const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
    REQUIRE(ts != nullptr);
    CHECK(ts->triggers_fired >= 1);
    CHECK(ts->last_fired == kHard);              // the Hard-slot trigger fired
    // The Easy-slot trigger is INELIGIBLE for a Difficult AI: TT3 is never
    // created, so its weight is never adjusted (adjustment runs only at the
    // disband of a team whose Team1 == the trigger's team) — it stays at start.
    CHECK(ts->weights[kEasy] == 50.0);
}

// World-level FAILURE attribution end-to-end [RE: ai-trigger.md §4/§6]. The
// trigger fires + creates its team; the recruited member is WIPED before the
// script reaches kOpDisband, so completed_success stays FALSE and the disband
// scan takes the RegisterFailure path. Success attribution is covered above;
// this pins the failure branch through the World glue. Weight math (E=0, first
// outcome, no track term): 50 + (-50) = 0, CLAMPED to min_weight 10.
TEST_CASE("World: a trigger whose team is WIPED before it completes takes the FAILURE weight path [RE: ai-trigger.md 4/6]") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);
    world.add_building(0, c->x + 20, c->y + 2, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    ra2engine::UnitCombat uc;
    uc.strength = 100; uc.image = "AITEST";
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(c->x + 3, c->y + 2, /*house=*/1, uc);
    world.enable_ai_house(1, ai3_cfg(/*triggers_enabled=*/true));
    REQUIRE(world.ai_trigger_state(1) != nullptr);
    REQUIRE(world.ai_trigger_state(1)->weights.size() == 1);
    CHECK(world.ai_trigger_state(1)->weights[0] == 50.0);
    // Fire the trigger + recruit the idle unit into the team (Gathering is
    // movement-bound, so this lands well before the bare-disband op).
    int member = -1;
    for (int i = 0; i < 4500 && member < 0; ++i) {
        world.step(1.0 / 15.0);
        const ra2engine::AiTeamState* tst = world.ai_team_state(1);
        if (tst)
            for (const ra2engine::TeamInstance& t : tst->teams)
                if (!t.members.empty()) { member = t.members[0]; break; }
    }
    REQUIRE(member >= 0);
    REQUIRE(world.ai_trigger_state(1)->adjustments == 0);   // not yet disbanded
    world.units()[member].alive = false;                    // WIPE mid-script
    for (int i = 0; i < 4500 && world.ai_trigger_state(1)->adjustments < 1; ++i)
        world.step(1.0 / 15.0);
    const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
    REQUIRE(ts != nullptr);
    REQUIRE(ts->adjustments == 1);
    CHECK(ts->last_adjusted == 0);
    CHECK_FALSE(ts->last_success);               // wipe -> completed_success FALSE
    CHECK(ts->times_executed[0] == 1);           // E incremented every outcome
    CHECK(ts->times_completed[0] == 0);          // C NOT incremented on failure
    CHECK(ts->weights[0] == 10.0);               // 50 - 50 = 0 -> clamp to min 10
    CHECK(ts->last_old_weight == 50.0);
    CHECK(ts->last_new_weight == 10.0);
}

// Direct step_team coverage the Task-3 review flagged, now that the World
// consumes the flag [RE: ai-trigger.md §4]: completed_success is set TRUE only
// when the script runs to kOpDisband (the original's team+0x84 success flag),
// and stays FALSE when the team reaches Disbanding via a member wipe. This
// pins the exact bit the AI-3 disband attribution keys success-vs-failure on.
TEST_CASE("step_team: completed_success TRUE at kOpDisband, FALSE on member wipe [RE: ai-trigger.md 4]") {
    ra2engine::AimdData aimd;
    ra2engine::TaskForce tf; tf.id = "TF1"; tf.members = {{1, 0}};  // 1 x type0
    aimd.taskforces.push_back(tf);
    ra2engine::ScriptType sc; sc.id = "SC1";
    sc.actions = {ra2engine::ScriptAction{ra2engine::kOpDisband, 0}};  // bare disband
    aimd.scripts.push_back(sc);
    ra2engine::TeamType tt; tt.id = "TT1"; tt.taskforce = 0; tt.script = 0;
    tt.priority = 5; tt.max = 1; tt.full = false;
    aimd.teams.push_back(tt);

    // --- success path: script runs to kOpDisband ---
    {
        ra2engine::TeamInstance t; t.teamtype = 0;
        t.members = {0}; t.member_types = {0};
        ra2engine::TeamView v;
        v.members.push_back(ra2engine::TeamMemberView{0, true, 5, 5, true});
        v.base_x = 5; v.base_y = 5; v.enemy_x = 5; v.enemy_y = 5;
        std::uint32_t rng = 0x2545F491u;
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // Recruiting -> Gathering
        REQUIRE(t.state == ra2engine::TeamState::Gathering);
        // Stand the member ON the realized gather cell so gather completes.
        v.members[0].cx = t.gather_x; v.members[0].cy = t.gather_y;
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // Gathering: emit move
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // Gathering: within -> Executing
        REQUIRE(t.state == ra2engine::TeamState::Executing);
        CHECK_FALSE(t.completed_success);                       // not yet
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // Executing: kOpDisband
        CHECK(t.state == ra2engine::TeamState::Disbanding);
        CHECK(t.completed_success);                             // success flag set
    }

    // --- wipe path: all members die before the script completes ---
    {
        ra2engine::TeamInstance t; t.teamtype = 0;
        t.members = {0}; t.member_types = {0};
        ra2engine::TeamView v;
        v.members.push_back(ra2engine::TeamMemberView{0, true, 5, 5, true});
        v.base_x = 5; v.base_y = 5; v.enemy_x = 5; v.enemy_y = 5;
        std::uint32_t rng = 0x2545F491u;
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // Recruiting -> Gathering
        REQUIRE(t.state == ra2engine::TeamState::Gathering);
        v.members[0].alive = false;                             // wiped
        ra2engine::step_team(t, v, aimd, rng, 1.0);            // alive==0 -> Disbanding
        CHECK(t.state == ra2engine::TeamState::Disbanding);
        CHECK_FALSE(t.completed_success);                       // wipe -> failure witness
    }
}

// ---- CB-1 Transports/Cargo (guarded real map) [RE: transport-cargo.md] ----
namespace {
// HTK-shaped transport stats scaled for tests: a real vehicle profile with a
// cargo pool. Task 4 resolves real HTK numbers from rulesmd; tests hand-pick
// small pools to hit boundaries fast.
ra2engine::UnitCombat transport_stats(int passengers, int size_limit) {
    ra2engine::UnitCombat c;
    c.strength = 200;
    c.armor = "light";
    c.image = "HTK";
    c.is_vehicle = true;
    c.rot = 5;
    c.speed_cps = 4.0;
    c.passengers = passengers;
    c.size_limit = size_limit;
    c.size = 3;
    return c;
}
// First free walkable cell scanning east from (x0, y).
bool free_cell_east(ra2engine::World& w, int x0, int y, int& outx) {
    for (int dx = 0; dx < 12; ++dx)
        if (w.walkable(x0 + dx, y) && w.unit_at_cell(x0 + dx, y) < 0) {
            outx = x0 + dx;
            return true;
        }
    return false;
}
// True iff NO occupancy cell anywhere is owned by unit `idx`.
bool owns_no_cell(const ra2engine::World& w, int idx) {
    const ra2engine::Occupancy& occ = w.occupancy();
    for (int y = 0; y < occ.height(); ++y)
        for (int x = 0; x < occ.width(); ++x)
            if (occ.at(x, y) == idx) return false;
    return true;
}
}  // namespace

TEST_CASE("World: cargo boarding takes the passenger off-map and releases ALL occupancy") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(2, 2));   // unit 0: transport
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 3, c->y, gx));
    world.add_combat_unit(gx, c->y, 0, gi_stats());              // unit 1: passenger
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    CHECK(world.units()[1].enter_target == 0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);                     // boarded
    REQUIRE(world.units()[0].cargo.size() == 1);
    CHECK(world.units()[0].cargo[0] == 1);
    CHECK(world.units()[1].enter_target == -1);
    CHECK_FALSE(world.units()[1].selected);                      // force-cleared
    CHECK(owns_no_cell(world, 1));      // ALL occupancy released (incl. any
                                        // mid-hop reservation) -- spec §9
    // Off-map: unpickable, undrawn, untargetable.
    CHECK(world.unit_at_cell(world.units()[1].cell_x, world.units()[1].cell_y) != 1);
    for (int i : ra2engine::unit_draw_order(world.units())) CHECK(i != 1);
    int ex = -1;
    REQUIRE(free_cell_east(world, tx + 1, c->y + 2, ex));
    world.add_combat_unit(ex, c->y + 2, 1, cons_stats());        // unit 2: enemy
    world.step(dt);                                              // auto-acquire pass
    CHECK(world.units()[2].target != 1);                         // never the carried unit
    // One-unit-per-cell audit stays green among on-map units.
    for (std::size_t a = 0; a < world.units().size(); ++a)
        for (std::size_t b = a + 1; b < world.units().size(); ++b) {
            const ra2engine::Unit& ua = world.units()[a];
            const ra2engine::Unit& ub = world.units()[b];
            if (!ua.alive || ua.cargo_of >= 0 || !ub.alive || ub.cargo_of >= 0) continue;
            CHECK((ua.cell_x != ub.cell_x || ua.cell_y != ub.cell_y));
        }
}

TEST_CASE("World: cargo eject uses the facing-scatter eject_cell, stamps coords/facing, LIFO one per frame") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(3, 2));   // unit 0: Passengers=3
    const double dt = 1.0 / 15.0;
    for (int k = 0; k < 2; ++k) {                                // units 1, 2
        int gx = -1;
        REQUIRE(free_cell_east(world, tx + 2 + k, c->y + 1, gx));
        world.add_combat_unit(gx, c->y + 1, 0, gi_stats());
    }
    world.units()[1].selected = true;
    world.units()[2].selected = true;
    world.order_enter_transport(0);
    for (int f = 0; f < 300 && world.units()[0].cargo.size() < 2; ++f) world.step(dt);
    REQUIRE(world.units()[0].cargo.size() == 2);
    const std::vector<int> board_order = world.units()[0].cargo;   // boarding order;
                                                                   // LIFO ejects the
                                                                   // BACK first
    (void)world.take_cargo_events();                               // drain the boards
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    CHECK(world.units()[0].unload_pending);
    // The eject cell must be EXACTLY the pure picker's answer [RE-GATE
    // APPLIED: transport-cargo.md §4 -- facing-scatter, NOT a ring walk].
    auto free_cell = [&](int x, int y) {
        return world.walkable(x, y) && world.occupancy().is_free(x, y);
    };
    const int ttx = world.units()[0].cell_x, tty = world.units()[0].cell_y;
    const ra2engine::Cellxy expect = ra2engine::eject_cell(
        ttx, tty, world.units()[0].body_facing, world.units()[0].eject_counter, free_cell);
    REQUIRE(expect.x >= 0);
    world.step(dt);                                                // one eject per frame
    std::vector<ra2engine::CargoEvent> ev = world.take_cargo_events();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].kind == ra2engine::CargoEvent::Kind::Eject);
    CHECK(ev[0].unit == board_order[1]);                           // LIFO: the LAST
                                                                   // boarded leaves
                                                                   // FIRST [RE-GATE
                                                                   // APPLIED: transport-
                                                                   // cargo.md §6]
    CHECK(ev[0].x == expect.x);                                    // the scatter cell
    CHECK(ev[0].y == expect.y);
    const ra2engine::Unit& p0 = world.units()[board_order[1]];
    CHECK(p0.cargo_of == -1);
    CHECK(p0.cell_x == expect.x);                                  // coords stamped
    CHECK(p0.cell_y == expect.y);
    CHECK(p0.fx == doctest::Approx((double)expect.x));
    CHECK(world.occupancy().at(expect.x, expect.y) == board_order[1]);  // claimed
    CHECK(p0.guard_x == expect.x);                                 // guard anchor
    CHECK(p0.facing >= 0);
    CHECK(p0.facing < 8);
    CHECK(world.units()[0].eject_counter == 1);                    // side alternator
                                                                   // bumped [§4]
    CHECK(world.units()[0].cargo.size() == 1);                     // ONE per frame
    world.step(dt);
    ev = world.take_cargo_events();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].unit == board_order[0]);                           // LIFO: the FIRST
                                                                   // boarded leaves LAST
    CHECK(world.units()[0].cargo.empty());
    CHECK_FALSE(world.units()[0].unload_pending);                  // cleared when empty
}

TEST_CASE("World: cargo boxed-in unload HOLDs, then resumes when a neighbouring cell frees") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    const int ty = c->y;
    world.add_combat_unit(tx, ty, 0, transport_stats(2, 2));     // unit 0
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 2, ty, gx));
    world.add_combat_unit(gx, ty, 0, gi_stats());                // unit 1
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);
    (void)world.take_cargo_events();  // drain the board event so the HOLD check
                                      // below sees ONLY unload-pass activity
    // Box the transport in: fill EVERY free walkable cell at Chebyshev
    // r <= 2 (eject_cell's whole reach -- the r=1 primary/fallback plus the
    // r=2 fallback ring) with plain units. Track one OUTER (r == 2) blocker
    // to release later -- inner blockers stay put.
    int mover = -1;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int x = tx + dx, y = ty + dy;
            if (!world.walkable(x, y) || world.unit_at_cell(x, y) >= 0) continue;
            world.add_unit(x, y, "GI");
            const int bi = static_cast<int>(world.units().size()) - 1;
            if (mover < 0 && std::max(std::abs(dx), std::abs(dy)) == 2) mover = bi;
        }
    REQUIRE(mover >= 0);
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    for (int f = 0; f < 5; ++f) world.step(dt);
    CHECK(world.units()[0].cargo.size() == 1);                   // HOLD: still aboard
    CHECK(world.units()[0].unload_pending);                      // order persists
    CHECK(world.take_cargo_events().empty());                    // no eject happened
    // Free one outer (r=2 fallback) cell: walk the tracked blocker away.
    world.clear_selection();
    world.units()[mover].selected = true;
    world.order_move_selected(tx + 8, ty + 8);
    for (int f = 0; f < 400 && world.units()[0].cargo.size() == 1; ++f) world.step(dt);
    CHECK(world.units()[0].cargo.empty());                       // resumed + ejected
    CHECK(world.units()[1].cargo_of == -1);
    CHECK_FALSE(world.units()[0].unload_pending);
}

TEST_CASE("World: cargo transport death tombstones its cargo; enter orders cancel cleanly") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(2, 2));   // unit 0
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y, gx));
    world.add_combat_unit(gx, c->y, 0, gi_stats());              // unit 1: boards
    int hx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y + 3, hx));
    world.add_combat_unit(hx, c->y + 3, 0, gi_stats());          // unit 2: en route
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);
    world.clear_selection();
    world.units()[2].selected = true;
    world.order_enter_transport(0);
    REQUIRE(world.units()[2].enter_target == 0);
    // Kill the transport the way combat does (die_time flips >= 0).
    world.units()[0].die_time = 0.0;
    world.step(dt);
    CHECK_FALSE(world.units()[1].alive);                         // died WITH it
    CHECK(world.units()[1].cargo_of == -1);
    CHECK(world.units()[1].cell_x == world.units()[0].cell_x);   // coords stamped
    CHECK(world.units()[1].cell_y == world.units()[0].cell_y);
    CHECK(world.units()[0].cargo.empty());
    CHECK(world.units()[2].enter_target == -1);                  // cancelled cleanly
    CHECK(world.units()[2].alive);                               // ...and unharmed
}

TEST_CASE("World: cargo capacity refusal at boarding; a new order supersedes enter/unload") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(1, 2));   // unit 0: ONE slot
    for (int k = 0; k < 2; ++k) {                                // units 1, 2
        int gx = -1;
        REQUIRE(free_cell_east(world, tx + 2 + k, c->y + 1, gx));
        world.add_combat_unit(gx, c->y + 1, 0, gi_stats());
    }
    world.units()[1].selected = true;
    world.units()[2].selected = true;
    world.order_enter_transport(0);                              // oversubscribed
    const double dt = 1.0 / 15.0;
    int boards = 0, refusals = 0;
    for (int f = 0; f < 400 && boards + refusals < 2; ++f) {
        world.step(dt);
        for (const ra2engine::CargoEvent& e : world.take_cargo_events()) {
            if (e.kind == ra2engine::CargoEvent::Kind::Board) ++boards;
            if (e.kind == ra2engine::CargoEvent::Kind::Refuse) ++refusals;
        }
    }
    CHECK(boards == 1);                                          // exactly one fits
    CHECK(refusals == 1);                                        // one REFUSED at full
    CHECK(world.units()[0].cargo.size() == 1);
    // A new move order on the transport supersedes a pending unload.
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    CHECK(world.units()[0].unload_pending);
    world.order_move_selected(tx + 6, c->y + 6);
    CHECK_FALSE(world.units()[0].unload_pending);                // superseded
    // A new move order on an en-route passenger supersedes its enter order.
    int wx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y + 4, wx));
    world.add_combat_unit(wx, c->y + 4, 0, gi_stats());          // unit 3
    int vx = -1;
    REQUIRE(free_cell_east(world, wx + 1, c->y + 4, vx));
    world.add_combat_unit(vx, c->y + 4, 0, transport_stats(2, 2));  // unit 4
    world.clear_selection();
    world.units()[3].selected = true;
    world.order_enter_transport(4);
    REQUIRE(world.units()[3].enter_target == 4);
    world.order_move_selected(tx, c->y + 6);                     // unit 3 still selected
    CHECK(world.units()[3].enter_target == -1);                  // superseded
}

// [CB-1 off-map invariant] A carried unit is off-map/invisible to census --
// ai_fill_trigger_view's own-unit count must exclude cargo_of>=0 units the
// same way it already excludes corpses (!alive / die_time>=0). Regression
// for the widening: mark the ONLY roster-type-0 unit as carried (cargo_of/
// cargo set directly -- order_enter_transport/order_unload_selected are
// house-0-only player orders and can't drive an AI-house [house=1] unit),
// key a trigger on kCondOwnerOwns unit-count >= 1, and confirm it never
// fires while the unit is carried, then DOES fire once un-carried (the same
// unit, now genuinely on-map) -- proving the harness actually exercises the
// census path rather than being vacuously green.
TEST_CASE("World: ai_fill_trigger_view excludes a carried unit from the own-unit census [CB-1]") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(map);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    world.add_building(0, c->x, c->y, 1, 1, /*house=*/1, 1000, 0, 0.0, true);  // AI conyard
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x + 3, c->y, tx));
    world.add_combat_unit(tx, c->y, /*house=*/1, transport_stats(2, 2));  // unit 0: transport
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 3, c->y, gx));
    ra2engine::UnitCombat uc;
    uc.strength = 100;
    uc.image = "AITEST";                 // matches ai2_cfg's unit_roster[0]
    uc.damage = 10; uc.rof = 15; uc.range = 4.0;
    uc.verses = std::vector<int>(11, 100);
    world.add_combat_unit(gx, c->y, /*house=*/1, uc);   // unit 1: the ONLY
                                                        // roster-type-0 unit
    // Mark unit 1 carried directly -- exercises the exact fields the census
    // fix guards, without needing the (house-0-only) player boarding orders.
    world.units()[1].cargo_of = 0;
    world.units()[0].cargo.push_back(1);
    ra2engine::AiHouseConfig cfg = ai3_cfg(/*triggers_enabled=*/true);
    // Rekey TRG1 off kCondEnemyOwns/building onto kCondOwnerOwns/unit-type-0
    // (>= 1 of the roster's OWN unit type) -- the exact slice this fix guards.
    cfg.team.aimd.triggers[0].condition_type = ra2engine::kCondOwnerOwns;
    cfg.team.aimd.triggers[0].condition_object =
        ra2engine::TriggerObject{ra2engine::TechnoKind::Unit, 0};
    cfg.team.aimd.triggers[0].comparator =
        ra2engine::TriggerComparator{1, ra2engine::kCompGreaterEq};
    world.enable_ai_house(1, cfg);
    REQUIRE(world.ai_trigger_state(1) != nullptr);
    const double dt = 1.0 / 15.0;
    for (int i = 0; i < 900; ++i) world.step(dt);   // many 1 s check passes
    CHECK(world.ai_trigger_state(1)->triggers_fired == 0);   // carried -> NOT
                                                              // counted -> never
                                                              // eligible
    // Control: un-carry the SAME unit and confirm the trigger now fires --
    // proving the census (and this test) is live, not toothless.
    world.units()[1].cargo_of = -1;
    world.units()[0].cargo.clear();
    for (int i = 0; i < 900 && world.ai_trigger_state(1)->triggers_fired < 1; ++i)
        world.step(dt);
    CHECK(world.ai_trigger_state(1)->triggers_fired == 1);   // now counted
}
