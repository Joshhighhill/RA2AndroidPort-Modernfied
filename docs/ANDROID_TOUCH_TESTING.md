# RA2 touch test — POCO X7 Pro

This is the native RA2 Engine test build with the Generals-inspired touch
changes. It is intended for your ARM64 Android phone. No retail game files
are included in the APK.

## Install and import

1. Transfer the APK beside this file to your phone and open it to install.
   Allow installation from the app you used to open it if Android asks.
2. Open **RA2 Engine** and choose **Import game files**.
3. Select the folder containing your original game files and allow access.
   The importer copies supported files into the app's own storage.
4. Start a small **Skirmish** against one AI for the first test.

The four required files are `ra2.mix`, `ra2md.mix`, `language.mix`, and
`multimd.mix`. This engine needs Yuri's Revenge data as well as RA2. Keep
the other MIX and CSF files together in the same folder too. If the import
screen lists missing files, send that list or a screenshot before testing
gameplay. Your original game folder is the import source, not a folder to
delete or replace.

## First test (about ten minutes)

| Test | Expected result |
|---|---|
| Tap one of your units, then empty ground | Select the unit, then order it to move |
| Drag immediately with one finger | Move the camera without issuing an order |
| Hold for a quarter second, then drag | Draw a box to select units |
| Double tap a unit | Select matching units on screen |
| Drag with two fingers | Move the camera without clearing selection |
| Add a second finger during a pan, then lift | Stop panning without an accidental cancel |
| Two-finger tap | Cancel an armed mode, otherwise clear selection |
| Open CMD | Show the existing command buttons and control groups |
| Build a structure and place it | Show and place the footprint normally |
| Fight the AI for several minutes | Commands and rendering continue working |

Stationary hold-release keeps the native engine's existing behavior: cancel
the armed mode, without clearing the selected units. Holds on production
icons and group buttons retain their existing tooltip/group actions.
**Pinch zoom is not implemented in this native build yet.**

## Tell me what happened

Please report your Android/HyperOS version and these results:

- Did installation, file import, the main menu and the skirmish all work?
- Are taps accurate, including near screen edges and the production sidebar?
- Is it easy to choose between dragging the camera and drawing a selection box?
- Does camera movement ever give an unwanted order or clear your selection?
- Are there black graphics, missing textures, freezes, crashes or heavy stutter?
- At which exact step did any issue happen? A screenshot or short recording helps.

If the app closes, mention whether it happened during import, loading or actual
play. This package has not been tested on your phone in advance; your test is
the real-device validation for this build.
