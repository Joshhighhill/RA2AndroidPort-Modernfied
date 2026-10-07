#pragma once
#include <atomic>
#include <cstdint>
namespace ra2engine {
class Mixer;
// SDL audio sink. W2-GA-23/P4: opens the default device at 22050 Hz STEREO S16
// and mixes on SDL's audio thread (callback) through Mixer::render_stereo, so
// a long game frame (asset load, a slow update) can no longer underrun the
// output the way the old 200 ms queue pumped from update() did. The mixer is
// internally locked; the device buffer is kChunkFrames (~93 ms) deep.
class AudioOutput {
public:
    AudioOutput() = default;
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    ~AudioOutput() { close(); }
    bool open();                 // false: no device (game continues silent)
    // Attaches `mixer` to the running device (idempotent; cheap per frame).
    // Kept under its old name so every existing per-frame call site works.
    void pump(Mixer& mixer);
    void detach();               // stop pulling from the mixer (before it dies)
    void close();
    bool ok() const { return dev_ != 0; }
private:
    static void callback(void* self, uint8_t* stream, int len);
    static constexpr int kChunkFrames = 2048;
    uint32_t dev_ = 0;
    bool inited_ = false;
    std::atomic<Mixer*> mixer_{nullptr};
};
}  // namespace ra2engine
