// ============================================================================
// Multiplayer shell pages (agent S): Network (LAN browser), Internet (online
// rooms on the player's own ra2lobbyd server), the multiplayer lobby, and the
// match hand-off. Drawn on the faithful shell (draw_shell + the skirmish
// page's widgets) at the 640x480 template canvas through ShellXform, so they
// scale and stay touch-friendly exactly like the Skirmish page (every target
// is at least a 19-px logical row = ~28 px on a 720p panel; lists scroll by
// tapping their strip).
//
// Included by main.cpp right after skirmish_page.inc. The protocol layers
// are the SDL-free engine/mp/ modules (tested in tests/test_mp.cpp):
//   LAN:    LanBrowser / LanAnnouncer (UDP 50010) + LobbyHost/LobbyClient
//           over MpNet (direct UDP; host game port [Net] LanPort = 50020)
//   Online: ServerLink (ra2lobbyd control plane) + the lobby over the
//           CnCNet V3 tunnel (MpNet::open_tunnel)
//   Start:  mp_start_session() -- THE hand-off to the lockstep layer.
//
// --play targets / probe keys: scripts/play/README.md "Multiplayer".
// docs/gap/net/05-transport-infra.md §3, §6; 01-original-protocol.md §6/§7.1.
// ============================================================================

// (engine/mp/ headers are included at the top of main.cpp: this file lands
// inside main.cpp's anonymous namespace.)

namespace mpui {

// --- RA2.INI [Net] -----------------------------------------------------------
struct Prefs {
    std::string nick = "Player";
    std::string server;               // "host[:port]" of the player's ra2lobbyd
    std::uint64_t pid = 0;            // random per install, never shown
    std::uint16_t lan_port = ra2mp::kLanGamePort;

    static Prefs load() {
        Prefs p;
        const ra2engine::IniDoc doc = read_settings_doc(settings_file_path());
        if (auto v = doc.get("Net", "Nickname")) p.nick = ra2mp::sanitize_name(*v);
        else if (auto h = doc.get("MultiPlayer", "Handle")) p.nick = ra2mp::sanitize_name(ra2engine::lobby_handle_decode(*h));
        if (p.nick.empty()) p.nick = "Player";
        if (auto v = doc.get("Net", "Server")) p.server = *v;
        if (auto v = doc.get("Net", "LanPort")) {
            const int n = std::atoi(v->c_str());
            if (n > 1024 && n < 65536) p.lan_port = static_cast<std::uint16_t>(n);
        }
        if (auto v = doc.get("Net", "PlayerId")) p.pid = std::strtoull(v->c_str(), nullptr, 16);
        if (p.pid == 0) {
            // Random 64-bit id: time + address entropy through splitmix64.
            std::uint64_t z = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^
                              reinterpret_cast<std::uintptr_t>(&p) ^ (static_cast<std::uint64_t>(std::rand()) << 32);
            z += 0x9E3779B97F4A7C15ull;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            p.pid = (z ^ (z >> 31)) | 1ull;
            p.save();
        }
        return p;
    }
    void save() const {
        const std::string path = settings_file_path();
        if (path.empty()) return;
        ra2engine::IniDoc doc = read_settings_doc(path);
        doc.set("Net", "Nickname", nick);
        doc.set("Net", "Server", server);
        doc.set("Net", "PlayerId", ra2mp::hex64(pid));
        doc.set("Net", "LanPort", std::to_string(lan_port));
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        const std::string t = doc.serialize();
        f.write(t.data(), static_cast<std::streamsize>(t.size()));
    }
};

// CRC-32 of the effective rulesmd.ini bytes: the version gate's "same game
// data" check (05 §2.5). Computed once.
inline std::uint32_t rules_crc() {
    static std::uint32_t crc = 0;
    static bool done = false;
    if (!done) {
        done = true;
        try {
            const std::vector<uint8_t> b = read_ra2md_file(env_install_dir(), "rulesmd.ini");
            crc = ra2engine::save::crc32(b.data(), b.size());
        } catch (const std::exception&) { crc = 0; }
    }
    return crc;
}

inline ra2mp::MpIdentity identity(const Prefs& p) {
    ra2mp::MpIdentity id;
    id.nick = p.nick;
    id.player_id = p.pid;
    id.rules_crc = rules_crc();
    id.version = RA2_VERSION;
    id.platform = ra2mp::platform_code();   // NET-PLAY: mixed platforms need the cross-platform option
    return id;
}

inline int platform_code() { return ra2mp::platform_code(); }

inline void init_once() {
    static bool done = false;
    if (done) return;
    done = true;
    ra2mp::net_init();
    // Android: WifiManager.MulticastLock while a LAN browser / announcer is open.
    ra2mp::lan_set_multicast_hook([](bool on) { ra2engine::os_android_multicast_lock(on); });
}

inline std::uint32_t random_u32() {
    static std::uint64_t s = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    std::uint32_t v = static_cast<std::uint32_t>(s >> 32);
    return v ? v : 1u;
}

// Log a multiplayer event to stderr and (under --play) the play log.
inline void log_line(const std::string& s) {
    std::cerr << "[mp] " << s << "\n";
    pres_log(0, "mp " + s);
}

}  // namespace mpui

// ---------------------------------------------------------------------------
// The hand-off to the lockstep layer (NET-PLAY): the GameScene is built from
// session_to_skirmish(s, ctx) with the session seed, the lobby's resolved
// start positions and the local house, and runs as a lockstep match over the
// live transport (the lobby's socket / tunnel id) -- see net_play.inc.
// ---------------------------------------------------------------------------
Scene* mp_start_session(const Rules& rules, bool no_audio, const MenuArt& art, const ra2mp::MpSessionConfig& s,
                        std::shared_ptr<ra2mp::MpNet> transport);

namespace mpui {

// A text edit owned by a page.
struct Edit {
    std::string id;                   // probe target "edit:<id>"
    ra2engine::SbRect r;
    std::string text;
    std::size_t max = 32;
    bool enabled = true;
};

}  // namespace mpui

// Common machinery of the multiplayer pages: shell chrome, right-column
// buttons, edits (Delete clears, Enter = on_enter), confirm popup, status
// line, --play probe. Subclasses draw/handle their body.
class MpPage : public Scene {
public:
    MpPage(const Rules& rules, bool no_audio, const MenuArt& art) : rules_(rules), no_audio_(no_audio), art_(art) {
        mpui::init_once();
    }
    ~MpPage() override { set_focus(-1); }

    void update(double dt) override {
        clock_ += dt;
        tick(ra2mp::now_ms());
        if (!art_.audio || !art_.audio->sys.enabled || !art_.audio->sys.mixer) return;
        if (!art_.audio->sys.mixer->playing("menu:grinder")) art_.audio->sys.mixer->play("menu:grinder");
        art_.audio->sys.out.pump(*art_.audio->sys.mixer);
    }
    Scene* next() override { Scene* n = next_; next_ = nullptr; return n; }
    bool locate(const std::string& what, int& x, int& y) const override {
        const auto t = targets();
        const auto it = t.find(what);
        if (it == t.end()) return false;
        const ra2engine::SbRect d = xform().hit(it->second);
        x = d.x + d.w / 2;
        y = d.y + d.h / 2;
        return true;
    }
    void play_probe(ra2play::Probe& p) override {
        p.scene = name();
        p.set_s("page", header_text());
        for (const auto& kv : targets()) {
            const ra2engine::SbRect d = xform().hit(kv.second);
            p.targets[kv.first] = ra2play::Rect{d.x, d.y, std::max(1, d.w), std::max(1, d.h)};
        }
        for (const ShellButton& b : buttons()) p.set("button_enabled(" + b.label + ")", b.enabled ? 1 : 0);
        for (const mpui::Edit& e : edits_) p.set_s("edit(" + e.id + ")", e.text);
        p.set_s("focus", focus_ >= 0 ? edits_[static_cast<std::size_t>(focus_)].id : "");
        p.set_s("status", status_);
        body_probe(p);
        confirm_.play_probe(p);
    }

    void render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override {
        SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
        const ShellXform xf = xform();
        const std::vector<ShellButton> btns = buttons();
        int lmx = -1000, lmy = -1000;
        if (mx_ >= 0) to_logical(mx_, my_, lmx, lmy);
        int hover = -1;
        for (int i = 0; i < static_cast<int>(btns.size()); ++i)
            if (btns[i].enabled && btns[i].rect.contains(lmx, lmy)) hover = i;
        draw_shell(plat, tc, art_, xf, btns, hover, (hover >= 0 && down_) ? hover : -1, header_text(), "v1.006",
                   -1, nullptr, 1);
        const ShellPainter p{ren, tc, art_, xf};
        draw_body(p, tc);
        for (std::size_t i = 0; i < edits_.size(); ++i) {
            const mpui::Edit& e = edits_[i];
            if (!e.enabled) {
                p.frame(p.dev(e.r), false);
                p.label(p.fit(e.text, e.r.w - 10), e.r.x + 4, e.r.y, e.r.w - 8, e.r.h, 0, false);
                continue;
            }
            p.edit(e.r, e.text, static_cast<int>(i) == focus_, std::fmod(clock_, 1.0) < 0.5);
        }
        draw_overlay(p);
        p.label(p.fit(status_, 454), 15, 458, 454, 19, 0);
        confirm_.render(ren, tc);
    }

    void handle(const ra2engine::InputEvent& ev) override {
        using ra2engine::InputEvent;
        if (ev.kind == InputEvent::PointerMove || ev.kind == InputEvent::PointerDown || ev.kind == InputEvent::PointerUp) {
            mx_ = ev.x;
            my_ = ev.y;
        }
        if (ev.button == InputEvent::Left) {
            if (ev.kind == InputEvent::PointerDown) down_ = true;
            if (ev.kind == InputEvent::PointerUp) down_ = false;
        }
        if (confirm_.open()) {
            const int b = confirm_.handle(ev);
            if (b >= 0) confirm_done(confirm_action_, b);
            return;
        }
        int lx = 0, ly = 0;
        to_logical(ev.x, ev.y, lx, ly);
        if (ev.kind == InputEvent::Key) {
            if (focus_ >= 0) {
                mpui::Edit& e = edits_[static_cast<std::size_t>(focus_)];
                if (ev.key == ra2engine::kKeyBackspace) {
                    while (!e.text.empty() && (static_cast<unsigned char>(e.text.back()) & 0xC0) == 0x80) e.text.pop_back();
                    if (!e.text.empty()) e.text.pop_back();
                    edit_changed(focus_);
                    return;
                }
                if (ev.key == ra2engine::kKeyDelete) { e.text.clear(); edit_changed(focus_); return; }
                if (ev.key == ra2engine::kKeyEnter) { const int f = focus_; on_enter(f); return; }
            }
            if (body_key(ev)) return;
            if (ev.key == ra2engine::kKeyEscape) {
                set_focus(-1);
                press(back_action());
            }
            return;
        }
        if (ev.kind == InputEvent::Text) {
            if (focus_ < 0) return;
            mpui::Edit& e = edits_[static_cast<std::size_t>(focus_)];
            for (const char* c = ev.text; *c; ++c) {
                const unsigned char ch = static_cast<unsigned char>(*c);
                if (ch < 32 || ch == 127) continue;
                if (e.text.size() < e.max) e.text += static_cast<char>(ch);
            }
            edit_changed(focus_);
            return;
        }
        if (ev.kind == InputEvent::PointerMove || ev.kind == InputEvent::PointerUp) {
            body_drag(ev, lx, ly);
            return;
        }
        if (ev.kind != InputEvent::PointerDown || ev.button != InputEvent::Left) return;
        if (body_modal(lx, ly)) return;
        for (const ShellButton& b : buttons())
            if (b.enabled && b.rect.contains(lx, ly)) {
                set_focus(-1);
                play_ui("menu:click");
                press(b.action);
                return;
            }
        for (std::size_t i = 0; i < edits_.size(); ++i)
            if (edits_[i].enabled && edits_[i].r.contains(lx, ly)) { set_focus(static_cast<int>(i)); return; }
        set_focus(-1);
        body_click(lx, ly);
    }

protected:
    static constexpr int kWinW = 1280, kWinH = 720;
    static ShellXform xform() { return ShellXform::make(kWinW, kWinH); }
    static void to_logical(int dx, int dy, int& lx, int& ly) {
        const ShellXform xf = xform();
        lx = static_cast<int>(std::floor((dx - xf.ox) / xf.s));
        ly = static_cast<int>(std::floor((dy - xf.oy) / xf.s));
    }
    static ShellButton btn(int row, const std::string& s, bool en, int act) {
        return ShellButton{{484, 199 + 42 * row, 156, 42}, s, en, act, PAGE_MAIN};
    }
    virtual std::vector<ShellButton> buttons() const = 0;
    virtual std::string header_text() const = 0;
    virtual int back_action() const = 0;
    virtual void press(int act) = 0;
    virtual void draw_body(const ShellPainter& p, ra2engine::TextureCache& tc) = 0;
    virtual void draw_overlay(const ShellPainter& p) { (void)p; }
    virtual bool body_modal(int lx, int ly) { (void)lx; (void)ly; return false; }
    virtual void body_click(int lx, int ly) { (void)lx; (void)ly; }
    virtual void body_drag(const ra2engine::InputEvent& ev, int lx, int ly) { (void)ev; (void)lx; (void)ly; }
    virtual bool body_key(const ra2engine::InputEvent& ev) { (void)ev; return false; }
    virtual void body_probe(ra2play::Probe& p) { (void)p; }
    virtual void body_targets(std::map<std::string, ra2engine::SbRect>& t) const { (void)t; }
    virtual void on_enter(int edit) { (void)edit; set_focus(-1); }
    virtual void edit_changed(int edit) { (void)edit; }
    virtual void tick(std::uint64_t now) { (void)now; }
    virtual void confirm_done(int action, int button) { (void)action; (void)button; }

    std::map<std::string, ra2engine::SbRect> targets() const {
        std::map<std::string, ra2engine::SbRect> t;
        for (const ShellButton& b : buttons())
            if (b.enabled) t["button:" + b.label] = b.rect;
        if (confirm_.open()) return t;
        for (const mpui::Edit& e : edits_)
            if (e.enabled) t["edit:" + e.id] = e.r;
        body_targets(t);
        return t;
    }
    void set_focus(int i) {
        if (i == focus_) return;
        if (i >= 0 && focus_ < 0) {
            text_was_active_ = SDL_IsTextInputActive();
            const ra2engine::SbRect& r = edits_[static_cast<std::size_t>(i)].r;
            const SDL_Rect d = xform().dev(r.x, r.y, r.w, r.h);
            set_text_input_rect_logical(d.x, d.y, d.w, d.h);   // keep the edit above the soft keyboard
            SDL_StartTextInput();
        } else if (i < 0 && focus_ >= 0 && !text_was_active_) {
            SDL_StopTextInput();
        }
        focus_ = i;
    }
    void play_ui(const char* key) const {
        if (art_.audio && art_.audio->sys.enabled && art_.audio->sys.mixer) art_.audio->sys.mixer->play(key);
    }
    void ask(int action, const std::string& text, std::vector<std::string> buttons) {
        confirm_action_ = action;
        confirm_.ask(text, std::move(buttons));
    }
    // A wrapped multi-line text block.
    int text_block(const ShellPainter& p, const std::string& s, int x, int y, int w, int max_lines = 99) const {
        int n = 0;
        for (const std::string& ln : shell_wrap(p, s, w)) {
            if (n++ >= max_lines) break;
            p.label(ln, x, y, w, 16, 0);
            y += 16;
        }
        return y;
    }

    const Rules& rules_;
    bool no_audio_ = false;
    const MenuArt& art_;
    std::vector<mpui::Edit> edits_;
    int focus_ = -1;
    bool text_was_active_ = true;
    std::string status_;
    GfConfirm confirm_;
    int confirm_action_ = 0;
    Scene* next_ = nullptr;
    int mx_ = -1, my_ = -1;
    bool down_ = false;
    double clock_ = 0.0;
};

// ===========================================================================
// The multiplayer lobby (built from the Skirmish page: 8 player rows with
// country / colour combos, the same options + sliders, map choice with the
// preview well) + Team / Start columns, slot kinds, ready, lobby chat.
// ===========================================================================
class MpLobbyScene : public MpPage {
public:
    enum Role { HOST, CLIENT };
    enum Net { LAN, ONLINE };

    // LAN host / online host.
    static MpLobbyScene* host(const Rules& r, bool na, const MenuArt& a, Net net,
                              std::unique_ptr<ra2mp::ServerLink> link = nullptr, bool priv = false) {
        auto* s = new MpLobbyScene(r, na, a, HOST, net);
        s->link_ = std::move(link);
        s->private_ = priv;
        s->start_host();
        return s;
    }
    // LAN join (ip:port) / online join (tunnel + host v3 id).
    static MpLobbyScene* join_lan(const Rules& r, bool na, const MenuArt& a, const ra2mp::Addr& host) {
        auto* s = new MpLobbyScene(r, na, a, CLIENT, LAN);
        s->start_client_lan(host);
        return s;
    }
    static MpLobbyScene* join_online(const Rules& r, bool na, const MenuArt& a, std::unique_ptr<ra2mp::ServerLink> link,
                                     std::uint32_t my_v3, std::uint32_t host_v3, const std::string& code) {
        auto* s = new MpLobbyScene(r, na, a, CLIENT, ONLINE);
        s->link_ = std::move(link);
        s->code_ = code;
        s->start_client_online(my_v3, host_v3);
        return s;
    }
    ~MpLobbyScene() override {
        // NET-PLAY: after the hand-off the peers are in the match on the same
        // socket -- a SIGN_OFF now would drop them from it.
        if (host_ && !handed_off_) host_->close();
        if (client_ && client_->state() == ra2mp::LobbyClient::State::Joined) client_->leave();
        if (link_ && role_ == HOST) link_->close_room();
    }
    const char* name() const override { return "mp-lobby"; }

private:
    enum Act { A_NONE, A_START, A_MAP, A_BACK, A_READY, A_USEMAP, A_CANCEL };
    enum Combo { CB_NONE, CB_KIND, CB_COUNTRY, CB_COLOR, CB_TEAM, CB_START };
    // NET-PLAY: CK_AIT (a leaver's house -> AI) and CK_XP (cross-platform, experimental).
    enum Check { CK_SHORT, CK_MCV, CK_CRATES, CK_SW, CK_AIT, CK_XP, CK_COUNT };
    enum Slider { SL_SPEED, SL_CREDITS, SL_UNITS, SL_COUNT };
    enum Confirm { C_LEAVE = 1, C_INFO };

    MpLobbyScene(const Rules& r, bool na, const MenuArt& a, Role role, Net net) : MpPage(r, na, a), role_(role), net_(net) {
        prefs_ = mpui::Prefs::load();
        countries_ = gf_lobby_countries();
        text_.load(countries_);
        auto ri = [&](const char* k, int d) { return static_cast<int>(r.section_double("MultiplayerDialogSettings", k, d)); };
        min_money_ = ri("MinMoney", 5000);
        max_money_ = std::max(min_money_, ri("MaxMoney", 10000));
        money_step_ = std::max(1, ri("MoneyIncrement", 100));
        min_units_ = ri("MinUnitCount", 0);
        max_units_ = std::max(min_units_, ri("MaxUnitCount", 10));
        defaults_.credits = std::max(min_money_, std::min(max_money_, ri("Money", 10000)));
        defaults_.unit_count = std::max(min_units_, std::min(max_units_, ri("UnitCount", 10)));
        defaults_.game_speed = std::max(0, std::min(6, ri("GameSpeed", 1)));
        defaults_.tech_level = ri("TechLevel", 10);
        try {
            catalog_ = std::make_unique<ra2engine::MapCatalog>(ra2engine::load_yr_map_catalog(env_install_dir(), "standard"));
            ra2engine::filter_lobby_theaters(*catalog_);
        } catch (const std::exception&) { catalog_.reset(); }
        edits_.push_back(mpui::Edit{"chat", {30, 432, 440, 19}, "", 100, true});
    }

    // --- setup ---------------------------------------------------------------
    void start_host() {
        ra2mp::MpLobbyModel m;
        m.opt = defaults_;
        const int mi = maps().empty() ? -1 : 0;
        m.map = map_info(mi);
        for (int i = std::max(1, m.map.max_players); i < ra2mp::kSlots; ++i) m.slots[i].kind = ra2mp::SlotKind::Closed;
        if (net_ == LAN) {
            auto t = std::make_shared<ra2mp::MpNet>();
            if (!t->open_direct(prefs_.lan_port) && !t->open_direct(0)) status_ = "Could not open a network port.";
            port_ = t->port();
            transport_ = t;
        } else {
            my_v3_ = mpui::random_u32();
            auto t = std::make_shared<ra2mp::MpNet>();
            t->open_tunnel(link_ ? link_->tunnel_addr() : ra2mp::Addr{}, my_v3_);
            transport_ = t;
        }
        host_ = std::make_unique<ra2mp::LobbyHost>(*transport_, mpui::identity(prefs_), m);
        host_->set_room_name(prefs_.nick + "'s game");
        host_->on_event = [](const std::string& s) { mpui::log_line(s); };
        if (net_ == LAN) {
            ra2mp::LanBeacon b;
            b.rules_crc = mpui::rules_crc();
            b.game_port = port_;
            b.nonce = mpui::random_u32();
            b.room = host_->room_name();
            b.host = prefs_.nick;
            b.version = RA2_VERSION;
            announcer_.open(b);
            // The address other players type into "Join by IP" (first LAN address).
            const std::vector<std::uint32_t> ips = ra2mp::local_ipv4s();
            where_ = ra2mp::Addr{ips.empty() ? ra2mp::kLoopback : ips.front(), port_}.str();
            mpui::log_line("hosting LAN game on port " + std::to_string(port_));
        } else if (link_) {
            link_->create(host_->room_name(), host_->model().map.max_players, private_, false, map_display(), my_v3_);
            where_ = "Creating room...";
        }
        map_ix_ = mi;
    }
    void start_client_lan(const ra2mp::Addr& host) {
        auto t = std::make_shared<ra2mp::MpNet>();
        t->open_direct(0);
        transport_ = t;
        client_ = std::make_unique<ra2mp::LobbyClient>(*transport_, ra2mp::peer_of(host), mpui::identity(prefs_));
        where_ = "Host " + host.str();
        mpui::log_line("joining LAN game at " + host.str());
        wire_client();
    }
    void start_client_online(std::uint32_t my_v3, std::uint32_t host_v3) {
        my_v3_ = my_v3;
        auto t = std::make_shared<ra2mp::MpNet>();
        t->open_tunnel(link_ ? link_->tunnel_addr() : ra2mp::Addr{}, my_v3);
        transport_ = t;
        client_ = std::make_unique<ra2mp::LobbyClient>(*transport_, ra2mp::peer_of_v3(host_v3), mpui::identity(prefs_));
        host_v3_ = host_v3;
        where_ = "Room " + code_;
        mpui::log_line("joining room " + code_ + " via tunnel " + (link_ ? link_->tunnel_addr().str() : std::string("?")));
        wire_client();
    }
    void wire_client() {
        client_->have_map = [this](const ra2mp::MpMap& m) { return local_map_matches(m); };
    }

    // --- model access ----------------------------------------------------------
    ra2mp::LobbyBase& lobby() { return host_ ? static_cast<ra2mp::LobbyBase&>(*host_) : *client_; }
    const ra2mp::LobbyBase& lobby() const { return host_ ? static_cast<const ra2mp::LobbyBase&>(*host_) : *client_; }
    const ra2mp::MpLobbyModel& model() const { return lobby().model(); }
    int me() const { return lobby().local_slot(); }
    bool is_host() const { return role_ == HOST; }
    bool joined() const { return host_ || (client_ && client_->state() == ra2mp::LobbyClient::State::Joined); }
    // Who may edit row k's country/colour/team/start.
    bool can_edit(int k) const {
        if (!joined()) return false;
        const ra2mp::MpSlot& s = model().slots[k];
        if (!s.occupied()) return false;
        return is_host() ? (k == 0 || s.kind == ra2mp::SlotKind::AI) : k == me();
    }

    // --- map catalog -----------------------------------------------------------
    const std::vector<ra2engine::MapEntry>& maps() const {
        static const std::vector<ra2engine::MapEntry> kEmpty;
        return catalog_ ? catalog_->entries : kEmpty;
    }
    std::string map_text(int i) const {
        if (!catalog_ || i < 0 || i >= static_cast<int>(maps().size())) return {};
        try {
            if (const ra2assets::MixEntry* e = catalog_->maps.find(maps()[static_cast<std::size_t>(i)].file_name)) {
                const std::vector<uint8_t> raw = catalog_->maps.read(*e);
                return std::string(raw.begin(), raw.end());
            }
        } catch (const std::exception&) {}
        return {};
    }
    ra2mp::MpMap map_info(int i) const {
        ra2mp::MpMap m;
        if (i < 0 || i >= static_cast<int>(maps().size())) return m;
        const ra2engine::MapEntry& e = maps()[static_cast<std::size_t>(i)];
        m.file = e.file_name;
        m.max_players = (e.max_players > 1) ? std::min(8, e.max_players) : 8;
        const std::string text = map_text(i);
        m.size = static_cast<std::uint32_t>(text.size());
        m.digest = ra2mp::normalize_digest(ra2mp::map_digest_of(text));
        m.official = true;
        return m;
    }
    int map_index_of(const std::string& file) const {
        for (int i = 0; i < static_cast<int>(maps().size()); ++i)
            if (maps()[static_cast<std::size_t>(i)].file_name == file) return i;
        return -1;
    }
    // [Digest] check (01 §7.1): a local map with the same file and digest.
    // Built-in maps are never transferred (map download is a follow-up).
    bool local_map_matches(const ra2mp::MpMap& m) {
        const auto it = digest_cache_.find(m.file);
        std::string d;
        if (it != digest_cache_.end()) d = it->second;
        else {
            const int i = map_index_of(m.file);
            d = i < 0 ? std::string("\x01missing") : ra2mp::normalize_digest(ra2mp::map_digest_of(map_text(i)));
            digest_cache_[m.file] = d;
        }
        return d != "\x01missing" && d == m.digest;
    }
    std::string map_display() const {
        const int i = map_index_of(model().map.file);
        return i >= 0 ? maps()[static_cast<std::size_t>(i)].display_name : model().map.file;
    }

    // --- labels ---------------------------------------------------------------
    std::string kind_label(int k) const {
        const ra2mp::MpSlot& s = model().slots[k];
        switch (s.kind) {
            case ra2mp::SlotKind::Open: return "Open";
            case ra2mp::SlotKind::Closed: return "Closed";
            case ra2mp::SlotKind::AI: return s.ai == ra2engine::kLobbyAiBrutal ? text_.ai[3]
                                             : s.ai == ra2engine::kLobbyAiMedium ? text_.ai[2] : text_.ai[1];
            default: return s.name;
        }
    }
    std::string country_label(int c) const {
        return (c >= 0 && c < static_cast<int>(text_.country.size())) ? text_.country[static_cast<std::size_t>(c)]
                                                                      : text_.random_country;
    }
    std::string color_label(int c) const { return (c >= 0 && c < 8) ? text_.color_abbrev[c] : text_.random_color; }
    static std::string team_label(int t) { return t < 0 ? "-" : std::string(1, static_cast<char>('A' + t)); }
    static std::string start_label(int s) { return s < 0 ? "?" : std::to_string(s + 1); }

    // --- layout (logical 640x480) -----------------------------------------------
    static int rowY(int k) { return 17 + 26 * k; }
    static ra2engine::SbRect kind_rect(int k) { return {30, rowY(k), 140, 19}; }
    static ra2engine::SbRect country_rect(int k) { return {176, rowY(k), 110, 19}; }
    static ra2engine::SbRect color_rect(int k) { return {292, rowY(k), 52, 19}; }
    static ra2engine::SbRect team_rect(int k) { return {350, rowY(k), 42, 19}; }
    static ra2engine::SbRect start_rect(int k) { return {398, rowY(k), 42, 19}; }
    static ra2engine::SbRect ready_rect(int k) { return {446, rowY(k) + 1, 18, 18}; }
    static ra2engine::SbRect check_rect(int c) {
        return c < CK_AIT ? ra2engine::SbRect{30, 232 + 20 * c, 150, 16}
                          : ra2engine::SbRect{170, 232 + 20 * (c - CK_AIT), 125, 16};
    }
    static ra2engine::SbRect slider_rect(int s) { return {300, 230 + 26 * s, 127, 21}; }
    static ra2engine::SbRect slider_label(int s) { return {190, slider_rect(s).y, 105, 16}; }
    static ra2engine::SbRect chat_rect() { return {30, 316, 440, 112}; }
    static ra2engine::SbRect preview_rect() { return {486, 37, 144, 112}; }
    static ra2engine::SbRect map_list_rect() { return {30, 60, 440, 340}; }
    int visible_rows() const { return std::max(1, std::min(ra2mp::kSlots, model().map.max_players)); }

    int slider_range(int s) const {
        if (s == SL_SPEED) return 6;
        if (s == SL_CREDITS) return (max_money_ - min_money_) / money_step_;
        return max_units_ - min_units_;
    }
    int slider_pos(int s) const {
        const ra2mp::MpOptions& o = model().opt;
        if (s == SL_SPEED) return 6 - o.game_speed;
        if (s == SL_CREDITS) return (o.credits - min_money_) / money_step_;
        return o.unit_count - min_units_;
    }
    void set_slider(int s, int pos) {
        if (!host_) return;
        pos = std::max(0, std::min(slider_range(s), pos));
        ra2mp::MpOptions o = model().opt;
        if (s == SL_SPEED) o.game_speed = 6 - pos;
        else if (s == SL_CREDITS) o.credits = min_money_ + pos * money_step_;
        else o.unit_count = min_units_ + pos;
        if (o.game_speed != model().opt.game_speed || o.credits != model().opt.credits || o.unit_count != model().opt.unit_count)
            host_->set_options(o);
    }
    std::string slider_text(int s) const {
        if (s == SL_CREDITS) return std::to_string(model().opt.credits);
        if (s == SL_UNITS) return std::to_string(model().opt.unit_count);
        return std::to_string(slider_pos(s));
    }
    bool check_val(int c) const {
        const ra2mp::MpOptions& o = model().opt;
        return c == CK_SHORT ? o.short_game : c == CK_MCV ? o.mcv_repacks : c == CK_CRATES ? o.crates
             : c == CK_AIT ? o.ai_takeover : c == CK_XP ? o.cross_platform : o.superweapons;
    }
    std::string check_label(int c) const {
        if (c == CK_AIT) return "AI takes over";
        if (c == CK_XP) return "Cross-platform (exp.)";
        return c == CK_SHORT ? text_.short_game : c == CK_MCV ? text_.mcv : c == CK_CRATES ? text_.crates : text_.sw;
    }
    static const char* check_id(int c) {
        static const char* k[CK_COUNT] = {"short", "mcv", "crates", "superweapons", "aitakeover", "crossplatform"};
        return k[c];
    }
    static const char* slider_id(int s) {
        static const char* k[SL_COUNT] = {"speed", "credits", "units"};
        return k[s];
    }

    // --- buttons ----------------------------------------------------------------
    std::vector<ShellButton> buttons() const override {
        if (choosing_) return {btn(0, text_.use_map, true, A_USEMAP), btn(5, text_.cancel, true, A_CANCEL)};
        if (is_host())
            return {btn(1, text_.start, joined(), A_START), btn(2, text_.choose, joined(), A_MAP),
                    btn(5, text_.back, true, A_BACK)};
        const bool ready = client_ && client_->ready();
        return {btn(1, ready ? "Not Ready" : "Ready", joined(), A_READY), btn(5, text_.back, true, A_BACK)};
    }
    std::string header_text() const override {
        if (choosing_) return text_.choose;
        return net_ == LAN ? "Network Game" : "Internet Game";
    }
    int back_action() const override { return choosing_ ? A_CANCEL : A_BACK; }

    void press(int act) override {
        switch (act) {
            case A_START: {
                if (!host_) return;
                const ra2mp::StartCheck c = host_->start(start_seed());
                if (c != ra2mp::StartCheck::Ok) ask(C_INFO, ra2mp::start_check_text(c), {text_.ok});
                else mpui::log_line("start requested seed=" + std::to_string(host_->model().seed));
                return;
            }
            case A_MAP:
                choosing_ = true;
                map_list_ = ShellList{};
                map_list_.r = map_list_rect();
                for (int i = 0; i < static_cast<int>(maps().size()); ++i) {
                    map_list_.items.push_back(maps()[static_cast<std::size_t>(i)].display_name);
                    map_list_.data.push_back(i);
                }
                map_list_.visible = 21;
                map_list_.sel = std::max(0, map_index_of(model().map.file));
                map_list_.reveal(map_list_.sel);
                return;
            case A_USEMAP:
                if (host_ && map_list_.sel >= 0) {
                    map_ix_ = map_list_.sel;
                    host_->set_map(map_info(map_ix_));
                    if (link_) link_->update_room(ra2mp::player_count(model()), model().map.max_players, map_display(), false);
                }
                choosing_ = false;
                return;
            case A_CANCEL: choosing_ = false; return;
            case A_READY:
                if (client_) client_->set_ready(!client_->ready());
                return;
            case A_BACK: leave(); return;
            default: return;
        }
    }
    std::uint32_t start_seed() const {
        // Host-chosen seed: wall clock + player id (any value works; every
        // machine receives it in GO and seeds identically from it).
        return static_cast<std::uint32_t>(ra2mp::now_ms() * 2654435761ull) ^ static_cast<std::uint32_t>(prefs_.pid) ^
               gf_seed_of(model().map.file);
    }
    void leave() {
        announcer_.close();
        if (host_) host_->close();
        if (client_) client_->leave();
        if (link_ && role_ == HOST) link_->close_room();
        next_ = make_return_page();
    }
    Scene* make_return_page();   // defined after the browser pages

    // --- combos -------------------------------------------------------------------
    void open_combo(Combo kind, int row) {
        combo_ = kind;
        combo_row_ = row;
        list_ = ShellList{};
        const ra2mp::MpSlot& s = model().slots[row];
        ra2engine::SbRect face = kind == CB_KIND ? kind_rect(row) : kind == CB_COUNTRY ? country_rect(row)
                                 : kind == CB_COLOR ? color_rect(row) : kind == CB_TEAM ? team_rect(row) : start_rect(row);
        int cur = 0;
        auto add = [&](const std::string& label, int v, bool sel) {
            list_.items.push_back(label);
            list_.data.push_back(v);
            if (sel) cur = static_cast<int>(list_.items.size()) - 1;
        };
        switch (kind) {
            case CB_KIND:   // data: -1 open, -2 closed, 0..2 AI level
                add("Open", -1, s.kind == ra2mp::SlotKind::Open);
                add("Closed", -2, s.kind == ra2mp::SlotKind::Closed);
                add(text_.ai[1], ra2engine::kLobbyAiEasy, s.kind == ra2mp::SlotKind::AI && s.ai == ra2engine::kLobbyAiEasy);
                add(text_.ai[2], ra2engine::kLobbyAiMedium, s.kind == ra2mp::SlotKind::AI && s.ai == ra2engine::kLobbyAiMedium);
                add(text_.ai[3], ra2engine::kLobbyAiBrutal, s.kind == ra2mp::SlotKind::AI && s.ai == ra2engine::kLobbyAiBrutal);
                list_.visible = 5;
                face.w = 150;
                break;
            case CB_COUNTRY:
                add(text_.random_country, ra2mp::kRandom, s.country < 0);
                for (int c = 0; c < static_cast<int>(countries_.size()); ++c) add(country_label(c), c, c == s.country);
                list_.visible = 7;
                break;
            case CB_COLOR:
                add(text_.random_color, ra2mp::kRandom, s.color < 0);
                list_.swatch.push_back(SDL_Color{0, 0, 0, 0});
                for (int c = 0; c < 8; ++c) {
                    if (ra2mp::color_owner(model(), c, row) >= 0) continue;
                    add(color_label(c), c, c == s.color);
                    Uint8 r, g, b;
                    ra2engine::lobby_color_rgb(c, r, g, b);
                    list_.swatch.push_back(SDL_Color{r, g, b, 255});
                }
                list_.visible = 9;
                face.w += 22;
                break;
            case CB_TEAM:
                for (int t = -1; t < 4; ++t) add(team_label(t), t, t == s.team);
                list_.visible = 5;
                break;
            case CB_START:
                add("?", ra2mp::kRandomStart, s.start < 0);
                for (int st = 0; st < model().map.max_players; ++st)
                    if (ra2mp::start_owner(model(), st, row) < 0) add(start_label(st), st, st == s.start);
                list_.visible = 9;
                break;
            default: break;
        }
        list_.sel = cur;
        const int h = list_.rows() * list_.item_h + 2;
        int y = face.y + face.h;
        if (y + h > 448) y = face.y - h;
        list_.r = {face.x, y, face.w, h};
        list_.reveal(cur);
        play_ui("menu:acbopen");
    }
    void close_combo(bool commit) {
        if (combo_ == CB_NONE) return;
        if (commit && list_.sel >= 0 && list_.sel < static_cast<int>(list_.data.size())) {
            const int v = list_.data[static_cast<std::size_t>(list_.sel)];
            const int k = combo_row_;
            const ra2mp::MpSlot& s = model().slots[k];
            if (combo_ == CB_KIND && host_) {
                host_->set_slot_kind(k, v == -1 ? ra2mp::SlotKind::Open : v == -2 ? ra2mp::SlotKind::Closed : ra2mp::SlotKind::AI,
                                     v >= 0 ? v : 1);
            } else {
                int country = s.country, color = s.color, team = s.team, start = s.start;
                if (combo_ == CB_COUNTRY) country = v;
                if (combo_ == CB_COLOR) color = v;
                if (combo_ == CB_TEAM) team = v;
                if (combo_ == CB_START) start = v;
                if (host_) host_->set_slot(k, country, color, team, start);
                else if (client_) client_->request_slot(country, color, team, start);
            }
        }
        combo_ = CB_NONE;
        play_ui("menu:acbclose");
    }

    // --- input ---------------------------------------------------------------------
    bool body_modal(int lx, int ly) override {
        if (choosing_) {
            if (map_list_.scrolls() && map_list_.strip_up().contains(lx, ly)) { map_list_.scroll(-map_list_.visible); return true; }
            if (map_list_.scrolls() && map_list_.strip_down().contains(lx, ly)) { map_list_.scroll(map_list_.visible); return true; }
            const int i = map_list_.hit(lx, ly);
            if (i >= 0) { map_list_.sel = i; play_ui("menu:click"); return true; }
            return false;
        }
        if (combo_ == CB_NONE) return false;
        if (list_.scrolls() && list_.strip_up().contains(lx, ly)) { list_.scroll(-1); return true; }
        if (list_.scrolls() && list_.strip_down().contains(lx, ly)) { list_.scroll(+1); return true; }
        const int i = list_.hit(lx, ly);
        if (i >= 0) { list_.sel = i; close_combo(true); }
        else close_combo(false);
        return true;
    }
    void body_click(int lx, int ly) override {
        if (choosing_ || !joined()) return;
        for (int k = 0; k < visible_rows(); ++k) {
            if (is_host() && k > 0 && model().slots[k].kind != ra2mp::SlotKind::Human && kind_rect(k).contains(lx, ly)) {
                open_combo(CB_KIND, k);
                return;
            }
            if (!can_edit(k)) continue;
            if (country_rect(k).contains(lx, ly)) { open_combo(CB_COUNTRY, k); return; }
            if (color_rect(k).contains(lx, ly)) { open_combo(CB_COLOR, k); return; }
            if (team_rect(k).contains(lx, ly)) { open_combo(CB_TEAM, k); return; }
            if (start_rect(k).contains(lx, ly)) { open_combo(CB_START, k); return; }
        }
        // Host: a click on a guest's name offers to remove them.
        if (is_host())
            for (int k = 1; k < visible_rows(); ++k)
                if (model().slots[k].kind == ra2mp::SlotKind::Human && kind_rect(k).contains(lx, ly)) {
                    kick_row_ = k;
                    ask(C_LEAVE, "Remove " + model().slots[k].name + " from the game?", {text_.ok, text_.cancel});
                    return;
                }
        if (!host_) return;
        for (int c = 0; c < CK_COUNT; ++c)
            if (check_rect(c).contains(lx, ly)) {
                ra2mp::MpOptions o = model().opt;
                bool* f = c == CK_SHORT ? &o.short_game : c == CK_MCV ? &o.mcv_repacks : c == CK_CRATES ? &o.crates
                        : c == CK_AIT ? &o.ai_takeover : c == CK_XP ? &o.cross_platform : &o.superweapons;
                *f = !*f;
                host_->set_options(o);
                play_ui("menu:click");
                return;
            }
        for (int s = 0; s < SL_COUNT; ++s) {
            const ra2engine::SbRect r = slider_rect(s);
            const ra2engine::SbRect track{r.x, r.y, r.w - ra2engine::kTrackbarReadoutW, r.h};
            if (!track.contains(lx, ly)) continue;
            const int tx = r.x + ra2engine::trackbar_thumb_x(slider_pos(s), slider_range(s), r.w, ra2engine::kTrackbarReadoutW);
            if (lx >= tx && lx < tx + ra2engine::kTrackbarThumbW) drag_ = s;
            else set_slider(s, ra2engine::trackbar_pos_from_x(lx - r.x, slider_range(s), r.w, ra2engine::kTrackbarReadoutW));
            return;
        }
    }
    void body_drag(const ra2engine::InputEvent& ev, int lx, int /*ly*/) override {
        if (drag_ < 0) return;
        const ra2engine::SbRect r = slider_rect(drag_);
        set_slider(drag_, ra2engine::trackbar_pos_from_x(lx - r.x, slider_range(drag_), r.w, ra2engine::kTrackbarReadoutW));
        if (ev.kind == ra2engine::InputEvent::PointerUp) drag_ = -1;
    }
    bool body_key(const ra2engine::InputEvent& ev) override {
        if (combo_ != CB_NONE) {
            if (ev.key == ra2engine::kKeyEscape) close_combo(false);
            else if (ev.key == ra2engine::kKeyEnter) close_combo(true);
            else if (ev.key == ra2engine::kKeyUp && list_.sel > 0) list_.reveal(--list_.sel);
            else if (ev.key == ra2engine::kKeyDown && list_.sel + 1 < static_cast<int>(list_.items.size())) list_.reveal(++list_.sel);
            return true;
        }
        if (choosing_ && ev.key == ra2engine::kKeyEnter) { press(A_USEMAP); return true; }
        return false;
    }
    void on_enter(int edit) override {
        if (edits_[static_cast<std::size_t>(edit)].id == "chat") {
            const std::string t = edits_[static_cast<std::size_t>(edit)].text;
            edits_[static_cast<std::size_t>(edit)].text.clear();
            if (!t.empty()) {
                if (host_) host_->chat(t);
                else if (client_) client_->chat(t);
            }
        }
    }
    void confirm_done(int action, int button) override {
        if (action == C_LEAVE && button == 0 && host_ && kick_row_ > 0) host_->kick(kick_row_);
        if (action == C_INFO && client_ && (client_->state() == ra2mp::LobbyClient::State::Rejected ||
                                             client_->state() == ra2mp::LobbyClient::State::Lost))
            leave();
    }

    // --- per frame -----------------------------------------------------------------
    void tick(std::uint64_t now) override {
        if (host_) host_->update(now);
        if (client_) client_->update(now);
        announcer_.update(now);
        if (announcer_.is_open() && host_) {
            ra2mp::LanBeacon b = announcer_.beacon();
            b.players = static_cast<std::uint8_t>(ra2mp::player_count(model()));
            b.max = static_cast<std::uint8_t>(model().map.max_players);
            b.map = map_display();
            b.started = host_->started();
            announcer_.set(b);
        }
        if (link_) {
            link_->update(now);
            while (!link_->events().empty()) {
                const ra2mp::ServerEvent ev = link_->events().front();
                link_->events().pop_front();
                if (ev.kind == ra2mp::ServerEvent::Created) {
                    code_ = ev.code;
                    where_ = "Room code " + code_ + (private_ ? " (private)" : "");
                    lobby().system_line("Room code: " + code_ + "  -- give it to your friends.");
                    mpui::log_line("room created code=" + code_);
                } else if (ev.kind == ra2mp::ServerEvent::Peer) {
                    mpui::log_line("server: " + ev.nick + " is joining (tunnel id " + std::to_string(ev.v3) + ")");
                } else if (ev.kind == ra2mp::ServerEvent::RoomClosed && client_) {
                    // The host's control connection dropped; the lobby link may still live.
                    mpui::log_line("server: room closed");
                } else if (ev.kind == ra2mp::ServerEvent::Error) {
                    status_ = "Server: " + ev.op + " failed (" + ev.reason + ")";
                } else if (ev.kind == ra2mp::ServerEvent::Disconnected) {
                    status_ = "Lost the connection to the server (the game lobby continues).";
                }
            }
            if (host_ && link_->state() == ra2mp::ServerLink::State::Ready) {
                const int players = ra2mp::player_count(model());
                if (players != last_players_ || host_->started() != last_started_) {
                    link_->update_room(players, model().map.max_players, map_display(), host_->started());
                    last_players_ = players;
                    last_started_ = host_->started();
                }
            }
        }
        if (client_) {
            const auto st = client_->state();
            if ((st == ra2mp::LobbyClient::State::Rejected || st == ra2mp::LobbyClient::State::Lost) &&
                !confirm_.open() && !told_) {
                told_ = true;
                const char* why = ra2mp::reject_text(client_->reject_reason());
                ask(C_INFO, *why ? why : "You left the game.", {text_.ok});
            }
            if (st == ra2mp::LobbyClient::State::Joining) status_ = "Contacting the host...";
            else if (st == ra2mp::LobbyClient::State::Joined && status_ == "Contacting the host...") status_.clear();
        }
        if (host_ && !confirm_.open()) {
            const ra2mp::StartCheck c = ra2mp::can_start(model());
            status_ = c == ra2mp::StartCheck::Ok ? "Everyone is ready." : ra2mp::start_check_text(c);
        }
        if (lobby().started() && !handed_off_) {
            handed_off_ = true;
            ra2mp::MpSessionConfig s = lobby().session();
            ra2mp::resolve_session(s, static_cast<int>(countries_.size()));
            s.transport = net_ == LAN ? ra2mp::MpTransportKind::Lan : ra2mp::MpTransportKind::Tunnel;
            if (net_ == LAN) s.host_addr = client_ ? transport_->describe(client_->host()) : ("0.0.0.0:" + std::to_string(port_));
            else if (link_) s.host_addr = link_->tunnel_addr().str();
            s.local_v3 = my_v3_;
            s.host_v3 = is_host() ? my_v3_ : host_v3_;
            // (MpPlayer::peer: resolve_session copied GO's peer table.)
            announcer_.close();
            next_ = mp_start_session(rules_, no_audio_, art_, s, transport_);
        }
    }

    // --- drawing ---------------------------------------------------------------------
    SDL_Texture* preview_tex(ra2engine::TextureCache& tc, int map_i) {
        if (map_i < 0 || map_i >= static_cast<int>(maps().size())) return nullptr;
        const std::string& f = maps()[static_cast<std::size_t>(map_i)].file_name;
        const std::string key = "skp:prev:" + f;
        if (void* t = tc.peek(key)) return static_cast<SDL_Texture*>(t);
        if (preview_size_.count(f)) return nullptr;
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;
        skp::decode_preview(map_text(map_i), w, h, rgba);
        preview_size_[f] = {w, h};
        if (w <= 0 || h <= 0) return nullptr;
        return static_cast<SDL_Texture*>(tc.get_rgba(key, rgba.data(), w, h));
    }

    void draw_body(const ShellPainter& p, ra2engine::TextureCache& tc) override {
        edits_[0].enabled = joined() && !choosing_;
        // Preview well + map name + where (LAN address / room code).
        const ra2engine::SbRect pr = preview_rect();
        p.fill(p.dev(pr), 0, 0, 0);
        const int mi = choosing_ ? map_list_.sel : map_index_of(model().map.file);
        if (SDL_Texture* t = preview_tex(tc, mi)) {
            const auto sz = preview_size_[maps()[static_cast<std::size_t>(mi)].file_name];
            const double sc = std::min(double(pr.w - 2) / sz.first, double(pr.h - 2) / sz.second);
            const int w = std::max(1, int(sz.first * sc)), h = std::max(1, int(sz.second * sc));
            const SDL_Rect d = p.dev(pr.x + (pr.w - w) / 2, pr.y + (pr.h - h) / 2, w, h);
            SDL_RenderCopy(p.ren, t, nullptr, &d);
        }
        if (choosing_) {
            p.header(text_.sel_engagement, 34, 32, 385, 19);
            map_list_.draw(p, false);
            return;
        }
        p.label(p.fit(where_, 150), 486, 152, 150, 16, 1);
        p.label(p.fit(map_display(), 150), 486, 170, 150, 16, 1);
        if (client_ && client_->state() == ra2mp::LobbyClient::State::Joined && !model().slots[me()].has_map)
            p.label("MAP MISSING", 486, 186, 150, 12, 1);

        for (int k = 0; k < visible_rows(); ++k) {
            const ra2mp::MpSlot& s = model().slots[k];
            const bool occ = s.occupied();
            const bool kind_en = is_host() && k > 0 && s.kind != ra2mp::SlotKind::Human;
            std::string nm = kind_label(k);
            if (s.kind == ra2mp::SlotKind::Human && k == me()) nm += " (you)";
            if (kind_en) p.combo(kind_rect(k), nm, joined(), combo_ == CB_KIND && combo_row_ == k);
            else {
                p.frame(p.dev(kind_rect(k)), occ);
                p.label(p.fit(nm, 132), kind_rect(k).x + 4, kind_rect(k).y, 132, 19, 0, occ);
            }
            const bool ed = can_edit(k);
            if (!occ) continue;
            p.combo(country_rect(k), country_label(s.country), ed, combo_ == CB_COUNTRY && combo_row_ == k);
            SDL_Color sw{0, 0, 0, 0};
            if (s.color >= 0) {
                Uint8 r, g, b;
                ra2engine::lobby_color_rgb(s.color, r, g, b);
                sw = SDL_Color{r, g, b, 255};
            }
            p.combo(color_rect(k), color_label(s.color), ed, combo_ == CB_COLOR && combo_row_ == k, sw.a ? &sw : nullptr);
            p.combo(team_rect(k), team_label(s.team), ed, combo_ == CB_TEAM && combo_row_ == k);
            p.combo(start_rect(k), start_label(s.start), ed, combo_ == CB_START && combo_row_ == k);
            if (s.kind == ra2mp::SlotKind::Human) {
                const bool lit = s.host || s.ready;
                p.fill(p.dev(ready_rect(k)), lit ? 40 : 60, lit ? 200 : 0, 0);
                p.frame(p.dev(ready_rect(k)), true);
            }
        }
        for (int c = 0; c < CK_COUNT; ++c) p.checkbox(check_rect(c), check_val(c), check_label(c), host_ != nullptr && joined());
        const std::string sl[SL_COUNT] = {text_.speed, text_.credits, text_.units};
        for (int s = 0; s < SL_COUNT; ++s) {
            const ra2engine::SbRect lr = slider_label(s);
            p.label(sl[s], lr.x, lr.y, lr.w, lr.h, 0);
            p.trackbar(slider_rect(s), slider_pos(s), slider_range(s), true, slider_text(s), host_ != nullptr && joined());
        }
        // Chat log (last lines that fit).
        const ra2engine::SbRect cr = chat_rect();
        p.fill(p.dev(cr), 0, 0, 0, 160);
        p.frame(p.dev(cr), true);
        const auto& log = lobby().chat_log();
        const int lines = cr.h / 16;
        int y = cr.y + 2;
        const int first = std::max(0, static_cast<int>(log.size()) - lines);
        for (int i = first; i < static_cast<int>(log.size()); ++i) {
            const ra2mp::ChatLine& l = log[static_cast<std::size_t>(i)];
            const std::string s = l.from.empty() ? l.text : l.from + ": " + l.text;
            p.label(p.fit(s, cr.w - 8), cr.x + 4, y, cr.w - 8, 16, 0);
            y += 16;
        }
    }
    void draw_overlay(const ShellPainter& p) override {
        if (combo_ != CB_NONE) list_.draw(p);
    }

    // --- --play ------------------------------------------------------------------------
    void body_targets(std::map<std::string, ra2engine::SbRect>& t) const override {
        if (choosing_) {
            for (int i = map_list_.top; i < map_list_.top + map_list_.rows(); ++i)
                t["map:" + maps()[static_cast<std::size_t>(i)].file_name] = map_list_.item_rect(i);
            if (map_list_.scrolls()) {
                t["list:up"] = map_list_.strip_up();
                t["list:down"] = map_list_.strip_down();
            }
            return;
        }
        if (combo_ != CB_NONE) {
            for (int i = list_.top; i < list_.top + list_.rows(); ++i) {
                t["item:" + std::to_string(i)] = list_.item_rect(i);
                t["pick:" + list_.items[static_cast<std::size_t>(i)]] = list_.item_rect(i);
            }
            if (list_.scrolls()) {
                t["list:up"] = list_.strip_up();
                t["list:down"] = list_.strip_down();
            }
            return;
        }
        if (!joined()) return;
        for (int k = 0; k < visible_rows(); ++k) {
            const std::string n = std::to_string(k);
            if (is_host() && k > 0) t["combo:kind" + n] = kind_rect(k);
            if (!can_edit(k)) continue;
            t["combo:country" + n] = country_rect(k);
            t["combo:color" + n] = color_rect(k);
            t["combo:team" + n] = team_rect(k);
            t["combo:start" + n] = start_rect(k);
        }
        if (host_) {
            for (int c = 0; c < CK_COUNT; ++c) t[std::string("check:") + check_id(c)] = {check_rect(c).x, check_rect(c).y, 18, 16};
            for (int s = 0; s < SL_COUNT; ++s) {
                const ra2engine::SbRect r = slider_rect(s);
                const int range = slider_range(s);
                for (int pos = 0; pos <= range; ++pos) {
                    std::string v = s == SL_CREDITS ? std::to_string(min_money_ + pos * money_step_)
                                  : s == SL_UNITS ? std::to_string(min_units_ + pos) : std::to_string(pos);
                    t[std::string("slider:") + slider_id(s) + "=" + v] =
                        {r.x + shell_trackbar_x_for(pos, range, r.w, ra2engine::kTrackbarReadoutW), r.y + r.h / 2, 1, 1};
                }
            }
        }
    }
    void body_probe(ra2play::Probe& p) override {
        static const char* kKind[4] = {"Open", "Closed", "Human", "AI"};
        p.set_s("mp_role", is_host() ? "host" : "client");
        p.set_s("mp_net", net_ == LAN ? "lan" : "online");
        std::string st = "joined";
        if (client_) {
            static const char* kSt[5] = {"joining", "joined", "rejected", "lost", "started"};
            st = kSt[static_cast<int>(client_->state())];
            p.set_s("reject_reason", ra2mp::reject_text(client_->reject_reason()));
        }
        if (lobby().started()) st = "started";
        p.set_s("mp_state", st);
        p.set("local_slot", me());
        p.set("members", host_ ? host_->members() : 0);
        p.set("rev", lobby().rev());
        p.set("players", ra2mp::player_count(model()));
        p.set("humans", ra2mp::human_count(model()));
        p.set("max_players", model().map.max_players);
        p.set_s("map", model().map.file);
        p.set_s("map_digest", model().map.digest);
        p.set_s("room_code", code_);
        p.set_s("where", where_);
        p.set("lan_port", port_);
        p.set("credits", model().opt.credits);
        p.set("unit_count", model().opt.unit_count);
        p.set("game_speed", model().opt.game_speed);
        for (int c = 0; c < CK_COUNT; ++c) p.set(std::string("check(") + check_id(c) + ")", check_val(c) ? 1 : 0);
        p.set("can_start", ra2mp::can_start(model()) == ra2mp::StartCheck::Ok ? 1 : 0);
        p.set("choosing", choosing_ ? 1 : 0);
        p.set("map_count", static_cast<double>(maps().size()));
        p.set_s("combo_open", combo_ == CB_NONE ? "" : combo_ == CB_KIND ? "kind" : combo_ == CB_COUNTRY ? "country"
                              : combo_ == CB_COLOR ? "color" : combo_ == CB_TEAM ? "team" : "start");
        for (int k = 0; k < ra2mp::kSlots; ++k) {
            const ra2mp::MpSlot& s = model().slots[k];
            const std::string n = "(" + std::to_string(k) + ")";
            p.set_s("slot_kind" + n, kKind[static_cast<int>(s.kind) & 3]);
            p.set_s("slot_name" + n, s.name);
            p.set("slot_ready" + n, s.ready ? 1 : 0);
            p.set("slot_has_map" + n, s.has_map ? 1 : 0);
            p.set("country" + n, s.country);
            p.set("color" + n, s.color);
            p.set("team" + n, s.team);
            p.set("start" + n, s.start);
            p.set("ai" + n, s.ai);
        }
        const auto& log = lobby().chat_log();
        p.set("chat_count", static_cast<double>(log.size()));
        p.set_s("chat_last", log.empty() ? "" : (log.back().from.empty() ? log.back().text : log.back().from + ": " + log.back().text));
        if (client_) p.set("ready", client_->ready() ? 1 : 0);
    }

    Role role_;
    Net net_;
    mpui::Prefs prefs_;
    std::vector<std::string> countries_;
    skp::Text text_;
    ra2mp::MpOptions defaults_;
    int min_money_ = 5000, max_money_ = 10000, money_step_ = 100, min_units_ = 0, max_units_ = 10;
    std::unique_ptr<ra2engine::MapCatalog> catalog_;
    std::map<std::string, std::string> digest_cache_;
    std::map<std::string, std::pair<int, int>> preview_size_;
    std::shared_ptr<ra2mp::MpNet> transport_;
    std::unique_ptr<ra2mp::LobbyHost> host_;
    std::unique_ptr<ra2mp::LobbyClient> client_;
    std::unique_ptr<ra2mp::ServerLink> link_;
    ra2mp::LanAnnouncer announcer_;
    std::uint16_t port_ = 0;
    std::uint32_t my_v3_ = 0, host_v3_ = 0;
    std::string code_, where_;
    bool private_ = false;
    int map_ix_ = -1;
    bool choosing_ = false;
    ShellList map_list_, list_;
    Combo combo_ = CB_NONE;
    int combo_row_ = 0, drag_ = -1, kick_row_ = -1;
    int last_players_ = -1;
    bool last_started_ = false, told_ = false, handed_off_ = false;
};

// ===========================================================================
// Network (LAN): game list from beacons, Host Game, Join, Join by IP.
// ===========================================================================
class MpLanScene : public MpPage {
public:
    MpLanScene(const Rules& r, bool na, const MenuArt& a) : MpPage(r, na, a) {
        prefs_ = mpui::Prefs::load();
        edits_.push_back(mpui::Edit{"nick", {150, 40, 160, 19}, prefs_.nick, 16, true});
        edits_.push_back(mpui::Edit{"ip", {150, 400, 200, 19}, "", 40, true});
        browser_.rules_crc = mpui::rules_crc();
        if (!browser_.open()) status_ = "Could not open the LAN discovery port.";
        list_.r = {30, 92, 440, 290};
        list_.visible = 18;
        list_.item_h = 16;
    }
    ~MpLanScene() override { browser_.close(); }
    const char* name() const override { return "mp-lan"; }

private:
    enum Act { A_NONE, A_HOST, A_JOIN, A_JOINIP, A_BACK };
    std::vector<ShellButton> buttons() const override {
        return {btn(0, "Host Game", true, A_HOST), btn(1, "Join Game", list_.sel >= 0, A_JOIN),
                btn(2, "Join by IP", !edits_[1].text.empty(), A_JOINIP), btn(5, "Back", true, A_BACK)};
    }
    std::string header_text() const override { return "Network"; }
    int back_action() const override { return A_BACK; }
    void commit_nick() {
        const std::string n = ra2mp::sanitize_name(edits_[0].text);
        if (!n.empty() && n != prefs_.nick) {
            prefs_.nick = n;
            prefs_.save();
        }
    }
    void press(int act) override {
        commit_nick();
        switch (act) {
            case A_HOST:
                browser_.close();
                next_ = MpLobbyScene::host(rules_, no_audio_, art_, MpLobbyScene::LAN);
                return;
            case A_JOIN:
                if (list_.sel >= 0 && list_.sel < static_cast<int>(browser_.games().size())) {
                    const ra2mp::Addr h = browser_.games()[static_cast<std::size_t>(list_.sel)].host;
                    browser_.close();
                    next_ = MpLobbyScene::join_lan(rules_, no_audio_, art_, h);
                }
                return;
            case A_JOINIP: {
                ra2mp::Addr h;
                if (!ra2mp::resolve(edits_[1].text, ra2mp::kLanGamePort, h)) {
                    ask(0, "Cannot find \"" + edits_[1].text + "\". Type the host's address, e.g. 192.168.1.20 or 192.168.1.20:50020.", {"OK"});
                    return;
                }
                browser_.close();
                next_ = MpLobbyScene::join_lan(rules_, no_audio_, art_, h);
                return;
            }
            case A_BACK:
                browser_.close();
                next_ = new ShellScene(rules_, no_audio_, art_, PAGE_MAIN);
                return;
            default: return;
        }
    }
    void on_enter(int edit) override {
        set_focus(-1);
        if (edits_[static_cast<std::size_t>(edit)].id == "ip") press(A_JOINIP);
        else commit_nick();
    }
    void tick(std::uint64_t now) override {
        browser_.update(now);
        const auto& g = browser_.games();
        list_.items.clear();
        list_.data.clear();
        for (std::size_t i = 0; i < g.size(); ++i) {
            const ra2mp::LanBeacon& b = g[i].beacon;
            std::string row = b.room + "  -  " + b.map + "  " + std::to_string(b.players) + "/" + std::to_string(b.max);
            if (b.started) row += "  (started)";
            if (!ra2mp::LanBrowser::compatible(g[i], ra2mp::kNetProto, ra2mp::kSimVersion, mpui::rules_crc())) row += "  (different version)";
            list_.items.push_back(row);
            list_.data.push_back(static_cast<int>(i));
        }
        if (list_.sel >= static_cast<int>(g.size())) list_.sel = -1;
        if (status_.empty() || status_.rfind("Games found", 0) == 0 || status_.rfind("Looking", 0) == 0)
            status_ = g.empty() ? "Looking for games on your network..." : "Games found: " + std::to_string(g.size());
    }
    bool body_modal(int lx, int ly) override {
        if (list_.scrolls() && list_.strip_up().contains(lx, ly)) { list_.scroll(-1); return true; }
        if (list_.scrolls() && list_.strip_down().contains(lx, ly)) { list_.scroll(1); return true; }
        const int i = list_.hit(lx, ly);
        if (i >= 0) { list_.sel = i; play_ui("menu:click"); return true; }
        return false;
    }
    void draw_body(const ShellPainter& p, ra2engine::TextureCache&) override {
        p.label("Your name", 30, 40, 115, 19, 0);
        p.header("Games on your network", 30, 70, 440, 19);
        list_.draw(p);
        p.label("Join by IP", 30, 400, 115, 19, 0);
        p.label("Not listed? Ask the host for the address shown in their lobby.", 30, 424, 440, 16, 0);
        text_block(p, "Players on the same Wi-Fi or wired network see each other here. "
                      "Allow the game through the firewall for private networks.", 486, 40, 144, 8);
    }
    void body_targets(std::map<std::string, ra2engine::SbRect>& t) const override {
        for (int i = list_.top; i < list_.top + list_.rows(); ++i) t["game:" + std::to_string(i)] = list_.item_rect(i);
    }
    void body_probe(ra2play::Probe& p) override {
        p.set("lan_games", static_cast<double>(browser_.games().size()));
        p.set("lan_listening", browser_.listening() ? 1 : 0);
        p.set("lan_sel", list_.sel);
        for (std::size_t i = 0; i < browser_.games().size(); ++i) {
            const auto& g = browser_.games()[i];
            p.set_s("lan_game_host(" + std::to_string(i) + ")", g.host.str());
            p.set_s("lan_game_room(" + std::to_string(i) + ")", g.beacon.room);
        }
        p.set_s("nick", prefs_.nick);
    }

    mpui::Prefs prefs_;
    ra2mp::LanBrowser browser_;
    ShellList list_;
};

// ===========================================================================
// Internet: your server, room list, host (public / private with code),
// join by list or by code.
// ===========================================================================
class MpOnlineScene : public MpPage {
public:
    MpOnlineScene(const Rules& r, bool na, const MenuArt& a) : MpPage(r, na, a) {
        prefs_ = mpui::Prefs::load();
        edits_.push_back(mpui::Edit{"server", {130, 40, 220, 19}, prefs_.server, 64, true});
        edits_.push_back(mpui::Edit{"nick", {130, 64, 160, 19}, prefs_.nick, 16, true});
        edits_.push_back(mpui::Edit{"code", {130, 404, 90, 19}, "", 6, true});
        list_.r = {30, 120, 440, 268};
        list_.visible = 16;
        link_ = std::make_unique<ra2mp::ServerLink>();
        if (!prefs_.server.empty()) connect();
        else status_ = "Type the address of your server (see docs/SERVER.md) and press Connect.";
    }
    const char* name() const override { return "mp-online"; }

private:
    enum Act { A_NONE, A_CONNECT, A_HOST, A_HOSTPRIV, A_JOIN, A_JOINCODE, A_BACK };
    bool ready() const { return link_ && link_->state() == ra2mp::ServerLink::State::Ready; }
    std::vector<ShellButton> buttons() const override {
        return {btn(0, "Connect", !edits_[0].text.empty(), A_CONNECT), btn(1, "Host Game", ready(), A_HOST),
                btn(2, "Host Private", ready(), A_HOSTPRIV), btn(3, "Join Game", ready() && list_.sel >= 0, A_JOIN),
                btn(4, "Join Code", ready() && edits_[2].text.size() == 6, A_JOINCODE), btn(5, "Back", true, A_BACK)};
    }
    std::string header_text() const override { return "Internet"; }
    int back_action() const override { return A_BACK; }
    void connect() {
        prefs_.server = edits_[0].text;
        const std::string n = ra2mp::sanitize_name(edits_[1].text);
        if (!n.empty()) prefs_.nick = n;
        prefs_.save();
        ra2mp::ServerHello h;
        h.ver = RA2_VERSION;
        h.sim = ra2mp::kSimVersion;
        h.proto = ra2mp::kNetProto;
        h.rules = mpui::rules_crc();
        h.plat = mpui::platform_code();
        h.pid = prefs_.pid;
        h.nick = prefs_.nick;
        status_ = "Connecting to " + prefs_.server + "...";
        if (!link_->connect(prefs_.server, h)) status_ = link_->error();
        mpui::log_line("connecting to server " + prefs_.server);
    }
    void join(const std::string& code) {
        if (!ready()) return;
        pending_v3_ = mpui::random_u32();
        pending_code_ = code;
        for (char& c : pending_code_) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        link_->join(pending_code_, pending_v3_);
        status_ = "Joining room " + pending_code_ + "...";
    }
    void press(int act) override {
        switch (act) {
            case A_CONNECT: connect(); return;
            case A_HOST:
            case A_HOSTPRIV:
                next_ = MpLobbyScene::host(rules_, no_audio_, art_, MpLobbyScene::ONLINE, std::move(link_), act == A_HOSTPRIV);
                return;
            case A_JOIN:
                if (list_.sel >= 0 && list_.sel < static_cast<int>(link_->rooms().size()))
                    join(link_->rooms()[static_cast<std::size_t>(list_.sel)].code);
                return;
            case A_JOINCODE: join(edits_[2].text); return;
            case A_BACK:
                next_ = new ShellScene(rules_, no_audio_, art_, PAGE_MAIN);
                return;
            default: return;
        }
    }
    void on_enter(int edit) override {
        set_focus(-1);
        const std::string& id = edits_[static_cast<std::size_t>(edit)].id;
        if (id == "server") connect();
        else if (id == "code" && edits_[2].text.size() == 6) join(edits_[2].text);
    }
    void tick(std::uint64_t now) override {
        if (!link_) return;
        link_->update(now);
        while (!link_->events().empty()) {
            const ra2mp::ServerEvent ev = link_->events().front();
            link_->events().pop_front();
            switch (ev.kind) {
                case ra2mp::ServerEvent::HelloOk:
                    status_ = "Connected to " + link_->server_name() + ".";
                    mpui::log_line("connected: " + link_->server_name());
                    link_->list();
                    last_list_ = now;
                    break;
                case ra2mp::ServerEvent::Rejected: status_ = link_->error(); break;
                case ra2mp::ServerEvent::Joined:
                    mpui::log_line("joined room " + ev.code + " host tunnel id " + std::to_string(ev.host_v3));
                    next_ = MpLobbyScene::join_online(rules_, no_audio_, art_, std::move(link_), pending_v3_, ev.host_v3, ev.code);
                    return;
                case ra2mp::ServerEvent::Error: {
                    static const std::map<std::string, std::string> kWhy = {
                        {"no_room", "No room with that code."}, {"full", "That game is full."},
                        {"started", "That game has already started."},
                        {"version", "That game uses a different game version or game data."},
                        {"rate", "Too many attempts: wait a minute."}, {"busy", "The server is busy."}};
                    const auto it = kWhy.find(ev.reason);
                    status_ = it != kWhy.end() ? it->second : "The server said: " + ev.reason;
                    break;
                }
                case ra2mp::ServerEvent::Disconnected: status_ = link_->error(); break;
                default: break;
            }
        }
        if (link_->state() == ra2mp::ServerLink::State::Failed && !link_->error().empty()) status_ = link_->error();
        if (ready() && now - last_list_ >= 5000) {
            link_->list();
            last_list_ = now;
        }
        list_.items.clear();
        list_.data.clear();
        for (const ra2mp::ServerRoom& r : link_->rooms()) {
            std::string row = r.name + "  -  " + r.map + "  " + std::to_string(r.players) + "/" + std::to_string(r.max);
            if (r.started) row += "  (started)";
            list_.items.push_back(row);
            list_.data.push_back(0);
        }
        if (list_.sel >= static_cast<int>(list_.items.size())) list_.sel = -1;
    }
    bool body_modal(int lx, int ly) override {
        if (list_.scrolls() && list_.strip_up().contains(lx, ly)) { list_.scroll(-1); return true; }
        if (list_.scrolls() && list_.strip_down().contains(lx, ly)) { list_.scroll(1); return true; }
        const int i = list_.hit(lx, ly);
        if (i >= 0) { list_.sel = i; play_ui("menu:click"); return true; }
        return false;
    }
    void draw_body(const ShellPainter& p, ra2engine::TextureCache&) override {
        p.label("Server", 30, 40, 95, 19, 0);
        p.label("Your name", 30, 64, 95, 19, 0);
        if (link_ && ready()) p.label(p.fit(link_->motd(), 440), 30, 90, 440, 16, 0);
        p.header("Public games", 30, 104, 440, 16);
        list_.draw(p);
        p.label("Room code", 30, 404, 95, 19, 0);
        p.label("Private games are joined by their 6-letter code.", 230, 404, 240, 19, 0);
        text_block(p, "Internet games run through your own server. All traffic is relayed: players never see each "
                      "other's IP address.", 486, 40, 144, 8);
    }
    void body_targets(std::map<std::string, ra2engine::SbRect>& t) const override {
        for (int i = list_.top; i < list_.top + list_.rows(); ++i) t["room:" + std::to_string(i)] = list_.item_rect(i);
    }
    void body_probe(ra2play::Probe& p) override {
        static const char* kSt[6] = {"idle", "resolving", "connecting", "hello", "ready", "failed"};
        p.set_s("online_state", link_ ? kSt[static_cast<int>(link_->state())] : "moved");
        p.set("rooms", link_ ? static_cast<double>(link_->rooms().size()) : 0.0);
        if (link_) {
            for (std::size_t i = 0; i < link_->rooms().size(); ++i)
                p.set_s("room_code(" + std::to_string(i) + ")", link_->rooms()[i].code);
            p.set_s("server_name", link_->server_name());
            p.set_s("motd", link_->motd());
            p.set_s("online_error", link_->error());
        }
    }

    mpui::Prefs prefs_;
    std::unique_ptr<ra2mp::ServerLink> link_;
    ShellList list_;
    std::uint64_t last_list_ = 0;
    std::uint32_t pending_v3_ = 0;
    std::string pending_code_;
};

Scene* MpLobbyScene::make_return_page() {
    if (net_ == LAN) return new MpLanScene(rules_, no_audio_, art_);
    return new MpOnlineScene(rules_, no_audio_, art_);
}

// ===========================================================================
// NET-PLAY: the match hand-off (replaces the "Match Ready" placeholder).
// ===========================================================================
// The full-state sync hash runs every kNetHashEvery frames (RA2.INI [Net]
// HashEvery overrides): fnv1a over save_world costs ~1-3 ms on a desktop for
// a 2-player map and several times that on a phone (README "Networking"), so
// once a second keeps the CPU cost low while a desync is still caught within
// ~1-2 s.
constexpr int kNetHashEvery = 15;

Scene* defer_net_game_scene(bool no_audio, ra2engine::SkirmishConfig cfg, std::shared_ptr<NetPlay> np,
                            std::uint32_t seed, std::vector<int> starts, int local_house) {
    auto c = std::make_shared<ra2engine::SkirmishConfig>(std::move(cfg));
    auto art = std::make_shared<SkirmishLoadArt>();
    std::function<void(SDL_Renderer*, ra2engine::TextureCache&, double)> backdrop;
    g_net_local_house = local_house;   // the loading art follows the local player's country
    art->load(*c);
    g_net_local_house = -1;
    backdrop = [art](SDL_Renderer* ren, ra2engine::TextureCache& tc, double frac) { art->draw(ren, tc, frac); };
    return defer_scene("LOADING", [no_audio, c, np, seed, starts, local_house]() -> Scene* {
        { int gw = 1280, gh = 720; game_logical_size(gw, gh); GameScene::set_canvas_size(gw, gh); }
        // Session-wide determinism inputs for the build: the local house
        // (local_slot_of), the session seed (crates), the lobby's resolved
        // start waypoints (the real-play start pick), the NetPlay itself.
        struct Restore {
            ~Restore() {
                g_net_local_house = -1;
                g_crate_seed_override = -1;
                g_simdiff_starts.clear();
                g_pending_netplay.reset();
            }
        } restore;
        g_net_local_house = local_house;
        g_crate_seed_override = seed;
        g_simdiff_starts = starts;
        g_pending_netplay = np;
        return new GameScene(no_audio, *c);
    }, backdrop);
}

Scene* mp_start_session(const Rules& rules, bool no_audio, const MenuArt& art, const ra2mp::MpSessionConfig& s,
                        std::shared_ptr<ra2mp::MpNet> transport) {
    (void)art;
    const std::string text = ra2mp::describe_session(s);
    std::size_t a = 0;
    while (a < text.size()) {
        std::size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        mpui::log_line(text.substr(a, b - a));
        a = b + 1;
    }
    ra2mp::MpConvertCtx ctx;
    ctx.countries = gf_lobby_countries();
    const Rules* r = &rules;
    ctx.side_of = [r](const std::string& k) { return gf_side_of_country(*r, k); };
    ra2engine::SkirmishConfig cfg = ra2mp::session_to_skirmish(s, ctx);

    auto np = std::make_shared<NetPlay>();
    np->mpnet = transport;
    np->session = s;
    np->ai_takeover = s.lobby.opt.ai_takeover;
    np->hash_every = net_ini_int("HashEvery", kNetHashEvery);
    ra2::net::NetGameConfig nc;
    nc.local_house = s.local_house;
    std::vector<int> starts;
    for (const ra2mp::MpPlayer& p : s.players) {
        starts.push_back(p.start);   // combatant k (slot order) -> the lobby's start waypoint
        if (p.is_ai) continue;
        nc.human_houses.push_back(p.house);
        if (!p.local && p.peer) nc.remotes.emplace_back(p.house, transport->addr_of(p.peer));
    }
    nc.master = s.is_host;
    const bool lan = s.transport == ra2mp::MpTransportKind::Lan;
    nc.mode = lan ? ra2::net::GameMode::LAN : ra2::net::GameMode::Internet;
    // Start values (net-protocol.md §8): LAN FSR 5 / MaxAhead 10; Internet
    // FSR 5 / MaxAhead 15 (3 x FSR, the master's TIMING floor). The host's
    // TIMING events then size both from the measured round trip.
    nc.frame_send_rate = static_cast<std::uint32_t>(net_ini_int("FrameSendRate", 5));
    nc.max_ahead = static_cast<std::uint32_t>(net_ini_int("MaxAhead", lan ? 10 : 15));
    nc.hash_every = np->hash_every;
    np->game = std::make_unique<ra2::net::NetGame>(transport->net(), nc);
    mpui::log_line("lockstep session: local house " + std::to_string(nc.local_house) + ", " +
                   std::to_string(nc.remotes.size()) + " remote player(s), " + (lan ? "LAN" : "tunnel") +
                   ", master " + (nc.master ? "yes" : "no"));
    for (const auto& rm : nc.remotes)
        mpui::log_line("  house " + std::to_string(rm.first) + " at " + transport->describe(transport->peer_of_addr(rm.second)));
    return defer_net_game_scene(no_audio, cfg, np, s.seed, starts, s.local_house);
}
