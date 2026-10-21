#pragma once
#include <algorithm>
namespace ra2engine {
struct TouchBarLayout {
    struct Rect { int x, y, w, h; };
    static constexpr int columns = 4, height = 56, gap = 4, margin = 8;
    static constexpr int panel_width = 300, header_height = 52;
    static int top(int viewport_height, int count) {
        const int content = count == 13 ? 164 : ((count + 3) / 4) * 60 - 4;
        return viewport_height - margin - content - header_height;
    }
    static Rect toggle(int viewport_width, int viewport_height, int count, bool collapsed) {
        if (collapsed) return {margin, viewport_height-margin-44, 44, 44};
        return {margin+std::min(panel_width,viewport_width-2*margin)-48,
                top(viewport_height,count)+4,44,44};
    }
    static Rect button(int viewport_width, int viewport_height, int count, int index) {
        const int pw = std::min(panel_width, viewport_width - 2 * margin);
        if (count == 13 && index >= 8) {
            const int w = (pw - 16) / 5;
            return {margin + (index - 8) * (w + gap), viewport_height - margin - 164,
                    w, 44};
        }
        const int rows = count == 13 ? 2 : (count + 3) / 4;
        const int w = (pw - 12) / 4;
        return {margin + (index % 4) * (w + gap),
                viewport_height - margin - (rows * 60 - 4) + (index / 4) * 60, w, height};
    }
};
}
