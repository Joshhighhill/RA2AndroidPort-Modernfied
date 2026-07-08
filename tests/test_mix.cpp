#include "doctest.h"
#include "ra2assets/mix.hpp"
#include <cstdint>
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
