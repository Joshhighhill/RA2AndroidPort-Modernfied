#include "doctest.h"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ra2assets;

namespace {
// Append one hand-crafted 11-byte IsoMapPack5 cell record (little-endian):
// int16 X, int16 Y, int32 Tile, uint8 SubTile, uint8 Level, uint8 (ignored).
void push_record(std::vector<uint8_t>& out, int16_t x, int16_t y, uint32_t tile,
                  uint8_t subtile, uint8_t level, uint8_t ignored = 0) {
    out.push_back((uint8_t)(x & 0xFF));
    out.push_back((uint8_t)((x >> 8) & 0xFF));
    out.push_back((uint8_t)(y & 0xFF));
    out.push_back((uint8_t)((y >> 8) & 0xFF));
    out.push_back((uint8_t)(tile & 0xFF));
    out.push_back((uint8_t)((tile >> 8) & 0xFF));
    out.push_back((uint8_t)((tile >> 16) & 0xFF));
    out.push_back((uint8_t)((tile >> 24) & 0xFF));
    out.push_back(subtile);
    out.push_back(level);
    out.push_back(ignored);
}
} // namespace

// --- SYNTHETIC: record-walking logic, driven directly with raw bytes -------
// Hand-synthesizing a real LZO1X-compressed [IsoMapPack5] blob for a
// black-box MapFile::parse() test is impractical (LZO1X has no trivial
// encoder in this codebase -- test_lzo1x.cpp's own streams are hand-traced
// token-by-token, not built from arbitrary input). Task 2b's real-map test
// already proves the LZO1X decompressor end to end. So these tests exercise
// the record-walking step directly via ra2assets::detail::parse_cell_records
// (exposed in map.hpp for exactly this purpose), feeding it raw
// ALREADY-DECOMPRESSED bytes -- precisely what decompress_mappack(...) hands
// MapFile::parse() on the real path. The INI-level parse() path
// (Theater/Size) is exercised separately below without needing terrain data.
TEST_CASE("map: record-walk parses two hand-built 11-byte cell records") {
    std::vector<uint8_t> raw;
    push_record(raw, 5, 7, 42, 1, 3);
    push_record(raw, 8, 9, 100, 2, 4);

    auto cells = detail::parse_cell_records(raw.data(), raw.size());
    REQUIRE(cells.size() == 2);
    CHECK(cells[0].x == 5);
    CHECK(cells[0].y == 7);
    CHECK(cells[0].tile == 42);
    CHECK(cells[0].subtile == 1);
    CHECK(cells[0].level == 3);
    CHECK(cells[1].x == 8);
    CHECK(cells[1].y == 9);
    CHECK(cells[1].tile == 100);
    CHECK(cells[1].subtile == 2);
    CHECK(cells[1].level == 4);
}

TEST_CASE("map: record-walk drops the 0xFFFF no-cell sentinel; keeps tile==0; ignores a short trailer") {
    std::vector<uint8_t> raw;
    push_record(raw, 1, 1, 0, 0, 0);      // tile==0 ("Clear") IS kept -- common, legit real terrain
    push_record(raw, 2, 2, 0xFFFF, 9, 9); // real "no cell here" sentinel -- dropped
    push_record(raw, 3, 3, 77, 5, 6);
    raw.push_back(0); raw.push_back(0); raw.push_back(0); // <11-byte zero pad trailer, ignored

    auto cells = detail::parse_cell_records(raw.data(), raw.size());
    REQUIRE(cells.size() == 2);
    CHECK(cells[0].x == 1);
    CHECK(cells[0].tile == 0);
    CHECK(cells[1].x == 3);
    CHECK(cells[1].tile == 77);
}

TEST_CASE("map: parse() reads [Map] Theater/Size even with no terrain section") {
    std::string text =
        "[Map]\n"
        "Theater=TEMPERATE\n"
        "Size=0,0,10,10\n";
    MapFile map = MapFile::parse(text);
    CHECK(map.theater() == "TEMPERATE");
    CHECK(map.width() == 10);
    CHECK(map.height() == 10);
    CHECK(map.cells().empty());
    CHECK(map.at(0, 0) == nullptr);
}

TEST_CASE("map: parse() throws without a [Map] section") {
    CHECK_THROWS_AS(MapFile::parse("[Other]\nFoo=1\n"), std::runtime_error);
}

// --- GUARDED real-map test --------------------------------------------------
// Extracts the map at RUNTIME from the install (durable -- no scratchpad
// dependency, unlike the RA2_TEST_MAP-gated tests in test_mappack.cpp /
// test_lzo1x.cpp): mapsmd03.mix, entry 0x133a0cb2 -- the same temperate
// 103x110 map Tasks 2/2b/2b already validated (see task-2-report.md /
// task-2b-report.md). Skips cleanly (does not fail) if the install/asset is
// absent.
namespace {
std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
} // namespace

TEST_CASE("map: parses the real mapsmd03.mix temperate map (entry 0x133a0cb2)") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto mix_bytes = slurp(base + "\\mapsmd03.mix");
    if (mix_bytes.empty()) { MESSAGE("skipped: mapsmd03.mix not found under " << base); return; }
    MixFile mix = MixFile::open(std::move(mix_bytes));

    const MixEntry* e = mix.find(0x133a0cb2u);
    if (!e) { MESSAGE("skipped: entry 0x133a0cb2 not found in mapsmd03.mix"); return; }
    std::vector<uint8_t> ini_bytes = mix.read(*e);
    std::string text(ini_bytes.begin(), ini_bytes.end());

    MapFile map = MapFile::parse(text);
    CHECK(map.theater() == "TEMPERATE");
    CHECK(map.width() == 103);
    CHECK(map.height() == 110);

    // Exact real-data count: 22550 raw records (== (2*103-1)*110, the
    // isometric cell count -- see task-2b-report.md) minus 829 real
    // 0xFFFF-sentinel "no cell" records = 21721 kept cells. This is a
    // strong, decode-path-specific assertion, not just ">0".
    REQUIRE(map.cells().size() == 21721);

    // Spot-check: raw record 0 is {X=103,Y=1,Tile=99,SubTile=2,Level=3}
    // (independently confirmed by Task 2b's own real-data dump in
    // task-2b-report.md) and is not filtered (tile 99 != the 0xFFFF
    // sentinel), so it must be cells()[0] here too.
    const MapCell& c0 = map.cells()[0];
    CHECK(c0.x == 103);
    CHECK(c0.y == 1);
    CHECK(c0.tile == 99);
    CHECK(c0.subtile == 2);
    CHECK(c0.level == 3);

    // at() finds that same cell by its raw (x,y); an absent coordinate
    // (well outside the observed 1..212 raw coordinate range) returns null.
    const MapCell* found = map.at(103, 1);
    REQUIRE(found != nullptr);
    CHECK(found->tile == 99);
    CHECK(map.at(9999, 9999) == nullptr);

    // Plausibility tripwire: every kept cell's raw coordinate must be well
    // within the TS/RA2 engine's hard 512x512 map-size cap (real data stays
    // in ~1..212 for this map -- see map.hpp for why this differs from
    // width()/height()).
    for (const auto& c : map.cells()) {
        CHECK(c.x >= 0);
        CHECK(c.x < 512);
        CHECK(c.y >= 0);
        CHECK(c.y < 512);
    }

    MESSAGE("real map: theater=" << map.theater() << " size=" << map.width() << "x" << map.height()
            << " cells=" << map.cells().size());
}
