# Broaden Unit/Building Roster — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the content-breadth arc (CB-5 GATECH/SREF append pattern, `kRoster` append-last precedent, `techtree`/`base` witness-count re-pin precedent) + the Skirmish shell phase (`docs/superpowers/specs/2026-07-20-skirmish-design.md` — provides `launch_skirmish`/`SkirmishConfig`, `GameScene`, the scene driver) + the Multi-house phase (`HouseRec`, `set_house`, `house_side`, `enable_ai_house`) + the base World/render/combat engine (`spawn_base`, `add_combat_unit`, `add_building`, `resolve_combat_unit`, `pal_for_house`).
**Calibration authority:** `docs/re/roster.md` (the `rulesmd.ini` add-batch + real stats + `ra2extract` art-presence recovery hunt — ALREADY COMPLETE; real rules at `D:\ra2-re\out\shroud_evidence\rulesmd.ini`, 742,958 bytes; art probed against this exact install's `ra2.mix`/`ra2md.mix`/`language.mix`). This spec is built on recovered original-game behavior + the controller's binding design decisions (D1–D7), not design guesswork.

## Goal

The roster is narrow: the engine wires ~14 unit ids and a 20-entry building `kRoster`, so most of the buildable Allied/Soviet cast never resolves. This phase **broadens the roster by 16 calibrated, art-present types** — 7 Soviet units, 5 Allied units, and 4 buildings — and makes the Allied AI/skirmish base **symmetric** with the Soviet one, using **append-only content growth** (the CB-5 pattern). This phase delivers:

- **16 new calibrated content types** — 4 buildings appended to `kRoster` (art-guarded, never dropped) and 12 units in a NEW content-unit roster table `kRosterUnits` (resolved generically via `resolve_combat_unit`, honoring `Image=` overrides). Every type's stats/prereq/cost/tech/weapon are copied verbatim from `roster.md §2`; all 16 are art-present in this install → they render for real.
- **A symmetric Allied base** — `spawn_base` gains a NEW trailing default-false `bool allied_full` parameter; when true (and the house is Allied) it places GAPOWR/GAPILE/GAWEAP/GAREFN(+GAAIRC) mirroring the Soviet arm 1:1, with the Allied refinery spawning a CMIN (Chrono Miner). This fixes the Phase-3 Allied-ConYard-only carry.
- **A new 18th gate — `--demo-roster`** — resolves + spawns all 16 new types (buildings via the Allied full base + explicit `add_building`; units via `add_combat_unit`), folds a `roster_fold`, and does a two-run compute-and-self-compare with wedge guards and NO pinned literal.

**Byte-identity promise.** Roster growth is **APPEND-ONLY**: the 4 buildings go at the END of `kRoster` (indices 20–23), the 12 units go in a brand-new `kRosterUnits` table, and **`kUnits` (player slots 0–4 incl. SREF) and `kSoviet` (the AI proxy roster) are UNTOUCHED**. Every existing roster index is preserved, so every existing spawn folds the identical `Building::type`/unit type → `world_hash` (which folds spawned INSTANCES by roster index, not the roster table) is unperturbed. The symmetric Allied base sits behind a NEW default-false `allied_full` flag; every existing caller omits it (identical output) and no `else` is added to the existing `if(soviet)` block. All **17 existing byte-identity gates stay bit-identical**; the feature adds an 18th, `--demo-roster` (fixed `dt = 1/15`, self-compare, no pinned literal). Each type's **unique active ability is content-only-deferred** (stats/weapon/armor/prereq/art/buildability + basic combat land this phase; the special mechanic is a documented carry-forward).

## Global Constraints

Copied VERBATIM from the controller's design direction. These bind every task.

- All 17 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661, multihouse 39412ae996cce8ae, skirmish e9ad3f1fd88495f9. Adds the 18th: `--demo-roster`.
- `world_hash` untouched (folds spawned INSTANCES by roster index + unit cell/hp/alive — NOT the roster table). Roster growth is APPEND-ONLY: new ids go at the END of kRoster / a new content-unit table, so every existing index is preserved → existing spawns fold identical `Building::type` → all 17 hashes unchanged.
- **kUnits (player, slots 0-4 incl SREF) and kSoviet (AI proxy) MUST stay untouched** — they feed --demo-prod slots + ai-base/ai-team hashes. New units go in a NEW content-unit table, NOT into kUnits or kSoviet.
- **Symmetric Allied base behind a NEW default-false spawn_base parameter** — every existing caller leaves it default (identical output → identical hashes); only the new --demo-roster gate (and optionally skirmish) sets it true. Do NOT add an `else` to the existing `if(soviet)` block.
- The `roster=N` WITNESS COUNT in --demo-base + --demo-techtree grows by the number of new BUILDINGS (both runs produce the same N → deterministic still passes; base_hash/techtree_hash UNCHANGED). Re-pin the documented count in README for those two gates (a content-count doc update, NOT a hash change — the CB-5 18→19→20 precedent).
- Demo gate = two-run compute-and-self-compare, NO pinned literal (README pins), wedge guards → exit 1.
- No assets/GPL/Ghidra raw output committed. Recovered CONSTANTS (unit/building stats, prereqs) ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the content contract)

Every value below is tagged as in `docs/re/roster.md` ([CONFIRMED] = read straight from the real `rulesmd.ini` or an `ra2extract` probe of this exact install; [ANCHORED] = corroborated by documented RA2/OpenRA structure; [RE-FALLBACK] = a port design decision — batch selection, art-guarding, APOC-promote path, spawn_base symmetry mapping). Task 1 re-adjudicates the [RE-FALLBACK] items (last section) before code. **Every stat, prereq, weapon value, side, and art verdict in the tables is [CONFIRMED]** (`roster.md §5`).

Side is from `Owner=` (`roster.md §2`): Soviet = Russians/Confederation/Africans/Arabs; Allied = British/French/Germans/Americans/Alliance. [CONFIRMED]

### The ADD BATCH — 16 core types (all art-present, all stats [CONFIRMED])

Primary weapon columns are Damage / ROF / Range / Warhead. "Image=" is the art override honored for the render/probe (`roster.md §0`, §2). "Art" = main art present in this install (`roster.md §4`). "Ability deferred?" names the unique active mechanic that is content-only-deferred this phase (D5 / see Scope).

**Soviet units** (`roster.md §2`):

| id | name | side | prereq | tech | cost | str | armor | speed | sight | Dmg/ROF/Rng/Warhead | Image= | art | ability deferred? |
|---|---|---|---|---|---|---|---|---|---|---|---|:--:|---|
| DRON | Terror Drone | Soviet | NAWEAP | 4 | 500 | 100 | special_1 | 10 | 4 | 50 / 60 / 1.83 / Parasite (DroneJump) | DRON | Y | burrow-into-vehicle parasite |
| V3 | V3 Launcher | Soviet | NAWEAP,NARADR | 3 | 800 | 150 | light | 4 | 7 | 1 / 150 / 18 / Special (V3Launcher, Spawns V3ROCKET) | V3 | Y | rocket-projectile flight |
| ZEP | Kirov Airship | Soviet | NAWEAP,NATECH | 10 | 2000 | 2000 | medium | 5 | 8 | 250 / 50 / 1.5 / BlimpHE (BlimpBomb) | ZEP | Y | bomb-drop + slow jumpjet flight |
| SHK | Shock/Tesla Trooper | Soviet | NAHAND | 5 | 500 | 130 | Plate | 4 | 6 | 50 / 60 / 3 / Shock (ElectricBolt) | SHK | Y | power-up Tesla coil + EMP |
| DESO | Desolator | Soviet | NAHAND,RADAR | 8 | 600 | 150 | plate | 4 | 6 | 125 / 50 / 6 / RadBeamWarhead | DESO | Y | radiation field + deploy fire |
| APOC | Apocalypse (promote proxy) | Soviet | NAWEAP,NATECH | 7 | 1750 | 800 | heavy | 4 | 6 | 100 / 80 / 5.75 / ApocAP (120mmx) | MTNK | Y | dual weapon (AA MammothTusk) |
| TERROR | Terrorist | Soviet | NAHAND,RADAR | 5 | 200 | 75 | flak | 6 | 9 | 225 / 10 / 1.5 / TerrorBombWH (TerrorBomb) | TRST | Y | suicide detonation |

**Allied units** (`roster.md §2`):

| id | name | side | prereq | tech | cost | str | armor | speed | sight | Dmg/ROF/Rng/Warhead | Image= | art | ability deferred? |
|---|---|---|---|---|---|---|---|---|---|---|---|:--:|---|
| GHOST | Navy SEAL | Allied | GAPILE,RADAR | 9 | 1000 | 125 | flak | — | 8 | 125 / 10 / 6 / HollowPoint (MP5) | SEAL | Y | C4 demolition + amphibious |
| CLEG | Chrono Legionnaire | Allied | GAPILE,TECH | 10 | 1500 | 125 | none | 5 | 8 | 8 / 120 / 5 / ChronoBeam (NeutronRifle) | CLEG | Y | teleport-erase + freeze |
| JUMPJET | Rocketeer | Allied | GAPILE,RADAR | 3 | 600 | 125 | none | 9 | 8 | 25 / 30 / 5 / SSA (20mm) | ROCK | Y | jumpjet air (use aircraft path if Jumpjet CLSID resolves) |
| MGTK | Mirage Tank | Allied | GAWEAP,GATECH | 9 | 1000 | 200 | light | 7 | 9 | 100 / 70 / 7 / MirageWH (MirageGun) | RTNK | Y | tree-disguise when still |
| ORCA | Nighthawk/Intruder | Allied | RADAR | 3 | 1200 | 150 | light | 14 | 8 | 150 / 10 / 6 / ORCAAP (Maverick) | FALC | Y | transport + dock/rearm (needs GAAIRC) |

**Buildings** (`roster.md §2`) — Damage/ROF/Range/Warhead columns are the defensive weapon where present:

| id | name | side | prereq | tech | cost | str | armor | sight | power | weapon (Dmg/ROF/Rng/WH) | Image= | art | ability deferred? |
|---|---|---|---|---|---|---|---|---|---|---|---|:--:|---|
| NANRCT | Nuclear Reactor | Soviet | NATECH,NACNST | 9 | 1000 | 1000 | concrete | 5 | +2000 | — | NANRCT | Y | huge power output (generic power path) |
| ATESLA | Prism Tower | Allied | POWER,RADAR,GACNST | 6 | 1500 | 600 | steel | 8 | −75 | 120 / 45 / 8 / PrismWarhead (PrismShot) | GAPRIS | Y | prism beam chain |
| NASAM | Patriot Missile | Allied | BARRACKS,GACNST | 4 | 1000 | 900 | steel | 10 | −50 | 75 / 55 / 12 / SAMWH (RedEye2, AA) | NASAM | Y | anti-air targeting |
| GAAIRC | Airforce Command HQ | Allied | GAREFN,GACNST | 3 | 1000 | 600 | steel | 5 | −50 | — | GAAIRC | Y | radar + aircraft factory + ORCA dock |

`Image=` overrides that MUST be honored (`roster.md §0`): APOC→MTNK, TERROR→TRST, MGTK→RTNK, JUMPJET→ROCK, ORCA→FALC, GHOST→SEAL, ATESLA→GAPRIS. (The other new ids draw under their own name.)

### Allied base set for spawn_base symmetry (`roster.md §3`)

`spawn_base` (`main.cpp:1785`) currently builds a full base only for Soviet houses; Allied houses get a ConYard (GACNST) only. The symmetric Allied set, mapped 1:1 from the Soviet placements, all stats [CONFIRMED]:

| Soviet (wired) | Allied | name | prereq | tech | cost | str | power | factory |
|---|---|---|---|---|---|---|---|---|
| NACNST | GACNST | Construction Yard | (none) | -1 | 3000 | 1000 | 0 | BuildingType |
| NAPOWR | GAPOWR | Power Plant | GACNST | 1 | 800 | 750 | +200 | — |
| NAHAND | GAPILE | Barracks | POWER,GACNST | 2 | 500 | 500 | −10 | InfantryType |
| NAWEAP | GAWEAP | War Factory | PROC,GAPILE,GACNST | 2 | 2000 | 1000 | −25 | UnitType |
| NAREFN | GAREFN | Ore Refinery | POWER,GACNST | 1 | 2000 | 1000 | −50 | FreeUnit **CMIN** |
| — | GAAIRC | Airforce Cmd HQ (radar) | GAREFN,GACNST | 3 | 1000 | 600 | −50 | AircraftType |

All six are ALREADY in `kRoster` EXCEPT GAAIRC (which this phase adds). Allied refinery FreeUnit = **CMIN** (Chrono Miner), vs the Soviet HARV. Placements mirror the Soviet arm: `place(GACNST)`; then GAPOWR `(cx+6,cy)`, GAPILE `(cx+6,cy+4)`, GAWEAP `(cx-1,cy+6)`, and with_refinery → GAREFN `(cx+9,cy+4)` + free CMIN at its dock. Art all present in `ra2.mix/generic.mix` (`roster.md §4`). [CONFIRMED ids/stats; RE-FALLBACK mapping]

### Dropped — already rostered, do NOT re-add (`roster.md §1`)

The engine's `kRoster[20]` (`main.cpp:1451`) already contains **TESLA (Tesla Coil), NAFLAK (Flak Cannon), NABNKR (Battle Bunker), GAPILL (Allied Pillbox), NALASR (Soviet Sentry Gun)** — buildings — and **HTK (Flak Track)** is already in the Soviet unit roster `kSoviet` (`main.cpp:1261`). The phase brief's candidate list included Tesla Coil/Flak Cannon/Battle Bunker as adds; they are dropped from the batch because they are already rostered. [CONFIRMED]

### Deferred blockers (documented, not in this batch — `roster.md §5`)

- **GASPYSAT** — art-present, but rostering it triggers the shroud-phase T7 reveal-ordering carry (`SpySat=` whole-map reveal, `main.cpp:1518`). DEFER to keep the phase bounded.
- **GAYARD (Naval Yard) + DEST (Destroyer) + AEGIS (Aegis Cruiser)** — the naval pair needs the not-yet-rostered `GAYARD` prereq. DEFER to a naval phase. (DEST/AEGIS main art IS present; only the prereq building is missing.)
- **ROBO / GAROBO (Robot Control Center)** — `GAROBO` art ABSENT everywhere in this install → cannot resolve the prereq. DEFER.
- **Yuri faction (NATBNK / YA\*)** — `Owner=YuriCountry`, out of Allied/Soviet scope; `NATBNK` art absent (Yuri faction). DEFER.
- **TTNK (Tesla Tank), GTGCAN (Grand Cannon)** — art-present drop-in extras beyond the core 16; not in the batch (bounded to 16). Available if T1 elects to include them, but the default batch is the 16 above.

## Current-engine reality (what we build on)

From the survey — file:line anchors (`main.cpp` unless noted). None is a sim invariant this phase alters; these are the seams the new content + gate attach to.

- **Building roster `kRoster[20]` at `main.cpp:1451`** — append-last (CB-5 added GATECH@18; capture added CAOILD@19). The building resolve loop `1510–1591` builds a `BuildingTypeDef` from Rules/Art; on art-FAIL it KEEPS the def with `slot.ok=false` (**buildings are never dropped**). `roster_index(ba,"ID")` maps an id → its `kRoster` index. `spawn_base` reaches buildings via `add_building(type,tx,py,fw,fh,house,strength,armor_ix,buildup,instant,sight,grants_map_reveal)` at `1798`.
- **Unit rosters** — `load_unit_roster_table` `1134–1219` builds `UnitRosterEnt{id,vehicle,prereq}` and **DROPS a unit** if `cost<=0 || tech<=0 || cameo empty`, or if `vehicle && !va.ok` (units ARE dropped on art-fail — hence the synthetic-fallback guard). `kUnits` (player) `1226–1241` = E1/GGI/MTNK/FV @0–3, SREF @4. `kSoviet` (AI proxy) `1257–1263` = E2/HTNK/APOC/HTK with `full_parse=false`, AI-only — **DO NOT TOUCH** (feeds ai-base/ai-team hashes + `--demo-prod` slots). The naval candidate loader is at `1087` (`{"HYD","DRED","DEST"}`); the aircraft loader is analogous.
- **`resolve_combat_unit` `613–708`** — resolves each unit's weapons/warheads/verses **GENERICALLY per-id from Rules** (no per-unit code): aircraft via the Jumpjet CLSID path `658–661`, naval via CLSID `695–698`. This is why a new unit needs only a roster row + calibrated Rules stats — its basic combat comes free.
- **Synthetic-fallback for a dropped unit** — the techtree gate probe `5589–5606` (SREF) shows the pattern: a dropped-on-art-fail unit can still resolve for tests via a synthetic def. Keep this guard for robustness even though all 12 new units are art-present.
- **`spawn_base` `1785–1833`** — `const bool soviet = house_side(house)==Soviet` (`1814`); the full base is gated ENTIRELY on `if(soviet)` (`1817–1831`); Allied houses get ConYard-only (there is NO `!soviet` branch). Feeds gate hashes → the new branch must be behind a default-false flag.
- **`world_hash` `4528–4544`** — folds spawned INSTANCES only (`Unit{cell,hp,alive}` + `Building{type,cx,cy,hp,state}`), NOT the roster table. Append-only roster growth cannot perturb it.
- **Gate harness** — demo dispatch `9020–9088`; `techtree_fold` `5517–5551` (fnv 0xcbf29ce484222325); `run_demo_naval` `7469` (the two-run compute-and-self-compare template). Structs: `BuildingTypeDef` `building.hpp:16–57`; `UnitCombat` `unit_combat.hpp:24–96`; `UnitTypeDef` `unit_type.hpp:13–22`. All roster tables live in `main.cpp` (no new module needed; the new content-unit table + the gate live in `main.cpp`).

## Architecture

Each component states what it does, its interface, its dependencies, and — critically — HOW it stays additive / byte-identical (append-only; `kUnits`/`kSoviet`/existing spawn callers untouched; `world_hash` folds instances not the roster).

### A. Four new buildings appended to `kRoster` (D1)

**What:** append `NANRCT`, `ATESLA`, `NASAM`, `GAAIRC` to `kRoster[20]` → `kRoster[24]`, occupying indices 20–23. The existing resolve loop (`1510–1591`) picks them up generically from Rules/Art — stats, prereq, power, cost, sight, armor, and the defensive weapon (ATESLA PrismShot, NASAM RedEye2) all flow from the calibrated Rules constants. All four are art-present (`roster.md §4`) → real render; the art-guard keeps them even if a future install lacks the SHP.

**Interface:** four new `kRoster` entries (id + the Rules-backed `BuildingTypeDef` fields); reachable via `roster_index(ba,"NANRCT"|"ATESLA"|"NASAM"|"GAAIRC")`.

**Dependencies:** the building resolve loop, `add_building`, the calibrated Rules stats (`roster.md §2`).

**Byte-identity:** APPEND-ONLY at the end of `kRoster` → every existing index (0–19) is preserved → every existing `spawn_base`/gate spawn folds the identical `Building::type`. The resolve loop keeps buildings on art-fail (`slot.ok=false`) so the table size grows deterministically. The only observable effect on existing gates is the `roster=N` witness COUNT in `--demo-base`/`--demo-techtree`, which grows by 4 (both runs agree → base_hash/techtree_hash UNCHANGED; re-pin the doc count only).

### B. Twelve new units in a NEW content-unit table `kRosterUnits` (D1)

**What:** a brand-new roster table `kRosterUnits[]` of `UnitRosterEnt{id,vehicle,prereq}` rows for the 12 units (7 Soviet + 5 Allied), consumed by the `--demo-roster` gate (and available to skirmish later via `launch_skirmish`). `resolve_combat_unit` resolves each generically — DroneJump/V3Launcher/BlimpBomb/ElectricBolt/RadBeamWeapon/120mmx/TerrorBomb/MP5/NeutronRifle/20mm/MirageGun/Maverick weapons and their warheads come free per-id from Rules. `Image=` overrides are honored (APOC→MTNK, TERROR→TRST, MGTK→RTNK, JUMPJET→ROCK, ORCA→FALC, GHOST→SEAL). All 12 are art-present → resolve + render for real; the synthetic-fallback guard is retained for robustness.

**Interface:** `static const UnitRosterEnt kRosterUnits[] = {…}`; loaded by the same `load_unit_roster_table` machinery (or an equivalent generic pass) into resolved `UnitCombat`/`UnitTypeDef` defs; consumed by `add_combat_unit` in the gate.

**Dependencies:** `resolve_combat_unit` (`613–708`), `load_unit_roster_table` (`1134–1219`), the calibrated Rules stats.

**Byte-identity:** `kRosterUnits` is a SEPARATE new table — **`kUnits` and `kSoviet` are not modified**. `--demo-prod` still draws its player slots from the unchanged `kUnits`; ai-base/ai-team still fold the unchanged `kSoviet` proxy roster. No existing unit index moves. `resolve_combat_unit` is called with new ids only from the new gate; existing callers pass the same ids as before.

**APOC promotion (keeps kSoviet untouched):** APOC exists today only as a hardcoded AI proxy token (`APOC→NAWEAP`, `kSoviet` slot, `main.cpp:1159` `ai_team_hash` caveat). This phase promotes APOC to a **real content type in `kRosterUnits`** with its recovered dual prereq `Prerequisite=NAWEAP,NATECH` — WITHOUT editing the `kSoviet` proxy entry (which stays as-is so ai-base/ai-team hashes are unperturbed). The content APOC (in `kRosterUnits`) and the proxy APOC (in `kSoviet`) coexist; the gate exercises the content one. [ANCHORED promotion path]

### C. Symmetric Allied base behind `allied_full` flag (D2)

**What:** add a NEW trailing default-false parameter `bool allied_full=false` to `spawn_base`. When `allied_full && !soviet`, place the Allied full base — GAPOWR/GAPILE/GAWEAP/GAREFN(+GAAIRC) — mirroring the Soviet placements 1:1 (§3 offsets); `with_refinery` spawns a **CMIN** (Chrono Miner) at the GAREFN dock (Allied FreeUnit), vs the Soviet HARV. GAAIRC (added to `kRoster` in A) also satisfies the Allied `RADAR` prereq category and the ORCA dock. The existing `if(soviet)` block is UNCHANGED — the Allied branch is a NEW separate `if(allied_full && !soviet)` block, NOT an `else`.

**Interface:** `spawn_base(..., bool allied_full=false)`; new callers that want a real Allied base pass `true` (the `--demo-roster` gate; optionally the skirmish `GameScene`).

**Dependencies:** `add_building`, `roster_index`, the six base-set ids in `kRoster` (GAAIRC newly added).

**Byte-identity:** the new parameter is DEFAULT-FALSE and TRAILING → every existing caller's call site is unchanged and produces byte-identical output. No `else` on `if(soviet)` means the Soviet path is textually and behaviorally identical. Only the new gate (and optionally skirmish, an unhashed launch path) passes `true`. The 17 gates that call `spawn_base` for Allied houses still get ConYard-only.

### D. `--demo-roster` gate — the 18th (D3)

**What:** a new gate arm mirroring `run_demo_naval`/`run_demo_multihouse`. It (1) resolves each of the 16 new types from Rules and asserts cost/tech/prereq + a key weapon/stat per type; (2) spawns instances into a fresh `World` — the 4 buildings via the Allied full base (`spawn_base(..., allied_full=true)` for an Allied house) plus explicit `add_building` for NANRCT/ATESLA/NASAM/GAAIRC, and the 12 units via `add_combat_unit`; (3) folds a `roster_fold` (per-instance type/cell/hp/alive + per-new-type resolved cost/tech/strength/damage sample + resolved count); (4) steps N frames at a FIXED `dt = 1/15`; (5) two-run compute-and-self-compare (run1 rendered+PNG, run2 headless fresh `World`), `identical = (witness && hash)`; wedge guards → exit 1; NO pinned literal (README pins the hash).

**Interface:** new arm in the demo dispatch (`9020–9088`), after the 17th gate arm and before the interactive fallthrough; `roster_fold` helper (fnv, same seed as `techtree_fold`).

**Dependencies:** the new `kRoster` buildings (A), `kRosterUnits` (B), `spawn_base(allied_full=true)` (C), `add_combat_unit`/`add_building`, the two-run self-compare harness.

**Byte-identity:** a new gate arm adds no code to any existing gate's path and is the only caller of `spawn_base(allied_full=true)` inside the harness. `world_hash` + all 17 prior `*_hash` untouched.

### E. Content-only abilities deferred (D5)

**What:** wire each type's STATS/WEAPON/ARMOR/PREREQ/ART/BUILDABILITY + basic combat via the generic weapon path (`resolve_combat_unit`). The UNIQUE ACTIVE ABILITY of each type is content-only-deferred (each is its own future mini-phase, matching how the CB arc added breadth). Building "abilities" that ARE stats land this phase: NANRCT power (+2000 via the generic power path), ATESLA/NASAM defensive weapons (via the generic weapon path). See Scope for the explicit per-type deferred list.

**Byte-identity:** deferring the special mechanics means NO new per-unit code paths, NO changes to combat resolution beyond the generic per-id path — nothing that could perturb an existing hash.

### F. Optional skirmish `allied_full` wiring (D6)

**What:** wire the skirmish `GameScene` launch (`launch_skirmish` / `spawn_base`) to pass `allied_full=true` for Allied houses, so a skirmish Allied player gets a real base (fixes the Phase-3 Allied-ConYard-only carry). This is the interactive/launch path (unhashed).

**Byte-identity:** the `--demo-*` gates keep `allied_full=false` (default) → byte-identical. OPTIONAL — include only if it doesn't risk a gate; if any doubt, defer to a carry-forward. This is a light integration, not a phase-critical deliverable.

## Data model (exact table/param additions)

| Addition | On | Shape / default | In `world_hash`? | Why additive/inert |
|---|---|---|---|---|
| 4 new `kRoster` entries (indices 20–23) | `kRoster` | NANRCT, ATESLA, NASAM, GAAIRC — each a Rules-backed `BuildingTypeDef` (id/prereq/tech/cost/str/armor/sight/power/weapon) | No (folds instances by index, not the table) | APPEND-ONLY at the end; every existing index 0–19 preserved. |
| `static const UnitRosterEnt kRosterUnits[]` | (new table) | 12 rows `{id, vehicle?, prereq}` for the 7 Soviet + 5 Allied units; `Image=` overrides honored at resolve | No | Brand-new table; `kUnits`/`kSoviet` untouched; consumed only by the new gate (+ skirmish later). |
| `bool allied_full = false` | `spawn_base(...)` param (trailing) | default false; true → Allied full base mirroring the Soviet arm, refinery FreeUnit = CMIN | No | Default-false trailing param; every existing caller omits it → identical output. |
| `roster_fold(...)` | (new gate helper) | fnv over per-instance type/cell/hp/alive + per-new-type resolved cost/tech/strength/damage sample + resolved count | Its own gate hash only | New gate-local fold; does not touch `world_hash` or any prior fold. |

**`world_hash` is untouched.** It folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` by roster index. Append-only roster growth preserves every existing index, so every existing spawn folds identically. The `roster=N` witness COUNT in `--demo-base` + `--demo-techtree` grows by **4** (the new buildings); both runs of each deterministic gate produce the same N, so `base_hash`/`techtree_hash` are UNCHANGED — only the documented count is re-pinned in the README (the CB-5 18→19→20 precedent).

## Behavior details (each tied to the recovered §)

- **Generic combat resolution** (§2, D1/E): `resolve_combat_unit` (`613–708`) resolves each new type's weapon(s) and warhead GENERICALLY per-id from the calibrated Rules — no per-unit code. DRON→DroneJump/Parasite, V3→V3Launcher/Special, ZEP→BlimpBomb/BlimpHE, SHK→ElectricBolt/Shock, DESO→RadBeamWeapon/RadBeamWarhead, APOC→120mmx/ApocAP, TERROR→TerrorBomb/TerrorBombWH, GHOST→MP5/HollowPoint, CLEG→NeutronRifle/ChronoBeam, JUMPJET→20mm/SSA, MGTK→MirageGun/MirageWH, ORCA→Maverick/ORCAAP. Each new type's **basic combat** works this phase via that path; the special mechanic behind the warhead (parasite/erase/radiation/suicide/etc.) is content-only-deferred. [CONFIRMED weapon values]
- **Image= overrides** (§0, D1): the resolver draws each id under its recovered `Image=` (APOC→MTNK, TERROR→TRST, MGTK→RTNK, JUMPJET→ROCK, ORCA→FALC, GHOST→SEAL, ATESLA→GAPRIS). The art probe used the Image, not the id, so all are confirmed present. [CONFIRMED]
- **Art-guard** (§4, D1): buildings are KEPT on art-fail (`slot.ok=false`, resolve loop `1510–1591`) — they never drop; units are DROPPED on art-fail (`load_unit_roster_table` `1134–1219`) but ALL 12 new units are art-present, so none drops. The synthetic-fallback guard (techtree probe pattern `5589–5606`) is retained for robustness. Some new types have NO sidebar cameo in this install (JUMPJET/AEGIS/NANRCT/ATESLA/NASAM/GTGCAN + several buildings) → the cameo lookup falls back to placeholder; the unit/building itself is still real and functional. [CONFIRMED]
- **Symmetric Allied base placements** (§3, D2): with `allied_full=true` and an Allied house, `spawn_base` places GACNST, then GAPOWR `(cx+6,cy)`, GAPILE `(cx+6,cy+4)`, GAWEAP `(cx-1,cy+6)`, and with_refinery GAREFN `(cx+9,cy+4)` + a free CMIN at the dock — mirroring the Soviet arm 1:1. GAAIRC (radar) is placed alongside so ORCA's dock + the Allied RADAR prereq resolve. [CONFIRMED ids/stats; RE-FALLBACK offsets mirror the Soviet arm]
- **Prereq chains** (§5): APOC resolves NAWEAP,NATECH (verify NATECH is a buildable/rostered tech gate or the tech tree stalls); ORCA resolves RADAR via the newly-rostered GAAIRC (+ uses it as its dock); ATESLA/GHOST/JUMPJET/CLEG resolve their Allied RADAR/TECH prereqs through GAAIRC/GATECH. NASAM's BARRACKS prereq resolves via GAPILE. [CONFIRMED prereq categories]
- **Building power/defense as stats** (§2, D5): NANRCT contributes +2000 power via the generic power path; ATESLA (PrismShot) and NASAM (RedEye2 AA) fire via the generic defensive-weapon path. These are building STATS, not deferred abilities. [CONFIRMED]

## Scope / non-goals (carry-forwards — document, do NOT implement)

**Each unique active ability is content-only-deferred this phase** (D5/D7) — stats/weapon/armor/prereq/art/buildability + basic combat land now; the special mechanic is its own future mini-phase:

| Type | Deferred unique active ability |
|---|---|
| DRON — Terror Drone | burrow-into-vehicle parasite (Parasite warhead mechanic) |
| V3 — V3 Launcher | rocket-projectile flight (V3ROCKET spawned-missile) |
| ZEP — Kirov Airship | bomb-drop + slow jumpjet flight |
| SHK — Tesla Trooper | power-up a friendly Tesla coil + EMP secondary |
| DESO — Desolator | radiation field + deploy-fire (RadEruptionWeapon) |
| TERROR — Terrorist | suicide detonation (DeathWeapon TerrorBomb) |
| APOC — Apocalypse | dual weapon (AA MammothTusk secondary) |
| GHOST — Navy SEAL | C4 demolition + amphibious movement |
| CLEG — Chrono Legionnaire | teleport-erase (ChronoBeam) + target freeze |
| JUMPJET — Rocketeer | jumpjet air movement — **allow if the Jumpjet CLSID resolves** via the existing aircraft path (`resolve_combat_unit` `658–661`); otherwise deferred |
| MGTK — Mirage Tank | tree-disguise when still (DisguiseWhenStill) |
| ORCA — Nighthawk | transport + dock/rearm (needs GAAIRC dock) |

Other explicit non-goals (documented, not implemented):

| Carry-forward | Recovered ref | Why deferred |
|---|---|---|
| **GASPYSAT + shroud-T7 reveal-ordering fix** | §5 [CONFIRMED art / carry] | Art-present but rostering triggers the shroud-phase T7 whole-map reveal carry (`SpySat=`, `main.cpp:1518`); keep the phase bounded. |
| **GAYARD (Naval Yard) + DEST + AEGIS naval pair** | §2/§5 [CONFIRMED stats] | The naval pair needs the not-yet-rostered `GAYARD` prereq; defer to a naval phase. |
| **ROBO / GAROBO** | §4 [CONFIRMED art absent] | `GAROBO` art ABSENT everywhere → prereq cannot resolve; defer. |
| **Yuri faction (NATBNK / YA\*)** | §1/§4 [CONFIRMED] | `Owner=YuriCountry`; out of Allied/Soviet scope; `Side::Yuri` unpopulated; NATBNK art absent. |
| **Per-country bonuses** | §1 [ANCHORED] | Country-specific stat modifiers not modeled; sides only. |
| **Remaining ~70 cast members** | §1 [CONFIRMED counts] | The buildable Allied/Soviet combat cast is ~40; the rest are civilians/heroes/props — future breadth phases. |
| **TTNK / GTGCAN drop-in extras** | §2/§4 [CONFIRMED] | Art-present beyond the core 16; available if T1 elects to include, else deferred to keep the batch at 16. |
| **All prior-phase carry-forwards** | — | GameSpeedBias movement reconciliation, map browser / MIX map enumeration, multiplayer netcode, load/save, campaign, multi-human input, etc. (inherited from the skirmish + multihouse phases). |

## Testing strategy

- **Pure host tests** (doctests, no install required) for the resolved content:
  - **Each new type resolves with the recovered stats:** each of the 12 units resolves via `resolve_combat_unit` with the calibrated cost/tech/prereq + a key weapon value (Damage/ROF/Range/Warhead) from `roster.md §2`; the `Image=` override is applied (APOC→MTNK, TERROR→TRST, MGTK→RTNK, JUMPJET→ROCK, ORCA→FALC, GHOST→SEAL).
  - **Buildings resolve art-guarded:** each of the 4 new buildings resolves from `kRoster` with its cost/tech/prereq/power/sight/armor + defensive weapon (ATESLA PrismShot, NASAM RedEye2) and is KEPT even under a simulated art-fail (`slot.ok` semantics); ATESLA→GAPRIS Image applied.
  - **The Allied base set resolves:** `spawn_base(..., allied_full=true)` for an Allied house places >1 building (GACNST + GAPOWR/GAPILE/GAWEAP + GAAIRC, +GAREFN with_refinery), and the refinery FreeUnit is CMIN; an Allied house with `allied_full=false` (default) still gets ConYard-only (behavior-preserving).
- **New 18th gate `--demo-roster`** (mirror `run_demo_naval`): resolve + assert all 16 new types → spawn into a fresh `World` (buildings via the Allied full base + explicit `add_building`; units via `add_combat_unit`) → step N frames at fixed `dt = 1/15` → two-run compute-and-self-compare (run1 rendered+PNG, run2 headless fresh `World`), `identical = (witness && hash)`, **NO pinned literal** (README pins), wedge guards → exit 1. Wedge guards: every new type resolvable + spawnable; the Allied full base placed >1 building; a spot stat matches Rules (e.g. APOC strength 800 / ATESLA is a building with a defensive weapon). Fold `roster_fold`: per-instance type/cell/hp/alive + per-new-type resolved cost/tech/strength/damage sample + resolved count. `world_hash` + all 17 prior `*_hash` untouched.
- **The 17 existing gates stay byte-identical** — re-verify at T2 (buildings appended — roster count grows, hashes EXACT), T3 (units in `kRosterUnits` — additive), T4 (Allied base behind `allied_full` — existing callers default-false byte-identical), and finalize T7 (full 18-gate sweep). **One gate per call** (each AI gate ≈2 min; batching exceeds the tool timeout — gate-runtime discipline). **Re-pin the `roster=N` witness COUNT** in the README for `--demo-base` + `--demo-techtree` (grows by 4 new buildings; base_hash/techtree_hash UNCHANGED — a content-count doc update, not a hash change).

## [RE-FALLBACK] items for Task 1 RE-GATE to adjudicate

Task 1 is a **confirmatory RE-GATE adjudication** (re-probe the batch stats/prereqs + art-presence via `ra2extract`; flip `[RE-FALLBACK]`/`[ANCHORED]` → adjudicated), NOT a fresh hunt. The items to close before code:

1. **Final batch membership** (§2/§5, D1): confirm all 16 core types are art-present AND resolvable from Rules (re-probe DRON/V3/ZEP/SHK/DESO/APOC/TERROR + GHOST/CLEG/JUMPJET/MGTK/ORCA + NANRCT/ATESLA/NASAM/GAAIRC via `ra2extract`, honoring `Image=`). **Drop any type that fails** to resolve or whose art is missing. Adjudicate whether the drop-in extras TTNK/GTGCAN join the batch or stay deferred (default: batch stays at 16).
2. **Which abilities are content-only-deferred** (§2, D5): confirm the full deferred list (the 12-row table above) — each type ships stats/weapon/armor/prereq/art/buildability + basic combat, and its unique active mechanic is a documented carry-forward. Specifically adjudicate JUMPJET's jumpjet-air: ALLOW it this phase IF the Jumpjet CLSID resolves via the existing aircraft path (`resolve_combat_unit` `658–661`), else defer.
3. **The Allied base set + placements** (§3, D2): confirm the six-building Allied set (GACNST/GAPOWR/GAPILE/GAWEAP/GAREFN + new GAAIRC), that all but GAAIRC are already in `kRoster`, that the refinery FreeUnit is CMIN (Chrono Miner), and that the mirrored placement offsets (GAPOWR cx+6,cy / GAPILE cx+6,cy+4 / GAWEAP cx-1,cy+6 / GAREFN cx+9,cy+4) match the Soviet arm 1:1.
4. **The APOC-promote approach that leaves kSoviet untouched** (§2, D1): confirm APOC is promoted to a real content type in the NEW `kRosterUnits` table with its dual prereq NAWEAP,NATECH, while the `kSoviet` AI-proxy APOC entry (`main.cpp:1261`, `ai_team_hash` caveat `1159`) is left byte-for-byte unchanged so ai-base/ai-team hashes are unperturbed — and verify NATECH is a rostered/buildable tech gate so the APOC prereq chain does not stall.
