# Android touch test 10 — Yuri factory and Slave Miner follow-up

Start a fresh match. Save format 50 records the new meaning of packed-worker
manager links; saves from earlier test builds are incompatible. Keep imported
retail files in the app. Multiplayer peers must use the same build.

## Engine changes

- `production.cpp/.hpp`, `main.cpp`, `world_econ.cpp`, `realplay_scene.inc`:
  implement the rules-defined single vehicle `PrerequisiteProcAlternate`.
  A living owned SMIN can satisfy PROC while mobile; the ordinary refinery
  building group still works. Other prerequisites remain mandatory. Both the
  sidebar and production authority see the same unit list. This fixes one
  concrete cause of a hidden YAWEAP; missing archive art or country restrictions
  can still hide it independently.
- `world_mcv.cpp`, `world.hpp`, `command.cpp`, `game_scene_controls.inc`,
  `touch_bar.inc`: selected deployed Slave Miners expose UNDEPLOY and use the
  ordinary Deploy command. Packing creates the configured SMIN with proportional
  health, releases the foundation and pays no sale refund. Ownership, Idle state,
  the rules-defined undeploy registry and an unoccupied spawn cell are required.
  This action is independent of the ConYard MCVRedeploys lobby setting.
- `world_yuri.cpp`, `unit_w7.hpp`, `world_mcv.cpp`: workers remain on the map,
  stop harvesting and follow their packed miner. Redeployment reassigns the
  existing workers and resumes harvesting rather than generating a free extra
  workforce. The association survives save/load. This is a practical follow
  implementation, not the original game's timed recall/boarding sequence.
- `inf_anim.cpp/.hpp`, `world.cpp`: use SlaveSequence Shovel while harvesting
  and Carry while moving with ore. Return to normal poses afterward; only use
  sequences that the art table actually defines with nonzero frames.
- `main.cpp`: preload manager-spawned worker SHPs and basic animation metadata,
  even when workers are absent from the buildable infantry roster. Logs include
  the resolved image/frame count or loading failure. Full infantry sequences
  still come from the existing all-infantry sequence registration.
- `asset_search.inc`, `pres_fx.inc`: NewTheater animations now try theater names
  before literal SHPs. Building lookup also permits a literal-name fallback
  after themed/generic names. This addresses inconsistent animation selection;
  it does not demonstrate the tester's actual asset set renders correctly.

## Phone checks

1. Start as Yuri. Build a Bio Reactor and Barracks; check for War Factory with
   both a deployed and a mobile Slave Miner. Other normal tech requirements apply.
2. Select the deployed miner and tap UNDEPLOY. Move it, watch workers follow,
   then deploy it again. Check health is retained, workers resume mining, the
   worker count does not double, and credits are not refunded by packing.
3. Watch workers idle, walk, dig and carry ore at several zoom levels. If a
   worker disappears, note its activity and whether its selection marker remains.
4. Save/load while the miner is packed; redeploy and check the same workers return.
5. Check ordinary MCV deployment and Allied/Soviet production still work.

## Limits

## Validation performed

Targeted native selection: 89 cases reported passed, 1,115 assertions, zero
failures. Twelve selected retail-asset cases returned early because those
assets are unavailable; 1,068 other cases were excluded by the file filter.
The production touch-panel integration harness passed, including the new
building UNDEPLOY label/ownership gate. Android main.cpp syntax checking passed
with two pre-existing warnings. Synthetic tests cover mobile PROC, production
acceptance, health-preserving packing, blocked packing, workforce reuse,
save/load of packed links and shovel/carry sequence transitions.

## Remaining limits

No POCO X7 Pro or user's retail assets are available locally. Worker visibility,
the precise factory cameo/art and deployed miner graphics require phone testing.
Voxel building turret overlays (including SMINTUR) remain unsupported. Worker
HarvestRate/SlaveReloadRate, original recall animations and exact original-game
timing remain incomplete. Yuri remains a preview: Yuri AI, superweapons and
several special unit/building abilities are not complete.

Reference configuration inspected: the publicly posted
[rulesmd.ini](https://github.com/Asxcvbn/Ra2Yr-ini-mod-simple/blob/master/rulesmd.ini)
and [artmd.ini](https://github.com/Asxcvbn/Ra2Yr-ini-mod-simple/blob/master/artmd.ini).
These are modified reference files, not proof of the tester's installed data.
No retail art is bundled. Native engine credit remains omarelarab/ra2-engine
(0BSD); see CREDITS.md and THIRD_PARTY_NOTICES.md.
