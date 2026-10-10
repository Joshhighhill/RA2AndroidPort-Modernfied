#pragma once
// engine/mp/mp_lobby -- the in-room multiplayer lobby (host-authoritative,
// star topology: every client talks only to the host), carried in the
// original 0x1236 global-channel framing (lobby_wire.hpp) over either a
// direct UDP socket (LAN / Join by IP) or the CnCNet V3 tunnel (online,
// through ra2lobbyd). docs/gap/net/05-transport-infra.md §5.3 / §6.1.
//
//   client                          host
//   QUERY_JOIN (ver, rules, pid) -> version / rules gate, first open slot
//                                <- CONFIRM_JOIN(slot) | REJECT_JOIN(reason)
//                                <- GAME_OPTIONS(rev, options string chunks)
//   SLOT_REQUEST / READY_TO_GO   -> host validates (unique colour / start)
//   MESSAGE (original chat 0x0C) -> host relays to everyone
//                                <- GO(rev, seed)   -> MpSessionConfig
//   SIGN_OFF                     <-> slot freed / room closed
//   PING (DATA_NOACK, 2 s)       <-> peer lost after 15 s of silence
//
// Pure + clock-injected (now in ms) so it runs in ra2assets_tests; the UI
// calls update(now_ms()) every frame. SDL-free.
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mp/lobby_wire.hpp"
#include "mp/mp_model.hpp"
#include "mp/mp_socket.hpp"

namespace ra2mp {

using PeerId = std::uint64_t;
constexpr PeerId kTunnelPeerFlag = 1ull << 63;
inline PeerId peer_of(const Addr& a) { return a.key(); }
inline PeerId peer_of_v3(std::uint32_t v3) { return kTunnelPeerFlag | v3; }

// Datagram transport under the lobby (and, later, the game).
class LobbyTransport {
public:
    virtual ~LobbyTransport() = default;
    virtual bool send(PeerId to, const std::vector<std::uint8_t>& dgram) = 0;
    virtual bool recv(PeerId& from, std::vector<std::uint8_t>& dgram) = 0;   // false = nothing waiting
    virtual void tick(std::uint64_t now) { (void)now; }                       // keep-alives
    virtual std::string describe(PeerId p) const;
};

// Direct UDP (LAN, Join by IP). Peers are ip:port.
class UdpLobbyTransport : public LobbyTransport {
public:
    bool open(std::uint16_t port);          // 0 = ephemeral
    std::uint16_t port() const { return sock_.local_port(); }
    bool send(PeerId to, const std::vector<std::uint8_t>& dgram) override;
    bool recv(PeerId& from, std::vector<std::uint8_t>& dgram) override;
    std::string describe(PeerId p) const override;

private:
    UdpSocket sock_;
};

// CnCNet V3 tunnel: every datagram goes to the tunnel with the 8-byte
// [sender LE][receiver LE] header; peers are V3 ids. A keep-alive to a
// dummy receiver registers/refreshes our id at the tunnel.
class TunnelLobbyTransport : public LobbyTransport {
public:
    bool open(const Addr& tunnel, std::uint32_t my_v3);
    std::uint32_t my_v3() const { return me_; }
    const Addr& tunnel() const { return tunnel_; }
    bool send(PeerId to, const std::vector<std::uint8_t>& dgram) override;
    bool recv(PeerId& from, std::vector<std::uint8_t>& dgram) override;
    void tick(std::uint64_t now) override;
    std::string describe(PeerId p) const override;
    // Tunnel ping (50 -> 12 bytes); last measured RTT in ms (-1 unknown).
    int rtt_ms() const { return rtt_; }

private:
    UdpSocket sock_;
    Addr tunnel_;
    std::uint32_t me_ = 0;
    std::uint64_t last_keep_ = 0, ping_sent_ = 0;
    std::uint32_t ping_nonce_ = 0;
    int rtt_ = -1;
};

// In-memory pair for tests: two endpoints that deliver to each other.
class MemoryHub {
public:
    class Endpoint : public LobbyTransport {
    public:
        bool send(PeerId to, const std::vector<std::uint8_t>& d) override;
        bool recv(PeerId& from, std::vector<std::uint8_t>& d) override;
        PeerId id = 0;
        MemoryHub* hub = nullptr;
        std::deque<std::pair<PeerId, std::vector<std::uint8_t>>> inbox;
        int drop_every = 0, sent = 0;   // drop every Nth datagram (loss test)
    };
    Endpoint* add(PeerId id);
    std::map<PeerId, std::unique_ptr<Endpoint>> eps;
};

// The global channel's reliability: DATA_ACK packets are resent until ACKed
// (every 250 ms, 40 tries), duplicates filtered by (peer, PacketID).
class GlobalChannel {
public:
    explicit GlobalChannel(LobbyTransport& t) : t_(t) {}
    void send(PeerId to, const std::vector<std::uint8_t>& body, bool reliable, std::uint64_t now);
    struct In { PeerId from; std::vector<std::uint8_t> body; };
    // Receive + ACK + dedupe + resend. `lost` gets peers whose packets
    // exhausted their retries.
    void poll(std::uint64_t now, std::vector<In>& in, std::vector<PeerId>& lost);
    void forget(PeerId p);
    bool idle_to(PeerId p) const;   // nothing un-ACKed to p
    LobbyTransport& transport() { return t_; }

private:
    struct Pending { PeerId to; std::uint32_t id; std::vector<std::uint8_t> dgram; std::uint64_t last; int tries; };
    LobbyTransport& t_;
    std::uint32_t next_id_ = 1;
    std::vector<Pending> pend_;
    std::deque<std::pair<PeerId, std::uint32_t>> seen_;
};

struct MpIdentity {
    std::string nick = "Player";
    std::uint64_t player_id = 0;
    std::uint16_t net_proto = kNetProto;
    std::uint32_t sim_ver = kSimVersion;
    std::uint32_t rules_crc = 0;
    std::string version;            // engine version string (informational)
};

enum class RejectReason : std::uint8_t {
    None = 0, Version = 1, Rules = 2, Full = 3, Started = 4, Kicked = 5, HostLeft = 6, Lost = 7
};
const char* reject_text(RejectReason r);

struct ChatLine {
    std::string from;
    std::string text;
    int color = -1;                 // lobby colour of the sender (-1 system line)
};

// Shared bits of host + client.
class LobbyBase {
public:
    virtual ~LobbyBase() = default;
    const MpLobbyModel& model() const { return model_; }
    const std::deque<ChatLine>& chat_log() const { return chat_; }
    int local_slot() const { return local_slot_; }
    bool started() const { return started_; }
    const MpSessionConfig& session() const { return session_; }
    std::uint32_t rev() const { return rev_; }
    void set_room_name(const std::string& n) { room_ = n; }
    const std::string& room_name() const { return room_; }
    void system_line(const std::string& text);

protected:
    void add_chat(const ChatLine& l);
    void build_session(bool is_host);
    MpLobbyModel model_;
    std::deque<ChatLine> chat_;
    int local_slot_ = -1;
    bool started_ = false;
    MpSessionConfig session_;
    std::uint32_t rev_ = 0;
    std::string room_;
};

class LobbyHost : public LobbyBase {
public:
    LobbyHost(LobbyTransport& t, const MpIdentity& me, const MpLobbyModel& initial);
    void update(std::uint64_t now);

    // Host edits (any slot). Every change bumps the options revision and is
    // broadcast; option/map changes clear every ready flag.
    void set_slot_kind(int slot, SlotKind kind, int ai = 1);
    void set_slot(int slot, int country, int color, int team, int start);
    void set_options(const MpOptions& o);
    void set_map(const MpMap& m);
    void kick(int slot);
    void chat(const std::string& text);
    // Validates (can_start) and sends GO; started() turns true once every
    // client ACKed it (or after 3 s).
    StartCheck start(std::uint32_t seed);
    void close();                   // SIGN_OFF to everyone

    std::function<void(const std::string&)> on_event;   // join / leave lines for the log
    int members() const { return static_cast<int>(peers_.size()); }
    // Peer of a slot (0 = the host itself / AI / open).
    PeerId peer_of_slot(int slot) const;

private:
    struct Peer { int slot = -1; std::uint64_t heard = 0; std::uint64_t pinged = 0; };
    void handle(PeerId from, const std::vector<std::uint8_t>& body, std::uint64_t now);
    void broadcast_options(std::uint64_t now);
    void send_options(PeerId to, std::uint64_t now);
    void drop_peer(PeerId p, const char* why);
    void changed(bool clear_ready);
    GlobalChannel ch_;
    MpIdentity me_;
    std::map<PeerId, Peer> peers_;
    bool dirty_ = false, going_ = false;
    std::uint64_t now_ = 0, go_at_ = 0;
};

class LobbyClient : public LobbyBase {
public:
    enum class State { Joining, Joined, Rejected, Lost, Started };
    LobbyClient(LobbyTransport& t, PeerId host, const MpIdentity& me);
    void update(std::uint64_t now);

    void request_slot(int country, int color, int team, int start);
    void set_ready(bool ready);
    void chat(const std::string& text);
    void leave();
    // The UI tells whether the host's map is installed locally (digest match).
    std::function<bool(const MpMap&)> have_map;

    State state() const { return state_; }
    RejectReason reject_reason() const { return reason_; }
    bool ready() const { return ready_; }
    PeerId host() const { return host_; }

private:
    void handle(const std::vector<std::uint8_t>& body, std::uint64_t now);
    void send_ready(std::uint64_t now);
    GlobalChannel ch_;
    PeerId host_;
    MpIdentity me_;
    State state_ = State::Joining;
    RejectReason reason_ = RejectReason::None;
    bool ready_ = false, has_map_ = true;
    std::uint64_t now_ = 0, last_query_ = 0, heard_ = 0, pinged_ = 0;
    int queries_ = 0;
    // GAME_OPTIONS reassembly
    std::uint32_t asm_rev_ = 0;
    std::vector<std::string> parts_;
    std::vector<bool> got_;
    std::uint32_t go_rev_ = 0, go_seed_ = 0;
    bool go_pending_ = false;
};

// Options-string chunking inside GAME_OPTIONS bodies (455 bytes each).
constexpr std::size_t kOptionsChunk = kGlobalBody - 12;
std::vector<std::vector<std::uint8_t>> options_bodies(std::uint32_t rev, const std::string& opts);

}  // namespace ra2mp
