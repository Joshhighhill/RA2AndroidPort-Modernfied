# CB-1 Transports/Cargo Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A generic cargo/passenger subsystem for the from-scratch RA2 engine — units LOAD into a friendly ground transport (right-click enter gesture, path there, board at Chebyshev adjacency), are CARRIED off-map (not rendered, not targetable, holding no cell, moving with the transport), and are UNLOADED one-per-logic-frame in LIFO order via the recovered facing±90° scatter picker (`eject_cell` — the transport's facing rotated ±0x4000 with the side alternating per eject; the original does NOT reuse `KickOutUnit`'s ring — HOLD when boxed in) [RE-GATE APPLIED: transport-cargo.md §4/§6], demonstrated deterministically on the real HTK Flak Track (`Passengers=5 SizeLimit=2`, FV art-guard fallback) and gated by the **9th permanent byte-identity gate `--demo-transport`** with `transport_hash` + wedge guards. This is CB-1, the first phase of the content-breadth arc; cargo is the substrate CB-2 Aircraft / CB-3 Naval / garrisons later stand on. Spec: `docs/superpowers/specs/2026-07-18-transport-cargo-design.md`.

**Architecture:** Task 1 recovers ground truth from gamemd.exe + the real rulesmd.ini into `docs/re/transport-cargo.md` (RE-GATE before any dependent code). Then the change lands additively into the CORE unit sim — it cannot hide behind the `ai_any_` firewall, so the byte-identity discipline is itself a deliverable (spec §8): a new pure module `engine/cargo.{hpp,cpp}` holds the COUNT capacity gate + per-passenger size cap (`can_load`), the pip-only Σ-Size metric (`cargo_total_size` — the `GetTotalSize` analogue, never consulted by the gate), the LIFO eject rule (`next_eject`), and `eject_cell` — the facing±90° scatter picker recovered from the original unload (`FUN_00415C60`). `engine/factory.cpp`/`exit_cell` is UNTOUCHED: the pre-hunt ring extraction was DROPPED at RE-GATE (the hunt proved the original unload does NOT reuse the KickOutUnit ring), making Task 2 purely additive and removing the phase's biggest byte-identity risk (the 8-gate re-verify still runs at the additive Task-2 commit). `engine/world.{hpp,cpp}` grows inert-by-default `Unit` cargo fields, `order_enter_transport`/`order_unload_selected`, a `step_cargo` step inserted between `step_units` and `step_turrets` behind a `cargo_any_` firewall bool, carried-unit guards grown onto EXISTING corpse-skip clauses (provably false in every existing demo), and the recovered transport-death disposition (cargo dies with the transport — CONFIRMED, no OpenTopped survival branch). `main.cpp` resolves `Passengers`/`SizeLimit`/`Size` from the real rulesmd, loads HTK art-guarded (FV fallback), wires the right-click enter gesture + the `D` unload key + a minimal loaded-count pip row + the guarded Enter/LeaveTransportSound hook, and adds the `--demo-transport` gate (scripted load→carry→unload→resume, FNV `transport_hash` folded at four checkpoints, two-run compute-and-self-compare, wedge guards exit 1). `world_hash` is NOT touched (strategy (a): cargo fields are hashed ONLY by `transport_hash`).

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle, NDK `D:\android-ndk-r27c`), Ghidra headless (`analyzeHeadless.bat`, JAVA_HOME=`C:\Program Files\Java\jdk-21`, project at `D:\ra2-re`), SDL2 (wiring layer only), `ra2assets::Rules`/`ra2assets::Ini` (unchanged) as the rulesmd substrate.

## Global Constraints

(Copied verbatim from spec §Global constraints (project-wide) — every task's requirements implicitly include this section.)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real rulesmd / prose-only refs) BEFORE implementing; the Task 1 hunt + RE-GATE precede all dependent tasks; every recovered constant cites `docs/re/transport-cargo.md`; [RE-FALLBACK] items become plan-amendment gates, never guesses.
- EIGHT byte-identity gates stay green (spec §8); `--demo-transport` becomes the 9th permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127). `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without the install. Long rendered demos auto-background on short timeouts; NEVER pipe a native exe through `Select-Object -First N` (stops draining stdout → false hang).
- No game assets / GPL source / Ghidra output committed; recovered numeric constants ARE committable.
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs — the plan's briefs must order FOREGROUND synchronous runs with generous timeouts.

**Byte-identity discipline for a CORE-SIM feature (spec §8 — copied, load-bearing for every task):**

- **The 8 existing permanent gates MUST stay bit-identical:** `--demo-combat` ends `deterministic=yes logs=78`; `--demo-tank` ends `deterministic=yes logs=144`; `--demo-base` prints `hash=5273b5e71ca009e1`; `--demo-econ` prints `econ_hash=7fed96cb98b68017`; `--demo-prod` prints `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` prints `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` prints `ai_team_hash=312e92ed862a5e74`; `--demo-ai-trigger` prints `ai_trigger_hash=8334a6ca1531cabc`. Any change to their output is a defect in YOUR change.
- **All new Unit fields default INERT** (`cargo` empty, `cargo_of = -1`, `enter_target = -1`, `unload_pending = false`, `passengers_max = 0`, `eject_counter = 0`): no existing demo ever issues a load/unload order and no existing spawn path sets `passengers_max > 0`.
- **`world_hash` strategy (a) — EXCLUDE, chosen:** `world_hash` reads exactly `(cell_x, cell_y, hp, alive)` per unit + the building fields and is NOT touched by this phase; the new cargo fields are hashed ONLY by `transport_hash` in the new gate. Coords stay STALE while carried and `alive` is only changed by the death rule no existing demo triggers — even the HASHED fields are untouched.
- **`step_cargo` is firewalled:** a World-level `cargo_any_` bool, set the first time an enter/unload order is issued, gates the entire function — `if (!cargo_any_) return;` is the only cost every existing demo pays. It is inserted as a NEW call between `step_units` and `step_turrets` in `World::step` — a pure insertion; no existing unit-step logic is reordered.
- **Every carried-unit guard (spec §4) is an added clause** (`|| cargo_of >= 0`) on an existing corpse-skip condition — provably false for every unit in every existing demo (default -1).
- **Task 2 is purely additive** (a new TU + inert trailing `Unit` fields; `engine/factory.cpp`/`exit_cell` is UNTOUCHED — the pre-hunt ring extraction was DROPPED at RE-GATE [transport-cargo.md §4]): the 8-gate re-verify still runs immediately at the additive Task-2 commit (there is no refactor to isolate now; the run simply pins the additive commit green).

**Plan-operational addenda (AI-1/AI-2/AI-3 precedent, unchanged):**

- **[RE-GATE] protocol:** Tasks 2–6 are written with best-known PRE-HUNT values, each consuming point marked `[RE-GATE APPLIED: transport-cargo.md §N]` with its fallback inline. After Task 1, the controller diffs `docs/re/transport-cargo.md` against every `[RE-GATE APPLIED]` block in Tasks 2–6, amends the plan, and commits the amendment BEFORE dispatching the task that uses it. Values the hunt cannot pin are **[RE-FALLBACK]** plan-amendment gates — STOP, present options to the human, amend the plan, commit the amendment BEFORE any dependent dispatch. Never guess silently. **STATUS: APPLIED (0cef283) — the Task 1 hunt ran (`docs/re/transport-cargo.md`, commit 0cef283) and this amendment folds the controller's adjudication (D1–D7) into every `[RE-GATE APPLIED]` block below: capacity = COUNT not size-sum (§1), container = LIFO not FIFO (§6), eject = facing±90° scatter via the new `eject_cell` with the `exit_cell` ring extraction DROPPED — `factory.cpp` untouched (§4), one eject per tick + HOLD with no DeployTime start delay (§4), death disposition confirmed (§5), same-house-only and adjacency boarding kept as documented deviations (§2/§3).**
- Run every build/test/demo **FOREGROUND, synchronously, blocking** (generous timeout, e.g. 600000 ms — rendered demos included) — do NOT background test runs and wait; prior phases lost subagents to that (one hung waiting on a monitor). NEVER pipe a native exe through `Select-Object -First N`.
- Desktop build: `cmake --build D:\ra2-android\build --target ra2assets_tests` then run `D:\ra2-android\build\ra2assets_tests.exe`. Engine: target `ra2engine`, exe `D:\ra2-android\build\ra2engine.exe`. doctest filters are name-based `--test-case="prefix*"`.
- Tests skip cleanly without `RA2_INSTALL_DIR` — doctest `MESSAGE` + `return`. The Task 2 pure cargo tests use hand-built Units and run host-only (no install needed).
- Scratch files (hunt scripts, extracted rulesmd.ini, screenshots) go to the session scratchpad, never the repo.
- Model assignments: implementation → opus; specs/plans/review → fable; easy/mechanical tasks → sonnet.

### Approved scope decisions (spec §Approved scope decisions)

- **Data model:** cargo list on the transport `Unit` + `cargo_of` back-link on the passenger; passengers held OFF-MAP (no render, no occupancy, no combat, no pathing); capacity is a **passenger COUNT** (`Passengers=` bodies — each passenger is ONE slot regardless of `Size`) with a separate per-passenger cap (`SizeLimit=` gates the largest admissible `Size`); the Σ-Size metric (`cargo_total_size`) is DISPLAY-ONLY (the pip row — the original `GetTotalSize` has no capacity caller) [RE-GATE APPLIED: transport-cargo.md §1 — CONFIRMED; the pre-hunt size-sum pool is DELETED].
- **Transport:** **HTK (Soviet Flak Track)** — `[HTK] Passengers=5, SizeLimit=2, Size=3`, ground Drive locomotor, art PRESENT on this install (htk.vxl/htk.hva/htktur.vxl/htktur.hva probed in ra2.mix/local.mix). **FV (IFV, Passengers=1, already producible)** is the art-guard fallback; multi-passenger logic fully implemented regardless, capacity>1 / Size>1 covered by pure tests.
- **Passengers (demo):** E1 (GI, Size=1). Size=2 passengers (DRON — art absent on this install) exercise the COUNT-slot vs pip-size split in pure tests only [RE-GATE APPLIED: transport-cargo.md §1].
- **Load:** right-click an own transport with eligible units selected → enter order; passenger paths there and boards at Chebyshev adjacency (documented deviation — the original boards on CO-LOCATION, the passenger pathing onto the transport's own tile [RE-GATE APPLIED: transport-cargo.md §3]), subject to remaining capacity (re-checked at boarding).
- **Carry:** `cargo_of >= 0` = off-map; passenger coords are STALE while carried (no reader consumes them) and stamped on eject/death.
- **Unload:** ejects one passenger per logic frame [RE-GATE APPLIED: transport-cargo.md §4 — one per mission tick CONFIRMED; the `DeployTime` deploy-START delay is NOT implemented in CB-1, its scale being unpinned — documented carry-forward] onto the facing±90° scatter cell (`eject_cell`: transport facing ±0x4000, side alternating per eject, nearest-free-neighbour fallback — NOT the `exit_cell` ring), LIFO [RE-GATE APPLIED: transport-cargo.md §6 — CONFIRMED], HOLD-and-retry when boxed in [§4 — CONFIRMED put-back].
- **Rendering/HUD:** no passenger render while loaded; a minimal loaded-count indicator reusing EXISTING chrome (no new font/UI systems); Enter/LeaveTransportSound through the existing guarded audio hook (nice-to-have, non-gating — cut first if Task 4 runs long).
- **Verification:** new deterministic gate `--demo-transport` (the 9th) + `transport_hash` (FNV, two-run compute-and-self-compare, NO pinned literal) + wedge guards (a load AND an unload AND a capacity refusal must actually happen, AND the eject order must be LIFO, else exit 1).
- **Decomposition tripwire:** if the carry guards (spec §4) fan out much wider than the enumerated sites and push the plan past ~9 tasks, the guard sweep splits into its own reviewed task with a per-site checklist. (This plan lands at 6 — the enumerated sweep is 12 sites, all one-clause edits, folded into Tasks 3/4; the tripwire holds.)

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/transport-cargo.md` (create, Task 1) | Calibration authority: Passengers/Size/SizeLimit semantics + defaults, the can-enter predicate chain, load trigger point + remove-order, unload ring/cadence/facing/HOLD, transport-death disposition, deterministic ordering |
| `engine/cargo.hpp` + `cargo.cpp` (create, Task 2) | Pure CB-1 module: `can_load` (COUNT capacity gate + per-passenger `Size<=SizeLimit`), `cargo_total_size` (pip-only Σ-Size), `next_eject` (LIFO), and `eject_cell` (facing±90° scatter picker + nearest-free-neighbour fallback). `engine/factory.cpp` is NOT touched — the pre-hunt ring extraction was DROPPED at RE-GATE [transport-cargo.md §4] |
| `engine/world.hpp` (modify, Tasks 2–3) | Task 2: inert `Unit` cargo fields. Task 3: `CargoEvent`, `order_enter_transport`/`order_unload_selected`/`take_cargo_events` declarations, private `step_cargo` + `cargo_any_` + `cargo_events_` |
| `engine/world.cpp` (modify, Task 3) | `step_cargo` (boarding / carry / eject / transport-death), the two orders, the `step()` insertion, `add_combat_unit` cargo-stat copy, carried-unit guards in `step_units`/`step_turrets`/`unit_at_cell`/`unit_draw_order`/`ai_idle_units`/`ai_quarry_exists`/`ai_pick_quarry_unit`, order-supersession clauses |
| `engine/unit_combat.hpp` (modify, Task 3) | `UnitCombat` gains `passengers`/`size_limit`/`size` (inert defaults) |
| `engine/combat.cpp` (modify, Task 3) | `valid_target` grows the carried clause; `step_combat` skips carried attackers |
| `engine/harvest.cpp` (modify, Task 3) | carried-harvester guard on the mission-FSM loop |
| `engine/input.cpp` (modify, Task 4) | `unit_under` carried guard; the right-click enter-transport gesture |
| `engine/iso_renderer.hpp` + `iso_renderer.cpp` (modify, Task 4) | `draw_cargo_pips` — the minimal loaded-count indicator (same anchor + rect primitive as `draw_hp_bar`) |
| `engine/main.cpp` (modify, Tasks 4–5) | Task 4: `resolve_combat_unit` cargo-stat resolution, `TransportAssets`/`load_transport_assets` (HTK art-guarded, FV fallback), interactive transport spawn + vlist, unload key `D`, render_world carried guards + pips, Enter/LeaveTransportSound. Task 5: `transport_fold` + `simulate_transport` + `run_demo_transport` + `--demo-transport` dispatch |
| `tests/test_cargo.cpp` (create, Task 2), `tests/test_world.cpp` (append, Task 3), `tests/test_input.cpp` (append, Task 4) | per-layer pins |
| `cmake/ra2_sources.cmake` + `CMakeLists.txt` (modify, Task 2) | register `engine/cargo.cpp` + `tests/test_cargo.cpp` in BOTH builds |
| `README.md` (modify, Task 6) | CB-1 section + 9th gate row + CLI bullet + deviations list |

**Module-placement decision:** `engine/cargo.hpp` includes `engine/world.hpp` for `Unit` — the exact layering `harvest.hpp` already uses (world.hpp defines Unit; the pure module layers on top; world.cpp includes the module back). `world.hpp` does NOT include `cargo.hpp` (no cycle): `CargoEvent` and all World declarations live in `world.hpp` itself, and `world.cpp`/`main.cpp` include `cargo.hpp` where they consume the pure functions (`factory.cpp` does NOT — it is untouched by CB-1).

---

### Task 1: Ghidra hunt → docs/re/transport-cargo.md

**Files:**
- Create: `docs/re/transport-cargo.md`
- Scratch: extracted rulesmd.ini + hunt scripts + decompile dumps under the session scratchpad (never committed; nothing off-repo committed)

**Interfaces:**
- Consumes: gamemd.exe YR 1.001 in the Ghidra project at `D:\ra2-re` (already analyzed in prior hunts; `analyzeHeadless.bat` + `-process` + `-postScript`, `JAVA_HOME="C:\Program Files\Java\jdk-21"`, FOREGROUND, generous timeouts — headless runs take minutes), plus the real rulesmd.ini extracted with `D:\ra2-android\build\ra2extract.exe` from the install (`RA2_INSTALL_DIR`, default `D:\Red Alert 2`) into the scratchpad.
- Known content anchors to seed the hunt — the **literal INI-key strings** the binary loads: `"Passengers"`, `"SizeLimit"`, `"Size"`, `"PipScale"`, `"OpenTopped"`, `"EnterTransportSound"`, `"LeaveTransportSound"`, `"DeployTime"`. Prior located functions to xref-walk from (the entry map is `docs/re/production.md` §located-functions): `FUN_00443BC0` (**BuildingClass::KickOutUnit** — the pinned `DAT_0089E5F8` 8-ring walk this phase reuses for unload), `FUN_0047C430` (cell-occupant test the ring probes with), `FUN_0045FE40` (BuildingTypeClass::LoadFromINI — the string-ref→struct-offset technique that also pins the TechnoTypeClass loader offsets for Passengers/SizeLimit/Size), `FUN_007C4D00`/`FUN_0065C220` (the lockstep RNG pair — flag any consult in the enter/eject paths). YRpp/Ares names are UNTRUSTED prose hints only (`TechnoTypeClass::Read_INI` for the key loads; the radio-message TRYENTER/QUEUE_MOVE handshake on TechnoClass/FootClass for boarding; `Mission_Unload` on FootClass/UnitClass for the deploy-unload; the TechnoClass destructor/`~FootClass` passenger sweep for transport death) — verify every address by content.
- Produces: `docs/re/transport-cargo.md` — sections §1–§6 mirroring the spec's §1 items 1–6 below, each finding **CONFIRMED / PROBABLE / [RE-FALLBACK]** with function address + decompile-excerpt reasoning, a per-finding confidence table, a located-functions table (name, address, one-line role), and a "deviations we take" section (mirroring `production.md`/`ai-team.md`).

**Questions the doc MUST answer** (spec §1 items 1–6):

1. **Capacity accounting:** is the load gate a passenger COUNT (`Passengers=N` bodies), a Size-sum against the `Passengers` pool (each passenger consumes `Size` slots), or both? The exact `Passengers`/`Size`/`SizeLimit` semantics, which value gates a load, and the DEFAULTS when a key is absent. (The rulesmd VALUES — `[HTK] Passengers=5 SizeLimit=2 Size=3`, `[FV] Passengers=1 SizeLimit=1 Size=3`, infantry `Size=1`, `DRON Size=2`, vehicles `Size=3` — are ground truth already; the hunt pins the FORMULA.)
2. **The "can X enter Y" predicate:** what marks a unit as a transport (`Passengers>0`?), passenger eligibility (infantry vs vehicles vs the `SizeLimit` test only), remaining-capacity check, ownership (same house only, or allies too), and the proximity requirement (same cell? adjacency?) — the full boolean chain, in evaluation order.
3. **Load mechanics:** the approach path target (the transport's own cell vs an adjacent cell), the exact trigger point where the passenger leaves the map (adjacent? on the transport's cell? a docking radius?), and the remove-from-map + occupancy-release order (which happens first). Our grid boards at Chebyshev adjacency — if the original requires walking ONTO the transport's cell, that lands as a documented deviation adjudicated at RE-GATE, not silently absorbed.
4. **Unload mechanics:** CONFIRM the eject cell selection is the `KickOutUnit`/`DAT_0089E5F8` 8-ring walk (or recover the actual routine); one-passenger-per-cell; the eject CADENCE (all at once, one per frame, or a timed delay — `[HTK] DeployTime=.022` is suspected to be this knob and must be pinned); facing on eject; and what happens when no free cell exists (HOLD vs order-abort vs partial unload).
5. **Transport-death passenger disposition:** do passengers die with the transport (expected default), always, or do `OpenTopped`/other flags change survival? Where do survivors (if any) get placed? CB-1 implements the recovered DEFAULT; exceptions are RE-documented and carried forward. The item-2/item-5 passes must also NOTE where the predicate/disposition touches `OpenTopped` and the `[General]` OpenTopped* knobs so a later phase recovers fire-out from a mapped starting point (spec Out-of-scope).
6. **Deterministic ordering:** the cargo container's order semantics — eject order relative to boarding order (FIFO vs LIFO), and any other sim-visible ordering the original relies on (e.g. which of several simultaneous boarders wins the last slot).

**Steps:**

- [ ] **Step 1: Re-read prior hunt method.** Read `docs/re/workflow.md` (the headless recipe), `docs/re/production.md` (§located-functions — `FUN_00443BC0` KickOutUnit + `DAT_0089E5F8` + `FUN_0047C430`, the entry map for the eject walk; and the doc format), and `docs/re/economy.md` §1 (the same 8-ring table shared with the harvester dig ring). Do NOT trust YRpp/Ares addresses; use them only as prose hints for what functions look like.

- [ ] **Step 2: Extract the real rulesmd.ini + record the ground-truth values.** Build the extractor if stale, then pull rulesmd.ini from the install mixes into the scratchpad (FOREGROUND, blocking):

```powershell
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/rulesmd.ini "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\rulesmd.ini"
```

If `localmd.mix/rulesmd.ini` misses, walk the same nested candidates `tests/test_rules.cpp`'s real-rulesmd case uses (`expandmd01..05.mix`, `expandmd99.mix`, `cachemd.mix`, `conqmd.mix`, then top level). Record IN SCRATCH NOTES, verbatim: the full `[HTK]`, `[FV]`, `[E1]`, `[DRON]` sections (Passengers/SizeLimit/Size/DeployTime/EnterTransportSound/LeaveTransportSound and any OpenTopped keys), plus `[General]`/`[AudioVisual]` EnterTransportSound/LeaveTransportSound defaults and the `[General]` OpenTopped* knobs. The committable findings (semantics, defaults, formula, cadence numbers) land in transport-cargo.md as prose + numbers; the raw ini stays in scratch.

- [ ] **Step 3: Anchor sweep (headless scripts, FOREGROUND, generous timeouts — do NOT background).** Per workflow.md's recipe (`JAVA_HOME="C:\Program Files\Java\jdk-21"`, `analyzeHeadless.bat D:\ra2-re <project> -process gamemd.exe -noanalysis -postScript <script>.py`), from the string data refs for `"Passengers"`/`"SizeLimit"`/`"Size"` locate the TechnoTypeClass INI loader offsets (the `FUN_0045FE40` string-ref→offset technique), then xref-walk the field reads to the consumers: (a) the **capacity/load gate** — where the Passengers pool and passenger Size are compared on an enter attempt (§1 item 1's formula) and the SizeLimit compare (§1 item 2); (b) the **enter predicate + handshake** — the full boolean chain (transport flag, eligibility, house/ally test, capacity) and the radio TRYENTER/proximity trigger point + the remove-from-map/occupancy-release order (§1 items 2–3); (c) the **unload path** — the Mission_Unload/deploy chain, whether it reaches `FUN_00443BC0`-style `DAT_0089E5F8` ring code or its own routine, the per-passenger cadence (find the `"DeployTime"` consult), eject facing, and the no-free-cell branch (§1 item 4); (d) the **transport-death sweep** — the destructor/death path that visits the passenger list, and any `OpenTopped` survival branch (§1 item 5); (e) the **container ordering** — the passenger list container shape and its insert/remove ends (FIFO vs LIFO), plus any lockstep-RNG consult (`FUN_007C4D00`/`FUN_0065C220`) in enter/eject ordering (§1 item 6); (f) the **HUD/audio references** — `"PipScale"` and Enter/LeaveTransportSound consults, RE-documented only (the pip row is reference look, not pixel-matched). Dump decompiled candidates to the scratchpad.

- [ ] **Step 4: Answer §1 items 1–6.** Iterate scripts until each item is CONFIRMED or PROBABLE with evidence (function address + decompile-excerpt reasoning in prose), or explicitly [RE-FALLBACK] with the adjudicated fallback stated (e.g. "eject cadence unpinnable → one per logic frame, Task 3's fallback"). Item 3's boarding trigger point must state plainly whether the original boards at adjacency or same-cell — this adjudicates the plan's Chebyshev-adjacency pre-hunt choice.

- [ ] **Step 5: Write `docs/re/transport-cargo.md`.** Sections §1–§6 mirroring the items above; the per-finding confidence table; the located-functions table; the "deviations we take" section (expected entries: Chebyshev-adjacency boarding vs the original's trigger point if same-cell; deterministic ascending-index boarding tie-break vs any lockstep-RNG order; one-eject-per-frame cadence if DeployTime resolves differently; sub-cell eject lean dropped at cell resolution — the production `ExitCoord` precedent; same-house-only loading while allies are unmodeled; no nested loaded transports — `can_load` rejects a passenger with non-empty cargo, data-unreachable with HTK `SizeLimit=2` < vehicle `Size=3`; passenger coords stale while carried).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add docs/re/transport-cargo.md
git -C "D:\ra2-android" commit -m "docs(re): transport-cargo hunt - Passengers/Size/SizeLimit semantics, enter predicate, load/unload mechanics, transport-death disposition, cargo ordering

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Expected: one commit, `docs/re/transport-cargo.md` created, no scripts/assets/ini dumps committed.

- [x] **Step 7 (CONTROLLER, not the implementer): RE-GATE review.** **(EXECUTED — this amendment; see STATUS: APPLIED (0cef283) above. The list below is the instruction as written pre-hunt; the adjudicated outcomes are folded into Tasks 2–6.)** Diff `transport-cargo.md` against every `[RE-GATE APPLIED]` block in Tasks 2–6; amend the plan; commit the amendment BEFORE dispatching Task 2. Specifically confirm/replace:
  - the **capacity formula + absent-key defaults** — Task 2's `can_load` slot arithmetic (`cargo_slots_used + p.size <= t.passengers_max`), the `SizeLimit` compare, and Task 4's `resolve_combat_unit` fallbacks (Passengers/SizeLimit absent → 0, Size absent → 1) [hunt §1]; **→ ADJUDICATED D1: capacity = COUNT (`t.cargo.size() < t.passengers_max`), size-sum DELETED, `cargo_slots_used` renamed `cargo_total_size` and demoted to pip-only; defaults kept;**
  - the **predicate chain + evaluation order** — Task 2's `can_load` member list (same-house-only, no-nested-cargo, carried-transport reject) and order [hunt §2]; **→ ADJUDICATED D6: same-house kept as a documented deviation (original = `IsAlliedWith`);**
  - the **boarding trigger point + remove order** — Task 3's Chebyshev `<= 1` gate in `step_cargo` and its release-occupancy-then-push sequence; if the original is same-cell and load-bearing, adjudicate (adjacency is the expected accepted deviation — same reachable set, deterministic) [hunt §3]; **→ ADJUDICATED D7: original is same-cell; adjacency kept as the documented deviation;**
  - the **eject cadence, facing, and HOLD** — Task 3's one-per-frame eject, `facing_from_delta(transport→cell)` eject facing, and the HOLD-and-retry branch; replace the cadence with the recovered `DeployTime` rule if pinned [hunt §4]; **→ ADJUDICATED D3/D4: eject cell = facing±90° scatter (`eject_cell`; ring extraction DROPPED, `factory.cpp` untouched); one-per-tick + HOLD confirmed; DeployTime start delay NOT implemented (unpinned — carry-forward);**
  - the **transport-death disposition** — Task 3's cargo-dies-with-transport tombstone (and where survivors would be placed if the recovered default differs); OpenTopped exceptions stay RE-documented carry-forwards [hunt §5]; **→ ADJUDICATED D5: plan OK, confirmed — no survival branch;**
  - the **ordering rules** — Task 2's `next_eject` FIFO head and Task 3's ascending-index boarding tie-break [hunt §6]; **→ ADJUDICATED D2: LIFO (`cargo.back()`); ascending-index tie-break kept (no RNG in the original);**
  - Task 5's demo expectations (exact-fill 5/5 + 1 refusal under the recovered formula; the FIFO eject witness) and Task 6's README deviations list (mirror the hunt doc's "deviations we take"). **→ ADJUDICATED: exact-fill is COUNT-based (`count=K/5` witness); the eject witness/wedge is LIFO (reverse boarding); README deviations rewritten to the hunt list.**

---

### Task 2: engine/cargo — the pure cargo module (purely additive; NO factory.cpp edit)

**Files:**
- Create: `engine/cargo.hpp`, `engine/cargo.cpp`, `tests/test_cargo.cpp`
- Modify: `engine/world.hpp` (inert Unit cargo fields), `cmake/ra2_sources.cmake`, `CMakeLists.txt`
- NOT touched: `engine/factory.cpp` [RE-GATE APPLIED: transport-cargo.md §4 — the original unload does NOT reuse the KickOutUnit/`DAT_0089E5F8` ring (it is `BuildingClass::KickOutUnit`-only); the pre-hunt `first_free_ring_cell` extraction + `exit_cell` delegator are DROPPED, keeping the 8 existing gates entirely outside this phase's blast radius]

**Interfaces:**
- Consumes: `Unit` (world.hpp — grown below), `Cellxy` (pathfind.hpp), `facing_u16_from_delta` (world.hpp — the engine's ONE 16-bit facing convention, docs/re/voxel-render.md §2; `eject_cell`'s 8-way quantization REUSES it rather than inventing a second convention).
- Produces — `engine/cargo.hpp`, complete:

```cpp
#pragma once
// Pure CB-1 transport/cargo logic [RE: docs/re/transport-cargo.md]: the
// COUNT capacity gate + per-passenger size cap (can_load), the pip-only
// total-size metric (cargo_total_size -- the original GetTotalSize), the
// LIFO eject rule (next_eject), and the facing+/-90-degree scatter
// eject-cell picker (eject_cell) recovered from the original unload
// (FUN_00415C60) [RE-GATE APPLIED: transport-cargo.md §4 -- the KickOutUnit
// DAT_0089E5F8 8-ring is NOT reused by unload; factory.cpp stays untouched].
// Layered on Unit exactly like harvest.hpp (world.hpp defines Unit; this
// module includes world.hpp; world.hpp does NOT include this back). No
// SDL/OS.
#include <functional>
#include <vector>
#include "pathfind.hpp"  // Cellxy
#include "world.hpp"     // Unit, facing_u16_from_delta

namespace ra2engine {

// DISPLAY-ONLY sum of units[i].size over the transport's cargo list -- the
// original's GetTotalSize (FUN_00473390), whose only callers are pip draws
// [RE-GATE APPLIED: transport-cargo.md §1 -- pip-only; NEVER consulted by
// the capacity gate]. DERIVED every call -- no stored counter to drift out
// of sync [spec §2]. Out-of-range entries are ignored defensively (cargo
// holds unit INDICES, stable for a World's lifetime because units are
// tombstoned, never erased -- the same invariant `target` relies on).
int cargo_total_size(const std::vector<Unit>& units, const Unit& transport);

// The "can X enter Y" predicate over plain Unit fields [RE-GATE APPLIED:
// transport-cargo.md §2 -- recovered chain (What_Action -> ReceiveMessage
// 0xF): transport flag, ownership, capacity COUNT, per-passenger size]:
//   1. both indices in range and distinct (not self),
//   2. transport is a transport (passengers_max > 0) [§2 CONFIRMED],
//   3. transport alive and non-dying, passenger alive and non-dying,
//   4. same house [RE-GATE APPLIED: transport-cargo.md §2 -- documented
//      deviation: the original gate is IsAlliedWith (FUN_004F98B0: same
//      house OR same side OR ally bitmask); allies are unmodeled in CB-1
//      (one house per side) -- this clause is where allied loading would
//      attach],
//   5. passenger not already carried (cargo_of < 0),
//   6. passenger's own cargo empty (no nested loaded transports -- spec
//      Out-of-scope; data-unreachable with HTK SizeLimit=2 < vehicle Size=3),
//   7. transport not itself carried (cargo_of < 0),
//   8. passenger.size <= transport.size_limit [RE-GATE APPLIED:
//      transport-cargo.md §1 -- a PER-PASSENGER cap (Size <= SizeLimit),
//      not a pool],
//   9. COUNT capacity: (int)transport.cargo.size() < transport.passengers_max
//      [RE-GATE APPLIED: transport-cargo.md §1 -- NumPassengers < Passengers
//      (FUN_004190B0 case 0xF/0xE); each body is ONE slot regardless of
//      Size -- the pre-hunt size-sum is DELETED].
bool can_load(const std::vector<Unit>& units, int transport_idx, int passenger_idx);

// The next passenger to eject: the LAST cargo entry (LIFO -- the original
// container is a head-pushed singly-linked stack and unload pops the head,
// i.e. the NEWEST passenger) [RE-GATE APPLIED: transport-cargo.md §6 --
// CONFIRMED]. Our cargo vector push_backs on board, so the stack head is
// the BACK. -1 when the cargo list is empty.
int next_eject(const Unit& transport);

// The eject-cell picker for one unload step [RE-GATE APPLIED:
// transport-cargo.md §4 -- CONFIRMED shape: the original eject
// (FUN_00415C60) does NOT walk the KickOutUnit ring; it rotates the
// transport's facing +/- a quarter turn (0x3FFF BAM ~ 90 deg;
// cell-equivalent 0x4000), ALTERNATING the side each eject via a counter's
// low bit, and places the passenger one step out]. transport_facing_u16 is
// the vehicle body_facing in the engine's ONE 16-bit facing space
// (voxel-render.md §2); the 8-way step is chosen by REUSING
// facing_u16_from_delta -- no second convention. If the scatter cell fails
// `free`, falls back to the first free cell of the immediate neighbourhood
// (Chebyshev r=1 then r=2, row-major within each ring -- documented
// deviation: FUN_00481090's exact fine placement is unpinned; behaviourally
// equivalent = first free cell near the transport). Returns {-1,-1} when
// fully boxed in (caller HOLDs: put-back/retry [§4 CONFIRMED]).
Cellxy eject_cell(int tx, int ty, double transport_facing_u16, int eject_counter,
                  const std::function<bool(int, int)>& free);

}  // namespace ra2engine
```

- Produces — `engine/cargo.cpp`, complete:

```cpp
#include "cargo.hpp"

#include <algorithm>
#include <cmath>

namespace ra2engine {

int cargo_total_size(const std::vector<Unit>& units, const Unit& transport) {
    int total = 0;
    for (int p : transport.cargo)
        if (p >= 0 && p < static_cast<int>(units.size())) total += units[p].size;
    return total;
}

bool can_load(const std::vector<Unit>& units, int transport_idx, int passenger_idx) {
    // [RE-GATE APPLIED: transport-cargo.md §2 -- recovered boolean chain;
    // see the header comment for the numbered members.]
    const int n = static_cast<int>(units.size());
    if (transport_idx < 0 || transport_idx >= n) return false;
    if (passenger_idx < 0 || passenger_idx >= n) return false;
    if (transport_idx == passenger_idx) return false;              // not self
    const Unit& t = units[transport_idx];
    const Unit& p = units[passenger_idx];
    if (t.passengers_max <= 0) return false;                       // not a transport
    if (!t.alive || t.die_time >= 0.0) return false;               // transport live
    if (!p.alive || p.die_time >= 0.0) return false;               // passenger live
    if (t.house != p.house) return false;      // same house only [RE-GATE APPLIED:
                                               // transport-cargo.md §2 -- documented
                                               // deviation: the original gate is
                                               // IsAlliedWith (FUN_004F98B0); allies
                                               // are unmodeled in CB-1 -- allied
                                               // loading would attach HERE]
    if (p.cargo_of >= 0) return false;                             // already carried
    if (!p.cargo.empty()) return false;        // no nested loaded transports
                                               // [spec Out-of-scope]
    if (t.cargo_of >= 0) return false;                             // carried transports
                                               // admit nobody
    if (p.size > t.size_limit) return false;   // PER-PASSENGER cap: Size <=
                                               // SizeLimit [RE-GATE APPLIED:
                                               // transport-cargo.md §1]
    // COUNT capacity gate [RE-GATE APPLIED: transport-cargo.md §1 --
    // NumPassengers < Passengers (FUN_004190B0 case 0xF/0xE): each body is
    // ONE slot regardless of Size; Size feeds only the pip display].
    return static_cast<int>(t.cargo.size()) < t.passengers_max;
}

int next_eject(const Unit& transport) {
    // LIFO: the LAST-boarded passenger leaves first (the original is a
    // head-pushed stack whose unload pops the head, i.e. the newest)
    // [RE-GATE APPLIED: transport-cargo.md §6 -- CONFIRMED]. Our boarding
    // push_backs, so the stack head is the BACK.
    return transport.cargo.empty() ? -1 : transport.cargo.back();
}

Cellxy eject_cell(int tx, int ty, double transport_facing_u16, int eject_counter,
                  const std::function<bool(int, int)>& free) {
    // Scatter facing = the transport's facing rotated a quarter turn, the
    // side alternating with the eject counter's low bit [RE-GATE APPLIED:
    // transport-cargo.md §4 -- facing +/- 0x3FFF (~90 deg BAM), cell-
    // equivalent 0x4000; alternation via (counter & 1). Which side is
    // "even" is an arbitrary-but-fixed choice inside the recovered
    // alternation -- deterministic].
    const double side = (eject_counter & 1) ? -16384.0 : 16384.0;
    const double scatter =
        std::fmod(transport_facing_u16 + side + 65536.0, 65536.0);
    // 8-way quantization REUSES facing_u16_from_delta (the engine's one
    // facing convention -- voxel-render.md §2) instead of a second
    // hand-built mapping: pick the neighbour step whose facing is angularly
    // closest to the scatter facing (ties -> lowest index; deterministic).
    static const int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int best = 0;
    double best_d = 65536.0;
    for (int k = 0; k < 8; ++k) {
        double d = std::fabs(facing_u16_from_delta(kDx[k], kDy[k]) - scatter);
        if (d > 32768.0) d = 65536.0 - d;
        if (d < best_d) {
            best_d = d;
            best = k;
        }
    }
    const int px = tx + kDx[best], py = ty + kDy[best];
    if (free(px, py)) return Cellxy{px, py};
    // Primary blocked: nearest-free-cell fallback over the immediate
    // neighbourhood, Chebyshev r = 1 then r = 2, row-major within each ring
    // [RE-GATE APPLIED: transport-cargo.md §4 -- documented deviation:
    // FUN_00481090's exact fine placement is unpinned; behaviourally
    // equivalent = first free cell near the transport].
    for (int r = 1; r <= 2; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy) != r) continue;
                if (free(tx + dx, ty + dy)) return Cellxy{tx + dx, ty + dy};
            }
    return Cellxy{-1, -1};   // fully boxed in -> caller HOLDs (put-back/retry)
}

}  // namespace ra2engine
```

- Produces — `engine/factory.cpp`: **NOTHING.** [RE-GATE APPLIED: transport-cargo.md §4 — the hunt CONFIRMED the original unload ejects via its own facing-scatter routine (`FUN_00415C60`), NOT the `KickOutUnit`/`DAT_0089E5F8` ring; the pre-hunt extraction/delegator is DROPPED and `exit_cell` keeps its current body. The existing `tests/test_factory.cpp` suite is untouched and stays green as-is.]

- Produces — `engine/world.hpp`: `struct Unit` gains this block at its tail (immediately after the `manual_hold` economy field):

```cpp
    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---------------------
    // ALL defaults INERT (spec §8): no existing demo issues a load/unload
    // order and no existing spawn path sets passengers_max > 0, so every
    // field below holds its default in every existing demo; world_hash reads
    // only cell/hp/alive and is untouched. cargo/cargo_of/enter_target store
    // unit INDICES -- stable for a World's lifetime (units are tombstoned,
    // never erased; the same invariant `target` relies on).
    int passengers_max = 0;      // capacity COUNT (Rules Passengers= bodies);
                                 // 0 = not a transport [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- NumPassengers <
                                 // Passengers, a body count, NOT a Size pool]
    int size_limit = 0;          // largest admissible passenger Size
                                 // (SizeLimit=) -- a PER-PASSENGER cap [§1]
    int size = 1;                // own bulk (Size=); feeds ONLY the pip display
                                 // (cargo_total_size) [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- absent-key defaults
                                 // CONFIRMED: Passengers/SizeLimit -> 0,
                                 // Size -> 1; Task 4 resolves real values]
    std::vector<int> cargo;      // carried unit indices in boarding order;
                                 // ejects pop the BACK (LIFO [RE-GATE APPLIED:
                                 // transport-cargo.md §6])
    int cargo_of = -1;           // >=0: index of the transport carrying this
                                 // unit -- the unit is OFF-MAP (spec §4)
    int enter_target = -1;       // pending enter order -> transport index
    bool unload_pending = false; // pending unload order on this transport
    int eject_counter = 0;       // eject_cell scatter-side alternator (+/-90 deg
                                 // by low bit), bumped once per eject
                                 // [transport-cargo.md §4]
```

- CMake registration (BOTH builds — the global constraint):
  - `cmake/ra2_sources.cmake`: append `    ${RA2_ROOT}/engine/cargo.cpp` to `RA2_ENGINE_SOURCES`, immediately after the `${RA2_ROOT}/engine/factory.cpp` line.
  - `CMakeLists.txt`: after the `target_sources(ra2assets_tests PRIVATE tests/test_factory.cpp engine/factory.cpp)` block, add:

```cmake
# cargo.cpp is pure C++17 (no SDL): CB-1 transport/cargo -- the COUNT
# capacity can_load predicate, LIFO next_eject, the facing-scatter eject_cell
# picker, and the pip-only cargo_total_size metric
# (docs/re/transport-cargo.md). Compiled into both this test binary and
# ra2engine from one source of truth (same pattern as factory.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_cargo.cpp engine/cargo.cpp)
```

**Steps:**

- [ ] **Step 1: Land the pure module.** Create `engine/cargo.hpp` and `engine/cargo.cpp` exactly as in Interfaces; create `tests/test_cargo.cpp` containing ONLY the include stub (filled in Step 5):

```cpp
#include "doctest.h"
#include "../engine/cargo.hpp"
using namespace ra2engine;
// CB-1 pure cargo tests land in Task 2 Step 5.
```

Add the Unit cargo-field block to `engine/world.hpp` and register both cmake entries. `engine/factory.cpp` is NOT touched [RE-GATE APPLIED: transport-cargo.md §4 — the exit_cell ring extraction was DROPPED; unload has its own recovered scatter routine]. NOTE: `can_load`/`cargo_total_size`/`next_eject`/`eject_cell` are fully implemented here (one TU); their TDD cycle is Steps 5–7 — acceptable because the new predicates' tests are written and green before their first CONSUMER (Task 3) ever runs.

- [ ] **Step 2: Build + full suite FOREGROUND, blocking (timeout 600000 ms):**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: build OK (the new TU compiles standalone); the FULL existing suite passes 0 failed unchanged — the module is purely additive (a new TU + trailing defaulted `Unit` fields; nothing existing is edited).

- [ ] **Step 3: 8-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; PowerShell, not Git Bash; do NOT pipe through Select-Object).** Task 2 changes no live path — but the 8-gate run still lands HERE to pin the additive commit green (spec §8; the RE-GATE amendment dropped the exit_cell refactor, so there is no extraction to isolate — this run simply proves the new TU + inert fields drift nothing):

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
```

Expected, all eight pins EXACT: `deterministic=yes logs=78`; `deterministic=yes logs=144`; `hash=5273b5e71ca009e1`; `econ_hash=7fed96cb98b68017`; `prod_hash=f0f4e1207ecd713c`; `ai_base_hash=cd1f7c6a29933f0a`; `ai_team_hash=312e92ed862a5e74`; `ai_trigger_hash=8334a6ca1531cabc`. ANY drift = defect in THIS task; fix before proceeding.

- [ ] **Step 4: Commit the additive module.**

```powershell
git -C "D:\ra2-android" add engine/cargo.hpp engine/cargo.cpp engine/world.hpp tests/test_cargo.cpp cmake/ra2_sources.cmake CMakeLists.txt
git -C "D:\ra2-android" commit -m "feat(engine): pure cargo module - COUNT-capacity can_load, LIFO next_eject, facing-scatter eject_cell, pip-only cargo_total_size (additive; 8 gates re-verified)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

- [ ] **Step 5: Write the pure cargo tests.** Replace `tests/test_cargo.cpp`'s stub body with (complete file):

```cpp
#include "doctest.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <vector>

#include "../engine/cargo.hpp"
using namespace ra2engine;

namespace {
// Hand-built units -- no map/World/install needed (spec §9: capacity>1 and
// Size>1 are covered HOST-ONLY regardless of which transport art the install
// fields).
Unit cargo_unit(int house = 0, int size = 1) {
    Unit u;
    u.house = house;
    u.size = size;
    return u;   // alive=true, cargo_of=-1, cargo empty -- the inert defaults
}
Unit transport_unit(int passengers, int size_limit, int house = 0) {
    Unit u = cargo_unit(house, 3);      // vehicles are Size=3 (rulesmd ground truth)
    u.passengers_max = passengers;
    u.size_limit = size_limit;
    return u;
}
}  // namespace

TEST_CASE("cargo: total_size is the pip-only Size sum, DERIVED from the cargo list") {
    // [RE-GATE APPLIED: transport-cargo.md §1 -- GetTotalSize is DISPLAY-
    // only; capacity is the COUNT, pinned in the next case.]
    std::vector<Unit> us;
    us.push_back(transport_unit(5, 2));          // [0] HTK-shaped: Passengers=5, SizeLimit=2
    us.push_back(cargo_unit(0, 1));              // [1] GI-shaped (Size=1)
    us.push_back(cargo_unit(0, 2));              // [2] DRON-shaped (Size=2)
    CHECK(cargo_total_size(us, us[0]) == 0);
    us[0].cargo = {1};
    CHECK(cargo_total_size(us, us[0]) == 1);
    us[0].cargo = {1, 2};
    CHECK(cargo_total_size(us, us[0]) == 3);     // 1 + 2: the Size=2 DRON shows
                                                 // TWO pips while consuming ONE
                                                 // count slot [RE-GATE APPLIED:
                                                 // transport-cargo.md §1]
    CHECK(static_cast<int>(us[0].cargo.size()) == 2);   // ...the COUNT the gate uses
    us[0].cargo = {1, 2, 99};                    // out-of-range entry ignored
    CHECK(cargo_total_size(us, us[0]) == 3);
}

TEST_CASE("cargo: COUNT fill to exact capacity; Size never feeds the gate; SizeLimit refusal") {
    // Exact fill is COUNT-based [RE-GATE APPLIED: transport-cargo.md §1 --
    // NumPassengers < Passengers]: 5 Size-1 GIs fill Passengers=5, the 6th
    // is refused.
    std::vector<Unit> us;
    us.push_back(transport_unit(5, 2));                          // [0]
    for (int i = 0; i < 6; ++i) us.push_back(cargo_unit(0, 1));  // [1..6]
    for (int i = 1; i <= 5; ++i) {               // exact fill: 5 Size-1 riders
        CHECK(can_load(us, 0, i));
        us[0].cargo.push_back(i);
        us[i].cargo_of = 0;
    }
    CHECK(static_cast<int>(us[0].cargo.size()) == 5);   // the exact-fill boundary
    CHECK_FALSE(can_load(us, 0, 6));             // the 6th is REFUSED at capacity
    // COUNT vs size-sum discriminator [RE-GATE APPLIED: transport-cargo.md
    // §1 -- the DELETED pre-hunt size-sum would refuse here; the recovered
    // COUNT admits]: four Size-2 DRON-shaped riders occupy FOUR count slots
    // (their size-sum is already 8 > Passengers=5), and a FIFTH rider still
    // boards; a SIXTH is refused by the count alone.
    std::vector<Unit> vs;
    vs.push_back(transport_unit(5, 2));                          // [0]
    for (int i = 0; i < 4; ++i) vs.push_back(cargo_unit(0, 2));  // [1..4] DRON-shaped
    vs.push_back(cargo_unit(0, 1));                              // [5] GI-shaped
    vs.push_back(cargo_unit(0, 1));                              // [6] GI-shaped
    for (int i = 1; i <= 4; ++i) {
        CHECK(can_load(vs, 0, i));
        vs[0].cargo.push_back(i);
        vs[i].cargo_of = 0;
    }
    CHECK(cargo_total_size(vs, vs[0]) == 8);     // pip metric exceeds Passengers --
                                                 // display-only, NOT the gate
    CHECK(can_load(vs, 0, 5));                   // count 4 < 5: the 5th boards
    vs[0].cargo.push_back(5);
    vs[5].cargo_of = 0;
    CHECK_FALSE(can_load(vs, 0, 6));             // count 5 = Passengers: full
    // SizeLimit gates the LARGEST admissible passenger independently of room
    // [RE-GATE APPLIED: transport-cargo.md §1 -- PER-PASSENGER Size <=
    // SizeLimit]: a Size=3 vehicle is rejected by SizeLimit=2 (HTK-shaped),
    // while a Size=2 DRON is admitted.
    std::vector<Unit> ws;
    ws.push_back(transport_unit(5, 2));          // [0] SizeLimit=2 (HTK-shaped)
    ws.push_back(cargo_unit(0, 3));              // [1] a Size=3 vehicle-shaped passenger
    ws.push_back(cargo_unit(0, 2));              // [2] a Size=2 DRON-shaped passenger
    CHECK_FALSE(can_load(ws, 0, 1));             // Size 3 > SizeLimit 2
    CHECK(can_load(ws, 0, 2));                   // Size 2 <= SizeLimit 2
}

TEST_CASE("cargo: the can_load matrix rejects every ineligible pairing") {
    std::vector<Unit> us;
    us.push_back(transport_unit(2, 2));          // [0] the transport
    us.push_back(cargo_unit(0, 1));              // [1] eligible baseline
    CHECK(can_load(us, 0, 1));
    CHECK_FALSE(can_load(us, 1, 0));             // not-a-transport (passengers_max=0)
    CHECK_FALSE(can_load(us, 0, 0));             // self
    us.push_back(cargo_unit(1, 1));              // [2] wrong house
    CHECK_FALSE(can_load(us, 0, 2));
    us.push_back(cargo_unit(0, 1));              // [3] the mutation probe
    us[0].die_time = 0.5;                        // dying transport
    CHECK_FALSE(can_load(us, 0, 3));
    us[0].die_time = -1.0;
    us[0].alive = false;                         // dead transport
    CHECK_FALSE(can_load(us, 0, 3));
    us[0].alive = true;
    us[3].die_time = 0.5;                        // dying passenger
    CHECK_FALSE(can_load(us, 0, 3));
    us[3].die_time = -1.0;
    us[3].alive = false;                         // dead passenger
    CHECK_FALSE(can_load(us, 0, 3));
    us[3].alive = true;
    CHECK(can_load(us, 0, 3));                   // probe restored -> eligible again
    us[3].cargo_of = 0;                          // already carried
    CHECK_FALSE(can_load(us, 0, 3));
    us[3].cargo_of = -1;
    us[3].cargo = {1};                           // passenger carrying cargo itself
    CHECK_FALSE(can_load(us, 0, 3));             // (no nested loaded transports)
    us[3].cargo.clear();
    us[0].cargo_of = 1;                          // the TRANSPORT is carried
    CHECK_FALSE(can_load(us, 0, 3));
    us[0].cargo_of = -1;
    CHECK_FALSE(can_load(us, -1, 1));            // out-of-range transport
    CHECK_FALSE(can_load(us, 0, 99));            // out-of-range passenger
}

TEST_CASE("cargo: eject_cell picks the facing +/-90-deg scatter cell, alternating by counter parity") {
    // Facing space = the engine's ONE convention (facing_u16_from_delta,
    // voxel-render.md §2): 0 = North (0,-1), 0x4000 = East (+1,0),
    // 0x8000 = South (0,+1), 0xC000 = West (-1,0), clockwise [RE-GATE
    // APPLIED: transport-cargo.md §4 -- facing +/- 0x4000 scatter,
    // alternating via (counter & 1)].
    auto all_free = [](int, int) { return true; };
    // Transport facing North (0.0): counter 0 -> +0x4000 = East -> (11,10);
    // counter 1 -> -0x4000 = West -> (9,10).
    const Cellxy c0 = eject_cell(10, 10, 0.0, 0, all_free);
    CHECK(c0.x == 11);
    CHECK(c0.y == 10);
    const Cellxy c1 = eject_cell(10, 10, 0.0, 1, all_free);
    CHECK(c1.x == 9);
    CHECK(c1.y == 10);
    // Spawn-default facing South (32768.0): the sides flip -- counter 0 ->
    // West, counter 1 -> East.
    const Cellxy s0 = eject_cell(10, 10, 32768.0, 0, all_free);
    CHECK(s0.x == 9);
    CHECK(s0.y == 10);
    const Cellxy s1 = eject_cell(10, 10, 32768.0, 1, all_free);
    CHECK(s1.x == 11);
    CHECK(s1.y == 10);
    // Deterministic: identical inputs, identical answer.
    const Cellxy again = eject_cell(10, 10, 0.0, 0, all_free);
    CHECK(again.x == c0.x);
    CHECK(again.y == c0.y);
}

TEST_CASE("cargo: eject_cell falls back to the nearest free neighbour and signals HOLD when boxed") {
    // Primary (East of a North-facing transport at counter 0) blocked -> the
    // r=1 row-major fallback finds (9,9) first (dy=-1 row, dx ascending)
    // [RE-GATE APPLIED: transport-cargo.md §4 -- documented deviation:
    // FUN_00481090's fine placement is unpinned; first free nearby cell].
    auto blocked_primary = [](int x, int y) { return !(x == 11 && y == 10); };
    const Cellxy f = eject_cell(10, 10, 0.0, 0, blocked_primary);
    CHECK(f.x == 9);
    CHECK(f.y == 9);
    // r=1 fully blocked -> the r=2 perimeter row-major scan -> (8,8).
    auto r1_blocked = [](int x, int y) {
        return std::max(std::abs(x - 10), std::abs(y - 10)) >= 2;
    };
    const Cellxy f2 = eject_cell(10, 10, 0.0, 0, r1_blocked);
    CHECK(f2.x == 8);
    CHECK(f2.y == 8);
    // Fully boxed -> {-1,-1} (the unload HOLD signal: put-back/retry [§4]).
    const Cellxy none = eject_cell(10, 10, 0.0, 0, [](int, int) { return false; });
    CHECK(none.x == -1);
    CHECK(none.y == -1);
}

TEST_CASE("cargo: next_eject is LIFO -- the LAST-boarded passenger leaves first") {
    Unit t = transport_unit(5, 2);
    CHECK(next_eject(t) == -1);                  // empty -> nothing to eject
    t.cargo = {11, 12, 13, 14, 15};              // boarded A,B,C,D,E
    std::vector<int> out;
    while (next_eject(t) != -1) {                // drain the way step_cargo does
        out.push_back(next_eject(t));
        t.cargo.pop_back();
    }
    CHECK(out == std::vector<int>{15, 14, 13, 12, 11});   // E,D,C,B,A [RE-GATE
                                                 // APPLIED: transport-cargo.md
                                                 // §6 -- LIFO CONFIRMED]
}
```

- [ ] **Step 6: Run the new tests FOREGROUND, blocking (timeout 600000 ms), expected PASS** (the implementations already landed in Step 1; a failure here is a real predicate bug):

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="cargo:*"
```

Expected: all `cargo:` cases PASS, 0 failed. If any fails, fix `engine/cargo.cpp` (never the RE-cited semantics) and re-run.

- [ ] **Step 7: Full suite + commit.** FOREGROUND, blocking:

```powershell
D:\ra2-android\build\ra2assets_tests.exe
git -C "D:\ra2-android" add tests/test_cargo.cpp
git -C "D:\ra2-android" commit -m "test(engine): cargo pure doctests - COUNT capacity boundaries, LIFO eject order, eject_cell scatter/fallback/HOLD

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Expected: 0 failed; one commit.

---

### Task 3: World integration — enter/unload orders, step_cargo, carry guards, transport death

**Files:**
- Modify: `engine/world.hpp`, `engine/world.cpp`, `engine/unit_combat.hpp`, `engine/combat.cpp`, `engine/harvest.cpp`
- Test: `tests/test_world.cpp` (append)
- CMake: **none** — every touched TU is already registered in both builds.

**Interfaces:**
- Consumes: `can_load` / `cargo_total_size` / `next_eject` / `eject_cell` (Task 2, `engine/cargo.hpp` — `world.cpp` gains `#include "cargo.hpp"`), `assign_formation` + `nearest_free_cells` (group_move.hpp, already included), `facing_from_delta` / `facing_u16_from_delta` (world.cpp), `Occupancy::release_all_of/set/is_free/at` (occupancy.hpp), the Task 2 Unit cargo fields (incl. `body_facing` + `eject_counter` feeding `eject_cell`).
- Produces — `engine/unit_combat.hpp`: `struct UnitCombat` gains this block at its tail (AFTER `is_vehicle` — existing aggregate initializers like `tests/test_input.cpp`'s `gi_combat()` keep compiling because the new fields trail with defaults):

```cpp
    // --- CB-1 Transports/Cargo [RE: transport-cargo.md §1] ---
    // Resolved once from Rules by main.cpp like every existing stat; ALL
    // defaults inert (0 passengers = not a transport). Copied onto the Unit
    // at add_combat_unit.
    int passengers = 0;        // Rules Passengers= (capacity COUNT [RE-GATE
                               // APPLIED: transport-cargo.md §1])
    int size_limit = 0;        // Rules SizeLimit= (largest admissible Size --
                               // a per-passenger cap)
    int size = 1;              // Rules Size= (own bulk; pip weight only)
                               // [RE-GATE APPLIED: transport-cargo.md §1 --
                               // absent-key default 1 CONFIRMED]
```

- Produces — `engine/world.hpp` additions:

After the `Unit` struct definition (before `struct SeqEntry`):

```cpp
// One cargo event (board / capacity-refusal / eject), drained by callers per
// frame via World::take_cargo_events -- the take_harvest_events pattern:
// main.cpp turns these into demo witness lines and the guarded Enter/
// LeaveTransportSound hook [RE: transport-cargo.md; spec §6/§7]. Defined
// HERE (not cargo.hpp) so World can store a vector without an include cycle.
struct CargoEvent {
    enum class Kind { Board, Refuse, Eject };
    Kind kind = Kind::Board;
    int unit = -1;       // the passenger
    int transport = -1;  // the transport
    // Board/Refuse: x = the passenger COUNT after the event resolved
    // (capacity is a COUNT [RE-GATE APPLIED: transport-cargo.md §1]), y =
    // the transport's passengers_max (the "K/5" witness pair). Eject:
    // (x, y) = the eject cell.
    int x = 0, y = 0;
};
```

In `class World`'s public section, after `factory_category_lost`:

```cpp
    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Enter order: every SELECTED house-0, alive, non-dying unit that passes
    // can_load NOW (capacity is re-checked at boarding -- orders may
    // oversubscribe) gets enter_target = t_idx, drops any engagement, and
    // paths toward the transport through assign_formation around its cell
    // (a raw move to the transport's own occupied cell would cancel as
    // unreachable) [spec §3]. No-op if t_idx is not an own, alive, non-dying
    // transport. Sets the cargo_any_ firewall.
    void order_enter_transport(int t_idx);
    // Unload order: sets unload_pending on every SELECTED house-0, alive,
    // non-dying transport with non-empty cargo [spec §5]. Any new move/
    // attack order on the transport clears unload_pending (an order
    // supersedes -- our order-model convention, not overturned by the hunt).
    // Sets the cargo_any_ firewall.
    void order_unload_selected();
    // Drain the board/refuse/eject events accumulated by step_cargo since
    // the last call (the take_harvest_events pattern). Empty if no cargo
    // activity ever happened.
    std::vector<CargoEvent> take_cargo_events();
```

In `class World`'s private section, after `ai_fill_trigger_view`:

```cpp
    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Boarding / carry / eject / transport-death, called between step_units
    // and step_turrets (a pure insertion -- spec §8). Early-returns unless
    // cargo_any_ (the firewall: false until the first enter/unload order).
    void step_cargo(double dt);
    bool cargo_any_ = false;
    std::vector<CargoEvent> cargo_events_;
```

- Produces — `engine/world.cpp` changes (each site complete):

(1) Include, at the top with the other engine includes:

```cpp
#include "cargo.hpp"   // can_load/cargo_total_size/next_eject/eject_cell (CB-1)
```

(2) `add_combat_unit` — append before `return u;`:

```cpp
    // CB-1 [RE: transport-cargo.md §1]: cargo stats, inert unless the caller
    // resolved them (default passengers=0 = not a transport).
    u.passengers_max = c.passengers;
    u.size_limit = c.size_limit;
    u.size = c.size;
```

(3) `order_move_selected` — in the per-selected-unit loop, right after `u.attack_order = false;`:

```cpp
        // CB-1: a new move order supersedes a pending enter/unload
        // [RE-GATE APPLIED: transport-cargo.md §3/§4 -- kept: our
        // order-model convention; the hunt pinned no contrary rule].
        // Provably no-op in every existing demo (fields hold their inert
        // defaults).
        u.enter_target = -1;
        u.unload_pending = false;
```

(4) `commit_attack_target` — after `u.attack_order = true;` (shared by the player AND AI attack paths; writes -1/false over the inert defaults everywhere cargo is unused, so byte-identity holds):

```cpp
    // CB-1: an attack order supersedes a pending enter/unload [RE-GATE
    // APPLIED: transport-cargo.md §3/§4 -- kept: our order-model
    // convention; the hunt pinned no contrary rule].
    u.enter_target = -1;
    u.unload_pending = false;
```

(5) `order_harvest` — after `u.manual_hold = false;`:

```cpp
        u.enter_target = -1;  // CB-1: a harvest order supersedes an enter order
```

(6) The two orders + the event drain + `step_cargo`, added after `factory_category_lost`'s definition:

```cpp
// --- CB-1 Transports/Cargo [RE: transport-cargo.md] --------------------------

void World::order_enter_transport(int t_idx) {
    if (t_idx < 0 || t_idx >= static_cast<int>(units_.size())) return;
    const Unit& t = units_[t_idx];
    if (!t.alive || t.die_time >= 0.0 || t.house != 0 || t.passengers_max <= 0) return;
    // Gather eligible movers: selected house-0, alive, non-dying, not the
    // transport itself, passing can_load NOW (capacity re-checked at
    // boarding -- orders may oversubscribe) [spec §3].
    std::vector<int> movers;
    std::vector<Cellxy> cur;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        if (static_cast<int>(i) == t_idx) continue;
        if (!can_load(units_, t_idx, static_cast<int>(i))) continue;
        movers.push_back(static_cast<int>(i));
        cur.push_back({units_[i].cell_x, units_[i].cell_y});
    }
    if (movers.empty()) return;
    cargo_any_ = true;   // the step_cargo firewall opens on the FIRST order (spec §8)
    // Batch per transport into assign_formation around the transport's cell,
    // so each passenger gets a DISTINCT free nearby cell (the same batching
    // step() uses for combat approach requests) [spec §3].
    const std::vector<Cellxy> targets =
        assign_formation({t.cell_x, t.cell_y}, cur, passable_, grid_w_, grid_h_, occ_);
    for (std::size_t k = 0; k < movers.size(); ++k) {
        Unit& u = units_[movers[k]];
        u.target = -1;               // an enter order supersedes engagement
        u.target_building = -1;
        u.attack_order = false;
        u.unload_pending = false;    // ...and any pending unload on the mover
        u.enter_target = t_idx;
        // A harvester passenger parks its mission FSM for the approach --
        // data-unreachable with HTK (SizeLimit=2 < harvester Size=3), guarded
        // anyway [spec §4].
        if (u.is_harvester) u.manual_hold = true;
        order_unit_move(movers[k], targets[k].x, targets[k].y);
    }
}

void World::order_unload_selected() {
    for (Unit& u : units_) {
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        if (u.passengers_max <= 0 || u.cargo.empty()) continue;
        u.unload_pending = true;
        cargo_any_ = true;
    }
}

std::vector<CargoEvent> World::take_cargo_events() {
    std::vector<CargoEvent> out = std::move(cargo_events_);
    cargo_events_.clear();
    return out;
}

void World::step_cargo(double /*dt*/) {
    // FIREWALL (spec §8): until the first enter/unload order ever, this whole
    // function is one branch -- the only cost every existing demo pays.
    if (!cargo_any_) return;

    auto free_cell = [this](int x, int y) {
        // The SAME free predicate launch_unit uses: walkable + occ_.is_free,
        // which sees mid-hop reservations [spec §5].
        return walkable(x, y) && occ_.is_free(x, y);
    };

    // 1) Transport-death disposition [RE-GATE APPLIED: transport-cargo.md §5
    //    -- CONFIRMED: every death path in the original (FUN_00700DA0 /
    //    FUN_00707130 / ~UnitClass) deletes each passenger with NO survival
    //    branch; OpenTopped grants NO death survival (it governs fire-out
    //    while alive -- an RE-documented carry-forward for the fire-out
    //    phase, not CB-1 code). Every carried unit dies WITH the transport,
    //    tombstoned directly (alive=false, no die anim -- it was never
    //    on-screen), coords stamped to the transport's cell.] A transport
    //    killed by step_combat in frame N is handled here
    //    in frame N+1 (step_cargo precedes step_combat in the step order) --
    //    a one-frame lag, deterministic, documented.
    for (std::size_t ti = 0; ti < units_.size(); ++ti) {
        Unit& t = units_[ti];
        if (t.cargo.empty()) continue;
        if (t.alive && t.die_time < 0.0) continue;   // transport still fine
        for (int p : t.cargo) {
            if (p < 0 || p >= static_cast<int>(units_.size())) continue;
            Unit& u = units_[p];
            u.alive = false;          // tombstone directly (no die anim)
            u.die_time = -1.0;
            u.hp = 0;
            u.cargo_of = -1;
            u.cell_x = t.cell_x;      // coords stamped to the transport's cell
            u.cell_y = t.cell_y;
            u.fx = t.cell_x;
            u.fy = t.cell_y;
        }
        t.cargo.clear();
        t.unload_pending = false;
    }

    // 2) Boarding pass -- ascending unit index (the deterministic last-slot
    //    winner [RE-GATE APPLIED: transport-cargo.md §6 -- the original's
    //    container ops consult NO RNG (FUN_007C4D00 is ftol, not a draw);
    //    the last slot goes to whichever arrival is processed first, so
    //    ascending-index processing is the faithful analogue -- and under
    //    LIFO the LAST-processed boarder sits at the back and ejects FIRST]).
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (u.enter_target < 0) continue;
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) {
            u.enter_target = -1;
            continue;
        }
        const int ti = u.enter_target;
        if (ti < 0 || ti >= static_cast<int>(units_.size())) {
            u.enter_target = -1;
            continue;
        }
        Unit& t = units_[ti];
        if (!t.alive || t.die_time >= 0.0) {   // transport died: cancel cleanly
            u.enter_target = -1;               // (unit stands) [spec §3]
            continue;
        }
        const int adx = u.cell_x - t.cell_x, ady = u.cell_y - t.cell_y;
        const int cheb = std::max(adx < 0 ? -adx : adx, ady < 0 ? -ady : ady);
        if (cheb <= 1) {
            // Boarding trigger point = Chebyshev adjacency [RE-GATE APPLIED:
            // transport-cargo.md §3 -- DOCUMENTED DEVIATION: the original
            // boards on CO-LOCATION (the passenger paths onto the
            // transport's own tile, FUN_004D8ED0 case 0x12); our
            // one-unit-per-cell grid has no walk-onto-occupied notion, so
            // adjacency <= 1 is the adjudicated stand-in (same reachable
            // set, deterministic)].
            if (can_load(units_, ti, static_cast<int>(i))) {
                // Remove-from-map: release ALL occupancy (current cell AND
                // any mid-hop reservation -- keep-cell (-1,-1) matches
                // nothing), clear path/selection/targets, THEN link [spec §3].
                occ_.release_all_of(static_cast<int>(i), -1, -1);
                u.path.clear();
                u.path_idx = 0;
                u.selected = false;        // boarding force-clears selection (spec §4)
                u.target = -1;
                u.target_building = -1;
                u.attack_order = false;
                u.wait_time = 0.0;
                t.cargo.push_back(static_cast<int>(i));
                u.cargo_of = static_cast<int>(ti);
                u.enter_target = -1;
                cargo_events_.push_back({CargoEvent::Kind::Board, static_cast<int>(i),
                                         static_cast<int>(ti),
                                         static_cast<int>(t.cargo.size()),
                                         t.passengers_max});   // x = COUNT after
                                                               // the board [§1]
            } else {
                // Arrived but REFUSED (full / no longer eligible): the enter
                // order cancels cleanly, the unit stands [spec §3; the §7
                // capacity-refusal witness].
                u.enter_target = -1;
                cargo_events_.push_back({CargoEvent::Kind::Refuse, static_cast<int>(i),
                                         static_cast<int>(ti),
                                         static_cast<int>(t.cargo.size()),
                                         t.passengers_max});   // x = COUNT (full)
            }
        } else if (!u.moving()) {
            // Not adjacent and stopped (its approach path completed/cancelled
            // while the transport moved): re-path to the nearest free cell
            // around the transport's CURRENT cell via the EXISTING pure
            // nearest_free_cells helper (group_move.hpp) -- deterministic
            // (our stand-in for the original's staggered pursuit re-path;
            // documented deviation) [spec §3]. No free cell nearby -> keep
            // standing, retried next frame.
            const std::vector<Cellxy> g = nearest_free_cells(
                {t.cell_x, t.cell_y}, 1, passable_, grid_w_, grid_h_, occ_);
            if (!g.empty()) order_unit_move(static_cast<int>(i), g[0].x, g[0].y);
        }
    }

    // 3) Unload pass -- ONE eject per transport per logic frame [RE-GATE
    //    APPLIED: transport-cargo.md §4 -- CONFIRMED one per mission tick
    //    (FUN_00415EE0: if a passenger remains, eject one and return). The
    //    DeployTime deploy-START delay is NOT implemented in CB-1 (its scale
    //    is unpinned -- documented carry-forward), so ejection begins
    //    immediately at one-per-tick].
    for (std::size_t ti = 0; ti < units_.size(); ++ti) {
        Unit& t = units_[ti];
        if (!t.unload_pending) continue;
        if (!t.alive || t.die_time >= 0.0) { t.unload_pending = false; continue; }
        if (t.cargo.empty()) { t.unload_pending = false; continue; }
        if (t.moving()) continue;              // a STATIONARY transport ejects [spec §5]
        const int p = next_eject(t);           // LIFO -- the LAST-boarded leaves
                                               // first [RE-GATE APPLIED:
                                               // transport-cargo.md §6 -- CONFIRMED]
        if (p < 0 || p >= static_cast<int>(units_.size())) {  // defensive: drop a
            t.cargo.pop_back();                               // corrupt entry (the
            continue;                                         // one next_eject saw)
        }
        // The recovered facing-scatter picker [RE-GATE APPLIED:
        // transport-cargo.md §4]: one step out along body_facing +/- 0x4000,
        // the side alternating per eject via eject_counter's low bit;
        // nearest-free-neighbour fallback. CB-1 transports are vehicles
        // (HTK/FV) so body_facing is live; a non-vehicle transport would
        // eject along its spawn default -- data-unreachable on our roster.
        const Cellxy cell = eject_cell(t.cell_x, t.cell_y, t.body_facing,
                                       t.eject_counter, free_cell);
        if (cell.x < 0) continue;              // boxed in: HOLD -- passenger stays
                                               // aboard, unload_pending stays set,
                                               // retried every frame [RE-GATE
                                               // APPLIED: transport-cargo.md §4 --
                                               // CONFIRMED put-back (AddPassenger
                                               // back), never a drop or an abort]
        Unit& u = units_[p];
        u.cell_x = cell.x;                     // stamp coords fresh on eject
        u.cell_y = cell.y;
        u.fx = cell.x;
        u.fy = cell.y;
        occ_.set(cell.x, cell.y, p);           // claim occupancy BEFORE combat
                                               // targeting sees the unit again
        u.cargo_of = -1;
        u.guard_x = cell.x;                    // guard anchor = the eject cell
        u.guard_y = cell.y;
        const int f = facing_from_delta(cell.x - t.cell_x, cell.y - t.cell_y);
        if (f >= 0) u.facing = f;              // eject facing = OUTWARD along the
                                               // eject delta (= the +/-90-deg
                                               // scatter side when the primary
                                               // cell was taken) [RE-GATE APPLIED:
                                               // transport-cargo.md §4 -- outward
                                               // CONFIRMED structurally; the final
                                               // facing is PROBABLE]
        if (u.is_vehicle) {
            u.body_facing = facing_u16_from_delta(cell.x - t.cell_x, cell.y - t.cell_y);
            u.turret_facing = u.body_facing;
        }
        if (const ra2assets::MapCell* mc = map_->at(cell.x, cell.y)) u.level = mc->level;
        t.cargo.pop_back();                    // LIFO pop (the entry next_eject saw)
        ++t.eject_counter;                     // alternate the scatter side next
                                               // eject [transport-cargo.md §4]
        cargo_events_.push_back({CargoEvent::Kind::Eject, p, static_cast<int>(ti),
                                 cell.x, cell.y});
        if (t.cargo.empty()) t.unload_pending = false;
    }
}
```

(7) `World::step` — the insertion between `step_units` and `step_turrets` (the current first two statements of `step()` become):

```cpp
    step_units(units_, passable_, grid_w_, grid_h_, occ_, dt, move_cps_, anims_);

    // CB-1 Transports/Cargo [RE: transport-cargo.md]: boarding resolves the
    // frame the passenger lands adjacent; ejects claim occupancy before
    // combat targeting. A pure INSERTION between step_units and step_turrets
    // -- no existing unit-step logic is reordered; `if (!cargo_any_) return;`
    // inside is the only cost every existing demo pays [spec §8].
    step_cargo(dt);

    // Slew vehicle turrets AFTER movement and BEFORE combat so the aim gate
    // (vehicle_aimed) in step_combat sees this frame's post-slew turret
    // facing (docs/re/voxel-render.md §5). No-op for infantry.
    step_turrets(units_, buildings_, dt);
```

(8) **The carried-unit guard sweep** (spec §4 — every guard is one added clause on an EXISTING corpse-skip, provably false in every existing demo; this is the per-site checklist):

| # | File / function | Existing condition | Becomes |
|---|---|---|---|
| 1 | `world.cpp` `step_units` per-unit skip | `if (!u.alive \|\| u.die_time >= 0.0) continue;` | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0) continue;` |
| 2 | `world.cpp` `step_turrets` per-unit skip | `if (!u.is_vehicle \|\| !u.alive \|\| u.die_time >= 0.0) continue;` | `if (!u.is_vehicle \|\| !u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0) continue;` |
| 3 | `world.cpp` `unit_at_cell` corpse skip | `if (!u.alive \|\| u.die_time >= 0.0) continue;` | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0) continue;  // carried: unpickable` |
| 4 | `world.cpp` `unit_draw_order` include test | `if (units[i].alive) order.push_back(i);` | `if (units[i].alive && units[i].cargo_of < 0) order.push_back(i);  // carried: not rendered` |
| 5 | `world.cpp` `ai_idle_units` recruit skip | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.house != house \|\| u.is_harvester)` | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0 \|\| u.house != house \|\| u.is_harvester)` |
| 6 | `world.cpp` `ai_quarry_exists` unit scan | `if (u.alive && u.die_time < 0.0 && u.house != house && ...)` | `if (u.alive && u.die_time < 0.0 && u.cargo_of < 0 && u.house != house && ...)` |
| 7 | `world.cpp` `ai_pick_quarry_unit` skip | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.house == house) continue;` | `if (!u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0 \|\| u.house == house) continue;` |
| 8 | `combat.cpp` `valid_target` | `return v.alive && v.die_time < 0.0 && v.house != self_house;` | `return v.alive && v.die_time < 0.0 && v.cargo_of < 0 && v.house != self_house;  // carried: untargetable; an attacker whose target boards drops it here (CB-1)` |
| 9 | `combat.cpp` `step_combat`, right after the `(b) if (!u.alive) continue;` branch | — (new) | `// (b2) CB-1: a CARRIED unit is off-map -- it cannot fire, cool down, acquire, or pursue [spec §4].` newline `if (u.cargo_of >= 0) continue;` |
| 10 | `harvest.cpp` mission-FSM loop skip (~line 176) | `if (!u.is_harvester \|\| !u.alive \|\| u.die_time >= 0.0) continue;` | `if (!u.is_harvester \|\| !u.alive \|\| u.die_time >= 0.0 \|\| u.cargo_of >= 0) continue;  // carried harvester: mission FSM parks (CB-1)` |

(`Input::unit_under` and main.cpp's render_world selection/path + HP-bar loops are sites 11–12 of the same sweep; they land with the presentation wiring in Task 4.) The two `World::step` passes that iterate all units WITHOUT a corpse-skip stay untouched by design: the hold-at-range pass and the anim-override pass act only on units with a live `target`, and a carried unit provably has `target == -1` (cleared at boarding; auto-acquire blocked by guard 9; retaliation blocked by guard 8); the terrain-level patch writes the same stale value back — a no-op. **Implementer sweep instruction:** after applying the table, run `grep -n "die_time >= 0" D:\ra2-android\engine\*.cpp` and confirm every OTHER hit either already routes through a guarded site above or provably never sees a carried unit (note the audit in the commit message). If the sweep fans out beyond ~4 extra sites, STOP — the spec's decomposition tripwire — and report to the controller instead of improvising.

**Steps:**

- [ ] **Step 1: Write the failing World tests.** Append to `tests/test_world.cpp` (at file end; the file's `load_real_map_w` + `gi_stats`/`cons_stats` helpers from its own anonymous namespace are reused). Add `#include "cargo.hpp"  // CB-1: eject_cell -- the eject-cell witness` into the file's engine include list (between `building.hpp` and `factory.hpp` — this test target resolves engine headers bare via its `engine` include dir, the file's own style). All cases are install-gated and skip cleanly:

```cpp
// ---- CB-1 Transports/Cargo (guarded real map) [RE: transport-cargo.md] ----
namespace {
// HTK-shaped transport stats scaled for tests: a real vehicle profile with a
// cargo pool. Task 4 resolves real HTK numbers from rulesmd; tests hand-pick
// small pools to hit boundaries fast.
ra2engine::UnitCombat transport_stats(int passengers, int size_limit) {
    ra2engine::UnitCombat c;
    c.strength = 200;
    c.armor = "light";
    c.image = "HTK";
    c.is_vehicle = true;
    c.rot = 5;
    c.speed_cps = 4.0;
    c.passengers = passengers;
    c.size_limit = size_limit;
    c.size = 3;
    return c;
}
// First free walkable cell scanning east from (x0, y).
bool free_cell_east(ra2engine::World& w, int x0, int y, int& outx) {
    for (int dx = 0; dx < 12; ++dx)
        if (w.walkable(x0 + dx, y) && w.unit_at_cell(x0 + dx, y) < 0) {
            outx = x0 + dx;
            return true;
        }
    return false;
}
// True iff NO occupancy cell anywhere is owned by unit `idx`.
bool owns_no_cell(const ra2engine::World& w, int idx) {
    const ra2engine::Occupancy& occ = w.occupancy();
    for (int y = 0; y < occ.height(); ++y)
        for (int x = 0; x < occ.width(); ++x)
            if (occ.at(x, y) == idx) return false;
    return true;
}
}  // namespace

TEST_CASE("World: cargo boarding takes the passenger off-map and releases ALL occupancy") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(2, 2));   // unit 0: transport
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 3, c->y, gx));
    world.add_combat_unit(gx, c->y, 0, gi_stats());              // unit 1: passenger
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    CHECK(world.units()[1].enter_target == 0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);                     // boarded
    REQUIRE(world.units()[0].cargo.size() == 1);
    CHECK(world.units()[0].cargo[0] == 1);
    CHECK(world.units()[1].enter_target == -1);
    CHECK_FALSE(world.units()[1].selected);                      // force-cleared
    CHECK(owns_no_cell(world, 1));      // ALL occupancy released (incl. any
                                        // mid-hop reservation) -- spec §9
    // Off-map: unpickable, undrawn, untargetable.
    CHECK(world.unit_at_cell(world.units()[1].cell_x, world.units()[1].cell_y) != 1);
    for (int i : ra2engine::unit_draw_order(world.units())) CHECK(i != 1);
    int ex = -1;
    REQUIRE(free_cell_east(world, tx + 1, c->y + 2, ex));
    world.add_combat_unit(ex, c->y + 2, 1, cons_stats());        // unit 2: enemy
    world.step(dt);                                              // auto-acquire pass
    CHECK(world.units()[2].target != 1);                         // never the carried unit
    // One-unit-per-cell audit stays green among on-map units.
    for (std::size_t a = 0; a < world.units().size(); ++a)
        for (std::size_t b = a + 1; b < world.units().size(); ++b) {
            const ra2engine::Unit& ua = world.units()[a];
            const ra2engine::Unit& ub = world.units()[b];
            if (!ua.alive || ua.cargo_of >= 0 || !ub.alive || ub.cargo_of >= 0) continue;
            CHECK((ua.cell_x != ub.cell_x || ua.cell_y != ub.cell_y));
        }
}

TEST_CASE("World: cargo eject uses the facing-scatter eject_cell, stamps coords/facing, LIFO one per frame") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(3, 2));   // unit 0: Passengers=3
    const double dt = 1.0 / 15.0;
    for (int k = 0; k < 2; ++k) {                                // units 1, 2
        int gx = -1;
        REQUIRE(free_cell_east(world, tx + 2 + k, c->y + 1, gx));
        world.add_combat_unit(gx, c->y + 1, 0, gi_stats());
    }
    world.units()[1].selected = true;
    world.units()[2].selected = true;
    world.order_enter_transport(0);
    for (int f = 0; f < 300 && world.units()[0].cargo.size() < 2; ++f) world.step(dt);
    REQUIRE(world.units()[0].cargo.size() == 2);
    const std::vector<int> board_order = world.units()[0].cargo;   // boarding order;
                                                                   // LIFO ejects the
                                                                   // BACK first
    (void)world.take_cargo_events();                               // drain the boards
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    CHECK(world.units()[0].unload_pending);
    // The eject cell must be EXACTLY the pure picker's answer [RE-GATE
    // APPLIED: transport-cargo.md §4 -- facing-scatter, NOT a ring walk].
    auto free_cell = [&](int x, int y) {
        return world.walkable(x, y) && world.occupancy().is_free(x, y);
    };
    const int ttx = world.units()[0].cell_x, tty = world.units()[0].cell_y;
    const ra2engine::Cellxy expect = ra2engine::eject_cell(
        ttx, tty, world.units()[0].body_facing, world.units()[0].eject_counter, free_cell);
    REQUIRE(expect.x >= 0);
    world.step(dt);                                                // one eject per frame
    std::vector<ra2engine::CargoEvent> ev = world.take_cargo_events();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].kind == ra2engine::CargoEvent::Kind::Eject);
    CHECK(ev[0].unit == board_order[1]);                           // LIFO: the LAST
                                                                   // boarded leaves
                                                                   // FIRST [RE-GATE
                                                                   // APPLIED: transport-
                                                                   // cargo.md §6]
    CHECK(ev[0].x == expect.x);                                    // the scatter cell
    CHECK(ev[0].y == expect.y);
    const ra2engine::Unit& p0 = world.units()[board_order[1]];
    CHECK(p0.cargo_of == -1);
    CHECK(p0.cell_x == expect.x);                                  // coords stamped
    CHECK(p0.cell_y == expect.y);
    CHECK(p0.fx == doctest::Approx((double)expect.x));
    CHECK(world.occupancy().at(expect.x, expect.y) == board_order[1]);  // claimed
    CHECK(p0.guard_x == expect.x);                                 // guard anchor
    CHECK(p0.facing >= 0);
    CHECK(p0.facing < 8);
    CHECK(world.units()[0].eject_counter == 1);                    // side alternator
                                                                   // bumped [§4]
    CHECK(world.units()[0].cargo.size() == 1);                     // ONE per frame
    world.step(dt);
    ev = world.take_cargo_events();
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].unit == board_order[0]);                           // LIFO: the FIRST
                                                                   // boarded leaves LAST
    CHECK(world.units()[0].cargo.empty());
    CHECK_FALSE(world.units()[0].unload_pending);                  // cleared when empty
}

TEST_CASE("World: cargo boxed-in unload HOLDs, then resumes when a neighbouring cell frees") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    const int ty = c->y;
    world.add_combat_unit(tx, ty, 0, transport_stats(2, 2));     // unit 0
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 2, ty, gx));
    world.add_combat_unit(gx, ty, 0, gi_stats());                // unit 1
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);
    // Box the transport in: fill EVERY free walkable cell at Chebyshev
    // r <= 2 (eject_cell's whole reach -- the r=1 primary/fallback plus the
    // r=2 fallback ring) with plain units. Track one OUTER (r == 2) blocker
    // to release later -- inner blockers stay put.
    int mover = -1;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int x = tx + dx, y = ty + dy;
            if (!world.walkable(x, y) || world.unit_at_cell(x, y) >= 0) continue;
            world.add_unit(x, y, "GI");
            const int bi = static_cast<int>(world.units().size()) - 1;
            if (mover < 0 && std::max(std::abs(dx), std::abs(dy)) == 2) mover = bi;
        }
    REQUIRE(mover >= 0);
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    for (int f = 0; f < 5; ++f) world.step(dt);
    CHECK(world.units()[0].cargo.size() == 1);                   // HOLD: still aboard
    CHECK(world.units()[0].unload_pending);                      // order persists
    CHECK(world.take_cargo_events().empty());                    // no eject happened
    // Free one outer (r=2 fallback) cell: walk the tracked blocker away.
    world.clear_selection();
    world.units()[mover].selected = true;
    world.order_move_selected(tx + 8, ty + 8);
    for (int f = 0; f < 400 && world.units()[0].cargo.size() == 1; ++f) world.step(dt);
    CHECK(world.units()[0].cargo.empty());                       // resumed + ejected
    CHECK(world.units()[1].cargo_of == -1);
    CHECK_FALSE(world.units()[0].unload_pending);
}

TEST_CASE("World: cargo transport death tombstones its cargo; enter orders cancel cleanly") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(2, 2));   // unit 0
    int gx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y, gx));
    world.add_combat_unit(gx, c->y, 0, gi_stats());              // unit 1: boards
    int hx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y + 3, hx));
    world.add_combat_unit(hx, c->y + 3, 0, gi_stats());          // unit 2: en route
    world.units()[1].selected = true;
    world.order_enter_transport(0);
    const double dt = 1.0 / 15.0;
    for (int f = 0; f < 300 && world.units()[1].cargo_of < 0; ++f) world.step(dt);
    REQUIRE(world.units()[1].cargo_of == 0);
    world.clear_selection();
    world.units()[2].selected = true;
    world.order_enter_transport(0);
    REQUIRE(world.units()[2].enter_target == 0);
    // Kill the transport the way combat does (die_time flips >= 0).
    world.units()[0].die_time = 0.0;
    world.step(dt);
    CHECK_FALSE(world.units()[1].alive);                         // died WITH it
    CHECK(world.units()[1].cargo_of == -1);
    CHECK(world.units()[1].cell_x == world.units()[0].cell_x);   // coords stamped
    CHECK(world.units()[1].cell_y == world.units()[0].cell_y);
    CHECK(world.units()[0].cargo.empty());
    CHECK(world.units()[2].enter_target == -1);                  // cancelled cleanly
    CHECK(world.units()[2].alive);                               // ...and unharmed
}

TEST_CASE("World: cargo capacity refusal at boarding; a new order supersedes enter/unload") {
    ra2assets::MapFile map;
    if (!load_real_map_w(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    ra2engine::World world(map);
    world.set_move_speed(4.0);
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    int tx = -1;
    REQUIRE(free_cell_east(world, c->x, c->y, tx));
    world.add_combat_unit(tx, c->y, 0, transport_stats(1, 2));   // unit 0: ONE slot
    for (int k = 0; k < 2; ++k) {                                // units 1, 2
        int gx = -1;
        REQUIRE(free_cell_east(world, tx + 2 + k, c->y + 1, gx));
        world.add_combat_unit(gx, c->y + 1, 0, gi_stats());
    }
    world.units()[1].selected = true;
    world.units()[2].selected = true;
    world.order_enter_transport(0);                              // oversubscribed
    const double dt = 1.0 / 15.0;
    int boards = 0, refusals = 0;
    for (int f = 0; f < 400 && boards + refusals < 2; ++f) {
        world.step(dt);
        for (const ra2engine::CargoEvent& e : world.take_cargo_events()) {
            if (e.kind == ra2engine::CargoEvent::Kind::Board) ++boards;
            if (e.kind == ra2engine::CargoEvent::Kind::Refuse) ++refusals;
        }
    }
    CHECK(boards == 1);                                          // exactly one fits
    CHECK(refusals == 1);                                        // one REFUSED at full
    CHECK(world.units()[0].cargo.size() == 1);
    // A new move order on the transport supersedes a pending unload.
    world.clear_selection();
    world.units()[0].selected = true;
    world.order_unload_selected();
    CHECK(world.units()[0].unload_pending);
    world.order_move_selected(tx + 6, c->y + 6);
    CHECK_FALSE(world.units()[0].unload_pending);                // superseded
    // A new move order on an en-route passenger supersedes its enter order.
    int wx = -1;
    REQUIRE(free_cell_east(world, tx + 2, c->y + 4, wx));
    world.add_combat_unit(wx, c->y + 4, 0, gi_stats());          // unit 3
    int vx = -1;
    REQUIRE(free_cell_east(world, wx + 1, c->y + 4, vx));
    world.add_combat_unit(vx, c->y + 4, 0, transport_stats(2, 2));  // unit 4
    world.clear_selection();
    world.units()[3].selected = true;
    world.order_enter_transport(4);
    REQUIRE(world.units()[3].enter_target == 4);
    world.order_move_selected(tx, c->y + 6);                     // unit 3 still selected
    CHECK(world.units()[3].enter_target == -1);                  // superseded
}
```

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

Expected: compile FAIL (`order_enter_transport`/`order_unload_selected`/`take_cargo_events`/`CargoEvent` not members/types of World; `UnitCombat` has no `passengers`). This is the failing-test proof for the whole World surface.

- [ ] **Step 3: Implement.** Apply ALL of Interfaces above: the `UnitCombat` fields, the `world.hpp` additions (CargoEvent + public declarations + private state), the `world.cpp` changes (1)–(7), and the guard-sweep table (8) across `world.cpp`/`combat.cpp`/`harvest.cpp` (including the grep audit).

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="World: cargo*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: the 5 new `World: cargo` cases PASS; then the FULL suite passes 0 failed (the guard clauses must not disturb any existing movement/combat/harvest/AI case).

- [ ] **Step 5: Byte-identity — prove the core-sim insertion is inert.** All EIGHT gates re-run FOREGROUND, blocking (timeout 600000 ms each; the same eight commands + expected pins as Task 2 Step 3, verbatim). Rationale to hold the implementer to: `step_cargo` is fenced behind `cargo_any_ = false` in every existing demo; every guard clause reads a field at its inert default; `commit_attack_target`/`order_move_selected`/`order_harvest` write -1/false over -1/false; `add_combat_unit` copies zeros. ANY drift = defect in THIS task.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp engine/unit_combat.hpp engine/combat.cpp engine/harvest.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "feat(world): transport cargo integration - enter/unload orders, step_cargo, carried-unit guards, transport-death disposition

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: main.cpp + input wiring — transport art/rules, enter gesture, unload key, HUD pips, sounds

**Files:**
- Modify: `engine/main.cpp`, `engine/input.cpp`, `engine/iso_renderer.hpp`, `engine/iso_renderer.cpp`
- Test: `tests/test_input.cpp` (append)
- CMake: **none** — all TUs already registered.

**Interfaces:**
- Consumes: `cargo_total_size` (Task 2 — pip-only), `Unit::passengers_max`/`cargo_of` (Task 2), `World::order_enter_transport`/`order_unload_selected`/`take_cargo_events`/`CargoEvent` (Task 3), `UnitCombat::{passengers,size_limit,size}` (Task 3), `load_vehicle_assets`/`VehicleAssets` (existing), `Rules::section_double` (existing), `Ini::get` (existing), `resolve_sound_event`/`AudioSystem`/`audio.mixer->play` (existing), `tile_center_screen`/`kHpBar*` (iso_renderer.cpp file-locals, reused in-file).
- Produces:

(1) `engine/main.cpp` — `resolve_combat_unit` gains three lines before the `std::cerr` log (EVERY resolved unit engine-wide gets its real cargo stats; inert everywhere — no existing demo issues an enter order, `passengers_max > 0` alone changes no behavior):

```cpp
    // CB-1 cargo stats [RE-GATE APPLIED: transport-cargo.md §1 -- absent-key
    // defaults CONFIRMED: Passengers/SizeLimit -> 0, Size -> 1. In the
    // binary Size/SizeLimit are doubles (type +0x380/+0x388); int is exact
    // for the roster values (1/2/3). Capacity gates on the COUNT
    // (NumPassengers < Passengers); Size/SizeLimit form the per-passenger
    // cap and Size otherwise feeds only the pip display]. Real rulesmd
    // ground truth: [HTK] Passengers=5 SizeLimit=2 Size=3, [FV] Passengers=1
    // SizeLimit=1 Size=3, infantry Size=1.
    r.combat.passengers = (int)rules.section_double(unit_id, "Passengers", 0.0);
    r.combat.size_limit = (int)rules.section_double(unit_id, "SizeLimit", 0.0);
    r.combat.size = (int)rules.section_double(unit_id, "Size", 1.0);
```

(2) `engine/main.cpp` — new struct + loader, placed immediately after `miner_for_id` (uses the already-open `ra2` + the shared unit palette, exactly like `load_miner_assets`):

```cpp
// CB-1: the demo/interactive multi-capacity transport [spec: Transport
// choice; RE: transport-cargo.md §1]. HTK (Soviet Flak Track: Passengers=5
// SizeLimit=2 Size=3, ground Drive, art PRESENT on this install) with FV
// (IFV, Passengers=1, already producible) as the art-guard fallback;
// multi-passenger logic is fully implemented regardless (capacity>1 is
// covered host-only by the pure tests). load_vehicle_assets already
// resolves the cargo stats via resolve_combat_unit, so a candidate is
// usable iff its art loaded AND it is actually a transport.
struct TransportAssets {
    VehicleAssets va;      // voxel render assets + combat (cargo stats included)
    std::string id;        // "HTK" or "FV"
    bool ok = false;       // art present AND Passengers > 0
};

TransportAssets load_transport_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                                      const ra2assets::Palette& unit_pal) {
    TransportAssets ta;
    for (const char* id : {"HTK", "FV"}) {
        VehicleAssets va = load_vehicle_assets(dir, ra2, unit_pal, id);
        if (!va.ok || va.combat.passengers <= 0) continue;  // art-guard-drop ->
        ta.va = std::move(va);                              // try the fallback
        ta.id = id;
        ta.ok = true;
        break;
    }
    if (ta.ok)
        std::cerr << "[transport] id=" << ta.id
                  << " passengers=" << ta.va.combat.passengers
                  << " size_limit=" << ta.va.combat.size_limit
                  << " size=" << ta.va.combat.size << "\n";
    else
        std::cerr << "[transport] unavailable (HTK and FV art both missing); "
                     "cargo gestures degrade to no-ops\n";
    return ta;
}
```

(3) `engine/iso_renderer.hpp` — after the `draw_hp_bar` declaration:

```cpp
// CB-1 Transports/Cargo: the minimal loaded-count indicator [RE:
// transport-cargo.md §1; spec §6] -- pips_max small tick rects in a row
// directly UNDER the HP bar (same tile_center_screen anchor and SDL_Rect
// fill primitive as draw_hp_bar; REUSE only, no new font/UI systems), the
// first pips_filled of them filled light, the rest dark. The fill count is
// the SIZE-SUM (cargo_total_size -- the original GetTotalSize, which is
// pip-only [RE-GATE APPLIED: transport-cargo.md §1]) while capacity is the
// COUNT, so a Size-2 passenger lights two pips yet consumes one slot; a
// fill above pips_max saturates the row. Vanilla's PipScale=Passengers pip
// row is the reference look, RE-documented but NOT pixel-matched in CB-1.
// Caller decides WHETHER to draw (a selected transport); no-op if
// pips_max <= 0.
void draw_cargo_pips(void* sdl_renderer, const Unit& unit, int pips_filled, int pips_max,
                     const Camera& cam, int view_w, int view_h);
```

(4) `engine/iso_renderer.cpp` — after `draw_hp_bar`'s definition:

```cpp
void draw_cargo_pips(void* sdl_renderer, const Unit& unit, int pips_filled, int pips_max,
                     const Camera& cam, int view_w, int view_h) {
    if (pips_max <= 0) return;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(sdl_renderer);
    int cx, cy;
    tile_center_screen(unit.fx, unit.fy, unit.level, cam, cx, cy);
    // One row directly UNDER the HP bar (same anchor math as draw_hp_bar).
    const int row_y = cy - kHpBarUp - kHpBarGap + kHpBarH + 2;
    const int pip_w = 4, pip_h = 3, gap = 2;
    const int total_w = pips_max * pip_w + (pips_max - 1) * gap;
    int x = cx - total_w / 2;
    if (x + total_w <= 0 || row_y + pip_h <= 0 || x >= view_w || row_y >= view_h) return;
    for (int k = 0; k < pips_max; ++k) {
        SDL_Rect r{x, row_y, pip_w, pip_h};
        if (k < pips_filled) SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        else SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderFillRect(renderer, &r);
        x += pip_w + gap;
    }
}
```

(5) `engine/main.cpp` — `render_world`: add `#include "cargo.hpp"` to main.cpp's engine includes; then grow the two carried-guards (sweep sites 11a/11b) and the pip call. The paths/selection loop condition becomes:

```cpp
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;  // CB-1:
        ra2engine::draw_path(ren, u, cam, W, H);       // carried units draw nothing
        if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H, veh_width(u));
    }
```

and the HP-bar loop becomes:

```cpp
    // HP bars after sprites, before the selection box: alive, non-dying,
    // NON-CARRIED units that are selected or currently damaged. A selected
    // transport also shows its loaded pip row -- filled with the SIZE-SUM
    // (cargo_total_size), the pip-only metric [RE-GATE APPLIED:
    // transport-cargo.md §1]; capacity itself is the COUNT [spec §6].
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        if (u.selected || u.hp < u.max_hp)
            ra2engine::draw_hp_bar(ren, u, cam, W, H, veh_width(u));
        if (u.selected && u.passengers_max > 0)
            ra2engine::draw_cargo_pips(ren, u, ra2engine::cargo_total_size(us, u),
                                       u.passengers_max, cam, W, H);
    }
```

(The sprite pass itself needs no guard: `unit_draw_order` already excludes carried units — Task 3 site 4.)

(6) `engine/input.cpp` — sweep site 12: `unit_under`'s corpse skip becomes

```cpp
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;  // dead/dying/
                                                     // CARRIED: unpickable (CB-1)
```

(7) `engine/input.cpp` — the enter gesture, inserted in `handle()`'s right-click branch INSIDE the ground-move `else` block, immediately AFTER the harvester refinery-return branch (`if (harv_selected && here_b >= 0 && ...) { world_.order_return(); break; }`) and BEFORE the attack-building resolution (`const int b_idx = ...`):

```cpp
                    // CB-1 enter-transport gesture [RE: transport-cargo.md
                    // §2/§3; spec §3]: a right-click on an OWN, alive,
                    // non-dying TRANSPORT (passengers_max > 0 -- the Unit
                    // carries the roster knowledge, the same way the
                    // refinery-return gesture reads roster types) with a
                    // selection containing at least one eligible
                    // NON-transport unit issues the enter order instead of a
                    // group move. Resolved AFTER the harvester branches (ore/
                    // refinery win for miners) and BEFORE attack-building/
                    // move. The original's "enter" cursor is cosmetic; ours
                    // stays default (noted divergence, no new UI systems).
                    if (idx >= 0 && world_.units()[idx].house == 0 &&
                        world_.units()[idx].passengers_max > 0) {
                        bool eligible = false;
                        for (std::size_t si = 0; si < world_.units().size(); ++si) {
                            const Unit& su = world_.units()[si];
                            if (!su.selected || su.house != 0 || !su.alive ||
                                su.die_time >= 0.0 || static_cast<int>(si) == idx)
                                continue;
                            if (su.passengers_max > 0) continue;  // transports
                            eligible = true;                      // do not ride
                            break;
                        }
                        if (eligible) {
                            const int actor = lowest_selected();
                            world_.order_enter_transport(idx);
                            actions_.move_ordered = true;   // move-ack voice
                            actions_.actor = actor;
                            break;  // handled: no attack/move for this click
                        }
                    }
```

(8) `engine/main.cpp` — `run_interactive` wiring:
  - After `MinerAssets miners = load_miner_assets(...)`: add `TransportAssets transport = load_transport_assets(dir, ra2, ua.pal);`
  - Where the interactive render `vlist` is assembled (`std::vector<const VehicleAssets*> vlist{&va, &miners.harv, &miners.cmin};` at ~line 2392, followed by the `ur`/`ur_ai` vehicle push_backs): add `if (transport.ok) vlist.push_back(&transport.va);` after the `ur_ai` push_back loop.
  - After the interactive world's initial unit spawns (immediately after the `spawn_tank_squads` call site): field ONE player transport, art-guarded (interactive/screenshot only — NEVER the byte-identity demos):

```cpp
    // CB-1: field one transport for the player (art-guarded; interactive
    // only). Ring-scan from the start cell for a free walkable cell -- the
    // same spawn discipline every other interactive extra uses.
    if (transport.ok) {
        const ra2assets::MapCell* tc0 = ra2engine::pick_start_cell(a.map);
        bool placed = false;
        for (int r = 1; tc0 && r <= 6 && !placed; ++r)
            for (int dy = -r; dy <= r && !placed; ++dy)
                for (int dx = -r; dx <= r && !placed; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    const int x = tc0->x + dx, y = tc0->y + dy;
                    if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                        world.add_combat_unit(x, y, /*house=*/0, transport.va.combat);
                        placed = true;
                    }
                }
    }
```

  - Before the main loop: `bool prev_unload_key = false;`. Inside the loop, right after the arrow-key pan block (which already fetched `ks`):

```cpp
        // CB-1 unload key [spec §5]: D (edge-detected) unloads every selected
        // own transport with cargo -- routed by main.cpp like Escape ->
        // placement-cancel (UI routing only; the sim rule lives in
        // World::order_unload_selected + step_cargo).
        const bool unload_key = ks[SDL_SCANCODE_D] != 0;
        if (unload_key && !prev_unload_key) world.order_unload_selected();
        prev_unload_key = unload_key;
```

  - Next to the harvest-event audio drain (the ChronoOut/ChronoIn block): the transport sound hook:

```cpp
        // CB-1 Enter/LeaveTransportSound (nice-to-have, NON-GATING [spec §6]
        // -- cut this block first if the task runs long): the same guarded
        // pattern as the chrono sounds above; absent audio degrades silently
        // (mixer->play("") is a no-op). Refusals are silent.
        {
            const std::vector<ra2engine::CargoEvent> cev = world.take_cargo_events();
            if (audio.enabled)
                for (const ra2engine::CargoEvent& e : cev) {
                    if (e.kind == ra2engine::CargoEvent::Kind::Board)
                        audio.mixer->play(audio.enter_transport);
                    else if (e.kind == ra2engine::CargoEvent::Kind::Eject)
                        audio.mixer->play(audio.leave_transport);
                }
        }
```

(9) `engine/main.cpp` — `AudioSystem` gains two members next to `chrono_out, chrono_in`:

```cpp
    // CB-1 transport sounds [RE: transport-cargo.md; spec §6]: soundmd
    // section names from rulesmd [HTK] EnterTransportSound/
    // LeaveTransportSound, falling back to the [AudioVisual] defaults
    // ("" -> silent no-op on play, the chrono pattern).
    std::string enter_transport, leave_transport;
```

and `init_audio`, INSIDE the existing `{ Ini cmin_ini = ... }` block after the chrono lines:

```cpp
        audio.enter_transport = cmin_ini.get("HTK", "EnterTransportSound", "");
        if (audio.enter_transport.empty())
            audio.enter_transport = cmin_ini.get("AudioVisual", "EnterTransportSound", "");
        audio.leave_transport = cmin_ini.get("HTK", "LeaveTransportSound", "");
        if (audio.leave_transport.empty())
            audio.leave_transport = cmin_ini.get("AudioVisual", "LeaveTransportSound", "");
        for (const std::string& k : {audio.enter_transport, audio.leave_transport})
            if (!k.empty() && !pending.count(k))
                pending[k] = resolve_sound_event(audio, sounds, idx, bag, k);
```

**Steps:**

- [ ] **Step 1: Write the failing Input test.** Append to `tests/test_input.cpp` (at file end; reuses the file's `load_real_map`/`ground_px_for_unit`/`right_click`/`left_click`/`gi_combat` helpers):

```cpp
TEST_CASE("Input: right-click on an own transport issues the enter order; carried units are unpickable") {
    MapFile map;
    if (!load_real_map(map)) {
        MESSAGE("skipped: real map assets not found under RA2_INSTALL_DIR");
        return;
    }
    World world(map);
    const MapCell* c = ra2engine::pick_start_cell(map);
    REQUIRE(c != nullptr);
    // An FV-shaped transport (1 slot, SizeLimit 2) + a GI on nearby cells.
    ra2engine::UnitCombat tc = gi_combat();
    tc.image = "FV";
    tc.is_vehicle = true;
    tc.passengers = 1;
    tc.size_limit = 2;
    tc.size = 3;
    int tx = -1, ty = -1, gx = -1, gy = -1;
    for (int dx = 0; dx < 10 && gx < 0; ++dx) {
        const int x = c->x + dx, y = c->y;
        if (!world.walkable(x, y) || world.unit_at_cell(x, y) >= 0) continue;
        if (tx < 0) { tx = x; ty = y; continue; }
        gx = x; gy = y;
    }
    REQUIRE(gx >= 0);
    world.add_combat_unit(tx, ty, 0, tc);            // unit 0: the transport
    world.add_combat_unit(gx, gy, 0, gi_combat());   // unit 1: the GI
    Camera cam;                                      // identity camera
    Input in(world, cam, 4000, 4000);
    world.units()[1].selected = true;
    int px = 0, py = 0;
    ground_px_for_unit(world.units()[0], px, py);
    right_click(in, px, py, 10);
    CHECK(world.units()[1].enter_target == 0);       // enter order, not a move
    CHECK(world.units()[0].enter_target == -1);      // transport itself untouched
    CHECK(world.units()[1].target == -1);            // no attack leaked through
    // A CARRIED unit is unpickable (sweep site 12): stuff the GI aboard by
    // hand and click its (stale) ground point -- nothing selects.
    world.clear_selection();
    world.units()[1].cargo_of = 0;
    ground_px_for_unit(world.units()[1], px, py);
    left_click(in, px, py, false, 500);
    CHECK_FALSE(world.units()[1].selected);
}
```

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="Input: right-click on an own transport*"
```

Expected: build OK (the test uses only Task 2/3 surface) but the case FAILS — `enter_target` stays -1 (the gesture doesn't exist yet, the click group-moves instead) — and the carried-unpickable CHECK fails (unit_under still picks it).

- [ ] **Step 3: Implement.** Apply Interfaces (1)–(9): resolve_combat_unit lines, TransportAssets + loader, draw_cargo_pips (hpp+cpp), render_world guards + pips, input.cpp unit_under guard + gesture, run_interactive wiring (transport load/spawn/vlist, unload key, sound drain), AudioSystem + init_audio.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="Input:*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: the new Input case + every existing Input case PASS; full suite 0 failed.

- [ ] **Step 5: Byte-identity — prove the resolution/wiring is inert.** All EIGHT gates re-run FOREGROUND, blocking (timeout 600000 ms each; same eight commands + pins as Task 2 Step 3, verbatim). Rationale: `resolve_combat_unit` now stamps real Passengers/Size values onto every resolved unit (e.g. FV in the prod roster gets passengers=1), but NOTHING reads them outside the cargo paths — `step_cargo` stays fenced (`cargo_any_` false in every demo), the gesture/key/pips/sounds are interactive-only, and no demo output includes the new `[transport]`/`[unit]`-adjacent stderr. ANY drift = defect in THIS task.

- [ ] **Step 6: Interactive smoke (rendered, FOREGROUND, blocking; generous timeout).** Launch the interactive session briefly to confirm the transport spawns and renders (art-guard log line `[transport] id=HTK ...` on this install), then close the window:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --screenshot "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\cb1_task4_smoke.png"
```

(`--screenshot` does not exercise run_interactive's transport spawn; its value here is proving the BUILD still runs end-to-end with the new TU wiring. The full scripted exercise of gesture/board/carry/unload is Task 5's demo — do NOT attempt to drive the interactive window programmatically.) Expected: exit 0, PNG written.

- [ ] **Step 7: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp engine/input.cpp engine/iso_renderer.hpp engine/iso_renderer.cpp tests/test_input.cpp
git -C "D:\ra2-android" commit -m "feat(app): transport wiring - HTK art-guarded assets (FV fallback), enter gesture, D unload key, cargo pips, transport sounds

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: --demo-transport — the 9th permanent byte-identity gate

**Files:**
- Modify: `engine/main.cpp` (transport_fold + DemoTransportResult + simulate_transport + run_demo_transport + arg dispatch)

**Interfaces:**
- Consumes: `fnv_mix` (main.cpp), `cargo_total_size`/`unit_draw_order` (Task 2/engine), `World::order_enter_transport`/`order_unload_selected`/`take_cargo_events`/`CargoEvent` (Task 3), `TransportAssets`/`load_transport_assets` (Task 4), `build_world`/`spawn_squad`/`pick_start_cell`/`capture_png`/`render_world`/`env_install_dir`/`load_assets`/`load_unit_assets` (existing).
- Produces — added to `engine/main.cpp` immediately after `run_demo_ai_trigger` (before `mode_demo_audio`):

```cpp
// --- --demo-transport helpers ------------------------------------------------

// FNV fold of the cargo-phase state [spec §7]: every unit's (cell_x, cell_y,
// alive, cargo_of), each transport's cargo list (in order) + the derived
// pip total-size (cargo_total_size -- capacity's COUNT is the folded list
// length) + its cell, plus the draw-order size and a carried-units-own-
// no-cells occupancy audit -- folded at the demo's four fixed checkpoints
// (post-load, post-carry, post-unload, end). A NEW accumulator; world_hash/
// econ_hash/prod_hash/ai_base_hash/ai_team_hash/ai_trigger_hash UNTOUCHED.
uint64_t transport_fold(uint64_t h, const ra2engine::World& world) {
    const std::vector<ra2engine::Unit>& us = world.units();
    for (const ra2engine::Unit& u : us) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.cargo_of + 1));
    }
    for (const ra2engine::Unit& u : us) {
        if (u.passengers_max <= 0) continue;   // transports only
        h = fnv_mix(h, static_cast<uint64_t>(u.cargo.size()));
        for (int p : u.cargo) h = fnv_mix(h, static_cast<uint64_t>(p));
        h = fnv_mix(h, static_cast<uint64_t>(ra2engine::cargo_total_size(us, u)));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
    }
    // No carried unit is rendered or owns a cell [spec §7 step 2].
    h = fnv_mix(h, static_cast<uint64_t>(ra2engine::unit_draw_order(us).size()));
    uint64_t carried_cells = 0;
    const ra2engine::Occupancy& occ = world.occupancy();
    for (int y = 0; y < occ.height(); ++y)
        for (int x = 0; x < occ.width(); ++x) {
            const int o = occ.at(x, y);
            if (o >= 0 && o < static_cast<int>(us.size()) && us[o].cargo_of >= 0)
                ++carried_cells;
        }
    h = fnv_mix(h, carried_cells);
    return h;
}

struct DemoTransportResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the transport demo [spec §7] (headless or rendered --
// bit-identical): on the standard demo map, spawn one transport (HTK on this
// install; the FV fallback scales the script to capacity 1) + capacity+1 GIs
// for house 0, all scripted (no player input):
//   1. LOAD: order all GIs to enter -- capacity board (witnessed per board;
//      capacity is the passenger COUNT [RE-GATE APPLIED: transport-cargo.md
//      §1]), the last is REFUSED at capacity (the exact-fill boundary AND
//      the refusal both witnessed).
//   2. CARRY: order the loaded transport to a distant cell; run to arrival;
//      no passenger is rendered/occupying (folded into the hash).
//   3. UNLOAD: eject one per logic frame onto facing-scatter cells
//      (eject_cell; ejection begins immediately -- the DeployTime deploy-
//      START delay is unpinned and NOT implemented, a documented
//      carry-forward), LIFO (reverse boarding) witnessed [RE-GATE APPLIED:
//      transport-cargo.md §4/§6].
//   4. RESUME: run fixed frames; ejected GIs stand idle/guard normally.
// transport_hash = FNV folded at the four checkpoints. Wedge guards: >=1
// board AND >=1 eject AND >=1 capacity refusal AND LIFO eject order, else
// ok=false (exit 1 -- the gate can never pass vacuously).
DemoTransportResult simulate_transport(Assets& a, UnitAssets& ua, const TransportAssets& ta,
                                       const std::vector<const VehicleAssets*>& vlist,
                                       bool render, SDL_Renderer* ren,
                                       ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                       int W, int H, const std::string& start_png,
                                       const std::string& end_png) {
    DemoTransportResult r;
    if (!ta.ok) { r.ok = false; return r; }
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    // Spawn the transport (unit 0) + capacity+1 GIs (units 1..n).
    const int cap = ta.va.combat.passengers;
    const int n_pass = cap + 1;
    world.add_combat_unit(pc->x, pc->y, /*house=*/0, ta.va.combat);
    const int t_idx = 0;
    spawn_squad(world, pc->x - 4, pc->y, n_pass, /*house=*/0, ua.gi);
    {
        char b[160];
        std::snprintf(b, sizeof b,
                      "[transport] variant=%s passengers=%d size_limit=%d size=%d gis=%d",
                      ta.id.c_str(), cap, ta.va.combat.size_limit, ta.va.combat.size,
                      n_pass);
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    int boards = 0, refusals = 0, ejects = 0;
    std::vector<int> board_seq, eject_seq;   // LIFO witness material
    uint64_t h = 0xcbf29ce484222325ull;      // FNV-1a offset basis (world_hash's)

    auto step_one = [&]() {
        world.step(dt);
        for (const ra2engine::CargoEvent& e : world.take_cargo_events()) {
            char b[140];
            if (e.kind == ra2engine::CargoEvent::Kind::Board) {
                ++boards;
                board_seq.push_back(e.unit);
                std::snprintf(b, sizeof b, "[cargo] board unit=%d count=%d/%d frame=%ld",
                              e.unit, e.x, e.y, frame);
            } else if (e.kind == ra2engine::CargoEvent::Kind::Refuse) {
                ++refusals;
                std::snprintf(b, sizeof b,
                              "[cargo] refuse unit=%d (full) count=%d/%d frame=%ld",
                              e.unit, e.x, e.y, frame);
            } else {
                ++ejects;
                eject_seq.push_back(e.unit);
                std::snprintf(b, sizeof b,
                              "[cargo] eject unit=%d cell=(%d,%d) order=%d frame=%ld",
                              e.unit, e.x, e.y, ejects, frame);
            }
            r.witness.emplace_back(b);
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };

    // 1) LOAD: select every GI and issue the enter order (oversubscribed by
    //    one -- the capacity refusal is part of the script).
    for (int i = 1; i <= n_pass; ++i) world.units()[i].selected = true;
    world.order_enter_transport(t_idx);

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < 900 && boards + refusals < n_pass) step_one();
    h = transport_fold(h, world);                     // checkpoint 1: post-load
    {
        char b[120];
        std::snprintf(b, sizeof b, "[cargo] loaded boards=%d refusals=%d cargo=%d",
                      boards, refusals, (int)world.units()[t_idx].cargo.size());
        r.witness.emplace_back(b);
    }

    // 2) CARRY: drive the loaded transport to a distant cell.
    world.clear_selection();
    world.units()[t_idx].selected = true;
    world.order_move_selected(pc->x + 12, pc->y + 8);
    while (frame < 2400 && world.units()[t_idx].moving()) step_one();
    h = transport_fold(h, world);                     // checkpoint 2: post-carry
    {
        char b[120];
        std::snprintf(b, sizeof b, "[cargo] carry arrived=(%d,%d) cargo=%d frame=%ld",
                      world.units()[t_idx].cell_x, world.units()[t_idx].cell_y,
                      (int)world.units()[t_idx].cargo.size(), frame);
        r.witness.emplace_back(b);
    }

    // 3) UNLOAD: one eject per frame via the facing-scatter eject_cell, LIFO.
    world.clear_selection();
    world.units()[t_idx].selected = true;
    world.order_unload_selected();
    while (frame < 3000 && !world.units()[t_idx].cargo.empty()) step_one();
    h = transport_fold(h, world);                     // checkpoint 3: post-unload

    // 4) RESUME: fixed frames; ejected GIs stand idle/guard normally.
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = transport_fold(h, world);                     // checkpoint 4: end
    r.hash = h;

    {
        char b[140];
        std::snprintf(b, sizeof b,
                      "[transport] final boards=%d refusals=%d ejects=%d drawn=%d frame=%ld",
                      boards, refusals, ejects,
                      (int)ra2engine::unit_draw_order(world.units()).size(), frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[transport] transport_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously.
    if (boards < 1) { r.ok = false; r.witness.emplace_back("[transport] WEDGE: no board"); }
    if (ejects < 1) { r.ok = false; r.witness.emplace_back("[transport] WEDGE: no eject"); }
    if (refusals < 1) {
        r.ok = false;
        r.witness.emplace_back("[transport] WEDGE: no capacity refusal");
    }
    const std::vector<int> lifo_expect(board_seq.rbegin(), board_seq.rend());
    if (eject_seq != lifo_expect) { // LIFO: eject order == REVERSE boarding order
        r.ok = false;               // [RE-GATE APPLIED: transport-cargo.md §6 --
                                    // CONFIRMED head-push/head-pop stack]
        r.witness.emplace_back(
            "[transport] WEDGE: eject order != LIFO (reverse boarding) order");
    }
    // Capture the ALREADY-rendered final frame WITHOUT stepping again (the
    // --demo-ai-team FIX-3 rule: an extra rendered-only step could append a
    // witness line and false-RED the byte-compare).
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-transport: the CB-1 cargo integration gate (9th permanent
// byte-identity gate). Mirrors run_demo_ai_trigger exactly: runs
// simulate_transport rendered (hidden window + start/end screenshots) then
// headless from a FRESH World, and byte-compares the two witness logs +
// hashes. Exit 0 iff both runs are byte-identical AND both ok. NO hardcoded
// hash literal -- compute-and-self-compare; the controller pins the observed
// value in the README gate table.
int run_demo_transport() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_transport_start.png";
    const std::string end_png = scratch + "demo_transport_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    TransportAssets ta = load_transport_assets(dir, ra2, ua.pal);
    if (!ta.ok) {
        std::cerr << "[demo-transport] FAILED: transport art unavailable (HTK and FV)\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist{&ta.va};

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 6, pc->y + 4, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoTransportResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-transport)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_transport(a, ua, ta, vlist, /*render=*/true, ren, &tc, &cam,
                                    W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoTransportResult r2 = simulate_transport(a, ua, ta, vlist, /*render=*/false, nullptr,
                                                nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-transport] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d transport_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}
```

- Dispatch, in `main()`: `bool demo_transport = false;` beside the other flags; `else if (arg == "--demo-transport") { demo_transport = true; }` in the arg parse; `if (demo_transport) return run_demo_transport();` immediately after the `--demo-ai-trigger` dispatch line.

**Steps:**

- [ ] **Step 1: Add `transport_fold` + `DemoTransportResult` + `simulate_transport` + `run_demo_transport` + the dispatch** exactly as in Interfaces.

- [ ] **Step 2: Build + run the new gate FOREGROUND, blocking (timeout 600000 ms — the rendered pass runs a few hundred frames; do NOT background, do NOT pipe through Select-Object):**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-transport
```

Expected: exit 0; witness chain shows `[transport] variant=HTK passengers=5 size_limit=2 size=3 gis=6` (this install), 5 `[cargo] board` lines with `count=1/5 .. 5/5` (the exact-fill COUNT boundary [RE-GATE APPLIED: transport-cargo.md §1]), exactly one `[cargo] refuse ... (full)`, a `[cargo] carry arrived=...cargo=5`, 5 `[cargo] eject` lines in LIFO order (the REVERSE of the boarding order [RE-GATE APPLIED: transport-cargo.md §6]) with `order=1..5`, no WEDGE lines, and a final `deterministic=yes lines=N transport_hash=<16 hex>`. Run it a SECOND time and confirm the hash value repeats across processes.

- [ ] **Step 3: Wedge-guard proof (negative check, FOREGROUND).** Temporarily set `const int n_pass = cap;` (no oversubscription), rebuild, run — expected exit 1 with `[transport] WEDGE: no capacity refusal` (proves the gate cannot pass vacuously). REVERT to `cap + 1`, rebuild, re-run, expected exit 0 again. Do not commit the temporary edit.

- [ ] **Step 4: ALL NINE demos FOREGROUND, blocking (timeout 600000 ms each) — the eight legacy pins byte-identical + the new gate deterministic:**

```powershell
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
```

Expected: the eight legacy pins EXACT (values in Global Constraints); `--demo-transport` `deterministic=yes` exit 0.

- [ ] **Step 5: Full suite FOREGROUND, blocking.** `D:\ra2-android\build\ra2assets_tests.exe` — expected 0 failed.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "feat(demo): --demo-transport 9th byte-identity gate - scripted load/carry/unload/resume, transport_hash, wedge guards

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

- [ ] **Step 7 (CONTROLLER, not the implementer): pin + eyeball.** Record the observed `transport_hash` value (it goes into Task 6's README gate row — never a literal in code), eyeball `demo_transport_start.png`/`demo_transport_end.png` in the scratchpad (transport + GI squad visible at start; GIs redeployed around the arrived transport at end), and confirm which variant (HTK vs FV) this install ran — the README documents it.

---

### Task 6: Finalize — full suite both modes, nine gates, arm64 nm-verify, README

**Files:**
- Modify: `README.md`; `android/app/jni/CMakeLists.txt` ONLY if it lists engine sources beyond `cmake/ra2_sources.cmake` (verify — `cargo.cpp` should flow in via the shared list, the same way `ai_trigger.cpp` did).

**Steps:**

- [ ] **Step 1: Full test suite, both modes, FOREGROUND, blocking (timeout 600000 ms):**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: 0 failed in both; the install-gated cases (`World: cargo*`, the new Input transport case) skip clean when the dir is absent; the pure `cargo:` cases run in BOTH modes (host-only, hand-built Units).

- [ ] **Step 2: ALL NINE demos FOREGROUND, blocking (timeout 600000 ms each) — the eight legacy pins byte-identical + the new pin stable:**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
```

Expected, explicitly all eight legacy: `--demo-combat` `deterministic=yes logs=78`; `--demo-tank` `deterministic=yes logs=144`; `--demo-base` `hash=5273b5e71ca009e1`; `--demo-econ` `econ_hash=7fed96cb98b68017`; `--demo-prod` `prod_hash=f0f4e1207ecd713c`; `--demo-ai-base` `ai_base_hash=cd1f7c6a29933f0a`; `--demo-ai-team` `ai_team_hash=312e92ed862a5e74`; `--demo-ai-trigger` `ai_trigger_hash=8334a6ca1531cabc`; plus `--demo-transport` `deterministic=yes` exit 0 with the transport_hash equal to Task 5 Step 7's controller-pinned value.

- [ ] **Step 3: arm64 NDK build FOREGROUND, blocking (timeout 600000 ms):**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Then nm-verify the new cargo symbols in the arm64 `libmain.so`:

```powershell
& "D:\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe" `
  (Get-ChildItem -Recurse -Filter libmain.so D:\ra2-android\android\app\build | Where-Object { $_.FullName -match "arm64" } | Select-Object -First 1).FullName `
  | Select-String -Pattern "cargo_total_size|can_load|eject_cell|next_eject|order_enter_transport|step_cargo"
```

(The `Select-Object`/`Select-String` here filter a CMDLET pipeline over an already-buffered file listing/nm output stream — this is not the forbidden pattern of piping a LIVE native exe's stdout into `Select-Object -First N`, which stops draining and false-hangs; llvm-nm writes bounded output and runs to completion.) Expected: at least one mangled/demangled match per symbol (nonempty output for all six).

- [ ] **Step 4: README — CB-1 section.** Add a "CB-1 — Transports/Cargo" section after the AI-3 section documenting: the pure `engine/cargo` module (capacity is a passenger COUNT — `can_load` gates on cargo count < `Passengers=` plus the per-passenger `Size <= SizeLimit` cap; `cargo_total_size` is the pip-only Σ-Size metric — the original `GetTotalSize` has no capacity caller; LIFO `next_eject` — the original container is a head-pushed/head-popped stack; and `eject_cell` — the facing±90° scatter picker recovered from the original unload, alternating sides per eject with a nearest-free-neighbour fallback; `factory.cpp`/`exit_cell` UNTOUCHED — the original unload does NOT reuse the KickOutUnit ring, so the pre-hunt extraction was dropped at RE-GATE); the World integration (`order_enter_transport` batching movers through `assign_formation`, boarding at Chebyshev adjacency with capacity re-checked and refusals witnessed, carried units OFF-MAP via one added clause on every corpse-skip — no movement/combat/targeting/selection/render/AI-recruitment — with STALE coords by design, `step_cargo` firewalled behind `cargo_any_` and inserted between `step_units` and `step_turrets`, one LIFO eject per logic frame via `eject_cell` with occupancy claimed before combat targeting, boxed-in HOLD mirroring the original's put-back-and-retry, transport death tombstoning its cargo — no OpenTopped survival branch exists in the original); the wiring (HTK art-guarded with FV fallback, the right-click enter gesture, the `D` unload key, the loaded pip row reusing the HP-bar chrome, Enter/LeaveTransportSound through the guarded audio hook); and the byte-identity discipline (world_hash untouched — strategy (a); the eight legacy gates re-verified at the additive Task-2 commit and at finalize). Update the "Determinism gates (permanent)" intro from "eight headless demos" to "nine headless demos" (the first EIGHT are the legacy gates; `--demo-transport` is the new CB-1 gate).

Add the CLI bullet after the `--demo-ai-trigger` bullet:

```
- `ra2engine.exe --demo-transport` — the CB-1 Transports/Cargo integration
  gate: spawns one HTK Flak Track (art-guarded; FV IFV fallback with the
  script scaled to capacity 1) plus capacity+1 GIs, orders all GIs aboard
  (capacity is a passenger COUNT — one board per arrival, the surplus GI is
  REFUSED at the full transport; the exact-fill boundary and the refusal
  both witnessed), carries them to a distant cell (no passenger rendered or
  occupying a cell), unloads one passenger per logic frame in LIFO order
  (last in, first out — the original's head-pushed passenger stack) onto
  facing±90° scatter cells that alternate sides per eject (`eject_cell`;
  nearest-free-neighbour fallback, boxed-in HOLD), then runs a fixed
  resume window — and hashes cargo membership + positions at four
  checkpoints (`transport_hash`). Runs twice (rendered + headless); both
  runs must produce identical witness lines and hash; wedge guards exit
  non-zero if no board, no eject, no capacity refusal, or a non-LIFO eject
  order ever happens.
```

Add the gate row (the hash value is Task 5 Step 7's controller-pinned observation, plus which variant this install ran):

```
| `--demo-transport` | `transport_hash=<pinned value from Task 5 Step 7>` | `deterministic=yes`; variant=HTK (Passengers=5 SizeLimit=2 on this install; FV fallback documented if art ever regresses); 5 boards to the exact-fill COUNT boundary + 1 refusal at full; carry with zero carried-unit draw/occupancy folded into the hash; 5 LIFO ejects one-per-frame onto facing±90° scatter cells (`eject_cell`); wedge guards armed (`docs/re/transport-cargo.md`) |
```

Add to the "Deviations we take" pattern (a CB-1 list, each item cited to transport-cargo.md — mirroring the hunt doc's own "Deviations we will take", adjudicated at RE-GATE 0cef283):
  - **Boarding at Chebyshev adjacency** — the original boards on CO-LOCATION (the passenger paths onto the transport's own tile, `FUN_004D8ED0` case 0x12); our one-unit-per-cell occupancy has no "walk onto an occupied cell" notion, so adjacency ≤ 1 is the adjudicated deviation (same reachable set, deterministic) [transport-cargo.md §3].
  - **Deterministic ordering with NO RNG** — the original's container ops consult no RNG (the suspected `FUN_007C4D00` is ftol, not a draw); the last slot goes to whichever boarder's arrival is processed first, so our ascending-unit-index boarding tie-break is the faithful analogue [transport-cargo.md §6].
  - **Eject fine-placement fallback** — the eject DIRECTION (facing ±90°, sides alternating per eject) and the one-per-tick cadence are the RECOVERED behavior; `FUN_00481090`'s exact fine placement is unpinned, so a blocked scatter cell falls back to the first free cell of the immediate neighbourhood (deterministic row-major, r=1 then r=2). The KickOutUnit `DAT_0089E5F8` 8-ring is NOT shared with unload — it stays `exit_cell`-only [transport-cargo.md §4].
  - **No DeployTime deploy-start delay** — `DeployTime` is a real countdown timer in the original (`FUN_004A5150`) but its scale/application point is unpinned; CB-1 begins ejecting immediately at one-per-tick (documented carry-forward) [transport-cargo.md §4].
  - **Same-house loading only** — the original gate is `IsAlliedWith` (same house OR same side OR ally bitmask, `FUN_004F98B0`); allies are unmodeled in CB-1 (one house per side), and `can_load`'s house clause is where allied loading would attach [transport-cargo.md §2].
  - **No nested loaded transports** — `can_load` rejects a passenger with non-empty cargo (data-unreachable with HTK `SizeLimit=2` < vehicle `Size=3`) [spec Out-of-scope; transport-cargo.md deviations].
  - **Passenger coords are STALE while carried** (stamped on eject/death); the original limbos carried passengers the same way — they live only in the passenger list [transport-cargo.md notes].
  - **Transport-death default**: cargo dies with the transport — CONFIRMED, three sweep sites, no survival branch; `OpenTopped` grants NO death survival (it governs fire-out while alive — an RE-documented carry-forward for the fire-out phase, not CB-1 code) [transport-cargo.md §5].
  - **Sub-cell eject lean dropped** at our cell resolution (the established `ExitCoord` divergence); the ±90° scatter side survives as the CELL choice + the outward eject facing [transport-cargo.md §4].
  - **Enter cursor art / full pip row rendering are cosmetic**: default cursor; minimal tick-rect indicator filled with the SIZE-SUM (`cargo_total_size` — the original `GetTotalSize` is pip-only, capacity is the COUNT) [transport-cargo.md §1].
  - **AI use of transports stays inert**: ScriptType opcodes 0x2e/0x2f remain inert in the AI-2 interpreter; flipping them implementable atop this subsystem is a recorded carry-forward.

- [ ] **Step 5: Commit.**

```powershell
git -C "D:\ra2-android" add README.md
git -C "D:\ra2-android" commit -m "docs: CB-1 transports/cargo README - 9th gate row (--demo-transport), CLI bullet, deviations

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

(If Step 3's verify found `android/app/jni/CMakeLists.txt` needed an explicit source line after all, include it in this commit and say so in the message body.)

---

## Self-Review (performed at write time; RE-RUN at the RE-GATE amendment, 0cef283 — cross-task signature consistency re-checked against the amended COUNT/LIFO/eject_cell surface)

1. **Spec coverage:**
   - Spec §1 (Ghidra hunt, six items) → Task 1 questions 1–6 verbatim, each landing CONFIRMED/PROBABLE/[RE-FALLBACK] with a located-functions table + confidence table + deviations section; the REAL rulesmd.ini extracted via ra2extract with the [HTK]/[FV]/[E1]/[DRON] + [General]/[AudioVisual] transport keys recorded verbatim in scratch; content-anchored headless sweep on the `"Passengers"`/`"SizeLimit"`/`"Size"`/`"PipScale"`/`"OpenTopped"`/`"EnterTransportSound"`/`"LeaveTransportSound"`/`"DeployTime"` strings, xref-walked from the production.md entry map (`FUN_00443BC0` KickOutUnit / `DAT_0089E5F8` / `FUN_0047C430` / `FUN_0045FE40` / the RNG pair); Step 7 is the controller RE-GATE amendment step with the explicit confirm/replace list (capacity formula + defaults, predicate chain, boarding trigger + remove order, eject cadence/facing/HOLD, death disposition, ordering, demo expectations) committed BEFORE Task 2 dispatch. OpenTopped mapping-for-later noted in item 5 (spec Out-of-scope requirement).
   - Spec §2 (architecture + data model) → the 6-task spine mirrors §2's list exactly (hunt → pure cargo → World → main.cpp/input → gate → finalize); the tripwire holds (6 ≤ ~9; the guard sweep is a 12-site checklist with an explicit STOP if it fans out). The approved data model lands verbatim: UnitCombat passengers/size_limit/size resolved once by main.cpp (Task 4 resolve_combat_unit), Unit fields all-inert (`passengers_max=0`, `size_limit=0`, `size=1`, `cargo` empty, `cargo_of=-1`, `enter_target=-1`, `unload_pending=false`, `eject_counter=0`) copied at spawn (Task 3 add_combat_unit), the pip Σ-Size DERIVED (cargo_total_size — no stored counter; capacity itself is the COUNT of the cargo list per the RE-GATE amendment), unit indices as tombstone-stable links, `engine/cargo.{hpp,cpp}` registered in BOTH cmake files (Task 2), `can_load` exactly the §2-listed member set including the no-nested-transports simplification (house clause = documented IsAlliedWith deviation).
   - Spec §3 (load flow) → Task 4's gesture (own alive non-dying transport by `passengers_max > 0`, selection containing eligible non-transport units, default cursor divergence noted) + Task 3's `order_enter_transport` (per-unit can_load NOW with boarding re-check, attack-target cleared, assign_formation batching around the transport cell) + `step_cargo` boarding pass (Chebyshev <= 1, release_all_of with no kept cell, clear path/selection/targets, push + cargo_of + enter_target=-1, transport-died/full/superseded cancel paths, ascending-index determinism, stopped-and-far re-path via nearest_free_cells).
   - Spec §4 (carry semantics) → the guard-sweep table covers every enumerated site: step_units (1), step_combat both directions (8: valid_target targets; 9: carried attacker) with the attacker-drops-boarding-target via the existing validity check, step_turrets (2), unit_at_cell (3), Input::unit_under (Task 4 site 12), unit_draw_order (4), harvester stepping (10, guarded though data-unreachable), AI ai_idle_units/quarry pickers (5/6/7), plus render_world's two presentation loops (Task 4 sites 11a/11b) and the boarding force-clear of `selected`. Stale-coords-by-design stated at step_cargo and the hold-at-range/anim-override/level-patch non-edits justified explicitly. Transport death: tombstone direct (alive=false, no die anim), coords stamped — the CONFIRMED recovered default (hunt §5: three sweep sites, no OpenTopped survival branch); the one-frame lag vs step_combat documented.
   - Spec §5 (unload flow) → order_unload_selected on selected transports with cargo, any new move/attack order clears unload_pending (order_move_selected + commit_attack_target edits), stationary-transport one-per-frame LIFO eject via `next_eject` + `eject_cell(t.cell_x, t.cell_y, t.body_facing, t.eject_counter, free_cell)` with the SAME free lambda launch_unit uses (walkable + occ_.is_free), coords/fx/fy stamp + occupancy claim + `facing_from_delta` outward eject facing (+ vehicle body/turret facing) + guard anchor = eject cell + `eject_counter` bump + normal idle resume, HOLD-and-retry mirroring the original's put-back (and launch_unit's -1 precedent). Task 2 is purely additive — `factory.cpp`/`exit_cell` untouched (the ring extraction was DROPPED at RE-GATE) — with the 8-gate re-verify still run at the additive commit (Task 2 Step 3).
   - Spec §6 (rendering/HUD/audio) → unit_draw_order guard (no passenger render); draw_cargo_pips reusing the HP-bar anchor + SDL_Rect fill (no new font/UI); PipScale RE-documented-not-pixel-matched stated; Enter/LeaveTransportSound via the chrono-sound guarded pattern with [AudioVisual] fallback, marked non-gating cut-first.
   - Spec §7 (gate) → Task 5: capacity+1 GIs (6 at HTK-5; FV fallback scales to 2-at-1 — both give >=1 board AND >=1 refusal), witness lines the spec's shapes amended at RE-GATE to the COUNT model (`[cargo] board unit=N count=K/5`, `refuse (full)`, `carry arrived=(x,y) cargo=5`, `eject unit=N cell=(x,y) order=K`), four fixed fold checkpoints (post-load/post-carry/post-unload/end) over (cell_x, cell_y, alive, cargo_of) per unit + per-transport cargo list/pip-total-size/cell + draw-order size + the carried-owns-no-cells occupancy audit, FNV via the established fnv_mix + world_hash's 0xcbf29ce484222325 basis, two-run compute-and-self-compare with NO pinned literal (controller pins in README), wedge guards (board/eject/refusal/LIFO-reverse-boarding) exit 1, rendered ≡ headless byte-compare with the FIX-3 no-extra-step end capture. Step 3 proves the wedge guard fires (negative check, reverted).
   - Spec §8 (byte-identity) → copied into Global Constraints; enforced structurally (inert defaults, firewall, strategy (a) world_hash untouched, guard clauses on existing conditions, pure insertion between step_units/step_turrets) and empirically (8-gate re-runs in Tasks 2, 3, 4, and the 9-demo runs in Tasks 5, 6).
   - Spec §9 (testing) → pure doctests (COUNT exact fill + 6th refused, the COUNT-vs-size-sum discriminator — four Size-2 riders sum to 8 > Passengers=5 yet a fifth still boards and a sixth is refused by count alone, Size=2 = ONE count slot / TWO pips via cargo_total_size, SizeLimit refusal — Size 3 > SizeLimit 2 while Size 2 admits, full can_load matrix incl. carried-transport/nested-cargo/self/house/dead/dying/out-of-range, eject_cell scatter cell + side alternation by counter parity + facing-space anchors + neighbourhood fallback r=1/r=2 + all-blocked {-1,-1} + determinism, LIFO next_eject — board A..E, eject E..A) — all host-only hand-built; World tests (board releases ALL occupancy + audit green + unpickable/untargetable/undrawn + force-cleared selection, eject = eject_cell-identical cell + occupancy claim + coords/facing/guard stamp + eject_counter bump + one-per-frame + LIFO reverse-boarding order, boxed-in HOLD then resume via a freed outer neighbourhood cell, transport death tombstones cargo + enter cancel, capacity refusal + order supersession both directions); integration (the gate with wedge guards + two-run self-compare); regression (8 gates at the additive Task-2 commit AND at finalize, full suite both modes, arm64 nm-verify of the six cargo symbols). Suite stays scoped per task (name-filtered doctest runs) per the shared-fixture discipline.
   - Spec §Out-of-scope → nothing in any task implements fire-out/OpenTopped (hunt notes only), naval/aircraft/garrison/BFRT, nested transports (rejected in can_load), AI transport opcodes (README records the carry-forward), enter-cursor art, or sub-cell lean.
2. **Placeholder scan:** no TODO/TBD/"similar to Task N"/"add error handling" in any operative step; every code step shows complete code; every run step shows the exact PowerShell command + expected output. Deliberate non-code references that remain are legitimate anchors into the EXISTING tree (e.g. "after the harvester refinery-return branch", "next to chrono_out/chrono_in") with the inserted code given in full. All numeric semantics are tagged `[RE-GATE APPLIED: transport-cargo.md §N]` (amended — STATUS: APPLIED (0cef283); every tag now carries the hunt's recovered value or a hunt-adjudicated documented deviation) or cite production.md's already-confirmed findings. The transport_hash pin is extract-then-pin by the controller (Task 5 Step 7 → Task 6 Step 4), deliberately never a literal in code.
3. **Type/signature consistency (checked across tasks):**
   - `int cargo_total_size(const std::vector<Unit>&, const Unit&)`, `bool can_load(const std::vector<Unit>&, int, int)`, `int next_eject(const Unit&)`, `Cellxy eject_cell(int, int, double, int, const std::function<bool(int,int)>&)` — defined Task 2 (`engine/cargo.{hpp,cpp}`), consumed with exactly these signatures by Task 2 tests, Task 3 (`step_cargo`: `can_load(units_, ti, i)` / `next_eject(t)` / `eject_cell(t.cell_x, t.cell_y, t.body_facing, t.eject_counter, free_cell)`; `order_enter_transport`: `can_load`), Task 3 tests (the `eject_cell` witness with the same five arguments), Task 4 (render_world pips: `cargo_total_size(us, u)`), Task 5 (`transport_fold`: `cargo_total_size(us, u)`). RE-GATE renames/replacements verified consistent at EVERY site: `cargo_slots_used` exists nowhere (renamed `cargo_total_size`, demoted to pip-only — no capacity call site), `first_free_ring_cell` exists nowhere (extraction DROPPED; `exit_cell` untouched and no longer consumed by this plan), `next_eject` is LIFO (`cargo.back()`) at its definition, both pure tests, the Task 3 world test, and the Task 5 wedge (`lifo_expect` reverse-boarding compare), and `eject_cell`'s `double transport_facing_u16` parameter matches `Unit::body_facing`'s type (double, spawn default 32768 = South) at the one `step_cargo` call site and all test call sites. `can_load(transport, passenger)` still takes `(units, transport_idx, passenger_idx)` (the COUNT gate reads `t.cargo.size()`; every call site holds indices).
   - `Unit::{passengers_max,size_limit,size,cargo,cargo_of,enter_target,unload_pending,eject_counter}` — defined Task 2 (world.hpp), consumed by Tasks 2–5 with these exact names (`eject_counter` is read+bumped only by `step_cargo`'s eject pass and read by the Task 3 eject test); `UnitCombat::{passengers,size_limit,size}` — defined Task 3, filled Task 4 (`resolve_combat_unit`), copied in Task 3's `add_combat_unit`.
   - `CargoEvent{kind∈{Board,Refuse,Eject}, unit, transport, x, y}` with the Board/Refuse x=COUNT-after/y=passengers_max and Eject x/y=cell convention — defined Task 3 (world.hpp), produced by `step_cargo` (`static_cast<int>(t.cargo.size())`), consumed by Task 3 world tests, Task 4's audio drain, Task 5's witness formatting (`count=%d/%d`) with the same convention.
   - `World::order_enter_transport(int)`, `World::order_unload_selected()`, `std::vector<CargoEvent> World::take_cargo_events()`, private `void World::step_cargo(double)` — declared+defined Task 3; consumed by Task 4 (input gesture calls `order_enter_transport`; the D key calls `order_unload_selected`; the audio drain calls `take_cargo_events`) and Task 5 (the demo script calls all three).
   - `TransportAssets{va,id,ok}` + `TransportAssets load_transport_assets(const std::string&, const ra2assets::MixFile&, const ra2assets::Palette&)` — defined Task 4, consumed by Task 4 (run_interactive) and Task 5 (run_demo_transport) with the same three-argument call.
   - `void draw_cargo_pips(void*, const Unit&, int, int, const Camera&, int, int)` — declared Task 4 (iso_renderer.hpp), defined Task 4 (iso_renderer.cpp), called from render_world with `(ren, u, cargo_total_size(us, u), u.passengers_max, cam, W, H)` (pips_filled = the Σ-Size pip metric, pips_max = the COUNT capacity — deliberately different quantities per the hunt's pip-only GetTotalSize finding).
   - `uint64_t transport_fold(uint64_t, const ra2engine::World&)` + `DemoTransportResult` + `simulate_transport(Assets&, UnitAssets&, const TransportAssets&, const std::vector<const VehicleAssets*>&, bool, SDL_Renderer*, TextureCache*, const Camera*, int, int, const std::string&, const std::string&)` + `int run_demo_transport()` — defined and consumed within Task 5 only.
   - Existing signatures used verbatim from the current tree (verified against source at write time; re-verified at the amendment): `assign_formation(Cellxy, const std::vector<Cellxy>&, const std::vector<std::uint8_t>&, int, int, const Occupancy&)` and `nearest_free_cells(Cellxy, int, const std::vector<uint8_t>&, int, int, const Occupancy&)` (group_move.hpp — the boarding re-path), `Occupancy::{at,is_free,set,release_all_of,width,height}`, `facing_from_delta(double,double)` / `facing_u16_from_delta(double,double)`, `fnv_mix(uint64_t,uint64_t)` + the `0xcbf29ce484222325ull` basis, `unit_draw_order(const std::vector<Unit>&)`, `load_vehicle_assets(dir, ra2, unit_pal, id)` → `VehicleAssets{ok, combat, ...}` (move-only via unique_ptr — moved, never copied), `Rules::section_double(section, key, fallback)`, `Ini::get(section, key, fallback)`, `resolve_sound_event(audio, sounds, idx, bag, k)` at its init_audio call shape, `spawn_squad(world, x, y, count, house, combat)`, `build_world(a, ua, 0)`, `pick_start_cell`, `capture_png(ren, path, W, H)`, `render_world(ren, tc, a, ua, world, cam, W, H, input, vlist, ba, ore_art)` (trailing defaults), `draw_hp_bar(..., width_px=0)` and iso_renderer.cpp's file-local `tile_center_screen`/`kHpBarW/H/Up/Gap` (draw_cargo_pips lives in the same TU so it may use them), `load_real_map_w`/`gi_stats`/`cons_stats` (tests/test_world.cpp anon namespace), `load_real_map`/`ground_px_for_unit`/`right_click`/`left_click`/`gi_combat` (tests/test_input.cpp anon namespace; `gi_combat`'s positional aggregate init stays valid because the new UnitCombat fields trail with defaults), doctest `--test-case` name filters.
   - Byte-identity: Task 2 = a purely additive new TU + inert trailing Unit fields with `factory.cpp` untouched (gates re-run in-task anyway); Task 3's live-path changes are all fenced (`cargo_any_`) or inert-writes/false-clauses (gates re-run); Task 4's resolution stamps unread-outside-cargo fields and interactive-only wiring (gates re-run); Task 5 adds a new demo beside the others (all nine run); Task 6 re-asserts everything + arm64.
4. **FOREGROUND discipline:** every build/test/demo step in every task explicitly orders a foreground, synchronous, blocking run with a generous timeout (up to 600000 ms for rendered demos), exes run from PowerShell (never Git Bash), and no step ever pipes a live native exe through `Select-Object -First N` (the one llvm-nm pipeline is bounded-output and annotated as such).
