# Android touch test 9 — faction panels and verified gameplay fixes

Install over test 8, then start a **fresh match**. Save format is now 49 for
Psychic Tower controller/victim state; older saves are incompatible. Imported
game files stay in the application. Use the same build on multiplayer peers.

## Changes

- `touch_theme.hpp`, `touch_bar.inc`: Allied blue, Soviet red and Yuri purple
  command panels, bevelled frames, faction-coloured active commands and icons,
  selection health bars and HP totals. The local player's faction owns the
  skin even when selecting captured units. Collapse and command positions stay
  consistent. The HTML comparison preview is included in source.
- `main.cpp`, `sidebar.cpp`: try Yuri's original sidebar archive and its own
  palette/radary.shp. Search expansion-nested, classic-nested and loose archive
  candidates, require the core chrome entries, and fall back to Allied chrome
  if the archive is absent/incomplete. Yuri uses Soviet control coordinates,
  as documented in docs/re/construction.md, and a separate texture cache key.
- `world_depth.cpp`, `world.cpp`: a ground Force Fire order replaces the old
  movement mission. Out-of-range units approach, then stop at weapon range
  instead of walking all the way into the tapped cell while firing. Holding
  uses committed cell coordinates like existing object attacks, avoiding a
  mid-segment range oscillation. Invalid off-grid ground orders are refused.
- `building_combat.cpp`, `world_yuri.cpp`, `weapon_resolve.cpp`: Psychic Towers
  capture eligible ground units through their actual combat shots rather than
  dealing plain weapon damage. Capacity comes from weapon Damage. Controlled
  units cannot be stolen by another controller. Destruction, owner change or
  loss of building power releases them. Ownership and capacity survive save/load.
  Campaign control remains with its existing runtime.
- `unit_orders.cpp`: ownership changes detach units from their previous AI
  teams and clear team aggression/suicide flags, preventing stale team orders.
- `voxel_raster.cpp`: match HVA animation sections to VXL model section names
  for bounds, pixels and shadows. Reordered named sections no longer apply
  another part's transform. Unmatched legacy sections retain index fallback.
  This fixes a reproducible renderer bug; it does not prove that this caused
  the tester's specific Lasher model issue.

## Checks actually run

- Rebuilt desktop test executable. Targeted Yuri, harvesting, voxel raster,
  sidebar layout, save/load and sync tests: **61 cases, 976 assertions passed**.
  Three retail-asset-dependent cases print internal skips (assets unavailable).
- New regressions cover ground-fire movement stopping, Psychic Tower capacity,
  capture without damage, power-loss release after loading, immediate AI team
  detachment, reordered VXL/HVA sections and Yuri sidebar control geometry.
- Production touch-panel runtime harness compiled and passed. Android main.cpp
  syntax check passed with two existing warnings. APK build/signature checks
  are recorded separately in the release's BUILD_INFO / INSTALL_CHECK reports.
- No Android phone test, real retail artwork render, or visual screenshot
  comparison was available here. Sidebar archive presence checks do not prove
  that the tester's archive is complete or that every graphic renders correctly.

## Remaining limits

Yuri is still a preview. Yuri Prime building capture, Mastermind overload,
gattling stages, Magnetron lifting, Chaos berserk, Disk drain, Grinder/Bio Reactor
special interactions, Yuri superweapons and Yuri AI are not complete. Worker
mining retains generic harvesting timing; mobile manager recall/redeployment
is incomplete. Deployed Slave Miner body artwork and the exact Lasher model
still require phone/asset verification. The original YR main menu is researched,
not restored. This release does not claim all game bugs or full YR compatibility
are fixed.

## Phone checklist

1. Start one new skirmish per faction; compare panel colour, collapse arrow,
   health display, production tabs, repair/sell hit boxes and radar.
2. Order a tank to move, then FIRE at a ground cell within range. It should
   cancel the old route. FIRE farther away should approach only to firing range.
3. Build a Psychic Tower and bring eligible enemy units near it. Check capacity,
   unchanged victim health, ownership colours, and release when power is lost.
4. Recheck Lasher/mobile miner models and deployed miner body separately from
   worker movement and credit income. Keep the exact unit names for follow-up.
