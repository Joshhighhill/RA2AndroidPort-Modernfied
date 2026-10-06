// pres_game.inc -- the GameScene-facing presentation driver (G-80/G-81/G-82 +
// effects/visual hooks). GameScene owns one `Presentation` and calls
// init()/update()/the render hooks; everything is observation of World /
// queue state (edge detection), so the simulation and every gate hash are
// untouched. Included by main.cpp after pres_audio.inc / pres_fx.inc.

class Presentation {
public:
    // side_ix: 0 Allied, 1 Soviet (Russian clips), 2 Yuri.
    void init(const std::string& dir, const ra2assets::MixFile& ra2, AudioSystem& audio, bool audio_on,
              const Rules& rules, const Ini& rules_ini, const Ini& art_ini,
              const std::vector<const std::vector<ra2engine::UnitTypeDef>*>& defs, int side_ix) {
        audio_ = &audio;
        // G-83: the effects layer (muzzle / impact / death anims, projectile art).
        fx_.init(dir, ra2, rules_ini, art_ini);
        for (const auto* list : defs)
            for (const ra2engine::UnitTypeDef& d : *list) fx_.add_unit(d.id, d.combat.image);
        for (const char* id : {"E1", "E2", "HTNK", "HARV", "CMIN"}) fx_.add_unit(id, rules.image(id));
        g_fx = &fx_;
        rules_ini_ = &rules_ini;
        snd_.open(dir, &audio, audio_on || g_pres_log_on);
        snd_.set_side(side_ix);
        // [G-81] Every producible / AI / crew unit gets its voice, death and
        // primary-weapon Report keys, keyed by image like the existing table.
        for (const auto* list : defs)
            for (const ra2engine::UnitTypeDef& d : *list) add_voices(rules, d.id, d.combat.image);
        for (const char* id : {"E1", "E2", "HTNK", "HARV", "CMIN"}) add_voices(rules, id, rules.image(id));
        auto av = [&](const char* k) { return pres_clean(rules_ini.get("AudioVisual", k)); };
        // G-88: health-bar colours from [AudioVisual] ConditionYellow/Red ("50%").
        auto pct = [&](const char* k, double d) {
            const std::string v = av(k);
            return v.empty() ? d : std::atof(v.c_str()) / 100.0;
        };
        ra2engine::set_health_conditions(pct("ConditionYellow", 0.5), pct("ConditionRed", 0.25));
        // RadarEvent table overrides + EVA repeat delay ([General]).
        {
            auto gen = [&](const char* k) { return pres_clean(rules_ini.get("General", k)); };
            std::stringstream ds(gen("RadarEventSuppressionDistances")), us(gen("RadarEventDurations"));
            std::string tok;
            for (int i = 0; i < 6 && std::getline(ds, tok, ','); ++i) radar_dist_[i] = std::atof(tok.c_str());
            for (int i = 0; i < 6 && std::getline(us, tok, ','); ++i) radar_dur_[i] = std::atoi(tok.c_str());
            const std::string sd = gen("SpeakDelay");
            if (!sd.empty()) speak_delay_frames_ = static_cast<long>(std::atof(sd.c_str()) * 900.0);
        }
        base_attack_siren_ = av("BaseUnderAttackSound");
        building_die_ = av("BuildingDieSound");
        building_damage_ = av("BuildingDamageSound");
        sell_sound_ = av("SellSound");
        vet_sound_ = av("UpgradeVeteranSound");
        elite_sound_ = av("UpgradeEliteSound");
        for (const std::string& k : {base_attack_siren_, building_die_, building_damage_, sell_sound_,
                                     vet_sound_, elite_sound_})
            snd_.ensure_sound(k);
        snd_.music_start();
        // G-84: SHROUD.SHP (conquer.mix) for the shroud/fog post-pass.
        try {
            MixFile conquer = ra2.open_nested("conquer.mix");
            if (const MixEntry* e = conquer.find(std::string("shroud.shp"))) {
                shroud_ = std::make_unique<ShpFile>(ShpFile::load(conquer.read(*e)));
                g_shroud_shp = shroud_.get();
            }
            if (const MixEntry* e = conquer.find(std::string("fog.shp"))) {
                fog_ = std::make_unique<ShpFile>(ShpFile::load(conquer.read(*e)));
                g_fog_shp = fog_.get();
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pres] shroud.shp unavailable: " << ex.what() << "\n";
        }
    }
    ~Presentation() {
        if (g_shroud_shp == shroud_.get()) g_shroud_shp = nullptr;
        if (g_fog_shp == fog_.get()) g_fog_shp = nullptr;
        if (g_fx == &fx_) g_fx = nullptr;
    }

    PresAudio& sound() { return snd_; }

    // Per-frame. `credits` = the player's wallet after this frame's production.
    void update(double dt, const ra2engine::World& w, const ra2engine::ProductionQueues& q,
                const BuildingAssets& ba, double credits, const ra2engine::InputActions& act,
                const ra2engine::Camera& cam, int view_w, int view_h,
                const std::vector<ra2engine::World::VetEvent>& vet,
                const std::vector<ra2engine::ProjectileEvent>& pev) {
        ++frame_;
        fx_.step(dt);
        for (const ra2engine::ProjectileEvent& e : pev)
            if (e.kind == ra2engine::ProjectileEvent::Detonate) {
                const double fx = e.x / 256.0, fy = e.y / 256.0;
                if (!w.visible_at(0, ra2engine::lepton_to_cell(e.x), ra2engine::lepton_to_cell(e.y))) continue;
                const ra2engine::Unit* a = (e.attacker >= 0 && e.attacker < static_cast<int>(w.units().size()))
                                               ? &w.units()[e.attacker] : nullptr;
                if (a) impact(*a, e.damage, fx, fy, level_at(w, fx, fy), e.z * 36.74 / 256.0);
            }
        clock_ += dt;
        snd_.set_frame(frame_);
        cam_ = cam;
        vw_ = view_w;
        vh_ = view_h;
        production_eva(w, q, credits);
        power_eva(w, ba);
        building_events(w, ba);
        unit_audio(w, act);
        for (const auto& e : vet) {
            if (e.unit < 0 || e.unit >= static_cast<int>(w.units().size())) continue;
            const ra2engine::Unit& u = w.units()[e.unit];
            if (u.house != 0) continue;
            snd_.play(e.new_rank >= 2 ? elite_sound_ : vet_sound_, dist_of(u.fx, u.fy), "promotion");
            snd_.eva("EVA_UnitPromoted");
        }
        superweapon_eva(w);
        snd_.pump(clock_);
        snd_.music_update(clock_);
        if (audio_ && audio_->enabled && audio_->mixer) audio_->out.pump(*audio_->mixer);
    }

    long frame() const { return frame_; }

private:
    // ---- G-08 (draw half) action cursors from MOUSE.SHP ---------------------
    // MouseCursorType ids (YRpp GeneralDefinitions.h) index the 86-entry
    // descriptor table at gamemd.exe 0x0082C038 {start, count, rate, hotX,
    // hotY} (dumped in D:\ra2-re\out\rs10_log.txt; repair-sell.md §D8 pinned
    // entries 30..35 against MOUSE.SHP). Hotspot codes: 0 = left/top,
    // 12345 = centre, 54321 = right/bottom.
    enum Cursor : int {
        kDefault = 0x00, kScrollN = 0x01, kNoScrollN = 0x09, kSelect = 0x11, kMove = 0x12,
        kNoMove = 0x13, kAttack = 0x14, kAttackOutOfRange = 0x15, kEnter = 0x19, kNoEnter = 0x1A,
        kDeploy = 0x1B, kNoDeploy = 0x1C, kNuke = 0x35, kIronCurtain = 0x39, kChronosphere = 0x3A,
        kLightningStorm = 0x32,
    };
    struct CursorDesc { int start, count, rate, hx, hy; };
    static const CursorDesc& cursor_desc(int id) {
        static const CursorDesc kTable[] = {
            {0, 1, 0, 0, 0},             // 0x00 Default
            {2, 1, 0, 12345, 0},         // 0x01 scroll N
            {3, 1, 0, 54321, 0},         // NE
            {4, 1, 0, 54321, 12345},     // E
            {5, 1, 0, 54321, 54321},     // SE
            {6, 1, 0, 12345, 54321},     // S
            {7, 1, 0, 0, 54321},         // SW
            {8, 1, 0, 0, 12345},         // W
            {9, 1, 0, 0, 0},             // NW
            {10, 1, 0, 12345, 0},        // 0x09 no-scroll N
            {11, 1, 0, 54321, 0},
            {12, 1, 0, 54321, 12345},
            {13, 1, 0, 54321, 54321},
            {14, 1, 0, 12345, 54321},
            {15, 1, 0, 0, 54321},
            {16, 1, 0, 0, 12345},
            {17, 1, 0, 0, 0},
            {18, 13, 4, 12345, 12345},   // 0x11 Select
            {31, 10, 4, 12345, 12345},   // 0x12 Move
            {41, 1, 0, 12345, 12345},    // 0x13 NoMove
            {53, 5, 4, 12345, 12345},    // 0x14 Attack
            {58, 5, 4, 12345, 12345},    // 0x15 AttackOutOfRange
            {68, 5, 4, 12345, 12345},    // 0x16 Protect
            {78, 10, 4, 12345, 12345},   // 0x17 DesolatorDeploy
            {88, 1, 0, 12345, 12345},    // 0x18
            {89, 10, 4, 12345, 12345},   // 0x19 Enter
            {99, 1, 0, 12345, 12345},    // 0x1A NoEnter
            {110, 9, 4, 12345, 12345},   // 0x1B Deploy
            {119, 1, 0, 12345, 12345},   // 0x1C NoDeploy
        };
        static const CursorDesc kNukeD{319, 10, 4, 12345, 12345};     // 0x35
        static const CursorDesc kIronD{394, 10, 4, 12345, 12345};     // 0x39 [table [57]]
        static const CursorDesc kChronoD{404, 9, 4, 12345, 12345};    // 0x3A
        static const CursorDesc kStormD{279, 20, 4, 12345, 12345};    // 0x32
        if (id >= 0 && id < static_cast<int>(sizeof kTable / sizeof kTable[0])) return kTable[id];
        if (id == kNuke) return kNukeD;
        if (id == kIronCurtain) return kIronD;
        if (id == kChronosphere) return kChronoD;
        if (id == kLightningStorm) return kStormD;
        return kTable[0];
    }

    // Fallback "what would a click do" probe [G-08: B owns the authoritative
    // cursor state; this mirrors the current Input order semantics so the
    // pointer never lies about them]. Returns a MouseCursorType id.
    int probe_cursor(const ra2engine::World& w, const ra2engine::Input& in, int mx, int my,
                     bool over_sidebar, int view_w, int win_w, int win_h) const {
        constexpr int kEdge = 24;   // GameScene's edge-scroll band
        int dx = (mx >= win_w - kEdge) - (mx <= kEdge), dy = (my >= win_h - kEdge) - (my <= kEdge);
        if (dx || dy) {
            static const int dir[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};   // [dy+1][dx+1]
            return kScrollN + dir[dy + 1][dx + 1];
        }
        if (over_sidebar || in.placement_active()) return kDefault;
        if (in.sw_targeting()) {
            const ra2engine::Superweapon* sw = w.sw_state(0);
            if (sw && sw->type == ra2engine::SuperweaponType::IronCurtain) return kIronCurtain;
            return kNuke;
        }
        // Object under the pointer.
        int hit = -1;
        const auto& us = w.units();
        for (int i = static_cast<int>(us.size()) - 1; i >= 0; --i) {
            const ra2engine::Unit& u = us[i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            if (u.house != 0 && !w.visible_at(0, u.cell_x, u.cell_y)) continue;
            const int gx = static_cast<int>(std::lround((u.fx - u.fy) * 30.0)) + cam_.x + 30;
            const int gy = static_cast<int>(std::lround((u.fx + u.fy) * 15.0 - u.level * 15)) + cam_.y + 15;
            const int hw = u.is_vehicle ? 22 : 10, up = u.is_vehicle ? 24 : 30;
            if (mx >= gx - hw && mx <= gx + hw && my >= gy - up && my <= gy + 6) { hit = i; break; }
        }
        bool any_sel = false;
        for (const ra2engine::Unit& u : us)
            if (u.selected && u.house == 0 && u.alive && u.die_time < 0.0) { any_sel = true; break; }
        if (hit >= 0) {
            const ra2engine::Unit& u = us[hit];
            if (u.house == 0 || !w.is_enemy(0, u.house)) return kSelect;
            if (!any_sel) return kDefault;
            // [RE: presentation.md §3 FUN_004aada0] Attack only when in range,
            // else AttackOutOfRange (measured from the lowest selected unit).
            for (const ra2engine::Unit& s : us)
                if (s.selected && s.house == 0 && s.alive && s.die_time < 0.0) {
                    const double d = std::hypot(s.fx - u.fx, s.fy - u.fy);
                    return d <= s.w_range ? kAttack : kAttackOutOfRange;
                }
            return kAttack;
        }
        const int bi = in.building_under(mx, my);
        if (bi >= 0 && bi < static_cast<int>(w.buildings().size())) {
            const ra2engine::Building& b = w.buildings()[bi];
            if (any_sel && b.house != 0 && w.is_enemy(0, b.house) && pres_building_seen(w, b)) return kAttack;
            if (b.house == 0) return any_sel ? kNoMove : kDefault;
        }
        if (!any_sel) return kDefault;
        if (mx >= view_w) return kDefault;
        // Ground: move where passable (or unexplored -- the original lets you
        // order into the shroud), no-move otherwise.
        int cx = 0, cy = 0;
        ra2engine::screen_to_cell(mx - cam_.x, my - cam_.y, 60, 30, cx, cy);
        if (!w.explored_at(0, cx, cy)) return kMove;
        const auto& pass = w.passable();
        const int gw = w.grid_w(), gh = w.grid_h();
        if (cx < 0 || cy < 0 || cx >= gw || cy >= gh) return kNoMove;
        return pass[static_cast<std::size_t>(cy) * gw + cx] ? kMove : kNoMove;
    }

public:
    // Draws the action cursor (repair/sell modes keep draw_mode_cursor).
    // Returns true when it drew, so the caller hides the OS pointer.
    bool draw_cursor(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ModeCursor& mc,
                     const ra2engine::World& w, const ra2engine::Input& in, int mx, int my,
                     bool over_sidebar, int view_w, int win_w, int win_h, std::uint64_t game_frame) {
        if (!mc.ok) return false;
        const int id = probe_cursor(w, in, mx, my, over_sidebar, view_w, win_w, win_h);
        if (id != last_cursor_) {
            pres_log(frame_, "cursor " + std::to_string(id));
            last_cursor_ = id;
        }
        const CursorDesc& d = cursor_desc(id);
        // [RE: presentation.md §3] one frame every Interval ticks of
        // timeGetTime()>>4 (16 ms), not game frames.
        (void)game_frame;
        const std::uint64_t tick = SDL_GetTicks() >> 4;
        int frame = d.start + (d.rate > 0 ? static_cast<int>((tick / d.rate) % d.count) : 0);
        if (frame >= mc.shp.count()) frame = 0;
        const ra2assets::ShpFile::Frame f = mc.shp.frame_info(frame);
        if (f.w <= 0 || f.h <= 0) return true;
        char key[32];
        std::snprintf(key, sizeof key, "mouse:%d", frame);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            std::vector<uint8_t> idx = mc.shp.decode_indexed(frame);
            tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, mc.pal));
        }
        auto hot = [](int code, int size) { return code == 12345 ? size / 2 : code == 54321 ? size - 1 : 0; };
        const int hx = hot(d.hx, mc.shp.width()), hy = hot(d.hy, mc.shp.height());
        SDL_Rect dst{mx - hx + f.x, my - hy + f.y, f.w, f.h};
        SDL_RenderCopy(ren, tex, nullptr, &dst);
        return true;
    }
    int cursor() const { return last_cursor_; }

private:
    int last_cursor_ = -1;
    // ---- G-83 effects -------------------------------------------------------
    static int level_at(const ra2engine::World&, double, double) { return 0; }
    // Warhead AnimList impact + the anim's Report=.
    // [RE-FALLBACK: RE-P5 open -- AnimList pick] index = damage / 25, clamped
    // to the list (bigger hits -> later, larger anims).
    void impact(const ra2engine::Unit& shooter, int damage, double fx, double fy, int level,
                double lift_px) {
        const FxWeapon* wp = fx_.weapon(shooter.image);
        if (!wp || wp->anim_list.empty()) return;
        const int n = static_cast<int>(wp->anim_list.size());
        const int ix = std::clamp(damage / 25, 0, n - 1);
        if (const FxAnimType* t = fx_.spawn(wp->anim_list[ix], fx, fy, level, lift_px))
            snd_.play(t->report, dist_of(fx, fy), "impact");
        pres_log(frame_, "fx impact " + wp->anim_list[ix]);
    }
    // Muzzle Anim= at the firing point; an instant (no flying projectile)
    // weapon also lands its impact on the target now.
    void fire_fx(const ra2engine::World& w, const ra2engine::Unit& u) {
        const FxWeapon* wp = fx_.weapon(u.image);
        if (!wp) return;
        if (!wp->muzzle.empty()) {
            std::string name = wp->muzzle.front();
            double ox = 0, lift = 0;
            if (u.is_vehicle) {
                // FLH muzzle point, the projection draw_vehicle_unit uses
                // (voxel-render.md §6).
                const double yaw = ra2engine::index_to_yaw(
                    ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings),
                    ra2engine::kVehicleFacings);
                const double fc = u.w_proj.flh[0] / 256.0, lc = u.w_proj.flh[1] / 256.0;
                const double mx = fc * std::cos(yaw) - lc * std::sin(yaw);
                const double my = fc * std::sin(yaw) + lc * std::cos(yaw);
                ox = (mx + my) * 30.0;
                lift = -((mx - my) * 15.0) + (u.w_proj.flh[2] / 256.0) * 36.74;
            } else {
                // Infantry: the 8 facing-ordered anims N,NE,E,SE,S,SW,W,NW;
                // Unit::facing is 0=N counter-clockwise (infantry-anim.md §4).
                if (wp->muzzle.size() >= 8) name = wp->muzzle[(8 - (u.facing & 7)) % 8];
                lift = 15.0;   // [RE-FALLBACK: infantry PrimaryFireFLH not applied]
            }
            fx_.spawn(name, u.fx, u.fy, u.level, lift, ox);
        }
        if (!u.w_proj.flies()) {
            const auto& us = w.units();
            if (u.target >= 0 && u.target < static_cast<int>(us.size())) {
                const ra2engine::Unit& t = us[u.target];
                impact(u, u.w_damage, t.fx, t.fy, t.level, t.is_vehicle ? 10.0 : 6.0);
            } else if (u.target_building >= 0 &&
                       u.target_building < static_cast<int>(w.buildings().size())) {
                const ra2engine::Building& b = w.buildings()[u.target_building];
                impact(u, u.w_damage, b.cx + (b.fw - 1) / 2.0, b.cy + (b.fh - 1) / 2.0, 0, 20.0);
            }
        }
    }
    // Building death: the type's Explosion= anims scattered over the
    // foundation [RE-FALLBACK: RE-P5 open -- count/placement; one per
    // foundation cell, cycling the list], each with its Report=.
    void building_explode(const ra2engine::World&, const ra2engine::Building& b, const std::string& id) {
        const auto* ex = fx_.explosions(id);
        if (!ex || ex->empty()) return;
        int k = 0;
        for (int j = 0; j < b.fh; ++j)
            for (int i = 0; i < b.fw; ++i, ++k) {
                const std::string& name = (*ex)[k % ex->size()];
                if (const FxAnimType* t = fx_.spawn(name, b.cx + i, b.cy + j, 0, 10.0 + 6.0 * (k % 3)))
                    if (k == 0) snd_.play(t->report, dist_of(b.cx + i, b.cy + j), "building explosion");
            }
    }

    void add_voices(const Rules& rules, const std::string& id, const std::string& image) {
        if (image.empty() || audio_->voices.count(image)) return;
        UnitVoices v;
        v.select = pres_clean(rules.voice_select(id));
        v.move = pres_clean(rules.voice_move(id));
        v.attack = pres_clean(rules.voice_attack(id));
        v.die = pres_clean(rules.die_sound(id));
        v.report = pres_clean(rules.weapon_report(pres_clean(rules.primary(id))));
        if (v.report.empty()) {
            // WeaponCount/Weapon1 units (SREF etc.) carry no Primary=.
            const std::string w1 = pres_clean(rules_ini_->get(id, "Weapon1"));
            if (!w1.empty()) v.report = pres_clean(rules.weapon_report(w1));
        }
        // Report= may list several sounds ("Report=A,B"): the game picks one;
        // the first is used here [RE-FALLBACK: multi-Report pick not traced].
        const std::size_t comma = v.report.find(',');
        if (comma != std::string::npos) v.report = pres_clean(v.report.substr(0, comma));
        audio_->voices[image] = v;
        for (const std::string& k : {v.select, v.move, v.attack, v.die, v.report}) snd_.ensure_sound(k);
    }

    double dist_of(double fx, double fy) const {
        // [RE: audio-playback.md "Decisions"] Chebyshev max(dx, 2|dy|) px from
        // the tactical view centre, /60 -> cells.
        const double vcx = vw_ / 2.0 - cam_.x, vcy = vh_ / 2.0 - cam_.y;
        const double sx = (fx - fy) * 30.0 - vcx, sy = (fx + fy) * 15.0 - vcy;
        return std::max(std::abs(sx), 2.0 * std::abs(sy)) / 60.0;
    }

    // [G-80] Production lines, observed from the four queue slots [RE:
    // presentation.md §1.3]: Building/Training when production starts
    // (FUN_006aa390), ConstructionComplete when a structure finishes
    // (FUN_006a81c0), UnitReady when a finished unit exits its factory
    // (FUN_004faf00, gated by RadarEvent 6 UnitProduced), Canceled when an
    // unfinished slot empties. InsufficientFunds (FUN_004f8350): local house,
    // money < 100 with production running, repeated every SpeakDelay.
    void production_eva(const ra2engine::World& w, const ra2engine::ProductionQueues& q, double credits) {
        bool producing = false;
        for (int c = 0; c < 4; ++c) {
            const ra2engine::QueueCat cat = static_cast<ra2engine::QueueCat>(c);
            const ra2engine::QueueSlot& s = q.state(cat).slot;
            Slot& p = slots_[c];
            const bool structure = c < 2;
            if (p.type < 0 && s.type >= 0) snd_.eva(structure ? "EVA_Building" : "EVA_Training");
            if (!p.ready && s.ready && structure) snd_.eva("EVA_ConstructionComplete");
            if (p.type >= 0 && s.type != p.type) {
                if (p.ready && !structure) {
                    const auto& us = w.units();
                    const bool ok = us.empty() || radar_event(6, us.back().fx, us.back().fy);
                    if (ok) snd_.eva("EVA_UnitReady");
                }
                else if (!p.ready) snd_.eva("EVA_Canceled");
            }
            if (s.type >= 0 && !s.ready) producing = true;
            p.type = s.type;
            p.ready = s.ready;
        }
        if (producing && credits < 100.0 && frame_ >= next_funds_frame_) {
            snd_.eva("EVA_InsufficientFunds");
            next_funds_frame_ = frame_ + speak_delay_frames_;
        }
    }

    // [G-80] Low power once per low-power episode (drain > output edge,
    // construction.md §8), only while the house owns a construction yard
    // [RE: presentation.md §1.3 -- 3-entry Rules list, probably BuildConst: Medium].
    void power_eva(const ra2engine::World& w, const BuildingAssets& ba) {
        if (!ba.ok) return;
        const ra2engine::PowerTotals pt = ra2engine::power_totals(w.buildings(), ba.roster, 0);
        const bool low = pt.drain > pt.output;
        bool yard = false;
        for (const ra2engine::Building& b : w.buildings())
            if (b.alive && b.house == 0 && b.type >= 0 && b.type < static_cast<int>(ba.roster.size()) &&
                ba.roster[b.type].conyard)
                yard = true;
        if (low && !low_power_ && yard) snd_.eva("EVA_LowPower");
        low_power_ = low;
    }

    // [G-80] Building-level lines + sounds: base under attack (throttled),
    // building lost/sold/captured, half-strength damage sound, die sound,
    // new construction options.
    void building_events(const ra2engine::World& w, const BuildingAssets& ba) {
        const auto& bs = w.buildings();
        if (bhp_.size() < bs.size()) bhp_.resize(bs.size(), Bld{});
        int buildable = 0;
        for (std::size_t i = 0; i < bs.size(); ++i) {
            const ra2engine::Building& b = bs[i];
            Bld& p = bhp_[i];
            const double cx = b.cx + (b.fw - 1) / 2.0, cy = b.cy + (b.fh - 1) / 2.0;
            if (!p.seen) {
                p = Bld{true, b.hp, b.house, b.state == ra2engine::Building::State::Dead, b.selling};
                continue;
            }
            const bool dead = b.state == ra2engine::Building::State::Dead;
            if (b.house == 0 && p.house == 0 && b.hp < p.hp && !dead) {
                // [RE: presentation.md §1.2 FUN_004f9200] throttled by
                // RadarEvent 3 BaseAttacked (8 cells, alive 400 frames); the
                // same event is the radar flash. Plays BaseUnderAttackSound.
                if (radar_event(3, cx, cy)) {
                    snd_.eva("EVA_OurBaseIsUnderAttack");
                    snd_.play(base_attack_siren_, 0.0, "base under attack");
                }
                const double half = 0.5 * b.max_hp;
                if (p.hp > half && b.hp <= half) snd_.play(building_damage_, dist_of(cx, cy), "half strength");
            }
            if (dead && !p.dead && !b.sold) {
                snd_.play(building_die_, dist_of(cx, cy), "building die");
                if (ba.ok && b.type >= 0 && b.type < static_cast<int>(ba.roster.size()) &&
                    pres_building_seen(w, b))
                    building_explode(w, b, ba.roster[b.type].id);
            }
            if (b.selling && !p.selling && b.house == 0) {
                snd_.play(sell_sound_, dist_of(cx, cy), "sell");
                snd_.eva("EVA_StructureSold");
            }
            if (b.house == 0 && p.house != 0 && !dead) snd_.eva("EVA_BuildingCaptured");
            p.hp = b.hp;
            p.house = b.house;
            p.dead = dead;
            p.selling = b.selling;
        }
        // New construction options: the count of roster types whose
        // prerequisites the player now meets grew.
        if (ba.ok) {
            for (int t = 0; t < static_cast<int>(ba.roster.size()); ++t)
                if (ba.roster[t].tech_level > 0 &&
                    ra2engine::prerequisites_met(ba.roster[t].prerequisite, bs, ba.roster, 0, ba.groups))
                    ++buildable;
            if (buildable_ >= 0 && buildable > buildable_) snd_.eva("EVA_NewConstructionOptions");
            buildable_ = buildable;
        }
    }

    // [G-81] Voices on the player's orders, weapon Report on every shot, death
    // sounds, EVA_UnitLost -- the former GameScene block, now for every unit.
    void unit_audio(const ra2engine::World& w, const ra2engine::InputActions& act) {
        const auto& units = w.units();
        prev_.resize(units.size(), {0.0, -1.0});
        for (std::size_t i = 0; i < units.size(); ++i) {
            const auto& u = units[i];
            const double dist = dist_of(u.fx, u.fy);
            const UnitVoices& v = audio_->voices[u.image];
            if (u.fire_flash > prev_[i].first && u.cargo_of < 0) {
                snd_.play(v.report, dist, "report");
                if (u.house == 0 || w.visible_at(0, u.cell_x, u.cell_y)) fire_fx(w, u);
            }
            if (u.die_time >= 0.0 && prev_[i].second < 0.0) {
                snd_.play(v.die, dist, "die");
                // Vehicle death: the Explosion= anim (drawn by draw_vehicle_unit)
                // carries its own Report= sound.
                if (u.is_vehicle && w.visible_at(0, u.cell_x, u.cell_y)) {
                    const auto* ex = fx_.explosions(u.image);
                    if (ex && !ex->empty())
                        if (const FxAnimType* t = fx_.type(ex->front())) snd_.play(t->report, dist, "explosion");
                }
                if (u.house == 0 && radar_event(7, u.fx, u.fy)) snd_.eva("EVA_UnitLost");
            }
            prev_[i] = {u.fire_flash, u.die_time};
        }
        if (act.actor >= 0 && act.actor < static_cast<int>(units.size())) {
            const auto& au = units[act.actor];
            const UnitVoices& av = audio_->voices[au.image];
            const double d = dist_of(au.fx, au.fy);
            if (act.selected) snd_.play(av.select, d, "select");
            else if (act.attack_ordered) snd_.play(av.attack, d, "attack order");
            else if (act.move_ordered) snd_.play(av.move, d, "move order");
        }
        if (act.repair_clicked) snd_.eva("EVA_Repairing");
    }

    void superweapon_eva(const ra2engine::World& w) {
        const ra2engine::Superweapon* sw = w.sw_state(0);
        const bool ready = sw && sw->ready;
        if (ready && !sw_ready_) {
            switch (sw->type) {
                case ra2engine::SuperweaponType::Nuke: snd_.eva("EVA_NuclearMissileReady"); break;
                case ra2engine::SuperweaponType::IronCurtain: snd_.eva("EVA_IronCurtainReady"); break;
                default: snd_.eva("EVA_ChronosphereReady"); break;
            }
        }
        if (!ready && sw_ready_ && sw) {
            if (sw->type == ra2engine::SuperweaponType::Nuke) snd_.eva("EVA_NuclearMissileLaunched");
        }
        sw_ready_ = ready;
    }

    struct Slot { int type = -1; bool ready = false; };
    struct Bld { bool seen = false; int hp = 0; int house = -1; bool dead = false; bool selling = false; };
    // RadarEvent::Create suppression [RE: presentation.md §1.2, FUN_0065f4b0]:
    // false while a live event of the same type lies within its distance.
    // {dist cells, duration frames}; entries 0..5 are overridden by [General]
    // RadarEventSuppressionDistances / RadarEventDurations (read in init).
    struct RadarEv { int type; double x, y; long until; };
    double radar_dist_[17] = {8, 8, 8, 8, 8, 6, 2, 8, 2, 5, 8, 8, 8, 8, 8, 8, 8};
    int radar_dur_[17] = {400, 400, 400, 600, 400, 400, 200, 200, 400, 400, 100, 200, 400, 5, 200, 400, 600};
    bool radar_supp_[17] = {1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1};
    std::vector<RadarEv> radar_;
    bool radar_event(int type, double x, double y) {
        // [Medium: removal point not read -- an event leaves at its duration.]
        radar_.erase(std::remove_if(radar_.begin(), radar_.end(),
                                    [&](const RadarEv& e) { return e.until <= frame_; }),
                     radar_.end());
        if (radar_supp_[type])
            for (const RadarEv& e : radar_)
                if (e.type == type &&
                    static_cast<int>(std::sqrt((e.x - x) * (e.x - x) + (e.y - y) * (e.y - y))) <
                        radar_dist_[type])
                    return false;
        radar_.push_back(RadarEv{type, x, y, frame_ + radar_dur_[type]});
        return true;
    }
    const std::vector<RadarEv>& radar_events() const { return radar_; }
    long next_funds_frame_ = 0;
    long speak_delay_frames_ = 1800;   // [General] SpeakDelay (minutes) x 900

    PresAudio snd_;
    PresFx fx_;
    std::unique_ptr<ShpFile> shroud_, fog_;
    AudioSystem* audio_ = nullptr;
    const Ini* rules_ini_ = nullptr;
    long frame_ = 0;
    double clock_ = 0.0;
    ra2engine::Camera cam_;
    int vw_ = 1280, vh_ = 720;
    Slot slots_[4];
    std::vector<Bld> bhp_;
    std::vector<std::pair<double, double>> prev_;
    bool low_power_ = false;
    bool sw_ready_ = false;
    int buildable_ = -1;
    std::string base_attack_siren_, building_die_, building_damage_, sell_sound_, vet_sound_,
        elite_sound_;
};
