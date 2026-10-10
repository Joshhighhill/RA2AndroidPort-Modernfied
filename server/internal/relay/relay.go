// Package relay is the TURN-style per-client UDP relay of
// docs/gap/net/05-transport-infra.md §4.3: every relayed client gets its own
// UDP port from a pool; that port has exactly two legitimate remotes (the
// room host and that client), learnt from authenticated BIND datagrams, and
// forwards raw datagrams between them unchanged (no header). Anything else is
// dropped. It complements the CnCNet-compatible header tunnel (package
// tunnel) for transports that cannot carry a tunnel header.
//
// BIND (sent to the allocated port, from the socket that will carry game
// traffic):    "RA2B" + token[16]                     (20 bytes)
// BIND_ACK:    "RA2A" + observed IPv4[4] + port BE[2] (10 bytes, never
// larger than the request, so the relay cannot amplify).
// A BIND with the right token from a new address re-points that side
// (Wi-Fi -> LTE handover, NAT rebinding).
package relay

import (
	"crypto/rand"
	"encoding/binary"
	"errors"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"ra2lobbyd/internal/limit"
)

const (
	BindSize    = 20
	BindAckSize = 10
	TokenSize   = 16
)

var (
	bindMagic = []byte("RA2B")
	ackMagic  = []byte("RA2A")

	ErrPoolExhausted = errors.New("relay: no free port")
)

type Token [TokenSize]byte

func NewToken() Token {
	var t Token
	if _, err := rand.Read(t[:]); err != nil {
		panic(err)
	}
	return t
}

// BindPacket builds the 20-byte BIND datagram for tok.
func BindPacket(tok Token) []byte { return append(append([]byte{}, bindMagic...), tok[:]...) }

// ParseBindAck returns the observed address in a BIND_ACK.
func ParseBindAck(b []byte) (*net.UDPAddr, bool) {
	if len(b) != BindAckSize || string(b[:4]) != string(ackMagic) {
		return nil, false
	}
	return &net.UDPAddr{IP: net.IPv4(b[4], b[5], b[6], b[7]), Port: int(binary.BigEndian.Uint16(b[8:]))}, true
}

// Config of the pool.
type Config struct {
	BindIP      string        // "" = all interfaces
	PortLo      int           // 50100
	PortHi      int           // 50999
	Idle        time.Duration // close an allocation idle this long (60 s)
	BytesPerSec float64       // per direction (64 KB/s)
	PktsPerSec  float64       // per direction (200 pkt/s)
	Now         limit.Clock
}

// Allocation is one relay port between a host and one client.
type Allocation struct {
	Port        int
	HostToken   Token
	ClientToken Token

	pool      *Pool
	conn      *net.UDPConn
	mu        sync.Mutex
	host      *net.UDPAddr
	client    *net.UDPAddr
	last      time.Time
	upBytes   *limit.Bucket // client -> host
	upPkts    *limit.Bucket
	downBytes *limit.Bucket // host -> client
	downPkts  *limit.Bucket
	closed    atomic.Bool
	Forwarded atomic.Uint64
	Dropped   atomic.Uint64
	onClose   func()
}

// Pool hands out ports in [PortLo, PortHi].
type Pool struct {
	cfg   Config
	mu    sync.Mutex
	used  map[int]*Allocation
	next  int
	Total atomic.Uint64
}

func NewPool(cfg Config) *Pool {
	if cfg.Idle <= 0 {
		cfg.Idle = 60 * time.Second
	}
	if cfg.BytesPerSec <= 0 {
		cfg.BytesPerSec = 64 * 1024
	}
	if cfg.PktsPerSec <= 0 {
		cfg.PktsPerSec = 200
	}
	if cfg.Now == nil {
		cfg.Now = time.Now
	}
	return &Pool{cfg: cfg, used: map[int]*Allocation{}, next: cfg.PortLo}
}

// InUse is the number of live allocations.
func (p *Pool) InUse() int { p.mu.Lock(); defer p.mu.Unlock(); return len(p.used) }

// Allocate opens a port (PortLo..PortHi; PortLo == 0 = any ephemeral port,
// for tests) and starts forwarding. onClose runs once when it is released.
func (p *Pool) Allocate(onClose func()) (*Allocation, error) {
	p.mu.Lock()
	defer p.mu.Unlock()
	var conn *net.UDPConn
	port := 0
	if p.cfg.PortLo == 0 {
		c, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.ParseIP(p.cfg.BindIP), Port: 0})
		if err != nil {
			return nil, err
		}
		conn, port = c, c.LocalAddr().(*net.UDPAddr).Port
	} else {
		span := p.cfg.PortHi - p.cfg.PortLo + 1
		for i := 0; i < span && conn == nil; i++ {
			cand := p.cfg.PortLo + (p.next-p.cfg.PortLo+i)%span
			if _, busy := p.used[cand]; busy {
				continue
			}
			c, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.ParseIP(p.cfg.BindIP), Port: cand})
			if err != nil {
				continue // taken by someone else on the box
			}
			conn, port = c, cand
			p.next = cand + 1
		}
		if conn == nil {
			return nil, ErrPoolExhausted
		}
	}
	now := p.cfg.Now
	a := &Allocation{Port: port, HostToken: NewToken(), ClientToken: NewToken(), pool: p, conn: conn,
		last:      now(),
		upBytes:   limit.NewBucket(p.cfg.BytesPerSec, p.cfg.BytesPerSec, now),
		upPkts:    limit.NewBucket(p.cfg.PktsPerSec, p.cfg.PktsPerSec, now),
		downBytes: limit.NewBucket(p.cfg.BytesPerSec, p.cfg.BytesPerSec, now),
		downPkts:  limit.NewBucket(p.cfg.PktsPerSec, p.cfg.PktsPerSec, now),
		onClose:   onClose}
	p.used[port] = a
	p.Total.Add(1)
	go a.serve()
	return a, nil
}

// Close releases the allocation (idempotent).
func (a *Allocation) Close() {
	if a.closed.Swap(true) {
		return
	}
	a.conn.Close()
	a.pool.mu.Lock()
	delete(a.pool.used, a.Port)
	a.pool.mu.Unlock()
	if a.onClose != nil {
		a.onClose()
	}
}

// Endpoints reports the bound host/client addresses (nil until bound).
func (a *Allocation) Endpoints() (host, client *net.UDPAddr) {
	a.mu.Lock()
	defer a.mu.Unlock()
	return a.host, a.client
}

func eqAddr(x, y *net.UDPAddr) bool { return x != nil && y != nil && x.Port == y.Port && x.IP.Equal(y.IP) }

func (a *Allocation) serve() {
	buf := make([]byte, 2048)
	idle := a.pool.cfg.Idle
	for {
		a.conn.SetReadDeadline(time.Now().Add(idle / 4))
		n, from, err := a.conn.ReadFromUDP(buf)
		if a.closed.Load() {
			return
		}
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				a.mu.Lock()
				stale := a.pool.cfg.Now().Sub(a.last) >= idle
				a.mu.Unlock()
				if stale {
					a.Close()
					return
				}
				continue
			}
			if errors.Is(err, net.ErrClosed) {
				a.Close()
				return
			}
			continue
		}
		a.handle(buf[:n], from)
	}
}

func (a *Allocation) handle(pkt []byte, from *net.UDPAddr) {
	if len(pkt) == BindSize && string(pkt[:4]) == string(bindMagic) {
		var tok Token
		copy(tok[:], pkt[4:])
		a.mu.Lock()
		ok := true
		switch tok {
		case a.HostToken:
			a.host = &net.UDPAddr{IP: append(net.IP{}, from.IP...), Port: from.Port}
		case a.ClientToken:
			a.client = &net.UDPAddr{IP: append(net.IP{}, from.IP...), Port: from.Port}
		default:
			ok = false
		}
		if ok {
			a.last = a.pool.cfg.Now()
		}
		a.mu.Unlock()
		if !ok {
			a.Dropped.Add(1)
			return
		}
		ack := make([]byte, BindAckSize)
		copy(ack, ackMagic)
		if v4 := from.IP.To4(); v4 != nil {
			copy(ack[4:8], v4)
		}
		binary.BigEndian.PutUint16(ack[8:], uint16(from.Port))
		a.conn.WriteToUDP(ack, from)
		return
	}
	a.mu.Lock()
	var to *net.UDPAddr
	var bb, pb *limit.Bucket
	switch {
	case eqAddr(from, a.client):
		to, bb, pb = a.host, a.upBytes, a.upPkts
	case eqAddr(from, a.host):
		to, bb, pb = a.client, a.downBytes, a.downPkts
	}
	if to != nil {
		a.last = a.pool.cfg.Now()
	}
	a.mu.Unlock()
	if to == nil || !pb.Allow() || !bb.AllowN(float64(len(pkt))) {
		a.Dropped.Add(1)
		return
	}
	a.conn.WriteToUDP(pkt, to)
	a.Forwarded.Add(1)
}
