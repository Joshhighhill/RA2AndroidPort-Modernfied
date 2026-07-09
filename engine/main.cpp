// RA2 engine desktop entry point.
//
// Default: opens an interactive SDL window and renders the real temperate map
// terrain (arrow keys / drag to pan, Esc/close to quit).
//
// `--screenshot <out.png>`: renders exactly one frame with the camera centered
// on the map, reads the framebuffer back, writes it to a PNG, and exits.
//
// Asset chain (all under $RA2_INSTALL_DIR, default "D:\Red Alert 2"):
//   * map     : mapsmd03.mix, entry id 0x133a0cb2 (temperate, 103x110)
//   * palette : ra2.mix -> cache.mix -> isotem.pal (768-byte 6-bit iso pal)
//   * tilecfg : ra2.mix -> local.mix -> temperat.ini  ([TileSetNNNN] mapping)
//   * tiles   : ra2.mix -> isotemp.mix -> <FileName><NN>.tem templates
// See engine/iso_renderer.hpp (Theater) for the tile-index -> template mapping.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <SDL.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "input.hpp"
#include "iso_project.hpp"
#include "iso_renderer.hpp"
#include "platform.hpp"
#include "texture_cache.hpp"
#include "world.hpp"

#include "ra2assets/fileio.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/rules.hpp"
#include "ra2assets/shp.hpp"

using namespace ra2assets;

namespace {

std::string env_install_dir() {
#ifdef __ANDROID__
    // Android has no env vars / CLI. The user pushes the game files to the
    // app's external files dir (see README "Android": adb push ... ra2.mix,
    // mapsmd03.mix, ...). SDL_AndroidGetExternalStoragePath() resolves that to
    // /storage/emulated/0/Android/data/org.ra2.engine/files (no runtime
    // permission needed for the app's own dir); we read the mix chain from its
    // `ra2/` subfolder. Assets are NOT bundled in the APK (copyright).
    const char* ext = SDL_AndroidGetExternalStoragePath();
    std::string dir = ext ? ext : "/sdcard/Android/data/org.ra2.engine/files";
    dir += "/ra2/";
    // std::cerr/std::cout are not captured on Android; SDL_Log routes to
    // logcat (tag "SDL"). Log the resolved asset dir so a missing/misplaced
    // game-file set is diagnosable from `adb logcat` instead of the app
    // silently exiting.
    SDL_Log("[assets] android asset dir = %s", dir.c_str());
    return dir;
#else
    const char* e = std::getenv("RA2_INSTALL_DIR");
    std::string dir = e ? e : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    return dir;
#endif
}

// Everything the renderer needs, loaded once from the mix chain.
struct Assets {
    MapFile map;
    Palette pal;
    ra2engine::Theater theater;
};

// `ra2` is the already-open ra2.mix (holds the nested theater/config/palette
// mixes) -- opened ONCE by each mode entry point and shared with
// load_unit_assets(), instead of each loader slurping the 268 MB file
// independently (was 79 s cold start; see docs/superpowers/specs/
// 2026-07-09-foundation-batch-design.md, sub-project 4).
Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2) {
    // Map: read the temperate reference map INI out of mapsmd03.mix.
    MixFile maps = MixFile::open(ra2assets::read_file(dir + "mapsmd03.mix"));
    const MixEntry* me = maps.find(0x133a0cb2u);
    if (!me) throw std::runtime_error("map entry 0x133a0cb2 not found in mapsmd03.mix");
    auto map_raw = maps.read(*me);
    MapFile map = MapFile::parse(std::string(map_raw.begin(), map_raw.end()));

    const std::string theater = map.theater();
    if (theater != "TEMPERATE") {
        // Milestone 1 wires temperate only; other theaters map the same way
        // (isosnow.mix/snow.ini/.sno, isourb.mix/urban.ini/.urb).
        throw std::runtime_error("unsupported theater (only TEMPERATE wired): " + theater);
    }

    // Palette: isotem.pal lives in cache.mix.
    MixFile cache = ra2.open_nested("cache.mix");
    const MixEntry* pe = cache.find(std::string("isotem.pal"));
    if (!pe) throw std::runtime_error("isotem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    Palette pal = Palette::load(pal_raw.data(), pal_raw.size());

    // Tile-set control INI: temperat.ini lives in local.mix.
    MixFile local = ra2.open_nested("local.mix");
    const MixEntry* ce = local.find(std::string("temperat.ini"));
    if (!ce) throw std::runtime_error("temperat.ini not found in ra2.mix/local.mix");
    auto ini_raw = local.read(*ce);
    std::string ini_text(ini_raw.begin(), ini_raw.end());

    // Tiles: the .tem templates live in isotemp.mix.
    MixFile isotemp = ra2.open_nested("isotemp.mix");

    ra2engine::Theater th(std::move(isotemp), ini_text, "tem");

    std::cerr << "[assets] theater=" << theater << " size=" << map.width() << "x"
              << map.height() << " cells=" << map.cells().size()
              << " templates=" << th.template_count() << "\n";

    return Assets{std::move(map), pal, std::move(th)};
}

// Everything needed to draw the one placed unit: its decoded SHP, the
// (separate, temperate UNIT, not terrain) palette, and the resolved image
// name (Rules::image() output, e.g. "GI") the World's Unit is tagged with.
struct UnitAssets {
    ShpFile shp;
    Palette pal;
    std::string image;
    int speed = 0;  // rulesmd.ini [unit] Speed= (drives walk pace; see build_world)
};

// Resolves unit_id's Image= (e.g. "E1" -> "GI") from rulesmd.ini. rulesmd.ini
// is not always a top-level ra2md.mix entry in a stock install -- it can be
// nested inside one of a handful of mixes depending on layout. Mirrors the
// exact fallback chain proven against the real install in
// tests/test_rules.cpp's real-asset test.
// Resolved rulesmd.ini fields the engine needs for one unit.
struct UnitRules {
    std::string image;
    int speed = 0;
};

UnitRules resolve_unit_rules(const std::string& dir, const std::string& unit_id) {
    MixFile top = MixFile::open(ra2assets::read_file(dir + "ra2md.mix"));

    std::vector<uint8_t> rules_bytes;
    if (const MixEntry* e = top.find(mix_id("rulesmd.ini"))) {
        rules_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",  "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix",  "conqmd.mix",
        };
        for (const char* name : candidates) {
            try {
                MixFile nested = top.open_nested(name);
                if (const MixEntry* e2 = nested.find(mix_id("rulesmd.ini"))) {
                    rules_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (rules_bytes.empty())
        throw std::runtime_error("rulesmd.ini not found in ra2md.mix (top level or nested)");

    Ini ini = Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    Rules rules = Rules::load(ini);
    return UnitRules{rules.image(unit_id), rules.speed(unit_id)};
}

// Loads the unit's SHP + palette for unit_id (e.g. "E1" -> image "GI" ->
// GI.SHP). Asset chain (Phase-1 proven, see task-10-report.md):
//   image name : ra2md.mix -> ... -> rulesmd.ini, [E1] Image=GI
//   sprite     : ra2.mix -> conquer.mix -> gi.shp   (lowercased image name)
//   palette    : ra2.mix -> cache.mix   -> unittem.pal (temperate UNIT
//                palette -- distinct from isotem.pal, the TERRAIN palette
//                used by load_assets()/draw_terrain)
// `ra2` is the already-open ra2.mix shared with load_assets() (opened once by
// the mode entry points -- previously this function slurped the 268 MB file a
// second time).
UnitAssets load_unit_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                            const std::string& unit_id) {
    const UnitRules ur = resolve_unit_rules(dir, unit_id);
    const std::string image = ur.image;

    MixFile conquer = ra2.open_nested("conquer.mix");
    std::string shp_name = image + ".shp";
    const MixEntry* se = conquer.find(shp_name);
    if (!se) {
        // conquer.mix's stock entry names are lowercase (e.g. "gi.shp"); try
        // as-is first (in case a mod/layout differs), then lowercased.
        std::string lower = shp_name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        se = conquer.find(lower);
        shp_name = lower;
    }
    if (!se)
        throw std::runtime_error("unit SHP not found in ra2.mix/conquer.mix: " + image + ".shp");
    ShpFile shp = ShpFile::load(conquer.read(*se));

    MixFile cache = ra2.open_nested("cache.mix");
    const MixEntry* pe = cache.find(std::string("unittem.pal"));
    if (!pe) throw std::runtime_error("unittem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    Palette pal = Palette::load(pal_raw.data(), pal_raw.size());

    std::cerr << "[unit] id=" << unit_id << " image=" << image << " shp=" << shp_name
              << " frames=" << shp.count() << " canvas=" << shp.width() << "x" << shp.height()
              << " speed=" << ur.speed << "\n";

    return UnitAssets{std::move(shp), pal, image, ur.speed};
}

// Constructs the World (places the unit on a passable Clear cell -- see
// world.hpp/pick_start_cell) and tags its Unit with the resolved image.
// Walk pace tuning: iso cells/sec = rules Speed * this. rulesmd Speed for E1
// is 4, so 4 * 0.75 = 3 cells/sec -- fast enough to visibly stride between
// tiles, slow enough to read as walking (not teleporting) and to cross a
// ~25-cell demo path in well under the 15s cap. Falls back to World's own
// default if Speed is missing (0).
constexpr double kCellsPerSecPerSpeed = 0.75;

// Spawn `count` GI units on distinct walkable cells spiralling out from the
// map's start cell (pick_start_cell). count==1 reproduces the old single-unit
// slice (used by the single-unit demos); the interactive/screenshot/group
// modes pass a small squad.
ra2engine::World build_world(const Assets& a, const UnitAssets& ua, int count) {
    ra2engine::World world(a.map);
    if (ua.speed > 0) world.set_move_speed(ua.speed * kCellsPerSecPerSpeed);

    const ra2assets::MapCell* c = ra2engine::pick_start_cell(a.map);
    int placed = 0;
    for (int r = 0; r <= 12 && placed < count; ++r)
        for (int dy = -r; dy <= r && placed < count; ++dy)
            for (int dx = -r; dx <= r && placed < count; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // ring border only
                const int x = c->x + dx, y = c->y + dy;
                if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                    world.add_unit(x, y, ua.image, 0);
                    ++placed;
                }
            }
    std::cerr << "[world] spawned " << world.units().size() << " unit(s) near (" << c->x << ","
              << c->y << ") grid=" << world.grid_w() << "x" << world.grid_h()
              << " move_cps=" << world.move_speed() << "\n";
    return world;
}

// Draws one full frame of the world into the current render target (does NOT
// clear or present): terrain, the ordered path (if any), the selection
// diamond (if selected), then the unit sprite on top. Shared by every mode
// so they stay visually consistent.
void render_world(SDL_Renderer* ren, ra2engine::TextureCache& tc, Assets& a,
                  UnitAssets& ua, ra2engine::World& world, const ra2engine::Camera& cam,
                  int W, int H, const ra2engine::Input* input = nullptr) {
    ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H);
    const std::vector<ra2engine::Unit>& us = world.units();
    // Paths + selection markers first (under the sprites).
    for (const ra2engine::Unit& u : us) {
        ra2engine::draw_path(ren, u, cam, W, H);
        if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H);
    }
    // Sprites back-to-front so nearer units overlap farther ones.
    for (int idx : ra2engine::unit_draw_order(us))
        ra2engine::draw_unit(ren, tc, us[idx], ua.shp, ua.pal, cam, W, H);
    // Live selection box on top (interactive only).
    if (input) {
        int x0, y0, x1, y1;
        if (input->selection_box(x0, y0, x1, y1))
            ra2engine::draw_selection_box(ren, x0, y0, x1, y1);
    }
}

// Read the framebuffer back and write it to a PNG. Throws on failure.
void capture_png(SDL_Renderer* ren, const std::string& out_path, int W, int H) {
    std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4);
    if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), W * 4) != 0)
        throw std::runtime_error(std::string("RenderReadPixels: ") + SDL_GetError());
    if (!stbi_write_png(out_path.c_str(), W, H, 4, px.data(), W * 4))
        throw std::runtime_error("stbi_write_png failed: " + out_path);
}

// Center the camera so the map's projected bounding-box center lands in the
// middle of the view (guarantees a populated region is shown).
ra2engine::Camera centered_camera(const MapFile& map, int view_w, int view_h) {
    ra2engine::Camera cam;
    int mnx, mny, mxx, mxy;
    if (ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy)) {
        cam.x = view_w / 2 - (mnx + mxx) / 2;
        cam.y = view_h / 2 - (mny + mxy) / 2;
    }
    return cam;
}

int run_screenshot(const std::string& out_path) {
    const int W = 1280, H = 720;
    // ra2.mix opened ONCE and shared by both loaders; the buffer stays
    // resident for the run (accepted tradeoff, see the load_assets comment).
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, 5);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (screenshot)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    // Scope the TextureCache so all SDL_Texture objects are destroyed BEFORE
    // the renderer / SDL_Quit below (destroying textures after SDL teardown is
    // an access violation).
    {
        ra2engine::TextureCache tc(ren);
        ra2engine::Camera cam = centered_camera(a.map, W, H);

        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, tc, a, ua, world, cam, W, H);
        capture_png(ren, out_path, W, H);

        std::cerr << "[screenshot] wrote " << out_path << " (" << W << "x" << H << ")\n";
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

int run_interactive() {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, 5);
    ra2engine::Platform platform = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(platform.renderer());

    ra2engine::Camera cam = centered_camera(a.map, platform.width(), platform.height());
    ra2engine::Input input(world, cam, platform.width(), platform.height());
    const int kPan = 40;

    Uint32 prev = SDL_GetTicks();
    while (!input.wants_quit()) {
        ra2engine::InputEvent ev;
        while (platform.poll(ev)) input.handle(ev);

        // Camera pan: arrow keys + screen-edge scroll (left-drag now box-selects).
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int pan_dx = 0, pan_dy = 0;
        if (ks[SDL_SCANCODE_LEFT]) pan_dx += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) pan_dx -= kPan;
        if (ks[SDL_SCANCODE_UP]) pan_dy += kPan;
        if (ks[SDL_SCANCODE_DOWN]) pan_dy -= kPan;
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        const int kEdge = 24;  // px band at the viewport edge that scrolls
        if (mx <= kEdge) pan_dx += kPan;
        else if (mx >= platform.width() - kEdge) pan_dx -= kPan;
        if (my <= kEdge) pan_dy += kPan;
        else if (my >= platform.height() - kEdge) pan_dy -= kPan;
        if (pan_dx || pan_dy) input.pan(pan_dx, pan_dy);

        const Uint32 now = SDL_GetTicks();
        const double dt = (now - prev) / 1000.0;
        prev = now;
        world.step(dt);

        platform.begin_frame();
        render_world(static_cast<SDL_Renderer*>(platform.renderer()), tc, a, ua, world, cam,
                     platform.width(), platform.height(), &input);
        platform.present();
        SDL_Delay(16);  // ~60fps
    }
    return 0;
}

// Non-interactive verification of the whole select -> path -> walk pipeline:
// programmatically select the unit, order a move to (goal_x, goal_y), then
// run the sim (fixed 60Hz dt) until arrival or a 15s cap, rendering each
// frame. Captures a START screenshot (unit at origin, selected, path drawn)
// and an END screenshot (unit arrived), and prints start/target/path-len/
// final. Screenshots land in the scratchpad, never the repo.
int run_demo_move(int goal_x, int goal_y) {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "task12_start.png";
    const std::string end_png = scratch + "task12_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, 1);

    const int start_x = world.units()[0].cell_x;
    const int start_y = world.units()[0].cell_y;

    // SELECT, then order the move.
    world.units()[0].selected = true;
    world.order_move_selected(goal_x, goal_y);
    const std::size_t path_len = world.units()[0].path.size();

    std::cerr << "[demo] start=(" << start_x << "," << start_y << ") target=(" << goal_x << ","
              << goal_y << ") path_len=" << path_len << " goal_walkable="
              << (world.walkable(goal_x, goal_y) ? "yes" : "no") << "\n";
    if (path_len == 0) {
        std::cerr << "[demo] no path to target (unreachable / off-grid / == start); aborting.\n";
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-move)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    int final_x = start_x, final_y = start_y;
    double sim_time = 0.0;
    {
        ra2engine::TextureCache tc(ren);
        // Center the (fixed) demo camera on the MIDPOINT of start->goal so
        // both endpoints stay comfortably in frame -- the map-centered camera
        // used elsewhere can leave a distant destination off-screen.
        ra2engine::Camera cam;
        {
            const ra2engine::ScreenPt mid = ra2engine::cell_to_screen(
                (start_x + goal_x) / 2, (start_y + goal_y) / 2, 0, 60, 30, 15);
            cam.x = W / 2 - mid.x;
            cam.y = H / 2 - mid.y;
        }

        auto draw = [&]() {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, tc, a, ua, world, cam, W, H);
        };

        // START frame: unit at origin, selected, path visible.
        draw();
        capture_png(ren, start_png, W, H);

        // Sim loop: fixed 60Hz steps until arrival or 15s of sim time.
        const double dt = 1.0 / 60.0;
        const double kMaxSim = 15.0;
        while (world.units()[0].moving() && sim_time < kMaxSim) {
            world.step(dt);
            sim_time += dt;
            draw();  // render every frame (exercises the render path)
        }

        final_x = world.units()[0].cell_x;
        final_y = world.units()[0].cell_y;

        // END frame: unit arrived (or stopped at the cap).
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cerr << "[demo] finished sim_time=" << sim_time << "s final=(" << final_x << ","
              << final_y << ") arrived=" << (!world.units()[0].moving() ? "yes" : "no (hit cap)")
              << "\n";
    std::cout << "start_cell=(" << start_x << "," << start_y << ") target=(" << goal_x << ","
              << goal_y << ") path_len=" << path_len << " final_cell=(" << final_x << ","
              << final_y << ")\n";
    std::cout << "start_png=" << start_png << "\n";
    std::cout << "end_png=" << end_png << "\n";
    return 0;
}

// Headless (no window/rendering) verification of the Task 12 review's
// Finding 1 fix: order a move to (x1,y1), let the sim run PARTWAY (not to
// completion -- so fx/fy is genuinely fractional/mid-walk), then retarget to
// a DIFFERENT cell (x2,y2) exactly as a live reorder would, and confirm (a)
// the retarget did NOT snap fx/fy backward to the last committed cell, and
// (b) the unit still ends up at the SECOND target, not stuck partway or
// stranded. Prints the numbers; exits 0 only if both checks hold.
int run_demo_retarget(int x1, int y1, int x2, int y2) {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, 1);

    const int start_x = world.units()[0].cell_x;
    const int start_y = world.units()[0].cell_y;

    world.units()[0].selected = true;
    world.order_move_selected(x1, y1);
    const std::size_t len1 = world.units()[0].path.size();
    std::cerr << "[demo-retarget] start=(" << start_x << "," << start_y << ") first_target=("
              << x1 << "," << y1 << ") path_len1=" << len1 << "\n";
    if (len1 == 0) {
        std::cerr << "[demo-retarget] no path to first target; aborting.\n";
        return 2;
    }

    // Step partway into the first order -- long enough to be well off the
    // starting cell, short enough (checked below) to still be mid-walk.
    const double dt = 1.0 / 60.0;
    double sim_time = 0.0;
    const double kRetargetAt = 1.5;  // seconds
    while (world.units()[0].moving() && sim_time < kRetargetAt) {
        world.step(dt);
        sim_time += dt;
    }
    const bool was_moving_mid = world.units()[0].moving();
    const double fx_mid = world.units()[0].fx;
    const double fy_mid = world.units()[0].fy;
    const int cell_mid_x = world.units()[0].cell_x;
    const int cell_mid_y = world.units()[0].cell_y;
    const bool genuinely_fractional = (fx_mid != cell_mid_x || fy_mid != cell_mid_y);

    // Retarget mid-walk.
    world.order_move_selected(x2, y2);
    const std::size_t len2 = world.units()[0].path.size();
    const bool no_backward_snap = (world.units()[0].fx == fx_mid && world.units()[0].fy == fy_mid);
    std::cerr << "[demo-retarget] retargeted at t=" << sim_time << "s (moving=" << was_moving_mid
              << " fractional=" << genuinely_fractional << ") mid_pos=(" << fx_mid << ","
              << fy_mid << ") second_target=(" << x2 << "," << y2 << ") path_len2=" << len2
              << " no_backward_snap=" << no_backward_snap << "\n";
    if (len2 == 0) {
        std::cerr << "[demo-retarget] no path to second target; aborting.\n";
        return 3;
    }

    // Run to arrival or a generous cap.
    const double kMaxSim = 30.0;
    while (world.units()[0].moving() && sim_time < kMaxSim) {
        world.step(dt);
        sim_time += dt;
    }
    const int final_x = world.units()[0].cell_x;
    const int final_y = world.units()[0].cell_y;
    const bool arrived_second = (final_x == x2 && final_y == y2);

    std::cout << "start=(" << start_x << "," << start_y << ") first_target=(" << x1 << "," << y1
              << ") second_target=(" << x2 << "," << y2 << ") mid_pos=(" << fx_mid << "," << fy_mid
              << ") no_backward_snap=" << (no_backward_snap ? "yes" : "no") << " final_cell=("
              << final_x << "," << final_y
              << ") arrived_at_second=" << (arrived_second ? "yes" : "no")
              << " sim_time=" << sim_time << "s\n";

    return (was_moving_mid && genuinely_fractional && no_backward_snap && arrived_second) ? 0 : 1;
}

// Non-interactive verification of the multi-unit pipeline: spawn `n` GI units,
// select them all, group-move to (gx, gy), run the sim to arrival (or a cap),
// and write before/after screenshots. Asserts the no-overlap invariant every
// step and prints per-unit arrival cells. Screenshots -> scratchpad only.
int run_demo_group(int n, int gx, int gy) {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_group_start.png";
    const std::string end_png = scratch + "demo_group_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2, "E1");
    ra2engine::World world = build_world(a, ua, n);
    for (auto& u : world.units()) u.selected = true;
    world.order_move_selected(gx, gy);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-group)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    bool overlap_seen = false;
    double sim_time = 0.0;
    {
        ra2engine::TextureCache tc(ren);
        ra2engine::Camera cam;
        {
            const ra2engine::ScreenPt mid = ra2engine::cell_to_screen(
                gx, gy, 0, 60, 30, 15);
            cam.x = W / 2 - mid.x;
            cam.y = H / 2 - mid.y;
        }
        auto draw = [&]() {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, tc, a, ua, world, cam, W, H);
        };
        auto any_moving = [&]() {
            for (const auto& u : world.units()) if (u.moving()) return true;
            return false;
        };
        auto check_overlap = [&]() {
            const auto& us = world.units();
            for (size_t i = 0; i < us.size(); ++i)
                for (size_t j = i + 1; j < us.size(); ++j)
                    if (us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y)
                        overlap_seen = true;
        };

        draw();
        capture_png(ren, start_png, W, H);

        const double dt = 1.0 / 60.0;
        const double kMaxSim = 30.0;
        while (any_moving() && sim_time < kMaxSim) {
            world.step(dt);
            check_overlap();
            sim_time += dt;
            draw();
        }
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cout << "units=" << world.units().size() << " target=(" << gx << "," << gy
              << ") sim_time=" << sim_time << "s overlap_seen=" << (overlap_seen ? "YES" : "no")
              << "\n";
    for (size_t i = 0; i < world.units().size(); ++i)
        std::cout << "  unit " << i << " final=(" << world.units()[i].cell_x << ","
                  << world.units()[i].cell_y << ")\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    return overlap_seen ? 1 : 0;  // fail if the invariant was ever violated
}

}  // namespace

int main(int argc, char** argv) {
    try {
#ifdef __ANDROID__
        // Android launches straight into the interactive engine: there is no
        // CLI, so the desktop-only verification modes (--screenshot/--demo-*)
        // are unreachable here. Touch drives select/move (see platform.cpp
        // SDL_FINGER* handling + input.cpp).
        (void)argc;
        (void)argv;
        return run_interactive();
#else
        std::string screenshot;
        bool demo_move = false;
        int demo_x = 0, demo_y = 0;
        bool demo_retarget = false;
        int retarget_x1 = 0, retarget_y1 = 0, retarget_x2 = 0, retarget_y2 = 0;
        bool demo_group = false;
        int group_n = 5, group_x = 0, group_y = 0;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--screenshot" && i + 1 < argc) {
                screenshot = argv[++i];
            } else if (arg == "--demo-move" && i + 2 < argc) {
                demo_move = true;
                demo_x = std::atoi(argv[++i]);
                demo_y = std::atoi(argv[++i]);
            } else if (arg == "--demo-retarget" && i + 4 < argc) {
                demo_retarget = true;
                retarget_x1 = std::atoi(argv[++i]);
                retarget_y1 = std::atoi(argv[++i]);
                retarget_x2 = std::atoi(argv[++i]);
                retarget_y2 = std::atoi(argv[++i]);
            } else if (arg == "--demo-group" && i + 3 < argc) {
                demo_group = true;
                group_n = std::atoi(argv[++i]);
                group_x = std::atoi(argv[++i]);
                group_y = std::atoi(argv[++i]);
            }
        }
        if (demo_group) return run_demo_group(group_n, group_x, group_y);
        if (demo_retarget)
            return run_demo_retarget(retarget_x1, retarget_y1, retarget_x2, retarget_y2);
        if (demo_move) return run_demo_move(demo_x, demo_y);
        if (!screenshot.empty()) return run_screenshot(screenshot);
        return run_interactive();
#endif
    } catch (const std::exception& ex) {
        // SDL_Log reaches Android logcat (tag "SDL"); std::cerr does not. Emit
        // both so the failure reason is visible on-device and on desktop
        // instead of the app just vanishing.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fatal: %s", ex.what());
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}
