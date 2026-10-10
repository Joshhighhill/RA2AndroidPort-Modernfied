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

#include <set>

using namespace ra2mp;

TEST_CASE("mp: Add_CRC is rotl1-then-add over LE words with a masked tail") {
    const std::uint8_t a[4] = {1, 0, 0, 0};
    CHECK(add_crc(a, 4) == 1u);
    const std::uint8_t b[8] = {1, 0, 0, 0, 2, 0, 0, 0};
    CHECK(add_crc(b, 8) == ((1u << 1) + 2u));
    const std::uint8_t c[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0x07};
    // crc = 0xFFFFFFFF; tail word 0x07 (mask 0xFF): rotl1(0xFFFFFFFF) + 7
    CHECK(add_crc(c, 5) == 0xFFFFFFFFu + 7u);
}

TEST_CASE("mp: 0x1236 global packet layout (16-byte header, 455-byte body, CRC)") {
    GlobalPacket g;
    g.code = kDataAck;
    g.packet_id = 0x11223344;
    g.body = {0x0C, 0, 0, 0, 'h', 'i'};
    const std::vector<std::uint8_t> d = encode_global(g);
    REQUIRE(d.size() == 4 + 16 + 455);
    CHECK(d[4] == 0x36);
    CHECK(d[5] == 0x12);                  // magic 0x1236 LE
    CHECK(d[6] == kDataAck);
    CHECK(d[8] == 0x44);
    CHECK(d[11] == 0x11);                 // PacketID LE
    CHECK(d[18] == 0x04);
    CHECK(d[19] == 0xAA);                 // ProductID 0xAA04 at +14
    CHECK(d[20] == 0x0C);                 // body starts at +16
    GlobalPacket back;
    REQUIRE(decode_global(d.data(), d.size(), back));
    CHECK(back.packet_id == 0x11223344u);
    CHECK(back.body.size() == 455u);
    CHECK(back.body[4] == 'h');
    std::vector<std::uint8_t> bad = d;
    bad[30] ^= 1;
    CHECK_FALSE(decode_global(bad.data(), bad.size(), back));   // CRC
    GlobalPacket ra2 = g;
    ra2.product = 0xAA03;                                         // RA2 game.exe
    const auto r = encode_global(ra2);
    CHECK_FALSE(decode_global(r.data(), r.size(), back));
    GlobalPacket ack;
    ack.code = kAck;
    ack.packet_id = 9;
    CHECK(encode_global(ack).size() == 20u);                      // ACKs carry no body
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
    MemoryHub hub;
    auto* eh = hub.add(1);
    auto* e1 = hub.add(2);
    auto* e2 = hub.add(3);
    LobbyHost host(*eh, ident("Host", 0x1111), host_model());
    host.set_room_name("Room");
    LobbyClient c1(*e1, 1, ident("Alice", 0x2222));
    LobbyClient c2(*e2, 1, ident("Bob", 0x3333));
    c1.have_map = c2.have_map = [](const MpMap& m) { return m.digest == "abc"; };
    std::uint64_t t = 1000;
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
}

TEST_CASE("mp: version / rules gate, full room, kick, leave") {
    MemoryHub hub;
    auto* eh = hub.add(1);
    LobbyHost host(*eh, ident("Host", 1), host_model());
    std::uint64_t t = 0;
    {
        auto* e = hub.add(2);
        MpIdentity old = ident("Old", 2);
        old.net_proto = 99;
        LobbyClient c(*e, 1, old);
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Version);
    }
    {
        auto* e = hub.add(3);
        LobbyClient c(*e, 1, ident("Modder", 3, 0xBAD));
        pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Rejected; });
        CHECK(c.reject_reason() == RejectReason::Rules);
    }
    // Fill the 4-player map: host + AI + AI + one human; the next is refused.
    host.set_slot_kind(1, SlotKind::AI, 1);
    host.set_slot_kind(2, SlotKind::AI, 1);
    auto* e4 = hub.add(4);
    auto* e5 = hub.add(5);
    LobbyClient c4(*e4, 1, ident("Four", 4));
    REQUIRE(pump(host, {&c4}, t, [&] { return c4.state() == LobbyClient::State::Joined; }));
    LobbyClient c5(*e5, 1, ident("Five", 5));
    pump(host, {&c4, &c5}, t, [&] { return c5.state() == LobbyClient::State::Rejected; });
    CHECK(c5.reject_reason() == RejectReason::Full);
    host.kick(3);
    REQUIRE(pump(host, {&c4}, t, [&] { return c4.state() == LobbyClient::State::Rejected; }));
    CHECK(c4.reject_reason() == RejectReason::Kicked);
    CHECK(host.model().slots[3].kind == SlotKind::Open);
    auto* e6 = hub.add(6);
    LobbyClient c6(*e6, 1, ident("Six", 6));
    REQUIRE(pump(host, {&c6}, t, [&] { return c6.state() == LobbyClient::State::Joined; }));
    c6.leave();
    REQUIRE(pump(host, {}, t, [&] { return host.members() == 0; }));
    CHECK(host.model().slots[3].kind == SlotKind::Open);
}

TEST_CASE("mp: lobby converges over a lossy link (resend + dedupe)") {
    MemoryHub hub;
    auto* eh = hub.add(1);
    auto* ec = hub.add(2);
    eh->drop_every = 3;
    ec->drop_every = 4;
    LobbyHost host(*eh, ident("Host", 1), host_model());
    LobbyClient c(*ec, 1, ident("Lossy", 2));
    std::uint64_t t = 0;
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
    MemoryHub hub;
    auto* eh = hub.add(1);
    auto* ec = hub.add(2);
    LobbyHost host(*eh, ident("Host", 1), host_model());
    LobbyClient c(*ec, 1, ident("C", 2));
    std::uint64_t t = 0;
    REQUIRE(pump(host, {&c}, t, [&] { return c.state() == LobbyClient::State::Joined; }));
    for (int i = 0; i < 400 && c.state() == LobbyClient::State::Joined; ++i) {
        t += 100;
        c.update(t);   // the host stops answering
    }
    CHECK(c.state() == LobbyClient::State::Lost);
}

TEST_CASE("mp: lobby over real loopback UDP sockets") {
    UdpLobbyTransport th, tc;
    REQUIRE(th.open(0));
    REQUIRE(tc.open(0));
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
            if (!v3_unwrap(buf, static_cast<std::size_t>(n), s, r, pl, pn) || s == 0) continue;
            reg[s] = from;
            const auto it = reg.find(r);
            if (it != reg.end()) { relay.send_to(it->second, buf, static_cast<std::size_t>(n)); ++forwarded; }
        }
    };
    TunnelLobbyTransport th, tc;
    REQUIRE(th.open(ra, 0xAAAA0001));
    REQUIRE(tc.open(ra, 0xBBBB0002));
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
