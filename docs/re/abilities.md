# Unit special abilities (batch 1) — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
`p4_hunt1.py … p4_hunt8.py`, `p4_dis.py` off-repo at `D:\ra2-re\scripts\`
(project copy `D:\ra2-re\p4proj`, so the shared `ra2` project is never locked);
decompiles + forced listings at `D:\ra2-re\out\p4\`. **Content-anchored only:**
every Rules/Type offset below was reached from an INI-key string reference in
*this* binary (`IFVMode` `0x842A84`, `Gunner` `0x842904`, `Teleporter`
`0x842E00`, `C4Delay` `0x839D80`, `C4Warhead` `0x83A1CC`, `IvanWarhead`
`0x83A14C`, `IvanDamage` `0x83A108`, `IvanTimedDelay` `0x83A0F8`,
`RadDurationMultiple` `0x83A328`, `RadApplicationDelay` `0x83A314`,
`RadLevelMax` `0x83A308`, `RadLevelDelay` `0x83A2F8`, `RadLevelFactor`
`0x83A2D8`, `RadSiteWarhead` `0x83A29C`, `ChronoDistanceFactor` `0x83B6E0`,
`ChronoMinimumDelay` `0x83B6BC`, `ChronoRangeMinimum` `0x83B6A8`,
`ChronoTrigger` `0x83B6D0`, `SuppressionThreshold` `0x84268C`, `Parasite`
`0x81617C`, `Temporal` `0x816168`, `IvanBomb` `0x81AD70`, `RadLevel`
`0x848218`, `Suicide` `0x841FF0`, `DeployFire` `0x842A40`, `Explodes`
`0x832554`, `DeathWeapon` `0x83A114`) and then followed by a whole-binary
displacement scan (`p4_hunt2.py`) to every reader. YRpp class layouts
(`ParasiteClass.h`, `TemporalClass.h`, `RadSiteClass.h`, `BombClass.h`) were used
as a *naming* map only — **their absolute addresses drifted in this build**
(e.g. YRpp `ParasiteClass::ctor 0x6292B0` lands mid-function here; YRpp
`TemporalClass::Fire 0x71AF20` is an unrelated class; the real Temporal code is
at `0x719A70…0x71A1A0`). Several bodies were not auto-defined by Ghidra and were
carved with `CreateFunctionCmd` (`p4_hunt5.py`).)

Rules ground truth = the real `rulesmd.ini` of this install (742 958 bytes,
`localmd.mix/rulesmd.ini` inside `ra2md.mix`, extracted with `ra2extract`).

---

## Located functions

| Address | Identity | Anchor |
|---|---|---|
| `FUN_004690A0` | `BulletClass::Detonate` — the warhead-flag dispatch chain + RadSite creation | xref of `0x65AF30` (SetRadLevel) and `0x438DF0` (Bomb plant) |
| `FUN_0065AF00/10/30/70` | `RadSite::SetBaseCell / SetSpread / SetRadLevel / Add` | readers of `RadDurationMultiple` (+0x1804) |
| `FUN_0065AFC0` | `RadSite::Activate` | reader of `RadLevelDelay` (+0x1810), caller of `0x65B400` |
| `FUN_0065B400` | `RadSite::Radiate` (per-cell add) | callee of Activate |
| `FUN_0065B740` | `RadSite::DecreaseRadiation` | callee of the RadSite AI |
| `FUN_0065B590` | `RadSite::Deactivate` (removes the remainder) | callee of `Add` |
| `0x0065B240` (carved) | `RadSite::AI` | caller of `0x65B740`; reads `RadLevelDelay` |
| `FUN_00487BC0` | `CellClass::GetRadLevel` (clamped to `RadLevelMax`) | reader of `RadLevelMax` (+0x180C) |
| `FUN_00487BF0/0x487C10` | `CellClass::RadLevel_Increase / _Decrease` (clamp at 0) | callees of Radiate/Decrease |
| `FUN_004DA440` | `FootClass::AI` — radiation damage application | reader of `RadApplicationDelay` (+0x1808) and `RadLevelFactor` (+0x1818) |
| `FUN_0062A420` | `ParasiteClass::TryInfect` | callee of Detonate's `Parasite` branch |
| `FUN_0062A380` | `ParasiteClass::CanInfect` | callee of TryInfect |
| `0x00629A50` (carved) | `ParasiteClass::AI` (drone branch) — the damage-delivery tick | reads `DamageDeliveryTimer` (+0x38/+0x40) |
| `FUN_00629F40` | `ParasiteClass::ExitUnit` | callee of AI / ReceiveDamage |
| `0x00629D00` | `ParasiteClass::PointerGotInvalid` (victim died / removed) | writes `victim+0x694` |
| `FUN_004D7250` | `FootClass::ReceiveDamage` — parasite suppression | reader of `SuppressionThreshold` (+0xD6C) |
| `FUN_0071A1A0` | `TemporalClass::Fire` | callee of Detonate's `Temporal` branch |
| `0x00719A70` (carved) | `TemporalClass::Update` (per-frame warp) | writes `WarpRemaining` (+0x48) / `WarpPerStep` (+0x4C) |
| `FUN_00719E20` | `TemporalClass::GetWarpPerStep` (helper chain, ≤ 50 deep) | callee of Update |
| `0x00718600…0x718AB3` | `TeleportLocomotionClass::Process` (warp delay + relocation) | reader of `ChronoDistanceFactor` (+0xBF4) etc. |
| `FUN_00519449` (`0x51A2E3…0x51A3F0`) | `InfantryClass` C4 plant on reaching the target building | writer of `C4Applied` (+0x6DF) = 1, reader of `C4Delay` (+0x1750) |
| `FUN_0043FAA0` (`0x440152…0x4402F2`) | `BuildingClass::AI` C4 detonation | reader of `C4Applied`, `C4Timer` (+0x528) |
| `FUN_00438DF0` | `BombListClass::Plant` | reader of `IvanTimedDelay` (+0xFD0) |
| `FUN_004389F0` | `BombClass::TimeToExplode` | reads `DetonationFrame` (+0x38) |
| `FUN_004386A0` | `BombClass::Detonate` | reader of `IvanDamage` (+0xFCC) |
| `FUN_006FD210` | `TechnoClass::Fire` — the `Suicide` early-out | reads weapon +0x144 |
| `FUN_00700DA0` | `TechnoClass::ReceiveDamage` — death → death weapon / bomb detonation | reads type `Explodes` (+0xD15) |
| `FUN_0070CAC0` | `TechnoClass::FireDeathWeapon` | reads type `DeathWeapon` (+0xD18), Rules `DeathWeapon` (+0xFDC) |
| `FUN_00745250` | `UnitClass::ReceiveGunner` | reads passenger type `IFVMode` (+0x688) |
| `FUN_0070D0A0` | `TechnoClass::SetCurrentWeapon(idx)` (0 ≤ idx < 18 else 0) | callee of ReceiveGunner |
| `FUN_004DE620` | `FootClass::RemoveFirstPassenger` → `RemoveGunner` when empty | reads type `Gunner` (+0x805) |

## Offsets recovered (string-anchored loader stores)

| Key | Owner | Offset | Default (ctor/Rules init) |
|---|---|---|---|
| `IFVMode` | TechnoType | +0x688 | 0 |
| `DeployFire` | TechnoType | +0x6AC | no |
| `Gunner` | TechnoType | +0x805 | no |
| `Teleporter` | TechnoType | +0xCD4 | no |
| `Explodes` | TechnoType | +0xD15 | no |
| `DeathWeapon` | TechnoType | +0xD18 | none |
| `ImmuneToRadiation` | TechnoType | +0xD37 | no |
| `Parasiteable` | TechnoType | +0xD38 | **infantry yes** (`0x52363A` InfantryType ctor writes 1); buildings no (`0x45E4E0`) |
| `Warpable` | TechnoType | +0xD3A | yes |
| `SuppressionThreshold` | TechnoType | +0xD6C | 0 |
| `Suicide` | WeaponType | +0x144 | no |
| `RadLevel` | WeaponType | +0x158 | 0 (PROBABLE — the ctor store was not isolated; every weapon without the key behaves as 0 in Detonate's `RadLevel > 0` test) |
| `IvanBomb` / `Parasite` / `Temporal` | WarheadType | +0x157 / +0x159 / +0x15A | no |
| `CellSpread` | WarheadType | +0x124 (float) | 0 |
| `ChronoDelay`/`ChronoDistanceFactor`/`ChronoTrigger`/`ChronoMinimumDelay`/`ChronoRangeMinimum` | Rules | +0xBEC/+0xBF4/+0xBF8/+0xBFC/+0xC00 | 60?/**32**/yes/0/0 (`0x666071…`) |
| `C4Warhead` | Rules | +0xFA8 | — |
| `IvanWarhead`/`IvanDamage`/`IvanTimedDelay` | Rules | +0xFC8/+0xFCC/+0xFD0 | —/**100**/**450** (`0x6665C8`) |
| `C4Delay` | Rules | +0x1750 (double, MINUTES) | 0.03 |
| `RadDurationMultiple`/`RadApplicationDelay`/`RadLevelMax`/`RadLevelDelay`/`RadLightDelay` | Rules | +0x1804/+0x1808/+0x180C/+0x1810/+0x1814 | — |
| `RadLevelFactor` | Rules | +0x1818 (double) | — |
| `RadSiteWarhead` | Rules | +0x1834 | — |

## Ground truth — the real `rulesmd.ini` on this install

```ini
[Radiation]  RadDurationMultiple=1 RadApplicationDelay=16 RadLevelMax=500
             RadLevelDelay=90 RadLightDelay=90 RadLevelFactor=0.2 RadSiteWarhead=RadSite
[RadSite]    Verses=100%,100%,100%,50%,10%,10%,0%,0%,0%,100%,100%
[General]    ChronoDistanceFactor=48 ChronoTrigger=yes ChronoMinimumDelay=16 ChronoRangeMinimum=0
[CombatDamage] C4Delay=.03 C4Warhead=Super  IvanWarhead=IvanWH IvanDamage=450 IvanTimedDelay=450
[Super]      Verses=100% x11
[IvanWH]     Verses=100,100,100,100,100,100,100,250,20,100,100  CellSpread=1.5 PercentAtMax=.25

[DRON] Primary=DroneJump Armor=special_1 Strength=100 SuppressionThreshold=5 Parasiteable=no ImmuneToRadiation=yes
[DroneJump] Damage=50 ROF=60 Range=1.83 Projectile=JUMP Warhead=Parasite LimboLaunch=yes
[Parasite]  Verses=100%,100%,100%,100%,100%,100%,0%,0%,0%,0%,0%  Parasite=yes

[DESO] Primary=RadBeamWeapon Secondary=RadEruptionWeapon Deployer=yes DeployFire=yes ImmuneToRadiation=yes IFVMode=9
[RadBeamWeapon]     Damage=125 ROF=50 Range=6 Warhead=RadBeamWarhead IsRadBeam=yes     (no RadLevel)
[RadEruptionWeapon] Damage=1 ROF=60 Range=4 AreaFire=yes FireOnce=no Warhead=RadEruptionWarhead RadLevel=500
[RadEruptionWarhead] Verses=100,100,100,20,10,10,0,0,0,100,100 CellSpread=10 CellInset=3

[CLEG] Primary=NeutronRifle Teleporter=yes Strength=125 IFVMode=10
[NeutronRifle] Damage=8 ROF=120 Range=5 Warhead=ChronoBeam
[ChronoBeam]   Verses=100% x10,0%  Temporal=yes

[TANY]  Primary=DoublePistols Secondary=Sapper C4=yes IFVMode=4
[GHOST] (Navy SEAL, Image=SEAL) Primary=MP5 Secondary=Sapper C4=yes IFVMode=4
[Sapper] Damage=2500 ROF=100 Range=1.5 Warhead=Mechanical

[IVAN]  Primary=IvanBomber Explodes=yes (no DeathWeapon) IFVMode=7
[IvanBomber] Damage=400 ROF=50 Range=1.5 FireOnce=yes Warhead=IvanBomb
[IvanBomb]   IvanBomb=yes

[TERROR] (Image=TRST) Primary=TerrorBomb Explodes=yes DeathWeapon=TerrorBomb
         CanPassiveAquire=no CanRetaliate=no IFVMode=11
[TerrorBomb]   Damage=225 Range=1.5 ROF=10 Suicide=yes Warhead=TerrorBombWH
[TerrorBombWH] Verses=150,100,100,90,50,50,100,150,30,100,100 CellSpread=2 PercentAtMax=.5

[FV] Primary=HoverMissile Passengers=1 Gunner=yes WeaponCount=17
     Weapon1=HoverMissile Weapon2=RepairBullet Weapon3=CRM60 Weapon4=CRFlakGuyGun
     Weapon5=CRMP5 Weapon6=AWPE Weapon7=CRElectricBolt Weapon8=CRNuke
     Weapon9=CRMindControl Weapon10=CRRadBeamWeapon Weapon11=CRNeutronRifle
     Weapon12=CRTerrorBomb ... Weapon17=CRMissileLauncher
[E1]/[E2] IFVMode=2  -> Weapon3=CRM60 (Damage=25 ROF=15 Range=6 Warhead=SSA)
[CRNuke]  Damage=200 Range=1 RadLevel=500 Warhead=CRNUKEWH Suicide=yes
```

---

## A. Radiation (Desolator deploy) — **CONFIRMED**

### A1. Creation (`BulletClass::Detonate`, `0x469140…0x4691FB`)
```
if (weapon.RadLevel > 0) {                          // weapon +0x158
    cell   = coord >> 8;                            // the detonation cell
    spread = ftol(weapon.Warhead.CellSpread);       // FLD [WH+0x124]; CALL __ftol
    site   = cell.RadSite;                          // FUN_00487B90
    if (!site) { site = new RadSite; SetBaseCell(cell); SetSpread(spread);
                 SetRadLevel(weapon.RadLevel); Activate(); cell.RadSite = site; }
    else         site->Add(weapon.RadLevel);
}
```
- **Spread is the warhead's `CellSpread`, truncated**: `RadEruptionWarhead CellSpread=10` → a **10-cell** site. `SetSpread` (`0x65AF10`): `Spread=n; SpreadInLeptons = n*256 + 128`.
- `SetRadLevel(L)` (`0x65AF30`): `RadLevel=L; RadDuration = RadTimeLeft = L * RadDurationMultiple`.
- `Add(L)` (`0x65AF70`): `cur = ftol(RadLevel * RadTimeLeft / RadDuration)` (FILD/FMUL/FDIV, x87 doubles); `Deactivate()`; `RadLevel = cur + L`; duration/time-left re-armed to `RadLevel * RadDurationMultiple`; `Activate()`. The repeated `FireOnce=no` eruption therefore **accumulates**: a desolator re-firing every 60 frames grows the level by `500 − 60` per shot. Damage is capped by `RadLevelMax` at read time (§A4), not here.

### A2. Activate / Radiate (`0x65AFC0`, `0x65B400`)
```
LevelTimer  = {start: Frame, left: RadLevelDelay}          // 90
LightTimer  = {start: Frame, left: RadLightDelay}          // cosmetic
LevelSteps  = RadDuration / RadLevelDelay                  // int division
for cell in square(BaseCell, Spread):                      // (2S+1)^2 cells
    d   = ftol(|cell.center - base.center|)                // 3-D leptons, sqrt via FUN_004CAB60
    amt = d <= SpreadInLeptons ? (SpreadInLeptons - d) / SpreadInLeptons * RadLevel : 0.0
    cell.RadLevel += amt                                   // double
```

### A3. Decay — RadSite AI (`0x65B240`, carved)
```
RadTimeLeft -= 1;                                           // every frame
if (LevelTimer expired) {                                   // Frame - start >= left
    DecreaseRadiation();  // each cell: RadLevel -= amt / LevelSteps; clamp to 0 (0x487C10)
    LevelTimer = {Frame, RadLevelDelay};
}
(light timer: cosmetic, skipped)
if (RadTimeLeft <= 0) delete this;                          // vtbl+0x20(1); clears cell.RadSite
```
`Deactivate` (`0x65B590`, used only by `Add`) removes `amt / LevelSteps * (RadTimeLeft / RadLevelDelay + 1)` per cell (the integer quotient is the number of decrements still owed, +1).

### A4. Damage application — `FootClass::AI` (`0x4DA4E0…0x4DA539`)
```
if (Frame % RadApplicationDelay == 0                        // absolute phase, 16
    && !Type.ImmuneToRadiation && !InAir && !InLimbo) {
    lvl = ftol(min(cell.RadLevel, RadLevelMax));           // 0x487BC0
    if (lvl > 0) {
        dmg = ftol(lvl * RadLevelFactor);                   // FILD; FMUL [Rules+0x1818]; __ftol
        ReceiveDamage(&dmg, 0, RadSiteWarhead, attacker=0,
                      ignoreDefenses=0, preventPassengerEscape=1, house=0);
    }
}
```
**Foot units only** (infantry/vehicles; buildings are never irradiated). Armor applies through `[RadSite] Verses` (infantry 100 %, light 50 %, medium/heavy 10 %).

**Rating: CONFIRMED** (every line above is a disassembled instruction sequence).
Deviation the clone takes: distances are **2-D** (no Z), the same documented
divergence as `combat.md` §6.

## B. Terror Drone (parasite) — **CONFIRMED** (one PROBABLE)

### B1. Infect (`0x62A420` TryInfect, `0x62A380` CanInfect)
CanInfect(victim): victim not in limbo (+0x81), alive (+0x90), Health > 0, `ParasiteEatingMe == 0` (+0x694), **`Type.Parasiteable`** (+0xD38), and not already the target of something at +0x2E4. On success the drone stays in limbo (`LimboLaunch=yes` — it *became the bullet*), `victim.ParasiteEatingMe = drone`, `parasite.Victim = victim`, and **`DamageDeliveryTimer = {Frame, 0}` — already expired**, so the first damage tick is immediate. On failure the drone is un-limboed back near the target cell.
Defaults: InfantryType ctor sets `Parasiteable=1` (`0x52363A`) → **infantry are infectable**; BuildingType ctor clears it (`0x45E4E0`); `[DRON] Parasiteable=no` (a drone cannot ride a drone).

### B2. The damage tick — `ParasiteClass::AI` (`0x629A50`, carved)
```
if (!DamageDeliveryTimer.expired) return;
DamageDeliveryTimer = {Frame, weapon.ROF};                  // DroneJump ROF=60
if (victim.WhatAmI == Infantry)
    dmg = victim.Health;  ReceiveDamage(&dmg, 0, Parasite, drone, ignoreDefenses=1, 1, 0);  // instant kill
else
    dmg = weapon.Damage;  ReceiveDamage(&dmg, 0, Parasite, drone, ignoreDefenses=0, 1, 0);  // 50 x Verses
```
So a drone **kills infantry on its first tick** (the infantry case uses the victim's *current* health with defenses ignored) and chews vehicles for `50 × Verses` every **60 frames**.

### B3. Suppression and host death (`0x4D7250`, `0x629D00`)
- In the host's `ReceiveDamage`, a hit from anyone but the drone with `damage > SuppressionThreshold` arms the parasite's `SuppressionTimer = 2*damage − threshold` frames. A negative (healing) hit arms it for 50 frames and **ejects** the drone.
- When the host dies (`PointerGotInvalid`): if the SuppressionTimer is still running the drone is **destroyed with it**; otherwise it is re-placed on the map at a free spot near the host (`FUN_005678D0`, radius search) — if no spot exists the drone is destroyed.
- **PROBABLE**: the clone cannot see individual hits on the host without touching `combat.cpp`'s damage path, so suppression is measured as the host's **per-frame external HP loss** (all non-drone damage summed per frame) — exact for one hit per frame.

## C. Chrono Legionnaire — **CONFIRMED** (teleport) / **CONFIRMED** (erase)

### C1. Teleport — `TeleportLocomotionClass::Process` (`0x718761…0x718AB3`)
```
dist  = ftol(|dest - here|)                                 // 3-D leptons
delay = 0;
if (ChronoTrigger) delay = dist / ChronoDistanceFactor;     // IDIV (integer)
delay = max(delay, ChronoMinimumDelay);
if (dist < ChronoRangeMinimum) delay = ChronoMinimumDelay;
unit.IsWarpingIn (+0x271) = 1;  parasite ejected (0x629F40);
Mark_All_Occupation_Bits(0) at the old spot; SetLocation(dest); mark at the new spot;
warp-out/in anims + ChronoOut/In sounds; StopMoving.
```
The relocation is **immediate**; the unit then sits frozen (`IsWarpingIn`) until the timer expires. With this install's `48 / 16 / 0`: a 10-cell hop (2560 leptons) = 53 frames; anything under 768 leptons = 16 frames.
"Frozen" (cannot move or fire while `IsWarpingIn`): **PROBABLE** — the flag is set here and tested by the fire/move paths, but the readers were not individually unwound.

### C2. Erase — `TemporalClass::Fire` (`0x71A1A0`) + `Update` (`0x719A70`)
- Fire: only if the target type is `Warpable` (+0xD3A) and the firer is not itself being warped. The first legionnaire becomes the primary: `target.TemporalTargetingMe = this; WarpRemaining = TargetType.Strength * 10` (**type** Strength, `+0xA0`, ×10). A second legionnaire on the same target joins the helper chain instead. The target's `BeingWarpedOut` (+0x270) is set → it stops acting.
- Update, **every frame**: `WarpPerStep = owner.CurrentWeapon.Damage` (+ each helper's, recursively, ≤ 50); `WarpRemaining -= WarpPerStep`; at `≤ 0` the target is **removed from the game** (`UnInit`, not `ReceiveDamage` — no death weapon, no survivors, its bomb is dropped).
- Letting go (owner ordered elsewhere / killed): the link is cut and the victim un-frozen; a later re-fire starts from a fresh `Strength*10`.
- `NeutronRifle Damage=8`: a GI (Strength 125) is erased in `ceil(1250/8) = 157` frames.
- Damage immunity of a warped target: `ReceiveDamage` early-outs on a vtable predicate (`+0x1D4`) — **PROBABLE** that this is "being warped out"; the clone does not model the immunity (cut, §Deviations).

## D. C4 (Tanya / Navy SEAL) — **CONFIRMED**

### D1. Plant (`0x51A2E3…0x51A3F0`)
When the `C4=yes` infantry reaches its target building and the building is **not selling** (`Mission != 0x13`) and not iron-curtained:
```
if (!building.C4Applied) {
    building.C4Applied = 1;
    building.C4Timer   = {Frame, ftol(C4Delay * 900.0)};    // [0x7E17F8] = 900.0  -> 0.03 min = 27 frames
    building.C4AppliedBy = infantry;
}
infantry scatters (mission reset)
```
### D2. Detonation — `BuildingClass::AI` (`0x440152…0x4402F2`)
```
if (C4Applied && C4Timer.expired) {                        // Frame - start >= 27
    dmg = Health;
    ReceiveDamage(&dmg, 0, C4Warhead /*Super*/, C4AppliedBy, ignoreDefenses=1, 0, 0);   // always lethal
}
```
### D3. The sell refusal
`Sell_Back` refuses while `C4Applied` (+0x6DF) — `repair-sell.md` §B5. The clone's `can_be_sold(…, c4_applied)` hook (always `false` until now) is wired to `Building::c4_applied`.

## E. Crazy Ivan bombs — **CONFIRMED**

- Plant (`0x438DF0`): only an **infantry** planter, only if the target has **no `AttachedBomb`** (+0x38). `DetonationFrame = Frame + IvanTimedDelay` (450).
- `TimeToExplode` (`0x4389F0`): `!DeathBomb && DetonationFrame < Frame && !Harmless` — strict, so the blast is on `plant + 451`.
- `Detonate` (`0x4386A0`): `DamageArea(target.coord, IvanDamage=450, owner, IvanWarhead, …)` — the general area applicator: `IvanWH CellSpread=1.5 PercentAtMax=.25`, **every house** in the radius, buildings included (250 % vs steel).
- The bombed object dying first detonates the bomb at once (`TechnoClass::ReceiveDamage` death path → `0x701B0A`).
- `FireOnce=yes` → the Ivan's attack mission ends after the plant.
- **Ivan's own death does nothing.** `Explodes=yes` with no `DeathWeapon` falls back to the Primary `IvanBomber`, whose `IvanBomb` warhead short-circuits `Detonate`'s chain into `Plant`, which needs a techno target a death-shot does not have. ("Damage is used only for death explosion" in the INI comment is obsolete — "deathbombs cut".)

## F. Terrorist / suicide weapons — **CONFIRMED**

- `TechnoClass::Fire` (`0x6FD27C`): a `Suicide=yes` weapon **fires no bullet** — the firer receives `Type.Strength` damage with `C4Warhead`, ignoring defenses, and returns.
- The death path (`0x701A23`): `Explodes=yes` **or** the current weapon is `Suicide` → `FireDeathWeapon(0)` (`0x70CAC0`): weapon = `Type.DeathWeapon`, else the current weapon, else `Rules.DeathWeapon`; a bullet is built and **detonated in place** (`Detonate` at the dying unit's own coord). So the terrorist's attack is: walk into range (1.5), die, `TerrorBomb` 225 / `TerrorBombWH` 2-cell, 50 % at the edge. Killing a terrorist triggers the same blast.
- `CanPassiveAquire=no` (no auto-acquire) and `CanRetaliate=no` are honored.
- Death-weapon damage = `ftol(Damage × firepower multiplier)` — rank-0 multiplier 1.0 (**PROBABLE**; the FPU operand was not isolated).

## G. IFV weapon swap — **CONFIRMED**

`UnitClass::ReceiveGunner` (`0x745250`): `SetCurrentWeapon(passenger.Type.IFVMode)`; `SetCurrentWeapon` (`0x70D0A0`) accepts `0 ≤ idx < 18`, else 0, and also selects the matching turret. `RemoveFirstPassenger` (`0x4DE620`) calls `RemoveGunner` when a `Gunner=yes` transport is left empty → weapon 0. `IFVMode` indexes the type's **0-based** weapon list (`Weapon1=` is index 0): GI/Conscript (2) → `CRM60`, SEAL/Tanya (4) → `CRMP5`, Ivan (7) → `CRNuke` (Suicide + RadLevel 500 — the IFV kamikazes into a mini-nuke), Desolator (9) → `CRRadBeamWeapon`, CLEG (10) → `CRNeutronRifle` (the IFV *erases*), Terrorist (11) → `CRTerrorBomb`. Infantry without `IFVMode` → 0 (`HoverMissile`).

## H. Cross-cutting observation (NOT acted on this phase)

`TechnoClass::ReceiveDamage` (`0x700DA0`) clamps a non-negative, defended hit to **at least 1** after the armor multiply (`if (*dmg < 1) *dmg = 1`). The clone's `damage_vs` truncates to 0. Changing it would move every existing gate; it is recorded here for a future combat-fidelity pass and the abilities use the clone's existing `damage_vs` for consistency.

---

## Confidence summary

| § | Item | Rating |
|---|---|---|
| A1–A4 | RadSite creation / Add / spread / falloff / decay / application cadence + formula | **CONFIRMED** |
| B1–B2 | infect conditions, immediate first tick, 60-frame tick, infantry instant kill | **CONFIRMED** |
| B3 | suppression arithmetic; host-death drone fate | **CONFIRMED** (per-frame aggregation in the clone: PROBABLE) |
| C1 | teleport delay formula + immediate relocation | **CONFIRMED**; frozen-while-warping PROBABLE |
| C2 | `Strength*10`, per-frame `WarpPerStep` = weapon Damage (+helpers), erase = UnInit | **CONFIRMED**; warped-target damage immunity PROBABLE (not modeled) |
| D1–D3 | C4 timer `ftol(C4Delay*900)`, lethal ignore-defense blast, sell refusal | **CONFIRMED** |
| E | plant rules, `plant+451` strict detonation, area blast, chain on target death, Ivan death = nothing | **CONFIRMED** |
| F | suicide = self-kill + death weapon; Explodes death weapon; firepower multiplier | **CONFIRMED** / multiplier PROBABLE |
| G | IFVMode → weapon index, 0..17, reset to 0 on empty | **CONFIRMED** |

## Deviations / cuts the clone takes (each justified)

1. **Instant hits** (no projectile flight): the drone's `JUMP` arc, the legionnaire's beam and the C4/Ivan approach resolve when `combat.cpp` would fire. Projectile flight is being built on another branch; abilities do not depend on it.
2. **2-D distances** (radiation falloff, teleport delay) — the existing `combat.md` §6 divergence.
3. **Area damage** (Ivan / terrorist / death weapons / eruption) uses `CellSpread*256` leptons with the clone's adjudicated linear `100 % → PercentAtMax` falloff (`superweapon.md` / `combat.md` §7) over **all houses**, units *and* buildings (building distance = nearest foundation cell). The firing unit itself is excluded from its own blast.
4. **Cut — needs unbuilt systems:** parasite removal by Service Depot / IFV repair (no unit-repair system), drone vs naval squid grapple, C4 on ships, Ivan bombs on friendlies (`AttackCursorOnFriendlies` — the clone's attack orders refuse friendlies), player-detonated bombs (`CanDetonateTimeBomb=no` anyway), legionnaire leash in an open-topped transport, mind control (`CRMindControl`), radiation lighting/tint, warp/erase/bomb/C4 animations and sounds.
5. **Not modeled:** damage immunity of a target being erased (§C2 PROBABLE) and the min-1 damage clamp (§H).
