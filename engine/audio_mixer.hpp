#pragma once
// Pure software mixer: no SDL/OS/file deps (same pattern as combat.hpp/
// occupancy.hpp), so it is unit-testable and compiled into both
// ra2assets_tests and ra2engine from a single source of truth (see
// CMakeLists.txt / cmake/ra2_sources.cmake). All PCM is 22050 Hz mono S16.
//
// Calibration authority: docs/re/audio-playback.md ("Decisions for
// ra2engine" section). That doc AMENDS the original plan after a Ghidra
// hunt; the semantics implemented here are the amended ones, not the
// plan's original guesses:
//   - Priority is the real 5-level scale (LOWEST=0..CRITICAL=4, unknown
//     string -> NORMAL=2), not the plan's 4-level 0..3.
//   - `Limit` is enforced by REJECTING the (Limit+1)-th play() at trigger
//     time (a documented approximation of vanilla's start-then-cull-to-cap
//     voice stealing, FUN_004041d0) -- both hold the same <=Limit bound.
//   - `interrupt` (Control=INTERRUPT) steals the oldest still-playing
//     voice of the SAME key rather than rejecting (FUN_004041d0, Medium).
//   - A full voice pool DROPS the new sound; vanilla's SFX allocator
//     (FUN_00405190) reclaims only finished voices and never evicts a
//     playing one by priority, no matter how high the new sound's
//     priority is (Medium).
//   - Attenuation is CONFIRMED linear, `gain = 1 - dist/range`, full at
//     dist=0, silent at dist>=range (FUN_0074f8e0, High). `Type=GLOBAL`
//     (MixerSound::global) floors the result at `min_volume` (scaled to
//     permille) instead of letting it reach silence -- MinVolume is
//     otherwise ignored (GLOBAL-only branch). The caller supplies
//     dist_cells in CELLS; the game's own metric is Chebyshev screen
//     pixels (`max(dx, 2|dy|)`) divided by 60 px/cell -- Task 6 computes
//     that projection and passes the resulting cell distance in here.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace ra2engine {

// [P8 / docs/re/options.md §5] The original routes every sound through one of
// three volume buses set from RA2.INI [Audio]: SoundVolume -> the SFX voice
// pool (DAT_00831458), VoiceVolume -> the speech/EVA streamer (DAT_00831450),
// ScoreVolume -> the music/theme player (DAT_00831454). Linear gain.
enum class AudioBus { Sound = 0, Voice = 1, Score = 2 };

// One [SoundList] event's mixer-relevant fields (Task 6 fills these from a
// parsed SoundEntry). `variants` are SoundBank clip ids; a sound with no
// variants can never play (register_sound accepts it, play() rejects it).
struct MixerSound {
    std::vector<int> variants;   // SoundBank clip ids (>=1 to be playable)
    int volume = 80;             // 0..100
    // 5-level scale (docs/re/audio-playback.md table @0x815018): lowest=0,
    // low=1, normal=2, high=3, critical=4. Stored verbatim from
    // priority_from_string; the mixer itself never evicts by priority (see
    // the full-pool DROP rule above), so this is carried on Voice mostly
    // for a future consumer, not consulted by play()/render() today.
    int priority = 1;
    int limit = 5;                // max simultaneous instances; <=0 unlimited
    bool interrupt = false;       // Control=interrupt: steal oldest same-key voice
    bool global = false;          // Type contains GLOBAL: MinVolume floor (never silent)
    double range = 10.0;          // cells
    int min_volume = 50;          // 0..100; GLOBAL-only floor
    AudioBus bus = AudioBus::Sound;   // [P8] volume bus (SFX by default)
};

// AMENDED: real 5-level scale (docs/re/audio-playback.md, table @0x815018):
// lowest=0 low=1 normal=2 high=3 critical=4, unknown -> 2 (NORMAL, the
// table's own default). Callers are expected to pass an already-lowercased
// token (Task 2's Sounds reader lowercases Priority at parse time).
int priority_from_string(const std::string& p);

// Owns decoded PCM clips (22050 Hz mono S16) by integer id. Pure storage --
// no decoding, no file I/O; Task 6 fills it from already-decoded audio.
class SoundBank {
public:
    int add(std::vector<int16_t> pcm);            // -> clip id
    const std::vector<int16_t>& pcm(int id) const;
    int count() const;

private:
    std::vector<std::vector<int16_t>> clips_;
};

// The mixer proper: keyed sound registry, a bounded voice pool, a
// single-active-plus-FIFO EVA speech slot, and a sample-accurate renderer.
class Mixer {
public:
    static constexpr int kMaxVoices = 16;
    using Bus = AudioBus;

    // [P8] Per-bus gain in permille (0..1000, clamped). Defaults to 1000 on
    // every bus = identity: render() takes the exact pre-P8 arithmetic path
    // for an identity bus, so no gate/test output moves unless the
    // interactive Options settings were applied. The EVA slot mixes on
    // Bus::Voice.
    void set_bus_gain(Bus bus, int permille);
    int  bus_gain(Bus bus) const;

    // `bank` must outlive the Mixer (stored by reference; Task 6 owns both
    // with the same lifetime, same pattern as Occupancy/World elsewhere).
    // `seed` initializes the audio-local xorshift32 RNG used for variant
    // selection -- deliberately NOT the sim's Randomizer (docs/re/
    // audio-playback.md's RNG-determinism caveat: audio must never perturb
    // sim RNG state so --demo-combat stays byte-identical).
    Mixer(const SoundBank& bank, uint32_t seed);

    // Registers/replaces the MixerSound for `key` (e.g. "GIAttack").
    void register_sound(const std::string& key, const MixerSound& s);

    // Triggers one instance of `key` at `dist_cells` (0.0 = full gain
    // subject only to the GLOBAL floor). Returns false when: the key is
    // unknown, it has no variants, it is inaudible (gain_permille <= 0,
    // so no voice is spent on a silent play), Limit is already at cap and
    // `interrupt` is false, or the pool is full (kMaxVoices) with no
    // Limit-driven steal to make room -- see the class doc comment above
    // for which of these is a documented approximation of vanilla.
    bool play(const std::string& key, double dist_cells = 0.0);

    // Queues an EVA line by SoundBank clip id: full volume, no gain
    // scaling, one active at a time (FIFO behind it). A new call starts
    // playing immediately iff no EVA line is currently active.
    void play_eva(int clip_id);
    // [G-80] EVA Type=INTERRUPT flush (audio-playback.md §8): stop the playing
    // speech and drop everything queued behind it.
    void stop_eva();
    // [G-82] Stop every voice playing `key` (music track change).
    void stop(const std::string& key);
    // [G-82] True while any voice of `key` is still playing.
    bool playing(const std::string& key) const;

    // Renders `frames` samples into `out` (must hold >= frames int16_t):
    // sums every active voice's contribution plus the active EVA line,
    // clamps to the int16 range, and advances all playback positions,
    // retiring any voice/EVA clip that finishes during this call.
    void render(int16_t* out, std::size_t frames);

    int active_voices() const;          // non-EVA voices currently playing
    bool eva_active() const;
    std::size_t eva_queued() const;     // waiting behind the active EVA line

    // CONFIRMED linear falloff (docs/re/audio-playback.md, FUN_0074f8e0,
    // High): gain = 1 - dist/range, full at 0, silent at range. Type=GLOBAL
    // events are floored at min_volume (never silent) instead. The caller
    // supplies dist in CELLS (the game divides its pixel metric by 60 --
    // Task 6 computes the exact Chebyshev metric). Returns 0..1000.
    static int gain_permille(const MixerSound& s, double dist_cells);

private:
    // One live (non-EVA) voice. `vol_x_gain` is volume(0..100) *
    // gain_permille(0..1000), precomputed at play() time (attenuation is
    // NOT re-evaluated per frame here -- docs/re/audio-playback.md notes
    // vanilla re-attenuates live voices every frame; this pure mixer keeps
    // the plan's simpler latch-at-trigger model, a documented
    // simplification Task 6 may revisit). `age` breaks ties for
    // interrupt's oldest-same-key steal.
    struct Voice {
        std::string key;
        int clip;
        std::size_t pos = 0;
        int vol_x_gain;      // volume * gain_permille, 0..100000
        int priority;
        std::uint64_t age;
        Bus bus = Bus::Sound;   // [P8] latched from MixerSound::bus at play()
    };

    std::uint32_t next_rand_();
    void kill_oldest_with_key_(const std::string& key);

    const SoundBank& bank_;
    std::map<std::string, MixerSound> sounds_;
    std::vector<Voice> voices_;

    int eva_clip_ = -1;
    std::size_t eva_pos_ = 0;
    std::deque<int> eva_queue_;

    std::uint32_t rng_;
    std::uint64_t age_counter_ = 0;
    int bus_gain_[3] = {1000, 1000, 1000};   // [P8] Sound / Voice / Score permille
};

}  // namespace ra2engine
