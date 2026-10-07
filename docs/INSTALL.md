# Install and run (players)

This project is an **engine only**. It plays Command & Conquer: Red Alert 2
using the game files from **your own copy** of the game. No game data is
included or downloadable here. It is not affiliated with or endorsed by
Electronic Arts. See the README's "Legal" section.

## 1. The game files you need

Copy these from the folder where Red Alert 2 (with Yuri's Revenge) is
installed, the one that holds `ra2.mix`:

| File | Required | Used for |
|---|---|---|
| `ra2.mix` | yes | rules, art, theaters, units, menus |
| `ra2md.mix` | yes | rulesmd / artmd / soundmd, voxels |
| `language.mix` | yes | voices, EVA, cameos, strings |
| `multimd.mix` | yes | the skirmish maps |
| `langmd.mix` | no | skirmish map names |
| `mapsmd03.mix` | no | the default map |
| `theme.mix` | no | music |
| `expand01.mix` | no | the campaign rules patch |
| `Maps01.mix`, `MAPS02.MIX` | no | Allied / Soviet campaign maps |
| `ra2.csf` | no | the patched string table |

About 750 MB in total. Without a required file the engine shows a
**GAME FILES NEEDED** page that lists what is missing and where it looked.
Without an optional file you lose only that feature; for example, starting a
campaign without `Maps01.mix` shows an error page that names the file and
returns you to the main menu.

The engine currently reads the **Yuri's Revenge** data set (`rulesmd.ini`,
`ra2md.mix`, `multimd.mix`). An RA2-only install is not enough yet. See
`docs/gap/wave2/06-android-robustness.md`, Q1.

## 2. Desktop (Windows)

1. Unzip or build `ra2engine.exe` together with `SDL2.dll`.
2. Start `ra2engine.exe`. It looks for the game files in this order:
   1. `--data <folder>` on the command line;
   2. the `RA2_INSTALL_DIR` environment variable;
   3. `RA2.INI` `[Engine] DataDir=` in `%APPDATA%\ra2engine\RA2\`;
   4. the folder that holds `ra2engine.exe`;
   5. the retail install's registry key
      (`HKLM\SOFTWARE\Westwood\Red Alert 2`, `InstallPath`);
   6. `D:\Red Alert 2`.
3. If the files are not found, the **GAME FILES NEEDED** page lets you choose
   the folder with **BROWSE**. The choice is saved to `RA2.INI` `[Engine] DataDir`.

Other options: `--no-audio`, `--version`, `--help`. Settings (`RA2.INI`) and
saved games live in `%APPDATA%\ra2engine\RA2\`.

## 3. Android (5.0 and later; tested on an Android 11 emulator)

1. Install the APK (`adb install -r app-release.apk`, or open it on the phone).
2. Put the game files on the phone, for example in `Download/RA2`. You can
   copy them over USB (file transfer), or use adb:
   ```powershell
   adb shell mkdir -p /sdcard/Download/RA2
   foreach ($f in "ra2.mix","ra2md.mix","language.mix","langmd.mix","multimd.mix",
                  "mapsmd03.mix","theme.mix","expand01.mix","Maps01.mix","MAPS02.MIX","ra2.csf") {
     adb push "C:\path\to\Red Alert 2\$f" /sdcard/Download/RA2/
   }
   ```
3. Open **RA2 Engine**. The first screen lists the missing files. Tap
   **Import game files**, choose the `RA2` folder and allow access. The app
   copies the files into its own storage (about 750 MB, which takes a minute)
   and then starts the game.

Alternative: copy the files over USB (MTP) straight into
`Android/data/org.ra2.engine/files/ra2/`. On Android 11 and later, `adb push`
and on-phone file managers can no longer write to `Android/data`, so use the
importer or a PC.

Settings (`RA2.INI`) and saves are in the app's internal storage. They are
included in Android backups; the game files are not.

If the app is closed in the background (low memory), the game was saved to
`AUTOSAVE`. The next launch asks **RESUME GAME?**.

## 4. Controls

**Mouse (the original's scheme):**
- Left-click selects, and gives an order with a selection.
- Right-click deselects or cancels.
- Drag to box-select.
- Ctrl forces fire. Alt forces a move. Shift adds to the selection.
- Hotkeys come from `KeyboardMD.ini`: S stop, G guard, X scatter, D deploy,
  Ctrl+1..0 create a team, 1..0 select a team, Esc options.

**Touch** (the original is a mouse game; this is the engine's touch scheme):

| Gesture | Does |
|---|---|
| Tap | Select, or give an order with a selection (a left-click) |
| Double tap a unit | Select every unit of that type on screen |
| Drag with one finger | Scroll the map |
| Drag with two fingers | Scroll the map |
| Hold (a ring fills at the finger), then drag | Draw a selection box |
| Hold, then lift | Cancel the armed mode (sell, repair, placement, superweapon target). It never deselects. On a cameo: hold / cancel the build. On a team button: create the team. |
| Two-finger tap | Right-click: cancel a mode, else deselect everything |
| Hold on a cameo or a sidebar button | Show its tooltip |
| Drag on the radar | Scroll the view with the finger |
| Swipe up / down on the cameo strip | Scroll the build list |

- A short hint with these gestures shows for 12 seconds when you get control.
- The **command column** sits on the left edge of the battlefield. It is
  closed by default: tap **CMD** to open it. It closes itself after a command
  or after 6 seconds. It holds:
  - **+SEL**: taps add to the selection;
  - **FIRE**: force-fire the next order;
  - **MOVE**: force-move the next order;
  - **STOP**, **GRD**, **SCAT**, **DPLY**;
  - **BRF**: the mission briefing (campaign; tap the map to close it);
  - **1**..**5**: tap to select the team, hold to create it.
  While +SEL, FIRE or MOVE is armed, the closed **CMD** button shows it.
- Sell, repair, placement and superweapon targeting show a banner at the
  bottom of the battlefield. Tap **X** on the banner (or hold anywhere) to
  cancel the mode.
- To place a building, tap once to see where it goes, then tap the same spot
  again to place it.
- During a mission's intro the game ignores input (as the original does) until
  the intro ends; Back still opens the options menu.
- Back opens the options menu in a game, goes back on menu pages, and asks to
  exit on the main menu.
- The engine build (for example `ENGINE 0.9.0+g1a2b3c4`) is shown in the
  bottom-left corner of the main menu and as the app's version name.

## 5. Troubleshooting

| Symptom | Fix |
|---|---|
| **GAME FILES NEEDED** page | Copy or import the listed files. File names are case-sensitive on Android; the importer fixes the case for you. |
| "COULD NOT START THE GAME: THE GAME FILE X IS MISSING" | An optional file needed by that mode (for example a campaign's maps) is missing. Add it and try again. |
| A save shows "Damaged" | The file failed its checksum (format v10). It cannot be loaded. |
| A save shows "Wrong version" | It was written by another engine version. |
| Android: logs | `adb logcat -s SDL SDL/APP RA2Setup` shows the asset folder, `[load]` timings and `[lifecycle]` events. |
