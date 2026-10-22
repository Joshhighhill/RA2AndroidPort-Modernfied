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
TEST_CASE("Yuri core: mobile slave miner satisfies PROC but not other prerequisites") {
    Rig r; World w(r.map);
    BuildingTypeDef cy; cy.id="YACNST"; cy.conyard=true; cy.strength=100;
    BuildingTypeDef barr; barr.id="YABRCK"; barr.strength=100;
    BuildingTypeDef factory; factory.id="YAWEAP"; factory.tech_level=1;
    factory.prerequisite={"PROC","YABRCK","YACNST"};
    BuildingTypeDef ref; ref.id="YAREFN";
    const std::vector<BuildingTypeDef> types={cy,barr,factory,ref};
    w.set_economy(types,{},OreValues{},1000,0,0);
    w.add_building(0,2,2,1,1,0,100,0,0,true);
    w.add_building(1,4,2,1,1,0,100,0,0,true);
    PlayerEconRules econ; econ.groups={{"PROC",{"YAREFN"}},{"PROC_ALTERNATE",{"SMIN"}}};
    w.enable_player_economy(0,5000); w.set_player_econ_rules(0,econ);
    CHECK_FALSE(w.econ_can_build(0,QueueCat::Structure,2));
    w.add_combat_unit(6,2,1,unit("SMIN"));
    CHECK_FALSE(w.econ_can_build(0,QueueCat::Structure,2));
    w.set_unit_owner(0,0);
    CHECK(w.econ_can_build(0,QueueCat::Structure,2));
    w.units()[0].die_time=0;
    CHECK_FALSE(w.econ_can_build(0,QueueCat::Structure,2));
    w.units()[0].die_time=-1;
    w.buildings()[1].alive=false;
    CHECK_FALSE(w.econ_can_build(0,QueueCat::Structure,2));
    w.buildings()[1].alive=true;
    CHECK(w.econ_produce(0,QueueCat::Structure,2)==ProdResult::Started);
}

TEST_CASE("Yuri core: miner packs without selling and reuses workers after redeploy") {
    Rig r; World w(r.map); BuildingTypeDef d;
    d.id="YAREFN"; d.fw=d.fh=2; d.strength=200; d.slave_count=2;
    d.slave_combat=unit("SLAV"); d.slave_combat.is_infantry=true;
    w.set_economy({d},{0},OreValues{},1000,0,0);
    auto mobile=unit("SMIN"); mobile.deploys_into="YAREFN";
    w.set_undeploy_unit(0,mobile);
    w.add_building(0,8,8,2,2,0,200,0,0,true); w.buildings()[0].hp=100;
    w.step_yuri_workers(); REQUIRE(w.units().size()==2);
    CHECK_FALSE(w.order_pack_slave_miner(0,1));
    REQUIRE(w.order_pack_slave_miner(0,0));
    CHECK(w.buildings()[0].state==Building::State::Dead); REQUIRE(w.units().size()==3);
    CHECK(w.units()[2].hp==50); CHECK(w.units()[2].deploys_into==0);
    CHECK(w.units()[0].w7.slave_manager==-4); CHECK_FALSE(w.units()[0].is_harvester);
    const auto state=save::save_world(w); World loaded(r.map); REQUIRE(save::load_world(loaded,state));
    CHECK(loaded.units()[0].w7.slave_manager==-4);
    REQUIRE(w.order_deploy(2));
    for(int i=0;i<150 && w.units()[2].alive;++i) w.step(1.0/15.0);
    REQUIRE_FALSE(w.units()[2].alive); REQUIRE(w.buildings().size()==2);
    CHECK(w.units().size()==3); CHECK(w.units()[0].w7.slave_manager==1);
    CHECK(w.units()[0].is_harvester); CHECK_FALSE(w.units()[0].manual_hold);
    CHECK(w.buildings()[1].hp==100); CHECK(w.take_income(0)==0);
}

TEST_CASE("Yuri core: blocked miner packing keeps the building and workforce") {
    Rig r; World w(r.map); BuildingTypeDef d;
    d.id="YAREFN";d.fw=d.fh=2;d.strength=100;d.slave_count=2;d.slave_combat=unit("SLAV");
    w.set_economy({d},{0},OreValues{},1000,0,0);w.set_undeploy_unit(0,unit("SMIN"));
    w.add_building(0,8,8,2,2,0,100,0,0,true);w.step_yuri_workers();
    w.add_combat_unit(8,8,0,unit("BLOCKER"));
    CHECK_FALSE(w.order_pack_slave_miner(0,0));
    CHECK(w.buildings()[0].state==Building::State::Idle);CHECK(w.buildings()[0].hp==100);
    CHECK(w.units().size()==3);CHECK(w.units()[0].w7.slave_manager==0);
}

TEST_CASE("Yuri core: worker shovel and carry sequences animate and return to ready") {
    InfTable t; t.crawls=false;
    t.e[kSqReady]={0,1,0,-1,true}; t.e[kSqWalk]={8,4,4,-1,true};
    t.e[kSqShovel]={40,3,3,-1,true}; t.e[kSqCarry]={64,4,4,-1,true};
    InfAnimState s; InfTickCtx c; int facing=0; c.shoveling=true;
    inf_anim_tick(s,t,facing,c); CHECK(s.seq==kSqShovel);
    int first=inf_anim_frame(s,t,facing); bool changed=false;
    for(int i=0;i<20;++i) changed |= inf_anim_tick(s,t,facing,c)!=first;
    CHECK(changed);
    c.shoveling=false; c.moving=true; c.carrying=true;
    inf_anim_tick(s,t,facing,c); CHECK(s.seq==kSqCarry);
    c.moving=false; c.carrying=false;
    inf_anim_tick(s,t,facing,c); CHECK(s.seq==kSqReady);
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

TEST_CASE("Yuri core: ground force fire stops at range instead of marching into target") {
    Rig r; World w(r.map); auto c=unit("TANK");c.damage=20;c.rof=10;c.range=3;
    w.add_combat_unit(2,2,0,c);
    w.order_unit_move(0,18,2);
    w.order_attack_cell(0,10,2);
    bool fired=false;
    for(int i=0;i<300;++i){w.step(1.0/15.0);fired=fired||w.units()[0].fire_flash>0;}
    CHECK(fired);CHECK(w.units()[0].fx<9);CHECK_FALSE(w.units()[0].moving());
    const double x=w.units()[0].fx,y=w.units()[0].fy;
    for(int i=0;i<60;++i)w.step(1.0/15.0);
    CHECK(w.units()[0].fx==x);CHECK(w.units()[0].fy==y);
}
TEST_CASE("Yuri core: Psychic Tower combat captures to capacity and releases after power off") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAPSYT";d.mind_control=true;
    d.strength=100;d.armed=true;d.weapon[0].valid=true;d.weapon[0].damage=2;
    d.weapon[0].range=10;d.weapon[0].rof=1;d.weapon[0].verses.assign(11,100);
    w.set_building_types({d});w.add_building(0,8,8,1,1,0,100,0,0,true);
    for(int i=0;i<3;++i)w.add_combat_unit(3+i,3,1,unit("ENEMY"));
    for(int i=0;i<60;++i)w.step(1.0/15.0);
    REQUIRE(w.buildings()[0].w7.mc_victims.size()==2);
    int controlled=0;for(const auto& u:w.units()){if(u.house==0)++controlled;CHECK(u.hp==100);}
    CHECK(controlled==2);
    const auto state=save::save_world(w);World loaded(r.map);REQUIRE(save::load_world(loaded,state));
    loaded.buildings()[0].power_off=true;loaded.step(1.0/15.0);
    CHECK(loaded.buildings()[0].w7.mc_victims.empty());
    for(const auto& u:loaded.units()){CHECK(u.house==1);CHECK(u.w7.mc_building==-1);}
}

TEST_CASE("Yuri core: captured units leave their previous AI team immediately") {
    Rig r;World w(r.map);AiHouseConfig cfg;cfg.team.enabled=true;
    w.enable_ai_house(1,cfg);w.add_combat_unit(2,2,1,unit("TANK"));
    auto* state=const_cast<AiTeamState*>(w.ai_team_state(1));REQUIRE(state);
    TeamInstance team;team.members={0};team.member_types={0};state->teams.push_back(team);
    w.units()[0].tg.team_aggressive=true;w.units()[0].tg.team_suicide=true;
    w.set_unit_owner(0,0);
    CHECK(state->teams[0].members.empty());CHECK(state->teams[0].member_types.empty());
    CHECK_FALSE(w.units()[0].tg.team_aggressive);CHECK_FALSE(w.units()[0].tg.team_suicide);
}
