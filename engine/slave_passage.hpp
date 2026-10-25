#pragma once
#include "world.hpp"
#include <algorithm>

namespace ra2engine {
// Narrow manager exception; never grants general infantry/building passage.
inline bool slave_mobile_pair(const Unit& worker,const Unit& miner,int miner_id) {
    return worker.is_infantry && !worker.is_vehicle && !worker.is_aircraft && worker.w7.slave_manager==-2-miner_id &&
        worker.house==miner.house && worker.alive && worker.die_time<0 &&
        miner.alive && miner.die_time<0 && miner.cargo_of<0 && miner.is_vehicle &&
        worker.on_deck==miner.on_deck;
}
inline const Building* slave_passage_building(const Unit& u,const std::vector<Building>& bs,
                                             const std::vector<BuildingTypeDef>& types) {
    const int bi=u.w7.slave_manager;
    if(!u.is_infantry || u.is_vehicle || u.is_aircraft || !u.alive || u.die_time>=0 || bi<0 || bi>=int(bs.size()))return nullptr;
    const auto& b=bs[bi];
    if(!b.alive || b.state!=Building::State::Idle || b.selling || b.house!=u.house ||
       b.type<0 || b.type>=int(types.size()) || types[b.type].slave_count<=0)return nullptr;
    return &b;
}
inline void append_slave_passage(const Unit& u,const std::vector<Building>& bs,
                                  const std::vector<BuildingTypeDef>& types,int width,int height,
                                  std::vector<int>& cells) {
    if(const auto* b=slave_passage_building(u,bs,types)) {
        for(int y=b->cy;y<b->cy+b->fh;++y)for(int x=b->cx;x<b->cx+b->fw;++x)
            if(x>=0 && y>=0 && x<width && y<height)cells.push_back(y*width+x);
        std::sort(cells.begin(),cells.end());cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
    }
}
}
