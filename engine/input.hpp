#pragma once

#include <cstdint>

#include "iso_renderer.hpp"  // Camera
#include "platform.hpp"      // InputEvent
#include "world.hpp"         // World / Unit

namespace ra2engine {

// Translates pointer InputEvents (Task 0 Platform) into actions on a World +
// Camera. PURE engine logic -- no SDL (only the SDL-free InputEvent struct,
// screen_to_cell, and World) -- so it is unit-testable without a window.
//
// RA2-classic gesture model (multi-unit selection, Task 5; combat, Task 6):
//   * Left-click on a unit selects only it; on empty ground clears selection.
//     A hostile (house != 0) is NEVER selectable -- a left-click landing on
//     one is treated like a click on empty ground.
//   * Left-drag (past a small threshold) draws a selection box; releasing it
//     box-selects every house-0, alive unit whose ground point falls inside
//     (hostiles and corpses are skipped even if boxed).
//   * Shift+left-click toggles a unit in/out of the selection; Shift+left-drag
//     adds the boxed units to the existing selection.
//   * A double-click (two quick left-clicks near the same spot) selects every
//     on-screen house-0 unit of the same image/type as the one clicked; a
//     double-click landing on a hostile does nothing.
//   * Right-click on a hostile (house != 0) issues an attack order
//     (World::order_attack) to the selection; on anything else (empty ground,
//     or a friendly unit) it group-moves the selection (order_move_selected)
//     to the clicked cell, resolved LEVEL-AWARE (see handle()'s two-pass
//     inversion) so raised terrain is targeted at the tile actually drawn
//     under the cursor.
//   * Dead/dying units are unpickable everywhere (unit_under skips them), so
//     neither selection nor the right-click attack path can ever target a
//     corpse.
//   * Camera panning is no longer driven by a left-drag -- main.cpp calls
//     Input::pan() directly (arrow keys / screen-edge scroll).
class Input {
public:
    Input(World& world, Camera& camera, int view_w, int view_h);

    // Feed one polled event (RA2-classic model, see input.cpp).
    void handle(const InputEvent& ev);

    // Pan the camera by (dx, dy) window pixels, clamped to the map bounds.
    // Called by main.cpp for arrow-key + screen-edge scrolling (left-drag no
    // longer pans -- it draws the selection box).
    void pan(int dx, int dy);

    // If a left-drag selection box is active, fill its normalized rectangle
    // (x0<=x1, y0<=y1) in window pixels and return true; else return false.
    bool selection_box(int& x0, int& y0, int& x1, int& y1) const;

    bool wants_quit() const { return quit_; }

private:
    void clamp_camera();
    // Screen pixel -> unit index or -1. Skips dead/dying units (corpses are
    // unpickable everywhere this is used, including the right-click attack
    // path) but still returns hostiles -- attack targeting needs them; it is
    // the CALLERS (commit_click/commit_box/commit_double) that additionally
    // restrict selection to house == 0.
    int unit_under(int px, int py) const;
    void unit_ground_px(const Unit& u, int& gx, int& gy) const;  // ground point in window px
    void commit_click(int px, int py, bool shift);  // house-0-only selection (see .cpp)
    void commit_box(bool shift);                    // house-0-only, corpse-skipping box select
    void commit_double(int px, int py);             // house-0-only type-select

    World& world_;
    Camera& cam_;
    bool quit_ = false;

    // Left-button gesture state.
    bool l_down_ = false;
    bool dragging_ = false;
    bool down_shift_ = false;
    int down_x_ = 0, down_y_ = 0, cur_x_ = 0, cur_y_ = 0;

    // Double-click detection (uses event t_ms; no clock read here).
    uint32_t last_click_t_ = 0;
    int last_click_x_ = 0, last_click_y_ = 0;

    // Camera clamp bounds (computed once from the map's screen bbox + viewport;
    // same rule as the previous drag clamp -- see input.cpp).
    int view_w_ = 0, view_h_ = 0;
    int min_cam_x_ = 0, max_cam_x_ = 0, min_cam_y_ = 0, max_cam_y_ = 0;
};

}  // namespace ra2engine
