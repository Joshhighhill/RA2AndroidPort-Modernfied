#include "doctest.h"
#include "../engine/ai_base.hpp"
#include "ra2assets/rules.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace ra2engine;

TEST_CASE("ai_base: ai_rand is deterministic xorshift32") {
    std::uint32_t a = 0x2545F491u, b = 0x2545F491u;
    for (int i = 0; i < 8; ++i) CHECK(ai_rand(a) == ai_rand(b));
    std::uint32_t s = 1u;
    CHECK(ai_rand(s) == 270369u);  // xorshift32(1): 1^<<13=8193, ^>>17=8193, ^<<5 -> 270369
}

TEST_CASE("ai_base: difficulty_mods resolves the three recovered groups") {
    // Groups hand-built FROM THE NAMED CONSTANTS [RE: ai-base.md §4] — the
    // constants now carry the REAL recovered values (not 1.0 placeholders);
    // the direct pins below keep them honest [RE-GATE APPLIED: ai-base.md §4].
    DifficultyGroupRaw g[3];
    for (int i = 0; i < 3; ++i) {
        g[i].cost = kDifficultyCost[i];
        g[i].build_time = kDifficultyBuildTime[i];
        g[i].build_delay = kDifficultyBuildDelayMin[i];
        g[i].build_slowdown = kDifficultyBuildSlowdown[i];
    }
    for (int i = 0; i < 3; ++i) {
        const DifficultyMods m = difficulty_mods(static_cast<AiDifficulty>(i), g);
        CHECK(m.cost_mul == doctest::Approx(kDifficultyCost[i]));
        CHECK(m.build_time_mul == doctest::Approx(kDifficultyBuildTime[i]));
        CHECK(m.build_delay_min == doctest::Approx(kDifficultyBuildDelayMin[i]));
        CHECK(m.build_slowdown == kDifficultyBuildSlowdown[i]);
        CHECK(m.defense_target == kAiDefenseTarget[i]);
    }
    // The REAL recovered per-difficulty values [RE: ai-base.md §4 — Easy/
    // Normal/Difficult: Cost 1.0/1.0/1.0, BuildTime 0.8/1.0/1.0,
    // BuildDelay(min) 0.03/0.03/0.1, BuildSlowdown no/yes/yes]:
    CHECK(kDifficultyCost[0] == doctest::Approx(1.0));
    CHECK(kDifficultyBuildTime[0] == doctest::Approx(0.8));      // Easy builds FASTER
    CHECK(kDifficultyBuildTime[1] == doctest::Approx(1.0));
    CHECK(kDifficultyBuildDelayMin[2] == doctest::Approx(0.1));  // Difficult waits longer
    CHECK_FALSE(kDifficultyBuildSlowdown[0]);
    CHECK(kDifficultyBuildSlowdown[2]);
    // Difficulty visibly separates the defense targets (Easy 6 < Difficult 25)
    // [RE: ai-base.md §2; RE-FALLBACK: count-array index binding].
    CHECK(kAiDefenseTarget[0] < kAiDefenseTarget[2]);
}

TEST_CASE("ai_base: AiBaseState defaults are the documented seeds") {
    AiBaseState s;
    CHECK(s.rng == 0x2545F491u);
    CHECK(s.plan.empty());
    CHECK(s.pending[0] == -1);
    CHECK(s.pending[1] == -1);
    CHECK(s.difficulty == AiDifficulty::Normal);  // default group [RE-FALLBACK: ai-base.md §4]
    CHECK(s.build_gate == 0.0);   // first build pass fires immediately [RE: ai-base.md §1]
    CHECK(s.build_clock == 0.0);
}

// --- Install-gated real-rulesmd pins (skip clean without an install, exactly
//     like tests/test_rules.cpp's install-gated cases) ---
static std::vector<uint8_t> slurp_ai(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("ai_base: real rulesmd [AI]/difficulty pins match the recovered constants") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp_ai(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    ra2assets::MixFile top = ra2assets::MixFile::open(std::move(top_bytes));

    std::vector<uint8_t> rules_bytes;
    if (const ra2assets::MixEntry* e = top.find(ra2assets::mix_id("rulesmd.ini"))) {
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
                ra2assets::MixFile nested = top.open_nested(name);
                if (const ra2assets::MixEntry* e2 = nested.find(ra2assets::mix_id("rulesmd.ini"))) {
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

    ra2assets::Ini ini = ra2assets::Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    ra2assets::Rules rules = ra2assets::Rules::load(ini);

    // [RE-GATE APPLIED: ai-base.md §2/§4 — these assert the REAL file values
    // equal the committed constants, so a rules/constant drift is caught.
    // Key spellings are the RECOVERED ones (PowerEmergency, NOT
    // PowerEmergencyFraction; no HarvestersPerRefinery exists in [AI]).]
    CHECK(rules.section_double("AI", "PowerSurplus", -1.0) ==
          doctest::Approx(ra2engine::kAiPowerSurplus));
    // PowerEmergency=75% -> strtod stops at '%' -> 75; the constant is the
    // fraction 0.75 [RE: ai-base.md §2].
    CHECK(rules.section_double("AI", "PowerEmergency", -1.0) ==
          doctest::Approx(ra2engine::kAiPowerEmergency * 100.0));
    CHECK(rules.section_double("AI", "BaseSizeAdd", -1.0) ==
          doctest::Approx((double)ra2engine::kAiBaseSizeAdd));
    CHECK(rules.section_double("AI", "AIBaseSpacing", -1.0) ==
          doctest::Approx((double)ra2engine::kAiBaseSpacing));
    CHECK(rules.section_double("AI", "RefineryRatio", -1.0) ==
          doctest::Approx(ra2engine::kAiRefineryRatio));
    CHECK(rules.section_double("AI", "RefineryLimit", -1.0) ==
          doctest::Approx((double)ra2engine::kAiRefineryLimit));
    CHECK(rules.section_double("AI", "WarRatio", -1.0) ==
          doctest::Approx(ra2engine::kAiWarRatio));
    CHECK(rules.section_double("AI", "WarLimit", -1.0) ==
          doctest::Approx((double)ra2engine::kAiWarLimit));
    CHECK(rules.section_double("AI", "DefenseRatio", -1.0) ==
          doctest::Approx(ra2engine::kAiDefenseRatio));
    CHECK(rules.section_double("AI", "DefenseLimit", -1.0) ==
          doctest::Approx((double)ra2engine::kAiDefenseLimit));
    CHECK(rules.section_double("AI", "CreditReserve", -1.0) ==
          doctest::Approx(ra2engine::kAiCreditReserve));
    CHECK(rules.section_double("General", "AISafeDistance", -1.0) ==
          doctest::Approx((double)ra2engine::kAiSafeDistance));
    CHECK(rules.section_double("General", "BaseBias", -1.0) ==
          doctest::Approx(ra2engine::kAiBaseBias));
    // Difficulty groups: the RECOVERED section names [Easy]/[Normal]/
    // [Difficult] (NOT Medium/Hard) -> Rules+0x1538/+0x1588/+0x15d8
    // [RE: ai-base.md §4]:
    const char* kGroups[3] = {"Easy", "Normal", "Difficult"};  // [RE: ai-base.md §4]
    for (int i = 0; i < 3; ++i) {
        CHECK(rules.section_double(kGroups[i], "Cost", -1.0) ==
              doctest::Approx(ra2engine::kDifficultyCost[i]));
        CHECK(rules.section_double(kGroups[i], "BuildTime", -1.0) ==
              doctest::Approx(ra2engine::kDifficultyBuildTime[i]));
        CHECK(rules.section_double(kGroups[i], "BuildDelay", -1.0) ==
              doctest::Approx(ra2engine::kDifficultyBuildDelayMin[i]));
    }
}
