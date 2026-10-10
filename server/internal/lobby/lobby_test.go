package lobby

import (
	"bufio"
	"encoding/hex"
	"encoding/json"
	"io"
	"log"
	"net"
	"net/http"
	"strings"
	"testing"
	"time"

	"ra2lobbyd/internal/relay"
	"ra2lobbyd/internal/tunnel"
)

type tc struct {
	t  *testing.T
	c  net.Conn
	br *bufio.Reader
}

func startServer(t *testing.T, cfg Config) (*Server, string) {
	cfg.Logger = log.New(io.Discard, "", 0)
	s := New(cfg)
	ln, err := net.Listen("tcp4", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go s.Serve(ln)
	return s, ln.Addr().String()
}

func dial(t *testing.T, addr string) *tc {
	c, err := net.Dial("tcp4", addr)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { c.Close() })
	return &tc{t: t, c: c, br: bufio.NewReader(c)}
}

func (c *tc) send(m Msg) {
	b, _ := json.Marshal(m)
	c.c.Write(append(b, '\n'))
}

func (c *tc) recv() Msg {
	c.c.SetReadDeadline(time.Now().Add(3 * time.Second))
	line, err := c.br.ReadBytes('\n')
	if err != nil {
		c.t.Fatalf("recv: %v", err)
	}
	var m Msg
	if err := json.Unmarshal(line, &m); err != nil {
		c.t.Fatalf("bad json %q", line)
	}
	return m
}

func (c *tc) expect(typ string) Msg {
	m := c.recv()
	if m.T != typ {
		c.t.Fatalf("want %s, got %+v", typ, m)
	}
	return m
}

func hello(nick string) Msg {
	return Msg{T: "hello", Magic: Magic, Proto: 1, Ver: "0.9.0", Sim: 1, Rules: "1234abcd", Plat: 1,
		PID: NewPID(), Nick: nick}
}

func TestCodes(t *testing.T) {
	seen := map[string]bool{}
	for i := 0; i < 2000; i++ {
		c := NewCode()
		if !ValidCode(c) {
			t.Fatalf("invalid code %q", c)
		}
		seen[c] = true
	}
	if len(seen) < 1990 {
		t.Fatalf("codes should be random, %d unique of 2000", len(seen))
	}
	for _, bad := range []string{"ABCDE", "ABCDEFG", "ABCDE0", "ABCDEI", "abcdeO"} {
		if ValidCode(NormalizeCode(bad)) {
			t.Errorf("%q must be invalid", bad)
		}
	}
	if !ValidCode(NormalizeCode(" k7q2mx ")) {
		t.Errorf("lower case + spaces normalise")
	}
}

func TestValidText(t *testing.T) {
	ok := []string{"Tanya", "Kane 2", "Ünïcødé"}
	bad := []string{"", " lead", "trail ", "a\x07b", "evil‮gnp", strings.Repeat("x", 17)}
	for _, s := range ok {
		if !ValidText(s, 16) {
			t.Errorf("%q should pass", s)
		}
	}
	for _, s := range bad {
		if ValidText(s, 16) {
			t.Errorf("%q should fail", s)
		}
	}
}

func TestVersionGate(t *testing.T) {
	_, addr := startServer(t, Config{})
	c := dial(t, addr)
	m := hello("Old")
	m.Proto = 99
	c.send(m)
	r := c.expect("reject")
	if r.Reason != "version" || !strings.Contains(r.Detail, "update") {
		t.Fatalf("version reject: %+v", r)
	}
	c2 := dial(t, addr)
	c2.send(Msg{T: "list"})
	if c2.expect("reject").Reason != "protocol" {
		t.Fatalf("hello must come first")
	}
	c3 := dial(t, addr)
	m = hello("bad‮name")
	c3.send(m)
	if c3.expect("reject").Reason != "bad_name" {
		t.Fatalf("bidi nickname rejected")
	}
	c4 := dial(t, addr)
	c4.c.Write([]byte("NICK foo\r\n"))
	if c4.expect("reject").Reason != "protocol" {
		t.Fatalf("non-JSON client rejected")
	}
}

func TestRoomFlowAddressExchange(t *testing.T) {
	_, addr := startServer(t, Config{V3Port: 50001, ServerName: "Test", MOTD: "hi"})
	host := dial(t, addr)
	host.send(hello("Host"))
	ok := host.expect("hello_ok")
	if ok.Server != "Test" || ok.MOTD != "hi" || ok.V3Port != 50001 || ok.ProtoLo != 1 {
		t.Fatalf("hello_ok %+v", ok)
	}
	host.send(Msg{T: "create", Name: "Friday", Max: 4, Map: "Tour of Egypt", V3: 0xAAAA0001})
	cr := host.expect("created")
	if !ValidCode(cr.Code) {
		t.Fatalf("bad code %q", cr.Code)
	}

	guest := dial(t, addr)
	guest.send(hello("Guest"))
	guest.expect("hello_ok")
	guest.send(Msg{T: "list"})
	rooms := guest.expect("rooms").Rooms
	if len(rooms) != 1 || rooms[0].Code != cr.Code || rooms[0].Host != "Host" || rooms[0].Max != 4 || rooms[0].Map != "Tour of Egypt" {
		t.Fatalf("room list %+v", rooms)
	}
	guest.send(Msg{T: "join", Code: strings.ToLower(cr.Code), V3: 0xBBBB0002})
	j := guest.expect("joined")
	if j.HostV3 != 0xAAAA0001 || j.HostNick != "Host" || j.V3Port != 50001 || j.CID != 1 {
		t.Fatalf("joined %+v", j)
	}
	p := host.expect("peer")
	if p.V3 != 0xBBBB0002 || p.Nick != "Guest" || p.CID != 1 || len(p.PID) != 16 {
		t.Fatalf("peer %+v", p)
	}
	// Same tunnel id twice is refused.
	g2 := dial(t, addr)
	g2.send(hello("Guest2"))
	g2.expect("hello_ok")
	g2.send(Msg{T: "join", Code: cr.Code, V3: 0xBBBB0002})
	if e := g2.expect("error"); e.Reason != "v3_taken" {
		t.Fatalf("duplicate v3 id: %+v", e)
	}
	// Host marks the room started -> joins refused.
	host.send(Msg{T: "update", Players: 2, Max: 4, Started: true})
	host.send(Msg{T: "ping"})
	host.expect("pong")
	g2.send(Msg{T: "join", Code: cr.Code, V3: 0xCCCC0003})
	if e := g2.expect("error"); e.Reason != "started" {
		t.Fatalf("started room: %+v", e)
	}
	// Host leaves -> joined members are told.
	host.c.Close()
	if rc := guest.expect("room_closed"); rc.Code != cr.Code {
		t.Fatalf("room_closed %+v", rc)
	}
	guest.send(Msg{T: "list"})
	if rs := guest.recv(); len(rs.Rooms) != 0 {
		t.Fatalf("room gone from the list")
	}
}

func TestPrivateRoomsAndVersionMismatch(t *testing.T) {
	_, addr := startServer(t, Config{})
	host := dial(t, addr)
	host.send(hello("Host"))
	host.expect("hello_ok")
	host.send(Msg{T: "create", Name: "Secret", Max: 2, Private: true, V3: 7})
	code := host.expect("created").Code
	g := dial(t, addr)
	m := hello("Guest")
	m.Rules = "deadbeef"
	g.send(m)
	g.expect("hello_ok")
	g.send(Msg{T: "list"})
	if rs := g.expect("rooms"); len(rs.Rooms) != 0 {
		t.Fatalf("private rooms are never listed")
	}
	g.send(Msg{T: "join", Code: code, V3: 8})
	if e := g.expect("error"); e.Reason != "version" {
		t.Fatalf("different rules CRC must be refused: %+v", e)
	}
	g.send(Msg{T: "join", Code: "ZZZZZZ", V3: 8})
	if e := g.expect("error"); e.Reason != "no_room" {
		t.Fatalf("unknown code: %+v", e)
	}
}

func TestLimits(t *testing.T) {
	_, addr := startServer(t, Config{MaxConnsPerIP: 2, MaxRoomsPerIP: 1, JoinsPerMinute: 3})
	a := dial(t, addr)
	a.send(hello("A"))
	a.expect("hello_ok")
	b := dial(t, addr)
	b.send(hello("B"))
	b.expect("hello_ok")
	c := dial(t, addr)
	if r := c.expect("reject"); r.Reason != "busy" {
		t.Fatalf("third connection from one IP: %+v", r)
	}
	a.send(Msg{T: "create", Name: "R1", Max: 2, V3: 1})
	a.expect("created")
	b.send(Msg{T: "create", Name: "R2", Max: 2, V3: 2})
	if e := b.expect("error"); e.Reason != "rate" {
		t.Fatalf("rooms per IP: %+v", e)
	}
	for i := 0; i < 3; i++ {
		b.send(Msg{T: "join", Code: "ZZZZZZ", V3: 5})
		b.expect("error")
	}
	b.send(Msg{T: "join", Code: "ZZZZZZ", V3: 5})
	if e := b.expect("error"); e.Reason != "rate" {
		t.Fatalf("join attempts are rate limited: %+v", e)
	}
	// Oversized line closes the connection.
	a.c.Write([]byte(`{"t":"ping","nick":"` + strings.Repeat("x", MaxLine) + "\"}\n"))
	if r := a.expect("reject"); r.Reason != "protocol" {
		t.Fatalf("oversized: %+v", r)
	}
}

func TestPortRelayJoin(t *testing.T) {
	s, addr := startServer(t, Config{})
	s.Pool = relay.NewPool(relay.Config{BindIP: "127.0.0.1"})
	host := dial(t, addr)
	host.send(hello("Host"))
	host.expect("hello_ok")
	host.send(Msg{T: "create", Name: "R", Max: 2, V3: 1})
	code := host.expect("created").Code
	g := dial(t, addr)
	g.send(hello("G"))
	g.expect("hello_ok")
	g.send(Msg{T: "join", Code: code, V3: 2, Relay: "port"})
	j := g.expect("joined")
	p := host.expect("peer")
	if j.RelayPort == 0 || j.RelayPort != p.RelayPort || j.RelayToken == p.RelayToken {
		t.Fatalf("relay allocation: joined %+v peer %+v", j, p)
	}
	// Both sides bind with their tokens and exchange a raw datagram.
	ra := &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: j.RelayPort}
	hu, _ := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	gu, _ := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	defer hu.Close()
	defer gu.Close()
	var ht, gt relay.Token
	hb, _ := hex.DecodeString(p.RelayToken)
	gb, _ := hex.DecodeString(j.RelayToken)
	copy(ht[:], hb)
	copy(gt[:], gb)
	buf := make([]byte, 64)
	hu.WriteToUDP(relay.BindPacket(ht), ra)
	hu.SetReadDeadline(time.Now().Add(2 * time.Second))
	hu.ReadFromUDP(buf)
	gu.WriteToUDP(relay.BindPacket(gt), ra)
	gu.SetReadDeadline(time.Now().Add(2 * time.Second))
	gu.ReadFromUDP(buf)
	gu.WriteToUDP([]byte("raw"), ra)
	hu.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err := hu.ReadFromUDP(buf)
	if err != nil || string(buf[:n]) != "raw" {
		t.Fatalf("relay forward: %v %q", err, buf[:n])
	}
	if s.Pool.InUse() != 1 {
		t.Fatalf("one allocation in use")
	}
	g.c.Close()
	deadline := time.Now().Add(3 * time.Second)
	for s.Pool.InUse() != 0 && time.Now().Before(deadline) {
		time.Sleep(20 * time.Millisecond)
	}
	if s.Pool.InUse() != 0 {
		t.Fatalf("allocation released when the client disconnects")
	}
}

func TestHTTPOnSamePort(t *testing.T) {
	s, addr := startServer(t, Config{})
	n := uint16(40)
	s.V2 = tunnel.NewV2(tunnel.Config{}, func() uint16 { n++; return n })
	resp, err := http.Get("http://" + addr + "/request?clients=2")
	if err != nil {
		t.Fatal(err)
	}
	body, _ := io.ReadAll(resp.Body)
	resp.Body.Close()
	if string(body) != "[41,42]" {
		t.Fatalf("/request body %q", body)
	}
	resp, err = http.Get("http://" + addr + "/status")
	if err != nil {
		t.Fatal(err)
	}
	body, _ = io.ReadAll(resp.Body)
	resp.Body.Close()
	if string(body) != "198 slots free.\n2 slots in use.\n" {
		t.Fatalf("/status %q", body)
	}
	resp, err = http.Get("http://" + addr + "/healthz")
	if err != nil {
		t.Fatal(err)
	}
	resp.Body.Close()
	if resp.StatusCode != 200 {
		t.Fatalf("healthz %d", resp.StatusCode)
	}
	// And the control plane still works on the same port.
	c := dial(t, addr)
	c.send(hello("X"))
	c.expect("hello_ok")
}
