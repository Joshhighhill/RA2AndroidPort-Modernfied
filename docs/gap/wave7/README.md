# Wave 7 — character behaviour gap analysis (2026-10-12, master b3a509f)

These are 12 read-only opus audits of every unit type against the original (YR 1.000 gamemd.exe in Ghidra, rulesmd/artmd/soundmd, `--play` runs). Each report has a per-unit table, numbered gaps W7-NN-MM with severity and evidence tags, and a verified-OK list.

| # | Report | Gaps |
|---|--------|------|
| 01 | [Allied infantry](01-allied-infantry.md) | 45 (11 major) |
| 02 | [Soviet infantry](02-soviet-infantry.md) | 12 (4 major) |
| 03 | [Allied vehicles](03-allied-vehicles.md) | 13 (4 major) |
| 04 | [Soviet vehicles](04-soviet-vehicles.md) | ~31 (6 major) |
| 05 | [Aircraft](05-aircraft.md) | 13 (5 major), 2 withdrawn |
| 06 | [Naval](06-naval.md) | 17 (5 major) |
| 07 | [Shared combat](07-shared-combat.md) | 29 (several major) |
| 08 | [Abilities & status](08-abilities-status.md) | 24 numbered IDs, 15 used (4 major) |
| 09 | [Civilians & campaign characters](09-civilians-campaign.md) | 19 (5 major) |
| 10 | [Yuri faction](10-yuri-faction.md) | 13 (2 major; only the Yuri Clone is reachable) |
| 11 | [Locomotion](11-locomotion.md) | 17 (3 major) |
| 12 | [Presentation](12-presentation.md) | 46 (7 major) |

## Cross-cutting root causes (one fix closes gaps in several reports)

1. **Allied crush.** `world.cpp:5670` crushes any other house. The original refuses allies (IsCrushable 0x5F64C0) and warns them to scatter first (0x7404B0). The neutral civilian house must count as allied for ALL01. Reports: W7-03-01, W7-04-01, W7-07-11, W7-09-04.
2. **Per-type tables are keyed by SHP/VXL image instead of rules id.** This affects:
   - voices and sounds;
   - fear flags;
   - Rocketeer wobble;
   - probe/AI identity.

   Reports: W7-01-12, W7-05-03/08, W7-09-01/02/17, W7-12-28.
3. **`valid_target` requires `is_enemy`.** This breaks force-fire on own, allied and neutral targets, Ivan bombs on own units, and Tesla coil charging. Reports: W7-07-10, W7-02-01/02.
4. **No mission model.** Mission Control keys are unread, so retaliation, Area Guard, Harmless/Sticky, AI fire-on-move and attack-move are all wrong. Reports: W7-07-01..09, W7-09-07..13.
5. **`OpportunityFire` is unread.** Grizzly, Rhino, Robot Tank, War Miner, Destroyer and Aegis never fire while moving. Reports: W7-03-03, W7-04-04, W7-06-05.
6. **Speed model.** The 1.6 GameSpeedBias is never read by the original (gamespeed.md §4 is wrong). Infantry and Hover move 1.6x too fast, Jumpjets about 4x (aircraft.md documented the Hover locomotor), and fixed-wing planes move and climb wrong. Reports: W7-11-01/02, W7-05-02/15.
7. **No repair/service-depot order.** You can't send units to the depot, so Terror Drones can't be removed. Reports: W7-04-06, W7-08-01.
8. **Spawn manager launches one spawn per reload.** The Carrier and Dreadnought are affected. Reports: W7-05-05, W7-06-03, W7-04/06-08.
9. **Select Combatants (P) ignores IsSelectableCombatant.** Reports: W7-01-13, W7-02-07.
10. **AI roster/deploy.** The AI never deploys GIs or Desolators (`AIAutoDeployFrameDelay`). Robot Tank, Battle Fortress and Siege Chopper are missing from the AI roster. The AI fleet musters on land. Reports: W7-01-08, W7-02-04, W7-03-08, W7-05, W7-06-01.
11. **Transports and passengers.** Passengers die with their transport; the original ejects them. Infantry walk beside a transport instead of entering its cell. Reports: W7-04-02, W7-11.
12. **BuildTimeMultiplier is unread.** Rhino 539 vs 810 frames. Report: W7-04-03.
13. **Iron Curtain only blocks damage.** It should also refuse erase, infest, crush, mind control and chronoshift, and it defuses C4. Report: W7-08-04 (this corrects W7-01-23).

## Docs found wrong
- `docs/re/gamespeed.md` §4 (the GameSpeedBias claim).
- `docs/re/aircraft.md` §1/§5: the jumpjet section actually describes Hover.
- `docs/re/transport-cargo.md` §5: passengers survive.
- `docs/re/audio-playback.md`: voices.
- `docs/re/capture.md` §A2/§C4 and `docs/re/abilities-w5.md` §1.

Ghidra decompile dumps are kept in `D:\ra2-re\w7-*-out`. Binary evidence (png/bin) stays local and untracked, as in earlier waves.
