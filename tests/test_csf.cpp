#include "doctest.h"
#include "ra2assets/csf.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic CSF builder. Emits the exact on-disk layout (little-endian,
// packed; byte-verified against the install's ra2.csf -- see src/csf.cpp):
//   file header (24B): " FSC", u32 version=3, u32 num_labels, u32 num_strings,
//                      u32 unused=0, u32 language=0
//   per label: " LBL", u32 num_values, u32 label_len, label bytes (no NUL)
//   per value: " RTS" (or "WRTS"), u32 val_len (UTF-16 code units), then
//              val_len*2 bytes UTF-16LE with every byte XOR 0xFF; a "WRTS"
//              record adds u32 extra_len + extra_len ASCII bytes.
// ---------------------------------------------------------------------------
struct CsfValue {
    std::u16string text;
    bool wide = false;      // emit "WRTS" instead of " RTS"
    std::string extra;      // WRTS-only trailing ASCII payload
};
struct CsfLabel {
    std::string name;
    std::vector<CsfValue> values;
};

static std::vector<uint8_t> make_csf(const std::vector<CsfLabel>& labels) {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) b.push_back((uint8_t)((v>>(8*i))&0xff)); };
    auto pmagic = [&](const char* m){ for (int i=0;i<4;i++) b.push_back((uint8_t)m[i]); };
    // file header (24 bytes)
    pmagic(" FSC");                            // 0x20 0x46 0x53 0x43
    p32(3);                                    // version
    p32((uint32_t)labels.size());              // num_labels
    uint32_t num_strings = 0;
    for (const auto& l : labels) num_strings += (uint32_t)l.values.size();
    p32(num_strings);                          // num_strings
    p32(0);                                    // unused
    p32(0);                                    // language
    for (const auto& l : labels) {
        pmagic(" LBL");                        // 0x20 0x4C 0x42 0x4C
        p32((uint32_t)l.values.size());
        p32((uint32_t)l.name.size());
        for (char c : l.name) p8((uint8_t)c);
        for (const auto& v : l.values) {
            pmagic(v.wide ? "WRTS" : " RTS");
            p32((uint32_t)v.text.size());
            for (char16_t u : v.text) {        // UTF-16LE, every byte XOR 0xFF
                p8((uint8_t)((((uint16_t)u)      & 0xff) ^ 0xff));
                p8((uint8_t)((((uint16_t)u >> 8) & 0xff) ^ 0xff));
            }
            if (v.wide) {
                p32((uint32_t)v.extra.size());
                for (char c : v.extra) p8((uint8_t)c);
            }
        }
    }
    return b;
}

TEST_CASE("csf synthetic: two labels load, find, count, absent label") {
    auto bytes = make_csf({
        {"GUI:Ok",    {{u"OK"}}},
        {"Name:HTNK", {{u"Rhino Heavy Tank"}}},
    });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 2);
    const std::u16string* ok = cs.find("GUI:Ok");
    REQUIRE(ok != nullptr);
    CHECK(*ok == u"OK");
    const std::u16string* tank = cs.find("Name:HTNK");
    REQUIRE(tank != nullptr);
    CHECK(*tank == u"Rhino Heavy Tank");
    CHECK(cs.find("Name:E1") == nullptr);      // absent -> nullptr
}

TEST_CASE("csf synthetic: label lookup is case-insensitive") {
    auto bytes = make_csf({ {"GUI:Ok", {{u"OK"}}} });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    const std::u16string* a = cs.find("gui:ok");
    REQUIRE(a != nullptr);
    CHECK(*a == u"OK");
    const std::u16string* c = cs.find("GUI:OK");
    REQUIRE(c != nullptr);
    CHECK(*c == u"OK");
}

TEST_CASE("csf synthetic: non-ASCII UTF-16 units survive the XOR transform") {
    // A Latin-1 char (U+00E9), a CJK char (U+4E2D), and a space: exercises
    // both bytes of each 16-bit unit through the XOR-0xFF store.
    const std::u16string text = u"Café 中";
    auto bytes = make_csf({ {"TEST:Unicode", {{text}}} });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    const std::u16string* got = cs.find("TEST:Unicode");
    REQUIRE(got != nullptr);
    CHECK(*got == text);
    CHECK(got->size() == 6);
}
