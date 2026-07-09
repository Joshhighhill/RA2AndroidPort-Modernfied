#pragma once

#include "iso_renderer.hpp"  // Camera
#include "platform.hpp"      // InputEvent
#include "world.hpp"         // World / Unit

namespace ra2engine {

// Translates pointer InputEvents (Task 0 Platform) into actions on a World +
// Camera. PURE engine logic -- no SDL (only the SDL-free InputEvent struct,
// screen_to_cell, and World) -- so it is unit-testable without a window.
//
// Interaction model (desktop-playable milestone):
//   * A quick TAP (pointer down then up with little movement) is a command:
//       - on the unit's cell  -> SELECT the unit
//       - elsewhere while the unit is selected -> issue a MOVE order to the
//         tapped iso cell (A* path stored on the unit)
//   * A PRESS-DRAG (pointer moved past a small threshold while held) PANS the
//     camera by the drag delta -- so dragging never fires a spurious
//     select/move, and a tap never pans. This click-vs-drag split on release
//     is what makes both gestures feel right with a single pointer.
//
// Coordinate reconciliation (the subtle part -- see README of this task):
// the pointer position is in window pixels; the camera is a pixel pan added
// to every projected tile. So the iso cell under the pointer is
//   screen_to_cell(px - cam.x, py - cam.y, tile_w, tile_h) -> (cx, cy)
// i.e. UNDO the camera pan first, then invert the projection. That (cx, cy)
// is in the exact same raw iso space as MapCell.x/y, the unit's cell_x/cell_y,
// and the World passability grid -- so it can be compared to the unit cell
// for hit-testing and fed straight to World::order_move / find_path.
class Input {
public:
    // view_w/view_h: the viewport size in window pixels (Platform::width()/
    // height() in the live loop). Used ONLY to clamp camera drag (see
    // clamp_camera() below, Task 12 review Finding 2) so the map can't be
    // dragged arbitrarily far off-screen; they don't otherwise affect
    // select/move hit-testing.
    Input(World& world, Camera& camera, int view_w, int view_h);

    // Feed one polled event. Mutates world_/cam_ per the model above.
    void handle(const InputEvent& ev);

    bool wants_quit() const { return quit_; }

private:
    void on_tap(int px, int py);  // resolve a tap into select / move
    void clamp_camera();          // keeps cam_ within the drag bounds below

    World& world_;
    Camera& cam_;
    bool quit_ = false;
    bool pressed_ = false;    // a pointer button is currently down
    bool dragging_ = false;   // this press has crossed the drag threshold
    int down_x_ = 0, down_y_ = 0;  // where the press started (click vs drag)
    int last_x_ = 0, last_y_ = 0;  // previous pointer pos (drag delta)

    // --- Camera drag clamp (Task 12 review Finding 2) ---
    // Computed once at construction from the map's screen-space bounding box
    // (terrain_screen_bounds, world.map()) plus the viewport size: cam_.x/y
    // is kept within [min_cam_x_, max_cam_x_] / [min_cam_y_, max_cam_y_] so a
    // drag can never push the map's ENTIRE screen bounding box more than
    // `margin` (half a viewport, view_w_/2 x view_h_/2) past the viewport's
    // edges -- i.e. some part of the map is always at most a half-viewport
    // away from being on-screen; it can never be dragged arbitrarily far
    // off. See input.cpp for the exact formula. Left wide-open
    // (INT_MIN/INT_MAX, i.e. unclamped) if the map has no cells (nothing to
    // bound against -- e.g. an empty synthetic test map).
    int view_w_ = 0, view_h_ = 0;
    int min_cam_x_ = 0, max_cam_x_ = 0, min_cam_y_ = 0, max_cam_y_ = 0;
};

}  // namespace ra2engine
