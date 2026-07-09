#include "doctest.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "input.hpp"
#include "iso_project.hpp"
#include "iso_renderer.hpp"  // terrain_screen_bounds
#include "pathfind.hpp"      // find_path (non-mutating reachability probing)
#include "world.hpp"

#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"

using namespace ra2assets;
using ra2engine::Camera;
using ra2engine::Cellxy;
using ra2engine::find_path;
using ra2engine::Input;
using ra2engine::InputEvent;
using ra2engine::Unit;
using ra2engine::World;

// Task 12 review Finding 3: Input had zero test coverage. These tests drive
// the exact same PURE functions the live loop uses -- Input::handle (tap vs.
// drag dispatch) and World::order_move / World::step (move-order lifecycle)
// -- with a REAL World built from the actual installed temperate map (same
// asset chain as engine/main.cpp's build_world / run_demo_move), rather than
// a hand-built MapFile: MapFile has no public constructor other than
// parse(), and hand-synthesizing a compressed [IsoMapPack5] blob is
// impractical in this codebase (see tests/test_map.cpp's own comment on
// this -- same reasoning applies here). Guarded like test_map.cpp's /
// test_rules.cpp's real-asset tests: skips cleanly (does not fail) if
// RA2_INSTALL_DIR's mapsmd03.mix isn't present.
namespace {

std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

// Loads the real temperate reference map (mapsmd03.mix, entry 0x133a0cb2 --
// same map + entry id as main.cpp/run_demo_move and test_map.cpp's guarded
// real-map test). Returns false (leaves `out` untouched) if the install /
// asset isn't found, so callers can skip the test cleanly.
bool load_real_map(MapFile& out) {
    const char* dir_env = std::getenv("RA2_INSTALL_DIR");
    std::string dir = dir_env ? dir_env : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';

    auto mix_bytes = slurp(dir + "mapsmd03.mix");
    if (mix_bytes.empty()) return false;
    MixFile mix = MixFile::open(std::move(mix_bytes));

    const MixEntry* e = mix.find(0x133a0cb2u);
    if (!e) return false;
    auto ini_bytes = mix.read(*e);
    out = MapFile::parse(std::string(ini_bytes.begin(), ini_bytes.end()));
    return true;
}

// 8 directions, cardinal first (immune to diagonal corner-cutting, so a hit
// at a given radius is very likely to be directly pathable).
constexpr int kDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                             {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};

// Scans outward from (from_x, from_y) for the first offset (skipping
// skip_dx,skip_dy, if given) that `find_path` -- the SAME pure algorithm
// World::order_move calls -- finds an actual (>1-cell) route to over
// `world`'s passability grid. Pure probing: calls find_path directly rather
// than World::order_move, so trying (and discarding) failing candidates has
// NO side effect on the unit's order/movement state -- essential for the
// mid-walk tests below, where a stray order_move call (even one that turns
// out unreachable and hits the cancel branch) would itself reset fx/fy and
// corrupt the very in-flight state being tested. The real temperate map's
// ~22550-cell interior blob (task-12-report.md) makes finding nothing within
// this radius effectively impossible.
bool find_nearby_reachable(const World& world, int from_x, int from_y, int skip_dx, int skip_dy,
                           int& out_x, int& out_y) {
    for (int r = 4; r <= 40; r += 2) {
        for (const auto& d : kDirs) {
            const int dx = d[0] * r, dy = d[1] * r;
            if (dx == skip_dx && dy == skip_dy) continue;
            const int gx = from_x + dx, gy = from_y + dy;
            std::vector<Cellxy> path = find_path(world.passable(), world.grid_w(), world.grid_h(),
                                                 {from_x, from_y}, {gx, gy});
            if (path.size() > 1) {
                out_x = gx;
                out_y = gy;
                return true;
            }
        }
    }
    return false;
}

// A screen pixel that Input::on_tap will invert back to EXACTLY (cell_x,
// cell_y) under an identity camera (cam.x == cam.y == 0): cell_to_screen /
// screen_to_cell are exact algebraic inverses for integer cells at level 0
// (see iso_project.hpp / test_projection.cpp / test_world.cpp's coordinate-
// reconciliation test) -- using level 0 here (not the cell's real level)
// mirrors exactly how those existing tests construct an exact-round-trip tap
// point, since screen_to_cell always inverts the level-0 plane.
void tap_point_for_cell(int cell_x, int cell_y, int& px, int& py) {
    const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cell_x, cell_y, 0, 60, 30, 15);
    px = p.x;
    py = p.y;
}

}  // namespace

TEST_CASE("Input: tap on the unit's cell selects it (identity camera)") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);

    REQUIRE_FALSE(world.unit().selected);
    int px, py;
    tap_point_for_cell(world.unit().cell_x, world.unit().cell_y, px, py);

    input.handle(InputEvent{InputEvent::PointerDown, px, py});
    input.handle(InputEvent{InputEvent::PointerUp, px, py});

    CHECK(world.unit().selected);
    CHECK_FALSE(world.unit().moving());  // a select tap must not also move it
}

TEST_CASE("Input: tap elsewhere while selected issues a real move order") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    world.unit().selected = true;  // pre-select, as if a prior tap already did

    int gx = 0, gy = 0;
    REQUIRE(find_nearby_reachable(world, world.unit().cell_x, world.unit().cell_y, 999999, 999999,
                                  gx, gy));
    int px, py;
    tap_point_for_cell(gx, gy, px, py);

    input.handle(InputEvent{InputEvent::PointerDown, px, py});
    input.handle(InputEvent{InputEvent::PointerUp, px, py});

    CHECK(world.unit().moving());
    REQUIRE_FALSE(world.unit().path.empty());
    CHECK(world.unit().path.back().x == gx);
    CHECK(world.unit().path.back().y == gy);
}

TEST_CASE("Input: small pointer move under the drag threshold is a tap, not a pan") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);

    // Arbitrary pixel far from the unit's real projected cell -- a tap here
    // must select nothing and move nothing.
    input.handle(InputEvent{InputEvent::PointerDown, 100, 100});
    input.handle(InputEvent{InputEvent::PointerMove, 102, 101});  // 2px < kDragThreshold(6)
    input.handle(InputEvent{InputEvent::PointerUp, 102, 101});

    CHECK_FALSE(world.unit().selected);
    CHECK_FALSE(world.unit().moving());
    CHECK(cam.x == 0);
    CHECK(cam.y == 0);
}

TEST_CASE("Input: drag past the threshold pans the camera and fires no select/move") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    const int view_w = 1280, view_h = 720;

    // Start from a CENTERED camera (same as main.cpp's centered_camera),
    // not the origin: the real map's screen-space bounding box sits far
    // from pixel (0,0) (its cells' raw iso x+y is never near zero), so an
    // uncentered (0,0) camera is already outside Finding 2's clamp region
    // on this map -- exactly what the dedicated clamp test above exercises.
    // This test is about the plain drag-vs-tap dispatch, so it needs a
    // realistic in-bounds starting camera (comfortably inside the clamped
    // range) so a small, ordinary drag is NOT itself clamped.
    int mnx, mny, mxx, mxy;
    REQUIRE(ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy));
    Camera cam;
    cam.x = view_w / 2 - (mnx + mxx) / 2;
    cam.y = view_h / 2 - (mny + mxy) / 2;
    const int cam_x0 = cam.x, cam_y0 = cam.y;

    Input input(world, cam, view_w, view_h);
    world.unit().selected = true;

    input.handle(InputEvent{InputEvent::PointerDown, 100, 100});
    input.handle(InputEvent{InputEvent::PointerMove, 140, 130});  // dx=40, dy=30, past threshold
    CHECK(cam.x == cam_x0 + 40);
    CHECK(cam.y == cam_y0 + 30);
    input.handle(InputEvent{InputEvent::PointerUp, 140, 130});

    // Drag never fires a tap, so selection state is untouched and no move
    // order was issued.
    CHECK(world.unit().selected);
    CHECK_FALSE(world.unit().moving());
}

TEST_CASE("Input: camera drag is clamped to the map's screen bounds (Finding 2)") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    const int view_w = 1280, view_h = 720;

    int mnx, mny, mxx, mxy;
    REQUIRE(ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy));
    const int margin_x = view_w / 2;
    const int margin_y = view_h / 2;
    const int expect_max_x = view_w + margin_x - mnx;
    const int expect_min_x = -margin_x - mxx;
    const int expect_max_y = view_h + margin_y - mny;
    const int expect_min_y = -margin_y - mxy;

    // Dragging absurdly far right/down clamps rather than following the raw
    // pointer delta.
    Camera cam_pos;
    Input input_pos(world, cam_pos, view_w, view_h);
    input_pos.handle(InputEvent{InputEvent::PointerDown, 0, 0});
    input_pos.handle(InputEvent{InputEvent::PointerMove, 1000000, 1000000});
    CHECK(cam_pos.x == expect_max_x);
    CHECK(cam_pos.y == expect_max_y);

    // ...and the opposite direction clamps to the min.
    Camera cam_neg;
    Input input_neg(world, cam_neg, view_w, view_h);
    input_neg.handle(InputEvent{InputEvent::PointerDown, 0, 0});
    input_neg.handle(InputEvent{InputEvent::PointerMove, -1000000, -1000000});
    CHECK(cam_neg.x == expect_min_x);
    CHECK(cam_neg.y == expect_min_y);
}

TEST_CASE("World::order_move: retargeting mid-walk keeps fx/fy (no backward snap, Finding 1)") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    Unit& u = world.unit();
    const int start_x = u.cell_x, start_y = u.cell_y;

    int t1x = 0, t1y = 0;
    REQUIRE(find_nearby_reachable(world, start_x, start_y, 999999, 999999, t1x, t1y));
    REQUIRE(world.order_move(t1x, t1y) > 1);
    REQUIRE(u.moving());

    // Walk partway: a small dt moves fx/fy off the starting (committed)
    // cell without completing the first path segment, i.e. genuinely
    // mid-walk with a fractional position -- exactly the state a live game
    // reorder would interrupt. move_cps defaults to 3.0 cells/sec, so 0.05s
    // covers 0.15 cells: comfortably short of any single 1.0/1.414-length
    // segment.
    world.step(0.05);
    REQUIRE(u.moving());
    const double fx_before = u.fx;
    const double fy_before = u.fy;
    const int cx_before = u.cell_x;
    const int cy_before = u.cell_y;
    REQUIRE(cx_before == start_x);  // confirms no cell was committed yet
    REQUIRE(cy_before == start_y);
    REQUIRE((fx_before != static_cast<double>(cx_before) ||
             fy_before != static_cast<double>(cy_before)));

    // Retarget to a DIFFERENT reachable cell mid-walk (probed purely, then
    // issued with a single order_move call so no extra order_move side
    // effect touches fx/fy before the assertions below).
    int t2x = 0, t2y = 0;
    REQUIRE(find_nearby_reachable(world, cx_before, cy_before, t1x - cx_before, t1y - cy_before,
                                  t2x, t2y));
    REQUIRE(world.order_move(t2x, t2y) > 1);

    // The fix: the SUCCESS branch must not reset fx/fy to cell_x/cell_y --
    // the in-flight interpolated position must survive the reorder exactly.
    CHECK(u.fx == fx_before);
    CHECK(u.fy == fy_before);
    // And a real new path toward the new target is active.
    CHECK(u.moving());
    REQUIRE_FALSE(u.path.empty());
    CHECK(u.path.back().x == t2x);
    CHECK(u.path.back().y == t2y);
}

TEST_CASE("World::order_move: cancel/off-grid order mid-walk snaps fx/fy cleanly (Finding 1)") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    Unit& u = world.unit();
    const int start_x = u.cell_x, start_y = u.cell_y;

    int t1x = 0, t1y = 0;
    REQUIRE(find_nearby_reachable(world, start_x, start_y, 999999, 999999, t1x, t1y));
    REQUIRE(world.order_move(t1x, t1y) > 1);
    world.step(0.05);
    REQUIRE(u.moving());
    // Confirm this is genuinely mid-walk (fractional), not sitting exactly
    // on the committed cell -- otherwise this test wouldn't exercise the bug.
    REQUIRE((u.fx != static_cast<double>(u.cell_x) || u.fy != static_cast<double>(u.cell_y)));

    // An off-grid goal always yields an empty path (out of
    // [0,grid_w)x[0,grid_h) range) -- the CANCEL/no-op branch.
    const std::size_t len2 = world.order_move(world.grid_w() + 1000, world.grid_h() + 1000);
    CHECK(len2 == 0);
    CHECK_FALSE(u.moving());
    CHECK(u.path.empty());
    // The fix: fx/fy must be snapped onto the committed cell, not stranded
    // at the fractional position it was interpolating through.
    CHECK(u.fx == static_cast<double>(u.cell_x));
    CHECK(u.fy == static_cast<double>(u.cell_y));
}
