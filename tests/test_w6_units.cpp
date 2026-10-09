// Wave-6 #4 (units and weapons, docs/gap/wave6/04-units.md) -- unit tests for
// the pure pieces and the real-rulesmd resolution.
#include "doctest.h"

#include <cmath>

#include "asset_fixture.hpp"
#include "combat.hpp"
#include "weapon.hpp"
#include "weapon_resolve.hpp"
#include "weapon_unit.hpp"
#include "world.hpp"

using namespace ra2engine;

TEST_CASE("w6units W6-04-01: Burst rearm = Random(3,5) inside a burst, ROF+Random(0,2) after the last shot") {
    // Burst 1 is exactly rearm_secs, with the same RNG draw.
    {
        ProjRng a, b;
        int idx = 0;
        for (int k = 0; k < 50; ++k) {
            CHECK(burst_rearm_secs(idx, 1, 60, &a) == rearm_secs(60, &b));
            CHECK(idx == 0);
        }
    }
    // Burst 4 (Apocalypse 120mmxE): three short rearms, then the full ROF.
    ProjRng rng;
    int idx = 0;
    for (int cycle = 0; cycle < 20; ++cycle) {
        for (int shot = 0; shot < 4; ++shot) {
            const double s = burst_rearm_secs(idx, 4, 80, &rng);
            const int f = static_cast<int>(std::lround(s * 15.0));
            if (shot < 3) {
                CHECK(f >= 3);
                CHECK(f <= 5);
                CHECK(idx == shot + 1);
            } else {
                CHECK(f >= 80);
                CHECK(f <= 82);
                CHECK(idx == 0);
            }
        }
    }
    // A weapon switch to a smaller Burst wraps the index (modulo, as Fire does).
    idx = 3;
    burst_rearm_secs(idx, 2, 60, nullptr);
    CHECK(idx == 0);
}

TEST_CASE("w6units W6-04-01: real rulesmd Burst resolves on every slot") {
    if (ra2fixture::rulesmd_bytes().empty()) { MESSAGE("no install"); return; }
    const ra2assets::Rules& r = ra2fixture::rulesmd();
    CHECK(resolve_weapon_slot(r, "120mmx").burst == 2);
    CHECK(resolve_weapon_slot(r, "120mmxE").burst == 4);
    CHECK(resolve_weapon_slot(r, "HoverMissile").burst == 2);
    CHECK(resolve_weapon_slot(r, "Maverick3").burst == 2);
    CHECK(resolve_weapon_slot(r, "105mm").burst == 1);
    CHECK(resolve_weapon_slot(r, "105mmE").burst == 2);
    CHECK(resolve_weapon_slot(r, "M60").burst == 1);
    const TypeWeapons apoc = resolve_type_weapons(r, "APOC");
    CHECK(apoc.slot[0].burst == 2);
    CHECK(apoc.slot[1].burst == 2);   // MammothTusk
    CHECK(apoc.elite[0].burst == 4);
}

TEST_CASE("w6units W6-04-02: shrapnel keys come from the projectile, elite slots included") {
    if (ra2fixture::rulesmd_bytes().empty()) { MESSAGE("no install"); return; }
    const ra2assets::Rules& r = ra2fixture::rulesmd();
    const WeaponSlot comet = resolve_weapon_slot(r, "Comet");          // LargeCometP
    CHECK(comet.shrapnel_count == 5);
    CHECK(comet.shrapnel_damage == r.weapon_damage("CometFragment"));
    CHECK(comet.shrapnel_range == r.weapon_range("CometFragment"));
    const WeaponSlot super = resolve_weapon_slot(r, "SuperComet");     // SuperCometP
    CHECK(super.shrapnel_count == 5);
    CHECK(super.shrapnel_damage == r.weapon_damage("SuperCometFragment"));
    CHECK(resolve_weapon_slot(r, "ElectricBoltE").shrapnel_count == 2);  // Electricbounce
    CHECK(resolve_weapon_slot(r, "TankBoltE").shrapnel_count == 2);
    CHECK(resolve_weapon_slot(r, "ElectricBolt").shrapnel_count == 0);
    CHECK(resolve_weapon_slot(r, "TankBolt").shrapnel_count == 0);
    const TypeWeapons sref = resolve_type_weapons(r, "SREF");
    CHECK(sref.slot[0].shrapnel_count == 5);
    CHECK(sref.elite[0].shrapnel_count == 5);
}

TEST_CASE("w6units W6-04-06: OmniFire is read for every weapon slot (Aegis Medusa, launchers)") {
    if (ra2fixture::rulesmd_bytes().empty()) { MESSAGE("no install"); return; }
    const ra2assets::Rules& r = ra2fixture::rulesmd();
    CHECK(resolve_weapon_slot(r, "Medusa").omni_fire);
    CHECK(resolve_weapon_slot(r, "HornetLauncher").omni_fire);
    CHECK(resolve_weapon_slot(r, "DredLauncher").omni_fire);
    CHECK_FALSE(resolve_weapon_slot(r, "120mm").omni_fire);
    // activate_weapon_slot carries the slot's OmniFire into the unit.
    Unit u;
    u.w2.valid = true;
    u.w2.omni_fire = true;
    u.vet.base.omni_fire = false;
    activate_weapon_slot(u, 1, nullptr);
    CHECK(u.w6.omni_fire);
    activate_weapon_slot(u, 0, nullptr);
    CHECK_FALSE(u.w6.omni_fire);
}

namespace {
UnitCombat w6mk(const char* image, int strength, const char* armor, int dmg, int rof, double range,
                std::vector<int> verses, bool vehicle = false) {
    UnitCombat c;
    c.image = image;
    c.strength = strength;
    c.armor = armor;
    c.damage = dmg;
    c.rof = rof;
    c.range = range;
    c.verses = std::move(verses);
    c.is_vehicle = vehicle;
    return c;
}
bool w6_open_rect(const World& w, int W, int H, int& x0, int& y0) {
    for (int y = 4; y + H < w.grid_h() - 4; ++y)
        for (int x = 4; x + W < w.grid_w() - 4; ++x) {
            bool ok = true;
            for (int dy = 0; dy < H && ok; ++dy)
                for (int dx = 0; dx < W && ok; ++dx)
                    if (!w.walkable(x + dx, y + dy) || w.unit_at_cell(x + dx, y + dy) >= 0) ok = false;
            if (ok && w.cell_level(x, y) == w.cell_level(x + W - 1, y + H - 1)) { x0 = x; y0 = y; return true; }
        }
    return false;
}
}  // namespace

TEST_CASE("w6units W6-04-03: an Ivan bomb on a 2x2 building hits once per foundation cell (DamageArea)") {
    if (!ra2fixture::mapsmd03_available()) { MESSAGE("no install"); return; }
    World world(ra2fixture::mapsmd03_map());
    int x0 = 0, y0 = 0;
    REQUIRE(w6_open_rect(world, 12, 4, x0, y0));
    AbilityProfile ip;
    ip.fire_kind = AbilityKind::IvanBomb;
    ip.infantry = true;
    world.register_ability_profile("IVAN", ip);
    const std::vector<int> all100 = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
    const int b = world.add_building(1, x0 + 6, y0, 2, 2, 1, 2000, armor_index("wood"), 0.0,
                                     /*instant=*/true);
    world.add_combat_unit(x0, y0, 0, w6mk("IVAN", 125, "none", 400, 50, 1.5, all100));
    world.units()[0].target_building = b;
    world.units()[0].attack_order = true;
    int blast = -1;
    for (int k = 0; k < 900 && blast < 0; ++k) {
        world.step(1.0 / 15.0);
        for (const World::AbilityEvent& e : world.take_ability_events())
            if (e.kind == World::AbilityEvent::Kind::BombBlast) blast = k;
    }
    REQUIRE(blast >= 0);
    // IvanWH 450, CellSpread 1.5 (384 lept), PercentAtMax .25, wood 100 %: the
    // impact cell takes 450, the other three cells (181 lept) 290 each.
    CHECK(world.buildings()[b].hp == 2000 - 450 - 3 * 290);
}
