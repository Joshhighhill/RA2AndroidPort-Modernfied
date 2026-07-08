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
#include <fstream>
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

#include "ra2assets/ini.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/rules.hpp"
#include "ra2assets/shp.hpp"

using namespace ra2assets;

namespace {

std::vector<uint8_t> slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open file: " + p);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

std::string env_install_dir() {
    const char* e = std::getenv("RA2_INSTALL_DIR");
    std::string dir = e ? e : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    return dir;
}

// Everything the renderer needs, loaded once from the mix chain.
struct Assets {
    MapFile map;
    Palette pal;
    ra2engine::Theater theater;
};

Assets load_assets() {
    const std::string dir = env_install_dir();

    // Map: read the temperate reference map INI out of mapsmd03.mix.
    MixFile maps = MixFile::open(slurp(dir + "mapsmd03.mix"));
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

    // ra2.mix holds the nested theater/config/palette mixes.
    MixFile ra2 = MixFile::open(slurp(dir + "ra2.mix"));

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
    MixFile top = MixFile::open(slurp(dir + "ra2md.mix"));

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
UnitAssets load_unit_assets(const std::string& dir, const std::string& unit_id) {
    const UnitRules ur = resolve_unit_rules(dir, unit_id);
    const std::string image = ur.image;

    MixFile ra2 = MixFile::open(slurp(dir + "ra2.mix"));

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

ra2engine::World build_world(const Assets& a, const UnitAssets& ua) {
    ra2engine::World world(a.map);
    world.set_unit_image(ua.image, 0);
    if (ua.speed > 0) world.set_move_speed(ua.speed * kCellsPerSecPerSpeed);
    const ra2engine::Unit& u = world.unit();
    std::cerr << "[unit] placed at cell (" << u.cell_x << "," << u.cell_y
              << ") level=" << u.level << " grid=" << world.grid_w() << "x" << world.grid_h()
              << " move_cps=" << world.move_speed() << "\n";
    return world;
}

// Draws one full frame of the world into the current render target (does NOT
// clear or present): terrain, the ordered path (if any), the selection
// diamond (if selected), then the unit sprite on top. Shared by every mode
// so they stay visually consistent.
void render_world(SDL_Renderer* ren, ra2engine::TextureCache& tc, Assets& a,
                  UnitAssets& ua, ra2engine::World& world, const ra2engine::Camera& cam,
                  int W, int H) {
    ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H);
    const ra2engine::Unit& u = world.unit();
    ra2engine::draw_path(ren, u, cam, W, H);
    if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H);
    ra2engine::draw_unit(ren, tc, u, ua.shp, ua.pal, cam, W, H);
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
    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);

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
    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);
    ra2engine::Platform platform = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(platform.renderer());

    ra2engine::Camera cam = centered_camera(a.map, platform.width(), platform.height());
    ra2engine::Input input(world, cam);
    const int kPan = 40;

    Uint32 prev = SDL_GetTicks();
    while (!input.wants_quit()) {
        ra2engine::InputEvent ev;
        while (platform.poll(ev)) input.handle(ev);

        // Keyboard arrow panning (polled directly; Platform only surfaces Quit
        // for keys).
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        if (ks[SDL_SCANCODE_LEFT]) cam.x += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) cam.x -= kPan;
        if (ks[SDL_SCANCODE_UP]) cam.y += kPan;
        if (ks[SDL_SCANCODE_DOWN]) cam.y -= kPan;

        const Uint32 now = SDL_GetTicks();
        const double dt = (now - prev) / 1000.0;
        prev = now;
        world.step(dt);

        platform.begin_frame();
        render_world(static_cast<SDL_Renderer*>(platform.renderer()), tc, a, ua, world, cam,
                     platform.width(), platform.height());
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

    Assets a = load_assets();
    UnitAssets ua = load_unit_assets(env_install_dir(), "E1");
    ra2engine::World world = build_world(a, ua);

    const int start_x = world.unit().cell_x;
    const int start_y = world.unit().cell_y;

    // SELECT, then order the move.
    world.unit().selected = true;
    const std::size_t path_len = world.order_move(goal_x, goal_y);

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
        while (world.unit().moving() && sim_time < kMaxSim) {
            world.step(dt);
            sim_time += dt;
            draw();  // render every frame (exercises the render path)
        }

        final_x = world.unit().cell_x;
        final_y = world.unit().cell_y;

        // END frame: unit arrived (or stopped at the cap).
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cerr << "[demo] finished sim_time=" << sim_time << "s final=(" << final_x << ","
              << final_y << ") arrived=" << (!world.unit().moving() ? "yes" : "no (hit cap)")
              << "\n";
    std::cout << "start_cell=(" << start_x << "," << start_y << ") target=(" << goal_x << ","
              << goal_y << ") path_len=" << path_len << " final_cell=(" << final_x << ","
              << final_y << ")\n";
    std::cout << "start_png=" << start_png << "\n";
    std::cout << "end_png=" << end_png << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string screenshot;
        bool demo_move = false;
        int demo_x = 0, demo_y = 0;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--screenshot" && i + 1 < argc) {
                screenshot = argv[++i];
            } else if (arg == "--demo-move" && i + 2 < argc) {
                demo_move = true;
                demo_x = std::atoi(argv[++i]);
                demo_y = std::atoi(argv[++i]);
            }
        }
        if (demo_move) return run_demo_move(demo_x, demo_y);
        if (!screenshot.empty()) return run_screenshot(screenshot);
        return run_interactive();
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}
