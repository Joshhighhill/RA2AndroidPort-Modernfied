// Wave-6 fix #2 (missions): target legality, IsInPlayfield, house handicap.
#include "doctest.h"

#include "asset_fixture.hpp"
#include "world.hpp"

using namespace ra2engine;

namespace {
bool open_2x2(const World& w, int& x, int& y) {
    for (int yy = 20; yy < w.grid_h() - 20; ++yy)
        for (int xx = 20; xx < w.grid_w() - 20; ++xx)
            if (xx + yy > w.grid_w() + 4 && w.walkable(xx, yy) && w.walkable(xx + 1, yy) && w.walkable(xx, yy + 1) &&
                w.walkable(xx + 1, yy + 1)) { x = xx; y = yy; return true; }
    return false;
}
}  // namespace

TEST_CASE("w6 legal_threat: campaign Insignificant + passive owner, LegalTarget, playfield") {
    if (!ra2fixture::mapsmd03_available()) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World w(ra2fixture::mapsmd03_map());
    int x = -1, y = -1;
    REQUIRE(open_2x2(w, x, y));
    const int b = w.add_building(-1, x, y, 2, 2, 2, 500, 0, 0.0, true);
    UnitCombat c;
    c.strength = 100;
    c.w6.insignificant = true;
    w.add_combat_unit(x + 4, y + 4, 2, c);
    const int u = static_cast<int>(w.units().size()) - 1;
    // Inert outside a campaign host: everything is a legal threat.
    w.set_building_target_flags(b, /*insignificant=*/true, /*legal=*/true);
    CHECK(w.legal_threat_building(1, b));
    CHECK(w.legal_threat_unit(1, u));
    // Campaign: an Insignificant unit is rejected; an Insignificant building
    // only when its owner is MultiplayPassive.
    w.set_playfield(/*campaign=*/true, /*valid=*/false, 0, 0, 0, 0, 0);
    CHECK_FALSE(w.legal_threat_unit(1, u));
    CHECK(w.legal_threat_building(1, b));
    w.set_passive_house(2, true);
    CHECK_FALSE(w.legal_threat_building(1, b));
    w.set_building_target_flags(b, false, true);
    CHECK(w.legal_threat_building(1, b));
    w.set_building_target_flags(b, false, /*legal=*/false);
    CHECK_FALSE(w.legal_threat_building(1, b));
    w.set_building_target_flags(b, false, true);
    // A usable area that excludes the building: not in the playfield until a
    // ResizeView (set_playfield again) covers it.
    w.set_playfield(true, true, w.grid_w(), 0, 0, 1, 1);
    CHECK_FALSE(w.cell_in_playfield(x, y));
    CHECK_FALSE(w.legal_threat_building(1, b));
    w.set_playfield(true, true, w.grid_w(), 0, 0, w.grid_w(), w.grid_h());
    CHECK(w.legal_threat_building(1, b));
}

TEST_CASE("w6 handicap: the house ROF multiplies a defence rearm (ftol)") {
    if (!ra2fixture::mapsmd03_available()) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World w(ra2fixture::mapsmd03_map());
    CHECK(w.house_rof_mul(1) == 1.0);
    CHECK(w.house_rof_frames(1, 80) == 80);
    w.set_house_handicap(1, 1.0, 1.2);   // [Difficult] ROF=1.2
    CHECK(w.house_rof_frames(1, 80) == 96);
    w.set_house_handicap(0, 1.0, 0.8);   // [Easy] ROF=.8
    CHECK(w.house_rof_frames(0, 60) == 48);
    CHECK(w.house_rof_frames(2, 60) == 60);
}
