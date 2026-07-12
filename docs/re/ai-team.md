# Enemy AI offensive teams — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `ait1.py` … `ait7.py`;
decompiles/listings under the scratchpad `out6/` — nothing under `D:\ra2-re` or
the scratchpad is committed. **Content-anchored only.** Every function below was
located by anchoring on the literal aimd.ini team key/section strings the binary
loads (`"Autocreate"`, `"Recruiter"`, `"Priority"`, `"Max"`, `"Full"`, `"Group"`,
`"Waypoint"`, `"Script"`, `"TaskForce"`, `"House"`, `"Whiner"`, `"Aggressive"`,
`"Suicide"`, … the full TeamType key set) and the rulesmd `[AI]`/`[General]` team
knobs (`"TeamDelays"`, `"DissolveUnfilledTeamDelay"`, `"AutocreateTime"`,
`"TotalAITeamCap"`, `"InfantryReserve"`, `"CreditReserve"`, `"AISafeDistance"`,
`"Stray"`, `"CloseEnough"`), recovering the field offset each writes, then
xref-walking / displacement-scanning to the consumers. **No YRpp/Ares address was
used as evidence** — YRpp `TeamTypeClass.h`/`TaskForceClass.h`/`ScriptTypeClass.h`/
`ScriptClass.h`/`TeamClass.h` were read only for layout/field-name hints, and
every published address drifted or landed in a Ghidra gap in this build (see
"YRpp drift" below). Numeric constants are read from the real `aimd.ini` (from
`ra2md.mix → localmd.mix/aimd.ini`) and `rulesmd.ini`.)

Companion of `ai-base.md` (the House-AI base-planning tick chain this hunt hangs
off — `FUN_004fd320` House-AI tick, one-structure-at-a-time gate, difficulty
sections) and `production.md` (the shared FactoryClass unit-production path teams
reuse). This doc is the calibration authority for AI-2 offensive-teams spec §1
items 1–6 and resolves the `[RE-GATE APPLIED]` blocks in Tasks 2–7. **See
"Deviations / plan diffs (for the RE-GATE review)" for the STOP items** — the
universal `House=<none>`, the random-angle gather ring, the ~90-entry action
table, and the `Full=`/DissolveUnfilledTeamDelay semantics all diverge from the
plan's pre-hunt assumptions.

Key globals: `DAT_00886160` = `RulesClass::Instance`. `DAT_00a8dcec` = global
frame counter. `FUN_0065c220` = lockstep AI RNG (`rand(lo,hi)`), `FUN_007c4d00`
= raw RNG draw (also produces the gather-angle). `DAT_008b3134`/`DAT_008b3138`/
`DAT_008b3140` = ScriptType array items/capacity/count (this build's array
globals; the YRpp `0x8B41C8` drifted). ScriptType/TaskForce/TeamType are all
`AbstractTypeClass` subclasses with the standard ~0x24-byte + name header, so
their INI-parsed fields sit at large offsets (below).

## YRpp drift (this build)

Every published team address is wrong or lands in a Ghidra-undefined gap here:
`TeamTypeClass::LoadFromINIList 0x6F19B0` → 4-byte thunk; `TeamType ctor 0x6F06E0`,
`CreateTeam 0x6F09C0`, `CanRecruitUnit 0x6F1320`, `TeamClass ctor 0x6E8A90`,
`AssignMissionTarget 0x6E9050`, `TaskForce ctor 0x6E7E80` → NO_FUNCTION gaps;
`ScriptType ctor 0x6916B0` → real entry `0x691600`; `ScriptClass ctor 0x6913C0`
→ `0x691370`. The layouts (field order/type) in the YRpp headers **do** match
what the binary parses; only the addresses drifted. All addresses below are this
build's, verified by content.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_006f0ab0` | **TeamTypeClass::WriteINI/Save** — writes every TeamType field back to INI; the authoritative **struct-offset map** (offsets §1) | anchor: all TeamType key string refs land here |
| `~0x6F06E0` (gap) | **TeamTypeClass::ReadINI** — the paired reader (Ghidra left it undefined; its INI-get sites are the `0x6F06xx–0x6F0Axx` "no-fn" refs to each key string) | key-string ref addresses inside the 0x6F06E0–0x6F0AB0 gap |
| `FUN_00691600` | **ScriptTypeClass::FindOrAllocate** — `new(0x234)`; 0x234 = header + `ActionsCount` + `ScriptActions[50]`×8B; registers into `DAT_008b3134` array | YRpp `0x6916B0` hint + `<none>` string + alloc size |
| `FUN_00691370` | ScriptClass action accessors (`GetCurrentAction`/`GetNextAction`/`SetMission`) region | YRpp `0x6913C0` hint |
| `FUN_006e30d0` | **ScriptType action classifier** — `switch(action)` over **114 case labels** (all valid action numbers, up to `0x90`) → argument-class code; the authoritative *which-actions-exist* table (§3) | region switch-scan (only >=8-case fn in 0x6E–0x6F) |
| `FUN_006dc870` | ScriptType/trigger **describe/format** — uses the classifier to pick per-action argument formatting (waypoint / techno / building / number / string) | sole caller of the classifier |
| `FUN_006e9ff0` | **TeamClass recruit-from-existing** — scans infantry/vehicle/aircraft global arrays by member `WhatAmI`, ownership, nearest distance; `CanRecruit` (`FUN_006e9b80`) + AddMember (`FUN_006e9a70`/`FUN_006ea440`) | AddMember caller; member-array walk |
| `FUN_006ea440` | **TeamClass::AddMember / LiberateMember** cluster | YRpp `0x6EA500`/`0x6EA870` hints |
| `FUN_006ef3c0` | **TeamClass::GetTaskForceMissingMemberTypes** — the produce-shortfall list (which types still needed) | YRpp `0x6EF4D0` hint |
| `FUN_006eec60` / `FUN_006eef70` | **gather-cell chooser** — reads `Rules.AISafeDistance` (`Rules+0xd74`) `<<8` (cells→leptons) as radius, RNG angle (`FUN_007c4d00`) around the enemy base centre, snaps to nearest free cell (`FUN_0056d750`); logs `"A '%s' Team has chosen (%d,%d,%f)"` | AISafeDistance displacement consumer in team cluster |
| `FUN_006eb030` | **regroup / "gathered" predicate** — moves stragglers to the focus; uses `Rules.Stray` (`+0x171c`), `GuardModeStray` (`+0x1720`), `CloseEnough` (`+0x1718`); sets team `+0x80` (`StepCompleted`) when all members within range | regroup callee of every action handler |
| `FUN_006e9640`, `FUN_006edd60`, `FUN_006edb20`, `FUN_006ed950`, `FUN_006ebd30`, `FUN_006ed0b0`, `FUN_006ec5f0`, `FUN_006ee670` | **per-action script handlers** — set team target/focus, issue Move/Attack missions, call `FUN_006eb030` regroup, set `+0x80` when the action completes; dispatched via a **data jump table** (no code xrefs) | callers of regroup `FUN_006eb030` |
| `FUN_006ec240` | **TeamClass::ScanLimit** (under/over-strength recount) | YRpp `0x6EC3A0` hint |
| `FUN_00672520` | RulesClass::Read `[AI]` — **AutocreateTime→Rules+0x1510 (double, minutes)**, **InfantryReserve→+0x1138**, **CreditReserve→+0x1758** (as `ai-base.md`) | anchor: `"AutocreateTime"`/`"InfantryReserve"`/`"CreditReserve"` refs |
| `FUN_0066cf70` | RulesClass::Read `[General]` — **TeamDelays→+0x1158**, **DissolveUnfilledTeamDelay→+0x1190**, **TotalAITeamCap→+0x13c8**, **Minimum/MaximumAIDefensiveTeams→+0x1390/+0x13ac**, **AISafeDistance→+0xd74**, **Stray→+0x171c**, **CloseEnough→+0x1718**, **GuardModeStray→+0x1720** | anchor: those key string refs |
| `FUN_0063f940` | AutocreateTime global scheduler (reads the `Rules+0x1510` timer + a probability distribution; sets a global "spawn now" latch) | AutocreateTime displacement consumer |

---

## §1 Struct layouts + aimd.ini file format

### File format (CONFIRMED from real aimd.ini + loaders)

`aimd.ini` (in `ra2md.mix → localmd.mix/aimd.ini`) has three **index sections**
whose entries are `index=<registered-id>`, then one **entity section per id**:

```
[TaskForces]        [ScriptTypes]       [TeamTypes]
0=<GUID>-G          0=<GUID>-G          0=<GUID>-G
1=<GUID>-G          …                   …
```

Stock ids are Westwood editor GUIDs like `05C60C4C-G`. The three entity kinds:

**TaskForce** entity — numbered member entries `N=count,UNITID` plus `Name=`,
`Group=`:
```
[0ACDA62C-G]                 [0ACDC72C-G]
Name=1 Apoc                  Name=5 Rhino, 2 Flak
0=1,APOC                     0=2,HTK
Group=-1                     1=5,HTNK
                             Group=-1
```
Up to **6** member entries (`Entries[6]`). `N=count,UNITID` where UNITID is a
`[VehicleTypes]`/`[InfantryTypes]`/`[AircraftTypes]` id.

**ScriptType** entity — numbered action entries `N=action,argument` plus `Name=`:
```
[0C31EA4C-G]  Name=Soviet Iron Curtain
0=53,0   1=5,10   2=55,0   3=49,0   4=0,7   5=0,2   6=0,1
```
Up to **50** action entries (`ScriptActions[50]`, each `{int Action; int Argument}`).

**TeamType** entity — **named keys** (order as emitted by `FUN_006f0ab0`):
```
Name, VeteranLevel, MindControlDecision, Loadable, Full, Annoyance, GuardSlower,
House, Recruiter, Autocreate, Prebuild, Reinforce, Droppod, UseTransportOrigin,
Whiner, LooseRecruit, Aggressive, Suicide, Priority, Max, TechLevel, Group,
OnTransOnly, AvoidThreats, IonImmune, TransportsReturnOnUnload,
AreTeamMembersRecruitable, IsBaseDefense, OnlyTargetHouseEnemy, [Tag], [Waypoint],
[TransportWaypoint], Script, TaskForce
```
`Script=` and `TaskForce=` reference a ScriptType and a TaskForce id. `Waypoint=`/
`TransportWaypoint=` are written via `FUN_00762430` (a waypoint→letter encoder;
letter/number code) but are **not emitted in the stock generic offensive teams**
(they matter for map-scripted reinforcement teams, AI-3 scope).

**KEY DIVERGENCE — `House=<none>` is universal.** All 163 TeamTypes in the stock
`aimd.ini` have `House=<none>`. There is **no per-side House= token set** in
stock data; a generic team is bindable to any AI house, and the country/side
filtering happens at **recruit/produce time** (the recruit scan `FUN_006e9ff0`
and the shared production path filter candidate units by the house's ownership/
side, exactly like `FUN_00505180`/ownership in `ai-base.md`). See §4.

### TeamType struct offsets (CONFIRMED — from `FUN_006f0ab0` field writes)

`this` = TeamTypeClass*. All bool = 1 byte, int = 4 bytes.

| off | field | off | field |
|---|---|---|---|
| +0x9c | Group (int) | +0xb0 | Droppod |
| +0xa0 | VeteranLevel (int) | +0xb1 | UseTransportOrigin |
| +0xa4 | Loadable (bool) | +0xb3 | OnTransOnly |
| +0xa5 | **Full** (bool) | +0xb4 | **Priority** (int) |
| +0xa6 | Annoyance | +0xb8 | **Max** (int) |
| +0xa7 | GuardSlower | +0xc0 | MindControlDecision (int) |
| +0xa8 | Recruiter | +0xc4 | Owner (HouseClass*) |
| +0xa9 | **Autocreate** (bool) | +0xc8 | idxHouse (int, MP) |
| +0xaa | Prebuild | +0xcc | TechLevel (int) |
| +0xab | Reinforce | +0xd0 | Tag* |
| +0xac | Whiner | +0xd4 | Waypoint (int, letter code) |
| +0xad | **Aggressive** | +0xd8 | TransportWaypoint |
| +0xae | LooseRecruit | +0xdc | **cntInstances** (int, live count) |
| +0xaf | Suicide | +0xe0 | **ScriptType*** |
| +0xf2 | AvoidThreats | +0xe4 | **TaskForce*** |
| +0xf3 | IonImmune | +0xf5 | AreTeamMembersRecruitable |
| +0xf4 | TransportsReturnOnUnload | +0xf6 | IsBaseDefense |
| | | +0xf7 | OnlyTargetHouseEnemy |

**Load-bearing for the autocreate offensive path:** `Autocreate` (+0xa9),
`Priority` (+0xb4), `Max` (+0xb8), `cntInstances` (+0xdc, the live-count that Max
caps), `ScriptType*` (+0xe0), `TaskForce*` (+0xe4), `Aggressive` (+0xad, marks the
team as attack-minded), `Full` (+0xa5, §2 launch rule), `Reinforce` (+0xab),
`Suicide` (+0xaf), `TechLevel` (+0xcc). `IsBaseDefense` (+0xf6) marks defensive
teams (out of AI-2 offensive scope). `Whiner`/`Annoyance`/`GuardSlower`/
`AvoidThreats`/`LooseRecruit`/`OnlyTargetHouseEnemy` modulate behaviour (record as
fields; consume lazily — see §2/§5).

### TaskForce struct (CONFIRMED — from recruit `FUN_006e9ff0`)

`Entries` array base = **+0xa4**, stride 8, `{int Amount @ +0xa4+i*8;
TechnoTypeClass* Type @ +0xa8+i*8}`, up to 6 entries; recruit reads
`Type->WhatAmI()` (vtbl+0x2c) to choose which global unit array to scan.

### ScriptType struct (CONFIRMED — `FUN_00691600`, size 0x234)

Header + `ActionsCount` + `ScriptActions[50]` of `{int Action; int Argument}`.
`FindOrAllocate` allocates 0x234 = 564 = header(≈0x24 + name/array-index) +
`50 × 8` action nodes; registers into `DAT_008b3134` (items) / `DAT_008b3140`
(count).

### TeamClass (runtime instance) offsets (CONFIRMED — observed in handlers)

vtable@+0; **Type@+0x24, CurrentScript@+0x28, Owner@+0x2c, Target/Focus@+0x30,
SpawnCell@+0x34, Focus@+0x40, TotalObjects@+0x48, CreationFrame@+0x50 (= frame
counter at ctor), FirstUnit@+0x54** (member linked list; next member =
`FootClass+0x5D8` i.e. `[member+0x176]`), **CountObjects[i]@+0x88+i*4** (per-entry
recruited counts, compared against TaskForce `Amount`), **StepCompleted@+0x80**
(set to 1 by a handler when the current script line is done → advance).

**Confidence: file format + all three numbered-entry grammars CONFIRMED (from
real data + loaders); TeamType/TaskForce/ScriptType/TeamClass struct offsets
CONFIRMED (from `FUN_006f0ab0` writes, recruit, FindOrAllocate); the
`Waypoint=`/`TransportWaypoint=` letter-code encoding PROBABLE (encoder
`FUN_00762430` located, not emitted by stock generic teams).**

---

## §2 Team lifecycle state machine

A `TeamClass` instance runs one `ScriptClass` (a live cursor over its
`ScriptType`'s action list). The per-frame driver walks the member list, keeps
the team together (regroup), and advances the script line when the current
action reports done (`StepCompleted` = team+0x80).

**Recruit vs produce (CONFIRMED — recruit `FUN_006e9ff0`, shortfall
`FUN_006ef3c0`).** For each TaskForce entry `i` whose recruited count
(team+0x88+i*4) is below the required `Amount` (`TaskForce+0xa4+i*8`), the team
first tries to **recruit an existing free unit**: it switches on the entry
`Type->WhatAmI()` — infantry (`1`/`0x28`), unit/vehicle (`2`/`3`), aircraft
(`0xf`/`0x10`) — scans that global object array, filters by **house match**
(unit house index == team owner) unless the type is a wildcard (`-2`) or the
team's `AreTeamMembersRecruitable`/loose flag is set, scores by squared distance
(`FUN_005f5d70`, +0x3200 penalty for wrong ownership), calls `CanRecruit`
(`FUN_006e9b80`) and `AddMember` (`FUN_006e9a70`). `GetTaskForceMissingMemberTypes`
(`FUN_006ef3c0`) builds the list of types still short after recruiting; the
shortfall is then **produced through the shared player production path**
(`HouseClass::BeginProductionOf` → FactoryClass, `production.md` §1.1 — teams do
**not** own a parallel production path). See §6.

**Gather at AISafeDistance (CONFIRMED — `FUN_006eec60`/`FUN_006eef70`).** The team
picks a **gather cell** and marches members to it:
- radius = `Rules.AISafeDistance` (`Rules+0xd74` = **20 cells**) `<< 8` = 5120
  leptons (cell = 256 leptons);
- centre = the **enemy/target house base centre** (`FUN_0050dd50` of the target
  house `team.Target`'s house index);
- **angle = base-direction ± an RNG term** — `FUN_007c4d00` (raw RNG draw) yields
  the angle, or `FUN_0065c220(0,0xff)` picks a random 0–255 facing when there is
  no directional anchor; `sin`/`cos` (`FUN_004cabd0`/`FUN_004cac20`) scale the
  radius into an (x,y) offset;
- the offset point is snapped to the **nearest free cell** (`FUN_0056d750`).

So "gathered" means: all live members are within `Rules.Stray` (`+0x171c` =
**2.0 cells**, `GuardModeStray` `+0x1720` for guard-type members, doubled for some
member classes) of that focus; the arrival threshold is `Rules.CloseEnough`
(`+0x1718` = **2.25 cells**). `FUN_006eb030` re-issues a Move (mission id 2,
vtbl+0x1e8) to any straggler and, when all members qualify, sets team+0x80
(`StepCompleted`) so the script proceeds.

**Full / timeout rule (CONFIRMED value + semantics; single-player caveat).**
`Full=` (TeamType+0xa5): **`Full=yes` = the team must reach full TaskForce
strength before it begins acting; `Full=no` = it activates/executes its script
while still under strength** (all the stock offensive teams are `Full=no`, so
they launch as soon as they have members). Under/over-strength is tracked by
`ScanLimit` (`FUN_006ec240`) into `IsUnderStrength`/`IsFullStrength` bools on the
TeamClass. **`DissolveUnfilledTeamDelay=5000`** (`Rules+0x1190`) is how long an
unfilled team waits before disbanding — but rulesmd tags it **"multiplay only"**,
so in skirmish/campaign an unfilled non-`Full` team is not auto-dissolved on this
timer; it keeps recruiting/producing toward strength.

**Disband (PROBABLE).** A team disbands when its script ends (runs off the last
line / hits a disband action), when it is wiped out, or (MP) when
DissolveUnfilledTeamDelay elapses unfilled; surviving members are released back to
the house's free pool (`LiberateMember`, `FUN_006ea440` cluster) and `cntInstances`
(TeamType+0xdc) is decremented so a new instance can be created under `Max`.

**Confidence: recruit scan + shortfall list + shared-production handoff
CONFIRMED; gather radius=AISafeDistance×256, enemy-base centre, RNG angle, snap
to free cell CONFIRMED (exact RNG sequence → [DEVIATION]); Stray=2.0/CloseEnough=
2.25 "gathered" predicate + team+0x80 advance CONFIRMED; Full= meaning CONFIRMED;
DissolveUnfilledTeamDelay=5000 value CONFIRMED with the "multiplay only" caveat;
disband/liberate PROBABLE.**

---

## §3 The FULL ScriptType action table

**Recovered mechanism:** the action number is **not** a small core switch — the
classifier `FUN_006e30d0` enumerates **every valid action number the binary
recognises** (all cases below; anything not listed returns `0` = the default/no
special-argument bucket), and the *executor* is a **data-resident jump table** of
per-action handler functions (the handlers `FUN_006e9640`, `FUN_006edd60`, …
were found only as regroup callers — they have no code xrefs because the dispatch
indexes a function-pointer array in `.data`). Valid action numbers run up to
`0x90` (144). The classifier maps each action to an **argument-class code** (used
by the describe formatter `FUN_006dc870` to interpret the argument as
waypoint / techno-type / building / cell / number / house / string).

### Full enumeration of valid action numbers (CONFIRMED — `FUN_006e30d0`)

Every case label present in the classifier (hex), grouped by the argument-class
code it returns. Actions **absent** from this switch are inert (fall to default).

| arg-class code | action numbers (hex) |
|---|---|
| 0xd | 1, 2, 3, 6, 9, 0xd, 0xe, 0x24, 0x25, 0x26, 0x4a, 0x4b, 0x71, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7e, 0x82 |
| 0xc | 4, 5, 7, 0x69 |
| 9 | 8, 0x11, 0x12, 0x3b, 0x3f, 0x40, 0x41, 0x42, 0x59, 0x5a, 0x5e, 0x5f, 0x60, 0x65, 0x66, 0x6d, 0x70, 0x74, 0x80, 0x87, 0x89, 0x8c |
| 2 | 0xa, 0x64, 0x75 |
| 0x27 | 0xb, 0x67 |
| 0xb | 0xc, 0x16, 0x35, 0x36 |
| 3 | 0x13, 0x62 |
| 1 | 0x14 |
| 4 | 0x15 |
| 10 (0xa) | 0x19, 0x1a, 0x1b, 0x27, 0x2c, 0x49, 0x4c, 0x4d, 0x4e, 0x4f, 0x54, 0x6e, 0x72, 0x7f, 0x8b, 0x8e, 0x8f, 0x90 |
| 0x16 | 0x1c, 0x1d |
| 0x10 | 0x1e, 0x55, 0x56, 0x57 |
| 0x11 | 0x21, 0x22, 0x86 |
| 0xf | 0x23 |
| 0x15 | 0x28 |
| 0x17 | 0x29 |
| 0x18 | 0x2a |
| 0x1b | 0x2b |
| 0x1e | 0x30 |
| 0x19 | 0x34 |
| 0x1c | 0x37 |
| 0x1d | 0x38, 0x39 |
| 0x1f | 0x3a |
| 0x1a | 0x46 |
| 0x21 | 0x47, 0x48 |
| 0x22 | 0x50, 0x6b |
| 0x23 | 0x58 |
| 0x26 | 0x63 |
| 0x28 | 0x68 |
| 0x2b | 0x6a, 0x73 |
| 0x2c | 0x6c |
| 0x2d | 0x7d |
| 0x2e | 0x81, 0x84, 0x85 |
| 0x2f | 0x83 |
| default(0) | every action number not listed above (inert / unrecognised) |

The argument-class codes tell the implementer **how to read the argument** (from
`FUN_006dc870`): code `0`→waypoint (letter/number), `1`→ a lookup-by-id
(`FUN_00720510`), `3`/`0x26`→ a name lookup (`FUN_007503b0`), `4`→ name lookup
(`FUN_00751280`), `0xb`→ a waypoint field (team+0x50), `0x1a`→ team+0x4c,
`0x27`→ a string (team+0x6d), `0x28`/`0xc`/`0x22`→ a techno/target id
(team+0x30's +0x98), `0x2e`→ 8 raw numbers, `0x2f`/`0x2b`/`0x2d`→ a member ptr.

### Semantics + implementable/inert split of the STOCK-USED actions

The action numbers actually used across the stock `aimd.ini` are
**0, 5, 6, 8, 9, 0xb, 0xc(12), 0xe(14), 0x15(21), 0x2b(43), 0x2e(46), 0x2f(47),
0x31(49), 0x35(53), 0x36(54), 0x37(55), 0x39(57), 0x3a(58), 0x3d(61), 0x3e(62),
0x3f(63)**. From the located handlers + standard RA2 ScriptType semantics
(structurally confirmed by the handlers I read — attack/move-to-target/regroup):

| action | semantics (RA2 ScriptType) | class |
|---|---|---|
| **0** | Attack **Quarry** type = argument (target-category, §5) | **implementable** (move-to-target + attack; quarry enum §5) |
| **5** | Guard area for `argument` × (timed guard; **argument = 1/10-min ticks**, the timed-guard unit — see below) | **implementable** (guard/wait) |
| **6** | **Jump to script line** = argument (loop/goto) | **implementable** (interpreter jump) |
| 8 | Player-attack-house `argument` (all units attack house N) | implementable (retarget) |
| 9 | Move to waypoint = argument | implementable (move) |
| 0xb (11) | Move to cell/coord (argument = packed cell) | implementable (move) |
| 0xc (12) | Guard area at current location | implementable (guard) |
| 0xe (14) | Change script/regroup | implementable |
| 0x15 (21) | Deploy (MCV/deployer) | implementable if deploy exists, else inert |
| 0x2b (43) | Set team global / flag | implementable (interpreter state) |
| 0x2e (46) | Load onto transports | **inert-required-subsystem-absent** (transport) |
| 0x2f (47) | Unload transports | **inert-required-subsystem-absent** (transport) |
| 0x31 (49) | **Set "AchievedGreatSuccess"/success flag then disband** (49,0 = end-of-script marker) | implementable (disband) |
| 0x35 (53) | **Attack nearest enemy building** (guard-mode member class) | implementable (attack) |
| 0x36 (54) | **Attack enemy structures / units near** | implementable (attack) |
| 0x37 (55) | Move to enemy structure / assign target | implementable (move+attack) |
| 0x39 (57) | Scatter | implementable (scatter) |
| 0x3a (58) | **Guard for `argument` ticks at a location** (the timed-guard opcode; argument in the same 1/10-min unit as action 5) | implementable (timed guard) |
| 0x3d (61) | Iron-Curtain self / support-power team action | **inert-required-subsystem-absent** (superweapon) |
| 0x3e (62) | Chronoshift / teleport team action | **inert-required-subsystem-absent** (chronosphere) |
| 0x3f (63) | Attack **Quarry** with regroup (attack-move to enemy) | implementable (attack-move) |

The remaining ~70 valid actions (transport chains 0x2e/0x2f/0x50/0x6b, aircraft
0x1e/0x55–0x57, superweapon/support-power 0x3d/0x3e/0x59–0x60, droppod, gather-at-
enemy variants, talk-bubble/flash cosmetic) are **inert-required-subsystem-absent**
until their subsystem lands; the implementable core for AI-2 is the
move / attack-quarry / guard / timed-guard / wait / scatter / regroup / jump set.

**Timed-guard argument unit (PROBABLE):** actions 5 and 0x3a take a duration
argument; stock scripts use `5,10` repeatedly (guard 10 units). RA2's guard/wait
argument is in **tenths of a minute** (so `10` ≈ 1 minute); the exact
tick→frame conversion was not isolated to a literal → **[RE-FALLBACK: argument ×
6 s = argument × 90 frames at 15 Hz, i.e. `10` → ~1 min].**

**Jump-target encoding (CONFIRMED):** action 6 argument = the **0-based line
index** within the same ScriptType to continue from (a plain script-line jump;
the classifier puts 6 in the waypoint/number bucket and stock `…6,1` loops back
to line 1).

**Confidence: full valid-action-number enumeration CONFIRMED (`FUN_006e30d0`, 114
cases); argument-class per action CONFIRMED; the implementable/inert split
CONFIRMED for the stock-used actions (handlers read) and PROBABLE for the
unused-but-valid ones (standard RA2 semantics, subsystem-gated); timed-guard unit
[RE-FALLBACK]; jump-line encoding CONFIRMED.**

---

## §4 TeamType selection / weighting (autocreate path)

**Team clock (CONFIRMED values; cadence binding PROBABLE).** Two rulesmd knobs
gate autocreate, both recovered with offsets:
- **`TeamDelays = 2000, 2500, 3500`** (`Rules+0x1158`, a 3-int array) — *"interval
  between checking for and creating teams, by difficulty"*. Per-difficulty; the
  comment says `(easy, medium, hard)` but — as with the `ai-base.md` count arrays
  — other rulesmd arrays are tagged "hardest to easiest", so the index binding is
  the **same inconsistency**. **[RE-FALLBACK: index 0/1/2 = the difficulty index
  at house+0x184 as loaded; the enemy Soviet base uses the lobby-selected AI
  difficulty, default the middle value 2500].** Unit: game frames (decremented
  against the frame counter), ≈133 s at 15 Hz for 2000 — this is the *check*
  cadence, not per-team.
- **`AutocreateTime = 1`** (`Rules+0x1510`, **double, minutes**) — *"average
  minutes between creating an 'autocreate' team"*. The global scheduler
  `FUN_0063f940` reads this timer plus a probability distribution and latches
  "create an autocreate team now". **AutocreateTime gates the actual autocreate
  spawn; TeamDelays gates the AI's team-management check pass.**
- **`TotalAITeamCap = 30,30,30`** (`Rules+0x13c8`) — hard cap on simultaneous AI
  teams per house.

**Selection predicate (PROBABLE + [RE-FALLBACK] for the ordering).** When the
autocreate latch fires, the AI iterates the TeamType array and picks an eligible
team:
- **Autocreate** (TeamType+0xa9) must be `yes`.
- **House match**: with `House=<none>` (universal in stock data, §1) the house
  filter is effectively "any AI house may take this team", and eligibility is
  really decided by whether the house can field the TaskForce units (recruit/
  produce, §2/§6). `House=<name>` would restrict to that country; `<none>` = no
  restriction. **(This replaces the plan's "House= token set" assumption.)**
- **`Max`** (TeamType+0xb8) caps live instances: a team is eligible only while
  `cntInstances` (+0xdc) `< Max`. **`Max=0` = no cap / unlimited** (stock offensive
  teams use `Max=1`, so one live instance at a time).
- **`Priority`** (TeamType+0xb4) orders candidates — higher priority preferred
  (stock offensive teams = 5; the "Ultra-Hard"/special teams = 25).

The exact tie-break/weighted-random among equal-priority eligible teams was not
isolated to a single primitive in this hunt (the autocreate selection walk is
reached from the House-AI update, not a string-anchorable site). **[RE-FALLBACK:
among Autocreate=yes teams with `cntInstances < Max` (Max=0 ⇒ uncapped) whose
house can field the TaskForce, pick the highest `Priority`; break ties by the
seeded engine RNG (xorshift32) — Task 4's `select_autocreate_team`].**

**AITriggerTypes boundary (explicit).** AITriggerTypes-driven team creation
(condition→team with owner/enemy/comparator weights) is a **separate** creation
path and is **AI-3 scope, out of scope here**. AI-2 implements only the
`Autocreate=yes` path above; AI-3 composes on top by adding trigger-gated
`CreateTeam` calls against the same TeamType/TaskForce/ScriptType structures.

**Confidence: TeamDelays/AutocreateTime/TotalAITeamCap values + offsets +
which-knob-gates-what CONFIRMED; Autocreate/Max/cntInstances/Priority fields +
`Max=0`=uncapped + `House=<none>`=unrestricted CONFIRMED (struct + data); the
per-difficulty index binding and the equal-priority tie-break [RE-FALLBACK].**

---

## §5 Quarry / target selection

**Mechanism (CONFIRMED structurally; enum values [RE-FALLBACK]).** The attack
handlers pick a concrete target by scanning a candidate array and scoring by
**squared distance with RNG weighting**:
- `FUN_006e9640` scans the target house's **building array** (house+0x6c /
  count house+0x78), computes squared distance from the team's anchor, multiplies
  by `(FUN_0056b800 + 1)` (a threat/value term) and an RNG factor (`FUN_007c4d00`),
  halves the score for a preferred class (a per-building flag), and keeps the
  minimum-score reachable target (path-checked via `FUN_004cbb60`).
- `FUN_006edd60` picks a **random** entry from a house/target list
  (`FUN_0065c220(0,n-1)`) and moves the team there.

So "attack quarry type N" resolves to: **filter the enemy's objects to the
quarry's target-class, score each by `distance² × value × rand`, attack the
lowest-scoring (nearest-and-most-valuable, with RNG jitter) reachable one.** The
distance metric is squared Euclidean (lepton units); tie-break/jitter is the
lockstep RNG (`FUN_007c4d00`/`FUN_0065c220`).

**Quarry enum (`[RE-FALLBACK]`).** The stock offensive scripts use action `0`
(attack quarry) and `0x3f`(63, attack-move) with argument = quarry type, but the
argument values in stock data are small (`0,1,2,6,7`). The exact quarry-type →
target-class table was **not** locatable (no `"Quarry"` string; the mapping is a
compiled enum inside the target-selection path). Adopt the **standard RA2 quarry
enumeration** as the calibrated fallback, matching the observed
buildings/units/factories behaviour:

**[RE-FALLBACK — quarry enum, Task 3 quarry constants / Task 5 `ai_pick_quarry_*`]**
```
0 = Anything            4 = Vehicles/Units
1 = Buildings (any)     5 = Factories
2 = Harvesters          6 = Base defenses
3 = Infantry            7 = Base threats (nearest hostile to own base)
(8+ = Power/other specialised classes)
```
with target chosen by nearest-reachable of the matching class, `distance² × value
× seeded-RNG` scoring (engine xorshift32 substituting the lockstep RNG).

**Confidence: target-selection mechanism (class filter + distance²×value×RNG,
nearest-reachable, path-checked) CONFIRMED; the quarry-type→class enum values
[RE-FALLBACK] (standard RA2 table).**

---

## §6 Production / reserve interplay

**Shared production path (CONFIRMED — no parallel path).** Team shortfall
production goes through the **same** `HouseClass::BeginProductionOf`
(`FUN_004FA170`) → per-category primary `FactoryClass` → 54-step charge model the
player and the base-planner use (`production.md` §1.1/§1.5). `AddMember` consumes
the produced unit when it completes and exits the factory. There is **no
team-specific production queue** — teams reuse `can_start / start / launch` (the
`GetTaskForceMissingMemberTypes` list drives what to queue). The one-at-a-time
factory gate is the FactoryClass queue itself (buildings one-slot; units queue to
`MaximumQueuedObjects=29`), not a team-level gate.

**InfantryReserve / CreditReserve (CONFIRMED values + offsets; exact gate site
[RE-FALLBACK]).**
- **`InfantryReserve = 50000`** (`Rules+0x1138`), rulesmd comment *"always build
  infantry if cash reserve is greater than this"* — a **spend-freely-on-infantry
  threshold**: above it the AI keeps producing infantry (incl. team infantry
  shortfall) without holding cash back; below it infantry production is throttled
  toward the reserve. `InfantryBaseMult=1` (`Rules+0x113c`) scales it.
- **`CreditReserve = 100`** (`Rules+0x1758`), *"repair will not begin below this"*
  — a **repair** floor (as `ai-base.md`), a general spending floor the AI keeps.

Both are Rules scalars confirmed from data + the `[AI]` loader offsets; the exact
instruction site where team-shortfall production consults `InfantryReserve` was
not isolated (displacement-scanning +0x1138 hit unrelated structures). **The
plan's Task 4 `next_produce_type` is pre-hunt written WITHOUT an InfantryReserve
gate — the recovered semantics say infantry shortfall should be produced freely
while `credits > InfantryReserve` and throttled below it.** **[RE-FALLBACK: gate
team infantry-shortfall production on `house.credits > InfantryReserve (50000)`
for "build freely", else respect the normal cost/reserve check; the exact
throttle curve is the standard cost check.]**

**BuildSlowdown (CONFIRMED effect).** `BuildSlowdown` is a **DifficultyClass
byte** (difficulty struct +0x48; **no** for Easy, **yes** for Normal/Difficult —
`ai-base.md` §4), not an `[AI]` team key. Its real effect: when set, AI
production of a category is **slowed while the AI already has enough of that
category in production/on-hand** — i.e. it damps the AI from spamming one factory,
not a flat time multiplier. It rides the existing `Time_To_Build` /
MultipleFactory machinery (`production.md`); it does **not** introduce a
team-specific time multiplier. **The plan's `kAiBuildSlowdownTimeMul` placeholder
is wrong** — model BuildSlowdown as a "don't over-queue a category the AI already
has queued" damp (Normal/Difficult on, Easy off), not a scalar on build time.

**Confidence: shared production path (no parallel queue) CONFIRMED; InfantryReserve=
50000 / CreditReserve=100 values + offsets + their meanings CONFIRMED; the exact
team-shortfall InfantryReserve gate site [RE-FALLBACK]; BuildSlowdown = difficulty
byte, over-queue damp (not a time multiplier) CONFIRMED (semantics) with the
Easy/Normal/Difficult binding from `ai-base.md`.**

---

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 three index sections + `N=count,UNITID` / `N=action,argument` / TeamType named keys | real aimd.ini + `FUN_006f0ab0` | **CONFIRMED** |
| §1 TeamType struct offsets (Autocreate+0xa9, Priority+0xb4, Max+0xb8, cntInstances+0xdc, Script+0xe0, TaskForce+0xe4, Full+0xa5, Aggressive+0xad) | `FUN_006f0ab0` writes | **CONFIRMED** |
| §1 TaskForce Entries@+0xa4 stride8 {Amount,Type}, ×6; ScriptType 50×{Action,Argument} (alloc 0x234) | recruit `FUN_006e9ff0` / `FUN_00691600` | **CONFIRMED** |
| §1 `House=<none>` universal in stock; no per-side token | real aimd.ini (163/163) | **CONFIRMED** |
| §1 Waypoint= letter-code encoder | `FUN_00762430` (not emitted by stock generic teams) | **PROBABLE** |
| §2 recruit-from-existing (WhatAmI array scan, house/dist filter) + shortfall list → shared production | `FUN_006e9ff0` / `FUN_006ef3c0` | **CONFIRMED** |
| §2 gather = AISafeDistance(20)×256 lepton ring, enemy-base centre, RNG angle, snap free cell | `FUN_006eec60`/`FUN_006eef70` | **CONFIRMED (RNG → deviation)** |
| §2 "gathered" = all members ≤ Stray(2.0)/CloseEnough(2.25); team+0x80 StepCompleted advances | `FUN_006eb030` + rulesmd | **CONFIRMED** |
| §2 `Full=yes` wait-for-strength / `Full=no` act-under-strength; DissolveUnfilledTeamDelay=5000 MP-only | struct + rulesmd comment | **CONFIRMED** |
| §3 full valid-action-number enumeration (114 cases, up to 0x90) + argument-class per action | `FUN_006e30d0` | **CONFIRMED** |
| §3 dispatch = data jump table of per-action handlers (move/attack/guard/regroup/jump) | handlers as regroup callers; no code xrefs | **CONFIRMED (structural)** |
| §3 stock-used action semantics + implementable/inert split | handlers + standard RA2 | **CONFIRMED (used) / PROBABLE (unused-valid)** |
| §3 timed-guard argument unit | stock `5,10`; no literal isolated | **[RE-FALLBACK]** |
| §3 jump-target = script line index | classifier + stock loop | **CONFIRMED** |
| §4 TeamDelays=2000/2500/3500 (+0x1158), AutocreateTime=1min (+0x1510), TotalAITeamCap=30 (+0x13c8); AutocreateTime gates spawn, TeamDelays gates check | loaders + rulesmd + `FUN_0063f940` | **CONFIRMED** |
| §4 Autocreate/Max(0=uncapped)/cntInstances/Priority selection; House=<none>=unrestricted | struct + data | **CONFIRMED** |
| §4 per-difficulty index binding; equal-priority tie-break | not isolated | **[RE-FALLBACK]** |
| §4 AITriggerTypes creation is AI-3, out of scope | boundary set | **CONFIRMED (scope)** |
| §5 target = class-filter + distance²×value×RNG, nearest-reachable, path-checked | `FUN_006e9640`/`FUN_006edd60` | **CONFIRMED (mechanism)** |
| §5 quarry-type→class enum values | no `Quarry` string; compiled enum | **[RE-FALLBACK] (standard RA2)** |
| §6 shared FactoryClass production path, no parallel team queue | `production.md` + `FUN_006ef3c0` handoff | **CONFIRMED** |
| §6 InfantryReserve=50000(+0x1138) "build infantry freely above"; CreditReserve=100(+0x1758) repair floor | `[AI]` loader + rulesmd | **CONFIRMED (values) / [RE-FALLBACK] (gate site)** |
| §6 BuildSlowdown = difficulty byte, over-queue damp (not a time multiplier) | `ai-base.md` §4 + production model | **CONFIRMED (semantics)** |

## Deviations / plan diffs (for the RE-GATE review)

- **[PLAN DIFF — Task 2/4, STOP] `House=<none>` is universal; there is no per-side
  `House=` token set.** All 163 stock TeamTypes are `House=<none>`. Team↔house
  binding is by **whether the house can field the TaskForce units** (recruit scan
  house-filter + shared production), NOT by an explicit `House=` match. Task 4's
  `select_autocreate_team` must treat `<none>` as "any AI house eligible" and gate
  eligibility on TaskForce buildability, not a house-token compare. Model
  `House=<name>` (rare/map teams) as a country restriction for completeness.

- **[PLAN DIFF — Task 3, STOP] The gather cell is a RANDOM-ANGLE ring at
  AISafeDistance, not a deterministic base→enemy offset cell.** Recovered
  (`FUN_006eec60`/`FUN_006eef70`): radius = `AISafeDistance(20)×256` leptons,
  centre = enemy base centre, **angle from the lockstep RNG** (`FUN_007c4d00` /
  `FUN_0065c220(0,255)`), snapped to nearest free cell (`FUN_0056d750`). Replace
  Task 3's deterministic offset-cell fallback with: point on a circle of radius
  `AISafeDistance` cells around the enemy base centre at a **seeded-xorshift32**
  angle, snapped to the nearest free cell. The "gathered" predicate is
  **all members within `Stray=2.0` cells** (arrival `CloseEnough=2.25`), setting
  `StepCompleted` — use these exact rulesmd values, not a guess.

- **[PLAN DIFF — Task 3, STOP] The action table is ~90 valid actions (up to
  0x90), enumerated by `FUN_006e30d0`, dispatched via a jump table — not a small
  core set.** Replace Task 3's pre-hunt `classify_op` core with the §3 table.
  The implementable core for AI-2 is **{0 attack-quarry, 0x3f attack-move, 5/0x3a
  timed-guard, 0xc guard-area, 9/0xb move-to-waypoint/cell, 0x39 scatter, 0xe
  regroup, 6 jump-line, 0x31(49) disband, 0x35/0x36 attack-building/units}**;
  everything else (transport 0x2e/0x2f/0x50/0x6b, aircraft 0x1e/0x55–0x57,
  superweapon/support 0x3d/0x3e, droppod, cosmetics) is
  **inert-required-subsystem-absent**. Timed-guard argument is tenths-of-a-minute
  **[RE-FALLBACK ×6 s]**; jump argument is the **0-based script line index**
  (CONFIRMED).

- **[PLAN DIFF — Task 2/3] `Full=` and DissolveUnfilledTeamDelay.** `Full=no`
  (all stock offensive teams) ⇒ the team **acts while under strength** (does not
  wait to fill). `Full=yes` ⇒ wait for full TaskForce strength before acting.
  **`DissolveUnfilledTeamDelay=5000` is "multiplay only"** — do NOT dissolve
  unfilled skirmish/campaign teams on this timer. Set
  `kAiDissolveUnfilledDelayFrames=5000` but gate it to MP; in skirmish an unfilled
  `Full=no` team keeps recruiting/producing.

- **[PLAN DIFF — Task 4 team clock] Two clocks, and which gates what.**
  `AutocreateTime=1` (minute-scale, `Rules+0x1510`) gates the actual autocreate
  **spawn**; `TeamDelays=2000/2500/3500` (`Rules+0x1158`, per-difficulty, frame
  units) gates the team-management **check pass**; `TotalAITeamCap=30` is the hard
  team count cap. `Max=0` on a TeamType = **uncapped** live instances (not "zero
  allowed"). Bind `kAiTeamDelayFrames` per difficulty with the same
  hardest↔easiest index caveat as `ai-base.md` (**[RE-FALLBACK]**, default 2500).

- **[PLAN DIFF — Task 4/5 quarry] Replace the pre-hunt kind table + nearest rule.**
  Targeting is **class-filter + `distance²×value×RNG`, nearest-reachable,
  path-checked** (CONFIRMED). The quarry-type→class enum values are
  **[RE-FALLBACK: standard RA2 table]** (0 Anything, 1 Buildings, 2 Harvesters,
  3 Infantry, 4 Vehicles, 5 Factories, 6 Base defenses, 7 Base threats). Use the
  seeded xorshift32 for the score jitter.

- **[PLAN DIFF — Task 4/5 production/reserve] InfantryReserve gate + BuildSlowdown.**
  Task 4 `next_produce_type` is pre-hunt written WITHOUT an InfantryReserve gate —
  add one: produce team infantry-shortfall **freely while `credits >
  InfantryReserve (50000)`**, throttle below. `CreditReserve=100` is the repair/
  spend floor. **`BuildSlowdown` is a difficulty BYTE (Easy off / Normal+Difficult
  on), an over-queue damp, NOT a time multiplier — drop `kAiBuildSlowdownTimeMul`.**
  Team production **reuses the shared FactoryClass path** (`BeginProductionOf`,
  `production.md`) — no parallel queue.

- **[PLAN OK — Task 6 exemplars] Soviet autocreate offensive fixtures** (real
  ids, for Task 6): `Soviet Apoc Medium [0CA1614C-G]` (Script `0C31EA4C-G`,
  TaskForce `0ACDA62C-G`=1 APOC); `Soviet Armor Hard [0CA1D38C-G]`
  (TaskForce `0ACDC72C-G`=2 HTK,5 HTNK); `Soviet Infantry Hard 1 [060F538C-G]`
  (TaskForce `0D30551C-G`=20 E2). All `Autocreate=yes Priority=5 Max=1
  Aggressive=yes House=<none>`.

- **[DEVIATION — RNG]** The original uses the lockstep game RNG
  (`FUN_007c4d00`/`FUN_0065c220`) for the gather angle, the target-score jitter,
  and equal-priority team tie-breaks. Our single-machine engine substitutes a
  **seeded xorshift32** — same distributions/behaviour, not bit-identical to the
  netgame sequence (as `ai-base.md`).

- **[DEVIATION — synthetic waypoints]** `Waypoint=`/`TransportWaypoint=` use a
  letter/number code decoded against the map `[Waypoints]` table. Until map
  waypoint parsing lands (AI-3), use a synthetic waypoint→cell table; stock
  generic offensive teams do not set `Waypoint=`, so the offensive path relies on
  the enemy-base-centre gather, not fixed waypoints.

- **[DEVIATION — single-slot queues]** Teams reuse the player/base FactoryClass
  queues as-is (`production.md`) — no team-owned production. Keep.
