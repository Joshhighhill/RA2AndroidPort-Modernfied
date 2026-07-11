#pragma once
// Pure RA2 sidebar geometry + interaction states. All pixel metrics
// [RE: construction.md §11] — the constants below are pre-hunt assumptions
// from the art dimensions; the RE-GATE amends them before this task runs.
#include <string>
#include <vector>

namespace ra2engine {

struct SbRect { int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

// Input art dimensions, measured from the loaded SHPs at wire-up time (so
// the layout adapts to the real art rather than hardcoding every number).
struct SidebarArtDims {
    // Defaults = the REAL measured art [RE-GATE: construction.md §11]:
    // top 168x32, credits 168x16, four separate 28x27 tab SHPs (tab00-tab03,
    // 5 state frames each — NOT one strip divided by 4), side1 168x69,
    // side2 168x50, cameo 60x48, powerp pips 12x2 at 3 px pitch.
    int top_w = 168, top_h = 32;       // top.shp (radar frame)
    int credits_w = 168, credits_h = 16;
    int tab_w = 28, tab_h = 27;        // ONE tab button (tab00..tab03 each)
    int side1_h = 69, side2_h = 50;    // strip background segments
    int cameo_w = 60, cameo_h = 48;
    int power_w = 12;                  // powerp.shp pip width (12x2, 3 px pitch)
};

enum class CameoState { Available, Disabled, Building, Ready };

// One sidebar cameo cell: which building type it shows + its rect.
struct CameoCell { int type = -1; SbRect rect; };

struct SidebarLayout {
    SbRect bar;              // whole sidebar (right edge strip)
    SbRect radar;            // top.shp area
    SbRect credits;          // credits readout
    SbRect tabs[4];          // Building, Defense, Infantry, Vehicle
    SbRect power;            // vertical power meter strip
    std::vector<CameoCell> cameos;   // 2-column grid, row-major

    // view_w/view_h = window size; `types` = roster indices to show, already
    // filtered (cameo != "" AND tech_level > 0) and ordered by tech_level
    // then roster order.
    static SidebarLayout compute(int view_w, int view_h,
                                 const SidebarArtDims& art,
                                 const std::vector<int>& types);

    int cameo_at(int px, int py) const;   // index into cameos, or -1
    int tab_at(int px, int py) const;     // 0..3, or -1
    bool contains(int px, int py) const { return bar.contains(px, py); }
};

}  // namespace ra2engine
