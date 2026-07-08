#include "doctest.h"
#include "iso_project.hpp"

using ra2engine::ScreenPt;
using ra2engine::cell_to_screen;
using ra2engine::screen_to_cell;

TEST_CASE("proj: diamond stagger sanity (tile 60x30)") {
    const int tw = 60, th = 30, lp = 10;

    ScreenPt p = cell_to_screen(0, 0, 0, tw, th, lp);
    CHECK(p.x == 0);
    CHECK(p.y == 0);

    p = cell_to_screen(1, 0, 0, tw, th, lp);
    CHECK(p.x == tw / 2);
    CHECK(p.y == th / 2);

    p = cell_to_screen(0, 1, 0, tw, th, lp);
    CHECK(p.x == -(tw / 2));
    CHECK(p.y == th / 2);

    p = cell_to_screen(1, 1, 0, tw, th, lp);
    CHECK(p.x == 0);
    CHECK(p.y == th);

    // level raises the tile on screen: py decreases by level * level_px,
    // px is unaffected.
    ScreenPt base = cell_to_screen(3, 4, 0, tw, th, lp);
    ScreenPt raised = cell_to_screen(3, 4, 2, tw, th, lp);
    CHECK(raised.x == base.x);
    CHECK(raised.y == base.y - 2 * lp);
}

TEST_CASE("proj: diamond stagger sanity (tile 48x24)") {
    const int tw = 48, th = 24, lp = 8;

    ScreenPt p = cell_to_screen(0, 0, 0, tw, th, lp);
    CHECK(p.x == 0);
    CHECK(p.y == 0);

    p = cell_to_screen(1, 0, 0, tw, th, lp);
    CHECK(p.x == tw / 2);
    CHECK(p.y == th / 2);

    p = cell_to_screen(0, 1, 0, tw, th, lp);
    CHECK(p.x == -(tw / 2));
    CHECK(p.y == th / 2);

    p = cell_to_screen(1, 1, 0, tw, th, lp);
    CHECK(p.x == 0);
    CHECK(p.y == th);
}

TEST_CASE("proj: round-trip grid 0..20, tile 60x30 (temperate)") {
    const int tw = 60, th = 30, lp = 10;
    for (int x = 0; x <= 20; ++x) {
        for (int y = 0; y <= 20; ++y) {
            ScreenPt p = cell_to_screen(x, y, 0, tw, th, lp);
            int rx = -1, ry = -1;
            screen_to_cell(p.x, p.y, tw, th, rx, ry);
            CHECK(rx == x);
            CHECK(ry == y);
        }
    }
}

TEST_CASE("proj: round-trip grid 0..20, tile 48x24") {
    const int tw = 48, th = 24, lp = 8;
    for (int x = 0; x <= 20; ++x) {
        for (int y = 0; y <= 20; ++y) {
            ScreenPt p = cell_to_screen(x, y, 0, tw, th, lp);
            int rx = -1, ry = -1;
            screen_to_cell(p.x, p.y, tw, th, rx, ry);
            CHECK(rx == x);
            CHECK(ry == y);
        }
    }
}

TEST_CASE("proj: round-trip with negative cells, tile 60x30") {
    const int tw = 60, th = 30, lp = 10;
    for (int x = -10; x <= 10; ++x) {
        for (int y = -10; y <= 10; ++y) {
            ScreenPt p = cell_to_screen(x, y, 0, tw, th, lp);
            int rx = -999, ry = -999;
            screen_to_cell(p.x, p.y, tw, th, rx, ry);
            CHECK(rx == x);
            CHECK(ry == y);
        }
    }
}

TEST_CASE("proj: round-trip ignores level (level offset stripped by caller)") {
    // screen_to_cell only inverts the level-0 plane; verify that inverting
    // a level-0 projection for the same (x,y) still matches regardless of
    // what level was used elsewhere (i.e. level never leaks into px).
    const int tw = 60, th = 30, lp = 10;
    ScreenPt p_lvl0 = cell_to_screen(5, 7, 0, tw, th, lp);
    ScreenPt p_lvl3 = cell_to_screen(5, 7, 3, tw, th, lp);
    CHECK(p_lvl0.x == p_lvl3.x);
    int rx = -1, ry = -1;
    screen_to_cell(p_lvl0.x, p_lvl0.y, tw, th, rx, ry);
    CHECK(rx == 5);
    CHECK(ry == 7);
}
