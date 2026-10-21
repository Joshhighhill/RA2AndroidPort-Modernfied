#include "doctest.h"
#include "world.hpp"
#include "harvest.hpp"
#include "savegame.hpp"
using namespace ra2engine;
namespace {
struct Rig {
    ra2assets::MapFile map = ra2assets::MapFile::parse("[Map]\nSize=0,0,20,20\nTheater=TEMPERATE\n");
    Rig() {
        // The non-const fixture owns this vector; no retail map/codec needed.
        auto& cells = const_cast<std::vector<ra2assets::MapCell>&>(map.cells());
        for (int y = 0; y < 20; ++y) for (int x = 0; x < 20; ++x)
            cells.push_back({x,y,0,0,0});
    }
};
UnitCombat unit(const char* image) {
    UnitCombat c; c.image = image; c.strength = 100; c.speed_cps = 2;
    c.verses.assign(11,100); c.tg.rules_id = image; return c;
}
}
TEST_CASE("Yuri core: shot captures and controller death restores original owner") {
    Rig r; World w(r.map);
    auto c = unit("YURI"); c.w7.mind_control = true;
    w.add_combat_unit(2,2,0,c); w.add_combat_unit(4,2,1,unit("TANK"));
    w.prepare_skirmish_mind_control(); CHECK(w.units()[0].w_mind_control);
    CHECK(w.units()[1].house == 1); // being in range alone never captures
    w.units()[0].w7.mc_shot = 1; w.resolve_skirmish_mind_control();
    CHECK(w.units()[1].house == 0); CHECK(w.units()[1].w7.mc_controller == 0);
    w.units()[0].alive = false; w.prepare_skirmish_mind_control();
    CHECK(w.units()[1].house == 1); CHECK(w.units()[1].w7.mc_controller == -1);
}
TEST_CASE("Yuri core: capacity one swaps; multiple capacity refuses extra; immunity refuses") {
    Rig r; World w(r.map); auto c = unit("YURI"); c.w7.mind_control = true;
    w.add_combat_unit(2,2,0,c);
    w.add_combat_unit(4,2,1,unit("A")); w.add_combat_unit(6,2,1,unit("B"));
    w.units()[0].w7.mc_shot = 1; w.resolve_skirmish_mind_control();
    w.units()[0].w7.mc_shot = 2; w.resolve_skirmish_mind_control();
    CHECK(w.units()[1].house == 1); CHECK(w.units()[2].house == 0);
    w.units()[0].w7.t.mc_capacity = 2;
    w.units()[0].w7.mc_shot = 1; w.resolve_skirmish_mind_control();
    REQUIRE(w.units()[0].w7.mc_victims.size() == 2);
    auto immune = unit("IMMUNE"); immune.tg.immune_psionics = true;
    w.add_combat_unit(8,2,1,immune);
    w.units()[0].w7.mc_shot = 3; w.resolve_skirmish_mind_control();
    CHECK(w.units()[3].house == 1);
}
TEST_CASE("Yuri core: ordinary combat shot captures without damaging victim") {
    Rig r; World w(r.map); auto c = unit("YURI");
    c.w7.mind_control = true; c.damage = 1; c.rof = 20; c.range = 6;
    w.add_combat_unit(2,2,0,c); w.add_combat_unit(4,2,1,unit("TANK"));
    w.units()[0].target = 1; w.units()[0].attack_order = true;
    for (int i = 0; i < 30 && w.units()[1].house == 1; ++i) w.step(1.0/15.0);
    CHECK(w.units()[1].house == 0); CHECK(w.units()[1].hp == 100);
    const auto state = save::save_world(w);
    World loaded(r.map); REQUIRE(save::load_world(loaded,state));
    CHECK(loaded.units()[1].w7.mc_original_house == 1);
    loaded.units()[0].alive = false; loaded.prepare_skirmish_mind_control();
    CHECK(loaded.units()[1].house == 1);
}
TEST_CASE("Yuri core: deployed miner spawns workers and harvests finite ore") {
    Rig r; World w(r.map); BuildingTypeDef d;
    d.id = "YAREFN"; d.fw = d.fh = 2; d.strength = 100;
    d.slave_count = 2; d.slave_regen_rate = 3; d.slave_storage = 1; d.slave_combat = unit("SLAV");
    w.set_economy({d},{0},OreValues{},1000,0,0);
    w.add_building(0,8,8,2,2,0,100,0,0,true);
    w.buildings()[0].state = Building::State::Idle;
    w.step_yuri_workers(); REQUIRE(w.units().size() == 2);
    CHECK(w.units()[0].is_harvester); CHECK(w.units()[0].w7.slave_manager == 0);
    w.ore().set(5,5,OreKind::Ore,10,0);
    for (int i = 0; i < 1800; ++i) w.step(1.0/15.0);
    CHECK(w.house_credits(0) > 0);
    CHECK(w.ore().density_at(5,5) < 10);
    const auto state = save::save_world(w);
    World loaded(r.map); REQUIRE(save::load_world(loaded,state));
    CHECK(save::save_world(loaded) == state);
    for (int i = 0; i < 30; ++i) { w.step(1.0/15.0); loaded.step(1.0/15.0); }
    CHECK(save::save_world(w) == save::save_world(loaded));
    w.buildings()[0].alive = false; w.step_yuri_workers();
    CHECK(w.units()[0].w7.slave_manager == -1); CHECK_FALSE(w.units()[0].is_harvester);
}
