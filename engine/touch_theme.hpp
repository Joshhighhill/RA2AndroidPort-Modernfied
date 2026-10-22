#pragma once
#include "ra2assets/pal.hpp"
namespace ra2engine {
struct TouchTheme { ra2assets::Rgba panel, border, accent, active, text; };
// World::Side: Allied=0, Soviet=1, Yuri=2. The local player's faction
// owns the skin, including when captured enemy units are selected.
inline TouchTheme touch_theme(int side) {
    if (side == 1) return {{29,22,23,240},{151,94,87,255},{235,108,91,255},{112,35,35,245},{247,225,213,255}};
    if (side == 2) return {{28,22,36,240},{144,116,163,255},{202,153,239,255},{88,45,119,245},{240,224,252,255}};
    return {{21,29,38,240},{112,143,173,255},{125,192,241,255},{34,73,117,245},{223,239,250,255}};
}
}
