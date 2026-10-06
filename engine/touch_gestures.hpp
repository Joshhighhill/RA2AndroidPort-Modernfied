#pragma once
// G-70 / G-71: touch gesture layer (Android). Pure C++17, no SDL -- the
// Platform feeds it finger events and drains the InputEvents it produces.
//
// The original is a mouse game; its scheme (left = select AND order, right =
// cancel/deselect, right-drag = scroll; see input_original.cpp) maps onto a
// finger like this [RE-FALLBACK: no original touch mapping exists; the time /
// distance thresholds are Android's ViewConfiguration defaults: long-press
// timeout 500 ms, touch slop 8 dp (~12 logical px at 1280x720 on a phone)]:
//   tap                       -> left click (select / order / sidebar / menu)
//   one-finger drag           -> left drag (band-box select)
//   long-press (no movement)  -> right click (cancel one mode, else deselect;
//                                on a cameo: hold / abandon)
//   two-finger drag           -> Pan (scroll the map with the fingers)
//   two-finger tap            -> right click
// Every emitted event has touch = true (GameScene then never edge-scrolls).
#include <cstdint>
#include <deque>

#include "platform.hpp"

namespace ra2engine {

class TouchGestures {
public:
    struct Config {
        uint32_t long_press_ms = 500;
        int slop_px = 12;
        uint32_t two_finger_tap_ms = 300;
    };
    TouchGestures() = default;
    explicit TouchGestures(Config c) : cfg_(c) {}

    void finger_down(int64_t id, int x, int y, uint32_t t_ms);
    void finger_move(int64_t id, int x, int y, uint32_t t_ms);
    void finger_up(int64_t id, int x, int y, uint32_t t_ms);
    // Time passing with no finger event (long-press detection).
    void tick(uint32_t now_ms);

    bool pending() const { return !out_.empty(); }
    bool pop(InputEvent& ev) {
        if (out_.empty()) return false;
        ev = out_.front();
        out_.pop_front();
        return true;
    }

private:
    enum class State { Idle, Pending, Dragging, LongPressed, TwoFinger };
    void emit(InputEvent::Kind k, int x, int y, bool right, uint32_t t);
    void emit_click(int x, int y, bool right, uint32_t t);

    Config cfg_;
    State st_ = State::Idle;
    int64_t id0_ = -1, id1_ = -1;
    int x0_ = 0, y0_ = 0, cx_ = 0, cy_ = 0;   // first finger: start / current
    int x1_ = 0, y1_ = 0;                     // second finger current
    uint32_t t0_ = 0;
    int mid_x_ = 0, mid_y_ = 0;               // two-finger centroid (last emitted)
    int travel_ = 0;                          // two-finger centroid travel (px)
    uint32_t t2_ = 0;                         // two-finger start time
    std::deque<InputEvent> out_;
};

}  // namespace ra2engine
