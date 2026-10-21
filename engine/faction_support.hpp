#pragma once
#include "world.hpp"
#include <algorithm>
#include <cctype>
#include <string>
namespace ra2engine {
inline World::Side faction_side(const std::string& country,std::string side) {
    std::transform(side.begin(),side.end(),side.begin(),[](unsigned char c){return char(std::toupper(c));});
    if(side=="THIRDSIDE" || country=="YuriCountry") return World::Side::Yuri;
    if(side=="NOD") return World::Side::Soviet;
    return World::Side::Allied;
}
inline const char* faction_country(World::Side side) {
    return side==World::Side::Yuri ? "YuriCountry" : side==World::Side::Soviet ? "Russians" : "Americans";
}
inline const char* faction_rules_side(World::Side side) {
    return side==World::Side::Yuri ? "THIRDSIDE" : side==World::Side::Soviet ? "NOD" : "GDI";
}
}
