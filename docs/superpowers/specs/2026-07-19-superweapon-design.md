# CB-4 Superweapons Phase Design — a Per-House Recharge-Timer Framework and a Nuke Detonation

**Date:** 2026-07-19
**Status:** Approved (controller-scoped, formalized autonomously)
**Depends on:** the complete AI arc, Unit Production, Economy, Buildings,
combat WITH area/splash damage, and CB-1/2/3 (cargo, aircraft/flight,
naval/water) — the **eleven** permanent byte-identity gates this phase must keep
bit-identical: `--demo-combat` logs=78, `--demo-tank` logs=144, `--demo-base`
5273b5e71ca009e1, `--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a,
`--demo-ai-team` ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc, `--demo-transport` transport_hash=fe500f946e913645,
`--demo-aircraft` aircraft_hash=a69642564cf64773, `--demo-naval`
naval_hash=6aee2c82bc024e86. Reuses **`apply_spread_damage`**
(`engine/combat.cpp`, the CB-2 area-damage applicator gated on warhead
`CellSpread>=1.0`, `docs/re/combat.md` §7) for the nuke detonation — NO new
damage system; the **per-house-registry + firewalled-step** discipline
(`ai_houses_`/`ai_any_` + `step_ai_houses`, and CB-1/2/3's inert-field /
added-clause byte-identity model, `naval-design.md` §8) as the shape for adding a
per-house subsystem without perturbing the existing sim; and the existing sidebar
**cameo + `gclock2.shp` build-clock chrome** (`main.cpp`, the Buildings phase's
55-frame production clock) for the SW cameo + recharge indicator.

## Goal

Introduce the **superweapon framework**: a **per-house superweapon** with a
**recharge timer** (charges over the recovered `RechargeTime` → READY, then
resets after firing), **player target selection** (when READY the player picks a
target cell — arming SW-targeting via a keybind or sidebar-cameo click, then a
map click fires at that cell), and a fired **EFFECT** at that location — an
**area-damage detonation** that REUSES `apply_spread_damage`/the combat damage
math. Demonstrated deterministically on one real superweapon — the **Soviet
Nuclear Missile (`[NukeSpecial]`, `Type=MultiMissile`)** — and gated by the
**12th permanent byte-identity gate `--demo-superweapon`**. This is **CB-4**, the
fourth content-breadth phase (CB-1 Transports ✓ → CB-2 Aircraft ✓ → CB-3 Naval ✓
→ **CB-4 Superweapons** → CB-5 Tech-tree).

Unlike the CB-1/2/3 locomotor/movement phases — whose hardest surface was the
sim's movement/pathfinding hot loop — CB-4 touches **no existing hot path**. The
superweapon is a **new per-house subsystem** (a timer that ticks, a fire order
that detonates once) grafted beside the existing sim exactly as the per-house AI
registry was: a `sw_any_` firewall makes `step_superweapon` a strict no-op in
every existing demo, no house owns a SW anywhere but the new gate, and the
detonation runs the ALREADY-SHIPPING `apply_spread_damage` with a nuke-scale
warhead. So this phase's byte-identity constraint (§8) is the *easiest* since the
AI phases — the risk is entirely in calibrating the timer/effect to the original
(§1), not in preserving a shared data structure. One spec→plan→execute cycle,
~7 tasks, the same spine as every prior phase.

## Recovered exemplar ground truth (committable — read from the real install)

Read from the real `ra2md.mix → localmd.mix → rulesmd.ini` on this install with
`ra2extract` (committable ground truth; their INTERPRETATION — the timer
units→frames conversion, the SW-fire delivery model, whether `[NukePayload]` or
`[NukeCarrier]` is what detonates at the target — is the §1 hunt's):

- **`[SuperWeaponTypes]`** (the full active SW roster, id → INI section — the
  enum the hunt maps §1 item 4):
  `1=NukeSpecial`, `2=IronCurtainSpecial`, `3=LightningStormSpecial`,
  `4=ChronoSphereSpecial`, `5=ChronoWarpSpecial`, `6=ParaDropSpecial`,
  `7=AmericanParaDropSpecial`, `8=PsychicDominatorSpecial`, `9=SpyPlaneSpecial`,
  `10=GeneticConverterSpecial`, `11=ForceShieldSpecial`, `12=PsychicRevealSpecial`.
- **`[NukeSpecial]`** (the exemplar SW — Soviet Nuclear Missile):
  `Type=MultiMissile`, **`RechargeTime=10`** (the commented-out test line
  `;RechargeTime=0.1` sits directly above — the RA2-canonical unit is **minutes**;
  the units→frames conversion is §1 item 1, NOT asserted here),
  `WeaponType=NukeCarrier`, `Action=Nuke`, `SidebarImage=NukeIcon`,
  `ShowTimer=yes`, `IsPowered=true`, `AIDefendAgainst=yes`, `Range=7`,
  `LineMultiplier=2`, `DisableableFromShell=yes`.
- **`[NukeCarrier]`** (the SW's `WeaponType` — the straight-up launch leg):
  `Projectile=GiantNukeUp`, `Speed=100`, `Warhead=NukeMaker`. `[NukeMaker]` has
  `NukeMaker=yes` ("the weapon that shoots straight up and then uses the houses'
  targeting for where to come down" — verbatim rules comment).
- **`[NukePayload]`** (the down-strike weapon — the damaging leg, `MultiMissile`):
  **`Damage=600`**, `Range=30`, `Projectile=GiantNukeDown`, `Speed=10`,
  `RadLevel=500`, **`Warhead=NUKE`**.
- **`[NUKE]`** (the detonation warhead — the area-damage effect):
  **`CellSpread=10`**, **`PercentAtMax=.02`**,
  **`Verses=100%,100%,100%,200%,100%,100%,60%,100%,8%,100%,100%`** (the 11-slot
  `none..special_2` armor array, `combat.md` §1/§4), `Wood=yes`, `Tiberium=yes`,
  `WallAbsoluteDestroyer=yes`, `InfDeath=7`, `AnimList=NUKEANIM`.
- **`[GiantNukeDown]`** (the down projectile): `Vertical=yes` (falls straight
  down, the `BlimpBombP` precedent), `DetonationAltitude=30000`, `Image=NKMSLDN`,
  `Acceleration=1`. **`[GiantNukeUp]`** is the launch projectile (Speed=100,
  `Vertical=yes`).

**Design consequence — the nuke effect is a large-`CellSpread` area detonation
of the `[NUKE]` warhead at the target cell.** `Damage=600` (from `[NukePayload]`),
`CellSpread=10` → `round(10)=10` cells → `10·kLeptonsPerCell` (2560 leptons)
splash radius, falling off LINEARLY from 100% at the impact point to
`PercentAtMax=.02` (2%) at the edge, then `damage_vs(base, NUKE-Verses, armor)`
truncated per victim. **This maps ONE-TO-ONE onto the existing
`apply_spread_damage`** (`combat.cpp:84`), which already reads exactly
`attacker.w_damage`, `attacker.w_cell_spread`, `attacker.w_percent_at_max`,
`attacker.w_verses`, and `center.fx/fy` — the same fields CB-2's `BlimpHE`
(`CellSpread=2 PercentAtMax=.5`) splash uses. CB-4 adds **no new damage math**;
it constructs a nuke-scale detonation and calls the shipping applicator (§5).

The one INTERPRETATION the hunt must settle (§1 item 2): the SW's own
`WeaponType=NukeCarrier` (`Warhead=NukeMaker`) is the *up-launch* — the actual
target-cell damage lives in `[NukePayload]`/`[NUKE]` ("Activated from code
now... see Bullet AI" — the rules' own comment on `AnimList=NUKEBALL`). The
`[NukePayload] Damage=600` + `[NUKE]` warhead VALUES are committable; that they
are what detonates at the picked cell (vs the carrier's `NukeMaker`) is the
hunt's to confirm from the `MultiMissile` fire path.

## Current-engine reality (verified — favorable for byte-identity)

- **No per-house superweapon state exists.** The only per-house registry today is
  **`HouseAiRec`** (`world.hpp:722`), grown on demand by `enable_ai_house`
  (`world.cpp:987`) and gated on `rec.is_ai` — it holds AI base/team/trigger
  state and is **only** materialized for AI-flagged houses. The **player house 0
  is never in `ai_houses_`**, so the SW registry CANNOT piggy-back on
  `HouseAiRec`; CB-4 adds a **separate, independent per-house SW registry**
  (`sw_houses_` + `sw_any_`) that works for the player house exactly as
  `ai_houses_` works for AI houses. This is the phase's one net-new World member.
- **`apply_spread_damage` already exists and is directly reusable**
  (`combat.cpp:84`): a self-contained loop over `units`, reading only the
  attacker's warhead fields + the center's `fx/fy` + the attacker's `house` (its
  "enemies only" filter is `if (w.house == attacker.house) continue;`). It takes
  `(std::vector<Unit>&, Occupancy&, const Unit& attacker, const Unit& center)` —
  so a SW detonation with no physical firing unit is expressed as a **transient
  virtual detonator `Unit`** (house = firing house, `fx/fy` = target-cell center,
  the four warhead fields = the recovered `[NUKE]`/`[NukePayload]` values) passed
  as BOTH `attacker` and `center`. **`combat.cpp` is not edited at all** —
  strongest possible byte-identity for the CB-2 `--demo-aircraft` splash path,
  which keeps calling the identical function (§5, §8).
- **`Unit` needs no new fields.** SW state is purely per-house + World-level;
  the detonation uses a transient stack `Unit`, not a persistent field. The
  `Unit` struct is **untouched** — even stronger than CB-1/2/3, which each added
  inert `Unit` fields. `world_hash` (`main.cpp:3855`, reads only
  `cell_x/cell_y/hp/alive` per unit + building fields) is therefore untouched by
  construction (§8).
- **`World::step` is a linear sequence of guarded passes** (`world.cpp:1977`):
  `step_units` → `step_cargo` (`if (!cargo_any_) return;`) → `step_flight`
  (`if (!aircraft_any_) return;`) → `step_turrets` → `step_buildings` → … →
  `step_ai_houses` (`if (ai_any_)`). CB-4 inserts one more guarded pass,
  `step_superweapon` (`if (!sw_any_) return;`), the exact same firewall shape (§3).
- **The sidebar already draws a cameo + a 55-frame `gclock2.shp` clock**
  (`main.cpp:2520–2524`: `gframe = 1 + stage`, clamped to `sb_gclock.count()`,
  blitted over a shade) and loads `NukeIcon`-style cameos through the same
  `cameo.mix` chain building/unit cameos use (`main.cpp:1420`, `load_sb`). The SW
  UI reuses this chrome verbatim — no new font/UI systems (§6).

## Approved scope decisions

| Decision | Choice |
|---|---|
| SW framework | A **per-house superweapon** with a recharge timer that counts up over the recovered `RechargeTime` → READY, and **resets to 0 after firing** (the recharge/ready/reset lifecycle is §1 item 1). A pure `engine/superweapon` module owns the lifecycle (`tick`, `is_ready`, `on_fired`); `World` owns the per-house registry and calls it |
| Per-house SW state | A NEW `std::vector<HouseSuperweapon> sw_houses_` (index = house id, grown on demand) + a `bool sw_any_ = false` firewall — the `ai_houses_`/`ai_any_` pattern, but INDEPENDENT of the AI registry so it works for the **player house 0**. `HouseSuperweapon` wraps a pure `Superweapon` lifecycle (type/recharge/timer/ready) + the resolved detonation params (see §2 data model). Default INERT (no house has a SW until `grant_superweapon`) |
| Grant | CB-4 **grants the SW to a house DIRECTLY** via `World::grant_superweapon(house, type, recharge_secs, effect-params)` (like CB-1/2/3 spawned their exemplars directly) — sets `sw_any_ = true`. Wiring "build the Nuke Silo → unlock the SW" is a carry-forward (§ Out of scope) |
| Timer tick | `step_superweapon(dt)` — a NEW `World::step` pass, firewalled behind `if (!sw_any_) return;`, that advances each granted house's `timer` toward `recharge_secs` and flips `ready` at the threshold. No RNG (a deterministic countdown; confirm §1 item 5) |
| Target selection | When READY, the player arms SW-targeting (a keybind — proposed **`N`** for Nuke — or a click on the sidebar SW cameo), then a **left-click on a map cell** fires at that cell, reusing the EXISTING screen→cell picking (`main.cpp` input). The demo scripts this deterministically via a direct `World::fire_superweapon(house, cx, cy)` call (no synthetic input — the `--demo-naval` `order_attack` precedent) |
| Fire | `World::fire_superweapon(house, cx, cy)` — no-op unless that house's SW is READY; on fire it detonates at the cell (§5), then calls `sw.on_fired()` to **reset the timer to 0** and `ready=false`. Returns whether it fired (for the wedge guard) |
| The fired EFFECT | An **area-damage detonation** at the target cell, REUSING `apply_spread_damage` unchanged: build a **transient virtual detonator `Unit`** (house = firing house; `fx/fy` = target-cell center; `w_damage`/`w_cell_spread`/`w_percent_at_max`/`w_verses` = the recovered `[NukePayload] Damage=600` + `[NUKE] CellSpread=10, PercentAtMax=.02, Verses`), and call `apply_spread_damage(units, occ, virt, virt)`. **No new damage system; `combat.cpp` untouched** (§5) |
| Sidebar SW UI | A SW cameo (`NukeIcon`, art-guarded) + a recharge-clock indicator drawn with the EXISTING `gclock2.shp` 55-frame chrome — `gframe = round(timer/recharge_secs · 54)` while charging, a READY tint (the existing flashing-green ready state) when charged. No new font/UI systems (§6) |
| Exemplar | **Nuclear Missile `[NukeSpecial]` `Type=MultiMissile`** (Soviet — the loaded roster is Soviet). Art-guarded on `NukeIcon`; if the cameo/anim regresses, the SW cameo drops to a placeholder tile and the demo runs headless (the sim logic is exemplar-art-independent and pure-tested regardless) |
| Verification | **New deterministic gate `--demo-superweapon`** (the 12th) + `superweapon_hash` (FNV over the SW timer/ready state + the target cell + the post-detonation per-unit `hp/alive`), two-run compute-and-self-compare (NO pinned literal in code); **wedge guards (§7): the SW must charge to READY AND fire AND the detonation must damage/kill target units, else exit 1** |
| Byte-identity | **Strategy (a)**: SW state excluded from `world_hash` (untouched); `Unit`/`combat.cpp` untouched; `step_superweapon` firewalled behind `sw_any_` (false in every existing demo → a strict no-op); the detonation fires only on an actual `fire_superweapon` call (which no existing demo issues). Re-verify all 11 gates at the first additive commit AND at finalize (§8) |

## §1 What the Ghidra hunt must recover (→ `docs/re/superweapon.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, `gamemd.exe` YR 1.001 Ghidra
project at `D:\ra2-re`, headless `analyzeHeadless.bat`,
`JAVA_HOME="C:\Program Files\Java\jdk-21"`) plus the real rulesmd, producing
`docs/re/superweapon.md` — the calibration authority BEFORE any code, with a
located-functions table, a "deviations we take" section (mirroring
`combat.md`/`naval.md`), and each item landing CONFIRMED / PROBABLE /
[RE-FALLBACK] with function address + decompile reasoning. **Ares/YRpp addresses
are UNTRUSTED prose hints only** — `combat.md`'s methodological finding stands:
Ares' hardcoded "YR 1.001" addresses do NOT transfer to this exact binary, so
content-anchor everything. Anchor on: the INI-key strings the binary loads
(`"RechargeTime"`, `"Type"`, `"WeaponType"`, `"Action"`, `"SidebarImage"`,
`"ShowTimer"`, `"IsPowered"`, `"AIDefendAgainst"`, `"LineMultiplier"`); the SW
type-name strings (`"NukeSpecial"`, `"MultiMissile"`, `"IronCurtain"`,
`"ChronoSphere"`, …); the `[SuperWeaponTypes]` list reader (a
`RulesClass::LoadFromINI` fragment, the `combat.md` §3 precedent for the
`[CombatDamage]`/`[General]` readers); and `SuperClass` / `SuperWeaponTypeClass`
(the per-instance timer + the type descriptor). **Do NOT guess numeric constants**
(the `RechargeTime` units→frames factor, whether firing resets to 0 or to a
partial value) — they are hunt outputs. The `[NukeSpecial]`/`[NukePayload]`/
`[NUKE]` VALUES quoted above are committable ground truth already; their
INTERPRETATION is the hunt's. Five items:

1. **The SW timer/ready/fire lifecycle** (`SuperClass`): how `RechargeTime` is
   converted to a countdown (**expected: minutes → `RechargeTime·60·kLogicFps`
   frames**, the `ai-base.md` §1 `BuildDelay` minutes→frames precedent — CONFIRM
   the unit and factor); the charge/ready predicate (`SuperClass::IsCharged` /
   the `+offset` timer field); whether **firing resets the timer to 0** (expected:
   yes, a full re-charge) or to a partial value; the per-house instancing (one
   `SuperClass` per house per owned SW type). `IsPowered=true` → the low-power
   suspend behavior is mapped-for-later (CB-4 charges unconditionally, § OoS).
2. **The nuclear-missile SW fire → effect** (`Type=MultiMissile`): the fire path
   from `Action=Nuke` target-pick to the detonation. CONFIRM that the target-cell
   damage is the `[NukePayload]` (`Damage=600`, `Warhead=NUKE`) down-strike, NOT
   the SW's own `WeaponType=NukeCarrier` up-launch (`Warhead=NukeMaker`, which
   only "shoots straight up"). Then the effect maps to `apply_spread_damage` via
   `[NUKE] CellSpread=10, PercentAtMax=.02, Verses[]` (`combat.md` §7). The exact
   `CellSpread` damage-falloff curve (`combat.md` §7's un-unwound `FUN_00489190`)
   stays the adjudicated linear 100%→`PercentAtMax` approximation CB-2 already
   ships — CB-4 does not re-open it. `RadLevel=500` (radiation lingering) is out
   of scope (§ OoS); map it for later.
3. **Target selection + fire trigger**: how `Action=Nuke` arms the targeting
   cursor and how a map click commits the target cell to the `SuperClass` fire;
   any restrictions (`Range`/`LineMultiplier` — expected cosmetic/targeting-aid
   for the nuke, not a range cap on where it can strike; CONFIRM). The screen→cell
   pick itself reuses the existing input path — the hunt need only confirm the
   arm→click→fire ordering, not re-derive picking.
4. **The full recovered SW `Type` enum** (`MultiMissile`/`IronCurtain`/
   `LightningStorm`/`ChronoSphere`/`ChronoWarp`/`ParaDrop`/`PsychicDominator`/
   `GeneticConverter`/`ForceShield`/`PsychicReveal`/`SpyPlane`, from
   `[SuperWeaponTypes]` above) — RE-documented so the deferred effect kinds have a
   mapped start (the `ai-team.md` §3 opcode-table precedent: document the full
   enum, implement one). CB-4 implements ONLY the area-damage `MultiMissile`/Nuke
   `Type`; every other `Type` is a mapped enum value with no CB-4 effect.
5. **Determinism + byte-identity**: how `SuperClass` integrates without
   perturbing the existing sim (expected: an independent per-house timer + a
   fire-triggered detonation — no existing step reordered); confirm **no RNG** on
   the SW charge or the nuke detonation critical path (`[NukePayload]`/
   `[GiantNukeDown]` declare no scatter fields; `apply_spread_damage` is already
   RNG-free — pin it). Any per-frame SW work the original does that an existing
   house would incur (expected: none unless a SW is granted).

Calibration discipline (unchanged from every prior phase): every engine constant
cites this doc. **All numeric semantics in this spec are pre-hunt expectations,
not ground truth** (the rulesmd VALUES quoted above were read from the real file
on this install and are committable; their INTERPRETATION is the hunt's). A
controller **RE-GATE** amendment step precedes implementation — the hunt's
findings amend the plan and are committed BEFORE dependent dispatch, with
`[RE-GATE APPLIED]` markers threaded through every dependent task (the CB-1/2/3
precedent). **[RE-FALLBACK]**: anything the hunt cannot pin becomes a
plan-amendment gate (present options to the human) — never an uncalibrated guess.
The **`RechargeTime` units→frames factor (item 1)** and the **`[NukePayload]`-vs-
`[NukeCarrier]` damage attribution (item 2)** are the most likely [RE-FALLBACK]s;
the fallbacks are documented (minutes→`·60·kLogicFps`; the `[NukePayload]`/`[NUKE]`
down-strike is the detonation).

## §2 Architecture & recovered data model

Consistent with every prior phase: a pure SDL-free engine module + a thin World
integration + a deterministic demo in `main.cpp`. ~7 tasks:

- **Task 1**: Ghidra hunt → `docs/re/superweapon.md` (RE-GATE before
  implementation; §1 above). Includes art-verifying `NukeIcon`/`NUKEANIM`
  presence in the real mixes with `ra2extract` (a demo-art [RE-FALLBACK] if
  absent — the sim is art-independent).
- **Task 2**: pure `engine/superweapon.{hpp,cpp}` — the `SuperweaponType` enum
  (the full recovered §1-item-4 list; only `Nuke`/`MultiMissile` active), a
  `Superweapon` lifecycle struct (`tick(dt)`, `is_ready()`, `on_fired()`) over
  plain scalars, no World/SDL/RNG dependency; doctest-covered (charges to READY at
  exactly `recharge_secs`; `on_fired` resets to 0 and clears ready; a mid-charge
  tick is monotonic and never overshoots). Registered in BOTH
  `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- **Task 3**: World integration — the `sw_houses_` registry + `sw_any_` firewall,
  `grant_superweapon(house, …)` (grows `sw_houses_`, sets `sw_any_`),
  `step_superweapon(dt)` (the guarded `World::step` pass, §3), `fire_superweapon`
  (§4), and read-only accessors for the gate/UI (`sw_state(house)`,
  `sw_ready(house)`, `sw_charge_fraction(house)`). **11-gate re-verify at this
  first additive commit** (`step_superweapon` no-op proven; §8).
- **Task 4**: the fired EFFECT — `fire_superweapon`'s detonation builds the
  transient virtual detonator `Unit` from the granted effect params and calls the
  EXISTING `apply_spread_damage` (§5); `main.cpp` resolves the `[NukePayload]`/
  `[NUKE]` warhead values from Rules into the `grant_superweapon` effect params
  (like every existing stat resolution). Pure/World tests for the detonation.
- **Task 5**: sidebar SW UI + input — the `NukeIcon` cameo + `gclock2.shp`
  recharge clock (art-guarded, §6); the `N`-keybind / cameo-click arm →
  map-click-fire wiring in `main.cpp` (reuses the existing screen→cell pick).
- **Task 6**: `--demo-superweapon` gate (the 12th) + `superweapon_hash` +
  witnesses + wedge guards (§7).
- **Task 7**: finalize — suites both modes, 11 existing gates byte-identical,
  arm64 NDK `nm`-verify of the new superweapon symbols, README (CB-4 section +
  12th gate row + deviations note citing `docs/re/superweapon.md`).

**Decomposition tripwire**: the framework is small (a timer + a fire order + one
detonation call) and this is the single coherent ~7-task phase. IF the sidebar
SW UI/input (Task 5) fans out — a full targeting-cursor art path, cameo hit-test
plumbing, a ready-flash animation beyond the existing chrome — the phase splits
at the natural seam: **(A) the SW framework + effect** (Tasks 1–4, 6 with a
headless-only `--demo-superweapon` that grants, charges, and fires via the direct
`fire_superweapon` call, no sidebar UI) then **(B) the sidebar SW UI + input
arming** (Task 5 + a rendered witness). Default is the single phase below.

### Data model

Field shapes below are the approved design; the Task 1 hunt CONFIRMS or REPLACES
the semantics (RE-GATE), and code/tests are written against the amended form.

- **`SuperweaponType`** (new enum, `superweapon.hpp`): `None = 0`, `Nuke = 1`
  (`MultiMissile`), then the deferred `IronCurtain`, `LightningStorm`,
  `ChronoSphere`, `ChronoWarp`, `ParaDrop`, `PsychicDominator`,
  `GeneticConverter`, `ForceShield`, `PsychicReveal`, `SpyPlane` (mapped from §1
  item 4; `None = 0` so a value-initialized record is inert). CB-4 acts only on
  `Nuke`.
- **`Superweapon`** (pure lifecycle struct, `superweapon.hpp`):
  `SuperweaponType type = None`, `double recharge_secs = 0.0`,
  `double timer = 0.0`, `bool ready = false`. Methods: `tick(double dt)` (adds
  `dt`, clamps at `recharge_secs`, sets `ready` at threshold — no overshoot),
  `is_ready()`, `on_fired()` (`timer = 0; ready = false`). No RNG, no World dep.
- **`HouseSuperweapon`** (World-private, mirrors `HouseAiRec`'s shape):
  `bool has_sw = false`, `Superweapon sw`, plus the resolved effect params
  `double eff_damage`, `eff_cell_spread`, `eff_percent_at_max`, and
  `std::array<double,11> eff_verses` (the `[NUKE]` armor array). Default INERT.
- **`World` members**: `std::vector<HouseSuperweapon> sw_houses_` (empty by
  default), `bool sw_any_ = false`. `grant_superweapon(int house,
  SuperweaponType, double recharge_secs, double dmg, double cell_spread, double
  pmax, const std::array<double,11>& verses)` grows the vector and sets
  `sw_any_`. `step_superweapon(double dt)` early-returns unless `sw_any_`.
  `fire_superweapon(int house, int cx, int cy)` → `bool fired`.
- **`Unit` — UNCHANGED.** No new `Unit` fields (the detonation uses a transient
  stack `Unit`). **`combat.cpp` — UNCHANGED** (`apply_spread_damage` reused as-is).
- **Pure module** (registered in BOTH cmake files): `engine/superweapon.{hpp,cpp}`.
  Every recovered constant cites `docs/re/superweapon.md`.

## §3 The SW framework — recharge-timer lifecycle

The superweapon is a per-house timer that ticks toward READY. There is **no
interaction with any existing step**:

1. **Grant**: `World::grant_superweapon(house, type, recharge_secs, effect…)`
   grows `sw_houses_` to cover `house` (the `enable_ai_house` resize precedent,
   `world.cpp:989`), sets `has_sw=true`, stores the lifecycle `recharge_secs` and
   the resolved effect params, and sets `sw_any_ = true`. Called ONLY by the new
   gate (and, later, a Nuke-Silo unlock — carry-forward). No existing demo calls
   it ⇒ `sw_houses_` empty, `sw_any_` false.
2. **Tick**: `step_superweapon(dt)` — `if (!sw_any_) return;` first line — walks
   `sw_houses_`, calling `rec.sw.tick(dt)` on each granted house. `tick` adds `dt`
   to `timer`, clamps at `recharge_secs`, and sets `ready` when
   `timer >= recharge_secs`. A deterministic countdown; **no RNG** (§1 item 5).
3. **Placement in `World::step`**: `step_superweapon(dt)` is inserted as a new
   guarded pass, alongside the other firewalled passes. Because it only advances a
   timer (the detonation is fire-triggered, not step-triggered) its ORDER relative
   to the other passes is behaviorally irrelevant for existing demos (no-op) and
   for the new gate (the timer is read at fire time, not mid-frame); it is placed
   after `step_flight` for locality with the other CB passes. No existing pass is
   reordered.
4. **Ready**: `sw_ready(house)` / `sw_charge_fraction(house)` expose the state to
   the sidebar (§6) and the gate (§7). READY persists until `fire_superweapon`
   consumes it.
5. **Determinism**: no new RNG, no reordering; `sw_houses_` iterates in house-id
   order. The whole subsystem is pure arithmetic on `dt`.

## §4 Target selection & fire

Reuses the existing input; scripted deterministically in the demo:

1. **Arm** (interactive): when the house's SW is READY, the player arms
   SW-targeting via the **`N` keybind** or a **click on the sidebar SW cameo**
   (§6) — setting a `main.cpp`-local `sw_targeting` UI flag that swaps the cursor
   to the target reticle. This is UI state only; the sim is untouched until fire.
2. **Fire** (interactive): the next **left-click on a map cell** resolves to a
   grid cell via the EXISTING screen→cell pick and calls
   `World::fire_superweapon(house, cx, cy)`, then clears `sw_targeting`. If the SW
   is not READY the arm is refused (no-op).
3. **Fire** (demo): the gate calls `World::fire_superweapon(house, cx, cy)`
   directly with a scripted target cell (no synthetic input events — the
   `--demo-naval` `order_attack(t_idx)` precedent). This keeps the gate a pure sim
   computation, byte-identical rendered vs headless.
4. **`fire_superweapon(house, cx, cy)`**: no-op returning `false` unless that
   house's SW `is_ready()`. On fire: detonate at the cell (§5), then
   `rec.sw.on_fired()` (reset `timer=0`, `ready=false`) and return `true`. The
   `Range`/`LineMultiplier` targeting-aid fields are cosmetic for the nuke
   (confirm §1 item 3); CB-4 places no range cap on the target cell.

## §5 The fired EFFECT — area-damage detonation via `apply_spread_damage`

The nuke effect reuses the shipping combat applicator with **no new damage
math and no `combat.cpp` edit**:

1. **Transient virtual detonator**: `fire_superweapon` builds a stack `Unit virt`
   (value-initialized), sets `virt.house = house`, `virt.fx/virt.fy` = the
   target-cell center in leptons (the existing cell→lepton convention), and the
   four warhead fields from the granted effect params: `virt.w_damage =
   eff_damage` (600), `virt.w_cell_spread = eff_cell_spread` (10),
   `virt.w_percent_at_max = eff_percent_at_max` (.02), `virt.w_verses =
   eff_verses` (the `[NUKE]` array). These are exactly and only the fields
   `apply_spread_damage` reads from its `attacker`/`center` args.
2. **Detonate**: call `apply_spread_damage(units, occ, virt, virt)` — `virt` is
   both the `attacker` (house + warhead params) and the `center` (`fx/fy`). The
   applicator runs its existing loop: every alive, non-carried, non-aircraft
   ENEMY unit within `round(10)·kLeptonsPerCell` (2560 leptons) of the target
   takes `damage_vs(round(600·scale), NUKE-Verses, armor)` with `scale` falling
   LINEARLY 100%→.02 across the radius (`combat.cpp:96–101`), and dies + releases
   occupancy if `hp<=0`. `CellSpread=10 >= 1.0`, so this is the genuine
   large-radius splash — the same code path CB-2's `BlimpHE` splash exercises,
   just at nuke scale.
3. **Buildings**: `apply_spread_damage` today damages `units` only (CB-2's aerial
   bomb). Whether the nuke also damages BUILDINGS in the radius is §1 item 2's
   effect scope — if the hunt confirms buildings take splash (expected: yes, a
   nuke levels a base), a parallel building-splash loop mirrors the unit loop
   (same falloff, `combat.md` §3 truncation) as an **added, SW-only** clause;
   otherwise CB-4 damages units only and building-splash is a carry-forward. The
   demo's kill target is a cluster of UNITS regardless, so the gate is
   building-splash-independent.
4. **No friendly fire, no radiation**: the reused applicator's "enemies only"
   filter (`w.house == attacker.house` skip) means the nuke spares the firing
   house — a documented divergence (real nukes hit friendlies; the established
   enemies-only splash is kept). `RadLevel=500` lingering radiation is out of
   scope (§ OoS). Both are recorded, not built.
5. **Delivery is instant** — the detonation applies at the target cell the frame
   `fire_superweapon` is called; the `GiantNukeUp`/`GiantNukeDown` missile flight,
   `DetonationAltitude`, and the `NUKEANIM` mushroom cloud are carry-forwards (the
   established instant/simplified-delivery precedent, `combat.md` / CB-2's
   instant-bomb). This is `apply_spread_damage`'s existing model — the damage half
   is exact, the flight half is skipped, exactly as CB-2 documented.

## §6 Sidebar SW UI

Reuses the existing cameo + build-clock chrome — **no new font/UI systems**:

- **Cameo**: the SW cameo (`NukeIcon`, loaded through the existing `cameo.mix`
  chain, `main.cpp:1420`) draws in a SW slot on the sidebar, art-guarded exactly
  like building/unit cameos — if `NukeIcon` is absent the slot draws a
  placeholder shade and the sim is unaffected.
- **Recharge clock**: the `gclock2.shp` 55-frame clock (`main.cpp:2520–2524`)
  overlays the cameo while charging: `gframe = clamp(round(sw_charge_fraction ·
  54), 0, sb_gclock.count()-1)`, drawn over the shade exactly as the production
  build-clock. `ShowTimer=yes` (rules) drives its presence.
- **READY state**: at full charge the clock clears and the cameo shows the
  EXISTING READY tint (the flashing-green ready state buildings/units already
  use — README "the READY cameo state shows a flashing green tint"). No new
  ready art.
- **Guarded**: no existing demo grants a SW, so no existing sidebar render
  changes — the SW slot is drawn only when the house has a granted SW (§8).

## §7 The `--demo-superweapon` gate (the 12th), `superweapon_hash` & witnesses

**Script** (headless or rendered — bit-identical): on the temperate reference
map, build a fresh World, spawn a cluster of enemy target units (house 1) around
a chosen target cell within the nuke radius, and grant house 0 the Nuclear
Missile via `grant_superweapon` with the recovered params:

1. **CHARGE**: step the sim until the SW reaches READY, advancing the real
   recharge timer (witness `[sw] charging recharge_secs=… frames_to_ready=N`);
   assert `sw_ready(0)` becomes true (folded into the hash). If the recovered
   `RechargeTime`→frames is impractically large for a gate, a **demo-only**
   recharge scale is the documented fallback [RE-FALLBACK] — the FRAMEWORK always
   uses the real value; only the demo's grant may scale it. Default: charge the
   real value (a headless countdown of `RechargeTime·60·15` frames is
   milliseconds; the `--demo-naval` 9000-frame precedent).
2. **FIRE**: once READY, call `fire_superweapon(0, cx, cy)` at the target cell;
   assert it returns `true`, the timer resets to 0, and `ready` clears (witness
   `[sw] fired target=(cx,cy) reset_timer=yes`).
3. **DETONATE**: the enemy cluster within the nuke radius takes damage and units
   die (witness `[sw] detonate damaged=D killed=K in_radius=R`); assert D≥1 and
   K≥1.
4. **RECHARGE**: run a fixed window; the SW begins re-charging from 0 (witness
   `[sw] recharging timer=…`).

**`superweapon_hash`**: FNV-1a 64 (the established `fnv_mix` helper) folded at
fixed checkpoints (post-charge, post-fire, post-detonate, end) over: every unit's
`(cell_x, cell_y, alive, hp)` PLUS the firing house's SW `(quantized timer, ready,
type)` and the target cell `(cx, cy)` — exactly the state the phase adds. The
timer is folded as a quantized integer (e.g. `round(timer·kLogicFps)` frames) to
stay bit-stable across the dt accumulation. Two-run compute-and-self-compare
(`deterministic=yes`), **NO pinned literal in code** (the `--demo-naval`/
`--demo-aircraft` precedent); README records the stable value in the gate table
once landed.

**Wedge guards** (exit 1, so the gate can never pass vacuously): the SW actually
reached READY AND `fire_superweapon` returned `true` AND the detonation
damaged/killed ≥1 target unit — all three must hold, else exit 1. Witness log +
hash are pure sim state → rendered and headless runs byte-compare identical.

## §8 Byte-identity discipline

The **eleven existing permanent gates MUST stay bit-identical** (values in
Depends-on). CB-4's discipline is the *simplest since the AI phases* — it adds a
per-house subsystem that touches no hot path:

- **`Unit` and `combat.cpp` are literally unchanged.** No new `Unit` field (the
  detonation uses a transient stack `Unit`); `apply_spread_damage` and every
  other combat function are byte-for-byte identical, so CB-2's `--demo-aircraft`
  splash path is provably untouched. This is stronger than CB-1/2/3, each of
  which added inert `Unit` fields.
- **`world_hash` is untouched (strategy (a))**: `world_hash` (`main.cpp:3855`)
  reads exactly `(cell_x, cell_y, hp, alive)` per unit + building fields and is
  NOT edited; the new SW state is hashed ONLY by `superweapon_hash` in the new
  gate. No existing demo grants a SW, so no unit's `hp/alive` is perturbed by any
  detonation in any existing demo.
- **The SW registry is default-empty and opt-in**: `sw_houses_` is empty and
  `sw_any_` is false until `grant_superweapon`, which no existing demo calls.
- **`step_superweapon` is firewalled**: `if (!sw_any_) return;` is its first
  line — a strict no-op in every existing demo (the `step_cargo`/`step_flight`/
  `step_ai_houses` firewall shape). It is a pure INSERTION into `World::step`; no
  existing pass is reordered or edited.
- **The detonation fires only on an actual `fire_superweapon` call**, which no
  existing demo issues — so `apply_spread_damage` is never invoked with a SW
  detonator outside the new gate.
- **The sidebar SW slot is drawn only when the house has a granted SW** — no
  existing demo grants one, so no existing screenshot changes.
- **The 11-gate re-verify runs at the first additive commit (Task 3) AND at
  finalize (Task 7)**, isolating any regression to the commit that introduced
  it — the CB-1/2/3 discipline.

## §9 Testing strategy

- **Pure doctests** (host-only, hand-built inputs, no install):
  `tests/test_superweapon.cpp` — a `Superweapon` charges to READY at exactly
  `recharge_secs` and not before; a mid-charge `tick` is monotonic and never
  overshoots the cap; `on_fired` resets `timer` to 0 and clears `ready`;
  `is_ready` tracks the threshold; the `SuperweaponType` enum maps the recovered
  `[SuperWeaponTypes]` ids.
- **World-level tests** (the existing `test_world` pattern): `grant_superweapon`
  sets `sw_any_` and materializes the house record; `step_superweapon` advances
  the granted house's timer and leaves ungranted houses inert; a World that never
  grants a SW has `step_superweapon` as a no-op (state byte-identical to no-CB-4);
  `fire_superweapon` is refused (returns false, no damage) before READY and
  succeeds after; a fired detonation damages/kills enemy units in the radius,
  spares the firing house (enemies-only), and leaves out-of-radius units
  untouched; the falloff matches `apply_spread_damage` (a unit at the edge takes
  ~`PercentAtMax` damage, one at center takes full).
- **Integration**: the `--demo-superweapon` gate (§7) with wedge guards (READY
  reached AND fired AND ≥1 kill) + two-run `superweapon_hash` self-compare (no
  pinned literal); rendered vs headless byte-identical.
- **Regression**: all 11 existing gates byte-identical (values in Depends-on),
  re-run at the first additive commit AND at finalize; full suite both modes;
  arm64 NDK build with the new superweapon symbols `nm`-verified. Suite stays
  scoped/fast (the shared-fixture discipline).

## Out of scope / divergences (explicit)

- **Silo-building-grants-SW** — CB-4 grants the SW to a house DIRECTLY (like
  CB-1/2/3 spawned their exemplars directly); wiring "build the Nuke Silo (Soviet)
  → unlock the SW" through the prerequisite/production system is a carry-forward.
- **Other superweapon TYPES** — Iron Curtain (invulnerability), Chronosphere /
  Chrono Warp (teleport), Lightning Storm (over-time area), Psychic Dominator,
  Genetic Converter, Force Shield, Spy Plane, Para Drop, Psychic Reveal —
  DEFERRED. The framework (timer + target + effect) supports them later; CB-4
  ships only the area-damage detonation effect. The full `Type` enum is mapped in
  the hunt (§1 item 4) so each deferred kind starts from a mapped point.
- **Missile flight / launch visual / mushroom-cloud anim** — the effect is an
  instant area-damage detonation at the target cell (the established
  instant/simplified-delivery precedent, §5); the `GiantNukeUp`/`GiantNukeDown`
  flight, `DetonationAltitude`, and `NUKEANIM` are carry-forwards.
- **Lingering radiation** (`[NukePayload] RadLevel=500`) — the nuke's radiation
  field / over-time damage is out; CB-4 applies the one-shot `[NUKE]` splash only.
- **Friendly fire** — the reused `apply_spread_damage` is enemies-only; the real
  nuke damages friendlies too. The enemies-only splash is kept (a documented
  divergence); friendly-fire splash is a carry-forward.
- **Building splash** — if the hunt confirms the nuke damages buildings (§5 item
  3), a SW-only building-splash loop is added; otherwise building damage is a
  carry-forward. The gate targets units regardless.
- **Power-suspend** (`IsPowered=true`) — CB-4 charges unconditionally; the
  low-power charge-suspend behavior is mapped-for-later.
- **AI use of superweapons** — the AI does not fire SWs in CB-4 (carry-forward);
  `AIDefendAgainst=yes`/the AI SuperDefense system stays inert. The AI-3 inert
  superweapon-charge conditions (`ai-trigger.md` ct=5 IronCurtainCharged / ct=6
  ChronoSphereCharged, README "evaluate FALSE until a superweapon subsystem
  exists") are NOT wired to this framework in CB-4 — bridging them is a
  carry-forward (CB-4 grants only the Nuke, which neither condition names).
- **Multiple SWs per house / SW ownership transfer / EMP / one-time-vs-permanent
  SW** — out.

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real rulesmd /
  prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE precede all
  dependent tasks; every recovered constant cites `docs/re/superweapon.md`;
  [RE-FALLBACK] items become plan-amendment gates, never guesses.
- ELEVEN byte-identity gates stay green (§8); `--demo-superweapon` becomes the
  12th permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the
  install. Long rendered demos auto-background on short timeouts; NEVER pipe a
  native exe through `Select-Object -First N` (stops draining stdout → false
  hang). The plan's implementer briefs must order FOREGROUND synchronous runs
  with generous timeouts (the recorded prior-subagent stall failure mode).
- No game assets / GPL source / Ghidra output committed; recovered numeric
  constants ARE committable (the `[NukeSpecial]`/`[NukePayload]`/`[NUKE]` VALUES
  above already are).
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`

## Testing summary

| Layer | Tests |
|---|---|
| superweapon lifecycle (pure) | charges to READY at exactly `recharge_secs`; mid-charge tick monotonic, no overshoot; `on_fired` resets timer→0 + clears ready; `is_ready` tracks threshold; `SuperweaponType` enum maps `[SuperWeaponTypes]` |
| World integration | `grant_superweapon` sets `sw_any_` + materializes the record; `step_superweapon` advances granted houses, leaves others inert; no-op (byte-identical state) when no SW granted; `fire_superweapon` refused before READY, succeeds after, resets timer; detonation damages/kills enemies in radius, spares firing house, leaves out-of-radius untouched; falloff matches `apply_spread_damage` (edge ~`PercentAtMax`, center full) |
| integration | `--demo-superweapon` witnesses (charge → fire → detonate → recharge) + `superweapon_hash` two-run self-compare (no pinned literal) + wedge guards (READY AND fired AND ≥1 kill else exit 1); rendered ≡ headless |
| regression | 11 existing gates byte-identical (§8), re-verified at the first additive commit and at finalize; full suite both modes; arm64 `nm`-verify of superweapon symbols |
