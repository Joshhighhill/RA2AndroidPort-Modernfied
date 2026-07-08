#include "doctest.h"
#include "ra2assets/blowfish.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

using ra2assets::Blowfish;

static void hex_to_bytes(const char* hex, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned b; sscanf(hex + i * 2, "%2x", &b); out[i] = (uint8_t)b;
    }
}

TEST_CASE("blowfish known-answer vectors (Eric Young)") {
    struct KAT { const char* key; const char* clear; const char* cipher; };
    KAT kats[] = {
        {"0000000000000000", "0000000000000000", "4EF997456198DD78"},
        {"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"},
        {"3000000000000000", "1000000000000001", "7D856F9A613063F2"},
        {"1111111111111111", "1111111111111111", "2466DD878B963C9D"},
        {"FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D"},
    };
    for (auto& k : kats) {
        uint8_t key[8], clr[8], exp[8], out[8];
        hex_to_bytes(k.key, key, 8);
        hex_to_bytes(k.clear, clr, 8);
        hex_to_bytes(k.cipher, exp, 8);
        Blowfish bf; bf.set_key(key, 8);
        bf.encrypt_block(clr, out);
        CHECK(std::memcmp(out, exp, 8) == 0);
        uint8_t back[8];
        bf.decrypt_block(exp, back);
        CHECK(std::memcmp(back, clr, 8) == 0);
    }
}
