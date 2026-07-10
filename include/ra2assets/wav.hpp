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
// Throws std::runtime_error on: non-RIFF/WAVE input, missing fmt/data
// chunks, a fmt tag other than 0x0011, or channels != 1 (all real EVA
// speech is mono; docs/re/audio.md's stereo note documents the game's
// N-channel chunk shape for anyone adding stereo support later).
std::vector<int16_t> load_wav_ima(const uint8_t* data, std::size_t len, WavInfo& out_info);

}  // namespace ra2assets
