#include "world.hpp"
#include <algorithm>

namespace ra2engine {
namespace {
bool active(const Unit& u) { return u.alive && u.die_time < 0 && u.cargo_of < 0 && u.garrison_of < 0; }
}

void World::release_unit_mind_control(int controller) {
    if (controller < 0 || controller >= static_cast<int>(units_.size())) return;
    auto victims=std::move(units_[controller].w7.mc_victims);
    units_[controller].w7.mc_victims.clear();
    units_[controller].w7.overload_left=0;
    for (int vi : victims) {
        if (vi < 0 || vi >= static_cast<int>(units_.size())) continue;
        Unit& v=units_[vi]; if (v.w7.mc_controller != controller) continue;
        const int original=v.w7.mc_original_house;
        v.w7.mc_controller=v.w7.mc_original_house=-1;
        if (v.alive && v.die_time < 0 && original >= 0) set_unit_owner(vi,original);
    }
}

void World::prepare_skirmish_mind_control() {
    if (playfield_.campaign) return; // CampaignRuntime owns its team bookkeeping.
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& v = units_[i];
        const int c = v.w7.mc_controller;
        const int bi = v.w7.mc_building;
        if (bi >= 0 && (bi >= static_cast<int>(buildings_.size()) || !buildings_[bi].alive ||
            buildings_[bi].state != Building::State::Idle || buildings_[bi].house != v.house ||
            buildings_[bi].power_off || buildings_[bi].w7.warp_unpowered ||
            (buildings_[bi].type >= 0 && buildings_[bi].type < static_cast<int>(btypes().size()) &&
             btypes()[buildings_[bi].type].powered && house_low_power(buildings_[bi].house)) || !v.alive || v.die_time>=0)) {
            if (bi < static_cast<int>(buildings_.size())) {
                auto& held = buildings_[bi].w7.mc_victims;
                held.erase(std::remove(held.begin(),held.end(),i),held.end());
            }
            if (v.alive && v.die_time < 0 && v.w7.mc_original_house >= 0)
                set_unit_owner(i,v.w7.mc_original_house);
            v.w7.mc_building = v.w7.mc_original_house = -1;
        }
        if (c >= 0 && (c >= static_cast<int>(units_.size()) || !active(units_[c]) ||
                       units_[c].warp_remaining > 0 || !v.alive || v.die_time>=0 || units_[c].house != v.house)) {
            if (c < static_cast<int>(units_.size())) {
                auto& victims = units_[c].w7.mc_victims;
                victims.erase(std::remove(victims.begin(), victims.end(), i), victims.end());
            }
            if (v.alive && v.die_time < 0 && v.w7.mc_original_house >= 0)
                set_unit_owner(i, v.w7.mc_original_house);
            v.w7.mc_controller = v.w7.mc_original_house = -1;
        }
    }
    for (Unit& u : units_) {
        if (!u.w7.t.mind_control) continue;
        auto& held=u.w7.mc_victims;
        const int ci=static_cast<int>(&u-units_.data());
        held.erase(std::remove_if(held.begin(),held.end(),[&](int vi) {
            return vi<0 || vi>=static_cast<int>(units_.size()) || !units_[vi].alive || units_[vi].die_time>=0 ||
                units_[vi].w7.mc_controller!=ci || units_[vi].house!=u.house;
        }),held.end());
        if (u.w7.t.mc_infinite && active(u)) {
            const auto& t=u.w7.t;
            const auto n=std::min({t.overload_count.size(),t.overload_damage.size(),t.overload_frames.size()});
            std::size_t band=0;
            while (band+1<n && static_cast<int>(held.size())>t.overload_count[band]) ++band;
            if (n && t.overload_damage[band]>0 && static_cast<int>(held.size())>u.w7.t.mc_capacity) {
                if (u.w7.overload_left>0) --u.w7.overload_left;
                else { ability_damage_unit(ci,t.overload_damage[band]); u.w7.overload_left=std::max(1,t.overload_frames[band])-1; }
            } else u.w7.overload_left=0;
        }
        u.w_mind_control = true;
        if (!active(u) || u.deployed) { u.w7.mc_shot = -1; u.target=-1; u.attack_order=false; continue; }
        if (u.target >= 0 && u.target < static_cast<int>(units_.size())) {
            const Unit& v = units_[u.target];
            const bool full = !u.w7.t.mc_infinite &&
                static_cast<int>(u.w7.mc_victims.size()) >= u.w7.t.mc_capacity;
            if (!psionic_target_ok(u.target) || v.an.iron_left_frames > 0 || v.w7.mc_controller >= 0 || v.w7.mc_building >= 0 ||
                (full && (!u.attack_order || u.w7.t.mc_capacity != 1))) {
                u.target = -1; u.attack_order = false;
            }
        }
    }
}

void World::resolve_skirmish_mind_control() {
    if (playfield_.campaign) return;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (!u.w7.t.mind_control) continue;
        const int vi = u.w7.mc_shot; u.w7.mc_shot = -1;
        if (!active(u) || u.deployed || vi < 0 || vi >= static_cast<int>(units_.size()) || vi == i) continue;
        Unit& v = units_[vi];
        if (!active(v) || v.house == u.house || v.is_aircraft || !psionic_target_ok(vi) ||
            v.an.iron_left_frames > 0 || v.w7.mc_controller >= 0 || v.w7.mc_building >= 0) continue;
        auto& held = u.w7.mc_victims;
        if (!u.w7.t.mc_infinite && static_cast<int>(held.size()) >= std::max(1, u.w7.t.mc_capacity)) {
            if (u.w7.t.mc_capacity != 1) continue;
            if (!u.attack_order) { u.target=-1; continue; } // full clones never auto-swap victims
            const int old = held.front();
            if (old >= 0 && old < static_cast<int>(units_.size())) {
                Unit& freed = units_[old];
                if (freed.w7.mc_controller == i) {
                    if (freed.alive && freed.die_time < 0 && freed.w7.mc_original_house >= 0)
                        set_unit_owner(old, freed.w7.mc_original_house);
                    freed.w7.mc_controller = freed.w7.mc_original_house = -1;
                }
            }
            held.clear();
        }
        const int original = v.house;
        set_unit_owner(vi, u.house);
        v.w7.mc_original_house = original; v.w7.mc_controller = i;
        held.push_back(vi);
        u.target = -1; u.attack_order = false;
    }
}

bool World::try_building_mind_control(int bi,int vi,int slot) {
    if (playfield_.campaign || bi<0 || bi>=static_cast<int>(buildings_.size()) ||
        vi<0 || vi>=static_cast<int>(units_.size()) || slot<0 || slot>1) return false;
    Building& b=buildings_[bi]; Unit& v=units_[vi];
    if (!b.alive || b.state!=Building::State::Idle || b.power_off || b.w7.warp_unpowered ||
        b.type<0 || b.type>=static_cast<int>(btypes().size())) return false;
    const auto& type=btypes()[b.type];
    if (!type.mind_control || (type.powered && house_low_power(b.house)) ||
        !active(v) || v.house==b.house || v.is_aircraft || !psionic_target_ok(vi) ||
        v.an.iron_left_frames>0 || v.w7.mc_controller>=0 || v.w7.mc_building>=0 ||
        static_cast<int>(b.w7.mc_victims.size())>=std::max(1,type.weapon[slot].damage)) return false;
    const int original=v.house;
    set_unit_owner(vi,b.house);
    v.w7.mc_original_house=original; v.w7.mc_building=bi;
    b.w7.mc_victims.push_back(vi);
    return true;
}

void World::step_yuri_workers() {
    // An undeployed/destroyed/captured manager releases its workers. A known
    // killer receives freed slaves; absent a killer they retain their house.
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i]; const int manager = u.w7.slave_manager;
        if (manager == -1) continue;
        if (manager < -1) {
            const int mobile = -2-manager;
            if (mobile >= 0 && mobile < static_cast<int>(units_.size()) &&
                active(units_[mobile]) && units_[mobile].house == u.house) {
                const Unit& miner = units_[mobile];
                if (active(u) && !u.moving() && std::max(std::abs(u.cell_x-miner.cell_x),std::abs(u.cell_y-miner.cell_y)) > 2)
                    order_unit_move(i,miner.cell_x,miner.cell_y);
                continue;
            }
            u.w7.slave_manager = -1;
            continue;
        }
        if (manager < static_cast<int>(buildings_.size())) {
            const Building& b = buildings_[manager];
            if (b.alive && b.state != Building::State::Dead && b.house == u.house) continue;
            if (b.killed_by >= 0 && b.killed_by < static_cast<int>(units_.size()) && active(u))
                set_unit_owner(i, units_[b.killed_by].house);
        }
        u.w7.slave_manager = -1; u.is_harvester = false;
        u.harv = HarvState::None; u.dock_refinery = -1;
        u.path.clear(); u.path_idx = 0;
    }
    for (int bi = 0; bi < static_cast<int>(buildings_.size()); ++bi) {
        Building& b = buildings_[bi];
        if (!b.alive || b.state != Building::State::Idle || b.type < 0 ||
            b.type >= static_cast<int>(btypes().size())) continue;
        const BuildingTypeDef& def = btypes()[b.type];
        if (def.slave_count <= 0 || def.slave_combat.image.empty()) continue;
        int count = 0;
        for (const Unit& u : units_)
            if (u.alive && u.die_time < 0 && u.w7.slave_manager == bi) ++count;
        if (count >= def.slave_count) continue;
        if (b.w7.slave_regen_left > 0) { --b.w7.slave_regen_left; continue; }
        const int to_spawn = b.w7.slaves_initialized ? 1 : def.slave_count - count;
        int spawned = 0;
        for (int n = 0; n < to_spawn; ++n) {
            const int id = launch_unit(bi, def, def.slave_combat);
            if (id < 0) break; // blocked exits retry without deleting workers
            make_harvester(id, false, std::max(1, def.slave_storage));
            units_[id].w7.slave_manager = bi;
            units_[id].dock_refinery = bi;
            ++spawned;
        }
        if (spawned > 0) {
            b.w7.slaves_initialized = true;
            b.w7.slave_regen_left = std::max(1, def.slave_regen_rate);
        }
    }
}
} // namespace ra2engine
