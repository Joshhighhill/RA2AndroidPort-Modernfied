#include "audio_sdl.hpp"

#include <SDL.h>

#include <cstring>

#include "audio_mixer.hpp"

namespace ra2engine {

void AudioOutput::callback(void* self, uint8_t* stream, int len) {
    AudioOutput* o = static_cast<AudioOutput*>(self);
    Mixer* m = o->mixer_.load();
    const std::size_t frames = static_cast<std::size_t>(len) / 4;   // S16 stereo
    if (!m) {
        std::memset(stream, 0, static_cast<std::size_t>(len));
        return;
    }
    m->render_stereo(reinterpret_cast<int16_t*>(stream), frames);
}

bool AudioOutput::open() {
    if (dev_ != 0) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    inited_ = true;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = static_cast<Uint16>(kChunkFrames);
    want.callback = &AudioOutput::callback;
    want.userdata = this;

    SDL_AudioSpec have;
    SDL_zero(have);
    // allowed_changes=0: SDL converts to the hardware format itself, so the
    // mixer's fixed 22050 Hz stereo S16 render never resamples at runtime.
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, /*allowed_changes=*/0);
    if (dev_ == 0) return false;

    SDL_PauseAudioDevice(dev_, 0);  // run the callback (silence until attached)
    return true;
}

void AudioOutput::pump(Mixer& mixer) {
    if (dev_ == 0) return;
    mixer_.store(&mixer);
}

void AudioOutput::detach() {
    if (dev_ == 0) return;
    SDL_LockAudioDevice(dev_);
    mixer_.store(nullptr);
    SDL_UnlockAudioDevice(dev_);
}

void AudioOutput::close() {
    if (dev_ != 0) {
        SDL_CloseAudioDevice(dev_);   // stops + joins the callback thread
        dev_ = 0;
    }
    mixer_.store(nullptr);
    if (inited_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        inited_ = false;
    }
}

}  // namespace ra2engine
