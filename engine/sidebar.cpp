#include "sidebar.hpp"

#include <algorithm>

namespace ra2engine {

SidebarLayout SidebarLayout::compute(int view_w, int view_h,
                                     const std::vector<int>& types) {
    SidebarLayout lay;

    // Whole sidebar column on the right edge, full window height
    // [RE: construction.md §11a — x0 = screen_w − 168, y0 = 0].
    lay.bar = {view_w - kSidebarW, 0, kSidebarW, view_h};
    const int x0 = lay.bar.x;

    // Fixed top stack: credits(0,0) -> top cap(0,16) -> radar(0,48) ->
    // side1(0,158). 158 is the binary's literal 0x9E = 16 + 32 + 110.
    lay.credits = {x0, 0, kSidebarW, kCreditsH};
    lay.top_cap = {x0, kTopCapY, kSidebarW, kRadarY - kTopCapY};
    lay.radar = {x0, kRadarY, kSidebarW, kRadarH};
    lay.side1 = {x0, kSide1Y, kSidebarW, kSide1H};

    // Tab row: four separate 28x27 buttons at x = 26 + 29*i, y = 197
    // [RE: §11a — pitch 29, NOT bar.w/4].
    for (int i = 0; i < 4; ++i)
        lay.tabs[i] = {x0 + kTabX0 + kTabPitch * i, kTabY, kTabW, kTabH};

    // Visible cameo rows: N = floor((view_h - 234 - side3_h)/50), min 1
    // [RE: §11a] — 6 at 600 px, 9 at 720 px. side2 tiles once per row.
    lay.side2_rows = std::max(1, (view_h - 234 - kSide3H) / kCameoRowPitch);
    const int strip_h = lay.side2_rows * kSide2H;

    // Power pip column hugs the strip's left edge from the strip top;
    // capacity = (strip_h + 3)/3 pips at 3 px pitch [RE: §11a/§8].
    lay.power_pips = (strip_h + kPowerPitch) / kPowerPitch;
    lay.power = {x0 + kPowerX, kStripTop, kPowerPipW, lay.power_pips * kPowerPitch};

    // Strip scroll buttons: r-dn left, r-up right, on the row 7 px below the
    // strip (inert this phase; rects reserved for hit-test completeness).
    const int scroll_y = kStripTop + strip_h + 7;
    lay.scroll_dn = {x0 + kScrollX, scroll_y, kScrollW, kScrollH};
    lay.scroll_up = {x0 + kScrollX + kScrollW, scroll_y, kScrollW, kScrollH};

    // Cameo grid: origin (22, strip_top + 1), column pitch 63, row pitch 50,
    // 2 columns row-major — these land the 60x48 cameos INSIDE the socket
    // recesses baked into side2.shp [RE: §11a]. Cells beyond the visible
    // 2*N are not laid out (no strip scrolling with this roster).
    const std::size_t max_cells = static_cast<std::size_t>(2 * lay.side2_rows);
    for (std::size_t i = 0; i < types.size() && i < max_cells; ++i) {
        const int col = static_cast<int>(i % 2);
        const int row = static_cast<int>(i / 2);
        CameoCell cell;
        cell.type = types[i];
        cell.rect = {x0 + kCameoX + col * kCameoColPitch,
                     kStripTop + 1 + row * kCameoRowPitch, kCameoW, kCameoH};
        lay.cameos.push_back(cell);
    }

    return lay;
}

int SidebarLayout::cameo_at(int px, int py) const {
    for (std::size_t i = 0; i < cameos.size(); ++i) {
        if (cameos[i].rect.contains(px, py)) return static_cast<int>(i);
    }
    return -1;
}

int SidebarLayout::tab_at(int px, int py) const {
    for (int i = 0; i < 4; ++i) {
        if (tabs[i].contains(px, py)) return i;
    }
    return -1;
}

}  // namespace ra2engine
