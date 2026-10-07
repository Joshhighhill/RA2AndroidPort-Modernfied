#include "touch_gestures.hpp"

#include <cstdlib>

namespace ra2engine {

void TouchGestures::emit(InputEvent::Kind k, int x, int y, bool right, uint32_t t) {
    InputEvent e;
    e.kind = k;
    e.x = x;
    e.y = y;
    e.button = right ? InputEvent::Right : InputEvent::Left;
    e.t_ms = t;
    e.touch = true;
    out_.push_back(e);
}

void TouchGestures::emit_click(int x, int y, bool right, uint32_t t) {
    emit(InputEvent::PointerMove, x, y, false, t);
    emit(InputEvent::PointerDown, x, y, right, t);
    emit(InputEvent::PointerUp, x, y, right, t);
}

void TouchGestures::finger_down(int64_t id, int x, int y, uint32_t t) {
    switch (st_) {
        case State::Idle:
            st_ = State::Pending;
            id0_ = id;
            x0_ = cx_ = x;
            y0_ = cy_ = y;
            t0_ = t;
            return;
        case State::Pending:
            // A second finger before the first moved: two-finger gesture.
            st_ = State::TwoFinger;
            id1_ = id;
            x1_ = x;
            y1_ = y;
            mid_x_ = (cx_ + x1_) / 2;
            mid_y_ = (cy_ + y1_) / 2;
            travel_ = 0;
            t2_ = t;
            return;
        default:
            return;   // extra fingers during a drag / long-press are ignored
    }
}

void TouchGestures::finger_move(int64_t id, int x, int y, uint32_t t) {
    if (st_ == State::TwoFinger) {
        if (id == id0_) { cx_ = x; cy_ = y; }
        else if (id == id1_) { x1_ = x; y1_ = y; }
        else return;
        const int mx = (cx_ + x1_) / 2, my = (cy_ + y1_) / 2;
        const int dx = mx - mid_x_, dy = my - mid_y_;
        if (dx || dy) {
            InputEvent e;
            e.kind = InputEvent::Pan;
            e.dx = dx;   // the map follows the fingers
            e.dy = dy;
            e.x = mx;
            e.y = my;
            e.t_ms = t;
            e.touch = true;
            out_.push_back(e);
            travel_ += std::abs(dx) + std::abs(dy);
            mid_x_ = mx;
            mid_y_ = my;
        }
        return;
    }
    if (id != id0_) return;
    cx_ = x;
    cy_ = y;
    if (st_ == State::Pending &&
        (std::abs(x - x0_) > cfg_.slop_px || std::abs(y - y0_) > cfg_.slop_px)) {
        st_ = State::Dragging;   // band box: press at the START point
        emit(InputEvent::PointerMove, x0_, y0_, false, t0_);
        emit(InputEvent::PointerDown, x0_, y0_, false, t0_);
    }
    if (st_ == State::Dragging) emit(InputEvent::PointerMove, x, y, false, t);
}

void TouchGestures::finger_cancel(int64_t id, int x, int y, uint32_t t) {
    switch (st_) {
        case State::Pending:
        case State::LongPressed:
            if (id == id0_) st_ = State::Idle;
            return;
        case State::Dragging:
            if (id != id0_) return;
            emit(InputEvent::PointerUp, x, y, false, t);
            st_ = State::Idle;
            return;
        case State::TwoFinger:
            if (id != id0_ && id != id1_) return;
            st_ = State::LongPressed;   // wait for the other finger, no click
            id0_ = (id == id0_) ? id1_ : id0_;
            return;
        default:
            return;
    }
}

void TouchGestures::finger_up(int64_t id, int x, int y, uint32_t t) {
    switch (st_) {
        case State::Pending:
            if (id != id0_) return;
            emit_click(x0_, y0_, false, t);   // tap
            st_ = State::Idle;
            return;
        case State::Dragging:
            if (id != id0_) return;
            emit(InputEvent::PointerUp, x, y, false, t);
            st_ = State::Idle;
            return;
        case State::LongPressed:
            if (id == id0_) st_ = State::Idle;
            return;
        case State::TwoFinger: {
            if (id != id0_ && id != id1_) return;
            // Two-finger TAP (short, no pan) = right click at the first finger.
            if (travel_ <= cfg_.slop_px && t - t2_ <= cfg_.two_finger_tap_ms)
                emit_click(x0_, y0_, true, t);
            // Wait for the other finger without re-entering a one-finger state.
            st_ = State::LongPressed;
            id0_ = (id == id0_) ? id1_ : id0_;
            return;
        }
        default:
            return;
    }
}

void TouchGestures::tick(uint32_t now) {
    if (st_ == State::Pending && now - t0_ >= cfg_.long_press_ms) {
        emit_click(x0_, y0_, true, now);   // long-press = right click
        st_ = State::LongPressed;
    }
}

}  // namespace ra2engine
