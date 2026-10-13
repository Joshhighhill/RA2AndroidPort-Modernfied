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

## §3 The ScriptType action table (AMENDED by gap fix G-61, 2026-10)

> **Correction.** The previous version of this section took its numbering from
> `FUN_006e30d0`. That function is the **map-trigger `[Actions]` argument
> classifier** (it is called from `FUN_006dc870`, which the P9 hunt dumped as a
> trigger-action helper, `p9out/actpre_006dc870.c:17`), **not** the team-script
> executor, and its 0..0x90 case labels are TAction numbers. Every AI-2 opcode
> number derived from it (8 = attack house, 9 = move to waypoint, 11 = move to
> cell, 12 = guard, 14 = regroup, 53/54 = attack building/units, 57 = scatter,
> 58 = timed guard, 63 = attack-move) was wrong, and the "stock-used" list did
> not match the shipped `aimd.ini`. The old table is retired; the one below is
> what `engine/script_actions.hpp` implements and the P10 campaign interpreter
> shares.

**Executor (CONFIRMED, `D:\ra2-re\p10out\c13.log:29-33`).** TeamClass reads the
current line's `(action, argument)` and does `CMP EAX,0x40 / JA <no-op> /
JMP [EAX*4 + 0x6E94E4]` — a **65-entry jump table, actions 0..64**. `c14.log`
dumps each handler; every anchor read agrees with the ModEnc / YRpp
`TeamMissionType` numbering (campaign.md §7). A handler that finishes its line
sets `team+0x80` ("step complete"); 49 additionally sets `team+0x84` (success).

| # | name | handler evidence | AI-2 behaviour |
|---:|---|---|---|
| 0 | Attack quarry (arg = QuarryType) | `FUN_006ec5f0` | attack while a target of that class exists |
| 1 | Attack waypoint | `FUN_006ebf00` | walk to the waypoint [RE-FALLBACK] |
| 2 | Go berserk | handler **is** the shared epilogue (no-op, line never completes) | inert |
| 3 | Move to waypoint | `FUN_006ebd30` | move, advance on CloseEnough arrival |
| 4 | Move to cell | `x = arg % 128, y = arg / 128` → `FUN_005653d0` | move |
| 5 | Guard area for arg × **15 frames** | `LEA ECX,[ECX+ECX*2]; LEA ECX,[ECX+ECX*4]` → timer +0x58 | timed guard |
| 6 | Jump to line (arg **1-based**) | `ADD ECX,-2` → `FUN_00690fa0`, then the cursor advances | jump (consumes the tick) |
| 7 / 23 | Player wins / loses | `FUN_004fc800` / `FUN_004fc9f0` | (campaign only) |
| 8 / 9 / 14 / 43 | Unload / deploy / load / wait loaded | `FUN_006ee670` / `FUN_006eca30` / `FUN_006ec760` | inert (no AI transports) |
| 10 | Follow friendlies | `FUN_006ed0b0` | inert |
| 11 | Do mission (arg = YRpp Mission) | `FUN_006ecd40` | Hunt (15) → attack anything; other missions → guard in place |
| 12 / 38 / 39 / 40 | Set/clear global, set/clear local | `PUSH 1/0` → `FUN_00689070` / `FUN_00689310` | (campaign only) |
| 16 | Patrol to waypoint | `FUN_006ec240` | move [RE-FALLBACK] |
| 20 / 37 | Change house / delete members | — | (campaign only) |
| 21 | Scatter | every member `vfunc+0x174` | scatter, one-shot |
| 46 | Attack enemy building (arg = type \| selector<<16) | `FUN_006ed870` | attack that building; advance when none |
| 47 | Move to enemy building (same encoding) | `FUN_006ed950` | move; advance when none |
| 49 | Success | `team+0x80 = 1; team+0x84 = 1` | **flag success and CONTINUE** (not a disband) |
| 53 | Gather at enemy base | `FUN_006eec60` | move to the AISafeDistance ring (§2) |
| 54 | Regroup at friendly base | `FUN_006eef70` | move to the own base centre |
| 55 / 56 / 57 | Iron Curtain me / chrono prep | `FUN_006ef1d0` / `FUN_006ef3c0` / `FUN_006ef690` | inert (no AI SW team actions) |
| 58 | Move to friendly building (same encoding as 46) | `FUN_006edb20` | move; advance when none |
| 59 | (attack structure) | `FUN_006ebfd0` | inert |
| 60..64 | for each member: if `vfunc(+0x33c..+0x34c)` → remove from team (`FUN_006e9dd0`), line complete | inline | inert (no member ever matches) |

**Building-target argument (46/47/58).** Low word = the rulesmd
`[BuildingTypes]` index (0-based list order — stock: `46,9` = NAPOWR, `47,7` =
GAWEAP, `58,1` = GAREFN, `58,196623` = 0x3000F = NAREFN), high word = which
instance (ModEnc: 0 least threat, 1 highest threat, 2 nearest, 3 farthest;
threat is not modelled, 0 → nearest and 1 → farthest [RE-FALLBACK]).

**QuarryType (action 0's argument)** is YRpp's: 1 Anything, 2 Buildings,
3 Harvesters, 4 Infantry, 5 Vehicles, 6 Factories, 7 Base defenses, 8 Threats,
9 Power, 10 Occupiable, 11 Tech buildings — confirmed by the stock script
names ("General Harvester Hunt" `0,3`, "General Factory Attack" `0,6`, "General
Base Defense Attack" `0,7`, generic tails `0,2` then `0,1`). The previous
0-based enum (0 = Anything) was off by one.

**Stock `aimd.ini` census** (88 ScriptTypes, action: uses): 0:136, 49:54,
58:49, 46:45, 54:34, 5:30, 47:24, 53:22, 11:16, 6:10, 8:6, 14:9, 43:9, 63:4,
9:3, 55:3, 21:1, 57:1, 61:1, 62:1. Actions 1-4 do not occur in `aimd.ini`
(they are map-script actions).

**Confidence:** table bounds and anchors CONFIRMED (c13/c14); guard unit ×15,
1-based jump, cell decode, 49 = flag-and-continue CONFIRMED from the handler
bytes; building-argument encoding CONFIRMED against stock data; QuarryType
enum PROBABLE (YRpp + stock names); 1/16 as plain moves, 11 mission mapping and
the threat selectors [RE-FALLBACK].

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

## Wave 7 (2026-10): naval muster, per-unit AI rules, capture fate

- **Naval teams muster on water (W7-06-01).** The pre-script muster at the own
  base (W2-SK-06, itself a [RE-FALLBACK]) is a land cell for a Naval TaskForce,
  so an all-ship team never satisfied the Stray predicate and sat in Gathering
  for good. `ai_execute_team_command` (MoveTeam) now snaps the focus of a team
  whose live members are all Water-domain to the free water cell, reachable from
  the lead ship (4-way flood fill), nearest the ideal cell; each ship's ring cell
  is snapped the same way. No RE shows a land muster for a naval team.
- **Infantry auto-deploy (W7-01-08 / W7-02-04).** `InfantryClass FUN_00521050`
  (called from the Guard / Area_Guard overrides 0x51F330 / 0x51F350): a
  computer-owned infantry with `Deployer` (+0xEC8) and `DeployFire` (+0x6AC),
  `UndeployDelay` (+0x6C4) < 0 and **not** `ImmuneToRadiation` (+0xD37) plays
  Deploy (Sequence 0x1B) once `Frame > CurrentMissionStartTime (+0xC0) +
  Rules+0xE30[house+0x184]` = `[General] AIAutoDeployFrameDelay=15,25,100`
  (h,m,e). `FUN_00522070` (Attack): deploy when the target is in the
  DeployFireWeapon's range, undeploy (0x1F) when not; a deploy weapon with
  `AreaFire` (WeaponType +0x150) is excluded. `FUN_0051F250` (Hunt): a deployed
  computer infantry with no target undeploys. `FUN_0051F370` (Move): it
  undeploys first. **Correction to W7-02-04:** the Desolator is ImmuneToRadiation
  and its RadEruptionWeapon is AreaFire, so neither branch deploys it -- a
  computer Desolator deploys only through a team-script Deploy line; once
  deployed, 0x521050 re-erupts while the cell's RadLevel is under
  `Weapon[1].RadLevel/3`. Implemented in `engine/ai_unit.cpp` (skirmish AI
  houses; the ArchiveTarget cell test @0x5210E0 is not modelled).
- **ToProtect (W7-04-18).** `ReceiveDamage FUN_00700DA0 @0x701C43 ->
  FUN_00707500`: a hit `ToProtect=yes` unit (HARV, CMIN, the navy) of a computer
  house, attacked by a non-allied Infantry/Unit, calls up to six recruitable,
  unteamed (or base-defense-team) objects of its house that can reach
  (FUN_0056CC30, movement zone) and fire on the attacker, ranked by threat value
  within the attacker-threat x Rules+0xB18 budget. fv7c: implemented as ONE
  routine for every computer house, `World::call_for_help` (world_w7.cpp):
  ThreatPosed (vtbl+0x2C0 = FUN_00707FC0 -> type +0x670) budget x
  ComputerBaseDefenseResponse, FUN_004D96B0 value, GetFireError(attacker, 0),
  Area Guard (roll >= 66) / Rescue, attacker cooldown BaseDefenseDelay x 900
  armed only when the budget is spent. [RE-OPEN: the x100/x10 multipliers on
  victim +0x218, unit tests UnitType +0x5EC / unit +0x2E4 / victim +0x2DC.]
- **Capture fate (W7-10-01 / W7-08-12).** `CaptureManagerClass::DecideUnitFate
  FUN_004722E0` (after Capture 0x471C80 and Free 0x471F20): the victim leaves its
  team; for a computer owner the weights `[General] AICaptureNormal /
  AICaptureWounded / AICaptureLowPower / AICaptureLowMoney` (chosen by
  `AICaptureLowMoneyMark`, power, `AICaptureWoundedMark`) are walked by a 1..100
  roll into 1 Add To Team, 2 Grinder, 3 Bio Reactor, 4 Hunt; the capturer
  TeamType's `MindControlDecision` (+0xC0) overrides (5 = nothing); a failed
  option is Mission Hunt. Implemented in `campaign_runtime.inc`
  (`decide_unit_fate`).
