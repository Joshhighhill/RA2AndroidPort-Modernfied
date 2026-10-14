# Superweapons (Nuke / MultiMissile) — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
(`sw_hunt1.py … sw_hunt10.py`), the extracted `rulesmd.ini`, decompile dumps
(`out_sw/`), and the `ra2extract` art probes are all off-repo in the session
scratchpad; nothing off-repo is committed. **Content-anchored only:** every
function below was reached from INI-key strings, the Type/Action enum name
tables, vtable layout, and xref/call-target evidence in THIS binary — never from
published Ares/YRpp addresses. The Ares/YRpp address drift documented in
`combat.md` §A and `workflow.md` §4 held here too — the `SuperClass`/
`SuperWeaponTypeClass` code is a completely different address range from any
published hook.)

This document is the calibration authority for the **CB-4 Superweapons** phase.
The exemplar is the **Nuke `[NukeSpecial]` (`Type=MultiMissile`)**; the other
eleven SW `Type`s are RE-documented (enum recovered) but only the area-damage
Nuke is implemented. The rules VALUES quoted below were read on this install and
are ground truth already; this hunt pins the FORMULAS / offsets / semantics that
turn them into engine constants. Confidence per finding; "Deviations we take" at
the end resolves the `[RE-GATE: superweapon.md §N]` markers in the CB-4 plan.

## TL;DR — the deltas from the plan's pre-hunt assumptions (what the RE-GATE needs)

1. **`RechargeTime` is MINUTES, converted at LOAD time by `× 900` (= 60 s × 15 fps),
   truncated to int frames.** [§1, CONFIRMED by disassembly]. `[NukeSpecial]
   RechargeTime=10` → `trunc(10 × 900) = 9000` frames = 600 s = 10 min @ 15 Hz.
   The multiply is a literal `FMUL dword [0x7F3100]` where `[0x7F3100] = 900.0f`,
   followed by `_ftol` (`FUN_007c4d00`), stored as an **int** at
   `SuperWeaponTypeClass+0xB0`. This is exactly the `ai-base.md` §1 BuildDelay
   `kLogicFps=15` precedent, now pinned to a single instruction. **NOT
   `[RE-FALLBACK]** — Task 2/4 `recharge_secs` = `RechargeTime × 60`, frames =
   `RechargeTime × 900`.

2. **Firing resets the timer to a FULL recharge** (not to 0-remaining, not to a
   partial value) for the Nuke/MultiMissile path. [§1, CONFIRMED]. After the fire
   dispatcher returns, `SuperClass::Fire` sets `TimeLeft(+0x38) = RechargeTime
   frames (type+0xB0, or the per-instance override +0x24 if set)` and
   `StartTime(+0x30) = CurrentFrame`. (A separate two-phase "charge-drain" model
   exists for `UseChargeDrain=true` SWs; `[NukeSpecial]` does not set it, so it
   takes the full-reset branch.)

3. **The target-cell damage is the `[NukePayload]`/`[NUKE]` DOWN-strike, NOT the
   SW's own `WeaponType=NukeCarrier`/`NukeMaker` UP-launch.** [§2, CONFIRMED].
   The rules `[Warheads]` list documents this verbatim: `33=NUKE ; new
   multimissile` (the damaging warhead) and `54=NukeMaker ; virtual WH that spawns
   the down nuke on explosion` (the up-launch, which does NO area damage — it has
   no `Damage`/`Verses`). The binary fire dispatcher (`FUN_006cba10` case 0)
   launches the up-bullet with the `[GiantNukeUp] DetonationAltitude=20000` and
   the SW's `WeaponType`; the descending leg (`GiantNukeDown`, `NUKE` warhead)
   applies the CellSpread damage. There is **no `[Nuke]` warhead section** — the
   legacy hardcoded `[SpecialWeapons] NukeWarhead=Nuke` silo path is a different,
   effectively-unused branch. Task 4 damages the target cell with `[NukePayload]`
   `Damage=600` × `[NUKE]` `Verses[armor]` via the CellSpread falloff.

4. **The recovered SW `Type` enum has 12 entries and differs from the plan's
   assumed list.** [§4, CONFIRMED from the binary name table `0x841560`]:
   `0=MultiMissile, 1=IronCurtain, 2=LightningStorm, 3=ChronoSphere,
   4=ChronoWarp, 5=ParaDrop, 6=AmerParaDrop, 7=PsychicDominator, 8=SpyPlane,
   9=GeneticConverter, 10=ForceShield, 11=PsychicReveal`. **Deltas vs plan:**
   `AmerParaDrop` is a **distinct** enum entry (index 6), NOT folded into
   `ParaDrop`; `SpyPlane` is index 8 (the plan omitted it and `AmerParaDrop`);
   the ordering is `LightningStorm` at 2 and `PsychicDominator` at 7 (corroborated
   independently — `SuperClass::Fire` special-cases `Type==2` and `Type==7`).

5. **Buildings DO take nuke splash.** [§2 / building-splash question, CONFIRMED].
   The `[NUKE]` warhead area-damage function (`FUN_00489190`, the `combat.md` §7
   `apply_spread_damage`) enumerates **every** damageable object in the CellSpread
   radius (filter = IsActive + `Strength > 0`) and applies virtual `ReceiveDamage`
   to each — buildings included (they have `Strength`); no unit-only filter. The
   `[NUKE]` flags `WallAbsoluteDestroyer=yes` + `Wood=yes` corroborate structure/
   wall destruction. **RE-GATE §5.3 decision: add the SW-only building loop in
   Task 4** (our reused `apply_spread_damage` currently damages units only).

6. **No RNG anywhere on the SW charge or the Nuke detonation critical path.**
   [§5, CONFIRMED]. Zero calls to the R250 `Randomizer` (`FUN_0065C220`/
   `FUN_0065C1C0`, `combat.md` §5) appear in ANY function of the SuperClass class
   region (`0x6C9000–0x6CDB20`) or the timer helpers — the timer is pure frame
   arithmetic against the global frame counter `DAT_00a8dcec`.

7. **Art present — NOT a demo-art `[RE-FALLBACK]`.** `NukeIcon`
   (`language.mix/cameo.mix/nukeicon.shp`, 2912 B) and `NUKEANIM`
   (`ra2.mix/conquer.mix/nukeanim.shp`, 966 984 B, plus `nukeball.shp`) are both
   present on this install. Task 5 loads them.

## Recovered ground truth (read from this install's `rulesmd.ini`)

```
[SuperWeaponTypes]   (id = SW section name)
1=NukeSpecial   2=IronCurtainSpecial   3=LightningStormSpecial
4=ChronoSphereSpecial   5=ChronoWarpSpecial   6=ParaDropSpecial
7=AmericanParaDropSpecial   8=PsychicDominatorSpecial   9=SpyPlaneSpecial
10=GeneticConverterSpecial   11=ForceShieldSpecial   12=PsychicRevealSpecial

[NukeSpecial]  IsPowered=true  RechargeTime=10  Type=MultiMissile
   SidebarImage=NukeIcon  Action=Nuke  WeaponType=NukeCarrier  ShowTimer=yes
   DisableableFromShell=yes  AIDefendAgainst=yes  Range=7  LineMultiplier=2
[NukeCarrier]  Projectile=GiantNukeUp  Speed=100  Warhead=NukeMaker   ; UP-launch
[NukeMaker]    NukeMaker=yes            ; "shoots straight up ... spawns the down nuke"
[NukePayload]  Damage=600  Range=30  Projectile=GiantNukeDown  Speed=10
   RadLevel=500  Warhead=NUKE           ; DOWN-strike (the damaging leg)
[NUKE]  CellSpread=10  PercentAtMax=.02  WallAbsoluteDestroyer=yes  Wood=yes
   Verses=100%,100%,100%,200%,100%,100%,60%,100%,8%,100%,100%
   InfDeath=7  Tiberium=yes  AnimList=NUKEANIM
[GiantNukeUp]   Vertical=yes  DetonationAltitude=20000  Image=NKMSLUP  Arm=2
[GiantNukeDown] Vertical=yes  DetonationAltitude=30000  Image=NKMSLDN  Arm=2

[Warheads]  33=NUKE ; new multimissile    54=NukeMaker ; virtual WH that spawns the down nuke on explosion
```

`[NANRCT]` (Soviet Nuclear Reactor) carries `DeathWeapon=NukePayload
DeathWeaponDamageModifier=0.5` — an independent corroboration that `NukePayload`
is the object that *delivers the nuke blast*. The `WEEDGUY` `Primary=NukeCarrier`
line is the well-known "Hack to make MultiMissile work! Don't change data!" — a
weapon referenced only by an SW must appear on some object to be loaded into the
weapon array; it is not a real unit weapon.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_006cdf90` @ `0x6CDF90` (1085 B) | **`SuperWeaponTypeClass::LoadFromINI`** — reads all SW INI keys to the field offsets below | vtable `0x7F3090` slot 25; spans the key-string reads `0x6CE003..0x6CE319` (undefined function gap Ghidra hadn't auto-defined, like `combat.md`'s WarheadType loader) |
| `FUN_006cdb20` @ `0x6CDB20` (331 B) | **`SuperWeaponTypeClass` ctor** — object is **0x100 bytes**, name at +0x24; installs vtable `0x7F3090`; registers into the SW-type array | called by the factory; sets defaults + array insert |
| `FUN_006ce460` @ `0x6CE460` (131 B) | **`SuperWeaponTypeClass::FindOrAllocate(name)`** — searches array (`DAT_00a8d29c`/count `DAT_00a8d2a8`, name at +0x24), else `operator new(0x100)` + ctor | called per `[SuperWeaponTypes]` entry |
| `FUN_006cdae0` @ `0x6CDAE0` (53 B) | **Type-name → index lookup** — iterates the 12-entry name table `0x841560..0x841590`, `strcmpi` each, returns index or −1 | referenced the `MultiMissile` string table |
| `FUN_00668630` @ `0x668630` (949 B) | RulesClass type-list registrar — for each `[SuperWeaponTypes]` id calls `FUN_006ce460` | `"SuperWeaponTypes"` section string xref |
| `FUN_006689f0` @ `0x6689F0` (545 B) | `[SpecialWeapons]` reader (legacy `NukeWarhead`/`NukeProjectile`/`NukeDown` → Rules+0xf8c..) — the OLD hardcoded silo path, distinct from `Type=MultiMissile` | `"NukeWarhead"`/`"NukeDown"` string xrefs |
| `FUN_006ca610` @ `0x6CA610` (389 B) | **`SuperClass` ctor** — object is **0x80 bytes**; +0x28 = `SuperWeaponTypeClass*`, +0x2c = owner `HouseClass*`, +0x30 = creation frame; vtable `0x7F2FE8` | `operator new(0x80)` callee in the HouseClass SW-init loop |
| `FUN_004f53b0` @ `0x4F53B0` (4250 B) | **HouseClass init** — loops all SW types, `operator new(0x80)` + `FUN_006ca610(swtype, house)` per type → **one `SuperClass` per house per SW type** | SW-type-array xref + `operator new(0x80)` |
| `FUN_006cba10` @ `0x6CBA10` (6568 B) | **`SuperClass` fire dispatcher** — `switch(type->Type +0xB4)`; case 0 = MultiMissile: launch up-bullet at `DetonationAltitude 20000` with the SW `WeaponType`; case 1 = IronCurtain; case 2 = LightningStorm; case 7 = PsychicDominator; … | sole caller = `FUN_006cafa0`; `switch` on +0xB4 |
| `FUN_006cafa0` @ `0x6CAFA0` (895 B) | **`SuperClass::Fire`** — ready-gate, calls the dispatcher, then **resets the timer to a full recharge** (`+0x38 = type+0xB0`, `+0x30 = CurrentFrame`) | caller of `FUN_006cba10` |
| `FUN_006caeb0` @ `0x6CAEB0`, `FUN_006cd620` @ `0x6CD620` | `SuperClass` charge (re)arm / CDTimer sync — `TimeLeft(+0x38) = override(+0x24) or type+0xB0`; `StartTime(+0x30) = CurrentFrame`; no RNG | timer-field + frame-counter content |
| `FUN_00489190` @ `0x489190` (4710 B) | **`[NUKE]` warhead CellSpread area damage** (`combat.md` §7 `apply_spread_damage`) — gathers all in-radius damageable objects (IsActive + `Strength>0`), applies virtual `ReceiveDamage` (vtable +0x16c) to each — **buildings included** | reused from `combat.md`; building-enum verified here (not re-derived) |
| `FUN_0075c6af` @ `0x75C6AF` (1568 B) | `WarheadTypeClass::LoadFromINI` — reads the `NukeMaker` flag + the `Verses[11]` array (offset +0xA0, the `combat.md` §1 layout) | `"NukeMaker"` string xref |
| data `0x841560` (12 ptrs) | **SW `Type` enum name table** — MultiMissile..PsychicReveal | pointer-table read |
| data `0x7E3C80…` | **`Action` enum name table** — `Nuke` @ `0x7E3CA0`, plus AmerParaDrop/PsychicDominator/SpyPlane/… | pointer-table read |
| const `0x7F3100` = `900.0f` | **RechargeTime minutes→frames factor** | `FMUL` operand in LoadFromINI |

**`SuperWeaponTypeClass` field map** (0x100-byte object, base = `this`): `+0x24`
Name, `+0x9C` WeaponType(idx), `+0xB0` **RechargeTime (int frames = minutes×900)**,
`+0xB4` **Type(enum 0..11)**, `+0xB8` resolved SidebarImage cameo id, `+0xBC`
Action(enum), `+0xC0` SpecialSound, `+0xC4` StartSound, `+0xC8` AuxBuilding,
`+0xCC` SidebarImage(name string), `+0xE5` UseChargeDrain, `+0xE6` IsPowered,
`+0xE7` DisableableFromShell, `+0xE8` FlashSidebarTabFrames, `+0xEC`
AIDefendAgainst, `+0xED` PreClick, `+0xEE` PostClick, `+0xF0` PreDependent(Type),
`+0xF4` ShowTimer, `+0xF5` ManualControl, `+0xF8` Range(float), `+0xFC`
LineMultiplier(int).

**`SuperClass` field map** (0x80-byte object): `+0x24` per-instance RechargeTime
override (−1 → use type+0xB0), `+0x28` `SuperWeaponTypeClass*`, `+0x2C` owner
`HouseClass*`, `+0x30` **timer StartTime** (frame stamp; −1 = paused/expired),
`+0x34` timer field, `+0x38` **timer TimeLeft** (frames), `+0x6D/+0x6E/+0x6F`
readiness flags, `+0x7C` charge-drain phase.

---

## §1 SW timer / ready / fire lifecycle (`SuperClass`)

**Per-house instancing — CONFIRMED.** HouseClass init (`FUN_004f53b0`) walks the
SW-type array and, for each type, does `operator new(0x80)` then
`FUN_006ca610(swtype, house)`. So there is **one `SuperClass` instance per house
per SW type**, holding the type ptr (+0x28) and owner house (+0x2c). (Whether a
given house is *granted* the SW — sidebar-visible — is a separate ownership gate;
the instance exists regardless. For CB-4 the SW is granted directly, § Deviations.)

**`RechargeTime` → frames — CONFIRMED by disassembly.** `SuperWeaponTypeClass::
LoadFromINI` reads the key as a **double** (minutes) and converts at load:

```asm
0x6CE2DE  CALL 0x00528100            ; ReadDouble("RechargeTime", default 0.0) -> ST0
0x6CE2E3  FCOM qword [0x007E1800]     ; compare to 0.0
0x6CE2EE  JNZ  0x006CE303             ; if 0.0, skip (leave +0xB0 default)
0x6CE2F0  FMUL dword [0x007F3100]     ; ST0 *= 900.0   <== minutes -> frames
0x6CE2F6  CALL 0x007C4D00             ; _ftol : truncate double -> int
0x6CE2FB  MOV  [EBP+0xB0], EAX        ; SuperWeaponTypeClass.RechargeTime (frames)
```

`[0x7F3100] = 900.0f` = 60 s × 15 fps. So **`RechargeTime` is in minutes** and the
stored value is `trunc(minutes × 900)` int frames. `[NukeSpecial] RechargeTime=10`
→ **9000 frames = 600 s = 10 minutes** at the engine's 15 Hz. Truncate-toward-zero
matches the pervasive `_ftol` convention (`combat.md` §3).

**Timer model = CDTimerClass (StartTime/TimeLeft) — CONFIRMED.** `SuperClass`
holds `StartTime(+0x30)` and `TimeLeft(+0x38)`. When running, `StartTime` = the
frame it was (re)started and `TimeLeft` = the total duration; when paused/expired,
`StartTime = −1` and `TimeLeft` holds the frozen remaining. The live remaining is
`(StartTime == −1) ? TimeLeft : TimeLeft − (CurrentFrame − StartTime)` — seen
identically in `FUN_006caeb0`/`FUN_006cd620`/`FUN_006cafa0`. **Ready/charged**
⇔ that remaining `≤ 0`. The countdown duration is `override(+0x24)` if set, else
`type+0xB0` (the RechargeTime frames above).

**Firing resets to a FULL recharge — CONFIRMED.** `SuperClass::Fire`
(`FUN_006cafa0`) gates on the readiness flags, calls the dispatcher
`FUN_006cba10`, then (non-charge-drain branch):

```text
StartTime (+0x30) = CurrentFrame (global 0xA8DCEC)
recharge = type's RechargeTime (+0xB0) if this+0x24 == -1, else this+0x24   // frames
TimeLeft  (+0x38) = recharge                       // full duration again
```

i.e. after firing the SW must wait the *entire* `RechargeTime` again (elapsed
progress → 0). **Not** a partial reset. `IsPowered=true` (type+0xE6) maps the
low-power suspend for later; CB-4 charges unconditionally (§ OoS).

**Confidence: per-house instancing CONFIRMED; `RechargeTime × 900` frame
conversion CONFIRMED (single-instruction); CDTimer ready predicate CONFIRMED;
full-recharge-on-fire CONFIRMED.**

## §2 Nuke fire → effect (`Type=MultiMissile`): NukePayload down-strike, not NukeCarrier up-launch

**The two legs — CONFIRMED.** `SuperClass::Fire` → dispatcher `FUN_006cba10`
`case 0` (MultiMissile): it takes the target cell (read through its first
argument, a cell pointer), computes the
cell-centre leptons (`X*256+128, Y*256+128`), a launch point **20000 leptons up**
(the `[GiantNukeUp] DetonationAltitude=20000`), resolves the SW's `WeaponType`
(type+0x9C = `NukeCarrier`), and creates the up-going bullet
(`FUN_0046af70`/`FUN_0046b180` = BulletClass alloc + fire), then flags the owner
house. The up-bullet's warhead is `NukeMaker`, which the rules `[Warheads]` list
labels **"virtual WH that spawns the down nuke on explosion"** and whose
`[NukeMaker]` section sets only `NukeMaker=yes` — **no `Damage`, no `Verses`** →
it does zero area damage. When it reaches its detonation altitude it spawns the
**descending** missile (`GiantNukeDown`) whose warhead is `NUKE` (`[Warheads]`
`33=NUKE ; new multimissile`), carrying `[NukePayload] Damage=600`.

**The damaging leg reaches the CellSpread falloff — CONFIRMED.** The `NUKE`
warhead detonation at the target cell runs `FUN_00489190` — the same
`apply_spread_damage`/CellSpread routine `combat.md` §7 located
(`CellSpread=10`, `PercentAtMax=.02`, `Verses[]`). Per the brief, the exact
falloff stays CB-2's adjudicated linear `100% → PercentAtMax` approximation;
CB-4 does not re-open `FUN_00489190`'s falloff.

**So: the target-cell damage is `[NukePayload]`/`[NUKE]` (the down-strike), NOT
`WeaponType=NukeCarrier`/`NukeMaker` (the up-launch).** CONFIRMED three ways: the
`[Warheads]` comments, the `[NukeMaker]`-has-no-damage-fields fact, and the binary
dispatcher (up-bullet only launches; damage is the descending `NUKE` leg).

**Building splash — CONFIRMED (buildings take damage).** `FUN_00489190` gathers
in-radius objects via the spatial iterator (`FUN_00412b40`/`FUN_004137a0`),
filtering only on IsActive (+0x24) and `Strength > 0` (+0x6c word `[0x1b]`), builds
a target list, and applies the virtual `ReceiveDamage` (vtable slot +0x16c) to
each — there is **no unit-only / building-exclusion filter**. Buildings have
`Strength`, so the `[NUKE]` splash damages them; `WallAbsoluteDestroyer=yes` +
`Wood=yes` further confirm structure/wall destruction is intended. This matches
game behaviour (a nuke levels a base). **RadLevel=500** lingering radiation is out
of CB-4 scope (mapped for later, § Deviations).

**Confidence: two-leg down-strike attribution CONFIRMED; feeds `apply_spread_damage`
CONFIRMED; building splash CONFIRMED.**

## §3 Target selection + fire trigger

`[NukeSpecial] Action=Nuke` sets the SW's `Action` enum (type+0xBC, from the
`Action` name table — `Nuke` @ `0x7E3CA0`), which arms the map-targeting cursor
for a Nuke strike. On a map click the chosen cell is committed and
`SuperClass::Fire` (`FUN_006cafa0`) is invoked with that cell → the dispatcher
launches at it (`case 0` reads the cell coord directly from the first argument). The screen→
cell pick reuses the existing input path (per brief, only the arm→click→fire
ordering needs confirming — it holds: arm cursor via `Action`, click commits the
cell, `Fire` launches then re-charges).

**`Range=7` / `LineMultiplier=2` are NOT a strike-range cap.** They are stored
(type+0xF8 float / type+0xFC int) but the `case 0` fire path applies **no** range
comparison against the clicked cell — the nuke launches from the source and the
down-strike lands at the clicked cell regardless of distance. They are the
targeting-cursor/aim aid (cosmetic reticle radius / line hint), consistent with
the plan's expectation. **Confidence: arm→click→fire ordering CONFIRMED; Range/
LineMultiplier as cosmetic-not-a-cap PROBABLE** (the storage + the absence of a
Range gate in the dispatcher are confirmed; the cursor-drawing code that *uses*
Range was not separately traced, and does not affect the sim).

## §4 The full recovered SW `Type` enum

**CONFIRMED from the binary name table at `0x841560`** (12 pointers, consumed by
the `FUN_006cdae0` name→index lookup and written to type+0xB4 in LoadFromINI):

| idx | `Type` name | idx | `Type` name |
|----:|-------------|----:|-------------|
| 0 | **MultiMissile** | 6 | AmerParaDrop |
| 1 | IronCurtain | 7 | PsychicDominator |
| 2 | LightningStorm | 8 | SpyPlane |
| 3 | ChronoSphere | 9 | GeneticConverter |
| 4 | ChronoWarp | 10 | ForceShield |
| 5 | ParaDrop | 11 | PsychicReveal |

**`[SuperWeaponTypes]` id → section → `Type` mapping** (from the real rules;
CONFIRMED):

| id | SW section | `Type` (enum idx) |
|---:|------------|-------------------|
| 1 | NukeSpecial | MultiMissile (0) |
| 2 | IronCurtainSpecial | IronCurtain (1) |
| 3 | LightningStormSpecial | LightningStorm (2) |
| 4 | ChronoSphereSpecial | ChronoSphere (3) |
| 5 | ChronoWarpSpecial | ChronoWarp (4) |
| 6 | ParaDropSpecial | ParaDrop (5) |
| 7 | AmericanParaDropSpecial | **AmerParaDrop (6)** |
| 8 | PsychicDominatorSpecial | PsychicDominator (7) |
| 9 | SpyPlaneSpecial | SpyPlane (8) |
| 10 | GeneticConverterSpecial | GeneticConverter (9) |
| 11 | ForceShieldSpecial | ForceShield (10) |
| 12 | PsychicRevealSpecial | PsychicReveal (11) |

Independent corroboration: `SuperClass::Fire` (`FUN_006cafa0`) special-cases
`type->Type == 2` (LightningStorm pre-fire hook `FUN_00539e10`) and `== 7`
(PsychicDominator hook `FUN_0053b130`), and the fire dispatcher `switch` has a
distinct case per enum value. **CB-4 implements ONLY the area-damage
`MultiMissile` (0) `Type`**; the other eleven are documented (deferred-effect
mapped start, `ai-team.md` §3 precedent). **Confidence: enum + id mapping
CONFIRMED.**

## §5 Determinism + byte-identity

- **No RNG on the SW charge or the Nuke detonation critical path — CONFIRMED.**
  Zero calls to the R250 `Randomizer` (`FUN_0065C220`/`FUN_0065C1C0`,
  `combat.md` §5) in ANY function of the SuperClass class region
  (`0x6C9000–0x6CDB20`), the fire dispatcher (`FUN_006cba10`), or `SuperClass::
  Fire` (`FUN_006cafa0`). The timer is pure integer frame arithmetic against the
  global frame counter `DAT_00a8dcec`. `[NukePayload]`/`[GiantNukeDown]` declare
  no scatter fields, and `apply_spread_damage` (`FUN_00489190`) is RNG-free
  (`combat.md` §7). (The vanilla up-missile is a `Vertical` non-scattering bullet;
  the descending strike lands at the committed cell.)
- **Step integration — no existing step reordered.** `SuperClass` is an
  independent per-house instance charged by a per-frame timer and fired by a
  discrete event; it adds a per-house SW-timer pass and a fire-triggered
  detonation without touching the unit/building sim step order. A house that owns
  no granted SW does no meaningful SW work (its `SuperClass` timers exist but are
  inert). **Confidence: no-RNG + independent step CONFIRMED; exact global
  call-site ordering PROBABLE (with no RNG, iteration order is the only
  determinism lever and it is under our control).**

## Confidence table

| Item | Finding | Evidence | Confidence |
|------|---------|----------|:----------:|
| §1 | per-house instancing (one `SuperClass`/house/type, 0x80 B) | `FUN_004f53b0` loop + ctor `FUN_006ca610` | **CONFIRMED** |
| §1 | `RechargeTime` minutes → `×900` int frames (trunc) | `FMUL [0x7F3100]=900.0` + `_ftol`, `[NukeSpecial]=10`→9000 | **CONFIRMED** |
| §1 | CDTimer StartTime/TimeLeft; ready ⇔ remaining ≤ 0 | `FUN_006caeb0`/`FUN_006cd620`/`FUN_006cafa0` | **CONFIRMED** |
| §1 | firing resets to FULL recharge | `FUN_006cafa0` post-dispatch reset (`+0x38 = type+0xB0`) | **CONFIRMED** |
| §2 | damage = `[NukePayload]`/`[NUKE]` down-strike, not `NukeCarrier`/`NukeMaker` up | `[Warheads]` comments + `[NukeMaker]` has no damage + dispatcher `case 0` | **CONFIRMED** |
| §2 | down-strike feeds `apply_spread_damage` (`FUN_00489190`, CellSpread=10) | `combat.md` §7 + `NUKE` warhead | **CONFIRMED** |
| §2 | **buildings take splash** | `FUN_00489190` enumerates all IsActive+Strength>0 objects, virtual ReceiveDamage; `WallAbsoluteDestroyer`/`Wood` | **CONFIRMED** |
| §3 | `Action=Nuke` arm → click commits cell → `Fire` launches | Action enum table + `FUN_006cafa0`→`FUN_006cba10(cell)` | **CONFIRMED** |
| §3 | `Range=7`/`LineMultiplier=2` cosmetic, not a strike cap | stored type+0xF8/+0xFC; no Range gate in `case 0` | **PROBABLE** |
| §4 | 12-entry `Type` enum + `[SuperWeaponTypes]` id→Type map | name table `0x841560` + rules + `FUN_006cafa0` case 2/7 | **CONFIRMED** |
| §5 | no RNG on charge/detonation; independent per-house step | no `FUN_0065C220` in SW region; timer = frame arithmetic | **CONFIRMED** (ordering PROBABLE) |
| art | `NukeIcon` + `NUKEANIM` present on this install | `ra2extract` name probes | **CONFIRMED** |

## Deviations we take (resolves the CB-4 `[RE-GATE: superweapon.md §N]` markers)

- **[PLAN CONFIRM — §1 recharge]** `RechargeTime` is **minutes**; frames =
  `trunc(RechargeTime × 900)`, seconds = `RechargeTime × 60`. Task 2/4
  `recharge_secs`/`resolve_superweapon` use `×60` (not the `[RE-FALLBACK] ×60`
  guess — it is confirmed). `[NukeSpecial]` = 10 min = 9000 frames.
- **[PLAN CONFIRM — §1 fire reset]** Firing resets the timer to a **full**
  recharge (wait the entire `RechargeTime` again). No partial-reset branch for
  the Nuke path.
- **[PLAN CONFIRM — §2 damage attribution]** Target-cell damage = `[NukePayload]`
  `Damage=600` × `[NUKE]` `Verses[armor]` via the CellSpread falloff. `NukeCarrier`/
  `NukeMaker` is the up-launch only (no damage) — do NOT attribute damage to it.
- **[PLAN DIFF — §5.3 building splash, RE-GATE STOP]** Buildings **do** take
  nuke splash in the original. Turn Task 4's `[RE-GATE: superweapon.md §5.3]`
  block into an **added, SW-only building loop** mirroring the unit CellSpread
  falloff (our reused `apply_spread_damage` damages units only). The gate itself
  targets a unit cluster and is building-splash-independent, but the added loop
  makes the Nuke faithfully hit structures in radius.
- **[PLAN DIFF — §4 Type enum]** Use the recovered 12-entry enum with **`AmerParaDrop`
  as a distinct index 6** and **`SpyPlane` at index 8** (the plan folded/omitted
  them). Task 2's `superweapon_type_from_id` maps `[SuperWeaponTypes]` ids
  1..12 → the section's `Type`; CB-4 implements only `MultiMissile`.
- **[PLAN OK — art]** `NukeIcon`/`NUKEANIM` present → Task 5 loads them; no
  placeholder-tile fallback.
- **[DEVIATION — instant detonation]** We collapse the two-leg missile flight
  (`GiantNukeUp` → `DetonationAltitude` → `GiantNukeDown` → 30000 detonate) into
  an instant target-cell detonation. No `NUKEANIM` mushroom-cloud timing, no
  `DetonationAltitude`/`Vertical` bullet AI — the `voxel-render.md` §7 /
  `aircraft.md` §4 / `naval.md` §4 projectile-simplification precedent.
- **[DEVIATION — enemies-only splash]** CB-4's splash hits enemies of the firing
  house only; the real `[NUKE]` hits friendlies too. (Consistent with the CB-2
  `apply_spread_damage` reuse.)
- **[DEVIATION — dropped effects]** `RadLevel=500` lingering radiation
  (`[Radiation]` `RadLevelFactor`, `RadApplicationDelay`), the low-power
  `IsPowered=true` suspend, and `AIDefendAgainst` counter-logic are all mapped
  for later, not implemented in CB-4.
- **[DEVIATION — CellSpread falloff]** Inherit CB-2's adjudicated linear
  `100% → PercentAtMax` CellSpread approximation; CB-4 does not re-open
  `FUN_00489190`'s exact falloff curve.
- **[DEVIATION — SW granted directly]** The SW is granted to the house directly
  (not via a built Nuke Silo `[NUKE]` `NukeSilo=yes` building + prerequisite);
  the `SuperClass` charges unconditionally once granted.
- **[DEVIATION — determinism]** No RNG on the SW critical path in the original;
  our engine keeps it deterministic with a fixed per-house SW-timer iteration
  order (the only determinism lever, under our control).
