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

}  // namespace ra2::net
