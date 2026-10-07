# Wave-5 unit and building abilities: reverse-engineering notes

Binary: YR 1.001 `gamemd.exe` (image base `0x00400000`, SHA1
`7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`). Project copies `D:\ra2-re\w5-abil*`
(outside the repo). Every offset below was reached from an INI-key string
reference in this binary; YRpp/Ares were used for naming only.

Code: `engine/abil_w5.hpp` (data), `engine/abil_w5_rules.cpp` (rulesmd ->
type data), `engine/world_w5.cpp` (the World pass), plus the touched sites
named in each section.

## §1 Spy infiltration dispatch and stolen tech (R-042) — CONFIRMED

`BuildingClass::Infiltrate(house)` = `FUN_004571E0` (sole caller the enter
executor `FUN_00519449` @ `0x519DF3`). Returns at once when the building's
owner is the spy's house. Then it tests the building TYPE in this order (the
first match wins):

| Order | Test | Anchor | Effect |
|---|---|---|---|
| 1 | `Radar` (+0x16A4) set | loader `FUN_0045FE40` stores `"Radar"` at +0x16A4 | `FUN_0050BB30` -> reset the OWNER's shroud (`FUN_005775E0`) unless latched (+0x577A) |
| 2 | `Power` (+0xEE0) >= 1 | | `FUN_0050BAB0(Rules->SpyPowerBlackout)` |
| 3 | type is in Rules list +0x920 (count +0x92C) | `FUN_00672520` reads `"BuildTech"` into the vector at Rules+0x91C | by `AIBasePlanningSide` (+0x6D0, `FUN_00711510` param[0x1B4]): 0 -> house+0x2BE, 1 -> house+0x2BD, else house+0x2BC (YRpp `Side0/1/2TechInfiltrated`) |
| 4 | `SuperWeapon` (+0x16F0) != -1 | | `FUN_006CD620`: reset THAT superweapon's charge |
| 5 | `Storage` (+0x800) > 0 | `FUN_00711510` param[0x200] = `"Storage"` | steal `SpyMoneyStealPercent` of the victim's money |
| 6 | `Factory` (+0xEB8) == 0x28 (`UnitType`) | loader stores `"Factory"` at +0xEB8 | house+0x2C0 (`WarFactoryInfiltrated`) |
| 7 | `Factory` == 0x10 (`InfantryType`) | | house+0x2BF (`BarracksInfiltrated`) |

`[AI] BuildTech=NATECH,GATECH,YATECH` in this install, so a spy in an Allied
Battle Lab (AIBasePlanningSide=0) gives `Side0TechInfiltrated`, a Soviet one
`Side1`. A unit with `RequiresStolenAlliedTech/SovietTech/ThirdTech` is
buildable only while the house holds every matching flag (YRpp
`HouseClass::HasAllStolenTech`): CCOMAND (Allied tech), CIVAN (Soviet tech),
PTROOP (Yuri tech, unreachable in an Allied/Soviet game).

Consequences in the clone:
- `fill_building_abilities` resolves `BldTypeW5::spy_purpose` in this exact
  order from rulesmd, so the gesture layer and the AI use the binary's
  dispatch. The old id list mapped `GACHRO` (no such type; the Chronosphere
  is `GACSPH`) and treated the Battle Labs as radars.
- The Naval Yard is `Factory=UnitType`, so it sets the same flag as a War
  Factory (vehicles and ships become veterans).
- The SW reset now resets only the infiltrated building's superweapon.
- `RequiresStolen*` types stay in the producible rosters (player and AI) with
  TechLevel 0 until `World::apply_stolen_tech_gate` sees the stolen bits.
  The stolen bits are saved (`WorldW5::stolen_tech`).
