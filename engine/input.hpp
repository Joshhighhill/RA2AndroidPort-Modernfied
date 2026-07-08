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
    Input(World& world, Camera& camera) : world_(world), cam_(camera) {}

    // Feed one polled event. Mutates world_/cam_ per the model above.
    void handle(const InputEvent& ev);

    bool wants_quit() const { return quit_; }

private:
    void on_tap(int px, int py);  // resolve a tap into select / move

    World& world_;
    Camera& cam_;
    bool quit_ = false;
    bool pressed_ = false;    // a pointer button is currently down
    bool dragging_ = false;   // this press has crossed the drag threshold
    int down_x_ = 0, down_y_ = 0;  // where the press started (click vs drag)
    int last_x_ = 0, last_y_ = 0;  // previous pointer pos (drag delta)
};

}  // namespace ra2engine
