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
