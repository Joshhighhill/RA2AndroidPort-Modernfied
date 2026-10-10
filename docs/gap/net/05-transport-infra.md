# N5 - Transport and infrastructure layer for online play

Status: research / design only. No engine code was changed.
Branch: `net5` (worktree `D:\ra2-wt\net5`), based on master `43f4128`.
Companion reports (written in parallel): N1 original-protocol RE, N2 CnCNet,
N3 sim bit-exactness, N4 engine lockstep readiness. This document owns
everything *below* the lockstep turn logic: sockets, reliability, discovery,
NAT traversal, the user-hosted server, the lobby/connection UX, security and
Android lifecycle. Where it depends on N3/N4 (determinism, order encoding,
snapshot restore) it says so explicitly.

---

## 0. TL;DR - the recommendation

| Decision | Choice | One-line reason |
|---|---|---|
| Transport library | **ENet 1.3.18**, vendored as source (MIT) | ~5 kLOC of C, builds unchanged with MinGW and the NDK, gives reliable-ordered + unreliable channels, fragmentation, RTT, timeouts. No dependencies. |
| Game topology | **Star**: one *session host* (a player's game) aggregates turns; every other player has exactly one link, to the host | One link per client = one NAT problem per client, not 28 for 8 players; the same code path for LAN and internet. |
| Lobby logic | **Runs in the host's game (C++)**, carried over the same ENet link, for LAN *and* online | One lobby implementation. The server never has to understand slots, colours or maps. |
| Server | **One small Go binary** (`ra2lobbyd`): room directory + room codes + rendezvous + **UDP relay**; no game logic | Static binary, ~10 MB, `FROM scratch` Docker image, cross-compiles to linux/arm64 (Oracle free tier) and amd64. |
| Relay mechanism | **TURN-style port allocation**: each relayed client gets its own relay UDP port; the relay forwards raw datagrams unchanged | Transport-agnostic: ENet runs end to end through it with no header and no ENet patch. The host simply sees N peers at `relay:port_i`. |
| NAT traversal | v1 = **always relay** online (works behind CGNAT, symmetric NAT, IPv6-only + CLAT). v2 = **try UDP hole punch first, fall back to relay** | Relay always works; hole punching saves relay bandwidth for ~70-80 % of pairs. STUN/TURN/ICE libraries are not needed. |
| LAN discovery | UDP broadcast on port 50010 to every interface's directed-broadcast address + 255.255.255.255; Android holds a `MulticastLock` while the browser is open | Works on home Wi-Fi, Ethernet, and Android hotspots (host or client side); plus a "join by IP" fallback. |
| Identity | **Nickname + random per-install player id; room codes; no accounts** in v1 | Zero personal data, nothing to leak; accounts can be layered later. |
| Hosting | **Hetzner CX22 (~EUR 4/month, 20 TB)** recommended; **Oracle Cloud Always Free** as the zero-cost option | Plain VPS with a public IPv4 and open UDP ranges. PaaS platforms (Fly, Render, Railway, Cloudflare) are poor fits for a UDP relay. |
| Reconnect | Session token + host-side turn log replay; snapshot resync (save-game archive) when the gap is too large | Android kills background sockets; reconnecting must be a first-class path, not an error. |

Estimated effort to a playable 2-8 player internet skirmish: **~6-8 engineer-weeks**
for the transport/infra parts in this document (plus whatever N3/N4 find on
determinism). Phased plan in section 9.

---

## 1. Component diagram

```
                                 user-hosted server (VPS, single Go binary "ra2lobbyd")
                       +------------------------------------------------------------------+
                       |  TCP 50000  control  : hello/version gate, room directory,        |
                       |                        room codes, rendezvous, relay allocation   |
                       |  UDP 50000  bind/probe: learns each client's public UDP mapping   |
                       |  UDP 50100-50999 relay: one port per relayed client, raw forward  |
                       |  (optional) TCP 8080 /healthz + /metrics, bound to localhost      |
                       +------------------^---------------------------^-------------------+
                          control (TCP)   |  game datagrams (UDP)      |
              +---------------------------+------+       +------------+--------------------+
              |                                  |       |                                 |
  +-----------+---------------------+     +------+-------+-----------+   (LAN: no server)
  | HOST player's game (C++)         |     | CLIENT player's game (C++)|
  |                                  |     |                           |
  |  SessionHost  (lobby SM, turn    |<===>|  SessionClient            |
  |    aggregator, turn log, desync  | ENet|   (lobby view, order      |
  |    arbiter, reconnect/resync)    | link|    submit, turn apply)    |
  |        |                         |     |        |                  |
  |  NetTransport (ENet host,        |     |  NetTransport (ENet host, |
  |    channels, framing, codec)     |     |    channels, codec)       |
  |        |                         |     |        |                  |
  |  NetPath: direct | punched |     |     |  NetPath                  |
  |           relay-allocated        |     |                           |
  |  LanBeacon (UDP 50010 bcast)     |     |  LanBrowser (+ Android    |
  |  ServerLink (TCP control)        |     |   MulticastLock via JNI)  |
  +----------------------------------+     +---------------------------+
          ^                                         ^
          | World::apply_orders(turn) / sync hash    | (N4 owns this boundary)
          v                                         v
     deterministic sim (dt = 1/15 s)          deterministic sim
```

Layering inside the game (new code under `engine/net/`, SDL-free except for
the thread/timer glue, so it can be unit-tested in `ra2assets_tests`):

```
 UI pages (multiplayer menu, browser, lobby, in-game chat/overlays)
   -> SessionHost / SessionClient      (game-level protocol, section 5)
   -> NetTransport                     (ENet wrapper, channels, codec, version gate)
   -> NetPath                          (address selection: LAN / punched / relay)
   -> LanBeacon / LanBrowser, ServerLink (TCP)   (discovery + control plane)
   -> platform sockets (ENet's unix.c / win32.c; ws2_32 on MinGW)
```

---

## 2. Transport (a)

### 2.1 Candidates

| Option | Licence | Android NDK | MinGW | Size / deps | Reliability features | NAT | Verdict |
|---|---|---|---|---|---|---|---|
| **Plain BSD sockets + own reliability** | ours | yes | yes (ws2_32) | 0 deps; ~1.5-2.5 kLOC to write *and debug* | whatever we write | n/a | Viable, but re-implements ENet's 20 years of fixes (fragment queues, sequence wrap, throttling). Not worth it. |
| **ENet 1.3.18** | MIT | yes (POSIX `unix.c`) | yes (`win32.c`, ws2_32/winmm) | ~5 kLOC C, no deps | connections, up to 255 channels, reliable-ordered, unreliable-sequenced, unreliable-unsequenced, fragmentation, RTT/ping, peer timeout, bandwidth throttle, MTU setting | none built in; plain UDP so hole punching/relay is just "which address to connect to" | **Chosen.** |
| SDL_net 2.x | zlib | yes | yes | tiny | none (thin TCP/UDP wrappers) | none | Gives nothing over raw sockets except portability we already get from ENet. |
| Valve GameNetworkingSockets | BSD-3 | possible but heavy | painful | needs protobuf + OpenSSL/libsodium; >100 kLOC | excellent (reliable msgs, lanes, encryption, ICE via WebRTC) | ICE built in | Too heavy for the NDK + MinGW static build; protobuf toolchain in the Android build is a maintenance tax. |
| yojimbo / netcode | BSD-3 | yes | yes | needs libsodium | encrypted, connect tokens, reliable-ordered | none | Good design (connect tokens), but client-server oriented and adds libsodium; its token model is overkill without accounts. |
| WebRTC data channels (libdatachannel + libjuice) | MPL-2.0 | yes | yes (with work) | needs OpenSSL/mbedTLS + usrsctp; ~MBs | SCTP reliable/unreliable | ICE/STUN/TURN built in | Best NAT story, but DTLS+SCTP+ICE in both native builds is a big dependency for a game that needs ~2 KB/s. Keep as a "maybe later" for a browser client. |
| QUIC (msquic / quiche / ngtcp2) | MIT / BSD-2 | yes | msquic on MinGW is not a supported path | TLS stack required | streams + datagrams, encrypted, migration | connection migration helps mobile | Mandatory TLS + certs for P2P links between players; complex. Overkill. |

Sources: ENet repository and ChangeLog (1.3.18, 2024-04-14: "MTU negotiation
fixes ... ENET_SOCKOPT_TTL"), MIT licence [1][2]; SDL_net [3]; GameNetworkingSockets
[4]; yojimbo/netcode [5]; libdatachannel/libjuice [6]; msquic [7].

### 2.2 Why ENet fits this design specifically

- **The relay design (section 4.3) makes ENet's lack of NAT features irrelevant.**
  An ENet peer is "an IPv4 address + port"; whether that address is the host on
  the LAN, the host's punched public mapping, or `relay:50123`, ENet does not
  care. No fork, no patch.
- **ENet's `ENET_SOCKOPT_TTL` (1.3.18) and access to `host->socket`** let us
  send the raw bind/probe datagrams for hole punching from the *same* socket
  ENet uses, so the NAT mapping we punch is the one ENet then uses.
- **IPv4 only (upstream).** Acceptable: every server address we publish has an
  A record, LAN is IPv4, and Android IPv6-only carriers run 464XLAT/CLAT so IPv4
  sockets to an IPv4 literal still work [8]. If native IPv6 P2P is ever wanted,
  ENet IPv6 forks exist (e.g. `zpl-c/enet`, MIT) and are API-compatible, but v1
  does not need it (section 4.5).
- Builds: add `third_party/enet/` (callbacks.c, compress.c, host.c, list.c,
  packet.c, peer.c, protocol.c, unix.c, win32.c, headers) to `CMakeLists.txt`
  and the Android jni build; link `ws2_32 winmm` on MinGW. Adds ~60-80 KB to
  each binary.

### 2.3 Channels

One ENet connection per client<->host link, 4 channels:

| Ch | Mode | Contents |
|---|---|---|
| 0 | reliable, ordered | session control: HELLO/WELCOME, lobby state, slot changes, START, PAUSE, DROP, desync verdicts, reconnect |
| 1 | reliable, ordered | **lockstep turns**: client ORDERS(turn) up, host TURN(turn, all orders) down; SYNC hashes |
| 2 | reliable, ordered | bulk: chat, map transfer chunks, snapshot (resync) chunks - separate so a 300 KB snapshot never head-of-line blocks turns |
| 3 | unreliable, unsequenced | PING/PONG with timestamps (latency HUD, adaptive turn length), "player typing", cursor/beacon pings |

ENet's own keep-alive and RTT already exist; channel 3 carries *application*
timing so the host can compute per-player turn latency including frame time.

### 2.4 Framing and codec

- Every ENet packet = one **message**: `u8 type, u8 flags, varint length?`
  (length is implicit = ENet packet size) then body. Little-endian fixed-width
  integers + LEB128 varints for counts. Hand-written encode/decode
  (`engine/net/codec.hpp`), with golden test vectors checked into `tests/`
  so the Go server's (much smaller) codec is tested against the same bytes.
- Several orders for one turn travel in one TURN message (one ENet packet).
- **MTU**: set ENet host MTU to **1200** (default 1400) - safe across Wi-Fi,
  mobile carriers, PPPoE, VPNs, 464XLAT/NAT64 and our relay without IP
  fragmentation; ENet fragments larger messages itself. Turn messages are
  normally 20-200 bytes, so this only matters for bulk.
- **Compression**: none for turns (they are tiny, and ENet's range coder adds
  CPU for nothing). **Deflate** for bulk (custom maps, snapshots, desync
  dumps): vendor `miniz` (MIT, single file) - the repo has no inflater today
  (only `stb_image_write`'s deflater).
- **Integrity**: ENet's optional CRC32 checksum (`enet_crc32`) enabled - cheap
  and catches corrupt relays/middleboxes.

### 2.5 Versioning / handshake (the "compatibility gate")

The first message on channel 0 is `HELLO`, answered by `WELCOME` or `REJECT`:

```
HELLO {
  u32 magic        = 'RA2N'
  u16 net_proto    // bump on any wire change (this document = 1)
  str engine_ver   // VERSION file, e.g. "0.9.0", + git short hash
  u32 sim_ver      // bump on any sim change that affects determinism (N3/N4)
  u32 rules_crc    // CRC-32 of the *effective* rulesmd.ini + artmd.ini + ai*.ini
                   //   bytes as loaded (after YR overlay), see decision-yr-data-set
  u32 data_crc     // CRC-32 over the MIX index tables the sim reads (expandmd*, ra2md, ...)
  u8  platform     // 1 win64, 2 android-arm64, 3 android-x86_64 ...
  u64 player_id    // random per install, persisted in RA2.INI [Net]
  str nickname     // 1-16 chars, validated (section 8)
  u8  session_token[16]  // zero on first join; non-zero = reconnect
  u32 last_turn_acked    // reconnect only
}
WELCOME { u8 slot, u8 host_slot, u32 server_tick_rate, room settings snapshot }
REJECT  { u8 reason: VERSION | SIM | RULES | DATA | FULL | BANNED | STARTED | BAD_NAME, str detail }
```

- `sim_ver` and `rules_crc` mismatches are hard rejects with a clear UI string
  ("Your game data differs from the host's: rules CRC 0x1234 vs 0x5678").
  This is the cheapest desync prevention there is: different INI = guaranteed desync.
- The **map** is checked separately when selected (section 6.4): `map_crc` of
  the exact `.map/.mpr` bytes.
- Cross-platform (Win64 x86-64 vs Android arm64) play is only allowed if N3
  confirms bit-exact simulation across ISAs (floating point!); until then the
  host can enforce `platform` equality per room ("Cross-platform: off").

---

## 3. LAN discovery (b)

### 3.1 Protocol

- **Beacon port: UDP 50010** (game traffic: UDP 50020, configurable in RA2.INI
  `[Net] LanPort=`). Avoid the original game's ports (N1) so an RA2 install on
  the same LAN is not confused.
- The **host** sends `LAN_BEACON` every 1 s while its lobby is open:
  `{magic 'RA2L', net_proto, sim_ver, rules_crc, room_name, host_nick, map_name,
  players/max, game_port, flags(started, password, cross_platform), nonce}` (<300 B).
- Clients may also send `LAN_QUERY` (broadcast) when the browser opens so hosts
  answer immediately (unicast `LAN_BEACON` reply) instead of waiting 1 s.
- Send to **every IPv4 interface's directed broadcast** (e.g. `192.168.1.255`,
  `192.168.43.255`, `172.20.10.15`) *and* `255.255.255.255`. Enumerate with
  `getifaddrs()` (Android NDK, API 24+; for minSdk 21 enumerate in Java via
  `NetworkInterface.getNetworkInterfaces()` and pass the list over JNI) and
  `GetAdaptersAddresses()` on Windows. Reason: limited broadcast
  (255.255.255.255) is often routed only out of the default-route interface,
  so on a phone that is *hosting* a hotspot (no default route over Wi-Fi, mobile
  data is default) it never reaches the hotspot clients.
- Browser entries expire after 4 s without a beacon. Join uses the beacon's
  source address + `game_port`.
- Optional (later): mDNS/DNS-SD (`_ra2._udp`) via Android `NsdManager` and Windows
  `DnsServiceRegister` for networks that drop broadcast but pass multicast. Not
  needed in v1.
- Always offer **"Join by IP"** (and show the host's LAN IP in its lobby) for
  networks that filter both (guest Wi-Fi with client isolation, corporate APs).

### 3.2 Android specifics

- **Permissions** (normal, install-time, no runtime prompt) to add to
  `android/app/src/main/AndroidManifest.xml` (it currently declares none):
  `android.permission.INTERNET`, `android.permission.ACCESS_NETWORK_STATE`,
  `android.permission.ACCESS_WIFI_STATE`, `android.permission.CHANGE_WIFI_MULTICAST_STATE`.
  The "No permissions" comment in the manifest must be updated.
- **MulticastLock**: many devices' Wi-Fi drivers filter broadcast *and*
  multicast frames when the screen/stack is in power-save; holding
  `WifiManager.MulticastLock` makes the stack deliver them [9][10]. Acquire
  (`setReferenceCounted(false)`) only while the LAN browser or a LAN lobby is
  open and release on leave/onPause - it costs battery. Implement in
  `RA2Activity.java` as two static methods called from native via JNI
  (`SDL_AndroidGetJNIEnv`).
- **Hotspot cases**:
  - Phone A hosts a hotspot and the game: works with the per-interface directed
    broadcast above (the AP interface is typically `wlan1`/`ap0`/`swlan0`, subnet
    randomised since Android 11 - never hard-code `192.168.43.x`).
  - Phone A hotspot, phone B and a PC join it and B hosts: normal Wi-Fi client case.
  - Some OEM hotspots enable client isolation: fall back to Join by IP;
    documented in the UI help.
  - Wi-Fi Direct is out of scope.
- **Multi-network phones**: bind the LAN game socket to the Wi-Fi interface's
  address, not `INADDR_ANY`, when the phone also has mobile data, so replies
  don't leave over the cellular interface. (`ConnectivityManager.bindProcessToNetwork`
  is *not* used - it would also move the internet lobby traffic.)
- **Windows**: the first `bind()` triggers a Defender Firewall prompt; tell the
  user to allow "Private networks". The installer/zip README should mention it.

---

## 4. Internet: NAT traversal and the server (c)

### 4.1 Options compared

| Option | Works when | Cost | Complexity |
|---|---|---|---|
| Port forwarding by the host | user configures router; never on CGNAT/mobile | free | user pain; unreliable - not offered except as "Direct IP" for experts |
| UPnP-IGD / NAT-PMP / PCP | home routers with UPnP on; never on CGNAT | free | miniupnpc (BSD); flaky; skip v1 |
| **Relay (our server forwards UDP)** | always (only outbound UDP needed) | server bandwidth | **low** |
| **UDP hole punching via our rendezvous** | both NATs endpoint-independent mapping; fails on symmetric/strict CGNAT | free bandwidth | medium |
| STUN + TURN (coturn) + ICE (libjuice) | always (TURN fallback) | TURN bandwidth = same as relay | high; TLS/DTLS stacks; standard but heavy |

Measurement data: classic studies found ~82 % of NATs allow UDP hole punching
(Ford et al.) [11]; a 2025 measurement of 4.4 M libp2p traversal attempts
across 85k networks puts the hole-punch success at ~70 % +/- 7 % [12][13].
Mobile carrier-grade NAT (CGNAT) is over-represented in the failures
(symmetric / port-randomising mappings), so for a mobile-first game, assume
**30-50 % of sessions with at least one phone on cellular need the relay**.

### 4.2 Recommended architecture

**The user-hosted server does: version gate, room directory, room codes,
rendezvous (public address exchange), and relay. It does not run lobbies or
games.** All lobby/game semantics live in the host player's C++ code
(section 5), identical on LAN.

Join flow online:

```
host                       server                              client
 |--TCP HELLO------------->|                                     |
 |--CREATE_ROOM(meta)----->|  (code "K7Q2MX")                    |
 |<-ROOM_CREATED(code,tok)-|                                     |
 |--UDP BIND(tok)--------->|  learns host public mapping Mh      |
 |<-UDP BIND_ACK-----------|                                     |
 |  (refresh meta every 5s: map, players/max, started)           |
 |                         |<--TCP HELLO-------------------------|
 |                         |<--LIST_ROOMS / JOIN_ROOM(code)------|
 |                         |--JOIN_OK(tok_c)-------------------->|
 |                         |<--UDP BIND(tok_c) ------------------| learns Mc
 |<-PEER_INCOMING(cid)-----|  allocates relay port Pc;           |
 |                         |--RELAY_READY(relay_ip:Pc)---------->|
 |  v1: client ENet-connects to relay_ip:Pc; relay forwards Pc<->Mh
 |  v2: server first sends PUNCH(Mh) to client and PUNCH(Mc) to host;
 |      both fire probes from their ENet socket for 1.5 s; on success
 |      client ENet-connects to Mh directly and tells server RELAY_RELEASE(Pc)
 |<========= ENet link (HELLO/WELCOME, lobby, chat, game) ========>|
```

### 4.3 The relay: TURN-style port allocation (no header)

- For each relayed client the server opens one UDP socket from the pool
  `50100-50999`. That socket has exactly two legitimate remotes: the client's
  bound mapping `Mc` and the host's bound mapping `Mh`.
  - datagram from `Mc` -> send to `Mh` from the same socket,
  - datagram from `Mh` -> send to `Mc`,
  - anything else -> dropped (and counted).
- The host therefore sees each client as a distinct ENet peer at
  `relay_ip:Pc`. **No relay header, no ENet modification; the relay is
  payload-agnostic** - exactly the property that lets a CnCNet-style tunnel or a
  different game protocol use it later.
- Mapping changes (phone switches Wi-Fi->LTE, NAT rebinding): the client
  re-sends `BIND(tok_c)` on UDP 50000 from its socket; the server updates `Mc`.
  This is how reconnects survive an IP change (section 7).
- Limits per allocation (abuse protection, section 4.8): 64 KB/s and 200 pkt/s
  sustained per direction, idle timeout 60 s (in-game keep-alives are 1 s),
  hard cap on allocations per source IP and per room.
- Capacity: 900 ports = 900 relayed clients = ~128 concurrent 8-player games
  on one box; widen the range if ever needed.
- **CnCNet compatibility hook**: CnCNet tunnels (N2) put a small sender/receiver
  id header in each datagram rather than allocating ports. If we later want to
  run *our* traffic over CnCNet tunnels, that needs a send/recv shim at the
  socket level (ENet's `enet_socket_send/receive` in `win32.c/unix.c` are the
  two functions to wrap - ~40 lines, compiled in under a flag). Conversely,
  `ra2lobbyd` can grow a "CnCNet tunnel v3" listener on a separate port
  without touching the port-allocation relay. Defer to N2 for the exact header.

### 4.4 Hole punching (phase 3)

Standard "simultaneous open" through the rendezvous: both sides learn the
other's public mapping (from `BIND`) and private LAN address (sent in
`HELLO`), then send `PROBE{nonce}` datagrams from the ENet socket every 50 ms
to both candidates for up to 1.5 s; the first `PROBE_ACK` wins. Order of
preference: same-LAN private address (players behind the same router -
hairpinning often fails) > public mapping > relay. The relay allocation is made
*in parallel*, so falling back costs no extra round trip. ENet's connect then
targets the winning address. Expected: ~70 % direct, the rest relayed. With the
star topology, only client<->host pairs need punching (7 attempts for 8 players).

### 4.5 Mobile CGNAT and IPv6

- CGNAT (RFC 6598, `100.64.0.0/10`) on cellular is the norm; a phone can
  essentially never *host* directly. With the relay it doesn't matter: hosting
  from a phone on LTE works, because the host also only makes outbound UDP.
- CGNAT UDP mapping timeouts can be as short as 30 s: keep-alives every 1 s in
  game (ENet does this) and every 10 s in the lobby to the server keep mappings
  alive.
- IPv6-only mobile networks (T-Mobile US, Reliance Jio, etc.) provide
  464XLAT/CLAT on Android, so the IPv4-only ENet + an IPv4 server address work
  [8]. Server: also publish an AAAA record and listen dual-stack on the *TCP
  control* port; v1 keeps the UDP relay IPv4-only. Native IPv6 P2P (no NAT!)
  is a v3 nicety via an ENet IPv6 fork.

### 4.6 The server, `ra2lobbyd`

**Language: Go** (1.22+). Reasons: single static binary (`CGO_ENABLED=0`),
trivial cross-compile to `linux/arm64` (Oracle Ampere) and `linux/amd64`;
goroutine-per-connection is ideal for TCP control + a few UDP sockets;
standard library has everything (net, crypto/rand, crypto/tls, expvar,
log/slog); memory-safe for an internet-facing daemon. Code size estimate:
~1.5-2 kLOC + tests.

Alternatives considered: C++ (could reuse the client codec, but an
internet-facing C++ daemon is more work to harden and to build for arm64 on
the user's Windows box); Node (fine for TCP/WebSocket, weaker for a
high-packet-rate UDP relay, needs a runtime image); Rust (excellent but the
team's toolchain is C++/MinGW). The only shared artefact between client and
server is the small control-plane codec, guarded by golden vectors.

**Ports** (all configurable):

| Port | Proto | Purpose |
|---|---|---|
| 50000 | TCP | control plane (framed binary, optionally TLS later) |
| 50000 | UDP | BIND / PROBE / mapping refresh |
| 50100-50999 | UDP | relay allocations |
| 127.0.0.1:8080 | TCP | `/healthz`, `/metrics` (expvar/Prometheus), never public |

**Resources**: idle ~10-15 MB RSS; each room ~2 KB, each relay allocation
one socket + one goroutine. Bandwidth per relayed 8-player game: each client
sends ~7-15 turn packets/s of ~60-150 B up and receives the same down; with all
7 clients relayed that is roughly 2 x 7 x 15 x 150 B ~= 32 KB/s ~= 0.25 Mbit/s,
**~115 MB per game-hour** worst case. A 20 TB/month allowance is therefore
~170,000 relayed 8-player game-hours. CPU is negligible (one `recvfrom` +
`sendto` per datagram; a 1-vCPU box forwards >50k pkt/s). **Any 1 vCPU /
512 MB box is overkill for hobby scale.**

**State**: in memory only (rooms vanish on restart; clients just re-create).
An optional JSON file for the ban list and MOTD. No database in v1.

**Configuration** (flags or env): `--listen :50000 --relay-ports 50100-50999
--public-ip <ip> --max-rooms 500 --max-rooms-per-ip 3 --motd "..." --ban-file
bans.json --min-net-proto 1 --server-name "Omar's RA2"`.

**TLS / auth**:
- v1: **no accounts, no passwords on the server**. Identity = nickname +
  random 64-bit `player_id` per install. Rooms are public (listed) or private
  (code only), optionally with a host-set room password that the *host* checks
  (it never touches the server). Nothing sensitive crosses the control plane,
  so plain TCP is acceptable for v1; chat and orders between players travel
  over the ENet link and are **not encrypted** in v1 (state this in the UI/README).
- v2 (if wanted): TLS on the control port. Easiest operationally: put the
  control plane on WebSocket over TLS behind **Caddy** (automatic Let's
  Encrypt) and link mbedTLS 3.6 LTS (Apache-2.0, builds on NDK and MinGW) in
  the client; game datagrams get per-session keys handed out over that TLS
  channel (ChaCha20-Poly1305 via monocypher, public domain/BSD, ~2 kLOC).
  Do not do this before people actually play online.
- Server admin auth: a shared secret for an admin command (`/kick`, `/ban`,
  `/motd`) issued from the game's chat when the admin's `player_id` is in the
  config's admin list - or simply SSH + edit `bans.json` + `SIGHUP`.

### 4.7 Hosting options and cost (verified 2026-10)

| Host | Plan | Price | Traffic | UDP relay OK? | Notes |
|---|---|---|---|---|---|
| **Hetzner Cloud** | CX22: 2 vCPU, 4 GB, 40 GB | ~EUR 3.99/mo (raised from 2.99 in Apr 2026) | 20 TB out, in free | yes (public IPv4, cloud firewall) | **Recommended**: cheapest predictable box; EU + US regions [14][15] |
| **Oracle Cloud Always Free** | Ampere A1 (arm64): 2 OCPU / 12 GB for new free accounts since Jun 2026 (was 4/24); or 2x AMD micro | $0 | 10 TB out/month | yes (open ports in the VCN security list *and* the instance's iptables) | Zero cost, but idle-reclamation of free instances and account-signup friction; good second option [16][17] |
| DigitalOcean / Vultr / Linode | smallest droplet | ~$4-6/mo | 0.5-1 TB | yes | fine; bandwidth caps are still ~4000+ game-hours |
| Fly.io | shared-cpu-1x 256 MB | ~$2/mo + $2/mo dedicated IPv4 (UDP needs it) | metered | UDP supported only on a dedicated IPv4 and per explicit port; port *ranges* are awkward | not recommended for the relay [18] |
| Render / Railway / Heroku / Cloudflare Workers | - | - | - | **no inbound UDP** | unsuitable (Cloudflare Spectrum UDP is enterprise) |
| Home PC / Raspberry Pi | - | electricity | ISP | only if the ISP gives a public IPv4 (no CGNAT) | works for friends; use a DDNS name |

### 4.8 Abuse protection (server)

- Version gate first: a `HELLO` with unknown magic or `net_proto < min` is
  closed after one message; no room listing without a valid HELLO.
- Per-IP limits: 5 TCP connections, 3 hosted rooms, 10 relay allocations,
  token-bucket on control messages (20 msg/s burst 50), max message 4 KB.
- Relay: allocations only exist between two authenticated BIND tokens
  (128-bit random); packets from any other address are dropped without reply
  -> the relay cannot be used as a reflector/amplifier (replies are never larger
  than requests, and only go to bound addresses). Per-allocation bandwidth cap
  and idle timeout.
- Room codes: 6 chars from Crockford base32 minus look-alikes (~1e9 space),
  join attempts rate-limited per IP (10/min) so codes cannot be brute-forced.
- Nickname/room-name filtering: length, charset (printable, no control chars,
  no bidi overrides), optional word list.
- Bans by `player_id` and by IP (`bans.json`, reloadable). Logs: one line per
  room create/join/leave with truncated IPs (/24) by default.
- OS level: `ufw`/cloud firewall allowing only 22, 50000/tcp, 50000/udp,
  50100-50999/udp; unattended-upgrades; the container runs as non-root with a
  read-only rootfs.

### 4.9 Deployment, step by step (what the user does)

The repo gets a `server/` directory: `go.mod`, `cmd/ra2lobbyd/main.go`,
`Dockerfile`, `docker-compose.yml`, `deploy/ra2lobbyd.service`, `README.md`.

```dockerfile
FROM golang:1.22-alpine AS build
WORKDIR /src
COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /ra2lobbyd ./cmd/ra2lobbyd
FROM scratch
COPY --from=build /ra2lobbyd /ra2lobbyd
USER 65534:65534
ENTRYPOINT ["/ra2lobbyd"]
```

```yaml
# docker-compose.yml - host networking: no NAT/conntrack in front of 900 UDP ports
services:
  ra2lobbyd:
    image: ghcr.io/<user>/ra2lobbyd:latest
    network_mode: host
    restart: unless-stopped
    read_only: true
    command: ["--listen", ":50000", "--relay-ports", "50100-50999", "--server-name", "My RA2"]
```

1. Create a Hetzner CX22 (Ubuntu 24.04, any region near the players) or an
   Oracle Always-Free Ampere instance. Note its public IPv4.
2. Open firewall: cloud firewall / VCN security list *and* `ufw allow 22,50000/tcp;
   ufw allow 50000/udp; ufw allow 50100:50999/udp`. (Oracle images also ship
   iptables rules that must be opened.)
3. Either `curl -fsSL https://get.docker.com | sh` then `docker compose up -d`,
   **or** without Docker: copy the static binary (`GOOS=linux GOARCH=arm64 go build`
   on Windows works out of the box) to `/usr/local/bin`, install the systemd unit,
   `systemctl enable --now ra2lobbyd`.
4. Optional: point a DNS name at it (`ra2.example.com`, A record; DuckDNS is free).
5. In the game: Options -> Multiplayer -> Server address (default baked into
   the build via `RA2.INI [Net] Server=`), or ship the user's server as the
   default. Multiple servers can be listed.
6. Check: `curl localhost:8080/healthz` on the box; in game the Online page
   shows the MOTD and room count.
7. Updates: GitHub Actions builds the image on tag; `docker compose pull && docker
   compose up -d`. The server accepts `net_proto` in `[min,max]` so old clients
   get a clear "please update" message.

---

## 5. Game-level session protocol (messages)

Lockstep model (details owned by N4, this is the transport contract):
the host collects each player's orders for turn *T* and broadcasts a single
authoritative `TURN(T)`; every game executes turn *T* only after receiving it.
Turn length = K sim ticks (sim is 15 ticks/s; K adapts 1-4 to measured RTT,
i.e. 67-267 ms), scheduled `delay` turns ahead so input latency hides RTT.
Host's own orders take the same path (loopback) so the host has no advantage.

### 5.1 Server control plane (TCP 50000)

| Dir | Message | Fields |
|---|---|---|
| C->S | `HELLO` | magic, net_proto, engine_ver, platform, player_id, nickname |
| S->C | `HELLO_OK` / `HELLO_REJECT` | server name, MOTD, min/max net_proto / reason |
| C->S | `LIST_ROOMS` | filter (sim_ver, rules_crc, has_space) |
| S->C | `ROOM_LIST` | [code?, room name, host nick, map name, players/max, flags, ping hint] (codes omitted for private rooms) |
| C->S | `CREATE_ROOM` | name, max_players, private, password_flag, sim_ver, rules_crc, map name |
| S->C | `ROOM_CREATED` | code, host_token |
| C->S | `UPDATE_ROOM` | players/max, map name, started, open slots |
| C->S | `CLOSE_ROOM` | - |
| C->S | `JOIN_ROOM` | code |
| S->C | `JOIN_OK` / `JOIN_FAIL` | client_token / reason (no such room, full, started, version) |
| S->C | `PEER_INCOMING` (to host) | conn_id, client LAN candidates, client mapping (v2 only, for punching) |
| S->C | `PUNCH` | peer candidates, nonce (v2) |
| S->C | `RELAY_READY` | relay_ip, relay_port, conn_id |
| C->S | `RELAY_RELEASE` | conn_id (punch succeeded) |
| C->S | `REJOIN` | room code, session token (reconnect after app restart / network change) |
| both | `PING`/`PONG` | 10 s keep-alive |
| S->C | `NOTICE` | text (server shutting down, MOTD change) |

UDP 50000: `BIND{token}` -> `BIND_ACK{observed public ip:port}`;
`PROBE`/`PROBE_ACK{nonce}` between peers.

### 5.2 LAN discovery (UDP 50010)

`LAN_QUERY{magic, net_proto}`, `LAN_BEACON{...}` (section 3.1).

### 5.3 Host <-> client session (ENet, channels per 2.3)

| Ch | Dir | Message | Purpose |
|---|---|---|---|
| 0 | C->H | `HELLO` | version gate (2.5), nickname, player_id, session token, room password hash |
| 0 | H->C | `WELCOME` / `REJECT` | assigned slot, session token / reason |
| 0 | H->C | `LOBBY_STATE` | full lobby: slots[8]{kind open/closed/human/AI, nick, player_id-hash, side, country, color_ix, team, start_pos, ai_diff, ready, ping}, map {name, map_crc, size, builtin?}, options {credits, unit_count, tech, shroud, crates, superweapons, short_game, mcv_redeploys, game_speed}, host-only flags |
| 0 | C->H | `SLOT_REQUEST` | side/country/color/team/start_pos change for own slot |
| 0 | C->H | `READY` | ready flag (cleared automatically on any settings change) |
| 0 | H->C | `KICKED` | reason |
| 0 | H->C | `START` | final SkirmishConfig, **random seed**, start turn, turn length K, delay, player->house mapping |
| 0 | C->H | `LOADED` | map loaded, sim at turn 0 (host waits for all, with a load progress bar) |
| 0 | H->C | `PAUSE` / `RESUME` | reason (player request, player away, waiting for player, resync) + countdown |
| 0 | H->C | `PLAYER_STATUS` | slot, state (connected, lagging, away, reconnecting, dropped, defeated, AI-takeover) |
| 0 | C->H | `LEAVE` / `SURRENDER` | graceful exit |
| 1 | C->H | `ORDERS` | turn T, [order...] (encoding from N4), ack of last applied TURN |
| 1 | H->C | `TURN` | turn T, K, [slot, order...]*, host's (implicit) frame time |
| 1 | C->H | `SYNC` | turn T, 64-bit sync hash (every 1-2 s; hash definition from N3) |
| 0 | H->C | `DESYNC` | turn T, which slots disagree, action (resync / end) |
| 2 | both | `CHAT` | scope (all, team, host->system), text <= 200 B |
| 2 | C->H | `MAP_REQUEST` | map_crc |
| 2 | H->C | `MAP_CHUNK` | map_crc, offset, total, deflated bytes |
| 2 | H->C | `SNAPSHOT_BEGIN/CHUNK/END` | turn T, size, crc32 of a save-format archive |
| 2 | C->H | `DESYNC_DUMP` | optional compressed state dump for bug reports |
| 0 | C->H | `RESUMED` | reconnect: last_turn_acked; host replies with missing TURNs or a SNAPSHOT |
| 3 | both | `PING`/`PONG` | t_send, t_echo, local sim turn (lag detection) |
| 3 | C->H | `BEACON` | map ping (cell), typing indicator |

The host's turn log (all `TURN` messages since turn 0) is kept for the whole
match (~10-50 KB per 10 min): it enables reconnect replay, replays, and
desync forensics.

---

## 6. Client UX flow (d)

```
Main menu
 '- Multiplayer (new; dialog style matches the existing skirmish page 0x102)
     |- LAN
     |    '- Browser: [list of games: name / host / map / players / ping]
     |         [Host game] [Join] [Join by IP...] [Back]     (MulticastLock held here)
     |- Online  (connects to the configured server; shows MOTD, server name, ping)
     |    |- Browse public rooms (filter: joinable / my version)
     |    |- Join with code  [ K 7 Q 2 M X ]
     |    '- Host game: room name, max players, public/private, password(optional)
     '- Settings: nickname, server address(es), cross-platform allowed, LAN port
```

### 6.1 Lobby (same screen for LAN and online)

- 8 slots: `Open / Closed / Human / AI (Easy|Normal|Hard)`. Host edits any slot;
  a player edits only their own side/country, colour, team, start position.
  Colours are unique (the 8 MP colours, `color_ix`), start positions unique or
  "random"; conflicts are resolved by the host (requests, not edits).
- Map selection (host only): list from the user's own MIX files
  (`multimd.mix`, `expandmd*.mix`, ... via the existing maplist code), with
  player-count filter and preview. Shows map name + `map_crc`.
- Options: credits, unit count, tech level, shroud, crates, superweapons,
  short game, MCV redeploy, game speed (speed must be identical for all and
  is a host setting).
- Chat box with the last 50 lines; system lines for joins/leaves/kicks.
- Ready toggles; host's "Start" enabled when all humans are ready and every
  client has the map; 5-s countdown cancellable by anyone un-readying.
- Host has "Kick", "Ban (for this session)", room code display + copy button,
  "Share" on Android (system share sheet with `ra2://join/K7Q2MX` - an intent
  filter can come later).
- Co-op vs AI: put humans on team A and AI slots on team B; nothing special at
  the transport level.

### 6.2 Map availability and transfer

- Built-in maps (in the user's own MIX files): the host sends name + `map_crc`;
  clients look it up locally by name and verify the CRC. **Built-in maps are
  never transferred** (the copyrighted data stays in each user's install).
- Custom maps (`.map/.mpr/.yrm` the user put in the game folder): clients without
  a CRC match get "Downloading map 34 %" in their slot; host streams
  `MAP_CHUNK`s (deflate, ~50-300 KB, size cap 2 MB) on channel 2; the client
  stores it under `Maps/Custom/<crc>.map`. The map is parsed with the same
  hardened loader as local maps; reject anything over the cap.

### 6.3 In game

- Chat: Enter (desktop) / chat button (Android) -> all / allies toggle; messages
  appear in the existing message line area.
- Latency/lag: small per-player ping indicator in the diplomacy/score panel;
  when the host is waiting for a player's ORDERS longer than ~1 turn, show
  "Waiting for <name>..." and after 10 s a dialog with a countdown and host
  options "Wait" / "Drop player (AI takes over | units removed)".
- Disconnect of a client: `PLAYER_STATUS reconnecting`; game pauses for
  everyone with a 60 s (configurable) countdown; on timeout the host decides.
- Disconnect of the host: v1 - game ends for everyone with "Host left"
  (results screen). v2 - **host migration**: the lowest connected slot becomes
  the new turn aggregator; all clients hold identical state and the turn log up
  to the last executed TURN, so migration = reconnect everyone to the new host
  (via the relay, which only needs to re-point allocations) and resume. Turns
  the old host had broadcast to some but not all clients are re-sent by
  whoever has the highest turn.
- Desync: host compares SYNC hashes per turn; on mismatch -> "Out of sync
  detected at 12:34 (turn 11106). [Resync] [Quit] [Save desync report]".
  Resync = host sends a snapshot and everyone reloads it (section 7.3). If the
  same desync recurs within 60 s, the game ends with the report saved.
- Results screen as for skirmish; the turn log can be saved as a replay later.

---

## 7. Disconnect, reconnect, resync, Android lifecycle (d/e)

### 7.1 What Android does to us

- When the user switches apps / turns off the screen, SDL pauses the main
  loop (`SDL_APP_WILLENTERBACKGROUND`); the native thread stops ticking, so a
  client stops sending ORDERS and the **whole lockstep game stalls** within a
  turn. Sockets themselves usually survive a short background, but Doze, App
  Standby and OEM killers will cut network access or the process after
  seconds to minutes [19][20]. Android 15 also throws socket exceptions for
  network requests started outside a valid process lifecycle [21].
- A foreground service could keep the socket alive, but Play policy limits FGS
  types (no type fits "keep a game connected"; `specialUse` needs review
  justification) and it would not keep the *sim* running anyway. **Decision:
  no foreground service; make reconnect fast and the pause fair.**

### 7.2 Away / reconnect design

1. `RA2Activity.onPause` -> JNI -> net layer sends `AWAY` (best effort, one
   unreliable+one reliable packet) before the loop stops. Host immediately
   broadcasts `PAUSE(reason=away, slot, countdown=90s)` - nobody wastes time
   waiting on a timeout.
2. On `onResume`, the client checks its ENet peer; if the host link is dead
   (or the network changed - `ConnectivityManager.NetworkCallback` in Java ->
   JNI event), it opens a fresh ENet host, re-BINDs on the server (online) and
   sends `HELLO{session_token, last_turn_acked}`.
3. Host verifies the token (16 random bytes issued in WELCOME, bound to the
   slot), then sends every `TURN` after `last_turn_acked` from the turn log;
   the client fast-forwards the sim (no rendering) to catch up, then
   `RESUMED`; host broadcasts `RESUME` with a 3-s countdown.
4. If the app process was killed: on restart, the main menu detects a saved
   `[Net] RejoinRoom/RejoinToken/RejoinTurn` in RA2.INI (written at START,
   cleared at match end) and offers "Rejoin game in progress". There is no
   local state, so the host sends a **snapshot** (7.3) instead of turn replay.
5. Catch-up limit: if > ~3000 turns are missing (or the client's own state is
   gone), use a snapshot instead of replay.
6. Desktop gets the same path for Wi-Fi blips; ENet's peer timeout is raised
   to ~15 s in game (`enet_peer_timeout`) so short outages are absorbed without
   a reconnect at all.

### 7.3 Snapshot resync

- Reuse the P7 save system (`engine/savegame.hpp`: versioned binary archive,
  bit-exact doubles, CRC-32 trailer, `kFormatVersion` = 31). The host
  serialises at a turn boundary, deflates, sends `SNAPSHOT_*` on channel 2,
  everyone (or the rejoining client) loads it and continues from turn T+1.
- **Hard dependency on N3/N4**: `load(save(x))` must continue bit-identically
  *including everything the sim reads* (RNG state, AI state, trigger state,
  queues). The P7 header comment claims this; it must be proven with a
  "save at T, reload, run to T+N, compare against uninterrupted run" gate
  before resync is offered.
- **Sync hash**: today's `world_hash` (`engine/main.cpp:9800`) reads only
  cell/hp/alive per the comments in `world.hpp`; that will miss many desyncs
  (credits, RNG, AI, production). N3 should define a richer per-turn sync hash
  (or hash the save archive bytes every N turns, which is slow but complete).

---

## 8. Security and privacy (e)

- **IP exposure**: online, players only ever see the relay's address; the
  host learns client mappings only in v2 punching, and only for its own room.
  Provide a client setting **"Always use relay (hide my IP)"**, default **on**
  for public rooms, off for code-only rooms among friends. LAN obviously
  exposes LAN addresses only. The server logs truncated IPs.
- **Input validation** (both client and server): every decoder checks lengths
  against the remaining buffer, caps counts (orders per turn <= 256, chat
  <= 200 B UTF-8, nickname 1-16 chars, map <= 2 MB, snapshot <= 16 MB),
  rejects unknown message types per channel, validates enum ranges (slot <8,
  color_ix <8, side, country from the rules list). Orders are validated
  *deterministically* by the sim on every machine (unit belongs to the
  sender's house, building type is buildable, etc.) - an invalid order is
  ignored identically everywhere rather than trusted; this is part of N4's
  order application. Fuzz the codec with libFuzzer/AFL in CI (desktop) and
  run decoders under ASan in tests.
- **Spoofing**: host stamps the sender slot from the ENet peer, never from the
  message body. Relay forwards only between bound tokens. Session tokens are
  random 128-bit, never shown in UI.
- **Lockstep cheat surface** (inherent, document it, don't fight it in v1):
  - *Map hack / shroud reveal*: every client simulates the whole world; any
    memory reader sees everything. Mitigation is impossible in lockstep.
  - *Speed hacks / order injection*: impossible to affect others beyond
    sending legal orders - the sim validates orders, and a client that
    computes differently just desyncs itself.
  - *Lag switching / stalling*: the host's waiting dialog + drop vote handles it.
  - *Modified rules*: blocked by the `rules_crc` handshake (a cheater can lie
    about the CRC, but then desyncs on the first divergent event).
  - *Host advantage*: host's orders go through the same turn delay;
    the host can still kick/ban - acceptable for a friends-oriented game.
- **Chat**: unencrypted in v1 (stated in the UI); profanity filter optional; local
  mute per player.
- **Privacy**: no accounts, no e-mail, no analytics; `player_id` is random,
  regenerable from settings. Play Store "data safety": nicknames + IP processed
  by the user's own server for matchmaking, not stored. If the user ships the
  app on Play with their server baked in, a short privacy policy is required.
- **Server hardening**: section 4.8.

---

## 9. Phased plan and effort

Effort in engineer-days for one developer familiar with the engine
(transport/infra only; turn logic and determinism are N4/N3 estimates).

| Phase | Scope | Effort | Exit gate |
|---|---|---|---|
| **T0 Foundations** | vendor ENet 1.3.18 + miniz in `third_party/`, CMake + Android jni build, MinGW `ws2_32 winmm`; `engine/net/codec` with golden vectors; `NetTransport` wrapper (channels, MTU 1200, CRC, timeouts) | 3-4 d | loopback unit test: 2 ENet hosts in one process exchange all message types; Android APK still builds and runs |
| **T1 LAN lobby** | LanBeacon/LanBrowser (per-interface broadcast, JNI MulticastLock, interface list), HELLO/WELCOME version gate, host-authoritative lobby state machine + lobby UI page, chat, START with seed -> `SkirmishConfig` | 8-10 d | 2 PCs + 1 phone on one Wi-Fi see each other, fill slots, start the same map with identical config |
| **T2 LAN lockstep** (with N4) | ORDERS/TURN on channel 1, adaptive turn length, waiting dialog, SYNC hash + desync message, leave/drop/AI takeover | 6-8 d (transport share) | 1v1 and 2v2+AI full matches on LAN without desync (N3 gate) |
| **T3 Server + relay (online v1)** | `server/` Go daemon: control plane, rooms, codes, BIND, port-allocation relay, limits, metrics, Dockerfile, systemd unit, deploy README; client `ServerLink`, Online browser/code join/host; always-relay | 7-9 d | match between a phone on LTE and a PC at home via the user's Hetzner/Oracle box |
| **T4 Reconnect + resync** | AWAY/PAUSE on onPause, session tokens, turn-log replay, NetworkCallback re-BIND, RA2.INI rejoin, snapshot via save archive (needs N3 save/reload bit-exact gate) | 6-8 d | kill the Android app mid-match, relaunch, rejoin and finish the match in sync |
| **T5 Custom map transfer** | MAP_REQUEST/MAP_CHUNK, cache, limits | 2 d | a custom map plays on a client that did not have it |
| **T6 Hole punching** | PEER_INCOMING/PUNCH/PROBE, candidate ordering, parallel relay fallback, "hide my IP" setting | 4-5 d | direct link measured on 2 home NATs; fallback verified with a cellular phone |
| **T7 Hardening** | codec fuzzing, server soak test (bots: 100 rooms), abuse limits verified, admin commands, privacy text | 3-4 d | 24 h soak, no leaks; fuzzers clean |
| **T8 (optional)** | host migration; TLS control plane + payload encryption; IPv6 P2P; mDNS; CnCNet tunnel shim (with N2) | 10-15 d | - |

**Total T0-T7 ~ 39-50 engineer-days (~8-10 weeks)**, of which the online
infrastructure itself (T3+T6) is ~2.5 weeks. A minimal "LAN + relay, no
reconnect" playable milestone is T0-T3 ~ 5-6 weeks.

---

## 10. Risks

1. **Determinism across platforms (highest).** Lockstep between Win64 x86-64
   (MinGW) and Android arm64 needs bit-identical float behaviour (FMA
   contraction on arm64, x87 vs SSE, `libm` differences). Until N3 proves it,
   rooms must default to same-platform only. The transport is unaffected; the
   product promise is.
2. **Snapshot resync depends on save/load being bit-exact** for every piece of
   sim state; any field missed in the save visitors turns resync into a
   delayed desync. Needs a dedicated gate.
3. **Weak sync hash** (`world_hash` = cell/hp/alive) would let desyncs go
   unnoticed for minutes; desync reports will be confusing. N3 to define.
4. **Android background behaviour varies by OEM** (Xiaomi/Huawei/Samsung
   killers). Mitigated by pause + rejoin-from-snapshot, but a user who leaves
   the app for minutes will stall a game; the drop vote is the safety valve.
5. **Hosting drift.** Free tiers change (Oracle cut A1 to 2 OCPU / 12 GB in
   June 2026; Fly removed its free allowance). Keep the server a plain Linux
   binary so moving hosts is a 10-minute job.
6. **Relay bandwidth** is tiny for lockstep, so cost is not a risk at hobby
   scale; it becomes one only if someone abuses the relay - hence bound-pair
   forwarding and per-allocation caps.
7. **ENet is IPv4-only upstream**; IPv6-only networks rely on CLAT (present
   on Android, absent on some desktop IPv6-only setups - rare).
8. **Hotspot/AP isolation** breaks LAN discovery on some devices; Join by IP
   and online-via-relay are the fallbacks.
9. **Unencrypted v1** traffic (chat, orders) is readable on-path; acceptable for
   a hobby game, but it must be disclosed and is the first thing to fix if the
   game is ever distributed widely.
10. **Legal/data**: only custom maps are transferred; built-in maps and all
    game data stay in each user's own install (consistent with the setup
    flow). The server never carries copyrighted assets.

---

## 11. Repository impact summary (for the implementation phase)

- New: `third_party/enet/` (MIT), `third_party/miniz/` (MIT) + `THIRD_PARTY_NOTICES.md`
  entries; `engine/net/{codec,transport,path,lan,serverlink,session_host,session_client}.{hpp,cpp}`;
  lobby/browser UI pages; `server/` (Go module, Dockerfile, compose, systemd,
  README); `tests/net_*` (codec vectors, loopback transport, lobby state machine).
- Changed: `CMakeLists.txt` + Android jni build (sources, `ws2_32 winmm`),
  `AndroidManifest.xml` (4 normal permissions, comment), `RA2Activity.java`
  (MulticastLock, interface list, NetworkCallback, onPause/onResume hooks),
  RA2.INI `[Net]` section (Nickname, PlayerId, Server, LanPort, AlwaysRelay,
  CrossPlatform, Rejoin*), `SkirmishConfig` gains `seed` and per-slot
  `player_id`/`is_remote` (N4).

---

## Sources

1. ENet repository (MIT licence): https://github.com/lsalzman/enet
2. ENet ChangeLog (1.3.18, 2024-04-14): https://raw.githubusercontent.com/lsalzman/enet/master/ChangeLog ; docs: http://sauerbraten.org/enet/
3. SDL_net (zlib): https://github.com/libsdl-org/SDL_net
4. Valve GameNetworkingSockets (BSD-3): https://github.com/ValveSoftware/GameNetworkingSockets
5. netcode / yojimbo (BSD-3): https://github.com/mas-bandwidth/netcode , https://github.com/mas-bandwidth/yojimbo
6. libdatachannel / libjuice (MPL-2.0): https://github.com/paullouisageneau/libdatachannel , https://github.com/paullouisageneau/libjuice
7. msquic (MIT): https://github.com/microsoft/msquic
8. Android 464XLAT / CLAT and IPv6-only networks: https://developer.android.com/training/basics/network-ops/ipv6 (Android "Support IPv6" guidance) and RFC 6877 (464XLAT)
9. Android `WifiManager.MulticastLock` reference: https://developer.android.com/reference/android/net/wifi/WifiManager.MulticastLock
10. Chainfire, "Android Datagrams, UDP broadcast and multicast" (MulticastLock also needed for broadcast on some devices): https://chainfire.eu/articles/742/_dev_Android_Datagrams_UDP_Broadcast_and_Multicast_.
11. Ford, Srisuresh, Kegel, "Peer-to-Peer Communication Across Network Address Translators" (~82 % UDP punchable): https://arxiv.org/abs/cs/0603074
12. "Challenging Tribal Knowledge - Large Scale Measurement Campaign on Decentralized NAT Traversal" (2025, 70 % +/- 7.1 %): https://arxiv.org/html/2510.27500v1
13. ProbeLab, "Can libp2p punch through NATs?": https://probelab.io/blog/can-libp2p-punch-through-nats/
14. Hetzner CX22 pricing/traffic 2026: https://deployhandbook.com/pricing/hetzner , https://dev.to/byte-guard/hetzner-review-2026-my-honest-experience-nd2
15. Hetzner CX22 plan data: https://vpsbenchmarks.com/hosters/hetzner/plans/cx22
16. Oracle Cloud Always Free breakdown (10 TB egress, storage): https://fullmetalbrackets.com/blog/oci-free-tier-breakdown
17. Oracle A1 free allocation reduced to 2 OCPU / 12 GB (June 2026): https://bestusavps.com/reviews/oracle-cloud/
18. Fly.io pricing (shared-cpu-1x 256 MB ~$1.94/mo; dedicated IPv4 $2/mo; free allowance legacy-only): https://fly.io/docs/about/pricing/ , https://fly.io/docs/networking/udp-and-tcp/
19. Android Doze and App Standby: https://developer.android.com/training/monitoring-device-state/doze-standby
20. "Beyond Doze: reliable background execution incl. OEM realities": https://proandroiddev.com/beyond-doze-building-reliable-background-execution-on-modern-android-including-oem-realities-5fa0a6e05672
21. Android 15 behaviour changes (network requests outside a valid lifecycle): https://engineeringportal.nielsen.com/wiki/Mitigation_of_Android_15_background , https://developer.android.com/about/versions/15/behavior-changes-15
