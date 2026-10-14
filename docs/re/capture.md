# Engineer Capture / Spy Infiltration — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
(`capture_hunt.py … capture_hunt8.py`), the extracted `rulesmd.ini`, decompile
dumps (`D:\ra2-re\out\capture_*.c`), and the `ra2extract` art/map probes are all
off-repo in the RE workspace; nothing off-repo is committed. **Content-anchored
only:** every function below was reached from INI-key strings, the RulesClass /
BuildingTypeClass / TechnoTypeClass field-loader offsets, the Rules static-object
base `0x886160`, and xref/call-target evidence in THIS binary — never from
published Ares/YRpp addresses. Several core handlers were **undefined functions**
in the analyzed project (the `-noanalysis` DB never carved the InfantryClass /
BuildingClass capture region); they were defined here by bracketing the
neighbouring functions and disassembling the gap, then confirmed by content.)

This document is the calibration authority for the **Engineer/Spy Capture** phase.
The exemplars are the **Allied Engineer `[ENGINEER]`** and the **Allied Spy
`[SPY]`**. Rules VALUES quoted below were read on this install and are ground
truth already; this hunt pins the FORMULAS / offsets / branch logic. Confidence
per finding; "Deviations / fallbacks" at the end resolves the `[RE-GATE:
capture.md §N]` markers.

## §1 — what was recovered (summary)

| # | Item | Evidence | Confidence |
|---|------|----------|:----------:|
| A1 | **Engineer always captures at any health** — `HealthRatio <= EngineerCaptureLevel`, default **1.0** (100%) | ctor `FUN_00665090` sets `+0x17f8 = 1.0f`; What_Action `FUN_0051e17c` line 81 compares; key absent from `rulesmd.ini` | **CONFIRMED** |
| A2 | **`EngineerDamage` / `EngineerAlwaysCaptureTech` do NOT exist** in YR 1.001 (Ares-era additions). **Corrected (wave 7, W7-01-45):** a damage-instead-of-capture branch DOES exist, gated on the MultiEngineer session option (off by default) [RE 0x519449 L1275-1311] | 0 string hits for the Ares keys; MultiEngineer branch in 0x519449 | **CONFIRMED** |
| A3 | **Ownership transfer** = `BuildingClass::ChangeOwner` (vtable **+0x3d4**) `FUN_004481ab` — removes power/production/SW/radar from old house, adds to new, grants `ProduceCashStartup` if from neutral, plays `CaptureEvaEvent` | `FUN_004481ab` + executor `FUN_00519449` line 236 virtual call | **CONFIRMED** |
| A4 | **Engineer is always consumed** (enters building, never re-emerges on capture) | executor `FUN_00519449` — enter removes unit; only C4/bridge branches early-return | **CONFIRMED** |
| A5 | **Engineer → own damaged building = repair to full, engineer consumed** (HealthRatio < 1.0 → repair; == 1.0 → no-op) | What_Action `FUN_0051e17c` L69-77 (`HealthRatio < Rules+0x16f8=1.0` → action 3); executor `FUN_00519449` L257-267 (`CurHP==MaxStrength` → reject, else `bldg->vtable+0x40c()` repair) | **CONFIRMED** (RE-GATE §A5) |
| B1 | **Spy infiltrate dispatch** = `BuildingClass::Infiltrate` `FUN_004571e0(spyHouse)` — early-returns if spy owns the building; branches by building-type purpose | `FUN_00519449` line 153 calls it; per-purpose field reads | **CONFIRMED** |
| B2 | **Power plant → blackout** for `SpyPowerBlackout=1000` frames (≈66.7 s @ 15 Hz) via `FUN_0050bab0(frames)` | `FUN_004571e0` power branch (`Type+0xee0≥1`) → `FUN_0050bab0(Rules+0xd64)` | **CONFIRMED** |
| B3 | **Refinery → cash steal = `SpyMoneyStealPercent=0.5` × victim's current credits** (debit victim, credit spy) | `FUN_004571e0` refinery branch (`Type+0x800>0`), disp32 `Rules+0xd68`@0x457441, `FUN_004f95b0`/`FUN_004f9770` | **CONFIRMED** |
| B4 | **Superweapon building → SW recharge-timer reset** | `FUN_004571e0` (`Type+0x16f0 != -1`) → `FUN_006cd620` (the `superweapon.md §1` CDTimer sync) | **CONFIRMED** |
| B5 | **Spy → factory veterancy = rank-1 veteran; firepower ×1.1, armor(toughness) ×1.5** (`VeteranCombat`/`VeteranArmor`) | rulesmd `[General]` (ra2extract); rank bump = veteran (VeteranCap=2). **RE correction:** the `Type+0x16a4`→`FUN_0050bb30` branch is NOT veterancy (see B6); veteran-application site not isolated, VALUES are ground truth | **CONFIRMED (values)** (RE-GATE §B5) |
| B6 | **Radar / Battle Lab / spy-plane → shroud/GPS-reveal effects only** (`Type+0x16a4`→`FUN_0050bb30`→`FUN_005775e0` reset-shroud; Rules list `+0x920`/`Type+0xeb8`∈{0x10,0x28} → house flags `+0x2bc/2bd/2be/2bf/2c0`,`+0x1fc`, global `883b0e`) → **INERT (no shroud in our engine)** | `FUN_004571e0` L31-134; `FUN_005775e0` str "We tried to reset the shroud…" | **CONFIRMED inert** (RE-GATE §B6) |
| C1 | **Disguise = two object fields** `DisguisedAsType (+0x518)` + `DisguisedAsHouse (+0x51c)` + disguised flag `+0x1d8` | creation init `FUN_006f34a0` lines 124-138 | **CONFIRMED** |
| C2 | **Spy initial (Perma)disguise = Rules `AlliedDisguise`/`SovietDisguise`/`ThirdDisguise` by owner side** (`E1`/`E2`/`INIT`) | `FUN_006f34a0`: `CanDisguise && PermaDisguise` → side-indexed Rules default | **CONFIRMED** |
| C3 | **MakeupKit re-disguise = `Disguise_As(target)`** copies target's type→`obj+0x518` and house→`obj+0x51c`, sets disguised flag | `FUN_007454a0` (vtable **+0x46c**): RTTI-kind 1/0xb/0x24 handled; `→FUN_0070d6b0` sets `+0x1d8=1` | **CONFIRMED (mechanism)** (RE-GATE §C3) |
| C4 | **Detection: `DetectDisguise=yes` units see through disguise; dogs carry it**; a normal unit cannot target a disguised spy | `FUN_006f71d0` line 316 (`IsDisguised && !DetectDisguise` → not targetable); `[ADOG]/[DOG]/[YDOG] DetectDisguise=yes` | **CONFIRMED** |
| C6 | **Reveal-on-fire applies ONLY to non-Perma disguisers (mirage); the PermaDisguise spy is EXEMPT** | fire path `FUN_00700da0` L253-264: `CanDisguise(+0xd2f) && !PermaDisguise(+0xd30) && IsDisguised` → `ClearDisguise` (vtable **+0x470**); SPY has `PermaDisguise=yes` ⇒ never clears. Rules: `PermaDisguise=yes; and I appear that way always` | **CONFIRMED** (RE-GATE §C6) |
| C5 | **Mirage/fake-blink detection is RNG** (`DisabledDisguiseDetectionPercent`) — permadisguise-spy-by-dog detection is deterministic | `FUN_006f71d0` `FUN_0065c220(0,99)` vs `Rules+0xe14` | **CONFIRMED** |
| D1 | **Neutral tech structures exist & are engineer-capturable** (oil derrick, hospital, machine shop, power, airport, …) with real passive values | `rulesmd.ini` `[CAOILD]/[CATHOSP]/[CAMACH]/[CAPOWR]/[CAAIRP]` + `FUN_00458731` cash tick | **CONFIRMED** |
| D2 | Which tech structures appear on **`mapsmd03`** | map is hashed inside `mapsmd03.mix`; not parsed | **[RE-FALLBACK]** |
| E | **No RNG in capture / ownership-transfer / spy-effect paths** (pure frame + credit arithmetic); RNG only in disguise *detection* probability and cosmetic grinder animations | grep `FUN_0065c220`/`65c1c0` over all dumped handlers | **CONFIRMED** |

Exemplar ids: engineer **`ENGINEER`** (`Engineer=yes`, art `conquer.mix/engineer.shp` 114 032 B present); spy **`SPY`** (`Agent=yes Infiltrate=yes CanDisguise=yes PermaDisguise=yes`, art `spy.shp` 82 240 B present); detector **`ADOG`/`DOG`/`YDOG`** (`DetectDisguise=yes`, art `dog.shp` 76 896 B present). Soviet `[SENGINEER]`, Yuri `[YENGINEER]` reuse `Image=ENGINEER`.

## Recovered ground truth (read from this install's `rulesmd.ini`)

```
[General]                                  ; NOTE the capture-level key is ABSENT
;   EngineerDamage           -> KEY DOES NOT EXIST (engine has no such field)
;   EngineerCaptureLevel     -> KEY ABSENT -> engine default 1.0 (100%) applies
;   EngineerAlwaysCaptureTech-> KEY DOES NOT EXIST
AlliedDisguise=E1
SovietDisguise=E2
ThirdDisguise=INIT
SpyPowerBlackout=1000        ; frame time a spy shuts down power for (900 = 1 minute)
SpyMoneyStealPercent=.5      ; percent of total money you take with a spy
MultiEngineer=no             ; (DESUPPORTED) -> one engineer captures
AttackCursorOnDisguise=yes
DisabledDisguiseDetectionPercent=15,5,2    ; h,m,e  (mirage fake-blink detect % per unit)

[ENGINEER]  Engineer=yes  Cost=500  Strength=75  Primary=DefuseKit  TechLevel=1
[SPY]       Agent=yes  Infiltrate=yes  CanDisguise=yes  PermaDisguise=yes
            Primary=MakeupKit  Cost=1000  Soylent=500  Strength=100  Armor=flak
[ADOG]/[DOG]/[YDOG]  DetectDisguise=yes  Primary=GoodTeeth

; neutral tech structures (all: Capturable=yes NeedsEngineer=yes Unsellable=yes TechLevel=-1)
[CAOILD]  ProduceCashStartup=1000  ProduceCashAmount=20  ProduceCashDelay=100
[CATHOSP] InfantryGainSelfHeal=1          [CAMACH]  UnitsGainSelfHeal=1
[CAPOWR]  Power=200                       [CAAIRP]  SuperWeapon=ParaDropSpecial  Ammo=5
```

`ProduceCashStartup=1000` carries the verbatim comment *"credits when captured from
the Neutral house"* — corroborated by `FUN_004481ab` gating the grant on the OLD
owner being neutral.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_0066cf70` @ `0x66CF70` (18790 B) | **RulesClass `[General]` reader** — reads `EngineerCaptureLevel`→`+0x17f8`, `SpyPowerBlackout`→`+0xd64`, `SpyMoneyStealPercent`→`+0xd68`(float), `AlliedDisguise`→`+0xd58`, `SovietDisguise`→`+0xd5c`, `ThirdDisguise`→`+0xd60`, `Engineer`(default type)→`+0xf70` | `"EngineerCaptureLevel"`/`"SpyPowerBlackout"`/… string xrefs |
| `FUN_00665090` @ `0x665090` (9175 B) | **RulesClass ctor / defaults** — `+0x17f8 = 1.0f` (EngineerCaptureLevel), `+0xd68 = 0.5f` (SpyMoneyStealPercent), `+0xd64 = 0` (SpyPowerBlackout), `+0x16f8 = 1.0` & `+0x1700 = 0.5` & `+0x1708 = 0.5` (doubles, ctor-only repair/capture thresholds) | writes the Rules static object at `0x886160` |
| `FUN_0045fe40` @ `0x45FE40` (19475 B) | **BuildingTypeClass::LoadFromINI** — `Capturable`→`+0x1572`, `Spyable`→`+0x1576`, `NeedsEngineer`→`+0x1552`, `CaptureEvaEvent`→`+0x1554`, `ProduceCashStartup`→`+0x1558`, `ProduceCashAmount`→`+0x155c`, `ProduceCashDelay`→`+0x1560` | INI-key string xrefs |
| `FUN_00711510` @ `0x711510` (16337 B) | **TechnoTypeClass::LoadFromINI** — `Sensors`→`+0xc9d`, `CanDisguise`→`+0xd2f`, `PermaDisguise`→`+0xd30`, `DetectDisguise`→`+0xd31`, `DetectDisguiseRange`→`+0xd34` | INI-key string xrefs |
| InfantryTypeClass loader (gap ~`0x524000`) | reads `Engineer=`/`Agent=`/`Infiltrate=` flags | string xrefs `Engineer`@0x5242A1, `Agent`@0x5242D1 (Ghidra gap) |
| `FUN_0051e17c` @ `0x51E17C` (3382 B) | **TechnoClass::What_Action** — for a `Capturable` building: `HealthRatio(FUN_005f5470) <= Rules.EngineerCaptureLevel(+0x17f8)` → **ACTION_CAPTURE (9)** else 0x1c; own-building engineer→repair branch (vs `Rules+0x16f8`); `Spyable`→enter action | seed = EngineerCaptureLevel/Capturable/Spyable disp32 in gap |
| `FUN_00519449` @ `0x519449` (5034 B) | **Enter-building action executor** (InfantryClass) — spy path: `FUN_004571e0(spyHouse)`; capture path: `building->ChangeOwner(unitHouse,1)` via vtable **+0x3d4**; engineer consumed; grinder/bioreactor branch gives Soylent credits | caller of `FUN_004571e0`; contains ChangeOwner virtual call |
| `FUN_004481ab` @ `0x4481AB` (4510 B) | **BuildingClass::ChangeOwner** (vtable slot +0x3d4) — no-op if new==old; subtracts building power/output/SW from old house (`+0x164/+0x168/…`), adds to new; grants `ProduceCashStartup` **iff old owner neutral** (`FUN_004f9770`); starts oil-derrick cash tick (`+0x1b4/5/6`, delay `+0x1560`); plays `CaptureEvaEvent (+0x1554)` | ProduceCashStartup/CaptureEvaEvent disp32 in gap; invoked virtually |
| `FUN_004571e0` @ `0x4571E0` (965 B) | **BuildingClass::Infiltrate / spy-effect dispatch** — returns if `owner==spyHouse`; factory(`Type+0x16a4`)→veterancy; power(`Type+0xee0≥1`)→`FUN_0050bab0(SpyPowerBlackout)`; SW(`Type+0x16f0≠-1`)→`FUN_006cd620`; refinery(`Type+0x800>0`)→steal `SpyMoneyStealPercent`×credits; radar/lab→house reveal flags | `SpyPowerBlackout`/`SpyMoneyStealPercent` disp32; caller `FUN_00519449` |
| `FUN_0050bab0` @ `0x50BAB0` | **apply power-blackout (N frames)** primitive | callers = infiltrate `FUN_004571e0` + SW dispatcher `FUN_006cba10` |
| `FUN_006cd620` @ `0x6CD620` | **SuperClass timer (re)sync** (`superweapon.md §1`) — SW-building infiltrate resets recharge | reused; called from `FUN_004571e0` SW branch |
| `FUN_004f9770` @ `0x4F9770` | **HouseClass::GiveCredits(amount)** | callers: ChangeOwner startup-cash, infiltrate steal, oil tick |
| `FUN_004f95b0` @ `0x4F95B0` | **HouseClass debit/spend credits** | infiltrate money-steal debit of victim |
| `FUN_00458731` @ `0x458731` (141 B) | **oil-derrick periodic tick** — every `ProduceCashDelay` frames grants `ProduceCashAmount` via `FUN_004f9770` | ProduceCashAmount disp32 |
| `FUN_006f34a0` @ `0x6F34A0` (953 B) | **TechnoClass creation/init** — if `CanDisguise(+0xd2f) && PermaDisguise(+0xd30)`: set disguised flag `obj+0x1d8`, `DisguisedAsType obj+0x518 = Rules {Allied,Soviet,Third}Disguise` by owner side (`house+0x1e8`), `DisguisedAsHouse obj+0x51c = ownerHouse` | Rules disguise-default immediate xref |
| `FUN_006f71d0` @ `0x6F71D0` (3259 B) | **What_Action target eligibility + disguise detection** — `IsDisguised(vtable+0xc8) && !GetType()->DetectDisguise(+0xd31)` ⇒ not targetable; mirage detect RNG `FUN_0065c220(0,99)` vs `Rules+0xe14` (`DisabledDisguiseDetectionPercent`) | Capturable/NeedsEngineer/DetectDisguise + Rules base xref |
| `FUN_0065c220`/`FUN_0065c1c0` | R250 randomizer (`combat.md §5`) — used ONLY in disguise/mirage detection + cosmetic grinder anims | grep of dumped handlers |
| Rules static object base `0x886160` | RulesClass instance (fields cited as `0x886160+off`) | ctor writes; disguise-default immediates |

---

## §A Engineer capture

**A1 — capture at ANY health (default 1.0). CONFIRMED.** The RulesClass ctor
(`FUN_00665090`) sets `Rules+0x17f8 = 0x3f800000 = 1.0f`. `EngineerCaptureLevel`
is **not present** in this install's `rulesmd.ini`, so the 1.0 default stands.
What_Action (`FUN_0051e17c`) at `0x51E38A`:

```c
if (BuildingType->Capturable (+0x1572) != 0) {
    ratio = HealthRatio(building);               // FUN_005f5470, 0.0 .. 1.0
    if (ratio <= Rules->EngineerCaptureLevel)    // float at Rules(0x886160)+0x17F8, == 1.0
        return 9;                                // ACTION_CAPTURE
    return 0x1c;                                 // (can't)
}
```

With the threshold at 1.0 every `HealthRatio ≤ 1.0` passes ⇒ **the engineer
captures a capturable enemy building at any health**. There is no separate
"damage the building instead" branch — see A2.

**A2 — `EngineerDamage`/`EngineerAlwaysCaptureTech` are not real YR keys.
CONFIRMED.** A whole-image C-string scan finds `EngineerCaptureLevel` (1 hit,
`0x83A40C`) but **zero** hits for `EngineerDamage` or `EngineerAlwaysCaptureTech`
(and none for `Detector`, `MakeupKit`, `VeteranBuildings`). These are Ares/later
additions; the original engine has no partial-health "damage-only" capture mode.

**A3 — ownership transfer = `BuildingClass::ChangeOwner` `FUN_004481ab` (vtable
+0x3d4). CONFIRMED.** Invoked from the executor `FUN_00519449` line 236 as
`(**(building+0x3d4))(engineerHouse, 1)`. What it updates:

- returns immediately if `newHouse == building->OwnerHouse (+0x21c / [0x87])`;
- removes the building's contribution from the **old** house — power drain/output
  (`house+0x164`/`+0x168 -= Type+0x1564/+0x1568`, clamped ≥0), superweapon slot,
  radar/spysat state, factory bookkeeping;
- reassigns owner and adds the same contributions to the **new** house
  (`house+0x538c` building-count++, power, `+0x2d4 += Type+0x1780`);
- **grants `ProduceCashStartup` iff the OLD owner is neutral** (`if
  oldOwner->Type[+0x1a6] && Type->ProduceCashStartup(+0x1558) != 0`) →
  `FUN_004f9770(startup)` and arms the periodic tick (`+0x1b4=frame`,
  `+0x1b6=ProduceCashDelay`);
- plays `CaptureEvaEvent (+0x1554)` (when `!= -1`) via the EVA/notify path.

**A4 — engineer always consumed. CONFIRMED.** The enter action removes the
infantry from the map before completion; the capture branch (`FUN_00519449`
line 208-238) transfers ownership and never re-spawns the unit. Only the C4/bomb
(`0xec4`) and bridge-repair branches early-return the unit; the capture branch
does not. (`MultiEngineer=no` ⇒ one engineer suffices, DESUPPORTED key.)

**A5 — engineer → own damaged building repairs it. PROBABLE.** What_Action
(`FUN_0051e17c` lines 65-77) has a distinct branch when the actor is an engineer
on its **own** building: it compares `HealthRatio` to `Rules+0x16f8` (= **1.0**,
ctor-only double). `HealthRatio < 1.0` (damaged) → returns a repair action (3);
`HealthRatio == 1.0` (full) → no action. The engineer is consumed on repair like
on capture. (Branch structure confirmed; the repair-amount application was not
separately traced, hence PROBABLE.)

## §B Spy infiltration (`FUN_004571e0`, one dispatch per building purpose)

Called as `Infiltrate(spyOwnerHouse)` from the enter executor (`FUN_00519449`
line 153) when the entering unit is a spy (`Agent=yes`). It **returns
immediately if `building->OwnerHouse == spyOwnerHouse`** (no self-spy — a spy
entering your own building does nothing). Then, on the building's TYPE fields:

| Building purpose | Discriminator | Effect | Key / value |
|---|---|---|---|
| **Factory** (barracks/war fac/naval) | `Type+0x16a4` set | veterancy: `FUN_0050bb30`, house factory-spied flag `+0x577a`/`+0x5778/9` | future units get a rank bump *(PROBABLE)* |
| **Power plant** | `Type+0xee0 ≥ 1` (power output) | `FUN_0050bab0(Rules->SpyPowerBlackout)` | **1000 frames ≈ 66.7 s** *(CONFIRMED)* |
| **Superweapon bldg** | `Type+0x16f0 ≠ -1` (SW index) | `FUN_006cd620` — reset SW recharge timer | *(CONFIRMED)* |
| **Refinery** | `Type+0x800 > 0` (storage) | steal `SpyMoneyStealPercent × victim credits`; `FUN_004f95b0` debit + `FUN_004f9770` credit | **0.5 = 50%** *(CONFIRMED)* |
| **Radar / lab / spy-plane** | `Type+0xeb8`∈{0x10,0x28}, Rules list `+0x920` | set house reveal flags `+0x2bc/2bd/2be/2bf/2c0` | *(PROBABLE)* |

**B2 power blackout — CONFIRMED.** `SpyPowerBlackout=1000` (INI; ctor default 0).
The blackout primitive `FUN_0050bab0(frames)` is shared with the SW fire
dispatcher `FUN_006cba10`. 1000 frames @ 15 Hz ≈ 66.7 s (rules comment "900 = 1
minute").

**B3 cash steal — CONFIRMED.** The refinery branch reads `Rules+0xd68`
(`SpyMoneyStealPercent`, disp32 at `0x457441`), multiplies the victim house's
available credits, `_ftol`s it (`FUN_007c4d00`), debits the victim
(`FUN_004f95b0`) and credits the spy owner (`FUN_004f9770`). Value **0.5** ⇒ the
spy takes 50 % of the victim's current money. (The multiply is folded into the
FPU in the decompile; the `+0xd68` reference in this function is the proof.)

**B4 superweapon reset — CONFIRMED.** SW buildings (`Type+0x16f0 != -1`) route to
`FUN_006cd620`, the `superweapon.md §1` CDTimer (re)sync — the SW recharge timer
is reset (delayed). Trivial to hit given the `SuperClass` per-house instancing
already recovered.

**B5 veterancy — PROBABLE.** Factory buildings (`Type+0x16a4`) set a house-level
"factory spied" flag (`+0x577a`); the game then produces subsequent units of that
factory category at veteran rank. The exact rank and per-category gating were not
fully isolated (the `FUN_0050bb30` helper reads the factory category), hence
PROBABLE.

**B6 radar / Battle Lab / spy-plane — PROBABLE.** The non-power/non-SW branch
walks a Rules category list (`+0x920`, count `+0x92c`) and, by `Type+0x6d0` and
`Type+0xeb8`, sets house flags `+0x2bc/+0x2bd/+0x2be/+0x2bf/+0x2c0` — the
radar/shroud-reveal and spy-plane markers. Radar reset is trivial (a house flag);
the Battle-Lab-specific effect was not separately confirmed.

## §C Disguise & detection

**C1/C2 disguise representation + initial disguise — CONFIRMED.** A disguisable
object carries `DisguisedAsType (obj+0x518)`, `DisguisedAsHouse (obj+0x51c)` and a
disguised flag `obj+0x1d8`. On creation (`FUN_006f34a0` lines 124-138), a unit
whose type has `CanDisguise (+0xd2f) && PermaDisguise (+0xd30)` (the spy) is set
disguised, `DisguisedAsHouse = own owner house`, and `DisguisedAsType = Rules->
{AlliedDisguise,SovietDisguise,ThirdDisguise}` chosen by the owner's side index
(`house+0x1e8` = 0/1/other) — i.e. the spy renders as its **own side's default
infantry** (`E1`/`E2`/`INIT`) until re-disguised.

**C3 disguise changes to the touched unit — PROBABLE.** `[SPY] Primary=MakeupKit`
(a virtual weapon; rules comment: *"virtual weapon that picks disguise"*). When
the spy targets a unit, it re-disguises as that unit's type+house (the disguise
fields C1). The MakeupKit weapon-effect function was not pinned to an address, so
the exact "last unit targeted" write is PROBABLE (representation is CONFIRMED).

**C4 detection via `DetectDisguise` — CONFIRMED.** A disguised object is
non-targetable by a would-be attacker unless the attacker's type has
`DetectDisguise=yes` (`+0xd31`): `FUN_006f71d0` line 316 gates on `IsDisguised
(vtable+0xc8) && GetType()->DetectDisguise == 0`. Attack **dogs** carry it —
`[ADOG]`, `[DOG]`, `[YDOG]` all `DetectDisguise=yes` (`Primary=GoodTeeth`). Note
YR uses two orthogonal flags: `DetectDisguise=` (see through spy PermaDisguise,
per-object) and `Sensors=` (`+0xc9d`, reveal cloaked/submerged) — `Detector=` is
**not** a YR key (0 string hits).

**C4 correction (wave 7, W7-01-02) — CONFIRMED [RE: 0x6F71D0 L455-467,
0x5224B0, 0x522520].** There is **no detection radius and no flag clear**: a
DetectDisguise scanner simply ignores the disguise in its own threat scan; the
disguise stays on for every other observer. A disguise fools an observer only
when the spy is not allied with it and the disguise house IS allied with it
(the PermaDisguise default, in the spy's own colours, fools nobody). The
clone's old 6-cell reveal loop (`capture.hpp` kDetectDisguiseRange) is gone;
`World::disguise_fools` + `Unit::detect_disguise` gate every scan site.
`DetectDisguiseRange` applies to buildings only (NAPSIS, W7-01-34).

**C5 detection RNG — CONFIRMED (and scoped).** The only randomness in the whole
disguise system is the *mirage / fake-blink* detection roll: `FUN_006f71d0`
calls `FUN_0065c220(0,99)` and compares to `Rules+0xe14`
(`DisabledDisguiseDetectionPercent = 15,5,2` by difficulty). A dog seeing through
a spy's PermaDisguise is **deterministic** (flag test, no roll).

## §D Neutral tech structures

**D1 — CONFIRMED.** All are `Capturable=yes NeedsEngineer=yes Unsellable=yes
TechLevel=-1` and captured by the same `ChangeOwner` path as any building:

| Section | Name | Passive benefit (real values) |
|---|---|---|
| `[CAOILD]` | Tech Oil Derrick | `ProduceCashStartup=1000` (on capture-from-neutral) + `ProduceCashAmount=20` every `ProduceCashDelay=100` frames (`FUN_00458731`) |
| `[CATHOSP]` | Tech Hospital | `InfantryGainSelfHeal=1` |
| `[CAMACH]` | Tech Machine Shop | `UnitsGainSelfHeal=1` |
| `[CAPOWR]` | Tech Civilian Power Plant | `Power=200` |
| `[CAAIRP]` | Tech Airport | `SuperWeapon=ParaDropSpecial`, `Ammo=5` |
| `[CACLNIC]`/`[CAHOSP]` (legacy) | civilian hospital variants | — |

**D2 — `mapsmd03` contents = [RE-FALLBACK].** `mapsmd03.mix` stores its maps
under hashed names; the map body was not parsed, so which tech structures the
reference map places is not recovered. Fallback: the engine spawns neutral tech
structures from map object lists exactly like any pre-placed building; the demo
should place at least one oil derrick (`CAOILD`) so the capture-cash path is
exercised.

## §E Determinism / byte-identity

- **No RNG on capture, ownership transfer, oil-derrick cash, or any spy-effect
  path — CONFIRMED.** `FUN_004481ab`, `FUN_004571e0`, `FUN_0051e17c`,
  `FUN_00458731` contain **zero** calls to the R250 randomizer
  (`FUN_0065c220`/`FUN_0065c1c0`, `combat.md §5`). The blackout timer, cash steal
  and cash tick are pure frame/credit integer arithmetic against the global frame
  counter `DAT_00a8dcec`.
- The enter executor `FUN_00519449` has **2** randomizer calls (`FUN_0065c1c0`,
  lines 79/89) — but they are in the **grinder / bio-reactor consume** branch
  (unit destroyed for `Soylent` credits, `FUN_0074f800` spawns debris at a random
  offset), i.e. cosmetic animation placement, **not** the capture or infiltrate
  logic.
- RNG in the disguise subsystem is confined to *mirage/fake-blink* detection
  probability (§C5). Deterministic engine behaviour is therefore preserved for
  capture/infiltration with a fixed per-house / per-object iteration order.

---

## §Adjudication (RE-GATE) — the five pinned items

Task-1 RE-GATE pass: `ra2extract` real `rulesmd.ini` + targeted re-decompile of the
already-named functions (dumps `D:\ra2-re\out\capture_*.c`, off-repo). Each item
below flips a PROBABLE/[RE-FALLBACK] marker to a pinned value with address/rules
evidence, or an adjudicated [RE-FALLBACK].

**§A5 — Engineer-repair threshold. CONFIRMED (matches prior fallback).**
What_Action `FUN_0051e17c` @0x51E17C L69-77: for an engineer (`actorHouseFlags+0xec3`)
on its **own** building (`FUN_004f98b0` true) whose type is repairable (`Type+0x16c1`),
`HealthRatio (FUN_005f5470) < Rules double at +0x16F8 (== 1.0)` ⇒ returns **action 3
(REPAIR)**; `HealthRatio == 1.0` (full) ⇒ no repair (returns 0x20/0xb). The threshold
`Rules+0x16f8=1.0` is ctor-only (`FUN_00665090`). Executor `FUN_00519449` @0x519449
L257-267 (own-building branch): `if (CurHP[0x1b] == Type.Strength[+0xa0]) reject-and-exit;
else building->vtable+0x40c()` (the building's repair op); the engineer is consumed
(enter removes the unit; single-use ⇒ not per-tick). **Adjudicated: heal to full HP +
consume the engineer** (the HP==MaxStrength guard + single consume proves heal-to-full,
not incremental). No `[General]` repair-amount key exists.

**§B5 — Veteran rank + firepower/armor multipliers. CONFIRMED from rules
(DIFFERS from the ×1.25/×1.25 fallback).** Real `rulesmd.ini [General]` (ra2extract):
`VeteranRatio=3.0  VeteranCombat=1.1  VeteranSpeed=1.2  VeteranSight=0.0
VeteranArmor=1.5  VeteranROF=0.6  VeteranCap=2  InitialVeteran=no`. Rules header:
*"they are all multipliers"*; `VeteranCombat` = "multiplier to damage" (firepower),
`VeteranArmor` = "max strength multiplied … damage divided by this" (toughness). A spy
in a Barracks/War-Factory grants produced units **veteran = rank 1** (one chevron;
elite=rank 2 is not granted by a spy). **Task-6 spawn-baked constants:
`kVeteranFirepowerMult = 1.1` (VeteranCombat), `kVeteranArmorMult = 1.5` (VeteranArmor).**
*RE correction:* the hunt's B5 attribution (`FUN_004571e0` `Type+0x16a4`→`FUN_0050bb30`)
is **wrong** — that branch is the radar shroud-reset (see §B6). `FUN_0050bb30` @0x50BB30 =
`if (house+0x577a==0) FUN_005775e0(house)`; `FUN_005775e0` @0x5775E0 is the shroud
reset (string *"We tried to reset the shroud…"*), and `house+0x577a` is the
shroud-reset latch (not a veteran latch; `house+0x5778` is a generic house-dirty flag
written in ~25 functions incl. ChangeOwner/blackout). The spy→factory veteran-*application*
site (unit production reading the factory-spied state) was **not isolated** this pass;
the multiplier VALUES that Task 6 needs are ground truth from rules, so this is pinned.

**§C3 — MakeupKit re-disguise. CONFIRMED mechanism (upgraded from PROBABLE).**
`Disguise_As` = `FUN_007454a0` @0x7454A0 (vtable **+0x46c**): given a target object it
writes `this.DisguisedAsType (obj+0x518 / field[0x146]) = target's type` and
`this.DisguisedAsHouse (obj+0x51c / field[0x147]) = target's house`, then `FUN_0070d6b0`
sets the disguised flag `obj+0x1d8=1` + stamp `obj+0x1dc`. RTTI kinds handled: 1 (techno →
copies the target's own disguise via vtable+0xcc/+0xd0), 0xb (building), 0x24
(terrain/overlay → GetType). Initial spy disguise is still the side default via
`FUN_006f34a0` (§C2). The MakeupKit *weapon-dispatch* call site into `Disguise_As` was
not separately pinned, but the disguise-change function itself is fully pinned.
**Adjudicated:** Task 7 may keep PermaDisguise side-default only as the carry-forward
(spec §Out), OR implement `Disguise_As(target)` now that the mechanism is exact — no
[RE-FALLBACK] needed for the representation.

**§C6 — Reveal-on-fire/attack. CONFIRMED — the spy is EXEMPT (DIFFERS from the
"clear disguise on attack" fallback).** Fire path `FUN_00700da0` @0x700DA0 L253-264:
after a successful fire (result ∉ {0,4}), `if (Type.CanDisguise[+0xd2f] != 0 &&
Type.PermaDisguise[+0xd30] == 0 && IsDisguised[vtable+0xc4]) → ClearDisguise
(vtable+0x470 = FUN_00745550)` which clears `obj+0x1d8` + type/house. The **SPY has
`PermaDisguise=yes` (`+0xd30 != 0`) so the guard is false ⇒ the spy NEVER drops its
disguise by firing.** Reveal-on-fire is the **mirage-tank fake-blink** only
(`CanDisguise=yes, PermaDisguise=no`), corroborated verbatim by rules: `[SPY]
PermaDisguise=yes; and I appear that way always (Mirage Tank will be Can but not Perma)`
and `DisabledDisguiseDetectionPercent … detect a Mirage Tank who is fake-blinking (from
Firing)`. (`ClearDisguise` is also invoked by the mirage per-frame AI `FUN_007456f0` on
enemy adjacency — again mirage, not spy.) **Adjudicated: Task 7 must NOT clear the spy's
disguise on attack/fire.** The only spy-disguise defeat is the `DetectDisguise` dog
targetability gate (§C4), which never clears the flag.

**§B6 — Radar / Battle-Lab reveal specifics. CONFIRMED inert (matches fallback).**
`FUN_004571e0` @0x4571E0 non-power/non-SW/non-refinery branches touch only shroud/GPS
reveal state: (a) `Type+0x16a4 != 0` (radar) → `FUN_0050bb30`→`FUN_005775e0` **reset
shroud for the building's owner house** (re-shroud), latch `house+0x577a`; (b) type in
Rules list `+0x920` (count `+0x92c`) → set `house+0x2be/+0x2bd/+0x2bc` by `Type+0x6d0`;
(c) `Type+0xeb8 == 0x10` → `house+0x2bf=1` + global `DAT_00883b0e=1`; (d) `Type+0xeb8 ==
0x28` → `house+0x2c0=1` + global `DAT_00883b0e=1`; all also set `house+0x1fc=1`. Our
engine has **no shroud/GPS model**, so every one of these is **INERT** (recognized,
no-op). Discriminators are pinned; nothing here transfers state a shroudless engine can
represent.

---

## Deviations / fallbacks (resolves `[RE-GATE: capture.md §N]`)

- **[PLAN CONFIRM — §A1]** Engineer captures at any health. Use
  `EngineerCaptureLevel = 1.0` (100 %); gate `HealthRatio ≤ 1.0` (always true).
  Do **not** implement `EngineerDamage` / `EngineerAlwaysCaptureTech` — they are
  not YR 1.001 features.
- **[PLAN CONFIRM — §A3/A4]** Capture = `ChangeOwner(newHouse)`: no-op if same
  owner; move power/production/SW/radar between houses; grant
  `ProduceCashStartup` only when captured from the neutral house; play
  `CaptureEvaEvent`. The engineer is always consumed.
- **[PLAN CONFIRM — §A5]** Engineer entering an OWN damaged building repairs it
  (heal toward full) and is consumed; no action on a full-health own building.
- **[PLAN CONFIRM — §B]** Spy infiltrate is a per-purpose dispatch keyed off
  building-type fields, gated by "not your own building". Power→blackout 1000 f;
  Refinery→steal 50 % of victim credits; SW→reset recharge timer; Factory→
  veteran future units; Radar/lab→reveal flags.
- **[RE-GATE APPLIED — §B5]** Spy→factory grants **veteran (rank 1, 1 chevron)**;
  the rank-1 multipliers are the real `[General]` values **firepower ×1.1**
  (`VeteranCombat`), **armor/toughness ×1.5** (`VeteranArmor`) — NOT the ×1.25/×1.25
  placeholder. Task-6 constants: `kVeteranFirepowerMult=1.1`, `kVeteranArmorMult=1.5`.
  (Veteran-application RE site not isolated; values are ground truth from rules.)
- **[RE-GATE APPLIED — §B6]** Radar / Battle-Lab / spy-plane infiltration is a
  **shroud/GPS reveal effect only** (reset-shroud + house reveal flags) and is
  **INERT** in our shroudless engine — recognized, no-op. Discriminators pinned in
  §Adjudication §B6.
- **[RE-GATE APPLIED — §C3]** Represent disguise as `(DisguisedAsType obj+0x518,
  DisguisedAsHouse obj+0x51c)` + flag `obj+0x1d8`; initial spy disguise = side default
  (`E1/E2/INIT`, §C2). MakeupKit re-disguise = `Disguise_As(target)` (`FUN_007454a0`,
  vtable+0x46c) copying the target's type+house — mechanism now pinned; Task 7 may
  implement it or keep PermaDisguise-only as the documented carry-forward.
- **[RE-GATE APPLIED — §C6]** **Do NOT clear the spy's disguise on fire/attack** — the
  PermaDisguise spy is exempt (`FUN_00700da0` L253-264 gates reveal-on-fire on
  `CanDisguise && !PermaDisguise`). Reveal-on-fire is mirage-only; the spy is defeated
  solely by `DetectDisguise` dogs (§C4).
- **[PLAN CONFIRM — §C4/C5]** Disguise is seen through only by `DetectDisguise=yes`
  units (dogs); this is deterministic. Only mirage fake-blink detection rolls
  against `DisabledDisguiseDetectionPercent` (RNG) — out of scope unless mirages
  are implemented.
- **[RE-FALLBACK — §D2]** `mapsmd03` tech-structure placement not recovered;
  demo spawns a `CAOILD` oil derrick (and optionally a hospital) via the map
  object list.
- **[PLAN CONFIRM — §E]** Keep capture/infiltration deterministic (no RNG in the
  original on these paths); fixed iteration order is the only determinism lever.
```
