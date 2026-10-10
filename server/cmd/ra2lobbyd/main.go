// ra2lobbyd: the small, self-hostable server for the RA2 clone's online play.
//
//	TCP  :50000         control plane (room directory, codes, version gate,
//	                    address exchange) + HTTP (CnCNet V2 /request, /status,
//	                    /healthz) on the same port
//	UDP  :50000         CnCNet tunnel V2 relay
//	UDP  :50001         CnCNet tunnel V3 relay (what the game uses)
//	UDP  50100-50999    per-client port relay (header-less, TURN-style)
//	TCP  127.0.0.1:8080 /healthz + /metrics (never expose publicly)
//
// No accounts, no database: everything is in memory. See docs/SERVER.md.
package main

import (
	"crypto/rand"
	"encoding/binary"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"ra2lobbyd/internal/lobby"
	"ra2lobbyd/internal/relay"
	"ra2lobbyd/internal/tunnel"
)

var version = "1.0.0"

func env(name, def string) string {
	if v := os.Getenv(name); v != "" {
		return v
	}
	return def
}

func envInt(name string, def int) int {
	if v := os.Getenv(name); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return def
}

func main() {
	listen := flag.String("listen", env("RA2_LISTEN", ":50000"), "control plane TCP + V2 tunnel UDP address")
	v3 := flag.String("v3", env("RA2_V3", ":50001"), "CnCNet tunnel V3 UDP address (\"\" = off)")
	relayPorts := flag.String("relay-ports", env("RA2_RELAY_PORTS", "50100-50999"), "per-client relay port range (\"\" = off)")
	publicIP := flag.String("public-ip", env("RA2_PUBLIC_IP", ""), "IPv4 advertised to clients (default: the address they dialled)")
	name := flag.String("server-name", env("RA2_SERVER_NAME", "RA2 server"), "name shown in the game")
	motd := flag.String("motd", env("RA2_MOTD", ""), "message of the day")
	admin := flag.String("admin", env("RA2_ADMIN", "127.0.0.1:8080"), "health/metrics HTTP address (\"\" = off)")
	maxRooms := flag.Int("max-rooms", envInt("RA2_MAX_ROOMS", 500), "maximum open rooms")
	roomsPerIP := flag.Int("max-rooms-per-ip", envInt("RA2_MAX_ROOMS_PER_IP", 3), "rooms one IP may host")
	connsPerIP := flag.Int("max-conns-per-ip", envInt("RA2_MAX_CONNS_PER_IP", 5), "control connections per IP")
	maxClients := flag.Int("tunnel-max-clients", envInt("RA2_TUNNEL_MAX_CLIENTS", 200), "tunnel ids (V2 and V3 each)")
	ipLimit := flag.Int("tunnel-ip-limit", envInt("RA2_TUNNEL_IP_LIMIT", 8), "V3 tunnel ids per IP")
	banFile := flag.String("ban-file", env("RA2_BAN_FILE", ""), "bans.json (re-read when it changes)")
	maint := flag.String("maint-password", env("RA2_MAINT_PASSWORD", ""), "CnCNet V3 maintenance password (\"\" = off)")
	allowLoop := flag.Bool("allow-loopback", os.Getenv("RA2_ALLOW_LOOPBACK") == "1", "accept tunnel traffic from 127.0.0.1 (local testing)")
	showVer := flag.Bool("version", false, "print the version and exit")
	flag.Parse()
	if *showVer {
		fmt.Println("ra2lobbyd", version)
		return
	}
	logger := log.New(os.Stderr, "", log.LstdFlags)

	tcfg := tunnel.Config{MaxClients: *maxClients, IPLimit: *ipLimit, AllowLoopback: *allowLoop,
		MaintPassword: *maint}
	v2 := tunnel.NewV2(tcfg, func() uint16 {
		var b [2]byte
		rand.Read(b[:])
		return binary.LittleEndian.Uint16(b[:])
	})
	_, lport, err := net.SplitHostPort(*listen)
	if err != nil {
		logger.Fatalf("bad --listen %q: %v", *listen, err)
	}
	v2port, _ := strconv.Atoi(lport)

	// V2 tunnel UDP on the control port number.
	v2conn, err := net.ListenUDP("udp", mustUDP(*listen))
	if err != nil {
		logger.Fatalf("V2 tunnel UDP %s: %v", *listen, err)
	}
	go tunnel.Serve(v2conn, v2, 10*time.Second)

	v3port := 0
	var v3t *tunnel.V3
	if *v3 != "" {
		v3t = tunnel.NewV3(tcfg)
		c, err := net.ListenUDP("udp", mustUDP(*v3))
		if err != nil {
			logger.Fatalf("V3 tunnel UDP %s: %v", *v3, err)
		}
		v3port = c.LocalAddr().(*net.UDPAddr).Port
		go tunnel.Serve(c, v3t, 10*time.Second)
	}

	var pool *relay.Pool
	if *relayPorts != "" {
		lo, hi, err := parseRange(*relayPorts)
		if err != nil {
			logger.Fatalf("bad --relay-ports: %v", err)
		}
		pool = relay.NewPool(relay.Config{PortLo: lo, PortHi: hi})
	}

	srv := lobby.New(lobby.Config{ServerName: *name, MOTD: *motd, PublicIP: *publicIP, V3Port: v3port,
		V2Port: v2port, MaxRooms: *maxRooms, MaxRoomsPerIP: *roomsPerIP, MaxConnsPerIP: *connsPerIP,
		Bans: lobby.LoadBans(*banFile), Logger: logger})
	srv.Pool = pool
	srv.V2 = v2

	ln, err := net.Listen("tcp", *listen)
	if err != nil {
		logger.Fatalf("control TCP %s: %v", *listen, err)
	}

	if *admin != "" {
		go serveAdmin(*admin, srv, v2, v3t, pool, logger)
	}
	logger.Printf("ra2lobbyd %s: control+V2 %s, V3 %s, relay %s, server name %q", version, *listen, *v3, *relayPorts, *name)

	go func() {
		sig := make(chan os.Signal, 1)
		signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
		<-sig
		logger.Printf("shutting down")
		ln.Close()
	}()
	if err := srv.Serve(ln); err != nil && !strings.Contains(err.Error(), "closed") {
		logger.Fatal(err)
	}
}

func mustUDP(a string) *net.UDPAddr {
	u, err := net.ResolveUDPAddr("udp", a)
	if err != nil {
		log.Fatalf("bad UDP address %q: %v", a, err)
	}
	return u
}

func parseRange(s string) (int, int, error) {
	parts := strings.SplitN(s, "-", 2)
	if len(parts) != 2 {
		return 0, 0, fmt.Errorf("want LO-HI, got %q", s)
	}
	lo, err1 := strconv.Atoi(parts[0])
	hi, err2 := strconv.Atoi(parts[1])
	if err1 != nil || err2 != nil || lo < 1024 || hi > 65535 || hi < lo {
		return 0, 0, fmt.Errorf("bad range %q", s)
	}
	return lo, hi, nil
}

func serveAdmin(addr string, srv *lobby.Server, v2 *tunnel.V2, v3 *tunnel.V3, pool *relay.Pool, logger *log.Logger) {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "ok rooms=%d\n", srv.RoomCount())
	})
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		m := &srv.Metrics
		fmt.Fprintf(w, "ra2lobbyd_rooms %d\n", srv.RoomCount())
		fmt.Fprintf(w, "ra2lobbyd_connections_total %d\n", m.Conns.Load())
		fmt.Fprintf(w, "ra2lobbyd_hellos_total %d\n", m.Hellos.Load())
		fmt.Fprintf(w, "ra2lobbyd_rejects_total %d\n", m.Rejects.Load())
		fmt.Fprintf(w, "ra2lobbyd_rooms_created_total %d\n", m.RoomsCreated.Load())
		fmt.Fprintf(w, "ra2lobbyd_joins_total %d\n", m.Joins.Load())
		fmt.Fprintf(w, "ra2lobbyd_join_failures_total %d\n", m.JoinFails.Load())
		free, used := v2.Slots()
		fmt.Fprintf(w, "ra2lobbyd_v2_slots_used %d\nra2lobbyd_v2_slots_free %d\n", used, free)
		fmt.Fprintf(w, "ra2lobbyd_v2_forwarded_total %d\n", v2.Stats.Forwarded.Load())
		if v3 != nil {
			fmt.Fprintf(w, "ra2lobbyd_v3_clients %d\n", v3.Clients())
			fmt.Fprintf(w, "ra2lobbyd_v3_forwarded_total %d\n", v3.Stats.Forwarded.Load())
			fmt.Fprintf(w, "ra2lobbyd_v3_dropped_total %d\n", v3.Stats.Dropped.Load())
			fmt.Fprintf(w, "ra2lobbyd_v3_bytes_total %d\n", v3.Stats.Bytes.Load())
			fmt.Fprintf(w, "ra2lobbyd_v3_pings_total %d\n", v3.Stats.Pings.Load())
		}
		if pool != nil {
			fmt.Fprintf(w, "ra2lobbyd_relay_allocations %d\n", pool.InUse())
			fmt.Fprintf(w, "ra2lobbyd_relay_allocations_total %d\n", pool.Total.Load())
		}
	})
	if err := http.ListenAndServe(addr, mux); err != nil {
		logger.Printf("admin listener %s: %v", addr, err)
	}
}
