# RC-5 — How the game's INI reader treats ';' (INIClass::Load)

Binary: `gamemd.exe`; Ghidra private project `D:\ra2-re\fixC`, scripts
`fixC_1.py`/`fixC_2.py`, dumps `D:\ra2-re\fixCout\ini_00525790.c`, `c2.log`.

`FUN_00525790` is `INIClass::Load(stream, merge)` — the only function in the
binary that pushes `';'` (0x3B) to `strchr` and also tests `'['`/`'='`.
Per entry line (read with `FUN_0065d000`, 0x200-byte buffer) [CONFIRMED]:

1. Skip leading bytes `<= 0x20`; a line starting `'['` with a `']'`
   (`PUSH 0x5d; CALL strchr` @`0x525bda`) ends the section.
2. `p = strchr(line, ';'); if (p) { *p = 0; strtrim(line); }` — **the comment
   is cut at the first ';' anywhere on the line**, then the remainder trimmed.
3. Skip the line if it is empty, starts with `';'`, starts with `'='`, or has no
   `'='`.
4. `*eq = 0`; `strtrim(key)`; **skip if the key is empty**; `strtrim(value)`;
   **skip if the value is empty**.
5. Store (key CRC → entry).

Consequences the clone now reproduces in `src/ini.cpp` (`Ini::parse`):

- `Teleporter=yes;`, `Primary=Vulcan2 ;comment`, `Naval=yes ;GS`,
  `Locomotor={...} ;Jumpjet` all read the bare value.
- `Key=` (empty) and `Key=;comment` are not entries at all, so they never
  shadow an earlier value of the same key.
- `'#'` is NOT a comment character in the game. The clone keeps its legacy
  "`#` at line start = comment" rule (no RA2 data file has such a line that
  the game would read as an entry), documented here as the one deviation.

Bool parsing (`FUN_0050f410`, options.md §3, CONFIRMED): `toupper(first char)`
in `{1,T,Y}` → true, `{0,F,N}` → false, anything else → the caller's default.
`Rules`' `is_truthy`/`section_flag` now follow this rule (they matched only the
exact words "yes"/"true").
