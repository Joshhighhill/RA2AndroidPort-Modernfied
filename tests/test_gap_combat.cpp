// Gap fix C (combat / units / movement) -- unit tests for the pure pieces and
// the real-rulesmd resolution. The real-play proof (worlds built through
// launch_skirmish + the GameScene wiring) is `ra2engine --check-gap-combat`.
#include "doctest.h"

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
