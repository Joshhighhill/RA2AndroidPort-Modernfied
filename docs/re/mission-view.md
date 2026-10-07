# Mission opening view, LocalSize and the input lock (W4-MC-02/03/04)

Source: `gamemd.exe` (YR 1.001). Private Ghidra copy `D:\ra2-re\w4fix-ctl`; scripts
`D:\ra2-re\scripts\w4fixctl\s1..s6.py`; output `D:\ra2-re\w4fix-ctl-out\`. The lock
functions were decompiled by the wave-4 audit (`D:\ra2-re\w4-ctl-out\lk_*.c`, `rd_*.c`).

## 1. LockInput / UnlockInput (TAction 0x2E / 0x2F) — CONFIRMED

- `FUN_00683c80` (lock): hides the mouse (cursor vtbl +0xc unless already hidden),
  `Scenario+0x35a2 = 1`, `DAT_00a8dd04 = 1`, `FUN_0054f3b0()` (drops the pending
  cursor / drag state), `DAT_00b066f4 = 0`.
- `FUN_00683cd0` (unlock): shows the mouse again, clears both flags.
- Readers of `DAT_00a8dd04`: the tactical WndProc `FUN_00692ab0` drops every mouse
  message but 0x215 (capture lost); edge scroll `FUN_00692940` is skipped; the
  keyboard paths `FUN_0054ee90` / `FUN_0054f420` pass only Esc; the sidebar
  `FUN_006ab0b0` returns 0.
- Other unlockers: the main loop's frame deadline (`0x55dabc`) and `FUN_0053b290`;
  other lockers: the game-end paths.

Engine: `GameScene::input_locked_` (`campaign_visuals.inc` cases 46/47). Pointer,
key and touch events past the options menu are dropped, edge/arrow scroll stops, the
cursor is hidden and a half-done band box / right-drag is cancelled. Esc / Android
Back, F1 (briefing) and the options button still work. The flag is not saved (a
plain global in the original); a lock with no unlock is released after 1800 logic
frames or when the mission ends [RE-FALLBACK: stands in for the deadline path].

## 2. The opening view — CONFIRMED

- `ScenarioClass::Read` `FUN_00689890` reads `[Basic] HomeCell` → `Scenario+0x20c`
  and `AltHomeCell` → `+0x210` (default 98 / 99 in every stock map).
- The map load `FUN_004acd80`: if `Waypoint[HomeCell]` is not set it is set to the map
  centre (`FUN_0068b950`); then the view is centred (`FUN_006d55e0`) on
  `Waypoint[HomeCell]` (`AltHomeCell` when `Scenario+0x1cb0` is set).
  `FUN_00684670` (scenario start) repeats it with the cell height (`FUN_00577ab0`).

ALL01: `HomeCell=98`, waypoint 98 = (58,72), the GIs on Liberty Island. Tanya is
staged at (62,108), outside the usable area, and walks in by script.

## 3. `[Map] LocalSize` — CONFIRMED

`MapClass::Read` stores `[Map] Size` at `Map+0xEC` and `LocalSize` at `Map+0xFC`.

**IsWithinUsableArea** `FUN_00577e90(cell, checkLevel)`, with `W = Size.w`,
`L = LocalSize`, `s = x + y`, `d = x - y`, `lvl` = the cell height when checked:

    W + 2*L.y + lvl < s <= W + 2 + 2*(L.y + L.h) + lvl
    2*L.x - W < d < 2*(L.x + L.w) - W

**Tactical clamp** `FUN_006d7bb0` (called by the set-position `FUN_006d55e0`), on the
view centre in the original pixel space (cell centre x = 30*(cx-cy),
y = 15*(cx+cy)+15), `vw`/`vh` = tactical view size:

    centre x in [vw/2 + 30*(2*L.x - W),  30*(2*(L.x+L.w) - W) - vw/2]
    centre y in [vh/2 + 15*(W - 5 + 2*L.y),  15*(W - 5 + 2*L.y) + 30*L.h + 135 - vh/2]

i.e. the view never leaves the rectangle left `30*(2Lx-W)`, right `30*(2(Lx+Lw)-W)`,
top `15*(W-5+2Ly)`, bottom `top + 30*Lh + 135`.

Engine: `parse_map_local_size` / `within_usable_area` (`campaign.cpp`),
`Input::set_view_limits` (fed from `GameScene::campaign_view_setup`), placement refuses
off-board foundation cells, and the `--play` probe keys `own_on_screen`,
`ctl_units` and `ctl_on_screen` count only on-board, unshrouded units.
Skirmish maps keep the old free camera (not yet applied there).
