// Wave-5 unit and building abilities (agent C) -- the World-side pass.
// [RE: docs/re/abilities-w5.md]. A World member TU (the world_abilities.cpp
// precedent) so the wave's churn stays out of world.cpp. Everything here is
// behind w5_ state that only a real-play roster (wire_w5) or a stolen-tech
// event fills; gate Worlds never arm it.
#include <algorithm>
#include <cmath>

#include "combat.hpp"   // deliver_hit_from, rearm_secs
#include "group_move.hpp"   // nearest_free_cells
#include "world.hpp"

namespace ra2engine {

namespace {
bool live(const Unit& u) { return u.alive && u.die_time < 0.0 && u.cargo_of < 0; }
bool live_bld(const Building& b) { return b.alive && b.state != Building::State::Dead; }
}  // namespace

void World::grant_stolen_tech(int house, std::uint8_t bits) {
    if (house < 0 || bits == 0) return;
    if ((int)w5_.stolen_tech.size() <= house) w5_.stolen_tech.resize((std::size_t)house + 1, 0);
    w5_.stolen_tech[(std::size_t)house] |= bits;
}

// [RE: abilities-w5.md §1, HouseClass::HasAllStolenTech (YRpp naming; the
// flags are the house+0x2BC..0x2BE bytes FUN_004571E0 writes)]: every
// RequiresStolen*Tech bit of the type must be held.
void World::apply_stolen_tech_gate(std::vector<UnitTypeDef>& defs, int house) const {
    const std::uint8_t have = stolen_tech(house);
    for (UnitTypeDef& d : defs) {
        if (d.w5.requires_stolen == 0) continue;
        d.tech_level = (have & d.w5.requires_stolen) == d.w5.requires_stolen ? d.w5.base_tech_level : 0;
    }
}

void World::arm_w5(const RulesW5& rules, const std::map<std::string, UnitProfileW5>& profiles) {
    w5_.armed = true;
    w5_.rules = rules;
    w5_.profiles = profiles;
    for (Unit& u : units_) u.w5.bound = false;   // re-bind
}

// IsPowerOnline for a Powered building [RE: abilities-w5.md §3,
// FUN_004555C0]: offline at low power unless two or more Tesla troopers
// overpower it.
bool World::w5_power_online(int b_idx) const {
    const Building& b = buildings_[(std::size_t)b_idx];
    if (b.type < 0 || b.type >= (int)btypes().size()) return true;
    const BuildingTypeDef& t = btypes()[(std::size_t)b.type];
    if (!t.powered || t.power >= 0) return true;
    if (b.w5.overpowerers.size() >= 2) return true;
    return !house_low_power(b.house);
}

void World::step_w5(double dt) {
    // AI houses: their producible rosters follow their stolen tech (a spy the
    // AI sent into an enemy Battle Lab unlocks the stolen-tech infantry for
    // its team builder).
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[(std::size_t)h];
        if (!rec.is_ai || !rec.cfg.team.enabled) continue;
        apply_stolen_tech_gate(rec.cfg.team.unit_roster, h);
    }
    if (!w5_.armed) return;
    const std::uint64_t F = w5_.frame;
    const int nu = (int)units_.size();
    const int nb = (int)buildings_.size();
    const std::vector<BuildingTypeDef>& types = btypes();

    // (1) bind unit profiles by image.
    for (Unit& u : units_) {
        if (u.w5.bound) continue;
        u.w5.bound = true;
        const auto it = w5_.profiles.find(u.image);
        if (it != w5_.profiles.end()) u.w5.p = it->second;
    }

    // (2) Tech Hospital / Machine Shop [RE: abilities-w5.md §5,
    // TechnoClass::Update FUN_006F9380]: per house the sum of its buildings'
    // InfantryGainSelfHeal / UnitsGainSelfHeal (HouseClass +0x164/+0x168);
    // on Frame % SelfHealInfantryFrames == 0 every damaged own infantry gains
    // min(SelfHealInfantryAmount * count, missing); vehicles likewise on
    // SelfHealUnitFrames.
    {
        const RulesW5& R = w5_.rules;
        const bool inf_tick = R.self_heal_inf_frames > 0 && F % (std::uint64_t)R.self_heal_inf_frames == 0;
        const bool unit_tick = R.self_heal_unit_frames > 0 && F % (std::uint64_t)R.self_heal_unit_frames == 0;
        if (inf_tick || unit_tick) {
            std::vector<int> inf((std::size_t)house_count(), 0), veh((std::size_t)house_count(), 0);
            for (const Building& b : buildings_) {
                if (!live_bld(b) || b.house < 0 || b.house >= house_count()) continue;
                if (b.type < 0 || b.type >= (int)types.size()) continue;
                inf[(std::size_t)b.house] += types[(std::size_t)b.type].w5.inf_self_heal;
                veh[(std::size_t)b.house] += types[(std::size_t)b.type].w5.unit_self_heal;
            }
            for (Unit& u : units_) {
                if (!live(u) || u.hp <= 0 || u.hp >= u.max_hp) continue;
                if (u.house < 0 || u.house >= house_count()) continue;
                const bool infantry = !u.is_vehicle && !u.is_aircraft && !u.is_naval;
                const bool vehicle = u.is_vehicle && !u.is_aircraft;
                int amt = 0;
                if (infantry && inf_tick) amt = R.self_heal_inf_amount * inf[(std::size_t)u.house];
                else if (vehicle && unit_tick) amt = R.self_heal_unit_amount * veh[(std::size_t)u.house];
                if (amt > 0) u.hp += std::min(amt, u.max_hp - u.hp);
            }
        }
    }

    // (3) Tesla Coil charging [RE: abilities-w5.md §3].
    //  a) An idle Tesla Trooper (Secondary warhead ElectricAssault) scans its 8
    //     neighbour cells for an own Overpowerable building and charges it
    //     (FootClass::Mission_Guard/AreaGuard); it keeps charging while it
    //     stays put and has no other target.
    for (int i = 0; i < nu; ++i) {
        Unit& u = units_[(std::size_t)i];
        if (!u.w5.p.assault) continue;
        if (!live(u) || u.moving() || u.target >= 0 || u.target_building >= 0 || u.attack_order) {
            u.w5.charging = -1;
            continue;
        }
        int c = u.w5.charging;
        if (c >= 0 && (c >= nb || !live_bld(buildings_[(std::size_t)c]) || buildings_[(std::size_t)c].house != u.house))
            c = -1;
        if (c >= 0) {   // still adjacent?
            const Building& b = buildings_[(std::size_t)c];
            const int dx = std::max({b.cx - u.cell_x, u.cell_x - (b.cx + b.fw - 1), 0});
            const int dy = std::max({b.cy - u.cell_y, u.cell_y - (b.cy + b.fh - 1), 0});
            if (std::max(dx, dy) > 1) c = -1;
        }
        if (c < 0) {
            static const int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            static const int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
            for (int k = 0; k < 8 && c < 0; ++k) {
                const int bi = building_at_cell(u.cell_x + kDx[k], u.cell_y + kDy[k]);
                if (bi < 0) continue;
                const Building& b = buildings_[(std::size_t)bi];
                if (b.house != u.house || b.type < 0 || b.type >= (int)types.size()) continue;
                if (!types[(std::size_t)b.type].w5.overpowerable) continue;
                c = bi;
            }
        }
        u.w5.charging = c;
        if (c < 0) continue;
        // AssaultBolt every ROF frames: BuildingClass::Overpower (FUN_00452810)
        // moves the trooper to the end of the set; the warhead does no damage.
        if ((std::int64_t)F >= u.w5.next_assault) {
            u.w5.next_assault = (std::int64_t)F + std::max(1, u.w5.p.assault_rof);
            std::vector<int>& ov = buildings_[(std::size_t)c].w5.overpowerers;
            ov.erase(std::remove(ov.begin(), ov.end(), i), ov.end());
            ov.push_back(i);
            u.fire_flash = 0.4;
            fire_events_.push_back({i, -1, -1, c, 1, "AssaultBolt"});
        }
    }
    //  b) BuildingClass::AI upkeep: a trooper that stopped charging this coil
    //     leaves the set; IsOverpowered = n >= 3, or n >= 1 with the house at
    //     full power.
    for (int bi = 0; bi < nb; ++bi) {
        Building& b = buildings_[(std::size_t)bi];
        if (b.w5.overpowerers.empty() && !b.w5.overpowered) continue;
        std::vector<int>& ov = b.w5.overpowerers;
        ov.erase(std::remove_if(ov.begin(), ov.end(),
                                [&](int t) {
                                    return t < 0 || t >= nu || !live(units_[(std::size_t)t]) ||
                                           units_[(std::size_t)t].w5.charging != bi;
                                }),
                 ov.end());
        const std::size_t n = live_bld(b) ? ov.size() : 0;
        b.w5.overpowered = n >= 3 || (n >= 1 && !house_low_power(b.house));
    }

    w5_step_defuse();   // (R-046) engineers disarming Ivan bombs
    w5_step_repair_ifv(dt);   // (R-048) the Engineer IFV repairs friendly vehicles

    // (4) OpenTopped transports: passengers fire out [RE: abilities-w5.md §6].
    {
        const RulesW5& R = w5_.rules;
        for (int ti = 0; ti < nu; ++ti) {
            Unit& t = units_[(std::size_t)ti];
            if (!t.w5.p.open_topped || t.cargo.empty() || !live(t)) continue;
            for (int pi : t.cargo) {
                if (pi < 0 || pi >= nu) continue;
                Unit& p = units_[(std::size_t)pi];
                if (!p.alive || p.die_time >= 0.0) continue;
                p.rof_cool = std::max(0.0, p.rof_cool - dt);
                if (p.w_damage <= 0 || p.w_range <= 0.0) continue;
                const double range = p.w_range + R.open_topped_range_bonus;
                auto ok = [&](int j) {
                    if (j < 0 || j >= nu) return false;
                    const Unit& v = units_[(std::size_t)j];
                    if (!live(v) || !is_enemy(p.house, v.house) || v.disguised || v.an.submerged) return false;
                    if (rocket_on_launcher(v)) return false;
                    if (p.house < 8 && !(v.seen_by_mask & (1u << p.house))) return false;
                    const bool air = v.is_aircraft && v.altitude > 0.0;
                    if (air ? !p.w_aa : !p.w_ag) return false;
                    return std::hypot(v.fx - t.fx, v.fy - t.fy) <= range;
                };
                int tgt = ok(t.target) ? t.target : (ok(p.target) ? p.target : -1);
                if (tgt < 0) {
                    double best = 0.0;
                    for (int j = 0; j < nu; ++j) {
                        if (!ok(j)) continue;
                        const double d = std::hypot(units_[(std::size_t)j].fx - t.fx, units_[(std::size_t)j].fy - t.fy);
                        if (tgt < 0 || d < best) { tgt = j; best = d; }
                    }
                }
                p.target = tgt;
                if (tgt < 0 || p.rof_cool > 0.0) continue;
                Unit shooter = p;
                shooter.fx = t.fx;
                shooter.fy = t.fy;
                shooter.cell_x = t.cell_x;
                shooter.cell_y = t.cell_y;
                shooter.w_damage = static_cast<int>(p.w_damage * R.open_topped_damage_mult);
                p.rof_cool = rearm_secs(p.w_rof, &combat_rng_);
                fire_events_.push_back({ti, -1, tgt, -1, 0, "OpenTopped"});
                deliver_hit_from(units_, occ_, shooter, pi, -1, tgt);
                ++w5_.passenger_shots;
            }
        }
    }
    ++w5_.frame;
}

// ---------------------------------------------------------------------------
// Gap Generator [RE: abilities-w5.md §4, TechnoClass::CreateGap FUN_006FA690,
// the 120-frame re-shroud pass FUN_00577B30]. Called from step_visibility
// between the per-house sight stamp and the SpySat reveal (`phase` 0) and
// after it (`phase` 1). The original keeps ONE shroud, the local player's;
// here every human (non-AI) house that is neither the gap's owner nor its
// ally is shrouded.
// ---------------------------------------------------------------------------
void World::w5_gap_pass(int phase) {
    if (!w5_.armed || grid_w_ <= 0 || grid_h_ <= 0) return;
    const std::vector<BuildingTypeDef>& types = btypes();
    const std::size_t cells = (std::size_t)grid_w_ * (std::size_t)grid_h_;
    const int nh = house_count();
    if (phase == 0) {
        // Which houses each covered cell hides from: a bit per house.
        w5_hide_.assign(cells, 0);
        bool any = false;
        for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
            Building& b = buildings_[(std::size_t)bi];
            const int R = (b.type >= 0 && b.type < (int)types.size()) ? types[(std::size_t)b.type].w5.gap_radius : 0;
            const bool on = R > 0 && live_bld(b) && b.state == Building::State::Idle && w5_power_online(bi);
            const bool created = on && !b.w5.gap_on;
            b.w5.gap_on = on;
            if (!on) continue;
            std::uint8_t mask = 0;
            for (int h = 0; h < nh && h < 8; ++h)
                if (h != b.house && house_combatant(h) && !ai_enabled(h) && !are_allied(h, b.house))
                    mask |= (std::uint8_t)(1u << h);
            if (!mask) continue;
            any = true;
            const int cx = b.cx + b.fw / 2, cy = b.cy + b.fh / 2;
            for (int dy = -R - 2; dy < R + 2; ++dy)
                for (int dx = -R - 2; dx < R + 2; ++dx) {
                    if (dx * dx + dy * dy >= (R + 1) * (R + 1)) continue;
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) continue;
                    w5_hide_[(std::size_t)y * grid_w_ + x] |= mask;
                }
            // CreateGap: the covered cells go black at once (not where the
            // victim's own units currently look).
            if (created) w5_.gap_created = true;
        }
        if (!any) { w5_hide_.clear(); w5_.gap_created = false; return; }
        const bool tick = w5_.gap_created || (w5_.frame % 120 == 0);
        w5_.gap_created = false;
        for (int h = 0; h < nh && h < 8; ++h) {
            if (visible_[(std::size_t)h].empty()) continue;
            const bool spysat = map_reveal_[(std::size_t)h] != 0;
            if (!tick && !spysat) continue;
            for (std::size_t c = 0; c < cells; ++c)
                if ((w5_hide_[c] & (1u << h)) && !visible_[(std::size_t)h][c]) explored_[(std::size_t)h][c] = 0;
        }
        // Remember which hidden cells the house's own sight covers (phase 1).
        w5_own_.assign(cells, 0);
        for (std::size_t c = 0; c < cells; ++c) {
            if (!w5_hide_[c]) continue;
            for (int h = 0; h < nh && h < 8; ++h)
                if (!visible_[(std::size_t)h].empty() && visible_[(std::size_t)h][c]) w5_own_[c] |= (std::uint8_t)(1u << h);
        }
        return;
    }
    // phase 1: a SpySat reveal does not lift an enemy gap (the gap raised the
    // cell's ShroudCounter; only own sight shows it).
    if (w5_hide_.empty()) return;
    for (int h = 0; h < nh && h < 8; ++h) {
        if (!map_reveal_[(std::size_t)h] || visible_[(std::size_t)h].empty()) continue;
        for (std::size_t c = 0; c < cells; ++c)
            if ((w5_hide_[c] & (1u << h)) && !(w5_own_[c] & (1u << h))) {
                explored_[(std::size_t)h][c] = 0;
                visible_[(std::size_t)h][c] = 0;
            }
    }
}

bool World::order_defuse(int eng, int unit_target, int b_target) {
    if (eng < 0 || eng >= (int)units_.size()) return false;
    Unit& e = units_[(std::size_t)eng];
    if (!live(e)) return false;
    if (unit_target >= 0 && (unit_target >= (int)units_.size() || units_[(std::size_t)unit_target].bomb_frame < 0))
        return false;
    if (b_target >= 0 && (b_target >= (int)buildings_.size() || buildings_[(std::size_t)b_target].bomb_frame < 0))
        return false;
    if (unit_target < 0 && b_target < 0) return false;
    e.w5.defuse_unit = unit_target;
    e.w5.defuse_bld = b_target;
    e.target = -1;
    e.target_building = -1;
    e.attack_order = false;
    e.capture_target = -1;
    return true;
}

// Engineer bomb defusal [RE: abilities-w5.md §8, BulletClass::Detonate
// BombDisarm branch (WarheadType +0x16E) -> FUN_00438930 on a target whose
// AttachedBomb (+0x38) is set]: the DefuseKit (Range 1.5, FireOnce) removes
// the Ivan bomb; the engineer is not consumed.
void World::w5_step_defuse() {
    const int nu = (int)units_.size();
    for (int i = 0; i < nu; ++i) {
        Unit& e = units_[(std::size_t)i];
        if (e.w5.defuse_unit < 0 && e.w5.defuse_bld < 0) continue;
        if (!live(e)) { e.w5.defuse_unit = e.w5.defuse_bld = -1; continue; }
        double tx, ty, reach = 1.5;
        if (e.w5.defuse_unit >= 0) {
            const Unit& v = units_[(std::size_t)e.w5.defuse_unit];
            if (!v.alive || v.die_time >= 0.0 || v.bomb_frame < 0) { e.w5.defuse_unit = -1; continue; }
            tx = v.fx; ty = v.fy;
        } else {
            const Building& b = buildings_[(std::size_t)e.w5.defuse_bld];
            if (!live_bld(b) || b.bomb_frame < 0) { e.w5.defuse_bld = -1; continue; }
            // nearest foundation cell
            tx = std::max<double>(b.cx, std::min<double>(e.fx, b.cx + b.fw - 1));
            ty = std::max<double>(b.cy, std::min<double>(e.fy, b.cy + b.fh - 1));
        }
        if (std::hypot(e.fx - tx, e.fy - ty) <= reach + 1e-9) {
            if (e.w5.defuse_unit >= 0) units_[(std::size_t)e.w5.defuse_unit].bomb_frame = -1;
            else buildings_[(std::size_t)e.w5.defuse_bld].bomb_frame = -1;
            fire_events_.push_back({i, -1, e.w5.defuse_unit, e.w5.defuse_bld, 0, "DefuseKit"});
            ++w5_.defusals;
            e.w5.defuse_unit = e.w5.defuse_bld = -1;
            continue;
        }
        if (!e.moving()) {
            const std::vector<Cellxy> g = nearest_free_cells({(int)std::lround(tx), (int)std::lround(ty)}, 1,
                                                             passable_, grid_w_, grid_h_, occ_);
            if (!g.empty()) order_unit_move(i, g[0].x, g[0].y);
        }
    }
}

// IFV + Engineer (RepairBullet Damage=-50, Range 1.8, Mechanical warhead:
// vehicles 100%, infantry 0%) [RE-FALLBACK: the original routes a negative
// weapon through the friendly-target acquisition (TechnoClass target scan with
// the "repair" threat type); here the nearest damaged allied vehicle within
// Sight is chosen, approached and healed every ROF].
void World::w5_step_repair_ifv(double dt) {
    const int nu = (int)units_.size();
    for (int i = 0; i < nu; ++i) {
        Unit& u = units_[(std::size_t)i];
        if (!u.w5.healer) continue;
        if (!live(u)) { u.w5.heal_target = -1; continue; }
        if (u.target >= 0 || u.target_building >= 0) {   // never "attack" with a repair gun
            u.target = -1;
            u.target_building = -1;
            u.attack_order = false;
        }
        auto ok = [&](int j) {
            if (j < 0 || j >= nu || j == i) return false;
            const Unit& v = units_[(std::size_t)j];
            return live(v) && v.is_vehicle && !v.is_aircraft && are_allied(u.house, v.house) && v.hp < v.max_hp &&
                   damage_vs(-u.w_damage, u.w_verses, v.armor_ix) > 0;
        };
        int t = ok(u.w5.heal_target) ? u.w5.heal_target : -1;
        if (t < 0 && !u.moving()) {
            double best = 0.0;
            const double reach = std::max(u.w_range, (double)u.sight);
            for (int j = 0; j < nu; ++j) {
                if (!ok(j)) continue;
                const double d = std::hypot(units_[(std::size_t)j].fx - u.fx, units_[(std::size_t)j].fy - u.fy);
                if (d > reach || (t >= 0 && d >= best)) continue;
                t = j;
                best = d;
            }
        }
        u.w5.heal_target = t;
        u.rof_cool = std::max(0.0, u.rof_cool - dt);
        if (t < 0) continue;
        Unit& v = units_[(std::size_t)t];
        if (std::hypot(v.fx - u.fx, v.fy - u.fy) <= u.w_range + 1e-9) {
            if (u.rof_cool > 0.0) continue;
            const int heal = damage_vs(-u.w_damage, u.w_verses, v.armor_ix);
            v.hp += std::min(heal, v.max_hp - v.hp);
            u.rof_cool = rearm_secs(u.w_rof, &combat_rng_);
            u.fire_flash = 0.4;
            fire_events_.push_back({i, -1, t, -1, 0, "RepairBullet"});
            ++w5_.repairs;
        } else if (!u.moving()) {
            const std::vector<Cellxy> g = nearest_free_cells({v.cell_x, v.cell_y}, 1, passable_, grid_w_, grid_h_, occ_);
            if (!g.empty()) order_unit_move(i, g[0].x, g[0].y);
        }
    }
}

int World::w5_gap_hidden(int h) const {
    if (h < 0 || h >= 8 || w5_hide_.empty() || h >= (int)explored_.size() || explored_[(std::size_t)h].empty())
        return 0;
    int n = 0;
    for (std::size_t c = 0; c < w5_hide_.size(); ++c)
        if ((w5_hide_[c] & (1u << h)) && !explored_[(std::size_t)h][c]) ++n;
    return n;
}

}  // namespace ra2engine
