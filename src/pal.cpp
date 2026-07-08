#include "ra2assets/pal.hpp"
#include <stdexcept>

namespace ra2assets {
static uint8_t expand6(uint8_t v){ v &= 63; return (uint8_t)((v << 2) | (v >> 4)); }

Palette Palette::load(const uint8_t* data, size_t len) {
    if (len < 768) throw std::runtime_error("pal: need 768 bytes");
    Palette p;
    for (int i = 0; i < 256; i++) {
        p.c_[i].r = expand6(data[i*3+0]);
        p.c_[i].g = expand6(data[i*3+1]);
        p.c_[i].b = expand6(data[i*3+2]);
        p.c_[i].a = 255;
    }
    return p;
}
}
