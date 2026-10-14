# Enemy AI weighted trigger engine (AITriggerTypes) — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `aitrig1.py` … `aitrig5.py`;
decompiles/listings under the scratchpad `out_trig/` — nothing under `D:\ra2-re`
or the scratchpad is committed. **Content-anchored only.** Every function below
was located by anchoring on the literal INI strings the binary loads —
`"AITriggerTypes"` / `"AITriggerTypesEnable"` (section names, at `0x8170F8` /
`0x8170E0`), the rulesmd `[General]` keys `"AITriggerSuccessWeightDelta"` /
`"AITriggerFailureWeightDelta"` / `"AITriggerTrackRecordCoefficient"`, and the
real `aimd.ini [AITriggerTypes]` data — then relocating each YRpp hint address by
**content** (drift ~+0x20 in this cluster) and xref-walking to the consumers. **No
YRpp/Ares address was used as evidence** — YRpp `AITriggerTypeClass.h` was read
only for the class layout / method names / the `FormatForSaving` comma format
string, and every published address drifted (`ConditionMet 0x41E720`→`0x41E740`,
`RegisterSuccess 0x41FD60`→`0x41FD80`, `Array 0xA8B200`→`0xA8A16C`; see "YRpp
drift"). Numeric constants are read from the real `rulesmd.ini`/`aimd.ini`
(from `ra2md.mix → localmd.mix/*`).)

Companion of `ai-team.md` (the AI-2 offensive-teams hunt — the TeamType/TaskForce/
ScriptType/TeamClass structures and the `FUN_004fd320` House-AI tick chain this
hunt hangs off) and `ai-base.md` (base-planning tick + the difficulty-index
inversion caveat). **This doc is the calibration authority for AI-3 spec §1
items 1–6 and gates Tasks 2–6.** See "DIVERGENCES FROM THE PLAN'S PRE-HUNT
ASSUMPTIONS" for the STOP items — the **18-field layout is confirmed but the
comparator packing is `{value, operator}` (value-first, 4×8-byte `Conditions[4]`,
only `[0]` used)**, the **weight rule is additive +20 / −50 (not ±3.0)**, **BOTH
Team1 and Team2 are created for the owner** (not team1-only / team2-reciprocal),
**`ct=4` is HouseCredits (not "enemy credits")**, **all 163 stock TeamTypes are
`Autocreate=yes` so the trigger pool == the autocreate pool**, and the
**per-difficulty enable consult is inverted** (house diff index 0 → the Hard flag).

Key globals: `DAT_00886160` = `RulesClass::Instance`. `DAT_00a8a16c` =
**AITriggerType array items base**, `DAT_00a8a178` = **count** (YRpp `0xA8B200`
drifted). `DAT_008b3054`/`DAT_008b3060` = TeamClass array base/count.
`DAT_00a7f194`/`DAT_00a7f1a0` = HouseClass array base/count. `DAT_00a8a1a0` =
game mode (0 = campaign; non-zero = skirmish/MP). `DAT_00a8dcec` = global frame
counter. `FUN_0065c220` = lockstep AI RNG `rand(lo,hi)`. `FUN_007c4d00` =
`ROUND(ST0)` FPU round-to-int (NOT an RNG — `ai-team.md` mislabelled it; here it
rounds `Weight_Current`→int for the draw). The `AITriggerTypeClass` is an
`AbstractTypeClass` subclass; **instance size = `0x110`** (from `operator_new(0x110)`
in the loader + ctor).

## YRpp drift (this build)

Every published AITrigger address is off by ~+0x20 or lands in a Ghidra gap here:
`ConditionMet 0x41E720`→**`0x41E740`**; `EnemyHouseOwns 0x41EAF0`→**`0x41EB10`**;
`CivilianHouseOwns 0x41EC90`→**`0x41ECB0`**; `OwnerHouseOwns 0x41EE90`→**`0x41EEB0`**;
`IronCurtainCharged 0x41F0D0`→**`0x41F0F0`**; `ChronoSphereCharged 0x41F180`→**`0x41F1A0`**;
`HouseCredits 0x41F230`→**`0x41F250`**; `LoadFromINIList 0x41F2E0`→**`0x41F300`**;
`SaveToINIList 0x41F490`→**`0x41F4B0`**; `RegisterSuccess 0x41FD60`→**`0x41FD80`**;
`RegisterFailure 0x41FE20`→**`0x41FE40`**; ctor `0x41E350`→**`0x41E370`**; array
`0xA8B200`→**`0xA8A16C`**. Each slave was finally ID'd by **content** (which house
it counts, which comparator it applies), not by the drifted hint — the drift is
inconsistent enough that two hints landed in the same wrong function. The YRpp
**layout** (field order/types + the `FormatForSaving` format string) matches the
binary exactly; only addresses drifted.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_0041e370` | **AITriggerTypeClass ctor** — `new(0x110)`; sets defaults (ConditionType −1, weights 1.0/1.0/1.0, all 3 enables 1); registers into `DAT_00a8a16c`/`DAT_00a8a178` | array-global writes + YRpp `0x41E350` hint |
| `FUN_0041e740` | **ConditionMet** — the eligibility gate + condition dispatch (`switch(ConditionType+1)`); enforces every filter + the Team1/Team2 buildable & `Max` caps (§2/§5/§6) | anchor: sole caller of every condition slave; YRpp `0x41E720` |
| `FUN_0041eb10` | **EnemyHouseOwns** (ct=0) — count target-house-owned of `ConditionObject`, apply comparator | dispatch case 1; content |
| `FUN_0041eeb0` | **OwnerHouseOwns** (ct=1) — count calling-house-owned of `ConditionObject` | dispatch case 2; content |
| `FUN_0041f250` | **HouseCredits** (ct=4) — read a house money scalar (`vtbl+0x18`), apply comparator | dispatch case 5; content |
| `FUN_0041f0f0` | **IronCurtainCharged** (ct=5) — scan house superweapon list for SW type `1`, charge-ratio ≥ `1.0 − Rules+0xd70` | dispatch case 6; content |
| `FUN_0041f1a0` | **ChronoSphereCharged** (ct=6) — same, SW type `3` | dispatch case 7; content |
| `FUN_0041ecb0` | **CivilianHouseOwns** (ct=7) — find neutral house (`FUN_006a3d60` side), count its `ConditionObject`, apply comparator | dispatch case 8; content |
| `FUN_0041f300` | **LoadFromINIList** — iterate `[AITriggerTypes]` section, find/`new` each id, `ReadFromINI` (vtbl+0x64); then apply the `[AITriggerTypesEnable]` per-id `IsEnabled(+0xa4)` list | anchor: refs both section strings + array globals |
| `FUN_0041f4b0` | **SaveToINIList / FormatForSaving** — writes both sections (the 18-comma format) | anchor: refs `"AITriggerTypes"`/`"...Enable"` |
| `FUN_0041fd80` | **RegisterSuccess** — `Weight += AITriggerSuccessWeightDelta + trackbonus`; clamp [Min,Max]; `++Completed(+0x104), ++Executed(+0x108)` | disband caller (success branch); YRpp `0x41FD60` |
| `FUN_0041fe40` | **RegisterFailure** — `Weight += AITriggerFailureWeightDelta + trackpenalty`; clamp; `++Executed(+0x108)` only | disband caller (fail branch); YRpp `0x41FE20` |
| `FUN_0041ff00` | **team-buildable predicate** — can this house field the TeamType (called twice by ConditionMet for Team1/Team2) | ConditionMet callee |
| `FUN_006f0010` | **the trigger DRIVER** — per-house team-management + weighted trigger selection: counts teams, disbands lowest-priority at `TotalAITeamCap`, iterates the trigger array calling ConditionMet, cumulative-weighted draw via `FUN_0065c220(1,ΣweightRound)`, queues **Team1+Team2**, marks them `Autocreate=1` | sole caller of ConditionMet + array-global refs |
| `FUN_006e8350` | **TeamClass destructor / disband** — the **attribution site**: scans the whole trigger array for `Team1(+0xdc)==team.TeamType(+0x24)` and calls RegisterSuccess/Failure keyed on the team's `+0x84` success flag | sole caller of Register* |
| `FUN_006e8000` | TeamClass ctor — initialises `team+0x84` success flag = 0 | `+0x84` writer scan |
| `FUN_0049f9f0` | HouseClass owned-count-of-type accessor (the `*Owns` conditions call it) | condition-slave callee |
| `FUN_0066cf70` | `[General]` loader — `AITriggerSuccessWeightDelta→Rules+0xc0`, `FailureWeightDelta→+0xc8`, `TrackRecordCoefficient→+0xd0`; `TotalAITeamCap→Rules+0x13c8` (per-difficulty array read at `+0x13cc`) | anchor: those key strings |
| `FUN_0065c220` | lockstep AI RNG `rand(lo,hi)` — the weighted draw + the cadence gate | ai-base/ai-team |
| `FUN_007c4d00` | `ROUND(ST0)`→longlong FPU intrinsic (rounds `Weight_Current`→int for the draw) | driver callee; content |
| `FUN_004f53b0` | House init — seeds the per-house trigger-pass probability `house+0x565c = 100` | `+0x565c` writer scan |

---

## §1 `[AITriggerTypes]` section format + field layout

### File format (CONFIRMED from real `aimd.ini` + `FormatForSaving`)

`[AITriggerTypes]` is a **flat positional comma-list section** (unlike TeamTypes'
named keys): each line is `<id>=<18 comma fields>`. There is **no index section**
and **no `<GUID>` entity section** — the section *is* the list (165 stock lines).
The comma order is fixed by the WriteINI format string (`AITriggerTypeClass::
FormatForSaving`, verbatim from YRpp, matched 1:1 by the data):

```
"%s = %s,%s,%s,%d,%d,%s,%s,%lf,%lf,%lf,%u,%d,%d,%u,%s,%u,%u,%u\n"
   id  name t1 own tl ct obj cmp  wc   wmin wmax skm 0 side bdef t2  eE eN eH
```

**The 18 fields after `=` (CONFIRMED — data + format string + struct offsets):**

| # | field | struct off | notes |
|---|---|---|---|
| 1 | **Name** | +0x??(name hdr) | display string (recorded-only) |
| 2 | **Team1** id | +0xdc (TeamType*) | **load-bearing** — the team created; the attribution key |
| 3 | **OwnerHouse** | +0xa0 type / +0xa8 idx | `<all>`→OwnerHouseType=2 (Any); country name→type=1 + HouseIndex; `<none>`→0 |
| 4 | **TechLevel** | +0xb0 (int) | house tech level must be **≥** this (1..10 in stock) |
| 5 | **ConditionType** | +0x98 (int) | the enum (§2); −1..7 valid, default −1 |
| 6 | **ConditionObject** | +0xd8 (TechnoType*) | the type the `*Owns` conditions count; `<none>` when N/A |
| 7 | **Conditions** blob | +0xe4 (32 bytes) | `AITriggerConditionComparator[4]`; **only `[0]` used** (§2) |
| 8 | **Weight_Current (start)** | +0xb8 (double) | the adaptive weight; initialised to this (§4) |
| 9 | **Weight_Minimum** | +0xc0 (double) | clamp floor (§4) |
| 10 | **Weight_Maximum** | +0xc8 (double) | clamp ceiling (§4) |
| 11 | **IsForSkirmish** | +0xd0 (bool) | skirmish/MP gate — must be 1 in skirmish (§5); =1 for ALL stock |
| 12 | **(unused 0)** | — | literal `0` written; not stored |
| 13 | **SideIndex** | +0xac (int) | 0=any, **1=Allied, 2=Soviet, 3=Yuri** (§5) |
| 14 | **IsForBaseDefense** | +0xd1 (bool) | defensive-team trigger (§5 split); =0 for ALL stock |
| 15 | **Team2** id | +0xe0 (TeamType*) | **also created** alongside Team1 (§3); `<none>` on 116/165 |
| 16 | **Enabled_Easy** | +0xd2 (bool) | per-difficulty enable (§5); default 1 |
| 17 | **Enabled_Normal** | +0xd3 (bool) | (§5) |
| 18 | **Enabled_Hard** | +0xd4 (bool) | (§5) |

**Load-bearing at runtime:** Team1 (+0xdc), Team2 (+0xe0), OwnerHouseType/Index
(+0xa0/+0xa8), TechLevel (+0xb0), ConditionType (+0x98), ConditionObject (+0xd8),
Conditions[0] (+0xe4/+0xe8), Weight_Current/Min/Max (+0xb8/+0xc0/+0xc8),
IsForSkirmish (+0xd0), SideIndex (+0xac), IsForBaseDefense (+0xd1), the 3 enables
(+0xd2/+0xd3/+0xd4), and (runtime, not INI) `IsEnabled` (+0xa4), TimesCompleted
(+0x104), TimesExecuted (+0x108). **Recorded-only:** Name; the field-12 `0`.

### AITriggerTypeClass struct offsets (CONFIRMED — ctor `FUN_0041e370` + ConditionMet + slaves + Register*)

`this` = AITriggerTypeClass*, size **0x110**. All bool = 1 byte, int = 4, double = 8.

| off | field (default) | off | field (default) |
|---|---|---|---|
| +0x98 | ConditionType int (**−1**) | +0xd1 | IsForBaseDefense bool (0) |
| +0x9c | IsGlobal int (0) | +0xd2 | Enabled_Easy bool (**1**) |
| +0xa0 | OwnerHouseType int (0) | +0xd3 | Enabled_Normal bool (**1**) |
| +0xa4 | IsEnabled bool (0) | +0xd4 | Enabled_Hard bool (**1**) |
| +0xa8 | HouseIndex int (**−1**) | +0xd8 | ConditionObject TechnoType* (0) |
| +0xac | SideIndex int (0) | +0xdc | **Team1** TeamType* (0) |
| +0xb0 | TechLevel int (0) | +0xe0 | **Team2** TeamType* (0) |
| +0xb4 | unknown_B4 int | +0xe4 | Conditions[0].Operand int (value/count) |
| +0xb8 | **Weight_Current** dbl (**1.0**) | +0xe8 | Conditions[0].Type uint (operator 0–5) |
| +0xc0 | **Weight_Minimum** dbl (**1.0**) | +0xec..+0x103 | Conditions[1..3] (24 B, unused/0) |
| +0xc8 | **Weight_Maximum** dbl (**1.0**) | +0x104 | **TimesCompleted** int (successes; 0) |
| +0xd0 | IsForSkirmish bool (0) | +0x108 | **TimesExecuted** int (total; 0) |

> Note on the counters: YRpp labels `+0x104=TimesExecuted, +0x108=TimesCompleted`,
> but the **increment behaviour is the reverse** — RegisterSuccess increments *both*
> +0x104 and +0x108; RegisterFailure increments *only* +0x108. So functionally
> **+0x104 = success count, +0x108 = total-attempt count**, and the ratio
> `+0x104/+0x108` is the success rate used by the weight formula (§4).

**Confidence: file format (flat positional 18-field list), field order, and every
struct offset CONFIRMED** (triangulated: real `aimd.ini` data + the `FormatForSaving`
format string + the ctor's field-by-field init + the reads in ConditionMet /
HouseCredits / RegisterSuccess).

---

## §2 The FULL ConditionType enum + comparator encoding

### The comparator blob (CONFIRMED — `FUN_0041f250` etc. + ctor + data)

Field 7 is `AITriggerConditionComparator Conditions[4]` = 4×8 bytes = 32 bytes,
serialised as 64 lowercase hex chars (LE). Each comparator is
`{int ComparatorOperand; uint ComparatorType}`:

- **dword0 = Conditions[0].ComparatorOperand @ +0xe4** = the threshold **count/value N**;
- **dword1 = Conditions[0].ComparatorType @ +0xe8** = the **operator**;
- **dwords 2–7 (Conditions[1..3]) = always `0` in all 165 stock lines** (reserved;
  the YRpp comment on the `[4]` array is literally *"// don't ask"*).

So the packing is **value-first, operator-second**, and the on-disk 32-byte width
is `4 ×` the 8-byte comparator with only slot 0 meaningful. The operator enum
(CONFIRMED — identical `switch` in every `*Owns`/HouseCredits slave, matches YRpp
`AITriggerConditionComparatorType`):

| op | meaning | test applied | stock use |
|---|---|---|---|
| 0 | Less | `count < N` | rare |
| 1 | LessOrEqual | `count <= N` | e.g. "Soviet Miners" (own HARV ≤ 3 → build more) |
| 2 | Equal | `count == N` | rare |
| **3** | **GreaterOrEqual** | `N <= count` | **148/165** (the default "owns ≥ N") |
| 4 | Greater | `N < count` | rare |
| 5 | NotEqual | `count != N` | (unused in stock) |

Applied as `actualCount  OP  N` (the slave computes `actualCount`, then the switch
compares against `+0xe4`). Stock counts (dword0) range 0–8; stock operators (dword1)
∈ {0,1,2,3,4}.

### The full ConditionType dispatch (CONFIRMED — `FUN_0041e740` `switch(ConditionType+1)`)

`ConditionMet(this, CallingHouse, TargetHouse, arg3)` dispatches
on the int at `this+0x98` (ConditionType) plus 1, cases `0..8` (i.e. ConditionType `−1..7`),
`default`→reject. When `TargetHouse==0`, only ct=−1 (always) and ct=1
(OwnerHouseOwns) are evaluated. **Every value the binary handles:**

| ct | name / slave | what it tests | class |
|---|---|---|---|
| **−1** | (none) | always true (no condition) | **implementable** (trivially true) |
| **0** | EnemyHouseOwns `FUN_0041eb10` | `TargetHouse` (enemy) owns `[op] N` of `ConditionObject` | **implementable** (World has per-house type counts) |
| **1** | OwnerHouseOwns `FUN_0041eeb0` | `CallingHouse` (owner) owns `[op] N` of `ConditionObject` | **implementable** |
| **2** | (inline) | `(TargetHouse[+0x24] metric − arg3) < 100.0` (`_DAT_007e1ac0`) | **inert / unused** — 0 stock triggers; obscure metric |
| **3** | (inline) | `(TargetHouse[+0x24] metric − arg3) < 0.0` (`_DAT_007e1800`) | **inert / unused** — 0 stock triggers |
| **4** | HouseCredits `FUN_0041f250` | a house money scalar `[op] N` (`vtbl+0x18` on house+0x24) | **implementable-limited** — ALL 8 stock uses are `N=0, op=≥` (⇒ **always true**); treat as always-true unless credits visible |
| **5** | IronCurtainCharged `FUN_0041f0f0` | owner's Iron-Curtain SW (type id `1`) charge ≥ `1.0 − Rules+0xd70` | **inert-required-subsystem-absent** (superweapons) |
| **6** | ChronoSphereCharged `FUN_0041f1a0` | owner's Chronosphere SW (type id `3`) charge ready | **inert-required-subsystem-absent** (superweapons) |
| **7** | CivilianHouseOwns `FUN_0041ecb0` | the neutral/civilian house (side `FUN_006a3d60`) owns `[op] N` of `ConditionObject` | **implementable-if-neutral-house** — needs a civilian house + capturable buildings; else inert |
| **≥8** | — | `default` → reject (never eligible) | n/a |

`*Owns` count via a `WhatAmI` switch (`3`/`7`/`0x10`/`0x28` = the techno kinds) →
`FUN_0049f9f0(house-owned-count-array, typeIndex)`. Stock-used ct values are
**{0,1,4,5,6,7}** (never 2, 3, or −1). Stock semantics per data: ct=0 sends
attack/engineer teams when the *enemy* fields a key building/unit (GACNST/NACNST/
MTNK/HTNK/ORCA/ZEP…); ct=1 gates on the *owner's* own tech (NATECH→Apoc) or a
shortfall (HARV ≤ 3 → miners); ct=7 sends engineers to capture neutral tech
(CAAIRP/CAOILD); ct=5/6 fire the superweapon-escort teams (Iron Curtain / C-Sphere).

**Confidence: comparator packing (value@+0xe4, op@+0xe8, `[4]` array, `[0]`-only) +
the 6-value operator enum CONFIRMED; the full −1..7 ConditionType dispatch + each
slave's semantics CONFIRMED (decompiled); ct=2/ct=3 are recovered-but-unused
(semantics PROBABLE); the implementable/inert split as tabled.**

---

## §3 Weighted-selection mechanism + which team is created

**The driver `FUN_006f0010` (per-house, invoked from the House-AI update via vtable —
no direct xref, same dispatch pattern as the base-build tick `FUN_004fd320`):**

1. **Cadence gate (CONFIRMED):** `if (rand(1,100) <= house+0x565c && house+0x1f2 != 0)`.
   `house+0x565c` is seeded to **100** at house init (`FUN_004f53b0`:
   `MOV [EBP+0x565c],0x64`), so `rand(1,100) <= 100` is **always true** by default;
   `house+0x1f2` is the AI-active flag. ⇒ the trigger pass runs on **every** team-
   management pass (§5).
2. **Team accounting + cap (CONFIRMED):** count this house's live teams
   (call it `liveTeams`); the whole **trigger block only runs while `liveTeams < TotalAITeamCap[diff]`**
   (`Rules+0x13cc[house+0x184]`); at/over the cap it instead disbands the lowest-
   priority defensive team.
3. **Gather eligible (CONFIRMED):** iterate the trigger array `DAT_00a8a16c[0..DAT_00a8a178)`;
   for each `trigger`, if `ConditionMet(house, enemyHouse, …) == 1`, fetch
   `w = round(trigger.Weight_Current)` (`FUN_007c4d00`), and append `{trigger, w}` to
   a collector (`FUN_006f1b40`), accumulating `Σw`.
4. **The "5000" mandatory tier (CONFIRMED):** if any eligible trigger has
   `round(Weight) == 5000`, the collector is reset to hold **only** 5000-weight
   triggers (a hard-priority override — matches the stock 5000-weight guard/
   superweapon triggers, which always win when eligible).
5. **The weighted draw (CONFIRMED — cumulative-sum walk):**
   `r = FUN_0065c220(1, Σw)` (lockstep RNG); walk the collector accumulating
   `cum += w`; **the first entry with `r <= cum` is selected.** This is exactly a
   cumulative-sum weighted-random draw over `round(Weight_Current)`.
6. **Which team(s) (CONFIRMED — DIVERGENCE):** on selection the driver queues
   **BOTH `trigger.Team1(+0xdc)` AND `trigger.Team2(+0xe0)`** (if non-null) for the
   **same owning house**, then sets each queued TeamType's `Autocreate(+0xa9)=1` and
   returns them. Team2 is **not** a reciprocal-for-the-other-side team — it is simply
   a *second* team created together with Team1 for the owner (e.g. "Soviet Apoc Hard"
   Team1=Apoc, Team2=Terror-Drone; some triggers set Team2==Team1 → two instances).

**Confidence: gather + `round(Weight)` fetch + the 5000 override + the cumulative-sum
weighted draw via `FUN_0065c220(1,Σw)` + Team1&Team2-both-created CONFIRMED
(decompiled). The exact `CreateTeam` invocation is on the shared team/autocreate
path (the driver marks `Autocreate=1` + returns the selected TeamTypes; the actual
build is the vtable-dispatched shared path, §6) — CONFIRMED as "same pool/flag",
the literal call site PROBABLE.**

---

## §4 Weight dynamics — the adaptive engine

**Starting/min/max (CONFIRMED):** field 8/9/10 = `Weight_Current` (start),
`Weight_Minimum`, `Weight_Maximum` (ctor default 1.0/1.0/1.0, overwritten by the
INI). `Weight_Current` is the live adaptive weight; every adjustment is **clamped
to `[Weight_Minimum, Weight_Maximum]`** so start=min=max (e.g. 70/10/70) pins the
weight (no adaptation) while start≠max (e.g. 30/30/50, 50/10/70) lets it drift.

**When adjusted (CONFIRMED):** only at **team disband** — `FUN_006e8350`
(TeamClass dtor) scans the whole trigger array and, for **every** trigger whose
`Team1(+0xdc) == this-team.TeamType(+0x24)**, calls RegisterSuccess or
RegisterFailure keyed on the team's `+0x84` success flag:

- `if (team+0x84 != 0)` → **RegisterSuccess** (`FUN_0041fd80`)
- `else` → **RegisterFailure** (`FUN_0041fe40`)

**What counts as success (CONFIRMED = the `team+0x84` flag; PROBABLE mapping to
script action 49):** `team+0x84` is initialised to 0 in the TeamClass ctor
(`FUN_006e8000`) and set to `1` by a script-action handler (write of `1` at
`0x6E9258`) — i.e. the team's ScriptType ran its **success/disband action** (the
`ai-team.md §3` action `0x31`=49 "achieved success then disband"). So **success =
the team completed its script to the success action, NOT team survival and NOT the
condition clearing.** A team wiped out before reaching action 49 disbands with
`+0x84==0` → failure.

**The EXACT adjustment rule (CONFIRMED — decompiled + rulesmd constants):**
let `E = TimesExecuted(+0x108)` (pre-increment), `C = TimesCompleted(+0x104)`, and
the three `[General]` doubles `S = AITriggerSuccessWeightDelta`,
`F = AITriggerFailureWeightDelta`, `K = AITriggerTrackRecordCoefficient`:

```
RegisterSuccess:                            RegisterFailure:
  d = 0                                        d = 0
  if E > 0:                                     if E > 0:
    d = (C/E - 0.5) * E                           d = (C/E - 0.5) * K * E
    if d < 0: d = 0        (bonus >= 0)           if d > 0: d = 0        (penalty <= 0)
  Weight += S + d                              Weight += F + d
  Weight = clamp(Weight, Min, Max)            Weight = clamp(Weight, Min, Max)
  ++C ; ++E                                    ++E            (C NOT incremented)
```

**Recovered rulesmd `[General]` values (CONFIRMED — `FUN_0066cf70` + real rulesmd.ini):**
`AITriggerSuccessWeightDelta = 20` (Rules+0xc0), `AITriggerFailureWeightDelta = -50`
(Rules+0xc8), `AITriggerTrackRecordCoefficient = 1` (Rules+0xd0). The `0.5` and
the success-`E` factor are hard-coded (`_DAT_007e0738=0.5`, `_DAT_007e1800=0.0`).

So the rule is **ADDITIVE and asymmetric**: a success adds `+20` (plus a
non-negative track-record bonus that grows with the success ratio and attempt
count); a failure adds `−50` (plus a non-positive track-record penalty scaled by
`K=1`); both clamp to `[Min,Max]`. **This is nothing like the pre-hunt additive
±3.0.**

**Confidence: the disband-only adjustment site, the success = `team+0x84` flag,
the exact success/failure formulae, the clamp, and the three rulesmd constants
(20 / −50 / 1) CONFIRMED. The precise script event that sets `team+0x84` (action
49) is PROBABLE (via `ai-team.md §3`).**

---

## §5 Enable filters + evaluation cadence (all CONFIRMED in `FUN_0041e740`)

ConditionMet applies, in order, before any condition is evaluated (any failing
check ⇒ trigger ineligible):

1. **Base-defense vs offense split** — Team1/Team2 `IsBaseDefense(+0xf6)` +
   `arg3` (EnoughBaseDefense) + `Rules+0x1394[diff]` (Min defensive teams) +
   `house+0x566c` (current defensive count): when the AI has enough base defense,
   base-defense triggers are skipped and vice-versa.
2. **IsGlobal** — `if (this+0x9c==1 && <global flag>) reject`.
3. **IsEnabled (`+0xa4`)** — `if (this+0xa4==0) reject`. Set at load by the
   **`[AITriggerTypesEnable]`** section (see below).
4. **Game-mode / per-difficulty enable (INVERTED):**
   - *Skirmish/MP* (`DAT_00a8a1a0 != 0`): require `IsForSkirmish(+0xd0)!=0`, then
     the per-difficulty enable is consulted with **house-index inversion** —
     `house+0x184 == 0 → Enabled_Hard(+0xd4)`, `== 1 → Enabled_Normal(+0xd3)`,
     `== 2 → Enabled_Easy(+0xd2)`. **So difficulty index 0 is the HARDEST** (same
     hardest↔easiest inversion as `ai-base.md`/`ai-team.md`, here CONFIRMED
     concretely). The on-disk field order is Easy/Normal/Hard (fields 16/17/18,
     corroborated by the stock `E1`/`M1`/`H1` naming), but the *consult* inverts.
   - *Campaign* (`DAT_00a8a1a0 == 0`): no IsForSkirmish check; a campaign-difficulty
     global (`DAT_00a8a198+0x60c`) selects the same three flags **without** the
     inversion (0→Easy, 1→Normal, 2→Hard). (Our engine is skirmish-only ⇒ use the
     inverted binding.)
5. **OwnerHouse (`+0xa0`/`+0xa8`):** `0`→reject; `1` (Single)→require
   `CallingHouse.HouseType(+0x34)+0xb8 == HouseIndex` (country match); `2` (Any /
   `<all>`)→pass.
6. **SideIndex (`+0xac`):** `0`→any; `1`→require house side(`house+0x1e8`)==0
   (Allied); `2`→side==1 (Soviet); `3`→side==2 (Yuri). **1-based, 0=any.**
7. **TechLevel (`+0xb0`):** `if (house.techLevel(+0x1d4) < this.TechLevel) reject`.

Then the condition (§2). On pass, ConditionMet **also** checks (this is the §6
cap): Team1 (and Team2 if set) is **buildable** (`FUN_0041ff00` twice + `FUN_00509430`)
and `cntInstances(FUN_005093f0) < Max(TeamType+0xb8)` (Max<0 ⇒ uncapped) — so the
per-TeamType `Max` cap is enforced **inside ConditionMet**, before the weighted draw.

**`[AITriggerTypesEnable]` (CONFIRMED — `FUN_0041f300`):** a **separate section**
(map/AI-ini, loaded when the loader's second argument is 0) of `<trigger-id>=<bool>` entries. For each
id it finds the trigger and sets `IsEnabled(+0xa4)` — `false` in campaign disables
it, otherwise enables. It is a **per-trigger on/off list**, gated at load and
consumed by filter #3. (When a trigger is loaded "Global", i.e. second argument 1, IsEnabled
is forced 1.) It does **not** gate the whole subsystem — there is no global
"AITriggerTypes off" switch; it enables/disables individual triggers.

**Cadence (CONFIRMED mechanism; the shared-pass binding PROBABLE):** trigger
evaluation is **not** a separate AITrigger frame period and is **not** the
`AutocreateTime` spawn timer. It lives *inside* the per-house **team-management
pass** `FUN_006f0010` (the same function that enforces `TotalAITeamCap` and
disbands over-cap teams — i.e. the `TeamDelays`-gated team pass of `ai-team.md`),
additionally gated by the always-true-by-default probability
`rand(1,100) <= house+0x565c` (100). No standalone AITrigger clock exists.

**Confidence: every filter (IsEnabled, IsForSkirmish, inverted per-difficulty
enable, OwnerHouse/`<all>`, SideIndex 1=Allied/2=Soviet/3=Yuri, TechLevel≥) +
the `[AITriggerTypesEnable]` per-id IsEnabled list + the Max cap inside ConditionMet
CONFIRMED. The cadence = the team-management pass (shared with the AI-2 team pass),
gated by `house+0x565c`(=100) — mechanism CONFIRMED; the exact outer timer that
invokes `FUN_006f0010` (vtable) not isolated ⇒ "shares the TeamDelays team pass"
PROBABLE.**

---

## §6 Composition — CreateTeam path, TotalAITeamCap, attribution

- **Same team pool + creation path (CONFIRMED):** trigger creation draws from the
  **same TeamType/TaskForce/ScriptType structures** as AI-2's autocreate. **All 163
  stock TeamTypes have `Autocreate=yes`** — there are **zero** non-autocreate teams —
  so the trigger pool *is* the autocreate pool. The driver, on selecting a trigger,
  literally **sets the chosen TeamType(s)' `Autocreate(+0xa9)=1`** and hands them to
  the shared team-build path (the `CreateTeam`/TeamType path of `ai-team.md §4`).
  There is no separate trigger-specific creation code — AI-3 composes on top of
  AI-2's team machinery exactly as the `ai-team.md §4` boundary predicted.
- **TotalAITeamCap interaction (CONFIRMED):** the driver's whole trigger block is
  gated by `house-team-count < TotalAITeamCap[diff]` (`Rules+0x13cc[house+0x184]`,
  =30 per `ai-team.md`), and **each candidate is additionally capped by the per-
  TeamType `Max` (cntInstances < Max) inside ConditionMet**. Both caps apply to
  trigger creation, identically to autocreate.
- **Attribution back to the source trigger (CONFIRMED — DIVERGENCE from a stored
  back-pointer):** there is **no team→trigger pointer**. At disband, `FUN_006e8350`
  **scans the entire trigger array** and applies the weight feedback to **every
  trigger whose `Team1(+0xdc)` equals the disbanding team's TeamType** — so
  (a) attribution is by **Team1 identity match**, (b) a team created as some
  trigger's **Team2 does NOT feed back** to that trigger (only Team1 matches), and
  (c) if several triggers share a Team1, **all** of them receive the same
  success/failure.

**Confidence: same-pool/same-path composition + the `Autocreate=1` marking + the
dual cap (TotalAITeamCap + per-TeamType Max) + the Team1-match-scan attribution
(no back-pointer, Team1-only, one-team→many-triggers) CONFIRMED.**

---

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 flat positional 18-field section format + comma order | real aimd.ini + `FormatForSaving` fmt string + struct offsets | **CONFIRMED** |
| §1 AITriggerTypeClass offsets (ConditionType+0x98 … TimesExecuted+0x108), size 0x110 | ctor `FUN_0041e370` + ConditionMet + slaves | **CONFIRMED** |
| §2 comparator = `Conditions[4]{int Operand@+0xe4, uint Type@+0xe8}`, `[0]`-only, value-first | `FUN_0041f250`/`*Owns` switch + ctor + data | **CONFIRMED** |
| §2 operator enum 0=< 1=<= 2=== 3=>= 4=> 5=!= | identical switch in every slave | **CONFIRMED** |
| §2 full ConditionType dispatch −1..7 (+ ≥8 reject) + each slave semantics | `FUN_0041e740` switch + slaves | **CONFIRMED** |
| §2 ct=2/ct=3 inline house-metric conditions (unused in stock) | ConditionMet inline | **PROBABLE (unused)** |
| §2 implementable/inert split | slaves + subsystem availability | **CONFIRMED** |
| §3 driver gather + cumulative-sum weighted draw via `FUN_0065c220(1,Σw)` over `round(Weight)` | `FUN_006f0010` + `FUN_007c4d00` + `FUN_006f1b40` | **CONFIRMED** |
| §3 5000-weight mandatory-tier override | `FUN_006f0010` | **CONFIRMED** |
| §3 BOTH Team1 and Team2 created for the owner | `FUN_006f0010` | **CONFIRMED** |
| §3 literal CreateTeam site (shared autocreate/team path, vtable) | driver marks Autocreate=1 + returns list | **PROBABLE** |
| §4 adjustment at disband only; success = `team+0x84` flag | `FUN_006e8350` + ctor + `+0x84` writer | **CONFIRMED** |
| §4 success = script-completion (action 49), not survival | `+0x84` set at 0x6E9258; `ai-team.md §3` | **CONFIRMED (flag) / PROBABLE (=action 49)** |
| §4 exact formula (additive +20 / −50 + track-record ratio, clamp [Min,Max]) | `FUN_0041fd80`/`FUN_0041fe40` | **CONFIRMED** |
| §4 constants: Success 20 / Failure −50 / TrackRecord 1 | `FUN_0066cf70` + rulesmd.ini | **CONFIRMED** |
| §5 filters: IsEnabled/IsForSkirmish/OwnerHouse `<all>`/SideIndex 1-3/TechLevel≥ | `FUN_0041e740` | **CONFIRMED** |
| §5 per-difficulty enable INVERTED (house idx 0→Hard flag) | `FUN_0041e740` skirmish branch | **CONFIRMED** |
| §5 `[AITriggerTypesEnable]` = per-trigger-id IsEnabled(+0xa4) list | `FUN_0041f300` | **CONFIRMED** |
| §5 cadence = team-management pass, gated by `house+0x565c`(=100); no own period | `FUN_006f0010` + `FUN_004f53b0` | **CONFIRMED (mech) / PROBABLE (TeamDelays binding)** |
| §6 same team pool/path; all 163 teams Autocreate=yes; driver marks Autocreate=1 | aimd.ini + `FUN_006f0010` | **CONFIRMED** |
| §6 dual cap TotalAITeamCap(+0x13cc,=30) + per-TeamType Max | `FUN_006f0010` + ConditionMet | **CONFIRMED** |
| §6 attribution = Team1-match scan at disband (no back-pointer, Team1-only) | `FUN_006e8350` | **CONFIRMED** |

---

## DIVERGENCES FROM THE PLAN'S PRE-HUNT ASSUMPTIONS (drives the RE-GATE)

- **[PLAN OK — Task 2] The 18-field order is confirmed** exactly as
  `Name, Team1, OwnerHouse, TechLevel, ConditionType, ConditionObject, Conditions,
  StartWeight, MinWeight, MaxWeight, IsForSkirmish, 0, SideIndex, IsForBaseDefense,
  Team2, Enabled_Easy, Enabled_Normal, Enabled_Hard`. Note TechLevel is field 4 and
  ConditionType is field 5 (both int); SideIndex is field 13 (0=any/1=Allied/2=Soviet/
  3=Yuri); the enables are fields 16/17/18 in **Easy,Normal,Hard** on-disk order.
  Replace any fixture that assumed a different order with the recovered one.

- **[PLAN DIFF — Task 2/3, STOP] Comparator packing is `{value, operator}`
  value-first**, a **32-byte `Conditions[4]` blob with only slot 0 used** (dword0 =
  count N @+0xe4, dword1 = operator @+0xe8, dwords 2–7 always 0). If `decode_comparator`
  assumed operator-first or a single 8-byte field, fix it: parse 64 hex chars → 8 LE
  dwords, use `[0]`=value and `[1]`=op, ignore `[2..7]`. Operator enum:
  `0<, 1<=, 2==, 3>=, 4>, 5!=`, tested as `actualCount OP value`.

- **[PLAN DIFF — Task 3, STOP] The ConditionType enum is −1..7, and `ct=4` is
  HouseCredits, NOT "enemy credits".** Recovered set:
  `−1 none/always; 0 EnemyHouseOwns; 1 OwnerHouseOwns; 2/3 inline house-metric (unused);
  4 HouseCredits; 5 IronCurtainCharged; 6 ChronoSphereCharged; 7 CivilianHouseOwns`.
  Replace the pre-hunt `kCond*` set (incl. `kCondEnemyCredits`) accordingly. Add a
  neutral/civilian-house count path for ct=7. **Stock uses only {0,1,4,5,6,7}**;
  ct=4 is always `N=0,op≥` ⇒ effectively "always true" (implement as such unless
  credits are readable). Implementable core for the engine: **−1, 0, 1, 4(as-always-
  true), 7(if a civilian house exists)**; **inert-required-subsystem-absent: 5, 6
  (superweapons)**; **inert/unused: 2, 3**. `TriggerView` needs per-house
  owned-count-by-TechnoType (owner + enemy + neutral), not enemy-credits.

- **[PLAN DIFF — Task 3, STOP] The weighted draw IS a cumulative-sum walk (keep it),
  BUT the weight is `round(Weight_Current)` (int), and there is a `5000` mandatory
  tier.** If any eligible trigger's rounded weight == 5000, only 5000-weight triggers
  are candidates. Use the seeded xorshift32 substituting `FUN_0065c220(1, Σw)`.

- **[PLAN DIFF — Task 3, STOP] BOTH Team1 and Team2 are created for the owner** — not
  "team1 for owner, team2 the reciprocal." `select_trigger`/`TriggerDecision` must
  emit up to two `CreateTeam` calls (Team1, then Team2 if set), both for the owning
  house. Team2 may equal Team1 (⇒ two instances).

- **[PLAN DIFF — Task 3/4, STOP] The weight rule is additive +20 (success) / −50
  (failure) + a track-record ratio term, clamped to [Min,Max] — NOT ±3.0.** Replace
  `kAiTriggerWeightDelta`: `kAiTriggerSuccessWeightDelta=+20`,
  `kAiTriggerFailureWeightDelta=-50`, `kAiTriggerTrackRecordCoefficient=1`, with
  `delta_success = max(0, (C/E − 0.5)*E)`, `delta_failure = min(0, (C/E − 0.5)*K*E)`
  (E=total attempts pre-increment, C=successes), then clamp. Weights where start==min==max
  never move (most stock triggers). Seed `Weight_Current=StartingWeight`.

- **[PLAN DIFF — Task 4, STOP] Success = the created team reached its script's success/
  disband action (`team+0x84` flag), NOT "survivors at disband".** Replace the
  pre-hunt survivors-at-disband success rule: a team that ran its ScriptType to the
  success action (action 49 per `ai-team.md §3`) → RegisterSuccess; a team wiped out
  or disbanded without it → RegisterFailure. Adjustment happens **only at disband**.

- **[PLAN DIFF — Task 3/4, STOP] Per-difficulty enable is INVERTED** — house difficulty
  index `0 → Enabled_Hard`, `1 → Enabled_Normal`, `2 → Enabled_Easy` (index 0 = hardest),
  even though the fields parse in Easy/Normal/Hard order. `trigger_filters_pass` must
  index the enables with the same hardest↔easiest inversion as `ai-base.md`/`ai-team.md`.
  Also: `IsForSkirmish` must be 1 (all stock are); SideIndex 1/2/3 = Allied/Soviet/Yuri
  (0=any); OwnerHouse `<all>`=Any, country name=Single-match, else reject; TechLevel is a
  `house.techLevel ≥ field` gate.

- **[PLAN DIFF — Task 4, cadence] There is NO separate AITrigger period.** Trigger
  evaluation runs inside the per-house **team-management pass** (`FUN_006f0010` — the
  same pass that enforces `TotalAITeamCap` and disbands over-cap teams), gated by an
  always-true-by-default probability (`rand(1,100) <= house+0x565c`, house+0x565c=100).
  Hook the trigger eval into the AI-2 team pass, not a new clock. If Task 4 added a
  standalone `kAiTriggerFrames` clock, drop it (or set it equal to the team pass).

- **[PLAN DIFF — Task 6, STOP] There are ZERO non-autocreate teams** — all 163 stock
  TeamTypes are `Autocreate=yes`, and the trigger pool == the autocreate pool. The
  Task 6 exemplar filter cannot be "team1 is a non-autocreate team." Use real
  autocreate teams; a created team is attributed to a trigger by **Team1 identity**.
  Good Soviet exemplars (enemy-owns-building → non-superweapon Soviet team): `Soviet
  Engineer vs Ally med [0D53430C-G]` (ct=0 GACNST ≥1 → team1 `Soviet Engineer vs Ally`),
  `Soviet Ivan vs Allies [0C87B45C-G]` (ct=0 GACNST ≥1), `Soviet Armor Hard
  [0C8C56FC-G]` (ct=4 always, weight 70/40/70 — adaptive!). Owner-owns exemplar:
  `Soviet Apoc Medium [0D2C45DC-G]` (ct=1 NATECH ≥1). Unit-object: `Soviet Terror D vs
  Allies [0A452E5C-G]` (ct=0 MTNK ≥8), `Soviet Miners [0C8C2C3C-G]` (ct=1 HARV ≤3).

- **[PLAN DIFF — Task 4, attribution] No team→trigger back-pointer.** The original
  scans all triggers at disband for `Team1==team.TeamType`. Task 4's parallel
  `team_links` should mirror this: match by Team1 identity, apply to **all** triggers
  sharing that Team1, and **only Team1** (a Team2-created team gives no feedback).

## Deviations we take (engine-local)

- **[DEVIATION — RNG]** The original uses the lockstep game RNG (`FUN_0065c220`) for
  the cumulative-sum weighted draw. Our single-machine engine substitutes a seeded
  **xorshift32** (a dedicated `AiTriggerState::rng` stream) — same distribution, not
  bit-identical to the netgame sequence (as `ai-base.md`/`ai-team.md`).

- **[DEVIATION — ct=4 HouseCredits]** Stock only uses ct=4 as `N=0, op≥` (always
  true), so we implement it as **always-true** for stock data; a general credits
  compare is implementable only if the owning/enemy house credits are visible to
  the trigger view (they are in our economy model), so ct=4 can be a real
  `credits OP N` if a mod supplies non-zero N.

- **[DEVIATION — inert conditions]** ct=5 (IronCurtainCharged) and ct=6
  (ChronoSphereCharged) evaluate FALSE until the superweapon subsystem exists;
  ct=2/ct=3 are recovered but unused by any stock trigger and are left unimplemented
  (evaluate FALSE). ct=7 (CivilianHouseOwns) evaluates FALSE unless a neutral/civilian
  house with capturable buildings is present.

- **[DEVIATION — enemy aggregation]** ConditionMet is called with a single
  `TargetHouse` (the AI's current enemy, `HouseArray[house+0x5600]`); EnemyHouseOwns
  counts that one house. Where our World has multiple rivals we aggregate all rival
  houses for the enemy-owns count (as `ai-team.md`), and use the neutral house for
  ct=7.
