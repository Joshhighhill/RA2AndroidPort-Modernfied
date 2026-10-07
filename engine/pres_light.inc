// pres_light.inc -- W2-GA-05 map [Lighting] in the real game. Included by
// main.cpp before render_world. GameScene loads one PresLight per map and
// publishes it through g_light; every gate leaves it null, so no gate render
// changes.
//
// [RE: RE-Q7, FUN_00689890 reader, FUN_00484090 cell intensity, FUN_00555570
// tint normalisation, LightConvertClass FUN_00555a30/FUN_00555d20]. In
// per-mille units:
//   I     = Ambient*1000 + Level*1000*height - Ground*1000   (min 2000)
//   R,G,B = Red/Green/Blue * 1000; when not all 1000 the tint is normalised
//           so its max channel is 1000 and I *= max/1000 (brightness moves
//           into the intensity)
//   colour = min(255, palette * I/1000 * tint/1000)
// W3-FIX-A: building light sources (LightVisibility/Intensity/*Tint, e.g. the
// invisible lamp posts) per RA2 game.exe FUN_00479440 [RE: asset-search.md
// §4]: per cell, every active source within LightVisibility leptons adds
// intensity*f and tint*f, f = (vis - d)/vis (linear, per mille). Applied as a
// per-cell light map multiplied over the playfield (values clamp at 1.0
// [RE-FALLBACK]). Ion storm / dominator lighting is not modelled.
//
// Draw: terrain tiles are baked per height level (draw_terrain's level
// palettes, factor I(h)/I(0)); everything in the world then gets the level-0
// ambient*tint as one MOD-blend multiply over the playfield before the shroud
// pass (objects take the ground-level light; their per-height brightening is
// not modelled).

struct PresLight {
    // W3-FIX-A light sources (see set_sources).
    struct Src {
        double x = 0, y = 0;            // centre, leptons
        int vis = 0;                    // LightVisibility (leptons)
        int intensity = 0, r = 0, g = 0, b = 0;   // per mille (x1000)
    };
    struct LitCell { int x, y, level; Uint8 c[3]; Uint8 e[3]; };
    bool active = false;          // false = neutral lighting, nothing to do
    double ambient = 1.0, red = 1.0, green = 1.0, blue = 1.0, ground = 0.0, level = 0.0;
    double tint[3] = {1000, 1000, 1000};
    double scale = 1.0;           // normalisation factor applied to I
    std::vector<ra2assets::Palette> level_pals;   // terrain palette per height 0..15
    std::vector<Src> srcs_;
    std::vector<LitCell> lit_;
    uint64_t src_sig_ = 0;
    SDL_Texture* lmap_ = nullptr;      // the light map (freed with its renderer)
    SDL_Texture* xmap_ = nullptr;      // W5-PRES: the > 1.0 excess map (MUL)
    SDL_Renderer* lmap_ren_ = nullptr;
    int lmap_w_ = 0, lmap_h_ = 0;

    // Intensity (per mille, after normalisation) at height h.
    double intensity(int h) const {
        double i = ambient * 1000.0 + level * 1000.0 * h - ground * 1000.0;
        i = std::min(i, 2000.0) * scale;
        return std::clamp(i, 0.0, 2000.0);
    }

    void load(const Ini& map_ini, const ra2assets::Palette& iso_pal) {
        auto num = [&](const char* k, double d) {
            const std::string v = pres_clean(map_ini.get("Lighting", k));
            return v.empty() ? d : std::atof(v.c_str());
        };
        // The reader stores int(x*100+0.01) / int(x*1000+0.01): keep that
        // quantisation so 0.039 etc. land on the same per-mille values.
        ambient = static_cast<int>(num("Ambient", 1.0) * 100 + 0.01) / 100.0;
        red = static_cast<int>(num("Red", 1.0) * 100 + 0.01) / 100.0;
        green = static_cast<int>(num("Green", 1.0) * 100 + 0.01) / 100.0;
        blue = static_cast<int>(num("Blue", 1.0) * 100 + 0.01) / 100.0;
        ground = static_cast<int>(num("Ground", 0.0) * 1000 + 0.01) / 1000.0;
        level = static_cast<int>(num("Level", 0.0) * 1000 + 0.01) / 1000.0;
        double r = std::clamp(red * 1000.0, 0.0, 2000.0), g = std::clamp(green * 1000.0, 0.0, 2000.0),
               b = std::clamp(blue * 1000.0, 0.0, 2000.0);
        scale = 1.0;
        if (!(r == 1000.0 && g == 1000.0 && b == 1000.0)) {
            const double mx = std::max({r, g, b});
            scale = mx / 1000.0;
            if (scale * 65536.0 < 0x42) { r = g = b = 1000.0; scale = 0.0; }
            else { r = r / scale; g = g / scale; b = b / scale; }
        }
        tint[0] = r; tint[1] = g; tint[2] = b;
        const double i0 = intensity(0);
        active = !(std::abs(i0 - 1000.0) < 0.5 && r == 1000.0 && g == 1000.0 && b == 1000.0 && level == 0.0);
        level_pals.clear();
        if (!active) return;
        for (int h = 0; h < 16; ++h) {
            ra2assets::Palette p = iso_pal;
            const double f = i0 > 0.0 ? intensity(h) / i0 : 1.0;
            for (int k = 1; k < 256; ++k) {
                ra2assets::Rgba& c = p[k];
                c.r = static_cast<uint8_t>(std::min(255.0, c.r * f));
                c.g = static_cast<uint8_t>(std::min(255.0, c.g * f));
                c.b = static_cast<uint8_t>(std::min(255.0, c.b * f));
            }
            level_pals.push_back(p);
        }
        std::cerr << "[light] Ambient=" << ambient << " RGB=" << red << "," << green << "," << blue
                  << " Ground=" << ground << " Level=" << level << " -> I0=" << i0 << " tint=" << tint[0] << ","
                  << tint[1] << "," << tint[2] << "\n";
    }

    // --- W3-FIX-A light sources --------------------------------------------
    // Replace the live source set (cheap no-op when unchanged).
    void set_sources(std::vector<Src> s, const ra2assets::MapFile& map) {
        uint64_t sig = 1469598103934665603ull ^ s.size();
        for (const Src& q : s)
            for (double v : {q.x, q.y, (double)q.vis, (double)q.intensity, (double)q.r, (double)q.g, (double)q.b})
                sig = (sig ^ static_cast<uint64_t>(static_cast<int64_t>(v * 16))) * 1099511628211ull;
        if (sig == src_sig_ && srcs_.size() == s.size()) return;
        src_sig_ = sig;
        srcs_ = std::move(s);
        rebuild(map);
    }
    const std::vector<LitCell>& lit_cells() const { return lit_; }
    std::size_t source_count() const { return srcs_.size(); }
    // The MOD colour (0..255 per channel) of a cell given its summed source
    // contributions (per mille), at ground height (terrain height is already
    // in the per-level tile palettes).
    // W5-PRES R-104: light above 1.0 brightens (cap 2.0x) [RE: cell I clamp
    // 0..2000 at 0x484590, LightConvert 53 shades 0..2.0x at 0x555D20]. `out`
    // is the MOD part (<= 1.0), `extra` the MUL part ((v - 1.0) * 255).
    void cell_colour(double add_i, double add_r, double add_g, double add_b, Uint8 out[3],
                     Uint8* extra = nullptr) const {
        double i = std::min(ambient * 1000.0 + add_i - ground * 1000.0, 2000.0);
        double r = std::clamp(red * 1000.0 + add_r, 0.0, 2000.0), g = std::clamp(green * 1000.0 + add_g, 0.0, 2000.0),
               b = std::clamp(blue * 1000.0 + add_b, 0.0, 2000.0);
        double sc = 1.0;
        if (!(r == 1000.0 && g == 1000.0 && b == 1000.0)) {
            const double mx = std::max({r, g, b});
            sc = mx / 1000.0;
            if (sc * 65536.0 < 0x42) { r = g = b = 1000.0; sc = 0.0; }
            else { r /= sc; g /= sc; b /= sc; }
        }
        i = std::clamp(i * sc, 0.0, 2000.0);
        const double t[3] = {r, g, b};
        for (int k = 0; k < 3; ++k) {
            const double v = std::clamp(i / 1000.0 * t[k] / 1000.0, 0.0, 2.0);
            out[k] = static_cast<Uint8>(std::min(1.0, v) * 255.0);
            if (extra) extra[k] = static_cast<Uint8>(std::max(0.0, v - 1.0) * 255.0);
        }
    }
    void rebuild(const ra2assets::MapFile& map) {
        lit_.clear();
        if (srcs_.empty()) return;
        struct Acc { double i = 0, r = 0, g = 0, b = 0; };
        std::map<long, Acc> acc;
        for (const Src& q : srcs_) {
            if (q.vis <= 0) continue;
            const int cx = static_cast<int>(q.x) / 256, cy = static_cast<int>(q.y) / 256;
            const int rad = q.vis / 256 + 1;
            for (int y = cy - rad; y <= cy + rad; ++y)
                for (int x = cx - rad; x <= cx + rad; ++x) {
                    if (x < 0 || y < 0 || !map.at(x, y)) continue;
                    const double dx = x * 256.0 + 128.0 - q.x, dy = y * 256.0 + 128.0 - q.y;
                    const unsigned d = static_cast<unsigned>(std::sqrt(dx * dx + dy * dy));
                    if (d > static_cast<unsigned>(q.vis)) continue;
                    // FUN_00479440: f = (vis*1000 - d*1000) / vis (unsigned), then
                    // each term (value * f) / 1000 with int truncation.
                    const unsigned f = (static_cast<unsigned>(q.vis) * 1000u - d * 1000u) / static_cast<unsigned>(q.vis);
                    Acc& a = acc[static_cast<long>(y) * 4096 + x];
                    a.i += (q.intensity * static_cast<int>(f)) / 1000;
                    a.r += (q.r * static_cast<int>(f)) / 1000;
                    a.g += (q.g * static_cast<int>(f)) / 1000;
                    a.b += (q.b * static_cast<int>(f)) / 1000;
                }
        }
        Uint8 base[3];
        cell_colour(0, 0, 0, 0, base);
        for (const auto& [k, a] : acc) {
            LitCell c;
            c.x = static_cast<int>(k % 4096);
            c.y = static_cast<int>(k / 4096);
            const ra2assets::MapCell* mc = map.at(c.x, c.y);
            c.level = mc ? mc->level : 0;
            cell_colour(a.i, a.r, a.g, a.b, c.c, c.e);
            if (c.c[0] == base[0] && c.c[1] == base[1] && c.c[2] == base[2] && !c.e[0] && !c.e[1] && !c.e[2]) continue;
            lit_.push_back(c);
        }
        std::cerr << "[light] sources=" << srcs_.size() << " lit-cells=" << lit_.size() << "\n";
    }

    // Ambient*tint (and the light sources' cells) multiplied over the playfield.
    // W5-PRES R-104: two maps -- MOD (the <= 1.0 part) then MUL with alpha 0
    // (dst*src + dst = dst*(1 + excess)) for the 1.0..2.0 brightening.
    void pass(SDL_Renderer* ren, int W, int H, const ra2engine::Camera& cam) {
        if (lit_.empty()) { pass(ren, W, H); return; }
        Uint8 base[3], bex[3];
        cell_colour(0, 0, 0, 0, base, bex);
        if (!lmap_ || lmap_ren_ != ren || lmap_w_ != W || lmap_h_ != H) {
            lmap_ = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
            xmap_ = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
            lmap_ren_ = ren;
            lmap_w_ = W;
            lmap_h_ = H;
        }
        if (!lmap_ || !xmap_) { pass(ren, W, H); return; }
        bool any_excess = bex[0] || bex[1] || bex[2];
        for (const LitCell& c : lit_) any_excess = any_excess || c.e[0] || c.e[1] || c.e[2];
        SDL_Texture* prev = SDL_GetRenderTarget(ren);
        for (int m = 0; m < (any_excess ? 2 : 1); ++m) {
            SDL_SetRenderTarget(ren, m == 0 ? lmap_ : xmap_);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            const Uint8* bc = m == 0 ? base : bex;
            SDL_SetRenderDrawColor(ren, bc[0], bc[1], bc[2], m == 0 ? 255 : 0);
            SDL_RenderClear(ren);
            std::vector<SDL_Vertex> v;
            std::vector<int> idx;
            for (const LitCell& c : lit_) {
                const ra2engine::ScreenPt p = ra2engine::cell_to_screen(c.x, c.y, c.level, 60, 30, 15);
                const float x0 = static_cast<float>(p.x + cam.x), y0 = static_cast<float>(p.y + cam.y);
                if (x0 + 60 < 0 || y0 + 30 < 0 || x0 > W || y0 > H) continue;
                const Uint8* cc = m == 0 ? c.c : c.e;
                const SDL_Color col{cc[0], cc[1], cc[2], static_cast<Uint8>(m == 0 ? 255 : 0)};
                const int b0 = static_cast<int>(v.size());
                v.push_back({{x0 + 30, y0}, col, {0, 0}});
                v.push_back({{x0 + 60, y0 + 15}, col, {0, 0}});
                v.push_back({{x0 + 30, y0 + 30}, col, {0, 0}});
                v.push_back({{x0, y0 + 15}, col, {0, 0}});
                for (int k : {0, 1, 2, 0, 2, 3}) idx.push_back(b0 + k);
            }
            if (!v.empty())
                SDL_RenderGeometry(ren, nullptr, v.data(), static_cast<int>(v.size()), idx.data(),
                                   static_cast<int>(idx.size()));
        }
        SDL_SetRenderTarget(ren, prev);
        const SDL_Rect all{0, 0, W, H};
        SDL_SetTextureBlendMode(lmap_, SDL_BLENDMODE_MOD);
        SDL_RenderCopy(ren, lmap_, nullptr, &all);
        if (any_excess) {
            SDL_SetTextureBlendMode(xmap_, SDL_BLENDMODE_MUL);
            SDL_RenderCopy(ren, xmap_, nullptr, &all);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    }

    // Level-0 ambient*tint multiply over the playfield (all world sprites);
    // W5-PRES R-104: the part above 1.0 brightens through a MUL fill.
    void pass(SDL_Renderer* ren, int W, int H) const {
        if (!active) return;
        const double i0 = intensity(0) / 1000.0;
        double v[3];
        for (int c = 0; c < 3; ++c) v[c] = std::clamp(i0 * tint[c] / 1000.0, 0.0, 2.0);
        const SDL_Rect all{0, 0, W, H};
        auto u8 = [](double x) { return static_cast<Uint8>(std::clamp(x, 0.0, 1.0) * 255.0); };
        if (v[0] < 1.0 || v[1] < 1.0 || v[2] < 1.0) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_MOD);
            SDL_SetRenderDrawColor(ren, u8(v[0]), u8(v[1]), u8(v[2]), 255);
            SDL_RenderFillRect(ren, &all);
        }
        if (v[0] > 1.0 || v[1] > 1.0 || v[2] > 1.0) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_MUL);
            SDL_SetRenderDrawColor(ren, u8(v[0] - 1.0), u8(v[1] - 1.0), u8(v[2] - 1.0), 0);
            SDL_RenderFillRect(ren, &all);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    }

    // W5-PRES R-104: an object's own light relative to the ground-level pass
    // [RE: cell I includes Level*height; a unit on a bridge adds Level*4 --
    // UnitClass::DrawIt 0x73BF87]: I(h) / I(0).
    double object_rel(int h) const {
        if (!active) return 1.0;
        const double i0 = intensity(0);
        return i0 > 0.0 ? intensity(h) / i0 : 1.0;
    }
};

PresLight* g_light = nullptr;   // GameScene only
