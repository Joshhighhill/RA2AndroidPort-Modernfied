#include "ra2assets/bink.hpp"
#include <stdexcept>

namespace ra2assets {
namespace {
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

BinkFile BinkFile::parse(std::vector<uint8_t> bytes) {
    // Header is 44 bytes (0x00..0x2B), with the audio-track count at 0x28. See
    // docs/re/bink.md §1 for the full field table.
    if (bytes.size() < 44) throw std::runtime_error("bink: shorter than header");
    const uint8_t* d = bytes.data();
    // Magic: "BIK" + revision letter (RA2 uses 'i'). The container layout is
    // shared across revisions, so gate on the 3-byte "BIK" prefix.
    if (d[0] != 'B' || d[1] != 'I' || d[2] != 'K')
        throw std::runtime_error("bink: bad magic (not \"BIK\")");

    BinkFile bf;
    const uint32_t num_frames = rd32(d + 0x08);
    bf.width = static_cast<int>(rd32(d + 0x14));
    bf.height = static_cast<int>(rd32(d + 0x18));
    bf.fps_num = static_cast<int>(rd32(d + 0x1C));
    bf.fps_den = static_cast<int>(rd32(d + 0x20));
    const uint32_t num_tracks = rd32(d + 0x28);
    bf.num_frames = static_cast<int>(num_frames);

    // Frame index sits after the header and the (12 bytes each) audio-track
    // headers: 0x2C + 12*num_tracks. It has num_frames+1 u32 entries.
    const std::size_t index_start = 0x2Cu + static_cast<std::size_t>(num_tracks) * 12u;
    const std::size_t count = static_cast<std::size_t>(num_frames) + 1u;
    // Bounds-safe: index_start + 4*count must fit inside the file.
    if (count > (bytes.size() - index_start) / 4u ||
        index_start > bytes.size())
        throw std::runtime_error("bink: truncated frame index");

    bf.frame_offsets.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        bf.frame_offsets[i] = rd32(d + index_start + i * 4u);

    bf.data = std::move(bytes);
    return bf;
}

bool BinkFile::is_keyframe(int f) const {
    if (f < 0 || static_cast<std::size_t>(f) >= frame_offsets.size())
        throw std::out_of_range("bink: frame index out of range");
    return (frame_offsets[static_cast<std::size_t>(f)] & 1u) != 0u;
}

std::pair<const uint8_t*, std::size_t> BinkFile::frame(int f) const {
    if (f < 0 || f >= num_frames)
        throw std::out_of_range("bink: frame index out of range");
    const uint32_t off = frame_offsets[static_cast<std::size_t>(f)] & ~1u;
    const uint32_t end = frame_offsets[static_cast<std::size_t>(f) + 1u] & ~1u;
    if (end < off || end > data.size())
        throw std::runtime_error("bink: frame range outside file");
    return {data.data() + off, static_cast<std::size_t>(end - off)};
}

}  // namespace ra2assets
