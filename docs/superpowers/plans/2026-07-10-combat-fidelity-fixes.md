# Combat Fidelity Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the two live-play fidelity bugs — single-file attackers and wrong (death-frame) fire/die animations — by driving infantry animation from the real artmd.ini with 8-way facing, and by assigning each attacker its own firing position.

**Architecture:** New thin `ra2assets::Art` reader (mirrors `Rules`); `UnitAnimSeq` reshaped to per-sequence `{first,count,fmult}` entries with a facing-aware frame formula; combat pursuit emits `approach` MoveRequests that `World::step` batch-resolves to distinct free cells via the existing `assign_formation`. Calibration authority: `docs/re/infantry-anim.md` + `docs/re/combat.md`.

**Tech Stack:** C++17, doctest, CMake+Ninja+MinGW; pure modules shared with the Android build via `cmake/ra2_sources.cmake`.

## Global Constraints

- Calibration values are BINDING: Ready=`0,1,1`, Walk=`8,6,6`, FireUp=`164,6,6`, Die1=`134,15,0` for BOTH GISequence and ConSequence (docs/re/infantry-anim.md §2); facing blocks counterclockwise-from-North N=0,NW=1,W=2,SW=3,S=4,SE=5,E=6,NE=7 (§4); cell-dir→facing table (§5): (0,−1)→7, (1,−1)→6, (1,0)→5, (1,1)→4, (0,1)→3, (−1,1)→2, (−1,0)→1, (−1,−1)→0.
- Die anim plays at 15 fps (new kDieAnimFps=15.0), 15 frames over kDieAnimSecs=1.0 exactly; fire anim unchanged (6 frames, 0.4 s, 15 fps).
- Tests must skip cleanly (MESSAGE + return) without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`); doctest `CHECK_FALSE((a && b))` paren convention.
- No game assets / GPL code / Ghidra project committed. Run test exes from PowerShell.
- Every pure-module file added must appear in `cmake/ra2_sources.cmake` (shared with the Android jni build) exactly as `src/rules.cpp` does.
- Repo `D:\ra2-android`, work on `master` (solo, no remote), frequent commits.

---

### Task 1: `ra2assets::Art` — artmd.ini sequence reader

**Files:**
- Create: `include/ra2assets/art.hpp`, `src/art.cpp`, `tests/test_art.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `src/art.cpp` beside `src/rules.cpp`), `CMakeLists.txt` (add `tests/test_art.cpp` to `ra2assets_tests` beside `tests/test_rules.cpp`)

**Interfaces:**
- Consumes: `ra2assets::Ini` (`ini.hpp`: `get(section,key,def)`, case-insensitive sections/keys, values raw).
- Produces: `struct ArtSequenceEntry { int start=0; int count=1; int facing_mult=0; };` and `class Art { static Art load(const Ini&); std::string sequence_name(const std::string& image) const; ArtSequenceEntry entry(const std::string& image, const std::string& name) const; }` — Task 4 consumes these verbatim.

- [ ] **Step 1: Write the failing tests** (`tests/test_art.cpp`)

```cpp
#include "doctest.h"
#include "ra2assets/art.hpp"
#include "ra2assets/ini.hpp"

using ra2assets::Art;
using ra2assets::ArtSequenceEntry;
using ra2assets::Ini;

static const char* kSyntheticArt =
    "[GI] ; GI\n"
    "Sequence=GISequence\n"
    "\n"
    "[GISequence]\n"
    "Ready=0,1,1\n"
    "Walk=8,6,6\n"
    "FireUp=164,6,6\n"
    "Die1=134,15,0\n"
    "Idle1=56,15,0,S\n"          // 4th field (facing label) must be tolerated
    "Cheer= 293 , 8 , 0 ,E\n";   // embedded spaces tolerated

TEST_CASE("art: sequence_name resolves [image] Sequence=") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    CHECK(art.sequence_name("GI") == "GISequence");
    CHECK(art.sequence_name("gi") == "GISequence");  // section lookup case-insensitive
    CHECK(art.sequence_name("NOPE") == "");
}

TEST_CASE("art: entry parses Start,Count,FacingMult and tolerates extras") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    ArtSequenceEntry fire = art.entry("GI", "FireUp");
    CHECK(fire.start == 164);
    CHECK(fire.count == 6);
    CHECK(fire.facing_mult == 6);
    ArtSequenceEntry die = art.entry("GI", "Die1");
    CHECK(die.start == 134);
    CHECK(die.count == 15);
    CHECK(die.facing_mult == 0);
    ArtSequenceEntry idle = art.entry("GI", "Idle1");   // 4-field line
    CHECK(idle.start == 56);
    CHECK(idle.count == 15);
    CHECK(idle.facing_mult == 0);
    ArtSequenceEntry cheer = art.entry("GI", "Cheer");  // spaced fields
    CHECK(cheer.start == 293);
    CHECK(cheer.count == 8);
    CHECK(cheer.facing_mult == 0);
}

TEST_CASE("art: absent image/section/key -> defaults {0,1,0}") {
    Ini ini = Ini::parse(kSyntheticArt);
    Art art = Art::load(ini);
    ArtSequenceEntry e = art.entry("NOPE", "FireUp");
    CHECK(e.start == 0);
    CHECK(e.count == 1);
    CHECK(e.facing_mult == 0);
    ArtSequenceEntry e2 = art.entry("GI", "NoSuchEntry");
    CHECK(e2.start == 0);
    CHECK(e2.count == 1);
    CHECK(e2.facing_mult == 0);
}
```

Then the install-guarded real pin (same skip pattern as `tests/test_rules.cpp`'s real-asset test — locate artmd.ini in `ra2md.mix` top level, else the SAME nested candidate list `load_rules_ini` uses in `engine/main.cpp:150` — copy the guard + fallback loop shape from `test_rules.cpp`):

```cpp
TEST_CASE("art: real artmd.ini pins (GISequence == ConSequence for our entries)") {
    // ... RA2_INSTALL_DIR guard exactly like test_rules.cpp (MESSAGE + return) ...
    // ... open ra2md.mix, find artmd.ini top-level else in the nested candidates ...
    Ini ini = Ini::parse(std::string(bytes.begin(), bytes.end()));
    Art art = Art::load(ini);
    CHECK(art.sequence_name("GI") == "GISequence");
    CHECK(art.sequence_name("CONS") == "ConSequence");
    for (const char* img : {"GI", "CONS"}) {
        CHECK(art.entry(img, "Ready").start == 0);
        CHECK(art.entry(img, "Ready").count == 1);
        CHECK(art.entry(img, "Ready").facing_mult == 1);
        CHECK(art.entry(img, "Walk").start == 8);
        CHECK(art.entry(img, "Walk").count == 6);
        CHECK(art.entry(img, "Walk").facing_mult == 6);
        CHECK(art.entry(img, "FireUp").start == 164);
        CHECK(art.entry(img, "FireUp").count == 6);
        CHECK(art.entry(img, "FireUp").facing_mult == 6);
        CHECK(art.entry(img, "Die1").start == 134);
        CHECK(art.entry(img, "Die1").count == 15);
        CHECK(art.entry(img, "Die1").facing_mult == 0);
    }
}
```

- [ ] **Step 2: Run to verify failure** — `cmake --build build && ./build/ra2assets_tests.exe -tc="art:*"` (from PowerShell, repo root). Expected: compile FAILURE (art.hpp missing).

- [ ] **Step 3: Implement** (`include/ra2assets/art.hpp` — the exact header from **Interfaces** above with doc comments mirroring rules.hpp's style; `src/art.cpp`):

```cpp
#include "ra2assets/art.hpp"
#include <cstdlib>

namespace ra2assets {

Art Art::load(const Ini& ini) { return Art(ini); }

std::string Art::sequence_name(const std::string& image) const {
    return ini_->get(image, "Sequence", "");
}

ArtSequenceEntry Art::entry(const std::string& image, const std::string& name) const {
    ArtSequenceEntry e;
    const std::string seq = sequence_name(image);
    if (seq.empty()) return e;
    const std::string v = ini_->get(seq, name, "");
    if (v.empty()) return e;
    // Parse up to three leading comma-separated integers; later fields
    // (facing labels, sound tags -- e.g. "Idle1=56,15,0,S") are ignored.
    int out[3] = {0, 1, 0};
    std::size_t pos = 0;
    for (int i = 0; i < 3 && pos <= v.size(); ++i) {
        const std::size_t comma = v.find(',', pos);
        const std::string field = v.substr(pos, comma == std::string::npos ? std::string::npos
                                                                           : comma - pos);
        out[i] = std::atoi(field.c_str());
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    e.start = out[0];
    e.count = out[1] > 0 ? out[1] : 1;
    e.facing_mult = out[2];
    return e;
}

}  // namespace ra2assets
```

- [ ] **Step 4: Wire the build** — add `src/art.cpp` to `cmake/ra2_sources.cmake` next to `src/rules.cpp`; add `tests/test_art.cpp` next to `tests/test_rules.cpp` in `CMakeLists.txt`.
- [ ] **Step 5: Run tests** — `-tc="art:*"` all pass WITH `RA2_INSTALL_DIR` set; then unset it in the shell and confirm the real-pin case SKIPS with a message. Run the FULL suite focused check is not needed (Task-scope only).
- [ ] **Step 6: Commit** — `git add -A && git commit -m "feat(assets): Art reader for artmd.ini infantry sequences"`.

---

### Task 2: Facing + data-driven animation in the engine

**Files:**
- Modify: `engine/world.hpp` (SeqEntry/UnitAnimSeq reshape, AnimTable, facing helpers, `Unit::facing`, `World::set_anim`, `step_units` signature), `engine/world.cpp` (default table = REAL values, facing selection, frame formula, die at 15 fps), `tests/test_world.cpp` (update Task-7-era pins to real frames + facing), `tests/test_movement.cpp` / `tests/test_group_move.cpp` (only if they call `step_units` / `anim_for_image` directly — pass the new arg).
- Test: `tests/test_world.cpp` (new facing cases appended).

**Interfaces:**
- Consumes: nothing new (Art wiring happens in Task 4 — this task is engine-internal, defaults carry the real values).
- Produces (Task 3/4 rely on these EXACT names):

```cpp
struct SeqEntry { int first = 0; int count = 1; int fmult = 0; };
struct UnitAnimSeq { SeqEntry stand, walk, fire, die; };
using AnimTable = std::map<std::string, UnitAnimSeq>;
const UnitAnimSeq& default_anim();                       // real GI/CONS values
const UnitAnimSeq& anim_for_image(const AnimTable& t, const std::string& image);
int seq_frame(const SeqEntry& e, int facing, int phase); // first + facing*e.fmult + clamped phase
int facing_from_delta(double dx, double dy);             // -1 when dx==dy==0 (keep current)
// Unit gains: int facing = 4;   (spawn facing S -- docs/re/infantry-anim.md §6)
// World gains: void set_anim(const std::string& image, const UnitAnimSeq& seq);
// step_units gains trailing param: const AnimTable& anims
```

- [ ] **Step 1: Write the failing tests** (append to `tests/test_world.cpp`):

```cpp
TEST_CASE("facing: 8-way cell-direction table (docs/re/infantry-anim.md §5)") {
    using ra2engine::facing_from_delta;
    CHECK(facing_from_delta(0, -1) == 7);   // screen NE
    CHECK(facing_from_delta(1, -1) == 6);   // E
    CHECK(facing_from_delta(1, 0) == 5);    // SE
    CHECK(facing_from_delta(1, 1) == 4);    // S
    CHECK(facing_from_delta(0, 1) == 3);    // SW
    CHECK(facing_from_delta(-1, 1) == 2);   // W
    CHECK(facing_from_delta(-1, 0) == 1);   // NW
    CHECK(facing_from_delta(-1, -1) == 0);  // N
    CHECK(facing_from_delta(0, 0) == -1);   // no direction -> keep current
    CHECK(facing_from_delta(5.0, 5.0) == 4);      // continuous, same ray as (1,1)
    CHECK(facing_from_delta(0.1, -0.1) == 6);     // continuous E ray
}

TEST_CASE("facing: seq_frame formula and real default table") {
    using namespace ra2engine;
    const UnitAnimSeq& a = default_anim();
    CHECK(a.stand.first == 0);   CHECK(a.stand.count == 1);  CHECK(a.stand.fmult == 1);
    CHECK(a.walk.first == 8);    CHECK(a.walk.count == 6);   CHECK(a.walk.fmult == 6);
    CHECK(a.fire.first == 164);  CHECK(a.fire.count == 6);   CHECK(a.fire.fmult == 6);
    CHECK(a.die.first == 134);   CHECK(a.die.count == 15);   CHECK(a.die.fmult == 0);
    CHECK(seq_frame(a.stand, 3, 0) == 3);          // stand facing SW -> frame 3
    CHECK(seq_frame(a.fire, 6, 2) == 164 + 36 + 2); // fire E phase 2 -> 202
    CHECK(seq_frame(a.die, 6, 4) == 138);           // die ignores facing (fmult 0)
    CHECK(seq_frame(a.fire, 0, 99) == 169);         // phase clamps to count-1
    AnimTable t;
    UnitAnimSeq custom = a; custom.fire.first = 500;
    t["ZZZ"] = custom;
    CHECK(anim_for_image(t, "ZZZ").fire.first == 500);
    CHECK(anim_for_image(t, "GI").fire.first == 164);  // miss -> default
}
```

And behavioral cases (use the existing test_world combat fixtures/pattern — a small World via the same helper the Task-5/7 tests use):
  - walking unit faces its segment: order a unit one cell east ((+1,0)) — after a step, `facing == 5`; walk frame in `[8+5*6, 8+5*6+6)`.
  - firing unit faces its target: attacker due cell-east of victim (delta (−1,0) attacker→victim… compute: victim at attacker + (1,0) ⇒ facing 5) — after `step` with in-range target, `facing == 5` and during flash `frame` in `[164+5*6, 164+5*6+6)`.
  - die frames now `134..148`, reach 148 before tombstone (mirror the existing die-clamp test, values updated).
  - spawn facing is 4.

- [ ] **Step 2: Run to verify failure** — build fails / cases fail (facing_from_delta undefined).

- [ ] **Step 3: Implement.** In `world.hpp`: replace the old UnitAnimSeq block with the **Interfaces** code (keep a doc comment pointing at docs/re/infantry-anim.md; delete the "verified BY EYE" comment block — it is superseded); add `int facing = 4;` to Unit; add `AnimTable anims_;` + `set_anim` to World; add `#include <map>`. In `world.cpp`:

```cpp
// Real artmd.ini values -- GISequence and ConSequence are identical for
// every entry we consume (docs/re/infantry-anim.md §2). Serves as the
// fallback for unregistered images AND keeps asset-less tests correct.
const UnitAnimSeq& default_anim() {
    static const UnitAnimSeq a{
        /*stand=*/{0, 1, 1}, /*walk=*/{8, 6, 6},
        /*fire=*/{164, 6, 6}, /*die=*/{134, 15, 0},
    };
    return a;
}

const UnitAnimSeq& anim_for_image(const AnimTable& t, const std::string& image) {
    const auto it = t.find(image);
    return it != t.end() ? it->second : default_anim();
}

int seq_frame(const SeqEntry& e, int facing, int phase) {
    const int p = phase < 0 ? 0 : (phase >= e.count ? e.count - 1 : phase);
    return e.first + facing * e.fmult + p;
}

// Cell-space direction -> SHP facing block index (counterclockwise from
// North on screen -- docs/re/infantry-anim.md §4/§5). Screen projection is
// (sx, sy) ~ (dx - dy, dx + dy) with y down.
int facing_from_delta(double dx, double dy) {
    if (dx == 0.0 && dy == 0.0) return -1;
    const double ang = std::atan2(dx + dy, dx - dy) * 180.0 / 3.14159265358979323846;
    const double ccw = std::fmod(-90.0 - ang + 720.0, 360.0);
    return static_cast<int>(std::lround(ccw / 45.0)) % 8;
}
```

`set_anim`: `anims_[image] = seq;`. `step_units` takes `const AnimTable& anims` (World::step passes `anims_`; tests pass a local `AnimTable{}`); inside, `const UnitAnimSeq& seq = anim_for_image(anims, u.image);` and:
  - stand writes become `u.frame = seq_frame(seq.stand, u.facing, 0);`
  - the walk loop: at segment selection (right after `const Cellxy tgt = u.path[u.path_idx];` when `tgt != cell`), `const int f = facing_from_delta(tgt.x - u.cell_x, tgt.y - u.cell_y); if (f >= 0) u.facing = f;`; walk frame write becomes `u.frame = seq_frame(seq.walk, u.facing, phase);`
  - In `World::step`'s anim-override pass: new constant `static constexpr double kDieAnimFps = 15.0;` (replaces kWalkFps in the die formula only — walk cycling stays 9.0); before the frame overrides, aim facing: `if (u.target >= 0 && u.target < (int)units_.size()) { const Unit& t = units_[u.target]; if (t.alive) { const int f = facing_from_delta(t.fx - u.fx, t.fy - u.fy); if (f >= 0) u.facing = f; } }`; die write: `u.frame = seq_frame(seq.die, u.facing, (int)(u.die_time * kDieAnimFps));` fire write: `u.frame = seq_frame(seq.fire, u.facing, adv);` (drop the old min-clamp — seq_frame clamps).
  - Update the OLD test pins in test_world.cpp (fire 158→164 CONS, die 80..87→134..148, `anim_for_image("GI")` call sites get the table arg) and any `step_units` direct callers in test_movement/test_group_move.

- [ ] **Step 4: Run** — `./build/ra2assets_tests.exe` FULL suite (this task touches shared movement code): all pass with assets; re-run without `RA2_INSTALL_DIR`: skips clean.
- [ ] **Step 5: Commit** — `git commit -m "feat(engine): 8-way facing + artmd-calibrated animation table (fixes die/fire frame bugs)"`.

---

### Task 3: Attack approach — firing positions & fan-out

**Files:**
- Modify: `engine/combat.hpp` (MoveRequest.approach), `engine/combat.cpp` (§f attack_order branch), `engine/world.cpp` (`World::step` request routing), `tests/test_combat.cpp` (pursuit-request semantics), `tests/test_world.cpp` (fan-out regression).

**Interfaces:**
- Consumes: `assign_formation(goal, mover_cells, passable, w, h, occ)` (group_move.hpp, already included by world.cpp); `SeqEntry`-era anim API from Task 2.
- Produces: `struct MoveRequest { int unit; int goal_x; int goal_y; bool approach = false; };` — approach=true means "goal is the TARGET's cell; caller must resolve a firing position", approach=false means "move exactly there" (guard-return unchanged).

- [ ] **Step 1: Write the failing tests.** In `tests/test_combat.cpp` (follow the existing step_combat fixture style):
  - out-of-range attacker with `attack_order` and NO path → one request with `approach == true` and goal == target's cell;
  - attacker whose `path.back()` is within `w_range` cells (Euclidean) of the target → NO new request while the path is live, even though `path.back() != target cell`;
  - target moved so `dist(path.back(), target) > w_range` → a fresh approach request;
  - guard-return requests still have `approach == false` (existing leash test: add the field check).

  In `tests/test_world.cpp`, THE regression for the user's bug:

```cpp
TEST_CASE("combat: five attackers fan out and ALL fire (no single-file queue)") {
    // Open synthetic world (same fixture helper as the other World combat
    // tests). One tough victim (hp large enough to survive >= 15 s of five
    // M60s) at (20,10); five house-0 attackers clustered single-file-prone
    // at x=4..8, y=10. Attack order for all five.
    // Simulate 12 s in 1/60 s steps. Then assert EVERY attacker:
    //   (a) got within weapon range of the victim at some point, and
    //   (b) actually fired (rof_cool > 0 observed, or cumulative damage
    //       accounting: victim hp dropped by >= 5 * one-volley damage).
    // Track per-attacker "has_fired" by sampling units()[i].fire_flash > 0
    // across steps. With the old code only ONE attacker ever fires.
}
```

  (Write the body concretely with the fixture at hand; the tracked-flags loop is ~15 lines. Choose victim strength via `add_combat_unit` stats, e.g. hp 100000, so nobody dies during the window.)

- [ ] **Step 2: Run to verify failure** — fan-out case fails (only one attacker fires); approach-flag cases fail to compile until the field exists (add field first if you prefer strict red/green split).

- [ ] **Step 3: Implement.**
  - `combat.hpp`: add `bool approach = false;` to MoveRequest (doc comment above).
  - `combat.cpp` §f — replace the attack_order branch body:

```cpp
        } else if (u.attack_order) {
            // Approach: ask the caller for a firing position near the target.
            // The original engine approaches until in weapon range and never
            // paths to the victim's own (occupied) cell (docs/re/
            // infantry-anim.md §7); World::step batch-assigns each attacker
            // a distinct free cell around the target. Re-request only when
            // the current path's goal has stopped being a firing position
            // for the target's CURRENT location (target moved out of the
            // goal's range circle), or there is no live path at all.
            bool need = !u.moving();
            if (!need) {
                const Cellxy g = u.path.back();
                need = dist_leptons(static_cast<double>(g.x), static_cast<double>(g.y),
                                    v.fx, v.fy) > range_leptons;
            }
            if (need) requests.push_back({i, v.cell_x, v.cell_y, /*approach=*/true});
        }
```

  - `world.cpp` `World::step` — replace the request loop with batch routing (needs `#include <map>` and `#include <utility>` if absent):

```cpp
    const std::vector<MoveRequest> reqs = step_combat(units_, occ_, dt);
    // Guard-return requests route directly. Approach requests are batched
    // per goal cell (one batch per target) and resolved to DISTINCT free
    // firing-position cells via assign_formation, so a group of attackers
    // fans out around the target instead of queueing single-file behind
    // whoever got in range first (spec: Combat Fidelity Fixes §B; the old
    // goal -- the target's own occupied cell -- was unreachable by
    // traffic-aware re-paths, which is exactly the reported stuck-queue
    // bug). A crowded-out attacker re-assigned its own cell re-requests
    // next frame: bounded churn, accepted for this unit count.
    std::map<std::pair<int, int>, std::vector<int>> approach_groups;
    for (const MoveRequest& r : reqs) {
        if (r.unit < 0 || r.unit >= static_cast<int>(units_.size())) continue;
        const Unit& u = units_[r.unit];
        if (!u.alive || u.die_time >= 0.0) continue;  // died this same step
        if (r.approach)
            approach_groups[{r.goal_x, r.goal_y}].push_back(r.unit);
        else
            order_unit_move(r.unit, r.goal_x, r.goal_y);
    }
    for (const auto& group : approach_groups) {
        const std::vector<int>& movers = group.second;
        std::vector<Cellxy> cur;
        cur.reserve(movers.size());
        for (int idx : movers) cur.push_back({units_[idx].cell_x, units_[idx].cell_y});
        const std::vector<Cellxy> targets =
            assign_formation({group.first.first, group.first.second}, cur, passable_,
                             grid_w_, grid_h_, occ_);
        for (std::size_t k = 0; k < movers.size(); ++k)
            order_unit_move(movers[k], targets[k].x, targets[k].y);
    }
```

  - Update the OLD pursuit tests in test_combat.cpp that asserted goal == target cell with exact-cell dedup — the request now carries approach=true and the dedup is range-based (rewrite those assertions per Step 1's semantics; do NOT delete coverage).

- [ ] **Step 4: Run** — FULL suite; fan-out regression green; hold-at-range and leash tests still green.
- [ ] **Step 5: Commit** — `git commit -m "fix(combat): attackers fan out to distinct firing positions (no single-file queue)"`.

---

### Task 4: Wire the real artmd.ini + finalize

**Files:**
- Modify: `engine/main.cpp` (generalize `load_rules_ini` → `load_ini_from_ra2md(dir, name)`; load artmd.ini once per mode entry; register anims), `README.md` (one short paragraph), plus full-suite/demo/arm64 verification.

**Interfaces:**
- Consumes: `Art` (Task 1), `World::set_anim` + `UnitAnimSeq`/`SeqEntry` (Task 2).

- [ ] **Step 1: Generalize the loader.** Rename `load_rules_ini(dir)` to `load_ini_from_ra2md(const std::string& dir, const std::string& name)` (the string literal `"rulesmd.ini"` becomes `name`, error message includes `name`); keep a one-line `load_rules_ini` wrapper OR update its two call sites — implementer's choice, note it in the report.
- [ ] **Step 2: Load + register.** In `load_unit_assets`: after the Rules resolution, `Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini"); Art art = Art::load(art_ini);` and build per-image `UnitAnimSeq`:

```cpp
ra2engine::UnitAnimSeq anim_from_art(const ra2assets::Art& art, const std::string& image) {
    auto conv = [&](const char* name) {
        const ra2assets::ArtSequenceEntry e = art.entry(image, name);
        return ra2engine::SeqEntry{e.start, e.count, e.facing_mult};
    };
    ra2engine::UnitAnimSeq seq{conv("Ready"), conv("Walk"), conv("FireUp"), conv("Die1")};
    std::cerr << "[anim] image=" << image << " fireup=" << seq.fire.first << ","
              << seq.fire.count << "," << seq.fire.fmult << " die1=" << seq.die.first
              << "," << seq.die.count << "\n";
    return seq;
}
```

  Store both in `UnitAssets` (add `std::map<std::string, ra2engine::UnitAnimSeq> anims;`) and in `build_world` call `world.set_anim(image, seq)` for each entry before spawning squads.
- [ ] **Step 3: Verify live.** Build; run `./build/ra2engine.exe --demo-combat` from PowerShell with `RA2_INSTALL_DIR="D:\Red Alert 2"`. Gates: exit 0 (bit-identical logs, no overlap, no dangling, ≥1 death, decisive); `[anim]` log lines show `fireup=164,6,6 die1=134,15` for BOTH images. Expect the battle timeline to CHANGE vs the pre-fix demo (five guns firing) — that is correct, not a regression.
- [ ] **Step 4: Suites both modes.** Full `ra2assets_tests.exe` WITH install (expect all green) and WITHOUT `RA2_INSTALL_DIR` (clean skips). Then the arm64 gate: NDK r27c build of the Android target (same command as the Combat Phase Task 9 used — see `.superpowers/sdd/progress.md` ledger entry) compiles clean including `src/art.cpp`.
- [ ] **Step 5: README.** In the combat paragraph: one sentence that animations/facing are driven by the real artmd.ini sequence data (8 facings) per `docs/re/infantry-anim.md`, and that group attacks fan attackers out to individual firing positions.
- [ ] **Step 6: Commit** — `git commit -m "feat(engine): artmd.ini-driven anims wired; fidelity fixes finalized"`.
