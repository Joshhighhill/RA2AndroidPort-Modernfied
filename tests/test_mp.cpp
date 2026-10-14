// Multiplayer shell (engine/mp/): the original 0x1236 global-channel framing,
// the original options string, the host-authoritative lobby over in-memory,
// loopback-UDP and CnCNet-V3-tunnel transports, LAN beacons, mini JSON.
// docs/gap/net/01-original-protocol.md §2.1/§2.3/§6/§7.1, 02-cncnet.md §3.2,
// 05-transport-infra.md §3/§5.3.
#include "doctest.h"

#include "mp/lan_discovery.hpp"
#include "mp/lobby_wire.hpp"
#include "mp/mini_json.hpp"
#include "mp/mp_lobby.hpp"
#include "mp/mp_model.hpp"
#include "mp/mp_net.hpp"
#include "net/wire.hpp"

#include <map>
#include <memory>
#include <set>

using namespace ra2mp;

TEST_CASE("mp: the room-name hash is engine/net's Add_CRC (packet_crc)") {
    // The datagram CRC, the 0x1236 header, ACK / resend / dedupe are
    // engine/net's (tests/test_net_wire.cpp); the lobby only hashes the name.
    CHECK(game_name_hash("") == 0u);
    CHECK(game_name_hash(std::string("\x01\x00\x00\x00\x02", 5)) == ((1u << 1) + 2u));
}

TEST_CASE("mp: chat packet uses the original 0x0C layout") {
    ChatPacket c;
    c.sender = "Tanya";
    c.text = "Hallo \xC3\xBC";   // u-umlaut
    c.color = 3;
    c.game_hash = 0xCAFEBABE;
    const auto b = encode_chat(c);
    REQUIRE(b.size() == 455u);
    CHECK(b[0] == 0x0C);
    CHECK(b[4] == 'T');
    CHECK(b[0x30] == 'H');
    CHECK(b[0x31] == 0);                  // UTF-16LE
    CHECK(b[0x30 + 2 * 6] == 0xFC);       // U+00FC
    CHECK(b[0x110] == 3);
    CHECK(b[0x114] == 0xBE);
    ChatPacket back;
    REQUIRE(decode_chat(b, back));
    CHECK(back.sender == "Tanya");
    CHECK(back.text == c.text);
    CHECK(back.color == 3);
    CHECK(back.game_hash == 0xCAFEBABEu);
}

namespace {
MpLobbyModel sample_model() {
    MpLobbyModel m;
    m.seed = 123456789;
    m.opt.credits = 7500;
    m.opt.unit_count = 4;
    m.opt.crates = false;
    m.opt.game_speed = 2;
    m.map.file = "xmp01t4.map";
    m.map.digest = normalize_digest("bmZ4QWcxYWRmZ2hqa2xxd2VydA==");
    m.map.max_players = 4;
    m.map.size = 98765;
    m.slots[0].kind = SlotKind::Human;
    m.slots[0].name = "Host,Name";   // comma sanitised
    m.slots[0].host = true;
    m.slots[0].country = 3;
    m.slots[0].color = 1;
    m.slots[0].team = 0;
    m.slots[0].pid_lo = 0xDEADBEEF;
    m.slots[1].kind = SlotKind::AI;
    m.slots[1].ai = 0;
    m.slots[1].country = 8;
    m.slots[2].kind = SlotKind::Human;
    m.slots[2].name = "Guest";
    m.slots[2].ready = true;
    m.slots[2].start = 2;
    m.slots[3].kind = SlotKind::Closed;
    for (int i = 4; i < 8; ++i) m.slots[i].kind = SlotKind::Closed;
    return m;
}
}  // namespace

TEST_CASE("mp: options string = original layout starting with the seed, round trip") {
    const MpLobbyModel m = sample_model();
    const std::string s = encode_options(m);
    CHECK(s.rfind("123456789,2,2,7500,4,", 0) == 0);   // Seed, GameSpeed x2, Credits, UnitCount
    CHECK(s.find("xmp01t4.map:") != std::string::npos);  // Scenario:Digest
    CHECK(s.find("Host_Name,3,1,-1,0,0,") != std::string::npos);   // name,side,colour,start,team,slot
    CHECK(s.find("slots=HAHCCCCC") != std::string::npos);
    MpLobbyModel back;
    REQUIRE(decode_options(s, back));
    CHECK(back.seed == m.seed);
    CHECK(back.opt.credits == 7500);
    CHECK_FALSE(back.opt.crates);
    CHECK(back.map.file == "xmp01t4.map");
    CHECK(back.map.digest == m.map.digest);
    CHECK(back.map.max_players == 4);
    CHECK(back.slots[0].kind == SlotKind::Human);
    CHECK(back.slots[0].name == "Host_Name");
    CHECK(back.slots[0].host);
    CHECK(back.slots[0].pid_lo == 0xDEADBEEFu);
    CHECK(back.slots[1].kind == SlotKind::AI);
    CHECK(back.slots[1].ai == 0);
    CHECK(back.slots[1].country == 8);
    CHECK(back.slots[2].name == "Guest");
    CHECK(back.slots[2].ready);
    CHECK(back.slots[2].start == 2);
    CHECK(back.slots[3].kind == SlotKind::Closed);
    CHECK(encode_options(back) == s);
    CHECK_FALSE(decode_options("1,2,3", back));
    CHECK_FALSE(decode_options(s.substr(0, s.size() - 3), back));
}

TEST_CASE("mp: map [Digest] lookup") {
    const std::string map = "[Basic]\r\nName=X\r\n[Digest]\r\n1=AbCd+/==\r\n[Map]\r\nSize=0,0,10,10\r\n";
    CHECK(map_digest_of(map) == "AbCd+/==");
    CHECK(normalize_digest("AbCd+/==") == "AbCd+/__");
    CHECK(map_digest_of("[Basic]\nName=X\n").empty());
}

TEST_CASE("mp: session resolution is deterministic and keeps colours / starts unique") {
    MpSessionConfig a;
    a.lobby = sample_model();
    a.lobby.slots[0].color = kRandom;
    a.seed = 42;
    a.local_slot = 2;
    MpSessionConfig b = a;
    b.local_slot = 0;
    resolve_session(a, 9);
    resolve_session(b, 9);
    REQUIRE(a.players.size() == 3u);
    std::set<int> colors, starts;
    for (std::size_t i = 0; i < a.players.size(); ++i) {
        CHECK(a.players[i].color == b.players[i].color);
        CHECK(a.players[i].country == b.players[i].country);
        CHECK(a.players[i].start == b.players[i].start);
        CHECK(a.players[i].house == static_cast<int>(i));
        colors.insert(a.players[i].color);
        starts.insert(a.players[i].start);
        CHECK(a.players[i].start < 4);
    }
    CHECK(colors.size() == 3u);
    CHECK(starts.size() == 3u);
    CHECK(a.local_house == 2);
    CHECK(b.local_house == 0);
    MpConvertCtx ctx;
    ctx.countries = {"Americans", "Alliance", "French", "Germans", "British", "Africans", "Arabs", "Confederation",
                     "Russians"};
    const auto cfg = session_to_skirmish(a, ctx);
    REQUIRE(cfg.slots.size() == 4u);          // 3 players + Neutral
    CHECK_FALSE(cfg.slots[0].is_ai);
    CHECK(cfg.slots[1].is_ai);
    CHECK_FALSE(cfg.slots[2].is_ai);          // remote human: driven by lockstep, not AI
    CHECK(cfg.slots[0].alliance_team == 0);
    CHECK(cfg.starting_cash == 7500);
    const auto prev = session_to_local_preview(a, ctx);
    CHECK_FALSE(prev.slots[0].is_ai);         // local player first
    CHECK(prev.slots[1].is_ai);
    CHECK(prev.slots[2].is_ai);               // remote human -> AI in the preview
}

namespace {
MpLobbyModel host_model() {
    MpLobbyModel m;
    m.map.file = "xmp01t4.map";
    m.map.digest = "abc";
    m.map.max_players = 4;
    for (int i = 4; i < 8; ++i) m.slots[i].kind = SlotKind::Closed;
    return m;
}

MpIdentity ident(const std::string& nick, std::uint64_t pid, std::uint32_t rules = 0x1234) {
    MpIdentity id;
    id.nick = nick;
    id.player_id = pid;
    id.rules_crc = rules;
    return id;
}

// In-process network (engine/net LoopbackHub) on the test's virtual clock:
// endpoint n is the address n.0.0.0:1.
struct MemNet {
    ra2::net::LoopbackHub hub;
    std::uint64_t* t;
    std::map<std::uint32_t, std::unique_ptr<MpNet>> nets;
    explicit MemNet(std::uint64_t& clock) : hub([&clock]() { return static_cast<std::uint32_t>(clock); }), t(&clock) {}
    MpNet& add(std::uint32_t n) {
        auto m = std::make_unique<MpNet>();
        std::uint64_t* tt = t;
        m->open_loopback(hub, Addr{n, 1}, [tt]() { return static_cast<std::uint32_t>(*tt); });
        MpNet& r = *m;
        nets[n] = std::move(m);
        return r;
    }
};
PeerId mem_peer(std::uint32_t n) { return peer_of(Addr{n, 1}); }

template <class F>
bool pump(LobbyHost& h, std::vector<LobbyClient*> cs, std::uint64_t& t, F done, int steps = 400) {
    for (int i = 0; i < steps; ++i) {
        t += 50;
        h.update(t);
        for (LobbyClient* c : cs) c->update(t);
        if (done()) return true;
    }
    return false;
}
}  // namespace

TEST_CASE("mp: lobby join, slot requests, ready, chat relay and start (memory transport)") {
    std::uint64_t t = 1000;
    MemNet hub(t);
    MpNet& eh = hub.add(1);
    MpNet& e1 = hub.add(2);
    MpNet& e2 = hub.add(3);
    LobbyHost host(eh, ident("Host", 0x1111), host_model());
    host.set_room_name("Room");
    LobbyClient c1(e1, mem_peer(1), ident("Alice", 0x2222));
    LobbyClient c2(e2, mem_peer(1), ident("Bob", 0x3333));
    c1.have_map = c2.have_map = [](const MpMap& m) { return m.digest == "abc"; };
    REQUIRE(pump(host, {&c1, &c2}, t, [&] {
        return c1.state() == LobbyClient::State::Joined && c2.state() == LobbyClient::State::Joined &&
               c1.rev() == host.rev() && c2.rev() == host.rev() && host.model().slots[1].has_map &&
               host.model().slots[2].has_map;
    }));
    CHECK(host.members() == 2);
    CHECK(c1.local_slot() == 1);
    CHECK(c2.local_slot() == 2);
    CHECK(c2.model().slots[1].name == "Alice");
    CHECK(host.model().slots[1].pid_lo == 0x2222u);

    // Both ask for colour 4: the first wins, the second keeps Random.
    c1.request_slot(2, 4, 1, 0);
    pump(host, {&c1, &c2}, t, [&] { return host.model().slots[1].color == 4; });
    c2.request_slot(5, 4, 1, 0);
    pump(host, {&c1, &c2}, t, [&] { return c2.rev() == host.rev() && host.model().slots[2].country == 5; });
    CHECK(host.model().slots[1].color == 4);
    CHECK(host.model().slots[2].color == kRandom);
    CHECK(host.model().slots[2].start == kRandomStart);   // start 0 taken by Alice
    CHECK(c2.model().slots[1].team == 1);

    // An AI in slot 3; not everyone ready yet -> no start.
    host.set_slot_kind(3, SlotKind::AI, 2);
    CHECK(host.start(777) == StartCheck::NotReady);
    c1.set_ready(true);
    c2.set_ready(true);
    REQUIRE(pump(host, {&c1, &c2}, t, [&] { return host.model().slots[1].ready && host.model().slots[2].ready; }));
    // An option change clears every ready flag.
    MpOptions o = host.model().opt;
    o.credits = 5000;
    host.set_options(o);
    CHECK_FALSE(host.model().slots[1].ready);
    pump(host, {&c1, &c2}, t, [&] { return c1.rev() == host.rev() && !c1.ready(); });
    CHECK(c1.model().opt.credits == 5000);
    c1.set_ready(true);
    c2.set_ready(true);
    REQUIRE(pump(host, {&c1, &c2}, t, [&] { return can_start(host.model()) == StartCheck::Ok; }));

    // Chat: Alice -> host -> Bob (sender stamped by the host).
    c1.chat("gl hf");
    host.chat("go go go");
    REQUIRE(pump(host, {&c1, &c2}, t, [&] {
        bool b_got = false, a_got = false;
        for (const auto& l : c2.chat_log()) b_got = b_got || (l.from == "Alice" && l.text == "gl hf");
        for (const auto& l : c1.chat_log()) a_got = a_got || (l.from == "Host" && l.text == "go go go");
        return a_got && b_got;
    }));

    // Start: everyone ends with the same seed, options and house table.
    REQUIRE(host.start(0xC0FFEE) == StartCheck::Ok);
    REQUIRE(pump(host, {&c1, &c2}, t, [&] { return host.started() && c1.started() && c2.started(); }));
    CHECK(c1.state() == LobbyClient::State::Started);
    CHECK(host.session().seed == 0xC0FFEEu);
    CHECK(c1.session().seed == 0xC0FFEEu);
    CHECK(c1.session().options == host.session().options);
    CHECK(c2.session().options == host.session().options);
    CHECK(host.session().players.size() == 4u);
    CHECK(c2.session().local_house == 2);
    CHECK(host.session().local_house == 0);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(c1.session().players[i].color == host.session().players[i].color);
        CHECK(c2.session().players[i].start == host.session().players[i].start);
    }
    // NET-PLAY: GO's peer table -- every machine knows every other human's
    // transport peer (the match is a full mesh), AIs and itself have none.
    CHECK(host.session().players[1].peer == mem_peer(2));
    CHECK(host.session().players[2].peer == mem_peer(3));
    CHECK(c1.session().players[0].peer == mem_peer(1));
    CHECK(c1.session().players[1].peer == 0u);           // itself
    CHECK(c1.session().players[2].peer == mem_peer(3));  // Bob, as the host sees him
    CHECK(c2.session().players[1].peer == mem_peer(2));
    CHECK(c2.session().players[3].peer == 0u);           // the AI
}

TEST_CASE("mp: version / rules gate, full room, kick, leave") {
    std::uint64_t t = 0;
    MemNet hub(t);
    MpNet& eh = hub.add(1);
    LobbyHost host(eh, ident("Host", 1), host_model());
    {
        MpNet& e = hub.add(2);
        MpIdentity old = ident("Old", 2);
        old.net_proto = 99;
        LobbyClient c(e, mem_peer(1), old);
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Version);
    }
    {
        MpNet& e = hub.add(3);
        LobbyClient c(e, mem_peer(1), ident("Modder", 3, 0xBAD));
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Rules);
    }
    // Fill the 4-player map: host + AI + AI + one human; the next is refused.
    host.set_slot_kind(1, SlotKind::AI, 1);
    host.set_slot_kind(2, SlotKind::AI, 1);
    MpNet& e4 = hub.add(4);
    MpNet& e5 = hub.add(5);
    LobbyClient c4(e4, mem_peer(1), ident("Four", 4));
    REQUIRE(pump(host, {&c4}, t, [&] { return c4.state() == LobbyClient::State::Joined; }));
    LobbyClient c5(e5, mem_peer(1), ident("Five", 5));
    pump(host, {&c4, &c5}, t, [&] { return c5.state() == LobbyClient::State::Rejected; });
    CHECK(c5.reject_reason() == RejectReason::Full);
    host.kick(3);
    REQUIRE(pump(host, {&c4}, t, [&] { return c4.state() == LobbyClient::State::Rejected; }));
    CHECK(c4.reject_reason() == RejectReason::Kicked);
    CHECK(host.model().slots[3].kind == SlotKind::Open);
    MpNet& e6 = hub.add(6);
    LobbyClient c6(e6, mem_peer(1), ident("Six", 6));
    REQUIRE(pump(host, {&c6}, t, [&] { return c6.state() == LobbyClient::State::Joined; }));
    c6.leave();
    REQUIRE(pump(host, {}, t, [&] { return host.members() == 0; }));
    CHECK(host.model().slots[3].kind == SlotKind::Open);
}

// W9-05-02: a different BUILD (sim version) or different game data is refused
// before the match, and BOTH sides are told which.
TEST_CASE("mp: different build / game data -> clear message on the joiner AND the host (W9-05-02)") {
    std::uint64_t t = 0;
    MemNet hub(t);
    MpNet& eh = hub.add(1);
    MpIdentity hid = ident("Host", 1);
    hid.version = "0.9.0+gaaaaaaa";
    LobbyHost host(eh, hid, host_model());
    auto host_said = [&](const std::string& needle) {
        for (const ChatLine& l : host.chat_log())
            if (l.text.find(needle) != std::string::npos) return true;
        return false;
    };
    {
        MpNet& e = hub.add(2);
        MpIdentity other = ident("Other", 2);
        other.sim_ver = sim_version() ^ 0x5A5A5A5Au;   // another commit's build
        other.version = "0.9.0+gbbbbbbb";
        LobbyClient c(e, mem_peer(1), other);
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Version);
        CHECK(c.reject_message().find("0.9.0+gbbbbbbb") != std::string::npos);   // mine
        CHECK(c.reject_message().find("0.9.0+gaaaaaaa") != std::string::npos);   // the host's
        CHECK(host_said("Other could not join: different game version (theirs: 0.9.0+gbbbbbbb)"));
    }
    {
        MpNet& e = hub.add(3);
        LobbyClient c(e, mem_peer(1), ident("Modder", 3, 0xBAD));
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Rules);
        CHECK(c.reject_message().find("artmd.ini") != std::string::npos);
        CHECK(host_said("Modder could not join: different game data"));
    }
    // Announced once per joiner, although the joiner re-queries.
    int n = 0;
    for (const ChatLine& l : host.chat_log()) n += l.text.find("could not join") != std::string::npos ? 1 : 0;
    CHECK(n == 2);
}

TEST_CASE("mp: the sim version comes from the build, not a constant (W9-05-02)") {
    CHECK(sim_version() != 1u);
    CHECK(sim_version() == sim_version());
    CHECK(std::string(sim_build_id()).size() > 0);
    CHECK(MpIdentity{}.sim_ver == sim_version());
}

TEST_CASE("mp: lobby converges over a lossy link (resend + dedupe)") {
    std::uint64_t t = 0;
    MemNet hub(t);
    hub.hub.set_loss(0.3);
    hub.hub.set_duplicate(0.1);
    MpNet& eh = hub.add(1);
    MpNet& ec = hub.add(2);
    LobbyHost host(eh, ident("Host", 1), host_model());
    LobbyClient c(ec, mem_peer(1), ident("Lossy", 2));
    // (With this loss pattern the CONFIRM is lost and arrives after the
    // options: the join must still complete.)
    REQUIRE(pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Joined && c.rev() == host.rev(); }));
    c.request_slot(1, 6, -1, 1);
    c.chat("one");
    c.chat("two");
    REQUIRE(pump(host, {&c}, t, [&] { return host.model().slots[1].color == 6 && c.rev() == host.rev(); }));
    int ones = 0;
    for (const auto& l : host.chat_log()) ones += l.text == "one" ? 1 : 0;
    CHECK(ones == 1);   // duplicates filtered
    c.set_ready(true);
    REQUIRE(pump(host, {&c}, t, [&] { return host.model().slots[1].ready; }));
    REQUIRE(host.start(5) == StartCheck::Ok);
    REQUIRE(pump(host, {&c}, t, [&] { return host.started() && c.started(); }));
    CHECK(c.session().options == host.session().options);
}

TEST_CASE("mp: host lost -> client reports it") {
    std::uint64_t t = 0;
    MemNet hub(t);
    MpNet& eh = hub.add(1);
    MpNet& ec = hub.add(2);
    LobbyHost host(eh, ident("Host", 1), host_model());
    LobbyClient c(ec, mem_peer(1), ident("C", 2));
    REQUIRE(pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Joined; }));
    for (int i = 0; i < 400 && c.state() == LobbyClient::State::Joined; ++i) {
        t += 100;
        c.update(t);   // the host stops answering
    }
    CHECK(c.state() == LobbyClient::State::Lost);
}

TEST_CASE("mp: lobby over real loopback UDP sockets") {
    MpNet th, tc;
    REQUIRE(th.open_direct(0));
    REQUIRE(tc.open_direct(0));
    LobbyHost host(th, ident("Host", 1), host_model());
    LobbyClient c(tc, peer_of(Addr{kLoopback, th.port()}), ident("Udp", 2));
    bool ok = false;
    for (int i = 0; i < 400 && !ok; ++i) {
        const std::uint64_t t = now_ms();
        host.update(t);
        c.update(t);
        ok = c.state() == LobbyClient::State::Joined && c.rev() == host.rev();
        if (!ok) {
            const std::uint64_t until = now_ms() + 5;
            while (now_ms() < until) {}
        }
    }
    REQUIRE(ok);
    CHECK(c.model().slots[0].name == "Host");
}

TEST_CASE("mp: lobby through a CnCNet V3 tunnel (8-byte LE header)") {
    // A minimal V3 forwarder: register sender -> endpoint, forward verbatim.
    UdpSocket relay;
    REQUIRE(relay.open(0));
    const Addr ra{kLoopback, relay.local_port()};
    std::map<std::uint32_t, Addr> reg;
    int forwarded = 0;
    auto run_relay = [&]() {
        std::uint8_t buf[2048];
        Addr from;
        for (int n; (n = relay.recv_from(from, buf, sizeof buf)) > 0;) {
            std::uint32_t s, r;
            const std::uint8_t* pl;
            std::size_t pn;
            if (n == 50 && buf[0] == 0 && buf[4] == 0) { relay.send_to(from, buf, 12); continue; }
            if (!ra2::net::tunnel_unwrap(ra2::net::TunnelMode::V3, buf, static_cast<std::size_t>(n), &s, &r, &pl, &pn) ||
                s == 0)
                continue;
            reg[s] = from;
            const auto it = reg.find(r);
            if (it != reg.end()) { relay.send_to(it->second, buf, static_cast<std::size_t>(n)); ++forwarded; }
        }
    };
    MpNet th, tc;
    REQUIRE(th.open_tunnel(ra, 0xAAAA0001));
    REQUIRE(tc.open_tunnel(ra, 0xBBBB0002));
    LobbyHost host(th, ident("Host", 1), host_model());
    LobbyClient c(tc, peer_of_v3(0xAAAA0001), ident("Tunnel", 2));
    bool ok = false;
    for (int i = 0; i < 600 && !ok; ++i) {
        const std::uint64_t t = now_ms();
        host.update(t);
        run_relay();
        c.update(t);
        run_relay();
        ok = c.state() == LobbyClient::State::Joined && c.rev() == host.rev();
        if (!ok) {
            const std::uint64_t until = now_ms() + 5;
            while (now_ms() < until) {}
        }
    }
    REQUIRE(ok);
    CHECK(forwarded > 2);
    CHECK(host.peer_of_slot(1) == peer_of_v3(0xBBBB0002));
    // The tunnel answered our 50-byte ping (seen through UdpDatagramIO::on_foreign).
    for (int i = 0; i < 200 && th.rtt_ms() < 0; ++i) {
        run_relay();
        th.net().service();
        const std::uint64_t until = now_ms() + 2;
        while (now_ms() < until) {}
    }
    CHECK(th.rtt_ms() >= 0);
}

TEST_CASE("mp: NET-PLAY platform tag -- mixed platforms need the cross-platform option") {
    std::uint64_t t = 0;
    MemNet hub(t);
    MpNet& eh = hub.add(1);
    MpIdentity hi = ident("Host", 1);
    hi.platform = 1;
    LobbyHost host(eh, hi, host_model());
    MpNet& e2 = hub.add(2);
    MpIdentity droid = ident("Droid", 2);
    droid.platform = 2;
    {
        LobbyClient c(e2, mem_peer(1), droid);
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Platform);
    }
    MpOptions o = host.model().opt;
    o.cross_platform = true;
    host.set_options(o);
    MpNet& e3 = hub.add(3);
    LobbyClient c(e3, mem_peer(1), droid);
    REQUIRE(pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Joined; }));
    CHECK(c.model().opt.cross_platform);   // travels in the options string (xp=1)
}

TEST_CASE("mp: LAN beacon / query codec") {
    LanBeacon b;
    b.room = "Friday night";
    b.host = "Tanya";
    b.map = "Tour of Egypt";
    b.version = "0.9.0";
    b.rules_crc = 0x89ABCDEF;
    b.players = 3;
    b.max = 6;
    b.nonce = 77;
    b.game_port = 50021;
    b.started = true;
    const auto d = encode_beacon(b);
    CHECK(d.size() < 300u);
    LanBeacon back;
    REQUIRE(decode_beacon(d.data(), d.size(), back));
    CHECK(back.room == "Friday night");
    CHECK(back.host == "Tanya");
    CHECK(back.map == "Tour of Egypt");
    CHECK(back.rules_crc == 0x89ABCDEFu);
    CHECK(back.game_port == 50021);
    CHECK(back.players == 3);
    CHECK(back.started);
    LanBeacon q;
    q.type = 2;
    const auto qd = encode_beacon(q);
    REQUIRE(decode_beacon(qd.data(), qd.size(), back));
    CHECK(back.type == 2);
    std::vector<std::uint8_t> bad = d;
    bad[0] = 'X';
    CHECK_FALSE(decode_beacon(bad.data(), bad.size(), back));
    bad = d;
    bad[24] = 200;   // room length (offset 24) past the end
    CHECK_FALSE(decode_beacon(bad.data(), bad.size(), back));
    LanGame g{b, Addr{}, 0};
    g.beacon.net_proto = 1;
    g.beacon.sim_ver = 1;
    CHECK(LanBrowser::compatible(g, 1, 1, 0x89ABCDEF));
    CHECK_FALSE(LanBrowser::compatible(g, 1, 1, 0x1));
    CHECK_FALSE(broadcast_targets().empty());
}

TEST_CASE("mp: mini JSON for the ra2lobbyd control plane") {
    Json j;
    REQUIRE(json_parse(R"({"t":"rooms","rooms":[{"code":"K7Q2MX","players":2,"pw":true,"name":"a\"bü"}],"v3":4294967295})", j));
    CHECK(j.s("t") == "rooms");
    REQUIRE(j["rooms"].arr.size() == 1u);
    CHECK(j["rooms"].arr[0].s("code") == "K7Q2MX");
    CHECK(j["rooms"].arr[0].i("players") == 2);
    CHECK(j["rooms"].arr[0].flag("pw"));
    CHECK(j["rooms"].arr[0].s("name") == "a\"b\xC3\xBC");
    CHECK(j.u32("v3") == 0xFFFFFFFFu);
    CHECK(j.s("missing", "d") == "d");
    CHECK_FALSE(json_parse("{\"t\":", j));
    CHECK_FALSE(json_parse("{} x", j));
    JsonWriter w;
    w.str("t", "hello").str("nick", "Q\"x").num("proto", 1).u64("v3", 4000000000ull).boolean("private", false);
    REQUIRE(json_parse(w.done(), j));
    CHECK(j.s("nick") == "Q\"x");
    CHECK(j.u32("v3") == 4000000000u);
    CHECK_FALSE(j.flag("private"));
}
