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
        g_bld_events.clear();   // W2-GA-14 per-building anim state is per game
        g_pres_tick = 0;        // W3-B: the logic-tick anim clock restarts per game
        // G-83: the effects layer (muzzle / impact / death anims, projectile art).
        fx_.init(dir, ra2, rules_ini, art_ini);
        for (const auto* list : defs)
            for (const ra2engine::UnitTypeDef& d : *list) fx_.add_unit(d.id, d.combat.image);
        for (const char* id : {"E1", "E2", "HTNK", "HARV", "CMIN"}) fx_.add_unit(id, rules.image(id));
        g_fx = &fx_;
        // W3-FX: particles, debris, trails, wakes, twinkles (pres_effects.inc).
        effects_.init(fx_, rules_ini, ra2, dir);
        for (const auto* list : defs)
            for (const ra2engine::UnitTypeDef& d : *list) effects_.note_unit_id(d.combat.image, d.id);
        g_fx3 = &effects_;
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
            std::stringstream vs(gen("RadarEventVisibilityDurations"));   // W3-FX minimap square
            for (int i = 0; i < 6 && std::getline(vs, tok, ','); ++i) radar_vis_[i] = std::atoi(tok.c_str());
            auto avd = [&](const char* k, double& out) {
                const std::string v = av(k);
                if (!v.empty()) out = std::atof(v.c_str());
            };
            avd("RadarEventMinRadius", g_radar_min_r);
            avd("RadarEventSpeed", g_radar_speed);
            avd("RadarEventRotationSpeed", g_radar_rot);
            avd("RadarEventColorSpeed", g_radar_col);
            g_radar_pings.clear();
            const std::string sd = gen("SpeakDelay");
            if (!sd.empty()) speak_delay_frames_ = static_cast<long>(std::atof(sd.c_str()) * 900.0);
        }
        base_attack_siren_ = av("BaseUnderAttackSound");
        radar_on_ = av("RadarOn");
        radar_off_ = av("RadarOff");
        building_die_ = av("BuildingDieSound");
        building_damage_ = av("BuildingDamageSound");
        sell_sound_ = av("SellSound");
        vet_sound_ = av("UpgradeVeteranSound");
        elite_sound_ = av("UpgradeEliteSound");
        impact_water_ = av("ImpactWaterSound");
        snd_.ensure_sound(impact_water_);
        for (const std::string& k : {base_attack_siren_, building_die_, building_damage_, sell_sound_,
                                     vet_sound_, elite_sound_})
            snd_.ensure_sound(k);
        snd_.music_start();
        // G-88: 12metfnt.fnt for the HUD text (credits, banners).
        try {
            MixFile cache = ra2.open_nested("cache.mix");
            if (const MixEntry* fe = cache.find(std::string("12metfnt.fnt"))) {
                font_ = std::make_unique<ra2assets::Font>(ra2assets::load_fnt(cache.read(*fe)));
                // [RE-FALLBACK: HUD text colour not read] pale yellow ramp.
                font_pal_ = make_font_palette_tinted(255, 240, 170);
                g_game_font = font_.get();
                g_game_font_pal = &font_pal_;
            }
        } catch (const std::exception& ex) {
            std::cerr << "[pres] 12metfnt.fnt unavailable: " << ex.what() << "\n";
        }
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
        if (g_game_font == font_.get()) { g_game_font = nullptr; g_game_font_pal = nullptr; }
        if (g_fx == &fx_) g_fx = nullptr;
        if (g_fx3 == &effects_) g_fx3 = nullptr;
    }

    PresAudio& sound() { return snd_; }
    // W3-FX: the trigger visuals (campaign_visuals.inc) drive these.
    PresFx& fx() { return fx_; }
    PresEffects& effects() { return effects_; }
    // Action 55 / any caller: a radar event at a cell (the minimap flash).
    bool radar_ping(int type, double x, double y) {
        return radar_event(std::clamp(type, 0, 16), x, y);
    }
    struct RadarPing { int type; double x, y; long born, until; };
    // Live radar events for the minimap "zoom square" (pres_radar.inc).
    std::vector<RadarPing> radar_pings() const {
        std::vector<RadarPing> out;
        for (const RadarEv& e : radar_) out.push_back(RadarPing{e.type, e.x, e.y, e.born, e.until});
        return out;
    }

    // Per-frame. `credits` = the player's wallet after this frame's production.
    void update(double dt, const ra2engine::World& w, const ra2engine::ProductionQueues& q,
                const BuildingAssets& ba, double credits, const ra2engine::InputActions& act,
                const ra2engine::Camera& cam, int view_w, int view_h,
                const std::vector<ra2engine::World::VetEvent>& vet,
                const std::vector<ra2engine::ProjectileEvent>& pev) {
        ++frame_;
        // W3-B anim clock: g_pres_tick is the LOGIC tick count (logic_tick()
        // below, called once per fixed 15 Hz step), not this per-render-frame
        // counter -- every anim keyed on it is frame-rate independent.
        world_ = &w;
        // W3-FX: particles / debris / trails / wakes on the World-frame clock.
        if (ba.ok) effects_.set_unit_palette(&ba.unit_pal);
        effects_.update(w, ba);
        ambient_sounds(w, ba);   // W3-FX AmbientSound loops
        snd_.set_view(cam.x, cam.y, view_w, view_h);   // W2-GA-23 positional audio
        snd_.update_voices();
        projectile_fx(w, pev);   // events not already taken per tick (W3-B)
        clock_ += dt;
        snd_.set_frame(frame_);
        cam_ = cam;
        vw_ = view_w;
        vh_ = view_h;
        production_eva(w, q, credits);
        power_eva(w, ba);
        if (ba.ok) unit_pal_ = &ba.unit_pal;
        if (ba.ok) fx_.set_unit_palette(&ba.unit_pal, &w);   // W3-B AltPalette base
        unit_audio(w, act);
        w5_update(w, ba, act, credits);   // W5-E (pres_audio_events.inc)
        for (const auto& e : vet) {
            if (e.unit < 0 || e.unit >= static_cast<int>(w.units().size())) continue;
            const ra2engine::Unit& u = w.units()[e.unit];
            if (u.house != 0) continue;
            snd_.play_at(e.new_rank >= 2 ? elite_sound_ : vet_sound_, u.fx, u.fy, "promotion");
            snd_.eva("EVA_UnitPromoted");
        }
        superweapon_events(w);   // W2-AN-05/20: the World SW event feed (any house)
        // [RE: presentation.md §1.3 FUN_004fc800 / FUN_004fc9f0] end of game,
        // local player only, once.
        if (!end_spoken_) {
            if (w.house_defeated(0)) { snd_.eva("EVA_YouHaveLost"); end_spoken_ = true; }
            else if (w.winning_team() >= 0 && w.winning_team() == w.house_team(0)) {
                snd_.eva("EVA_YouAreVictorious");
                end_spoken_ = true;
            }
        }
        // G-09: radar power-up animation + RadarOn/RadarOff ([AudioVisual]).
        {
            const bool online = w.shroud_enabled() && house_owns_operational_radar(w, ba, 0);
            if (online && !radar_online_) {
                g_radar_online_since = g_pres_tick;
                snd_.play(radar_on_, 0.0, "radar on");
            } else if (!online && radar_online_) {
                g_radar_offline_since = g_pres_tick;   // W2-UI-09 power-down animation
                snd_.play(radar_off_, 0.0, "radar off");
            }
            radar_online_ = online;
        }
        // W3-FX: the live radar events for the minimap zoom square.
        g_radar_pings.clear();
        for (const RadarEv& e : radar_)
            if (e.until > frame_ && radar_vis_[e.type] > 0)
                g_radar_pings.push_back(RadarPingDraw{e.x, e.y, frame_ - e.born, radar_vis_[e.type]});
        snd_.pump(clock_);
        snd_.music_update(clock_);
        if (audio_ && audio_->enabled && audio_->mixer) audio_->out.pump(*audio_->mixer);
    }

    long frame() const { return frame_; }

    // W3-B [RE: presentation.md §5 -- AnimClass::AI steps once per LOGIC
    // frame; Rate stores 900/Rate game frames per anim frame]: GameScene calls
    // this once per fixed logic tick (inside its G-13 accumulator loop), so
    // every anim (effects, building slots, terrain, radar) advances with game
    // time -- at the original rate on a 60 Hz desktop and a 30 Hz phone alike,
    // and scaled by GameSpeed exactly as the original's frames are.
    void logic_tick(const ra2engine::World& w, const BuildingAssets& ba, double player_credits,
                    const std::vector<ra2engine::ProjectileEvent>& pev) {
        g_pres_tick = ++logic_;
        fx_.tick();
        projectile_fx(w, pev);   // W3-B: impacts on the tick they detonate
        if (audio_) unit_fx_tick(w);
        // W3-B: the building slot machine sees this tick's state and events.
        const auto& fe = w.pending_fire_events();
        std::vector<ra2engine::World::FireEvent> fresh;
        for (std::size_t i = std::min(fire_seen_, fe.size()); i < fe.size(); ++i) fresh.push_back(fe[i]);
        fire_seen_ = fe.size();
        defense_fire_tick(w, fresh);   // muzzle / beam / report on the tick of the shot
        pres_bld_anim_tick(w, ba, fx_, g_pres_tick, fresh, units_seen_, bld_seen_,
                           [&](int h) { return h == 0 ? player_credits : w.house_credits(h); });
        building_anim_events(w, ba);   // W2-GA-14 SW charge edges, per tick
        building_events(w, ba);        // damage EVA, death explosion + DestroyAnim, per tick (W3-B)
        w5_tick(w, ba);                // W5-E sounds observed per logic tick (pres_audio_events.inc)
        g_walk_rules = rules_ini_;
        pres_walk_tick(w, g_pres_tick); // W3-B walk stage (SHP vehicle frames, HVA frames)
    }
    std::size_t fire_seen_ = 0, bld_seen_ = 0;   // W3-B
    void projectile_fx(const ra2engine::World& w, const std::vector<ra2engine::ProjectileEvent>& pev) {
        for (const ra2engine::ProjectileEvent& e : pev)
            if (e.kind == ra2engine::ProjectileEvent::Detonate) {
                const double fx = e.x / 256.0, fy = e.y / 256.0;
                if (!w.object_visible_at(0, ra2engine::lepton_to_cell(e.x), ra2engine::lepton_to_cell(e.y))) continue;
                const ra2engine::Unit* a = (e.attacker >= 0 && e.attacker < static_cast<int>(w.units().size()))
                                               ? &w.units()[e.attacker] : nullptr;
                if (a) impact(*a, e.damage, fx, fy, level_at(w, fx, fy), e.z * 36.74 / 256.0);
            }
    }

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
    // W2-AN-04 [RE: presentation.md §3 ConvertAction: 0x14 Nuke -> 0x35,
    // 0x25 IronCurtain -> 0x39, 0x26 LightningStorm -> 0x32, 0x27/0x28 Chrono
    // -> 0x3A, 0x29 ParaDrop -> 0x2F]: the armed SW's targeting cursor.
    mutable int armed_sw_ = 0;
    static int sw_cursor(int armed, const ra2engine::World& w) {
        using T = ra2engine::SuperweaponType;
        T t = static_cast<T>(armed);
        if (t == T::None) {
            const ra2engine::Superweapon* sw = w.sw_state(0);
            if (sw) t = sw->type;
        }
        switch (t) {
            case T::IronCurtain: return kIronCurtain;
            case T::LightningStorm: return kLightningStorm;
            case T::ChronoSphere: case T::ChronoWarp: return kChronosphere;
            case T::ParaDrop: case T::AmerParaDrop: return kNuke;   // [0x2F frames not in the dumped table]
            default: return kNuke;
        }
    }
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
            {120, 9, 4, 12345, 12345},   // 0x1D
            {129, 10, 4, 12345, 12345},  // 0x1E Sell
            {139, 10, 4, 12345, 12345},  // 0x1F SellUnit
            {149, 1, 0, 12345, 12345},   // 0x20 NoSell
            {150, 20, 4, 12345, 12345},  // 0x21 Repair
            {170, 20, 4, 12345, 12345},  // 0x22 EngineerRepair
            {190, 1, 0, 12345, 12345},   // 0x23 NoRepair
        };
        static const CursorDesc kRallyD{269, 10, 0, 12345, 12345};   // 0x30 RallyPoint [table [48]]
        static const CursorDesc kNukeD{319, 10, 4, 12345, 12345};     // 0x35
        static const CursorDesc kIronD{394, 10, 4, 12345, 12345};     // 0x39 [table [57]]
        static const CursorDesc kChronoD{404, 9, 4, 12345, 12345};    // 0x3A
        static const CursorDesc kStormD{279, 20, 4, 12345, 12345};    // 0x32
        if (id >= 0 && id < static_cast<int>(sizeof kTable / sizeof kTable[0])) return kTable[id];
        if (id == kNuke) return kNukeD;
        if (id == kIronCurtain) return kIronD;
        if (id == kChronosphere) return kChronoD;
        if (id == kLightningStorm) return kStormD;
        if (id == 0x30) return kRallyD;
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
        if (in.sw_targeting()) return sw_cursor(in.sw_armed_type(), w);   // W2-AN-04
        // Object under the pointer.
        int hit = -1;
        const auto& us = w.units();
        for (int i = static_cast<int>(us.size()) - 1; i >= 0; --i) {
            const ra2engine::Unit& u = us[i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            if (u.house != 0 && !w.object_visible_at(0, u.cell_x, u.cell_y)) continue;
            const int gx = static_cast<int>(std::lround((u.fx - u.fy) * 30.0)) + cam_.x + 30;
            int gy = static_cast<int>(std::lround((u.fx + u.fy) * 15.0 - u.level * 15)) + cam_.y + 15;
            // W2-AN-24: the cursor tracks the RAISED sprite of an airborne unit.
            if (u.is_aircraft && u.altitude > 0.0) gy -= ra2engine::altitude_to_screen_dy(u.altitude);
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
        const int gw = w.grid_w(), gh = w.grid_h();
        if (cx < 0 || cy < 0 || cx >= gw || cy >= gh) return kNoMove;
        return w.selection_can_enter(0, cx, cy) ? kMove : kNoMove;   // W2-AN-09
    }

    // B's Input::Action (the authoritative "what would a click do") ->
    // MouseCursorType [RE: presentation.md §3, DisplayClass::ConvertAction
    // FUN_004aada0]: Attack shows AttackOutOfRange when the selection cannot
    // reach; over a shrouded cell Attack and Move both show Move.
    int cursor_for_action(ra2engine::Input::Action a, const ra2engine::World& w, int mx, int my) const {
        using A = ra2engine::Input::Action;
        int cx = 0, cy = 0;
        ra2engine::screen_to_cell(mx - cam_.x, my - cam_.y, 60, 30, cx, cy);
        const bool shrouded = !w.explored_at(0, cx, cy);
        switch (a) {
            case A::Select: case A::ToggleSelect: case A::SelectBuilding: case A::SetPrimary: return kSelect;
            case A::Move: return kMove;
            case A::NoMove: return kNoMove;
            case A::Attack: {
                if (shrouded) return kMove;
                for (const ra2engine::Unit& s : w.units())
                    if (s.selected && s.house == 0 && s.alive && s.die_time < 0.0)
                        return std::hypot(s.fx - cx, s.fy - cy) <= s.w_range + 0.5 ? kAttack : kAttackOutOfRange;
                return kAttack;
            }
            case A::Harvest: return kAttackOutOfRange;   // Action 6 Harvest -> 0x15
            case A::Enter: case A::Dock: case A::Capture: case A::Infiltrate: return kEnter;
            case A::EngineerRepair: return 0x22;
            case A::Deploy: return kDeploy;
            case A::Rally: return 0x30;
            case A::Sell: return 0x1E;
            case A::NoSell: return 0x20;
            case A::Repair: return 0x21;
            case A::NoRepair: return 0x23;
            case A::SuperWeapon: return sw_cursor(armed_sw_, w);   // W2-AN-04
            default: return kDefault;
        }
    }
public:
    // Draws the action cursor (repair/sell modes keep draw_mode_cursor).
    // Returns true when it drew, so the caller hides the OS pointer.
    bool draw_cursor(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ModeCursor& mc,
                     const ra2engine::World& w, const ra2engine::Input& in, int mx, int my,
                     bool over_sidebar, int view_w, int win_w, int win_h, std::uint64_t game_frame,
                     int action = -1) {
        if (!mc.ok) return false;
        armed_sw_ = in.sw_armed_type();
        int id = probe_cursor(w, in, mx, my, over_sidebar, view_w, win_w, win_h);
        // Edge-scroll arrows and the sidebar keep the probe's answer; the
        // playfield follows B's action_at when supplied.
        const bool scroll = id >= kScrollN && id < kNoScrollN;
        if (action >= 0 && !scroll && !over_sidebar && mx < view_w)
            id = cursor_for_action(static_cast<ra2engine::Input::Action>(action), w, mx, my);
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
    // G-83 / W2-AN-05/20 superweapon visuals + EVA, driven by the World's SW
    // event feed (World::sw_events, read by sequence number) so an AI launch
    // shows and sounds like the player's.
    //  * Launched: [General] NukeTakeOff=NUKETO at the silo; EVA
    //    EVA_NuclearMissileLaunched for EVERY player [RE: presentation.md §1,
    //    FUN_006cba10 @6cd208 -- no local-player check].
    //  * Detonated: [NUKE] AnimList=NUKEANIM at the target + its Report=.
    //  * Activated: IronCurtainInvokeAnim=IRONBLST / ChronoBlast=CHRONOFD +
    //    ChronoBlastDest=CHRONOTG / the storm clouds; EVA *Activated /
    //    LightningStormCreated for every player.
    //  * Granted to an enemy house: EVA_NuclearSiloDetected /
    //    IronCurtainDetected / ChronosphereDetected / WeatherDeviceReady [RE:
    //    FUN_00445ee0 -- played only when the placing house is not the player
    //    and not allied; SW types 0..3 only].
    //  * Ready (own): EVA_*Ready.
    //  * Bolt: WeatherConBolts=WCLBOLT1 + WeatherConBoltExplosion + LightningSounds.
    int sw_seen_ = 0;
    std::string gen_s(const char* k) const {
        std::string v = pres_clean(rules_ini_->get("General", k));
        if (v.empty()) v = pres_clean(rules_ini_->get("AudioVisual", k));
        const std::size_t c = v.find(',');
        return c == std::string::npos ? v : pres_clean(v.substr(0, c));
    }
    void superweapon_events(const ra2engine::World& w) {
        using T = ra2engine::SuperweaponType;
        using E = ra2engine::SwEvent;
        for (const ra2engine::SwEvent& e : w.sw_events()) {
            if (e.seq <= sw_seen_) continue;
            sw_seen_ = e.seq;
            const T t = static_cast<T>(e.type);
            const bool own = e.house == 0;
            const bool hostile = !own && w.is_enemy(0, e.house);
            switch (e.kind) {
                case E::Granted:
                    if (!hostile) break;
                    if (t == T::Nuke) snd_.eva("EVA_NuclearSiloDetected");
                    else if (t == T::IronCurtain) snd_.eva("EVA_IronCurtainDetected");
                    else if (t == T::ChronoSphere) snd_.eva("EVA_ChronosphereDetected");
                    else if (t == T::LightningStorm) snd_.eva("EVA_WeatherDeviceReady");
                    break;
                case E::Ready:
                    if (!own) break;
                    if (t == T::Nuke) snd_.eva("EVA_NuclearMissileReady");
                    else if (t == T::IronCurtain) snd_.eva("EVA_IronCurtainReady");
                    else if (t == T::ChronoSphere) snd_.eva("EVA_ChronosphereReady");
                    else if (t == T::LightningStorm) snd_.eva("EVA_LightningStormReady");
                    else if (t == T::ParaDrop || t == T::AmerParaDrop) snd_.eva("EVA_ReinforcementsReady");
                    break;
                case E::Launched:
                    fx_.spawn(gen_s("NukeTakeOff"), e.x2, e.y2, level_at(w, e.x2, e.y2), 20.0);
                    snd_.eva("EVA_NuclearMissileLaunched");
                    pres_log(frame_, "fx superweapon launch");
                    break;
                case E::Detonated: {
                    std::string wh = gen_s("NukeWarhead");
                    if (wh.empty()) wh = "NUKE";
                    std::string list = pres_clean(rules_ini_->get(pres_upper(wh), "AnimList"));
                    const std::size_t comma = list.find(',');
                    if (comma != std::string::npos) list = pres_clean(list.substr(0, comma));
                    if (const FxAnimType* ft = fx_.spawn(list, e.x, e.y, level_at(w, e.x, e.y), 0.0))
                        snd_.play_at(ft->report, e.x, e.y, "nuke detonation");
                    pres_log(frame_, "fx superweapon " + list);
                    break;
                }
                case E::Activated:
                    if (t == T::IronCurtain) {
                        fx_.spawn(gen_s("IronCurtainInvokeAnim"), e.x, e.y, level_at(w, e.x, e.y), 0.0);
                        snd_.eva("EVA_IronCurtainActivated");
                    } else if (t == T::ChronoSphere || t == T::ChronoWarp) {
                        fx_.spawn(gen_s("ChronoBlast"), e.x, e.y, level_at(w, e.x, e.y), 0.0);
                        fx_.spawn(gen_s("ChronoBlastDest"), e.x2, e.y2, level_at(w, e.x2, e.y2), 0.0);
                        snd_.eva("EVA_ChronosphereActivated");
                    } else if (t == T::LightningStorm) {
                        fx_.spawn(gen_s("WeatherConClouds"), e.x, e.y, level_at(w, e.x, e.y), 60.0);
                        snd_.eva("EVA_LightningStormCreated");
                    }
                    pres_log(frame_, std::string("fx superweapon activated ") + ra2engine::superweapon_type_name(t));
                    break;
                case E::Bolt:
                    if (const FxAnimType* ft = fx_.spawn(gen_s("WeatherConBolts"), e.x, e.y, level_at(w, e.x, e.y), 0.0))
                        snd_.play_at(ft->report, e.x, e.y, "bolt");
                    fx_.spawn(gen_s("WeatherConBoltExplosion"), e.x, e.y, level_at(w, e.x, e.y), 0.0);
                    snd_.play_at(gen_s("LightningSounds"), e.x, e.y, "bolt");
                    break;
                case E::Dropped:
                    if (own) snd_.eva("EVA_ReinforcementsHaveArrived");
                    break;
                default:
                    break;
            }
        }
    }
    // ---- G-83 effects -------------------------------------------------------
    // W4-FIX-POS: the terrain level of the cell holding cell-space point
    // (x, y) (was a stub returning 0: every FX anim on raised ground drew
    // 15 px/level too low). Cell (x, y)'s centre is the integer point.
    static int level_at(const ra2engine::World& w, double x, double y) {
        const ra2assets::MapCell* c =
            w.map().at(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
        return c ? static_cast<int>(c->level) : 0;
    }
    static int building_level(const ra2engine::World& w, const ra2engine::Building& b) {
        return level_at(w, b.cx, b.cy);   // a building flattens to its origin cell's level
    }
    // Warhead AnimList impact + the anim's Report=.
    // [RE-FALLBACK: RE-P5 open -- AnimList pick] index = damage / 25, clamped
    // to the list (bigger hits -> later, larger anims).
    void impact(const ra2engine::Unit& shooter, int damage, double fx, double fy, int level,
                double lift_px) {
        const FxWeapon* wp = fx_.weapon(shooter.image);
        if (!wp || wp->anim_list.empty()) return;
        // W3-FX [RE: FUN_0048a400 SelectDamageAnimation]: a Conventional
        // warhead landing low on a water cell shows SplashList[min(damage,
        // n*35-1)/35] instead of its AnimList.
        const std::vector<std::string>& splash = effects_.splash_list();
        if (wp->conventional && !splash.empty() && on_water(fx, fy) && lift_px < 30.0) {
            const int ns = static_cast<int>(splash.size());
            const std::string& sname = splash[static_cast<std::size_t>(std::min(damage, ns * 35 - 1) / 35)];
            if (const FxAnimType* st = fx_.spawn(sname, fx, fy, level, 0.0)) {
                if (!impact_water_.empty()) snd_.play_at(impact_water_, fx, fy, "impact water");
                else snd_.play_at(st->report, fx, fy, "splash");
                pres_log(frame_, "fx splash " + sname);
                effects_.note_splash();
                return;
            }
        }
        const int n = static_cast<int>(wp->anim_list.size());
        const int ix = std::clamp(damage / 25, 0, n - 1);
        const FxAnimType* t = fx_.spawn(wp->anim_list[ix], fx, fy, level, lift_px);
        // W2-GA-25: a hit on a water cell sounds [AudioVisual] ImpactWaterSound.
        if (on_water(fx, fy) && !impact_water_.empty()) snd_.play_at(impact_water_, fx, fy, "impact water");
        else if (t) snd_.play_at(t->report, fx, fy, "impact");
        pres_log(frame_, "fx impact " + wp->anim_list[ix]);
    }
    // W4: a RocketLocomotion rocket's explosion -- its block's warhead
    // AnimList (FUN_0048A400 pick: damage / 25, as impact()), at its height;
    // a splash when it comes down low on water.
    void rocket_fx(const ra2engine::World& w, const ra2engine::Unit& u) {
        const ra2engine::RocketBlock& blk = w.sw_params().rockets.block[static_cast<std::size_t>(
            std::clamp(u.an.t.rocket_kind < 0 ? 1 : u.an.t.rocket_kind, 0, 2))];
        const ra2engine::RocketWarhead& wh = ra2engine::rocket_warhead(blk, u.an.rocket_elite);
        const int damage = ra2engine::rocket_damage(blk, u.an.rocket_elite);
        const double lift = u.altitude * kFxLepPx;
        if (u.an.rocket_end == 2 || (u.alive && u.an.rocket_end == 0)) {
            // Shot down (killed this tick; World marks rocket_end next step): the ordinary death -- a random Explosion= entry, in the air.
            const auto* ex = fx_.explosions(u.image);
            if (ex && !ex->empty()) {
                const uint32_t h = (static_cast<uint32_t>(u.cell_x * 131 + u.cell_y) * 2654435761u) ^
                                   (static_cast<uint32_t>(g_pres_tick) * 40503u);
                if (const FxAnimType* t = fx_.spawn((*ex)[(h >> 7) % ex->size()], u.fx, u.fy, level_at(w, u.fx, u.fy), lift)) {
                    snd_.play_at(t->report, u.fx, u.fy, "rocket shot down");
                    ++explosions_;
                }
            }
            return;
        }
        const std::vector<std::string>& splash = effects_.splash_list();
        if (!splash.empty() && on_water(u.fx, u.fy) && lift < 30.0) {
            const int ns = static_cast<int>(splash.size());
            const std::string& sname = splash[static_cast<std::size_t>(std::min(damage, ns * 35 - 1) / 35)];
            if (const FxAnimType* st = fx_.spawn(sname, u.fx, u.fy, level_at(w, u.fx, u.fy), 0.0)) {
                snd_.play_at(st->report, u.fx, u.fy, "splash");
                effects_.note_splash();
                return;
            }
        }
        std::vector<std::string> list;
        {
            const std::string raw = pres_clean(rules_ini_->get(pres_upper(wh.id), "AnimList"));
            std::string cur;
            for (char c : raw + ",") {
                if (c == ',') {
                    const std::string t = pres_clean(cur);
                    if (!t.empty()) list.push_back(t);
                    cur.clear();
                } else {
                    cur += c;
                }
            }
        }
        if (list.empty()) return;
        const int ix = std::clamp(damage / 25, 0, static_cast<int>(list.size()) - 1);
        if (const FxAnimType* t = fx_.spawn(list[static_cast<std::size_t>(ix)], u.fx, u.fy, level_at(w, u.fx, u.fy), lift)) {
            snd_.play_at(t->report, u.fx, u.fy, "rocket explosion");
            ++explosions_;
        }
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
                // W3-B: 8 facing-ordered muzzle anims (N, NE .. NW, clockwise)
                // pick by the turret facing [RE-FALLBACK: the 8-way rounding of
                // the u16 facing as the SHP/voxel facing rule].
                if (wp->muzzle.size() >= 8)
                    name = wp->muzzle[static_cast<std::size_t>((((static_cast<int>(u.turret_facing) & 0xFFFF) >> 12) + 1 >> 1) & 7)];
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
        if (wp->bolt || wp->laser) {   // W2-GA-15 Tesla / Prism beams
            double tx = 0, ty = 0, tl = 0;
            int tlev = 0;
            if (target_point(w, u.target, u.target_building, tx, ty, tlev, tl))
                fx_.add_beam(u.fx, u.fy, u.level, u.is_vehicle ? 18.0 : 15.0, tx, ty, tlev, tl, *wp,
                             house_color(w, u.house));
        }
        if (!u.w_proj.flies()) {
            // W3-FX: an instant shot whose projectile art has a line trail.
            {
                double tx = 0, ty = 0, tl = 0;
                int tlev = 0;
                if (target_point(w, u.target, u.target_building, tx, ty, tlev, tl))
                    effects_.instant_line(effects_.unit_weapon(u.image, 0), u.fx * 256.0, u.fy * 256.0,
                                          u.level * 104.0 + 60.0, tx * 256.0, ty * 256.0, tlev * 104.0 + 40.0);
            }
            const auto& us = w.units();
            if (u.target >= 0 && u.target < static_cast<int>(us.size())) {
                const ra2engine::Unit& t = us[u.target];
                impact(u, u.w_damage, t.fx, t.fy, t.level, t.is_vehicle ? 10.0 : 6.0);
            } else if (u.target_building >= 0 &&
                       u.target_building < static_cast<int>(w.buildings().size())) {
                const ra2engine::Building& b = w.buildings()[u.target_building];
                impact(u, u.w_damage, b.cx + (b.fw - 1) / 2.0, b.cy + (b.fh - 1) / 2.0, building_level(w, b), 20.0);
            }
        }
    }
    bool target_point(const ra2engine::World& w, int tu, int tb, double& x, double& y, int& level,
                      double& lift) const {
        const auto& us = w.units();
        if (tu >= 0 && tu < static_cast<int>(us.size())) {
            const ra2engine::Unit& t = us[static_cast<std::size_t>(tu)];
            x = t.fx; y = t.fy; level = t.level;
            lift = t.is_vehicle ? 10.0 : 8.0;
            return true;
        }
        if (tb >= 0 && tb < static_cast<int>(w.buildings().size())) {
            const ra2engine::Building& b = w.buildings()[static_cast<std::size_t>(tb)];
            x = b.cx + (b.fw - 1) / 2.0; y = b.cy + (b.fh - 1) / 2.0; level = building_level(w, b);
            lift = 20.0;
            return true;
        }
        return false;
    }
    SDL_Color house_color(const ra2engine::World& w, int house) const {
        if (!unit_pal_) return SDL_Color{255, 0, 255, 255};
        const ra2assets::Rgba c = w.house_palette(*unit_pal_, house)[24];
        return SDL_Color{c.r, c.g, c.b, 255};
    }
    const ra2assets::Palette* unit_pal_ = nullptr;
    // Building death: the type's Explosion= anims scattered over the
    // foundation [RE-FALLBACK: RE-P5 open -- count/placement; one per
    // foundation cell, cycling the list], each with its Report=.
    void building_explode(const ra2engine::World& w, const ra2engine::Building& b, const std::string& id) {
        const auto* ex = fx_.explosions(id);
        if (!ex || ex->empty()) return;
        int k = 0;
        for (int j = 0; j < b.fh; ++j)
            for (int i = 0; i < b.fw; ++i, ++k) {
                const std::string& name = (*ex)[k % ex->size()];
                if (const FxAnimType* t = fx_.spawn(name, b.cx + i, b.cy + j, building_level(w, b), 10.0 + 6.0 * (k % 3)))
                    if (k == 0) snd_.play_at(t->report, b.cx + i, b.cy + j, "building explosion");
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
        // Report= may list several sounds ("Report=A,B"): the game picks one
        // per shot (W2-GA-24; uniform pick on the audio-local RNG, the same
        // RandomRanged rule the event variants use) -- see pick_report.
        // W2-GA-25: MoveSound (the vehicle move-start one-shot, e.g.
        // GrizzlyTankMoveStart: Control=random predelay) and CrushSound
        // (TankCrush on crushers, InfantrySquish on crushable infantry).
        v.move_sound = pres_clean(rules_ini_->get(id, "MoveSound"));
        v.crush = pres_clean(rules_ini_->get(id, "CrushSound"));
        add_w5_voices(id, v);   // W5-E (pres_audio_events.inc)
        audio_->voices[image] = v;
        for (const std::string& k : {v.select, v.move, v.attack, v.die, v.move_sound, v.crush}) snd_.ensure_sound(k);
        std::stringstream rs(v.report);
        for (std::string tok; std::getline(rs, tok, ',');) snd_.ensure_sound(pres_clean(tok));
    }

    std::string pick_report(const std::string& list) {
        if (list.find(',') == std::string::npos) return list;
        std::vector<std::string> opts;
        std::stringstream rs(list);
        for (std::string tok; std::getline(rs, tok, ',');)
            if (!pres_clean(tok).empty()) opts.push_back(pres_clean(tok));
        if (opts.empty()) return std::string();
        report_rng_ = report_rng_ * 1664525u + 1013904223u;
        return opts[(report_rng_ >> 8) % opts.size()];
    }
#include "pres_audio_events.inc"   // W5-E: the remaining unit/world sounds and EVA lines
    std::uint32_t report_rng_ = 0x51A7u;
    std::vector<uint8_t> moving_prev_;
    std::vector<int> uhp_prev_;
    std::string impact_water_;
    const ra2engine::World* world_ = nullptr;
    bool on_water(double fx, double fy) const {
        if (!world_) return false;
        const auto& wg = world_->grid_for(ra2engine::MovementDomain::Water);
        const int x = static_cast<int>(std::lround(fx)), y = static_cast<int>(std::lround(fy));
        if (wg.empty() || x < 0 || y < 0 || x >= world_->grid_w() || y >= world_->grid_h()) return false;
        return wg[static_cast<std::size_t>(y) * world_->grid_w() + x] != 0;
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
        g_pres_low_power = low;
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
            // W2-GA-26 [RE: FUN_004f9200 @4f93ce]: an ALLIED (non-local)
            // house's building attacked -> RadarEvent 16 AllyBaseAttacked ->
            // EVA_OurAllyIsUnderAttack plus the siren.
            if (b.house != 0 && b.house == p.house && b.house >= 0 && w.are_allied(0, b.house) &&
                b.hp < p.hp && !dead && radar_event(16, cx, cy)) {
                snd_.eva("EVA_OurAllyIsUnderAttack");
                snd_.play(base_attack_siren_, 0.0, "ally under attack");
            }
            if (b.house == 0 && p.house == 0 && b.hp < p.hp && !dead) {
                // [RE: presentation.md §1.2 FUN_004f9200] throttled by
                // RadarEvent 3 BaseAttacked (8 cells, alive 400 frames); the
                // same event is the radar flash. Plays BaseUnderAttackSound.
                if (radar_event(3, cx, cy)) {
                    snd_.eva("EVA_OurBaseIsUnderAttack");
                    snd_.play(base_attack_siren_, 0.0, "base under attack");
                }
                const double half = 0.5 * b.max_hp;
                if (p.hp > half && b.hp <= half) snd_.play_at(building_damage_, cx, cy, "half strength");
            }
            if (dead && !p.dead && !b.sold) {
                snd_.play_at(building_die_, cx, cy, "building die");
                if (ba.ok && b.type >= 0 && b.type < static_cast<int>(ba.roster.size()) &&
                    pres_building_seen(w, b)) {
                    building_explode(w, b, ba.roster[b.type].id);
                    // W3-FX: building debris (the same TechnoClass death path).
                    const ra2assets::MapCell* mc = w.map().at(b.cx, b.cy);
                    effects_.spawn_debris(ba.roster[b.type].id, (b.cx + (b.fw - 1) / 2.0) * 256.0,
                                          (b.cy + (b.fh - 1) / 2.0) * 256.0, (mc ? mc->level : 0) * 104.0, false);
                }
                // W3-B: DestroyAnim= ruins (landmarks) [RE: TechnoType read
                // FUN_00711510 "DestroyAnim"], drawn through the owner's convert.
                if (ba.ok && b.type >= 0 && b.type < static_cast<int>(ba.roster.size())) {
                    const std::string& da = fx_.building_rules(pres_upper(ba.roster[b.type].id)).destroy_anim;
                    // W4-FIX-POS [RE: 0x441C2C-0x441C9A new AnimClass(DestroyAnim,
                    // vtbl+0xAC, ...)]: the ruin shares the building's draw point
                    // (origin-cell top vertex = cell (cx-0.5, cy-0.5)'s centre) and
                    // level; ruin SHPs share the building canvas.
                    std::shared_ptr<ra2assets::Palette> own;   // W4: art Palette= (lib) for the ruin
                    if (static_cast<std::size_t>(b.type) < ba.art.size()) own = ba.art[static_cast<std::size_t>(b.type)].own_pal;
                    if (!da.empty()) fx_.spawn_hold(da, b.cx - 0.5, b.cy - 0.5, building_level(w, b), b.house, own);
                }
            }
            if (b.selling && !p.selling && b.house == 0) {
                snd_.play_at(sell_sound_, cx, cy, "sell");
                snd_.eva("EVA_StructureSold");
            }
            // (W5-E: capture lines moved to pres_audio_events.inc w5_tick.)
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

public:
    // Base-defense shots (gap fix C's World::take_fire_events; unit shots are
    // already observed through fire_flash in unit_audio): the weapon's Report=
    // sound and muzzle Anim= at the building's centre, when visible.
    // W3-B: GameScene drains the queue once per frame; the events were
    // already handled tick by tick (defense_fire_tick from logic_tick).
    void defense_fire(const ra2engine::World&, const std::vector<ra2engine::World::FireEvent>&) {
        fire_seen_ = 0;
    }
    void defense_fire_tick(const ra2engine::World& w, const std::vector<ra2engine::World::FireEvent>& evs) {
        const auto& bs = w.buildings();
        for (const auto& e : evs) {
            if (e.building < 0 || e.building >= static_cast<int>(bs.size()) || e.weapon.empty()) continue;
            const ra2engine::Building& b = bs[e.building];
            const double fx = b.cx + (b.fw - 1) / 2.0, fy = b.cy + (b.fh - 1) / 2.0;
            snd_.play_at(fx_.weapon_report(e.weapon), fx, fy, "defense report");
            if (b.house != 0 && !w.object_visible_at(0, b.cx, b.cy)) continue;
            // W2-GA-15: Tesla Coil bolts / Prism Tower beams from the tower top.
            const FxWeapon& bw = fx_.weapon_def(e.weapon);
            if (bw.bolt || bw.laser) {
                double tx = 0, ty = 0, tl = 0;
                int tlev = 0;
                if (target_point(w, e.target_unit, e.target_building, tx, ty, tlev, tl))
                    fx_.add_beam(fx, fy, building_level(w, b), 60.0, tx, ty, tlev, tl, bw, house_color(w, b.house));
            }
            const std::vector<std::string> m = fx_.weapon_muzzle(e.weapon);
            if (!m.empty()) fx_.spawn(m.front(), fx, fy, building_level(w, b), 20.0);   // [RE-FALLBACK: building FireFLH not applied]
        }
    }

private:
    // W3-B [RE: InfantryClass::ReceiveDamage FUN_00517db5 InfDeath switch;
    // sequence-finished FUN_00520810 Die1-5 case]: an armed infantry whose
    // kill picked a death ANIM (World set ia.vanish) gets it at the body:
    // 3 InfantryExplode, 4 FlamingInfantry, 5 ELECTRO (AnimTypes[1] = rules
    // [Animations] second entry), 6 InfantryHeadPop, 7 InfantryNuked,
    // 8 InfantryVirus, 10 InfantryBrute (0: nothing). A body that played
    // Die1/Die2 leaves a corpse when it tombstones: a random entry of the
    // type's DeadBodies= or, for humans, [General] DeadBodies=.
    std::vector<char> alive_prev_;
    uint32_t corpse_rng_ = 0xC0FFEEu;
public:
    int corpses_ = 0, death_anims_ = 0, explosions_ = 0;   // W3-B --play probe counters
private:
    void infantry_death_fx(const ra2engine::World& w, const ra2engine::Unit& u, bool was_living, bool was_alive) {
        if (!u.ia.armed || u.is_vehicle || u.is_aircraft || u.cargo_of >= 0) return;
        if (!w.object_visible_at(0, u.cell_x, u.cell_y) && u.house != 0) return;
        const int lvl = level_at(w, u.fx, u.fy);
        if (u.die_time >= 0.0 && was_living && u.ia.vanish) {
            static const char* kKey[11] = {nullptr, nullptr, nullptr, "InfantryExplode", "FlamingInfantry",
                                           nullptr, "InfantryHeadPop", "InfantryNuked", "InfantryVirus",
                                           nullptr, "InfantryBrute"};
            const int d = u.ia.death;
            std::string anim;
            if (d == 5) anim = "ELECTRO";
            else if (d >= 0 && d <= 10 && kKey[d]) {
                anim = pres_clean(rules_ini_->get("AudioVisual", kKey[d]));
                if (anim.empty()) anim = pres_clean(rules_ini_->get("General", kKey[d]));
            }
            if (!anim.empty())
                if (const FxAnimType* t = fx_.spawn(anim, u.fx, u.fy, lvl, 0.0, 0.0, u.house)) {
                    snd_.play_at(t->report, u.fx, u.fy, "infantry death anim");
                    ++death_anims_;
                }
            return;
        }
        if (!u.alive && was_alive && u.die_time >= 0.0 && !u.ia.vanish) {
            std::string list = pres_clean(rules_ini_->get(u.image, "DeadBodies"));
            const ra2engine::InfTable* it = w.inf_table(u.image);
            if (list.empty() && !(it && it->not_human))
                list = pres_clean(rules_ini_->get("General", "DeadBodies"));
            std::vector<std::string> v;
            std::stringstream ss(list);
            std::string tok;
            while (std::getline(ss, tok, ',')) { tok = pres_clean(tok); if (!tok.empty()) v.push_back(tok); }
            if (v.empty()) return;
            corpse_rng_ = corpse_rng_ * 1664525u + 1013904223u;
            if (fx_.spawn(v[(corpse_rng_ >> 8) % v.size()], u.fx, u.fy, lvl, 0.0)) ++corpses_;
        }
    }
    // W3-B: weapon Report + muzzle/impact anims on each shot and the death
    // sound/explosion, observed once per LOGIC tick (Presentation::logic_tick)
    // so an effect is born on the tick its event happened, whatever the
    // render rate (was per render frame inside unit_audio).
    void unit_fx_tick(const ra2engine::World& w) {
        const auto& units = w.units();
        prev_.resize(units.size(), {0.0, -1.0});
        alive_prev_.resize(units.size(), 1);
        for (std::size_t i = 0; i < units.size(); ++i) {
            const auto& u = units[i];
            const UnitVoices& v = audio_->voices[u.image];
            infantry_death_fx(w, u, prev_[i].second < 0.0, alive_prev_[i] != 0);   // W3-B
            alive_prev_[i] = u.alive ? 1 : 0;
            if (u.fire_flash > prev_[i].first && u.cargo_of < 0) {
                snd_.play_at(pick_report(v.report), u.fx, u.fy, "report");
                if (u.house == 0 || w.object_visible_at(0, u.cell_x, u.cell_y)) fire_fx(w, u);
            }
            if (u.die_time >= 0.0 && prev_[i].second < 0.0) {
                // W2-GA-25: crushed infantry (killed by an adjacent Crusher
                // vehicle that ran into its cell) squish instead of dying.
                const int k = u.vet.killed_by;
                const bool crushed = k >= 0 && k < static_cast<int>(units.size()) && u.crushable &&
                                     !u.is_vehicle && units[k].is_vehicle && units[k].crusher &&
                                     std::abs(units[k].fx - u.fx) <= 1.6 && std::abs(units[k].fy - u.fy) <= 1.6;
                if (crushed) {
                    snd_.play_at(v.crush, u.fx, u.fy, "crushed");
                    snd_.play_at(audio_->voices[units[k].image].crush, units[k].fx, units[k].fy, "crusher");
                } else {
                    snd_.play_at(v.die, u.fx, u.fy, "die");
                }
                // Vehicle death: the Explosion= anim (drawn by draw_vehicle_unit)
                // carries its own Report= sound.
                // W3-B [RE: presentation.md §5 FUN_007376c0]: a uniformly
                // random Explosion= entry, spawned here on the death tick.
                if (u.an.t.missile) {
                    // W4 [FUN_00662A70]: a rocket (detonated or shot down)
                    // explodes with its warhead's anim where it is -- in the
                    // air when it was hit in flight.
                    if (w.object_visible_at(0, u.cell_x, u.cell_y)) rocket_fx(w, u);
                } else if (u.is_vehicle && !u.an.crashing && !u.an.sinking &&
                    w.object_visible_at(0, u.cell_x, u.cell_y)) {
                    const auto* ex = fx_.explosions(u.image);
                    if (ex && !ex->empty()) {
                        const uint32_t h = (static_cast<uint32_t>(i) * 2654435761u) ^
                                           (static_cast<uint32_t>(g_pres_tick) * 40503u);
                        const std::string& name = (*ex)[(h >> 7) % ex->size()];
                        if (const FxAnimType* t = fx_.spawn(name, u.fx, u.fy, u.level, 0.0)) {
                            snd_.play_at(t->report, u.fx, u.fy, "explosion");
                            ++explosions_;
                        }
                    }
                    // W3-FX [RE: FUN_00700da0 death]: Min/MaxDebris debris
                    // (voxel DebrisTypes / DebrisAnims / MetallicDebris), none
                    // when low over water.
                    const double z = u.level * 104.0 + (u.is_aircraft ? u.altitude : 0.0);
                    effects_.spawn_debris(effects_.unit_id(u.image), u.fx * 256.0, u.fy * 256.0, z,
                                          on_water(u.fx, u.fy) && (!u.is_aircraft || u.altitude < 11.0));
                }
                if (u.house == 0 && radar_event(7, u.fx, u.fy)) snd_.eva("EVA_UnitLost");
            }
            prev_[i] = {u.fire_flash, u.die_time};
        }
    }
    // [G-81] Voices on the player's orders, weapon Report on every shot, death
    // sounds, EVA_UnitLost -- the former GameScene block, now for every unit.
    void unit_audio(const ra2engine::World& w, const ra2engine::InputActions& act) {
        const auto& units = w.units();
        moving_prev_.resize(units.size(), 0);
        uhp_prev_.resize(units.size(), -1);
        for (std::size_t i = 0; i < units.size(); ++i) {
            const auto& u = units[i];
            const UnitVoices& v = audio_->voices[u.image];
            // W2-GA-25: a vehicle that starts moving plays its MoveSound.
            const bool mv = u.alive && u.die_time < 0.0 && u.cargo_of < 0 && u.moving();
            if (mv && !moving_prev_[i] && u.is_vehicle) snd_.play_at(v.move_sound, u.fx, u.fy, "move start");
            moving_prev_[i] = mv ? 1 : 0;
            // W2-GA-26 [RE: presentation.md §1.3 FUN_00736d60]: the player's
            // harvester taking (non-lethal) damage -> RadarEvent 4
            // HarvesterAttacked (8 cells) -> EVA_OreMinerUnderAttack, no siren.
            if (u.house == 0 && u.is_harvester && u.alive && u.die_time < 0.0 && uhp_prev_[i] >= 0 &&
                u.hp < uhp_prev_[i] && u.hp > 0 && radar_event(4, u.fx, u.fy))
                snd_.eva("EVA_OreMinerUnderAttack");
            uhp_prev_[i] = u.hp;
        }
        if (act.actor >= 0 && act.actor < static_cast<int>(units.size())) {
            const auto& au = units[act.actor];
            const UnitVoices& av = audio_->voices[au.image];
            if (act.selected) snd_.play_at(av.select, au.fx, au.fy, "select");
            else if (act.attack_ordered || act.move_ordered) order_voice(act, au, av);   // W5-E ClickedMission
        }
        if (act.repair_clicked) snd_.eva("EVA_Repairing");
    }

    // W2-GA-26: each superweapon type speaks its own lines (eva.ini
    // [EVA_*]): the player's READY line per type (Weather Control no longer
    // says "Chronosphere ready"); the "Warning: ... detected" line when an
    // ENEMY house gains a superweapon (EVA_WeatherDeviceReady is the Weather
    // Control *detected* warning); and the launch / activation warning heard
    // by everyone when any house fires one. Types without a line (paradrop,
    // spy plane, ...) stay silent.
    static const char* sw_line(ra2engine::SuperweaponType t, int what) {   // 0 ready, 1 detected, 2 fired
        using T = ra2engine::SuperweaponType;
        static const char* nuke[3] = {"EVA_NuclearMissileReady", "EVA_NuclearSiloDetected", "EVA_NuclearMissileLaunched"};
        static const char* ic[3] = {"EVA_IronCurtainReady", "EVA_IronCurtainDetected", "EVA_IronCurtainActivated"};
        static const char* ls[3] = {"EVA_LightningStormReady", "EVA_WeatherDeviceReady", "EVA_LightningStormCreated"};
        static const char* cs[3] = {"EVA_ChronosphereReady", "EVA_ChronosphereDetected", "EVA_ChronosphereActivated"};
        switch (t) {
            case T::Nuke: return nuke[what];
            case T::IronCurtain: return ic[what];
            case T::LightningStorm: return ls[what];
            case T::ChronoSphere: return cs[what];
            default: return nullptr;
        }
    }
    void superweapon_eva(const ra2engine::World& w) {
        const int n = w.house_count();
        if (static_cast<int>(sw_prev_.size()) < n) sw_prev_.resize(static_cast<std::size_t>(n));
        for (int h = 0; h < n; ++h) {
            const ra2engine::Superweapon* sw = w.sw_state(h);
            SwSeen& p = sw_prev_[static_cast<std::size_t>(h)];
            const ra2engine::SuperweaponType t = sw ? sw->type : ra2engine::SuperweaponType::None;
            const bool ready = sw && sw->ready;
            if (t != ra2engine::SuperweaponType::None) {
                if (h == 0 && ready && !p.ready)
                    if (const char* l = sw_line(t, 0)) snd_.eva(l);
                if (h != 0 && p.type != t && w.is_enemy(0, h))
                    if (const char* l = sw_line(t, 1)) snd_.eva(l);
                // Fired: was ready, same type, charge restarted from zero.
                if (p.ready && !ready && p.type == t && sw->timer <= 0.5)
                    if (const char* l = sw_line(t, 2)) snd_.eva(l);
            }
            p.type = t;
            p.ready = ready;
        }
    }
    // W2-GA-14: start the factory door when a unit appears at a factory, and
    // the superweapon building's charge anims on READY / FIRE edges.
    void building_anim_events(const ra2engine::World& w, const BuildingAssets& ba) {
        if (!ba.ok) return;
        const auto& bs = w.buildings();
        auto image_of = [&](const ra2engine::Building& b) -> std::string {
            if (b.type < 0 || b.type >= static_cast<int>(ba.roster.size())) return std::string();
            const ra2engine::BuildingTypeDef& d = ba.roster[static_cast<std::size_t>(b.type)];
            return pres_upper(d.image.empty() ? d.id : d.image);
        };
        const int n = w.house_count();
        if (static_cast<int>(sw_anim_prev_.size()) < n) sw_anim_prev_.resize(static_cast<std::size_t>(n), 0);
        for (int h = 0; h < n; ++h) {
            const ra2engine::Superweapon* sw = w.sw_state(h);
            const int st = !sw || sw->type == ra2engine::SuperweaponType::None ? 0 : (sw->ready ? 2 : 1);
            int& prev = sw_anim_prev_[static_cast<std::size_t>(h)];
            if (st != prev && st != 0) {
                for (const ra2engine::Building& b : bs) {
                    if (!b.alive || b.house != h) continue;
                    const std::string img = image_of(b);
                    if (img.empty() || !fx_.building(img).slot[kBsSuper].t) continue;
                    BldEventState& e = g_bld_events[bld_event_key(b)];
                    if (st == 2) { e.super_phase = 0; e.super_start = g_pres_tick; }                    // ready: open
                    else if (prev == 2) { e.super_phase = 2; e.super_start = g_pres_tick; }             // fired: close
                    else if (e.super_phase < 0) { e.super_phase = 2; e.super_start = -1; }          // charging
                }
            }
            prev = st;
        }
    }
    // W3-FX AmbientSound: one looping positional voice per live object whose
    // type has AmbientSound= (units, buildings, map terrain objects such as
    // the ore mines' OreMineExtract), restarted when it ends, stopped when the
    // object dies. Keyed by kind + index ("u", "b", "t").
    struct Amb { std::string key; std::uint32_t h = 0; double x = 0, y = 0; bool seen = false; };
    std::map<std::string, Amb> amb_;
    std::map<std::string, std::string> amb_type_;   // type id -> AmbientSound ("" none)
    long amb_started_ = 0;
    const std::string& ambient_of(const std::string& id) {
        auto it = amb_type_.find(id);
        if (it != amb_type_.end()) return it->second;
        return amb_type_.emplace(id, effects_.rules_key(id, "AmbientSound")).first->second;
    }
    void ambient_sounds(const ra2engine::World& w, const BuildingAssets& ba) {
        for (auto& kv : amb_) kv.second.seen = false;
        auto keep = [&](const std::string& k, const std::string& snd, double x, double y) {
            if (snd.empty()) return;
            Amb& a = amb_[k];
            a.seen = true;
            a.x = x;
            a.y = y;
            if (a.key.empty()) {
                a.key = snd;
                a.h = snd_.loop_at(snd, x, y);
                ++amb_started_;
                pres_log(frame_, "sound " + snd + " (ambient " + k + ")");
            } else if (a.h && !snd_.update_loop(a.h, x, y)) {
                a.h = snd_.loop_at(snd, x, y);   // the controller keeps it going
            }
        };
        const auto& us = w.units();
        for (std::size_t i = 0; i < us.size(); ++i) {
            const ra2engine::Unit& u = us[i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            keep("u" + std::to_string(i), ambient_of(effects_.unit_id(u.image)), u.fx, u.fy);
        }
        const auto& bs = w.buildings();
        for (std::size_t i = 0; i < bs.size(); ++i) {
            const ra2engine::Building& b = bs[i];
            if (!b.alive || b.state == ra2engine::Building::State::Dead || b.type < 0 ||
                b.type >= static_cast<int>(ba.roster.size()))
                continue;
            keep("b" + std::to_string(i), ambient_of(ba.roster[static_cast<std::size_t>(b.type)].id),
                 b.cx + (b.fw - 1) / 2.0, b.cy + (b.fh - 1) / 2.0);
        }
        if (g_map_dressing) {
            const auto& os = g_map_dressing->objects();
            for (std::size_t i = 0; i < os.size(); ++i)
                if (os[i].key.rfind("terr:", 0) == 0)
                    keep("t" + std::to_string(i), ambient_of(os[i].key.substr(5)), os[i].x, os[i].y);
        }
        for (auto it = amb_.begin(); it != amb_.end();) {
            if (!it->second.seen) {
                snd_.stop_loop(it->second.h);
                it = amb_.erase(it);
            } else {
                ++it;
            }
        }
    }
public:
    long ambient_started() const { return amb_started_; }
    std::size_t ambient_live() const { return amb_.size(); }
private:
    std::size_t units_seen_ = 0;
    std::vector<int> sw_anim_prev_;
    struct SwSeen { ra2engine::SuperweaponType type = ra2engine::SuperweaponType::None; bool ready = false; };
    std::vector<SwSeen> sw_prev_;

    struct Slot { int type = -1; bool ready = false; };
    struct Bld { bool seen = false; int hp = 0; int house = -1; bool dead = false; bool selling = false; };
    // RadarEvent::Create suppression [RE: presentation.md §1.2, FUN_0065f4b0]:
    // false while a live event of the same type lies within its distance.
    // {dist cells, duration frames}; entries 0..5 are overridden by [General]
    // RadarEventSuppressionDistances / RadarEventDurations (read in init).
    struct RadarEv { int type; double x, y; long until; long born = 0; };
    double radar_dist_[17] = {8, 8, 8, 8, 8, 6, 2, 8, 2, 5, 8, 8, 8, 8, 8, 8, 8};
    int radar_dur_[17] = {400, 400, 400, 600, 400, 400, 200, 200, 400, 400, 100, 200, 400, 5, 200, 400, 600};
    int radar_vis_[17] = {200, 200, 200, 200, 200, 200, 0, 0, 0, 0, 0, 200, 200, 0, 0, 0, 200};
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
        radar_.push_back(RadarEv{type, x, y, frame_ + radar_dur_[type], frame_});
        return true;
    }
    const std::vector<RadarEv>& radar_events() const { return radar_; }
    long next_funds_frame_ = 0;
    long speak_delay_frames_ = 1800;   // [General] SpeakDelay (minutes) x 900

    PresAudio snd_;
    PresFx fx_;
    PresEffects effects_;   // W3-FX
    std::unique_ptr<ShpFile> shroud_, fog_;
    std::unique_ptr<ra2assets::Font> font_;
    ra2assets::Palette font_pal_;
    AudioSystem* audio_ = nullptr;
    const Ini* rules_ini_ = nullptr;
    long frame_ = 0;
    long logic_ = 0;   // W3-B: logic ticks since init (g_pres_tick)
    double clock_ = 0.0;
    ra2engine::Camera cam_;
    int vw_ = 1280, vh_ = 720;
    Slot slots_[4];
    std::vector<Bld> bhp_;
    std::vector<std::pair<double, double>> prev_;
    bool low_power_ = false;
    bool sw_ready_ = false;
    bool radar_online_ = false;
    bool end_spoken_ = false;
    std::string radar_on_, radar_off_;
    int buildable_ = -1;
    std::string base_attack_siren_, building_die_, building_damage_, sell_sound_, vet_sound_,
        elite_sound_;
};
