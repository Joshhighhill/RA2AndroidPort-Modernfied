// Wave-7 AI per-unit rules (engine/ai_unit.cpp): W7-01-08 auto-deploy,
// W7-04-18 ToProtect, W7-09-17 rules-id unit typing.
#include "doctest.h"

#include <vector>

#include "asset_fixture.hpp"
#include "world.hpp"

namespace {

ra2engine::UnitCombat w7_gi(bool deployer) {
    ra2engine::UnitCombat c;
    c.strength = 125;
    c.armor = "none";
    c.image = "GI";
    c.damage = 15;
    c.rof = 20;
    c.range = 4.0;
    c.verses = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};
    c.tg.rules_id = "E1";
    if (deployer) {
        c.deploy_fire = true;
        c.deploy_fire_slot = 1;
        c.secondary.valid = true;
        c.secondary.id = "Para";
        c.secondary.damage = 15;
        c.secondary.rof = 10;
        c.secondary.range = 5.0;
        c.secondary.verses = c.verses;
    }
    return c;
}

bool free_cell(const ra2engine::World& w, int x, int y) {
    return w.walkable(x, y) && w.unit_at_cell(x, y) < 0 && w.building_at_cell(x, y) < 0;
}

// A free walkable cell in the ring around (cx,cy) at Chebyshev radius >= r0.
bool ring_cell(const ra2engine::World& w, int cx, int cy, int r0, int& ox, int& oy) {
    for (int r = r0; r <= r0 + 10; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                if (free_cell(w, cx + dx, cy + dy)) { ox = cx + dx; oy = cy + dy; return true; }
            }
    return false;
}

void three_houses(ra2engine::World& world) {
    for (int h = 0; h < 3; ++h) {
        ra2engine::World::HouseRec r;
        r.side = h == 1 ? ra2engine::World::Side::Soviet : ra2engine::World::Side::Allied;
        r.color_ix = h;
        world.set_house(h, r);
    }
}

}  // namespace

// [RE: InfantryClass FUN_00521050 @0x52128C, rulesmd [General]
// AIAutoDeployFrameDelay=15,25,100]: a computer house's GI standing in Guard
// deploys once the delay has run; a house without the rule (or a human one)
// never does; a move order packs it up again.
TEST_CASE("W7-01-08: a computer GI auto-deploys after AIAutoDeployFrameDelay frames in Guard") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(ra2fixture::mapsmd03_map());
    three_houses(world);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(ra2fixture::mapsmd03_map());
    REQUIRE(c != nullptr);
    ra2engine::AiHouseConfig cfg;
    cfg.auto_deploy_frames = 15;            // the Hard (house+0x184 = 0) entry
    world.enable_ai_house(1, cfg);
    ra2engine::AiHouseConfig off;          // auto_deploy_frames = -1: no rule
    world.enable_ai_house(2, off);
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x0 = 0, y0 = 0;
    REQUIRE(ring_cell(world, c->x, c->y, 0, x1, y1));
    world.add_combat_unit(x1, y1, 1, w7_gi(true));
    REQUIRE(ring_cell(world, c->x, c->y, 3, x2, y2));
    world.add_combat_unit(x2, y2, 2, w7_gi(true));
    world.set_pair_alliance(1, 2, true);
    REQUIRE(ring_cell(world, c->x + 40, c->y + 40, 0, x0, y0));
    world.add_combat_unit(x0, y0, 0, w7_gi(true));   // human house: never auto-deploys
    for (int f = 0; f < 10; ++f) world.step(1.0 / 15.0);
    CHECK_FALSE(world.units()[0].deployed);          // the delay has not run yet
    for (int f = 0; f < 20; ++f) world.step(1.0 / 15.0);
    CHECK(world.units()[0].deployed);                 // computer house, rule armed
    CHECK_FALSE(world.units()[1].deployed);           // rule off
    CHECK_FALSE(world.units()[2].deployed);           // human house
    // A move undeploys (FUN_0051F370) and the guard clock restarts on arrival.
    int mx = 0, my = 0;
    REQUIRE(ring_cell(world, x1, y1, 2, mx, my));
    world.order_unit_move(0, mx, my);
    CHECK_FALSE(world.units()[0].deployed);
    for (int f = 0; f < 600 && world.units()[0].moving(); ++f) world.step(1.0 / 15.0);
    CHECK_FALSE(world.units()[0].moving());
    for (int f = 0; f < 20; ++f) world.step(1.0 / 15.0);
    CHECK(world.units()[0].deployed);                 // re-deployed at the new post
}

// [RE: FUN_00522070]: in Attack, a computer GI deploys once its target is inside
// the deploy weapon's range, and undeploys (to pursue) when it is not.
TEST_CASE("W7-01-08: a computer GI deploys on an in-range attack target and undeploys for a far one") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(ra2fixture::mapsmd03_map());
    three_houses(world);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(ra2fixture::mapsmd03_map());
    REQUIRE(c != nullptr);
    ra2engine::AiHouseConfig cfg;
    cfg.auto_deploy_frames = 100000;   // keep the Guard branch out of the way
    world.enable_ai_house(1, cfg);
    int x1 = 0, y1 = 0;
    REQUIRE(ring_cell(world, c->x, c->y, 0, x1, y1));
    world.add_combat_unit(x1, y1, 1, w7_gi(true));
    int tx = 0, ty = 0;
    REQUIRE(ring_cell(world, x1, y1, 3, tx, ty));     // 3 cells: inside Para's 5
    ra2engine::UnitCombat tgt = w7_gi(false);
    tgt.strength = 100000;                            // survives the test
    tgt.damage = 0;
    world.add_combat_unit(tx, ty, 0, tgt);
    world.order_unit_attack(0, 1);
    world.step(1.0 / 15.0);
    CHECK(world.units()[0].deployed);
}

// [RE: FUN_00707500 via ReceiveDamage @0x701C43]: a hit ToProtect unit of a
// computer house calls idle, unteamed units of its house onto the attacker.
TEST_CASE("W7-04-18: an attacked ToProtect unit calls idle units onto its attacker") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(ra2fixture::mapsmd03_map());
    three_houses(world);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(ra2fixture::mapsmd03_map());
    REQUIRE(c != nullptr);
    ra2engine::AiHouseConfig cfg;
    cfg.team.unit_roster.resize(2);
    cfg.team.unit_roster[0].id = "HARV";
    cfg.team.unit_roster[1].id = "E2";
    cfg.roster_to_protect = {1, 0};
    world.enable_ai_house(1, cfg);
    int vx = 0, vy = 0, ax = 0, ay = 0, hx = 0, hy = 0;
    REQUIRE(ring_cell(world, c->x, c->y, 0, vx, vy));
    ra2engine::UnitCombat victim = w7_gi(false);
    victim.tg.rules_id = "HARV";
    victim.strength = 100000;
    victim.damage = 0;
    world.add_combat_unit(vx, vy, 1, victim);                 // 0: the "miner"
    REQUIRE(ring_cell(world, vx, vy, 3, ax, ay));
    ra2engine::UnitCombat attacker = w7_gi(false);
    attacker.strength = 100000;
    world.add_combat_unit(ax, ay, 0, attacker);               // 1: the enemy
    REQUIRE(ring_cell(world, vx, vy, 9, hx, hy));
    ra2engine::UnitCombat helper = w7_gi(false);
    helper.tg.rules_id = "E2";
    helper.range = 1.0;                                       // cannot see the attacker itself
    world.add_combat_unit(hx, hy, 1, helper);                 // 2: an idle defender
    world.order_unit_attack(1, 0);
    bool called = false;
    for (int f = 0; f < 120 && !called; ++f) {
        world.step(1.0 / 15.0);
        called = world.units()[2].target == 1 && world.units()[2].attack_order;
    }
    CHECK(world.units()[0].hp < 100000);   // the miner was hit
    CHECK(called);                          // and its house answered
}

// W7-09-17: AI unit typing goes by the rules id, not the shared image.
TEST_CASE("W7-09-17: a civilian TRUCKA is not typed as the AI's DTRUCK") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("skipped: real map assets not found"); return; }
    ra2engine::World world(ra2fixture::mapsmd03_map());
    three_houses(world);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(ra2fixture::mapsmd03_map());
    REQUIRE(c != nullptr);
    ra2engine::AiHouseConfig cfg;
    cfg.team.unit_roster.resize(1);
    cfg.team.unit_roster[0].id = "DTRUCK";
    cfg.team.unit_roster[0].combat.image = "TRUCKA";
    cfg.roster_to_protect = {1};   // DTRUCK "ToProtect" here only to observe typing
    world.enable_ai_house(1, cfg);
    int vx = 0, vy = 0, ax = 0, ay = 0;
    REQUIRE(ring_cell(world, c->x, c->y, 0, vx, vy));
    ra2engine::UnitCombat truck = w7_gi(false);
    truck.image = "TRUCKA";
    truck.tg.rules_id = "TRUCKA";   // the civilian truck: same image, other type
    truck.strength = 100000;
    truck.damage = 0;
    world.add_combat_unit(vx, vy, 1, truck);
    REQUIRE(ring_cell(world, vx, vy, 3, ax, ay));
    ra2engine::UnitCombat attacker = w7_gi(false);
    attacker.strength = 100000;
    world.add_combat_unit(ax, ay, 0, attacker);
    int hx = 0, hy = 0;
    REQUIRE(ring_cell(world, vx, vy, 9, hx, hy));
    ra2engine::UnitCombat helper = w7_gi(false);
    helper.tg.rules_id = "DTRUCK";
    helper.range = 1.0;
    world.add_combat_unit(hx, hy, 1, helper);
    world.order_unit_attack(1, 0);
    for (int f = 0; f < 120; ++f) world.step(1.0 / 15.0);
    CHECK(world.units()[0].hp < 100000);
    CHECK(world.units()[2].target != 1);   // the TRUCKA is not a (protected) DTRUCK
}
