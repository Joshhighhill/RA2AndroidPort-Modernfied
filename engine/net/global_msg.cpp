// engine/net/global_msg.cpp -- see global_msg.hpp.
#include "net/global_msg.hpp"

#include "net/net_bytes.hpp"

namespace ra2::net {

std::vector<uint8_t> encode_chat(const ChatMessage& m) {
    std::vector<uint8_t> b(kGlobalPacketSize, 0);
    put_u32le(b.data(), kGlobalChat);
    for (size_t i = 0; i < m.sender.size() && i < 0x2A; ++i) b[0x04 + i] = static_cast<uint8_t>(m.sender[i]);
    b[0x2F] = m.chat_mask;
    for (size_t i = 0; i < m.text.size() && i < 0x6F; ++i) put_u16le(b.data() + 0x30 + 2 * i, m.text[i]);
    put_i32le(b.data() + 0x110, m.color);
    put_u32le(b.data() + 0x114, m.game_hash);
    return b;
}

bool decode_chat(const std::vector<uint8_t>& b, ChatMessage& out) {
    if (b.size() < 0x118 || global_cmd(b) != kGlobalChat) return false;
    out = ChatMessage{};
    for (size_t i = 0x04; i < 0x2F && b[i]; ++i) out.sender.push_back(static_cast<char>(b[i]));
    out.chat_mask = b[0x2F];
    for (size_t i = 0x30; i + 1 < 0x110; i += 2) {
        const uint16_t c = get_u16le(b.data() + i);
        if (!c) break;
        out.text.push_back(static_cast<char16_t>(c));
    }
    out.color = get_i32le(b.data() + 0x110);
    out.game_hash = get_u32le(b.data() + 0x114);
    return true;
}

std::vector<uint8_t> encode_kick_vote(const KickVote& k) {
    std::vector<uint8_t> b(kGlobalPacketSize, 0);
    put_u32le(b.data(), kGlobalKickVote);
    for (size_t i = 0; i < k.name.size() && i < 19; ++i) b[0x04 + i] = static_cast<uint8_t>(k.name[i]);
    put_i32le(b.data() + 0x2F, k.target_house);
    put_i32le(b.data() + 0x33, k.proposer_house);
    return b;
}

bool decode_kick_vote(const std::vector<uint8_t>& b, KickVote& out) {
    if (b.size() < 0x37 || global_cmd(b) != kGlobalKickVote) return false;
    out = KickVote{};
    for (size_t i = 0x04; i < 0x18 && b[i]; ++i) out.name.push_back(static_cast<char>(b[i]));
    out.target_house = get_i32le(b.data() + 0x2F);
    out.proposer_house = get_i32le(b.data() + 0x33);
    return true;
}

std::vector<uint8_t> encode_away(const AwayNotice& a) {
    std::vector<uint8_t> b(kGlobalPacketSize, 0);
    put_u32le(b.data(), a.back ? kGlobalBack : kGlobalAway);
    put_i32le(b.data() + 0x04, a.house);
    put_u32le(b.data() + 0x08, a.seconds);
    return b;
}

bool decode_away(const std::vector<uint8_t>& b, AwayNotice& out) {
    const uint32_t c = global_cmd(b);
    if (b.size() < 0x0C || (c != kGlobalAway && c != kGlobalBack)) return false;
    out.back = c == kGlobalBack;
    out.house = get_i32le(b.data() + 0x04);
    out.seconds = get_u32le(b.data() + 0x08);
    return true;
}

std::u16string utf8_to_u16(const std::string& s) {
    std::u16string o;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = '?';
        std::size_t n = 1;
        if (c < 0x80) cp = c;
        else if ((c >> 5) == 6 && i + 1 < s.size()) { cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); n = 2; }
        else if ((c >> 4) == 14 && i + 2 < s.size()) {
            cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
            n = 3;
        } else if ((c >> 3) == 30) n = 4;
        o.push_back(cp <= 0xFFFF ? static_cast<char16_t>(cp) : u'?');
        i += n;
    }
    return o;
}

std::string u16_to_utf8(const std::u16string& s) {
    std::string o;
    for (char16_t ch : s) {
        const uint32_t c = ch;
        if (c < 0x80) o.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            o.push_back(static_cast<char>(0xC0 | (c >> 6)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            o.push_back('?');
        } else {
            o.push_back(static_cast<char>(0xE0 | (c >> 12)));
            o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return o;
}

}  // namespace ra2::net
