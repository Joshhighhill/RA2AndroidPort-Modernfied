#include "ra2assets/fnt.hpp"
#include <stdexcept>

namespace ra2assets {
namespace {
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
}  // namespace

// Westwood classic variable-width bitmap font (C&C/TS/RA2 ".fnt").
//
// CALIBRATE-FIRST (T3 Step 1): the exact header layout below was CONFIRMED by a
// hex probe of the real cache.mix/12metfnt.fnt in this install -- it is NOT the
// XCC-documented byte offsets the brief sketched (which assumed the newer
// "FoNt"-magic face). Probed facts, all cross-checked against the real bytes:
//
//   0x00 u16  fileSize            == exact file length (magic/format guard)
//   0x06 u16  glyphOffsetTable    -> u16 * count, ABSOLUTE byte offset of each
//                                    glyph's 8-bit-per-pixel bitmap
//   0x08 u16  widthTable          -> u8  * count, per-glyph advance width
//   0x0C u16  heightTable         -> u16 * count: low byte = y-offset (top pad),
//                                    high byte = rows (lit height)
//   0x0E u8   firstChar           (0 on 12metfnt)
//   0x12 u8   height              max cell height (16)
//   0x13 u8   maxWidth            (16)
//
// count is not a stored field; it is the table stride between the offset table
// (u16) and the width table (u8): count = (widthTable - glyphOffsetTable)/2,
// cross-validated against (heightTable - widthTable). On 12metfnt all three
// spacings agree at 219 (first=0..last=218).
//
// Glyphs pack 1 byte per pixel (values 0..~4 = a shading/bevel ramp; 0 =
// transparent), row-major, `rows` rows of `width` bytes, placed at vertical
// y-offset `yoff` inside the `height`-tall cell. The offset-table gaps equal
// width*rows exactly for the ASCII letters, which is what pins the depth at
// 8bpp (a 4bpp read would need width/2 bytes/row and would not close the gaps).
Font Font::load(std::vector<uint8_t> b) {
    if (b.size() < 0x14) throw std::runtime_error("fnt: shorter than header");
    const uint8_t* d = b.data();

    // Format guard: the classic face stores its own byte length at 0x00. This
    // rejects the "FoNt"/"fonT"-magic faces (metal12.fnt/game.fnt), whose first
    // u16 is the ASCII "Fo"/"fo" -- never the file size.
    const uint16_t file_size = rd16(d + 0x00);
    if (file_size != b.size())
        throw std::runtime_error("fnt: not a classic Westwood .fnt (size field mismatch)");

    const uint16_t off_glyphs = rd16(d + 0x06);   // u16 per glyph: bitmap offset
    const uint16_t off_width  = rd16(d + 0x08);   // u8  per glyph: advance width
    const uint16_t off_height = rd16(d + 0x0C);   // u16 per glyph: {yoff, rows}

    if (off_glyphs >= off_width || off_width >= off_height)
        throw std::runtime_error("fnt: header offset tables out of order");
    if ((off_width - off_glyphs) % 2 != 0)
        throw std::runtime_error("fnt: glyph offset table not u16-aligned");

    const int count = (off_width - off_glyphs) / 2;         // u16 offset table
    if (count <= 0) throw std::runtime_error("fnt: empty font");
    if (off_height - off_width != count)                    // u8 width table
        throw std::runtime_error("fnt: width/height table size mismatch");

    Font f;
    f.first_     = d[0x0E];
    f.height_    = d[0x12];
    f.max_width_ = d[0x13];
    f.last_      = f.first_ + count - 1;
    if (f.height_ <= 0) throw std::runtime_error("fnt: bad height");

    // Bounds: every referenced table must lie inside the file.
    const size_t need = static_cast<size_t>(off_height) + static_cast<size_t>(count) * 2;
    if (need > b.size()) throw std::runtime_error("fnt: tables exceed file");

    f.glyphs_.resize(count);
    f.widths_.resize(count);
    for (int i = 0; i < count; ++i) {
        const int w = d[off_width + i];
        const uint8_t* hh = d + off_height + i * 2;
        const int yoff = hh[0];
        int rows = hh[1];
        const uint16_t gdo = rd16(d + off_glyphs + i * 2);  // this glyph's bitmap

        f.widths_[i] = w;
        FntGlyph& g = f.glyphs_[i];
        g.w = w;
        g.h = f.height_;                                    // uniform cell height
        g.pixels.assign(static_cast<size_t>(g.w) * g.h, 0);

        // Clamp the lit block into the cell and to the file, defensively.
        if (w <= 0 || rows <= 0) continue;
        if (yoff >= g.h) continue;
        if (yoff + rows > g.h) rows = g.h - yoff;
        const size_t glyph_bytes = static_cast<size_t>(w) * rows;
        if (static_cast<size_t>(gdo) + glyph_bytes > b.size()) continue;  // corrupt entry
        const uint8_t* src = d + gdo;
        for (int ry = 0; ry < rows; ++ry) {
            for (int rx = 0; rx < w; ++rx) {
                g.pixels[static_cast<size_t>(yoff + ry) * g.w + rx] =
                    src[static_cast<size_t>(ry) * w + rx];
            }
        }
    }
    return f;
}

int Font::width(int ch) const {
    if (ch < first_ || ch > last_) return 0;
    return widths_[ch - first_];
}

const FntGlyph& Font::glyph(int ch) const {
    if (ch < first_ || ch > last_) throw std::runtime_error("fnt: glyph out of range");
    return glyphs_[ch - first_];
}

}  // namespace ra2assets
