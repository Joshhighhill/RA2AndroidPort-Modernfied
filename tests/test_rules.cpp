#include "doctest.h"
#include "ra2assets/rules.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace ra2assets;

TEST_CASE("rules: synthetic - explicit Image/Speed, unknown id falls back") {
    std::string text =
        "[GI]\n"
        "Image=GI\n"
        "Speed=4\n";
    Ini ini = Ini::parse(text);
    Rules rules = Rules::load(ini);

    CHECK(rules.image("GI") == "GI");
    CHECK(rules.speed("GI") == 4);

    // Unknown unit id: image falls back to the id itself; speed defaults to 0.
    CHECK(rules.image("NOSUCHUNIT") == "NOSUCHUNIT");
    CHECK(rules.speed("NOSUCHUNIT") == 0);

    // Voice getters: absent section/key -> "".
    CHECK(rules.voice_select("E9") == "");
}

TEST_CASE("rules: image() returns the explicit Image= value, not just the id") {
    // Real rulesmd.ini's actual GI unit id is "E1" (see the real-asset test
    // below) with Image=GI -- distinguish "image looks up the key" from
    // "image always equals the unit id" by using a section whose id differs
    // from its Image= value.
    std::string text =
        "[E1]\n"
        "Image=GI\n"
        "Speed=4\n";
    Ini ini = Ini::parse(text);
    Rules rules = Rules::load(ini);

    CHECK(rules.image("E1") == "GI");
    CHECK(rules.speed("E1") == 4);
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("rules: locates real rulesmd.ini from the install and reads GI (unit id E1)") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    // rulesmd.ini is NOT a top-level entry of ra2md.mix in a stock YR 1.001
    // install -- it lives inside a nested mix. Try the top level first (some
    // mod layouts flatten it there), then descend the usual homes: the local
    // assets mix and the expansion mixes.
    std::vector<uint8_t> rules_bytes;
    if (const MixEntry* e = top.find(mix_id("rulesmd.ini"))) {
        rules_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",
            "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix", "conqmd.mix",
        };
        for (const char* name : candidates) {
            try {
                MixFile nested = top.open_nested(name);
                if (const MixEntry* e2 = nested.find(mix_id("rulesmd.ini"))) {
                    rules_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (rules_bytes.empty()) {
        MESSAGE("skipped: rulesmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    Rules rules = Rules::load(ini);

    // The real rulesmd.ini's rules-section id for the GI is "E1" -- "GI" is
    // only the unit's flavor Name= / Image= (SHP base name) value, not the
    // section id itself (confirmed by inspecting the extracted rulesmd.ini
    // directly: "[E1]" / "Name=GI" / "Image=GI" / "Speed=4"). So the
    // real-data assertion below is phrased against the real section id
    // rather than a literal "[GI]" section, which does not exist in stock
    // rulesmd.ini.
    REQUIRE(ini.has("E1"));
    CHECK(rules.image("E1") == "GI");
    CHECK(rules.speed("E1") > 0);
    MESSAGE("rulesmd.ini [E1] (GI) speed=" << rules.speed("E1"));
}

TEST_CASE("rules: combat getters read unit/weapon/warhead fields") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[E9]\nStrength=125\nArmor=flak\nPrimary=Zap\n"
        "[Zap]\nDamage=15\nROF=25\nRange=4\nWarhead=SA\nReport=ZapSound\n"
        "[SA]\nVerses=100%,80%,80%,50%,25%,25%,75%,50%,25%,100%,100%\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.strength("E9") == 125);
    CHECK(r.armor("E9") == "flak");
    CHECK(r.primary("E9") == "Zap");
    CHECK(r.weapon_damage("Zap") == 15);
    CHECK(r.weapon_rof("Zap") == 25);
    CHECK(r.weapon_range("Zap") == doctest::Approx(4.0));
    CHECK(r.weapon_warhead("Zap") == "SA");
    CHECK(r.weapon_report("Zap") == "ZapSound");
    auto v = r.verses("SA");
    REQUIRE(v.size() == 11);
    CHECK(v[0] == 100); CHECK(v[1] == 80); CHECK(v[4] == 25); CHECK(v[6] == 75);
    // Absent sections/keys fall back safely.
    CHECK(r.strength("NOPE") == 0);
    CHECK(r.armor("NOPE") == "none");
    CHECK(r.primary("NOPE") == "");
    CHECK(r.verses("NOPE").empty());
}

TEST_CASE("rules: verses parsing tolerates spaces and missing % signs") {
    ra2assets::Ini ini = ra2assets::Ini::parse("[W]\nVerses= 100% , 80 ,70%\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    auto v = r.verses("W");
    REQUIRE(v.size() == 3);
    CHECK(v[0] == 100); CHECK(v[1] == 80); CHECK(v[2] == 70);
}

TEST_CASE("rules: vehicle getters") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[HTNK]\nROT=5\nTurret=yes\nExplosion=TWLT070,S_BANG48\n"
        "[UTNK]\nTurret=no\n"
        "[120mm]\nRange=5.75\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.rot("HTNK") == 5);
    CHECK(r.rot("NOPE") == 0);
    CHECK(r.has_turret("HTNK"));
    CHECK_FALSE(r.has_turret("UTNK"));
    CHECK_FALSE(r.has_turret("NOPE"));
    CHECK(r.explosion_first("HTNK") == "TWLT070");
    CHECK(r.explosion_first("NOPE") == "");
    CHECK(r.weapon_range("120mm") == doctest::Approx(5.75));
}

TEST_CASE("rules: real rulesmd combat values pin (E1/E2/M60/M1Carbine/SA)") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    std::vector<uint8_t> rules_bytes;
    if (const MixEntry* e = top.find(mix_id("rulesmd.ini"))) {
        rules_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",
            "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix", "conqmd.mix",
        };
        for (const char* name : candidates) {
            try {
                MixFile nested = top.open_nested(name);
                if (const MixEntry* e2 = nested.find(mix_id("rulesmd.ini"))) {
                    rules_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
            }
        }
    }
    if (rules_bytes.empty()) {
        MESSAGE("skipped: rulesmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    Rules rules = Rules::load(ini);

    // E1 (GI) combat values
    CHECK(rules.strength("E1") == 125);
    CHECK(rules.armor("E1") == "none");
    CHECK(rules.primary("E1") == "M60");
    CHECK(rules.image("E1") == "GI");
    CHECK(rules.voice_select("E1") == "GISelect");
    CHECK(rules.voice_move("E1") == "GIMove");
    CHECK(rules.voice_attack("E1") == "GIAttackCommand");
    CHECK(rules.die_sound("E1") == "GIDie");

    // E2 (CONS) combat values
    CHECK(rules.strength("E2") == 125);
    CHECK(rules.armor("E2") == "flak");
    CHECK(rules.primary("E2") == "M1Carbine");
    CHECK(rules.image("E2") == "CONS");
    CHECK(rules.voice_select("E2") == "ConscriptSelect");
    CHECK(rules.die_sound("E2") == "ConscriptDie");

    // M60 weapon values
    CHECK(rules.weapon_damage("M60") == 15);
    CHECK(rules.weapon_rof("M60") == 20);
    CHECK(rules.weapon_range("M60") == doctest::Approx(4.0));
    CHECK(rules.weapon_warhead("M60") == "SA");

    // M1Carbine weapon values
    CHECK(rules.weapon_damage("M1Carbine") == 15);
    CHECK(rules.weapon_rof("M1Carbine") == 25);
    CHECK(rules.weapon_range("M1Carbine") == doctest::Approx(4.0));
    CHECK(rules.weapon_warhead("M1Carbine") == "SA");

    // Vehicle getters (HTNK) + 120mm weapon range pin
    CHECK(rules.rot("HTNK") == 5);
    CHECK(rules.has_turret("HTNK"));
    CHECK(rules.explosion_first("HTNK") == "TWLT070");
    CHECK(rules.weapon_range("120mm") == doctest::Approx(5.75));

    // SA warhead verses (armor effectiveness multipliers)
    auto sa_verses = rules.verses("SA");
    REQUIRE(sa_verses.size() == 11);
    std::vector<int> expected_verses{100, 80, 80, 50, 25, 25, 75, 50, 25, 100, 100};
    CHECK(sa_verses == expected_verses);
}

TEST_CASE("rules: building accessors (synthetic)") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[General]\nBuildSpeed=.7\nRefundPercent=50%\nPrerequisitePower=GAPOWR,NAPOWR\n"
        "[MultiplayerDialogSettings]\nMoney=10000\n"
        "[GAPOWR]\nCost=800\nPower=200\nAdjacent=2\nTechLevel=1\nPrerequisite=GACNST\n"
        "[GACNST]\nConstructionYard=yes\nCost=3000\n"
        "[GAPILE]\nPrerequisite= power , GACNST\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.cost("GAPOWR") == 800);
    CHECK(r.power("GAPOWR") == 200);
    CHECK(r.power("MISSING") == 0);
    CHECK(r.adjacent("GAPOWR") == 2);
    CHECK(r.tech_level("GAPOWR") == 1);
    CHECK(r.is_construction_yard("GACNST"));
    CHECK_FALSE(r.is_construction_yard("GAPOWR"));
    auto pre = r.prerequisite("GAPILE");
    REQUIRE(pre.size() == 2);
    CHECK(pre[0] == "POWER");   // trimmed + uppercased
    CHECK(pre[1] == "GACNST");
    auto grp = r.prerequisite_group("Power");
    REQUIRE(grp.size() == 2);
    CHECK(grp[0] == "GAPOWR");
    CHECK(r.general_double("BuildSpeed", 0.0) == doctest::Approx(0.7));
    CHECK(r.general_double("RefundPercent", 0.0) == doctest::Approx(50.0));
    CHECK(r.general_double("Absent", 3.5) == doctest::Approx(3.5));
    CHECK(r.mp_money() == 10000);
}

TEST_CASE("rules: economy accessors") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[General]\nGrowthRate=5\n"
        "[Riparius]\nValue=25\nGrowthPercentage=.06\nSpreadPercentage=.06\n"
        "[HARV]\nStorage=40\nHarvester=yes\nDock=NAREFN,GAREFN\nUnloadingClass=HORV\n"
        "[NAREFN]\nStorage=200\nFreeUnit=HARV\nNumberOfDocks=1\n");
    auto r = ra2assets::Rules::load(ini);
    CHECK(r.value("Riparius") == 25);
    CHECK(r.growth_percentage("Riparius") == doctest::Approx(0.06));
    CHECK(r.spread_percentage("Riparius") == doctest::Approx(0.06));
    CHECK(r.storage("HARV") == 40);
    CHECK(r.harvester("HARV"));
    CHECK_FALSE(r.harvester("NAREFN"));
    CHECK(r.dock("HARV") == std::vector<std::string>{"NAREFN", "GAREFN"});
    CHECK(r.free_unit("NAREFN") == "HARV");
    CHECK(r.free_unit("HARV") == "");
    CHECK(r.number_of_docks("NAREFN") == 1);
    CHECK(r.growth_rate_minutes() == doctest::Approx(5.0));
    CHECK(r.unloading_class("HARV") == "HORV");
}

TEST_CASE("rules: real rulesmd building pins") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    std::vector<uint8_t> rules_bytes;
    if (const MixEntry* e = top.find(mix_id("rulesmd.ini"))) {
        rules_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",
            "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix", "conqmd.mix",
        };
        for (const char* name : candidates) {
            try {
                MixFile nested = top.open_nested(name);
                if (const MixEntry* e2 = nested.find(mix_id("rulesmd.ini"))) {
                    rules_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
            }
        }
    }
    if (rules_bytes.empty()) {
        MESSAGE("skipped: rulesmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    Rules rules = Rules::load(ini);

    CHECK(rules.cost("GAPOWR") == 800);
    CHECK(rules.cost("NAPOWR") == 600);
    CHECK(rules.cost("GAPILE") == 500);
    CHECK(rules.cost("GAWEAP") == 2000);
    CHECK(rules.cost("GACNST") == 3000);

    CHECK(rules.power("GAPOWR") == 200);
    CHECK(rules.power("NAPOWR") == 150);
    CHECK(rules.power("GAWEAP") == -25);
    CHECK(rules.power("NAHAND") == -10);
    CHECK(rules.power("GACNST") == 0);

    CHECK(rules.adjacent("GACNST") == 2);
    CHECK(rules.tech_level("GACNST") == -1);

    CHECK(rules.is_construction_yard("GACNST"));
    CHECK(rules.is_construction_yard("NACNST"));
    CHECK_FALSE(rules.is_construction_yard("GAPOWR"));

    auto gaweap_pre = rules.prerequisite("GAWEAP");
    std::vector<std::string> expected_pre{"PROC", "GAPILE", "GACNST"};
    CHECK(gaweap_pre == expected_pre);

    auto proc_group = rules.prerequisite_group("Proc");
    REQUIRE_FALSE(proc_group.empty());
    CHECK(proc_group[0] == "GAREFN");

    CHECK(rules.general_double("BuildSpeed", 0.0) == doctest::Approx(0.7));
    CHECK(rules.general_double("RefundPercent", 0.0) == doctest::Approx(50.0));
    CHECK(rules.general_double("LowPowerPenaltyModifier", 0.0) == doctest::Approx(1.0));
    CHECK(rules.general_double("ConditionYellow", 0.0) == doctest::Approx(50.0));
    CHECK(rules.general_double("ConditionRed", 0.0) == doctest::Approx(25.0));

    CHECK(rules.mp_money() == 10000);
}
