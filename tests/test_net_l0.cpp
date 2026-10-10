// Net L0 determinism foundations: the session seed (one value seeds every sim
// RNG stream, recorded in saves).
#include "doctest.h"

#include <cstdint>
#include <vector>

#include "asset_fixture.hpp"
#include "savegame.hpp"
#include "world.hpp"

using namespace ra2engine;

TEST_CASE("netL0 session seed: 0 keeps the fixed seeds, a seed derives every stream, saves keep it") {
    if (!ra2fixture::mapsmd03_available()) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    const ra2assets::MapFile& map = ra2fixture::mapsmd03_map();
    World plain(map);
    CHECK(plain.session_seed() == 0u);
    CHECK(plain.session_stream_seed(0x2545F491u) == 0x2545F491u);   // gates: historical seeds

    World a(map), b(map), c(map);
    a.set_session_seed(0x1234u);
    b.set_session_seed(0x1234u);
    c.set_session_seed(0x1235u);
    CHECK(a.session_stream_seed(0x2545F491u) != 0x2545F491u);
    CHECK(a.session_stream_seed(0x2545F491u) == b.session_stream_seed(0x2545F491u));
    CHECK(a.session_stream_seed(0x2545F491u) != c.session_stream_seed(0x2545F491u));
    CHECK(a.session_stream_seed(7u, 1u) != a.session_stream_seed(7u, 2u));   // per-house salt
    CHECK(save::save_world(a) == save::save_world(b));
    CHECK(save::save_world(a) != save::save_world(c));
    CHECK(save::save_world(a) != save::save_world(plain));

    World d(map);
    REQUIRE(save::load_world(d, save::save_world(a)));
    CHECK(d.session_seed() == 0x1234u);
    CHECK(save::save_world(d) == save::save_world(a));
}
