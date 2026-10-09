RA2 Engine - phone install
==========================

1. Copy RA2-Engine.apk to the phone and open it to install
   (allow "Install unknown apps" when asked). To update, install the new
   APK over the old one - your game files and saves are kept.

2. Copy the whole RA2 folder (11 files, ~712 MB) into the phone's
   Download folder, so you have Download/RA2/ra2.mix etc.

3. Open RA2 Engine -> "Import game files". The picker opens at
   Download/RA2. IMPORTANT: select the RA2 folder itself (Android does not
   allow picking Download directly), tap USE THIS FOLDER, then ALLOW.
   Copying takes about 30 seconds - keep the screen open until it ends -
   then the game starts. Afterwards you can delete Download/RA2.

Touch controls
--------------
- Tap: select, or give an order to the selected units.
- Tap a building anywhere on its picture (also a tower's top) to pick it.
- With two or more units selected, tapping one of them moves the group
  there; tap a unit that is not selected to select it.
- Double tap a unit: select every unit of that type on screen (never deploys).
- Tap the only selected unit, then wait: deploy / unload it (or DPLY).
- Drag with one or two fingers: scroll the map.
- Hold (a ring fills), then drag: draw a selection box. Or turn on BOX in
  the command column: then a one-finger drag draws the box at once.
- Hold, then lift: cancel the armed mode (sell / repair / placement /
  superweapon). It never deselects.
- Hold a cameo: its name and price show above your finger; the build keeps
  going. While it builds, a small menu opens: PAUSE / RESUME, CANCEL, ALL.
- Sell / repair: tap a building to mark it, tap it again to do it.
- Superweapon: tap to see the target area, tap the same spot again to fire.
- Tap the radar: look there (arm MOVE first to send the units there).
- Two-finger tap: cancel a mode, else deselect everything.
- Three-finger tap: show / hide the frame-rate overlay.
- CMD (left edge): the command column - +SEL, FIRE, MOVE, STOP, GRD, SCAT,
  DPLY, BOX, BRF (mission briefing), teams 1-5 (tap = select, hold =
  create), MORE: TYPE, HOME, EVT, NEXT, PREV, FOLW, teams 6-0,
  V1-V3 (hold = save the view, tap = jump back).
- Sell / repair / placement modes show a banner with an X to cancel.
- Back button (or the Options button on the sidebar): in-game options menu.

Reporting performance
---------------------
Three-finger tap in a game shows FPS (frames per second), LOGIC (15/s =
full speed) and the milliseconds per frame. Please send a screenshot of it
in a busy moment, with the phone model. With a PC: see docs/INSTALL.md
section 5 (adb shell setprop debug.ra2.perf 1, then adb logcat).
