#include "input.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>

#include "iso_project.hpp"  // screen_to_cell

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
    const std::vector<Unit>& us = world_.units();
    int best = -1;
    long best_d2 = 0;
    for (std::size_t i = 0; i < us.size(); ++i) {
        int gx, gy;
        unit_ground_px(us[i], gx, gy);
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

bool Input::selection_box(int& x0, int& y0, int& x1, int& y1) const {
    if (!(l_down_ && dragging_)) return false;
    x0 = std::min(down_x_, cur_x_);
    x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_);
    y1 = std::max(down_y_, cur_y_);
    return true;
}

void Input::commit_click(int px, int py, bool shift) {
    const int idx = unit_under(px, py);
    if (idx >= 0) {
        if (shift) {
            world_.units()[idx].selected = !world_.units()[idx].selected;  // toggle
        } else {
            world_.clear_selection();
            world_.units()[idx].selected = true;
        }
    } else if (!shift) {
        world_.clear_selection();  // click on empty ground clears (Shift+empty = no-op)
    }
}

void Input::commit_box(bool shift) {
    int x0, y0, x1, y1;
    x0 = std::min(down_x_, cur_x_); x1 = std::max(down_x_, cur_x_);
    y0 = std::min(down_y_, cur_y_); y1 = std::max(down_y_, cur_y_);
    if (!shift) world_.clear_selection();  // plain box replaces; Shift+box adds
    for (Unit& u : world_.units()) {
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= x0 && gx <= x1 && gy >= y0 && gy <= y1) u.selected = true;
    }
}

void Input::commit_double(int px, int py) {
    const int idx = unit_under(px, py);
    if (idx < 0) return;
    const std::string type = world_.units()[idx].image;
    world_.clear_selection();
    for (Unit& u : world_.units()) {
        if (u.image != type) continue;
        int gx, gy;
        unit_ground_px(u, gx, gy);
        if (gx >= 0 && gx < view_w_ && gy >= 0 && gy < view_h_) u.selected = true;  // on-screen
    }
}

void Input::handle(const InputEvent& ev) {
    switch (ev.kind) {
        case InputEvent::Quit:
            quit_ = true;
            break;

        case InputEvent::PointerDown:
            if (ev.button == InputEvent::Right) {
                // Right-click = group-move the current selection (issued on
                // down). The half-tile offset before screen_to_cell maps the
                // click to the cell whose visible diamond was clicked.
                int cx = 0, cy = 0;
                screen_to_cell(ev.x - cam_.x - kTileW / 2, ev.y - cam_.y - kTileH / 2,
                               kTileW, kTileH, cx, cy);
                world_.order_move_selected(cx, cy);
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
