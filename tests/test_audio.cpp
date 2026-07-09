#include "doctest.h"
#include "ra2assets/audio_bag.hpp"
#include "ra2assets/mix.hpp"     // used by the Task-4 real-asset oracle test
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>            // used by the Task-4 real-asset oracle test
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic audio.idx builder. On-disk layout (little-endian, packed) as
// verified against the real install (12 + 1153*36 = 41,520 bytes exactly) and
// XCC's t_audio_idx_header/t_audio_idx_entry:
//   header: char magic[4]="GABA"; u32 version(=2); u32 count
//   entry : char fname[16] (NUL-padded); u32 offset; u32 size;
//           u32 sample_rate; u32 flags; u32 chunk_size
// ---------------------------------------------------------------------------
struct IdxEntrySpec {
    std::string name;
    uint32_t offset, size, sample_rate, flags, chunk_size;
};

static void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)((v >> (8 * i)) & 0xff));
}

static std::vector<uint8_t> make_idx(const std::vector<IdxEntrySpec>& entries) {
    std::vector<uint8_t> b;
    b.push_back('G'); b.push_back('A'); b.push_back('B'); b.push_back('A');
    put32(b, 2);                                  // version
    put32(b, (uint32_t)entries.size());           // count
    for (const IdxEntrySpec& e : entries) {
        for (size_t i = 0; i < 16; i++)           // fname[16], NUL-padded
            b.push_back(i < e.name.size() ? (uint8_t)e.name[i] : 0);
        put32(b, e.offset); put32(b, e.size);
        put32(b, e.sample_rate); put32(b, e.flags); put32(b, e.chunk_size);
    }
    return b;
}

TEST_CASE("audio idx: valid header parses, order preserved") {
    auto b = make_idx({
        {"IchRatta", 0, 71148, 22050, 12, 512},
        {"gradio3f", 71148, 512, 22050, 12, 512},
    });
    REQUIRE(b.size() == 12 + 2 * 36);             // mirror XCC is_valid arithmetic
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    REQUIRE(idx.clips().size() == 2);
    CHECK(idx.clips()[0].name == "IchRatta");     // raw on-disk name preserved
    CHECK(idx.clips()[0].offset == 0);
    CHECK(idx.clips()[0].size == 71148);
    CHECK(idx.clips()[0].sample_rate == 22050);
    CHECK(idx.clips()[0].flags == 12);
    CHECK(idx.clips()[0].chunk_size == 512);
    CHECK(idx.clips()[1].name == "gradio3f");     // order preserved
    CHECK(idx.clips()[1].offset == 71148);
}

TEST_CASE("audio idx: find is case-insensitive, absent -> nullptr") {
    auto b = make_idx({{"IchRatta", 0, 100, 22050, 12, 512}});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    const BagClip* c1 = idx.find("ichratta");
    const BagClip* c2 = idx.find("ICHRATTA");
    const BagClip* c3 = idx.find("IchRatta");
    REQUIRE(c1 != nullptr);
    CHECK(c1 == c2);
    CHECK(c1 == c3);
    CHECK(c1->size == 100);
    CHECK(idx.find("nope") == nullptr);
    CHECK(idx.find("") == nullptr);
    CHECK(idx.find("ichratta_longer_than_16") == nullptr);
}

TEST_CASE("audio idx: 16-char name (no NUL terminator on disk)") {
    auto b = make_idx({{"abcdefghijklmnop", 4, 8, 22050, 12, 512}});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    REQUIRE(idx.clips().size() == 1);
    CHECK(idx.clips()[0].name == "abcdefghijklmnop");
    CHECK(idx.find("ABCDEFGHIJKLMNOP") == &idx.clips()[0]);
}

TEST_CASE("audio idx: empty index (count=0) is valid") {
    auto b = make_idx({});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    CHECK(idx.clips().empty());
    CHECK(idx.find("anything") == nullptr);
}

TEST_CASE("audio idx: validation rejects malformed input") {
    // Too small for even the 12-byte header.
    {
        std::vector<uint8_t> b = {'G', 'A', 'B', 'A', 2, 0, 0, 0};
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong magic.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[0] = 'X';
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong version (XCC is_valid: h.two != 2 -> invalid).
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[4] = 1;
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Size mismatch: trailing byte (XCC: 12 + count*36 must EQUAL the size).
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b.push_back(0);
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Size mismatch: truncated final entry.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b.pop_back();
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Count claims more entries than the bytes hold.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[8] = 200;                                // count = 200, size unchanged
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
}
