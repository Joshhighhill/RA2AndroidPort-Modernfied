// Gap fix C -- the World side of the Stop / Guard / Scatter orders (gap owner
// B binds the S / G / X keys and Android buttons to these). [RE: FootClass
// Mission_Stop / Mission_Guard / Scatter are the standard Westwood missions;
// the clone's model:] stop = finish the cell being entered, drop every order;
// guard = stop + guard post at the current cell (idle units auto-acquire and
// are leashed to it -- combat.cpp); scatter = step to a free adjacent cell.
#include <cmath>

#include "world.hpp"

namespace ra2engine {

namespace {
bool orderable(const Unit& u) { return u.alive && u.die_time < 0.0 && u.cargo_of < 0; }
}  // namespace

void World::order_stop(int idx) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[idx];
    if (!orderable(u)) return;
    u.attack_cell_x = u.attack_cell_y = -1;   // G16: Stop ends a ground fire order
    u.occupy_target = -1;
    if (u.is_aircraft) {
        // W2-AN-07: Stop halts a hovering jumpjet where it is (the cruise goal
        // is dropped; a landable Night Hawk then descends); a fixed-wing
        // aircraft abandons its sortie and returns to its airfield.
        u.target = -1; u.target_building = -1; u.attack_order = false;
        u.has_flight_goal = false;
        u.an.attack_unit = u.an.attack_building = -1;
        u.an.attack_x = u.an.attack_y = -1;
        if (u.an.t.fixed_wing && u.an.sortie != 0) u.an.sortie = 3;   // RTB
        return;
    }
    // A unit standing on its cell stops right there; one between cells finishes
    // entering the cell already reserved, then stands (no backward snap).
    if (u.moving()) {
        const bool on_cell = std::abs(u.fx - u.cell_x) < 1e-6 && std::abs(u.fy - u.cell_y) < 1e-6;
        if (on_cell) {
            occ_.release_all_of(idx, u.cell_x, u.cell_y);
            occ_.set(u.cell_x, u.cell_y, idx);
            u.path.clear();
            u.path_idx = 0;
        } else {
            u.path.resize(u.path_idx + 1);
        }
    }
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.enter_target = -1;
    u.capture_target = -1;
    u.infiltrate_target = -1;
    u.unload_pending = false;
    u.has_flight_goal = false;
    const Cellxy post = u.moving() ? u.path.back() : Cellxy{u.cell_x, u.cell_y};
    u.guard_x = post.x;
    u.guard_y = post.y;
    u.w6.area_guard = false;   // W6-02-11: Stop leaves Area_Guard
}

void World::order_guard(int idx) {
    // Guard = stop and hold this post (the guard anchor step_combat leashes
    // auto-acquired engagements to).
    order_stop(idx);
    // W6-02-11 [RE: GuardObject FUN_0072FE30 = MegaMission(Area_Guard)]: the
    // doubled area-guard threat radius.
    if (idx >= 0 && idx < static_cast<int>(units_.size()) && orderable(units_[idx]) && !units_[idx].is_aircraft)
        units_[idx].w6.area_guard = true;
}

void World::order_scatter(int idx) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[idx];
    if (!orderable(u) || u.frozen || u.is_aircraft) return;
    static const int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    const int start = combat_rng_.random(0, 7);   // deterministic sync-RNG pick
    for (int k = 0; k < 8; ++k) {
        const int d = (start + k) & 7;
        const int x = u.cell_x + kDx[d], y = u.cell_y + kDy[d];
        // W5-B R-061: with sub-cells an infantry may scatter into a cell that
        // still has a free spot.
        const bool taken = occ_.subcells() ? !occ_.can_enter(x, y, idx) : unit_at_cell(x, y) >= 0;
        if (!walkable(x, y) || taken || building_at_cell(x, y) >= 0) continue;
        u.target = -1;
        u.target_building = -1;
        u.attack_order = false;
        order_unit_move(idx, x, y);
        return;
    }
}

void World::order_stop_selected() {
    for (int i = 0; i < static_cast<int>(units_.size()); ++i)
        if (units_[i].selected && units_[i].house == 0) order_stop(i);
}
void World::order_guard_selected() {
    for (int i = 0; i < static_cast<int>(units_.size()); ++i)
        if (units_[i].selected && units_[i].house == 0) order_guard(i);
}
void World::order_scatter_selected() {
    for (int i = 0; i < static_cast<int>(units_.size()); ++i)
        if (units_[i].selected && units_[i].house == 0) order_scatter(i);
}

}  // namespace ra2engine
