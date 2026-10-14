# Ghidra RE workflow — `decompile → reimplement → verify`

This is the reusable machine the asset pipeline was built on and that later
phases should follow. It documents how to import `gamemd.exe` into Ghidra, how
to use YRpp/Ares as a symbol/layout map, the confidence loop, and — most
importantly — the failure modes we hit so the next person does not re-learn them.

The guiding principle: **we already have a known-good answer.** Tasks 1–9
produced an oracle-verified reimplementation (Blowfish, Westwood key derivation,
MIX reader with CRC32 `mix_id`, 6-bit palette `expand6`, TS/RA2 SHP decoder).
RE here is *not* discovery — it is **calibration**: locate each algorithm in the
real binary, confirm it matches our code, rate confidence, and record any
divergence. A documented, honestly-rated *failed* hunt is a valid result.

---

## 0. Toolchain (confirmed 2026-07-08)

| Component | Value |
|-----------|-------|
| Ghidra    | 11.2.1 PUBLIC — `C:\work\DentrixDocumentsExtractor\ghidra_11.2.1_PUBLIC\` (not on PATH) |
| JDK       | 21.0.8 LTS — `C:\Program Files\Java\jdk-21` (JDK 11 also present; **11.2.1 refuses JDK 11**) |
| Target    | `D:\Red Alert 2\gamemd.exe`, SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`, 4,808,976 bytes (YR 1.001) |
| Project   | `D:\ra2-re\` project `ra2` — **outside the repo** (legal: no binary/project/dumps committed) |
| Refs      | `D:\ra2-re\reference\{YRpp,Ares,xcc,OpenRA}` — read for layouts, never vendored into the repo |

**Everything RE lives under `D:\ra2-re\`, never in `D:\ra2-android\`.** The repo
gets prose + short illustrative snippets only.

### Launching headless (what actually worked)

`analyzeHeadless.bat` is a Windows batch file. From Git Bash it launches fine as
long as `JAVA_HOME` points at JDK 21 first. Redirect stdin from `/dev/null` so a
trailing `pause` (only appears on the usage/error path) can never hang a
background run.

```bash
export JAVA_HOME="/c/Program Files/Java/jdk-21"
GHIDRA="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC/support/analyzeHeadless.bat"
# sanity: prints usage + exits 1 (JVM launched OK)
"$GHIDRA" < /dev/null
```

## 1. Import + auto-analyze (once, ~5 min wall)

```bash
"$GHIDRA" "D:/ra2-re" ra2 -import "D:/Red Alert 2/gamemd.exe" \
  -analysisTimeoutPerFile 3600 -log "D:/ra2-re/out/analyze.log" < /dev/null
```

For a 4.8 MB PE this took **~4 min 45 s wall / 252 s of analyzer work** on this
machine (`REPORT: Analysis succeeded`). Run it in the background and wait — do
not kill it. No PDB is present (`Skipping PDB processing`), so every function
comes out as `FUN_<addr>`; naming is on us. Image base is `0x00400000`, so YRpp
absolute addresses map 1:1 to Ghidra addresses **when they are correct for this
build** (see §4).

## 2. Iterate with headless post-scripts (seconds each)

Re-opening the already-analyzed program with `-process` + `-noanalysis` is fast
(~30–60 s to load). Drive extraction with a Jython post-script that decompiles
functions and writes C to `D:\ra2-re\out\*.c`; read those into the docs.

```bash
"$GHIDRA" "D:/ra2-re" ra2 -process gamemd.exe -noanalysis \
  -scriptPath "D:/ra2-re/scripts" -postScript dump.py < /dev/null
```

Skeleton (globals `currentProgram`, `getFunctionContaining`, `getInt`, etc. are
injected into the script scope by `GhidraScript`):

```python
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
decomp = DecompInterface(); decomp.openProgram(currentProgram)
fn = getFunctionContaining(toAddr(0x4A1CF0))
res = decomp.decompileFunction(fn, 60, ConsoleTaskMonitor())
open("D:/ra2-re/out/crc.c","w").write(res.getDecompiledFunction().getC())
```

Location techniques that worked, in order of reliability:

1. **Constant tables.** The most reliable anchor of all. The reflected CRC32
   table (`0x00000000, 0x77073096, 0xEE0E612C, …`) is unmistakable: search
   memory for the 4-byte LE image of `Table[1]=0x77073096`, land on the table,
   then `getReferencesTo` the table address → the four functions that use it are
   the CRC engine. This nailed `mix_id` in one shot.
2. **Algorithm shape.** Decompile a candidate and match the *structure* against
   our code (e.g. `ret = (ret>>8) ^ Table[(ret&0xFF)^byte]`, or "read dword; if
   low word == 0 it's the new format"). This is what confirmed the MIX parser.
3. **Region enumeration.** `getFunctionManager().getFunctions(startAddr, True)`
   over an address window, sorted by size/call-count, to spot the big/hot
   function in a subsystem (found the `MixFileClass` ctor and the SHP class).
4. **Reference (byte) search.** `Memory.getBytes` a block into a Python `bytes`
   and `.find()` the LE pattern (Ghidra's `Memory.findBytes` string-regex form
   was fiddly under Jython 2.7 — the block-read approach is robust).

## 3. Using YRpp / Ares as the map

YRpp (`Phobos-developers/YRpp`) and Ares (`Ares-Developers/Ares`) publish the
**class layouts and method addresses** of the YR binary. Read them to *name*
Ghidra functions and *interpret* struct offsets. The ones we leaned on:

| YRpp header | Gave us |
|-------------|---------|
| `MixFileClass.h` | `struct MixHeaderData{DWORD ID,Offset,Size;}` (= our `MixEntry`); field order `FileName, Blowfish, Encryption, CountFiles, FileSize, FileStartOffset, Headers` |
| `CRC.h` (`CRCEngine`) | byte-wise reflected CRC32 `Memory()`/`String()`, and the `Value()` staging-buffer padding (`Buffer[Index]=Index; Buffer[i]=Buffer[0]`) — exactly `mix_id`'s "count byte then repeat" |
| `FileFormats/SHP.h` | `SHPStruct{Type,Width,Height,Frames}`, `SHPFrame{Left,Top,Width,Height,Flags,…,Offset}`, `HasCompression = Flags & 2`, `IsReference = Type==0xFFFF` |
| `FileSystem.h` | `AllocatePalette`: *"the 6 bit colors converted to 8 bit (not the proper way. how the game does it.)"* → `color.R <<= 2` — the authoritative palette-scaling tell |
| `BasicStructures.h` | `ColorStruct{BYTE R,G,B}`, `BytePalette{ColorStruct[256]}` |

Cross-checks from `xcc` (`get_id`, `decode3`, `palet.cpp`) and OpenRA
(`PackageEntry.HashFilename` CRC32-vs-Classic) confirm the same algorithms our
code already ports.

## 4. LESSONS LEARNED — what Ghidra (and the maps) got wrong

**Published absolute addresses are build-specific and DRIFT.** This was the
biggest trap. YRpp targets a particular YR build; this install is a different
1.001 binary, and code motion shifts addresses non-uniformly — the drift *grows*
with address:

- `CRCEngine` (~`0x4A1xxx`) and `ConvertClass` (~`0x48Exxx`): small drift; the
  YRpp address landed a few dozen bytes *inside* the real function (Ghidra's
  entry a bit earlier).
- `MixFileClass::Offset` YRpp `0x5B4430`: in **this** binary `0x5B43E0` is an
  unrelated *"ModemGuest_Dialog"* handler. Completely wrong subsystem.
- `SHPStruct` methods (`0x69E090/69E500/69E580/69E900`): several resolved to
  `NO_FUNCTION` (address in a gap Ghidra didn't auto-define) or tiny thunks.
- CRC table YRpp `0x81F7B4`: in this binary that offset is RTTI *string* data;
  the real table is `0x81E7C4`.

**Takeaway:** treat YRpp addresses as *hints*, verify by **content** (constant
tables, algorithm shape) and relocate. Use the maps for **layouts and algorithm
shape**, not as ground-truth addresses.

Other MSVC/optimizer artifacts (all expected, all material):

- **`thiscall`/`fastcall`**: `this` shows up as an auto-named first parameter
  with an extra `_00` suffix; ECX/EDX args get reordered (for example
  `FUN_004a1cf0` is typed as a thiscall taking `this`, a byte buffer and a
  length). Some bodies show "unaffected register" placeholders for EBP/ESI
  where a register was passed in — decompiler guesswork, read past it.
- **vtables & inlining**: `SHPStruct::GetPixels`/`HasCompression` are virtual
  dispatches (an indirect call through the vtable slot at `+0x10`, with the
  vtable read from `this-0x14`); the SHP frame *decode is
  inlined into the RLE blitters*, so there is **no standalone decompressor
  function** to diff (see `shp.md`).
- **No string xrefs for some data**: the Westwood public key and `.pal`
  filenames are reached via base-pointer arithmetic, so `getReferencesTo` on the
  string address returned nothing — constant-table anchoring beat string search.
- **Function boundaries**: expect to `createFunction()` or region-scan where
  auto-analysis left gaps.

## 5. The confidence loop (how the loop *closes*)

```
YRpp layout  ─►  locate candidate in gamemd.exe  ─►  decompile
     ▲                                                   │
     │                                                   ▼
 our src (known answer)  ◄── compare shape/constants ── rate H/M/L
     │                                                   │
     └────────────── oracle byte-parity ◄───────────────┘
```

- **High**: constants/tables match bit-for-bit and the control flow maps 1:1 to
  our code (e.g. the CRC table + padding; the MIX header/index parse).
- **Medium**: structure and layout confirmed, but the exact leaf routine is
  inlined/virtual/build-shifted and not disassembled 1:1 (e.g. SHP decode via
  blitters; palette `<<2` read from YRpp + corroborated, not isolated as a
  literal `shl`).
- **Low**: could not confidently locate; document the search and what was ruled
  out.

**Why the oracle matters and where RE is still needed.** Task 9's indexed
byte-parity (2044 real frames, 0 mismatches) *proves* the extraction chain is
correct; the RE explains *why* (same CRC, same format flags, same 12-byte index
layout). But the oracle compares **indexed** pixels, so it is palette-blind — it
can never adjudicate the 6→8-bit scaling. That is precisely the one question RE
had to answer directly (see `pal.md`), which is the model for anything the oracle
can't see.

## 6. Confidence summary (this task)

| Algorithm | Function(s) | Confidence |
|-----------|-------------|:----------:|
| MIX id-hash (CRC32 + pad) | `FUN_004a1cf0` / `FUN_004a1ec0` / table `0x81E7C4` | **High** |
| MIX archive loader / index-parse | `FUN_005b3650` (`MixFileClass` ctor) | **High** |
| MIX filename→id resolve + cache | `FUN_005b3ae0` | **High** |
| Palette 6→8-bit scaling (`<<2`) | YRpp `AllocatePalette` + `FUN_004bba20` corroboration | **Medium-High** |
| SHP header/flags + subsystem | `FUN_0069dac0` / `FUN_0069dc10`; layout via `SHP.h` | **Medium** |

Details, decompiled excerpts, and divergence notes are in `mix.md`, `pal.md`,
`shp.md`.
