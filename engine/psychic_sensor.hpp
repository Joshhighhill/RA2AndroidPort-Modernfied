#pragma once
#include "world.hpp"
#include <cmath>
#include <array>
namespace ra2engine {
inline std::vector<std::array<double,4>> psychic_intent_dashes(double x0,double y0,double x1,double y1) {
    std::vector<std::array<double,4>> out;
    const double length=std::hypot(x1-x0,y1-y0);
    if(!std::isfinite(length) || length<0.001)return out;
    const double dx=(x1-x0)/length,dy=(y1-y0)/length;
    // Bound rendering work; retain visible gaps even for unusually long lines.
    const double period=std::max(10.0,length/512.0);
    for(double start=0;start<length;start+=period) {
        const double end=std::min(length,start+period*0.6);
        out.push_back({x0+dx*start,y0+dy*start,x0+dx*end,y0+dy*end});
    }
    return out;
}
inline ra2assets::Rgba psychic_intent_color(const World& w,const ra2assets::Palette& pal,int house) {
    return w.house_palette(pal,house)[24];
}
inline bool psychic_sensor_active(const World& w,int bi,int viewer) {
    if(bi<0 || bi>=static_cast<int>(w.buildings().size())) return false;
    const auto& b=w.buildings()[bi];
    if(!b.alive || b.state!=Building::State::Idle || b.power_off || b.w7.warp_unpowered ||
       b.type<0 || b.type>=static_cast<int>(w.btypes().size()) || !w.are_allied(viewer,b.house)) return false;
    const auto& t=w.btypes()[b.type];
    return t.w5.psychic_radius>0 && (!t.powered || !w.house_low_power(b.house));
}
// Return only the commanded destination inside the sensor radius. The intent
// line is detectable through fog; this does not reveal the unit or map cells.
inline bool psychic_sensor_target(const World& w,int bi,int ui,int& x,int& y) {
    if(bi<0 || bi>=static_cast<int>(w.buildings().size())) return false;
    if(ui<0 || ui>=static_cast<int>(w.units().size())) return false;
    const auto& b=w.buildings()[bi];const auto& u=w.units()[ui];
    if(b.type<0 || b.type>=static_cast<int>(w.btypes().size())) return false;
    if(!u.alive || u.die_time>=0 || u.cargo_of>=0 || !w.is_enemy(b.house,u.house)) return false;
    if(u.target>=0 && u.target<static_cast<int>(w.units().size())) {
        const auto& v=w.units()[u.target];if(!v.alive || v.die_time>=0) return false;x=v.cell_x;y=v.cell_y;
    } else if(u.target_building>=0 && u.target_building<static_cast<int>(w.buildings().size())) {
        const auto& v=w.buildings()[u.target_building];if(!v.alive || v.state==Building::State::Dead) return false;x=v.cx;y=v.cy;
    } else if(u.moving()) {x=u.path.back().x;y=u.path.back().y;}
    else return false;
    const double dx=x-(b.cx+(b.fw-1)*0.5),dy=y-(b.cy+(b.fh-1)*0.5);
    const double radius=w.btypes()[b.type].w5.psychic_radius;
    return dx*dx+dy*dy<=radius*radius;
}
}
