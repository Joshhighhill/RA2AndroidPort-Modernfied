# Original Yuri's Revenge menu versus this port

## Confirmed mismatch in the source

`docs/re/mainmenu_true.md` explicitly scopes its reconstruction to original
RA2 (`game.exe`, `ra2.mix`) and says ra2md/YR is ignored. The renderer in
`engine/main.cpp::draw_shell` uses a logical 640x480 composition.
`load_menu_art` opens these classic assets directly:

- `ra2.mix/neutral.mix`: mnscrnl/s, sdbtnanm, sdwrnanm and campaign chrome.
- `ra2.mix/local.mix`: logo.pcx, title.pcx, button/control skins and flags.
- `ra2.mix/cache.mix`: shell palettes.
- `language.mix`: ra2ts_s.bik menu movie.

Importing ra2md.mix does not switch those explicit reads to an expansion
shell. Bigger Android labels only improve readability; they do not restore
the original YR composition or movie.

## References found, 2026-10-21

- Original Westwood/EA Yuri's Revenge manual, archived PDF: the Display
  Options section says menus always use 800x600. Search indexing exposed
  that passage; direct retrieval failed in this session, so this is not a
  page-by-page visual verification.
  https://gamepedia.cursecdn.com/cnc_gamepedia_en/8/8e/Red_Alert_2_-_Yuri%27s_Revenge_-_English.pdf
- Archived original Windows main-menu screenshot, identified as 800x600:
  https://www.mobygames.com/game/5120/command-conquer-yuris-revenge/screenshots/windows/22940/
  The page returned HTTP 403 on direct access. Do not claim its pixels were
  visually inspected in this session.
- Modder discussion of the original menu movie and shared RA2 shell buttons:
  https://ppmforums.com/topic-17247/yr-main-menu-graphics/
  Search preview describes the animated radar and sd*.shp buttons; direct
  retrieval timed out. This is a lead, not a verified expansion asset list.

## Restoration approach

1. Inventory the tester's original expansion archives and language archives.
   Log file/archive provenance, dimensions, palettes and movie metadata.
2. Establish a YR shell variant explicitly, with an 800x600 logical canvas,
   its matching art/movie/palettes and button coordinates. Retain the classic
   shell when the needed expansion assets are unavailable.
3. Preserve the original artwork's aspect ratio. Scale touch hit areas and
   legible labels separately from tactical map resolution.
4. Compare an actual rendered YR menu against an accessible original
   screenshot before calling it faithful. Check English and the tester's
   game-file language, long labels and the POCO landscape aspect ratio.

No replacement copyrighted menu art is bundled or generated. Exact YR movie
names and button coordinates still need asset-level verification. This note
does not claim the original YR menu has already been implemented.
