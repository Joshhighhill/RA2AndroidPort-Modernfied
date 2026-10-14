# Combat formulas — reverse-engineering notes

(Method: `docs/re/workflow.md`. Hunt scripts, all off-repo at
`D:\ra2-re\scripts\`: `combat_hunt.py` (Ares/YRpp direct-address probes +
buggy first-pass string anchors), `combat_probe2.py` (disassembly-level
verification of those address probes — this is what caught the drift
described below), `combat_hunt3.py` (content-based "algorithm shape" scan:
every `FLD`/`FMUL`/`FADD`/`FSUB` qword-pointer instruction in `0x400000`–
`0x800000` whose SIB addressing scales an index by 8 — the signature of
indexing a `double` array — 1,128,545 instructions scanned, 13 candidate
functions), `combat_hunt4.py` (fixed string-anchor xref walk + disassembly of
the shared double→int helper), `combat_hunt5.py` (sqrt/RNG confirmation).
Logs and full decompiles under `D:\ra2-re\out\combat*`. Nothing under
`D:\ra2-re` is committed.)

## A methodological finding first: Ares/YRpp addresses do not transfer here

The brief's suggested shortcut — Ares (Ares-Developers/Ares) ships
`DEFINE_HOOK(0x6FF28F, TechnoClass_Fire_BerserkROFMultiplier, ...)`-style
hardcoded absolute addresses for "YR 1.001", the same version string
`preflight.py` verifies (`gamemd.exe` SHA1
`7be4e5508655f8af482ab3ff5965d196e1b6b6c7`) — **does not hold** for this
binary. `combat_probe2.py` disassembled the exact bytes at a dozen Ares/YRpp
hook addresses:

- `0x6FF28F` (Ares: mid-`TechnoClass::Fire`, `EAX`=ROF) is actually inside a
  **voice/sound-report dispatcher** (`FUN_006ff0a0`) in this build — a
  completely unrelated subsystem, not a small offset error.
- The YRpp `Randomizer` addresses (`Randomizer::Random` `0x65C780`,
  `RandomRanged` `0x65C7E0`, ctor `0x65C6D0`) resolve to a **file-read class**
  (`CreateFileA`/`ReadFile`/`GetLastError`) — again, a different subsystem
  entirely.
- The three Ares `Verses_fmul_0/1/2` hook sites (`0x70CEB2`/`0x70CEC7`/
  `0x70CF49`, all documented as `FMUL_VERSES`) land in a **screen-shake /
  explosion-notification** routine (reads `WarheadTypeClass::ShakeXlo/Xhi/
  Ylo/Yhi`-shaped fields, no `Verses` access at all) that Ghidra hadn't even
  auto-defined as a function.
- A few hook addresses (`0x701C97`, `0x7087EB`) *do* land inside the right
  function (confirmed independently below), but the probed byte offset
  itself sits mid-instruction — i.e. small (single-digit-to-low-dozens-of-
  bytes) drift, not the alignment you'd get from a byte-identical binary.

**Conclusion for future RE tasks on this project: never trust an external
tool's hardcoded address for this exact exe, even when the version strings
and SHA1 match everyone's "1.001."** Every function used below was
re-confirmed either by disassembling the exact probed bytes
(`combat_probe2.py`) or, far more reliably, by a pure content/shape search
(`combat_hunt3.py`'s SIB-scale-8 scan) that never referenced an external
address at all. This is `workflow.md` §4's drift warning, just sharper than
expected — Ares' own build is evidently not byte-identical to this install's
`gamemd.exe` either, despite both being nominally "1.001."

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_006fb5d0` @ `0x6FB5D0` (1958 lines decompiled, 3212 bytes) | `TechnoClass::GetFireError(pTarget, nWeaponIndex, ignoreRange)` — returns a `FireError`-shaped byte (`'\x00'..'\t'`) | content scan (SIB `*8` hit); signature/return-domain matches YRpp's `virtual FireError GetFireError(...)` |
| `FUN_00707c40` @ `0x707C40` (867 bytes) | `TechnoClass::CanRetaliateToAttacker(pAttacker, pWH)` | content scan; independently corroborated — `FUN_00700da0` (`ReceiveDamage`, below) calls it directly, passing its own 4th and 3rd arguments in that order, i.e. exactly `(Attacker, pWH)` |
| `FUN_0070c190` @ `0x70C190` (960 bytes) | threat/"expected-damage" weighting helper — `Σ (…) * Verses[targetArmor]`, used both by weapon/target priority and by retaliate-target choice | content scan; called from both `FUN_006f2890` (target priority) and `FUN_00707c40` (retaliation) |
| `FUN_006f2890` @ `0x6F2890` (1254 bytes) | target-priority / auto-acquire threat check (reads `Verses[armor]`, same offset shape as above) | content scan |
| `FUN_00700da0` @ `0x700DA0` (5133 bytes) | `TechnoClass::ReceiveDamage` (the `args_ReceiveDamage`-shaped entry point: `int* pDamage` first arg, then a `WarheadTypeClass*` whose boolean tail fields — `Wall`, `PenetratesBunker`, `Wood`, `Tiberium`, …, at the offsets YRpp's field list predicts — are read one after another) | Ares hook `0x701C97` lands inside it (drifted a few bytes, verified by disassembly); signature shape independently confirms the identification |
| `FUN_006fd210` @ `0x6FD210` (7167 bytes) | `TechnoClass::Fire` — target-lead/ballistic-scatter math, bullet spawn, and the **ROF-cooldown reset** (`this->RearmTimer = GetROF() [/2 if a per-unit flag]; this->LastFireFrame = CurrentFrame`) | Ares hook `0x6FE31C` lands inside it (an `FILD`/`FCOM` pair matches exactly, so this address had *no* drift); ROF-reset shape confirmed by direct read (excerpt below) |
| `FUN_007c4d00` @ `0x7C4D00` (61 bytes) | the shared double→int cast helper — classic MSVC **`_ftol`**: `FNSTCW` current control word, compare to a cached "truncate" word, `FLDCW` to switch if needed, then `FISTP` | found as the ubiquitous callee of every double→int cast seen in every function above; disassembled directly to confirm the `_ftol` shape |
| `FUN_0065c220` @ `0x65C220` (173 bytes) | `Randomizer::RandomRanged(nMin, nMax)` — rejection-sampling wrapper around the core generator | found as the callee `TechnoClass::Fire` (`FUN_006fd210`) uses for ballistic scatter/inaccuracy (`RulesClass::BallisticScatter`-shaped rules field) |
| `FUN_0065c1c0` @ `0x65C1C0` (81 bytes) | `Randomizer`'s core generator step (see PRNG section) | the callee `RandomRanged` calls on every rejection-loop iteration |
| `FUN_004cab60` @ `0x4CAB60` (110 bytes) | fast `sqrt` (exponent bit-hack + 256-entry mantissa correction table) | found as the common callee of every 3D distance computation (`Σ Δ²` in, linear distance out) across all functions above |
| `FUN_00673c80` @ `0x673C80`, `FUN_0066b5f0` @ `0x66B5F0`, `FUN_0066cf70` @ `0x66CF70` | fragments of `RulesClass::LoadFromINI` (the `[General]`/`[CombatDamage]` section readers) | string xref walk (`combat_hunt4.py`, fixed from the buggy first pass) on the `"GuardArea"`, `"MinDamage"`, `"CloseEnough"` ini-key literals |

Two ini-key string hits (`"Verses"` at `0x846BB8`, `"MinimumRange"` at
`0x8483A8`) have exactly one xref each but land in a Ghidra function-boundary
gap (`WarheadTypeClass`/`WeaponTypeClass::LoadFromINI`, presumably one giant
function like `RulesClass::LoadFromINI` above) — not independently
decompiled; the layout facts they'd confirm are instead nailed directly by
content (next section).

## Findings

### 1. `WarheadTypeClass::Verses` layout and indexing — confirmed by content, in four independent functions

`AbstractTypeClass` (the common base of `WarheadTypeClass`/`WeaponTypeClass`)
occupies the first `0x98` bytes of the object in this build. `Verses` is
immediately after `WarheadTypeClass`'s own leading `Deform` field, so in four
independently-located functions the same shape shows up:

- `FUN_00707c40` (CanRetaliateToAttacker): it follows the weapon type's
  `Warhead` pointer (WeaponType `+0xAC`), reads the attacker type's armor int
  (TechnoType `+0x9C`), indexes the warhead's `double` array at
  `warhead + 0xA0 + armor * 8`, and tests whether that value is less than or
  equal to the double constant stored at `0x7F3E34` (behaviourally 0.0).
- `FUN_006fb5d0` (GetFireError): the same address computation, with the
  warhead pointer already loaded. If `Verses[armor]` equals the double
  constant at `0x7E1800` (0.0), the function returns FireError `5` and the
  unit refuses to fire (0% Verses against this armor).

The armor read at TechnoType `+0x9C` is `TechnoTypeClass::Armor` (an `int`/enum read),
used directly as the `* 8` (sizeof(double)) index into an 11-slot array
starting at warhead+0xA0 — i.e. `Verses[0..10]`, exactly YRpp's
`double Verses[0xB]` and the brief's `none..special_2 = 0..10` order. This
was confirmed independently in `FUN_006f2890`, `FUN_006fb5d0`,
`FUN_00707c40`, and `FUN_0070c190` — four separate functions, same base
offset, same scale, same index source. **High confidence.**

### 2. The 0%-Verses special case — confirmed twice, directly

- `GetFireError` (`FUN_006fb5d0`): `Verses[armor] == 0.0` → returns a
  distinct `FireError` code (`'\x05'`) *before* any other targeting checks —
  the unit refuses to even consider firing this weapon at that target.
- `CanRetaliateToAttacker` (`FUN_00707c40`): `Verses[armor] <= 0.0` → returns
  `0` (don't retaliate) — a weapon that provably cannot hurt you doesn't
  provoke a retaliation response.

Both are direct reads of the same `Verses[armor]` cell described above, not
inference. **High confidence.**

### 3. Damage rounding

No single instruction was isolated that literally computes
`(int)(WeaponDamage * Verses[armor])` for the live per-hit number (see
"What wasn't found," below) — `ReceiveDamage` itself (`FUN_00700da0`)
receives `int* pDamage` already resolved and only *adjusts* it further (crate
buffs, difficulty scaling, tiberium-heal exceptions). But the rounding
*convention* used for every double→int conversion in this whole area of the
code is unambiguous and pervasive: it is always routed through
`FUN_007c4d00`, and that function is the textbook MSVC **`_ftol`**:

```c
// FUN_007c4d00 disassembly, in full:
SUB   ESP,0xc
FNSTCW  word ptr [ESP]          ; save current FPU control word
...
CMP   EDX,dword ptr [0x00821d80]; already in "truncate" mode?
JNZ   set_trunc
FISTP qword ptr [EAX]           ; yes -> convert directly
...
set_trunc:
MOV   EDX,dword ptr [0x00821d80]
MOV   dword ptr [ESP],EDX
FLDCW word ptr [ESP]            ; switch FPU rounding-control to truncate
FISTP qword ptr [EAX]           ; convert (now truncates toward zero)
```

`_ftol` (not `_ftol2`) implements C's truncate-toward-zero cast semantics by
temporarily forcing the FPU control word's rounding bits to `11b`
(round-toward-zero) around the `FISTP` — this is **not** round-to-nearest and
**not** the FPU's power-on-default rounding mode. Every double→int cast
observed in `ReceiveDamage`, the threat-eval helper, and the target-priority
code goes through this exact helper. **Verdict: truncation (round toward
zero), high confidence as the general convention; medium for it specifically
being the operator applied to `Damage × Verses`, since that exact multiply
site wasn't isolated.**

`ReceiveDamage`'s difficulty-scaling branch also confirms a **minimum-damage
floor of 1**, directly:

In `FUN_00700da0`'s difficulty-multiplier branch, the difficulty-scaled
damage is truncated to an int with `FUN_007c4d00` (`_ftol`) and written back
through the `int* pDamage` first argument. A little later the stored value is
checked: if it is below 1 it is set to 1, so positive, non-ignored damage can
never round down to 0.

Separately, `RulesClass::LoadFromINI` (`FUN_0066b5f0`) reads configurable
`int MinDamage` / `int MaxDamage` fields from the **`[CombatDamage]`** ini
section (confirmed by the section-name string `PTR_s_CombatDamage_007efc84`
threaded through every read call in that stretch of the function, alongside
`Crush`, `ExpSpread`, `FireSupress`, `HomingScatter`, `BridgeStrength`,
`C4Delay`, `TiberiumExplosive` — all of which match rulesmd.ini's real
`[CombatDamage]` section key set), stored at Rules-instance offsets `0x16c4`
(`MinDamage`) / `0x16c8` (`MaxDamage`). Their *existence and ini-configurability*
is confirmed; the call site that actually clamps a live hit's damage against
these two globals was not isolated in the time available.

**What wasn't found:** the exact instruction(s) computing
`int scaledDamage = (int)((double)weapon->Damage * warhead->Verses[armor])`
for a live hit. The content scan (`combat_hunt3.py`) covered every `FLD`/
`FMUL` whose *own* addressing mode scales an index by 8 across `0x400000`–
`0x800000`; if the compiler instead pre-computed `armor*8` in a separate
`SHL`/`LEA` and fed a plain `[base+reg]` form to the FPU op, that scan would
miss it. This is the one genuine gap in the mechanic; the rounding
convention and the floor-of-1 above are the closest confirmed proxies.

### 4. Verses indexing / armor enum order

Already shown in §1: `TechnoTypeClass::Armor` (an int, `techtype+0x9c` in
this build) is used directly, unscaled, as the `Verses[]` index — no
remapping table, no `Verses[armor-1]` off-by-one, no separate lookup. This
directly confirms the brief's assumed `none..special_2 = 0..10` order
(`CCINIClass.h`'s `INI_READ(ArmorType, ...)` enum-name-list reader — not
independently decompiled in this build, but the *runtime consumer* of the
resulting int is, which is the stronger of the two claims). **High
confidence.**

### 5. ROF cooldown + randomness verdict

`TechnoClass::Fire` (`FUN_006fd210`) resets the weapon cooldown as follows
(our summary; dword indices into the `this` object are given in brackets, so
`[0xEE]` means byte offset `0xEE*4`):

```text
this[0xEE] += 1                         // burst-index advance
rof = this->vtable[0x318/4](weapon)     // virtual GetROF(weapon)
if (byte at this[0xA6] is nonzero)      // a per-unit status flag
    rof = rof / 2                       // halve the cooldown (2x rate of fire)
this[0xBE] = rof
this[0xBB] = CurrentFrame (global 0xA8DCEC)   // LastFireFrame
this[0xBC] = a value from Fire's local stack frame (not identified)
this[0xBD] = rof                        // RearmTimer/cooldown := ROF (or ROF/2)
```

**No call into the RNG appears anywhere near this reset** — the cooldown
value is the virtual `GetROF()` result, optionally halved by a status flag
(this line/offset is *exactly* what Ares patches with their
`TechnoClass_Fire_BerserkROFMultiplier` hook in their own build — they
replace this same fixed "halve" behavior with a configurable multiplier,
which is why their hook exists at all: there is nothing to randomize here in
vanilla). The cooldown *check* (`GetFireError`, `FUN_006fb5d0`) is equally
deterministic frame arithmetic:

```text
remaining = this[0xBD]                  // stored cooldown duration
if this[0xBB] != -1:                    // a LastFireFrame has been recorded
    elapsed = CurrentFrame (0xA8DCEC) - this[0xBB]
    if elapsed >= remaining: ready to fire (skip the cooldown error)
    remaining = remaining - elapsed
if remaining != 0: return FireError 3   // still cooling down
```

**Verdict: no ROF jitter/randomness in vanilla YR 1.001. High confidence** —
this is a direct read of the reset and the check, not inference.

The RNG (`Randomizer`) itself **is** used elsewhere in the same function,
for ballistic scatter/inaccuracy (`FUN_0065c220(min, max)` seeded from a
`RulesClass` scatter field). Its core generator (`FUN_0065c1c0`) is the
callee `RandomRanged`'s rejection loop calls, and it matches YRpp's
`Randomizer` layout field-for-field:

```c
// FUN_0065c1c0 — operates on the Randomizer object ("this"), byte layout:
//   +0: bool unknown_00 (disabled flag)   +4: int Next1   +8: int Next2
//   +0xc + i*4: DWORD Table[i]   (0xFA = 250 entries)
Table[Next1] ^= Table[Next2];
result = Table[Next1];
Next1++;  Next2++;
if (Next1 > 249) Next1 = 0;
if (Next2 > 249) Next2 = 0;
return result;
```

This is the classic **R250** (Kirkpatrick–Stoll) lagged-Fibonacci/XOR
shift-register generator — a 250-word state table, two rolling cursors, XOR
combine, advance-and-wrap both. It is a "well-known" PRNG family exactly as
the brief anticipated, though the specific family is R250 (XOR-feedback
shift register) rather than a multiply-add LCG; the two are often confused
informally. `RandomRanged(min,max)` (`FUN_0065c220`) wraps this with
proper power-of-two-mask rejection sampling to stay unbiased over
`[min,max]`. The fixed lag between `Next1`/`Next2` (canonically 103 for
R250) is set at construction/seeding time, which wasn't traced. **High
confidence on the generator family and step function; the initial lag/seed
schedule is unconfirmed.**

### 6. Range metric

Every distance check across every function examined above funnels through
the same pattern: accumulate `Δx² + Δy² + Δz²` (three squared lepton-coordinate
deltas, full 3D — Z included, not just the 2D ground plane) and pass the sum
into `FUN_004cab60`:

```text
dist = int_truncate( fast_sqrt(dy*dy + dz*dz + dx*dx) )   // FUN_004cab60, then _ftol FUN_007c4d00
```

(The squares are computed in double precision, and the sqrt result is
truncated to int.)

`FUN_004cab60` is a fast `sqrt`: it normalizes the exponent, splits the
mantissa's top bits as an index into a 256-entry correction table at
`DAT_0086403c`, and reconstructs a `float` result — a classic "bit-hack +
table" square-root approximation, not a lookup for something else (the input
is always a sum of squares; the output is always compared against/subtracted
from other linear coordinate deltas and range constants). **Verdict:
Euclidean (3D) distance, not Chebyshev/octile grid distance. High
confidence** — this is a direct read, corroborated across four separate
call sites (threat-eval, `GetFireError`'s implicit range gate via the
in-range checks upstream of it, `TechnoClass::Fire`'s target-lead math, and
the explosion/shake-effect function).

No separate +/- slack constant was isolated around a range comparison in the
time available (would need the actual `IsCloseEnoughToAttack`/`InRange`
virtual bodies, which weren't independently anchored) — ModEnc documents a
small tolerance is generally not added for weapon range (unlike `CloseEnough`
for movement/guard, which is a separate, explicitly ini-configurable
constant confirmed to exist in §7). **Low confidence specifically on
range-check slack; not located.**

### 7. Guard / retaliate

**Retaliation target choice — confirmed directly.** `CanRetaliateToAttacker`
(`FUN_00707c40`) doesn't just gate on Verses (§2); when the unit already has
an `ArchiveTarget` (dword index `0xAD` of `this`, matching YRpp's "set when told to
guard a unit" field) it compares the *threat* of the current archive target
against the new attacker using the same helper (`FUN_0070c190`) the AI uses
for target-priority scoring, and only switches to retaliate against the new
attacker if it scores higher:

```text
threatCurrent  = ThreatScore(currentArchiveTarget, &global_0xB0DA00)   // FUN_0070c190
threatAttacker = ThreatScore(newAttacker,          &global_0xB0DA00)
if threatAttacker < threatCurrent:
    return false    // current target is scarier: keep at it, don't retaliate
```

`FUN_0070c190` itself folds `Verses[targetArmor]` into its score (§1),
confirming the "damage-per-frame-ish, Verses-weighted" shape Ares'
`TechnoClassExt::EvalVersesAgainst` (their own hand-written analog for AI
weapon-priority) documents independently. **Medium-High confidence** — the
comparison and its inputs are directly read; the exact scoring formula
(`FUN_0070c190`'s full body) involves several unidentified `DAT_`/vtable
terms not fully unwound.

**Idle-unit guard-acquire radius and pursuit/stray distance — not
independently confirmed; ModEnc/YRpp fallback, Low confidence.**
`RulesClass::LoadFromINI` (`FUN_00673c80`) confirms `GuardArea` is a real,
ini-configurable `[General]` int field (offset `Rules+0x1440`), and a
different fragment of the same reader confirms `CloseEnough`, `Stray`,
`RelaxedStray`, `GuardModeStray` all exist as sibling `[General]` int fields
(their string literals each have exactly one xref, into the giant
`RulesClass::LoadFromINI`/reader region) — so the *knobs* are real and
present in this build. `TechnoClass::GetGuardRange(int control)` (declared
virtual in `TechnoClass.h`, with a documented 0/1/2 control-parameter
behavior: 0 = use `GuardRange`/weapon range as-is, 1 = double it
capped at 4096 leptons, 2 = double it clamped to `[1792, 4096]`) was not
independently located or decompiled — no anchor was found for a virtual
method with no distinguishing string/constant in the search budget available.
Likewise, no call site applying `Stray`/`RelaxedStray`/`GuardModeStray` to an
actual pursuit-vs-return decision was isolated. Per the brief's fallback
rule, **the guard-acquire-radius formula and the exact stray distance/trigger
condition default to ModEnc's documented behavior (weapon range for
opportunistic fire while idle; a wider explicit scan radius under an
explicit Guard order; a stray-distance leash that returns the unit to its
guard post) and are marked Low confidence** — the search is documented above
so a future task can pick up exactly where this one stopped (next anchor to
try: locate the vtable slot for `GetGuardRange` via the `TechnoClass`
vtable layout, or search for xrefs to the confirmed `Rules+0x1440`
(`GuardArea`) global from outside the ini reader).

## Hand-computed canonical vectors

Using the recovered formula (`Damage × Verses[armor]`, truncated toward
zero, per §3) and the brief's ground truth:

| Attacker weapon | Damage | Target armor | Verses | Raw product | Rounding | Result |
|---|---:|---|---:|---:|---|---:|
| GI's `M60` (`[E1]`, Warhead `SA`) | 15 | `flak` (`[E2]`'s armor) | 80% | 12.0 | exact, no rounding needed | **12** |
| Conscript's `M1Carbine` (`[E2]`, Warhead `SA`) | 15 | `none` (`[E1]`'s armor) | 100% | 15.0 | exact | **15** |
| (illustrative, non-integer case) 15-damage `SA` hit | 15 | `wood` | 75% | 11.25 | **truncated toward zero** (§3) | **11** |

(`[SA]` Verses order used above, per the brief:
`none=100%, flak=80%, plate=80%, light=50%, medium=25%, heavy=25%, wood=75%,
steel=50%, concrete=25%, special_1=100%, special_2=100%` — armor order
`none..special_2 = 0..10` per §1/§4.)

## Decisions

- **Damage rounding: CORRECT to truncate-toward-zero (round toward zero),
  not round-to-nearest or ceiling.** Confirmed as the pervasive convention
  via the disassembled `_ftol`-shaped helper (`FUN_007c4d00`); the specific
  `Damage × Verses` multiply instruction itself wasn't isolated (see §3
  "What wasn't found"), so treat this as Medium-High rather than a
  single-instruction-verified High.
- **Verses indexing: CONFIRM** `none..special_2 = 0..10`, a flat 11-entry
  `double[]` on `WarheadTypeClass` indexed directly (unscaled, unshifted) by
  `TechnoTypeClass::Armor`. High confidence, four independent sightings.
- **ROF jitter: NO — CONFIRM no randomness.** Cooldown reset is
  `RearmTimer := GetROF()` (optionally halved by a status flag), directly
  read with zero RNG calls nearby. The game's RNG (confirmed as an R250
  lagged-Fibonacci/XOR generator, `FUN_0065c1c0`/`FUN_0065c220`) is real and
  used elsewhere (ballistic scatter) but not here. High confidence.
- **Range metric: CONFIRM Euclidean, 3D** (`sqrt(Δx²+Δy²+Δz²)` via a fast-sqrt
  bit-hack routine, `FUN_004cab60`), not Chebyshev/octile. High confidence.
  No range-check slack constant located — Low confidence / not implemented
  on that specific sub-point.
- **Guard radius: Low confidence, ModEnc/YRpp fallback** — the ini knobs
  (`GuardArea`, `CloseEnough`, `Stray`, `RelaxedStray`, `GuardModeStray`) are
  confirmed present and ini-configurable in this build, but the consuming
  formula (`GetGuardRange`'s virtual body) was not located.
- **Stray distance: Low confidence, ModEnc fallback** — same ini-field
  confirmation as above; no pursuit/return call site was isolated.
- **Retaliation target choice: CONFIRM** — threat-comparison
  (`Verses`-weighted) between the current archive target and a new attacker,
  not "always switch to whoever hit you last." Medium-High confidence.

## Confidence table

| Mechanic | Confidence | Why |
|---|:---:|---|
| Verses array layout/indexing (`this+0xA0`, 11 doubles, armor unscaled index) | **High** | four independent decompiled functions read the identical shape |
| 0%-Verses special case (refuse-to-fire, decline-to-retaliate) | **High** | two direct reads, not inference |
| Damage rounding convention (truncate toward zero) | **Medium-High** | the shared `_ftol` helper is disassembled and unambiguous; the specific `Damage×Verses` call site wasn't isolated |
| Minimum-damage floor of 1 | **High** | directly read in `ReceiveDamage`'s difficulty-scaling branch |
| `MinDamage`/`MaxDamage` existence as ini-configurable `[CombatDamage]` globals | **High** | directly read in `RulesClass::LoadFromINI`, matches real rulesmd.ini section key set |
| `MinDamage`/`MaxDamage` clamp *application* site | **Low** | fields confirmed to exist; the consuming clamp wasn't located |
| ROF cooldown reset (= `GetROF()`, no jitter) | **High** | directly read, both the reset and the check |
| RNG algorithm identity (R250 XOR/lagged-Fibonacci) | **High** | core step function fully disassembled, matches YRpp's `Randomizer{Next1,Next2,Table[0xFA]}` layout exactly |
| RNG seed/lag schedule | **Low** | not traced (construction-time code not examined) |
| Range metric (Euclidean 3D via fast-sqrt) | **High** | direct read, four independent call sites |
| Range-check slack constant | **Low** | not located |
| Retaliation target choice (threat comparison, not "last attacker") | **Medium-High** | direct read of the comparison; the full threat-scoring formula has unidentified terms |
| Guard acquire radius (`GetGuardRange` formula) | **Low** | ModEnc/YRpp fallback; ini knobs confirmed present, consuming virtual method not located |
| Pursuit/stray distance | **Low** | ModEnc/YRpp fallback; ini knobs confirmed present, consuming logic not located |

Everything marked Low above is an honest, documented stopping point (per the
brief's fallback rule), not a guess dressed up as a finding — the anchors
tried and the reason each one didn't pan out are recorded in the relevant
section so a follow-up task doesn't have to re-discover the same dead ends.

## 8. Area damage, falloff and friendly fire (wave 5, agent B -- R-060)

Private Ghidra copy `D:\ra2-re\w5-combat` (scripts `dec.py`/`decf.py`/
`dis.py`/`refs.py`, outputs `D:\ra2-re\w5-combat-out\{dmg.c,objrd.c,md,
ring,ringinit}`).

**Modify_Damage `FUN_00489090` (disassembled in full, `md/dis_489090.txt`)**
`(damage=ECX, warhead=EDX, armor, distance)`:

```
if (!damage || !warhead) return 0;
if (damage < 0) return distance < 8 ? damage : 0;          // healing
float fd = damage, atmax = fd * WH.PercentAtMax;            // [wh+0x12C]
int spread = ftol(WH.CellSpread * 256.0f);                  // [wh+0x124], [0x7E1224]=256.0
if (atmax != fd && spread != 0)
    damage = ftol((fd - atmax) * (spread - distance) / spread + atmax);   // LINEAR
damage = max(damage, 0);
damage = ftol(damage * WH.Verses[armor]);                   // double [wh+0xA0+8*armor]
return min(damage, Rules.MaxDamage);                        // [Rules+0x16C8] = 10000
```

The falloff is linear (the old `[RE-FALLBACK]` guess was right in shape); the
distance is **not** clamped to the radius, and there is **no** floor of 1 here.
`ObjectClass::ReceiveDamage` (`FUN_005F4BA0`) calls it and then floors only
**buildings** at 1. `TechnoClass::ReceiveDamage` (`0x700DA0`) floors the RAW
damage at 1 after the house/veterancy armour multipliers (before
Modify_Damage), and returns 0 when the warhead is `AffectsAllies=no`
(`[wh+0x179]`) and the firer's house is allied with the victim's. No stock
warhead sets `AffectsAllies=no` (the only rulesmd hit is a comment), so splash
hurts own and allied units in practice.

**DamageArea `FUN_00489190`** `(coord, damage, source, warhead, ..., house)`:
- radius (a local) `= ftol(CellSpread*256)`; cells scanned = the first
  `DAT_007EC3D0[ftol(CellSpread)]` entries of the offset table
  `DAT_00ABC408` (counts `1,9,21,37,61,89,121,161,205,253,309,369`). The
  offsets are written by the static initializer `0x561540` (unrolled
  `CellStruct` stores); they were recovered by emulating its stores
  (`ringemu2.py`). They are NOT a simple `dx²+dy² <= f(r)` circle (ring 8
  and ring 9 overlap at d²=65).
- every object in those cells' content chains (both layers by bridge flag),
  plus -- when the impact is above the ground -- the airborne objects within
  the radius; the source object is skipped (unless its type flag `+0xCA0` or
  the warhead is the nuke).
- distance: 3-D `sqrt(dx²+dy²+dz²)` to the object's coords; for a
  **building**, distance 0 in the impact cell (or `dist-208` if the impact is
  more than 2 levels above it), and from the cell CENTRE in every other cell.
  A building sits in the content chain of every foundation cell
  (`AddContent FUN_0047E7B0` appends buildings at the chain end) and the
  victim vector is not de-duplicated, so **a building takes one hit per
  foundation cell inside the ring set**.
- an aircraft in the air has its distance halved before ReceiveDamage.
- each victim with `Strength>0`, on the map, `distance <= radius` gets
  `ReceiveDamage(&damage, distance, warhead, source, 0, 0, house)`.

Every bullet detonation (`BulletClass::Detonate FUN_004690A0`) calls
DamageArea, so the clone now routes instant hits (detonating at the victim's
coordinates), projectile detonations and base-defense shots through one
`damage_area` (`engine/combat.cpp`). A CellSpread 0 warhead still hits its
target (distance 0).

**WarheadType default CellSpread** for the many warheads without the key (SA,
Super, Electric...): `[RE-FALLBACK: ModEnc default 0]` -- the constructor was
not decompiled. With 0 the radius is 0: the aimed victim (distance 0) only.

## 9. Weapon range in 3-D, elevation bonus, cliff trace (wave 5 -- R-063 / R-057)

`TechnoClass` in-range test `FUN_006F6750` (callers in the fire / approach
code): `range = Weapon.Range` (leptons, `-0x200` = infinite); a target in the
air adds the type's AirRangeBonus; garrison/transport bonuses
(`Rules+0xF48/+0xF54/+0xF5C`); a `SubjectToElevation` projectile adds the
[ElevationModel] bonus `FUN_006F6490`: `diff` = shooter level - target level
(0 if negative), `n = diff / ElevationIncrement(4)`, bonus = `min(n *
ElevationIncrementBonus(2), ElevationBonusCap(2))` cells, returned as
`sqrt((ftol(bonus)*256)^2 + (104*diff)^2)` leptons. `MinimumRange` (`+0xB8`)
refuses closer targets. A **building** target widens the range by
`(foundation w + h) * 64` and is measured to its centre. The distance is 3-D
(`dx²+dy²+dz²`, ftol) except for a flying shooter (`TechnoClass+0x29B`
branch, 2-D). A passing check is followed by the line trace `FUN_004CC230 ->
FUN_004CC020`: `n = max(|dcx|,|dcy|)` steps from the shooter toward the
target; with `SubjectToCliffs` (`BulletType+0x296`) a step into a cell more
than 3 levels above the previous one blocks the shot (`FUN_004CC280`); with
`SubjectToWalls` (`+0x298`) a wall overlay cell not below the shooter blocks
it (unless [WallModel] AlliedWallTransparency and allied). Cannon (tank guns)
and InvisibleLow (M60, MP5, AKM, ...) are SubjectToCliffs+Elevation(+Walls).

Clone: `weapon_in_range` / `weapon_in_range_from` (`engine/combat.cpp`) for
unit, building and ground-cell targets and the hold-at-range pass; elevation
and cliff parts armed by `w5_arm_combat`. Walls are not traced
`[RE-FALLBACK: no wall overlay state on the combat path]`; AirRangeBonus and
the garrison bonuses are not modelled here (garrisons have their own range).

## 10. Flak scatter, re-aim snap, sight from height (wave 5 -- R-057 and follow-ups)

- **[FlakProj] (FlakScatter + Inviso)** -- `projectile.md` §5.3 applied on the
  clone's instant path: `r = Random(0, 2*BallisticScatter) * dist /
  WeaponRange`, random angle, added to the impact point before DamageArea
  (`engine/combat.cpp` try_fire; real game only, the projectile layer on).
  `MinimumRange` of the primary was already honoured (`an.t.min_range`).
- **Close-enough re-aim** (`BulletClass::Update` tail 0x467D60..0x467E3E): a
  non-Inaccurate, non-Airburst shell detonating in its target's cell snaps onto
  the target when `dist/3 <= max(2*|V|, 128.0 [DAT_007E1808])`. This matters
  with sub-cell infantry: the arrival test (shell enters the target's cell
  below 208 leptons) fires on the cell edge, up to ~190 leptons from a soldier
  on a spot; without the re-aim an AP .3 (76-lepton) shell would miss.
- **Sight from height** (`FUN_0070A240`, the reveal update): the unit's
  `Z / [General] LeptonsPerSightIncrease(2000) * 10` is cached at `+0x108` and
  only decides when the reveal is re-run; the reveal radius itself is
  `ftol(Sight * veterancy multiplier)`. No radius bonus from height was found,
  so the clone adds none.
- **Walls** (SubjectToWalls, [WallModel]): not traced -- the combat path has no
  wall overlay state `[RE-FALLBACK]`.

## 11. Missions, retaliation, threat and crush (wave 7 -- w7fix-combat)

Source: `docs/gap/wave7/07-shared-combat.md` (Ghidra slices A/B/C on YR 1.001).
Corrections to the sections above:

- **§7 guard leash is wrong.** `kGuardStray` (anchor leash + walk back) is not
  the original. Plain Guard has no anchor: a scan-acquired target out of
  weapon range is dropped at each targeting tick (GetFireError RANGE,
  0x6F9B8A..0x6F9C1E) and the unit rescans from its current cell. Area Guard
  (FUN_004D41A0) keeps a home cell and drops a target that strays
  `(FUN_007072E0(2) >> 8) + 6` cells (13..22) from it, then rescans or walks
  home. The clone keeps the old leash only for an unarmed (synthetic) World.
- **Retaliation** is CanRetaliateToAttacker 0x707C40 (mission Retaliate flag,
  not allied, armed, PlayerReturnFire=no -> a human unit only in Guard / Area
  Guard / Patrol, a computer unit in any mission, threat comparison
  0x70C190, Verses > 1%) and Override_Mission(Attack) 0x4D8E60, which resumes
  the old order afterwards.
- **Target choice** is the maximum of FUN_0070C190 (Greatest_Threat
  0x6F8320 / Evaluate_Object 0x6F71D0), not the nearest; Verses <= 2% is never
  passively acquired. Scans run every NormalTargetingDelay (27) /
  GuardAreaTargetingDelay (36) + Random(0,2) frames.
- **Crush** refuses allies (IsCrushable 0x5F64C0 -> IsAlliedWith 0x4F98B0,
  including a neutral house the map lists) and Iron-Curtained victims; the
  crusher warns the cell (Crush(cell,1) 0x7404B0 -> Incoming 0x481580) and the
  squash runs on arrival (Per_Cell_Process 0x739AC0).
- **ROF jitter** is applied before VeteranROF:
  `ftol(ftol(ROF * houseROF + Random(0,2)) * VeteranROF)` (0x6FC55E..0x6FC60C).
