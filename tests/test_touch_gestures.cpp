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

TEST_CASE("touch: a hold fires on the lift as a right click marked hold (W4-AM-07)") {
    TouchGestures g;
    g.finger_down(1, 50, 60, 1000);
    g.tick(1500);                        // 500 ms: still a tap (old threshold)
    CHECK_FALSE(g.pending());
    int hx = 0, hy = 0;
    float pr = 0.0f;
    CHECK(g.hold_feedback(1500, hx, hy, pr));   // ring shown after 150 ms
    CHECK(hx == 50);
    CHECK(pr > 0.0f);
    CHECK(pr < 1.0f);
    g.tick(1600);                        // 600 ms: held, nothing emitted yet
    CHECK_FALSE(g.pending());
    CHECK(g.hold_feedback(1600, hx, hy, pr));
    CHECK(pr == doctest::Approx(1.0f));
    g.finger_up(1, 50, 60, 1700);
    auto v = drain(g);
    REQUIRE(v.size() == 3);
    CHECK(v[1].button == InputEvent::Right);
    CHECK(v[1].hold);
    CHECK(v[2].button == InputEvent::Right);
    CHECK(v[2].hold);

    TouchGestures s;                     // a 550 ms slow tap is a TAP
    s.finger_down(1, 50, 60, 1000);
    s.tick(1550);
    s.finger_up(1, 50, 60, 1550);
    v = drain(s);
    REQUIRE(v.size() == 3);
    CHECK(v[1].button == InputEvent::Left);
    CHECK_FALSE(v[1].hold);
}

TEST_CASE("touch: one-finger drag PANS the map (W4-AM-01)") {
    TouchGestures g;
    g.finger_down(1, 100, 100, 1000);
    g.finger_move(1, 105, 105, 1010);   // within slop: nothing yet
    CHECK_FALSE(g.pending());
    g.finger_move(1, 160, 140, 1020);
    g.finger_move(1, 200, 180, 1030);
    g.tick(5000);                        // no hold once panning
    g.finger_up(1, 200, 180, 1040);
    const auto v = drain(g);
    REQUIRE(v.size() == 2);
    int dx = 0, dy = 0;
    for (const auto& e : v) {
        CHECK(e.kind == InputEvent::Pan);
        CHECK(e.x0 == 100);
        CHECK(e.y0 == 100);
        dx += e.dx;
        dy += e.dy;
    }
    CHECK(dx == 100);   // the map follows the finger from the touch-down point
    CHECK(dy == 80);
}

TEST_CASE("touch: hold then drag is a left band box from the hold point") {
    TouchGestures g;
    g.finger_down(1, 100, 100, 1000);
    g.tick(1700);
    CHECK_FALSE(g.pending());
    g.finger_move(1, 160, 140, 1750);
    g.finger_move(1, 200, 180, 1800);
    g.finger_up(1, 200, 180, 1850);
    const auto v = drain(g);
    REQUIRE(v.size() == 5);
    CHECK(v[1].kind == InputEvent::PointerDown);
    CHECK(v[1].button == InputEvent::Left);
    CHECK(v[1].x == 100);
    CHECK(v[3].kind == InputEvent::PointerMove);
    CHECK(v[4].kind == InputEvent::PointerUp);
    CHECK(v[4].x == 200);
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
    d.tick(1700);
    d.finger_move(1, 160, 140, 1750);
    (void)drain(d);
    d.finger_cancel(1, 160, 140, 1100);
    auto v = drain(d);
    REQUIRE(v.size() == 1);
    CHECK(v[0].kind == InputEvent::PointerUp);
}
