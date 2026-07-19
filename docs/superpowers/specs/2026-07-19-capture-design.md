# Engineer / Spy Capture — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the full content-breadth arc (CB-1..CB-5) + AI-1 (standing-building↔base-node sync) + CB-4 (per-house superweapon recharge timer) + the Economy phase (`house_credits_`, income) + Unit Production (the four production queues + `prerequisites_met`).
**Calibration authority:** `docs/re/capture.md` (the Ghidra + rulesmd recovery hunt — ALREADY COMPLETE; raw dumps in `D:\ra2-re\out\capture_*`). This spec is built on recovered original-game behavior, not design guesswork.

## Goal

Add the two RA2 building-interaction mechanics that share a "unit enters an enemy/neutral building" trigger:

1. **Engineer capture** — an `ENGINEER` transfers a building's ownership to its house (and captures neutral tech structures), matching `BuildingClass::ChangeOwner`.
2. **Spy infiltration** — a `SPY` produces a per-building-type effect (cash steal, power blackout, superweapon-timer reset, produced-unit veterancy), matching `BuildingClass::Infiltrate`, with the spy's **disguise** (appears as an enemy infantry type) and **detection** (dogs see through it).

Demonstrated deterministically and guarded by the **14th permanent byte-identity gate `--demo-capture`** (`capture_hash`). The 13 existing gates stay bit-identical.

## Recovered ground truth (the behavior contract — from `docs/re/capture.md`)

Each item is CONFIRMED / PROBABLE / [RE-FALLBACK]; the plan's Task 1 RE-GATE re-adjudicates the PROBABLE/[RE-FALLBACK] items and pins any remaining values via `ra2extract` before code.

### Ownership transfer
- **`BuildingClass::ChangeOwner` (`FUN_004481ab`, vtable +0x3d4)** moves power / production / superweapon / radar from old house → new house; plays `CaptureEvaEvent`; grants `ProduceCashStartup` **only** when captured from the neutral house. [CONFIRMED]
- Invoked from the enter executor `FUN_00519449:236`. [CONFIRMED]

### Engineer capture
- What_Action (`FUN_0051e17c`): captures iff `HealthRatio ≤ EngineerCaptureLevel`. **`EngineerCaptureLevel` is ABSENT from YR-1.001 rules → ctor default `1.0` → the engineer captures at ANY health.** `EngineerDamage` / `EngineerAlwaysCaptureTech` are **Ares-era keys, not present in this build** (0 string hits). [CONFIRMED — overturns the pre-hunt "capture threshold / damage-only branch" assumption]
- Engineer is **always consumed** on capture. [CONFIRMED]
- Entering your OWN damaged building **repairs** it (threshold `Rules+0x16f8 = 1.0`). [PROBABLE]

### Neutral tech structures (all `Capturable=yes NeedsEngineer=yes`)
- `CAOILD` oil derrick: `ProduceCashStartup=1000` (one-time on capture-from-neutral), **`ProduceCashAmount=20` every `ProduceCashDelay=100` frames** (`FUN_00458731`). [CONFIRMED]
- `CATHOSP` (`InfantryGainSelfHeal=1`), `CAMACH` (`UnitsGainSelfHeal=1`), `CAPOWR` (`Power=200`), `CAAIRP` (`SuperWeapon=ParaDropSpecial`). [CONFIRMED values]
- The reference map `mapsmd03` is not parsed for pre-placed tech structures → **[RE-FALLBACK]: the demo spawns a `CAOILD`.**

### Spy infiltration — `BuildingClass::Infiltrate` (`FUN_004571e0`); early-returns if the spy already owns the building
- **Power plant → blackout `SpyPowerBlackout = 1000` frames (≈66.7 s @15fps)** (`FUN_0050bab0`). [CONFIRMED]
- **Refinery → steal `SpyMoneyStealPercent = 0.5` × victim credits** into the spy's house (`Rules+0xd68` @0x457441; `FUN_004f95b0/004f9770`). [CONFIRMED]
- **Superweapon → recharge-timer reset** (`FUN_006cd620`). [CONFIRMED — reuses CB-4's timer]
- Factory → future produced units gain **veterancy** (house-flag +0x577a). [PROBABLE — rank/bonus not pinned]
- Radar / Battle Lab → reveal flags. [PROBABLE]

### Disguise / detection
- Spy `PermaDisguise` by owner side: `AlliedDisguise=E1` / `SovietDisguise=E2` / `ThirdDisguise=INIT` (`FUN_006f34a0`); stored as object fields `DisguisedAsType(+0x518)` / `DisguisedAsHouse(+0x51c)` / flag(+0x1d8). [CONFIRMED]
- Re-disguise to a targeted unit via `Primary=MakeupKit`. [PROBABLE — exact site not pinned]
- **Detection: only `DetectDisguise=yes` units see through — the dogs `[ADOG]/[DOG]/[YDOG]` carry it.** `Detector=` is NOT a YR key (uses `DetectDisguise=` / `Sensors=`). [CONFIRMED]

### Determinism
- **No RNG** in capture / transfer / infiltrate / cash-steal paths. RNG only in mirage fake-blink detection (`DisabledDisguiseDetectionPercent`) and cosmetic grinder animations — neither in scope. [CONFIRMED] → the gate is bit-deterministic.

### Exemplars (all art present on this install)
- `ENGINEER` (`Engineer=yes`, engineer.shp), `SPY` (`Agent=yes`/`Infiltrate`/`CanDisguise`/`PermaDisguise`, spy.shp), `DOG` (dog.shp).

## Current-engine reality (what we build on)

- **Ownership is per-frame–derived, not cached.** `power_totals(buildings_, roster, house)` and `prerequisites_met(...)` scan the flat `std::vector<Building> buildings_` filtered by `b.house` on every query. **⇒ flipping `b.house` automatically re-homes power + buildability + the player sidebar** — no re-registration. (This is why Approach A below is correct.)
- Houses are `0` (player/Allied) and `1` (enemy/Soviet). Per-house arrays are size-2 (`house_credits_[2]`). A **neutral owner (house id 2)** must be added as a building-only owner tag; per-house arrays are widened to 3 with the neutral index inert (no credits/units/AI for neutral).
- Superweapons are **house-granted** (`sw_houses_`), with a reusable `Superweapon::tick`/reset. No shroud/fog exists. No veterancy exists. AI base-nodes link to standing buildings (AI-1 sync).

## Approved scope

| In scope | Out of scope (carry-forward) |
|---|---|
| Engineer capture of enemy buildings (ChangeOwner: flip house + move SW/AI-node/primary) | Bridge repair; engineer entering to garrison |
| Engineer capture of neutral tech structures + `CAOILD` income + self-heal/power benefits | Full neutral-structure roster placement from the map |
| Engineer repair of own damaged building (PROBABLE) | — |
| Spy: cash-steal, power-blackout, SW-reset (all CONFIRMED) | — |
| Spy → factory veterancy: infiltration flag **+ minimal real veteran combat bonus** on produced units | Promotion-by-kills, elite rank, chevron UI, self-heal/speed veteran bonuses |
| Spy → radar/lab reveal: **[RE-FALLBACK] inert** (no shroud) | Real radar/shroud reveal (needs a shroud phase) |
| Spy disguise (render as `AlliedDisguise`/`SovietDisguise` infantry) + dog `DetectDisguise` reveal + reveal-on-fire/attack (PROBABLE) | Mirage-tank disguise; MakeupKit click-to-re-disguise; fake-blink RNG |
| Neutral house (owner id 2, building-only) | Neutral units / neutral AI |
| 14th gate `--demo-capture` | — |

## Design

### 1. Ownership-transfer core — `capture_building(world, building_idx, new_house)`
Approach A (flip-the-flag + targeted fix-ups, matching `ChangeOwner`):
1. Set `b.house = new_house` — power/economy/buildability/sidebar follow for free (per-frame derived).
2. **SW move:** if the building granted a superweapon, move that grant between `sw_houses_`.
3. **AI node sync both sides:** the losing AI house drops the base-node (reuse AI-1 standing-building→node sync); a capturing AI house adopts it. The player is not an AI house (no node work when the player captures).
4. **Primary-factory re-eval** for the affected `BuildCat` on both houses.
5. **Neutral→owner cash:** if captured from the neutral house, grant `ProduceCashStartup` (CAOILD=1000) to the new owner once.
Pure helper logic in a new `engine/capture.{hpp,cpp}` operating on views where possible; the mutation entry point is a `World` method.

### 2. Engineer capture + repair
- `ENGINEER` unit (art present). Right-click an enemy or neutral building → move adjacent → on arrival call `capture_building(...)`, consume the engineer. Captures at **any** health (no threshold — recovered). Entering an **own damaged** building → repair to full (PROBABLE), consume engineer.

### 3. Neutral tech structures
- Add neutral house (id 2). Spawn a `CAOILD` for the demo ([RE-FALLBACK]). While neutral it pays nobody; once captured, `capture_building` grants Startup=1000 and thereafter the owner receives **Amount=20 every Delay=100 frames** via a new `step_tech_structures` tick (behind the firewall). `CAPOWR` (+200 power once owned — auto via power_totals if roster carries it), self-heal structures documented; their unit-heal effect is minimal/deferred as noted.

### 4. Spy infiltration — `infiltrate_building(world, spy, building_idx)` (early-return if spy owns it)
- **Cash steal** (CONFIRMED): `steal = 0.5 × house_credits_[victim]`; add to spy house, subtract from victim.
- **Power blackout** (CONFIRMED): new per-house `blackout_frames` timer (1000); while >0 that house's power output is forced to 0 in `power_totals` (drain unaffected) → low-power build clamp + offline defenses follow naturally.
- **SW reset** (CONFIRMED): reset the victim's `Superweapon` charge to 0 (reuse the existing reset path).
- **Veterancy** (PROBABLE, scope = flag + minimal bonus): set a per-house `produced_veteran` flag; units launched by that house thereafter get `veteran_rank=1` (a new `UnitCombat` field) applying the recovered Veteran firepower/armor multipliers in the damage calc (exact multipliers pinned at the RE-GATE via `ra2extract` `[General]` veteran keys; [RE-FALLBACK] to documented defaults if unpinned).
- **Radar/lab** ([RE-FALLBACK]): no shroud → the effect is recognized but inert; documented.
- Spy consumed on infiltration.

### 5. Minimal veterancy core
- `UnitCombat.veteran_rank` (0=none,1=veteran) additive field; `veteran_rank=0` reproduces today's numbers exactly (byte-identity for existing gates). Damage/armor multipliers applied only when `rank>0`. Only spy-infiltration sets the produced-veteran flag this phase.

### 6. Disguise / detection
- Additive `Unit.disguised_as_type` / `disguised_as_house` (+ a bool). A disguised spy renders as that infantry type in that house's palette (reuse the house-remap). Default spy disguise = `AlliedDisguise=E1`/`SovietDisguise=E2` by owner side.
- **Detection:** a `DetectDisguise` flag on unit types (dogs carry it). A disguised spy within a detector's sight → revealed (disguise cleared). Also revealed on firing / being attacked (PROBABLE). Enemy AI/targeting treats a still-disguised spy as friendly (not auto-acquired) — matching the disguise's purpose.

### 7. Gate — `--demo-capture` (14th permanent gate, `capture_hash`)
Scripted, two-run (rendered + headless) self-compare, **NO pinned literal** (controller pins in README), wedge guards → exit 1. Sequence:
1. Engineer captures an enemy building → assert `b.house` flipped AND a buildability/power delta followed.
2. Engineer captures a spawned neutral `CAOILD` → assert ownership + Startup grant + income tick.
3. Spy infiltrates enemy refinery → assert cash moved by 0.5×; power plant → blackout timer set; SW building → timer reset; factory → produced-veteran flag set (and a subsequently produced unit carries `veteran_rank=1`).
4. Disguised spy passes an enemy non-detector (stays disguised) then a `DOG` (revealed).
Hash folds: building ownership list, both houses' credits, blackout timer, SW charge, produced-veteran flag, the spy's disguise state, and the veteran rank of a produced unit — at fixed checkpoints. **Wedge guards:** capture must flip ownership, cash-steal must move credits, blackout must arm, SW must reset, dog must reveal — else exit 1.

## Byte-identity discipline (§8)

- A **`capture_any_` firewall**: `step_tech_structures` / blackout decay / disguise-detection / veteran-on-produce are all early-returns unless a capture/spy/neutral/disguise entity exists. No existing demo has any → the 13 gates run the exact prior paths.
- New per-house state (blackout timer, produced_veteran flag, neutral index) defaults inert; per-house arrays widened to 3 with index 2 unused by existing demos.
- `veteran_rank=0` default → combat math byte-identical (combat 78 / tank 144 unaffected).
- `capture_building` is only reachable via the new engineer/spy gestures; `world_hash` untouched; new state hashed only by `capture_hash`.
- The 13 gates that must stay bit-identical: combat 78, tank 144, base `5273b5e71ca009e1`, econ `7fed96cb98b68017`, prod `f0f4e1207ecd713c`, ai-base `cd1f7c6a29933f0a`, ai-team `312e92ed862a5e74`, ai-trigger `8334a6ca1531cabc`, transport `fe500f946e913645`, aircraft `a69642564cf64773`, naval `6aee2c82bc024e86`, superweapon `189496664f5eca68`, techtree `e73adae61ff6f71f`. Re-verify at the first ownership-touching commit AND at finalize, **one gate per call** (each AI gate ≈2 min — see the gate-runtime discipline).

## Global constraints

- **Calibrate-first:** the hunt is COMPLETE (`docs/re/capture.md`). The plan's Task 1 is a **RE-GATE adjudication** (confirm the PROBABLE items — engineer-repair, veterancy rank/multipliers, MakeupKit, reveal-on-fire, radar/lab — via `ra2extract` + targeted decompile; flip `[RE-GATE]`→`[RE-GATE APPLIED]`), NOT a fresh hunt. YRpp/Ares addresses UNTRUSTED (content-anchor only).
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Run exes from PowerShell (not Git Bash → exit 127); tests skip cleanly without an install; NEVER pipe a native exe through `Select-Object -First N`; run demo gates **one per call** (they take ~2 min each; batching exceeds the 600s tool timeout → auto-background stall).
- No assets/GPL/Ghidra output committed; recovered constants ARE committable.
- Repo solo `master`. Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` (project convention).

## Testing

- Pure doctests for `engine/capture.*` (capture flips owner + moves SW/primary; neutral Startup grant only from neutral; cash-steal 0.5×; blackout arms/decays; veteran multiplier applies only at rank>0; disguise set/clear; dog detection). Host-side, no install.
- Both-modes finalize sweep (with/without install); arm64 `assembleDebug`; the 14th gate + all 13 prior gates re-verified one-per-call.

## Out of scope / carry-forwards

Bridge repair; MakeupKit click-to-re-disguise; mirage/fake-blink RNG disguise; full veterancy (kills-based promotion, elite, chevrons, self-heal/speed bonuses); real radar/shroud reveal (needs a shroud phase); neutral-structure map parsing + full neutral roster; neutral AI; garrisonable buildings; AI use of engineers/spies.
