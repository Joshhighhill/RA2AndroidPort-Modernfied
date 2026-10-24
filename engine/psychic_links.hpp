#pragma once
#include "world.hpp"
#include <utility>
#include <vector>
namespace ra2engine {
// Negative controller IDs encode buildings as -1-index; units remain >=0.
inline std::vector<std::pair<int,int>> skirmish_control_links(const World& w) {
    std::vector<std::pair<int,int>> out;
    for (int i=0;i<static_cast<int>(w.units().size());++i) {
        const auto& v=w.units()[i];
        if (!v.alive || v.die_time>=0) continue;
        const int c=v.w7.mc_controller,b=v.w7.mc_building;
        if(c>=0 && c<static_cast<int>(w.units().size()) && w.units()[c].alive && w.units()[c].die_time<0)
            out.emplace_back(c,i);
        else if(b>=0 && b<static_cast<int>(w.buildings().size()) && w.buildings()[b].alive &&
                w.buildings()[b].state!=Building::State::Dead) out.emplace_back(-1-b,i);
    }
    return out;
}
}
