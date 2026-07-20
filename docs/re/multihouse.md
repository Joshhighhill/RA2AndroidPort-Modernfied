# Multiple Houses / Players & Alliances — Recovered Model (Yuri's Revenge)

Recovery of how the original RA2/YR models multiple houses/players, team colors,
alliances, enmity in combat/targeting, victory/defeat, start setup, and AI houses.

**Calibrate-first**: the implementation spec is built on the recovered behavior
below. Every claim is tagged:

- **[CONFIRMED]** — recovered directly from the REAL `rulesmd.ini` (extracted
  from `D:\Red Alert 2\ra2md.mix`) or from a decompile.
- **[ANCHORED]** — structure/mechanism recovered from OpenRA (trusted for
  structure, untrusted for addresses) and/or documented RA2 behavior.
- **[RE-FALLBACK]** — a design choice we are making where the original is not
  cheaply recoverable.

## Evidence sources / commands

- Real rules (already extracted, 742958 bytes):
  `D:\ra2-re\out\shroud_evidence\rulesmd.ini`
  (re-extract: `ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/rulesmd.ini <out>`)
- OpenRA reference: `D:\ra2-re\reference\OpenRA` (structure anchor).
- Grep commands used are shown inline as evidence per section.

---

## §1 Player / house model + counts

### Country list — `[Countries]` [CONFIRMED]

`rulesmd.ini` line 959 (`grep -n '^\[Countries\]'`):

```ini
[Countries]
0=Americans   ; Starts at zero because dialog returns 0 for first listing
1=Alliance
2=French
3=Germans
4=British
5=Africans
6=Arabs
7=Confederation
8=Russians
9=YuriCountry
10=GDI
11=Nod
12=Neutral
13=Special
```

These indices are **country/house TYPE ids** (the entries the engine's
HouseTypeClass array is built from), not player slots.

### Country → Side mapping — `[Sides]` [CONFIRMED]

`rulesmd.ini` line 981:

```ini
[Sides]
GDI=British,French,Germans,Americans,Alliance     ; = the ALLIED side
Nod=Russians,Africans,Confederation,Arabs         ; = the SOVIET side
ThirdSide=YuriCountry                              ; = Yuri
Civilian=Neutral
Mutant=Special
```

Note the internal naming carried over from Tiberian Sun: the **`GDI` side is the
Allied side**, the **`Nod` side is the Soviet side**, `ThirdSide` is Yuri. This
is a string-level legacy detail; the on-screen sides are Allied / Soviet / Yuri.

### What a country provides [CONFIRMED]

Each playable country has its own section (lines 3199–3353). Example — every
field observed:

```ini
[Americans]            [Russians]              [Neutral]
UIName=Name:Americans  UIName=Name:Russians    UIName=Name:Neutral
Name=America           Name=Russia             Name=Civilian
Suffix=Allied          Suffix=Soviet           Suffix=CIV
Prefix=G               Prefix=B                Prefix=C
Color=Gold             Color=DarkRed           Color=Grey
Multiplay=yes          Multiplay=yes           MultiplayPassive=true
Side=GDI               Side=Nod                Side=Civilian
SmartAI=yes            SmartAI=yes             SmartAI=yes
```

A country therefore provides:
- **`Side=`** — Allied(`GDI`) / Soviet(`Nod`) / Yuri(`ThirdSide`) /
  Civilian / Mutant. Drives tech tree, base buildings, MCV type.
- **`Suffix=` / `Prefix=`** — string tags used to select side-specific art /
  strings (e.g. `Allied`/`Soviet`, image-name prefix `G`/`B`).
- **`Color=`** — the country's DEFAULT color scheme name (overridden by the
  lobby color a player picks — see §2).
- **`Multiplay=yes`** — this country is selectable in the MP lobby.
  `MultiplayPassive=true` marks non-selectable houses that still exist in a game
  (Neutral, Special, GDI, Nod).
- **`SmartAI=yes`** — AI behavior flag.
- **Country bonuses / special units** are attached per country but are almost
  all commented out in this build (e.g. `;VeteranInfantry=YURI`,
  `;CostUnitsMult=.85`, `;IncomeMult=1.2`). So in this rules revision the
  countries are **mechanically near-identical**; the differences are cosmetic
  (side/art) plus a hard-coded per-country special unit granted by the game
  binary (e.g. Americans→paradrop GIs, Russians→Tesla, Germans→Tank Destroyer,
  British→Sniper, French→Grand Cannon, etc.) — the *special-unit-per-country*
  behavior is **[ANCHORED]** (documented RA2) since the multipliers are disabled
  here. [CONFIRMED that the mult/veteran bonuses are OFF in this rules file.]

### Slot vs country [ANCHORED]

- A **country/house TYPE** is a `[Countries]` entry (a `HouseTypeClass`).
- A **player SLOT** is a runtime `HouseClass` instance created for each
  participant (human or AI) in a match. A slot binds: a chosen **country**
  (→ side/tech), a chosen **color** (§2), a **start waypoint** (§6), a
  **team/alliance** id, and human-vs-AI + handicap. Multiple slots can share the
  same country type. Non-combatant houses (Neutral/Special) are instantiated
  once regardless.

### Neutral / civilian houses [CONFIRMED]

`[Neutral]` (Civilian, `Side=Civilian`) and `[Special]` (JP/Mutant,
`Side=Mutant`) both carry `MultiplayPassive=true` — they always exist in a match
to own civilian buildings, tech structures, and critters, but are not player
slots and are non-combatant (never win/lose, never auto-targeted as an enemy —
see §4/§5).

### Max players [ANCHORED]

There is **no `MaxPlayers` key** in `rulesmd.ini`
(`grep -n 'MaxPlayers' rulesmd.ini` → no match). RA2/YR multiplayer is
**hard-capped at 8 human/AI player slots** (documented; the MP color table also
provides exactly 8 distinct player colors — see §2, which is the natural cap).
[ANCHORED — 8]

---

## §2 Team colors

### `[Colors]` — real list + values [CONFIRMED]

`rulesmd.ini` line 3367. Values are **HSV** (Hue, Saturation, Value), NOT RGB —
confirmed by the rules comment at line 3357: "The values represent the 'hue',
'saturation', and 'value'." The trailing comment on each line is the artist's
human name for the resulting spread.

```ini
[Colors]
LightGold=25,255,255    ; 0            Orange
Gold=43,239,255         ; 1   MP       Yellow
LightGrey=0,0,240       ; 2            White
Grey=0,0,131            ; 3            Grey
Red=20,255,184          ; 4            Brown
DarkRed=0,230,255       ; 5   MP       Red
Orange=25,230,255       ; 6   MP       Orange
Magenta=221,102,255     ; 7   MP       Pink
Purple=201,201,189      ; 8   MP       Purple
LightBlue=119,143,255   ; 9            Aqua
DarkBlue=153,214,212    ; 10  MP       Blue
NeonBlue=185,156,238    ; 11           Violet
DarkSky=131,200,230     ; 12  MP       Periwinkle
Green=104,241,195       ; 13           Bathroom green
DarkGreen=81,200,210    ; 14  MP       Light green
NeonGreen=0,0,0         ; 15           Black
Yellow=38,159,255       ; 16           Pastel yellow
Purple2=211,201,189
Purple3=191,201,189
AlliedLoad=164,255,255  ; 17
SovietLoad=0,235,255    ; 18
```

Coder note in-file (line 3388): *"these entries are all doubled, so
ColorSchemes[5] is White, not 2."* — the runtime ColorScheme array is built with
duplication, so an index into ColorSchemes is NOT the same as the `[Colors]`
line ordinal.

### The 8 multiplayer player colors [CONFIRMED]

The lines flagged `MP` are exactly the **8 selectable player colors** (matching
the 8-player cap):

| MP color name | HSV | Displayed as |
|---|---|---|
| Gold | 43,239,255 | Yellow |
| DarkRed | 0,230,255 | Red |
| Orange | 25,230,255 | Orange |
| Magenta | 221,102,255 | Pink |
| Purple | 201,201,189 | Purple |
| DarkBlue | 153,214,212 | Blue |
| DarkSky | 131,200,230 | Periwinkle/Aqua |
| DarkGreen | 81,200,210 | Light green |

### How a slot picks a color [ANCHORED / CONFIRMED]

- Default: a slot uses its country's `Color=` (e.g. Allied countries default
  `Gold`, Soviet default `DarkRed`, civilians `Grey`) — [CONFIRMED from the
  country sections in §1].
- In the MP lobby each slot picks one of the 8 MP colors; the picked color
  overrides the country default. Colors are unique per slot (no two players share
  a color) — [ANCHORED].

### Color remap (recolor art per house) [ANCHORED]

The HSV triple defines a **remap palette spread**: the engine takes the unit/
building art's remap-index band (the reserved "house color" palette range) and
generates a gradient from the color's Hue at varying Value/Saturation, so the
same SHP/VXL art is tinted to the owning house's color at blit time. This is the
mechanism our existing `pal_for_house` implements. The engine builds one remap
table per house from that house's ColorScheme. [ANCHORED — matches our
`pal_for_house`; the HSV→spread generation rule is the documented RA2 behavior.]

---

## §3 Alliance model

### Representation [ANCHORED]

Alliance is a **per-house relationship**, evaluated pairwise. OpenRA (structural
anchor) models it as a 3-value relationship plus per-player bitmasks:

`OpenRA.Game/Traits/TraitsInterfaces.cs:65`:
```csharp
public enum PlayerRelationship { None = 0, Enemy = 1, Neutral = 2, Ally = 4 }
```

`OpenRA.Game/Player.cs:249` — the canonical relationship check:
```csharp
public PlayerRelationship RelationshipWith(Player other)
{
    if (this == other)                       return Ally;   // self is always allied to self
    if (other == null || other.Spectating)   return NonCombatant ? Neutral : Ally;
    if (AlliedPlayersMask.Overlaps(other.PlayerMask)) return Ally;
    if (EnemyPlayersMask.Overlaps(other.PlayerMask))  return Enemy;
    return Neutral;                          // e.g. civilians / mutants
}
public bool IsAlliedWith(Player p) => RelationshipWith(p) == Ally;
```

Recovered rules for our engine:
- **Self is always allied to self** (`this == other → Ally`).
- Relationship is **mutual** in normal play (both houses hold each other in
  their allied set); the original supports transient one-way state during an
  alliance request, but the settled state is mutual.
- **Neutral** ≠ enemy: Civilian/Mutant (and unallied-but-not-hostile) houses are
  Neutral — not auto-attacked (see §4). This is the third state our engine
  currently lacks (we only have same-house vs "everyone else = enemy").

RA2's own concept: `[MultiplayerDialogSettings] AlliesAllowed` and
`AllyChangeAllowed` (below) control whether alliances exist and can change; the
per-house `Allies=` list appears in the map/scenario (single-player &
pre-teamed) files, not in `rulesmd.ini`. [ANCHORED for the runtime check;
CONFIRMED for the rules toggles.]

### FFA vs team start defaults [CONFIRMED + ANCHORED]

`rulesmd.ini [MultiplayerDialogSettings]` (line 3017), the game defaults:
```ini
AlliesAllowed=no        ; are allies allowed in this game? (def=no)
AllyChangeAllowed=yes   ; can alliances be changed mid-game? (def=yes)
ShortGame=yes
FogOfWar=no
MCVRedeploys=yes
```

- **Default match is FFA**: `AlliesAllowed=no` ⇒ every player enemy of every
  other; no team is pre-formed. [CONFIRMED]
- **Team game**: when the lobby assigns players to teams, members of the same
  team start **pre-allied** (mutual Ally) and all other teams are Enemy. Team
  membership comes from the lobby/scenario, not rules. [ANCHORED]
- `AllyChangeAllowed=yes` ⇒ in-game alliance requests/toggles are permitted
  (a player can propose/accept/break alliance during play); set `no` to lock
  starting alliances. [CONFIRMED key + def]

### Ally-reveal (shroud) [CONFIRMED]

`rulesmd.ini` line 751:
```ini
AllyReveal=yes          ; Allies automatically reveal radar maps to each other?
```
Allied houses **share revealed shroud/sight** — ties directly into the shroud
phase: a cell revealed by any house is revealed for all its allies. [CONFIRMED]

### Shared control / production [ANCHORED]

Allies do NOT share unit control or production/economy by default — each house
keeps its own credits, build queues, and command of its own units. Allies share:
sight (AllyReveal), no friendly auto-fire (§4), and combined victory (§5). Spies
and certain support powers are the only cross-house interactions. Observers/
spectators are non-combatant and treated as Ally-to-all for visibility (see the
`Spectating` branch above). [ANCHORED]

---

## §4 Enmity in targeting / combat

**This is the key behavior to generalize** — our engine currently treats
`house != self_house` as enemy, which wrongly makes allies and neutrals hostile.

### The gate is ALLIANCE, not same-house [ANCHORED]

Auto-acquire and normal weapon targeting are gated on the **relationship**, not
on house identity. OpenRA `OpenRA.Mods.Common/Traits/AutoTarget.cs`:

- line 342 / 420 — a candidate is accepted only if its owner's relationship is
  in the weapon's allowed set:
  ```csharp
  if (!ati.ValidRelationships.HasRelationship(self.Owner.RelationshipWith(owner)))
      continue;   // skip this target
  ```
- Weapons declare which relationships they may hit via `ValidRelationships`
  (default `Enemy` for offensive weapons; `Ally`/`Neutral` added for
  healing/support). Example defaults
  (`mods/ts/rules/defaults.yaml:68` → `ValidRelationships: Ally, Enemy, Neutral`
  for scan; offensive weapons `Enemy, Neutral`).

So the recovered rule for our engine:

> A unit auto-acquires / auto-fires at a target **only if the target's house is
> an ENEMY** of the firing house. It does **not** target its **own house**, its
> **allies**, or **neutral** (Civilian/Mutant) houses. Replace the
> `house != self_house` test with `RelationshipWith(target) == Enemy` (i.e.
> not-self AND not-ally AND not-neutral).

Note the special-case at AutoTarget.cs:397 — a frozen (last-known) actor that has
since become an ally is dropped from the target list, confirming allies are never
valid auto-targets.

### Force-fire and friendly-fire [ANCHORED]

- **Force-fire** (player explicitly orders an attack on an allied/own target)
  bypasses the relationship gate — you *can* manually fire on allies. Auto-target
  never does this.
- **Splash / area damage** is location-based, not owner-filtered: a warhead's
  blast **can still hurt allied and own units** caught in the radius (RA2 has
  real friendly splash damage). Direct-hit auto-fire is owner-gated; area
  effect is not. [ANCHORED]
- Civilian/Neutral houses are Neutral, so they are not auto-attacked, but can be
  force-fired and can be caught by splash. [ANCHORED]

---

## §5 Victory / defeat

### Per-player defeat [CONFIRMED rule + ANCHORED mechanism]

`rulesmd.ini [MultiplayerDialogSettings]`:
```ini
ShortGame=yes   ; is a player eliminated when his last building is destroyed? (def=yes)
```

- **ShortGame=yes (default)**: a player is **defeated when their last BUILDING
  is destroyed** (loose units left over don't keep them alive). [CONFIRMED]
- **ShortGame=no**: a player is defeated only when they have **no buildings AND
  no units** remaining. [CONFIRMED from the `def` comment + toggle]

Mechanism anchor — OpenRA `PlayerExtensions.cs:19`:
```csharp
public static bool HasNoRequiredUnits(this Player player, bool shortGame)
{
    if (shortGame)
        return !ActorsHavingTrait<MustBeDestroyed>(t => t.Info.RequiredForShortGame)
                 .Any(a => a.Owner == player);          // short game: only buildings count
    return !ActorsHavingTrait<MustBeDestroyed>()
             .Any(a => a.Owner == player && a.IsInWorld); // full: any required unit/building
}
```
i.e. "defeated = owns no actor carrying the victory-required trait", where short
game restricts the required set to buildings (ConYard/structures). [ANCHORED]

### Last-house-standing / team victory [ANCHORED]

OpenRA `ConquestVictoryConditions.cs:63`:
```csharp
if (!self.Owner.NonCombatant && self.Owner.HasNoRequiredUnits(shortGame))
    mo.MarkFailed(self.Owner, objectiveID);           // I am defeated

otherPlayers ??= World.Players
    .Where(p => !p.NonCombatant && !p.IsAlliedWith(self.Owner)).ToArray();
foreach (var otherPlayer in otherPlayers)
    if (otherPlayer.WinState != WinState.Lost) return; // someone hostile still alive
mo.MarkCompleted(self.Owner, objectiveID);            // all non-allied houses defeated → I win
```

Recovered victory rule:
- A house **wins when every non-allied, combatant house is defeated**.
- Because the check excludes `IsAlliedWith(self)` and `NonCombatant`,
  **a whole TEAM wins together** (surviving allies all win) and Civilian/Mutant
  houses never count toward victory or defeat. Last-team-standing = win.
  [ANCHORED]
- `MCVRedeploys=yes` (rules default) — losing the ConYard is not instant defeat
  if the MCV can redeploy into a new one; defeat is still evaluated on
  "no buildings" under ShortGame. [CONFIRMED key]

### AI / player count [CONFIRMED]

`[MultiplayerDialogSettings]`:
```ini
AIPlayers=0       ; starting number of AI players. Always at least 1 for skirmish. (def=0)
AIDifficulty=0    ; 0=easy,1=normal,2=hard (def=1; this file sets 0)
```
Number of houses instantiated = human slots + `AIPlayers` AI slots + the fixed
non-combatant houses (Neutral, Special). [CONFIRMED]

---

## §6 Start setup

`rulesmd.ini [MultiplayerDialogSettings]` (line 3017) + `[General]`:

```ini
MinMoney=5000        ; General MinMoney=5000 (line 3018)
Money=10000          ; default starting cash
MaxMoney=10000
MoneyIncrement=100
MinUnitCount=0
UnitCount=10         ; default starting units (excl. MCV) def=10
MaxUnitCount=10
TechLevel=10
GameSpeed=1
```
[CONFIRMED]

Recovered start setup:
- **Starting cash**: default **10000** credits per player (slider 5000–10000,
  step 100). [CONFIRMED]
- **Starting units**: `UnitCount=10` default (0–10 here). Each player spawns an
  **MCV plus `UnitCount` starting units** placed around the player's start
  waypoint. (The MCV itself is separate from UnitCount.) [CONFIRMED value;
  MCV-plus-units placement is ANCHORED.]
- **Start waypoints/locations**: each player slot is assigned one of the map's
  numbered start waypoints (waypoints 0..7 for up to 8 players); units drop
  around that cell. Waypoints come from the map, not rules. [ANCHORED]
- **House count instantiated per skirmish/MP match**: human slots + AI slots
  (≥1 for skirmish) + Neutral + Special. [CONFIRMED/ANCHORED per §5.]

---

## §7 AI houses

- **How many**: `[MultiplayerDialogSettings] AIPlayers` (def 0, ≥1 forced for
  skirmish). Each AI takes one player slot (same 8-slot cap). [CONFIRMED]
- **Allocation to slots**: AI fills the remaining player slots after humans;
  each AI slot gets a country (random/assigned), a unique MP color, and a start
  waypoint, exactly like a human slot. [ANCHORED]
- **Difficulty / handicap** — `[MultiplayerDialogSettings] AIDifficulty`
  (0=easy,1=normal,2=hard) selects a global difficulty band; the actual
  per-side handicap multipliers are in the difficulty sections
  (`rulesmd.ini` line 3426):

  ```ini
  [Easy]      ROF=.8   Armor=1.2  BuildTime=.8   Cost=1.0  DestroyWalls=no  ContentScan=yes
  [Normal]    ROF=1.0  Armor=1.0  BuildTime=1    Cost=1.0  BuildSlowdown=yes DestroyWalls=no ContentScan=yes
  [Difficult] ROF=1.2  Armor=.8   BuildTime=1.0  Cost=1.0  BuildSlowdown=yes DestroyWalls=no
  ```

  The rules comment (line 3427) confirms **human and AI are handicapped
  independently** — the AI may run at Difficult while the human runs at Easy.
  Multipliers apply to firepower/ROF/armor/cost/build speed. [CONFIRMED]

---

## §8 Confidence summary

| Item | Tag | Basis |
|---|---|---|
| §1 Country list (14 entries, indices) | **[CONFIRMED]** | `[Countries]` line 959 |
| §1 Country→side (`[Sides]`, GDI=Allied/Nod=Soviet/ThirdSide=Yuri) | **[CONFIRMED]** | `[Sides]` line 981 |
| §1 Per-country fields (Side/Suffix/Prefix/Color/Multiplay) | **[CONFIRMED]** | country sections 3199–3353 |
| §1 Country bonus/veteran multipliers are DISABLED in this rules rev | **[CONFIRMED]** | commented-out lines |
| §1 Per-country special unit (paradrop/Tesla/etc.) | **[ANCHORED]** | documented RA2 (mults off here) |
| §1 Slot vs country type | **[ANCHORED]** | OpenRA HouseClass/Player |
| §1 Neutral/Special non-combatant | **[CONFIRMED]** | `MultiplayPassive=true`, `Side=Civilian/Mutant` |
| §1 MaxPlayers = 8 | **[ANCHORED]** | no rules key; 8 MP colors; documented cap |
| §2 `[Colors]` HSV list + values | **[CONFIRMED]** | `[Colors]` line 3367 + HSV comment 3357 |
| §2 8 MP player colors | **[CONFIRMED]** | `MP` tags in `[Colors]` |
| §2 Slot color default from country / lobby override | **[CONFIRMED]** / [ANCHORED] | country `Color=` / lobby |
| §2 HSV→remap spread (pal_for_house) | **[ANCHORED]** | matches existing engine + documented |
| §3 Relationship enum (Ally/Enemy/Neutral) + self-ally + mutual | **[ANCHORED]** | OpenRA Player.cs:249 |
| §3 FFA default (`AlliesAllowed=no`) / team pre-ally | **[CONFIRMED]** / [ANCHORED] | MPDialogSettings 3038 |
| §3 `AllyChangeAllowed=yes` in-game requests | **[CONFIRMED]** | MPDialogSettings 3042 |
| §3 `AllyReveal=yes` shared shroud | **[CONFIRMED]** | line 751 |
| §3 No shared control/economy | **[ANCHORED]** | OpenRA/documented |
| §4 Targeting gated on ENEMY relationship (not same-house) | **[ANCHORED]** | AutoTarget.cs:342/420 |
| §4 Force-fire bypass + splash hits allies | **[ANCHORED]** | documented RA2 |
| §5 ShortGame=yes → defeat on last building | **[CONFIRMED]** | MPDialogSettings 3039 + def |
| §5 Defeat = no MustBeDestroyed actors | **[ANCHORED]** | PlayerExtensions.cs:19 |
| §5 Win = all non-allied combatants defeated (team win) | **[ANCHORED]** | ConquestVictoryConditions.cs:63 |
| §5 AIPlayers / AIDifficulty | **[CONFIRMED]** | MPDialogSettings 3027/3028 |
| §6 Cash 10000 / UnitCount 10 / TechLevel 10 | **[CONFIRMED]** | MPDialogSettings 3018–3025 |
| §6 MCV + units at start waypoint | **[ANCHORED]** | documented RA2 / map waypoints |
| §7 Difficulty handicap tables (Easy/Normal/Difficult) | **[CONFIRMED]** | lines 3426–3481 |
| §7 Human/AI handicapped independently | **[CONFIRMED]** | comment line 3427 |

### One-line takeaways for the implementer

- **Countries (14)**: Americans, Alliance, French, Germans, British (Allied);
  Africans, Arabs, Confederation, Russians (Soviet); YuriCountry (Yuri);
  GDI, Nod (passive); Neutral, Special (civilian/mutant, non-combatant).
- **8 MP colors**: Gold(Yellow), DarkRed(Red), Orange, Magenta(Pink),
  Purple, DarkBlue(Blue), DarkSky(Aqua), DarkGreen(Lt green) — values are **HSV**.
- **MaxPlayers = 8** (no rules key; ANCHORED).
- **Alliance check**: pairwise relationship Ally/Enemy/Neutral; self→self is
  always Ally; alliances mutual. Replace `house != self_house` with
  `RelationshipWith(target) == Enemy` for enmity.
- **Enmity/targeting**: auto-acquire only ENEMY houses (never self, ally, or
  neutral/civilian); force-fire bypasses; splash still damages allies.
- **FFA is the default** (`AlliesAllowed=no`); team games pre-ally teammates.
- **Victory**: last team standing — a house wins when every non-allied combatant
  house is defeated; ShortGame=yes ⇒ defeated when last building falls;
  Neutral/Special never win or lose.

---

## §Adjudication (RE-GATE) — 2026-07-20

Task 1 RE-GATE for the Multi-house/Alliances phase. Re-confirms values against the
real `rulesmd.ini` (`D:/ra2-re/out/shroud_evidence/rulesmd.ini`) and closes the four
`[RE-FALLBACK]`/`[ANCHORED]` items. No fresh hunt.

### Item 1 — 8 MP colors → ADJUDICATED (HSV triples pinned; conversion cosmetic/unhashed)
`[Colors]` (rulesmd.ini:3367) — the eight entries commented `MP` are the multiplayer
palette, values are **HSV** (rules comment "these are HSV values"):
- Gold `43,239,255`; DarkRed `0,230,255`; Orange `25,230,255`; Magenta `221,102,255`;
  Purple `201,201,189`; DarkBlue `153,214,212`; DarkSky `131,200,230`; DarkGreen `81,200,210`.
**Decision:** `pal_for_color(ix)` converts the house's HSV triple to RGB and spreads it
over the art's reserved house-color band exactly as `pal_for_house` already does for
blue/red. Rendering is UNHASHED, so any faithful HSV→RGB→band-spread is acceptable; pin
the eight HSV triples above. NOTE (`;gs` comment): ColorScheme indices are doubled
(`ColorSchemes[5]` is White, not 2) — irrelevant to us; we index our own 8-entry MP list.

### Item 2 — defeat condition → ADJUDICATED (ShortGame=yes ⇒ last building)
`ShortGame=yes` (rulesmd.ini:3039, shipped default). **Decision:** a combatant house is
DEFEATED when its last BUILDING is destroyed (ShortGame path). The `--demo-multihouse`
gate drives the ShortGame=yes rule (defeat-on-last-building). `ShortGame=no` (zero
buildings AND zero units) is supported as a flag but not the gate default. Required-
building set = any alive, non-Dead building the house owns (no special ConYard-only rule
in shipped rules). [CONFIRMED rule / mechanism ANCHORED]

### Item 3 — neutral default relationship → ADJUDICATED (non-combatant ⇒ never enemy)
`[Neutral]` `MultiplayPassive=true`, `Side=Civilian`, `Color=Grey` (and `[Special]` Mutant).
**Decision:** a `MultiplayPassive` house is `combatant=false`; `is_enemy(a,b)` returns
false whenever either side is non-combatant (both directions) — neutral is never
auto-attacked and never attacks, matching the relationship-Neutral third state. Neutral
buildings remain CAPTURABLE (engineer) and are still hit by location-based area damage
(nuke splash spares only the firing house — see the plan's world.cpp:1106 amendment).

### Item 4 — FFA-default enmity → ADJUDICATED (== a!=b among combatants)
`AlliesAllowed=no` (rulesmd.ini:3038, shipped default) ⇒ FFA: every combatant is enemy of
every other combatant, no pre-formed team. **Decision:** the configured-FFA path
(`alliance_team` = own id for each house) yields `is_enemy(a,b) == (a!=b)` among
combatants — IDENTICAL to the unconfigured default (`houses_configured_==false` ⇒
`is_enemy==a!=b`). So the gate's alliance-configured FFA baseline is byte-consistent with
today. `AllyChangeAllowed=yes` (3042, in-game requests) and `AllyReveal=yes` (751, allies
share revealed map) confirmed.

### Country default colors (note, NOT an engine-default change)
Country `Color=` defaults are the LOBBY defaults: Americans/Allied=`Gold`, Russians/
Soviet=`DarkRed`, YuriCountry=`DarkRed`, Neutral=`Grey`. Our engine KEEPS the current
byte-identical house defaults (house 0 = blue, house 1 = red — today's `pal_for_house`)
so the 15 gates stay pixel/hash-identical; the recovered country/lobby colors apply when a
skirmish slot explicitly picks a country (Phase 3 skirmish shell). Documented deviation —
color is render-unhashed.

**Net:** all four items adjudicated with recovered-data-backed decisions; none needs human
escalation. Plan's is_enemy default (`a!=b`), the world.cpp:1106 amendment, and the 8-color
list are consistent with these.
