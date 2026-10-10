#pragma once
// engine/mp/lan_discovery -- LAN game discovery (docs/gap/net/05-transport-
// infra.md §3): the host's lobby sends a LAN_BEACON every second to every
// interface's directed broadcast + 255.255.255.255 (+127.0.0.1 for a second
// instance on the same PC) on UDP 50010; a browser listens on 50010 and also
// broadcasts a LAN_QUERY when it opens so hosts answer at once (unicast).
// Entries expire 4 s after their last beacon. Join uses the beacon's source
// address + game_port. Android: a WifiManager.MulticastLock is held while a
// browser or announcer is open (many Wi-Fi drivers filter broadcast frames
// in power-save otherwise) -- the platform installs the hook.
//
// Wire (little-endian): "RA2L" u16 net_proto u8 type(1 beacon, 2 query)
// u8 flags(bit0 started, bit1 password) u32 sim_ver u32 rules_crc
// u16 game_port u8 players u8 max u32 nonce, then u8-length-prefixed
// room name (<=32), host nick (<=16), map name (<=48), engine version (<=24).
// SDL-free.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mp/mp_socket.hpp"

namespace ra2mp {

constexpr std::uint16_t kLanBeaconPort = 50010;
constexpr std::uint16_t kLanGamePort = 50020;    // default host game port (RA2.INI [Net] LanPort)

struct LanBeacon {
    std::uint16_t net_proto = 1;
    std::uint8_t type = 1;          // 1 beacon, 2 query
    bool started = false, password = false;
    std::uint32_t sim_ver = 1, rules_crc = 0;
    std::uint16_t game_port = kLanGamePort;
    std::uint8_t players = 1, max = 8;
    std::uint32_t nonce = 0;        // per-host-instance id (de-duplicates interfaces)
    std::string room, host, map, version;
};

std::vector<std::uint8_t> encode_beacon(const LanBeacon& b);
bool decode_beacon(const std::uint8_t* p, std::size_t n, LanBeacon& out);

// Android MulticastLock (reference counted here; the hook does the JNI call).
void lan_set_multicast_hook(std::function<void(bool)> hook);
void lan_multicast_acquire();
void lan_multicast_release();

class LanAnnouncer {
public:
    ~LanAnnouncer() { close(); }
    bool open(const LanBeacon& b);
    void close();
    void set(const LanBeacon& b) { beacon_ = b; beacon_.type = 1; }
    const LanBeacon& beacon() const { return beacon_; }
    void update(std::uint64_t now);   // beacons every second, answers queries
    bool is_open() const { return sock_.is_open(); }

private:
    UdpSocket sock_;
    LanBeacon beacon_;
    std::vector<std::uint32_t> targets_;
    std::uint64_t last_ = 0, targets_at_ = 0;
    bool locked_ = false;
};

struct LanGame {
    LanBeacon beacon;
    Addr host;                      // source ip + game_port
    std::uint64_t seen = 0;
};

class LanBrowser {
public:
    ~LanBrowser() { close(); }
    bool open();                    // binds 50010 (shared) + a query socket, sends a query
    void close();
    void update(std::uint64_t now);
    const std::vector<LanGame>& games() const { return games_; }
    bool is_open() const { return listen_.is_open() || query_.is_open(); }
    bool listening() const { return listen_.is_open(); }   // false: 50010 busy -> query replies only
    // Filter: only beacons matching our protocol/sim/rules are "compatible".
    std::uint16_t net_proto = 1;
    std::uint32_t sim_ver = 1, rules_crc = 0;
    static bool compatible(const LanGame& g, std::uint16_t proto, std::uint32_t sim, std::uint32_t rules);

private:
    void take(const Addr& from, const std::uint8_t* p, int n, std::uint64_t now);
    UdpSocket listen_, query_;
    std::vector<LanGame> games_;
    std::vector<std::uint32_t> targets_;
    std::uint64_t last_query_ = 0;
    bool locked_ = false;
};

}  // namespace ra2mp
