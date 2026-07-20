#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace ra2assets {

struct WavInfo { int sample_rate = 0; int channels = 1; int bits = 16; };

// Decode a RIFF fmt-0x0011 (IMA-ADPCM) WAV -- the container EVA speech and
// music ship in (docs/re/audio.md). Shares its per-nibble arithmetic with
// the BAG chunked decoder (src/audio_bag.cpp) via src/ima_adpcm.hpp; the two
// containers use the identical IMA cascade, confirmed independently against
// two separate copies of the game's own decoder.
//
// Mono (channels==1) yields one sample per frame; stereo (channels==2, e.g.
// the menu theme theme.mix/grinder.wav) decodes the MS IMA-ADPCM stereo
// layout -- an 8-byte per-block dual header (L then R: int16 predictor + u8
// step index + u8 reserved) followed by interleaved 4-byte words (ch0 word
// then ch1 word) -- into interleaved L,R,L,R samples with out_info.channels=2.
//
// Throws std::runtime_error on: non-RIFF/WAVE input, missing fmt/data
// chunks, a fmt tag other than 0x0011, or channels outside {1, 2}.
std::vector<int16_t> load_wav_ima(const uint8_t* data, std::size_t len, WavInfo& out_info);

}  // namespace ra2assets
