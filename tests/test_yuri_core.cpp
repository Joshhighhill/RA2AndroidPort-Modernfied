#include "doctest.h"
#include "world.hpp"
#include "harvest.hpp"
#include "savegame.hpp"
#include "weapon_resolve.hpp"
#include "ra2assets/rules.hpp"
#include "weapon_sound_loop.hpp"
#include "weapon_unit.hpp"
#include "report_probe.hpp"
#include "psychic_links.hpp"
#include "psychic_sensor.hpp"
#include "presentation_policy.hpp"
#include "weapon_art.hpp"
#include "ai_base.hpp"
#include "ground_presentation.hpp"
#include "production.hpp"
#include "asset_fixture.hpp"
#include "projectile_presentation.hpp"
#include "building_transition.hpp"
#include "faction_support.hpp"
#include "command.hpp"
#include "selection.hpp"
#include <fstream>
#include <iterator>
using namespace ra2engine;

TEST_CASE("weapon art: correct image, numbered, secondary and elite muzzle entries") {
    const auto ini=ra2assets::Ini::parse(
        "[LTNK]\nPrimaryFireFLH=225,0,100\nSecondaryFireFLH=80,15,90\n"
        "ElitePrimaryFireFLH=250,0,120\nWeapon1FLH=160,30,150\n"
        "Weapon2FLH=160,-30,150\nEliteWeapon2FLH=170,-35,160\n"
        "Weapon3FLH=bad\nWeapon4FLH=0,0,0\nTurretOffset=-16\n"
        "[RULESID]\nPrimaryFireFLH=999,999,999\n");
    const auto art=ra2assets::Art::load(ini);
    CHECK(weapon_art_flh(art,"LTNK",0,false,false)==std::array<int,3>{225,0,100});
    CHECK(weapon_art_flh(art,"LTNK",0,true,false)==std::array<int,3>{250,0,120});
    CHECK(weapon_art_flh(art,"LTNK",1,true,false)==std::array<int,3>{80,15,90});
    CHECK(weapon_art_flh(art,"LTNK",1,true,true)==std::array<int,3>{170,-35,160});
    CHECK(weapon_art_flh(art,"LTNK",2,false,true)==std::array<int,3>{225,0,100});
    CHECK(weapon_art_flh(art,"LTNK",3,false,true)==std::array<int,3>{0,0,0});
    const auto ri=ra2assets::Ini::parse("[RULESID]\nImage=LTNK\nWeaponCount=6\n");
    const auto rules=ra2assets::Rules::load(ri);
    UnitCombat c;c.image="LTNK";c.proj.kind=ProjKind::Ballistic;c.damage=37;
    c.gattling.weapons.resize(6);c.gattling.elite_weapons.resize(6);
    bind_unit_weapon_art(c,art,rules,"RULESID");
    CHECK(c.proj.flh[0]==160);CHECK(c.secondary.proj.flh[1]==-30);
    CHECK(c.elite_secondary.proj.flh[2]==160);
    CHECK(c.gattling.weapons[1].proj.flh[1]==-30);
    CHECK(c.gattling.elite_weapons[1].proj.flh[2]==160);
    CHECK(c.proj.turret_offset==-16);CHECK(c.proj.kind==ProjKind::Ballistic);CHECK(c.damage==37);
}
TEST_CASE("projectile presentation: first interval starts at launch and subsequent ticks interpolate") {
    ProjectilePresentation motion;Projectile p;p.attacker=4;p.weapon_id="120mm";
    p.launch_frame=20;p.x=1100;p.y=500;p.z=140;
    ProjectileEvent e;e.kind=ProjectileEvent::Launch;e.attacker=4;e.frame=20;
    e.x=1000;e.y=500;e.z=100;
    motion.capture({p},{e});
    CHECK(motion.sample(0,p,0)==std::array<double,3>{1000,500,100});
    CHECK(motion.sample(0,p,0.5)==std::array<double,3>{1050,500,120});
    CHECK(motion.sample(0,p,1)==std::array<double,3>{1100,500,140});
    CHECK(p.x==1100);CHECK(p.z==140);
    p.x=1200;p.z=170;motion.capture({p},{});
    CHECK(motion.sample(0,p,0)==std::array<double,3>{1100,500,140});
    CHECK(motion.sample(0,p,0.5)==std::array<double,3>{1150,500,155});
    CHECK(motion.sample(0,p,2)==std::array<double,3>{1200,500,170});
    motion.capture({},{});CHECK(motion.size()==0);
    motion.clear();CHECK(motion.sample(0,p,0)==std::array<double,3>{1200,500,170});
}
TEST_CASE("projectile presentation: reordered bullets, salvos and separate defense launches stay independent") {
    ProjectilePresentation m;Projectile a;a.attacker=1;a.launch_frame=3;a.x=100;
    Projectile b=a;b.attacker=2;b.x=1000;m.capture({a,b},{});
    a.x=200;b.x=1200;m.capture({b,a},{});
    CHECK(m.sample(0,b,0.5)[0]==1100);CHECK(m.sample(1,a,0.5)[0]==150);
    a.attacker=b.attacker=-1;a.attacker_building=10;b.attacker_building=20;
    a.launch_frame=b.launch_frame=9;a.x=200;b.x=1200;
    ProjectileEvent e;e.frame=9;e.attacker=-1;e.x=100;
    auto f=e;f.x=1100;m.clear();m.capture({b,a},{e,f});
    CHECK(m.sample(0,b,0)[0]==1100);CHECK(m.sample(1,a,0)[0]==100);
    a.attacker=1;a.attacker_building=-1;b=a;b.x=220;
    e.attacker=f.attacker=1;e.x=100;f.x=120;
    m.clear();m.capture({a,b},{e,f});
    CHECK(m.size()==2);CHECK(m.sample(0,a,0)[0]!=m.sample(1,b,0)[0]);
    // One salvo sibling expires; its survivor must retain its own segment.
    a.x=100;b.x=300;m.clear();m.capture({a,b},{});
    b.x=320;b.vx=20;m.capture({b},{});
    CHECK(m.sample(0,b,0)[0]==300);CHECK(m.sample(0,b,0.5)[0]==310);
}

TEST_CASE("presentation policy: repeated queue and rally speech has independent three-second windows") {
    SpeechCooldown gate;
    CHECK(gate.allow("building",0));CHECK_FALSE(gate.allow("building",1000));
    CHECK_FALSE(gate.allow("building",2999));CHECK(gate.allow("building",3000));
    CHECK(gate.allow("rally",3001));CHECK_FALSE(gate.allow("rally",4000));
    CHECK(gate.allow("training",4000));gate.clear();CHECK(gate.allow("rally",4000));
}
TEST_CASE("presentation policy: nuclear visual follows existing up and down strike phases") {
    CHECK(nuke_visual_height(0,200,200)==0);
    CHECK(nuke_visual_height(10,200,200)==55);
    CHECK(nuke_visual_height(200,200,200)==20000);
    CHECK(nuke_visual_height(300,200,200)==10000);
    CHECK(nuke_visual_height(400,200,200)==0);
    CHECK(nuke_visual_height(500,200,200)==0);
}
TEST_CASE("Tank Bunker: drive state is reset on attachment and ejection without changing type settings") {
    // Fixture is declared below; standalone minimal terrain keeps this test isolated.
    ra2assets::MapFile map;
    auto& cells=const_cast<std::vector<ra2assets::MapCell>&>(map.cells());
    for(int y=0;y<20;++y)for(int x=0;x<20;++x)cells.push_back({x,y,0,0,0});
    World w(map);BuildingTypeDef d;d.id="NATBNK";d.tank_bunker=true;
    d.can_be_occupied=true;d.max_occupants=1;d.strength=1000;d.fw=d.fh=2;
    w.set_building_types({d});w.add_building(0,8,8,2,2,0,1000,0,0,true);
    UnitCombat tank;tank.strength=200;tank.is_vehicle=true;tank.w7.bunkerable=true;
    w.add_combat_unit(7,8,0,tank);
    auto poison=[&](){auto& u=w.units()[0];u.loco.trk=5;u.loco.accum=20;u.loco.path_delay=80;u.wait_time=5;u.loco.t.accel_factor=0.07;};
    poison();REQUIRE(w.campaign_occupy_now(0,0));
    CHECK(w.units()[0].loco.trk==-1);CHECK(w.units()[0].loco.accum==0);
    CHECK(w.units()[0].wait_time==0);CHECK(w.units()[0].loco.t.accel_factor==doctest::Approx(0.07));
    poison();w.unload_occupants(0);
    CHECK(w.units()[0].garrison_of==-1);CHECK(w.units()[0].loco.trk==-1);
    CHECK(w.units()[0].loco.path_delay==0);CHECK(w.units()[0].wait_time==0);
    World restored(map);REQUIRE(save::load_world(restored,save::save_world(w)));
    for(int i=0;i<120;++i){w.step(1.0/15.0);restored.step(1.0/15.0);}
    CHECK(save::save_world(w)==save::save_world(restored));
}
TEST_CASE("Yuri roster: real structures exclude shared scenery and dormant prototypes") {
    for(const char* id:{"YACNST","YAREFN","NAPSIS","NACLON","GAFWLL","NATBNK"})
        CHECK(vanilla_yuri_building(id));
    for(const char* id:{"GASAND","CAFNCB","CAFNCP","GALAMP","GAWALL","NAWALL","YACOMD","YAROCK","YADEPT"})
        CHECK_FALSE(vanilla_yuri_building(id));
}
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
TEST_CASE("missile launchers: explicit V3 and Boomer land orders launch spawned rockets") {
    for(bool boomer : {false,true}) {
        Rig r;World w(r.map);
        auto rocket=unit(boomer ? "CMISL" : "V3ROCKET");
        rocket.is_aircraft=true;rocket.is_vehicle=true;rocket.an.spawned=true;
        rocket.an.missile=true;rocket.an.rocket_kind=boomer ? 2 : 0;rocket.an.rocket_speed=38;
        w.register_an_type(rocket.image,rocket);
        auto launcher=unit(boomer ? "BSUB" : "V3");launcher.is_vehicle=true;
        launcher.damage=1;launcher.range=18;launcher.rof=150;launcher.omni_fire=true;
        launcher.an.spawner_slot=boomer ? 1 : 0;launcher.an.spawns=rocket.image;
        launcher.an.spawns_number=boomer ? 2 : 1;launcher.an.spawner_range=20;
        launcher.an.spawn_regen_frames=400;
        if(boomer) {
            launcher.an.naval_targeting=7;launcher.an.land_targeting=2;
            launcher.secondary.valid=true;launcher.secondary.damage=25;
            launcher.secondary.range=20;launcher.secondary.rof=50;
            launcher.secondary.verses.assign(11,100);launcher.secondary.omni_fire=true;
        }
        w.add_combat_unit(2,2,0,launcher);w.add_combat_unit(14,2,1,unit("TARGET"));
        w.group_attack(0,{0},1);
        for(int f=0;f<180 && w.units().size()<3;++f)w.step(1.0/15.0);
        REQUIRE(w.units().size()>=3);
        CHECK(w.units()[0].w_active==(boomer ? 1 : 0));
        CHECK(w.units()[2].an.t.missile);CHECK(w.units()[2].an.owner==0);
    }
}
TEST_CASE("diagnostics: unit probes preserve saved state and catch broken references") {
    Rig r; World w(r.map);
    w.add_combat_unit(2,2,0,unit("SLAV"));
    auto& worker=w.units()[0];
    worker.w7.slave_manager=999;
    worker.path={{2,2},{3,2}}; worker.path_idx=1; worker.wait_time=4;
    worker.bales_ore=3;
    BuildingTypeDef aa;aa.id="NASAM";aa.armed=true;
    aa.weapon[0].valid=true;aa.weapon[0].ag=false;aa.weapon[0].aa=true;
    w.set_economy({aa},{},OreValues{},1000,0,0);
    w.add_building(0,10,10,1,1,0,100,0,0,true);
    const auto before=save::save_world(w);
    const auto root=std::filesystem::temp_directory_path()/"ra2-probe-stability";
    struct Stop { ~Stop(){diag::stop();} } stop;
    REQUIRE(diag::start(root.u8string()));
    diag::probe_world(w);
    CHECK(save::save_world(w)==before);
    diag::flush();
    std::ifstream stream(root/"engine-current.log",std::ios::binary);
    const std::string log{std::istreambuf_iterator<char>(stream),{}};
    CHECK(log.find("invalid_slave_manager=999")!=std::string::npos);
    CHECK(log.find("blocked_over_3s=1")!=std::string::npos);
    CHECK(log.find("ROUTE unit=0 next=3,2")!=std::string::npos);
    CHECK(log.find("ground=0 air=1")!=std::string::npos);
    worker.path_idx=900;
    diag::probe_world(w); // Invalid index must be reported without dereferencing it.
}

TEST_CASE("Yuri core: numbered Gatling guns retain ground and air weapons and auto-acquire") {
    const auto ini=ra2assets::Ini::parse(
        "[YAGGUN]\nIsGattling=yes\nWeaponCount=6\nWeapon1=Ground\nWeapon2=Air\nTurret=yes\n"
        "[YTNK]\nIsGattling=yes\nPrimary=Ground\nSecondary=Air\n"
        "[Ground]\nDamage=5\nROF=15\nRange=5\nProjectile=Bullet\nWarhead=AP\n"
        "[Air]\nDamage=5\nROF=15\nRange=5\nProjectile=AA\nWarhead=AP\n"
        "[Bullet]\nAG=yes\nAA=no\n[AA]\nAG=no\nAA=yes\n[AP]\nVerses=100%,100%,100%,100%,100%,100%,100%,100%,100%,100%,100%\n");
    const auto rules=ra2assets::Rules::load(ini);
    auto tw=resolve_type_weapons(rules,"YAGGUN");
    CHECK(tw.slot[0].id=="Ground");CHECK(tw.slot[1].id=="Air");
    CHECK(tw.slot[1].aa);CHECK_FALSE(tw.slot[1].ag);
    CHECK(resolve_type_weapons(rules,"YTNK").slot[0].id=="Ground");
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAGGUN";d.strength=500;
    fill_building_weapons(rules,"YAGGUN",d);REQUIRE(d.armed);
    w.set_economy({d},{},OreValues{},1000,0,0);w.add_building(0,8,8,1,1,0,500,0,0,true);
    auto enemy=unit("ENEMY");enemy.strength=1000;w.add_combat_unit(10,8,1,enemy);
    for(int i=0;i<120;++i)w.step(1.0/15.0);
    CHECK(w.units()[0].hp<1000);
}

TEST_CASE("Yuri core: automatic ranged pursuit stops before melee contact") {
    Rig r;World w(r.map);auto tank=unit("LTNK");tank.range=5;tank.damage=5;tank.rof=15;tank.w6.omni_fire=true;
    w.add_combat_unit(2,2,0,tank);auto enemy=unit("TARGET");enemy.strength=1000;
    w.add_combat_unit(8,2,1,enemy);
    w.order_unit_move(0,7,2);w.units()[0].target=1;w.units()[0].attack_order=false;
    w.units()[0].guard_x=2;w.units()[0].guard_y=2;
    for(int i=0;i<180;++i)w.step(1.0/15.0);
    INFO("pursuer x="<<w.units()[0].fx<<" target="<<w.units()[0].target<<" range="<<w.units()[0].w_range<<" damage="<<w.units()[0].w_damage<<" flash="<<w.units()[0].fire_flash);
    CHECK(w.units()[0].fx<=4);CHECK(w.units()[1].hp<1000);
}

TEST_CASE("Yuri core: psychic sensor uses powered allied radius and commanded destinations") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="NAPSIS";d.strength=500;d.w5.psychic_radius=4;
    w.set_building_types({d});w.add_building(0,8,8,1,1,0,500,0,0,true);
    w.add_combat_unit(2,2,1,unit("ENEMY"));w.order_unit_move(0,9,9);
    int x=0,y=0;CHECK(psychic_sensor_active(w,0,0));CHECK_FALSE(psychic_sensor_active(w,0,1));
    CHECK(psychic_sensor_target(w,0,0,x,y));CHECK(x==9);CHECK(y==9);
    w.order_unit_move(0,18,18);CHECK_FALSE(psychic_sensor_target(w,0,0,x,y));
    w.buildings()[0].power_off=true;CHECK_FALSE(psychic_sensor_active(w,0,0));
    ra2assets::Palette pal{};
    for(int i=0;i<256;++i)pal[i]={static_cast<unsigned char>(i),static_cast<unsigned char>(i),static_cast<unsigned char>(i),255};
    w.set_house(1,World::HouseRec{World::Side::Soviet,2,"Russians",-1,true});
    w.set_house(2,World::HouseRec{World::Side::Allied,5,"Americans",-1,true});
    const auto color=psychic_intent_color(w,pal,1),expected=w.house_palette(pal,1)[24];
    CHECK(color.r==expected.r);CHECK(color.g==expected.g);CHECK(color.b==expected.b);
    const auto other=psychic_intent_color(w,pal,2);
    CHECK((color.r!=other.r || color.g!=other.g || color.b!=other.b));
    // Presentation queries cannot alter the simulation/save state.
    const auto before=save::save_world(w);(void)psychic_intent_color(w,pal,2);
    CHECK(save::save_world(w)==before);

}

TEST_CASE("Yuri core: presentation collects unit and building mind-control links") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAPSYT";d.strength=500;
    w.set_building_types({d});w.add_building(0,8,8,1,1,0,500,0,0,true);
    for(int i=0;i<3;++i)w.add_combat_unit(2+i,2,0,unit("UNIT"));
    w.units()[1].w7.mc_controller=0;w.units()[2].w7.mc_building=0;
    const auto before=save::save_world(w);const auto links=skirmish_control_links(w);
    REQUIRE(links.size()==2);CHECK(links[0]==std::make_pair(0,1));CHECK(links[1]==std::make_pair(-1,2));
    CHECK(save::save_world(w)==before);
    w.units()[0].alive=false;CHECK(skirmish_control_links(w).size()==1);
}
TEST_CASE("Yuri core: infantry caps never auto-swap and deploy preserves ownership") {
    for (const char* id : {"YURI","YURIPR"}) {
        Rig r;World w(r.map);auto c=unit(id);c.w7.mind_control=true;
        c.deploy_fire=true;c.deploy_fire_slot=1;c.w7.deploy_area_fire=true;
        c.secondary.valid=true;c.secondary.range=1;c.secondary.damage=0;
        w.add_combat_unit(2,2,0,c);w.add_combat_unit(5,2,1,unit("A"));w.add_combat_unit(7,2,1,unit("B"));
        w.units()[0].w7.mc_shot=1;w.resolve_skirmish_mind_control();
        for(int n=0;n<20;++n){w.units()[0].w7.mc_shot=2;w.resolve_skirmish_mind_control();}
        CHECK(w.units()[0].w7.mc_victims.size()==1);CHECK(w.units()[1].house==0);CHECK(w.units()[2].house==1);
        REQUIRE(w.order_deploy(0));CHECK(w.units()[1].house==0);
        REQUIRE(w.units()[0].w7.mc_victims.size()==1);
        CHECK(w.units()[0].w7.mc_victims[0]==1);
        CHECK(w.units()[1].w7.mc_controller==0);
        const auto saved=save::save_world(w);World loaded(r.map);REQUIRE(save::load_world(loaded,saved));
        CHECK(loaded.units()[1].house==0);CHECK(loaded.units()[1].w7.mc_controller==0);
        w.units()[0].w7.mc_shot=2;w.resolve_skirmish_mind_control();CHECK(w.units()[2].house==1);
    }
}
TEST_CASE("Yuri core: enslaved workers reject player commands but retain internal movement") {
    Rig r;World w(r.map);auto slave=unit("SLAV");slave.is_infantry=true;
    w.add_combat_unit(2,2,0,slave);w.set_order_issuer(0);
    for(int manager:{0,-2}) {
        auto& u=w.units()[0];u.w7.slave_manager=manager;
        CHECK(Selection::unselectable(u));
        w.order_unit_move(0,6,2);REQUIRE(u.moving());
        const auto path=u.path;
        apply_command(w,make_whom(w,EventType::Idle,{0}));
        CHECK(u.path.size()==path.size());CHECK(u.moving());
        apply_command(w,make_mega(w,{0},Mission::Move,Target{},Target::cell(2,8)));
        CHECK(u.path.back().x==6);CHECK(u.path.back().y==2);
        apply_command(w,make_whom(w,EventType::Scatter,{0}));
        CHECK(u.path.back().x==6);CHECK(u.path.back().y==2);
    }
    w.units()[0].w7.slave_manager=-1;
    CHECK_FALSE(Selection::unselectable(w.units()[0]));
    apply_command(w,make_whom(w,EventType::Idle,{0}));
    CHECK_FALSE(w.units()[0].moving());
}
TEST_CASE("Yuri core: surviving slaves become controllable by their miner's killer") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;
    d.slave_count=2;d.slave_combat=unit("SLAV");d.slave_combat.is_infantry=true;
    w.set_economy({d},{0},OreValues{},1000,0,0);
    w.add_building(0,8,8,2,2,0,100,0,0,true);w.step_yuri_workers(false);
    REQUIRE(w.units().size()==2);
    auto killer=unit("LTNK");w.add_combat_unit(2,2,1,killer);
    w.units()[1].alive=false;w.units()[1].die_time=0;
    w.buildings()[0].alive=false;w.buildings()[0].state=Building::State::Dead;
    w.buildings()[0].killed_by=2;w.step_yuri_workers(false);
    CHECK(w.units()[0].house==1);CHECK(w.units()[0].w7.slave_manager==-1);
    CHECK_FALSE(w.units()[0].is_harvester);CHECK_FALSE(Selection::unselectable(w.units()[0]));
    w.set_order_issuer(0);
    apply_command(w,make_mega(w,{0},Mission::Move,Target{},Target::cell(3,3)));
    CHECK_FALSE(w.units()[0].moving());
    w.set_order_issuer(1);
    auto order=make_mega(w,{0},Mission::Move,Target{},Target::cell(3,3));order.house=1;
    apply_command(w,order);
    REQUIRE(w.units()[0].moving());CHECK(w.units()[0].path.back().x==3);
    CHECK_FALSE(w.units()[1].alive); // dead workers are not resurrected/transferred
    const auto state=save::save_world(w);World loaded(r.map);REQUIRE(save::load_world(loaded,state));
    CHECK(loaded.units()[0].house==1);CHECK(loaded.units()[0].w7.slave_manager==-1);
}
TEST_CASE("Yuri core: Mastermind overload uses configured damage and releases on death") {
    Rig r;World w(r.map);auto c=unit("MIND");c.strength=500;c.w7.mind_control=true;
    c.w7.mc_capacity=3;c.w7.mc_infinite=true;c.w7.overload_count={3,6,10};
    c.w7.overload_damage={0,50,100};c.w7.overload_frames={2,2,2};w.add_combat_unit(2,2,0,c);
    for(int j=0;j<4;++j){w.add_combat_unit(4+j,2,1,unit("TANK"));w.units()[0].w7.mc_shot=j+1;w.resolve_skirmish_mind_control();}
    REQUIRE(w.units()[0].w7.mc_victims.size()==4);w.prepare_skirmish_mind_control();CHECK(w.units()[0].hp==450);
    const auto state=save::save_world(w);World loaded(r.map);REQUIRE(save::load_world(loaded,state));
    CHECK(loaded.units()[0].w7.overload_left==1);
    w.prepare_skirmish_mind_control();CHECK(w.units()[0].hp==450);
    w.prepare_skirmish_mind_control();CHECK(w.units()[0].hp==400);
    w.units()[0].alive=false;w.prepare_skirmish_mind_control();
    for(int j=1;j<=4;++j)CHECK(w.units()[j].house==1);
}
TEST_CASE("Yuri core: Psychic Tower refuses its fourth victim") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAPSYT";d.mind_control=true;d.strength=100;
    d.weapon[0].damage=3;w.set_economy({d},{},OreValues{},1000,0,0);w.add_building(0,2,2,1,1,0,100,0,0,true);
    for(int i=0;i<4;++i)w.add_combat_unit(5+i,2,1,unit("TANK"));
    for(int i=0;i<3;++i)CHECK(w.try_building_mind_control(0,i));
    CHECK_FALSE(w.try_building_mind_control(0,3));CHECK(w.units()[3].house==1);
    CHECK(w.buildings()[0].w7.mc_victims.size()==3);
}
TEST_CASE("Yuri core: Tank Bunker accepts one eligible tank, fires and unloads without changing owner") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="NATBNK";d.tank_bunker=true;
    d.can_be_occupied=d.can_occupy_fire=true;d.max_occupants=1;d.fw=d.fh=2;d.strength=1000;
    w.set_economy({d},{},OreValues{},1000,0,0);w.add_building(0,8,8,2,2,0,1000,0,0,true);
    auto tank=unit("LTNK");tank.is_vehicle=true;tank.w7.bunkerable=true;
    tank.occupy_weapon.valid=true;tank.occupy_weapon.damage=20;tank.occupy_weapon.rof=10;tank.occupy_weapon.range=5;
    tank.occupy_weapon.verses.assign(11,100);
    w.add_combat_unit(7,8,0,tank);w.add_combat_unit(6,8,0,tank);w.add_combat_unit(12,8,1,unit("TARGET"));
    CHECK(w.can_occupy(0,0));REQUIRE(w.campaign_occupy_now(0,0));
    CHECK_FALSE(w.can_occupy(1,0));CHECK(w.units()[0].garrison_of==0);
    w.buildings()[0].garrison_target=2;w.step(1.0/15.0);
    CHECK(w.units()[2].hp==74);CHECK(w.garrison_range(0)==7);
    const auto state=save::save_world(w);World loaded(r.map);REQUIRE(save::load_world(loaded,state));
    CHECK(loaded.units()[0].garrison_of==0);CHECK(loaded.btypes()[0].tank_bunker);
    w.unload_occupants(0);CHECK(w.units()[0].cargo_of<0);CHECK(w.units()[0].alive);CHECK(w.buildings()[0].house==0);
    w.units()[1].w7.t.bunkerable=false;CHECK_FALSE(w.can_occupy(1,0));
    w.units()[1].w7.t.bunkerable=true;w.units()[1].house=1;CHECK_FALSE(w.can_occupy(1,0));
}
TEST_CASE("Yuri core: a controlled tank retains its link inside a bunker and leaves on controller loss") {
    Rig r;World w(r.map);BuildingTypeDef b;b.id="NATBNK";b.tank_bunker=true;
    b.can_be_occupied=b.can_occupy_fire=true;b.max_occupants=1;b.strength=1000;
    w.set_economy({b},{},OreValues{},1000,0,0);w.add_building(0,8,8,1,1,0,1000,0,0,true);
    auto psychic=unit("YURI");psychic.w7.mind_control=true;w.add_combat_unit(2,2,0,psychic);
    auto tank=unit("LTNK");tank.is_vehicle=true;tank.w7.bunkerable=true;w.add_combat_unit(7,8,1,tank);
    w.units()[0].w7.mc_shot=1;w.resolve_skirmish_mind_control();REQUIRE(w.campaign_occupy_now(1,0));
    w.prepare_skirmish_mind_control();CHECK(w.units()[1].house==0);CHECK(w.units()[0].w7.mc_victims.size()==1);
    w.units()[0].alive=false;w.step(1.0/15.0);w.step(1.0/15.0);
    CHECK(w.units()[1].house==1);CHECK(w.units()[1].cargo_of<0);CHECK(w.buildings()[0].house==0);
}
TEST_CASE("Yuri core: magnetic locomotor damage never becomes an ordinary lethal shot") {
    Rig r;World w(r.map);auto c=unit("TELE");c.w7.magnetic=true;c.damage=5000;c.range=10;c.rof=1;
    w.add_combat_unit(2,2,0,c);w.add_combat_unit(4,2,1,unit("TANK"));w.units()[0].target=1;w.units()[0].attack_order=true;
    for(int i=0;i<30;++i)w.step(1.0/15.0);
    CHECK(w.units()[1].hp==100);CHECK(w.units()[1].alive);
    w.order_attack_cell(0,4,2);for(int i=0;i<30;++i)w.step(1.0/15.0);
    CHECK(w.units()[1].hp==100);
    w.units()[0].w2.valid=true;w.units()[0].w2.damage=80;w.units()[0].w2.ag=true;
    CHECK(unit_weapon_for_building(w.units()[0],0)==1);
    w.units()[0].w2.range=10;w.units()[0].w2.rof=10;w.units()[0].w2.verses.assign(11,100);
    BuildingTypeDef target;target.id="TARGET";target.strength=200;
    w.set_economy({target},{},OreValues{},1000,0,0);w.add_building(0,5,5,1,1,1,200,0,0,true);
    w.order_unit_attack_building(0,0);
    for(int i=0;i<30 && w.buildings()[0].hp==200;++i)w.step(1.0/15.0);
    CHECK(w.buildings()[0].hp==120);
}
TEST_CASE("Yuri core: rules resolve bunker eligibility, magnetic behavior and overload tables") {
    const auto ini=ra2assets::Ini::parse(
        "[CombatDamage]\nOverloadCount=3,6,10\nOverloadDamage=0,50,100\nOverloadFrames=30,60,60\nBunkerWeaponRangeBonus=4\n"
        "[VehicleTypes]\n0=LTNK\n1=TELE\n2=MIND\n[LTNK]\nTurret=yes\nPrimary=Gun\n[TELE]\nTurret=yes\nBunkerable=no\nPrimary=Beam\n"
        "[MIND]\nPrimary=MC\n[NATBNK]\nBunker=yes\n[Gun]\nDamage=65\nROF=10\nRange=5\n[Beam]\nDamage=5000\nWarhead=Lift\n"
        "[Lift]\nIsLocomotor=yes\n[MC]\nDamage=3\nInfiniteMindControl=yes\nWarhead=Control\n[Control]\nMindControl=yes\n");
    const auto rules=ra2assets::Rules::load(ini);UnitCombat tank,mag,mind;
    fill_unit_combat_extras(rules,"LTNK",resolve_type_weapons(rules,"LTNK"),tank);
    fill_unit_combat_extras(rules,"TELE",resolve_type_weapons(rules,"TELE"),mag);
    fill_unit_combat_extras(rules,"MIND",resolve_type_weapons(rules,"MIND"),mind);
    CHECK(tank.w7.bunkerable);CHECK(tank.occupy_weapon.damage==65);CHECK_FALSE(mag.w7.bunkerable);CHECK(mag.w7.magnetic);
    CHECK(mind.w7.mc_capacity==3);CHECK(mind.w7.mc_infinite);CHECK(mind.w7.overload_damage==std::vector<int>{0,50,100});
    BuildingTypeDef b;fill_building_weapons(rules,"NATBNK",b);
    CHECK(b.tank_bunker);CHECK(b.can_be_occupied);CHECK(b.max_occupants==1);CHECK(b.bunker_range==4);
}
TEST_CASE("Yuri core: firing report loop has one owner and stops on stop or distance loss") {
    struct Audio {
        int starts=0,stops=0,oneshots=0;bool audible=true;
        bool sound_loops(const std::string& key){return key=="Gattling";}
        unsigned loop_at(const std::string&,double,double){return ++starts;}
        void stop_loop(unsigned){++stops;}
        bool update_loop(unsigned,double,double){return audible;}
        void play_at(const std::string&,double,double,const char*){++oneshots;}
    } audio;
    WeaponSoundLoop loop;
    for(int i=0;i<100;++i){loop.shot(audio,"Gattling",2,2);loop.update(audio,true,2,2);}
    CHECK(audio.starts==1);loop.update(audio,false,2,2);CHECK(audio.stops==1);CHECK(loop.handle==0);
    loop.shot(audio,"Gattling",2,2);CHECK(audio.starts==2);audio.audible=false;loop.update(audio,true,2,2);CHECK(loop.handle==0);
    loop.update(audio,true,2,2);CHECK(audio.starts==2);
    audio.audible=true;loop.shot(audio,"Gattling",2,2);loop.shot(audio,"Cannon",2,2);
    CHECK(audio.stops==2);CHECK(audio.oneshots==1);CHECK(loop.handle==0);
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

TEST_CASE("Yuri core: mobile miner seeks ore and explicit Stop persists through save") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;d.slave_count=2;
    d.slave_combat=unit("SLAV");
    w.set_economy({d},{0},OreValues{},1000,0,0);
    auto miner=unit("SMIN");miner.deploys_into="YAREFN";
    w.set_undeploy_unit(0,miner);
    w.add_combat_unit(2,2,0,miner);
    w.ore().set(17,17,OreKind::Ore,5,0);
    w.step_yuri_workers();
    REQUIRE(w.units()[0].moving());CHECK(w.units()[0].path.back().x==17);
    w.order_stop(0);CHECK(w.units()[0].manual_hold);
    w.step_yuri_workers();CHECK_FALSE(w.units()[0].moving());
    World restored(r.map);REQUIRE(save::load_world(restored,save::save_world(w)));
    CHECK(restored.units()[0].manual_hold);
    w.order_unit_move(0,3,3);CHECK_FALSE(w.units()[0].manual_hold);
}

TEST_CASE("Yuri core: building-strip miner relocates only after its local ore is gone") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;d.strength=100;d.slave_count=2;
    d.slave_combat=unit("SLAV");d.slave_combat.is_infantry=true;
    w.set_economy({d},{0},OreValues{},1000,0,0);
    auto miner=unit("SMIN");miner.deploys_into="YAREFN";
    w.set_undeploy_unit(0,miner);
    w.add_building(0,2,2,2,2,0,100,0,0,true);
    w.ore().set(5,5,OreKind::Ore,5,0);
    w.ore().set(18,18,OreKind::Ore,5,0);
    w.step_yuri_workers();CHECK(w.buildings()[0].alive);REQUIRE(w.units().size()==2);
    w.ore().set(5,5,OreKind::None,0,0);
    w.step_yuri_workers();CHECK(w.buildings()[0].state==Building::State::Dead);REQUIRE(w.units().size()==3);
    CHECK(w.units()[2].image=="SMIN");REQUIRE(w.units()[2].moving());
    CHECK(w.units()[2].path.back().x==18);CHECK(w.units()[0].w7.slave_manager==-4);
}

TEST_CASE("garrison: other passive owners transfer control and infantry evacuate alive") {
    Rig r;World w(r.map);
    w.set_house(0,{World::Side::Allied,0,"Player",0,true});
    w.set_house(2,{World::Side::Neutral,2,"Neutral",2,false});
    w.set_house(3,{World::Side::Neutral,6,"Civilian",3,false});
    BuildingTypeDef d;d.id="CIVILIAN";d.fw=d.fh=2;d.strength=100;
    d.can_be_occupied=true;d.max_occupants=5;
    w.set_building_types({d});
    const int bi=w.add_building(0,8,8,2,2,3,100,0,0,true);
    auto infantry=unit("GI");infantry.is_infantry=true;infantry.occupier=true;
    w.add_combat_unit(7,8,0,infantry);
    REQUIRE(w.campaign_occupy_now(0,bi));CHECK(w.buildings()[bi].house==0);
    w.unload_occupants(bi);
    CHECK(w.buildings()[bi].occupants.empty());CHECK(w.units()[0].alive);
    CHECK(w.units()[0].garrison_of==-1);CHECK(w.units()[0].cargo_of==-1);
    CHECK(w.units()[0].house==0);CHECK(w.buildings()[bi].house==2);
}

TEST_CASE("Battle Bunker: empty and manually evacuated structure keeps its owner and repair access") {
    Rig r;World w(r.map);
    w.set_house(0,{World::Side::Soviet,0,"Player",0,true});
    w.set_house(2,{World::Side::Neutral,2,"Neutral",2,false});
    BuildingTypeDef d;d.id="NABNKR";d.strength=600;d.can_be_occupied=true;
    d.max_occupants=5;d.click_repairable=true;
    w.set_building_types({d});
    const int bi=w.add_building(0,8,8,1,1,0,600,0,0,true);
    auto c=unit("CONS");c.is_infantry=true;c.occupier=true;
    w.add_combat_unit(7,8,0,c);
    REQUIRE(w.campaign_occupy_now(0,bi));
    w.unload_occupants(bi);w.step(1.0/15.0);
    CHECK(w.buildings()[bi].occupants.empty());
    CHECK(w.buildings()[bi].house==0);
    w.buildings()[bi].hp=1;
    CHECK(w.set_repair(bi,0,1));
    CHECK(w.buildings()[bi].being_repaired);
    CHECK_FALSE(w.set_repair(bi,2,1));
}

TEST_CASE("Battle Bunker: red damage retains infantry while civilian garrisons evacuate") {
    for(bool military:{false,true}) {
        Rig r;World w(r.map);
        w.set_house(0,{World::Side::Soviet,0,"Player",0,true});
        w.set_house(2,{World::Side::Neutral,2,"Neutral",2,false});
        BuildingTypeDef d;d.id=military?"NABNKR":"CIVILIAN";d.strength=600;
        d.can_be_occupied=true;d.max_occupants=5;
        w.set_building_types({d});const int bi=w.add_building(0,8,8,1,1,0,600,0,0,true);
        auto c=unit("CONS");c.is_infantry=true;c.occupier=true;
        w.add_combat_unit(7,8,0,c);REQUIRE(w.campaign_occupy_now(0,bi));
        w.buildings()[bi].hp=100;w.step(1.0/15.0);
        CHECK(w.buildings()[bi].occupants.empty()==!military);
        CHECK(w.units()[0].alive);
        CHECK((w.units()[0].garrison_of==bi)==military);
        CHECK(w.buildings()[bi].house==(military?0:2));
        World restored(r.map);
        REQUIRE(save::load_world(restored,save::save_world(w)));
        for(int frame=0;frame<120;++frame) {
            w.step(1.0/15.0);restored.step(1.0/15.0);
        }
        CHECK(save::save_world(restored)==save::save_world(w));
    }
}

TEST_CASE("Yuri core: Psychic Reveal maps only its area and fog sight is temporary") {
    Rig r;World w(r.map);w.enable_shroud(true);w.set_fog_of_war(true);
    std::array<int,11> verses{};verses.fill(100);
    w.grant_superweapon_type(0,SuperweaponType::PsychicReveal,0,false,0,2,0,verses);
    w.step(1.0/15.0);
    REQUIRE(w.sw_ready(0,SuperweaponType::PsychicReveal));
    CHECK_FALSE(w.explored_at(0,10,10));
    REQUIRE(w.fire_superweapon_at(0,SuperweaponType::PsychicReveal,10,10));
    CHECK(w.explored_at(0,10,10));CHECK(w.visible_at(0,10,10));
    CHECK_FALSE(w.explored_at(0,0,0));CHECK_FALSE(w.explored_at(1,10,10));
    w.step(1.0/15.0);
    CHECK(w.explored_at(0,10,10));CHECK_FALSE(w.visible_at(0,10,10));
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

TEST_CASE("Slave Miner move command: owner packs and drives; enemy cannot convert building") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;
    d.strength=200;d.slave_count=2;d.slave_combat=unit("SLAV");
    w.set_economy({d},{0},OreValues{},1000,0,0);
    auto mobile=unit("SMIN");mobile.deploys_into="YAREFN";mobile.rot=8;
    w.set_undeploy_unit(0,mobile);w.add_building(0,8,8,2,2,0,200,0,0,true);
    w.buildings()[0].hp=100;w.step_yuri_workers();const int index=int(w.units().size());
    auto cmd=make_mega(w,{},Mission::Move,Target{},Target::cell(14,14));
    cmd.whom.push_back(Target::building(0));cmd.house=1;apply_command(w,cmd);
    REQUIRE(w.buildings()[0].alive);CHECK(w.units().size()==index);
    World restored(r.map);REQUIRE(save::load_world(restored,save::save_world(w)));
    cmd.house=0;apply_command(w,cmd);apply_command(restored,cmd);
    REQUIRE(w.units().size()==index+1);CHECK(w.buildings()[0].state==Building::State::Dead);
    CHECK(w.units()[index].hp==50);REQUIRE(w.units()[index].moving());
    CHECK(w.units()[index].path.back().x==14);CHECK(w.units()[index].path.back().y==14);
    for(int t=0;t<180;++t){w.step(1.0/15.0);restored.step(1.0/15.0);}
    CHECK(save::save_world(w)==save::save_world(restored));
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
    w.units()[0].attack_order = true;
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

TEST_CASE("Slave Miner deploy: friendly blockers walk out before conversion; no teleport") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;d.strength=1000;d.slave_count=5;
    w.set_building_types({d});auto miner=unit("SMIN");miner.is_vehicle=true;miner.deploys_into="YAREFN";
    w.add_combat_unit(8,8,0,miner);w.add_combat_unit(9,8,0,unit("FRIEND"));
    REQUIRE_FALSE(w.can_deploy_at(0,8,8));REQUIRE(w.order_deploy_mcv(0));
    CHECK(w.units()[1].cell_x==9);CHECK(w.units()[1].cell_y==8);CHECK(w.units()[1].fx==9);
    CHECK(w.units()[1].moving());CHECK(w.units()[0].deploy_mission==1);
    for(int i=0;i<240 && w.buildings().empty();++i)w.step(1.0/15.0);
    REQUIRE(w.buildings().size()==1);CHECK(w.buildings()[0].cx==8);CHECK(w.buildings()[0].cy==8);
    CHECK_FALSE((w.units()[1].cell_x>=8 && w.units()[1].cell_x<10 && w.units()[1].cell_y>=8 && w.units()[1].cell_y<10));
    World restored(r.map);REQUIRE(save::load_world(restored,save::save_world(w)));
    for(int i=0;i<90;++i){w.step(1.0/15.0);restored.step(1.0/15.0);}
    CHECK(save::save_world(w)==save::save_world(restored));
}
TEST_CASE("Slave Miner deploy: enemy and static blockers are not pushed by footprint clearing") {
    for(bool enemy:{false,true}) {
        Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;d.strength=1000;d.slave_count=5;
        w.set_building_types({d});auto m=unit("SMIN");m.is_vehicle=true;m.deploys_into="YAREFN";
        w.add_combat_unit(8,8,0,m);w.add_combat_unit(9,8,enemy?1:0,unit("BLOCKER"));
        if(!enemy)w.units()[1].frozen=true;
        (void)w.order_deploy_mcv(0);
        CHECK_FALSE(w.units()[1].moving());CHECK(w.units()[1].cell_x==9);CHECK(w.units()[1].cell_y==8);
        CHECK_FALSE(w.can_deploy_at(0,8,8));
    }
}
TEST_CASE("bridge deck speed: Road on deck, river below, no fictitious hill at bridge end") {
    Rig r;World w(r.map);const int W=w.grid_w(),H=w.grid_h();
    std::vector<uint8_t> hb(W*H,0);hb[8*W+8]=1;w.set_high_bridges(hb);
    std::vector<uint8_t> land(W*H,0);land[8*W+8]=2;
    std::vector<int8_t> levels(W*H,4);levels[8*W+8]=0;
    MoveSpeedCtx c;c.armed=true;c.land=&land;c.level=&levels;c.world=&w;
    c.cost[0][1]=1;c.cost[1][1]=0.9f;c.cost[2][1]=0;c.cost[2][5]=1;
    c.up[0]=0.25;c.down[0]=0.5;
    Unit u;u.is_vehicle=true;u.land_speed_loco=true;u.speed_type=1;u.max_hp=u.hp=100;
    u.cell_x=7;u.cell_y=8;
    CHECK(land_speed_mult(c,u,W,H,8,8)==doctest::Approx(0.9));
    u.cell_x=8;u.on_deck=1;
    CHECK(land_speed_mult(c,u,W,H,9,8)==doctest::Approx(1.0));
    u.on_deck=0;u.is_naval=true;u.speed_type=5;u.movement_domain=MovementDomain::Water;
    levels[8*W+7]=0;u.cell_x=7;
    CHECK(land_speed_mult(c,u,W,H,8,8)==doctest::Approx(1.0));
}
TEST_CASE("bridge traversal: infantry and tracked units cross both directions on legal height layer") {
    for(bool vehicle:{false,true})for(bool reverse:{false,true}) {
        Rig r;
        // An unparsed synthetic map uses the linear lookup, not parse()'s
        // immutable coordinate index (the Rig appends cells after parsing).
        r.map=ra2assets::MapFile{};
        auto& cells=const_cast<std::vector<ra2assets::MapCell>&>(r.map.cells());
        for(int y=0;y<40;++y)for(int x=0;x<40;++x)cells.push_back({x,y,0,0,0});
        for(auto& cell:const_cast<std::vector<ra2assets::MapCell>&>(r.map.cells()))cell.level=(cell.x>=18 && cell.x<=21)?0:4;
        REQUIRE(r.map.at(17,18));REQUIRE(r.map.at(22,18));
        World w(r.map);const int gw=w.grid_w(),gh=w.grid_h();
        std::vector<ra2assets::LandType> land(gw*gh,ra2assets::LandType::Clear);
        std::vector<uint8_t> hb(gw*gh,0),speed(gw*gh,0),ramp(gw*gh,0);
        for(int y=0;y<gh;++y)for(int x=18;x<=21;++x){land[y*gw+x]=ra2assets::LandType::Water;speed[y*gw+x]=2;}
        w.set_land_terrain(land);for(int x=18;x<=21;++x)hb[18*gw+x]=1;w.set_high_bridges(hb);
        World::LandSpeedRules rules;w.arm_land_speed(speed,ramp,rules);
        auto mover=unit(vehicle?"SMCV":"GI");mover.is_vehicle=vehicle;mover.speed_cps=4;
        mover.land_speed_loco=vehicle;mover.speed_type=1;mover.rot=8;
        w.add_combat_unit(reverse?24:15,18,0,mover);w.order_unit_move(0,reverse?15:24,18);
        REQUIRE(w.units()[0].moving());bool saw_deck=false;
        for(int t=0;t<500 && w.units()[0].moving();++t){w.step(1.0/15.0);saw_deck=saw_deck || w.units()[0].on_deck;}
        CHECK(saw_deck);CHECK(w.units()[0].cell_x==(reverse?15:24));CHECK(w.units()[0].on_deck==0);
    }
}
TEST_CASE("Gatling loop: keep audio over ROF gaps, stop on target loss and charge exhaustion") {
    CHECK(weapon_loop_active(true,true,0,true,200));
    CHECK_FALSE(weapon_loop_active(true,true,0,true,0));
    CHECK_FALSE(weapon_loop_active(true,false,0,true,200));
    CHECK_FALSE(weapon_loop_active(false,true,0,true,200));
    CHECK_FALSE(weapon_loop_active(true,true,0,false,200));
    CHECK(weapon_loop_active(true,true,0.2,false,0));
}
TEST_CASE("AI plan: TECH group recognizes the existing side-specific tech laboratory") {
    std::vector<BuildingTypeDef> defs(5);defs[0].id="YACNST";defs[1].id="YAPOWR";
    defs[2].id="YATECH";defs[3].id="SPECIAL";defs[3].prerequisite={"TECH"};
    defs[4].id="YABRCK";
    AiCatalog cat;cat.conyard=0;cat.power=1;cat.tech=2;cat.barracks=4;cat.defenses={3};
    AiRules rules;rules.extra_refineries=0;DifficultyMods mods;mods.defense_target=1;
    std::uint32_t rng=123;
    const auto plan=build_base_plan(BaseView{},cat,defs,rules,mods,&rng);
    int tech=-1,special=-1;for(int i=0;i<int(plan.size());++i){if(plan[i].type==2)tech=i;if(plan[i].type==3)special=i;}
    REQUIRE(tech>=0);REQUIRE(special>=0);CHECK(tech<special);
}
TEST_CASE("Flying Disc retail weapon: anti-air remains available after a secondary selection") {
    if(!ra2fixture::assets_available())return;
    const auto& rules=ra2fixture::rulesmd();const auto tw=resolve_type_weapons(rules,"DISK");
    REQUIRE(tw.slot[0].aa);REQUIRE_FALSE(tw.slot[1].aa);
    Rig r;World w(r.map);auto disk=unit("DISK");disk.is_vehicle=disk.is_aircraft=true;
    disk.damage=90;disk.range=7;disk.rof=80;disk.verses=tw.slot[0].verses;disk.proj=tw.slot[0].proj;
    fill_unit_combat_extras(rules,"DISK",tw,disk);
    w.add_combat_unit(2,2,0,disk);auto target=unit("ROCK");target.is_aircraft=true;w.add_combat_unit(4,2,1,target);
    w.units()[1].altitude=300;w.units()[1].flight_state=FlightState::Cruising;
    CHECK(unit_weapon_for_unit(w.units()[0],w.units()[1])==0);
    activate_weapon_slot(w.units()[0],1,nullptr);CHECK_FALSE(w.units()[0].w_aa);
    CHECK(unit_weapon_for_unit(w.units()[0],w.units()[1])==0);
}

TEST_CASE("Magnetron retail audit: building weapon damages structures, infantry remains ineligible") {
    if(!ra2fixture::assets_available())return;
    const auto& rules=ra2fixture::rulesmd();const auto tw=resolve_type_weapons(rules,"TELE");
    REQUIRE(tw.slot[1].valid);CHECK(tw.slot[1].damage==80);
    Rig r;World w(r.map);BuildingTypeDef d;d.id="TARGET";d.strength=1000;d.armor_ix=6;w.set_building_types({d});
    w.add_building(0,7,2,1,1,1,1000,6,0,true);
    auto magnet=unit("TELE");magnet.is_vehicle=true;magnet.w7.magnetic=true;magnet.rot=8;
    magnet.damage=5000;magnet.range=12;magnet.rof=20;magnet.verses=tw.slot[0].verses;magnet.omni_fire=true;
    fill_unit_combat_extras(rules,"TELE",tw,magnet);w.add_combat_unit(2,2,0,magnet);
    REQUIRE(unit_weapon_for_building(w.units()[0],6)==1);
    w.units()[0].target_building=0;w.units()[0].attack_order=true;
    for(int t=0;t<180;++t)w.step(1.0/15.0);
    INFO("slot="<<w.units()[0].w_active<<" damage="<<w.units()[0].w_damage<<" target="<<w.units()[0].target_building<<" range="<<w.units()[0].w_range<<" rearm="<<w.units()[0].rearm_frames);
    CHECK(w.buildings()[0].hp<1000);CHECK(w.buildings()[0].hp>0);
    Unit infantry;infantry.armor_ix=0;infantry.is_infantry=true;
    CHECK(unit_weapon_for_unit(w.units()[0],infantry)<0);
}

TEST_CASE("deploy feedback: automatic attempts stay quiet across save while manual failures report") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.fw=d.fh=2;d.slave_count=5;
    w.set_building_types({d});auto m=unit("SMIN");m.deploys_into="YAREFN";
    w.add_combat_unit(8,8,0,m);REQUIRE(w.order_deploy_mcv_auto(0));
    CHECK(w.units()[0].deploy_mission==3);
    World loaded(r.map);REQUIRE(save::load_world(loaded,save::save_world(w)));
    for(auto* world:{&w,&loaded}) {
        world->add_building(0,8,8,2,2,1,100,0,0,true);world->step(1.0/15.0);
        const auto events=world->take_deploy_events();
        CHECK(std::none_of(events.begin(),events.end(),[](const auto& e){return e.kind==World::DeployEvent::Kind::CannotDeploy;}));
        CHECK(world->units()[0].deploy_mission==0);
    }
    CHECK(save::save_world(w)==save::save_world(loaded));
    // Cover the entire relocation ring: a manual attempt must still report failure.
    for(int y=3;y<14;++y)for(int x=3;x<14;++x)if(w.building_at_cell(x,y)<0)w.add_building(0,x,y,1,1,1,100,0,0,true);
    CHECK_FALSE(w.order_deploy_mcv(0));const auto failed=w.take_deploy_events();
    REQUIRE(failed.size()==1);CHECK(failed[0].kind==World::DeployEvent::Kind::CannotDeploy);
    CHECK_FALSE(w.order_deploy_mcv_auto(0));CHECK(w.take_deploy_events().empty());
}
TEST_CASE("ConYard deploy command: all faction mappings obey option ownership and blockers") {
    for(const auto& name:{"GACNST","NACNST","YACNST"})for(bool enabled:{false,true}) {
        Rig r;World w(r.map);BuildingTypeDef d;d.id=name;d.fw=d.fh=4;d.strength=1000;d.conyard=true;
        w.set_building_types({d});auto m=unit("MCV");m.deploys_into=name;w.set_undeploy_unit(0,m);
        w.set_skirmish_session(true);w.set_mcv_redeploys(enabled);
        w.add_building(0,8,8,4,4,0,1000,0,0,true);w.buildings()[0].hp=500;
        auto c=make_building_event(EventType::Deploy,0);c.house=1;apply_command(w,c);CHECK(w.units().empty());
        c.house=0;apply_command(w,c);
        if(!enabled){CHECK(w.units().empty());CHECK(w.buildings()[0].alive);continue;}
        REQUIRE(w.units().size()==1);CHECK(w.units()[0].hp==50);CHECK(w.units()[0].deploys_into==0);
        CHECK(w.buildings()[0].state==Building::State::Dead);
    }
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YACNST";d.fw=d.fh=4;d.conyard=true;
    w.set_building_types({d});w.set_undeploy_unit(0,unit("PCV"));w.set_skirmish_session(true);w.set_mcv_redeploys(true);
    w.add_building(0,8,8,4,4,0,100,0,0,true);w.add_combat_unit(9,9,0,unit("BLOCKER"));
    CHECK_FALSE(w.can_pack_conyard(0));CHECK_FALSE(w.order_pack_conyard(0,0));CHECK(w.buildings()[0].alive);
}
TEST_CASE("ground presentation: legal slope Z interpolates and bridge deck cannot overpaint own tank") {
    for(int i=0;i<=10;++i){const double t=i/10.0;
        CHECK(segment_ground_level(8+t,8,8,8,9,8,2,3)==doctest::Approx(2+t));
        CHECK(segment_ground_level(9-t,8,9,8,8,8,3,2)==doctest::Approx(3-t));
        CHECK(segment_ground_level(8+t,8,8,8,9,8,4,4)==4);
    }
    CHECK(deck_unit_depth(7.7,8,8,8,true)>16.99);
    CHECK(deck_unit_depth(7.7,8,8,8,false)==15.7);
}
TEST_CASE("Slave followers: packed workers use distinct reachable neighbours and deterministic continuation") {
    Rig r;World w(r.map);BuildingTypeDef d;d.id="YAREFN";d.slave_count=4;d.fw=d.fh=2;
    d.slave_combat=unit("SLAV");w.set_economy({d},{0},OreValues{},1000,0,0);
    auto m=unit("SMIN");m.deploys_into="YAREFN";w.set_undeploy_unit(0,m);
    w.add_building(0,4,4,2,2,0,100,0,0,true);w.step_yuri_workers();REQUIRE(w.order_pack_slave_miner(0,0));
    const int mobile=int(w.units().size())-1;w.order_unit_move(mobile,13,13);
    World loaded(r.map);REQUIRE(save::load_world(loaded,save::save_world(w)));
    for(int t=0;t<450;++t){w.step(1.0/15.0);loaded.step(1.0/15.0);}
    CHECK(save::save_world(w)==save::save_world(loaded));
    for(int i=0;i<mobile;++i){CHECK(w.units()[i].w7.slave_manager==-2-mobile);
        CHECK(std::max(std::abs(w.units()[i].cell_x-w.units()[mobile].cell_x),std::abs(w.units()[i].cell_y-w.units()[mobile].cell_y))<=2);}
}

TEST_CASE("retail Boomer prerequisites: yard and radar required; Battle Lab is not required") {
    if(!ra2fixture::assets_available())return;
    const auto& rules=ra2fixture::rulesmd();const auto required=rules.prerequisite("BSUB");
    CHECK(required==std::vector<std::string>{"YAYARD","RADAR"});CHECK(rules.section_double("BSUB","TechLevel",-1)==2);
    std::vector<BuildingTypeDef> defs(3);defs[0].id="YAYARD";defs[1].id="NAPSIS";defs[2].id="YATECH";
    std::vector<std::pair<std::string,std::vector<std::string>>> groups{{"RADAR",rules.prerequisite_group("RADAR")}};
    std::vector<Building> buildings(3);for(int i=0;i<3;++i){buildings[i].type=i;buildings[i].house=0;buildings[i].alive=true;buildings[i].state=Building::State::Idle;}
    buildings[2].alive=false;CHECK(prerequisites_met(required,buildings,defs,0,groups));
    buildings[0].alive=false;CHECK_FALSE(prerequisites_met(required,buildings,defs,0,groups));
    buildings[0].alive=true;buildings[1].house=1;CHECK_FALSE(prerequisites_met(required,buildings,defs,0,groups));
    buildings[1].house=0;buildings[1].state=Building::State::Dead;CHECK_FALSE(prerequisites_met(required,buildings,defs,0,groups));
}
