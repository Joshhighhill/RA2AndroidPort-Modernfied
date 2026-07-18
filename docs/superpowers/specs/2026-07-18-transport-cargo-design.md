# CB-1 Transports/Cargo Phase Design — Units Ride Inside Other Units

**Date:** 2026-07-18
**Status:** Approved (controller-scoped, formalized autonomously)
**Depends on:** the complete AI arc and everything under it — Unit Production
(`--demo-prod` prod_hash=f0f4e1207ecd713c; CB-1 reuses its `KickOutUnit`
exit-cell ring for unload), Economy (`--demo-econ`
econ_hash=7fed96cb98b68017), Buildings (`--demo-base` 5273b5e71ca009e1),
combat (`--demo-combat` logs=78, `--demo-tank` logs=144), and the three AI
phases (`--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a, `--demo-ai-team`
ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc).

## Goal

A generic cargo/passenger subsystem: units LOAD into a transport (right-click
a friendly transport with a passenger selected, path there, board), are
CARRIED (off-map — not rendered, not targetable, holding no cell — moving
with the transport), and are UNLOADED (ejected one-per-free-cell via the
SAME 8-neighbour ring walk unit production's `KickOutUnit` uses, HOLD when
boxed in), demonstrated deterministically on a real multi-capacity ground
transport and gated by the **9th permanent byte-identity gate
`--demo-transport`**. This is **CB-1**, the first phase of the
content-breadth arc (CB-1 Transports/Cargo → CB-2 Aircraft → CB-3 Naval →
CB-4 Superweapons → CB-5 Tech-tree): the full AI arc is done, and cargo is
the substrate several later phases stand on (naval landing craft, aircraft
delivery, garrisons). Unlike the AI phases, cargo lives in the CORE unit
sim — it cannot hide behind the `ai_any_` firewall — so the byte-identity
discipline (§8) is itself a first-class design deliverable. One
spec→plan→execute cycle, ~6–7 tasks, the same spine as every prior phase.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Data model | **Cargo list on the transport `Unit` + `cargo_of` back-link on the passenger**; passengers held off-map (no render, no occupancy, no combat, no pathing); capacity is a **Size-weighted slot pool** (`Passengers=` slots, each passenger consumes `Size` slots, `SizeLimit=` gates the largest passenger) — pre-hunt expectation, exact semantics are §1 item 1 |
| Transport choice | **HTK (Soviet Flak Track)** — the brief's "HALF" id corrected to the real rulesmd id: `[HTK] Passengers=5, SizeLimit=2, Size=3, Category=Transport`, ground Drive locomotor, and its art is PRESENT on this install (htk.vxl/htk.hva/htktur.vxl/htktur.hva probed in ra2.mix/local.mix) — renderable via the existing voxel_raster/facing_cache pipeline. **FV (IFV, Passengers=1, already producible)** is the art-guard fallback; multi-passenger logic is fully implemented regardless and capacity>1 / Size>1 covered by pure tests |
| Passengers (demo) | **E1 (GI, Size=1)** — already loaded/fieldable; Size=2 passengers (DRON — art absent on this install, probed) exercise Size-weighting in pure tests only |
| Load | Player right-clicks an own transport with eligible units selected → enter order; passenger paths to the transport and **boards at Chebyshev adjacency** (pre-hunt trigger point; §1 item 3), subject to remaining capacity |
| Carry | `cargo_of >= 0` = off-map: excluded from render, selection, targeting, movement, occupancy, AI recruitment; passenger coords are stale while carried (no reader consumes them) and stamped on eject/death |
| Unload | An unload order on the transport ejects passengers **one per logic frame** (pre-hunt cadence; §1 item 4) onto the first free cell of the SAME Chebyshev-ring walk as `exit_cell` (`KickOutUnit`/`DAT_0089E5F8`), FIFO (pre-hunt order; §1 item 6), **HOLD-and-retry when boxed in** (mirror production's HOLD) |
| Rendering/HUD | No passenger render while loaded; a minimal loaded-count indicator on the selected transport reusing EXISTING chrome (no new font/UI systems); Enter/LeaveTransportSound wired through the existing guarded audio hook (nice-to-have, non-gating) |
| Verification | **New deterministic gate `--demo-transport`** (the 9th permanent gate) + `transport_hash` (FNV, two-run compute-and-self-compare, no pinned literal) + **wedge guards (§7): a load AND an unload AND a capacity refusal must actually happen, else exit 1** |
| Byte-identity | **Strategy (a)**: cargo fields excluded from `world_hash` (it already reads only cell/hp/alive per unit — untouched); `step_cargo` early-returns behind a `cargo_any_` firewall; every new guard in existing loops is provably false in every existing demo (§8) |

## §1 What the Ghidra hunt must recover (→ `docs/re/transport-cargo.md`)

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001
Ghidra project at D:\ra2-re, headless analyzeHeadless.bat,
JAVA_HOME="C:\Program Files\Java\jdk-21") plus the real rulesmd.ini,
producing `docs/re/transport-cargo.md` — the calibration authority BEFORE
any code, with a located-functions table, a "deviations we take" section
(mirroring `production.md`/`ai-team.md`), and each item landing CONFIRMED /
PROBABLE / [RE-FALLBACK] with function address + decompile reasoning.
Anchor on the INI-key strings `"Passengers"`, `"SizeLimit"`, `"Size"`,
`"PipScale"`, `"OpenTopped"`, `"EnterTransportSound"`,
`"LeaveTransportSound"`, the passenger/cargo methods on
FootClass/TechnoClass (enter/load, `Unload`, the "can this unit enter that
transport" predicate, transport-death disposition), and the already-pinned
`KickOutUnit` `FUN_00443BC0` / `DAT_0089E5F8` ring (production.md §1.3) as
the entry map for the eject-cell walk. YRpp/Ares addresses are UNTRUSTED —
prose hints only, verify by content. Six items:

1. **Capacity accounting**: is the load gate a passenger COUNT
   (`Passengers=N` bodies), a Size-sum against the `Passengers` pool (each
   passenger consumes `Size` slots), or both? The exact `Passengers` /
   `Size` / `SizeLimit` semantics, which value gates a load, and the
   DEFAULTS when a key is absent (read from the real rulesmd on this
   install: `[HTK] Passengers=5 SizeLimit=2 Size=3`, `[FV] Passengers=1
   SizeLimit=1 Size=3`, infantry `Size=1`, `DRON Size=2`, vehicles
   `Size=3` — values are ground truth already; the hunt pins the FORMULA).
2. **The "can X enter Y" predicate**: what marks a unit as a transport
   (`Passengers>0`?), passenger eligibility (infantry vs vehicles vs the
   `SizeLimit` test only), remaining-capacity check, ownership (same house
   only, or allies too), and the proximity requirement (same cell?
   adjacency?) — the full boolean chain, in evaluation order.
3. **Load mechanics**: the approach path target (the transport's own cell
   vs an adjacent cell), the exact trigger point where the passenger
   leaves the map (adjacent? on the transport's cell? a docking radius?),
   and the remove-from-map + occupancy-release order (which happens
   first). Our grid boards at Chebyshev adjacency (§3) — if the original
   requires walking ONTO the transport's cell, that lands as a documented
   deviation adjudicated at RE-GATE, not silently absorbed.
4. **Unload mechanics**: CONFIRM the eject cell selection is the
   `KickOutUnit`/`DAT_0089E5F8` 8-ring walk (or recover the actual
   routine); one-passenger-per-cell; the eject CADENCE (all at once, one
   per frame, or a timed delay — `[HTK] DeployTime=.022` is suspected to
   be this knob and must be pinned); facing on eject; and what happens
   when no free cell exists (HOLD vs order-abort vs partial unload).
5. **Transport-death passenger disposition**: do passengers die with the
   transport (expected default), always, or do `OpenTopped`/other flags
   change survival? Where do survivors (if any) get placed? CB-1
   implements the recovered DEFAULT; exceptions are RE-documented and
   carried forward.
6. **Deterministic ordering**: the cargo container's order semantics —
   eject order relative to boarding order (FIFO vs LIFO), and any other
   sim-visible ordering the original relies on (e.g. which of several
   simultaneous boarders wins the last slot).

Calibration discipline (unchanged): every engine constant cites this doc.
**All numeric semantics in this spec are pre-hunt expectations, not ground
truth** (the rulesmd VALUES quoted above were read from the real file on
this install and are committable; their INTERPRETATION is the hunt's). A
controller **RE-GATE** amendment step precedes implementation — the hunt's
findings amend the plan and are committed BEFORE dependent dispatch, with
`[RE-GATE APPLIED]` markers threaded through every dependent task (the
ai_base.hpp/factory.hpp precedent). **[RE-FALLBACK]**: anything the hunt
cannot pin becomes a plan-amendment gate (present options to the human) —
never an uncalibrated guess.

## §2 Architecture & recovered data model

Consistent with every prior phase: a pure SDL-free engine module + thin
World integration + a deterministic demo in main.cpp. ~6–7 tasks:

- **Task 1**: Ghidra hunt → `docs/re/transport-cargo.md` (RE-GATE before
  implementation; §1 above).
- **Task 2**: pure engine `engine/cargo.{hpp,cpp}` — capacity accounting +
  `can_load` predicate + the shared ring-walk extraction (§5) — doctest
  coverage on hand-built units, no World/SDL.
- **Task 3**: World integration — Unit cargo fields, `order_enter_transport`
  / `order_unload`, `step_cargo`, the carried-unit guards (§4), transport
  death disposition.
- **Task 4**: main.cpp + input wiring — HTK roster/art load (art-guarded, FV
  fallback), rules resolution of `Passengers`/`SizeLimit`/`Size`, the
  right-click enter gesture + unload key, the loaded-count indicator.
- **Task 5**: `--demo-transport` gate (the 9th) + `transport_hash` + witness
  lines + wedge guards.
- **Task 6**: finalize — suites both modes, 8 existing gates byte-identical,
  arm64 NDK nm-verify of the new cargo symbols, README (CB-1 section + 9th
  gate row + deviations note citing `docs/re/transport-cargo.md`).

**Decomposition tripwire**: if the carry guards (§4) fan out much wider than
the enumerated sites and push the plan past ~9 tasks, the guard sweep splits
into its own reviewed task with a per-site checklist.

### Data model

Field shapes below are the approved design; the Task 1 hunt CONFIRMS or
REPLACES the semantics (RE-GATE), and code/tests are written against the
amended form.

- **`UnitCombat` extension** (resolved once from Rules by main.cpp, like
  every existing stat): `passengers` (slot pool; 0 = not a transport),
  `size_limit` (largest admissible passenger `Size`; the recovered default
  when the key is absent), `size` (the unit's own bulk; recovered default —
  rulesmd shows infantry 1, DRON 2, vehicles 3).
- **`Unit` extension** (all defaults INERT — §8): `passengers_max = 0`,
  `size_limit = 0`, `size = 1` (copied from UnitCombat at spawn);
  `cargo` (vector<int> of unit indices, boarding order — empty);
  `cargo_of = -1` (index of the transport carrying this unit);
  `enter_target = -1` (pending enter order → transport index);
  `unload_pending = false` (pending unload order on a transport).
  Slots-used is DERIVED (sum of `units_[i].size` over `cargo`) — no
  stored counter to drift out of sync.
- **Pure module `engine/cargo.{hpp,cpp}`** (registered in BOTH
  `cmake/ra2_sources.cmake` AND `CMakeLists.txt`): `slots_used(...)`,
  `can_load(transport, passenger)` — the §1-item-2 predicate over plain
  Unit fields (transport has capacity remaining for `passenger.size`,
  `passenger.size <= size_limit`, same house, both alive/non-dying, not
  self, passenger not already carried, passenger's own cargo empty — the
  no-nested-transports simplification, see Out-of-scope) — and
  `first_free_ring_cell(...)` (§5). All doctest-testable on hand-built
  Units; every recovered constant cites `docs/re/transport-cargo.md`.
- **Unit indices as links**: `cargo`/`cargo_of`/`enter_target` store unit
  INDICES — safe because units are tombstoned, never erased (indices are
  stable for a World's lifetime, the same invariant `target` already
  relies on).

## §3 Load flow

Player-facing (house 0), mirroring the existing order verbs:

1. **Gesture**: `Input` gets `set_transport_types(...)`-style knowledge the
   same way the refinery-return gesture does — a right-click on an OWN,
   alive, non-dying unit that is a transport (`passengers_max > 0`) with a
   selection containing eligible non-transport units issues
   `World::order_enter_transport(t_idx)` instead of a group move. (The
   original's "enter" cursor is cosmetic; our cursor stays default — noted
   divergence, no new UI systems.)
2. **`order_enter_transport(t_idx)`**: for each selected house-0, alive,
   non-dying unit that passes `can_load` NOW (capacity is re-checked at
   boarding — orders may oversubscribe): set `enter_target = t_idx`, clear
   any attack target, and path toward the transport through the SAME
   approach machinery attack pursuit already uses — enter movers batch per
   transport into `assign_formation` around the transport's cell, so each
   passenger gets a DISTINCT free nearby cell (a raw order_unit_move to
   the transport's own occupied cell would cancel as unreachable).
3. **Boarding (in `step_cargo`)**: each frame, a unit with
   `enter_target >= 0` whose Chebyshev distance to the transport is <= 1
   boards IF `can_load` still passes: release ALL its occupancy
   (`release_all_of` with no kept cell), clear path/selection/targets,
   push its index onto the transport's `cargo`, set `cargo_of`,
   `enter_target = -1`. If the transport moved, re-path on the same
   staggered cadence pursuit already uses; if the transport died, became
   full, or the order was superseded by any new order, the enter order
   cancels cleanly (unit stands, `enter_target = -1`). Boarding processes
   units in ascending index order (deterministic last-slot winner —
   pre-hunt tie-break, §1 item 6).
4. **Boarding trigger point** is the §1-item-3 recovery. Pre-hunt we board
   at adjacency: our one-unit-per-cell occupancy has no "walk onto an
   occupied cell" notion, so same-cell entry would need a special
   handoff. If the hunt shows same-cell entry is load-bearing, the
   RE-GATE adjudicates (adjacency is the expected accepted deviation —
   same reachable set, deterministic).

## §4 Carry — off-map semantics

A carried unit (`cargo_of >= 0`) is OFF the map. The rule is uniform:
**everything that already skips corpses (`!alive || die_time >= 0`) also
skips carried units** — the same guard, one more clause, false for every
unit in every existing demo (§8). Enumerated sites (the plan's Task 3
checklist; the implementer sweeps for stragglers):

- `step_units` (no movement/animation; owns no occupancy cell),
- `step_combat` + auto-acquire in BOTH directions (cannot fire, cannot be
  targeted; an attacker whose target boards drops it via the existing
  target-validity check growing the clause),
- `step_turrets`, `unit_at_cell` (unpickable), `Input::unit_under`
  (unselectable; boarding force-clears `selected`),
- `unit_draw_order` (not rendered),
- harvester stepping (a carried harvester's mission FSM parks — data-
  unreachable with HTK `SizeLimit=2` vs harvester `Size=3`, guarded
  anyway),
- AI `ai_idle_units` / quarry pickers (not recruitable, not a quarry).

**Coordinates while carried are STALE** (the boarding cell) by design: no
surviving reader consumes them, and not touching them keeps `step_cargo`'s
carry path a no-op for the hash of any demo that never loads cargo. They
are stamped fresh on eject (§5) and on death-in-transit (below). The
passenger "moves with the transport" solely by virtue of being ejected at
the transport's CURRENT position.

**Transport death**: when a transport with cargo dies (die_time flips >= 0
in step_combat), every carried unit dies WITH it — tombstoned directly
(`alive = false`, no die animation — it was never on-screen), coords
stamped to the transport's cell. This is the pre-hunt DEFAULT; §1 item 5
recovers the real disposition and `OpenTopped`-style exceptions land as
RE-documented carry-forwards, not CB-1 code.

## §5 Unload flow — the KickOutUnit ring, reused

**Order**: with an own transport selected, the unload key (`D`, routed by
main.cpp like Escape → placement-cancel) calls
`World::order_unload_selected()` → sets `unload_pending = true` on each
selected transport that has cargo. Any new move/enter order on the
transport clears `unload_pending` (an order supersedes — pre-hunt rule).
The original's trigger is the Deploy/Unload mission (§1 item 4); our key
binding is UI, not sim.

**Eject (in `step_cargo`)**: a stationary transport with `unload_pending`
and non-empty cargo ejects ONE passenger per logic frame (pre-hunt
cadence — §1 item 4 pins the real rate, suspected `DeployTime`): take the
FIRST cargo entry (FIFO — pre-hunt, §1 item 6), run the ring walk from the
transport's cell, and if a free cell exists: stamp the passenger's
cell/fx/fy there, claim occupancy, set `cargo_of = -1`, facing =
`facing_from_delta(transport→cell)` (pre-hunt; §1 item 4 recovers the real
eject facing), and the unit resumes normal idle (guard anchor = eject
cell) — an ejected AI-team member simply rejoins its team's normal
stepping. When the cargo empties, `unload_pending` clears.

**The ring is the SAME ring**: `factory.cpp`'s `exit_cell` body — the
expanding Chebyshev-ring perimeter walk in the pinned deterministic order
(south-first, column-centered, west-before-east; [RE-GATE APPLIED:
production.md §1.3, `KickOutUnit` `FUN_00443BC0` / `DAT_0089E5F8`]) — is
EXTRACTED VERBATIM into a shared pure helper
`first_free_ring_cell(ax, ay, max_r, free)` in `engine/cargo.{hpp,cpp}`;
`exit_cell` becomes a thin delegator (anchor = foundation center,
max_r = max(fw,fh)+1) and unload calls it with anchor = the transport's
cell and max_r = 2 (the 1×1-footprint value of the same formula). The
extraction changes NO behavior — `--demo-prod`/`--demo-ai-*` staying
byte-identical is the proof, re-verified in Task 2 before anything else
lands. The `free` predicate is the SAME lambda `launch_unit` uses
(walkable + `occ_.is_free`, which sees mid-hop reservations).

**HOLD when boxed in**: no free ring cell → the passenger STAYS aboard,
`unload_pending` stays set, and the transport retries every frame —
mirroring production's ready-HOLD (`launch_unit` returning -1). A later
freed cell resumes the unload; witnesses make a HOLD visible in tests.

## §6 Rendering / HUD / audio

- **No passenger render while loaded** — `unit_draw_order` skips
  `cargo_of >= 0` (§4). No new art is loaded for cargo state.
- **Loaded-count indicator, minimal**: when a transport is selected,
  slots-used renders as filled/empty tick rects drawn with the SAME rect
  primitive and placement pass as the existing selected-unit HP bar
  (main.cpp already draws HP bars for alive, selected-or-damaged units) —
  REUSE only, no new font/UI systems. Vanilla's `PipScale=Passengers` pip
  row is the reference look, RE-documented but NOT pixel-matched in CB-1.
- **Audio (nice-to-have, non-gating)**: `EnterTransportSound=` /
  `LeaveTransportSound=` (`[HTK]` carries both) wired through the same
  guarded voice-hook pattern the harvester sounds use; absent audio
  degrades silently. Cut first if Task 4 runs long.

## §7 The `--demo-transport` gate (the 9th), `transport_hash` & witnesses

**Script** (headless or rendered — bit-identical): on the standard demo
map, spawn one HTK transport (art-guarded; FV fallback with the script's
counts scaled to capacity 1) + 6 GIs for house 0, all scripted (no player
input):

1. **LOAD**: order all 6 GIs to enter. With `Passengers=5`/`Size=1`, 5
   board (witness per board: `[cargo] board unit=N slots=K/5`), the 6th is
   REFUSED at capacity (`[cargo] refuse unit=N (full)`) — the exact-fill
   boundary AND the refusal both witnessed.
2. **CARRY**: order the loaded transport to a distant cell; run to
   arrival (`[cargo] carry arrived=(x,y) cargo=5`); assert no passenger
   rendered/occupying (`unit_draw_order` size + occupancy audit fold into
   the hash).
3. **UNLOAD**: issue unload; passengers eject one per frame onto ring
   cells (`[cargo] eject unit=N cell=(x,y) order=K`), FIFO order
   witnessed.
4. **RESUME**: run fixed frames; ejected GIs stand idle/guard normally.

**`transport_hash`**: FNV-1a 64 (the established `fnv_mix` helper) folded
at four fixed checkpoints (post-load, post-carry, post-unload, end) over:
every unit's `(cell_x, cell_y, alive, cargo_of)`, each transport's cargo
list (in order) + derived slots-used, and the transport's cell — cargo
membership + positions across the sequence, exactly the state the phase
adds. Two-run compute-and-self-compare (`deterministic=yes`), NO pinned
literal in code (the `--demo-ai-team`/`--demo-ai-trigger` precedent);
README records the stable value in the gate table once landed.

**Wedge guards** (exit 1, so the gate can never pass vacuously): boards
counted >= 1 AND ejects counted >= 1 AND the capacity refusal observed —
all three hold under the FV fallback too (its scaled script orders 2 GIs
at a 1-slot transport: 1 boards, 1 refused; the README documents which
variant this install runs). Witness log + hash are pure sim state →
rendered and headless runs byte-compare identical.

## §8 Byte-identity discipline (core-sim — the phase's hardest constraint)

The **8 existing permanent gates MUST stay bit-identical**: `--demo-combat`
logs=78, `--demo-tank` logs=144, `--demo-base` 5273b5e71ca009e1,
`--demo-econ` econ_hash=7fed96cb98b68017, `--demo-prod`
prod_hash=f0f4e1207ecd713c, `--demo-ai-base` ai_base_hash=cd1f7c6a29933f0a,
`--demo-ai-team` ai_team_hash=312e92ed862a5e74, `--demo-ai-trigger`
ai_trigger_hash=8334a6ca1531cabc. Cargo cannot hide behind `ai_any_` — the
discipline instead:

- **All new Unit fields default INERT** (`cargo` empty, `cargo_of = -1`,
  `enter_target = -1`, `unload_pending = false`, `passengers_max = 0`).
  No existing demo ever issues a load/unload order and no existing spawn
  path sets `passengers_max > 0`, so no unit ever has cargo state in any
  existing demo.
- **`world_hash` strategy (a) — EXCLUDE, chosen**: `world_hash` reads
  exactly `(cell_x, cell_y, hp, alive)` per unit + the building fields
  (main.cpp `world_hash`) and is NOT touched by this phase; the new cargo
  fields are hashed ONLY by `transport_hash` in the new gate. Justification
  over strategy (b) (include-and-prove-empty): (a) requires zero edits to
  a function five gates hash through, making the no-change argument
  structural rather than empirical; nothing about cargo state is
  meaningful to the legacy demos. The carried-unit design keeps even the
  HASHED fields untouched (coords stale while carried, `alive` only
  changed by the death rule no existing demo triggers).
- **`step_cargo` is firewalled**: a World-level `cargo_any_` bool, set the
  first time an enter/unload order is issued or any unit boards, gates the
  entire function — `if (!cargo_any_) return;` is the only cost every
  existing demo pays. It is inserted as a NEW call between `step_units`
  and `step_turrets` in `World::step` (boarding resolves the frame the
  passenger lands adjacent, ejects claim occupancy before combat
  targeting) — a pure insertion; **no existing unit-step logic is
  reordered**.
- **Every §4 guard is an added clause** (`|| cargo_of >= 0`) on an existing
  corpse-skip condition — provably false for every unit in every existing
  demo (default -1), so control flow is identical instruction-for-
  instruction where it matters: same branches taken, same state written.
- **The §5 ring extraction is behavior-preserving by construction**
  (verbatim body move + delegation) and is landed FIRST in its task with
  the 8-gate re-verify run immediately, isolating any regression to the
  refactor commit.

## §9 Testing strategy

- **Pure doctests `tests/test_cargo.cpp`** (host-only, hand-built Units, no
  install): capacity accounting — Size-weighted fill to exact capacity, a
  Size=2 passenger consuming 2 slots (the DRON-shaped fixture), refusal at
  overflow, `SizeLimit` refusal of an oversized passenger; the `can_load`
  matrix (not-a-transport / full / wrong house / dead / dying / self /
  already-carried / carrying-cargo-itself); `first_free_ring_cell` —
  IDENTITY with `exit_cell`'s walk (same anchor/max_r → same cell, the
  extraction proof at unit granularity), ring order determinism
  (south-first at r=1), all-blocked → {-1,-1}; FIFO eject order over a
  hand-built cargo list; capacity>1 logic fully exercised regardless of
  which transport art this install fields.
- **World-level tests** (the existing test_world pattern): board releases
  ALL occupancy (including a mid-hop reservation) and the one-unit-per-
  cell audit stays green; boarded unit is unpickable/unselectable/
  untargetable and skipped by draw order; eject claims occupancy,
  one-per-cell, stamps coords/facing; boxed-in unload HOLDs then resumes
  when a cell frees; transport death tombstones cargo; enter order
  cancels cleanly on transport death/full; a new order supersedes
  `unload_pending`.
- **Integration**: the `--demo-transport` gate (§7) with wedge guards +
  two-run self-compare; rendered vs headless byte-identical.
- **Regression**: all 8 existing gates byte-identical (values in §8),
  re-run at the ring-extraction commit AND at finalize; full suite both
  modes; arm64 NDK build with the new cargo symbols nm-verified. Suite
  stays scoped/fast (the shared-fixture discipline).

## Out of scope / divergences (explicit)

- **Passenger fire-out / IFV weapon-swap** (`OpenTopped=` firing ports,
  the IFV's passenger-driven turret modes) — DEFERRED. The hunt's item 2/5
  passes will note where the predicate/disposition touches `OpenTopped`
  and the `[General]` OpenTopped* knobs so a later phase recovers the
  mechanism from a mapped starting point; CB-1 ships no fire-out.
- **Amphibious/naval transports** (LCRF etc.) — need water movement, CB-3
  Naval. CB-1 is GROUND transports only.
- **Aircraft/paradrop delivery, tank bunker, building garrison, Battle
  Fortress ram** — out entirely.
- **Transport-death fate beyond the recovered default** (§4) — exceptions
  RE-documented, not built.
- **Nested loaded transports** (a transport carrying a loaded transport) —
  `can_load` rejects a passenger with non-empty cargo; data-unreachable
  with HTK (`SizeLimit=2` < vehicle `Size=3`) and a simplification the
  hunt's item 2 will bound precisely.
- **AI use of transports** — ScriptType opcodes 0x2e/0x2f (load/unload
  transports) STAY inert in the AI-2 interpreter (ai-team.md's table);
  flipping them implementable atop this subsystem is a recorded
  carry-forward, not CB-1.
- **Enter cursor art / full pip row rendering** — cosmetic; minimal
  indicator only (§6).
- **Sub-cell eject lean** — like production's `ExitCoord`, any sub-cell
  offset is dropped at our cell resolution (the established divergence).

## Global constraints (project-wide)

- Ghidra-ground-truth-first: recover original behavior (Ghidra / real
  rulesmd / prose-only refs) BEFORE implementing; the Task 1 hunt +
  RE-GATE precede all dependent tasks; every recovered constant cites
  `docs/re/transport-cargo.md`; [RE-FALLBACK] items become plan-amendment
  gates, never guesses.
- EIGHT byte-identity gates stay green (§8); `--demo-transport` becomes the
  9th permanent gate.
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND
  `CMakeLists.txt`.
- Env: run exes from PowerShell, NOT Git Bash (Git Bash gives exit 127).
  `RA2_INSTALL_DIR` default `D:\Red Alert 2`. Tests skip cleanly without
  the install. Long rendered demos auto-background on short timeouts;
  NEVER pipe a native exe through `Select-Object -First N` (stops draining
  stdout → false hang).
- No game assets / GPL source / Ghidra output committed; recovered numeric
  constants ARE committable.
- Repo on master, solo, no remote. NDK D:\android-ndk-r27c for arm64.
- Commits end with: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`
- Prior subagent failure mode: implementers stalling on backgrounded runs —
  the plan's briefs must order FOREGROUND synchronous runs with generous
  timeouts.

## Testing summary

| Layer | Tests |
|---|---|
| engine/cargo (pure) | Size-weighted capacity (exact fill, Size=2 consumes 2 slots, overflow refusal, SizeLimit refusal); can_load matrix (non-transport/full/house/dead/dying/self/carried/carrying); first_free_ring_cell ≡ exit_cell walk (extraction identity), deterministic ring order, all-blocked → {-1,-1}; FIFO eject order; capacity>1 covered host-only regardless of fielded art |
| World integration | board = release-all-occupancy + off-map (unpickable/untargetable/undrawn), occupancy audit green; eject = ring cell + occupancy claim + coords/facing stamp, one per frame; boxed-in HOLD then resume; transport death kills cargo; enter-order cancel paths; new order supersedes unload_pending |
| integration | `--demo-transport` witnesses (board K/5 → refuse-at-full → carry → eject FIFO → resume) + transport_hash two-run self-compare (no pinned literal) + wedge guards (≥1 board AND ≥1 eject else exit 1); rendered ≡ headless |
| regression | eight existing gates byte-identical (§8), re-verified at the ring-extraction commit and at finalize; full suite both modes; arm64 nm-verify of cargo symbols |
