// pres_audio.inc -- G-80 EVA, G-81 unit voices/reports for every unit, G-82
// in-game music, plus the presentation event log the --scene-shot harness
// prints. Included by main.cpp inside its anonymous namespace, after
// init_audio (it extends AudioSystem's bank/mixer, never replaces it).
//
// Data sources (all original game files, nothing hardcoded):
//   evamd.ini  [DialogList] + [EVA_*] Allied/Russian/Yuri clip, Priority, Type
//   soundmd.ini sound events (voices, reports, anim Report=, building sounds)
//   thememd.ini [Themes] playlist (Normal=yes tracks) -> thememd.mix/<Sound>.wav
//   clips: langmd.mix/audiomd.mix (YR) before language.mix/audio.mix (RA2),
//   the expansion-overrides-base mix order the game itself uses.

// G-88: the game's own font (cache.mix 12metfnt.fnt) for in-game HUD text;
// set by Presentation::init, null in gates (callers keep their bitmap font).
const ra2assets::Font* g_game_font = nullptr;
const ra2assets::Palette* g_game_font_pal = nullptr;

// ---- presentation event log (harness evidence; never read by a gate) -------
bool g_pres_log_on = false;
std::vector<std::string> g_pres_log;
inline void pres_log(long frame, const std::string& what) {
    if (!g_pres_log_on) return;
    g_pres_log.push_back("[pres] f=" + std::to_string(frame) + " " + what);
}

// Strip an inline ';' comment and surrounding whitespace/tabs (evamd.ini lines
// carry trailing tabs: "Allied=ceva050\t").
inline std::string pres_clean(std::string v) {
    const std::size_t sc = v.find(';');
    if (sc != std::string::npos) v.resize(sc);
    std::size_t a = 0, b = v.size();
    while (a < b && std::isspace(static_cast<unsigned char>(v[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(v[b - 1]))) --b;
    return v.substr(a, b - a);
}
inline std::string pres_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
inline std::string pres_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Downmix to mono and resample to the 22050 Hz sink (linear). The game's
// clips are 22050 Hz; a few BAG clips are 11025/44100 and would otherwise play
// at the wrong pitch/length.
inline std::vector<int16_t> pres_to_sink(std::vector<int16_t> pcm, int rate, int channels) {
    if (channels == 2) {
        std::vector<int16_t> mono(pcm.size() / 2);
        for (std::size_t i = 0; i < mono.size(); ++i)
            mono[i] = static_cast<int16_t>((int(pcm[2 * i]) + int(pcm[2 * i + 1])) / 2);
        pcm.swap(mono);
    }
    if (rate > 0 && rate != 22050 && !pcm.empty()) {
        const double step = static_cast<double>(rate) / 22050.0;
        const std::size_t n = static_cast<std::size_t>(pcm.size() / step);
        std::vector<int16_t> out(n);
        for (std::size_t i = 0; i < n; ++i) {
            const double s = i * step;
            const std::size_t k = static_cast<std::size_t>(s);
            const double f = s - k;
            const int a = pcm[std::min(k, pcm.size() - 1)];
            const int b = pcm[std::min(k + 1, pcm.size() - 1)];
            out[i] = static_cast<int16_t>(a + (b - a) * f);
        }
        pcm.swap(out);
    }
    return pcm;
}

// One evamd.ini entry [RE: audio-playback.md §8: per-side clip, Priority
// LOW=0 NORMAL=1 IMPORTANT=2 CRITICAL=3 (default NORMAL), Type STANDARD=0
// QUEUE=1 INTERRUPT=2 QUEUED_INTERRUPT=3 (default STANDARD)].
struct EvaDef {
    std::string clip[3];   // 0 Allied, 1 Russian, 2 Yuri
    int priority = 1;
    int type = 0;
};

class PresAudio {
public:
    // Loads soundmd/evamd/thememd and the clip containers. `audio` may be
    // disabled (no device / --no-audio): everything still runs and logs, so the
    // trigger logic is observable headless; only the mixer calls are skipped.
    void open(const std::string& dir, AudioSystem* audio, bool want_clips) {
        audio_ = audio;
        try {
            snd_ini_ = std::make_unique<Ini>(load_ini_from_ra2md(dir, "soundmd.ini"));
            sounds_ = std::make_unique<ra2assets::Sounds>(ra2assets::Sounds::load(*snd_ini_));
        } catch (const std::exception& ex) {
            std::cerr << "[pres-audio] soundmd.ini unavailable: " << ex.what() << "\n";
        }
        try {
            Ini eva = load_ini_from_ra2md(dir, "evamd.ini");
            for (const auto& kv : eva.section("DialogList")) {
                const std::string name = pres_clean(kv.second);
                if (name.empty() || !eva.has(name)) continue;
                EvaDef d;
                d.clip[0] = pres_clean(eva.get(name, "Allied"));
                d.clip[1] = pres_clean(eva.get(name, "Russian"));
                d.clip[2] = pres_clean(eva.get(name, "Yuri"));
                const std::string p = pres_upper(pres_clean(eva.get(name, "Priority", "NORMAL")));
                d.priority = p == "LOW" ? 0 : p == "IMPORTANT" ? 2 : p == "CRITICAL" ? 3 : 1;
                const std::string t = pres_upper(pres_clean(eva.get(name, "Type", "STANDARD")));
                d.type = t == "QUEUE" ? 1 : t == "INTERRUPT" ? 2 : t == "QUEUED_INTERRUPT" ? 3 : 0;
                eva_[name] = d;
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pres-audio] evamd.ini unavailable: " << ex.what() << "\n";
        }
        try {
            Ini th = load_ini_from_ra2md(dir, "thememd.ini");
            for (const auto& kv : th.section("Themes")) {
                const std::string sec = pres_clean(kv.second);
                if (sec.empty() || !th.has(sec)) continue;
                Theme t;
                t.section = sec;
                t.sound = pres_clean(th.get(sec, "Sound"));
                t.name = pres_clean(th.get(sec, "Name"));   // W2-UI-14 jukebox label (CSF)
                t.normal = pres_lower(pres_clean(th.get(sec, "Normal", "yes")))[0] != 'n';
                t.repeat = pres_lower(pres_clean(th.get(sec, "Repeat", "no")))[0] == 'y';
                if (!t.sound.empty()) themes_.push_back(t);
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pres-audio] thememd.ini unavailable: " << ex.what() << "\n";
        }
        if (!want_clips) return;
        // Clip containers: YR first, RA2 second.
        const std::pair<const char*, const char*> chain[] = {{"langmd.mix", "audiomd.mix"},
                                                             {"language.mix", "audio.mix"}};
        for (const auto& [top, nested] : chain) {
            try {
                MixFile t = MixFile::open(ra2assets::read_file(dir + top));
                auto m = std::make_unique<MixFile>(t.open_nested(nested));
                Bag b;
                if (const MixEntry* ie = m->find("audio.idx")) {
                    auto ib = m->read(*ie);
                    b.idx = ra2assets::AudioIdx::load(ib.data(), ib.size());
                    if (const MixEntry* be = m->find("audio.bag")) b.bag = m->read(*be);
                }
                bags_.push_back(std::move(b));
                wav_mixes_.push_back(std::move(m));
            } catch (const std::exception& ex) {
                std::cerr << "[pres-audio] " << top << "/" << nested << " unavailable: " << ex.what()
                          << "\n";
            }
        }
        try {
            theme_mix_ = std::make_unique<MixFile>(
                MixFile::open(ra2assets::read_file(dir + "thememd.mix")));
        } catch (const std::exception&) {}
        std::cerr << "[pres-audio] eva=" << eva_.size() << " themes=" << themes_.size()
                  << " bags=" << bags_.size() << "\n";
    }

    bool mixer_live() const { return audio_ && audio_->enabled && audio_->mixer; }
    void set_side(int side_ix) { side_ = std::clamp(side_ix, 0, 2); }
    void set_frame(long f) { frame_ = f; }

    // ---- sound events (soundmd.ini) ---------------------------------------
    // Resolves + registers `key` on the live mixer on first use (the bank is
    // indexed per render, so adding clips after Mixer construction is safe).
    void ensure_sound(const std::string& key) {
        if (key.empty() || registered_.count(key) || !sounds_ || !audio_ || !audio_->mixer) return;
        registered_.insert(key);
        const ra2assets::SoundEntry e = sounds_->entry(key);
        ra2engine::MixerSound ms;
        for (const std::string& v : e.sounds) {
            bool found = false;
            for (const Bag& b : bags_) {
                const ra2assets::BagClip* c = b.idx.find(v);
                if (!c || b.bag.empty()) continue;
                ra2assets::AudioClipInfo info;
                std::vector<int16_t> pcm =
                    ra2assets::decode_bag_clip(b.bag.data(), b.bag.size(), *c, info);
                ms.variants.push_back(
                    audio_->bank.add(pres_to_sink(std::move(pcm), info.sample_rate, info.channels)));
                found = true;
                break;
            }
            if (!found) std::cerr << "[pres-audio] missing clip " << v << " (key " << key << ")\n";
        }
        ms.volume = e.volume;
        ms.priority = ra2engine::priority_from_string(e.priority);
        ms.limit = e.limit;
        ms.interrupt = has_token(e.control, "interrupt");
        ms.global = has_token(e.type, "global");
        ms.range = e.range;
        ms.min_volume = e.min_volume;
        audio_->mixer->register_sound(key, ms);
    }
    // Plays a positional sound event; logged even without a device.
    void play(const std::string& key, double dist_cells, const char* why) {
        if (key.empty()) return;
        pres_log(frame_, std::string("sound ") + key + " (" + why + ")");
        if (!mixer_live()) return;
        ensure_sound(key);
        audio_->mixer->play(key, dist_cells);
    }

    // W2-CA-12: a classic RA2 campaign speaks lines evamd.ini lacks (the
    // Mis_* mission dialogue lives in RA2 local.mix/eva.ini only). Entries
    // already known keep their evamd definition.
    void add_eva_fallback(const Ini& eva) {
        for (const auto& kv : eva.section("DialogList")) {
            const std::string name = pres_clean(kv.second);
            if (name.empty() || !eva.has(name) || eva_.count(name)) continue;
            EvaDef d;
            d.clip[0] = pres_clean(eva.get(name, "Allied"));
            d.clip[1] = pres_clean(eva.get(name, "Russian"));
            d.clip[2] = pres_clean(eva.get(name, "Yuri"));
            const std::string p = pres_upper(pres_clean(eva.get(name, "Priority", "NORMAL")));
            d.priority = p == "LOW" ? 0 : p == "IMPORTANT" ? 2 : p == "CRITICAL" ? 3 : 1;
            const std::string t = pres_upper(pres_clean(eva.get(name, "Type", "STANDARD")));
            d.type = t == "QUEUE" ? 1 : t == "INTERRUPT" ? 2 : t == "QUEUED_INTERRUPT" ? 3 : 0;
            eva_[name] = d;
        }
    }
    bool has_eva(const std::string& name) const { return eva_.count(name) != 0; }

    // ---- EVA [RE: audio-playback.md §8 FUN_007512a0/007513b0/00751580] -----
    void eva(const std::string& name) {
        auto it = eva_.find(name);
        if (it == eva_.end()) {
            pres_log(frame_, "eva-unknown " + name);
            return;
        }
        const EvaDef& d = it->second;
        Req r{name, d.priority, d.type};
        if (d.type == 2) {   // INTERRUPT: stop current, clear queue, play now
            queue_.clear();
            has_pending_ = false;
            busy_until_ = 0.0;
            if (mixer_live()) audio_->mixer->stop_eva();
            pres_log(frame_, "eva-request " + name + " interrupt");
            start(r);
            return;
        }
        if (d.type == 1 || d.type == 3 || d.priority >= 3) {
            // Real FIFO; de-dupe against the tail (same line already waiting).
            if (!queue_.empty() && queue_.back().name == name) {
                pres_log(frame_, "eva-request " + name + " dedupe");
                return;
            }
            queue_.push_back(r);
            pres_log(frame_, "eva-request " + name + " queued");
        } else if (queue_.empty() && (!has_pending_ || r.priority > pending_.priority)) {
            // Transient pending slot: only the latest highest-priority line waits.
            pending_ = r;
            has_pending_ = true;
            pres_log(frame_, "eva-request " + name + " pending");
        } else {
            pres_log(frame_, "eva-request " + name + " dropped");
            return;
        }
        pump(clock_);
    }
    // Starts the next waiting line when EVA is idle. One at a time.
    void pump(double now) {
        clock_ = now;
        if (now < busy_until_) return;
        if (!queue_.empty()) {
            Req r = queue_.front();
            queue_.pop_front();
            start(r);
        } else if (has_pending_) {
            has_pending_ = false;
            start(pending_);
        }
    }

    // ---- music [G-82] --------------------------------------------------------
    // [RE-FALLBACK: RE-P2 open -- playlist order] The in-game score plays the
    // Normal=yes [Themes] tracks in list order, looping the list; Repeat=yes
    // tracks loop themselves. Replace with the recovered ThemeClass rule.
    void music_start() {
        playlist_.clear();
        for (int i = 0; i < static_cast<int>(themes_.size()); ++i)
            if (themes_[i].normal) playlist_.push_back(i);
        track_ix_ = -1;
        music_on_ = !playlist_.empty();
    }
    void music_update(double now) {
        if (!music_on_) return;
        if (jukebox_pending_ >= 0) {   // W2-UI-14 jukebox Play
            const int ti = jukebox_pending_;
            jukebox_pending_ = -1;
            for (int k = 0; k < static_cast<int>(playlist_.size()); ++k)
                if (playlist_[k] == ti) track_ix_ = k;
            play_track(ti, now);
            return;
        }
        if (track_ix_ >= 0 && now < track_end_) return;
        if (track_ix_ >= 0 && themes_[playlist_[track_ix_]].repeat) {
            play_track(playlist_[track_ix_], now);
            return;
        }
        track_ix_ = (track_ix_ + 1) % static_cast<int>(playlist_.size());
        play_track(playlist_[track_ix_], now);
    }
    void music_stop() {
        music_on_ = false;
        if (mixer_live() && !cur_music_key_.empty()) audio_->mixer->stop(cur_music_key_);
    }
    const std::string& current_track() const { return cur_track_; }
    // W2-UI-14 (0xB8 jukebox): the score list (CSF Name labels, [Themes]
    // order) and Play = start that theme now, then continue the playlist.
    std::vector<std::string> theme_labels() const {
        std::vector<std::string> v;
        for (const Theme& t : themes_) v.push_back(t.name.empty() ? t.section : t.name);
        return v;
    }
    void jukebox_play(const std::string& section) {
        int ti = -1;
        for (int i = 0; i < static_cast<int>(themes_.size()); ++i)
            if (pres_lower(themes_[i].section) == pres_lower(section)) ti = i;
        if (ti < 0) return;
        if (playlist_.empty()) music_start();
        music_on_ = true;
        jukebox_pending_ = ti;
    }

private:
    struct Req { std::string name; int priority; int type; };
    struct Bag { ra2assets::AudioIdx idx; std::vector<uint8_t> bag; };
    struct Theme { std::string section, sound, name; bool normal = true, repeat = false; };

    // Decodes the side's clip for `r` (cached), logs, plays, and holds EVA busy
    // for the clip's real length.
    void start(const Req& r) {
        const EvaDef& d = eva_.at(r.name);
        std::string clip = d.clip[side_];
        if (clip.empty()) clip = d.clip[0];
        double secs = 1.5;
        int id = -1;
        auto c = eva_clips_.find(clip);
        if (c == eva_clips_.end()) {
            std::vector<int16_t> pcm;
            for (const auto& m : wav_mixes_) {
                const MixEntry* e = m->find(pres_lower(clip) + ".wav");
                if (!e) continue;
                try {
                    auto bytes = m->read(*e);
                    ra2assets::WavInfo info;
                    pcm = pres_to_sink(ra2assets::load_wav_ima(bytes.data(), bytes.size(), info),
                                       info.sample_rate, info.channels);
                } catch (const std::exception&) {
                    // YR's audiomd.mix carries zero-filled placeholder entries
                    // for the RA2 EVA lines: fall through to language.mix.
                    pcm.clear();
                }
                if (!pcm.empty()) break;
            }
            if (pcm.empty()) std::cerr << "[pres-audio] EVA clip " << clip << " unavailable\n";
            ClipRec rec;
            rec.secs = pcm.empty() ? 0.0 : pcm.size() / 22050.0;
            if (!pcm.empty() && audio_ && audio_->mixer) rec.id = audio_->bank.add(std::move(pcm));
            c = eva_clips_.emplace(clip, rec).first;
        }
        if (c->second.secs > 0) secs = c->second.secs;
        id = c->second.id;
        // [RE: presentation.md §1.1 DAT_00b1c438] the next line also waits until
        // 500 ms (wall clock) after this one started; INTERRUPT resets it.
        busy_until_ = clock_ + std::max(secs, 0.5);
        char b[64];
        std::snprintf(b, sizeof b, " clip=%s len=%.2fs", clip.c_str(), secs);
        pres_log(frame_, "eva-play " + r.name + b);
        if (mixer_live() && id >= 0) audio_->mixer->play_eva(id);
    }

    void play_track(int ti, double now) {
        const Theme& t = themes_[ti];
        cur_track_ = t.section;
        double secs = 180.0;
        const std::string key = "music:" + t.sound;
        auto it = music_len_.find(key);
        if (it == music_len_.end() && theme_mix_ && mixer_live()) {
            if (const MixEntry* e = theme_mix_->find(pres_lower(t.sound) + ".wav")) {
                try {
                    auto bytes = theme_mix_->read(*e);
                    ra2assets::WavInfo info;
                    std::vector<int16_t> pcm = pres_to_sink(
                        ra2assets::load_wav_ima(bytes.data(), bytes.size(), info),
                        info.sample_rate, info.channels);
                    const double len = pcm.size() / 22050.0;
                    ra2engine::MixerSound ms;
                    ms.variants.push_back(audio_->bank.add(std::move(pcm)));
                    ms.volume = 100;
                    ms.global = true;
                    ms.min_volume = 100;
                    ms.range = 1000.0;
                    ms.limit = 1;
                    ms.bus = ra2engine::AudioBus::Score;   // ScoreVolume (options.md §5)
                    audio_->mixer->register_sound(key, ms);
                    it = music_len_.emplace(key, len).first;
                } catch (const std::exception& ex) {
                    std::cerr << "[pres-audio] theme " << t.sound << " failed: " << ex.what() << "\n";
                }
            }
        }
        if (it != music_len_.end()) secs = it->second;
        if (mixer_live()) {
            if (!cur_music_key_.empty()) audio_->mixer->stop(cur_music_key_);
            audio_->mixer->play(key, 0.0);
        }
        cur_music_key_ = key;
        track_end_ = now + secs;
        pres_log(frame_, "music " + t.section + " sound=" + t.sound);
    }

    AudioSystem* audio_ = nullptr;
    std::unique_ptr<Ini> snd_ini_;
    std::unique_ptr<ra2assets::Sounds> sounds_;
    std::vector<Bag> bags_;
    std::vector<std::unique_ptr<MixFile>> wav_mixes_;
    std::unique_ptr<MixFile> theme_mix_;
    std::set<std::string> registered_;
    std::map<std::string, EvaDef> eva_;
    struct ClipRec { int id = -1; double secs = 0.0; };
    std::map<std::string, ClipRec> eva_clips_;
    std::deque<Req> queue_;
    Req pending_;
    bool has_pending_ = false;
    double busy_until_ = 0.0, clock_ = 0.0;
    int side_ = 0;
    long frame_ = 0;
    std::vector<Theme> themes_;
    std::vector<int> playlist_;
    int track_ix_ = -1;
    int jukebox_pending_ = -1;   // W2-UI-14
    bool music_on_ = false;
    double track_end_ = 0.0;
    std::string cur_track_, cur_music_key_;
    std::map<std::string, double> music_len_;
};
