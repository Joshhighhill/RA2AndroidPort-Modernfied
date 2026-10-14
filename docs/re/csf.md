# CSF string table — reverse-engineering notes

(Method: `docs/re/workflow.md`. Hunt scripts: `D:/ra2-re/scripts/csf_dump.py`
(magic + filename byte search), `csf_lookup.py` (reference-walk from the
runtime globals the loader populates), `csf_cmp.py` (targeted decompile of
the label comparator). Raw byte search for the ` FSC`/` LBL`/` RTS`/`WRTS`
magics (their ASCII bytes equal their little-endian immediate encodings) plus
the `ra2.csf`/`ra2md.csf`/`.csf` filename strings, over every initialized
block; hits → containing/referencing functions → decompiled to
`D:/ra2-re/out/csf*.c`. Nothing under `D:/ra2-re` is committed.)

## File format (byte-verified against the install's ra2.csf, 491,826 bytes)

File header (24 bytes, packed, little-endian):

| offset | type    | field       | ra2assets read              |
|-------:|---------|-------------|------------------------------|
| 0      | char[4] | magic       | must be ` FSC` (20 46 53 43)|
| 4      | u32     | version     | 3 here (ignored)            |
| 8      | u32     | num_labels  | label-record count (4562)   |
| 12     | u32     | num_strings | (ignored)                   |
| 16     | u32     | unused      | (ignored)                   |
| 20     | u32     | language    | 0 here (ignored)            |

Then `num_labels` sequential label records — ` LBL`, `u32 num_values`,
`u32 label_len`, `label_len` ASCII bytes (no NUL) — each followed by
`num_values` value records: ` RTS` (or `WRTS`), `u32 val_len` in UTF-16
units, `val_len*2` bytes UTF-16LE with every byte XOR 0xFF (= bitwise NOT
per 16-bit unit); `WRTS` adds `u32 extra_len` + `extra_len` ASCII bytes.
All 4562 labels of the real file walk cleanly with exactly this structure
(independent Python decode, 2026-07-09).

## Located functions

The magic search landed directly in `.text` on the first try (all four
magics hit inside two adjacent functions), and a follow-up reference-walk
from the runtime globals those two functions populate (`Labels`,
`LabelCount`, `Values`, `ExtraValues`, `IsLoaded`) turned up the lookup
routine and its comparator. All five identify cleanly against YRpp's
`StringTable.h` (`CSFHeader`/`CSFLabel`/`StringTable` class), which was read
*after* location, purely to name/cross-check — the addresses below are
found-by-content in this build, not copied from YRpp (see "Build drift"
below; YRpp's published `LoadFile`/`ReadFile`/`LoadString` addresses do not
match this binary).

| Function | Role | Evidence |
|----------|------|----------|
| `FUN_00733770` (`StringTable::LoadFile`) | opens `<name>.csf`, validates the 24-byte header, allocates `Labels[]`/`Values[]`/`ExtraValues[]`, then sorts `Labels[]` | contains the ` FSC` magic compare; strips the caller's extension and appends the `.csf` string at `0084476c`; reads exactly `0x18` bytes and checks that the first dword equals `0x43534620` |
| `FUN_00733a60` (`StringTable::ReadFile`) | walks ` LBL` label records and their ` RTS`/`WRTS` value records into the arrays above | contains the ` LBL` (`0x4c424c20`), ` RTS` (`0x53545220`) and `WRTS` (`0x53545257`) dword compares; the 16-bit inversion loop |
| `FUN_00733f30` (`StringTable::LoadString`) | label → string lookup | `bsearch`-shapes a call `FUN_007c7c25(label, Labels, LabelCount, 0x28, FUN_007c7b20)`; on miss allocates a `"MISSING: <label>"` placeholder (string `s____NO_STRING__…` logged) |
| `FUN_007c7b20` | shared label comparator (both sort and lookup) | CRT-`_stricmp`-shaped: byte-scan, and on a mismatch **case-folds both bytes** before deciding order |
| `FUN_007c7948` / `FUN_007c7c25` | generic `qsort`/`bsearch` (CRT-style, `(base,count,size,cmp)` / `(key,base,count,size,cmp)`) | called from `LoadFile` (sort) and `LoadString` (search) with the *same* comparator pointer `FUN_007c7b20` |
| `FUN_00733e00` (`StringTable::Unload`) | frees `Labels`/`Values`/`ExtraValues` and any leaked `"MISSING:"` placeholders | references all four runtime globals; corroborates their roles |

Runtime globals (this build): `Labels=0xb1bed4`, `LabelCount=0xb1becc`,
`Values=0xb1bed8`, `ExtraValues=0xb1bedc`, `IsLoaded=0xb1bee0`,
`MaxLabelLen=0xb1beb8` — all match YRpp's `StringTable` `DEFINE_REFERENCE`
*roles* (its absolute addresses are for a different 1.001 build and land a
few hundred bytes off; see "Build drift").

## Game behavior confirmed

- **Header validation:** the loader checks **only the magic**. It reads
  exactly `0x18` (24) bytes into a stack header and requires the magic
  dword (offset 0) to equal `0x43534620` (` FSC`);
  no other magic value is accepted, and a mismatch aborts the load (no
  partial label table is built). `version` (offset 4) is read
  but is **not** compared against 3 or rejected for any value. Its *only*
  use is to compute the global language at `0x8446A8`: it is set to 0 when
  version < 2, and otherwise to the header's `language` field (offset 20).
  The engine does this without a branch, by building an all-zeros or
  all-ones mask from the `version < 2` comparison and ANDing it with
  `language`.

  This is bit-for-bit what YRpp's own `CSFHeader` comment says: `Language;
  //CSF_LANG_*, forced to US if CSFVersion < 2`. `num_strings` (offset 12) is
  stored and used purely as the allocation count for the flat `Values[]`/
  `ExtraValues[]` arrays; `num_labels` (offset 8) sizes the `Labels[]` array.
  The `unused` dword (offset 16) is read into a stack temp and never
  referenced again — genuinely dead, confirming its name. One quirk worth
  recording: the record-walk loop is **not** bounded by `num_labels` — it
  keeps consuming ` LBL` records until the magic check fails or the file
  runs out, using `num_labels` only to size the allocation up front (an
  engine-side risk if a header ever undercounts, irrelevant to a
  well-formed real file).

- **Value decode (bit inversion):** confirmed exactly. After reading
  `val_len*2` raw bytes, the loop inverts every 16-bit code unit in place,
  replacing each UTF-16LE unit with its 16-bit bitwise NOT.

  `~x` on a 16-bit value flips all 16 bits, which is exactly XOR 0xFF on
  each of its two bytes — matches `src/csf.cpp`'s byte-wise `^ 0xFF`
  precisely. `WRTS`'s extra `u32 extra_len` + ASCII bytes are read and
  copied verbatim into `ExtraValues[]`, also confirmed. One divergence
  worth recording for anyone chasing pixel/text-perfect UI parity (not a
  file-format concern, so it does not affect the decoder): immediately
  after inversion the engine runs an in-place whitespace-normalization pass
  over the decoded text (collapses runs of space/tab/newline, trims
  edges) before allocating the final wide string. `ra2assets` intentionally
  returns the raw decoded text and should **not** replicate this — it is
  display-side massaging, not part of the CSF value encoding.

- **Label case handling: confirmed case-insensitive.** This settles the
  task's key open question directly, and with a mechanism more interesting
  than a simple uppercase-at-insert: the label bytes are copied into
  `Labels[].Name` **verbatim** (a plain memcpy, no `toupper`/`_strupr`/
  `and 0xDF` at insert), but immediately after the whole file is read,
  `LoadFile` **sorts** `Labels[]` with a CRT-shaped `qsort` (`FUN_007c7948`),
  and `LoadString` looks a label up with a CRT-shaped `bsearch`
  (`FUN_007c7c25`) — both using the **same comparator**, `FUN_007c7b20`.
  That comparator is a `_stricmp`-equivalent: on a byte mismatch it
  case-folds both bytes before deciding order. For each byte `x` it computes
  `x + 0xbf` (≡ `x - 0x41`, as a byte); if that lands in `[0,0x1a)` — i.e.
  `x` was `'A'..'Z'` — it adds `0x20` before adding `0x41` back, i.e. folds
  upper→lower; otherwise the byte is unchanged. The engine does this without
  a branch, by turning the range test into a `0x20`-or-0 mask. It also has a second, locale-aware/
  thread-safe path (`FUN_007c9e63`) guarded by a recursion counter — the
  classic dual-path shape of the MSVC CRT's own `_stricmp`. Net effect:
  label matching is case-insensitive at both build (sort) and query
  (bsearch) time, even though the stored bytes keep the file's original
  case. This is functionally equivalent to `ra2assets`' current
  uppercase-normalize-at-insert-and-lookup default (both approaches make
  `"Some_Label"` and `"SOME_LABEL"` match), just implemented the opposite
  way (fold-in-comparator vs. normalize-in-storage) — **no divergence in
  observable behavior**, so the existing default is confirmed correct
  rather than merely retained on UI-file evidence.

### Build drift (per `workflow.md` §4)

YRpp publishes `StringTable::LoadFile`/`ReadFile`/`LoadString` at
`0x7346A0`/`0x734990`/`0x734E60`. In this binary the actual functions are at
`0x733770`/`0x733a60`/`0x733f30` — a consistent few-hundred-byte-earlier
offset, the same kind of build-specific drift `workflow.md` already
documented for `CRCEngine`/`MixFileClass`/the SHP methods. Located by
content (magic bytes, then reference-walk, then layout match against
`CSFHeader`/`CSFLabel`), not by trusting the published address.

## Confidence

| Item | Confidence | Why |
|------|:----------:|-----|
| Header layout (`magic,version,num_labels,num_strings,unused,language`) | **High** | decompiled 24-byte read + `magic==0x43534620` check match the format table exactly; independently corroborated by YRpp's `CSFHeader` struct, including the non-obvious `version<2 → language=0` gate matching bit-for-bit |
| Label/value record walk (` LBL`/` RTS`/`WRTS`, field order) | **High** | decompiled loop's field reads and the `0x28`-byte `CSFLabel` offsets (`Name[0x20]`, `NumValues@0x20`, `FirstValueIndex@0x24`) match YRpp's `CSFLabel` exactly |
| Value bit-inversion | **High** | a literal 16-bit bitwise NOT in the decompiled loop, matches `src/csf.cpp`'s byte-wise XOR 0xFF construct |
| Label case handling (case-insensitive lookup) | **High** | not inferred — directly decompiled: shared `qsort`+`bsearch` comparator `FUN_007c7b20` is a `_stricmp`-shaped case-fold; this is the mechanism, not a guess |
| Exact function *addresses* as "the" canonical YR 1.001 addresses | **Medium** | correct for *this* installed binary only; YRpp's published addresses for the same methods drift by ~0x1000, confirming `workflow.md`'s general drift warning applies here too |

All of the above is corroborating/explanatory RE — the Task 4 real-asset
oracle remains the actual correctness gate for the byte-level decoder.
