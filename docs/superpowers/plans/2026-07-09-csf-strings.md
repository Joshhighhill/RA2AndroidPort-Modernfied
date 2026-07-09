# CSF String-Table Reader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a pure-library CSF string-table reader (`ra2assets::CsfStrings`) that maps ASCII labels to UTF-16 display strings, Ghidra-calibrated against the game's own loader and oracle-verified against the install's loose `ra2.csf`.

**Architecture:** One new pure C++17 decoder in the `ra2assets` library (`include/ra2assets/csf.hpp` + `src/csf.cpp`) that parses a whole CSF file image from a byte span into an `unordered_map<std::string, std::u16string>` (labels uppercased for case-insensitive lookup, first value per label only). Synthetic-byte doctest cases are the primary TDD driver; a guarded real-asset test checks known vectors from `D:\Red Alert 2\ra2.csf`. A read-only Ghidra hunt in the existing `D:\ra2-re` project locates the game's CSF loader and documents its validation/inversion/case behavior in `docs/re/csf.md`.

**Tech Stack:** C++17, CMake + Ninja + MinGW-w64, doctest; Ghidra 11.2.1 headless (Jython) for the RE task.

## Global Constraints

- C++17, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF` is already set).
- Pure library: no SDL/Win32/OS/DirectX includes anywhere under `include/ra2assets/` or `src/` — pure functions over byte spans.
- No game assets, no game binary, no Ghidra project, no GPL code committed. Everything RE lives under `D:\ra2-re\`, never in the repo; the repo gets prose + short illustrative excerpts only.
- Real-asset tests skip cleanly (doctest `MESSAGE` + `return`) when assets are absent, so asset-free CI still builds and passes. Assets are found via `RA2_INSTALL_DIR` (default `D:\Red Alert 2`).
- The test exe MUST be run from PowerShell, not Git Bash (known environment issue). Ghidra `analyzeHeadless.bat` runs from Git Bash per `docs/re/workflow.md`.
- Every new `src/*.cpp` is added to `cmake/ra2_sources.cmake` `RA2_LIB_SOURCES` (single source list shared by desktop + Android builds); every new `tests/*.cpp` is added to `CMakeLists.txt` via `target_sources(ra2assets_tests PRIVATE tests/...)`.
- TDD: failing test first, then minimal implementation, then green.
- Pristine test output (intentional skip `MESSAGE`s are fine; no stray prints/warnings).
- Commit after every task (conventional commits: `feat:` / `test:` / `docs:` / `perf:`).

## Context — key reference files (read before implementing)

- `tests/test_theater.cpp` — the synthetic-byte-builder test pattern to imitate (`make_tmp`, `p32` lambda style).
- `tests/test_oracle.cpp` — the real-asset skip-cleanly pattern (`MESSAGE(...); return;`, `RA2_INSTALL_DIR`, final anti-silent-skip `CHECK`).
- `src/theater.cpp` — library code style: static `r32()` little-endian reader, bounds checks throwing `std::runtime_error` with context strings, on-disk struct-size constants with provenance comments.
- `docs/re/workflow.md` — Ghidra headless methodology (JDK 21, `-process gamemd.exe -noanalysis`, byte-pattern search beats string xrefs).
- `docs/re/shp.md` — the RE-doc format `docs/re/csf.md` must imitate.

## Format ground truth (byte-verified against the install's `ra2.csf`, 491,826 bytes, 2026-07-09)

All multi-byte integers little-endian, structures packed, records strictly sequential:

- **File header, 24 bytes:** `char magic[4] = " FSC"` (bytes `0x20 0x46 0x53 0x43`) @0; `u32 version` @4 (= 3 here); `u32 num_labels` @8 (= 4562 here); `u32 num_strings` @12 (= 4562); `u32 unused` @16; `u32 language` @20 (= 0).
- **Then `num_labels` label records:** `char magic[4] = " LBL"` (`0x20 0x4C 0x42 0x4C`); `u32 num_values`; `u32 label_len`; `label_len` bytes of ASCII label (NOT NUL-terminated).
- **Each label is followed by `num_values` value records:** `char magic[4] = " RTS"` (`0x20 0x52 0x54 0x53`) or `"WRTS"` (`0x57 0x52 0x54 0x53`); `u32 val_len` (in UTF-16 code units); `val_len*2` bytes of UTF-16LE text with EVERY BYTE XOR `0xFF` (equivalently each 16-bit unit bitwise-NOT). If magic is `"WRTS"` there is additionally `u32 extra_len` followed by `extra_len` bytes of ASCII (skipped — out of scope; only the first value is stored).
- All 4562 labels of the real file walk cleanly with exactly this structure (verified with an independent Python decode).
- **Label case:** the game resolves labels case-insensitively (`gui:cancel` and `GUI:Cancel` both work in its UI files). Default implementation: normalize keys to UPPERCASE (`std::toupper` on `unsigned char`) at insert AND lookup. Task 1's RE confirms or corrects this; if the game turns out case-sensitive, drop the normalization and record it in `docs/re/csf.md`.
- **Zero-value labels** (`num_values == 0`) are legal-looking on disk: walk them but store nothing (`find()` returns `nullptr`, `count()` excludes them). Documented in `csf.hpp`.
- **Known-good oracle vectors** (decoded from the real file): `GUI:Cancel` → `u"Cancel"`; `Name:HTNK` → `u"Rhino Heavy Tank"`; `Name:E1` → `u"GI"`; `GUI:LoadGame` → `u"Load Game"`. `count() == 4558` for this install (4,562 label records minus 4 case-insensitive duplicate pairs — `txt_defense_tab`, `txt_defense_tab_desc`, `txt_structure_tab`, `txt_structure_tab_desc` each appear in both cases; keep-first collapses them, and their existence is direct real-file evidence for the case-insensitive normalization). The oracle asserts `count() > 4000` (not exact) plus the four vectors, so other installs' CSF variants still pass.

---

### Task 1: Ghidra RE — locate the CSF loader in gamemd.exe

**Files:**
- Create: `D:/ra2-re/scripts/csf_dump.py` (OUTSIDE the repo — never `git add` this)
- Create: `docs/re/csf.md` (in repo — prose + short excerpts only, NEVER full decompilation dumps)

**Interfaces:** Consumes: the analyzed Ghidra project `D:\ra2-re` / `ra2` / `gamemd.exe` (image base `0x00400000`, YR 1.001, SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`). Produces: `docs/re/csf.md` — the game's header validation, bit-inversion loop, and label-case behavior; consumed as provenance comments by Tasks 2–5. An honestly-documented failed hunt (Low confidence) is an acceptable outcome — the Task-4 oracle still gates correctness, and the case-insensitive default stands unless the RE contradicts it.

**Anchor rationale:** the loader compares dword magics. Read as a little-endian dword, `" FSC"` is `0x43534620`; when encoded as an x86 immediate that dword is stored little-endian — instruction bytes `20 46 53 43`, i.e. the ASCII bytes `" FSC"` in file order. So one raw byte search per ASCII magic finds both data copies and `cmp`-immediate uses. Same for `" LBL"`, `" RTS"`, `"WRTS"`. Secondary anchor: the filename strings `"ra2.csf"` / `"ra2md.csf"`.

- [ ] **Step 1: Write the Jython post-script** — save the following COMPLETE file to `D:/ra2-re/scripts/csf_dump.py`:

```python
# csf_dump.py -- locate the CSF string-table loader in gamemd.exe (YR 1.001).
#
# Technique (docs/re/workflow.md: constant anchoring beats string xrefs):
# the CSF loader validates record magics " FSC" (file), " LBL" (label),
# " RTS"/"WRTS" (value) by comparing dwords. Read as a little-endian dword,
# " FSC" is 0x43534620; encoded as an x86 immediate that dword is stored
# little-endian, i.e. the instruction bytes contain 20 46 53 43 -- exactly
# the ASCII bytes " FSC" in file order. A plain byte search over every
# initialized block therefore finds BOTH data copies and cmp-immediate uses.
# Executable hits -> containing function; every hit also gets
# getReferencesTo -> referencing functions. All candidates are decompiled to
# D:/ra2-re/out/csf_<entry>.c with a csf_summary.txt index.
#
# Run (Git Bash):
#   export JAVA_HOME="/c/Program Files/Java/jdk-21"
#   GHIDRA="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC/support/analyzeHeadless.bat"
#   "$GHIDRA" "D:/ra2-re" ra2 -process gamemd.exe -noanalysis -readOnly \
#       -scriptPath "D:/ra2-re/scripts" -postScript csf_dump.py < /dev/null
import os
import jarray
from java.lang import String
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

OUT = "D:/ra2-re/out/"
if not os.path.isdir(OUT):
    os.makedirs(OUT)

mem = currentProgram.getMemory()
fm  = currentProgram.getFunctionManager()
log = open(OUT + "csf_summary.txt", "w")

MAGICS  = [" FSC", " LBL", " RTS", "WRTS"]   # ASCII bytes == LE immediate bytes
STRINGS = ["ra2.csf", "ra2md.csf", ".csf"]
MAX_DECOMPILES = 40

def block_text(block):
    n = int(block.getSize())
    buf = jarray.zeros(n, 'b')
    block.getBytes(block.getStart(), buf)
    # ISO-8859-1 maps bytes 1:1 to chars 0..255, so unicode .find() below is
    # a raw byte search (all patterns are pure ASCII -> promotion-safe).
    return unicode(String(buf, "ISO-8859-1"))

funcs = {}   # entry offset -> [function, set(tags)]

def note_function(f, tag):
    key = f.getEntryPoint().getOffset()
    if key not in funcs:
        funcs[key] = [f, set()]
    funcs[key][1].add(tag)

for block in mem.getBlocks():
    if not block.isInitialized():
        continue
    data = block_text(block)
    start = block.getStart()
    patterns = [(m, m) for m in MAGICS] + \
               [("str:" + s, s + "\x00") for s in STRINGS]
    for tag, pat in patterns:
        i = data.find(pat)
        while i >= 0:
            a = start.add(i)
            log.write("hit %s at %s (block %s, exec=%s)\n" %
                      (tag, a, block.getName(), block.isExecute()))
            if block.isExecute():
                f = fm.getFunctionContaining(a)
                if f is not None:
                    note_function(f, tag)
                else:
                    log.write("  (no defined function contains this hit)\n")
            for r in getReferencesTo(a):
                fa = r.getFromAddress()
                f = fm.getFunctionContaining(fa)
                log.write("  ref from %s (%s)\n" %
                          (fa, f.getName() if f is not None else "?"))
                if f is not None:
                    note_function(f, tag + "-ref")
            i = data.find(pat, i + 1)

log.write("\n%d candidate function(s)\n" % len(funcs))

decomp = DecompInterface()
decomp.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
count = 0
for key in sorted(funcs.keys()):
    f, tags = funcs[key]
    if count >= MAX_DECOMPILES:
        log.write("decompile cap (%d) reached; skipped %s\n" %
                  (MAX_DECOMPILES, f.getName()))
        continue
    res = decomp.decompileFunction(f, 120, mon)
    if res is not None and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
    else:
        c = "// decompile FAILED: " + (res.getErrorMessage() if res else "null result")
    path = OUT + "csf_%08x.c" % key
    fh = open(path, "w")
    fh.write("// %s  entry 0x%08x  tags: %s\n" %
             (f.getName(), key, ", ".join(sorted(tags))))
    fh.write(c)
    fh.close()
    log.write("decompiled 0x%08x %s -> %s (tags: %s)\n" %
              (key, f.getName(), path, ", ".join(sorted(tags))))
    count += 1

log.close()
print("csf_dump done: %d functions decompiled, see %scsf_summary.txt" % (count, OUT))
```

- [ ] **Step 2: Run the headless dump (Git Bash, NOT PowerShell)** — the Ghidra project is EXCLUSIVELY LOCKED while any headless run is open, and other parallel agents share it: **if the log reports the project is locked (`Unable to lock project` / `LockException`), wait 60 seconds and retry** — each dump run takes ~1–2 minutes.

```bash
export JAVA_HOME="/c/Program Files/Java/jdk-21"
GHIDRA="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC/support/analyzeHeadless.bat"
"$GHIDRA" "D:/ra2-re" ra2 -process gamemd.exe -noanalysis -readOnly \
    -scriptPath "D:/ra2-re/scripts" -postScript csf_dump.py < /dev/null
```

Expected: Ghidra log lines ending with the script's own print, e.g. `csf_dump done: N functions decompiled, see D:/ra2-re/out/csf_summary.txt` (N between 1 and 40), and no Java stack trace. If N is 0, the hunt found no candidates — proceed to Step 3/4 and document the failed hunt honestly (Low confidence).

- [ ] **Step 3: Identify the loader in the dumps** — read `D:/ra2-re/out/csf_summary.txt` and each `D:/ra2-re/out/csf_*.c`. The real CSF loader is the function (or small cluster) showing this shape:
  - a 24-byte header read with a compare against the `0x43534620` (`" FSC"`) dword and a read of `num_labels`;
  - an outer loop guarded by a `0x4C424C20` (`" LBL"`) compare that reads `num_values` and `label_len`;
  - an inner loop guarded by `0x53545220` (`" RTS"`) / `0x53545257` (`"WRTS"`) compares reading `val_len` and then a byte- or word-inversion loop (`~x` or `^ 0xFF`) over `val_len*2` bytes / `val_len` words;
  - label case handling: look for a `toupper`/`_strupr`-style loop, an `and 0xDF` bit trick, a case-insensitive compare (`_stricmp`), or a hash call applied to the label before table insert.

  Record: the function address(es), which header fields the game actually validates (does it check `version`? `num_strings`? does it accept any other magic?), the exact inversion construct, and the case behavior.

- [ ] **Step 4: Write `docs/re/csf.md`** — imitate `docs/re/shp.md` (prose + short excerpts, no full dumps). Use this skeleton VERBATIM, then replace every `«...»` with actual findings from Step 3 (or with the honest failed-hunt wording given below the skeleton):

```markdown
# CSF string table — reverse-engineering notes

(Method: `docs/re/workflow.md`. Hunt script: `D:/ra2-re/scripts/csf_dump.py`
— raw byte search for the ` FSC`/` LBL`/` RTS`/`WRTS` magics (their ASCII
bytes equal their little-endian immediate encodings) plus the
`ra2.csf`/`ra2md.csf` filename strings, over every initialized block; hits →
containing/referencing functions → decompiled to `D:/ra2-re/out/csf_*.c`.
Nothing under `D:/ra2-re` is committed.)

## File format (byte-verified against the install's ra2.csf, 491,826 bytes)

File header (24 bytes, packed, little-endian):

| offset | type    | field       | ra2assets read              |
|-------:|---------|-------------|-----------------------------|
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

| Function | Role | Evidence |
|----------|------|----------|
| «FUN_00xxxxxx» | «CSF file loader (header + label/value walk)» | «which magic compares / loops it contains» |
| «FUN_00xxxxxx» | «value decode / inversion» | «the ~ / XOR construct» |

## Game behavior confirmed

- **Header validation:** «what the decompiled loader actually checks — magic
  only? version? num_strings? any alternate magic accepted?»
- **Value decode (bit inversion):** «short excerpt (a few lines max) of the
  decompiled inversion loop, e.g. the `~` on each 16-bit unit» — matches
  `src/csf.cpp`'s byte-wise `^ 0xFF`.
- **Label case handling:** «uppercase-at-insert? case-insensitive compare?
  hash?» — ra2assets normalizes labels to ASCII uppercase at insert and
  lookup («confirmed by the above / NOT confirmed — retained as the default
  because the game's own UI files reference labels in mixed case»).

## Confidence

(Table finalized after the real-asset oracle runs — see the plan's Task 5.)
```

  **If the hunt failed** (no credible loader function found): keep the format section as-is (it is byte-verified independently of the RE), put a single row in "Located functions" — `| (not located) | — | searched all initialized blocks for the four magics + filename strings; N hits, none decompiled to a loader shape |` — state in "Game behavior confirmed" that header-validation and case behavior are UNCONFIRMED and the case-insensitive default stands on UI-file evidence, and note the Task-4 oracle is the correctness gate. That is an acceptable, honest outcome.

- [ ] **Step 5: Commit** (PowerShell):

```powershell
cd D:\ra2-android
git add docs/re/csf.md
git commit -m "docs: locate CSF loader in gamemd.exe (RE notes)"
```

---

### Task 2: CsfStrings core — synthetic TDD (load / find / count / case-insensitivity)

**Files:**
- Create: `include/ra2assets/csf.hpp`, `src/csf.cpp`, `tests/test_csf.cpp`
- Modify: `cmake/ra2_sources.cmake` (one line in `RA2_LIB_SOURCES`), `CMakeLists.txt` (one `target_sources` line)
- Test: `tests/test_csf.cpp`

**Interfaces:**
- Consumes: nothing from the library (self-contained decoder; no `MixFile`/`Palette` use — `ra2.csf` is a loose file).
- Produces (exact signatures Tasks 3–4 and Phases 10/11 rely on):
  - `static CsfStrings CsfStrings::load(const uint8_t* data, size_t len);` (throws `std::runtime_error` on structural errors)
  - `const std::u16string* CsfStrings::find(const std::string& label) const;` (case-insensitive; `nullptr` if absent)
  - `size_t CsfStrings::count() const;` (labels stored, i.e. labels with ≥1 value)

This task's implementation accepts `" RTS"` value records only (any other value magic throws — including `"WRTS"`, which Task 3 adds), but already walks ALL `num_values` records per label storing only value 0, and bounds-checks every read.

- [ ] **Step 1: Write the failing test** — create `tests/test_csf.cpp` with exactly this content:

```cpp
#include "doctest.h"
#include "ra2assets/csf.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;

// ---------------------------------------------------------------------------
// Synthetic CSF builder. Emits the exact on-disk layout (little-endian,
// packed; byte-verified against the install's ra2.csf -- see src/csf.cpp):
//   file header (24B): " FSC", u32 version=3, u32 num_labels, u32 num_strings,
//                      u32 unused=0, u32 language=0
//   per label: " LBL", u32 num_values, u32 label_len, label bytes (no NUL)
//   per value: " RTS" (or "WRTS"), u32 val_len (UTF-16 code units), then
//              val_len*2 bytes UTF-16LE with every byte XOR 0xFF; a "WRTS"
//              record adds u32 extra_len + extra_len ASCII bytes.
// ---------------------------------------------------------------------------
struct CsfValue {
    std::u16string text;
    bool wide = false;      // emit "WRTS" instead of " RTS"
    std::string extra;      // WRTS-only trailing ASCII payload
};
struct CsfLabel {
    std::string name;
    std::vector<CsfValue> values;
};

static std::vector<uint8_t> make_csf(const std::vector<CsfLabel>& labels) {
    std::vector<uint8_t> b;
    auto p8  = [&](uint8_t v){ b.push_back(v); };
    auto p32 = [&](uint32_t v){ for (int i=0;i<4;i++) b.push_back((uint8_t)((v>>(8*i))&0xff)); };
    auto pmagic = [&](const char* m){ for (int i=0;i<4;i++) b.push_back((uint8_t)m[i]); };
    // file header (24 bytes)
    pmagic(" FSC");                            // 0x20 0x46 0x53 0x43
    p32(3);                                    // version
    p32((uint32_t)labels.size());              // num_labels
    uint32_t num_strings = 0;
    for (const auto& l : labels) num_strings += (uint32_t)l.values.size();
    p32(num_strings);                          // num_strings
    p32(0);                                    // unused
    p32(0);                                    // language
    for (const auto& l : labels) {
        pmagic(" LBL");                        // 0x20 0x4C 0x42 0x4C
        p32((uint32_t)l.values.size());
        p32((uint32_t)l.name.size());
        for (char c : l.name) p8((uint8_t)c);
        for (const auto& v : l.values) {
            pmagic(v.wide ? "WRTS" : " RTS");
            p32((uint32_t)v.text.size());
            for (char16_t u : v.text) {        // UTF-16LE, every byte XOR 0xFF
                p8((uint8_t)((((uint16_t)u)      & 0xff) ^ 0xff));
                p8((uint8_t)((((uint16_t)u >> 8) & 0xff) ^ 0xff));
            }
            if (v.wide) {
                p32((uint32_t)v.extra.size());
                for (char c : v.extra) p8((uint8_t)c);
            }
        }
    }
    return b;
}

TEST_CASE("csf synthetic: two labels load, find, count, absent label") {
    auto bytes = make_csf({
        {"GUI:Ok",    {{u"OK"}}},
        {"Name:HTNK", {{u"Rhino Heavy Tank"}}},
    });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 2);
    const std::u16string* ok = cs.find("GUI:Ok");
    REQUIRE(ok != nullptr);
    CHECK(*ok == u"OK");
    const std::u16string* tank = cs.find("Name:HTNK");
    REQUIRE(tank != nullptr);
    CHECK(*tank == u"Rhino Heavy Tank");
    CHECK(cs.find("Name:E1") == nullptr);      // absent -> nullptr
}

TEST_CASE("csf synthetic: label lookup is case-insensitive") {
    auto bytes = make_csf({ {"GUI:Ok", {{u"OK"}}} });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    const std::u16string* a = cs.find("gui:ok");
    REQUIRE(a != nullptr);
    CHECK(*a == u"OK");
    const std::u16string* c = cs.find("GUI:OK");
    REQUIRE(c != nullptr);
    CHECK(*c == u"OK");
}

TEST_CASE("csf synthetic: non-ASCII UTF-16 units survive the XOR transform") {
    // A Latin-1 char (U+00E9), a CJK char (U+4E2D), and a space: exercises
    // both bytes of each 16-bit unit through the XOR-0xFF store.
    const std::u16string text = u"Café 中";
    auto bytes = make_csf({ {"TEST:Unicode", {{text}}} });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    const std::u16string* got = cs.find("TEST:Unicode");
    REQUIRE(got != nullptr);
    CHECK(*got == text);
    CHECK(got->size() == 6);
}
```

- [ ] **Step 2: Register the test and verify the build fails** — in `CMakeLists.txt`, immediately after the line `target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)`, add this line:

```cmake
target_sources(ra2assets_tests PRIVATE tests/test_csf.cpp)
```

Then build (PowerShell; if `build/` is missing or stale, first run `cmake -S . -B build -G Ninja` from `D:\ra2-android`):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
```

Expected FAILURE containing:

```
tests/test_csf.cpp:2:10: fatal error: ra2assets/csf.hpp: No such file or directory
```

and `ninja: build stopped: subcommand failed.`

- [ ] **Step 3: Write the minimal implementation** — three pieces.

Create `include/ra2assets/csf.hpp` with exactly this content:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace ra2assets {

// Reader for Westwood CSF string tables (the loose ra2.csf / ra2md.csf in
// the install root). Maps ASCII labels ("GUI:Cancel", "Name:HTNK", ...) to
// UTF-16 display strings. Decode only -- no UI (Phases 10/11 consume this).
//
// Scope decisions (Foundation Batch spec, sub-project 2):
//  * Only the FIRST value of a multi-value label is stored; the remaining
//    value records are walked (to keep the parse in sync) and discarded.
//  * A label with num_values == 0 is legal-looking on disk but has no text:
//    it is walked and NOT inserted -- find() returns nullptr for it and
//    count() excludes it. (Storing an empty string instead would be
//    indistinguishable from a genuine empty first value.)
//  * Labels are matched case-insensitively (the game resolves "gui:cancel"
//    and "GUI:Cancel" identically): keys are normalized to ASCII uppercase
//    at insert and at lookup. RE evidence in docs/re/csf.md; if the game
//    ever proves case-sensitive, drop the normalization there and here
//    together.
//  * Duplicate labels keep the FIRST occurrence.
//  * "WRTS" values carry trailing extra ASCII data; it is skipped.
class CsfStrings {
public:
    // Parses a whole CSF file image. Throws std::runtime_error with a
    // context message on any structural error (bad magic, record running
    // past the end of the buffer, ...).
    static CsfStrings load(const uint8_t* data, size_t len);

    // Case-insensitive lookup. Returns nullptr when the label is absent.
    const std::u16string* find(const std::string& label) const;

    // Number of labels stored (labels with at least one value).
    size_t count() const { return table_.size(); }

private:
    // label (uppercased) -> first value (extra/secondary values ignored for
    // this milestone)
    std::unordered_map<std::string, std::u16string> table_;
};

} // namespace ra2assets
```

Create `src/csf.cpp` with exactly this content:

```cpp
#include "ra2assets/csf.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ra2assets {

// On-disk CSF layout (little-endian, packed). Byte-verified against the
// install's ra2.csf (491,826 bytes, version 3, 4562 labels -- every label of the
// real file walks cleanly with exactly this structure); game-loader RE
// evidence in docs/re/csf.md.
//
//   file header, 24 bytes:
//     char magic[4] = " FSC"   @0   (bytes 0x20 0x46 0x53 0x43)
//     u32  version             @4   (3 in ra2.csf; not validated here)
//     u32  num_labels          @8
//     u32  num_strings         @12  (ignored)
//     u32  unused              @16  (ignored)
//     u32  language            @20  (ignored)
//   then num_labels sequential label records:
//     char magic[4] = " LBL"; u32 num_values; u32 label_len;
//     label_len bytes of ASCII label (NOT NUL-terminated)
//   each label is followed by num_values value records:
//     char magic[4] = " RTS" (or "WRTS"); u32 val_len (UTF-16 code units);
//     val_len*2 bytes UTF-16LE with every byte XOR 0xFF (== bitwise NOT of
//     each 16-bit unit); a "WRTS" record additionally carries
//     u32 extra_len + extra_len ASCII bytes (skipped).
static constexpr size_t kFileHeader  = 24;
static constexpr size_t kLabelHeader = 12;  // " LBL" + num_values + label_len
static constexpr size_t kValueHeader = 8;   // magic + val_len

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static std::string upper_ascii(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = (char)std::toupper((unsigned char)c);
    return out;
}

CsfStrings CsfStrings::load(const uint8_t* data, size_t len) {
    CsfStrings cs;
    if (len < kFileHeader)
        throw std::runtime_error("csf: file smaller than header (" +
                                 std::to_string(len) + " bytes)");
    if (!(data[0]==0x20 && data[1]==0x46 && data[2]==0x53 && data[3]==0x43))
        throw std::runtime_error("csf: bad file magic (want \" FSC\")");
    const uint32_t num_labels = r32(data + 8);
    // Capped reserve: num_labels is attacker-controlled input.
    cs.table_.reserve(std::min<uint32_t>(num_labels, 65536u));

    size_t pos = kFileHeader;                  // invariant: pos <= len
    for (uint32_t li = 0; li < num_labels; li++) {
        if (kLabelHeader > len - pos)
            throw std::runtime_error("csf: label record " + std::to_string(li) +
                " header at " + std::to_string(pos) + " exceeds file");
        if (!(data[pos]==0x20 && data[pos+1]==0x4C && data[pos+2]==0x42 && data[pos+3]==0x4C))
            throw std::runtime_error("csf: bad label magic at offset " +
                std::to_string(pos) + " (label " + std::to_string(li) + ")");
        const uint32_t num_values = r32(data + pos + 4);
        const uint32_t label_len  = r32(data + pos + 8);
        pos += kLabelHeader;
        if (label_len > len - pos)
            throw std::runtime_error("csf: label " + std::to_string(li) +
                " name (" + std::to_string(label_len) + " bytes at " +
                std::to_string(pos) + ") exceeds file");
        std::string label((const char*)data + pos, label_len);
        pos += label_len;

        for (uint32_t vi = 0; vi < num_values; vi++) {
            if (kValueHeader > len - pos)
                throw std::runtime_error("csf: value record header for label \"" +
                    label + "\" at " + std::to_string(pos) + " exceeds file");
            const uint8_t* m = data + pos;
            if (!(m[0]==0x20 && m[1]==0x52 && m[2]==0x54 && m[3]==0x53))   // " RTS"
                throw std::runtime_error("csf: bad value magic at offset " +
                    std::to_string(pos) + " (label \"" + label + "\", value " +
                    std::to_string(vi) + ")");
            const uint32_t val_len = r32(data + pos + 4);
            pos += kValueHeader;
            const size_t val_bytes = (size_t)val_len * 2;
            if (val_bytes > len - pos)
                throw std::runtime_error("csf: value " + std::to_string(vi) +
                    " of label \"" + label + "\" (" + std::to_string(val_bytes) +
                    " bytes at " + std::to_string(pos) + ") exceeds file");
            if (vi == 0) {
                // UTF-16LE with every byte XOR 0xFF (= ~ per 16-bit unit).
                std::u16string text;
                text.reserve(val_len);
                for (uint32_t u = 0; u < val_len; u++) {
                    const uint16_t lo = (uint16_t)(data[pos + 2*(size_t)u]     ^ 0xFF);
                    const uint16_t hi = (uint16_t)(data[pos + 2*(size_t)u + 1] ^ 0xFF);
                    text.push_back((char16_t)(lo | (hi << 8)));
                }
                cs.table_.emplace(upper_ascii(label), std::move(text));
            }
            pos += val_bytes;
        }
    }
    return cs;
}

const std::u16string* CsfStrings::find(const std::string& label) const {
    auto it = table_.find(upper_ascii(label));
    return it == table_.end() ? nullptr : &it->second;
}

} // namespace ra2assets
```

In `cmake/ra2_sources.cmake`, inside `RA2_LIB_SOURCES`, after the line `    ${RA2_ROOT}/src/theater.cpp` and before the closing `)`, add:

```cmake
    ${RA2_ROOT}/src/csf.cpp
```

- [ ] **Step 4: Run the tests to verify they pass** (PowerShell):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="csf*"
```

Expected: `test cases: 3 | 3 passed | 0 failed` (rest of the suite reported as skipped) and final line:

```
[doctest] Status: SUCCESS!
```

- [ ] **Step 5: Run the full suite** (PowerShell):

```powershell
cd D:\ra2-android
& .\build\ra2assets_tests.exe
```

Expected final line: `[doctest] Status: SUCCESS!` (intentional oracle-skip `MESSAGE` lines from other tests are fine; no failures).

- [ ] **Step 6: Commit**:

```powershell
cd D:\ra2-android
git add include/ra2assets/csf.hpp src/csf.cpp tests/test_csf.cpp cmake/ra2_sources.cmake CMakeLists.txt
git commit -m "feat: CSF string-table reader (labels, RTS values, case-insensitive find)"
```

---

### Task 3: WRTS records, multi-value walk, malformed-input errors

**Files:**
- Modify: `src/csf.cpp` (full replacement shown below), `tests/test_csf.cpp` (append test cases)
- Test: `tests/test_csf.cpp`

**Interfaces:** Consumes/Produces: unchanged public signatures (`CsfStrings::load/find/count` from Task 2). New behavior: `"WRTS"` value records are accepted and their `u32 extra_len` + ASCII payload is skipped, with two new bounds errors (truncated `extra_len` field; `extra_len` past EOF).

- [ ] **Step 1: Write the failing tests** — append exactly this to the END of `tests/test_csf.cpp`:

```cpp
TEST_CASE("csf synthetic: WRTS value skips extra ASCII and stays in sync") {
    // A WRTS first value carries trailing ASCII data; the reader must skip
    // it and land exactly on the next label record.
    auto bytes = make_csf({
        {"GUI:Save", {{u"Save", true, "save_extra"}}},
        {"GUI:Load", {{u"Load"}}},
    });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 2);
    const std::u16string* save = cs.find("GUI:Save");
    REQUIRE(save != nullptr);
    CHECK(*save == u"Save");
    const std::u16string* load = cs.find("GUI:Load");
    REQUIRE(load != nullptr);                  // the walk stayed in sync
    CHECK(*load == u"Load");
}

TEST_CASE("csf synthetic: multi-value label stores first value, walks the rest") {
    auto bytes = make_csf({
        {"Multi:Val",   {{u"first"}, {u"second"}}},
        {"After:Multi", {{u"still here"}}},
    });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 2);
    const std::u16string* v = cs.find("Multi:Val");
    REQUIRE(v != nullptr);
    CHECK(*v == u"first");                     // value 0 wins
    const std::u16string* after = cs.find("After:Multi");
    REQUIRE(after != nullptr);                 // second value was walked over
    CHECK(*after == u"still here");
}

TEST_CASE("csf synthetic: zero-value label is walked but not stored") {
    auto bytes = make_csf({
        {"EMPTY:NoValues", {}},                // num_values == 0
        {"GUI:Ok", {{u"OK"}}},
    });
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 1);                    // zero-value label excluded
    CHECK(cs.find("EMPTY:NoValues") == nullptr);
    const std::u16string* ok = cs.find("GUI:Ok");
    REQUIRE(ok != nullptr);
    CHECK(*ok == u"OK");
}

TEST_CASE("csf synthetic: empty-TEXT value is stored (distinct from zero values)") {
    auto bytes = make_csf({ {"Blank:Text", {{u""}}} });   // one value, val_len 0
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());
    CHECK(cs.count() == 1);
    const std::u16string* blank = cs.find("Blank:Text");
    REQUIRE(blank != nullptr);                 // present ...
    CHECK(blank->empty());                     // ... and empty
}

TEST_CASE("csf malformed input throws with context") {
    // Base image for the corruption cases: one label "AA" with one " RTS"
    // value u"X". Offsets: file header 0..23, label magic @24, num_values
    // @28, label_len @32, name "AA" @36..37, value magic @38, val_len @42,
    // payload @46..47. Total 48 bytes.
    auto base = make_csf({ {"AA", {{u"X"}}} });
    REQUIRE(base.size() == 48);

    // Too small for the 24-byte file header.
    CHECK_THROWS_AS(CsfStrings::load(base.data(), 3), std::runtime_error);

    // Wrong file magic.
    {
        auto b = base;
        b[0] = 0x21;                           // " FSC" -> "!FSC"
        CHECK_THROWS_AS(CsfStrings::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong label magic -> the message reports the file offset (24).
    {
        auto b = base;
        b[24] = 0x00;
        try {
            CsfStrings::load(b.data(), b.size());
            FAIL("expected std::runtime_error");
        } catch (const std::runtime_error& e) {
            CHECK(std::string(e.what()).find("offset 24") != std::string::npos);
        }
    }
    // Label name length runs past EOF.
    {
        auto b = base;
        b[32]=0xFF; b[33]=0xFF; b[34]=0x00; b[35]=0x00;   // label_len = 65535
        CHECK_THROWS_AS(CsfStrings::load(b.data(), b.size()), std::runtime_error);
    }
    // Wrong value magic.
    {
        auto b = base;
        b[38] = 0x00;                          // " RTS" -> "\0RTS"
        CHECK_THROWS_AS(CsfStrings::load(b.data(), b.size()), std::runtime_error);
    }
    // Value payload length runs past EOF.
    {
        auto b = base;
        b[42]=0xFF; b[43]=0xFF; b[44]=0x00; b[45]=0x00;   // val_len = 65535 units
        CHECK_THROWS_AS(CsfStrings::load(b.data(), b.size()), std::runtime_error);
    }
    // Truncated mid value header.
    CHECK_THROWS_AS(CsfStrings::load(base.data(), 40), std::runtime_error);

    // num_labels claims more records than the file holds.
    {
        auto b = base;
        b[8] = 2;                              // num_labels 1 -> 2
        CHECK_THROWS_AS(CsfStrings::load(b.data(), b.size()), std::runtime_error);
    }

    // WRTS extra_len runs past EOF -> the error mentions the extra data.
    // Layout: label "AA" @24 (12B hdr + 2B name), "WRTS" @38, val_len @42,
    // payload @46..47, extra_len @48, extra "hi" @52..53. Total 54 bytes.
    {
        auto b = make_csf({ {"AA", {{u"X", true, "hi"}}} });
        REQUIRE(b.size() == 54);
        b[48]=0xFF; b[49]=0xFF; b[50]=0x00; b[51]=0x00;   // extra_len = 65535
        try {
            CsfStrings::load(b.data(), b.size());
            FAIL("expected std::runtime_error");
        } catch (const std::runtime_error& e) {
            CHECK(std::string(e.what()).find("extra") != std::string::npos);
        }
    }
    // WRTS truncated before the extra_len field itself.
    {
        auto b = make_csf({ {"AA", {{u"X", true, "hi"}}} });
        CHECK_THROWS_AS(CsfStrings::load(b.data(), 50), std::runtime_error);
    }
}
```

- [ ] **Step 2: Run tests to verify the new behavior fails** (PowerShell):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="csf*"
```

Expected: `[doctest] Status: FAILURE!` with EXACTLY 2 failing test cases:

1. `csf synthetic: WRTS value skips extra ASCII and stays in sync` — fails with `test case THREW exception: csf: bad value magic at offset 44 (label "GUI:Save", value 0)` (Task 2's reader rejects `"WRTS"`).
2. `csf malformed input throws with context` — 1 failed `CHECK` in the "WRTS extra_len runs past EOF" block (the thrown message is `bad value magic...`, which does not contain `"extra"`).

Summary line: `test cases: 8 | 6 passed | 2 failed`. The multi-value / zero-value / empty-text / other malformed cases already pass — that is expected; they lock in Task 2's walk behavior. If anything OTHER than those 2 cases fails, stop and debug before proceeding.

- [ ] **Step 3: Write the implementation** — replace the ENTIRE contents of `src/csf.cpp` with:

```cpp
#include "ra2assets/csf.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ra2assets {

// On-disk CSF layout (little-endian, packed). Byte-verified against the
// install's ra2.csf (491,826 bytes, version 3, 4562 labels -- every label of the
// real file walks cleanly with exactly this structure); game-loader RE
// evidence in docs/re/csf.md.
//
//   file header, 24 bytes:
//     char magic[4] = " FSC"   @0   (bytes 0x20 0x46 0x53 0x43)
//     u32  version             @4   (3 in ra2.csf; not validated here)
//     u32  num_labels          @8
//     u32  num_strings         @12  (ignored)
//     u32  unused              @16  (ignored)
//     u32  language            @20  (ignored)
//   then num_labels sequential label records:
//     char magic[4] = " LBL"; u32 num_values; u32 label_len;
//     label_len bytes of ASCII label (NOT NUL-terminated)
//   each label is followed by num_values value records:
//     char magic[4] = " RTS" or "WRTS"; u32 val_len (UTF-16 code units);
//     val_len*2 bytes UTF-16LE with every byte XOR 0xFF (== bitwise NOT of
//     each 16-bit unit); a "WRTS" record additionally carries
//     u32 extra_len + extra_len ASCII bytes (out of scope -- skipped).
static constexpr size_t kFileHeader  = 24;
static constexpr size_t kLabelHeader = 12;  // " LBL" + num_values + label_len
static constexpr size_t kValueHeader = 8;   // magic + val_len

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static std::string upper_ascii(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = (char)std::toupper((unsigned char)c);
    return out;
}

CsfStrings CsfStrings::load(const uint8_t* data, size_t len) {
    CsfStrings cs;
    if (len < kFileHeader)
        throw std::runtime_error("csf: file smaller than header (" +
                                 std::to_string(len) + " bytes)");
    if (!(data[0]==0x20 && data[1]==0x46 && data[2]==0x53 && data[3]==0x43))
        throw std::runtime_error("csf: bad file magic (want \" FSC\")");
    const uint32_t num_labels = r32(data + 8);
    // Capped reserve: num_labels is attacker-controlled input.
    cs.table_.reserve(std::min<uint32_t>(num_labels, 65536u));

    size_t pos = kFileHeader;                  // invariant: pos <= len
    for (uint32_t li = 0; li < num_labels; li++) {
        if (kLabelHeader > len - pos)
            throw std::runtime_error("csf: label record " + std::to_string(li) +
                " header at " + std::to_string(pos) + " exceeds file");
        if (!(data[pos]==0x20 && data[pos+1]==0x4C && data[pos+2]==0x42 && data[pos+3]==0x4C))
            throw std::runtime_error("csf: bad label magic at offset " +
                std::to_string(pos) + " (label " + std::to_string(li) + ")");
        const uint32_t num_values = r32(data + pos + 4);
        const uint32_t label_len  = r32(data + pos + 8);
        pos += kLabelHeader;
        if (label_len > len - pos)
            throw std::runtime_error("csf: label " + std::to_string(li) +
                " name (" + std::to_string(label_len) + " bytes at " +
                std::to_string(pos) + ") exceeds file");
        std::string label((const char*)data + pos, label_len);
        pos += label_len;

        for (uint32_t vi = 0; vi < num_values; vi++) {
            if (kValueHeader > len - pos)
                throw std::runtime_error("csf: value record header for label \"" +
                    label + "\" at " + std::to_string(pos) + " exceeds file");
            const uint8_t* m = data + pos;
            bool wide = false;
            if (m[0]==0x57 && m[1]==0x52 && m[2]==0x54 && m[3]==0x53) {        // "WRTS"
                wide = true;
            } else if (!(m[0]==0x20 && m[1]==0x52 && m[2]==0x54 && m[3]==0x53)) { // " RTS"
                throw std::runtime_error("csf: bad value magic at offset " +
                    std::to_string(pos) + " (label \"" + label + "\", value " +
                    std::to_string(vi) + ")");
            }
            const uint32_t val_len = r32(data + pos + 4);
            pos += kValueHeader;
            const size_t val_bytes = (size_t)val_len * 2;
            if (val_bytes > len - pos)
                throw std::runtime_error("csf: value " + std::to_string(vi) +
                    " of label \"" + label + "\" (" + std::to_string(val_bytes) +
                    " bytes at " + std::to_string(pos) + ") exceeds file");
            if (vi == 0) {
                // UTF-16LE with every byte XOR 0xFF (= ~ per 16-bit unit).
                std::u16string text;
                text.reserve(val_len);
                for (uint32_t u = 0; u < val_len; u++) {
                    const uint16_t lo = (uint16_t)(data[pos + 2*(size_t)u]     ^ 0xFF);
                    const uint16_t hi = (uint16_t)(data[pos + 2*(size_t)u + 1] ^ 0xFF);
                    text.push_back((char16_t)(lo | (hi << 8)));
                }
                cs.table_.emplace(upper_ascii(label), std::move(text));
            }
            pos += val_bytes;

            if (wide) {
                // "WRTS" trailing extra ASCII payload: skipped (only the
                // UTF-16 text is in scope), but bounds-walked to stay in
                // sync with the next record.
                if (4 > len - pos)
                    throw std::runtime_error("csf: WRTS extra length at " +
                        std::to_string(pos) + " exceeds file (label \"" +
                        label + "\")");
                const uint32_t extra_len = r32(data + pos);
                pos += 4;
                if (extra_len > len - pos)
                    throw std::runtime_error("csf: WRTS extra data (" +
                        std::to_string(extra_len) + " bytes at " +
                        std::to_string(pos) + ") exceeds file (label \"" +
                        label + "\")");
                pos += extra_len;
            }
        }
    }
    return cs;
}

const std::u16string* CsfStrings::find(const std::string& label) const {
    auto it = table_.find(upper_ascii(label));
    return it == table_.end() ? nullptr : &it->second;
}

} // namespace ra2assets
```

- [ ] **Step 4: Run tests to verify they pass** (PowerShell):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
& .\build\ra2assets_tests.exe --test-case="csf*"
```

Expected: `test cases: 8 | 8 passed | 0 failed` and final line `[doctest] Status: SUCCESS!`.

- [ ] **Step 5: Run the full suite** (PowerShell):

```powershell
cd D:\ra2-android
& .\build\ra2assets_tests.exe
```

Expected final line: `[doctest] Status: SUCCESS!`

- [ ] **Step 6: Commit**:

```powershell
cd D:\ra2-android
git add src/csf.cpp tests/test_csf.cpp
git commit -m "feat: CSF WRTS records, multi-value walk, malformed-input errors"
```

---

### Task 4: Real-asset oracle against the loose ra2.csf

**Files:**
- Modify: `tests/test_csf.cpp` (extend includes; append one guarded test case)
- Test: `tests/test_csf.cpp`

**Interfaces:** Consumes: `CsfStrings::load/find/count` (Task 2/3) and the loose file `<RA2_INSTALL_DIR>\ra2.csf` (default `D:\Red Alert 2\ra2.csf`). Produces: nothing new — this is the real-asset correctness gate.

Note: **no MIX extraction is needed** — `ra2.csf` sits loose in the install root, so `ra2extract.exe` (and its ~20 s per-invocation `ra2.mix` re-read) is not involved at all. The test reads the file directly and skips cleanly when it is absent.

- [ ] **Step 1: Extend the test file's includes** — in `tests/test_csf.cpp`, replace this exact block at the top of the file:

```cpp
#include "doctest.h"
#include "ra2assets/csf.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ra2assets;
```

with:

```cpp
#include "doctest.h"
#include "ra2assets/csf.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ra2assets;
```

- [ ] **Step 2: Write the oracle test** — append exactly this to the END of `tests/test_csf.cpp`:

```cpp
// ---------------------------------------------------------------------------
// Guarded real-asset oracle: loads the LOOSE ra2.csf from the install root
// (no MIX extraction needed) and checks known label->text vectors decoded
// independently (Python byte-walk of the same file, 2026-07-09). Skips
// cleanly with a doctest MESSAGE when the install is absent so asset-free
// CI still builds and passes.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> slurp_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("csf real asset: ra2.csf known vectors") {
    const char* id = std::getenv("RA2_INSTALL_DIR");
    fs::path install = id ? id : "D:\\Red Alert 2";
    fs::path csf = install / "ra2.csf";
    if (!fs::exists(csf)) { MESSAGE("ra2.csf absent; skipped"); return; }

    auto bytes = slurp_file(csf);
    REQUIRE_FALSE(bytes.empty());
    CsfStrings cs = CsfStrings::load(bytes.data(), bytes.size());

    // THIS install's ra2.csf has 4562 label records = 4558 distinct labels
    // after case-insensitive normalization (4 duplicate pairs, keep-first);
    // assert loosely so other language/patch variants still pass.
    CHECK(cs.count() > 4000);

    struct Vec { const char* label; std::u16string want; };
    const Vec vecs[] = {
        {"GUI:Cancel",   u"Cancel"},
        {"Name:HTNK",    u"Rhino Heavy Tank"},
        {"Name:E1",      u"GI"},
        {"GUI:LoadGame", u"Load Game"},
    };
    int checked = 0;
    for (const auto& v : vecs) {
        const std::u16string* got = cs.find(v.label);
        REQUIRE(got != nullptr);
        CHECK(*got == v.want);
        checked++;
    }
    CHECK(checked == 4);   // guards against a silent skip when assets exist
}
```

- [ ] **Step 3: Build and run the oracle against the real install** (PowerShell):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe --test-case="csf real asset*"
```

Expected: `test cases: 1 | 1 passed | 0 failed` and final line `[doctest] Status: SUCCESS!`. If any vector mismatches or `count()` is too low, that is a REAL decoder bug — use superpowers:systematic-debugging (compare the failing label's raw record bytes against the format table in `src/csf.cpp`) before touching the test.

- [ ] **Step 4: Verify the clean-skip path** (PowerShell — point at a nonexistent install, then restore):

```powershell
cd D:\ra2-android
$env:RA2_INSTALL_DIR = "D:\definitely-not-an-install"; & .\build\ra2assets_tests.exe --test-case="csf real asset*"
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: output contains the message `ra2.csf absent; skipped`, `test cases: 1 | 1 passed | 0 failed`, final line `[doctest] Status: SUCCESS!` (a skip is a pass, never a failure).

- [ ] **Step 5: Run the full suite** (PowerShell):

```powershell
cd D:\ra2-android
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected final line: `[doctest] Status: SUCCESS!`

- [ ] **Step 6: Commit**:

```powershell
cd D:\ra2-android
git add tests/test_csf.cpp
git commit -m "test: CSF real-asset oracle against ra2.csf known vectors"
```

---

### Task 5: Finalize — RE confidence table, README mention, wiring verification

**Files:**
- Modify: `docs/re/csf.md` (replace the Confidence placeholder section), `README.md` (one-line library-description mention)

**Interfaces:** Consumes: Task 1's RE findings and Task 4's oracle result. Produces: the finished `docs/re/csf.md` (spec success-criterion 3) and the README mention.

- [ ] **Step 1: Finalize the confidence table** — in `docs/re/csf.md`, replace the section

```markdown
## Confidence

(Table finalized after the real-asset oracle runs — see the plan's Task 5.)
```

with the following, filling each `«H/M/L»` cell honestly from Task 1's actual findings (use the `docs/re/workflow.md` rubric: High = constants + control flow match 1:1; Medium = structure confirmed but leaf routine inlined/shifted; Low = not confidently located, oracle gates correctness):

```markdown
## Confidence

| Item | Confidence | Basis |
|------|:----------:|-------|
| File/label/value record layout | **High** | byte-verified: all 4562 labels of the real ra2.csf walk cleanly with exactly this structure (independent Python decode) |
| CSF loader location in gamemd.exe | **«H/M/L»** | «magic-immediate hits + decompiled walk shape / honest failed-hunt note» |
| Value bit-inversion (byte XOR 0xFF == ~ per 16-bit unit) | **«H/M/L»** | «decompiled inversion loop / format-verified only: inverting the real file's bytes yields the known English strings» |
| Label case handling (uppercase-normalized, case-insensitive find) | **«H/M/L»** | «decompiled toupper/stricmp/hash evidence / retained as default on UI-file evidence (gui:cancel vs GUI:Cancel both resolve)» |
| End-to-end decode correctness | **High** | real-asset oracle: count() > 4000 and 4/4 known vectors (GUI:Cancel, Name:HTNK, Name:E1, GUI:LoadGame) decode byte-exactly (tests/test_csf.cpp) |
```

- [ ] **Step 2: README mention** — in `README.md`, apply this exact edit to the library description (line 5):

Old string:

```
rules, `.map` cell grids, TMP theater tiles), plus `ra2engine` — an SDL2
```

New string:

```
rules, `.map` cell grids, TMP theater tiles, CSF string tables), plus `ra2engine` — an SDL2
```

- [ ] **Step 3: Verify the CMake wiring is in place** (PowerShell; both commands must print exactly one matching line each — they were added in Task 2):

```powershell
cd D:\ra2-android
Select-String -Path cmake\ra2_sources.cmake -Pattern "src/csf.cpp"
Select-String -Path CMakeLists.txt -Pattern "tests/test_csf.cpp"
```

Expected: `cmake\ra2_sources.cmake:...:    ${RA2_ROOT}/src/csf.cpp` and `CMakeLists.txt:...:target_sources(ra2assets_tests PRIVATE tests/test_csf.cpp)`. If either is missing, add it exactly as specified in Task 2 Step 2 / Step 3 before proceeding.

- [ ] **Step 4: Full suite green** (PowerShell):

```powershell
cd D:\ra2-android
cmake --build build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"; & .\build\ra2assets_tests.exe
```

Expected final line: `[doctest] Status: SUCCESS!` — with the CSF cases contributing 9 test cases (8 synthetic + 1 real-asset oracle), and pristine output (only intentional skip `MESSAGE`s from oracle tests whose asset sets are absent).

- [ ] **Step 5: Commit**:

```powershell
cd D:\ra2-android
git add docs/re/csf.md README.md
git commit -m "docs: finalize CSF RE confidence table + README mention"
```
