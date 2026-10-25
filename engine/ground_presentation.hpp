#pragma once
#include <algorithm>
namespace ra2engine {
inline double deck_unit_depth(double fx,double fy,int cx,int cy,bool deck) {
    return deck ? std::max(fx+fy,double(cx+cy))+1.001 : fx+fy;
}
// Visual interpolation of legal movement only; no simulation height changes.
inline double segment_ground_level(double fx,double fy,int sx,int sy,int tx,int ty,double a,double b) {
    const double dx=tx-sx,dy=ty-sy,len=dx*dx+dy*dy;
    if(len<=0)return a;
    const double t=std::clamp(((fx-sx)*dx+(fy-sy)*dy)/len,0.0,1.0);
    return a+(b-a)*t;
}
}
