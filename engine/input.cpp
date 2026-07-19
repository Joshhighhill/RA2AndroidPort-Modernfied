#include "input.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>

#include "flight.hpp"         // altitude_to_screen_dy (CB-2 raised-hitbox pick)
#include "iso_project.hpp"    // screen_to_cell
#include "ra2assets/map.hpp"  // MapCell (level lookup for the level-aware ground target)

namespace ra2engine {

static constexpr int kTileW = 60;
static constexpr int kTileH = 30;
static constexpr int kLevelPx = 15;

// Selection hit box around a unit's on-screen ground point (its feet): about
// half a tile to each side, most of a sprite height up, a little below.
// Restored from the Phase-2 tap hit-test: picking must be done in SCREEN
// space against the LEVEL-AWARE ground point the sprite is actually drawn at
// -- a bare screen_to_cell inversion is level-blind and resolves a full cell
// off for a unit standing on level-2 terrain.
static constexpr int kHitHalfW = 32;
static constexpr int kHitUp = 46;
static constexpr int kHitDown = 16;

// Pointer travel (window px) past which a left press becomes a selection-box
// drag rather than a click.
static constexpr int kDragThreshold = 6;
// Double-click window (ms) and max pixel slop between the two clicks.
static constexpr uint32_t kDblMs = 400;
static constexpr int kDblSlop = 6;

Input::Input(World& world, Camera& camera, int view_w, int view_h)
    : world_(world), cam_(camera), view_w_(view_w), view_h_(view_h) {
    int mnx, mny, mxx, mxy;
    if (terrain_screen_bounds(world_.map(), mnx, mny, mxx, mxy)) {
        const int margin_x = view_w_ / 2;
        const int margin_y = view_h_ / 2;
        min_cam_x_ = -margin_x - mxx;
        max_cam_x_ = view_w_ + margin_x - mnx;
        min_cam_y_ = -margin_y - mxy;
        max_cam_y_ = view_h_ + margin_y - mny;
    } else {
        min_cam_x_ = INT_MIN; max_cam_x_ = INT_MAX;
        min_cam_y_ = INT_MIN; max_cam_y_ = INT_MAX;
    }
}

void Input::clamp_camera() {
    if (cam_.x < min_cam_x_) cam_.x = min_cam_x_;
    else if (cam_.x > max_cam_x_) cam_.x = max_cam_x_;
    if (cam_.y < min_cam_y_) cam_.y = min_cam_y_;
    else if (cam_.y > max_cam_y_) cam_.y = max_cam_y_;
}

void Input::pan(int dx, int dy) {
    cam_.x += dx;
    cam_.y += dy;
    clamp_camera();
}

void Input::unit_ground_px(const Unit& u, int& gx, int& gy) const {
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    gx = static_cast<int>(std::lround((u.fx - u.fy) * hw)) + kTileW / 2 + cam_.x;
    gy = static_cast<int>(std::lround((u.fx + u.fy) * hh - u.level * kLevelPx)) + kTileH / 2 +
         cam_.y;
}

int Input::unit_under(int px, int py) const {
    // Screen-space pick: test the click against every unit's hit box around
    // its level-aware ground point (unit_ground_px -- identical anchor to
    // draw_unit). Among units whose box contains the click, the NEAREST
    // ground point wins; ties break to the lowest index (deterministic).
    // Corpses are unpickable ANYWHERE this is used -- including the
    // right-click attack path -- so the alive/die_time check lives here once
    // rather than being repeated by every caller.
    const std::vector<Unit>& us = world_.units();
    int best = -1;
    long best_d2 = 0;
    for (std::size_t i = 0; i < us.size(); ++i) {
        const Unit& u = us[i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;  // dead/dying/
                                                     // CARRIED: unpickable (CB-1)
        int gx, gy;
        unit_ground_px(u, gx, gy);
        // CB-2 Aircraft [RE: aircraft.md §3]: an airborne unit is drawn RAISED
        // by altitude_to_screen_dy, so its hit box lives at the raised screen
        // position; shift the ground point up to match. A grounded aircraft
        // (altitude 0) picks normally. Interactive-only -- the scripted demos
        // don't hit-test, so this never affects a gate.
        if (u.is_aircraft && u.altitude > 0.0)
            gy -= ra2engine::altitude_to_screen_dy(u.altitude);
        if (px < gx - kHitHalfW || px > gx + kHitHalfW) continue;
        if (py < gy - kHitUp || py > gy + kHitDown) continue;
        const long dx = px - gx, dy = py - gy;
        const long d2 = dx * dx + dy * dy;
        if (best < 0 || d2 < best_d2) {
            best = static_cast<int>(i);
            best_d2 = d2;
        }
    }
    return best;
}

void Input::resolve_ground_cell(int px, int py, int& cx, int& cy) const {
    // LEVEL-AWARE two-pass inversion, identical to the right-click move path
    // (see handle()'s PointerDown/Right branch for the full rationale). Pass 1
    // inverts at level 0; if the resolved cell's real terrain level > 0, add
    // the raise back to py and re-invert once.
    int cx0 = 0, cy0 = 0;
    screen_to_cell(px - cam_.x - kTileW / 2, py - cam_.y - kTileH / 2, kTileW, kTileH, cx0, cy0);
    const ra2assets::MapCell* cell0 = world_.map().at(cx0, cy0);
    const int level = cell0 ? cell0->level : 0;
    cx = cx0;
    cy = cy0;
    if (level > 0) {
        screen_to_cell(px - cam_.x - kTileW / 2, py - cam_.y - kTileH / 2 + level * kLevelPx,
                       kTileW, kTileH, cx, cy);
    }
}

void Input::begin_placement(int roster_type, int fw, int fh) {
    place_type_ = roster_type;
    place_fw_ = fw > 0 ? fw : 1;
    place_fh_ = fh > 0 ? fh : 1;
    // Any in-flight left gesture is abandoned when placement begins.
    l_down_ = false;
    dragging_ = false;
}

void Input::cancel_placement() {
    place_type_ = -1;
    place_fw_ = place_fh_ = 1;
}

void Input::placement_cell(int& cx, int& cy) const {
    resolve_ground_cell(cur_x_, cur_y_, cx, cy);
    cx -= place_fw_ / 2;
    cy -= place_fh_ / 2;
}

int Input::lowest_selected() const {
    const std::vector<Unit>& us = world_.units();
    for (std::size_t i = 0; i < us.size(); ++i)
        if (us[i].selected) return static_cast<int>(i);
    return -1;
}

bool Input::selection_box(int& x0, int& y0, int& x1, int& y1) const {
    if (!(l_down_ && dragging_)) return false;
    x0 = std::min(down_x_, cur_x_);
    x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_);
    y1 = std::max(down_y_, cur_y_);
    return true;
}

bool Input::commit_factory_click(int px, int py) {
    // Resolve the clicked ground cell, then the building (if any) on it. The
    // factory gesture is OWN-HOUSE GATED [Task 5 review]: only an alive,
    // house-0 building whose type is a unit factory can ever be selected,
    // rallied, or primed -- an enemy factory click falls through to normal
    // selection (and clears any prior factory selection).
    int cx = 0, cy = 0;
    resolve_ground_cell(px, py, cx, cy);
    const int b = world_.building_at_cell(cx, cy);
    if (b >= 0 && world_.buildings()[b].house == 0 && world_.buildings()[b].alive &&
        is_unit_factory_type(world_.buildings()[b].type)) {
        if (selected_building_ == b) {
            // Re-click the selected factory -> set primary [RE: production.md
            // §1.2, RE-FALLBACK: original toggles by right-click; our right-
            // click sets the rally, so LEFT re-click primes]. Resolve the
            // building's category so set_primary clears only its sibling group.
            const int t = world_.buildings()[b].type;
            std::vector<int> cat;
            for (int x : inf_factory_types_)
                if (x == t) { cat = inf_factory_types_; break; }
            if (cat.empty())
                for (int x : veh_factory_types_)
                    if (x == t) { cat = veh_factory_types_; break; }
            if (cat.empty()) cat = {t};
            world_.set_primary(b, cat);
        } else {
            selected_building_ = b;
            world_.clear_selection();  // factory selection clears the unit selection
        }
        return true;
    }
    selected_building_ = -1;  // click NOT on an own factory: drop the selection
    return false;
}

void Input::commit_click(int px, int py, bool shift) {
    // Left-click never selects a hostile: a pick that lands on a house != 0
    // unit is treated exactly like a pick on empty ground (falls through to
    // the clear-selection branch below).
    int idx = unit_under(px, py);
    if (idx >= 0 && world_.units()[idx].house != 0) idx = -1;
    if (idx >= 0) {
        if (shift) {
            world_.units()[idx].selected = !world_.units()[idx].selected;  // toggle
        } else {
            world_.clear_selection();
            world_.units()[idx].selected = true;
        }
    } else if (!shift) {
        world_.clear_selection();  // click on empty ground (or a hostile) clears (Shift+empty = no-op)
    }
    const int lo = lowest_selected();
    if (lo >= 0) { actions_.selected = true; actions_.actor = lo; }
}

void Input::commit_box(bool shift) {
    int x0, y0, x1, y1;
    x0 = std::min(down_x_, cur_x_); x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_); y1 = std::max(down_y_, cur_y_);
    if (!shift) world_.clear_selection();  // plain box replaces; Shift+box adds
    for (Unit& u : world_.units()) {
        // A box drag never selects hostiles, corpses, or CARRIED units --
        // house != 0, dead/dying, and cargo_of >= 0 (aboard a transport,
        // off-map [CB-1]) are skipped even if their ground point falls
        // inside the dragged rectangle.
        if (u.house != 0 || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= x0 && gx <= x1 && gy >= y0 && gy <= y1) u.selected = true;
    }
    const int lo = lowest_selected();
    if (lo >= 0) { actions_.selected = true; actions_.actor = lo; }
}

void Input::commit_double(int px, int py) {
    // Same house-0-only rule as commit_click: a double-click landing on a
    // hostile does nothing (no type-select, no selection change).
    const int idx = unit_under(px, py);
    if (idx < 0 || world_.units()[idx].house != 0) return;
    const std::string type = world_.units()[idx].image;
    world_.clear_selection();
    for (Unit& u : world_.units()) {
        // The "same type" scan is also house-0-only and skips corpses AND
        // CARRIED units (a hostile, a dead unit, or one aboard a transport
        // (cargo_of >= 0 [CB-1]) sharing the clicked unit's image must never
        // join the selection).
        if (u.house != 0 || u.image != type || !u.alive || u.die_time >= 0.0 ||
            u.cargo_of >= 0)
            continue;
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= 0 && gx < view_w_ && gy >= 0 && gy < view_h_) u.selected = true;  // on-screen
    }
    const int lo = lowest_selected();
    if (lo >= 0) { actions_.selected = true; actions_.actor = lo; }
}

void Input::handle(const InputEvent& ev) {
    // NOTE: actions_ is deliberately NOT touched here -- it ACCUMULATES
    // across handle() calls (the commit/order sites below OR their flags in)
    // and is cleared only by take_actions(), the game loop's once-per-frame
    // consume point. See InputActions in input.hpp for the rationale.

    // CB-4 Superweapons [spec §4]: while SW-targeting is armed, a left Down+Up
    // (no drag) commits the target cell -- resolved via the SAME level-aware
    // screen->cell pick the placement path uses (resolve_ground_cell) -- and
    // disarms; a right-click cancels; PointerMove just tracks the cursor.
    // Checked BEFORE placement/selection so an armed SW consumes the click
    // first. UI state only -- the sim is untouched until main.cpp calls
    // World::fire_superweapon. No existing flow arms this, so this branch is
    // never entered in any existing demo. Quit still propagates.
    if (sw_targeting_) {
        switch (ev.kind) {
            case InputEvent::Quit:
                quit_ = true;
                break;
            case InputEvent::PointerMove:
                cur_x_ = ev.x;
                cur_y_ = ev.y;
                break;
            case InputEvent::PointerDown:
                if (ev.button == InputEvent::Right) {
                    sw_targeting_ = false;   // cancel
                } else {
                    l_down_ = true;
                    dragging_ = false;
                    down_x_ = cur_x_ = ev.x;
                    down_y_ = cur_y_ = ev.y;
                }
                break;
            case InputEvent::PointerUp:
                if (ev.button == InputEvent::Left && l_down_ && !dragging_) {
                    int cx = 0, cy = 0;
                    resolve_ground_cell(ev.x, ev.y, cx, cy);
                    actions_.sw_fire_confirmed = true;
                    actions_.sw_cx = cx;
                    actions_.sw_cy = cy;
                    sw_targeting_ = false;   // one-shot
                }
                l_down_ = false;
                dragging_ = false;
                break;
            default:
                break;
        }
        return;
    }

    // Placement mode (Task 7a) intercepts the whole gesture model: a left
    // Down+Up (no drag) confirms at the hovered foundation origin; a right
    // click cancels; PointerMove just tracks the cursor (so placement_cell
    // follows it); everything else is swallowed so no selection/attack/move
    // gesture leaks through while placing. Quit still propagates.
    if (placement_active()) {
        switch (ev.kind) {
            case InputEvent::Quit:
                quit_ = true;
                break;
            case InputEvent::PointerMove:
                cur_x_ = ev.x;
                cur_y_ = ev.y;
                break;
            case InputEvent::PointerDown:
                if (ev.button == InputEvent::Right) {
                    actions_.place_cancelled = true;
                    cancel_placement();
                } else {
                    l_down_ = true;
                    dragging_ = false;
                    down_x_ = cur_x_ = ev.x;
                    down_y_ = cur_y_ = ev.y;
                }
                break;
            case InputEvent::PointerUp:
                if (ev.button == InputEvent::Left && l_down_ && !dragging_) {
                    int cx = 0, cy = 0;
                    resolve_ground_cell(ev.x, ev.y, cx, cy);
                    actions_.place_confirmed = true;
                    actions_.place_cx = cx - place_fw_ / 2;
                    actions_.place_cy = cy - place_fh_ / 2;
                }
                l_down_ = false;
                dragging_ = false;
                break;
            default:
                break;
        }
        return;
    }

    switch (ev.kind) {
        case InputEvent::Quit:
            quit_ = true;
            break;

        case InputEvent::PointerDown:
            if (ev.button == InputEvent::Right) {
                // Rally-point set [RE: production.md §1.4]: with an own unit
                // factory selected, a right-click sets its rally cell and
                // consumes the event (no attack/move). Resolved BEFORE the
                // harvester/attack path so a selected factory always takes
                // precedence. A factory that died since selection must NOT keep
                // swallowing right-clicks: if the selection is stale (out of
                // range or dead/tombstoned), clear it and fall through so the
                // click resumes normal attack/move behavior.
                if (selected_building_ >= 0) {
                    const auto& blds = world_.buildings();
                    if (selected_building_ >= static_cast<int>(blds.size()) ||
                        !blds[selected_building_].alive) {
                        selected_building_ = -1;   // stale selection: don't consume
                    } else {
                        int rcx = 0, rcy = 0;
                        resolve_ground_cell(ev.x, ev.y, rcx, rcy);
                        world_.set_rally(selected_building_, rcx, rcy);
                        break;
                    }
                }
                // Right-click: attack a hostile under the cursor if the pick
                // lands on one (house != 0) -- a right-click on a FRIENDLY
                // unit is NOT an attack order and falls through to the
                // ground-move branch below (RA2 behavior: there is no
                // "attack own unit" order, so it's treated like clicking the
                // ground under that unit).
                const int idx = unit_under(ev.x, ev.y);
                // Attack requires BOTH a hostile under the cursor AND a live
                // selection (spec gate); otherwise fall through to a ground move
                // so an unselected right-click behaves identically everywhere.
                bool any_selected = false;
                for (const Unit& su : world_.units())
                    if (su.selected && su.alive && su.die_time < 0.0) { any_selected = true; break; }
                if (idx >= 0 && world_.units()[idx].house != 0 && any_selected) {
                    // Capture actor BEFORE the order mutates any state.
                    const int actor = lowest_selected();
                    world_.order_attack(idx);
                    actions_.attack_ordered = true;
                    actions_.actor = actor;
                } else {
                    // Group-move to the clicked ground cell (issued on down).
                    // The half-tile offset before screen_to_cell maps the
                    // click to the cell whose visible diamond was clicked --
                    // LEVEL-AWARE, two-pass: raised terrain projects its
                    // diamond kLevelPx*level higher on screen than the
                    // level-0 projection (cell_to_screen subtracts
                    // level*kLevelPx from py), so a single level-blind
                    // inversion resolves 1-2 cells short of the tile actually
                    // drawn under the cursor. Pass 1 inverts at level 0 to get
                    // a first-guess cell; looking up THAT cell's real level
                    // and re-inverting with the raise added back to py
                    // converges on the true cell on smooth terrain (one
                    // refinement step, not a fixed-point loop -- see
                    // task-6-report.md).
                    int cx = 0, cy = 0;
                    resolve_ground_cell(ev.x, ev.y, cx, cy);
                    // Harvester orders [RE: economy.md §1], resolved BEFORE the
                    // attack-building branch: with a house-0 harvester in the
                    // selection, a right-click on an ore cell issues a harvest
                    // order and one on an own alive REFINERY (is_refinery_type
                    // -- the roster list set_refinery_types installed; any
                    // other own building stays a plain ground move) sends the
                    // miner(s) home. order_harvest/order_return only touch
                    // selected house-0 harvesters, so non-harvester
                    // right-clicks fall straight through to attack/move below.
                    bool harv_selected = false;
                    for (const Unit& su : world_.units())
                        if (su.selected && su.is_harvester && su.house == 0 && su.alive &&
                            su.die_time < 0.0) {
                            harv_selected = true;
                            break;
                        }
                    const int here_b = world_.building_at_cell(cx, cy);
                    if (harv_selected && world_.ore().has_ore(cx, cy)) {
                        world_.order_harvest(cx, cy);
                        // MIXED selection (fix round): co-selected NON-harvester
                        // units are not harvest-capable -- they get the normal
                        // group move to the same cell (each unit interprets the
                        // click per its own logic). The harvesters are dropped
                        // from the selection for the move call so
                        // order_move_selected can't clobber the harvest mission
                        // just issued (it would set manual_hold + repath), then
                        // restored -- the player's selection is untouched.
                        std::vector<std::size_t> harv_idx;
                        bool any_other = false;
                        for (std::size_t i = 0; i < world_.units().size(); ++i) {
                            const Unit& su = world_.units()[i];
                            // CB-1: also skip CARRIED units (cargo_of >= 0, aboard a
                            // transport / off-map) -- a widening of the corpse/
                            // validity skip, the fourth carry-guard site.
                            if (!su.selected || !su.alive || su.die_time >= 0.0 ||
                                su.cargo_of >= 0)
                                continue;
                            if (su.is_harvester && su.house == 0)
                                harv_idx.push_back(i);
                            else
                                any_other = true;
                        }
                        if (any_other) {
                            const int actor = lowest_selected();
                            for (std::size_t i : harv_idx) world_.units()[i].selected = false;
                            world_.order_move_selected(cx, cy);
                            for (std::size_t i : harv_idx) world_.units()[i].selected = true;
                            if (actor >= 0) {
                                actions_.move_ordered = true;
                                actions_.actor = actor;
                            }
                        }
                        break;  // handled: no attack for this right-click
                    }
                    if (harv_selected && here_b >= 0 && world_.buildings()[here_b].house == 0 &&
                        world_.buildings()[here_b].alive &&
                        is_refinery_type(world_.buildings()[here_b].type)) {
                        world_.order_return();
                        break;  // handled
                    }
                    // CB-1 enter-transport gesture [RE: transport-cargo.md
                    // §2/§3; spec §3]: a right-click on an OWN, alive,
                    // non-dying TRANSPORT (passengers_max > 0 -- the Unit
                    // carries the roster knowledge, the same way the
                    // refinery-return gesture reads roster types) with a
                    // selection containing at least one eligible
                    // NON-transport unit issues the enter order instead of a
                    // group move. Resolved AFTER the harvester branches (ore/
                    // refinery win for miners) and BEFORE attack-building/
                    // move. The original's "enter" cursor is cosmetic; ours
                    // stays default (noted divergence, no new UI systems).
                    if (idx >= 0 && world_.units()[idx].house == 0 &&
                        world_.units()[idx].passengers_max > 0) {
                        bool eligible = false;
                        for (std::size_t si = 0; si < world_.units().size(); ++si) {
                            const Unit& su = world_.units()[si];
                            if (!su.selected || su.house != 0 || !su.alive ||
                                su.die_time >= 0.0 || static_cast<int>(si) == idx)
                                continue;
                            if (su.passengers_max > 0) continue;  // transports
                            eligible = true;                      // do not ride
                            break;
                        }
                        if (eligible) {
                            const int actor = lowest_selected();
                            world_.order_enter_transport(idx);
                            actions_.move_ordered = true;   // move-ack voice
                            actions_.actor = actor;
                            break;  // handled: no attack/move for this click
                        }
                    }
                    // Interactive attack-building (Task 4 machinery wired to the
                    // player here): if the resolved ground cell lands on a
                    // HOSTILE building's foundation it's an attack order, not a
                    // move -- mirrors the hostile-unit gate above (house != 0 +
                    // a live selection). building_at_cell returns only alive,
                    // non-Dead foundations and order_attack_building re-guards
                    // house-0/dead/out-of-range internally (world.cpp:464), so
                    // the lone house != 0 test here is just the input-level
                    // friendly-vs-hostile decision: a click on an own/friendly
                    // building is NOT an attack and falls through to the move
                    // below, exactly like a friendly-unit right-click.
                    const int b_idx = world_.building_at_cell(cx, cy);
                    if (b_idx >= 0 && world_.buildings()[b_idx].house != 0 && any_selected) {
                        // Capture actor BEFORE the order mutates any state.
                        const int actor = lowest_selected();
                        world_.order_attack_building(b_idx);
                        actions_.attack_ordered = true;
                        actions_.actor = actor;
                    } else {
                        // Group-move to the clicked ground cell (issued on
                        // down). Capture actor BEFORE the order mutates state;
                        // the move flag is set only when the selection is
                        // non-empty (an empty-selection right-click is a no-op
                        // move).
                        const int actor = lowest_selected();
                        world_.order_move_selected(cx, cy);
                        if (actor >= 0) {
                            actions_.move_ordered = true;
                            actions_.actor = actor;
                        }
                    }
                }
            } else {
                l_down_ = true;
                dragging_ = false;
                down_shift_ = ev.shift;
                down_x_ = cur_x_ = ev.x;
                down_y_ = cur_y_ = ev.y;
            }
            break;

        case InputEvent::PointerMove:
            cur_x_ = ev.x;
            cur_y_ = ev.y;
            if (l_down_ && !dragging_ &&
                (std::abs(ev.x - down_x_) > kDragThreshold ||
                 std::abs(ev.y - down_y_) > kDragThreshold)) {
                dragging_ = true;
            }
            break;

        case InputEvent::PointerUp:
            if (ev.button == InputEvent::Right) break;
            if (l_down_) {
                if (dragging_) {
                    commit_box(down_shift_);
                } else if (commit_factory_click(ev.x, ev.y)) {
                    // Handled as a factory select/primary [RE: production.md
                    // §1.2] -- takes precedence over unit selection AND the
                    // double-click type-select; reset the double-click timer so
                    // a following click on a unit isn't spuriously a "double".
                    last_click_t_ = 0;
                } else {
                    const bool is_double =
                        (ev.t_ms - last_click_t_) <= kDblMs &&
                        std::abs(ev.x - last_click_x_) <= kDblSlop &&
                        std::abs(ev.y - last_click_y_) <= kDblSlop;
                    if (is_double) {
                        commit_double(ev.x, ev.y);
                        last_click_t_ = 0;  // consume, so a triple-click isn't a 2nd double
                    } else {
                        commit_click(ev.x, ev.y, down_shift_);
                        last_click_t_ = ev.t_ms;
                        last_click_x_ = ev.x;
                        last_click_y_ = ev.y;
                    }
                }
            }
            l_down_ = false;
            dragging_ = false;
            break;

        default:
            break;
    }
}

}  // namespace ra2engine
