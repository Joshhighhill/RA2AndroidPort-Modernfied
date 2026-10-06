#pragma once
// G0 verification harness (--play): the pure, SDL-free half -- the script
// parser, the assertion evaluator over a scene probe, and the play_hash.
// The driver half (scripted events through the REAL scene tree) lives in
// engine/play_driver.inc, included by main.cpp.
//
// Script format: one command per line, '#' starts a comment, blank lines are
// ignored. Every INPUT command (click/drag/key/move/select) ends the current
// frame, so the scene sees its events and gets one update/render before the
// next command runs (a page/scene transition is therefore always settled).
//
//   wait N                         advance N frames
//   click x y [right]              click window pixel (x, y)
//   click button <Name...>         a shell/lobby/game button by its label
//   click cell cx cy [right]       the centre of map cell (cx, cy)
//   click cameo <ID>               a sidebar cameo by rules id (switches tab)
//   click unit H:IMAGE[:n] [right] the n-th (0-based) alive unit of house H
//   select unit H:IMAGE[:n]        = click unit (left)
//   drag x1 y1 x2 y2               left-drag in window pixels
//   drag cell cx1 cy1 cx2 cy2      left-drag between two cells
//   move x y                       pointer move only
//   key <name> [ctrl] [shift] [alt]  SDL key name ("Escape", "S", "1", "F10")
//   shot <file.png>                capture the frame (relative to --play-out)
//   assert <key> <op> <int>        op: == != < <= > >=  (probe key, see below)
//   until N <key> <op> <int>       wait up to N frames for the condition
//   cheat <args...>                scene cheat (GameScene: kill_house H)
//   expect quit                    a later Quit/exit is the expected result
//   end                            stop; PASS if no assertion failed
//
// Probe keys are scene-defined integers (e.g. "scene.game", "menu", "sel",
// "units.0", "units.0.GI", "credits"); a missing key evaluates as 0.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ra2engine {
namespace play {

struct Command {
    std::string op;                 // "wait", "click", ...
    std::vector<std::string> args;  // whitespace-split remainder
    int line = 0;                   // 1-based source line
};

using Probe = std::map<std::string, long long>;

// Parse a script. Unknown ops are a parse error (err = "line N: ..."), so a
// typo never silently passes.
bool parse_script(const std::string& text, std::vector<Command>& out, std::string& err);

// Evaluate "<key> <op> <int>" (three tokens) against a probe. Returns false
// on a malformed expression (err set); `result` is the comparison outcome.
bool eval_assert(const std::vector<std::string>& expr, const Probe& p, bool& result,
                 std::string& err);

// One probe as a stable text line: "key=value key=value ..." (map order).
std::string probe_line(const Probe& p);

// 64-bit FNV-1a, chained (play_hash folds every printed play_state line).
uint64_t fnv1a(const std::string& s, uint64_t h = 1469598103934665603ULL);

// Parse "H:IMAGE[:n]" (unit selector). Returns false when malformed.
bool parse_unit_selector(const std::string& s, int& house, std::string& image, int& n);

}  // namespace play
}  // namespace ra2engine
