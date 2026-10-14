# RE-T1 — Logic frame rate per GameSpeed, and the GameSpeedBias movement multiplier

Binary: `gamemd.exe` (YR 1.001), Ghidra headless on a private copy of the
project (`D:\ra2-re\fixC`, scripts `D:\ra2-re\scripts\fixC_1..11.py`, dumps
`D:\ra2-re\fixCout\`). Tags as elsewhere: **[CONFIRMED]** read from code,
**[ANCHORED]** derived from confirmed code + data, **[RE-FALLBACK]** assumed.

## §1 Globals

| Global | Meaning | Evidence |
|---|---|---|
| `0x00A8DAC8` | `OptionsClass` instance, **`GameSpeed` at +0x00** (int, 0..6, 0 = fastest) | `FUN_005f9e10` (RA2.INI `[Options] GameSpeed` load) is called with `MOV ECX,0xa8dac8` @`0x52c397`; save `FUN_005fa500` likewise; options sliders show `6 - GameSpeed` (`FUN_004e1ef0`, `FUN_005b7d20`) [CONFIRMED] |
| `0x00A8A1A0` | session GameMode (0 = campaign, 5 = skirmish; others = network) | branch structure below [ANCHORED: YRpp `SessionClass::GameMode`] |
| `0x00A8A4C0` | negotiated network frame rate (fps) | written by the network packet parser `FUN_004c6bd0` (the 16-bit value at packet offset 7) and `FUN_0052d6e0` (`=0x1e`), printed as `"Req fps: %d"` (`FUN_0055ee10`) [CONFIRMED] |
| `0x008862D0` | the frame limiter timer's **rate in 1/60 s ticks per logic frame** | set in `FUN_0055cff0` (below); `rate = 60/fps` on the network path [CONFIRMED] |

## §2 The limiter (`FUN_0055cff0`, main-loop frame pacing) [CONFIRMED]

```
saved_speed = GameSpeed                            // global 0xA8DAC8, kept in a local
if GameMode == 0 (campaign):
    if global 0xA8DD44 == 0: GameSpeed = 2; rate = 2   // forced 30 fps
    else: rate = GameSpeed
elif GameMode == 5 (skirmish) or global 0xA8A1B4 != 2:
    rate = GameSpeed                               // code at 0x55D42E
else:                                              // network
    rate = 2 if fps == 0 else 60 / fps             // fps = global 0xA8A4C0
    frame_ms = 1000 / fps
```

So in **skirmish (and campaign when the speed is player-selectable)** one logic
frame lasts **`GameSpeed` ticks of a 60 Hz timer**:

| `GameSpeed` (INI, 0 = fastest) | UI slider (`6 - x`) | ticks/frame | logic fps |
|---|---|---|---|
| 0 | 6 "Fastest" | 0 | **unlimited** (timer rate 0 = never waits) |
| 1 (shipped `[MultiplayerDialogSettings] GameSpeed=1`) | 5 | 1 | **60** |
| 2 | 4 | 2 | 30 |
| 3 (OptionsClass ctor default) | 3 | 3 | 20 |
| 4 | 2 | 4 | **15** |
| 5 | 1 | 5 | 12 |
| 6 | 0 "Slowest" | 6 | 10 |

The network path (`FUN_00647090`, "Generate Real Timing Event") derives the
requested fps from the same option with one special case:
`GameSpeed 0 -> 60, 1 -> 45, n -> 60/n` (computed as `0x3C / GameSpeed`,
with special cases `0 -> 0x3c`, `1 -> 0x2d`), then caps it by the slowest peer's reported rate.
[CONFIRMED]

Campaign: `DAT_00a8dd44` (set to 1 only by `FUN_0052f360`) gates whether the
campaign honours the player's speed; when it is 0 the campaign is pinned to
`GameSpeed = 2` = **30 fps**. The identity of the flag's setter (which
scenario/menu path) was not unwound — [RE-FALLBACK: treat campaign as
player-selectable like skirmish; the clone has no network play].

### Consequence for the clone

The original is **frame-locked**: every rule quantity (ROF, Speed leptons,
timers, `*900` minute conversions) is per logic frame; GameSpeed changes only
wall-clock pacing. The clone's established convention "1 engine second = 15
logic frames" (`kLogicFps = 15`, ROF/15, `RechargeTime*900/15`, ...) is exactly
`GameSpeed = 4`. A speed setting therefore maps to a **frames-per-wall-second
multiplier** of `fps/15` (GameSpeed 1 = 4x, 2 = 2x, 3 = 1.333x, 4 = 1x, 5 = 0.8x,
6 = 0.667x; 0 = as fast as the host runs). Per-frame constants must be
converted at **15 frames per engine second**, never at a "feel" scale.

## §2a Network games (W9 recovery, `D:/ra2-re/w9fix-speed-out/q3..q6`) [CONFIRMED]

* **Rate.** The network branch above runs one frame per `1000 / DesiredFrameRate` ms
  (`0xA8A4C0`; 0 -> 33 ms). `FUN_0052D6E0 @0x52D7FD` initialises it to **30** at scenario start; the
  master's TIMING event (`FUN_004C6BD0 @0x4C7F9D`) replaces it with
  `min(1000 / slowest PROCESS_TIME, cap)`, cap = GameSpeed 0 -> 60, 1 -> 45, n -> 60/n.
  Lobby default speed 1 therefore plays at 45 fps once the first TIMING (frame 256) executed.
* **No catch-up.** One logic frame per main-loop pass when the timer expired; time lost in
  Wait_For_Players is not made up.
* **Internet lag throttle** (`0x55D1C8..0x55D2F0`, GameMode 4, MaxAhead `0xA8A4B8` > 30): +10 ms per
  frame for each of MaxAhead/4, /2, 3/4 that the most-behind peer (`0xA8CAE4[]`) lags.
* **PROCESS_TIME** = ProcessTicks / ProcessFrames, accumulated from the loop start `0x55D04E` to just
  before Queue_AI `0x55DA5B` -- the frame's own work, not the limiter wait or the network wait.
* **In-game speed slider.** The Options dialog `FUN_004E1EF0` shows slider `0x529` (`6 - GameSpeed`)
  in network games too (hidden only in a campaign without `0xA8DD44`, or with `0xA8A4A0` set). Its OK
  handler `FUN_004E1CF0` does NOT write GameSpeed in a network game (GameMode != 0 and != 5): it
  queues a **GAMESPEED event (0x0D)** carrying the new value; `EventClass::Execute FUN_004C6BD0
  @0x4C786E` stores it in `0xA8DAC8` on every peer at the same frame, and the master's next TIMING
  uses it as the cap. Any player may change it.

Clone (W9-01-01/03/05/07): `GameScene::net_tick_fps` (game_scene_net.inc), one tick per rendered
frame in a match, `LockstepSession::note_process_time`, the GAMESPEED command from the Options
slider.

## §3 `GameSpeedBias` — the ground/air speed multiplier [CONFIRMED data flow]

`[General] GameSpeedBias` (`; multiplier to overall game object movement
speed`, value **1.6** on this install) is read by the Rules `[General]` loader
`FUN_0066cf70` into **`Rules+0x1418`** (double). Its only reader is
`FUN_004f6dd0` = `HouseClass::AssignHandicap(difficulty)`:

```
house+0x188 = Diff[d].Firepower                       // Rules+0x1538+d*0x50
house+0x190 = Diff[d].Groundspeed * GameSpeedBias     // +0x1540
house+0x198 = Diff[d].Airspeed    * GameSpeedBias     // +0x1548
house+0x1A0 = Diff[d].Armor, +0x1A8 ROF, +0x1B0 Cost
house+0x1B8 = Diff[d].BuildTime   * GameSpeedBias     // +0x1568
(network modes additionally multiply by the country's SpeedGroundMult etc.,
 HouseType+0xC8..0xF8)
```

`[Easy]/[Normal]/[Difficult] Groundspeed=1.0` on this install, so every
house's ground (and air) speed bias would be **1.6** -- but **nothing reads
it** (W7 correction, see §4): a whole-exe operand scan finds only the two
`FSTP` writes in `FUN_004F6DD0` and no read of `house+0x190` [CONFIRMED-RE by
absence; `D:/ra2-re/w7-loco-out/ds3.txt`]. The earlier wording here
("applied on the strength of the field it lands in") was [ANCHORED] and is
**refuted**.

Note for the production owner: `house+0x1B8` = `BuildTime * GameSpeedBias`
is the same product for the build-time bias.

**Production check (gap owner A, fixA2 Ghidra pass, `scripts/fixA2_1.py`).**
`house+0x1B8` has **no consumer in production**: an instruction scan of
every function for an x87 load/multiply/divide of `[reg + 0x1b8]` finds
**zero** hits, and `Time_To_Build` (`FUN_006F3D00`) multiplies only
`FUN_0050BEC0` (= the house's HouseType+0x134..0x144 country
`BuildTime<class>Mult` floats -- not +0x1B8), the type multiplier, the
power factor (`FUN_004FCC50`) and MultipleFactory. So GameSpeedBias does
**not** change build time; the clone's `trunc(Cost x BuildSpeed x 0.9)`
chain (construction.md §7) stands. [CONFIRMED by absence of a reader]

## §4 Movement constant (fixes G-42; corrected by W7-11-01)

**Speed in leptons.** `type+0x678 = min(255, clamp(Speed,0,100)*256/100)`
leptons/frame (`voxel-render.md §5`, CONFIRMED).

**GetCurrentSpeed** (`FootClass` `FUN_004DB0B0`, Unit vt+0x538) [CONFIRMED-RE]:

```
s = ftol( type+0x678 * countryMult * Foot+0x580 SpeedMultiplier (crate x1.2) )
if FASTER (veteran): s = ftol( s * VeteranSpeed 1.2 )
s = ftol( s * Foot+0x578 SpeedPercentage )        // the locomotor's 0..1 fraction
if carrying a CTF flag: s /= 2
```

`countryMult` (`FUN_0050BE70`) is the **country's** SpeedUnitsMult /
SpeedInfantryMult / SpeedAircraftMult (HouseType +0x12C/+0x128/+0x130, all 1.0
on this install) -- **not** `house+0x190`. **There is no GameSpeedBias in the
speed** (§3). InfantryClass overrides it (`FUN_00521AB0`): a prone
(`+0x6DB`) soldier with art `Crawls=yes` moves `s - trunc(s/3)` (GI 10 -> 7).

**How far a unit gets per frame depends on its locomotor:**

| Locomotor | Movement per frame | Source |
|---|---|---|
| Walk (infantry) | `s` raw leptons along the facing | `FUN_00759CE0` |
| Hover (ROBO, LCRF, SAPC) | `ftol(s * cur)` raw leptons, `cur` ramped by HoverAcceleration / HoverBrake | `FUN_00514130` / `FUN_00515CF0` |
| Drive (vehicles), Ship | one **track point per 7 speed units**; the points are ~11.1 leptons apart (23 per straight cell = 161 units, 31 per diagonal = 217 units) | `FUN_004B0E50`, RawTrack `0x7E6A28` (Ship `0x7F1960`, identical data) |

So vehicles and ships cover about **x1.59** (straight) / **x1.67** (diagonal)
their Speed leptons -- numerically the old "x1.6", but through the track
geometry, and only for the track locomotors. Infantry and hovers move their
raw leptons.

```
cells/engine-second (Walk, Hover cruise) = trunc(Speed*256/100) * 15 / 256
cells/engine-second (Drive/Ship, straight) = trunc(Speed*256/100) * 15 / 161
```

| Unit | Speed | lept/frame | cells/engine-s | before W7 (x1.6 for all) |
|---|---|---|---|---|
| E1 GI (Walk) | 4 | 10 | 0.586 | 0.9375 |
| DOG (Walk) | 8 | 20 | 1.17 | 1.875 |
| ROBO (Hover) | 10 | 25 | 1.46 (cruise) | 2.344 |
| MTNK Grizzly (Drive) | 7 | 17 | 1.584 straight, 1.666 diagonal | 1.594 |
| HTNK Rhino (Drive) | 6 | 15 | 1.398 straight | 1.406 |
| FV IFV (Drive) | 10 | 25 | 2.329 straight | 2.344 |

The clone (`engine/main.cpp` `speed_to_cps`, `engine/world_loco.cpp`) stores
the raw `Speed` leptons (`cells/s = lept * 15/256`) for every unit; the track
model in `world_loco.cpp` produces the Drive/Ship geometry. Drive/Ship also
apply the SpeedPercentage terms: the GroundType/slope fraction, x0.75 at
<= ConditionYellow (`FUN_004B2550`), and the Accelerates ramp
(`AccelerationFactor` up, `Speed x DeaccelerationFactor` down, a 0.3 floor
within `SlowdownDistance` of the goal; `FUN_004B0E50`).

That is the ~3.2x slow-down relative to the clone's former 0.75 scale.
