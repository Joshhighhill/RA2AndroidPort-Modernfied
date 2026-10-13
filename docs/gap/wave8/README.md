# Wave 8 — interaction audit (2026-10-13, master 34cb652)

**Why this wave exists.** Per-area audits (waves 1–7) missed bugs that only appear when two areas meet. The user found one in real play: Tanya playing her Swim animation on a bridge deck (fixed in 3a7c551).

**What it covered.** Six read-only opus agents tested UNIT FAMILY × TERRAIN/LAYER × STATE against the original, using Ghidra on YR 1.000, game data and --play runs.

| # | Report | Gaps |
|---|--------|------|
| 01 | [Swimming infantry](01-swimming-infantry.md) | 7 (4 major) |
| 02 | [Ground infantry](02-ground-infantry.md) | 4 (2 major) |
| 03 | [Ground vehicles](03-ground-vehicles.md) | 9 + 1 suspected (2 major) |
| 04 | [Hover / amphibious / naval](04-hover-amphib-naval.md) | 5 (1 major) |
| 05 | [Air / jumpjet / paradrop](05-air-jumpjet.md) | 7 (2 major) |
| 06 | [Special effects × terrain](06-special-effects.md) | 7 (1 blocker, 3 major) |

## Root causes (one fix closes gaps in several reports)

1. **Saving during a paradrop cannot be loaded.**
   - Gap: W8-06-01, BLOCKER.
   - Symptom: "Error loading game", and a crash in the campaign.
   - Cause: `savegame.cpp:306` rejects `kParachuteCargo`.
2. **The low bridge is not Road.**
   - Gaps: W8-01-04, W8-03-03, W8-04-01.
   - Rule: an intact low-bridge overlay makes its cell Road (0x47D1C0, loader 0x5FE148). The clone keeps the tile land type under it, which causes:
     - Swim and water sounds on the bridge;
     - subs and Aegis able to target units on it;
     - half speed over water.
   - Also W8-01-06: a section destroyed at runtime over Beach becomes walkable, because `blocks_ground` lacks Beach.
3. **The deck and the ground under it share one occupancy layer.**
   - Gaps: W8-01-05, W8-02-02, W8-03-04.
   - The original keeps separate lists for the two layers (0x47EAB0..0x47EB50, 0x47E7B0). In the clone:
     - deck tanks block ships and swimmers below;
     - a unit on one layer pushes or scatters units on the other;
     - crush warnings cross layers.
4. **Bridge collapse deaths are wrong.**
   - Gaps: W8-01-01/02/03, W8-02-01, W8-03-05/06/07, W8-06-02/03.
   - The clone kills one unit per cell (`occ_.at`) and spares units under the span; passengers escape.
   - The original:
     - high bridge (0x47DC80, 0x5F3970, 0x5F3680): every deck unit falls and dies, everything under the span dies, and PreventPassengerEscape applies;
     - low bridge (0x487920): kills whatever can no longer enter the cell, plus jumpjets.
5. **Bridge damage sources are missing or ungated.**
   - Gaps: W8-06-04, W8-05-04/05, W8-04-03, W8-03-09.
   - Missing: the Lightning Storm 17% roll (0x53A030 / 0x489DCB), and V3/Dreadnought rockets and crash death weapons through DamageArea.
   - Ungated: the high-bridge roll needs the deck-height window (0x489E87), and it must not depend on the map also having a low bridge.
6. **Blasts and effects ignore layers.**
   - Gaps: W8-04-02, W8-05-06, W8-02-03, W8-03-08, W8-05-03.
   - DamageArea picks the deck or the ground layer by impact height (0x489190).
   - Effects on a deck are drawn 60 px low.
   - Height for range, elevation bonus and line of fire ignores the +4 deck levels (0x487C60).
   - Jumpjet altitude ignores the deck, and steps 60 px at a cliff edge.
7. **Placement rules are wrong.**
   - W8-03-01: buildings and MCVs are allowed on ore, slopes and bridge cells; the original refuses them (0x47C530).
   - W8-03-02: rock and fence overlays are passable; the original makes them Rock (0x47D1C0).
8. **Exit and landing cells are wrong.**
   - W8-06-07 / W8-02-04: garrison exits must be a cell on the building's border, otherwise the occupants are killed (0x457DD0 / 0x4585B0).
   - W8-05-01: the Night Hawk lands on a water deck or walks to the nearest ground; the original hovers (0x54C2AA).
   - W8-05-02: a jumpjet landed on a deck is drawn under it.
   - W8-03-10 (suspected): drone and passenger exits have no height check.
9. **Chronosphere on layers.**
   - W8-06-05: deck vs under-deck.
   - W8-06-06: riding parasites must be ejected (0x7188D9).
10. **Smaller items.**
    - W8-05-07: paradrop cadence and sub-cell placement (0x415960 / 0x415C60).
    - W8-04-05: Robot Tank over water losing control.
    - W8-04-04: LCRF/SAPC/ROBO wakes.
    - W8-01-07: leave-water sound when boarding a transport.

Evidence (scripts, logs, dumps) is kept in `D:\ra2-re\w8int-0N-out`.
