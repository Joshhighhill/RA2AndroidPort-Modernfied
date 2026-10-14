# Tech-tree / prerequisites — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
(`techtree_hunt.py`), the extracted `rulesmd.ini`, the decompile dumps
(`D:\ra2-re\out\tt_*.c`), the CRC mix-id listings and the `ra2extract` art
probes are all off-repo in the session scratchpad — nothing off-repo is
committed. **Content-anchored only:** the two functions below were reached from
the TechnoTypeClass prereq-vector offsets and the `[General]` `Prerequisite*`
INI-key string refs in THIS binary, never from published Ares/YRpp addresses.
The Ares/YRpp address drift documented in `workflow.md` §4 / `combat.md` §A
holds here too — the prereq subsystem is content-relocated. Per-unit numeric
constants below are read from the real `rulesmd.ini` extracted from `ra2md.mix`
with the repo's `ra2extract`.)

This document is the calibration authority for the **CB-5 Tech-Tree /
Prerequisites** phase. It is **largely CONFIRMATORY**: the prereq AND-mechanism
(`FUN_00505180`) and the six `[General] Prerequisite<Group>` member lists were
already recovered in `construction.md` §10 and `production.md` §1.6 — this hunt
RE-anchors them for the tech-tree phase and PINS the Battle-Lab / advanced-unit
specifics (`[GATECH]`, `[SREF]`, `[E1]/[GGI]/[MTNK]/[FV]/[APOC]`) plus the art
presence on this install. Confidence per finding; "Deviations we take" at the
end resolves the `[RE-GATE: techtree.md §N]` markers in the CB-5 plan.

Key global: `DAT_00886160` = `RulesClass::Instance` (matches `production.md`).

## TL;DR — the deltas from the plan's pre-hunt assumptions (what the RE-GATE needs)

1. **Battle-Lab id is `GATECH`, not `GACTEK`.** [§4, CONFIRMED]. `[GACTEK]` does
   not exist in the real `rulesmd.ini` (0 occurrences). The Allied Battle Lab is
   `[GATECH]`, registered `[BuildingTypes] 7=GATECH`. Treat "GACTEK" in the
   controller brief as a typo; all code must use `GATECH`.

2. **`[E1]/[GGI]/[MTNK]/[FV] Prerequisite=` are each a SINGLE plain token** —
   `GAPILE`, `GAPILE`, `GAWEAP`, `GAWEAP` respectively. [§5, CONFIRMED]. None is
   a group token; the full-parse resolves them byte-identically to the hardcoded
   `GAPILE`/`GAWEAP` demo tokens. Task 2 stays byte-identical.

3. **`[GATECH]`'s OWN prereq is `GAWEAP,RADAR,GACNST` (TechLevel=8)** — it is
   itself radar-gated. [§4, CONFIRMED]. This does NOT gate the demo: the plan
   places GATECH directly, so its own prereq is not evaluated. Recorded for
   completeness; the RE-GATE should note the demo bypasses it.

4. **`[SREF] Prerequisite=GAWEAP,GATECH` (TechLevel=8, Cost=1200)** — exactly the
   `production.md` §1.6 recovery, now confirmed from the real section. [§4,
   CONFIRMED]. `[APOC] Prerequisite=NAWEAP,NATECH` (TechLevel=7) confirmed — the
   Soviet-AI `APOC→NAWEAP` proxy stays justified. [§5, CONFIRMED].

5. **ART DELTA — the Allied Battle Lab `GATECH` art is ABSENT on this install
   (partial / "demo-art").** [§4/§5, [RE-FALLBACK]]. `gatech.shp`/`gatek.shp`
   (building SHP) and its cameo (`gatechicon.shp`/`gatekicon.shp`) are in NO mix.
   The Soviet (`natek.shp` + `ntekicon.shp`) and Yuri (`ngtek.shp`) Battle-Lab
   art ARE present, and even the base Allied building SHPs (`gapowr/gacnst/
   gaweap/gapile.shp`) are absent — this install ships the YR expansion-cache art
   but not the base-RA2 Allied building set. **`SREF` (Prism Tank) art IS fully
   present** (`sref.vxl`+`sref.hva` voxel + `sreficon.shp` cameo). The sim is
   art-independent; the GATECH fallback (placeholder building, `art.ok=false`,
   kept in roster) applies per spec §5.

   > **CORRECTION (CB-5 finalize, commit `99cdd58`):** the `ra2extract`
   > by-filename probe above missed the runtime resolution. When GATECH is
   > loaded through the *actual* Allied-building-art path (which resolves via the
   > NewTheater generic-art munge), its SHP + cameo **DO resolve on this install**
   > (`ok=true`, `cameo=y`, `fnd=3x2`). The [RE-FALLBACK] placeholder path is
   > retained defensively (a partial install missing the Allied set would still
   > degrade gracefully — buildings are never dropped from the roster), but it is
   > NOT exercised here. This does not affect roster membership, buildability, or
   > any gate hash (all art-independent). The ABSENT claim below reflects the raw
   > by-filename probe, not the runtime munge — read it with this correction.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00505180` | **prerequisite predicate** — every `Prerequisite=` token ANDed; a plain id must be owned; a negative (group) id switches to one of six `RulesClass` BuildingType vectors and passes if ≥1 member is owned. Pure over the house owned-building list; **no RNG**. | consumer of the TechnoType prereq vector `type+0x63c`(ptr)/`+0x648`(count); re-anchored this hunt (`construction.md` §10) |
| `FUN_0066cf70` | **`RulesClass::LoadFromINI` `[General]` loader** — reads `PrerequisitePower/Factory/Barracks/Radar/Tech/Proc` (+ `PrerequisiteProcAlternate`) into six+1 int-list vectors at `Rules+0x358…+0x3e4` (stride 0x1C) then `+0x400`. | `s_PrerequisitePower_0083bc00` … `s_PrerequisiteProc_0083bb98` string refs (`construction.md` §10 = `FUN_0066CF70`) |

## §1 `Prerequisite=` syntax + group tokens + satisfaction rule (item 1)

`FUN_00505180` re-anchored this hunt (`D:\ra2-re\out\tt_prereq_check.c`, entry
`0x505180`). Arguments: (1) the TechnoType, (2) a vector whose data pointer is
at offset `+4` holding the house's owned building ids, (3) that list's count.
Our pseudocode of the structure:

```text
prereqs_met(type, ownedList, ownedCount):
    tokens = type->[0x63C]      // pointer to int array
    count  = type->[0x648]      // token count on the TechnoType
    for i in 0 .. count-1:
        tok = tokens[i]
        if tok < 0:             // GROUP token -> pick a Rules (0x886160) BuildingType vector
            group = Rules + { -6: 0x8E4, -5: 0x91C, -4: 0xA34,
                              -3: 0x900, -2: 0x938, -1: 0x8C8 }[tok]
            // the group's member count is the 5th dword of that vector (index 4)
            if no member id of group appears in ownedList[0..ownedCount):
                return 0        // no member owned -> FAIL
            continue            // some member owned -> token passes
        // PLAIN token: the id must appear in ownedList[0..ownedCount)
        if tok not found in ownedList: return 0
    return 1                    // every token satisfied
```

- **Semantics: pure AND.** The first unmet token `return 0`s; the function
  returns 1 only after every token passes. Matches `construction.md` §10 and the
  engine's existing `prerequisites_met` AND-loop.
- **Plain token** = own ≥1 building of exactly that BuildingType (scan of the
  house owned-building list: argument 2's data pointer at `+4`, count in
  argument 3). The "live/placed
  (Idle) building" requirement is a property of what the house owned-list
  contains (placed buildings register there; queued/unplaced do not) — see
  `construction.md` §9/§10.
- **Group token** (negative id) → one of the six `RulesClass` BuildingType
  vectors at `Rules+{0x8c8,0x938,0x900,0xa34,0x91c,0x8e4}`; satisfied if the
  house owns ≥1 member. The negative-id→group-name binding is semantically
  forced by the loader key order (§2); the exact numbering is
  [RE-FALLBACK: ModEnc] and behaviourally irrelevant.
- **No per-frame RNG.** The body is deterministic iteration over the owned-list
  and the rules vectors — no call to the game RNG (`FUN_007C4D00`/`FUN_0065C220`).

Member lists (real `rulesmd.ini`, `[General]`, verbatim):

```
PrerequisitePower        = GAPOWR,NAPOWR,NANRCT,YAPOWR   ; (comment: gs NAAPWR)
PrerequisiteFactory      = GAWEAP,NAWEAP,YAWEAP
PrerequisiteBarracks     = NAHAND,GAPILE,YABRCK
PrerequisiteRadar        = GAAIRC,NARADR,AMRADR,NAPSIS
PrerequisiteTech         = GATECH,NATECH,YATECH
PrerequisiteProc         = GAREFN,NAREFN,YAREFN
PrerequisiteProcAlternate= SMIN                          ; still counts under a PROC listing
```

**Confidence: CONFIRMED** (predicate decompiled; member lists read from real
rulesmd — identical to `production.md` §1.6).

## §2 How a building declares its group membership (item 2)

The `[General]` loader `FUN_0066cf70` reads the six keys in order into six
adjacent RulesClass vectors (stride `0x1C` = 28 bytes), then the alternate:

| key (string) | RulesClass offset |
|---|---|
| `PrerequisitePower` (`0083bc00`) | +0x358 |
| `PrerequisiteFactory` (`0083bbec`) | +0x374 |
| `PrerequisiteBarracks` (`0083bbd4`) | +0x390 |
| `PrerequisiteRadar` (`0083bbc0`) | +0x3ac |
| `PrerequisiteTech` (`0083bbac`) | +0x3c8 |
| `PrerequisiteProc` (`0083bb98`) | +0x3e4 |
| `PrerequisiteProcAlternate` | +0x400 |

Each is parsed by the comma-separated INI-list reader (`FUN_00477010`) into a
BuildingType id vector. A building "declares membership" purely by being LISTED
in one of these `[General]` keys — there is no per-building membership flag; the
name→group binding (`TECH`→`PrerequisiteTech`, etc.) is the loader key order.
These load-time vectors (+0x358…) are the parsed int lists; the runtime check
(§1) consults the resolved BuildingType pointer-vectors at +0x8c8…+0xa34. This
is exactly what the engine's `Rules::prerequisite_group` / `BuildingAssets::
groups` already read. Matches `construction.md` §10 (`Prerequisite{...}` →
+0x358…+0x3E4 stride 0x1C, alternate +0x400).

**Confidence: CONFIRMED** (six key string refs + offsets in the loader listing).

## §3 The buildability predicate consuming owned buildings (item 3)

`FUN_00505180` (§1) IS the predicate: it walks the house owned-building-type
list (argument 2's data pointer at `+4`, count in argument 3) per token and returns a single "buildable
now" boolean. The engine's `can_start` / `can_start_unit` wrap it with the two
other gates recovered previously: `tech_level > 0` (per-type `TechLevel=`, §4)
and a factory-alive check, then `prerequisites_met`. No RNG in the predicate
(§1). This is faithful to the engine's existing `can_start`/`can_start_unit`.

**Confidence: CONFIRMED** (same function as §1; the tech_level + factory gates
are `construction.md` §10 / `production.md` §1.1).

## §4 The Battle Lab + the advanced unit requiring TECH (item 4)

Ground truth from the real `rulesmd.ini` (extracted `ra2md.mix →
localmd.mix/rulesmd.ini`, 742 958 bytes):

| section | Prerequisite= | TechLevel= | Cost= | notes |
|---|---|---:|---:|---|
| **`[GATECH]`** Allied Battle Lab | `GAWEAP,RADAR,GACNST` | **8** | 2000 | `BuildCat=Tech`, `Power=-100`, `Armor=wood`, `Strength=500`, `SuperWeapon=ForceShieldSpecial`; Owner = all sides |
| **`[SREF]`** Prism Tank | `GAWEAP,GATECH` | **8** | 1200 | Allied vehicle; `[VehicleTypes] 37=SREF` |
| `[NATECH]` Soviet Battle Lab | `NAWEAP,RADAR,NACNST` | 7 | — | TECH-group sibling |
| `[YATECH]` Yuri Battle Lab | `YAWEAP,YACNST,RADAR` | 8 | — | TECH-group sibling |

- **`[GATECH] Prerequisite=GAWEAP,RADAR,GACNST`** — the Battle Lab is itself
  War-Factory + Radar-group + ConYard gated. **The demo places GATECH directly,
  so this is not the gating path** (recorded for the RE-GATE).
- **`[SREF] Prerequisite=GAWEAP,GATECH`** — exactly `production.md` §1.6.
  Confirmed from the section.
- **`TECH` group members = `GATECH,NATECH,YATECH`** — confirmed (§1).
- **Id resolution:** `[GACTEK]` = 0 occurrences; `[GATECH]` is the id
  (`[BuildingTypes] 7=GATECH`). **The Battle Lab id is `GATECH`.**

**Art presence** (`ra2extract`, verified two ways — CRC mix-id membership over
each sub-mix's full id set AND direct `--raw` extraction; the mix-id hash is the
repo's `mix_id`, TS/RA2 scheme):

| asset | file | location | result |
|---|---|---|---|
| SREF voxel body | `sref.vxl` (50 277 B) | `ra2.mix → local.mix` | **PRESENT** |
| SREF voxel anim | `sref.hva` | `ra2.mix → local.mix` | **PRESENT** |
| SREF cameo | `sreficon.shp` (2 912 B) | `language.mix → cameo.mix` | **PRESENT** |
| GATECH building SHP | `gatech.shp` / `gatek.shp` | — | **ABSENT** |
| GATECH cameo | `gatechicon.shp` / `gatekicon.shp` | — | **ABSENT** |
| (Soviet ref) NATECH SHP | `natek.shp` (53 736 B) | `ecache01.mix` | present |
| (Soviet ref) NATECH cameo | `ntekicon.shp` | `ecache01.mix` | present |
| (Yuri ref) YATECH SHP | `ngtek.shp` (53 720 B) | `ecache01.mix` | present |

This install is **partial / "demo-art"**: `ecache01.mix` carries only ~527
entries (a full YR ecache is thousands) and holds the YR-era expansion art
(NATECH/YATECH Battle Labs, `gaarty/gahpad/gagap/gadpsa/gamine`, …). The base-RA2
Allied building SHP set — `gapowr.shp`, `gacnst.shp`, `gaweap.shp`, `gapile.shp`
AND `gatech.shp` — is absent from every mix on this install. **GATECH art is an
[RE-FALLBACK]**: per spec §5 the building loader keeps `GATECH` in `ba.roster`
with `ba.art[i].ok=false` and renders a placeholder (buildings are never dropped
from the roster), so the demo still places it. **SREF art needs no fallback.**

**Confidence: CONFIRMED** (rulesmd values + id) + art-presence measured
([RE-FALLBACK] for GATECH art only).

## §5 Determinism + byte-identity (item 5)

- **No RNG.** `FUN_00505180` is pure over the owned-building list and the rules
  group vectors (§1) — the buildability change cannot alter what the existing
  demos build.
- **Player-unit full-parse byte-identity** (real `rulesmd.ini`):

  | section | Prerequisite= | TechLevel= | Cost= | resolves to |
  |---|---|---:|---:|---|
  | `[E1]` (GI, `Image=GI`) | `GAPILE` | 1 | 200 | plain `GAPILE` |
  | `[GGI]` (Guardian GI) | `GAPILE` | 2 | 400 | plain `GAPILE` |
  | `[MTNK]` (Grizzly, `Image=GTNK`) | `GAWEAP` | 2 | 700 | plain `GAWEAP` |
  | `[FV]` (IFV) | `GAWEAP` | 3 | 600 | plain `GAWEAP` |

  Every one is a **single plain token** — no group token — so the full-parse
  resolves `E1/GGI → GAPILE` and `MTNK/FV → GAWEAP` byte-identically to the
  hardcoded demo tokens. Task 2 stays byte-identical.
- **Soviet-AI proxy justification:** `[APOC] Prerequisite=NAWEAP,NATECH`
  (TechLevel=7, Cost=1750). The real Apocalypse ANDs `NAWEAP` with the Soviet
  Battle Lab `NATECH`; keeping the `APOC→NAWEAP` proxy (dropping the `NATECH`
  conjunct the AI base never builds) protects `ai_team_hash` while never making
  APOC buildable where the real rule would not.

**Confidence: CONFIRMED** (no RNG in the predicate; per-unit values quoted).

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 `Prerequisite=` AND-semantics; plain=own-1, group=own-1-member; no RNG | `FUN_00505180` decompile + real rulesmd group lists | **CONFIRMED** |
| §2 six `[General] Prerequisite<Group>` keys → six RulesClass vectors (+0x358…+0x3e4) | `FUN_0066cf70` loader listing + key string refs | **CONFIRMED** |
| §3 predicate consumes house owned-list; wrapped by tech_level>0 + factory-alive | `FUN_00505180` + `construction.md`/`production.md` gates | **CONFIRMED** |
| §4 `[GATECH]`=`GAWEAP,RADAR,GACNST`/TL8/2000; `[SREF]`=`GAWEAP,GATECH`/TL8/1200; TECH={GATECH,NATECH,YATECH} | real rulesmd sections | **CONFIRMED** |
| §4 Battle-Lab id = `GATECH` (GACTEK absent) | rulesmd `[BuildingTypes] 7=GATECH`; 0× GACTEK | **CONFIRMED** |
| §4 SREF art present (voxel+cameo) | `ra2extract` mix-id + `--raw` | **CONFIRMED (present)** |
| §4/§5 GATECH art absent (partial install) | `ra2extract` mix-id + `--raw`; NATECH/YATECH present, base Allied SHPs absent | **[RE-FALLBACK]** |
| §5 no RNG; E1/GGI=GAPILE, MTNK/FV=GAWEAP (single plain tokens); APOC=NAWEAP,NATECH | `FUN_00505180` + real rulesmd | **CONFIRMED** |

## Deviations we take (for the RE-GATE review)

- **[PLAN OK — id]** Battle-Lab id is `GATECH`; "GACTEK" in the brief is a typo.
  Code uses `GATECH` everywhere. No amendment beyond confirming the id.
- **[PLAN OK — byte-identity]** `[E1]/[GGI]=GAPILE`, `[MTNK]/[FV]=GAWEAP` are
  single plain tokens; Task 2 remains byte-identical to the hardcoded demo
  tokens. No group token sneaks in.
- **[PLAN OK — SREF]** `[SREF] Prerequisite=GAWEAP,GATECH`, `TechLevel=8`. The
  full-parse consumes real rulesmd (data-driven); the synthetic fallback def
  uses `{GAWEAP,GATECH}` + `tech_level = max(1, TechLevel)` per spec §6.
- **[PLAN OK — APOC proxy]** `[APOC] Prerequisite=NAWEAP,NATECH`; the Soviet-AI
  roster keeps its `APOC→NAWEAP` proxy (byte-identity for `ai_team_hash`).
- **[DEVIATION — GATECH placed directly]** The Battle Lab is placed directly in
  the demo rather than tech-gated by radar; its own `Prerequisite=GAWEAP,RADAR,
  GACNST` is therefore not evaluated in the demo (recorded, not enforced).
- **[RE-FALLBACK — GATECH art]** GATECH building SHP + cameo are absent on this
  partial install (Soviet/Yuri Battle-Lab art present; base Allied building SHPs
  absent). Building loader keeps GATECH in `ba.roster` with `art.ok=false` and
  renders a placeholder; the demo still places it. **SREF art is fully present —
  no fallback needed there.**
- **[DEFERRED — documented, not implemented]** Only the Allied player tech tree
  is demonstrated; Soviet/Yuri `NATECH`/`YATECH` are data-present but not
  demonstrated. `BuildLimit`, `Prerequisite.Negative`, engineer-capture, and
  building-upgrade prereq effects are documented but deferred.
