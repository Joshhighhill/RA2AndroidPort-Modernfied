# N2 — The CnCNet ecosystem for RA2/YR, and what joining it would take

Research date: 2026-10-10. Read-only research; no engine code was changed.
Scope: how present-day original RA2/YR online play works through CnCNet (lobby, tunnels, spawner,
LAN), what a third-party client would have to implement to join a real CnCNet YR game, and the
licence/policy position. N1 covers gamemd's own in-game packet layer (EventClass/IPX/UDP framing);
this report covers everything around it, plus the CnCNet patches to that layer.

Sources were read **from the source code** where possible (GitHub `CnCNet/*` repos, `develop`/`main`
branches as of this date), not from forum summaries. A copy of every source file quoted was fetched
into the session scratchpad. File-level citations are listed at the end of each section and in
§9.

---

## 0. TL;DR

* **CnCNet YR is no longer "vanilla YR 1.001 over a tunnel."** Since the YR 9.0 client (posted
  2025-11-10) every CnCNet YR match runs `Syringe.exe -i=Ares.dll -i=CnCNet-Spawner.dll -i=Phobos.dll gamemd-spawn.exe -SPAWN ...`,
  so the game runs **Ares 3.0p1 + Phobos v0.4 + the yrpp-spawner** injected into the YR 1.001
  executable. On top of that come CnCNet's own rule and asset packs (`cncnet.mix`, the
  `INI/Map Code/*.ini` and `INI/Game Options/*.ini` overlays, `spawner.xdp`). A peer has to
  reproduce **that** patched simulation bit for bit, not stock YR.
* **The lobby is IRC** on the public **GameSurge** network. It uses the channels `#cncnet`,
  `#cncnet-yr` (chat) and `#cncnet-yr-games` (game listing), plus one `+k` password-protected
  channel per game room. All lobby state travels as IRC CTCP messages with short text verbs (`GAME`,
  `PO`, `GO`, `R`, `FHSH`, `STARTV2`/`STARTV3`, `MAPREQ`…).
* **The tunnel protocol is small and fully documented by its GPLv3 source.** V2 uses a 4-byte header
  `[from u16 BE][to u16 BE]`, and the server hands out IDs over HTTP `/request?clients=N`. V3 uses an
  8-byte header `[sender u32 LE][receiver u32 LE]`, IDs are chosen by the client and the server
  learns them on first packet. Tunnel ping is a 50-byte packet answered with 12 bytes. A STUN-like
  helper runs on UDP 8054/3478. The current XNA client (develop) adds a **client-side V3 bridge with
  per-pair negotiation and P2P hole-punching**.
* **The game itself never talks V3.** The spawner prefixes every gamemd datagram with
  `[myId u16][destId u16]` and sends it to `[Tunnel] Ip:Port`. In V3 mode that address is
  `127.0.0.1:<bridge port>` of the XNA client, which rewrites the header to V3.
* **CnCNet YR uses its own network protocol variant**, `Protocol=0` ("ProtocolZero") with
  `FrameSendRate=2`. It adds a custom event `0x30 ResponseTime2` that adapts MaxAhead to latency.
  Phobos adds events `0x40/0x41` and Ares adds `0x60/0x61`. All are forced via
  `[ForcedSpawnIniOptions]`.
* **Licences are permissive enough to self-host everything.** The tunnel server, XNA client,
  spawner, ladder API and Phobos are GPL/LGPL, and Ares is BSD-4. The tunnel server has
  `--nomaster`, and the XNA client reads its IRC server list and tunnel-list URL from
  `NetworkDefinitions.ini`. CnCNet has **no written policy for or against third-party clients**.
  Its rules ban cheating, "unauthorised third party tools that can be used to cheat", and anything
  "perceived as threats to CnCNet". The anti-cheat file-hash exchange (`FHSH`) is self-reported, so
  a clone could only pass it by sending another client's hash string. That is spoofing, and it is
  the single biggest policy risk.
* **Verdict:** a *lobby + tunnel* client that can see and enter CnCNet YR rooms is a moderate,
  well-specified job (≈3–5 engineer-weeks). **Actually playing a match against real gamemd peers is
  gated by N3**: bit-exact emulation of YR 1.001 + Ares 3.0p1 + Phobos 0.4 + spawner desync fixes +
  CnCNet rules overlays. That is a multi-person-year reverse-engineering effort, and it has to track
  a moving target (Phobos and the spawner ship nightly). The recommended path is our **own
  CnCNet-style stack** (self-hosted IRC-or-WS lobby + the GPL tunnel server, or a re-implementation
  of it) for clone-to-clone play. Original-game interop should be an explicit stretch goal that is
  only attempted after N3 proves determinism against gamemd.

---

## 1. Local install check (`D:\Red Alert 2`)

| Finding | Detail |
|---|---|
| CnCNet client installed? | **No.** There is no `Resources\clientdx.exe`, `ClientDefinitions.ini`, `Syringe.exe`, `Ares.dll`, `Phobos.dll`, `CnCNet-Spawner.dll`, `spawn.ini` or `gamemd-spawn.exe`. |
| Executable | `gamemd.exe`: 4,808,976 bytes, SHA-1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`, version resource `1.05` (product "Command & Conquer : Yuri's Revenge"). Whether this is the YR 1.001 image YRpp/Ares expect is **not verified here** (N1 should confirm against the YRpp address map; the version-resource string alone doesn't settle it). |
| `ddraw.dll` (42 KB) | A ddraw wrapper is present. It is not CnCNet's cnc-ddraw package. |
| `LAN-play-serials\1..4\{ra2.reg,Woldata.key}` | Evidence of **vanilla in-game LAN** use. The original LAN lobby rejects duplicate CD keys, so each machine needs a distinct serial. This is the original Westwood LAN path, unrelated to CnCNet. |
| `RA2MD.INI [Network] Socket=65535 NetCard=0` | Stock settings for the original IPX/UDP LAN code. |
| `wolinfo.ini` | 0 bytes. Westwood Online is defunct. |

Conclusion: we have the stock game only. To capture real CnCNet traffic for N1/N3 we would need to
install the CnCNet YR client package separately, e.g. in a copy of the game directory. Don't modify
`D:\Red Alert 2`.

---

## 2. How a CnCNet YR game is formed

### 2.1 Components on a player's machine

From `cncnet-yr-client-package/package/` (develop):

| File | Role |
|---|---|
| `CnCNetYRLauncher.exe` | Entry point. It checks the install and starts the XNA client (`Resources/clientdx.exe` / `clientogl.exe` / `clientxna.exe`). |
| `Resources/client*.exe` | **XNA CnCNet Client** (Rampastring, GPLv3, C#). It runs the lobby, tunnels, map sharing, spawn.ini writing and the launch. |
| `Syringe.exe` | DLL injector (SyringeEx, LGPLv3). |
| `Ares.dll` + `ares.mix` | Ares 3.0p1 engine extension (BSD-4). |
| `Phobos.dll` | Phobos v0.4 engine extension. The package README says GPLv3; the Phobos repo LICENSE is LGPLv3. |
| `CnCNet-Spawner.dll` | **yrpp-spawner** (GPLv3). Reads `spawn.ini`, skips the menus, patches networking. |
| `Qt/CnCNetQM.exe` | Quick Match (ladder) client. |
| `INI/MPMaps.ini`, `INI/Map Code/*.ini`, `INI/Game Options/*.ini` | Game-mode and option overlays that get merged into `spawnmap.ini`. |
| `Resources/ClientDefinitions.ini`, `GameOptions.ini`, `GameLobbyBase.ini`, `NetworkDefinitions.ini` | Client configuration: launch line, forced spawn options, option tables, IRC server list and URLs. |
| `game-assets/cncnet.pack`, `ra2mode.pack` | Packed into `.mix` at build time. They hold extra assets and RA2-mode `rulesmd.ini`/`artmd.ini`. |

The launch line (`ClientDefinitions.ini`) is:

```
GameExecutableNames=Syringe.exe
ExtraCommandLineParams=-i=Ares.dll -i=CnCNet-Spawner.dll -i=Phobos.dll gamemd-spawn.exe --args="-SPAWN -LOG -CD -Include -Inheritance -RA2ModeSaveID=0x8d113b94"
ClientGameType=Ares
CompatibilityCheckExecutables=CnCNetYRLauncher.exe,gamemd.exe,gamemd-spawn.exe
DisallowJoiningIncompatibleGames=true
```

`gamemd-spawn.exe` is the game executable actually started; it is listed alongside `gamemd.exe` in
the compatibility check. It is not in the package repo, so it is produced on the user's machine.
Presumably the launcher copies the user's YR 1.001 `gamemd.exe` (not verified in source). Ares
injects into the YR 1.001 `gamemd.exe` and supports the patched CD, TFD and Ultimate Collection
versions.

### 2.2 Lobby: IRC on GameSurge

* **Servers** (`NetworkDefinitions.ini [IRCServers]`): `irc.gamesurge.net:6667` plus about 8 named
  GameSurge leaf servers and IPs on ports 5960/6660–6669/6697/7000/8080. The hard-coded fallback in
  `Connection.cs` is `irc.gamesurge.net|GameSurge|6667,6660,6666,6668,6669`. CnCNet runs no IRC
  server of its own. It uses the public GameSurge network, whose AUP applies.
* **Registration** (`Connection.Register`): `USER <localGame>.<systemId> 0 * :<GAME_VERSION> <localGame> CnCNet`,
  then `NICK <playername>`. For YR `localGame` = `YR`. `GAME_VERSION` is the client package version
  string. `MaxNameLength=15`.
* **Channels** (`GameCollection.cs`): `#cncnet` (global), `#cncnet-yr` (YR chat),
  `#cncnet-yr-games` (YR game broadcast). Other games follow the same pattern (`#cncnet-mo` =
  Mental Omega, `#cncnet-ts`, `#cncnet-ra`, …). In the CnCNet YR client, RA2 is offered as
  "RA2 Mode" (`Ra2Mode=` spawn key + `ra2mode.pack`) rather than as a separate client.
* **Live player counts:** `https://api.cncnet.org/status` returned on 2026-10-10:
  `{"cncnet5":757,"cncnet5_yr":311,"cncnet5_mo":261,"cncnet5_ra":98,...}`, i.e. about 300 YR players
  online at that moment.

### 2.3 Game listing: the `GAME` CTCP

The host broadcasts in `#cncnet-yr-games` on a timer (`CnCNetGameLobby.BroadcastGame`). Fields are
`;`-separated and there must be exactly 14 of them:

```
GAME <rev>;<gameVersion>;<maxPlayers>;<roomChannel>;<roomName>;<flags5>;<p1,p2,..>;<mapName>;<gameMode>;<tunnel>;<loadedGameId>;<skill>;<mapSHA1>;<packedOptions>
```

* `rev` = `CNCNET_PROTOCOL_REVISION` = `"A"`. Messages with another revision are dropped.
* `flags5` = 5 chars: locked, customPassword, closed, isLoadedGame, isLadder.
* `tunnel` = `ip:port` of the host-selected tunnel, or `[DYN]` for V3 dynamic per-pair tunnels.
* `packedOptions` = broadcastable checkboxes packed into 32-bit ints, followed by dropdown indices.
  These are only interpreted if `gameVersion` matches the client's exactly.
* A listed game whose `gameVersion` differs is flagged `Incompatible`, and with
  `DisallowJoiningIncompatibleGames=true` the YR client **refuses to join it**. **A third-party
  client must therefore advertise the current CnCNet YR package version string** to interoperate.

### 2.4 Game room: a password-protected IRC channel

* The host creates `#cncnet-yr-game<7 random digits>`. It sets `MODE <chan> +klnNs <password> <limit>`
  and `TOPIC <chan> :A;yr`.
* Default password (no custom password) = `SHA1(channelName)` hex, first 10 chars. For a loaded
  (saved) game it is `SHA1(spawnSG.ini [Settings] GameID)` first 10 chars. Joiners compute the same
  value, so "password" here is only a soft lock.
* Lobby protocol is CTCP inside the room channel. Handlers are registered in
  `CnCNetGameLobby` ctor:

| Verb | Dir | Meaning |
|---|---|---|
| `OR <int>` / `R <int>` | C→H | options request / ready state |
| `PO …` | H→all | player options. Per player: `name;int32` where the int packs team, start, colour, side… (bytes of an LE int32). AI players are encoded as an integer AI level instead of a name. |
| `GO …` | H→all | game options: N packed checkbox int32s, then each dropdown index, then `mapOfficial;mapSHA1;gameMode;FrameSendRate;MaxAhead;ProtocolVersion;RandomSeed;RemoveStartingLocations;mapName;tunnelMode` |
| `FHSH <hash>` | C→all | **file hash** of the game files (see §4.3) |
| `TNLPNG <ms>` | C→all | ping to current tunnel |
| `MAPREQ/MAPOK/MAPFAIL <sha1>`, `MAPSDISABLED` | both | custom-map sharing (§2.5) |
| `STARTV2 gameId;name;tunnelIp:id;…` | H→all | start, V2 tunnel. The IDs come from the tunnel's `/request`. |
| `STARTV3 gameId;<v3id>;<name>;<tunnelIp:port>;…` | H→all | start, V3 (3 fields per player) |
| `NEGRPT`, `RENEGALL`, `CHTNL ip:port` | both | V3 negotiation report / renegotiate / change tunnel |
| `STRTD`, `RETURN`, `GETREADY`, `INSFSPLRS`, `TMPLRS`, `CLRS`, `SLOC`, `LCKGME`, `NVRFY n`, `INGM n`, `AISPECS` | H→all | notifications |
| `CD`, `MM <name>` | any | cheat detected / modified-files ("cheater") notice |
| `DR sides,results` | any | dice roll |
| `GSETTINGS` | H→all | room name, max players, skill, password |

Chat is ordinary channel PRIVMSG with mIRC colour codes.

Important coupling: the `GO` message is **positional**. Its layout depends on how many checkboxes and
dropdowns are defined in the YR package's `GameLobbyBase.ini`/`GameOptions.ini`, in order. A
third-party lobby must carry an exact copy of the current package's option tables, along with the
INI overlay each option maps to.

### 2.5 Map transfer

* Official maps ship in the package (`Maps/Yuri's Revenge/**`, about 570 map files, indexed by
  `INI/MPMaps.ini`) and are identified by **SHA-1** of the map file.
* For custom maps, the host uploads `<sha1>.zip` (containing `<sha1>.map`) as an HTTP POST to
  `https://mapdb2.cncnet.org/upload` (game id `yr`). A joiner missing the SHA-1 downloads from
  `https://mapdb2.cncnet.org/...` after `MAPOK`. The client defaults are `mapdb.cncnet.org`; the YR
  package overrides them to `mapdb2`. Lobby messages only ever carry the SHA-1.

### 2.6 spawn.ini / spawnmap.ini and launch

On start, every client writes its **own** `spawn.ini` (from `GameLobbyBase.WriteSpawnIni` +
`MultiplayerGameLobby/CnCNetGameLobby.WriteSpawnIniAdditions`). It also writes `spawnmap.ini`: the
map file plus the game-mode INI (`INI/Map Code/<mode>.ini`) plus every enabled option's INI (e.g.
`INI/Game Options/Yuri Rebalance Patch.ini`, a full rules delta). Then it runs the launch line from
§2.1. Keys actually consumed by the spawner (`Spawner.Config.cpp`):

```ini
[Settings]
Name=..  Side=..  Color=..  IsSpectator=..        ; local player (index 0)
Scenario=spawnmap.ini  UIMapName=..  UIGameMode=..  MapHash=..
GameMode=1  Seed=<RandomSeed>  TechLevel=10  IsSinglePlayer=no  Tournament=0  GameID=<UniqueGameID>
Bases= Credits= BridgeDestroy= Crates= ShortGame= Superweapons= BuildOffAlly= GameSpeed=
MultiEngineer= UnitCount= AIPlayers= AIDifficulty= AlliesAllowed= HarvesterTruce= FogOfWar= MCVRedeploy=
Protocol=0  FrameSendRate=2  MaxAhead=  PreCalcMaxAhead=  MaxLatencyLevel=  ReconnectTimeout=1400  ConnTimeout=
Port=<my tunnel id / listen port>
LoadSaveGame= SavedGameDir= SaveGameName= AutoSaveCount= AutoSaveInterval= NextAutoSaveNumber=
Ra2Mode= QuickMatch= DisableGameSpeed= SkipScoreScreen= WriteStatistics= DisableChat= ContinueWithoutHumans=
DefeatedBecomesObserver=yes  Observer.ShowAIOnSidebar=yes  SpecialHouseIsAlly=no  AINamesByDifficulty=yes
[Tunnel]  Ip=<tunnel ip | 127.0.0.1>  Port=<tunnel port | bridge port>
[Other1..7]  Name= Side= Color= IsSpectator= Ip= Port=<peer tunnel id>
[HouseCountries]/[HouseColors]/[HouseHandicaps] MultiN=..     ; AI slots
[SpawnLocations] MultiN=..  [IsSpectator] MultiN=..  [CreditsFactor]/[HandicapDifficulty] MultiN=..
[MultiN_Alliances] HouseAllyOne..Eight=..
```

`GameOptions.ini [ForcedSpawnIniOptions]` (YR package) **always** forces `Protocol=0`,
`FrameSendRate=2`, `ReconnectTimeout=1400`, `Bases=Yes`, `FogOfWar=No`, `SidebarHack=Yes`,
`AINamesByDifficulty=Yes`, `Observer.ShowAIOnSidebar=Yes`, `SpecialHouseIsAlly=No`,
`DefeatedBecomesObserver=Yes`. Map/game-mode `[ForcedOptions]` can override lobby options
(`ForcedOptions.md`).

### 2.7 What the yrpp-spawner patches (gamemd-side)

`CnCNet/yrpp-spawner` (GPLv3, built on YRpp; the "hardened" anti-cheat build uses a **private**
submodule `CnCNet/yrpp-spawner-private`):

* **Session bootstrap** (`Spawner.cpp`): builds `NodeNameType` entries from spawn.ini. Peer *i* gets
  the fake address `sin_addr = i` and its real ip/port is kept in `ListAddress::Array`. It sets
  `GameMode=Internet` and then flips it to `LAN` after `StartScenario`. It calls
  `SessionClass::CreateConnections()`.
* **Network constants** (`Spawner::InitNetwork`): `UDPInterfaceClass` listens on `ListenPort`, or on
  port 0 when tunnelled. `IPXManager.SetTiming(60,-1,600,1)`, `GameStockKeepingUnit=0x2901`,
  `ProtocolVersion=2`, `MaxAhead = FrameSendRate*6` unless given, `RequestedFPS=60`,
  `LatencyFudge=0`, `ReconnectTimeout` from ini.
* **ProtocolZero** (`Protocol=0`): `FrameSendRate=2`, starting `WorstMaxAhead` = level 6 (24 frames).
  Every 30 frames each player sends extension event **`0x30 ResponseTime2`**
  `{int8 MaxAhead, uint8 LatencyLevel}` (EventExt is 111 bytes packed, data at offset 7). Everyone
  takes the worst level and **only ever raises** it. Level→MaxAhead: `1,4,6,12,16,20,24,28,32,36`.
  The spawner hooks `QueueAIMultiplayer`, `ExecuteDoList` and the event-size tables
  (0x64B6FE/0x64BE7D/0x64C30E).
* **NetHack/Tunnel** (`NetHack.cpp`, ISC-licensed code by Toni Spets): wraps `sendto`/`recvfrom`.
  Outgoing packets get the header `[Tunnel::Id u16][dest port u16]`, **both in network order** (the
  spawner stores `htons(id)`), and go to `Tunnel::Ip:Port`. Incoming packets are dropped unless the
  `to` field equals our id. The `from` field becomes the source "port" and is mapped back to the
  player index. Max payload is 1024 bytes.
* **Desync fixes** (`Bugfixes.Desyncs.cpp`, 400+ lines): adds missing `CellClass::RecalcAttributes`
  calls for crates, flags, veinholes and roughly 30 bridge code paths. It also disables the
  aircraft-shadow `Set_Height(0)` calls, which changed `LandType` only for players who could see
  the aircraft. **These change simulation results**, so a compatible peer must replicate the
  *fixed* behaviour.
* **Other:** copy-protection/launcher bypass (`CopyProtection.cpp`, including disabling the "blow
  your base up" anti-piracy AuxLoop), NoCD, multiplayer save/load + autosaves (`spawnSG.ini`),
  observers, in-game chat toggle, QuickMatch name hiding, `stats.dmp` statistics writer (for the
  ladder), event verification checks, game-speed slider toggle, random-map support, custom mixes.

Ares and Phobos are loaded too. Phobos reserves events **0x40 ApproachObject** and **0x41
TogglePlayerAutoRepair**, and its comments note "CnCNet reserved Events from 0x30 to 0x3F" and "Ares
used Events 0x60 and 0x61". Vanilla uses 0x00–0x2F. Ares and Phobos also change much in-game logic
(YR 9.0 notes cite burst-abuse, pathfinding and oil-refinery chrono fixes, among "countless
bugfixes"), and CnCNet's rules overlays (e.g. RA2 mode, Yuri Rebalance Patch, crate/AI option
INIs) change data.

Sources: [yrpp-spawner](https://github.com/CnCNet/yrpp-spawner) (`src/Spawner/*.cpp`,
`src/Misc/Bugfixes.Desyncs.cpp`, `src/Ext/Event/Body.*`, `README.md`, `.gitmodules`);
[cncnet-yr-client-package](https://github.com/CnCNet/cncnet-yr-client-package)
(`package/Resources/ClientDefinitions.ini`, `GameOptions.ini`, `NetworkDefinitions.ini`,
`ForcedOptions.md`); [Phobos Ext/Event/Body.h](https://github.com/Phobos-developers/Phobos/blob/develop/src/Ext/Event/Body.h);
[YR 9.0 announcement](https://forums.cncnet.org/topic/13131-yuris-revenge-90-phobosares/);
[Ares docs](https://ares-docs.readthedocs.io/zh-cn/latest/).

---

## 3. Tunnel protocol and LAN mode

### 3.1 Tunnel discovery

`https://cncnet.org/master-list` (plain text, `;`-separated). Live sample:

```
address;country;countrycode;name;password;clients;maxclients;official;latitude;longitude;version;distance
49.13.152.109:50001;Germany;DE;CnCNet Europe;0;0;250;2;0;0;3;0
49.13.152.109:50000;Germany;DE;CnCNet Europe;0;13;250;2;0;0;2;0
49.13.152.109:50002;Germany;DE;CnCNet Europe (Matchmaking);0;0;250;2;0;0;4;0
```

`official`: 2 = official, 1 = recommended. `version`: 2, 3, or 4 (= matchmaking/QM tunnels, port
50002). Official hosts were seen in FR, DE, AU, JP and US. Tunnels register themselves by HTTP GET to
`http://cncnet.org/master-announce?version=&name=&port=&clients=&maxclients=&masterpw=&maintenance=`
every 60 s. A tunnel started with `--nomaster` stays private.

### 3.2 Tunnel server (`CnCNet/cncnet-server`, C#/.NET, GPLv3)

One process runs: **V3 on UDP 50001**, **V2 on UDP+HTTP 50000**, and (unless `--nop2p`) the
**STUN-like helpers on UDP 8054 and 3478**. Options: `--name --maxclients(200) --iplimit(8)
--iplimitv2(4) --master --masterpw --maintpw --nomaster`.

**V3** (`TunnelV3.cs`):
* Header: `uint32 senderId (LE) | uint32 receiverId (LE) | payload` (min 8 bytes, buffer 1024).
* `sender==0 && receiver==0 && size==50` → **ping**: reply with the first 12 bytes (rate-limited to
  20 per IP per minute, 5,000 globally).
* `sender==0 && receiver==0xFFFFFFFF`, then a 1-byte command and a 20-byte SHA-1 of the maintenance
  password → maintenance toggle.
* Otherwise: the first packet from an unknown `senderId` **registers** it to the source endpoint.
  This is subject to MaxClients and IpLimit; IDs are chosen by the clients. A known ID arriving from
  a new endpoint is only re-bound after the old mapping timed out (NAT rebinding). The packet is
  then forwarded verbatim (header included) to the endpoint mapped to `receiverId`. Packets
  where `sender==receiver` are dropped, as are packets from loopback, any, broadcast or port 0.
  Mappings expire on timeout during the 60-s heartbeat sweep.
* No authentication and no encryption: anyone who knows or guesses an ID pair can inject. IDs are
  32-bit random.

**V2** (`TunnelV2.cs`):
* HTTP on the same port: `GET /request?clients=N` (2–8) → `[id1,id2,...]` (random signed 16-bit
  IDs, pre-allocated); `GET /status`; `GET /maintenance/<pw>`.
* UDP header: `int16 senderId (BE) | int16 receiverId (BE) | payload`. The first packet from an
  allocated ID binds the endpoint, and only allocated IDs are forwarded. Ping is the same as V3
  (50 bytes → 12).
* **This header is exactly what the spawner emits**, so in V2 mode gamemd talks to the tunnel
  directly.

**STUN-like helper** (`PeerToPeerUtil.cs`): request = 48 bytes with `0x66B6` (26262) big-endian at
offset 0. Response = 40 bytes: bytes 0–3 are the IPv4 address, bytes 4–5 the port (BE), the first
6 bytes are XOR-ed with `0x20`, and the rest is random. Rate-limited to 20 per IP per minute.

### 3.3 XNA client V3 bridge, negotiation and P2P (develop branch)

* Tunnel mode follows from the host's selection: no tunnel → **V3Dynamic** (`[DYN]`, per player-pair
  negotiation); a v3 tunnel → **V3Static**; a v2 tunnel → **V2Legacy**.
* In V3 modes the client writes `[Tunnel] Ip=127.0.0.1, Port=<reserved local port>` into spawn.ini.
  `V3GameTunnelBridge` reads gamemd's datagrams, takes the **16-bit destination id** at payload
  offset 2 (BE) to choose the recipient, prepends the 8-byte V3 header (`localId`, `recipient.Id`,
  both u32 LE) and sends to that recipient's chosen tunnel, or to a direct P2P endpoint. Inbound V3
  packets are stripped and forwarded to the game's endpoint.
* Control packets share the V3 wire. After the 8-byte header comes the magic `"CNCNET"` (6 bytes)
  and then a type byte: `01 Connected, 02 PingRequest, 03 PingResponse, 04 TunnelChoice, 05
  TunnelAck, 06 NegotiationFailed, 07 Register, 08 GameData, 09 P2PInfo (IPv4+port BE), 0A
  P2PDecline, 0B/0C KeepAlive ping/pong, 0D/0E ProbeRequest/ProbeReport`. Players ping candidate
  tunnels pairwise, agree via `TunnelChoice/Ack`, report over IRC (`NEGRPT`), and can upgrade to a
  direct hole-punched path using STUN results (tunnel servers' 3478/8054, plus optional
  `P2PStunServers`).
* NAT handling summary: (1) relay through a public tunnel (always works; the default), (2)
  opportunistic UDP hole punching in V3Dynamic. No UPnP. No TURN beyond the tunnels themselves.

### 3.4 LAN mode (XNA client)

`ProgramConstants`: `LAN_PROTOCOL_REVISION="RL8"`, **UDP 1232** = lobby broadcast (per-interface
broadcast, verbs `ALIVE`, `CHAT`, `GAME`, `QUIT`; fields separated by `0x01`, messages by `0x02`,
UTF-8). **TCP 1233** = game-room connection to the host. **UDP 1234** = in-game port. In LAN games
spawn.ini has no `[Tunnel]` section. Each `[OtherN]` gets the real `Ip` and `Port=1234`, and the
spawner's `NetHack` maps them to player indices (`RequirePortMatch` when ports differ). This is
**not** the original in-game Westwood LAN lobby (`RA2MD.INI [Network]`, IPX-over-UDP broadcast,
serial check). It replaces that lobby but keeps gamemd's in-game protocol (N1).

Sources: [cncnet-server](https://github.com/CnCNet/cncnet-server) (`CnCNet/Net/Tunnel/TunnelV2.cs`,
`TunnelV3.cs`, `PeerToPeer/PeerToPeerUtil.cs`, `Options.cs`, `Program.cs`);
[xna-cncnet-client](https://github.com/CnCNet/xna-cncnet-client) (`DXMainClient/Domain/Multiplayer/CnCNet/V3GameTunnelBridge.cs`,
`V3TunnelCommunicator.cs`, `TunnelMode.cs`, `TunnelNegotiationCommands.cs`, `StunHelper.cs`,
`CnCNetTunnel.cs`, `DXGUI/Multiplayer/LANLobby.cs`, `LANGameLobby.cs`, `ClientCore/ProgramConstants.cs`,
`ClientCore/ClientConfiguration.cs`); [master list](https://cncnet.org/master-list);
[docker image of the server](https://hub.docker.com/r/efeoge/cncnet_server);
[V3 tunnel funding page](https://opencollective.com/cncnet/projects/xna-cncnet-client-v3-tunnels).

---

## 4. What a third-party client must implement to join a real CnCNet YR game

### 4.1 Out-of-game (lobby/transport) layer: well specified

1. **IRC client** (plain TCP 6667, optionally TLS 6697) to GameSurge. It must handle `USER`/`NICK`
   as in §2.2, join `#cncnet`, `#cncnet-yr` and `#cncnet-yr-games`, parse CTCP (`\x01...\x01`),
   and handle mIRC colours, channel modes `+k/+l`, WHO, kicks and flood control. The XNA client
   rate-limits with a priority queue, and GameSurge will kill flooders.
2. **Game list parser** for `GAME` (§2.3). We must present the **same `gameVersion`** as the current
   YR package, or real clients mark us incompatible and refuse to join, or simply ignore our options.
3. **Game-room state machine** with all verbs from §2.4, including bit-exact `PO`/`GO` packing. This
   needs a copy of the current package's `GameLobbyBase.ini`/`GameOptions.ini` option order, the
   `MPMaps.ini` game modes, and the map SHA-1 set. Map download via mapdb2 is required for custom
   maps.
4. **Tunnel client:** master-list fetch, 50-byte ping, V2 `/request` (if we host) and the V2/V3
   headers. For V3Dynamic we also need the `CNCNET`-magic negotiation protocol and keepalive/probe
   packets. A minimum viable joiner could work only with hosts who picked a static V2/V3 tunnel and
   skip dynamic negotiation, but newer clients default to dynamic.
5. **spawn.ini-equivalent bootstrap:** derive player order, colours, sides, start spots, seed,
   alliances, AI slots and spectators from `PO`/`GO`/`STARTVx` exactly as `GameLobbyBase.WriteSpawnIni`
   does. Then merge `spawnmap.ini` (map + `Map Code/<mode>.ini` + option INIs) the way the client
   does. That includes its `Include`/`Inheritance` INI features (Ares/Phobos INI extensions
   triggered by `-Include -Inheritance`).

### 4.2 In-game layer: N1 + N3 territory, with CnCNet specifics

6. **gamemd's UDP game protocol** (N1): IPXManager/`UDPInterfaceClass` framing, connection
   handshake, `EventClass` packets, frame-sync CRCs, the `ReconnectTimeout` "waiting for players"
   behaviour, and chat events. It runs **through the spawner's 4-byte header** and the
   `NodeNameType` fake addressing (`sin_addr = player index`).
7. **ProtocolZero** (`Protocol=0`, `FrameSendRate=2`, event `0x30 ResponseTime2`, latency-level
   table). Without it our client cannot even agree on MaxAhead with real peers.
8. **Extension events** `0x40/0x41` (Phobos), `0x60/0x61` (Ares) and the spawner's event
   verification checks. We must parse them, and also issue them when the user does the equivalent
   action.
9. **Bit-exact simulation** of **YR 1.001 + Ares 3.0p1 + Phobos 0.4 + spawner desync fixes +
   CnCNet rules/asset overlays** (`cncnet.mix`, `ra2mode` pack, option INIs), plus the YR RNG and
   `Seed`. Any divergence shows up as a sync error, and there is no resync in gamemd: the game just
   ends with a desync. **This is N3's problem and dominates the effort.** It also moves: Phobos and
   the spawner have nightly builds, and the client auto-updates every user.
10. **Multiplayer save/load** (`spawnSG.ini`, `LoadSaveGame`) if we want CnCNet-style "reconnect".
    CnCNet has no join-in-progress or state transfer. Recovery means everyone saves, then everyone
    reloads the same save.

### 4.3 Anti-cheat and ladder constraints

* **`FHSH` file hash:** each joiner computes a SHA-1 over a fixed file list. For `ClientGameType=Ares`
  that is `Ares.dll, Ares.dll.inj, Ares.mix, Syringe.exe, cncnet5.dll, rulesmd.ini, artmd.ini,
  soundmd.ini, aimd.ini, shroud.shp`, plus client-INI-configured `FHCConfig.ini` extras and the
  map-code/option INIs. It broadcasts `FHSH <hash>`. A mismatch makes the host/peers mark the
  player (`MM <name>`, "modified files"), and the client re-hashes at start and sends `CD` if files
  changed during the session. **The check is self-reported.** A clone has no Ares.dll to hash, so it
  could only pass by echoing the hash a genuine install produces. That is functionally spoofing the
  integrity check, and CnCNet's rules call circumventing restrictions a separate offence. Being
  flagged doesn't block the game technically, but hosts routinely kick flagged players.
* **Hardened spawner / event verification:** CnCNet ships a "hardened" spawner build whose source is
  in a private submodule (`yrpp-spawner-private`). Its exact checks are unknown. Any in-game
  verification we fail to emulate would show up as a desync or a kick.
* **Ladder / Quick Match** (`Qt/CnCNetQM.exe`, version-4 matchmaking tunnels on :50002,
  `ladder.cncnet.org`, open-source `cncnet-ladder-api`, Laravel, GPLv3): QM requires a CnCNet
  account and token auth (`/api/v1/auth/token`). The QM server writes the spawn settings and the game
  writes `stats.dmp` (spawner `Statistics.cpp`) for upload via `POST /api/v1/result/:game/:player`.
  **A third-party client must never touch ladder/QM.** Results would be unverifiable, and this is
  the area where CnCNet enforces cheating rules most strictly.

Sources: [FileHashCalculator.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/Online/FileHashCalculator.cs),
[CnCNetGameLobby.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/DXGUI/Multiplayer/GameLobby/CnCNetGameLobby.cs),
[CnCNetLobby.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/DXGUI/Multiplayer/CnCNet/CnCNetLobby.cs),
[Connection.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/Online/Connection.cs),
[GameCollection.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/Domain/Multiplayer/CnCNet/GameCollection.cs),
[MapSharer.cs](https://github.com/CnCNet/xna-cncnet-client/blob/develop/DXMainClient/Domain/Multiplayer/CnCNet/MapSharer.cs),
[cncnet-ladder-api](https://github.com/CnCNet/cncnet-ladder-api) (`API.md`),
[ladder news](https://ladder.cncnet.org/news/red-alert-2-new-maps-ladder-announced),
[CnCNet 2023 wrap-up](https://forums.cncnet.org/topic/12712-cncnet-2023-wrap-up-happy-new-year).

---

## 5. Licences, terms and policy

| Component | Licence | Implication for us |
|---|---|---|
| cncnet-server (tunnel) | **GPL-3.0** | We may run it unmodified as a separate service with no effect on our engine's licence. If we *re-implement* the protocol from this documentation (it is tiny), there is no copyleft concern either. We just shouldn't paste the C# into our C++. |
| xna-cncnet-client | **GPL-3.0** | Reading it for protocol facts is fine. Porting code into our engine would make the engine GPL-3.0. Clean-room re-implementation from this spec avoids that. |
| yrpp-spawner | **GPL-3.0** (NetHack.cpp is **ISC** © Toni Spets) | Same. Its desync-fix list is a behavioural spec we may re-implement. |
| Phobos | **LGPL-3.0** per repo LICENSE (package README says GPLv3) | Behavioural reference only. |
| Ares | **BSD 4-clause** (advertising clause) | Behavioural reference. The source is public (Ares-Developers). |
| cncnet-ladder-api | **GPL-3.0**; CnCNet badges/logo explicitly "strictly forbidden" without consent | Don't use CnCNet branding/assets. |
| CnCNet community rules | Rules cover "all CnCNet services": game services, IRC channels, forums, Discord | They forbid cheating and the "unrestricted spreading or promotion of unauthorised third party tools that can be used to cheat", allow "authorised tools" as decided per game/mod staff, and impose "immediate and indefinite bans" for "any actions perceived as threats to CnCNet". **No clause addresses third-party game clients explicitly, either way.** |
| GameSurge AUP | Network-wide | "Abuse or blatant misuse … including … 'idler bots' may result in revocation." Per-IP connection limits are enforced administratively. A custom IRC client is normal on IRC. |
| EA / the game | RA2/YR are commercial EA titles; CnCNet's YR client requires an owned install (`RequiredFiles=ra2.mix,ra2md.mix,…`) | Our clone already requires user-supplied original data (decision U-1). There is no EA online service involved. Westwood Online is gone. |

**Can our clone connect to CnCNet servers?** Technically yes: IRC is open and tunnels need no
auth. Policy-wise it is **unaddressed**, and CnCNet staff have broad discretion ("perceived as
threats"). Joining public rooms with a client that (a) can't pass `FHSH` honestly and (b) may desync
other people's games would degrade their service, and would very likely be treated as an
unauthorised tool. **Recommendation: don't ship CnCNet-public connectivity without written approval
from CnCNet staff** (contact: `https://cncnet.org/discord`, `forums.cncnet.org`). If we pursue it,
propose a distinct client identity (our own `USER` realname/ident and our own `gameVersion`, never
an impersonated one) and stay out of ladder/QM.

**Is hosting our own compatible tunnel/lobby allowed and documented?** **Yes.**
* Tunnel: GPLv3 source, CLI flags documented in `Options.cs`, a community Docker image, forum
  threads on systemd setup, and `--nomaster` to stay off CnCNet's list. Self-hosted tunnels can
  also be announced to CnCNet's master list (that is how the community-run servers appear). A
  `--masterpw` exists to stop impersonation of official names.
* Lobby: the XNA client takes its IRC servers from `NetworkDefinitions.ini [IRCServers]` and its
  tunnel list from `CnCNetTunnelListURL`, so any ircd (InspIRCd/UnrealIRCd) plus a static
  master-list file makes a fully private CnCNet-protocol network. Mods already do this.

Sources: [CnCNet community guidelines](https://cncnet.org/community-guidelines-and-rules),
[GameSurge AUP](https://gamesurge.net/aup/), [cncnet-server LICENSE](https://github.com/CnCNet/cncnet-server),
[yrpp-spawner README/LICENSE](https://github.com/CnCNet/yrpp-spawner),
[cncnet-yr-client-package README](https://github.com/CnCNet/cncnet-yr-client-package),
[Phobos](https://github.com/Phobos-developers/Phobos), [Ares docs](https://ares-docs.readthedocs.io/zh-cn/latest/),
[systemd tunnel thread](https://forums.cncnet.org/topic/9836-cncnet-server-systemd-auto-start-service/),
[CnCNet on C&C wiki](https://cnc.fandom.com/wiki/CnCNet).

---

## 6. Mapping CnCNet onto our requested modes

| Our requirement | CnCNet equivalent | Notes for our design |
|---|---|---|
| Skirmish maps 1–8 players | 8 slots (`[Other1..7]`, `clients` 2–8 on V2) | Same cap: gamemd has 8 houses. |
| Co-op vs AI | AI slots via `[HouseCountries/Colors/Handicaps]`. Co-op mode = `Map Code/Cooperative.ini`. QM co-op AI exists. | Pure lobby configuration. |
| Chat | Lobby: IRC PRIVMSG. In game: gamemd chat events (can be disabled with `DisableChat`). | Our own stack can do both over one connection. |
| Reconnect | gamemd `ReconnectTimeout` wait dialog plus V3 endpoint re-binding after NAT timeout. No join-in-progress. | Real resync needs our own design (N4). CnCNet's fallback is multiplayer save then reload (`spawnSG.ini`). |
| Resync | **None.** A desync ends the game (the spawner's whole desync-fix file exists because of this). | Our engine can do better (state hash + snapshot transfer) only on clone-only networks. |
| LAN | XNA LAN lobby (UDP 1232 broadcast / TCP 1233 / UDP 1234) or the original in-game IPX-over-UDP LAN. | For clone-only LAN, copy the *shape*: broadcast discovery, TCP room, UDP game. |
| Internet via user-hosted relay | Exactly what cncnet-server V3 is. | We can run cncnet-server itself, or a 200-line re-implementation, as our relay. Adopting the V3 header and ping format costs nothing and keeps the door open. |

---

## 7. What compatibility would concretely require

**Level A: "Clone-to-clone on a CnCNet-style stack" (recommended).**
Implement a lobby on any IRC server or our own small service, the V3 tunnel header/ping/registration
(§3.2), optional STUN 26262/XOR-0x20 discovery, and LAN broadcast discovery. Self-host
`cncnet-server --nomaster` (or our re-implementation) on the user's box/VPS. No dependency on
CnCNet's policy or on gamemd bit-exactness. **Effort: ~2–3 engineer-weeks** for the networking
pieces, on top of N4's lockstep work.

**Level B: "See and enter real CnCNet YR rooms" (lobby interop only).**
Level A plus the GameSurge IRC client, `GAME`/`PO`/`GO`/`STARTV2/3`/`MAPREQ` verbs with bit-exact
packing, a mirrored copy of the current YR package's option tables + `MPMaps.ini` + map SHA-1s,
mapdb2 download, V2 and V3Static, and V3Dynamic negotiation (`CNCNET` magic packets, NEGRPT). It
also needs continuous tracking of CnCNet client releases, because `gameVersion` and option layouts
change with each package. **Effort: ~3–5 engineer-weeks**, plus ongoing maintenance. **Blocked on
policy**: we can't present an honest `FHSH` or `gameVersion`, so written approval from CnCNet is
needed before any public use.

**Level C: "Actually play a match with original-game players."**
Level B plus everything in §4.2: gamemd's UDP/IPX in-game protocol (N1), the spawner tunnel
framing and NodeName addressing, ProtocolZero + event 0x30, Ares/Phobos events, the spawner event
verification, and **bit-exact lockstep simulation of YR 1.001 + Ares 3.0p1 + Phobos 0.4 +
spawner desync fixes + CnCNet INI/mix overlays** for every unit, weapon, AI and pathfinding path
that a 1–8 player game can exercise, including the RNG call order. Our engine is a from-scratch
re-implementation calibrated against Ghidra and YRpp for *behaviour*, not *bit-for-bit state*. Every
`Ares`/`Phobos` bugfix that touches gameplay would also have to be re-implemented, and kept current
with nightly upstream changes. **Effort: very large and open-ended. Realistically 1–3+ person-years
of RE plus a differential-testing harness that runs real gamemd+Ares+Phobos headless and compares
per-frame CRCs**, with a high risk of never fully converging. Even then, the anti-cheat and policy
issues of Level B remain.

### Biggest risks
1. **Target is no longer vanilla YR.** CnCNet YR = YR 1.001 + Ares 3.0p1 + Phobos 0.4 + spawner
   fixes + overlays (since Nov 2025), and it updates continuously. Bit-exactness against that is the
   dominant cost, and it is a moving target (N3).
2. **Policy:** there is no explicit third-party-client policy, and staff have broad "threat" ban
   discretion. The `FHSH` integrity check can only be passed by spoofing. Ladder/QM must be off
   limits.
3. **Version coupling:** `gameVersion` equality is enforced (`DisallowJoiningIncompatibleGames`),
   `GO`/`PO` are positional on package option tables, and official maps are matched by SHA-1. Every
   CnCNet release can break a third-party lobby.
4. **Closed piece:** the hardened spawner (`yrpp-spawner-private`) has unknown in-game checks.
5. **Infrastructure dependency:** the lobby rides the public GameSurge IRC network (third-party AUP,
   per-IP limits), and mapdb2/master-list are CnCNet-operated HTTP endpoints.
6. **Security:** tunnels are unauthenticated and unencrypted with guessable-ish IDs. That is fine for
   a game, but our own stack should add a session token if we expose it publicly.

### Recommendation
Build Level A, and wire-format-align our relay with cncnet-server V3 (8-byte LE header, 50→12-byte
ping, optional `--nomaster`-style private operation) so users can reuse existing tunnel software.
Treat Level B/C original-game interop as a **gated stretch goal**. Revisit it only if (1) N3 shows a
credible path to bit-exact parity with gamemd+Ares+Phobos via a differential harness, and (2) CnCNet
staff agree in writing to a distinct, honestly identified third-party client in casual
(non-ladder) rooms.

---

## 8. Open questions for other agents

* **N1:** confirm `D:\Red Alert 2\gamemd.exe` (SHA-1 above, version resource 1.05) is the YR 1.001
  image YRpp addresses; document UDPInterfaceClass/IPXManager framing under the spawner's 4-byte
  tunnel header; document `ProtocolVersion=2` vs vanilla behaviour.
* **N3:** size the delta of Ares 3.0p1 + Phobos 0.4 + `Bugfixes.Desyncs.cpp` gameplay changes; decide
  whether a headless gamemd differential harness is feasible.
* **N4/N5:** adopt the V3 header and ping (or justify not doing so). Decide IRC vs a custom WS lobby
  for our own stack.

## 9. Source index

* https://github.com/CnCNet/cncnet-server — TunnelV2.cs, TunnelV3.cs, PeerToPeerUtil.cs, Options.cs, Program.cs, LICENSE (GPL-3.0)
* https://github.com/CnCNet/yrpp-spawner — src/Spawner/{Spawner.cpp,Spawner.Config.*,NetHack.*,ProtocolZero*.cpp,QuickMatch.cpp,Statistics.cpp}, src/Misc/{Bugfixes.Desyncs.cpp,CopyProtection.cpp}, src/Ext/Event/Body.*, README.md, .gitmodules
* https://github.com/CnCNet/xna-cncnet-client — DXMainClient/Online/{Connection.cs,CnCNetManager.cs,FileHashCalculator.cs}, DXGUI/Multiplayer/CnCNet/CnCNetLobby.cs, DXGUI/Multiplayer/GameLobby/{CnCNetGameLobby.cs,GameLobbyBase.cs,MultiplayerGameLobby.cs,LANGameLobby.cs}, DXGUI/Multiplayer/{LANLobby.cs,LANLobbyBroadcastManager.cs}, Domain/Multiplayer/CnCNet/{V3GameTunnelBridge.cs,V3TunnelCommunicator.cs,TunnelHandler.cs,CnCNetTunnel.cs,TunnelMode.cs,TunnelNegotiationCommands.cs,StunHelper.cs,MapSharer.cs,GameCollection.cs}, ClientCore/{ProgramConstants.cs,ClientConfiguration.cs}
* https://github.com/CnCNet/cncnet-yr-client-package — package/Resources/{ClientDefinitions.ini,GameOptions.ini,NetworkDefinitions.ini}, package/INI/Game Options/Yuri Rebalance Patch.ini, ForcedOptions.md, README.md (file tree for Ares.dll/Phobos.dll/CnCNet-Spawner.dll/Syringe.exe/Qt/CnCNetQM.exe)
* https://github.com/Phobos-developers/Phobos/blob/develop/src/Ext/Event/Body.h
* https://github.com/CnCNet/cncnet-ladder-api (API.md)
* https://cncnet.org/master-list (live, fetched 2026-10-10)
* https://api.cncnet.org/status (live, fetched 2026-10-10)
* https://forums.cncnet.org/topic/13131-yuris-revenge-90-phobosares/
* https://forums.cncnet.org/topic/12712-cncnet-2023-wrap-up-happy-new-year
* https://forums.cncnet.org/topic/9836-cncnet-server-systemd-auto-start-service/
* https://ladder.cncnet.org/news/red-alert-2-new-maps-ladder-announced
* https://cncnet.org/community-guidelines-and-rules
* https://gamesurge.net/aup/
* https://ares-docs.readthedocs.io/zh-cn/latest/
* https://hub.docker.com/r/efeoge/cncnet_server
* https://opencollective.com/cncnet/projects/xna-cncnet-client-v3-tunnels
* https://cnc.fandom.com/wiki/CnCNet
