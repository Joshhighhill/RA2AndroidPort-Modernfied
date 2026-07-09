#include "ra2assets/audio_bag.hpp"
#include <stdexcept>
#include <string>

namespace ra2assets {

// ---------------------------------------------------------------------------
// audio.idx (GABA) index. On-disk layout (little-endian, packed), verified
// against the real install: 12-byte header + count * 36-byte entries; the
// real audio.idx is 41,520 = 12 + 1153*36 bytes exactly. Corroborated by
// XCC misc/cc_structures.h t_audio_idx_header / t_audio_idx_entry (read
// off-repo at D:\ra2-re\reference\xcc -- no GPL code copied here) and the
// game's own loader (docs/re/audio.md).
//   header: char magic[4] = "GABA"; u32 version (=2); u32 count
//   entry : char fname[16] (NUL-padded, no extension); u32 offset; u32 size;
//           u32 sample_rate; u32 flags; u32 chunk_size
// ---------------------------------------------------------------------------
static constexpr size_t kIdxHeader = 12;
static constexpr size_t kIdxEntry  = 36;

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static char upcase(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool iequal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (upcase(a[i]) != upcase(b[i])) return false;
    return true;
}

AudioIdx AudioIdx::load(const uint8_t* data, size_t len) {
    if (len < kIdxHeader)
        throw std::runtime_error("audio.idx: smaller than 12-byte header (" +
                                 std::to_string(len) + " bytes)");
    if (!(data[0] == 'G' && data[1] == 'A' && data[2] == 'B' && data[3] == 'A'))
        throw std::runtime_error("audio.idx: bad magic (want \"GABA\")");
    const uint32_t version = r32(data + 4);
    if (version != 2)
        throw std::runtime_error("audio.idx: unsupported version " +
                                 std::to_string(version) + " (want 2)");
    const uint32_t count = r32(data + 8);
    if (count > 1000000u)
        throw std::runtime_error("audio.idx: implausible entry count " +
                                 std::to_string(count));
    // Mirror XCC Caudio_idx_file::is_valid: header + count*entry must EQUAL
    // the byte length exactly.
    if (len != kIdxHeader + (size_t)count * kIdxEntry)
        throw std::runtime_error("audio.idx: size " + std::to_string(len) +
                                 " != 12 + " + std::to_string(count) + "*36");
    AudioIdx idx;
    idx.clips_.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* e = data + kIdxHeader + (size_t)i * kIdxEntry;
        BagClip c;
        size_t n = 0;
        while (n < 16 && e[n] != 0) n++;          // fname[16], NUL-padded
        c.name.assign(reinterpret_cast<const char*>(e), n);
        c.offset      = r32(e + 16);
        c.size        = r32(e + 20);
        c.sample_rate = r32(e + 24);
        c.flags       = r32(e + 28);
        c.chunk_size  = r32(e + 32);
        idx.clips_.push_back(std::move(c));
    }
    return idx;
}

const BagClip* AudioIdx::find(const std::string& name) const {
    // Case-insensitive: the game resolves sound names from INI keys whose
    // casing is inconsistent (docs/re/audio.md).
    for (const BagClip& c : clips_)
        if (iequal(c.name, name)) return &c;
    return nullptr;
}

} // namespace ra2assets
