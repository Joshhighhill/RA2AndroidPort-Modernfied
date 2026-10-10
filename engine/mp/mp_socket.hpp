#pragma once
// engine/mp/mp_socket -- the minimal plain-socket layer the multiplayer shell
// (LAN discovery, lobby, server link) needs: non-blocking IPv4 UDP + TCP,
// name resolution, and the per-interface broadcast list.
//
// NET-PLAY: the UDP socket is engine/net's (UdpSocket below is an alias);
// this file keeps only what engine/net does not have: the TCP stream for the
// ra2lobbyd control plane, name resolution and the interface list. SDL-free;
// compiled into ra2assets_tests.
//
// docs/gap/net/05-transport-infra.md §3 (LAN discovery: directed broadcast
// on every interface + 255.255.255.255).
#include <cstdint>
#include <string>
#include <vector>

#include "net/udp_socket.hpp"

namespace ra2mp {

// IPv4 address + port, both in HOST byte order: engine/net's NetAddr plus
// the display / key helpers the shell uses.
struct Addr : ra2::net::NetAddr {
    Addr() = default;
    Addr(std::uint32_t ip_, std::uint16_t port_) : ra2::net::NetAddr{ip_, port_} {}
    Addr(const ra2::net::NetAddr& a) : ra2::net::NetAddr(a) {}   // NOLINT: implicit on purpose
    bool valid() const { return port != 0; }
    std::string ip_str() const;    // "192.168.1.5"
    std::string str() const;       // "192.168.1.5:50020"
    std::uint64_t key() const { return (std::uint64_t(ip) << 16) | port; }
};

// UDP: engine/net's socket (one implementation for lobby, beacons and game).
using UdpSocket = ra2::net::UdpSocket;

constexpr std::uint32_t kLoopback = 0x7F000001u;
constexpr std::uint32_t kBroadcast = 0xFFFFFFFFu;

// Process-wide init (WSAStartup on Windows). Idempotent; true when usable.
bool net_init();

// "1.2.3.4" -> ip (false on a malformed literal; no DNS).
bool parse_ipv4(const std::string& s, std::uint32_t& ip);
// "host[:port]" -> Addr. DNS lookup (blocking) when host is a name; the
// default port fills a missing ":port". False on failure.
bool resolve(const std::string& host_port, std::uint16_t default_port, Addr& out);

// Every IPv4 interface's directed broadcast address (e.g. 192.168.1.255,
// 192.168.43.255) plus 255.255.255.255 and 127.0.0.1, de-duplicated. The
// limited broadcast alone only leaves through the default-route interface,
// which misses an Android hotspot's clients (05 §3.1).
std::vector<std::uint32_t> broadcast_targets();
// The machine's own non-loopback IPv4 addresses (shown in the host's lobby
// for "Join by IP").
std::vector<std::uint32_t> local_ipv4s();

// Non-blocking TCP client stream with an internal receive buffer and
// line splitting (the ra2lobbyd control plane is JSON lines).
class TcpStream {
public:
    enum class State { Closed, Connecting, Open, Failed };
    TcpStream() = default;
    ~TcpStream();
    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;

    bool connect(const Addr& to);        // starts a non-blocking connect
    void close();
    // Advance the connect / flush the send queue / read. Call every frame.
    State poll();
    State state() const { return state_; }
    const std::string& error() const { return err_; }
    void send(const std::string& bytes); // queued; flushed by poll()
    // Pop one complete '\n'-terminated line (without the newline).
    bool pop_line(std::string& line);
    std::size_t pending_out() const { return out_.size(); }

private:
#ifdef _WIN32
    using Fd = std::uintptr_t;
    static constexpr Fd kInvalid = ~static_cast<Fd>(0);
#else
    using Fd = int;
    static constexpr Fd kInvalid = -1;
#endif
    Fd fd_ = kInvalid;
    State state_ = State::Closed;
    std::string in_, out_, err_;
};

// Monotonic milliseconds (for timeouts that must follow the wall clock:
// network peers do not run on the game's virtual clock).
std::uint64_t now_ms();

}  // namespace ra2mp
