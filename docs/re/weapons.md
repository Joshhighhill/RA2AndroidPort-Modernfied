# RE-W1/W3 — TechnoType weapon slots, weapon choice, DeployFire

Binary `gamemd.exe`, private Ghidra project `D:\ra2-re\fixC`, scripts
`fixC_12..17.py`, dumps `D:\ra2-re\fixCout\wload_00711510.c`, `w1*.log`.

## §1 Slot loading — `FUN_00711510` (TechnoTypeClass::LoadFromINI) [CONFIRMED]

Every weapon key (`Primary`, `Secondary`, `ElitePrimary`, `EliteSecondary`,
`WeaponCount`, `Weapon%d`, `EliteWeapon%d`, `DeployFireWeapon`,
`NavalTargeting`, `LandTargeting`) has exactly one reader: this function.
Type layout: normal slots at `type+0x898 + i*0x1C` (dword index `0x226`),
elite slots at `type+0xA94 + i*0x1C` (`0x2A5`).

```
TurretCount = ReadInt(+0x808)            // dword 0x202
WeaponCount = ReadInt(+0x80C)            // dword 0x203
IsChargeTurret = ReadBool(+0x810)
ClearAllWeapons = ReadBool(+0xA90)
if (TurretCount < 1) {
    if (!ClearAllWeapons) {
        slot[0]  = Primary        (default: current)
        slot[1]  = Secondary
        elite[0] = ElitePrimary
        elite[1] = EliteSecondary
    }
} else if (WeaponCount > 0) {
    for N in 1..WeaponCount:
        slot[N-1]  = Weapon%d(N)
        elite[N-1] = EliteWeapon%d(N)
}
if (ClearAllWeapons) slot[0]=slot[1]=elite[0]=elite[1]=0
DeployFireWeapon = ReadInt(+0x6A8, def 1)   // dword 0x1AA
```

Consequences:
- **SREF Prism Tank** (`; Primary=Comet` commented out, `TurretCount=4`,
  `WeaponCount=1`, `Weapon1=Comet`, `EliteWeapon1=SuperComet`) fires **Comet**
  from slot 0. The clone read only `Primary=` and so gave it no weapon.
- **FV IFV** (`TurretCount=4`, `WeaponCount=17`): slot 0 = `Weapon1=HoverMissile`;
  `Weapon2..17` are the Gunner modes (P4 `ifv_weapons`), **not** a secondary.
  The clone therefore drops slot 1 for every `TurretCount >= 1` type.
- The elite slots carry their own weapon **and projectile**
  (`EliteWeapon1=SuperComet` / `ElitePrimary=...E`).

## §2 Weapon choice per target [RE-FALLBACK]

The body of `TechnoClass::SelectWeapon` ("WhatWeaponShouldIUse") was **not
located** in this build: the YRpp/Ares address `0x6F3330` falls mid-instruction
inside an FLH helper here (`FUN_006f32c0`), and the callers of the GetWeapon
virtual (`vtbl+0x3F8`, confirmed by `FUN_006f34a0` / `FUN_00741fa0`) with
constant indices are threat/range aggregators, not the chooser.

What is confirmed and used:
- `GetFireError` (`FUN_006fb5d0`, combat.md §2) refuses a weapon whose warhead
  `Verses[targetArmor] == 0`.
- `FUN_00741fa0` builds the unit's target-type mask as the OR of
  `GetThreatFlags(weapon)` (`FUN_007718b0`) over slots 0 and 1 — a unit can
  acquire whatever EITHER weapon can hit; the projectile's `AA=`/`AG=` flags
  decide air vs ground.

The clone's rule (`engine/weapon.cpp choose_weapon`): **slot 0 if it can
engage the target (AA for an airborne target / AG for a ground one, and
non-zero Verses), else slot 1 if it can, else none.** This reproduces every
stock pairing the gap report lists: HTK `FlakTrackGun`/`FlakTrackAAGun`, APOC
`120mmx`/`MammothTusk`, FLAKT `FlakGuyGun`/`FlakGuyAAGun`, GGI `M60` /
`MissileLauncher` (deployed), and leaves `TESLA OPCoilBolt` / `ATESLA
PrismSupport` (charge/support secondaries) unused because the primary always
qualifies.

AA/AG defaults when a projectile omits the key: `AA=no`, `AG=yes`
[RE-FALLBACK: the BulletType loader sits in an unanalysed gap
(voxel-render.md §7); every stock AA projectile states `AA=yes` explicitly].

## §3 DeployFire (GI / Guardian GI) [CONFIRMED key, RE-FALLBACK path]

`DeployFireWeapon` (default **1**) is the slot a deployed `Deployer=yes
DeployFire=yes` infantry fires. GI: slot 1 = `Para`; GGI: slot 1 =
`MissileLauncher` (AA+AG). The clone's P4 model treated every such type as the
Desolator (area eruption at its own cell every ROF); the Desolator differs only
in that its deploy weapon carries `RadLevel` (the radiation eruption). Rule now:
- deploy weapon without `RadLevel` → **DeployFire**: the deployed unit is
  immobile, keeps acquiring/firing through the normal target path with slot
  `DeployFireWeapon`;
- deploy weapon with `RadLevel` (DESO) → the P4 eruption ability (unchanged).

Whether the deployed infantry may rotate to face (`InfantryClass::AI` deploy
branch) was not unwound; infantry have no turret in the clone, so facing is
cosmetic [RE-FALLBACK].
