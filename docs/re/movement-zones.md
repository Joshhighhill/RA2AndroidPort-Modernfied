# MovementZone / SpeedType: which units cross water (W4-MC-01)

Source: `gamemd.exe` (YR 1.001), private Ghidra copy `D:\ra2-re\w4fix-ctl`, script
`D:\ra2-re\scripts\w4fixctl\s1.py`, output `D:\ra2-re\w4fix-ctl-out\_s1.txt`.

## 1. The MovementZone name table (CONFIRMED)

The INI reader parses `MovementZone=` against a pointer table at `0x0081AA98`
(the string `AmphibiousDestroyer` at `0x0081AB48` is referenced from `0x0081AAA4`):

| Value | Name | Value | Name |
|---|---|---|---|
| 0 | Normal | 7 | Infantry |
| 1 | Crusher | 8 | InfantryDestroyer |
| 2 | Destroyer | 9 | Fly |
| 3 | **AmphibiousDestroyer** | 10 | Water |
| 4 | **AmphibiousCrusher** | 11 | WaterBeach |
| 5 | **Amphibious** | 12 | CrusherAll |
| 6 | Subterannean (sic) | | |

The three `Amphibious*` zones are the land-and-water zones; `Destroyer` /
`Crusher` only add the crush/destroy behaviour on top of the domain.

## 2. The units

| Type | MovementZone | SpeedType |
|---|---|---|
| TANY (Tanya), GHOST (SEAL), PCOMMANDO, JFISH | AmphibiousDestroyer | Amphibious |
| CPHAL | AmphibiousDestroyer | (none) |
| APC (Amphibious Transport) | AmphibiousCrusher | Amphibious |
| LCRF, SAPC, HVR | Amphibious | Hover |

## 3. Engine rule

`ra2engine::is_amphibious_zone(MovementZone, SpeedType)` (`engine/unit_combat.hpp`)
is true for any of the three `Amphibious*` zones, or for `SpeedType=Amphibious`.
Such a unit gets `MovementDomain::Amphibious` (the land OR water grid). An amphibious
unit takes the water exit ring only when it leaves a **naval** factory (`Factory=UnitType`
with `Naval=yes`); Tanya and the SEAL leave the barracks on land. Infantry standing in
a water cell plays the `Swim` / `WetIdle` sequences (`inf_anim.cpp`, from
`World` `in_water`).

Before this fix only the exact string `Amphibious` matched, so Tanya and the SEAL
were land-bound and ALL01's GI hand-over (the player's unit entering the water cells
between the pier and Liberty Island) could never fire.
