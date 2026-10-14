# Network protocol (implemented): original gamemd wire format

Status of `engine/net/` (agent T). This page describes what the clone puts on the wire and the
gamemd.exe code each part was ported from. It builds on `docs/gap/net/01-original-protocol.md` (N1),
`02-cncnet.md` (N2) and `05-transport-infra.md` (N5).

* **Binary:** `D:\Red Alert 2\gamemd.exe` (YR 1.001, the build used by every RE note in this repo).
  All addresses below are in this binary. YRpp and the CnCNet spawner use a different 1.001 build, so
  their addresses are shifted.
* **Method:** Ghidra 11.2.1 headless, run on a private copy of `p4proj`. Decompiles came from N1's
  `D:\ra2-re\net1-out\` plus fresh dumps (vtable `0x7E41B4`, `0x48C230`, `0x48C4B0`, `0x4E7FB0`, the
  CRC disassembly, and the encoder call sites `0x64B2C0..0x64B820`). `yrpp-spawner` (GPL) was read
  only, for Protocol 0 behaviour; none of its code was copied.
* **Design decision:** our wire format **is** the original's. That covers the datagram checksum, the
  0xAA04 private channel and its reliability rules, the 0x1236 global channel, EventClass and its
  compressed encoding, FRAMEINFO and FRAMESYNC. A future bit-exact simulation can then talk to real
  gamemd peers without new network code. ENet is not used.

## 1. Layering and module map

```
 [tunnel hdr: none | V2 u16BE from,to | V3 u32LE from,to]   wire.{hpp,cpp}        UdpDatagramIO
 [u32 LE rotl1-add checksum]                                 wire.{hpp,cpp}        NetManager
 [0xAA04 14-byte private hdr | 0x1236 16-byte global hdr]    channel.{hpp,cpp}     PrivateChannel / GlobalChannel
 [game packet: FRAMEINFO/FRAMESYNC + event records]          event_codec.{hpp,cpp} EventEncoder / EventDecoder
 [EventClass, 47 + 1 types]                                  event.{hpp,cpp}       Event, ev::*
 lockstep scheduler                                          lockstep.{hpp,cpp}    LockstepSession
 Event <-> clone command                                     event_map.{hpp,cpp}   NetCommand
 in-game global messages (chat, kick vote)                   global_msg.{hpp,cpp}
 UDP sockets (Winsock / BSD, non-blocking, IPv4)             udp_socket.{hpp,cpp}  UdpSocket, NetAddr
 test model                                                  stub_sim.hpp          StubSim, StubInput
```

All of it is SDL-free and is compiled into `ra2assets_tests`, `ra2engine`/`ra2game`, the Android
`libmain.so` (`RA2_NET_SOURCES` in `cmake/ra2_sources.cmake`), and `tools/ra2netpeer`. On Windows it
links `ws2_32`.

## 2. Datagram and tunnel (`wire.hpp`): byte-exact

| Item | Original | Implemented |
|---|---|---|
| `Add_CRC` | `FUN_0064D8C0`: `crc = rotl1(crc) + w` | `add_crc()` |
| Build CRC | `FUN_007B0DD0`: whole LE dwords, then a 1..3-byte tail read as a dword and masked with `0xFFFFFFFF >> (32 - 8*tail)` | `packet_crc()`, `build_datagram()` → `[crc][payload]` |
| Check CRC | `FUN_007B0E50`: reject when payload length >= 0x280, or on a CRC mismatch | `check_datagram()` |
| Receive cap | `recvfrom(..., 0x280)` in `FUN_007B28A0` | `kMaxDatagram = 0x280` |
| Tunnel V2 | spawner NetHack: `[u16 BE from][u16 BE to]`; drop if shorter than 5 bytes or `to != own id` | `tunnel_wrap/unwrap(TunnelMode::V2)` |
| Tunnel V3 | cncnet-server: `[u32 LE sender][u32 LE receiver]`, at least 8 bytes | `TunnelMode::V3` |
| Tunnel ping | 50 bytes with ids 0/0; the server echoes the first 12 | `make_tunnel_ping()`, `is_tunnel_ping_reply()` |

## 3. Channels (`channel.hpp`)

Constants come from the IPXManager static init at `0x4E7FB0` (`ctor 0x540620`): global max payload
`0x1C7`, private max `0x22E`, global queue `0xA0`, private queue `0x20`, ProductID `0xAA04`. RA2's
`game.exe` uses `0xAA03` (`kProductIdRA2`). At most 7 private connections (`FUN_00540AF0`). Default
timing is `Set_Timing(2, -1, 60)`.

### 3.1 Private channel (ConnectionClass, vtable `0x7E41B4`): byte-exact

| Off | Field |
|---|---|
| 0 | u16 magic = ProductID `0xAA04` |
| 2 | u8 code: 0 DATA_ACK, 1 DATA_NOACK, 2 ACK |
| 3 | u8 router byte (0) |
| 4 | u32 PacketID. DATA_ACK and NOACK have separate counters (`conn+0x40` / `+0x3C`), both starting at 0 |
| 8 | 6 bytes, never written by gamemd. **We write 0** (D1) |

* **Init** `0x48BE20`: all counters 0, `LastSeqID` (`+0x44`) = `LastReadID` (`+0x48`) = -1.
* **Send_Packet** `0x48BE50` builds the header. **Queue_Send** `0x48B320` refuses a full queue or an
  oversize packet. The PacketID counter only advances when the packet was queued.
* **Service_Send_Queue** `0x48C2F0`:
  * Pass 1 drops done entries. An ACKed DATA_ACK first gives an RTT sample `now - first_send`, fed to
    `0x48B920`: a plain average up to 256 samples, then a 256-sample running average, rounded.
  * Pass 2 (re)sends every entry with `now - last_send > RetryDelta`.
  * A NOACK entry is done after its first send.
  * An entry fails when `count > MaxRetries` or `last - first > Timeout`, which marks the connection
    bad.
  * Time is `timeGetTime() >> 4` (`FUN_006C82C0`). One tiny difference: a fresh entry always goes out
    on the next service. In the original its `last_send` of 0 is equivalent, because that clock is
    never near 0.
* **Receive_Packet** `0x48BF50`:
  * ACK: marks the matching DATA_ACK done.
  * NOACK: queued only while at least 2 slots are free.
  * DATA_ACK:
    * Already queued, or `id <= LastSeqID` with `LastSeqID != -1`: ACK only.
    * Fewer than 2 free slots and `id != LastSeqID + 1`: dropped **without** an ACK, so the sender
      retries.
    * Otherwise queued, and `LastSeqID` advances over the contiguous queued ids.
  * Every accepted DATA_ACK is ACKed with a 14-byte header: code 2, the same id, bytes 8..13 = 0 (D1).
* **Get_Packet** `0x48C230` (newly recovered): DATA_ACK packets are delivered **strictly in PacketID
  order** (`id == LastReadID + 1`). NOACK packets are delivered as they arrive. A delivered packet is
  marked read and stays queued.
* **Service_Receive_Queue** `0x48C4B0` (newly recovered): a read packet is removed when it is NOACK or
  `id < LastSeqID`.
* The two receive-side rules above are what make the channel exactly-once and in-order. The
  megamission cache depends on this: a reordered packet would replay a stale unit list.

### 3.2 Global channel (IPXGlobalConnClass, magic `0x1236`): byte-exact

Header `0x53F900`: `u16 0x1236, u8 code, u8 0, u32 PacketID, u32 0, u16 0, u16 ProductID`.

* `code` is 0 only when an ACK is requested **and** the packet is addressed. A broadcast is always
  NOACK.
* PacketID = packets queued so far.
* Receive `0x53F9E0`:
  * DATA_ACK is deduped against a ring of (address, PacketID) entries, sized 2 × queue = 320. It is
    ACKed (16 bytes, ProductID at +14, bytes 8..13 = 0) unless queuing failed.
  * NOACK is queued.
  * ACK retires the matching entry.
* The ProductID is not checked by the channel. Consumers read `ReceivedPacket::product`. In game,
  gamemd's `Call_Back` drops anything that is not `0xAA04`.

### 3.3 NetManager (IPXManagerClass `0x541550`)

* Demultiplexes on the first u16:
  * `0x1236` goes to the global channel.
  * ProductID goes to the private channel whose peer address equals the sender.
  * Anything else is counted as unknown.
* Datagrams with a bad CRC are counted and dropped.
* `send_private(..., -1)` sends to every connection.
* `response_time()` = the worst average RTT over the private channels (`0x542180`).
* `set_rebind_on_framesync(true)` enables the NAT fix-up from `0x541550`: a NOACK FRAMESYNC from an
  unknown endpoint moves that house's connection to the new address.
* **Transports:**
  * `UdpDatagramIO`: direct, or tunnel V2/V3. In tunnel mode peers are `PeerAddr{server, tunnel_id}`.
  * `LoopbackHub`: in-process, with seeded loss, duplication and latency, plus a partition switch.

## 4. EventClass (`event.hpp`): byte-exact

* 111 bytes: `+0 Type, +1 flags (bit 0 IsExecuted), +2 i8 HouseIndex, +3 u32 Frame, +7 104-byte
  union`.
* `TargetClass` = `u32 id` (24 bits significant) + `u8 RTTI`.
* Payload sizes are a verbatim copy of `0x81F8FC` (`kEventPayloadSize[48]`). Names are from
  `0x81F92C`.
* Typed builders and accessors in `ev::` give the union offsets, relative to `data` = event + 7:

| Type | Size | Layout |
|---|---|---|
| POWERON/OFF, IDLE, SCATTER, DEPLOY, DETONATE, PRIMARY, REPAIR, SELL, PLANNODEDELETE | 5 | Target @0 |
| ALLY, GAMESPEED, SPECIAL, REMOVEPLAYER, LATENCYFUDGE, FALLBACKHOST, ALLCHEER | 4 | i32 @0 |
| MEGAMISSION 0x04 | 23 | Whom @0, Mission @5, cache slot @6, Target @7, Dest @12, Follow @17, IsPlanning @22 |
| MEGAMISSION_F 0x05 | 24 | Whom @0, Mission @5, Target @6, Dest @11, i32 Speed @16, i32 MaxSpeed @20 |
| PLACE 0x0B | 16 | i32 RTTI, i32 HeapID, i32 IsNaval, CellStruct @12 |
| PRODUCE / SUSPEND / ABANDON / ABANDON_ALL | 12 | i32 RTTI, HeapID, IsNaval |
| SPECIAL_PLACE 0x12 | 8 | i32 SW index, CellStruct @4 |
| ANIMATION 0x14 | 16 | i32 AnimID, i32 HouseID, i32 x, i32 y |
| SELLCELL 0x17 | 4 | CellStruct |
| RESPONSE_TIME 0x1B | 1 | **u8 @6** (event `+0x0D`) |
| FRAMEINFO 0x1C | 7 | u32 CRC, u16 CommandCount, u8 Delay |
| ARCHIVE / PLANCONNECT | 10 | Target @0, Target @5 |
| ADDPLAYER 0x1F | 4 | u32 length @4 (heap pointer @0); the blob follows on the wire |
| TIMING 0x20 | 5 | u16 DesiredFPS, u16 MaxAhead, u8 FrameSendRate |
| PROCESS_TIME 0x21 | 2 | u16 avg ms per frame |
| ADDRESSCHANGE 0x29 | 5 | i8 player, u32 addr |
| MEGAFRAMEINFO / PACKETTIMING | 104 / 64 | raw (WOL router only) |
| ResponseTime2 0x30 (spawner) | 2 | i8 MaxAhead, u8 LatencyLevel |

Execution semantics live in `EventClass::Execute` `FUN_004C6BD0`. The simulation owns them. The
scheduler executes only the network cases (§6).

## 5. Game packet codec (`event_codec.hpp`): byte-exact

Header (14 bytes, `Send_Packets` `0x649710` / `Send_FrameSync` `0x649A70`):

| Off | Field |
|---|---|
| 0 | type: 0x1C FRAMEINFO, or 0x19 FRAMESYNC |
| 1 | 0 (D1) |
| 2 | house |
| 3 | u32 frame |
| 7 | u32 CRC (FRAMESYNC: ScenarioMagic) |
| 11 | u16 CommandCount |
| 13 | u8 Delay = MaxAhead |

* Frame stamp: protocol 2 uses `((FSR + now - 1 + MaxAhead) / FSR) * FSR`, otherwise
  `now + MaxAhead` (`stamp_frame`).
* CommandCount is the cumulative count sent **before** this `Send_Packets` call (caller `0x647A44`
  passes `0xAF936A`, then adds the sent count). The receiver's lockstep condition is therefore "I have
  every command you sent before your last packet".

**Records:**

* General form: `[type][size bytes]`.
* MEGAMISSION: `[04][count][23 B]{[Whom 5 B] x (count-1)}`.
* RESPONSE_TIME: `[1B][data+6]`.
* ADDPLAYER: `[1F][u32 len][blob]`.

**Encoder** `EventEncoder::add_compressed` = `0x64B0D0` plus `0x64AF40`, ported decision for decision:

* A run is consecutive MEGAMISSIONs with equal Mission, Target and Destination. Whom, slot, Follow and
  IsPlanning are **not** compared, so Follow/IsPlanning of the 2nd+ members are not transmitted
  (tested).
* Cache: 8 sender slots holding `(count, first Whom + list)`, with an LRU on the frame of saving
  (`0xAF9394`). The first slot with frame 0 is used, else the oldest.
* A run is finalised when it ends, at the last event, or at the max count.
  * On a cache hit the list is rewound, `count = 0` and `slot` is written to payload byte 6.
  * On a miss the run is saved and its slot written.
* The "early repeat" search after each appended Whom forces the next MEGAMISSION to start a new run.
  This was verified from the call-site registers: `ECX = &count` (the local run counter), `EDX =
  &offset`.
* The out-of-space path reproduces the original. It finalises with the incremented count, so the list
  compare reads one Whom past `offset`, then writes `count - 1`. The send buffer carries 8 tail bytes,
  the first 4 holding u32 `0x22E` (the Session field after the buffer at `0xA8C9A8`) (D6).

**Decoder** `EventDecoder::extract` = `0x64B840`:

* The receiver cache is `MegaMissionTargets[house*8 + slot]`. `house` is the packet header byte 2.
* `count >= 2` stores the list, `count == 0` replays it, `count == 1` is a single event.
* A FRAMESYNC at the start skips 14 bytes. A mid-packet FRAMESYNC skips by the *previous*
  `size[next]`, a quirk kept as is.
* The FRAMEINFO itself is returned first. The caller subtracts it from the received count (`0x649B30`).

CommProtocol 0 (`encode_raw_events` / `decode_raw_events`): a 111-byte FRAMEINFO, then 111-byte
events. The frame is `now + MaxAhead`, never FSR-rounded in this branch.

## 6. Lockstep scheduler (`lockstep.hpp`)

`LockstepSession` is a non-blocking port. Per frame the caller runs `logic_step` → `begin_frame(crc)`
→ `poll()` until it is not Stall → apply `executed()` → `end_frame()`.

| Original | Implemented |
|---|---|
| `Queue_AI_Multiplayer 0x647090`: CRC history `[frame & 255]`; frame 0 / load: reset state, the CRC grace CDTimer `{start, start+256}`, the megamission caches, send FRAMESYNC, init wait; every 128 frames: `Set_Timing(RT+10, -1, max(8RT+15, 120))` (RT scaled by LatencyFudge) and queue `PROCESS_TIME`; master TIMING; **CommProtocol 2: on non-FSR-boundary frames only service, no send/wait/execute**; frame 0 never waits or executes | `begin_frame`, `poll` |
| `Send_Packets 0x649710`: cap = `min(OutList, 0x2000 - DoList)`; ACK flag; several packets per frame; own events echoed to DoList at the same frame | `send_packets` |
| `Wait_For_Players 0x648180`: timers resend / dialog / total; ≤ 64 packets per pass; init: every connection heard ("Player ready"); normal: `Frame < min(theirFrame) + MaxAhead` and `received >= announced` for all peers; return codes 2/3/7 | `poll` (FrameStatus) |
| Timeouts table `0x836338` (LAN 60/900/420/2400, Internet 120/1800/720/7200 ticks); per-frame FRAMESYNC resend 180 (`0x647AF1`); dialog = `max(T.dialog_frame, 3*RT)` | `wait_timeouts()` |
| `Process_Receive_Packet 0x649B30`: theirFrame = max(Frame - Delay); announced = max; FRAMESYNC magic compare (RTT sample) else 2; received += decoded - FRAMEINFO | `process_packet` |
| `Execute_DoList 0x64BDF0`: move events in `(0xA8A160, 0xA8A144)` to the new horizon after TIMING growth; per house in array order, DoList order; late non-FRAMEINFO events dropped ("Packet received too late!"); EXIT; OPTIONS/PAGEUSER local only; FRAMEINFO CRC compare `LatestFramesCRC[(Frame-Delay)&255]` when the grace timer expired and `Frame == now`; megamissions buffered per house, runs grouped (`0x64C810`) and executed last; pop executed/stale entries from the head | `execute_dolist`, `ExecItem.group` |
| TIMING generation `0x64761C..0x6479C4` (fps from the players' `PROCESS_TIME`, game-speed cap 60/45/60÷speed, Internet FSR 5/10, LatencyFudge, growth capped at +FSR, rounded to FSR, min 3·FSR, max `((FSR+249)/FSR)·FSR`, +10 under fog; precalc branch) | `master_timing` |
| `Execute` TIMING (case 0x20: −10 under fog, horizon move), RESPONSE_TIME, PROCESS_TIME, LATENCYFUDGE | `execute_net_event` |
| `0x64AE80` total timeout → `Kick_Player(min theirFrame)` → `Destroy_Connection 0x5DA170(id, 1)` → queue `REMOVEPLAYER(id)` when frame > 0 | `kick_slowest`, `destroy_connection`, `on_player_removed` |
| Exit wait `0x647DE0`: send, FRAMESYNC every 15 ticks, give up after 600, done when `Frame <= FSR + min(theirFrame)`, 4 final FRAMESYNCs | `request_exit` / `poll_exit` |
| Spawner Protocol 0: ResponseTime2 every 30 frames (first after FSR+240); worst level over players heard in the last 120 frames; ratchet; `PreCalcMaxAhead = {1,4,6,12,16,20,24,28,32,36}[lvl]`, FSR = level, fps 60 | `protocol_zero_send/handle` |

The frame CRC is supplied by the caller (the tests use `StubSim::crc()`).

**W9 (wave 9): the match CRC is `net_frame_crc` (`engine/net_frame_crc.cpp`), computed every
frame.** `ComputeFrameCRC` `FUN_0064D520` (dump `D:/ra2-re/net1-out/dec2.c`) folds
`CRC = rotl1(CRC) + term` over every infantry and building (`X/10 + (Y/10)<<16 + facing8`, facing8 =
`((DirStruct>>7)+1)>>1 & 0xFF` from `FUN_004C92F0`), every unit (both facings), the five map
layers and the Logic list (`X/10`, `Y/10`, `WhatAmI`), then ONE `Scenario->Random` draw
(`FUN_0065C1C0`, not two). The clone keeps the rotl1 + term fold and the object order and folds more
(exact positions, health, targets, every house's economy and production slots, projectiles,
crates, and the state of every sim RNG stream -- peeked, not drawn). Cost: microseconds per frame
instead of the 10-107 ms of the old L0 placeholder (fnv1a over `save_world`, 8 MB on xmp27t8),
which stays available as a debug mode (`RA2.INI [Net] FullHash=1` / `RA2_NET_FULL_HASH=1`, every
15 frames; now `save::world_sync_hash`, see "Sync hash input" below).

## 7. Deviations and approximations

**Byte-level deviations.** These only show on malformed input or in unwritten bytes:

* **D1.** Bytes gamemd never writes are zero: private header 8..13, ACK 8..13, global ACK 8..13,
  FRAMEINFO byte 1, CommProtocol 0 FRAMEINFO bytes 14..110. Receivers ignore them.
* **D2.** A private packet with code > 2 is rejected. gamemd logs it and handles it as DATA_ACK.
* **D3.** The decoder stops on malformed input, where gamemd reads out of bounds. Malformed means: an
  unknown type, a truncated record or blob, a replay of an empty or out-of-range cache slot, or 0x30
  without the spawner extension.
* **D4.** CommProtocol 0 ADDPLAYER: the blob is taken from after its event. gamemd copies it from
  packet offset 111.
* **D5.** `Send_Packets` stops when an event can never fit. gamemd would loop forever.
* **D6.** The stale bytes read by the encoder's out-of-space cache compare come from our buffer, not
  gamemd's Session memory. This only matters when that read happens to match a cached list.

**Behavioural approximations.** None of these change the wire format:

* **A1.** Protocol 0 hooks were mapped from yrpp-spawner's YRpp-build addresses by behaviour:
  * `0x4C8011` is assumed to skip the fog −10 in Execute(TIMING).
  * `0x647DF2` is not mapped.
* **A2.** `NodeNameType.Time` starts at −1, so no real TIMING is sent until every player has sent
  PROCESS_TIME. Its initialiser was not located.
* **A3.** ScenarioMagic at frame 0 = that frame's CRC. gamemd sets `0xA8D710` outside
  `Queue_AI_Multiplayer` (only the load-game assignment is in `0x647090`).
* **A4.** PROCESS_TIME uses the wall time between `begin_frame` calls (or
  `set_process_time_sample`). It does not come from the main loop's ProcessTicks/Frames accounting.
* **A5.** Stall statistics are counted once per stalled frame, not once per wait-loop pass.
* **A6.** The kick on total timeout is `Destroy_Connection(id, 1)`. The flag is inferred from the
  `0x64AE80` call (`PUSH 1`).
* **A7.** Not implemented:
  * WOL packet-router paths (`0xCDEF` channel, MEGAFRAMEINFO).
  * The serial/modem modes.
  * The reconnect dialog UI and kick-vote counting. `reconnect_dialog()` and the global kick-vote
    codec are provided.
  * The scenario-load progress handshake (global 0x0F, `FUN_00683DB0`).
  * The game-name hash in chat (`FUN_005DA620`).

**Clone hardening (W9-05-12; gamemd trusts its peers).** Honest peers never trigger these, so
the wire format and every in-sync behaviour are unchanged:

* **H1.** `process_packet` drops a whole packet whose header house is not the sending connection's
  house, or whose sender frame (Frame − Delay) is more than `kFrameWindow` (1024) frames ahead of
  ours. It drops single events whose house differs from the connection's or whose frame is beyond
  the window. Dropped events do not count toward `received` (the command gate).
* **H2.** TIMING / RESPONSE_TIME / LATENCYFUDGE execute only from the master's house
  (`LockstepConfig::master_house`, the lobby host in slot 0). TIMING is clamped to MaxAhead 1..255
  (the u8 header Delay), FSR 1..30 and fps 1..60. LATENCYFUDGE is clamped to 0..3.
* **H3.** The megamission decoder refuses a cache slot above 7 (or a header house outside 0..7):
  `house*8 + slot` would otherwise read or overwrite another house's cache on one receiver only.
* **H4.** `NetGame` honours REMOVEPLAYER(target) only from the master, from the target itself, or
  against the master (the host vanished), and only for a human house of the match. A machine that
  executes its own REMOVEPLAYER ends with "Connection lost" instead of freezing.
* **H5.** Commands are bounds-checked against the World before they are queued
  (`valid_net_command`: issuing house, unit / building indices, cells on the map).

**Sync hash input (W9-05).** The full-World debug hash follows one policy for every hash peers
compare (`savegame.hpp`, "the SYNC-HASH POLICY"): derived caches that a const accessor rebuilds
lazily (`amphib_passable_`, `crush_grid_`/`crush_leak_`, `deck_land_`/`deck_amph_`/`deck_ver_`,
`w5_hide_`/`w5_own_`) are saved but not hashed; the unsaved per-house production rules are hashed as
a digest (`econ_rules_digest_`); NaNs are written as one canonical quiet NaN; and the campaign
trigger runtime is hashed when there is one. The debug full hash streams it through `SyncHasher`
(`save::world_sync_hash`, no 8 MB buffer). The per-frame `net_frame_crc` obeys the same policy
(w9int): it folds doubles through the same `om::canonical_bits` (NaN canonical, -0 kept), never
reads a derived cache (a source-scan test in `tests/test_saveload.cpp` checks every `io_unhashed`
member), folds `econ_rules_set_` + `econ_rules_digest_` + `human_mask_`, and, in a campaign / co-op
match, the trigger runtime (RNG, frame, globals / locals, mission timer, outcome, house flags,
every tag / trigger instance; not the text log).

**Correction to N1 §7.4.** On non-boundary frames under CommProtocol 2, `Queue_AI_Multiplayer`
returns right after servicing the network (code at `0x6479C9`). It does not execute the DoList. Events are
stamped on boundaries, so nothing is lost.

## 8. API for agent S (lobby / relay / LAN discovery)

* **Global channel:**
  * `NetManager::send_global(payload, len, ack, &peer | nullptr)`.
  * `NetManager::get_global(ReceivedPacket&)` gives `from`, `product` and `payload`.
  * `set_broadcast(PeerAddr)`.
  * Payloads are 0x1C7 bytes or less. Message helpers are in `global_msg.hpp` (chat 0x0C, kick vote
    0x1C).
* **Tunnel / relay:**
  * `UdpDatagramIO::set_tunnel(TunnelMode::V2|V3, server, local_id)`; peers are
    `PeerAddr{server, peer_id}`.
  * Our relay can speak the V2/V3 header unchanged. A payload-agnostic port-allocation relay also
    works with `TunnelMode::None`.
  * `make_tunnel_ping` / `is_tunnel_ping_reply` are provided.
* **Raw sockets:** `UdpSocket` (non-blocking, `set_broadcast`) for the LAN beacon. Android needs the
  manifest permissions added here, plus a Java `MulticastLock` while browsing.
* **Session start:**
  1. `NetManager::create_connection(house, peer)` for every remote human.
  2. Fill `LockstepConfig`: houses, local house, master, MaxAhead/FSR (LAN start values: FSR 5,
     MaxAhead `max(10, roundup(RT/8, FSR))`, Set_Timing `(max(RT+2, 30), -1, 600)`, from
     `FUN_005DBD30`; spawner: FSR from ini, MaxAhead = 6·FSR, `(60, -1, 600)`).
  3. Drive `LockstepSession` from the tick loop (agent L0).
* **Commands:** `event_map.hpp`. The agreed entry points for L1's `engine/command.hpp` are
  `bool ra2::net::to_event(const ra2::Command&, Event&)` and
  `bool ra2::net::from_event(const Event&, ra2::Command&)`, layered on `command_to_event` /
  `event_to_command`. Until then the mapping targets the minimal `NetCommand`.

## 9. Tests

* `ra2assets_tests --test-case="net*"` runs 32 cases in about 1.2 s:
  * `test_net_wire.cpp`: CRC vectors, datagram rules, V2/V3 golden headers, private/global golden
    headers, retransmit/timeout, the dedupe/in-order/retirement rules, a lossy loopback manager
    (exactly once, in order), and real 127.0.0.1 sockets direct and through an in-test V2 relay.
  * `test_net_event.cpp`: the size and name tables, the 111-byte layout, a golden FRAMEINFO+PRODUCE
    packet, golden MEGAMISSION run and cache-hit packets, early repeat, the Follow quirk,
    RESPONSE_TIME/ADDPLAYER/0x30/FRAMESYNC, a round trip of every type (compressed and CommProtocol
    0), buffer splitting, and the command map.
  * `test_net_lockstep.cpp`:
    * Two peers for 700 frames, identical per-frame CRCs, both adopting the master's TIMING.
    * Three peers with 15 % loss, duplication and jitter.
    * CRC divergence detected after the grace window.
    * A frame-0 mismatch gives "scenarios do not match".
    * A dropped peer is kicked and REMOVEPLAYER runs on both survivors at the same frame.
    * The initial-sync timeout, and EXIT.
    * CommProtocol 0, and spawner Protocol 0 latency-level convergence.
* `tools/ra2netpeer` with `scripts/net_two_process.ps1` runs two OS processes over localhost UDP and
  compares their hash trails (900 frames). `ra2netpeer --selftest` runs both peers in one process over
  real sockets. The same binary, cross-built with the NDK (x86_64), passes `--selftest` on the
  `ra2x86` emulator (adb shell, `/data/local/tmp`).
