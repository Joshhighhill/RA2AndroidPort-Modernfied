#include "ra2assets/mix.hpp"
#include <stdexcept>
#include <cstring>

namespace ra2assets {

static uint32_t rd32(const uint8_t* p){ return p[0]|(p[1]<<8)|(p[2]<<16)|(uint32_t(p[3])<<24); }
static uint16_t rd16(const uint8_t* p){ return p[0]|(p[1]<<8); }

MixFile MixFile::open(std::vector<uint8_t> bytes) {
    MixFile m;
    m.bytes_ = std::move(bytes);
    const uint8_t* p = m.bytes_.data();
    size_t n = m.bytes_.size();
    if (n < 4) throw std::runtime_error("mix: too small");

    uint32_t first = rd32(p);
    size_t pos;
    if ((first & 0x0000FFFF) == 0) {          // new format: flags dword
        uint32_t flags = first;
        m.encrypted_   = (flags & 0x00020000) != 0;
        m.checksummed_ = (flags & 0x00010000) != 0;
        pos = 4;
        if (m.encrypted_) {
            // Encrypted header handled in Task 4.
            throw std::runtime_error("mix: encrypted header (see Task 4)");
        }
    } else {                                   // old TD/RA1 format: starts with count
        m.encrypted_ = false; m.checksummed_ = false;
        pos = 0;
    }
    uint16_t count = rd16(p + pos); pos += 2;
    /* body size */ (void)rd32(p + pos); pos += 4;
    m.entries_.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
        MixEntry e{ rd32(p+pos), rd32(p+pos+4), rd32(p+pos+8) };
        pos += 12;
        m.entries_.push_back(e);
    }
    m.body_start_ = pos;
    return m;
}

const MixEntry* MixFile::find(uint32_t id) const {
    for (auto& e : entries_) if (e.id == id) return &e;
    return nullptr;
}

std::vector<uint8_t> MixFile::read(const MixEntry& e) const {
    size_t start = body_start_ + e.offset;
    if (start + e.size > bytes_.size()) throw std::runtime_error("mix: entry out of range");
    return std::vector<uint8_t>(bytes_.begin()+start, bytes_.begin()+start+e.size);
}

} // namespace ra2assets
