# Presentation layer: recovered behavior (EVA, music, cursors, shroud, anims)

Calibration RE against `gamemd.exe` (YR 1.001, SHA1 `7BE4E550…`). Ghidra project:
private copy `D:\ra2-re\fixE` (copy of `p4proj`). Scripts are in `D:\ra2-re\fixE\scripts\fx.py`
(a generic job runner: `str`/`fn`/`ctx`/`dwords`/`range` commands) with job files `job*.txt`.
Output goes to `D:\ra2-re\fixEout\<tag>\`. The excerpts below are short pseudo-C or
disassembly paraphrases. Confidence: **High** means read directly in the binary with clear
control flow. **Medium** means the flow was read but a field name or meaning was inferred from
YRpp or the INI. **Low** means inferred.

Conventions used below:

- `PlayerPtr` = `DAT_00a82cb4` (the local house).
- `GameMode` = `DAT_00a8a1a0` (0 = campaign, nonzero = skirmish or MP).
- `Frame` = `DAT_00a8dcec`.
- `Rules` = `DAT_00886160`.
- `EVA_Say(name)` = `FUN_00751520(name)`. This is a by-name lookup in `DAT_00b1c40c` followed
  by `FUN_007512a0(index, -1, -1)`.
- `IsPlayerControl(house)` = `FUN_0050b510`. Its body is
  `GameMode ? house == PlayerPtr : (house->IsHuman(+0x1ec) || house->PlayerControl(+0x1ed))`.
  In skirmish this simply means "house is the local player".
- `RadarEvent::Create(type, cell)` = `FUN_0065f4b0` (ECX = type). It returns **false**
  when the event is suppressed. **Many EVA lines are called only if this returns true**, so
  radar-event suppression is the real EVA throttle (see 1.2).

---

## 1. EVA trigger points and throttling

### 1.1 The EVA queue and its minimum gap

`FUN_007512a0` (speak) and `FUN_00751580` (pump) are described in `audio-playback.md` §8.
New finding: the minimum-gap value.

```c
// FUN_00751580 (pump)
if (!eva_stream_busy(stream) /*FUN_00408070: stream+0xc & 1*/ &&
    stream->time64(+0x20) /*FUN_00408140*/ + DAT_00b1c438 < Clock_ms() /*FUN_004093b0*/ &&
    DAT_00b1c390 == 0) {
   ... dequeue next ...
   if (play_ok) { DAT_00b1c438 = 500; DAT_00b1c43c = 0; DAT_00b1c42c = entry; }
}
// FUN_007512a0, INTERRUPT path: stop current, clear queue, DAT_00b1c438 = 0
```

- `Clock_ms`: `FUN_004093b0` jumps through `DAT_0087d7c8`, which `FUN_00409360` sets to a
  QueryPerformanceCounter reader (frequency divided by 1000). It returns **milliseconds**,
  so the gap is wall-clock time and not game frames.
- **Gap = 500 ms** (a 64-bit value). It is measured from the timestamp the audio stream
  keeps at `stream+0x20`, which is most likely the start or end time of the last voice. A new
  EVA line starts only when no EVA voice is playing **and** 500 ms have passed. An `INTERRUPT`
  line resets the gap to 0. **High** for 500 ms and the ms clock. **Medium** for what the
  `stream+0x20` timestamp is.

### 1.2 Radar-event suppression (the real per-event throttle)

`RadarEvent::Create(type, cell)` (`FUN_0065f4b0`):

```c
if (RadarEventTable[type].suppress /*+0xc*/) {
  for (ev in ActiveRadarEvents /*DAT_00b03d14, count DAT_00b03d20*/)
    if (ev->type == type && (int)sqrt(dx*dx+dy*dy) /*cells*/ < RadarEventTable[type].suppressDist)
      return false;                         // suppressed -> caller skips the EVA
}
new RadarEventClass(type, cell);            // FUN_0065f5c0: the radar "zoom square" flash
return true;
```

The static table is at `0x007EF998`, with stride 16 bytes:
`{suppressDist (cells), visibilityFrames, durationFrames, suppressFlag}`. `[General]`
`RadarEventSuppressionDistances/VisibilityDurations/Durations` (parsed in `FUN_0066cf70`,
stored into `Rules+0x43c/+0x458/+0x474` vectors) override the first 6 entries. In rulesmd
those are `8,8,8,8,8,6` / `200`x6 / `400`x6.

| # | RadarEventType (YRpp) | dist | vis | dur | suppress | EVA that uses it |
|--:|---|--:|--:|--:|:-:|---|
| 0 | Combat | 8 | 200 | 400 | 1 | |
| 1 | Noncombat | 8 | 200 | 400 | 0 | |
| 2 | DropZone | 8 | 200 | 400 | 0 | |
| 3 | BaseAttacked | 8 | 200 | 600 (INI: 400) | 1 | `EVA_OurBaseIsUnderAttack` |
| 4 | HarvesterAttacked | 8 | 200 | 400 | 1 | `EVA_OreMinerUnderAttack` |
| 5 | EnemySensed | 6 | 200 | 400 | 1 | |
| 6 | UnitProduced | **2** | 0 | 200 | 1 | `EVA_UnitReady` |
| 7 | UnitLost | 8 | 0 | 200 | 1 | `EVA_UnitLost` |
| 8 | UnitRepaired | 2 | 0 | 400 | 1 | `EVA_UnitRepaired` |
| 9 | BuildingInfiltrated | 5 | 0 | 400 | 0 | |
| 10 | BuildingCaptured | 8 | 0 | 100 | 0 | `EVA_BuildingCaptured` |
| 11 | BeaconPlaced | 8 | 200 | 200 | 1 | `EVA_BeaconDetected` |
| 12 | SuperweaponDetected | 8 | 200 | 400 | 0 | (SW detected) |
| 13 | SuperweaponActivated | 8 | 0 | 5 | 0 | |
| 14 | BridgeRepaired | 8 | 0 | 200 | 1 | `EVA_BridgeRepaired` |
| 15 | GarrisonAbandoned | 8 | 0 | 400 | 1 | `EVA_StructureAbandoned` |
| 16 | AllyBaseAttacked | 8 | 200 | 600 | 1 | `EVA_OurAllyIsUnderAttack` |

Event lifetime (`FUN_0065f840`, per-event AI): the event first plays a "zoom-in" square
on the radar. It starts at the radar size and shrinks by `RadarEventSpeed` per frame down
to `RadarEventMinRadius`, rotating at `RadarEventRotationSpeed` and colour-cycling at
`RadarEventColorSpeed`. It then starts two timers: `+0x30` = visibility (table `+4`) and
`+0x24` = duration (table `+8`). The draw-active flag (`+0x3d`) clears when the visibility
timer runs out.

The suppression loop scans every event still in the list. The list-removal function was not
read. The most likely behaviour is that the event leaves the list when its duration expires.

- So **"base under attack" fires at most once per 8-cell neighbourhood while the earlier
  BaseAttacked event is alive**. That is roughly the zoom-in time plus 400 frames (INI
  value). A hit on a building more than 8 cells from every live BaseAttacked event fires
  again immediately.
- **Yes, it flashes the radar**: the created RadarEvent *is* the minimap flash/zoom square.
- **High** for the table values, the suppression predicate, and the EVA gating. **Medium**
  for the exact removal time.

### 1.3 Per-line trigger table

All lines go through the queue in 1.1. "Local" means `IsPlayerControl(house)`, i.e. the
local player in skirmish.

| EVA line | Function (call site) | Trigger | Gating / throttle | Conf |
|---|---|---|---|:-:|
| `EVA_ConstructionComplete` | `FUN_006a81c0` @6a84ba (StripClass::AI, sidebar) | A sidebar cameo's factory reports completed and the object is a **Building** (RTTI 6). Then `FUN_00733320` (ready-to-place). Units (RTTI 1/2/0xF) instead queue a PLACE event (`0xb`) with no EVA here. | Sidebar is local-only. No throttle. | High |
| `EVA_UnitReady` | `FUN_004faf00` @4fb45f (HouseClass place/exit-from-factory event handler) | A produced unit/infantry/aircraft successfully exits its factory. | Local house only. **`RadarEvent::Create(6 UnitProduced, factoryExitCell)` must succeed**: suppress radius **2 cells**, duration 200 frames. So rapid exits from the same factory within the event lifetime are silent. | High |
| `EVA_Building` / `EVA_Training` | `FUN_006aa390` @6aab19/6aab23, 6aad44/6aad54 (sidebar cameo click) | Left-click a cameo that starts production. `EVA_Building` for buildings, `EVA_Training` otherwise (selected on RTTI). | Local UI only. Event `0xE` (PRODUCE) is sent. | High |
| `EVA_OnHold` | `FUN_006aa390` @6aa692, 6aa793 | Right-click a cameo that is in production → pause (event `0xF`/`0x10` SUSPEND). | Local | High |
| `EVA_Canceled` | `FUN_006aa390` @6aa4c4 | Right-click a cameo already on hold → abandon (event `0x10`; with shift `0x2E` = abandon all). Also used for the SW-target cancel path at line ~77. | Local | High |
| `EVA_UnableToComply` | `FUN_006aa390` @6aaa3c, 6aad1e | Clicked cameo cannot be built now (e.g. factory busy with another type / RTTI 7 case). | Local | Medium |
| `EVA_InsufficientFunds` | (a) `FUN_004f8350` @4f89c4 (HouseClass::AI) | `house == PlayerPtr`, the timer `house+0x57d4` has expired, `AvailableMoney() < 100`, and the house has production in progress (`+0x537c..+0x5388` sum > 0). After speaking, the timer is re-armed to **`SpeakDelay` (Rules+0x16a8 = 2.0 min) × 900 frames/min**, speed-adjusted via `FUN_005faad0` (`frames*8/(GameSpeed+1)` for values ≥ 5). The same timer is also re-armed when money is healthy (`credits > 50` and a small delta test). | Local, **once per SpeakDelay (≈1800 frames at normal speed)**. | High (timer) / Medium (money test detail) |
| | (b) `FUN_0044b780` @44bfe5 (BuildingClass repair step) | Repairing a building and the owner's money is 0. | Gated by the building's `+0x41a` owner-is-player flag (Medium). | Medium |
| `EVA_LowPower` | `FUN_004f8350` @4f8b38 | `house == PlayerPtr`, `PowerOutput (+0x53a4) < PowerDrain (+0x53a8)` with drain ≠ 0 and the ratio below the `0x7e0718` constant, **and** the house owns ≥1 of `Rules+0x8b0[0..2]` (3-entry list; most likely `BuildConst` = GACNST/NACNST/YACNST, Medium). | **Once per low-power episode**: global flag `DAT_00a8dfac` is set when spoken and cleared when power is no longer low. It also posts the `TXT_LOW_POWER` message for `MessageDelay` (0.6 min) and re-arms `house+0x57bc` with SpeakDelay. No separate radar-down sound was found here (radar on/off is handled by the radar-tab code). | High |
| `EVA_OurBaseIsUnderAttack` | `FUN_004f9200` @4f9376 (HouseClass::BuildingUnderAttack) | Called from `FUN_004421b0` @4428fb (BuildingClass::ReceiveDamage) when **damage > 0 and attacker ≠ null**, the building type's `+0x232` flag is clear, and not (vtbl+0x80) (e.g. not iron-curtained). Also called from `FUN_0071a1a0` (chrono-legionnaire erase of a building). | House is the local player (campaign: IsHuman/PlayerControl). `RadarEvent::Create(3, buildingCell)` must succeed (8 cells, see 1.2). On success it also **plays `BaseUnderAttackSound` (Rules+0x184, `BaseUnderAttackSiren`=`ubase`, Priority critical)** via `FUN_0074f740(snd, 1.0f)`. | High |
| `EVA_OreMinerUnderAttack` | `FUN_004f9200` @4f9316 (building that is a harvester type, `type+0x408 && type+0x5ec`, e.g. deployed slave miner); `FUN_00736d60` @73756d (UnitClass::ReceiveDamage); `FUN_0071a1a0` @71a2fc (chrono attack on a harvester) | Damage result ≠ none/destroyed on a unit whose type is `Harvester` (`+0xe0e`), owned by `PlayerPtr`. | `RadarEvent::Create(4, cell)`, 8 cells. **No siren.** | High |
| `EVA_OurAllyIsUnderAttack` | `FUN_004f9200` @4f93ce | Building of an **allied** (non-local) house is attacked. | `RadarEvent::Create(16, cell)`, 8 cells, plus the siren. | High |
| (units) | | **There is no generic "unit under attack" EVA.** Only harvesters (OreMiner) and buildings. | | High |
| `EVA_UnitLost` | `FUN_004d97d0` @4d981c (FootClass virtual, 4 vtables = Unit/Infantry/Aircraft/...) | Unit dies. | `IsPlayerControl(owner)`, type `+0xd54` flag clear (likely `DontScore`/insignificant), `RadarEvent::Create(7, cell)` (8 cells, 200 frames). | High |
| `EVA_NewConstructionOptions` | `FUN_006a5990` @6a5aa0 (SidebarClass::Add(rtti,id)), `FUN_006a7e80` @6a7ec2 (StripClass::Add) | A cameo **not already present** is added to a strip (strip not full: < 0x4C / 76 entries). | Skipped when `DAT_00a8d714 != 0` (bulk/initial population) and for RTTI `0x1F` (superweapons). One request per new cameo, and the queue de-dupes. So: **when the buildable set grows**. Called from `FUN_0050aff0` (house tech recalc) and the event handler `FUN_006dce20`. | High |
| `EVA_StructureSold` | `FUN_00449b70` @449c20, 44ab34 (BuildingClass mission Deconstruction/sell) | Building sold. The first site skips types with `+0x408 != 0`. | Building `+0x41a` byte (owner-is-player) | Medium |
| `EVA_UnitSold` | `FUN_00449b70` @44aa83 (unit sold on a repair depot / grinder); `FUN_004d9e80` @4d9e9f (FootClass sold) | Unit sold | `+0x41a` / `IsPlayerControl`. Plays a sound too (`FUN_0074f740`). | High |
| `EVA_BuildingCaptured` | `FUN_004481ab` @4483d5 (BuildingClass capture) | Captured building | If the building type has a per-type EVA index (`type+0x1554 != -1`) **and** `IsPlayerControl(newOwner)`, that index is spoken directly. Otherwise `RadarEvent::Create(10, cell)` (never suppressed) then `EVA_BuildingCaptured`. `EVA_TechBuildingLost` @448373 is the loser's line. | Medium |
| `EVA_UnitPromoted` | `FUN_006f9380` @6f95f6 / 6f9664 (TechnoClass veterancy update) | Veteran rank reached (plays `Rules+0x22c` UpgradeVeteranSound at the unit) and elite (`Rules+0x228`). | `IsPlayerControl(owner)`. No throttle. | High |
| `EVA_*Ready` (SW) | `FUN_006cb320` @6cb466 switch (SuperClass::AI, called from HouseClass::AI with `isLocal = house==PlayerPtr`); `FUN_006cb700` (SuperClass force-charge) | The charge timer (`+0x30` start, `+0x38` length) completes and `isLocal`. Switch on `SWType+0xb4`: 0 `EVA_NuclearMissileReady`, 1 `EVA_IronCurtainReady`, 2 `EVA_ForceShieldReady`, 3 `EVA_LightningStormReady`, 4 `EVA_PsychicDominatorReady`, 5 `EVA_ChronosphereReady`, 6 `EVA_ReinforcementsReady`, 7 `EVA_SpyPlaneReady`, 8 `EVA_GeneticMutatorReady`, 9 `EVA_PsychicRevealReady`, 10/11 none. | Local, once per charge | High |
| `EVA_*Detected` | `FUN_00445ee0` @4468a8 (BuildingClass::Unlimbo/placement) | A SW building is placed. Always `RadarEvent::Create(12, cell)` (radar flash). EVA only if the **owner is not the player, not allied to the player, `DAT_00a8a4a0 == 0`, `GameMode != 0` (MP/skirmish only)**, and the house owns the SW. Switch on SW type: 0 `EVA_NuclearSiloDetected`, 1 `EVA_IronCurtainDetected`, 4 `EVA_PsychicDominatorDetected`, 5 `EVA_ChronosphereDetected`, 8 `EVA_GeneticMutatorDetected`, ... | | High |
| `EVA_*Launched/Activated` | `FUN_006cba10` (SuperClass::Place/launch) @6cd208, 6cd371 (nuke), 6cc27c Chrono, 6cc2fa LightningStormCreated, 6cc373 PsychicDominatorActivated (radar-event gated), 6cc49a IronCurtainActivated, 6cce36 GeneticMutator | Played on launch **for every player** (no local check) when `DAT_00a8a4a0 == 0`. | | High |
| `EVA_YouAreVictorious` | `FUN_004fc800` @4fc9c4 (HouseClass::Flag_To_Win) | Win. Shows `TXT_VICTORIOUS`. | Campaign: human house. MP: `house == PlayerPtr`. Once (`+0x1f7`). | High |
| `EVA_YouHaveLost` | `FUN_004fc9f0` @4fcbbc (Flag_To_Lose); `FUN_004fbed0` @4fc105 (MP defeat with message, `MessageDelay`) | Lose | Local. `EVA_PlayerDefeated` @4fc1d7 for other players. | High |
| `EVA_PrimaryBuildingSelected` | `FUN_004481ab`-region @448171 (BuildingClass set-primary) | Factory made primary. Other same-type factories get `+0x3d3=0`. | `IsPlayerControl(owner)` | High |
| `EVA_NewRallyPointEstablished` | `FUN_004437d0` @4439d4 (BuildingClass assign rally/waypoint) | Rally point set on a factory | the second argument (from-player-click) and `IsPlayerControl` and type `+0x16b9`/`+0x5ed` flags clear | High |
| `EVA_CannotDeployHere` | `FUN_004faf00` @4fb192 (building placement failed, `house==PlayerPtr`); `FUN_004ab8c0` @4abb8b (DisplayClass placement click invalid); `FUN_007383e0` @738522 (UnitClass deploy into building, e.g. MCV, fails) | | Local | High |
| `EVA_Repairing` | `FUN_0044b780` @44c502 (service-depot repair begins), `FUN_00446f40` @447002 (BuildingClass::Toggle repair, when turning repair **on** and not at full strength) | | `IsPlayerControl` / `+0x41a` | High |
| `EVA_BuildingOffLine` / `EVA_BuildingOnLine` | `FUN_00452350` / `FUN_00452250` (Building go offline / online, power toggle) | Player toggles power on a building with power drain (`type+0xee4 > 0`), or online restore. Both are called from the event dispatcher `FUN_006dce20`. | `IsPlayerControl` | High |
| `EVA_EstablishBattlefieldControl` | none | **The string is not present in gamemd.exe.** It is only reachable from map/trigger speech actions by index. | | High |
| `EVA_BattleControlTerminated` | `FUN_00685fb0` @68604f (game shutdown, called from `FUN_0055cc60`) | Leaving a game: stops the theme (`FUN_00720140(1)`), says the line, then pumps the EVA until it finishes or 300 timer units pass (`DAT_008862c0`). | | High |
| `EVA_UnitRepaired` | `FUN_0044b780` @44b96e/44bdc0 | Repair complete | `RadarEvent::Create(8)` (2 cells) | High |

Other strings located, for completeness: `EVA_BeaconPlaced`/`Detected` (`FUN_00430b20`,
radar event 11), `EVA_AllianceRequested/Broken` (`FUN_004f9990`/`FUN_004f9db0`),
`EVA_RobotTanksOffline/BackOnline` (`FUN_0050df00`/`FUN_0050de30`/`FUN_00502410`), spy
infiltration lines (`FUN_004571e0`), `EVA_StructureGarrisoned` (`FUN_00522640`),
`EVA_StructureAbandoned` (`FUN_004581f0`, radar event 15), `EVA_BridgeRepaired`
(`FUN_00519449`, radar event 14), `EVA_Unit*Upgraded` (crates, `0x482c66`…).

---

## 2. In-game music (ThemeClass)

In this build the YRpp addresses for ThemeClass are wrong; they land in Tiberium code. The real
code lives at `0x71F720..0x720560`. The instance is `ThemeClass Theme @ 0x00A82C78`.

| Off | Global | Field |
|---|---|---|
| +0x00 | `a82c78` | playing index (Stop sets it to -1) |
| +0x04 | `a82c7c` | **current** theme (passed to Next_Song) |
| +0x08 | `a82c80` | **queued** theme (-1 none, **-2 = "pick next"**, -3 = "music off") |
| +0x0c | `a82c84` | volume 0..255 |
| +0x10 | `a82c88` | IsScoreRepeat |
| +0x11 | `a82c89` | IsFading |
| +0x12 | `a82c8a` | IsScoreShuffle |
| +0x18/+0x24 | | `ThemeControl*` vector / count |
| +0x2c | `a82ca4` | stream |

`ThemeControl` (0x290 bytes, ctor inside `FUN_0071f830`) has these defaults: **`Normal=yes`**,
`Repeat=no`, `Scenario=0`, `Side=-1`, `Exists=0`. INI reader `FUN_0071f720` reads
`Sound`, `Scenario`, `Normal`, `Repeat`, `Side`, and `Name` (only when Normal). The list loader
`FUN_0071f830` walks `[Themes]` in key order. It skips empty values (`;Grinder` comments)
and duplicates. Theme index order = list order.

**IsAvailable(i)** (`FUN_007203e0`):

```c
t = themes[i];
if (!t->Exists /*+0x28a, audio file found by Scan FUN_0071fa90*/) return false;
if (!t->Normal) return false;
if (PlayerPtr && t->Side != -1 && PlayerPtr->Type->SideIndex(+0xbc) != t->Side) return false;
if (GameMode == 0 /*campaign only*/ && Scen->Number(+0x1254) < t->Scenario) return false;
return true;
```

So in **skirmish/MP, `Scenario=` is ignored and `Side=` is honoured** (against the local
player's side). **High**.

**Next_Song(cur)** (`FUN_0071fd20`):

```c
if (cur < 0 || (!themes[cur]->Repeat && !IsScoreRepeat)) {
  if (IsScoreShuffle) {            // random, != cur, available; 1000 tries then 0
    do r = Random(0, count-1); while (r == cur || !IsAvailable(r));
    return r;
  }
  do { cur = (cur+1) % count; } while (!IsAvailable(cur));  // sequential wrap (gives up after count+1 -> 0)
  return cur;
}
return cur;                         // Repeat=yes on the track OR global IsScoreRepeat -> loop same track
```

**AI** (`FUN_0071fc70`, per frame):

```c
if (stream && stream_busy) return;                     // still playing
if (queued != -1 && queued != -3 && !DAT_00a8d714) {
  if (queued == -2) queued = Next_Song(current);       // "Theme::AI(Next song = %d)"
  Play(queued); queued = -2;                           // after any song ends, the next is auto-picked
}
```

So when a track ends, the next one starts on the next AI tick **with no added gap**. AI
frequency is per game loop iteration. The no-gap claim is **Medium**: there is no timer in AI,
but the call site rate was not read.

**Play(i)** (`FUN_0071fe50`): stops the current track if it differs, then opens
`<Sound>.wav` (format `DAT_00843708`) and plays it. Its log line says "Repeating" if
`IsScoreRepeat || theme.Repeat`. **Stop(fade)** (`FUN_00720140`): fades if asked
(`FUN_004080c0`) and sets all indices to -1.

**Game start** (`FUN_0055cff0`, scenario start, when `DAT_00a8d2e0 == 1`):

```c
cur = (Theme.current == -1) ? Theme.queued : Theme.current;
if (!Options.MusicOn /*DAT_00a8db0d = Options+0x45, default 1*/) { if (cur != -3) { Stop(fade); Queue(-3); } }
else if (cur == -1) Queue(-2);     // auto-pick via Next_Song(-1)
```

Next_Song(-1) means: **with shuffle off (the default), the first available track in list
order**. In YR's `thememd.ini` that is `BrainFreeze`, because INTRO/SCORE/LOADING/CREDITS
are `Normal=no` (and `RA2Options` is assumed the same, not checked). With shuffle on, it is a
random available track. **High** for the logic. **Medium** on whether `theme.ini` entries are
merged in front; only the `thememd.ini` section loader was read.

**Options defaults** (`OptionsClass` ctor `0x5F9B40`, object `0xA8DAC8`), overridden by
`RA2MD.INI [Audio]` (`FUN_005f9e10`):

| Key | Default | Notes |
|---|---|---|
| `SoundVolume` | **0.7** | clamped to ≤ 1.0 |
| `VoiceVolume` | **0.7** | |
| `ScoreVolume` | **0.4** | clamped to ≤ 1.0. Theme volume = `(int)(ScoreVolume*255)` (`FUN_00720530`, cap 255). Stream gain = `ScoreVolume*16384`. |
| `IsScoreRepeat` | **no** | yes = loop the current track forever |
| `IsScoreShuffle` | **no** | yes = random order |
| music on (`+0x45`) | 1 | toggled by the in-game music dialog Play/Stop (`FUN_006b5980` buttons 0x531/0x535) |

**`Repeat=yes` on a theme loops that track** (Next_Song returns the same index). All the
special themes (INTRO, LOADING, CREDITS, SCORE) are Repeat=yes and Normal=no, so they are
never auto-picked. **High**.

---

## 3. Mouse cursors (`MouseCursor` table @ `0x0082C038`)

The table has **86 entries × 28 bytes**: `{Frame, Count, Interval, MiniFrame, MiniCount, HotX, HotY}`. This is the
YRpp `MouseCursor` layout. YRpp's address `0x82D028` is wrong for this build; that address holds
string data here. Readers are `FUN_005bd6b0` (SetCursor), `FUN_005bd7f0` (per-frame animate), `FUN_005bd4e0`
(switch to the minimap variant) and `FUN_005bd5c0` (frame lookup). Frames index into `MOUSE.SHA`.

- **Mini variant:** used while the pointer is over the radar/minimap, but only when `MiniFrame != -1`
  (`SetCursor` forces mini = false when MiniFrame == -1). In mini mode the frame is `MiniFrame + n` and it
  cycles modulo `MiniCount`.
- **Animation:** the cursor animates only if `Interval != 0`. The frame counter advances by 1 every
  `Interval` ticks and wraps at `Count` (or at `MiniCount` in mini mode). The tick source is `FUN_006c82c0`,
  which returns `timeGetTime() >> 4`, i.e. **16 ms ticks (62.5 Hz) of wall-clock time, not game frames**.
  `Interval = 4` therefore means a new frame every 64 ms. Changing cursors resets the frame to 0.
- **Hotspot:** `HotX`: `0`=Left (x=0), `12345`=Center (x = SHP width/2), `54321`=Right (x = width).
  `HotY` works the same way with Top, Middle and Bottom. Width and height come from the MOUSE.SHA header
  (`DAT_00abe204+2/+4`). Confidence **High** (table values dumped byte-exact, code read).

| Idx | YRpp name | Frame | Count | Interval | MiniFrame | MiniCount | Hot (X,Y) |
|---|---|--:|--:|--:|--:|--:|---|
| 0x00 | Default | 0 | 1 | 0 | 1 | 1 | L,T |
| 0x01 | Move_N | 2 | 1 | 0 | -1 | -1 | C,T |
| 0x02 | Move_NE | 3 | 1 | 0 | -1 | -1 | R,T |
| 0x03 | Move_E | 4 | 1 | 0 | -1 | -1 | R,M |
| 0x04 | Move_SE | 5 | 1 | 0 | -1 | -1 | R,B |
| 0x05 | Move_S | 6 | 1 | 0 | -1 | -1 | C,B |
| 0x06 | Move_SW | 7 | 1 | 0 | -1 | -1 | L,B |
| 0x07 | Move_W | 8 | 1 | 0 | -1 | -1 | L,M |
| 0x08 | Move_NW | 9 | 1 | 0 | -1 | -1 | L,T |
| 0x09 | NoMove_N | 10 | 1 | 0 | -1 | -1 | C,T |
| 0x0A | NoMove_NE | 11 | 1 | 0 | -1 | -1 | R,T |
| 0x0B | NoMove_E | 12 | 1 | 0 | -1 | -1 | R,M |
| 0x0C | NoMove_SE | 13 | 1 | 0 | -1 | -1 | R,B |
| 0x0D | NoMove_S | 14 | 1 | 0 | -1 | -1 | C,B |
| 0x0E | NoMove_SW | 15 | 1 | 0 | -1 | -1 | L,B |
| 0x0F | NoMove_W | 16 | 1 | 0 | -1 | -1 | L,M |
| 0x10 | NoMove_NW | 17 | 1 | 0 | -1 | -1 | L,T |
| 0x11 | Select | 18 | 13 | 4 | -1 | -1 | C,M |
| 0x12 | Move | 31 | 10 | 4 | 42 | 10 | C,M |
| 0x13 | NoMove | 41 | 1 | 0 | 52 | 1 | C,M |
| 0x14 | Attack | 53 | 5 | 4 | 63 | 5 | C,M |
| 0x15 | AttackOutOfRange | 58 | 5 | 4 | 63 | 5 | C,M |
| 0x16 | Protect | 68 | 5 | 4 | 73 | 5 | C,M |
| 0x17 | DesolatorDeploy | 78 | 10 | 4 | -1 | 10 | C,M |
| 0x18 | Cursor_18 | 88 | 1 | 0 | -1 | 1 | C,M |
| 0x19 | Enter | 89 | 10 | 4 | 100 | 10 | C,M |
| 0x1A | NoEnter | 99 | 1 | 0 | 63 | 1 | C,M |
| 0x1B | Deploy | 110 | 9 | 4 | -1 | -1 | C,M |
| 0x1C | NoDeploy | 119 | 1 | 0 | -1 | -1 | C,M |
| 0x1D | Cursor_1D | 120 | 9 | 4 | -1 | -1 | C,M |
| 0x1E | Sell | 129 | 10 | 4 | -1 | -1 | C,M |
| 0x1F | SellUnit | 139 | 10 | 4 | -1 | -1 | C,M |
| 0x20 | NoSell | 149 | 1 | 0 | -1 | -1 | C,M |
| 0x21 | Repair | 150 | 20 | 4 | -1 | -1 | C,M |
| 0x22 | EngineerRepair | 170 | 20 | 4 | -1 | -1 | C,M |
| 0x23 | NoRepair | 190 | 1 | 0 | -1 | -1 | C,M |
| 0x24 | Waypoint | 191 | 7 | 0 | -1 | -1 | C,M |
| 0x25 | Disguise | 199 | 5 | 0 | -1 | -1 | C,M |
| 0x26 | IvanBomb | 204 | 5 | 0 | -1 | -1 | C,M |
| 0x27 | MindControl | 209 | 5 | 0 | -1 | -1 | C,M |
| 0x28 | RemoveSquid | 214 | 5 | 0 | -1 | -1 | C,M |
| 0x29 | Crush | 219 | 5 | 0 | -1 | -1 | C,M |
| 0x2A | SpyTech | 224 | 5 | 0 | -1 | -1 | C,M |
| 0x2B | SpyPower | 229 | 5 | 0 | -1 | -1 | C,M |
| 0x2C | Cursor_2C | 234 | 5 | 0 | -1 | -1 | C,M |
| 0x2D | GIDeploy | 239 | 10 | 0 | -1 | -1 | C,M |
| 0x2E | Cursor_2E | 249 | 10 | 0 | -1 | -1 | C,M |
| 0x2F | ParaDrop | 259 | 10 | 0 | 516 | -1 | C,M |
| 0x30 | RallyPoint(30) | 269 | 10 | 0 | -1 | -1 | C,M |
| 0x31 | CloseWaypoint | 356 | 1 | 0 | -1 | -1 | C,M |
| 0x32 | LightningStorm | 279 | 20 | 4 | 514 | 1 | C,M |
| 0x33 | Detonate | 299 | 10 | 0 | -1 | -1 | C,M |
| 0x34 | Demolish | 309 | 10 | 4 | -1 | -1 | C,M |
| 0x35 | Nuke | 319 | 10 | 4 | 513 | 1 | C,M |
| 0x36 | BlueMove(36) | 329 | 10 | 0 | -1 | -1 | C,M |
| 0x37 | Power | 339 | 6 | 0 | -1 | -1 | C,M |
| 0x38 | NoBlueMove(38) | 345 | 1 | 0 | -1 | -1 | C,M |
| 0x39 | IronCurtain | 346 | 5 | 0 | -1 | -1 | C,M |
| 0x3A | Chronosphere | 357 | 12 | 0 | -1 | -1 | C,M |
| 0x3B | Disarm | 369 | 15 | 0 | -1 | -1 | C,M |
| 0x3C | Disallowed | 384 | 1 | 0 | -1 | -1 | C,M |
| 0x3D | Scroll | 385 | 1 | 0 | -1 | -1 | C,M |
| 0x3E | Scroll_ESW | 386 | 1 | 0 | -1 | -1 | C,M |
| 0x3F | Scroll_SW | 387 | 1 | 0 | -1 | -1 | C,M |
| 0x40 | Scroll_NSW | 388 | 1 | 0 | -1 | -1 | C,M |
| 0x41 | Scroll_NW | 389 | 1 | 0 | -1 | -1 | C,M |
| 0x42 | Scroll_NEW | 390 | 1 | 0 | -1 | -1 | C,M |
| 0x43 | Scroll_NE | 391 | 1 | 0 | -1 | -1 | C,M |
| 0x44 | Scroll_NES | 392 | 1 | 0 | -1 | -1 | C,M |
| 0x45 | Scroll_ES | 393 | 1 | 0 | -1 | -1 | C,M |
| 0x46 | Protect2 | 394 | 10 | 4 | -1 | -1 | C,M |
| 0x47 | AttackOutOfRange2 | 404 | 9 | 4 | 63 | 5 | C,M |
| 0x48 | LeaveBuilding(48) | 413 | 9 | 4 | -1 | -1 | C,M |
| 0x49 | InfantryAbsorb | 422 | 9 | 4 | -1 | -1 | C,M |
| 0x4A | NoMindControl | 431 | 1 | 0 | -1 | -1 | C,M |
| 0x4B | NoRallyPoint(4B) | 432 | 1 | 0 | -1 | -1 | C,M |
| 0x4C | Cursor_4C | 433 | 1 | 0 | -1 | -1 | C,M |
| 0x4D | Cursor_4D | 434 | 1 | 0 | -1 | -1 | C,M |
| 0x4E | Beacon | 435 | 15 | 4 | -1 | -1 | C,M |
| 0x4F | ForceShield | 450 | 10 | 4 | -1 | 1 | C,M |
| 0x50 | NoForceShield | 460 | 10 | 4 | -1 | -1 | C,M |
| 0x51 | GeneticMutator | 470 | 10 | 4 | -1 | -1 | C,M |
| 0x52 | AirStrike | 480 | 8 | 4 | -1 | -1 | C,M |
| 0x53 | PsychicDominator | 488 | 8 | 4 | 516 | -1 | C,M |
| 0x54 | PsychicReveal | 496 | 8 | 4 | 515 | 1 | C,M |
| 0x55 | SpyPlane | 504 | 8 | 4 | 512 | 1 | C,M |

How the requested cursors map onto the table:

- **Arrow** = `0x00`: frame 0, static, hotspot top-left. Its mini variant is frame 1.
- **Select** (hover over an own unit) = `0x11`: frames 18..30, 13 frames, interval 4.
- **Move** = `0x12`: frames 31..40. Its minimap variant is 42..51.
- **No-move** = `0x13`: frame 41. Minimap variant is 52.
- **Attack** = `0x14`: frames 53..57. Minimap variant is 63..67.
- **Attack out of range** = `0x15`: frames 58..62.
- **Enter** (transport, building, engineer capture) = `0x19`: frames 89..98.
- **No-enter** = `0x1A`: frame 99.
- **Deploy** = `0x1B`: frames 110..118, 9 frames.
- **No-deploy** = `0x1C`: frame 119.
- **Sell** = `0x1E`: frames 129..138.
- **No-sell** = `0x20`: frame 149.
- **Repair** = `0x21`: frames 150..169, 20 frames.
- **No-repair** = `0x23`: frame 190.
- **Nuke target** = `0x35`: frames 319..328, interval 4. Its mini variant is frame 513.
- **Edge-scroll arrows** = `0x01..0x08` (N, NE, E, SE, S, SW, W, NW): frames 2..9. Each has its hotspot on
  the matching screen edge.
- **"Cannot scroll" arrows** = `0x09..0x10`: frames 10..17.

The `0x3D..0x45` "Scroll*" entries (frames 385..393) are a separate single-frame set. The code that uses
them was not traced.

### Action → cursor mapping

This mapping comes from `FUN_004aada0` (DisplayClass::ConvertAction; Ares hooks it at `0x4AB366/0x4AB44A`).
First, when the target cell holds a waypoint (`FUN_005021d0`), actions 1 and 2 become `0x2D`, actions 3, 9
and 0xB become `0x32`, and actions 5 and 6 become `0x31`. Those are the waypoint-mode cursors. The switch then
runs on the Action value, and the function calls `this->SetCursor(cursor, minimap)` (vtbl `+0x48`).

**Visible cell** (second argument is 0):

| Action (YRpp enum) | Cursor |
|---|---|
| 1 Move | 0x12 Move |
| 2 NoMove, 0x36 | 0x13 NoMove |
| 3 Enter, 9 Capture, 0xB Repair(into depot), 0x23 EnterTunnel | 0x19 Enter |
| 4 Self_Deploy, 0x34 | 0x1B Deploy |
| 5 Attack | 0x14 Attack if the selected object can hit the target from where it is (vtbl `+0x3ac`); otherwise **0x15 AttackOutOfRange** |
| 6 Harvest | 0x15 |
| 7 Select, 8 ToggleSelect, 0x3D | 0x11 Select |
| 0xA Eaten | 0x22 |
| 0xC Sell / 0xD SellUnit / 0xE NoSell | 0x1E / 0x1F / 0x20 |
| 0xF NoRepair, 0x20 NoGRepair | 0x23 NoRepair |
| 0x1D GRepair | 0x21 Repair |
| 0x1E NoDeploy | 0x1C NoDeploy |
| 0x1F NoEnter, 0x24 | 0x1A NoEnter |
| 0x10 Sabotage, 0x37 Detonate, 0x38, 0x40 | 0x34 Demolish |
| 0x14 Nuke | 0x35 |
| 0x25 IronCurtain / 0x26 LightningStorm / 0x27, 0x28 Chrono | 0x39 / 0x32 / 0x3A |
| 0x29 ParaDrop, 0x41 | 0x2F |
| 0x35 IvanBomb | 0x26 |
| 0x39 DisarmBomb | 0x3B |
| 0x3C | 0x4E Beacon |
| 0x42 / 0x43 / 0x44 / 0x45 / 0x46 / 0x47 / 0x48 | 0x53 / 0x55 / 0x51 / 0x4F / 0x50 / 0x52 / 0x54 (SW cursors) |
| 0x11, 0x1B, 0x21, 0x22, 0x2A..0x2F, 0x31..0x33 | 0x3C Disallowed |
| 0x1A GuardArea | `FUN_00730d90()` (dynamic) |
| 0x3E, 0x3F | `FUN_00730d80()` (dynamic) |
| 0x1C | no change |
| anything else (0 None) | 0x00 arrow |

**Shrouded cell** (second switch): **Attack (5) and Move (1) both show the Move cursor (0x12)**. Action 2 shows
NoMove unless the selected unit's type has `+0xc8d`. 0xA, 0xF and 0x20 show NoRepair. 0xC..0xE show NoSell.
The rest are as in the visible table.

Confidence: **High** for the table. **Medium** for the Action-enum labels: the switch argument passes through
`FUN_0070e4e0`, which was not read, and the labels are YRpp's.

---

## 4. Shroud and fog rendering

**Per-cell entry point.** `FUN_00480100` (CellClass draw shroud) is called from `FUN_006d2bd0` and
`FUN_006d6750` (TacticalClass redraw loops). It draws at the cell's screen point minus `(30,15)`, so the
60×30 iso tile is anchored at its top-left.

```c
f = ShroudFrame(cell, /*fog*/0);  cell->ShroudFrameCache(+0x120) = f;
Draw_Shroud_Tile(pt, clip, f == -2 ? 15 : f == -1 ? 0 : f);            // FUN_0047eef0
f = ShroudFrame(cell, 1);         cell->FogFrameCache(+0x121) = f;
if ((Scen->Flags & 0x1000 /*FogOfWar*/) && !PlayerPtr->(+0x1f5))
    Draw_Fog_Tile(pt, clip, f == -2 ? 15 : f == -1 ? 0 : f);           // FUN_0047f160
```

**Frame selection.** `FUN_006d7c70(cell, mode)`. Mode 0 is shroud; it tests the "mapped" bit `0x08` of
`cell+0x12C`, and "any explored" is `0x18`. Mode 1 is fog; it tests the "visible" bit `0x02` of `cell+0x140`,
and "any" is `0x03`.

```c
if (!(self & ANY))   return -2;          // fully black -> frame 15
if (!(self & BIT))   return -1;          // -> frame 0
mask = 0;  // a neighbour bit is SET when that neighbour LACKS the bit (is shrouded/fogged)
(x-1,y-1)=0x40  (x,y-1)=0x80  (x+1,y-1)=0x01
(x-1,y  )=0x20                (x+1,y  )=0x02
(x-1,y+1)=0x10  (x,y+1)=0x08  (x+1,y+1)=0x04
return (signed char)ShroudLUT[mask];     // ShroudLUT @ 0x007F3194, 256 bytes
```

Here `x` and `y` are CellStruct X and Y (map cell coordinates, not screen coordinates).

`ShroudLUT` (byte value 0xFF = -1 → frame 0; 0xFE = -2 → frame 15). Rows are `mask` 0x00, 0x10, …, 0xF0:

```
00: ff 21 02 02 22 25 02 02 04 1a 06 06 04 1a 06 06
10: 23 2d 11 11 26 29 11 11 04 1a 06 06 04 1a 06 06
20: 08 15 0a 0a 1b 1f 0a 0a 0c 17 0e 0e 0c 17 0e 0e
30: 08 15 0a 0a 1b 1f 0a 0a 0c 17 0e 0e 0c 17 0e 0e
40: 20 24 19 19 2c 28 19 19 13 1e 14 14 13 1e 14 14
50: 27 2b 1d 1d 2a 2e 1d 1d 13 1e 14 14 13 1e 14 14
60: 08 15 0a 0a 1b 1f 0a 0a 0c 17 0e 0e 0c 17 0e 0e
70: 08 15 0a 0a 1b 1f 0a 0a 0c 17 0e 0e 0c 17 0e 0e
80: 01 01 03 03 10 10 03 03 05 05 07 07 05 05 07 07
90: 18 18 12 12 1c 1c 12 12 05 05 07 07 05 05 07 07
a0: 09 09 0b 0b 16 16 0b 0b 0d 0d fe fe 0d 0d fe fe
b0: 09 09 0b 0b 16 16 0b 0b 0d 0d fe fe 0d 0d fe fe
c0: 01 01 03 03 10 10 03 03 05 05 07 07 05 05 07 07
d0: 18 18 12 12 1c 1c 12 12 05 05 07 07 05 05 07 07
e0: 09 09 0b 0b 16 16 0b 0b 0d 0d fe fe 0d 0d fe fe
f0: 09 09 0b 0b 16 16 0b 0b 0d 0d fe fe 0d 0d fe fe
```

Frame values span 0..0x2E (47 frames). For example, `mask = 0` (all neighbours revealed) gives -1, which is
frame 0 (the empty frame). **High** for the LUT and the bit layout.

**Blit.** The tiles are not palette-drawn onto the screen. Both blitters write the SHP's **raw byte values into
a 16-bit per-pixel intensity surface** (`DAT_0087d824`). The neutral value is `0x7F`, which means full
brightness. A later pass, not traced, uses that surface to modulate the frame.

- `Draw_Shroud_Tile` (`FUN_0047eef0`): `if (p != 0xFE) dst = p;` This is a plain overwrite where 0xFE is the
  transparent key (`DAT_007e3eb8 = 0xFE`). Asset choice: `SHROUD.SHP` normally, `FOG.SHP` when
  `Scen->Flags & 0x1000` (fog-of-war game). The order of the two `FUN_005b3ae0` loads was assumed from the
  string order; **Medium**.
- `Draw_Fog_Tile` (`FUN_0047f160`, always `FOG.SHP`):
  `if (p < 0x80) dst = (dst == 0x7F) ? p : max(0, dst - 0x7F + p);` This is a subtractive combine with
  whatever darkness is already present, so overlapping fog tiles darken further.
- `BlendedFog` (`[General]`, parsed at `0x670946`) was not traced into the compose pass. **Low** on how it
  changes the final blend.

For our engine: compute `mask` per cell, look up the frame in the LUT, and treat SHROUD.SHP/FOG.SHP pixel
values as brightness, where 0x7F is no darkening and 0 is black.

---

## 7. SHP unit/infantry shadows (extra item)

**Draw path.** `FUN_00705290` (TechnoClass SHP draw helper; called for infantry and SHP units) calls
`CC_Draw_Shape` (`FUN_004aec80`).

```c
// body
CC_Draw_Shape(surf, convert, shp, frame, pt, ..., flags | 0x600 /*Centered|bf_400*/, ..., zAdj-2, ...);
if (drawShadow) {                           // a caller-supplied/stack bool
   frame += shp->Frames(+6) / 2;            // shadow frame = second half of the SHP
   flags  = (flags & ~6 /*no translucency*/) | 0x601;   // + Darken(1)
   zAdj   = -4 - heightZ;
   CC_Draw_Shape(..., frame, ..., flags, ...);
}
```

**Blitter.** `FUN_00490aa0` maps the flags to a ConvertClass blitter slot. `Darken` (bit 1) selects
`convert+0x10`, or `+0x2c`/`+0x54` with Z-read or Z-write. That slot is created in `FUN_0048eb00`.

- 16-bit surface, `FUN_00492580` (vtbl `0x7E49F0`):
  `for each src pixel: if (src != 0) dst = (dst >> 1) & convert->HalfMask(+0x180);`
  So every non-zero source pixel **halves the destination colour (50% darken)**. The source index value,
  whether 12 or anything else, is **ignored**; only zero versus non-zero matters. There is no shadow table and
  no alpha.
- 8-bit surface, `FUN_00491580` (vtbl `0x7E4B28`): `dst = ShadowTable[dst]` for non-zero source pixels.

**What this means for the claim about baked index-12 blobs.** The binary takes the infantry shadow
**only from frame + Frames/2**, darkened as above. Nothing in the body blit path or in the Darken blitter
treats palette index 12 specially. A body frame's index-12 pixels go through the normal palette blitter and
come out as palette colour 12.

If gi.shp's second half really looks empty and the blobs sit inside the first-half frames, re-check the
SHP frame count and the half split first. This is a parser-side question that I could not settle from the
exe. **High** for the shadow frame offset and the 50% darken blitter. **Medium** that `FUN_00705290` is the
path infantry use: its RTTI branches include Unit/Aircraft/Infantry, but the InfantryClass vtable call chain
was not walked.

**Buildings.** BuildingClass's own draw code, which handles the shadow frames of the building SHPs, was not
traced in this pass. Any draw that sets the `Darken` flag goes through the same blitter, so the result would
be the same 50% halving. **Low/unverified** that buildings use `Darken` for their shadow frames.

---

## 5. Anim system (AnimTypeClass / AnimClass)

`AnimTypeClass` ctor is `FUN_004274b0`. The INI reader is `FUN_00427c80`. Shape load / End default is
`FUN_00427ad0`. `AnimClass` ctor is `FUN_00421ec0(type, coord, delay, loops, flags, …)`. Start is
`FUN_00424c60`. AI is `FUN_00423ae0`.

| Key | Field | Default | Semantics |
|---|---|---|---|
| `Rate` | `+0x2B0` "frame delay" | **1** | Stored as **`900 / Rate`** (integer division) = game frames per anim frame. `Rate ≤ 0` gives 0. A missing key keeps the delay at **1, so the anim advances 1 frame every game frame**. **`Rate > 900` gives delay 0, and the anim never advances**: AI only steps when `delay != 0`. |
| `RandomRate=a,b` | `+0x2E4/+0x2E8` | 0,0 | Each is converted to 900/x, clamped so that min ≤ max. If either is set, the delay is `Random(min,max)` at start and at each Next= transition. |
| `Normalized` | `+0x362` | no | If yes, the delay is passed through `FUN_005faad0` (game-speed normalisation). |
| `Start` | `+0x2B4` | 0 | First frame. |
| `End` | `+0x2C0` | 0, which becomes **SHP frame count** (or count/2 when `Shadow=yes`) | One past the last frame of the final pass. |
| `LoopStart` | `+0x2B8` | 0 | |
| `LoopEnd` | `+0x2BC` | 0 / -1, which becomes **End** | |
| `LoopCount` | `+0x2C4` | 0 | AnimClass loops = `(byte)(LoopCount × ctorLoops)`, clamped to **≥ 1**. **-1 gives 255, which is infinite** (255 is never decremented). |
| `Next` | `+0x2C8` | none | Chained AnimType. |
| `Translucent` / `Translucency` | `+0x36A` / `+0x2EC` | no / 0 | Parsed; the draw-flag mapping was not traced (YRpp BlitterFlags: TransLucent25 = 2, 50 = 4, 75 = 6). **Low**. |
| `StartSound`, fallback `Report` | `+0x2F8` | -1 | `Report=` is only read when `StartSound` is absent. |
| `StopSound` | `+0x2FC` | -1 | Played when the anim is removed. |
| `RandomLoopDelay=a,b` | `+0x2DC/+0x2E0` | | Random pause before each repeat. |
| `PingPong`, `Reverse` | `+0x370`, `+0x371` | | Direction flip, or start from LoopEnd. |

**Ticking** (AI):

```c
if (timer_expired && delay) { frame += step /*±1*/; restart timer(delay); }
end = (loopsLeft >= 2) ? LoopEnd - Start : End;      // every pass but the last stops at LoopEnd
if (frame >= end) {
   if (loopsLeft != 0 && loopsLeft != 255) loopsLeft--;
   if (loopsLeft) { frame = LoopStart - Start; pause = Random(RandomLoopDelay); }   // loop
   else if (type->Next) { type = Next; loopsLeft = Next->LoopCount; delay = Next->Rate(/RandomRate);
                          frame = Next->Start; Start(); }                           // chain
   else  expire (MakeInfantry / ExpireAnim / remove)
}
```

- **LoopCount 0 and 1 both play once.** LoopCount N plays N−1 passes over `[LoopStart, LoopEnd)` and then one
  final pass to `End`.
- **Sound.** `Start()` (`FUN_00424c60`) plays `StartSound`/`Report` once when the anim actually begins, after
  the ctor `delay`. AI then keeps the sound's position updated (`FUN_0074fb60`). **Medium-High**.

### Warhead impact anim: `FUN_0048a400` (MapClass::SelectDamageAnimation(damage, WH, landType, coord))

```c
if (land == Water && WH->Conventional(+0x14d) && !bridgeCell && coord.Z < cellHeight + 2*LevelHeight)
    return SplashList[min(damage, n*35-1) / 35];          // Rules+0xbc4, n = Rules+0xbd0; 35-damage buckets
if (WH == Rules+0x17b4 /*special WH*/) return Rules+0x2f4;  // single fixed anim (likely C4/Ivan)
n = WH->AnimList.Count(+0x114);
if (n) {
    if (WH->EMEffect(+0x154)) return AnimList[Random(0, n-1)];
    return AnimList[min(damage, n*25-1) / 25];             // **25-damage buckets**: 0-24 -> [0], 25-49 -> [1] ...
}
return NULL;
```

**High**. Example: `AnimList=A,B,C` with damage 60 gives `C` (60/25 = 2). Damage ≥ 75 is clamped to the last
entry.

### Unit death: `FUN_007376c0` (UnitClass explode)

Called from UnitClass::ReceiveDamage `FUN_00736d60` on destruction.

```c
if (Type->Explosion.Count(+0x73c)) {
   anim = Explosion[Random() % count];                     // uniform random
   if ((Type->(+0xd15) || FUN_0070c550(10)) && (Type->(+0x684) == -1 || this->(+0x2fc) > 0))
       anim = Explosion[count-1];                          // special case: last entry (condition not decoded)
   new AnimClass(anim, Coord, 0, 1, 0x600, 0, 0);
   ... optional ore spill (Rules+0x17e5), screen shake if Strength > Rules+0x5c8
}
if (Type->(+0x758)) new AnimClass(List2[Random() % n] ...);  // second random list at +0x74c (debris-style)
```

`Explosion=` is **picked at random**. **High** for the random pick. **Low** for the "last entry" override
condition. `DeathWeapon=` (TechnoType `+0xd18`) is parsed by `FUN_00711510`, but where it is fired was not
traced.

## 6. Building animations

The art reader is `FUN_0045fe40` (BuildingTypeClass art section). Building anims live in **21 slots of
0x44 bytes** at `type+0xF4C…`. For each slot the reader takes:

- a normal name (`+0xF4C + slot*0x44`), a damaged name (`+0xF5C`), and a garrisoned name;
- `X, Y` at `+0xF7C/+0xF80`, `ZAdjust` at `+0xF84`, and `YSort` at `+0xF88`;
- the `Powered`, `PoweredLight`, `PoweredEffect` and `PoweredSpecial` flags.

Slot 3 is `ActiveAnim` (`ActiveAnimX` @ `+0x1048`). The `ActiveAnimTwo/Three/Four`, `SuperAnim*`,
`IdleAnim`, `ProductionAnim` and related keys follow the same pattern, one slot each.

**Play** (`FUN_00451880` BuildingClass::PlayAnim(name, slot, damaged, delay)):

```c
if (damaged != this->AnimDamagedState(+0x6e6)) { state = damaged; re-create every live slot anim (FUN_00451740); }
pos  = PixelToCoord(slot.X, slot.Y) + this->RenderCoord();
anim = new AnimClass(AnimTypes[find(name)], pos, delay, /*loops*/1, /*flags*/0x1600, 0, 0);
anim->ZAdjust(+0x100) = slot.ZAdjust;  anim->YSort(+0x104) = slot.YSort;  anim->OwnedByBuilding(+0x118) = 1;
```

- The **frame rate source is the anim's own artmd `[Anim]` section**: `Rate`, `LoopStart/End`, `LoopCount`
  and so on, exactly as in section 5. The building does not drive the frames.
- The **Y-sort offset is `ActiveAnim*YSort`**, added to the anim's sort position. `ZAdjust` is the depth
  bias. `X/Y` are pixel offsets from the building's render coordinate.
- The **Damaged variants** are chosen by `BuildingClass::ReceiveDamage` (`FUN_004421b0` ~L284). When the
  health ratio crosses the yellow threshold it calls `PlayAnim(damaged ? slot.DamagedName : slot.Name, …)`
  for each slot that has an anim.
- **High** for the slot layout and PlayAnim. **Medium** for the threshold used, which was read only as a
  ratio comparison.
- `BibShape` is read in `FUN_0045f220` (art). Its draw path was **not traced**. **Low**.
- **DamageFireTypes** is a `[General]` rules list of fire AnimTypes (FIRE01..), parsed in `FUN_0066cf70`.
  **DamageFireOffset0..7** is read per building art (`FUN_0045fe40` ~L210) as 8 `x,y` pixel pairs into
  `type+0x15D8`. `0xFFFF,0xFFFF` is stored as `0,0`; the defaults come from `DAT_0089b840`, set by
  `FUN_0045dd80`. The routine that spawns fires at these offsets was **not located**: no direct
  displacement reference outside parse/init was found. **Low**. Expected behaviour, which is not verified:
  a random `DamageFireTypes` anim at each offset when the building becomes damaged.


---

## 8. Wave 3 (W3-B) additions: anim clock, AnimClass draw keys, building slots

**Clock.** AnimClass AI, building slot anims and terrain anims step once per
LOGIC frame; the clone advances `g_pres_tick` in `Presentation::logic_tick`,
called inside GameScene's fixed 15 Hz loop (was once per render frame: ~4x too
fast at 60 Hz). Unit fire/death effects, projectile impacts, defence muzzles and
building deaths are spawned on the tick of their event.

**Draw keys** (`FUN_00422cc0`, AnimType INI `FUN_00427c80`, HIGH):
- `Translucency=25/50/75` -> BlitterFlags 2/4/6; `Translucent=yes` instead FADES
  by life: stage > End*0.6 -> 6, > 0.4 -> 4, > 0.2 -> 2 (constants 0x7E2558..48).
- `Shadow=yes` draws the frame + SHP/2 with the Darken blitter.
- `Flat` (+0x369), `Layer` (+0x364): Flat / underground / surface draw on the
  ground under objects [RE-FALLBACK: Ground-layer anims are not depth-sorted].
- `AltPalette` (+0x361): the anim's own convert (+0xD4, set from the owner
  house's ColorScheme where the spawner passes one), else `ColorSchemes[0]`
  (+0x30C) = `[Colors]` first entry LightGold — RE-Q3 answered.
- `IsVeins` (+0x355) uses the player's scheme; `UseNormalLight` +0x35D.

**Building slots** (`PlayAnim FUN_00451880`, callers listed by
`callctx.py 451880`): slot = name/Damaged/Garrisoned at type+0xF4C+slot*0x44.
- UpdateAnimations `FUN_004509c0`: a `Refinery=yes` type shows ONE of
  Active..ActiveFour by storage level `stored*4/Storage` (0,1,2,>=3) — RE-Q4
  answered; `UnitRepair` returns to SpecialThree when not repairing.
- Slot-finished callback `FUN_00451b30`: Special (depot) -> SpecialTwo while a
  unit is serviced, else Idle; SpecialThree -> Idle; SuperTwo -> SuperThree,
  SuperFour -> Super; IsAnimDelayedFire Special -> Active.
- Production (8): the construction yard on a placement (`FUN_0044d880`,
  `FUN_0043c250` case 0xC kill PreProduction+Idle then play Production).
- Shape_Number `FUN_0043ef10`: CanBeOccupied = (occupants ? 2 : 0) +
  (ratio <= ConditionRed, or <= ConditionYellow for garrisonable types ? 1 : 0);
  Gate = GateStages stage, +GateStages+1 damaged.
- `LeaveRubble=yes` (type+0x1768) -> `GetRubbleImage FUN_0045f150`: the main SHP's
  frame 3 (shadow frames/2+3) when the image has > 3 frames.

**Theater tile anims** (RE-Q6): a tile set's `SetName` section of the theater
INI lists `Tile<NN>Anim/XOffset/YOffset/AttachesTo/ZAdjust`; the anim stands on
the template's AttachesTo sub-tile (temperat.ini `[Waterfalls]` WA01X..WD04X, the
tunnel tops) [MEDIUM: the pixel origin of the offset was fitted to the art].
