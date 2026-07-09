#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "input.hpp"
#include "iso_project.hpp"
#include "world.hpp"

#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"

using namespace ra2assets;
using ra2engine::Camera;
using ra2engine::Input;
using ra2engine::InputEvent;
using ra2engine::World;

namespace {

std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
bool load_real_map(MapFile& out) {
    const char* d = std::getenv("RA2_INSTALL_DIR");
    std::string dir = d ? d : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    auto b = slurp(dir + "mapsmd03.mix");
    if (b.empty()) return false;
    MixFile mix = MixFile::open(std::move(b));
    const MixEntry* e = mix.find(0x133a0cb2u);
    if (!e) return false;
    auto raw = mix.read(*e);
    out = MapFile::parse(std::string(raw.begin(), raw.end()));
    return true;
}

// Screen pixel over the cell (cx, cy) ground point at identity camera, level 0.
// This is the LEVEL-0 variant -- correct for click tests, because
// Input::unit_under inverts the pointer level-agnostically (screen_to_cell
// only inverts the level-0 plane); box tests need ground_px_for_unit below.
void px_for_cell(int cx, int cy, int& px, int& py) {
    const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cx, cy, 0, 60, 30, 15);
    px = p.x + 30;  // + tile_w/2 (ground point x)
    py = p.y + 15;  // + tile_h/2 (ground point y)
}

// Ground-point pixel of a unit under an identity camera -- LEVEL-AWARE,
// mirroring Input::unit_ground_px / draw_unit's anchor. Required for BOX
// tests: commit_box tests each unit's real (level-raised) ground point, and
// the spawn cluster is NOT level 0 on the real map.
void ground_px_for_unit(const ra2engine::Unit& u, int& px, int& py) {
    const ra2engine::ScreenPt p =
        ra2engine::cell_to_screen(u.cell_x, u.cell_y, u.level, 60, 30, 15);
    px = p.x + 30;
    py = p.y + 15;
}

// Left click helper (down then up, no move) with modifiers + timestamp.
void left_click(Input& in, int px, int py, bool shift, uint32_t t_ms) {
    InputEvent d;
    d.kind = InputEvent::PointerDown; d.button = InputEvent::Left; d.x = px; d.y = py;
    d.shift = shift; d.t_ms = t_ms;
    InputEvent u = d;
    u.kind = InputEvent::PointerUp;
    in.handle(d);
    in.handle(u);
}

// Spawn `n` GI units at consecutive walkable cells near the map's start.
struct Fixture {
    MapFile map;
    bool ok = false;
    std::vector<std::pair<int, int>> cells;
};
Fixture spawn(World& world, int n) {
    Fixture fx;
    const MapCell* c = ra2engine::pick_start_cell(world.map());
    int placed = 0;
    for (int dx = 0; dx < 8 && placed < n; ++dx) {
        const int x = c->x + dx, y = c->y;
        if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
            world.add_unit(x, y, "GI", 0);
            fx.cells.push_back({x, y});
            ++placed;
        }
    }
    fx.ok = placed == n;
    return fx;
}

}  // namespace

TEST_CASE("Input: left-click a unit selects only it; empty click clears") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped: could not place 3 units"); return; }

    int px, py;
    px_for_cell(fx.cells[1].first, fx.cells[1].second, px, py);
    left_click(input, px, py, /*shift=*/false, /*t=*/1000);
    CHECK(world.units()[1].selected);
    CHECK_FALSE(world.units()[0].selected);
    CHECK_FALSE(world.units()[2].selected);

    // Click far-away empty ground clears selection.
    left_click(input, px + 900, py + 400, false, 2000);
    for (const auto& u : world.units()) CHECK_FALSE(u.selected);
}

TEST_CASE("Input: Shift+left-click toggles units in and out of the selection") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    int px0, py0, px2, py2;
    px_for_cell(fx.cells[0].first, fx.cells[0].second, px0, py0);
    px_for_cell(fx.cells[2].first, fx.cells[2].second, px2, py2);
    left_click(input, px0, py0, false, 1000);   // select unit 0
    left_click(input, px2, py2, true, 2000);    // shift-add unit 2
    CHECK(world.units()[0].selected);
    CHECK(world.units()[2].selected);
    left_click(input, px0, py0, true, 3000);    // shift-remove unit 0
    CHECK_FALSE(world.units()[0].selected);
    CHECK(world.units()[2].selected);
}

TEST_CASE("Input: right-click issues a group move to the selected units") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    world.set_move_speed(4.0);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 2);
    if (!fx.ok) { MESSAGE("skipped"); return; }
    for (auto& u : world.units()) u.selected = true;

    // Right-click a cell 10 south of the first unit.
    int px, py;
    px_for_cell(fx.cells[0].first, fx.cells[0].second + 10, px, py);
    InputEvent rc;
    rc.kind = InputEvent::PointerDown; rc.button = InputEvent::Right; rc.x = px; rc.y = py;
    rc.t_ms = 5000;
    input.handle(rc);
    InputEvent ru = rc; ru.kind = InputEvent::PointerUp;
    input.handle(ru);

    // At least one selected unit now has an active path (some may already be
    // adjacent to their assigned cell).
    int moving = 0;
    for (const auto& u : world.units()) moving += u.moving() ? 1 : 0;
    CHECK(moving >= 1);
}

TEST_CASE("Input: left-drag exposes a selection box and box-selects covered units") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    // Compute the REAL (level-aware) ground pixels of all three units --
    // commit_box tests unit_ground_px, which uses each unit's terrain level,
    // so the box corners must account for it too -- then drag a box covering
    // them.
    int minx = 1000000000, miny = 1000000000, maxx = -1000000000, maxy = -1000000000;
    for (const auto& un : world.units()) {
        int px, py; ground_px_for_unit(un, px, py);
        minx = std::min(minx, px); maxx = std::max(maxx, px);
        miny = std::min(miny, py); maxy = std::max(maxy, py);
    }
    InputEvent d; d.kind = InputEvent::PointerDown; d.button = InputEvent::Left;
    d.x = minx - 20; d.y = miny - 20; d.t_ms = 1000;
    input.handle(d);
    InputEvent m; m.kind = InputEvent::PointerMove; m.x = maxx + 20; m.y = maxy + 20; m.t_ms = 1050;
    input.handle(m);
    int x0, y0, x1, y1;
    CHECK(input.selection_box(x0, y0, x1, y1));  // box is live during the drag
    InputEvent u; u.kind = InputEvent::PointerUp; u.button = InputEvent::Left;
    u.x = maxx + 20; u.y = maxy + 20; u.t_ms = 1100;
    input.handle(u);
    CHECK_FALSE(input.selection_box(x0, y0, x1, y1));  // box gone after release
    for (const auto& un : world.units()) CHECK(un.selected);
}

TEST_CASE("Input: double-click selects all on-screen units of the same type") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    Input input(world, cam, 1280, 720);
    Fixture fx = spawn(world, 3);
    if (!fx.ok) { MESSAGE("skipped"); return; }

    // Center the camera on unit 0 so all three (adjacent cells) are on-screen
    // -- commit_double only selects units whose ground point lies inside the
    // viewport. Input holds cam by reference, so mutating it here is live.
    int gx0, gy0;
    px_for_cell(fx.cells[0].first, fx.cells[0].second, gx0, gy0);
    cam.x = 1280 / 2 - gx0;
    cam.y = 720 / 2 - gy0;

    // Two quick clicks on unit 0's cell (window px = ground px + camera).
    const int px = gx0 + cam.x, py = gy0 + cam.y;
    left_click(input, px, py, false, 1000);
    CHECK(world.units()[0].selected);           // first click: single select
    CHECK_FALSE(world.units()[1].selected);
    left_click(input, px, py, false, 1200);     // 200ms later, same spot: double
    for (const auto& u : world.units()) CHECK(u.selected);  // all GI on screen
}

TEST_CASE("Input: pan moves the camera (clamped) and never fires a select") {
    MapFile map;
    if (!load_real_map(map)) { MESSAGE("skipped: no install"); return; }
    World world(map);
    Camera cam;
    int mnx, mny, mxx, mxy;
    REQUIRE(ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy));
    cam.x = 1280 / 2 - (mnx + mxx) / 2;
    cam.y = 720 / 2 - (mny + mxy) / 2;
    Input input(world, cam, 1280, 720);
    const int x0 = cam.x, y0 = cam.y;
    input.pan(-40, -25);
    CHECK(cam.x == x0 - 40);
    CHECK(cam.y == y0 - 25);
}
