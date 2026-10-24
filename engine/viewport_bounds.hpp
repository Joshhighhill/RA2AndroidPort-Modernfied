#pragma once
#include <cstdint>
namespace ra2engine {
// Half-open pixel bounds; include cliff extensions independently of the base.
inline bool viewport_intersects(int x, int y, int w, int h, int vw, int vh) {
    return w > 0 && h > 0 && vw > 0 && vh > 0 && x < vw && y < vh &&
        static_cast<std::int64_t>(x) + w > 0 && static_cast<std::int64_t>(y) + h > 0;
}
}
