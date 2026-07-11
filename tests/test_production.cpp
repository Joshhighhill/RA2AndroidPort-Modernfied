#include "doctest.h"
#include "../engine/production.hpp"
using namespace ra2engine;

static std::vector<BuildingTypeDef> roster() {
    std::vector<BuildingTypeDef> t(3);
    t[0].id = "GACNST"; t[0].conyard = true; t[0].cost = 3000; t[0].tech_level = -1;
    t[1].id = "GAPOWR"; t[1].cost = 800; t[1].power = 200; t[1].tech_level = 1;
    t[1].prerequisite = {"GACNST"};
    t[2].id = "GAPILE"; t[2].cost = 500; t[2].power = -10; t[2].tech_level = 2;
    t[2].prerequisite = {"POWER", "GACNST"};
    return t;
}
static Building live(int type, int house = 0) {
    Building b; b.type = type; b.house = house; b.alive = true;
    b.state = Building::State::Idle; b.hp = b.max_hp = 100; return b;
}
static const std::vector<std::pair<std::string, std::vector<std::string>>> kGroups =
    {{"POWER", {"GAPOWR", "NAPOWR"}}};

TEST_CASE("build_frames: BuildSpeed formula [RE: construction.md §7]") {
    // Pins recovered from the binary (FUN_00711280): ftol(Cost*BuildSpeed*0.9).
    // Operation-order sensitive: the algebraically equal /1000*900 rounds to
    // 503/1889 under IEEE 754 instead of the binary's 504/1890.
    CHECK(build_frames(1000, 0.7) == 630);   // 0.7 min * 900
    CHECK(build_frames(800, 0.7) == 504);    // GAPOWR pin (Task-1 hunt)
    CHECK(build_frames(500, 0.7) == 315);
    CHECK(build_frames(2000, 0.7) == 1260);
    CHECK(build_frames(3000, 0.7) == 1890);  // GACNST pin (Task-1 hunt)
}

TEST_CASE("power_totals + build_speed_multiplier [RE-GATE: construction.md §8]") {
    auto t = roster();
    std::vector<Building> bs = {live(1), live(2)};       // +200, -10
    auto p = power_totals(bs, t, 0);
    CHECK(p.output == 200); CHECK(p.drain == 10);
    CHECK(build_speed_multiplier(p, 1.0) == doctest::Approx(1.0));
    bs[0].state = Building::State::Buildup;              // powerplant not up yet
    p = power_totals(bs, t, 0);
    CHECK(p.output == 0); CHECK(p.drain == 10);
    // f = 0/10 = 0 -> speed 1-1*(1-0) = 0 -> max(.5) -> min(.8) = 0.5 exactly
    CHECK(build_speed_multiplier(p, 1.0) == doctest::Approx(kMinLowPowerSpeed));
    // partial shortage: out 90 / drain 100 -> 1-1*(1-.9) = .9 -> capped at .8
    PowerTotals part; part.output = 90; part.drain = 100;
    CHECK(build_speed_multiplier(part, 1.0) == doctest::Approx(kMaxLowPowerSpeed));
    // heavy modifier: out 90/drain 100, mod 4 -> 1-4*.1 = .6 (within [.5,.8])
    CHECK(build_speed_multiplier(part, 4.0) == doctest::Approx(0.6));
    kill_building(bs[1]);
    p = power_totals(bs, t, 0);
    CHECK(p.drain == 0);                                 // dead building drains nothing
    // enemy buildings don't count
    std::vector<Building> enemy = {live(1, 1)};
    CHECK(power_totals(enemy, t, 0).output == 0);
}

TEST_CASE("prerequisites_met: plain ids, groups, Buildup doesn't count") {
    auto t = roster();
    std::vector<Building> bs = {live(0)};                // ConYard only
    CHECK(prerequisites_met({"GACNST"}, bs, t, 0, kGroups));
    CHECK_FALSE(prerequisites_met({"POWER", "GACNST"}, bs, t, 0, kGroups));
    bs.push_back(live(1));                               // + power plant
    CHECK(prerequisites_met({"POWER", "GACNST"}, bs, t, 0, kGroups));
    bs[1].state = Building::State::Buildup;              // not finished
    CHECK_FALSE(prerequisites_met({"POWER"}, bs, t, 0, kGroups));
    CHECK_FALSE(prerequisites_met({"PROC"}, bs, t, 0, kGroups));   // unknown group
}

TEST_CASE("can_start: slot, conyard, tech level") {
    auto t = roster();
    std::vector<Building> bs = {live(0)};
    ProductionState ps; ps.credits = 10000;
    CHECK(can_start(ps, 1, t, bs, 0, kGroups, true));
    CHECK_FALSE(can_start(ps, 1, t, bs, 0, kGroups, false));   // no ConYard
    CHECK_FALSE(can_start(ps, 0, t, bs, 0, kGroups, true));    // TechLevel -1
    CHECK_FALSE(can_start(ps, 2, t, bs, 0, kGroups, true));    // POWER missing
    start(ps, 1, t, 0.7);
    CHECK_FALSE(can_start(ps, 1, t, bs, 0, kGroups, true));    // slot busy
}

TEST_CASE("tick: 54-step economy [RE-GATE: construction.md §10]") {
    auto t = roster();
    ProductionState ps; ps.credits = 10000;
    start(ps, 1, t, 0.7);                    // GAPOWR: 504 frames, 800 credits
    CHECK(ps.slot.total_frames == 504);
    CHECK(ps.slot.step_ticks == 9);          // trunc(504/54)
    // 27 stages = half the steps = 27*9 = 243 frames = 16.2 s
    tick(ps, 243.0 / 15.0, 1.0, t);
    CHECK(ps.slot.stage == 27);
    CHECK(ps.credits == doctest::Approx(10000 - 400.0));   // equal cost/54 chunks
    CHECK_FALSE(ps.slot.ready);
    tick(ps, 504.0, 1.0, t);                 // way past the end
    CHECK(ps.slot.ready);
    CHECK(ps.slot.stage == kProductionSteps);
    CHECK(ps.credits == doctest::Approx(9200.0));           // exactly cost deducted
    placed(ps);
    CHECK(ps.slot.type == -1);

    // stall: 100 credits fund 6 chunks of 800/54 (= 88.9); the 7th (14.8) is
    // unaffordable -> stalls with ~11.1 left, no progress past stage 6
    ps.credits = 100;
    start(ps, 1, t, 0.7);
    tick(ps, 1000.0, 1.0, t);
    CHECK(ps.slot.stage == 6);
    CHECK(ps.credits == doctest::Approx(100.0 - 800.0 * 6 / 54).epsilon(0.001));
    CHECK_FALSE(ps.slot.ready);
    ps.credits += 5000;                       // funds arrive -> resumes
    tick(ps, 1000.0, 1.0, t);
    CHECK(ps.slot.ready);
    CHECK(ps.credits == doctest::Approx(100.0 + 5000.0 - 800.0));  // total spend == cost
    placed(ps);

    // stall never produces a catch-up burst: broke for a long time, then
    // funded -> at most ONE stage per step_ticks afterwards
    ProductionState nb; nb.credits = 0;
    start(nb, 1, t, 0.7);
    tick(nb, 100.0, 1.0, t);                  // long broke stretch
    CHECK(nb.slot.stage == 0);
    nb.credits = 10000;
    tick(nb, 9.0 / 15.0, 1.0, t);             // exactly one step_ticks worth
    CHECK(nb.slot.stage == 1);                // NOT a burst to stage 11

    // cancel refunds spent
    ProductionState c; c.credits = 1000;
    start(c, 2, t, 0.7);                      // GAPILE: 315 frames, step_ticks 5
    CHECK(c.slot.step_ticks == 5);            // trunc(315/54)
    tick(c, 90.0 / 15.0, 1.0, t);             // 90 frames = 18 stages
    CHECK(c.slot.stage == 18);
    CHECK(c.credits < 1000.0);
    cancel(c);
    CHECK(c.credits == doctest::Approx(1000.0));
    CHECK(c.slot.type == -1);

    // half-speed multiplier halves stage cadence
    ProductionState s; s.credits = 10000;
    start(s, 1, t, 0.7);                      // step_ticks 9
    tick(s, 1.0, 0.5, t);                     // 7.5 frames < 9 -> no stage yet
    CHECK(s.slot.stage == 0);
    tick(s, 1.0, 0.5, t);                     // 15 frames -> one stage
    CHECK(s.slot.stage == 1);
}
