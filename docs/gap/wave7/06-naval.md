# Wave 7 / 06: Naval units — gap analysis

**Tree:** master `b3a509f`, `build/ra2engine.exe` (built 2026-10-10 20:03; the last commit only changes scripts).
**Scope:** every naval-capable VehicleType in `rulesmd.ini`:
- Allied: DEST Destroyer (+ ASW Osprey), AEGIS Aegis Cruiser, CARRIER (+ HORNET), DLPH Dolphin, LCRF Amphibious Transport.
- Soviet: SUB Typhoon, HYD Sea Scorpion, SQD Giant Squid, DRED Dreadnought (+ DMISL), SAPC Amphibious Transport.
- Campaign-only: CDEST, VLAD, CRUISE, TUG, DNOB.
- Yuri-side: BSUB Boomer (+ CMISL), YHVR, YAYARD. These are out of scope, because the clone has no Yuri side (U-5), and are listed for completeness only.
- The ship side of the SEAL and Tanya swim interaction.

**Method:**
- **Data:** `rulesmd.ini` (`D:\ra2-re\out\shroud_evidence\rulesmd.ini`) and `aimd.ini` (`D:\ra2-re\w5-abil-out\aimd.ini`).
- **Docs:** `docs/re/naval.md` and the earlier gap reports: wave2/02, wave6/04 with part C, REMAINING.md, and the sibling report wave7/05-aircraft.md.
- **Code:** `world_an.cpp`, `w2air_main.inc`, `weapon_unit.cpp`, `combat.cpp`, `pres_effects.inc`, `ai_team.cpp`, `world.cpp` and `world_w5.cpp`.
- **Ghidra:** a private copy of the project, `D:\ra2-re\w7-naval`. Its scripts are in `D:\ra2-re\w7-naval-scripts` and its raw dumps in `D:\ra2-re\w7-naval-out` (`dec_r2.txt`, `dec_r8.txt`, `dec_r9.txt`, `dec_r13.txt`, `d3`…`d15`).
- **Play runs:** 3 `--play` runs on Bay of Pigs (America vs France, Easy AI), and one `--ai-soak --diff brutal --map xmp10s4.map --minutes 35` (Depth Charge). The scripts and the extracted probe values are in `06-evidence/`.

**Time base:** a skirmish runs 4 logic frames per `--play` driver frame (wave6/04). RE timings are given in logic frames.

## 1. Summary

The core naval loop works in real play:
- Ships build at the yard and sail on water only.
- The Destroyer's sonar finds a sub, and its Osprey kills it.
- A SEAL plants C4 on a ship (rechecked).
- Ships sink.
- The Dreadnought's missiles fly.
- The Naval Yard repairs ships.

Most of the wave-6 naval items are fixed: OmniFire, DecloakToFire, Sonic eject, transport Size, the amphibious approach and docked spawns.

What is still wrong is the layer above that: who may shoot what, how spawners cycle, stealth, and the AI.

**Totals:** 17 items.
- 5 MAJOR: 01, 02, 03 (shared with W7-05-05), 04, 05.
- 9 MINOR or COSMETIC.
- 3 INFO or out of scope.

**Top gaps:**
1. **W7-06-01 (MAJOR).** The skirmish AI's naval teams never attack. Each team musters at a cell within ±3 of the AI's base centre. That cell is on land, so ships can never "gather" there, and the team stays in the Gathering state for good. In 35 minutes on Depth Charge, 'Soviet Navy Bombard' and 'Soviet Navy Hunters' never executed [CONFIRMED-PLAY].
2. **W7-06-02 (MAJOR).** In the original, what counts as a naval target is the target's **cell LandType** (Water or Beach). The clone uses the victim's own flags. As a result, Typhoons, Dolphins and Squids cannot attack a Naval Yard or any other structure on water, and cannot attack land units standing on a beach [CONFIRMED-RE GetFireError @0x6FBC86-0x6FBD8C, SelectWeapon `FUN_006F2890`; CONFIRMED-PLAY: a SUB ordered onto a GAYARD never takes it as a target].
3. **W7-06-03 (MAJOR, the same gap as W7-05-05).** The Carrier keeps one Hornet in the air, against 3 launched 20 frames apart in the original [CONFIRMED-RE SpawnManager AI `FUN_006B68B0`; CONFIRMED-PLAY].
4. **W7-06-04 (MAJOR).** Submerged units leave wakes. A moving Typhoon spawned 22 WAKE1 anims in 60 frames, and the wake is drawn for any visible cell, so a hidden enemy sub gives itself away. The original never spawns a wake for an `Underwater=yes` type [CONFIRMED-RE ShipLoco Process @0x69F4FE; CONFIRMED-PLAY].
5. **W7-06-05 (MAJOR).** `OpportunityFire=yes` (DEST, AEGIS, CDEST) is ignored. No clone unit fires while it is under a move order, so an Aegis escorting a moving fleet never shoots at aircraft [CONFIRMED-RE `FUN_00708710` @0x70885F].
6. **W7-06-06 (MINOR).** Spawn regeneration is serial in the clone; the original gives every node its own timer. A Carrier that loses all 3 Hornets is back to strength in 600 frames in the original and 1800 in the clone, and the Dreadnought refills both missiles in parallel [CONFIRMED-RE `FUN_006B72E0`].
7. **W7-06-07 (MINOR, R-090).** The Naval Yard repairs for free. The land Service Depot charges, and the clone already has that code [CONFIRMED-PLAY: 300 HP repaired, credits unchanged].

## 2. Per-unit table

Legend: **OK** = measured in play now or in wave 6. **OK?** = checked in code and data only.

| Unit | Rules keys read / ignored | Movement and stealth | Targeting and weapons | Spawns / special | Verdict |
|---|---|---|---|---|---|
| DEST Destroyer | Read: Naval, NavalTargeting=1, Sensors/SensorsSight 8, Spawns ASW ×1, SpawnRegen 400, SpawnReload 150, Weight 3 (sinks), ElitePrimary. Ignored: **OpportunityFire** (05), **NoSpawnAlt** (12), FireAngle, TooBigToFitUnderBridge (14), ToProtect | Water only (Float) OK. A low bridge blocks it (bridge cell = Road, faithful) | 155mm vs land and ships OK. vs a submerged sub it uses the ASW secondary OK | Osprey sortie, dock and rearm OK (W6). Regen is serial (06) | PARTIAL: 05/06/12 |
| AEGIS Aegis Cruiser | Read: NavalTargeting=6, LandTargeting=1, OmniFire (W6-04-06 fixed). Ignored: **OpportunityFire**, **DistributedFire**, RadialFireSegments (11) | OK? | Medusa AA only (SAMWH 0% vs ships and buildings), faithful. Shoots DMISL/V3 (interceptable, W4-PJ-10) | — | PARTIAL: 05/11 |
| CARRIER | Read: CanPassiveAquire=no, Spawns HORNET ×3, SpawnRegen 600, SpawnReload 150, OmniFire | OK? | HornetLauncher range 25 | **Launches one Hornet per 150 frames** (03); serial regen (06). Hornets crash when the carrier dies (faithful, W7-05) | WRONG: 03/06 |
| DLPH Dolphin | Read: Underwater, Cloakable, Sensors 8, NavalTargeting=5, LandTargeting=1, Organic, Sonic warhead (W6-04-09 fixed), DecloakToFire=no (fixed). Ignored: **IsSonic / AmbientDamage** (10), CloakingSpeed, Accelerates | Submerged, hidden without sonar OK. **Leaves wakes** (04) | Cannot hit a yard or a beached unit (02) | Frees a Squid host by a Sonic hit (W6-04-09) | PARTIAL: 02/04/10 |
| SUB Typhoon | Read: Underwater, Cloakable, Sensors 7, NavalTargeting=5, LandTargeting=1, DecloakToFire=no. Ignored: CloakingSpeed, Accelerates | Submerged; DecloakToFire honoured (W6-04-25 fixed). **Leaves wakes** (04) [CONFIRMED-PLAY] | Torpedoes ships OK. **Cannot attack a Naval Yard** [CONFIRMED-PLAY] or a beached unit (02) | Weight 4 + Underwater: explodes, does not sink (faithful) | WRONG: 02/04 |
| HYD Sea Scorpion | Read: Primary FlakTrackGun (AG) + Secondary FlakWeapon (AA), elite pair, Weight 2 | OK (naval.md exemplar) | Default NavalTargeting 0: never hits a submerged sub (faithful) | Explodes (Weight 2 < 3), faithful | OK? |
| SQD Giant Squid | Read: NavalTargeting=3, LandTargeting=1, Organic, Paralyzes, Culling at 25%, ParasitePlus, Sensors 8. Ignored: **DefaultToGuardArea** (15), Unnatural on victims (Yuri only), CloakingSpeed | Leaves wakes (04) | Grabs ships (W6 OK); vs an organic target uses SquidPunch (faithful). Cannot grab or punch on a beach cell (02) | Leap not drawn (R-054 open) | PARTIAL: 02/04/15 |
| DRED Dreadnought | Read: CanPassiveAquire=no, MinimumRange 8 (`combat.cpp:1264`), Spawns DMISL ×2, Regen 80, Weight 4 (sinks) | OK? | DredLauncher OmniFire (fixed) | Volley fires in one frame instead of 9-frame spacing, and regen is serial and starts at launch (08). Ship does not halt while missiles launch (16, SUSPECTED) | PARTIAL: 06/08 |
| LCRF / SAPC Amphibious Transport | Read: MovementZone=Amphibious, Hover, Passengers 12 (Size pool, W6-04-08 fixed), SizeLimit 6, Naval=yes (built at the yard), Crusher (SAPC) | Beaches and unloads on land only OK (W6) | In water: a sub may torpedo it (in_water = amphibious on a water cell) OK? | — | OK |
| ASW Osprey / HORNET | see wave7/05-aircraft.md | — | Osprey NavalTargeting=2: subs only (faithful) | — | see 05 |
| DMISL | RocketLocomotion (W4) | — | Interceptable OK | — | OK |
| CDEST, VLAD | as DEST and DRED (same keys, campaign TechLevel -1) | as DEST / DRED | as DEST / DRED | as DEST / DRED | inherits 03/05/06/08 |
| CRUISE, TUG | Insignificant civilian ships, AmbientSound read | ShipLoco | none | — | OK? |
| DNOB "Animal Bront" | Locomotor {4A582744} with MovementZone=Water: the clone gives it the **Land** domain because only the ShipLoco CLSID sets `is_naval` (17) | — | — | — | INFO (no stock map uses it) |
| BSUB Boomer, YHVR, YAYARD, CMISL | Need NavalTargeting=7, LandTargeting=2 (LAND_SECONDARY), Unnatural, VoiceSecondaryWeaponAttack, SensorArray (NAPSIS) | — | — | — | OUT OF SCOPE (U-5) |
| GAYARD / NAYARD | WaterBound placement OK; Naval factory OK; UnitRepair **free** (07) | — | **Not targetable by subs, dolphins or squids** (02) | — | WRONG: 02/07 |

## 3. Gaps

### W7-06-01: The AI's naval teams never leave the Gathering state, so the AI fleet never attacks
- **Severity / effort:** MAJOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] `06-evidence/soak-xmp10s4-brutal.txt`:
    - The Brutal Soviet AI built NAYARD at min 10 and its first ship at min 25, then had `ai_navy=6` by min 35.
    - At the end, 'Soviet Navy Bombard' (1 DRED, 2 HYD, 1 SQD) was in state=1 (Gathering) with 4 members, gather=(122,82), at=(133,89), issued=0, moving=0.
    - 'Soviet Navy Hunters' (3 SUB) was also in state=1 with 2 members, gather=(127,80), at=(136,88).
    - Neither team ever reached Executing. The land teams in the same run did.
  - [CONFIRMED-code] `ai_team.cpp:42-45`: `base_gather_cell` returns the AI base centre ±3 cells, which is land.
    - `ai_team.cpp:117-125` sends every team there on Recruiting → Gathering.
    - The team only executes when `all_within(view, gather, Stray)` holds (`ai_team.cpp:163-165`).
    - When the team stalls, the code picks another base cell (`world.cpp:4802-4808`), which again is land. That is a loop forever.
  - The "pre-script muster at the own base" is itself a [RE-FALLBACK] (W2-SK-06, `ai_team.cpp:118-121`).
- **Original:** stock aimd naval scripts are 'General Naval Bombard' `0,6 / 49,0 / 0,1` (Attack Factories, …) and 'General Factory Attack'. TeamClass runs them; no RE shows a land muster for a naval TaskForce (ai-team.md §2). The wave-6 note "ships only after min 24" (03-skirmish-ai §3) is the same symptom.
- **Fix:**
  - In `step_team`'s Recruiting → Gathering step, when every member is in the Water domain (or the TaskForce is all Naval=yes), pick the gather cell on the water grid. Use the nearest free water cell to the base, or the own Naval Yard's exit cell.
  - Alternatively, skip the muster for naval teams and go straight to Executing.
  - The stall re-pick at `world.cpp:4804` must use the same domain-aware picker.

### W7-06-02: Naval targeting keys off the victim's flags instead of its cell's LandType: no sub, dolphin or squid attack on a Naval Yard or a beach
- **Severity / effort:** MAJOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] n1 T1 (`06-evidence/out1-probes.txt`):
    - My SUB was ordered onto an enemy GAYARD. For 240 frames `utgtb(0,SUB,0)=-1` and `bhp(1,GAYARD)=1500`.
    - Control: the Destroyer on the same yard had `utgtb=80`, and the yard took 1500 → 1480.
  - [CONFIRMED-code] `weapon_unit.cpp:71-72` calls `an_naval_slot(..., victim_in_water=false, ..., is_building=true)` for every structure, so LandTargeting=1 returns -1.
    - For units, `victim_in_water` is `Unit::an.in_water` (`world_an.cpp:117-123`). That is true only for ships, and for amphibious units on a **Water** cell.
- **Original** [CONFIRMED-RE]:
  - **TechnoClass::GetFireError**, the techno-target block (0x6FBC86-0x6FBD8C):
    - `inWater = (Target->GetCell()->LandType ∈ {Water(2), Beach(6)}) && !Target->IsInAir() && !Target->OnBridge(+0x8C)`.
    - If inWater: `if (this->NavalTargeting-switch(Target) == -1) return ILLEGAL(5)`, through vtbl+0x2E8 (`FUN_006F2D80`).
    - Else, if the target IsOnFloor and `Type->LandTargeting(+0x604) == 1`, it returns ILLEGAL.
  - **SelectWeapon** `FUN_006F2890` uses the same cell test (`+0xEC == 2 || == 6`) before it calls the switch.
  - For a structure, the cell is the foundation cell. A Naval Yard stands on Water, so to a Typhoon it is a NAVAL_ALL target.
- **Code:** `weapon_unit.cpp:56-74`, `world_an.cpp:35-61`, `world_an.cpp:117-123`.
- **Fix:**
  - Compute `victim_in_water` from the victim's cell, for units and buildings alike: water grid or Beach LandType (ShorePieces set), not on a bridge, not airborne.
  - Pass it for buildings too, using the foundation origin cell.
  - Keep `is_building` only for the BombDisarm rule.
- **Related slot rules** in the same switch, which the clone approximates [CONFIRMED-RE `FUN_006F2D80`]:
  - It returns a **fixed** slot: case 5 → 0, case 1 → `Underwater ? 1 : 0`, case 2 → `Underwater ? 0 : -1`, case 6 → -1. The clone returns `want`, which only matters for two-weapon naval units.
  - Case 3 (Squid) uses the secondary when the target is Organic(+0xD97) **or Unnatural(+0x694)**.
  - Case 4 (SEAL) uses the secondary C4 only when the target is **neither Hover (SpeedType 3) nor Organic**. See W7-06-09.

### W7-06-03: The Carrier keeps one Hornet airborne. It should be the whole wing, launched 20 frames apart (shared with W7-05-05)
- **Severity / effort:** MAJOR / M
- **Evidence:**
  - [CONFIRMED-PLAY] n1 T2: `spawned_out(0)` was never above 1 across 30 samples (150 driver frames = 600 logic frames), with the Carrier attacking a yard.
  - [CONFIRMED-RE] SpawnManager AI `FUN_006B68B0`:
    - UpdateTimer re-arms every 10 frames.
    - For each node in state 0, while the manager holds a target (+0x68) and SpawnTimer (+0x5C/+0x64) has run out: launch, then set `SpawnTimer = MissileSpawn(+0xD68) ? 9 : 20`, node state 2, give the spawn the target.
- **Code:** `world_an.cpp:291-334` (`count = missile ? spawn_ready : 1`, once per weapon shot, HornetLauncher ROF 150).
- **Fix:** see W7-05-05. Keep a per-spawner launch timer and, while it holds a target, launch one ready spawn every 20 frames (9 for MissileSpawn), independent of the weapon ROF.

### W7-06-04: Submerged units leave wakes, which give away hidden subs, dolphins and squids
- **Severity / effort:** MAJOR (stealth information leak) / S
- **Evidence:**
  - [CONFIRMED-PLAY] n2 T4: my Typhoon was the only moving ship; `fx_wakes` went 0 → 22 over 60 frames.
  - [CONFIRMED-code] `pres_effects.inc:546-551` spawns WAKE1 for every `u.is_naval && u.moving()` on a water cell whose cell is visible (`object_visible_at` is a shroud test, not a cloak test). So an enemy sub with no sonar contact still draws wakes.
- **Original** [CONFIRMED-RE ShipLocomotion Process `FUN_0069F2A0` @0x69F4C9-0x69F57B]: the wake is spawned only when all of these hold:
  - the unit is moving;
  - `(Frame & 7) == 0`;
  - `Type->Underwater(+0xD69) == 0`;
  - `!OnBridge(+0x8C)`;
  - `Cell->LandType == Water`;
  - `Rules->Wake` is set.
  - Underwater types never spawn a wake.
- **Fix:** add `!u.an.t.underwater` to the wake condition (`pres_effects.inc:546`).

### W7-06-05: OpportunityFire is ignored: Destroyers and Aegis Cruisers never fire while they sail
- **Severity / effort:** MAJOR (Aegis air cover) / M
- **Evidence:**
  - [CONFIRMED-DATA] `OpportunityFire=yes` on DEST, AEGIS and CDEST. The land units MTNK, HTNK and HARV also have it, so this is a cross-slice item.
  - [CONFIRMED-code] the key has no reader. `combat.cpp:1142` blocks every auto-acquire while `u.moving()`.
- **Original** [CONFIRMED-RE]:
  - The TechnoTypeClass loader stores `OpportunityFire` at +0x6AF (@0x713BDE…0x713BEA).
  - Its reader is `FUN_00708710` @0x70885F, called from TechnoClass AI `FUN_006F9380` @0x6F9BDE: when the flag is clear, the unit may fire only in mission 5; when it is set, it may fire in any mission, including Move.
  - [SUSPECTED] The caller then fires at targets in range while the move continues; the full caller path was not traced.
- **Fix:**
  - Parse `OpportunityFire` into UnitCombat.
  - In `combat.cpp:1142`, let such a unit acquire a target in weapon range without cancelling its path.
  - Turret=no hulls still need their facing gate, or the active slot needs OmniFire (the Aegis has it).

### W7-06-06: Spawn regeneration is serial; the original has one timer per node
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-code] `world_an.cpp:274-281`: one `spawn_regen_timer` rebuilds **one** spawn every SpawnRegenRate while `ready+out+pending < SpawnsNumber`.
  - [CONFIRMED-RE] `FUN_006B72E0` (spawn unlinked): the dead spawn's node goes to state 7, and its own timer (+0x8/+0xC/+0x10) starts at SpawnRegenRate (+0x30) at that moment.
  - State 7 in the AI `FUN_006B68B0` rebuilds that node when its own timer runs out. All dead nodes regenerate in parallel.
- **Effect:**
  - A Carrier that loses 3 Hornets regains the wing in 600 frames in the original and 1800 in the clone.
  - A Destroyer has one node, so it is unaffected.
- **Fix:** keep a per-node array of (state, timer) in `AnUnit` (save field), mirroring nodes 0/2/6/7.

### W7-06-07: The Naval Yard repairs for free (R-090, still open)
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-PLAY] n1 T3: DEST at 300/600 was moved to `yard_dock` and was back at 600 after 148 driver frames, with `credits=309000` before and after.
  - [CONFIRMED-code] `world_an.cpp:163-187` (comment: "the ship repairs free").
  - The land Service Depot (`world.cpp:2457-2484`) charges `repair_step_cost` per step and stops when the house is broke.
- **Original:** UnitRepair is the same BuildingClass repair mission for the depot and the yard (repair-sell.md §Service Depot, PROBABLE). RepairPercent 0.15 makes a full DEST repair cost 75 credits.
- **Fix:**
  - Drop the naval copy and let `depot_client` accept `is_naval` units when the building is WaterBound (the adjacency test is already the same).
  - That also gives the yard the depot's Terror-Drone/parasite handling. Keep the W6-04-09 heal-hit eject.

### W7-06-08: Dreadnought missiles: the volley goes in one frame, and regeneration counts from the launch
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-code] `world_an.cpp:297` (`count = spawn_ready` for missiles) and `world_an.cpp:266-273` (a missile node "is spent at once", so regeneration runs from the launch) [PROBABLE in the code comment].
  - [CONFIRMED-RE] `FUN_006B68B0` launches missile nodes 9 frames apart. Because the AI runs every 10 frames, the effective gap is 10.
  - `FUN_006B72E0` puts a node into regeneration only when its missile object is unlinked: at detonation or shoot-down, or when `ResetTarget FUN_006B7230` drops a missile still in state 2.
- **Fix:**
  - Stagger the launch through the W7-06-03 timer.
  - Start the node's regen timer at `rocket_explode` (or on shoot-down), not at launch.
  - Re-check ALL01's missile cadence afterwards; that was the reason for the current choice.

### W7-06-09: Navy SEAL vs a hovercraft uses C4; the original uses the MP5
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-RE] `FUN_006F2D80` case 4: secondary only when `!(Target SpeedType == Hover) && !Target Organic`, otherwise 0 (primary).
  - [CONFIRMED-code] `world_an.cpp:55-57`: the clone returns the secondary for any non-organic target, which includes the LCRF and SAPC on water.
- **Fix:** pass a `victim_hover` flag (SpeedType Hover) to `an_naval_slot`, and for case 4 return 0 when `victim_hover || victim_organic`.
- **Rechecked OK:** a SEAL on the shore, ordered onto an enemy Destroyer, swam out and destroyed it within 25 frames [CONFIRMED-PLAY n3 T7], so W6-04-07 holds.

### W7-06-10: The Dolphin's sonic wave (IsSonic, AmbientDamage) is not modelled (from W6 part C, never filed)
- **Severity / effort:** MINOR / M
- **Evidence:**
  - [CONFIRMED-DATA] SonicZap: `IsSonic=Yes AmbientDamage=10`; SonicZapE: `AmbientDamage=15 Burst=2`.
  - [CONFIRMED-code] neither key has a reader (`git grep`).
  - [CONFIRMED-RE] the WeaponType loader stores AmbientDamage at +0x98 (@0x770EDC) and IsSonic at +0x130 (@0x770EF4).
  - [SUSPECTED] The wave damages everything it passes; the WaveClass damage path was not traced.
- **Fix:** RE the WaveClass sonic damage loop first, then add a line-sweep `damage_area` along the shot.

### W7-06-11: The Aegis's DistributedFire and RadialFireSegments are ignored
- **Severity / effort:** MINOR / M
- **Evidence:**
  - [CONFIRMED-RE] TechnoType +0x6B0 (DistributedFire) is read six times in CanAutoTarget `FUN_006F8320` and once in Fire `FUN_006FD210`.
  - RadialFireSegments (+0x6A4) is read in `FUN_006FD210` @0x6FDECE.
  - [SUSPECTED] DistributedFire spreads the elite Burst 2 over different air targets.
- **Fix:** RE `FUN_006F8320`'s use of +0x6B0 before implementing it.

### W7-06-12: NoSpawnAlt (the Destroyer or Dreadnought drawn without its Osprey or missiles) is not drawn
- **Severity / effort:** COSMETIC / S
- **Evidence:**
  - [CONFIRMED-DATA] `NoSpawnAlt=yes` on DEST, DRED, CDEST and VLAD.
  - [CONFIRMED-RE] ObjectType loader `FUN_005F8AC0` stores it at +0x1E8 (@0x5F8C2E).
  - The clone has no reader.
  - [SUSPECTED] The alternate voxel is `<Image>WO`; not found by name in ra2md.mix.
- **Fix:** when `spawn_ready == 0`, draw the `WO` voxel if it exists.

### W7-06-13: An own submerged unit is drawn as if surfaced, and cloaking is instant
- **Severity / effort:** COSMETIC / S
- **Evidence:**
  - [CONFIRMED-code] `main.cpp:3923` and `main.cpp:4099` only *hide* an undetected enemy sub. Own and detected subs are drawn normally, with no cloak or translucency effect.
  - `CloakingSpeed` (SUB/DLPH 1, SQD 5) has no reader.
  - [SUSPECTED] The original draws own cloaked or submerged units with the cloak shimmer and fades in at CloakingSpeed. Not traced.
- **Fix:** draw `u.an.submerged` units of the local house (or detected ones) translucent; RE the TechnoClass draw VisualType first.

### W7-06-14: TooBigToFitUnderBridge (draw order under a high bridge) is not read
- **Severity / effort:** COSMETIC / S
- **Evidence:**
  - [CONFIRMED-RE] UnitType +0xE16 (loader @0x74657E). Its only readers are the UnitClass draw slots `FUN_0073A070` @0x73A0E0 and `FUN_0073B520` @0x73BD3D, so it is draw-only; movement under a high bridge is unaffected.
  - [CONFIRMED-DATA] the flag is set on every ship.
- **Fix:** with the bridge deck layer (R-100), draw these units above the deck rather than clipped under it.

### W7-06-15: The Giant Squid's DefaultToGuardArea is ignored
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-DATA] `[SQD] DefaultToGuardArea=yes GuardRange=5`.
  - [CONFIRMED-RE] the loader stores it at TechnoType +0xD39 (@0x7142E5).
  - The clone has no reader, so an idle Squid stays in plain Guard.
- **Fix:** when a DefaultToGuardArea unit goes idle, put it in Area Guard (the clone's guard_x/y leash with GuardRange).

### W7-06-16: A Dreadnought may keep sailing while its missiles launch (SUSPECTED)
- **Severity / effort:** MINOR / S
- **Evidence:**
  - [CONFIRMED-RE] `FUN_006B7400` counts the nodes in state 1, plus MissileSpawn nodes in state 2.
  - It is called from the ShipLoco move helper `FUN_006A1310` @0x6A13E6, and from Drive/Walk loco and UnitClass code.
  - [SUSPECTED] The ship locomotor waits while spawns are launching. The branch was not decompiled.
  - The clone's ships ignore launch state (`world_an.cpp` and `step_units`).
- **Fix:** RE @0x6A13E6, then hold the path while `rocket.phase < kRocketClimb` for own spawns.

### W7-06-17: INFO / out of scope
- **DNOB:** `Locomotor={4A582744…}` + `MovementZone=Water` gets the Land domain, because `main.cpp:1026` keys `is_naval` on the ShipLoco CLSID only. No stock skirmish or Allied/Soviet campaign map uses it.
- **Yuri naval (BSUB, YHVR, YAYARD, CMISL, NAPSIS SensorArray):** the clone has no Yuri side (U-5). The keys these units need:
  - NavalTargeting=7 (switch default → primary);
  - LandTargeting=2 (secondary vs land, and never auto-acquire land: CanAutoTarget `FUN_006F71D0` reads +0x604==2);
  - Unnatural (+0x694);
  - VoiceSecondaryWeaponAttack;
  - building SensorArray (parsed in `abil_w5_rules.cpp:75`, never used for sonar).

## 4. Verified OK (do not re-report)

- **Water-only movement:** Float passable only on Water; low bridges block ships (bridge cell = Road) [naval.md §2; code `world_depth.cpp:353`].
- **Ship sinking rule** matches [CONFIRMED-RE @0x736E3F: `Naval && !Underwater && !Organic && Weight >= Rules+0x630 (ShipSinkingWeight)`]:
  - the clone (`world_an.cpp:663`) sinks DEST (3), AEGIS, CARRIER and DRED;
  - HYD (2) and SUB (Underwater) explode.
- **NavalTargeting cases 0/1/2/6 and the LandTargeting=1 gate** match the original apart from the cell test (W7-06-02) [CONFIRMED-RE `FUN_006F2D80`, GetFireError @0x6FBD7B]:
  - AEGIS is air-only;
  - DEST uses ASW vs an Underwater target;
  - the Osprey hits subs only;
  - default units cannot hit a cloaked sub.
- **Sonar:** a Sensors unit reveals a submerged sub to its house within SensorsSight (W6: `sub_detected_by0=1` at once). The Destroyer cannon does not hit a submerged sub; the Osprey kills it.
- **DecloakToFire=no** (SubTorpedo, SonicZap, SquidGrab): units stay submerged when they fire (W6-04-25 fixed `b885a04`).
- **OmniFire** on Medusa / HornetLauncher / DredLauncher / ASWLauncher (W6-04-06 fixed `355a063`).
- **Sonic or heal hit frees a Squid host** (W6-04-09 fixed `b03b13d`); Squid Paralyzes pins the host; Culling kills at ≤ 25% (`world_w5.cpp:199-215`).
- **Transport capacity** is a Size pool (W6-04-08 fixed); the LCRF/SAPC beach and unload on land only (W6 / naval.play).
- **SEAL C4 on a ship** after a swim [CONFIRMED-PLAY n3 T7]. W6-04-07 holds.
- **Docked spawns** are not counted as lost (W6-04-26 fixed). Hornets crash when the carrier dies (faithful, W7-05-05).
- **The Destroyer vs a Naval Yard** works (12 per 155mm hit) [CONFIRMED-PLAY n1 T1b].
- **The AI builds a Naval Yard** (min 10 on Depth Charge) and **ships** from it [CONFIRMED-PLAY soak]. The stock aimd names no amphibious transports, so none are built (faithful).
- **Dreadnought MinimumRange 8** is enforced for units (`combat.cpp:1264`). Missiles are interceptable by AA (W4-PJ-10).
- **SinkingSound** and **MoveSound** are played (`pres_audio_events.inc`).
- **Wakes** for surface ships: every 8 frames on Water while moving, matching the original apart from W7-06-04.

## 5. Open RE questions

1. The OpportunityFire caller path from `FUN_006F9380`: does the unit keep its move while it fires, and is a Turret=no hull exempt from the facing gate? (W7-06-05)
2. The DistributedFire target spread in `FUN_006F8320`. (W7-06-11)
3. The ShipLoco launch hold @0x6A13E6. (W7-06-16)
4. The sonic WaveClass damage sweep for IsSonic / AmbientDamage. (W7-06-10)
5. Whether Naval Yard repair requires a dock order (as the Service Depot does), and its cost cadence. (W7-06-07)
6. Whether allied houses share Sensors detection. The clone uses per-house bits only (`world_an.cpp:151-162`).
