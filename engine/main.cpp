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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "iso_renderer.hpp"
#include "platform.hpp"
#include "texture_cache.hpp"

#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"

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
        int drawn = ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H);

        std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4);
        if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), W * 4) != 0)
            throw std::runtime_error(std::string("RenderReadPixels: ") + SDL_GetError());

        if (!stbi_write_png(out_path.c_str(), W, H, 4, px.data(), W * 4))
            throw std::runtime_error("stbi_write_png failed: " + out_path);

        std::cerr << "[screenshot] wrote " << out_path << " (" << W << "x" << H
                  << ", tiles drawn " << drawn << ")\n";
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

int run_interactive() {
    Assets a = load_assets();
    ra2engine::Platform platform = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(platform.renderer());

    ra2engine::Camera cam = centered_camera(a.map, platform.width(), platform.height());

    bool running = true;
    bool dragging = false;
    int last_x = 0, last_y = 0;
    const int kPan = 40;

    while (running) {
        ra2engine::InputEvent ev;
        while (platform.poll(ev)) {
            switch (ev.kind) {
                case ra2engine::InputEvent::Quit:
                    running = false;
                    break;
                case ra2engine::InputEvent::PointerDown:
                    dragging = true;
                    last_x = ev.x;
                    last_y = ev.y;
                    break;
                case ra2engine::InputEvent::PointerUp:
                    dragging = false;
                    break;
                case ra2engine::InputEvent::PointerMove:
                    if (dragging) {
                        cam.x += ev.x - last_x;
                        cam.y += ev.y - last_y;
                        last_x = ev.x;
                        last_y = ev.y;
                    }
                    break;
                default:
                    break;
            }
        }
        // Keyboard arrow panning (polled directly; Platform only surfaces Quit
        // for keys).
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        if (ks[SDL_SCANCODE_LEFT]) cam.x += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) cam.x -= kPan;
        if (ks[SDL_SCANCODE_UP]) cam.y += kPan;
        if (ks[SDL_SCANCODE_DOWN]) cam.y -= kPan;

        platform.begin_frame();
        ra2engine::draw_terrain(platform.renderer(), tc, a.map, a.theater, a.pal, cam,
                                platform.width(), platform.height());
        platform.present();
        SDL_Delay(16);  // ~60fps
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string screenshot;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--screenshot" && i + 1 < argc) {
                screenshot = argv[++i];
            }
        }
        if (!screenshot.empty()) return run_screenshot(screenshot);
        return run_interactive();
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}
