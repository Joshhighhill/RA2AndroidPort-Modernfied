#include "ra2assets/wav.hpp"
#include "ima_adpcm.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ra2assets {
namespace {
uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
}  // namespace

std::vector<int16_t> load_wav_ima(const uint8_t* data, std::size_t len, WavInfo& out_info) {
    if (len < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
        throw std::runtime_error("wav: not a RIFF WAVE");
    std::size_t pos = 12;
    uint16_t tag = 0, channels = 0, block_align = 0;
    uint32_t rate = 0, fact_samples = 0;
    const uint8_t* body = nullptr;
    uint32_t body_len = 0;
    bool have_fmt = false, have_fact = false;
    while (pos + 8 <= len) {
        const uint32_t csz = rd32(data + pos + 4);
        const uint8_t* c = data + pos + 8;
        if (pos + 8 + csz > len) throw std::runtime_error("wav: truncated chunk");
        if (!std::memcmp(data + pos, "fmt ", 4)) {
            if (csz < 16) throw std::runtime_error("wav: short fmt");
            tag = rd16(c); channels = rd16(c + 2); rate = rd32(c + 4);
            block_align = rd16(c + 12);
            have_fmt = true;
        } else if (!std::memcmp(data + pos, "fact", 4) && csz >= 4) {
            fact_samples = rd32(c);
            have_fact = true;
        } else if (!std::memcmp(data + pos, "data", 4)) {
            body = c; body_len = csz;
        }
        pos += 8 + csz + (csz & 1);  // chunks are word-aligned
    }
    if (!have_fmt || !body) throw std::runtime_error("wav: missing fmt/data");
    if (tag != 0x0011) throw std::runtime_error("wav: fmt tag not IMA-ADPCM (0x0011)");
    if (channels != 1)
        throw std::runtime_error("wav: only mono IMA supported (docs/re/audio.md stereo note)");
    if (block_align <= 4) throw std::runtime_error("wav: bad block align");

    std::vector<int16_t> pcm;
    for (uint32_t off = 0; off + 4 <= body_len; off += block_align) {
        const uint32_t blk = std::min<uint32_t>(block_align, body_len - off);
        const uint8_t* b = body + off;
        int sample = static_cast<int16_t>(rd16(b));   // predictor, emitted verbatim
        int index = b[2];
        if (index > 88) throw std::runtime_error("wav: bad block step index");
        pcm.push_back(static_cast<int16_t>(sample));
        for (uint32_t i = 4; i < blk; ++i) {
            pcm.push_back(static_cast<int16_t>(ima::decode_nibble(b[i] & 0xf, sample, index)));
            pcm.push_back(static_cast<int16_t>(ima::decode_nibble(b[i] >> 4, sample, index)));
        }
    }
    if (have_fact && fact_samples < pcm.size()) pcm.resize(fact_samples);
    out_info.sample_rate = static_cast<int>(rate);
    out_info.channels = 1;
    out_info.bits = 16;
    return pcm;
}
}  // namespace ra2assets
