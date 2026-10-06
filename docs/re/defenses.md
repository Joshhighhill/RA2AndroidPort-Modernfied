# RE-W2 (+W5) — Base defenses: weapons, range, power, turret

Binary `gamemd.exe`, private Ghidra project `D:\ra2-re\fixC`, scripts
`fixC_18..20.py`, dumps `D:\ra2-re\fixCout\w2*.log`.

## §1 Weapons

Buildings are TechnoTypes: their `Primary=`/`Secondary=` (or `Weapon1..N`)
are loaded by the same `FUN_00711510` slot rule as units (weapons.md §1).
Stock defenses on this install:

| Type | Weapon | Turret | ROT | Powered |
|---|---|---|---|---|
| GAPILL Pillbox | Vulcan2 | — | 10 | no (Power=0) |
| NASAM Patriot | RedEye2 (AA) | yes | 8 | yes (-50) |
| ATESLA Prism Tower | PrismShot (+PrismSupport) | no | — | yes (-75) |
| TESLA Tesla Coil | CoilBolt (+OPCoilBolt) | no | — | yes (-75) |
| NALASR Sentry Gun | Vulcan | yes | 10 | no |
| NAFLAK Flak Cannon | FlakWeapon (AA) | yes | 8 | yes (-50) |
| GTGCAN Grand Cannon | GrandCannonWeapon | yes | 1 | yes (-100) |

The secondaries of the Prism Tower / Tesla Coil are the charge/support
weapons; the weapon choice (weapons.md §2) never selects them because the
primary always qualifies.

## §2 Power — `FUN_004555c0` (building "is operational") [CONFIRMED]

```
if (Type->Powered (+0x1573) && Type->PowerDrain (+0xEE4) > 0
    && House->PowerFraction() (FUN_004fcc50) < 1.0 (DAT_007e0718)
    && this->Overpowered (+0x67C) < 2)
    return 0;                       // not operational -> no firing
```

`Powered` is read by the BuildingType loader `FUN_0045fe40` into `+0x1573`.
So a `Powered=yes` defense stops working whenever its house's output/drain
ratio is below 1 (drain > output), unless charged by >= 2 Tesla Troopers
(`Overpowered`, not modelled). A spy blackout (output forced to 0) has the
same effect (`apply_blackout`).

## §3 Range — `FUN_007072e0` = TechnoClass::GetGuardRange(mode) [CONFIRMED]

```
mode 0: GuardRange (type+0x5B8, leptons) if set, else 0
mode 1/2: r = GuardRange ? GuardRange : max(WeaponRange(0), WeaponRange(1));
          r *= 2;
          mode 2: clamp(r, 0x700, 0x1000); else clamp(r, 0, 0x1000)
```

`GuardRange=` is read in leptons (`FUN_00474550`, cells x 256). The x2 guard
area is the *roaming* radius of a guarding mobile unit (RE-W5; the clone's
`kGuardStray` stand-in is left as is). A defense cannot move, so the clone
acquires within the range of the weapon that can engage the target (or
`GuardRange` when the type sets one). [ANCHORED: the scan function
`Target_Something_Nearby` was not decompiled.]

## §4 Turret

`Turret=yes` defenses slew their turret at `ROT` (the unit `Set_ROT` rate,
voxel-render.md §5) and fire only once on target (rotation-complete gate,
like vehicles). `Turret=no` towers (Prism, Tesla) fire immediately.
[ANCHORED on the vehicle mechanism; the building turret AI was not unwound.]

## §5 What the clone does (engine/building_combat.cpp)

Per frame, for each alive Idle armed building: Powered check (§2) → target
validation/acquisition (nearest visible enemy unit the weapon can engage —
AA weapons engage airborne aircraft, AG ones ground units; 0 % Verses
excluded) → turret slew → fire on ROF (ROF/15 s; jitter per combat G-50) via
the same projectile / instant damage layer as units. A unit hit by a defense
while idle retaliates against the building. Events: `World::take_fire_events`.
Defenses do not target buildings (only explicit orders do, which the player
cannot give a building).
