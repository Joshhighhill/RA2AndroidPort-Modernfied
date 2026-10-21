# Next-session handoff: RA3-inspired unit panel and Generals touch audit

Researched 21 October 2026. User requests a bottom-left icon GUI and later Yuri's Revenge fixes. Research and standalone interactive HTML prototype only: no engine changes or new APK in this pass.

## Design decision

Use a compact bottom-left panel with selected-unit identity, a stable group 1–5 row, and a 4-by-2 grid: Stop, Guard, Deploy, Scatter, Force fire, Force move, Add selection, More. The prototype uses original inline SVG paths and text fallback, with no EA/Generals artwork or fonts copied. Buttons explain the intended actions; they issue no game commands. Group hold/double-tap behaviours are described, not simulated. The selected-units toggle demonstrates disabled actions; actual capability predicates must come from the live selection.

This adapts RA3's selection/command organisation to the user's requested corner; do not claim the mockup reproduces the exact retail RA3 layout. The EA-authored manual's search-indexed controls list confirms reverse move, force attack and stances. Full manual fetch failed in this environment, so exact icon positions, visual spacing and all stance rules remain unverified. Do not add pretend RA3 capabilities to RA2.

## Sources and findings

- EA-authored RA3 manual distributed by Steam: https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/17480/manuals/manual_english.pdf?t=1712755099 . Search indexing exposes unit commands including reverse move, force attack and aggressive stance; full PDF unavailable through the browser tool during this pass.
- MYSOREZ live README: https://github.com/MYSOREZ/GeneralsZH-Android-Port#touch-controls . Consulted current main; it can change. Describes native battlefield taps, double-tap type selection, hold-drag box selection, drag pan, two-finger pan/zoom, stationary hold and two-finger cancel, button descriptions on hold, and aimed abilities. Builder command pages and force-attack/waypoint affordances are documented.
- MYSOREZ port guide: https://github.com/MYSOREZ/GeneralsZH-Android-Port/blob/main/docs/port/ANDROID_PORT.md#2a-input-native-touch-not-mouse-emulation . UI gets input before the battlefield. Builder touch orders use a separate page; registerBuilderPageImages generates page arrows at startup. This does not prove every command icon is generated or diagnose an installation's missing assets.

## Current fork audit (8ea5e4a)

- engine/touch_bar.inc: TouchBtn stores only label and command ID; draw_touch_bar fills rectangles and calls draw_text. There is no icon texture lookup. Missing command pictures in OUR bar are an implementation gap, not evidence of failed game-file import. If the user means missing production cameo art or icons inside Generals itself, request a screenshot and precise screen later; the cause is not established.
- engine/touch_bar_layout.hpp: current geometry anchors nine-column rows to bottom centre; implement a bottom-left grid here, sharing geometry between rendering and hit testing.
- engine/touch_ui.inc: mode/cancel banner and first-use hints stack above current dock. Update their anchoring with the panel. Reserve safe areas, avoid the right production sidebar and bottom system gestures.
- engine/game_scene_controls.inc and engine/input.hpp: existing Stop/Guard/Scatter/Deploy, TypeSelect, CenterBase and groups can be reused. Force fire and force move are modifier modes, not new movement algorithms. PlanningMode is explicitly not implemented in game_scene_controls.inc; do not label MOVE as waypoint queue. RA3 reverse move, stances and attack-move need independent simulation/command audit.
- Group holds already assign groups; do not replace these with tooltip holds. Ordinary action buttons can support descriptions, but their hold/release must suppress accidental activation. Show armed state and Cancel; consume complete panel gestures so no order reaches terrain beneath.

## Gesture gaps versus Generals reference

Keep the pan and hold-box scheme the user reports works well. Native pinch zoom is still absent in this fork. Generals' aimed-ability release flow differs from this fork's confirmation taps. Neither can be supplied by icons alone. Retain RA2 group hold semantics and document intentional differences instead of promising identical gestures.

## Next implementation and checks

1. Review prototype on the POCO landscape screen; confirm panel size and icon readability.
2. Add an engine-native icon ID and SDL vector/primitive drawing with label fallback. Avoid dependency on retail icon sheets. Contextually enable Deploy and other actions from actual eligibility, without duplicating simulation rules.
3. Reposition the panel, banner, tooltips and probe targets. Keep groups visible after selecting a group; extra commands may use pages. Offer size/opacity and corner preference after the base layout works.
4. Test all rectangle bounds, menu suppression, slide-off, panel-origin drag, long-press description vs group assignment, modifier cancellation, and no map click-through. Re-run gesture regressions and a signed ARM64 release build with the existing testOnly guard. No phone or GUI rendering tests have been run on this prototype.
5. Yuri's Revenge: later collect the exact problem (import, startup, faction/unit behaviour, campaign or rendering), map/files involved and logs. No specific Yuri bug has yet been reported; do not invent a diagnosis or claim it fixed.

## Artifacts

ra3-unit-panel.html is an interactive design prototype. Existing APK test 3 is unchanged. No full APK rebuild was done for this research pass, to conserve the user's remaining usage.
