#pragma once
// G0 verification harness (`ra2engine --play <script>`): drives the REAL scene
// tree (run_menu -> ShellScene -> lobby/campaign -> GameScene) with scripted
// input at a deterministic fixed tick. See scripts/play/README.md.
//
// Coupling to the engine is three tiny hooks:
//   * run_scene_driver (main.cpp) calls g_play->pre_frame / on_event / on_quit /
//     dt / want_render / post_frame when g_play is set (null in normal play);
//   * Scene::play_probe(Probe&) -- each scene reports its state + click targets;
//   * main() dispatches `--play` to ra2play::run_play.
#include <functional>
#include <string>

#include "platform.hpp"
#include "play_script.hpp"

namespace ra2play {

using ProbeFn = std::function<void(Probe&)>;

class PlayDriver {
public:
    virtual ~PlayDriver() = default;
    // Before polling input for this frame: run script commands, inject events.
    // Returns false when the run is over (the driver loop must return).
    virtual bool pre_frame(ra2engine::Platform& plat, const ProbeFn& probe) = 0;
    // Every polled event passes here first (virtual timestamps for determinism).
    virtual void on_event(ra2engine::InputEvent& ev) = 0;
    // InputEvent::Quit reached the driver (the real program exits here).
    // Recorded as a failure unless expected. Returns true to SWALLOW it and
    // keep running (script `set on_quit continue`, so later steps can still
    // be checked); false = return from the driver like the real program.
    virtual bool on_quit() = 0;
    // The driver swapped scenes (Scene::next() returned non-null).
    virtual void on_transition() = 0;
    virtual double dt() const = 0;           // fixed logic step (default 1/15 s)
    virtual bool want_render() const = 0;    // render this frame?
    // After update + render: screenshots, per-15-frame probe/hash line.
    virtual void post_frame(ra2engine::Platform& plat, const ProbeFn& probe) = 0;
};

// Non-null only inside `--play`.
extern PlayDriver* g_play;

// Run `script_path` against `run_menu` (which must create its Platform AFTER
// this sets up the hidden video driver). `out_dir` receives the PNGs and the
// run log. Returns the process exit code: 0 all assertions passed, 1 any
// failed, 2 script/setup error, 3 exception.
int run_play(const std::string& script_path, const std::string& out_dir,
             const std::function<int()>& run_menu);

}  // namespace ra2play
