#include "doctest.h"
#include "../engine/map_trigger.hpp"
#include "asset_fixture.hpp"
#include "ra2assets/ini.hpp"
#include <map>
#include <string>
#include <vector>

using namespace ra2engine;
using ra2assets::Ini;

namespace {
const std::vector<std::string> kRulesCountries = {"Americans", "Alliance", "Russians"};
const std::vector<std::string> kRulesBuildings = {"GAPOWR", "NAPOWR", "GACNST"};

MapTriggerData parse(const std::string& text) {
    return parse_map_triggers(Ini::parse(text), kRulesCountries, kRulesBuildings);
}
}  // namespace

// ===========================================================================
// Task 1 -- parser [RE: map-triggers.md §1]
// ===========================================================================

TEST_CASE("map_trigger: waypoint letter decode (FUN_007624b0)") {
    CHECK(map_waypoint_letters("A") == 0);
    CHECK(map_waypoint_letters("Z") == 25);
    CHECK(map_waypoint_letters("AA") == 26);
    CHECK(map_waypoint_letters("AB") == 27);
    CHECK(map_waypoint_letters("BA") == 52);
    CHECK(map_waypoint_letters("b") == 1);       // case-insensitive
    CHECK(map_waypoint_letters("A1") == 0);      // non-alpha 2nd char -> one letter
    CHECK(map_waypoint_letters("1") == -1);
    CHECK(map_waypoint_letters("") == -1);
}

TEST_CASE("map_trigger: attach flags per event/action kind [RE §3.1]") {
    CHECK(map_event_flags(kEvElapsedTime) == kAttachLogic);
    CHECK(map_event_flags(kEvLocalSet) == kAttachLogic);
    CHECK(map_event_flags(kEvBuildingExists) == kAttachHouse);
    CHECK(map_event_flags(kEvDestroyedByAnything) == kAttachObject);
    CHECK(map_event_flags(kEvEnteredBy) == (kAttachCell | kAttachObject));
    CHECK(map_event_flags(kEvAnyEvent) ==
          (kAttachCell | kAttachObject | kAttachMap | kAttachHouse | kAttachLogic));
    CHECK(map_event_flags(kEvZoneEntryBy) == (kAttachCell | kAttachMap));
    CHECK(map_action_flags(14) == kAttachObject);
    CHECK(map_action_flags(53) == 0);
}

TEST_CASE("map_trigger: [Triggers] fields, house space, linked forward ref") {
    const std::string text =
        "[Countries]\n0=Player\n1=Russians\n"
        "[Triggers]\n"
        "AAAA0001=Player,AAAA0002,First,0,1,0,1,0\n"
        "AAAA0002=Russians,<none>,Second,1,1,1,1,1\n"
        "AAAA0003=<none>,none,Third\n";
    MapTriggerData d = parse(text);
    // house space: rules list then map-new appended (Russians already present)
    REQUIRE(d.house_types.size() == 4);
    CHECK(d.house_types[3] == "Player");
    CHECK(d.find_house_type("player") == 3);
    REQUIRE(d.triggers.size() == 3);
    const MapTriggerType& t1 = d.triggers[d.find_trigger("AAAA0001")];
    CHECK(t1.house_type == 3);
    CHECK(t1.name == "First");
    CHECK(t1.enabled);
    CHECK(t1.difficulty[0]); CHECK_FALSE(t1.difficulty[1]); CHECK(t1.difficulty[2]);
    CHECK(t1.linked == d.find_trigger("AAAA0002"));
    // the forward reference created AAAA0002 BEFORE its own line: array order
    CHECK(d.find_trigger("AAAA0002") == 1);
    const MapTriggerType& t2 = d.triggers[1];
    CHECK(t2.house_type == 2);   // Russians, rules index
    CHECK_FALSE(t2.enabled);     // Disabled=1
    CHECK(t2.must_transfer);
    CHECK(t2.linked == -1);
    const MapTriggerType& t3 = d.triggers[2];
    CHECK(t3.house_type == 0);   // "<none>" -> HouseType[0]
    CHECK(t3.linked == -1);      // "none"
    CHECK_FALSE(t3.enabled);     // missing Disabled field -> disabled
    CHECK_FALSE(t3.difficulty[1]);
}

TEST_CASE("map_trigger: [Events] count, Param==2 string, reversed storage") {
    const std::string text =
        "[Triggers]\nT1=Americans,<none>,x,0,1,1,1,0\n"
        "[Events]\nT1=3,13,0,10,61,2,0,GAPOWR,36,0,5\n";
    MapTriggerData d = parse(text);
    const MapTriggerType& t = d.triggers[0];
    REQUIRE(t.events.size() == 3);
    // memory order = INI order reversed (ReadINI prepends) [RE §1.2]
    CHECK(t.events[0].kind == 36); CHECK(t.events[0].value == 5);
    CHECK(t.events[1].kind == 61); CHECK(t.events[1].param == 2);
    CHECK(t.events[1].str == "GAPOWR");
    CHECK(t.events[2].kind == 13); CHECK(t.events[2].value == 10);
}

TEST_CASE("map_trigger: [Actions] ParamKind interpretation + last token") {
    const std::string text =
        "[TeamTypes]\n0=TEAMA\n1=TEAMB\n"
        "[TEAMA]\nName=a\n[TEAMB]\nName=b\nTag=TAGT\n"
        "[Triggers]\nT1=Americans,<none>,x,0,1,1,1,0\nTRIG0002=Americans,<none>,y,0,1,1,1,0\n"
        "[Actions]\nT1=9,"
        "53,2,TRIG0002,0,0,0,0,A,"    // trigger ref, waypoint A
        "4,1,1,0,0,0,0,AB,"           // team by 1-char index -> TEAMB
        "7,1,TEAMA,0,0,0,0,B,"        // team by ID
        "80,5,TEAMB,0,0,0,0,7,"       // ParamKind 5: last token -> Value
        "11,4,  Txt:Label ,0,0,0,0,A,"  // text, trimmed
        "125,10,NAMISL,0,0,0,0,C,"     // TechnoID
        "56,0,12,1,2,3,4,A,"           // Value + p[]
        "46,3,TAGZ,0,0,0,0,A,"         // tag ref (forward-created)
        "99,11,3,0,0,0,0,9\n";         // ParamKind 11 -> Value2
    MapTriggerData d = parse(text);
    REQUIRE(d.team_ids.size() == 2);
    const MapTriggerType& t = d.triggers[0];
    REQUIRE(t.actions.size() == 9);
    CHECK(t.actions[0].kind == 53); CHECK(t.actions[0].trigger == 1);
    // a <3-char P1 is an ARRAY INDEX, not an ID [RE §1.3]
    CHECK(t.actions[0].waypoint == 0);
    CHECK(t.actions[1].team == 1); CHECK(t.actions[1].waypoint == 27);
    CHECK(t.actions[2].team == 0); CHECK(t.actions[2].waypoint == 1);
    CHECK(t.actions[3].team == 1); CHECK(t.actions[3].value == 7);
    CHECK(t.actions[3].waypoint == -1);
    CHECK(t.actions[4].text == "Txt:Label");
    CHECK(t.actions[5].text == "NAMISL"); CHECK(t.actions[5].waypoint == 2);
    CHECK(t.actions[6].value == 12);
    CHECK(t.actions[6].p[0] == 1); CHECK(t.actions[6].p[3] == 4);
    CHECK(t.actions[7].tag == d.find_tag("TAGZ"));
    CHECK(t.actions[8].value == 3); CHECK(t.actions[8].value2 == 9);
    CHECK(t.actions[8].waypoint == -1);
    // TagType creation order: TeamType Tag= first, then action refs [RE §1.8]
    CHECK(d.find_tag("TAGT") == 0);
    CHECK(d.find_tag("TAGZ") == 1);
}

TEST_CASE("map_trigger: [Tags], [VariableNames], [CellTags], unknown refs") {
    const std::string text =
        "[Triggers]\nT1=Americans,<none>,x,0,1,1,1,0\n"
        "[Actions]\nT1=1,53,2,NOPE,0,0,0,0,A\n"
        "[Tags]\nG1=2,Tag One,T1\nG2=0,Tag Two,MISSING\n"
        "[VariableNames]\n0=First,1\n3=Fourth,0\n150=Out,1\n"
        "[CellTags]\n30105=G1\n107018=G2\n";
    MapTriggerData d = parse(text);
    REQUIRE(d.tags.size() == 2);
    CHECK(d.tags[0].persistence == 2);
    CHECK(d.tags[0].name == "Tag One");
    CHECK(d.tags[0].first_trigger == 0);
    // a dangling trigger id is created (find-or-create) but never defined
    CHECK(d.tags[1].first_trigger == d.find_trigger("MISSING"));
    CHECK_FALSE(d.triggers[d.find_trigger("MISSING")].defined);
    CHECK(d.triggers[0].actions[0].trigger == d.find_trigger("NOPE"));
    REQUIRE(d.local_init.size() == 100);
    CHECK(d.local_init[0] == 1); CHECK(d.local_names[0] == "First");
    CHECK(d.local_init[3] == 0); CHECK(d.local_names[3] == "Fourth");
    REQUIRE(d.cell_tags.size() == 2);
    CHECK(d.cell_tags[0].x == 105); CHECK(d.cell_tags[0].y == 30);
    CHECK(d.cell_tags[0].tag == 0);
    CHECK(d.cell_tags[1].x == 18); CHECK(d.cell_tags[1].y == 107);
}

TEST_CASE("map_trigger: real sov07tmd.map parses [install-gated]") {
    if (!ra2fixture::assets_available()) { MESSAGE("install absent; skipped"); return; }
    if (ra2fixture::mix_bytes_by_name("mapsmd03.mix").empty()) {
        MESSAGE("mapsmd03.mix absent; skipped"); return;
    }
    const ra2assets::MixFile& maps = ra2fixture::mix_by_name("mapsmd03.mix");
    const ra2assets::MixEntry* e = maps.find(0x133a0cb2u);
    REQUIRE(e != nullptr);
    auto raw = maps.read(*e);
    Ini map = Ini::parse(std::string(raw.begin(), raw.end()));
    const Ini& rules = ra2fixture::rulesmd_ini();
    MapTriggerData d = parse_map_triggers(map, ini_list_values(rules, "Countries"),
                                          ini_list_values(rules, "BuildingTypes"));
    CHECK(d.find_house_type("Americans") == 0);
    CHECK(d.find_house_type("Einstein") == 14);
    CHECK(d.find_house_type("Player") == 15);   // the _Lose DestroyedAll value
    const int lose = d.find_trigger("05FD863C");
    REQUIRE(lose >= 0);
    CHECK(d.triggers[lose].name == "_Lose");
    REQUIRE(d.triggers[lose].events.size() == 1);
    CHECK(d.triggers[lose].events[0].kind == kEvDestroyedAll);
    CHECK(d.triggers[lose].events[0].value == 15);
    int defined = 0;
    for (const MapTriggerType& t : d.triggers) defined += t.defined ? 1 : 0;
    CHECK(defined == (int)map.section("Triggers").size());
    CHECK(d.cell_tags.size() == map.section("CellTags").size());
    CHECK(d.local_names[5] == "Soviets Freed");
}
