# CB-4 Superweapons Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a **superweapon framework** to the from-scratch RA2 engine — a **per-house recharge-timer superweapon** (charges over the recovered `RechargeTime` → READY, resets after firing), **player target selection** (arm SW-targeting via an `N` keybind or a sidebar-cameo click when READY, then a map click fires at that cell), and a fired **area-damage detonation** at that cell that **REUSES the shipping `apply_spread_damage`** combat applicator via a **transient virtual detonator `Unit`** (no new damage math). Demonstrated deterministically on the **Soviet Nuclear Missile** (`[NukeSpecial]`, `Type=MultiMissile`) and gated by the **12th permanent byte-identity gate `--demo-superweapon`**. Spec: `docs/superpowers/specs/2026-07-19-superweapon-design.md` (commit 147b5bc).

**Architecture:** Task 1 recovers ground truth from gamemd.exe (YR 1.001) + the real rulesmd into `docs/re/superweapon.md` (RE-GATE before any dependent code): the SW timer/ready/fire lifecycle + `RechargeTime` units→frames, the `MultiMissile`/Nuke fire→effect with the `[NukePayload]`-vs-`[NukeCarrier]` damage attribution, target-selection/fire trigger, the full SW `Type` enum, and determinism/no-RNG — plus **art-verifying `NukeIcon`/`NUKEANIM`** in the real mixes and the **building-splash question** (does the nuke damage buildings in the radius?). Task 2 lands the pure SDL/World/RNG-free `engine/superweapon.{hpp,cpp}` — the `SuperweaponType` enum (full recovered roster, only `Nuke` active) + a `Superweapon` lifecycle struct (`tick`/`is_ready`/`on_fired`/`charge_fraction`) over plain scalars, doctest-covered. Task 3 grafts a **NEW AI-independent per-house registry** into `World` — `sw_houses_` + a `sw_any_` firewall (the `ai_houses_`/`ai_any_` shape, but INDEPENDENT so it works for the **player house 0**, which is never in `ai_houses_`), `grant_superweapon`, a firewalled `step_superweapon` pass, and read accessors; **11-gate re-verify at this first additive commit**. Task 4 lands the fired EFFECT — `fire_superweapon` builds the transient virtual detonator `Unit` (house + target-cell `fx/fy` + the four recovered warhead fields) and calls the EXISTING `apply_spread_damage`; because that applicator lives in `combat.cpp`'s **anonymous namespace** it must be **promoted to external linkage** (the ONE combat.cpp edit — linkage-only, body byte-for-byte unchanged, gates re-verified — see Global Constraints); `main.cpp` resolves the `[NukePayload]`/`[NUKE]` values into the grant params; building-splash per the RE-GATE decision. Task 5 wires the sidebar SW UI (`NukeIcon` cameo + `gclock2.shp` recharge clock, art-guarded) and the `N`-keybind / cameo-click arm → map-click-fire input. Task 6 adds the `--demo-superweapon` gate (charge → fire → detonate → recharge, two-run `superweapon_hash` self-compare, wedge guards). `Unit` and `world_hash` are **NOT touched** (strategy (a): SW state hashed ONLY by `superweapon_hash`). Task 7 finalizes.

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle, NDK `D:\android-ndk-r27c`), Ghidra headless (`analyzeHeadless.bat`, JAVA_HOME=`C:\Program Files\Java\jdk-21`, project at `D:\ra2-re`), SDL2 (wiring/UI layer only), `ra2assets::Rules`/`ra2assets::Ini` as the rulesmd substrate, reusing `engine/combat.cpp`'s `apply_spread_damage` (the CB-2 area-damage applicator, `docs/re/combat.md` §7) for the nuke detonation, the `ai_houses_`/`ai_any_` per-house registry + firewalled-`step` discipline (`docs/re/ai-base.md`) as the shape for the new `sw_houses_`/`sw_any_` registry, and the existing sidebar cameo + `gclock2.shp` build-clock chrome (`main.cpp`) for the SW cameo + recharge indicator.

## Global Constraints

(Copied verbatim from spec §Global constraints (project-wide) — every task's requirements implicitly include this section.)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real rulesmd / prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE precede all dependent tasks; every recovered constant cites `docs/re/superweapon.md`; [RE-FALLBACK] items become plan-amendment gates, never guesses.
- ELEVEN byte-identity gates stay green (spec §8); `--demo-superweapon` becomes the 12th permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127). `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the install. Long rendered demos auto-background on short timeouts; NEVER pipe a native exe through `Select-Object -First N` (stops draining stdout → false hang).
- No game assets / GPL source / Ghidra output committed; recovered numeric constants ARE committable (the `[NukeSpecial]`/`[NukePayload]`/`[NUKE]` VALUES in the spec already are).
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs — the plan's briefs order FOREGROUND synchronous runs with generous timeouts.

**Byte-identity discipline for an ADD-BESIDE feature (spec §8 — copied, load-bearing for every task):**

- **The 11 existing permanent gates MUST stay bit-identical:** `--demo-combat` ends `deterministic=yes logs=78`; `--demo-tank` ends `deterministic=yes logs=144`; `--demo-base` prints `hash=5273b5e71ca009e1`; `--demo-econ` prints `econ_hash=7fed96cb98b68017`; `--demo-prod` prints `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` prints `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` prints `ai_team_hash=312e92ed862a5e74`; `--demo-ai-trigger` prints `ai_trigger_hash=8334a6ca1531cabc`; `--demo-transport` prints `transport_hash=fe500f946e913645`; `--demo-aircraft` prints `aircraft_hash=a69642564cf64773`; `--demo-naval` prints `naval_hash=6aee2c82bc024e86`. Any change to their output is a defect in YOUR change.
- **`Unit` is LITERALLY unchanged.** SW state is purely per-house + World-level; the detonation uses a **transient stack `Unit`**, never a persistent field. No new `Unit` field is added (stronger than CB-1/2/3, which each added inert `Unit` fields). `world_hash` (`main.cpp:3855`, reads only `cell_x/cell_y/hp/alive` per unit + building fields) is therefore untouched by construction.
- **`combat.cpp` receives ONE linkage-only edit — its BEHAVIOR is byte-for-byte unchanged.** The spec's premise "`apply_spread_damage` is directly reusable / `combat.cpp` is not edited at all" assumes the applicator has external linkage; it does NOT — it lives in `combat.cpp`'s anonymous namespace (`combat.cpp:12–152`), so `world.cpp` cannot call it. The ONLY faithful way to REUSE the exact function (rather than reimplement the falloff loop — which §5/§8 forbid) is to **promote `apply_spread_damage` from the anonymous namespace to the `ra2engine` namespace and declare it in `combat.hpp`** (Task 4). This is **linkage-only**: the function body and its sole existing caller (`try_fire`) are byte-for-byte identical, so every existing combat path computes the same results and all 11 gate OUTPUTS stay bit-identical (re-verified empirically at Task 3 AND Task 4). This preserves the invariant the spec actually cares about — identical gate output + one source of truth for the damage math — and is the one documented deviation from the spec's "`combat.cpp` literally untouched" wording. See Task 4 Step 1.
- **`world_hash` is untouched (strategy (a))**: the new SW state is hashed ONLY by `superweapon_hash` in the new gate. No existing demo grants a SW, so no unit's `hp/alive` is perturbed by any detonation in any existing demo.
- **The SW registry is default-empty and opt-in**: `sw_houses_` is empty and `sw_any_` is false until `grant_superweapon`, which no existing demo calls.
- **`step_superweapon` is firewalled**: `if (!sw_any_) return;` is its first line — a strict no-op in every existing demo (the `step_cargo`/`step_flight`/`step_ai_houses` firewall shape). It is a pure INSERTION into `World::step` (after `step_flight`); no existing pass is reordered or edited.
- **The detonation fires only on an actual `fire_superweapon` call**, which no existing demo issues — so `apply_spread_damage` is never invoked with a SW detonator outside the new gate.
- **The sidebar SW slot is drawn only when the house has a granted SW** — no existing demo grants one, so no existing screenshot changes.
- **The 11-gate re-verify runs at the first additive commit (Task 3) AND at finalize (Task 7)**, isolating any regression to the commit that introduced it — the CB-1/2/3 discipline. Task 4 (the combat.cpp linkage edit + the live fire path + `resolve_superweapon`) ALSO re-verifies all eleven.

**Plan-operational addenda (CB-1/2/3 precedent):**

- **[RE-GATE] protocol — STATUS: PENDING.** Tasks 2–6 are written with best-known PRE-HUNT values; each consuming point is marked `[RE-GATE: superweapon.md §N]` with its fallback inline. After the Task-1 hunt lands (`docs/re/superweapon.md`), the **controller** runs the RE-GATE amendment step (Task 1, Step 8): diff the hunt against every `[RE-GATE: superweapon.md §N]` block, amend the plan, and flip each marker to `[RE-GATE APPLIED: superweapon.md §N]` — committed BEFORE dispatching Task 2. The **`RechargeTime` units→frames factor** (§1 item 1), the **`[NukePayload]`-vs-`[NukeCarrier]` damage attribution** (§1 item 2), and the **building-splash decision** (§5.3) are the likely [RE-FALLBACK]s / design-calls.
- Run every build/test/demo **FOREGROUND, synchronously, blocking** (generous timeout, e.g. 600000 ms — rendered demos included) — do NOT background test runs and wait; prior phases lost subagents to that. NEVER pipe a native exe through `Select-Object -First N`. Run the twelve gate demos INDIVIDUALLY (a 12-in-one chain exceeds 600 s).
- Desktop build: `cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine`. Tests: `D:\ra2-android\build\ra2assets_tests.exe --test-case="<filter>*"`. Engine exe: `D:\ra2-android\build\ra2engine.exe`. doctest filters are name-based.
- Tests skip cleanly without `RA2_INSTALL_DIR` — doctest `MESSAGE` + `return`. The Task 2 pure tests + the Task 3/4 pure-World tests use hand-built inputs and run host-only (no install needed).
- Scratch files (hunt scripts, extracted rulesmd, decompile dumps, screenshots) go to the session scratchpad, never the repo.
- Model assignments: implementation → opus; specs/plans/review → fable; easy/mechanical tasks → sonnet.

### Approved scope decisions (spec §Approved scope decisions)

- **SW framework:** a per-house superweapon with a recharge timer that counts up over the recovered `RechargeTime` → READY, and **resets to 0 after firing** (the lifecycle is §1 item 1). A pure `engine/superweapon` module owns the lifecycle (`tick`, `is_ready`, `on_fired`, `charge_fraction`); `World` owns the per-house registry and calls it.
- **Per-house SW state:** a NEW `std::vector<HouseSuperweapon> sw_houses_` (index = house id, grown on demand) + a `bool sw_any_ = false` firewall — the `ai_houses_`/`ai_any_` pattern, but INDEPENDENT of the AI registry so it works for the **player house 0** (which is never in `ai_houses_`). Default INERT (no house has a SW until `grant_superweapon`).
- **Grant:** CB-4 grants the SW to a house DIRECTLY via `World::grant_superweapon(house, type, recharge_secs, effect-params)` (like CB-1/2/3 spawned their exemplars directly) — sets `sw_any_ = true`. "Build the Nuke Silo → unlock the SW" is a carry-forward (§ Out of scope).
- **Timer tick:** `step_superweapon(dt)` — a NEW `World::step` pass, firewalled behind `if (!sw_any_) return;`, that advances each granted house's `timer` toward `recharge_secs` and flips `ready` at the threshold. No RNG (a deterministic countdown; confirm §1 item 5).
- **Target selection:** when READY, the player arms SW-targeting (the `N` keybind, or a click on the sidebar SW cameo), then a left-click on a map cell fires at that cell, reusing the EXISTING screen→cell picking (`main.cpp` input). The demo scripts this deterministically via a direct `World::fire_superweapon(house, cx, cy)` call (no synthetic input — the `--demo-naval` `order_attack` precedent).
- **Fire:** `World::fire_superweapon(house, cx, cy)` — no-op unless that house's SW is READY; on fire it detonates at the cell (§5), then calls `sw.on_fired()` to reset the timer to 0 and `ready=false`. Returns whether it fired (for the wedge guard).
- **The fired EFFECT:** an area-damage detonation at the target cell, REUSING `apply_spread_damage` unchanged: build a transient virtual detonator `Unit` (house = firing house; `fx/fy` = target cell; `w_damage`/`w_cell_spread`/`w_percent_at_max`/`w_verses` = the recovered `[NukePayload] Damage=600` + `[NUKE] CellSpread=10, PercentAtMax=.02, Verses`), and call `apply_spread_damage(units, occ, virt, virt)`. No new damage system; `combat.cpp`'s applicator BODY unchanged (only its linkage; see Global Constraints).
- **Sidebar SW UI:** a SW cameo (`NukeIcon`, art-guarded) + a recharge-clock indicator drawn with the EXISTING `gclock2.shp` 55-frame chrome — `gframe = round(charge_fraction · 54)` while charging, the existing flashing-green READY tint when charged. No new font/UI systems (§6).
- **Exemplar:** Nuclear Missile `[NukeSpecial]` `Type=MultiMissile` (Soviet). Art-guarded on `NukeIcon`; if the cameo/anim regresses, the SW cameo drops to a placeholder tile and the sim runs headless (the sim logic is exemplar-art-independent and pure-tested regardless).
- **Verification:** new deterministic gate `--demo-superweapon` (the 12th) + `superweapon_hash` (FNV over the SW timer/ready state + the target cell + the post-detonation per-unit `hp/alive`), two-run compute-and-self-compare (NO pinned literal in code); wedge guards (§7): the SW must charge to READY AND fire AND the detonation must damage/kill target units, else exit 1.
- **Byte-identity:** Strategy (a): SW state excluded from `world_hash` (untouched); `Unit` untouched; `combat.cpp` behavior unchanged (linkage-only edit); `step_superweapon` firewalled behind `sw_any_` (false in every existing demo → a strict no-op); the detonation fires only on `fire_superweapon` (which no existing demo issues). Re-verify all 11 gates at the first additive commit AND at finalize (§8).

### Decomposition tripwire (spec §2)

The framework is small (a timer + a fire order + one detonation call) and this is the single coherent ~7-task phase. IF the sidebar SW UI/input (Task 5) fans out — a full targeting-cursor art path, cameo hit-test plumbing, a ready-flash animation beyond the existing chrome — the phase splits at the natural seam: **(A) the SW framework + effect** (Tasks 1–4, 6 with a headless-only `--demo-superweapon` that grants, charges, and fires via the direct `fire_superweapon` call, no sidebar UI) then **(B) the sidebar SW UI + input arming** (Task 5 + a rendered witness). Default is the single phase below.

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/superweapon.md` (create, Task 1) | Calibration authority: the SW timer/ready/fire lifecycle + `RechargeTime` units→frames; the `MultiMissile`/Nuke fire→effect + `[NukePayload]`-vs-`[NukeCarrier]` attribution; target-selection/fire trigger; the full SW `Type` enum; determinism/no-RNG; the `NukeIcon`/`NUKEANIM` art-verify; the building-splash question; a located-functions table + a per-finding confidence table + a "deviations we take" section |
| `engine/superweapon.hpp` + `engine/superweapon.cpp` (create, Task 2) | Pure CB-4 framework: `enum class SuperweaponType`, `struct Superweapon` (inline lifecycle) + the recovered enum↔`[SuperWeaponTypes]`-id mapping helpers. No SDL/World/RNG. Registered in BOTH cmake files |
| `tests/test_superweapon.cpp` (create, Task 2) | Pure doctests: charges to READY at exactly `recharge_secs`, no overshoot, `on_fired` resets, enum order + id mapping |
| `engine/world.hpp` (modify, Tasks 3–4) | Task 3: `#include "superweapon.hpp"` + `<array>`; `HouseSuperweapon` (private); `sw_houses_`/`sw_any_` members; `grant_superweapon`/`step_superweapon` decls; `sw_ready`/`sw_charge_fraction`/`sw_state` accessors. Task 4: `fire_superweapon` decl |
| `engine/world.cpp` (modify, Tasks 3–4) | Task 3: `grant_superweapon`, `step_superweapon`, the accessors, the `World::step` insertion. Task 4: `fire_superweapon` (transient detonator + `apply_spread_damage`) + the building-splash [RE-GATE] |
| `engine/combat.hpp` + `engine/combat.cpp` (modify, Task 4) | The ONE linkage-only edit: declare `apply_spread_damage` in `combat.hpp`; move its definition out of `combat.cpp`'s anonymous namespace into `ra2engine`. Body byte-for-byte unchanged |
| `engine/main.cpp` (modify, Tasks 4–6) | Task 4: `SuperweaponParams` + `resolve_superweapon`. Task 5: `NukeIcon` cameo load + SW-slot draw + the `N`-keybind/cameo-click arm → map-click-fire wiring. Task 6: `superweapon_fold` + `simulate_superweapon` + `run_demo_superweapon` + the `--demo-superweapon` dispatch |
| `engine/input.hpp` + `engine/input.cpp` (modify, Task 5) | SW-targeting mode mirroring placement mode: `arm_superweapon`/`sw_targeting`/`cancel_superweapon` + `InputActions::{sw_fire_confirmed,sw_cx,sw_cy}` reported on a left-click while armed (reusing the existing screen→cell pick) |
| `tests/test_world.cpp` (append, Tasks 3–4) | World-level pins for grant/step/inert + fire/detonation/falloff |
| `cmake/ra2_sources.cmake` + `CMakeLists.txt` (modify, Task 2) | register `engine/superweapon.cpp` in `RA2_ENGINE_SOURCES` + `tests/test_superweapon.cpp engine/superweapon.cpp` in `CMakeLists.txt` |
| `README.md` (modify, Task 7) | CB-4 section + 12th gate row + CLI bullet + deviations |

**Module-placement decision:** `engine/superweapon.{hpp,cpp}` lives in `ra2engine` (the `Superweapon`/`SuperweaponType` are engine types `World` embeds). The `struct Superweapon` lifecycle methods are **header-inline** (small scalar arithmetic — the `Unit::moving()` precedent), so any TU that includes the header (`world.cpp`, `main.cpp`, the tests) gets them with NO link dependency on `superweapon.cpp`; `superweapon.cpp` holds the two genuinely-used free functions (`superweapon_type_name` / `superweapon_type_from_id`). This avoids listing `superweapon.cpp` twice for `ra2assets_tests` (which would duplicate-symbol): it is registered ONCE via `RA2_ENGINE_SOURCES` (→ `ra2engine`) and ONCE on the `test_superweapon.cpp` line (→ `ra2assets_tests`); `world.cpp` uses only the inline methods, so `test_world.cpp`'s existing `engine/world.cpp` line needs no change.

---

### Task 1: Ghidra hunt → docs/re/superweapon.md

**Files:**
- Create: `docs/re/superweapon.md`
- Scratch: extracted rulesmd.ini + hunt scripts + decompile dumps under the session scratchpad (never committed; nothing off-repo committed)

**Interfaces:**
- Consumes: gamemd.exe YR 1.001 in the Ghidra project at `D:\ra2-re` (already analyzed in prior hunts; `analyzeHeadless.bat` + `-process gamemd.exe -noanalysis -postScript <script>.py`, `JAVA_HOME="C:\Program Files\Java\jdk-21"`, FOREGROUND, generous timeouts — headless runs take minutes), plus the real rulesmd.ini extracted with `D:\ra2-android\build\ra2extract.exe` from the install into the scratchpad, and `ra2extract` for the art-verify.
- Known content anchors to seed the hunt — the **literal INI-key strings** the binary loads: `"RechargeTime"`, `"Type"`, `"WeaponType"`, `"Action"`, `"SidebarImage"`, `"ShowTimer"`, `"IsPowered"`, `"AIDefendAgainst"`, `"LineMultiplier"`, `"Range"`; the SW type-name strings `"NukeSpecial"`, `"MultiMissile"`, `"IronCurtain"`, `"ChronoSphere"`, `"LightningStorm"`, `"ParaDrop"`, `"PsychicDominator"`, `"SpyPlane"`, …; the `[SuperWeaponTypes]` list reader (a `RulesClass::LoadFromINI` fragment, the `combat.md` §3 precedent for the `[CombatDamage]`/`[General]` readers); and `SuperClass` / `SuperWeaponTypeClass` (the per-instance timer + the type descriptor). Prior located functions to xref-walk from (`docs/re/combat.md` entry map): `FUN_00489190` (the `CellSpread` damage-falloff `apply_spread_damage` maps to — do NOT re-open; confirm the nuke feeds it), the `RulesClass::LoadFromINI` readers, `damage_vs`/`ReceiveDamage`. **Ares/YRpp names (`SuperClass`, `SuperWeaponTypeClass`, `NukeFlash`, `SW_*`) are UNTRUSTED prose hints only** — `combat.md`'s finding stands: Ares' hardcoded "YR 1.001" addresses do NOT transfer to this exact binary, so content-anchor everything. **Do NOT guess numeric constants** (the `RechargeTime` units→frames factor, whether firing resets to 0 or a partial value) — they are hunt outputs. The `[NukeSpecial]`/`[NukePayload]`/`[NUKE]` VALUES quoted in the spec are committable ground truth; their INTERPRETATION is the hunt's.
- Produces: `docs/re/superweapon.md` — sections §1–§5 mirroring the spec's §1 items 1–5, each finding **CONFIRMED / PROBABLE / [RE-FALLBACK]** with function address + decompile-excerpt reasoning, a per-finding confidence table, a located-functions table (name, address, one-line role), and a "deviations we take" section (mirroring `combat.md`/`naval.md`).

**Questions the doc MUST answer** (spec §1 items 1–5, verbatim intent):

1. **The SW timer/ready/fire lifecycle** (`SuperClass`): how `RechargeTime` is converted to a countdown (**expected: minutes → `RechargeTime·60·kLogicFps` frames**, the `ai-base.md` §1 `BuildDelay` minutes→frames precedent — CONFIRM the unit and factor); the charge/ready predicate (`IsCharged` / the `+offset` timer field); whether **firing resets the timer to 0** (expected: yes, a full re-charge) or to a partial value; the per-house instancing (one `SuperClass` per house per owned SW type). `IsPowered=true` → the low-power suspend is mapped-for-later (CB-4 charges unconditionally, § OoS).
2. **The nuclear-missile SW fire → effect** (`Type=MultiMissile`): the fire path from `Action=Nuke` target-pick to the detonation. CONFIRM that the target-cell damage is the `[NukePayload]` (`Damage=600`, `Warhead=NUKE`) down-strike, NOT the SW's own `WeaponType=NukeCarrier` up-launch (`Warhead=NukeMaker`, which only "shoots straight up"). Then the effect maps to `apply_spread_damage` via `[NUKE] CellSpread=10, PercentAtMax=.02, Verses[]` (`combat.md` §7). The exact `CellSpread` falloff (`combat.md` §7's un-unwound `FUN_00489190`) stays the adjudicated linear 100%→`PercentAtMax` approximation CB-2 already ships — CB-4 does not re-open it. `RadLevel=500` is out of scope; map it for later.
3. **Target selection + fire trigger**: how `Action=Nuke` arms the targeting cursor and how a map click commits the target cell to the fire; any restrictions (`Range=7`/`LineMultiplier=2` — expected cosmetic/targeting-aid for the nuke, not a range cap on where it can strike; CONFIRM). The screen→cell pick reuses the existing input path — the hunt need only confirm the arm→click→fire ordering.
4. **The full recovered SW `Type` enum** (`MultiMissile`/`IronCurtain`/`LightningStorm`/`ChronoSphere`/`ChronoWarp`/`ParaDrop`/`PsychicDominator`/`GeneticConverter`/`ForceShield`/`PsychicReveal`/`SpyPlane`, from `[SuperWeaponTypes]`) — RE-documented so the deferred effect kinds have a mapped start (the `ai-team.md` §3 opcode-table precedent: document the full enum, implement one). Confirm the `[SuperWeaponTypes]` id → SW-section → `Type` mapping (`1=NukeSpecial`→`MultiMissile`, `2=IronCurtainSpecial`→`IronCurtain`, … `6=ParaDropSpecial`/`7=AmericanParaDropSpecial`→`ParaDrop`, `9=SpyPlaneSpecial`→`SpyPlane`, `12=PsychicRevealSpecial`→`PsychicReveal`). CB-4 implements ONLY the area-damage `MultiMissile`/Nuke `Type`.
5. **Determinism + byte-identity**: how `SuperClass` integrates without perturbing the existing sim (expected: an independent per-house timer + a fire-triggered detonation — no existing step reordered); confirm **no RNG** on the SW charge or the nuke detonation critical path (`[NukePayload]`/`[GiantNukeDown]` declare no scatter fields; `apply_spread_damage` is already RNG-free — pin it). Any per-frame SW work the original does that an existing house would incur (expected: none unless a SW is granted).

**Steps:**

- [ ] **Step 1: Re-read prior hunt method.** Read `docs/re/workflow.md` (the headless recipe), `docs/re/combat.md` (§7 the `apply_spread_damage`/`CellSpread` falloff the nuke reuses; §3 the `RulesClass::LoadFromINI` reader technique; the doc format), `docs/re/ai-base.md` §1 (the `BuildDelay` minutes→frames precedent for the `RechargeTime` conversion), and `docs/re/naval.md` (the CB-3 hunt's format + confidence/deviations sections). Do NOT trust YRpp/Ares addresses; use them only as prose hints.

- [ ] **Step 2: Extract the real rulesmd.ini + record the ground-truth values.** Build the extractor if stale, then pull rulesmd from the install mixes into the scratchpad (FOREGROUND, blocking):

```powershell
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/rulesmd.ini "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\rulesmd.ini"
```

If `localmd.mix/rulesmd.ini` misses, walk the same nested candidates `tests/test_rules.cpp`'s real-rulesmd case uses (`expandmd01..05.mix`, `expandmd99.mix`, `cachemd.mix`, `conqmd.mix`, then top level). Record IN SCRATCH NOTES, verbatim: `[SuperWeaponTypes]`, `[NukeSpecial]`, `[NukeCarrier]`, `[NukeMaker]`, `[NukePayload]`, `[NUKE]`, `[GiantNukeUp]`, `[GiantNukeDown]` (all keys). Cross-check against the spec's quoted values (committable). The committable findings land in superweapon.md as prose + numbers; the raw ini stays in scratch.

- [ ] **Step 3: Anchor sweep (headless scripts, FOREGROUND, generous timeouts — do NOT background).** Per workflow.md's recipe, from the string data refs for `"RechargeTime"`/`"Type"`/`"WeaponType"`/`"Action"`/`"NukeSpecial"`/`"MultiMissile"` locate the `SuperWeaponTypeClass` INI-loader offsets (the `RulesClass::LoadFromINI` string-ref→offset technique), then xref-walk the field reads to the consumers: (a) the **`SuperClass` timer** — how `RechargeTime` becomes a countdown, the charge/ready predicate, and the fire-reset value (§1); (b) the **`MultiMissile`/Nuke fire path** — from `Action=Nuke` target-pick to the down-strike, confirming `[NukePayload]`/`[NUKE]` (not `[NukeCarrier]`) is the damaging leg and that it reaches the `CellSpread` falloff `FUN_00489190` `apply_spread_damage` maps to (§2); (c) the **`[SuperWeaponTypes]` list reader** + the `Type=` enum (§4); (d) the **target arm→click→fire ordering** (§3); (e) the **determinism** — the SW step ordering + any RNG consult on the charge/detonation path (§5). Dump decompiled candidates to the scratchpad.

- [ ] **Step 4: Answer §1 items 1–5.** Iterate scripts until each item is CONFIRMED or PROBABLE with evidence (function address + decompile-excerpt reasoning), or explicitly [RE-FALLBACK] with the adjudicated fallback stated. Item 1 must state the `RechargeTime` unit + the exact factor to frames/seconds (or [RE-FALLBACK] minutes→`·60`). Item 2 must state plainly whether `[NukePayload]`/`[NUKE]` or `[NukeCarrier]`/`[NukeMaker]` detonates at the target cell (or [RE-FALLBACK] `[NukePayload]`). Item 4 must give the `[SuperWeaponTypes]` id → `Type` mapping.

- [ ] **Step 5 (CRITICAL — art-verify).** With `ra2extract`, verify **`NukeIcon`** (the SW cameo, in `cameo.mix`/`sidebarmd`) and **`NUKEANIM`** (the `[NUKE] AnimList`, in the art mixes) are present on this install. Record the result in superweapon.md. If `NukeIcon` is absent that is a **demo-art [RE-FALLBACK]** (the SW cameo drops to a placeholder tile; the sim is art-independent and the gate runs headless regardless) — surface it in the RE-GATE (Step 8). Example probe (FOREGROUND, blocking):

```powershell
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --list | Select-String -Pattern "nukeicon|nukeanim|nukmsl"
```

(This filters a bounded, already-completed `--list` dump through `Select-String` — NOT the forbidden pattern of piping a LIVE simulation exe's stdout through `Select-Object -First N`; `ra2extract --list` writes finite output and exits.)

- [ ] **Step 6 (CRITICAL — the building-splash question, §5.3).** Record in superweapon.md §2: **does the nuke damage BUILDINGS in the radius?** The reused `apply_spread_damage` damages `units` only (the CB-2 aerial bomb). If the hunt confirms buildings take splash (expected: yes, a nuke levels a base), the RE-GATE amendment (Step 8) turns Task 4's `[RE-GATE: superweapon.md §5.3]` building-splash block into an **added, SW-only** building loop mirroring the unit falloff; otherwise CB-4 damages units only and building-splash is a carry-forward. State the finding + the adjudicated decision. (The gate targets a cluster of UNITS regardless, so it is building-splash-independent.)

- [ ] **Step 7: Write `docs/re/superweapon.md`.** Sections §1–§5 mirroring the items above; the per-finding confidence table; the located-functions table; the "deviations we take" section (expected entries: instant detonation vs the `GiantNukeUp`/`GiantNukeDown` missile flight + `DetonationAltitude` + `NUKEANIM` mushroom cloud; enemies-only splash vs the real nuke hitting friendlies; `RadLevel=500` lingering radiation dropped; the linear `CellSpread` falloff approximation inherited from CB-2; the full `Type` enum documented but only `MultiMissile`/Nuke implemented; SW granted directly, not via a Nuke Silo).

- [ ] **Step 8 (CONTROLLER, not the implementer): RE-GATE amendment. STATUS: PENDING.** After the hunt lands (`docs/re/superweapon.md`), the controller diffs it against every `[RE-GATE: superweapon.md §N]` block in Tasks 2–6, amends the plan (with the Fable trailer) BEFORE dispatching Task 2, and flips each marker to `[RE-GATE APPLIED: superweapon.md §N]` (the CB-1/2/3 precedent). Expected adjudications: (1) the **`RechargeTime`→frames/secs factor** (§1) — CONFIRM or [RE-FALLBACK] minutes→`·60` (Task 2/4 `recharge_secs` + `resolve_superweapon`); (2) the **damage attribution** (§2) — CONFIRM `[NukePayload]`/`[NUKE]` is the down-strike or [RE-FALLBACK] to it (Task 4 `resolve_superweapon`); (3) the **SW `Type` enum + id mapping** (§4) — CONFIRM the `[SuperWeaponTypes]` ids (Task 2 enum + `superweapon_type_from_id`); (4) the **building-splash decision** (§5.3) — CONFIRM buildings take splash → add the SW-only building loop in Task 4, or units-only → leave the [RE-GATE] block as a carry-forward comment; (5) the **`NukeIcon`/`NUKEANIM` art** (art-verify) — present → Task 5 loads them; absent → placeholder-tile fallback. If a fire-reset-to-partial value or a non-`apply_spread_damage` effect path is discovered, amend Task 2/4 accordingly.

---

### Task 2: pure engine/superweapon.{hpp,cpp} — the SuperweaponType enum + Superweapon lifecycle

**Files:**
- Create: `engine/superweapon.hpp`, `engine/superweapon.cpp`, `tests/test_superweapon.cpp`
- Modify: `cmake/ra2_sources.cmake`, `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (pure — no SDL/World/RNG dependency).
- Produces — `engine/superweapon.hpp`, complete:

```cpp
#pragma once
// CB-4 Superweapon framework [RE: docs/re/superweapon.md §1/§4]: a pure,
// SDL/World/RNG-free per-house recharge-timer lifecycle + the recovered
// superweapon type roster. World embeds a HouseSuperweapon per granted house
// (world.hpp); this header owns only the type-agnostic lifecycle arithmetic.
// The struct methods are header-inline (small scalar math -- the Unit::moving()
// precedent) so any including TU gets them with no link dependency on the .cpp;
// superweapon.cpp holds the enum<->id mapping helpers.
#include <cstdint>

namespace ra2engine {

// The superweapon roster, from the real rulesmd [SuperWeaponTypes]
// [RE-GATE: superweapon.md §4 -- the hunt CONFIRMS the id->Type mapping; the
// FALLBACK below is the pre-hunt reading of [SuperWeaponTypes]]. None = 0 so a
// value-initialized record is inert. CB-4 acts ONLY on Nuke (Type=MultiMissile);
// every other value is a mapped enum with no CB-4 effect (deferred, spec §Out).
enum class SuperweaponType {
    None             = 0,
    Nuke             = 1,   // [NukeSpecial]           Type=MultiMissile  (ACTIVE)
    IronCurtain      = 2,   // [IronCurtainSpecial]    Type=IronCurtain
    LightningStorm   = 3,   // [LightningStormSpecial] Type=LightningStorm
    ChronoSphere     = 4,   // [ChronoSphereSpecial]   Type=ChronoSphere
    ChronoWarp       = 5,   // [ChronoWarpSpecial]     Type=ChronoWarp
    ParaDrop         = 6,   // [ParaDropSpecial]/[AmericanParaDropSpecial]
    PsychicDominator = 7,   // [PsychicDominatorSpecial]
    GeneticConverter = 8,   // [GeneticConverterSpecial]
    ForceShield      = 9,   // [ForceShieldSpecial]
    PsychicReveal    = 10,  // [PsychicRevealSpecial]
    SpyPlane         = 11,  // [SpyPlaneSpecial]
};

// The pure per-superweapon recharge lifecycle over plain scalars (spec §2 data
// model). `recharge_secs` is the recovered RechargeTime already converted to
// SECONDS [RE-GATE: superweapon.md §1 -- minutes->secs factor; main.cpp's
// resolve_superweapon does the RechargeTime*60 conversion]. No RNG, no World dep.
struct Superweapon {
    SuperweaponType type = SuperweaponType::None;
    double recharge_secs = 0.0;
    double timer = 0.0;
    bool ready = false;

    // Advance the charge by dt seconds: accumulate, clamp at recharge_secs (no
    // overshoot), and latch `ready` at the threshold. No-op once ready or when
    // uninitialized (type None). A deterministic countdown -- no RNG (§1 item 5).
    void tick(double dt) {
        if (type == SuperweaponType::None || ready) return;
        timer += dt;
        if (timer >= recharge_secs) {
            timer = recharge_secs;   // clamp -- never overshoot the cap
            ready = true;
        }
    }
    bool is_ready() const { return ready; }
    // Consume a fire: full re-charge from 0 [RE-GATE: superweapon.md §1 -- the
    // hunt CONFIRMS reset-to-0 vs reset-to-partial; FALLBACK reset-to-0].
    void on_fired() {
        timer = 0.0;
        ready = false;
    }
    // 0..1 charge progress for the sidebar recharge clock (§6). Clamped.
    double charge_fraction() const {
        if (recharge_secs <= 0.0) return ready ? 1.0 : 0.0;
        double f = timer / recharge_secs;
        return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    }
};

// The display name of a SuperweaponType (witnesses / RE-doc cross-checks).
const char* superweapon_type_name(SuperweaponType t);

// Map a [SuperWeaponTypes] roster id (1..12 in the real rulesmd) to a
// SuperweaponType [RE-GATE: superweapon.md §4]. Unknown/absent ids -> None.
SuperweaponType superweapon_type_from_id(int id);

}  // namespace ra2engine
```

- Produces — `engine/superweapon.cpp`, complete:

```cpp
#include "superweapon.hpp"

namespace ra2engine {

const char* superweapon_type_name(SuperweaponType t) {
    switch (t) {
        case SuperweaponType::None:             return "None";
        case SuperweaponType::Nuke:             return "Nuke";
        case SuperweaponType::IronCurtain:      return "IronCurtain";
        case SuperweaponType::LightningStorm:   return "LightningStorm";
        case SuperweaponType::ChronoSphere:     return "ChronoSphere";
        case SuperweaponType::ChronoWarp:       return "ChronoWarp";
        case SuperweaponType::ParaDrop:         return "ParaDrop";
        case SuperweaponType::PsychicDominator: return "PsychicDominator";
        case SuperweaponType::GeneticConverter: return "GeneticConverter";
        case SuperweaponType::ForceShield:      return "ForceShield";
        case SuperweaponType::PsychicReveal:    return "PsychicReveal";
        case SuperweaponType::SpyPlane:         return "SpyPlane";
    }
    return "None";
}

// [RE-GATE: superweapon.md §4] the recovered [SuperWeaponTypes] roster:
// 1=NukeSpecial, 2=IronCurtainSpecial, 3=LightningStormSpecial,
// 4=ChronoSphereSpecial, 5=ChronoWarpSpecial, 6=ParaDropSpecial,
// 7=AmericanParaDropSpecial, 8=PsychicDominatorSpecial, 9=SpyPlaneSpecial,
// 10=GeneticConverterSpecial, 11=ForceShieldSpecial, 12=PsychicRevealSpecial.
// 6 and 7 are both ParaDrop kinds; 9 is SpyPlane. Only id 1 (Nuke) is active.
SuperweaponType superweapon_type_from_id(int id) {
    switch (id) {
        case 1:  return SuperweaponType::Nuke;
        case 2:  return SuperweaponType::IronCurtain;
        case 3:  return SuperweaponType::LightningStorm;
        case 4:  return SuperweaponType::ChronoSphere;
        case 5:  return SuperweaponType::ChronoWarp;
        case 6:  return SuperweaponType::ParaDrop;           // ParaDropSpecial
        case 7:  return SuperweaponType::ParaDrop;           // AmericanParaDropSpecial
        case 8:  return SuperweaponType::PsychicDominator;
        case 9:  return SuperweaponType::SpyPlane;           // SpyPlaneSpecial
        case 10: return SuperweaponType::GeneticConverter;
        case 11: return SuperweaponType::ForceShield;
        case 12: return SuperweaponType::PsychicReveal;
        default: return SuperweaponType::None;
    }
}

}  // namespace ra2engine
```

- CMake registration (BOTH builds — the global constraint):
  - `cmake/ra2_sources.cmake`: append `    ${RA2_ROOT}/engine/superweapon.cpp` to `RA2_ENGINE_SOURCES` (after the last engine line, e.g. `${RA2_ROOT}/engine/ai_trigger.cpp`). This compiles it into `ra2engine` (via `RA2_ENGINE_SOURCES`) and into the `ra2assets` library (via `RA2_SOURCES` = `RA2_LIB_SOURCES`? — NO: `superweapon.cpp` is engine, so it belongs in `RA2_ENGINE_SOURCES` only; `ra2engine` links it, `main.cpp` uses it).
  - `CMakeLists.txt`: after the last `target_sources(ra2assets_tests PRIVATE tests/test_*.cpp engine/*.cpp)` line, add:

```cmake
target_sources(ra2assets_tests PRIVATE tests/test_superweapon.cpp engine/superweapon.cpp)
```

  (This is the sole `engine/superweapon.cpp` entry for `ra2assets_tests` — `world.cpp` uses only the header-inline lifecycle methods, so `test_world.cpp`'s existing `engine/world.cpp` line needs no `superweapon.cpp` addition and there is no duplicate-source risk. The mapping helpers in `superweapon.cpp` are exercised by `test_superweapon.cpp`.)

**Steps:**

- [ ] **Step 1: Write the failing tests.** Create `tests/test_superweapon.cpp` (complete file — host-only, hand-built inputs, no install):

```cpp
#include "doctest.h"

#include "superweapon.hpp"
using namespace ra2engine;

TEST_CASE("superweapon: charges to READY at exactly recharge_secs, not before") {
    Superweapon sw;
    sw.type = SuperweaponType::Nuke;
    sw.recharge_secs = 10.0;
    const double dt = 1.0;
    for (int i = 0; i < 9; ++i) { sw.tick(dt); CHECK_FALSE(sw.is_ready()); }
    CHECK(sw.timer == doctest::Approx(9.0));
    sw.tick(dt);                     // reaches 10.0 -> READY
    CHECK(sw.is_ready());
    CHECK(sw.timer == doctest::Approx(10.0));
}

TEST_CASE("superweapon: a mid-charge tick is monotonic and never overshoots") {
    Superweapon sw;
    sw.type = SuperweaponType::Nuke;
    sw.recharge_secs = 5.0;
    double last = 0.0;
    for (int i = 0; i < 3; ++i) { sw.tick(2.0); CHECK(sw.timer >= last); last = sw.timer; }
    CHECK(sw.timer == doctest::Approx(5.0));   // clamped, not 6.0
    CHECK(sw.is_ready());
    sw.tick(100.0);                            // no-op once ready
    CHECK(sw.timer == doctest::Approx(5.0));   // never overshoots the cap
}

TEST_CASE("superweapon: on_fired resets timer to 0 and clears ready") {
    Superweapon sw;
    sw.type = SuperweaponType::Nuke;
    sw.recharge_secs = 4.0;
    sw.tick(4.0);
    REQUIRE(sw.is_ready());
    sw.on_fired();
    CHECK(sw.timer == doctest::Approx(0.0));
    CHECK_FALSE(sw.is_ready());
    // Re-charges from 0 after firing.
    sw.tick(4.0);
    CHECK(sw.is_ready());
}

TEST_CASE("superweapon: charge_fraction tracks 0..1 and an uninitialized SW is inert") {
    Superweapon sw;                            // type None, recharge 0
    CHECK(sw.charge_fraction() == doctest::Approx(0.0));
    sw.tick(100.0);                            // type None -> no-op
    CHECK_FALSE(sw.is_ready());
    sw.type = SuperweaponType::Nuke;
    sw.recharge_secs = 8.0;
    sw.tick(4.0);
    CHECK(sw.charge_fraction() == doctest::Approx(0.5));
    sw.tick(4.0);
    CHECK(sw.charge_fraction() == doctest::Approx(1.0));
}

TEST_CASE("superweapon: the SuperweaponType enum + [SuperWeaponTypes] id mapping") {
    // [RE-GATE: superweapon.md §4] the recovered roster (only Nuke active).
    CHECK(static_cast<int>(SuperweaponType::None) == 0);
    CHECK(static_cast<int>(SuperweaponType::Nuke) == 1);
    CHECK(superweapon_type_from_id(1) == SuperweaponType::Nuke);
    CHECK(superweapon_type_from_id(2) == SuperweaponType::IronCurtain);
    CHECK(superweapon_type_from_id(6) == SuperweaponType::ParaDrop);
    CHECK(superweapon_type_from_id(7) == SuperweaponType::ParaDrop);   // American too
    CHECK(superweapon_type_from_id(0) == SuperweaponType::None);       // unknown -> None
    CHECK(superweapon_type_from_id(99) == SuperweaponType::None);
    CHECK(std::string(superweapon_type_name(SuperweaponType::Nuke)) == "Nuke");
}
```

  (`#include <string>` is pulled in by `doctest.h`/the toolchain for the `std::string` compare; add it explicitly if the build complains.)

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).** The module does not exist yet:

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: compile FAILS (`superweapon.hpp` not found). Confirms the tests are wired before the implementation lands.

- [ ] **Step 3: Land the code.** Create `engine/superweapon.hpp` + `engine/superweapon.cpp` exactly as in Interfaces; register `engine/superweapon.cpp` in `cmake/ra2_sources.cmake` (`RA2_ENGINE_SOURCES`) and `tests/test_superweapon.cpp engine/superweapon.cpp` in `CMakeLists.txt`.

- [ ] **Step 4: Build + run the new tests FOREGROUND, blocking (timeout 600000 ms), expected PASS. Build BOTH targets:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="superweapon*"
```

Expected: both targets build; all `superweapon*` cases pass, 0 failed (host-only, no install needed).

- [ ] **Step 5: Full suite FOREGROUND, blocking (timeout 600000 ms), expected 0 failed** (the module is purely additive — a new pure TU; nothing existing is edited):

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
```

- [ ] **Step 6: Commit the additive module.**

```powershell
git -C "D:\ra2-android" add engine/superweapon.hpp engine/superweapon.cpp tests/test_superweapon.cpp cmake/ra2_sources.cmake CMakeLists.txt
git -C "D:\ra2-android" commit -m "feat(engine): pure Superweapon lifecycle module - SuperweaponType enum (recovered roster) + tick/is_ready/on_fired/charge_fraction + [SuperWeaponTypes] id mapping (additive) [RE: superweapon.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Expected: one commit; `superweapon.{hpp,cpp}` + `test_superweapon.cpp` created; both cmake files updated.

---

### Task 3: World integration — the sw_houses_ registry, sw_any_ firewall, step_superweapon pass

**Files:**
- Modify: `engine/world.hpp` (`#include "superweapon.hpp"` + `<array>`; `HouseSuperweapon` private struct; `sw_houses_`/`sw_any_` members; `grant_superweapon`/`step_superweapon` decls; `sw_ready`/`sw_charge_fraction`/`sw_state` accessors), `engine/world.cpp` (`grant_superweapon`, `step_superweapon`, the accessors, the `World::step` insertion)
- Test: `tests/test_world.cpp` (append)

**Interfaces:**
- Consumes: `Superweapon`/`SuperweaponType` (superweapon.hpp — Task 2).
- Produces — `engine/world.hpp`: add `#include "superweapon.hpp"` and `#include <array>` to the includes (next to the other engine/std includes).
- Produces — `engine/world.hpp`: the public accessors + mutators, immediately after the AI-3 `ai_trigger_state` accessor block (world.hpp:695, before the `step` doc comment at 697):

```cpp
    // --- CB-4 Superweapons [RE: superweapon.md] -----------------------------
    // A NEW per-house superweapon registry, INDEPENDENT of the AI registry so
    // it works for the PLAYER house 0 (which is never in ai_houses_). Default
    // INERT: no house has a SW until grant_superweapon, which no existing demo
    // calls -> sw_houses_ empty, sw_any_ false, step_superweapon a strict no-op
    // (spec §8). Unit and combat.cpp behavior are untouched.
    //
    // Grant `house` a superweapon: grow sw_houses_ to cover it, store the pure
    // lifecycle (type + recharge_secs already in SECONDS) and the resolved
    // detonation params ([NukePayload] Damage + [NUKE] CellSpread/PercentAtMax/
    // Verses), and open the sw_any_ firewall. verses is the 11-slot armor array
    // (int percents, matching Unit::w_verses / damage_vs).
    void grant_superweapon(int house, SuperweaponType type, double recharge_secs,
                           int dmg, double cell_spread, double pmax,
                           const std::array<int, 11>& verses);
    // Fire `house`'s superweapon at cell (cx, cy): no-op returning false unless
    // that house's SW is READY; on fire it detonates at the cell (§5) and resets
    // the timer (§4). Returns whether it fired (the wedge guard reads this).
    // [Task 4 lands the detonation body; Task 3 lands the registry it uses.]
    bool fire_superweapon(int house, int cx, int cy);
    // Read-only SW state for the sidebar (§6) and the gate (§7). false / 0 /
    // nullptr when `house` has no granted SW.
    bool sw_ready(int house) const;
    double sw_charge_fraction(int house) const;
    const Superweapon* sw_state(int house) const;
```

  (`fire_superweapon` is declared here in Task 3 so the accessor block is complete; its DEFINITION lands in Task 4 with the detonation. Task 3 does not test firing.)

- Produces — `engine/world.hpp`: the private record + members + step decl, immediately after the AI-3 `ai_fill_trigger_view` private decl (world.hpp:759):

```cpp
    // --- CB-4 Superweapons [RE: superweapon.md] ---
    // Per-house SW record: the pure lifecycle + the resolved detonation params.
    // Mirrors HouseAiRec's shape but INDEPENDENT of ai_houses_ (spec §2). All
    // defaults INERT (has_sw=false). eff_verses is the [NUKE] 11-slot armor
    // array as int percents (assigned straight into the transient detonator's
    // Unit::w_verses in fire_superweapon -- damage_vs takes std::vector<int>).
    struct HouseSuperweapon {
        bool has_sw = false;
        Superweapon sw;
        int eff_damage = 0;                 // [NukePayload] Damage=600
        double eff_cell_spread = 0.0;       // [NUKE] CellSpread=10
        double eff_percent_at_max = 1.0;    // [NUKE] PercentAtMax=.02
        std::array<int, 11> eff_verses{};   // [NUKE] Verses (armor percents)
    };
    std::vector<HouseSuperweapon> sw_houses_;  // index = house id (grown on demand)
    bool sw_any_ = false;                      // the step() firewall bool
    void step_superweapon(double dt);          // firewalled World::step pass (§3)
```

- Produces — `engine/world.cpp`: `grant_superweapon` + `step_superweapon` + the accessors, defined next to `enable_ai_house`/`ai_state` (after world.cpp:1015, `ai_credits`):

```cpp
void World::grant_superweapon(int house, SuperweaponType type, double recharge_secs,
                              int dmg, double cell_spread, double pmax,
                              const std::array<int, 11>& verses) {
    if (house < 0) return;
    if ((int)sw_houses_.size() <= house) sw_houses_.resize(house + 1);   // enable_ai_house precedent
    HouseSuperweapon& rec = sw_houses_[house];
    rec.has_sw = true;
    rec.sw = Superweapon{};
    rec.sw.type = type;
    rec.sw.recharge_secs = recharge_secs;
    rec.eff_damage = dmg;
    rec.eff_cell_spread = cell_spread;
    rec.eff_percent_at_max = pmax;
    rec.eff_verses = verses;
    sw_any_ = true;                                                       // open the firewall
}

void World::step_superweapon(double dt) {
    if (!sw_any_) return;                        // firewall (spec §8) -- no-op everywhere else
    for (HouseSuperweapon& rec : sw_houses_)     // house-id order; pure arithmetic, no RNG (§3.5)
        if (rec.has_sw) rec.sw.tick(dt);
}

bool World::sw_ready(int house) const {
    return house >= 0 && house < (int)sw_houses_.size() &&
           sw_houses_[house].has_sw && sw_houses_[house].sw.is_ready();
}

double World::sw_charge_fraction(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || !sw_houses_[house].has_sw) return 0.0;
    return sw_houses_[house].sw.charge_fraction();
}

const Superweapon* World::sw_state(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || !sw_houses_[house].has_sw) return nullptr;
    return &sw_houses_[house].sw;
}
```

- Produces — `engine/world.cpp`: the `step_superweapon` insertion into `World::step`, immediately AFTER `step_flight(dt);` (world.cpp:1992) and BEFORE `step_turrets` (world.cpp:1997):

```cpp
    // CB-4 Superweapons [RE: superweapon.md §3]: advance each granted house's
    // recharge timer. A pure INSERTION after step_flight; `if (!sw_any_) return;`
    // inside is the only cost every existing demo pays (no house is granted a SW
    // outside the new gate). The detonation is fire-triggered, not step-
    // triggered, so this pass's ORDER is behaviorally irrelevant (spec §3.3).
    step_superweapon(dt);
```

**Steps:**

- [ ] **Step 1: Write the failing World tests.** Append to `tests/test_world.cpp` (host-only, hand-built — no install). These drive the registry lifecycle (firing is tested in Task 4):

```cpp
TEST_CASE("World SW: grant_superweapon materializes the record and opens sw_any_") {
    ra2engine::World world;
    CHECK_FALSE(world.sw_ready(0));
    CHECK(world.sw_charge_fraction(0) == doctest::Approx(0.0));
    CHECK(world.sw_state(0) == nullptr);          // no SW granted yet
    std::array<int, 11> verses;
    verses.fill(100);
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, /*recharge_secs=*/10.0,
                            /*dmg=*/600, /*cell_spread=*/10.0, /*pmax=*/0.02, verses);
    REQUIRE(world.sw_state(0) != nullptr);
    CHECK(world.sw_state(0)->type == ra2engine::SuperweaponType::Nuke);
    CHECK_FALSE(world.sw_ready(0));               // not charged yet
}

TEST_CASE("World SW: step_superweapon charges the granted house to READY at recharge_secs") {
    ra2engine::World world;
    std::array<int, 11> verses; verses.fill(100);
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, 10.0, 600, 10.0, 0.02, verses);
    const double dt = 1.0 / 15.0;
    int frames = 0;
    for (; frames < 100000 && !world.sw_ready(0); ++frames) world.step(dt);
    CHECK(world.sw_ready(0));
    CHECK(world.sw_charge_fraction(0) == doctest::Approx(1.0));
    // ~10s / (1/15 s) = 150 frames; allow the exact clamp frame.
    CHECK(frames >= 150);
    CHECK(frames <= 152);
}

TEST_CASE("World SW: an ungranted house stays inert and a no-SW World is a no-op") {
    // step_superweapon must not touch an ungranted house, and a World that never
    // grants a SW steps exactly as before (sw_any_ false -> strict no-op, §8).
    ra2engine::World world;
    std::array<int, 11> verses; verses.fill(100);
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, 5.0, 600, 10.0, 0.02, verses);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 30; ++f) world.step(dt);
    CHECK(world.sw_state(1) == nullptr);          // house 1 was never granted
    CHECK_FALSE(world.sw_ready(1));
    ra2engine::World bare;                         // never grants a SW
    for (int f = 0; f < 30; ++f) bare.step(dt);    // step_superweapon early-returns
    CHECK(bare.sw_state(0) == nullptr);
}
```

  (`#include <array>` — add to `tests/test_world.cpp` if not already present.)

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: compile FAILS (`grant_superweapon`/`sw_ready`/`sw_state`/`SuperweaponType` not members / not found).

- [ ] **Step 3: Implement.** Apply the `world.hpp` includes + accessor block + private record/members + `step_superweapon` decl, and the `world.cpp` definitions + the `World::step` insertion. (Do NOT define `fire_superweapon` yet — Task 4. It is declared but its definition lands next task; do not build `ra2engine` until Task 4 if the linker would complain about the missing definition. `ra2assets_tests` does not reference `fire_superweapon` in Task 3, so it links; `ra2engine`/`main.cpp` also does not call it until Task 5, so `ra2engine` also links — an unreferenced declared-but-undefined member is fine. If any TU references it, defer to Task 4.)

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="World SW:*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: the new `World SW:*` cases pass; full suite 0 failed (the registry is additive — a new member + a firewalled step pass; nothing existing is edited behaviorally).

- [ ] **Step 5: 11-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; PowerShell, not Git Bash; do NOT pipe through Select-Object).** This is the first additive commit that touches `World::step` (the `step_superweapon` insertion); the 11-gate run isolates any regression HERE (spec §8):

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
```

Expected, all eleven pins EXACT: `deterministic=yes logs=78`; `deterministic=yes logs=144`; `hash=5273b5e71ca009e1`; `econ_hash=7fed96cb98b68017`; `prod_hash=f0f4e1207ecd713c`; `ai_base_hash=cd1f7c6a29933f0a`; `ai_team_hash=312e92ed862a5e74`; `ai_trigger_hash=8334a6ca1531cabc`; `transport_hash=fe500f946e913645`; `aircraft_hash=a69642564cf64773`; `naval_hash=6aee2c82bc024e86`. ANY drift = defect in THIS task; fix before proceeding.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "feat(engine): superweapon World registry - sw_houses_/sw_any_ (AI-independent, player-house 0), grant_superweapon, firewalled step_superweapon pass, read accessors; INERT defaults (11 gates re-verified) [RE: superweapon.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: the fired EFFECT — fire_superweapon detonation via apply_spread_damage + main.cpp resolve

**Files:**
- Modify: `engine/combat.hpp` (declare `apply_spread_damage`), `engine/combat.cpp` (move `apply_spread_damage` out of the anonymous namespace — linkage-only), `engine/world.cpp` (`fire_superweapon` definition + the building-splash [RE-GATE]), `engine/main.cpp` (`SuperweaponParams` + `resolve_superweapon`)
- Test: `tests/test_world.cpp` (append)

**Interfaces:**

- Produces — `engine/combat.hpp`: the `apply_spread_damage` declaration (in `ra2engine`, next to `damage_vs` at combat.hpp:40). `Occupancy` is already visible via the `world.hpp` include (combat.hpp:14):

```cpp
// CB-4 Superweapons [RE: superweapon.md §2/§5]: the CB-2 area-damage applicator,
// PROMOTED from combat.cpp's anonymous namespace to ra2engine so World::
// fire_superweapon can REUSE it for the nuke detonation (a transient virtual
// detonator Unit passed as BOTH attacker and center). LINKAGE-ONLY change: the
// function body + its existing caller (try_fire) are byte-for-byte unchanged, so
// every existing combat path computes identical results and all 11 gate outputs
// stay bit-identical (re-verified below). This is the one deviation from the
// spec's "combat.cpp literally untouched" wording -- necessitated because the
// applicator has internal linkage and is otherwise uncallable from world.cpp;
// reimplementing the falloff loop (a "new damage system") is what §5/§8 forbid.
void apply_spread_damage(std::vector<Unit>& units, Occupancy& occ,
                         const Unit& attacker, const Unit& center);
```

- Produces — `engine/combat.cpp`: MOVE the `apply_spread_damage` definition (its doc comment `combat.cpp:76–83` + the function body `combat.cpp:84–110`) OUT of the anonymous namespace (which spans `combat.cpp:12–152`) to just AFTER the anon namespace closes (`}  // namespace`, combat.cpp:152), immediately before `int armor_index(...)` (combat.cpp:154), in the `ra2engine` namespace. **The function body is copied verbatim — not a single line of logic changes.** `try_fire` (still anonymous, combat.cpp:112–150) calls `apply_spread_damage` at combat.cpp:122; the declaration now in `combat.hpp` (included at combat.cpp:1) makes that call resolve to `ra2engine::apply_spread_damage` — compiles and runs identically. Net diff: one function relocated from inside `namespace {}` to the enclosing `ra2engine` namespace + one declaration added to `combat.hpp`. No behavior change.

- Produces — `engine/world.cpp`: `fire_superweapon` (defined next to the Task-3 `grant_superweapon`). Needs `combat.hpp` (already included at world.cpp:9):

```cpp
bool World::fire_superweapon(int house, int cx, int cy) {
    if (house < 0 || house >= (int)sw_houses_.size()) return false;
    HouseSuperweapon& rec = sw_houses_[house];
    if (!rec.has_sw || !rec.sw.is_ready()) return false;   // refused unless READY (§4)

    // Transient virtual detonator [spec §5.1]: a value-initialized STACK Unit
    // (Unit struct UNCHANGED -- no persistent field). Its house + fx/fy (target
    // cell, in CELL-space: apply_spread_damage's dist_leptons scales by
    // kLeptonsPerCell internally, combat.cpp:50/95, and an idle unit's fx/fy IS
    // its integer cell) + the four warhead fields are EXACTLY and ONLY the fields
    // apply_spread_damage reads from its attacker/center args.
    Unit virt;
    virt.house = house;
    virt.fx = static_cast<double>(cx);
    virt.fy = static_cast<double>(cy);
    virt.w_damage = rec.eff_damage;                 // [NukePayload] Damage=600
    virt.w_cell_spread = rec.eff_cell_spread;        // [NUKE] CellSpread=10
    virt.w_percent_at_max = rec.eff_percent_at_max;  // [NUKE] PercentAtMax=.02
    virt.w_verses.assign(rec.eff_verses.begin(), rec.eff_verses.end());  // [NUKE] Verses

    // Detonate: REUSE the EXACT CB-2 applicator (combat.cpp) -- virt is both the
    // attacker (house + warhead) and the center (fx/fy). Every alive, non-carried,
    // non-aircraft ENEMY unit within round(CellSpread)*kLeptonsPerCell of the cell
    // takes damage_vs(round(Damage*scale), NUKE-Verses, armor) with scale falling
    // LINEARLY 1.0 -> PercentAtMax across the radius; enemies-only spares the
    // firing house. NO new damage math (spec §5/§8).
    apply_spread_damage(units_, occ_, virt, virt);

    // [RE-GATE: superweapon.md §5.3] BUILDING SPLASH -- design-call routed to the
    // RE-GATE. FALLBACK (pre-hunt): UNITS ONLY (apply_spread_damage's scope, the
    // CB-2 aerial bomb). IF the Task-1 hunt CONFIRMS the nuke levels buildings in
    // the radius, the controller amendment adds an SW-ONLY building loop HERE
    // mirroring the unit falloff (round(CellSpread) radius, linear 1.0->PercentAtMax
    // scale, damage_vs truncation over buildings_ + kill_building on hp<=0,
    // combat.md §3 / building.cpp). It is an ADDED clause reached only by this
    // SW-only call, so no existing demo is affected. The gate targets UNITS
    // regardless, so it is building-splash-independent (§5.3). Do NOT add the
    // building loop until the RE-GATE confirms it.

    rec.sw.on_fired();   // reset timer=0, clear ready (§4)
    return true;
}
```

- Produces — `engine/main.cpp`: `SuperweaponParams` + `resolve_superweapon` (near the other Rules resolution, next to `resolve_combat_unit`). Resolves the recovered warhead values from Rules into the `grant_superweapon` params (like every existing stat resolution):

```cpp
// CB-4 Superweapons [RE-GATE: superweapon.md §1/§2]: resolve the Nuclear Missile
// grant params from the real rulesmd. RechargeTime is MINUTES (RA2-canonical) ->
// recharge_secs = RechargeTime*60 [RE-GATE: superweapon.md §1 -- the hunt CONFIRMS
// the unit/factor; FALLBACK *60]. The DAMAGING leg is [NukePayload]/[NUKE] (the
// down-strike), NOT the SW's [NukeCarrier] up-launch [RE-GATE: superweapon.md §2 --
// FALLBACK [NukePayload] Damage=600 + Warhead=NUKE]. Verses is the [NUKE] 11-slot
// armor array (int percents). All values cite docs/re/superweapon.md.
struct SuperweaponParams {
    double recharge_secs = 0.0;
    int damage = 0;
    double cell_spread = 0.0;
    double percent_at_max = 1.0;
    std::array<int, 11> verses{};
    bool ok = false;
};

SuperweaponParams resolve_superweapon(const ra2assets::Rules& rules) {
    SuperweaponParams p;
    const double recharge_min = rules.section_double("NukeSpecial", "RechargeTime", 10.0);
    p.recharge_secs = recharge_min * 60.0;                        // minutes -> seconds
    p.damage = (int)rules.section_double("NukePayload", "Damage", 600.0);
    p.cell_spread = rules.section_double("NUKE", "CellSpread", 10.0);
    p.percent_at_max = rules.section_double("NUKE", "PercentAtMax", 0.02);
    const std::vector<int> v = rules.verses("NUKE");             // [NUKE] Verses (combat.md §1)
    for (std::size_t i = 0; i < p.verses.size(); ++i)
        p.verses[i] = i < v.size() ? v[i] : 100;                 // absent slot -> 100% (no mod)
    p.ok = true;
    std::cerr << "[sw] resolve nuke recharge_secs=" << p.recharge_secs
              << " damage=" << p.damage << " cell_spread=" << p.cell_spread
              << " percent_at_max=" << p.percent_at_max << "\n";
    return p;
}
```

  (`<array>` is included by `main.cpp` via `world.hpp`; add `#include <array>` explicitly if needed. `Rules::section_double` + `Rules::verses` are the existing accessors used at main.cpp:616/624.)

**Steps:**

- [ ] **Step 1: The linkage-only combat export.** Add the `apply_spread_damage` declaration to `engine/combat.hpp`; move its definition out of `combat.cpp`'s anonymous namespace into `ra2engine` (verbatim body). Build BOTH targets and re-run the combat suite to prove the relocation changed nothing:

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="*splash*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="*combat*"
```

Expected: both build; all combat/splash cases pass unchanged (the applicator body is identical; only its linkage moved).

- [ ] **Step 2: Write the failing fire/detonation tests.** Append to `tests/test_world.cpp` (host-only, hand-built — no install). Build a small World with a granted SW + hand-placed units and drive the detonation:

```cpp
namespace {
// A minimal target combat-stat block (hand-built -- no rulesmd/install needed).
ra2engine::UnitCombat sw_target_stats() {
    ra2engine::UnitCombat c;
    c.image = "E1";
    c.strength = 100;
    c.armor = "none";
    return c;
}
// The [NUKE] verses (all 100% except the recovered slots) as a full array.
std::array<int, 11> nuke_verses() {
    std::array<int, 11> v; v.fill(100);
    return v;   // uniform 100% keeps the test's armor math simple (damage_vs identity)
}
}  // namespace

TEST_CASE("World SW: fire_superweapon is refused before READY, succeeds after, resets") {
    ra2engine::World world;
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, 5.0, 600, 10.0, 0.02,
                            nuke_verses());
    world.add_combat_unit(20, 20, /*house=*/1, sw_target_stats());   // an enemy at the cell
    CHECK_FALSE(world.fire_superweapon(0, 20, 20));   // not charged -> refused
    CHECK(world.units()[0].hp == world.units()[0].max_hp);   // undamaged
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 200 && !world.sw_ready(0); ++f) world.step(dt);
    REQUIRE(world.sw_ready(0));
    CHECK(world.fire_superweapon(0, 20, 20));          // charged -> fires
    CHECK_FALSE(world.sw_ready(0));                    // timer reset, ready cleared
    CHECK(world.sw_charge_fraction(0) == doctest::Approx(0.0));
}

TEST_CASE("World SW: the detonation kills enemies in radius, spares the firing house, "
          "leaves out-of-radius units untouched") {
    ra2engine::World world;
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, 1.0, 600, 10.0, 0.02,
                            nuke_verses());
    ra2engine::Unit& near_enemy = world.add_combat_unit(20, 20, /*house=*/1, sw_target_stats());
    ra2engine::Unit& own       = world.add_combat_unit(21, 20, /*house=*/0, sw_target_stats());
    ra2engine::Unit& far_enemy = world.add_combat_unit(20, 90, /*house=*/1, sw_target_stats());
    (void)near_enemy; (void)own; (void)far_enemy;
    const int own_hp0 = world.units()[1].hp, far_hp0 = world.units()[2].hp;
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 30 && !world.sw_ready(0); ++f) world.step(dt);
    REQUIRE(world.sw_ready(0));
    REQUIRE(world.fire_superweapon(0, 20, 20));         // detonate AT the enemy cluster cell
    CHECK(world.units()[0].hp < world.units()[0].max_hp);  // near enemy damaged (600 dmg >> 100 hp)
    CHECK_FALSE(world.units()[0].alive == true ? world.units()[0].hp > 0 : false); // near enemy dead
    CHECK(world.units()[1].hp == own_hp0);              // firing house SPARED (enemies-only)
    CHECK(world.units()[2].hp == far_hp0);              // 70 cells away -> outside CellSpread=10
}

TEST_CASE("World SW: the detonation falloff -- edge damage < center damage") {
    // Center takes full; a unit near the radius edge takes ~PercentAtMax*Damage.
    // With CellSpread=10 the radius is round(10)=10 cells; a unit 9 cells out is
    // near the edge. Use high hp so neither dies and we can compare damage taken.
    ra2engine::World world;
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, 1.0, 600, 10.0, 0.02,
                            nuke_verses());
    ra2engine::UnitCombat tough = sw_target_stats();
    tough.strength = 100000;                            // survive to measure damage
    ra2engine::Unit& center = world.add_combat_unit(20, 20, /*house=*/1, tough);
    ra2engine::Unit& edge   = world.add_combat_unit(29, 20, /*house=*/1, tough);  // 9 cells out
    (void)center; (void)edge;
    const int c_hp0 = world.units()[0].hp, e_hp0 = world.units()[1].hp;
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 30 && !world.sw_ready(0); ++f) world.step(dt);
    REQUIRE(world.fire_superweapon(0, 20, 20));
    const int c_dmg = c_hp0 - world.units()[0].hp;
    const int e_dmg = e_hp0 - world.units()[1].hp;
    CHECK(c_dmg > 0);
    CHECK(e_dmg > 0);
    CHECK(e_dmg < c_dmg);                               // linear falloff toward the edge
}
```

  (The exact "near enemy dead" assertion above is written defensively; if `add_combat_unit`'s death path tombstones differently, assert `world.units()[0].hp <= 0` — 600 dmg vs 100 hp is unambiguously lethal. Keep the intent: near enemy lethal, own spared, far untouched, edge < center.)

- [ ] **Step 3: Implement `fire_superweapon` + `resolve_superweapon`.** Apply the `world.cpp` `fire_superweapon` definition (with the building-splash [RE-GATE] comment block — do NOT add the building loop unless the RE-GATE confirmed it) and the `main.cpp` `SuperweaponParams`/`resolve_superweapon`.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="World SW:*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: the new `World SW:*` fire/detonation/falloff cases pass; full suite 0 failed.

- [ ] **Step 5: 11-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH).** Task 4 relocated `apply_spread_damage` (linkage-only) — the CB-2 `--demo-aircraft` splash path and the CB-3 `--demo-naval` FlakTWH splash both flow through it, so proving them bit-identical is the whole point of this step:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
```

Expected: all eleven pins EXACT (values in Task 3 Step 5). ANY drift = defect in THIS task (most likely the `apply_spread_damage` relocation changed a byte — re-diff the moved body). Fix before proceeding.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/combat.hpp engine/combat.cpp engine/world.cpp engine/main.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "feat(engine): superweapon detonation - fire_superweapon builds a transient virtual detonator Unit + REUSES apply_spread_damage (promoted to external linkage, body unchanged); resolve_superweapon reads [NukePayload]/[NUKE] (11 gates re-verified) [RE: superweapon.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: sidebar SW UI + input — NukeIcon cameo + gclock2 recharge clock + N-keybind/click arm → map-click fire

**Files:**
- Modify: `engine/input.hpp` (`InputActions` SW-fire fields; `arm_superweapon`/`sw_targeting`/`cancel_superweapon` + the `sw_targeting_` member), `engine/input.cpp` (the SW-targeting left-click → report a fire cell, mirroring placement mode), `engine/main.cpp` (`NukeIcon` cameo load, the SW-slot draw, the `N`-keybind/cameo-click arm, the fire routing)
- Test: (interactive-only; not gate-tested — the sim is proven by Task 4 + the Task 6 gate)

**Interfaces:**

- Produces — `engine/input.hpp`: `InputActions` gains the SW-fire report (mirroring `place_confirmed`/`place_cx`/`place_cy` at input.hpp:35–37):

```cpp
    // CB-4 Superweapons [spec §4]: a left-click committed a SW target cell while
    // SW-targeting was armed (mirrors place_confirmed/place_cx/cy). main.cpp
    // routes this to World::fire_superweapon. Default inert (no existing flow
    // arms SW-targeting, so no existing demo/frame reports it).
    bool sw_fire_confirmed = false;
    int sw_cx = 0, sw_cy = 0;
```

- Produces — `engine/input.hpp`: the SW-targeting mode API (mirroring `placement_active`/`cancel_placement` at input.hpp:84–85) in the public block, plus the private flag:

```cpp
    // CB-4: arm/disarm SW-targeting -- a lightweight parallel of placement mode.
    // While armed, a left-click on the map reports sw_fire_confirmed + sw_cx/cy
    // (via the SAME screen->cell pick placement uses); a right-click/Esc cancels.
    void arm_superweapon() { sw_targeting_ = true; }
    void cancel_superweapon() { sw_targeting_ = false; }
    bool sw_targeting() const { return sw_targeting_; }
    // ... in the private block, next to place_type_ (input.hpp:191):
    bool sw_targeting_ = false;
```

- Produces — `engine/input.cpp`: the SW-targeting click handler, added as the FIRST branch of the event loop (before the `if (placement_active())` block at input.cpp:269) so an armed SW consumes the click before normal select/move:

```cpp
        // CB-4 Superweapons [spec §4]: while SW-targeting is armed, a left-click
        // commits the target cell (the SAME level-aware screen->cell pick the
        // placement path uses, input.cpp:114-120) and disarms; a right-click or
        // Esc cancels. UI state only -- the sim is untouched until main.cpp calls
        // World::fire_superweapon. No existing flow arms this, so this branch is
        // never entered in any existing demo.
        if (sw_targeting_) {
            if (ev.type == InputEvent::Mouse) {
                if (ev.button == InputEvent::Right) {
                    sw_targeting_ = false;              // cancel
                } else if (ev.button == InputEvent::Left && l_down_ && !dragging_) {
                    int cx = 0, cy = 0;
                    // Reuse the placement pick (level-aware, half-tile offset).
                    screen_to_cell(ev.x - cam_.x - kTileW / 2, ev.y - cam_.y - kTileH / 2,
                                   kTileW, kTileH, cx, cy);
                    actions_.sw_fire_confirmed = true;
                    actions_.sw_cx = cx;
                    actions_.sw_cy = cy;
                    sw_targeting_ = false;             // one-shot
                }
            }
            continue;   // an armed SW swallows the event -- no select/move this click
        }
```

  (The exact `screen_to_cell` call MUST mirror the placement pick at input.cpp:114–120 — same offsets/level handling. If placement resolves the cell via a private helper rather than a bare `screen_to_cell`, call that same helper here. `actions_` is the per-batch `InputActions` accumulator; `l_down_`/`dragging_`/`cam_`/`kTileW`/`kTileH` are the existing input state used by the placement branch. `take_actions()` already resets `actions_` each batch, so `sw_fire_confirmed` clears automatically.)

- Produces — `engine/main.cpp`: load the `NukeIcon` cameo art-guarded (in the sidebar-assets load path next to the `load_sb`/`cameo.mix` chain, main.cpp:1456–1470). Add to the `BuildingAssets`/sidebar-assets struct a guarded field + load it:

```cpp
    // CB-4 Superweapons [RE: superweapon.md; spec §6]: the SW cameo, loaded
    // through the EXISTING cameo.mix chain (main.cpp:1420-1426). Art-guarded --
    // if NukeIcon is absent the SW slot draws a placeholder shade and the sim is
    // unaffected [RE-GATE: superweapon.md art-verify -- present on this install
    // unless the art regresses]. sb_nukeicon.count()==0 signals "no art".
    ba.sw_has_cameo = false;
    try {
        MixFile language = /* the same language.mix handle the block above opened */;
        MixFile cameo_mix = language.open_nested("cameo.mix");
        if (const MixEntry* ce = cameo_mix.find("nukeicon.shp")) {
            ba.sb_nukeicon = ShpFile::load(cameo_mix.read(*ce));
            ba.sw_has_cameo = ba.sb_nukeicon.count() > 0;
        }
    } catch (const std::exception&) { ba.sw_has_cameo = false; }
```

  (Add `ra2assets::ShpFile sb_nukeicon;` + `bool sw_has_cameo = false;` to the sidebar-assets struct next to `sb_gclock` at main.cpp:329. Reuse the exact `language`/`cameo.mix` handle the surrounding block already opens — do not re-open the chain if one is in scope.)

- Produces — `engine/main.cpp`: the SW-slot draw (a helper drawn from the sidebar render, next to the cameo/gclock2 block at main.cpp:2515–2528). Drawn ONLY when house 0 has a granted SW (`world.sw_state(0) != nullptr`):

```cpp
// CB-4 Superweapons [spec §6]: draw the SW cameo + recharge indicator in a
// sidebar slot, reusing the EXISTING gclock2.shp 55-frame chrome + the flashing-
// green READY tint. Drawn ONLY when house 0 has a granted SW, so no existing
// demo (which grants none) changes its sidebar. `slot` is the target rect.
void draw_sw_slot(SDL_Renderer* ren, ra2engine::TextureCache& tc, const BuildingAssets& ba,
                  const ra2engine::World& world, const SDL_Rect& slot, double t_now) {
    const ra2engine::Superweapon* sw = world.sw_state(0);
    if (!sw) return;                                    // no SW granted -> nothing drawn
    // Cameo (art-guarded): NukeIcon, else a placeholder shade.
    if (ba.sw_has_cameo)
        blit_shp(ren, tc, "swcam:nukeicon", ba.sb_nukeicon, 0, ba.cameo_pal, slot.x, slot.y);
    else
        shade_rect(ren, slot, 140);                     // placeholder tile
    if (world.sw_ready(0)) {
        // READY: 2 Hz flashing-green tint (the existing production-ready state).
        if (static_cast<int>(t_now * 2) % 2 == 0) shade_rect(ren, slot, 90, 0, 200, 0);
    } else {
        // Charging: gclock2 frame over a shade (the build-clock chrome, §6).
        shade_rect(ren, slot, 90);
        int gframe = (int)std::lround(world.sw_charge_fraction(0) * 54.0);
        if (gframe < 0) gframe = 0;
        if (gframe >= ba.sb_gclock.count()) gframe = ba.sb_gclock.count() - 1;
        blit_shp(ren, tc, "sw:gclk" + std::to_string(gframe), ba.sb_gclock, gframe, ba.sb_pal,
                 slot.x, slot.y);
    }
}
```

  (Call `draw_sw_slot(ren, tc, ba, world, sw_slot_rect, t_now)` in the sidebar render pass, choosing an unused sidebar slot rect for `sw_slot_rect` — e.g. one cameo-cell below the production tabs. `blit_shp`/`shade_rect`/`ba.cameo_pal`/`ba.sb_pal`/`ba.sb_gclock` are the existing sidebar-draw helpers used at main.cpp:2504–2524.)

- Produces — `engine/main.cpp`: the arm + fire wiring in the interactive loop. The `N`-key arm (edge-detected, mirroring `unload_key` at main.cpp:2908–2910) — place next to it:

```cpp
        // CB-4 Superweapons [spec §4]: N arms SW-targeting when house 0's SW is
        // READY (a click on the sidebar SW cameo can also call arm_superweapon()).
        const bool sw_key = ks[SDL_SCANCODE_N] != 0;
        if (sw_key && !prev_sw_key && world.sw_ready(0)) input.arm_superweapon();
        prev_sw_key = sw_key;
```

  (Add `bool prev_sw_key = false;` next to `prev_unload_key`.) The fire routing — after `const ra2engine::InputActions act = input.take_actions();` (main.cpp:2990), next to the `act.place_confirmed` block:

```cpp
        // CB-4 Superweapons [spec §4]: a committed SW target cell -> fire. no-op
        // unless the SW is READY (World::fire_superweapon re-checks). Refused
        // fires are silent (the arm was already consumed).
        if (act.sw_fire_confirmed) world.fire_superweapon(0, act.sw_cx, act.sw_cy);
```

**Steps:**

- [ ] **Step 1: Land the input + UI code.** Apply the `InputActions` fields + the `Input` SW-targeting API/flag (input.hpp), the SW-targeting click branch (input.cpp — mirroring the placement pick exactly), the `NukeIcon` load + `draw_sw_slot` + the `N`-key arm + the fire routing (main.cpp). Ensure `main.cpp` calls `draw_sw_slot` from the sidebar render pass with a real slot rect.

- [ ] **Step 2: Build FOREGROUND, blocking (timeout 600000 ms), expected OK:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
```

Expected: both targets build.

- [ ] **Step 3: Full suite FOREGROUND, blocking (timeout 600000 ms), expected 0 failed:**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: 0 failed. The SW UI/input is not unit-tested directly (interactive); the `test_input` suite still passes (the SW branch is entered only when `sw_targeting_` is armed, which no existing test does).

- [ ] **Step 4: 11-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH).** The sidebar draw gained a `draw_sw_slot` call guarded on `world.sw_state(0) != nullptr` (null in every existing demo) and the input loop gained a firewalled arm/fire path (never armed in any existing demo) — re-verify the rendered gates draw + hash identically:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
```

Expected: all eleven pins EXACT (values in Task 3 Step 5). ANY drift = defect in THIS task.

- [ ] **Step 5: Interactive smoke (rendered, FOREGROUND, blocking; generous timeout).** OPTIONAL — the definitive SW proof is the Task 6 gate. If a rendered interactive run is available with a granted SW, confirm the SW cameo + recharge clock draw and that `N` → map-click fires; otherwise defer to Task 6. Do NOT wire an interactive SW grant into an existing demo (byte-identity).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/input.hpp engine/input.cpp engine/main.cpp
git -C "D:\ra2-android" commit -m "feat(engine): superweapon sidebar UI + input - NukeIcon cameo + gclock2 recharge clock (art-guarded), N-keybind/cameo-click arm -> map-click fire (guarded; 11 gates re-verified) [RE: superweapon.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: --demo-superweapon — the 12th permanent byte-identity gate

**Files:**
- Modify: `engine/main.cpp` (`superweapon_fold` + `DemoSuperweaponResult` + `simulate_superweapon` + `run_demo_superweapon` + the `--demo-superweapon` dispatch)

**Interfaces:**
- Consumes: `fnv_mix`/`world_hash` (main.cpp — UNTOUCHED; `superweapon_fold` is a NEW accumulator), `build_world`/`capture_png`/`render_world`/`cell_to_screen`/`Camera`/`TextureCache`/`load_assets`/`load_unit_assets`/`env_install_dir`/`pick_start_cell` (main.cpp — the `simulate_aircraft`/`run_demo_aircraft` template), `resolve_superweapon`/`SuperweaponParams` (Task 4), `World::{add_combat_unit,grant_superweapon,fire_superweapon,sw_ready,sw_state,sw_charge_fraction,units,step}` (world), `ra2engine::Superweapon` (superweapon.hpp).

- Produces — `engine/main.cpp`: `superweapon_fold` (folds the exact state CB-4 adds — every unit's cell/alive/hp + the firing house's SW quantized-timer/ready/type + the target cell):

```cpp
// FNV fold of the superweapon-phase state [spec §7]: every unit's (cell_x,
// cell_y, alive, hp) PLUS the firing house's SW (quantized timer, ready, type)
// and the target cell (cx, cy). The timer is folded as an integer frame count
// (round(timer*kLogicFps)) to stay bit-stable across the dt accumulation. Folded
// at the demo's four fixed checkpoints (post-charge, post-fire, post-detonate,
// end). A NEW accumulator; world_hash and every prior *_hash UNTOUCHED.
uint64_t superweapon_fold(uint64_t h, const ra2engine::World& world, int house,
                          int cx, int cy) {
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
    }
    if (const ra2engine::Superweapon* sw = world.sw_state(house)) {
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(sw->timer * 15.0)));  // quantized frames
        h = fnv_mix(h, static_cast<uint64_t>(sw->ready ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(sw->type)));
    }
    h = fnv_mix(h, static_cast<uint64_t>(cx));
    h = fnv_mix(h, static_cast<uint64_t>(cy));
    return h;
}

struct DemoSuperweaponResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };
```

  (`std::llround` needs `<cmath>` — already included by main.cpp. `15.0` is `kLogicFps`; use the named constant if one is in scope.)

- Produces — `engine/main.cpp`: `simulate_superweapon` (the `simulate_aircraft` template; grants house 0 the Nuke, spawns an enemy cluster in-radius + one own unit in-radius + one far enemy out-of-radius; CHARGE → FIRE → DETONATE → RECHARGE; folds at four checkpoints; witnesses + wedge guards). Art-independent (the SW cameo is sidebar-only; the gate hashes pure sim). Complete:

```cpp
DemoSuperweaponResult simulate_superweapon(Assets& a, UnitAssets& ua,
                                           const std::vector<const VehicleAssets*>& vlist,
                                           bool render, SDL_Renderer* ren,
                                           ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                           int W, int H, const std::string& start_png,
                                           const std::string& end_png) {
    DemoSuperweaponResult r;
    ra2engine::World world = build_world(a, ua, 0);

    // A hand-built target stat block (the sim is exemplar-art-independent -- the
    // gate hashes pure state, not art; targets render if E1 anim is available).
    auto target_stats = []() {
        ra2engine::UnitCombat c; c.image = "E1"; c.strength = 100; c.armor = "none"; return c;
    };
    if (ua.anims.count("E1")) world.set_anim("E1", ua.anims.at("E1"));

    // Target cell + an enemy cluster within the nuke radius (CellSpread=10), one
    // OWN (house 0) unit in-radius (to prove enemies-only sparing), and one FAR
    // enemy well outside the radius (to prove out-of-radius is untouched).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int cx = pc ? pc->x + 6 : 30;
    const int cy = pc ? pc->y + 6 : 30;
    std::vector<int> in_radius;                       // enemy indices expected to die
    for (int d = 0; d < 6; ++d) {
        const int ex = cx + (d % 3) - 1;              // a tight 3-wide cluster on the cell
        const int ey = cy + (d / 3) - 1;
        ra2engine::Unit& e = world.add_combat_unit(ex, ey, /*house=*/1, target_stats());
        (void)e;
        in_radius.push_back((int)world.units().size() - 1);
    }
    world.add_combat_unit(cx + 2, cy, /*house=*/0, target_stats());       // OWN unit in-radius
    const int own_idx = (int)world.units().size() - 1;
    world.add_combat_unit(cx, cy + 40, /*house=*/1, target_stats());      // FAR enemy (out of radius)
    const int far_idx = (int)world.units().size() - 1;
    const int own_hp0 = world.units()[own_idx].hp;
    const int far_hp0 = world.units()[far_idx].hp;

    // Grant house 0 the Nuclear Missile with the recovered params (a small
    // recharge for the gate is NOT used -- the framework charges the REAL value;
    // resolve_superweapon supplied it in run_demo_superweapon and passes it in
    // via the demo's grant below). Here we grant with the resolved secs.
    SuperweaponParams p = resolve_superweapon(a.rules);
    std::array<int, 11> verses = p.verses;
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, p.recharge_secs,
                            p.damage, p.cell_spread, p.percent_at_max, verses);
    {
        char b[192];
        std::snprintf(b, sizeof b,
                      "[sw] granted recharge_secs=%.1f damage=%d cell_spread=%.1f pmax=%.3f "
                      "target=(%d,%d) enemies=%d", p.recharge_secs, p.damage, p.cell_spread,
                      p.percent_at_max, cx, cy, (int)in_radius.size());
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    uint64_t h = 0xcbf29ce484222325ull;               // FNV-1a offset basis
    auto step_one = [&]() {
        world.step(dt);
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };
    if (render) { step_one(); capture_png(ren, start_png, W, H); }

    // 1) CHARGE: step until READY (the real recharge; RechargeTime*60*15 frames
    //    is a headless-fast countdown -- the --demo-naval 9000-frame precedent).
    bool reached_ready = false;
    const long charge_cap = (long)std::llround(p.recharge_secs * 15.0) + 100;
    while (frame < charge_cap && !world.sw_ready(0)) step_one();
    reached_ready = world.sw_ready(0);
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 1: post-charge
    {
        char b[144];
        std::snprintf(b, sizeof b, "[sw] charging recharge_secs=%.1f frames_to_ready=%ld ready=%s",
                      p.recharge_secs, frame, reached_ready ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 2) FIRE at the target cell; assert it fires + resets.
    const bool fired = world.fire_superweapon(0, cx, cy);
    const bool reset_ok = !world.sw_ready(0) && world.sw_charge_fraction(0) < 1.0;
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 2: post-fire
    {
        char b[128];
        std::snprintf(b, sizeof b, "[sw] fired target=(%d,%d) returned=%s reset_timer=%s",
                      cx, cy, fired ? "yes" : "no", reset_ok ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 3) DETONATE: count the in-radius kills + confirm own spared / far untouched.
    int killed = 0, damaged = 0;
    for (int idx : in_radius) {
        const ra2engine::Unit& u = world.units()[idx];
        if (u.hp <= 0 || !u.alive) ++killed;
        else if (u.hp < 100) ++damaged;
    }
    const bool own_spared = world.units()[own_idx].hp == own_hp0;
    const bool far_untouched = world.units()[far_idx].hp == far_hp0;
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 3: post-detonate
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[sw] detonate killed=%d damaged=%d in_radius=%d own_spared=%s far_untouched=%s",
                      killed, damaged, (int)in_radius.size(), own_spared ? "yes" : "no",
                      far_untouched ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 4) RECHARGE: a fixed window; the SW begins re-charging from 0.
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 4: end
    r.hash = h;
    {
        char b[128];
        std::snprintf(b, sizeof b, "[sw] recharging charge_fraction=%.3f frame=%ld",
                      world.sw_charge_fraction(0), frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[sw] superweapon_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously -- the SW must
    // reach READY AND fire_superweapon must return true AND >=1 in-radius kill.
    if (!reached_ready) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: never reached READY"); }
    if (!fired) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: fire_superweapon refused"); }
    if (killed < 1) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: no in-radius kill"); }
    if (render) { capture_png(ren, end_png, W, H); }   // capture the already-rendered frame
    return r;
}
```

- Produces — `engine/main.cpp`: `run_demo_superweapon` (the `run_demo_aircraft` template — rendered run + headless run from a FRESH World, byte-compare witness logs + hashes, NO hardcoded hash literal):

```cpp
int run_demo_superweapon() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_superweapon_start.png";
    const std::string end_png = scratch + "demo_superweapon_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    std::vector<const VehicleAssets*> vlist;           // no voxel units in this gate

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 6, pc->y + 6, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    DemoSuperweaponResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-superweapon)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_superweapon(a, ua, vlist, /*render=*/true, ren, &tc, &cam,
                                      W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    DemoSuperweaponResult r2 = simulate_superweapon(a, ua, vlist, /*render=*/false, nullptr,
                                                    nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-superweapon] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d superweapon_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}
```

  (`a.rules` is the `ra2assets::Rules` on `Assets` that `resolve_superweapon` reads — verify the field name against `load_assets`; if the rules live elsewhere on `Assets`, pass that. `build_world`/`pick_start_cell`/`cell_to_screen(...,60,30,15)`/`render_world`/`capture_png` are the existing demo helpers.)

- Produces — `engine/main.cpp`: the dispatch (append after the `--demo-naval` arm at main.cpp:6753):

```cpp
            } else if (arg == "--demo-superweapon") {
                return run_demo_superweapon();
```

**Steps:**

- [ ] **Step 1: Add `superweapon_fold` + `DemoSuperweaponResult` + `simulate_superweapon` + `run_demo_superweapon` + the dispatch** exactly as in Interfaces.

- [ ] **Step 2: Build + run the new gate FOREGROUND, blocking (timeout 600000 ms — the rendered pass charges up to RechargeTime*60*15 frames; do NOT background, do NOT pipe through Select-Object):**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-superweapon
```

Expected: exit 0; the tail line reads `deterministic=yes lines=<N> superweapon_hash=<H>`; witnesses show `[sw] granted ...`, `[sw] charging ... ready=yes`, `[sw] fired ... returned=yes reset_timer=yes`, `[sw] detonate killed>=1 ... own_spared=yes far_untouched=yes`, `[sw] recharging charge_fraction=...`. NO wedge lines. If a wedge fires (`never reached READY` / `fire refused` / `no in-radius kill`), diagnose (systematic-debugging) — do NOT weaken the guard. If the charge is impractically slow, the demo-only recharge scale is the documented [RE-FALLBACK] (the FRAMEWORK always uses the real value; only the demo's grant may scale it) — but the default charges the real value.

- [ ] **Step 3: Wedge-guard proof (negative check, FOREGROUND).** Temporarily change the `simulate_superweapon` fire cell to a far corner (e.g. `world.fire_superweapon(0, 1, 1)` so no enemy is in radius), rebuild, run — expected exit 1 with `[sw] WEDGE: no in-radius kill` (proves the gate cannot pass vacuously). REVERT the edit, rebuild, re-run, expected exit 0. Do not commit the temporary edit.

- [ ] **Step 4: ALL TWELVE demos FOREGROUND, blocking (timeout 600000 ms each) — the eleven legacy pins byte-identical + the new gate deterministic:**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
D:\ra2-android\build\ra2engine.exe --demo-superweapon
```

Expected: eleven legacy pins EXACT (values in Task 3 Step 5) + `--demo-superweapon` `deterministic=yes` exit 0.

- [ ] **Step 5: Full suite FOREGROUND, blocking (timeout 600000 ms).** `D:\ra2-android\build\ra2assets_tests.exe` — expected 0 failed.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "feat(engine): --demo-superweapon gate (12th) - superweapon_fold + simulate_superweapon + run_demo_superweapon, charge->fire->detonate->recharge, two-run self-compare, wedge guards (READY AND fired AND >=1 kill) [RE: superweapon.md]

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

- [ ] **Step 7 (CONTROLLER, not the implementer): pin + eyeball.** Record the observed `superweapon_hash` value (it goes into Task 7's README gate row — never a literal in code), eyeball `demo_superweapon_start.png`/`demo_superweapon_end.png` in the scratchpad (enemy cluster present at start; cluster dead/gone after the detonation at end), and confirm whether `NukeIcon` art was present on this install (the README documents it).

---

### Task 7: Finalize — full suite both modes, twelve gates, arm64 nm-verify, README

**Files:**
- Modify: `README.md`; `android/app/jni/CMakeLists.txt` ONLY if it lists sources beyond `cmake/ra2_sources.cmake` (verify — `superweapon.cpp` should flow in via `RA2_ENGINE_SOURCES`, the same way the other engine modules do).

**Steps:**

- [ ] **Step 1: Full test suite, both modes, FOREGROUND, blocking (timeout 600000 ms):**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: 0 failed in both; the pure `superweapon*` + `World SW:*` cases run in BOTH modes (host-only, hand-built — no install needed).

- [ ] **Step 2: ALL TWELVE demos FOREGROUND, blocking (timeout 600000 ms each) — the eleven legacy pins byte-identical + the new pin stable:**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
D:\ra2-android\build\ra2engine.exe --demo-superweapon
```

Expected, explicitly all eleven legacy: `--demo-combat` `deterministic=yes logs=78`; `--demo-tank` `deterministic=yes logs=144`; `--demo-base` `hash=5273b5e71ca009e1`; `--demo-econ` `econ_hash=7fed96cb98b68017`; `--demo-prod` `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` `ai_team_hash=312e92ed862a5e74`; `--demo-ai-trigger` `ai_trigger_hash=8334a6ca1531cabc`; `--demo-transport` `transport_hash=fe500f946e913645`; `--demo-aircraft` `aircraft_hash=a69642564cf64773`; `--demo-naval` `naval_hash=6aee2c82bc024e86`; plus `--demo-superweapon` `deterministic=yes` exit 0 with `superweapon_hash` equal to Task 6 Step 7's controller-pinned value.

- [ ] **Step 3: arm64 NDK build FOREGROUND, blocking (timeout 600000 ms):**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Then nm-verify the new superweapon symbols in the arm64 `libmain.so`:

```powershell
& "D:\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe" `
  (Get-ChildItem -Recurse -Filter libmain.so D:\ra2-android\android\app\build | Where-Object { $_.FullName -match "arm64" } | Select-Object -First 1).FullName `
  | Select-String -Pattern "grant_superweapon|fire_superweapon|step_superweapon|superweapon_type_from_id|apply_spread_damage"
```

(The `Select-Object`/`Select-String` here filter an already-buffered file listing / bounded nm output stream — NOT the forbidden pattern of piping a LIVE native exe's stdout into `Select-Object -First N`; llvm-nm writes finite output and runs to completion.) Expected: at least one mangled/demangled match per symbol (`grant_superweapon`/`fire_superweapon`/`step_superweapon` are `World` members; `superweapon_type_from_id` is the pure mapping helper; `apply_spread_damage` is now the externally-linked combat applicator).

- [ ] **Step 4: README — CB-4 section.** Add a "CB-4 — Superweapons" section after the CB-3 section documenting: the SW framework (a NEW AI-independent per-house registry `sw_houses_`/`sw_any_` that works for the player house 0 — which is never in `ai_houses_`; the pure `Superweapon` lifecycle `tick`/`is_ready`/`on_fired`; `grant_superweapon` opt-in, never called by an existing demo; the firewalled `step_superweapon` pass); target selection + fire (`fire_superweapon` refused unless READY, resets after firing; the `N`-keybind/cameo-click arm → map-click fire reusing the existing screen→cell pick); the fired EFFECT (a transient virtual detonator `Unit` + the REUSED `apply_spread_damage` — `Unit` untouched, `combat.cpp` behavior unchanged, the applicator promoted to external linkage so `world.cpp` can call it; the `[NukePayload]`/`[NUKE]` values resolved from Rules; enemies-only, instant delivery); the sidebar SW UI (`NukeIcon` cameo + `gclock2.shp` recharge clock + the flashing-green READY tint, art-guarded); and the byte-identity discipline (`Unit` + `world_hash` untouched — strategy (a); the combat linkage-only edit; the eleven legacy gates re-verified at the first additive commit (Task 3) and at finalize). Update the "Determinism gates (permanent)" intro from "eleven headless demos" to "twelve headless demos" and note `--demo-superweapon` is the new CB-4 gate.

Add the CLI bullet after the `--demo-naval` bullet:

```
- `ra2engine.exe --demo-superweapon` — the CB-4 Superweapons integration gate:
  grants house 0 the Soviet Nuclear Missile (`[NukeSpecial]`, `Type=MultiMissile`)
  via `grant_superweapon` with the recovered `[NukePayload]`/`[NUKE]` params
  (Damage=600, CellSpread=10, PercentAtMax=.02), spawns a cluster of enemy units
  (house 1) around a target cell within the nuke radius plus one own unit in-radius
  and one far enemy out-of-radius, then scripts CHARGE (steps the real recharge
  timer to READY), FIRE (`fire_superweapon` at the cell — returns true, resets the
  timer), DETONATE (the reused `apply_spread_damage` kills the in-radius enemies,
  spares the firing house, leaves the far enemy untouched), and RECHARGE (the SW
  re-charges from 0) — and hashes every unit's cell/alive/hp + the firing house's
  SW quantized-timer/ready/type + the target cell at four checkpoints
  (`superweapon_hash`). Runs twice (rendered + headless); both runs must produce
  identical witness lines and hash; wedge guards exit non-zero unless the SW
  reached READY AND fired AND >=1 in-radius enemy was killed.
```

Add the gate row (the hash value is Task 6 Step 7's controller-pinned observation, plus whether `NukeIcon` art was present):

```
| `--demo-superweapon` | `superweapon_hash=<pinned value from Task 6 Step 7>` | `deterministic=yes`; grants the Soviet Nuclear Missile (`[NukeSpecial]` `Type=MultiMissile`, `RechargeTime` -> recharge_secs, `[NukePayload]` Damage=600, `[NUKE]` CellSpread=10/PercentAtMax=.02/Verses); charges to READY, fires at the target cell, detonates via the REUSED `apply_spread_damage` (enemies-only, instant), spares the firing house, leaves out-of-radius units untouched, re-charges from 0; NukeIcon cameo + gclock2 recharge clock (art-guarded, placeholder if absent); wedge guards armed (`docs/re/superweapon.md`) |
```

Add to the "Deviations we take" pattern (a CB-4 list, each item cited to superweapon.md — mirroring the hunt doc's own "deviations we take", adjudicated at the Task 1 RE-GATE):
  - **`apply_spread_damage` promoted to external linkage (combat.cpp behavior unchanged)** — the nuke REUSES the exact CB-2 area-damage applicator; because it lived in `combat.cpp`'s anonymous namespace it was moved to the `ra2engine` namespace + declared in `combat.hpp` so `world.cpp` can call it. Linkage-only: the function body + its existing caller (`try_fire`) are byte-for-byte unchanged and all twelve gate outputs are bit-identical. This is the one departure from the spec's "`combat.cpp` literally untouched" wording — the alternative (reimplementing the falloff loop) is the "new damage system" §5/§8 forbid [`superweapon.md` §2; spec §5/§8].
  - **Instant area-damage detonation; missile flight deferred** — the effect applies at the target cell the frame `fire_superweapon` is called (the established instant/simplified-delivery precedent); the `GiantNukeUp`/`GiantNukeDown` missile flight, `DetonationAltitude`, and the `NUKEANIM` mushroom cloud are carry-forwards [`superweapon.md` §5; spec §Out].
  - **Enemies-only splash** — the reused applicator's `w.house == attacker.house` skip spares the firing house; the real nuke damages friendlies too. The enemies-only splash is kept (a documented divergence); friendly-fire splash is a carry-forward [`superweapon.md` §5; spec §Out].
  - **Building splash per the RE-GATE** — if the hunt confirmed the nuke damages buildings, an SW-only building-splash loop mirrors the unit falloff; otherwise building damage is a carry-forward. The gate targets units regardless [`superweapon.md` §5.3; spec §Out].
  - **`RadLevel=500` lingering radiation dropped** — CB-4 applies the one-shot `[NUKE]` splash only; the radiation field / over-time damage is out [`superweapon.md` §2; spec §Out].
  - **SW granted directly, not via a Nuke Silo** — CB-4 grants the SW to a house directly (like CB-1/2/3 spawned their exemplars directly); wiring "build the Nuke Silo → unlock the SW" through the prerequisite/production system is a carry-forward [spec §Out].
  - **Only the `MultiMissile`/Nuke `Type` implemented** — the full SW `Type` enum (Iron Curtain, Chronosphere, Lightning Storm, Psychic Dominator, …) is RE-documented and mapped (`SuperweaponType` + `superweapon_type_from_id`) so each deferred kind starts from a mapped point; CB-4 ships only the area-damage detonation effect [`superweapon.md` §4; spec §Out].
  - **AI does not use superweapons; the AI-3 inert SW-charge conditions stay inert** — `AIDefendAgainst=yes` / the AI SuperDefense system stays inert; the `ai-trigger.md` ct=5/ct=6 IronCurtainCharged/ChronoSphereCharged conditions are NOT wired to this framework in CB-4 (CB-4 grants only the Nuke, which neither names) — a carry-forward [spec §Out].
  - **SW state is LIVE but excluded from `world_hash`** — the SW timer/ready/type is hashed only by `superweapon_hash` (strategy (a)); `world_hash` and `Unit` are untouched; no existing demo grants a SW, so no existing hash changes [`superweapon.md` §5; spec §8].

- [ ] **Step 5: Commit.**

```powershell
git -C "D:\ra2-android" add README.md
git -C "D:\ra2-android" commit -m "docs: CB-4 superweapon README - 12th gate row (--demo-superweapon), CLI bullet, deviations

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

(If Step 3's verify found `android/app/jni/CMakeLists.txt` needed an explicit `superweapon.cpp` line after all, include it in this commit and say so in the message body.)

---

## Self-Review (performed at write time; PRE-HUNT — re-run after the RE-GATE amendment)

> **PRE-HUNT status.** This plan is authored BEFORE the Task-1 hunt. Every numeric/semantic uncertainty is tagged `[RE-GATE: superweapon.md §N]` with its pre-hunt FALLBACK inline; the controller flips each to `[RE-GATE APPLIED: superweapon.md §N]` at Task 1 Step 8 (STATUS: PENDING) and re-runs this self-review. The cross-task contract below is the pre-hunt design.

1. **Spec coverage:**
   - Spec §1 (Ghidra hunt, five items + art-verify + building-splash question) → Task 1 questions 1–5 verbatim, each landing CONFIRMED/PROBABLE/[RE-FALLBACK] with a located-functions table + confidence table + deviations section; the REAL rulesmd extracted via ra2extract with `[NukeSpecial]`/`[NukePayload]`/`[NUKE]`/`[SuperWeaponTypes]` recorded verbatim in scratch; content-anchored headless sweep on `"RechargeTime"`/`"Type"`/`"NukeSpecial"`/`"MultiMissile"` xref-walked from the combat.md/ai-base.md entry maps; Step 5 art-verifies `NukeIcon`/`NUKEANIM`; Step 6 records the building-splash decision; Step 8 is the controller RE-GATE amendment step (STATUS: PENDING). The full `Type` enum mapping-for-later is in §4 + the deviations (spec §Out requirement).
   - Spec §2 (architecture + data model) → the 7-task spine mirrors §2's list exactly (hunt → pure module → World registry → fired effect → sidebar UI+input → gate → finalize); the tripwire is stated (Task 5 sidebar UI is the split seam). The approved data model lands: `SuperweaponType { None=0, Nuke=1, … }` (12 named, only Nuke active), `Superweapon { type, recharge_secs, timer, ready }` with `tick`/`is_ready`/`on_fired`/`charge_fraction`, `HouseSuperweapon { has_sw, sw, eff_damage, eff_cell_spread, eff_percent_at_max, eff_verses }`, `World { sw_houses_, sw_any_ }` + `grant_superweapon`/`step_superweapon`/`fire_superweapon` + accessors, `Unit` UNCHANGED, `combat.cpp` behavior UNCHANGED (linkage-only export), the pure module registered in BOTH cmake files.
   - Spec §3 (recharge-timer lifecycle) → Task 3's `grant_superweapon` (grow + set params + open `sw_any_`), `step_superweapon` (firewalled tick in house-id order, no RNG), the `World::step` insertion after `step_flight`, the `sw_ready`/`sw_charge_fraction` accessors; determinism (no new RNG, no reordering, pure arithmetic on dt).
   - Spec §4 (target selection & fire) → Task 5's `N`-keybind/cameo-click arm → map-click fire reusing the existing screen→cell pick (the placement-mode parallel in `Input`); Task 4's `fire_superweapon` refused unless READY, detonates, `on_fired` resets; the demo's direct `fire_superweapon(0, cx, cy)` call (Task 6, no synthetic input); no range cap (Range/LineMultiplier cosmetic, [RE-GATE §3]).
   - Spec §5 (the fired EFFECT) → Task 4's transient virtual detonator `Unit` (house + fx/fy + the four warhead fields) + the REUSED `apply_spread_damage(units_, occ_, virt, virt)`; the building-splash [RE-GATE §5.3] block (fallback units-only); enemies-only + instant delivery documented; `combat.cpp` behavior unchanged (the one linkage-only edit is the documented refinement of the spec's "untouched" premise — see the Known-gap note below).
   - Spec §6 (sidebar SW UI) → Task 5's `NukeIcon` cameo (art-guarded) + `gclock2.shp` recharge clock (`gframe = round(charge_fraction·54)`) + the flashing-green READY tint, drawn only when house 0 has a granted SW (no existing screenshot changes).
   - Spec §7 (gate) → Task 6: grant house 0 the Nuke, spawn an enemy cluster in-radius + own-in-radius + far-enemy, CHARGE → FIRE → DETONATE → RECHARGE, `superweapon_fold` over every unit's (cell_x, cell_y, alive, hp) + the SW (quantized timer, ready, type) + the target cell, four fixed checkpoints, FNV via `fnv_mix` + `world_hash`'s basis, two-run compute-and-self-compare with NO pinned literal (controller pins in README), wedge guards (READY AND fired AND ≥1 kill) exit 1, rendered ≡ headless. Step 3 proves the wedge fires (negative check, reverted).
   - Spec §8 (byte-identity) → copied into Global Constraints; enforced structurally (`Unit`/`world_hash` untouched, `combat.cpp` behavior unchanged, inert `sw_houses_`/`sw_any_` defaults, firewalled `step_superweapon`, detonation fires only on `fire_superweapon`, SW slot drawn only when granted) and empirically (11-gate re-runs in Tasks 3, 4, 5; the 12-demo runs in Tasks 6, 7).
   - Spec §9 (testing) → pure doctests `tests/test_superweapon.cpp` (charges at exactly recharge_secs, no overshoot, on_fired resets, is_ready threshold, enum + id mapping) + `tests/test_world.cpp` (grant materializes + opens sw_any_, step charges granted/leaves others inert, no-op when none granted, fire refused-before/succeeds-after + resets, detonation kills enemies/spares firing house/leaves out-of-radius untouched, falloff edge<center); integration (the `--demo-superweapon` gate with wedge guards + two-run self-compare, rendered ≡ headless); regression (11 gates byte-identical at the first additive commit AND at finalize, full suite both modes, arm64 nm-verify).
   - Spec §Out-of-scope → nothing implements silo-grants-SW, other SW types, missile flight/mushroom-cloud, lingering radiation, friendly fire, AI SW use, power-suspend, or (unless the RE-GATE confirms) building splash; each is a documented deviation/carry-forward (Task 7 Step 4 + the superweapon.md deviations).
2. **Placeholder scan:** no TODO/TBD/"similar to Task N" in any operative step; every code step shows complete code; every run step shows the exact PowerShell command + expected output. Deliberate non-code references that remain are legitimate anchors into the EXISTING tree (e.g. "world.cpp:1992", "the placement block at input.cpp:269", "the cameo/gclock2 block at main.cpp:2515–2528") with the inserted code given in full. All numeric/semantic uncertainties are tagged `[RE-GATE: superweapon.md §N]` with the FALLBACK inline (RechargeTime→secs ·60; `[NukePayload]`/`[NUKE]` attribution; the `[SuperWeaponTypes]` id map; building-splash; NukeIcon art). The `superweapon_hash` pin is extract-then-pin by the controller (Task 6 Step 7 → Task 7 Step 4), deliberately never a literal in code.
3. **Type/signature consistency (checked across tasks):**
   - `SuperweaponType` (superweapon.hpp, Task 2) `None=0, Nuke=1, IronCurtain=2, …, SpyPlane=11` — used by Task 2 (tests + `superweapon_type_from_id`), Task 3 (`grant_superweapon` param, `Superweapon::type`), Task 6 (`superweapon_fold` folds `static_cast<int>(sw->type)`), and the enum ordering/id map is [RE-GATE §4]. `None=0` so a value-initialized record is inert.
   - `struct Superweapon { SuperweaponType type; double recharge_secs; double timer; bool ready; tick(double)/is_ready()/on_fired()/charge_fraction() }` (superweapon.hpp, Task 2, header-inline) — embedded in `HouseSuperweapon` (Task 3), read via `World::sw_state` (Tasks 3/6), ticked by `step_superweapon` (Task 3), reset by `fire_superweapon` (Task 4). `recharge_secs` is SECONDS throughout (main.cpp does the minutes→secs conversion in `resolve_superweapon`).
   - `HouseSuperweapon { bool has_sw; Superweapon sw; int eff_damage; double eff_cell_spread; double eff_percent_at_max; std::array<int,11> eff_verses }` (world.hpp private, Task 3) — written by `grant_superweapon`, read by `fire_superweapon`/accessors. **Deviation from spec §2 data model (double eff_damage / std::array<double,11> eff_verses):** `eff_damage` is `int` and `eff_verses` is `std::array<int,11>` to match the DESTINATION `Unit::w_damage` (int) and `Unit::w_verses` (`std::vector<int>`, consumed by `damage_vs(int, const std::vector<int>&, int)`) — the whole reuse hinges on feeding `apply_spread_damage` without touching combat, so the effect-param types must match its int-based inputs. `fire_superweapon` does `virt.w_verses.assign(eff_verses.begin(), eff_verses.end())`.
   - `void World::grant_superweapon(int house, SuperweaponType, double recharge_secs, int dmg, double cell_spread, double pmax, const std::array<int,11>& verses)` (Task 3) — consumed by the Task 3/4 World tests and by `simulate_superweapon` (Task 6) with `grant_superweapon(0, Nuke, p.recharge_secs, p.damage, p.cell_spread, p.percent_at_max, p.verses)` where `p` is `resolve_superweapon`'s `SuperweaponParams`. Field/param names line up 1:1.
   - `bool World::fire_superweapon(int house, int cx, int cy)` (decl Task 3, def Task 4) — builds the transient detonator `Unit virt` with the field set `{house, fx, fy, w_damage, w_cell_spread, w_percent_at_max, w_verses}` (exactly the fields `apply_spread_damage` reads, verified against combat.cpp:84–101) and calls `apply_spread_damage(units_, occ_, virt, virt)`. Consumed by Task 4 tests, Task 5 input routing (`world.fire_superweapon(0, act.sw_cx, act.sw_cy)`), Task 6 gate.
   - `void World::step_superweapon(double dt)` (Task 3, private) — firewalled `if (!sw_any_) return;`, inserted in `World::step` after `step_flight`. No caller outside `World::step`.
   - `bool World::sw_ready(int) const` / `double World::sw_charge_fraction(int) const` / `const Superweapon* World::sw_state(int) const` (Task 3) — consumed by Task 5 (`draw_sw_slot`, the `N`-key arm gate) and Task 6 (`simulate_superweapon`, `superweapon_fold`).
   - `void apply_spread_damage(std::vector<Unit>&, Occupancy&, const Unit& attacker, const Unit& center)` (combat.hpp decl + combat.cpp def, Task 4) — the exact existing signature (verified combat.cpp:84), now externally linked; called by `fire_superweapon` (Task 4) and (unchanged) `try_fire` (combat.cpp:122).
   - `SuperweaponParams { double recharge_secs; int damage; double cell_spread; double percent_at_max; std::array<int,11> verses; bool ok }` + `SuperweaponParams resolve_superweapon(const ra2assets::Rules&)` (main.cpp, Task 4) — consumed by `simulate_superweapon` (Task 6). Reads `rules.section_double("NukeSpecial","RechargeTime")*60`, `("NukePayload","Damage")`, `("NUKE","CellSpread"/"PercentAtMax")`, `rules.verses("NUKE")` (the existing accessors, main.cpp:616/624).
   - `InputActions::{sw_fire_confirmed, sw_cx, sw_cy}` + `Input::{arm_superweapon,cancel_superweapon,sw_targeting,sw_targeting_}` (input.hpp/cpp, Task 5) — mirror the placement `place_confirmed`/`place_cx`/`place_cy` + `placement_active`/`cancel_placement`; consumed by main.cpp's `N`-key arm + the `act.sw_fire_confirmed` fire routing.
   - `uint64_t superweapon_fold(uint64_t, const ra2engine::World&, int house, int cx, int cy)` + `DemoSuperweaponResult` + `simulate_superweapon(...)` + `int run_demo_superweapon()` (main.cpp, Task 6) — defined and consumed within Task 6; `superweapon_fold` reads `u.cell_x/cell_y/alive/hp` + `sw->timer/ready/type`; uses `fnv_mix` + the `0xcbf29ce484222325ull` basis (world_hash UNTOUCHED).
   - Existing signatures used verbatim from the current tree (verified at write time): `World::add_combat_unit(int,int,int,const UnitCombat&) -> Unit&`, `World::{units,step,set_anim}`, `UnitCombat::{image,strength,armor}`, `Unit::{house,fx,fy,w_damage,w_cell_spread,w_percent_at_max,w_verses,hp,max_hp,alive}`, `damage_vs(int,const std::vector<int>&,int)`, `fnv_mix(uint64_t,uint64_t)` + `0xcbf29ce484222325ull`, `world_hash` (UNTOUCHED), `build_world(a,ua,0)`, `pick_start_cell`, `capture_png`, `render_world(...)`, `cell_to_screen(x,y,level,60,30,15)`, `load_assets`/`load_unit_assets`/`env_install_dir`, `Rules::{section_double,verses}`, the `load_sb`/`cameo.mix` chain + `blit_shp`/`shade_rect`/`sb_gclock`, the placement `screen_to_cell` pick (input.cpp:114–120), the `unload_key` edge-detect (main.cpp:2908).
   - Byte-identity: Task 2 = a new pure TU (additive — gates unaffected); Task 3 = a new World member + a firewalled `step_superweapon` insertion provably no-op (`sw_any_` false everywhere) — gates re-run; Task 4 = the linkage-only `apply_spread_damage` relocation (body unchanged) + `fire_superweapon` (called by no existing demo) + `resolve_superweapon` (called by no existing demo) — gates re-run; Task 5 = the SW slot drawn only when `sw_state(0)!=nullptr` (null in every existing demo) + the firewalled arm/fire path — gates re-run; Task 6 adds a new demo beside the others (all twelve run); Task 7 re-asserts everything + arm64.
4. **FOREGROUND discipline:** every build/test/demo step in every task explicitly orders a foreground, synchronous, blocking run with a generous timeout (up to 600000 ms for rendered demos), exes run from PowerShell (never Git Bash), the twelve gate demos run INDIVIDUALLY (never a 12-in-one chain), and no step ever pipes a live native exe through `Select-Object -First N` (the two bounded llvm-nm / ra2extract-`--list` pipelines are annotated as finite-output, run-to-completion — not the forbidden live-sim pattern).

> **Known gap flagged to the controller (NOT a stale placeholder):** the spec repeatedly asserts `combat.cpp` is "LITERALLY untouched." That is **not achievable** as literally worded — `apply_spread_damage` lives in `combat.cpp`'s anonymous namespace (internal linkage), so `world.cpp` cannot call it. The plan resolves this with the minimal faithful edit (promote the function to `ra2engine` + declare it in `combat.hpp` — Task 4 Step 1), which is linkage-only (body + `try_fire`'s call byte-for-byte unchanged) and re-verified to keep all twelve gate OUTPUTS bit-identical. This preserves the invariant the spec actually cares about (identical gate output + one source of truth for the damage math) at the cost of the literal "untouched" wording. Surfaced here + in Global Constraints + in the README deviations.
