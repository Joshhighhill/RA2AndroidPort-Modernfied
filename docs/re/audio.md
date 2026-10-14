# Audio (audio.idx / audio.bag + IMA-ADPCM) — reverse-engineering notes

Foundation-batch sub-project 3. Method per `docs/re/workflow.md`; image base
`0x00400000`. Hunt scripts: `D:/ra2-re/scripts/audio_dump.py` (constant-table
anchoring for the IMA step/index tables), `audio_dump2.py` (`.idx`/`.bag`
extension strings + `AUDIO`/`AUDIOMD` strings), `audio_dump3.py` (raw
disassembly window over a Ghidra function-boundary gap, plus the shared
CSF-comparator cross-reference), `audio_dump4.py` (codec-dispatch caller).
Outputs under `D:/ra2-re/out/`, nothing committed. The correctness gate is
twofold: the located game decoder below, and the Task-4 byte-parity oracle
against XCC's own ADPCM decode (`tests/test_audio.cpp`, oracle generation in
`scripts/make_audio_oracle.md`).

## Scope decision: no headered `.aud` support

This install (YR 1.001, `gamemd.exe` SHA1
`7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`) ships **zero** headered `.aud`
files (verified 2026-07-09). SFX/voices live in
`language.mix → audio.mix → audio.idx + audio.bag`; EVA speech and music are
RIFF WAVs (`fmt` 0x0011, same IMA codec — trivial container, deferred to the
audio phase; see "A second, generic IMA codec" below for its game-side
counterpart). Headered-AUD and WS-ADPCM ("type 1") are therefore **not
implemented** — asset-less here.

## audio.idx (`GABA`) layout — confirmed, with a bonus legacy variant found

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

The located loader (`FUN_004011c0`, see below) reveals the game itself
supports **two on-disk record widths**, keyed off the header's `version`
field: `version==1` reads legacy 32-byte records (no `chunk_size`) and
**zero-fills** a 4th dword after each one; any other version (our real
file's `version=2`) bulk-reads the full 36-byte records directly. This is a
previously-undocumented confirmation that `chunk_size` is exactly the field
that was *added* when the format went from 32→36 bytes — not something we
had independent evidence for before this hunt.

## Real-install census (all 1153 entries)

All 22,050 Hz. Flags histogram: `12` ×1126 (chunk 512), `6` ×21 (chunk 0),
`7` ×4 (chunk 0), `13` ×2 (chunk 1024: `uslide1`, `uslide2`). The flags-6
(mono raw PCM) entries are loop/ambient sounds (`icraloop`, `vblhlo2a-c`,
`vkirlo1/2a-c`, `schrlo1/2a-c`, `gcraprom`, `vcoplo1/2a-b`, `gpsyampa-c`,
`ubase`, `utext`); flags-7 (stereo raw PCM) are `uscolo1/2a-c`.

Flag bits (XCC `xse.cpp` `Cxse::extract`/`insert`, corroborated by the
chunk_size correlation above and, at the game side, by the format-type
dispatch in `FUN_00409c40` — see "Flag/format-type dispatch" below): bit 0
(0x1) stereo; bit 1 (0x2) uncompressed 16-bit LE PCM; bit 2 (0x4) set on
every entry (16-bit marker, no decode effect); bit 3 (0x8) IMA-ADPCM.

## BAG ADPCM payload: chunked, with per-chunk 4-byte headers — confirmed directly in the game decoder

A flags-bit-3 clip is `chunk_size`-byte chunks; each starts
`{i16 predictor; u8 step_index; u8 reserved}` — the predictor is emitted
verbatim as the chunk's first sample, then `(chunk_bytes-4)*2` nibbles decode
**low nibble first**. Predictor/index restart from each chunk header (NOT
fixed 0/0 per clip; nothing carries across chunks). Mono sample count:
`(size - 4*ceil(size/chunk_size))*2 + ceil(size/chunk_size)` — e.g.
`ichratta` (71,148 B, 139 chunks) → 141,323 samples. This matches XCC's
`Cima_adpcm_wav_decode::load`, which XCC's own BAG extractor calls, and is
byte-parity-verified by the Task-4 oracle.

The un-named chunk-decoder located at `0x0040aa70` (see below) confirms every
clause of this model bit-for-bit: it reads `num_channels` consecutive 4-byte
headers at the top of *every call* into a per-channel state array, fully
overwriting the persisted `{predictor, index}` pair before any nibble is
decoded — i.e. state genuinely restarts from the header each call, nothing
carries across chunks. It goes further than our prior model in two ways
worth recording: (1) it **validates** the header rather than merely reading
it — `step_index > 88` or `reserved != 0` aborts the whole chunk decode
(returns failure) instead of silently clamping; (2) it **generalizes to N
channels** — for `num_channels > 1` it reads one 4-byte header per channel
(back-to-back, not interleaved with body data) and then decodes the nibble
body in per-channel sub-blocks of 4 source bytes (8 nibbles) at a time,
cycling through channels in order and writing to a fully-interleaved 16-bit
output buffer. Real BAG assets never exercise this (every ADPCM entry in the
census is mono), so it does not change the mono decode path we implement,
but it is the concrete shape any future stereo-ADPCM support would need to
match.

## Per-nibble arithmetic — confirmed bit-for-bit against the game decoder

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
`entry[88]==32767`, and the index-adjust image `-1,-1,-1,-1,2,4,6,8` — found
the tables and the single-nibble decode primitive on the first pass. The
literal strings `audio.idx`/`audio.bag`/`AUDIO.IDX`/`AUDIO.BAG` are **not
present anywhere in this binary** (0 hits, all four case variants) — same
"no full-name literal" pattern documented for `.vxl`/`.hva` in `vxl.md`. A
follow-up string hunt for the bare extensions and `AUDIO`/`audio` fragments
found the real anchors: exact-case literals `.idx` / `.bag` (extension-append
construction, exactly like `DPOD.VXL`/`DPOD.HVA` in `vxl.md`) and
`AUDIO.MIX` / `AUDIOMD.MIX` (the containing MIX archive names).

### IMA step/index tables and the single-nibble decode primitive

- **Step table:** `0x00815558` (width i32), 89 entries, `entry[88]==32767`.
  Confirmed a second, independent copy at `0x00874864` (width i16) used by a
  different, generic IMA codec (see "A second, generic IMA codec" below).
- **Index-adjust table:** `0x00815518` (width i32) — the classic **16-entry**
  IMA `IndexTable` (`-1,-1,-1,-1,2,4,6,8` twice, i.e. the pattern search hit
  it at both `0x00815518` and `0x00815538 = 0x00815518+0x20`), which sits
  immediately before the step table: `0x00815518 + 16*4 = 0x00815558`. Same
  layout for the second copy: `0x00874844` (i16) + `16*2 = 0x00874864`.
- **Decode primitive: `FUN_0040acd0`** — fastcall, taking a code value and a
  pointer to a two-int state, where `state[0]` = predictor/sample, `state[1]` = index.
  Per-nibble shape **matches the ground-truth block above exactly**:
  `step = step_table[state[1]]`; `delta = step>>3`, `+= step>>2` if bit0,
  `+= step>>1` if bit1, `+= step` if bit2, negated (not added) if bit3;
  `sample = state[0]+delta` clamped to `[-32768,32767]` via a straight
  `< 0x8000` / `< -0x8000` / else `0x7fff` compare (bit-for-bit the
  documented clamp); `state[1] += index_table[code & 0xff]` (mask is a
  no-op given real nibble values are always 0-15) clamped to `[0,88]` via
  `< 0` → 0 / `> 0x58` → `0x58`. Returns the (clamped) sample in the low
  16 bits of EAX. This is the single most literal 1:1 match in the whole
  hunt.
- **Nibble order: low-first, confirmed directly.** Every call site does
  `AND CL,0xf` → `CALL FUN_0040acd0` (low nibble) **then**
  `SHR CL,0x4` → `CALL FUN_0040acd0` (high nibble) — exactly the documented
  order, read straight out of the disassembly, not inferred.
- **Chunk decoder: address `0x0040aa70`** (see next section for why it has
  no `FUN_` name). This is the function that owns the `{predictor, index}`
  state array (at `this+0xb4`, 8 bytes/channel: i32 predictor + i32 index),
  reads the chunk header(s), and drives the nibble loop that calls
  `FUN_0040acd0` per nibble.

### A Ghidra function-boundary gap, and how it was worked around

`getReferencesTo` on the index-adjust table turned up call sites at
`0x0040ab86`/`0x0040ab9c`/`0x0040ac39`/`0x0040ac53`, but
`getFunctionContaining` returned `None` for all four — a genuine analysis
gap of exactly the kind `docs/re/workflow.md` §4 warns about ("expect to
`createFunction()` or region-scan where auto-analysis left gaps"), not a
tooling mistake on our part (`getFunctionAt` on several guessed entries in
the region also came back `None`). Rather than mutate the read-only project,
`audio_dump3.py` dumped a raw disassembly listing of `0x0040aa00`–`0x0040ad00`
and the function was read directly from assembly (16-byte-aligned entry at
`0x0040aa70`, immediately after the previous function's `RET` at
`0x0040aa63` + padding). A later pass (`audio_dump4.py`) found this address
referenced as **DATA** (a function-pointer store, not a `CALL`) from
`FUN_00409c40` — i.e. it is installed as a callback, which is exactly why no
ordinary `CALL`-based caller exists and reinforces that the gap is a
legitimate, indirectly-invoked function rather than dead/misidentified code.

### `audio.idx`/`audio.bag` loader

- **`FUN_00406b10`** — audio subsystem bootstrap. Opens the containing MIX
  (`FUN_005b3650`, the `MixFileClass` ctor documented in `mix.md`) with
  either `"AUDIO.MIX"` (string `0x00815274`) or `"AUDIOMD.MIX"` (string
  `0x00815280`) depending on an expansion/version probe, then calls
  `FUN_004011c0()` to open the idx/bag pair inside it.
- **`FUN_004011c0`** (`char *base, char *override_path`) — the GABA loader/
  ctor. Builds `"<base>.idx"` by truncating at the last `.` (`strrchr`) and
  appending the literal string `".idx"` (`0x00814e80`); on success, repeats
  for `".bag"` (`0x00814e78`, found immediately preceding `.idx` in memory —
  same extension-append pattern `vxl.md` documented for `DPOD.VXL`/`.HVA`).
  Reads exactly `0xc` (12) bytes into `{magic[4]; version; count}` and
  **never compares the magic bytes to anything** — only the read-length
  (`==12`) gates success. `count` sizes an allocation of `count*0x24` (36)
  bytes; the version-1-vs-other branch described above selects 32- or
  36-byte on-disk records. After loading, it sorts the entries with
  `FUN_007c7948(entries, count, 0x24, FUN_007c7b20)` — a CRT-shaped `qsort`
  with stride `0x24` (36, confirming the entry size independently of the
  header table) using **`FUN_007c7b20`, the exact same case-fold comparator
  csf.md already identified** for `StringTable`'s label sort/lookup.
- **`FUN_004015c0`** — the name→index lookup: `bsearch(key, entries, count,
  0x24, FUN_007c7b20)`, same comparator, returning `(found-base)/0x24` or
  `-1`. Since `FUN_007c7b20` is the `_stricmp`-shaped case-fold comparator
  documented in `csf.md` (fold-in-comparator, not normalize-at-insert),
  **`AudioIdx::find` is case-insensitive by the identical mechanism as CSF
  label lookup** — not a fresh guess, a second confirmed instance of a
  comparator this project has already fully dissected.

### Flag/format-type dispatch

**`FUN_00409c40`** reads a per-clip "format descriptor" (`format_type` at
`+4`, a sample-rate-like field at `+0xc`, `chunk_size` at `+0x18`) and
branches on `format_type`: `0` → a different codec entry point
(`FUN_00409d90`, not chased — out of scope, no census entries need it);
`1` → installs **our located chunk decoder, `0x0040aa70`**, as the per-chunk
decompression callback and sizes the output buffer as
`chunk_size*4 + 0x80` bytes; `2` → a generic raw-copy path with no
decompression callback at all. The `*4` output-sizing constant is an exact,
independent confirmation of the ADPCM expansion ratio our decoder relies on
(1 source byte = 2 nibbles = 2 int16 samples = 4 output bytes). The precise
computation that turns the raw 36-byte entry's `flags` field into this
`format_type` enum was not traced to a specific function within budget — an
honest secondary gap; the flag-bit table above still rests primarily on the
XCC + census correlation, now with this dispatch as corroborating (not
definitive) game-side evidence that "ADPCM" is a single discrete branch,
consistent with a single flag bit driving it.

### A second, generic IMA codec (out of BAG scope, corroborating)

**`FUN_007c50ac`** is a separate, self-contained IMA decoder (its own step
table `0x00874864`/index table `0x00874844`, both independently confirmed by
the same `entry[88]==32767` check) that handles mono/stereo and 8-/16-bit
output in one function, with the identical arithmetic cascade inlined
(`step>>3` base + conditional `step>>2`/`step>>1`/`step`, negate on bit 3,
clamp to `[-32768,32767]`/index `[0,88]`) rather than calling a shared
primitive. No direct `CALL`-based reference to it was found (consistent with
it being reached through a codec vtable/function-pointer table, the same
pattern noted for `0x0040aa70` above). This is almost certainly the RIFF WAV
(`fmt` 0x0011) IMA codec the scope-decision section defers — same math,
different container, out of scope for the BAG decoder itself, but useful
confirmation that the canonical IMA tables and arithmetic are used
uniformly across the engine, not just in the BAG path.

## Confidence

| Item | Evidence | Confidence |
|------|----------|:----------:|
| idx layout (12 + n*36, field order) | exact size arithmetic on the real file + XCC structs + loader dump `FUN_004011c0` (12-byte header read, `qsort` stride `0x24`, and the version-1 32-byte legacy branch that pins down `chunk_size` as the added 4th field) | **High** |
| flags bit semantics | XCC extract/insert + census correlation + game-side format-type dispatch (`FUN_00409c40`) corroborating a single discrete ADPCM branch and the exact `*4` expansion-ratio output sizing, though the flags→format_type bit computation itself wasn't traced | **Medium-High** |
| ADPCM tables (step/index-adjust) | located at `0x00815558`/`0x00815518` (i32, BAG decoder) and independently at `0x00874864`/`0x00874844` (i16, generic codec) — both bit-identical per the `entry[88]==32767` check | **High** |
| per-nibble arithmetic + clamps | `FUN_0040acd0` decompiles to a literal 1:1 match of the documented cascade and clamps; independently re-confirmed by the differently-coded `FUN_007c50ac` | **High** |
| chunked model (4-byte chunk headers) | the chunk decoder at `0x0040aa70` (recovered from disassembly across a Ghidra function-boundary gap) reads and validates the header, fully overwrites persisted state every call, and generalizes cleanly to N channels — direct, not inferred | **High** |

Divergences found (both are refinements, not contradictions):
1. The game **validates** chunk headers (`step_index<=88`, `reserved==0`)
   and aborts the whole chunk on violation, rather than silently clamping.
2. The chunk decoder supports **N-channel headers and sub-block-interleaved
   nibble bodies** generally; real BAG assets are all mono so this never
   fires, but a future stereo-ADPCM implementation should match this exact
   shape (per-channel 4-byte headers back-to-back, then per-channel 4-byte/
   8-nibble sub-blocks cycling channel-by-channel into an interleaved
   16-bit output), not simple byte-interleaving.

A `Low` row would mean the hunt failed to locate that item in the binary;
none of the rows above are Low — every element of the ground-truth model was
found and confirmed directly in `gamemd.exe`. The Task-4 oracle (byte-for-
byte PCM parity on real clips) still gates decoder correctness end-to-end.

## Decisions

- Headered `.aud`: **not implemented** (no assets in this install).
- Stereo IMA-ADPCM (flags 13; only `uslide1`/`uslide2`): **unimplemented —
  `decode_bag_clip` throws** with a message pointing here. Stereo raw PCM
  (flags 7) is passthrough and implemented. The game-side N-channel chunk
  shape is now documented above for whoever picks this up.
- `AudioIdx::find` is ASCII case-insensitive (INI sound keys are mixed-case)
  — confirmed via the same shared CRT comparator csf.md already documented
  for `StringTable`, not merely retained on prior assumption.
