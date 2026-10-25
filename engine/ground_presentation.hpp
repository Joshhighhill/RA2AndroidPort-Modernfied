#pragma once
#include <algorithm>
#include <cmath>
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
template<class W,class U> double moving_ground_level(const W& w,const U& u) {
    const double current=w.cell_level(u.cell_x,u.cell_y)+(u.on_deck?4:0);
    if(u.is_aircraft || u.is_naval || !u.moving() || !w.terrain_armed() || u.path.size()<2)return current;
    const std::size_t first=std::max<std::size_t>(1,std::min(u.path_idx,u.path.size()-1));
    double best=0.75*0.75,result=current;
    for(std::size_t k=first;k<u.path.size() && k<=first+1;++k) {
        const auto s=u.path[k-1],t=u.path[k];
        const double dx=t.x-s.x,dy=t.y-s.y,len=dx*dx+dy*dy;
        if(len<=0 || std::abs(dx)>1 || std::abs(dy)>1)continue;
        const double f=std::clamp(((u.fx-s.x)*dx+(u.fy-s.y)*dy)/len,0.0,1.0);
        const double ex=u.fx-(s.x+f*dx),ey=u.fy-(s.y+f*dy),distance=ex*ex+ey*ey;
        if(distance>best)continue;
        auto height=[&](int x,int y){const int b=w.cell_level(x,y);return double(b+(w.deck_intact(x,y) && b+4==current ? 4 : 0));};
        const double a=height(s.x,s.y),b=height(t.x,t.y);
        if(std::abs(a-b)>1)continue; // stale cliff or broken bridge, not a ramp
        best=distance;result=a+(b-a)*f;
    }
    return result;
}
}
