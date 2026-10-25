// gF: MCV deploy / undeploy -- a World member TU (the world_abilities.cpp split
// precedent). Every value and rule below is recovered from gamemd.exe; see
// docs/re/mcv-deploy.md for the listings and confidence tags.
//
//   UnitClass deploy mission   FUN_007383E0  (unfold: facing gate -> building)
//   UnitClass try-to-deploy    FUN_00737D50  (order: legal here, else ring search)
//   BuildingType legality      vtbl+0xA8 -> FUN_00464AB0 -> FUN_007154F0
//   AdjacentCell[NW]           0x0089E614 = (-1,-1) (static init FUN_0049F200)
//   DeployFacing               BuildingType+0xEDC, default 0x80 (FUN_0045DD80)
//   Undeploy (sell ConYard)    BuildingClass::Can_Undeploy FUN_0044F5C0
//                              (repair-sell.md §B6)
#include <algorithm>
#include <cmath>
#include <set>

#include "building.hpp"
#include "orig_math.hpp"   // Net L0: om::ftol (_ftol 0x7C4D00) for double->int
#include "world.hpp"

namespace ra2engine {

namespace {

// The recovered 36-entry relocation ring of FUN_00737D50 (CellStruct table at
// 0x00B1BF78..0x00B1C004, statically initialised in that function): first the
// rows NORTH of the MCV (y -1..-4, x 0,+1,-1,+2,-2), then SOUTH (y +1..+4),
// then the row itself west (-1..-4) and east (+1..+4). Order is load-bearing:
// the first legal entry wins.
struct Off { int dx, dy; };
constexpr Off kDeployRing[] = {
    {0, -1}, {0, -2}, {1, -2}, {-1, -2}, {0, -3}, {1, -3}, {-1, -3}, {2, -3}, {-2, -3},
    {0, -4}, {1, -4}, {-1, -4}, {2, -4}, {-2, -4},
    {0, 1},  {0, 2},  {1, 2},  {-1, 2},  {0, 3},  {1, 3},  {-1, 3},  {2, 3},  {-2, 3},
    {0, 4},  {1, 4},  {-1, 4},  {2, 4},  {-2, 4},
    {-1, 0}, {-2, 0}, {-3, 0}, {-4, 0}, {1, 0},  {2, 0},  {3, 0},  {4, 0},
};

bool live_on_map(const Unit& u) { return u.alive && u.die_time < 0.0 && u.cargo_of < 0; }

// 16-bit facing -> the 8-bit DirStruct value the deploy gate compares
// (FUN_007383E0: ((raw >> 7) + 1) >> 1 & 0xFF).
int facing8(double f16) {
    int raw = static_cast<int>(om::lround(f16)) & 0xFFFF;
    return (((raw >> 7) + 1) >> 1) & 0xFF;
}

}  // namespace

bool World::is_mcv(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return false;
    const Unit& u = units_[idx];
    return live_on_map(u) && u.deploys_into >= 0 &&
           u.deploys_into < static_cast<int>(btypes().size());
}

bool World::can_deploy_at(int idx, int x, int y, int* ox, int* oy) const {
    if (!is_mcv(idx)) return false;
    const BuildingTypeDef& d = btypes()[static_cast<std::size_t>(units_[idx].deploys_into)];
    // [FUN_007383E0 @0x738480..0x7384DE] origin = cell, shifted by
    // AdjacentCell[NW] only when the foundation exceeds 2 in either axis.
    int cx = x, cy = y;
    if (d.fw > 2 || d.fh > 2) { cx -= 1; cy -= 1; }
    if (ox) *ox = cx;
    if (oy) *oy = cy;
    // The MCV lifts its own occupancy (vtbl+0x124 Mark(0)) before the test;
    // every other occupied cell blocks. Adjacency is NOT part of the test.
    Occupancy occ = occ_;
    occ.release_all_of(idx, -1, -1);
    const PlacementCheck chk = placement_check(cx, cy, d.fw, d.fh, units_[idx].house, d.adjacent,
                                               passable_, grid_w_, grid_h_, occ, buildings_,
                                               /*bypass_adjacency=*/true, nullptr,
                                               build_mask(/*wall=*/false));   // W8-03-01 [RE: FUN_0047C530 via FUN_007154F0]
    return chk.ok;
}

bool World::order_deploy_mcv(int idx) {
    if (!is_mcv(idx)) return false;
    Unit& u = units_[idx];
    if (u.frozen || u.is_aircraft) return false;
    deploy_any_ = true;
    if (!can_deploy_at(idx,u.cell_x,u.cell_y) && clear_slave_deploy_blockers(idx)) {
        order_stop(idx);
        units_[idx].deploy_mission=1;
        return true;
    }
    if (can_deploy_at(idx, u.cell_x, u.cell_y)) {
        // Legal here: stop and run the deploy mission (turn, then unfold).
        if (u.moving()) {
            occ_.release_all_of(idx, u.cell_x, u.cell_y);
            occ_.set(u.cell_x, u.cell_y, idx);
            u.path.clear();
            u.path_idx = 0;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
        }
        u.target = -1;
        u.target_building = -1;
        u.attack_order = false;
        u.deploy_mission = 1;
        return true;
    }
    // [FUN_00737D50] Illegal here: walk the recovered ring, drive to the first
    // legal spot and deploy on arrival.
    for (const Off& o : kDeployRing) {
        const int x = u.cell_x + o.dx, y = u.cell_y + o.dy;
        if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) continue;
        if (!can_deploy_at(idx, x, y)) continue;
        if (!walkable(x, y) || !occ_.can_enter(x, y, idx)) continue;
        u.target = -1;
        u.target_building = -1;
        u.attack_order = false;
        u.deploy_goal_x = x;
        u.deploy_goal_y = y;
        u.deploy_mission = 2;
        order_unit_move(idx, x, y);
        return true;
    }
    // Nowhere to go: the unfold attempt fails on the spot (EVA for a human).
    deploy_events_.push_back({DeployEvent::Kind::CannotDeploy, idx, -1, u.house});
    u.deploy_mission = 0;
    return false;
}

bool World::order_deploy_mcv_auto(int idx) {
    const auto events=deploy_events_.size();
    const bool accepted=order_deploy_mcv(idx);
    // Saved mission values 3/4 are quiet versions of turn/walk (1/2).
    if(accepted && units_[idx].deploy_mission>0)units_[idx].deploy_mission+=2;
    deploy_events_.erase(std::remove_if(deploy_events_.begin()+events,deploy_events_.end(),
        [](const DeployEvent& e){return e.kind==DeployEvent::Kind::CannotDeploy;}),deploy_events_.end());
    return accepted;
}

bool World::clear_slave_deploy_blockers(int idx) {
    if(!is_mcv(idx))return false;
    const auto& miner=units_[idx];
    const auto& d=btypes()[miner.deploys_into];
    if(d.slave_count<=0)return false; // do not change the ordinary MCV relocation policy
    int x=miner.cell_x,y=miner.cell_y;
    if(d.fw>2 || d.fh>2){--x;--y;}
    auto inside=[&](int cx,int cy){return cx>=x && cy>=y && cx<x+d.fw && cy<y+d.fh;};
    std::vector<int> blockers;
    Occupancy cleared=occ_;cleared.release_all_of(idx,-1,-1);
    for(int i=0;i<static_cast<int>(units_.size());++i) {
        const auto& u=units_[i];
        if(i==idx || !live_on_map(u) || !inside(u.cell_x,u.cell_y))continue;
        if(!are_allied(miner.house,u.house) || u.frozen || u.deployed || u.is_aircraft || u.is_naval || u.deploy_mission)
            return false;
        blockers.push_back(i);cleared.release_all_of(i,-1,-1);
    }
    if(blockers.empty())return false;
    // Never scatter units for a footprint still blocked by terrain, a building,
    // an enemy, a reservation or another static obstacle.
    if(!placement_check(x,y,d.fw,d.fh,miner.house,d.adjacent,passable_,grid_w_,grid_h_,cleared,
        buildings_,true,nullptr,build_mask(false)).ok)return false;
    bool waiting=false;std::set<std::pair<int,int>> goals;
    for(int i:blockers) {
        auto& u=units_[i];
        if(u.moving() && u.wait_time<3.0 && !inside(u.path.back().x,u.path.back().y)) {
            goals.emplace(u.path.back().x,u.path.back().y);waiting=true;continue;
        }
        bool routed=false;
        for(int r=1;r<=4 && !routed;++r)for(int dy=-r;dy<=r && !routed;++dy)for(int dx=-r;dx<=r && !routed;++dx) {
            if(std::abs(dx)!=r && std::abs(dy)!=r)continue;
            const int gx=u.cell_x+dx,gy=u.cell_y+dy;
            if(inside(gx,gy) || goals.count({gx,gy}) || !unit_can_enter_cell(u,gx,gy) ||
               building_at_cell(gx,gy)>=0 || !occ_.can_enter(gx,gy,i,u.on_deck))continue;
            order_unit_move(i,gx,gy);
            if(u.moving() && u.path.back().x==gx && u.path.back().y==gy) {
                goals.emplace(gx,gy);waiting=true;routed=true;
            }
        }
    }
    return waiting;
}

void World::finish_deploy(int idx) {
    Unit& u = units_[idx];
    const int type = u.deploys_into;
    const BuildingTypeDef& d = btypes()[static_cast<std::size_t>(type)];
    int ox = 0, oy = 0;
    can_deploy_at(idx, u.cell_x, u.cell_y, &ox, &oy);
    const double ratio = u.max_hp > 0 ? static_cast<double>(u.hp) / u.max_hp : 1.0;
    const int house = u.house;
    // The unit leaves first (its cell becomes part of the foundation).
    consume_unit(idx);
    units_[idx].deployed_away = true;
    const double buildup = (type < static_cast<int>(buildup_totals_.size()))
                               ? buildup_totals_[static_cast<std::size_t>(type)]
                               : 0.0;
    // Mission Construction (0x12) = the buildup animation [FUN_007383E0 ->
    // vtbl+0x1E8(0x12)]; instant only when the type has no buildup art.
    const int b = add_building(type, ox, oy, d.fw, d.fh, house, d.strength, d.armor_ix, buildup,
                               /*instant=*/buildup <= 0.0, d.sight, d.grants_map_reveal);
    // Health carried as a ratio, floored at 1 [FUN_007C4D00 ftol, min 1].
    Building& nb = buildings_[static_cast<std::size_t>(b)];
    nb.hp = std::max(1, om::ftol(ratio * nb.max_hp));
    nb.ai_repairable = !human_commanded(house);   // fv7c [RE: FUN_007383E0, non-human deploy]
    if (d.slave_count > 0) {
        for (int wi = 0; wi < static_cast<int>(units_.size()); ++wi) {
            Unit& worker = units_[wi];
            if (worker.w7.slave_manager != -2-idx || !live_on_map(worker) || worker.house != house) continue;
            order_stop(wi);
            worker.w7.slave_manager = b;
            make_harvester(wi, false, std::max(1,d.slave_storage));
            worker.manual_hold = false;
            worker.dock_refinery = b;
        }
    }
    deploy_events_.push_back({DeployEvent::Kind::Deployed, idx, b, house});
}

void World::step_deploy(double dt) {
    if (!deploy_any_) return;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (u.deploy_mission == 0) continue;
        if (!live_on_map(u) || u.deploys_into < 0) { u.deploy_mission = 0; continue; }
        const bool quiet=u.deploy_mission>=3;
        if (u.deploy_mission == 2 || u.deploy_mission==4) {
            if (u.moving()) {
                // A new player order re-pathed the unit elsewhere: drop the deploy.
                const Cellxy& g = u.path.back();
                if (g.x != u.deploy_goal_x || g.y != u.deploy_goal_y) u.deploy_mission = 0;
                continue;
            }
            u.deploy_mission = quiet ? 3 : 1;   // arrived: attempt the unfold here
        }
        if (u.moving()) { u.deploy_mission = 0; continue; }   // re-ordered mid-turn
        if (!can_deploy_at(i, u.cell_x, u.cell_y)) {
            if(clear_slave_deploy_blockers(i))continue;
            // [FUN_007383E0 @0x738500] failure on the spot -> EVA_CannotDeployHere
            // (human player) and back to Guard.
            if(!quiet)deploy_events_.push_back({DeployEvent::Kind::CannotDeploy, i, -1, u.house});
            u.deploy_mission = 0;
            continue;
        }
        const int want8 = btypes()[static_cast<std::size_t>(u.deploys_into)].deploy_facing & 0xFF;
        if (facing8(u.body_facing) != want8) {
            const double want16 = static_cast<double>(want8 << 8);
            const double step = u.rot_u16_per_sec > 0.0 ? u.rot_u16_per_sec * dt : 65536.0;
            u.body_facing = turn_toward(u.body_facing, want16, step);
            u.turret_facing = u.body_facing;
            if (facing8(u.body_facing) != want8) continue;   // still turning
        }
        finish_deploy(i);
    }
}

std::vector<World::DeployEvent> World::take_deploy_events() {
    std::vector<DeployEvent> out;
    out.swap(deploy_events_);
    return out;
}

void World::set_undeploy_unit(int btype, const UnitCombat& c) {
    for (std::size_t k = 0; k < undeploy_types_.size(); ++k)
        if (undeploy_types_[k] == btype) { undeploy_units_[k] = c; return; }
    undeploy_types_.push_back(btype);
    undeploy_units_.push_back(c);
}

// [RE: repair-sell.md §B6, FUN_0044F5C0 + Mission_Selling 0x00449C2A]: a
// ConstructionYard packs back into its UndeploysInto unit only in a
// multiplayer/skirmish session, for a human owner, with MCVRedeploys on, and
// when the unit can be placed; health ratio carried (min 1). Returns true when
// the building was converted (the caller then skips the refund).
bool World::try_undeploy(int b_idx) {
    if (!skirmish_session_ || !mcv_redeploys_) return false;
    Building& b = buildings_[static_cast<std::size_t>(b_idx)];
    if (ai_enabled(b.house)) return false;   // !Is_Human(Owner) -> plain refund
    const BuildingTypeDef& t = btypes()[static_cast<std::size_t>(b.type)];
    if (!t.conyard) return false;
    std::size_t k = 0;
    while (k < undeploy_types_.size() && undeploy_types_[k] != b.type) ++k;
    if (k == undeploy_types_.size()) return false;
    // Inverse of the deploy offset: the MCV stands where it unfolded.
    int ux = b.cx, uy = b.cy;
    if (b.fw > 2 || b.fh > 2) { ux += 1; uy += 1; }
    if (!walkable(ux, uy) || !occ_.is_free(ux, uy)) return false;   // fallback: refund
    const double ratio = b.max_hp > 0 ? static_cast<double>(b.hp) / b.max_hp : 1.0;
    const int house = b.house;
    Unit& u = add_combat_unit(ux, uy, house, undeploy_units_[k]);
    u.hp = std::max(1, om::ftol(ratio * u.max_hp));
    u.body_facing = u.turret_facing =
        static_cast<double>((t.deploy_facing & 0xFF) << 8);
    const int ui = static_cast<int>(units_.size()) - 1;
    deploy_events_.push_back({DeployEvent::Kind::Undeployed, ui, b_idx, house});
    return true;
}

bool World::can_pack_conyard(int bi) const {
    if(!skirmish_session_ || !mcv_redeploys_ || bi<0 || bi>=static_cast<int>(buildings_.size()))return false;
    const auto& b=buildings_[bi];
    if(!b.alive || b.state!=Building::State::Idle || b.selling || b.c4_applied || ai_enabled(b.house) ||
       b.type<0 || b.type>=static_cast<int>(btypes().size()) || !btypes()[b.type].conyard)return false;
    if(std::find(undeploy_types_.begin(),undeploy_types_.end(),b.type)==undeploy_types_.end())return false;
    const int x=b.cx+(b.fw>2 || b.fh>2),y=b.cy+(b.fw>2 || b.fh>2);
    return occ_.is_free(x,y);
}
bool World::order_pack_conyard(int bi,int house) {
    return can_pack_conyard(bi) && buildings_[bi].house==house && begin_sell(bi,house);
}

bool World::can_pack_slave_miner(int bi) const {
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return false;
    const Building& b = buildings_[bi];
    return b.alive && b.state == Building::State::Idle && !b.selling &&
        b.type >= 0 && b.type < static_cast<int>(btypes().size()) &&
        btypes()[b.type].slave_count > 0 &&
        std::find(undeploy_types_.begin(), undeploy_types_.end(), b.type) != undeploy_types_.end();
}

bool World::order_pack_slave_miner(int bi, int house) {
    if (!can_pack_slave_miner(bi) || buildings_[bi].house != house) return false;
    Building& b = buildings_[bi];
    const auto k = static_cast<std::size_t>(std::find(undeploy_types_.begin(), undeploy_types_.end(), b.type) - undeploy_types_.begin());
    // Validate before changing the footprint: blocked packing must never sell it.
    const int x = b.cx, y = b.cy;
    if (!occ_.is_free(x,y)) return false;
    const double ratio = b.max_hp > 0 ? static_cast<double>(b.hp) / b.max_hp : 1.0;
    for (const Cellxy& c : foundation_cells(b.cx,b.cy,b.fw,b.fh))
        if (c.x >= 0 && c.y >= 0 && c.x < grid_w_ && c.y < grid_h_)
            passable_[static_cast<std::size_t>(c.y)*grid_w_+c.x] = 1;
    b.blocking_released = true;
    const int facing = btypes()[b.type].deploy_facing;
    kill_building(b); b.sold = true; b.selling = false; b.being_repaired = false;
    Unit& u = add_combat_unit(x,y,house,undeploy_units_[k]);
    u.hp = std::max(1,om::ftol(ratio*u.max_hp));
    u.body_facing = u.turret_facing = static_cast<double>((facing & 255) << 8);
    const int mobile = static_cast<int>(units_.size())-1;
    for (int wi = 0; wi < mobile; ++wi) {
        Unit& worker = units_[wi];
        if (worker.w7.slave_manager != bi || !live_on_map(worker) || worker.house != house) continue;
        order_stop(wi);
        worker.w7.slave_manager = -2-mobile;
        worker.is_harvester = false; worker.harv = HarvState::None; worker.dock_refinery = -1;
    }
    deploy_events_.push_back({DeployEvent::Kind::Undeployed,mobile,bi,house});
    step_yuri_workers(false); // packing updates followers without recursive relocation
    return true;
}

void World::reshroud_area(int house, int cx, int cy, int radius) {
    if (house < 0 || house >= house_count()) return;
    if (house < static_cast<int>(map_reveal_.size()) && map_reveal_[house]) return;
    if (explored_.size() <= static_cast<std::size_t>(house) || explored_[house].empty()) return;
    for (int y = cy - radius; y <= cy + radius; ++y)
        for (int x = cx - radius; x <= cx + radius; ++x) {
            if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > radius * radius) continue;
            const std::size_t i = static_cast<std::size_t>(y) * grid_w_ + x;
            explored_[house][i] = 0;
            if (!visible_[house].empty()) visible_[house][i] = 0;
        }
}

}  // namespace ra2engine
