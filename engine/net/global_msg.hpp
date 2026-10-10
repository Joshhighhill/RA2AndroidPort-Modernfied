// engine/net/global_msg.hpp -- in-game messages carried on the 0x1236
// global channel (GlobalPacketType, 0x1C7 bytes). They are out-of-band: they
// never enter the lockstep stream and never affect sync.
//
// Recovered layouts (docs/gap/net/01-original-protocol.md section 6):
//   chat  (cmd 0x0C, FUN_0055E050, handler case 0x0C in FUN_0048D0F0)
//     +0x000 u32 cmd  +0x004 ANSI sender name  +0x02F u8 router chat mask
//     +0x030 UTF-16 message  +0x110 i32 colour scheme  +0x114 u32 game-name hash
//   kick proposal (cmd 0x1C, FUN_0064A700)
//     +0x000 u32 cmd  +0x004 name[20]  +0x02F i32 target house  +0x033 i32 proposer
// Chat and kick proposals are sent DATA_ACK, one copy per recipient.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ra2::net {

constexpr size_t kGlobalPacketSize = 0x1C7;

enum GlobalCmd : uint32_t {
    kGlobalSignOff = 0x0A,     // NET_SIGN_OFF -> Destroy_Connection
    kGlobalChat = 0x0C,        // NET_MESSAGE
    kGlobalLoadProgress = 0x0F,
    kGlobalKickVote = 0x1C,
    kGlobalTaunt = 0x1D,
    kGlobalBeaconPlace = 0x20,
    kGlobalBeaconDelete = 0x21,
    kGlobalBeaconMessage = 0x22,
};

inline uint32_t global_cmd(const std::vector<uint8_t>& body) {
    return body.size() >= 4 ? (uint32_t(body[0]) | uint32_t(body[1]) << 8 | uint32_t(body[2]) << 16 |
                               uint32_t(body[3]) << 24)
                            : 0xFFFFFFFFu;
}

struct ChatMessage {
    std::string sender;       // ANSI, max 42 chars (0x04..0x2E)
    uint8_t chat_mask = 0;
    std::u16string text;      // max 111 UTF-16 units (+0x30..+0x10F)
    int32_t color = 0;
    uint32_t game_hash = 0;   // FUN_005DA620 over the session name (not computed here)
};
std::vector<uint8_t> encode_chat(const ChatMessage& m);  // 0x1C7 bytes
bool decode_chat(const std::vector<uint8_t>& body, ChatMessage& out);

struct KickVote {
    std::string name;         // max 19 chars
    int32_t target_house = -1;
    int32_t proposer_house = -1;
};
std::vector<uint8_t> encode_kick_vote(const KickVote& k);  // 0x1C7 bytes
bool decode_kick_vote(const std::vector<uint8_t>& body, KickVote& out);

}  // namespace ra2::net
