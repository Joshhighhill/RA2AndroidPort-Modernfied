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
// Light sources (LightVisibility/Intensity/*Tint) and ion storm / dominator
// lighting are not modelled [RE-FALLBACK: W2-GA-05 light-source pass open].
//
// Draw: terrain tiles are baked per height level (draw_terrain's level
// palettes, factor I(h)/I(0)); everything in the world then gets the level-0
// ambient*tint as one MOD-blend multiply over the playfield before the shroud
// pass (objects take the ground-level light; their per-height brightening is
// not modelled).

struct PresLight {
    bool active = false;          // false = neutral lighting, nothing to do
    double ambient = 1.0, red = 1.0, green = 1.0, blue = 1.0, ground = 0.0, level = 0.0;
    double tint[3] = {1000, 1000, 1000};
    double scale = 1.0;           // normalisation factor applied to I
    std::vector<ra2assets::Palette> level_pals;   // terrain palette per height 0..15

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

    // Level-0 ambient*tint multiply over the playfield (all world sprites).
    void pass(SDL_Renderer* ren, int W, int H) const {
        if (!active) return;
        const double i0 = intensity(0) / 1000.0;
        auto ch = [&](int c) { return static_cast<Uint8>(std::clamp(i0 * tint[c] / 1000.0 * 255.0, 0.0, 255.0)); };
        const Uint8 r = ch(0), g = ch(1), b = ch(2);
        if (r == 255 && g == 255 && b == 255) return;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_MOD);
        SDL_SetRenderDrawColor(ren, r, g, b, 255);
        const SDL_Rect all{0, 0, W, H};
        SDL_RenderFillRect(ren, &all);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    }
};

const PresLight* g_light = nullptr;   // GameScene only
