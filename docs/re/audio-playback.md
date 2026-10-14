# Audio playback semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`. Hunt scripts, all
off-repo at `D:\ra2-re\scripts\`: `sndplay_dump.py` (sound-INI key/token
string anchors → parse sites), `sndplay_dump2.py` (sound-entry ctor + per-key
setters + Control/Type token handlers + EVA per-entry parser), `sndplay_dump3.py`
(the Control/Type/Priority token *tables* as `{string,value}` pairs), `..4.py`
(SoundList-table globals + play-registry functions), `..5.py` (the VocClass
"play/maintain" methods), `..6.py` (**the attenuation calculator** + its
accessors + constants), `..7.py` (the per-frame voice maintainer + coordinate
projection), `..8.py` (the low-level voice pool + **EVA speech queue**),
`..9.py` (EVA enqueue/pump + the EVA token strings), `..10.py` (a scoped
opcode-operand scan of the audio subsystem that pinned the **variant-select**
and **VShift/FShift** functions by the sound-entry offsets they read). Full
decompiles under `D:\ra2-re\out\snd*.c`; nothing under `D:\ra2-re` is committed.
This is a from-decompile hunt: unlike the BAG decoder in `audio.md`, the
playback layer has no oracle, so every claim below names the function and the
offset/constant/table that anchors it.)

The decoding layer is already done (`docs/re/audio.md`): the entry points it
named — audio bootstrap `FUN_00406b10`, GABA loader `FUN_004011c0`, name→index
bsearch `FUN_004015c0`, format dispatch `FUN_00409c40`, chunk decoder
`0x0040aa70` — are the *sample* pipeline. This document is the *event* layer on
top of it: what happens to a `soundmd.ini`/`evamd.ini` entry when the game makes
noise.

## A methodological note: the token strings are UPPERCASE and case-folded

The first pass searched for the `Control=`/`Type=` value literals as they appear
in `soundmd.ini` (`random`, `interrupt`, `loop`, `ambient`, `screen`, …) and got
**zero hits** for every lowercase token. They are not absent — they are stored
**uppercase** in `{string, bitvalue}` tables and matched through the same
`_stricmp`-shaped case-fold comparator `FUN_007c7b20` that `csf.md`/`audio.md`
already dissected (`AudioIdx::find`'s comparator). So the INI is free-case; the
binary's canonical spelling is `RANDOM`/`INTERRUPT`/`SCREEN`/… This is the same
"case-fold in the comparator, not at insert" mechanism seen twice before, now a
third instance. (`docs/re/workflow.md` §4's "no string xref for some data" trap,
inverted: the data was there, the *casing* was the miss.)

Ares/YRpp absolute addresses were not used as anchors at all (per every prior
hunt on this exact binary they drift); every function below was reached by
string/table/global content-anchoring and its identity confirmed by the struct
offsets it reads or the constants it multiplies by.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_0074fef0` @ `0x74FEF0` (791 b) | soundmd loader — reads `[Defaults]`, then walks `[SoundList]`, `operator_new`-ing each event (ctor `FUN_004063b0`) and parsing it (`FUN_0074f260`) | xref of `"SoundList"`/`"Defaults"` string literals |
| `FUN_0074f260` @ `0x74F260` (1246 b) | per-event parser — one `INIClass` read per key, in a fixed order, each feeding a small setter | xref of `"VShift"`/`"FShift"`/`"MinVolume"`/`"Sounds"` string literals |
| `FUN_004063b0` @ `0x4063B0` (226 b) | sound-event ctor — `operator_new(0x148)`, zero-init, then writes the hard defaults | xref/caller of the parser; `new(0x148)` |
| `FUN_004064a0` @ `0x4064A0` (127 b) | `add_sound` — strips leading `$`/`#`, resolves the name via `FUN_004015c0`, appends the audio.idx index | `"Missing sample %s"` string xref |
| `FUN_00406820`/`FUN_00406870`/`FUN_004067d0` | Control / Type / Priority token→bit(level) lookups over the tables below | callees in the parser, one per tokenized key |
| `FUN_004048b0` @ `0x4048B0` (753 b) | **variant assembly** — builds the play sequence (attack / body / decay) with the RNG | opcode-scan: reads event `+0x134`(count) ×5 and `+0xb4`(sample array) ×8 |
| `FUN_0074f8e0` @ `0x74F8E0` (634 b) | **attenuation calculator** — event + world coord → final volume 0..1 (+ pan out) | callee of the play/maintain path; reads `+0x50`(Range)/`+0x54`(MinVolume)/`+0x14`(Type) |
| `FUN_006d16b0` @ `0x6D16B0` (306 b) | world-coord → screen-pixel (isometric) projection, minus the tactical-view scroll offset | the coordinate source inside `FUN_0074f8e0` |
| `FUN_0074fb60` @ `0x74FB60` (224 b) / `FUN_0074fce0` @ `0x74FCE0` | per-frame voice **maintenance** — re-run attenuation on a live voice, stop it if it went inaudible | xref of the attenuation calc; per-frame update-loop caller |
| `FUN_004055c0` @ `0x4055C0` (1038 b) | **voice start state-machine** — allocates a HW channel, applies FShift (pitch) and VShift (volume) and predelay | opcode-scan: reads `+0x60`/`+0x64`(FShift), `+0x68`(VShift), `+0x5c`(Delay) and calls `RandomRanged` |
| `FUN_004041d0` @ `0x4041D0` (1316 b) | low-level driver tick — per-frame per-event instance count vs **Limit**, channel pick (with the INTERRUPT bit) | opcode-scan (`fshift_hi` false-positive) + reads event `+0x48`(Limit)/`+0x90` |
| `FUN_00405190` @ `0x405190` (323 b) | voice-pool allocator — reuse free slot, else reclaim *finished* voices, else fail | callee of the play path; owns pool list `DAT_0087d100`, count `DAT_0087d20c` |
| `FUN_0065c220` @ `0x65C220` | `Randomizer::RandomRanged(min,max)` (R250, per `combat.md`) — the RNG for variants, VShift, FShift, predelay | callee of `FUN_004048b0`/`FUN_004055c0` |
| `FUN_00751e20`/`FUN_00751bd0` | EVA loader / per-entry parser (`[DialogList]`, `Priority`, `Type`, per-side clip) | `"DialogList"` string xref |
| `FUN_007512a0` @ `0x7512A0` (260 b) | **EVA speak** — the queue front door (flush-on-INTERRUPT, enqueue, pump) | xref of the EVA table `DAT_00b1c40c` |
| `FUN_007513b0`/`FUN_00751580` | EVA enqueue (bucket by type/priority) / pump (start next, one at a time) | callees of EVA speak |

Token/level tables (all `{char* string, DWORD value}`, NUL-terminated list;
read directly out of `.rdata`):

- **Priority** `@0x815018`: `LOWEST=0, LOW=1, NORMAL=2, HIGH=3, CRITICAL=4` (default `2`).
- **Control** `@0x8150C0`: `LOOP=1, RANDOM=2, ALL=4, PREDELAY=8, INTERRUPT=0x10, ATTACK=0x20, DECAY=0x40, AMBIENT=0x80`.
- **Type** `@0x815048`: `VIOLENT=1, MOVEMENT=2, QUIET=4, LOUD=8, GLOBAL=0x10, SCREEN=0x20, LOCAL=0x40, PLAYER=0x80, NOISE_SHY=0x100, GUN_SHY=0x200, UNSHROUD=0x400, SHROUD=0x800, AMBIENT=0x1000, NORMAL=0`.

## The sound-event struct (`VocClass`, `0x148` bytes)

Recovered from the ctor `FUN_004063b0` (hard defaults) and the setters the
parser `FUN_0074f260` calls in fixed key order. Two setters were initially
mislabelled by assumed key order and corrected against the confirmed play-time
getters (`get_type` reads `+0x14`, `get_range` reads `+0x50`): `FUN_004065c0`
stores **Type** at `+0x14` (not Limit); `FUN_004065d0` stores **Limit** at
`+0x48`; `FUN_00406640` stores **Loop** at `+0x4c`.

| Offset | Field | Type | Ctor default | INI `[Defaults]` |
|-------:|-------|------|-------------:|------------------|
| `+0x0c` | loaded/valid flag | int | `1` | — (cleared if a sample is missing) |
| `+0x10` | **Control** bitfield | u32 | `0` | — |
| `+0x14` | **Type** bitfield | u32 | `0x20` (`SCREEN`) | `NORMAL SCREEN UNSHROUD` = `0x420` |
| `+0x40` | **Priority** (0..4) | int | `2` (`NORMAL`) | `NORMAL` |
| `+0x48` | **Limit** | int | `3` | `5` |
| `+0x4c` | Loop count | int | `0` | — |
| `+0x50` | **Range** (cells) | int | `10` | `10` |
| `+0x54` | **MinVolume** (float, 0..1) | float | `0.2` | `50` → `0.5` |
| `+0x58`/`+0x5c` | Delay min/max (ms) | int | `0`/`0` | — |
| `+0x60`/`+0x64` | **FShift** min/max (% pitch) | int | `0`/`0` | — |
| `+0x68` | **VShift** (%, clamped 0..100) | int | `0` | — |
| `+0x6c` | name | char[31] | `strncpy` | — |
| `+0xb4[]` | resolved audio.idx indices | int[32] | — | — |
| `+0x134` | sample **count** (cap `0x20`=32) | int | `0` | — |
| `+0x138` | **Attack** (intro sample count) | int | `0` | — |
| `+0x13c` | **Decay** (outro sample count) | int | `0` | — |

`Volume` (0..100 in the INI, `_ftol`'d to int) is pushed into an embedded 16.16
fixed-point volume fader (`FUN_00406550`→`FUN_00407150`/`FUN_004071c0`), not a
plain field; the number that matters downstream is the 0..100 percentage.

`[Defaults]` is read once into globals (`DAT_00845434` Volume, `..38` MinVolume,
`..40` Range, `..44` Limit, `DAT_0084543c` Type seeded to `0x20`) and used as the
per-key parse default, so the ctor's hard defaults are only visible for a field
the `[Defaults]` block omits. The real block (`MinVolume=50 Range=10 Volume=80
Limit=5 Type=NORMAL SCREEN UNSHROUD Priority=NORMAL`) matches the task pins.

## Findings

### 1. `$` (and `#`) stripping — confirmed, and it is BOTH characters

`add_sound` (`FUN_004064a0`) advances the name pointer past **`$` and `#`**
before the audio.idx lookup:

```text
skip every leading '$' or '#' in the name
idx = AudioIdx::find(stripped name)          // FUN_004015c0
if idx != -1:
    store idx in the entry's sample slot at +0xB4 + count*4
    entry +0x134 = count + 1                 // sample count
else:
    log "Missing sample %s" with the name
    entry +0xC = 0                           // mark invalid, keep going
```

So the leading sigils are cosmetic (they flag "no subtitle"/"no text" in the
editor) and never reach the idx. The engine strips **both** `$` and `#`; the plan
only mentions `$`. A missing sample is logged and the *event* is flagged invalid
(`+0xc = 0`) but parsing continues. The variant array is capped at **32**
(`+0x134 == 0x20` → `add_sound` returns early). **High** — direct read.

### 2. Variant selection is uniform-with-replacement via the R250 RNG

`FUN_004048b0` assembles the actual play sequence from the event's sample array,
which is partitioned into **Attack** (first `+0x138`), **body** (middle), and
**Decay** (last `+0x13c`) samples. The body pick is driven by the Control bits:

```c
// entry = instance[0x24];  count = entry[0x134]; A = entry[0x138]; D = entry[0x13c]
if (A != 0) FUN_0065c220(0, A-1);                     // random Attack (intro) sample
if      (Control & 0x4 /*ALL*/)    for (i=A; i<count-D; ++i) append(i);   // every body sample
else if (Control & 0x2 /*RANDOM*/) FUN_0065c220(A, (count-D)-1);          // ONE uniform body sample
else                               /* the single/next body sample */;
if (D != 0) FUN_0065c220(count-D, count-1);           // random Decay (outro) sample
```

`FUN_0065c220` is `Randomizer::RandomRanged(min,max)` — the **R250** lagged-XOR
generator `combat.md` §5 fully disassembled (rejection-sampled, unbiased). So
`Control=random` means **one body sample chosen uniformly at random, with
replacement** — there is no anti-repeat / shuffle-bag; back-to-back repeats are
possible. `Control=all` plays every body sample in order. The `ATTACK`/`DECAY`
Control tokens reserve the first/last sample as a fixed intro/outro
(`FUN_00406570` sets Attack/Decay to ≥1 when those bits are present). **High** —
the RNG call and the `count/Attack/Decay` arithmetic are read directly. GIAttack
(`random interrupt`, 3 samples, no attack/decay) → `RandomRanged(0,2)`, one of
three uniformly.

### 3. Attenuation — a linear falloff over a Chebyshev screen-pixel distance

`FUN_0074f8e0(event_coord, &pan_out)` is the volume calculator. Distilled:

```c
maxdist = Range_cells * 60;                       // cells -> pixels (a cell is 60 px wide)
FUN_006d16b0(coord, &p);                           // world -> screen pixels, relative to the view
dx = |p.x|;  dy = |p.y|;
if ((Type & 0x40 /*LOCAL*/) == 0) {                // SCREEN/NORMAL: measure from the view centre
    dx -= view_cx*0.5;  if (dx < 0) dx = 0;
    dy -= view_cy*0.5;  if (dy < 0) dy = 0;
}
dy *= 2;                                            // undo the 2:1 isometric vertical squash
if (dx < maxdist && dy < maxdist && maxdist > 0) {
    dist = (dy < dx) ? dx : dy;                     // dist = max(dx, 2*|y|)  -- Chebyshev-ish
    vol  = (maxdist - dist) / maxdist;              // == 1 - dist/(Range*60)   LINEAR
}
if (Type & 0x10 /*GLOBAL*/) vol = max(vol, MinVolume);   // GLOBAL floor
return (vol >= ~0) ? vol : 0;                       // below threshold -> inaudible
```

Every clause is anchored:

- **Curve = LINEAR**, full (`1.0`) at distance 0, zero at `Range` — `vol =
  1 - dist/(Range*60)`. This *confirms* the plan's `[RE-FALLBACK]` linear
  assumption directly, not by fallback.
- **Distance metric = screen pixels, relative to the tactical view.**
  `FUN_006d16b0` is the isometric projection `screenX = (x-y)*30`,
  `screenY = (x+y)*15` per cell (`0x3c`=60 and `0x1e`=30 halved), then it
  subtracts the view scroll offset. Range (in cells) is converted to that pixel
  space by `*60`. The vertical axis is doubled to compensate the 2:1 iso aspect,
  and the combine is `dist = max(dx, 2·|dy|)` — a **Chebyshev/max metric**, not
  the Euclidean `sqrt(Δ²)` that gameplay range checks use (`combat.md` §6). Audio
  and combat use *different* distance metrics; do not share the helper.
- **`Type=GLOBAL` (0x10)** floors the result at `MinVolume` so the event is
  audible everywhere — exactly the `soundmd.ini` comment "*MinVolume = minimum
  volume for GLOBAL Type event (ignored for all other types)*". `MinVolume`
  (`+0x54`) is only read on the GLOBAL branch. **This is the whole point of the
  Type field for volume.**
- **`Type=LOCAL` (0x40)** skips the view-centre subtraction (distance from the
  coordinate as-is), i.e. a sound anchored to a fixed local origin rather than
  the scrolling view.
- **`Type=SHROUD` (0x800)** adds a visibility gate (returns silence when the
  source cell is in the wrong shroud state); `UNSHROUD`/`SHROUD` are mutually
  exclusive and `SCREEN`/`LOCAL` are mutually exclusive (`FUN_00406870` clears
  the sibling bit before OR-ing).

Attenuation is also re-evaluated **every frame** on live voices
(`FUN_0074fb60`/`FUN_0074fce0`): a sound that scrolls off range is *stopped*, so
volume tracks the camera continuously, it is not latched at trigger time.
**High** — the `*60`, the `dy*2`, the `(maxdist-dist)/maxdist`, and the GLOBAL
`max(vol,MinVolume)` are all literal reads.

### 4. VShift / FShift — random pitch and volume jitter, R250 RNG

At voice start (`FUN_004055c0`, the "begin playback" state):

```c
pitch_pct = 100 + FUN_0065c220(entry[0x60] /*FShift_lo*/, entry[0x64] /*FShift_hi*/);
vol_drop  =       FUN_0065c220(0,             entry[0x68] /*VShift*/);
// predelay (only if Control PREDELAY 0x8 / AMBIENT 0x80):
delay_ms  =       FUN_0065c220(entry[0x58] or 33, entry[0x5c] /*Delay_max*/);
```

- **FShift** is a **pitch** (playback-rate) delta in percent: final rate =
  `100 + RandomRanged(FShift_lo, FShift_hi)`. `GIDie FShift=-10 10` → 90..110%
  pitch. Symmetric two-value min/max, so it can shift up or down.
- **VShift** is a **volume-reduction** in percent: `RandomRanged(0, VShift)` is
  *subtracted* (0..VShift). `GIAttack VShift=15` → the clip plays 0..15% quieter
  than its `Volume=65` base. VShift is clamped to 0..100 at parse.
- Both, plus the predelay, pull from `FUN_0065c220` — the **R250 `RandomRanged`**
  again.

**High** — direct reads of `+0x60/+0x64/+0x68` feeding `RandomRanged`.

> **RNG-determinism caveat for the port.** Vanilla draws variant/VShift/FShift
> from the game `Randomizer` (`FUN_0065c220`). The ra2engine plan deliberately
> uses an **audio-local** RNG instead, so `--demo-combat` stays byte-identical
> (audio never perturbs sim RNG state). That divergence is intentional and
> correct — do *not* "fix" it to share the sim Randomizer.

### 5. `Limit` is enforced by voice-stealing, not by trigger-time rejection

There is no "count the instances and refuse the (Limit+1)-th before it starts"
check. Instead the driver tick `FUN_004041d0` reconciles every frame:

```c
// walking the live-voice pool DAT_0087d100; entry = voice[9]:
count = ++entry[0x44];                              // per-event instance counter (frame-stamped at +0x98)
if (entry[0x48] /*Limit*/ != 0 && entry[0x48] < count) {
    victim = entry[0x90];                           // head of this event's instance list
    stop(victim);  victim.state = 4;  victim.flags |= 1;
    entry[0x44]--;                                  // keep the running total at Limit
}
```

So the observable invariant is **at most `Limit` simultaneous instances of the
same event**, achieved by *stopping an existing instance* when a new one pushes
the count over the cap (the new voice was already allocated). This is a genuine
divergence from the plan's "reject the 4th at trigger": vanilla lets it start and
culls to the cap. Which instance dies is the `+0x90`-list head; I could not prove
from statics whether that list is head-oldest or head-newest, so the *cap* is
**High** but the *victim selection* is **Medium**. For a pure mixer both
"reject-newest-at-trigger" and "steal-to-cap" hold the same ≤`Limit` bound and
are audibly near-equivalent.

`ConscriptAttack Limit=3` therefore means never more than three conscript-attack
barks overlapping.

### 6. `interrupt` is a channel-reuse flag, not a priority steal

The `Control=interrupt` bit (`0x10`) is read in the driver's channel-selection
loop (`FUN_004041d0`), where it flips the accept condition for reusing a busy
channel that already carries the same sound:

```c
if ((entry[0x10] & 0x10 /*INTERRUPT*/) == 0) { if (chan.busyflag == 0) break; }  // want a free channel
else                                          { if (chan.busyflag != 0) break; } // reuse the busy one
```

i.e. `interrupt` lets a new trigger **take over an existing playing channel of
the same event** rather than needing a fresh one — behaviourally "steal the same
sound's own voice", which is close to the plan's "interrupt steals oldest
same-key". The exact victim ordering was not fully unwound. **Medium.**

### 7. The SFX voice pool does NOT steal by priority when full

The pool allocator `FUN_00405190` is unambiguous: try a free slot; if none,
**scan and reclaim only *finished* voices** (state flag bit 1 set); if still
none, **return 0 (the new sound is dropped)**. It never evaluates `Priority`
against still-playing voices to evict one.

```c
slot = pop_freelist();
if (slot == 0) {
    for (v in DAT_0087d100) if (v.flags & 1 /*finished*/) reclaim(v);   // finished only
    slot = pop_freelist();
    if (slot == 0) return 0;                                            // give up; no priority steal
}
```

`Priority` (`+0x40`, the 5-level scale) is stored and *does* matter for the EVA
queue (§8) and is carried on the voice, but the SFX pool itself is
first-come-until-a-slot-frees. So the plan's "full voice pool steals
strictly-lower priority" does **not** match vanilla for SFX. **Medium** (the
allocator is read directly; I did not exhaustively prove no *other* caller
pre-empts by priority, but none was found in the play path). Note also the
**5-level** Priority scale (`LOWEST/LOW/NORMAL/HIGH/CRITICAL = 0..4`) vs the
plan's 4-level `MixerSound.priority` (`low=0…critical=3`, no `LOWEST`); vanilla's
`low` is `1`, not `0`.

### 8. EVA speech queue

EVA entries (`evamd.ini [DialogList]`, parser `FUN_00751bd0`) are a separate,
`0x54`-byte class with their own volume class — **no positional attenuation, no
Range**:

| Offset | Field | Values |
|-------:|-------|--------|
| `+0x28` | Volume (float) | default `1.0` (a plain 0..1 gain, not the SFX 0..100+attenuation path) |
| `+0x48` | Priority | `LOW=0, NORMAL=1, IMPORTANT=2, CRITICAL=3` (default `1`) |
| `+0x4c` | Type / category | `STANDARD=0, QUEUE=1, INTERRUPT=2, QUEUED_INTERRUPT=3` |
| `+0x2c`/`+0x35`/`+0x3e` | per-side clip name | 9 bytes each (Yuri / Russian / Allied) |

The speak entry point `FUN_007512a0(index, type=-1, priority=-1)` (defaults pull
the entry's own `+0x4c`/`+0x48`) drives the queue (`FUN_007513b0` enqueue,
`FUN_00751580` pump):

- **One at a time.** `DAT_00b1c42c` holds the single currently-playing speech;
  the pump starts the next only when it is idle, gated by an inter-speech min-gap
  timer (`DAT_00b1c438`).
- **`Type=INTERRUPT` (2) flushes.** An INTERRUPT request stops the current speech
  *and clears the whole queue*, then plays immediately. This is the only way a
  playing EVA is cut off — otherwise a playing line is **never** interrupted.
- **`QUEUE` (1), `QUEUED_INTERRUPT` (3), or `Priority=CRITICAL` (3) always
  enqueue** into a real FIFO list (order preserved via a `seq % 100` sequence
  stamp).
- **`STANDARD`/`INTERRUPT` at non-critical priority are transient**: they occupy
  a single "pending" slot (`DAT_00b1c420`) that is only kept if the queue is
  otherwise empty *and* the newcomer's priority beats the current pending one;
  otherwise the request is dropped (`entry.state = 2`). So low-priority
  fire-and-forget lines (the "unit lost" class) never pile up — only the latest,
  highest-priority one waits, and it yields to anything genuinely queued.
- **De-dupe**: the speak path won't enqueue if the queue tail already carries the
  same category.

`EVA_UnitLost` (`Priority=IMPORTANT`, i.e. `2`; `Allied=ceva064`) is thus an
important-but-not-critical speech: it plays when EVA is idle, waits behind real
queued lines, and is superseded by a later higher-priority pending line — it does
not interrupt whatever EVA is currently saying. **High** on the queue structure
and the INTERRUPT-flush/priority-bucket rules; **Medium** on the exact pending-
slot replacement predicate (read once, not cross-checked).

## Hand-computed vectors (for the mixer tests)

Using the recovered formulas. Distance is in screen pixels; `Range*60` is the
zero point. Assume the source is at the view centre unless noted.

| Event | Base `Volume` | Situation | Computed |
|---|---:|---|---|
| `GIAttack` (`Range` 10, `SCREEN`) | 65 | at view centre (`dist=0`) | `65 * (1 - 0/600) = 65`, then `-RandomRanged(0,15)%` VShift |
| `GIAttack` | 65 | `dist = 300 px` (5 cells) | `65 * (1 - 300/600) = 32.5` → 32 |
| `GIAttack` | 65 | `dist ≥ 600 px` (≥10 cells) | `0` (inaudible → not started) |
| any `GLOBAL` event, `MinVolume=50` | 80 | `dist = 900 px` (off range) | linear gives 0, GLOBAL floor → `80 * 0.5 = 40` |
| `GIDie` (`FShift=-10 10`) | 85 | at centre | `85` volume, pitch `100 + RandomRanged(-10,10)` = 90..110% |

(`GIAttack Sounds=igiat1a/b/c`, `Control=random interrupt`, `VShift=15`,
`Volume=65`; `GIDie` 5 `$`-variants, `Priority=low`, `FShift=-10 10`,
`Volume=85`; `ConscriptAttack Limit=3` — the task pins, all verified against the
extracted `soundmd.ini`.)

## Decisions for ra2engine

Each `[RE-FALLBACK]` item from the plan (Tasks 4–6), resolved:

- **Linear attenuation curve — CONFIRMED, keep it (upgrade Low→High).**
  `vol = 1 - dist/(Range·60)`, full at 0, silent at `Range` (`FUN_0074f8e0`).
  `Mixer::gain_permille`'s linear `1 - dist/range` is correct. Two refinements
  the mixer should adopt if it wants fidelity: (a) the distance is a **Chebyshev
  `max(dx, 2·|dy|)` in screen pixels** measured from the tactical-view centre
  (not a Euclidean cell distance, and not the combat range metric); a scalar
  `dist_cells` fed straight in is a reasonable simplification but is *not* what
  the engine computes. (b) `Range` is cells, converted to pixels by `×60`
  internally — `gain_permille`'s `dist_cells / range` (both in cells) is
  equivalent as long as the caller's `dist_cells` is a true cell distance.

- **`Type=` positional mapping — RESOLVED, and different from the plan's guess.**
  The real rule is **`positional = (Type does NOT contain GLOBAL)`** for *all*
  events, with `GLOBAL` events floored at `MinVolume` (audible everywhere, never
  below MinVolume). Everything non-GLOBAL (`SCREEN`, `NORMAL`, `LOCAL`) is
  distance-attenuated **when the event is started positionally**. The plan's
  original "reports/deaths positional, voice acks non-positional" is in fact
  **correct for the acks** -- see the wave-7 correction below. **Amend Task 6's
  positional flag to `!(type contains "global")`, and add a MinVolume floor on
  the GLOBAL path.**

  > **Correction (wave 7, W7-12-25/26).** Unit voices (VoiceSelect /
  > VoiceMove / VoiceAttack / VoiceEnter / VoiceCapture / VoiceDeploy) are
  > **NOT** positional. `QueueVoice` (0x708210) only stores the voice at
  > techno+0x4F0 (when `AllowVoice` DAT_00821CF2 is set and the owner is the
  > player); `TechnoClass::AI` (0x6F9380 @6F9400) then starts it with
  > `FUN_0074F740(voice, 1.0)`, which never calls the attenuation
  > `FUN_0074F8E0` (the positional start is `FUN_0074F800`): centred, full
  > volume, wherever the unit is. Each unit has ONE voice controller
  > (+0x4DC): a request equal to the playing voice is dropped, a different
  > one waits until the controller is free, only the last request survives.
  > Only the FIRST selected object answers an order (the dispatch 0x4AE660
  > clears AllowVoice after `CurrentObjects[0]`); a click / box select speaks
  > through the object whose `Select()` succeeds (0x6FB4C0, also for a
  > MultiplayPassive owner: civilians); select-same-type (0x7318A0) is silent.
  > Weapon reports, deaths and other unit sounds stay positional.
  > EVA_UnitLost is spoken only for non-`Spawned=` types (0x4D97D0 @4D981C);
  > EVA_UnitReady comes from the factory exit (FUN_004FAF00 @4FB45F,
  > RadarEvent 6), EVA_Canceled only from an abandon click (FUN_006AA390
  > @6AA4C4). The clone keys every per-type sound by the RULES id, not the
  > Image= (W7-12-27/28). `LOCAL`/`SHROUD`/`AMBIENT` refinements can be ignored for the
  current SFX set (none of E1/E2's reports/voices use them).

- **`Limit` reject-vs-steal — the engine STEALS to a cap, not rejects.**
  Observable invariant is ≤ `Limit` simultaneous instances of an event
  (`FUN_004041d0`), enforced by stopping an existing instance when a new one
  exceeds the cap. **Recommendation: keep the plan's cap semantics.** The plan's
  `reject-the-(Limit+1)-th-at-trigger` and vanilla's `start-then-cull-to-Limit`
  both hold the same ≤`Limit` bound and are audibly near-equivalent; reject-at-
  trigger is simpler for a pure mixer and acceptable. If exact parity is wanted
  later, steal an existing same-key voice instead of rejecting. **Note in Task 4
  that this is an approximation, not a bug.**

- **`interrupt` = steal-oldest-same-key — APPROXIMATELY confirmed.**
  `Control=interrupt` (bit `0x10`) is a driver flag to **reuse/steal a busy
  channel already carrying the same event** rather than requiring a free one
  (`FUN_004041d0`). The plan's "interrupt steals the oldest same-key voice" is a
  faithful behavioural model. **Keep it.** (Medium confidence on victim
  ordering.)

- **Priority stealing when the pool is full — does NOT happen for SFX in
  vanilla.** The SFX voice pool reclaims only *finished* voices and otherwise
  **drops the new sound**; it never evicts a lower-priority *playing* voice
  (`FUN_00405190`). **Recommendation: the accurate model is "drop the new sound
  when all `kMaxVoices` are busy" (no priority steal).** The plan's
  "steal strictly-lower priority, else reject" is a *reasonable enhancement* but
  is not vanilla; if the goal is calibration, prefer plain drop-when-full. Either
  way, correct the priority scale to the real **5 levels**
  (`LOWEST=0, LOW=1, NORMAL=2, HIGH=3, CRITICAL=4`) — the plan's
  `priority_from_string` maps `low→0`, but vanilla's `low` is `1` and there is a
  distinct `LOWEST=0` below it. **Amend `priority_from_string`.**

Additional carry-forwards the plan should fold in:

- **`$`-strip must also strip `#`** (`add_sound` strips both); Task 2's parser
  should drop a leading `$` *or* `#`.
- **`Control=random` is uniform-with-replacement** (no anti-repeat) — Task 4's
  seeded `next_rand_() % variants.size()` is exactly right; do not add a
  shuffle-bag.
- **VShift/FShift** (if implemented): `pitch% = 100 + RandomRanged(FShift_lo,
  FShift_hi)`, `volume% -= RandomRanged(0, VShift)`, both from the audio-local
  RNG. Not in the current mixer interface; safe to defer, formula recorded here.
- **EVA is a non-positional, one-at-a-time, priority-bucketed queue** with
  INTERRUPT-flush semantics (§8). Task 4's `play_eva` (single active + FIFO
  queue, full volume) is a faithful subset; the only vanilla nuance worth keeping
  is that a *playing* EVA is never interrupted except by an `INTERRUPT`-category
  line, and low-priority lines don't stack. For the current `EVA_UnitLost`
  (single IMPORTANT line) the plan's simple FIFO is sufficient.

## Confidence table

| Mechanic | Confidence | Why |
|---|:---:|---|
| Sound-event struct layout + `[Defaults]`/ctor defaults | **High** | ctor `new(0x148)` + every setter's offset read directly; play-time getters cross-confirm `+0x14`(Type)/`+0x50`(Range)/`+0x54`(MinVolume) |
| Control / Type / Priority token→bit tables | **High** | `{string,value}` tables read verbatim from `.rdata` |
| `$` and `#` strip | **High** | direct read in `add_sound` |
| Variant selection = uniform R250 `RandomRanged`, Attack/body/Decay split | **High** | RNG call + count/Attack/Decay arithmetic read in `FUN_004048b0` |
| Attenuation curve = linear `1 - dist/(Range·60)` | **High** | literal `(maxdist-dist)/maxdist` in `FUN_0074f8e0` |
| Distance metric = Chebyshev `max(dx,2·dy)` in screen px from view centre | **High** | iso projection `FUN_006d16b0` + `dy*2` + `max` read directly |
| `Type=GLOBAL` floors at MinVolume; MinVolume ignored otherwise | **High** | GLOBAL-only `max(vol,MinVolume)` branch; matches INI comment |
| `Type=LOCAL`/`SHROUD` behaviour | **Medium** | branches identified; only GLOBAL/SCREEN exercised by the current SFX set |
| VShift/FShift formula + RNG source | **High** | `100+RandomRanged(FShift)`, `RandomRanged(0,VShift)` read in `FUN_004055c0` |
| `Limit` = ≤N simultaneous (cap) | **High** | per-frame count-vs-`+0x48` cull in `FUN_004041d0` |
| `Limit` victim = oldest vs newest | **Medium** | culls the `+0x90` list head; list ordering not proven from statics |
| `interrupt` = reuse/steal same-event busy channel | **Medium** | Control `0x10` flips channel-accept condition; victim order not unwound |
| SFX pool: no priority steal (drop-when-full) | **Medium** | allocator reclaims finished-only then fails; no priority eval found in the play path |
| Priority scale = 5 levels | **High** | table `@0x815018` read verbatim |
| EVA queue: one-at-a-time, INTERRUPT-flush, priority buckets, transient pending slot | **High** | `FUN_007512a0`/`FUN_007513b0`/`FUN_00751580` + EVA token strings |
| EVA pending-slot replacement predicate exact form | **Medium** | read once, not cross-checked |

Everything marked Medium is an honest stopping point with the anchor and the
open question recorded, per the fallback rule — not a guess. There are no Low
rows: every hunt target (parse struct, defaults, `$`-strip, variant RNG, Limit,
Priority, interrupt, attenuation curve + metric + Type semantics, VShift/FShift,
EVA queue) was located and read directly in `gamemd.exe`.
