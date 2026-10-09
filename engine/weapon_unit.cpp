#include "weapon_unit.hpp"

#include "capture.hpp"  // veteran_firepower (legacy rank bake)

namespace ra2engine {

namespace {
bool unit_is_elite(const Unit& u) { return is_elite(u.vet.veterancy); }
}  // namespace

WeaponSlot primary_view(const Unit& u) {
    WeaponSlot s;
    s.valid = true;   // legacy: the primary is always "present" (range 0 = inert)
    // The live w_verses while the primary is active (an IFV's passenger swap
    // rewrites w_* in place), else the type's base/elite primary.
    if (u.w_active == 0) s.verses = u.w_verses;
    else if (u.vet.recv && unit_is_elite(u) && u.vet.has_elite) s.verses = u.vet.elite.verses;
    else s.verses = u.vet.base.verses;
    s.aa = u.p_aa;
    s.ag = u.p_ag;
    return s;
}

const WeaponSlot& secondary_view(const Unit& u) {
    if (u.vet.recv && unit_is_elite(u) && u.w2_elite.valid) return u.w2_elite;
    return u.w2;
}

namespace {
int unit_weapon_for_unit_base(const Unit& u, const Unit& v) {
    if (u.deploy_fire && u.deployed) return u.deploy_fire_slot == 1 && u.w2.valid ? 1 : 0;
    if (!u.w2.valid) {
        // No secondary: the primary alone, gated like GetFireError.
        return slot_can_target(primary_view(u), unit_airborne(v), v.armor_ix) ? 0 : -1;
    }
    return choose_weapon(primary_view(u), secondary_view(u), unit_airborne(v), v.armor_ix);
}
}  // namespace

int unit_weapon_for_unit(const Unit& u, const Unit& v) {
    const int want = unit_weapon_for_unit_base(u, v);
    // W2-AN-14 [rules NavalTargeting/LandTargeting, defaults 0]: the water /
    // land / underwater split. A unit with all-default keys attacking a land
    // or surfaced target is unchanged (the rule returns `want`).
    return an_naval_slot(u.an.t, want, u.w2.valid, v.an.in_water, v.an.submerged,
                         v.an.t.organic || (!v.is_vehicle && !v.is_aircraft && !v.is_naval),
                         unit_airborne(v), false);
}

int unit_weapon_for_building(const Unit& u, int armor_ix) {
    int want;
    if (u.deploy_fire && u.deployed) want = u.deploy_fire_slot == 1 && u.w2.valid ? 1 : 0;
    else if (!u.w2.valid) want = slot_can_target(primary_view(u), false, armor_ix) ? 0 : -1;
    else want = choose_weapon(primary_view(u), secondary_view(u), false, armor_ix);
    // W2-AN-14: LandTargeting=1 / UnderwaterOnly cannot hit a structure.
    return an_naval_slot(u.an.t, want, u.w2.valid, false, false, false, false, true);
}

void activate_weapon_slot(Unit& u, int slot, const VeterancyRules* vr) {
    if (slot == u.w_active) return;
    if (slot == 1 && !u.w2.valid) return;
    const bool vet = vr && u.vet.recv;
    const UnitVet& v = u.vet;
    auto has = [&](Ability a) {
        return has_ability(v.veterancy, v.vet_abilities, v.elite_abilities, a);
    };
    auto dmg = [&](int d) {
        if (vet) return fire_damage(d, v.firepower_mult, has(Ability::Firepower), vr->combat);
        return u.veteran_rank > 0 ? veteran_firepower(d, u.veteran_rank) : d;
    };
    auto rof = [&](int r) { return vet ? rof_frames(r, has(Ability::Rof), vr->rof) : r; };
    if (slot == 0) {
        const bool el = vet && unit_is_elite(u) && v.has_elite;
        const WeaponStats& w = el ? v.elite : v.base;
        u.w_damage = dmg(w.damage);
        u.w_rof = rof(w.rof);
        u.w6.burst = w.burst;                 // W6-04-01
        u.shrapnel_count = w.shrapnel_count;   // W6-04-02
        u.shrapnel_damage = w.shrapnel_damage;
        u.shrapnel_range = w.shrapnel_range;
        u.shrapnel_verses = w.shrapnel_verses;
        u.w_range = w.range;
        u.w_verses = w.verses;
        u.w_cell_spread = w.cell_spread;
        u.w_percent_at_max = w.percent_at_max;
        u.w_affects_allies = w.affects_allies;
        u.w_inf_death = w.inf_death;          // W3-B
        u.w_prone_damage = w.prone_damage;
        u.w_proj = (vet && unit_is_elite(u) && u.p_proj_elite_set) ? u.p_proj_elite : u.p_proj;
        u.w_aa = u.p_aa;
        u.w_ag = u.p_ag;
    } else {
        const WeaponSlot& s = secondary_view(u);
        u.w_damage = dmg(s.damage);
        u.w_rof = rof(s.rof);
        u.w6.burst = s.burst;                 // W6-04-01
        u.shrapnel_count = s.shrapnel_count;   // W6-04-02
        u.shrapnel_damage = s.shrapnel_damage;
        u.shrapnel_range = s.shrapnel_range;
        u.shrapnel_verses = s.shrapnel_verses;
        u.w_range = s.range;
        u.w_verses = s.verses;
        u.w_cell_spread = s.cell_spread;
        u.w_percent_at_max = s.percent_at_max;
        u.w_affects_allies = s.affects_allies;
        u.w_inf_death = s.inf_death;          // W3-B
        u.w_prone_damage = s.prone_damage;
        u.w_proj = s.proj;
        u.w_aa = s.aa;
        u.w_ag = s.ag;
    }
    u.w_active = slot;
}

}  // namespace ra2engine
