#include "input.hpp"

#include <climits>  // INT_MIN/INT_MAX
#include <cmath>    // std::lround
#include <cstdlib>  // std::abs

#include "iso_project.hpp"  // screen_to_cell

namespace ra2engine {

// RA2 temperate diamond tile geometry -- must match iso_renderer.cpp's
// kTileW/kTileH (the projection the terrain + unit are drawn with) so a tap
// inverts to the same cell the sprite sits on.
static constexpr int kTileW = 60;
static constexpr int kTileH = 30;

// Terrain height step in px -- must match iso_renderer.cpp's kLevelPx. The
// unit sprite is drawn RAISED by level*kLevelPx, so a selection hit-test that
// ignored height would miss any unit standing on elevated ground (the visible
// sprite sits above the flat-tile position the tap would otherwise invert to).
static constexpr int kLevelPx = 15;

// Selection hit box around the unit's on-screen ground point (its "feet").
// A single tile is a tiny target -- especially for touch -- so accept a tap
// anywhere over the sprite's footprint and body: about half a tile to each
// side, most of a sprite-height up (the body rises above the feet), a little
// below. Generous on purpose; there is only one unit to disambiguate.
static constexpr int kHitHalfW = 32;
static constexpr int kHitUp = 46;
static constexpr int kHitDown = 16;

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
    Unit& u = world_.unit();

    // Hit-test in SCREEN space against where the unit's sprite is actually
    // drawn -- its ground point (feet) in window pixels. This is the same
    // anchor draw_unit uses in iso_renderer.cpp: project the unit's continuous
    // position, add the tile center, raise by the terrain height, apply the
    // camera. Testing here (rather than inverting the tap to a cell and
    // comparing) both (a) accounts for the height raise, so taps on elevated
    // units land, and (b) lets the hit box be as large as a comfortable tap
    // target instead of a single 60x30 tile.
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    const int gx = static_cast<int>(std::lround((u.fx - u.fy) * hw)) + kTileW / 2 + cam_.x;
    const int gy = static_cast<int>(std::lround((u.fx + u.fy) * hh - u.level * kLevelPx)) +
                   kTileH / 2 + cam_.y;

    if (px >= gx - kHitHalfW && px <= gx + kHitHalfW && py >= gy - kHitUp && py <= gy + kHitDown) {
        u.selected = true;  // tapped on/near the unit -> select it
        return;
    }
    if (u.selected) {
        // Tapped away from the unit while selected -> move there. The
        // destination is taken on the ground plane (screen_to_cell ignores
        // height, which is ambiguous from a pixel alone).
        int cx = 0, cy = 0;
        screen_to_cell(px - cam_.x, py - cam_.y, kTileW, kTileH, cx, cy);
        world_.order_move(cx, cy);
    }
}

}  // namespace ra2engine
