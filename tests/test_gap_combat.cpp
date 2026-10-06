// Gap fix C (combat / units / movement) -- unit tests for the pure pieces and
// the real-rulesmd resolution. The real-play proof (worlds built through
// launch_skirmish + the GameScene wiring) is `ra2engine --check-gap-combat`.
#include "doctest.h"

#include <cmath>

#include "asset_fixture.hpp"
#include "combat.hpp"
#include "weapon.hpp"
#include "weapon_resolve.hpp"
#include "weapon_unit.hpp"
#include "world.hpp"

using namespace ra2engine;

namespace {
WeaponSlot slot(bool aa, bool ag, std::vector<int> verses = {}) {
    WeaponSlot s;
    s.valid = true;
    s.aa = aa;
    s.ag = ag;
    s.verses = std::move(verses);
    return s;
}
}  // namespace

TEST_CASE("gapC weapon: choose_weapon = primary if it can engage, else secondary (G-41/G-44)") {
    const WeaponSlot ground = slot(false, true);
    const WeaponSlot aa = slot(true, false);
    CHECK(choose_weapon(ground, aa, /*air=*/false, 0) == 0);
    CHECK(choose_weapon(ground, aa, /*air=*/true, 0) == 1);
    CHECK(choose_weapon(ground, WeaponSlot{}, /*air=*/true, 0) == -1);
    // 0 % Verses forbids the shot (GetFireError, combat.md §2).
    const WeaponSlot no_vs_heavy = slot(false, true, {100, 100, 100, 100, 100, 0});
    CHECK(choose_weapon(no_vs_heavy, WeaponSlot{}, false, 5) == -1);
    CHECK(choose_weapon(no_vs_heavy, ground, false, 5) == 1);
}

TEST_CASE("gapC weapon: real rulesmd slots (SREF Weapon1, APOC/HTK secondary AA, FV no secondary)") {
    if (!ra2fixture::assets_available()) { MESSAGE("no install"); return; }
    const ra2assets::Rules& r = ra2fixture::rulesmd();
    const TypeWeapons sref = resolve_type_weapons(r, "SREF");
    CHECK(sref.slot[0].id == "Comet");
    CHECK(sref.slot[0].damage > 0);
    CHECK(sref.slot[0].range > 0.0);
    CHECK(sref.elite[0].id == "SuperComet");
    CHECK_FALSE(sref.slot[1].valid);
    const TypeWeapons htk = resolve_type_weapons(r, "HTK");
    CHECK(htk.slot[0].id == "FlakTrackGun");
    CHECK(htk.slot[1].id == "FlakTrackAAGun");
    CHECK(htk.slot[1].aa);
    CHECK_FALSE(htk.slot[0].aa);
    const TypeWeapons apoc = resolve_type_weapons(r, "APOC");
    CHECK(apoc.slot[1].id == "MammothTusk");
    CHECK(apoc.slot[1].aa);
    const TypeWeapons fv = resolve_type_weapons(r, "FV");
    CHECK(fv.slot[0].id == "HoverMissile");
    CHECK_FALSE(fv.slot[1].valid);   // Weapon2=RepairBullet is an IFV mode, not a secondary
    const TypeWeapons gi = resolve_type_weapons(r, "E1");
    CHECK(gi.slot[0].id == "M60");
    CHECK(gi.slot[1].id == "Para");
    CHECK(gi.elite[1].id == "ParaE");
    const TypeWeapons ggi = resolve_type_weapons(r, "GGI");
    CHECK(ggi.slot[1].id == "MissileLauncher");   // ";GEF New Guardian GI weapon" cut (RC-5)
    CHECK(ggi.slot[1].aa);
    UnitCombat c;
    fill_unit_combat_extras(r, "E1", gi, c);
    CHECK(c.deploy_fire);
    CHECK(c.deploy_fire_slot == 1);
    CHECK(c.crushable);
    UnitCombat d;
    fill_unit_combat_extras(r, "DESO", resolve_type_weapons(r, "DESO"), d);
    CHECK_FALSE(d.deploy_fire);   // RadEruptionWeapon has RadLevel -> P4 eruption
    UnitCombat m;
    fill_unit_combat_extras(r, "MTNK", resolve_type_weapons(r, "MTNK"), m);
    CHECK(m.crusher);
}

TEST_CASE("gapC weapon: a Flak Track engages an airborne Kirov with its AA secondary") {
    ra2assets::MapFile map;
    World w(map);
    UnitCombat flak;
    flak.strength = 300;
    flak.damage = 25;
    flak.rof = 25;
    flak.range = 5.0;
    flak.secondary = slot(true, false);
    flak.secondary.damage = 30;
    flak.secondary.rof = 20;
    flak.secondary.range = 8.0;
    Unit& f = w.add_combat_unit(2, 2, 0, flak);
    f.vet.base.range = 5.0;
    UnitCombat zep;
    zep.strength = 2000;
    zep.is_aircraft = true;
    Unit& k = w.add_combat_unit(8, 2, 1, zep);
    k.altitude = 500.0;
    k.flight_state = FlightState::Cruising;
    Unit& fu = w.units()[0];
    const Unit& ku = w.units()[1];
    CHECK(unit_weapon_for_unit(fu, ku) == 1);
    activate_weapon_slot(fu, 1, nullptr);
    CHECK(fu.w_range == 8.0);
    CHECK(fu.w_damage == 30);
    activate_weapon_slot(fu, 0, nullptr);
    CHECK(fu.w_range == 5.0);
    CHECK(fu.w_damage == 25);
}

TEST_CASE("gapC deploy: a GI deploys into a firing post with Para, not a frozen eruption (G-45)") {
    ra2assets::MapFile map;
    World w(map);
    UnitCombat gi;
    gi.strength = 125;
    gi.damage = 15;
    gi.rof = 20;
    gi.range = 4.0;
    gi.secondary = slot(false, true);
    gi.secondary.damage = 25;
    gi.secondary.rof = 15;
    gi.secondary.range = 5.0;
    gi.deploy_fire = true;
    gi.deploy_fire_slot = 1;
    w.add_combat_unit(2, 2, 0, gi);
    UnitCombat e2;
    e2.strength = 125;
    w.add_combat_unit(5, 2, 1, e2);
    CHECK(unit_weapon_for_unit(w.units()[0], w.units()[1]) == 0);
    REQUIRE(w.order_deploy(0));
    CHECK(w.units()[0].deployed);
    CHECK_FALSE(w.units()[0].frozen);
    CHECK(unit_weapon_for_unit(w.units()[0], w.units()[1]) == 1);
    // It keeps fighting from the post: step until the Para shot lands.
    for (int f = 0; f < 30; ++f) w.step(1.0 / 15.0);
    CHECK(w.units()[1].hp < 125);
    CHECK(w.units()[0].w_active == 1);
    CHECK(w.units()[0].cell_x == 2);
    // A move order packs it up.
    w.order_unit_move(0, 2, 4);
    CHECK_FALSE(w.units()[0].deployed);
}

TEST_CASE("gapC defenses: a pillbox acquires, turns and kills; Powered goes offline at low power (G-40)") {
    ra2assets::MapFile map;
    World w(map);
    BuildingTypeDef pill;
    pill.id = "GAPILL";
    pill.strength = 400;
    pill.weapon[0] = slot(false, true);
    pill.weapon[0].damage = 20;
    pill.weapon[0].rof = 10;
    pill.weapon[0].range = 5.0;
    pill.armed = true;
    pill.turret = true;
    pill.rot = 10;
    BuildingTypeDef sam = pill;
    sam.id = "NASAM";
    sam.powered = true;
    sam.power = -50;
    sam.turret = false;
    w.set_building_types({pill, sam});
    w.add_building(0, 2, 2, 1, 1, 0, 400, 0, 0.0, /*instant=*/true);
    UnitCombat e2;
    e2.strength = 125;
    w.add_combat_unit(5, 2, 1, e2);
    for (int f = 0; f < 120; ++f) w.step(1.0 / 15.0);
    CHECK(w.units()[0].hp <= 0);
    bool fired = false;
    for (const auto& ev : w.take_fire_events()) fired = fired || ev.building == 0;
    CHECK(fired);

    // Powered defense with no power plant: drain 50 > output 0 -> offline.
    World w2(map);
    w2.set_building_types({pill, sam});
    w2.add_building(1, 2, 2, 1, 1, 0, 400, 0, 0.0, true);
    w2.add_combat_unit(4, 2, 1, e2);
    for (int f = 0; f < 60; ++f) w2.step(1.0 / 15.0);
    CHECK(w2.units()[0].hp == 125);
    CHECK(w2.buildings()[0].def_offline);
    CHECK_FALSE(w2.defense_power_ok(0));
}

TEST_CASE("gapC passability: water / cliff / wall cells close the Land grid, shore stays open (G-43)") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("no install"); return; }
    World w(ra2fixture::mapsmd03_map());
    const int gw = w.grid_w(), gh = w.grid_h();
    REQUIRE(gw > 0);
    // Pick four materialized land cells.
    std::vector<int> picks;
    for (int i = 0; i < gw * gh && picks.size() < 4; ++i)
        if (w.passable()[i]) picks.push_back(i);
    REQUIRE(picks.size() == 4);
    std::vector<ra2assets::LandType> lt(static_cast<std::size_t>(gw) * gh, ra2assets::LandType::Clear);
    lt[picks[0]] = ra2assets::LandType::Water;
    lt[picks[1]] = ra2assets::LandType::Rock;
    lt[picks[2]] = ra2assets::LandType::Wall;
    lt[picks[3]] = ra2assets::LandType::Beach;
    w.set_land_terrain(lt);
    CHECK_FALSE(w.walkable(picks[0] % gw, picks[0] / gw));
    CHECK_FALSE(w.walkable(picks[1] % gw, picks[1] / gw));
    CHECK_FALSE(w.walkable(picks[2] % gw, picks[2] / gw));
    CHECK(w.walkable(picks[3] % gw, picks[3] / gw));
    CHECK(w.grid_for(MovementDomain::Water)[picks[0]] == 1);   // ships still sail it
}

TEST_CASE("gapC G-50: min-1 damage after Verses, 0 % stays 0; ROF rearm jitter is Random(0,2) frames") {
    const std::vector<int> v = {100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};
    CHECK(damage_vs(3, v, 4) == 1);     // 3 x 25 % = 0.75 -> 1 (ReceiveDamage clamp)
    CHECK(damage_vs(15, v, 4) == 3);
    CHECK(damage_vs(0, v, 4) == 0);
    const std::vector<int> z = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(damage_vs(50, z, 3) == 0);
    ProjRng rng;
    int seen[3] = {0, 0, 0};
    for (int i = 0; i < 300; ++i) {
        const double s = rearm_secs(20, &rng);
        const int extra = static_cast<int>(std::lround(s * 15.0)) - 20;
        REQUIRE(extra >= 0);
        REQUIRE(extra <= 2);
        ++seen[extra];
    }
    CHECK(seen[0] > 0);
    CHECK(seen[1] > 0);
    CHECK(seen[2] > 0);
    CHECK(rearm_secs(20, nullptr) == 20.0 / 15.0);
}

TEST_CASE("gapC crush + stop/scatter on the real map (G-47)") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("no install"); return; }
    World w(ra2fixture::mapsmd03_map());
    const ra2assets::MapCell* c = pick_start_cell(ra2fixture::mapsmd03_map());
    REQUIRE(c != nullptr);
    // A free straight 1x6 corridor along +x from the start cell.
    int x0 = c->x, y0 = c->y;
    UnitCombat tank;
    tank.strength = 300;
    tank.is_vehicle = true;
    tank.speed_cps = 2.0;
    tank.rot = 127;
    tank.crusher = true;
    UnitCombat inf;
    inf.strength = 125;
    inf.crushable = true;
    REQUIRE(w.walkable(x0, y0));
    REQUIRE(w.walkable(x0 + 1, y0));
    REQUIRE(w.walkable(x0 + 2, y0));
    w.add_combat_unit(x0, y0, 0, tank);
    w.add_combat_unit(x0 + 1, y0, 1, inf);
    w.order_unit_move(0, x0 + 2, y0);
    for (int f = 0; f < 60; ++f) w.step(1.0 / 15.0);
    CHECK(w.units()[1].die_time >= 0.0);   // squashed
    CHECK(w.units()[0].cell_x == x0 + 2);

    // Stop: the tank halts at the cell it is entering.
    w.order_unit_move(0, x0 + 2, y0 + 6);
    w.step(1.0 / 15.0);
    w.order_stop(0);
    for (int f = 0; f < 30; ++f) w.step(1.0 / 15.0);
    CHECK_FALSE(w.units()[0].moving());
    CHECK(std::abs(w.units()[0].cell_y - y0) <= 1);
    // Scatter: steps to an adjacent free cell.
    const int sx = w.units()[0].cell_x, sy = w.units()[0].cell_y;
    w.order_scatter(0);
    for (int f = 0; f < 30; ++f) w.step(1.0 / 15.0);
    CHECK((w.units()[0].cell_x != sx || w.units()[0].cell_y != sy));
}
