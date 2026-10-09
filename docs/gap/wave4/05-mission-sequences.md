# Wave-4 gap analysis 05: scripted sequences and mission flow (ALL02-ALL12, SOV01-SOV12)

This is a read-only audit of master `4647b17`, done on 2026-10-07. The worktree is `D:\ra2-wt\w4-seq` (branch `w4-seq`,
Release build `build-rel`). Nothing is committed and no engine source was changed. ALL01 is out of scope (agent 03). So are
unit positioning, input control and projectiles (other wave-4 agents), except where they shape a scripted beat.

The question here is not "can the trigger chain reach Win" (wave 2 proved that with cheats). It is "does the mission play
out the way the map's own triggers, teams and scripts intend": intro sequences, enemy waves, AI-vs-AI set pieces,
reinforcement arrivals, objectives and end sequences.

## R3 status (mission-win agent R3, branch `w6r3`: SOV09 + SOV08)

Real orders only (no `camp_*`/kill/reveal/spawn cheats), Easy (slider 0: the player reloads x0.8, every computer
house x1.2).

| Mission | Before | R3 | Evidence / what blocks |
|---|---|---|---|
| SOV09 | PROBABLE (`m-sov09-real` FAIL since W6) | **YES (proven)** | `m-sov09-real` **WON**: driver frame 8772 (campaign ~11600), run ends f9612, `play_hash=be172b860ffabe9a` (two identical runs), generator `gen/mk_sov09.py`. Easy drops **three** Yuris; input unlocks at ~65 s while they walk into the Prisms, so they are pulled back the moment `input_locked` clears. A Yuri takes the x=37 IFV patrol, Yuri 0 the British engineer, which captures the Alliance barracks (dogs then clear the door GIs and the American GI/dogs south of the compound). The **IFV force-fires the compound's south wall at (66,71)** from (66,75) (out of every Prism's 8 cells) until the section is gone, and from the gap shells the three plants it can reach -- (68,66), (70,66), (68,64): output 1000 -> 400 < 575, all Prisms offline. No Africans unit touches the 36-cell `Player in Base` ring (all of it lies inside the walls), so the Americans never Hunt. Yuri 1 takes the President's British guard SEAL from 7.3 cells (its acquisition is range 6 + 1); Yuri 2 stands at (67,68), 7.6 cells from the President's (64,61) stop and from the guard on it, and takes him when he steps there (the President shuttles (63,60) <-> (64,61) every ~400 frames on Easy; he never walks to (61,65)). `Pres is Captured` -> Win. The north plant / SEAL-C4 route of the W6 plan was not needed: the map's [MindControl] is Range=8. `m-sov09` (cheat scripted capture) PASS unchanged |
| SOV08 | YES (not proven) | not won | Best attempt `gen/mk_sov08.py` (no .play). The White House (4x4) is in V3 range up to 20 cells from its centre (18 + (4+4)*64 leptons); the only sites inside `[Map] LocalSize` are the walled pocket west of it (x 83-92, y 13-29) and the road at Vlad2's war factory. 10 V3s destroy the pocket's two Conscript garrisons (Vlad2 Garrison1/2, script 46 -> CANEWY12 (88,26), CAWASH07 (88,20)) and the Tesla Coils (94,23), (97,28) from outside their reach. It stops when the 16 freed Conscripts, Vlad2's Shock Troopers, Rhinos and the Flak Track (98,24) counter-attack the pocket: the Apocalypse escort has no anti-infantry weapon and the V3s die before the White House is seen. The W6 "only one V3" was **not reproduced**: four or six cameo clicks (`train()` or single clicks) queue and build four or six V3s on this tree; that attempt most likely ran out of money (the Easy economy dies with its harvesters to Vlad2 raids) |

Engine fixes (each a separate commit, with a unit test):

- **PlayerControl houses take force-fire, Stop, Guard and Scatter** (`07f034e`, `engine/unit_orders.cpp`,
  `engine/world_depth.cpp`) [RE: gamemd StopCommandClass::Execute `FUN_0072ff70` (event 6), Guard `FUN_0072fe30`
  (mission 0xB), Scatter `FUN_007300b0` (event 7): each walks CurrentObjects and orders every selected object the
  player commands -- no Owner == PlayerPtr test]. The helpers tested `house == 0`, so a mind-controlled IFV ignored
  Ctrl+click (the SOV09 wall breach). Ghidra: private copy `D:\ra2-re\r3`, scripts `r3_cmd.py` / `r3_exec.py`.
- **An attack on a building stops at weapon range** (`afb160d`, `engine/world.cpp`) [RE: infantry-anim.md §7 --
  Mission_Attack re-checks range every frame while approaching any target and stops when in range; FUN_006F6750
  measures a building to its centre with range + (w+h)*64]. The hold-at-range pass covered unit targets only: a
  unit ordered onto a structure drove to its foundation (V3s into Vlad2's base; SOV09's IFV from the gap into the
  Prism compound). Gate re-pins with A/B proof in README: `--demo-base`, `--demo-ai-team`, `--demo-ai-trigger`.
- **A flyer cruising to a flight goal does not return fire** (`c41c8be`, `engine/combat.cpp`) [RE-GATE: aircraft.md
  §6, the gate acquisition already used]. Retaliation tested `!moving()` only, which is always true for a flyer
  (empty ground path): Kirovs ordered across the map stopped on the first IFV that hit them. Exposed by
  `m-sov12-real` after the building hold changed the AI's timing; it WINS again (f8606).

Harness (no sim change, `a5a140c`): `cheat map_levels PATH` (cell heights) and, with `RA2_CAMP_DEBUG`, the base
defences aiming at a unit on its death line (` def=TYPE@x,y`).

Other scripts (scoped re-run, final tree): `m-all01`, `m-sov01`, `m-sov09`, `m-sov01/04/07/11/12-real`,
`m-sov02-real`, `m-sov02-mcv`, `m-all04/07/12-real`, `m-all09-opening`, `m-trn02-real`, `sk-a` PASS.
`m-trn02-real` needed a script change (`gen/mk_trn02.py`: `building 1:TYPE:near`; with the hold the GI squad
stands in front of its target and a centre click hit a selected GI). **`m-all03-real` now FAILS (LOST at f12689)**
because of the faithful building hold: ALL03's map-placed Rhino raid now shells the base from 6-7 cells south
instead of driving up to the buildings, beyond the reach (range + 1) of the Grizzlies idling north of it, and the
base falls at f9090-10125 (area guard or parking the tanks south did not save it in three tries); it needs a
re-tuned defence. `mc-range` and `sk-s` fail with and without the R3 building hold (pre-existing on this tree).
R1/R2 scripts were not run here; any proof that relied on attackers walking up to buildings may move.

## W6 fix #2 status (wave-6 fix agent 2, branch `w6fix-missions`)

Engine fixes from docs/gap/wave6/02-missions.md (W6-02-01..05, 07, 08, 11, 12, 14..18): per-house difficulty handicap (Easy really is easier now: player ROF x0.8, every computer house and its defences x1.2), the Evaluate_Object target rejects (ALL12's Moscow is no longer razed, so `Kremlin is Destroyed` no longer fires; SOV10's WCD is not a target outside the playfield), the original threat radius (weapon range + 1, area guard x2, never Sight), ImmuneToPsionics, team tags on recruits (ALL05 is lost when Tanya dies), build events by rules id (TRN02 objective 6), full-strength team start, recruit filters, Patrol = attack-move.

- **TRN02 (Boot Camp 2): WON by real orders** (`m-trn02-real`, `gen/mk_trn02.py`).
- **ALL09 paradrop correction (W6-02-19):** the captured Tech Airport grants ParaDropSpecial (Type=ParaDrop): an Allied owner drops AllyParaDropNum = **6** GIs per cycle (AmerParaDrop's 8 is the American power).
- Real-order scripts re-run after the fixes: see the table in docs/gap/wave6/02-missions.md ("Mission scripts after the fixes"). SOV01 / SOV07 / SOV12 / ALL12-opening adapted; ALL05, ALL06, ALL10, ALL11, SOV09 now fail with the bisected causes listed there.
## W6 restore R1 status (branch `w6r1`, on master 11ea069)

Generator-only changes; no engine bug found, no gate re-pinned.

| Script | Result | Difficulty | play_hash | Change |
|---|---|---|---|---|
| `m-all05-real` (`gen/mk_all05.py`) | **WON** f5561 | Normal (loading-screen default) | `dd42d4dd6a32152e` | The bisected cause (W6-02-11) was not the spy: the first spy still reaches the red plant (blackout at f~710). Tanya, left idle while the spy walked, auto-acquired a passing Conscript with the original threat radius, chased it north to (108,119) and the red dogs one-shot her at f~670 ('Tanya Dies' -> LOST, W6-02-14). She now keeps the dog-first micro (`act`) until the blackout. |
| `m-all06-real` (`gen/mk_all06.py`) | **WON** f34825 | **Easy** (`set campaign ALL06u.MAP GDI 0`) | `3d03f0b40d706910` | First divergence vs the pre-wave-6 win: f900 (Prism demo, BadGuy1 37 units vs 36); by f6000 the start force is down to 13 units (was 22), BadGuy1 fields 35 units at f12000 (was 23), massing 55 Grizzlies takes until f21157 (was f18875). BadGuy2's Shock Troopers, Rhinos and a rebuilt Tesla Coil destroy the 58-tank sweep (killers: SHK, HTNK, defences; 58 -> 0, BadGuy2 loses 3 buildings). On Easy the same plan razes BadGuy2 with 54 tanks left. Sweep clicks are now sent only while a Grizzly is selected (one `click unit 0:MTNK:0` landed on a building and selected nothing -> `no building 2:TESLA:near`). |

Also re-run, unchanged: `m-all01` PASS (f2304, `eb02e33e4f82942d`), `m-sov01-real` PASS (f6679, `7df46a58430c01e5`).

Observed, not changed (outside this task): BadGuy2's dogs fire at Grizzlies (~580 `BadTeeth` shots in 500 frames during the sweep). `BadTeeth`'s warhead `ParasiteDog` has Verses 0% against heavy armour, which the original treats as no legal target. No Grizzly death was credited to a dog, so the shots do not affect the result.

- **W6 restore R2 (branch `w6r2`): ALL10 and ALL11 WON again with real orders.** RE re-check first (Ghidra headless, private project copy): the full-strength start (`FUN_006E86B0` begins a team only when `+0x79` IsFullStrength or `+0x77` IsForcedActive; `FUN_006E9950` sets `+0x79` when the member count equals `FUN_006E76D0` = the sum of every TaskForce count; `FUN_006E9640` regroups a begun Reinforce=yes team) and Patrol (`FUN_006EC240`: every N frames the strongest member runs Greatest_Threat(THREAT_RANGE), retarget `FUN_006E85C0`, attack `FUN_006EA9F0`, else regroup/move) both match the engine, so no engine change; the generators were adapted.
  - `m-all10-real` (`gen/mk_all10.py`): **WON f=23052, Normal** (loading-screen default), play_hash `8583dbd59315b552`. BadGuy2's waiting 14-unit team at its Con Yard broke the army after it had taken BadGuy3 and BadGuy1 with no income left (the start Chrono Miner died, ~9400 credits sat idle). Change: two extra Chrono Miners after the second war factory, and the lab guard (GIs, Rocketeers) joins the final sweeps.
  - `m-all11-real` (`gen/mk_all11.py`): **WON f=17355, Easy**, play_hash `03b9b3a41ee6b2b8`. On Normal the landings now take the radar (the SEAL / Prism prerequisite) and the base. On Easy the old plan failed in four places, all fixed in the generator: (1) only 1 of 10 SEALs boarded -- the IFVs stood packed at the factory exit, and a still-selected SEAL turned the next IFV click into a board order (now: war-factory rally to a holding ground south of the base, IFVs spread on their own cells, descending-index pairing so a mid-sweep boarding only shifts already-clicked indexes, a right-click deselect between phases); (2) the loaded IFVs chased landings at guard range and died while the Chronosphere charged (now: the SEALs/IFVs are trained only once the Chronosphere is charging, Grizzlies + 3 pairs of Prism towers hold the base, the group is called back to the holding ground every 150 frames); (3) only ~5 IFVs were inside the lifted 3x3 -- the old stage (116,96) sits on a level-3 slope where several cells cannot be reached by a click (now: per-cell Alt force-move parking on the flat (119,106), a conditional second pass, box select); (4) the shifted SEALs take two silos and die on the way to silo A (now: a second chronoshift wave; only SEALs on Cuba get the C4 orders).
## W4-FIX-SEQ status (wave-4 fix agent 4, branch `w4fix-seq`)

The same no-input runs (`scratch\play\sq-*.play`, 400 campaign seconds) were re-run on the fixed tree; real-order play
scripts were added where the player must act (`scripts/play/m-sov02-real`, `m-sov02-mcv`, `m-all04-real`,
`m-sov09-real`).

| ID | Status | What changed / evidence |
|---|---|---|
| W4-SQ-01 transport cargo | **Fixed.** | [RE: FUN_0065d770] Full=yes reinforcements create the non-transport members INSIDE the last-created transport (no capacity test; transport = Passengers>0, a UnitClass, or an AircraftClass when the TaskForce has an aircraft transport type FUN_006EE9D0; Naval types only when the TaskForce has one, TechnoType +0xCCE). [RE: FUN_006EE670] script 8 Unload orders every member transport to unload and completes only when all are empty; its arg mode then decides who stays in the team: **0 everyone, 1 the transports (unloaded units leave), 2 the unloaded units (transports leave), 3 nobody**; TransportsReturnOnUnload sends the transports home. Script 14 Load (FUN_006EC760) and 43 WaitLoaded (@0x6E9192) implemented. Carried members are skipped by team coordination (IsOnMap). SOV02: the player keeps 20-22 units (was 0 at 180 s), only the empty SAPCs change house; the MCV chain works (`m-sov02-mcv`: SMCV unloaded with mode 0, `MCV ON the Beach` hands it over). ALL05: the 3 spies reach the Spies house at 13 s and stay (Reinforcement house losses 1, was 49 -- no loop; Tanya no longer shoots the heli, see the alliance row). ALL04: the LCRFs carry the AMCV / Grizzlies / GIs; `m-all04-real` drives the craft to the beach and unloads it with the Deploy key. |
| W4-SQ-06 paradrops | **Fixed.** | [RE: FUN_0065d320 Droppod branch] a PDPLANE (name string @0x838700) enters flying at the TransportWaypoint (UseTransportOrigin) or on the house's Edge= (FUN_004AA350; RE-FALLBACK: the edge cell straight up/right/down/left of the drop point), carries the members and drops them one per frame on the free cells around the waypoint; they fall under the parachute at ParachuteMaxFallRate (5 leptons/frame) drawn with the art Paradrop sequence; the team script starts once all are down. ALL09 `Seals have Landed` at 9 s (was 119 s), SOV10 `Landed` at 30 s (was 120 s), ALL08's start force falls, SOV09's Yuris and drop groups fall. Plane exits are not counted as losses. |
| W4-SQ-04 VeteranLevel | **Fixed.** | [RE: FUN_0065d770 -> FUN_0074EE80/EEB0/EED0] 2 = veteran, 3 = elite on every created member (0 = -0.25 "green", no stat effect). ALL09/ALL12 SEALs `urank=2`. |
| W4-SQ-02 garrison script arg | **Fixed.** | [RE: FUN_006EE130] 46/47/58: type = arg & 0xFFFF (BuildingTypes position), selector = arg >> 16 (2 nearest, 3 farthest; 0/1 threat RE-FALLBACK by HP); candidates owned by a non-allied or MultiplayPassive house. An Occupier "attacking" an occupiable building it may enter goes inside (CanBeOccupiedBy FUN_00457CE0 now accepts MultiplayPassive owners). ALL04: 85 Conscripts in 9 buildings; ALL03: 28 in 4; SOV08: 24. Note: the all-infantry reinforcement branch of FUN_0065d320 is a door exit next to an adjacent building (KickOutUnit), not a garrison -- the clone's placement around the waypoint stands in for it. |
| W4-SQ-05 Sleep/Harmless | **Fixed.** | Placed Mission=Sleep/Harmless objects never acquire or retaliate until 81 WakeupSelf / 82 WakeupAllSleepers / 83 WakeupAllHarmless. ALL12's guards and prisoners no longer fight (they are also mutual allies, see below). |
| W4-SQ-03 / -08 aircraft leaving | **Fixed (no-dock case); ALL02 corrected.** | [RE: AircraftClass mission FUN_004176F0: AirportBound + no free dock -> the no-dock exit; empty team drop planes Retreat] a team-less computer fixed-wing aircraft with no live airfield flies off the nearest edge. **ALL02's intro Harriers have a dock**: the British GAAIRC at (58,86), the target of `Airplanes are US` -- they return and park there, which is the map's intent. |
| W4-SQ-10 SOV01 bridge | **Fixed.** | [RE: FUN_006DFA00 calls FUN_00586BB0 on the waypoint cell up to 4 times; FUN_005766D0 concrete / FUN_00570FC0 wood] destructible high bridges: BRIDGE1/2 + BRIDGEB1/B2 deck cells carry ground units while intact (the clone blocked every high bridge before), a break takes the connected span and kills ground units on it, the deck stops drawing, the hut repairs it. SOV01 `Break Bridge`: `a63 -> ok [bridge cells broken=1/3]` at f0; ALL01's 10 `Break Bridges` actions break both bridges (wp79/wp80). RE-FALLBACK: whole span at once; ramp tiles keep their art. Unit test `test_high_bridges.cpp`. |
| W4-MC-08 SOV09 early loss | **Fixed (cause found).** | Not a timer: the clone's 2-team alliance put the Russian drop groups (Allies=Alliance,Russians,Neutral) on the Americans' side, so their 35 hunting Conscripts killed the Yuris. Houses now see each other through their OWN Allies= list (HouseClass::IsAlliedWith, one-sided); the player's side (team 0) unchanged. Also: a player order releases the unit from its script team (W4-MC-06, RE-FALLBACK) and a mind controller holding a victim is not re-armed by its team's Attack (the CAR/IFV ping-pong). `m-sov09-real`: Yuris paradropped, pulled back by real clicks at battle control, alive past the 100 s loss arm, mission RUNNING at 147 s. |
| W4-SQ-12 AI members for scripted teams | **Done [RE-FALLBACK]** (RE-W4-SQ-2 not traced). | A recruiting CreateTeam team of an AI-enabled computer house hands its missing TaskForce types to that house's AI-2 production (`World::set_ai_script_demand`); the recruit wait restarts on every recruit. |
| W4-SQ-11 actions/events | **Partly.** | Done: 18 RevealZone (connected area, 64 cells; ALL07), 5 HouseDiscovered (SOV10 `Bad Guy 3 Startup`), 81/82/83. Open: 61/62 TurnOff/On (ALL10 Chronosphere), 35 PreferredTarget, events 23/34, SOV09 `Switch Power House` changed=0, ALL10 FireSale skipped. |
| W4-SQ-07 late LZ beats | **Fixed** (falls out of 01/06). | |
| W4-SQ-09 input lock | Owned and fixed by W4-FIX-CTL. | |

### Real-play verdict, before -> after

| Mission | Before | After | Why |
|---|---|---|---|
| ALL02 | YES | YES | unchanged; intro Harriers dock by design |
| ALL03 | PROBABLE | PROBABLE | 4 Soviet garrisons now occupied (harder, as designed) |
| ALL04 | PROBABLE | YES (probable) | craft carry the MCV; player beaches + unloads (`m-all04-real`); 85 Conscripts garrisoned as designed |
| ALL05 | PROBABLE | YES (probable) | spies delivered once to the player's Spies house; spy objective possible |
| ALL06 | YES | YES | landing craft carry the start force |
| ALL07 | PROBABLE | PROBABLE | RevealZone done |
| ALL08 | YES | YES | start force by plane |
| ALL09 | PROBABLE | YES (probable) | elite SEALs land in 9 s; no-input run no longer LOST |
| ALL10 | YES | YES | |
| ALL11 | PROBABLE | PROBABLE | paradrops + invader cargo delivered |
| ALL12 | PROBABLE | **DOUBTFUL** | no-input LOST at ~17 s: the 3 elite SEALs drop among 16 guard Conscripts + 8 dogs that (faithfully) no longer fight the prisoners; a real-order focus-fire trial also lost. Needs a combat-balance check of SEAL vs Conscript/dog in the clone |
| SOV01 | YES | YES | bridges broken at f0, high bridges crossable, hut repair |
| SOV02 | **NO** | **YES** | landing force kept, MCV chain proven (`m-sov02-real`, `m-sov02-mcv`) |
| SOV03 | YES | YES | |
| SOV04 | YES | YES | |
| SOV05 | YES | YES | |
| SOV06 | YES | YES | |
| SOV07 | PROBABLE | PROBABLE | |
| SOV08 | YES | YES | garrisons occupied |
| SOV09 | DOUBTFUL | PROBABLE | the forced early loss is gone; Yuris survive under real orders (`m-sov09-real`); the President capture through the Prism compound was not reproduced by script clicks (wave-2 proved the capture itself) |
| SOV10 | YES | YES | `Landed` at 30 s |
| SOV11 | YES | YES | |
| SOV12 | PROBABLE | PROBABLE | |

Summary: before 12 YES / 9 PROBABLE / 1 DOUBTFUL / 1 NO; after 16 YES / 6 PROBABLE / 1 DOUBTFUL (ALL12) / 0 NO
(SOV09 PROBABLE).

## W5-A status (wave-5 fix agent A, branch `w5-camp`)

Real-order proofs added: `m-sov01-real` (SOV01 **WON**), `m-all03-real` (ALL03 **WON**), `m-all12-real` (ALL12 opening:
the SEALs reach the reactor through the fence's corner gap, C4 it, the Chrono MCV / IFVs / Grizzlies arrive),
`m-sov02-mcv` now cheat-free. Root causes fixed on the way: no campaign economy (no harvesting in any mission),
wall corner gaps sealed by the pathfinder (ALL12), PlayerControl houses unable to take attack orders and Yuri
mind-control ping-pong (SOV09), EnteredBy raised after the trigger loops (ALL02 lost on a race), recruit priority
(SOV02 MCV boat), wall-clock crate seed (non-deterministic proofs).

| Mission | W4 after | W5-A | Evidence / what blocks |
|---|---|---|---|
| ALL01 | YES | YES | hand-over cells = the map's own CellTags (61..64,84) |
| ALL02 | YES | YES | `m-all02` (engineer capture) |
| ALL03 | PROBABLE | **YES** | `m-all03-real` WON by real orders (needed the campaign economy) |
| ALL04 | YES (probable) | YES (probable) | `m-all04-real` opening |
| ALL05 | YES (probable) | YES (probable) | not re-run |
| ALL06 | YES | YES (probable) | not re-run with real orders (Destroy BG1+BG2) |
| ALL07 | PROBABLE | PROBABLE | not attempted |
| ALL08 | YES | YES | `m-all08` |
| ALL09 | YES (probable) | YES (probable) | not attempted |
| ALL10 | YES | YES (probable) | FireSale/TurnOff now modelled; not attempted |
| ALL11 | PROBABLE | PROBABLE | PreferredTarget now modelled; not attempted |
| ALL12 | DOUBTFUL | **PROBABLE** | `m-all12-real` opening proven; a direct Grizzly/IFV/Prism assault (29 units) died to BadGuy1's base; the nuke (IQ 5) hits the ConYard at ~10.5 min. Needs a SW/Chrono strategy |
| SOV01 | YES | **YES (proven)** | `m-sov01-real` WON |
| SOV02 | YES | YES | `m-sov02-real`, `m-sov02-mcv` (cheat-free) |
| SOV03 | YES | YES | `m-sov03` |
| SOV04 | YES (probable) | YES (probable) | not attempted |
| SOV05 | YES | YES | `m-sov05` |
| SOV06 | YES | YES | `m-sov06` |
| SOV07 | PROBABLE | PROBABLE | two defence scripts lost by ~10-15 min (spy/waves; Tesla needs NARADR); needs a tuned build |
| SOV08 | YES | YES | not re-run |
| SOV09 | PROBABLE | PROBABLE | Yuris now obey (attack order + MC fixes); MC'd British engineer captures the Alliance barracks; blocked by **captured-factory owner rule** (HouseClass +0x2C4..+0x2D0 bits in CanBuild FUN_004F7780 not modelled -> no Allied spies for the power-plant blackout). **P2: disproved** -- the bits are never set in either exe (see "P2 status") |
| SOV10 | YES (probable) | YES (probable) | not attempted |
| SOV11 | YES (probable) | YES (probable) | not attempted |
| SOV12 | PROBABLE | PROBABLE | not attempted |

## P5 status (mission-proof agent P5, branch `p5-missions`)

Real-order proofs for SOV10-SOV12 (no `camp_*`/kill/reveal/spawn cheats; generators in `scripts/play/gen/`).
All three were played at the loading screen's default difficulty.

| Mission | W5-A | P5 | Evidence / what blocks |
|---|---|---|---|
| SOV10 | YES (probable) | **NO (tactical blocker)** | The WCD (`GAWEAT`, 200-HP wood in this map, the `WCD Destroyed` win) stands outside `[Map] LocalSize` (x-y = -78, the view's left edge is -66): it cannot be clicked or nuked until `BL Captured` runs `ResizeView`. The Battle Lab sits on BadGuy1's cliff-ringed island; the only ground access is the east beach (182,117) and up through the base (7 Prism towers, 8 pillboxes). A landing craft sent at the lab is killed on that beach (probe, with and without BadGuy1's fleet). Needs a landed assault (2 SAPC x 2 Apocalypse + an engineer wave) plus a Nuclear Silo charged for the WCD -- not achieved. Also observed: with the player's base alive past ~14 min, BadGuy2's Harriers (`Harassment Team_5`) sometimes bomb the WCD and win the mission for the player -- `Other3` is hostile to BadGuy2 by its own `Allies=` list, so the original may do the same (not RE-verified; open). `mk_sov10.py` (WIP) builds to a charged nuke. |
| SOV11 | YES (probable) | **YES (proven)** | `m-sov11-real` WON (8289 frames): base to battle lab, two Kirovs built, all four flown by move orders along the least-flak line (BadGuy1's SW corner, the west map edge) onto the Kremlin. Notes: a nuke is useless here (`[NUKE]` Verses concrete 8%); BadGuy3's Kirov counter-attack comes only after a Nuclear Silo is built. |
| SOV12 | PROBABLE | **YES (proven)** | `m-sov12-real` WON (8381 frames): two Kirovs fly the patriot-free north coast to the civilian airfield, `Kirovs Gained` gives the three captured ones, the five bomb the Chronosphere (750, concrete). The island base falls to the chrono raids meanwhile -- won only after the DestroyedAll fix below. A click on the shrouded Chronosphere is a move; the attack order is re-clicked until the airships see it. |

Engine fixes (each a separate commit, calibrated in gamemd.exe):

- **AllyReveal one hop** (`a8e9403`, `engine/world.cpp`) [RE: FUN_00566fd0 per-object reveal; FUN_004f9870
  IsAlliedWith]: an object reveals for a viewer only if its house is the viewer or counts the viewer as an ally (the
  source's own view). The clone OR-merged each ally's accumulated explored map every frame, so sight chained
  Civilian -> Other1 -> Player: SOV10's player "discovered" BadGuy3 at f3 and `Bad Guy 3 Startup` (HouseDiscovered)
  ran at f8. Shared system (shroud). Gates unchanged.
- **DestroyedAll counts flying vehicles** (`694a0cc`, `engine/campaign_runtime.inc`) [RE: FUN_0071dbe0 case 9/0xB:
  buildings +0x2F0, OwnedUnitTypes +0x5564, OwnedInfantryTypes +0x5578]: membership by rules list, so a Kirov (ZEP is a
  `[VehicleTypes]` UnitClass) keeps the house alive. Shared system (campaign events 9/11). Gates unchanged.

## P2 status (mission-proof agent P2, branch `p2-missions`)

Missions SOV07, SOV08 and SOV09 from the real campaign start with real player orders only (no `camp_*`, kill, reveal
or spawn cheats). Scripts are generated by `scripts/play/gen/mk_sov07.py` / `mk_sov09.py`.

| Mission | W5-A | P2 | Evidence / what blocks |
|---|---|---|---|
| SOV07 | PROBABLE | **YES (proven)** | `m-sov07-real` **WON** (driver frame 33255, 0 failures, `play_hash=0d53929454916007`, two identical runs). Power, refinery, war factory, **Flak Tracks first** (the 400 s Rocketeers target the Battle Lab; Tesla Coils cannot hit air -- this is what lost W5-A's two tries), Apocalypse tanks, a second factory, radar, four Tesla Coils, then one Apocalypse + one Flak Track per round rallied by the lab; after the 2550 s `Massive Last Attempt` the army hunts the last BadGuy1 units -> `PlayerWin1` -> `PlayerWin2` |
| SOV08 | YES | **PROBABLE** (not proven) | No real-order win. What was learned: the SMCV cannot unfold at its start cell (move it to (59,97) first); clicking a cell under a selected Desolator deploys it (harness gotcha -- the paratroop defence can use it on purpose); Vlad1's three 15-Conscript paradrops (60/120/210 s) land around the start and, with Vlad1's base next door, wipe a base built there; a starting-army rush on the White House dies in Vlad2's base; the White House pocket is walled (x=93) and its south gate is covered by four Tesla Coils (range 10) and two sentry guns. Needs a defended economy and a V3/heavy-army push -- tactics, no engine blocker found |
| SOV09 | PROBABLE | **PROBABLE** (not proven) | `m-sov09-real` now proves, cheat-free: the Yuri pull-back, the IFV patrol mind-controlled (and released from its team -- engine fix below), the British engineer mind-controlled from **outside** the door GIs' reach capturing the Alliance barracks (dogs then clear the GIs), and the power-plant SEAL mind-controlled so its C4 destroys the north plant: American power **1000 -> 800** (drain 575). **No captured-factory engine change is needed** -- see the RE note below; the map's "spies" hint has no working mechanism in either exe. What blocks the win: the Prisms go offline only below 575, and the other four plants sit in the walled Prism compound; from outside, only one more plant ((68,66), from (67,73)) is in reach (-> 600), and the roaming American SEALs (`MP5` 125 damage, ROF 10, range 6, YR data) kill every dog, Yuri or IFV that comes near. Tactics, no engine blocker found |

Engine fixes (each a separate commit on `p2-missions`, calibrated in `gamemd.exe`, Ghidra copy `D:\ra2-re\p2-camp`):

- **Power output scales with health** -- `b0cd134`, `engine/production.cpp` power_totals. [RE: BuildingClass::Power_Output
  `FUN_0044E7B0` @0x44E85F..0x44E86A: `ftol(Health / Type.Strength * PowerBonus)` (`FUN_005F5470` = health ratio, `fimul`,
  `ftol`); drain `FUN_0044E880` is not scaled.] construction.md §8 had recovered it; the clone counted full output.
  Unit test `power_totals: output scales with health`. No pinned gate moved (skirmish, saveload, campaign, base, econ,
  prod, ai-base, ai-team, ai-trigger, capture, repair, superweapon, triggers, abilities, veterancy, techtree all print
  their README pins).
- **A unit mind-controlled by a human house leaves its script team** -- `bac04a4`, `engine/campaign_runtime.inc`
  poll_mind_control. [RE: FootClass::SetOwningHouse `FUN_004DBDE0`: after TechnoClass `FUN_00700940` sets the owner,
  `if (Owner->+0x1EC && Team) FUN_006E9DD0(...)` removes the foot from its TeamClass.] Before, SOV09's controlled IFV
  kept driving its patrol route into the Alliance GIs.

**RE note -- captured factories do NOT unlock foreign tech.** `HouseClass::CanBuild` (`FUN_004F7780` @0x4F7917..0x4F7981)
does test four per-house bitfields (YR +0x2C4 infantry / +0x2C8 unit / +0x2CC aircraft / +0x2D0 building, against the
type's Owner bitmask `+0xDA0`), but a byte- and capstone-level scan of the whole `.text` finds **no writer except the
HouseClass constructor** `FUN_004F53B0` (zero at @0x4F5789..0x4F579B, EBX = 0) -- no OR/MOV/BTS store, no LEA/immediate
pointer to those fields, no indexed `[reg+reg*4+disp]` store. RA2's `game.exe` has the same check at @0x4E379F (fields
+0x208..+0x214) and the same ctor-only writer @0x4E1BAD. The bits are therefore always 0 in the shipped games, and a
Soviet house that captures an Allied barracks trains only types its own country may build (here: Attack Dogs and
Engineers, whose `Owner=` lists every country). The clone's country-filtered roster (`rp_country_allows`) is already
the original's behaviour -- implementing the bits would be a fidelity regression, so it was not done (save version
stays **v22**).

Harness additions (`f152dc8`): `power_out(h)` / `power_drain(h)` / `low_power(h)` probe keys for every AI house, and
`cheat map_ascii PATH` (debug only) for route planning; `RA2_CAMP_DEBUG` death lines name the killer unit.

## P4 status (mission-proof agent P4, branch `p4-missions`)

Missions ALL09, ALL10 and SOV04 from the real campaign start (the loading screen's default difficulty) with real
player orders only (no `camp_*`, kill, reveal or spawn cheats). Generators: `scripts/play/gen/mk_sov04.py`,
`mk_all10.py`.

| Mission | W5-A | P4 | Evidence / what blocks |
|---|---|---|---|
| SOV04 | YES (probable) | **YES (proven)** | `m-sov04-real` WON (f=13875): the base grows by the northern gem field (`place auto` anchors on the map's lamp post, BaseNormal like the original), ~35 Rhinos trained during the 10-minute timer and staged at (108,48); when the Allied MCV lands the type-selected Rhinos kill it and every BadGuy1 object in turn, four Typhoons hunt the destroyers; `Win Timer` (DestroyedAll BadGuy1). |
| ALL10 | YES (probable) | **YES (proven)** | `m-all10-real` WON (f=15074 after the master merge): GIs/Rocketeers guard Einstein's lab (barrels beside it), refineries + a 2nd war factory, Grizzlies/IFVs while the invasion breaks on the base; the army (Grizzlies, IFVs, Mirages) clears BadGuy3, BadGuy1, BadGuy2 one house at a time (ConYards first), regrouping before each. Needed the barrel-chain fix below: before it the mission was **won with no input at 7 min** (see engine fixes). |
| ALL09 | YES (probable) | **NOT WON (tactical; no engine blocker known)** | `m-all09-opening` (PASS, `end expect RUNNING`, f=16654, hash `d78158acd471b2e3`; after the P3 C4 merge it is cut at phase 5 -- airport captured, 33 GIs -- because the longer run no longer reproduces) proves by real orders the opening below; before that merge the same generator (`mk_all09.py out.play 9`) went on, deterministically: the SEALs clear the village, both camps are freed (the Grizzlies crush out through the sandbags and open the POW fence -- two engine fixes below), the Hack engineers land and the village holds against the POW-check teams and the flak patrol, an engineer captures the Tech Airport (ParaDrop: 8 GIs every 4 min), the paradropped GI wave destroys Pyramid 1 (half of objective 1), an engineer captures Reactor 2, the SEALs C4 Reactor 1, and an engineer behind the GIs **captures the BadGuy1 Construction Yard**. After that BadGuy1's `Attk Response` teams (4 HTNK, 3 HTK, 10 SHK + 20 E2 recruited from the base) overrun the small force before a base can be raised; a win needs a finer, reactive micro script (or many more paradrop cycles before the push). |

Engine fixes (each a separate commit; shared systems flagged):

- **Building death weapons + DelayKill** (`2a3d27f`, `engine/world_abilities.cpp`, `main.cpp`) [RE: FUN_00700DA0 death
  path -> FireDeathWeapon FUN_0070CAC0 for every TechnoClass; delay kill @0x701327..0x701403; expiry in BuildingClass::AI
  0x440152..0x4402A0]: Explodes=yes buildings (barrels CAMISC01/02, AMMOCRAT) fire DeathWeapon (BarrelExplosion,
  OilExplosionWH CellSpread 4); a lethal CausesDelayKill hit on an EligibleForDelayKill building sets the C4 timer slot
  (+0x6DF/+0x528/+0x530) to ftol(F + (F*AtMax - F) * dist / (ftol(CellSpread)*256)) frames (a running shorter timer is
  kept), Strength = 1, then dies by C4Warhead. Before: ALL10's `German Explode` blew 3 barrels per village and nothing
  chained, the German houses lived ~100 s longer, every Soviet MCV landed after `Kill BGn Local Guy` had destroyed its
  volatile tag (and `BGn Production Start` with it), FireSale state 4 made them hunt, and ALL10 was won with no input.
  Interactive only (GameScene), save v23.
- **House counters skip Insignificant / DontScore** (`fe23895`, `engine/campaign_runtime.inc`) [RE: RegisterGain
  FUN_004FF520 @0x4FF53B..0x4FF55B; keys verified at 0x5F8D0B (Insignificant +0x232) and 0x7132F8 (DontScore +0xC9F)]:
  lamp posts, barrels, ammo crates, civilian props, gates and spent V3 rockets no longer keep a house alive for events
  9/10/11/55/56 (map overrides apply: ALL09 `MAYAN Insignificant=no`). **Shared system**: it changes the win/lose
  counts of ALL01/02/06/07/08/09/10 and SOV04. P3 fixed the same in parallel; the master merge keeps P3's
  `CampaignRuntime::tracked()` implementation (plus P5's flying-unit class).
- **An ordered Sleep/Harmless unit wakes** (`81b6570`, `engine/campaign_runtime.inc`) [RE-FALLBACK: TS/RA1
  Assign_Mission replaces Mission_Sleep/Harmless]: a dormant unit that is moving or has an attack order (player or team
  script) leaves the mission. ALL09's freed POW GIs and Grizzlies never fired. **Shared system** (W4-SQ-05 dormancy).
- **Crushable walls** (`629b1e3`, `engine/world.cpp`, `pres_map.inc`) [RE-FALLBACK: RA1 UnitClass::Can_Enter_Cell -- a
  Crusher on a Crushable overlay gets MOVE_OK and crushes it; data OverlayType Crushable=yes]: Crusher land vehicles path
  over standing GASAND / CAFNC* walls and flatten them (cell opens, leaves the leak mask, overlay no longer drawn).
  ALL09's freed Grizzlies and POWs were walled in for good. Save v23 (`crush_wall_`). **Shared system** (pathing).

Harness (no sim change): `place near X Y` and placement retries that watch the hovered origin (`5a94fa1`); building
clicks fall back to a visible foundation cell, `click ... soft`, probe key `last_fired` (`ad521b0`); `H:TYPE:near`
targets and `sel_near(h,ID)` (`84f71bf`). Gates: `--demo-saveload` re-pinned to `ea91172edde017b6` (save v23 format
only); `--demo-campaign` and `--demo-abilities` re-run unchanged.
## P3 status (mission-proof agent P3, branch `p3-missions`)

Real-order proofs (no `camp_*`/kill/reveal/spawn cheats; the only harness help is choosing which visible unit or
building to click):

| Mission | Before | P3 | Evidence / what blocks |
|---|---|---|---|
| ALL05 | YES (probable) | **YES (proven)** | `m-all05-real` WON (5558 frames, gen `gen/mk_all05.py`): spies disguise, black out the RedTeam base and infiltrate the Battle Lab (map enlarges); Tanya C4s the three red Tesla Power Plants (red reactor -> neutral, coils dark); the re-supplied spies black out YellowTeam, then OrangeTeam twice, while Tanya C4s both wall-ringed silos -> `Victory 1`. Needed the C4 range/approach fix below |
| ALL06 | YES (probable) | **YES (proven)** | `m-all06-real` WON (frame 40380 on the merged tree, gen `gen/mk_all06.py`; the sweep skips Insignificant decorations): the landed AMCV deploys; power, 3 refineries, barracks, 3 war factories; Grizzlies massed to 55 and staged, sweep BadGuy2 (Tesla Coils first, then the nearest building), regroup to 70 and sweep BadGuy1 while the factories top the army up, then hunt the last units. Needed two engine fixes: Insignificant/DontScore objects no longer count for `_Win Occurs` (BadGuy1's flags, guard shack and gate kept it alive), and an orphaned dog "riding" a vanished host is let go |
| ALL07 | PROBABLE | **YES (proven)** | `m-all07-real` WON (~3:20 game time): `_Win Occurred` is DestroyedBuildingsAll(BadGuy1) and BadGuy1's only building before Niihau falls is the Sentry Gun outpost at (92,206); the type-selected Pearl fleet is ordered onto it and the Carrier Hornets kill it. (Event 10 semantics are CONFIRMED RE, map-triggers.md; a full Niihau-base kill was not needed) |

Engine fixes (each a separate commit):

1. `combat: C4 sappers ...`: a C4=yes infantry measured its plant range to the NEAREST foundation
cell (<= 1.5) and approached a free cell next to the foundation cell nearest itself. FUN_006F6750 (combat.md §9)
measures any weapon vs a building to its centre with the range widened by (w+h)*64, and [Sapper]/[Sapper2] have
CellRangefinding=yes (from the sapper's cell). With the old rule ALL05's Tesla Reactors (fenced on three sides) left
Tanya standing still forever and the wall-ringed silos were unreachable. Now the sapper heads for the walkable cell
nearest it from which the plant is in range (cell choice RE-FALLBACK, infantry-anim.md §7).
2. `campaign: Insignificant/DontScore objects do not count ...` [RE: HouseClass RegisterGain FUN_004FF520 /
   RegisterLoss FUN_004FF370 skip types with Insignificant (+0x232, INI read 0x5F8D0B) or DontScore (+0xC9F); the
   events 9/10/11/55/56 read those counters]. Merged with P5's flying-class rule.
3. `abilities: a parasite whose host left the world by another path is let go` [abilities.md §B3]: a dog left
   `cargo_of` a vanished host stayed invisible, unkillable and counted for BadGuy1 forever.

Harness (no sim change): `repeat N`/`endrepeat`, `waituntil`, `unit H:TYPE:near` / `unit e:TYPE:near` / `building H:TYPE:near`,
probe keys `nenemy ndog utgtb ucell bc4n blackout(h) view_resized` (scripts/play/README.md).

Observed, not investigated: in failed ALL05 attempts Tanya died and the mission stayed RUNNING (the original loses via
`Tanya Dies` -> `Mission Failed`); check the tag hand-over when the team re-creates her (`Change Tanya House`).

Gates: no demo gate re-pinned (none run; the C4, counting and parasite changes only act on C4 sappers,
campaign Destroyed*All events and orphaned parasites). Save format unchanged (v22).

## P2b status (branch `p2b-missions`)

- **R-205 fixed / re-diagnosed.** The capture test in `poll_mind_control` now uses the same 3-D fire-range rule as
  combat (`unit_in_weapon_range`, FUN_006F6750): in `gamemd.exe` the capture is done by the controller's shot --
  CaptureManager `FUN_00471C80` has a single caller, the bullet detonation @0x4692C0. The reported "Yuri does nothing at
  8.5 cells" was not the range split: the script had re-clicked the already-selected Yuri, which DEPLOYS him (deploy
  cursor), and a deployed DeployFire unit holds its post. Play test `scripts/play/mc-range.play` (PASS).
- **SOV08 -- not won (tactics).** Best attempt generator: `scripts/play/gen/mk_sov08_attempt.py` (~16 iterations).
  What works: MCV to (59,97) before deploying; two Sentry Guns before the refinery stop Vlad1's paradrops; the base
  then survives the whole game (Tesla Coils, barracks rebuilds). What fails: production tops out at ~16 Apocalypse
  tanks by campaign ~53000; the push reaches the White House gate and takes it to ~800/1000 before dying to the four
  Tesla Coils, two sentry guns and Vlad2's infantry (5 Shock Troopers alone killed a 20-tank army with no
  anti-infantry). A V3 siege of Vlad2's west power plants failed (rockets die in flight near the flak; V3s ordered past
  their range wander into the base). Engineers walking behind the Apocalypse ball died before the gate. No engine
  blocker identified.
- **SOV09 -- retried, still blocked (tactics).** The R-205 fix does not change the picture. Best: north plant by the
  MC'd SEAL's C4 (1000 -> 800), then the IFV by a southern route damages the (68,66) plant from (67,73) (-> **742**)
  before a base defence kills it. The Prisms need < 575; the remaining plants are only reachable inside the Prism
  compound, and the roaming SEALs kill every infantry escort. No engine blocker identified.

## P1 status (mission-proof agent P1, branch `p1-missions`)

Real-order proofs (no `camp_*`/kill/reveal/spawn cheats; harness help is limited to choosing which visible unit,
building or cell to click):

| Mission | Before | P1 | Evidence / what blocks |
|---|---|---|---|
| ALL04 | YES (probable) | **YES (proven)** | `m-all04-real` WON (f~10870, gen `gen/mk_all04.py`, Normal): beach the MCV craft, unload (Deploy), drive the MCV off the quay and deploy; power, refinery, barracks, war factory; 12 Grizzlies (incl. PlayerAlt2's handed-over ones) staged mid-map, then type-selected Grizzlies/IFVs destroy the Psychic Amplifier -> `AmplifierDestroyed` -> Win |
| ALL11 | PROBABLE | **YES (proven)** | `m-all11-real` WON (f~10070, gen `gen/mk_all11.py`, Normal): war factory, Battle Lab, Chronosphere ('Chronosphere is Built' reveals Cuba and hands the silos to BadGuy2), Prism towers; 12 IFVs + 12 Navy SEALs, each SEAL boards an empty IFV; the loaded IFVs gather and are chronoshifted (source = the 3x3 holding most of the group) to (190,195) -- a building-free 3x3 between silos B and C; Deploy unloads, the SEALs C4 all three silos -> `Win (Nukes KIA)`. Needed the Mission=Harvest fix (no income at all before) |
| ALL12 | PROBABLE | PROBABLE (blocked) | `m-all12-real` still proves the opening only. WIP generator `gen/mk_all12.py` (Easy): Chronosphere + Weather Control built; a Chronoshift dumps the eastern Black Guard Apocalypse pair into the lake (MammothDead1/2), a Lightning Storm on (118,122) kills the western pair and BadGuy3's Tesla coil. Blockers: the last four BadGuy3 buildings (2 flak, 2 sentry guns): no SEALs on ALL12's sidebar, the player's units do not survive at home long enough to mass Prism tanks for a yard Chronoshift, and a second storm at (127,121) is followed by `Kremlin is Destroyed` (Kremlin 1000 -> 838 -> lost; IonWH is 2% vs concrete, so this needs a look -- see below). On Normal the base dies at ~6.5 min to a Kirov bomb run (`Kirov Medium` AITrigger, faithful) before the Battle Lab completes |

Engine fixes (separate commits):

1. `fix(campaign): map-placed miners with Mission=Harvest start harvesting` (`engine/campaign_runtime.inc`
   `place_unit`) [RE: campaign.md §4 -- campaign objects get their INI mission (vtbl+0x1E8); economy.md §1 --
   Mission_Harvest runs the harvester FSM FUN_0073C560]. Every map-placed HARV/CMIN with Mission=Harvest (ALL11's three
   player Chrono Miners, the AI miners of 14 maps) stood idle; ALL11 had no income at all. Shared: also strengthens AI
   economies (ALL04 Russian1, ALL12 BadGuy1 -- the earlier Kirov in ALL12 Normal follows from that, faithful).

Harness (no sim change): `place auto` only offers origins whose click resolves back to the intended cell (a 4x4
Chronosphere was dropped onto the war factory on raised ground); `point sel_pack` / `sel_pack_n` (Chronoshift source =
the 3x3 holding most selected units); `unit H:TYPE~:n` (n-th transport with a free seat); probes `free(h,ID)` (units not
aboard anything) and `sw_last_nuke_house/x/y`. Note: superweapon buildings (Chronosphere, Weather Control) are built
through the DEFENSE queue (`queue_ready(defense)`), not `structure`.

Open questions: (a) ALL12 Kremlin lost ~700 frames after a Lightning Storm centred 2 cells from it (IonWH verses give
2% vs concrete; check the building armor/insignificant path in `World::an_area_damage`, or another cause); (b) the
AI nuke picker scores every enemy house's objects (RE-FALLBACK) where FUN_0050CA10 walks only house+0x5600's objects --
harmless in these missions (Civilian houses share BadGuy's alliance team), not changed.

Gates: `--demo-campaign` re-run, unchanged (`dd71d9faceb255e1`). Save format unchanged (v22).

## How this was exercised

| Tool | What it does | Where (all under `D:\ra2-wt\w4-seq\scratch\`) |
|---|---|---|
| `timeline.py` | Decodes every map's `[Triggers]/[Events]/[Actions]`. It resolves team names, trigger names, waypoints and `TextTrigger` labels against `D:\Red Alert 2\ra2.csf` (the objective text), and lists every TeamType with its house, waypoint, `UseTransportOrigin`/`Droppod`/`Reinforce` flags, TaskForce and script. This gives the **intended** sequence of each mission. | `tl\<map>.txt` (23 files) |
| `play\sq-<map>.play` | The real campaign `GameScene` (`set campaign <MAP> <SIDE>`, then the loading screen's Start). It runs with **no input** for 300 driver-seconds (about 400 campaign seconds, `campaign_frame` 6009). It takes shots at 0/10/30/60/120/180/300 s, probe dumps at 60/180/300 s, and `cheat camp_log` (the full trigger/team witness). | `out\sq-<map>\t*.png`, `play.log`, `stdout.txt` |
| `play\sq-zsov04long.play` | SOV04 with no input for 20 campaign minutes, to watch the Allied fleet landing. | `out\sq-zsov04long\` |
| `play\sq-zsov09.play` | SOV09 with the witness taken at 80 s, before the early loss. | `out\sq-zsov09\` |
| `an.py` | Turns each run into per-house units/buildings/losses at every dump, plus the first firing of each trigger and every team placement or recruit result. | — |
| `notes.txt` | Per-mission observations as written down during the runs. | — |
| Ghidra, private copy `D:\ra2-re\w4-seq` | Decompiles of team action 46/47 and its building finder; the TeamType writer (flag offsets); `Reinforcement` and its object-build and placement callees (RE-C1). | `D:\ra2-re\w4-seq-out\dec46.txt`, `dec46b.txt`, `decrf.txt`, `dectt.txt` |

**Limits.**
- Every `--play` run is silent (audio off). Speech, sounds and music cannot be heard here. W2-CA-12 routed them, and the
  witness shows that every `PlaySpeech`/`PlaySoundEffect` is issued at the right frame.
- `MOVIES01/02.MIX` are stubs, so `PlayIngameMovie` (100/117) and `PlayMovie` (10) cannot play. This is not counted as a
  gap.
- "No input" is the right probe for "what does the mission do on its own". It does not prove real-play winnability. The
  winnability column combines this run, wave-2's cheat and `--play` proofs (`docs/gap/wave2/01-campaign.md` §5), and
  the blockers found here.

## 1. Per-mission table

Times are campaign seconds (ElapsedTime ×15 frames). "Intended" is read from the map's triggers and teams. Screenshot
paths are relative to `D:\ra2-wt\w4-seq\scratch\out\`.

| Mission | Intended opening (map data) | Clone opening (no-input run) | Broken beats | Attack waves happen? | Real-play winnable? | Screenshots |
|---|---|---|---|---|---|---|
| **ALL02** Colorado | 0 s: Russian base production (no AI teams yet). Confederate road patrols, 4 British Harriers fly over and are removed at 5 s. 30 s: "Rocketeer movie". 35 s: 8 Rocketeers fly in and turn American (Alliance PC), then move. 60 s: "Objective 1: use ENGINEERS to re-capture our base", repeated every 75 s until the base is seen. 90 s: Tanya Flak hint. Russian AI triggers start **only** when the player builds or hits the harvester. | Patrols, the Rocketeer sequence and the objective/hint text all fire on time. The 4 British Harriers (house 4, enemy) are **never removed**: `DestroyTeam` → `skipped`, and they are still there at 13 min. Russians build 10→13 buildings and never attack (correct). Confederates lose 4 E2 to the player's Tanya auto-fire. | 4 enemy Harriers loiter (W4-SQ-08). End-game paradrop teams `10 Paratroopers BE/BI/BJ` are Droppod but would appear on the ground (W4-SQ-06). | Correctly none without player action. Patrols present. | **YES** (wave-2 `m-all02`: engineer capture → WON) | `sq-all02s\t000.png`, `t060.png` |
| **ALL03** Washington | 0 s: input locked, reveal. 3-15 s: 4 Russian **garrison teams** (7 Conscripts each) occupy civilian buildings. 5-28 s: IFV tutorial (3 IFVs flash, an engineer boards an IFV to repair the damaged one; unlocks at 28 s). 34 s: "Destroy the Psychic Beacon". 120/220/300 s: Civilian-Army hordes. 250 s: Shock-trooper intro. 270/470 s: Russian2/Russian1 AI production. 700/950 s: player reinforcements. | Hordes arrive at 120/220/300 s (12 each) and hit the player: the player loses 3 units by 240 s and 6 by 400 s. Shock intro and Russian2 production happen on time. **Garrison teams are placed as 7 loose E2 that never enter a building**: `garrison_shots=0`, and only the civilian house owns garrisonables. In the IFV tutorial, `Load` is skipped, so the engineer never boards. | Garrisons absent (W4-SQ-02). IFV/engineer tutorial (W4-SQ-01). Input never locked (W4-SQ-09). | YES (hordes, shock squad). | **PROBABLE** (wave-2 probe WON; garrisons missing make it easier) | `sq-all03u\t000.png`, `t030.png`, `t060.png`, `t180.png` |
| **ALL04** Chicago | 0 s: 6 Destroyers created. 1 s: 4 `StartingForce` teams = landing craft **carrying** the AMCV and 3×(Grizzly + 5 GI), sailing to the shore (`Full=yes`). 3 s: Rocketeers. 12-30 s: 9 Russian garrison teams (10 Conscripts each) occupy civilian buildings. Hint: "AIR UNITS are effective against Soviet-occupied civilian structures". 22 s: "Clear shore defenses, land and establish a base". | The LCRFs are spawned **empty** at sea. The AMCV is put on the nearest land cell, and the PlayerAlt2 tanks/GIs on the shore at f0. **85 Conscripts stand in the open streets** instead of in buildings. The start camera looks at empty lake, with the fleet at the screen corner (t030/t180 identical). Russian patrols come at 200 s. | Landing (W4-SQ-01). Garrisons (W4-SQ-02), which also break the "air vs garrisons" design and `TankAttack*` teams (attack-specific-building). | Patrols only in 5 min (correct; AI is gated on the player's ConYard). | **PROBABLE** (wave-2 `m-all04` PASS) | `sq-all04u\t000.png`, `t030.png`, `t180.png` |
| **ALL05** Dark Night | 0 s: camera, flare, Tanya changes house. 7 s: a Night Hawk **carrying 3 Spies** lands at wp12 and unloads. Spies change to the PC "Spies" house; heli leaves and is deleted. "Battle control online". 73 s: "Infiltrate the Battle Lab with a Spy" (secondary). Silos objective after that. Re-supply of spies only **if all spies die**. | Spies are placed **on the ground at wp13** beside an empty heli. `Change Spies House` recruits **0** (the spies are still members of the heli team). The heli script's `DeleteMembers` then **deletes the spies** → `Spies Dead 1` → new heli → loop. The Reinforcements house loses 12 by 80 s, 32 by 240 s and 49 by 400 s. That is an EVA/Tanya "need spies" line and "Additional Spies have arrived" every ~35 s, and **the player never owns a spy**. Tanya works. | Spy delivery loop (W4-SQ-01). 15 `Sleep` objects awake (W4-SQ-05). | Red patrols present. No AI attack (correct). | **PROBABLE** (only via Tanya's C4 on the two silos; the spy objective is impossible) | `sq-all05s\t000.png`, `t030.png`, `t180.png` |
| **ALL06** Pentagon | 0 s: 3 landing-craft teams bring the AMCV, 20 GI and 3 engineers. 7 s: Pentagon reveal. 15 s: "Prism fodder" demo (Soviet units walk into the Prism towers) + Einstein movie. Objective 1 "reinforce and take command" = reach the Pentagon. BG1/BG2 production at 10/15 min or on discovery. | Landing forces placed at the beach next to empty LCRFs. Prism demo works: BG1/BG2 lose 14/20 units to the towers. `Player Takes Command` fires at 183 s: the start teams' scripts walk into the Pentagon cell. BG1/BG2 production and map AI start at 227 s (17/18 nodes). BG1 Garrisoners recruit 7 E2 per team but never garrison. | Garrisons (W4-SQ-02). Landing visuals (W4-SQ-01). | YES (street patrols, flank attack; player loses 1 building by 240 s). | **YES** (probable) | `sq-all06u\t000.png`, `t060.png` |
| **ALL07** Hawaii | 0 s: camera tour (Aegis, Carrier flashes, tab flash), input locked 48 s. 10-40 s (by difficulty): Niihau "softener" landings and 3 Pearl attack fleets. Niihau defenders fight back (an AI-vs-AI battle). BG1 builds after Niihau falls. | Tour, Niihau and Pearl attack sequences all fire on time. 18 Soviet reinforcements are placed at sea. The player's fleet at Pearl (2 Aegis, 3 Destroyers, 2 Carriers) is destroyed by 400 s with no input. Niihau defenders recruit and fight. `RevealZone` ×3 unimplemented. | `a18` RevealZone (W4-SQ-11). Pearl/Niihau `PAttk_Infantry` paradrops would appear on the ground (W4-SQ-06). | YES, and they reach the player. | **PROBABLE** | `sq-all07t\t000.png`, `t180.png` |
| **ALL08** St Louis | 0 s: **paradrop** of Tanya, 5 GI and 2 engineers (`Droppod`, `UseTransportOrigin`), input locked. 12 s: beacon reveal and camera tour. 25 s: battle control. 27 s: units split into 3 PC houses, 20-min timer. BG2 production at 52 s, BG1 at 150 s. Reinforcement landing craft (`Player Reinforcements 1-5`) unload and change house to the player. | The start force **appears on the ground** at f0 (no plane). The 3-house split recruits 5/1/2 (W2-CA-09 fix holds). BG2 and BG1 production start on time (13 and 28 nodes). With no input, both engineers die by 107 s (`Engineers KIA`) to patrols. | Paradrop (W4-SQ-06). Later LCRF reinforcements carry cargo beside them (W4-SQ-01; their `ChangeHouse:13` to the player is harmless). | YES (patrols kill the engineers). | **YES** (wave-2 `m-all08` PASS) | `sq-all08u\t000.png`, `t060.png` |
| **ALL09** Yucatan | 0 s: **SEAL insertion** by plane (`Droppod`, `UseTransportOrigin`, `TransportWaypoint=J`, **VeteranLevel=3** elite), input locked. The SEALs land on the LZ cell tag, which unlocks input and states the objectives. Village/base patrols. Attacking BG1 wakes 8 response teams. | SEALs (veteran, not elite) **appear on the ground at wp10 at f0**. The LZ cell tag `Seals have Landed` only fires at **119 s**, so objectives come 2 min late. With no input, the SEALs auto-fire, `BG1 Attk Response 1` sends 8 teams, and the mission is **LOST at ~285 s**. | Paradrop/LZ beat (W4-SQ-06). VeteranLevel (W4-SQ-04). Input lock (W4-SQ-09). | YES (response teams reach the player). | **PROBABLE** (wave 2: SEALs orderable; hard with non-elite SEALs) | `sq-all09t\t000.png`, `t060.png` |
| **ALL10** Mirage | 0 s: input locked, Einstein's Chronosphere powered **off**. 5 s: Mirage tanks delivered + movie, camera to Einstein. 30 s: control. 150-170 s: Soviet invasion of 3 German villages (2 waves × 3 houses). Villages explode when attacked (scripted). Soviet MCVs land and build. Einstein assault squads at 10/15/20 min. | Intro and camera fire on time. Invasion at 169/189 s; German villages attacked, `German Explode` destroys them, all 3 German houses wiped by about 5.5 min. MCV teams placed, `Invaders Reform` recruits. `TurnOff` (a61) unimplemented, so the Chronosphere stays powered. `FireSale` ×3 `skipped`. Player loses 1 unit. | a61 (W4-SQ-11). FireSale skipped. | YES (AI-vs-AI set piece plays). | **YES** (probable) | `sq-all10s\t000.png`, `t180.png`, `t300.png` |
| **ALL11** island | 0 s: input locked 62 s. 5 s: first Giant Squid attack on a civilian boat (+ movie). 62 s: BG2 patrols (squid, subs), persistent teams. 150-360 s: BG2 paradrops and seaborne invasions (`SAPC` + cargo: Unload, return, DeleteMembers). 16-31 min: Cuba nukes, Chronosphere objective. | Squid/civ boat at 5 s; patrols and persistent teams at 62 s. **ParaTeams placed on the ground** (150 s, 360 s). Invaders 240 s+ placed as SAPC + cargo. They land because the `X Landed` CreateTeams recruit the cargo before `DeleteMembers`. Player loses 4 units by 400 s. | Paradrops (W4-SQ-06). Invader cargo (W4-SQ-01). `PreferredTarget` (a35) unimplemented (W4-SQ-11). | YES. | **PROBABLE** | `sq-all11t\t000.png`, `t180.png` |
| **ALL12** Moscow | 0 s: **ParaSEALS** dropped at wp3. 8 s: "Clear the area so Chrono reinforcements can arrive". Prisoners (Sleep) wait to be freed for "Additional troops gained". Chrono MCV and 3 later Chrono waves after the generators fall. | 3 SEALs placed on the ground at f0. **8 Sleep prisoners and 8 guard Conscripts kill each other in the first 60 s**, so the "Additional troops gained" bonus is lost. Nothing else moves for 400 s (correct: gated on the generators). | Sleep not honoured (W4-SQ-05). Paradrop (W4-SQ-06). | Correctly none before the player acts. | **PROBABLE** | `sq-all12s\t000.png`, `t060.png` |
| **SOV01** Pentagon | 0 s: input locked. `Break Bridge` (8× Apply100Damage) destroys the bridge so Sofia can ask for an engineer repair. 10 s: 3 paratrooper drops (21 Conscripts). 38 s: Soviet MCV + "Crush the Pentagon". Iwo Jima text, GI introduction flash, AI production at 0 s. | Paratroopers appear on the ground at 10 s. **The bridge is never broken** (`a63 … skipped` ×8), so the "repair the bridge" Sofia beat and its arrows are moot. MCV, objective, GI flashes and patrols fire. Player loses 4 of 18 Conscripts by 400 s. | Bridge (W4-SQ-10). Paradrop (W4-SQ-06). | YES (patrols, `Angry GIs` on Pentagon damage). | **YES** | `sq-sov01t\t000.png`, `t060.png` |
| **SOV02** Florida | 0 s: input locked. 1 s: 7 beach-landing teams (`SAPC` **carrying** HTNK/Conscripts). Each reaches the beach, unloads, sets local 2 (unlock) and `ChangeHouse:5` hands the **empty transport** to the transport house. Vlad's dreadnought shells the beach defences, and their destruction lands the MCV. 20 s: "Force a beach landing and build a base". | Transports and cargo are placed side by side. Cargo are team members, so **`ChangeHouse:5` converts the player's landed tanks and Conscripts to the AI transport house**: player units 9 at 60 s → **0 at 180 s**, house 6 gains 7 E2, 4 HTNK and 2 transports. No MCV by 400 s (`Land the MCV` never fires). The player is left with a radar and nothing to command. | Landing hand-off (W4-SQ-01), **BLOCKER**. | American attack (`USA 1st Attack` 31 s) reaches the beach. | **NO** (regression of the wave-2 "YES probable", which used cheats) | `sq-sov02t\t030.png`, `t180.png` |
| **SOV03** Battle Lab | 0 s: BG1 production, outposts, IFV patrols, garrison teams. 1 s: radar movie. 41 s: "Capture the Allied BATTLE LAB with an ENGINEER". 200/360/500/600 s: harassment teams. After the beacon: Psi-Corps, then Allied conversion and the final rush. | All placed on time. `Garrison Teams` recruit 1 each but do not garrison. `Opening Harass 1` (200 s) recruits **0** (no stock yet); Harass 2 (360 s) recruits 12 from AI production. Player unharmed by 400 s. | Garrisons (W4-SQ-02). Harass team waits for stock (W4-SQ-13). | Weak in the first 6 min. | **YES** (wave-2 `m-sov03` PASS) | `sq-sov03u\t000.png`, `t180.png` |
| **SOV04** Korea | 20 s: 10-min timer "Establish a base". 250/400 s: Intruder harassment. 620 s: Allied feint landings, then the fleet (LCRF + cargo incl. **Allied MCV**). Landed cargo regroups and the MCV builds a base. "Drive the invaders back". | Timer, harassment, feints (10:20) and fleet (13:40) all arrive. The `GIs/Grizz/MCV Landed` CreateTeams recruit the cargo, the AMCV deploys and BG1 production starts at 14:52. With no input the player is wiped (only a sub left at 20 min). | Cargo not inside the LCRFs (W4-SQ-01, cosmetic here). Invasion paras on the ground (W4-SQ-06). | YES. | **YES** (probable) | `sq-sov04s\t000.png`; `sq-zsov04long\l700.png`, `l800.png`, `l900.png` |
| **SOV05** Paris | 0 s: GI patrols, radar movie. 3 s: "Energize the PARIS TOWER with 3 TESLA TROOPERS". Discovering transports/Flak Tracks gives free units. | Patrols placed; civ cars drive. Player loses 10 units by 400 s with no input. `Transport Discovered` gives SAPC + 6 Shock troopers (cargo `ChangeHouse:13` to the player, harmless). | Re_ reinforcements are Droppod (W4-SQ-06). | YES (patrols). | **YES** (wave-2 `m-sov05` PASS) | `sq-sov05u\t000.png`, `t180.png` |
| **SOV06** Hawaii | 0-6 s: 3 landing waves; camera at 12 s; control at 35 s. Base defence teams. 90/190/350 s: Allied scouts and harassment. ConYard starts the Asian fleet timer. | Landing waves placed at wp18 (22 units). Scouts/harassment teams created at 90/190/340 s, but **no contact by 400 s** (player lostU 0). | Input lock (W4-SQ-09). | Teams form; none reached the undeployed player in 400 s. | **YES** (wave-2 `m-sov06` PASS) | `sq-sov06t\t000.png`, `t300.png` |
| **SOV07** Chronosphere | 10 s: "Defend your Battle Lab". Waves by timer from 150 s (Chrono-teleported tanks and IFVs, rocketeers, Harriers, snipers, spies). Propaganda truck. | Waves at 150/250/320/400 s placed via `ReinforcementAt`/`ByChrono`; spies and propaganda truck. Player loses 1 unit by 400 s. | 12 Droppod wave teams appear on the ground (W4-SQ-06). | YES. | **PROBABLE** | `sq-sov07s\t000.png`, `t300.png` |
| **SOV08** Washington | 1-2 s: patrols and 3 garrison teams (8 Conscripts each). 5 s: "Capture the White House". 60/120/210 s: Vlad paratroopers. 250/300 s: AI production. | Patrols placed. Garrison teams stand in the open. **Paratroopers (15) appear on the ground at 60 s**, next to the player. Vlad1 loses 40 units and the player 8 by 400 s. | Garrisons (W4-SQ-02). Paradrop (W4-SQ-06). | YES. | **YES** (probable) | `sq-sov08u\t000.png`, `t060.png` |
| **SOV09** Mexico | 0 s: input locked 65 s; Russian drop groups (Conscripts, Shock, **paradropped**) are the player's army. 30 s: **2 Yuris paradropped**. 35 s: President and guards appear. 100 s: `Yuri's are Dead` armed (lose if every Yuri dies). 105 s: "Use Mind-Control to capture the President". | Drop groups and Yuris **appear on the ground** at the drop points (the Yuris at 30 s). The Yuris mind-control a civilian car (f525, f720) and **both are dead by 80 s**. At 100 s the loss trigger arms, and the mission is **LOST at ~107 s** with no input. | Paradrop (W4-SQ-06). Input lock (W4-SQ-09). `Switch Power House` ChangeHouse `changed=0` (W4-SQ-11). | YES (instantly lethal). | **DOUBTFUL** (wave-2 `m-sov09` PASS used a scripted capture; real play must save the Yuris inside ~50 s) | `sq-sov09u\t000.png`; `sq-zsov09\t030.png`, `t080.png` |
| **SOV10** Weather Device | 0 s: 2 transport teams (SAPC + cargo + SMCV) cross to the LZ. 7-57 s: camera tour and Zofia lines. LZ cell tag `Landed`, then "Capture the BATTLE LAB". 200 s+: landing-craft harassment. | Transports and cargo placed side by side at the start waypoints. Zofia tour and speech fire on time. `Landed` only at **120 s**, so the objective is late. Harassment at 200/300/400 s placed. Player loses 0 by 400 s. | Cargo / LZ timing (W4-SQ-01). `HouseDiscovered` (event 5) never raised, so `Bad Guy 3 Startup` is dead (the alternative `Zofia 5` path exists) (W4-SQ-11). | YES (harassment). | **YES** (probable) | `sq-sov10t\t000.png`, `t120.png` |
| **SOV11** Moscow | 0 s: input locked until the ConYard exists; BG1/BG3 production. 3/9 s: player Kirovs and MCV. 180 s: Iron Curtain movie, Yuri's base ping. 280 s: BG2. Kirov counter-attacks when the player builds a nuke. | All on time. Player loses 7 and BG1 6 by 400 s. | Input lock (W4-SQ-09). | YES. | **YES** (probable) | `sq-sov11s\t000.png`, `t300.png` |
| **SOV12** Allied island | 3 s: "Build naval forces and invade". BG1 at 240 s, BG2/BG3 staggered. 300 s+: Chrono harassment raids on the player's island. | Text at 3 s. BG1/2/3 production at 245/275/335 s. Chrono IFV raid at 305 s. With no input the player (no units, 4 buildings) loses 2 buildings by 400 s. | — | YES (chrono raids reach the island). | **PROBABLE** | `sq-sov12s\t000.png`, `t300.png` |

**Summary.**
- In the first 5 minutes, **every mission's opening triggers fire on time**: objectives, hints, camera moves, speech
  requests, timers and AI production starts. Enemy waves and AI-vs-AI set pieces are placed and fight: ALL03 hordes,
  ALL06 Prism demo, ALL07 Pearl/Niihau, ALL10 German villages, SOV04 invasion, SOV07 waves.
- What is wrong is **how scripted forces are delivered and what the scripts do to them afterwards**:
  - transports that arrive empty, with the cargo standing beside them as team members;
  - paradrops that spawn on the ground;
  - garrison teams that never garrison;
  - sleeping prisoners that fight at f0.
- One of these is a hard blocker: **SOV02**, where the player loses the entire landing force to `ChangeHouse`. Two make a
  mission a near-loss: **SOV09** (both Yuris dead by 80 s, loss at 107 s) and **ALL05** (the spy loop).
- Real-play verdict: **12 YES / 9 PROBABLE / 1 DOUBTFUL (SOV09) / 1 NO (SOV02)**. Wave-2's "after" table had SOV02 YES
  and SOV09 YES; both relied on cheats.

## 2. Gap table

| ID | System | Issue | Original (map data / RE) | Clone (file:line / evidence) | Sev | Missions | Fix sketch |
|---|---|---|---|---|---|---|---|
| **W4-SQ-01** | Reinforcement delivery | **Transport cargo is never loaded.** For a TaskForce with a transport (`Passengers>0`: SAPC, LCRF, SHAD, FV) plus passengers, every member is placed on its own cell and stays a team member. `Load` (14) and `WaitLoaded` (43) are skipped, so `Unload` (8) has nothing to unload. The map scripts' follow-ups then act on the passengers: <br>- `DeleteMembers` (37) deletes them;<br>- `ChangeHouse` (20) converts them.<br>40 transport teams in 9 maps use `MoveWP, Unload, …, DeleteMembers/ChangeHouse`. | [RE w4 `decrf.txt`] `FUN_0065d770` (TeamType → objects, called by Reinforcement `FUN_0065d320`) builds the transports and the others as two lists. When TeamType `+0xA5` (= `Full`, offset from the writer `FUN_006f0ab0`, `dectt.txt`) is set, it puts the non-transport members **into** the transport (`FUN_004732d0` = add passenger). `Unload` then ejects them, and the unloaded passengers are no longer part of the transport team. | `campaign_runtime.inc:1333-1380` (`create_team`, one `place_unit` per member); `campaign.cpp:447` default `Skip` (14/43); `campaign_runtime.inc:1572` `Unload` → `order_unload` on empty transports; `:1583-1586` `DeleteMembers` erases every member; `:1558-1564` `ChangeHouse` converts every member. Evidence: SOV02 player 9 → 0 units; ALL05 `recruited=0` ×3 and the spy loop (lostU 12/32/49); ALL04/06/SOV06/SOV10 empty landing craft. | **BLOCKER** (SOV02); MAJOR (ALL05); visual (ALL04/06/08/11, SOV04/06/10) | SOV02, ALL05, ALL04, ALL06, ALL08, ALL09, ALL11, ALL12, SOV01, SOV04, SOV05, SOV10 | In `create_team(reinforce)`: when TeamType `Full=yes` and the TaskForce has a transport, create the passengers **inside** it with the existing World cargo API (`order_enter_transport`, the naval.play LCRF path). On `Unload` (8, arg = mode), eject, and remove the ejected units from the team (team = -1, recruitable). Implement `Load` (14) / `WaitLoaded` (43) the same way. |
| **W4-SQ-02** | Team scripts | **Script 46/47 (attack / move to enemy building) ignore the building-type argument and selector.** They pick the nearest *enemy* building of any type. Map "garrison" teams target a specific **civilian** building type, so in the clone they never garrison, and other targeted teams hit the wrong building. | [RE w4 `dec46b.txt`] `FUN_006ee130` (called by 46 `FUN_006ed870`, 47 `FUN_006ed950`): scans all buildings whose type `+0x520` equals the arg's low 16 bits and whose owner is non-allied **or a passive house** (`Owner→Type+0x1A6`, the neutral flag of `world-depth.md` A2). It ranks them by selector (arg >> 16): 0/1 = threat (inverse/direct), 2 = nearest, 3 = farthest. An Occupier infantry "attacking" an occupiable neutral building enters it (`CanBeOccupiedBy` `FUN_00457CE0`). Data: ALL03 hint "Beware of SOVIET TROOPS garrisoned in CIVILIAN STRUCTURES"; ALL04 hint "AIR UNITS are very effective for assaults on Soviet-occupied civilian structures". | `campaign_runtime.inc:1514-1541` (`nearest_enemy(…, 2, …)`, `c.arg` unused for 46); `:1492-1497` (47). Evidence: ALL03 `garrison_shots=0`, `owned_garrisons(4)=36` (civilian only) at 300 s; ALL04 85 E2 in the streets. | MAJOR (mission feel/difficulty; ALL04's design is built on it) | ALL03 (12 teams), ALL04 (12), ALL06 (4), SOV03 (2), SOV08 (3); targeted attacks in ALL02/04/09, SOV02 | Decode `arg & 0xFFFF` as the map BuildingTypes index (RA2 rules index space, campaign.md §6) and `arg >> 16` as the selector. Search owner = enemy or passive house. If the member is an Occupier and the target is occupiable and neutral, use the World garrison order (`order_occupy`, G16) instead of attack. |
| **W4-SQ-03** | Team scripts | `DestroyTeam` (5) is `skipped` once the team's script has ended. Intro flyovers made of aircraft never leave: their script ends with no exit and they stay on the map. | ModEnc "Destroy Team"; flyover data: ALL02 `4 Intruders - Start` (British, enemy) + `Destroy Airplane Team` 5 s later. Exact original semantics: RE-W4-SQ-3. | `campaign_runtime.inc:1776-1786` (only non-done teams). Evidence: ALL02 `FALC4` alive at 13 min; ALL10/SOV06 Harrier teams the same. | MINOR | ALL02, ALL10, SOV06 | After RE: aircraft whose team finished or was destroyed fly off the map edge (or to an airfield) and are removed. |
| **W4-SQ-04** | Team scripts | TeamType `VeteranLevel=` is parsed and never applied. | Writer `FUN_006f0ab0` `+0xA0` VeteranLevel. Created members get the rank (`FUN_0065d770`: `FUN_0074ee80/eeb0/eed0` by level). | `campaign.cpp:357` read only; no use in `campaign_runtime.inc`. | MINOR (ALL09 elite SEALs are much weaker) | ALL09, others | Apply `ra2engine` veterancy (P5) to each created member: 0/1 rookie, 2 veteran, 3 elite. |
| **W4-SQ-05** | Trigger setup (objects) | Placed objects' `Mission=Sleep/Harmless/Area Guard` are ignored (W2-CA-18, still OPEN). Sleeping prisoners and their guards start shooting each other at f0. | campaign.md §4 field order. "Prisoners" triggers expect sleeping units to survive until freed. | `campaign_runtime.inc` `place_unit` (`:940-1010`, only Hunt honoured). Evidence: ALL12 houses 2 and 5 lose 8 + 8 units in the first 60 s; the "Additional troops gained" bonus is lost. | MAJOR (ALL12, ALL05 15 sleepers); MINOR elsewhere | ALL02, ALL05, ALL09 (Harmless ×6), ALL12, SOV10 | `World::set_passive(u)` = no auto-target and no retaliation until action 81/82/83 (Wake*) or damage, as for Sleep; Harmless never auto-targets. |
| **W4-SQ-06** | Reinforcement delivery | **`Droppod=yes` teams (paradrops) spawn on the ground at the waypoint at once**: no transport plane, no fall, no delay. 70 Droppod TeamTypes in 18 maps. Beats that depend on landing then shift. | [RE w4 `dec46.txt`] `FUN_0065d320` with TeamType `+0xB0` (`Droppod`): creates a transport aircraft for the house (`DAT_00a8a184[...]->CreateObject(house)`). It enters at the house's edge (`house+0x1E0`, `FUN_004aa350`), or at the TransportWaypoint when `+0xB1` `UseTransportOrigin` is set (`FUN_006f0e40`). It loads the members (`FUN_004732d0`) and flies Mission `0x1A` (paradrop) to the team waypoint. | `campaign_runtime.inc:1343-1346` (`[RE-FALLBACK §9.3] TaskForce members appear at the team waypoint`). Evidence: ALL08 start force at f0; ALL09 SEALs on the ground with the LZ tag only at 119 s; SOV09 Yuris dead by 80 s → LOST 107 s; SOV08 15 paratroopers dropped beside the player. | MAJOR (SOV09 → DOUBTFUL; ALL09 objective delay) | ALL02, ALL03, ALL07, ALL08, ALL09, ALL11, ALL12, SOV01, SOV02, SOV03, SOV04, SOV05, SOV07, SOV08, SOV09, SOV12 | Reuse the W2-AN `AmerParaDrop` superweapon plane path (`sw.play`: plane from the edge, parachute drop). Spawn the plane at the house edge / TransportWaypoint, drop the members over the waypoint, then hand the members to the team and start its script. |
| **W4-SQ-07** | Reinforcement delivery | Landing/arrival cell tags fire late: scripted "landed" beats wait for a unit to wander onto the LZ cell, because units are placed around the waypoint instead of being unloaded onto it. | Data: ALL09 `Seals have Landed`, SOV10 `Landed` (EnteredBy on LZ cells). | Evidence: ALL09 tag at 119 s, SOV10 at 120 s (the original unloads onto the LZ seconds after arrival). | MINOR | ALL09, SOV10, SOV02 | Falls out of W4-SQ-01/06 (units disembark on the LZ). |
| **W4-SQ-08** | Team scripts | Scripted flyovers and patrols that should leave stay: aircraft with no remaining orders loiter. Covered with W4-SQ-03. | — | ALL02 4 enemy Harriers over the start area for the whole mission. | MINOR | ALL02 | See 03. |
| **W4-SQ-09** | Trigger actions | **`LockInput` (46) / `UnlockInput` (47) are recorded only.** The player has full control during every intro, while `MoveCamera` / `CenterCamera` pull the view away. The player can also disturb set pieces that the original protects, such as SOV02's landing before the hand-off and ALL08's units before `Each to His Own House`. | `map-triggers.md` §5 (46/47); intro locks in the data: ALL03 28 s, ALL04 22 s, ALL05 ~13 s, ALL07 48 s, ALL08 25 s, ALL09 until landed, ALL10 30 s, ALL11 62 s, SOV01 until the bridge hut, SOV02 until LocalSet 2, SOV06 35 s, SOV09 65 s, SOV10 10 s, SOV11 until the ConYard. | `map_trigger.cpp:118-127` (`presentation_action` 46/47); no consumer (`grep -rn LockInput engine/` → `map_trigger.cpp` only). | MAJOR (feel; protects scripted openings) | 14 missions | `CampaignRuntime::input_locked()` set/cleared by 46/47. GameScene ignores selection, orders and sidebar while it is set; the camera is still scripted. |
| **W4-SQ-10** | Trigger actions | SOV01 `Break Bridge` (8× `Apply100Damage` on the bridge waypoints) does nothing, so the bridge stays up. Sofia's "repair the bridge" line, the hut arrow and the engineer hint play for a bridge that is not broken. | effects.md §5.2: `FUN_006dfa00` = 5× `DamageArea(100, C4)`; the visible result is a high-bridge collapse. | Witness `a63 … skipped` ×8 at f0. Depends on W2-GA-07 (destructible high bridges). | MINOR (SOV01 flow) | SOV01 | W2-GA-07, then apply `DamageArea` at the waypoint. |
| **W4-SQ-11** | Trigger actions / events | Still unimplemented in this survey's paths: <br>- `RevealZone` 18 (ALL07 ×3)<br>- `TurnOff` 61 (ALL10: Einstein's Chronosphere stays powered)<br>- `PreferredTarget` 35 (ALL11 nuke targeting)<br>- events 5 `HouseDiscovered` (SOV10 `Bad Guy 3 Startup`), 34 `ComesNearWaypoint`, 23 `TeamLeavesMap`<br>- `ChangeHouse` 14 on a tag-attached building reports `changed=0` (SOV09 `Switch Power House`)<br>- `FireSale` 9 `skipped` (ALL10 BG1-3) | `map-triggers.md` §5 | `map_trigger.cpp:77-84` (unimplemented events), `:1007` (unimplemented actions); witness lines | MINOR | ALL07, ALL10, ALL11, SOV09, SOV10 | Implement each (S each). |
| **W4-SQ-12** | AI production in missions | A `CreateTeam` for a computer house with no matching stock recruits 0. The team waits 900 frames and is dropped. The original's TeamType `Recruiter`/`Prebuild`/`Autocreate` and the AI house's team-driven production are not modelled for map teams. | SOV03 `Opening Harass 1` (200 s): `4Rocket Team` / `6 Rocket Team` `recruited=0`. RE-W4-SQ-2. | `campaign_runtime.inc:1446` (recruit timeout 900 → done) | MINOR-MAJOR (early harassment missing in SOV03; later waves depend on AI stock) | SOV03, SOV12 (`BG1 Startup` Rocketeer team recruited=0) | After RE: the AI house queues the missing TaskForce types (the skirmish team-brain path, W2-CA-07) and the team recruits when they are built. |
| **W4-SQ-13** | Timing | No wrong timer was found: every ElapsedTime/ScenarioTime beat fired at the frame the data gives (×15). The **delivery-dependent** beats are late (W4-SQ-07). In-game movies (100/117) are absent, so cutscene beats (ALL11 Romanov `IngameMoviePause`, SOV03 Yuri) are speech-only. That is not counted (install stubs). | — | witness | — | — | — |
| W4-SQ-14 | Performance (observation) | ALL03, the most unit-heavy opening (59 Russian units + hordes), ran at ~8.6 logic frames/s in the hidden `--play` (521 s for 4500 frames, while another agent's engine was running). All other missions ran at 60-400 frames/s. | — | `out\progress.txt` | INFO | ALL03 | Profile the combat step with many idle infantry (outside this audit's scope). |

## 3. Root causes grouped by system

- **Reinforcement delivery: W4-SQ-01, 06, 07.** The single fallback "TaskForce members appear at the team waypoint" (campaign.md
  §9.3) is behind every intro and landing difference: empty landing craft, cargo on the beach, paradrops on the ground,
  late LZ beats, SOV02's lost army, ALL05's spy loop and SOV09's dead Yuris. RE-C1 is now largely recovered (this
  report, from `FUN_0065d320` / `FUN_0065d770` / `FUN_0065da50`):
  - a non-Droppod reinforcement **is** created at the team waypoint, so the clone's position is right;
  - but a `Full=yes` team carries its passengers inside the transport;
  - and a `Droppod=yes` team arrives by plane from the house edge (or TransportWaypoint) and paradrops.
- **Team scripts: W4-SQ-02, 03, 04, 08.** These are argument decoding gaps (46/47 type + selector), unapplied TeamType
  fields (VeteranLevel), and the end-of-team behaviour of aircraft.
- **Object setup: W4-SQ-05.** Sleep/Harmless is not honoured (wave-2's W2-CA-18, still open). It now has a concrete cost:
  the ALL12 prisoners.
- **Trigger actions: W4-SQ-09, 10, 11.** Input lock is the one with the widest effect; the rest are single actions.
- **AI production in missions: W4-SQ-12.** Map AITriggers are armed and production starts on time (witness
  `production begins +map AI triggers` in ALL03/06/08/09, SOV03/04/08/10/11/12). The gap is scripted CreateTeams that
  expect the house to build their members.
- **Timing: none intrinsic.** All trigger timers matched the data.

## 4. Prioritized fix list

1. **W4-SQ-01 transport cargo** (M). This fixes the SOV02 blocker and the ALL05 spy loop, and makes every landing look
   right.
   - Acceptance: `sq-sov02t` keeps the player's 4 HTNK + Conscripts after the landing, and the transports alone change
     house.
   - Acceptance: `sq-all05s` gives the player's Spies house 3 spies at about 15 s, and `Spies Dead 1` never fires.
2. **W4-SQ-06 Droppod paradrops** (M, reusing the AmerParaDrop plane).
   - Acceptance: `sq-zsov09` keeps both Yuris alive at 100 s with no input.
   - Acceptance: ALL09 `Seals have Landed` fires within about 20 s.
   - Acceptance: ALL08 shows the start force falling.
3. **W4-SQ-09 input lock** (S). It protects every scripted opening and gives the original's "Battle control online"
   moment.
4. **W4-SQ-02 script 46/47 type/selector + garrison entry** (S-M).
   - Acceptance: `sq-all03u` has `garrisoned(1) > 0` by 20 s.
   - Acceptance: `sq-all04u` has no Conscript blobs in the streets.
5. **W4-SQ-05 Sleep/Harmless** (S). Acceptance: `sq-all12s`, houses 2 and 5 lose 0 units in the first 60 s.
6. **W4-SQ-04 VeteranLevel** (S); **W4-SQ-03/08 aircraft team exit** (S after RE).
7. **W4-SQ-12 AI stock for scripted teams** (M, needs RE-W4-SQ-2).
8. **W4-SQ-10** (with W2-GA-07) and **W4-SQ-11** (S each).

To re-run the evidence, run `pwsh D:\ra2-wt\w4-seq\scratch\runsq.ps1 [-Only sq-<map>]` (about 15-100 s per mission;
ALL03 about 9 min), then `python scratch\an.py sq-<map>`.

## 5. Open RE questions

- **RE-W4-SQ-1** `FUN_0065da50` placement details: the facing from the house edge (`house+0x1E0`), the airborne (z −400)
  branch for all-aircraft teams, and which "occupied cell" retry rule is used. Also which mission an `Unload` (8) arg 0/1/2
  selects (unload all / transports return / keep).
- **RE-W4-SQ-2** CreateTeam (4) for a computer house when the TaskForce has no stock: does the TeamType's `Recruiter` /
  `Prebuild` / `Autocreate` (`+0xA8..+0xAA`) make the house produce the missing members, and for how long does the
  team wait?
- **RE-W4-SQ-3** `DestroyTeam` (5) semantics, and what an aircraft does when its team script ends (return to an airfield,
  leave the map, or circle).
- **RE-C1** is mostly answered above. What is still open is the paradrop plane type per house (`DAT_00a8a184` index from
  `FUN_0041cad0`) and its flight entry cell.
