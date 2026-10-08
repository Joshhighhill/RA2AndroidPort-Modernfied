// movie_player.inc -- W5-E (R-034 / R-200 / U-5): campaign movies through the
// existing Bink decoder (src/bink.cpp). Included by main.cpp before
// campaign_scenes.inc (CampaignLoadScene / CampaignResultScene) and GameScene.
//
// [RE: campaign.md §8] the map's [Basic] Intro= / Brief= / Win= / Lose= name
// the movies (A01_f00e, ...), found in MOVIES01.MIX / MOVIES02.MIX /
// MOVMD03.MIX. On this install those archives are 4-byte stubs, so the player
// looks, in order, for:
//   0. a loose <name>.bik in $RA2_MOVIE_DIR (a user movie folder),
//   1. a loose <name>.bik in the data dir or in <data dir>/Movies/,
//   2. a loose <name>.bik in the writable pref dir's Movies/ folder,
//   3. <name>.bik inside any user-supplied MIX in the data dir whose file name
//      starts with "MOVIES" / "MOVMD" (the stock names included, when real).
// A missing movie (or an unreadable / non-Bink file) is skipped silently and
// logged ("[pres] movie NAME unavailable"); the Bink audio track is not
// decoded (R-134), so movies play silent.

std::string pref_dir_path();

struct MovieFile {
    std::unique_ptr<ra2assets::BinkFile> file;
    std::unique_ptr<ra2assets::BinkVideoDecoder> dec;
    std::string name, source;
};

inline std::string movie_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Locate + parse `name` (without extension); false when unavailable.
inline bool find_movie(const std::string& name_in, MovieFile& out) {
    if (name_in.empty()) return false;
    std::string name = movie_lower(name_in);
    if (name.size() > 4 && name.substr(name.size() - 4) == ".bik") name.resize(name.size() - 4);
    const std::string file = name + ".bik";
    auto accept = [&](std::vector<uint8_t> bytes, const std::string& src) {
        if (bytes.size() < 44 || bytes[0] != 'B' || bytes[1] != 'I' || bytes[2] != 'K') return false;
        try {
            out.file = std::make_unique<ra2assets::BinkFile>(ra2assets::BinkFile::parse(std::move(bytes)));
            if (out.file->num_frames <= 0 || out.file->width <= 0) return false;
            out.dec = std::make_unique<ra2assets::BinkVideoDecoder>(*out.file);
            out.name = name;
            out.source = src;
            return true;
        } catch (const std::exception&) {
            out.file.reset();
            out.dec.reset();
            return false;
        }
    };
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string dir = env_install_dir();
    std::vector<std::string> dirs;
    if (const char* md = std::getenv("RA2_MOVIE_DIR")) dirs.push_back(std::string(md) + "/");   // user movie folder
    dirs.insert(dirs.end(), {dir, dir + "Movies/", dir + "movies/"});
    const std::string pref = pref_dir_path();
    if (!pref.empty()) dirs.push_back(pref + "Movies/");
    for (const std::string& d : dirs) {
        if (!fs::is_directory(d, ec)) continue;
        for (const auto& e : fs::directory_iterator(d, ec)) {
            if (!e.is_regular_file(ec) || movie_lower(e.path().filename().string()) != file) continue;
            try {
                if (accept(ra2assets::read_file(e.path().string()), e.path().string())) return true;
            } catch (const std::exception&) {}
        }
    }
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        const std::string fn = movie_lower(e.path().filename().string());
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".mix") continue;
        if (fn.rfind("movies", 0) != 0 && fn.rfind("movmd", 0) != 0) continue;
        if (e.file_size(ec) < 64) continue;   // the 4-byte install stubs
        try {
            ra2assets::MixFile m = ra2assets::MixFile::open_path(e.path().string());
            if (const ra2assets::MixEntry* me = m.find(file))
                if (accept(m.read(*me), e.path().string() + ":" + file)) return true;
        } catch (const std::exception&) {}
    }
    return false;
}

// A full-screen movie, letterboxed into the window, at the file's frame rate;
// any key / click skips it.
class MoviePlayer {
public:
    bool start(const std::string& name) {
        mv_ = MovieFile{};
        frame_ = -1;
        acc_ = 0.0;
        tex_frame_ = -1;
        if (!find_movie(name, mv_)) {
            std::cerr << "[pres] movie " << name << " unavailable (skipped)\n";
            pres_log(0, "movie " + name + " unavailable");
            return false;
        }
        const double num = mv_.file->fps_num > 0 ? mv_.file->fps_num : 15.0;
        const double den = mv_.file->fps_den > 0 ? mv_.file->fps_den : 1.0;
        dt_ = den / num;
        mv_.dec->decode_frame(0);
        frame_ = 0;
        pres_log(0, "movie " + name + " playing from " + mv_.source);
        return true;
    }
    bool active() const { return mv_.dec && frame_ >= 0 && frame_ < mv_.file->num_frames; }
    void skip() { frame_ = mv_.file ? mv_.file->num_frames : -1; }
    void update(double dt) {
        if (!active()) return;
        acc_ += dt;
        while (acc_ >= dt_ && active()) {
            acc_ -= dt_;
            if (frame_ + 1 >= mv_.file->num_frames) { frame_ = mv_.file->num_frames; break; }
            mv_.dec->decode_frame(++frame_);
        }
    }
    void render(SDL_Renderer* ren) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (!active()) return;
        const int w = mv_.file->width, h = mv_.file->height;
        if (!tex_) tex_ = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!tex_) return;
        if (tex_frame_ != frame_) {
            const std::vector<uint8_t> px = mv_.dec->rgba();
            if (static_cast<int>(px.size()) == w * h * 4) SDL_UpdateTexture(tex_, nullptr, px.data(), w * 4);
            tex_frame_ = frame_;
        }
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        SDL_RenderGetLogicalSize(ren, &ow, &oh);
        if (ow <= 0 || oh <= 0) { ow = 1280; oh = 720; }
        const double sc = std::min(static_cast<double>(ow) / w, static_cast<double>(oh) / h);
        const SDL_Rect d{static_cast<int>((ow - w * sc) / 2), static_cast<int>((oh - h * sc) / 2),
                         static_cast<int>(w * sc), static_cast<int>(h * sc)};
        SDL_RenderCopy(ren, tex_, nullptr, &d);
    }
    ~MoviePlayer() { if (tex_) SDL_DestroyTexture(tex_); }
    MoviePlayer() = default;
    MoviePlayer(const MoviePlayer&) = delete;
    MoviePlayer& operator=(const MoviePlayer&) = delete;

private:
    MovieFile mv_;
    int frame_ = -1, tex_frame_ = -1;
    double acc_ = 0.0, dt_ = 1.0 / 15.0;
    SDL_Texture* tex_ = nullptr;
};

// A scene that plays a list of movies (missing ones skipped) and then hands
// over to `then()`. With nothing playable it hands over on the first frame.
class MovieScene : public Scene {
public:
    MovieScene(std::vector<std::string> names, std::function<Scene*()> then)
        : names_(std::move(names)), then_(std::move(then)) {
        advance();
    }
    void handle(const ra2engine::InputEvent& ev) override {
        if (ev.kind == ra2engine::InputEvent::Key ||
            (ev.kind == ra2engine::InputEvent::PointerDown && ev.button == ra2engine::InputEvent::Left))
            player_.skip();   // [the original: Esc / a click skips the movie]
    }
    void update(double dt) override {
        player_.update(dt);
        if (!player_.active()) advance();
    }
    void render(ra2engine::Platform& plat, ra2engine::TextureCache&) override {
        player_.render(static_cast<SDL_Renderer*>(plat.renderer()));
    }
    Scene* next() override {
        if (!done_) return nullptr;
        done_ = false;
        return then_ ? then_() : nullptr;
    }
    void play_probe(ra2play::Probe& p) override {
        p.scene = "movie";
        p.set("movies_played", played_);
        p.targets["skip"] = ra2play::Rect{560, 300, 160, 120};
    }

private:
    void advance() {
        while (ix_ < names_.size()) {
            if (player_.start(names_[ix_++])) { ++played_; return; }
        }
        done_ = true;
    }
    std::vector<std::string> names_;
    std::function<Scene*()> then_;
    MoviePlayer player_;
    std::size_t ix_ = 0;
    int played_ = 0;
    bool done_ = false;
};

// True when at least one of `names` can be played (so the flow only detours
// through a MovieScene when there is something to show).
inline bool any_movie(const std::vector<std::string>& names) {
    for (const std::string& n : names) {
        MovieFile m;
        if (find_movie(n, m)) return true;
        if (!n.empty()) pres_log(0, "movie " + n + " unavailable (skipped)");
    }
    return false;
}
