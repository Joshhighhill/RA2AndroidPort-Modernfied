#pragma once
#include <algorithm>
#include <cmath>
namespace ra2engine {
inline double menu_font_scale(double canvas,double logical_px,bool touch) {
    return touch ? std::max(1.0,std::floor(canvas*std::max(16.0,logical_px)/16.0)) :
                   std::max(1.0,std::round(canvas*logical_px/16.0));
}
}
