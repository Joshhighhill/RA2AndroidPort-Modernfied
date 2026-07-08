#include "iso_renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

#include "iso_project.hpp"
#include "texture_cache.hpp"

namespace ra2engine {

// RA2 temperate tile geometry (see iso_project.hpp / TmpFile). tile_w/tile_h
// are the diamond bounding rect; level_px is the on-screen rise per height
// step.
static constexpr int kTileW = 60;
static constexpr int kTileH = 30;
static constexpr int kLevelPx = 15;

Theater::Theater(ra2assets::MixFile isotile_mix, const std::string& control_ini_text,
                 std::string ext)
    : mix_(std::move(isotile_mix)), ext_(std::move(ext)) {
    ra2assets::Ini ini = ra2assets::Ini::parse(control_ini_text);
    // Walk [TileSet0000], [TileSet0001], ... in order; assign a running global
    // index to each template 1..TilesInSet. Sets with TilesInSet=0 contribute
    // nothing (matches OpenRA/XCC). Stop at the first missing section.
    for (int set_idx = 0;; ++set_idx) {
        char sec[32];
        std::snprintf(sec, sizeof sec, "TileSet%04d", set_idx);
        if (!ini.has(sec)) break;
        const int tiles = ini.get_int(sec, "TilesInSet", 1);
        const std::string file = ini.get(sec, "FileName", "");
        for (int i = 1; i <= tiles; ++i) slots_.push_back(Slot{file, i});
    }
}

std::string Theater::entry_name(const Slot& s) const {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s%02d.%s", s.file.c_str(), s.local, ext_.c_str());
    return buf;
}

std::string Theater::filename_for_tile(uint32_t global_index) const {
    if (global_index >= slots_.size()) return std::string();
    return entry_name(slots_[global_index]);
}

const ra2assets::TmpFile* Theater::tmp_for_tile(uint32_t global_index) {
    if (global_index >= slots_.size()) return nullptr;
    const std::string name = entry_name(slots_[global_index]);

    auto hit = cache_.find(name);
    if (hit != cache_.end()) return &hit->second;
    if (misses_.count(name)) return nullptr;

    const ra2assets::MixEntry* e = mix_.find(name);
    if (!e) {
        misses_.emplace(name, true);
        return nullptr;
    }
    try {
        ra2assets::TmpFile t = ra2assets::TmpFile::load(mix_.read(*e));
        auto ins = cache_.emplace(name, std::move(t));
        return &ins.first->second;
    } catch (const std::exception&) {
        misses_.emplace(name, true);
        return nullptr;
    }
}

bool terrain_screen_bounds(const ra2assets::MapFile& map, int& min_x, int& min_y,
                           int& max_x, int& max_y) {
    const auto& cells = map.cells();
    if (cells.empty()) return false;
    int mnx = 1 << 30, mny = 1 << 30, mxx = -(1 << 30), mxy = -(1 << 30);
    for (const auto& c : cells) {
        const ScreenPt p =
            cell_to_screen(c.x, c.y, static_cast<int>(c.level), kTileW, kTileH, kLevelPx);
        mnx = std::min(mnx, p.x);
        mny = std::min(mny, p.y);
        mxx = std::max(mxx, p.x + kTileW);
        mxy = std::max(mxy, p.y + kTileH);
    }
    min_x = mnx;
    min_y = mny;
    max_x = mxx;
    max_y = mxy;
    return true;
}

int draw_terrain(void* sdl_renderer, TextureCache& tc,
                 const ra2assets::MapFile& map, Theater& theater,
                 const ra2assets::Palette& pal, const Camera& cam, int view_w,
                 int view_h) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);

    // Back-to-front painter order. Cells with a smaller (x+y) project higher
    // on screen (smaller py) and are drawn first, so nearer cells overwrite
    // farther ones where diamonds/cliffs overlap. Secondary key: cell.x, so a
    // given depth row is drawn left-to-right (stable, avoids z-fighting for
    // equal-depth raised tiles). We sort indices to avoid copying MapCell.
    const auto& cells = map.cells();
    std::vector<uint32_t> order(cells.size());
    for (uint32_t i = 0; i < cells.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const auto& ca = cells[a];
        const auto& cb = cells[b];
        const int da = ca.x + ca.y;
        const int db = cb.x + cb.y;
        if (da != db) return da < db;
        if (ca.level != cb.level) return ca.level < cb.level;
        return ca.x < cb.x;
    });

    int drawn = 0;
    for (uint32_t idx : order) {
        const ra2assets::MapCell& c = cells[idx];
        const ra2assets::TmpFile* tmp = theater.tmp_for_tile(c.tile);
        if (!tmp) continue;
        if (c.subtile >= tmp->count()) continue;

        const ScreenPt p =
            cell_to_screen(c.x, c.y, static_cast<int>(c.level), kTileW, kTileH, kLevelPx);
        SDL_Rect dst;
        dst.x = p.x + cam.x;
        dst.y = p.y + cam.y;
        dst.w = tmp->tile_w();
        dst.h = tmp->tile_h();

        // Cull tiles fully off-screen.
        if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= view_w || dst.y >= view_h)
            continue;

        // Key the texture cache by template file + subtile: identical pixels
        // across every cell that references it. Decode only on a cache miss
        // (a real map references the same few hundred tiles thousands of
        // times, so this keeps steady-state frames allocation-free).
        char key[80];
        std::snprintf(key, sizeof key, "%s#%u", theater.filename_for_tile(c.tile).c_str(),
                      static_cast<unsigned>(c.subtile));

        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            ra2assets::TileImage img = tmp->decode(c.subtile);
            tex = static_cast<SDL_Texture*>(
                tc.get(key, img.indices.data(), img.w, img.h, pal));
        }
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        ++drawn;
    }
    return drawn;
}

}  // namespace ra2engine
