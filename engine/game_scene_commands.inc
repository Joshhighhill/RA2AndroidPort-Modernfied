// NET L1: GameScene's command layer -- #included INSIDE the GameScene class body
// (public section). Every player action reaches the World through a Command
// (engine/command.hpp): the UI builds it, issue() stamps it for the next logic
// tick (single player: delay 0) and queues it, apply_due_commands() applies the
// due ones at the START of each logic tick in the deterministic order (frame,
// issuing house, sequence). Production and placement act on this scene's
// economy through the CommandHost below (L2 moves the economy into the World and
// swaps these bodies).
//
// Record / replay (R-184 reworked): `--record <f>` writes this scene's config
// and every applied command with its tick; `--replay <f>` rebuilds the scene
// from the config and re-applies the log tick by tick (game_scene_replay.inc).

    std::unique_ptr<CmdRecorder> cmd_rec_;   // --record
    ra2engine::CommandQueue cmdq_;
    ra2engine::CommandScheduler sched_{cmdq_, 0, 0};
    std::uint32_t sim_frame_ = 0;            // logic ticks this scene has run (the command clock)
    ra2engine::Selection sim_sel_;           // the selection the sim saw last (SELECT_NOTIFY)
    std::vector<int> notified_sel_;          // ids of the last SELECT_NOTIFY issued
    bool replaying_ = false;                 // --replay: the log feeds the queue, one tick per update
    std::uint32_t cmds_applied_ = 0;

    struct SceneCmdHost : ra2engine::CommandHost {
        GameScene* g;
        explicit SceneCmdHost(GameScene* gs) : g(gs) {}
        const ra2engine::OrderRules* order_rules() const override { return &g->input_.order_rules(); }
        void produce(int h, int rtti, int type, bool) override { g->cmd_produce(h, rtti, type); }
        void suspend(int h, int rtti, int type, bool) override { g->cmd_suspend(h, rtti, type); }
        void abandon(int h, int rtti, int type, bool) override { g->cmd_abandon(h, rtti, type, false); }
        void abandon_all(int h, int rtti, int type, bool) override { g->cmd_abandon(h, rtti, type, true); }
        void place(int h, int, int type, int cx, int cy) override { g->cmd_place(h, type, cx, cy); }
        void selection_notify(int h, const std::vector<int>& ids) override {
            if (h != g->local_house_) return;
            g->sim_sel_.clear();
            for (int i : ids) g->sim_sel_.set(i);
        }
    };
    SceneCmdHost cmd_host_{this};

    // The UI's way in: stamp (house, frame = next tick + delay, sequence), queue.
    void issue(ra2engine::Command c) {
        if (replaying_) return;   // a replay takes its commands from the log only
        sched_.set_house(local_house_);
        sched_.set_now(sim_frame_);
        sched_.schedule(std::move(c));
    }
    // Apply every queued command due by the current tick (tick start).
    void apply_due_commands() {
        if (cmdq_.empty()) return;
        std::vector<ra2engine::Command> due = cmdq_.take_due(sim_frame_);
        for (ra2engine::Command& c : due) {
            c.frame = sim_frame_;   // the tick it took effect on (= its stamp with delay 0)
            if (cmd_rec_) cmd_rec_->command(c);
            world_.set_order_issuer(c.house);   // L2: the issuer the World's checks answer for
            ra2engine::apply_command(world_, c, &cmd_host_);
            world_.set_order_issuer(local_house_);
            ++cmds_applied_;
        }
    }
    // Event 33 "Selected by player": a campaign's triggers see the selection
    // through the command log (a replay reproduces it).
    void select_notify_tick() {
        if (!campaign_ || replaying_) return;
        input_.selection().prune(world_);
        std::vector<int> ids = input_.selection().ids();
        if (ids == notified_sel_) return;
        notified_sel_ = ids;
        ra2engine::Command c;
        c.type = ra2engine::EventType::SelectNotify;
        c.ext.assign(ids.begin(), ids.end());
        issue(std::move(c));
    }

    // --- production / placement applies (the pre-L1 cameo_click bodies) -----
    // [RE: StripClass cameo action FUN_006AA390 decides the event; HouseClass
    // Begin_Production FUN_004FA170 / FUN_004FA730 / Abandon FUN_004FCCA0 run
    // it] through the World's per-house economy (L2).
    static bool is_building_rtti(int rtti) { return rtti == static_cast<int>(ra2engine::Rtti::BuildingType); }
    ra2engine::QueueCat building_cat(int type) const {
        const bool def = type >= 0 && type < static_cast<int>(ba_.defense.size()) && ba_.defense[type] != 0;
        return def ? ra2engine::QueueCat::Defense : ra2engine::QueueCat::Structure;
    }
    ra2engine::Rtti unit_rtti(int type) const {
        if (type < 0 || type >= static_cast<int>(ur_.defs.size())) return ra2engine::Rtti::UnitType;
        const ra2engine::UnitTypeDef& d = ur_.defs[static_cast<std::size_t>(type)];
        if (d.combat.is_aircraft) return ra2engine::Rtti::AircraftType;
        return d.is_vehicle ? ra2engine::Rtti::UnitType : ra2engine::Rtti::InfantryType;
    }
    bool unit_naval(int type) const {
        return type >= 0 && type < static_cast<int>(ur_.defs.size()) && ur_.defs[static_cast<std::size_t>(type)].combat.is_naval;
    }
    void issue_type_event(ra2engine::EventType t, bool building, int type) {
        issue(ra2engine::make_type_event(t, building ? ra2engine::Rtti::BuildingType : unit_rtti(type), type,
                                         building ? false : unit_naval(type)));
    }
    // L2: the economy lives in the World (HouseEcon); each apply is the
    // World's command API, which re-validates the event (EventClass::Execute).
    ra2engine::QueueCat type_cat(int rtti, int type) const {
        return is_building_rtti(rtti) ? building_cat(type) : rp_cat_of_unit(type);
    }
    bool type_ok(int rtti, int type) const {
        return type >= 0 && type < static_cast<int>(is_building_rtti(rtti) ? ba_.roster.size() : ur_.defs.size());
    }
    void cmd_produce(int house, int rtti, int type) {
        if (type_ok(rtti, type)) world_.econ_produce(house, type_cat(rtti, type), type);
    }
    void cmd_suspend(int house, int rtti, int type) {
        if (!type_ok(rtti, type)) return;
        const ra2engine::QueueCat cat = type_cat(rtti, type);
        const ra2engine::HouseEcon* e = world_.econ(house);
        if (e && e->queues.producing(cat) == type) world_.econ_suspend(house, cat);
    }
    void cmd_abandon(int house, int rtti, int type, bool all) {
        if (type_ok(rtti, type)) world_.econ_abandon(house, type_cat(rtti, type), type, all);
    }
    void cmd_place(int house, int type, int cx, int cy) {
        if (type < 0 || type >= static_cast<int>(ba_.roster.size())) return;
        const ra2engine::QueueCat cat = building_cat(type);
        const ra2engine::HouseEcon* e = world_.econ(house);
        if (!e || e->queues.producing(cat) != type) return;   // nothing of this type to place
        world_.econ_place(house, cat, cx, cy);
    }

    // The complete simulation state the replay compares: the World, this
    // scene's economy (wallet, queues, backlog / hold, building states) and the
    // campaign trigger runtime. Camera, sidebar tab and the rolling credits
    // display are UI and stay out.
    std::uint64_t sim_state_hash() const {
        std::uint64_t h = 1469598103934665603ull;
        auto mix = [&h](const std::vector<uint8_t>& b) {
            for (uint8_t c : b) { h ^= c; h *= 1099511628211ull; }
        };
        mix(ra2engine::save::save_world(world_));
        ra2engine::save::SceneSave sc = capture_save();
        sc.cam_x = sc.cam_y = 0;
        sc.active_tab = 0;
        sc.credits_shown = 0.0;
        mix(ra2engine::save::to_bytes(sc));
        if (campaign_) {
            mix(ra2engine::save::save_triggers(const_cast<CampaignRuntime&>(*campaign_).trigger_system()));
            ra2engine::CampaignHostSave hs = const_cast<CampaignRuntime&>(*campaign_).capture();
            mix(ra2engine::save::to_bytes(hs));
        }
        return h;
    }
    std::uint32_t sim_frame() const { return sim_frame_; }
    const ra2engine::Selection& ui_selection() const { return input_.selection(); }
    ra2engine::Selection& ui_selection_mut() { return input_.selection(); }
    // --replay: the log's commands go straight into the queue (each at its
    // recorded tick); update() then runs exactly one tick per call.
    void begin_replay(const std::vector<ra2engine::Command>& cmds) {
        replaying_ = true;
        input_.set_command_sink([](ra2engine::Command) {});   // no UI commands in a replay
        for (const ra2engine::Command& c : cmds) cmdq_.push(c);
    }
