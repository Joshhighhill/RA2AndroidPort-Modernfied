// Package tunnel is a clean-room re-implementation of the CnCNet tunnel
// server's two UDP relay protocols, written from the wire description in
// docs/gap/net/02-cncnet.md §3.2 (no CnCNet source was copied):
//
//	V3 (UDP, default port 50001):
//	  [u32 senderId LE][u32 receiverId LE][payload]  (min 8 bytes, max 1024)
//	  sender==0 && receiver==0 && len==50   -> ping: reply with the first 12 bytes
//	  sender==0 && receiver==0xFFFFFFFF     -> maintenance: [cmd u8][sha1(pw) 20]
//	  otherwise the first packet from an unknown senderId registers it to the
//	  source endpoint (MaxClients, IPLimit); a known id from a new endpoint is
//	  only re-bound once its old mapping timed out; the packet is forwarded
//	  verbatim (header included) to the endpoint registered for receiverId.
//
//	V2 (UDP + HTTP on the same port, default 50000):
//	  [i16 senderId BE][i16 receiverId BE][payload]
//	  ids are pre-allocated over HTTP GET /request?clients=N (2..8) -> [id,...]
//	  the first packet from an allocated id binds it; only allocated, bound ids
//	  are forwarded. Ping: same as V3 (50 bytes with ids 0/0 -> 12 bytes).
//
// Both sit next to our own room directory (package lobby): game clients of
// this project speak V3 through it, and stock CnCNet clients / spawners can
// use it as an ordinary private tunnel (it never announces to a master list).
package tunnel

import (
	"crypto/sha1"
	"encoding/binary"
	"errors"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"ra2lobbyd/internal/limit"
)

const (
	MaxPacket    = 1024 // CnCNet buffer size: larger datagrams are dropped
	PingSize     = 50   // a tunnel ping request
	PingReply    = 12   // ... answered with its first 12 bytes
	V3HeaderSize = 8
	V2HeaderSize = 4
)

// Stats are exported through /metrics.
type Stats struct {
	Forwarded atomic.Uint64
	Dropped   atomic.Uint64
	Pings     atomic.Uint64
	Bytes     atomic.Uint64
}

// Config for both tunnel flavours.
type Config struct {
	MaxClients    int           // CnCNet --maxclients (200)
	IPLimit       int           // V3 --iplimit (8): ids per source IP
	IPLimitV2     int           // V2 --iplimitv2 (4): /request calls per IP per minute
	Timeout       time.Duration // idle mapping timeout (re-bind allowed after it)
	PingPerIP     int           // pings per IP per minute (20)
	PingGlobal    int           // pings per minute, all IPs (5000)
	AllowLoopback bool          // CnCNet drops loopback senders; tests / LAN demos need them
	MaintPassword string        // V3 maintenance toggle; "" = disabled
	Now           limit.Clock
}

func (c *Config) defaults() {
	if c.MaxClients <= 0 {
		c.MaxClients = 200
	}
	if c.IPLimit <= 0 {
		c.IPLimit = 8
	}
	if c.IPLimitV2 <= 0 {
		c.IPLimitV2 = 4
	}
	if c.Timeout <= 0 {
		c.Timeout = 30 * time.Second
	}
	if c.PingPerIP <= 0 {
		c.PingPerIP = 20
	}
	if c.PingGlobal <= 0 {
		c.PingGlobal = 5000
	}
	if c.Now == nil {
		c.Now = time.Now
	}
}

// Out is one datagram the tunnel wants sent.
type Out struct {
	To   *net.UDPAddr
	Data []byte
}

// validSource rejects the addresses CnCNet refuses (any, broadcast, port 0,
// and loopback unless allowed).
func validSource(a *net.UDPAddr, allowLoop bool) bool {
	if a == nil || a.Port == 0 || a.IP == nil || a.IP.IsUnspecified() {
		return false
	}
	if a.IP.Equal(net.IPv4bcast) {
		return false
	}
	if a.IP.IsLoopback() && !allowLoop {
		return false
	}
	return true
}

func sameAddr(a, b *net.UDPAddr) bool {
	return a != nil && b != nil && a.Port == b.Port && a.IP.Equal(b.IP)
}

func cloneAddr(a *net.UDPAddr) *net.UDPAddr {
	ip := make(net.IP, len(a.IP))
	copy(ip, a.IP)
	return &net.UDPAddr{IP: ip, Port: a.Port, Zone: a.Zone}
}

type pinger struct {
	perIP  *limit.Window
	global *limit.Window
}

func newPinger(c *Config) pinger {
	return pinger{perIP: limit.NewWindow(c.PingPerIP, time.Minute, c.Now),
		global: limit.NewWindow(c.PingGlobal, time.Minute, c.Now)}
}

func (p pinger) allow(from *net.UDPAddr) bool {
	return p.perIP.Allow(from.IP.String()) && p.global.Allow("*")
}

// ---------------------------------------------------------------- V3 ----

type v3Client struct {
	addr     *net.UDPAddr
	lastSeen time.Time
}

// V3 is the CnCNet tunnel V3 state machine (transport-free: Handle returns
// what to send, Serve drives it on a socket).
type V3 struct {
	cfg         Config
	mu          sync.Mutex
	clients     map[uint32]*v3Client
	perIP       map[string]int
	maintenance bool
	ping        pinger
	Stats       Stats
}

func NewV3(cfg Config) *V3 {
	cfg.defaults()
	return &V3{cfg: cfg, clients: map[uint32]*v3Client{}, perIP: map[string]int{}, ping: newPinger(&cfg)}
}

// V3Header builds the 8-byte header (exported for tests and tools).
func V3Header(sender, receiver uint32) []byte {
	b := make([]byte, V3HeaderSize)
	binary.LittleEndian.PutUint32(b[0:], sender)
	binary.LittleEndian.PutUint32(b[4:], receiver)
	return b
}

// Maintenance reports whether new registrations are refused.
func (t *V3) Maintenance() bool { t.mu.Lock(); defer t.mu.Unlock(); return t.maintenance }

// Clients is the number of registered ids.
func (t *V3) Clients() int { t.mu.Lock(); defer t.mu.Unlock(); return len(t.clients) }

func (t *V3) Handle(pkt []byte, from *net.UDPAddr) []Out {
	n := len(pkt)
	if n < V3HeaderSize || n > MaxPacket || !validSource(from, t.cfg.AllowLoopback) {
		t.Stats.Dropped.Add(1)
		return nil
	}
	sender := binary.LittleEndian.Uint32(pkt[0:])
	receiver := binary.LittleEndian.Uint32(pkt[4:])
	if sender == 0 {
		if receiver == 0 && n == PingSize {
			if !t.ping.allow(from) {
				t.Stats.Dropped.Add(1)
				return nil
			}
			t.Stats.Pings.Add(1)
			r := make([]byte, PingReply)
			copy(r, pkt[:PingReply])
			return []Out{{To: cloneAddr(from), Data: r}}
		}
		if receiver == 0xFFFFFFFF && n >= V3HeaderSize+1+sha1.Size {
			t.maint(pkt[V3HeaderSize], pkt[V3HeaderSize+1:V3HeaderSize+1+sha1.Size])
		}
		t.Stats.Dropped.Add(1)
		return nil
	}
	if sender == receiver || receiver == 0 {
		t.Stats.Dropped.Add(1)
		return nil
	}
	now := t.cfg.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	c, ok := t.clients[sender]
	switch {
	case !ok:
		if t.maintenance || len(t.clients) >= t.cfg.MaxClients || t.perIP[from.IP.String()] >= t.cfg.IPLimit {
			t.Stats.Dropped.Add(1)
			return nil
		}
		c = &v3Client{addr: cloneAddr(from), lastSeen: now}
		t.clients[sender] = c
		t.perIP[from.IP.String()]++
	case !sameAddr(c.addr, from):
		// NAT re-binding: only once the old mapping has timed out.
		if now.Sub(c.lastSeen) < t.cfg.Timeout {
			t.Stats.Dropped.Add(1)
			return nil
		}
		t.release(c.addr)
		if t.perIP[from.IP.String()] >= t.cfg.IPLimit {
			delete(t.clients, sender)
			t.Stats.Dropped.Add(1)
			return nil
		}
		c.addr = cloneAddr(from)
		t.perIP[from.IP.String()]++
	}
	c.lastSeen = now
	dst, ok := t.clients[receiver]
	if !ok {
		t.Stats.Dropped.Add(1)
		return nil
	}
	out := make([]byte, n)
	copy(out, pkt)
	t.Stats.Forwarded.Add(1)
	t.Stats.Bytes.Add(uint64(n))
	return []Out{{To: dst.addr, Data: out}}
}

func (t *V3) release(a *net.UDPAddr) {
	k := a.IP.String()
	if t.perIP[k] <= 1 {
		delete(t.perIP, k)
	} else {
		t.perIP[k]--
	}
}

func (t *V3) maint(cmd byte, hash []byte) {
	if t.cfg.MaintPassword == "" {
		return
	}
	want := sha1.Sum([]byte(t.cfg.MaintPassword))
	for i := range want {
		if want[i] != hash[i] {
			return
		}
	}
	t.mu.Lock()
	defer t.mu.Unlock()
	if cmd == 0 { // toggle
		t.maintenance = !t.maintenance
	}
}

// Sweep drops mappings idle for longer than the timeout.
func (t *V3) Sweep() int {
	now := t.cfg.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	n := 0
	for id, c := range t.clients {
		if now.Sub(c.lastSeen) >= t.cfg.Timeout {
			t.release(c.addr)
			delete(t.clients, id)
			n++
		}
	}
	return n
}

// ---------------------------------------------------------------- V2 ----

type v2Client struct {
	addr      *net.UDPAddr // nil until the first packet
	lastSeen  time.Time
	allocated time.Time
}

// V2 is the CnCNet tunnel V2 state machine (pre-allocated 16-bit ids).
type V2 struct {
	cfg     Config
	mu      sync.Mutex
	clients map[int16]*v2Client
	reqs    *limit.Window
	ping    pinger
	rnd     func() uint16
	Stats   Stats
}

func NewV2(cfg Config, rnd func() uint16) *V2 {
	cfg.defaults()
	return &V2{cfg: cfg, clients: map[int16]*v2Client{}, rnd: rnd,
		reqs: limit.NewWindow(cfg.IPLimitV2, time.Minute, cfg.Now), ping: newPinger(&cfg)}
}

func V2Header(sender, receiver int16) []byte {
	b := make([]byte, V2HeaderSize)
	binary.BigEndian.PutUint16(b[0:], uint16(sender))
	binary.BigEndian.PutUint16(b[2:], uint16(receiver))
	return b
}

// Request allocates n ids (HTTP /request?clients=n). nil when refused.
func (t *V2) Request(n int, ip string) []int16 {
	if n < 2 || n > 8 || !t.reqs.Allow(ip) {
		return nil
	}
	now := t.cfg.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	if len(t.clients)+n > t.cfg.MaxClients {
		return nil
	}
	ids := make([]int16, 0, n)
	for tries := 0; len(ids) < n && tries < 10000; tries++ {
		id := int16(t.rnd())
		if id == 0 {
			continue
		}
		if _, used := t.clients[id]; used {
			continue
		}
		t.clients[id] = &v2Client{allocated: now}
		ids = append(ids, id)
	}
	if len(ids) < n {
		for _, id := range ids {
			delete(t.clients, id)
		}
		return nil
	}
	return ids
}

// Slots reports (free, used) for /status.
func (t *V2) Slots() (int, int) {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.cfg.MaxClients - len(t.clients), len(t.clients)
}

func (t *V2) Handle(pkt []byte, from *net.UDPAddr) []Out {
	n := len(pkt)
	if n < V2HeaderSize || n > MaxPacket || !validSource(from, t.cfg.AllowLoopback) {
		t.Stats.Dropped.Add(1)
		return nil
	}
	sender := int16(binary.BigEndian.Uint16(pkt[0:]))
	receiver := int16(binary.BigEndian.Uint16(pkt[2:]))
	if sender == 0 && receiver == 0 {
		if n == PingSize && t.ping.allow(from) {
			t.Stats.Pings.Add(1)
			r := make([]byte, PingReply)
			copy(r, pkt[:PingReply])
			return []Out{{To: cloneAddr(from), Data: r}}
		}
		t.Stats.Dropped.Add(1)
		return nil
	}
	if sender == receiver {
		t.Stats.Dropped.Add(1)
		return nil
	}
	now := t.cfg.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	c, ok := t.clients[sender]
	if !ok {
		t.Stats.Dropped.Add(1)
		return nil
	}
	if c.addr == nil || (!sameAddr(c.addr, from) && now.Sub(c.lastSeen) >= t.cfg.Timeout) {
		c.addr = cloneAddr(from)
	} else if !sameAddr(c.addr, from) {
		t.Stats.Dropped.Add(1)
		return nil
	}
	c.lastSeen = now
	dst, ok := t.clients[receiver]
	if !ok || dst.addr == nil {
		t.Stats.Dropped.Add(1)
		return nil
	}
	out := make([]byte, n)
	copy(out, pkt)
	t.Stats.Forwarded.Add(1)
	t.Stats.Bytes.Add(uint64(n))
	return []Out{{To: dst.addr, Data: out}}
}

// Sweep frees ids idle (or never bound) for longer than the timeout x2.
func (t *V2) Sweep() int {
	now := t.cfg.Now()
	t.mu.Lock()
	defer t.mu.Unlock()
	n := 0
	for id, c := range t.clients {
		ref := c.lastSeen
		if c.addr == nil {
			ref = c.allocated
		}
		if now.Sub(ref) >= 2*t.cfg.Timeout {
			delete(t.clients, id)
			n++
		}
	}
	return n
}

// ------------------------------------------------------------- serving --

// Handler is V2 or V3.
type Handler interface {
	Handle(pkt []byte, from *net.UDPAddr) []Out
	Sweep() int
}

// Serve runs h on conn until conn is closed. Sweeps every sweepEvery.
func Serve(conn *net.UDPConn, h Handler, sweepEvery time.Duration) error {
	if sweepEvery > 0 {
		stop := make(chan struct{})
		defer close(stop)
		go func() {
			tk := time.NewTicker(sweepEvery)
			defer tk.Stop()
			for {
				select {
				case <-stop:
					return
				case <-tk.C:
					h.Sweep()
				}
			}
		}()
	}
	buf := make([]byte, 2048)
	for {
		n, from, err := conn.ReadFromUDP(buf)
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return err
			}
			continue // timeouts, Windows ICMP resets: keep serving
		}
		for _, o := range h.Handle(buf[:n], from) {
			conn.WriteToUDP(o.Data, o.To)
		}
	}
}
