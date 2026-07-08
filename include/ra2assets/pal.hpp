#pragma once
#include <cstdint>
#include <cstddef>

namespace ra2assets {
struct Rgba { uint8_t r,g,b,a; };
class Palette {
public:
    static Palette load(const uint8_t* data, size_t len);
    const Rgba& operator[](int i) const { return c_[i]; }
private:
    Rgba c_[256];
};
}
