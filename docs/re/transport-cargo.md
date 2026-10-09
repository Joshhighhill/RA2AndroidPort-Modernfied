# Transport / cargo semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `tc_hunt.py` … `tc_hunt13.py`;
decompiles/listings under `D:\ra2-re\out\tc*`. Nothing under `D:\ra2-re` is
committed. Content-anchored only — located from the TechnoType INI-key string
cross-refs `"Passengers"` `"SizeLimit"` `"Size"` `"PipScale"` `"OpenTopped"`
`"EnterTransportSound"` `"LeaveTransportSound"` `"DeployTime"` (all loaded in
`FUN_00711510` = TechnoTypeClass::LoadFromINI), from the PassengersClass helper
cluster located by content at `0x4732D0`–`0x473430`, from a 1.13 M-instruction
displacement scan for the recovered type/object offsets, and by xref-walking the
container ops to their consumers. Published YRpp addresses were used ONLY as prose
hints and every one drifted as `workflow.md` §4 warns: the `PassengersClass`
helper hints `0x4733A0`/`0x473430`/`0x473460`/`0x4734B0`/`0x473500` all landed
~0xD0 off their real functions; `MarkPassengersAsExited 0x7104C0`,
`ExitedOpenTopped 0x7104A0`, `SetTargetForPassengers 0x710550` all resolved into
one unrelated 17 KB function; `EnterAsPassenger 0x4DE630` / `RemovePassenger
0x4DE670` drifted to a gunner-update stub / a 3-byte thunk. Each real function was
relocated by content. **Per the `calibrate-first` directive the rulesmd VALUES
([HTK] Passengers=5 SizeLimit=2 Size=3, [FV] Passengers=1 SizeLimit=1 Size=3,
infantry Size=1, DRON Size=2) are ground truth already; this hunt pins the
FORMULA / semantics / mechanics.**)

Companion of `production.md` (KickOutUnit / `DAT_0089E5F8` 8-ring — which the unit
unload does **not** reuse, see §4) and `economy.md` (the `FUN_0043C250`
Receive_Message dock handshake shares the same message-switch as the load
handshake here). "Deviations we take" at the end resolves the `[RE-GATE]` markers
in the Transports/Cargo plan Tasks 2–6.

## TL;DR — the deltas from the plan's pre-hunt assumptions

Four load-bearing findings differ from the plan and need RE-GATE adjudication:

1. **[SUPERSEDED in wave 6, W6-04-08 -- see §1: vehicle transports ARE a Size
   pool (`UnitClass::ReceiveMessage` FUN_00736500); the count below is the
   AircraftClass rule.]** **Capacity is a passenger COUNT, not a Size-sum.** The load gate is
   `NumPassengers < Passengers` (each body = 1 slot). `Size`/`SizeLimit` are a
   **separate per-passenger size cap** (`passenger.Size <= transport.SizeLimit`),
   not a running total against `Passengers`. `GetTotalSize()` (Σ Size) exists but
   is used **only for the pip display**, never for the capacity gate. The plan's
   `cargo_slots_used + p.size <= t.passengers_max` (size-sum) is WRONG for the
   original.
2. **The container is LIFO (a head-pushed singly-linked stack), not FIFO.**
   `AddPassenger` inserts at the head; unload pops the head → last-in-first-out.
3. **The eject uses its own facing-scatter routine, NOT the KickOutUnit
   `DAT_0089E5F8` 8-ring.** One passenger per mission tick; no free cell → the
   passenger is put back (HOLD/retry).
4. **The ownership gate is allied-or-same-house, not strictly same-house.**

Items 5 (cargo dies with transport) and the same-cell boarding trigger match the
expected default / land as documented deviations.

## Recovered layout (offsets are content-verified)

**TechnoTypeClass fields** (loaded by `FUN_00711510`, confirmed a second time by
the type-CRC serializer `FUN_00716540` and a third by `GetTotalSize`'s
`FADD [type+0x380]`):

| Key | Offset | Type | Notes |
|-----|--------|------|-------|
| `Passengers` | `+0x5E0` | int | capacity **count**; read via `CCINIClass::ReadInteger` (`0x527400`) |
| `Size` | `+0x380` | **double** | this unit's size *as a passenger* |
| `SizeLimit` | `+0x388` | **double** | max single-passenger `Size` this transport accepts |
| `DeployTime` | `+0x3C8` | **double** | fed to a countdown timer (`FUN_004A5150`), §4 |
| `OpenTopped` | `+0x5E4` | bool | fire-out flag; §5 |
| `PipScale` | `+0x3D4` | int (enum) | `Passengers` = 2; drives the pip row |
| `EnterTransportSound` | `+0x564` | int (sound idx) | played by `AddPassenger`, §3 |
| `LeaveTransportSound` | `+0x568` | int (sound idx) | −1 when absent |

(Size/SizeLimit/DeployTime being **doubles** matches the YRpp `TechnoTypeClass`
layout hint; for the roster values 1/2/3 the double-vs-int distinction is
behaviourally irrelevant.)

**TechnoClass (object) fields** — the cargo container is an embedded
`PassengersClass` at **`this+0x114`**:

| Field | Offset | Notes |
|-------|--------|-------|
| `Passengers.NumPassengers` | `this+0x114` | the capacity **count** compared against type `Passengers` |
| `Passengers.FirstPassenger` | `this+0x118` | head of the singly-linked passenger list |
| passenger link (`NextObject`) | `passenger+0x30` | threads the passenger list |

`[General]` knobs (loaded by `FUN_0066B5F0`, the `[General]`/`[JumpjetControls]`
rules loader): `OpenToppedRangeBonus=2`, `OpenToppedDamageMultiplier=1.2`,
`OpenToppedWarpDistance=7` — reference-only for CB-1 (fire-out is out of scope).

---

## §1 Capacity accounting — a Size POOL for vehicle transports, a COUNT for aircraft and garrisons

> **Corrected in wave 6 (W6-04-08).** This section used to say the load gate is a
> body count for every transport. That read `FUN_004190B0`, which is
> **`AircraftClass::ReceiveMessage`** (it sits in the AircraftClass vtable at
> `0x7E1438`, next to `AircraftClass::Mission_Attack` `FUN_00417FE0` at
> `0x7E14B4`), not a FootClass base. Vehicle and ship transports are UnitClass,
> whose `ReceiveMessage` `FUN_00736500` overrides it with a Size sum.

**Vehicle / ship transports (UnitClass): `ΣSize + Size ≤ Passengers`.**
`UnitClass::ReceiveMessage` (`FUN_00736500`, decompiled in wave 6,
`D:/ra2-re/w6fix-units-out/a3.txt`):

- **`case 0xF`** ("Can I be loaded?") and **`case 0xE`** (do the load), after the
  `Passengers != 0` / `IsAlliedWith` gates:
  ```c
  int total = GetTotalSize();                       // FUN_00473390: sum of the passengers' Size
  if ((double)type->Passengers(+0x5E0) < total + passenger->Type->Size(+0x380))
      return 10;                                    // NEGATIVE: no room
  if (type->SizeLimit(+0x388) < passenger->Type->Size(+0x380))
      return 10;                                    // NEGATIVE: too big
  return 1;                                         // ROGER
  ```
  So `Passengers` is a **Size pool**: the Amphibious Transport (`[LCRF]`/`[SAPC]`
  `Passengers=12 SizeLimit=6`) takes 12 infantry (Size 1), 4 Grizzlies (Size 3)
  or 2 Apocalypses (Size 6). Every Allied/Soviet infantry is Size 1, so the IFV
  (`Passengers=1`), Flak Track (`Passengers=5`) and Battle Fortress
  (`Passengers=5`) behave exactly as a body count for infantry.
- **`case 0x15`**: when `GetTotalSize() == Passengers` (full) it arms the
  `DeployTime` timer (`FUN_004A5150(type+0x3C8, …)`).

**Aircraft transports (AircraftClass, the Night Hawk): a COUNT.**
`AircraftClass::ReceiveMessage` (`FUN_004190B0`) answers `case 0xF` with
`((Passengers <= NumPassengers(+0x114)) - 1 & ~8) + 10`, i.e. room ⇔
`NumPassengers < Passengers`, and `case 0xE` uses the same guard.

**Garrisons (BuildingClass): a COUNT plus the per-passenger cap.**
`BuildingClass::ReceiveMessage` (`FUN_0043C250` `case 0xF`, occupiable path):
```c
if (this->NumPassengers(+0x114) + 1 <= type->Passengers(+0x5e0)       // COUNT gate
    && passenger->Type->Size(+0x380) <= this->Type->SizeLimit(+0x388))  // per-passenger cap
    return 1;
```

`AddPassenger` (`FUN_004732D0`) increments `NumPassengers` by 1 per node.
`GetTotalSize()` (`FUN_00473390`, `total += Type->Size` per node, then `ftol`)
feeds the UnitClass capacity gate above, the refinery pip animation
(`FUN_004509C0`) and the unit pip draw (`0x7363xx`).

**Defaults when a key is absent** (from `FUN_00711510` read helpers, all pass the
current field value as the INI default so an absent key leaves the ctor default):
`Passengers` absent → 0 (not a transport); missing `Size` → **1**; missing
`SizeLimit` → **0**.

**Confidence: CONFIRMED** (UnitClass Size pool `FUN_00736500`; aircraft count
`FUN_004190B0`; garrison count + cap `FUN_0043C250`). The clone:
`engine/cargo.cpp` `can_load` (aircraft: count; every other unit transport: Size
pool; `Size <= SizeLimit` for both); garrisons are counted in `world_depth.cpp`.

## §2 The "can X enter Y" predicate (evaluation order)

From `UnitClass::What_Action` (`FUN_0073EB90`) → the target's
`ReceiveMessage(0xF)` (`FUN_004190B0`):

1. **Is Y a transport?** `Y.Type->Passengers(+0x5e0) > 0`. (`What_Action`
   reads `0 < *(targetType+0x5e0)`; `ReceiveMessage 0xF` rechecks `!= 0`.)
2. **Sender valid & ownership** — `FUN_004F98B0(Y.owner, X)` =
   `IsAlliedWith`: returns true if `X.house == Y.house` **OR** `X.house.side ==
   Y.house.side` **OR** the ally bitmask `(1 << X.houseIdx) & Y.house->Allies(+0x5788)`.
   So **allied-or-same-house**, not strictly same-house.
3. **Remaining capacity** — `NumPassengers < Passengers` (§1).
4. **Per-passenger size** — `X.Type->Size <= Y.Type->SizeLimit` (§1; inline in the
   building path, PROBABLE placement for the mobile path).
5. **Proximity** — resolved at the load handshake, not this predicate: X paths to
   Y's cell and the load fires on co-location (§3).

The IFV (`FV`) gunner is a special case of this: `Passengers=1`, the single
passenger becomes the turret gunner (type `Gunner=yes`, TechnoClass `+0x805`
consulted in `AddPassenger`'s sibling path). `Turret>0` transports also keep a
`NonPassengerCount` (the gunner is not unloaded — YRpp `FootClass::NonPassengerCount`).

**OpenTopped touch-point (for the later fire-out phase):** `FUN_00700760` reads
`type->OpenTopped(+0x5E4)` and, when set, walks the passenger list to compute a
min metric (fire-out range gating). The `[General]` `OpenTopped*` knobs load in
`FUN_0066B5F0`. Mapped, not implemented (out of scope).

**Confidence: transport-flag (Passengers>0) CONFIRMED; ownership = IsAlliedWith
CONFIRMED (`FUN_004F98B0` content); capacity gate CONFIRMED; the full ordering is
PROBABLE (What_Action + message-0xF reconstruct it but the cursor-side
`Size<=SizeLimit` gate for mobile transports was not isolated).**

## §3 Load mechanics — approach to the transport's cell, then limbo

The passenger side is `FootClass::ReceiveMessage` (`FUN_004D8ED0`):

- **`case 0x12`** ("proceed / come to me"): it compares the passenger's **cell**
  against the **transport's cell** (`passenger.CoordToCell == target.Coord >> 8`);
  if equal it returns `0x14` ("I'm here — do it now"), otherwise it assigns
  mission Move (id 2) toward the transport and sets the radio link
  (`vtbl+0x480`). So the passenger **paths onto the transport's own cell**; the
  load completes on **co-location (same cell)**, not at adjacency.
- The transport then runs `ReceiveMessage(0xE)` (`FUN_004190B0` case 0xE): with
  room, it links the passenger and pulls it off the map.
- **Remove-from-map / occupancy order:** the passenger is limboed
  (`vtbl+0x480(transport,1)` sets Transporter, then the object is un-marked from
  its cell and `AddPassenger` links it) — occupancy is released as part of the
  limbo, then the node is pushed. `TechnoClass::AddPassenger` (`FUN_0070FA10`)
  then plays `EnterTransportSound` (`type+0x564`, if `!= -1`).

**This is a documented deviation for the grid engine:** our engine boards at
Chebyshev `<= 1` adjacency; the original boards at **same-cell** (the passenger
walks onto the transport's tile). Adjacency yields the same reachable set and is
deterministic — accept as a RE-GATE-adjudicated deviation (the plan's expected
outcome).

**Confidence: same-cell trigger PROBABLE (cell-equality compare in `FUN_004D8ED0`
case 0x12); limbo-then-link order PROBABLE (limbo + AddPassenger seen; the exact
un-occupy instruction not isolated to one literal).**

## §4 Unload mechanics — own facing-scatter, one per tick, put-back on no-cell

The unload mission is `FUN_00415EE0`: **each tick, if the transport still has a
passenger it ejects exactly one and returns**:
```c
if (this->FirstPassenger(+0x118) != 0) { eject_one(FUN_00415C60); return; }
// (only after the hold is empty does it run the transport's own movement/turret)
```

`eject_one` = `FUN_00415C60`:
```c
p = RemoveFirstPassenger();               // pop the HEAD (newest) — LIFO, §6
if (p) {
    facing = transport.facing +/- 0x3FFF;  // ±~90°, alternates via (counter & 1)
    cell   = FUN_00481090(scatter-from-transport-coord-along-facing);
    if (p->CanEnterCell(cell) == OK && cell valid)
        if (p->Unlimbo(cell, ...) /*vtbl+0xE8*/) { set scatter mission; return; }
    AddPassenger(p);                        // NO free cell -> put it BACK (HOLD/retry)
}
```

- **Eject cell selection:** a **facing-relative scatter** — the transport's facing
  rotated ±90° (`0x3FFF ≈ 90° BAM`), alternating side each eject via a counter's
  low bit — then `FUN_00481090` finds the actual placement cell and
  `CanEnterCell`/`FUN_0047C430` (the same cell-occupant test the harvester/
  KickOutUnit ring uses) validates it. **It does NOT walk the `DAT_0089E5F8`
  8-ring** — that ring is `BuildingClass::KickOutUnit`-only (`production.md`).
  This is a delta from the brief's "reuses the KickOutUnit ring" assumption.
- **One passenger per cell / per tick:** one `RemoveFirstPassenger` +
  one `Unlimbo` per `FUN_00415C60` call; one `FUN_00415C60` per
  `FUN_00415EE0` tick.
- **Cadence:** one per logic tick. `DeployTime` (`type+0x3C8`) is fed to a
  countdown timer by `FUN_004A5150` (`time = DeployTime * scale`, stamped with
  the current frame `DAT_00A8DCEC`) — it gates the deploy/unload *start* delay;
  the per-passenger step itself is one-per-tick.
- **Eject facing:** derived from the eject-scatter direction (transport
  facing ± 90°); the passenger is placed facing outward.
- **No free cell:** `AddPassenger` puts the passenger **back into the hold** and
  the mission retries next tick — i.e. **HOLD-and-retry**, never a silent drop and
  never an order-abort.

**Confidence: one-per-tick eject + put-back-on-no-cell + own-scatter-routine
(not the 8-ring) CONFIRMED (`FUN_00415EE0`/`FUN_00415C60` content). DeployTime is
a real timer CONFIRMED (`FUN_004A5150`); its exact application (deploy-start delay
vs per-eject throttle) is PROBABLE. Eject facing = ±90° scatter CONFIRMED
structurally; the passenger's final facing is PROBABLE.**

## §5 Transport-death passenger disposition — cargo dies with the transport

Three independent sweep sites all remove **and destroy** every passenger, with no
`OpenTopped`/survival branch:

- `FUN_00700DA0` (TechnoClass death/Limbo handler):
  ```c
  while (this->FirstPassenger(+0x118) != 0) {
      p = GetFirstPassenger(); if (p->+0x5d4) FUN_006E9DD0(p,-1,0);  // detach mgr
      p = RemoveFirstPassenger();
      p->vtbl+0xE0(transport);   // unlink / notify
      p->vtbl+0x20(1);           // delete (dtor, arg 1)
  }
  ```
- `FUN_00707130` — the same loop (a `KillPassengers`-style helper).
- `FUN_00734850` (`~UnitClass`) — the same loop as final cleanup.

Every path calls `passenger->vtbl+0x20(1)` (the delete/destroy slot). **No branch
checks `OpenTopped` or any survival flag; there is no "eject survivors on death"
default in this binary.** So the recovered default is: **passengers are destroyed
with the transport.** (`OpenTopped` governs fire-*out* while alive, via
`FUN_00700760`, not death survival — mapped for the later phase, §2.)

**Confidence: CONFIRMED** (three sweep sites, identical delete-each pattern, no
survival branch).

## §6 Deterministic ordering — LIFO container, deterministic tie-break

The cargo container is a **singly-linked stack with head insertion**:

- `PassengersClass::AddPassenger` (`FUN_004732D0`): main path is
  `p->NextObject(+0x30) = this->FirstPassenger; this->FirstPassenger = p;` then it
  recounts `NumPassengers` by walking the list. **Head insertion.**
- `PassengersClass::RemoveFirstPassenger` (`FUN_00473360`):
  `FirstPassenger = head->NextObject; head->NextObject = 0; NumPassengers--`.
  **Head removal.**
- The unload (`FUN_00415C60`, §4) pops via `RemoveFirstPassenger`.

Therefore boarding A,B,C gives list `[C,B,A]` and unload ejects **C,B,A** — the
**last unit in is the first out (LIFO)**. The plan's `next_eject` = FIFO head is a
delta.

**Simultaneous boarders / last-slot tie-break:** the container ops consult **no
RNG** — `FUN_007C4D00`, which the brief/`production.md` label as half of "the
lockstep RNG pair", is here proven by content to be **`ftol` (ROUND, float→long)**,
used inside `GetTotalSize` for the size accumulation, not a random draw. So the
last slot goes to whichever boarder's arrival message is processed first —
deterministic by processing order, no lockstep-RNG dependence. Our engine's
deterministic ascending-index tie-break is a faithful analogue.

**Confidence: LIFO head-push/head-pop CONFIRMED (`FUN_004732D0`/`FUN_00473360`
content, corroborated by the eject's `RemoveFirstPassenger`); no-RNG-in-ordering
CONFIRMED (`FUN_007C4D00` = ftol by content).**

---

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00711510` | **TechnoTypeClass::LoadFromINI** — Passengers→+0x5E0(int), Size→+0x380(dbl), SizeLimit→+0x388(dbl), DeployTime→+0x3C8(dbl), OpenTopped→+0x5E4(bool), PipScale→+0x3D4, EnterTransportSound→+0x564, LeaveTransportSound→+0x568 | INI-key string refs → store offsets (`tc2`) |
| `FUN_004732D0` | **PassengersClass::AddPassenger** — HEAD insert + recount NumPassengers | region enum `0x4732D0`; content |
| `FUN_00473360` | **PassengersClass::RemoveFirstPassenger** — pop HEAD, NumPassengers-- | region enum; content |
| `FUN_00473380` | PassengersClass::GetFirstPassenger — return `this+4` | region enum; content |
| `FUN_00473390` | **PassengersClass::GetTotalSize** — Σ ftol(passenger.Type.Size @+0x380); pip-only | disasm `FADD [type+0x380]`+ftol |
| `FUN_004733E0` | PassengersClass::RemovePassenger(specific) — unlink node | region enum; content |
| `FUN_00473430` | PassengersClass::IndexOf — walk-count to candidate | region enum; content |
| `FUN_0070FA10` | **TechnoClass::AddPassenger (virtual)** — calls AddPassenger, plays EnterTransportSound(+0x564) | AddPassenger caller; content |
| `FUN_004190B0` | **Foot/UnitClass::ReceiveMessage** — load handshake; case 0xF/0xE capacity gate `NumPassengers<Passengers` + IsAlliedWith; case 0x15 arms DeployTime timer | msg-switch + content |
| `FUN_0043C250` | **BuildingClass::ReceiveMessage** — garrison/dock; case 0xF: `NumPassengers+1<=Passengers && Size<=SizeLimit` | `economy.md` + content |
| `FUN_006F4010` | TechnoClass::ReceiveMessage (base) — mission/radio fallthrough (no case 0xF) | msg-switch + content |
| `FUN_004D8ED0` | **FootClass::ReceiveMessage** (passenger side) — case 0x12 same-cell arrival → 0x14; case 0x23 eject-cell test via `FUN_0047C430` | msg-switch + content |
| `FUN_0073EB90` | **UnitClass::What_Action** — enter valid iff `target.Type.Passengers>0`, queries via msg 0xF | content |
| `FUN_004F98B0` | **IsAlliedWith** — same-house OR same-side OR ally-bitmask(house+0x5788) | ownership gate; content |
| `FUN_00415EE0` | **Unit unload mission** — `if FirstPassenger: eject one; return` (one per tick) | eject caller; content |
| `FUN_00415C60` | **eject-one-passenger** — RemoveFirst; facing±0x3FFF scatter; `FUN_00481090` placement; Unlimbo (vtbl+0xE8); no-cell → AddPassenger back (HOLD) | RemoveFirst+AddPassenger caller; content |
| `FUN_004A5150` | **DeployTime timer set** — `time = DeployTime(+0x3C8) * scale`, stamped `DAT_00A8DCEC` | DeployTime consumer; content |
| `FUN_00700DA0` | TechnoClass death/Limbo — passenger-kill sweep (delete each) | death sweep; content |
| `FUN_00707130` | KillPassengers-style sweep — delete all passengers | RemoveFirst caller; content |
| `FUN_00734850` | ~UnitClass destructor — passenger-kill sweep | RemoveFirst caller; content |
| `FUN_00700760` | OpenTopped fire-out metric — reads `type.OpenTopped(+0x5E4)`, walks passengers | OpenTopped reader; content |
| `FUN_0066B5F0` | `[General]` rules loader — OpenToppedRangeBonus/DamageMultiplier | string refs (`tc1`) |
| `FUN_007C4D00` | **ftol (ROUND, float→long)** — NOT the RNG (content overrides the prose hint) | `GetTotalSize` FADD/ftol |

## Confidence summary

| Item | Finding | Evidence | Confidence |
|------|---------|----------|:----------:|
| §1 | (wave 6) vehicle transports: **Size pool** `ΣSize + Size <= Passengers`; aircraft and garrisons: **COUNT**; Size <= SizeLimit per passenger | `FUN_00736500` (UnitClass) case 0xE/0xF + `FUN_004190B0` (AircraftClass) + `FUN_0043C250` case 0xF | **CONFIRMED** |
| §1 | `Size<=SizeLimit` enforcement site for *mobile* transports | inline for buildings; mobile site not isolated | **PROBABLE** |
| §2 | transport flag = `Passengers>0` | `FUN_0073EB90` + `FUN_004190B0` | **CONFIRMED** |
| §2 | ownership = **IsAlliedWith** (same-house OR allied), not same-house-only | `FUN_004F98B0` content | **CONFIRMED** |
| §3 | load trigger = **same-cell** (passenger paths onto transport's tile) | `FUN_004D8ED0` case 0x12 cell-equality | **PROBABLE** |
| §3 | limbo-then-link on load; EnterTransportSound on AddPassenger | `FUN_0070FA10` + case 0xE | **PROBABLE** |
| §4 | eject = own facing±90° scatter + `FUN_00481090`, **NOT** the `DAT_0089E5F8` 8-ring | `FUN_00415C60` content | **CONFIRMED** |
| §4 | one passenger per unload tick | `FUN_00415EE0` (`if passengers: eject one; return`) | **CONFIRMED** |
| §4 | no-free-cell → **HOLD** (AddPassenger back, retry) | `FUN_00415C60` put-back path | **CONFIRMED** |
| §4 | DeployTime = timer (deploy-start delay) | `FUN_004A5150` | **CONFIRMED (timer); PROBABLE (per-eject vs start)** |
| §5 | passengers **die with the transport**; no OpenTopped survival branch | `FUN_00700DA0`/`FUN_00707130`/`FUN_00734850`, all `vtbl+0x20(1)` | **CONFIRMED** |
| §6 | container = **LIFO** (head push/head pop) | `FUN_004732D0` head-insert + `FUN_00473360` head-pop + eject via RemoveFirst | **CONFIRMED** |
| §6 | no RNG in ordering (deterministic) | `FUN_007C4D00` = ftol | **CONFIRMED** |

## Deviations we will take

- **[SUPERSEDED in wave 6, W6-04-08 -- §1: the plan's size-sum was right for
  vehicle transports.]** **[PLAN DIFF — §1 capacity model, RE-GATE STOP].** The plan's size-sum
  `cargo_slots_used + p.size <= t.passengers_max` is WRONG. The original is a
  **body-COUNT**: `NumPassengers < Passengers`. `SizeLimit` is a per-passenger
  cap (`p.size <= t.size_limit`), and `Size` feeds only the pip display
  (`GetTotalSize`). **Task 2 `can_load` must become
  `cargo_count < passengers_max && p.size <= size_limit`** (not a size-sum), and
  Task 5's demo "exact-fill 5/5 + 1 refusal" holds under the count model with the
  all-Size-1 roster. For our roster the two models coincide for infantry loads;
  they diverge only for Size≥2 passengers, so this is safe to switch and should be
  switched for fidelity.
- **[PLAN DIFF — §6 order, RE-GATE].** The original is **LIFO** (head push, head
  pop); the plan's FIFO `next_eject` head is reversed. Either adopt LIFO
  (last-in-first-out) to match, or accept FIFO as a deliberate, documented
  deviation — but it must be adjudicated, not silently kept. Recommend LIFO for
  fidelity; the container is a stack.
- **[DEVIATION — §3 boarding trigger].** Original boards at **same-cell**
  (passenger walks onto the transport's tile). Our grid boards at Chebyshev `<= 1`
  adjacency — same reachable set, deterministic. Accepted deviation.
- **[DEVIATION — §4 eject routine].** Original uses a facing±90° scatter +
  `FUN_00481090` placement (NOT the `DAT_0089E5F8` KickOutUnit 8-ring). Our engine
  may reuse its own nearest-free-cell search; behaviourally equivalent
  (first free cell near the transport, one per tick). Document that the 8-ring is
  NOT shared with unload.
- **[DEVIATION — §2 ownership].** Original gate is **allied-or-same-house**
  (`IsAlliedWith`). CB-1 models one house per side, so same-house-only is a valid
  simplification; note allies are unmodeled and this is where allied loading would
  attach.
- **[DEVIATION — §4 eject facing / sub-cell].** Original leans the passenger to
  the ±90° scatter side with a sub-cell offset; at cell resolution we keep the
  cell and drop the sub-cell lean (the production `ExitCoord` precedent).
- **[DEVIATION — §4 cadence].** One eject per logic tick (recovered) with a
  `DeployTime`-derived start delay; if we cannot reproduce the exact timer we use
  one-per-frame — same visible order and set.
- **[PLAN OK — §5 death].** Cargo dies with the transport (the recovered default);
  `OpenTopped` does NOT grant death survival. Keep the tombstone-on-death model;
  `OpenTopped*` stays an RE-documented carry-forward for the fire-out phase.
- **[DATA-UNREACHABLE — nested transports].** `SizeLimit` (HTK=2, FV=1) is below
  vehicle `Size=3`, so a loaded transport (a vehicle) can never be a passenger;
  `can_load` rejecting a passenger that itself carries cargo is correct and
  data-unreachable for our roster.
- **[NOTE — passenger coords].** Carried passengers are limboed; their world
  coordinates are stale while carried (they live only in the linked list), exactly
  as our tombstone/limbo model assumes.
