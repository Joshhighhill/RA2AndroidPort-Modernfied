#include "doctest.h"
#include "ra2assets/bink.hpp"
#include "ra2assets/mix.hpp"
#include "asset_fixture.hpp"
#include <cstdint>
#include <vector>

using namespace ra2assets;

// Synthetic minimal BIKi container: 44-byte header + a 3-entry frame index for
// a 2-frame clip. Exercises the header field decode, the bit0 keyframe flag,
// and the frame() byte-range mask without needing the real install.
static std::vector<uint8_t> tiny_bik() {
    auto put32 = [](std::vector<uint8_t>& v, uint32_t x) {
        v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8));
        v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 24));
    };
    std::vector<uint8_t> v;
    v.push_back('B'); v.push_back('I'); v.push_back('K'); v.push_back('i');  // 0x00 magic
    put32(v, 0);            // 0x04 filesize-8 (unused by parser)
    put32(v, 2);            // 0x08 num_frames
    put32(v, 8);            // 0x0C largest frame
    put32(v, 2);            // 0x10 num_frames again
    put32(v, 64);           // 0x14 width
    put32(v, 48);           // 0x18 height
    put32(v, 30);           // 0x1C fps_num
    put32(v, 1);            // 0x20 fps_den
    put32(v, 0);            // 0x24 video flags
    put32(v, 0);            // 0x28 audio track count
    // frame index at 0x2C: 3 entries (num_frames+1). First frame at 0x2C+12=56,
    // keyframe (bit0 set); second at 60; end == total size.
    const uint32_t idx0 = 56;
    put32(v, idx0 | 1u);    // frame 0: offset 56, keyframe
    put32(v, 60);           // frame 1: offset 60, not a keyframe
    put32(v, 64);           // end sentinel == file size
    v.resize(64, 0xAB);     // frame payload bytes
    return v;
}

TEST_CASE("bink: synthetic BIKi header + frame index + keyframe flag") {
    BinkFile bf = BinkFile::parse(tiny_bik());
    CHECK(bf.width == 64);
    CHECK(bf.height == 48);
    CHECK(bf.num_frames == 2);
    CHECK(bf.fps_num == 30);
    CHECK(bf.fps_den == 1);
    REQUIRE(bf.frame_offsets.size() == 3);
    CHECK(bf.is_keyframe(0) == true);
    CHECK(bf.is_keyframe(1) == false);
    auto f0 = bf.frame(0);
    CHECK(f0.first == bf.data.data() + 56);   // bit0 masked off (57 -> 56)
    CHECK(f0.second == 4);                     // [56, 60)
    CHECK(bf.frame_offsets.back() == bf.data.size());
}

TEST_CASE("bink: bad magic / truncation throw") {
    std::vector<uint8_t> bad(64, 0);
    CHECK_THROWS_AS(BinkFile::parse(bad), std::runtime_error);          // bad magic
    std::vector<uint8_t> shortbuf = {'B', 'I', 'K', 'i'};
    CHECK_THROWS_AS(BinkFile::parse(shortbuf), std::runtime_error);     // short header
}

// Real asset: ra2ts_s.bik is a top-level entry in language.mix (hash C1E6E166 /
// name "ra2ts_s.bik"). Skip cleanly if the install / language.mix is absent.
TEST_CASE("bink: real ra2ts_s.bik container (language.mix)") {
    const std::vector<uint8_t>& lang = ra2fixture::mix_bytes_by_name("language.mix");
    if (lang.empty()) { MESSAGE("language.mix absent; skipped"); return; }
    const MixFile& mix = ra2fixture::mix_by_name("language.mix");
    const MixEntry* e = mix.find("ra2ts_s.bik");
    REQUIRE(e != nullptr);
    auto bytes = mix.read(*e);
    const std::size_t filesize = bytes.size();

    BinkFile bf = BinkFile::parse(std::move(bytes));
    CHECK(bf.width == 472);
    CHECK(bf.height == 450);
    CHECK(bf.num_frames == 431);
    CHECK(bf.fps_num == 15);
    CHECK(bf.fps_den == 1);
    REQUIRE(bf.frame_offsets.size() == 432);
    CHECK(bf.is_keyframe(0) == true);
    CHECK(bf.is_keyframe(1) == false);
    auto f0 = bf.frame(0);
    CHECK(f0.second > 0);                          // frame 0 non-empty
    CHECK(bf.frame_offsets.back() == filesize);    // last index entry == file size
    CHECK(filesize == 7184988u);
}
