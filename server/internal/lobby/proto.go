// Package lobby is ra2lobbyd's control plane: a version gate, the room
// directory, 6-character room codes and the address exchange that tells a
// joining client how to reach the host through the relay. It runs NO game
// logic: slots, colours, maps and the start all live in the host player's
// game (the in-room lobby speaks the original 0x1236 global-channel framing
// through the tunnel; see engine/mp/).
//
// Wire format: one JSON object per line ("\n"), at most MaxLine bytes, over
// TCP. The type is the "t" field. The first client message must be "hello".
// The same TCP port also answers HTTP (the CnCNet V2 tunnel's /request and
// /status, plus /healthz), told apart by the first bytes of the stream.
//
//	C->S hello   {magic:"RA2N", proto, ver, sim, rules, plat, pid, nick}
//	S->C hello_ok{server, motd, proto_min, proto_max, v3_port, v2_port, public_ip}
//	S->C reject  {reason: version|bad_name|banned|busy|protocol, detail}   (then close)
//	C->S list    {}                                -> rooms {rooms:[RoomInfo]}
//	C->S create  {name, max, private, pw, map, v3} -> created {code, v3_port}
//	C->S update  {players, max, map, started}
//	C->S close   {}
//	C->S join    {code, v3, relay:"v3"|"port"}     -> joined {...} | error {op, reason}
//	S->C peer    {cid, nick, pid, v3, relay_port, relay_token}       (to the host)
//	S->C room_closed {code}                         (to joined clients)
//	C->S ping    {}                                -> pong {}
//	S->C notice  {text}
package lobby

// ProtoMin/ProtoMax: the net_proto range this server accepts (bump on any
// wire change; old clients get a clear "please update").
const (
	ProtoMin = 2
	ProtoMax = 2
	Magic    = "RA2N"
	MaxLine  = 4096
)

// Msg is the union of every control-plane message (unused fields omitted).
type Msg struct {
	T string `json:"t"`

	// hello
	Magic string `json:"magic,omitempty"`
	Proto int    `json:"proto,omitempty"`
	Ver   string `json:"ver,omitempty"`
	Sim   uint32 `json:"sim,omitempty"`
	Rules string `json:"rules,omitempty"`
	Plat  int    `json:"plat,omitempty"`
	PID   string `json:"pid,omitempty"`
	Nick  string `json:"nick,omitempty"`

	// hello_ok
	Server   string `json:"server,omitempty"`
	MOTD     string `json:"motd,omitempty"`
	ProtoLo  int    `json:"proto_min,omitempty"`
	ProtoHi  int    `json:"proto_max,omitempty"`
	V3Port   int    `json:"v3_port,omitempty"`
	V2Port   int    `json:"v2_port,omitempty"`
	PublicIP string `json:"public_ip,omitempty"`

	// reject / error / notice
	Reason string `json:"reason,omitempty"`
	Detail string `json:"detail,omitempty"`
	Op     string `json:"op,omitempty"`
	Text   string `json:"text,omitempty"`

	// rooms
	Rooms []RoomInfo `json:"rooms,omitempty"`

	// create / update / join
	Name    string `json:"name,omitempty"`
	Max     int    `json:"max,omitempty"`
	Private bool   `json:"private,omitempty"`
	PW      bool   `json:"pw,omitempty"`
	Map     string `json:"map,omitempty"`
	V3      uint32 `json:"v3,omitempty"`
	Players int    `json:"players,omitempty"`
	Started bool   `json:"started,omitempty"`
	Code    string `json:"code,omitempty"`
	Relay   string `json:"relay,omitempty"`

	// joined / peer
	CID        int    `json:"cid,omitempty"`
	HostNick   string `json:"host_nick,omitempty"`
	HostV3     uint32 `json:"host_v3,omitempty"`
	RelayPort  int    `json:"relay_port,omitempty"`
	RelayToken string `json:"relay_token,omitempty"`
}

// RoomInfo is one row of the public room list. Private rooms are never
// listed (join by code only).
type RoomInfo struct {
	Code    string `json:"code"`
	Name    string `json:"name"`
	Host    string `json:"host"`
	Map     string `json:"map"`
	Players int    `json:"players"`
	Max     int    `json:"max"`
	Started bool   `json:"started"`
	PW      bool   `json:"pw"`
	Sim     uint32 `json:"sim"`
	Rules   string `json:"rules"`
	Plat    int    `json:"plat"`
}
