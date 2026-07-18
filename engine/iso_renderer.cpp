#include "iso_renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// NOTE: terrain_screen_bounds is declared above (iso_renderer.hpp) but
// DEFINED in engine/world.cpp, not here -- this file (iso_renderer.cpp)
// pulls in SDL and is not part of the SDL-free `ra2assets_tests` binary,
// while world.cpp already is. Input needs this pure function to clamp
// camera drag to the map's extent (Task 12 review Finding 2) without
// dragging SDL into the test build; see world.cpp for the implementation
// and rationale.

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

        // Composite the tile's EXTRA image (cliff/slope face) if present.
        // Cliff/slope tiles store their above-ground geometry -- the wall that
        // rises out of the diamond -- in a separate plain rectangle; without
        // it those cells render as transparent holes (window clear color).
        // Placement: the extra image's (x_extra, y_extra) and the base
        // diamond's (x, y) are in the SAME cell-anchor space, so the extra's
        // offset RELATIVE to the diamond we just drew (at `p`) is
        // (x_extra - x, y_extra - y). This matters for multi-cell templates
        // (ramps/double slopes) whose higher subtiles carry a non-zero base
        // (x, y): the map already places each cell at its own screen position,
        // so we must measure the extra from THIS subtile's diamond, not the
        // template origin. Cross-checked against XCC Ctmp_ts_file::draw and
        // OpenRA TmpTSLoader (extraX -= (u-v)*w/2 == x_extra - base x).
        if (tmp->has_extra(c.subtile)) {
            const int ex_off = tmp->extra_x(c.subtile) - tmp->tile_x(c.subtile);
            const int ey_off = tmp->extra_y(c.subtile) - tmp->tile_y(c.subtile);

            char ekey[96];
            std::snprintf(ekey, sizeof ekey, "tmp_extra:%s#%u",
                          theater.filename_for_tile(c.tile).c_str(),
                          static_cast<unsigned>(c.subtile));

            SDL_Rect edst;
            edst.x = p.x + cam.x + ex_off;
            edst.y = p.y + cam.y + ey_off;

            SDL_Texture* etex = static_cast<SDL_Texture*>(tc.peek(ekey));
            if (!etex) {
                ra2assets::TileImage eimg = tmp->decode_extra(c.subtile);
                edst.w = eimg.w;
                edst.h = eimg.h;
                etex = static_cast<SDL_Texture*>(
                    tc.get(ekey, eimg.indices.data(), eimg.w, eimg.h, pal));
            } else {
                // Cached: recover the dimensions from the texture itself.
                SDL_QueryTexture(etex, nullptr, nullptr, &edst.w, &edst.h);
            }

            if (!(edst.x + edst.w <= 0 || edst.y + edst.h <= 0 || edst.x >= view_w ||
                  edst.y >= view_h))
                SDL_RenderCopy(renderer, etex, nullptr, &edst);
        }
    }
    return drawn;
}

bool draw_unit(void* sdl_renderer, TextureCache& tc, const Unit& unit,
              ra2assets::ShpFile& shp, const ra2assets::Palette& pal,
              const Camera& cam, int view_w, int view_h) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    if (unit.frame < 0 || unit.frame >= shp.count()) return false;

    // Project the unit's CONTINUOUS iso-cell position (fx, fy) -- not the
    // snapped integer cell -- so it renders smoothly between cells while
    // walking. cell_to_screen is linear in (x, y), so the same shear applies
    // to fractional coordinates (see iso_project.hpp). Idle units have
    // fx/fy == cell_x/cell_y, so this is identical to the integer path.
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    const int tile_screen_x =
        static_cast<int>(std::lround((unit.fx - unit.fy) * hw)) + cam.x;
    const int tile_screen_y =
        static_cast<int>(std::lround((unit.fx + unit.fy) * hh - unit.level * kLevelPx)) +
        cam.y;

    const ra2assets::ShpFile::Frame f = shp.frame_info(unit.frame);

    // Anchor math (derived + verified against the real gi.shp, not just the
    // brief's starting heuristic -- see task-10-report.md): TS/RA2 SHP
    // sprites are drawn onto a fixed global cx*cy canvas whose CENTER is the
    // sprite's "ground point"; each frame's (x, y, w, h) is only the tight
    // bounding box of that frame's non-transparent pixels *within* that
    // canvas (used to shrink the stored/RLE-encoded data). So the correct
    // anchor uses the canvas center (shp.width()/2, shp.height()/2), NOT the
    // current frame's own w/h -- a per-frame-size anchor would make the feet
    // jitter up/down as the animation cycles through frames of different
    // heights. Verified: gi.shp's canvas is 78x66 (center 39,33); frame 0 is
    // 13x29 at offset (32,7) -- its bottom edge lands 3px BELOW the canvas
    // center, i.e. the character's feet sit almost exactly on the tile's
    // ground point across every frame checked (frame 4, a taller frame,
    // lands 8px below) -- both a couple pixels of slack for the foot/shadow
    // pixels, not floating or sunk.
    const int canvas_cx = shp.width();
    const int canvas_cy = shp.height();
    SDL_Rect dst;
    dst.w = f.w;
    dst.h = f.h;
    dst.x = tile_screen_x + kTileW / 2 - canvas_cx / 2 + f.x;
    dst.y = tile_screen_y + kTileH / 2 - canvas_cy / 2 + f.y;

    // Cull if fully off-screen.
    if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= view_w || dst.y >= view_h)
        return false;

    // Key includes the house so the two houses' recolors (pal_for_house)
    // cache as distinct textures -- otherwise whichever house rendered a
    // given image:frame FIRST would "win" the cache slot and every other
    // house's unit sharing that image:frame would silently render in the
    // wrong color. pal_for_house is only ever called below, on a cache MISS,
    // so steady-state per-frame cost is unchanged (Task 7).
    char key[80];
    std::snprintf(key, sizeof key, "unit:%s:%d:h%d", unit.image.c_str(), unit.frame, unit.house);
    SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
    if (!tex) {
        std::vector<uint8_t> idx = shp.decode_indexed(unit.frame);
        const ra2assets::Palette house_pal = pal_for_house(pal, unit.house);
        tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, house_pal));
    }
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    return true;
}

bool draw_voxel_sprite(void* sdl_renderer, TextureCache& tc, const std::string& key,
                       const VoxelSprite& s, double fx, double fy, int level,
                       const Camera& cam, int view_w, int view_h) {
    if (s.w <= 0 || s.h <= 0) return false;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);

    // Same fractional diamond projection as draw_unit: project the continuous
    // (fx, fy) ground point, then offset to the tile CENTER (kTileW/2,
    // kTileH/2) -- the iso ground reference. The sprite's model-origin anchor
    // is then placed on that center so hull/turret/barrel (which all share the
    // model origin) stack coherently.
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    const int center_x =
        static_cast<int>(std::lround((fx - fy) * hw)) + cam.x + kTileW / 2;
    const int center_y =
        static_cast<int>(std::lround((fx + fy) * hh - level * kLevelPx)) + cam.y + kTileH / 2;

    SDL_Rect dst;
    dst.x = center_x - s.anchor_x;
    dst.y = center_y - s.anchor_y;
    dst.w = s.w;
    dst.h = s.h;

    if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= view_w || dst.y >= view_h)
        return false;

    SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
    if (!tex) tex = static_cast<SDL_Texture*>(tc.get_rgba(key, s.rgba.data(), s.w, s.h));
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    return true;
}

// Screen-space center (in projected pixels + camera) of the diamond tile at
// continuous iso position (fx, fy, level). The tile's ground reference is the
// center of its bounding rect (tile_w/2, tile_h/2 below the projected
// top-left), matching draw_unit's anchor.
static void tile_center_screen(double fx, double fy, int level, const Camera& cam,
                               int& out_x, int& out_y) {
    const double hw = kTileW / 2.0;
    const double hh = kTileH / 2.0;
    out_x = static_cast<int>(std::lround((fx - fy) * hw)) + cam.x + kTileW / 2;
    out_y = static_cast<int>(std::lround((fx + fy) * hh - level * kLevelPx)) + cam.y +
            kTileH / 2;
}

void draw_selection(void* sdl_renderer, const Unit& unit, const Camera& cam, int view_w,
                    int view_h, int width_px) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    int cx, cy;
    tile_center_screen(unit.fx, unit.fy, unit.level, cam, cx, cy);

    // Diamond size: default one-cell footprint (60x30, 2:1), or scaled to
    // `width_px` for wide voxel sprites (keeping the 2:1 iso ratio).
    const int diamond_w = width_px > 0 ? width_px : kTileW;
    // Cull if the diamond is well off-screen.
    if (cx + diamond_w < 0 || cy + diamond_w < 0 || cx - diamond_w >= view_w ||
        cy - diamond_w >= view_h)
        return;

    const int hw = diamond_w / 2;
    const int hh = diamond_w / 4;
    // A green isometric diamond (the tile footprint) centered on the ground
    // point: top, right, bottom, left corners.
    SDL_Point pts[5] = {
        {cx, cy - hh}, {cx + hw, cy}, {cx, cy + hh}, {cx - hw, cy}, {cx, cy - hh},
    };
    SDL_SetRenderDrawColor(renderer, 40, 240, 40, 255);
    SDL_RenderDrawLines(renderer, pts, 5);
    // Small corner brackets (thicker read) at the two side vertices.
    SDL_RenderDrawLine(renderer, cx - hw, cy, cx - hw + 6, cy - 3);
    SDL_RenderDrawLine(renderer, cx + hw, cy, cx + hw - 6, cy - 3);
}

void draw_path(void* sdl_renderer, const Unit& unit, const Camera& cam, int view_w,
               int view_h) {
    if (unit.path.size() < 2) return;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    (void)view_w;
    (void)view_h;
    // The path color's alpha (200, i.e. ~78% opaque) only takes effect if
    // the renderer's blend mode allows it -- SDL's default draw-color blend
    // mode is SDL_BLENDMODE_NONE, which ignores alpha and draws fully
    // opaque (Task 12 review Finding 4). Save/restore so this draw call
    // doesn't change blend behavior for whatever renders after it.
    SDL_BlendMode prev_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &prev_blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 60, 220, 90, 200);
    // Poly-line connecting the tile centers of the remaining path cells (from
    // the cell currently being walked toward, back one so it starts at the
    // unit) so the ordered route is visible.
    int prev_x = 0, prev_y = 0;
    bool have_prev = false;
    const size_t begin = unit.path_idx > 0 ? unit.path_idx - 1 : 0;
    for (size_t i = begin; i < unit.path.size(); ++i) {
        int px, py;
        tile_center_screen(unit.path[i].x, unit.path[i].y, unit.level, cam, px, py);
        if (have_prev) SDL_RenderDrawLine(renderer, prev_x, prev_y, px, py);
        // A small node marker at each cell.
        SDL_Rect dot{px - 1, py - 1, 3, 3};
        SDL_RenderFillRect(renderer, &dot);
        prev_x = px;
        prev_y = py;
        have_prev = true;
    }
    SDL_SetRenderDrawBlendMode(renderer, prev_blend);
}

void draw_selection_box(void* sdl_renderer, int x0, int y0, int x1, int y1) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    SDL_Rect r;
    r.x = std::min(x0, x1);
    r.y = std::min(y0, y1);
    r.w = std::abs(x1 - x0);
    r.h = std::abs(y1 - y0);
    SDL_SetRenderDrawColor(renderer, 60, 220, 90, 255);
    SDL_RenderDrawRect(renderer, &r);
}

// HP bar geometry: kHpBarUp matches input.cpp's kHitUp (46px -- "most of a
// sprite height up" from the ground point, the same anchor the click hit-box
// and draw_selection use), plus a small gap so the bar clears the selection
// diamond's top corner instead of overlapping it.
static constexpr int kHpBarW = 24;
static constexpr int kHpBarH = 3;
static constexpr int kHpBarUp = 46;
static constexpr int kHpBarGap = 6;

void draw_hp_bar(void* sdl_renderer, const Unit& unit, const Camera& cam, int view_w, int view_h,
                 int width_px) {
    if (unit.max_hp <= 0) return;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);

    // Bar width: default 24px, or the caller-supplied sprite width (voxel
    // hull) so the bar spans a wider vehicle.
    const int bar_w = width_px > 0 ? width_px : kHpBarW;

    int cx, cy;
    tile_center_screen(unit.fx, unit.fy, unit.level, cam, cx, cy);
    const int bar_cy = cy - kHpBarUp - kHpBarGap;

    SDL_Rect bg{cx - bar_w / 2, bar_cy - kHpBarH / 2, bar_w, kHpBarH};
    if (bg.x + bg.w <= 0 || bg.y + bg.h <= 0 || bg.x >= view_w || bg.y >= view_h) return;

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderFillRect(renderer, &bg);

    const int hp = unit.hp < 0 ? 0 : unit.hp;
    int fill_w = static_cast<int>(static_cast<long long>(hp) * bar_w / unit.max_hp);
    if (fill_w > bar_w) fill_w = bar_w;
    if (fill_w < 0) fill_w = 0;
    // clamp: a living unit always shows at least 1px of health
    if (unit.hp > 0 && fill_w < 1) fill_w = 1;
    if (fill_w > 0) {
        SDL_Rect fg{bg.x, bg.y, fill_w, kHpBarH};
        // Integer thresholds (brief-specified): green above 2/3, yellow above
        // 1/3, red at or below 1/3 -- avoids floating-point division.
        if (unit.hp * 3 > unit.max_hp * 2) SDL_SetRenderDrawColor(renderer, 40, 220, 40, 255);
        else if (unit.hp * 3 > unit.max_hp) SDL_SetRenderDrawColor(renderer, 230, 210, 40, 255);
        else SDL_SetRenderDrawColor(renderer, 220, 40, 40, 255);
        SDL_RenderFillRect(renderer, &fg);
    }
}

void draw_cargo_pips(void* sdl_renderer, const Unit& unit, int pips_filled, int pips_max,
                     const Camera& cam, int view_w, int view_h) {
    if (pips_max <= 0) return;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    int cx, cy;
    tile_center_screen(unit.fx, unit.fy, unit.level, cam, cx, cy);
    // One row directly UNDER the HP bar (same anchor math as draw_hp_bar).
    const int row_y = cy - kHpBarUp - kHpBarGap + kHpBarH + 2;
    const int pip_w = 4, pip_h = 3, gap = 2;
    const int total_w = pips_max * pip_w + (pips_max - 1) * gap;
    int x = cx - total_w / 2;
    if (x + total_w <= 0 || row_y + pip_h <= 0 || x >= view_w || row_y >= view_h) return;
    for (int k = 0; k < pips_max; ++k) {
        SDL_Rect r{x, row_y, pip_w, pip_h};
        if (k < pips_filled) SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        else SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderFillRect(renderer, &r);
        x += pip_w + gap;
    }
}

}  // namespace ra2engine
