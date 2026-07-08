#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace ra2assets {

// One decoded isometric theater tile: a frame-local, row-major indexed image.
// w == tile width (cx, 48 or 60), h == tile height (cy, cx/2). `indices` holds
// w*h palette indices; index 0 is transparent (the out-of-diamond corners are
// always 0). Compositing tiles into a map canvas is a separate concern (Task 9).
struct TileImage {
    int w = 0, h = 0;
    std::vector<uint8_t> indices;
};

// Decoder for TS/RA2 `.tmp` isometric theater templates (as found in
// temperat.mix / isotemp.mix, extension .tem/.sno/.urb). A template is a grid
// of cblocks_x*cblocks_y diamond tiles; each present tile is a diamond of
// cx*cy/2 palette-index bytes that unpacks into a cx*cy rectangle with the
// four corner triangles left transparent (0).
class TmpFile {
public:
    // Parse the header + index table. Throws std::runtime_error (with context)
    // on a malformed/truncated header or index table. Individual tile pixels
    // are validated lazily in decode(i).
    static TmpFile load(std::vector<uint8_t> bytes);

    int count() const { return cblocks_x_ * cblocks_y_; }   // cblocks_x*cblocks_y
    int tile_w() const { return cx_; }                      // cx
    int tile_h() const { return cy_; }                      // cy

    int cblocks_x() const { return cblocks_x_; }
    int cblocks_y() const { return cblocks_y_; }

    // Decode tile i (0..count-1) into a frame-local cx*cy indexed buffer. An
    // absent tile (index-table entry 0) decodes to an all-zero (transparent)
    // buffer. Throws std::runtime_error if i is out of range or the tile's
    // image header / diamond pixels fall outside the file.
    TileImage decode(int i) const;

private:
    std::vector<uint8_t> bytes_;
    std::vector<uint32_t> index_;   // per-tile byte offset to its image header (0 = absent)
    int cblocks_x_ = 0, cblocks_y_ = 0;
    int cx_ = 0, cy_ = 0;
};

} // namespace ra2assets
