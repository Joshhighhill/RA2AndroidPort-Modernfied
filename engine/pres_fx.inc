// pres_fx.inc -- G-83 render-side AnimList: weapon muzzle Anim=, warhead
// AnimList impacts, death Explosion= anims (with each anim's Report= sound),
// projectile Image= art in flight. Included by main.cpp BEFORE render_world
// (draw_projectiles / render_world call the hooks below). Purely cosmetic: it
// observes World state and owns its own clock + RNG, so no simulation value or
// gate hash moves.
//
// Data: rulesmd [weapon] Anim= / Projectile= / Warhead=, [warhead] AnimList=,
// [projectile] Image= / Rotates=, [unit/building] Explosion=; artmd [anim]
// Image= / Translucent= / Report= / Next= / Shadow= / Rate=. SHPs from
// conquer.mix (RA2) then conqmd.mix (YR), palette anim.pal.

struct FxAnimType {
    std::string name;
    std::shared_ptr<ra2assets::ShpFile> shp;
    int frames = 0;            // image frames (half the SHP when Shadow=yes)
    bool translucent = false;  // Translucent=yes -> 50 % blend
    std::string report;        // Report= sound event
    std::string next;          // Next= chained anim
    // [RE: presentation.md §5] Rate= stores 900/Rate game frames per anim
    // frame (no Rate -> 1); Start/End (End defaults to the image frames),
    // LoopStart/LoopEnd (LoopEnd defaults to End), LoopCount (0/1 = once,
    // -1 = 255): every pass but the last loops LoopStart..LoopEnd, the last
    // runs to End, then Next= chains.
    int delay = 1;
    int start = 0, end = 0, loop_start = 0, loop_end = 0, loops = 1;
    // Anim frame for game-frame tick 	, or -1 once finished.
    int frame_at(long t) const {
        if (delay <= 0) return start;
        long step = t / delay;
        const long first = loop_end - start;          // first pass to LoopEnd
        const long mid = loop_end - loop_start;       // each middle pass
        if (loops <= 1) return step < end - start ? static_cast<int>(start + step) : -1;
        if (step < first) return static_cast<int>(start + step);
        step -= first;
        for (int k = 1; k < loops - 1; ++k) {
            if (step < mid) return static_cast<int>(loop_start + step);
            step -= mid;
        }
        const long last = end - loop_start;
        return step < last ? static_cast<int>(loop_start + step) : -1;
    }
};

struct FxWeapon {
    std::vector<std::string> muzzle;     // Anim= (1 or 8 facing-ordered N..NW)
    std::vector<std::string> anim_list;  // warhead AnimList=
    std::string proj_image;              // projectile Image= ("" / none -> nothing in flight)
    bool proj_rotates = false;
    bool instant = true;                 // no flying projectile -> impact at the target on fire
};

class PresFx {
public:
    struct Anim {
        const FxAnimType* t = nullptr;
        double sx = 0, sy = 0;   // cell-space position (fx, fy)
        int level = 0;
        double lift_px = 0;      // screen-space height above the ground point
        double age = 0;
        double ox_px = 0;        // extra screen-x offset (FLH muzzle point)
    };

    void init(const std::string& dir, const ra2assets::MixFile& ra2, const Ini& rules_ini,
              const Ini& art_ini) {
        ra2_ = &ra2;
        rules_ = &rules_ini;
        art_ = &art_ini;
        try { conquer_ = std::make_unique<MixFile>(ra2.open_nested("conquer.mix")); } catch (...) {}
        try {
            MixFile md = MixFile::open_path(dir + "ra2md.mix");
            conqmd_ = std::make_unique<MixFile>(md.open_nested("conqmd.mix"));
        } catch (...) {}
        try {
            MixFile cache = ra2.open_nested("cache.mix");
            if (const MixEntry* e = cache.find(std::string("anim.pal"))) {
                auto raw = cache.read(*e);
                pal_ = ra2assets::Palette::load(raw.data(), raw.size());
                has_pal_ = true;
            }
        } catch (...) {}
    }

    // Weapon effects for a unit type (by image), resolved once.
    void add_unit(const std::string& id, const std::string& image) {
        if (image.empty() || weapons_.count(image)) return;
        std::string w = rget(id, "Primary");
        if (w.empty()) w = rget(id, "Weapon1");
        FxWeapon fw;
        if (!w.empty()) {
            fw.muzzle = split(rget(w, "Anim"));
            const std::string wh = rget(w, "Warhead");
            if (!wh.empty()) fw.anim_list = split(rget(wh, "AnimList"));
            const std::string pj = rget(w, "Projectile");
            if (!pj.empty()) {
                const std::string img = rget(pj, "Image");
                if (!img.empty() && pres_lower(img) != "none") fw.proj_image = img;
                fw.proj_rotates = pres_lower(rget(pj, "Rotates")).rfind("y", 0) == 0 ||
                                  pres_lower(rget(pj, "Rotates")).rfind("t", 0) == 0;
            }
        }
        weapons_[image] = fw;
        explosions_[image] = split(rget(id, "Explosion"));
    }
    void set_instant(const std::string& image, bool instant) {
        auto it = weapons_.find(image);
        if (it != weapons_.end()) it->second.instant = instant;
    }
    const FxWeapon* weapon(const std::string& image) const {
        auto it = weapons_.find(image);
        return it == weapons_.end() ? nullptr : &it->second;
    }
    const std::vector<std::string>* explosions(const std::string& id_or_image) {
        auto it = explosions_.find(id_or_image);
        if (it == explosions_.end())
            it = explosions_.emplace(id_or_image, split(rget(id_or_image, "Explosion"))).first;
        return &it->second;
    }

    // ---- G-85 building anims ------------------------------------------------
    // [RE: presentation.md §6] ActiveAnim/ActiveAnimTwo/Three/Four (+ the
    // *Damaged variant below ConditionYellow), drawn at the building's
    // anchor + the slot's X/Y pixel offset, timed by the anim's own [Anim]
    // Rate/Loop keys; BibShape under the building; DamageFireTypes at
    // DamageFireOffset0..7 while damaged. Art resolves like the building's
    // (NewTheater candidates over the theater mixes).
    struct BldAnims {
        struct Slot { const FxAnimType* t = nullptr; const FxAnimType* dmg = nullptr; int x = 0, y = 0; bool powered = true; };
        std::vector<Slot> slots;
        std::shared_ptr<ra2assets::ShpFile> bib;
        std::vector<std::pair<int, int>> fire_offsets;
    };
    const BldAnims& building(const std::string& image) {
        auto it = bld_.find(image);
        if (it != bld_.end()) return it->second;
        BldAnims a;
        static const char* kSlots[] = {"ActiveAnim", "ActiveAnimTwo", "ActiveAnimThree", "ActiveAnimFour"};
        for (const char* s : kSlots) {
            const std::string n = aget(image, s);
            if (n.empty()) continue;
            BldAnims::Slot sl;
            sl.t = theater_type(n);
            sl.dmg = theater_type(aget(image, std::string(s) + "Damaged"));
            sl.x = std::atoi(aget(image, std::string(s) + "X").c_str());
            sl.y = std::atoi(aget(image, std::string(s) + "Y").c_str());
            const std::string pw = pres_lower(aget(image, std::string(s) + "Powered"));
            sl.powered = pw.empty() || pw[0] == 'y' || pw[0] == 't';
            if (sl.t) a.slots.push_back(sl);
        }
        const std::string bib = aget(image, "BibShape");
        if (!bib.empty()) a.bib = theater_shp(bib);
        for (int i = 0; i < 8; ++i) {
            const std::string v = aget(image, "DamageFireOffset" + std::to_string(i));
            if (v.empty()) continue;
            const std::size_t k = v.find(',');
            if (k == std::string::npos) continue;
            a.fire_offsets.push_back({std::atoi(v.c_str()), std::atoi(v.c_str() + k + 1)});
        }
        std::cerr << "[fx] building " << image << " anims=" << a.slots.size() << " bib=" << (a.bib ? 1 : 0)
                  << " fires=" << a.fire_offsets.size() << "\n";
        return bld_.emplace(image, std::move(a)).first->second;
    }
    std::vector<std::string> damage_fires() { return split(rget("General", "DamageFireTypes")); }
    const FxAnimType* theater_type(const std::string& name) {
        if (name.empty()) return nullptr;
        if (const FxAnimType* t = type(name)) return t;
        // NewTheater building anims live in the theater mixes.
        const std::string key = pres_upper(name);
        auto shp = theater_shp(aget(key, "Image").empty() ? key : aget(key, "Image"));
        if (!shp) return nullptr;
        auto t = std::make_unique<FxAnimType>();
        fill_type(*t, key, shp);
        const FxAnimType* raw = t.get();
        types_[key] = std::move(t);
        return raw;
    }
    std::shared_ptr<ra2assets::ShpFile> theater_shp(const std::string& image) {
        if (!ra2_) return nullptr;
        const ra2assets::TheaterChain& ch = art_chain();
        for (const auto& c : ra2assets::theater_candidates(image, ch.letter, ch.iso_mix, ch.art_mix, ".shp")) {
            MixFile* m = nested(c.mix);
            if (!m) continue;
            if (const MixEntry* e = m->find(c.file)) {
                try { return std::make_shared<ra2assets::ShpFile>(ShpFile::load(m->read(*e))); } catch (...) {}
            }
        }
        return nullptr;
    }
    // Looping frame for building anims (they loop until the building changes).
    static int loop_frame(const FxAnimType& t, long tick) {
        if (t.delay <= 0) return t.start;
        long step = tick / t.delay;
        const long first = t.loop_end - t.start;
        if (step < first) return static_cast<int>(t.start + step);
        const long span = std::max(1, t.loop_end - t.loop_start);
        return static_cast<int>(t.loop_start + (step - first) % span);
    }
    // Loads (once) the anim type `name`; nullptr when its SHP is missing.
    const FxAnimType* type(const std::string& name) {
        if (name.empty()) return nullptr;
        const std::string key = pres_upper(name);
        auto it = types_.find(key);
        if (it != types_.end()) return it->second.get();
        std::unique_ptr<FxAnimType> t;
        std::string image = aget(key, "Image");
        if (image.empty()) image = key;
        if (auto shp = load_shp(image)) {
            t = std::make_unique<FxAnimType>();
            fill_type(*t, key, shp);
        }
        const FxAnimType* raw = t.get();
        types_[key] = std::move(t);
        return raw;
    }
    void fill_type(FxAnimType& t, const std::string& key, std::shared_ptr<ra2assets::ShpFile> shp) {
        t.name = key;
        t.shp = shp;
        const bool shadow = pres_lower(aget(key, "Shadow")).rfind("y", 0) == 0;
        t.frames = shadow ? std::max(1, shp->count() / 2) : shp->count();
        t.translucent = pres_lower(aget(key, "Translucent")).rfind("y", 0) == 0;
        t.report = aget(key, "Report");
        auto num = [&](const char* k, int d) {
            const std::string v = aget(key, k);
            return v.empty() ? d : std::atoi(v.c_str());
        };
        const int rate = num("Rate", 0);
        t.delay = rate > 0 ? (rate > 900 ? 0 : 900 / rate) : 1;
        t.start = std::clamp(num("Start", 0), 0, t.frames - 1);
        t.end = std::clamp(num("End", t.frames), t.start + 1, t.frames);
        t.loop_start = std::clamp(num("LoopStart", t.start), t.start, t.end - 1);
        t.loop_end = std::clamp(num("LoopEnd", t.end), t.loop_start + 1, t.end);
        const int lc = num("LoopCount", 1);
        t.loops = lc < 0 ? 255 : std::max(1, lc);
        t.next = aget(key, "Next");
    }
    std::shared_ptr<ra2assets::ShpFile> projectile_shp(const std::string& image) {
        auto it = proj_shps_.find(image);
        if (it != proj_shps_.end()) return it->second;
        auto s = load_shp(image);
        proj_shps_[image] = s;
        return s;
    }

    // Spawns `name` at a cell-space point; returns the type (for the Report).
    const FxAnimType* spawn(const std::string& name, double fx, double fy, int level, double lift_px,
                            double ox_px = 0.0) {
        const FxAnimType* t = type(name);
        if (!t || t->frames <= 0) return nullptr;
        anims_.push_back(Anim{t, fx, fy, level, lift_px, 0.0, ox_px});
        return t;
    }
    void step(double dt) {
        std::vector<Anim> next;
        for (Anim& a : anims_) {
            a.age += dt;
            if (a.t->frame_at(static_cast<long>(a.age * 15.0)) >= 0) continue;
            if (!a.t->next.empty())
                if (const FxAnimType* n = type(a.t->next)) next.push_back(Anim{n, a.sx, a.sy, a.level, a.lift_px, 0, a.ox_px});
            a.t = nullptr;
        }
        anims_.erase(std::remove_if(anims_.begin(), anims_.end(), [](const Anim& a) { return !a.t; }),
                     anims_.end());
        anims_.insert(anims_.end(), next.begin(), next.end());
    }
    void draw(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::Camera& cam, int W,
              int H) {
        for (const Anim& a : anims_) {
            const int fr = a.t->frame_at(static_cast<long>(a.age * 15.0));
            if (fr < 0) continue;
            const int cx = static_cast<int>(std::lround((a.sx - a.sy) * 30.0 + a.ox_px)) + cam.x + 30;
            const int cy = static_cast<int>(std::lround((a.sx + a.sy) * 15.0 - a.level * 15 - a.lift_px)) +
                           cam.y + 15;
            blit(ren, tc, *a.t->shp, "fx:" + a.t->name, fr, cx, cy, a.t->translucent, W, H);
        }
    }
    // Centered SHP frame blit with anim.pal (translucent -> 50 %).
    void blit(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2assets::ShpFile& shp,
              const std::string& tag, int fr, int cx, int cy, bool translucent, int W, int H,
              const ra2assets::Palette* pal_override = nullptr) {
        if (fr < 0 || fr >= shp.count()) return;
        const ra2assets::ShpFile::Frame f = shp.frame_info(fr);
        if (f.w <= 0 || f.h <= 0) return;
        SDL_Rect dst{cx - shp.width() / 2 + f.x, cy - shp.height() / 2 + f.y, f.w, f.h};
        if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H) return;
        const std::string key = tag + ":" + std::to_string(fr);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            const std::vector<uint8_t> idx = shp.decode_indexed(fr);
            tex = static_cast<SDL_Texture*>(
                tc.get(key, idx.data(), f.w, f.h, pal_override ? *pal_override : pal()));
        }
        SDL_SetTextureAlphaMod(tex, translucent ? 128 : 255);
        SDL_RenderCopy(ren, tex, nullptr, &dst);
        SDL_SetTextureAlphaMod(tex, 255);
    }
    const ra2assets::Palette& pal() const { return pal_; }
    std::size_t live() const { return anims_.size(); }
    // Base-defense shot (World::FireEvent with a weapon id): the weapon's
    // muzzle Anim= list and its first Report= sound, straight from rules.
    std::vector<std::string> weapon_muzzle(const std::string& wid) const { return split(rget(wid, "Anim")); }
    std::string weapon_report(const std::string& wid) const {
        const std::vector<std::string> r = split(rget(wid, "Report"));
        return r.empty() ? std::string() : r.front();   // [RE-FALLBACK: multi-Report pick, as fire_fx]
    }

private:
    std::string rget(const std::string& s, const std::string& k) const {
        return rules_ ? pres_clean(rules_->get(s, k)) : std::string();
    }
    std::string aget(const std::string& s, const std::string& k) const {
        return art_ ? pres_clean(art_->get(s, k)) : std::string();
    }
    static std::vector<std::string> split(const std::string& v) {
        std::vector<std::string> out;
        std::stringstream ss(v);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            tok = pres_clean(tok);
            if (!tok.empty()) out.push_back(tok);
        }
        return out;
    }
    std::shared_ptr<ra2assets::ShpFile> load_shp(const std::string& image) {
        const std::string file = pres_lower(image) + ".shp";
        for (MixFile* m : {conqmd_.get(), conquer_.get()}) {
            if (!m) continue;
            if (const MixEntry* e = m->find(file)) {
                try {
                    return std::make_shared<ra2assets::ShpFile>(ShpFile::load(m->read(*e)));
                } catch (...) {}
            }
        }
        return nullptr;
    }

    MixFile* nested(const std::string& name) {
        auto it = nested_.find(name);
        if (it != nested_.end()) return it->second.get();
        std::unique_ptr<MixFile> m;
        try { m = std::make_unique<MixFile>(ra2_->open_nested(name)); } catch (...) {}
        MixFile* raw = m.get();
        nested_.emplace(name, std::move(m));
        return raw;
    }
    const ra2assets::MixFile* ra2_ = nullptr;
    std::map<std::string, std::unique_ptr<MixFile>> nested_;
    std::map<std::string, BldAnims> bld_;
    const Ini* rules_ = nullptr;
    const Ini* art_ = nullptr;
    std::unique_ptr<MixFile> conquer_, conqmd_;
    ra2assets::Palette pal_;
    bool has_pal_ = false;
    std::map<std::string, std::unique_ptr<FxAnimType>> types_;
    std::map<std::string, FxWeapon> weapons_;
    std::map<std::string, std::vector<std::string>> explosions_;
    std::map<std::string, std::shared_ptr<ra2assets::ShpFile>> proj_shps_;
    std::vector<Anim> anims_;
};

PresFx* g_fx = nullptr;   // the live game's effects (GameScene); null in gates

// Projectile Image= art in flight [G-83]. Returns false (caller draws the old
// placeholder) when there is no FX layer or no art for this shooter.
bool pres_draw_projectile(SDL_Renderer* ren, ra2engine::TextureCache& tc,
                          const ra2engine::World& world, const ra2engine::Projectile& p, int gx,
                          int sy, int W, int H) {
    if (!g_fx || p.attacker < 0 || p.attacker >= static_cast<int>(world.units().size())) return false;
    const FxWeapon* w = g_fx->weapon(world.units()[p.attacker].image);
    if (!w || w->proj_image.empty()) return false;
    auto shp = g_fx->projectile_shp(w->proj_image);
    if (!shp || shp->count() <= 0) return false;
    int fr = 0;
    if (w->proj_rotates && shp->count() > 1) {
        // Facing frames: 0 = north, clockwise (TS/RA2 bullet SHP convention)
        // [RE-FALLBACK: RE-P5 open -- rotating-bullet frame order].
        // Screen-space velocity (all three axes in leptons; voxel-render.md §6).
        const double sdx = (p.vx - p.vy) * 30.0, sdy = (p.vx + p.vy) * 15.0 - p.vz * 36.74;
        const double ang = std::atan2(sdx, -sdy);
        const int n = shp->count();
        fr = static_cast<int>(std::lround((ang < 0 ? ang + 2 * 3.14159265358979 : ang) / (2 * 3.14159265358979) * n)) % n;
    }
    g_fx->blit(ren, tc, *shp, "proj:" + w->proj_image, fr, gx, sy, false, W, H);
    return true;
}

long g_pres_tick = 0;          // presentation game-frame clock (Presentation::update)
bool g_pres_low_power = false;  // house 0 drain > output (Presentation::power_eva)

// G-85: building overlays, called by draw_building. pass 0 = BibShape (under
// the main image), pass 1 = active anims + damage fires (over it). `pal` is
// the building's house-remapped palette; `ckey` its colour key.
void pres_building_overlay(SDL_Renderer* ren, ra2engine::TextureCache& tc, const std::string& image,
                           int fcx, int fcy, bool damaged, bool powered, const ra2assets::Palette& pal,
                           int ckey, int W, int H, int pass) {
    if (!g_fx) return;
    const PresFx::BldAnims& a = g_fx->building(pres_upper(image));
    const std::string ck = ":c" + std::to_string(ckey);
    if (pass == 0) {
        if (a.bib && a.bib->count() > 0)
            g_fx->blit(ren, tc, *a.bib, "bib:" + image + ck, damaged && a.bib->count() > 1 ? 1 : 0, fcx,
                       fcy, false, W, H, &pal);
        return;
    }
    for (const PresFx::BldAnims::Slot& s : a.slots) {
        const FxAnimType* t = (damaged && s.dmg) ? s.dmg : s.t;
        if (!t) continue;
        // ActiveAnimPowered (default yes): the anim holds still without power.
        const int fr = (s.powered && !powered) ? t->start : PresFx::loop_frame(*t, g_pres_tick);
        g_fx->blit(ren, tc, *t->shp, "banim:" + t->name + ck, fr, fcx + s.x, fcy + s.y, t->translucent,
                   W, H, &pal);
    }
    if (damaged && !a.fire_offsets.empty()) {
        // [RE-FALLBACK: RE-P6 open -- fire spawn code not read] one
        // DamageFireTypes anim per DamageFireOffset while below ConditionYellow,
        // offsets in pixels from the building's centre.
        static std::vector<std::string> fires = g_fx->damage_fires();
        if (fires.empty()) return;
        for (std::size_t i = 0; i < a.fire_offsets.size(); ++i) {
            const FxAnimType* t = g_fx->type(fires[i % fires.size()]);
            if (!t) continue;
            const int fr = PresFx::loop_frame(*t, g_pres_tick + static_cast<long>(i) * 7);
            g_fx->blit(ren, tc, *t->shp, "fx:" + t->name, fr, fcx + a.fire_offsets[i].first,
                       fcy + a.fire_offsets[i].second, t->translucent, W, H);
        }
    }
}