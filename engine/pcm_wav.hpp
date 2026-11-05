#pragma once
// Plain (uncompressed) RIFF/WAVE for mod audio: integer PCM 8/16/24/32-bit or 32-bit float,
// mono or stereo. (The stock game files are IMA ADPCM; those keep using the existing decoder.)
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ra2engine {

struct PcmWavView {
    int channels = 0, rate = 0, bits = 0;
    bool is_float = false;
    const uint8_t* data = nullptr;
    size_t bytes = 0;
    size_t frames() const {
        return channels > 0 && bits > 0 ? bytes / (static_cast<size_t>(channels) * static_cast<size_t>(bits / 8)) : 0;
    }
};

inline uint32_t pcmw_u32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline int pcmw_u16(const uint8_t* p) { return p[0] | (p[1] << 8); }

// True when `d` is a RIFF/WAVE in a format we can read; fills the view (pointing into `d`).
inline bool parse_pcm_wav(const uint8_t* d, size_t n, PcmWavView& v) {
    if (n < 12 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0) return false;
    size_t pos = 12;
    bool have_fmt = false;
    int tag = 0;
    while (pos + 8 <= n) {
        const uint32_t sz = pcmw_u32(d + pos + 4);
        const uint8_t* body = d + pos + 8;
        const size_t avail = n - (pos + 8);
        if (std::memcmp(d + pos, "fmt ", 4) == 0 && sz >= 16 && avail >= 16) {
            tag = pcmw_u16(body);
            v.channels = pcmw_u16(body + 2);
            v.rate = static_cast<int>(pcmw_u32(body + 4));
            v.bits = pcmw_u16(body + 14);
            if (tag == 0xFFFE && sz >= 26 && avail >= 26) tag = pcmw_u16(body + 24);   // EXTENSIBLE
            have_fmt = true;
        } else if (std::memcmp(d + pos, "data", 4) == 0) {
            if (!have_fmt) return false;
            v.data = body;
            v.bytes = sz > avail ? avail : sz;
            v.is_float = (tag == 3);
            const bool int_ok = tag == 1 && (v.bits == 8 || v.bits == 16 || v.bits == 24 || v.bits == 32);
            const bool flt_ok = tag == 3 && v.bits == 32;
            return (int_ok || flt_ok) && v.channels >= 1 && v.channels <= 2 && v.rate > 0;
        }
        pos += 8 + static_cast<size_t>(sz) + (sz & 1);
    }
    return false;
}

// Appends frames [f0, f0+nf) as interleaved int16 (channel count unchanged).
inline void pcmw_read(const PcmWavView& v, size_t f0, size_t nf, std::vector<int16_t>& out) {
    const size_t bps = static_cast<size_t>(v.bits / 8), total = v.frames();
    if (f0 >= total) return;
    if (f0 + nf > total) nf = total - f0;
    const uint8_t* p = v.data + f0 * static_cast<size_t>(v.channels) * bps;
    for (size_t i = 0; i < nf * static_cast<size_t>(v.channels); ++i, p += bps) {
        int s;
        if (v.is_float) {
            float f;
            std::memcpy(&f, p, 4);
            if (f > 1.f) f = 1.f;
            if (f < -1.f) f = -1.f;
            s = static_cast<int>(f * 32767.f);
        } else if (v.bits == 8)  s = (static_cast<int>(p[0]) - 128) << 8;
        else if (v.bits == 16)   s = static_cast<int16_t>(p[0] | (p[1] << 8));
        else if (v.bits == 24)   s = static_cast<int16_t>(p[1] | (p[2] << 8));
        else                     s = static_cast<int16_t>(p[2] | (p[3] << 8));
        out.push_back(static_cast<int16_t>(s));
    }
}

}  // namespace ra2engine