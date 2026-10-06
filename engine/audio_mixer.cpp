#include "audio_mixer.hpp"

#include <algorithm>

namespace ra2engine {

int priority_from_string(const std::string& p) {
    // docs/re/audio-playback.md table @0x815018 (High -- read verbatim from
    // .rdata): LOWEST=0, LOW=1, NORMAL=2, HIGH=3, CRITICAL=4, default 2.
    if (p == "lowest") return 0;
    if (p == "low") return 1;
    if (p == "normal") return 2;
    if (p == "high") return 3;
    if (p == "critical") return 4;
    return 2;  // unknown -> NORMAL, the table's own default
}

int SoundBank::add(std::vector<int16_t> pcm) {
    clips_.push_back(std::move(pcm));
    return static_cast<int>(clips_.size()) - 1;
}

const std::vector<int16_t>& SoundBank::pcm(int id) const {
    return clips_.at(static_cast<std::size_t>(id));
}

int SoundBank::count() const { return static_cast<int>(clips_.size()); }

// CONFIRMED linear falloff (docs/re/audio-playback.md FUN_0074f8e0, High):
// gain = 1 - dist/range; Type=GLOBAL floors the result at MinVolume (the
// max(vol,MinVolume) branch is GLOBAL-only in gamemd; MinVolume is ignored
// for every other Type). `range <= 0` is treated as "no falloff" (full
// gain) -- a defensive fallback for a malformed/zero Range rather than a
// divide-by-zero.
int Mixer::gain_permille(const MixerSound& s, double dist_cells) {
    int g = 1000;
    if (s.range > 0.0) {
        const double f = 1.0 - dist_cells / s.range;
        g = f <= 0.0 ? 0 : (f >= 1.0 ? 1000 : static_cast<int>(f * 1000.0));
    }
    if (s.global) g = std::max(g, s.min_volume * 10);
    return g;
}

Mixer::Mixer(const SoundBank& bank, uint32_t seed)
    : bank_(bank), rng_(seed != 0 ? seed : 1u) {}
// xorshift32 with a zero state never leaves zero, so a caller-supplied seed
// of 0 is nudged to 1 -- audio-local RNG, deliberately never the sim's
// Randomizer (docs/re/audio-playback.md's RNG-determinism caveat).

void Mixer::register_sound(const std::string& key, const MixerSound& s) {
    sounds_[key] = s;
}

std::uint32_t Mixer::next_rand_() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

void Mixer::kill_oldest_with_key_(const std::string& key) {
    auto oldest = voices_.end();
    for (auto it = voices_.begin(); it != voices_.end(); ++it) {
        if (it->key != key) continue;
        if (oldest == voices_.end() || it->age < oldest->age) oldest = it;
    }
    if (oldest != voices_.end()) voices_.erase(oldest);
}

bool Mixer::play(const std::string& key, double dist_cells) {
    auto it = sounds_.find(key);
    if (it == sounds_.end() || it->second.variants.empty()) return false;
    const MixerSound& s = it->second;
    const int gain = gain_permille(s, dist_cells);
    if (gain <= 0) return false;  // inaudible: don't burn a voice on silence

    int same = 0;
    for (const Voice& v : voices_)
        if (v.key == key) ++same;

    if (s.limit > 0 && same >= s.limit) {
        // Reject-at-trigger: a documented approximation of vanilla's
        // start-then-cull-to-cap (docs/re/audio-playback.md FUN_004041d0,
        // "Decisions" section) -- both hold the same <=Limit invariant.
        if (!s.interrupt) return false;
        kill_oldest_with_key_(key);  // interrupt: steal same-event channel (Medium)
    } else if (static_cast<int>(voices_.size()) >= kMaxVoices) {
        // Vanilla drops the new sound when the pool is full -- it never
        // evicts a playing voice, regardless of priority (docs/re/
        // audio-playback.md FUN_00405190, "Decisions" section).
        return false;
    }

    const int clip = s.variants[next_rand_() % s.variants.size()];
    voices_.push_back({key, clip, 0, s.volume * gain, s.priority, age_counter_++, s.bus});
    return true;
}

void Mixer::set_bus_gain(Bus bus, int permille) {
    bus_gain_[static_cast<int>(bus)] = std::max(0, std::min(1000, permille));
}

int Mixer::bus_gain(Bus bus) const { return bus_gain_[static_cast<int>(bus)]; }

void Mixer::play_eva(int clip_id) {
    if (eva_clip_ == -1) {
        eva_clip_ = clip_id;
        eva_pos_ = 0;
    } else {
        eva_queue_.push_back(clip_id);
    }
}

void Mixer::stop_eva() {
    eva_clip_ = -1;
    eva_pos_ = 0;
    eva_queue_.clear();
}

void Mixer::stop(const std::string& key) {
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [&](const Voice& v) { return v.key == key; }),
                  voices_.end());
}

bool Mixer::playing(const std::string& key) const {
    for (const Voice& v : voices_)
        if (v.key == key) return true;
    return false;
}

int Mixer::active_voices() const { return static_cast<int>(voices_.size()); }
bool Mixer::eva_active() const { return eva_clip_ != -1; }
std::size_t Mixer::eva_queued() const { return eva_queue_.size(); }

void Mixer::render(int16_t* out, std::size_t frames) {
    for (std::size_t f = 0; f < frames; ++f) {
        std::int64_t acc = 0;

        for (Voice& v : voices_) {
            const std::vector<int16_t>& pcm = bank_.pcm(v.clip);
            if (v.pos < pcm.size()) {
                // int64 intermediate: pcm(<=32767) * vol_x_gain(<=100000)
                // overflows int32, so this must not be done in 32-bit.
                std::int64_t c = static_cast<std::int64_t>(pcm[v.pos]) * v.vol_x_gain / 100000;
                // [P8] bus volume; identity (1000) keeps the exact pre-P8 value.
                const int bg = bus_gain_[static_cast<int>(v.bus)];
                if (bg != 1000) c = c * bg / 1000;
                acc += c;
                ++v.pos;
            }
        }

        if (eva_clip_ != -1) {
            const std::vector<int16_t>& pcm = bank_.pcm(eva_clip_);
            if (eva_pos_ < pcm.size()) {
                // EVA: full volume, no attenuation; [P8] scaled by the Voice
                // bus only when it is not identity.
                std::int64_t c = pcm[eva_pos_];
                const int vg = bus_gain_[static_cast<int>(Bus::Voice)];
                if (vg != 1000) c = c * vg / 1000;
                acc += c;
                ++eva_pos_;
            }
            if (eva_pos_ >= pcm.size()) {
                if (!eva_queue_.empty()) {
                    eva_clip_ = eva_queue_.front();
                    eva_queue_.pop_front();
                    eva_pos_ = 0;
                } else {
                    eva_clip_ = -1;
                    eva_pos_ = 0;
                }
            }
        }

        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        out[f] = static_cast<int16_t>(acc);

        voices_.erase(
            std::remove_if(voices_.begin(), voices_.end(),
                            [this](const Voice& v) { return v.pos >= bank_.pcm(v.clip).size(); }),
            voices_.end());
    }
}

}  // namespace ra2engine
