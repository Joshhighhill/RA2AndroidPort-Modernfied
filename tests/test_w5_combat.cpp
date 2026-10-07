// W5-B (wave-5 combat and movement core) -- unit tests for the RE'd pieces:
// Modify_Damage (FUN_00489090), the DamageArea ring tables and applicator
// (FUN_00489190), infantry sub-cell spots (FindInfantrySubposition
// FUN_00481090), land-type speed and the crush/3-D range rules.
#include "doctest.h"

#include <cmath>

#include "combat.hpp"
#include "ra2assets/map.hpp"
#include "world.hpp"

using namespace ra2engine;

namespace {
const std::vector<int> kFlat(11, 100);

UnitCombat inf(int strength, int damage, double spread, double pam) {
    UnitCombat c;
    c.strength = strength;
    c.armor = "none";
    c.image = "E1";
    c.damage = damage;
    c.rof = 20;
    c.range = 5.0;
    c.verses = kFlat;
    c.cell_spread = spread;
    c.percent_at_max = pam;
    return c;
}

struct Rig {
    ra2assets::MapFile map;
    World world{map};
};
}  // namespace

TEST_CASE("w5 modify_damage: linear falloff to PercentAtMax at the CellSpread edge (FUN_00489090)") {
    // BlimpHE: CellSpread=2 (512 leptons), PercentAtMax=.5.
    CHECK(modify_damage(100, kFlat, 0, 2.0, 0.5, 0) == 100);
    CHECK(modify_damage(100, kFlat, 0, 2.0, 0.5, 256) == 75);
    CHECK(modify_damage(100, kFlat, 0, 2.0, 0.5, 512) == 50);
    // No clamp of the distance: past the edge it keeps falling.
    CHECK(modify_damage(100, kFlat, 0, 2.0, 0.5, 600) == 41);
    // Verses after the falloff, truncated: 75 * 0.5 = 37.
    std::vector<int> half(11, 50);
    CHECK(modify_damage(100, half, 0, 2.0, 0.5, 256) == 37);
    // PercentAtMax 1 (or CellSpread 0): no falloff; NO floor of 1 for a 0 product.
    CHECK(modify_damage(90, kFlat, 0, 0.3, 1.0, 70) == 90);
    std::vector<int> quarter(11, 25);
    CHECK(modify_damage(3, quarter, 0, 0.0, 1.0, 0) == 0);
    // Healing applies only within 8 leptons.
    CHECK(modify_damage(-50, kFlat, 0, 1.0, 1.0, 7) == -50);
    CHECK(modify_damage(-50, kFlat, 0, 1.0, 1.0, 8) == 0);
    // MaxDamage=10000.
    CHECK(modify_damage(50000, kFlat, 0, 0.0, 1.0, 0) == 10000);
}

TEST_CASE("w5 DamageArea ring tables: counts DAT_007EC3D0 and the first offsets") {
    CHECK(spread_ring_count(0) == 1);
    CHECK(spread_ring_count(1) == 9);
    CHECK(spread_ring_count(2) == 21);
    CHECK(spread_ring_count(10) == 309);
    int dx, dy;
    spread_ring_offset(0, dx, dy);
    CHECK((dx == 0 && dy == 0));
    // Every ring-2 cell is within Chebyshev 2 and not a 5x5 corner.
    for (int k = 9; k < 21; ++k) {
        spread_ring_offset(k, dx, dy);
        CHECK(std::max(std::abs(dx), std::abs(dy)) == 2);
        CHECK(!(std::abs(dx) == 2 && std::abs(dy) == 2));
    }
}

TEST_CASE("w5 damage_area: splash hits buildings and own units, not the shooter (R-060)") {
    Rig r;
    World& w = r.world;
    // Shooter (house 0) and a friendly bystander next to the enemy target.
    w.add_combat_unit(0, 5, 0, inf(100, 100, 2.0, 0.5));
    w.add_combat_unit(5, 5, 1, inf(1000, 0, 0.0, 1.0));   // enemy target
    w.add_combat_unit(6, 5, 0, inf(1000, 0, 0.0, 1.0));   // own unit, 1 cell away
    const int bi = w.add_building(0, 4, 3, 1, 1, 1, 1000, 0, 0.0, true);   // 2 cells off
    Unit shooter = w.units()[0];
    Occupancy occ = w.occupancy();
    const AreaResult ar = damage_area(w, w.units(), w.buildings(), occ, shooter, 0, -1,
                                      5 * 256, 5 * 256, 0);
    CHECK(w.units()[1].hp == 1000 - 100);   // distance 0
    CHECK(w.units()[2].hp == 1000 - 75);    // friendly fire, 256 leptons
    // Building cell (4,3): offset (-1,-2) is in ring 2; d = sqrt(256^2+512^2)=572 > 512.
    CHECK(w.buildings()[bi].hp == 1000);
    CHECK(w.units()[0].hp == 100);           // the shooter itself is excluded
    CHECK(ar.damage == 175);
    // A building whose cell is within the radius takes splash (floor 1).
    const int b2 = w.add_building(0, 5, 4, 1, 1, 1, 1000, 0, 0.0, true);   // 1 cell off
    damage_area(w, w.units(), w.buildings(), occ, shooter, 0, -1, 5 * 256, 5 * 256, 0);
    CHECK(w.buildings()[b2].hp == 1000 - 75);
}

TEST_CASE("w5 damage_area: a building is hit once per foundation cell in the ring set") {
    Rig r;
    World& w = r.world;
    w.add_combat_unit(0, 0, 0, inf(100, 100, 2.0, 0.5));
    const int bi = w.add_building(0, 4, 4, 3, 3, 1, 5000, 0, 0.0, true);   // cells 4..6
    Unit shooter = w.units()[0];
    Occupancy occ = w.occupancy();
    damage_area(w, w.units(), w.buildings(), occ, shooter, 0, -1, 5 * 256, 5 * 256, 0);
    // centre cell 100, 4 edge cells at 256 -> 75, 4 corners at 362 -> 64.
    CHECK(w.buildings()[bi].hp == 5000 - (100 + 4 * 75 + 4 * 64));
}

TEST_CASE("w5 damage_area: AffectsAllies=no spares own and allied units (ReceiveDamage 0x700DA0)") {
    Rig r;
    World& w = r.world;
    w.add_combat_unit(0, 5, 0, inf(100, 100, 2.0, 0.5));
    w.add_combat_unit(5, 5, 1, inf(1000, 0, 0.0, 1.0));
    w.add_combat_unit(6, 5, 0, inf(1000, 0, 0.0, 1.0));
    Unit shooter = w.units()[0];
    Occupancy occ = w.occupancy();
    shooter.w_affects_allies = false;
    damage_area(w, w.units(), w.buildings(), occ, shooter, 0, -1, 5 * 256, 5 * 256, 0);
    CHECK(w.units()[1].hp == 900);
    CHECK(w.units()[2].hp == 1000);
}
