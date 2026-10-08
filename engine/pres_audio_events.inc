// pres_audio_events.inc -- W5-E (wave 5, agent E): the unit / world sounds and
// EVA lines that were never played (REMAINING R-130 / R-131), at their
// original trigger points. Included INSIDE class Presentation (pres_game.inc)
// so it shares snd_ / radar_event / rules_ini_; everything is observation of
// World / input state (edge detection) or an explicit GameScene note, so the
// simulation and every gate hash are untouched.
//
// RE (gamemd.exe YR 1.001, Ghidra copy D:\ra2-re\w5-audio, jobs j1..j22):
//   TechnoTypeClass::LoadFromINI FUN_00711510 stores VoiceSelect 0x414 (list),
//   VoiceMove 0x468, VoiceAttack 0x484, VoiceSpecialAttack 0x4A0 (list),
//   VoiceDie 0x4BC, VoiceFeedback 0x4D8 (list), CrashingSound 0x544,
//   SinkingSound 0x548, VoiceCrashing 0x550, VoiceSinking 0x554, VoiceEnter
//   0x558, VoiceCapture 0x55C, TurretRotateSound 0x560, DeploySound 0x56C,
//   UndeploySound 0x570, VoiceHarvest 0x57C, VoiceDeploy 0x590.
//   RulesClass FUN_00668C20: BuildingGarrisonedSound +0x1BC,
//   BuildingRepairedSound +0x1C4, CheerSound +0x1C8, CrateMoney/Reveal/Fire/
//   Armour/Speed/Unit/PromoteSound +0x1E4..+0x1FC, SinkingSound +0x208,
//   CreditTicks list +0x6CC.
//
//   * ClickedMission FUN_006FF0A0: the order voice by mission (input.hpp).
//   * TechnoClass::ReceiveDamage FUN_00700DA0 @701B20: on the NowYellow damage
//     state (health just crossed ConditionYellow) a player-controlled object
//     with VoiceFeedback= says a random entry 30% of the time
//     (Random(0,99) < 0x1E), positional.
//   * FootClass AI FUN_004DA440 @4DAB2E/@4DAC61: when an object starts sinking
//     it plays VoiceSinking then SinkingSound (the type's, else the Rules
//     one) on a looping controller at its coords; crashing likewise plays
//     VoiceCrashing then CrashingSound (no Rules fallback).
//   * TechnoClass::AI FUN_006F9380 @6F94D9: a type with TurretRotateSound keeps
//     it going (controller +0x488) while the turret is turning (+0x4A0) and
//     stops it when it stops.
//   * UnitClass deploy FUN_007383E0/@738A6C and InfantryClass deploy: the
//     type's DeploySound at the object; undeploy @738ECB/@51D792 UndeploySound.
//   * InfantryClass occupy FUN_00522640: the FIRST occupant of a player
//     building -> EVA_StructureGarrisoned + BuildingGarrisonedSound at it.
//   * FUN_004581F0: a garrison emptied back to neutral -> RadarEvent 15 ->
//     EVA_StructureAbandoned for the old owner.
//   * FUN_007008B0 (Repair_To_Full, engineer repair) -> BuildingRepairedSound.
//   * Crate pickup FUN_00481EAD.. (CellClass crate): per powerup the matching
//     Crate*Sound / HealCrateSound and, for the local player, the
//     EVA_Unit{Armor,Speed,FirePower}Upgraded line (@482DC6/@482FB5/@483195).
//   * Capture FUN_004481AB @448310: a tech building (type flag +0x1552) ->
//     EVA_TechBuildingLost for the losing player and the type's
//     CaptureEvaEvent (+0x1554) for the gaining player; any other building ->
//     RadarEvent 10 then EVA_BuildingCaptured when either side is the player.
//   * Bridge repair FUN_00519449 @5199E4 -> RadarEvent 14 -> EVA_BridgeRepaired
//     (+ Rules RepairBridgeSound).
//   * Service depot FUN_0044B780 @44B96E -> RadarEvent 8 (2 cells) ->
//     EVA_UnitRepaired.
//   * AllToCheer FUN_0050C6E0: every techno of the house cheers, then
//     FUN_0074F740(CheerSound, 1.0).
//   * CreditClass::AI FUN_004A2510 / draw FUN_004A2280: the shown credits move
//     |diff|>>3 (clamped 1..143) toward the real value per call; a moving
//     counter plays CreditTicks[0] (up) / [1] (down) at volume 0.5.
public:
    // ---- per-type data (add_voices) ------------------------------------------
    void add_w5_voices(const std::string& id, UnitVoices& v) {
        if (!rules_ini_) return;
        auto k = [&](const char* key) { return pres_clean(rules_ini_->get(id, key)); };
        v.feedback = k("VoiceFeedback");
        v.special = k("VoiceSpecialAttack");
        v.enter = k("VoiceEnter");
        v.capture = k("VoiceCapture");
        v.harvest = k("VoiceHarvest");
        v.deploy_voice = k("VoiceDeploy");
        v.deploy_snd = k("DeploySound");
        v.undeploy_snd = k("UndeploySound");
        v.sinking = k("SinkingSound");
        v.crashing = k("CrashingSound");
        v.voice_sinking = k("VoiceSinking");
        v.voice_crashing = k("VoiceCrashing");
        v.turret_rotate = k("TurretRotateSound");
        const std::string c4 = pres_lower(k("C4"));
        v.c4 = !c4.empty() && (c4[0] == 'y' || c4[0] == 't' || c4[0] == '1');
        for (const std::string* s : {&v.feedback, &v.special})
            for (const std::string& t : w5_split(*s)) snd_.ensure_sound(t);
        for (const std::string& s : {v.enter, v.capture, v.harvest, v.deploy_voice, v.deploy_snd, v.undeploy_snd,
                                     v.sinking, v.crashing, v.voice_sinking, v.voice_crashing, v.turret_rotate})
            snd_.ensure_sound(s);
    }

    // ---- GameScene notes -----------------------------------------------------
    // A UI-originated EVA line (cameo click / hotkey / mode), logged + queued.
    void note_eva(const std::string& name) { snd_.eva(name); }
    // DeployObject on the selection: Mission_Unload (16) -> VoiceDeploy
    // [RE: FUN_006FF0A0 / FUN_00708280].
    void note_deploy_order(const ra2engine::World& w, int unit) {
        if (unit < 0 || unit >= static_cast<int>(w.units().size()) || !audio_) return;
        const ra2engine::Unit& u = w.units()[static_cast<std::size_t>(unit)];
        const UnitVoices& v = audio_->voices[u.image];
        snd_.play_at(v.deploy_voice, u.fx, u.fy, "deploy order");
    }
    // AllToCheer [RE: FUN_0050C6E0]: CheerSound at full volume, non-positional.
    void note_cheer() {
        w5_init_once();
        snd_.play_scaled(w5_cheer_, 1.0, "cheer");
    }
    // [RE: FUN_004A2280] one credit-counter step this frame (up / down).
    void credit_tick(bool up) {
        w5_init_once();
        const std::string& k = up ? w5_credit_up_ : w5_credit_down_;
        if (w5_credit_ticks_ >= 2) snd_.play_scaled(k, 0.5, up ? "credits up" : "credits down");
    }
    // A building's power toggle [RE: FUN_00452350 / FUN_00452250].
    void note_power_toggle(bool now_online) {
        snd_.eva(now_online ? "EVA_BuildingOnLine" : "EVA_BuildingOffLine");
    }
    // World event feeds GameScene used to discard: crates, abilities (deploy),
    // garrisons, MCV deploys.
    void on_world_events(const ra2engine::World& w, const BuildingAssets& ba,
                         const std::vector<ra2engine::World::CrateEvent>& crates,
                         const std::vector<ra2engine::World::AbilityEvent>& abil,
                         const std::vector<ra2engine::World::GarrisonEvent>& gar) {
        w5_init_once();
        using P = ra2engine::Powerup;
        for (const auto& e : crates) {
            const double x = e.x, y = e.y;
            const bool own = e.house == 0;
            switch (static_cast<P>(e.applied)) {
                case P::Money: snd_.play_at(w5_crate_[0], x, y, "crate money"); break;
                case P::Reveal: snd_.play_at(w5_crate_[1], x, y, "crate reveal"); break;
                case P::Firepower:
                    snd_.play_at(w5_crate_[2], x, y, "crate firepower");
                    if (own) snd_.eva("EVA_UnitFirePowerUpgraded");
                    break;
                case P::Armor:
                    snd_.play_at(w5_crate_[3], x, y, "crate armor");
                    if (own) snd_.eva("EVA_UnitArmorUpgraded");
                    break;
                case P::Speed:
                    snd_.play_at(w5_crate_[4], x, y, "crate speed");
                    if (own) snd_.eva("EVA_UnitSpeedUpgraded");
                    break;
                case P::Unit: snd_.play_at(w5_crate_[5], x, y, "crate unit"); break;
                case P::Veteran: snd_.play_at(w5_crate_[6], x, y, "crate promote"); break;
                case P::HealBase: snd_.play_at(w5_heal_crate_, x, y, "crate heal"); break;
                default: break;   // explosion / napalm / ICBM ... carry their own anims
            }
        }
        for (const auto& e : abil) {
            if (e.kind != ra2engine::World::AbilityEvent::Kind::Deploy) continue;
            if (e.a < 0 || e.a >= static_cast<int>(w.units().size())) continue;
            const ra2engine::Unit& u = w.units()[static_cast<std::size_t>(e.a)];
            const UnitVoices& v = audio_->voices[u.image];
            if (e.x) snd_.play_at(v.deploy_snd, u.fx, u.fy, "deploy");
            else snd_.play_at(v.undeploy_snd, u.fx, u.fy, "undeploy");
        }
        const auto& bs = w.buildings();
        for (const auto& e : gar) {
            if (e.building < 0 || e.building >= static_cast<int>(bs.size())) continue;
            const ra2engine::Building& b = bs[static_cast<std::size_t>(e.building)];
            const double cx = b.cx + (b.fw - 1) / 2.0, cy = b.cy + (b.fh - 1) / 2.0;
            if (e.kind == ra2engine::World::GarrisonEvent::Garrisoned && e.house == 0) {
                snd_.eva("EVA_StructureGarrisoned");
                snd_.play_at(w5_garrisoned_, cx, cy, "garrisoned");
            } else if (e.kind == ra2engine::World::GarrisonEvent::Abandoned && e.house == 0 &&
                       radar_event(15, cx, cy)) {
                snd_.eva("EVA_StructureAbandoned");
            }
            w5_garrison_flip_[e.building] = frame_;   // not a capture (building_events)
        }
        (void)ba;
    }
    // gF MCV deploy: the unit type's DeploySound at the new building.
    void on_mcv_deployed(const ra2engine::World& w, const std::string& unit_image, int building) {
        if (!audio_ || building < 0 || building >= static_cast<int>(w.buildings().size())) return;
        const ra2engine::Building& b = w.buildings()[static_cast<std::size_t>(building)];
        snd_.play_at(audio_->voices[unit_image].deploy_snd, b.cx + (b.fw - 1) / 2.0, b.cy + (b.fh - 1) / 2.0,
                     "deploy");
    }
    // Probe counters (--play evidence).
    long w5_count(const std::string& what) const {
        auto it = w5_counts_.find(what);
        return it == w5_counts_.end() ? 0 : it->second;
    }

private:
    static std::vector<std::string> w5_split(const std::string& list) {
        std::vector<std::string> out;
        std::stringstream ss(list);
        for (std::string t; std::getline(ss, t, ',');)
            if (!pres_clean(t).empty()) out.push_back(pres_clean(t));
        return out;
    }
    void w5_init_once() {
        if (w5_inited_ || !rules_ini_) return;
        w5_inited_ = true;
        auto av = [&](const char* k) { return pres_clean(rules_ini_->get("AudioVisual", k)); };
        w5_cheer_ = av("CheerSound");
        w5_bld_repaired_ = av("BuildingRepairedSound");
        w5_garrisoned_ = av("BuildingGarrisonedSound");
        static const char* kCrate[7] = {"CrateMoneySound", "CrateRevealSound", "CrateFireSound", "CrateArmourSound",
                                        "CrateSpeedSound", "CrateUnitSound", "CratePromoteSound"};
        for (int i = 0; i < 7; ++i) w5_crate_[i] = av(kCrate[i]);
        w5_heal_crate_ = av("HealCrateSound");
        if (w5_heal_crate_.empty()) w5_heal_crate_ = pres_clean(rules_ini_->get("General", "HealCrateSound"));
        w5_sinking_ = av("SinkingSound");
        w5_repair_bridge_ = av("RepairBridgeSound");
        const std::vector<std::string> ct = w5_split(av("CreditTicks"));
        w5_credit_ticks_ = static_cast<int>(ct.size());
        if (ct.size() >= 1) w5_credit_up_ = ct[0];
        if (ct.size() >= 2) w5_credit_down_ = ct[1];
        const std::string cy = av("ConditionYellow");
        if (!cy.empty()) w5_yellow_ = std::atof(cy.c_str()) / 100.0;
        for (const std::string& s : {w5_cheer_, w5_bld_repaired_, w5_garrisoned_, w5_heal_crate_, w5_sinking_,
                                     w5_repair_bridge_, w5_credit_up_, w5_credit_down_})
            snd_.ensure_sound(s);
        for (const std::string& s : w5_crate_) snd_.ensure_sound(s);
        // Type=SHROUD events are silent over unrevealed cells (local player).
        snd_.set_reveal_fn([this](int x, int y) { return !world_ || world_->object_visible_at(0, x, y); });
    }

    // [RE: FUN_006FF0A0] the acknowledgement voice of the click's mission.
    void order_voice(const ra2engine::InputActions& act, const ra2engine::Unit& au, const UnitVoices& av) {
        using O = ra2engine::InputActions::Order;
        if (act.attack_ordered) {
            // Sabotage (C4 on a building) is not Attack/Move -> VoiceSpecialAttack.
            if (act.target_building >= 0 && av.c4 && !av.special.empty())
                snd_.play_at(pick_report(av.special), au.fx, au.fy, "special attack order");
            else
                snd_.play_at(av.attack, au.fx, au.fy, "attack order");
            return;
        }
        switch (act.order) {
            case O::Harvest: snd_.play_at(av.harvest.empty() ? av.move : av.harvest, au.fx, au.fy, "harvest order"); return;
            case O::Enter: snd_.play_at(av.enter.empty() ? av.move : av.enter, au.fx, au.fy, "enter order"); return;
            case O::Capture:
                snd_.play_at(!av.capture.empty() ? av.capture : !av.enter.empty() ? av.enter : av.move, au.fx, au.fy,
                             "capture order");
                return;
            case O::Deploy: snd_.play_at(av.deploy_voice, au.fx, au.fy, "deploy order"); return;
            case O::Special:
                snd_.play_at(av.special.empty() ? av.move : pick_report(av.special), au.fx, au.fy, "special order");
                return;
            case O::Move: default: snd_.play_at(av.move, au.fx, au.fy, "move order"); return;
        }
    }

    // Per render frame: the click-made EVA lines.
    void w5_update(const ra2engine::World& w, const BuildingAssets& ba, const ra2engine::InputActions& act, double) {
        w5_init_once();
        (void)w;
        (void)ba;
        // [RE: @448171 IsPlayerControl] / [@4439D4 from a player click].
        if (act.primary_set) snd_.eva("EVA_PrimaryBuildingSelected");
        if (act.rally_set) snd_.eva("EVA_NewRallyPointEstablished");
    }

    // Per logic tick: the state-edge sounds.
    void w5_tick(const ra2engine::World& w, const BuildingAssets& ba) {
        if (!audio_) return;
        w5_init_once();
        const auto& us = w.units();
        if (w5_u_.size() < us.size()) w5_u_.resize(us.size());
        for (std::size_t i = 0; i < us.size(); ++i) {
            const ra2engine::Unit& u = us[i];
            W5U& p = w5_u_[i];
            const UnitVoices& v = audio_->voices[u.image];
            // W5-E2 [RE: spy infiltration FUN_004571E0]: a spy consumed on the
            // tick after it had an infiltrate order -> the victim's and the
            // spy owner's lines by purpose (Medium: the per-branch guards).
            if (p.seen && p.inf_target >= 0 && !u.alive && u.die_time < 0.0 && p.alive)
                w5_infiltrated(w, p.inf_target, u.house, p.inf_purpose);
            p.alive = u.alive;
            p.inf_target = u.infiltrate_target;
            p.inf_purpose = u.infiltrate_purpose;
            if (!p.seen) {
                p.seen = true;
                p.hp = u.hp;
                p.sinking = u.an.sinking;
                p.crashing = u.an.crashing;
                p.turret = u.turret_facing;
                continue;
            }
            // VoiceFeedback on the NowYellow edge, 30%, player-controlled.
            if (u.house == 0 && u.alive && u.die_time < 0.0 && u.max_hp > 0 && u.hp > 0 && u.hp < p.hp &&
                !v.feedback.empty()) {
                const double was = static_cast<double>(p.hp) / u.max_hp, now = static_cast<double>(u.hp) / u.max_hp;
                if (was > w5_yellow_ && now <= w5_yellow_) {
                    w5_rng_ = w5_rng_ * 1103515245u + 12345u;
                    if ((w5_rng_ >> 8) % 100u < 30u) snd_.play_at(pick_report(v.feedback), u.fx, u.fy, "feedback");
                }
            }
            // UnitRepaired: a player vehicle the service depot brought to full.
            if (u.house == 0 && u.alive && u.die_time < 0.0 && u.hp >= u.max_hp && p.hp < u.max_hp && p.hp > 0 &&
                u.is_vehicle && w5_next_to_depot(w, ba, u) && radar_event(8, u.fx, u.fy))
                snd_.eva("EVA_UnitRepaired");
            // Sinking / crashing start: voice, then the sound on a controller.
            if (u.an.sinking && !p.sinking) {
                snd_.play_at(v.voice_sinking, u.fx, u.fy, "voice sinking");
                const std::string& s = v.sinking.empty() ? w5_sinking_ : v.sinking;
                p.loop = w5_loop(s, u.fx, u.fy, "sinking");
            }
            if (u.an.crashing && !p.crashing) {
                snd_.play_at(v.voice_crashing, u.fx, u.fy, "voice crashing");
                p.loop = w5_loop(v.crashing, u.fx, u.fy, "crashing");
            }
            if (p.loop && (!u.alive || (!u.an.sinking && !u.an.crashing))) {
                snd_.stop_loop(p.loop);
                p.loop = 0;
            } else if (p.loop && !snd_.update_loop(p.loop, u.fx, u.fy)) {
                p.loop = 0;
            }
            // TurretRotateSound while the turret turns.
            if (!v.turret_rotate.empty()) {
                const bool turning = u.alive && u.die_time < 0.0 && std::abs(u.turret_facing - p.turret) > 1e-6;
                w5_turret(p.turret_loop, turning, v.turret_rotate, u.fx, u.fy);
            }
            p.hp = u.hp;
            p.sinking = u.an.sinking;
            p.crashing = u.an.crashing;
            p.turret = u.turret_facing;
        }
        // Buildings: turret rotation (Grand Cannon), engineer repair-to-full,
        // capture lines.
        const auto& bs = w.buildings();
        if (w5_b_.size() < bs.size()) w5_b_.resize(bs.size());
        for (std::size_t i = 0; i < bs.size(); ++i) {
            const ra2engine::Building& b = bs[i];
            W5B& p = w5_b_[i];
            const double cx = b.cx + (b.fw - 1) / 2.0, cy = b.cy + (b.fh - 1) / 2.0;
            const bool dead = !b.alive || b.state == ra2engine::Building::State::Dead;
            const ra2engine::BuildingTypeDef* t =
                (ba.ok && b.type >= 0 && b.type < static_cast<int>(ba.roster.size())) ? &ba.roster[static_cast<std::size_t>(b.type)]
                                                                                       : nullptr;
            if (!p.seen) {
                p = W5B{true, b.hp, b.house, b.def_turret_facing, b.being_repaired, 0, b.power_off};
                continue;
            }
            // W5-E2 [RE: FUN_00452350 / FUN_00452250 -- IsPlayerControl(owner)]:
            // the player's building powered down / up (toggle mode or trigger).
            if (!dead && b.house == 0 && b.power_off != p.power_off) note_power_toggle(!b.power_off);
            p.power_off = b.power_off;
            if (t) {
                const std::string& trs = w5_bld_key(t->id, "TurretRotateSound");
                if (!trs.empty())
                    w5_turret(p.turret_loop, !dead && std::abs(b.def_turret_facing - p.turret) > 1e-6, trs, cx, cy);
            }
            // Repair_To_Full by an engineer: an allied building jumped to full
            // health without the wrench running.
            if (!dead && b.house == 0 && b.hp >= b.max_hp && p.hp < b.max_hp && !p.wrench && b.house == p.house &&
                b.max_hp - p.hp > 8)
                snd_.play_at(w5_bld_repaired_, cx, cy, "building repaired");
            // Capture [RE: FUN_004481AB @448310]. A garrison flip is not a capture.
            if (!dead && b.house != p.house && p.house >= 0 && !w5_garrison_flip(static_cast<int>(i))) {
                const bool tech = t && t->capturable;
                if (tech) {
                    if (p.house == 0) snd_.eva("EVA_TechBuildingLost");
                    if (b.house == 0) {
                        const std::string& ev = w5_bld_key(t->id, "CaptureEvaEvent");
                        if (!ev.empty() && snd_.has_eva(ev)) snd_.eva(ev);
                    }
                } else if ((p.house == 0 || b.house == 0) && radar_event(10, cx, cy)) {
                    snd_.eva("EVA_BuildingCaptured");
                }
            }
            p.hp = b.hp;
            p.house = b.house;
            p.turret = b.def_turret_facing;
            p.wrench = b.being_repaired;
        }
        // Bridge repaired (a hut entered): fewer destroyed bridge cells.
        if (w.bridges_enabled()) {
            const int d = w.bridge_cells_destroyed();
            if (w5_bridge_dead_ >= 0 && d < w5_bridge_dead_) {
                double bx = 0, by = 0;
                w5_bridge_pos(w, ba, bx, by);
                if (radar_event(14, bx, by)) snd_.eva("EVA_BridgeRepaired");
                snd_.play_at(w5_repair_bridge_, bx, by, "bridge repaired");
            }
            w5_bridge_dead_ = d;
        }
    }

    // Helpers ------------------------------------------------------------------
    std::uint32_t w5_loop(const std::string& key, double fx, double fy, const char* why) {
        if (key.empty()) return 0;
        pres_log(frame_, std::string("sound ") + key + " (" + why + ")");
        ++w5_counts_[why];
        return snd_.loop_at(key, fx, fy);
    }
    void w5_turret(std::uint32_t& h, bool turning, const std::string& key, double fx, double fy) {
        if (turning && !h) {
            pres_log(frame_, "sound " + key + " (turret rotate)");
            ++w5_counts_["turret rotate"];
            h = snd_.loop_at(key, fx, fy);
            if (!h) h = 1;   // deviceless: remember "playing" so the log shows edges only
        } else if (!turning && h) {
            if (h != 1) snd_.stop_loop(h);
            h = 0;
        } else if (h > 1 && !snd_.update_loop(h, fx, fy)) {
            h = 1;   // one-shot ended while still turning: do not restart (one per turn)
        }
    }
    bool w5_next_to_depot(const ra2engine::World& w, const BuildingAssets& ba, const ra2engine::Unit& u) const {
        if (!ba.ok) return false;
        for (const ra2engine::Building& b : w.buildings()) {
            if (!b.alive || b.house != u.house || b.type < 0 || b.type >= static_cast<int>(ba.roster.size()) ||
                !ba.roster[static_cast<std::size_t>(b.type)].unit_repair)
                continue;
            const int dx = u.cell_x < b.cx ? b.cx - u.cell_x : (u.cell_x >= b.cx + b.fw ? u.cell_x - (b.cx + b.fw - 1) : 0);
            const int dy = u.cell_y < b.cy ? b.cy - u.cell_y : (u.cell_y >= b.cy + b.fh ? u.cell_y - (b.cy + b.fh - 1) : 0);
            if (std::max(dx, dy) <= 1) return true;
        }
        return false;
    }
    void w5_bridge_pos(const ra2engine::World& w, const BuildingAssets& ba, double& x, double& y) const {
        // The repair hut nearest any own engineer is not tracked; use the
        // nearest bridge-repair hut to the view centre [RE-FALLBACK: the radar
        // event cell].
        x = y = 0;
        double best = 1e18;
        const double vx = (vw_ / 2.0 - cam_.x) / 60.0 + (vh_ / 2.0 - cam_.y) / 30.0;
        const double vy = (vh_ / 2.0 - cam_.y) / 30.0 - (vw_ / 2.0 - cam_.x) / 60.0;
        for (const ra2engine::Building& b : w.buildings()) {
            if (!b.alive || !ba.ok || b.type < 0 || b.type >= static_cast<int>(ba.roster.size()) ||
                !ba.roster[static_cast<std::size_t>(b.type)].bridge_repair_hut)
                continue;
            const double d = (b.cx - vx) * (b.cx - vx) + (b.cy - vy) * (b.cy - vy);
            if (d < best) { best = d; x = b.cx; y = b.cy; }
        }
    }
    const std::string& w5_bld_key(const std::string& id, const char* key) {
        const std::string k = id + "|" + key;
        auto it = w5_bkeys_.find(k);
        if (it != w5_bkeys_.end()) return it->second;
        return w5_bkeys_.emplace(k, rules_ini_ ? pres_clean(rules_ini_->get(id, key)) : std::string()).first->second;
    }
    bool w5_garrison_flip(int b) const {
        auto it = w5_garrison_flip_.find(b);
        return it != w5_garrison_flip_.end() && frame_ - it->second <= 2;
    }

    struct W5U {
        bool seen = false;
        int hp = 0;
        bool sinking = false, crashing = false;
        double turret = 0.0;
        std::uint32_t loop = 0, turret_loop = 0;
        bool alive = true;
        int inf_target = -1, inf_purpose = 0;
    };
    void w5_infiltrated(const ra2engine::World& w, int bi, int spy_house, int purpose) {
        if (bi < 0 || bi >= static_cast<int>(w.buildings().size())) return;
        const int victim = w.buildings()[static_cast<std::size_t>(bi)].house;
        using P = ra2engine::World::InfiltratePurpose;
        const char* vline = nullptr;   // the infiltrated player
        std::vector<const char*> sline;   // the spy's owner
        switch (static_cast<P>(purpose)) {
            case P::RadarLab: vline = "EVA_RadarSabotaged"; sline = {"EVA_BuildingInfRadarSabotaged"}; break;
            case P::PowerPlant: vline = "EVA_PowerSabotaged"; sline = {"EVA_BuildingInfiltrated", "EVA_EnemyBasePoweredDown"}; break;
            case P::Refinery: vline = "EVA_CashStolen"; sline = {"EVA_BuildingInfCashStolen"}; break;
            case P::StolenAllied: case P::StolenSoviet: case P::StolenThird:
                vline = "EVA_TechnologyStolen"; sline = {"EVA_BuildingInfiltrated", "EVA_NewTechnologyAcquired"}; break;
            case P::Factory: vline = "EVA_TechnologyStolen"; sline = {"EVA_BuildingInfiltrated"}; break;
            default: sline = {"EVA_BuildingInfiltrated"}; break;
        }
        pres_log(frame_, "infiltrated b" + std::to_string(bi) + " purpose=" + std::to_string(purpose));
        if (victim == 0 && vline) snd_.eva(vline);
        if (spy_house == 0)
            for (const char* l : sline) snd_.eva(l);
    }
    struct W5B {
        bool seen = false;
        int hp = 0, house = -1;
        double turret = 0.0;
        bool wrench = false;
        std::uint32_t turret_loop = 0;
        bool power_off = false;
    };
    std::vector<W5U> w5_u_;
    std::vector<W5B> w5_b_;
    std::map<std::string, std::string> w5_bkeys_;
    std::map<int, long> w5_garrison_flip_;
    std::map<std::string, long> w5_counts_;
    bool w5_inited_ = false;
    std::string w5_cheer_, w5_bld_repaired_, w5_garrisoned_, w5_crate_[7], w5_heal_crate_, w5_sinking_,
        w5_repair_bridge_, w5_credit_up_, w5_credit_down_;
    int w5_credit_ticks_ = 0;
    double w5_yellow_ = 0.5;
    std::uint32_t w5_rng_ = 0x5EED5u;   // audio-local (never the sim RNG)
    int w5_bridge_dead_ = -1;
