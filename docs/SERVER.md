# Hosting your own online server (`ra2lobbyd`)

`ra2lobbyd` is the small server that makes internet play work. You host it;
nobody else's infrastructure is involved. It is one static binary (about 7 MB),
keeps everything in memory, has no accounts and no database, and costs
between EUR 0 and EUR 4 a month to run.

It does four things:

1. **Version gate.** A game whose network protocol is too old or too new is
   turned away with a "please update" message. A player whose rules data
   differs from the host's cannot join that room, because different
   `rulesmd.ini` data would desync the match.
2. **Room directory and room codes.** A host creates a room. The server
   gives it a 6-character code such as `K7Q2MX` (look-alike characters
   I/L/O/U/0/1 are left out). Public rooms appear in the in-game list.
   Private rooms can only be joined by typing the code.
3. **Address exchange.** When a player joins, the server tells host and
   guest each other's tunnel ids. It never gives out anyone's IP address.
   Players only ever see the server's address.
4. **UDP relay.** Game traffic goes through the server, so it works behind
   home routers, carrier-grade NAT on mobile data, and IPv6-only phones
   (464XLAT).
   * **CnCNet-compatible tunnel** (V3 on UDP 50001, V2 on UDP+HTTP 50000).
     This is what the game uses. It is a clean-room re-implementation of the
     CnCNet tunnel wire protocol: V3 has an 8-byte header with two
     little-endian u32 ids, V2 has a 4-byte header with two big-endian i16
     ids plus HTTP `/request?clients=N`, and both answer a 50-byte ping with
     12 bytes. So stock CnCNet tunnel clients can use your server too. It
     never announces itself to CnCNet's master list.
   * **Per-client port relay** (UDP 50100-50999). Each relayed client gets
     its own port that forwards raw datagrams between that client and the
     host, with no header. It is used by transports that cannot add a
     tunnel header.

Rooms, slots, maps, colours, chat and the match itself all run in the host
player's game. The server never sees game data.

## Ports

| Port | Protocol | Purpose | Must be open |
|---|---|---|---|
| 50000 | TCP | control plane (rooms, codes) + HTTP (`/request`, `/status`, `/healthz`) | yes |
| 50000 | UDP | CnCNet tunnel V2 | yes (needed for stock CnCNet V2 clients) |
| 50001 | UDP | CnCNet tunnel V3 (the game's relay) | **yes** |
| 50100-50999 | UDP | per-client port relay | yes, unless started with `--relay-ports ""` |
| 127.0.0.1:8080 | TCP | `/healthz`, `/metrics` | **no**: localhost only, never open it |
| 22 | TCP | SSH, for you | yes |

## Cost and sizing

The game uses lockstep: players exchange orders, not world state. A relayed
8-player match therefore uses about 0.25 Mbit/s in total, or roughly
115 MB per game-hour in the worst case. CPU and memory are negligible:
about 15 MB RSS when idle and about 2 KB per room. Any 1 vCPU / 512 MB
machine is more than enough.

| Host | Plan | Price (Oct 2026) | Traffic included | Notes |
|---|---|---|---|---|
| **Hetzner Cloud** | CX22 (2 vCPU, 4 GB) | about EUR 3.99/month | 20 TB/month (about 170,000 relayed 8-player game-hours) | **Recommended.** Predictable price, public IPv4, simple firewall. EU + US regions. |
| **Oracle Cloud Always Free** | Ampere A1 (arm64), 2 OCPU / 12 GB | $0 | 10 TB/month | Free. Needs ports opened in **two** places (VCN security list + the instance's iptables). Oracle may reclaim idle free instances. |
| DigitalOcean / Vultr / Linode | smallest VM | $4-6/month | 0.5-1 TB | Fine. |
| Fly.io / Render / Railway / Cloudflare | - | - | - | Poor fit: no inbound UDP, or no UDP port ranges. |
| Home PC / Raspberry Pi | - | electricity | your ISP | Only works if your ISP gives you a public IPv4 (no CGNAT). Use a DDNS name. |

## Option A: Hetzner Cloud CX22 (recommended)

1. **Create the server.** In the Hetzner Cloud console, choose
   *Add Server*. Pick a location near your players, image **Ubuntu 24.04**,
   type **CX22**, and add your SSH key. Note the public IPv4, for example
   `203.0.113.10`.
2. **Firewall (cloud).** Go to *Firewalls*, then *Create Firewall*, and add
   these inbound rules:
   * TCP 22 (you can restrict it to your own IP)
   * TCP 50000
   * UDP 50000
   * UDP 50001
   * UDP 50100-50999

   Apply the firewall to the server. Leave outbound open.
3. **Firewall (host)**, optional, as defence in depth:
   ```sh
   ssh root@203.0.113.10
   ufw allow 22/tcp
   ufw allow 50000/tcp
   ufw allow 50000/udp
   ufw allow 50001/udp
   ufw allow 50100:50999/udp
   ufw enable
   ```
4. **Install the binary.** On your Windows machine, in the repo:
   ```powershell
   cd server
   .\build.ps1                       # dist\ra2lobbyd-linux-amd64 (+ arm64, .exe)
   scp dist\ra2lobbyd-linux-amd64 root@203.0.113.10:/usr/local/bin/ra2lobbyd
   scp deploy\ra2lobbyd.service   root@203.0.113.10:/etc/systemd/system/
   ```
   Then on the server:
   ```sh
   chmod 0755 /usr/local/bin/ra2lobbyd
   # edit the server name / MOTD in the ExecStart= line if you like:
   nano /etc/systemd/system/ra2lobbyd.service
   systemctl daemon-reload
   systemctl enable --now ra2lobbyd
   journalctl -u ra2lobbyd -f        # "ra2lobbyd 1.0.0: control+V2 :50000, V3 :50001 ..."
   ```
5. **Check it.**
   ```sh
   curl -s localhost:8080/healthz     # ok rooms=0
   curl -s localhost:50000/status     # 200 slots free. / 0 slots in use.
   ```
   From your PC: `curl http://203.0.113.10:50000/healthz`.
6. **Optional DNS name.** Add an A record such as `ra2.example.com`
   pointing to the IPv4. DuckDNS is free. Players can then type the name
   instead of the IP.
7. **In the game.** Open *Main Menu*, then *Internet*. Put
   `203.0.113.10` (or the DNS name) in the server field and press
   *Connect*. The setting is saved in `RA2.INI`:
   `[Net] Server=203.0.113.10:50000`. Port 50000 is the default and can be
   left out.

## Option B: Oracle Cloud Always Free (EUR 0)

1. Create an *Always Free* account. Go to *Compute*, then *Instances*,
   then *Create instance*. Choose image **Ubuntu 24.04** and shape
   **VM.Standard.A1.Flex** (Ampere, arm64). 1 OCPU / 6 GB is plenty.
   Add your SSH key.
2. **Open the ports in the VCN.** Go to *Networking*, then *Virtual
   cloud networks*, then your VCN, then *Security Lists*, then *Default
   Security List*, then *Add Ingress Rules*. Use source `0.0.0.0/0` for
   each of these rules:
   * TCP 50000
   * UDP 50000
   * UDP 50001
   * UDP 50100-50999
3. **Open the ports on the instance too.** Oracle's Ubuntu images ship
   iptables rules that reject everything except SSH:
   ```sh
   sudo iptables -I INPUT 6 -p tcp --dport 50000 -j ACCEPT
   sudo iptables -I INPUT 6 -p udp --dport 50000:50001 -j ACCEPT
   sudo iptables -I INPUT 6 -p udp --dport 50100:50999 -j ACCEPT
   sudo netfilter-persistent save
   ```
4. **Install the binary.** Use the arm64 build:
   ```powershell
   scp server\dist\ra2lobbyd-linux-arm64 ubuntu@<ip>:/tmp/ra2lobbyd
   scp server\deploy\ra2lobbyd.service   ubuntu@<ip>:/tmp/
   ```
   ```sh
   sudo install -m 0755 /tmp/ra2lobbyd /usr/local/bin/ra2lobbyd
   sudo install -m 0644 /tmp/ra2lobbyd.service /etc/systemd/system/
   sudo systemctl daemon-reload && sudo systemctl enable --now ra2lobbyd
   ```
5. Oracle instances sit behind 1:1 NAT. The instance only sees its private
   address. That is fine, because clients use the address they dialled. If
   you put a DNS name in front, add `Environment=RA2_PUBLIC_IP=<public ip>`
   to the unit so the advertised relay address is the public one.
6. Check as in Option A, step 5.

## Option C: Docker (any Linux VPS)

```sh
curl -fsSL https://get.docker.com | sh
git clone <your repo> && cd <repo>/server
docker compose up -d --build      # host networking, read-only, non-root, restart unless-stopped
docker compose logs -f
```
The image is `FROM scratch` and contains only the binary, running as
`nobody`. The compose file uses `network_mode: host`, so the 900 relay ports
are not NATed by Docker. Open the same firewall ports as above.

## Configuration

Every flag also has an environment-variable form.

| Flag | Env | Default | Meaning |
|---|---|---|---|
| `--listen` | `RA2_LISTEN` | `:50000` | control TCP + V2 tunnel UDP |
| `--v3` | `RA2_V3` | `:50001` | V3 tunnel UDP (`""` = off) |
| `--relay-ports` | `RA2_RELAY_PORTS` | `50100-50999` | per-client relay range (`""` = off) |
| `--public-ip` | `RA2_PUBLIC_IP` | (dialled address) | IPv4 advertised to clients |
| `--server-name` | `RA2_SERVER_NAME` | `RA2 server` | shown in the game |
| `--motd` | `RA2_MOTD` | | message of the day |
| `--admin` | `RA2_ADMIN` | `127.0.0.1:8080` | `/healthz` + `/metrics` (Prometheus text) |
| `--max-rooms` | `RA2_MAX_ROOMS` | 500 | |
| `--max-rooms-per-ip` | `RA2_MAX_ROOMS_PER_IP` | 3 | |
| `--max-conns-per-ip` | `RA2_MAX_CONNS_PER_IP` | 5 | control connections |
| `--tunnel-max-clients` | `RA2_TUNNEL_MAX_CLIENTS` | 200 | tunnel ids, V2 and V3 each |
| `--tunnel-ip-limit` | `RA2_TUNNEL_IP_LIMIT` | 8 | V3 ids per IP |
| `--ban-file` | `RA2_BAN_FILE` | | `{"ips":["1.2.3.4"],"pids":["0123456789abcdef"]}`, re-read when changed |
| `--maint-password` | `RA2_MAINT_PASSWORD` | | CnCNet V3 maintenance toggle |
| `--allow-loopback` | `RA2_ALLOW_LOOPBACK=1` | off | accept tunnel traffic from 127.0.0.1 (testing on one PC) |

## Abuse protection (built in)

* `hello` must come first, with the right magic and protocol range. Anything
  else is closed after one message.
* Per IP: 5 control connections, 3 hosted rooms, 10 join attempts per
  minute (so room codes cannot be brute-forced), and 10 relay ports.
* Per connection: 20 messages/s (burst 50), at most 4 KB per message, and a
  60 s idle timeout. Clients ping every 10 s.
* Tunnels: 20 pings per IP per minute (5000 globally), at most 1024-byte
  datagrams, and no forwarding from loopback, any, broadcast or port 0
  sources. A live id cannot be taken over from another address until it has
  been idle for 30 s.
* Port relay: only the two endpoints that proved their 128-bit tokens get
  traffic. Anything else is dropped without a reply, so the relay cannot be
  used as a reflector or amplifier. Limits are 64 KB/s and 200 packets/s per
  direction, and a port is released after 60 s idle.
* Nicknames and room names must be printable, with no control characters
  and no bidi overrides. Nicknames are 1-16 characters, room names 1-32.

## Privacy

* No accounts, e-mail addresses, passwords or analytics. A player is a
  nickname plus a random 64-bit id that the game generates once per install
  (`RA2.INI [Net] PlayerId`). The player can regenerate it at any time by
  deleting the key.
* The server logs room creation and joins with **truncated** addresses only
  (`203.0.113.0/24`).
* Players never learn each other's IP addresses online. All traffic goes
  through the relay.
* Chat and game orders are **not encrypted** in this version. Tell your
  players. If you publish the game with your server built in (for example
  on Google Play), you need a short privacy policy: "nicknames and IP
  addresses are processed by <your server> to run multiplayer games and are
  not stored".

## Updating

Replace the binary and run `systemctl restart ra2lobbyd`, or
`docker compose up -d --build`. Rooms are in memory, so open rooms disappear
and hosts simply create a new one. The server accepts a range of game
network-protocol versions (`proto_min..proto_max`). Older games get a clear
"please update" message.

## Testing on one PC

```powershell
server\dist\ra2lobbyd.exe --allow-loopback --relay-ports 50100-50199
# then run two game instances and use server address 127.0.0.1
```
`scripts/play/mp-online-loopback.ps1` does this automatically. It starts
the server, a hosting game and a joining game, and checks that the join
went through the V3 relay.

## Control-plane protocol (for client authors)

The protocol is JSON lines over TCP 50000: one JSON object per line, at
most 4 KB, with the message type in the `t` field. The canonical
description is in `server/internal/lobby/proto.go`. In short:

```
C->S {"t":"hello","magic":"RA2N","proto":1,"ver":"0.9.0","sim":1,"rules":"89abcdef","plat":1,"pid":"<16 hex>","nick":"Tanya"}
S->C {"t":"hello_ok","server":"...","motd":"...","proto_min":1,"proto_max":1,"v3_port":50001,"v2_port":50000}
C->S {"t":"list"}                         S->C {"t":"rooms","rooms":[{"code":"K7Q2MX","name":"...","host":"...","map":"...","players":2,"max":4,...}]}
C->S {"t":"create","name":"Friday","max":4,"private":false,"map":"Tour of Egypt","v3":2864434397}
S->C {"t":"created","code":"K7Q2MX","v3_port":50001}
C->S {"t":"join","code":"K7Q2MX","v3":3149642683,"relay":"v3"}
S->C {"t":"joined","code":"K7Q2MX","cid":1,"host_nick":"Tanya","host_v3":2864434397,"v3_port":50001}
S->C {"t":"peer","cid":1,"nick":"Boris","pid":"...","v3":3149642683}         (to the host)
C->S {"t":"update","players":2,"max":4,"map":"...","started":false}
C->S {"t":"ping"}  S->C {"t":"pong"}      every 10 s
```
After `joined`/`peer`, host and guest send UDP to `server:v3_port` with the
CnCNet V3 header `[u32 sender LE][u32 receiver LE]`. The payload is the
in-room lobby, which uses the original game's 0x1236 global-channel framing
(see `engine/mp/lobby_wire.hpp`), and later the game itself.
