// libm cross-platform probe: hashes the bit patterns of the transcendental
// functions the RA2 sim calls (world/projectile/rocket/combat/ai_team) over
// deterministic inputs in the ranges the sim uses; prints per-function hash +
// a dump file of every result so two platforms can be diffed exactly.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static uint64_t bits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }
static uint32_t bitsf(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static uint64_t fnv(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 0xff; h *= 1099511628211ull; }
    return h;
}
// xorshift64 deterministic input generator
static uint64_t s = 0x9E3779B97F4A7C15ull;
static uint64_t nx() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
static double unit() { return (nx() >> 11) * (1.0 / 9007199254740992.0); }

int main(int argc, char** argv) {
    const int N = argc > 1 ? std::atoi(argv[1]) : 1000000;
    FILE* dump = argc > 2 ? std::fopen(argv[2], "wb") : nullptr;
    const char* names[] = {"sin", "cos", "atan2", "acos", "hypot", "pow", "fmod", "sqrt", "sinf"};
    uint64_t h[9];
    for (auto& x : h) x = 1469598103934665603ull;
    for (int i = 0; i < N; ++i) {
        // angles: radians in [-4pi, 4pi] (yaw/pitch/theta in projectile/rocket)
        const double a = (unit() * 8.0 - 4.0) * 3.141592653589793;
        // deltas in cells/leptons: [-30000, 30000]
        const double dx = unit() * 60000.0 - 30000.0, dy = unit() * 60000.0 - 30000.0;
        const double c = unit() * 2.0 - 1.0;
        const double base = unit(), ex = (double)(nx() % 8);
        double r[9];
        r[0] = std::sin(a);
        r[1] = std::cos(a);
        r[2] = std::atan2(dy, dx);
        r[3] = std::acos(c);
        r[4] = std::hypot(dx, dy);
        r[5] = std::pow(1.0 / (1.0 + base), ex);
        r[6] = std::fmod(std::atan2(dx, -dy) * (65536.0 / (2.0 * 3.141592653589793)) + 65536.0, 65536.0);
        r[7] = std::sqrt(dx * dx + dy * dy);
        r[8] = (double)std::sin((float)(nx() % 8192) * 6.2831855f / 8192.0f);
        for (int k = 0; k < 9; ++k) h[k] = fnv(h[k], k == 8 ? bitsf((float)r[8]) : bits(r[k]));
        if (dump) std::fwrite(r, sizeof r, 1, dump);
    }
    if (dump) std::fclose(dump);
    for (int k = 0; k < 9; ++k) std::printf("%-6s %016llx\n", names[k], (unsigned long long)h[k]);
    return 0;
}
