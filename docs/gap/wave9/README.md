# Wave 9 — multiplayer audit (2026-10-14, master f188125)

The user's request: "audit multiplayer especially speed and menus". Five read-only opus audits looked at the clone against the original. Their sources were Ghidra on YR 1.000, the shell dialogs and csf strings, measured matches and the Android 11 emulator.

| # | Report | Gaps |
|---|--------|------|
| 01 | [Speed / lockstep timing](01-speed-lockstep.md) | 8 (1 blocker) |
| 02 | [MP performance under load](02-mp-performance.md) | 11 (2 blockers) |
| 03 | [Menus vs the original shell](03-menus-vs-original.md) | 27 (11 major) |
| 04 | [Phone / touch / Android lifecycle](04-phone-touch-mp.md) | 16 (1 blocker) |
| 05 | [In-match features and robustness](05-ingame-mp-robustness.md) | 17 (1 blocker) |

## Why multiplayer feels slow

All of these were measured.

1. **The sim rate is fixed at 15 Hz** (W9-01-01, W9-02-05).
   - The lobby Game Speed and the host's TIMING fps are ignored (main.cpp:6920; `desired_fps()` has no caller).
   - At default speed the original runs 45 fps, so matches play about 3x slower.
   - Order delay is about 3x the original's, because it is the same frame counts at 3x the frame time: LAN 933 ms vs 333 ms, Internet 1.3 s vs 0.67 s (W9-01-02).
2. **The sync hash serialises the whole World** (8 MB) every 15 frames (W9-01-04, W9-02-04).
   - Cost on the desktop: 9–12 ms in a 1v1, and an average of 38–47 ms (max 107 ms) with 8 houses.
   - On a phone that is an estimated 200–500 ms freeze every game second.
   - The original (0x64D520) folds only positions, facings and the RNG.
3. **`Occupancy::release_all_of` scans the full map on every order** (W9-02-01).
   - It covers 69k cells on both planes, and is about 66% of the frame late in a game.
   - Wave 8's deck plane doubled it (W9-02-09).
   - The AI triggers it every frame through re-issued orders (W9-02-02/03):
     - engineers and spies that cannot path;
     - teams re-targeting buildings they cannot damage;
     - Team Load.

     Those units also never move, which is a behaviour bug.
4. **Slow-player handling is broken** (W9-01-03).
   - The original lowers everyone's fps to the slowest player.
   - The clone measures processing time including the wait, then discards the result.
   - The result is a fast peer stalling on 91% of frames and the slow peer at 3.2 fps.
5. **Smaller items:**
   - per-path full-map cost grid;
   - base defences scanning every unit;
   - a full-map bridge count every tick;
   - catch-up of up to 2 s after a stall;
   - a 1 s first retry online;
   - the in-game speed slider does nothing in a match.

The fix order matters. Items 2 and 3 have to be fixed before item 1 can raise the sim rate.

## Blockers outside speed

- **W9-05-01:** the touch cameo-menu Cancel mutates the World directly (`touch_ui.inc:407/411`). It desyncs any match within about 1 s. Fix: `issue_type_event(ET::Abandon, …)`.
- **W9-04-01:** on Android 11+, the keyboard delete key can't remove pre-filled text, such as the name "Player" or a saved server address. Cause: the SDL 2.30.9 input connection.

## Menus

There are 27 gaps, covering lobby rendering/state bugs and missing original features. Highlights:

- **Lobby display bugs:**
  - the clone-only check-boxes overlap the labels;
  - guests see blank Speed/Credits/Units;
  - every lobby after the first shows a black map preview (texture cache);
  - a smaller map silently kicks humans.
- **Missing original features:**
  - no Game Type list (Free For All / Megawealth / Duel…) and no Random Map;
  - the LAN page is only a browser, with no pre-game lobby chat or player list.
- **In-game:**
  - Diplomacy and Score show country names, and the joiner's "self" row is wrong;
  - diplomacy is read-only, and alliances cannot be formed (W9-05-03/04);
  - no observer after defeat (W9-05-05);
  - no beacons or taunts;
  - dead Load/Save/Delete buttons;
  - the "Skirmish Score" title;
  - Continue returns to the main menu.

## Phone

- **Keyboard:**
  - it never reappears after Back;
  - it never hides.
- **Black map on resume in a match:** a phantom mouse at (0,0) edge-scrolls.
- **Backgrounding:** 15 s in the background drops you from the lobby, and resuming a match can kick peers (stall timer).
- **Touch:**
  - sliders and lists can't be dragged;
  - targets are 14–36 dp (guideline 48);
  - Back leaves the lobby with no confirm.
- **Wrong address:** the host may show its mobile-data IP instead of its Wi-Fi IP.

## Robustness and determinism

The network layer held under loss, spikes, host crash and relay restart. A desync is a hard stop, as in the original. Remaining items:

- `kSimVersion` is always 1, and only rulesmd is checksummed, so different builds or mods can join and then desync (W9-05-02).
- Packets are trusted (W9-05-12).
- On a real arm64 phone, NaN bit patterns go into the hash raw, and out-of-range conversions differ (W9-05-13; also `savegame.hpp:145-149`).
- Lazily recomputed caches (amphib/crush/deck grids) are hashed. A presentation-side call on one peer would cause a false desync.
- `player_orders_` grows without bound in MP and is hashed.
- `econ_rules_` is built per peer and is not hashed.
- The trigger state is not in the net hash (this matters for co-op later).
- `ra2netpeer --selftest` reports a false out-of-sync after 2,550 frames, from the tool's own model (W9-05-14).

## Environment notes

- Windows reserves UDP 50000–50059 on this PC, which broke the stock net-match.ps1 once (port 50020).
- Android figures are estimates (5–8x the desktop); the emulator was unstable during the audit.

Evidence is in `D:\ra2-re\w9mp-0N-out`.
