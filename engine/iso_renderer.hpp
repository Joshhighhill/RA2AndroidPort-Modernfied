#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ra2assets/ini.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/shp.hpp"     // ShpFile (draw_unit)
#include "ra2assets/theater.hpp"  // TmpFile / TileImage
#include "world.hpp"              // Unit

namespace ra2engine {

class TextureCache;  // fwd (engine/texture_cache.hpp)

// Resolves a map cell's GLOBAL tile index -> a theater `.tmp` template file +
// the sub-tile within it, then decodes it. This is the TS/RA2 tile-index
// mapping: the theater control INI (temperat.ini / snow.ini / urban.ini,
// found in ra2.mix/local.mix) lists ordered `[TileSetNNNN]` sections, each
// with `FileName=` and `TilesInSet=`. Walking the sets in order and, within
// each, templates 1..TilesInSet, assigns a running GLOBAL index to every
// template; template T of set S resolves to the mix entry
// "<FileName><NN>.<ext>" (NN = 1-based within-set index, zero-padded to 2).
// A map cell's `tile` field is that global index; its `subtile` field is the
// tile within that template's cblocks_x*cblocks_y grid. Cross-checked against
// OpenRA's ImportLegacyTilesetCommand and XCC's itc_ini_reader.
//
// PURE ra2assets (no SDL) -- it only opens mix entries and decodes TMPs.
class Theater {
public:
    // isotile_mix : the theater tile mix (e.g. isotemp.mix) that holds the
    //               "<file><NN>.<ext>" template entries. Taken by value (owns
    //               the archive bytes); pass with std::move.
    // control_ini_text : the theater control INI text (e.g. temperat.ini).
    // ext : tile file extension WITHOUT the dot, e.g. "tem".
    Theater(ra2assets::MixFile isotile_mix, const std::string& control_ini_text,
            std::string ext);

    // Number of resolvable global templates (sum of TilesInSet over all sets).
    size_t template_count() const { return slots_.size(); }

    // Resolve a global tile index to its decoded TMP, loading+caching lazily.
    // Returns nullptr if the index is out of range or the mix entry is absent.
    const ra2assets::TmpFile* tmp_for_tile(uint32_t global_index);

    // Diagnostic: the "<file><NN>.<ext>" name a global index maps to
    // (empty string if out of range).
    std::string filename_for_tile(uint32_t global_index) const;

private:
    struct Slot {
        std::string file;  // FileName= (lower/verbatim from INI)
        int local;         // 1-based index within its tile set
    };

    std::string entry_name(const Slot& s) const;

    ra2assets::MixFile mix_;
    std::string ext_;
    std::vector<Slot> slots_;  // global index -> slot
    // filename -> decoded TmpFile (absent entries stored as a nullptr sentinel
    // via `misses_`); keeps repeated cells cheap.
    std::unordered_map<std::string, ra2assets::TmpFile> cache_;
    std::unordered_map<std::string, bool> misses_;  // name -> true if known absent
};

// Camera pan: a screen-pixel offset added to every projected tile so the
// terrain (whose cell (0,0) projects to screen (0,0)) can be scrolled into
// view. Larger x scrolls the map right, larger y scrolls it down.
struct Camera {
    int x = 0;
    int y = 0;
};

// Screen bounding box (in camera-local projected pixels, level included) of
// every cell in `map`, using the RA2 tile geometry (60x30, level step 15).
// Useful for centering the camera on a populated region. Returns false (and
// leaves outputs untouched) if the map has no cells.
bool terrain_screen_bounds(const ra2assets::MapFile& map, int& min_x, int& min_y,
                           int& max_x, int& max_y);

// Draw the map's terrain, back-to-front (painter's order: cells sorted by
// cell.x+cell.y ascending == increasing screen py, so nearer/lower diamonds
// overwrite farther ones). Each cell is projected with
// cell_to_screen(x, y, level, 60, 30, 15), offset by the camera, its tile
// decoded via `theater`, palettized+uploaded via `tc`, and blitted with
// SDL_RenderCopy at (screen + camera, tile_w x tile_h). Cells whose rect is
// fully outside [0,view_w) x [0,view_h) are skipped. Returns the number of
// tiles actually drawn (for diagnostics).
int draw_terrain(void* sdl_renderer, TextureCache& tc,
                 const ra2assets::MapFile& map, Theater& theater,
                 const ra2assets::Palette& pal, const Camera& cam, int view_w,
                 int view_h);

// Draw a unit's current SHP frame on top of the terrain (call AFTER
// draw_terrain so the sprite is not painted over). The unit's interpolated
// position is projected with cell_to_screen(fx, fy, level, 60, 30, 15) +
// camera to get the tile's on-screen position; the SHP frame is then anchored so
// the sprite's fixed "ground point" -- the CENTER of the SHP's global
// cx*cy canvas (`shp.width()/2`, `shp.height()/2`), the standard TS/RA2
// sprite-canvas convention -- lands on the tile's screen-space center
// (tile_w/2, tile_h/2), which is the isometric ground reference for a
// diamond tile. Using the canvas center (not the current frame's own w/h,
// which varies per facing/animation frame) keeps the character's feet
// planted at the same spot across every frame. See iso_renderer.cpp for the
// exact formula and its empirical validation against gi.shp (Task 10
// report). The frame is palettized/uploaded via `tc`, keyed
// "unit:<image>:<frame>". Returns false (no-op, nothing drawn) if
// unit.frame is out of range or the sprite rect is fully off-screen.
bool draw_unit(void* sdl_renderer, TextureCache& tc, const Unit& unit,
               ra2assets::ShpFile& shp, const ra2assets::Palette& pal,
               const Camera& cam, int view_w, int view_h);

// Draw a green isometric-diamond selection marker on the unit's tile
// (at its interpolated fx/fy position). No-op if fully off-screen. Call
// AFTER draw_terrain (so it's not painted over) and typically BEFORE
// draw_unit so the sprite sits on top of the marker.
void draw_selection(void* sdl_renderer, const Unit& unit, const Camera& cam, int view_w,
                    int view_h);

// Draw the unit's remaining ordered path as a green poly-line + node dots
// connecting the tile centers of its path cells (for visual feedback and
// demo verification). No-op if the unit has no active path.
void draw_path(void* sdl_renderer, const Unit& unit, const Camera& cam, int view_w,
               int view_h);

// Draw the live left-drag selection rectangle outline (green) in window-pixel
// space. Coordinates are the raw drag endpoints (any order). HUD overlay: call
// LAST, after terrain + units.
void draw_selection_box(void* sdl_renderer, int x0, int y0, int x1, int y1);

}  // namespace ra2engine
