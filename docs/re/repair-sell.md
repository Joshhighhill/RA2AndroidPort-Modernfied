# Building Repair & Sell — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts `rs1.py … rs15.py`
off-repo at `D:\ra2-re\scripts\`; decompiles + forced listings at
`D:\ra2-re\out\rs*`. **Content-anchored only.** Every offset below was reached
from an INI-key string reference in *this* binary (`RepairPercent` `0x83ADEC`,
`RefundPercent` `0x83ADFC`, `RepairStep` `0x83ADE0`, `RepairRate` `0x83ADC8`,
`Unsellable` `0x819DEC`, `ClickRepairable` `0x819DDC`, `ConstructionYard`
`0x819A84`, `Crewed` `0x84290C`, `AlliedSurvivorDivisor` `0x83AEA0`,
`MCVRedeploys` `0x83BF60`), from the Rules static base `0x886160`, or from
vtable-slot walks + xrefs. Published YRpp/Ares addresses were used only as a
*naming* map; the RulesClass field order in `YRpp/RulesClass.h` was
independently corroborated at four points in this binary and is quoted as a
hint, never as evidence.)

Companion of `construction.md` (BuildTime/BuildupTime/money helpers),
`building-render.md` (§4 Shape_Number, §5 buildup anim) and `capture.md`
(§A5 engineer repair). Calibration authority for the **Repair button** and the
**Sell button**, both currently inert in the clone.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_0066CF70` | `RulesClass` `[General]` loader — `RepairPercent`→`+0x16D0`(dbl), `RepairStep`→`+0x16CC`(int), `IRepairStep`→`+0x16D8`, `RepairRate`→`+0x16E0`(dbl), `URepairRate`→`+0x16E8`, `RefundPercent`→`+0x1738`(dbl), `AlliedSurvivorDivisor`→`+0x14F8`, `Soviet…`→`+0x14FC`, `Third…`→`+0x1500` | INI-key string refs @`0x6707CD`–`0x67086D`, `0x67065D` |
| `FUN_006718E0` | `[MultiplayerDialogSettings]` loader — `MCVRedeploys`→ rules `+0x14B8` | string ref `0x671C46` |
| `FUN_0045FE40` | `BuildingTypeClass::LoadFromINI` — `Unsellable`→`+0x1579`, `ClickRepairable`→`+0x157A`, `ConstructionYard`→`+0x16B9`, `Wall`→`+0x1571`, `UnitReload`→`+0x16A9`, `FirestormWall`→`+0x16C0`, `Capturable`→`+0x1572` | key→store map, `rs13_keymap_bldgtype_45fe40.txt` |
| `FUN_00711510` | `TechnoTypeClass::LoadFromINI` — `Crewed`→`+0xCCD` | string ref `0x713DD6` |
| `FUN_00446F40` | **`BuildingClass::Repair(int control)`** — vtable **+0x19C** | vtable walk; `-1/0/1` control convention |
| `FUN_00447060` | **`BuildingClass::Sell_Back(int control)`** — vtable **+0x1A0** | vtable walk; queues mission `0x13` |
| `FUN_00450620` | **the per-tick repair applier + AI repair/sell decision** (`BuildingClass::Repair_AI`) | only non-vtable caller of `vt+0x19C`; forced listing `rs11_lst_repair_applier_450620.txt` |
| `FUN_00449B70` | **`BuildingClass::Mission_Selling`** — 3-state machine, vtable +0x248 | `EVA_StructureSold` @`0x818040` / `EVA_UnitSold` @`0x818030` refs |
| `FUN_00711470` | `TechnoTypeClass::GetRepairStepCost` — btvtbl **+0xB0** | BuildingTypeClass vtable slot walk; listing |
| `FUN_007114C0` | `TechnoTypeClass::GetRepairStep` — btvtbl **+0xB4** (returns `Rules+0x16CC`) | same |
| `FUN_00711300` | `TechnoTypeClass::GetRefund(HouseClass*, bool)` — btvtbl **+0xB8** | same; listing `rs5_listing_getrefund_711300.txt` |
| `FUN_0070A220` | `TechnoClass::Refund_Amount()` — vtbl **+0x2BC** = `Type->GetRefund(Owner, false)` | callee at the sell payout site |
| `FUN_0070B300` | `TechnoClass::Is_Repair_Due()` — the *unit/infantry* self-repair cadence (same `RepairRate×900` gate) | listing `rs5_listing_repairdue_70b300.txt` |
| `FUN_00449410` | **`BuildingClass::Can_Be_Sold()`** | reader of `Type+0x1579` (`Unsellable`) |
| `FUN_00452620` | **`BuildingClass::Can_Be_Click_Repaired()`** | reader of `Type+0x157A` (`ClickRepairable`) |
| `FUN_0044F5C0` | **`BuildingClass::Can_Undeploy()`** — the MCV rule | reader of `Type+0x16B9` + `Type+0x408` |
| `FUN_00451320` | `BuildingClass::Survivor_Count()` — vtbl **+0x2D0** | reader of `Rules+0x14F8/4FC/500` |
| `FUN_0044EB10` | `BuildingClass::Crew_Type()` — vtbl **+0x30C** | callee of the survivor loop |
| `FUN_0043EF10` | `BuildingClass::Shape_Number` — mirrors the buildup range when mission == `0x13` | `building-render.md` §4; re-read here |
| `FUN_00451ED0` / `FUN_00451880` | damage-state anim swap / `PlayAnim(name, slot, damaged)` | callees of the repair tick |
| `FUN_004AC7D0` / `FUN_004AC570` / `FUN_004AC730` / `FUN_004AC610` / `FUN_004AC870` | `DisplayClass::Set{Repair,Sell,Power,Planning,PlaceBeacon}Mode` — flags `+0x11B0…+0x11B4` | the 5-call "clear all modes" prologue at `FUN_00730290/470/590` |
| `FUN_006A6E10` | `SidebarClass::Action` — button `0x8065` → `SetRepairMode(-1)`, `0x8066` → `SetSellMode(-1)` | reader of `+0x11B1`, `+0x11B0` |
| `FUN_004AAC40` | mode-cancel priority chain = DisplayClass right-mouse-up virtual (called from the tactical WndProc `FUN_00692AB0` on WM_RBUTTONUP when no right-drag scroll happened); ends in `FUN_0048DBA0` (deselect all). **Not** bound to ESC — see note below | reader of all five mode flags |
| `FUN_004C6BD0` | network `EventClass::Execute` — the REPAIR and SELL event cases at `0x4C6DF2` / `0x4C6E32` | only non-vtable callers of `vt+0x19C`/`vt+0x1A0` in the executor |
| `FUN_004F95B0` / `FUN_004F9770` | `HouseClass::Spend_Money` / `Refund_Money` (`Credits` @house `+0x30C`) | already pinned in `construction.md`; re-confirmed |
| `FUN_005F5470` | `TechnoClass::Health_Ratio()` = `Health(+0x6C) / Type->Strength(+0xA0)` (x87 double) | callee everywhere below |
| `FUN_0050B510` / `FUN_0050B550` | house "is the local player" / "is human-controlled" | `construction.md` |
| `FUN_007008B0` | `TechnoClass::Repair_To_Full` — vtbl **+0x40C**, the **engineer** path | `capture.md` §A5 |

Object-field offsets used below (all read out of the code, not from YRpp):

| Offset | Field | Evidence |
|--------|-------|----------|
| `obj+0x6C` / `+0x70` | `Health` / `EstimatedHealth` | both incremented by the repair step; clamped to `Type->Strength` |
| `bldg+0x21C` | `Owner` (HouseClass\*) | argument to `Spend_Money`/`Refund_Money`/`Is_Human` |
| `bldg+0x520` | `Type` (BuildingTypeClass\*) | reads `+0xA0 Strength`, `+0x408 UndeploysInto` |
| `bldg+0x534` | `BState` | `0` ⇒ buildup; `Can_Be_Sold` requires `!= 0` |
| `bldg+0x55C[21]` | `Anims[21]` (stride 4) | loop bound `0x594/0x44 = 21` in the damage-anim swap |
| `bldg+0x5E8[3]` / `+0x702` | `Upgrades[3]` / `UpgradesCount` | the upgrade-refund loop |
| `bldg+0x6DE` | `NeedsRepairs` (toggled every repair tick) | `SETZ DL; MOV [ESI+0x6DE],DL` |
| `bldg+0x6DF` | `C4Applied` | blocks `Sell_Back(1)` |
| `bldg+0x6E0` / `+0x6E3` | `NoCrew` / `HasBeenCaptured` | survivor count |
| `bldg+0x6E6` | `IsDamaged` | set to `HealthRatio <= ConditionYellow`, drives the anim swap |
| `bldg+0x6E8` | **`IsBeingRepaired`** — the per-building repair flag | set/cleared by `Repair()`, consumed by the tick |
| `bldg+0x6E9` | `HasBuildUp` | `Sell_Back` takes the instant path when clear |
| `house+0x30C` | `Credits` | `Refund_Money` is literally `Credits += n` |
| `type+0x610` / `+0x614` | `Cost` / `Soylent` | `GetCost` base impl; `GetRefund` override |
| `type+0xF04/0xF08` (+`BState*0xC`) | buildup frame `start`/`count` | `Shape_Number` |
| `type+0xF4C+i*0x44` / `+0xF5C+i*0x44` | healthy / damaged anim name for anim slot *i* | the damage-anim swap |

---

## Ground truth — the real `rulesmd.ini` on this install

Extracted from `ra2md.mix` via the nested fallback chain (`ra2extract`), file
`D:\ra2-re\out\shroud_evidence\rulesmd.ini`, `[General]` section, **verbatim**:

```ini
; repair and refit
RefundPercent=50%       ; percent of original cost to refund when building/unit is sold
ReloadRate=.3           ; minutes to reload each ammo point for aircraft or helicopters
RepairPercent=15%       ; percent cost to fully repair as ratio of full cost
RepairRate=.016         ; minutes between applying repair step
RepairStep=8            ; hit points to heal per repair 'tick'
URepairRate=.016        ; [units only] minutes between applying repair step
IRepairRate=.001        ; [infantry only] minutes between applying repair step
IRepairStep=20           ; [infantry only] hit points to heal per repair 'tick' for infantry
```

```ini
BuildupTime=.06         ; average minutes that building build-up animation runs
```

```ini
ConditionRed=25%        ; when damaged to this percentage, health bar turns red
ConditionYellow=50%     ; when damaged to this percentage, health bar turns yellow
```

```ini
SurvivorRate=.4         ; fraction of building cost to be converted to survivors when sold ;gs This is no longer used, as the sell price of the building gives more control
AlliedSurvivorDivisor=500     ; gs divide the sell cost of a building by this to get number of survivors
SovietSurvivorDivisor=250 ; gs this split is needed since no longer have a common infantry
ThirdSurvivorDivisor=750
```

```ini
[MultiplayerDialogSettings]
MCVRedeploys=yes
```

> **`URepairStep` and `URepairPercent` DO NOT EXIST.** Neither key appears in
> `rulesmd.ini` and neither string exists in `gamemd.exe`. There is exactly one
> `RepairStep` (buildings + vehicles) and one `IRepairStep` (infantry);
> there is exactly one `RepairPercent`. `URepairRate` exists but is a *rate*,
> not a step. The task brief's assumption is wrong. **CONFIRMED.**
>
> `SurvivorRate` is present but **dead** — the engine never reads it (see §B3);
> the comment in the file says so and the code agrees.

---

# A. Repair

## A1 — The rules keys and their offsets — **CONFIRMED**

`RulesClass::General()` = `FUN_0066CF70`, forced listing (`rs12_inikeys.txt`):

```
006707cd  PUSH 0x83adfc          ; "RefundPercent"
006707d5  CALL 0x00528100        ; INIClass::ReadDouble
006707da  FSTP qword ptr [ESI + 0x1738]

006707f4  PUSH 0x83adec          ; "RepairPercent"
006707fc  CALL 0x00528100
00670801  FSTP qword ptr [ESI + 0x16d0]

00670813  PUSH 0x83ade0          ; "RepairStep"
0067081b  CALL 0x00527400        ; INIClass::ReadInt
00670820  MOV dword ptr [ESI + 0x16cc],EAX

00670833  PUSH 0x83add4          ; "IRepairStep"    -> [ESI + 0x16d8]
0067085a  PUSH 0x83adc8          ; "RepairRate"
00670862  CALL 0x00528100
00670867  FSTP qword ptr [ESI + 0x16e0]
00670881  PUSH 0x83adbc          ; "URepairRate"    -> [ESI + 0x16e8]
```

`Rules` static instance = `[0x00886160]`. So:

| Key | Rules offset | Type | Real value |
|-----|--------------|------|------------|
| `RepairStep` | `+0x16CC` | int | **8** |
| `RepairPercent` | `+0x16D0` | double | **0.15** |
| `IRepairStep` | `+0x16D8` | int | 20 |
| `RepairRate` | `+0x16E0` | double | **0.016** |
| `URepairRate` | `+0x16E8` | double | .016 |
| `ConditionYellow` | `+0x1700` | double | **0.50** |
| `ConditionRed` | `+0x1708` | double | **0.25** |
| `RefundPercent` | `+0x1738` | double | **0.50** |

`ConditionYellow/Red` at `+0x1700/+0x1708` are confirmed by *use* (§A5), and
`+0x1738 RefundPercent` is independently cross-checked: `construction.md`
content-pinned `BuildSpeed` at `Rules+0x1748`, and `RefundPercent(dbl) +
BridgeStrength(int) + pad` lands exactly on it.

## A2 — The repair tick — **CONFIRMED** (exact arithmetic)

`FUN_00450620`, forced listing `0x00450803`–`0x004509B2`:

```
00450803  MOV AL,byte ptr [ESI + 0x6e8]       ; IsBeingRepaired
0045080b  JZ  0x004509ab                      ; not repairing -> done
00450811  MOV EAX,[0x00886160]
00450816  FLD  qword ptr [EAX + 0x16e0]       ; Rules.RepairRate
0045081c  FMUL qword ptr [0x007e17f8]         ; * 900.0   (verified: [7E17F8] == 900.0)
00450822  CALL 0x007c4d00                     ; __ftol   -> interval (frames)
00450829  MOV EAX,[0x00a8dcec]                ; Frame
0045082f  IDIV ECX
00450831  TEST EDX,EDX
00450833  JNZ 0x004509ab                      ; Frame % interval != 0 -> done
00450839  MOV BL,byte ptr [ESI + 0x6de]
00450847  SETZ DL
0045084a  MOV byte ptr [ESI + 0x6de],DL       ; NeedsRepairs = !NeedsRepairs  (blink)
00450852  CALL dword ptr [EAX + 0xb0]         ; EDI = Type->GetRepairStepCost()
00450862  CALL dword ptr [EDX + 0xb4]         ; EBP = Type->GetRepairStep()
0045086c  CALL 0x0043ef10                     ; oldShape = Shape_Number()
00450881  CALL dword ptr [ECX + 0x18]         ; EAX = Owner->Available_Money()
00450884  CMP EAX,EDI
00450886  JL  0x004509a4                      ; credits < stepCost  ->  STOP REPAIRING
00450893  CALL 0x004f95b0                     ; Owner->Spend_Money(stepCost)
00450898  MOV EBX,[ESI + 0x6c]                ; Health
0045089b  MOV EDI,[ESI + 0x70]                ; EstimatedHealth
004508a4  ADD EBX,EBP                         ; += stepHP
004508a6  ADD EDI,EBP
004508b6  CMP ECX,EAX                         ; vs Type->Strength
004508b8  JL  0x004508c7
004508ba  MOV [ESI + 0x6c],EAX                ; clamp to Strength
004508bd  MOV [ESI + 0x70],EAX
004508c0  MOV byte ptr [ESI + 0x6e8],0x0      ; AUTO-STOP at full health
...
004509a4  MOV byte ptr [ESI + 0x6e8],0x0      ; the out-of-money exit
```

**Cadence.** `interval = trunc(RepairRate × 900.0)` frames, and the tick fires
only when `Frame % interval == 0` — a **global phase**, not a per-building
timer. With `RepairRate=.016`: `0.016 × 900 = 14.4 → trunc = 14` frames. At
15 Hz that is **one repair step every 14 frames (0.9333 s)**.

> Two traps here. (1) `.016 × 900` is `14.4`, and `__ftol` **truncates**, so the
> interval is 14 and not 15 — a 7 % speed difference. (2) It is `Frame %
> interval`, so *all* repairing buildings of *all* houses tick on the same
> frames; a building that starts repairing does not get its own timer and may
> wait 0–13 frames for the first step.

**HP per step.** `Type->GetRepairStep()` = `FUN_007114C0`, seven bytes:

```
007114c0  MOV EAX, dword ptr [0x00886160 + 0x16cc]   ; = Rules.RepairStep
          RET
```

So **8 HP per step, flat**, independent of the building. `Health` *and*
`EstimatedHealth` (`+0x70`) both advance, and both clamp to `Type->Strength`.

**Cost per step.** `Type->GetRepairStepCost()` = `FUN_00711470`, whole function:

```
00711477  CALL dword ptr [EAX + 0xac]        ; EDI = this->GetCost()      (Cost)
0071147d  MOV ECX,[0x00886160]               ; Rules
00711485  MOV EAX,[ESI + 0xa0]               ; Strength
0071148b  CDQ
0071148c  IDIV dword ptr [ECX + 0x16cc]      ; ESI = Strength / RepairStep      (int div)
00711497  IDIV ESI                           ; EAX = Cost / (Strength/RepairStep)  (int div)
0071149d  FILD dword ptr [ESP + 0x8]
007114a1  FMUL qword ptr [ECX + 0x16d0]      ; * Rules.RepairPercent
007114a7  CALL 0x007c4d00                    ; __ftol  (truncate)
007114ad  CMP EAX,0x1
007114b1  JG  0x007114b8
007114b3  MOV EAX,0x1                        ; clamp to >= 1
```

```
stepCost = max( 1, trunc( (Cost / (Strength / RepairStep)) * RepairPercent ) )
                          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                          BOTH divisions are integer, truncating
```

Worked from the real rules (`RepairStep=8`, `RepairPercent=0.15`):

| Building | Cost | Strength | `Strength/8` | `Cost/that` | `×0.15` | **stepCost** | total to full |
|----------|-----:|---------:|-------------:|------------:|--------:|-------------:|--------------:|
| `GAPOWR` | 800 | 1000 | 125 | 6 | 0.90 | **1** | 125 |
| `GACNST` | 3000 | 1000 | 125 | 24 | 3.60 | **3** | 375 |
| `GAWEAP` | 2000 | 1000 | 125 | 16 | 2.40 | **2** | 250 |

> **This overturns the naive implementation.** "15 % of Cost to fully repair"
> is *not* what the engine does — the double truncation makes real repair
> markedly cheaper (GACNST: 375 credits, not 450; GAPOWR: 125, not 120 — it can
> go either way because of the `max(1)` clamp). Anything that computes
> `Cost × RepairPercent × (damage/maxHP)` in floating point will be wrong.

## A3 — Out of credits mid-repair: **STOP, silently** — **CONFIRMED**

`00450884: CMP EAX,EDI / JL 0x004509a4` → `MOV byte [ESI+0x6E8], 0`. The check
is *before* spending; the step is not applied, no partial repair, no free
repair, and the per-building repair flag is **cleared** — the player must click
the wrench again after refilling. No EVA line is played on this path
(`EVA_InsufficientFunds` @`0x818054` is referenced only from `FUN_0044B780`,
the *service-depot unit-repair* path, and from `FUN_004F8350`).

Note `Owner->Available_Money()` is `(Owner+0x24)->vtbl[0x18]()`, and
`Spend_Money` (`FUN_004F95B0`) will drain **stored ore** when `Credits` alone is
short — so "credits" here means credits + silo contents, matching the rest of
the economy code (`economy.md`).

## A4 — Repair is a **per-building toggle** — **CONFIRMED**

`BuildingClass::Repair(int control)` = `FUN_00446F40` (vtbl **+0x19C**), the
classic Westwood `-1 = toggle / 0 = off / 1 = on` convention:

```c
if (control == -1) IsBeingRepaired = !IsBeingRepaired;
else if (control == 0) { if (!IsBeingRepaired) return; IsBeingRepaired = 0; }
else if (control == 1) { if ( IsBeingRepaired) return; IsBeingRepaired = 1; }

if (IsBeingRepaired && Health != Type->Strength) {
    if (Is_Local_Player(Owner)) this->vtbl[0x148](7);      // click/wrench sound id 7
    this->NeedsRepairs = 1;                                // +0x6DE
    if (Is_Local_Player(Owner)) Speak("EVA_Repairing");    // 0x817F18
}
if (Is_Local_Player(Owner)) Sound_Effect(..., this->Coords);   // 0x0074F800
```

The network event carries the toggle: `EventClass::Execute` (`FUN_004C6BD0`)
case at `0x004C6DF2`:

```
004c6df5  CALL 0x006e6490          ; resolve event target
004c6e0d  MOV AL,[ESI + 0x90]      ; target alive/on-map?
004c6e1d  PUSH -0x1
004c6e21  CALL dword ptr [EAX + 0x19c]     ; target->Repair(TOGGLE)
```

So: **click the wrench cursor on one of your buildings → a REPAIR event is
queued → on execution it toggles that building's `IsBeingRepaired`.** It is a
per-building flag, not a global mode; the *cursor* is the mode.

**Turning off** happens in five ways, all seen in code:
1. clicking the same building again (toggle),
2. `Repair(0)` at the head of `Mission_Selling` (`0x00449B81`) — selling always
   stops repair,
3. reaching full health (`0x004508C0`),
4. running out of money (`0x004509A4`),
5. the engineer full-repair path calls `Repair(0)` (`FUN_007008B0`).

**Which buildings accept the wrench** — `FUN_00452620`
(`Can_Be_Click_Repaired`):

```c
if (this->Health == 0) return false;
if (!Type->ClickRepairable /*+0x157A*/) return false;   // default yes
if (this->vtbl[0x80]()) return false;                   // in-limbo / being built
return FUN_007005E0(this);                              // "is a valid selection target"
```

## A5 — Damage state and the fire/damaged art — **CONFIRMED**

Immediately after each successful repair step (`0x004508C7`–`0x00450993`):

```
004508c9  CALL 0x005f5470                    ; ratio = Health / Strength
004508d3  FCOMP qword ptr [EAX + 0x1700]     ; vs Rules.ConditionYellow
004508db  TEST AH,0x41                       ; C3|C0 -> ratio <= ConditionYellow
004508e0/e7  EBX = damaged ? 1 : 0
004508e9  CMP byte ptr [ESI + 0x6e6],BL      ; IsDamaged changed?
   -> for i in 0..20:                        ; 0x594 / 0x44 == 21 anim slots
        if (this->Anims[i] != NULL) {
            name = damaged ? (char*)(Type + 0xF5C + i*0x44)
                           : (char*)(Type + 0xF4C + i*0x44);
            if (name[0]) this->PlayAnim(name, i, damaged, 0, 0);   ; FUN_00451880
        }
0045095e  CALL 0x005f5470
00450969  FCOMP qword ptr [EDX + 0x1700]
00450974  JNZ ...                            ; ratio > ConditionYellow ?
00450976  MOV ECX,[ESI + 0x310]              ; the damage smoke/fire anim
00450982  CALL dword ptr [EAX + 0xf8]        ;   -> destroy it
0045098a  CALL 0x0043ef10                    ; newShape = Shape_Number()
00450993  JZ ...
00450995  MOV byte ptr [ESI + 0x80],0x1      ; mark for redraw
```

So yes, and precisely:

* the threshold is `HealthRatio <= ConditionYellow` (**≤**, not `<`) — repairing
  *above* 50 % flips `IsDamaged` back to 0;
* the flip re-issues **every** building anim from the *undamaged* name table
  (`Type+0xF4C+i*0x44`) instead of the *damaged* one (`Type+0xF5C+i*0x44`), for
  all 21 anim slots that currently have a live anim;
* the persistent damage-smoke/fire anim at `bldg+0x310` is **destroyed** the
  moment the ratio rises above `ConditionYellow`;
* if `Shape_Number()` changed, the building is marked dirty for redraw.

The same swap is factored into `FUN_00451ED0(damaged)` and used by the engineer
path, so the clone can share one routine. `ConditionRed` (`+0x1708`) is **not**
used for building art here — only for the health-bar colour and for the AI's
"sell it, it's nearly dead" test (§A7).

## A6 — Engineer repair is a **different** code path — **CONFIRMED**

`capture.md` §A5 pinned the engineer to `bldg->vtbl[0x40C]` = `FUN_007008B0`:

```c
this->vtbl[0x124]();                                  // stop/clear
this->Health = this->GetType()->Strength;             // INSTANT full heal
this->EstimatedHealth = Strength;
if (this->WhatAmI() == 6 /*Building*/) {
    this->Repair(0);                                  // vtbl+0x19C, force repair OFF
    Update_Damage_Anims( HealthRatio() <= Rules.ConditionYellow );   // FUN_00451ED0
    Sound_Effect(2, this->Coords);
}
```

No credits are spent, no `RepairStep`, no cadence — one instant heal to
`Strength`. It shares only the damage-anim update. Keep the two implementations
separate.

## A7 — (bonus) the AI's own use of the same two entry points — **PROBABLE**

The first half of `FUN_00450620` (before the tick) is the AI hand-holder. It is
worth recording because it explains why AI bases repair themselves and sell
dying buildings — the clone can ignore it for now, but it must **not** be
mistaken for player logic:

```c
if (Owner->[0x24C] >= Rules->[0x1444]                 // house IQ >= Rules.IQ.<something>
 && GetCurrentMission() != 0x12 /*Construction*/
 && GetCurrentMission() != 0x13 /*Selling*/
 && this->vtbl[0x94]()) {
    if (Owner->Available_Money() >= Rules->[0x1758]) {
        if (Owner->[0x245]) goto tick;                // one repair-start per house per pass
        if (IsBeingRepaired) goto tick;
        if (HasBeenCaptured || this->[0x6CB] || Is_Human(Owner)) {
            Owner->[0x245] = 1;
            this->Repair(1);                          // <-- same vtbl+0x19C
        }
    } else {                                          // low on money
        if ((IsMultiplayer || this->AI_Sellable /*+0x6DC*/)
         && this->[0x3D1] && Owner->[0x1D0] >= Rules->[0x145C]
         && Random(0,50) < Owner->[0x1D4]
         && this->Factory == 0
         && Type->[0xEB8] != 7
         && HealthRatio() < Rules.ConditionRed)       // strict <
            this->Sell_Back(1);                       // <-- same vtbl+0x1A0
    }
}
```

---

# B. Sell

## B1 — `RefundPercent` and the refund formula — **CONFIRMED**

Payout site, `Mission_Selling` state 2 (decompile `rs3_sell_evaref_00449b70.c`
L483-484, listing `0x0044AAE5`):

```
0044aae5  CALL dword ptr [EDX + 0x2bc]    ; refund = this->Refund_Amount()
0044aaf2  CALL 0x004f9770                 ; Owner->Credits += refund
```

`TechnoClass::Refund_Amount()` = `FUN_0070A220` (vtbl +0x2BC) is two lines:

```c
return this->GetTechnoType()->GetRefund(this->Owner, /*ignorePercent=*/false);
```

`TechnoTypeClass::GetRefund(HouseClass* h, bool ignorePercent)` = `FUN_00711300`
(btvtbl +0xB8), from the forced listing `rs5_listing_getrefund_711300.txt`:

```
0071130c  FLD  qword ptr [Rules + 0x1738]   ; RefundPercent
00711316  MOV AL, [arg2]                    ; ignorePercent
0071131e  MOV dword [ESP+8], 0x3f800000     ;   -> mul = 1.0f if set
;
; h == NULL:   return ftol( GetCost() * mul )
;
00711335  CALL 0x0050bc10                   ; m1 = h->CountryCostMult(this)     (float)
00711341  CALL 0x0050bcd0                   ; m2 = h->HouseCostMult(this)       (float)
00711346  MOV EAX,[ESI + 0x614]             ; Soylent
00711356  JZ  0x0071136d
00711358  FILD ...; FMUL m1; CALL __ftol     ; Soylent != 0 -> return ftol(Soylent * m1)
;
00711371  CALL dword [EDX + 0xac]           ; Cost
0071137b  FILD; FMUL m2; FMUL m1; __ftol     ; v = ftol(Cost * m2 * m1)
00711394  CALL 0x0050b550                   ; if (h is human-controlled)
0071139d  FILD; FMUL mul; __ftol             ;    v = ftol(v * RefundPercent)
```

```
refund = trunc( trunc(Cost × HouseCostMult × CountryCostMult) × RefundPercent )
         ── or, when Soylent != 0 ──
refund = trunc( Soylent × CountryCostMult )        // RefundPercent NOT applied
```

* **The refund does NOT scale with remaining HP.** A building at 1 HP refunds
  exactly as much as a building at full health. There is no health term
  anywhere in `GetRefund`, `Refund_Amount` or the payout site. **CONFIRMED.**
  With the real `RefundPercent=50%`: GACNST → `trunc(3000 × 0.5) = 1500`.
* `Soylent` (`type+0x614`) is an **absolute credit override** that bypasses
  `RefundPercent`. No RA2 *building* in this `rulesmd.ini` sets it (all 20
  `Soylent=` lines are on infantry/vehicles, for Yuri's Grinder), so for
  buildings the first line always applies — but the clone should still read the
  key rather than hard-code the Cost path.
* Both cost multipliers (`FUN_0050BC10` = country `HouseTypeClass+0x114…0x124`,
  `FUN_0050BCD0` = house runtime `+0x5390…0x53A0`) are `1.0` in a default
  skirmish; they are the `FactoryPlant`/`OrePurifier`/difficulty modifiers.
* `RefundPercent` is applied **only when the owning house is human-controlled**
  (`FUN_0050B550`). AI houses receive the *unscaled* value. This is in the
  binary; it is not an artefact of the decompiler (see the `TEST AL,AL / JZ`
  at `0x00711399`). **CONFIRMED**, and it is a real asymmetry to preserve.
* The `ignorePercent` flag is used by the *upgrade* refund path (§B4).

## B2 — What sell looks like: the buildup, **reversed** — **CONFIRMED**

`Sell_Back(control)` = `FUN_00447060` (vtbl **+0x1A0**), same `-1/0/1`
convention, and its whole job is to put the building into **mission `0x13`
(Selling)**:

```
00447063  MOV AL,[ESI + 0x6e9]             ; HasBuildUp
0044706b  JZ  0x00447115                   ;   -> the no-buildup path (below)
00447085  CALL dword [EAX + 0x184]         ; GetCurrentMission()
0044708b  CMP EAX,0x13                     ; already selling?
00447094  MOV AL,[ESI + 0x6df]             ; C4Applied
0044709c  JNZ 0x00447153                   ;   -> REFUSE: cannot sell a C4'd building
004470d1  PUSH 0x13
004470d5  CALL dword [EDX + 0x1e8]         ; QueueMission(Mission::Selling, false)
004470df  CALL dword [EAX + 0x1ec]         ; NextMission()
004470f4  ... CALL 0x0074f740              ; local player: play the sell click sound
```

`BuildingClass::Shape_Number` (`FUN_0043EF10`) then supplies the visual:

```c
if (this->BState == 0) {                                   // buildup state
    stage = this->AnimStage;                               // +0xF8
    if (Type->SAM) stage = (Type->[0xF08] + Type->[0xF04] - stage) - 1;
    if (GetCurrentMission() != 0x13) return stage;
    return (Type->[0xF08 + BState*0xC] + Type->[0xF04 + BState*0xC] - stage) - 1;
}
```

i.e. during mission `0x13` the frame index is **mirrored inside the BState-0
buildup range**: the buildup animation runs **backwards**, over the same frame
count, at the same rate that `building-render.md` §5 already pinned:

```
image_frames = buildup_shp_frames / 2
frame_period = max(1, trunc(Rules.BuildupTime × 900 / image_frames))  ticks
sell_duration = image_frames × frame_period                          ticks @15 Hz
```

With `BuildupTime=.06` → `0.06 × 900 = 54` ticks ≈ **3.6 s**, exactly as long as
the building took to appear. `Mission_Selling` state 1 spins on the
"animation still running" flag `bldg+0x418` and only then proceeds.

The state machine (`FUN_00449B70`, mission status at `bldg+0xBC`):

| status | what happens |
|:--:|---|
| entry | `this->Repair(0)` — selling always cancels repair (`0x00449B81`) |
| 0 | handle deployed-vehicle undeploy; **Service-Depot special case** (§B3); **upgrade peel-off** (§B4); clear the 8 `DamageFireAnims`; broadcast msg `0x17` to linked objects; status ← 1 |
| 1 | broadcast msg `3`; wait on `+0x418` (reverse buildup playing); **release garrison occupants**; **spawn survivors**; play the sell sound; status ← 2 |
| 2 | `EVA_StructureSold` (only when not undeploying); then either **undeploy into `UndeploysInto`** (§B6) or **`Credits += Refund_Amount()`**, return stored ore, `Limbo()`, `delete this` |

## B3 — Selling **does** return infantry — **CONFIRMED** (formula included)

Two separate mechanisms fire in state 1:

**(a) Garrison occupants are released.** If `Type->UnitAbsorb (+0x16AE)` or
`Type->SecretLab (+0x16AF)` and `this->OccupantCount (+0x114) > 0`, each
occupant is popped (`FUN_00473360`), unlimboed onto a foundation cell, and (for
non-local houses) given `QueueMission(0x0F /*Hunt*/)`.

**(b) Survivors.** `BuildingClass::Survivor_Count()` = `FUN_00451320`
(vtbl +0x2D0):

```c
if (this->NoCrew /*+0x6E0*/)          return 0;
if (!Type->Crewed /*type+0xCCD*/)     return 0;
switch (Owner->SideIndex /*+0x1E8*/) {
  case 0: div = Rules.AlliedSurvivorDivisor;  /* +0x14F8 = 500 */  break;
  case 1: div = Rules.SovietSurvivorDivisor;  /* +0x14FC = 250 */  break;
  case 2: div = Rules.ThirdSurvivorDivisor;   /* +0x1500 = 750 */  break;
  default: return 0;
}
if (div == 0) return 0;
if (this->HasBeenCaptured /*+0x6E3*/) div *= 2;
n = Type->GetRefund(Owner, false) / div;        // integer division of the REFUND
if (n < 1) return 1;
if (n > 5) return 5;
return n;
```

```
survivors = clamp( refund / SurvivorDivisor[side], 1, 5 )      // 0 if !Crewed or NoCrew
                                                              // divisor doubled if captured
```

**`SurvivorRate` is never read** — the divisors are the whole story, exactly as
the rules comment says. Worked examples with `RefundPercent=50%`:

| Building | Cost | refund | Allied (÷500) | Soviet (÷250) | Yuri (÷750) |
|---|---:|---:|---:|---:|---:|
| `GACNST` (`Crewed=yes`) | 3000 | 1500 | 3 | 5 (clamped from 6) | 2 |
| `GAPOWR` (`Crewed=no`) | 800 | 400 | **0** | 0 | 0 |
| `GAWEAP` (`Crewed=yes`) | 2000 | 1000 | 2 | 4 | 1 |

**Which infantry** — `BuildingClass::Crew_Type()` = `FUN_0044EB10`
(vtbl +0x30C):

```c
if (!this->HasBeenCaptured) {
    if (Random(0,99) < 25 && Type->[0xEB8] == 7)
        return Rules->[0xF70];        // a special InfantryType (the "engineer" slot)
}
return House_Default_Crew_Type();     // FUN_007071A0 — the side's basic rifle infantry
```

So a **25 % chance per survivor** of the special type for building purpose-class
`7`, otherwise the house's basic infantry (`E1`/`E2`/`INIT`). The identity of
`Rules+0xF70` is **PROBABLE** (engineer) — it was not string-anchored.

**Where they exit**: the foundation cell list (`vtbl+0x108`) is walked and each
survivor is unlimboed onto a cell of the building's own footprint, then given
`QueueMission(2)`; if the cell is blocked the unit is deleted rather than
displaced.

## B4 — Mid-production / primary factory / upgrades — mixed

* **Upgrades are peeled off one per sell click — CONFIRMED.** In state 0
  (`0x0044AAB8`):

  ```
  0044aab8  MOV AL,[EBP + 0x702]                 ; UpgradesCount
  0044aac9  MOV ECX,[EBP + EAX*4 + 0x5e8]        ; Upgrades[n]
  0044aad9  CALL dword [EDX + 0x8c]              ; upgType->CreateObject(Owner)
  0044aae5  CALL dword [EDX + 0x2bc]             ;   refund = tmp->Refund_Amount()
  0044aaf2  CALL 0x004f9770                      ;   Owner->Credits += refund
  0044aaff  CALL dword [EAX + 0x20]              ;   delete tmp
  0044ab06  CALL 0x00451680                      ; remove one upgrade from the building
  0044ab34  ... EVA_StructureSold
  0044ab45  CALL dword [EAX + 0x1e8]             ; QueueMission(5) -- back to Guard
  ```

  Selling a building that carries upgrades (power turbines etc.) **refunds and
  removes exactly one upgrade and stops** — the building itself survives. The
  player must click sell once per upgrade, then once more for the building.
  (`FUN_004575B0`, a different caller, does the same in a loop but passes
  `ignorePercent = 1`, i.e. **full cost** for upgrades — that path is the
  building-destroyed path, not the player sell.)

* **Service Depot with a docked unit — PROBABLE.** Also in state 0: if
  `Type->UnitReload (+0x16A9)` and the linked object answers message `0x13`
  and is within `0x80` leptons, the engine plays **`EVA_UnitSold`**, calls
  `Sell_Back(1)` on **that unit**, and returns the depot to Guard. That is the
  RA2 "drive a unit onto the depot and hit sell" behaviour, and it is mirrored
  by the network event (§C1). The exact identity of `vtbl+0x274` (radio-message
  query) was not isolated.

* **Mid-production / primary factory — [RE-FALLBACK].** `Mission_Selling` state
  0 calls `FUN_006B0430` when `this->Factory (+0x218)` is set and the factory's
  product class is `5`, and `FUN_00459390` when `bldg+0x2E4` is set. Neither
  was decompiled far enough to state what happens to a partially-built item or
  to the primary-factory flag. `FactoryClass::Abandon` (`FUN_004C9F10`, pinned
  in `construction.md`) refunds `Cost_Of − balance` when a factory dies, which
  is the most likely mechanism, but the **link from `Mission_Selling` to
  `Abandon` was not proven**. Ruled out: there is no explicit refund of
  production money inside `FUN_00449B70` itself.

## B5 — When is selling refused — **CONFIRMED**

`BuildingClass::Can_Be_Sold()` = `FUN_00449410`:

```c
if (FUN_0070F2C0(this))                    return false;   // this->[0x1D0] != 0
if (Type->Unsellable /*+0x1579*/)          return false;
if (this->HasBuildUp /*+0x6E9*/
 && this->BState /*+0x534*/ != 0                           // buildup finished
 && GetCurrentMission() != 0x13 /*Selling*/
 && GetCurrentMission() != 0x12 /*Construction*/) return true;
if (Type->FirestormWall /*+0x16C0*/ && !Owner->[0x1FA]) return true;
return false;
```

and `Sell_Back(1)` itself additionally refuses when `this->C4Applied (+0x6DF)`
is set.

So selling is refused for:

1. `Unsellable=yes` types (none in stock RA2 `rulesmd.ini` — verified);
2. buildings still **under construction** (`BState == 0`, or mission
   `0x12 Construction`) — the sell click is simply ignored;
3. buildings **already selling** (mission `0x13`);
4. buildings with **C4 planted** (`+0x6DF`);
5. objects with `+0x1D0` non-zero (**PROBABLE**: mind-controlled);
6. buildings **without a buildup** (`HasBuildUp == 0`) unless
   `Type->FirestormWall` — those take the instant-removal branch at
   `0x00447115` instead. In stock RA2 that is the wall/fence case.

**There is no "last Construction Yard" special case and no "captured building"
special case.** Both were searched for and neither exists: `HasBeenCaptured`
(`+0x6E3`) affects only the survivor divisor and the AI repair trigger, never
sellability. **CONFIRMED by absence.**

## B6 — The MCV case: `UndeploysInto` — **CONFIRMED (the rule), PROBABLE (the option global)**

`BuildingClass::Can_Undeploy()` = `FUN_0044F5C0` — the single cleanest statement
of the rule in the binary:

```c
if (this->[0x504] > 0) return false;               // occupied
if (this->vtbl[0x284]())  return true;             // simple deployer (TickTank/Artillary)
if (Type->ConstructionYard /*+0x16B9*/) {
    if (DAT_00a8a1a0 && DAT_00a8a288 && Is_Human(Owner)) goto ok;
    return false;
}
ok:
return Type->UndeploysInto /*+0x408*/ != 0;
```

and the identical gate is inlined twice inside `Mission_Selling`
(`0x00449C2A` and `0x0044A701`):

```
        Type->UndeploysInto == 0                       ->  plain refund
        Type->ConstructionYard == 0                    ->  UNDEPLOY
        DAT_00a8a1a0 == 0                              ->  plain refund
        this->Factory (+0x218) == 0                    ->  plain refund
        !Is_Human(Owner)                               ->  plain refund
        DAT_00a8a288 == 0                              ->  plain refund
        otherwise                                      ->  UNDEPLOY
```

**So, for THIS build:**

* A building whose type has `UndeploysInto=` and is **not** `ConstructionYard=yes`
  (the deployed TickTank / `Artillary` cases) **always** undeploys back into its
  unit — it never pays credits.
* A **Construction Yard** (`GACNST`: `ConstructionYard=yes`, `UndeploysInto=AMCV`)
  undeploys into an **AMCV** *only* when the session-mode global `0x00A8A1A0`
  is set, the owner is a human house, and the option global `0x00A8A288` is on.
  Otherwise it pays out `Refund_Amount()` like any other building.
* When the undeploy path is taken, **`EVA_StructureSold` is suppressed**
  (`0x00449C0B`: the EVA call is skipped when `Type->UndeploysInto != 0`), and
  the new unit's health is scaled from the building's: the MCV is created via
  `FUN_00734490(Type->UndeploysInto, Owner)`, its `Health`/`EstimatedHealth` are
  set from `ftol(HealthRatio × newType->Strength)` **clamped to ≥ 1**, and the
  building's veterancy block (`+0x4DC…+0x4F4`) is copied across. If placement
  fails, the engine falls back to `Credits += Refund_Amount()`.

`0x00A8A288` was **not** string-anchored. The `MCVRedeploys` key itself loads to
`Rules+0x14B8` in `FUN_006718E0`, so `0x00A8A288` is the *runtime* option copy —
it is written by the multiplayer lobby / game-options serialisation functions
(`0x5B4910`, `0x5B7D20`, `0x5DEA50`, `0x6AC570`, `0x5B6220`) and read only here
and in `Can_Undeploy`. **PROBABLE**: it is the "MCV Repacks/Redeploys" lobby
option, defaulting from `[MultiplayerDialogSettings] MCVRedeploys=yes`. Likewise
`0x00A8A1A0 != 0` reads as "multiplayer/skirmish session" (it is the branch in
`FUN_0050B510` that compares against `PlayerPtr` rather than testing the
per-house human flags). Neither was proven by string.

---

# C. Sidebar / input

## C1 — The two buttons are **mutually-exclusive cursor modes** — **CONFIRMED**

`DisplayClass` carries **five** adjacent one-byte mode flags:

| Offset | Mode | Setter |
|--------|------|--------|
| `+0x11B0` | **RepairMode** | `FUN_004AC7D0` |
| `+0x11B1` | **SellMode** | `FUN_004AC570` |
| `+0x11B2` | PowerToggleMode | `FUN_004AC730` |
| `+0x11B3` | PlanningMode (waypoints) | `FUN_004AC610` |
| `+0x11B4` | PlaceBeaconMode | `FUN_004AC870` |

(The earlier hunt's note "`SellMode` at `+0x11B1`" is right; its labelling of
`FUN_004AC870` as *SetRepairMode* was **wrong** — that one gates on a 3-slot
beacon array and is `SetPlaceBeaconMode`. `SetRepairMode` is `FUN_004AC7D0`.)

All five setters have the identical shape, and **every one clears the other
four**:

```c
void DisplayClass::SetSellMode(int control) {            // FUN_004AC570
    cur = SellMode;
    new = (control == -1) ? !cur : (control == 0 ? 0 : (control == 1 ? 1 : cur));
    if (new == cur) return;
    if (this->PendingObject /*+0x11A8*/ != 0) return;    // <-- placement wins
    this->vtbl[0x48](0, 0);                              // clear the pending-placement preview
    RepairMode = PowerToggleMode = PlanningMode = PlaceBeaconMode = 0;
    if (new && PlayerPtr->[0x2F0] > 0) {                 // must own at least one building
        SellMode = 1;
        Reset_Cursor();                                  // FUN_0048DBA0 — deselect all + arm cursor
        return;
    }
    SellMode = 0;
    this->vtbl[0x50]();                                  // restore the normal cursor
}
```

Three things follow directly:

* **Mutual exclusion is total** — the five modes, and the whole set is
  refused while a **building placement is pending** (`DisplayClass+0x11A8 != 0`),
  which is the exclusion with build-placement.
* Arming a mode calls `FUN_0048DBA0`, which **deselects everything currently
  selected** and then re-arms the cursor. So entering repair/sell mode clears
  the selection.
* Entering repair/sell/power mode requires `PlayerPtr->[0x2F0] > 0`
  (**PROBABLE**: the count of owned buildings) — with no buildings the buttons
  are dead. `FUN_006A6E10` uses the same predicate to grey the sidebar buttons.

**The sidebar buttons** (`SidebarClass::Action`, `FUN_006A6E10`):

```c
if (buttonID == 0x8066) { Sound(...); DisplayClass::SetSellMode(-1);   }   // SELL
if (buttonID == 0x8065) { Sound(...); DisplayClass::SetRepairMode(-1); }   // REPAIR
```

Both **toggle**. `construction.md` already pinned `FUN_006A4ED0` as the sidebar
art loader that reads `REPAIR`/`SELL` shapes plus `R-UP`/`R-DN`.

**Cancelling** — `FUN_004AAC40` (the right-click-up handler; **correction 2026-10 (fixB hunt):** ESC is the `[Hotkey] Options=27` command whose Execute `FUN_00646AE0` only queues the options event — it does not run this chain) cancels exactly
one thing per press, in this priority order:

```
pending placement  ->  RepairMode  ->  SellMode  ->  PowerToggleMode
                   ->  pending superweapon (+0x11B8)  ->  PlanningMode
                   ->  PlaceBeaconMode  ->  (nothing: just reset the cursor)
```

Several commands also hard-clear all five before acting — e.g. every unit-group
hotkey begins with the same five calls (`FUN_00730290/470/590`).

**The click → event path.** Both modes queue a network event rather than acting
locally; `EventClass::Execute` (`FUN_004C6BD0`) has adjacent cases:

```
; REPAIR event @004c6df2
    target = Resolve(ev.Target);  if (!target || !target->[0x90]) break;
    target->vtbl[0x19C](-1);                       ; Repair(TOGGLE)

; SELL event @004c6e32
    target = Resolve(ev.Target);  if (!target || !target->[0x90]) break;
    if (target->Owner->[0x30] != (signed char)ev.HouseIndex) break;   ; ownership check
    rtti = target->WhatAmI();
    if (rtti == 6 /*Building*/) goto sell;
    if (rtti != 1 && rtti != 2) break;                                ; else must be a unit
    cell = Map[target->Coords];
    if (!FUN_0047C430(cell)) break;                ; ...standing on a BUILDING (depot/grinder)
sell:
    target->vtbl[0x1A0](-1);                       ; Sell_Back(TOGGLE)
```

So the SELL event is authoritative about ownership (**you cannot sell someone
else's building even if the cursor lets you click it**) and it is the same event
that sells *units* — but only when the unit is standing on a building.

## C2 — Cursor art — **[RE-FALLBACK]**

`FUN_0048DBA0` only *resets* the cursor (`FUN_00730DD0(0)` +
`FUN_00430FE0(0x89B328)`); the repair/sell cursor frames come from the mouse
cursor descriptor table found at `0x0082C038` (86 entries of
`{startFrame, count, rate, miniStart, miniCount, hotX, hotY}`, dumped in
`rs10_log.txt`), but the **specific indices for the wrench and the dollar-sign
cursors were not isolated** — the selection function that maps
`RepairMode`/`SellMode` → cursor index was not found. Ruled out:
`FUN_0048DBA0`, `FUN_00730DD0`, `FUN_00430FE0`, `FUN_004AC220` (that one is the
drag-select guard, which simply refuses to start a rubber-band while any mode
is armed). The art itself is `MOUSE.SHP`; the clone can pick frames empirically.

---

## Confidence summary

| # | Finding | Confidence |
|---|---------|:----------:|
| A1 | rules offsets `RepairStep +0x16CC`, `RepairPercent +0x16D0`, `RepairRate +0x16E0`, `RefundPercent +0x1738`; values 8 / 0.15 / .016 / 0.50 | **CONFIRMED** |
| A1 | `URepairStep` / `URepairPercent` do not exist; `SurvivorRate` is dead | **CONFIRMED** |
| A2 | cadence `Frame % trunc(RepairRate×900) == 0` → **every 14 frames**, global phase | **CONFIRMED** |
| A2 | `+8 HP` per step; `Health` and `EstimatedHealth` both, clamped to `Strength` | **CONFIRMED** |
| A2 | `stepCost = max(1, trunc((Cost / (Strength/RepairStep)) × RepairPercent))`, both divisions integer | **CONFIRMED** |
| A3 | out of money ⇒ repair flag cleared, no partial step, no EVA | **CONFIRMED** |
| A4 | per-building toggle via `vtbl+0x19C`, network event carries `-1` | **CONFIRMED** |
| A4 | auto-stops at full health; `ClickRepairable` (`type+0x157A`) gates the wrench | **CONFIRMED** |
| A5 | `IsDamaged = HealthRatio <= ConditionYellow`; anim tables `type+0xF4C` / `+0xF5C`, 21 slots; smoke anim destroyed above the threshold | **CONFIRMED** |
| A6 | engineer = `vtbl+0x40C`, instant full heal, free, separate path | **CONFIRMED** |
| A7 | AI auto-repair / auto-sell-below-`ConditionRed` reuses the same two vfns | **PROBABLE** |
| B1 | `refund = trunc(trunc(Cost×m2×m1) × RefundPercent)`; **no HP scaling**; `Soylent` overrides and bypasses `RefundPercent` | **CONFIRMED** |
| B1 | `RefundPercent` applied only to human-controlled houses | **CONFIRMED** |
| B2 | sell = mission `0x13`; buildup range played **mirrored**; duration = buildup duration (54 ticks ≈ 3.6 s) | **CONFIRMED** |
| B3 | `survivors = clamp(refund / SurvivorDivisor[side], 1, 5)`, 0 when `!Crewed`/`NoCrew`, divisor ×2 if captured | **CONFIRMED** |
| B3 | crew type = house basic infantry, 25 % special for purpose-class 7 | **PROBABLE** |
| B4 | one **upgrade** refunded+removed per sell click before the building is sold | **CONFIRMED** |
| B4 | Service-Depot: docked unit sold instead of the depot | **PROBABLE** |
| B4 | fate of in-progress production / primary-factory flag | **[RE-FALLBACK]** |
| B5 | refusals: `Unsellable`, under construction (`BState==0`), already selling, C4 applied, no-buildup (non-wall); **no last-ConYard rule, no captured rule** | **CONFIRMED** |
| B6 | `UndeploysInto` always undeploys, **except** `ConstructionYard=yes`, which needs MP-session + human owner + the MCV-redeploy option | **CONFIRMED** (rule) |
| B6 | `0x00A8A288` = the MCV-redeploy lobby option; `0x00A8A1A0` = multiplayer session | **PROBABLE** |
| B6 | undeployed MCV health = `max(1, ftol(HealthRatio × MCV.Strength))`; veterancy carried | **PROBABLE** |
| C1 | five mutually-exclusive mode flags `+0x11B0…+0x11B4`; placement wins; arming clears the selection; needs ≥1 owned building | **CONFIRMED** |
| C1 | sidebar ids `0x8065` repair / `0x8066` sell, both toggle; cancel priority chain | **CONFIRMED** |
| C1 | SELL event verifies ownership; unit-sell requires standing on a building | **CONFIRMED** |
| C2 | wrench / dollar cursor frame indices | **[RE-FALLBACK]** |

---

## Deviations we will take

1. **Implement repair as a per-building `bool IsBeingRepaired` + a global frame
   phase**, not a per-building timer. `interval = trunc(RepairRate × 900)` must
   be computed once with truncation (= 14 at `.016`), and the tick must be
   `frame % interval == 0` so all repairing buildings step together. Cheap and
   deterministic; matches the original exactly.

2. **Compute `stepCost` with integer division, in this order**, and cache it per
   `BuildingType` (it only depends on `Cost`, `Strength`, and two rules values):

   ```cpp
   int step_cost(const BuildingType& t) {
       int per = t.strength / rules.repair_step;          // integer
       int n   = per ? t.cost / per : t.cost;             // integer
       int c   = (int)((double)n * rules.repair_percent); // truncating
       return c > 1 ? c : 1;
   }
   ```

   Do **not** use `cost * repair_percent * damage_fraction`. Prior hunts have
   twice been bitten by re-deriving "obvious" formulas; this one is off by
   ~17 % on a Construction Yard.

3. **Refund is flat**: `credits += trunc(cost * refund_percent)`. Do not scale
   by health. Read `Soylent` and let it override (bypassing `RefundPercent`)
   even though no stock building uses it. Skip the human-vs-AI asymmetry only if
   the clone has no AI houses yet; when AI arrives, an AI house must get the
   *unscaled* value.

4. **Sell = a mission, not an instant action.** Reuse the existing
   `BuildupTime`/buildup-frame machinery from `construction.md`: set the
   building to buildup state, run the same frame count at the same period, but
   index the frames mirrored (`start + count - stage - 1`). Only when the
   animation completes do survivors spawn and the credits land. Selling must
   first call `Repair(0)`.

5. **Survivors**: implement the divisor formula. It needs `Crewed=` on the
   building type, the owner's side index, and the already-computed refund. Cap
   at 5, floor at 1, zero when `Crewed=no`. Skip the 25 %-engineer special until
   `Rules+0xF70` is string-anchored; using the basic infantry always is a
   defensible first cut.

6. **Refusals**: gate the sell click on `Unsellable`, `BState != 0`, and
   "not already selling". Do **not** add a last-Construction-Yard guard or a
   captured-building guard — the original has neither, and adding one would be
   exactly the kind of invented behaviour this hunt exists to prevent.

7. **`UndeploysInto` for the Construction Yard**: this build undeploys the
   ConYard into an `AMCV` **only** under the MCV-redeploy option. Expose it as a
   settings flag defaulting to `MCVRedeploys=yes` from
   `[MultiplayerDialogSettings]`, so both behaviours are reachable, and suppress
   the `EVA_StructureSold` line on the undeploy path. Deployed-vehicle types
   (`TickTank`/`Artillary`) always undeploy.

8. **Upgrades**: if/when upgrade buildings (power turbines) exist in the clone,
   sell must peel exactly one upgrade per click. Until then this is a no-op.

9. **Modes**: model the five flags as one enum `InputMode { None, Repair, Sell,
   Power, Planning, Beacon }` plus the independent "pending placement" state.
   Setting any mode must (a) fail while a placement is pending, (b) clear the
   current selection, (c) be refused when the player owns no buildings.
   ESC/right-click cancels in the documented priority order. Both sidebar
   buttons toggle.

10. **Cursor frames** are unresolved; pick the wrench/dollar frames from
    `MOUSE.SHP` empirically and record the chosen indices here when they are
    confirmed, so the `[RE-FALLBACK]` in §C2 can be closed.

---

# §D Adjudication (RE-GATE)

*This section closes the nine items the hunt left open before the Repair & Sell
implementation starts. Everything marked **CONFIRMED** in §A1–A6, §B1–B3, §B5,
§C1 is settled elsewhere and was NOT re-opened here — except where §D9's
measurement of **this install** contradicts a worked example printed above; those
contradictions are called out in §D0 and the numbers in §D9 win.*

Ground-truth INI for every measurement below (the command that actually worked —
`rulesmd.ini` is **not** a top-level entry of `ra2md.mix`):

```powershell
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw "localmd.mix/rulesmd.ini" "$SC\rulesmd.ini"
# -> wrote ...\rulesmd.ini (742958 bytes)
```

Cursor art (item 8) — `MOUSE.SHP` is **not** in `ra2md.mix` at all; two copies
exist on this install and both are byte-compatible in geometry:

```powershell
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2.mix"      --raw "conquer.mix/mouse.shp" "$SC\mouse.shp"        # 359800 bytes
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ecache01.mix" --raw "mouse.shp"             "$SC\mouse_ecache.shp" # 361912 bytes
```

Roster under test = `engine/main.cpp:1535` `kRoster[24]` **verbatim**
(`GACNST GAPOWR GAPILE GAWEAP NACNST NAPOWR NAHAND NAWEAP GAREFN NAREFN GAPILL
GATOW NALASR NABNKR TESLA NAFLAK NAWALL NARADR GATECH CAOILD NANRCT ATESLA
NASAM GAAIRC`). Note this is **`TESLA`** at index 14 and **`ATESLA`** at index
21 — two different types, both present.

---

## §D0 — CONTRADICTIONS with the text above. Read this first.

Four statements printed earlier in this document are **wrong on this install**.
They are corrected here, not quietly harmonised; the superseded text is left in
place so the diff is visible.

1. **§A2's and §B3's `GAPOWR` rows are WRONG.** The real `[GAPOWR]`
   (`rulesmd.ini` line 11649) is **`Strength=750`**, not 1000, and
   **`Crewed=yes`**, not `Crewed=no`:

   ```ini
   [GAPOWR]
   Strength=750
   Cost=800
   Crewed=yes
   ```

   Consequences: `Strength/8 = 93` (not 125), `Cost/that = 8` (not 6),
   `×0.15 = 1.20` (not 0.90). **`stepCost` is still 1** — the two errors happen
   to cancel under the `max(1,…)` clamp — but **total-to-full is 94 steps, not
   125**, and **`GAPOWR` yields 1 Allied / 1 Soviet survivor, not 0**. §B3's
   "`GAPOWR` (`Crewed=no`) → **0**" row is the single most misleading line in
   this document. `NAPOWR` is likewise `Strength=750 / Crewed=yes`.

2. **§B1's `Soylent` claim is WRONG.** It says "No RA2 *building* in this
   `rulesmd.ini` sets it (all 20 `Soylent=` lines are on infantry/vehicles)".
   There are **135** `Soylent=` lines, and **`GAREFN` and `NAREFN` — both in
   `kRoster` — set `Soylent=300`** (lines 11735 / 12529). So a refinery refunds
   **300**, not `trunc(2000 × 0.50) = 1000`, and `RefundPercent` is **not**
   applied to it. The rest of §B1's formula is unaffected and correct; only the
   "no building uses it" aside is false. **Deviation #3's instruction to read
   `Soylent` and let it override is therefore load-bearing, not defensive
   coding** — skipping it makes the two refineries pay 3.3× too much.
   `Survivor_Count` divides that same `GetRefund` value, so the refineries also
   drop 1/1 survivors, not 2/4.

3. **§B5's "`Unsellable=yes` … none in stock RA2 `rulesmd.ini` — verified" is
   WRONG.** Eleven sections set it (`SMON SMIN YAREFN CAOUTP CAAIRP CAOILD
   CAPOWR CAMACH CATHOSP CAHOSP CASLAB`), and one of them — **`CAOILD`** — is
   `kRoster` index 19. The `Unsellable` refusal is therefore **reachable in the
   clone today**, not dormant. §B5's *mechanism* is unchanged and CONFIRMED;
   only its "no stock type uses it" aside is false.

4. **`GATOW` has no `[GATOW]` section anywhere in `rulesmd.ini`** (zero
   occurrences of the string in the whole file). `Rules::cost`/`Rules::strength`
   return the `0` default, so the clone's roster slot 11 is `Cost=0,
   Strength=0`. This makes the `per == 0` guard in Deviation #2
   (`int n = per ? t.cost / per : t.cost;`) **load-bearing**: the original's
   `IDIV ESI` at `0x00711497` would raise `#DE` on a `Strength=0` type, which is
   unreachable in the original because no such type exists. The guard is a
   *deviation*, and a necessary one. Recorded, not hidden.

Nothing in §A1–A6/§B1–B2/§B5–§B6/§C1's **code-derived** findings changed. Every
correction above is to a *worked example* or an *aside about the data*, not to a
recovered algorithm.

---

## §D1 — Survivor crew type (§B3, PROBABLE) → **CUT this phase. PINNED ids `E1`/`E2`.**

**Adjudication: the 25 % special-crew branch is CUT.** Every survivor is the
owning house's basic infantry.

Pinned from `[General]` of the extracted `rulesmd.ini`, **verbatim**:

```ini
AlliedCrew=E1
SovietCrew=E2
ThirdCrew=INIT
Technician=CTECH
Engineer=ENGINEER
```

So `House_Default_Crew_Type()` (`FUN_007071A0`, §B3) resolves to **`E1` for
Allied, `E2` for Soviet** — the exact pair `engine/main.cpp:9608-9609` already
uses to pick the starting infantry:

```cpp
resolve_combat_unit(rules, s.side == ra2engine::World::Side::Soviet ? "E2" : "E1")
```

**Why the 25 % branch is cut, with the evidence that it cannot be pinned.**
`Rules+0xF70` is still **not string-anchored**, and the one attempt allowed here
confirmed *why*: the only write to `+0xF70` reachable from the rules loaders is
in `FUN_00665090` — the `RulesClass` **constructor's zero/default-init run**, not
an INI key handler (`rs13_keymap_rules_665090.txt`):

```
`(Q  -> +0xf6c  (00666538  MOV dword ptr [ESI + 0xf6c],EBX)
`(Q  -> +0xf70  (0066653e  MOV dword ptr [ESI + 0xf70],EBX)   <-- ctor default, no key string
`(Q  -> +0xf74  (00666544  MOV dword ptr [ESI + 0xf74],EBX)
```

It sits in an unbroken run of ~40 identical `MOV [ESI+n],EBX` stores with no
intervening string push, so there is no key name to anchor it to. **Not
determined.** Ruled out as the anchor: `AlliedCrew`/`SovietCrew`/`ThirdCrew`
(those feed `House_Default_Crew_Type`, the *other* return path) and
`Technician`/`Engineer` (present as `[General]` keys but not shown to store at
`+0xF70`).

**Second, independent reason to cut it:** the branch is
`if (Random(0,99) < 25 && Type->[0xEB8] == 7)` — an **RNG draw per survivor**.
The clone's demo gates hash the world; an un-seeded 25 % draw inside the sell
path would make `--demo-*` world hashes non-reproducible. The determinism cost
is not worth an unanchored InfantryType. `Type->[0xEB8] == 7` (building purpose
class 7) is also unanchored, so we could not even tell which rostered types
would qualify.

**What the implementation does:** `survivor_type(house) = side == Soviet ? "E2"
: "E1"`, unconditionally, for all `survivor_count` survivors. A future phase that
string-anchors `Rules+0xF70` **and** `Type+0xEB8` may add the branch — and must
route the draw through the lockstep RNG before it does.

---

## §D2 — Service-Depot / `UnitReload` case (§B4, PROBABLE) → **OUT of scope — but NOT for the expected reason.**

**The brief's expectation was wrong.** It expected *no* `kRoster` id to set
`UnitReload=yes`. Measured against the real `rulesmd.ini`, exactly **two**
sections in the entire file set it, and **one of them is in the roster**:

```
=== global scan: which sections in rulesmd.ini set UnitReload=yes ===
   GAAIRC UnitReload= yes
   AMRADR UnitReload= yes
```

`[GAAIRC]` (`rulesmd.ini` line 11802) — the Allied Airforce Command HQ, `kRoster`
index **23**:

```ini
[GAAIRC]
UnitReload=yes
Helipad=yes
Factory=AircraftType
NumberOfDocks=4
```

**Adjudicated OUT of scope anyway**, on a *reachability* argument rather than a
data argument. §B4's special case fires only when **all** of these hold: the
depot's `UnitReload` is set, **and** the building has a live radio link whose
partner answers message `0x13`, **and** that partner is within `0x80` leptons.
The clone has:

* no aircraft and no `AircraftType` production (`GAAIRC`'s `Factory=AircraftType`
  is inert — the clone's factory categories are Structure/Defense/Infantry/Vehicle
  only);
* no docking, no `NumberOfDocks` handling, and no radio-contact/link mechanism at
  all for buildings.

With no dockable object in existence the guard can never be true, so
`begin_sell(GAAIRC)` reduces to the ordinary building sell. **`vtbl+0x274` (the
radio-message query) remains unidentified — not determined**, and it does not
need to be for this phase.

**Recorded for the phase that adds aircraft:** the moment a helicopter can land
on `GAAIRC`, selling `GAAIRC` with a unit docked must sell **the unit**
(`EVA_UnitSold`), not the building, and return the building to Guard. That is a
behaviour change to `begin_sell`, and this note is the trigger for it.

---

## §D3 — AI reuse of `Repair(1)` / `Sell_Back(1)` (§A7, PROBABLE) → **OUT of scope.**

**Adjudicated OUT of scope.** The clone's AI (`ai_base` / `ai_team` /
`ai_trigger`) will neither auto-repair nor auto-sell in this phase.

**The blocker list, explicitly** — every term in §A7's recovered code that is
**not** content-anchored and therefore cannot be implemented without inventing a
value:

| Term | What it gates | Status |
|------|---------------|--------|
| `Rules->[0x1444]` | the house-IQ threshold above which the AI repairs at all | **unanchored** (no INI-key string reached it) |
| `Rules->[0x1758]` | the credit floor: above it the AI repairs, below it the AI sells | **unanchored** |
| `Rules->[0x145C]` | the second threshold compared against `Owner->[0x1D0]` in the sell branch | **unanchored** |
| `Owner->[0x245]` | the "one repair-start per house per pass" latch | **unanchored** |
| `Owner->[0x1D0]` | house-level counter compared against `Rules->[0x145C]` (**note:** this is a *HouseClass* offset and is NOT the same field as the `TechnoClass+0x1D0` of §D6 — do not conflate them) | **unanchored** |
| `Owner->[0x1D4]` | the per-house probability weight the `Random(0,50)` draw is compared against | **unanchored** |
| `this->[0x3D1]` | per-building AI-sell eligibility flag | **unanchored** |
| `Type->[0xEB8]` | building purpose class (the `!= 7` exclusion) | **unanchored** — the same field §D1 could not anchor |

Plus the hard blocker: **`Random(0,50) < Owner->[0x1D4]`** is a lockstep RNG draw
executed **once per AI building per repair pass**. Wiring it in would perturb the
RNG stream for every subsequent consumer and break the byte-stability of
`--demo-ai-base`, `--demo-ai-team` and `--demo-ai-trigger` — three gates that are
green today. That alone is disqualifying regardless of the offsets.

**What a future hunt must string-anchor first**, in order: (1) the three
`Rules+0x1444 / 0x145C / 0x1758` fields, from `[General]`/`[AI]` INI-key string
refs inside `FUN_0066CF70` / `FUN_006718E0` — the same technique §A1 used; (2)
`Type+0xEB8` from `BuildingTypeClass::LoadFromINI` (`FUN_0045FE40`)'s key→store
map, which already yielded `Unsellable`/`ClickRepairable`/`UnitReload`; (3) the
`HouseClass` offsets `+0x1D0/+0x1D4/+0x245` from whichever `[AI]`/difficulty
loader writes them. Only then is the AI branch implementable — and even then it
needs a deterministic RNG plan.

The two vfns themselves (`vtbl+0x19C` `Repair`, `vtbl+0x1A0` `Sell_Back`) are
CONFIRMED and are exactly what the player path uses; the clone's
`toggle_repair` / `begin_sell` are the same entry points, so a later AI phase
plugs in without re-plumbing.

---

## §D4 — Mid-production / primary-factory fate on sell (§B4) → **FALLBACK adopted. `[RE-FALLBACK]` STANDS.**

The link from `Mission_Selling` to `FactoryClass::Abandon` was **not proven** and
this task did not prove it. The `[RE-FALLBACK]` tag in §B4 and in the confidence
summary **remains**. What changes is that the fallback behaviour is now *chosen
and justified* instead of open.

**Adjudicated behaviour: a sold building takes the clone's existing
building-death path. Selling a factory behaves exactly as destroying one.**

**Why this is the safe choice.** It is the one thing certainly true of the
original: whichever route `Mission_Selling` takes, the building is `Limbo()`'d and
`delete this`'d in state 2 (§B2), which runs `~BuildingClass` → `~FactoryClass`.
`construction.md` §10 pins that destructor (`FUN_004C9930`) as *abandon **with
refund***, and pins `FactoryClass::Abandon` (`FUN_004C9F10`) as refunding
`Cost_Of − balance`, i.e. **everything spent so far, in one lump**. So a sold
factory's in-progress item is cancelled and fully refunded *at the latest* when
the building is deleted. The only thing we cannot state is whether it happens at
sell **start** (via `FUN_006B0430` in state 0) or at sell **completion** (via the
destructor) — a difference of ~54 ticks.

**What the clone does today when a building with a live queue and/or
`primary == true` dies** — measured, not assumed:

* `kill_building` (`engine/building.cpp:97-102`) sets `hp = 0`,
  `state = Dead`, `die_time = 0`. `alive` stays true until the death anim ends;
  the building is never erased (tombstone).
* `conyard_alive` (`engine/main.cpp:2425-2432`) requires
  `b.alive && b.state == State::Idle`. A `Dead` (or `Buildup`) ConYard fails it.
* `engine/main.cpp:2608-2619`: when `!conyard_alive`, **both** building queues
  cancel through `ProductionQueues::cancel` — Structure first, then Defense, in
  that order so the two refunds accumulate in the shared pool.
* `World::factory_category_lost` (`engine/world.cpp:917-924`) uses the same
  `alive && state == Idle` test per category; `engine/main.cpp:2621-2624` cancels
  the Infantry / Vehicle queue when its category is lost.
* `ra2engine::cancel` (`engine/production.cpp:133`) is
  `ps.credits += ps.slot.spent; ps.slot = QueueSlot{};` — **a full refund of
  everything spent so far**, tagged `[RE: construction.md §10]`. That is
  literally `Cost_Of − balance`.
* `primary_factory` (`engine/factory.cpp:55-66`) skips any building that is not
  `alive && Idle`, so a dead/selling factory stops being selectable as primary
  **automatically** and the next eligible factory becomes primary by fallback.
  The stale `b.primary == true` left on the corpse is harmless: it is only ever
  read behind the same `alive && Idle` filter, and `set_primary` clears every
  sibling when a new primary is chosen.

**Therefore: `begin_sell` / `complete_sell` must add NOTHING.** The queue-cancel
and primary-fallback both key off `state != Idle`, and the sell mission puts the
building into a non-`Idle` state at `begin_sell`. This has a pleasant side
effect: the refund lands at **sell start**, which is the *earlier* of the two
possible original timings, matching the `FUN_006B0430`-in-state-0 reading rather
than the destructor reading.

**Two consequences the implementation must accept knowingly:**

1. If a sell is ever made cancellable, the cancelled item is **not** restored —
   it was already refunded. The original has no sell-cancel either
   (`Sell_Back` refuses when the mission is already `0x13`, §B5 refusal 3), so
   this is consistent.
2. Selling the **last** ConYard cancels *and refunds* the in-progress building,
   then also pays the ConYard refund. Both payouts are correct per the recovered
   code; they are simply not additive-double-counting.

`[RE-FALLBACK: Mission_Selling → FactoryClass::Abandon link unproven; the clone
routes sell through its existing building-death queue-cancel path]`

---

## §D5 — `PlayerPtr->[0x2F0]` (§C1, PROBABLE) → **PINNED: owned-building count. Upgraded PROBABLE → CONFIRMED.**

The one permitted attempt **succeeded**, from the existing listings, by content.
Three independent uses agree.

**(a) It is one of four adjacent per-category owned-object counters.** The
sidebar's redraw/dirty check sums four adjacent ints on `HouseClass`
(`sb2_s_6A81C0_6A81C0.c:285`, `sb_writer_6A49A0.c:148`, identically):

```text
house->[0x2F8] + house->[0x2F4] + house->[0x2F0] + house->[0x2E8]    // four int fields, summed
```

Four counters at `+0x2E8 / +0x2F0 / +0x2F4 / +0x2F8` — the classic Westwood
`CurBuildings / CurUnits / CurInfantry / CurAircraft` block.

**(b) The defeat test proves `+0x2F0` is the *building* one.**
`FUN_004F8350` (the house per-frame update, already referenced in §A3 as an
`EVA_InsufficientFunds` site), `rs9_nofunds_004f8350.c:442-462`:

```text
total = house->[0x2F0]                          // buildings
      + FUN_0049fa70() + FUN_0049fa70() + FUN_0049fa70()   // three OTHER category counts (three calls)
...
if total == 0: FUN_004fc4f0(); FUN_004fbed0()   // owns nothing at all -> defeat
```

`+0x2F0` is summed with **three** separately-computed category counts to answer
"does this house own anything?". It is therefore exactly one category — and it
is the one the *building*-only mode gates use.

**(c) The 1500-credit free-MCV rule nails it.** `combat3_fp8_481910.c:147-150`:

The condition requires all four of the following:
1. `house->[0x2F0] == 0` (no buildings);
2. the value returned by the virtual call at vtable offset `+0x18` of the
   object stored at `house+0x24` is greater than `0x5DC` (credits);
3. `FUN_0049f9f0()` returns 0 (no MCV);
4. the global byte at `0xA8A1C0` is nonzero.

`0x5DC == 1500`. This is RA2's well-known **"no buildings + ≥ 1500 credits + no
MCV ⇒ deliver a free MCV"** rule, and its "no buildings" term is
`house->[0x2F0] == 0`. A count that means "buildings" is the only reading under
which that constant makes sense.

**(d) Corroborating use, same function** (`rs9_nofunds_004f8350.c:315-319`):

If the house being updated is the global at `0xA82CB4` (the local player),
`house->[0x2F0] == 0`, and the global byte at `0x87F919` is nonzero, the
function calls `FUN_004ac570(0)` (SetSellMode(OFF)).

Losing your last building **force-disarms sell mode**. Exactly the inverse of
the `> 0` arming gate in §C1.

**Pinned:** `HouseClass+0x2F0` = **count of buildings this house owns**.
`DAT_00a82cb4` = `PlayerPtr`. The arming predicate in `SetRepairMode` /
`SetSellMode` / `SetPowerToggleMode` (`rs3_settogglepower_004ac7d0.c:27`,
`rs6_setsellmode2_004ac570.c:28`, `rs6_mode_4ac730_004ac730.c:28`) and the
sidebar button-grey predicate (`rs5_sb_6a6e10_006a6e10.c:41`) are all literally
"the int at `PlayerPtr + 0x2F0` is greater than 0".

**Implementation:** "the player owns ≥ 1 building" — counting **alive, non-`Dead`**
buildings, which is what the clone's `Building` tombstone model makes available.
The `[RE-FALLBACK: PlayerPtr+0x2F0 identity]` tag the brief pre-authorised is
**NOT needed and must not be emitted**; the predicate is confirmed.

*(One residual, recorded for honesty: the original counts a building the instant
it is created, including one still in `BState == 0` buildup. The clone's
"alive && !Dead" predicate includes `Buildup` buildings too, so the two agree.
A building in the **selling** mission is still owned and still counted in the
original — the clone must NOT exclude it, or arming would break on the last
building mid-sell.)*

---

## §D6 — Mind-control sell refusal (`+0x1D0`, §B5 refusal 5, PROBABLE) → **OUT of scope.**

**Adjudicated OUT of scope.** The clone has no mind-control mechanic — no
Yuri, no `Psychic` weapon, no `ImmuneToPsionics` handling, no controller/controllee
link of any kind. A refusal term reading a field that can never be non-zero is
dead code, and dead code that encodes an *unproven* identity is worse than none.

The accessor's shape is CONFIRMED (`rs13_cansell_veto_70f2c0_0070f2c0.c`, the
whole 14-byte function):

`FUN_0070f2c0` (fastcall, takes the object in ECX) returns true exactly when
the int/pointer field at `this+0x1D0` is non-zero.

A pointer-shaped field on `TechnoClass` whose non-null state vetoes selling. The
**mind-controller-link** reading is the standard one and remains **PROBABLE**;
it was **not** string-anchored and this task did not attempt to anchor it.

**Recorded so the mind-control phase knows to come back:** `can_be_sold()`
**deliberately omits** this term. When mind control is added, the *first* thing it
must do is restore the refusal — a mind-controlled building must not be sellable
by its nominal owner. Grep for this subsection.

> **Do not confuse this with §D3's `Owner->[0x1D0]`.** That one is a
> `HouseClass` offset in the AI sell branch; this one is a `TechnoClass` offset.
> Same number, different object, unrelated fields.

---

## §D7 — `UndeploysInto` option globals (§B6, PROBABLE) → **OUT of scope. The clone takes the plain-refund branch, and that is NOT a deviation.**

**Adjudicated OUT of scope.** `0x00A8A288` (the MCV-redeploy lobby option) and
`0x00A8A1A0` (the multiplayer-session flag) were **not** string-anchored, and
this task did not attempt to anchor them. Both stay **PROBABLE**.

They do not need to be anchored, because **the clone takes the plain-refund
branch, and the plain-refund branch is a branch the recovered code itself
takes.** From §B6's inlined gate, the original pays `Refund_Amount()` — no
undeploy — whenever *any* of these holds:

```
Type->UndeploysInto == 0        ->  plain refund
DAT_00a8a1a0 == 0               ->  plain refund      (not a multiplayer session)
this->Factory (+0x218) == 0     ->  plain refund
!Is_Human(Owner)                ->  plain refund
DAT_00a8a288 == 0               ->  plain refund      (MCV-redeploy option off)
```

That is **five** independent ways to reach plain refund, and the clone satisfies
at least two of them unconditionally: it has no multiplayer session
(`DAT_00A8A1A0 == 0`) and no MCV-redeploy lobby option (`DAT_00A8A288 == 0`).
So selling a `GACNST` for `trunc(3000 × 0.50) = 1500` credits is **a reachable,
correct configuration of the original binary** — a single-player/option-off
game — not an approximation of it. This is the §Scope justification, stated in
full so nobody later "fixes" it.

Two further facts make the cut cheap:

* The clone has **no unit type `AMCV`** and no deploy/undeploy mechanic for
  vehicles at all, so there is nothing for `GACNST` to undeploy *into*.
* No other rostered id sets `UndeploysInto=`; `GACNST`/`NACNST` are the only
  `ConstructionYard=yes` types, and `UndeploysInto=AMCV` appears only there
  (`rulesmd.ini` line 11625).

Deviation #7's "expose it as a settings flag defaulting to `MCVRedeploys=yes`"
is **deferred with the mechanic**, not implemented now — there is no second
behaviour to switch to. When MCV deploy/undeploy arrives, that phase implements
both branches and the flag, and must suppress `EVA_StructureSold` on the
undeploy path (§B6).

---

## §D8 — Wrench / dollar cursor frames (§C2) → **PINNED. The art is identified; §C2's *selection function* stays open.**

`MOUSE.SHP` **is not in `ra2md.mix`**. Two copies exist on this install
(`ra2.mix/conquer.mix/mouse.shp`, 359 800 bytes; `ecache01.mix/mouse.shp`,
361 912 bytes). Both have the **identical geometry**:

```
header: zero=0  width=55  height=43  frames=450
```

**450 frames**, canvas **55 × 43**, per-frame sub-rects varying from 2×11 to
54×26. Frames were decoded with the repo's own `ShpFile` algorithm
(`src/shp.cpp`) reimplemented in a scratch script and rendered as labelled
silhouette contact sheets. **Both a wrench and a dollar sign are unmistakably
identifiable.**

| Frames | Content | Count |
|--------|---------|------:|
| **129–138** | **dollar sign `$`** with four targeting brackets closing in around it | 10 |
| **139–148** | plain **`$`**, spinning (edge-on at 143–144) | 10 |
| **149** | **`$` inside a crossed-out circle** — the *cannot-sell* cursor | 1 |
| **150–169** | **wrench** with four targeting brackets closing in around it | 20 |
| **170–189** | plain **wrench**, rotating | 20 |
| **190** | **wrench inside a crossed-out circle** — the *cannot-repair* cursor | 1 |

**These boundaries are independently corroborated by the binary.** The cursor
descriptor table at `0x0082C038` (86 entries of
`{startFrame, count, rate, miniStart, miniCount, hotX, hotY}`, dumped in
`rs10_log.txt`) has six consecutive entries whose `frame`/`count` pairs land
**exactly** on the six visual groups above:

```
[30] frame=129  count=10  int=4      <- $ + brackets
[31] frame=139  count=10  int=4      <- $ spinning
[32] frame=149  count=1   int=0      <- no-$
[33] frame=150  count=20  int=4      <- wrench + brackets
[34] frame=170  count=20  int=4      <- wrench spinning
[35] frame=190  count=1   int=0      <- no-wrench
```

Six boundary matches by content is not coincidence: the table at `0x0082C038` is
the real cursor descriptor table, and animation rate **4** (i.e. one cursor frame
every 4 game ticks) is pinned for all four animated variants.

**What is still NOT determined.** The function mapping
`RepairMode`/`SellMode` → *cursor index* was not found (§C2's original open
question; ruled out again: `FUN_0048DBA0`, `FUN_00730DD0`, `FUN_00430FE0`,
`FUN_004AC220`). So **which member of each pair is the "armed, not over a valid
target" cursor and which is the "over a valid target" cursor is PROBABLE, not
confirmed.** The reading adopted — brackets = over a valid target, plain spin =
armed — follows from the brackets being RA2's universal "this action applies to
the thing under the cursor" ornament, and from the fact that each group has
exactly one crossed-out refusal frame, so the two animated members must be two
*affirmative* states.

**Adjudicated for the implementation:**

* **Sell cursor** — armed: frames **139–148** @ 4 ticks/frame; over a sellable
  own building: **129–138** @ 4; refused: **149**.
* **Repair cursor** — armed: frames **170–189** @ 4 ticks/frame; over a
  repairable own building: **150–169** @ 4; refused: **190**.
* If the mode→index function is ever found and the pairing is inverted, only two
  constants change.

**§C2's `[RE-FALLBACK]` is narrowed, not deleted:** the *art* is pinned (the
"pick frames empirically" instruction of Deviation #10 is discharged — these
indices came from the file plus the binary's own table, not from guessing), while
the *mode→cursor selection function* remains unfound. Update the confidence
summary accordingly: C2 art = **CONFIRMED**; C2 pair assignment = **PROBABLE**;
C2 selection function = **[RE-FALLBACK]**.

The shipped substitute from spec §B12 (pressed sidebar frame 1 + hovered-building
outline) is **no longer necessary as the cursor**, but remains valid as the
*mode-armed* sidebar affordance and should be kept for that.

> Recorded discrepancy, for whoever next uses `0x0082C038`: entries `[79]`
> onward reference start frames ≥ 450 (up to `frame=504 count=8`), and `[49]`
> `frame=356` breaks the otherwise-monotonic ordering. `MOUSE.SHP` on this
> install has only 450 frames, so **the tail of that scan is misaligned or the
> table is longer than 86 entries**. Entries `[0]`–`[48]` are self-consistent
> and stay inside 450; do not trust the tail.

---

## §D9 — Re-measured against THIS install (the wedge bounds)

Rules constants read from the extracted `rulesmd.ini` `[General]`, matching §A1
exactly:

```
RepairStep=8   RepairPercent=0.15   RefundPercent=0.50
AlliedSurvivorDivisor=500   SovietSurvivorDivisor=250   ThirdSurvivorDivisor=750
```

Formulas applied (§A2 / §B1 / §B3), integer division and truncation throughout:

```
per      = Strength / 8                                  (integer)
n        = per ? Cost / per : Cost                        (integer; the per==0 guard is OURS — see §D0.4)
stepCost = max(1, trunc(n * 0.15))
refund   = Soylent ? trunc(Soylent * 1.0)                 (RefundPercent NOT applied)
                   : trunc(trunc(Cost * 1.0 * 1.0) * 0.50)
survA    = Crewed ? clamp(refund / 500, 1, 5) : 0
survS    = Crewed ? clamp(refund / 250, 1, 5) : 0
```

`Crewed` absent ⇒ **no**. `TechnoTypeClass::LoadFromINI` reads it with
`ReadBool(section, "Crewed", <current field value>)` (`rs14_keys.txt`,
`00713dd6 PUSH 0x84290c … 00713de3 MOV byte ptr [EBP + 0xccd],AL`), and the
field's pre-read value on a fresh type is the constructor's zero — so an absent
key leaves `Crewed = 0`.

### The 24-row measured table — Tasks 2 and 7 copy their expectations from here VERBATIM

| # | id | Cost | Strength | Str/8 | Cost/that | stepCost | steps to full | refund | survA | survS | Crewed | Unsellable | ClickRepairable | Soylent | UnitReload |
|--:|----|-----:|---------:|------:|----------:|---------:|--------------:|-------:|------:|------:|--------|------------|-----------------|--------:|-----------|
| 0 | `GACNST` | 3000 | 1000 | 125 | 24 | **3** | 125 | **1500** | **3** | **5** | yes | — | — | — | — |
| 1 | `GAPOWR` | 800 | 750 | 93 | 8 | **1** | 94 | **400** | **1** | **1** | yes | — | — | — | — |
| 2 | `GAPILE` | 500 | 500 | 62 | 8 | **1** | 63 | **250** | **1** | **1** | yes | — | — | — | — |
| 3 | `GAWEAP` | 2000 | 1000 | 125 | 16 | **2** | 125 | **1000** | **2** | **4** | yes | — | — | — | — |
| 4 | `NACNST` | 3000 | 1000 | 125 | 24 | **3** | 125 | **1500** | **3** | **5** | yes | — | — | — | — |
| 5 | `NAPOWR` | 600 | 750 | 93 | 6 | **1** | 94 | **300** | **1** | **1** | yes | — | — | — | — |
| 6 | `NAHAND` | 500 | 500 | 62 | 8 | **1** | 63 | **250** | **1** | **1** | yes | — | — | — | — |
| 7 | `NAWEAP` | 2000 | 1000 | 125 | 16 | **2** | 125 | **1000** | **2** | **4** | yes | — | — | — | — |
| 8 | `GAREFN` | 2000 | 1000 | 125 | 16 | **2** | 125 | **300** | **1** | **1** | yes | — | — | **300** | — |
| 9 | `NAREFN` | 2000 | 1000 | 125 | 16 | **2** | 125 | **300** | **1** | **1** | yes | — | — | **300** | — |
| 10 | `GAPILL` | 500 | 400 | 50 | 10 | **1** | 50 | **250** | **0** | **0** | no | — | — | — | — |
| 11 | `GATOW` *(no section)* | 0 | 0 | 0 | 0 | **1** | n/a | **0** | **0** | **0** | *(absent → no)* | — | — | — | — |
| 12 | `NALASR` | 500 | 400 | 50 | 10 | **1** | 50 | **250** | **0** | **0** | no | — | — | — | — |
| 13 | `NABNKR` | 500 | 600 | 75 | 6 | **1** | 75 | **250** | **0** | **0** | no | — | **yes** | — | — |
| 14 | `TESLA` | 1500 | 600 | 75 | 20 | **3** | 75 | **750** | **0** | **0** | no | — | — | — | — |
| 15 | `NAFLAK` | 1000 | 900 | 112 | 8 | **1** | 113 | **500** | **0** | **0** | no | — | — | — | — |
| 16 | `NAWALL` | 100 | 300 | 37 | 2 | **1** | 38 | **50** | **0** | **0** | *(absent → no)* | — | — | — | — |
| 17 | `NARADR` | 1000 | 1000 | 125 | 8 | **1** | 125 | **500** | **1** | **2** | yes | — | — | — | — |
| 18 | `GATECH` | 2000 | 500 | 62 | 32 | **4** | 63 | **1000** | **2** | **4** | yes | — | — | — | — |
| 19 | `CAOILD` | 0 | 1000 | 125 | 0 | **1** | 125 | **0** | **0** | **0** | *(absent → no)* | **yes** | — | — | — |
| 20 | `NANRCT` | 1000 | 1000 | 125 | 8 | **1** | 125 | **500** | **1** | **2** | yes | — | — | — | — |
| 21 | `ATESLA` | 1500 | 600 | 75 | 20 | **3** | 75 | **750** | **0** | **0** | no | — | — | — | — |
| 22 | `NASAM` | 1000 | 900 | 112 | 8 | **1** | 113 | **500** | **0** | **0** | no | — | — | — | — |
| 23 | `GAAIRC` | 1000 | 600 | 75 | 13 | **1** | 75 | **500** | **1** | **2** | yes | — | — | — | **yes** |

*"steps to full" = `ceil(Strength / 8)`, the number of 8-HP repair ticks from 0
to full; multiply by `stepCost` for the total credit cost of a full repair, and
by 14 frames for its duration (§A2).*

### Key-presence scans over the WHOLE `rulesmd.ini`

| Key | Roster hits | File-wide |
|-----|-------------|-----------|
| `Unsellable=yes` | **`CAOILD` (index 19)** | 11 sections: `SMON SMIN YAREFN CAOUTP CAAIRP CAOILD CAPOWR CAMACH CATHOSP CAHOSP CASLAB` |
| `ClickRepairable=` | **`NABNKR` (index 13) — explicitly `yes`** | 246 sections; every rostered id other than `NABNKR` is absent (⇒ default **yes**), and **no rostered id sets `no`** |
| `Soylent=` | **`GAREFN` (8) and `NAREFN` (9), both `300`** | 135 sections (mostly infantry/vehicles for Yuri's Grinder) |
| `UnitReload=yes` | **`GAAIRC` (index 23)** | 2 sections: `GAAIRC`, `AMRADR` |
| `Crewed=` | 20 rostered ids carry it; 4 do not (`GATOW`, `NAWALL`, `CAOILD`, and no other) | — |

**Every one of the four "expected: none" predictions in the brief was wrong.**
`Unsellable`, `ClickRepairable`, `Soylent` and `UnitReload` each have at least one
rostered hit. `ClickRepairable` and `UnitReload` are behaviourally inert
(`NABNKR`'s explicit `yes` equals the default; `GAAIRC`'s depot case is
unreachable per §D2), but **`Unsellable` on `CAOILD` and `Soylent` on
`GAREFN`/`NAREFN` change real numbers and must be implemented.**

### Cross-check against the printed §A2 / §B3 examples

```
CROSSCHECK GACNST   expected=(3000, 1000, 3, 1500, 3, 5) got=(3000, 1000, 3, 1500, 3, 5)  OK
CROSSCHECK GAPOWR   expected=(800,  1000, 1,  400, 0, 0) got=(800,   750, 1,  400, 1, 1)  *** MISMATCH ***
CROSSCHECK GAWEAP   expected=(2000, 1000, 2, 1000, 2, 4) got=(2000, 1000, 2, 1000, 2, 4)  OK
```

`GACNST` and `GAWEAP` reproduce **exactly**. `GAPOWR` does **not** — investigated
and resolved in **§D0.1**: the document's `Strength=1000 / Crewed=no` for
`GAPOWR` is simply wrong; the file says `750 / yes`. `stepCost` is unaffected;
`steps to full` and `survivors` are. **The §D9 table is authoritative; the §A2
and §B3 `GAPOWR` rows are not.**

### Implementation constants that fall out of this table

* `stepCost` range over the roster: **1 … 4**. `GATECH` is the only 4.
* `refund` range: **0 … 1500**. Two zero-refund types (`GATOW` has no section,
  `CAOILD` has `Cost=0` **and** `Unsellable=yes` so it can never be sold anyway).
* `survivor_count` range: **0 … 5**. Allied maxes at 3 (`GACNST`); Soviet reaches
  the clamp of 5 on `GACNST`/`NACNST`.
* Types yielding **0** survivors (`Crewed` no/absent): `GAPILL GATOW NALASR
  NABNKR TESLA NAFLAK NAWALL CAOILD ATESLA NASAM` — 10 of 24.
* The only sell refusal reachable from rules data today is `CAOILD`'s
  `Unsellable=yes`; the other refusals (`BState == 0`, already-selling, C4,
  no-buildup) are state-driven.

---

## §D — adjudication summary

| Item | Verdict | Tag |
|---:|---|---|
| 1 | Crew type **cut to `E1`/`E2`**; `Rules+0xF70` shown un-anchorable (ctor default, no key string); 25 % branch is an RNG draw that would break gate determinism | **PINNED (ids) / CUT (branch)** |
| 2 | Service-Depot **OUT of scope** — but **`GAAIRC` DOES set `UnitReload=yes`**; cut on unreachability (no aircraft/docking/radio link), not on data. `vtbl+0x274` still unidentified | **OUT of scope** |
| 3 | AI reuse **OUT of scope**; 8 unanchored offsets + a `Random(0,50)` draw that would perturb three green demo gates | **OUT of scope** |
| 4 | Sell routes through the clone's building-death path; `begin_sell`/`complete_sell` add **nothing**; refund lands at sell start | **[RE-FALLBACK] stands** |
| 5 | `HouseClass+0x2F0` = **owned-building count** — three independent confirmations incl. the `0x5DC`/1500 free-MCV rule | **PINNED — upgraded to CONFIRMED** |
| 6 | Mind-control refusal **OUT of scope**; `can_be_sold` deliberately omits the term; `FUN_0070F2C0` shape confirmed, identity still PROBABLE | **OUT of scope** |
| 7 | `UndeploysInto` **OUT of scope**; the clone takes the plain-refund branch, which is a **reachable configuration of the original**, not a deviation | **OUT of scope** |
| 8 | `MOUSE.SHP` = 450 frames, 55×43. `$` = **129–148 + 149**; wrench = **150–189 + 190**; all four animations at **4 ticks/frame**; corroborated by six exact matches against `0x0082C038`. Pair assignment PROBABLE; selection function still unfound | **PINNED (art); §C2 narrowed** |
| 9 | 24-row table measured; `GACNST`/`GAWEAP` reproduce exactly; **`GAPOWR` does not** (§D0.1); four "expected none" key scans all wrong (§D0.2/3, §D2) | **MEASURED — 4 contradictions** |
