# VXL + HVA Voxel Model Decoder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure-library decoder for RA2 `.vxl` voxel models and their sibling `.hva` animation-transform files (decode only, no rendering), Ghidra-calibrated against the game's own loader and byte-gated against an XCC-source oracle dump of the real `htnk.vxl`/`htnk.hva` (Rhino tank).

**Architecture:** Two new pure C++17 translation units in the `ra2assets` library — `src/hva.cpp` (`load_hva`) and `src/vxl.cpp` (`load_vxl`, reusing the existing `Palette::load` for the game-accurate 6-bit palette expansion) — driven by synthetic-byte doctest cases in `tests/test_vxl.cpp`, plus a guarded real-asset oracle test that compares a canonical text dump of our decode against a dump produced by XCC's own VXL/HVA code built into an off-repo tool. A Ghidra pass locates the game's loaders first so any layout divergence corrects the facts before code lands.

**Tech Stack:** C++17, CMake + Ninja + MinGW-w64, doctest, Ghidra 11.2.1 headless (Jython post-scripts), off-repo g++ oracle tool.

## Global Constraints

- C++17, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Pure library: no SDL/Win32/OS/DirectX includes anywhere under `include/ra2assets/` or `src/` — pure functions over byte spans.
- No game assets, no game binary, no Ghidra project, and no GPL code committed to the repo (XCC source is read off-repo at `D:\ra2-re\reference\xcc` only; the oracle tool that embeds it lives at `D:\ra2-re\oracle-tool\`, outside the repo; oracle output under `D:\ra2-android\oracle\` is git-ignored).
- Real-asset tests skip cleanly (doctest `MESSAGE(...)` + `return`) when the install or oracle files are absent, so asset-free CI still builds and passes.
- The test exe MUST be run from PowerShell, not Git Bash (known environment issue).
- Every new `src/*.cpp` is added to `RA2_LIB_SOURCES` in `cmake/ra2_sources.cmake` (single list shared by desktop + Android builds); every new `tests/*.cpp` is added via `target_sources(ra2assets_tests PRIVATE tests/...)` in `CMakeLists.txt`.
- TDD: write the failing test first, watch it fail, then implement.
- Pristine test output: full suite ends `[doctest] Status: SUCCESS!` with no failures (intentional skip MESSAGEs are fine).
- Commit after every task (conventional commits: `feat:` / `test:` / `docs:` / `perf:`).
- Bounds-check every read against the byte-span length and throw `std::runtime_error` with a context message (imitate `src/theater.cpp`).
- All in-repo paths below assume the checkout at `D:\ra2-android`; if executing in a git worktree, substitute the worktree root for repo files (the off-repo paths `D:\ra2-re\...`, `D:\Red Alert 2`, and the oracle default `D:\ra2-android\oracle` stay exactly as written).

---

## Format ground truth (byte-verified 2026-07-09 against the real `htnk.vxl`, 34,550 bytes)

These facts are the format contract for every task below. Task 1 (Ghidra) verifies them against the game's own loader; **if the game's loader contradicts a field, THE GAME WINS** and the later tasks use the corrected fact (Task 1 records any correction in `docs/re/vxl.md`, and the implementer of the affected task applies it).

**VXL file header — 802 bytes total:**

| offset | type | field |
|-------:|------|-------|
| 0 | `char[16]` | magic `"Voxel Animation"` (15 chars, NUL padded) |
| 16 | u32 | `palette_count` (= 1) |
| 20 | u32 | `section_header_count` |
| 24 | u32 | `section_tailer_count` (must equal header count) |
| 28 | u32 | `body_size` |
| 32 | u8 | `palette_remap_start` (real files: 0x10) |
| 33 | u8 | `palette_remap_end` (real files: 0x1F) |
| 34 | `u8[768]` | palette, 6-bit RGB exactly like `.pal` — reuse `ra2assets::Palette::load` on these 768 bytes for the game-accurate `<<2` expansion |

**Section headers — 28 bytes each, at `802 + i*28`:** `char name[16]` (trim at first NUL); u32 `limb_number`; u32 `unknown1`; u32 `unknown2`.

**Body region:** at `802 + N*28`, length `body_size`.

**Section tailers — 92 bytes each, at `802 + N*28 + body_size + i*92`:**

| offset | type | field |
|-------:|------|-------|
| +0 | u32 | `span_start_ofs` (relative to body start) |
| +4 | u32 | `span_end_ofs` (relative to body start) |
| +8 | u32 | `span_data_ofs` (relative to body start) |
| +12 | f32 | `det_scale` |
| +16 | `f32[3][4]` | transform, row-major (row 0 = 4 floats, then row 1, row 2) |
| +64 | `f32[3]` | `min_bounds` |
| +76 | `f32[3]` | `max_bounds` |
| +88 | u8 | `size_x` |
| +89 | u8 | `size_y` |
| +90 | u8 | `size_z` |
| +91 | u8 | `normals_mode` (2 = TS, 4 = RA2) |

Arithmetic check: `802 + 1*28 + 33,628 + 1*92 = 34,550` = the exact size of the real `htnk.vxl` (1 section, `body_size` 33,628).

**Span tables (per section, inside the body):** at `body + span_start_ofs`: `u32[size_x*size_y]` column start offsets; at `body + span_end_ofs`: `u32[size_x*size_y]` column end offsets. `0xFFFFFFFF` in the start (and end) entry marks an EMPTY column. Column `i` is `(x = i % size_x, y = i / size_x)`. Column voxel data lives at `body + span_data_ofs + start[i]`.

**Column span decode** (XCC `Cvxl_file` shape; Task 1 verifies against the game's own loader):

```text
int z = 0;
while (z < size_z) {
    z += *r++;                    // skip count
    if (z >= size_z) break;       // column may end on a skip
    int c = *r++;                 // voxel count
    for (int j = 0; j < c; j++) { color = *r++; normal = *r++; set voxel (x, y, z); z++; }
    r++;                          // trailing duplicate count byte
}
```

Every byte read must be bounds-checked against the body region.

**Voxel array layout:** `voxels[x + size_x * (y + (size_t)size_y * z)]`, `present == false` by default.

**HVA layout** (byte-verified vs the real 88-byte `htnk.hva` = 16 + 4 + 4 + 16 + 48): `char path[16]` @0 (content ignored); u32 `frame_count` @16; u32 `section_count` @20; `char section_names[section_count][16]`; then `frame_count * section_count * 12` f32 matrices (3 rows x 4 cols, row-major), **FRAME-major** order (all sections of frame 0, then frame 1, ...). Validate: `24 + 16*S + 48*F*S == len`, else throw.

> **Known open question the Ghidra task must attack:** OpenRA's `HvaReader` and this spec say the HVA matrix block is FRAME-major, but XCC's `Chva_file::get_transform_matrix(i, j)` (`D:\ra2-re\reference\xcc\misc\hva_file.h`) indexes `(c_frames * section + frame)` — SECTION-major. The real `htnk.hva` (1 frame, 1 section) cannot distinguish the two. This plan implements FRAME-major per the approved spec; Task 1 tries to settle it from the game's loader and Task 6 records the verdict. The Task-5 oracle dumps matrices in raw FILE order, so it stays correct either way.

---

### Task 1: Ghidra RE — locate the VXL + HVA loaders in gamemd.exe

**Files:**
- Create: `D:\ra2-re\scripts\vxl_hunt.py` (OFF-repo — Ghidra Jython post-script, never committed)
- Create: `docs/re/vxl.md` (in-repo — prose + short excerpts only, NEVER full decompilation dumps)

**Interfaces:**
- Consumes: existing analyzed Ghidra project `D:\ra2-re` name `ra2` (program `gamemd.exe`, YR 1.001, image base 0x00400000); methodology in `docs/re/workflow.md`.
- Produces: `docs/re/vxl.md` with located function addresses + High/Med/Low confidence, and any layout corrections that Tasks 2-4 must apply (THE GAME WINS over the ground-truth table above).

**Shared-project lock rule (read first):** the Ghidra project is EXCLUSIVELY LOCKED while any headless run is open, and other parallel agents share it. If `analyzeHeadless` reports the project is locked (e.g. `Unable to lock project` / `project ... is already open`), **wait 60 seconds and retry** — each dump run takes ~1-2 minutes, so a few retries always get through. Never delete the lock file.

- [ ] **Step 1: Write the hunt post-script** — save exactly this to `D:\ra2-re\scripts\vxl_hunt.py` (block-read byte search is the proven idiom from `D:\ra2-re\scripts\dump6.py`; see `docs/re/workflow.md` §2):

```python
# vxl_hunt.py - Ghidra postScript: locate the VXL + HVA loaders in gamemd.exe
# (YR 1.001) by anchoring on the "Voxel Animation" magic string the VXL loader
# validates, plus the ".vxl"/".hva" extension strings. For each anchor:
#   (1) collect direct data xrefs (getReferencesTo),
#   (2) fallback: block-read byte search for the little-endian image of the
#       anchor address inside executable blocks (catches push-immediate refs
#       Ghidra missed - docs/re/workflow.md lesson "No string xrefs"),
# then decompile every candidate function plus its one-level callers to
# D:/ra2-re/out/vxl_<tag>_<entry>.c and log a summary to
# D:/ra2-re/out/vxl_hunt.txt.
import os, struct
from jarray import zeros
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.mem import MemoryAccessException

OUT = "D:\\ra2-re\\out"
prog = currentProgram
af = prog.getAddressFactory(); sp = af.getDefaultAddressSpace()
def A(va): return sp.getAddress(va)
mon = ConsoleTaskMonitor()
mem = prog.getMemory()
decomp = DecompInterface(); decomp.openProgram(prog)
S = []
def log(s):
    S.append(s)
    print(s)

def read_block(block):
    n = int(block.getSize())
    if n > 8*1024*1024: n = 8*1024*1024
    ba = zeros(n, 'b')
    try: mem.getBytes(block.getStart(), ba, 0, n)
    except MemoryAccessException: return None, None, None
    return block.getStart().getOffset(), bytes(bytearray((x & 0xff) for x in ba[:n])), block

BLOCKS = []
for b in mem.getBlocks():
    if b.isInitialized():
        base, buf, blk = read_block(b)
        if buf is not None: BLOCKS.append((base, buf, blk))

def find_bytes(needle, exec_only=False):
    out = []
    for (base, buf, blk) in BLOCKS:
        if exec_only and not blk.isExecute(): continue
        i = buf.find(needle)
        while i != -1:
            out.append(base + i)
            i = buf.find(needle, i + 1)
    return out

def containing(off):
    return getFunctionContaining(A(off))

def candidate_entries(anchor_off):
    ents = set()
    for r in getReferencesTo(A(anchor_off)):
        f = containing(r.getFromAddress().getOffset())
        if f is not None: ents.add(f.getEntryPoint().getOffset())
    for off in find_bytes(struct.pack("<I", anchor_off & 0xFFFFFFFF), True):
        f = containing(off)
        if f is not None: ents.add(f.getEntryPoint().getOffset())
    return sorted(ents)

seen = set()
def dump(tag, ent):
    if ent in seen: return None
    seen.add(ent)
    fn = containing(ent)
    if fn is None:
        log("  0x%X: no function defined (gap) - candidate for manual createFunction" % ent)
        return None
    res = decomp.decompileFunction(fn, 120, mon)
    c = res.getDecompiledFunction().getC() if (res and res.decompileCompleted()) else "// DECOMPILE FAILED"
    name = "vxl_%s_%X.c" % (tag, ent)
    open(os.path.join(OUT, name), "w").write(
        "// %s entry=0x%X size=%d\n%s" % (fn.getName(), ent, fn.getBody().getNumAddresses(), c))
    log("  dumped %s (%s, size %d)" % (name, fn.getName(), fn.getBody().getNumAddresses()))
    return fn

for (needle, tag) in [("Voxel Animation", "magic"), (".vxl", "extvxl"), (".hva", "exthva")]:
    log("=== anchor %r ===" % needle)
    hits = find_bytes(needle)
    log("%d raw byte hit(s)" % len(hits))
    for off in hits:
        log("anchor bytes at 0x%X" % off)
        ents = candidate_entries(off)
        log("  %d candidate function(s): %s" % (len(ents), ", ".join("0x%X" % e for e in ents)))
        for ent in ents[:12]:            # cap dumps per anchor hit
            fn = dump(tag, ent)
            if fn is not None:
                for cf in fn.getCallingFunctions(mon):
                    dump(tag + "_caller", cf.getEntryPoint().getOffset())

open(os.path.join(OUT, "vxl_hunt.txt"), "w").write("\n".join(S))
log("hunt complete: %d function(s) dumped" % len(seen))
```

- [ ] **Step 2: Run the hunt headless (Git Bash — the one exception to the PowerShell rule, because `analyzeHeadless.bat` is proven from Git Bash per `docs/re/workflow.md`):**

```bash
export JAVA_HOME="/c/Program Files/Java/jdk-21"
GHIDRA="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC/support/analyzeHeadless.bat"
"$GHIDRA" "D:/ra2-re" ra2 -process gamemd.exe -noanalysis -readOnly \
  -scriptPath "D:/ra2-re/scripts" -postScript vxl_hunt.py < /dev/null
```

Expected: ~1-2 min wall; console shows `=== anchor 'Voxel Animation' ===` lines, `dumped vxl_magic_....c (...)` lines, and finally `hunt complete: N function(s) dumped`; `D:/ra2-re/out/vxl_hunt.txt` plus `vxl_*.c` files exist. **If it prints a project-lock error, wait 60 s and re-run the same command.**

- [ ] **Step 3: Analyze the dumps against the ground-truth table.** Read `D:/ra2-re/out/vxl_hunt.txt` and each `D:/ra2-re/out/vxl_*.c`. For each candidate decide which is (a) the VXL file loader, (b) the span/column walker, (c) the HVA loader. Verification checklist — mark each item confirmed / contradicted / not-found:
  - file header stride 802 (constant 0x322) and palette copy from header offset 34;
  - section header stride 28 (0x1C) and tailer stride 92 (0x5C);
  - `span_start_ofs`/`span_end_ofs`/`span_data_ofs` read at tailer +0/+4/+8, relative to the body;
  - `0xFFFFFFFF` (`== -1`) empty-column test on the span start entry;
  - the skip/count/(color,normal)pairs/trailing-byte walk shape from the ground-truth pseudocode (and whether the game breaks on a skip reaching `size_z` or always expects the 3-byte tail);
  - `normals_mode` byte at tailer +91 selecting a normal table (2 vs 4);
  - transform floats at tailer +16 as 3 rows x 4 cols;
  - HVA: header 24 bytes, names `16*section_count`, matrix indexing math — specifically whether the index is `frame * section_count + section` (FRAME-major, what we implement) or `section * frame_count + frame` (XCC's accessor order). This is the open question flagged in the ground-truth section.

  If the string anchors found nothing, grep the off-repo maps for hint addresses and re-run a targeted decompile (addresses are HINTS that drift — verify by content, `docs/re/workflow.md` §4): `grep -rin "vxl\|voxel\|hva\|MotLib\|VoxLib" /d/ra2-re/reference/YRpp --include=*.h | head -50`. An honestly-documented failed hunt (Low confidence) is an acceptable outcome — the Task-5 oracle still gates correctness.

- [ ] **Step 4: Write `docs/re/vxl.md`** with exactly this skeleton, replacing each `<FILL: ...>` marker with real findings from Step 3 (imitate `docs/re/shp.md`'s tone; short illustrative excerpts from the dumps are fine, full decompilations are NOT):

```markdown
# VXL + HVA (voxel models) — reverse-engineering notes

(Loader-location pass for the Foundation Batch VXL/HVA decoder. The oracle
byte-gate lands with the decoder — see the plan and `scripts/make_vxl_oracle.md`.)

## Layout (byte-verified against the real htnk.vxl, 34,550 bytes)

File header, 802 bytes: `char magic[16]="Voxel Animation"` (NUL padded) @0;
u32 palette_count(=1) @16; u32 section_header_count @20; u32
section_tailer_count @24 (== header count); u32 body_size @28; u8
palette_remap_start @32; u8 palette_remap_end @33; u8 palette[768] @34
(6-bit RGB, expanded `<<2` like `.pal` — see `docs/re/pal.md`).

Section headers, 28 B each at 802 + i*28: `char name[16]`; u32 limb_number;
u32 unknown1; u32 unknown2. Body at 802 + N*28, length body_size. Section
tailers, 92 B each after the body: u32 span_start_ofs/span_end_ofs/
span_data_ofs (body-relative) @+0/+4/+8; f32 det_scale @+12; f32
transform[3][4] row-major @+16; f32 min_bounds[3] @+64; f32 max_bounds[3]
@+76; u8 size_x/size_y/size_z @+88/89/90; u8 normals_mode @+91 (2=TS, 4=RA2).

Arithmetic: 802 + 1*28 + 33,628 + 1*92 = 34,550 = htnk.vxl exactly.

Span tables per section: u32[size_x*size_y] starts at body+span_start_ofs,
ends at body+span_end_ofs, 0xFFFFFFFF = empty column, column i = (i % size_x,
i / size_x), data at body + span_data_ofs + start[i]; column decode is the
skip/count/(color,normal)-pairs/trailing-count walk (cross-checked against
XCC `Cvxl_file`, off-repo).

HVA, cross-checked against the 88-byte htnk.hva (16+4+4+16+48): char
path[16]; u32 frame_count @16; u32 section_count @20; names[S][16]; then
F*S 3x4 f32 matrices. Size must equal 24 + 16*S + 48*F*S.

## Ghidra location in `gamemd.exe` (YR 1.001). Confidence: <FILL: High/Medium/Low>

Method (`docs/re/workflow.md`): anchored on the ASCII "Voxel Animation" /
".vxl" / ".hva" strings via block-read byte search + xrefs
(`D:\ra2-re\scripts\vxl_hunt.py`), decompiled candidates + one-level callers.

| Function | Role | Notes |
|----------|------|-------|
| <FILL: FUN_00xxxxxx> | VXL file loader (header validate + palette) | <FILL> |
| <FILL: FUN_00xxxxxx> | VXL span/column walk | <FILL: or "inlined into the loader/rasterizer — explain"> |
| <FILL: FUN_00xxxxxx> | HVA loader | <FILL> |

### Layout verification against the game loader

<FILL: one bullet per checklist item from the plan's Task 1 Step 3 —
confirmed / contradicted / not visible in the decompilation. If ANY field is
contradicted, state the corrected fact prominently: THE GAME WINS and the
decoder tasks use the correction.>

### HVA matrix order (frame-major vs section-major)

OpenRA reads the matrix block frame-major; XCC's `Chva_file` accessor indexes
it section-major (`c_frames * section + frame`). The 1-frame/1-section
htnk.hva cannot distinguish them. Game loader verdict: <FILL: frame-major /
section-major / not resolved (Low)>. Our `load_hva` implements
<FILL: which> per <FILL: the game loader / the approved spec pending RE>.

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| File/section/tailer layout | htnk.vxl arithmetic + XCC structs + <FILL: game loader?> | <FILL> |
| Span walk shape | <FILL> | <FILL> |
| Palette `<<2` expansion | reuse of `Palette::load` (docs/re/pal.md, Medium-High) | Medium-High |
| normals_mode semantics (2/4) | <FILL> | <FILL> |
| HVA layout + matrix order | <FILL> | <FILL> |

(Oracle results are appended by the finalize task once the decoder lands.)
```

- [ ] **Step 5: Commit** (only the in-repo doc; the script and dumps stay off-repo):

```bash
git add docs/re/vxl.md
git commit -m "docs(re): locate VXL/HVA loaders in gamemd.exe, record layout verification"
```

---

### Task 2: HVA decoder (`load_hva`) — TDD with a synthetic builder

**Files:**
- Create: `include/ra2assets/hva.hpp`, `src/hva.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `src/hva.cpp`), `CMakeLists.txt` (register `tests/test_vxl.cpp`)
- Test: `tests/test_vxl.cpp` (new file; covers both HVA and, in later tasks, VXL)

**Interfaces:**
- Consumes: nothing from other tasks (apply any HVA layout correction recorded in `docs/re/vxl.md` by Task 1 — THE GAME WINS).
- Produces (Tasks 5-6 and future Phase 6 rely on these exact signatures):

```cpp
namespace ra2assets {
struct HvaSectionXform { float transform[3][4]; };
struct HvaModel {
    int frame_count = 0;
    std::vector<std::string> section_names;
    std::vector<std::vector<HvaSectionXform>> frames;  // [frame][section]
};
HvaModel load_hva(const uint8_t* data, size_t len);
}
```

- [ ] **Step 1: Register the new test file in `CMakeLists.txt`.** In `D:\ra2-android\CMakeLists.txt`, immediately after the line `target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)`, add:

```cmake
target_sources(ra2assets_tests PRIVATE tests/test_vxl.cpp)
```

- [ ] **Step 2: Write the failing test** — create `D:\ra2-android\tests\test_vxl.cpp` with exactly:

```cpp
#include "doctest.h"
#include "ra2assets/hva.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic .hva builder. Layout (little-endian, packed; byte-verified against
// the real 88-byte htnk.hva = 16 + 4 + 4 + 16 + 48):
//   char path[16]                          @ 0   (content ignored by the loader)
//   u32  frame_count                       @ 16
//   u32  section_count                     @ 20
//   char section_names[section_count][16]  @ 24
//   f32  matrices[frames][sections][3][4]  frame-major, each matrix row-major
// Matrix value at (frame f, section s, row r, col c) = 100*f + 10*s + (4*r+c),
// so every slot is distinct and exactly representable in f32.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_hva(int frames, int sections) {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) p8((uint8_t)((v>>(8*i))&0xff)); };
    auto pf32 = [&](float f){ uint32_t v; std::memcpy(&v, &f, 4); p32(v); };
    auto pname = [&](const std::string& s){
        for (int i=0;i<16;i++) p8(i < (int)s.size() ? (uint8_t)s[i] : 0);
    };
    pname("test.vxl");                       // path (ignored)
    p32((uint32_t)frames);
    p32((uint32_t)sections);
    for (int s=0;s<sections;s++) pname("sec" + std::to_string(s));
    for (int f=0; f<frames; f++)
        for (int s=0; s<sections; s++)
            for (int r=0;r<3;r++)
                for (int c=0;c<4;c++)
                    pf32((float)(100*f + 10*s + 4*r + c));
    return b;
}

TEST_CASE("hva synthetic: names, frame count, frame-major matrices") {
    auto bytes = make_hva(2, 3);
    REQUIRE(bytes.size() == 24u + 16u*3 + 48u*2*3);   // 360
    HvaModel m = load_hva(bytes.data(), bytes.size());
    CHECK(m.frame_count == 2);
    REQUIRE(m.section_names.size() == 3);
    CHECK(m.section_names[0] == "sec0");
    CHECK(m.section_names[1] == "sec1");
    CHECK(m.section_names[2] == "sec2");
    REQUIRE(m.frames.size() == 2);
    REQUIRE(m.frames[0].size() == 3);
    REQUIRE(m.frames[1].size() == 3);
    CHECK(m.frames[0][0].transform[0][0] == 0.0f);
    CHECK(m.frames[0][1].transform[1][2] == 16.0f);    // 10*1 + 4*1+2
    CHECK(m.frames[1][2].transform[2][3] == 131.0f);   // 100 + 20 + 11
    CHECK(m.frames[1][0].transform[0][1] == 101.0f);   // 100 + 0 + 1
}

TEST_CASE("hva synthetic: size mismatches and bad counts throw") {
    auto good = make_hva(2, 3);
    // one byte short
    {
        auto b = good; b.pop_back();
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // one byte long
    {
        auto b = good; b.push_back(0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // smaller than the 24-byte header
    {
        std::vector<uint8_t> b(10, 0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // zero frames (bytes internally consistent: 24 + 16*3 + 0 matrix bytes)
    {
        auto b = make_hva(0, 3);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
    // zero sections
    {
        auto b = make_hva(2, 0);
        CHECK_THROWS_AS(load_hva(b.data(), b.size()), std::runtime_error);
    }
}
```

- [ ] **Step 3: Run the build to verify it fails** (PowerShell, from `D:\ra2-android`; run `cmake -S . -B build -G Ninja` first only if `build/` is missing or stale):

```powershell
cmake --build build --target ra2assets_tests
```

Expected: FAIL —

```text
tests/test_vxl.cpp:2:10: fatal error: ra2assets/hva.hpp: No such file or directory
...
ninja: build stopped: subcommand failed.
```

- [ ] **Step 4: Write the minimal implementation.** Create `D:\ra2-android\include\ra2assets\hva.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace ra2assets {

// One section's 3x4 transform for one animation frame (row-major rows).
struct HvaSectionXform { float transform[3][4]; };

// Decoded .hva voxel-animation file: per animation frame, one transform per
// VXL section. section_names[i] pairs with frames[f][i].
struct HvaModel {
    int frame_count = 0;
    std::vector<std::string> section_names;
    std::vector<std::vector<HvaSectionXform>> frames;  // [frame][section]
};

// Parse a .hva byte buffer. Throws std::runtime_error (with context) on a
// truncated buffer, zero frame/section counts, or a total-size mismatch
// (the file must be exactly 24 + 16*sections + 48*frames*sections bytes).
HvaModel load_hva(const uint8_t* data, size_t len);

} // namespace ra2assets
```

Create `D:\ra2-android\src\hva.cpp`:

```cpp
#include "ra2assets/hva.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace ra2assets {

// On-disk HVA layout (little-endian, packed). Byte-verified against the real
// htnk.hva (88 bytes = 16 path + 4 frame_count + 4 section_count + 16 name +
// 48 matrix) and cross-checked against the game's own loader (docs/re/vxl.md):
//   0   char path[16]                          (content ignored)
//   16  u32 frame_count
//   20  u32 section_count
//   24  char section_names[section_count][16]
//   ..  f32 matrices[frames][sections][3][4]   frame-major, rows row-major
static constexpr size_t kHvaHeader  = 24;
static constexpr size_t kNameSize   = 16;
static constexpr size_t kMatrixSize = 48;   // 12 * f32

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static float rf32(const uint8_t* p) {
    uint32_t v = r32(p);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
static std::string name16(const uint8_t* p) {
    size_t n = 0;
    while (n < kNameSize && p[n] != 0) n++;
    return std::string(reinterpret_cast<const char*>(p), n);
}

HvaModel load_hva(const uint8_t* data, size_t len) {
    if (data == nullptr || len < kHvaHeader)
        throw std::runtime_error("hva: file smaller than header (" +
                                 std::to_string(len) + " bytes)");
    const uint32_t frames   = r32(data + 16);
    const uint32_t sections = r32(data + 20);
    if (frames == 0 || sections == 0)
        throw std::runtime_error("hva: zero frame or section count (frames=" +
            std::to_string(frames) + ", sections=" + std::to_string(sections) + ")");
    if (frames > 1000000 || sections > 1000000)
        throw std::runtime_error("hva: implausible counts (frames=" +
            std::to_string(frames) + ", sections=" + std::to_string(sections) + ")");
    const uint64_t expect = (uint64_t)kHvaHeader + (uint64_t)kNameSize * sections +
                            (uint64_t)kMatrixSize * frames * sections;
    if (expect != (uint64_t)len)
        throw std::runtime_error("hva: size mismatch: header implies " +
            std::to_string(expect) + " bytes, file has " + std::to_string(len));

    HvaModel m;
    m.frame_count = (int)frames;
    m.section_names.reserve(sections);
    for (uint32_t s = 0; s < sections; s++)
        m.section_names.push_back(name16(data + kHvaHeader + (size_t)s * kNameSize));

    // Matrices are FRAME-major on disk (all sections of frame 0, then frame 1,
    // ...); each 3x4 matrix is row-major. See docs/re/vxl.md for the
    // frame-vs-section-major evidence trail.
    const size_t mat0 = kHvaHeader + (size_t)kNameSize * sections;
    m.frames.assign(frames, std::vector<HvaSectionXform>(sections));
    for (uint32_t f = 0; f < frames; f++)
        for (uint32_t s = 0; s < sections; s++) {
            const uint8_t* p = data + mat0 + ((size_t)f * sections + s) * kMatrixSize;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 4; c++)
                    m.frames[f][s].transform[r][c] = rf32(p + (size_t)(r * 4 + c) * 4);
        }
    return m;
}

} // namespace ra2assets
```

In `D:\ra2-android\cmake\ra2_sources.cmake`, add one line to `RA2_LIB_SOURCES` immediately after `    ${RA2_ROOT}/src/theater.cpp`:

```cmake
    ${RA2_ROOT}/src/hva.cpp
```

- [ ] **Step 5: Run the tests to verify they pass** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="hva*"
```

Expected: build succeeds; test run reports `test cases: 2 | 2 passed | 0 failed` (plus the other suites listed as skipped by the filter) and final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 6: Run the full suite without assets to confirm nothing broke** (asset tests skip; keeps the run fast):

```powershell
Remove-Item Env:RA2_INSTALL_DIR -ErrorAction SilentlyContinue; & .\build\ra2assets_tests.exe
```

Expected: skip MESSAGEs from the guarded real-asset suites, `0 failed`, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 7: Commit**

```bash
git add include/ra2assets/hva.hpp src/hva.cpp tests/test_vxl.cpp cmake/ra2_sources.cmake CMakeLists.txt
git commit -m "feat(hva): HVA voxel-animation decoder with synthetic-byte tests"
```

---

### Task 3: VXL header / palette / section-header / tailer parse

**Files:**
- Create: `include/ra2assets/vxl.hpp`, `src/vxl.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `src/vxl.cpp`), `tests/test_vxl.cpp` (append)

**Interfaces:**
- Consumes: `ra2assets::Palette::load(const uint8_t* data, size_t len)` and `struct Rgba { uint8_t r,g,b,a; }` from `include/ra2assets/pal.hpp` (game-accurate 6-bit `<<2` expansion — 6-bit 63 becomes 252). Apply any layout correction recorded in `docs/re/vxl.md` by Task 1 — THE GAME WINS.
- Produces (Tasks 4-5 and future Phase 6 rely on these exact signatures):

```cpp
namespace ra2assets {
struct VxlVoxel { uint8_t color; uint8_t normal; bool present; };
struct VxlSection {
    std::string name;
    int size_x, size_y, size_z;
    float scale;
    float transform[3][4];          // section transform (tailer)
    float min_bounds[3], max_bounds[3];
    uint8_t normals_mode;           // 2 (TS) | 4 (RA2)
    std::vector<VxlVoxel> voxels;   // size_x*size_y*size_z, present=false if empty
};
struct VxlModel {
    Rgba palette[256];
    std::vector<VxlSection> sections;
};
VxlModel load_vxl(const uint8_t* data, size_t len);
}
```

- [ ] **Step 1: Write the failing test.** First add the include: in `D:\ra2-android\tests\test_vxl.cpp` replace

```cpp
#include "doctest.h"
#include "ra2assets/hva.hpp"
```

with

```cpp
#include "doctest.h"
#include "ra2assets/hva.hpp"
#include "ra2assets/vxl.hpp"
```

Then append at the end of the file:

```cpp
// ---------------------------------------------------------------------------
// Synthetic .vxl builder: 1 section "TESTSEC", size_x=2, size_y=1, size_z=2.
// Column 0 (x=0,y=0) holds one voxel at z=1 (color 7, normal 3); column 1
// (x=1,y=0) is EMPTY (span start/end entries = 0xFFFFFFFF). Palette entry 7
// is 6-bit (63,0,0), which must expand game-accurately (<<2) to r=252.
//
// Body layout (21 bytes): span starts u32[2] @0, span ends u32[2] @8, span
// data @16: skip=1, count=1, color=7, normal=3, trailing-count=1.
// File = 802 (file header) + 28 (section header) + 21 (body) + 92 (tailer)
//      = 943 bytes — same arithmetic that pins the real htnk.vxl at
//        802 + 28 + 33,628 + 92 = 34,550.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_vxl() {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) p8((uint8_t)((v>>(8*i))&0xff)); };
    auto pf32 = [&](float f){ uint32_t v; std::memcpy(&v, &f, 4); p32(v); };
    auto pname = [&](const std::string& s){
        for (int i=0;i<16;i++) p8(i < (int)s.size() ? (uint8_t)s[i] : 0);
    };
    // --- file header (802 bytes) ---
    pname("Voxel Animation");        // magic: 15 chars + NUL padding
    p32(1);                          // palette_count
    p32(1);                          // section_header_count
    p32(1);                          // section_tailer_count
    p32(21);                         // body_size
    p8(0x10); p8(0x1f);              // palette remap start/end (as real files)
    for (int i=0;i<256;i++) {        // 768-byte 6-bit palette; entry 7 = (63,0,0)
        if (i == 7) { p8(63); p8(0); p8(0); }
        else        { p8(0);  p8(0); p8(0); }
    }
    // --- section header (28 bytes) ---
    pname("TESTSEC");
    p32(0);                          // limb_number
    p32(1);                          // unknown1 (XCC-written files hold 1)
    p32(0);                          // unknown2 (XCC-written files hold 0)
    // --- body (21 bytes) ---
    p32(0);  p32(0xFFFFFFFFu);       // span starts: col0=0, col1=EMPTY
    p32(4);  p32(0xFFFFFFFFu);       // span ends:   col0=4 (last byte index), col1=EMPTY
    p8(1); p8(1); p8(7); p8(3); p8(1); // col0 data: skip 1, count 1, (7,3), trailing 1
    // --- section tailer (92 bytes) ---
    p32(0);                          // span_start_ofs (body-relative)
    p32(8);                          // span_end_ofs
    p32(16);                         // span_data_ofs
    pf32(0.25f);                     // det_scale (exactly representable)
    for (int i=1;i<=12;i++) pf32((float)i);   // transform rows {1..4},{5..8},{9..12}
    pf32(-1.5f); pf32(-0.5f); pf32(0.0f);     // min_bounds
    pf32( 1.5f); pf32( 0.5f); pf32(2.0f);     // max_bounds
    p8(2); p8(1); p8(2);             // size_x, size_y, size_z
    p8(4);                           // normals_mode (RA2)
    return b;
}

TEST_CASE("vxl synthetic: header, palette, section header + tailer parse") {
    auto bytes = make_vxl();
    REQUIRE(bytes.size() == 943);
    VxlModel m = load_vxl(bytes.data(), bytes.size());

    // Palette entry 7 is 6-bit (63,0,0) -> game-accurate <<2 expansion = 252.
    CHECK(m.palette[7].r == 252);
    CHECK(m.palette[7].g == 0);
    CHECK(m.palette[7].b == 0);
    CHECK(m.palette[7].a == 255);
    CHECK(m.palette[0].r == 0);

    REQUIRE(m.sections.size() == 1);
    const VxlSection& s = m.sections[0];
    CHECK(s.name == "TESTSEC");
    CHECK(s.size_x == 2);
    CHECK(s.size_y == 1);
    CHECK(s.size_z == 2);
    CHECK(s.scale == 0.25f);
    CHECK(s.transform[0][0] == 1.0f);
    CHECK(s.transform[0][3] == 4.0f);
    CHECK(s.transform[1][1] == 6.0f);
    CHECK(s.transform[2][3] == 12.0f);
    CHECK(s.min_bounds[0] == -1.5f);
    CHECK(s.min_bounds[1] == -0.5f);
    CHECK(s.min_bounds[2] == 0.0f);
    CHECK(s.max_bounds[0] == 1.5f);
    CHECK(s.max_bounds[1] == 0.5f);
    CHECK(s.max_bounds[2] == 2.0f);
    CHECK(s.normals_mode == 4);
    CHECK(s.voxels.size() == 4u);            // 2*1*2 cells allocated
}

TEST_CASE("vxl bounds checks reject malformed input") {
    // truncated: smaller than the 802-byte file header
    {
        auto b = make_vxl(); b.resize(100);
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // wrong magic
    {
        auto b = make_vxl(); b[0] = 'W';
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // palette_count != 1
    {
        auto b = make_vxl(); b[16] = 2;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // tailer_count != header_count
    {
        auto b = make_vxl(); b[24] = 2;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // total size must equal 802 + N*28 + body_size + N*92 exactly
    {
        auto b = make_vxl(); b.push_back(0);
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // span start TABLE beyond the body: tailer span_start_ofs (abs offset 851,
    // = 802 + 28 + 21) corrupted to 0x40000000
    {
        auto b = make_vxl();
        b[851] = 0x00; b[852] = 0x00; b[853] = 0x00; b[854] = 0x40;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // span_data_ofs beyond the body (abs offset 859 -> 64 > body_size 21)
    {
        auto b = make_vxl();
        b[859] = 64; b[860] = 0; b[861] = 0; b[862] = 0;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
}
```

- [ ] **Step 2: Run the build to verify it fails** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
```

Expected: FAIL —

```text
tests/test_vxl.cpp:3:10: fatal error: ra2assets/vxl.hpp: No such file or directory
...
ninja: build stopped: subcommand failed.
```

- [ ] **Step 3: Write the minimal implementation.** Create `D:\ra2-android\include\ra2assets\vxl.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "ra2assets/pal.hpp"

namespace ra2assets {

// One voxel cell. `color` indexes VxlModel::palette; `normal` indexes the
// engine's fixed normal table selected by VxlSection::normals_mode. Cells
// default to present == false (empty space).
struct VxlVoxel { uint8_t color; uint8_t normal; bool present; };

// One named section (limb) of a voxel model. `voxels` is a dense
// size_x*size_y*size_z grid laid out as
//   voxels[x + size_x * (y + (size_t)size_y * z)]
// with present == false for empty cells.
struct VxlSection {
    std::string name;
    int size_x, size_y, size_z;
    float scale;
    float transform[3][4];          // section transform (tailer)
    float min_bounds[3], max_bounds[3];
    uint8_t normals_mode;           // 2 (TS) | 4 (RA2)
    std::vector<VxlVoxel> voxels;   // size_x*size_y*size_z, present=false if empty
};

// Decoded .vxl voxel model: the 256-entry palette (6-bit source channels
// expanded game-accurately via Palette::load's <<2) plus one VxlSection per
// section header/tailer pair.
struct VxlModel {
    Rgba palette[256];
    std::vector<VxlSection> sections;
};

// Parse a .vxl byte buffer (decode only -- no rendering). Throws
// std::runtime_error (with context) on: truncation, bad magic,
// palette_count != 1, header/tailer count mismatch, total-size mismatch
// (len must equal 802 + N*28 + body_size + N*92 exactly), span tables or
// span data reaching outside the body region, or a voxel run exceeding
// size_z. See docs/re/vxl.md for the layout evidence.
VxlModel load_vxl(const uint8_t* data, size_t len);

} // namespace ra2assets
```

Create `D:\ra2-android\src\vxl.cpp` (this task parses everything and allocates the voxel grid all-absent; the span walk lands in Task 4):

```cpp
#include "ra2assets/vxl.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace ra2assets {

// Fixed on-disk sizes of the VXL structures (little-endian, packed).
// Byte-verified against the real htnk.vxl (34,550 bytes):
//   802 + 1*28 + body 33,628 + 1*92 = 34,550 exactly.
// Cross-checked against the game's own loader (docs/re/vxl.md) and, off-repo,
// XCC misc/cc_structures.h (t_vxl_header / t_vxl_section_header /
// t_vxl_section_tailer).
//   file header (802): char magic[16]="Voxel Animation"; u32 palette_count(=1);
//     u32 section_header_count; u32 section_tailer_count; u32 body_size;
//     u8 palette_remap_start; u8 palette_remap_end; u8 palette[768] (6-bit RGB)
//   section header (28): char name[16]; u32 limb_number; u32 unknown1; u32 unknown2
//   section tailer (92): u32 span_start_ofs; u32 span_end_ofs; u32 span_data_ofs
//     (all relative to the body region); f32 det_scale; f32 transform[3][4]
//     (row-major); f32 min_bounds[3]; f32 max_bounds[3]; u8 size_x; u8 size_y;
//     u8 size_z; u8 normals_mode (2=TS, 4=RA2)
static constexpr size_t kFileHeader    = 802;
static constexpr size_t kPaletteOfs    = 34;
static constexpr size_t kSectionHeader = 28;
static constexpr size_t kSectionTailer = 92;
static const char kMagic[16] = "Voxel Animation";   // 15 chars + NUL

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static float rf32(const uint8_t* p) {
    uint32_t v = r32(p);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
static std::string name16(const uint8_t* p) {
    size_t n = 0;
    while (n < 16 && p[n] != 0) n++;
    return std::string(reinterpret_cast<const char*>(p), n);
}

VxlModel load_vxl(const uint8_t* data, size_t len) {
    if (data == nullptr || len < kFileHeader)
        throw std::runtime_error("vxl: file smaller than 802-byte header (" +
                                 std::to_string(len) + " bytes)");
    if (std::memcmp(data, kMagic, 16) != 0)
        throw std::runtime_error("vxl: bad magic (expected \"Voxel Animation\")");
    const uint32_t palette_count = r32(data + 16);
    if (palette_count != 1)
        throw std::runtime_error("vxl: palette_count " +
            std::to_string(palette_count) + " != 1");
    const uint32_t n_headers = r32(data + 20);
    const uint32_t n_tailers = r32(data + 24);
    const uint32_t body_size = r32(data + 28);
    if (n_headers != n_tailers)
        throw std::runtime_error("vxl: section header count " +
            std::to_string(n_headers) + " != tailer count " + std::to_string(n_tailers));
    if (n_headers == 0)
        throw std::runtime_error("vxl: zero sections");
    const uint64_t expect = (uint64_t)kFileHeader +
                            (uint64_t)n_headers * kSectionHeader +
                            (uint64_t)body_size +
                            (uint64_t)n_tailers * kSectionTailer;
    if (expect != (uint64_t)len)
        throw std::runtime_error("vxl: size mismatch: header implies " +
            std::to_string(expect) + " bytes, file has " + std::to_string(len));

    VxlModel m;
    // 6-bit palette expanded exactly as the game does (<<2) by Palette::load.
    Palette pal = Palette::load(data + kPaletteOfs, 768);
    for (int i = 0; i < 256; i++) m.palette[i] = pal[i];

    const size_t body0 = kFileHeader + (size_t)n_headers * kSectionHeader;
    const uint8_t* body = data + body0;
    const size_t tail0 = body0 + (size_t)body_size;
    m.sections.reserve(n_headers);
    for (uint32_t si = 0; si < n_headers; si++) {
        const uint8_t* sh = data + kFileHeader + (size_t)si * kSectionHeader;
        const uint8_t* st = data + tail0 + (size_t)si * kSectionTailer;
        VxlSection sec;
        sec.name = name16(sh);
        const uint32_t span_start_ofs = r32(st + 0);
        const uint32_t span_end_ofs   = r32(st + 4);
        const uint32_t span_data_ofs  = r32(st + 8);
        sec.scale = rf32(st + 12);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 4; c++)
                sec.transform[r][c] = rf32(st + 16 + (size_t)(r * 4 + c) * 4);
        for (int k = 0; k < 3; k++) {
            sec.min_bounds[k] = rf32(st + 64 + (size_t)k * 4);
            sec.max_bounds[k] = rf32(st + 76 + (size_t)k * 4);
        }
        sec.size_x = st[88];
        sec.size_y = st[89];
        sec.size_z = st[90];
        sec.normals_mode = st[91];

        const size_t ncols = (size_t)sec.size_x * (size_t)sec.size_y;
        if ((uint64_t)span_start_ofs + 4ull * ncols > (uint64_t)body_size)
            throw std::runtime_error("vxl: section " + std::to_string(si) +
                " span start table (" + std::to_string(4 * ncols) + " bytes at " +
                std::to_string(span_start_ofs) + ") exceeds body (" +
                std::to_string(body_size) + " bytes)");
        if ((uint64_t)span_end_ofs + 4ull * ncols > (uint64_t)body_size)
            throw std::runtime_error("vxl: section " + std::to_string(si) +
                " span end table (" + std::to_string(4 * ncols) + " bytes at " +
                std::to_string(span_end_ofs) + ") exceeds body (" +
                std::to_string(body_size) + " bytes)");
        if (span_data_ofs > body_size)
            throw std::runtime_error("vxl: section " + std::to_string(si) +
                " span data offset " + std::to_string(span_data_ofs) +
                " exceeds body (" + std::to_string(body_size) + " bytes)");

        sec.voxels.assign(ncols * (size_t)sec.size_z, VxlVoxel{0, 0, false});
        // Span decode (filling sec.voxels) lands in the next task; the grid
        // stays all-absent here.
        (void)body;
        m.sections.push_back(std::move(sec));
    }
    return m;
}

} // namespace ra2assets
```

In `D:\ra2-android\cmake\ra2_sources.cmake`, add one line to `RA2_LIB_SOURCES` immediately after `    ${RA2_ROOT}/src/hva.cpp`:

```cmake
    ${RA2_ROOT}/src/vxl.cpp
```

- [ ] **Step 4: Run the tests to verify they pass** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="vxl*"
```

Expected: build succeeds; `test cases: 2 | 2 passed | 0 failed`, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 5: Commit**

```bash
git add include/ra2assets/vxl.hpp src/vxl.cpp tests/test_vxl.cpp cmake/ra2_sources.cmake
git commit -m "feat(vxl): VXL header/palette/section-header/tailer parser"
```

---

### Task 4: VXL span decode — fill the voxel grid inside `load_vxl`

**Files:**
- Modify: `src/vxl.cpp` (add the span walk), `tests/test_vxl.cpp` (append)

**Interfaces:**
- Consumes: `load_vxl` / `VxlSection` / `VxlVoxel` exactly as declared in Task 3; the `make_vxl()` builder already present in `tests/test_vxl.cpp`.
- Produces: `load_vxl` now returns sections whose `voxels` grid is populated (`present == true` cells carry `color`/`normal`); no signature changes.

- [ ] **Step 1: Write the failing tests** — append at the end of `D:\ra2-android\tests\test_vxl.cpp`:

```cpp
// ---------------------------------------------------------------------------
// Second synthetic .vxl: 1 section, size 1x1x8, ONE column whose data holds
// two voxel spans separated by skips and ends on a BARE skip (no trailing
// count/duplicate bytes after it):
//   skip 1, count 2, (10,1), (11,2), trailing 2,
//   skip 2, count 1, (12,3),         trailing 1,
//   skip 2                                          -> z = 8 = size_z, column ends
// Voxels present: z=1 (10,1), z=2 (11,2), z=5 (12,3). 13 data bytes; body =
// 4 (starts) + 4 (ends) + 13 = 21; file = 802 + 28 + 21 + 92 = 943 bytes.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_vxl_multispan() {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) p8((uint8_t)((v>>(8*i))&0xff)); };
    auto pf32 = [&](float f){ uint32_t v; std::memcpy(&v, &f, 4); p32(v); };
    auto pname = [&](const std::string& s){
        for (int i=0;i<16;i++) p8(i < (int)s.size() ? (uint8_t)s[i] : 0);
    };
    // file header (802 bytes)
    pname("Voxel Animation");
    p32(1); p32(1); p32(1); p32(21);            // palette_count, headers, tailers, body_size
    p8(0x10); p8(0x1f);
    for (int i=0;i<768;i++) p8(0);              // all-zero palette
    // section header (28 bytes)
    pname("MULTI");
    p32(0); p32(1); p32(0);
    // body (21 bytes)
    p32(0);                                      // span start[0] = 0
    p32(12);                                     // span end[0]   = 12 (last data byte)
    p8(1); p8(2); p8(10); p8(1); p8(11); p8(2); p8(2);   // span 1: skip,count,2 voxels,trailing
    p8(2); p8(1); p8(12); p8(3); p8(1);                  // span 2: skip,count,1 voxel,trailing
    p8(2);                                               // final bare skip -> z = 8
    // section tailer (92 bytes)
    p32(0);                                      // span_start_ofs
    p32(4);                                      // span_end_ofs
    p32(8);                                      // span_data_ofs
    pf32(1.0f);                                  // det_scale
    for (int i=0;i<12;i++) pf32(0.0f);           // transform
    for (int i=0;i<6;i++) pf32(0.0f);            // min/max bounds
    p8(1); p8(1); p8(8);                         // size_x, size_y, size_z
    p8(4);                                       // normals_mode
    return b;
}

TEST_CASE("vxl span decode: skip/count walk fills the voxel grid") {
    auto bytes = make_vxl();
    VxlModel m = load_vxl(bytes.data(), bytes.size());
    REQUIRE(m.sections.size() == 1);
    const VxlSection& s = m.sections[0];
    REQUIRE(s.voxels.size() == 4u);
    // documented layout: voxels[x + size_x*(y + size_y*z)]
    auto at = [&](int x, int y, int z) -> const VxlVoxel& {
        return s.voxels[(size_t)x + (size_t)s.size_x *
                        ((size_t)y + (size_t)s.size_y * (size_t)z)];
    };
    CHECK(at(0,0,0).present == false);
    CHECK(at(0,0,1).present == true);      // the one voxel: skip 1 then (7,3)
    CHECK(at(0,0,1).color == 7);
    CHECK(at(0,0,1).normal == 3);
    CHECK(at(1,0,0).present == false);     // empty column (0xFFFFFFFF start)
    CHECK(at(1,0,1).present == false);
}

TEST_CASE("vxl span decode: multi-span column with skips, ends on a bare skip") {
    auto bytes = make_vxl_multispan();
    REQUIRE(bytes.size() == 943);
    VxlModel m = load_vxl(bytes.data(), bytes.size());
    REQUIRE(m.sections.size() == 1);
    const VxlSection& s = m.sections[0];
    CHECK(s.name == "MULTI");
    REQUIRE(s.voxels.size() == 8u);        // 1*1*8; index of (0,0,z) is z
    int present = 0;
    for (const auto& v : s.voxels) if (v.present) present++;
    CHECK(present == 3);
    CHECK(s.voxels[1].present); CHECK(s.voxels[1].color == 10); CHECK(s.voxels[1].normal == 1);
    CHECK(s.voxels[2].present); CHECK(s.voxels[2].color == 11); CHECK(s.voxels[2].normal == 2);
    CHECK(s.voxels[5].present); CHECK(s.voxels[5].color == 12); CHECK(s.voxels[5].normal == 3);
    CHECK_FALSE(s.voxels[0].present);
    CHECK_FALSE(s.voxels[3].present);
    CHECK_FALSE(s.voxels[4].present);
    CHECK_FALSE(s.voxels[6].present);
    CHECK_FALSE(s.voxels[7].present);
}

TEST_CASE("vxl span decode bounds violations throw") {
    // Voxel count pushes z past size_z: column 0 data of make_vxl() lives at
    // abs 846..850 = [skip 1, count 1, color 7, normal 3, trailing 1]
    // (body starts at abs 830 = 802+28; data at body+16). Corrupt the count.
    {
        auto b = make_vxl();
        b[847] = 2;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // Span start points at the last body byte -> the count read (and beyond)
    // falls off the body region: span start[0] entry is at abs 830.
    {
        auto b = make_vxl();
        b[830] = 4;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
    // Span start beyond the body region entirely.
    {
        auto b = make_vxl();
        b[830] = 100;
        CHECK_THROWS_AS(load_vxl(b.data(), b.size()), std::runtime_error);
    }
}
```

- [ ] **Step 2: Run the tests to verify they fail** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="vxl span*"
```

Expected: build succeeds, run FAILS — output includes lines like

```text
tests/test_vxl.cpp(...): ERROR: CHECK( at(0,0,1).present == true ) is NOT correct!
  values: CHECK( false == true )
...
[doctest] Status: FAILURE!
```

(all three new cases fail: the grid stays all-absent and the corrupted-span cases don't throw yet).

- [ ] **Step 3: Implement the span walk.** In `D:\ra2-android\src\vxl.cpp`, insert this function directly after `name16` (before `load_vxl`):

```cpp
// Walk one section's voxel columns. Column i covers (x = i % size_x,
// y = i / size_x); its data at body + span_data_ofs + start[i] is a run of
// {skip, count, count*(color,normal), trailing-duplicate-count} packets along
// +z, and 0xFFFFFFFF in the start table marks an EMPTY column. This is the
// walk shape of the game's own loader (docs/re/vxl.md; cross-checked off-repo
// against XCC Cvxl_file). A column may end on a bare skip reaching size_z.
// Every byte is bounds-checked against the body region.
static void decode_spans(const uint8_t* body, size_t body_size, size_t sec_index,
                         uint32_t span_start_ofs, uint32_t span_data_ofs,
                         VxlSection& sec) {
    const size_t ncols = (size_t)sec.size_x * (size_t)sec.size_y;
    auto next_byte = [&](size_t& off, const char* what) -> uint8_t {
        if (off >= body_size)
            throw std::runtime_error("vxl: section " + std::to_string(sec_index) +
                " " + what + " at body offset " + std::to_string(off) +
                " exceeds body (" + std::to_string(body_size) + " bytes)");
        return body[off++];
    };
    for (size_t i = 0; i < ncols; i++) {
        const uint32_t start = r32(body + span_start_ofs + i * 4);
        if (start == 0xFFFFFFFFu)
            continue;                          // empty column
        const uint64_t data_off = (uint64_t)span_data_ofs + start;
        if (data_off >= (uint64_t)body_size)
            throw std::runtime_error("vxl: section " + std::to_string(sec_index) +
                " column " + std::to_string(i) + " span start " +
                std::to_string(start) + " exceeds body (" +
                std::to_string(body_size) + " bytes)");
        const int x = (int)(i % (size_t)sec.size_x);
        const int y = (int)(i / (size_t)sec.size_x);
        size_t off = (size_t)data_off;
        int z = 0;
        while (z < sec.size_z) {
            z += next_byte(off, "span skip byte");
            if (z >= sec.size_z)
                break;                         // column may end on a skip
            const int count = next_byte(off, "span voxel count");
            for (int j = 0; j < count; j++) {
                if (z >= sec.size_z)
                    throw std::runtime_error("vxl: section " + std::to_string(sec_index) +
                        " column " + std::to_string(i) +
                        " voxel run exceeds size_z " + std::to_string(sec.size_z));
                const uint8_t color  = next_byte(off, "voxel color");
                const uint8_t normal = next_byte(off, "voxel normal");
                VxlVoxel& v = sec.voxels[(size_t)x + (size_t)sec.size_x *
                                         ((size_t)y + (size_t)sec.size_y * (size_t)z)];
                v.color = color;
                v.normal = normal;
                v.present = true;
                z++;
            }
            off++;                             // trailing duplicate count byte
        }
    }
}
```

Then, inside `load_vxl`'s section loop, replace

```cpp
        sec.voxels.assign(ncols * (size_t)sec.size_z, VxlVoxel{0, 0, false});
        // Span decode (filling sec.voxels) lands in the next task; the grid
        // stays all-absent here.
        (void)body;
```

with

```cpp
        sec.voxels.assign(ncols * (size_t)sec.size_z, VxlVoxel{0, 0, false});
        decode_spans(body, (size_t)body_size, si, span_start_ofs, span_data_ofs, sec);
```

(`span_end_ofs` stays parse-validated in Task 3's code but is not needed by the walk — the walk is delimited by `size_z` and the body bounds, matching the game/XCC shape.)

- [ ] **Step 4: Run the tests to verify they pass** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="vxl*"
```

Expected: `test cases: 5 | 5 passed | 0 failed`, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 5: Run the full suite without assets:**

```powershell
Remove-Item Env:RA2_INSTALL_DIR -ErrorAction SilentlyContinue; & .\build\ra2assets_tests.exe
```

Expected: `0 failed`, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 6: Commit**

```bash
git add src/vxl.cpp tests/test_vxl.cpp
git commit -m "feat(vxl): span decode fills the voxel grid (skip/count walk, bounds-checked)"
```

---

### Task 5: Real-asset oracle — htnk.vxl + htnk.hva vs an XCC-source dump

**Files:**
- Create: `D:\ra2-re\oracle-tool\xcc_shim.h`, `D:\ra2-re\oracle-tool\xcc_verbatim.h` (generated), `D:\ra2-re\oracle-tool\xcc_vxl_dump.cpp` (ALL OFF-repo — they embed GPLv3 XCC material and must never be committed)
- Create: `scripts/make_vxl_oracle.md` (in-repo — generation instructions, no GPL code)
- Create: `D:\ra2-android\oracle\vxl\htnk.vxl.txt`, `D:\ra2-android\oracle\vxl\htnk.hva.txt` (generated; `oracle/` is already git-ignored)
- Modify: `tests/test_vxl.cpp` (append the guarded oracle test)

**Interfaces:**
- Consumes: `MixFile::open(std::vector<uint8_t>)`, `MixFile::open_nested(const std::string&)`, `const MixEntry* MixFile::find(const std::string&)`, `std::vector<uint8_t> MixFile::read(const MixEntry&)` from `include/ra2assets/mix.hpp`; `load_vxl` / `load_hva` from Tasks 2-4; the existing `build\ra2extract.exe` CLI (`--raw` mode).
- Produces: the canonical dump format both sides emit (documented below and in `scripts/make_vxl_oracle.md`) and the git-ignored oracle files.

**Canonical dump format** (identical on both sides; floats are printed as the raw IEEE-754 bit pattern `0x%08x` so no float-to-decimal formatting can ever diverge):

```text
per VXL section:  section <i> <name> <size_x> <size_y> <size_z> <normals_mode> <scale-bits>
then one line per present voxel, sorted by (z, y, x):  <x> <y> <z> <color> <normal>
for HVA:          hva <frame_count> <section_count>
then, per matrix in raw FILE order, 3 lines of 4 float-bit values each.
```

(The HVA matrices are dumped in FILE order on purpose: it sidesteps the frame-vs-section-major question — see the ground-truth section — and is unambiguous for the 1-frame/1-section htnk.hva. The VXL palette has no dump line: the palette path is already pinned by the synthetic entry-7 `<<2` test in Task 3 plus the existing `docs/re/pal.md` RE gate that `Palette::load` carries.)

- [ ] **Step 1: Write the failing (skipping) test.** In `D:\ra2-android\tests\test_vxl.cpp` replace the include block

```cpp
#include "doctest.h"
#include "ra2assets/hva.hpp"
#include "ra2assets/vxl.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;
```

with

```cpp
#include "doctest.h"
#include "ra2assets/hva.hpp"
#include "ra2assets/vxl.hpp"
#include "ra2assets/mix.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ra2assets;
```

Then append at the end of the file:

```cpp
// ---------------------------------------------------------------------------
// Guarded real-asset oracle: decode the real htnk.vxl + htnk.hva (Rhino tank,
// resolved from ra2.mix -> local.mix -- NOTE: voxels live in local.mix, NOT
// conquer.mix) and compare a canonical text dump line-for-line against ground
// truth produced by XCC's own VXL/HVA code (built verbatim, OUTSIDE this repo,
// into xcc_vxl_dump -- see scripts/make_vxl_oracle.md). Floats are compared as
// raw IEEE-754 bit patterns so no formatting can diverge. Skips cleanly when
// the oracle files or the install are absent.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> slurp_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

static std::vector<std::string> read_lines(const fs::path& p) {
    std::ifstream f(p);
    std::vector<std::string> out;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

static std::string fbits(float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08x", v);
    return buf;
}

static void canonical_vxl(const VxlModel& m, std::vector<std::string>& out) {
    char line[160];
    for (size_t i = 0; i < m.sections.size(); i++) {
        const VxlSection& s = m.sections[i];
        std::snprintf(line, sizeof line, "section %u %s %d %d %d %d %s",
                      (unsigned)i, s.name.c_str(), s.size_x, s.size_y, s.size_z,
                      (int)s.normals_mode, fbits(s.scale).c_str());
        out.push_back(line);
        for (int z = 0; z < s.size_z; z++)
            for (int y = 0; y < s.size_y; y++)
                for (int x = 0; x < s.size_x; x++) {
                    const VxlVoxel& v = s.voxels[(size_t)x + (size_t)s.size_x *
                                                 ((size_t)y + (size_t)s.size_y * (size_t)z)];
                    if (!v.present) continue;
                    std::snprintf(line, sizeof line, "%d %d %d %d %d",
                                  x, y, z, (int)v.color, (int)v.normal);
                    out.push_back(line);
                }
    }
}

static void canonical_hva(const HvaModel& m, std::vector<std::string>& out) {
    char line[160];
    std::snprintf(line, sizeof line, "hva %d %u",
                  m.frame_count, (unsigned)m.section_names.size());
    out.push_back(line);
    // Frame-major model emission == raw file order under our frame-major read.
    for (const auto& frame : m.frames)
        for (const auto& xf : frame)
            for (int r = 0; r < 3; r++) {
                std::snprintf(line, sizeof line, "%s %s %s %s",
                              fbits(xf.transform[r][0]).c_str(),
                              fbits(xf.transform[r][1]).c_str(),
                              fbits(xf.transform[r][2]).c_str(),
                              fbits(xf.transform[r][3]).c_str());
                out.push_back(line);
            }
}

static int compare_lines(const std::vector<std::string>& got,
                         const std::vector<std::string>& want) {
    REQUIRE(got.size() == want.size());
    size_t mismatches = 0;
    for (size_t i = 0; i < got.size(); i++) {
        if (got[i] != want[i]) {
            mismatches++;
            if (mismatches <= 5)
                MESSAGE("line " << i << ": got '" << got[i] << "' want '" << want[i] << "'");
        }
    }
    CHECK(mismatches == 0);
    return (int)got.size();
}

TEST_CASE("vxl oracle: htnk.vxl + htnk.hva match the XCC dump line-for-line") {
    const char* od = std::getenv("RA2_ORACLE_DIR");
    fs::path base = od ? od : "D:\\ra2-android\\oracle";
    fs::path vxl_txt = base / "vxl" / "htnk.vxl.txt";
    fs::path hva_txt = base / "vxl" / "htnk.hva.txt";
    if (!fs::exists(vxl_txt) || !fs::exists(hva_txt)) {
        MESSAGE("vxl oracle files absent; skipped");
        return;
    }
    const char* id = std::getenv("RA2_INSTALL_DIR");
    fs::path install = id ? id : "D:\\Red Alert 2";
    if (!fs::exists(install / "ra2.mix")) {
        MESSAGE("RA2 install absent; skipped");
        return;
    }

    auto mixbytes = slurp_file(install / "ra2.mix");
    REQUIRE_FALSE(mixbytes.empty());
    MixFile outer = MixFile::open(std::move(mixbytes));
    MixFile local = outer.open_nested("local.mix");

    int lines_checked = 0;
    {   // VXL
        const MixEntry* e = local.find("htnk.vxl");
        REQUIRE(e != nullptr);
        std::vector<uint8_t> raw = local.read(*e);
        REQUIRE(raw.size() == 34550);          // byte-verified size in this install
        VxlModel m = load_vxl(raw.data(), raw.size());
        std::vector<std::string> got;
        canonical_vxl(m, got);
        lines_checked += compare_lines(got, read_lines(vxl_txt));
    }
    {   // HVA
        const MixEntry* e = local.find("htnk.hva");
        REQUIRE(e != nullptr);
        std::vector<uint8_t> raw = local.read(*e);
        REQUIRE(raw.size() == 88);             // byte-verified size in this install
        HvaModel h = load_hva(raw.data(), raw.size());
        CHECK(h.frame_count == 1);
        CHECK(h.section_names.size() == 1);
        std::vector<std::string> got;
        canonical_hva(h, got);
        lines_checked += compare_lines(got, read_lines(hva_txt));
    }
    CHECK(lines_checked > 0);   // guard against silently comparing nothing
}
```

- [ ] **Step 2: Build and verify the clean skip** (oracle files don't exist yet; PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="vxl oracle*"
```

Expected: build succeeds; the run prints `MESSAGE: vxl oracle files absent; skipped`, `test cases: 1 | 1 passed | 0 failed`, final line `[doctest] Status: SUCCESS!`. (This IS the red step for an oracle gate: it proves the guard, then Steps 3-6 arm it.)

- [ ] **Step 3: Build the off-repo oracle tool.** Create `D:\ra2-re\oracle-tool\xcc_shim.h` (same types-only shim as the Task-9 SHP tool):

```cpp
// xcc_shim.h -- types-only portability shim for the embedded XCC excerpts.
// Zero decode logic. MinGW-w64 g++ already accepts the MSVC __int8/16/32/64
// sized types and #pragma pack (proven by the Task-9 SHP oracle tool).
#pragma once
typedef unsigned char byte;
```

Generate `D:\ra2-re\oracle-tool\xcc_verbatim.h` by extracting the struct declarations VERBATIM from the pinned XCC checkout — run in **Git Bash** (text processing only, no exes):

```bash
mkdir -p /d/ra2-re/oracle-tool
cd /d/ra2-re/oracle-tool
XCC=/d/ra2-re/reference/xcc/misc
{
  echo '// xcc_verbatim.h -- struct layouts extracted VERBATIM from the XCC'
  echo '// utilities source tree at D:\ra2-re\reference\xcc (GPLv3, by Olaf van der Spek,'
  echo '// 2000). Generated by the commands in'
  echo '// D:\ra2-android\scripts\make_vxl_oracle.md. This file and the tool'
  echo '// including it live OUTSIDE the ra2-android repo and are never'
  echo '// committed (no GPL code in-repo).'
  echo '#pragma once'
  echo '#include "xcc_shim.h"'
  echo '#pragma pack(push, 1)'
  awk '/^struct t_palet_entry/,/^};/'                          "$XCC/palet.h"
  grep '^using t_palet = '                                     "$XCC/palet.h"
  awk '/^struct t_hva_header/,/^using t_hva_transform_matrix/' "$XCC/cc_structures.h"
  awk '/^struct t_vxl_header/,/^const char vxl_id/'            "$XCC/cc_structures.h"
  echo '#pragma pack(pop)'
} > xcc_verbatim.h
grep -c '^struct t_' xcc_verbatim.h
```

Expected: the final `grep -c` prints `5` (t_palet_entry, t_hva_header, t_vxl_header, t_vxl_section_header, t_vxl_section_tailer). If it prints anything else, open `xcc_verbatim.h` and compare against the named declarations in the two source headers before proceeding.

Create `D:\ra2-re\oracle-tool\xcc_vxl_dump.cpp`:

```cpp
// xcc_vxl_dump.cpp -- standalone VXL/HVA ground-truth dumper for the
// ra2assets oracle gate (Task-9 pattern; see
// D:\ra2-android\scripts\make_vxl_oracle.md).
//
// Lives OUTSIDE the ra2-android repo (D:\ra2-re\oracle-tool\) because it
// embeds/transcribes GPLv3 source from the XCC utilities (by Olaf van der Spek,
// 2000). Verification tool only; nothing from it ships.
//
// Provenance:
//   * xcc_verbatim.h        -- structs extracted VERBATIM from
//                              misc/palet.h + misc/cc_structures.h.
//   * VxlView accessors     -- 1:1 transcription of Cvxl_file's inline
//                              getters (misc/vxl_file.h).
//   * collect_section_voxels-- the span walk transcribed line-for-line from
//                              Cvxl_file::extract_as_text (misc/vxl_file.cpp);
//                              only change: voxels are collected into a vector
//                              instead of streamed to an ostream.
//   * validity checks       -- Cvxl_file::is_valid (misc/vxl_file.cpp) and
//                              Chva_file::is_valid (misc/hva_file.cpp).
//
// Usage:
//   xcc_vxl_dump vxl <in.vxl> <out.txt>
//   xcc_vxl_dump hva <in.hva> <out.txt>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "xcc_shim.h"
#include "xcc_verbatim.h"

static_assert(sizeof(t_vxl_header) == 802, "t_vxl_header must pack to 802 bytes");
static_assert(sizeof(t_vxl_section_header) == 28, "t_vxl_section_header must pack to 28 bytes");
static_assert(sizeof(t_vxl_section_tailer) == 92, "t_vxl_section_tailer must pack to 92 bytes");
static_assert(sizeof(t_hva_header) == 24, "t_hva_header must pack to 24 bytes");

static std::vector<unsigned char> slurp(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> b((size_t)(n > 0 ? n : 0));
    if (!b.empty() && std::fread(b.data(), 1, b.size(), f) != b.size()) b.clear();
    std::fclose(f);
    return b;
}

// Canonical float text: the raw IEEE-754 bit pattern (matches the test side).
static std::string fbits(float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08x", v);
    return buf;
}

struct Voxel { int x, y, z, color, normal; };

// 1:1 transcription of Cvxl_file's inline accessors (misc/vxl_file.h, GPLv3)
// over a plain byte buffer; `size` is the full file size, exactly like XCC's
// Ccc_file_sh::get_size().
struct VxlView {
    const unsigned char* d;
    int size;
    const t_vxl_header& header() const { return *(const t_vxl_header*)d; }
    const byte* section_body() const {
        return d + sizeof(t_vxl_header) +
               sizeof(t_vxl_section_header) * header().c_section_tailers;
    }
    const t_vxl_section_header* section_header(int i) const {
        return (const t_vxl_section_header*)(d + sizeof(t_vxl_header) +
                                             sizeof(t_vxl_section_header) * i);
    }
    const t_vxl_section_tailer* section_tailer(int i) const {
        return (const t_vxl_section_tailer*)(d + size +
            sizeof(t_vxl_section_tailer) * (i - (int)header().c_section_tailers));
    }
    const int* span_start_list(int i) const {
        return (const int*)(section_body() + section_tailer(i)->span_start_ofs);
    }
    const byte* span_data(int i, int j) const {
        if (span_start_list(i)[j] == -1) return 0;
        return section_body() + section_tailer(i)->span_data_ofs + span_start_list(i)[j];
    }
};

// Span walk transcribed line-for-line from Cvxl_file::extract_as_text
// (misc/vxl_file.cpp, GPLv3); the only change is collecting into `out`.
static void collect_section_voxels(const VxlView& f, int i, std::vector<Voxel>& out) {
    const t_vxl_section_tailer& t = *f.section_tailer(i);
    const int cx = t.cx, cy = t.cy, cz = t.cz;
    int j = 0;
    for (int y = 0; y < cy; y++) {
        for (int x = 0; x < cx; x++) {
            const byte* r = f.span_data(i, j);
            if (r) {
                int z = 0;
                while (z < cz) {
                    z += *r++;
                    int c = *r++;
                    while (c--) {
                        int color = *r++;
                        int normal = *r++;
                        out.push_back(Voxel{x, y, z, color, normal});
                        z++;
                    }
                    r++;
                }
            }
            j++;
        }
    }
}

static int dump_vxl(const std::vector<unsigned char>& b, std::FILE* out) {
    if (b.size() < sizeof(t_vxl_header)) { std::fprintf(stderr, "vxl too small\n"); return 1; }
    VxlView f{ b.data(), (int)b.size() };
    const t_vxl_header& h = f.header();
    // Cvxl_file::is_valid checks (misc/vxl_file.cpp): magic, one==1, equal
    // header/tailer counts, exact size arithmetic, unknown==0x1f10.
    if (std::strcmp(h.id, vxl_id) != 0 || h.one != 1 ||
        h.c_section_headers != h.c_section_tailers ||
        (int)h.size != (int)b.size() - (int)sizeof(t_vxl_header)
            - (int)h.c_section_headers * (int)sizeof(t_vxl_section_header)
            - (int)h.c_section_tailers * (int)sizeof(t_vxl_section_tailer) ||
        h.unknown != 0x1f10) {
        std::fprintf(stderr, "vxl failed XCC is_valid checks\n");
        return 1;
    }
    for (int i = 0; i < (int)h.c_section_headers; i++) {
        const t_vxl_section_header& sh = *f.section_header(i);
        const t_vxl_section_tailer& t = *f.section_tailer(i);
        char name[17];
        std::memcpy(name, sh.id, 16);
        name[16] = 0;
        std::fprintf(out, "section %d %s %d %d %d %d %s\n", i, name,
                     (int)t.cx, (int)t.cy, (int)t.cz,
                     (int)(unsigned char)t.unknown, fbits(t.scale).c_str());
        std::vector<Voxel> vox;
        collect_section_voxels(f, i, vox);
        std::sort(vox.begin(), vox.end(), [](const Voxel& a, const Voxel& b2) {
            if (a.z != b2.z) return a.z < b2.z;
            if (a.y != b2.y) return a.y < b2.y;
            return a.x < b2.x;
        });
        for (const Voxel& v : vox)
            std::fprintf(out, "%d %d %d %d %d\n", v.x, v.y, v.z, v.color, v.normal);
    }
    return 0;
}

static int dump_hva(const std::vector<unsigned char>& b, std::FILE* out) {
    if (b.size() < sizeof(t_hva_header)) { std::fprintf(stderr, "hva too small\n"); return 1; }
    const t_hva_header& h = *(const t_hva_header*)b.data();
    // Chva_file::is_valid (misc/hva_file.cpp): nonzero counts and
    // sizeof(t_hva_header) + (48*c_frames + 16)*c_sections == size.
    if (h.c_frames <= 0 || h.c_sections <= 0 ||
        sizeof(t_hva_header) + (size_t)(48 * h.c_frames + 16) * h.c_sections != b.size()) {
        std::fprintf(stderr, "hva failed XCC is_valid checks\n");
        return 1;
    }
    std::fprintf(out, "hva %d %d\n", (int)h.c_frames, (int)h.c_sections);
    // Matrices in raw FILE order (3 lines of 4 bit-exact floats per matrix):
    // unambiguous for 1 frame x 1 section; the frame/section-major question is
    // settled by the game-loader RE (docs/re/vxl.md), not by this dump.
    const float* m = (const float*)(b.data() + sizeof(t_hva_header) + 16 * (size_t)h.c_sections);
    const long total = (long)h.c_frames * (long)h.c_sections;
    for (long k = 0; k < total; k++)
        for (int row = 0; row < 3; row++)
            std::fprintf(out, "%s %s %s %s\n",
                         fbits(m[12 * k + 4 * row + 0]).c_str(),
                         fbits(m[12 * k + 4 * row + 1]).c_str(),
                         fbits(m[12 * k + 4 * row + 2]).c_str(),
                         fbits(m[12 * k + 4 * row + 3]).c_str());
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: xcc_vxl_dump vxl|hva <in> <out.txt>\n");
        return 2;
    }
    std::vector<unsigned char> b = slurp(argv[2]);
    if (b.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
    std::FILE* out = std::fopen(argv[3], "wb");   // "wb": LF-only output
    if (!out) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 2; }
    int rc;
    if (std::strcmp(argv[1], "vxl") == 0)      rc = dump_vxl(b, out);
    else if (std::strcmp(argv[1], "hva") == 0) rc = dump_hva(b, out);
    else { std::fprintf(stderr, "unknown mode %s\n", argv[1]); rc = 2; }
    std::fclose(out);
    return rc;
}
```

Build it (Git Bash; static so it runs without MinGW DLLs on PATH):

```bash
cd /d/ra2-re/oracle-tool
g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ \
    xcc_vxl_dump.cpp -o xcc_vxl_dump.exe
ls -la xcc_vxl_dump.exe
```

Expected: compiles with no errors (the four `static_assert`s prove the packed layouts: 802/28/92/24) and `xcc_vxl_dump.exe` exists. If a `static_assert` fires, the awk extraction grabbed the wrong declarations — regenerate `xcc_verbatim.h` and diff it against the named structs in `misc/cc_structures.h`/`misc/palet.h`.

- [ ] **Step 4: Extract the real assets and generate the oracle files** (PowerShell — exes must run from PowerShell; build `ra2extract` first if missing; **each `ra2extract` invocation re-reads the 268 MB ra2.mix and takes ~20 s** — the two calls below are the whole batch):

```powershell
Set-Location D:\ra2-android
cmake --build build --target ra2extract
New-Item -ItemType Directory -Force D:\ra2-re\raw | Out-Null
New-Item -ItemType Directory -Force D:\ra2-android\oracle\vxl | Out-Null
& .\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --raw local.mix/htnk.vxl D:\ra2-re\raw\htnk.vxl
& .\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --raw local.mix/htnk.hva D:\ra2-re\raw\htnk.hva
(Get-Item D:\ra2-re\raw\htnk.vxl).Length
(Get-Item D:\ra2-re\raw\htnk.hva).Length
& D:\ra2-re\oracle-tool\xcc_vxl_dump.exe vxl D:\ra2-re\raw\htnk.vxl D:\ra2-android\oracle\vxl\htnk.vxl.txt
& D:\ra2-re\oracle-tool\xcc_vxl_dump.exe hva D:\ra2-re\raw\htnk.hva D:\ra2-android\oracle\vxl\htnk.hva.txt
Get-Content D:\ra2-android\oracle\vxl\htnk.vxl.txt -TotalCount 3
Get-Content D:\ra2-android\oracle\vxl\htnk.hva.txt -TotalCount 2
(Get-Content D:\ra2-android\oracle\vxl\htnk.vxl.txt).Count
```

Expected: lengths `34550` and `88`; both dump commands exit silently (no `failed XCC is_valid` on stderr); `htnk.vxl.txt` starts with `section 0 <name> <sx> <sy> <sz> <normals> 0x........` followed by `x y z color normal` integer lines (total line count in the thousands — a real tank has thousands of voxels); `htnk.hva.txt` starts with `hva 1 1` followed by 3 matrix-row lines of 4 `0x........` values.

- [ ] **Step 5: Run the armed oracle test** (PowerShell; ~30-60 s — it slurps the 268 MB ra2.mix once):

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="vxl oracle*"
```

Expected: `test cases: 1 | 1 passed | 0 failed`, final line `[doctest] Status: SUCCESS!` (no `MESSAGE:` skip lines, no mismatch lines). If any `line N: got ... want ...` mismatches print, STOP and debug with the systematic-debugging skill — the game-shape decoder and XCC's decoder disagree, which is exactly what this gate exists to catch; re-check the walk against `docs/re/vxl.md` before touching the test.

- [ ] **Step 6: Write `scripts/make_vxl_oracle.md`** (in-repo generation doc — instructions only, no GPL code) with exactly:

```markdown
# Generating the VXL/HVA oracle (htnk)

The oracle test in `tests/test_vxl.cpp` ("vxl oracle: ...") compares a
canonical text dump of `ra2assets::load_vxl` / `load_hva` output for the real
`htnk.vxl` + `htnk.hva` (Rhino tank) against ground truth produced by **XCC's
own VXL/HVA code**. The oracle set lives in the git-ignored `oracle/`
directory and is **never committed** (derived from EA's game assets).

* `oracle/vxl/htnk.vxl.txt` + `oracle/vxl/htnk.hva.txt` — ground truth.
* Assets resolve from `ra2.mix -> local.mix` (voxels live in **local.mix**,
  NOT conquer.mix).
* The test skips cleanly (doctest MESSAGE) when these files or the install
  are absent, and guards `lines_checked > 0` against silent skips.

## Canonical dump format (identical on both sides)

* Floats are printed as the raw IEEE-754 bit pattern (`0x%08x` of the f32
  bits) so float-to-decimal formatting can never diverge.
* VXL, per section: `section <i> <name> <size_x> <size_y> <size_z>
  <normals_mode> <scale-bits>`, then one line per present voxel
  `<x> <y> <z> <color> <normal>` sorted by (z, y, x).
* HVA: `hva <frame_count> <section_count>`, then per matrix in raw FILE
  order, 3 lines of 4 float-bit values. (File order sidesteps the
  frame-major vs section-major question — see `docs/re/vxl.md`.)
* The palette has no dump line: the palette path is pinned by the synthetic
  entry-7 `<<2` test in `tests/test_vxl.cpp` plus `docs/re/pal.md`.

## Build the standalone oracle tool (OUTSIDE this repo — XCC is GPLv3)

Location: `D:\ra2-re\oracle-tool\`. Files: `xcc_shim.h` (types-only shim:
`typedef unsigned char byte;`), `xcc_verbatim.h` (struct declarations
extracted VERBATIM from the XCC checkout by the commands below), and
`xcc_vxl_dump.cpp` (accessors + span walk transcribed 1:1 from XCC
`Cvxl_file`/`Chva_file` with provenance comments; validity checks from
`is_valid`). The tool's source is specified, in full, in the plan
`docs/superpowers/plans/2026-07-09-vxl-hva-decoder.md` (Task 5).

Extract the verbatim structs (Git Bash):

    cd /d/ra2-re/oracle-tool
    XCC=/d/ra2-re/reference/xcc/misc
    {
      echo '// xcc_verbatim.h -- struct layouts extracted VERBATIM from the XCC'
      echo '// utilities source tree at D:\ra2-re\reference\xcc (GPLv3, by Olaf van der Spek,'
      echo '// 2000). Generated by the commands in'
      echo '// D:\ra2-android\scripts\make_vxl_oracle.md. This file and the tool'
      echo '// including it live OUTSIDE the ra2-android repo and are never'
      echo '// committed (no GPL code in-repo).'
      echo '#pragma once'
      echo '#include "xcc_shim.h"'
      echo '#pragma pack(push, 1)'
      awk '/^struct t_palet_entry/,/^};/'                          "$XCC/palet.h"
      grep '^using t_palet = '                                     "$XCC/palet.h"
      awk '/^struct t_hva_header/,/^using t_hva_transform_matrix/' "$XCC/cc_structures.h"
      awk '/^struct t_vxl_header/,/^const char vxl_id/'            "$XCC/cc_structures.h"
      echo '#pragma pack(pop)'
    } > xcc_verbatim.h
    grep -c '^struct t_' xcc_verbatim.h     # must print 5

Build (static-linked; the static_asserts in the .cpp pin the packed sizes
802/28/92/24):

    g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ \
        xcc_vxl_dump.cpp -o xcc_vxl_dump.exe

## Extract the raw assets and dump the oracle (PowerShell)

Each `ra2extract` invocation re-reads the 268 MB `ra2.mix` (~20 s each), so
batch all extractions together:

    Set-Location D:\ra2-android
    cmake --build build --target ra2extract
    New-Item -ItemType Directory -Force D:\ra2-re\raw | Out-Null
    New-Item -ItemType Directory -Force D:\ra2-android\oracle\vxl | Out-Null
    & .\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --raw local.mix/htnk.vxl D:\ra2-re\raw\htnk.vxl
    & .\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --raw local.mix/htnk.hva D:\ra2-re\raw\htnk.hva
    & D:\ra2-re\oracle-tool\xcc_vxl_dump.exe vxl D:\ra2-re\raw\htnk.vxl D:\ra2-android\oracle\vxl\htnk.vxl.txt
    & D:\ra2-re\oracle-tool\xcc_vxl_dump.exe hva D:\ra2-re\raw\htnk.hva D:\ra2-android\oracle\vxl\htnk.hva.txt

Expected sizes: `htnk.vxl` = 34,550 bytes, `htnk.hva` = 88 bytes
(`802 + 1*28 + 33,628 + 1*92 = 34,550`). Then run the gate from PowerShell:

    $env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="vxl oracle*"
```

- [ ] **Step 7: Run the full suite with assets** (PowerShell; the SHP/TMP oracle suites also run, total a few minutes):

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected: `0 failed`, final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 8: Commit** (the oracle .txt files are inside git-ignored `oracle/` and must NOT be committed; verify with `git status` that only the two files below are staged):

```bash
git add tests/test_vxl.cpp scripts/make_vxl_oracle.md
git commit -m "test(vxl): real-asset oracle vs XCC dump for htnk.vxl/htnk.hva + generation doc"
```

---

### Task 6: Finalize — RE doc confidence table, README bullet, wiring check, full green suite

**Files:**
- Modify: `docs/re/vxl.md` (append oracle results, finalize confidence), `README.md` (library description mentions VXL/HVA)

**Interfaces:**
- Consumes: the Task-5 oracle run results and the Task-1 findings.
- Produces: nothing new in code — this is the merge-ready gate for the whole piece.

- [ ] **Step 1: Append the oracle results to `docs/re/vxl.md`.** Compute the numbers first (PowerShell):

```powershell
(Get-Content D:\ra2-android\oracle\vxl\htnk.vxl.txt).Count
(Get-Content D:\ra2-android\oracle\vxl\htnk.hva.txt).Count
Get-Content D:\ra2-android\oracle\vxl\htnk.vxl.txt -TotalCount 1
```

Append this section at the end of `docs/re/vxl.md`, replacing each `<FILL: ...>` with the values just printed and the suite result:

```markdown
## Oracle: line-for-line parity with XCC's decoder (htnk)

`tests/test_vxl.cpp` ("vxl oracle: ...") decodes the real `htnk.vxl` (34,550
bytes) + `htnk.hva` (88 bytes) from `ra2.mix -> local.mix` and compares a
canonical dump (voxels sorted by z,y,x; floats as raw IEEE-754 bit patterns)
against XCC's own `Cvxl_file`/`Chva_file` logic built into an off-repo tool
(`scripts/make_vxl_oracle.md`).

**Result: <FILL: N> VXL lines (section header + every present voxel's
x/y/z/color/normal) + <FILL: M> HVA lines, 0 mismatches.** htnk section 0:
<FILL: paste the `section 0 ...` line>. The test skips cleanly (doctest
MESSAGE) without the oracle or install, and `lines_checked > 0` guards
against a silent skip.
```

Then revisit the `## Confidence summary` table written in Task 1 and finalize every rating now that the oracle has run (e.g. layout/span-walk rows earn their final High/Medium; anything the hunt failed to locate stays honestly Low with the oracle carrying correctness). Also resolve or restate the "HVA matrix order" verdict line with the final evidence.

- [ ] **Step 2: Add the README mention.** In `D:\ra2-android\README.md`, replace

```markdown
(`ra2assets`: MIX archives, PAL palettes, SHP sprites, LZO/LCW map packs, INI,
rules, `.map` cell grids, TMP theater tiles), plus `ra2engine` — an SDL2
```

with

```markdown
(`ra2assets`: MIX archives, PAL palettes, SHP sprites, VXL/HVA voxel models
(vehicle meshes + animation transforms, decode only), LZO/LCW map packs, INI,
rules, `.map` cell grids, TMP theater tiles), plus `ra2engine` — an SDL2
```

(match the existing em-dash character in `ra2engine —` exactly when editing).

- [ ] **Step 3: Double-check the CMake wiring** (both new library sources in the shared list; the test registered):

```powershell
Select-String -Path D:\ra2-android\cmake\ra2_sources.cmake -Pattern "hva.cpp|vxl.cpp"
Select-String -Path D:\ra2-android\CMakeLists.txt -Pattern "test_vxl.cpp"
```

Expected: two hits in `ra2_sources.cmake` (`${RA2_ROOT}/src/hva.cpp`, `${RA2_ROOT}/src/vxl.cpp`) inside the `RA2_LIB_SOURCES` list, and one `target_sources(ra2assets_tests PRIVATE tests/test_vxl.cpp)` hit in `CMakeLists.txt`. If either is missing, add the line as specified in Tasks 2/3 before proceeding.

- [ ] **Step 4: Full suite green, both with and without assets** (PowerShell, from `D:\ra2-android`):

```powershell
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
Remove-Item Env:RA2_INSTALL_DIR; & .\build\ra2assets_tests.exe
```

Expected: both runs end `[doctest] Status: SUCCESS!` with `0 failed`; the second run prints the guarded suites' skip MESSAGEs (including `vxl oracle files absent; skipped` only if `oracle/` was moved — with the oracle present but no install env var the install default still resolves, which is fine either way; what matters is `0 failed`).

- [ ] **Step 5: Commit**

```bash
git add docs/re/vxl.md README.md
git commit -m "docs(vxl): finalize RE confidence table with oracle results; README voxel bullet"
```

---

## Self-review notes (kept for the executing agents)

- **Spec coverage:** interface copied verbatim from the spec's sub-project 1 (`VxlVoxel`/`VxlSection`/`VxlModel`/`load_vxl`, `HvaSectionXform`/`HvaModel`/`load_hva`); Ghidra-exact gate = Task 1 (+ THE-GAME-WINS correction rule); real-asset oracle on `htnk.vxl`/`htnk.hva` from `ra2.mix/local.mix` = Task 5; `docs/re/vxl.md` with confidence ratings = Tasks 1 + 6; pure library + cmake wiring = Tasks 2-3 + 6; skip-clean tests throughout.
- **Type consistency:** `load_hva(const uint8_t*, size_t)` and `load_vxl(const uint8_t*, size_t)` are used with exactly these signatures in Tasks 2, 3, 4 and 5; the voxel indexing formula `x + size_x*(y + size_y*z)` appears identically in `vxl.hpp`, `decode_spans`, the Task-4 tests, and `canonical_vxl`.
- **Known risk consciously carried:** the HVA frame-major vs section-major question (flagged in the ground-truth section) cannot be settled by the 1x1 htnk.hva; the oracle dumps file order so it cannot mask the issue, Task 1 attacks it in the binary, and the verdict is recorded either way in `docs/re/vxl.md`.
