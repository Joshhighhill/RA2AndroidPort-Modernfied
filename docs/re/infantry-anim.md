# Infantry animation sequences & facing — original-game calibration

Recovered 2026-07-10, after live play exposed two fidelity bugs in the Combat
Phase's **empirically eyeballed** animation table (engine/world.cpp Task 7).
This doc replaces that guesswork as the calibration authority for infantry
frame selection. Method: original game DATA (artmd.ini extracted from the
install's `ra2md.mix/localmd.mix` via `ra2extract --raw`) + structure
cross-check against the YRpp reference headers (read off-repo at
`D:\ra2-re\reference\YRpp`; prose conclusions only, no GPL code copied) +
direct inspection of the SHP art itself where the data file is the wrong
authority (facing block order).

## 1. Sequence data model (HIGH)

Infantry art sections name a sequence section: `[GI] Sequence=GISequence`,
`[CONS] Sequence=ConSequence` (artmd.ini). Each sequence entry is

```
Name=StartFrame,FrameCount,FacingMultiplier[,Facing/Sound extras]
```

confirmed structurally by YRpp `SubSequenceStruct { StartFrame; CountFrames;
FacingMultiplier; Facing; ...sounds }` (InfantryTypeClass.h). Frame selection:

```
frame = StartFrame + facing_index * FacingMultiplier + phase   (phase < FrameCount)
```

`FacingMultiplier=0` ⇒ facing-independent (deaths, idles). Extra trailing
fields exist (e.g. `Idle1=56,15,0,S` — the `S` labels the fixed facing that
anim renders at) and MUST be tolerated by a parser reading the 3 ints.

## 2. Ground truth for this slice's units (HIGH — read from the real artmd.ini)

`[GISequence]` and `[ConSequence]` are IDENTICAL for every entry we consume:

| Entry  | Value      | Frames covered                      |
|--------|-----------|--------------------------------------|
| Ready  | `0,1,1`   | 0..7 (1 frame × 8 facings)           |
| Walk   | `8,6,6`   | 8..55 (6 × 8)                        |
| FireUp | `164,6,6` | 164..211 (6 × 8)                     |
| Die1   | `134,15,0`| 134..148 (15, no facing)             |
| Die2   | `149,15,0`| 149..163 (15, no facing)             |
| Prone  | `86,1,6` / Crawl `86,6,6` / Idle2 `71,15,0,E` | (context for §3) |

## 3. The two Task-7 defects this data exposes

- **CONS fire was hardcoded 158..163** — those are the LAST SIX FRAMES OF
  DIE2 (149..163). Every Conscript shot played half a death animation
  ("soldier keeps falling before dying" — user bug report, confirmed).
  Real FireUp is 164, same as GI.
- **Die was hardcoded 80..87 for both units** — frames 80..85 are the tail
  of Idle2 (71..85) and 86..87 the head of the Prone/Crawl block. Real
  deaths are Die1=134..148 (15 frames). At the game's 15 Hz anim rate that
  is exactly 1.0 s — independently validating combat.cpp's kDieAnimSecs=1.0.
- GI FireUp=164..169 happened to be correct (luck, not method).

Lesson recorded as standing policy: original data first, no eyeball-derived
frame tables.

## 4. Facing block order (HIGH — from the art itself)

The art, not the ini, defines which block index faces which way. Dumped the
first frame of each of GI FireUp's 8 blocks (164+6k, k=0..7) and read the
muzzle-flash direction (unambiguous in all 8; see scratch contact sheet,
reproducible via `ra2extract ... <frame>`):

| k | frame | flash points | screen dir |
|---|-------|--------------|------------|
| 0 | 164   | straight up (back view)   | N  |
| 1 | 170   | up-left      | NW |
| 2 | 176   | left         | W  |
| 3 | 182   | down-left    | SW |
| 4 | 188   | down (frontal view)       | S  |
| 5 | 194   | down-right   | SE |
| 6 | 200   | right        | E  |
| 7 | 206   | up-right     | NE |

**Blocks run COUNTERCLOCKWISE on screen from North.** Corroboration: the
artmd-labeled `Idle1=56,15,0,S` frame (56) is frontal exactly like block
k=4; YRpp's `SequenceFacing {N=0, NE=1, E=2 ...}` enum is a CLOCKWISE label
ordering used for those per-entry labels — it is NOT the frame block order.
A clockwise assumption would mirror-flip every aim except N/S.

## 5. Mapping our cell axes to facing k (HIGH — pure geometry given §4)

Our iso projection: screen (sx,sy) ∝ (x−y, x+y). So a cell-space direction
(dx,dy) has screen angle `atan2(dx+dy, dx−dy)` (y-down), and

```
k = round( ((−90° − screen_angle) mod 360°) / 45° ) mod 8
```

Worked 8-way table (cell dir → k): (0,−1)→7 NE, (1,−1)→6 E, (1,0)→5 SE,
(1,1)→4 S, (0,1)→3 SW, (−1,1)→2 W, (−1,0)→1 NW, (−1,−1)→0 N.

## 6. Divergences / not modelled (documented, deliberate)

- `[GI] FireUp=2` / `[CONS] FireUp=6` (art flag, distinct from the sequence
  entry): the frame within the fire anim at which the projectile actually
  launches. Our sim applies damage at anim start (≤0.4 s cosmetic skew). Low.
- Prone behavior (Down/Crawl/FireProne when under fire) not modelled — in
  the original, infantry taking fire DO drop prone deliberately; ours stay
  standing. Carry-forward candidate, do not confuse with the §3 bug.
- Per-facing spawn direction in the original is not RE'd; we spawn facing
  S (k=4, frontal) as a cosmetic choice matching the game's placed-infantry
  look. Low.
- Walk anim rate: we keep kWalkFps=9.0 (Phase 2 tuning); artmd has no
  WalkRate= for these units and the engine default rate wasn't RE'd. Low.

## 7. Attack-approach behavior (structure MEDIUM, knobs HIGH)

Companion finding for the same bug batch (single-file attackers): the
original never paths an attacker to the target's own (occupied) cell.
`FootClass::ApproachTarget` (YRpp FootClass.h vtable; Ares hooks
UnitClass's override at 0x741613) computes a DESTINATION NEAR the target
and `Mission_Attack` re-checks weapon range every frame while moving,
stopping the unit as soon as it is in range — the destination is a firing
position, never the victim's cell. Tuning knobs confirmed to exist:
`TechnoTypeClass::CanApproachTarget`, `CanRecalcApproachTarget`,
`RulesClass::ApproachTargetResetMultiplier`. The original additionally
packs up to 3 infantry per cell via sub-cell `CellClass::OccupationFlags`
— our Phase-A 1-unit-per-cell model is kept (user-approved simplification),
with fan-out achieved by assigning each attacker its own free firing cell.
Full decompile of ApproachTarget's destination choice NOT done (content-
anchored hunt deferred); our ring-assignment is a behavioral equivalent,
flagged MEDIUM.
