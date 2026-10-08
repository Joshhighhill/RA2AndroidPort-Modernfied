// W4-SQ-10: destructible high bridges on a real World (the deck carries ground
// units while intact, a break takes the whole span, a hut repairs it).
#include "doctest.h"
#include "asset_fixture.hpp"
#include "world.hpp"

using namespace ra2engine;

TEST_CASE("high bridges: deck passable while intact, broken by the trigger call, repaired by a hut") {
    if (!ra2fixture::mapsmd03_available()) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World w(ra2fixture::mapsmd03_map());
    const int W = w.grid_w();
    REQUIRE(W > 40);
    // A 5-cell deck over "water" (1) along y = 20, x = 10..14, with a land
    // ramp cell (2) at each end.
    std::vector<std::uint8_t> hb(static_cast<std::size_t>(W) * w.grid_h(), 0);
    hb[20 * W + 9] = 2;
    for (int x = 10; x <= 14; ++x) hb[20 * W + x] = 1;
    hb[20 * W + 15] = 2;
    w.set_high_bridges(hb);
    for (int x = 10; x <= 14; ++x) CHECK(w.walkable(x, 20));
    CHECK(w.high_bridge_cell(12, 20));
    CHECK_FALSE(w.high_bridge_destroyed(12, 20));
    CHECK(w.bridge_cells_destroyed() == 0);

    // Apply100Damage's FUN_00586BB0 call: the whole span goes.
    CHECK(w.force_bridge_break(12, 20) == 7);
    for (int x = 10; x <= 14; ++x) CHECK_FALSE(w.walkable(x, 20));
    CHECK(w.high_bridge_destroyed(9, 20));
    CHECK(w.bridge_cells_destroyed() == 7);
    CHECK(w.force_bridge_break(12, 20) == 0);   // nothing left to break

    // A hut within reach of the span restores it.
    const int hut = w.add_building(0, 16, 21, 1, 1, 0, 100, 0, 0.0, true);
    REQUIRE(hut >= 0);
    CHECK(w.repair_high_bridge_near(hut) == 7);
    for (int x = 10; x <= 14; ++x) CHECK(w.walkable(x, 20));
    CHECK(w.bridge_cells_destroyed() == 0);
}
