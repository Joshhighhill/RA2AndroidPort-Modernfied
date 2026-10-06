#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "iso_renderer.hpp"  // Camera
#include "platform.hpp"      // InputEvent
#include "world.hpp"         // World / Unit

namespace ra2engine {

// Reports what handle() did since the last take_actions() call, for audio
// voice-ack hooks (Task 5 of the audio-playback phase) -- pure observation,
// no behavior change to selection/orders. ACCUMULATE-THEN-CONSUME: handle()
// never resets this; each commit/order site ORs its flag in (actor = lowest
// selected unit index, captured before the order mutates state), and
// Input::take_actions() is the single consume point -- it returns the
// accumulated struct and clears it. WHY: the game loop feeds a whole frame's
// event batch (a click's Down+Up typically arrive together) and then reads
// the actions ONCE per frame -- accumulation means the Down-recorded action
// survives the batch's trailing events without special-casing any of them,
// and consuming means an idle frame (or a frame of plain mouse moves) reads
// back all-false instead of re-triggering the previous frame's ack forever.
// Within one consume window the flags are independent bits: a select-click
// and a right-click order in the same frame both report (Task 6's ack
// precedence -- selected, else attack, else move -- picks one voice line).
struct InputActions {
    bool selected = false;       // a commit produced a non-empty selection
    bool move_ordered = false;   // right-click group move issued
    bool attack_ordered = false; // right-click attack issued
    int actor = -1;              // representative unit index (lowest selected)

    // --- Buildings phase: placement mode (Task 7a) ---
    bool place_confirmed = false;   // left-click confirmed a placement this batch
    bool place_cancelled = false;   // right-click/Esc cancelled placement this batch
    int place_cx = 0, place_cy = 0; // confirmed foundation ORIGIN cell

    // CB-4 Superweapons [spec §4]: a left-click committed a SW target cell while
    // SW-targeting was armed (mirrors place_confirmed/place_cx/cy). main.cpp
    // routes this to World::fire_superweapon. Default inert (no existing flow
    // arms SW-targeting, so no existing demo/frame reports it).
    bool sw_fire_confirmed = false;
    int sw_cx = 0, sw_cy = 0;

    // Repair & Sell [RE: repair-sell.md §C1]: a left-click in Repair/Sell mode
    // that landed on an OWN (house 0) alive building. main.cpp routes these to
    // World::toggle_repair / World::begin_sell. Default inert -- no existing
    // flow arms either mode, so no existing demo/frame reports them.
    bool repair_clicked = false;
    int repair_building = -1;
    bool sell_clicked = false;
    int sell_building = -1;
};

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

    // --- Buildings phase: placement mode (Task 7a) -------------------------
    // While active, a left Down+Up (no drag) confirms placement at the hovered
    // foundation ORIGIN cell (reported via InputActions::place_confirmed +
    // place_cx/cy); a right click OR cancel_placement() (main.cpp routes the
    // Escape key here) cancels back to READY. Selection/attack/move gestures
    // are suppressed while placing. main.cpp routes sidebar-rect clicks BEFORE
    // Input::handle (the sidebar consumes them), so Input never needs to know
    // the sidebar exists.
    void begin_placement(int roster_type, int fw, int fh);
    void cancel_placement();
    bool placement_active() const { return place_type_ >= 0; }

    // CB-4: arm/disarm SW-targeting -- a lightweight parallel of placement mode.
    // While armed, a left-click on the map reports sw_fire_confirmed + sw_cx/cy
    // (via the SAME level-aware screen->cell pick placement uses); a right-click/
    // Esc cancels. UI state only -- the sim is untouched until main.cpp calls
    // World::fire_superweapon. No existing flow arms this, so this branch is
    // never entered in any existing demo.
    void arm_superweapon() { sw_targeting_ = true; }
    void cancel_superweapon() { sw_targeting_ = false; }
    bool sw_targeting() const { return sw_targeting_; }
    int placement_type() const { return place_type_; }

    // --- Repair & Sell cursor modes [RE: repair-sell.md §C1] --------------
    // The original's DisplayClass carries FIVE mutually-exclusive one-byte mode
    // flags (RepairMode +0x11B0, SellMode +0x11B1, PowerToggleMode +0x11B2,
    // PlanningMode +0x11B3, PlaceBeaconMode +0x11B4) and EVERY setter clears
    // the other four. The last three have no feature behind them in this engine
    // (no per-building power toggle, no waypoint planning, no beacons), so they
    // are recorded but not modeled: the exclusion set here is
    // {pending placement, sw_targeting, Repair, Sell}.
    enum class BaseMode { None, Repair, Sell };

    // SetRepairMode / SetSellMode (FUN_004AC7D0 / FUN_004AC570), transcribed:
    //   1. control -1 toggle / 0 off / 1 on; no-op if the value is unchanged;
    //   2. REFUSED outright while a placement is pending (placement WINS);
    //   3. clears every other mode (here: the sibling mode + sw_targeting);
    //   4. arming requires the player to own at least one building
    //      (PlayerPtr->[0x2F0] > 0, §D5 CONFIRMED), and calls Reset_Cursor
    //      (FUN_0048DBA0) -- which DESELECTS EVERYTHING.
    // The mode is NOT one-shot: it stays armed until cancelled, so several
    // buildings can be repaired/sold in a row.
    void set_repair_mode(int control);
    void set_sell_mode(int control);
    BaseMode base_mode() const { return mode_; }

    // The ESC / right-click chain (FUN_004AAC40): cancels EXACTLY ONE thing per
    // press, in the recovered priority order
    //   pending placement -> RepairMode -> SellMode -> PowerToggleMode
    //   -> pending superweapon -> PlanningMode -> PlaceBeaconMode -> (nothing).
    // The three unmodeled modes are simply absent from the chain. Returns true
    // iff something was cancelled.
    bool cancel_one_mode();

    // HouseClass+0x2F0 = the player's owned-building count [RE: repair-sell.md
    // §D5, CONFIRMED]: arming either mode is refused when it is zero. main.cpp
    // refreshes it once per frame (alive, non-Dead house-0 buildings -- a
    // building mid-sell still counts, §D5 residual).
    void set_owned_buildings(int n) { owned_buildings_ = n; }
    int owned_buildings() const { return owned_buildings_; }

    // The alive, non-Dead building under window pixel (px, py) (level-aware
    // ground pick), or -1. Used by main.cpp's mode cursor.
    int building_under(int px, int py) const;
    // Hovered foundation ORIGIN cell for the CURRENT pointer position:
    // resolve_ground_cell(cur) minus half the foundation, so the cursor sits
    // near the foundation's center. Meaningful only while placement_active().
    void placement_cell(int& cx, int& cy) const;

    // Pan the camera by (dx, dy) window pixels, clamped to the map bounds.
    // Called by main.cpp for arrow-key + screen-edge scrolling (left-drag no
    // longer pans -- it draws the selection box).
    void pan(int dx, int dy);

    // If a left-drag selection box is active, fill its normalized rectangle
    // (x0<=x1, y0<=y1) in window pixels and return true; else return false.
    bool selection_box(int& x0, int& y0, int& x1, int& y1) const;

    // Economy phase: the roster type indices that are refineries
    // (GAREFN/NAREFN) -- the SAME list main.cpp's wire_economy hands to
    // World::set_economy. Gates the right-click "return to refinery" order:
    // with a harvester selected, a right-click on an own alive building
    // issues order_return ONLY when that building's type is in this list; a
    // click on any other own building stays a plain ground move. Empty
    // (never called) = no building ever triggers a return order.
    void set_refinery_types(std::vector<int> types) { refinery_types_ = std::move(types); }

    // Production phase [RE: production.md §1.2/§1.4]: the roster-index lists of
    // the player's UNIT factories, used by the factory-select / rally / primary
    // gestures. `unit_factory_types_` is the UNION (any left-click on an own
    // alive building of one of these types selects it for rally/primary); the
    // per-category lists (infantry = {GAPILE}, vehicle = {GAWEAP}) let the
    // primary-set trigger resolve the SELECTED factory's category so
    // set_primary clears the right sibling group. All own-house 0 only.
    void set_unit_factory_types(std::vector<int> t) { unit_factory_types_ = std::move(t); }
    void set_infantry_factory_types(std::vector<int> t) { inf_factory_types_ = std::move(t); }
    void set_vehicle_factory_types(std::vector<int> t) { veh_factory_types_ = std::move(t); }

    // Engineer/Spy Capture phase [RE: capture.md §B]: the per-roster-index
    // infiltration purpose map (main.cpp resolves it from building ids:
    // GAREFN/NAREFN->Refinery, GAPOWR/NAPOWR->PowerPlant, a SW building->
    // Superweapon, GAPILE/NAHAND/GAWEAP/NAWEAP->Factory, radar/lab->RadarLab,
    // else None) plus the two rules constants the spy gesture passes through
    // (SpyMoneyStealPercent=0.5, SpyPowerBlackout=1000). Empty/unset (default)
    // -> every building resolves to None and the spy gesture no-ops. Interactive-
    // only: no demo selects a spy, so no gate exercises any of this.
    void set_infiltrate_params(std::vector<World::InfiltratePurpose> purposes,
                               double steal_pct, int blackout_frames) {
        infiltrate_purposes_ = std::move(purposes);
        spy_steal_pct_ = steal_pct;
        spy_blackout_frames_ = blackout_frames;
    }

    // The own (house 0) factory currently selected for rally/primary, or -1.
    // Consumed by main.cpp's rally-marker chrome.
    int selected_building() const { return selected_building_; }

    bool wants_quit() const { return quit_; }

    // Consume the actions accumulated by every handle() call since the
    // previous take_actions(): returns them and resets the accumulator (see
    // InputActions above for the accumulate-then-consume rationale). Call
    // ONCE per frame, after the frame's event-poll loop.
    InputActions take_actions() {
        InputActions out = actions_;
        actions_ = InputActions{};
        return out;
    }

private:
    void clamp_camera();
    // Lowest index i with units()[i].selected == true, or -1 if none.
    int lowest_selected() const;
    // Screen pixel -> unit index or -1. Skips dead/dying units (corpses are
    // unpickable everywhere this is used, including the right-click attack
    // path) but still returns hostiles -- attack targeting needs them; it is
    // the CALLERS (commit_click/commit_box/commit_double) that additionally
    // restrict selection to house == 0.
    int unit_under(int px, int py) const;
    void unit_ground_px(const Unit& u, int& gx, int& gy) const;  // ground point in window px
    // Level-aware two-pass window-pixel -> ground cell (the exact resolution
    // the right-click move path uses); shared with placement_cell.
    void resolve_ground_cell(int px, int py, int& cx, int& cy) const;
    void commit_click(int px, int py, bool shift);  // house-0-only selection (see .cpp)
    void commit_box(bool shift);                    // house-0-only, corpse-skipping box select
    void commit_double(int px, int py);             // house-0-only type-select
    // True iff roster type `t` is in refinery_types_ (set_refinery_types).
    bool is_refinery_type(int t) const {
        for (int r : refinery_types_)
            if (r == t) return true;
        return false;
    }
    // True iff roster type `t` is one of the player's unit-factory types.
    bool is_unit_factory_type(int t) const {
        for (int r : unit_factory_types_)
            if (r == t) return true;
        return false;
    }
    // Engineer/Spy Capture [RE: capture.md §B]: the infiltration purpose for a
    // building of roster type `t` (None if unset or out of range).
    World::InfiltratePurpose infiltrate_purpose_of(int t) const {
        return (t >= 0 && t < static_cast<int>(infiltrate_purposes_.size()))
                   ? infiltrate_purposes_[t]
                   : World::InfiltratePurpose::None;
    }
    // Factory-select / primary-set on a non-drag left click [RE: production.md
    // §1.2]. If the click resolves to an own (house 0) alive unit-factory:
    // re-clicking the ALREADY-selected factory sets it primary; otherwise it
    // becomes the selection (clearing any unit selection). Returns true when a
    // factory was handled (so the caller skips unit selection / double-click);
    // returns false for a click NOT on an own factory, after clearing the
    // factory selection so the caller's normal selection logic runs.
    bool commit_factory_click(int px, int py);

    World& world_;
    Camera& cam_;
    bool quit_ = false;
    InputActions actions_;  // accumulator; cleared only by take_actions()

    // Left-button gesture state.
    bool l_down_ = false;
    bool dragging_ = false;
    bool down_shift_ = false;
    int down_x_ = 0, down_y_ = 0, cur_x_ = 0, cur_y_ = 0;

    // Double-click detection (uses event t_ms; no clock read here).
    uint32_t last_click_t_ = 0;
    int last_click_x_ = 0, last_click_y_ = 0;

    // Placement mode (Task 7a): place_type_ >= 0 while placing a building of
    // that roster type; place_fw_/fh_ is its foundation size.
    int place_type_ = -1;
    int place_fw_ = 1, place_fh_ = 1;

    // CB-4 Superweapons: true while SW-targeting is armed (a click on the map
    // then reports sw_fire_confirmed). Default false -- inert in every demo.
    bool sw_targeting_ = false;

    // Repair & Sell cursor modes [RE: repair-sell.md §C1]. Default None +
    // 0 owned -> both modes are unreachable until main.cpp arms one.
    BaseMode mode_ = BaseMode::None;
    int owned_buildings_ = 0;
    void set_mode_(BaseMode want, int control);

    // Economy phase: refinery roster types (set_refinery_types).
    std::vector<int> refinery_types_;

    // Production phase: unit-factory roster types + the currently selected own
    // factory (set_unit_factory_types / commit_factory_click). -1 = none.
    std::vector<int> unit_factory_types_, inf_factory_types_, veh_factory_types_;
    int selected_building_ = -1;

    // Engineer/Spy Capture phase [RE: capture.md §B]: per-roster-index spy
    // infiltration purpose + the two rules constants (set_infiltrate_params).
    // Empty/defaults -> the spy gesture no-ops (interactive-only).
    std::vector<World::InfiltratePurpose> infiltrate_purposes_;
    double spy_steal_pct_ = 0.5;       // SpyMoneyStealPercent
    int spy_blackout_frames_ = 1000;   // SpyPowerBlackout

    // Camera clamp bounds (computed once from the map's screen bbox + viewport;
    // same rule as the previous drag clamp -- see input.cpp).
    int view_w_ = 0, view_h_ = 0;
    int min_cam_x_ = 0, max_cam_x_ = 0, min_cam_y_ = 0, max_cam_y_ = 0;
};

}  // namespace ra2engine
