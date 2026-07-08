#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace ra2assets::detail {
// Derives up to 56 key bytes from the 80-byte MIX key-source block via
// Westwood's fixed public-key decryption (a.k.a. get_blowfish_key).
// Returns the number of key bytes produced (56 for standard WW MIX).
size_t westwood_blowfish_key(const uint8_t src[80], uint8_t out_key[56]);
} // namespace ra2assets::detail

namespace ra2assets {
struct MixEntry { uint32_t id; uint32_t offset; uint32_t size; };
class MixFile {
public:
    static MixFile open(std::vector<uint8_t> bytes);
    bool encrypted() const { return encrypted_; }
    bool checksummed() const { return checksummed_; }
    size_t count() const { return entries_.size(); }
    const std::vector<MixEntry>& entries() const { return entries_; }
    const MixEntry* find(uint32_t id) const;
    const MixEntry* find(const std::string& name) const;
    std::vector<uint8_t> read(const MixEntry& e) const;
    // Names listed inside "local mix database.dat", if this MIX embeds one.
    std::vector<std::string> local_names() const;
    // Extract a child MIX by name and open it.
    MixFile open_nested(const std::string& name) const;
private:
    std::vector<uint8_t> bytes_;
    std::vector<MixEntry> entries_;
    size_t body_start_ = 0;
    bool encrypted_ = false, checksummed_ = false;
};
uint32_t mix_id(const std::string& name);
} // namespace ra2assets
