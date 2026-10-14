# Install and run (players)

This project is an **engine only**. It plays Command & Conquer: Red Alert 2
using the game files from **your own copy** of the game. No game data is
included or downloadable here. It is not affiliated with or endorsed by
Electronic Arts. See the "Legal" section of the main README.

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

1. Unzip the release zip (`ra2engine-<version>-win64.zip`) into any folder.
   It holds `ra2game.exe`, `ra2engine.exe`, `SDL2.dll`, this file, `LICENSE`
   and `THIRD_PARTY_NOTICES.md`; nothing else needs installing (the C++
   runtime is built into the exes; Windows 10 or later, or Windows 7/8.1 with
   the Universal C Runtime update). Keep `SDL2.dll` next to the exes.
   To build the zip yourself from a checkout (MinGW-w64 g++, CMake, Ninja):
   `pwsh scripts/package-win.ps1 -Configure` writes it to `dist/` and checks
   that it starts from a clean folder.
2. Start `ra2game.exe` (or `ra2engine.exe`). It looks for the game files in this order:
   1. `--data <folder>` on the command line;
   2. the `RA2_INSTALL_DIR` environment variable;
   3. `RA2.INI` `[Engine] DataDir=` in `%APPDATA%\ra2engine\RA2\`;
   4. the folder that holds `ra2engine.exe`;
   5. the retail install's registry key
      (`HKLM\SOFTWARE\Westwood\Red Alert 2`, `InstallPath`);
   6. `D:\Red Alert 2`.
3. If the files are not found, the **GAME FILES NEEDED** page lets you choose
   the folder with **BROWSE**. The choice is saved to `RA2.INI` `[Engine] DataDir`.

Other options: `--no-audio`, `--fps` (the perf overlay, section 5),
`--record <file>` / `--replay <file>` (section 6), `--version`, `--help`.
Settings (`RA2.INI`) and saved games live in `%APPDATA%\ra2engine\RA2\`.

`ra2game.exe` is the same engine built as a Windows GUI program: a
double-click opens no console window, and its log goes to
`%APPDATA%\ra2engine\RA2\ra2engine.log`. `ra2engine.exe` is the console
build (the log prints in the terminal).

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

**Adding or replacing files later.** Put them in `Download/RA2` and import
again: long-press the app icon and choose **Import game files**, or in the
game open **Options**, then **Game Files**. Saves and settings are kept; you
never need to clear the app's data. Besides the files in the table, the
importer copies patch archives (`expand##.mix`, `ecache##.mix`,
`elocal##.mix`), `thememd.mix`, and movie files (`MOVIES##.MIX`,
`MOVMD##.MIX`, and loose `*.bik`, also from a `Movies` sub-folder).

Settings (`RA2.INI`) and saves are in the app's internal storage. They are
included in Android backups; the game files are not.

If Android closes the app in the background (low memory), or restarts it
(for example after a switch to gesture navigation or a theme change), the
game was saved to `AUTOSAVE`. The next launch shows **RESUME GAME?** with
the mission name and play time; **MAIN MENU** asks before it discards that
game. If you won a mission and the app was closed on the results page or
on the next loading screen, the next launch offers **CONTINUE CAMPAIGN?**.

**Updates.** Install the new APK over the old one: the game files and
settings are kept. Saved games do not carry over between engine versions
yet (they are listed as "Older version"), so finish a mission first.

## 4. Controls

**Mouse (the original's scheme):**
- Left-click selects, and gives an order with a selection.
- Right-click deselects or cancels.
- Drag to box-select.
- Ctrl forces fire. Alt forces a move. Shift adds to the selection.
- Hotkeys come from the install's `Keyboard.ini` (`KeyboardMD.ini` when it is
  missing): S stop, G guard, X scatter, D deploy, Ctrl+1..0 create a team,
  1..0 select a team, P select every combatant on screen, U / Y step through
  the health / veterancy groups, F follow the selected unit, C all infantry
  cheer, Space jump to the latest radar event, Ctrl+C screenshot
  (`SCRN0000.png` in the settings folder), Esc options.
- **Changing the keys.** Options -> **Keyboard** (main menu), or in a game
  Esc -> Game Controls -> **Keyboard**. Pick a category, a command, click the
  "Current Hotkey" box, press the new key (with Ctrl / Alt / Shift if you
  want), then **Assign**. A key that another command used moves to the new
  command ("Currently assigned to" shows which). **Reset All** restores the
  original keys. **Back** saves to `Keyboard.ini` in the settings folder
  (`%APPDATA%\ra2engine\RA2\` on Windows); the install folder is never
  written. Beacons, alliances, taunts and Page User only work in a network
  game in the original, so they do nothing here.

**Touch** (the original is a mouse game; this is the engine's touch scheme):

| Gesture | Does |
|---|---|
| Tap | Select, or give an order with a selection (a left-click) |
| Tap a building's picture | Picks that building, also its upper part (a tower's head, a lab's dome), even with a soldier standing next to it |
| Tap one of your selected units (two or more selected) | Moves the selection there (it does not re-select that one unit). Tap a unit that is NOT selected to select it. |
| Double tap a unit | Select every unit of that type on screen. It never deploys. |
| Tap the only selected unit (wait half a second) | Deploy / unload it (or use **DPLY**) |
| Drag with one finger | Scroll the map (with **BOX** on: draw a selection box) |
| Drag with two fingers | Scroll the map |
| Hold (a ring fills at the finger), then drag | Draw a selection box |
| Hold, then lift | Cancel the armed mode (sell, repair, placement, superweapon target). It never deselects. On a team button: create the team. |
| Hold on a cameo | Show its name and price above the finger. It does not pause the build. If the cameo is building, a small menu opens beside it: **PAUSE** / **RESUME**, **CANCEL** (one), **ALL** (every queued copy). Tap elsewhere to close it. |
| Two-finger tap | Right-click: cancel a mode, else deselect everything |
| Hold on a sidebar button | Show its tooltip |
| Tap the radar | Look there (it never moves the selection; arm **MOVE** first to send units there) |
| Drag on the radar | Scroll the view with the finger |
| Swipe up / down on the cameo strip | Scroll the build list |

- A short hint with these gestures shows for 12 seconds when you get control.
- The **command column** sits on the left edge of the battlefield. It is
  closed by default: tap **CMD** to open it. It closes itself after a command
  or after 6 seconds. It holds:
  - **+SEL**: taps add to the selection;
  - **FIRE**: force-fire the next order;
  - **MOVE**: force-move the next order (also a radar tap);
  - **STOP**, **GRD**, **SCAT**, **DPLY**;
  - **BOX**: stays on until tapped again; while on, a one-finger drag draws a
    selection box at once (two fingers still scroll);
  - **BRF**: the mission briefing (campaign; tap the map to close it);
  - **1**..**5**: tap to select the team, hold to create it;
  - **MORE**: the second page -- **TYPE** (every unit of the selected types),
    **HOME** (centre on the base), **EVT** (jump to the latest radar event),
    **NEXT** / **PREV** (step through your units), **FOLW** (follow the
    selected unit), **6**..**0** (teams 6-10), **V1**..**V3** (view bookmarks:
    hold to save the view, tap to jump back), **BACK**.
  While +SEL, FIRE, MOVE or BOX is armed, the closed **CMD** button shows it.
- Sell, repair, placement and superweapon targeting show a banner at the
  bottom of the battlefield. Tap **X** on the banner (or hold anywhere) to
  cancel the mode.
- **Sell / repair**: tap a building to mark it (its foundation lights up and
  the banner names it), then tap it again to sell / repair it. Tapping another
  building moves the mark.
- **Superweapons**: tap the target to see the area it hits, then tap the same
  spot again to fire.
- To place a building, tap once to see where it goes, then tap the same spot
  again to place it.
- Messages, objectives and tooltips are drawn 1.5x larger on a phone.
- With gesture navigation, a swipe in from the left edge next to the command
  column, or from the right edge along the build list, stays in the game
  (Android 10+); a swipe elsewhere on the edge is the system's.
- During a mission's intro the game ignores input (as the original does) until
  the intro ends; Back still opens the options menu.
- Back opens the options menu in a game, goes back on menu pages, and asks to
  exit on the main menu.
- The engine build (for example `ENGINE 0.9.0+g1a2b3c4`) is shown in the
  bottom-left corner of the main menu and as the app's version name.
  **Options > About** shows the version, the licence and the third-party
  notices.
- Back on the mission results page = Continue (Main Menu after a lost
  mission); on a mission loading screen Back returns to the main menu.

### Movies

The campaign movies (each mission's intro, briefing, win and lose movies, and
the in-mission ones) play when the movie files exist. On most installs
`MOVIES01.MIX` / `MOVIES02.MIX` / `MOVMD03.MIX` are tiny placeholder files, so
the movies are skipped and the game goes straight on. To add your own copies,
put the Bink files (`A01_f00e.bik`, ...) in any of:

- a folder named by the environment variable `RA2_MOVIE_DIR`;
- the game folder, or a `Movies` folder inside it;
- `Movies` inside the settings folder (`%APPDATA%\ra2engine\RA2\Movies\`);
- or replace the placeholder `MOVIES0x.MIX` / `MOVMD03.MIX` with real ones.

On Android, put the movie files next to the game files and import them
(section 3): the importer copies them where the engine looks.

With movie files present, **Movies & Credits > Play Movies** lists every
campaign movie it finds (by mission) and plays the one you pick. Winning the
last mission of a campaign plays the campaign's final movie (when present),
then the credits, then returns to the main menu.

A movie's sound plays with it when the file carries a Bink Audio track (RDFT or
DCT, mono or stereo). The music pauses for the movie and resumes after it. The
menu's own CRT movie has no sound track, so it is silent.

A key or a click skips a movie.

## 5. Measuring performance (please report real-phone numbers)

The engine has a built-in **frame-rate / perf overlay**. It shows, averaged
over the last second, the rendered frames per second (FPS), the logic rate
(LOGIC: 15/s is full speed at the default game speed; lower means the
simulation runs slow) and the milliseconds per frame spent updating (UPD),
drawing (REN) and presenting (PRES), plus the worst frame (MAX).

Turn it on:
- **In a game**: tap the screen with **three fingers** at once (phone), or
  press **Ctrl+Shift+F** (keyboard). Do the same again to turn it off.
- **Desktop**: start with `--fps`, or set `RA2_PERF=1`, or put
  `ShowFPS=yes` under `[Engine]` in `RA2.INI`.
- **Android, from a PC**: `adb shell setprop debug.ra2.perf 1`, then start
  the app (the setting lasts until the phone restarts).

While it is on, the same numbers are written to the log every 5 seconds.

How to measure on a real phone and send the numbers:
1. Enable USB debugging on the phone (Settings > About phone > tap Build
   number 7 times; then Developer options > USB debugging) and connect it.
2. On the PC: `adb shell setprop debug.ra2.perf 1`
3. Start the game and play the scene you want to measure (for example the
   first minute of the Allied mission 1, and a busy skirmish).
4. Capture the log: `adb logcat -s SDL/APP | Select-String perf,load`
   (PowerShell) or `adb logcat -s SDL/APP | grep -E "perf|load"`.
   Lines look like
   `[perf] fps=58.9 logic=15.0/s input=0.05ms update=3.10ms render=6.40ms present=6.80ms worst=21.4ms canvas=1172x548`
   and `[load] LOADING done in 5928 ms`.
5. Send the `[perf]` and `[load]` lines with the phone model.

A screenshot with the overlay on (three-finger tap) is just as good.

Optional system-side numbers: `adb shell dumpsys gfxinfo org.ra2.engine`
(frame times) and `adb shell dumpsys meminfo org.ra2.engine` (memory).

## 5b. Network play (LAN and Internet)

**LAN.** All machines on the same network. Host: *Network* -> *Host Game*
(the lobby shows the address others can type, e.g. `192.168.1.20:50020`).
Others: *Network*, pick the game from the list, or *Join by IP*. Choose
country / colour / team / start, joiners press *Ready*, the host presses
*Start Game*. Ports: UDP 50010 (LAN game list, broadcast) and UDP 50020
(the game; `RA2.INI [Net] LanPort=`). Allow `ra2engine.exe` / `ra2game.exe`
through the Windows firewall for private networks when Windows asks.

**Internet.** One person runs `ra2lobbyd` on a server ([SERVER.md](SERVER.md)).
Everyone types its address under *Internet* -> *Connect*; the host creates a
room (public, or private with a code), the others join from the list or with
the code. Game traffic is relayed by the server's CnCNet-V3 tunnel (UDP 50001
on the server), so nobody needs port forwarding.

**In the game.** Enter (desktop) or the *Chat* button (touch) opens the chat
line. The options menu does not pause the game; saving / loading is disabled.
*Abort* leaves cleanly (the others see "has left the game"). If a player's
connection stops, everyone waits; a dialog names the player with a countdown
and offers *Drop player* / *Quit game*. A dropped or leaving player's base is
destroyed, or taken over by the computer when the host ticked *AI takes over*.

**Limits.** Windows and Android cannot play the same match yet (they would go
out of sync until the deterministic-maths work, net L0, is merged); the host
refuses a player on the other platform unless *Cross-platform (exp.)* is on.
A player whose app was closed or whose network changed cannot rejoin a running
match yet. Every player needs the same game files and engine version.

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| **GAME FILES NEEDED** page | Copy or import the listed files. File names are case-sensitive on Android; the importer fixes the case for you. |
| "COULD NOT START THE GAME: THE GAME FILE X IS MISSING" | An optional file needed by that mode (for example a campaign's maps) is missing. Add it and try again. |
| A save shows "Damaged" | The file failed its checksum. It cannot be loaded. |
| The window is 1280x720 although `RA2.INI` asked for more | `ScreenWidth` / `ScreenHeight` did not fit the display (or the graphics driver's limit); the engine fell back and rewrote `[Video]`. Pick a size in Options. |
| A save shows "Older version" / "Newer version" | It was written by another engine version and cannot be loaded (saves do not carry over between versions yet). |
| Android: logs | `adb logcat -s SDL SDL/APP RA2Setup` shows the asset folder, `[load]` timings and `[lifecycle]` events. |
| Network: the game is not in the LAN list | Use *Join by IP* with the host's address. Check that both are on the same network and the firewall allows the game (UDP 50010 / 50020). Guest / hotel Wi-Fi often blocks devices from seeing each other. |
| Network: "Your game version / game data differs from the host's" | Everyone needs the same engine version and the same `rulesmd.ini` (the same game files). |
| Network: "The host plays on another platform" | Windows and Android cannot play together yet; the host can tick *Cross-platform (exp.)* to try anyway (expect "Out of sync"). |
| Network: "Scenarios do not match" at the start | The players' game files (rules / maps) differ. |
| Network: "Out of sync!" | The games stopped agreeing. `Saved Games/DESYNC_F<frame>_H<house>.SAV` and `.TXT` were written on every machine; please send all of them (the TXT files name the part of the game that differed). |
| Network: "Waiting for players..." for long | That player's connection stopped (or the phone app is in the background: then a 90 s countdown shows). After the countdown use *Drop player*, or wait for the automatic drop. |
| Internet: cannot connect to the server | Check the address and that the server's TCP 50000 / UDP 50001 are open ([SERVER.md](SERVER.md)). |
| Windows: logs of `ra2game.exe` | `%APPDATA%\ra2engine\RA2\ra2engine.log` (rewritten on each start). |
| "COULD NOT LOAD THAT SAVED GAME" / "Error loading game" | The save is from another engine version, or its contents point outside the game (the loader range-checks every unit and building reference). |

**Reporting a bug with a recording (desktop).** Start the game with
`ra2engine.exe --record bug.rec`, play until the problem shows, then quit.
`bug.rec` is a small text file with every click, key and frame time of the
session. Send it with the engine version (`ra2engine --version`).
`ra2engine.exe --replay bug.rec` plays the session back (`--hidden` runs it
without a window); it needs the same game files and the same `RA2.INI`
settings (game speed, difficulty). Both runs print a
`[replay] end ... probe_hash=` line; equal hashes mean the replay reproduced
the session.
