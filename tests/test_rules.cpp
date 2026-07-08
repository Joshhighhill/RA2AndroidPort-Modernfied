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
