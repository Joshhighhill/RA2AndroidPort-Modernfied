// pres_map.inc -- G-87 map dressing in the real game: the map's [Terrain]
// objects (trees, rocks, lamp posts), non-ore [OverlayPack] overlays (walls,
// fences, sandbags, bridges), [Smudge] craters/scorch marks and the neutral
// [Structures] (tech buildings join the world as neutral buildings; civilian
// eye-candy is drawn + blocks its foundation). Formats per maps.md §4.5/§4.7
// (packed cell 1000*y+x; Smudge/Structures "TYPE,x,y,..."/"HOUSE,TYPE,HP,x,y").
// Art resolves through the theater candidates (Theater=yes -> .tem/.sno/.urb,
// NewTheater buildings -> munged .shp), conquer.mix last.
// Included by main.cpp BEFORE render_world (which calls the draw hooks).

class PresMap {
public:
    struct Obj {
        std::shared_ptr<ra2assets::ShpFile> shp;
        int x = 0, y = 0, fw = 1, fh = 1, level = 0, frame = 0;
        int pal = 0;   // 0 iso/terrain palette, 1 theater (overlay) palette, 2 unit palette
        std::string key;
        bool centered = true;   // building-style anchor (foundation centre) vs ore-style top-left
        int ovl_id = -1;        // G16: runtime overlay index (live bridge state lookup)
        // W2-GA-06/08 [RE: TerrainClass::Draw_It FUN_0071b450]: terrain objects
        // draw their shadow half (frame + Frames/2, Darken) -- DAT_00821cf1 is a
        // constant-true gate (RE-Q1); SpawnsTiberium types (ore mines) draw
        // through the UNIT palette convert 0x87E63C 16 px higher, and
        // IsAnimated types step frames at AnimationRate.
        bool shadow = false;
        int lift = 0;                 // screen px up
        bool animated = false;
        int anim_rate = 1;
        double anim_prob = 0.0;
        // Render-only animation state (never touches the sim).
        mutable int anim_stage = -1;  // -1 idle, else current frame
        mutable int anim_timer = 0;
        mutable uint32_t anim_rng = 0;
        // W2-GA-P1: per-frame texture pointers (pinned), no per-frame string keys.
        mutable std::vector<SDL_Texture*> tex, stex;
        mutable uint64_t tex_tc = 0;
    };

    // `map_ini` = the raw map text, `defs` the playable building roster (for
    // tech buildings that join the world). Returns cells to block.
    void load(const ra2assets::MixFile& ra2, const Ini& map_ini, const Ini& rules_ini, const Ini& art_ini,
              const ra2assets::MapFile& map) {
        objects_.clear(); smudges_.clear(); overlays_.clear(); wall_ids_.clear(); block_.clear();
        missing_ = placed_ = civ_ = 0;
        ra2_ = &ra2;
        rules_ = &rules_ini;
        art_ = &art_ini;
        map_ = &map;
        // [Terrain]
        for (const auto& kv : map_ini.section("Terrain")) {
            const int packed = std::atoi(kv.first.c_str());
            const std::string type = pres_upper(pres_clean(kv.second));
            Obj o;
            o.x = packed % 1000;
            o.y = packed / 1000;
            o.shp = art_shp(type);
            if (!o.shp) { ++missing_; continue; }
            o.key = "terr:" + type;
            o.level = level(o.x, o.y);
            o.pal = 0;
            o.shadow = true;
            if (pres_lower(rget(type, "SpawnsTiberium")).rfind("y", 0) == 0) {
                o.pal = 2;          // RE-Q1: the unit palette convert
                o.lift = 16;
            }
            if (pres_lower(rget(type, "IsAnimated")).rfind("y", 0) == 0) {
                o.animated = true;
                o.anim_rate = std::max(1, std::atoi(rget(type, "AnimationRate").c_str()));
                o.anim_prob = std::atof(rget(type, "AnimationProbability").c_str());
                o.anim_rng = 0x9E3779B9u ^ static_cast<uint32_t>(packed * 2654435761u);
            }
            objects_.push_back(o);
            block_.push_back({o.x, o.y});
        }
        // [Smudge] TYPE,x,y,0
        for (const auto& kv : map_ini.section("Smudge")) {
            const std::vector<std::string> f = fields(kv.second);
            if (f.size() < 3) continue;
            Obj o;
            const std::string type = pres_upper(pres_clean(f[0]));
            o.x = std::atoi(f[1].c_str());
            o.y = std::atoi(f[2].c_str());
            o.shp = art_shp(type);
            if (!o.shp) { ++missing_; continue; }
            o.key = "smudge:" + type;
            o.level = level(o.x, o.y);
            o.pal = 0;
            o.centered = false;
            smudges_.push_back(o);
        }
        // Non-ore overlays.
        // G16 (agent G) [RE: world-depth.md §B1]: [OverlayPack] stores the
        // RUNTIME index = order of appearance in [OverlayTypes]; the ini's own
        // numbers start at 1 and skip 40/41/183, so keying by them shifted every
        // overlay (BRIDGE1 read as DUMMY15, LOBRDG01 as PALET02, ...).
        std::map<int, std::string> otypes;
        names_ = ra2engine::overlay_names_in_order(rules_ini.section("OverlayTypes"));
        for (std::size_t i = 0; i < names_.size(); ++i) otypes[static_cast<int>(i)] = names_[i];
        if (map.has_overlay()) {
            for (const ra2assets::MapCell& c : map.cells()) {
                const int id = map.overlay_at(c.x, c.y);
                if (id < 0 || id == 255) continue;
                auto it = otypes.find(id);
                if (it == otypes.end()) continue;
                const std::string& name = it->second;
                if (pres_lower(rget(name, "Tiberium")).rfind("y", 0) == 0) continue;   // ore/gems: OreField
                if (name.rfind("DUMMY", 0) == 0) continue;
                auto shp = art_shp(name);
                if (!shp) { ++missing_; continue; }
                Obj o;
                o.shp = shp;
                o.x = c.x;
                o.y = c.y;
                o.level = c.level;
                o.frame = map.overlay_data_at(c.x, c.y);
                o.key = "ovl:" + name;
                o.ovl_id = id;
                const bool wall = pres_lower(rget(name, "Wall")).rfind("y", 0) == 0;
                // Non-ore overlays (rocks, bridges, low bridges) draw through the
                // cell convert = the ISO palette (visual check: srock/bridge art is
                // clean only with iso*.pal); walls use the unit palette.
                o.pal = wall ? 2 : 0;
                o.centered = false;
                overlays_.push_back(o);
                if (wall) { wall_ids_[c.y * 1000 + c.x] = id; block_.push_back({c.x, c.y}); }
            }
            // Walls draw their N/E/S/W connection frame (roster2.md §D: frame =
            // stage*16 + mask; map walls start undamaged).
            for (Obj& o : overlays_) {
                auto self = wall_ids_.find(o.y * 1000 + o.x);
                if (self == wall_ids_.end()) continue;
                auto same = [&](int x, int y) {
                    auto w = wall_ids_.find(y * 1000 + x);
                    return w != wall_ids_.end() && w->second == self->second;
                };
                o.frame = ra2engine::wall_mask(o.x, o.y, same);
            }
        }
        std::cerr << "[map-dressing] terrain=" << objects_.size() << " smudge=" << smudges_.size()
                  << " overlays=" << overlays_.size() << " missing-art=" << missing_ << "\n";
    }

    // Neutral [Structures]: roster types -> World buildings of `neutral_house`;
    // everything else (civilian buildings) -> drawn dressing with a blocked
    // foundation.
    template <class AddBuilding>
    void place_structures(const Ini& map_ini, AddBuilding add_building) {
        for (const auto& kv : map_ini.section("Structures")) {
            const std::vector<std::string> f = fields(kv.second);
            if (f.size() < 5) continue;
            const std::string type = pres_upper(pres_clean(f[1]));
            const int x = std::atoi(f[3].c_str()), y = std::atoi(f[4].c_str());
            if (add_building(type, x, y, std::atoi(f[2].c_str()))) { ++placed_; continue; }
            std::string image = pres_clean(rget(type, "Image"));
            if (image.empty()) image = type;
            Obj o;
            o.shp = art_shp(pres_upper(image));
            if (!o.shp) { ++missing_; continue; }
            int fw = 1, fh = 1;
            const std::string fnd = pres_clean(art_->get(pres_upper(image), "Foundation"));
            if (fnd.size() >= 3) {
                fw = std::max(1, std::atoi(fnd.c_str()));
                const std::size_t xx = fnd.find_first_of("xX");
                if (xx != std::string::npos) fh = std::max(1, std::atoi(fnd.c_str() + xx + 1));
            }
            o.x = x;
            o.y = y;
            o.fw = fw;
            o.fh = fh;
            o.level = level(x, y);
            o.key = "civ:" + image;
            o.pal = 2;
            o.shadow = true;
            objects_.push_back(o);
            for (int j = 0; j < fh; ++j)
                for (int i = 0; i < fw; ++i) block_.push_back({x + i, y + j});
            ++civ_;
        }
        std::cerr << "[map-dressing] structures: neutral-world=" << placed_ << " civilian-dressing=" << civ_
                  << "\n";
    }

    // W2-GA-15: a crater (Crater=yes smudge types) or scorch mark (Burn=yes)
    // left where a Crater=/Scorch= explosion anim went off; one per cell.
    void add_smudge(bool crater, double fx, double fy) {
        const int x = static_cast<int>(std::lround(fx)), y = static_cast<int>(std::lround(fy));
        if (!map_ || !map_->at(x, y)) return;
        if (!smudge_cells_.insert(y * 1000 + x).second) return;
        if (smudge_kinds_[0].empty() && rules_) {
            for (const auto& kv : rules_->section("SmudgeTypes")) {
                const std::string n = pres_upper(pres_clean(kv.second));
                if (n.empty()) continue;
                if (!rget(n, "Width").empty() || !rget(n, "Height").empty()) continue;   // 1x1 only
                if (pres_lower(rget(n, "Crater")).rfind("y", 0) == 0) smudge_kinds_[0].push_back(n);
                else if (pres_lower(rget(n, "Burn")).rfind("y", 0) == 0) smudge_kinds_[1].push_back(n);
            }
        }
        const std::vector<std::string>& kinds = smudge_kinds_[crater ? 0 : 1];
        if (kinds.empty()) return;
        smudge_rng_ = smudge_rng_ * 1103515245u + 12345u;
        const std::string& type = kinds[(smudge_rng_ >> 8) % kinds.size()];
        Obj o;
        o.shp = art_shp(type);
        if (!o.shp) return;
        o.x = x;
        o.y = y;
        o.level = level(x, y);
        o.key = "smudge:" + type;
        o.pal = 0;
        o.centered = false;
        smudges_.push_back(o);
    }
    const std::vector<std::pair<int, int>>& blocked() const { return block_; }
    // W2-SK-01: the map's ore mines ([Terrain] types with SpawnsTiberium=yes)
    // with their animation data, for World::add_ore_mine.
    template <class F> void for_each_ore_mine(F f) const {
        for (const Obj& o : objects_) {
            if (o.key.rfind("terr:", 0) != 0 || !o.shp) continue;
            const std::string type = o.key.substr(5);
            if (pres_lower(rget(type, "SpawnsTiberium")).rfind("y", 0) != 0) continue;
            const std::string rate = rget(type, "AnimationRate"), prob = rget(type, "AnimationProbability");
            f(o.x, o.y, std::max(1, o.shp->count() / 2), rate.empty() ? 3 : std::atoi(rate.c_str()),
              prob.empty() ? 0.003 : std::atof(prob.c_str()));
        }
    }
    const std::vector<Obj>& objects() const { return objects_; }
    const std::vector<Obj>& overlays() const { return overlays_; }

    void draw_ground(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::World& w,
                     const ra2assets::Palette* pals[3], const ra2engine::Camera& cam, int W, int H) {
        step_anims();
        for (const Obj& o : smudges_) draw(ren, tc, w, o, pals, cam, W, H);
        for (const Obj& o : overlays_) draw(ren, tc, w, o, pals, cam, W, H);
    }
    // One depth-sorted object (render_world merges these with buildings/units).
    void draw(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::World& w, const Obj& o,
              const ra2assets::Palette* pals[3], const ra2engine::Camera& cam, int W, int H) {
        if (!w.explored_at(0, o.x, o.y)) return;
        // G16: a low bridge section shows its LIVE overlay (damaged / destroyed /
        // repaired) -- World::bridge_overlay_at.
        if (o.ovl_id >= 0 && w.bridges_enabled()) {
            const int live = w.bridge_overlay_at(o.x, o.y);
            if (live >= 0 && live != o.ovl_id && live < static_cast<int>(names_.size())) {
                Obj alt = o;
                alt.ovl_id = -1;
                alt.tex.clear();
                alt.stex.clear();
                alt.tex_tc = 0;
                alt.shp = art_shp(names_[live]);
                alt.key = "ovl:" + names_[live];
                if (alt.shp) draw(ren, tc, w, alt, pals, cam, W, H);
                return;
            }
        }
        const int images = std::max(1, o.shp->count() / 2);   // second half = shadows
        const int want = o.animated && o.anim_stage >= 0 ? o.anim_stage : o.frame;
        const int fr = std::clamp(want, 0, std::max(0, std::min(images, o.shp->count()) - 1));
        const ra2assets::ShpFile::Frame f = o.shp->frame_info(fr);
        if (f.w <= 0 || f.h <= 0) return;
        const ra2engine::ScreenPt p = ra2engine::cell_to_screen(o.x, o.y, o.level, 60, 30, 15);
        int ox, oy;   // SHP canvas origin on screen
        if (o.centered) {
            // Foundation bbox centre (building-render.md §2 centre registration).
            const int cx = p.x + cam.x + 30 + (o.fw - o.fh) * 15;
            const int cy = p.y + cam.y + 15 + (o.fw + o.fh - 2) * 15 / 2;
            ox = cx - o.shp->width() / 2;
            oy = cy - o.shp->height() / 2 - o.lift;
        } else {
            ox = p.x + cam.x;
            oy = p.y + cam.y - o.lift;
        }
        const SDL_Rect dst{ox + f.x, oy + f.y, f.w, f.h};
        // Shadow extents can exceed the image: cull on a generous box.
        if (dst.x + dst.w + 96 <= 0 || dst.y + dst.h + 64 <= 0 || dst.x - 96 >= W || dst.y - 64 >= H) return;
        if (o.tex_tc != tc.instance_id()) {
            o.tex.assign(static_cast<std::size_t>(o.shp->count()), nullptr);
            o.stex.assign(static_cast<std::size_t>(o.shp->count()), nullptr);
            o.tex_tc = tc.instance_id();
        }
        // Shadow half first, 50 % Darken (same blend as buildings, G-85).
        const int sf = fr + o.shp->count() / 2;
        if (o.shadow && o.shp->count() >= 2 && sf < o.shp->count()) {
            const ra2assets::ShpFile::Frame s = o.shp->frame_info(sf);
            if (s.w > 0 && s.h > 0) {
                SDL_Texture*& st = o.stex[static_cast<std::size_t>(sf)];
                if (!st) {
                    const std::string skey = o.key + ":sh" + std::to_string(sf);
                    st = static_cast<SDL_Texture*>(tc.peek(skey));
                    if (!st) {
                        const std::vector<uint8_t> si = o.shp->decode_indexed(sf);
                        std::vector<uint8_t> rgba(si.size() * 4, 0);
                        for (std::size_t k = 0; k < si.size(); ++k) if (si[k]) rgba[k * 4 + 3] = 128;
                        st = static_cast<SDL_Texture*>(tc.get_rgba(skey, rgba.data(), s.w, s.h));
                    }
                    tc.pin(skey);
                }
                // The shadow sits on the ground: no SpawnsTiberium lift.
                const SDL_Rect sd{ox + s.x, oy + o.lift + s.y, s.w, s.h};
                SDL_RenderCopy(ren, st, nullptr, &sd);
            }
        }
        SDL_Texture*& tex = o.tex[static_cast<std::size_t>(fr)];
        if (!tex) {
            const std::string key = o.key + ":" + std::to_string(fr);
            tex = static_cast<SDL_Texture*>(tc.peek(key));
            if (!tex) {
                const std::vector<uint8_t> idx = o.shp->decode_indexed(fr);
                tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, *pals[o.pal]));
            }
            tc.pin(key);
        }
        SDL_RenderCopy(ren, tex, nullptr, &dst);
    }

    // IsAnimated terrain (TIBTRE ore mines) [RE: FUN_0071b450 frame = anim
    // stage when IsAnimated]: each game frame an idle object starts with
    // AnimationProbability, then steps one frame per AnimationRate ticks
    // through the image half and returns to idle. Render-only RNG.
    void step_anims() {
        const long now = g_pres_tick;
        if (now == last_tick_) return;
        const long n = std::min<long>(now - last_tick_, 30);
        last_tick_ = now;
        for (long t = 0; t < n; ++t)
            for (const Obj& o : objects_) {
                if (!o.animated || !o.shp) continue;
                const int images = std::max(1, o.shp->count() / 2);
                if (o.anim_stage < 0) {
                    o.anim_rng = o.anim_rng * 1664525u + 1013904223u;
                    if ((o.anim_rng >> 8) * (1.0 / 16777216.0) < o.anim_prob) { o.anim_stage = 0; o.anim_timer = 0; }
                } else if (++o.anim_timer >= o.anim_rate) {
                    o.anim_timer = 0;
                    if (++o.anim_stage >= images) o.anim_stage = -1;
                }
            }
    }

private:
    static std::vector<std::string> fields(const std::string& v) {
        std::vector<std::string> f;
        std::string cur;
        for (char c : v) {
            if (c == ',') { f.push_back(cur); cur.clear(); } else cur += c;
        }
        f.push_back(cur);
        return f;
    }
    std::string rget(const std::string& s, const std::string& k) const {
        return pres_clean(rules_->get(s, k));
    }
    int level(int x, int y) const {
        const ra2assets::MapCell* c = map_->at(x, y);
        return c ? c->level : 0;
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
    // Theater=yes art -> <name>.<theater ext>; NewTheater/plain -> .shp;
    // generic.mix / conquer.mix last.
    std::shared_ptr<ra2assets::ShpFile> art_shp(const std::string& name) {
        auto it = cache_.find(name);
        if (it != cache_.end()) return it->second;
        const ra2assets::TheaterChain& ch = art_chain();
        // W2-GA-07: Image= from artmd, else from rules (BRIDGE1/2 carry
        // Image=BRIDGE in rulesmd only), else the name itself.
        std::string image = pres_clean(art_->get(name, "Image"));
        if (image.empty()) image = rget(name, "Image");
        if (image.empty()) image = name;
        std::vector<ra2assets::TheaterCandidate> cands;
        for (const std::string ext : {std::string(".") + ch.ext, std::string(".shp")})
            for (const auto& c : ra2assets::theater_candidates(image, ch.letter, ch.iso_mix, ch.art_mix, ext))
                cands.push_back(c);
        cands.push_back({"conquer.mix", pres_lower(image) + ".shp"});
        std::shared_ptr<ra2assets::ShpFile> out;
        for (const auto& c : cands) {
            MixFile* m = nested(c.mix);
            if (!m) continue;
            const MixEntry* e = m->find(c.file);
            if (!e) e = m->find(pres_lower(c.file));
            if (!e) continue;
            try {
                out = std::make_shared<ra2assets::ShpFile>(ShpFile::load(m->read(*e)));
                break;
            } catch (...) {}
        }
        cache_[name] = out;
        return out;
    }

    const ra2assets::MixFile* ra2_ = nullptr;
    const Ini* rules_ = nullptr;
    const Ini* art_ = nullptr;
    const ra2assets::MapFile* map_ = nullptr;
    std::map<std::string, std::unique_ptr<MixFile>> nested_;
    std::map<std::string, std::shared_ptr<ra2assets::ShpFile>> cache_;
    std::vector<Obj> objects_, smudges_, overlays_;
    std::vector<std::string> names_;   // [OverlayTypes] by runtime index
    std::map<int, int> wall_ids_;
    std::vector<std::pair<int, int>> block_;
    int missing_ = 0, placed_ = 0, civ_ = 0;
    long last_tick_ = 0;
    std::set<int> smudge_cells_;
    std::vector<std::string> smudge_kinds_[2];
    uint32_t smudge_rng_ = 0x5A17u;
};

PresMap* g_map_dressing = nullptr;                    // live game only
const ra2assets::Palette* g_map_pals[3] = {nullptr, nullptr, nullptr};   // iso, theater, unit
