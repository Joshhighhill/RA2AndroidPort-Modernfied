// engine/mp/mp_socket.cpp -- see mp_socket.hpp (interim minimal socket layer).
#include "mp/mp_socket.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ra2mp {

namespace {

#ifdef _WIN32
using Fd = std::uintptr_t;
const Fd kBad = ~static_cast<Fd>(0);
void close_fd(Fd f) { ::closesocket(static_cast<SOCKET>(f)); }
bool would_block() {
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
}
bool set_nonblock(Fd f) {
    u_long one = 1;
    return ioctlsocket(static_cast<SOCKET>(f), FIONBIO, &one) == 0;
}
#else
using Fd = int;
const Fd kBad = -1;
void close_fd(Fd f) { ::close(f); }
bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }
bool set_nonblock(Fd f) {
    const int fl = fcntl(f, F_GETFL, 0);
    return fl >= 0 && fcntl(f, F_SETFL, fl | O_NONBLOCK) == 0;
}
#endif

sockaddr_in to_sa(const Addr& a) {
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(a.ip);
    sa.sin_port = htons(a.port);
    return sa;
}

Addr from_sa(const sockaddr_in& sa) {
    return Addr{ntohl(sa.sin_addr.s_addr), ntohs(sa.sin_port)};
}

}  // namespace

std::string Addr::ip_str() const {
    return std::to_string((ip >> 24) & 255) + "." + std::to_string((ip >> 16) & 255) + "." +
           std::to_string((ip >> 8) & 255) + "." + std::to_string(ip & 255);
}

std::string Addr::str() const { return ip_str() + ":" + std::to_string(port); }

bool net_init() {
#ifdef _WIN32
    static int state = 0;   // 0 untried, 1 ok, -1 failed
    if (state == 0) {
        WSADATA d;
        state = WSAStartup(MAKEWORD(2, 2), &d) == 0 ? 1 : -1;
    }
    return state == 1;
#else
    return true;
#endif
}

bool parse_ipv4(const std::string& s, std::uint32_t& ip) {
    unsigned a, b, c, d;
    char tail;
    if (std::sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    ip = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

bool resolve(const std::string& host_port, std::uint16_t default_port, Addr& out) {
    std::string host = host_port;
    std::uint16_t port = default_port;
    const std::size_t colon = host_port.rfind(':');
    if (colon != std::string::npos) {
        host = host_port.substr(0, colon);
        const std::string ps = host_port.substr(colon + 1);
        char* end = nullptr;
        const long p = std::strtol(ps.c_str(), &end, 10);
        if (ps.empty() || *end || p <= 0 || p > 65535) return false;
        port = static_cast<std::uint16_t>(p);
    }
    while (!host.empty() && host.front() == ' ') host.erase(host.begin());
    while (!host.empty() && host.back() == ' ') host.pop_back();
    if (host.empty() || port == 0) return false;
    std::uint32_t ip = 0;
    if (parse_ipv4(host, ip)) {
        out = Addr{ip, port};
        return true;
    }
    if (!net_init()) return false;
    addrinfo hints;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    bool ok = false;
    for (addrinfo* r = res; r; r = r->ai_next)
        if (r->ai_family == AF_INET) {
            out = Addr{ntohl(reinterpret_cast<sockaddr_in*>(r->ai_addr)->sin_addr.s_addr), port};
            ok = true;
            break;
        }
    freeaddrinfo(res);
    return ok;
}

namespace {
// (address, netmask) of every up IPv4 interface.
std::vector<std::pair<std::uint32_t, std::uint32_t>> interfaces() {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    if (!net_init()) return out;
#ifdef _WIN32
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return out;
    INTERFACE_INFO info[32];
    DWORD got = 0;
    if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, nullptr, 0, info, sizeof info, &got, nullptr, nullptr) == 0) {
        const int n = static_cast<int>(got / sizeof(INTERFACE_INFO));
        for (int i = 0; i < n; ++i) {
            if (!(info[i].iiFlags & IFF_UP)) continue;
            const auto* a = reinterpret_cast<const sockaddr_in*>(&info[i].iiAddress);
            const auto* m = reinterpret_cast<const sockaddr_in*>(&info[i].iiNetmask);
            if (a->sin_family != AF_INET) continue;
            out.push_back({ntohl(a->sin_addr.s_addr), ntohl(m->sin_addr.s_addr)});
        }
    }
    ::closesocket(s);
#else
    // SIOCGIFCONF works on every Android API level (getifaddrs needs 24+).
    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return out;
    char buf[4096];
    ifconf ifc;
    ifc.ifc_len = sizeof buf;
    ifc.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &ifc) == 0) {
        for (int off = 0; off + static_cast<int>(sizeof(ifreq)) <= ifc.ifc_len; off += sizeof(ifreq)) {
            ifreq* r = reinterpret_cast<ifreq*>(buf + off);
            if (r->ifr_addr.sa_family != AF_INET) continue;
            const std::uint32_t ip = ntohl(reinterpret_cast<sockaddr_in*>(&r->ifr_addr)->sin_addr.s_addr);
            ifreq q;
            std::memcpy(&q, r, sizeof q);
            if (ioctl(s, SIOCGIFFLAGS, &q) != 0 || !(q.ifr_flags & IFF_UP)) continue;
            std::memcpy(&q, r, sizeof q);
            std::uint32_t mask = 0xFFFFFF00u;
            if (ioctl(s, SIOCGIFNETMASK, &q) == 0)
                mask = ntohl(reinterpret_cast<sockaddr_in*>(&q.ifr_netmask)->sin_addr.s_addr);
            out.push_back({ip, mask});
        }
    }
    ::close(s);
#endif
    return out;
}
}  // namespace

std::vector<std::uint32_t> broadcast_targets() {
    std::vector<std::uint32_t> out;
    for (const auto& im : interfaces()) {
        if ((im.first >> 24) == 127 || im.first == 0) continue;
        const std::uint32_t mask = im.second ? im.second : 0xFFFFFF00u;
        if (mask == 0xFFFFFFFFu) continue;           // point-to-point (VPN /32): no broadcast
        out.push_back(im.first | ~mask);
    }
    out.push_back(kBroadcast);
    out.push_back(kLoopback);                        // same-machine hosts (two instances on one PC)
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<std::uint32_t> local_ipv4s() {
    std::vector<std::uint32_t> out;
    for (const auto& im : interfaces())
        if ((im.first >> 24) != 127 && im.first != 0) out.push_back(im.first);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// ------------------------------------------------------------ UdpSocket --

UdpSocket::~UdpSocket() { close(); }

UdpSocket::UdpSocket(UdpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = kInvalid; }

UdpSocket& UdpSocket::operator=(UdpSocket&& o) noexcept {
    if (this != &o) {
        close();
        fd_ = o.fd_;
        o.fd_ = kInvalid;
    }
    return *this;
}

bool UdpSocket::open(std::uint16_t port, bool reuse, bool broadcast, std::uint32_t bind_ip) {
    close();
    if (!net_init()) return false;
    const auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) return false;
    fd_ = static_cast<Fd>(s);
    // Windows reports an ICMP port-unreachable for an earlier send as a
    // WSAECONNRESET on the NEXT recvfrom; turn that off (SIO_UDP_CONNRESET).
    BOOL off = FALSE;
    DWORD ret = 0;
    WSAIoctl(s, _WSAIOW(IOC_VENDOR, 12), &off, sizeof off, nullptr, 0, &ret, nullptr, nullptr);
#else
    if (s < 0) return false;
    fd_ = s;
#endif
    const int one = 1;
    if (reuse) {
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof one);
#if defined(SO_REUSEPORT) && !defined(_WIN32)
        setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&one), sizeof one);
#endif
    }
    if (broadcast)
        setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&one), sizeof one);
    const sockaddr_in sa = to_sa(Addr{bind_ip, port});
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&sa), sizeof sa) != 0 || !set_nonblock(fd_)) {
        close();
        return false;
    }
    return true;
}

void UdpSocket::close() {
    if (fd_ != kInvalid) close_fd(fd_);
    fd_ = kInvalid;
}

std::uint16_t UdpSocket::local_port() const {
    if (fd_ == kInvalid) return 0;
    sockaddr_in sa;
    socklen_t len = sizeof sa;
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&sa), &len) != 0) return 0;
    return ntohs(sa.sin_port);
}

bool UdpSocket::send_to(const Addr& to, const void* data, std::size_t len) {
    if (fd_ == kInvalid) return false;
    const sockaddr_in sa = to_sa(to);
    const auto n = ::sendto(fd_, static_cast<const char*>(data), static_cast<int>(len), 0,
                            reinterpret_cast<const sockaddr*>(&sa), sizeof sa);
    return n == static_cast<decltype(n)>(len);
}

int UdpSocket::recv_from(Addr& from, void* buf, std::size_t cap) {
    if (fd_ == kInvalid) return -1;
    for (int guard = 0; guard < 64; ++guard) {
        sockaddr_in sa;
        socklen_t len = sizeof sa;
        const auto n = ::recvfrom(fd_, static_cast<char*>(buf), static_cast<int>(cap), 0,
                                  reinterpret_cast<sockaddr*>(&sa), &len);
        if (n >= 0) {
            from = from_sa(sa);
            return static_cast<int>(n);
        }
        if (would_block()) return 0;
#ifdef _WIN32
        const int e = WSAGetLastError();
        if (e == WSAECONNRESET || e == WSAEMSGSIZE) continue;   // stale ICMP / oversize: skip it
#else
        if (errno == EINTR || errno == ECONNREFUSED) continue;
#endif
        return -1;
    }
    return 0;
}

// ------------------------------------------------------------ TcpStream --

TcpStream::~TcpStream() { close(); }

void TcpStream::close() {
    if (fd_ != UdpSocket::kInvalid) close_fd(fd_);
    fd_ = UdpSocket::kInvalid;
    state_ = State::Closed;
    in_.clear();
    out_.clear();
}

bool TcpStream::connect(const Addr& to) {
    close();
    err_.clear();
    if (!net_init()) { state_ = State::Failed; err_ = "network unavailable"; return false; }
    const auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) { state_ = State::Failed; err_ = "socket"; return false; }
#else
    if (s < 0) { state_ = State::Failed; err_ = "socket"; return false; }
#endif
    fd_ = static_cast<UdpSocket::Fd>(s);
    const int one = 1;
    setsockopt(fd_, IPPROTO_TCP, 1 /*TCP_NODELAY*/, reinterpret_cast<const char*>(&one), sizeof one);
    set_nonblock(fd_);
    const sockaddr_in sa = to_sa(to);
    if (::connect(fd_, reinterpret_cast<const sockaddr*>(&sa), sizeof sa) == 0) {
        state_ = State::Open;
        return true;
    }
    if (!would_block()) {
        close();
        state_ = State::Failed;
        err_ = "connection refused";
        return false;
    }
    state_ = State::Connecting;
    return true;
}

TcpStream::State TcpStream::poll() {
    if (state_ == State::Connecting) {
        fd_set wr, ex;
        FD_ZERO(&wr);
        FD_ZERO(&ex);
        FD_SET(fd_, &wr);
        FD_SET(fd_, &ex);
        timeval tv{0, 0};
        const int r = ::select(static_cast<int>(fd_ + 1), nullptr, &wr, &ex, &tv);
        if (r < 0) { close(); state_ = State::Failed; err_ = "select"; return state_; }
        if (r == 0) return state_;
        int soerr = 0;
        socklen_t len = sizeof soerr;
        getsockopt(fd_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr), &len);
        if (FD_ISSET(fd_, &ex) || soerr != 0) {
            close();
            state_ = State::Failed;
            err_ = "could not connect";
            return state_;
        }
        state_ = State::Open;
    }
    if (state_ != State::Open) return state_;
    while (!out_.empty()) {
        const auto n = ::send(fd_, out_.data(), static_cast<int>(out_.size()), 0);
        if (n > 0) { out_.erase(0, static_cast<std::size_t>(n)); continue; }
        if (n < 0 && would_block()) break;
        close();
        state_ = State::Failed;
        err_ = "connection lost";
        return state_;
    }
    char buf[4096];
    for (;;) {
        const auto n = ::recv(fd_, buf, sizeof buf, 0);
        if (n > 0) {
            in_.append(buf, static_cast<std::size_t>(n));
            if (in_.size() > (1u << 20)) { close(); state_ = State::Failed; err_ = "flood"; return state_; }
            continue;
        }
        if (n < 0 && would_block()) break;
        // n == 0: orderly close (keep the lines already received).
        const std::string keep = in_;
        close();
        in_ = keep;
        state_ = State::Failed;
        err_ = n == 0 ? "server closed the connection" : "connection lost";
        break;
    }
    return state_;
}

void TcpStream::send(const std::string& bytes) { out_ += bytes; }

bool TcpStream::pop_line(std::string& line) {
    const std::size_t nl = in_.find('\n');
    if (nl == std::string::npos) return false;
    line = in_.substr(0, nl);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    in_.erase(0, nl + 1);
    return true;
}

std::uint64_t now_ms() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace ra2mp
