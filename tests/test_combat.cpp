#include "doctest.h"

#include <cstdint>
#include <vector>

#include "combat.hpp"
#include "occupancy.hpp"
#include "world.hpp"

using ra2engine::Occupancy;
using ra2engine::Unit;

namespace {

std::vector<uint8_t> open_grid(int w, int h) { return std::vector<uint8_t>((size_t)w * h, 1); }

// Builds a combat-ready soldier at cell (x,y): GI (house 0, M60: 15 dmg / 20
// ROF frames / 4.0 range, armor "none") or Conscript (house 1, M1Carbine:
// 15 dmg / 25 ROF frames / 4.0 range, armor "flak") -- ground truth from
// docs/re/combat.md / the Combat Phase's extracted rulesmd values (both
// Strength=125, SA warhead). Claims its cell in `occ`, mirroring
// test_movement.cpp's make_mover helper.
Unit make_soldier(int x, int y, int house, int idx, Occupancy& occ) {
    Unit u;
    u.cell_x = x;
    u.cell_y = y;
    u.fx = x;
    u.fy = y;
    u.house = house;
    u.hp = u.max_hp = 125;
    u.w_verses = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};  // kSAVerses
    u.w_damage = 15;
    u.w_range = 4.0;
    if (house == 0) {
        u.armor_ix = 0;  // none
        u.w_rof = 20;
    } else {
        u.armor_ix = 1;  // flak
        u.w_rof = 25;
    }
    occ.set(x, y, idx);
    return u;
}

}  // namespace

TEST_CASE("armor_index maps the 11 class names, unknown -> none") {
    CHECK(ra2engine::armor_index("none") == 0);
    CHECK(ra2engine::armor_index("flak") == 1);
    CHECK(ra2engine::armor_index("plate") == 2);
    CHECK(ra2engine::armor_index("light") == 3);
    CHECK(ra2engine::armor_index("medium") == 4);
    CHECK(ra2engine::armor_index("heavy") == 5);
    CHECK(ra2engine::armor_index("wood") == 6);
    CHECK(ra2engine::armor_index("steel") == 7);
    CHECK(ra2engine::armor_index("concrete") == 8);
    CHECK(ra2engine::armor_index("special_1") == 9);
    CHECK(ra2engine::armor_index("special_2") == 10);
    CHECK(ra2engine::armor_index("Flak") == 1);      // case-insensitive (ini convention)
    CHECK(ra2engine::armor_index("weird") == 0);
}

TEST_CASE("damage_vs: hand-computed vectors from the real SA warhead") {
    const std::vector<int> sa = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};
    CHECK(ra2engine::damage_vs(15, sa, 0) == 15);  // M1Carbine vs GI (none)
    CHECK(ra2engine::damage_vs(15, sa, 1) == 12);  // M60 vs Conscript (flak): 15*80/100
    CHECK(ra2engine::damage_vs(15, sa, 6) == 11);  // 15*75/100 = 11.25 -> trunc (Task 1 confirms)
    CHECK(ra2engine::damage_vs(15, {}, 1) == 15);  // absent verses -> 100% (defensive)
}

TEST_CASE("step_combat: in-range hostile is auto-acquired and killed on exact ROF cadence") {
    // GI (house 0) at (0,0); Conscript (house 1) at (3,0): distance 3 cells <= range 4.
    // M60: 12 dmg/shot vs flak; 125 hp -> 11 shots; ROF 20 frames = 4/3 s between shots.
    // Disarm the Conscript (w_damage=0) so only one direction fires.
    auto p = open_grid(10, 1);
    Occupancy occ(10, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(3, 0, 1, 1, occ));
    units[1].w_damage = 0;

    const double dt = 1.0 / 60.0;
    double t = 0;
    while (units[1].die_time < 0 && t < 30.0) { ra2engine::step_combat(units, occ, dt); t += dt; }
    REQUIRE(units[1].die_time >= 0);          // died
    // 11th shot fires after 10 full cooldowns: t = 10 * (20/15) = 13.333s (+1 frame slop)
    CHECK(t >= 10.0 * (20.0 / 15.0) - 2 * dt);
    CHECK(t <= 10.0 * (20.0 / 15.0) + 2 * dt);
    CHECK(units[1].hp <= 0);
}

TEST_CASE("step_combat: out-of-range hostile is NOT auto-acquired; explicit order requests pursuit") {
    // Conscript at (8,0): distance 8 > range 4 -> no auto-acquire, no fire.
    auto p = open_grid(10, 1);
    Occupancy occ(10, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(8, 0, 1, 1, occ));

    auto reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    CHECK(units[0].target == -1);
    CHECK(reqs.empty());
    // Explicit attack order pursues: target set + MoveRequest toward the victim.
    units[0].target = 1; units[0].attack_order = true;
    reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    REQUIRE(reqs.size() == 1);
    CHECK(reqs[0].unit == 0);
    CHECK(reqs[0].goal_x == 8);
}

TEST_CASE("step_combat: return fire — idle victim retaliates against its attacker") {
    // GI fires first (in range, victim idle, victim.w_damage kept): once the
    // first shot lands the Conscript is no longer idle and must retaliate.
    auto p = open_grid(10, 1);
    Occupancy occ(10, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(3, 0, 1, 1, occ));

    const double dt = 1.0 / 60.0;
    double t = 0;
    // Run until the first GI shot lands.
    while (units[1].hp >= 125 && t < 5.0) { ra2engine::step_combat(units, occ, dt); t += dt; }
    REQUIRE(units[1].hp < 125);
    // After the first GI shot lands, the Conscript's target must be the GI.
    CHECK(units[1].target == 0);
}

TEST_CASE("step_combat: auto-acquired target dropped beyond guard stray; anchor return requested") {
    // Auto-acquire at range, then teleport the victim beyond range+kGuardStray:
    // target dropped, MoveRequest back to the guard anchor.
    auto p = open_grid(10, 1);
    Occupancy occ(10, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(3, 0, 1, 1, occ));
    units[1].w_damage = 0;  // isolate: only the GI's own targeting is under test

    auto reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    REQUIRE(units[0].target == 1);
    CHECK(units[0].guard_x == 0);
    CHECK(units[0].guard_y == 0);

    // Teleport the Conscript to (7,0): 7 cells from the GI's anchor (0,0),
    // beyond weapon range (4) + kGuardStray (2) = 6.
    units[1].cell_x = 7;
    units[1].fx = 7.0;
    reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    CHECK(units[0].target == -1);
    CHECK(reqs.empty());  // the GI never left its own anchor, so no return move is needed.
}

TEST_CASE("step_combat: death invariants — dying then tombstoned, occupancy freed, no dangling targets") {
    // 3 units: two GIs both targeting one Conscript; kill it.
    auto p = open_grid(10, 2);
    Occupancy occ(10, 2);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(0, 1, 0, 1, occ));
    units.push_back(make_soldier(3, 0, 1, 2, occ));
    units[2].w_damage = 0;  // isolate: the Conscript never damages either GI back

    const double dt = 1.0 / 60.0;
    double t = 0;
    while (units[2].die_time < 0 && t < 20.0) { ra2engine::step_combat(units, occ, dt); t += dt; }
    REQUIRE(units[2].die_time >= 0);
    CHECK(occ.at(units[2].cell_x, units[2].cell_y) == ra2engine::Occupancy::kEmpty);
    // Both attackers' targets cleared by validation on the next step.
    ra2engine::step_combat(units, occ, 1.0 / 60.0);
    CHECK(units[0].target == -1);
    CHECK(units[1].target == -1);
    // Die anim completes -> tombstone.
    for (double t = 0; t < 1.5; t += 1.0 / 60.0) ra2engine::step_combat(units, occ, 1.0 / 60.0);
    CHECK_FALSE(units[2].alive);
}
