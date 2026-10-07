// G-70 / G-71: the touch gesture layer (engine/touch_gestures.{hpp,cpp}).
#include "doctest.h"
#include "touch_gestures.hpp"

#include <vector>

using namespace ra2engine;

namespace {
std::vector<InputEvent> drain(TouchGestures& g) {
    std::vector<InputEvent> v;
    InputEvent e;
    while (g.pop(e)) v.push_back(e);
    return v;
}
}  // namespace

TEST_CASE("touch: a tap is ONE left click (no synthetic duplicate)") {
    TouchGestures g;
    g.finger_down(1, 100, 200, 1000);
    g.tick(1100);
    g.finger_up(1, 102, 201, 1150);
    const auto v = drain(g);
    REQUIRE(v.size() == 3);
    CHECK(v[1].kind == InputEvent::PointerDown);
    CHECK(v[1].button == InputEvent::Left);
    CHECK(v[2].kind == InputEvent::PointerUp);
    CHECK(v[1].x == 100);
    CHECK(v[1].touch);
}

TEST_CASE("touch: long-press is a right click and the lift adds nothing") {
    TouchGestures g;
    g.finger_down(1, 50, 60, 1000);
    g.tick(1400);
    CHECK_FALSE(g.pending());
    g.tick(1500);
    auto v = drain(g);
    REQUIRE(v.size() == 3);
    CHECK(v[1].button == InputEvent::Right);
    CHECK(v[2].button == InputEvent::Right);
    g.finger_up(1, 50, 60, 1700);
    CHECK_FALSE(g.pending());
}

TEST_CASE("touch: one-finger drag is a left band drag from the start point") {
    TouchGestures g;
    g.finger_down(1, 100, 100, 1000);
    g.finger_move(1, 105, 105, 1010);   // within slop: nothing yet
    CHECK_FALSE(g.pending());
    g.finger_move(1, 160, 140, 1020);
    g.finger_move(1, 200, 180, 1030);
    g.tick(5000);                        // no long-press once dragging
    g.finger_up(1, 200, 180, 1040);
    const auto v = drain(g);
    REQUIRE(v.size() == 5);
    CHECK(v[1].kind == InputEvent::PointerDown);
    CHECK(v[1].x == 100);
    CHECK(v[3].kind == InputEvent::PointerMove);
    CHECK(v[4].kind == InputEvent::PointerUp);
    CHECK(v[4].button == InputEvent::Left);
}

TEST_CASE("touch: two-finger drag pans; a two-finger tap is a right click") {
    TouchGestures g;
    g.finger_down(1, 100, 100, 1000);
    g.finger_down(2, 200, 100, 1010);
    g.finger_move(1, 130, 120, 1020);
    g.finger_move(2, 230, 120, 1030);
    auto v = drain(g);
    REQUIRE(v.size() == 2);
    CHECK(v[0].kind == InputEvent::Pan);
    int dx = 0, dy = 0;
    for (const auto& e : v) { dx += e.dx; dy += e.dy; }
    CHECK(dx == 30);
    CHECK(dy == 20);
    g.finger_up(1, 130, 120, 1100);
    g.finger_up(2, 230, 120, 1110);
    CHECK_FALSE(g.pending());   // it panned: no right click
    g.tick(9000);
    CHECK_FALSE(g.pending());

    TouchGestures h;
    h.finger_down(1, 100, 100, 1000);
    h.finger_down(2, 150, 100, 1020);
    h.finger_up(2, 150, 100, 1100);
    h.finger_up(1, 100, 100, 1120);
    v = drain(h);
    REQUIRE(v.size() == 3);
    CHECK(v[1].button == InputEvent::Right);
}

TEST_CASE("touch: a system-cancelled gesture never clicks (W2-AR-18)") {
    TouchGestures g;
    g.finger_down(1, 100, 100, 1000);
    g.finger_cancel(1, 100, 100, 1050);   // back-swipe took the touch
    g.tick(9000);
    CHECK_FALSE(g.pending());

    TouchGestures h;   // two-finger tap cancelled: no right click
    h.finger_down(1, 100, 100, 1000);
    h.finger_down(2, 150, 100, 1020);
    h.finger_cancel(2, 150, 100, 1100);
    h.finger_cancel(1, 100, 100, 1110);
    CHECK_FALSE(h.pending());

    TouchGestures d;   // a band box in progress is released, not left pressed
    d.finger_down(1, 100, 100, 1000);
    d.finger_move(1, 160, 140, 1050);
    (void)drain(d);
    d.finger_cancel(1, 160, 140, 1100);
    auto v = drain(d);
    REQUIRE(v.size() == 1);
    CHECK(v[0].kind == InputEvent::PointerUp);
}
