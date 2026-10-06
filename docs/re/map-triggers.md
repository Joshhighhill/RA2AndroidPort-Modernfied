# Map triggers / scripting (`[Triggers]` `[Events]` `[Actions]` `[Tags]`) — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`
SHA1 `7BE4E550…B6C7`. Hunt scripts `p9_trig1.py` … `p9_trig10.py` +
`p9_common.py`, all off-repo under `D:\ra2-re\scripts\`; decompiles/listings
under `D:\ra2-re\p9out\` against a private copy of the analysed project
(`D:\ra2-re\p9proj`, so the parallel phases' Ghidra sessions are not locked).
Nothing under `D:\ra2-re` is committed. **Content-anchored only**: every function
below was located from the INI section-name strings the loaders consume
(`"Triggers"` `0x8439DC`, `"Events"` `0x8439CC`, `"Actions"` `0x8439D4`, `"Tags"`
`0x841BDC`, `"CellTags"` `0x81EFF4`, `"VariableNames"` `0x83C81C` — reached through
a **pointer table** at `0x7F48C8`/`0x7F48CC`/`0x7F48D0`/`0x7F3550`, which is why a
direct-xref search for the strings returns nothing), the debug source path
`"D:\Ra2 Addon\TAction.CPP"` (`0x841B98`, referenced only by the action executor),
and then xref/caller walks. YRpp `TEventClass.h`/`TActionClass.h`/
`TriggerTypeClass.h`/`TagTypeClass.h`/`TriggerClass.h`/`TagClass.h` and the
`TriggerEvent`/`TriggerAction`/`TriggerPersistence` enums in
`GeneralDefinitions.h` were read **for layouts and names only**; every published
address in this cluster drifted (see "YRpp drift"), several by more than 0xA00.)

Companion docs: `maps.md` (map file container, `[Waypoints]` decode, the default
gate map is the campaign mission `sov07tmd.map`), `ai-team.md` (TeamTypes /
TaskForces / ScriptTypes — the **same INI format** a map's own
`[TeamTypes]`/`[TaskForces]`/`[ScriptTypes]` use), `ai-trigger.md`
(`[AITriggerTypes]` — a *different* system: weighted AI team selection, not map
scripting).

Confidence labels: **CONFIRMED** (decompile/disassembly of `gamemd.exe` + real
data on this install agree), **PROBABLE** (binary structure confirmed, a name or
receiver taken from the YRpp layout), **[RE-FALLBACK]** (not determined; what was
ruled out).

---

## Located functions (this build)

| Function | Role | How found |
|---|---|---|
| `FUN_007264e0` | **TriggerTypeClass::ReadINI** — parses one `[Triggers]` line, then its `[Events]` and `[Actions]` lines (§1.1–1.3) | only reader of `PTR "Triggers"`+`"Events"`+`"Actions"` together (gap function, created at `0x7264E0`) |
| `FUN_00726870` | TriggerType list loader — for each `[Triggers]` key, find-or-create (`new(0xB4)`) then ReadINI | `PTR "Triggers"` user; called from the scenario loader |
| `FUN_00726b20` | TriggerType list **writer** (clears + rewrites the three sections) | `PTR "Triggers"`+`"Events"`+`"Actions"` user, vtbl `+0x68` WriteINI |
| `FUN_00726d40` | TriggerType find-or-create by ID (forward references) | callee of action ReadINI kind 2 |
| `FUN_00725f20` | TriggerTypeClass ctor (difficulty flags default 1, array registration) | `new(0xB4)` sites |
| `FUN_0071e780` | **TEventClass::ReadINI** (strtok continuation, §1.2) | TriggerType ReadINI callee per event |
| `FUN_006dcb20` | **TActionClass::ReadINI** (§1.3, the "kind" switch) | TriggerType ReadINI callee per action |
| `FUN_007624b0` | waypoint-letter decode (`A`=0 … `Z`=25, `AA`=26 …) | action ReadINI tail |
| `FUN_006e55f0` | **TagTypeClass::ReadINI** (§1.4) | `PTR "Tags"` user (gap function) |
| `FUN_006e5440` | TagType list loader (find-or-create `new(0xA4)` + ReadINI) | `PTR "Tags"` user |
| `FUN_006e5880` | TagType find-or-create by ID (forward refs from actions/TeamTypes/CellTags) | callee of action ReadINI kind 3, CellTags, TeamType read |
| `FUN_00689520` | map `[VariableNames]` → the **100 locals** (§1.5) | `"VariableNames"` user |
| `FUN_00686560` | scenario INI load; also fills the **50 global** names from the rules `[VariableNames]` | `"VariableNames"` user + `Scenario.CPP` |
| `FUN_004acd80` | map object/cell load incl. `[CellTags]` (§1.6) | `"CellTags"` user |
| `FUN_0071e920` | **TEvent attach flags** per event kind (§3.1) | sole callee of TriggerType GetFlags |
| `FUN_006e3450` | TAction attach flags per action kind | sole callee of TriggerType GetFlags |
| `FUN_00726480` | TriggerType GetFlags = OR over events+actions, recursing through the linked trigger | callers `FUN_006e5760` (TagType flags) |
| `FUN_00684670` | **scenario start**: builds the Map / Logic / per-House tag lists (§3.2) | user of the logic-list globals `0x8B3034/0x8B3040` |
| `FUN_006e4810` | TagClass::GetInstance(TagType) — find existing instance or `new(0x38)` | scenario start + CellTags |
| `FUN_006e4350` | TagClass ctor — makes one **new** TriggerClass per TriggerType in the chain | `new(0x38)` site |
| `FUN_00725240` | TriggerClass ctor (`new(0x48)`) — Enabled=1, difficulty gate, **ResetTimers** | TagClass ctor callee |
| `FUN_007256a0` | **TriggerClass::ResetTimers** (§3.5) | ctor/Enable/variable-change callee |
| `FUN_006e4910` | **TagClass::RaiseEvent** — persistence handling (§3.4) | callee of every event raise site |
| `FUN_00725760` | **TriggerClass::RegisterEvent** — AND over events + latch (§3.3) | RaiseEvent callee |
| `FUN_0071dbe0` | **TEventClass::HasOccurred** — the per-kind predicate switch (§4) | RegisterEvent callee (2264 bytes) |
| `FUN_00725860` | **TriggerClass::FireActions** — runs every action in order | RaiseEvent / ForceTrigger callee |
| `FUN_006dce20` | **TActionClass::Execute** — the 146-way action switch (§5) | sole `TAction.CPP` code user; FireActions callee |
| `FUN_007259c0` | TriggerClass::Destroy (Destroyed=1, queue for deletion) | RaiseEvent / DestroyTrigger |
| `FUN_00725b90` / `FUN_00725ba0` | TriggerClass Enable (+ResetTimers) / Disable | EnableTrigger / DisableTrigger actions |
| `FUN_00689070` / `FUN_00689310` | set global (50) / set local (100) — sets the "variables changed" flag + notifies triggers (§3.6) | GlobalSet/LocalSet action targets |
| `FUN_00689160` / `FUN_00689400` | get global / get local | HasOccurred cases 27/28/36/37 |
| `FUN_0055ac40` | **LogicClass::AI** — per-frame logic-tag raise loop (§3.2) | the only user of the logic-list loop index `0xA82C44` besides the tag dtor |
| `FUN_004f8350` | HouseClass::AI — raises **kind 8** on the house's tag list every 8 frames (§3.2) | `PUSH 0x8` raise call site |
| `FUN_00502b50` | HouseClass find by HouseType index (`house->Type->ArrayIndex == idx`) | content |
| `FUN_006e3b50` | TAction FindHouseByIndex (`8997` = trigger's house, `-1` = none, else by type index) | content |
| `FUN_004fc800` / `FUN_004fc9f0` | HouseClass Flag_To_Win / Flag_To_Lose (sets `+0x1F7` "winner" / the lose path that logs "BorrowedTime") | Win/Lose action targets; `House.CPP` users |

### YRpp drift (this build)

`TActionClass::Execute 0x6DD8B0`→**`0x6DCE20`** (−0xA90; YRpp's `LoadFromINI
0x6DD5B0` and ctor `0x6DD000` both land *inside* this one function here);
`TActionClass::LoadFromINI`→**`0x6DCB20`**; `GetMode 0x6E3B60`→`0x6E3B50` is
actually *FindHouseByIndex*; `TEventClass::HasOccured 0x71E940`→**`0x71DBE0`**
(the YRpp address is the attach-flags function `0x71E920`);
`TEventClass::LoadFromINI 0x71F4E0`→**`0x71E780`** (the YRpp address is theme
code); `TriggerClass::RegisterEvent 0x7264C0`→**`0x725760`**; `FireActions
0x7265C0`→**`0x725860`**; `TagClass::RaiseEvent 0x6E53A0`→**`0x6E4910`**;
`TriggerTypeClass::LoadFromINIList 0x7275D0` is the *Tubes* writer here. As in
every prior hunt: **layouts and enum names held, addresses did not**. Every role
in the table above was assigned by content (which section it parses, which
fields it touches), not by the hint.

---

## §1 File formats (all CONFIRMED — ReadINI decompiles + 44 campaign maps parsed with 0 malformed lines)

### 1.1 `[Triggers]` — `ID = House,LinkedTrigger,Name,Disabled,Easy,Normal,Hard,MustTransfer`

`FUN_007264e0` strtoks the value on `,`:

| # | field | stored | rule |
|---|---|---|---|
| 1 | **House** | `+0xA4` HouseType* | `<none>` → HouseType[0]; else `HouseTypeClass::FindIndex(name)` (§7 index space) |
| 2 | **LinkedTrigger** | `+0xA8` NextTrigger | `<none>` → null; else find-or-create TriggerType by ID (forward refs allowed). **This is the tag's trigger chain** (§3.2), not a "fire this too" link. |
| 3 | **Name** | `+0x64` (0x30 chars) | display only |
| 4 | **Disabled** | `+0x9F` Enabled = `!(atoi != 0)`; a *missing* field also leaves Enabled=0 | |
| 5/6/7 | **Easy / Normal / Hard** | `+0x9C/+0x9D/+0x9E` | `atoi != 0` |
| 8 | **MustTransfer** | `+0xA0` | set only when non-zero (vehicle-thief tag carry) |

Real line (sov07tmd): `0D06808C=BadGuy2,<none>,Soviets Build Nuke (M),1,0,1,0,0`.

### 1.2 `[Events]` — `ID = N, (Kind,Param,Value[,String])×N`

TriggerType ReadINI reads the count, then calls `FUN_0071e780` N times. Each event:
`Kind=atoi`, `Param=atoi`, `Value=atoi`; **only when `Param==2` an extra string
token follows** (a TechnoType ID, 0x18 chars at `+0x38`, used by events 60/61).
`Param==0` → `Value` at `+0x34`; `Param==1` → the token is a TeamType reference
at `+0x30`.

**Event order is REVERSED in memory**: each new event is *prepended*
(`ev->Next = FirstEvent; FirstEvent = ev`). The occurred-bit index used by
RegisterEvent/ResetTimers is the position in this reversed list (bit 0 = the LAST
event in the INI line). This matters for which timer event "wins" ResetTimers
(§3.5). Census: `Param` is 0 for every event except kinds 60/61 (`Param=2`, 74
uses) — 4,857 events, 0 malformed.

### 1.3 `[Actions]` — `ID = N, (Kind,ParamKind,P1,P2,P3,P4,P5,Last)×N` (8 tokens each)

`FUN_006dcb20`. `Kind` at `+0x2C`. `ParamKind` selects how `P1` is interpreted:

| ParamKind | P1 is | stored |
|---|---|---|
| 0, 11 | integer | `Value +0x90` |
| 1, 5 | TeamType: if the token is ≤ 2 chars → TeamType **array index**, else TeamType ID (find) | `TeamType +0x30` |
| 2 | TriggerType ID (find-or-create) | `TriggerType +0x50` |
| 3 | TagType ID (find-or-create) | `TagType +0x4C` |
| 4 | text label (leading/trailing ≤0x20 trimmed, 31 chars) — a CSF label | `Text +0x6D` |
| 6 | speech (EVA) name → index | `Value` |
| 7 | sound name → index | `Value` |
| 8 | theme name → index | `Value` |
| 9, 10 | TechnoType ID string (0x18 chars) | `TechnoID +0x54` |

`P2..P5` → `+0x34,+0x38,+0x3C,+0x40` (ints). The **last** token: ParamKind 5 or 9
→ `Value = atoi`; ParamKind 11 → `Value2 +0x48 = atoi`; otherwise **`Waypoint +0x44`
= letter decode** (`FUN_007624b0`): non-alpha → −1; one letter → `L−'A'`; two
letters → `(L1−'A'+1)·26 + (L2−'A')` (so `A`=0, `Z`=25, `AA`=26, `BA`=52).
Actions are **appended** (file order preserved). Census: 13,871 actions, 0
malformed.

### 1.4 `[Tags]` — `ID = Persistence,Name,TriggerID`

`FUN_006e55f0`: `Persistence=atoi` (`+0x9C`: **0 Volatile, 1 SemiPersistant,
2 Persistent** — enum names from YRpp, behaviour from §3.4), `Name` (`+0x64`),
`FirstTrigger` (`+0xA0`, find-or-create). The tag's triggers are
`FirstTrigger` followed by each trigger's `LinkedTrigger` (§1.1 field 2).

### 1.5 `[VariableNames]` — `idx = Name,InitialValue`

Map section → the **100 locals** (`FUN_00689520`; idx ≥ 100 ignored; value
`atoi != 0`; all locals zeroed first). The **50 globals** take names from the
rules `[VariableNames]` (`FUN_00686560`, `rulesmd.ini` has
`0=<Alternate Start Location>` …) and start cleared. Get/set clamp the index
(`<50` / `<100`) and return 0/no-op out of range.

### 1.6 `[CellTags]` — `PackedCell = TagID`

`FUN_004acd80`: key decoded as `x = v % 1000, y = v / 1000` for NewINIFormat ≥ 4
(else the legacy `v & 0x7F`, `v >> 7`) — the same packing as `[Waypoints]`
(maps.md §4.3); the cell's tag = `GetInstance(TagType)` (one shared TagClass per
TagType) and is only set if the cell has no tag yet.

### 1.7 `[TaskForces]` / `[ScriptTypes]` / `[TeamTypes]`

Same index-section + per-ID-section format as `aimd.ini` (ai-team.md §1); the
scenario loader calls the TeamType/ScriptType/TaskForce list loaders twice each
(create pass, read pass) **before** triggers and tags, so a TeamType's `Tag=`
creates TagTypes first (`FUN_006e5880` from the TeamType reader gap at
`0x6F08AE`). The repo's `parse_aimd` reads these sections from any `Ini`.

### 1.8 Load order (CONFIRMED, `FUN_00686560` tail)

TeamTypes(×2) → ScriptTypes(×2) → TaskForces(×2) → **Triggers** (`FUN_00726870`)
→ **Tags** (`FUN_006e5440`) → AITriggerTypes(×2) → … → map objects + `[CellTags]`
(`FUN_004acd80`). Therefore the TagType array order (which fixes the logic-loop
order, §3.2) is: TagTypes first referenced by TeamType `Tag=` (TeamType order),
then by trigger actions with ParamKind 3 (trigger parse order), then `[Tags]`
key order. TriggerType array order likewise = first reference
(`[Triggers]` key order, forward references created early).

---

## §2 Runtime objects (CONFIRMED offsets, YRpp names)

- **TriggerClass** (`0x48`): `+0x24` Type, `+0x28` Next (in its tag), `+0x2C` House
  override (set from an event's "house" result), `+0x30` Destroyed, `+0x34..0x3C`
  Timer `{StartFrame, -, TimeLeft}`, `+0x40` OccurredEvents bitfield, `+0x44`
  Enabled.
- **TagClass** (`0x38`): `+0x24` Type, `+0x28` FirstTrigger, `+0x2C` InstanceCount
  (attached objects), `+0x30` default cell, `+0x34` Destroyed, `+0x35`
  IsExecuting (re-entrancy guard).
- Scenario: Frame counter `DAT_00a8dcec`; MissionTimer at Scenario `+0x11E8`
  `{StartFrame (−1 = stopped), -, TimeLeft}`; Difficulty `+0x60C` (0/1/2);
  "variables changed this frame" flag `+0x34AA`; globals `+0x1C88 + i·0x29`
  (value byte at `+0x28`), locals `+0x248A + i·0x29`.

---

## §3 Engine semantics

### 3.1 Attach flags (`FUN_0071e920` per event, `FUN_006e3450` per action)

| flag | events (kind) | meaning (YRpp name) |
|---|---|---|
| `0x01` Cell | 0 1 4 8 24 25 26 31 53 54 59 | raised by a techno entering a cell (YRpp calls it "Global") |
| `0x02` Object | 0 1 2 4 6 7 8 29 33 34 35 38–44 48 49 | raised on/with an object |
| `0x04` Map | 8 24 | map-wide list |
| `0x08` House | 3 5 8 9 10 11 12 15–22 30 32 52 55–58 | per-house list |
| `0x10` Logic | 8 13 14 23 27 28 36 37 45 46 47 50 51 60 61 | per-frame logic list |

Actions 14 32 60 61 62 91 111 add `0x02`. A tag's flags = OR over its whole
trigger chain (`FUN_006e5760` → `FUN_00726480`). Event 8 (AnyEvent) carries
every flag.

### 3.2 Who raises what, and when (CONFIRMED)

At **scenario start** (`FUN_00684670`), for every TagType in array order:
flags & 4 → `GetInstance` → Map list; flags & 0x10 → `GetInstance` → **Logic
list**; flags & 8 → `GetInstance` → appended to the tag list of the house
`FindByIndex(FirstTrigger.House.ArrayIndex)` (skipped if that house does not
exist). `GetInstance` returns the single TagClass for a TagType (creating it once,
which constructs its TriggerClasses → ResetTimers, §3.5).

**Every frame**, `LogicClass::AI` (`FUN_0055ac40`) walks the Logic list in order
(index 0 → count−1) and, per tag, tries raises **short-circuiting on the first
that fires anything**:

1. `0x32` (PickupCrate_any) — only if the scenario "crate picked" flag is set;
2. `27`, `28`, `36`, `37` — only if the **variables-changed flag** (`+0x34AA`) is set;
3. `45`, `46` — only if the ambient-light-changed flag is set;
4. `13` (ElapsedTime);
5. `51` (RandomDelay);
6. `14` (MissionTimerExpired) — only if the mission timer is running and expired.

After the loop the flags (`+0x34AA`, ambient, crate) are cleared, and an expired
mission timer is stopped (StartFrame=−1, TimeLeft=0). So event 14 is visible for
exactly one frame. A variable set by a tag *earlier* in the same loop is seen by
*later* tags that frame (the flag is read per tag), not by earlier ones.

**Every 8th frame** (`Frame % 8 == 0`), each `HouseClass::AI` raises kind **8**
on its tag list, iterating **from the last tag to the first**.

Objects/cells raise their own kinds (call sites in Techno/Foot/Building code:
1/24/25/26/53/54/59 on cell entry `FUN_004d84f0`; 6/44 attacked; 7/48/29 destroyed;
4/34 discovered/near; 38–43 damage; 49 crate; 33 selected; 23 team-leaves-map;
31 bridges). These need placed, tagged campaign objects.

### 3.3 RegisterEvent (`FUN_00725760`) — the AND rule

`RegisterEvent(kind, object, forceFire, persistent, source)`: returns false if
the trigger is **disabled or destroyed**. Unless `forceFire`, for each event
(bit `b` = position in the reversed list) whose bit is not already latched:
`HasOccurred(kind, triggerHouse, object, &timer, &persistent, source)`; any false ⇒
the trigger does not fire. HasOccurred receives the `persistent` flag **by
reference** and *sets* it for kinds 1/24/25/26/59 (on entry), 19–22 (build
events) and 32/57 (BuildingExists/DoesNotExist) — so a build event latches and a
BuildingExists success re-arms timers even on a non-repeating tag. An occurred (or already latched) event may set the
trigger's house override (`+0x2C`) from the event, and is **latched** (bit set)
only when `persistent && StateA(ev) && StateB(ev)`, where StateA = "event kind is
an object/raised kind" (`FUN_0071ebf0`: 1 2 3 4 6 7 18–26 29 31 33–35 38–44 48–50
53 54 59) and StateB = not in {1, 6, 31, 44, 59} (`FUN_0071ec60`). When all events
hold and `persistent`, **ResetTimers** runs (so a repeating timer trigger
re-arms). `triggerHouse` = `FindByIndex(TriggerType.House.ArrayIndex)`.

### 3.4 RaiseEvent (`FUN_006e4910`) — persistence

Refuses if the game-over flag is set, the tag is executing (re-entrancy), the
tag is destroyed, or it has no type. For each TriggerClass in the tag (instance
list = the type chain **reversed**, because the ctor prepends):
`ok = RegisterEvent(kind, obj, forceFire, persistent = (Persistence == 2), src)`;
if ok:

| Persistence | effect |
|---|---|
| 0 Volatile | FireActions; **Destroy the trigger**; mark the tag for destruction |
| 1 SemiPersistant | only if `InstanceCount == 1`: FireActions + Destroy + tag destroyed; otherwise just detach from this object |
| 2 Persistent | FireActions (trigger stays) |

Return = any trigger fired. A destroyed tag is removed from every list.

### 3.5 Timers — ResetTimers (`FUN_007256a0`)

For each event in (reversed) list order: kind 13 → `Timer = {Frame, Value·15}`
and clear its bit; kind 51 → `Timer = {Frame, (Value/2 + Random(0,Value))·15}`
and clear its bit (`FUN_0065c220` lockstep RNG, inclusive range). One timer per
trigger, so the **last** timer event in memory order (= the FIRST in the INI
line) wins. Called by: the TriggerClass ctor (scenario start), `Enable`
(action 53, even if already enabled), a fired Persistent trigger (§3.3), and the
variable-change notification (§3.6).

### 3.6 Variables — set and notify (`FUN_00689070` / `FUN_00689310`)

`set(i, v)`: if the stored value differs → store it, set the variables-changed
flag (`+0x34AA`), and for **every tag → every trigger** whose type has a
GlobalSet/Cleared (27/28) [resp. LocalSet/Cleared (36/37)] event on index `i`,
**ResetTimers**. Consequence (very common in campaign data, e.g. sov07tmd
`0D06808C = 37 LocalCleared 5 + 13 ElapsedTime 205`): every change of the local
restarts that trigger's countdown.

### 3.7 Difficulty

TriggerClass ctor: `Enabled = Type.Enabled && Type.Difficulty[Scenario.Difficulty]`
(0 Easy, 1 Normal, 2 Hard). Action 53 Enable only enables instances whose type
flag for the current difficulty is set. [ai-base.md records a difficulty-index
inversion for *AI houses*; the scenario index here is read as the plain
0/1/2 slot. The gate runs **Normal (1)**, which is invariant under that
inversion.]

---

## §4 Event semantics (`FUN_0071dbe0`) + campaign census

Census: **44 unique campaign maps** (RA2 `Maps01.mix` 18 + `MAPS02.MIX` 18 + YR
`mapsmd03.mix` 14 entries, de-duplicated by content — `all01t`/`sov01t`/`e31`/
`e32` ship in both RA2 mixes; one unnamed `MAPS02` entry `439FEA47` is a map and
is counted), **4,341 triggers, 4,857 events, 13,871 actions**, 0 malformed lines.
`uses` = event count, `maps` = maps using it.

Common preamble: kind 0 → false. For the object kinds {1 2 3 4 6 7 18–26 29 31
33–35 38–44 48–50 53 54 59} the raised kind must equal the event kind and the
game-over flag must be clear. Then the per-house switch (receiver = the
*trigger's* house; skipped entirely when that house does not exist), then the
**value-house** switch (receiver = `FindByIndex(Value)`; **if no such house
exists the event is TRUE**).

| kind | name | uses | maps | predicate (CONFIRMED) |
|---:|---|---:|---:|---|
| 13 | ElapsedTime | 2022 | 43 | trigger timer expired: `Frame − Start ≥ TimeLeft` (TimeLeft = Value·15), or TimeLeft==0 |
| 36 | LocalSet | 551 | 37 | `local[Value] != 0` (not edge-gated inside HasOccurred) |
| 47 | ElapsedScenarioTime | 342 | 32 | `Value ≤ Frame / 15` |
| 37 | LocalCleared | 298 | 20 | `local[Value] == 0` |
| 1 | EnteredBy | 295 | 38 | object kind; Value≠−1 ⇒ entering object's owner house type == Value |
| 48 | DestroyedByAnything | 293 | 39 | object kind (raised on death) |
| 32 | BuildingExists | 121 | 29 | trigger house owns ≥1 BuildingType[Value] (`OwnedBuildingTypes` counter) |
| 57 | BuildingDoesNotExist | 116 | 21 | trigger house owns 0 of BuildingType[Value] |
| 4 | DiscoveredByPlayer | 114 | 29 | object kind |
| 11 | DestroyedAll | 105 | 35 | house[Value]: buildings ≤ 0 and owned vehicle-types total ≤ 0 and infantry total ≤ 0 (aircraft not counted) |
| 8 | AnyEvent | 103 | 12 | **always true** when evaluated (no case) — the kind HouseClass::AI raises |
| 61 | TechTypeDoesntExist | 67 | 9 | no techno anywhere whose type ID == String |
| 51 | RandomDelay | 59 | 10 | trigger timer expired (TimeLeft from §3.5) |
| 19 | BuildBuildingType | 37 | 17 | trigger house's last-built BuildingType == Value |
| 6 | AttackedByAnybody | 35 | 15 | object kind |
| 44 | AttackedByHouse | 32 | 10 | object kind; attacker's house type == Value |
| 10 | DestroyedBuildingsAll | 29 | 17 | house[Value] buildings ≤ 0 |
| 33 | SelectedByPlayer | 28 | 14 | object kind |
| 29 | DestroyedFakesAll | 17 | 8 | object kind |
| 9 | DestroyedUnitsAll | 16 | 10 | house[Value] vehicles total ≤ 0 and infantry total ≤ 0 |
| 17 | NoFactoriesLeft | 15 | 8 | trigger house has buildings and none of its live buildings is a factory |
| 31 | AllBridgesDestroyed | 14 | 7 | cell/object kind |
| 7 | DestroyedByAnybody | 14 | 12 | object kind |
| 20 / 21 | BuildUnitType / BuildInfantryType | 13 / 12 | 9 / 9 | last-built type == Value |
| 30 | LowPower | 12 | 8 | house[Value] power ratio < 1.0 |
| 59 | EnteredOrOverflownBy | 11 | 4 | as EnteredBy |
| 16 / 15 | DestroyedUnitsNum / DestroyedBuildingsNum | 11 / 9 | 7 / 7 | trigger house's destroyed-count (`+0x5434` / `+0x5488`) ≥ Value |
| 26 / 25 | CrossesVertical / HorizontalLine | 9 / 4 | 5 / 4 | cell kind |
| 14 | MissionTimerExpired | 8 | 7 | mission timer running and expired |
| 58 | PowerFull | 7 | 5 | house[Value] power ratio ≥ 1.0 |
| 60 | TechTypeExists | 7 | 5 | ≥ Value technos anywhere whose type ID == String |
| 27 / 28 | GlobalSet / GlobalCleared | 4 / 4 | 1 / 3 | `global[Value]` != 0 / == 0 |
| 41, 55, 56 | half-health / naval / land destroyed | 4 each | — | object / house counters |
| 52 | CreditsBelow | 3 | 3 | trigger house credits ≤ Value |
| 40, 39, 43, 22, 2, 5 | damage / aircraft built / spied / house discovered | ≤3 | — | object / house kinds |
| 12 | CreditsExceed | 0 | 0 | trigger house credits ≥ Value |
| 45 / 46 | AmbientLightBelow/Above | 0 | 0 | scenario ambient ≤/≥ Value |

The remaining kinds (3, 18, 23, 24, 34, 35, 38, 42, 49, 50, 53, 54) are unused in
the stock campaigns.

---

## §5 Action semantics (`FUN_006dce20`) + census

Execute is called as `Execute(triggerHouse, object, trigger, location)` for every
action in file order; **the return value is ignored by FireActions** (all actions
run). `FindHouseByIndex(trigger, v)`: `v == 8997` → the trigger's house override;
`v == −1` → none; else the house of HouseType `v`.

| kind | name | uses | maps | semantics (CONFIRMED unless noted) |
|---:|---|---:|---:|---|
| 53 | EnableTrigger | 2706 | 43 | every TriggerClass of TriggerType `P1` whose type flag for the current difficulty is set → Enabled=1 **and ResetTimers** |
| 54 | DisableTrigger | 1836 | 41 | every TriggerClass of the type → Enabled=0 |
| 99 | PlaySoundAtWaypoint | 1289 | 35 | sound `Value` at waypoint `Last` (presentation) |
| 4 | CreateTeam | 1208 | 43 | `TeamType.CreateOne(house=TeamType.House)`; the trigger path bypasses the TeamType `Max` cap (a re-entrancy counter is raised around the call) |
| 7 | Reinforcement | 941 | 33 | TeamType reinforcement (delivery per the TeamType) |
| 17 | RevealAroundWaypoint | 615 | 41 | **P1 is the waypoint index**; reveal radius = rules `RevealTriggerRadius` around it, for the **player** house; no-op if the player already has the whole map revealed |
| 21 | PlaySpeech | 570 | 41 | EVA speech `Value` (presentation) |
| 11 | TextTrigger | 451 | 40 | display CSF label `Text` (presentation) |
| 80 | ReinforcementAt | 425 | 32 | TeamType reinforcement at waypoint `Last`; refuses with no team or waypoint −1 |
| 5 | DestroyTeam | 403 | 23 | destroy every live team of TeamType `P1` |
| 55 | CreateRadarEvent | 372 | 40 | radar ping at waypoint (presentation) |
| 56 / 57 | LocalSet / LocalClear | 354 / 171 | 30 / 18 | `set_local(Value, 1/0)` (§3.6) |
| 19 | PlaySoundEffect | 310 | 31 | sound (presentation) |
| 14 | ChangeHouse | 186 | 28 | attached object(s) change owner (object action) |
| 108 | CreateCrate | 130 | 27 | crate of type `Value` at waypoint cell |
| 63 | Apply100Damage | 100 | 4 | 100 damage at waypoint |
| 38 / 37 | MakeEnemy / MakeAlly | 93 / 21 | 30 / 8 | trigger house ↔ house `Value` alliance change |
| 12 | DestroyTrigger | 84 | 14 | every TriggerClass of TriggerType `P1` → Destroy |
| 112 | CenterCameraAtWaypoint | 84 | 18 | camera (presentation) |
| 107 | ReinforcementByChrono | 84 | 5 | TeamType chrono-in at waypoint |
| 3 | ProductionBegins | 81 | 36 | house `Value`: production enabled (`+0x1EE` = 1) |
| 100 | PlayIngameMovie | 79 | 36 | presentation |
| 48 | MoveCameraToWaypoint | 76 | 31 | presentation |
| 74 / 75 | AITriggersBegin / Stop | 76 / 3 | 33 / 2 | house `Value`: AI-trigger flag |
| 41 | PlayAnimAt | 68 | 13 | presentation |
| 6 | AllToHunt | 66 | 24 | house `Value`: every unit to Hunt |
| 36 | AllChangeHouse | 64 | 16 | every object of the trigger house → house `Value` |
| 113 | MakeHouseCheer | 58 | 34 | presentation |
| 2 / 1 | Lose / Win | 55 / 43 | 39 / 41 | **Win**: `Value` == player's HouseType index ⇒ player wins, else player loses. **Lose**: `Value` != player index ⇒ player wins, else loses |
| 46 / 47 | LockInput / UnlockInput | 51 / 46 | 32 / 33 | input lock (presentation) |
| 32 | DestroyAttachedObject | 51 | 17 | object action |
| 116, 104, 114, 115, 131 | movie+pause, flash team, sidebar tab, flash cameo, flash buildings | 49/45/37/34/30 | — | presentation |
| 125 | CreateBuilding | 40 | 5 | building `TechnoID` for the trigger house at waypoint `Last` |
| 101 | ReshroudMapAtWaypoint | 38 | 6 | reshroud around waypoint |
| 9 | FireSale | 29 | 16 | house `Value` sells everything |
| 81 | WakeupSelf | 25 | 1 | object action |
| 23 / 24 / 25 / 26 / 27 | Timer Start / Stop / Extend / Shorten / Set | 18 / 6 / 2 / 0 / 19 | — | mission timer: Start: if stopped, StartFrame = Frame. Stop: if running, TimeLeft = remaining, StartFrame = −1. Extend: timer = {Frame, remaining + Value·15}. Shorten: remaining ≤ Value·15 ⇒ {Frame, 0} else {Frame, remaining − Value·15}. Set: {Frame, Value·15} |
| 103 | TimerText | 19 | 10 | presentation |
| 28 / 29 | GlobalSet / GlobalClear | 13 / 0 | 6 / 0 | `set_global(Value, 1/0)` |
| 22 | ForceTrigger | 3 | 2 | FireActions on every instance of TriggerType `P1` (still requires that instance Enabled and not Destroyed; no persistence handling) |
| 0 | None | 12 | 4 | no-op |
| 16 | RevealAllMap | 11 | 9 | reveal the whole map for the player |
| others | 60 88 135 121 128 130 40 111 119 10 122 132 106 70 18 51 42 129 20 90 117 118 137 75 35 68 127 139 136 25 102 120 44 61 78 77 89 | ≤ 18 each | — | listed with counts in the spec's table |

---

## §6 House-type index space (CONFIRMED by data + loader shape)

Event/action house values and the trigger `House` field index **HouseTypes**:
the rules `[Countries]` list first (`rulesmd.ini`: `0=Americans … 13=Special`),
then each map `[Countries]` entry **not already present** appended
(find-or-allocate by name). Decisive check on the gate map `sov07tmd.map`: its
`[Countries]` lists `Einstein, Player, BadGuy1 …`; the trigger `_Lose`
(`05FD863C=Player,<none>,_Lose,…`) has event `11 DestroyedAll` with **Value 15**
— Einstein=14, **Player=15** under the append model (under an override-by-key
model 15 would not exist). Trigger `House` fields also name rules countries that
have no `[X House]` instance (sov07tmd: 15 triggers owned by `Americans`): those
triggers have **no house** (FindByIndex → null; §4 null-house rules apply; they
never join a house tag list). House *instances* come only from the map `[Houses]`
list (`… House` sections, `Country=`).

---

**BuildingType index space** (event 19/32/57 values) is the rules
`[BuildingTypes]` list (map additions appended): on sov07tmd the trigger
`Soviets Build Nuke (On-nuke silo)` waits on `19 BuildBuildingType 54` and its
chained sibling `(On-Iron Curtain)` on `19 … 24` — rulesmd `[BuildingTypes]`
index 54 = `NAMISL` (nuclear silo) and 24 = `NAIRON` (Iron Curtain). CONFIRMED.

## §7 Deviations / fall-backs we take

1. **RNG** — RandomDelay draws from the original lockstep RNG; we draw the same
   inclusive range `[0, Value]` from the trigger system's own seeded generator,
   in the same call order (tag creation order, then re-arm order). The value
   sequence differs from the original's. [RE-FALLBACK: RNG stream]
2. **Presentation actions** (speech, sound, text, movies, camera, radar, input
   lock, flashing, tabs, cheer, timer text) have no simulation state; they are
   *recorded* in the trigger log, not rendered/played.
3. **Object/cell events and object actions** need placed, tagged campaign
   objects (`[Units]/[Infantry]/[Structures]/[Aircraft]` tag fields,
   `[CellTags]`), which the engine does not place yet (maps.md §4.7). The engine
   exposes `raise_cell`/`raise_object` entry points with the §4 semantics; wiring
   them to world objects is P10.
4. **Teams / reinforcements** (4, 5, 7, 80, 107) need the map's own TeamTypes to
   run through a TeamClass with mission scripts; the trigger engine resolves the
   TeamType and hands a request to the host. The gate host places the
   TaskForce's members at the team's waypoint (no transport delivery, no script)
   when their types are in the loaded rosters, and records the rest as skipped.
   [RE-FALLBACK: TeamClass execution for map teams]
5. **InstanceCount** for SemiPersistant tags is only meaningful with attached
   objects (item 3).
6. The game-over flag (`DAT_00a8dcd3`) that short-circuits RaiseEvent and the
   object kinds is not modelled (the demo keeps simulating after an outcome).
   [RE-FALLBACK: game-over gating]

## §8 Confidence summary

| claim | evidence | confidence |
|---|---|---|
| `[Triggers]/[Events]/[Actions]/[Tags]/[CellTags]/[VariableNames]` field order + parse rules | ReadINI decompiles + 44 maps, 0 malformed | **CONFIRMED** |
| Event list reversed in memory; actions in order | prepend/append in `FUN_007264e0` | **CONFIRMED** |
| Waypoint letter decode | `FUN_007624b0` | **CONFIRMED** |
| Logic loop order + short-circuit + flag-gated raises; house loop every 8 frames, reversed | `FUN_0055ac40`, `FUN_004f8350` | **CONFIRMED** |
| AND rule, latching only for Persistent with StateA&&StateB; ResetTimers on persistent fire | `FUN_00725760`, `FUN_0071ebf0`, `FUN_0071ec60` | **CONFIRMED** |
| Persistence 0/1/2 behaviour | `FUN_006e4910` | **CONFIRMED** |
| ElapsedTime = Value·15 frames from (re)arm; RandomDelay = (Value/2 + rand[0,Value])·15; ElapsedScenarioTime = Value ≤ Frame/15 | `FUN_007256a0`, `FUN_0071dbe0` | **CONFIRMED** |
| Variable change ⇒ flag + ResetTimers of referencing triggers | `FUN_00689070/310` → `FUN_00725640/670` | **CONFIRMED** |
| Win/Lose player-relative rule | Execute cases 1/2 + `FUN_004fc800` sets the winner byte | **CONFIRMED** (which of `4fc800/4fc9f0` is win: PROBABLE-high) |
| Counter receivers for 9/10/11 (`+0x5550` buildings, `+0x5564` vehicles, `+0x5578` infantry) | disassembly `LEA ECX,[house+off]` + YRpp layout names | **PROBABLE** (names) |
| HouseType index space = rules countries then map countries appended | `_Lose` Value 15 on sov07tmd | **CONFIRMED** |
| Team/reinforcement delivery, object/cell event wiring | — | **[RE-FALLBACK]** (§7) |
