#include "ra2assets/map.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mappack.hpp"
#include <stdexcept>

// MapFile: [Map] Theater/Size + [IsoMapPack5] terrain, sitting on top of Ini
// (Task 3) and decompress_mappack (Tasks 2/2b). See map.hpp for the cell
// record layout and coordinate-space caveats.
namespace ra2assets {

namespace {

// Parse the `index`-th (0-based) comma-separated decimal field of `csv`, or
// `def` if the field is missing/unparseable. Used for `[Map] Size=x,y,w,h`.
int nth_int_field(const std::string& csv, size_t index, int def) {
    size_t pos = 0;
    size_t field = 0;
    while (pos <= csv.size()) {
        size_t comma = csv.find(',', pos);
        std::string tok = (comma == std::string::npos) ? csv.substr(pos) : csv.substr(pos, comma - pos);
        if (field == index) {
            try {
                return std::stoi(tok);
            } catch (...) {
                return def;
            }
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
        field++;
    }
    return def;
}

} // namespace

namespace detail {

std::vector<MapCell> parse_cell_records(const uint8_t* data, size_t len) {
    constexpr size_t kRecordSize = 11;
    // Real "no cell here" sentinel, confirmed against a real 103x110 map's
    // decoded IsoMapPack5 (829 of 22550 records): Tile's low 16 bits are
    // 0xFFFF with the upper 16 bits (always observed zero otherwise) also
    // zero, i.e. the combined LE int32 reads as plain 0xFFFF. 0xFFFFFFFF is
    // also treated as the sentinel defensively (the brief's original guess),
    // though it was never observed in real data.
    constexpr uint32_t kNoCellTile16 = 0xFFFFu;
    constexpr uint32_t kNoCellTile32 = 0xFFFFFFFFu;

    std::vector<MapCell> cells;
    size_t count = len / kRecordSize; // trailing <kRecordSize bytes (a small
                                       // zero pad in real maps) are ignored.
    cells.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * kRecordSize; // in-bounds: i < count == len/11
        int16_t x = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        int16_t y = (int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
        uint32_t tile = (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                        ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        uint8_t subtile = p[8];
        uint8_t level = p[9];
        // p[10] ("IceGrowth"/ignored per the brief) intentionally not read.

        if (tile == kNoCellTile16 || tile == kNoCellTile32) continue;

        MapCell c;
        c.x = x;
        c.y = y;
        c.tile = tile;
        c.subtile = subtile;
        c.level = level;
        cells.push_back(c);
    }
    return cells;
}

} // namespace detail

MapFile MapFile::parse(const std::string& ini_text) {
    Ini ini = Ini::parse(ini_text);
    if (!ini.has("Map"))
        throw std::runtime_error("MapFile::parse: input has no [Map] section");

    MapFile map;
    map.theater_ = ini.get("Map", "Theater", "");

    std::string size = ini.get("Map", "Size", "");
    map.width_ = nth_int_field(size, 2, 0);
    map.height_ = nth_int_field(size, 3, 0);

    const auto& kvs = ini.section("IsoMapPack5");
    if (!kvs.empty()) {
        std::string b64;
        for (const auto& kv : kvs) b64 += kv.second;
        std::vector<uint8_t> packed = base64_decode(b64);
        if (!packed.empty()) {
            std::vector<uint8_t> raw =
                decompress_mappack(packed.data(), packed.size(), MapPackCodec::LZO);
            map.cells_ = detail::parse_cell_records(raw.data(), raw.size());
        }
    }

    return map;
}

const MapCell* MapFile::at(int x, int y) const {
    for (const auto& c : cells_) {
        if (c.x == x && c.y == y) return &c;
    }
    return nullptr;
}

} // namespace ra2assets
