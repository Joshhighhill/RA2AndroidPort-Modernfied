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
        std::map<int, std::string> otypes;
        for (const auto& kv : rules_ini.section("OverlayTypes"))
            otypes[std::atoi(kv.first.c_str())] = pres_upper(pres_clean(kv.second));
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
                const bool wall = pres_lower(rget(name, "Wall")).rfind("y", 0) == 0;
                o.pal = wall ? 2 : 1;
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
            objects_.push_back(o);
            for (int j = 0; j < fh; ++j)
                for (int i = 0; i < fw; ++i) block_.push_back({x + i, y + j});
            ++civ_;
        }
        std::cerr << "[map-dressing] structures: neutral-world=" << placed_ << " civilian-dressing=" << civ_
                  << "\n";
    }

    const std::vector<std::pair<int, int>>& blocked() const { return block_; }
    const std::vector<Obj>& objects() const { return objects_; }
    const std::vector<Obj>& overlays() const { return overlays_; }

    void draw_ground(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::World& w,
                     const ra2assets::Palette* pals[3], const ra2engine::Camera& cam, int W, int H) {
        for (const Obj& o : smudges_) draw(ren, tc, w, o, pals, cam, W, H);
        for (const Obj& o : overlays_) draw(ren, tc, w, o, pals, cam, W, H);
    }
    // One depth-sorted object (render_world merges these with buildings/units).
    void draw(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::World& w, const Obj& o,
              const ra2assets::Palette* pals[3], const ra2engine::Camera& cam, int W, int H) {
        if (!w.explored_at(0, o.x, o.y)) return;
        const int images = std::max(1, o.shp->count() / 2);   // second half = shadows
        const int fr = std::clamp(o.frame, 0, std::max(0, std::min(images, o.shp->count()) - 1));
        const ra2assets::ShpFile::Frame f = o.shp->frame_info(fr);
        if (f.w <= 0 || f.h <= 0) return;
        const ra2engine::ScreenPt p = ra2engine::cell_to_screen(o.x, o.y, o.level, 60, 30, 15);
        SDL_Rect dst;
        if (o.centered) {
            // Foundation bbox centre (building-render.md §2 centre registration).
            const int cx = p.x + cam.x + 30 + (o.fw - o.fh) * 15;
            const int cy = p.y + cam.y + 15 + (o.fw + o.fh - 2) * 15 / 2;
            dst = SDL_Rect{cx - o.shp->width() / 2 + f.x, cy - o.shp->height() / 2 + f.y, f.w, f.h};
        } else {
            dst = SDL_Rect{p.x + cam.x + f.x, p.y + cam.y + f.y, f.w, f.h};
        }
        if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H) return;
        const std::string key = o.key + ":" + std::to_string(fr);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            const std::vector<uint8_t> idx = o.shp->decode_indexed(fr);
            tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, *pals[o.pal]));
        }
        SDL_RenderCopy(ren, tex, nullptr, &dst);
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
        std::string image = pres_clean(art_->get(name, "Image"));
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
    std::map<int, int> wall_ids_;
    std::vector<std::pair<int, int>> block_;
    int missing_ = 0, placed_ = 0, civ_ = 0;
};

PresMap* g_map_dressing = nullptr;                    // live game only
const ra2assets::Palette* g_map_pals[3] = {nullptr, nullptr, nullptr};   // iso, theater, unit
