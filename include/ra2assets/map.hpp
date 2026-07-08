#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ra2assets {

// One decoded [IsoMapPack5] cell record. x/y are the RAW coordinates stored
// in the record itself -- an isometric coordinate space used only inside
// IsoMapPack5 (confirmed empirically against a real 103x110 map: both range
// roughly 1..212, i.e. on the order of width+height, NOT the same space as
// MapFile::width()/height(), which come from [Map] Size, the map's ordinary
// playable-grid rectangle). tile 0xFFFF is the real "no cell here" sentinel
// (see detail::parse_cell_records) and is filtered out before a MapCell is
// ever constructed for it, so every MapCell here is a real, drawn tile --
// including the very common, legitimate tile id 0 ("Clear" terrain).
struct MapCell {
    int x = 0, y = 0;
    uint32_t tile = 0;
    uint8_t subtile = 0;
    uint8_t level = 0;
};

namespace detail {
// Parse an ALREADY-DECOMPRESSED IsoMapPack5 byte buffer (i.e. the output of
// decompress_mappack(..., MapPackCodec::LZO) -- NOT the base64/LZO-packed
// bytes) into cell records. Each record is 11 bytes, little-endian:
//   int16  X
//   int16  Y
//   int32  Tile      (real layout, confirmed against Task 2b's real-map
//                      dump: a uint16 tile id followed by an always-zero
//                      uint16 pad half, read together as one LE int32 --
//                      equivalently OpenRA's ReadTiles reads them as two
//                      separate uint16/int16 fields)
//   uint8  SubTile
//   uint8  Level
//   uint8  (ignored -- "IceGrowth" per some community docs)
// Walks floor(len/11) records; any trailing <11 bytes (real maps emit a
// small zero pad, e.g. 4 bytes for the reference 103x110 map) are ignored,
// not treated as a malformed/short record.
//
// Records whose Tile equals the real "no cell here" sentinel are dropped;
// every other Tile value (INCLUDING 0, the common "Clear" terrain tile) is
// kept. NOTE vs. the original task brief: the brief guessed the sentinel is
// 0xFFFFFFFF, but real data shows it is 0xFFFF (the low 16 bits only, with
// the upper Tile half always 0 in practice) -- both values are treated as
// the sentinel here defensively, but 0xFFFF is the one actually observed.
//
// Exposed here (not `static` in map.cpp) specifically so Task 5's synthetic
// unit test can drive the record-walking logic directly with hand-built raw
// bytes: hand-synthesizing a real LZO1X-compressed [IsoMapPack5] blob for a
// black-box MapFile::parse() test is impractical (LZO1X has no trivial
// encoder in this codebase -- see test_lzo1x.cpp). This function is the
// entire record-walking contract MapFile::parse() relies on for terrain, so
// testing it directly with raw bytes exercises exactly the same logic.
std::vector<MapCell> parse_cell_records(const uint8_t* data, size_t len);
} // namespace detail

class MapFile {
public:
    // Parses a full map/.mpr INI: `[Map] Theater=`/`Size=`, and the
    // `[IsoMapPack5]` terrain section (concatenated base64 -> LZO1X Format5
    // -> cell records via detail::parse_cell_records). Throws
    // std::runtime_error (with context) if the input has no `[Map]` section.
    // A missing `[IsoMapPack5]` section is NOT an error -- cells() is simply
    // empty in that case.
    static MapFile parse(const std::string& ini_text);

    // Playable map dimensions, from `[Map] Size=x,y,w,h` (width is the 3rd
    // comma-separated field, height the 4th). 0 if `[Map]`/`Size=` is
    // absent or a field fails to parse.
    int width() const { return width_; }
    int height() const { return height_; }

    // Decoded, sentinel-filtered [IsoMapPack5] cell records, in file/decode
    // order. Empty if the map has no [IsoMapPack5] section.
    const std::vector<MapCell>& cells() const { return cells_; }

    // [Map] Theater=, e.g. "TEMPERATE". Empty string if absent.
    std::string theater() const { return theater_; }

    // First cell whose (x,y) match the query, in the RAW IsoMapPack5
    // coordinate space (see MapCell) -- or nullptr if none. Linear scan:
    // fine for the vertical slice's one-shot-per-map lookups; revisit with
    // an index if a caller needs repeated random-access lookups.
    const MapCell* at(int x, int y) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::string theater_;
    std::vector<MapCell> cells_;
};

} // namespace ra2assets
