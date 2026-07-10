# Combat Fidelity Fixes — attack fan-out & artmd-driven infantry animation

**Date:** 2026-07-10 · **Status:** approved (user chose "fix bugs + 8-way facing")
**Calibration authority:** `docs/re/infantry-anim.md` (new) + `docs/re/combat.md`

## Problem (from live play, user report)

1. **Single-file attackers:** ordering a group attack, only one soldier ever
   reaches firing range; the rest stack up behind him until he dies. Root
   cause (proven in code): every attacker's pursuit goal is the target's OWN
   cell (`combat.cpp` §f). That cell is occupied, so blocked followers'
   traffic-aware re-paths (`passable_minus_units` + A*) can never reach the
   goal — they wait, give up after 2 s, get re-issued the same impossible
   goal, forever. The original approaches until IN RANGE and never targets
   the victim's cell (re doc §7).
2. **Soldiers "fall" before dying:** the Conscript's hardcoded fire frames
   158–163 are the second half of the Die2 death sequence; the die frames
   80–87 for both units are Idle2/Crawl frames. Real values (artmd.ini):
   FireUp=164,6,6 and Die1=134,15,0 for BOTH units (re doc §2–3).

## Fix design

### A. Data-driven animation + 8-way facing

- **New `ra2assets::Art`** (mirrors `Rules`: thin wrapper over an already-
  parsed `Ini`): `sequence_name(image)` reads `[image] Sequence=`;
  `entry(image, name)` returns `{start, count, facing_mult}` parsed from
  `Name=a,b,c[,extras]` (extras tolerated; missing → `{0,1,0}`).
- **`UnitAnimSeq` becomes four `SeqEntry{first,count,fmult}`** (stand=Ready,
  walk=Walk, fire=FireUp, die=Die1). Frame selection everywhere:
  `frame = first + facing*fmult + phase`. Built-in default table = the real
  GI/CONS values (they are identical), so asset-less tests and unknown
  images stay correct; `main.cpp` registers per-image tables read from the
  real artmd.ini at startup via `World::set_anim(image, seq)`.
- **`Unit::facing` (0..7, spawn 4=S)** — counterclockwise-from-North block
  index per re doc §4–5. `facing_from_delta(dx,dy)` (pure) implements §5.
  Walking units face their current path segment; a unit with a target faces
  it (continuous delta) whenever the engage/anim pass runs. Idle keeps last
  facing. Die ignores facing (fmult=0). Die plays at 15 fps (15 frames ×
  1/15 s = kDieAnimSecs=1.0 exactly); fire stays 6 frames over 0.4 s.

### B. Attack approach = firing positions (fan-out)

- `MoveRequest` gains `approach` flag. An out-of-range attacker with
  `attack_order` requests approach toward the target's cell — but only when
  its current path goal is no longer a firing position (path empty, or
  `dist(path.back(), target) > weapon range`). The old exact-cell dedup goes
  away (it would re-request every frame once goals aren't the target cell).
- `World::step` resolves approach requests as a batch: groups them by goal
  cell (one group per target) and runs `assign_formation(target_cell,
  requester_cells, …)` — each attacker gets a DISTINCT free walkable cell
  spiralling out from the target, i.e. its own reachable firing position.
  Blocked followers can now re-path around traffic successfully and fan out.
- The existing hold-at-range pass is unchanged and supplies the original's
  "stop the moment you're in range en route" behavior; assigned cells only
  need to be reachable, not final (most units stop at the range-4 boundary
  before arriving).
- Known bounded churn (accepted): a crowded-out attacker (assigned its own
  cell) re-requests each frame — n≤10 units makes this trivial; occupancy
  still guarantees one-per-cell if two groups' assignments collide.
- NOT doing: 3-infantry-per-cell sub-cells (original's model) — Phase A's
  1-per-cell grid is a user-approved simplification; fan-out just spreads
  one ring wider.

## Testing

- `test_art.cpp`: synthetic INI (3-field parse, 4-field tolerance, missing
  defaults) + install-guarded pins on the real artmd.ini (FireUp=164,6,6,
  Die1=134,15,0, Walk=8,6,6, Ready=0,1,1 for BOTH GISequence/ConSequence).
- Facing: `facing_from_delta` 8-way table (re doc §5) + continuous angles;
  frame formula (stand k → frame k; fire k=6 phase 2 → 202; die ignores k).
- Combat: approach-request semantics (no-path → request; in-range goal → no
  re-request; target moved past goal's range → re-request).
- **Regression for the user's bug:** 5 attackers vs 1 tough target on an
  open grid — every attacker fires within the first simulated seconds
  (deterministic), instead of one-at-a-time-until-death.
- `--demo-combat` gates unchanged (bit-determinism rendered-vs-headless,
  decisive outcome); timeline WILL change (fan-out = more guns firing).

## Constraints (unchanged, binding)

GPL-clean repo (YRpp/Ares read off-repo, prose only); no game assets or
Ghidra project committed; tests skip cleanly (MESSAGE+return) without
RA2_INSTALL_DIR (default `D:\Red Alert 2`); pure modules compiled into both
test and engine targets via `cmake/ra2_sources.cmake`; run test exes from
PowerShell; arm64 NDK build must stay green.
