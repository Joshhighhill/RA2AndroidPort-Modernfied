// engine/mp/mp_lobby.cpp -- see mp_lobby.hpp.
#include "mp/mp_lobby.hpp"

#include <algorithm>

namespace ra2mp {

namespace {
constexpr std::uint64_t kResendMs = 250;
constexpr int kMaxTries = 40;            // 10 s of resends
constexpr std::uint64_t kPingMs = 2000;
constexpr std::uint64_t kSilenceMs = 15000;
constexpr std::uint64_t kQueryMs = 1000;
constexpr int kMaxQueries = 15;
constexpr std::uint64_t kGoWaitMs = 3000;
constexpr std::size_t kSeenRing = 512;
constexpr std::size_t kChatLines = 50;

std::uint32_t body_cmd(const std::vector<std::uint8_t>& b) {
    return b.size() >= 4 ? (std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) | (std::uint32_t(b[2]) << 16) |
                            (std::uint32_t(b[3]) << 24))
                         : 0u;
}

std::vector<std::uint8_t> simple(std::uint32_t cmd) {
    BodyWriter w;
    w.u32(cmd);
    return w.b;
}
}  // namespace

// ------------------------------------------------------------- transports --

std::string LobbyTransport::describe(PeerId p) const {
    if (p & kTunnelPeerFlag) return "tunnel id " + std::to_string(static_cast<std::uint32_t>(p));
    return Addr{static_cast<std::uint32_t>(p >> 16), static_cast<std::uint16_t>(p & 0xFFFF)}.str();
}

bool UdpLobbyTransport::open(std::uint16_t port) { return sock_.open(port, false, false); }

bool UdpLobbyTransport::send(PeerId to, const std::vector<std::uint8_t>& d) {
    const Addr a{static_cast<std::uint32_t>(to >> 16), static_cast<std::uint16_t>(to & 0xFFFF)};
    return sock_.send_to(a, d.data(), d.size());
}

bool UdpLobbyTransport::recv(PeerId& from, std::vector<std::uint8_t>& d) {
    std::uint8_t buf[2048];
    Addr a;
    const int n = sock_.recv_from(a, buf, sizeof buf);
    if (n <= 0) return false;
    from = peer_of(a);
    d.assign(buf, buf + n);
    return true;
}

std::string UdpLobbyTransport::describe(PeerId p) const { return LobbyTransport::describe(p); }

bool TunnelLobbyTransport::open(const Addr& tunnel, std::uint32_t my_v3) {
    tunnel_ = tunnel;
    me_ = my_v3;
    last_keep_ = 0;
    return sock_.open(0, false, false);
}

bool TunnelLobbyTransport::send(PeerId to, const std::vector<std::uint8_t>& d) {
    if (!(to & kTunnelPeerFlag)) return false;
    std::vector<std::uint8_t> out;
    v3_wrap(me_, static_cast<std::uint32_t>(to), d, out);
    return sock_.send_to(tunnel_, out.data(), out.size());
}

bool TunnelLobbyTransport::recv(PeerId& from, std::vector<std::uint8_t>& d) {
    std::uint8_t buf[2048];
    for (;;) {
        Addr a;
        const int n = sock_.recv_from(a, buf, sizeof buf);
        if (n <= 0) return false;
        if (a != tunnel_) continue;   // only the tunnel talks to this socket
        if (ping_sent_ && v3_is_ping_reply(buf, static_cast<std::size_t>(n), ping_nonce_)) {
            rtt_ = static_cast<int>(now_ms() - ping_sent_);
            ping_sent_ = 0;
            continue;
        }
        std::uint32_t s = 0, r = 0;
        const std::uint8_t* pl = nullptr;
        std::size_t plen = 0;
        if (!v3_unwrap(buf, static_cast<std::size_t>(n), s, r, pl, plen) || r != me_ || s == 0) continue;
        from = peer_of_v3(s);
        d.assign(pl, pl + plen);
        return true;
    }
}

void TunnelLobbyTransport::tick(std::uint64_t now) {
    // Register / refresh our id at the tunnel (V3 learns the sender's
    // endpoint from its first packet; the receiver may not exist yet, the
    // tunnel then just drops the payload). 0xFFFFFFFE is never a real id.
    if (last_keep_ == 0 || now - last_keep_ >= 5000) {
        std::vector<std::uint8_t> out;
        v3_wrap(me_, 0xFFFFFFFEu, {}, out);
        sock_.send_to(tunnel_, out.data(), out.size());
        if (ping_sent_ == 0 || now - ping_sent_ > 4000) {
            ping_nonce_ = static_cast<std::uint32_t>(now * 2654435761u) | 1u;
            const std::vector<std::uint8_t> ping = v3_ping_request(ping_nonce_);
            sock_.send_to(tunnel_, ping.data(), ping.size());
            ping_sent_ = now_ms();
        }
        last_keep_ = now;
    }
}

std::string TunnelLobbyTransport::describe(PeerId p) const { return LobbyTransport::describe(p); }

MemoryHub::Endpoint* MemoryHub::add(PeerId id) {
    auto e = std::make_unique<Endpoint>();
    e->id = id;
    e->hub = this;
    Endpoint* raw = e.get();
    eps[id] = std::move(e);
    return raw;
}

bool MemoryHub::Endpoint::send(PeerId to, const std::vector<std::uint8_t>& d) {
    ++sent;
    if (drop_every > 0 && sent % drop_every == 0) return true;   // lost on the "wire"
    const auto it = hub->eps.find(to);
    if (it == hub->eps.end()) return false;
    it->second->inbox.push_back({id, d});
    return true;
}

bool MemoryHub::Endpoint::recv(PeerId& from, std::vector<std::uint8_t>& d) {
    if (inbox.empty()) return false;
    from = inbox.front().first;
    d = std::move(inbox.front().second);
    inbox.pop_front();
    return true;
}

// ---------------------------------------------------------- global channel --

void GlobalChannel::send(PeerId to, const std::vector<std::uint8_t>& body, bool reliable, std::uint64_t now) {
    GlobalPacket g;
    g.code = reliable ? kDataAck : kDataNoAck;
    g.packet_id = next_id_++;
    g.body = body;
    std::vector<std::uint8_t> d = encode_global(g);
    t_.send(to, d);
    if (reliable) pend_.push_back(Pending{to, g.packet_id, std::move(d), now, 1});
}

void GlobalChannel::poll(std::uint64_t now, std::vector<In>& in, std::vector<PeerId>& lost) {
    t_.tick(now);
    PeerId from = 0;
    std::vector<std::uint8_t> d;
    while (t_.recv(from, d)) {
        GlobalPacket g;
        if (!decode_global(d.data(), d.size(), g)) continue;
        if (g.code == kAck) {
            pend_.erase(std::remove_if(pend_.begin(), pend_.end(),
                                       [&](const Pending& p) { return p.to == from && p.id == g.packet_id; }),
                        pend_.end());
            continue;
        }
        if (g.code == kDataAck) {
            GlobalPacket ack;
            ack.code = kAck;
            ack.packet_id = g.packet_id;
            t_.send(from, encode_global(ack));
            const auto key = std::make_pair(from, g.packet_id);
            if (std::find(seen_.begin(), seen_.end(), key) != seen_.end()) continue;   // duplicate
            seen_.push_back(key);
            if (seen_.size() > kSeenRing) seen_.pop_front();
        }
        in.push_back(In{from, std::move(g.body)});
    }
    for (std::size_t i = 0; i < pend_.size();) {
        Pending& p = pend_[i];
        if (now - p.last >= kResendMs) {
            if (p.tries >= kMaxTries) {
                if (std::find(lost.begin(), lost.end(), p.to) == lost.end()) lost.push_back(p.to);
                pend_.erase(pend_.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            t_.send(p.to, p.dgram);
            p.last = now;
            ++p.tries;
        }
        ++i;
    }
}

void GlobalChannel::forget(PeerId p) {
    pend_.erase(std::remove_if(pend_.begin(), pend_.end(), [&](const Pending& x) { return x.to == p; }), pend_.end());
}

bool GlobalChannel::idle_to(PeerId p) const {
    for (const Pending& x : pend_)
        if (x.to == p) return false;
    return true;
}

// ------------------------------------------------------------------ shared --

const char* reject_text(RejectReason r) {
    switch (r) {
        case RejectReason::Version: return "Your game version differs from the host's.";
        case RejectReason::Rules: return "Your game data (rules) differs from the host's.";
        case RejectReason::Full: return "The game is full.";
        case RejectReason::Started: return "The game has already started.";
        case RejectReason::Kicked: return "You were removed from the game by the host.";
        case RejectReason::HostLeft: return "The host closed the game.";
        case RejectReason::Lost: return "Connection to the host was lost.";
        default: return "";
    }
}

std::vector<std::vector<std::uint8_t>> options_bodies(std::uint32_t rev, const std::string& opts) {
    std::vector<std::vector<std::uint8_t>> out;
    const std::size_t parts = std::max<std::size_t>(1, (opts.size() + kOptionsChunk - 1) / kOptionsChunk);
    for (std::size_t i = 0; i < parts; ++i) {
        const std::string chunk = opts.substr(i * kOptionsChunk, kOptionsChunk);
        BodyWriter w;
        w.u32(kCmdGameOptions);
        w.u32(rev);
        w.u8(static_cast<std::uint8_t>(i));
        w.u8(static_cast<std::uint8_t>(parts));
        w.u16(static_cast<std::uint16_t>(chunk.size()));
        for (char c : chunk) w.u8(static_cast<std::uint8_t>(c));
        out.push_back(w.b);
    }
    return out;
}

void LobbyBase::add_chat(const ChatLine& l) {
    chat_.push_back(l);
    while (chat_.size() > kChatLines) chat_.pop_front();
}

void LobbyBase::system_line(const std::string& text) { add_chat(ChatLine{"", text, -1}); }

void LobbyBase::build_session(bool is_host) {
    session_ = MpSessionConfig{};
    session_.lobby = model_;
    session_.options = encode_options(model_);
    session_.seed = model_.seed;
    session_.is_host = is_host;
    session_.local_slot = local_slot_;
    resolve_session(session_, 9);   // the UI re-resolves with the real country list
    started_ = true;
}

// -------------------------------------------------------------------- host --

LobbyHost::LobbyHost(LobbyTransport& t, const MpIdentity& me, const MpLobbyModel& initial)
    : ch_(t), me_(me) {
    model_ = initial;
    MpSlot& h = model_.slots[0];
    h.kind = SlotKind::Human;
    h.name = sanitize_name(me.nick);
    h.host = true;
    h.has_map = true;
    h.ready = true;
    h.pid_lo = static_cast<std::uint32_t>(me.player_id);
    local_slot_ = 0;
    rev_ = 1;
}

PeerId LobbyHost::peer_of_slot(int slot) const {
    for (const auto& kv : peers_)
        if (kv.second.slot == slot) return kv.first;
    return 0;
}

void LobbyHost::changed(bool clear_ready) {
    if (clear_ready)
        for (MpSlot& s : model_.slots)
            if (s.kind == SlotKind::Human && !s.host) s.ready = false;
    ++rev_;
    dirty_ = true;
}

void LobbyHost::set_slot_kind(int slot, SlotKind kind, int ai) {
    if (slot <= 0 || slot >= kSlots || going_) return;
    MpSlot& s = model_.slots[slot];
    if (s.kind == SlotKind::Human) {
        if (kind == SlotKind::Human) return;
        kick(slot);   // replacing a human = removing them
    }
    if (kind == SlotKind::Human) return;   // humans only arrive by joining
    const SlotKind was = s.kind;
    s = MpSlot{};
    s.kind = kind;
    s.ai = std::max(0, std::min(2, ai));
    if (kind == SlotKind::AI && was == SlotKind::AI) {
        // keep nothing else: an AI level change resets the row like the skirmish page
    }
    changed(true);
}

void LobbyHost::set_slot(int slot, int country, int color, int team, int start) {
    if (slot < 0 || slot >= kSlots || going_) return;
    MpSlot& s = model_.slots[slot];
    if (!s.occupied()) return;
    if (color >= 0 && color_owner(model_, color, slot) >= 0) color = s.color;
    if (start >= 0 && start_owner(model_, start, slot) >= 0) start = s.start;
    s.country = country < 0 ? kRandom : country;
    s.color = color < 0 ? kRandom : std::min(color, 7);
    s.team = team < 0 ? kNoTeam : std::min(team, 3);
    s.start = start < 0 ? kRandomStart : std::min(start, std::max(0, model_.map.max_players - 1));
    changed(slot != 0);
}

void LobbyHost::set_options(const MpOptions& o) {
    if (going_) return;
    model_.opt = o;
    changed(true);
}

void LobbyHost::set_map(const MpMap& m) {
    if (going_) return;
    model_.map = m;
    // Slots past the new map's max close; humans there are removed.
    for (int i = std::max(1, m.max_players); i < kSlots; ++i) {
        if (model_.slots[i].kind == SlotKind::Human) kick(i);
        model_.slots[i] = MpSlot{};
        model_.slots[i].kind = SlotKind::Closed;
    }
    for (int i = 1; i < std::min(kSlots, m.max_players); ++i)
        if (model_.slots[i].kind == SlotKind::Closed) model_.slots[i].kind = SlotKind::Open;
    for (MpSlot& s : model_.slots) {
        if (s.start >= m.max_players) s.start = kRandomStart;
        if (s.kind == SlotKind::Human && !s.host) s.has_map = false;   // until the client re-checks
    }
    changed(true);
}

void LobbyHost::kick(int slot) {
    if (slot <= 0 || slot >= kSlots) return;
    const PeerId p = peer_of_slot(slot);
    if (p) {
        BodyWriter w;
        w.u32(kCmdRejectJoin);
        w.u8(static_cast<std::uint8_t>(RejectReason::Kicked));
        ch_.send(p, w.b, true, now_);
        system_line(model_.slots[slot].name + " was removed from the game.");
        peers_.erase(p);   // the REJECT keeps resending until ACKed
    }
    model_.slots[slot] = MpSlot{};
    changed(false);
}

void LobbyHost::chat(const std::string& text) {
    if (text.empty()) return;
    ChatPacket c;
    c.sender = model_.slots[0].name;
    c.text = text;
    c.color = model_.slots[0].color;
    c.game_hash = game_name_hash(room_);
    add_chat(ChatLine{c.sender, text, c.color});
    const std::vector<std::uint8_t> body = encode_chat(c);
    for (const auto& kv : peers_) ch_.send(kv.first, body, true, now_);
}

StartCheck LobbyHost::start(std::uint32_t seed) {
    const StartCheck c = can_start(model_);
    if (c != StartCheck::Ok || going_) return c;
    model_.seed = seed;
    ++rev_;
    broadcast_options(now_);
    dirty_ = false;
    for (const auto& kv : peers_) {
        BodyWriter w;
        w.u32(kCmdGo);
        w.u32(rev_);
        w.u32(seed);
        ch_.send(kv.first, w.b, true, now_);
    }
    going_ = true;
    go_at_ = now_;
    return c;
}

void LobbyHost::close() {
    for (const auto& kv : peers_) {
        ch_.send(kv.first, simple(kCmdSignOff), false, now_);
        ch_.send(kv.first, simple(kCmdSignOff), false, now_);
    }
    peers_.clear();
}

void LobbyHost::send_options(PeerId to, std::uint64_t now) {
    for (const auto& b : options_bodies(rev_, encode_options(model_))) ch_.send(to, b, true, now);
}

void LobbyHost::broadcast_options(std::uint64_t now) {
    const std::string opts = encode_options(model_);
    const auto bodies = options_bodies(rev_, opts);
    for (const auto& kv : peers_)
        for (const auto& b : bodies) ch_.send(kv.first, b, true, now);
}

void LobbyHost::drop_peer(PeerId p, const char* why) {
    const auto it = peers_.find(p);
    if (it == peers_.end()) return;
    const int slot = it->second.slot;
    peers_.erase(it);
    ch_.forget(p);
    if (slot > 0 && slot < kSlots && model_.slots[slot].kind == SlotKind::Human) {
        system_line(model_.slots[slot].name + why);
        if (on_event) on_event(model_.slots[slot].name + why);
        model_.slots[slot] = MpSlot{};
        changed(false);
    }
}

void LobbyHost::handle(PeerId from, const std::vector<std::uint8_t>& body, std::uint64_t now) {
    const std::uint32_t cmd = body_cmd(body);
    auto pit = peers_.find(from);
    if (pit != peers_.end()) pit->second.heard = now;
    BodyReader r(body);
    r.u32();
    switch (cmd) {
        case kCmdQueryJoin: {
            const std::uint16_t proto = r.u16();
            const std::uint32_t sim = r.u32();
            const std::uint32_t rules = r.u32();
            const std::uint64_t pid = r.u64();
            const std::string nick = sanitize_name(r.fixed(20));
            if (pit != peers_.end()) {   // a repeat (our CONFIRM was lost / slow)
                BodyWriter w;
                w.u32(kCmdConfirmJoin);
                w.u8(static_cast<std::uint8_t>(pit->second.slot));
                ch_.send(from, w.b, true, now);
                send_options(from, now);
                return;
            }
            RejectReason why = RejectReason::None;
            int slot = -1;
            if (proto != me_.net_proto || sim != me_.sim_ver) why = RejectReason::Version;
            else if (rules != me_.rules_crc) why = RejectReason::Rules;
            else if (going_ || started_) why = RejectReason::Started;
            else if ((slot = first_open_slot(model_)) < 0) why = RejectReason::Full;
            if (why != RejectReason::None) {
                BodyWriter w;
                w.u32(kCmdRejectJoin);
                w.u8(static_cast<std::uint8_t>(why));
                ch_.send(from, w.b, true, now);
                return;
            }
            MpSlot& s = model_.slots[slot];
            s = MpSlot{};
            s.kind = SlotKind::Human;
            s.name = nick.empty() ? "Player" : nick;
            s.pid_lo = static_cast<std::uint32_t>(pid);
            s.has_map = false;
            peers_[from] = Peer{slot, now, now};
            BodyWriter w;
            w.u32(kCmdConfirmJoin);
            w.u8(static_cast<std::uint8_t>(slot));
            ch_.send(from, w.b, true, now);
            system_line(s.name + " joined the game.");
            if (on_event) on_event(s.name + " joined from " + ch_.transport().describe(from));
            changed(false);
            return;
        }
        case kCmdSlotRequest: {
            if (pit == peers_.end() || going_) return;
            const int country = r.i32(), color = r.i32(), team = r.i32(), start = r.i32();
            const int slot = pit->second.slot;
            MpSlot& s = model_.slots[slot];
            if (color >= 0 && color_owner(model_, color, slot) < 0) s.color = std::min(color, 7);
            else if (color < 0) s.color = kRandom;
            if (start >= 0 && start < model_.map.max_players && start_owner(model_, start, slot) < 0) s.start = start;
            else if (start < 0) s.start = kRandomStart;
            s.country = country < 0 ? kRandom : country;
            s.team = team < 0 ? kNoTeam : std::min(team, 3);
            s.ready = false;
            changed(false);   // rebroadcast even when the request was refused (the client resyncs)
            return;
        }
        case kCmdReadyToGo: {
            if (pit == peers_.end() || going_) return;
            MpSlot& s = model_.slots[pit->second.slot];
            const bool ready = r.u8() != 0;
            const bool has_map = r.u8() != 0;
            if (s.ready != ready || s.has_map != has_map) {
                s.ready = ready && has_map;
                s.has_map = has_map;
                changed(false);
            }
            return;
        }
        case kCmdMessage: {
            if (pit == peers_.end()) return;
            ChatPacket c;
            if (!decode_chat(body, c) || c.text.empty()) return;
            const MpSlot& s = model_.slots[pit->second.slot];
            c.sender = s.name;   // stamped from the peer, never trusted from the body
            c.color = s.color;
            c.game_hash = game_name_hash(room_);
            add_chat(ChatLine{c.sender, c.text, c.color});
            const std::vector<std::uint8_t> out = encode_chat(c);
            for (const auto& kv : peers_)
                if (kv.first != from) ch_.send(kv.first, out, true, now);
            return;
        }
        case kCmdSignOff:
            drop_peer(from, " left the game.");
            return;
        case kCmdPing:
        default:
            return;
    }
}

void LobbyHost::update(std::uint64_t now) {
    now_ = now;
    std::vector<GlobalChannel::In> in;
    std::vector<PeerId> lost;
    ch_.poll(now, in, lost);
    for (const auto& m : in) handle(m.from, m.body, now);
    for (PeerId p : lost) drop_peer(p, " lost connection.");
    std::vector<PeerId> silent;
    for (auto& kv : peers_) {
        if (now - kv.second.heard > kSilenceMs) silent.push_back(kv.first);
        else if (now - kv.second.pinged >= kPingMs) {
            ch_.send(kv.first, simple(kCmdPing), false, now);
            kv.second.pinged = now;
        }
    }
    for (PeerId p : silent) drop_peer(p, " timed out.");
    if (dirty_ && !going_) {
        broadcast_options(now);
        dirty_ = false;
    }
    if (going_ && !started_) {
        bool all = true;
        for (const auto& kv : peers_) all = all && ch_.idle_to(kv.first);
        if (all || now - go_at_ >= kGoWaitMs) build_session(true);
    }
}

// ------------------------------------------------------------------ client --

LobbyClient::LobbyClient(LobbyTransport& t, PeerId host, const MpIdentity& me) : ch_(t), host_(host), me_(me) {}

void LobbyClient::request_slot(int country, int color, int team, int start) {
    if (state_ != State::Joined) return;
    BodyWriter w;
    w.u32(kCmdSlotRequest);
    w.i32(country);
    w.i32(color);
    w.i32(team);
    w.i32(start);
    ch_.send(host_, w.b, true, now_);
    ready_ = false;
}

void LobbyClient::send_ready(std::uint64_t now) {
    BodyWriter w;
    w.u32(kCmdReadyToGo);
    w.u8(ready_ ? 1 : 0);
    w.u8(has_map_ ? 1 : 0);
    ch_.send(host_, w.b, true, now);
}

void LobbyClient::set_ready(bool ready) {
    if (state_ != State::Joined) return;
    ready_ = ready && has_map_;
    send_ready(now_);
}

void LobbyClient::chat(const std::string& text) {
    if (state_ != State::Joined || text.empty()) return;
    ChatPacket c;
    c.sender = sanitize_name(me_.nick);
    c.text = text;
    c.color = local_slot_ >= 0 ? model_.slots[local_slot_].color : -1;
    c.game_hash = game_name_hash(room_);
    add_chat(ChatLine{c.sender, text, c.color});
    ch_.send(host_, encode_chat(c), true, now_);
}

void LobbyClient::leave() {
    if (state_ == State::Joined || state_ == State::Joining) {
        ch_.send(host_, simple(kCmdSignOff), false, now_);
        ch_.send(host_, simple(kCmdSignOff), false, now_);
    }
    state_ = State::Rejected;
    reason_ = RejectReason::None;
}

void LobbyClient::handle(const std::vector<std::uint8_t>& body, std::uint64_t now) {
    const std::uint32_t cmd = body_cmd(body);
    BodyReader r(body);
    r.u32();
    switch (cmd) {
        case kCmdConfirmJoin:
            if (state_ == State::Joining) {
                local_slot_ = r.u8();
                state_ = State::Joined;
                system_line("Joined the game.");
            }
            return;
        case kCmdRejectJoin:
            reason_ = static_cast<RejectReason>(r.u8());
            state_ = State::Rejected;
            system_line(reject_text(reason_));
            return;
        case kCmdGameOptions: {
            const std::uint32_t rev = r.u32();
            const std::uint8_t part = r.u8(), nparts = r.u8();
            const std::uint16_t len = r.u16();
            if (r.bad || nparts == 0 || part >= nparts || len > kOptionsChunk || rev < rev_) return;
            if (rev != asm_rev_ || parts_.size() != nparts) {
                asm_rev_ = rev;
                parts_.assign(nparts, std::string());
                got_.assign(nparts, false);
            }
            std::string chunk;
            for (std::uint16_t i = 0; i < len; ++i) chunk += static_cast<char>(r.u8());
            parts_[part] = chunk;
            got_[part] = true;
            for (bool g : got_)
                if (!g) return;   // still missing a part
            std::string all;
            for (const std::string& p : parts_) all += p;
            parts_.clear();
            got_.clear();
            MpLobbyModel m;
            if (!decode_options(all, m)) return;
            model_ = m;
            rev_ = rev;
            if (local_slot_ >= 0) {
                const MpSlot& me = model_.slots[local_slot_];
                has_map_ = have_map ? have_map(model_.map) : true;
                ready_ = me.ready;                  // the host is authoritative (it clears ready on changes)
                if (me.has_map != has_map_) send_ready(now);   // report whether we have this map
            }
            if (go_pending_ && go_rev_ == rev_) {
                model_.seed = go_seed_;
                go_pending_ = false;
                state_ = State::Started;
                build_session(false);
            }
            return;
        }
        case kCmdGo: {
            go_rev_ = r.u32();
            go_seed_ = r.u32();
            if (go_rev_ == rev_ && state_ == State::Joined) {
                model_.seed = go_seed_;
                state_ = State::Started;
                build_session(false);
            } else {
                go_pending_ = true;
            }
            return;
        }
        case kCmdMessage: {
            ChatPacket c;
            if (decode_chat(body, c)) add_chat(ChatLine{c.sender, c.text, c.color});
            return;
        }
        case kCmdSignOff:
            state_ = State::Rejected;
            reason_ = RejectReason::HostLeft;
            system_line(reject_text(reason_));
            return;
        default:
            return;
    }
}

void LobbyClient::update(std::uint64_t now) {
    now_ = now;
    if (heard_ == 0) heard_ = now;
    std::vector<GlobalChannel::In> in;
    std::vector<PeerId> lost;
    ch_.poll(now, in, lost);
    for (const auto& m : in) {
        if (m.from != host_) continue;   // the lobby is a star: only the host talks to us
        heard_ = now;
        if (state_ == State::Rejected || state_ == State::Lost) continue;
        handle(m.body, now);
    }
    if (state_ == State::Joining && (last_query_ == 0 || now - last_query_ >= kQueryMs)) {
        if (queries_ >= kMaxQueries) {
            state_ = State::Lost;
            reason_ = RejectReason::Lost;
            system_line("No answer from the host.");
            return;
        }
        BodyWriter w;
        w.u32(kCmdQueryJoin);
        w.u16(me_.net_proto);
        w.u32(me_.sim_ver);
        w.u32(me_.rules_crc);
        w.u64(me_.player_id);
        w.fixed(sanitize_name(me_.nick), 20);
        w.fixed(me_.version, 24);
        ch_.send(host_, w.b, false, now);   // repeated until CONFIRM / REJECT
        last_query_ = now;
        ++queries_;
    }
    if (state_ == State::Joined) {
        if (!lost.empty() || now - heard_ > kSilenceMs) {
            state_ = State::Lost;
            reason_ = RejectReason::Lost;
            system_line(reject_text(reason_));
            return;
        }
        if (now - pinged_ >= kPingMs) {
            ch_.send(host_, simple(kCmdPing), false, now);
            pinged_ = now;
        }
    }
}

}  // namespace ra2mp
