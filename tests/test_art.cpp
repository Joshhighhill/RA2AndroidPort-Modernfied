#include "doctest.h"
#include "ra2assets/art.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using ra2assets::Art;
using ra2assets::ArtSequenceEntry;
using ra2assets::Ini;
using namespace ra2assets;

static const char* kSyntheticArt =
    "[GI] ; GI\n"
    "Sequence=GISequence\n"
    "\n"
    "[GISequence]\n"
    "Ready=0,1,1\n"
    "Walk=8,6,6\n"
    "FireUp=164,6,6\n"
    "Die1=134,15,0\n"
    "Idle1=56,15,0,S\n"          // 4th field (facing label) must be tolerated
    "Cheer= 293 , 8 , 0 ,E\n";   // embedded spaces tolerated

TEST_CASE("art: sequence_name resolves [image] Sequence=") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    CHECK(art.sequence_name("GI") == "GISequence");
    CHECK(art.sequence_name("gi") == "GISequence");  // section lookup case-insensitive
    CHECK(art.sequence_name("NOPE") == "");
}

TEST_CASE("art: entry parses Start,Count,FacingMult and tolerates extras") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    ArtSequenceEntry fire = art.entry("GI", "FireUp");
    CHECK(fire.start == 164);
    CHECK(fire.count == 6);
    CHECK(fire.facing_mult == 6);
    ArtSequenceEntry die = art.entry("GI", "Die1");
    CHECK(die.start == 134);
    CHECK(die.count == 15);
    CHECK(die.facing_mult == 0);
    ArtSequenceEntry idle = art.entry("GI", "Idle1");   // 4-field line
    CHECK(idle.start == 56);
    CHECK(idle.count == 15);
    CHECK(idle.facing_mult == 0);
    ArtSequenceEntry cheer = art.entry("GI", "Cheer");  // spaced fields
    CHECK(cheer.start == 293);
    CHECK(cheer.count == 8);
    CHECK(cheer.facing_mult == 0);
}

TEST_CASE("art: absent image/section/key -> defaults {0,1,0}") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    ArtSequenceEntry e = art.entry("NOPE", "FireUp");
    CHECK(e.start == 0);
    CHECK(e.count == 1);
    CHECK(e.facing_mult == 0);
    ArtSequenceEntry e2 = art.entry("GI", "NoSuchEntry");
    CHECK(e2.start == 0);
    CHECK(e2.count == 1);
    CHECK(e2.facing_mult == 0);
}

TEST_CASE("art: voxel getters") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[HTNK]\nVoxel=yes\nRemapable=yes\nPrimaryFireFLH=150,0,100\n"
        "[GI]\nCameo=GIICON\n");
    ra2assets::Art a = ra2assets::Art::load(ini);
    CHECK(a.voxel("HTNK"));
    CHECK_FALSE(a.voxel("GI"));
    CHECK(a.remapable("HTNK"));
    int flh[3] = {-1, -1, -1};
    CHECK(a.fire_flh("HTNK", flh));
    CHECK(flh[0] == 150); CHECK(flh[1] == 0); CHECK(flh[2] == 100);
    int flh2[3] = {-1, -1, -1};
    CHECK_FALSE(a.fire_flh("GI", flh2));
    CHECK(flh2[0] == -1);
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("art: real artmd.ini pins (GISequence == ConSequence for our entries)") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    // artmd.ini is NOT a top-level entry of ra2md.mix in a stock YR 1.001
    // install -- it lives inside a nested mix. Try the top level first (some
    // mod layouts flatten it there), then descend the usual homes: the local
    // assets mix and the expansion mixes (same fallback chain as
    // tests/test_rules.cpp's real-asset test / engine/main.cpp:150's
    // load_rules_ini).
    std::vector<uint8_t> art_bytes;
    if (const MixEntry* e = top.find(mix_id("artmd.ini"))) {
        art_bytes = top.read(*e);
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
                if (const MixEntry* e2 = nested.find(mix_id("artmd.ini"))) {
                    art_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (art_bytes.empty()) {
        MESSAGE("skipped: artmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(art_bytes.begin(), art_bytes.end()));
    Art art = Art::load(ini);
    CHECK(art.sequence_name("GI") == "GISequence");
    CHECK(art.sequence_name("CONS") == "ConSequence");
    CHECK(art.voxel("HTNK"));
    CHECK(art.remapable("HTNK")); /* fire_flh pins 150,0,100 */
    for (const char* img : {"GI", "CONS"}) {
        CHECK(art.entry(img, "Ready").start == 0);
        CHECK(art.entry(img, "Ready").count == 1);
        CHECK(art.entry(img, "Ready").facing_mult == 1);
        CHECK(art.entry(img, "Walk").start == 8);
        CHECK(art.entry(img, "Walk").count == 6);
        CHECK(art.entry(img, "Walk").facing_mult == 6);
        CHECK(art.entry(img, "FireUp").start == 164);
        CHECK(art.entry(img, "FireUp").count == 6);
        CHECK(art.entry(img, "FireUp").facing_mult == 6);
        CHECK(art.entry(img, "Die1").start == 134);
        CHECK(art.entry(img, "Die1").count == 15);
        CHECK(art.entry(img, "Die1").facing_mult == 0);
    }
}
