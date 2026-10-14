# Audio Decoder (BAG/IDX + Westwood IMA-ADPCM) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decode RA2's shipped sound-effect/voice container (`language.mix → audio.mix → audio.idx` + `audio.bag`) to interleaved 16-bit PCM in the pure `ra2assets` library, Ghidra-verified against `gamemd.exe` and byte-parity-gated against XCC's own ADPCM decoder.

**Architecture:** One new pure-library unit (`include/ra2assets/audio_bag.hpp` + `src/audio_bag.cpp`) exposing `AudioIdx` (parses the `GABA` index) and `decode_bag_clip` (raw-PCM and chunked IMA-ADPCM paths) over byte spans — no I/O, no playback. Synthetic-byte doctests drive TDD; a guarded real-asset test decodes clips from the user's own install and compares PCM byte-for-byte against an off-repo oracle tool built from XCC's verbatim GPL source. A Ghidra task locates the game's own step/index tables and loader and records them in `docs/re/audio.md`.

**Tech Stack:** C++17, CMake + Ninja + MinGW-w64, doctest; Ghidra 11.2.1 headless (Jython) against the existing `D:\ra2-re` project; MinGW g++ for the off-repo oracle tool.

## Global Constraints

- C++17, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Pure library: no SDL/Win32/OS/DirectX includes anywhere under `include/ra2assets` or `src/` — pure functions over byte spans.
- No game assets, no game binary, no Ghidra project, no GPL code committed to the repo.
- Real-asset tests skip cleanly (doctest `MESSAGE(...)` + `return`) when assets are absent, so asset-free CI still builds and passes.
- The test exe MUST be run from **PowerShell**, not Git Bash (known environment issue).
- Every new `src/*.cpp` is added to `cmake/ra2_sources.cmake` `RA2_LIB_SOURCES`; every new `tests/*.cpp` is added to `CMakeLists.txt` `target_sources(ra2assets_tests ...)`.
- TDD: write the failing test first, then the minimal implementation.
- Pristine test output: no stray prints; skips announce themselves via doctest `MESSAGE` only.
- Commit after every task.

## Byte-format ground truth (verified against the real install, 2026-07-09)

Implementers: these facts were measured from the actual files (`ra2extract` + a byte parser) and from XCC's own reader source read off-repo at `D:\ra2-re\reference\xcc` (GPL — never copy it into the repo). Cite them in code comments; do not re-derive.

**Container.** This install ships **no headered `.aud` files**. SFX + unit voices live in `language.mix → audio.mix → audio.idx` + `audio.bag`.

**`audio.idx` layout** (little-endian, packed). Header: `char magic[4] = "GABA"`, `u32 version (= 2)`, `u32 count (= 1153 here)`. Then `count` × 36-byte entries: `char fname[16]` (NUL-padded, no extension, e.g. `"ichratta"`), `u32 offset`, `u32 size`, `u32 sample_rate`, `u32 flags`, `u32 chunk_size`. Size check: `12 + 1153*36 = 41,520` = the real `audio.idx` size **exactly**. Layout matches XCC's `t_audio_idx_header`/`t_audio_idx_entry` (`D:\ra2-re\reference\xcc\misc\cc_structures.h` lines 79–96) and XCC's `is_valid` requires `size == 12 + count*36` exactly.

**Real-entry census** (parsed from the real `audio.idx`): all 1153 entries are 22,050 Hz. Flags histogram: `12` ×1126 (chunk_size 512), `6` ×21 (chunk_size 0), `7` ×4 (chunk_size 0), `13` ×2 (chunk_size 1024 — entries `uslide1`, `uslide2`). First entry: `ichratta offset=0 size=71148 rate=22050 flags=12 chunk=512`. Smallest flags-12 whole-chunk clip: `gradio3f size=512` (exactly one chunk).

**Flags bit semantics** (from XCC `misc/xse.cpp` `Cxse::extract`/`insert`, corroborated by the census — chunk_size is 0 on every bit-1 entry and nonzero on every bit-3 entry; the Ghidra task confirms the game side):

| bit | value | meaning |
|----:|------:|---------|
| 0 | 0x1 | stereo (else mono) |
| 1 | 0x2 | payload is **uncompressed 16-bit LE PCM** (chunk_size = 0) |
| 2 | 0x4 | set on every real entry (16-bit marker; no decode effect) |
| 3 | 0x8 | payload is **IMA-ADPCM 4-bit, chunked** per chunk_size |

**ADPCM payload model** (from XCC `misc/ima_adpcm_wav_decode.cpp` `Cima_adpcm_wav_decode::load`, which XCC's own BAG extractor `Cxse::extract` calls; the oracle in Task 4 gates it byte-for-byte). A flags-bit-3 clip is split into `chunk_size`-byte chunks (last one may be short). Each chunk begins with a 4-byte header — verbatim struct `t_ima_adpcm_chunk_header` from XCC `misc/riff_structures.h`:

```
i16 sample      -- initial predictor, emitted verbatim as the chunk's first output sample
u8  index       -- initial step-table index (0..88)
u8  reserved
```

then `(chunk_bytes - 4) * 2` nibbles, **low nibble first**. The predictor/index restart from each chunk's header (they are NOT fixed 0/0 per clip and do NOT carry across chunks). Total decoded samples for a mono clip:

```
n_chunks = ceil(size / chunk_size)
samples  = (size - 4*n_chunks) * 2 + n_chunks
```

e.g. `ichratta`: 139 chunks → **141,323 samples**; `gradio3f`: 1 chunk → **1,017 samples**.

**Per-nibble arithmetic** (XCC `misc/aud_decode.cpp` `aud_decode_ima_chunk`; the Ghidra task confirms the game uses the same tables/formulation — this exact shape ships in `src/audio_bag.cpp`):

```
step  = step_table[index]                 // 89-entry table, below
delta = step >> 3
if (code & 1) delta += step >> 2
if (code & 2) delta += step >> 1
if (code & 4) delta += step
if (code & 8) { sample -= delta; clamp at -32768 }
else          { sample += delta; clamp at  32767 }
emit sample
index += index_adjust[code & 7]           // {-1,-1,-1,-1,2,4,6,8}
clamp index to [0, 88]
```

Step table (89 entries): `7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767`.

**Worked example** (this exact arithmetic is asserted literally in Task 3's test). Chunk header `{sample=0, index=0}`, then data bytes `{0x07, 0x80}` → nibble sequence `7, 0, 0, 8`:

| nibble | code | index in | step | delta computation | sample out | index adjust |
|-------:|-----:|--------:|-----:|-------------------|-----------:|-------------:|
| 1 | 7 | 0 | 7 | `7>>3=0` `+7>>2=1` `+7>>1=3` `+7` = 11; `+` → 0+11 | **11** | `adjust[7]=+8` → 8 |
| 2 | 0 | 8 | 16 | `16>>3=2`; `+` → 11+2 | **13** | `adjust[0]=-1` → 7 |
| 3 | 0 | 7 | 14 | `14>>3=1`; `+` → 13+1 | **14** | `-1` → 6 |
| 4 | 8 | 6 | 13 | `13>>3=1`; sign bit → 14−1 | **13** | `adjust[0]=-1` → 5 |

Decoded clip = `{0, 11, 13, 14, 13}` (header sample first).

**Clamp sequence** (also asserted literally in Task 3): header `{0,0}` then six `0xFF` bytes = 12 nibbles of code 15 (`delta = step>>3 + step>>2 + step>>1 + step`, sign bit set): samples `-11, -41, -104, -240, -533, -1164, -2521, -5431, -11667, -25039, -32768, -32768` (index walks 0→8→16→24→32→40→48→56→64→72→80→88; steps 7,16,34,73,157,337,724,1552,3327,7132,15289,32767; deltas 11,30,63,136,293,631,1357,2910,6236,13372,28666,61436; the 11th subtraction −25039−28666 = −53705 clamps to −32768).

**Decisions bound by this plan:**
- Headered `.aud` support is **not implemented** — this install ships zero such files (recorded in `docs/re/audio.md`).
- Stereo IMA-ADPCM (flags 13 — only `uslide1`/`uslide2`, 2 of 1153 entries) is **unimplemented-with-throw**; stereo raw PCM (flags 7) is a passthrough and IS implemented. Recorded in `docs/re/audio.md`.
- `AudioIdx::find` is case-insensitive (ASCII); the game resolves sound names from mixed-case INI keys.

---

### Task 1: Ghidra RE — locate the IMA-ADPCM decoder + idx/bag loader in gamemd.exe

**Files:** Create: `D:\ra2-re\scripts\audio_dump.py` (OUTSIDE the repo — never committed), `D:\ra2-android\docs\re\audio.md` (in repo). No library code.

**Interfaces:** Consumes: the analyzed Ghidra project `D:\ra2-re` (project name `ra2`, program `gamemd.exe`, image base `0x00400000`) — read-only. Produces: `docs/re/audio.md` — the located addresses and confirmed arithmetic that `src/audio_bag.cpp` (Tasks 2–3) cites in comments.

**Concurrency rule:** the Ghidra project is EXCLUSIVELY LOCKED while any headless run is open, and other parallel agents share it. If `analyzeHeadless` reports the project is locked, **wait 60 s and retry** (each dump run takes ~1–2 min).

An honestly-documented failed hunt (Low confidence, tables not located) is an acceptable outcome — the Task-4 oracle still gates correctness. The doc template below has explicit fallback text for that case.

- [ ] **Step 1: Save the Jython post-script** to `D:\ra2-re\scripts\audio_dump.py` (outside the repo) with exactly this content:

```python
# audio_dump.py -- locate the IMA-ADPCM step/index tables and the
# audio.idx/audio.bag loader in gamemd.exe (YR 1.001) and dump the
# decompiled C of every function that references them.
#
# Anchor strategy (docs/re/workflow.md: constant tables > algorithm shape):
#   1. step table: LE image of 7,8,9,10,11,12 contiguous, entry[88] == 32767
#      (tried at both i32 and i16 width)
#   2. index-adjust table: -1,-1,-1,-1,2,4,6,8 (tried at i32 and i8 width)
#   3. strings "audio.idx" / "audio.bag" for the loader
#
# Outputs (all under D:/ra2-re/out/, never committed):
#   audio_hunt.log       -- addresses found + per-hit reference lists
#   audio_fn_<tag>_<addr>.c -- decompiled C of each referencing function
import struct
from jarray import zeros
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

OUT = "D:/ra2-re/out/"
mem = currentProgram.getMemory()
monitor = ConsoleTaskMonitor()
decomp = DecompInterface()
decomp.openProgram(currentProgram)
log = open(OUT + "audio_hunt.log", "w")

def say(s):
    print(s)
    log.write(s + "\n")

def block_data(block):
    n = int(block.getSize())
    buf = zeros(n, 'b')
    block.getBytes(block.getStart(), buf)
    return buf.tostring()

def find_all(pattern, what):
    hits = []
    for block in mem.getBlocks():
        if not block.isInitialized():
            continue
        try:
            data = block_data(block)
        except:
            continue
        i = data.find(pattern)
        while i != -1:
            hits.append(block.getStart().add(i))
            i = data.find(pattern, i + 1)
    say("%s: %d hit(s): %s" % (what, len(hits), ", ".join(str(h) for h in hits)))
    return hits

def read_le(addr, width):
    try:
        buf = zeros(width, 'b')
        mem.getBytes(addr, buf)
        fmt = "<i" if width == 4 else "<h"
        return struct.unpack(fmt, buf.tostring())[0]
    except:
        return None

dumped = {}
def dump_refs(addr, tag):
    # decompile every function containing a reference to `addr`
    refs = getReferencesTo(addr)          # FlatProgramAPI -> Reference[]
    n = 0
    for ref in refs:
        n += 1
        src = ref.getFromAddress()
        fn = getFunctionContaining(src)
        say("  ref to %s from %s (%s) fn=%s" % (addr, src, ref.getReferenceType(), fn))
        if fn is None:
            continue
        key = str(fn.getEntryPoint())
        if key in dumped:
            continue
        dumped[key] = True
        res = decomp.decompileFunction(fn, 120, monitor)
        if res is not None and res.getDecompiledFunction() is not None:
            path = OUT + "audio_fn_%s_%s.c" % (tag, key)
            open(path, "w").write(res.getDecompiledFunction().getC())
            say("  dumped %s (entry %s) -> %s" % (fn.getName(), key, path))
        else:
            say("  DECOMPILE FAILED for %s" % fn)
    if n == 0:
        say("  (no direct references to %s)" % addr)

# ---- 1. IMA step-size table: 7,8,9,10,11,12 contiguous; entry[88]==32767 ----
say("== IMA step-size table hunt ==")
for a in find_all(struct.pack("<6i", 7, 8, 9, 10, 11, 12), "step-table prefix (i32)"):
    v = read_le(a.add(88 * 4), 4)
    say("step-table(i32) candidate %s: entry[88]=%s %s" %
        (a, v, "CONFIRMED-89-ENTRY" if v == 32767 else "reject"))
    if v == 32767:
        for k in range(4):            # refs may target the first few entries
            dump_refs(a.add(k * 4), "step")
for a in find_all(struct.pack("<6h", 7, 8, 9, 10, 11, 12), "step-table prefix (i16)"):
    v = read_le(a.add(88 * 2), 2)
    say("step-table(i16) candidate %s: entry[88]=%s %s" %
        (a, v, "CONFIRMED-89-ENTRY" if v == 32767 else "reject"))
    if v == 32767:
        for k in range(4):
            dump_refs(a.add(k * 2), "step16")

# ---- 2. index-adjust table: -1,-1,-1,-1,2,4,6,8 ----
say("== index-adjust table hunt ==")
for a in find_all(struct.pack("<8i", -1, -1, -1, -1, 2, 4, 6, 8), "index-adjust (i32)"):
    dump_refs(a, "adjust")
for a in find_all(struct.pack("8b", -1, -1, -1, -1, 2, 4, 6, 8), "index-adjust (i8)"):
    dump_refs(a, "adjust8")

# ---- 3. loader strings ----
say("== audio.idx / audio.bag string hunt ==")
for s in ("audio.idx", "audio.bag", "AUDIO.IDX", "AUDIO.BAG"):
    for a in find_all(s, "string '%s'" % s):
        dump_refs(a, "str")

log.close()
say("audio_dump.py done")
```

- [ ] **Step 2: Run the headless dump** (Git Bash — the ONE part of this plan that uses Git Bash, per `docs/re/workflow.md`; Ghidra runs read-only). If the output contains "locked" (another agent holds the project), the loop waits 60 s and retries, up to 10 times:

```bash
export JAVA_HOME="/c/Program Files/Java/jdk-21"
GHIDRA="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC/support/analyzeHeadless.bat"
mkdir -p /d/ra2-re/out
for i in $(seq 1 10); do
  "$GHIDRA" "D:/ra2-re" ra2 -process gamemd.exe -noanalysis -readOnly \
    -scriptPath "D:/ra2-re/scripts" -postScript audio_dump.py \
    < /dev/null 2>&1 | tee /tmp/audio_ghidra.log
  if grep -qi "locked" /tmp/audio_ghidra.log; then
    echo "project locked by another agent; retrying in 60s ($i/10)"; sleep 60
  else
    break
  fi
done
grep -E "hit\(s\)|CONFIRMED|dumped|done" /tmp/audio_ghidra.log
```

Expected output: the run takes ~1–2 min; the final grep shows the hit lines (e.g. `step-table prefix (i32): 1 hit(s): 007xxxxx`), one or more `CONFIRMED-89-ENTRY` lines, `dumped FUN_00xxxxxx ... -> D:/ra2-re/out/audio_fn_step_...c` lines, and `audio_dump.py done`. If every hunt reports `0 hit(s)`, that is the documented-failure path — proceed to Step 4 and use the fallback text.

- [ ] **Step 3: Inspect the dumps.** Read `D:/ra2-re/out/audio_hunt.log` and each `D:/ra2-re/out/audio_fn_*.c`. In the decompiled decode function(s), identify and note: (a) the per-nibble arithmetic (compare shape against the "Per-nibble arithmetic" block in this plan's ground-truth section — `step>>3` cascade, ±delta on bit 3, clamps at −32768/32767, index clamp 0..88); (b) whether nibble order is low-first (`sample_index & 1 ? code >> 4 : code & 0xf` shape); (c) where the initial predictor/index come from (a 4-byte chunk-header read — i16 + u8 — confirms the chunked model; a hardcoded 0/0 with no chunk loop would contradict it and MUST be recorded as a divergence); (d) in the loader dump(s), the `audio.idx` header validation (magic/version/count) and the 36-byte entry stride, plus any name uppercasing/case-insensitive compare in the lookup path. Record actual addresses.

- [ ] **Step 4: Write `docs/re/audio.md`** (in-repo; prose + short illustrative excerpts ONLY — never paste full decompilation dumps). Use exactly this document, replacing each `«...»` token with the real value from `audio_hunt.log`/the dumps. If a hunt found nothing, replace its table row's last two cells with `not located — search documented in this section` and `Low`, and say in prose what was searched (both widths, all four strings) and that the Task-4 oracle carries the correctness gate:

````markdown
# Audio (audio.idx / audio.bag + IMA-ADPCM) — reverse-engineering notes

Foundation-batch sub-project 3. Method per `docs/re/workflow.md`; image base
`0x00400000`. The correctness gate is twofold: the located game decoder below,
and the Task-4 byte-parity oracle against XCC's own ADPCM decode
(`tests/test_audio.cpp`, oracle generation in `scripts/make_audio_oracle.md`).

## Scope decision: no headered `.aud` support

This install (YR 1.001, `gamemd.exe` SHA1
`7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`) ships **zero** headered `.aud`
files (verified 2026-07-09). SFX/voices live in
`language.mix → audio.mix → audio.idx + audio.bag`; EVA speech and music are
RIFF WAVs (`fmt` 0x0011, same IMA codec — trivial container, deferred to the
audio phase). Headered-AUD and WS-ADPCM ("type 1") are therefore **not
implemented** — asset-less here.

## audio.idx (`GABA`) layout — confirmed

12-byte header + `count` × 36-byte entries; the real file is
`12 + 1153*36 = 41,520` bytes exactly. Matches XCC `t_audio_idx_header` /
`t_audio_idx_entry` (`misc/cc_structures.h`), whose `is_valid` also requires
the exact-size equality our loader mirrors.

| offset | type | field | ra2assets read |
|-------:|------|-------|----------------|
| 0 | char[4] | magic `"GABA"` | validated |
| 4 | u32 | version (= 2) | validated |
| 8 | u32 | count | entry loop |
| +0 | char[16] | fname, NUL-padded, no extension | `BagClip::name` |
| +16 | u32 | offset into audio.bag | `BagClip::offset` |
| +20 | u32 | size in bytes | `BagClip::size` |
| +24 | u32 | sample rate | `BagClip::sample_rate` |
| +28 | u32 | flags | `BagClip::flags` |
| +32 | u32 | chunk size | `BagClip::chunk_size` |

## Real-install census (all 1153 entries)

All 22,050 Hz. Flags histogram: `12` ×1126 (chunk 512), `6` ×21 (chunk 0),
`7` ×4 (chunk 0), `13` ×2 (chunk 1024: `uslide1`, `uslide2`). The flags-6
(mono raw PCM) entries are loop/ambient sounds (`icraloop`, `vblhlo2a-c`,
`vkirlo1/2a-c`, `schrlo1/2a-c`, `gcraprom`, `vcoplo1/2a-b`, `gpsyampa-c`,
`ubase`, `utext`); flags-7 (stereo raw PCM) are `uscolo1/2a-c`.

Flag bits (XCC `xse.cpp` `Cxse::extract`/`insert`, corroborated by the
chunk_size correlation above; game side: see located loader below):
bit 0 (0x1) stereo; bit 1 (0x2) uncompressed 16-bit LE PCM; bit 2 (0x4) set
on every entry (16-bit marker, no decode effect); bit 3 (0x8) IMA-ADPCM.

## BAG ADPCM payload: chunked, with per-chunk 4-byte headers

A flags-bit-3 clip is `chunk_size`-byte chunks; each starts
`{i16 predictor; u8 step_index; u8 reserved}` — the predictor is emitted
verbatim as the chunk's first sample, then `(chunk_bytes-4)*2` nibbles decode
**low nibble first**. Predictor/index restart from each chunk header (NOT
fixed 0/0 per clip; nothing carries across chunks). Mono sample count:
`(size - 4*ceil(size/chunk_size))*2 + ceil(size/chunk_size)` — e.g.
`ichratta` (71,148 B, 139 chunks) → 141,323 samples. This matches XCC's
`Cima_adpcm_wav_decode::load`, which XCC's own BAG extractor calls, and is
byte-parity-verified by the Task-4 oracle.

## Per-nibble arithmetic — confirmed against the game decoder

```
step  = step_table[index]            // 89 entries, 7..32767
delta = step>>3
if (code&1) delta += step>>2
if (code&2) delta += step>>1
if (code&4) delta += step
if (code&8) sample -= delta (clamp -32768) else sample += delta (clamp 32767)
index += {-1,-1,-1,-1,2,4,6,8}[code & 7]; clamp to [0,88]
```

Worked vector (asserted literally in `tests/test_audio.cpp`): header
`{sample=0,index=0}` + bytes `{0x07,0x80}` → nibbles 7,0,0,8 → PCM
`{0, 11, 13, 14, 13}`.

## Ghidra location in `gamemd.exe` (YR 1.001)

Hunt: constant-table anchoring (`docs/re/workflow.md` technique #1) — the LE
dword/word image of step-table prefix `7,8,9,10,11,12` verified by
`entry[88]==32767`, the index-adjust image `-1,-1,-1,-1,2,4,6,8`, and the
strings `audio.idx`/`audio.bag`; then `getReferencesTo` → decompile
(`D:/ra2-re/scripts/audio_dump.py`, outputs under `D:/ra2-re/out/`, neither
committed).

- Step table: `«address»` (width: `«i32 or i16»`), 89 entries ending 32767.
- Index-adjust table: `«address»` (width: `«i32 or i8»`).
- Decode function(s) referencing them: `«FUN_00xxxxxx, ...»` — per-nibble
  shape `«matches / diverges: detail»`; nibble order `«low-first / other»`;
  initial predictor/index `«read from 4-byte chunk header / other»`.
- `audio.idx`/`audio.bag` loader: `«FUN_00xxxxxx, ...»` — header validation
  `«magic/version/count details»`; 36-byte entry stride `«confirmed / not seen»`;
  name lookup casing `«uppercased / case-insensitive compare / not seen»`.

Illustrative excerpt (decode core, from `«audio_fn_..._....c»`):

```c
«3-10 line excerpt showing the step>>3 delta cascade or the clamp — SHORT,
illustrative only»
```

## Confidence

| Item | Evidence | Confidence |
|------|----------|:----------:|
| idx layout (12 + n*36, field order) | exact size arithmetic on the real file + XCC structs + loader dump `«addr»` | «High/Med/Low» |
| flags bit semantics | XCC extract/insert + census correlation + `«game-side evidence»` | «High/Med/Low» |
| ADPCM tables (step/index-adjust) | `«located at ... , bit-identical / not located»` | «High/Med/Low» |
| per-nibble arithmetic + clamps | `«decompiled FUN_... matches 1:1 / oracle-only»` + Task-4 byte parity | «High/Med/Low» |
| chunked model (4-byte chunk headers) | XCC decode path + real chunk_size census + `«game-side evidence»` + oracle | «High/Med/Low» |

Divergences found: «none / list them». A `«Low»` row means the hunt failed to
locate that item in the binary; the Task-4 oracle (byte-for-byte PCM parity on
real clips) still gates decoder correctness.

## Decisions

- Headered `.aud`: **not implemented** (no assets in this install).
- Stereo IMA-ADPCM (flags 13; only `uslide1`/`uslide2`): **unimplemented —
  `decode_bag_clip` throws** with a message pointing here. Stereo raw PCM
  (flags 7) is passthrough and implemented.
- `AudioIdx::find` is ASCII case-insensitive (INI sound keys are mixed-case).
````

- [ ] **Step 5: Commit** (from `D:\ra2-android`; the script and dumps live outside the repo and are NOT added):

```powershell
git add docs/re/audio.md
git commit -m "docs: RE notes - audio.idx/bag loader + IMA-ADPCM decoder located in gamemd.exe"
```

---

### Task 2: AudioIdx — the GABA audio.idx parser

**Files:** Create: `include/ra2assets/audio_bag.hpp`, `src/audio_bag.cpp`, `tests/test_audio.cpp`. Modify: `cmake/ra2_sources.cmake` (one line), `CMakeLists.txt` (one line).

**Interfaces:** Consumes: nothing outside the standard library. Produces (the spec's exact public interface — later tasks and future phases rely on these signatures):

```cpp
namespace ra2assets {
struct BagClip {
    std::string name;               // idx name field (no extension)
    uint32_t offset = 0, size = 0;  // clip bytes within audio.bag
    uint32_t sample_rate = 0, flags = 0, chunk_size = 0;
};
class AudioIdx {
public:
    static AudioIdx load(const uint8_t* data, size_t len);
    const BagClip* find(const std::string& name) const;  // nullptr if absent
    const std::vector<BagClip>& clips() const;
};
struct AudioClipInfo { int sample_rate = 0; int channels = 1; int bits = 16; };
std::vector<int16_t> decode_bag_clip(const uint8_t* bag_data, size_t bag_len,
                                     const BagClip& clip, AudioClipInfo& out_info);
}
```

(The header declares `decode_bag_clip` now; its definition lands in Task 3 — nothing references it until Task 3's tests, so the library links fine.)

- [ ] **Step 1: Write the failing test.** Create `tests/test_audio.cpp` with exactly:

```cpp
#include "doctest.h"
#include "ra2assets/audio_bag.hpp"
#include "ra2assets/mix.hpp"     // used by the Task-4 real-asset oracle test
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>            // used by the Task-4 real-asset oracle test
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic audio.idx builder. On-disk layout (little-endian, packed) as
// verified against the real install (12 + 1153*36 = 41,520 bytes exactly) and
// XCC's t_audio_idx_header/t_audio_idx_entry:
//   header: char magic[4]="GABA"; u32 version(=2); u32 count
//   entry : char fname[16] (NUL-padded); u32 offset; u32 size;
//           u32 sample_rate; u32 flags; u32 chunk_size
// ---------------------------------------------------------------------------
struct IdxEntrySpec {
    std::string name;
    uint32_t offset, size, sample_rate, flags, chunk_size;
};

static void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)((v >> (8 * i)) & 0xff));
}

static std::vector<uint8_t> make_idx(const std::vector<IdxEntrySpec>& entries) {
    std::vector<uint8_t> b;
    b.push_back('G'); b.push_back('A'); b.push_back('B'); b.push_back('A');
    put32(b, 2);                                  // version
    put32(b, (uint32_t)entries.size());           // count
    for (const IdxEntrySpec& e : entries) {
        for (size_t i = 0; i < 16; i++)           // fname[16], NUL-padded
            b.push_back(i < e.name.size() ? (uint8_t)e.name[i] : 0);
        put32(b, e.offset); put32(b, e.size);
        put32(b, e.sample_rate); put32(b, e.flags); put32(b, e.chunk_size);
    }
    return b;
}

TEST_CASE("audio idx: valid header parses, order preserved") {
    auto b = make_idx({
        {"IchRatta", 0, 71148, 22050, 12, 512},
        {"gradio3f", 71148, 512, 22050, 12, 512},
    });
    REQUIRE(b.size() == 12 + 2 * 36);             // mirror XCC is_valid arithmetic
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    REQUIRE(idx.clips().size() == 2);
    CHECK(idx.clips()[0].name == "IchRatta");     // raw on-disk name preserved
    CHECK(idx.clips()[0].offset == 0);
    CHECK(idx.clips()[0].size == 71148);
    CHECK(idx.clips()[0].sample_rate == 22050);
    CHECK(idx.clips()[0].flags == 12);
    CHECK(idx.clips()[0].chunk_size == 512);
    CHECK(idx.clips()[1].name == "gradio3f");     // order preserved
    CHECK(idx.clips()[1].offset == 71148);
}

TEST_CASE("audio idx: find is case-insensitive, absent -> nullptr") {
    auto b = make_idx({{"IchRatta", 0, 100, 22050, 12, 512}});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    const BagClip* c1 = idx.find("ichratta");
    const BagClip* c2 = idx.find("ICHRATTA");
    const BagClip* c3 = idx.find("IchRatta");
    REQUIRE(c1 != nullptr);
    CHECK(c1 == c2);
    CHECK(c1 == c3);
    CHECK(c1->size == 100);
    CHECK(idx.find("nope") == nullptr);
    CHECK(idx.find("") == nullptr);
    CHECK(idx.find("ichratta_longer_than_16") == nullptr);
}

TEST_CASE("audio idx: 16-char name (no NUL terminator on disk)") {
    auto b = make_idx({{"abcdefghijklmnop", 4, 8, 22050, 12, 512}});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    REQUIRE(idx.clips().size() == 1);
    CHECK(idx.clips()[0].name == "abcdefghijklmnop");
    CHECK(idx.find("ABCDEFGHIJKLMNOP") == &idx.clips()[0]);
}

TEST_CASE("audio idx: empty index (count=0) is valid") {
    auto b = make_idx({});
    AudioIdx idx = AudioIdx::load(b.data(), b.size());
    CHECK(idx.clips().empty());
    CHECK(idx.find("anything") == nullptr);
}

TEST_CASE("audio idx: validation rejects malformed input") {
    // Too small for even the 12-byte header.
    {
        std::vector<uint8_t> b = {'G', 'A', 'B', 'A', 2, 0, 0, 0};
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong magic.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[0] = 'X';
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong version (XCC is_valid: h.two != 2 -> invalid).
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[4] = 1;
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Size mismatch: trailing byte (XCC: 12 + count*36 must EQUAL the size).
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b.push_back(0);
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Size mismatch: truncated final entry.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b.pop_back();
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
    // Count claims more entries than the bytes hold.
    {
        auto b = make_idx({{"x", 0, 1, 22050, 12, 512}});
        b[8] = 200;                                // count = 200, size unchanged
        CHECK_THROWS_AS(AudioIdx::load(b.data(), b.size()), std::runtime_error);
    }
}
```

- [ ] **Step 2: Register the test file.** In `D:\ra2-android\CMakeLists.txt`, find the line `target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)` and add directly below it:

```cmake
target_sources(ra2assets_tests PRIVATE tests/test_audio.cpp)
```

- [ ] **Step 3: Run the build to verify it fails.** From `D:\ra2-android` in PowerShell (run `cmake -S . -B build -G Ninja` first ONLY if `build/` doesn't exist or is stale):

```powershell
cmake --build build --target ra2assets_tests
```

Expected failure: compilation of `tests/test_audio.cpp` fails with `fatal error: ra2assets/audio_bag.hpp: No such file or directory`.

- [ ] **Step 4: Write the header.** Create `include/ra2assets/audio_bag.hpp` with exactly:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ra2assets {

// One entry of audio.idx: a named clip inside audio.bag.
// flags (docs/re/audio.md): bit0 stereo, bit1 uncompressed 16-bit PCM,
// bit2 always set on real entries (16-bit marker), bit3 chunked IMA-ADPCM.
struct BagClip {
    std::string name;               // idx name field (no extension)
    uint32_t offset = 0, size = 0;  // clip bytes within audio.bag
    uint32_t sample_rate = 0, flags = 0, chunk_size = 0;
};

// Parser for audio.idx ("GABA" magic, version 2): 12-byte header +
// count * 36-byte entries (see docs/re/audio.md).
class AudioIdx {
public:
    static AudioIdx load(const uint8_t* data, size_t len);
    const BagClip* find(const std::string& name) const;  // nullptr if absent
    const std::vector<BagClip>& clips() const { return clips_; }
private:
    std::vector<BagClip> clips_;
};

struct AudioClipInfo { int sample_rate = 0; int channels = 1; int bits = 16; };

// Decode one clip's bytes (from audio.bag) to interleaved 16-bit PCM.
// Raw-PCM clips (flags bit 1) pass through; IMA-ADPCM clips (flags bit 3)
// decode chunk-by-chunk per clip.chunk_size. Stereo IMA-ADPCM throws
// (unimplemented; 2 of 1153 real entries -- docs/re/audio.md).
std::vector<int16_t> decode_bag_clip(const uint8_t* bag_data, size_t bag_len,
                                     const BagClip& clip, AudioClipInfo& out_info);

} // namespace ra2assets
```

- [ ] **Step 5: Write the minimal implementation.** Create `src/audio_bag.cpp` with exactly (AudioIdx only — `decode_bag_clip` lands in Task 3):

```cpp
#include "ra2assets/audio_bag.hpp"
#include <stdexcept>
#include <string>

namespace ra2assets {

// ---------------------------------------------------------------------------
// audio.idx (GABA) index. On-disk layout (little-endian, packed), verified
// against the real install: 12-byte header + count * 36-byte entries; the
// real audio.idx is 41,520 = 12 + 1153*36 bytes exactly. Corroborated by
// XCC misc/cc_structures.h t_audio_idx_header / t_audio_idx_entry (read
// off-repo at D:\ra2-re\reference\xcc -- no GPL code copied here) and the
// game's own loader (docs/re/audio.md).
//   header: char magic[4] = "GABA"; u32 version (=2); u32 count
//   entry : char fname[16] (NUL-padded, no extension); u32 offset; u32 size;
//           u32 sample_rate; u32 flags; u32 chunk_size
// ---------------------------------------------------------------------------
static constexpr size_t kIdxHeader = 12;
static constexpr size_t kIdxEntry  = 36;

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static char upcase(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool iequal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (upcase(a[i]) != upcase(b[i])) return false;
    return true;
}

AudioIdx AudioIdx::load(const uint8_t* data, size_t len) {
    if (len < kIdxHeader)
        throw std::runtime_error("audio.idx: smaller than 12-byte header (" +
                                 std::to_string(len) + " bytes)");
    if (!(data[0] == 'G' && data[1] == 'A' && data[2] == 'B' && data[3] == 'A'))
        throw std::runtime_error("audio.idx: bad magic (want \"GABA\")");
    const uint32_t version = r32(data + 4);
    if (version != 2)
        throw std::runtime_error("audio.idx: unsupported version " +
                                 std::to_string(version) + " (want 2)");
    const uint32_t count = r32(data + 8);
    if (count > 1000000u)
        throw std::runtime_error("audio.idx: implausible entry count " +
                                 std::to_string(count));
    // Mirror XCC Caudio_idx_file::is_valid: header + count*entry must EQUAL
    // the byte length exactly.
    if (len != kIdxHeader + (size_t)count * kIdxEntry)
        throw std::runtime_error("audio.idx: size " + std::to_string(len) +
                                 " != 12 + " + std::to_string(count) + "*36");
    AudioIdx idx;
    idx.clips_.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* e = data + kIdxHeader + (size_t)i * kIdxEntry;
        BagClip c;
        size_t n = 0;
        while (n < 16 && e[n] != 0) n++;          // fname[16], NUL-padded
        c.name.assign(reinterpret_cast<const char*>(e), n);
        c.offset      = r32(e + 16);
        c.size        = r32(e + 20);
        c.sample_rate = r32(e + 24);
        c.flags       = r32(e + 28);
        c.chunk_size  = r32(e + 32);
        idx.clips_.push_back(std::move(c));
    }
    return idx;
}

const BagClip* AudioIdx::find(const std::string& name) const {
    // Case-insensitive: the game resolves sound names from INI keys whose
    // casing is inconsistent (docs/re/audio.md).
    for (const BagClip& c : clips_)
        if (iequal(c.name, name)) return &c;
    return nullptr;
}

} // namespace ra2assets
```

- [ ] **Step 6: Register the library source.** In `D:\ra2-android\cmake\ra2_sources.cmake`, inside `RA2_LIB_SOURCES`, add directly below the `${RA2_ROOT}/src/theater.cpp` line (keeping the closing `)` after it):

```cmake
    ${RA2_ROOT}/src/audio_bag.cpp
```

- [ ] **Step 7: Run the tests to verify they pass.** From `D:\ra2-android` in PowerShell:

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="audio idx*"
```

Expected: build succeeds; the run reports 5 test cases, 0 failures, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 8: Commit.**

```powershell
git add include/ra2assets/audio_bag.hpp src/audio_bag.cpp tests/test_audio.cpp cmake/ra2_sources.cmake CMakeLists.txt
git commit -m "feat: AudioIdx - GABA audio.idx parser (load/find/clips)"
```

---

### Task 3: decode_bag_clip — chunked IMA-ADPCM + raw-PCM decode

**Files:** Modify: `src/audio_bag.cpp` (add tables + `decode_bag_clip`), `tests/test_audio.cpp` (append decode tests). No new files, no CMake changes.

**Interfaces:** Consumes: `BagClip` (Task 2). Produces: `std::vector<int16_t> decode_bag_clip(const uint8_t* bag_data, size_t bag_len, const BagClip& clip, AudioClipInfo& out_info)` — already declared in the Task-2 header; this task defines it.

- [ ] **Step 1: Write the failing tests.** Append to the END of `tests/test_audio.cpp` exactly:

```cpp
// ---------------------------------------------------------------------------
// decode_bag_clip synthetic tests. BAG ADPCM clips are chunked: chunk_size-
// byte chunks, each opening with a 4-byte header {i16 predictor; u8 index;
// u8 reserved} whose predictor is emitted as the chunk's first sample, then
// (chunk_bytes-4)*2 nibbles, LOW nibble first (docs/re/audio.md; XCC
// Cima_adpcm_wav_decode::load + aud_decode_ima_chunk are the cross-check).
// All expected PCM vectors below are HAND-COMPUTED from the step/index
// tables -- the arithmetic is worked in the plan and docs/re/audio.md.
// ---------------------------------------------------------------------------
static BagClip clip_of(uint32_t offset, uint32_t size, uint32_t flags,
                       uint32_t chunk_size) {
    BagClip c;
    c.name = "test";
    c.offset = offset; c.size = size;
    c.sample_rate = 22050; c.flags = flags; c.chunk_size = chunk_size;
    return c;
}

static void put_chunk_header(std::vector<uint8_t>& b, int16_t sample, uint8_t index) {
    b.push_back((uint8_t)((uint16_t)sample & 0xff));
    b.push_back((uint8_t)(((uint16_t)sample >> 8) & 0xff));
    b.push_back(index);
    b.push_back(0);                                // reserved
}

TEST_CASE("bag decode: worked IMA example (predictor 0, index 0)") {
    std::vector<uint8_t> bag;
    put_chunk_header(bag, 0, 0);
    bag.push_back(0x07);                           // nibbles: 7 (low), 0 (high)
    bag.push_back(0x80);                           // nibbles: 0 (low), 8 (high)
    AudioClipInfo info;
    auto pcm = decode_bag_clip(bag.data(), bag.size(),
                               clip_of(0, 6, /*flags*/ 12, /*chunk*/ 512), info);
    // Hand-computed: header sample 0; nibble 7 (step 7, delta 0+1+3+7=11) -> 11,
    // index += 8; nibble 0 (step 16, delta 2) -> 13, index 7; nibble 0 (step 14,
    // delta 1) -> 14, index 6; nibble 8 (step 13, delta 1, sign) -> 13.
    const std::vector<int16_t> want = {0, 11, 13, 14, 13};
    CHECK(pcm == want);
    CHECK(info.sample_rate == 22050);
    CHECK(info.channels == 1);
    CHECK(info.bits == 16);
}

TEST_CASE("bag decode: each chunk restarts from its own header") {
    // chunk_size=8 -> each chunk is a 4-byte header + 4 nibble bytes.
    std::vector<uint8_t> bag;
    put_chunk_header(bag, 0, 0);
    for (int i = 0; i < 4; i++) bag.push_back(0xFF);   // 8 x code 15
    put_chunk_header(bag, 1000, 0);
    for (int i = 0; i < 4; i++) bag.push_back(0x00);   // 8 x code 0
    AudioClipInfo info;
    auto pcm = decode_bag_clip(bag.data(), bag.size(), clip_of(0, 16, 12, 8), info);
    // Chunk 2 must NOT continue from -5431: it restarts at its header's
    // predictor 1000, and code-0 nibbles at index 0 (step 7, delta 7>>3 = 0)
    // leave the predictor unchanged.
    const std::vector<int16_t> want = {
        0, -11, -41, -104, -240, -533, -1164, -2521, -5431,     // chunk 1
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,   // chunk 2 (reset)
    };
    CHECK(pcm == want);
}

TEST_CASE("bag decode: predictor clamps to int16 range") {
    std::vector<uint8_t> bag;
    put_chunk_header(bag, 0, 0);
    for (int i = 0; i < 6; i++) bag.push_back(0xFF);   // 12 x code 15
    AudioClipInfo info;
    auto pcm = decode_bag_clip(bag.data(), bag.size(), clip_of(0, 10, 12, 512), info);
    // Hand-computed walk (index 0,8,16,...,80,88; steps 7,16,34,73,157,337,
    // 724,1552,3327,7132,15289,32767; deltas 11,30,63,136,293,631,1357,2910,
    // 6236,13372,28666,61436): the 11th subtraction hits -53705 -> clamp.
    const std::vector<int16_t> want = {
        0, -11, -41, -104, -240, -533, -1164, -2521, -5431,
        -11667, -25039, -32768, -32768,
    };
    CHECK(pcm == want);
}

TEST_CASE("bag decode: raw 16-bit PCM path (flags bit 1)") {
    // flags 6 = 0x4|0x2 (mono raw PCM), chunk_size 0 -- 21 entries in the real
    // audio.idx use exactly this (e.g. "utext"); bytes pass through as LE i16.
    std::vector<uint8_t> bag = {0xFE, 0xFF, 0xBC, 0x02, 0xFF, 0x7F};  // -2, 700, 32767
    AudioClipInfo info;
    auto pcm = decode_bag_clip(bag.data(), bag.size(), clip_of(0, 6, 6, 0), info);
    const std::vector<int16_t> want = {-2, 700, 32767};
    CHECK(pcm == want);
    CHECK(info.channels == 1);

    // flags 7 adds the stereo bit: same passthrough (already interleaved).
    auto pcm2 = decode_bag_clip(bag.data(), bag.size(), clip_of(0, 6, 7, 0), info);
    CHECK(pcm2 == want);
    CHECK(info.channels == 2);
}

TEST_CASE("bag decode: empty clip -> empty vector") {
    std::vector<uint8_t> bag = {0x00};
    AudioClipInfo info;
    auto pcm = decode_bag_clip(bag.data(), bag.size(), clip_of(0, 0, 12, 512), info);
    CHECK(pcm.empty());
    CHECK(info.sample_rate == 22050);
    CHECK(info.channels == 1);
}

TEST_CASE("bag decode: errors") {
    std::vector<uint8_t> bag(16, 0);
    AudioClipInfo info;
    // Clip range beyond bag_len.
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(8, 12, 12, 512), info),
                    std::runtime_error);
    // Offset far beyond the bag (also exercises the overflow-safe check).
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0xFFFFFFFFu, 4, 12, 512), info),
                    std::runtime_error);
    // ADPCM with chunk_size too small to hold the 4-byte chunk header.
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0, 8, 12, 0), info),
                    std::runtime_error);
    // Stereo IMA-ADPCM (flags 13): unimplemented-with-throw (only
    // uslide1/uslide2 in the real install -- docs/re/audio.md).
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0, 16, 13, 1024), info),
                    std::runtime_error);
    // Neither PCM (bit 1) nor ADPCM (bit 3) flagged.
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0, 8, 4, 0), info),
                    std::runtime_error);
    // Raw PCM with odd byte size.
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0, 5, 6, 0), info),
                    std::runtime_error);
    // Trailing partial chunk too small to hold its 4-byte header
    // (size 10, chunk 8 -> second chunk is only 2 bytes).
    CHECK_THROWS_AS(decode_bag_clip(bag.data(), bag.size(),
                                    clip_of(0, 10, 12, 8), info),
                    std::runtime_error);
    // Chunk-header step index out of range (> 88).
    {
        std::vector<uint8_t> b2;
        put_chunk_header(b2, 0, 89);
        b2.push_back(0x00);
        CHECK_THROWS_AS(decode_bag_clip(b2.data(), b2.size(),
                                        clip_of(0, 5, 12, 512), info),
                        std::runtime_error);
    }
}
```

- [ ] **Step 2: Run to verify it fails.** From `D:\ra2-android` in PowerShell:

```powershell
cmake --build build --target ra2assets_tests
```

Expected failure: the tests compile (the header declares the function) but the link fails with `undefined reference to 'ra2assets::decode_bag_clip(...)'`.

- [ ] **Step 3: Write the implementation.** Replace the entire contents of `src/audio_bag.cpp` with exactly (this is the Task-2 file plus the flags constants, the IMA tables, and `decode_bag_clip`):

```cpp
#include "ra2assets/audio_bag.hpp"
#include <stdexcept>
#include <string>

namespace ra2assets {

// ---------------------------------------------------------------------------
// audio.idx (GABA) index. On-disk layout (little-endian, packed), verified
// against the real install: 12-byte header + count * 36-byte entries; the
// real audio.idx is 41,520 = 12 + 1153*36 bytes exactly. Corroborated by
// XCC misc/cc_structures.h t_audio_idx_header / t_audio_idx_entry (read
// off-repo at D:\ra2-re\reference\xcc -- no GPL code copied here) and the
// game's own loader (docs/re/audio.md).
//   header: char magic[4] = "GABA"; u32 version (=2); u32 count
//   entry : char fname[16] (NUL-padded, no extension); u32 offset; u32 size;
//           u32 sample_rate; u32 flags; u32 chunk_size
// ---------------------------------------------------------------------------
static constexpr size_t kIdxHeader = 12;
static constexpr size_t kIdxEntry  = 36;

// audio.bag entry flags. Real-install histogram: 12 x1126, 6 x21, 7 x4,
// 13 x2. Semantics per the game loader (docs/re/audio.md), cross-checked
// against XCC xse.cpp Cxse::extract/insert and the chunk_size correlation
// (every bit-1 entry has chunk_size 0; every bit-3 entry has 512/1024):
//   bit 0 (0x1) stereo
//   bit 1 (0x2) uncompressed 16-bit LE PCM
//   bit 2 (0x4) set on every real entry (16-bit marker; no decode effect)
//   bit 3 (0x8) IMA-ADPCM 4-bit, chunked per chunk_size
static constexpr uint32_t kFlagStereo = 0x1u;
static constexpr uint32_t kFlagPcm    = 0x2u;
static constexpr uint32_t kFlagAdpcm  = 0x8u;

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static char upcase(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool iequal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (upcase(a[i]) != upcase(b[i])) return false;
    return true;
}

AudioIdx AudioIdx::load(const uint8_t* data, size_t len) {
    if (len < kIdxHeader)
        throw std::runtime_error("audio.idx: smaller than 12-byte header (" +
                                 std::to_string(len) + " bytes)");
    if (!(data[0] == 'G' && data[1] == 'A' && data[2] == 'B' && data[3] == 'A'))
        throw std::runtime_error("audio.idx: bad magic (want \"GABA\")");
    const uint32_t version = r32(data + 4);
    if (version != 2)
        throw std::runtime_error("audio.idx: unsupported version " +
                                 std::to_string(version) + " (want 2)");
    const uint32_t count = r32(data + 8);
    if (count > 1000000u)
        throw std::runtime_error("audio.idx: implausible entry count " +
                                 std::to_string(count));
    // Mirror XCC Caudio_idx_file::is_valid: header + count*entry must EQUAL
    // the byte length exactly.
    if (len != kIdxHeader + (size_t)count * kIdxEntry)
        throw std::runtime_error("audio.idx: size " + std::to_string(len) +
                                 " != 12 + " + std::to_string(count) + "*36");
    AudioIdx idx;
    idx.clips_.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* e = data + kIdxHeader + (size_t)i * kIdxEntry;
        BagClip c;
        size_t n = 0;
        while (n < 16 && e[n] != 0) n++;          // fname[16], NUL-padded
        c.name.assign(reinterpret_cast<const char*>(e), n);
        c.offset      = r32(e + 16);
        c.size        = r32(e + 20);
        c.sample_rate = r32(e + 24);
        c.flags       = r32(e + 28);
        c.chunk_size  = r32(e + 32);
        idx.clips_.push_back(std::move(c));
    }
    return idx;
}

const BagClip* AudioIdx::find(const std::string& name) const {
    // Case-insensitive: the game resolves sound names from INI keys whose
    // casing is inconsistent (docs/re/audio.md).
    for (const BagClip& c : clips_)
        if (iequal(c.name, name)) return &c;
    return nullptr;
}

// ---------------------------------------------------------------------------
// IMA-ADPCM tables. The standard IMA step-size (89 entries) and index-adjust
// tables -- the same constants the game's own decoder in gamemd.exe uses
// (located via constant-table anchoring; addresses in docs/re/audio.md).
// ---------------------------------------------------------------------------
static const int kStepTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,
    17,    19,    21,    23,    25,    28,    31,    34,    37,
    41,    45,    50,    55,    60,    66,    73,    80,    88,
    97,    107,   118,   130,   143,   157,   173,   190,   209,
    230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,
    1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,
    3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
    7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};
static const int kIndexAdjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

std::vector<int16_t> decode_bag_clip(const uint8_t* bag_data, size_t bag_len,
                                     const BagClip& clip, AudioClipInfo& out_info) {
    out_info.sample_rate = (int)clip.sample_rate;
    out_info.channels    = (clip.flags & kFlagStereo) ? 2 : 1;
    out_info.bits        = 16;

    if (clip.size == 0)
        return {};
    if (clip.offset > bag_len || clip.size > bag_len - clip.offset)
        throw std::runtime_error("bag: clip '" + clip.name + "' range [" +
            std::to_string(clip.offset) + ", +" + std::to_string(clip.size) +
            ") exceeds bag (" + std::to_string(bag_len) + " bytes)");
    const uint8_t* src = bag_data + clip.offset;

    if (clip.flags & kFlagPcm) {
        // Uncompressed 16-bit little-endian PCM; stereo data is already
        // interleaved on disk, so mono and stereo share this passthrough.
        if (clip.size % 2 != 0)
            throw std::runtime_error("bag: PCM clip '" + clip.name +
                "' has odd byte size " + std::to_string(clip.size));
        std::vector<int16_t> out(clip.size / 2);
        for (size_t i = 0; i < out.size(); i++)
            out[i] = (int16_t)((uint16_t)src[2 * i] |
                               ((uint16_t)src[2 * i + 1] << 8));
        return out;
    }

    if (!(clip.flags & kFlagAdpcm))
        throw std::runtime_error("bag: clip '" + clip.name +
            "' has unsupported flags " + std::to_string(clip.flags) +
            " (neither PCM bit 1 nor ADPCM bit 3)");
    if (out_info.channels != 1)
        throw std::runtime_error("bag: clip '" + clip.name +
            "' is stereo IMA-ADPCM -- unimplemented (only uslide1/uslide2 in "
            "the real install; see docs/re/audio.md)");
    if (clip.chunk_size < 4)
        throw std::runtime_error("bag: ADPCM clip '" + clip.name +
            "' bad chunk_size " + std::to_string(clip.chunk_size) +
            " (must hold the 4-byte chunk header)");

    // Chunked IMA-ADPCM (docs/re/audio.md): the clip is chunk_size-byte
    // chunks (last may be short); each opens with a 4-byte header
    // {i16 predictor; u8 step_index; u8 reserved} whose predictor is emitted
    // verbatim as the chunk's first sample, then (chunk_bytes-4)*2 nibbles
    // decode LOW nibble first. Predictor/index restart from every chunk
    // header. Total samples: (size - 4*n_chunks)*2 + n_chunks.
    std::vector<int16_t> out;
    const size_t n_chunks =
        ((size_t)clip.size + clip.chunk_size - 1) / clip.chunk_size;
    if ((size_t)clip.size >= 4 * n_chunks)
        out.reserve(((size_t)clip.size - 4 * n_chunks) * 2 + n_chunks);

    size_t remaining = clip.size;
    const uint8_t* p = src;
    while (remaining > 0) {
        const size_t chunk =
            remaining < clip.chunk_size ? remaining : clip.chunk_size;
        if (chunk < 4)
            throw std::runtime_error("bag: ADPCM clip '" + clip.name +
                "' trailing " + std::to_string(chunk) +
                " byte(s) cannot hold a 4-byte chunk header");
        int sample = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        int index  = p[2];                        // p[3] reserved
        if (index > 88)
            throw std::runtime_error("bag: ADPCM clip '" + clip.name +
                "' chunk step index " + std::to_string(index) + " > 88");
        out.push_back((int16_t)sample);
        const size_t nibbles = (chunk - 4) * 2;
        const uint8_t* d = p + 4;
        for (size_t i = 0; i < nibbles; i++) {
            int code = d[i >> 1];
            code = (i & 1) ? (code >> 4) : (code & 0xf);   // low nibble first
            const int step = kStepTable[index];
            int delta = step >> 3;
            if (code & 1) delta += step >> 2;
            if (code & 2) delta += step >> 1;
            if (code & 4) delta += step;
            if (code & 8) {
                sample -= delta;
                if (sample < -32768) sample = -32768;
            } else {
                sample += delta;
                if (sample > 32767) sample = 32767;
            }
            out.push_back((int16_t)sample);
            index += kIndexAdjust[code & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;
        }
        p += chunk;
        remaining -= chunk;
    }
    return out;
}

} // namespace ra2assets
```

- [ ] **Step 4: Run to verify it passes.** From `D:\ra2-android` in PowerShell:

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="bag decode*"
```

Expected: 6 test cases, 0 failures, final line `[doctest] Status: SUCCESS!`. Then run the whole file's cases to confirm nothing regressed:

```powershell
& .\build\ra2assets_tests.exe --test-case="audio idx*,bag decode*"
```

Expected: 11 test cases, 0 failures, `[doctest] Status: SUCCESS!`.

- [ ] **Step 5: Commit.**

```powershell
git add src/audio_bag.cpp tests/test_audio.cpp
git commit -m "feat: decode_bag_clip - chunked IMA-ADPCM + raw-PCM audio.bag clips"
```

---

### Task 4: Real-asset oracle — byte-parity with XCC's own ADPCM decoder

**Files:** Modify: `tests/test_audio.cpp` (append the oracle test). Create: `scripts/make_audio_oracle.md` (in repo), `D:\ra2-re\oracle-tool\xcc_adpcm_dump.cpp` (OUTSIDE the repo — GPL, never committed), oracle data under `D:\ra2-android\oracle\audio\` (git-ignored via the existing `/oracle/` rule in `.gitignore` — verify, do not re-add).

**Interfaces:** Consumes: `MixFile::open(std::vector<uint8_t>)`, `MixFile::open_nested(const std::string&)`, `const MixEntry* MixFile::find(const std::string&)`, `std::vector<uint8_t> MixFile::read(const MixEntry&)` (all existing, `include/ra2assets/mix.hpp`); `AudioIdx` + `decode_bag_clip` (Tasks 2–3); the prebuilt `build\ra2extract.exe` (`--raw` mode); `D:\ra2-re\oracle-tool\xcc_shim.h` (already exists from the Task-9 SHP oracle — provides `typedef unsigned char byte;`). Produces: `oracle/audio/manifest.txt` + `oracle/audio/<name>.pcm` ground truth; the guarded oracle TEST_CASE.

**Speed note:** each `ra2extract` invocation re-reads the outer mix from scratch (~5 s for the 53 MB `language.mix`; ~20 s for `ra2.mix` — not needed here). Batch the extraction: exactly TWO invocations (idx + bag), staged once, then the oracle tool reads the staged files for every clip. Do NOT extract per-clip.

- [ ] **Step 1: Append the failing (skipping) oracle test.** Append to the END of `tests/test_audio.cpp` exactly:

```cpp
// ---------------------------------------------------------------------------
// Guarded real-asset oracle: locate clips through OUR AudioIdx +
// decode_bag_clip from the REAL install files and compare the PCM
// byte-for-byte against ground truth decoded by XCC's own ADPCM code
// (built verbatim, OUTSIDE this repo, into xcc_adpcm_dump -- see
// scripts/make_audio_oracle.md). Skips cleanly if the install or the oracle
// set is absent, so asset-free CI still builds and passes.
//
// oracle/audio/manifest.txt: one clip per line, whitespace-separated:
//   <name> <sample_rate> <flags> <chunk_size>
// oracle/audio/<name>.pcm: raw little-endian int16 interleaved PCM.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> slurp_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("audio bag: real clips match XCC oracle byte-for-byte") {
    const char* od = std::getenv("RA2_ORACLE_DIR");
    fs::path base = od ? od : "D:\\ra2-android\\oracle";
    fs::path manifest = base / "audio" / "manifest.txt";
    if (!fs::exists(manifest)) { MESSAGE("no audio oracle manifest; skipped"); return; }
    const char* id = std::getenv("RA2_INSTALL_DIR");
    fs::path install = id ? id : "D:\\Red Alert 2";
    fs::path lang = install / "language.mix";
    if (!fs::exists(lang)) { MESSAGE("language.mix absent; skipped"); return; }

    auto mixbytes = slurp_file(lang);
    REQUIRE_FALSE(mixbytes.empty());
    MixFile language = MixFile::open(std::move(mixbytes));
    MixFile audio = language.open_nested("audio.mix");
    const MixEntry* ie = audio.find("audio.idx");
    REQUIRE(ie != nullptr);
    auto idx_bytes = audio.read(*ie);
    AudioIdx idx = AudioIdx::load(idx_bytes.data(), idx_bytes.size());
    CHECK(idx.clips().size() > 0);        // 1153 in this install
    const MixEntry* be = audio.find("audio.bag");
    REQUIRE(be != nullptr);
    auto bag = audio.read(*be);

    std::ifstream mf(manifest);
    std::string name;
    uint32_t rate = 0, flags = 0, chunk = 0;
    int clips_checked = 0;
    while (mf >> name >> rate >> flags >> chunk) {
        const BagClip* clip = idx.find(name);
        REQUIRE(clip != nullptr);
        CHECK(clip->sample_rate == rate);   // idx fields match the manifest
        CHECK(clip->flags == flags);
        CHECK(clip->chunk_size == chunk);

        AudioClipInfo info;
        auto pcm = decode_bag_clip(bag.data(), bag.size(), *clip, info);
        CHECK(info.sample_rate == (int)rate);

        // Sample count must match what the format implies.
        if (clip->flags & 0x8u) {
            size_t n_chunks =
                ((size_t)clip->size + clip->chunk_size - 1) / clip->chunk_size;
            CHECK(pcm.size() == ((size_t)clip->size - 4 * n_chunks) * 2 + n_chunks);
        } else {
            CHECK(pcm.size() == (size_t)clip->size / 2);
        }

        auto want = slurp_file(base / "audio" / (name + ".pcm"));
        REQUIRE_FALSE(want.empty());
        REQUIRE(want.size() == pcm.size() * 2);
        bool equal = true;
        for (size_t i = 0; i < pcm.size() && equal; i++) {
            uint16_t u = (uint16_t)pcm[i];
            if (want[2 * i] != (uint8_t)(u & 0xff) ||
                want[2 * i + 1] != (uint8_t)(u >> 8))
                equal = false;
        }
        CHECK(equal);                       // byte-for-byte LE int16 parity
        clips_checked++;
    }
    CHECK(clips_checked > 0);   // a manifest exercising zero clips is a failure
}
```

- [ ] **Step 2: Build and run to verify the clean skip.** From `D:\ra2-android` in PowerShell:

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="audio bag*"
```

Expected: the case runs, prints `MESSAGE: no audio oracle manifest; skipped` (the oracle set doesn't exist yet), 0 failures, `[doctest] Status: SUCCESS!` — this proves the clean-skip path before the oracle exists.

- [ ] **Step 3: Batch-extract the real idx + bag ONCE.** From `D:\ra2-android` in PowerShell (exactly two invocations; ~5 s each):

```powershell
New-Item -ItemType Directory -Force D:\ra2-re\oracle-tool\staging | Out-Null
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.idx D:\ra2-re\oracle-tool\staging\audio.idx
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.bag D:\ra2-re\oracle-tool\staging\audio.bag
```

Expected output: `wrote D:\ra2-re\oracle-tool\staging\audio.idx (41520 bytes)` then `wrote D:\ra2-re\oracle-tool\staging\audio.bag (...)` (~17.6 MB; the exact byte count is printed).

- [ ] **Step 4: Write the oracle tool** at `D:\ra2-re\oracle-tool\xcc_adpcm_dump.cpp` (OUTSIDE the repo — it embeds GPL source) with exactly:

```cpp
// xcc_adpcm_dump.cpp -- standalone audio.idx/audio.bag clip decoder built from
// XCC's OWN unmodified decode source, serving as the byte-for-byte PCM oracle
// for ra2assets::decode_bag_clip (ra2-android audio piece).
//
// GPL NOTICE: This tool embeds source copied verbatim from XCC Utilities,
// which is licensed GPLv3. It therefore lives OUTSIDE the ra2-android repo
// (which is NOT GPL) and no XCC/GPL code is committed there.
//
// PROVENANCE of every copied fragment (local clone D:\ra2-re\reference\xcc):
//   Source repo: https://github.com/OlafvdSpek/xcc
//   - struct t_audio_idx_header / t_audio_idx_entry : misc/cc_structures.h (verbatim)
//   - struct t_ima_adpcm_chunk_header               : misc/riff_structures.h (verbatim)
//   - aud_ima_index_adjust_table, aud_ima_step_table,
//     aud_decode_ima_chunk(...)                     : misc/aud_decode.cpp (verbatim)
//   - the decode DISPATCH and chunk walk mirror misc/xse.cpp Cxse::extract
//     (c_channels = flags&1 ? 2 : 1; flags&2 -> raw 16-bit PCM copied out;
//     else assert(flags&8) -> IMA-ADPCM) and misc/ima_adpcm_wav_decode.cpp
//     Cima_adpcm_wav_decode::load (mono path), kept arithmetically identical
//     -- see the precedence notes at ima_wav_decode_mono below.
//
// usage: xcc_adpcm_dump <audio.idx> <audio.bag> list
//        xcc_adpcm_dump <audio.idx> <audio.bag> <clipname> <out.pcm>
// out.pcm = raw little-endian int16 interleaved PCM, no header.
#include "xcc_shim.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#pragma pack(push, 1)
// ---- verbatim from XCC misc/cc_structures.h ----
struct t_audio_idx_header
{
	__int32 id;
	__int32 two;
	__int32 c_sounds;
};

struct t_audio_idx_entry
{
	char fname[16];
	__int32 offset;
	__int32 size;
	__int32 samplerate;
	__int32 flags;
	__int32 chunk_size;
};
// ---- verbatim from XCC misc/riff_structures.h ----
struct t_ima_adpcm_chunk_header
{
	__int16 sample;
	unsigned __int8 index;
	unsigned __int8 reserved;
};
// ---- end verbatim structs ----
#pragma pack(pop)

// ---- verbatim from XCC misc/aud_decode.cpp ----
const static int aud_ima_index_adjust_table[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

const static int aud_ima_step_table[89] = 
{
	7,     8,     9,     10,    11,    12,     13,    14,    16,
    17,    19,    21,    23,    25,    28,     31,    34,    37,
    41,    45,    50,    55,    60,    66,     73,    80,    88,
    97,    107,   118,   130,   143,   157,    173,   190,   209,
    230,   253,   279,   307,   337,   371,    408,   449,   494,
    544,   598,   658,   724,   796,   876,    963,   1060,  1166,
    1282,  1411,  1552,  1707,  1878,  2066,   2272,  2499,  2749,
    3024,  3327,  3660,  4026,  4428,  4871,   5358,  5894,  6484,
    7132,  7845,  8630,  9493,  10442, 11487,  12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086,  29794, 32767
};

void aud_decode_ima_chunk(const byte* audio_in, short* audio_out, int& index, int& sample, int cs_chunk)
{
	int code;
	int delta;
	int step;
	for (int sample_index = 0; sample_index < cs_chunk; sample_index++)
	{
		code = audio_in[sample_index >> 1];
		code = sample_index & 1 ? code >> 4 : code & 0xf;
		step = aud_ima_step_table[index];
		delta = step >> 3;
		if (code & 1)
			delta += step >> 2;
		if (code & 2)
			delta += step >> 1;
		if (code & 4)
			delta += step;
		if (code & 8)
		{
			sample -= delta;
			if (sample < -32768)
				sample = -32768;
		}
		else
		{
			sample += delta;
			if (sample > 32767)
				sample = 32767;
		}
		audio_out[sample_index] = short(sample);
		index += aud_ima_index_adjust_table[code & 7];
		if (index < 0)
			index = 0;
		else if (index > 88)
			index = 88;  
	}
}
// ---- end verbatim aud_decode.cpp fragments ----

// Mirror of Cima_adpcm_wav_decode::load (misc/ima_adpcm_wav_decode.cpp),
// MONO path only (both oracle clips are mono; stereo ADPCM is out of the
// oracle's scope). XCC's expressions, with the C++ precedence they actually
// have made explicit (<< binds looser than binary -):
//   c_chunks   = (cb_s + chunk_size - 1) / chunk_size
//   mc_samples = ((cb_s - 4*c_chunks) << 1) + c_chunks
//   cs_chunk   = min(cs_remaining, (chunk_size - 4) << 1)
// The only mechanical change is memcpy instead of reinterpret_cast for the
// chunk-header read (alignment hygiene); values are identical.
static std::vector<short> ima_wav_decode_mono(const byte* r, int cb_s, int chunk_size)
{
	int c_chunks = (cb_s + chunk_size - 1) / chunk_size;
	int mc_samples = ((cb_s - (int)sizeof(t_ima_adpcm_chunk_header) * c_chunks) << 1) + c_chunks;
	std::vector<short> out((size_t)mc_samples);
	short* w = out.data();
	int cs_remaining = mc_samples;
	while (cs_remaining)
	{
		t_ima_adpcm_chunk_header chunk_header;
		std::memcpy(&chunk_header, r, sizeof chunk_header);
		r += sizeof(t_ima_adpcm_chunk_header);
		*w++ = chunk_header.sample;
		cs_remaining--;
		int cs_chunk = std::min(cs_remaining,
			(chunk_size - (int)sizeof(t_ima_adpcm_chunk_header)) << 1);
		int index = chunk_header.index;
		int sample = chunk_header.sample;
		aud_decode_ima_chunk(r, w, index, sample, cs_chunk);
		r += cs_chunk >> 1;
		w += cs_chunk;
		cs_remaining -= cs_chunk;
	}
	return out;
}

static std::vector<unsigned char> slurp(const std::string& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), {});
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::fprintf(stderr,
			"usage: xcc_adpcm_dump <audio.idx> <audio.bag> list\n"
			"       xcc_adpcm_dump <audio.idx> <audio.bag> <clipname> <out.pcm>\n");
		return 2;
	}
	std::vector<unsigned char> idx = slurp(argv[1]);
	std::vector<unsigned char> bag = slurp(argv[2]);
	if (idx.size() < sizeof(t_audio_idx_header))
	{
		std::fprintf(stderr, "error: idx too small (%zu bytes)\n", idx.size());
		return 1;
	}
	t_audio_idx_header h;
	std::memcpy(&h, idx.data(), sizeof h);
	// Caudio_idx_file::is_valid (misc/audio_idx_file.h): id == "GABA",
	// two == 2, header + c_sounds*entry == file size exactly.
	if (std::memcmp(&h.id, "GABA", 4) != 0 || h.two != 2 ||
		sizeof(t_audio_idx_header) + sizeof(t_audio_idx_entry) * (size_t)h.c_sounds != idx.size())
	{
		std::fprintf(stderr, "error: audio.idx failed XCC is_valid check\n");
		return 1;
	}
	const t_audio_idx_entry* entries =
		reinterpret_cast<const t_audio_idx_entry*>(idx.data() + sizeof(t_audio_idx_header));

	const std::string want = argv[3];
	if (want == "list")
	{
		for (int i = 0; i < h.c_sounds; i++)
		{
			const t_audio_idx_entry& e = entries[i];
			char name[17]; std::memcpy(name, e.fname, 16); name[16] = 0;
			std::printf("%-16s offset=%-10d size=%-8d rate=%d flags=%d chunk=%d\n",
				name, e.offset, e.size, e.samplerate, e.flags, e.chunk_size);
		}
		return 0;
	}
	if (argc < 5) { std::fprintf(stderr, "need <out.pcm>\n"); return 2; }

	const t_audio_idx_entry* e = 0;
	for (int i = 0; i < h.c_sounds; i++)
	{
		char name[17]; std::memcpy(name, entries[i].fname, 16); name[16] = 0;
		if (want == name) { e = &entries[i]; break; }
	}
	if (!e) { std::fprintf(stderr, "error: clip '%s' not in idx\n", want.c_str()); return 1; }
	if ((size_t)e->offset + (size_t)e->size > bag.size())
	{
		std::fprintf(stderr, "error: clip range exceeds bag\n");
		return 1;
	}
	const byte* s = bag.data() + e->offset;

	// Dispatch mirrors Cxse::extract (misc/xse.cpp):
	//   c_channels = e.flags & 1 ? 2 : 1
	//   flags & 2  -> raw 16-bit PCM copied out unchanged
	//   else       -> assert(flags & 8); IMA-ADPCM via Cima_adpcm_wav_decode
	int c_channels = e->flags & 1 ? 2 : 1;
	std::vector<short> pcm;
	if (e->flags & 2)
	{
		pcm.resize((size_t)e->size / 2);
		std::memcpy(pcm.data(), s, (size_t)e->size / 2 * 2);
	}
	else
	{
		if (!(e->flags & 8))
		{
			std::fprintf(stderr, "error: clip flags %d unsupported\n", e->flags);
			return 1;
		}
		if (c_channels != 1)
		{
			std::fprintf(stderr, "error: stereo ADPCM not needed for the oracle set\n");
			return 1;
		}
		pcm = ima_wav_decode_mono(s, e->size, e->chunk_size);
	}

	std::ofstream out(argv[4], std::ios::binary);
	if (!out) { std::fprintf(stderr, "error: cannot write %s\n", argv[4]); return 1; }
	// x86 is little-endian: writing the short array raw emits LE int16 PCM.
	out.write(reinterpret_cast<const char*>(pcm.data()),
	          (std::streamsize)(pcm.size() * 2));
	std::printf("%s: flags=%d chunk=%d rate=%d -> %s (%d samples, %d bytes)\n",
		want.c_str(), e->flags, e->chunk_size, e->samplerate, argv[4],
		(int)pcm.size(), (int)(pcm.size() * 2));
	return 0;
}
```

- [ ] **Step 5: Build the tool** (PowerShell; static-linked so it runs without MinGW DLLs on PATH; `xcc_shim.h` already sits in that directory from the Task-9 SHP oracle):

```powershell
Set-Location D:\ra2-re\oracle-tool
g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ xcc_adpcm_dump.cpp -o xcc_adpcm_dump.exe
```

Expected: compiles with no errors (warnings about unused variables are acceptable — the code is verbatim XCC); `xcc_adpcm_dump.exe` appears.

- [ ] **Step 6: Generate the oracle set** (PowerShell). List first to confirm the entry names, then decode the two chosen clips — `ichratta` (the idx's first entry; 71,148 bytes, 139 chunks — exercises the multi-chunk path hard) and `gradio3f` (512 bytes — exactly one whole chunk):

```powershell
Set-Location D:\ra2-re\oracle-tool
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag list | Select-Object -First 3
New-Item -ItemType Directory -Force D:\ra2-android\oracle\audio | Out-Null
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag ichratta D:\ra2-android\oracle\audio\ichratta.pcm
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag gradio3f D:\ra2-android\oracle\audio\gradio3f.pcm
```

Expected: the list's first line is `ichratta         offset=0          size=71148   rate=22050 flags=12 chunk=512`; the decode lines end `(141323 samples, 282646 bytes)` for ichratta and `(1017 samples, 2034 bytes)` for gradio3f (these counts follow from `(size - 4*n_chunks)*2 + n_chunks`).

- [ ] **Step 7: Write the manifest** `D:\ra2-android\oracle\audio\manifest.txt` (git-ignored — the whole `oracle/` tree is derived from EA-owned assets and is never committed) with exactly:

```
ichratta 22050 12 512
gradio3f 22050 12 512
```

- [ ] **Step 8: Run the oracle test to verify it passes.** From `D:\ra2-android` in PowerShell:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="audio bag*"
```

Expected: no skip MESSAGE this time; 1 test case, 0 failures, `[doctest] Status: SUCCESS!` (the case slurps the 53 MB `language.mix` — allow a couple of seconds).

- [ ] **Step 9: Document the generation** in `scripts/make_audio_oracle.md` with exactly:

````markdown
# Generating the byte-for-byte audio (BAG/ADPCM) oracle

The oracle test (`tests/test_audio.cpp`, case "audio bag: real clips match
XCC oracle byte-for-byte") compares `ra2assets`' PCM output against ground
truth decoded by **XCC's own IMA-ADPCM code**. The oracle set lives in the
git-ignored `oracle/audio/` directory and is **never committed** (derived
from EA's game assets). Same Task-9 pattern as `make_oracle.md`.

> Extraction vs decode: the idx/bag bytes are extracted by **ra2assets**
> (`ra2extract --raw`); the decode of clip bytes into PCM is done by **XCC's
> code**. The byte-for-byte gate therefore validates *our decoder* against
> XCC's, holding extraction constant.

## 1. Extract the real idx + bag ONCE with ra2assets

Each `ra2extract` invocation re-reads `language.mix` (~5 s) — batch these,
never per-clip:

```powershell
Set-Location D:\ra2-android
New-Item -ItemType Directory -Force D:\ra2-re\oracle-tool\staging | Out-Null
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.idx D:\ra2-re\oracle-tool\staging\audio.idx
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.bag D:\ra2-re\oracle-tool\staging\audio.bag
```

Expected: `audio.idx (41520 bytes)`, `audio.bag` ~17.6 MB.

## 2. Build the standalone oracle tool (OUTSIDE this repo — XCC is GPLv3)

`D:\ra2-re\oracle-tool\xcc_adpcm_dump.cpp` embeds, verbatim and with
provenance comments, from `D:\ra2-re\reference\xcc`:
`t_audio_idx_header`/`t_audio_idx_entry` (`misc/cc_structures.h`),
`t_ima_adpcm_chunk_header` (`misc/riff_structures.h`), and the step/index
tables + `aud_decode_ima_chunk` (`misc/aud_decode.cpp`); its dispatch and
chunk walk mirror `Cxse::extract` (`misc/xse.cpp`) and
`Cima_adpcm_wav_decode::load` (`misc/ima_adpcm_wav_decode.cpp`).

```powershell
Set-Location D:\ra2-re\oracle-tool
g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ xcc_adpcm_dump.cpp -o xcc_adpcm_dump.exe
```

## 3. Dump the oracle PCM with XCC's decoder

```powershell
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag list          # inspect entries
New-Item -ItemType Directory -Force D:\ra2-android\oracle\audio | Out-Null
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag ichratta D:\ra2-android\oracle\audio\ichratta.pcm
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag gradio3f D:\ra2-android\oracle\audio\gradio3f.pcm
```

Expected: `ichratta` → 141,323 samples (282,646 bytes); `gradio3f` → 1,017
samples (2,034 bytes). `<name>.pcm` files are raw little-endian int16 mono
PCM, no header.

## 4. Write the manifest

`oracle/audio/manifest.txt` — one clip per line, whitespace-separated
`<name> <sample_rate> <flags> <chunk_size>`:

```
ichratta 22050 12 512
gradio3f 22050 12 512
```

The test skips cleanly (doctest MESSAGE) when the manifest or
`language.mix` is absent, and fails if a present manifest exercises zero
clips (`clips_checked > 0` guard).
````

- [ ] **Step 10: Verify nothing under `oracle/` or `D:\ra2-re` is staged, then commit.**

```powershell
Set-Location D:\ra2-android
git status --short
git add tests/test_audio.cpp scripts/make_audio_oracle.md
git commit -m "test: audio oracle - real BAG clips byte-identical to XCC ADPCM decode"
```

Expected: `git status` shows no `oracle/` paths (the `/oracle/` gitignore rule covers them); only the two listed files are committed.

---

### Task 5: Finalize — RE confidence table check, README, full suite

**Files:** Modify: `README.md` (one line), `docs/re/audio.md` (only if Task 1 left any `«...»` token unresolved). No code changes.

**Interfaces:** Consumes: everything above. Produces: the finished sub-project.

- [ ] **Step 1: Verify `docs/re/audio.md` is complete.** Open it and confirm no `«...»` fill-token remains and the Confidence table has a High/Med/Low rating in every row. If any hunt row still has a token, resolve it now from `D:/ra2-re/out/audio_hunt.log` (or set that row to `not located — search documented in this section` / `Low` per Task 1 Step 4's fallback rule) and confirm the two "Decisions" bullets (no headered `.aud`; stereo ADPCM throws) are present.

- [ ] **Step 2: Add the README mention.** In `D:\ra2-android\README.md`, replace the line:

```
rules, `.map` cell grids, TMP theater tiles), plus `ra2engine` — an SDL2
```

with:

```
rules, `.map` cell grids, TMP theater tiles, BAG/IDX audio decoded from
Westwood IMA-ADPCM to PCM), plus `ra2engine` — an SDL2
```

- [ ] **Step 3: Verify the CMake wiring is in place.** Confirm `cmake/ra2_sources.cmake` contains the line `${RA2_ROOT}/src/audio_bag.cpp` inside `RA2_LIB_SOURCES`, and `CMakeLists.txt` contains `target_sources(ra2assets_tests PRIVATE tests/test_audio.cpp)` (both added in Task 2 — this step is a check, not an edit; fix them if missing).

- [ ] **Step 4: Run the FULL suite.** From `D:\ra2-android` in PowerShell:

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected: every pre-existing test still passes plus the 12 new audio cases (11 synthetic + 1 oracle); output is pristine (the only MESSAGE lines are other suites' skip notices if their oracle sets are absent); final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 5: Commit.**

```powershell
git add README.md docs/re/audio.md
git commit -m "docs: audio decoder - README mention + RE confidence table finalized"
```
