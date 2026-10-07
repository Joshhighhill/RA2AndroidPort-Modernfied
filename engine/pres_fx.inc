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
    // W3-B AnimClass draw keys [RE: AnimTypeClass INI FUN_00427c80 + draw
    // FUN_00422cc0]: Translucent=yes FADES over the anim's life (frame >
    // 0.2/0.4/0.6 x End -> 25/50/75 % translucent); Translucency=25/50/75 is
    // a fixed level; Shadow=yes draws frame + SHP/2 as a 50 % darken under
    // the body; Flat=yes / Layer=underground|surface draw on the ground under
    // every object; AltPalette=yes draws through a house colour-scheme convert
    // of the theater unit palette (the owner's, else scheme 0 LightGold).
    bool translucent = false;  // Translucent=yes (life fade)
    int translucency = 0;      // Translucency=25/50/75 (0 = opaque)
    bool shadow = false;       // Shadow=yes
    bool flat = false;         // Flat=yes or Layer=underground/surface
    bool altpal = false;       // AltPalette=yes
    int rand_min = 0, rand_max = 0;   // RandomRate=a,b as delays (900/x), 0 = unset
    std::string report;        // StartSound= (wins), else Report= sound event
    std::string next;          // Next= chained anim
    bool crater = false, scorch = false;   // W2-GA-15 Crater=/Scorch= (leave a smudge)
    // [RE: presentation.md §5] Rate= stores 900/Rate game frames per anim
    // frame (no Rate -> 1); Start/End (End defaults to the image frames),
    // LoopStart/LoopEnd (LoopEnd defaults to End), LoopCount (0/1 = once,
    // -1 = 255): every pass but the last loops LoopStart..LoopEnd, the last
    // runs to End, then Next= chains.
    int delay = 1;
    int start = 0, end = 0, loop_start = 0, loop_end = 0, loops = 1;
    // Anim frame for game-frame tick 	, or -1 once finished.
    int frame_at(long t) const { return frame_at(t, delay); }
    int frame_at(long t, int dly) const {
        if (dly <= 0) return start;
        long step = t / dly;
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
    // W2-GA-15 [RE: rulesmd weapon IsElectricBolt (Tesla coil / trooper /
    // tank), IsLaser + LaserDuration + IsHouseColor (Prism)]: drawn as a beam
    // from the muzzle to the target instead of only an impact.
    bool bolt = false, laser = false, laser_house = false;
    int laser_frames = 15;
};
// W2-GA-15: smudge hook (craters / scorch marks left by Crater=/Scorch=
// explosion anims), set by GameScene to its map dressing.
std::function<void(bool crater, double fx, double fy)> g_smudge_hook;

class PresFx {
public:
    struct Anim {
        const FxAnimType* t = nullptr;
        double sx = 0, sy = 0;   // cell-space position (fx, fy)
        int level = 0;
        double lift_px = 0;      // screen-space height above the ground point
        long age = 0;            // W3-B: logic ticks since spawn (was wall-clock seconds)
        double ox_px = 0;        // extra screen-x offset (FLH muzzle point)
        int house = -1;          // W3-B owner (AltPalette remap), -1 none
        int delay = 1;           // W3-B per-instance delay (RandomRate), else the type's
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
        if (!w.empty()) fill_beam(fw, w);
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
    void fill_beam(FxWeapon& fw, const std::string& w) {
        auto yes = [&](const char* k) {
            const std::string v = pres_lower(rget(w, k));
            return v.rfind("y", 0) == 0 || v.rfind("t", 0) == 0;
        };
        fw.bolt = yes("IsElectricBolt");
        fw.laser = yes("IsLaser");
        fw.laser_house = yes("IsHouseColor");
        const std::string ld = rget(w, "LaserDuration");
        if (!ld.empty()) fw.laser_frames = std::max(1, std::atoi(ld.c_str()));
    }
    // A defence's weapon by id (Tesla Coil CoilBolt, Prism Tower PrismShot).
    const FxWeapon& weapon_def(const std::string& w) {
        auto it = weapon_defs_.find(w);
        if (it != weapon_defs_.end()) return it->second;
        FxWeapon fw;
        fill_beam(fw, w);
        return weapon_defs_.emplace(w, fw).first->second;
    }
    // ---- W2-GA-15 beams ----------------------------------------------------
    struct Beam {
        double x0, y0, lift0, x1, y1, lift1;
        int l0, l1;
        bool bolt;
        int life;          // game frames
        long age = 0;      // W3-B: logic ticks
        SDL_Color inner, outer;
        uint32_t seed;
    };
    void add_beam(double x0, double y0, int l0, double lift0, double x1, double y1, int l1, double lift1,
                  const FxWeapon& fw, SDL_Color house) {
        Beam b{x0, y0, lift0, x1, y1, lift1, l0, l1, fw.bolt, fw.bolt ? 8 : fw.laser_frames, 0,
               SDL_Color{255, 255, 255, 255}, SDL_Color{110, 110, 255, 255}, beam_seed_++ * 2654435761u};
        if (fw.laser) {
            // [RE-FALLBACK: LaserInner/OuterColor are commented out in rulesmd;
            // IsHouseColor=yes -> the firing house's colour, white core]
            b.outer = fw.laser_house ? house : SDL_Color{216, 0, 184, 255};
            b.inner = SDL_Color{255, 255, 255, 255};
        }
        beams_.push_back(b);
    }
    void draw_beams(SDL_Renderer* ren, const ra2engine::Camera& cam) const {
        if (beams_.empty()) return;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (const Beam& b : beams_) {
            auto pt = [&](double x, double y, int l, double lift, int& sx, int& sy) {
                sx = static_cast<int>(std::lround((x - y) * 30.0)) + cam.x + 30;
                sy = static_cast<int>(std::lround((x + y) * 15.0 - l * 15 - lift)) + cam.y + 15;
            };
            int ax, ay, bx, by;
            pt(b.x0, b.y0, b.l0, b.lift0, ax, ay);
            pt(b.x1, b.y1, b.l1, b.lift1, bx, by);
            const double fade = 1.0 - static_cast<double>(b.age) / std::max(1, b.life);
            const Uint8 a = static_cast<Uint8>(std::clamp(fade, 0.0, 1.0) * 255.0);
            if (!b.bolt) {
                SDL_SetRenderDrawColor(ren, b.outer.r, b.outer.g, b.outer.b, static_cast<Uint8>(a / 2));
                for (int o = -2; o <= 2; ++o) {
                    SDL_RenderDrawLine(ren, ax + o, ay, bx + o, by);
                    SDL_RenderDrawLine(ren, ax, ay + o, bx, by + o);
                }
                SDL_SetRenderDrawColor(ren, b.inner.r, b.inner.g, b.inner.b, a);
                SDL_RenderDrawLine(ren, ax, ay, bx, by);
                continue;
            }
            // Electric bolt: three jagged arcs re-jittered every frame.
            const double len = std::hypot(bx - ax, by - ay);
            const int segs = std::max(2, static_cast<int>(len / 14.0));
            const double nx = len > 0 ? -(by - ay) / len : 0, ny = len > 0 ? (bx - ax) / len : 0;
            uint32_t r = b.seed ^ static_cast<uint32_t>(beam_tick_ * 2246822519u);
            for (int arc = 0; arc < 3; ++arc) {
                const SDL_Color c = arc == 0 ? b.inner : b.outer;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, arc == 0 ? a : static_cast<Uint8>(a * 3 / 4));
                int px = ax, py = ay;
                for (int s = 1; s <= segs; ++s) {
                    r = r * 1664525u + 1013904223u;
                    const double off = s == segs ? 0.0 : (static_cast<int>((r >> 16) % 13) - 6);
                    const int qx = static_cast<int>(ax + (bx - ax) * s / static_cast<double>(segs) + nx * off);
                    const int qy = static_cast<int>(ay + (by - ay) * s / static_cast<double>(segs) + ny * off);
                    SDL_RenderDrawLine(ren, px, py, qx, qy);
                    px = qx;
                    py = qy;
                }
            }
        }
    }
    // W2-GA-15 radiation glow: cells with RadLevel tinted with [Radiation]
    // RadColor, alpha by level / RadLevelMax. [RE-FALLBACK: the original
    // lights the cell through a radiation LightSource; a translucent cell tint
    // stands in.]
    void draw_radiation(SDL_Renderer* ren, const ra2engine::World& w, const ra2assets::MapFile& map,
                        const ra2engine::Camera& cam, int W, int H) {
        const ra2engine::RadField& rf = w.rad_field();
        if (rf.empty() || rf.live_sites() <= 0) return;
        if (rad_max_ <= 0) {
            const std::vector<std::string> c = split(rget("Radiation", "RadColor"));
            if (c.size() >= 3) rad_color_ = SDL_Color{static_cast<Uint8>(std::atoi(c[0].c_str())),
                                                      static_cast<Uint8>(std::atoi(c[1].c_str())),
                                                      static_cast<Uint8>(std::atoi(c[2].c_str())), 255};
            const std::string mx = rget("Radiation", "RadLevelMax");
            rad_max_ = mx.empty() ? 500 : std::max(1, std::atoi(mx.c_str()));
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
        ra2engine::for_each_visible_cell(map, cam, W, H, 0, [&](const ra2assets::MapCell& c, int sx, int sy) {
            const double lv = rf.level_at(c.x, c.y);
            if (lv <= 0.0) return;
            const int a = static_cast<int>(std::min(1.0, lv / rad_max_) * 110.0);
            if (a <= 0) return;
            SDL_SetRenderDrawColor(ren, rad_color_.r, rad_color_.g, rad_color_.b, static_cast<Uint8>(a));
            for (int y = 0; y < 30; ++y) {   // the cell's diamond
                const int half = (y < 15 ? y + 1 : 30 - y) * 2;
                SDL_RenderDrawLine(ren, sx + 30 - half, sy + y, sx + 30 + half - 1, sy + y);
            }
        });
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    }
    SDL_Color rad_color_{0, 255, 0, 255};
    int rad_max_ = 0;
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
        // W2-GA-14 event slots [RE: presentation.md §6, the 21 BuildingType
        // anim slots; RE-Q13 open -> RE-FALLBACK ModEnc order]: the factory
        // door set (DeployingAnim over, UnderDoorAnim under, played once when a
        // unit leaves) and the superweapon charge states SuperAnim (opening
        // on READY) -> SuperAnimTwo (loop while ready) -> SuperAnimThree
        // (closing on FIRE) -> SuperAnimFour (loop while charging).
        Slot door, under_door, super_[4];
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
        auto event_slot = [&](const std::string& s) {
            BldAnims::Slot sl;
            sl.t = theater_type(aget(image, s));
            sl.dmg = theater_type(aget(image, s + "Damaged"));
            sl.x = std::atoi(aget(image, s + "X").c_str());
            sl.y = std::atoi(aget(image, s + "Y").c_str());
            return sl;
        };
        a.door = event_slot("DeployingAnim");
        a.under_door = event_slot("UnderDoorAnim");
        static const char* kSuper[4] = {"SuperAnim", "SuperAnimTwo", "SuperAnimThree", "SuperAnimFour"};
        for (int i = 0; i < 4; ++i) a.super_[i] = event_slot(kSuper[i]);
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
        auto yes = [&](const char* k) {
            const std::string v = pres_lower(aget(key, k));
            return v.rfind("y", 0) == 0 || v.rfind("t", 0) == 0 || v == "1";
        };
        t.translucent = yes("Translucent");
        t.translucency = std::atoi(aget(key, "Translucency").c_str());
        t.shadow = shadow;
        const std::string layer = pres_lower(aget(key, "Layer"));
        t.flat = yes("Flat") || layer == "underground" || layer == "surface";
        t.altpal = yes("AltPalette");
        t.report = aget(key, "StartSound");
        if (t.report.empty()) t.report = aget(key, "Report");
        {   // RandomRate=a,b [RE: presentation.md §5]: each 900/x, min <= max.
            const std::string rr = aget(key, "RandomRate");
            const std::size_t c = rr.find(',');
            if (c != std::string::npos) {
                const int a = std::atoi(rr.c_str()), b = std::atoi(rr.c_str() + c + 1);
                int da = a > 0 ? (a > 900 ? 0 : 900 / a) : 0, db = b > 0 ? (b > 900 ? 0 : 900 / b) : 0;
                if (da > db) std::swap(da, db);
                if (da > 0 || db > 0) { t.rand_min = std::max(1, da); t.rand_max = std::max(t.rand_min, db); }
            }
        }
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
        t.crater = pres_lower(aget(key, "Crater")).rfind("y", 0) == 0;
        t.scorch = pres_lower(aget(key, "Scorch")).rfind("y", 0) == 0;
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
                            double ox_px = 0.0, int house = -1) {
        const FxAnimType* t = type(name);
        if (!t || t->frames <= 0) return nullptr;
        anims_.push_back(Anim{t, fx, fy, level, lift_px, 0, ox_px, house, instance_delay(*t)});
        if ((t->crater || t->scorch) && g_smudge_hook && lift_px < 30.0) g_smudge_hook(t->crater, fx, fy);
        return t;
    }
    // W3-B: one logic tick (Presentation::logic_tick, inside GameScene's
    // fixed 15 Hz loop) -- anims and beams age in game frames, never by the
    // render rate.
    void tick() {
        ++beam_tick_;
        for (Beam& b : beams_) ++b.age;
        beams_.erase(std::remove_if(beams_.begin(), beams_.end(),
                                    [](const Beam& b) { return b.age >= b.life; }),
                     beams_.end());
        std::vector<Anim> next;
        for (Anim& a : anims_) {
            ++a.age;
            if (a.t->frame_at(a.age, a.delay) >= 0) continue;
            if (!a.t->next.empty())
                if (const FxAnimType* n = type(a.t->next))
                    next.push_back(Anim{n, a.sx, a.sy, a.level, a.lift_px, 0, a.ox_px, a.house, instance_delay(*n)});
            a.t = nullptr;
        }
        anims_.erase(std::remove_if(anims_.begin(), anims_.end(), [](const Anim& a) { return !a.t; }),
                     anims_.end());
        anims_.insert(anims_.end(), next.begin(), next.end());
    }
    // pass 0: Flat / ground-layer anims, drawn after the ground overlays and
    // before every object; pass 1 (default): the rest, over the objects
    // [RE-FALLBACK: Ground-layer anims are depth-sorted with objects in the
    // original (W2-GA-17 open); the clone draws them after the objects].
    void draw(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::Camera& cam, int W,
              int H, int pass = 1) {
        for (const Anim& a : anims_) {
            if (a.t->flat != (pass == 0)) continue;
            const int fr = a.t->frame_at(a.age, a.delay);
            if (fr < 0) continue;
            const int cx = static_cast<int>(std::lround((a.sx - a.sy) * 30.0 + a.ox_px)) + cam.x + 30;
            const int cy = static_cast<int>(std::lround((a.sx + a.sy) * 15.0 - a.level * 15 - a.lift_px)) +
                           cam.y + 15;
            draw_anim_frame(ren, tc, *a.t, fr, cx, cy, W, H, a.t->altpal ? alt_palette(a.house) : nullptr,
                            a.t->altpal ? ":h" + std::to_string(a.house) : std::string());
        }
        if (pass == 1) draw_beams(ren, cam);   // W2-GA-15
    }
    // W3-B: one anim frame with its Shadow (darken, under the body) and its
    // translucency; `pal` overrides anim.pal (AltPalette / building convert).
    void draw_anim_frame(SDL_Renderer* ren, ra2engine::TextureCache& tc, const FxAnimType& t, int fr, int cx,
                         int cy, int W, int H, const ra2assets::Palette* pal = nullptr,
                         const std::string& tag_extra = std::string()) {
        if (t.shadow) blit_shadow(ren, tc, *t.shp, "fxs:" + t.name, fr + t.shp->count() / 2, cx, cy, W, H);
        blit(ren, tc, *t.shp, "fx:" + t.name + tag_extra, fr, cx, cy, alpha_for(t, fr), W, H, pal);
    }
    // [RE: FUN_00422cc0] Translucency=25/50/75 -> BlitterFlags 2/4/6;
    // Translucent=yes -> by life: frame > 0.6 End -> 6, > 0.4 -> 4, > 0.2 -> 2.
    // Flag 2/4/6 = 25/50/75 % translucent -> alpha 191/128/64.
    static int alpha_for(const FxAnimType& t, int fr) {
        int lvl = 0;
        if (t.translucent) {
            const double e = t.end;
            lvl = fr > e * 0.6 ? 3 : fr > e * 0.4 ? 2 : fr > e * 0.2 ? 1 : 0;
        } else if (t.translucency >= 75) lvl = 3;
        else if (t.translucency >= 50) lvl = 2;
        else if (t.translucency >= 25) lvl = 1;
        static const int kA[4] = {255, 191, 128, 64};
        return kA[lvl];
    }
    // The Shadow= half: every non-zero pixel halves the destination
    // [RE: presentation.md §7 Darken blitter] -> black at alpha 128.
    void blit_shadow(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2assets::ShpFile& shp,
                     const std::string& tag, int fr, int cx, int cy, int W, int H) {
        if (fr < 0 || fr >= shp.count()) return;
        const ra2assets::ShpFile::Frame f = shp.frame_info(fr);
        if (f.w <= 0 || f.h <= 0) return;
        SDL_Rect dst{cx - shp.width() / 2 + f.x, cy - shp.height() / 2 + f.y, f.w, f.h};
        if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H) return;
        const std::string key = tag + ":" + std::to_string(fr);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            const std::vector<uint8_t> idx = shp.decode_indexed(fr);
            std::vector<uint8_t> rgba(idx.size() * 4, 0);
            for (std::size_t i = 0; i < idx.size(); ++i) rgba[i * 4 + 3] = idx[i] ? 128 : 0;
            tex = static_cast<SDL_Texture*>(tc.get_rgba(key, rgba.data(), f.w, f.h));
        }
        if (tex) SDL_RenderCopy(ren, tex, nullptr, &dst);
    }
    // AltPalette [RE: FUN_00422cc0 + RE-Q3 resolved: the anim's explicit
    // convert (anim+0xD4, set from the owner house's ColorScheme where the
    // spawner passes one) else ColorSchemes[0] (rules [Colors] first entry,
    // LightGold 25,255,255)] over the theater unit palette.
    const ra2assets::Palette* alt_palette(int house) {
        if (!unit_pal_) return nullptr;
        auto it = alt_pals_.find(house);
        if (it != alt_pals_.end()) return &it->second;
        ra2assets::Palette p = house >= 0 && world_for_pal_ ? world_for_pal_->house_palette(*unit_pal_, house)
                                                            : ra2engine::pal_for_hsv(*unit_pal_, 25, 255, 255);
        return &alt_pals_.emplace(house, p).first->second;
    }
    void set_unit_palette(const ra2assets::Palette* unit_pal, const ra2engine::World* w) {
        if (unit_pal != unit_pal_ || w != world_for_pal_) alt_pals_.clear();
        unit_pal_ = unit_pal;
        world_for_pal_ = w;
    }
    // RandomRate: a fresh Random(min,max) delay per instance (render RNG).
    int instance_delay(const FxAnimType& t) {
        if (t.rand_max <= 0) return t.delay;
        fx_rng_ = fx_rng_ * 1664525u + 1013904223u;
        return t.rand_min + static_cast<int>((fx_rng_ >> 8) % static_cast<uint32_t>(t.rand_max - t.rand_min + 1));
    }
    // Centered SHP frame blit with anim.pal (`alpha` 255 = opaque).
    void blit(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2assets::ShpFile& shp,
              const std::string& tag, int fr, int cx, int cy, int alpha, int W, int H,
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
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(std::clamp(alpha, 0, 255)));
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
    std::map<std::string, FxWeapon> weapon_defs_;   // W2-GA-15 defence weapons by id
    std::vector<Beam> beams_;
    uint32_t beam_seed_ = 1;
    long beam_tick_ = 0;
    std::map<std::string, std::vector<std::string>> explosions_;
    std::map<std::string, std::shared_ptr<ra2assets::ShpFile>> proj_shps_;
    std::vector<Anim> anims_;
    const ra2assets::Palette* unit_pal_ = nullptr;          // W3-B AltPalette base
    const ra2engine::World* world_for_pal_ = nullptr;
    std::map<int, ra2assets::Palette> alt_pals_;
    uint32_t fx_rng_ = 0x5EED1234u;                          // W3-B render-only RNG
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
    g_fx->blit(ren, tc, *shp, "proj:" + w->proj_image, fr, gx, sy, 255, W, H);
    return true;
}

long g_pres_tick = 0;          // W3-B: LOGIC-tick anim clock (Presentation::logic_tick)
// W2-GA-14: the building draw_building is drawing (its overlay pass reads the
// per-building event state below) and that state, keyed by the building's
// identity (origin cell + type, stable for its life).
const ra2engine::Building* g_overlay_bld = nullptr;
struct BldEventState {
    long door_start = -1;          // tick a unit left the factory
    long super_start = -1;         // tick of the last SW state change
    int super_phase = -1;          // -1 none, 0 opening (ready), 2 closing (fired)
};
std::map<long long, BldEventState> g_bld_events;
inline long long bld_event_key(const ra2engine::Building& b) {
    return (static_cast<long long>(b.cx) << 32) ^ (static_cast<long long>(b.cy) << 16) ^ b.type;
}
// Frame of a one-shot anim started at `start`; -1 once it has run out.
inline int oneshot_frame(const FxAnimType& t, long start, long now) {
    if (start < 0) return -1;
    const long step = t.delay <= 0 ? 0 : (now - start) / t.delay;
    const long f = t.start + step;
    return f >= t.end ? -1 : static_cast<int>(f);
}
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
    const BldEventState* ev = nullptr;
    if (g_overlay_bld) {
        auto e = g_bld_events.find(bld_event_key(*g_overlay_bld));
        if (e != g_bld_events.end()) ev = &e->second;
    }
    auto slot_t = [&](const PresFx::BldAnims::Slot& s) { return (damaged && s.dmg) ? s.dmg : s.t; };
    if (pass == 0) {
        if (a.bib && a.bib->count() > 0)
            g_fx->blit(ren, tc, *a.bib, "bib:" + image + ck, damaged && a.bib->count() > 1 ? 1 : 0, fcx,
                       fcy, 255, W, H, &pal);
        // W2-GA-14: the dark interior behind an opening factory door.
        if (ev && ev->door_start >= 0)
            if (const FxAnimType* d = slot_t(a.door))
                if (oneshot_frame(*d, ev->door_start, g_pres_tick) >= 0)
                    if (const FxAnimType* u = slot_t(a.under_door))
                        g_fx->blit(ren, tc, *u->shp, "banim:" + u->name + ck, PresFx::loop_frame(*u, g_pres_tick),
                                   fcx + a.under_door.x, fcy + a.under_door.y, PresFx::alpha_for(*u, PresFx::loop_frame(*u, g_pres_tick)), W, H, &pal);
        return;
    }
    // W2-GA-14: factory door (one shot per exiting unit) and SW charge states.
    if (ev && ev->door_start >= 0)
        if (const FxAnimType* d = slot_t(a.door)) {
            const int fr = oneshot_frame(*d, ev->door_start, g_pres_tick);
            if (fr >= 0)
                g_fx->blit(ren, tc, *d->shp, "banim:" + d->name + ck, fr, fcx + a.door.x, fcy + a.door.y,
                           PresFx::alpha_for(*d, fr), W, H, &pal);
        }
    if (ev && ev->super_phase >= 0) {
        const int open = ev->super_phase;          // 0 (ready) or 2 (fired)
        const PresFx::BldAnims::Slot& s1 = a.super_[open];
        const PresFx::BldAnims::Slot& s2 = a.super_[open + 1];
        const FxAnimType* t1 = slot_t(s1);
        const int fr = t1 ? oneshot_frame(*t1, ev->super_start, g_pres_tick) : -1;
        if (t1 && fr >= 0)
            g_fx->blit(ren, tc, *t1->shp, "banim:" + t1->name + ck, fr, fcx + s1.x, fcy + s1.y, PresFx::alpha_for(*t1, fr),
                       W, H, &pal);
        else if (const FxAnimType* t2 = slot_t(s2))
            g_fx->blit(ren, tc, *t2->shp, "banim:" + t2->name + ck, PresFx::loop_frame(*t2, g_pres_tick),
                       fcx + s2.x, fcy + s2.y, 255, W, H, &pal);
    }
    for (const PresFx::BldAnims::Slot& s : a.slots) {
        const FxAnimType* t = (damaged && s.dmg) ? s.dmg : s.t;
        if (!t) continue;
        // ActiveAnimPowered (default yes): the anim holds still without power.
        const int fr = (s.powered && !powered) ? t->start : PresFx::loop_frame(*t, g_pres_tick);
        g_fx->blit(ren, tc, *t->shp, "banim:" + t->name + ck, fr, fcx + s.x, fcy + s.y, PresFx::alpha_for(*t, fr),
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
                       fcy + a.fire_offsets[i].second, PresFx::alpha_for(*t, fr), W, H);
        }
    }
}