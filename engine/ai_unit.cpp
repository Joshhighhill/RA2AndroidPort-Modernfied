// Wave-7 AI per-unit rules: the computer houses' Deployer-infantry auto-deploy
// (W7-01-08 / W7-02-04) and the ToProtect call for help (W7-04-18). A World
// member TU split out of world.cpp; runs after step_ai_teams inside the
// ai_any_ firewall, and only for houses whose AiHouseConfig arms it
// (auto_deploy_frames >= 0 / roster_to_protect non-empty: the skirmish AI).
#include "world.hpp"
#include "combat.hpp"   // unit_lept_x/y/z, building_aim_lept

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ra2engine {

namespace {

// Is (tx,ty,tz) within the DEPLOY weapon's Range of `u`? [RE: vtbl+0x3A8
// InRange(target, DeployFireWeapon) @0x5220A6; RE-FALLBACK: the plain 3-D
// lepton distance, without the elevation bonus of the active-weapon test].
bool in_deploy_range(const Unit& u, int tx, int ty, int tz) {
    // DeployFireWeapon (type+0x6A8): slot 1 for every stock deployer (GI Para,
    // GGI MissileLauncher; the elite slot once elite), slot 0 = the primary.
    double range = u.w_range;
    if (u.deploy_fire_slot == 1) {
        const WeaponSlot& w = (u.veteran_rank >= 2 && u.w2_elite.valid) ? u.w2_elite : u.w2;
        if (!w.valid) return false;
        range = w.range;
    }
    const long long r = (long long)(range * 256.0);
    const long long dx = tx - unit_lept_x(u), dy = ty - unit_lept_y(u), dz = tz - unit_lept_z(u);
    return dx * dx + dy * dy + dz * dz <= r * r;
}

}  // namespace

void World::step_ai_unit_rules() {
    const std::int64_t now = static_cast<std::int64_t>(repair_frame_);
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[(std::size_t)h];
        if (!rec.is_ai) continue;
        const int delay = rec.cfg.auto_deploy_frames;
        if (delay < 0) continue;
        // W7-01-08 / W7-02-04 [RE: InfantryClass FUN_00521050 (Guard /
        // Area_Guard, via 0x51F330 / 0x51F350) and FUN_00522070 (Attack)]:
        //  * a computer-owned Deployer + DeployFire infantry whose type is NOT
        //    ImmuneToRadiation (+0xD37) and has no UndeployDelay (+0x6C4 < 0)
        //    deploys once it has stood in Guard for AIAutoDeployFrameDelay
        //    frames (Frame > CurrentMissionStartTime +0xC0 + delay);
        //  * in Attack it deploys when the target is inside its deploy weapon's
        //    range and undeploys when the target is not (a deploy weapon with
        //    AreaFire -- the Desolator's eruption -- is excluded @0x5220C0);
        //  * a move order undeploys it first (0x51F370; order_unit_move does).
        // In the clone that is exactly the deploy_fire infantry (GI / GGI): the
        // Desolator (ImmuneToRadiation, AreaFire eruption) is excluded by both
        // the Guard and the Attack branch, so it deploys only through a team
        // script Deploy line, as in the original (W7-02-04 corrected).
        // [RE-FALLBACK: the ArchiveTarget cell test @0x5210E0 is not modelled
        // -- skirmish AI units carry no archive target.]
        for (int i = 0; i < (int)units_.size(); ++i) {
            Unit& u = units_[(std::size_t)i];
            if (u.house != h || !u.alive || u.die_time >= 0.0 || !u.deploy_fire) continue;
            if (u.cargo_of >= 0 || u.garrison_of >= 0 || u.frozen || u.abil.immune_radiation) {
                u.ai.guard_since = -1;
                continue;
            }
            int tx = 0, ty = 0, tz = 0;
            bool has_target = false;
            if (u.target >= 0 && u.target < (int)units_.size() && units_[(std::size_t)u.target].alive) {
                const Unit& t = units_[(std::size_t)u.target];
                tx = unit_lept_x(t); ty = unit_lept_y(t); tz = unit_lept_z(t);
                has_target = true;
            } else if (u.target_building >= 0 && u.target_building < (int)buildings_.size() &&
                       buildings_[(std::size_t)u.target_building].alive) {
                const Building& b = buildings_[(std::size_t)u.target_building];
                building_aim_lept(b, cell_level(b.cx, b.cy), tx, ty, tz);
                has_target = true;
            }
            if (u.deployed) {
                u.ai.guard_since = -1;
                // Attack: the deployed post cannot reach the target -> undeploy
                // (Sequence Undeploy 0x1F) and pursue it.
                if (u.attack_order && has_target && !in_deploy_range(u, tx, ty, tz)) order_deploy(i);
                continue;
            }
            if (u.attack_order) {
                u.ai.guard_since = -1;
                // Attack: the target is inside the deploy weapon's range ->
                // deploy (Sequence Deploy 0x1B); the post keeps the target.
                if (has_target && in_deploy_range(u, tx, ty, tz)) order_deploy(i);
                continue;
            }
            if (u.moving() || u.enter_target >= 0 || u.occupy_target >= 0) {
                u.ai.guard_since = -1;
                continue;
            }
            if (u.ai.guard_since < 0) {
                u.ai.guard_since = now;   // entered Guard this frame
                continue;
            }
            if (now > u.ai.guard_since + delay) order_deploy(i);
        }
    }
    // W7-04-18: this step's ToProtect hits, in arrival order.
    for (const AiProtectReq& r : ai_protect_reqs_) {
        if (r.victim < 0 || r.victim >= (int)units_.size()) continue;
        const int h = units_[(std::size_t)r.victim].house;
        if (h < 0 || h >= (int)ai_houses_.size() || !ai_houses_[(std::size_t)h].is_ai) continue;
        ai_protect(h, ai_houses_[(std::size_t)h], r.victim, r.attacker_house);
    }
    ai_protect_reqs_.clear();
}

// [RE: FUN_00707500, called from TechnoClass::ReceiveDamage @0x701C74 for a
// ToProtect victim] -- refused when the victim's owner is human, the attacker
// is allied, in limbo, or not an Infantry/Unit (aircraft and buildings are
// ignored), or the victim type is Insignificant. It then walks the infantry and
// unit arrays for same-house objects that are recruitable, not in a team (or in
// a base-defense team), reachable in their movement zone (FUN_0056CC30) and
// able to fire on the attacker (GetFireError != ILLEGAL), keeps the best six by
// threat value and orders them to attack. [RE-FALLBACK: the six are picked
// nearest-to-the-victim first instead of by threat value (FUN_004D96B0) and
// the Rules+0xB18 threat budget is not modelled; a reinforcement is sent only
// while no other own unit already attacks that attacker, standing in for the
// attacker's +0x650 cooldown timer.]
void World::ai_protect(int house, const HouseAiRec& rec, int victim, int attacker_house) {
    const Unit& v = units_[(std::size_t)victim];
    if (!v.alive || v.die_time >= 0.0 || v.tg.t.insignificant) return;
    if (attacker_house < 0 || !is_enemy(house, attacker_house)) return;
    int a = -1;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[(std::size_t)i];
        if (u.house == attacker_house && u.alive && u.die_time < 0.0 && u.target == victim && !u.is_aircraft &&
            u.cargo_of < 0) {
            a = i;
            break;
        }
    }
    if (a < 0) return;
    const Unit& au = units_[(std::size_t)a];
    std::vector<char> in_team(units_.size(), 0);
    for (const TeamInstance& t : rec.team_state.teams)
        for (int m : t.members)
            if (m >= 0 && m < (int)units_.size()) in_team[(std::size_t)m] = 1;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[(std::size_t)i];
        if (i != victim && u.house == house && u.alive && u.die_time < 0.0 && u.target == a && u.attack_order)
            return;   // help is already on its way
    }
    struct Cand { long long d2; int idx; };
    std::vector<Cand> cands;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[(std::size_t)i];
        if (i == victim || u.house != house || !u.alive || u.die_time >= 0.0 || in_team[(std::size_t)i]) continue;
        if (u.is_harvester || u.cargo_of >= 0 || u.garrison_of >= 0 || u.frozen) continue;
        if (u.moving() || u.attack_order || u.target >= 0 || u.target_building >= 0) continue;
        if (u.w_damage <= 0 || !u.w_ag) continue;   // must be able to fire on a ground attacker
        if (u.an.t.fixed_wing || u.an.t.spawned || u.an.t.missile) continue;
        if (u.deployed && !u.deploy_fire) continue;   // a deployed Desolator cannot go
        // Movement zone: a ship helps only against an attacker on water; a
        // land unit only against one it can walk to (same domain or amphibious).
        if (u.movement_domain == MovementDomain::Water && au.movement_domain != MovementDomain::Water) continue;
        if (u.movement_domain == MovementDomain::Land && au.movement_domain == MovementDomain::Water) continue;
        const long long dx = u.cell_x - v.cell_x, dy = u.cell_y - v.cell_y;
        cands.push_back(Cand{dx * dx + dy * dy, i});
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.d2 < y.d2; });
    for (std::size_t k = 0; k < cands.size() && k < 6; ++k) order_unit_attack(cands[k].idx, a);
}

}  // namespace ra2engine
