#include "doctest.h"
#include "ra2assets/hva.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic .hva builder. Layout (little-endian, packed; byte-verified against
// the real 88-byte htnk.hva = 16 + 4 + 4 + 16 + 48):
//   char path[16]                          @ 0   (content ignored by the loader)
//   u32  frame_count                       @ 16
//   u32  section_count                     @ 20
//   char section_names[section_count][16]  @ 24
//   f32  matrices[frames][sections][3][4]  frame-major, each matrix row-major
// Matrix value at (frame f, section s, row r, col c) = 100*f + 10*s + (4*r+c),
// so every slot is distinct and exactly representable in f32.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_hva(int frames, int sections) {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) p8((uint8_t)((v>>(8*i))&0xff)); };
    auto pf32 = [&](float f){ uint32_t v; std::memcpy(&v, &f, 4); p32(v); };
    auto pname = [&](const std::string& s){
        for (int i=0;i<16;i++) p8(i < (int)s.size() ? (uint8_t)s[i] : 0);
    };
    pname("test.vxl");                       // path (ignored)
    p32((uint32_t)frames);
    p32((uint32_t)sections);
    for (int s=0;s<sections;s++) pname("sec" + std::to_string(s));
    for (int f=0; f<frames; f++)
        for (int s=0; s<sections; s++)
            for (int r=0;r<3;r++)
                for (int c=0;c<4;c++)
                    pf32((float)(100*f + 10*s + 4*r + c));
    return b;
}

TEST_CASE("hva synthetic: names, frame count, frame-major matrices") {
    auto bytes = make_hva(2, 3);
    REQUIRE(bytes.size() == 24u + 16u*3 + 48u*2*3);   // 360
    HvaModel m = load_hva(bytes.data(), bytes.size());
    CHECK(m.frame_count == 2);
    REQUIRE(m.section_names.size() == 3);
    CHECK(m.section_names[0] == "sec0");
    CHECK(m.section_names[1] == "sec1");
    CHECK(m.section_names[2] == "sec2");
    REQUIRE(m.frames.size() == 2);
    REQUIRE(m.frames[0].size() == 3);
    REQUIRE(m.frames[1].size() == 3);
    CHECK(m.frames[0][0].transform[0][0] == 0.0f);
    CHECK(m.frames[0][1].transform[1][2] == 16.0f);    // 10*1 + 4*1+2
    CHECK(m.frames[1][2].transform[2][3] == 131.0f);   // 100 + 20 + 11
    CHECK(m.frames[1][0].transform[0][1] == 101.0f);   // 100 + 0 + 1
}

TEST_CASE("hva synthetic: size mismatches and bad counts throw") {
    auto good = make_hva(2, 3);
    // one byte short
    {
        auto b = good; b.pop_back();
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // one byte long
    {
        auto b = good; b.push_back(0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // smaller than the 24-byte header
    {
        std::vector<uint8_t> b(10, 0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // zero frames (bytes internally consistent: 24 + 16*3 + 0 matrix bytes)
    {
        auto b = make_hva(0, 3);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // zero sections
    {
        auto b = make_hva(2, 0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
}
