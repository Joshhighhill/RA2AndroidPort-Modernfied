// tests/test_vehicle.cpp
#include "doctest.h"
#include "world.hpp"
#include "combat.hpp"
#include <cmath>

using ra2engine::Unit;
using ra2engine::turn_toward;
using ra2engine::facing_u16_from_delta;

TEST_CASE("vehicle: turn_toward takes the shortest arc with wraparound") {
    CHECK(turn_toward(0.0, 100.0, 300.0) == doctest::Approx(100.0));     // within step: clamp at target
    CHECK(turn_toward(0.0, 1000.0, 300.0) == doctest::Approx(300.0));    // step toward, positive arc
    CHECK(turn_toward(1000.0, 0.0, 300.0) == doctest::Approx(700.0));    // step back, negative arc
    // wraparound: 65000 -> 200 is a +736 arc (through 0), not -64800
    CHECK(turn_toward(65000.0, 200.0, 300.0) == doctest::Approx(65300.0));  // steps toward the wrap
    CHECK(turn_toward(65400.0, 200.0, 400.0) == doctest::Approx(200.0));    // arc 336 <= step 400: reaches target through 0
    // result always normalized to [0, 65536)
    CHECK(turn_toward(65500.0, 300.0, 100.0) == doctest::Approx(64.0));  // 65500 + 100 = 65600 -> 64
}

TEST_CASE("vehicle: facing_u16_from_delta agrees with the 8-block convention") {
    // facing_from_delta (infantry) maps each direction to block 0..7
    // (N=0 counterclockwise-on-screen to NE=7, spawn S=4). The continuous
    // facing must land inside block*8192 +/- 4096 of the SAME block.
    const double dirs[8][2] = {
        // the same 8 cell-space rays test_world.cpp uses for facing_from_delta
        {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1},
    };
    for (int k = 0; k < 8; ++k) {
        const int block = ra2engine::facing_from_delta(dirs[k][0], dirs[k][1]);
        REQUIRE(block >= 0);
        const double f = facing_u16_from_delta(dirs[k][0], dirs[k][1]);
        const double center = block * 8192.0;
        double d = std::fmod(f - center + 65536.0 * 2, 65536.0);
        if (d > 32768.0) d -= 65536.0;
        CHECK(std::abs(d) <= 4096.0);
    }
}

namespace {
std::vector<std::uint8_t> open_grid(int w, int h) {
    return std::vector<std::uint8_t>(static_cast<size_t>(w) * h, 1);
}
Unit tank_at(int x, int y) {
    Unit u;
    u.cell_x = x; u.cell_y = y; u.fx = x; u.fy = y;
    u.is_vehicle = true;
    u.rot_u16_per_sec = 5 * ra2engine::kRotU16PerRotPerSec;  // ROT=5
    u.speed_cps = 4.5;
    return u;
}
}  // namespace

TEST_CASE("vehicle: turns in place before rolling, then moves at its own speed") {
    std::vector<Unit> units = {tank_at(5, 5)};
    // Spawn faces S (32768); path due N-ish: goal (4,4) -> want facing 0-block.
    units[0].path = {{5, 5}, {4, 4}};
    units[0].path_idx = 1;
    ra2engine::Occupancy occ(16, 16);
    occ.set(5, 5, 0);
    auto grid = open_grid(16, 16);
    const ra2engine::AnimTable anims;

    // First small step: facing is ~180 degrees off -> must rotate, not move.
    ra2engine::step_units(units, grid, 16, 16, occ, 1.0 / 60.0, 3.0, anims);
    CHECK(units[0].fx == doctest::Approx(5.0));
    CHECK(units[0].fy == doctest::Approx(5.0));
    CHECK(units[0].body_facing != doctest::Approx(32768.0));  // it turned

    // Run 2 sim-seconds: ROT=5 covers 180 degrees in ~1.7s, then it rolls.
    for (int i = 0; i < 120; ++i)
        ra2engine::step_units(units, grid, 16, 16, occ, 1.0 / 60.0, 3.0, anims);
    CHECK((units[0].fx != doctest::Approx(5.0) || units[0].fy != doctest::Approx(5.0)));

    // Per-unit speed: a fresh tank on a long straight path outruns move_cps.
    std::vector<Unit> fast = {tank_at(1, 8)}, slow = {tank_at(1, 8)};
    fast[0].body_facing = facing_u16_from_delta(1.0, 0.0);   // pre-aligned
    slow[0].body_facing = facing_u16_from_delta(1.0, 0.0);
    slow[0].is_vehicle = false; slow[0].speed_cps = 0.0;     // infantry control
    for (auto* v : {&fast, &slow}) {
        (*v)[0].path.clear();
        for (int x = 1; x <= 14; ++x) (*v)[0].path.push_back({x, 8});
        (*v)[0].path_idx = 1;
    }
    ra2engine::Occupancy o2(16, 16), o3(16, 16);
    o2.set(1, 8, 0); o3.set(1, 8, 0);
    for (int i = 0; i < 60; ++i) {
        ra2engine::step_units(fast, grid, 16, 16, o2, 1.0 / 60.0, 3.0, anims);
        ra2engine::step_units(slow, grid, 16, 16, o3, 1.0 / 60.0, 3.0, anims);
    }
    CHECK(fast[0].fx > slow[0].fx);  // 4.5 cps vs 3.0 cps after 1s
}

TEST_CASE("vehicle: turret slews to target and returns to body when disengaged") {
    std::vector<Unit> units = {tank_at(5, 5), tank_at(9, 9)};
    units[1].is_vehicle = false;  // any target works
    units[0].target = 1;
    units[1].hp = units[1].max_hp = 100;
    units[0].hp = units[0].max_hp = 100;
    const double want = facing_u16_from_delta(units[1].fx - units[0].fx,
                                              units[1].fy - units[0].fy);
    for (int i = 0; i < 300; ++i) ra2engine::step_turrets(units, 1.0 / 60.0);
    CHECK(units[0].turret_facing == doctest::Approx(want));
    units[0].target = -1;
    for (int i = 0; i < 300; ++i) ra2engine::step_turrets(units, 1.0 / 60.0);
    CHECK(units[0].turret_facing == doctest::Approx(units[0].body_facing));
}

TEST_CASE("vehicle: fire waits for turret aim; infantry does not") {
    // Two hostile units in range of each other. The vehicle's turret starts
    // pointing away; step_combat alone (no step_turrets) must never let it
    // fire, then after slewing it fires on the next combat step.
    std::vector<Unit> units = {tank_at(5, 5), tank_at(7, 5)};
    for (auto& u : units) { u.hp = u.max_hp = 400; u.w_damage = 90; u.w_rof = 65;
                            u.w_range = 5.75; u.w_verses = {100}; u.alive = true; }
    units[0].house = 0; units[1].house = 1;
    units[0].target = 1; units[0].attack_order = true;
    units[1].is_vehicle = false;   // control: infantry-rules unit fires immediately
    units[1].target = 0; units[1].attack_order = true;
    units[0].turret_facing = turn_toward(facing_u16_from_delta(1.0, 0.0), 0.0, 20000.0);  // way off
    ra2engine::Occupancy occ(16, 16);
    occ.set(5, 5, 0); occ.set(7, 5, 1);

    ra2engine::step_combat(units, occ, 1.0 / 15.0);
    CHECK(units[1].hp == 400);          // vehicle held fire (not aimed)
    CHECK(units[0].hp < 400);           // infantry-rules unit fired

    // Aim EXACTLY (tolerance 0): use the same delta step_combat computes,
    // (t.fx - u.fx, t.fy - u.fy) = (2, 0), so the doubles compare equal.
    units[0].turret_facing = facing_u16_from_delta(2.0, 0.0);
    ra2engine::step_combat(units, occ, 1.0 / 15.0);
    CHECK(units[1].hp < 400);           // vehicle fired once aimed
}
