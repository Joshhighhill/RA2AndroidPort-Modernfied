package lobby

import (
	"encoding/json"
	"os"
	"strings"
	"sync"
	"time"
)

// Bans is the optional ban list (bans.json: {"ips":["1.2.3.4"],"pids":["0123456789abcdef"]}).
// It is re-read when the file's modification time changes (checked at most
// every 10 s), so an admin edits the file instead of restarting the server.
type Bans struct {
	path    string
	mu      sync.Mutex
	ips     map[string]bool
	pids    map[string]bool
	mtime   time.Time
	checked time.Time
}

type banFile struct {
	IPs  []string `json:"ips"`
	PIDs []string `json:"pids"`
}

// LoadBans reads path ("" = no bans). A missing file is an empty list.
func LoadBans(path string) *Bans {
	b := &Bans{path: path, ips: map[string]bool{}, pids: map[string]bool{}}
	b.reload(true)
	return b
}

func (b *Bans) reload(force bool) {
	if b.path == "" {
		return
	}
	now := time.Now()
	if !force && now.Sub(b.checked) < 10*time.Second {
		return
	}
	b.checked = now
	st, err := os.Stat(b.path)
	if err != nil || (!force && st.ModTime().Equal(b.mtime)) {
		return
	}
	data, err := os.ReadFile(b.path)
	if err != nil {
		return
	}
	var f banFile
	if json.Unmarshal(data, &f) != nil {
		return
	}
	b.mtime = st.ModTime()
	b.ips, b.pids = map[string]bool{}, map[string]bool{}
	for _, ip := range f.IPs {
		b.ips[strings.TrimSpace(ip)] = true
	}
	for _, p := range f.PIDs {
		b.pids[strings.ToLower(strings.TrimSpace(p))] = true
	}
}

// Banned reports whether the IP or the player id is banned.
func (b *Bans) Banned(ip, pid string) bool {
	if b == nil {
		return false
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	b.reload(false)
	return b.ips[ip] || b.pids[strings.ToLower(pid)]
}
