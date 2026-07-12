#include "doctest.h"
#include "../engine/aimd_parse.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace ra2engine;

namespace {
// Real aimd.ini shapes [RE-GATE APPLIED: ai-team.md §1 — index sections
// (index=id) + numbered entity entries; ids/values here are fixture stand-ins
// in the recovered FORMAT].
const char* kAimdFixture =
    "[TaskForces]\n0=01000050-G\n1=01000051-G\n2=09999999-G\n"
    "[ScriptTypes]\n0=03000015-G\n"
    "[TeamTypes]\n0=02000010-G\n1=02000011-G\n2=02000012-G\n"
    "[01000050-G]\nName=Rhino Pair\nGroup=-1\n0=2,HTNK\n1=1,E2\n"
    "[01000051-G]\nName=Ghost Force\n0=1,ZZNOART\n"   // unknown UNITID -> dropped
    "[03000015-G]\nName=March And Attack\n0=3,2\n1=0,1\n2=49,0\n"
    "[02000010-G]\nName=Sov Rhino Rush\nHouse=<none>\nPriority=7\nMax=2\n"
    "TaskForce=01000050-G\nScript=03000015-G\nAutocreate=yes\nFull=no\nWaypoint=C\n"
    "Aggressive=yes\nReinforce=no\nSuicide=no\nTechLevel=5\nIsBaseDefense=no\n"
    "[02000011-G]\nName=Ghost Team\nHouse=Russians\nTaskForce=01000051-G\n"
    "Script=03000015-G\nAutocreate=yes\n"              // rides the dropped TF
    "[02000012-G]\nName=No Script\nHouse=Russians\nTaskForce=01000050-G\n"
    "Script=09999999-G\nAutocreate=yes\n";             // missing script -> dropped

// Test resolver: HTNK->0, E2->1, everything else unknown (-1).
int test_unit_index(const std::string& id) {
    if (id == "HTNK") return 0;
    if (id == "E2") return 1;
    return -1;
}
}  // namespace

TEST_CASE("aimd: waypoint_from_letter decodes the letter encoding") {
    CHECK(waypoint_from_letter("A") == 0);
    CHECK(waypoint_from_letter("C") == 2);
    CHECK(waypoint_from_letter("Z") == 25);
    CHECK(waypoint_from_letter("AA") == 26);   // [RE-GATE APPLIED: ai-team.md §1]
    CHECK(waypoint_from_letter("") == -1);
    CHECK(waypoint_from_letter("7") == -1);
}

TEST_CASE("aimd: parse_aimd walks the index sections and numbered entries") {
    ra2assets::Ini ini = ra2assets::Ini::parse(kAimdFixture);
    const AimdData d = parse_aimd(ini, test_unit_index);
    // TaskForces: Rhino Pair kept, Ghost Force art-guard-dropped, the
    // dangling index entry 2=09999999-G (no section) skipped.
    REQUIRE(d.taskforces.size() == 1);
    CHECK(d.taskforces[0].id == "01000050-G");
    CHECK(d.taskforces[0].name == "Rhino Pair");
    CHECK(d.taskforces[0].group == -1);
    REQUIRE(d.taskforces[0].members.size() == 2);
    CHECK(d.taskforces[0].members[0].count == 2);   // 0=2,HTNK
    CHECK(d.taskforces[0].members[0].unit == 0);
    CHECK(d.taskforces[0].members[1].count == 1);   // 1=1,E2
    CHECK(d.taskforces[0].members[1].unit == 1);
    // ScriptTypes: all actions kept verbatim (inert split is the interpreter's).
    REQUIRE(d.scripts.size() == 1);
    REQUIRE(d.scripts[0].actions.size() == 3);
    CHECK(d.scripts[0].actions[0].action == 3);     // 0=3,2
    CHECK(d.scripts[0].actions[0].argument == 2);
    CHECK(d.scripts[0].actions[1].action == 0);     // 1=0,1
    CHECK(d.scripts[0].actions[1].argument == 1);
    CHECK(d.scripts[0].actions[2].action == 49);    // 2=49,0
    // TeamTypes: only the team whose TaskForce AND Script survived.
    REQUIRE(d.teams.size() == 1);
    const TeamType& t = d.teams[0];
    CHECK(t.id == "02000010-G");
    CHECK(t.house == "<none>");   // universal in stock data [RE: ai-team.md §1]
    CHECK(t.priority == 7);
    CHECK(t.max == 2);
    CHECK(t.taskforce == 0);
    CHECK(t.script == 0);
    CHECK(t.autocreate);
    CHECK_FALSE(t.full);
    CHECK(t.waypoint == 2);   // Waypoint=C
    // New load-bearing / informational TeamType fields [RE: ai-team.md §1]:
    CHECK(t.aggressive);
    CHECK_FALSE(t.reinforce);
    CHECK_FALSE(t.suicide);
    CHECK(t.tech_level == 5);
    CHECK_FALSE(t.is_base_defense);
}

TEST_CASE("aimd: parse_aimd is deterministic") {
    ra2assets::Ini ini = ra2assets::Ini::parse(kAimdFixture);
    const AimdData a = parse_aimd(ini, test_unit_index);
    const AimdData b = parse_aimd(ini, test_unit_index);
    REQUIRE(a.teams.size() == b.teams.size());
    REQUIRE(a.taskforces.size() == b.taskforces.size());
    for (std::size_t i = 0; i < a.teams.size(); ++i) CHECK(a.teams[i].id == b.teams[i].id);
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("aimd: real aimd.ini parses with resolved cross-references") {
    using namespace ra2assets;
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    // aimd.ini is NOT a top-level entry of ra2md.mix in a stock YR 1.001
    // install -- it lives inside a nested mix. Try the top level first, then
    // descend the usual homes: the local assets mix and the expansion mixes
    // (same walk as tests/test_rules.cpp's real-rulesmd.ini case).
    std::vector<uint8_t> bytes;
    if (const MixEntry* e = top.find(mix_id("aimd.ini"))) {
        bytes = top.read(*e);
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
                if (const MixEntry* e2 = nested.find(mix_id("aimd.ini"))) {
                    bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (bytes.empty()) {
        MESSAGE("skipped: aimd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    ra2assets::Ini ini = ra2assets::Ini::parse(std::string(bytes.begin(), bytes.end()));
    REQUIRE(ini.has("TeamTypes"));
    REQUIRE(ini.has("TaskForces"));
    REQUIRE(ini.has("ScriptTypes"));
    // Permissive resolver (every UNITID "known") -> structure-only smoke:
    const AimdData d = parse_aimd(ini, [](const std::string&) { return 0; });
    CHECK(d.taskforces.size() > 50);   // the real file carries hundreds
    CHECK(d.scripts.size() > 50);
    CHECK(d.teams.size() > 50);
    int autocreate = 0;
    for (const TeamType& t : d.teams) {
        REQUIRE(t.taskforce >= 0);
        REQUIRE(t.taskforce < (int)d.taskforces.size());
        REQUIRE(t.script >= 0);
        REQUIRE(t.script < (int)d.scripts.size());
        if (t.autocreate) ++autocreate;
    }
    CHECK(autocreate > 0);   // the autocreate pool AI-2 draws from is nonempty
    MESSAGE("real aimd: tf=" << d.taskforces.size() << " sc=" << d.scripts.size()
            << " teams=" << d.teams.size() << " autocreate=" << autocreate);
}
