#include "audio_sdl.hpp"

#include <SDL.h>

#include "audio_mixer.hpp"

namespace ra2engine {

bool AudioOutput::open() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = static_cast<Uint16>(kChunkFrames);
    want.callback = nullptr;  // queue-push model: no callback, no lock

    SDL_AudioSpec have;
    SDL_zero(have);
    // allowed_changes=0: require the exact format we asked for, so the
    // mixer's fixed 22050 Hz mono S16 render never needs runtime resampling.
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, /*allowed_changes=*/0);
    if (dev_ == 0) return false;

    SDL_PauseAudioDevice(dev_, 0);  // unpause -- start draining the queue
    return true;
}

void AudioOutput::pump(Mixer& mixer) {
    if (dev_ == 0) return;
    const Uint32 target_bytes =
        static_cast<Uint32>(22050 * 2 /*bytes/sample*/ * kTargetQueuedMs / 1000);
    while (SDL_GetQueuedAudioSize(dev_) < target_bytes) {
        int16_t buf[kChunkFrames];
        mixer.render(buf, kChunkFrames);
        SDL_QueueAudio(dev_, buf, sizeof buf);
    }
}

void AudioOutput::close() {
    if (dev_ != 0) {
        SDL_CloseAudioDevice(dev_);
        dev_ = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace ra2engine
