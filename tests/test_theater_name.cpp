#include "doctest.h"
#include "ra2assets/theater_name.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using ra2assets::theater_candidates;
using ra2assets::MixFile;
using ra2assets::MixEntry;
namespace fs = std::filesystem;

TEST_CASE("theater_candidates: temperate GACNST order") {
    auto c = theater_candidates("GACNST", 'T', "isotemp.mix", "temperat.mix", ".shp");
    REQUIRE(c.size() == 6);            // [gtcnst x3 mixes, ggcnst x3 mixes]
    CHECK(c[0].mix == "isotemp.mix");   CHECK(c[0].file == "gtcnst.shp");
    CHECK(c[1].mix == "temperat.mix");  CHECK(c[1].file == "gtcnst.shp");
    CHECK(c[2].mix == "generic.mix");   CHECK(c[2].file == "gtcnst.shp");
    CHECK(c[3].mix == "isotemp.mix");   CHECK(c[3].file == "ggcnst.shp");
    CHECK(c[5].mix == "generic.mix");   CHECK(c[5].file == "ggcnst.shp");
}

TEST_CASE("theater_candidates: snow letter A themes to the literal spelling") {
    auto c = theater_candidates("GACNST", 'A', "isosnow.mix", "snow.mix", ".shp");
    REQUIRE(c.size() == 6);
    CHECK(c[0].file == "gacnst.shp");   // letter A == literal spelling
    CHECK(c[1].mix == "snow.mix");      // where the real file lives
    CHECK(c[3].file == "ggcnst.shp");
}

TEST_CASE("theater_candidates: non-NewTheater-pattern name is literal only") {
    // First-two-chars pattern {g,n,c,y}x{a,t} not matched -> no munge
    // [RE-GATE: building-render.md §1 FUN_005F8EA0 pattern check].
    auto c = theater_candidates("XXICON", 'T', "isotemp.mix", "temperat.mix", ".shp");
    REQUIRE(c.size() == 3);
    CHECK(c[0].file == "xxicon.shp");
    CHECK(c[2].mix == "generic.mix");
}

TEST_CASE("theater_candidates: real-install resolution pins") {
    // Guarded (install_dir pattern from test_vpl.cpp). Open ra2.mix; walk the
    // temperate candidates for GACNST(.shp): the FIRST candidate that
    // resolves must be generic.mix/ggcnst.shp (63,760 bytes), proving both
    // the fallback order and the empirical layout. Then GACNSTMK: first hit
    // must be isotemp.mix/gtcnstmk.shp (428,080 bytes).
    const char* id = std::getenv("RA2_INSTALL_DIR");
    fs::path install = id ? id : "D:\\Red Alert 2";
    if (!fs::exists(install / "ra2.mix")) {
        MESSAGE("RA2 install absent; skipping");
        return;
    }

    std::ifstream f(install / "ra2.mix", std::ios::binary);
    std::vector<uint8_t> mixbytes((std::istreambuf_iterator<char>(f)), {});
    REQUIRE_FALSE(mixbytes.empty());
    MixFile outer = MixFile::open(std::move(mixbytes));

    // Open the nested mixes that the candidate walk order names, keyed by
    // lowercase filename so lookups below can match theater_candidates()'s
    // lowercase mix names directly.
    std::vector<std::pair<std::string, MixFile>> open_mixes;
    for (const char* name : {"generic.mix", "isotemp.mix", "temperat.mix"}) {
        try {
            open_mixes.emplace_back(name, outer.open_nested(name));
        } catch (const std::exception&) {
            // Nested mix not present in this install/layout; skip it.
        }
    }
    REQUIRE_FALSE(open_mixes.empty());

    auto resolve_first = [&](const std::string& image, const std::string& ext)
        -> std::pair<std::string, size_t> {
        auto candidates = theater_candidates(image, 'T', "isotemp.mix", "temperat.mix", ext);
        for (const auto& cand : candidates) {
            for (auto& [mix_name, mix_file] : open_mixes) {
                if (mix_name != cand.mix) continue;
                if (const MixEntry* e = mix_file.find(cand.file)) {
                    return {cand.mix, e->size};
                }
            }
        }
        return {"", 0};
    };

    auto [cnst_mix, cnst_size] = resolve_first("GACNST", ".shp");
    CHECK(cnst_mix == "generic.mix");
    CHECK(cnst_size == 63760);

    auto [cnstmk_mix, cnstmk_size] = resolve_first("GACNSTMK", ".shp");
    CHECK(cnstmk_mix == "isotemp.mix");
    CHECK(cnstmk_size == 428080);
}
