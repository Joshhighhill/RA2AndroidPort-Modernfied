// Package limit holds the small abuse-protection helpers shared by the
// control plane and the tunnels: token buckets, per-key sliding counters and
// per-IP concurrency counters. Everything is in memory and safe for
// concurrent use. (docs/gap/net/05-transport-infra.md §4.8)
package limit

import (
	"net"
	"sync"
	"time"
)

// Clock lets tests drive time.
type Clock func() time.Time

// Bucket is a classic token bucket: rate tokens per second, capacity burst.
type Bucket struct {
	mu     sync.Mutex
	rate   float64
	burst  float64
	tokens float64
	last   time.Time
	now    Clock
}

func NewBucket(rate, burst float64, now Clock) *Bucket {
	if now == nil {
		now = time.Now
	}
	return &Bucket{rate: rate, burst: burst, tokens: burst, last: now(), now: now}
}

// Allow takes one token; false when the bucket is empty.
func (b *Bucket) Allow() bool { return b.AllowN(1) }

func (b *Bucket) AllowN(n float64) bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	t := b.now()
	el := t.Sub(b.last).Seconds()
	if el > 0 {
		b.tokens += el * b.rate
		if b.tokens > b.burst {
			b.tokens = b.burst
		}
	}
	b.last = t
	if b.tokens < n {
		return false
	}
	b.tokens -= n
	return true
}

// Window counts events per key in fixed windows (e.g. "20 pings per IP per
// minute", "10 join attempts per IP per minute").
type Window struct {
	mu     sync.Mutex
	limit  int
	period time.Duration
	start  time.Time
	counts map[string]int
	now    Clock
}

func NewWindow(limit int, period time.Duration, now Clock) *Window {
	if now == nil {
		now = time.Now
	}
	return &Window{limit: limit, period: period, start: now(), counts: map[string]int{}, now: now}
}

// Allow records one event for key; false when key is over its limit for the
// current window. A limit <= 0 disables the check.
func (w *Window) Allow(key string) bool {
	if w.limit <= 0 {
		return true
	}
	w.mu.Lock()
	defer w.mu.Unlock()
	t := w.now()
	if t.Sub(w.start) >= w.period {
		w.start = t
		w.counts = map[string]int{}
	}
	if w.counts[key] >= w.limit {
		return false
	}
	w.counts[key]++
	return true
}

// Counter tracks how many live things (connections, rooms, allocations) each
// key owns.
type Counter struct {
	mu    sync.Mutex
	limit int
	n     map[string]int
}

func NewCounter(limit int) *Counter { return &Counter{limit: limit, n: map[string]int{}} }

// Acquire increments key unless it is at the limit (limit <= 0: unlimited).
func (c *Counter) Acquire(key string) bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.limit > 0 && c.n[key] >= c.limit {
		return false
	}
	c.n[key]++
	return true
}

func (c *Counter) Release(key string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.n[key] <= 1 {
		delete(c.n, key)
		return
	}
	c.n[key]--
}

func (c *Counter) Get(key string) int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.n[key]
}

// IPKey is the per-IP key of an address (port dropped).
func IPKey(a net.Addr) string {
	switch v := a.(type) {
	case *net.UDPAddr:
		return v.IP.String()
	case *net.TCPAddr:
		return v.IP.String()
	}
	h, _, err := net.SplitHostPort(a.String())
	if err != nil {
		return a.String()
	}
	return h
}

// Truncate renders an IP for logs with the host part dropped (/24 for IPv4,
// /48 for IPv6): the server never logs full client addresses (§4.8, §8).
func Truncate(ip string) string {
	p := net.ParseIP(ip)
	if p == nil {
		return "?"
	}
	if v4 := p.To4(); v4 != nil {
		return net.IPv4(v4[0], v4[1], v4[2], 0).String() + "/24"
	}
	m := net.CIDRMask(48, 128)
	return p.Mask(m).String() + "/48"
}
