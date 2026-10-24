#pragma once
#include "superweapon.hpp"
namespace ra2engine {
inline int power_cameo_id(SuperweaponType type) { return -1000-static_cast<int>(type); }
inline SuperweaponType power_cameo_type(int id) {
    if(id>-1001 || id<-1012) return SuperweaponType::None;
    return static_cast<SuperweaponType>(-1000-id);
}
}
