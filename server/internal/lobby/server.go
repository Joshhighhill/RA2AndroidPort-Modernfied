package lobby

import (
	"bufio"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unicode"
	"unicode/utf8"

	"ra2lobbyd/internal/limit"
	"ra2lobbyd/internal/relay"
	"ra2lobbyd/internal/tunnel"
)

// Config of the control plane.
type Config struct {
	ServerName     string
	MOTD           string
	PublicIP       string // advertised to clients ("" = clients use the address they dialled)
	V3Port         int
	V2Port         int
	MaxRooms       int // 500
	MaxRoomsPerIP  int // 3
	MaxConnsPerIP  int // 5
	JoinsPerMinute int // per IP (10): room codes cannot be brute-forced
	MsgRate        float64
	MsgBurst       float64
	HelloTimeout   time.Duration
	IdleTimeout    time.Duration
	MaxRelayPerIP  int // port-relay allocations per client IP (10)
	Bans           *Bans
	Logger         *log.Logger
}

// Room is one listed (or code-only) game room.
type Room struct {
	Code    string
	Name    string
	Max     int
	Players int
	Map     string
	Private bool
	PW      bool
	Started bool
	Sim     uint32
	Rules   string
	Plat    int
	HostV3  uint32
	host    *client
	members map[*client]bool
	v3ids   map[uint32]bool
	nextCID int
	created time.Time
}

type client struct {
	srv     *Server
	conn    net.Conn
	ip      string
	out     chan []byte
	hello   bool
	nick    string
	pid     string
	sim     uint32
	rules   string
	plat    int
	hosted  *Room
	joined  map[*Room]bool
	allocs  []*relay.Allocation
	bucket  *limit.Bucket
	closing atomic.Bool
}

// Server is the room directory.
type Server struct {
	cfg     Config
	Pool    *relay.Pool // nil = port relay disabled (V3 tunnel only)
	V2      *tunnel.V2  // for the HTTP side of the V2 tunnel (nil = no /request)
	mu      sync.Mutex
	rooms   map[string]*Room
	conns   *limit.Counter
	roomsIP *limit.Counter
	relayIP *limit.Counter
	joins   *limit.Window
	Metrics struct {
		Conns, Hellos, Rejects, RoomsCreated, Joins, JoinFails atomic.Uint64
	}
}

func New(cfg Config) *Server {
	if cfg.MaxRooms <= 0 {
		cfg.MaxRooms = 500
	}
	if cfg.MaxRoomsPerIP <= 0 {
		cfg.MaxRoomsPerIP = 3
	}
	if cfg.MaxConnsPerIP <= 0 {
		cfg.MaxConnsPerIP = 5
	}
	if cfg.JoinsPerMinute <= 0 {
		cfg.JoinsPerMinute = 10
	}
	if cfg.MsgRate <= 0 {
		cfg.MsgRate = 20
	}
	if cfg.MsgBurst <= 0 {
		cfg.MsgBurst = 50
	}
	if cfg.HelloTimeout <= 0 {
		cfg.HelloTimeout = 10 * time.Second
	}
	if cfg.IdleTimeout <= 0 {
		cfg.IdleTimeout = 60 * time.Second
	}
	if cfg.MaxRelayPerIP <= 0 {
		cfg.MaxRelayPerIP = 10
	}
	if cfg.ServerName == "" {
		cfg.ServerName = "ra2lobbyd"
	}
	if cfg.Logger == nil {
		cfg.Logger = log.Default()
	}
	return &Server{cfg: cfg, rooms: map[string]*Room{},
		conns:   limit.NewCounter(cfg.MaxConnsPerIP),
		roomsIP: limit.NewCounter(cfg.MaxRoomsPerIP),
		relayIP: limit.NewCounter(cfg.MaxRelayPerIP),
		joins:   limit.NewWindow(cfg.JoinsPerMinute, time.Minute, nil)}
}

func (s *Server) logf(f string, a ...any) { s.cfg.Logger.Printf(f, a...) }

// RoomCount is the number of open rooms.
func (s *Server) RoomCount() int { s.mu.Lock(); defer s.mu.Unlock(); return len(s.rooms) }

// ---------------------------------------------------------- room codes --

// CodeAlphabet: Crockford-style base32 minus the look-alikes I L O U 0 1
// (30 symbols, 30^6 ~= 7.3e8 codes).
const CodeAlphabet = "ABCDEFGHJKMNPQRSTVWXYZ23456789"

// NewCode returns a random 6-character room code.
func NewCode() string {
	var b [6]byte
	var r [6]byte
	if _, err := rand.Read(r[:]); err != nil {
		panic(err)
	}
	for i := range b {
		b[i] = CodeAlphabet[int(r[i])%len(CodeAlphabet)]
	}
	return string(b[:])
}

// NormalizeCode upper-cases a typed code and trims spaces. The look-alike
// symbols (I L O U 0 1) are not in the alphabet, so a mistyped code simply
// fails ValidCode instead of silently matching another room.
func NormalizeCode(c string) string {
	return strings.ToUpper(strings.TrimSpace(c))
}

// ValidCode checks length and alphabet.
func ValidCode(c string) bool {
	if len(c) != 6 {
		return false
	}
	for i := 0; i < len(c); i++ {
		if !strings.ContainsRune(CodeAlphabet, rune(c[i])) {
			return false
		}
	}
	return true
}

// ---------------------------------------------------- input validation --

// ValidText accepts 1..max runes of printable text: no control characters,
// no bidi overrides / isolates, no leading/trailing space (§4.8).
func ValidText(s string, max int) bool {
	if s == "" || !utf8.ValidString(s) || utf8.RuneCountInString(s) > max {
		return false
	}
	if strings.TrimSpace(s) != s {
		return false
	}
	for _, r := range s {
		if unicode.IsControl(r) || !unicode.IsPrint(r) && r != ' ' {
			return false
		}
		if (r >= 0x202A && r <= 0x202E) || (r >= 0x2066 && r <= 0x2069) || r == 0x200E || r == 0x200F {
			return false
		}
	}
	return true
}

func validPID(p string) bool {
	if len(p) != 16 {
		return false
	}
	_, err := hex.DecodeString(p)
	return err == nil
}

// ------------------------------------------------------------ serving --

// Serve accepts on ln: HTTP requests go to httpHandler (CnCNet V2 /request,
// /status, /healthz), everything else is the line-JSON control plane.
func (s *Server) Serve(ln net.Listener) error {
	hl := &chanListener{addr: ln.Addr(), ch: make(chan net.Conn, 16), done: make(chan struct{})}
	hs := &http.Server{Handler: s.HTTPHandler(), ReadHeaderTimeout: 5 * time.Second}
	go hs.Serve(hl)
	defer hl.Close()
	for {
		c, err := ln.Accept()
		if err != nil {
			return err
		}
		go s.sniff(c, hl)
	}
}

func (s *Server) sniff(c net.Conn, hl *chanListener) {
	// Per-IP connection cap first: a silent connection must not get past
	// the cap while we wait for its first bytes.
	ip := limit.IPKey(c.RemoteAddr())
	if !s.conns.Acquire(ip) {
		b, _ := json.Marshal(Msg{T: "reject", Reason: "busy", Detail: "too many connections from your address"})
		c.SetWriteDeadline(time.Now().Add(2 * time.Second))
		c.Write(append(b, '\n'))
		c.Close()
		return
	}
	br := bufio.NewReaderSize(c, MaxLine+16)
	c.SetReadDeadline(time.Now().Add(s.cfg.HelloTimeout))
	head, err := br.Peek(4)
	if err != nil {
		s.conns.Release(ip)
		c.Close()
		return
	}
	pc := &peekConn{Conn: c, r: br}
	switch string(head) {
	case "GET ", "HEAD", "POST":
		s.conns.Release(ip) // HTTP is short-lived; net/http has its own timeouts
		c.SetReadDeadline(time.Time{})
		select {
		case hl.ch <- pc:
		case <-hl.done:
			c.Close()
		}
	default:
		s.handleConn(pc, br)
	}
}

type peekConn struct {
	net.Conn
	r *bufio.Reader
}

func (p *peekConn) Read(b []byte) (int, error) { return p.r.Read(b) }

type chanListener struct {
	addr net.Addr
	ch   chan net.Conn
	done chan struct{}
	once sync.Once
}

func (l *chanListener) Accept() (net.Conn, error) {
	select {
	case c := <-l.ch:
		return c, nil
	case <-l.done:
		return nil, net.ErrClosed
	}
}
func (l *chanListener) Close() error   { l.once.Do(func() { close(l.done) }); return nil }
func (l *chanListener) Addr() net.Addr { return l.addr }

// HTTPHandler serves the CnCNet V2 tunnel HTTP API and health.
func (s *Server) HTTPHandler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "ok rooms=%d\n", s.RoomCount())
	})
	mux.HandleFunc("/request", func(w http.ResponseWriter, r *http.Request) {
		if s.V2 == nil {
			http.Error(w, "v2 disabled", http.StatusNotFound)
			return
		}
		n, _ := strconv.Atoi(r.URL.Query().Get("clients"))
		host, _, _ := net.SplitHostPort(r.RemoteAddr)
		ids := s.V2.Request(n, host)
		if ids == nil {
			http.Error(w, "refused", http.StatusBadRequest)
			return
		}
		parts := make([]string, len(ids))
		for i, id := range ids {
			parts[i] = strconv.Itoa(int(id))
		}
		fmt.Fprintf(w, "[%s]", strings.Join(parts, ","))
	})
	mux.HandleFunc("/status", func(w http.ResponseWriter, r *http.Request) {
		free, used := 0, 0
		if s.V2 != nil {
			free, used = s.V2.Slots()
		}
		fmt.Fprintf(w, "%d slots free.\n%d slots in use.\n", free, used)
	})
	return mux
}

func (s *Server) handleConn(c net.Conn, br *bufio.Reader) {
	ip := limit.IPKey(c.RemoteAddr()) // the per-IP slot was acquired by sniff
	s.Metrics.Conns.Add(1)
	cl := &client{srv: s, conn: c, ip: ip, out: make(chan []byte, 64), joined: map[*Room]bool{},
		bucket: limit.NewBucket(s.cfg.MsgRate, s.cfg.MsgBurst, nil)}
	done := make(chan struct{})
	go cl.writer(done)
	defer func() {
		s.drop(cl)
		close(cl.out)
		<-done
		c.Close()
		s.conns.Release(ip)
	}()
	for {
		if cl.hello {
			c.SetReadDeadline(time.Now().Add(s.cfg.IdleTimeout))
		}
		line, err := readLine(br)
		if err != nil {
			if errors.Is(err, errTooLong) {
				cl.send(Msg{T: "reject", Reason: "protocol", Detail: "message too long"})
			}
			return
		}
		if len(line) == 0 {
			continue
		}
		if !cl.bucket.Allow() {
			cl.send(Msg{T: "error", Reason: "rate"})
			continue
		}
		var m Msg
		if err := json.Unmarshal(line, &m); err != nil {
			if !cl.hello {
				cl.send(Msg{T: "reject", Reason: "protocol", Detail: "not a ra2lobbyd client"})
				return
			}
			cl.send(Msg{T: "error", Reason: "bad_request"})
			continue
		}
		if !s.dispatch(cl, &m) {
			return
		}
	}
}

var errTooLong = errors.New("line too long")

func readLine(br *bufio.Reader) ([]byte, error) {
	var buf []byte
	for {
		frag, isPrefix, err := br.ReadLine()
		if err != nil {
			return nil, err
		}
		buf = append(buf, frag...)
		if len(buf) > MaxLine {
			return nil, errTooLong
		}
		if !isPrefix {
			return buf, nil
		}
	}
}

func (cl *client) writer(done chan struct{}) {
	defer close(done)
	for b := range cl.out {
		cl.conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
		if _, err := cl.conn.Write(b); err != nil {
			cl.closing.Store(true)
			cl.conn.Close()
		}
	}
}

func (cl *client) send(m Msg) {
	if cl.closing.Load() {
		return
	}
	b, _ := json.Marshal(m)
	b = append(b, '\n')
	select {
	case cl.out <- b:
	default: // a client that does not read is cut off
		cl.closing.Store(true)
		cl.conn.Close()
	}
}

// dispatch handles one message; false closes the connection.
func (s *Server) dispatch(cl *client, m *Msg) bool {
	if !cl.hello {
		if m.T != "hello" {
			cl.send(Msg{T: "reject", Reason: "protocol", Detail: "hello first"})
			return false
		}
		return s.onHello(cl, m)
	}
	switch m.T {
	case "ping":
		cl.send(Msg{T: "pong"})
	case "list":
		cl.send(Msg{T: "rooms", Rooms: s.list()})
	case "create":
		s.onCreate(cl, m)
	case "update":
		s.onUpdate(cl, m)
	case "close":
		s.closeRoom(cl)
	case "join":
		s.onJoin(cl, m)
	default:
		cl.send(Msg{T: "error", Reason: "bad_request", Op: m.T})
	}
	return true
}

func (s *Server) onHello(cl *client, m *Msg) bool {
	reject := func(reason, detail string) bool {
		s.Metrics.Rejects.Add(1)
		cl.send(Msg{T: "reject", Reason: reason, Detail: detail})
		return false
	}
	if m.Magic != Magic {
		return reject("protocol", "not a ra2lobbyd client")
	}
	if m.Proto < ProtoMin || m.Proto > ProtoMax {
		return reject("version", fmt.Sprintf("this server speaks net protocol %d..%d, your game speaks %d: please update",
			ProtoMin, ProtoMax, m.Proto))
	}
	if !ValidText(m.Nick, 16) {
		return reject("bad_name", "nickname must be 1-16 printable characters")
	}
	if !validPID(m.PID) {
		return reject("protocol", "bad player id")
	}
	if s.cfg.Bans != nil && s.cfg.Bans.Banned(cl.ip, m.PID) {
		return reject("banned", "")
	}
	cl.hello, cl.nick, cl.pid, cl.sim, cl.rules, cl.plat = true, m.Nick, strings.ToLower(m.PID), m.Sim, m.Rules, m.Plat
	s.Metrics.Hellos.Add(1)
	cl.send(Msg{T: "hello_ok", Server: s.cfg.ServerName, MOTD: s.cfg.MOTD, ProtoLo: ProtoMin, ProtoHi: ProtoMax,
		V3Port: s.cfg.V3Port, V2Port: s.cfg.V2Port, PublicIP: s.cfg.PublicIP})
	return true
}

func (s *Server) list() []RoomInfo {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := make([]RoomInfo, 0, len(s.rooms))
	for _, r := range s.rooms {
		if r.Private {
			continue
		}
		out = append(out, RoomInfo{Code: r.Code, Name: r.Name, Host: r.host.nick, Map: r.Map, Players: r.Players,
			Max: r.Max, Started: r.Started, PW: r.PW, Sim: r.Sim, Rules: r.Rules, Plat: r.Plat})
	}
	// Stable order for clients: oldest first.
	for i := 1; i < len(out); i++ {
		for j := i; j > 0 && s.rooms[out[j].Code].created.Before(s.rooms[out[j-1].Code].created); j-- {
			out[j], out[j-1] = out[j-1], out[j]
		}
	}
	return out
}

func (s *Server) onCreate(cl *client, m *Msg) {
	fail := func(r string) { cl.send(Msg{T: "error", Op: "create", Reason: r}) }
	if cl.hosted != nil {
		fail("already")
		return
	}
	if !ValidText(m.Name, 32) || (m.Map != "" && !ValidText(m.Map, 64)) || m.Max < 2 || m.Max > 8 || m.V3 == 0 {
		fail("bad_request")
		return
	}
	if !s.roomsIP.Acquire(cl.ip) {
		fail("rate")
		return
	}
	s.mu.Lock()
	if len(s.rooms) >= s.cfg.MaxRooms {
		s.mu.Unlock()
		s.roomsIP.Release(cl.ip)
		fail("full")
		return
	}
	code := NewCode()
	for s.rooms[code] != nil {
		code = NewCode()
	}
	r := &Room{Code: code, Name: m.Name, Max: m.Max, Players: 1, Map: m.Map, Private: m.Private, PW: m.PW,
		Sim: cl.sim, Rules: cl.rules, Plat: cl.plat, HostV3: m.V3, host: cl, members: map[*client]bool{},
		v3ids: map[uint32]bool{m.V3: true}, created: time.Now()}
	s.rooms[code] = r
	cl.hosted = r
	s.mu.Unlock()
	s.Metrics.RoomsCreated.Add(1)
	s.logf("room %s created by %s (private=%v)", code, limit.Truncate(cl.ip), m.Private)
	cl.send(Msg{T: "created", Code: code, V3Port: s.cfg.V3Port})
}

func (s *Server) onUpdate(cl *client, m *Msg) {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := cl.hosted
	if r == nil {
		cl.send(Msg{T: "error", Op: "update", Reason: "no_room"})
		return
	}
	if m.Max >= 2 && m.Max <= 8 {
		r.Max = m.Max
	}
	if m.Players >= 1 && m.Players <= 8 {
		r.Players = m.Players
	}
	if m.Map != "" && ValidText(m.Map, 64) {
		r.Map = m.Map
	}
	r.Started = m.Started
}

func (s *Server) onJoin(cl *client, m *Msg) {
	fail := func(r string) {
		s.Metrics.JoinFails.Add(1)
		cl.send(Msg{T: "error", Op: "join", Reason: r})
	}
	if !s.joins.Allow(cl.ip) {
		fail("rate")
		return
	}
	code := NormalizeCode(m.Code)
	if !ValidCode(code) {
		fail("no_room")
		return
	}
	if m.V3 == 0 {
		fail("bad_request")
		return
	}
	s.mu.Lock()
	r := s.rooms[code]
	switch {
	case r == nil:
		s.mu.Unlock()
		fail("no_room")
		return
	case r.host == cl:
		s.mu.Unlock()
		fail("already")
		return
	case r.Started:
		s.mu.Unlock()
		fail("started")
		return
	case r.Players >= r.Max:
		s.mu.Unlock()
		fail("full")
		return
	case r.Sim != cl.sim || r.Rules != cl.rules:
		s.mu.Unlock()
		fail("version")
		return
	case r.v3ids[m.V3]:
		s.mu.Unlock()
		fail("v3_taken")
		return
	}
	r.nextCID++
	cid := r.nextCID
	r.v3ids[m.V3] = true
	r.members[cl] = true
	cl.joined[r] = true
	host := r.host
	hostV3, hostNick := r.HostV3, r.host.nick
	s.mu.Unlock()

	joined := Msg{T: "joined", Code: code, CID: cid, HostNick: hostNick, HostV3: hostV3, V3Port: s.cfg.V3Port,
		PublicIP: s.cfg.PublicIP}
	peer := Msg{T: "peer", CID: cid, Nick: cl.nick, PID: cl.pid, V3: m.V3}
	if m.Relay == "port" && s.Pool != nil {
		if !s.relayIP.Acquire(cl.ip) {
			fail("rate")
			return
		}
		ip := cl.ip
		a, err := s.Pool.Allocate(func() { s.relayIP.Release(ip) })
		if err != nil {
			s.relayIP.Release(ip)
			fail("busy")
			return
		}
		cl.allocs = append(cl.allocs, a)
		joined.RelayPort, joined.RelayToken = a.Port, hex.EncodeToString(a.ClientToken[:])
		peer.RelayPort, peer.RelayToken = a.Port, hex.EncodeToString(a.HostToken[:])
	}
	s.Metrics.Joins.Add(1)
	s.logf("room %s: join from %s (cid %d)", code, limit.Truncate(cl.ip), cid)
	host.send(peer)
	cl.send(joined)
}

func (s *Server) closeRoom(cl *client) {
	s.mu.Lock()
	r := cl.hosted
	if r == nil {
		s.mu.Unlock()
		return
	}
	delete(s.rooms, r.Code)
	cl.hosted = nil
	members := make([]*client, 0, len(r.members))
	for m := range r.members {
		members = append(members, m)
		delete(m.joined, r)
	}
	s.mu.Unlock()
	s.roomsIP.Release(cl.ip)
	for _, m := range members {
		m.send(Msg{T: "room_closed", Code: r.Code})
	}
	s.logf("room %s closed", r.Code)
}

func (s *Server) drop(cl *client) {
	s.closeRoom(cl)
	s.mu.Lock()
	for r := range cl.joined {
		delete(r.members, cl)
	}
	cl.joined = map[*Room]bool{}
	s.mu.Unlock()
	for _, a := range cl.allocs {
		a.Close()
	}
}

// NewPID makes a random per-install player id (used by tests / tools).
func NewPID() string {
	var b [8]byte
	rand.Read(b[:])
	return hex.EncodeToString(b[:])
}
