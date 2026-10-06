// Gap fix C (G-40, G-29 defense half) -- base defenses acquire, aim and fire.
// [RE: docs/re/defenses.md]. World members kept in their own TU (the
// world_veterancy.cpp precedent); world.cpp only calls step_building_combat.
#include <algorithm>
#include <cmath>

#include "capture.hpp"     // apply_blackout
#include "combat.hpp"      // deliver_hit_from, apply_spread_damage, damage_vs
#include "production.hpp"  // power_totals
#include "weapon_unit.hpp"
#include "world.hpp"

namespace ra2engine {

namespace {
constexpr double kLogicFps = 15.0;        // ROF frames -> seconds (combat.md §5)
constexpr double kLept = 256.0;
constexpr double kFireFlashSecs = 0.4;    // renderer cue, same as units

double bcenter_x(const Building& b) { return b.cx + (b.fw - 1) / 2.0; }
double bcenter_y(const Building& b) { return b.cy + (b.fh - 1) / 2.0; }
}  // namespace

std::vector<World::FireEvent> World::take_fire_events() {
    std::vector<FireEvent> out;
    out.swap(fire_events_);
    return out;
}

bool World::defense_power_ok(int house) const {
    // [RE: defenses.md §2] FUN_004555c0: Powered && drain > 0 && the house's
    // output/drain ratio (FUN_004fcc50) < 1.0 -> not operational. A spy
    // blackout zeroes the output (apply_blackout).
    const PowerTotals pt =
        apply_blackout(power_totals(buildings_, btypes(), house), house_blacked_out(house));
    return pt.drain <= pt.output;
}

Unit World::defense_shooter(int b_idx, int slot) const {
    const Building& b = buildings_[b_idx];
    const BuildingTypeDef& t = btypes()[b.type];
    const WeaponSlot& w = t.weapon[slot];
    Unit s;
    s.house = b.house;
    s.fx = bcenter_x(b);
    s.fy = bcenter_y(b);
    s.cell_x = static_cast<int>(std::lround(s.fx));
    s.cell_y = static_cast<int>(std::lround(s.fy));
    s.w_damage = w.damage;
    s.w_rof = w.rof;
    s.w_range = w.range;
    s.w_verses = w.verses;
    s.w_cell_spread = w.cell_spread;
    s.w_percent_at_max = w.percent_at_max;
    s.w_proj = w.proj;
    s.w_aa = w.aa;
    s.w_ag = w.ag;
    s.alive = false;   // never a real unit
    return s;
}

void World::launch_defense_projectile(int b_idx, int target_unit, int slot) {
    const Building& b = buildings_[b_idx];
    const ProjectileSpec& spec = btypes()[b.type].weapon[slot].proj;
    const Unit& v = units_[target_unit];
    const Vec3i src{ftol(bcenter_x(b) * kLept), ftol(bcenter_y(b) * kLept), 0};
    const Vec3i tgt{ftol(v.fx * kLept), ftol(v.fy * kLept),
                    v.is_aircraft ? ftol(v.altitude) : 0};
    const LaunchResult lr = compute_launch(spec, proj_rules_, src, tgt, proj_rng_);
    ProjectileEvent ev;
    ev.frame = proj_frame_;
    ev.attacker = -1;
    ev.x = src.x; ev.y = src.y; ev.z = src.z;
    ev.speed = lr.speed;
    ev.scatter_dx = lr.scatter_dx;
    ev.scatter_dy = lr.scatter_dy;
    if (!lr.ok) {
        ev.kind = ProjectileEvent::Refused;
        proj_events_.push_back(ev);
        return;
    }
    ev.kind = ProjectileEvent::Launch;
    proj_events_.push_back(ev);
    Projectile p;
    p.kind = spec.kind;
    p.attacker = -1;
    p.attacker_building = b_idx;
    p.weapon_slot = slot;
    p.target_unit = target_unit;
    p.x = src.x; p.y = src.y; p.z = src.z;
    p.vx = lr.vx; p.vy = lr.vy; p.vz = lr.vz;
    p.tx = tgt.x; p.ty = tgt.y; p.tz = tgt.z;
    p.speed_cap = spec.speed_cap;
    p.acceleration = spec.acceleration;
    p.detonation_altitude = spec.detonation_altitude;
    p.inaccurate = spec.inaccurate;
    p.floater = spec.floater;
    p.launch_frame = proj_frame_;
    p.launch_speed = lr.speed;
    p.max_z = src.z;
    projectiles_.push_back(p);
}

void World::step_building_combat(double dt) {
    const std::vector<BuildingTypeDef>& types = btypes();
    if (types.empty()) return;
    std::vector<int> power_cache;   // per house: -1 unknown, 0 low, 1 ok
    const int nb = static_cast<int>(buildings_.size());
    const int nu = static_cast<int>(units_.size());
    for (int bi = 0; bi < nb; ++bi) {
        Building& b = buildings_[bi];
        if (!b.alive || b.state != Building::State::Idle) continue;
        if (b.type < 0 || b.type >= static_cast<int>(types.size())) continue;
        const BuildingTypeDef& t = types[b.type];
        if (!t.armed) continue;

        b.def_fire_flash = std::max(0.0, b.def_fire_flash - dt);
        // [defenses.md §2] Powered=yes: offline while the house is short of power.
        b.def_offline = false;
        if (t.powered) {
            if (b.house >= static_cast<int>(power_cache.size()))
                power_cache.resize(static_cast<std::size_t>(b.house) + 1, -1);
            if (power_cache[b.house] < 0) power_cache[b.house] = defense_power_ok(b.house) ? 1 : 0;
            if (power_cache[b.house] == 0) {
                b.def_offline = true;
                b.def_target = -1;
                continue;
            }
        }
        b.def_rof_cool = std::max(0.0, b.def_rof_cool - dt);

        const double cx = bcenter_x(b), cy = bcenter_y(b);
        auto slot_for = [&](const Unit& v) {
            return choose_weapon(t.weapon[0], t.weapon[1], unit_airborne(v), v.armor_ix);
        };
        auto in_range = [&](const Unit& v, int slot) {
            const double r = (t.guard_range > 0.0 ? std::max(t.guard_range, t.weapon[slot].range)
                                                  : t.weapon[slot].range) * kLept;
            return std::hypot((v.fx - cx) * kLept, (v.fy - cy) * kLept) <= r;
        };
        auto engageable = [&](int j) {
            if (j < 0 || j >= nu) return false;
            const Unit& v = units_[j];
            if (!v.alive || v.die_time >= 0.0 || v.cargo_of >= 0) return false;
            if (!is_enemy(b.house, v.house) || v.disguised) return false;
            if (!(v.seen_by_mask & (1u << b.house))) return false;
            const int s = slot_for(v);
            return s >= 0 && in_range(v, s);
        };
        // Target validation, then acquire the nearest engageable enemy (ties:
        // lowest index) -- a defense never moves, so out of range = dropped.
        if (!engageable(b.def_target)) {
            b.def_target = -1;
            double best = 0.0;
            for (int j = 0; j < nu; ++j) {
                if (!engageable(j)) continue;
                const Unit& v = units_[j];
                const double d = std::hypot(v.fx - cx, v.fy - cy);
                if (b.def_target < 0 || d < best) { b.def_target = j; best = d; }
            }
        }
        if (b.def_target < 0) continue;
        const Unit& v = units_[b.def_target];
        const int slot = slot_for(v);
        // Turret=yes: slew at ROT (Set_ROT, voxel-render.md §5) and fire only
        // once on target; Turret=no (Prism/Tesla) fires at once.
        if (t.turret) {
            const double want = facing_u16_from_delta(v.fx - cx, v.fy - cy);
            const double step = std::max(1, t.rot) * kRotU16PerRotPerSec * dt;
            b.def_turret_facing = turn_toward(b.def_turret_facing, want, step);
            if (b.def_turret_facing != want) continue;
        }
        if (b.def_rof_cool > 0.0) continue;
        const WeaponSlot& w = t.weapon[slot];
        b.def_rof_cool = w.rof / kLogicFps;
        b.def_fire_flash = kFireFlashSecs;
        b.def_slot = slot;
        fire_events_.push_back({-1, bi, b.def_target, -1, slot, w.id});
        if (projectiles_enabled_ && w.proj.flies()) {
            launch_defense_projectile(bi, b.def_target, slot);
            continue;
        }
        const Unit shooter = defense_shooter(bi, slot);
        if (w.cell_spread >= 1.0)
            apply_spread_damage(units_, occ_, shooter, units_[b.def_target]);
        else
            deliver_hit_from(units_, occ_, shooter, -1, bi, b.def_target);
    }
}

}  // namespace ra2engine
