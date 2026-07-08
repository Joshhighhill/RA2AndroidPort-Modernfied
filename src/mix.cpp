#include "ra2assets/mix.hpp"
#include "ra2assets/blowfish.hpp"
#include <cctype>
#include <stdexcept>

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
            // Layout after flags: 80-byte key source, then Blowfish-encrypted
            // header+index in 8-byte blocks.
            if (n < 4 + 80 + 8) throw std::runtime_error("mix: truncated encrypted header");
            const uint8_t* ks = p + pos;                 // pos == 4
            uint8_t key[56];
            detail::westwood_blowfish_key(ks, key);
            Blowfish bf; bf.set_key(key, 56);
            pos += 80;

            // Decrypt first block (8 bytes) to read count+bodysize (6 bytes).
            uint8_t hdr[8];
            bf.decrypt_block(p + pos, hdr);
            uint16_t count = rd16(hdr);
            // header is 6 bytes; index is count*12 bytes; total rounded up to /8.
            size_t index_bytes = size_t(count) * 12;
            size_t total = 6 + index_bytes;
            size_t blocks = (total + 7) / 8;
            // A wrong Blowfish key (a botched Task 2 port) yields a garbage
            // count. Fail loudly and diagnosably instead of reading out of
            // bounds — this is the primary tripwire for the key-derivation port.
            if (pos + blocks * 8 > n)
                throw std::runtime_error("mix: encrypted index exceeds file (wrong key derivation?)");
            std::vector<uint8_t> dec(blocks * 8);
            for (size_t b = 0; b < blocks; b++)
                bf.decrypt_block(p + pos + b*8, dec.data() + b*8);
            // body starts after the encrypted region (block-aligned)
            m.body_start_ = pos + blocks * 8;
            const uint8_t* d = dec.data();
            /* count */ (void)rd16(d);
            /* bodysize */ (void)rd32(d + 2);
            size_t ip = 6;
            m.entries_.reserve(count);
            for (uint16_t i = 0; i < count; i++) {
                MixEntry e{ rd32(d+ip), rd32(d+ip+4), rd32(d+ip+8) };
                ip += 12;
                m.entries_.push_back(e);
            }
            return m;
        }
    } else {                                   // old TD/RA1 format: starts with count
        m.encrypted_ = false; m.checksummed_ = false;
        pos = 0;
    }
    if (n < pos + 6) throw std::runtime_error("mix: truncated header");
    uint16_t count = rd16(p + pos); pos += 2;
    /* body size */ (void)rd32(p + pos); pos += 4;
    if ((uint64_t)pos + (uint64_t)count * 12 > n) throw std::runtime_error("mix: truncated index");
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
    uint64_t start = (uint64_t)body_start_ + e.offset;
    if (start + e.size > bytes_.size()) throw std::runtime_error("mix: entry out of range");
    return std::vector<uint8_t>(bytes_.begin() + (size_t)start, bytes_.begin() + (size_t)(start + e.size));
}

uint32_t mix_id(const std::string& name_in) {
    // TS/RA2/YR scheme — NOT the TD/RA1 rotate-add scheme. Uppercase the name;
    // if the length is not a multiple of 4, append ONE byte whose value is
    // (len & 3), then repeat the FIRST character of the final 4-byte block
    // until the length is a multiple of 4; then take a standard CRC32
    // (reflected poly 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF).
    // Reference: XCC `get_id` (Cc_structures); OpenRA PackageHashType.CRC32.
    std::string s;
    s.reserve(name_in.size() + 4);
    for (char c : name_in) s.push_back((char)std::toupper((unsigned char)c));
    size_t l = s.size();
    if (l & 3u) {
        s.push_back((char)(l & 3u));
        size_t block_start = l & ~size_t(3);   // first char of the final block
        while (s.size() & 3u) s.push_back(s[block_start]);
    }
    uint32_t crc = 0xFFFFFFFFu;                // standard CRC32, bitwise form
    for (unsigned char ch : s) {
        crc ^= ch;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

const MixEntry* MixFile::find(const std::string& name) const {
    return find(mix_id(name));
}

std::vector<std::string> MixFile::local_names() const {
    // "local mix database.dat" id, then parse its XCC-format string table.
    const MixEntry* e = find(mix_id("local mix database.dat"));
    std::vector<std::string> out;
    if (!e) return out;
    auto data = read(*e);
    // XCC local db format: 32-byte header ("XCC by Olaf van der Spek\0" + type),
    // uint32 size, uint32 count, then `count` NUL-terminated names.
    if (data.size() < 0x34) return out;
    const uint8_t* p = data.data();
    uint32_t count = rd32(p + 0x30);
    size_t pos = 0x34;
    for (uint32_t i = 0; i < count && pos < data.size(); i++) {
        std::string nm;
        while (pos < data.size() && p[pos]) nm.push_back((char)p[pos++]);
        pos++; // skip NUL
        out.push_back(nm);
    }
    return out;
}

MixFile MixFile::open_nested(const std::string& name) const {
    const MixEntry* e = find(name);
    if (!e) throw std::runtime_error("mix: nested entry not found: " + name);
    return MixFile::open(read(*e));
}

} // namespace ra2assets
