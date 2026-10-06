#pragma once
// G0 verification harness -- the SDL-free half: the `.play` script parser, the
// state-probe record every Scene fills, and the assertion expression evaluator.
// Unit-tested by tests/test_play_script.cpp; driven by engine/playtest.cpp.
// The script format is documented in scripts/play/README.md.
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ra2play {

// One script line, tokenised. `op` is the first word (lower-cased); `args` the
// remaining words (a "double quoted" run is one arg, quotes stripped); `tags`
// collects the trailing `@G-20`/`@owner:A` annotations; `expr` is the raw
// expression text for assert/until/require/let.
struct Cmd {
    int line = 0;
    std::string op;
    std::vector<std::string> args;
    std::vector<std::string> tags;
    std::string expr;
    int n = 0;          // until N / wait N / hold KEY N
    std::string raw;    // the source line (trimmed, without the comment)
};

struct Script {
    std::string name;
    std::vector<Cmd> cmds;
};

// Parse a whole script. Returns false and sets `err` ("line N: ...") on the
// first malformed line. Unknown ops are a parse error (typos must not silently
// pass a run).
bool parse_script(const std::string& text, Script& out, std::string& err);

// ---- probe -----------------------------------------------------------------
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
};

// The flat state a scene reports. Keys are `name` or `name(arg,arg)`. A key
// whose family (the part before '(') is in `zero_families` reads 0 when absent
// (e.g. `units(1,E2)` with no E2 alive); any other absent key is MISSING --
// the run reports `missing-probe:<key>` instead of crashing, so scripts written
// for a later phase degrade gracefully on an older tree.
struct ProbeData {
    std::map<std::string, double> n;
    std::map<std::string, std::string> s;
    std::set<std::string> zero_families;
    void set(const std::string& k, double v) { n[k] = v; }
    void set_s(const std::string& k, const std::string& v) { s[k] = v; }
    void add(const std::string& k, double v) { n[k] += v; }
};

// What a Scene hands the harness each time it is asked (Scene::play_probe).
// `targets` are named clickable rects in window pixels ("button:Skirmish",
// "start", "cameo:GAPOWR", "tab:2", "menu:SAVE GAME", "dialog:action"). The
// hooks are optional (null when the scene has no world).
struct Probe : ProbeData {
    std::string scene = "unknown";
    std::map<std::string, Rect> targets;
    // Map cell centre -> window pixel (false when off the playfield view).
    std::function<bool(double cx, double cy, int& sx, int& sy)> cell_to_screen;
    // Scroll the view so cell (cx,cy) is centred (camera pan, as the arrow keys would).
    std::function<void(double cx, double cy)> center_on;
    // "H:TYPE[:n]" -> the n-th (0-based) live unit / building of that house+type.
    std::function<bool(const std::string& spec, double& cx, double& cy)> find_unit;
    std::function<bool(const std::string& spec, double& cx, double& cy)> find_building;
    // The nearest valid origin cell for the currently armed placement.
    std::function<bool(int& cx, int& cy)> auto_place;
    // Dev cheats: returns "" on success or an error string.
    std::function<std::string(const std::vector<std::string>& args)> cheat;
};

// ---- expressions -----------------------------------------------------------
struct Value {
    bool is_num = true;
    double num = 0.0;
    std::string str;
};

enum class Eval { True, False, Missing, Error };

// `expr` := clause { ("and"|"or") clause }   (left to right, no precedence)
// clause := operand OP operand | operand        (bare operand: non-zero / non-empty)
// OP     := == != < <= > >=
// operand:= number | "string" | $var | probe-key | bare word
// A bare word that is not a probe key is a string literal on the right-hand
// side and MISSING on the left. `detail` gets a human-readable account
// ("credits=10000 > 10000").
Eval eval_expr(const std::string& expr, const ProbeData& p,
               const std::map<std::string, Value>& vars, std::string& detail);

// Resolve one operand (exposed for `let`). Returns false when missing.
bool resolve_operand(const std::string& tok, const ProbeData& p,
                     const std::map<std::string, Value>& vars, bool lhs, Value& out);

// Deterministic FNV-1a over a probe dump (sorted keys; numbers printed %.6g).
std::uint64_t probe_hash(const ProbeData& p, std::uint64_t seed);
std::string probe_dump(const ProbeData& p);

}  // namespace ra2play
