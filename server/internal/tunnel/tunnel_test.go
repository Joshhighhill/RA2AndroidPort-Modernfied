package tunnel

import (
	"bytes"
	"crypto/sha1"
	"net"
	"testing"
	"time"
)

type fakeClock struct{ t time.Time }

func (f *fakeClock) now() time.Time { return f.t }

func addr(s string) *net.UDPAddr {
	a, err := net.ResolveUDPAddr("udp4", s)
	if err != nil {
		panic(err)
	}
	return a
}

func pingPacket(header []byte) []byte {
	p := make([]byte, PingSize)
	copy(p, header)
	for i := len(header); i < PingSize; i++ {
		p[i] = byte(i * 7)
	}
	return p
}

func TestV3PingFormat(t *testing.T) {
	v := NewV3(Config{})
	p := pingPacket(V3Header(0, 0))
	out := v.Handle(p, addr("203.0.113.5:4000"))
	if len(out) != 1 || len(out[0].Data) != PingReply {
		t.Fatalf("want one 12-byte reply, got %+v", out)
	}
	if !bytes.Equal(out[0].Data, p[:12]) {
		t.Fatalf("reply must be the first 12 bytes of the request")
	}
	if out[0].To.String() != "203.0.113.5:4000" {
		t.Fatalf("reply goes back to the sender, got %v", out[0].To)
	}
	// Not 50 bytes -> not a ping.
	if out := v.Handle(p[:49], addr("203.0.113.5:4000")); len(out) != 0 {
		t.Fatalf("49-byte 0/0 packet must be dropped")
	}
}

func TestV3PingRateLimit(t *testing.T) {
	clk := &fakeClock{t: time.Unix(1000, 0)}
	v := NewV3(Config{Now: clk.now})
	p := pingPacket(V3Header(0, 0))
	from := addr("198.51.100.1:5000")
	got := 0
	for i := 0; i < 30; i++ {
		got += len(v.Handle(p, from))
	}
	if got != 20 {
		t.Fatalf("20 pings per IP per minute, got %d replies", got)
	}
	clk.t = clk.t.Add(61 * time.Second)
	if len(v.Handle(p, from)) != 1 {
		t.Fatalf("limit resets after a minute")
	}
}

func TestV3RegisterAndForward(t *testing.T) {
	v := NewV3(Config{})
	a, b := addr("192.0.2.10:1111"), addr("192.0.2.20:2222")
	// A's first packet registers A; B is unknown -> dropped.
	if out := v.Handle(append(V3Header(0xA1, 0xB2), 'x'), a); len(out) != 0 {
		t.Fatalf("receiver unknown: drop")
	}
	// B registers and talks to A: forwarded verbatim, header included.
	pkt := append(V3Header(0xB2, 0xA1), []byte("hello")...)
	out := v.Handle(pkt, b)
	if len(out) != 1 || out[0].To.String() != a.String() || !bytes.Equal(out[0].Data, pkt) {
		t.Fatalf("forward to A failed: %+v", out)
	}
	// And back.
	pkt2 := append(V3Header(0xA1, 0xB2), []byte("hi")...)
	out = v.Handle(pkt2, a)
	if len(out) != 1 || out[0].To.String() != b.String() {
		t.Fatalf("forward to B failed")
	}
	if v.Clients() != 2 {
		t.Fatalf("2 clients registered, got %d", v.Clients())
	}
}

func TestV3Drops(t *testing.T) {
	v := NewV3(Config{})
	a := addr("192.0.2.10:1111")
	cases := map[string][]byte{
		"short":         {1, 2, 3},
		"sender==recv":  V3Header(5, 5),
		"receiver zero": V3Header(5, 0),
		"too big":       make([]byte, MaxPacket+1),
	}
	for name, p := range cases {
		if len(v.Handle(p, a)) != 0 {
			t.Errorf("%s: must drop", name)
		}
	}
	// Loopback refused by default, allowed by flag.
	v.Handle(V3Header(7, 8), addr("127.0.0.1:9"))
	if v.Clients() != 0 {
		t.Errorf("loopback sender must not register by default")
	}
	vl := NewV3(Config{AllowLoopback: true})
	vl.Handle(V3Header(7, 8), addr("127.0.0.1:9"))
	if vl.Clients() != 1 {
		t.Errorf("AllowLoopback registers loopback senders")
	}
	if len(v.Handle(V3Header(7, 8), addr("0.0.0.0:9"))) != 0 || len(v.Handle(V3Header(7, 8), addr("1.2.3.4:0"))) != 0 {
		t.Errorf("any / port 0 must drop")
	}
}

func TestV3RebindOnlyAfterTimeout(t *testing.T) {
	clk := &fakeClock{t: time.Unix(1000, 0)}
	v := NewV3(Config{Now: clk.now, Timeout: 30 * time.Second})
	a, a2, b := addr("192.0.2.10:1111"), addr("192.0.2.99:3333"), addr("192.0.2.20:2222")
	v.Handle(V3Header(1, 2), a)
	v.Handle(V3Header(2, 1), b)
	// Spoof from a new endpoint while A is live: refused.
	if len(v.Handle(V3Header(1, 2), a2)) != 0 {
		t.Fatalf("live mapping must not be stolen")
	}
	clk.t = clk.t.Add(31 * time.Second)
	out := v.Handle(V3Header(1, 2), a2)
	if len(out) != 1 {
		t.Fatalf("after the timeout the id re-binds (NAT rebinding)")
	}
	out = v.Handle(V3Header(2, 1), b)
	if len(out) != 1 || out[0].To.String() != a2.String() {
		t.Fatalf("B now reaches A at its new endpoint")
	}
}

func TestV3Limits(t *testing.T) {
	v := NewV3(Config{MaxClients: 3, IPLimit: 2})
	ip := "192.0.2.1:"
	v.Handle(V3Header(1, 9), addr(ip+"1"))
	v.Handle(V3Header(2, 9), addr(ip+"2"))
	v.Handle(V3Header(3, 9), addr(ip+"3")) // IP limit 2
	if v.Clients() != 2 {
		t.Fatalf("ip limit: want 2 got %d", v.Clients())
	}
	v.Handle(V3Header(4, 9), addr("192.0.2.2:1"))
	v.Handle(V3Header(5, 9), addr("192.0.2.3:1")) // max clients 3
	if v.Clients() != 3 {
		t.Fatalf("max clients: want 3 got %d", v.Clients())
	}
}

func TestV3SweepAndMaintenance(t *testing.T) {
	clk := &fakeClock{t: time.Unix(1000, 0)}
	v := NewV3(Config{Now: clk.now, Timeout: 10 * time.Second, MaintPassword: "s3cret"})
	v.Handle(V3Header(1, 2), addr("192.0.2.1:1"))
	clk.t = clk.t.Add(11 * time.Second)
	if v.Sweep() != 1 || v.Clients() != 0 {
		t.Fatalf("idle mapping swept")
	}
	h := sha1.Sum([]byte("wrong"))
	v.Handle(append(append(V3Header(0, 0xFFFFFFFF), 0), h[:]...), addr("192.0.2.1:1"))
	if v.Maintenance() {
		t.Fatalf("wrong password must not toggle")
	}
	h = sha1.Sum([]byte("s3cret"))
	v.Handle(append(append(V3Header(0, 0xFFFFFFFF), 0), h[:]...), addr("192.0.2.1:1"))
	if !v.Maintenance() {
		t.Fatalf("maintenance on")
	}
	v.Handle(V3Header(1, 2), addr("192.0.2.1:1"))
	if v.Clients() != 0 {
		t.Fatalf("no registration during maintenance")
	}
}

func TestV2RequestAndForward(t *testing.T) {
	n := uint16(100)
	v := NewV2(Config{}, func() uint16 { n++; return n })
	ids := v.Request(2, "192.0.2.1")
	if len(ids) != 2 || ids[0] == ids[1] || ids[0] == 0 {
		t.Fatalf("bad ids %v", ids)
	}
	a, b := addr("192.0.2.1:1000"), addr("192.0.2.2:2000")
	// Unallocated sender: drop.
	if len(v.Handle(append(V2Header(999, ids[0]), 'x'), a)) != 0 {
		t.Fatalf("unallocated sender must drop")
	}
	v.Handle(append(V2Header(ids[0], ids[1]), 'x'), a) // binds ids[0]; ids[1] unbound -> drop
	pkt := append(V2Header(ids[1], ids[0]), []byte("data")...)
	out := v.Handle(pkt, b)
	if len(out) != 1 || out[0].To.String() != a.String() || !bytes.Equal(out[0].Data, pkt) {
		t.Fatalf("V2 forward failed: %+v", out)
	}
	// Header is big-endian.
	if pkt[0] != byte(uint16(ids[1])>>8) || pkt[1] != byte(uint16(ids[1])) {
		t.Fatalf("V2 header must be big-endian")
	}
	free, used := v.Slots()
	if used != 2 || free != 198 {
		t.Fatalf("slots %d/%d", free, used)
	}
}

func TestV2PingAndLimits(t *testing.T) {
	v := NewV2(Config{IPLimitV2: 2}, func() uint16 { return 7 })
	p := pingPacket(V2Header(0, 0))
	out := v.Handle(p, addr("192.0.2.1:1"))
	if len(out) != 1 || !bytes.Equal(out[0].Data, p[:12]) {
		t.Fatalf("V2 ping")
	}
	if v.Request(1, "x") != nil || v.Request(9, "x") != nil {
		t.Fatalf("clients must be 2..8")
	}
	// rnd always 7: second id cannot be found -> refused, nothing leaked.
	if v.Request(2, "y") != nil {
		t.Fatalf("collision exhaustion refuses")
	}
	if _, used := v.Slots(); used != 0 {
		t.Fatalf("refused request must not leak ids")
	}
}

// Real sockets: two UDP clients exchange datagrams through a V3 tunnel on
// loopback (the relay-forwarding acceptance test).
func TestV3ServeLoopback(t *testing.T) {
	srv, err := net.ListenUDP("udp4", addr("127.0.0.1:0"))
	if err != nil {
		t.Fatal(err)
	}
	defer srv.Close()
	go Serve(srv, NewV3(Config{AllowLoopback: true}), time.Second)
	ca, _ := net.ListenUDP("udp4", addr("127.0.0.1:0"))
	cb, _ := net.ListenUDP("udp4", addr("127.0.0.1:0"))
	defer ca.Close()
	defer cb.Close()
	to := srv.LocalAddr().(*net.UDPAddr)
	// Register A (to an unknown B) then B -> A, then A -> B.
	ca.WriteToUDP(V3Header(11, 22), to)
	time.Sleep(50 * time.Millisecond)
	cb.WriteToUDP(append(V3Header(22, 11), []byte("from-b")...), to)
	buf := make([]byte, 64)
	ca.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err := ca.ReadFromUDP(buf)
	if err != nil || string(buf[8:n]) != "from-b" {
		t.Fatalf("A did not receive B's datagram: %v %q", err, buf[:n])
	}
	ca.WriteToUDP(append(V3Header(11, 22), []byte("from-a")...), to)
	cb.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err = cb.ReadFromUDP(buf)
	if err != nil || string(buf[8:n]) != "from-a" {
		t.Fatalf("B did not receive A's datagram: %v", err)
	}
	// Ping over the socket.
	ca.WriteToUDP(pingPacket(V3Header(0, 0)), to)
	ca.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err = ca.ReadFromUDP(buf)
	if err != nil || n != 12 {
		t.Fatalf("ping reply: %v n=%d", err, n)
	}
}
