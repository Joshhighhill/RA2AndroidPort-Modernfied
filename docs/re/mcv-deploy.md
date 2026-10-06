# MCV deploy / undeploy, skirmish start and game-over flow (gF, RE-E2/E3/I8)

Recovered 2026-10-06 from `gamemd.exe` (YR 1.001, SHA1 7BE4E550…) with a
private Ghidra copy (`D:\ra2-re\fixF`). YRpp's `UnitClass::TryToDeploy`
(0x7393C0) / `Deploy` (0x739AC0) addresses have drifted in this build; the
functions were re-anchored through the `DeploysInto` field instead.

## 1. Field anchors

| Item | Where | Value | Tag |
|---|---|---|---|
| `DeploysInto` | TechnoType `+0x404`, read in `FUN_00711510` @0x712619 (string 0x843120) | BuildingType* | CONFIRMED |
| `UndeploysInto` | TechnoType `+0x408`, same reader @0x712652 | UnitType* | CONFIRMED |
| `DeployFacing` | BuildingType `+0xEDC`; ctor `FUN_0045DD80` stores `0x80`; `FUN_0045FE40` @0x460C5C reads the INI value as `default>>5` and stores `<<5` (an INI value 0..7) | default 0x80 | CONFIRMED |
| `AdjacentCell[8]` | 0x0089E5F8.. static init `FUN_0049F200`; `[7]` = NW @0x0089E614 = (-1,-1) | (-1,-1) | CONFIRMED |
| Foundation W/H | `FUN_0045EC80` / `FUN_0045EC90` (tables 0x8182C8 / 0x818320 by `Foundation` enum +0xEF0) | | CONFIRMED |

## 2. Deploy mission — `FUN_007383E0` (UnitClass, the unfold)

1. No `DeploysInto` → nothing. The unit lifts its own occupancy (`vtbl+0x124 Mark(0)`).
2. Origin cell = the unit's cell; **if the foundation width > 2 or height > 2,
   origin += AdjacentCell[NW] = (-1,-1)** (@0x738480–0x7384DE).
3. Legality: `DeploysInto->vtbl+0xA8(cell, 0)` = `FUN_00464AB0` →
   `FUN_007154F0`: every foundation cell must pass the cell clear-to-build test;
   **no adjacency/proximity test**.
4. Illegal: if the owner is the player (`FUN_0050B510`) → `EVA_CannotDeployHere`
   (string 0x81F13C via `FUN_00751520`); an AI house picks another spot
   (`FUN_0045EE60`); mission → Guard (5).
5. Legal: compare the body facing, as `((raw16 >> 7) + 1) >> 1 & 0xFF`, with
   `DeploysInto->DeployFacing` (`FUN_00465D60` = `+0xEDC`). Not equal → turn
   the body toward it and return (retry next tick).
6. Facing reached: `new BuildingClass(DeploysInto, Owner)`, mission
   **Construction (0x12)** = the buildup animation, `Unlimbo` at
   `cell*256+128`; passengers/occupants transferred; health =
   `ftol(HealthRatio × Strength)`, **min 1** (`FUN_007C4D00`); veterancy block
   copied; for an AI ConYard in MP the house's base centre is moved here
   (`FUN_0050DE20`, AI concern). The unit is then removed (`vtbl+0x3A0`,
   `vtbl+0xF8`) — not killed.

## 3. Try-to-deploy — `FUN_00737D50` (the order)

* Legal at the current cell (same origin/legality as §2) → accept.
* Otherwise the unit scans a **36-entry offset ring** (CellStruct table
  0x00B1BF78..0x00B1C004, static-initialised in the function) and is given
  a destination (`vtbl+0x480`) at the **first** offset where the building is
  legal: `(0,-1) (0,-2) (1,-2) (-1,-2) (0,-3) (1,-3) (-1,-3) (2,-3) (-2,-3)
  (0,-4) (1,-4) (-1,-4) (2,-4) (-2,-4)`, the same 14 mirrored to `y>0`, then
  `(-1..-4, 0)` and `(1..4, 0)`.
* A branch keyed on a rules `BuildConst` list and house flag `+0x1F1` moves an
  MP start MCV to a computed base cell; it is an AI/start-placement concern
  and is not reproduced. [RE-FALLBACK: not implemented]

## 4. Undeploy (`UndeploysInto`)

Already recovered in `repair-sell.md §B6` (`BuildingClass::Can_Undeploy`
`FUN_0044F5C0`): a `ConstructionYard=yes` type packs into its `UndeploysInto`
unit only via the **Sell** command, in a multiplayer/skirmish session, for a
human owner, with the `MCVRedeploys` option on (rules
`[MultiplayerDialogSettings] MCVRedeploys=yes`); otherwise the plain refund.
No `EVA_StructureSold`; health ratio carried (min 1); placement failure falls
back to the refund. The clone spawns the unit on the cell the MCV deployed
from (origin + (1,1) for a > 2 foundation) facing `DeployFacing`.

## 5. Skirmish game-over (RE-I8) — `HouseClass::Flag_To_Win` `FUN_004FC800`, `Flag_To_Lose` `FUN_004FC9F0`

| Session | Win | Lose |
|---|---|---|
| Campaign (`0x00A8A1A0 == 0`) | text `TXT_SCENARIO_WON`, EVA `EVA_MissionAccomplished` | text `TXT_SCENARIO_LOST`, EVA `EVA_MissionFailed` |
| MP / skirmish (player house only) | text `TXT_VICTORIOUS` (type 2), EVA `EVA_YouAreVictorious` | text `TXT_LOST` (type 3), EVA `EVA_YouHaveLost` (not for an observer) |

Both arm **BorrowedTime** = `ftol(SavourDelay …)` frames (the campaign value the
clone already uses, `borrowed_time_frames`); a network session (`!= 0 && != 5`)
rounds it up to a multiple of 10 frames and at least MaxAhead — skirmish
(session 5) does not. `FUN_004F4200(0)` then ends the scenario when the timer
runs out. A defeated opponent (not the player) gets `EVA_PlayerDefeated` +
`TXT_PLAYER_DEFEATED` (`FUN_004FBED0`, `MPlayer_Defeated`).
The MP score screen that follows is not recovered; the clone returns to its
result screen and the main menu. [RE-FALLBACK: score screen]

## 6. Skirmish start (RE-E2)

`multihouse.md §6`: each combatant starts with **an MCV plus `UnitCount`
starting units** around its start waypoint (ANCHORED). The MCV type is the
house side's entry of `[General] BaseUnit=AMCV,SMCV,PCV`.
