#pragma once
#include <cstdint>
namespace ra2engine {
class Mixer;
// Thin SDL audio sink: opens the default device at 22050 Hz mono S16 and
// keeps ~kTargetQueuedMs of mixed audio queued via SDL_QueueAudio (pumped
// once per frame from the main loop -- no audio-thread callback, no locks).
class AudioOutput {
public:
    bool open();                 // false: no device (game continues silent)
    void pump(Mixer& mixer);     // render+queue until the target depth is met
    void close();
    bool ok() const { return dev_ != 0; }
private:
    static constexpr int kTargetQueuedMs = 200;
    static constexpr int kChunkFrames = 1024;
    uint32_t dev_ = 0;
};
}  // namespace ra2engine
