#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace ra2assets {

// Decode a base64 string (standard alphabet: A-Z a-z 0-9 + /, '=' padding),
// skipping any embedded whitespace. RA2/YR map INIs wrap long base64 blobs
// (e.g. [IsoMapPack5]) across many numbered keys whose values get
// concatenated by the caller before being handed to this function.
std::vector<uint8_t> base64_decode(const std::string& s);

// Decompress a concatenated "Format5" pack: a back-to-back sequence of
// chunks, each laid out as [u16 le compressedSize][u16 le uncompressedSize]
// [LCW/Format80 body of compressedSize bytes], continuing until `len` bytes
// of `data` are consumed. Each chunk's body is decompressed via
// ra2assets::lcw_decompress (Task 1) and must yield EXACTLY the declared
// uncompressedSize; every chunk header and body is bounds-checked against
// `len`. Throws std::runtime_error (with context) on any malformed input.
std::vector<uint8_t> decompress_format5(const uint8_t* data, size_t len);

} // namespace ra2assets
