#include "doctest.h"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/sounds.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace ra2assets;

static const char* kSyntheticSound =
    "[Defaults]\n"
    "MinVolume=50\nRange=10\nVolume=80\nLimit=5\n"
    "Type= NORMAL SCREEN UNSHROUD\nPriority=NORMAL\n"
    "\n"
    "[GIAttack]\n"
    "Sounds=igiat1a igiat1b igiat1c\n"
    "Control= random interrupt\n"
    "VShift=15\nVolume=65\n"
    "\n"
    "[GIDie]\n"
    "Sounds= $igidia $igidib\n"
    "Priority=low\nControl= random\nFShift= -10 10\nVolume=85\n"
    "\n"
    "[HashCase]\n"
    "Sounds= #habc $hdef\n";

TEST_CASE("sounds: entry inherits [Defaults] for absent keys") {
    Ini ini = Ini::parse(kSyntheticSound);
    Sounds snd = Sounds::load(ini);
    SoundEntry e = snd.entry("GIAttack");
    CHECK(e.volume == 65);      // overridden
    CHECK(e.limit == 5);        // from [Defaults]
    CHECK(e.range == doctest::Approx(10.0));
    CHECK(e.min_volume == 50);
    CHECK(e.priority == "normal");
    REQUIRE(e.type.size() == 3);
    CHECK(e.type[0] == "normal");
    CHECK(e.type[1] == "screen");
    CHECK(e.type[2] == "unshroud");
}

TEST_CASE("sounds: variant list, $ stripping, control tokens, shifts") {
    Ini ini = Ini::parse(kSyntheticSound);
    Sounds snd = Sounds::load(ini);
    SoundEntry a = snd.entry("GIAttack");
    REQUIRE(a.sounds.size() == 3);
    CHECK(a.sounds[0] == "igiat1a");
    CHECK(a.sounds[2] == "igiat1c");
    REQUIRE(a.control.size() == 2);
    CHECK(a.control[0] == "random");
    CHECK(a.control[1] == "interrupt");
    CHECK(a.vshift == 15);
    SoundEntry d = snd.entry("GIDie");
    REQUIRE(d.sounds.size() == 2);
    CHECK(d.sounds[0] == "igidia");   // '$' stripped
    CHECK(d.priority == "low");
    CHECK(d.fshift_lo == -10);
    CHECK(d.fshift_hi == 10);
    // '#' strips exactly like '$' (docs/re/audio-playback.md finding 1: the
    // real add_sound() strips BOTH leading sigils, not just '$'):
    SoundEntry hc = snd.entry("HashCase");
    REQUIRE(hc.sounds.size() == 2);
    CHECK(hc.sounds[0] == "habc");
    CHECK(hc.sounds[1] == "hdef");
    CHECK(snd.has("GIDie"));
    CHECK_FALSE(snd.has("NoSuchSound"));
    SoundEntry missing = snd.entry("NoSuchSound");   // -> defaults verbatim
    CHECK(missing.sounds.empty());
    CHECK(missing.volume == 80);
    CHECK(missing.limit == 5);
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("sounds: real soundmd.ini pins") {
    // Install guard + ra2md.mix nested-candidate loading: same shape as
    // tests/test_art.cpp's real-artmd test / tests/test_rules.cpp's
    // real-rulesmd test, but for "soundmd.ini" (lives in the same
    // localmd.mix as artmd.ini/rulesmd.ini).
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    // soundmd.ini is NOT a top-level entry of ra2md.mix in a stock YR 1.001
    // install -- it lives inside a nested mix. Try the top level first (some
    // mod layouts flatten it there), then descend the usual homes: the local
    // assets mix and the expansion mixes (same fallback chain as
    // tests/test_art.cpp / tests/test_rules.cpp's real-asset tests).
    std::vector<uint8_t> sound_bytes;
    if (const MixEntry* e = top.find(mix_id("soundmd.ini"))) {
        sound_bytes = top.read(*e);
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
                if (const MixEntry* e2 = nested.find(mix_id("soundmd.ini"))) {
                    sound_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (sound_bytes.empty()) {
        MESSAGE("skipped: soundmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(sound_bytes.begin(), sound_bytes.end()));
    Sounds snd = Sounds::load(ini);
    SoundEntry def = snd.entry("NoSuchSound__");
    CHECK(def.volume == 80);
    CHECK(def.limit == 5);
    CHECK(def.range == doctest::Approx(10.0));
    CHECK(def.min_volume == 50);
    SoundEntry gia = snd.entry("GIAttack");
    REQUIRE(gia.sounds.size() == 3);
    CHECK(gia.sounds[0] == "igiat1a");
    CHECK(gia.volume == 65);
    CHECK(gia.vshift == 15);
    REQUIRE(gia.control.size() == 2);
    CHECK(gia.control[1] == "interrupt");
    SoundEntry cona = snd.entry("ConscriptAttack");
    CHECK(cona.sounds.size() == 7);
    CHECK(cona.limit == 3);
    SoundEntry gid = snd.entry("GIDie");
    CHECK(gid.sounds.size() == 5);
    CHECK(gid.sounds[0] == "igidia");
    CHECK(gid.priority == "low");
    CHECK(gid.fshift_lo == -10);
    CHECK(gid.fshift_hi == 10);
    SoundEntry gis = snd.entry("GISelect");
    CHECK(gis.sounds.size() == 6);
    CHECK(gis.volume == 85);
}
