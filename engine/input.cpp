#include "input.hpp"

#include <climits>  // INT_MIN/INT_MAX
#include <cstdlib>  // std::abs

#include "iso_project.hpp"  // screen_to_cell

namespace ra2engine {

// RA2 temperate diamond tile geometry -- must match iso_renderer.cpp's
// kTileW/kTileH (the projection the terrain + unit are drawn with) so a tap
// inverts to the same cell the sprite sits on.
static constexpr int kTileW = 60;
static constexpr int kTileH = 30;

// Pointer travel (window px) past which a press is treated as a camera drag
// rather than a tap. Small enough that deliberate clicks never pan, large
// enough to absorb hand jitter / touch wobble.
static constexpr int kDragThreshold = 6;

Input::Input(World& world, Camera& camera, int view_w, int view_h)
    : world_(world), cam_(camera), view_w_(view_w), view_h_(view_h) {
    int mnx, mny, mxx, mxy;
    if (terrain_screen_bounds(world_.map(), mnx, mny, mxx, mxy)) {
        // Clamp rule (Task 12 review Finding 2): keep the map's screen
        // bounding box [mnx,mxx]x[mny,mxy] within `margin` (half a
        // viewport) of the viewport [0,view_w_]x[0,view_h_] on every side.
        // Solving "camera-shifted box overlaps [-margin, view+margin]" for
        // cam_.x gives this range (symmetric for y); a drag can push at
        // most a half-viewport of empty space past the map's edge before it
        // stops.
        const int margin_x = view_w_ / 2;
        const int margin_y = view_h_ / 2;
        min_cam_x_ = -margin_x - mxx;
        max_cam_x_ = view_w_ + margin_x - mnx;
        min_cam_y_ = -margin_y - mxy;
        max_cam_y_ = view_h_ + margin_y - mny;
    } else {
        // No cells (e.g. an empty synthetic test map): nothing to bound
        // against, leave the camera unclamped.
        min_cam_x_ = INT_MIN;
        max_cam_x_ = INT_MAX;
        min_cam_y_ = INT_MIN;
        max_cam_y_ = INT_MAX;
    }
}

void Input::clamp_camera() {
    if (cam_.x < min_cam_x_) cam_.x = min_cam_x_;
    else if (cam_.x > max_cam_x_) cam_.x = max_cam_x_;
    if (cam_.y < min_cam_y_) cam_.y = min_cam_y_;
    else if (cam_.y > max_cam_y_) cam_.y = max_cam_y_;
}

void Input::handle(const InputEvent& ev) {
    switch (ev.kind) {
        case InputEvent::Quit:
            quit_ = true;
            break;

        case InputEvent::PointerDown:
            pressed_ = true;
            dragging_ = false;
            down_x_ = last_x_ = ev.x;
            down_y_ = last_y_ = ev.y;
            break;

        case InputEvent::PointerMove:
            if (pressed_) {
                if (!dragging_ &&
                    (std::abs(ev.x - down_x_) > kDragThreshold ||
                     std::abs(ev.y - down_y_) > kDragThreshold)) {
                    dragging_ = true;
                }
                if (dragging_) {
                    // Pan the camera by the incremental drag delta (larger
                    // cam.x/y scrolls the map right/down -- see Camera).
                    cam_.x += ev.x - last_x_;
                    cam_.y += ev.y - last_y_;
                    clamp_camera();  // Finding 2: bound the drag to the map
                }
            }
            last_x_ = ev.x;
            last_y_ = ev.y;
            break;

        case InputEvent::PointerUp:
            if (pressed_ && !dragging_) on_tap(ev.x, ev.y);
            pressed_ = false;
            dragging_ = false;
            break;

        default:
            break;
    }
}

void Input::on_tap(int px, int py) {
    // Undo the camera pan, then invert the projection to the iso cell (same
    // space as MapCell / unit cell / passability grid).
    int cx = 0, cy = 0;
    screen_to_cell(px - cam_.x, py - cam_.y, kTileW, kTileH, cx, cy);

    Unit& u = world_.unit();
    if (cx == u.cell_x && cy == u.cell_y) {
        u.selected = true;  // tapped the unit -> select it
        return;
    }
    if (u.selected) {
        world_.order_move(cx, cy);  // tapped elsewhere while selected -> move
    }
}

}  // namespace ra2engine
