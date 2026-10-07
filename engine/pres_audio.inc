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

// RA2.INI [Audio] IsScoreShuffle / IsScoreRepeat (defined next to
// g_user_settings in main.cpp).
bool pres_score_shuffle();
bool pres_score_repeat();

class PresAudio {
public:
    ~PresAudio() {
        if (audio_ && audio_->mixer) audio_->mixer->set_music(nullptr);
    }
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
        // W2-GA-21 [RE: RE-Q12, ThemeClass FUN_0071f830/FUN_0071f720]: the
        // classic RA2 target reads THEME.INI (ra2.mix/local.mix) and plays
        // from THEME.MIX; the YR thememd pair is only the fallback (on this
        // install every thememd.mix track is zero-filled). Per theme: Sound=
        // ('$'/'#' stripped), Normal (def yes), Repeat (def no), Side (def
        // all), Scenario (def 0).
        auto read_themes = [&](const Ini& th) {
            for (const auto& kv : th.section("Themes")) {
                const std::string sec = pres_clean(kv.second);
                if (sec.empty() || !th.has(sec)) continue;
                Theme t;
                t.section = sec;
                t.sound = pres_clean(th.get(sec, "Sound"));
                while (!t.sound.empty() && (t.sound[0] == '$' || t.sound[0] == '#')) t.sound.erase(0, 1);
                const std::string nm = pres_lower(pres_clean(th.get(sec, "Normal", "yes")));
                t.normal = nm.empty() || nm[0] != 'n';
                const std::string rp = pres_lower(pres_clean(th.get(sec, "Repeat", "no")));
                t.repeat = !rp.empty() && rp[0] == 'y';
                t.side = pres_lower(pres_clean(th.get(sec, "Side", "")));
                t.scenario = std::atoi(pres_clean(th.get(sec, "Scenario", "0")).c_str());
                t.name = pres_clean(th.get(sec, "Name"));   // W2-UI-14 jukebox label (CSF)
                if (!t.sound.empty()) themes_.push_back(t);
            }
        };
        try {
            MixFile local = MixFile::open_path(dir + "ra2.mix").open_nested("local.mix");
            if (const MixEntry* e = local.find(std::string("theme.ini"))) {
                const std::vector<uint8_t> b = local.read(*e);
                read_themes(Ini::parse(std::string(b.begin(), b.end())));
                theme_mix_name_ = "theme.mix";
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pres-audio] theme.ini unavailable: " << ex.what() << "\n";
        }
        if (themes_.empty()) {
            try {
                read_themes(load_ini_from_ra2md(dir, "thememd.ini"));
                theme_mix_name_ = "thememd.mix";
            } catch (const std::exception& ex) {
                std::cerr << "[pres-audio] thememd.ini unavailable: " << ex.what() << "\n";
            }
        }
        if (!want_clips) return;
        // Clip containers: YR first, RA2 second.
        const std::pair<const char*, const char*> chain[] = {{"langmd.mix", "audiomd.mix"},
                                                             {"language.mix", "audio.mix"}};
        for (const auto& [top, nested] : chain) {
            try {
                MixFile t = MixFile::open_path(dir + top);
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
        // Theme archives: the one the ini came from first, then the other (the
        // binary itself opens THEMEMD.MIX and falls back to THEME.MIX when the
        // open fails, FUN_005301a0). File-backed: tracks are read on demand.
        for (const std::string& name : {theme_mix_name_, std::string(theme_mix_name_ == "theme.mix" ? "thememd.mix" : "theme.mix")}) {
            try {
                theme_mixes_.push_back(std::make_unique<MixFile>(MixFile::open_path(dir + name)));
            } catch (const std::exception&) {}
        }
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
        // W2-GA-24 [RE: audio-playback.md §2/§4]: VShift/FShift jitter and
        // the ALL / ATTACK / DECAY / LOOP sample sequencing.
        ms.vshift = e.vshift;
        ms.fshift_lo = e.fshift_lo;
        ms.fshift_hi = e.fshift_hi;
        ms.all = has_token(e.control, "all");
        ms.attack = has_token(e.control, "attack");
        ms.decay = has_token(e.control, "decay");
        ms.loop = has_token(e.control, "loop");
        audio_->mixer->register_sound(key, ms);
    }
    // Plays a non-positional (screen-centred) sound event; logged even
    // without a device.
    void play(const std::string& key, double dist_cells, const char* why) {
        if (key.empty()) return;
        pres_log(frame_, std::string("sound ") + key + " (" + why + ")");
        if (!mixer_live()) return;
        ensure_sound(key);
        audio_->mixer->play(key, dist_cells);
    }

    // W2-GA-23 [RE: audio-playback.md §3, RE-Q11 FUN_0074f8e0]: the tactical
    // view the positional sounds are measured from (set every frame).
    void set_view(int cam_x, int cam_y, int view_w, int view_h) {
        cam_x_ = cam_x; cam_y_ = cam_y; view_w_ = view_w; view_h_ = view_h;
    }
    // Chebyshev max(dx, 2|dy|) px from the view centre / 60 -> cells, and the
    // pan: relX = screen x - halfW clamped to +-W, pan = relX*8192/W + 8192.
    void locate(double fx, double fy, double& dist_cells, int& pan) const {
        const double sx = (fx - fy) * 30.0 + cam_x_ + 30.0;
        const double sy = (fx + fy) * 15.0 + cam_y_ + 15.0;
        const double hw = view_w_ / 2.0, hh = view_h_ / 2.0;
        dist_cells = std::max(std::abs(sx - hw), 2.0 * std::abs(sy - hh)) / 60.0;
        const double w = std::max(1.0, 2.0 * hw);
        const double rel = std::clamp(sx - hw, -w, w);
        pan = static_cast<int>(rel * 8192.0 / w + 8192.0);
    }
    // A positional event at world (fx, fy): panned, and re-attenuated every
    // frame while it plays (stopped once it leaves range), as the original
    // does for live voices (FUN_0074fb60).
    void play_at(const std::string& key, double fx, double fy, const char* why) {
        if (key.empty()) return;
        pres_log(frame_, std::string("sound ") + key + " (" + why + ")");
        if (!mixer_live()) return;
        ensure_sound(key);
        double d = 0.0;
        int pan = 8192;
        locate(fx, fy, d, pan);
        if (const std::uint32_t h = audio_->mixer->play_at(key, d, pan)) live_.push_back({h, fx, fy});
    }
    // Per frame: follow the camera for every live positional voice.
    void update_voices() {
        if (!mixer_live()) { live_.clear(); return; }
        std::size_t k = 0;
        for (const LiveVoice& v : live_) {
            double d = 0.0;
            int pan = 8192;
            locate(v.fx, v.fy, d, pan);
            if (audio_->mixer->update_voice(v.handle, d, pan)) live_[k++] = v;
        }
        live_.resize(k);
    }

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

    // ---- music [G-82, W2-GA-21/22] ------------------------------------------
    // [RE: RE-Q12] ThemeClass::Next_Song (FUN_0071fd20): with the current
    // theme not Repeat and IsScoreRepeat off (or nothing playing yet), shuffle
    // picks Random(0,Count-1) until an allowed theme other than the current
    // (999 tries, then 0); otherwise the next allowed index after the current,
    // wrapping (none -> 0). Else the current theme repeats. Is_Allowed
    // (FUN_007203e0): available && Normal && (Side any || the player's side)
    // && (not campaign || scenario >= Scenario=). IsScoreRepeat/IsScoreShuffle
    // come from RA2.INI [Audio] (FUN_005fa500). Tracks are streamed.
    void music_start() {
        track_ix_ = -1;
        music_on_ = false;
        for (int i = 0; i < static_cast<int>(themes_.size()); ++i)
            if (allowed(i)) { music_on_ = true; break; }
    }
    void music_update(double now) {
        if (!music_on_) return;
        if (jukebox_pending_ >= 0) {   // W2-UI-14 jukebox Play: that theme now, then Next_Song
            const int ti = jukebox_pending_;
            jukebox_pending_ = -1;
            play_track(ti, now);
            return;
        }
        if (track_ix_ >= 0) {
            // Stream still playing (or, deviceless, the header length not yet
            // elapsed): nothing to do.
            if (stream_ && !stream_->finished()) return;
            if (!stream_ && now < track_end_) return;
        }
        const int next = next_song(track_ix_);
        if (next < 0 || next >= static_cast<int>(themes_.size()) || !allowed(next)) {
            music_on_ = false;
            return;
        }
        play_track(next, now);
    }
    void music_stop() {
        music_on_ = false;
        if (mixer_live()) audio_->mixer->set_music(nullptr);
        stream_.reset();
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
        music_on_ = true;
        jukebox_pending_ = ti;
    }

private:
    struct Req { std::string name; int priority; int type; };
    struct Bag { ra2assets::AudioIdx idx; std::vector<uint8_t> bag; };
    struct Theme {
        std::string section, sound, side, name;
        bool normal = true, repeat = false;
        int scenario = 0;
        int avail = -1;   // -1 unknown, 0 missing/zero-filled, 1 playable
    };
    struct LiveVoice { std::uint32_t handle; double fx, fy; };

    // IsAvailable (FUN_0071fa90): the <Sound>.wav entry exists in a theme
    // archive and is a real RIFF WAVE (the thememd.mix placeholders are
    // zero-filled). Checked lazily from the first 12 bytes.
    const MixFile* theme_source(int ti) {
        Theme& t = themes_[static_cast<std::size_t>(ti)];
        const std::string file = pres_lower(t.sound) + ".wav";
        for (const auto& m : theme_mixes_) {
            const MixEntry* e = m->find(file);
            if (!e || e->size < 12) continue;
            if (t.avail < 0) {
                MixEntry head = *e;
                head.size = 12;
                const std::vector<uint8_t> h = m->read(head);
                if (std::memcmp(h.data(), "RIFF", 4) != 0 || std::memcmp(h.data() + 8, "WAVE", 4) != 0) continue;
            }
            t.avail = 1;
            return m.get();
        }
        t.avail = 0;
        return nullptr;
    }
    bool allowed(int ti) {
        if (ti < 0 || ti >= static_cast<int>(themes_.size())) return false;
        const Theme& t = themes_[static_cast<std::size_t>(ti)];
        if (!t.normal) return false;
        if (!t.side.empty() && t.side != "-1") {
            const char* mine = side_ == 1 ? "soviet" : side_ == 2 ? "thirdside" : "allied";
            const bool gdi = t.side == "gdi" || t.side == "allied", nod = t.side == "nod" || t.side == "soviet";
            if ((gdi && side_ != 0) || (nod && side_ != 1) || (!gdi && !nod && t.side != mine)) return false;
        }
        if (t.avail == 0) return false;
        return theme_source(ti) != nullptr;
    }
    int next_song(int cur) {
        const int count = static_cast<int>(themes_.size());
        if (count <= 0) return -1;
        const bool repeat_cur = cur >= 0 && (themes_[static_cast<std::size_t>(cur)].repeat || pres_score_repeat());
        if (repeat_cur) return cur;
        if (pres_score_shuffle()) {
            int n = 0, r = 0;
            for (;;) {
                theme_rng_ = theme_rng_ * 1103515245u + 12345u;
                r = static_cast<int>((theme_rng_ >> 8) % static_cast<std::uint32_t>(count));
                if (++n > 999) break;
                if (r != cur && allowed(r)) break;
            }
            return n >= 1000 ? 0 : r;
        }
        for (int k = count + 1;;) {
            if (++cur >= count) cur = 0;
            if (--k == 0) return 0;
            if (allowed(cur)) return cur;
        }
    }

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

    // W2-GA-22: the track is streamed -- a worker thread reads the WAV off
    // the (file-backed) MIX and decodes it block by block just ahead of the
    // audio thread; the previous track's stream is released, nothing is kept
    // in the SoundBank. Deviceless runs only parse the header for the length.
    void play_track(int ti, double now) {
        const auto t0 = std::chrono::steady_clock::now();
        const Theme& t = themes_[static_cast<std::size_t>(ti)];
        track_ix_ = ti;
        cur_track_ = t.section;
        double secs = 180.0;
        const MixFile* src = theme_source(ti);
        const MixEntry* e = src ? src->find(pres_lower(t.sound) + ".wav") : nullptr;
        if (mixer_live()) audio_->mixer->set_music(nullptr);
        stream_.reset();
        if (src && e) {
            const MixFile mix = *src;   // shares the file handle
            const MixEntry ent = *e;
            if (mixer_live()) {
                stream_ = std::make_shared<ra2engine::MusicStream>([mix, ent]() { return mix.read(ent); });
                audio_->mixer->set_music(stream_);
            } else {
                // Header only: the length the deviceless clock waits for.
                try {
                    const std::vector<uint8_t> b = mix.read(ent);
                    const ra2assets::WavImaStream w = ra2assets::WavImaStream::open(b);
                    if (w.info().sample_rate > 0)
                        secs = static_cast<double>(w.total_frames()) / w.info().sample_rate;
                } catch (const std::exception&) {}
            }
        }
        track_end_ = now + secs;
        // Evidence for W2-GA-22: the game thread only starts the stream.
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        char tb[48];
        std::snprintf(tb, sizeof tb, " setup_ms=%.2f", ms);
        pres_log(frame_, "music " + t.section + " sound=" + t.sound + " src=" +
                             (src ? theme_mix_name_of(src) : std::string("none")) + tb);
    }
    std::string theme_mix_name_of(const MixFile* m) const {
        for (std::size_t i = 0; i < theme_mixes_.size(); ++i)
            if (theme_mixes_[i].get() == m)
                return i == 0 ? theme_mix_name_ : (theme_mix_name_ == "theme.mix" ? "thememd.mix" : "theme.mix");
        return "?";
    }

    AudioSystem* audio_ = nullptr;
    std::unique_ptr<Ini> snd_ini_;
    std::unique_ptr<ra2assets::Sounds> sounds_;
    std::vector<Bag> bags_;
    std::vector<std::unique_ptr<MixFile>> wav_mixes_;
    std::vector<std::unique_ptr<MixFile>> theme_mixes_;
    std::string theme_mix_name_ = "theme.mix";
    std::shared_ptr<ra2engine::MusicStream> stream_;
    std::uint32_t theme_rng_ = 0x7E3A11u;
    std::vector<LiveVoice> live_;
    int cam_x_ = 0, cam_y_ = 0, view_w_ = 1, view_h_ = 1;
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
    int track_ix_ = -1;
    int jukebox_pending_ = -1;   // W2-UI-14
    bool music_on_ = false;
    double track_end_ = 0.0;
    std::string cur_track_;
};
