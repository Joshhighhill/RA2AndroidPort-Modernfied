#include "doctest.h"
#include "faction_support.hpp"
#include "menu_readability.hpp"
#include "ra2assets/rules.hpp"
TEST_CASE("Yuri country remains a distinct side with its own default") {
 using namespace ra2engine;
 CHECK(faction_side("YuriCountry","ThirdSide")==World::Side::Yuri);
 CHECK(faction_side("YuriCountry","")==World::Side::Yuri);
 CHECK(faction_side("Russians","Nod")==World::Side::Soviet);
 CHECK(std::string(faction_country(World::Side::Yuri))=="YuriCountry");
 CHECK(std::string(faction_rules_side(World::Side::Yuri))=="THIRDSIDE");
 auto ini=ra2assets::Ini::parse("[BuildingTypes]\n0=GACNST\n7=YACNST\n12=YAPOWR\n");
 const auto rules=ra2assets::Rules::load(ini);const auto types=rules.listed_types("BuildingTypes");
 CHECK(types.size()==3);
 CHECK(std::find(types.begin(),types.end(),"YACNST")!=types.end());
 CHECK(rules.listed_types("missing").empty());
}
TEST_CASE("touch menu text avoids integer rounding that shrinks control labels") {
 using namespace ra2engine;
 const double touch=menu_font_scale(1.5,12,true);
 CHECK(touch>menu_font_scale(1.5,12,false));
 CHECK(touch*16<=1.5*19); // Fits the 19-logical-pixel country combobox.
 CHECK(menu_font_scale(1.5,14,true)==touch); // fit() and drawing agree.
}
