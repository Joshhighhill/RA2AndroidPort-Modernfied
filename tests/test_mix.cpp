#include "doctest.h"
#include "ra2assets/mix.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

TEST_CASE("westwood key derivation is deterministic and full-length") {
    uint8_t src[80];
    for (int i = 0; i < 80; i++) src[i] = (uint8_t)(i * 7 + 3);
    uint8_t k1[56] = {0}, k2[56] = {0};
    size_t n1 = ra2assets::detail::westwood_blowfish_key(src, k1);
    size_t n2 = ra2assets::detail::westwood_blowfish_key(src, k2);
    CHECK(n1 == 56);
    CHECK(n2 == 56);
    for (int i = 0; i < 56; i++) CHECK(k1[i] == k2[i]);   // deterministic
    bool all_zero = true;
    for (int i = 0; i < 56; i++) if (k1[i]) all_zero = false;
    CHECK_FALSE(all_zero);                                 // non-degenerate
}

TEST_CASE("mix parses unencrypted new-format header and reads entries") {
    using namespace ra2assets;
    std::vector<uint8_t> buf;
    auto put32 = [&](uint32_t v){ for(int i=0;i<4;i++) buf.push_back(v>>(8*i)); };
    auto put16 = [&](uint16_t v){ buf.push_back(v&0xff); buf.push_back(v>>8); };
    // flags = 0 (new format, not encrypted / not checksummed)
    put32(0);
    // header: count, body size
    const char* A = "hello"; const char* B = "world!!";
    uint32_t offA = 0, sizeA = 5, offB = 5, sizeB = 7;
    put16(2); put32(sizeA + sizeB);
    // index entries (order by id not required for this test)
    put32(1234); put32(offA); put32(sizeA);
    put32(5678); put32(offB); put32(sizeB);
    // body
    for (const char* p = A; *p; ++p) buf.push_back((uint8_t)*p);
    for (const char* p = B; *p; ++p) buf.push_back((uint8_t)*p);

    MixFile m = MixFile::open(std::move(buf));
    CHECK_FALSE(m.encrypted());
    CHECK(m.count() == 2);
    const MixEntry* e = m.find(5678);
    REQUIRE(e != nullptr);
    auto data = m.read(*e);
    REQUIRE(data.size() == 7);
    CHECK(std::string(data.begin(), data.end()) == "world!!");
}

TEST_CASE("mix rejects truncated and out-of-range input") {
    using namespace ra2assets;

    // Case 1: Truncated new-format file (4 bytes, not enough for header)
    CHECK_THROWS_AS(MixFile::open({0, 0, 0, 0}), std::runtime_error);

    // Case 2: New-format with count=100 but no index data
    {
        std::vector<uint8_t> buf;
        auto put32 = [&](uint32_t v){ for(int i=0;i<4;i++) buf.push_back(v>>(8*i)); };
        auto put16 = [&](uint16_t v){ buf.push_back(v&0xff); buf.push_back(v>>8); };
        put32(0);  // flags = 0 (new format)
        put16(100);  // count = 100
        put32(0);  // body size = 0
        // Missing 100*12 bytes of index; buffer ends here
        CHECK_THROWS_AS(MixFile::open(std::move(buf)), std::runtime_error);
    }

    // Case 3: Valid header/index but out-of-range entry read
    {
        std::vector<uint8_t> buf;
        auto put32 = [&](uint32_t v){ for(int i=0;i<4;i++) buf.push_back(v>>(8*i)); };
        auto put16 = [&](uint16_t v){ buf.push_back(v&0xff); buf.push_back(v>>8); };
        // flags = 0 (new format, not encrypted / not checksummed)
        put32(0);
        // header: count=2, body size = 100
        put16(2);
        put32(100);
        // index entries: one valid, one out-of-range
        put32(1234); put32(0); put32(10);  // id=1234, offset=0, size=10 (valid)
        put32(5678); put32(0xFFFFFFF0u); put32(0x20u);  // id=5678, offset=huge, size=32 (out of range)
        // body (just 100 bytes)
        for (int i = 0; i < 100; i++) buf.push_back((uint8_t)i);

        MixFile m = MixFile::open(std::move(buf));
        const MixEntry* e = m.find(5678);
        REQUIRE(e != nullptr);
        CHECK_THROWS_AS(m.read(*e), std::runtime_error);
    }
}
