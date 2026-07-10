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
    // approach == true: the goal is the target's OWN cell; World (caller)
    // resolves a distinct firing position near it rather than pathing the
    // unit exactly there (docs/re/infantry-anim.md §7).
    units[0].target = 1; units[0].attack_order = true;
    reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    REQUIRE(reqs.size() == 1);
    CHECK(reqs[0].unit == 0);
    CHECK(reqs[0].goal_x == 8);
    CHECK(reqs[0].goal_y == 0);
    CHECK(reqs[0].approach == true);
}

TEST_CASE("step_combat: attack_order approach — live path within weapon range of the target needs no re-request") {
    // GI (idx0) pursuing a Conscript (idx1) that is out of firing range, but
    // the GI already has a LIVE path whose back() sits within weapon range
    // (4.0 cells) of the target's CURRENT position -- even though back() !=
    // the target's own cell. No fresh MoveRequest should be emitted: the
    // approach goal is still a valid firing position (range-based dedup,
    // replacing the old exact-cell-equality dedup).
    auto p = open_grid(20, 1);
    Occupancy occ(20, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(10, 0, 1, 1, occ));  // dist 10 > range 4 -> out of range
    units[0].target = 1;
    units[0].attack_order = true;
    // Live path already headed toward a firing position 3 cells short of the
    // target's cell (dist(7,0 -> 10,0) == 3 <= range 4).
    units[0].path = {{7, 0}};
    units[0].path_idx = 0;
    REQUIRE(units[0].moving());

    auto reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    CHECK(reqs.empty());
}

TEST_CASE("step_combat: attack_order approach — target moving out of the live path's range triggers a fresh re-request") {
    // Same live-path setup as above, but the target has since moved further
    // away: path.back() (7,0) is now 6 cells from it (> range 4) -> the
    // approach goal has stopped being a firing position, so a fresh request
    // is emitted, aimed at the target's (new) cell.
    auto p = open_grid(20, 1);
    Occupancy occ(20, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(10, 0, 1, 1, occ));
    units[0].target = 1;
    units[0].attack_order = true;
    units[0].path = {{7, 0}};
    units[0].path_idx = 0;

    units[1].cell_x = 13;
    units[1].fx = 13.0;

    auto reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    REQUIRE(reqs.size() == 1);
    CHECK(reqs[0].unit == 0);
    CHECK(reqs[0].approach == true);
    CHECK(reqs[0].goal_x == 13);  // fresh request aimed at the target's cell
    CHECK(reqs[0].goal_y == 0);
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

TEST_CASE("step_combat: leash drop off-anchor emits a return-to-anchor move request") {
    // GI auto-acquires Conscript at range; GI drifts off anchor while target
    // remains near the anchor, triggering leash drop. GI must emit a MoveRequest
    // back to the ORIGINAL anchor, not the target's cell.
    auto p = open_grid(20, 1);
    Occupancy occ(20, 1);
    std::vector<Unit> units;
    units.push_back(make_soldier(0, 0, 0, 0, occ));
    units.push_back(make_soldier(3, 0, 1, 1, occ));
    units[1].w_damage = 0;  // isolate: only the GI's targeting is under test

    auto reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    REQUIRE(units[0].target == 1);
    CHECK(units[0].guard_x == 0);
    CHECK(units[0].guard_y == 0);

    // Simulate the GI having drifted to (8,0): 8 cells from anchor (0,0),
    // beyond weapon range (4) + kGuardStray (2) = 6; also > 4 cells from
    // Conscript at (3,0) to ensure out-of-range firing check.
    occ.release_all_of(0, -1, -1);
    units[0].cell_x = 8;
    units[0].fx = 8.0;
    units[0].fy = 0.0;
    occ.set(8, 0, 0);

    reqs = ra2engine::step_combat(units, occ, 1.0 / 60.0);
    // Target should be dropped due to leash violation (GI strayed too far).
    CHECK(units[0].target == -1);
    // Exactly one MoveRequest back to the ORIGINAL anchor.
    REQUIRE(reqs.size() == 1);
    CHECK(reqs[0].unit == 0);
    CHECK(reqs[0].goal_x == 0);  // anchor x, not target's cell_x
    CHECK(reqs[0].goal_y == 0);  // anchor y, not target's cell_y
    CHECK(reqs[0].approach == false);  // guard-return: move exactly there, no firing-position resolution
}
