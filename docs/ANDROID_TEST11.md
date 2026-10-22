# Android test 11: Yuri bunker, control ownership and firing audio

Install over test 10 and start a fresh match. Save format 51 includes bunker
rules and Mastermind overload state. Old saves are incompatible. Imported game
files stay in the app. All multiplayer peers need this build.

## Research and expected behavior

The original [Yuri's Revenge manual](https://manuals.plus/m/6272c40b5b024d58eb23722bcdcfc61902ffe7b6d76c2b33249827b105be1718.pdf),
printed pages 20–24, distinguishes the special mechanics below. It is original
publisher material hosted by a third party. Reference rules/art from
[this public INI repository](https://github.com/Asxcvbn/Ra2Yr-ini-mod-simple)
are modified, so they establish tag usage, not the tester's exact retail values.

| Mechanic | Intended behavior | Change / remaining gap |
|---|---|---|
| Tank Bunker | One eligible turreted vehicle; Magnetron excluded; occupant gains defenses and firepower | Parse Bunker/Bunkerable and share existing entry, occupancy, firing and exit machinery. Apply CombatDamage bunker bonuses and display the stored tank. Full original damage routing and exact art alignment remain unverified. |
| Yuri Clone / Prime | Stock primary mind-control capacity is one; choosing a new victim replaces the previous one | Prevent full single-link controllers from automatically cycling victims. Explicit replacement still releases the previous owner correctly. Prime building capture remains unimplemented. |
| Psychic Tower | Stock weapon capacity is three | Existing capacity gate is regression-tested against a fourth victim. |
| Mastermind | More than three captures are possible; excess control causes overload damage | Parse CombatDamage OverloadCount/Damage/Frames and apply the matching damage band. Capacity still follows the weapon rules, not a universal hardcoded three-unit ceiling. |
| Psychic deploy | Area-fire ability is separate from mind control | Requested touch convenience: release existing unit links before the deploy blast; label RELEASE when applicable. This is an explicit behavior choice, not a claim of exact original-game parity. |
| Magnetron | Lifts/pulls vehicles; separate magnetic attack damages structures | IsLocomotor primary damage no longer becomes an ordinary instant-kill shell. Building targeting selects the secondary. **Vehicle lifting, pulling and dropping are still unimplemented: the primary has no vehicle effect in this build.** |
| Gattling firing sound | A firing effect must end and attenuate spatially | Use one owned report loop per unit/building, stop on firing-state loss, and update its spatial position. Actual retail sound playback still needs a device test. Heat stages and the full multi-weapon Gattling system remain incomplete. |

A primary gameplay reference was found:
[Testing Tank Bunkers](https://www.youtube.com/watch?v=A1j8sNm8IEA).
Its creator describes using controlled vehicles and anti-air occupants. Search
metadata/description was inspected; the footage was not played or visually
verified by this run. Modified-faction showcase videos were not treated as
stock-game evidence.

## File-level implementation

- `weapon_resolve.cpp`, `building.hpp`, `unit_w7.hpp`: parse Bunker,
  Bunkerable, IsLocomotor and the CombatDamage bunker/overload settings. Reuse
  resolved primary/elite occupant weapon slots for vehicles.
- `world_depth.cpp`: eligible own tanks use the existing Enter mission, one
  occupant slot, their own weapon (including an available anti-air secondary),
  bunker range/damage/ROF bonuses, and normal unload. Empty bunkers keep their
  owner; blocked manual exits keep their tank. Owner-mismatched occupants cannot
  fire for the bunker. Movement/infantry garrison code is shared, not copied.
- `main.cpp`: draw the occupied tank at the bunker center as a presentation-only
  copy, without putting the off-map stored unit back into simulation occupancy.
- `world_yuri.cpp`, `unit_orders.cpp`: enforce explicit replacement for a full
  one-link controller, prune stale links, preserve controlled victims inside
  storage, release ownership when the controller changes owner, and apply
  Mastermind overload damage. Stored tanks are ejected on controller loss when
  their owner no longer matches the bunker.
- `world_abilities.cpp`, `touch_bar.inc`: release-before-blast deployment and a
  RELEASE label for a psychic currently holding units. This affects the skirmish
  links implemented in this fork; campaign control has its separate runtime.
- `combat.cpp`, `weapon_unit.cpp`: guard unsupported locomotor-beam damage and
  select the magnetic secondary for structures. This is a guard against false
  damage, not an implementation of the missing magnetic locomotor.
- `weapon_sound_loop.hpp`, `pres_game.inc`: shared report-loop ownership for
  units, garrisons and defenses. A hundred repeated shots no longer imply a
  hundred independent looping voices. Normal one-shot sounds keep their path.
- `save_fields.hpp`, `savegame.hpp`: serialize the new rule/state fields and
  bump save version. Source credits and license remain intact.

## Phone testing

1. Select a Lasher and tap an empty own Tank Bunker. Check entry, visible tank,
   automatic fire and UNLOAD. Try a second tank, enemy tank and Magnetron:
   they should not enter. Block the exits and check manual unload loses no tank.
2. Control a tank, place it in the bunker, kill its psychic controller, then
   verify ownership is restored and the tank leaves when the exit is free.
3. For Clone and Prime, control a unit, wait near other enemies, then explicitly
   target another. There should be only one current link. Tap RELEASE to release
   and blast; expect the normal deploy cooldown.
4. Fill a Psychic Tower to its configured capacity; a further enemy stays enemy.
   Give a Mastermind more than three victims under stock rules and watch its HP.
5. Force-fire a Gattling Tank, press STOP, pan away and return. Repeat with a
   Gattling Tank in a bunker. Check the firing sound does not remain stuck.
6. Magnetron can attack buildings with its secondary, but its vehicle lift is
   unavailable. Do not treat a harmless primary beam as a working lift mechanic.

## Verification limits

Targeted native run: 178 selected cases reported passed, 2,400 assertions, zero
failures; 44 selected retail-dependent cases returned early. Another 987 cases
were excluded by the file/case filters. An earlier broader run threw `mix: too
small` in the retail-only `w7units: rulesmd type flags` case; it was explicitly
excluded from the final run, not repaired or reported as passing. Synthetic
INI tests cover this update's newly parsed fields. The production touch-panel
harness passed. Android main.cpp syntax checking passed with two existing
warnings. Actual APK checks are attached separately with the release.

No device, retail-art render or real sound playback was available. Synthetic
tests exercise ownership/capacity, overload timing, bunker occupancy/combat,
save/load, special-damage guards and the production report-loop helper. Allied
and Soviet mechanics are reused and regression-checked where available; neither
faction is claimed fully verified. Yuri remains a preview with several special
abilities, superweapons and AI still incomplete.
