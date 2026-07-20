# Skirmish / Multiplayer Lobby + Main-Menu Structure — Recovered (Yuri's Revenge)

Recovery of the ORIGINAL Red Alert 2 / Yuri's Revenge **skirmish & multiplayer
lobby options** (the option list, defaults, and ranges the skirmish config must be
faithful to), the top-level **main-menu structure**, the **skirmish launch flow**
(how lobby slots map to our multi-house model), and the **game-speed / tick model**.

This phase is frontend/design-heavy: the **DATA** (option list, ranges, defaults)
is recovered faithfully from the real rules; the UI/scene framework is design
(we document WHAT the original menu offers, not pixel layout).

**Tags:**
- **[CONFIRMED]** — recovered directly from the REAL `rulesmd.ini` (extracted from
  `D:\Red Alert 2\ra2md.mix`) or a decompile.
- **[ANCHORED]** — structure/mechanism from OpenRA (trusted for structure, not
  addresses) and/or documented RA2/YR behavior.
- **[RE-FALLBACK]** — a design choice where the original isn't cheaply recoverable.

## Evidence sources / commands

- Real rules (already extracted, 742958 bytes):
  `D:\ra2-re\out\shroud_evidence\rulesmd.ini`
  (re-extract: `ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/rulesmd.ini <out>`)
- OpenRA reference (structure anchor only): `D:\ra2-re\reference\OpenRA`.
- Our engine multi-house model: `D:\ra2-android\engine\world.hpp`,
  `D:\ra2-android\engine\world.cpp`, `D:\ra2-android\engine\main.cpp`.
- Sibling recovery: `D:\ra2-android\docs\re\multihouse.md` (country/color/alliance model).
- Grep/read commands shown inline as evidence per section.

Primary evidence block — `grep -n 'MultiplayerDialogSettings' rulesmd.ini` →
line **3017**, preceded by the option-by-option comment glossary at lines
**2983–3015** (each key's meaning + `def=` default):

```ini
; ******* Multiplayer Dialog Settings *******            ; rulesmd.ini:2983
; These are the multiplayer dialog settings. Does not apply to Westwood chat,
; only to the in-game dialogs.  Gads what a mess this was!
; PCG; 05/02/2K; Added a bunch of stuff to this to try to get everything
; controllable from one place.
; MinMoney = minimum amount of money selectable in the slider. (def=2500)
; Money = default amount of money that shows up on the slider. (def=10000)
; MaxMoney = maximum amount of money selectable in the slider. (def=10000)
; MoneyIncrement = the amount of money that changes when the slider position
;                  changes its minimum amount. (def=100)
; MinUnitCount = minumum number of starting units in a game. (def=1)
; UnitCount = the default starting units. (def=10)
; MaxUnitCount = maximum number of starting units. (def=20)
; TechLevel = maximum tech level achievable in a game. (def=10)
; GameSpeed = starting game speed. For some wacky reason, 0=fastest, 6=slowest. (def=0)
; AIDifficulty = starting AI difficulty. (def=1)  (0=easy, 1=normal, 2=hard)
; AIPlayers = starting number of AI players.  Always at least 1 for skirmish.  (def=0)
; BridgeDestruction = can bridges be destroyed? (def=true)
; ShadowGrow = deos the shroud regrow? (def=no)  DESUPPORTED.
; Shroud = is there a shroud on the map? (def=yes)  NOT YET SUPPORTED.
; Bases = can you build buildings or is this just a unit-to-unit slugfest. (def=yes)  DESUPPORTED.
; TiberiumGrows = does ore/tiberium/whatever regenerate? (def=yes)
; Crates = do crates appear in the game? (def=yes)
; CaptureTheFlag = is this a CTF game? (def=no)  DESUPPORTED.
; HarvesterTruce = are harvesters immune to enemy fire? (def=no)  DESUPPORTED.
; MultiEngineer = do you need more than one engineer to take over a building? (def=no)  DESUPPORTED.
; AlliesAllowed = are allies allowed in this game? (def=no)
; ShortGame = is a player eliminated when his last building is destroyed? (def=yes)
; FogOfWar = is there fog of war in this game? (def=no)
; MCVRedploys = can the player redeploy the MCV? (def=yes)
; AllyChangeAllowed = can alliances be changed mid-game? (def=yes)

[MultiplayerDialogSettings]                              ; rulesmd.ini:3017
MinMoney=5000
Money=10000
MaxMoney=10000
MoneyIncrement=100
MinUnitCount=0
UnitCount=10
MaxUnitCount=10
TechLevel=10
GameSpeed=1
AIDifficulty=0
AIPlayers=0
BridgeDestruction=yes
ShadowGrow=no
Shroud=yes
Bases=yes
TiberiumGrows=yes
Crates=yes
CaptureTheFlag=no
HarvesterTruce=no
MultiEngineer=no
AlliesAllowed=no
ShortGame=yes
FogOfWar=no
MCVRedeploys=yes
AllyChangeAllowed=yes
```

> **Two default sets exist.** The comment glossary (2990–3015) gives Westwood's
> *code defaults* (`def=…`, e.g. MinMoney 2500, MaxUnitCount 20, GameSpeed 0,
> AIDifficulty 1). The `[MultiplayerDialogSettings]` VALUES (3018–3042) are the
> **shipped rules overrides** the game actually loads (MinMoney 5000, MaxUnitCount
> 10, GameSpeed 1, AIDifficulty 0). **Our skirmish config must use the shipped
> values** (the ones the retail game presents), noting the code default where it
> differs. Both are [CONFIRMED].

---

## §1 Skirmish / MP lobby OPTION list + defaults + ranges

All values below [CONFIRMED] from `[MultiplayerDialogSettings]` (rulesmd.ini:3017)
unless tagged otherwise. "Shipped" = value in the section; "code def" = the `def=`
in the comment glossary.

### Sliders / numeric options

| Option | Shipped default | Range (min…max, step) | Rules key + line | Tag |
|---|---|---|---|---|
| **Starting Credits (cash)** | **10000** | **5000 … 10000**, step **100** | `Money=10000` (:3019); `MinMoney=5000` (:3018); `MaxMoney=10000` (:3020); `MoneyIncrement=100` (:3021) | [CONFIRMED] |
| **Unit Count** (starting units, excl. MCV) | **10** | **0 … 10** | `UnitCount=10` (:3023); `MinUnitCount=0` (:3022); `MaxUnitCount=10` (:3024) | [CONFIRMED] |
| **Tech Level** (max buildable tech) | **10** | **1 … 10** | `TechLevel=10` (:3025) | [CONFIRMED] value; 1..10 range [ANCHORED] (10 discrete tech tiers) |
| **Game Speed** | **1** | **0 … 6** (0=fastest, 6=slowest) | `GameSpeed=1` (:3026) + comment :2999 | [CONFIRMED] |
| **AI Players** (count) | **0** | **0 … 7** (≥1 forced in skirmish) | `AIPlayers=0` (:3028) + comment :3001 | [CONFIRMED] value; 0..7 cap [ANCHORED] (8-slot cap minus ≥1 human) |
| **AI Difficulty** | **0 = Easy** | Easy=0 / Normal=1 / Brutal(Hard)=2 | `AIDifficulty=0` (:3027) + comment :3000 | [CONFIRMED] |

Notes:
- **Credits:** shipped slider is 5000–10000 (code def min was 2500). Matches the
  10000 default recovered earlier in `multihouse.md §6`. **Re-confirmed.**
- **Unit Count:** shipped MAX is **10** (equal to the default), so in retail the
  slider effectively offers 0–10; the code default MAX was 20. The MCV is spawned
  **in addition** to UnitCount (MCV is not counted). [MCV-plus-units placement:
  ANCHORED — see §3.]
- **AI Difficulty label:** retail UI shows Easy / Normal / **Brutal**; the rules
  value maps 0/1/2 = Easy/Normal/Hard. The AI handicap multipliers live in the
  `[Easy]`/`[Normal]`/`[Difficult]` sections (rulesmd.ini ~3426; see
  `multihouse.md §7`). Human and AI are handicapped independently. [CONFIRMED]

### Toggles / checkboxes

| Option | Shipped default | Rules key + line | Status in retail | Tag |
|---|---|---|---|---|
| **Bridge Destruction** | **yes (on)** | `BridgeDestruction=yes` (:3029) | Active | [CONFIRMED] |
| **Crates** (goodies/pickups) | **yes (on)** | `Crates=yes` (:3034) | Active | [CONFIRMED] |
| **Short Game** (defeat on last building) | **yes (on)** | `ShortGame=yes` (:3039) | Active | [CONFIRMED] |
| **MCV Redeploys** (repack/redeploy) | **yes (on)** | `MCVRedeploys=yes` (:3041) | Active | [CONFIRMED] |
| **Allies Allowed** (teams/alliances) | **no (off, = FFA)** | `AlliesAllowed=no` (:3038) | Active | [CONFIRMED] |
| **Ally Change Allowed** (mid-game re-ally) | **yes (on)** | `AllyChangeAllowed=yes` (:3042) | Active | [CONFIRMED] |
| **Fog of War** | **no (off)** | `FogOfWar=no` (:3040) | Active | [CONFIRMED] |
| **Shroud** (map starts shrouded) | **yes (on)** | `Shroud=yes` (:3031) | "NOT YET SUPPORTED" (comment :3004) — always-on in retail | [CONFIRMED] |
| **Ore/Tiberium Grows** | **yes (on)** | `TiberiumGrows=yes` (:3033) | Active | [CONFIRMED] |
| **Bases** (build buildings vs unit-slugfest) | **yes (on)** | `Bases=yes` (:3032) | **DESUPPORTED** (comment :3005) | [CONFIRMED key; feature dead] |
| **Bridge shadow regrow** | **no** | `ShadowGrow=no` (:3030) | **DESUPPORTED** (comment :3003) | [CONFIRMED; dead] |
| **Capture The Flag** | **no** | `CaptureTheFlag=no` (:3035) | **DESUPPORTED** (comment :3008) | [CONFIRMED; dead] |
| **Harvester Truce** | **no** | `HarvesterTruce=no` (:3036) | **DESUPPORTED** (comment :3009) | [CONFIRMED; dead] |
| **Multi Engineer** (>1 eng to capture) | **no** | `MultiEngineer=no` (:3037) | **DESUPPORTED** (comment :3010) | [CONFIRMED; dead] |

**Key finding on the "DESUPPORTED" keys:** the trailing comments explicitly mark
`Bases`, `ShadowGrow`, `CaptureTheFlag`, `HarvesterTruce`, `MultiEngineer` as
**DESUPPORTED** in this (retail YR) build — the keys still parse but the features
were cut. A faithful skirmish shell should NOT offer these as live toggles;
they remain fixed at their shipped values. `Shroud` is "NOT YET SUPPORTED" =
effectively always-on. This is the difference between the *rules schema* and the
*retail lobby*: the retail lobby exposes Credits, Unit Count, Tech Level, Game
Speed, AI Players/Difficulty, Crates, Short Game, MCV Repack, Bridge Destruction,
Superweapons, and Multiplayer Alliances — the DESUPPORTED keys are not user-facing.

### Options NOT in `[MultiplayerDialogSettings]` (recovered elsewhere / anchored)

| Option | Default | Source | Tag |
|---|---|---|---|
| **Superweapons allowed** (lobby toggle) | on | Retail lobby exposes a "Superweapons" checkbox; NOT a `[MultiplayerDialogSettings]` key. The rules `[IQ] SuperWeapons=4` (:3159) is the *AI IQ level at which the computer auto-fires* SWs, **not** the lobby allow-toggle — do not conflate. | [ANCHORED] toggle; [CONFIRMED] the IQ key is a different thing |
| **Build Off Ally** | on | Documented YR lobby toggle (share build radius with allies); no `[MultiplayerDialogSettings]` key in this rules rev (`grep BuildOffAlly rulesmd.ini` → no match). | [ANCHORED] |
| **Max Players / start locations** | per-map (2..8) | No `MaxPlayers` key in rules (`grep MaxPlayers` → none); hard 8-slot cap. Per-match start-location count comes from the **map's** `[Waypoints]` (waypoints 0..7). See §3. | [ANCHORED] (see `multihouse.md §1 MaxPlayers=8`) |
| **Map selection** | — | Lobby lists maps from the game's map store (`.mpr`/`.map` multiplayer maps + `.mmx`/`.yro` bundles); each map declares its start-waypoint count = its MaxPlayers. | [ANCHORED] |
| **Random / chosen country & color per slot** | country default color | Per-slot country (14 `[Countries]`) + one of 8 MP colors; see `multihouse.md §1/§2`. | [CONFIRMED list / ANCHORED lobby pick] |

**`[Maximums] Players=8`** (rulesmd.ini:3056, `; ipx layer limits this to 8
maximum`) is the object-heap cap and independently corroborates the 8-player limit.

---

## §2 Main-menu structure (top-level RA2 / YR menu)

Documented retail YR main menu. [ANCHORED — documented menu; not a rules artifact.]
"In scope" = relevant to this engine-clone's skirmish-first roadmap; "stub" = a
menu entry we present but do not implement now.

| Menu entry | Purpose | Scope for this clone |
|---|---|---|
| **Skirmish** | Single human + AI players vs each other on an MP map | **IN SCOPE** (the phase target) |
| **Campaign / Single Player** (Allied / Soviet / Yuri ops) | Scripted mission campaigns | Stub (out of scope now) |
| **Multiplayer** → LAN / Internet (WOL) / Network / Modem | Human-vs-human matches | Stub / partial — same lobby data model as Skirmish, minus AI-fill + netcode. The **option list & launch flow are shared with Skirmish**, so §1/§3 cover both. |
| **Load Game** | Restore a saved sim | Stub |
| **Options** | Audio/video/keyboard settings | Stub (engine has its own config) |
| **Map Editor** (FinalAlert 2, external tool) | Author maps | Out of scope (external) |
| **Movies / Sneak Peek / View Intro** | Play bundled videos | Stub |
| **Credits** | Studio credits scroll | Stub |
| **Exit / Quit** | Leave game | IN SCOPE (trivial) |

Takeaway: for this phase only **Skirmish** (and the shared MP lobby data model)
is live; everything else is a menu stub. Skirmish IS a local Multiplayer match
with AI-filled slots and no netcode — one code path serves both.

---

## §3 Skirmish launch flow → multi-house model mapping

What launching a skirmish match instantiates, and how the lobby config maps onto
the engine's `World` house API (`engine/world.hpp`). [Flow: ANCHORED; the engine
API + rules-backed defaults: CONFIRMED where noted.]

### What a match instantiates

1. **House slots** = (human slots) + (`AIPlayers` AI slots, ≥1 forced for
   skirmish) + the two fixed non-combatant houses **Neutral** and **Special**
   (Mutant). Combatant slots ≤ 8 (`[Maximums] Players=8`). [CONFIRMED counts /
   ANCHORED assembly — `multihouse.md §5/§6`.]
2. **Per combatant slot** binds: a **country** (one of the 14 `[Countries]`,
   → Side via `[Sides]`), a **color** (one of 8 MP colors, unique per slot), a
   **start waypoint** (map `[Waypoints]`), an **alliance team** id, and
   **human vs AI + AI difficulty handicap**.
3. **Start placement:** each slot spawns an **MCV** at its assigned start waypoint
   plus **`UnitCount` (=10)** starting combat units around that cell; **`Money`
   (=10000)** starting credits. Waypoints 0..N-1 come from the map, not rules.
   [ANCHORED placement; CONFIRMED counts.]
4. The sim then starts under the fixed tick (see §4).

### Mapping to the engine API (`engine/world.hpp`)

The engine already exposes the exact seams (recovered in `multihouse.md`):

```cpp
struct HouseRec {                 // world.hpp:785
    Side        side          = Side::Allied;   // from country's [Sides] side
    int         color_ix      = 0;              // 0..7 into the 8 MP colors
    std::string country       = "";             // one of the 14 [Countries]
    int         alliance_team = -1;             // -1 -> own id (FFA); shared id = team
    bool        combatant     = true;           // false for Neutral/Special
};
void set_house(int h, HouseRec rec);            // world.hpp:810  (opens firewall)
void enable_ai_house(int house, AiHouseConfig cfg);  // world.hpp:728
bool ai_enabled(int house) const;               // world.hpp:729
int  house_team(int h) const;                   // -1 sentinel -> own id
```

Launch → engine mapping:

| Lobby datum | Engine call | Notes |
|---|---|---|
| Slot's country → Side (Allied/Soviet/Yuri) | `set_house(h, {.side=…, .country=…})` | Side from `[Sides]` (GDI=Allied, Nod=Soviet, ThirdSide=Yuri). [CONFIRMED map] |
| Slot's picked MP color | `HouseRec.color_ix = 0..7` | 8 MP colors (`multihouse.md §2`). Render-unhashed. [CONFIRMED list] |
| Team assignment (`AlliesAllowed`) | `HouseRec.alliance_team` | FFA (`AlliesAllowed=no`, default): each slot's team = own id (sentinel -1). Team game: teammates share one team id → pre-allied. `is_enemy(a,b)` = `(team_a != team_b)` among combatants. [CONFIRMED default / ANCHORED teams] |
| Neutral + Special houses | `set_house(h, {.combatant=false})` | `MultiplayPassive=true`; never win/lose, never auto-targeted. [CONFIRMED] |
| `AIPlayers` count | one `enable_ai_house(h, cfg)` per AI slot | AI fills remaining slots after humans; ≥1 for skirmish. [CONFIRMED count] |
| `AIDifficulty` (0/1/2) | AI cfg handicap band | Easy/Normal/Difficult multiplier tables (rulesmd ~3426, `multihouse.md §7`). [CONFIRMED] |
| Start waypoint | spawn MCV + `UnitCount` units at map waypoint | Waypoints from map `[Waypoints]`. [ANCHORED] |
| `Money` / `UnitCount` | seed credits + spawn count | `grant_credits_for_test` exists as a test seam (world.hpp:978); real path seeds `house_credits_`. [CONFIRMED values] |

Firewall note: `set_house` sets `houses_configured_ = true`. With it FALSE the
engine keeps today's exact 3-house blue/red derivation (byte-identical to existing
gates); the skirmish shell is the first real caller. FFA with each team = own id
yields `is_enemy(a,b) == (a != b)`, identical to the unconfigured default — so the
FFA skirmish baseline is consistent with the current gates (`multihouse.md
§Adjudication Item 4`).

---

## §4 Game speed / tick model

### Original RA2/YR speed setting [CONFIRMED value / ANCHORED mechanism]

- `GameSpeed=1` shipped default (rulesmd.ini:3026). The comment (:2999) states:
  *"starting game speed. For some wacky reason, 0=fastest, 6=slowest. (def=0)."*
  So there are **7 discrete speed levels 0..6**, **inverted** (lower = faster).
  Retail ships **GameSpeed=1** (one notch below maximum). [CONFIRMED]
- Mechanism: RA2's `GameSpeed` throttles the **inter-frame delay** of the logic
  loop — level 0 runs frames back-to-back (fastest), each higher level inserts
  more timer delay per logic frame (slowest at 6). It scales *frame cadence*
  (wall-clock pacing), NOT per-frame movement magnitude. [ANCHORED — documented
  Westwood timer behavior.]
- Separately, `[General] GameSpeedBias=1.6` (rulesmd.ini:60,
  `; multiplier to overall game object movement speed`, "was 1.2") is a **global
  movement-speed multiplier** applied to object motion, independent of the
  GameSpeed cadence setting. [CONFIRMED it exists / value 1.6.]

### OpenRA structural anchor [ANCHORED]

OpenRA models the same concept as a per-preset **Timestep (ms/frame)** rather than
an inverted 0..6 index (`mods/ra/mod.yaml:286`):

```yaml
GameSpeeds:
  DefaultSpeed: default
  Speeds:
    slowest: { Timestep: 80 }   # 12.5 fps
    slower:  { Timestep: 50 }   # 20 fps
    default: { Timestep: 40 }   # 25 fps  (OpenRA "normal")
    fast:    { Timestep: 35 }   # ~28.6 fps
    faster:  { Timestep: 30 }   # ~33.3 fps
    fastest: { Timestep: 20 }   # 50 fps
```

Same idea (lower ms = faster); OpenRA exposes 6 presets vs RA2's 7-step index.

### Our engine vs the original [CONFIRMED our side / documented deviation]

- Our sim/demo gates use a **fixed logic step `dt = 1.0/15.0`** (~66.7 ms/frame,
  ~15 logic fps) — confirmed throughout `engine/main.cpp` (the demo/sim loops at
  main.cpp:4433, 4723, 5050, 5410, 5633, 5900, 6248, 6543, 6813, 7126, 7427, and
  `world.cpp:1667` "kLogicFps (the existing 1/15 s tick)"). Rendering-only demos
  use `dt = 1/60`. [CONFIRMED — code].
- **Deviation:** our engine has **no runtime game-speed control** — it is a single
  fixed 15 fps logical step, whereas the original offers 7 selectable cadences
  (0..6, default shipped = 1). Our fixed 1/15 (~66.7 ms) is *slower* than every
  OpenRA preset and corresponds to a slow RA2 cadence. For skirmish parity the
  `GameSpeed` lobby option is a **cadence/pacing** control; because our step is
  deterministic and fixed, GameSpeed can be treated as a **presentation-time
  pacing knob** (frames emitted per wall-second) that does NOT change the
  deterministic logic step or the world hash. The `GameSpeedBias=1.6` movement
  multiplier IS a logic-affecting constant and, if not already folded into our
  movement constants, must be honored to match original unit speeds. [Deviation:
  documented / RE-FALLBACK for how we expose GameSpeed.]

---

## §5 Confidence summary

| Item | Tag | Basis |
|---|---|---|
| Credits default 10000, slider 5000–10000 step 100 | **[CONFIRMED]** | `Money/MinMoney/MaxMoney/MoneyIncrement` (rulesmd.ini:3018–3021) |
| Unit Count default 10, range 0–10 (MCV extra) | **[CONFIRMED]** value; ANCHORED MCV-extra | `UnitCount/MinUnitCount/MaxUnitCount` (:3022–3024) |
| Tech Level default 10 | **[CONFIRMED]** value; ANCHORED 1–10 range | `TechLevel=10` (:3025) |
| Game Speed default 1, range 0–6 (0=fastest) | **[CONFIRMED]** | `GameSpeed=1` (:3026) + comment :2999 |
| AI Players default 0 (≥1 skirmish), cap 7 | **[CONFIRMED]** value; ANCHORED cap | `AIPlayers=0` (:3028) + comment :3001 |
| AI Difficulty default 0=Easy (0/1/2) | **[CONFIRMED]** | `AIDifficulty=0` (:3027) + comment :3000 |
| Crates on, Short Game on, MCV Redeploys on, Bridge Destruction on, Fog off, Allies off (FFA), AllyChange on | **[CONFIRMED]** | toggles :3029–3042 |
| Bases/ShadowGrow/CTF/HarvesterTruce/MultiEngineer = DESUPPORTED | **[CONFIRMED]** | trailing comments :3003–3010 |
| Shroud = NOT YET SUPPORTED (always-on) | **[CONFIRMED]** | comment :3004 |
| Superweapons lobby toggle (≠ `[IQ] SuperWeapons=4`) | **[ANCHORED]** toggle / **[CONFIRMED]** IQ key is separate | `[IQ]` :3159 |
| Build Off Ally lobby toggle | **[ANCHORED]** | no rules key; documented YR lobby |
| Max players 8 / start locations per-map | **[ANCHORED]** | `[Maximums] Players=8` :3056; no MaxPlayers key |
| Two default sets (code `def=` vs shipped values) | **[CONFIRMED]** | comment glossary vs section |
| Main-menu structure (Skirmish in scope; rest stubs) | **[ANCHORED]** | documented YR menu |
| Launch flow → HouseRec/set_house/enable_ai_house/alliance_team | **[CONFIRMED]** API + rules / **[ANCHORED]** flow | world.hpp:728/785/810; multihouse.md |
| Engine fixed dt=1/15, no speed control (deviation) | **[CONFIRMED]** code / documented deviation | main.cpp (11 sites), world.cpp:1667 |
| GameSpeedBias=1.6 movement multiplier | **[CONFIRMED]** | rulesmd.ini:60 |

### One-line takeaways for the implementer

- **Use the SHIPPED values** (section 3018–3042), not the code `def=` glossary:
  Credits **10000** (5000–10000/100), Unit Count **10** (0–10, MCV extra),
  Tech Level **10**, Game Speed **1** (0=fastest…6=slowest), AI Players **0**
  (≥1 in skirmish), AI Difficulty **0=Easy** (0/1/2=Easy/Normal/Brutal).
- **Live toggles** (all default as shipped): Crates on, Short Game on, MCV
  Repack on, Bridge Destruction on, Fog off, Superweapons on (anchored),
  Build Off Ally on (anchored), Multiplayer Alliances **off = FFA default**.
- **Do NOT expose** the DESUPPORTED keys (Bases, ShadowGrow, CTF, Harvester
  Truce, Multi Engineer) as lobby options — they were cut in retail YR.
- **Main menu:** only **Skirmish** (+ the shared MP lobby data model) is in
  scope; Campaign/Multiplayer-net/Load/Options/Movies/Credits are stubs; Skirmish
  is a local MP match with AI-filled slots.
- **Launch:** for each combatant slot call `set_house(h, {side, color_ix,
  country, alliance_team})`; FFA → team = own id (`is_enemy == a!=b`); Neutral +
  Special as `combatant=false`; one `enable_ai_house(h, cfg)` per AI slot;
  spawn MCV + 10 units at the map waypoint with 10000 credits.
- **Game speed:** original has 7 cadence levels; our engine is fixed 1/15 with no
  speed control — treat GameSpeed as presentation pacing (does not alter the
  deterministic step/hash); honor `GameSpeedBias=1.6` in movement constants.

---

## §Adjudication (RE-GATE) — 2026-07-20

Task 1 RE-GATE for the Skirmish Shell + Main Menu phase. The option data (§1) is
verbatim from the shipped `[MultiplayerDialogSettings]`. This closes the four items.

### Item 1 — in-scope option subset → ADJUDICATED
The SkirmishSetupScene EXPOSES (with shipped defaults/ranges from §1): number of
players + AI players (total combatant slots ≤ 8) & per-AI difficulty (Easy/Normal/
Difficult); per-slot side (Allied/Soviet) + color (8 MP colors) + alliance team;
starting Credits (slider 5000–10000 step 100, default 10000); Unit Count (0–10,
default 10); Tech Level (1–10, default 10); toggles Crates, Super Weapons, Short
Game (defaults on/on/on per §1 — note Crates default is on in shipped rules; the
design-direction's config default crates=false is the SAFE inert gate default, the
LOBBY default shown to the user is on). HIDDEN (desupported, §1): Bases, ShadowGrow,
CaptureTheFlag, HarvesterTruce, MultiEngineer. Build Off Ally + Super Weapons are
lobby toggles not in rules ([ANCHORED]) — Super Weapons exposed, Build Off Ally
deferred (shared-control carry-forward). Yuri side selectable in enum but Allied/
Soviet only have art (roster phase).

### Item 2 — GameSpeed → ADJUDICATED (presentation-only, omitted from config)
Our engine steps a FIXED dt=1/15 (§4, 11 sites). GameSpeed alters only inter-frame
pacing, never the deterministic step/hash. Decision: GameSpeed is NOT in
SkirmishConfig and NOT wired to the sim; the setup screen may show a cosmetic
speed control that only changes the interactive present-delay (does not touch
--demo-skirmish, which steps fixed 1/15). GameSpeedBias=1.6 movement multiplier =
CARRY-FORWARD (our movement constants are already calibrated; applying a bias now
would break the movement gates).

### Item 3 — map-list scope → ADJUDICATED (single mapsmd03; slot cap = min(8, starts))
Only `mapsmd03` exists in this install and there is no MIX map-enumeration path
(§7 survey). Decision: the setup screen offers the single map `mapsmd03`; a map
browser (MIX scan + .mpr parse) is a CARRY-FORWARD. Slot cap = min(8, map start-
location count); mapsmd03 is used for the demo/interactive 1v1..N with start cells
derived via pick_start_cell (no per-slot distinct waypoints required for the gate —
houses may share the reference start region since the gate validates config→launch
determinism, not spatial placement).

### Item 4 — --demo-skirmish fixed config shape → ADJUDICATED
The gate builds a FIXED in-code SkirmishConfig: 3 combatant slots in 2 alliance
teams — slot 0 {Allied, team 0, human-role} + slot 2 {Allied, team 0, AI Easy}
(allied) vs slot 1 {Soviet, team 1, AI Normal}; starting_cash 10000, unit_count 10,
tech_level 10, shroud=true, short_game=true, superweapons=true, crates=false
(inert). launch_skirmish → World, step a FIXED N=300 frames at dt=1/15, fold
skirmish_fold (house_count, per-house alive bldg+unit counts, per-pair relationship
sample, ai_enabled flags, victory_enabled/shroud_enabled, per-house credits),
two-run rendered+headless self-compare, NO pinned literal. Wedge guards: house_count
==expected(≥3), ai_enabled(1)&&ai_enabled(2), are_allied(0,2)&&is_enemy(0,1),
victory+shroud enabled, sim advanced (a determinism/liveness witness). Neutral house
optional (kept OUT to avoid the capture/infiltrate writer-guard carry — matches the
multihouse gate's discipline).

**Net:** all four items adjudicated; none needs human escalation. The plan's Task
2/3/6 markers are consistent with these (option subset, GameSpeed-omitted, single
map, demo config shape).
