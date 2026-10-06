# Save / Load — RE hunt (P7)

Target: `gamemd.exe` YR 1.001 (see `workflow.md` §0). Method: string scan of the
binary + YRpp layout map (`LoadOptionsClass.h`, `ScenarioClass.h`) + a headless
Ghidra pass on a private project copy (`D:\ra2-re\p7proj`, script
`D:\ra2-re\scripts\p7_save1.py`). Scope was deliberately short: the **UI, flow
and file naming** only. The on-disk payload is not recovered (see §4 — it is a
raw class-memory dump and is not reproducible by a from-scratch engine).

Confidence tags: **CONFIRMED** = read in the decompile; **YRpp** = from the
YRpp layout map, not re-verified; **FALLBACK** = our choice.

## 1. Entry points

| Address | Name (YRpp) | Role |
|---|---|---|
| `0x558740` | `LoadOptionsClass` ctor | `Mode`, `Extension` (`"SAV"`), `SpaceRequirement` (0x800) |
| `0x5587F0` | `LoadOptionsClass::LoadDialog` | opens the shared Load/Save/Delete dialog |
| `0x558A60` | (dialog body, CONFIRMED) | one modal loop for all three modes |
| `0x559330` | file-list fill | enumerates `*.%3s` (the extension), one `FileEntryClass` per file |
| `0x559ED0` | `GetFileEntry` | reads the header of one file into a `FileEntryClass` |
| `0x559D60` / `0x559E7B` / `0x559EB0` | Load/Save/DeleteMission | vtable +4 / +8 / +0xC of the dialog object |
| `0x67CEF0` | `ScenarioClass::SaveGame(file, desc, bargraph)` | the actual writer |
| `0x67E440` | `ScenarioClass::LoadGame(file)` | the actual reader |
| `0x67FD20` | `SavegameInformation::ReadFromFile` | header-only read for the list |

## 2. Dialog flow (FUN_00558a60, CONFIRMED)

One dialog resource serves three modes (`LoadOptionsMode` 1=Load, 2=Save,
3=Delete); controls: list `0x525`, description edit `0x526`, Load `0x40F`,
Save `0x6C7`, Delete `0x6C8`, Back `0x686`.

* **Load (1)**: list of saves; Load button enabled iff the list is non-empty;
  double-click (`LBN_DBLCLK` on `0x525`) == Load. An entry flagged
  `IsWrongVersion` (`FileEntryClass+0x1B5`) is refused. On failure a message
  box is shown and the dialog stays up.
* **Save (2)**: first a free-disk check (`< SpaceRequirement` -> error box).
  The edit box is limited to **0x50 = 80 characters** (`EM_LIMITTEXT`-style
  `0x4B2`). An **empty description is refused** (error box, focus back to the
  edit). If the selected list row is the "new slot" row (entry `+0x1B4 == 0`)
  the filename is generated as **`SAVE%04lX.%3s`** with `rand()` as the
  number, retried until no such file exists; otherwise the selected file is
  **overwritten after a `TXT_CONFIRM_SAVE` yes/no**. Success -> the
  "game was saved" message (`GetUIString_GameSaved`, `TXT_GAME_WAS_SAVED`);
  failure -> `TXT_ERROR_SAVING_GAME`.
* **Delete (3)**: `"%s %s"` confirm (prompt + description), delete the file,
  remove the row; the dialog stays open while rows remain.
* The list is sorted **newest first** (`FileEntryClass::Compare` = negated
  `CompareFileTime(LastWriteTime)`, YRpp).

## 3. Where the dialog is reached (FUN_00603850 tooltip table, CONFIRMED)

* In-game single-player options dialog **0xB5**: buttons `Controls` 0x521,
  `Load` 0x51E, `Save` 0x51F, `Delete` 0x520, `Abort` 0x522, `Back/Resume`
  0x686 (`STT:IGSingleOpt*`). The multiplayer variant (0xBBA) has no Save/Load.
* Main menu: `STT:SingleButtonLoadSavedGame` ("Load Saved Game") on the
  single-player page.
* Default description: `"%sSav"` formatted with the scenario's `UIName` and
  looked up in the CSF (falls back when `MISSING`) — i.e. the description
  defaults to the scenario's display name (FUN_00686560, CONFIRMED).
* While writing: `TXT_SAVING_GAME` ("Saving game...") banner (FUN_004c6bd0).

## 4. On-disk format (YRpp + strings; not re-implemented)

The original file is an **OLE structured-storage compound document**
(`OleSaveToStream`, `IStorage`): a `SummaryInformation` property set
(`SavegameInformation`: `InternalVersion`(PIDSI_CHARCOUNT), `Version`
(PIDSI_REVNUMBER), `ScenarioDescription`(PIDSI_TITLE, 0x80 wchar),
`PlayerHouse`(PIDSI_SUBJECT), `Campaign`/`ScenarioNumber`/`GameType` (custom
ids 101/100/102), player name, exe name, start/play/last-save FILETIMEs) plus
streams that are **raw `IPersistStream` dumps of every live class instance**
(pointers swizzled), gated by `Savegame_Magic` (0x83D560). That payload is a
memory image of the original's classes; a from-scratch engine cannot produce
or consume it meaningfully.

**Decision (FALLBACK, documented in the spec):** keep the original's
*file naming* (`SAVE%04lX.SAV`, newest-first list, 80-char description, the
description-as-title header the list reads without loading the body) and its
*UI flow*, but use our **own versioned binary payload** (`RA2SAVE` magic,
format version, header block, then length-prefixed World / map-trigger /
scene sections).
