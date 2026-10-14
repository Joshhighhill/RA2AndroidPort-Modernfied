# Net gap 03: how far the clone's simulation is from bit-exact with gamemd.exe

**Question.** Online play that is compatible with the original game (joining original RA2/YR or CnCNet games) needs lockstep. Peers exchange only player events and compare a CRC of the game state every frame. A compatible client therefore has to simulate exactly as `gamemd.exe` does, bit for bit and frame by frame. This report measures how far the clone is from that, what bit-exactness would cost, and how progress could be measured.

**Scope.** Simulation fidelity only. The wire protocol and event format belong to N1, CnCNet to N2, the clone's own lockstep readiness (clone-vs-clone) to N4, and transport to N5. Section 3 (the sync CRC) is only what is needed to judge CRC parity; N1 owns the full protocol.

**How it was done.**
- Tree: `master` @ `43f4128`, in worktree `D:\ra2-wt\net3` (branch `net3`). No engine code was changed.
- Original: a private Ghidra copy `D:\ra2-re\net3` of `p4proj` (`gamemd.exe`), run headless, read-only.
  - Every script and every raw output is in `03-evidence/` (`ghidra/*.py`, `logs/*.txt`).
  - Addresses below are in this binary.
- Two measurements were run in Python against the installed `D:\Red Alert 2\gamemd.exe`, which is the same build:
  - `03-evidence/fastsqrt.py`: the FastMath square root versus the IEEE square root the clone uses.
  - `03-evidence/timer_drift.py`: the clone's timers kept in seconds versus counting whole frames.

---

## 0. Verdict

**Bit-exact compatibility with the original game is not reachable by evolving this clone.** It would take a separate project: an instruction-faithful re-implementation of the original simulation's ~2,000–2,850 functions, in the original's object model and update order. Very little of today's simulation code would carry over.

- **Effort:** of the order of **5–15 expert person-years** (section 4.3).
- **Risk:** high that it never fully converges. One wrong bit in any of ~640 random-number call sites or ~2,000 float-to-int conversions desyncs the match within seconds. That includes effects code (animations, particles, debris) that the clone treats as presentation.

**What is genuinely reusable:**
- the exact `RandomClass` generator (`engine/crate.cpp:33-91`);
- `rulesmd.ini` parsing;
- the ~550 cited original addresses in the RE notes;
- the formulas already ported with `_ftol` truncation.

**A harness that measures convergence is feasible on this machine, with caveats** (section 5). The binary has `-record`/`-play` switches, a per-frame CRC history, and an `ExtraSyncChecking` object logger. An AI-vs-AI game needs no input at all, so the first comparisons need no event translation.

**A blocking fact for compatibility:** the `gamemd.exe` that every RE note in this repository was done against is very likely **not the 1.001 build** that CnCNet's Yuri's Revenge runs (section 1). Every address would have to be re-pinned first.

**Recommendation:**
- Treat original-game compatibility as out of reach for this codebase.
- Build online play as **clone-to-clone lockstep** (N4's scope), which only needs the clone to be deterministic with itself.
- If original-game compatibility stays a goal, the only realistic route is to **run the original's own simulation code** rather than re-implement it. That is a different architecture, outside this report.

---

## 1. Which original? (a precondition)

- The RE project (`p4proj`) and `D:\Red Alert 2\gamemd.exe` are one build.
  - The FastMath sqrt table read from the file matches the Ghidra database: `tab[512]=0x3f07b`, `tab[1024]=0x7c3b6`, ...
  - File date 2001-09-18; VQA library string "Aug 22 2001".
  - SHA-1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`.
- Its global data sits at a **constant offset of −0x1098** from the public YRpp (Yuri's Revenge 1.001) constants:
  - frame counter: `0xA8DCEC` here, `0xA8ED84` in YRpp;
  - `ScenarioClass*`: `0xA8A198` here, `0xA8B230` in YRpp.
- Code is shifted too, by a varying amount:
  - `LogicClass::AI` is `0x55AC40` here; Phobos hooks `0x55AFB3`.
  - `AnimClass::AI` is `0x423AE0` here, `0x424CB0` in YRpp.
- **Conclusion (high confidence):** this is a pre-1.001 build, most likely retail 1.000.
- CnCNet YR games run 1.001 plus the CnCNet spawner patches (N2 to confirm the exact executable).
  - 1.001 fixed gameplay bugs, so its simulation differs from 1.000.
  - Every `FUN_xxxxxxxx` in `engine/` and `docs/re/` would need re-pinning against 1.001 before any compatibility work.
- Joining *original RA2* games (`game.exe`, `rules.ini`) is a different executable and rule set again. The clone runs on the YR data set by decision (U-1, `decision-yr-data-set`).

---

## 2. (a) The original's numeric model

### 2.1 Integers, floats and the x87

Instruction census over `.text` (`logs/survey.txt`):

| x87 mnemonic | count | | x87 mnemonic | count |
|---|---|---|---|---|
| FSTP | 6,866 | | FDIV | 943 |
| FLD | 5,660 | | FADDP | 579 |
| FMUL | 4,384 | | FSUB | 578 |
| FILD | 3,093 | | FISTP (only in `_ftol` + 3 others) | 9 |
| FADD | 1,069 | | FSQRT / FSIN / FCOS / FPATAN / FPTAN | **0** (`logs/s2.txt`: raw opcode hits all fall inside other instructions) |

**Model.**
- Coordinates are integer leptons (256 per cell; `CoordStruct` x/y/z ints at object `+0x9C/+0xA0/+0xA4`, as read by the CRC).
- Facings are 16-bit `DirStruct` values. The CRC folds them to 8 bits as `((dir>>7)+1)>>1`.
- HP, money and timers are integers. Timers count frames against the global frame counter `0xA8DCEC`.
- The **rule arithmetic in between is x87 double math truncated back to int**, everywhere:
  - `_ftol` is `FUN_007C4D00`: 1,977 call sites in 501 functions, 271 of them in the simulation call graph (`logs/reach.txt`).
  - It returns `(longlong)` of ST0 under a truncating control word (`logs/dec1.txt` top). Callers keep EAX.
  - So an out-of-range value **wraps modulo 2^32**. It does not saturate.

**FPU control word.**
- Game code never executes `FLDCW` except inside `_ftol` and CRT helpers (`0x7CAD14`, `0x7CE1xx`, `0x7D6CE0`, `0x7DAA77`).
- Three `FLDCW` bytes sit in undecoded code that was not examined.
- So the game runs on the process default: Win32's 53-bit precision, round-to-nearest.
- Under PC=53, x87 `+ − × ÷` produce the same results as SSE2 doubles, except for double rounding in the denormal range and the wider exponent range. That part is reproducible.
- What is hard is replicating, expression by expression, **where the 1999–2001 MSVC compiler spilled to memory** (float vs double stores) and **in what order it evaluated**. The decompiler's 80-bit x87 temporary type hides both, so a port has to be checked against the assembly, not the C.
- **Open risk:** DirectDraw wrappers used on CnCNet (cnc-ddraw/ts-ddraw; the install has a 42 KB `ddraw.dll`) could change the x87 control word if they create a Direct3D device without FPU-preserve. Mixed-renderer games evidently stay in sync, so this probably does not matter. N2 should confirm.

### 2.2 FastMath: transcendentals are float32 lookup tables

The original never uses the CRT `sqrt/sin/atan2`. All of them are Westwood FastMath tables in `.data`, shipped inside the executable (`logs/s2.txt`, `logs/s3.txt`):

| Function | Refs | Table | Algorithm |
|---|---|---|---|
| `FUN_004CAB60` sqrt | 550 refs, 443 functions | `0x86403C`, 16,384 int mantissas | Arg rounded to **float32**, mantissa index = top 14 bits incl. exponent parity, exponent halved. Max rel. error 6.1e-5 |
| `FUN_004CABD0` sin / `FUN_004CAC20` cos | 151 / 161 refs | `0x84E004`, float32, 8,192 steps/turn | `_ftol(x·k)`, half-step rounding |
| `FUN_004CAC70` (tan) | 254 refs | `0x85C024`, 4,096 entries | |
| `FUN_004CACA0` / `FUN_004CACD0` asin / acos | 4 / 5 refs | `0x858014` | |
| `FUN_004CAD00` atan / `FUN_004CAD50` atan2 | 509 / 72 refs | `0x860034`, 4,097 entries | quadrant fix-ups in float32 |

This is good news for portability: the tables can be copied byte for byte and evaluated with IEEE float32 on ARM64 or x86-64. It is bad news for the clone, which uses libm `std::sqrt/atan2/sin/cos/hypot` at 49 + 19 + 52 + 36 sites.

**Measured** (`03-evidence/fastsqrt.py`, 200,000 random offsets within ±30 cells):
- `_ftol(FastSqrt(d²))` differs from the clone's `(int)std::sqrt(d²)` in **13.8 %** of distances, always by 1 lepton. Examples: (6204, 5464) → 8266 vs 8267; (−383, −3317) → 3338 vs 3339.
- Whole-cell offsets happen to agree (0 of 121).
- Every range check, splash falloff (`FUN_00489190`) and scatter that uses a distance is therefore off by one lepton about one time in seven.

### 2.3 Random numbers

**The generator (exact).**
- `RandomClass` = R250 over 250 words, seeded by 4-round psdes:
  - seed: `FUN_0065C110`;
  - draw: `FUN_0065C1C0`;
  - ranged draw: `FUN_0065C220`, which rejection-samples `next() & mask(bitlen(hi−lo))`.
- The clone already has this exactly (`engine/crate.hpp:12-24`, `engine/crate.cpp:33-91`).

**Instances and seeding** (`logs/dec1.txt` `FUN_0052F960` "Init random number"; `logs/s3.txt`):

| Instance | Seeded from | Use (call sites by `this`) |
|---|---|---|
| `Scenario->Random` = `[0xA8A198]+0x218` | the session seed `DAT_00A8DCFC`. Single-player: from `GetSystemTime` and `GetTickCount`. Multiplayer, playback, or seed preset: the session value ("Seed is %08x") | the main sync stream: ~109 + 182 ranged and ~25 + 39 raw sites (the "?" sites compute `ECX` as `LEA [reg+0x218]`) |
| `0x885B08` (≈ YRpp's global randomizer) | **the same seed**, re-seeded right after | 70 ranged + 18 raw sites. Deterministic across peers, so also sync state |
| `0xABD800` | the random-map seed (`FUN_00598390`, "RMG: Init random map") | random-map generator only (136 raw draws in `0x58C000–0x5A9000`) |
| CRT `_rand` `0x7CA2AA` | — | 8 refs (UI/shell) |

**Call sites.**
- `FUN_0065C220`: 397 references in 224 functions. `FUN_0065C1C0`: 244 references in 102 functions (`logs/c1.txt`).
- At least 133 of the 237 random-drawing functions are in the simulation call graph. That is a lower bound, because the graph walk under-resolves virtual calls.
- Draws come from effects as well as gameplay:
  - `AnimClass::AI` `0x423AE0` (6 draws);
  - `BounceClass` `0x421EC0`;
  - particle systems `0x62E2E0` / `0x62ECD0` / `0x62B080` ff.;
  - `BulletClass::Detonate` `0x4690A0` (6);
  - DamageArea `0x489190` (6);
  - ore spread `0x483690` / `0x4870A0`;
  - infantry idle code `0x51CBCF` ff.;
  - the AI trigger driver `0x6F0010`;
  - `TechnoClass::Fire` `0x6FD210`;
  - crate code, and more.
- So **the random-number order depends on every animation, particle and bullet that exists**, not only on gameplay decisions.

**The CRC itself draws from the sync stream** (section 3). In every session type except 5, each frame costs two `Scenario->Random.next()` calls: `0x64D87E` inside the CRC and `0x6504F7` in the logger.

**RE-note correction.** `docs/re/ai-base.md:30`, `docs/re/ai-team.md:32, 63, 220, 396, 406, 525, 585` call `FUN_007C4D00` "the raw RNG draw". It is `_ftol` (float-to-int truncation). The "RNG factor" readings in those sections are float-to-int conversions, and the real random sources there need re-reading.

### 2.4 Update order

**The main loop** (`FUN_0055CFF0`, `logs/dec1.txt`): input and events, then `LogicClass::AI` `FUN_0055AC40`, then network `Queue_AI` `FUN_00646D00` (event execution, the CRC `FUN_0064D520` into history `0xB033DC[frame&0xFF]`, the sync compare `FUN_0064BDF0`), then `++Frame` (`0xA8DCEC`). Record/playback I/O sits in the same loop (section 5).

**Inside `LogicClass::AI`** (`FUN_0055AC40`). Identities are inferred from structure; the order is exact:
1. Frame bookkeeping; global trigger-event springs per house (`FUN_006E4910` with event ids 0x32/0x1B/0x1C/0x24/0x25/0x2D/0x2E/0x0D/0x33); scenario timers; `FUN_00577B30` every 120 frames; weather/storm timers (`FUN_00539E40…FUN_0053B130`).
2. `FUN_00721EE0`, `FUN_00721450`, `FUN_00438B70`, `FUN_0054E160`, `FUN_0055B7D0(0,0)`.
3. Filtered snapshot of vector `0x8B3054`, then `AI()` (vtable +0x5C) on each, forward.
4. Vector `0x89F17C`: `AI()` **in reverse index order**.
5. `FUN_005FEB80`, `FUN_0054FDE0`, `FUN_0053A3F0`; vector `0xB03B3C` in reverse.
6. `FUN_005549E0`, `FUN_004C53C0`.
7. **The Logic layer itself (`this+4`, count `this+0x10`): `AI()` per object, forward.** Each object runs its whole turn here (mission, locomotor, targeting, firing) before the next object moves.
8. Multiplayer only (session ≠ 0, ≠ 5): vector `0xA82D6C`.
9. `FUN_0053D040`, `FUN_00420EB0`, `FUN_0056B710`, singleton `[0x8862A4]->AI()`, vector `0xA82D9C`.
10. **Houses (`0xA7F194`) `AI()` last**, after all objects.

**Pathfinding.**
- A* over cells with the per-step height gate `FUN_004D9B70` and cell-occupancy virtuals (`FUN_00429A10`, cited in `engine/pathfind.hpp`).
- Its cost function, tie-breaking, heap order and node limits decide which of several equal-cost paths a unit takes, and so its position on every later frame.
- None of that is traced in this repository.

---

## 3. (c) The sync CRC

`FUN_0064D520` (`logs/s4.txt`). It is called from `FUN_00647090` every frame unless session type = 5. Here `Add(v)` is `crc = ROL1(crc) + v`, the helper at `0x64D8C0`.

```
crc = 0
for o in vector 0xA82D54 (infantry):          Add(o.X/10 + facing8(o) + (o.Y/10)<<16)
for o in vector 0x8B3074 (units):             Add(o.X/10 + (o.Y/10)<<16 + facing8(body) + facing8(turret))
for o in vector 0xA8DAAC (third techno vector, N1 to pin): Add(o.X/10 + facing8(o) + (o.Y/10)<<16)
for L in 5 map layers @0x89F2D4 (stride 0x18), for o in L:
      unless (o.WhatAmI()==4 /*Anim*/ && o.type->vfn+0x10 == -2):  Add(o.X/10 + o.WhatAmI() + (o.Y/10)<<16)
for o in vector 0x87E6FC (same rule as layers)
Add(Scenario->Random.next())                  // consumes a draw
FUN_006504D0(frame & 0xFF)                    // another Scenario->Random.next() (logger)
history[0xB033DC + (frame & 0xFF)] = crc
```

- A peer's event carries its CRC for frame `F − delay`. `FUN_0064BDF0` compares it with `history[(F − delay) & 0xFF]`.
- On a mismatch the game writes `SYNC%d.TXT` (`FUN_0064D8E0`: a per-class CRC dump plus the RNG value). With `ExtraSyncChecking` on (`0x5377E0`) it writes `SYNC%d_%03d.TXT` (`FUN_00651130`): 256 frames of per-object snapshots (position, facing, type name, owner, health…) from the ring filled by `FUN_006504D0`.

**Could the clone produce an identical CRC if its state matched? Not today, for structural reasons:**
1. **Object set.** The layers hold every `ObjectClass`: animations, bullets, terrain, overlay and smudge objects, voxel animations, and so on. In the clone, animations and most effects are presentation (`engine/pres_*.inc`), outside `World`. They would have to become simulated objects, created in the original's order, with the original's random draws.
2. **Order.**
   - The CRC walks the original's `DynamicVectorClass` order: creation order, compacted on removal.
   - It also walks layer order, and layers are re-sorted for drawing. Whether the CRC sees sorted order, and with what sort stability, must be checked; `FUN_0055B7D0` is the candidate.
   - The clone keeps one `units_` vector with tombstones (`engine/world.cpp:4880-4891`), buildings separately, and no layers.
3. **Representation.**
   - Positions are double cells (`Unit::fx/fy`, `engine/world.hpp:99`) plus sub-cell double offsets (`world.hpp:137`). Leptons/10 needs the exact integer lepton coordinate.
   - Vehicle facings are double u16 (`world.hpp:170-171`). Infantry facing is an 8-way index (`world.hpp:105`), which loses the 16-bit direction.
4. **RNG coupling.** The clone has no single sync stream (section 4.1), so it cannot add `Random.next()` to the CRC or advance the stream twice per frame.
5. Then **the house/sides/trigger sections** that `FUN_0064D8E0` also prints would need checking against the 1.001 CRC. That is N1's scope.

Computing the formula is easy. Every input to it is a deep divergence.

---

## 4. (b) and (d) The clone's numeric model, the divergences, and a census

### 4.1 The clone's numeric model

- **Time.**
  - Fixed tick `kLogicDt = 1/15 s` (`engine/game_scene_controls.inc:359`; accumulator in `engine/main.cpp:6374-6379`). Every subsystem receives `double dt` seconds; there are 140 `dt` uses in simulation files.
  - The original counts integer frames. GameSpeed only changes how many frames run per second (`docs/re/gamespeed.md`).
- **Space.**
  - Unit position is `double fx, fy` in cells (`world.hpp:99`).
  - Movement is `move_cps` cells/s × `dt` (`world.hpp:565`, `world.cpp:5488`), with `speed_mult` doubles (`world.hpp:143`).
  - The original uses integer leptons with integer speeds of leptons/frame (`docs/re/gamespeed.md` §3, `type+0x678`).
- **Facing.** Infantry `int facing` 0–7 (`world.hpp:105`); vehicles `double body_facing/turret_facing` (`world.hpp:170-171`); `std::atan2` + `std::fmod` (`world.cpp:59, 82`).
- **Libm.**
  - Simulation files use libm 137 times. Engine-wide: `sqrt` 49, `sin/cos` 52, `atan2` 19, `hypot` 36, `round/floor/ceil` 235.
  - `-ffp-contract=off` is set (`CMakeLists.txt:363`), but desktop MinGW libm and Android bionic libm are not guaranteed to agree for `sin/cos/atan2`. N4 should note this for clone-vs-clone play too.
- **RNG streams** (none is the original's sync stream except the crate one):

| Stream | Where | Kind |
|---|---|---|
| `GameRandom` (exact R250) | `crate.hpp:14`, crates only | exact; seed = wall clock (`main.cpp:5529`) or a mission-name hash |
| `ProjRng` | `projectile.hpp:79` | xorshift32 + exact `FUN_0065C220` range mapping; scatter |
| `combat_rng_` | `world.hpp:2567` | ProjRng, seed `0x2545F491`; ROF jitter |
| `OreRng` | `ore.hpp:37` | xorshift32, fixed seed |
| `ai_rand` (per house) | `ai_base.cpp:8` | xorshift32 (AI base, team, trigger draws: `ai_team.cpp:42, 53, 444`, `ai_trigger.cpp:205`) |
| map triggers | `map_trigger.cpp:486-492` | xorshift32, modulo mapping |
| infantry idles | `inf_anim.cpp:33-40` | per-unit LCG, modulo mapping |
| mirage disguise | `world.cpp:2446-2447` | per-unit multiplicative hash |
| skirmish starts / lobby | `gf_start.inc:22`, `skirmish_lobby.cpp:16` | map-name-seeded LCG |

- **Update order.** `World::step` (`engine/world.cpp:4869-5420`) runs **system phases over all units**, not per-object turns:
  - all movement (`step_units`, `world.cpp:4948`), then crates, playfield, visibility, cargo, capture, depots, disguises, deploy, flight, air/naval, superweapons, tech structures, turrets, buildings, repair/sell, projectiles, combat (`step_combat`), building combat, depth, w5, veterancy, mines, harvest, AI houses, AI teams, abilities, victory, sub-offsets;
  - campaign triggers and production run from `main.cpp` around `World::step`.
- In the original, unit A finishes its whole turn (move *and* fire) before unit B starts, and houses run last. Any interaction inside one frame resolves differently. **This is an architectural mismatch, not a calibration bug.**

### 4.2 Concrete divergences (each one alone breaks lockstep)

| # | Clone | Original | Effect |
|---|---|---|---|
| D1 | `std::sqrt` distances: `combat.cpp:420` `dist3_i`, `combat.cpp:159, 521, 1276`, `world.cpp:5649, 5792`, `abilities.cpp:69` (whose comment cites `FUN_004CAB60` but calls `std::sqrt`) | FastMath `FUN_004CAB60` (float32 table) | 13.8 % of distances off by 1 lepton (measured) |
| D2 | `std::atan2/fmod` facing, `world.cpp:59, 82`; `double` facings | `FUN_004CAD50` atan2 table + `_ftol` to a 16-bit `DirStruct` | facing values and turn-completion frames differ |
| D3 | rearm timer in seconds: `combat.cpp:199` `(frames+jitter)/15.0`, decremented by `dt` at `combat.cpp:876`, fires at `<= 0` | integer frame timer (`FUN_006FC460` 0x6FC55E: `RearmTimer = ftol(ROF·houseROF + Random(0,2))`) | **247 of 300 timer lengths expire one frame late** (k ≥ 3 → k+1; measured, `timer_drift.py`). Not uniform even within the clone. Also a fidelity bug outside netplay |
| D4 | 9 independent RNG streams (table above) | one sync stream (plus a twin at `0x885B08`), ~640 call sites in a fixed order | every random outcome differs from the first draw |
| D5 | movement in double cells/s (`world.hpp:565`) | integer leptons/frame; locomotors such as Drive `FUN_004B2550` | positions differ within frames |
| D6 | phase-ordered `World::step` (`world.cpp:4869`) | per-object `AI()` in Logic-vector order, houses last (`0x55AC40`) | intra-frame interaction order |
| D7 | generic octile A*, 10/14 costs (`pathfind.hpp`, corner rule `[RE-FALLBACK]` `pathfind.cpp:139`) | untraced A* (`FUN_00429A10` neighbourhood) | different equal-cost paths |
| D8 | animations, particles, debris are presentation | sim objects in the CRC layers that draw from the sync RNG | object set and RNG order |
| D9 | `static_cast<int>(double)` for `_ftol` (`combat.cpp:417`, `projectile.cpp:31`, `crate.cpp:20`, `veterancy.cpp:9`) | `_ftol` returns 64-bit, caller keeps low 32 → wraps | out-of-range casts: C++ UB; x86-64 gives `0x80000000`, ARM64 saturates, original wraps |
| D10 | crate seed from wall clock (`main.cpp:5529`) | session seed `DAT_00A8DCFC` | — |
| D11 | ~320 `[RE-FALLBACK]` rules (section 4.3) | traced or not, the original's code | each one is a behavioural divergence, not just a numeric one |

Every row needs fixing for bit-exactness. D1–D3 and D9 are cheap locally. D4–D8 mean rewriting the simulation.

### 4.3 Census: calibrated vs guessed, and the effort

| Measure | Value | Source |
|---|---|---|
| Engine source (`engine/*.cpp/hpp/inc`) | 84,615 lines; simulation `.cpp` subset 19,496 lines | `wc` |
| `[RE:` citations | 1,280 | grep |
| `RE-GATE` markers | 295 | grep |
| `[RE-FALLBACK]` markers (code) | 320 in engine (164 in `.cpp/.h` incl. tests/src); 112 in simulation `.cpp` | grep |
| `DEVIATION` markers | 48 | grep |
| REMAINING.md | 135 rows; 22 RE-FALLBACK rows; "about 230 `[RE-FALLBACK]` guesses" | `docs/gap/REMAINING.md` |
| Distinct `FUN_` addresses cited by the engine | 550 | `logs/cited.txt` |
| Original functions in total | 9,442 | `logs/reach.txt` |
| Simulation call graph (from `LogicClass::AI` + `AI()` of 52 sim classes) | 2,216 static / **3,087** with approximate virtual dispatch (incl. ~220–240 CRT) | `logs/reach.txt` |
| Cited addresses inside that graph | **318** (≈ 11–14 % of the game functions in it) | `logs/reach.txt` |
| README gate re-pins | 39 mentions: gate hashes still move with every RE fix | `README.md` |

How to read this:
- A "citation" in the clone means **the rule was calibrated** from that function (constants, formulas, branch structure), re-expressed in the clone's own model: double seconds, double cells, phase order, its own RNG.
- None of the 318 is an instruction-faithful port in the sense lockstep needs. `GameRandom` and the `_ftol`-truncating damage formulas come closest.
- The other ~2,000+ functions of the simulation graph have never been looked at. That is ~85 % by count, and it includes all of AnimClass, Particle and Bounce, the locomotors' inner loops, and the pathfinder.

**Effort estimate.**
- A faithful port means 2,000–2,850 functions, each matched to the assembly: x87 spill points, `_ftol` sites, FastMath calls, random draws in order, virtual dispatch.
- Each needs per-function differential checking. At **0.5–2 expert-days per function including verification**, that is **1,000–5,700 person-days ≈ 5–25 person-years**. A realistic central figure is 5–15 person-years.
- On top of that, the 1.001 re-pin (section 1) and building the harness (section 5, about 1–2 months).
- Agent help speeds up the transcription but not the verification, which is what limits the pace.
- Precedent: OpenRCT2 reached full replacement of RollerCoaster Tycoon 2 only by **running inside the original executable and replacing one function at a time**, each validated live, over several years with many contributors.

**Rewrite list (nothing below survives as-is):**
- time base (seconds → frames);
- coordinates (double cells → leptons) and facings (→ `DirStruct` + `FacingClass` with ROT);
- one sync RNG threaded through every caller in original order;
- object model (typed `DynamicVector`s, layers, Logic vector, creation/compaction order);
- per-object `AI()` dispatch;
- locomotors;
- pathfinder;
- animations, particles, bullets, debris as sim objects;
- every subsystem currently marked `[RE-FALLBACK]`: AI (~55 markers, R-083), harvest/ore (~20, R-190), combat edge rules (R-191), and the rest.

**Risk of never converging (high):**
- One flipped bit anywhere spreads into positions or random-number order within a few frames.
- The long tail includes visual-only-looking code paths: anim random frames, debris bounce, particle drift.
- 1.001 behaviour includes bugs that must be reproduced exactly: the original's own uninitialised-memory reads, iteration while removing, and so on.
- Any undefined-but-deterministic x87/MSVC behaviour must be emulated on ARM64.

---

## 5. Measuring progress: a differential harness

**Feasible here, with caveats.** Everything a harness needs exists in the original:
- **A deterministic driver needing no input translation.**
  - An AI-vs-AI skirmish with a fixed session seed produces zero player events.
  - `Init_Random` (`FUN_0052F960`) takes the seed from session state whenever the game is networked, in playback, or the seed is preset; the CnCNet spawner sets it via `spawn.ini`.
  - For human input: `-record` / `-play` exist in this build (`FUN_0052F360`, `DAT_00A8E6F4` = 1 or 2; `RECORD.BIN` written by `FUN_00696AB0`). The main loop serialises each frame's selection set plus a checksum, and on playback verifies it (`FUN_0055CFF0`: `FUN_00473A10` write, `FUN_00473A40` read, mismatch → `FUN_0048DBA0`). Whether playback works for skirmish in 1.001 is **untested**.
- **Per-frame state to compare.**
  - The CRC history `0xB033DC[frame&0xFF]`.
  - The `ExtraSyncChecking` ring (`FUN_006504D0`): per-object position, facing, type, owner, health, in vector order.
  - Debug-log checkpoints such as "Creating %d units - Random seed is %08x" and "Finished unit generation. Random number is %d".
- **A probe.** A 32-bit DLL loaded through a `ddraw.dll` proxy or a Syringe-style loader. It hooks the end of `LogicClass::AI` (`0x55AC40`) and dumps, in vector order:
  - the frame number, the Scenario RNG state (`idx1/idx2` + table hash), and the CRC recomputed **without** the RNG draw;
  - per techno: id, type, owner, lepton xyz, `DirStruct`, HP, mission, target.
  - The probe must never call `FUN_0064D520` in a session type where the game does not, because that call advances the RNG.
- **The clone side.**
  - The same dump from `World` (lepton coordinates, u16 facings, RNG state).
  - `scripts/play` plus `--ai-soak` already give scripted, headless runs.
  - A small differ reports the first divergent frame and field.

**Caveats:**
- The toolchain on this box is x86_64 MinGW only. A 32-bit DLL needs an i686 MinGW or `clang --target=i686-w64-windows-gnu`.
- The target must be **1.001**, i.e. CnCNet's executable, not the 1.000 build used so far. All hook addresses change.
- Running the game on Windows 11 needs the windowed `ddraw` wrapper. Runs are 32-bit and should be done one at a time.
- A clone run takes about 2 minutes and 3 GB per gate (`gate-runtime-and-false-hangs`).

**Progress metric.** A ladder of scenarios, each scored by **frames to first divergence** (and the field that diverged), on a fixed corpus of seeds and maps:

| Level | Scenario | Exercises |
|---|---|---|
| L0 | seed → first 10⁶ draws of both RNG instances | `GameRandom` (expected to pass today) |
| L1 | map load, frame 0: object vectors, IDs, lepton positions, CRC | object model, map/INI load order |
| L2 | idle map, 2,000 frames (ore growth, animated terrain, ambient anims, crates) | anim/particle RNG, ore spread |
| L3 | map-trigger-scripted moves (triggers as input, no events) | locomotors, pathfinder, occupancy |
| L4 | scripted combat (trigger-spawned opposing units) | targeting, `_ftol` damage, projectiles, debris |
| L5 | AI-vs-AI skirmish | AI, production, economy |
| L6 | `RECORD.BIN` replays of human games | event execution |

**Expected status today:**
- L0 passes.
- L1 fails at frame 0, because animations and terrain are not objects and the vector layout differs.
- L2 and beyond diverge within the first few frames.
- The metric stays at about 0 frames until the time, space, RNG and object-model rewrite lands. That itself shows why this cannot be done incrementally inside the current architecture.

---

## 6. What to do instead (inputs for the roadmap)

1. **Clone-to-clone lockstep (N4).**
   - Needs only self-determinism: one seeded simulation RNG, integer frame timers, no libm transcendentals (or FastMath-style tables, which also lifts fidelity), `-ffp-contract=off` (already set), and no wall-clock seeds (`main.cpp:5529`).
   - Several items here also improve single-player fidelity and are cheap: D1 (copy the FastMath tables), D3 (frame-integer timers), D9 (`_ftol` wrap), D10 (session seed).
2. **If original compatibility stays a hard requirement**, it should be scoped as running the original simulation, not re-implementing it. That route has its own feasibility questions: x86 emulation on Android, the user's own game files, CnCNet's spawner (N2/N5). It is not an extension of this engine.
3. **Fix the RE notes.** `FUN_007C4D00` is `_ftol`, not an RNG draw (section 2.3), in `docs/re/ai-base.md` and `docs/re/ai-team.md`. Also record that the RE build is pre-1.001 (section 1).

---

### Evidence index (`03-evidence/`)

| File | What it shows |
|---|---|
| `logs/survey.txt` | x87 census; RNG and `LogicClass` xrefs; sync-related strings |
| `logs/dec1.txt` | `_ftol`; `Init_Random`; seeders; `LogicClass::AI`; main loop; out-of-sync handler; FastMath sin/cos; `Print_CRCs` |
| `logs/s2.txt` | raw opcode scan (no x87 transcendentals); CRC history writers; FastMath region |
| `logs/s3.txt` | FastMath tables (initialised `.data`); RNG `this` census; `FUN_00647090` CRC store |
| `logs/s4.txt`, `logs/s5.txt` | `FUN_0064D520` (the CRC) and `FUN_006504D0` (sync logger) |
| `logs/d1.txt`–`logs/d3.txt` | disassembly proving both CRC-time draws are `Scenario+0x218` |
| `logs/c1.txt` | per-function call counts: `RandomClass`, FastMath sqrt/atan2/sin |
| `logs/reach.txt` | simulation call-graph size; cited-address overlap |
| `logs/st1.txt`, `logs/st2.txt` | `-record`/`-play`, `RECORD.BIN`, `ExtraSyncChecking`, seed strings; version strings |
| `logs/cited.txt` | the 550 `FUN_` addresses cited in `engine/` |
| `fastsqrt.py` | FastMath sqrt vs IEEE: the 13.8 % measurement (reads the table from the installed exe) |
| `timer_drift.py` | seconds-based timer vs frame countdown: the 247/300 measurement |
| `ghidra/*.py`, `ghidra/run.ps1` | the headless scripts (private project `D:\ra2-re\net3`, read-only) |
