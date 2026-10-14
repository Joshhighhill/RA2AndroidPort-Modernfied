# MIX format — reverse-engineering notes

This file records the Blowfish key-derivation port provenance (Task 2) and,
appended at the end, the **Task 11 Ghidra location** of the MIX loader and the
`mix_id` hash inside the real `gamemd.exe`. See `docs/re/workflow.md` for the
toolchain and method.

## Key derivation port provenance

`ra2assets::detail::westwood_blowfish_key` (in `src/mix_keyderiv.cpp`) turns the
80-byte key-source block at the start of an encrypted MIX header into the
56-byte Blowfish key. As of the "clean-room reimplementation of key derivation"
change it is an **independent clean-room reimplementation** written from the
public RSA public-key algorithm and the publicly documented MIX framing.

**The prior GPLv3 dependency has been removed.** The file used to be a verbatim
port of XCC Utilities' `misc/mix_decode.cpp` (GPLv3), which made the whole repo
a GPL derivative and conflicted with the goal of a cleanly-licensable library.
The reimplementation was authored **without opening, transcribing, or
paraphrasing** the XCC source, OpenRA's `BlowfishKeyProvider.cs`, or the prior
port's bignum routines. It shares none of their function decomposition, naming,
or code expression: the new code uses generic big-integer helpers
(`multiply`/`modulo`/`mod_pow` over `uint32_t` limbs with bitwise long-division
reduction, no 16-bit-word type-punning and no Montgomery form) and standard RSA
modular exponentiation. Standard algorithms (RSA, base64, bignum modexp) are not
copyrightable and were written fresh.

### Public key constant

The one value shared with the references is the published Westwood MIX public
key — this is interoperability **data** (a fixed key baked into the shipped
game binary; it is present in `gamemd.exe`'s `.rdata`, see Task 11 below), not
source code:

```
AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V
```

### Algorithm summary (as reimplemented)

1. Base64-decode the public-key string into a 42-byte DER-style blob. It is a
   single INTEGER: tag byte `0x02`, one length byte `0x28` (= 40), then 40
   big-endian content bytes = the RSA modulus `n` (a 319-bit number). The public
   exponent is the standard `e = 0x10001` (65537).
2. Split the 80-byte key source into two 40-byte blocks. Read each block as a
   **little-endian** integer `m` and compute `c = m^e mod n` by square-and-
   multiply.
3. Emit the low **39** bytes of each `c` in little-endian order. Concatenating
   the two blocks' outputs yields 78 bytes; the first **56** are the Blowfish
   key. (39+39 ≥ 56, so block 0 contributes 39 bytes and block 1 the next 17.)

### Reentrancy / build flags

- The implementation holds **no file-scope mutable state** — all working
  buffers are local to the call — so it is thread-safe by construction. The
  `std::mutex` guard that the old module-global-scratch version required (added
  in commit `497bb18`) has been removed.
- Byte handling uses `uint8_t` throughout with no `*(T*)` type-punning, so the
  per-file `-fno-strict-aliasing;-fsigned-char` compile options this TU used to
  need were removed from `CMakeLists.txt`. Verified to build clean at `-O2` on
  the host (MinGW g++) and for Android `arm64-v8a` (NDK r27c, Clang, where plain
  `char` is unsigned) with those flags gone.

### Correctness gate

Two committed known-answer vectors (`tests/test_mix.cpp`,
"westwood key derivation reproduces golden vectors") pin the exact 56-byte
output for the real `ra2md.mix` and `ra2.mix` 80-byte key sources — these are
black-box input/output pairs (no GPL text) and are byte-exact. Real end-to-end
correctness is additionally proven in **Task 4**, where a genuine encrypted MIX
header must decrypt to a sane index.

## Task 4: real-archive validation (`ra2md.mix`)

`tests/test_mix.cpp` ("mix opens a real encrypted archive from the install")
opens the real `ra2md.mix` from `RA2_INSTALL_DIR` (204,527,696 bytes) end to
end: encrypted-flag detection, key-source read, `westwood_blowfish_key`
derivation, Blowfish-decrypt of the header+index blocks, and index parsing.
The decrypted index is internally consistent — every entry's `offset + size`
falls within the file body (verified via `MixFile::read()` on all entries) —
which is the strongest signal available right now that Task 2's ported key
derivation is correct, since a wrong key produces either the
`mix: encrypted index exceeds file (wrong key derivation?)` throw or a
garbage/non-plausible entry count, not a clean, self-consistent index.

**Observed for `ra2md.mix`:** 25 entries (`encrypted=1`, `checksummed=1`).
IDs (hex, as stored — i.e. `mix_id`-hashed internal filenames of the nested
mixes, not human-readable names):

```
0x8C8CCA19 0xA01A9A03 0xA9817776 0xA9FED5B2 0xBA5EA181 0xC6B134FB 0xC93B27A0
0xCADDCD81 0xCD50B6E6 0xD77312D5 0xDC0084EB 0xDF966243 0xEB108109 0xF5590C4A
0xFBE0D09D 0xFF6808D5 0x0BB7A3BC 0x1D8D0CAB 0x3BFB683C 0x4DE6A424 0x51FC6863
0x68EEC99A 0x763C81DD 0x78C2BB3F 0x7DAA6FA4
```

As an extra (unrecorded-as-test) check, `ra2.mix` (281,074,192 bytes, also
`encrypted=1`) was opened the same way and yielded 21 entries, all
bounds-valid — confirming the encrypted-header branch generalizes beyond the
one archive under test.

**Deferred to Task 9:** cross-checking this entry list (count + IDs) against
XCC Mixer's view of `ra2md.mix` — XCC Mixer is not installed in this
environment yet. Task 5's well-known-name resolution (mapping `mix_id` hashes
back to filenames like `english.mix`, `local.mix`, etc.) will independently
corroborate these IDs before the XCC cross-check happens.

### Update (Task 9): the XCC cross-check is now SUPERSEDED

The deferred "compare our entry list against XCC Mixer's view" cross-check is
**superseded by the stronger Task 9 byte-level oracle gate**
(`tests/test_oracle.cpp`, see `docs/re/shp.md`). Rather than merely confirming
that XCC sees the same *entries*, the oracle feeds our extracted entry bytes
(via `ra2extract --raw`) directly into **XCC's own decoder** and asserts the
decoded output is **byte-identical** across 2044 real frames from `ra2.mix`.
Byte-for-byte agreement on the decoded pixels is only possible if the entire
extraction chain — encrypted-header decrypt, `mix_id` resolution, offset/size
tiling, nested-mix descent — produced exactly the correct bytes for each
asset. This subsumes the entry-list comparison, so no separate XCC-Mixer
entry cross-check is needed.

---

# Task 11 — Ghidra location in `gamemd.exe` (YR 1.001)

Image base `0x00400000`. Addresses below are Ghidra addresses in this build
(YRpp's published addresses drifted — see `workflow.md` §4). Decompiled
excerpts are lightly edited (`FUN_` names kept) for illustration; full dumps
live outside the repo under `D:\ra2-re\out\`.

## `mix_id` — the filename hash (CRC32). Confidence: **HIGH**

The hash is a **standard reflected CRC32** (poly `0xEDB88320`), confirmed three
independent ways:

1. **The table.** A 256-entry table at `0x0081E7C4` reads
   `00000000 77073096 EE0E612C 990951BA 076DC419 706AF48F E963A535 9E6495A3 …`
   — the canonical reflected-CRC32 table, byte-identical to what our bitwise
   `mix_id` inner loop computes. `getReferencesTo(0x81E7C4)` → exactly the four
   CRCEngine methods (`FUN_004a1b20/1bb0/1cf0/1ec0`).

2. **The inner loop** (`FUN_004a1ec0`, YRpp `CRCEngine::Memory`):

   Arguments are (data pointer, byte count, running crc). It inverts the
   incoming crc, then for each input byte replaces `crc` with
   `(crc >> 8) XOR table[(crc & 0xFF) XOR byte]`, where `table` is the 256
   dword table at `0x81E7C4`, and finally returns the inverted crc.
   This is exactly our `crc = (crc>>1)^(0xEDB88320 & -(crc&1))` bit-loop's
   table-driven equivalent (init/final `~`, LSB-first). Same result for every
   input.

3. **The count-byte + repeat padding** — the subtle part `mix_id` gets right and
   the TD/RA1 rotate-add scheme does *not* have. It lives in the streaming
   finalizer inside `FUN_004a1cf0` (YRpp `CRCEngine::operator()(void*,int)` /
   `Value()`):

   When 1..3 bytes are left over in the 4-byte staging buffer, the finalizer
   writes the leftover count into the next staging slot (`Buffer[Index] =
   Index`, the count byte), fills every remaining slot up to 4 with a copy of
   the staging buffer's first byte, and then CRCs the completed 4-byte
   staging word. With 0 leftover bytes nothing extra is hashed.
   Our `mix_id` does the identical thing: append `(l & 3)`, then repeat the
   first char of the final 4-byte block until length ≡ 0 (mod 4).

The **uppercasing** happens in the caller, `FUN_005b3ae0` (below): it copies the
name, calls `strupr` (`FUN_007dbdc4`), then feeds it to `FUN_004a1cf0`. This is
our `std::toupper` step. **Divergence: none.** OpenRA's `PackageEntry` shows the
same CRC32 (TS) vs Classic rotate-add (TD/RA1) split, matching Review Correction
#1's replacement of the draft rotate-add hash.

## MIX filename → id → cache lookup. Confidence: **HIGH**

`FUN_005b3ae0` (the function YRpp's `0x5B3C20` lands inside) is the file/shape
retrieve: it uppercases the name, hashes it with the CRC engine above, then
binary-tree-searches a cache keyed by that id, comparing `node.id == crc`:

```text
name_copy = uppercase(copy of name)            // stack buffer at frame offset -0x104, strupr
id = crc_engine(name_copy, strlen(name_copy))  // FUN_004a1cf0, == mix_id(name)
walk the binary tree from its root:
    if node.key == id and node has a value: return that value
    go left if id < node.key, else go right
```

Confirms `mix_id` is the exact key used to find entries — the same lookup our
`MixFile::find(name)` performs against the parsed index.

## MIX archive loader / index-parse. Confidence: **HIGH**

`FUN_005b3650` is the `MixFileClass` constructor. Mapped onto our
`MixFile::open` it matches step-for-step. In our summary below, `this[n]`
means the dword at byte offset `4n` of the object:

```text
this[3] = file handle for name
hdr = first dword of the file
if low 16 bits of hdr == 0:                     // NEW format
    byte +0x10 = bit 16 of hdr                  // 0x10000 checksummed (YRpp "Blowfish")
    byte +0x11 = bit 17 of hdr                  // 0x20000 encrypted   (YRpp "Encryption")
    if hdr has 0x20000: attach a Blowfish decrypting straw to the stream
    read the 6-byte header (count, bodysize) through the (possibly encrypted) stream
else:                                           // OLD TD/RA1 format: that dword WAS count+start
    reuse the 4 bytes already read, read 2 more // 6-byte header, no flags
this[5] = count                                 // u16 CountFiles
this[6] = bodysize                              // u32 FileSize
this[8] = new buffer of count * 0xC bytes       // count x 12-byte entries {id,offset,size}
read count * 0xC bytes of index into it (through the stream)
this[7] = body start offset                     // FileStartOffset
link the object into the global MixFileClass list
```

Point-by-point vs `src/mix.cpp`:

| Game (`FUN_005b3650`) | `MixFile::open` |
|---|---|
| `(short)hdr == 0` → new format | `(first & 0x0000FFFF) == 0` → new format |
| `hdr & 0x20000` → encrypted, wire Blowfish straw | `encrypted_ = flags & 0x00020000`, derive key + Blowfish |
| `hdr & 0x10000` → checksummed bit | `checksummed_ = flags & 0x00010000` |
| 6-byte header (u16 count, u32 bodysize) | `count = rd16`, bodysize `rd32` (6 bytes) |
| `operator_new(count * 0xc)`, 12-byte entries | `MixEntry{id,offset,size}` × count, `ip += 12` |
| body start recorded after header+index | `body_start_` after block-aligned region |

The encrypted branch sets up a Westwood **straw** (streaming filter) chain
(`FUN_00632b60`/`FUN_00632bb0` link the straw; the Blowfish decrypt is the
straw's read method) before the 6-byte header and the index are read — so the
index is decrypted transparently. This is the streaming form of our
"derive key → Blowfish-decrypt header+index in 8-byte blocks" path, and confirms
the `MixHeaderData{ID,Offset,Size}` 12-byte layout (YRpp `MixFileClass.h`).
**Divergence: none of substance.** Our port decrypts the whole header+index
region up-front into a buffer; the game decrypts on the fly through the straw —
same bytes, different plumbing.

The Westwood public-key string (`AihRvN…`, Task 2) is present in `.rdata` at
`0x007E0A8E`, confirming the key-derivation path exists in this binary
(consistent with `ra2md.mix`/`ra2.mix` opening as encrypted archives in Task 4).
