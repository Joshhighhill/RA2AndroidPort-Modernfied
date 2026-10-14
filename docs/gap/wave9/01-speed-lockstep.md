# W9-01: Multiplayer game speed and lockstep timing (auditor w9mp-01)

Clone: D:\ra2-android master f188125 (build\ra2engine.exe, ra2netpeer.exe, build\play-out\ra2lobbyd.exe).
Original: gamemd.exe (YR 1.000 retail, the addresses below; see docs/re/gamespeed.md, docs/gap/net/01-original-protocol.md).
Ghidra decompiles reused (verified, not re-run): `D:\ra2-re\fixCout\div60_0055cff0.c` (main loop / frame limiter) and `mainloop_00647090.c` (Queue_AI_Multiplayer).
Outputs: `D:\ra2-re\w9mp-01-out\` (pace\ = timing driver runs, eng\ = real engine pair runs, soak_desk_*.txt).

## Summary

What a player feels in a clone match, and why:

1. **The match runs 3x slower than the original.** The clone's multiplayer simulation is hard-wired to 15 logic frames/s.
   - The lobby's Game Speed and the session's TIMING/DesiredFrameRate are both ignored.
   - The original runs a network game at the TIMING frame rate: 45 fps at the default lobby speed 1, capped by the slowest machine.
   - Units, production and every timer therefore advance at one third of the original's wall-clock rate.
2. **Orders take about 1 s (LAN) and 1.3 s (online) to start.** The original takes about 0.33 s (LAN) and 0.67 s (Internet).
   - The clone uses the original's MaxAhead/FrameSendRate values, which are counted in frames.
   - At 15 fps those frame counts become three times as many milliseconds.
3. **A slow machine makes the game stutter instead of slowing it down smoothly.**
   - The original measures each machine's processing time (PROCESS_TIME), and the host lowers the frame rate for everyone (TIMING).
   - The clone computes the same TIMING values and then throws them away. A fast peer stalls about every 400 ms, then bursts ticks.
   - The slow peer runs up to 4 catch-up ticks per render frame, and its screen drops to about 3 fps.
4. **The sync hash causes a visible hitch every second.**
   - fnv1a(save_world) costs 9-12 ms per call in a 1v1 and 38-39 ms in an 8-player match (max 68 ms) on the desktop.
   - It runs every 15 ticks, on the main thread, inside a tick. On a phone (about 5-8x slower) that is a 200-500 ms freeze once per game second.
   - The original's CRC is a cheap rolling sum over positions and facings, plus one RNG draw.
5. **After any network stall the clone fast-forwards** up to 2 s of game time (4 ticks per render frame).
   - The original never catches up: its frame timer just waits.
   - With jitter and loss this produced 8-22 ticks/s swings (measured) instead of a steady 15.

Coordination: w9mp-02 independently confirmed the fixed 15 Hz and the hash cost (8 MB World, 47 ms avg / 107 ms max in their 8-player Brutal match), and found the `Occupancy::release_all_of` order hot spot. The hash numbers here are mine and are consistent with theirs. I have not repeated their order-path profiling.

## Measurements

### Method

1. **`w9pace.exe`** (source `w9mp-01-out\pace\w9pace.cpp`, built outside the repo from the unmodified `engine/net/*.cpp`).
   - One lockstep peer that uses the game's own `LockstepSession`, driven exactly like `GameScene::update` in a match:
     - a 60 Hz render loop;
     - `acc += dt`;
     - a gate before each tick;
     - at most 4 ticks per render frame;
     - a backlog cap of 30 ticks.
   - Options add per-tick work and a periodic hash spike.
   - Two processes ran over real UDP, optionally through `delayproxy.py` (one-way delay, jitter, loss).
   - Input delay is measured from issue to execution, in wall-clock ms.
   - All pace numbers below come from the **re-runs made under the network lock** (`pace\rerun1.txt`, `rerun2.txt`). They match the first runs, which overlapped w9mp-02's unlocked lobby runs, to within noise.
2. **Real engine pairs** (`eng\pair.ps1` with my own host/join .play scripts, run under the lock after 08:17).
   - The `--play` harness steps a fixed dt of 1/15 with no frame delay. Wall-clock fps is therefore not observable there.
   - These runs measure the input delay in ticks (order click until `sel_moving == 1`), the stall counts, and the sync-hash cost.
3. **Hash cost, single process:** `--ai-soak --ai-vs-ai --map xmp27t8.map --frames 2700 --diff brutal`, with `--hash-every 0` and with `--hash-every 1`.
4. **Android emulator:** the emulator (emulator-5584) went offline in the middle of my run, after another agent restarted it, and did not come back. The phone figures are **estimates** from the desktop timings, using the ratio in docs/gap/net/04 (emulator `update` 25-41 ms vs about 3.5-5 ms per tick on the desktop, so 5-8x).

### Table A: timing-driver pairs (wall clock, 45 s per run, both peers reported)

| Condition (clone unless marked) | Sim rate achieved | Input delay, median / p90 / max | Stalls on the fast peer | Notes |
|---|---|---|---|---|
| LAN loopback, 15 Hz (clone config: MA 10 → 15, FSR 5) | 15.0 ticks/s | 998 / 1002 / 1005 ms (670 ms before TIMING at frame 271) | 0 | DesiredFPS computed = 15 (self-measured), unused |
| LAN loopback, 45 Hz (the original's speed-1 rate), same session code | 45.0 | **331 / 342 / 358 ms** | 0 | what the original feels like |
| Relay/Internet config (MA 15, FSR 5), 0 ms proxy | 15.0 | 1000 / 1002 / 1016 ms | 0 | |
| Internet, 100 ms one way (RTT 200) | 15.0 | 1000 / 1003 / 1016 ms | 0 | RT 14 ticks; MaxAhead stays at the 3·FSR floor |
| Internet, 250 ms one way (RTT 500) | 15.0 | 1000 / 1003 / 1030 ms | 0 | RT 33 |
| Internet, 500 ms one way (RTT 1 s) | 15.1 | 1000 / 1004 / 1015 ms | 1 episode, 52 ms | RT 64 |
| Internet, 100 ms ± 60 ms jitter, 3 % loss | 15.0 avg, **per second 8..22** | 1000 / 1003 / **1314 ms** | 2 episodes, longest **620 ms** (first run: 1.7 s) | catch-up bursts after each stall |
| Internet, 100 ms, 45 Hz (original-like; TIMING → FSR 10, MA 30) | 45.0 | 673 / 785 / 900 ms | 0 | the original's Internet feel |
| Slow peer B, 80 ms/tick (a phone that cannot hold 15 Hz) | **12.7 / 12.5** | A 1577 ms, B 1200 ms | A: 90.8 % of render frames stalled, 110 episodes, longest 408 ms | **B renders 145 frames in 45 s (3.2 fps), worst frame 320 ms** |
| Slow peer B, 30 ms/tick at 45 Hz | 33.5 | A 582 ms | A: 88 % stalled, 299 episodes | B renders 8.4 fps, every frame a 4-tick burst |
| Peer B 30 ms/tick + 45 ms hash spike every 15 ticks, 15 Hz | 15.0 | 999 ms | 0 | B: worst frame 76 ms, a local hitch once per second |

### Table B: real engine (2 processes, `--play`, fixed dt)

| Run | Houses | MaxAhead / FSR (start → frame 600) | Click → unit moving | Sync hash per call (avg / max) | Stalls (host / join) |
|---|---|---|---|---|---|
| LAN 1v1 (`eng\lan2b`) | 2 humans | 10 / 5 → 10 / 5 | 14 ticks (42 → 56, 602 → 616) = **933 ms at 15 Hz** | 9.3 / 27.4 ms, 9.2 / 21.0 ms | 19 / 33 |
| Relay V3 tunnel 1v1 (`eng\online2`, 1269 datagrams forwarded) | 2 humans | 15 / 5 → 15 / 5 | 19-20 ticks (42 → 61, 602 → 621, join 62 → 82) = **1267-1333 ms** | 11.8 / 24.3 ms, 11.6 / 27.8 ms | 20 / 14 |
| LAN 8-player, xmp27t8, 2 humans + 6 Brutal AI (`eng\big8b`) | 8 | 10 / 5 → 10 / 5 | 14 ticks | **37.8 / 68.1 ms, 38.5 / 54.8 ms** | 6 / 1 |
| Single process: ai-soak AI vs AI on xmp27t8, 2700 frames | 2 | | | (75.6 s − 13.5 s) / 2700 = **23 ms per hash** vs about 3.5-5 ms per sim tick | |

The hash is 5-10x the cost of a whole sim tick.

**Phone estimate (5-8x the desktop):** 1v1 hash 50-90 ms; 8-player hash 190-300 ms avg, 340-550 ms max, once every 15 ticks (1 s). Added to a 25-41 ms tick, this exceeds the 66 ms tick budget, so the phone peer falls behind every second. The row "slow peer B" shows what that does to both screens.

## Gaps

### W9-01-01: The multiplayer sim rate is fixed at 15 Hz; lobby Game Speed and TIMING DesiredFrameRate are ignored

- **Severity:** blocker (the user's first complaint, "speed").
- **Evidence:** [CONFIRMED-RE 0x55D10E..0x55D1C0, 0x6476C4..0x647700] [CONFIRMED-MEASURED]
- **Original:**
  - The main loop `FUN_0055CFF0` network branch sets the frame timer to `1000 / DesiredFrameRate` ms (a local frame time `= 1000/fps`, `DAT_008862d0 = 60/fps`; `fps = DAT_00A8A4C0`).
  - DesiredFrameRate comes from the host's TIMING event: `min(1000 / max(PROCESS_TIME of all players), cap)`, with `cap` = speed 0 → 60, 1 → 45, n → 60/n (`FUN_00647090` at `0x647784..0x6477B8`).
  - The default lobby speed is 1, so the game runs at **45 fps = 3x the clone**.
- **Clone:**
  - `engine/main.cpp:6920` `logic_acc_ += min(dt,1) * (net_ ? 1.0 ...)` ("NET-PLAY: fixed 15 Hz logic"); `engine/game_scene_net.inc:15` says "the lobby speed is not used".
  - `LockstepSession::desired_fps()` (`engine/net/lockstep.hpp:139`, set at `lockstep.cpp:364`) has **no caller**.
  - The lobby speed travels in the session (`mp_model.cpp:332`, probe `game_speed=1`) but only reaches `LockstepConfig::game_speed`, where it is used as the TIMING cap.
- **Measured:** 15.0 ticks/s in every condition. The same session code driven at 45 Hz reaches 45.0 ticks/s with 0 stalls on LAN and with 100 ms latency.
- **Root cause:** speed in MP was deferred, "phones cannot sustain 60 Hz" (docs/gap/net/04 §186), and the TIMING result was never wired to the tick rate.
- **Fix:**
  1. In a match, set the tick rate from the executed TIMING:
     - `ticks/s = session.desired_fps()`, which is identical on every peer because it executes at the same frame;
     - `logic_acc_ += dt * desired_fps/15` (the clone's per-tick rules stay at 15 "engine frames per second", as for single-player speed).
  2. Before the first TIMING, use the lobby speed cap (45 at speed 1).
  3. Keep the 15 Hz convention only for GameSpeed 4.
- **Test:**
  - Unit: TIMING{fps=45} executes, then GameScene runs 45 ticks per wall second under the real driver.
  - Pair: a w9pace-style driver plus a lobby speed 1 match measuring ticks/s ≈ 45.

### W9-01-02: Input delay is 3x the original's (MaxAhead in frames at 15 fps)

- **Severity:** major.
- **Evidence:** [CONFIRMED-MEASURED] [CONFIRMED-RE 0x6477C0..0x647850]
- **Original:**
  - MaxAhead = `RT*fps/120 + fudge`, rounded up to FSR, minimum 3·FSR. Internet FSR is 10 when fps ≥ 31.
  - At 45 fps that gives LAN 15 frames = **333 ms** and Internet 30 frames = **667 ms** (measured with the same code at 45 Hz: 331 ms and 673 ms median).
- **Clone:**
  - Starts at LAN MA 10 / Internet MA 15, FSR 5 (`engine/mp_pages.inc:1555-1556`).
  - The first TIMING raises LAN to 15 frames. At 15 fps that is **1000 ms** (LAN) and **1000-1333 ms** (relay). The engine measured click → move at 14 ticks (LAN, 933 ms) and 19-20 ticks (relay, 1267-1333 ms).
  - Because fps = 15 < 31, the Internet FSR stays 5 and MaxAhead never drops below 15 frames, whatever the latency (RTT 0-1000 ms all gave 1000 ms).
- **Root cause:** the frame-denominated constants are correct, but the frame rate is a third of the original's (W9-01-01).
- **Fix:** fixing W9-01-01 fixes this. If the rate must stay low on phones, scale the floor in time rather than frames, e.g. MaxAhead floor = ceil(0.33 s × fps / FSR) × FSR on LAN.
- **Test:** the engine pair measuring click → `sel_moving` in ms under the real driver; LAN ≤ 400 ms, Internet at 100 ms RTT ≤ 700 ms.

### W9-01-03: No slowest-player adaptation; a slow peer causes stutter, not a uniform slowdown

- **Severity:** major (phones).
- **Evidence:** [CONFIRMED-RE 0x647690..0x6476C4, 0x55D10E] [CONFIRMED-MEASURED]
- **Original:**
  - Every 128 frames each player sends PROCESS_TIME = the average **processing** ms per frame. `ProcessTicks` is accumulated from the loop start `0x55D04E` to just before Queue_AI `0x55DA5B`, so it excludes the frame-limiter wait.
  - The master sets DesiredFrameRate = 1000 / worst; everyone's frame timer then paces evenly at the slowest machine's rate.
  - On the Internet with MaxAhead > 30 it adds +10 ms per frame when peers fall behind by MaxAhead/4, /2 or 3/4 (`0x55D1C8..0x55D2F0`).
- **Clone:**
  - `lockstep.cpp:501-508` measures **wall time between begin_frame calls**, which includes the pacing wait and the stalls. The reported fps is therefore always the current rate (15, or 12 when slowed): self-limiting, it can never rise.
  - Its TIMING fps is unused anyway (W9-01-01). The only throttle is the lockstep gate.
- **Measured** (peer B needs 80 ms per tick):
  - Both peers drop to about 12.6 ticks/s.
  - Fast peer A: 90.8 % of render frames stalled, 110 stall episodes in 45 s (longest 408 ms), and bursts after each stall. Input delay is 1.58 s.
  - Slow peer B: runs 4 catch-up ticks per render frame and **renders 3.2 fps** (worst frame 320 ms), so its UI is effectively frozen.
- **Root cause:** the wrong PROCESS_TIME sample, and DesiredFrameRate is not applied.
- **Fix:**
  1. Time only the tick (logic plus hash) for PROCESS_TIME, as in the original.
  2. Apply DesiredFrameRate (W9-01-01).
  3. In a match, cap catch-up at 1 tick per render frame (or 2), instead of 4 plus a 30-tick backlog.
- **Test:** a w9pace-style pair with `--work-ms 80` on one peer. Expect: both at about 1000/80 ticks/s, < 5 % stalled render frames on the fast peer, and slow-peer render ≥ 10 fps.

### W9-01-04: The sync hash is a full save_world serialisation every 15 ticks, on the main thread

- **Severity:** major (the per-second hitch). With w9mp-02's finding this is the likeliest cause of the hitch the user feels on a phone.
- **Evidence:** [CONFIRMED-MEASURED] [CONFIRMED-RE 0x64D520]
- **Original:**
  - `ComputeFrameCRC` `FUN_0064D520` runs every frame.
  - `CRC = rotl1(CRC) + term` over infantry, units and buildings (X/10, Y/10, facing), the 5 map layers and Logic objects (X/10, Y/10, WhatAmI), plus one `Scenario->Random` draw.
  - That is a few µs per object, cheap and even.
- **Clone:**
  - `engine/game_scene_net.inc:81-99` `net_state_hash()` = fnv1a over `save::save_world(world_)`, byte at a time. `net_game.cpp:59-63` calls it every `kNetHashEvery = 15` frames (`mp_pages.inc:1485`), inside the tick.
  - Measured on the desktop:
    - 9-12 ms per call in a 1v1 (max 28 ms);
    - **38 ms avg / 68 ms max** with 8 houses on xmp27t8 at about 4500 frames;
    - single-process ai-soak: 23 ms per hash vs about 3.5-5 ms per sim tick.
  - Phone estimate: 190-550 ms every game second.
  - In w9pace a 45 ms spike every 15 ticks already gives a 76 ms render hitch on the hashing peer. A larger spike pushes the phone behind the lockstep gate (W9-01-03).
- **Root cause:** the full-state hash was chosen for debuggability (desync dumps) and costs O(world bytes) with serialisation.
- **Fix:**
  - Use an original-style rolling CRC over sim-relevant scalars for the per-frame FRAMEINFO CRC. It is cheap enough to compute every frame, which also matches the original's semantics.
  - Keep the full save hash only for the desync dump / on demand.
  - Alternatively, hash incrementally, or spread serialisation over the 15 frames.
- **Test:** a perf gate. On xmp27t8 with 8 houses at frame 4500, the hash costs < 1 ms avg on the desktop; and `net_hash_avg_us` stays < 2 % of tick time.

### W9-01-05: Catch-up after a stall fast-forwards the game (up to 4 ticks per frame, 2 s backlog)

- **Severity:** minor/major (feel).
- **Evidence:** [CONFIRMED-MEASURED] [CONFIRMED-RE 0x55D10E]
- **Original:** the frame timer waits; time lost in Wait_For_Players is simply lost. The game never runs extra frames to catch up.
- **Clone:** `engine/main.cpp:6930-6940`: at most `kMaxLogicSteps = 4` per render frame (`game_scene_controls.inc:376`), and the backlog is capped at 30 ticks (`main.cpp:6937`) but **kept**.
- **Measured:** after jitter or loss stalls (longest 620 ms, 1.7 s in the first run) the game ran up to 22 ticks/s (37 in the first run) for a while. Units visibly lurch.
- **Fix:** in a match, drop the backlog after a lockstep stall (`logic_acc_ = min(logic_acc_, tick)` when the gate refused). The game should resume at normal speed, as in the original.
- **Test:** a jitter/loss pair checking that per-second tick counts stay ≤ rate + 1.

### W9-01-06: Online channel timing starts at RetryDelta 60 ticks (1 s), so one lost order packet stalls for about 1 s

- **Severity:** minor.
- **Evidence:** [CONFIRMED-MEASURED] [SUSPECTED vs original]
- **Original / spawner:** the CnCNet spawner uses (60, -1, 600) at start. The vanilla game re-tunes with Set_Timing(RT+10, ...) every 128 frames, which the clone also does (`lockstep.cpp:534-542`).
- **Clone:** `net_game.cpp:34` sets (60, -1, 600) for Internet. With 3 % loss the measured stalls lasted 620 ms and 1.7 s, the longest before the first re-tune at frame 128 (8.5 s at 15 Hz, about 3 s at 45 Hz).
- **Fix:** start with RetryDelta = max(10, measured lobby RTT + 10). The lobby already pings.
- **Test:** the loss pair; the longest stall is < 400 ms at 100 ms RTT and 3 % loss.

### W9-01-07: The in-game Options speed slider does nothing in a match and is not disabled or synced

- **Severity:** cosmetic/minor.
- **Evidence:** [CONFIRMED-DATA code] [SUSPECTED original UI state]
- **Original:** speed changes in MP go through the GAMESPEED event (0x0D), which sets 0xA8DAC8 on every client at the same frame. The TIMING cap then follows the new speed.
- **Clone:**
  - `ingame_pages.inc:722-735` writes `set_.game_speed`; the match ignores it (W9-01-01).
  - The command bridge has no GAMESPEED event path to the tick rate.
- **Fix:** grey the slider out in a match, or send GAMESPEED and apply it via TIMING.
- **Test:** an in-match options probe.

### W9-01-08 (observation, menus): the joiner timed out while the host was in the map chooser

- **Severity:** minor.
- **Evidence:** [CONFIRMED-PLAY `eng\big8\host\stderr.txt`: "Joiner timed out."]
- When the host opened "Customize Battle" and set 6 AI slots right after the joiner connected, the joiner was dropped. The host then started a 7-house match without them.
- Re-ordering (map first, then the joiner) worked.
- Passed to the menu auditors; not investigated further.

### Environment note (not a product bug)

UDP 50000-50059 is an *administered* Windows port exclusion on this PC (`netsh int ipv4 show excludedportrange protocol=udp`). LanPort 50020 could not be bound in one run: the host fell back to an ephemeral port, the stock `net-match.ps1` / `mp-lan-join.play` typed `:50020` and failed.

The clone's fallback (`mp_pages.inc:443`) is silent apart from the log. The LAN beacon carries the real port, so the browser still works, but "Join by IP" users would type the wrong port. Consider showing the actual port in the lobby.

## Verified OK

- **TIMING generation** (`lockstep.cpp:236-292`) matches the original `0x64761C..0x6479C4`:
  - the fps cap 60/45/60÷n;
  - RT × fps / 120;
  - LatencyFudge;
  - growth ≤ +FSR;
  - FSR rounding and the 3·FSR floor;
  - the ((FSR+249)/FSR)·FSR ceiling;
  - +10 under fog;
  - Internet FSR 10 at fps ≥ 31.

  Measured: 45 Hz Internet gives MA 30 / FSR 10, as the original.
- **The TIMING execute path:** the "DoList: Moving event" re-stamp works (`moved 2` in the stats), and both peers adopt identical values at the same frame.
- **Set_Timing every 128 frames** (RT+10, max(8RT+15, 120)) and PROCESS_TIME every 128 frames are emitted with the original layout.
- **Lockstep gate.** With latency up to RTT 1 s there are no stalls at 15 Hz (MaxAhead 15 frames = 1 s of slack), and both peers execute identical frames (engine trails identical).
- **Responsive UI during stalls.** The tick gate is non-blocking: rendering, chat and the "Waiting for players" overlay continue (w9pace: the render loop held 60 fps on the fast peer while stalled).
- **The relay path** (V3 tunnel through ra2lobbyd): 1269 datagrams forwarded, the match completed, MaxAhead 15 / FSR 5 as configured.

## Proposed regression tests

1. `w9pace`-style timing driver in `tools/` (paced lockstep pair with work/hash/delay knobs) plus a CI case for each table row:
   - the rate equals the TIMING fps;
   - input delay ≤ the original's at the same latency;
   - < 5 % stalled render frames with one slow peer.
2. An engine perf gate for the sync CRC on xmp27t8 with 8 houses.
3. A real-driver (paced) mode for `--play` (`set realtime on`), so the network .play tests can measure wall-clock fps and delay.
