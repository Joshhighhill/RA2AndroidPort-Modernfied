package relay

import (
	"net"
	"testing"
	"time"
)

func udp(t *testing.T) *net.UDPConn {
	c, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		t.Fatal(err)
	}
	return c
}

func recv(t *testing.T, c *net.UDPConn) []byte {
	buf := make([]byte, 2048)
	c.SetReadDeadline(time.Now().Add(2 * time.Second))
	n, _, err := c.ReadFromUDP(buf)
	if err != nil {
		t.Fatalf("recv: %v", err)
	}
	return buf[:n]
}

func TestPortRelayForwardsBetweenBoundPeers(t *testing.T) {
	closed := make(chan struct{}, 1)
	p := NewPool(Config{BindIP: "127.0.0.1", Idle: 2 * time.Second})
	a, err := p.Allocate(func() { closed <- struct{}{} })
	if err != nil {
		t.Fatal(err)
	}
	relay := &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: a.Port}
	host, client, stranger := udp(t), udp(t), udp(t)
	defer host.Close()
	defer client.Close()
	defer stranger.Close()

	host.WriteToUDP(BindPacket(a.HostToken), relay)
	ack, ok := ParseBindAck(recv(t, host))
	if !ok || ack.Port != host.LocalAddr().(*net.UDPAddr).Port {
		t.Fatalf("host BIND_ACK must echo the observed mapping, got %v", ack)
	}
	client.WriteToUDP(BindPacket(a.ClientToken), relay)
	if _, ok := ParseBindAck(recv(t, client)); !ok {
		t.Fatalf("client BIND_ACK")
	}
	// Raw payload, no header, forwarded unchanged both ways.
	client.WriteToUDP([]byte("enet-connect"), relay)
	if got := string(recv(t, host)); got != "enet-connect" {
		t.Fatalf("host got %q", got)
	}
	host.WriteToUDP([]byte("enet-verify"), relay)
	if got := string(recv(t, client)); got != "enet-verify" {
		t.Fatalf("client got %q", got)
	}
	// A stranger (no token) cannot inject or bind.
	stranger.WriteToUDP([]byte("evil"), relay)
	var bogus Token
	stranger.WriteToUDP(BindPacket(bogus), relay)
	stranger.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
	if _, _, err := stranger.ReadFromUDP(make([]byte, 64)); err == nil {
		t.Fatalf("a stranger must never get a reply (no reflection)")
	}
	host.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
	if _, _, err := host.ReadFromUDP(make([]byte, 64)); err == nil {
		t.Fatalf("stranger's datagram must not be forwarded")
	}
	if a.Dropped.Load() < 2 {
		t.Fatalf("drops counted, got %d", a.Dropped.Load())
	}
	// Mobility: the client re-binds from a new socket.
	client2 := udp(t)
	defer client2.Close()
	client2.WriteToUDP(BindPacket(a.ClientToken), relay)
	recv(t, client2)
	host.WriteToUDP([]byte("after-handover"), relay)
	if got := string(recv(t, client2)); got != "after-handover" {
		t.Fatalf("re-bound client got %q", got)
	}
	// Idle timeout releases the port.
	select {
	case <-closed:
	case <-time.After(5 * time.Second):
		t.Fatalf("idle allocation was not released")
	}
	if p.InUse() != 0 {
		t.Fatalf("pool still has %d allocations", p.InUse())
	}
}

func TestPortRelayPoolRange(t *testing.T) {
	// A 2-port range: the third allocation fails, a release frees a port.
	base := 0
	for try := 0; try < 20 && base == 0; try++ {
		c, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
		if err != nil {
			t.Fatal(err)
		}
		cand := c.LocalAddr().(*net.UDPAddr).Port
		c.Close()
		c2, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: cand + 1})
		if err == nil {
			c2.Close()
			base = cand
		}
	}
	if base == 0 {
		t.Skip("no free consecutive ports")
	}
	p := NewPool(Config{BindIP: "127.0.0.1", PortLo: base, PortHi: base + 1})
	a1, err1 := p.Allocate(nil)
	a2, err2 := p.Allocate(nil)
	if err1 != nil || err2 != nil || a1.Port == a2.Port {
		t.Fatalf("two allocations: %v %v", err1, err2)
	}
	if _, err := p.Allocate(nil); err != ErrPoolExhausted {
		t.Fatalf("third must exhaust, got %v", err)
	}
	a1.Close()
	a3, err := p.Allocate(nil)
	if err != nil || a3.Port != a1.Port {
		t.Fatalf("released port reused: %v", err)
	}
	a2.Close()
	a3.Close()
}

func TestPortRelayRateLimit(t *testing.T) {
	p := NewPool(Config{BindIP: "127.0.0.1", PktsPerSec: 5})
	a, err := p.Allocate(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer a.Close()
	relay := &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: a.Port}
	host, client := udp(t), udp(t)
	defer host.Close()
	defer client.Close()
	host.WriteToUDP(BindPacket(a.HostToken), relay)
	recv(t, host)
	client.WriteToUDP(BindPacket(a.ClientToken), relay)
	recv(t, client)
	for i := 0; i < 20; i++ {
		client.WriteToUDP([]byte{byte(i)}, relay)
	}
	got := 0
	for {
		host.SetReadDeadline(time.Now().Add(300 * time.Millisecond))
		if _, _, err := host.ReadFromUDP(make([]byte, 16)); err != nil {
			break
		}
		got++
	}
	if got < 4 || got > 7 {
		t.Fatalf("burst of 5 pkt/s should pass ~5 of 20, got %d", got)
	}
}
