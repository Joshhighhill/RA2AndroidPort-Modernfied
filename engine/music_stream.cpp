#include "music_stream.hpp"
#include "pcm_wav.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

#include "ra2assets/wav.hpp"

namespace ra2engine {

MusicStream::MusicStream(std::function<std::vector<uint8_t>()> fetch)
    : worker_([this, f = std::move(fetch)]() mutable { run(std::move(f)); }) {}

MusicStream::~MusicStream() {
    stop_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool MusicStream::finished() const {
    std::lock_guard<std::mutex> lk(mu_);
    return failed_.load() || (decoder_done_ && count_ == 0);
}

bool MusicStream::wait_ready(int ms) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(ms), [&] { return ready_ || failed_.load(); });
    return ready_ && !failed_.load();
}

std::size_t MusicStream::read(int16_t* lr, std::size_t frames) {
    std::size_t n = 0;
    {
        std::lock_guard<std::mutex> lk(mu_);
        n = std::min(frames, count_ / 2);
        const std::size_t cap = ring_.size();
        for (std::size_t i = 0; i < n * 2;) {
            const std::size_t run = std::min(n * 2 - i, cap - head_);
            std::copy(ring_.begin() + static_cast<std::ptrdiff_t>(head_),
                      ring_.begin() + static_cast<std::ptrdiff_t>(head_ + run), lr + i);
            head_ = (head_ + run) % cap;
            i += run;
        }
        count_ -= n * 2;
    }
    cv_.notify_all();
    return n;
}

namespace {
struct PcmReader {
    ra2engine::PcmWavView v;
    size_t pos = 0;
    bool next_block(std::vector<int16_t>& out) {
        if (pos >= v.frames()) return false;
        ra2engine::pcmw_read(v, pos, 4096, out);
        pos += 4096;
        return true;
    }
};
}  // namespace

void MusicStream::run(std::function<std::vector<uint8_t>()> fetch) {
    try {
        std::vector<uint8_t> file = fetch();
        PcmWavView pcmv;
        const bool is_pcm = parse_pcm_wav(file.data(), file.size(), pcmv);
        auto decode = [&](auto& src, int ch, int rate, double total_frames) {
            length_ms_.store(static_cast<long>(total_frames * 1000.0 / rate));
            {
                std::lock_guard<std::mutex> lk(mu_);
                ready_ = true;
            }
            cv_.notify_all();
            const double step = static_cast<double>(rate) / 22050.0;
            double t = 0.0;
            std::vector<int16_t> in;
            std::vector<int16_t> blk, out;
            bool eof = false;
            while (!stop_.load()) {
                blk.clear();
                if (!eof && !src.next_block(blk)) eof = true;
                if (ch == 1) {
                    for (int16_t s : blk) { in.push_back(s); in.push_back(s); }
                } else {
                    in.insert(in.end(), blk.begin(), blk.end());
                }
                const std::size_t avail = in.size() / 2;
                out.clear();
                if (step == 1.0) {
                    out.swap(in);
                    in.clear();
                } else {
                    while (t + 1.0 < static_cast<double>(avail) || (eof && t < static_cast<double>(avail))) {
                        const std::size_t k = static_cast<std::size_t>(t);
                        const double fr = t - static_cast<double>(k);
                        const std::size_t k1 = std::min(k + 1, avail - 1);
                        for (int c = 0; c < 2; ++c) {
                            const int a = in[2 * k + c], b = in[2 * k1 + c];
                            out.push_back(static_cast<int16_t>(a + (b - a) * fr));
                        }
                        t += step;
                    }
                    const std::size_t drop = std::min(avail, static_cast<std::size_t>(t));
                    in.erase(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(drop * 2));
                    t -= static_cast<double>(drop);
                }
                {
                    std::unique_lock<std::mutex> lk(mu_);
                    std::size_t i = 0;
                    while (i < out.size()) {
                        cv_.wait(lk, [&] { return stop_.load() || count_ < ring_.size(); });
                        if (stop_.load()) break;
                        const std::size_t cap = ring_.size();
                        const std::size_t room = cap - count_;
                        const std::size_t n = std::min(room, out.size() - i);
                        for (std::size_t k = 0; k < n; ++k) ring_[(head_ + count_ + k) % cap] = out[i + k];
                        count_ += n;
                        i += n;
                    }
                    if (stop_.load()) break;
                }
                if (eof && in.empty()) break;
                if (eof && step != 1.0 && in.size() / 2 <= 1) break;
            }
        };
        if (is_pcm) {
            PcmReader r{pcmv, 0};
            decode(r, pcmv.channels, pcmv.rate, static_cast<double>(pcmv.frames()));
        } else {
            ra2assets::WavImaStream wav = ra2assets::WavImaStream::open(std::move(file));
            const int rate = wav.info().sample_rate > 0 ? wav.info().sample_rate : 22050;
            decode(wav, wav.info().channels, rate, static_cast<double>(wav.total_frames()));
        }
    } catch (const std::exception&) {
        failed_.store(true);
    }
    {
        std::lock_guard<std::mutex> lk(mu_);
        decoder_done_ = true;
    }
    cv_.notify_all();
}

}  // namespace ra2engine
