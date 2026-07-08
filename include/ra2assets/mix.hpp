#pragma once
#include <cstdint>
#include <cstddef>

namespace ra2assets::detail {
// Derives up to 56 key bytes from the 80-byte MIX key-source block via
// Westwood's fixed public-key decryption (a.k.a. get_blowfish_key).
// Returns the number of key bytes produced (56 for standard WW MIX).
size_t westwood_blowfish_key(const uint8_t src[80], uint8_t out_key[56]);
} // namespace ra2assets::detail
