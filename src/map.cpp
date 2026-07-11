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
    // "No tile" sentinel, confirmed against a real 103x110 map's decoded
    // IsoMapPack5 (829 of 22550 records): Tile's low 16 bits are 0xFFFF with
    // the upper 16 bits (always observed zero otherwise) also zero, i.e. the
    // combined LE int32 reads as plain 0xFFFF. Per ModEnc, this is NOT a "no
    // cell here" marker -- the game replaces it with a clear tile when
    // loading, so the coordinate is real, walkable ground. We therefore
    // MATERIALIZE these records as tile 0 ("Clear") rather than dropping
    // them (see the Task 5 review finding / task-5-report.md). Only the
    // 16-bit form (int32 value 65535) was ever observed in real data; the
    // brief's original 0xFFFFFFFF guess is not handled since it has never
    // been seen and there is no test data to justify carrying it as dead
    // code (ModEnc specifies the sentinel as a 16-bit tile index).
    constexpr uint32_t kNoTileSentinel = 0xFFFFu;

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

        MapCell c;
        c.x = x;
        c.y = y;
        c.level = level;
        if (tile == kNoTileSentinel) {
            // Materialize as Clear ground, not a dropped/void cell.
            c.tile = 0;
            c.subtile = 0;
        } else {
            c.tile = tile;
            c.subtile = subtile;
        }
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

    // [OverlayPack]/[OverlayDataPack]: same collect-and-decode shape as
    // [IsoMapPack5] above, but the Format5 chunk BODY codec is LCW ("Format80"),
    // not LZO1X (confirmed in task-2-report.md / test_mappack.cpp). Each
    // decodes to a flat 512x512 (262,144-byte) per-cell array; keep the
    // result only if it is exactly that size -- anything else (malformed
    // pack, truncated section, wrong codec desync) is treated as an absent
    // section rather than crashing map load.
    constexpr size_t kOverlayBytes = 512 * 512;
    auto collect_overlay = [&ini](const std::string& section) -> std::vector<uint8_t> {
        const auto& sec_kvs = ini.section(section);
        if (sec_kvs.empty()) return {};
        std::string b64;
        for (const auto& kv : sec_kvs) b64 += kv.second;
        std::vector<uint8_t> raw;
        try {
            // Both stages throw on malformed input (base64_decode on any
            // non-alphabet character, decompress_mappack on a bad Format5/
            // LCW stream); either way the pack is treated as absent -- a
            // malformed overlay section must never crash map load.
            std::vector<uint8_t> packed = base64_decode(b64);
            if (packed.empty()) return {};
            raw = decompress_mappack(packed.data(), packed.size(), MapPackCodec::LCW);
        } catch (const std::exception&) {
            return {};
        }
        if (raw.size() != kOverlayBytes) return {};
        return raw;
    };
    map.overlay_ = collect_overlay("OverlayPack");
    map.overlay_data_ = collect_overlay("OverlayDataPack");

    return map;
}

const MapCell* MapFile::at(int x, int y) const {
    for (const auto& c : cells_) {
        if (c.x == x && c.y == y) return &c;
    }
    return nullptr;
}

int MapFile::overlay_at(int x, int y) const {
    if (overlay_.empty() || x < 0 || x >= 512 || y < 0 || y >= 512) return -1;
    const uint8_t v = overlay_[static_cast<size_t>(x) + static_cast<size_t>(y) * 512];
    return v == 0xFF ? -1 : v;
}
int MapFile::overlay_data_at(int x, int y) const {
    if (overlay_data_.empty() || x < 0 || x >= 512 || y < 0 || y >= 512) return -1;
    if (overlay_at(x, y) < 0) return -1;   // data byte is meaningless without an overlay
    return overlay_data_[static_cast<size_t>(x) + static_cast<size_t>(y) * 512];
}

} // namespace ra2assets
