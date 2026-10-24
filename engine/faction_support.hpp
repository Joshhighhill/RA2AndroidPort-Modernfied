#pragma once
#include "world.hpp"
#include <algorithm>
#include <cctype>
#include <string>
namespace ra2engine {
// Vanilla Yuri structures only. Owner= includes Yuri on shared civilian scenery
// and must not promote every such object into the global simulation roster.
inline bool vanilla_yuri_building(const std::string& id) {
    static const char* ids[]={"YACNST","YAPOWR","YABRCK","YAREFN","YAWEAP",
        "YAYARD","NAPSIS","YATECH","YAGRND","NACLON","YAGGUN","YAPSYT",
        "NATBNK","YAGNTC","YAPPET","GAFWLL"};
    for(const char* value:ids) if(id==value) return true;
    return false;
}
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
