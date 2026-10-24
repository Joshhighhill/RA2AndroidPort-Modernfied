#pragma once
#include "sidebar.hpp"
#include <cmath>
#include <utility>
namespace ra2engine {
// Adapt existing dialog templates to the original large shell. The left
// content panel grows; native-width sidebar controls keep their size.
inline SbRect yuri_shell_rect(SbRect r) {
    if(r.x>=472) {
        r.x+=160;
        if(r.y>=409 && r.y<448) r.y+=126;
        else if(r.y>=448) r.y+=120;
    } else {
        const int right=r.x+r.w;
        const int x=static_cast<int>(std::lround(r.x*632.0/472));
        const int end=right<=472 ? static_cast<int>(std::lround(right*632.0/472)) : right+160;
        r.x=x;r.w=end-x;
        if(r.y<448) {
            const int bottom=r.y+r.h;
            const int y=static_cast<int>(std::lround(r.y*568.0/448));
            const int end_y=bottom<=448 ? static_cast<int>(std::lround(bottom*568.0/448)) : bottom+120;
            r.y=y;r.h=end_y-y;
        } else r.y+=120;
    }
    return r;
}
inline std::pair<int,int> yuri_shell_point(int x,int y) {
    if(x>=632) {
        x-=160;
        if(y>=535 && y<577) y-=126;
        else if(y>=568) y-=120;
    } else {
        x=static_cast<int>(std::lround(x*472.0/632));
        y=y<568 ? static_cast<int>(std::lround(y*448.0/568)) : y-120;
    }
    return {x,y};
}
}
