#pragma once
#include <algorithm>

namespace ra2engine {
// Shared by drawing, input hit testing and the diagnostics targets.
struct TouchBarLayout {
    struct Rect { int x, y, w, h; };
    static constexpr int columns = 9;
    static constexpr int height = 56, gap = 2, margin = 8;
    static int top(int viewport_height, int count) {
        return viewport_height - margin - height - (count > columns ? height + gap : 0);
    }
    static Rect button(int viewport_width, int viewport_height, int count, int index) {
        const int row = index / columns;
        const int row_count = std::min(columns, count - row * columns);
        const int width = std::min(50, std::max(1, (viewport_width - 2 * margin - (columns - 1) * gap) / columns));
        const int row_width = row_count * width + (row_count - 1) * gap;
        return {(viewport_width - row_width) / 2 + (index % columns) * (width + gap),
                viewport_height - margin - height - row * (height + gap), width, height};
    }
};
}
