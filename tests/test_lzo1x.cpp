#include "doctest.h"
#include "ra2assets/lzo1x.hpp"
#include "ra2assets/mappack.hpp"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace ra2assets;

// Decode helper: run lzo1x_decompress into an exactly-sized buffer and return
// the bytes as a string (all synthetic streams below use printable literals).
static std::string dec(std::vector<uint8_t> s, size_t out_len) {
    std::vector<uint8_t> o(out_len);
    size_t n = lzo1x_decompress(s.data(), s.size(), o.data(), o.size());
    return std::string(o.begin(), o.begin() + n);
}

// --- Hand-built LZO1X streams with hand-computed output ---------------------
// LZO1X has no trivial one-liner known-answer test, so these are constructed
// byte-by-byte from the documented token grammar and traced by hand. Every
// stream ends with the 0x11 0x00 0x00 end-of-stream marker (a 0x10-0x1F copy
// token whose decoded distance is exactly 16384). The real-data test at the
// bottom is the primary correctness arbiter.

TEST_CASE("lzo1x: literal run + short back-copy + end marker") {
    // 0x04            : t<16, state==0 -> literal run, len = 3 + 4 = 7
    //                   copy 'a''b''c''d''e''f''g'            -> "abcdefg", state:=4
    // 0x48 0x00       : 0x40-0x7F copy. len = 3 + ((0x48>>5)&1)=3.
    //                   D=(0x48>>2)&7=2, H=0x00 -> dist=(0<<3)+2+1=3, state=0.
    //                   copy 3 from dist 3 = "efg"           -> "abcdefgefg"
    // 0x11 0x00 0x00  : end of stream
    auto r = dec({0x04,'a','b','c','d','e','f','g', 0x48,0x00, 0x11,0x00,0x00}, 10);
    CHECK(r == "abcdefgefg");
}

TEST_CASE("lzo1x: 0x80-0xFF match performs overlapping (RLE) copy") {
    // 0x01            : literal run, len = 3 + 1 = 4 -> "WXYZ", state:=4
    // 0x80 0x00       : 0x80-0xFF copy. len = 5 + ((0x80>>5)&3)=5.
    //                   D=(0x80>>2)&7=0, H=0x00 -> dist=1, state=0.
    //                   copy 5 from dist 1 -> repeats 'Z' 5x (overlap) => "ZZZZZ"
    // end marker      : "WXYZ" + "ZZZZZ" = "WXYZZZZZZ"
    auto r = dec({0x01,'W','X','Y','Z', 0x80,0x00, 0x11,0x00,0x00}, 9);
    CHECK(r == "WXYZZZZZZ");
}

TEST_CASE("lzo1x: first-byte literal form (t>17) + 0x20-0x3F copy") {
    // 0x15 (21)       : first byte > 17 -> bare literal run of 21-17 = 4
    //                   copy 't''e''s''t' -> "test", state:=4
    // 0x22 0x0C 0x00  : 0x20-0x3F copy. len = 2 + (0x22&31)=2+2=4.
    //                   d16=0x000C -> dist=(12>>2)+1=4, state=12&3=0.
    //                   copy 4 from dist 4 = "test" -> "testtest"
    // end marker
    auto r = dec({0x15,'t','e','s','t', 0x22,0x0C,0x00, 0x11,0x00,0x00}, 8);
    CHECK(r == "testtest");
}

TEST_CASE("lzo1x: trailing literals + short-distance state path") {
    // 0x02              : literal run len = 3 + 2 = 5 -> "ABCDE", state:=4
    // 0x42 0x00 'X' 'Y' : 0x40-0x7F copy. len=3, D=0,H=0->dist=1 (RLE of 'E')
    //                     => "EEE"; state = 0x42&3 = 2 trailing literals "XY".
    //                     -> "ABCDEEEEXY", state:=2
    // 0x04 0x00         : t<16 with state 1-3 -> 2-byte copy. H=0x00,
    //                     dist=(0<<2)+(0x04>>2)+1 = 2, state=0x04&3=0.
    //                     copy 2 from dist 2 = "XY" -> "ABCDEEEEXYXY"
    // end marker
    auto r = dec({0x02,'A','B','C','D','E', 0x42,0x00,'X','Y', 0x04,0x00, 0x11,0x00,0x00}, 12);
    CHECK(r == "ABCDEEEEXYXY");
}

TEST_CASE("lzo1x: literal-run length extension via zero byte") {
    // 0x00 0x03 : t<16, state==0, t==0 -> length extension. base 15, next
    //             byte 0x03 (non-zero) -> count = 15 + 3 = 18, len = 3+18 = 21.
    //             copy 21 literals, then end marker.
    std::vector<uint8_t> s = {0x00, 0x03};
    std::string expect;
    for (int i = 0; i < 21; i++) { uint8_t b = (uint8_t)('0' + i % 10); s.push_back(b); expect.push_back((char)b); }
    s.push_back(0x11); s.push_back(0x00); s.push_back(0x00);
    auto r = dec(s, 21);
    CHECK(r == expect);
}

TEST_CASE("lzo1x rejects malformed / overrunning streams") {
    uint8_t out[64];

    // Source overrun: literal run of 7 but only 2 literal bytes present.
    {
        std::vector<uint8_t> s = {0x04, 'a', 'b'};
        CHECK_THROWS_AS(lzo1x_decompress(s.data(), s.size(), out, sizeof(out)), std::runtime_error);
    }
    // Back-reference before start of output: match token as the very first
    // instruction references bytes that were never written.
    {
        // 0x48 0x00 -> 0x40-0x7F copy dist 3, but 0 bytes written so far.
        std::vector<uint8_t> s = {0x48, 0x00, 0x11, 0x00, 0x00};
        CHECK_THROWS_AS(lzo1x_decompress(s.data(), s.size(), out, sizeof(out)), std::runtime_error);
    }
    // Destination overrun: literal run larger than the output buffer.
    {
        std::vector<uint8_t> s = {0x04, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 0x11, 0x00, 0x00};
        uint8_t tiny[3];
        CHECK_THROWS_AS(lzo1x_decompress(s.data(), s.size(), tiny, sizeof(tiny)), std::runtime_error);
    }
    // Produced-size mismatch: valid stream, but caller declares a larger size.
    {
        std::vector<uint8_t> s = {0x04, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 0x11, 0x00, 0x00};
        CHECK_THROWS_AS(lzo1x_decompress(s.data(), s.size(), out, 999), std::runtime_error);
    }
}

// --- Guarded real-data test: decode a real map's [IsoMapPack5] terrain ------
// This is the correctness arbiter for the LZO1X grammar. Point RA2_TEST_MAP at
// an extracted .map text file (the SAME temperate map Task 2 used, pulled out
// of mapsmd03.mix -- see task-2-report.md / task-2b-report.md). Skips cleanly
// (does not fail) when the asset is absent so the suite stays green on
// machines without the RA2 install.
namespace {
// Minimal inline extractor (identical approach to test_mappack.cpp): pull the
// numbered-key values out of [<section>] and concatenate them in file order.
// Not a general INI parser -- just enough to feed base64_decode.
std::string extract_section_base64(const std::string& text, const std::string& section) {
    const std::string marker = "[" + section + "]";
    size_t start = text.find(marker);
    if (start == std::string::npos) return "";
    start += marker.size();
    std::string out;
    std::istringstream lines(text.substr(start));
    std::string line;
    while (std::getline(lines, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '[') break; // next INI section
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        out += line.substr(eq + 1);
    }
    return out;
}
} // namespace

TEST_CASE("lzo1x: real map [IsoMapPack5] terrain decodes via decompress_mappack(LZO)") {
    const char* path = std::getenv("RA2_TEST_MAP");
    if (!path) { MESSAGE("skipped: RA2_TEST_MAP env var not set"); return; }
    std::ifstream f(path, std::ios::binary);
    if (!f) { MESSAGE("skipped: cannot open RA2_TEST_MAP=" << path); return; }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (text.empty()) { MESSAGE("skipped: RA2_TEST_MAP file is empty"); return; }

    std::string b64 = extract_section_base64(text, "IsoMapPack5");
    if (b64.empty()) { MESSAGE("skipped: no [IsoMapPack5] section in RA2_TEST_MAP"); return; }

    auto packed = base64_decode(b64);
    REQUIRE(packed.size() > 0);

    // (a) Must decode without throwing. This IS the strong arbiter: the
    //     Format5 loop requires EACH chunk's LZO1X body to produce exactly its
    //     declared uncompressed size (throws otherwise), across every ~8192-
    //     byte chunk of genuine LZO1X-compressed terrain -- a wrong decoder
    //     desyncs and mismatches on real, multi-chunk data.
    std::vector<uint8_t> cells = decompress_mappack(packed.data(), packed.size(), MapPackCodec::LZO);
    REQUIRE(cells.size() > 0);

    // (b) Independently re-sum the Format5 chunk headers' declared
    //     uncompressed sizes and require the decoder to have reproduced that
    //     total exactly (makes the per-chunk guarantee above explicit).
    size_t declared = 0, chunk_count = 0;
    for (size_t p = 0; p + 4 <= packed.size();) {
        uint16_t cmp = (uint16_t)(packed[p] | (packed[p + 1] << 8));
        uint16_t unc = (uint16_t)(packed[p + 2] | (packed[p + 3] << 8));
        declared += unc; ++chunk_count; p += 4 + cmp;
    }
    CHECK(cells.size() == declared);

    // (c) 11-byte cell-record structure. Real RA2 IsoMapPack5 uncompressed
    //     data is an array of 11-byte cell records ({X,Y : int16; Tile :
    //     int32; SubTile,Level,IceGrowth : uint8}) optionally followed by a
    //     small zero pad. For the temperate mapsmd03 test map (103x110) the
    //     total is 22550 cells * 11 = 248050 bytes + a 4-byte zero trailer =
    //     248054 (22550 == (2*103-1)*110, the isometric cell count). So the
    //     cell array proper is a positive multiple of 11; any trailing bytes
    //     are zero pad. (The brief assumed the raw total itself is a bare
    //     multiple of 11; the 4-byte trailer is documented in
    //     task-2b-report.md.)
    size_t cell_count = cells.size() / 11;
    size_t trailer    = cells.size() % 11;
    CHECK(cell_count > 0);
    for (size_t i = cell_count * 11; i < cells.size(); ++i) CHECK(cells[i] == 0); // trailer is zero pad
    MESSAGE("real IsoMapPack5: " << packed.size() << " packed bytes, " << chunk_count
            << " LZO1X chunks -> " << cells.size() << " bytes = " << cell_count
            << " cells (11B each) + " << trailer << "B zero trailer");
}
