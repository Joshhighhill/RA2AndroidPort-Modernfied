#include "doctest.h"
#include "../engine/sidebar.hpp"
using namespace ra2engine;

TEST_CASE("sidebar layout: geometry from art dims") {
    SidebarArtDims art;   // defaults
    auto lay = SidebarLayout::compute(1280, 720, art, {1, 2, 3});
    CHECK(lay.bar.x == 1280 - art.top_w);
    CHECK(lay.bar.w == art.top_w);
    CHECK(lay.bar.h == 720);
    CHECK(lay.radar.y == 0);
    CHECK(lay.credits.y == lay.radar.y + lay.radar.h);
    CHECK(lay.tabs[0].y == lay.credits.y + lay.credits.h);
    CHECK(lay.tabs[1].x == lay.tabs[0].x + lay.bar.w / 4);   // 42 px pitch, 28 wide
    CHECK(lay.tabs[0].w == art.tab_w);
    REQUIRE(lay.cameos.size() == 3);
    CHECK(lay.cameos[0].type == 1);
    // 2-column grid: cell 0 and 1 same row, cell 2 next row
    CHECK(lay.cameos[0].rect.y == lay.cameos[1].rect.y);
    CHECK(lay.cameos[2].rect.y > lay.cameos[0].rect.y);
    CHECK(lay.cameos[2].rect.x == lay.cameos[0].rect.x);
    // power strip hugs the bar's left edge below the tabs
    CHECK(lay.power.x == lay.bar.x);
    CHECK(lay.power.y == lay.tabs[0].y + lay.tabs[0].h);
}

TEST_CASE("sidebar hit-testing") {
    SidebarArtDims art;
    auto lay = SidebarLayout::compute(1280, 720, art, {1, 2});
    auto c0 = lay.cameos[0].rect;
    CHECK(lay.cameo_at(c0.x + 1, c0.y + 1) == 0);
    CHECK(lay.cameo_at(c0.x - 200, c0.y) == -1);         // in the world, not the bar
    CHECK(lay.tab_at(lay.tabs[2].x + 1, lay.tabs[2].y + 1) == 2);
    CHECK(lay.tab_at(0, 0) == -1);
    CHECK(lay.contains(1280 - 1, 0));
    CHECK_FALSE(lay.contains(1280 - art.top_w - 1, 0));
}
