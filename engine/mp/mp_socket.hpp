#pragma once
// engine/mp/mp_socket -- the minimal plain-socket layer the multiplayer shell
// (LAN discovery, lobby, server link) needs: non-blocking IPv4 UDP + TCP,
// name resolution, and the per-interface broadcast list.
//
// INTERIM: agent T owns the real transport (engine/net/: sockets, the
// 0xAA04/0x1236 framing, the tunnel header wrapper, the scheduler). Until it
// is merged this file is the "minimal interface" the shell depends on; it is
// deliberately tiny (no reliability, no threads) so swapping it for T's
// socket API is a mechanical change. SDL-free; compiled into ra2assets_tests.
//
// docs/gap/net/05-transport-infra.md §3 (LAN discovery: directed broadcast
// on every interface + 255.255.255.255).
#include <cstdint>
#include <string>
#include <vector>

namespace ra2mp {

// IPv4 address + port, both in HOST byte order.
struct Addr {
    std::uint32_t ip = 0;
    std::uint16_t port = 0;
    bool operator==(const Addr& o) const { return ip == o.ip && port == o.port; }
    bool operator!=(const Addr& o) const { return !(*this == o); }
    bool operator<(const Addr& o) const { return ip != o.ip ? ip < o.ip : port < o.port; }
    bool valid() const { return port != 0; }
    std::string ip_str() const;    // "192.168.1.5"
    std::string str() const;       // "192.168.1.5:50020"
    std::uint64_t key() const { return (std::uint64_t(ip) << 16) | port; }
};

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

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& o) noexcept;
    UdpSocket& operator=(UdpSocket&& o) noexcept;

    // Bind to port (0 = ephemeral) on all interfaces, non-blocking.
    // reuse: SO_REUSEADDR (several processes on one machine listening for
    // the same broadcast port); broadcast: SO_BROADCAST.
    bool open(std::uint16_t port, bool reuse = false, bool broadcast = false, std::uint32_t bind_ip = 0);
    void close();
    bool is_open() const { return fd_ != kInvalid; }
    std::uint16_t local_port() const;
    bool send_to(const Addr& to, const void* data, std::size_t len);
    // One datagram (non-blocking): >0 bytes, 0 none waiting, <0 error.
    int recv_from(Addr& from, void* buf, std::size_t cap);

private:
#ifdef _WIN32
    using Fd = std::uintptr_t;
    static constexpr Fd kInvalid = ~static_cast<Fd>(0);
#else
    using Fd = int;
    static constexpr Fd kInvalid = -1;
#endif
    Fd fd_ = kInvalid;
    friend class TcpStream;
};

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
    UdpSocket::Fd fd_ = UdpSocket::kInvalid;
    State state_ = State::Closed;
    std::string in_, out_, err_;
};

// Monotonic milliseconds (for timeouts that must follow the wall clock:
// network peers do not run on the game's virtual clock).
std::uint64_t now_ms();

}  // namespace ra2mp
