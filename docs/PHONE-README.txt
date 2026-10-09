RA2 Engine - phone install
==========================

1. Copy RA2-Engine.apk to the phone and open it to install
   (allow "Install unknown apps" when asked). To update, install the new
   APK over the old one - your game files and settings are kept. NOTE:
   saved games do not carry over between engine versions yet; after an
   update the old saves are listed as "Older version" and cannot be
   loaded. Finish a mission before you update.

2. Copy the whole RA2 folder (11 files, ~712 MB) into the phone's
   Download folder, so you have Download/RA2/ra2.mix etc.

3. Open RA2 Engine -> "Import game files". The picker opens at
   Download/RA2. IMPORTANT: select the RA2 folder itself (Android does not
   allow picking Download directly), tap USE THIS FOLDER, then ALLOW.
   Copying takes about 30 seconds - keep the screen open until it ends -
   then the game starts. Afterwards you can delete Download/RA2.

   To add or replace files later (campaign maps, music, patches such as
   ecache01.mix, or your own movie files - MOVIES01.MIX, MOVMD03.MIX or
   *.bik), put them in Download/RA2 and import again: long-press the app
   icon -> "Import game files", or in the game Options -> Game Files.
   Saves and settings are kept; there is no need to clear the app data.

Touch controls
--------------
- Tap: select, or give an order to the selected units.
- Double tap a unit: select every unit of that type on screen.
- Drag with one or two fingers: scroll the map.
- Hold (a ring fills), then drag: draw a selection box.
- Hold, then lift: cancel the armed mode (sell / repair / placement /
  superweapon). It never deselects.
- Two-finger tap: cancel a mode, else deselect everything.
- Three-finger tap: show / hide the frame-rate overlay.
- CMD (left edge): the command column - +SEL, FIRE, MOVE, STOP, GRD, SCAT,
  DPLY, BRF (mission briefing), teams 1-5 (tap = select, hold = create).
- Sell / repair / placement modes show a banner with an X to cancel.
- Back button: in-game options menu. On the mission results page Back =
  Continue (or Main Menu after a lost mission); on a loading screen it
  returns to the main menu.

If the game is closed
---------------------
If Android closes the game in the background, or restarts it (for example
after switching to gesture navigation or changing the theme), the game was
saved automatically: the next start shows RESUME GAME? with the mission
name. MAIN MENU asks before it throws that game away. If you won a mission
and the game was closed on the results page or the next loading screen,
the next start offers CONTINUE CAMPAIGN?.

About
-----
Options -> About shows the version, the licence and the third-party
notices.

Reporting performance
---------------------
Three-finger tap in a game shows FPS (frames per second), LOGIC (15/s =
full speed) and the milliseconds per frame. Please send a screenshot of it
in a busy moment, with the phone model. With a PC: see docs/INSTALL.md
section 5 (adb shell setprop debug.ra2.perf 1, then adb logcat).
