#include "doctest.h"
#include "ra2assets/ini.hpp"
#include "ra2assets/rules.hpp"

// The 4 new buildings' recovered stat blocks [RE: roster.md §2], authored as a
// minimal rulesmd fragment. Asserting through ra2assets::Rules proves our
// recovered constants round-trip through the exact parser the building resolve
// loop (main.cpp:1510-1591) feeds from. (The full-install byte-identity of the
// resolved kRoster is covered by --demo-roster + the 17-gate re-verify.)
static const char* kRosterBuildingsIni = R"INI(
[NANRCT]
Prerequisite=NATECH,NACNST
TechLevel=9
Cost=1000
Strength=1000
Armor=concrete
Sight=5
Power=2000

[ATESLA]
Image=GAPRIS
Prerequisite=POWER,RADAR,GACNST
TechLevel=6
Cost=1500
Strength=600
Armor=steel
Sight=8
Power=-75
Primary=PrismShot

[NASAM]
Prerequisite=BARRACKS,GACNST
TechLevel=4
Cost=1000
Strength=900
Armor=steel
Sight=10
Power=-50
Primary=RedEye2

[GAAIRC]
Prerequisite=GAREFN,GACNST
TechLevel=3
Cost=1000
Strength=600
Armor=steel
Sight=5
Power=-50

[PrismShot]
Damage=120
ROF=45
Range=8
Warhead=PrismWarhead

[RedEye2]
Damage=75
ROF=55
Range=12
Warhead=SAMWH
)INI";

TEST_CASE("roster: 4 new buildings resolve recovered stats [RE: roster.md §2]") {
    ra2assets::Ini ini = ra2assets::Ini::parse(kRosterBuildingsIni);
    ra2assets::Rules rules = ra2assets::Rules::load(ini);

    // NANRCT — Soviet Nuclear Reactor (huge power output; generic power path).
    CHECK(rules.cost("NANRCT") == 1000);
    CHECK(rules.tech_level("NANRCT") == 9);
    CHECK(rules.strength("NANRCT") == 1000);
    CHECK(rules.power("NANRCT") == 2000);
    CHECK(rules.prerequisite("NANRCT") == std::vector<std::string>{"NATECH", "NACNST"});

    // ATESLA — Allied Prism Tower (Image=GAPRIS; PrismShot defensive weapon).
    CHECK(rules.image("ATESLA") == "GAPRIS");
    CHECK(rules.cost("ATESLA") == 1500);
    CHECK(rules.tech_level("ATESLA") == 6);
    CHECK(rules.strength("ATESLA") == 600);
    CHECK(rules.power("ATESLA") == -75);
    CHECK(rules.primary("ATESLA") == "PrismShot");
    CHECK(rules.weapon_damage("PrismShot") == 120);
    CHECK(rules.weapon_range("PrismShot") == doctest::Approx(8.0));
    CHECK(rules.weapon_warhead("PrismShot") == "PrismWarhead");

    // NASAM — Allied Patriot Missile (RedEye2 AA weapon).
    CHECK(rules.cost("NASAM") == 1000);
    CHECK(rules.tech_level("NASAM") == 4);
    CHECK(rules.strength("NASAM") == 900);
    CHECK(rules.power("NASAM") == -50);
    CHECK(rules.primary("NASAM") == "RedEye2");
    CHECK(rules.weapon_damage("RedEye2") == 75);
    CHECK(rules.weapon_warhead("RedEye2") == "SAMWH");

    // GAAIRC — Allied Airforce Command HQ (radar; no weapon).
    CHECK(rules.cost("GAAIRC") == 1000);
    CHECK(rules.tech_level("GAAIRC") == 3);
    CHECK(rules.strength("GAAIRC") == 600);
    CHECK(rules.power("GAAIRC") == -50);
    CHECK(rules.prerequisite("GAAIRC") == std::vector<std::string>{"GAREFN", "GACNST"});
}
