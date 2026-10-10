// simdiff L0 rung, clone side: seed the engine's GameRandom (engine/crate.cpp)
// and write the same draw file the original-game hook writes with
// SIMHOOK_L0=<n> (<dump>.l0): n raw draws, then n ranged draws over a fixed
// ladder of ranges, continuing the stream. Compare with
//   python tools/simdiff/simdiff.py l0 <orig>.l0 <clone>.l0
//   simdiff_l0 <seed-hex> <n> <out>
#include <cstdio>
#include <cstdlib>
#include "../../engine/crate.hpp"

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: simdiff_l0 <seed-hex> <n> <out>\n"); return 2; }
    const std::uint32_t seed = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 16));
    const int n = std::atoi(argv[2]);
    std::FILE* f = std::fopen(argv[3], "w");
    if (!f) return 1;
    ra2engine::GameRandom r(seed);
    std::fprintf(f, "L0 seed %08X draws %d\n", seed, n);
    for (int i = 0; i < n; ++i) std::fprintf(f, "%08X\n", r.next());
    static const int ranges[8][2] = { {0, 1}, {0, 2}, {0, 99}, {-5, 5}, {0, 255}, {1, 6}, {0, 1000}, {0, 0x7FFF} };
    for (int i = 0; i < n; ++i) {
        const int* rg = ranges[i % 8];
        std::fprintf(f, "R %d %d %d\n", rg[0], rg[1], r.ranged(rg[0], rg[1]));
    }
    std::fclose(f);
    return 0;
}
