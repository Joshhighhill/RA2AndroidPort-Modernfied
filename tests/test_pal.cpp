#include "doctest.h"
#include "ra2assets/pal.hpp"

TEST_CASE("palette expands 6-bit channels to 8-bit by bit replication") {
    using namespace ra2assets;
    uint8_t raw[768] = {0};
    raw[0]=0;  raw[1]=63; raw[2]=32;      // index 0
    raw[3]=63; raw[4]=63; raw[5]=63;      // index 1 -> white
    Palette p = Palette::load(raw, 768);
    CHECK(p[0].r == 0);
    CHECK(p[0].g == 255);                 // (63<<2)|(63>>4) = 252|3 = 255
    CHECK(p[0].b == ((32<<2)|(32>>4)));   // 128|2 = 130
    CHECK(p[0].a == 255);
    CHECK(p[1].r == 255);
    CHECK(p[1].g == 255);
    CHECK(p[1].b == 255);
}
