# Shroud / Fog-of-War — recovered behavior (Red Alert 2: Yuri's Revenge)

Calibrate-first RE hunt. Every claim is tagged **[CONFIRMED]** (from the real
`rulesmd.ini` and/or `gamemd.exe` decompile), **[ANCHORED]** (from OpenRA — a
faithful RA2 re-implementation — or documented RA2 behavior, structure only), or
**[RE-FALLBACK]** (neither; design choice for our engine).

Evidence sources:

- Real `rulesmd.ini` (742 958 bytes) extracted from the game install:
  ```
  cd "D:/Red Alert 2"
  D:/ra2-android/build/ra2extract.exe ra2md.mix --raw localmd.mix/rulesmd.ini <out>/rulesmd.ini
  # -> wrote <out>/rulesmd.ini (742958 bytes)
  ```
  Copy kept at `D:/ra2-re/out/shroud_evidence/rulesmd.ini`.
- Ghidra decompiles of `gamemd.exe` (YR v1.001), project `D:/ra2-re` / `ra2`,
  scripts `D:/ra2-re/scripts/shroud_hunt*.py`, output `D:/ra2-re/out/shroud_*.c`.
- OpenRA reference: `D:/ra2-re/reference/OpenRA/OpenRA.Game/Traits/Player/Shroud.cs`,
  `OpenRA.Mods.Common/Traits/{AffectsShroud,RevealsShroud,CreatesShroud}.cs`.

---

## §1 Visibility data model

**State set (three states).** [ANCHORED — OpenRA `Shroud.cs`; CONFIRMED by the
cell-flag pairs in the binary]

| State | Meaning | Rendering |
|---|---|---|
| **SHROUDED** | never seen — black | black tile (`SHROUD.SHP`) |
| **FOGGED / explored** | terrain seen once, stays visible dimmed; enemy units/activity there are HIDDEN | terrain drawn, dimmed; mobile objects suppressed |
| **VISIBLE** | currently inside a friendly unit/building's sight this frame | full brightness, all objects shown |

OpenRA encodes this as `enum ShroudCellType : byte { Shroud, Fog, Visible }` and a
combinable `[Flags] enum CellVisibility : byte { Hidden=0, Explored=1, Visible=2 }`
(`Shroud.cs` lines 78, 83). "Explored" (fog) is *not* a superset of "Visible" —
they are resolved from independent per-cell counters.

**Where it lives in RA2 — per-cell flags in `CellClass`.** [CONFIRMED]

The binary stores visibility **per cell**, in the `CellClass` structure, not as a
separate per-house bitfield. The reset ("re-shroud") routine
`FUN_005775e0` and its inverse "clear shroud" (reveal-all) `FUN_005778c0`
manipulate exactly these fields on every cell (`D:/ra2-re/out/shroud_reset_5775e0.c`,
`shroud_clearall_5778F4.c`):

| Cell field | Re-shroud (`FUN_005775e0`) | Reveal (`FUN_005778c0`) | Interpretation |
|---|---|---|---|
| `cell + 0x12C` | `&= 0xFFFFFFE7` (clear bits `0x08`,`0x10`) | `\|= 0x18` | **shroud / mapped** bits (terrain explored) |
| `cell + 0x140` | `&= 0xFFFFFFFC` (clear bits `0x01`,`0x02`) | `\|= 0x03` | **fog / visible-this-frame** bits |
| `cell + 0x130` | `= 1` | `= 0` | shrouded flag / counter |
| `cell + 0x134` | `= 0` | `= 0` | (cleared in both) |

Full re-shroud clears both bit-pairs (→ black); full reveal sets both (→ visible).
The two independent bit-pairs are what give the three-state model (mapped-but-fogged
vs. currently-visible). [offsets CONFIRMED; the exact bit→state mapping is ANCHORED
from the OpenRA three-state semantics.]

**Single rendered perspective.** [CONFIRMED] Both routines only do the per-cell
work when the target house equals `DAT_00a82cb4` (the *local / currently-rendered*
player); for any other house they early-out (`shroud_reset_5775e0.c` L12-17,
`shroud_clearall_5778F4.c` L14-19). RA2 maintains the cell-level shroud for the
**local player's viewpoint**; per-house *reveal events* are tracked separately as
house flags (see §7). Diagnostic strings confirm ownership gating:
`"We tried to reset the shroud without an owner"` (0x829D58),
`"We tried to clear the shroud without an owner"` (0x829D88).

**Map cell array.** [CONFIRMED] `MapClass` holds a flat array of cell pointers,
row stride `0x200` (512) cells; the shroud loop iterates it isometrically
(diagonal raster) via the cell iterator `FUN_00577cc0`
(`D:/ra2-re/out/shroud_celliter_577cc0.c`, index `(y*0x200 + x)*4`).

**Reveal / re-shroud entry points (recovered):**
- `FUN_005778c0` — clear shroud (reveal whole map for a house). [CONFIRMED]
- `FUN_005775e0` — reset shroud (re-shroud whole map for a house). [CONFIRMED]
- `FUN_0050bb30(house)` — `if (house+0x577a == 0) FUN_005775e0(house)` (guarded reset). [CONFIRMED]
- `FUN_00508d80(house)` — per-house radar/GPS maintenance: reveal-all vs re-shroud (see §7). [CONFIRMED]

---

## §2 Sight ranges (real values from `rulesmd.ini`)

All values are **`Sight=` in whole cells**, read verbatim from the extracted
`rulesmd.ini`. Sight is a `TechnoTypeClass` field at **offset `0x5E8`** (parser
`FUN_00711510`, key `"Sight"` @ str 0x842D28 → `param+0x17a`, int). [CONFIRMED]

> **Hard cap = 10 cells.** `rulesmd.ini` `[General]` comment on `VeteranSight`:
> *"multiplier to sight !!! going past ten is a hard code Vegas crash !!!"* — sight
> is clamped at 10. `RevealTriggerRadius=9 ; … 10 is maximum`. [CONFIRMED]

### Units (our roster)

| Unit | Code | `Sight=` |
|---|---|---|
| GI (Allied basic infantry) | `E1` (Name:GI) | **5** |
| Conscript (Soviet basic) | `E2` | 5 |
| Guardian GI | `GGI` | **6** |
| Attack Dog (Soviet) | `DOG` | **9** |
| Attack Dog (Allied) | `ADOG` | **9** |
| Grizzly Battle Tank (MTNK) | `MTNK` | **8** |
| Rhino Heavy Tank (HTNK) | `HTNK` | **8** |
| IFV / Multigunner | `FV` | **8** |
| Engineer | `ENGINEER` | **4** |
| Spy | `SPY` | **9** |
| Prism Tank (SREF) | `SREF` | **8** |
| Harvester | `HARV` | **4** |
| MCV (Allied) | `AMCV` | **6** |
| MCV (Soviet) | `SMCV` | **6** |

(`[GI]`/`[IFV]`/`[MTNK]` as codes: the Allied GI's internal id is `E1` — its
`Name=GI`; the IFV's id is `FV`. Confirmed via `[InfantryTypes]` list.)

### Buildings (our roster)

| Building | Code | `Sight=` | Notes |
|---|---|---|---|
| Construction Yard (Allied/Soviet) | `GACNST` / `NACNST` | **8** | |
| Power Plant | `GAPOWR` / `NAPOWR` | **4** | |
| Refinery | `GAREFN` / `NAREFN` | **6** | |
| Barracks (Allied) | `GAPILE` | **5** | |
| Barracks (Soviet) | `NAHAND` | **6** | |
| War Factory | `GAWEAP` / `NAWEAP` | **4** | |
| Service Depot (Allied) | `GADEPT` | 5 | |
| Radar — Allied Airforce Command HQ | `GAAIRC` / `AMRADR` | **5** | `Radar=yes` |
| Radar — Soviet Radar Tower | `NARADR` | **10** | `Radar=yes` |
| Battle Lab | `GATECH` / `NATECH` | **6** | |
| Oil Derrick (neutral tech) | `CAOILD` | **6** | |
| Gap Generator (Allied) | `GAGAP` | **5** | `GapGenerator=yes`, `GapRadiusInCells=10`, `SuperGapRadiusInCells=10` |
| SpySat Uplink (Allied) | `GASPYSAT` | 5 | `SpySat=yes` — reveals entire map (§7) |
| Yuri Psychic Sensor (radar) | `NAPSIS` | **10** | `Radar=yes`, `SensorsSight=15` |

### General reveal / sight radii (`[General]`) [CONFIRMED]

| Key | Value | Meaning |
|---|---|---|
| `CameraRange` | **9** | cells around the spy-camera/spy-plane snapshot that reveal the map |
| `AircraftFogReveal` | **6** | reveal radius under a flying aircraft |
| `AttackingAircraftSightRange` | **2** | sight of an aircraft while attacking ("pings" the map) |
| `RevealTriggerRadius` | **9** | "reveal around waypoint" trigger radius (max 10) |
| `DropZoneRadius` | **4** | reveal radius around a paradrop flare |
| `PsychicRevealRadius` | **15** | radius the PsychicReveal superweapon clears |
| `LeptonsPerSightIncrease` | **2000** | height (leptons) a unit must gain per +1 cell of sight |
| `VeteranSight` | **0.0** | sight multiplier from veterancy (default: no bonus) |
| `SensorsSight` (`NAPSIS`) | **15** | psychic-sensor detection radius (cloak/subterranean) |

There is **no global default `Sight=`** in `[General]`; every combat type sets its
own (all our roster types do). Types omitting `Sight=` keep the value the
`TechnoTypeClass` constructor pre-seeds. [CONFIRMED absence of a General default]

---

## §3 Reveal algorithm

**Radius = `Sight` cells around each friendly unit/building.** [CONFIRMED value/units]
Sight is stored and used as a whole number of **cells**; general radii above are
likewise "in cells" (e.g. `RadarEventSuppressionDistances = 8,8,… ; … in cells`).

**Distance metric — radial / Euclidean within a diamond of candidate tiles.**
[ANCHORED — OpenRA `Shroud.ProjectedCellsInRange` + `AffectsShroud.ProjectedCells`]
OpenRA (faithful reimpl) enumerates candidate cells with `FindTilesInAnnulus`
(a diamond in cell space) then keeps a cell only if its center is within the
Euclidean range (`HorizontalLengthSquared <= maxRange²`), yielding a roughly
**circular** reveal footprint (`Shroud.cs` L245-267). RA2's own reveal produces the
same near-circular footprint per source. The exact in-engine rasterization is deep
in map code and not pinned to an address here; the *result* (radius = `Sight`
cells, near-circular) is what must be faithful. [ANCHORED]

**Buildings reveal from their footprint.** [ANCHORED — OpenRA `VisibilityType.Footprint`
default in `AffectsShroudInfo`] Reveal is computed from each occupied cell of the
building's footprint out to `Sight`, not from a single center point.

**Recomputed:** on add-to-world, on move (OpenRA `MoveRecalculationThreshold`
default 256 leptons = ¼ cell), and when the source's range changes each tick
(`AffectsShroud.cs` L89-172). [ANCHORED]

**Initial reveal at game start:** each house's owned objects reveal around
themselves (the add-to-world reveal), i.e. the start location and starting units are
revealed; the rest of the map is SHROUDED. [ANCHORED] (See §8 for the
reveal-all / pre-explored map options.)

**Height bonus:** a unit gains +1 cell of sight per `LeptonsPerSightIncrease`
(=2000) leptons of elevation (`RevealByHeight=` gate,
`AllowShroudedSubteranneanMoves`). [CONFIRMED keys]

---

## §4 Reshroud / fog regrow

**Default: fog does NOT regrow.** [CONFIRMED — `rulesmd.ini [General]`/`[AudioVisual]`]

```
FogOfWar=no             ; Is fog of war enabled?
ShroudGrow=no           ; Does the shroud grow back over time?
ShroudRate=4            ; minutes between each shroud creep process [0 means no shadow creep]
FogRate=.01
```

- `ShroudGrow=no` ⇒ once terrain is explored it stays revealed (dimmed) permanently;
  the shroud does **not** creep back. `ShroudGrow` is parsed into the Rules
  singleton at `+0x5FC` (parser `FUN_00668c20` @ 0x66A0C6, key str 0x839594).
  [CONFIRMED]
- `ShroudRate=4` is the creep interval **only when `ShroudGrow=yes`** (0 disables).
- `FogOfWar=no` (default, and again in the map-defaults block at line 3040): the
  regrowing fog that re-hides mobile objects in explored areas is **off by default**;
  the black shroud over unexplored terrain is always active. Multiplayer can enable
  it (§8).

**GapGenerator (re-shroud for enemies).** [CONFIRMED] `[GAGAP]` (Allied Gap
Generator): `GapGenerator=yes`, `GapRadiusInCells=10`, `SuperGapRadiusInCells=10`,
`Sight=5`. It projects **shroud** over a 10-cell radius onto *enemy* players
(re-covering already-explored terrain for them), while its owner still sees through
it. Mechanically this is a shroud *source* (OpenRA `CreatesShroud`: a positive
"generated shroud" count per cell that a viewer must out-reveal;
`Shroud.cs` `UpdateCell` L214 — a cell with generated-shroud > 0 and no local
visibility resolves to Shroud even if previously explored). [flag values CONFIRMED;
per-cell mechanism ANCHORED from OpenRA]

---

## §5 Rendering consultation

[state→render mapping ANCHORED from OpenRA + the CONFIRMED cell-flag pairs; exact
blit code is design]

- **SHROUDED** cells → drawn black with `SHROUD.SHP` (str 0x81BE28, renderer
  `FUN_0047EEF0` region). [asset CONFIRMED]
- **FOGGED (explored)** cells → terrain drawn but **dimmed/blended**
  (`BlendedFog=yes` ⇒ blend rather than dither the fog); mobile objects (enemy
  units, and enemy building state changes) in fogged cells are **suppressed** —
  the player sees the last-known terrain, not current activity. [CONFIRMED key
  `BlendedFog`]
- **VISIBLE** cells → full brightness, all objects drawn. [ANCHORED]
- Cell state is read from the `cell+0x12C` (explored) / `cell+0x140` (visible)
  bit-pairs recovered in §1. [CONFIRMED offsets]
- **Radar / minimap** is gated by explored state and by owning an operational
  radar building (`Radar=yes`); with a SpySat/GPS the whole minimap is revealed
  (§7). [CONFIRMED via §7]

For our engine: shrouded = don't draw terrain/objects (black); fogged = draw
terrain dimmed, hide mobile/enemy objects; visible = draw everything.
[RE-FALLBACK for the exact dim factor — pick a blend to match `BlendedFog`.]

---

## §6 Targeting / AI consultation

[ANCHORED — OpenRA + documented RA2 behavior; the concrete auto-acquire scan lives
in our combat path]

- A unit/building in a **non-visible** cell (shrouded *or* fogged) **cannot be
  auto-acquired or targeted** by an enemy. Auto-acquire and the AI target scan must
  consult the scanning house's cell visibility and skip objects the house cannot
  currently see. [ANCHORED]
- OpenRA gates target validity on `Shroud.IsVisible`/frozen-actor state; RA2 uses
  the same per-cell visibility bits recovered in §1 (a fogged enemy is represented
  by its last-known frozen state, not a live target). [ANCHORED]
- This resolves the open thread from `docs/re/combat.md` auto-acquire: the scan
  should reject candidates whose cell is not VISIBLE to the scanning house.
  [RE-FALLBACK until our engine has the per-house visibility layer — see §9]
- Cloak/submerged detection is a separate layer: `SensorsSight`
  (`TechnoType+0x5F0`) and `DetectDisguiseRange` reveal cloaked/disguised/subterranean
  objects within their radius. [CONFIRMED fields]

---

## §7 Spy / radar reveal effect (resolves the capture-phase INERT item)

This closes the item left INERT in `docs/re/capture.md` §B6.

**A. Owning an operational radar / SpySat → reveal whole map.** [CONFIRMED]
`FUN_00508d80(house)` scans the house's building list (`house+0x6c`, count
`+0x78`); if a powered, operational building whose **type** has the flag at
`Type+0x16A5` (SpySat/GPS provider) is found, it calls
`FUN_005778c0(house)` = **clear shroud (reveal the entire map)** and sets the latch
`house+0x577A = 1`. If that reveal source is lost, it calls
`FUN_005775e0(house)` = **re-shroud the whole map** (`shroud_caller_508D80.c`).
So the full-map reveal (Allied `GASPYSAT` `SpySat=yes`) is a house-wide shroud clear,
toggled by the `house+0x577A` latch.

**B. Spy infiltrating an enemy Radar → re-shroud the victim.** [CONFIRMED]
In the infiltrate dispatch `FUN_004571e0` (`shroud_infiltrate_4571e0.c`), when the
infiltrated building's type is a radar (`Type+0x16A4 != 0`, L124) it calls
`FUN_0050bb30(victimHouse)` → `if (victimHouse+0x577A == 0) FUN_005775e0(victimHouse)`
= **reset (re-shroud) the radar owner's entire map**, unless that house is protected
by an active SpySat/GPS reveal (latch set). This is the classic "radar sabotage":
the spy's victim loses their explored map and must re-scout. Diagnostic string
`"We tried to reset the shroud without an owner"` (0x829D58) is emitted by the reset
routine. **Recovered effect for our engine: on spy→enemy-radar infiltration, reset
the victim house's cell visibility to fully shrouded (re-explore required).**

**C. Other reveal sources (house reveal flags).** `FUN_004571e0` also sets per-house
reveal/notify flags for radar/lab/spy-plane branches
(`house+0x2BC..0x2C0`, `house+0x1FC`, global `DAT_00883b0e`); these drive
map-reveal-to-ally/GPS bookkeeping rather than the cell bits directly. [CONFIRMED
locations, per capture.md §B6]

**D. Spy-plane / camera reveal.** A spy-plane snapshot reveals `CameraRange=9`
cells around each look; `AircraftFogReveal=6` under aircraft;
`Camera=yes` warheads reveal around the impact. [CONFIRMED keys]

---

## §8 Game options

[CONFIRMED keys; MP lobby wiring ANCHORED]

- **Fog of War** — `FogOfWar=no` default. OpenRA exposes it as the lobby "fog"
  checkbox (default on in OpenRA, off in RA2's shipped rules); when off, explored
  terrain never re-fogs. `[MultiplayerDialogSettings]`-style overrides can enable it.
- **Reveal / pre-explored map** — the "clear shroud" routine `FUN_005778c0` is the
  reveal-all primitive; a "no shroud / reveal map" match option calls it for a house
  at start (OpenRA `ExploredMapCheckboxEnabled` / `ExploreAll`, `Shroud.cs` L150-157).
  [ANCHORED]
- **AllyReveal=yes** — allies automatically share their revealed radar map with each
  other. [CONFIRMED]
- **Per-house start reveal** — each house's starting objects reveal around
  themselves at game start (§3 initial reveal). [ANCHORED]

---

## §9 Confidence summary

| Item | Finding | Tag | Why |
|---|---|---|---|
| §1 three-state model (Shroud/Fog/Visible) | State set correct | **[ANCHORED]** | OpenRA `ShroudCellType`/`CellVisibility`; consistent with two cell bit-pairs |
| §1 storage = per-cell `CellClass` flags | `cell+0x12C` bits `0x18` (explored), `cell+0x140` bits `0x03` (visible), `+0x130`/`+0x134` | **[CONFIRMED]** | reset `FUN_005775e0` / reveal `FUN_005778c0` decompiles |
| §1 local-player single perspective | cell bits gated on `DAT_00a82cb4` | **[CONFIRMED]** | both routines early-out for non-local house |
| §2 Sight table (units + buildings) | all values in table | **[CONFIRMED]** | verbatim from real `rulesmd.ini` |
| §2 Sight field offset `0x5E8`, cap 10 | TechnoType field; hard cap 10 | **[CONFIRMED]** | parser `FUN_00711510`; `VeteranSight` rules comment |
| §2 General radii (CameraRange 9, etc.) | table | **[CONFIRMED]** | real `rulesmd.ini [General]` |
| §3 radius = Sight cells | units used | **[CONFIRMED]** | Sight is cells; radii "in cells" |
| §3 distance metric (near-circular/Euclidean, footprint) | diamond-of-tiles + Euclidean filter | **[ANCHORED]** | OpenRA `ProjectedCellsInRange`; in-engine raster not address-pinned |
| §3 recompute on move/tick, initial reveal | | **[ANCHORED]** | OpenRA `AffectsShroud` |
| §4 fog does NOT regrow | `ShroudGrow=no`, `FogOfWar=no`, `ShroudRate=4` | **[CONFIRMED]** | real `rulesmd.ini`; parser `FUN_00668c20` |
| §4 GapGenerator re-shrouds enemies | `GapGenerator=yes`, radius 10 | **[CONFIRMED]** (flags) / **[ANCHORED]** (per-cell mechanism) | `[GAGAP]` rules; OpenRA `CreatesShroud` |
| §5 rendering (black/dim/full, blended fog) | | **[ANCHORED]** + **[CONFIRMED]** keys | `BlendedFog`, `SHROUD.SHP`; cell bits from §1 |
| §6 targeting skips non-visible cells | | **[ANCHORED]** / **[RE-FALLBACK]** | OpenRA; our engine lacks the per-house layer yet |
| §7 SpySat/GPS reveal-all + spy-radar re-shroud | `FUN_00508d80`, `FUN_0050bb30`, `FUN_005775e0`, `Type+0x16A4/0x16A5`, latch `+0x577A` | **[CONFIRMED]** | decompiles + capture.md §B6; resolves the INERT item |
| §8 FogOfWar / reveal-map / AllyReveal options | | **[CONFIRMED]** keys / **[ANCHORED]** lobby | rules + OpenRA |

**Net for implementation.** The DATA is faithful and CONFIRMED: sight values,
the three-state model, per-cell storage with two bit-pairs, `Sight` in cells capped
at 10, **no fog regrow by default**, GapGenerator radius 10, and the full
spy/radar/SpySat reveal semantics (own radar/SpySat → reveal whole map; spy→enemy
radar → re-shroud the victim). The reveal *rasterization* (exact near-circular
footprint) and the render/targeting *consultation* are ANCHORED from OpenRA and safe
to implement as design, since RA2's own cell-visibility result is what matters.

---

## §Adjudication (RE-GATE) — 2026-07-19

Task 1 RE-GATE for the Shroud/Fog phase. The hunt (§1–§9) is the calibration
authority; this section re-confirms the values against the real `rulesmd.ini`
(extracted copy `D:/ra2-re/out/shroud_evidence/rulesmd.ini`, 742 958 bytes) and
closes the three `[RE-FALLBACK]` items flagged in the design spec. No fresh hunt.

### Re-pinned Sight table (spot-check vs §2 — CONFIRMED)
Verbatim from the extracted `rulesmd.ini`: `[E1] Sight=5`, `[MTNK] Sight=8`,
`[DOG] Sight=9`, `[NARADR] Sight=10`, `[NAPSIS] Sight=10`, `[GAGAP] Sight=5` —
all match §2. No `[General]` global default `Sight=`. Hard cap 10 stands.

### Item 1 — Fog dim factor → ADJUDICATED (cosmetic, unhashed)
`BlendedFog=yes` is present (`rulesmd.ini:408` — *"should we blend the fog (as
opposed to dither it)"*). RA2 BLENDS fogged terrain rather than dithering it.
**Decision:** fogged cells are dimmed with a translucent-black `shade_rect`
(alpha **128** = ~50% black) approximating the blend; shrouded cells are painted
full black. This is COSMETIC and UNHASHED (render is not part of any gate hash),
so the exact alpha is a free design choice — 128 chosen. Flip §5 → adjudicated.

### Item 2 — GASPYSAT roster presence → ADJUDICATED (in rules, NOT in our kRoster)
`[GASPYSAT]` exists in the real rules (`rulesmd.ini:12181`, `SpySat=yes`
`:12189`, `[BuildingTypes] 63=GASPYSAT` `:1247`). It is NOT in our engine's
`kRoster` (the Capture phase ended the roster at `CAOILD`, roster=20).
**Decision:** the SpySat mechanism ships driven by a `grants_map_reveal` type
flag resolved from `SpySat=` in rules (`rules.spy_sat(id)` / `SpySat != 0`); since
no rostered building is a SpySat, the flag is FALSE for every currently-placed
building → the reveal-all path is inert in normal play until a SpySat building is
added to the roster (a future roster-broadening phase). The `--demo-shroud` gate
spawns a `grants_map_reveal=true` building directly to exercise the path. Flip
§7A → adjudicated.

### Item 3 — Reveal rasterization boundary → ADJUDICATED (dx²+dy²≤sight², inclusive)
The exact in-engine near-circular raster is not address-pinned (§3 ANCHORED).
**Decision:** near-circular membership is `dx*dx + dy*dy <= sight*sight` with
INTEGER cell centers and an INCLUSIVE boundary (a cell at exactly `sight²`
qualifies), enumerated over the `[-sight,sight]²` bounding box, out-of-bounds
cells skipped. `sight <= 0` reveals only the source cell (`dx=dy=0`). This is
deterministic and stable, which is what the `--demo-shroud` gate requires; it
reproduces RA2's per-source footprint (radius = Sight cells, roughly circular).
Buildings reveal from EACH occupied footprint cell (§3). Flip §3 → adjudicated.

### Fog-no-regrow re-confirmed
`FogOfWar=no` (`:205` and map-defaults `:3040`), `ShroudGrow=no` (`:677`).
`explored_` is sticky; only `visible_` is recomputed per tick. CONFIRMED.

**Net:** all three `[RE-FALLBACK]` items are adjudicated with concrete, recovered-
data-backed decisions. The plan's Task 3/5/7 `[RE-GATE APPLIED]` markers reflect
these (dx²+dy²≤sight² inclusive; fog alpha 128 cosmetic; SpySat via
`grants_map_reveal` flag, inert until rostered). No item requires human escalation.
