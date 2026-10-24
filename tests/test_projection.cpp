#include "doctest.h"
#include "iso_project.hpp"
#include "viewport_bounds.hpp"
#include "vehicle_animation.hpp"
#include "building_transition.hpp"
#include "power_cameo.hpp"
#include "civilian_frames.hpp"
#include "shell_layout.hpp"
#include <limits>
#include <algorithm>

using ra2engine::ScreenPt;
using ra2engine::cell_to_screen;
using ra2engine::screen_to_cell;

TEST_CASE("building transitions: packing covers each reverse frame and releases the vehicle at the end") {
    for(long tick=0;tick<30;++tick)
        CHECK(ra2engine::packing_frame(10,100,30,100+tick)==9-tick/3);
    CHECK(ra2engine::packing_frame(10,100,30,99)==-1);
    CHECK(ra2engine::packing_frame(10,100,30,130)==-1);
    CHECK_FALSE(ra2engine::transition_active(100,30,130));
    CHECK(ra2engine::packing_frame(0,0,10,0)==-1);
    CHECK(ra2engine::packing_frame(10,0,0,0)==-1);
}
TEST_CASE("building transitions: bunker opening and closing choose distinct front and back art") {
    CHECK(ra2engine::bunker_animation_slot(true,1)==10);
    CHECK(ra2engine::bunker_animation_slot(false,1)==11);
    CHECK(ra2engine::bunker_animation_slot(true,0)==10);
    CHECK(ra2engine::bunker_animation_slot(true,-1)==12);
    CHECK(ra2engine::bunker_animation_slot(false,-1)==13);
}

TEST_CASE("vehicle animation: unequal part lengths cycle independently without lost frames") {
    const int hull[]={0,1,2,0,1,2};
    const int turret[]={0,1,0,1,0,1};
    for(std::uint64_t stage=0;stage<6;++stage) {
        CHECK(ra2engine::hva_animation_frame(stage,3)==hull[stage]);
        CHECK(ra2engine::hva_animation_frame(stage,2)==turret[stage]);
    }
    CHECK(ra2engine::hva_animation_frame(100,0)==0);
    CHECK(ra2engine::hva_animation_frame(100,1)==0);
}

TEST_CASE("Yuri shell: sidebar and dialog hit targets match their large-shell positions") {
    using namespace ra2engine;
    const auto exit=yuri_shell_rect({484,409,156,42});
    CHECK(exit.x==644);CHECK(exit.y==535);CHECK(exit.w==156);CHECK(exit.h==42);
    const auto back=yuri_shell_point(exit.x+78,exit.y+21);
    CHECK(back.first==562);CHECK(back.second==430);
    const SbRect field{200,80,170,20};const auto expanded=yuri_shell_rect(field);
    const auto point=yuri_shell_point(expanded.x+expanded.w/2,expanded.y+expanded.h/2);
    CHECK(std::abs(point.first-(field.x+field.w/2))<=1);
    CHECK(std::abs(point.second-(field.y+field.h/2))<=1);
    const auto footer=yuri_shell_rect({0,448,472,32});
    CHECK(footer.x==0);CHECK(footer.y==568);CHECK(footer.w==632);CHECK(footer.h==32);
}

TEST_CASE("civilian frames: occupied damage never chooses the rubble image") {
    CHECK(ra2engine::civilian_main_frame(false,false)==0);
    CHECK(ra2engine::civilian_main_frame(true,false)==1);
    CHECK(ra2engine::civilian_main_frame(false,true)==2);
    CHECK(ra2engine::civilian_main_frame(true,true)==1);
}

TEST_CASE("sidebar: power identifiers cannot alias production entries") {
    using namespace ra2engine;
    for (int n=1;n<=12;++n) {
        const auto type=static_cast<SuperweaponType>(n);
        CHECK(power_cameo_type(power_cameo_id(type))==type);
        CHECK(power_cameo_id(type)<-1);
    }
    CHECK(power_cameo_type(-1)==SuperweaponType::None);
    CHECK(power_cameo_type(0)==SuperweaponType::None);
    CHECK(power_cameo_type(-2000)==SuperweaponType::None);
}

TEST_CASE("presentation: spinning turret uses game ticks without changing combat facing") {
    const double combat=1234;
    CHECK(ra2engine::spinning_turret_facing(combat,1,65536,false)==combat);
    CHECK(ra2engine::spinning_turret_facing(combat,1,0,true)==combat);
    CHECK(ra2engine::spinning_turret_facing(combat,0,65536,true)==0);
    CHECK(ra2engine::spinning_turret_facing(combat,1,65536,true)==doctest::Approx(65536.0/15));
    CHECK(ra2engine::spinning_turret_facing(combat,15,65536,true)==0);
    CHECK(combat==1234);
}

TEST_CASE("viewport: cliff extension remains visible when its base is outside") {
    using ra2engine::viewport_intersects;
    CHECK_FALSE(viewport_intersects(20, -40, 60, 30, 100, 100));
    CHECK(viewport_intersects(20, -10, 60, 60, 100, 100));
    CHECK_FALSE(viewport_intersects(-70, 20, 60, 30, 100, 100));
    CHECK(viewport_intersects(-10, 20, 60, 30, 100, 100));
    CHECK_FALSE(viewport_intersects(100, 0, 60, 30, 100, 100));
    CHECK_FALSE(viewport_intersects(0, 0, 0, 30, 100, 100));
    CHECK(viewport_intersects(-10, 0, std::numeric_limits<int>::max(), 30, 100, 100));
}

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

// W4-FIX-POS [RE: FUN_00459ee0 / game.exe FUN_00453c70]: the building canvas
// centre is the origin cell's TOP vertex for every foundation size. The old
// clone anchor (foundation bbox centre) sat ((fw-fh)*15, (fw+fh)*7.5) px
// away -- the audit's per-size error table (docs/gap/wave4/01-positioning.md §2).
TEST_CASE("proj: building draw point = origin cell top vertex, any foundation") {
    struct F { int fw, fh, ex, ey; };
    const F sizes[] = {{1, 1, 0, 15}, {2, 2, 0, 30}, {3, 3, 0, 45}, {4, 4, 0, 60}, {3, 4, -15, 52}, {2, 6, -60, 60}};
    for (const F& f : sizes) {
        const int cx = 61, cy = 69, level = 6;
        const ra2engine::ScreenPt d = ra2engine::building_draw_point(cx, cy, level);
        const ra2engine::ScreenPt o = ra2engine::cell_to_screen(cx, cy, level, 60, 30, 15);
        CHECK(d.x == o.x + 30);
        CHECK(d.y == o.y);
        int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
        for (int j = 0; j < f.fh; ++j)
            for (int i = 0; i < f.fw; ++i) {
                const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cx + i, cy + j, level, 60, 30, 15);
                minx = std::min(minx, p.x); miny = std::min(miny, p.y);
                maxx = std::max(maxx, p.x + 60); maxy = std::max(maxy, p.y + 30);
            }
        CHECK((minx + maxx) / 2 - d.x == f.ex);
        CHECK((miny + maxy) / 2 - d.y == f.ey);
    }
}

TEST_CASE("proj: overlay y offset table (FUN_005fd4b0 / FUN_00480020)") {
    CHECK(ra2engine::overlay_y_offset(false, false, 5, false, 0) == 0);     // rock / low bridge
    CHECK(ra2engine::overlay_y_offset(true, false, 5, false, 0) == -12);    // wall / ore / crate
    CHECK(ra2engine::overlay_y_offset(false, true, 5, false, 0) == -1);     // railroad
    CHECK(ra2engine::overlay_y_offset(false, false, 0x7E, false, 0) == -13);
    CHECK(ra2engine::overlay_y_offset(false, false, 25, true, 0) == -16);   // high bridge
    CHECK(ra2engine::overlay_y_offset(false, false, 25, true, 9) == -31);
    CHECK(ra2engine::overlay_y_offset(false, false, 25, true, 17) == -31);
    CHECK(ra2engine::overlay_y_offset(false, false, 25, true, 18) == -16);
    CHECK(ra2engine::overlay_y_offset(false, false, 0xEF, false, 0) == -15);
    const ra2engine::ScreenPt c = ra2engine::overlay_draw_center(3, 4, 2, -12);
    const ra2engine::ScreenPt p = ra2engine::cell_to_screen(3, 4, 2, 60, 30, 15);
    CHECK(c.x == p.x + 30);
    CHECK(c.y == p.y + 3);
    const ra2engine::ScreenPt v = ra2engine::cell_top_vertex(3, 4, 2);
    CHECK(v.x == p.x + 30);
    CHECK(v.y == p.y);
}
