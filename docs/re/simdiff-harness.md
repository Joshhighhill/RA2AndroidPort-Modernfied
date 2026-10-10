# simdiff: a differential harness against the original `gamemd.exe`

**Purpose.** Online play with original-game (and CnCNet) players needs the clone to simulate
exactly like `gamemd.exe`, bit for bit and frame by frame (`docs/gap/net/03-sim-bitexact.md`,
`docs/gap/net/01-original-protocol.md`). That is a long program. This harness measures it: it
runs the **original** game and the **clone** on the same fixed-seed scenario, dumps the same
per-frame state record from both, and reports the first divergent frame and field, plus a score
ladder.

Agent H, branch `net-h`. Everything RE-side (Ghidra project copy, game copy, run outputs) lives
outside the repo: `C:\ra2-h\` and `D:\ra2-re\h\`. See §9 for why some of it is on `C:`.

**What is in the repo:**

| Path | What |
|---|---|
| `tools/simdiff/hook/simhook.c` | 32-bit DLL injected into a **copy** of `gamemd.exe`; dumps one record per logic frame |
| `tools/simdiff/hook/simlaunch.c` | 32-bit launcher: starts the copy suspended, injects the DLL, resumes, waits |
| `tools/simdiff/hook/build.ps1` | builds both with the local MSVC x86 compiler |
| `tools/simdiff/simdiff.py` | differ, matcher, markers, build order, ladder runner |
| `tools/simdiff/l0_clone.cpp` | L0 rung, clone side (CMake target `simdiff_l0`, `EXCLUDE_FROM_ALL`) |
| `tools/simdiff/run_ladder.ps1` | end-to-end driver: game copy, hook build, original and clone runs, ladder |
| `engine/simdiff_dump.inc` + `engine/ai_soak.inc` | the clone's `--ai-soak --diff-dump` switch |

---

## 0. Results in one screen

- **The binary is retail Yuri's Revenge 1.000, not 1.001** (§1). Proof: the PE timestamp is
  `0x3B846665` and the size is `0x496110`. Those are exactly Ares's `YR_TIME_1000` and
  `YR_SIZE_1000`. The game's own crash log also says "Internal Version 1.000". Every address in
  `docs/re/*.md` is for 1.000. CnCNet and YRpp use 1.001 (timestamp `0x3BDF544E`).
- **No toolchain was installed.** The VS 2022 Community/BuildTools already on the machine ship
  `Hostx64\x86\cl.exe` and the x86 SDK libraries (§2).
- **The hook works on the original** (§4). A fixed-seed AI-vs-idle-human skirmish on XMP29U2
  starts with no input. The DLL clicks through the shell. About 3000 frames take 75 s.
- **The original is deterministic run to run.** CRC, Scenario RNG and every object field are
  identical across separate runs: 600, 1200 and 3000 frames, with and without the RNG trace.
  One exception: the global "twin" RNG `0x885B08` diverges by frame 12–42. The audio code draws
  from it (§7.2). It is not sync state.
- **The clone side is in place.** `--diff-dump` is read-only: soak output is identical with and
  without it over 2700 frames. The clone is also deterministic run to run.
- **Baseline: L0 passes. Everything else diverges at frame 0** (§6), as N3 predicted. The harness
  gives the reasons in order: missing civilian objects, start-force composition, instant MCV
  deploy, a half-cell lepton origin, facings, and no sync RNG stream. Even so, 42 of 63 paired
  objects sit at the exact original position. The AI's build order is the same sequence, but
  late by 107–228 frames.

---

## 1. Which `gamemd.exe` is this?

| Evidence | Value in `D:\Red Alert 2\gamemd.exe` | Reference | Verdict |
|---|---|---|---|
| PE `TimeDateStamp` | `0x3B846665` (2001-08-23 02:11:49 UTC) | Ares `Ares.cpp`: `YR_TIME_1000 = 0x3B846665`, `YR_TIME_1001 = 0x3BDF544E` | **1.000** |
| File size | 4,808,976 = `0x496110` | Ares: `YR_SIZE_1000 = 0x496110`, `YR_SIZE_1001 = 0x497110` | **1.000** |
| Game's own version string | `except.txt` (written by this exe in 2021): "Internal Version 1.000 / Release Build: 104 by gsmallwoo - 08/22/2001 - 19:12:30" | | **1.000** |
| VQA lib build string | "Aug 22 2001 19:11:41" | matches the build date | |
| VERSIONINFO | FileVersion/ProductVersion "1.05", InternalName "Sun" | the marketing resource is not the patch level | — |
| CRC32 of the file | `0x620E1D9A` | Ares `YR_CRC_1000 = 0xB701D792` | **modified 1.000** (likely a no-CD crack; the install has crack artefacts). Section layout is plain (4 sections, no SafeDisc) |
| Global-data shift vs YRpp (1.001) | `.bss` −0x1098, Logic/layers/global RNG −0x1080/−0x1090 | | different link → different build |

**Conclusion: retail 1.000 with a byte-modified (cracked) image.** N3's "pre-1.001, most likely
1.000" was right. N1's "1.001 with shifted code" was wrong. The shifts are simply the
1.000→1.001 relink.

Consequences:
- Every 1.000 address in `docs/re/*.md` and `engine/` is valid for this file only.
- A CnCNet-compatible target needs the 1.001 executable plus the spawner. Every address would
  have to be re-pinned using the table below as a start.
- 1.001 also changed behaviour: it fixed gameplay bugs. A clone that matched this 1.000 bit for
  bit would still desync against 1.001 wherever the patch changed the logic.

### 1.1 Address translation, 1.000 (this file) → 1.001 (YRpp / spawner)

Pairs marked **V** were verified here: same role in both, by xref, decompile or a live dump.
Pairs marked *N1* come from `01-original-protocol.md`. Code shifts are not uniform, so do not
interpolate far from an anchor.

**Globals**

| Object | 1.000 | 1.001 | Δ | |
|---|---|---|---|---|
| `Frame` (CurrentFrame) | `0xA8DCEC` | `0xA8ED84` | −0x1098 | V |
| `Seed` | `0xA8DCFC` | `0xA8ED94` | −0x1098 | V |
| preset seed (read only by Init_Random) | `0xA8DD00` | `0xA8ED98` (inferred) | −0x1098 | V (1.000 side) |
| `SessionClass` (GameMode at +0) | `0xA8A1A0` | `0xA8B238` | −0x1098 | V |
| `ScenarioClass*` (+0x218 = `Random`) | `0xA8A198` | `0xA8B230` | −0x1098 | V |
| `ScenarioName[0x200]` | `0xA8A848` | `0xA8B8E0` | −0x1098 | V (dump header reads "XMP29U2.MAP") |
| `HouseClass::Array` | `0xA7F190` | `0xA80228` | −0x1098 | V |
| `InfantryClass::Array` | `0xA82D50` | `0xA83DE8` | −0x1098 | V |
| `UnitClass::Array` | `0x8B3070` | `0x8B4108` | −0x1098 | V |
| `AircraftClass::Array` | `0xA8D2F8` | `0xA8E390` | −0x1098 | V |
| `BuildingClass::Array` | `0xA8DAA8` | `0xA8EB40` | −0x1098 | V |
| `LogicClass::Instance` | `0x87E6F8` | `0x87F778` | −0x1080 | V |
| `ObjectsInLayers[5]` | `0x89F2D0` | `0x8A0360` | −0x1090 | V |
| global RNG (`Randomizer::Global`, the twin) | `0x885B08` | `0x886B88` | −0x1080 | V |
| `EventClass::OutList` | `0xA7F230` | `0xA802C8` | −0x1098 | *N1* |
| `CurrentFrameCRC` / `LatestFramesCRC[256]` | `0xAC4164` / `0xB033DC` | — | | V (1.000 side) |

**Functions**

| Function | 1.000 | 1.001 | Δ | |
|---|---|---|---|---|
| Main loop | `0x55CFF0` | `0x55D360` | −0x370 | V |
| `LogicClass::AI` call in the main loop | `0x55D92E` → `0x55AC40` | Phobos hooks inside it at `0x55AFB3` | | V |
| `Queue_AI` call in the main loop | `0x55DA6D` → `0x646D00` | — | | V |
| `Init_Random` | `0x52F960` | `0x52FC20` | −0x2C0 | V |
| `RandomClass` seed / next / ranged | `0x65C110` / `0x65C1C0` / `0x65C220` | `0x65C6D0` / `0x65C780` / `0x65C7E0` | −0x5C0 | V |
| `ComputeFrameCRC` | `0x64D520` | `0x64DAB0` | −0x590 | V |
| `Print_CRCs` | `0x64D8E0` | `0x64DEA0` | −0x5C0 | *N1* |
| `FacingClass::Current` | `0x4C92F0` | `0x4C93D0` | −0xE0 | V |
| CopyProtect IsLauncherRunning / NotifyLauncher / Validate | `0x49F4D0` / `0x49F530` / `0x49F6B0` | `0x49F5C0` / `0x49F620` / `0x49F7A0` | −0xF0 | V |
| `AnimClass::AI` | `0x423AE0` | `0x424CB0` | −0x11D0 | N3 |

**Object fields.** These are identical in both builds, confirmed live by the dump: health values
equal type Strength, owners are correct, and so on.
- Abstract `UniqueID +0x10`
- Object `Health +0x6C`, `Location +0x9C/A0/A4`
- Techno `Owner +0x21C`, `PrimaryFacing +0x388`, `SecondaryFacing (turret) +0x3A0`
- Type pointer: Infantry `+0x6C0`, Unit/Aircraft `+0x6C4`, Building `+0x520`
- Type ID string at type `+0x24`
- House `ArrayIndex +0x30`, `IsHumanPlayer +0x1EC`, `IsInPlayerControl +0x1ED`
- vtable slots: `+0x2C` WhatAmI, `+0x3C` owning house, `+0x88` GetType, `+0x184` current mission
- `RandomClass` (1012 bytes): `+0` disabled flag, `+4` idx1, `+8` idx2, `+0xC` table[250]. Seeding
  sets idx2 = 103; both indices wrap at 250.

---

## 2. 32-bit toolchain

**Nothing was installed and the system was not changed.** VS 2022 Community (and BuildTools)
on this machine already include:
- MSVC 14.44 `bin\Hostx64\x86\cl.exe`
- `lib\x86`
- Windows SDK 10.0.26100 `um\x86` and `ucrt\x86`

`tools/simdiff/hook/build.ps1` calls `vcvarsall.bat x64_x86` and builds with `/MT`, so there is
no runtime dependency. A portable i686 MinGW under `D:\ra2-deps` was not needed (D: is also
full, §9). The DLL uses MSVC-only features: `__try`, `__declspec(naked)` and inline asm.

---

## 3. The record format (simdiff v1)

Both producers write the same text format, one file per run:

```
# simdiff v1 src=<gamemd-1.000|clone> seed=<hex> session=5 scenario=<map> every=<n>
F <frame> crc <hex> rng <idx1> <idx2> <fnv32(table)> rng2 <idx1> <idx2> <fnv32> vec <inf> <unit> <air> <bld> logic <n> layers <l0..l4> cen <rtti>:<n> ...
O <rtti> <uid> <typeID> <owner> <x> <y> <z> <facing16> <turret16|-1> <hp> <mission>
```

**Header line (`F`)**
- **`frame`:** the logic frame just simulated. Original: dumped after `LogicClass::AI` and
  `Queue_AI`, before `++Frame`. Clone: dumped after `GameScene::update`, so frame 0 is the state
  after the first logic step.
- **`crc`:** what `ComputeFrameCRC` `0x64D520` returns at that point. That is the value a
  multiplayer peer would exchange. In a skirmish the game never calls it, so the hook calls it
  and then restores both RNGs, `CurrentFrameCRC` and the logger sink. The skirmish's random
  stream is therefore untouched (verified: hooked runs are identical to each other and to
  traced runs).
- **`rng`:** the sync stream `Scenario+0x218`. **`rng2`:** the twin `0x885B08`.
- **`vec`:** counts of the four typed vectors. **`logic`:** the Logic vector count.
  **`layers`:** the five map layers. **`cen`:** census of Logic by RTTI (`1` Unit, `4` Anim,
  `6` Building, `15` Infantry, `36` Terrain, …).

**Object lines (`O`)**
- One per Unit, Aircraft, Building and Infantry, **in Logic-vector order**.
- `x y z` are `ObjectClass::Location` in leptons. For a building this is the **centre of the
  foundation's origin cell** (measured).
- `facing16` is `FacingClass::Current()` of the primary facing, as a raw 16-bit DirStruct.
- `mission` is the vtable `+0x184` result (5 Guard, 11 Area Guard, …).
- A field the producer cannot supply is written as `-`. The differ reports it as "n/a", never
  as a match.

**The clone currently writes `-` for:**
- `crc`: there are no layers, Logic vector or sync RNG to fold;
- `uid`, `mission`, `rng2`, `logic`, `layers`, `cen`.

**What the clone writes instead:**
- `rng` is its crate `GameRandom`, the only R250 instance it has.
- Infantry facing is the 8-way index × 0x2000.
- Unit position is the clone's own `unit_lept_x/y/z`.
- Building position is `cx*256+128, cy*256+128`.

---

## 4. The original side: how a run is driven

**The game copy.** `D:\ra2-re\h\game` is never the install itself.
- Small files are copied. Big MIX and data files are **hard links**: no space used, read-only
  use.
- `RA2MD.INI` in the copy holds the skirmish defaults the shell starts with:
  - `[Skirmish] ScenIndex=0` (resolves to **XMP29U2.MAP** on this install), `GameSpeed=0`,
    `ShortGame=no`, `UnitCount=10`, `Credits=10000`;
  - `Slot01=4,8,1` is one AI (Russians). The other slots are `1,-2,-2`;
  - `[MultiPlayer] Side=YuriCountry`;
  - `[Video]` is 800×600.
- `RA2MD.INI` is parsed by `FUN_00697920`; each `Slot%02d` holds three ints.

**Launch.** `simlaunch.exe <copy-dir> gamemd.exe simhook.dll [timeout-s]`
- It calls `CreateProcess(CREATE_SUSPENDED)`, then a remote `LoadLibraryA`, then `ResumeThread`.
- Configuration travels in the environment:

| Variable | Meaning (default) |
|---|---|
| `SIMHOOK_OUT` | dump path (`simdump.txt`). Also writes `.log`, `.l0`, `.rngtrace` |
| `SIMHOOK_SEED` | session seed (`0x2A2A2A2A`). 0 is not allowed (it means "not preset") |
| `SIMHOOK_FRAMES` | terminate after this frame (3000) |
| `SIMHOOK_EVERY` | dump every n-th frame (1) |
| `SIMHOOK_AUTOSTART` | click through the shell (1) |
| `SIMHOOK_L0` | write n raw + n ranged draws of a freshly seeded `RandomClass` (0) |
| `SIMHOOK_PASSIVE` | house bitmask made idle before frame 0's logic (0) |
| `SIMHOOK_RNGTRACE` | log the caller of every sync/twin RNG draw for frames < n (0) |

**What the DLL does in memory** (nothing is written to disk):
1. **Checks** the PE timestamp (`0x3B846665`) and the exact bytes at both call sites. If either
   is wrong it stays inert.
2. **Seed.** Writes `SIMHOOK_SEED` to `0xA8DD00`. `Init_Random` (`0x52F960`) reads that preset
   only in the single-player branch: `if (DAT_00a8dd00 != 0) Seed = DAT_00a8dd00; else
   GetTickCount()`. Nothing else in the game writes it. So the skirmish is seeded
   deterministically without touching the session path. This is the in-engine equivalent of the
   spawner's `Seed=`.
3. **Launcher.** Patches `IsLauncherRunning` and `NotifyLauncher` to `mov al,1; ret`, so the
   copy runs without `YURI.exe`. This is the same as the spawner's CopyProtection patches.
   `Validate` already returns 1.
4. **Frame hook.** Redirects `CALL Queue_AI` at `0x55DA6D` to a stub (Queue_AI, then dump).
   Redirects `CALL LogicClass::AI` at `0x55D92E` to a naked stub that preserves `ECX` and runs
   the pre-logic scenario shaping (passive houses).
5. **Shell.** A thread posts `BN_CLICKED` to the shell dialogs, found by control ID:
   dialog 226 → 1667 *Single Player*, dialog 256 → 1401 *Skirmish*, dialog 258 → 1559
   *Start Game*. This is the original's own skirmish start path, end to end, with no input
   translation.
6. **RNG trace** (optional). Replaces `RandomClass::next` (`0x65C1C0`) and `::ranged`
   (`0x65C220`) with exact transcriptions of their decompiled bodies. `ranged` inlines the
   R250 step, so it must be hooked too. The transcriptions also log the caller's return address.
   Traced dumps are identical to untraced ones.

**Cost.** About 3000 frames per 75 s wall, 1 frame ≈ 7 KB of text. The window appears on the
desktop (ts-ddraw, windowed).

**Known flake.** Two of about 17 starts failed before the main menu: one hang and one crash with
`0xC000041D` (fatal user-callback exception). Both happened in an RDP session. `run_ladder.ps1`
retries up to 3 times. `simlaunch` kills only **its own** child PID at its timeout.

---

## 5. The clone side

`ra2engine --ai-soak` already drives a real `GameScene` headless at dt = 1/15 with a passive
human. It gained these switches:

| Switch | Effect |
|---|---|
| `--diff-dump <file>` | write simdiff v1 after every logic frame (read-only; §3) |
| `--frames N` | run N frames instead of `--minutes` |
| `--no-human-deploy` | the passive human does **not** unfold its MCV (the original run's human is fully idle) |
| `--long-game` | lobby ShortGame=no (the original copy's setting) |
| `--ai-passive` | slot 1 is not an AI, for the "map ticking" rung |
| `--human-country C` / `--ai-country C` | country and side of slots 0 and 1 |
| `--starts a,b` | pin combatant k to `[Waypoints]` index a, b, … The normal pick still runs first, so the RNG stream is unchanged |

The scenario matching the original run:

```
ra2engine --ai-soak --map xmp29u2.map --human-country YuriCountry --ai-country Russians \
          --no-human-deploy --long-game --starts 1,0 --frames 3000 --diff-dump clone.txt
```

The original put the human (Yuri) at waypoint 1 (cell 65,20) and the AI at waypoint 0 (52,97).

**Invariance check.** `min …`/`RESULT` soak output over 2700 frames is byte-identical with and
without `--diff-dump`. Two clone dumps of the same scenario are identical. No gate path reads the
new code: `g_simdiff_starts` is empty unless `--starts` is given, and the two new accessors are
const.

---

## 6. Differ and ladder; the baseline

```
python tools/simdiff/simdiff.py diff A B [--ignore rng2,uid,...] [--frames N] [--json out]
python tools/simdiff/simdiff.py l0 orig.l0 clone.l0
python tools/simdiff/simdiff.py markers A          # first move / new object / damage
python tools/simdiff/simdiff.py buildorder A B     # first frame of each type for house 1
python tools/simdiff/simdiff.py ladder ladder.json
pwsh tools/simdiff/run_ladder.ps1 -Engine <ra2engine.exe> -Out <dir>   # everything, once
```

**What `diff` reports**
- The strict, positional first divergence: frame, field, and both records.
- The first divergence of each field class: crc, rng, counts, object set, Logic order, uid,
  type, owner, position, facing, hp, mission.
- An order-free pairing of objects by (rtti, type, owner) at nearest position. It counts exact
  positions, positions within a cell, mean L1 distance, exact facings and exact HP. This keeps
  scoring progress meaningfully long before a strict compare could pass.

### 6.1 Baseline (seed `0x01234567`, XMP29U2, idle Yuri human vs Russian AI)

Command: `pwsh tools/simdiff/run_ladder.ps1 -Engine C:\ra2-h\build\ra2engine.exe -Out C:\ra2-h\ladder`.
Run once, 2026-10-10, with the clone at `net-h`. Original: `orig_ai` 11000 frames, `orig_ai_b` 600, `orig_passive` 1500.

| Rung | Scenario | Score | First divergent field / note |
|---|---|---|---|
| L0 | RNG algorithm: 1e5 raw + 1e5 ranged draws, seed `0x01234567` | **PASS** | `GameRandom` is bit-exact with `RandomClass` (N3's prediction confirmed) |
| ref | original vs original, 600 frames | **PASS** | identical except `rng2` (§7.2) |
| L1 | frame 0: map load + start forces | **frame 0** | `rng`; also counts, object set, Logic order (details below) |
| L2 | map ticking: both houses idle, 1500 frames | **frame 0** | `rng`. Order-free: 64 of 64 clone objects paired, 42 at the exact original position, held for all 1500 frames |
| L3 | first unit moves (markers) | info | original: first move at frame 27 (AI E2), new object at 26 (NACNST). Clone: move at 2192 (HARV), new object at 489 (NAPOWR) |
| L4 | combat (strict diff over the AI run) | **frame 0** | first HP loss: original frame 10806 (the AI hits the idle human's INIT). Clone: 4548 (an AI E2) |
| L5 | AI economy: house 1 build order | 13 of 22 types in both | see below |

Score = frames matched before the first strict divergence. Today every scenario rung scores 0.
The order-free pairing and the build order measure progress until rung L1 starts to pass.

**Details of the frame-0 divergence** (`orig_ai` vs `clone_ai`):

| Field | Original | Clone |
|---|---|---|
| sync RNG at frame 0 | idx 83/186 | crate RNG untouched: 0/103, unseeded by the session |
| technos | 132 (19 inf, 51 units, 62 buildings) | 64 (13 inf, 8 units, 43 buildings) |
| civilian vehicles (CAR, PTRUCK, BUS, CONA, STANG, SUVB, SUVW, TAXI…; 40) | present | **absent** |
| civilian buildings | 62 | 42. 42 of them are at the exact original location |
| human start force | PCV + 5 LTNK + 7 INIT | PCV + 4 LTNK + 4 INIT |
| AI start force | SMCV + 4 HTNK + 12 E2 | 3 HTNK + 9 E2, and the MCV **already deployed** (NACNST at frame 0; the original deploys at frame 26) |
| unit position origin | Location = cell×256 + **128** (e.g. PCV 16768, 5248) | `unit_lept_x` = cell×256 (16640, 5120): a **half-cell offset** in the clone's lepton convention |
| initial facing | 0 for every start unit | 32768 |
| mission | 5 (human), 11 Area Guard (AI) | not modelled |
| uid | 1034187… | not modelled |

**Later rungs**
- **L2.** With nobody acting, the original still moves its sync RNG by about 4 draws a frame
  (§7.1). Its object records never change position, but 19 idle facings change. The clone's
  objects stay frozen.
- **L3.** Original: first new object (NACNST) at frame 26, first unit move at frame 27 (E2 Area
  Guard dispersal). Clone: first new object (NAPOWR) at frame 489; nothing moves until the
  harvester at frame 2192.
- **L4.** The original AI first damages the idle human at frame **10806**. The clone's first HP
  loss is at frame 4548.
- **L5.** Build order of the AI house, original vs clone (first frame each type appears):

  | Type | Original | Clone |
  |---|---|---|
  | NACNST | 26 | 0 |
  | NAPOWR | 382 | 489 |
  | NAHAND | 656 | 820 |
  | NAREFN | 1900 | 2126 |
  | HARV | 1950 | 2178 |
  | FLAKT | 2340 | — |
  | NABNKR | 3792 | 4109 |
  | NALASR | 4064 | 3781 |
  | SENGINEER | 4448 | — |
  | CAOILD (captured) | 4678 | — |
  | NAWEAP | 5580 | 3442 |
  | NARADR | 7268 | 5308 |
  | TESLA | 9128 | 6296 |
  | HTK | 9264 | 5271 |
  | SPYP | 10874 | — |
  | NAWALL | — | 4225 |
  | NAFLAK | — | 7411 |
  | TTNK | — | 9338 |
  | DRON | — | 10420 |

  The opening is the same sequence, but late by 107–228 frames. After the refinery the clone
  techs faster: War Factory 2138 frames early, Tesla 2832 early. It also builds walls and flak
  that the original does not build in this run, and never captures the oil derrick.

---

## 7. Findings from the measurements

### 7.1 Who draws from the sync stream `Scenario+0x218`

From the RNG trace over 1200 frames of the AI run. Callers of the sync stream are given as
return address and containing function. Identities are tentative where marked with "?".

**Draws at frame 0 (scenario load)**
- `0x6F27B9` in `FUN_006F20A0`, the **TechnoClass constructor**: one raw draw per techno
  created (stored at techno `+0x3C8`). 132 draws at load, 136 by frame 1200. The RNG state
  therefore depends on the exact number and order of every techno created, civilians included.
- `0x684AE3` in `FUN_00684670` (256 ranged): scenario read / house assignment?
- `FUN_004FE880` / `FUN_004FED23` / `FUN_004FF053` (149 ranged each): HouseClass AI base setup?
- `FUN_00688910` (28): start-force creation.
- `FUN_00481090` (26): FindInfantrySubposition.
- `FUN_007221A0` / `FUN_007216E0` / `FUN_00722840`: Tiberium (ore) init.

**Draws every frame**
- `0x71BA06`, in undefined code in TerrainClass (`0x71B9F5`): **one raw draw per
  ore-spawning terrain object per frame**, `abs(Random()) % 1000000`, gated by type `+0x2B3`
  and object `+0xC0`. On XMP29U2 these four trees are the only actors in an idle game: about
  4500 draws per 1200 frames.
- Ranged draws once things move:
  - `FUN_004D4F90` (2354), `FUN_004D69C0` (434), `FUN_004D4120` (207): FootClass, i.e. movement
    and locomotion;
  - `0x708CD2` / `0x708D08` (854): TechnoClass, unanalysed code;
  - `0x51CBD4` / `0x51CC6A` / `0x51CE3C` / `0x51CDBE`: InfantryClass idle/fidget (N3's
    `0x51CBCF`);
  - `FUN_00423AE0` (45): AnimClass::AI;
  - `FUN_005052D0` (25): HouseClass AI.

### 7.2 The twin RNG `0x885B08` is not sync state

The twin is consumed by:
- `FUN_00481400` (132 draws at load);
- `FUN_004DA440`;
- the **audio event picker** `FUN_004047B0` / `FUN_004055C0`: random sound variation, ranged
  draws.

The audio draws depend on timing, which is why `rng2` differs between two otherwise identical
runs. The CRC never reads the twin. N3 called it "deterministic across peers, so also sync
state"; that should be read as "not sync". The differ compares it, but the ladder ignores it.

### 7.3 Other measured conventions

- **Building location.** `Location` is the centre of the foundation's origin cell, not of the
  foundation. Measured on CAOILD (2×2), CAAIRP (3×3) and CASTL01 (4×4).
- **The sync CRC is computed every frame.** It needs the five layers and the Logic list. The
  idle map already has 389 objects in layer 2 and 16 anims plus terrain in Logic: everything N3
  §3 lists as missing from the clone.

---

## 8. Prioritized work list for a bit-exact program

Order = what the ladder needs next. Each step is scoreable with this harness.

1. **Frame-0 object set (L1).**
   - Load and instantiate the map's civilian `[Units]`, `[Infantry]` and all `[Structures]`
     (40 vehicles and 20 buildings are missing on XMP29U2).
   - Reproduce the start force exactly: UnitCount 10 gives PCV + 5 LTNK + 7 INIT for Yuri and
     SMCV + 4 HTNK + 12 E2 for Russians.
   - Do not deploy the AI MCV on load.
   - Create objects in the original's order; the Logic order then follows.
2. **Coordinate and facing conventions.**
   - Lepton origin at the cell centre (+128). This is a one-line fix in the clone's lepton
     helpers, but every range check that uses them must follow.
   - The initial facing of spawned units is 0.
   - Infantry facing becomes a 16-bit DirStruct.
3. **One sync RNG seeded from the session seed.**
   - Wire `GameRandom` (L0 already passes) to `Seed`.
   - Draw once per techno constructor, at `+0x3C8`.
   - Reproduce the load-time draws (§7.1) in order.
   - This is agent L0's area; this harness confirms it.
4. **Idle ticking (L2).**
   - TerrainClass ore-spawner AI: one draw per tree per frame, plus the spawn rule.
   - Infantry idle fidgets and turns.
   - Ambient anims as Logic objects.
   - Then the map-ticking rung can pass for N frames. That needs the layers and Logic list as
     real objects, so `crc` becomes computable.
5. **AI opening timing (L3/L5).**
   - MCV deploy at frame 26.
   - Area-guard dispersal at frame 27.
   - Production timing: the clone is 107–228 frames late on every early building. The likely
     cause is the factory/credit-drain rate per frame, a fixed per-frame quantity.
6. **Per-object `AI()` order and integer locomotion** (N3 D5/D6), then FootClass ranged draws
   (`FUN_004D4F90` and friends), so movement-driven RNG order matches.
7. **Combat (L4).**
   - Targeting: the AI's first attack lands at frame 10806.
   - `_ftol` damage, FastMath distances (N3 D1–D3).
8. **Then re-pin to 1.001**, using §1.1 as the anchor set, before any CnCNet interop.

The CnCNet spawner's desync fixes would then have to be matched as well (N1 §8.2).

---

## 9. Caveats and housekeeping

- **Disk.** `D:` was full: 0.01–0.26 GB free while this ran, because of stale build
  directories from earlier waves. So the private Ghidra copy is `C:\ra2-h\h-proj` (not
  `D:\ra2-re\h-proj`), and the build dir and run outputs are under `C:\ra2-h\`. The game copy
  stays on `D:` because hard links must share the volume. It uses only about 10 MB of real
  space.
- **Scripts.** Ghidra helper scripts are in `D:\ra2-re\h-scripts\` (`run.sh`, `multi.py`).
  Headless runs must be started from Bash or as a background job. A PowerShell-launched
  `analyzeHeadless.bat` returned early, and its JVM was killed with partial output.
- **Mission is a value, not a match key.** `mission` is the raw vtable result. Values are
  comparable between two original runs. The clone has no mission model yet.
- **Not covered: scripted input.** One-unit-moving and combat scenarios with injected orders
  are not built yet. L3/L4 currently use the AI run's natural events.
  - Injecting a `MEGAMISSION` into `EventClass::OutList` (`0xA7F230`) at a fixed frame is the
    natural next step. The skirmish `Queue_AI` path executes the OutList directly. Pair it with
    the clone's L1 command work.
  - The original's own `-record`/`-play` (N3 §5) is the alternative.
