#include "doctest.h"
#include "ra2assets/fnt.hpp"
#include "ra2assets/mix.hpp"
#include "asset_fixture.hpp"
#include <cstdlib>

using namespace ra2assets;

// Structural self-consistency of the real Westwood variable-width bitmap font.
//
// CALIBRATE-FIRST note (T3 Step 1): the brief nominated `local.mix/metal12.fnt`,
// but a hex probe showed that file (and `local.mix/game.fnt`) use the newer
// "FoNt"/"fonT"-magic face, NOT the classic Westwood layout the decoder targets.
// The classic-format shell/menu font in this install is `cache.mix/12metfnt.fnt`
// (RE mainmenu.md §3 treats "12metfnt.fnt / metal12.fnt" as the same bevelled
// menu face). Its header (u16 fileSize == file length) was confirmed against the
// real bytes, so this case exercises that font. Observed on this install:
// first=0, last=218, count=219, height=16, max_width=16 (recorded, not asserted).
TEST_CASE("fnt: real 12metfnt.fnt decodes to a self-consistent variable-width font") {
    if (!ra2fixture::ra2_available()) { MESSAGE("ra2.mix absent; skipped"); return; }
    const MixFile& top = ra2fixture::mix_by_name("ra2.mix");
    MixFile cache = top.open_nested("cache.mix");
    const MixEntry* e = cache.find("12metfnt.fnt");
    REQUIRE(e != nullptr);
    Font f = load_fnt(cache.read(*e));

    CHECK(f.count() == f.last() - f.first() + 1);   // header self-consistency
    CHECK(f.count() > 0);
    CHECK(f.height() > 0);
    CHECK(f.height() <= 64);
    CHECK(f.max_width() > 0);
    // Every glyph's advance width is within the font max, and its bitmap is
    // exactly width*height indices (0 = transparent).
    for (int c = f.first(); c <= f.last(); ++c) {
        CAPTURE(c);
        CHECK(f.width(c) <= f.max_width());
        const FntGlyph& g = f.glyph(c);
        CHECK(g.pixels.size() == static_cast<size_t>(g.w) * g.h);
    }
    // A visible glyph ('A') has a positive width and at least one lit pixel.
    if ('A' >= f.first() && 'A' <= f.last()) {
        const FntGlyph& a = f.glyph('A');
        CHECK(a.w > 0);
        bool any = false;
        for (uint8_t p : a.pixels) if (p != 0) { any = true; break; }
        CHECK(any);
    }
}
