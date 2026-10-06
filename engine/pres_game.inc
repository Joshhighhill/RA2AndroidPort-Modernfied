// pres_game.inc -- the GameScene-facing presentation driver (G-80/G-81/G-82 +
// effects/visual hooks). GameScene owns one `Presentation` and calls
// init()/update()/the render hooks; everything is observation of World /
// queue state (edge detection), so the simulation and every gate hash are
// untouched. Included by main.cpp after pres_audio.inc / pres_fx.inc.

class Presentation {
public:
    // side_ix: 0 Allied, 1 Soviet (Russian clips), 2 Yuri.
    void init(const std::string& dir, AudioSystem& audio, bool audio_on, const Rules& rules,
              const Ini& rules_ini, const std::vector<const std::vector<ra2engine::UnitTypeDef>*>& defs,
              int side_ix) {
        audio_ = &audio;
        rules_ini_ = &rules_ini;
        snd_.open(dir, &audio, audio_on || g_pres_log_on);
        snd_.set_side(side_ix);
        // [G-81] Every producible / AI / crew unit gets its voice, death and
        // primary-weapon Report keys, keyed by image like the existing table.
        for (const auto* list : defs)
            for (const ra2engine::UnitTypeDef& d : *list) add_voices(rules, d.id, d.combat.image);
        for (const char* id : {"E1", "E2", "HTNK", "HARV", "CMIN"}) add_voices(rules, id, rules.image(id));
        auto av = [&](const char* k) { return pres_clean(rules_ini.get("AudioVisual", k)); };
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
    }

    PresAudio& sound() { return snd_; }

    // Per-frame. `credits` = the player's wallet after this frame's production.
    void update(double dt, const ra2engine::World& w, const ra2engine::ProductionQueues& q,
                const BuildingAssets& ba, double credits, const ra2engine::InputActions& act,
                const ra2engine::Camera& cam, int view_w, int view_h,
                const std::vector<ra2engine::World::VetEvent>& vet) {
        ++frame_;
        clock_ += dt;
        snd_.set_frame(frame_);
        cam_ = cam;
        vw_ = view_w;
        vh_ = view_h;
        production_eva(q, credits);
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

    // [G-80] Production lines, observed from the four queue slots.
    // [RE-FALLBACK: RE-P1 pending -- trigger points per the evamd.ini line
    // names: Building/Training on start, ConstructionComplete when a structure
    // finishes, UnitReady when a finished unit leaves its factory, Canceled
    // when an unfinished slot empties.]
    void production_eva(const ra2engine::ProductionQueues& q, double credits) {
        for (int c = 0; c < 4; ++c) {
            const ra2engine::QueueCat cat = static_cast<ra2engine::QueueCat>(c);
            const ra2engine::QueueSlot& s = q.state(cat).slot;
            Slot& p = slots_[c];
            const bool structure = c < 2;
            if (p.type < 0 && s.type >= 0) snd_.eva(structure ? "EVA_Building" : "EVA_Training");
            if (!p.ready && s.ready && structure) snd_.eva("EVA_ConstructionComplete");
            if (p.type >= 0 && s.type != p.type) {
                if (p.ready && !structure) snd_.eva("EVA_UnitReady");
                else if (!p.ready) snd_.eva("EVA_Canceled");
            }
            // Insufficient funds: production stalls on an empty wallet.
            // [RE-FALLBACK: RE-P1 pending] once per stall.
            const bool stalled = s.type >= 0 && !s.ready && credits < 1.0;
            if (stalled && !p.stalled) snd_.eva("EVA_InsufficientFunds");
            p.type = s.type;
            p.ready = s.ready;
            p.stalled = stalled;
        }
    }

    // [G-80] Low power on the drain>output edge (construction.md §8 rule).
    void power_eva(const ra2engine::World& w, const BuildingAssets& ba) {
        if (!ba.ok) return;
        const ra2engine::PowerTotals pt = ra2engine::power_totals(w.buildings(), ba.roster, 0);
        const bool low = pt.drain > pt.output;
        if (low && !low_power_) snd_.eva("EVA_LowPower");
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
                // [RE-FALLBACK: RE-P1 pending -- BaseUnderAttack throttle] one
                // line per kBaseAttackGap seconds.
                if (clock_ >= next_base_attack_) {
                    snd_.eva("EVA_OurBaseIsUnderAttack");
                    snd_.play(base_attack_siren_, 0.0, "base under attack");
                    next_base_attack_ = clock_ + kBaseAttackGap;
                }
                const double half = 0.5 * b.max_hp;
                if (p.hp > half && b.hp <= half) snd_.play(building_damage_, dist_of(cx, cy), "half strength");
            }
            if (dead && !p.dead && !b.sold) snd_.play(building_die_, dist_of(cx, cy), "building die");
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
            if (u.fire_flash > prev_[i].first && u.cargo_of < 0) snd_.play(v.report, dist, "report");
            if (u.die_time >= 0.0 && prev_[i].second < 0.0) {
                snd_.play(v.die, dist, "die");
                if (u.house == 0) snd_.eva("EVA_UnitLost");
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

    struct Slot { int type = -1; bool ready = false; bool stalled = false; };
    struct Bld { bool seen = false; int hp = 0; int house = -1; bool dead = false; bool selling = false; };
    // [RE-FALLBACK: RE-P1 pending] seconds between "base under attack" lines.
    static constexpr double kBaseAttackGap = 30.0;

    PresAudio snd_;
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
    double next_base_attack_ = 0.0;
    std::string base_attack_siren_, building_die_, building_damage_, sell_sound_, vet_sound_,
        elite_sound_;
};
