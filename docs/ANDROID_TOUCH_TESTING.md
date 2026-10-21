# RA2 touch test â€” POCO X7 Pro

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
| Bottom shortcuts | STOP, GRD, SCAT, DPLY and selection modifiers stay visible at bottom centre |
| Tap CMD | Show/hide the second row: groups 1–5, TYPE, HOME and MORE |
| Group button | Tap to select; hold to assign selected units; double tap to centre |
| Tap MORE / BACK | Reach teams 6–10, views and extra commands / return |
| Arm sell/repair/placement | Instructions and X cancel appear above the dock, without covering it |
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

## Corrected sideload package (test 2)
Test 1 accidentally used Android Studio's internal injected ABI option, marking the APK testOnly and blocking normal installation. Build test 2 with ordinary assembleRelease, -Pra2.abis=arm64-v8a and -Pra2.buildNumber=3. Keep the existing signing key in ignored android/keystore.properties. Before publication run tools/android/verify_sideload.py with --build-tools and --report; it rejects testOnly and debuggable manifests and verifies signing, alignment and ZIP integrity. No phone installation is implied by these checks.


## Bottom shortcut dock (test 3)
The primary command row remains visible at the bottom centre of the tactical viewport. CMD toggles the second row, initially expanded. The extra row collapses after a one-shot action; there is no idle timeout. The mode banner and first-use hints stack above the dock. Gesture thresholds and game command handlers are unchanged.
