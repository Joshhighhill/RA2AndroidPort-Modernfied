// engine/mp/lobby_wire.cpp -- see lobby_wire.hpp.
#include "mp/lobby_wire.hpp"

#include <algorithm>
#include <cstring>

namespace ra2mp {

std::uint32_t add_crc(const std::uint8_t* p, std::size_t n) {
    std::uint32_t crc = 0;
    const std::size_t words = n / 4;
    for (std::size_t i = 0; i < words; ++i) {
        const std::uint32_t w = std::uint32_t(p[i * 4]) | (std::uint32_t(p[i * 4 + 1]) << 8) |
                                (std::uint32_t(p[i * 4 + 2]) << 16) | (std::uint32_t(p[i * 4 + 3]) << 24);
        crc = ((crc << 1) | (crc >> 31)) + w;
    }
    const std::size_t tail = n & 3;
    if (tail) {
        std::uint32_t w = 0;
        for (std::size_t k = 0; k < tail; ++k) w |= std::uint32_t(p[words * 4 + k]) << (8 * k);
        // u32 read at the tail, masked to the valid low bytes.
        w &= 0xFFFFFFFFu >> (32 - 8 * tail);
        crc = ((crc << 1) | (crc >> 31)) + w;
    }
    return crc;
}

std::vector<std::uint8_t> encode_global(const GlobalPacket& g) {
    std::vector<std::uint8_t> payload(kGlobalHeader, 0);
    payload[0] = kGlobalMagic & 0xFF;
    payload[1] = kGlobalMagic >> 8;
    payload[2] = g.code;
    payload[3] = 0;
    for (int k = 0; k < 4; ++k) payload[4 + k] = static_cast<std::uint8_t>(g.packet_id >> (8 * k));
    // +8 u32 0, +12 u16 0 (already zero)
    payload[14] = g.product & 0xFF;
    payload[15] = g.product >> 8;
    if (g.code != kAck) {
        std::vector<std::uint8_t> body = g.body;
        body.resize(kGlobalBody, 0);
        payload.insert(payload.end(), body.begin(), body.end());
    }
    const std::uint32_t crc = add_crc(payload.data(), payload.size());
    std::vector<std::uint8_t> out(4);
    for (int k = 0; k < 4; ++k) out[k] = static_cast<std::uint8_t>(crc >> (8 * k));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool decode_global(const std::uint8_t* p, std::size_t n, GlobalPacket& out) {
    if (n < 4 + kGlobalHeader || n >= kMaxDatagram) return false;
    const std::uint32_t crc = std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
                              (std::uint32_t(p[3]) << 24);
    if (add_crc(p + 4, n - 4) != crc) return false;
    const std::uint8_t* h = p + 4;
    if ((h[0] | (h[1] << 8)) != kGlobalMagic) return false;
    out.code = h[2];
    if (out.code > kAck) return false;
    out.packet_id = std::uint32_t(h[4]) | (std::uint32_t(h[5]) << 8) | (std::uint32_t(h[6]) << 16) |
                    (std::uint32_t(h[7]) << 24);
    out.product = static_cast<std::uint16_t>(h[14] | (h[15] << 8));
    if (out.product != kProductYR) return false;   // RA2 (0xAA03) and YR filter each other (§2.4)
    out.body.clear();
    if (out.code != kAck) {
        if (n - 4 - kGlobalHeader < kGlobalBody) return false;
        out.body.assign(h + kGlobalHeader, h + kGlobalHeader + kGlobalBody);
    }
    return true;
}

void BodyWriter::fixed(const std::string& s, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) u8(i + 1 < width && i < s.size() ? static_cast<std::uint8_t>(s[i]) : 0);
}

std::string BodyReader::fixed(std::size_t width) {
    std::string s;
    bool end = false;
    for (std::size_t i = 0; i < width; ++i) {
        const std::uint8_t c = u8();
        if (c == 0) end = true;
        if (!end) s += static_cast<char>(c);
    }
    return s;
}

std::vector<std::uint16_t> utf8_to_utf16(const std::string& s) {
    std::vector<std::uint16_t> out;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = '?';
        std::size_t len = 1;
        if (c < 0x80) cp = c;
        else if ((c >> 5) == 6 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); len = 2; }
        else if ((c >> 4) == 14 && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            len = 3;
        } else if ((c >> 3) == 30) { cp = '?'; len = 4; }
        out.push_back(cp > 0xFFFF ? '?' : static_cast<std::uint16_t>(cp));
        i += len;
    }
    return out;
}

std::string utf16_to_utf8(const std::uint16_t* p, std::size_t n) {
    std::string o;
    for (std::size_t i = 0; i < n && p[i]; ++i) {
        const unsigned cp = p[i];
        if (cp >= 0xD800 && cp < 0xE000) { o += '?'; continue; }
        if (cp < 0x80) o += static_cast<char>(cp);
        else if (cp < 0x800) {
            o += static_cast<char>(0xC0 | (cp >> 6));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            o += static_cast<char>(0xE0 | (cp >> 12));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return o;
}

std::uint32_t game_name_hash(const std::string& name) {
    return add_crc(reinterpret_cast<const std::uint8_t*>(name.data()), name.size());
}

namespace {
constexpr std::size_t kChatName = 0x004, kChatMask = 0x02F, kChatText = 0x030, kChatColor = 0x110,
                      kChatHash = 0x114;
constexpr std::size_t kChatUnits = (kChatColor - kChatText) / 2;   // 112 UTF-16 units incl. NUL
}  // namespace

std::vector<std::uint8_t> encode_chat(const ChatPacket& c) {
    BodyWriter w;
    w.u32(kCmdMessage);
    w.at(kChatName);
    w.fixed(c.sender, kChatMask - kChatName);
    w.at(kChatMask);
    w.u8(c.chat_mask);
    w.at(kChatText);
    std::vector<std::uint16_t> u = utf8_to_utf16(c.text);
    if (u.size() > kChatUnits - 1) u.resize(kChatUnits - 1);
    for (std::size_t i = 0; i < kChatUnits; ++i) w.u16(i < u.size() ? u[i] : 0);
    w.at(kChatColor);
    w.i32(c.color);
    w.at(kChatHash);
    w.u32(c.game_hash);
    return w.b;
}

bool decode_chat(const std::vector<std::uint8_t>& body, ChatPacket& out) {
    if (body.size() < kChatHash + 4) return false;
    BodyReader r(body);
    if (r.u32() != kCmdMessage) return false;
    r.at(kChatName);
    out.sender = r.fixed(kChatMask - kChatName);
    r.at(kChatMask);
    out.chat_mask = r.u8();
    r.at(kChatText);
    std::uint16_t u[kChatUnits];
    for (std::size_t i = 0; i < kChatUnits; ++i) u[i] = r.u16();
    u[kChatUnits - 1] = 0;
    out.text = utf16_to_utf8(u, kChatUnits);
    r.at(kChatColor);
    out.color = r.i32();
    r.at(kChatHash);
    out.game_hash = r.u32();
    return !r.bad;
}

void v3_wrap(std::uint32_t sender, std::uint32_t receiver, const std::vector<std::uint8_t>& payload,
             std::vector<std::uint8_t>& out) {
    out.resize(kV3Header + payload.size());
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<std::uint8_t>(sender >> (8 * k));
        out[4 + k] = static_cast<std::uint8_t>(receiver >> (8 * k));
    }
    if (!payload.empty()) std::memcpy(out.data() + kV3Header, payload.data(), payload.size());
}

bool v3_unwrap(const std::uint8_t* p, std::size_t n, std::uint32_t& sender, std::uint32_t& receiver,
               const std::uint8_t*& payload, std::size_t& plen) {
    if (n < kV3Header) return false;
    sender = std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    receiver = std::uint32_t(p[4]) | (std::uint32_t(p[5]) << 8) | (std::uint32_t(p[6]) << 16) | (std::uint32_t(p[7]) << 24);
    payload = p + kV3Header;
    plen = n - kV3Header;
    return true;
}

std::vector<std::uint8_t> v3_ping_request(std::uint32_t nonce) {
    std::vector<std::uint8_t> p(50, 0);
    for (int k = 0; k < 4; ++k) p[8 + k] = static_cast<std::uint8_t>(nonce >> (8 * k));
    return p;
}

bool v3_is_ping_reply(const std::uint8_t* p, std::size_t n, std::uint32_t nonce) {
    if (n != 12) return false;
    for (int k = 0; k < 8; ++k)
        if (p[k] != 0) return false;
    const std::uint32_t got = std::uint32_t(p[8]) | (std::uint32_t(p[9]) << 8) | (std::uint32_t(p[10]) << 16) |
                              (std::uint32_t(p[11]) << 24);
    return got == nonce;
}

}  // namespace ra2mp
