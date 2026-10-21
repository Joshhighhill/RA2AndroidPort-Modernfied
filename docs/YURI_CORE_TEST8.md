# Yuri core follow-up, Android touch test 8

This update implements the first skirmish economy and mind-control loop. It
does not complete Yuri's Revenge compatibility.

## Implemented

- Force Fire and Force Move: `touch_bar.inc` now stamps modifiers on pointer
  move and map-origin pointer release. Previously that release returned before
  stamping Ctrl/Alt, so Input discarded the armed modifier when issuing the
  order. The committed standalone `tests/touch_panel_runtime_harness.cpp`
  includes the production panel code and checks press/move/release Ctrl plus
  one-shot reset, panel capture, groups and folding.
- `world_yuri.cpp`: skirmish mind-control units intercept the real combat shot,
  capture eligible units, retain original ownership, enforce capacity and
  capacity-one replacement, and release victims when the controller dies,
  boards, changes owner or is erased. Campaign control remains with its own
  runtime/team bookkeeping. Unit types resolve MindControl from their primary
  weapon's warhead in `weapon_resolve.cpp`.
- Deployed buildings with Enslaves spawn real workers using SlavesNumber,
  SlaveRegenRate, the worker's resolved combat/infantry type and Storage.
  Workers use existing finite-ore harvesting, movement, dock queues and income
  accounting, returning to their own manager. Lost workers regenerate. A dead
  manager releases workers to its known unit killer's house, otherwise they
  retain their house and stop harvesting. Standard miners do not unload at
  slave managers.
- The new ownership/worker/manager state and resolved building worker profiles
  are serialized. Save format is **48**: start a fresh match rather than loading
  a save from tests 1–7. Both multiplayer peers would need the same build.

## Validation

The desktop test executable was rebuilt. Targeted Yuri, harvest, save/load and
sync-serialization tests passed: 47 cases, 833 assertions. Two existing
retail-map-dependent cases print an internal skip because those assets are
absent. The four new synthetic Yuri cases passed 25 assertions, including an
ordinary combat shot, finite ore income, owner restoration after load and
identical simulation continuation after saving workers.

The first wider run failed an existing save-slot filesystem test in the
sandbox's default temporary directory; rerunning with a checked workspace
temporary directory passed. This was not silently counted as a passing run.
The production touch-panel integration harness passed. Android main.cpp syntax
check passed with two existing warnings. APK build/package results are reported
separately with the release; no phone or real-retail-art test is claimed.

## Remaining gameplay and visual gaps

- Worker harvesting currently uses generic harvester dig/unload timing and dock
  positioning, not a complete reconstruction of SlaveReloadRate/SlaveManager.
  Automatic mobile-miner redeployment/worker recall is not implemented.
- Deployed Slave Miner body artwork still needs verification/fixing with actual
  game files. This update does not claim to repair the missing body.
- Psychic Tower and Yuri Prime building capture, Mastermind overload, gattling
  stages, Magnetron lifting, Chaos berserk, Disk drain, Grinder/Bio Reactor
  special interactions and Yuri superweapons remain incomplete or unsupported.
  These units/buildings must not be taken as faithful simply because visible
  in the build list. Yuri AI remains disabled.
- Menu research is in `docs/design/yuri-main-menu-research.md`. The current
  shell is an RA2 reconstruction; the original 800x600 YR menu is not restored.

## Phone test

Install over test 7 and start a new human-Yuri skirmish. Deploy a Slave Miner
near ore and confirm workers appear, move and increase credits. Build a Yuri
Clone and attack a non-immune enemy ground unit; destroy the controller and
confirm the victim returns to its previous owner. Select a tank, tap FIRE,
then tap ground or a friendly target and check it attacks. Test MOVE the same
way. Report missing worker/body art separately from income or order failures.
