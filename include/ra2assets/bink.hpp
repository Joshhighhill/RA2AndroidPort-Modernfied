#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace ra2assets {

// A parsed Bink 1 ("BIKi") video container: the 44-byte little-endian header
// plus the (num_frames+1)-entry frame index. This is the CONTAINER only -- no
// video decoding. [RE: docs/re/bink.md §1]
//
// `frame_offsets` holds the (num_frames+1) raw index entries as stored in the
// file. Entry i is the absolute file offset of frame i's data, and its bit 0 is
// the keyframe flag (1 = keyframe) -- so the raw entry is kept and bit 0 is
// masked off wherever an actual offset is needed (is_keyframe / frame). The
// final entry equals the total file size. [RE: docs/re/bink.md §1 "Frame index"]
struct BinkFile {
    int width = 0, height = 0;
    int num_frames = 0;
    int fps_num = 0, fps_den = 0;
    std::vector<uint32_t> frame_offsets;   // size num_frames+1, raw (bit0 = keyframe)
    std::vector<uint8_t> data;             // the whole file, verbatim

    // True iff frame f's index entry has bit 0 set (keyframe). In the RA2 files
    // only frame 0 is a keyframe.
    bool is_keyframe(int f) const;

    // Frame f's video packet byte range: [offset[f] & ~1, offset[f+1] & ~1),
    // pointing into `data`. Throws std::out_of_range if f is not in [0,num_frames).
    std::pair<const uint8_t*, std::size_t> frame(int f) const;

    // Parse a Bink container from a whole-file byte buffer. Throws
    // std::runtime_error on a bad magic ("BIK" prefix) or a truncated
    // header/frame-index.
    static BinkFile parse(std::vector<uint8_t> bytes);
};

}  // namespace ra2assets
