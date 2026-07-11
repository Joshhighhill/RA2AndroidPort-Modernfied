#include "sidebar.hpp"

namespace ra2engine {

SidebarLayout SidebarLayout::compute(int view_w, int view_h,
                                     const SidebarArtDims& art,
                                     const std::vector<int>& types) {
    SidebarLayout lay;

    // bar = whole sidebar strip on the right edge
    lay.bar = {view_w - art.top_w, 0, art.top_w, view_h};

    // radar at the top of the bar
    lay.radar = {lay.bar.x, lay.bar.y, art.top_w, art.top_h};

    // credits directly below radar
    lay.credits = {lay.bar.x, lay.radar.y + lay.radar.h, art.credits_w, art.credits_h};

    // 4 tabs in one row below credits, pitch = bar.w/4 (42), each tab_w (28) wide centered in its slot
    int tab_pitch = lay.bar.w / 4;  // 42 for 168-wide bar
    int tabs_y = lay.credits.y + lay.credits.h;
    for (int i = 0; i < 4; ++i) {
        lay.tabs[i].x = lay.bar.x + i * tab_pitch + (tab_pitch - art.tab_w) / 2;
        lay.tabs[i].y = tabs_y;
        lay.tabs[i].w = art.tab_w;
        lay.tabs[i].h = art.tab_h;
    }

    // power strip: left edge of bar, below tabs, width art.power_w, full height remaining
    int tabs_bottom = tabs_y + art.tab_h;
    lay.power = {lay.bar.x, tabs_bottom, art.power_w, view_h - tabs_bottom};

    // cameo grid: 2 columns starting right of power strip (x = bar.x + power_w + 2),
    // rows from tabs_bottom + 2, row-major ordering, pitch = cameo size + 1px gutter
    int cameo_x_start = lay.bar.x + art.power_w + 2;
    int cameo_y_start = tabs_bottom + 2;
    int cameo_pitch_x = art.cameo_w + 1;
    int cameo_pitch_y = art.cameo_h + 1;

    for (size_t i = 0; i < types.size(); ++i) {
        int col = i % 2;  // 0 or 1 for 2-column grid
        int row = i / 2;

        CameoCell cell;
        cell.type = types[i];
        cell.rect.x = cameo_x_start + col * cameo_pitch_x;
        cell.rect.y = cameo_y_start + row * cameo_pitch_y;
        cell.rect.w = art.cameo_w;
        cell.rect.h = art.cameo_h;

        lay.cameos.push_back(cell);
    }

    return lay;
}

int SidebarLayout::cameo_at(int px, int py) const {
    for (size_t i = 0; i < cameos.size(); ++i) {
        if (cameos[i].rect.contains(px, py)) {
            return i;
        }
    }
    return -1;
}

int SidebarLayout::tab_at(int px, int py) const {
    for (int i = 0; i < 4; ++i) {
        if (tabs[i].contains(px, py)) {
            return i;
        }
    }
    return -1;
}

}  // namespace ra2engine
