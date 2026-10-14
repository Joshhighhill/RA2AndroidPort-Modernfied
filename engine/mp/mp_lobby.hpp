#pragma once
// engine/mp/mp_lobby -- the in-room multiplayer lobby (host-authoritative,
// star topology: every client talks only to the host), carried in the
// original 0x1236 global-channel framing (lobby_wire.hpp) over either a
// direct UDP socket (LAN / Join by IP) or the CnCNet V3 tunnel (online,
// through ra2lobbyd) -- MpNet (mp_net.hpp): engine/net's NetManager, whose
// endpoint the match keeps after GO. docs/gap/net/05-transport-infra.md §5.3 / §6.1.
//
//   client                          host
//   QUERY_JOIN (ver, rules, pid, platform)
//                                -> version / rules / platform gate, first open slot
//                                <- CONFIRM_JOIN(slot) | REJECT_JOIN(reason)
//                                <- GAME_OPTIONS(rev, options string chunks)
//   SLOT_REQUEST / READY_TO_GO   -> host validates (unique colour / start)
//   MESSAGE (original chat 0x0C) -> host relays to everyone
//                                <- GO(rev, seed, peer table) -> MpSessionConfig
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
#include <set>
#include <string>
#include <vector>

#include "mp/lobby_wire.hpp"
#include "mp/mp_model.hpp"
#include "mp/mp_net.hpp"

namespace ra2mp {

struct MpIdentity {
    std::string nick = "Player";
    std::uint64_t player_id = 0;
    std::uint16_t net_proto = kNetProto;
    std::uint32_t sim_ver = sim_version();   // W9-05-02: from the build
    std::uint32_t rules_crc = 0;
    std::string version;            // engine version string (informational)
    // NET-PLAY: 1 Windows, 2 Android arm64, 3 Android x86_64 (mp_model.hpp
    // platform_code()). Mixed platforms desync until the cross-platform
    // deterministic math (net L0) lands, so the host refuses them unless the
    // lobby's "cross-platform (experimental)" option is on.
    std::uint8_t platform = 0;
};

enum class RejectReason : std::uint8_t {
    None = 0, Version = 1, Rules = 2, Full = 3, Started = 4, Kicked = 5, HostLeft = 6, Lost = 7, Platform = 8
};
const char* reject_text(RejectReason r);
// W9-05-02: the joiner's message; names both builds on a version mismatch
// when the host's REJECT carried its version string.
std::string reject_message(RejectReason r, const std::string& mine, const std::string& host);

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
    // slot_peer: the transport peer of each remote human slot (the host's
    // view; a client gets it in GO).
    void build_session(bool is_host, const std::vector<std::pair<int, PeerId>>& slot_peer);
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
    LobbyHost(MpNet& t, const MpIdentity& me, const MpLobbyModel& initial);
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
    LobbyChannel ch_;
    MpIdentity me_;
    std::map<PeerId, Peer> peers_;
    std::set<std::pair<std::uint64_t, int>> told_rejects_;   // W9-05-02: (player id, reason) already announced
    bool dirty_ = false, going_ = false;
    std::uint64_t now_ = 0, go_at_ = 0;
};

class LobbyClient : public LobbyBase {
public:
    enum class State { Joining, Joined, Rejected, Lost, Started };
    LobbyClient(MpNet& t, PeerId host, const MpIdentity& me);
    void update(std::uint64_t now);

    void request_slot(int country, int color, int team, int start);
    void set_ready(bool ready);
    void chat(const std::string& text);
    void leave();
    // The UI tells whether the host's map is installed locally (digest match).
    std::function<bool(const MpMap&)> have_map;

    State state() const { return state_; }
    RejectReason reject_reason() const { return reason_; }
    // W9-05-02: what to tell the player (reject_text, plus both versions).
    std::string reject_message() const { return reject_message_.empty() ? std::string(reject_text(reason_)) : reject_message_; }
    bool ready() const { return ready_; }
    PeerId host() const { return host_; }

private:
    void handle(const std::vector<std::uint8_t>& body, std::uint64_t now);
    void send_ready(std::uint64_t now);
    LobbyChannel ch_;
    PeerId host_;
    MpIdentity me_;
    State state_ = State::Joining;
    RejectReason reason_ = RejectReason::None;
    std::string reject_message_;
    bool ready_ = false, has_map_ = true;
    std::uint64_t now_ = 0, last_query_ = 0, heard_ = 0, pinged_ = 0;
    int queries_ = 0;
    // GAME_OPTIONS reassembly
    std::uint32_t asm_rev_ = 0;
    std::vector<std::string> parts_;
    std::vector<bool> got_;
    std::uint32_t go_rev_ = 0, go_seed_ = 0;
    bool go_pending_ = false;
    std::vector<std::pair<int, PeerId>> go_peers_;   // GO's peer table (slot -> peer as the host sees it)
};

// Options-string chunking inside GAME_OPTIONS bodies (455 bytes each).
constexpr std::size_t kOptionsChunk = kGlobalBody - 12;
std::vector<std::vector<std::uint8_t>> options_bodies(std::uint32_t rev, const std::string& opts);

}  // namespace ra2mp
