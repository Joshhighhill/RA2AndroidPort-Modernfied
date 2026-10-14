# W9-02: Multiplayer performance and speed under load

Auditor: w9mp-02. Clone: D:\ra2-android master f188125 (`build\ra2engine.exe`, RelWithDebInfo `-O2`). Original: gamemd.exe YR 1.000 (existing RE in `D:\ra2-re\net1-out\dec2.c` and `docs/re/net-protocol.md`; no new Ghidra project was needed).

Machine: i7-10750H laptop (6C/12T, 2.6 GHz base). The machine was shared with the other w9mp auditors' engine runs, so treat single readings as about ±30 %. The trends and the profile shares are stable.

All evidence is in `D:\ra2-re\w9mp-02-out`:

| File or folder | What it holds |
|---|---|
| `m27_40.play`, `lobby8.play` | The 8-slot match script |
| `runA\` | The first 40-minute attempt. The human was killed at frame 8250, so only minutes 1–9 are used. |
| `runB\` | **The main run.** 40 game minutes, 1 human + 7 Brutal AI, through the real MP path. Contains `stderr.txt` (perf log every 5 s), `minutes.txt`, `profile.txt`, `prof_report.txt` and `prof400.txt` (sampling profile). |
| `runC\` | The same match with a visible window: real D3D renderer, vsync off, rendering every frame, 12 game minutes. |
| `soak_cur\`, `soak_mr2\` | The same 20-minute 2-AI `--ai-soak` on f188125 and on the pre-wave-7 build `D:\ra2-wt\mr2-build`, plus a profile of the f188125 run |
| `parse.py`, `parse2.py` | The log parsers |

## How it was measured

**The match.** `--play` drives the real shell:
- Network → Host Game (LAN);
- map **xmp27t8 (8 players, 263×263 cell grid)**;
- slots 1–7 Brutal AI; slot 1 is Libya on the human's team A; Short Game off;
- three Rhinos spawned for the human each minute, so the human survives all 40 minutes.

`Start Game` then runs a real `NetGame` lockstep session: FRAMEINFO, a sync hash every 15 frames, `net_gate` on every tick. The harness runs one fixed 1/15 s tick per driver frame with no throttle, so ms per frame is the true cost.

**The tools.**
- The engine's perf meter (`--fps`): `[perf] … update / render / present` every 5 s wall.
- The engine's own sync-hash timer (`[net] sync hash: N computed, avg, max`, `net_hash_avg_us`).
- `RA2_PROFILE` (the R-171 sampling profiler, about 514 samples/s) with `tools/profile_report.py`. It ran for the whole 25.5-minute wall run, which was dominated by the slow late game.
- `input` (3–6 ms) and the 17 % "probe" share in the profile are the `--play` harness's per-frame probe. They are **not** game cost and are excluded below.

## Summary

1. **Late in an 8-player match the simulation alone costs 55–65 ms per logic frame on a desktop i7** (runB, minutes 26–39, about 280 units and 290 buildings). The 15 Hz budget is 66 ms, so the desktop sits at the edge. A phone at 3–5× slower runs 3–6 logic frames/s instead of 15, and lockstep makes every player wait for the slowest machine.
2. **About two thirds of that (about 38 ms per frame) is one O(map) loop.** `Occupancy::release_all_of` scans all 263×263 cells, on both occupancy planes, with 3 infantry spots each. It runs inside every `order_unit_move` and `commit_attack_target`, and the AI re-issues those orders every frame for units that cannot act on them:
   - engineers that cannot reach their target re-path every frame (`step_capture`);
   - team members are given a building they cannot damage, combat drops it, and the team re-gives it the next frame;
   - team Load re-paths every frame.

   These two functions take **51 % of all profile samples** (self). The original keeps occupancy per object (O(1) per order), so this cost does not exist there.
3. **The sync hash serialises the whole World every 15 frames: avg 47 ms, max 107 ms, on a single frame.**
   - The World image is 8.0 MB on this map even with only 2 AIs (`--ai-soak` `bytes=7966270`).
   - Every int is widened to 8 bytes and appended one at a time; all static rules and AI config tables are serialised every time.
   - That gives a hitch of up to 0.1 s once a second on desktop and 0.3–0.5 s on a phone.
   - The original's frame CRC (`FUN_0064D520`) folds only each object's coordinates and facing plus the RNG. It is O(objects) and costs microseconds.
4. **Multiplayer runs at a fixed 15 Hz and ignores the lobby Game Speed** (default 1). The original's master sends TIMING with DesiredFPS = min(achievable, 60 / 45 / 60÷speed). At the default speed an original MP match runs about **3× faster in game time** than ours.
   - This is probably the single largest "speed" difference a player feels.
   - It cannot be fixed until items 2 and 3 are fixed: 45 Hz needs a sim cost of 22 ms or less per frame.
5. **Wave 7 and wave 8 regressions.**
   - The w8fix-layer deck plane doubles the `release_all_of` scan once any deck unit exists.
   - It also adds **2.2 MB to every sync hash** (World image 5.80 → 7.97 MB, +37 %).
   - In an identical 2-AI soak, f188125 costs **6.7 ms per frame against 3.6 ms on the pre-wave-7 build mr2** (1.85×).
   - The wave-7 threat scan and call_for_help are **not** hot: their delays are applied, and they are below 0.1 % of the profile.
6. **Rendering is cheap, and the network layer is negligible.**
   - GPU path at 1280×720, early to mid game: draw-list build 0.75–1.5 ms plus D3D present 1.5–2.9 ms.
   - The network layer (event serialisation, packet service, logging) does not show in the profile (below 0.1 %).

## Frame budget

Two columns use **runB**: 8 houses, xmp27t8, MP lockstep path, i7-10750H.
- The late-game ms values are the measured `update` mean (58 ms, minutes 26–39) split by the profile's inclusive shares, normalised to GameScene::update = 77.7 % of samples.
- The early-game column is the perf log for minutes 2–4.
- Render comes from **runC** (D3D, vsync off).
- The phone column is late game × 3–5.

| Phase | Early game (min 2–4, ~200 objs) | Late game (min 26–39, ~570 objs) | % of late frame | Phone ×3–5 (late) | Original budget (estimate) |
|---|---|---|---|---|---|
| `release_all_of` O(w·h) scans (via order_unit_move + commit_attack_target) | <1 ms | **~38 ms** | 66 % | 115–190 ms | ~0 (per-object occupancy) |
| … of which engineer re-path every frame (`step_capture`) | – | ~17 ms | 29 % | | |
| … of which AI team re-orders (`ai_execute_team_command`) | – | ~20 ms | 34 % | | |
| Pathfinding (A*, memo, `build_occupancy_cost` 69 k alloc+fill) | ~0.3 ms | ~3 ms | 5 % | 9–15 ms | ~1 ms |
| Sync hash, amortised over 15 frames | 1.6 ms (24 ms spike) | 3–5.5 ms (**47 ms avg, 107 ms max spike**) | 6–9 % | spikes 140–535 ms | µs per frame (0x64D520) |
| Building defences target scan (every frame, all units) | ~0.2 ms | ~3.8 ms | 6.5 % | 11–19 ms | staggered |
| AI views and quarry (`make_ai_base_view`, `ai_fill_team_view`, quarry picks) | ~0.2 ms | ~3 ms | 5 % | 9–15 ms | |
| Visibility | ~0.1 ms | ~1.2 ms | 2 % | 4–6 ms | |
| Presentation logic tick (incl. 69 k-cell `bridge_cells_destroyed` every tick) | ~0.3 ms | ~1.5 ms | 2.6 % | 5–8 ms | |
| Rest of the sim (combat, units, locomotion, flight, threat scan, call_for_help…) | ~1 ms | ~3.5 ms | 6 % | 10–18 ms | |
| **Sim + hash total (`update`)** | **2.7–4 ms** | **55–65 ms** | 100 % | **165–325 ms** | **few ms** |
| Render: draw list (CPU) | 0.75 ms | 1.5 ms (min 12) / 2.3–2.8 ms (min 30–35, SW path) | | 5–14 ms | |
| Render: GPU present (D3D) | 1.5 ms | 2.9 ms (min 12) | | GLES, fill-bound | |
| Net: event codec, packet service, FRAMEINFO | <0.05 ms | <0.05 ms | ~0 | ~0 | ~0 |
| Logging (`[net] trail` every 15 frames, `[banim]`) | ~1.5 lines/s | same | ~0 | logcat, ~0 | |

Measured progression of `update` in runB, which includes the amortised hash:

| Game minutes | `update` per frame | Objects (units + buildings, all 10 houses) |
|---|---|---|
| 2–4 | 2.7–4 ms | 104–187 |
| 6–9 | 17–24 ms | 183–254 |
| 11–19 | 18–23 ms | 301–481 |
| 21–24 | 26–29 ms | 493–516 |
| 25–39 | **46–65 ms** | 533–593 |

Over the same period the hash grew from avg 24.6 ms to 46.9 ms (max 107 ms). The jump at minute 25 has no matching rise in object count. It is consistent with more stuck engineers and teams re-ordering every frame: each of those calls is a full-map scan.

**Comparison with the original's budget.** On 2001 hardware (a PIII at 500 MHz–1 GHz, roughly 10–20× slower per thread than this i7), gamemd ran 8-player matches at the MP speed cap of 30–60 fps. That is a whole frame (sim + render) of 15–30 ms there, which is about 1–3 ms of sim on this machine. Our late-game sim costs **about 20–50× more than that**.

## Gaps

### W9-02-01 `Occupancy::release_all_of` scans the whole map on every order (blocker)

**Severity:** blocker.

**Evidence:** [CONFIRMED-MEASURED]

| Run | `World::order_unit_move` | `World::commit_attack_target` | Combined |
|---|---|---|---|
| runB (8 houses, 40 min) | 26.2 % self | 25.0 % self | 51 % of all samples |
| `soak_cur` (2 houses, 20 min) | 30.9 % self | 35.0 % self | 66 % |

`release_all_of` is inlined in both, so it carries their self time.

**Clone:**
- `engine/occupancy.hpp:213-219` loops `for y < h_, x < w_ : release(x,y,unit)`.
  - `release` (`:205-208`) loops over `planes()`.
  - `drop_plane` (`:247-253`) checks the owner plus 3 spots.
  - That is 69,169 cells × (1 or 2 planes) × 4 compares per call.
- Callers:
  - `engine/world.cpp:1443` (every ground `order_unit_move`);
  - `engine/world.cpp:4604-4605` (every `commit_attack_target`);
  - 36 more sites.
- The comment says "called only on user orders, not per frame". That no longer holds: see W9-02-02 and W9-02-03.

**Original:** occupancy is per cell and per object. A FootClass knows the cells it marked (its own cell plus its next-cell reservation), so an order costs O(1) for occupancy.

**Root cause:** nothing records which cells a unit owns. A unit holds at most its own cell, one reserved next cell, and the matching spots.

**Fix:**
- Keep a small per-unit list of owned (plane, cell, spot) entries in Occupancy, maintained by `set`, `set_spot`, `release` and the reservation calls. `release_all_of` then walks only those 1–4 entries.
- Alternatively, rebuild a unit→cells index lazily.
- This is deterministic: the same cells are freed.
- Expected gain: about 38 ms per frame, roughly 65 % of the late-game sim.

**Test:**
- A unit test: `release_all_of` on a 263×263 grid with 1,000 units equals the old result.
- A timing guard: 10,000 calls in under 5 ms.
- Re-run `m27_40.play`. Expect `update` at minutes 26–39 below 20 ms. The saveload / sync gates must stay byte-identical.

### W9-02-02 AI engineers and spies re-path every frame when they cannot reach their target (major)

**Severity:** major.

**Evidence:** [CONFIRMED-MEASURED]
- `World::step_capture` is **22.7 % inclusive** in runB and **41.2 %** in `soak_cur`. Nearly all of it is in `order_unit_move` → `release_all_of` + A*.
- [CONFIRMED by code]:
  - `engine/world.cpp:3233-3240` (engineer);
  - `engine/world.cpp:3278-3283` (spy).

  A stopped, non-adjacent unit calls `order_unit_move` every tick ("retried next frame"). When the goal is unreachable, the path is empty, so the unit stays `!moving()` and the loop never ends.

**Original:** the Capture/Enter mission is driven by the mission timer (`Mission_Enter`, FootClass approach), with a re-evaluation delay, not every frame. A failed approach falls back to Guard.

**Root cause:** there is no back-off or give-up when the approach goal is unreachable.

**Fix:**
- Re-path at most every N frames (the original's mission `Timer`, about 15–30 frames, or BlockagePathDelay).
- After K failures, drop `capture_target` and go to Guard.
- Each retry also re-runs `find_path_nearest_t` with the occupancy-cost rebuild (W9-02-06).

**Test:**
- A play script with an engineer ordered at a building whose adjacent cells are walled in: assert at most one `order_unit_move` every 15 frames, using a probe counter.
- Perf: `step_capture` below 1 % in the 8-AI profile.

### W9-02-03 AI teams re-issue attack and load orders every frame (major)

**Severity:** major. It is also a behaviour bug: the unit's path is cleared every frame, so it never moves.

**Evidence:**
- [CONFIRMED-MEASURED] `ai_execute_team_command` is 26.6 % inclusive (runB) and 35.6 % (soak). `commit_attack_target` is 25 % self.
- [CONFIRMED by code]:
  - **Attack quarry.** `engine/world.cpp:4881-4899` re-targets every member whose `attack_order` is not set. `ai_pick_quarry_building` does not check whether that member can damage the building. `engine/combat.cpp:1213-1218` then clears `target_building` and `attack_order` on the next tick when `unit_weapon_for_building` is < 0 (AA-only, 0 % Verses, no-weapon members). The team re-orders, `commit_attack_target` clears the path again and runs `release_all_of` again, every frame, forever.
  - **Team Load.** `engine/world.cpp:4915-4930` calls `order_unit_move` toward the transport for every not-yet-loaded member on every tick, with no "already moving there" test. That is a full A* plus `release_all_of` per member per frame.

**Original:** a team issues a mission once (`TeamClass::Coordinate_*`, `Assign_Mission_Target`). Members keep their TarCom until it expires. Quarry selection goes through `Greatest_Threat` with the member's own weapon (`Evaluate_Object` / `GetFireError`), so an unusable target is never assigned.

**Fix:**
- Filter quarry candidates with the same weapon/Verses gate that combat uses (`unit_weapon_for_building` / `GetFireError`).
- Remember a per-member "no usable target" stamp and re-try only on the team's mission timer.
- For Load, skip members already moving to the transport's cell.

**Test:**
- A play script: an AI team with an AA-only member (e.g. a Flak Track) told to attack a building. Assert that the member's `commit_attack_target` count is ≤ 1 per 30 frames.
- Perf: `ai_execute_team_command` below 2 % in the profile.

### W9-02-04 The sync hash serialises the whole World every 15 frames, with 47–107 ms spikes (blocker for phones)

**Severity:** blocker for phones.

**Evidence:** [CONFIRMED-MEASURED]

| Measurement | Value |
|---|---|
| Engine timer, runB (2,401 hashes) | avg 46.9 ms, max 107.2 ms |
| Engine timer, runC (781 hashes, 12-min game) | avg 25.1 ms, max 46.9 ms |
| `--ai-soak` World image size | 7,966,270 bytes |

Profile shares: `net_gate` 7.4 % inclusive of all samples, of which:
- `save_world` 4.9 %;
- `vector::_M_range_insert` 4.3 %;
- the FNV byte loop (lambda self) 2.4 %.

Clone code:
- `engine/game_scene_net.inc:83-99`: `save_world(world_)`, then FNV-1a byte by byte.
- `engine/savegame.hpp:43-49`: `Writer::bytes` = `buf.insert` with no reserve.
- `engine/savegame.hpp:136-218`: every `int` / `int16` / enum is widened to 8 bytes, and `vector<int>` is written element by element.
- `engine/net/net_game.cpp:56-66`: the hash is taken every `hash_every` (15, `engine/mp_pages.inc:1485`) frames, entirely on that one frame.

The bytes include:
- the 263² `int` occupancy planes: owner + 3 spots, ×2 planes, ×8 bytes, about 4.4 MB;
- `bridge_ovl_` as int16 → 8 bytes per cell;
- every Unit's copied WeaponSpec / AbilityProfile / type data;
- the static per-house `AiHouseConfig` / `AiTeamConfig` tables (all visible in the profile).

**Original:** `ComputeFrameCRC` `FUN_0064D520` [CONFIRMED-RE 0x64D520, `D:\ra2-re\net1-out\dec2.c:1-110`] runs every frame.
- It covers:
  - for each infantry, unit, aircraft and building, `crc = crc*2 + X/10 + (Y/10)<<16 + facing`;
  - the layer lists;
  - `FUN_0065c1c0` (the RNG).
- That is O(objects) with no allocation: tens of microseconds.
- The result goes into `LatestFramesCRC[frame & 255]` for FRAMEINFO.

**Root cause:** the L0 placeholder ("save serializer's bytes fed through add_crc", `docs/re/net-protocol.md:234-237`) was never replaced.

**Fix, in order of value:**
1. A streaming hash archive: `Hasher` with the `Writer` interface that folds bytes into a 64-bit hash directly. No 8 MB buffer, no realloc. Add bulk-hash for POD vectors and skip the constant rules and config tables (hash them once at match start). Expect about 10× less.
2. Port `FUN_0064D520` as the per-frame CRC that goes into FRAMEINFO. Keep the full-World hash only for desync diagnosis, e.g. every 450 frames or on a CRC mismatch.
3. Spread the full hash over 15 frames (hash 1/15 of the sections each frame), so there is no spike.

**Test:**
- A gate: `net_hash_avg_us` below 2,000 and max below 5,000 in `m27_40.play`.
- Desync detection (`scripts/play/net-lan-desync.ps1`) must still fire within 15 frames of the injected credit.

### W9-02-05 MP logic runs at a fixed 15 Hz; the lobby Game Speed and the TIMING DesiredFPS are ignored (major)

**Severity:** major (what a player feels as "slow").

**Evidence:** [CONFIRMED-RE 0x64761C..0x6479C4, per `docs/re/net-protocol.md:228`]: TIMING generation uses "fps from the players' PROCESS_TIME, game-speed cap 60/45/60÷speed". This is ported in `engine/net/lockstep.cpp:251`, but `desired_fps()` (`engine/net/lockstep.hpp:131`) has no caller. [CONFIRMED by code]:
- `engine/main.cpp:6921`: `net_ ? 1.0 /* NET-PLAY: fixed 15 Hz logic */`;
- `engine/game_scene_net.inc:15`: "Logic is a fixed 15 Hz (the lobby speed is not used)".

The lobby default is `GameSpeed=1` (`engine/mp_pages.inc:426`).

**Original vs clone:**
- The original MP plays at min(slowest peer's achievable fps, 45 fps at speed 1 or 60 at speed 0).
- The clone plays at 15 frames/s, i.e. one third of the game pace of an original speed-1 match.
- On a slow peer the clone stalls ("Waiting for players") instead of lowering the shared rate smoothly.

**Fix:**
- Drive the GameScene tick rate in MP from `session().desired_fps()`: `logic_acc_ += dt * desired_fps / 15`, capped by the lobby speed exactly as `lockstep.cpp:251` already computes.
- This is deterministic: every peer executes the same frame sequence; only wall pacing changes.
- **Depends on W9-02-01/02/03/04.** At 45 Hz the sim must stay at 22 ms per frame or less, and late game is 58 ms today.

**Test:**
- A two-process `net-match.ps1` at lobby speed 1: assert about 45 logic/s in `[perf]` on both machines, and identical trails.
- With an artificially slowed peer (`RA2_NET_SLOW`), assert that the TIMING fps drops and that there are no stall dialogs.

### W9-02-06 `build_occupancy_cost` allocates and fills a 69k-cell cost grid on every blocked path (minor)

**Severity:** minor.

**Evidence:** [CONFIRMED-MEASURED] 1.3 % inclusive in runB; `operator new` is 2.7 % inclusive overall.

**Clone:** `engine/world_loco.cpp:112-114` does `out.assign(w*h, 1)` per call. It is called from `engine/world.cpp:1477` (each order whose terrain path meets a unit) and from `engine/world_loco.cpp:406` (each repath). This came with wave 7 (w7fix-move, `29962fc`).

**Original:** the A* evaluates `Can_Enter` cost lazily per expanded cell (`FUN_00429A10`).

**Fix:** compute the unit cost on demand inside A* expansion (occupancy lookup per expanded node), or keep one scratch buffer and stamp only the cells units occupy (a generation counter instead of a fill).

**Test:** a pathfind unit test gives the same paths before and after; the `build_occupancy_cost` share in the profile drops below 0.2 %.

### W9-02-07 Base defences scan every unit on every frame, with no targeting delay (minor)

**Severity:** minor (3.8 ms late game; scales with defences × units).

**Evidence:** [CONFIRMED-MEASURED] `World::step_building_combat` is 5.1 % inclusive (runB). [CONFIRMED by code] `engine/building_combat.cpp:280-289`:
- a full `for j < nu` `engageable(j)` scan per armed idle building per tick whenever it has no valid target;
- plus a full building scan at `:335`.

**Original:** `BuildingClass::Greatest_Threat` (`FUN_00445E60`) is called from the techno targeting tick, which is gated by the TargetingTimer (NormalTargetingDelay). Units already have this (`combat.cpp:1536-1540`).

**Fix:** use the same `scan_timer` stagger for buildings (`R.normal_targeting_delay + rng(0..2)`) and the existing unit spatial buckets (`buckets / bucket_of` in step_combat) for the range query.

**Test:** the existing defence gates give identical results only if the delay is RE-confirmed for buildings. Otherwise gate the change with a byte-identity re-pin and a perf assert.

### W9-02-08 The presentation counts destroyed bridge cells over the whole map on every tick (cosmetic)

**Severity:** cosmetic (0.5–2.4 %).

**Evidence:** [CONFIRMED-MEASURED] `World::bridge_cells_destroyed` is 0.5 % (runB) and 2.4 % self (soak). `engine/pres_audio_events.inc:570-571` calls `engine/world_depth.cpp:545-553`, which loops over all of `bridge_ovl_` + `hbridge_`, on every logic tick.

**Fix:** compare `world.bridge_version()` (the counter already exists) and recount only when it changes.

### W9-02-09 Wave 8's deck occupancy plane doubles both the order cost and the hash size (major, a regression)

**Severity:** major (regression).

**Evidence:**
- [CONFIRMED-MEASURED] The same 20-minute 2-AI soak (`--ai-soak --diff brutal --ai-vs-ai --map xmp27t8.map --seed 12345`):
  - f188125: `bytes=7966270`, 126.2 s wall (12.3 s for a 1-minute run);
  - mr2-build (pre-wave-7, `D:\ra2-wt\mr2-build`): `bytes=5801265`, 69.4 s wall (7.9 s for 1 minute);
  - so **6.7 against 3.6 ms per frame**, with load time subtracted.
  - The two games diverge (the f188125 human was defeated), so the ratio is indicative.
- The +2.17 MB is exactly the w8fix-layer deck planes: `owner_d_` + `spots_d_` = 4 × 69,169 ints × 8 bytes.
- [CONFIRMED by code]:
  - `engine/occupancy.hpp:229` `planes()` = 2 as soon as `owner_d_` is allocated (`grow_deck` `:243-246`, triggered by any deck unit);
  - `release()` then visits both planes for each of the 69k cells.
- Commits:
  - `3baff2a` / `e7ccefd` (w8fix-layer);
  - `29962fc` (w7fix-move: `build_occupancy_cost`).

**Root cause:** a dense full-map deck plane for a feature used by a handful of bridge cells.

**Fix:** make the deck plane sparse (a hash map keyed by cell for the few deck cells), and leave the empty plane out of the save and the hash (or write it as a run-length list). W9-02-01 removes the scan cost entirely.

**Test:** the World image for xmp27t8 is at most 5.8 MB, or below 1 MB after W9-02-04's fix; the saveload gate is re-pinned once.

### W9-02-10 Per-frame allocations in the order and hash paths (minor)

**Severity:** minor.

**Evidence:** [CONFIRMED-MEASURED] `operator new` is 2.7 % inclusive.
- The 8 MB hash buffer is regrown from empty every 15 frames (`savegame.cpp:23-28`).
- `order_unit_move` allocates `track_prefix`, `gates`, `path`, `p2` and `cost` per call (`engine/world.cpp:1430-1480`).

On Android's allocator this costs more and fragments a process of 0.7–1.4 GB.

**Fix:**
- Reuse scratch vectors held in World, cleared each time.
- For the hash, see W9-02-04 (no buffer).

**Test:** an allocation counter in the debug build around `World::step`: no more than 50 allocations per tick at 500 units.

### W9-02-11 The dev profiler crashes the engine when the window is visible (cosmetic, tooling)

**Severity:** cosmetic (tooling).

**Evidence:** [CONFIRMED-PLAY] `RA2_PROFILE` + `RA2_PLAY_VISIBLE=1` exits with 0xC0000005 within 3 s (`runC` first attempt). Hidden window plus profiler, or visible window without profiler, both work.

**Root cause, [SUSPECTED]:** `SuspendThread` / `RtlVirtualUnwind` on the main thread while it is inside the D3D11 driver, whose frames have no .pdata (`engine/os_profiler.cpp`).

**Fix:**
- Skip samples whose RIP is outside the exe (`RtlLookupFunctionEntry` == null, so stop unwinding).
- Alternatively, wrap the walk in SEH.

**Test:** run `probe-perf.play` with `RA2_PLAY_VISIBLE=1 RA2_PROFILE=...` and expect a clean exit.

## Phone estimate

The Android emulator on this box is x86_64 (`ra2x86` / `ra2test` AVDs), so it does not show ARM cost; no arm64 phone was available.

Scaling the measured desktop numbers by 3–5× for a mid-range arm64 core (smaller caches hurt the full-map scans even more):

| Stage | Desktop | Phone (×3–5) | Effect |
|---|---|---|---|
| Early game (min 2–4) | 3–4 ms | 9–20 ms | fine |
| Mid game (min 6–20) | 17–23 ms | 50–115 ms | below 15 logic fps from about minute 6 |
| Late game | 55–65 ms | **165–325 ms** | 3–6 logic fps. In lockstep every peer, including desktop players, runs at the phone's pace: 20–40 % of normal speed. |
| Sync-hash spike | 47–107 ms | **140–535 ms** | a freeze every second |

**After W9-02-01..04, 06 and 09:** the late-game sim should fall to about 8–10 ms on desktop (25–50 ms on a phone). That is enough for 15 Hz everywhere, and for 30–45 Hz (W9-02-05) on desktop.

## Verified OK

| Area | Evidence |
|---|---|
| **The wave-7 threat scan honours its delays.** | `combat.cpp:1481` decrements `scan_timer`. `:1536-1540` scans only when `scan_timer <= 0` and then reloads it with `Normal/GuardAreaTargetingDelay + rng(0..2)`, using spatial `buckets`. `w7_greatest_threat` is below 0.05 % of samples. |
| **call_for_help** | 0.0 % self, 130 samples inclusive out of 786k. Event-driven, not per frame. |
| **Per-tile LandType (fv7c)** | `World::speed_land_at` is 0.03 % self. No per-frame cost. |
| **Visibility, combat, locomotion, `step_units`** | All below 2 % each. |
| **Network layer cost** | `NetGame::step` / `LockstepSession` / event codec / `NetManager::service` add nothing measurable: `net_gate` inclusive equals the hash lambda inclusive within 0.04 %. There is one `[net] trail` cerr line per 15 frames. Total log volume over 40 minutes was 4,697 lines, about 2 per second. |
| **No dropped or doubled ticks** | 36,000 frames, 0 stalls (`stalls 0`); a single human, so no remote latency. Remote pair behaviour is covered by w9mp-01 and w9mp-05. |
| **GPU rendering at 1280×720** | Draw list 0.75–1.5 ms, D3D present 1.5–2.9 ms, at 59 fps average with logic at 11 ms per frame (runC). Rendering is not the bottleneck. |
| **Logic pacing** | The accumulator never drops ticks and bounds the MP backlog to 2 s (`main.cpp:6938-6941`), as documented. |

## Process note

**LAN ports.** runA (07:33) and runB (07:52–08:17) hosted single-human LAN lobbies and matches without taking `w9mp-net.lock`, because I treated them as single-process runs. They bind LAN sockets, and port 50020 for the match. w9mp-01's pair test was holding the lock from 07:48. If w9mp-01 saw a stray lobby or a bind failure in that window, this is the likely cause. runC was run under the lock and stayed under 10 minutes. No D:\ra2-android files were changed and no Ghidra copy was made.
