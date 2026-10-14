# ra2assets Asset Pipeline (MIX→PAL→SHP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `ra2assets`, a portable C++ library (plus an `ra2extract` CLI) that turns an encrypted Red Alert 2 / Yuri's Revenge `.mix` archive into decoded, palettized RGBA bitmaps of real in-game sprites — and prove a repeatable `decompile → reimplement → verify` Ghidra workflow against it.

**Architecture:** Three focused, independently testable units — `mix` (Blowfish-decrypting archive reader), `pal` (6-bit palette loader), `shp` (TS/RA2 sprite decoder) — behind clean C++17 interfaces with no DirectX/OS dependencies. A CLI and a golden-file oracle harness sit on top. The same source tree cross-compiles for host x86-64 and Android `arm64-v8a`. Correctness is gated primarily by **byte-for-byte equality of indexed pixels against an oracle** (XCC Mixer), which is palette-independent and therefore unambiguous.

**Tech Stack:** C++17, CMake + Ninja, MinGW-w64 gcc (host) / Android NDK clang (`arm64-v8a`), doctest (header-only tests), stb_image_write (header-only PNG), Ghidra + JDK for RE. In-house Blowfish (no crypto library).

---

## Global Constraints

Every task's requirements implicitly include this section. Values are copied verbatim from the spec and the preflight investigation.

- **Language:** C++17, no compiler-specific extensions. The library (`mix`/`pal`/`shp`) must contain **no** DirectX, Win32, or any OS-specific calls — pure portable C++ reading from `std::istream` / byte spans.
- **Third-party dependencies limited to:** `stb_image_write.h` (single-header PNG writer) and `doctest.h` (single-header test framework), both vendored into the repo. **Blowfish is reimplemented in-house** — no OpenSSL/libtomcrypt/etc.
- **Two build targets from one source tree:** host `x86-64` (dev/test/oracle) and Android `arm64-v8a` via the NDK toolchain file.
- **No game assets and no game binary are ever committed.** The user supplies their own legally-owned copy. All asset paths resolve through the `RA2_INSTALL_DIR` environment variable (default `D:\Red Alert 2`). Oracle golden files derived from those assets are also never committed (they are derivative works) — they live in a git-ignored directory.
- **Target binary pin:** `gamemd.exe` must be SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7` (Yuri's Revenge 1.001, 4,808,976 bytes). RE work assumes this exact build so YRpp/Ares addresses match.
- **Code repo location:** `D:\ra2-android\` — a **separate directory from the game install**, git-initialized there. The game install is referenced externally, never copied in.
- **Methodology split:** data-format parsers in this sub-project are **functional reimplementations verified by output equality** (not byte-matching decompilation). Matching-grade fidelity is a later-phase (simulation) concern.
- **Palette scaling rule (this project's canonical default):** 6-bit channel `v` (0–63) → 8-bit via bit-replication `out8 = (v << 2) | (v >> 4)` (so 63→255). See Task 5 for the oracle-calibration procedure that confirms or adjusts this single constant.

---

## File Structure

Code repo root is `D:\ra2-android\`. Layout:

```
D:\ra2-android\
├── CMakeLists.txt                  # top-level build; host + defines options
├── .gitignore                      # blocks assets, oracle/, build/
├── README.md                       # build + oracle-generation instructions
├── include\ra2assets\
│   ├── blowfish.hpp                # Blowfish ECB cipher (Task 1)
│   ├── mix.hpp                     # MixFile reader (Tasks 2–4)
│   ├── pal.hpp                     # Palette (Task 6)
│   └── shp.hpp                     # ShpFile decoder (Task 7)
├── src\
│   ├── blowfish.cpp
│   ├── mix_keyderiv.cpp            # Westwood 80-byte-block → Blowfish key
│   ├── mix.cpp
│   ├── pal.cpp
│   └── shp.cpp
├── tools\
│   └── ra2extract.cpp              # CLI (Task 8)
├── tests\
│   ├── test_main.cpp               # doctest entry
│   ├── test_blowfish.cpp
│   ├── test_mix.cpp
│   ├── test_pal.cpp
│   ├── test_shp.cpp
│   └── test_oracle.cpp             # golden-file comparison (Task 9)
├── third_party\
│   ├── doctest.h
│   └── stb_image_write.h
├── scripts\
│   ├── preflight.py                # asserts gamemd.exe hash + install layout
│   ├── make_oracle.md              # documented manual oracle-gen steps
│   └── oracle_to_raw.py            # indexed BMP/PCX/PNG → headerless .raw (Task 9)
└── docs\
    ├── 2026-07-08-ra2-android-asset-pipeline-design.md   # spec copy (Task 0)
    ├── 2026-07-08-ra2assets-asset-pipeline.md            # plan copy (Task 0)
    └── re\                         # Ghidra methodology output (Task 11)
        ├── workflow.md
        ├── mix.md
        ├── pal.md
        └── shp.md
```

**Ghidra project** (containing the binary) lives at `D:\ra2-re\` — **outside** the code repo, never committed.

Task ordering builds the dependency chain bottom-up: cipher → key derivation → archive → palette → sprite → CLI → oracle → Android → RE docs.

---

## Prerequisites (Task 0 verifies/installs these)

- **Ghidra 11.2.1** — installed at `C:\work\DentrixDocumentsExtractor\ghidra_11.2.1_PUBLIC\` (headless at `support\analyzeHeadless.bat`). Not on PATH; invoke by full path or set `GHIDRA_HOME` to that directory. **JDK 21** is present at `C:\Program Files\Java\jdk-21` (JDK 11 also installed at `C:\Program Files\Java\jdk-11`); Ghidra 11.2.1 requires JDK 21, so ensure `JAVA_HOME` points at jdk-21 when launching headless. Only needed for Task 11.
- **Android NDK** (r25+); not currently installed. Needed only for Task 10.
- **Oracle: XCC Mixer** (Windows GUI, community-standard). Used to export ground-truth indexed frames and file lists. Not currently installed.
- **Reference sources (read-only, for cross-checking algorithms):** XCC Utilities source and/or OpenRA source. Clone locally under `D:\ra2-re\reference\`; do not vendor into the repo.
- Present already: git, CMake, MinGW-w64 gcc, ninja, Python 3.13.

---

## Task 0: Repo scaffold, preflight, and empty build

**Files:**
- Create: `D:\ra2-android\CMakeLists.txt`
- Create: `D:\ra2-android\.gitignore`
- Create: `D:\ra2-android\README.md`
- Create: `D:\ra2-android\tests\test_main.cpp`
- Create: `D:\ra2-android\third_party\doctest.h` (download pinned release)
- Create: `D:\ra2-android\scripts\preflight.py`

**Interfaces:**
- Produces: a buildable CMake project with a `ra2assets` static library target (initially empty) and a `ra2assets_tests` executable using doctest; a `preflight.py` that exits non-zero unless the game install and binary hash are correct.

- [ ] **Step 1: Create the repo directory and initialize git (outside the game install)**

```bash
mkdir -p /d/ra2-android
cd /d/ra2-android
git init
# Version the spec + plan with the code (they contain no game assets)
mkdir -p docs
cp "/d/Red Alert 2/docs/superpowers/specs/2026-07-08-ra2-android-asset-pipeline-design.md" docs/
cp "/d/Red Alert 2/docs/superpowers/plans/2026-07-08-ra2assets-asset-pipeline.md" docs/
```

- [ ] **Step 2: Write `.gitignore`**

```gitignore
# Build output
/build/
# Oracle golden files derived from EA-owned assets — never commit
/oracle/
# Any accidental asset copies
*.mix
*.shp
*.pal
*.exe
# Ghidra project lives outside the repo, but guard anyway
*.gpr
*.rep/
```

- [ ] **Step 3: Vendor doctest**

Download the pinned single header to `third_party/doctest.h`:

```bash
curl -L -o /d/ra2-android/third_party/doctest.h \
  https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
```

Expected: file ~250 KB, contains `#define DOCTEST_VERSION_STR "2.4.11"`.

- [ ] **Step 4: Write the doctest entry point** (`tests/test_main.cpp`)

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("harness sanity") {
    CHECK(1 + 1 == 2);
}
```

- [ ] **Step 5: Write the top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(ra2assets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library sources are added by later tasks; start empty via a stub object.
add_library(ra2assets STATIC)
target_include_directories(ra2assets PUBLIC include)
# Keeps the target valid before any .cpp exists:
target_sources(ra2assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/_placeholder.cpp)

enable_testing()
add_executable(ra2assets_tests
    tests/test_main.cpp
)
target_include_directories(ra2assets_tests PRIVATE third_party include)
target_link_libraries(ra2assets_tests PRIVATE ra2assets)
add_test(NAME ra2assets_tests COMMAND ra2assets_tests)
```

Create `src/_placeholder.cpp` with a single line so the static lib links:

```cpp
namespace ra2assets { int _placeholder = 0; }
```

- [ ] **Step 6: Write `scripts/preflight.py`**

```python
#!/usr/bin/env python3
"""Verify the game install and binary before RE/oracle work."""
import hashlib, os, sys

EXPECTED_SHA1 = "7be4e5508655f8af482ab3ff5965d196e1b6b6c7"
install = os.environ.get("RA2_INSTALL_DIR", r"D:\Red Alert 2")

def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    exe = os.path.join(install, "gamemd.exe")
    if not os.path.isfile(exe):
        print(f"FAIL: gamemd.exe not found under {install}", file=sys.stderr)
        return 1
    got = sha1(exe)
    if got != EXPECTED_SHA1:
        print(f"FAIL: gamemd.exe SHA1 {got} != expected {EXPECTED_SHA1} (need YR 1.001)", file=sys.stderr)
        return 1
    for mix in ("ra2.mix", "ra2md.mix", "language.mix", "ecache01.mix"):
        if not os.path.isfile(os.path.join(install, mix)):
            print(f"WARN: expected MIX {mix} missing", file=sys.stderr)
    print(f"OK: install={install}, gamemd.exe verified (YR 1.001)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 7: Configure, build, and run the empty test suite**

```bash
cd /d/ra2-android
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds; ctest reports `1 test passed` (`harness sanity`).

- [ ] **Step 8: Run preflight**

```bash
RA2_INSTALL_DIR="D:\Red Alert 2" python scripts/preflight.py
```

Expected: `OK: install=..., gamemd.exe verified (YR 1.001)`.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "chore: scaffold ra2assets repo, doctest harness, preflight check"
```

---

## Task 1: Blowfish ECB cipher (in-house)

**Files:**
- Create: `include/ra2assets/blowfish.hpp`
- Create: `src/blowfish.cpp`
- Create: `tests/test_blowfish.cpp`
- Modify: `CMakeLists.txt` (add `src/blowfish.cpp` to lib, `tests/test_blowfish.cpp` to tests)

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    class Blowfish {
    public:
        void set_key(const uint8_t* key, size_t len);      // len 1..56
        void decrypt_block(const uint8_t in[8], uint8_t out[8]) const;
        void encrypt_block(const uint8_t in[8], uint8_t out[8]) const;
        // decrypts n*8 bytes in place (ECB); n = len/8, trailing <8 bytes untouched
        void decrypt(uint8_t* data, size_t len) const;
    };
  }
  ```
- Consumes: nothing.

- [ ] **Step 1: Write the failing test** (`tests/test_blowfish.cpp`)

Use Eric Young's published Blowfish ECB known-answer vectors (standard, public):

```cpp
#include "doctest.h"
#include "ra2assets/blowfish.hpp"
#include <cstdint>
#include <cstring>

using ra2assets::Blowfish;

static void hex_to_bytes(const char* hex, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned b; sscanf(hex + i * 2, "%2x", &b); out[i] = (uint8_t)b;
    }
}

TEST_CASE("blowfish known-answer vectors (Eric Young)") {
    struct KAT { const char* key; const char* clear; const char* cipher; };
    KAT kats[] = {
        {"0000000000000000", "0000000000000000", "4EF997456198DD78"},
        {"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"},
        {"3000000000000000", "1000000000000001", "7D856F9A613063F2"},
        {"1111111111111111", "1111111111111111", "2466DD878B963C9D"},
        {"FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D"},
    };
    for (auto& k : kats) {
        uint8_t key[8], clr[8], exp[8], out[8];
        hex_to_bytes(k.key, key, 8);
        hex_to_bytes(k.clear, clr, 8);
        hex_to_bytes(k.cipher, exp, 8);
        Blowfish bf; bf.set_key(key, 8);
        bf.encrypt_block(clr, out);
        CHECK(std::memcmp(out, exp, 8) == 0);
        uint8_t back[8];
        bf.decrypt_block(exp, back);
        CHECK(std::memcmp(back, clr, 8) == 0);
    }
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build && ./build/ra2assets_tests -tc="blowfish*"
```

Expected: FAIL to compile — `ra2assets/blowfish.hpp` not found.

- [ ] **Step 3: Write the header** (`include/ra2assets/blowfish.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace ra2assets {
class Blowfish {
public:
    void set_key(const uint8_t* key, size_t len);
    void encrypt_block(const uint8_t in[8], uint8_t out[8]) const;
    void decrypt_block(const uint8_t in[8], uint8_t out[8]) const;
    void decrypt(uint8_t* data, size_t len) const;
private:
    uint32_t P_[18];
    uint32_t S_[4][256];
    uint32_t f(uint32_t x) const;
};
} // namespace ra2assets
```

- [ ] **Step 4: Write the implementation** (`src/blowfish.cpp`)

Standard Blowfish. The P-array and S-box initial values are the digits of pi — reproduce them from the public Blowfish spec (Schneier). The file is large but mechanical.

```cpp
#include "ra2assets/blowfish.hpp"
#include "blowfish_tables.inc"   // P_INIT[18], S_INIT[4][256] — pi digits, public constants

namespace ra2assets {

uint32_t Blowfish::f(uint32_t x) const {
    uint32_t a = S_[0][(x >> 24) & 0xff];
    uint32_t b = S_[1][(x >> 16) & 0xff];
    uint32_t c = S_[2][(x >> 8) & 0xff];
    uint32_t d = S_[3][x & 0xff];
    return ((a + b) ^ c) + d;
}

void Blowfish::set_key(const uint8_t* key, size_t len) {
    for (int i = 0; i < 18; i++) P_[i] = P_INIT[i];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 256; j++) S_[i][j] = S_INIT[i][j];

    size_t k = 0;
    for (int i = 0; i < 18; i++) {
        uint32_t d = 0;
        for (int j = 0; j < 4; j++) { d = (d << 8) | key[k]; k = (k + 1) % len; }
        P_[i] ^= d;
    }
    uint32_t l = 0, r = 0;
    auto enc = [&](uint32_t& L, uint32_t& R) {
        for (int i = 0; i < 16; i += 2) {
            L ^= P_[i];   R ^= f(L);
            R ^= P_[i+1]; L ^= f(R);
        }
        uint32_t t = L; L = R ^ P_[17]; R = t ^ P_[16];
    };
    for (int i = 0; i < 18; i += 2) { enc(l, r); P_[i] = l; P_[i+1] = r; }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 256; j += 2) { enc(l, r); S_[i][j] = l; S_[i][j+1] = r; }
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
static void wr32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

void Blowfish::encrypt_block(const uint8_t in[8], uint8_t out[8]) const {
    uint32_t L = be32(in), R = be32(in + 4);
    for (int i = 0; i < 16; i += 2) {
        L ^= P_[i];   R ^= f(L);
        R ^= P_[i+1]; L ^= f(R);
    }
    uint32_t t = L; L = R ^ P_[17]; R = t ^ P_[16];
    wr32(out, L); wr32(out + 4, R);
}

void Blowfish::decrypt_block(const uint8_t in[8], uint8_t out[8]) const {
    uint32_t L = be32(in), R = be32(in + 4);
    for (int i = 16; i > 0; i -= 2) {
        L ^= P_[i+1]; R ^= f(L);
        R ^= P_[i];   L ^= f(R);
    }
    uint32_t t = L; L = R ^ P_[0]; R = t ^ P_[1];
    wr32(out, L); wr32(out + 4, R);
}

void Blowfish::decrypt(uint8_t* data, size_t len) const {
    for (size_t o = 0; o + 8 <= len; o += 8) decrypt_block(data + o, data + o);
}

} // namespace ra2assets
```

Create `src/blowfish_tables.inc` by pasting the published Blowfish initialization constants **verbatim** and committing the file. They are the fractional hexadecimal digits of pi and are identical across every correct implementation — copy them from a canonical source (Schneier's reference release or Paul Kocher's public-domain C implementation); do not hand-type or regenerate them. Format:

```c
static const uint32_t P_INIT[18] = {
    0x243F6A88u, 0x85A308D3u, 0x13198A2Eu, /* ... 18 values total ... */
};
static const uint32_t S_INIT[4][256] = {
    { 0xD1310BA6u, 0x98DFB5ACu, /* ... 256 values ... */ },
    /* ... 4 boxes total ... */
};
```

> Correctness gate: the KAT test in Step 1. If any pasted digit is wrong, the known-answer vectors fail.

- [ ] **Step 5: Wire into CMake**

Add to `CMakeLists.txt`:
```cmake
target_sources(ra2assets PRIVATE src/blowfish.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_blowfish.cpp)
```

- [ ] **Step 6: Build and run — verify pass**

```bash
cmake --build build && ./build/ra2assets_tests -tc="blowfish*"
```

Expected: PASS, all 5 KAT vectors encrypt and round-trip correctly.

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(blowfish): in-house Blowfish ECB verified against KAT vectors"
```

---

## Task 2: Westwood Blowfish key derivation

The encrypted MIX header is preceded by an 80-byte block. Westwood derives the 56-byte Blowfish key from it via a fixed 1024-bit public-key decryption (`get_blowfish_key`). This is the one piece with no simple public KAT; it is validated functionally in Task 4 (a real MIX header decrypts to a sane count/size). Here we port the algorithm and unit-test its deterministic sub-steps.

**Files:**
- Create: `src/mix_keyderiv.cpp`
- Modify: `include/ra2assets/mix.hpp` (add declaration — created here)
- Create: `tests/test_mix.cpp` (key-derivation section)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets::detail {
    // Derives up to 56 key bytes from the 80-byte MIX key source block.
    // Returns number of key bytes produced (56 for standard WW MIX).
    size_t westwood_blowfish_key(const uint8_t src[80], uint8_t out_key[56]);
  }
  ```
- Consumes: nothing (pure arithmetic).

- [ ] **Step 1: Write the header stub** (`include/ra2assets/mix.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
namespace ra2assets::detail {
    size_t westwood_blowfish_key(const uint8_t src[80], uint8_t out_key[56]);
}
```

- [ ] **Step 2: Write the failing test** (append to `tests/test_mix.cpp`)

Determinism + shape test (no external KAT available, so we assert stable, non-degenerate output; real validation is Task 4):

```cpp
#include "doctest.h"
#include "ra2assets/mix.hpp"
#include <cstdint>

TEST_CASE("westwood key derivation is deterministic and full-length") {
    uint8_t src[80];
    for (int i = 0; i < 80; i++) src[i] = (uint8_t)(i * 7 + 3);
    uint8_t k1[56] = {0}, k2[56] = {0};
    size_t n1 = ra2assets::detail::westwood_blowfish_key(src, k1);
    size_t n2 = ra2assets::detail::westwood_blowfish_key(src, k2);
    CHECK(n1 == 56);
    CHECK(n2 == 56);
    for (int i = 0; i < 56; i++) CHECK(k1[i] == k2[i]);   // deterministic
    bool all_zero = true;
    for (int i = 0; i < 56; i++) if (k1[i]) all_zero = false;
    CHECK_FALSE(all_zero);                                 // non-degenerate
}
```

- [ ] **Step 3: Run to verify it fails**

```bash
cmake --build build && ./build/ra2assets_tests -tc="westwood*"
```

Expected: FAIL — `westwood_blowfish_key` undefined.

- [ ] **Step 4: Port the algorithm** (`src/mix_keyderiv.cpp`)

Reimplement Westwood's `get_blowfish_key` from the XCC/OpenRA reference. It performs a big-integer (little-endian) modular exponentiation against a fixed public key, then unpacks 56 key bytes. Port the reference big-int helpers (`init_bignum`, `len_bignum`, `move_key_to_big`, `calc_a_bignum`, `dec_bignum`) faithfully.

```cpp
#include "ra2assets/mix.hpp"
#include <cstring>
#include <cstdint>

// Port of Westwood's public-key blob decryptor (a.k.a. get_blowfish_key).
// Reference: XCC Utilities `mix_file.cpp` / `blowfish_key.cpp`; OpenRA `Blowfish`.
// The fixed public key string below is a published constant used by every
// RA2/TS MIX reader.
namespace ra2assets::detail {

static const char* PUBKEY =
    "AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V";

// --- big-num helpers (little-endian uint32 limbs), ported verbatim from
// --- the reference implementation. Kept internal to this TU. ---
// [Port init_bignum, significance, mov_bignum, shr_bignum, shl_bignum,
//  sub_bignum, cmp_bignum, inv_bignum, inc_bignum, dec_bignum, neg_bignum,
//  clear_tempvars, mul_bignum, not_bignum, mod_bignum, calc_a_bignum,
//  and the base64-ish key decode from the reference. These are mechanical;
//  reproduce them exactly. See docs/re/mix.md for the annotated listing.]

size_t westwood_blowfish_key(const uint8_t src[80], uint8_t out_key[56]) {
    // 1. init_two_dw / init pubkey from PUBKEY
    // 2. for each 40-byte input group, calc_a_bignum (modexp) → 39/40 out bytes
    // 3. concatenate to 56 output bytes
    // Full body per reference; returns 56.
    // (Implementation omitted here for brevity ONLY because it is a verbatim
    //  port — the executing agent MUST paste the complete reference port,
    //  not a stub. The Task 4 functional test is the correctness gate.)
    (void)src; (void)out_key; (void)PUBKEY;
    return 56;
}

} // namespace ra2assets::detail
```

> IMPORTANT for the executing agent: Step 4 must contain the **complete** verbatim port of the reference big-int routines and `get_blowfish_key`. Do not ship the `(void)`-stub above — it exists only to show the interface. Obtain the reference from a local clone of XCC Utilities or OpenRA under `D:\ra2-re\reference\` (see Prerequisites). Record the source file and commit hash in `docs/re/mix.md`.

- [ ] **Step 5: Wire into CMake and build**

```cmake
target_sources(ra2assets PRIVATE src/mix_keyderiv.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_mix.cpp)
```
```bash
cmake --build build && ./build/ra2assets_tests -tc="westwood*"
```

Expected: PASS (deterministic, non-degenerate). Full correctness is proven in Task 4.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(mix): port Westwood public-key Blowfish-key derivation"
```

---

## Task 3: MIX header/index parsing (unencrypted path first)

**Files:**
- Modify: `include/ra2assets/mix.hpp` (add `MixFile` class)
- Create: `src/mix.cpp`
- Modify: `tests/test_mix.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    struct MixEntry { uint32_t id; uint32_t offset; uint32_t size; };
    class MixFile {
    public:
        // Opens from an owned byte buffer (whole file in memory).
        static MixFile open(std::vector<uint8_t> bytes);
        bool encrypted() const;
        bool checksummed() const;
        size_t count() const;
        const std::vector<MixEntry>& entries() const;
        const MixEntry* find(uint32_t id) const;          // nullptr if absent
        std::vector<uint8_t> read(const MixEntry& e) const;
    };
    // Filename → entry id (TS/RA2 scheme). Defined in Task 5.
    uint32_t mix_id(const std::string& name);
  }
  ```
- Consumes: `Blowfish` (Task 1), `westwood_blowfish_key` (Task 2) — used in Task 4.

- [ ] **Step 1: Write the failing test** with a synthetic unencrypted MIX

Build a minimal new-format, unencrypted MIX in memory: flags dword `0`, then `count=2`, `bodysize`, two 12-byte entries, then body.

```cpp
TEST_CASE("mix parses unencrypted new-format header and reads entries") {
    using namespace ra2assets;
    std::vector<uint8_t> buf;
    auto put32 = [&](uint32_t v){ for(int i=0;i<4;i++) buf.push_back(v>>(8*i)); };
    auto put16 = [&](uint16_t v){ buf.push_back(v&0xff); buf.push_back(v>>8); };
    // flags = 0 (new format, not encrypted / not checksummed)
    put32(0);
    // header: count, body size
    const char* A = "hello"; const char* B = "world!!";
    uint32_t offA = 0, sizeA = 5, offB = 5, sizeB = 7;
    put16(2); put32(sizeA + sizeB);
    // index entries (order by id not required for this test)
    put32(1234); put32(offA); put32(sizeA);
    put32(5678); put32(offB); put32(sizeB);
    // body
    for (const char* p = A; *p; ++p) buf.push_back((uint8_t)*p);
    for (const char* p = B; *p; ++p) buf.push_back((uint8_t)*p);

    MixFile m = MixFile::open(std::move(buf));
    CHECK_FALSE(m.encrypted());
    CHECK(m.count() == 2);
    const MixEntry* e = m.find(5678);
    REQUIRE(e != nullptr);
    auto data = m.read(*e);
    REQUIRE(data.size() == 7);
    CHECK(std::string(data.begin(), data.end()) == "world!!");
}
```

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build && ./build/ra2assets_tests -tc="mix parses unencrypted*"
```

Expected: FAIL — `MixFile` undefined.

- [ ] **Step 3: Add `MixFile` to the header**

```cpp
#include <vector>
#include <string>
namespace ra2assets {
struct MixEntry { uint32_t id; uint32_t offset; uint32_t size; };
class MixFile {
public:
    static MixFile open(std::vector<uint8_t> bytes);
    bool encrypted() const { return encrypted_; }
    bool checksummed() const { return checksummed_; }
    size_t count() const { return entries_.size(); }
    const std::vector<MixEntry>& entries() const { return entries_; }
    const MixEntry* find(uint32_t id) const;
    std::vector<uint8_t> read(const MixEntry& e) const;
private:
    std::vector<uint8_t> bytes_;
    std::vector<MixEntry> entries_;
    size_t body_start_ = 0;
    bool encrypted_ = false, checksummed_ = false;
};
uint32_t mix_id(const std::string& name);
}
```

- [ ] **Step 4: Implement the unencrypted path** (`src/mix.cpp`)

```cpp
#include "ra2assets/mix.hpp"
#include <stdexcept>
#include <cstring>

namespace ra2assets {

static uint32_t rd32(const uint8_t* p){ return p[0]|(p[1]<<8)|(p[2]<<16)|(uint32_t(p[3])<<24); }
static uint16_t rd16(const uint8_t* p){ return p[0]|(p[1]<<8); }

MixFile MixFile::open(std::vector<uint8_t> bytes) {
    MixFile m;
    m.bytes_ = std::move(bytes);
    const uint8_t* p = m.bytes_.data();
    size_t n = m.bytes_.size();
    if (n < 4) throw std::runtime_error("mix: too small");

    uint32_t first = rd32(p);
    size_t pos;
    if ((first & 0x0000FFFF) == 0) {          // new format: flags dword
        uint32_t flags = first;
        m.encrypted_   = (flags & 0x00020000) != 0;
        m.checksummed_ = (flags & 0x00010000) != 0;
        pos = 4;
        if (m.encrypted_) {
            // Encrypted header handled in Task 4.
            throw std::runtime_error("mix: encrypted header (see Task 4)");
        }
    } else {                                   // old TD/RA1 format: starts with count
        m.encrypted_ = false; m.checksummed_ = false;
        pos = 0;
    }
    uint16_t count = rd16(p + pos); pos += 2;
    /* body size */ (void)rd32(p + pos); pos += 4;
    m.entries_.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
        MixEntry e{ rd32(p+pos), rd32(p+pos+4), rd32(p+pos+8) };
        pos += 12;
        m.entries_.push_back(e);
    }
    m.body_start_ = pos;
    return m;
}

const MixEntry* MixFile::find(uint32_t id) const {
    for (auto& e : entries_) if (e.id == id) return &e;
    return nullptr;
}

std::vector<uint8_t> MixFile::read(const MixEntry& e) const {
    size_t start = body_start_ + e.offset;
    if (start + e.size > bytes_.size()) throw std::runtime_error("mix: entry out of range");
    return std::vector<uint8_t>(bytes_.begin()+start, bytes_.begin()+start+e.size);
}

} // namespace ra2assets
```

Also add `src/mix.cpp` to the lib in CMake.

- [ ] **Step 5: Build and run — verify pass**

```bash
cmake --build build && ./build/ra2assets_tests -tc="mix parses unencrypted*"
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(mix): new/old-format header + index parsing (unencrypted)"
```

---

## Task 4: Encrypted MIX header + real-archive integration

**Files:**
- Modify: `src/mix.cpp` (encrypted-header branch)
- Modify: `tests/test_mix.cpp` (real-archive test, guarded by `RA2_INSTALL_DIR`)

**Interfaces:**
- Consumes: `Blowfish` (Task 1), `westwood_blowfish_key` (Task 2), `MixFile` (Task 3).
- Produces: fully-working `MixFile::open` on encrypted, real archives.

- [ ] **Step 1: Write the failing real-archive test** (guarded — skips if assets absent)

```cpp
#include <cstdlib>
#include <fstream>
static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
TEST_CASE("mix opens a real encrypted archive from the install") {
    using namespace ra2assets;
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto bytes = slurp(base + "\\ra2md.mix");
    if (bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile m = MixFile::open(std::move(bytes));
    CHECK(m.count() > 0);
    // ra2md.mix is a top-level container of nested mixes; entry count is stable.
    MESSAGE("ra2md.mix entries: " << m.count());
}
```

- [ ] **Step 2: Run to verify it fails**

```bash
cmake --build build && RA2_INSTALL_DIR="D:\Red Alert 2" ./build/ra2assets_tests -tc="mix opens a real*"
```

Expected: FAIL — throws `mix: encrypted header (see Task 4)`.

- [ ] **Step 3: Implement the encrypted-header branch** in `src/mix.cpp`

Replace the `throw` with:

```cpp
        if (m.encrypted_) {
            // Layout after flags: 80-byte key source, then Blowfish-encrypted
            // header+index in 8-byte blocks.
            if (n < 4 + 80 + 8) throw std::runtime_error("mix: truncated encrypted header");
            const uint8_t* ks = p + pos;                 // pos == 4
            uint8_t key[56];
            detail::westwood_blowfish_key(ks, key);
            Blowfish bf; bf.set_key(key, 56);
            pos += 80;

            // Decrypt first block (8 bytes) to read count+bodysize (6 bytes).
            uint8_t hdr[8];
            bf.decrypt_block(p + pos, hdr);
            uint16_t count = rd16(hdr);
            // header is 6 bytes; index is count*12 bytes; total rounded up to /8.
            size_t index_bytes = size_t(count) * 12;
            size_t total = 6 + index_bytes;
            size_t blocks = (total + 7) / 8;
            // A wrong Blowfish key (a botched Task 2 port) yields a garbage
            // count. Fail loudly and diagnosably instead of reading out of
            // bounds — this is the primary tripwire for the key-derivation port.
            if (pos + blocks * 8 > n)
                throw std::runtime_error("mix: encrypted index exceeds file (wrong key derivation?)");
            std::vector<uint8_t> dec(blocks * 8);
            for (size_t b = 0; b < blocks; b++)
                bf.decrypt_block(p + pos + b*8, dec.data() + b*8);
            // body starts after the encrypted region (block-aligned)
            m.body_start_ = pos + blocks * 8;
            const uint8_t* d = dec.data();
            /* count */ (void)rd16(d);
            /* bodysize */ (void)rd32(d + 2);
            size_t ip = 6;
            m.entries_.reserve(count);
            for (uint16_t i = 0; i < count; i++) {
                MixEntry e{ rd32(d+ip), rd32(d+ip+4), rd32(d+ip+8) };
                ip += 12;
                m.entries_.push_back(e);
            }
            return m;
        }
```

Add `#include "ra2assets/blowfish.hpp"` and ensure `detail::westwood_blowfish_key` is declared (it is, via `mix.hpp`).

- [ ] **Step 4: Build and run — verify pass against the real archive**

```bash
cmake --build build && RA2_INSTALL_DIR="D:\Red Alert 2" ./build/ra2assets_tests -tc="mix opens a real*"
```

Expected: PASS; message prints a plausible entry count (a handful of nested mixes for `ra2md.mix`).

- [ ] **Step 5: Cross-check against the oracle**

Open `ra2md.mix` in XCC Mixer, note its entry list, and confirm `ra2assets` reports the same count and IDs. Record the comparison in `docs/re/mix.md`.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(mix): decrypt encrypted headers; opens real YR archives"
```

---

## Task 5: Filename → entry-ID hashing + filename resolution

The index stores IDs, not names. `mix_id` reproduces the TS/RA2 hashing scheme so callers can look up assets by filename. This is the calibration centerpiece: correctness is proven by matching IDs the game itself uses.

**Files:**
- Modify: `src/mix.cpp` (`mix_id`, `find(name)`, local-mix-database, nested MIX)
- Modify: `include/ra2assets/mix.hpp`
- Modify: `tests/test_mix.cpp`

**Interfaces:**
- Produces:
  ```cpp
  uint32_t mix_id(const std::string& name);          // TS/RA2 scheme
  const MixEntry* MixFile::find(const std::string& name) const;
  // Local mix database: names listed inside "local mix database.dat"
  std::vector<std::string> MixFile::local_names() const;
  // Nested: extract a child MIX by name and open it.
  MixFile MixFile::open_nested(const std::string& name) const;
  ```

- [ ] **Step 1: Write the failing tests** — nested-name resolution is the primary gate

The primary, stock-asset gate: the names of the nested mixes inside `ra2.mix` / `ra2md.mix` are well known (`cache.mix`, `local.mix`, `conquer.mix`; `cachemd.mix`, `localmd.mix`, `conqmd.mix`) and present in every install. If `mix_id` is correct, each name must resolve to an entry. This gate **cannot silently skip** as long as either stock archive exists.

```cpp
TEST_CASE("mix_id resolves well-known nested mix names (stock assets)") {
    using namespace ra2assets;
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    int archives_checked = 0;
    {
        auto bytes = slurp(base + "\\ra2.mix");
        if (!bytes.empty()) {
            MixFile m = MixFile::open(std::move(bytes));
            CHECK(m.find(mix_id("cache.mix"))   != nullptr);
            CHECK(m.find(mix_id("local.mix"))   != nullptr);
            CHECK(m.find(mix_id("conquer.mix")) != nullptr);
            archives_checked++;
        }
    }
    {
        auto bytes = slurp(base + "\\ra2md.mix");
        if (!bytes.empty()) {
            MixFile m = MixFile::open(std::move(bytes));
            CHECK(m.find(mix_id("cachemd.mix")) != nullptr);
            CHECK(m.find(mix_id("localmd.mix")) != nullptr);
            CHECK(m.find(mix_id("conqmd.mix"))  != nullptr);
            archives_checked++;
        }
    }
    CHECK(archives_checked > 0);   // at least one stock archive must be exercised
}
```

Secondary check via a local mix database, if one exists. **Stock Westwood mixes do not ship `local mix database.dat`** — it is an XCC-tooling artifact — so this test targets `ecache01.mix` (XCC-created expansion mixes usually embed one) and skips gracefully otherwise:

```cpp
TEST_CASE("mix_id matches real IDs via local mix database (if present)") {
    using namespace ra2assets;
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto bytes = slurp(base + "\\ecache01.mix");
    if (bytes.empty()) { MESSAGE("skipped: ecache01.mix not found"); return; }
    MixFile m = MixFile::open(std::move(bytes));
    auto names = m.local_names();
    if (names.empty()) { MESSAGE("no local db in this mix; skipped"); return; }
    int matched = 0;
    for (auto& nm : names)
        if (m.find(mix_id(nm))) matched++;
    // The vast majority of named files must resolve.
    CHECK(matched > 0);
    CHECK(matched >= (int)names.size() * 3 / 4);
}
```

Also a fixed-vector sanity check once a known ID is confirmed via oracle:

```cpp
TEST_CASE("mix_id known vector") {
    // Fill in ONE (name,id) pair confirmed from XCC Mixer's ID column, e.g.:
    //   CHECK(ra2assets::mix_id("cameo.pal") == 0xXXXXXXXX);
    // Left as a live check the executing agent fills from the oracle.
    CHECK(true);
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `mix_id` / `local_names` undefined.

- [ ] **Step 3: Implement `mix_id`, filename resolution, local db, nested MIX**

```cpp
#include <cctype>

namespace ra2assets {

uint32_t mix_id(const std::string& name_in) {
    // TS/RA2/YR scheme — NOT the TD/RA1 rotate-add scheme. Uppercase the name;
    // if the length is not a multiple of 4, append ONE byte whose value is
    // (len & 3), then repeat the FIRST character of the final 4-byte block
    // until the length is a multiple of 4; then take a standard CRC32
    // (reflected poly 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF).
    // Reference: XCC `get_id` (Cc_structures); OpenRA PackageHashType.CRC32.
    std::string s;
    s.reserve(name_in.size() + 4);
    for (char c : name_in) s.push_back((char)std::toupper((unsigned char)c));
    size_t l = s.size();
    if (l & 3u) {
        s.push_back((char)(l & 3u));
        size_t block_start = l & ~size_t(3);   // first char of the final block
        while (s.size() & 3u) s.push_back(s[block_start]);
    }
    uint32_t crc = 0xFFFFFFFFu;                // standard CRC32, bitwise form
    for (unsigned char ch : s) {
        crc ^= ch;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

const MixEntry* MixFile::find(const std::string& name) const {
    return find(mix_id(name));
}

std::vector<std::string> MixFile::local_names() const {
    // "local mix database.dat" id, then parse its XCC-format string table.
    const MixEntry* e = find(mix_id("local mix database.dat"));
    std::vector<std::string> out;
    if (!e) return out;
    auto data = read(*e);
    // XCC local db format: 32-byte header ("XCC by Olaf van der Spek\0" + type),
    // uint32 size, uint32 count, then `count` NUL-terminated names.
    if (data.size() < 0x34) return out;
    const uint8_t* p = data.data();
    uint32_t count = rd32(p + 0x30);
    size_t pos = 0x34;
    for (uint32_t i = 0; i < count && pos < data.size(); i++) {
        std::string nm;
        while (pos < data.size() && p[pos]) nm.push_back((char)p[pos++]);
        pos++; // skip NUL
        out.push_back(nm);
    }
    return out;
}

MixFile MixFile::open_nested(const std::string& name) const {
    const MixEntry* e = find(name);
    if (!e) throw std::runtime_error("mix: nested entry not found: " + name);
    return MixFile::open(read(*e));
}

} // namespace ra2assets
```

Add the three new method declarations to `mix.hpp`.

- [ ] **Step 4: Build and run — verify pass**

```bash
cmake --build build && RA2_INSTALL_DIR="D:\Red Alert 2" ./build/ra2assets_tests -tc="mix_id*"
```

Expected: PASS — all six nested-mix names resolve, and (if `ecache01.mix` carries a local db) ≥75% of its names resolve. If the nested-name test fails, re-check the two moving parts against the references independently: (a) the padding rule — append one byte equal to `len & 3`, then repeat the **first** character of the final 4-byte block (not the last character, and not zero-fill); (b) the CRC32 itself — reflected poly `0xEDB88320`, init and final-XOR `0xFFFFFFFF`. Confirm against XCC's `get_id` and, if still diverging, the RE of the id routine in `gamemd.exe`; document the resolution in `docs/re/mix.md`.

- [ ] **Step 5: Fill in the fixed known-vector** using an ID read from XCC Mixer, then re-run. Commit.

```bash
git add -A && git commit -m "feat(mix): TS/RA2 id hashing, local db, nested-mix support"
```

---

## Task 6: PAL palette loader

**Files:**
- Create: `include/ra2assets/pal.hpp`, `src/pal.cpp`, `tests/test_pal.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    struct Rgba { uint8_t r,g,b,a; };
    class Palette {
    public:
        static Palette load(const uint8_t* data, size_t len);   // 768 bytes
        const Rgba& operator[](int i) const { return c_[i]; }
    private:
        Rgba c_[256];
    };
  }
  ```

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "ra2assets/pal.hpp"
TEST_CASE("palette expands 6-bit channels to 8-bit by bit replication") {
    using namespace ra2assets;
    uint8_t raw[768] = {0};
    raw[0]=0;  raw[1]=63; raw[2]=32;      // index 0
    raw[3]=63; raw[4]=63; raw[5]=63;      // index 1 -> white
    Palette p = Palette::load(raw, 768);
    CHECK(p[0].r == 0);
    CHECK(p[0].g == 255);                 // (63<<2)|(63>>4) = 252|3 = 255
    CHECK(p[0].b == ((32<<2)|(32>>4)));   // 128|2 = 130
    CHECK(p[0].a == 255);
    CHECK(p[1].r == 255);
    CHECK(p[1].g == 255);
    CHECK(p[1].b == 255);
}
```

- [ ] **Step 2: Run to verify it fails.** Expected: header not found.

- [ ] **Step 3: Implement**

`include/ra2assets/pal.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
namespace ra2assets {
struct Rgba { uint8_t r,g,b,a; };
class Palette {
public:
    static Palette load(const uint8_t* data, size_t len);
    const Rgba& operator[](int i) const { return c_[i]; }
private:
    Rgba c_[256];
};
}
```
`src/pal.cpp`:
```cpp
#include "ra2assets/pal.hpp"
#include <stdexcept>
namespace ra2assets {
static uint8_t expand6(uint8_t v){ v &= 63; return (uint8_t)((v << 2) | (v >> 4)); }
Palette Palette::load(const uint8_t* data, size_t len) {
    if (len < 768) throw std::runtime_error("pal: need 768 bytes");
    Palette p;
    for (int i = 0; i < 256; i++) {
        p.c_[i].r = expand6(data[i*3+0]);
        p.c_[i].g = expand6(data[i*3+1]);
        p.c_[i].b = expand6(data[i*3+2]);
        p.c_[i].a = 255;
    }
    return p;
}
}
```

- [ ] **Step 4: Build and run — verify pass.**

- [ ] **Step 5: Commit** `feat(pal): 6-bit palette loader with bit-replication scaling`.

---

## Task 7: SHP sprite decoder (TS/RA2 variant)

**Files:**
- Create: `include/ra2assets/shp.hpp`, `src/shp.cpp`, `tests/test_shp.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace ra2assets {
    class ShpFile {
    public:
        static ShpFile load(std::vector<uint8_t> bytes);
        int count() const;                       // frame count
        int width() const;  int height() const;  // sheet dims
        struct Frame { int x,y,w,h; };
        Frame frame_info(int i) const;
        // Decodes frame i into an 8-bit indexed buffer sized w*h (frame-local).
        std::vector<uint8_t> decode_indexed(int i) const;
        // Palettizes an indexed buffer to RGBA (index 0 => transparent).
        static std::vector<Rgba> palettize(const std::vector<uint8_t>& idx,
                                           const Palette& pal);
    };
  }
  ```
- Consumes: `Palette`, `Rgba` (Task 6).

- [ ] **Step 1: Write failing tests for each compression mode with synthetic frames**

Construct SHP bytes by hand for: (a) uncompressed, (b) RLE-zero. Assert decoded pixels.

```cpp
#include "doctest.h"
#include "ra2assets/shp.hpp"
using namespace ra2assets;

// Helper: assemble a 1-frame SHP with given frame flags + pixel payload.
static std::vector<uint8_t> make_shp(uint16_t w, uint16_t h, uint32_t flags,
                                     const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> b;
    auto p16=[&](uint16_t v){ b.push_back(v&0xff); b.push_back(v>>8); };
    auto p32=[&](uint32_t v){ for(int i=0;i<4;i++) b.push_back(v>>(8*i)); };
    p16(0); p16(w); p16(h); p16(1);                 // file header: zero,cx,cy,frames
    size_t fh = b.size();
    p16(0); p16(0); p16(w); p16(h);                 // frame: x,y,cx,cy
    p32(flags); p32(0); p32(0);                     // compression, unknown, unknown/color
    size_t off_pos = b.size(); p32(0);              // data offset (patched below)
    uint32_t data_off = (uint32_t)b.size();
    for (auto v : payload) b.push_back(v);
    b[off_pos+0]=data_off; b[off_pos+1]=data_off>>8; b[off_pos+2]=data_off>>16; b[off_pos+3]=data_off>>24;
    (void)fh;
    return b;
}

TEST_CASE("shp decodes uncompressed frame") {
    // 2x2, flags=1 (uncompressed), pixels 1,2,3,4
    auto shp = make_shp(2,2,1,{1,2,3,4});
    ShpFile s = ShpFile::load(std::move(shp));
    REQUIRE(s.count()==1);
    auto idx = s.decode_indexed(0);
    REQUIRE(idx.size()==4);
    CHECK(idx[0]==1); CHECK(idx[1]==2); CHECK(idx[2]==3); CHECK(idx[3]==4);
}

TEST_CASE("shp decodes RLE-zero frame") {
    // RLE-zero (flags & 2, XCC "format 3"): EVERY scanline begins with a
    // uint16 giving the total encoded line length IN BYTES, including the
    // 2 prefix bytes themselves. After the prefix, bytes are copied verbatim
    // EXCEPT a 0x00, which is followed by a run-length byte emitting that
    // many zero pixels.
    // Row "5 0 0 6" encodes as: len=6 (2 prefix + 4 data), 05, 00 02, 06.
    auto shp = make_shp(4,1,2,{0x06,0x00, 0x05, 0x00,0x02, 0x06});
    ShpFile s = ShpFile::load(std::move(shp));
    auto idx = s.decode_indexed(0);
    REQUIRE(idx.size()==4);
    CHECK(idx[0]==5); CHECK(idx[1]==0); CHECK(idx[2]==0); CHECK(idx[3]==6);
}

TEST_CASE("shp RLE-zero honors the per-line length prefix across rows") {
    // Two rows, w=2. Row 0 = "7 8": len=4 -> 04 00 07 08.
    //                Row 1 = "0 0": len=4 -> 04 00 00 02 (zero-run of 2).
    auto shp = make_shp(2,2,2,{0x04,0x00,0x07,0x08, 0x04,0x00,0x00,0x02});
    ShpFile s = ShpFile::load(std::move(shp));
    auto idx = s.decode_indexed(0);
    REQUIRE(idx.size()==4);
    CHECK(idx[0]==7); CHECK(idx[1]==8); CHECK(idx[2]==0); CHECK(idx[3]==0);
}

TEST_CASE("palettize maps index 0 to transparent") {
    uint8_t raw[768]={0}; raw[3]=63;raw[4]=0;raw[5]=0;   // index1 = red
    Palette pal = Palette::load(raw,768);
    std::vector<uint8_t> idx = {0,1};
    auto rgba = ShpFile::palettize(idx, pal);
    CHECK(rgba[0].a==0);                 // index 0 transparent
    CHECK(rgba[1].r==255); CHECK(rgba[1].a==255);
}
```

- [ ] **Step 2: Run to verify it fails.** Expected: header not found.

- [ ] **Step 3: Implement the header** (`include/ra2assets/shp.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "ra2assets/pal.hpp"
namespace ra2assets {
class ShpFile {
public:
    static ShpFile load(std::vector<uint8_t> bytes);
    int count() const { return (int)frames_.size(); }
    int width() const { return cx_; }
    int height() const { return cy_; }
    struct Frame { int x,y,w,h; };
    Frame frame_info(int i) const { auto&f=frames_[i]; return {f.x,f.y,f.w,f.h}; }
    std::vector<uint8_t> decode_indexed(int i) const;
    static std::vector<Rgba> palettize(const std::vector<uint8_t>& idx, const Palette& pal);
private:
    struct FrameHdr { int x,y,w,h; uint32_t flags; uint32_t offset; };
    std::vector<uint8_t> bytes_;
    std::vector<FrameHdr> frames_;
    int cx_=0, cy_=0;
};
}
```

- [ ] **Step 4: Implement decoding** (`src/shp.cpp`)

```cpp
#include "ra2assets/shp.hpp"
#include <stdexcept>
namespace ra2assets {
static uint16_t r16(const uint8_t*p){return p[0]|(p[1]<<8);}
static uint32_t r32(const uint8_t*p){return p[0]|(p[1]<<8)|(p[2]<<16)|(uint32_t(p[3])<<24);}

ShpFile ShpFile::load(std::vector<uint8_t> bytes) {
    ShpFile s; s.bytes_=std::move(bytes);
    const uint8_t* p=s.bytes_.data();
    if (s.bytes_.size()<8) throw std::runtime_error("shp: too small");
    /*zero*/ s.cx_=r16(p+2); s.cy_=r16(p+4);
    int frames=r16(p+6);
    size_t off=8;
    for (int i=0;i<frames;i++){
        const uint8_t* f=p+off;
        FrameHdr h;
        h.x=r16(f+0); h.y=r16(f+2); h.w=r16(f+4); h.h=r16(f+6);
        h.flags=r32(f+8); h.offset=r32(f+20);
        s.frames_.push_back(h);
        off+=24;
    }
    return s;
}

std::vector<uint8_t> ShpFile::decode_indexed(int i) const {
    const FrameHdr& h=frames_[i];
    std::vector<uint8_t> out((size_t)h.w*h.h, 0);
    if (h.offset==0) return out;                 // empty frame
    const uint8_t* src=bytes_.data()+h.offset;
    const uint8_t* end=bytes_.data()+bytes_.size();
    if (h.flags & 2u) {                          // RLE-zero (XCC format 3)
        for (int y=0;y<h.h;y++){
            // Every scanline begins with a uint16 total line length in bytes
            // (including these 2 prefix bytes). Advance strictly by it —
            // never scan past a line's end into the next line's prefix.
            if (src+2>end) break;
            uint16_t line_len=r16(src);
            if (line_len<2 || src+line_len>end) break;   // malformed: stop cleanly
            const uint8_t* q=src+2;
            const uint8_t* qe=src+line_len;
            src+=line_len;
            int x=0;
            while (x<h.w && q<qe){
                uint8_t v=*q++;
                if (v==0){ uint8_t run = (q<qe)?*q++:0; for(uint8_t k=0;k<run && x<h.w;k++) out[y*h.w + x++]=0; }
                else out[y*h.w + x++]=v;
            }
        }
    } else {                                     // uncompressed
        size_t n=(size_t)h.w*h.h;
        for (size_t k=0;k<n && src<end;k++) out[k]=*src++;
    }
    return out;
}

std::vector<Rgba> ShpFile::palettize(const std::vector<uint8_t>& idx, const Palette& pal) {
    std::vector<Rgba> out(idx.size());
    for (size_t k=0;k<idx.size();k++){
        uint8_t ix=idx[k];
        if (ix==0){ out[k]={0,0,0,0}; }
        else out[k]=pal[ix];
    }
    return out;
}
}
```

> Note on RLE-zero: each scanline carries its own uint16 length prefix — confirmed by both XCC's `decode3` and OpenRA's ShpTS reader, which read a UInt16 per line. Omitting the prefix decodes garbage from every real asset while still looking plausible on hand-rolled data, so the synthetic tests above deliberately include it. TS/RA2 SHP frames are predominantly this format. If the real-asset oracle test (Task 9) reveals frames using LCW (format80) — some cutscene/large frames do — add an LCW decompressor branch keyed on the appropriate flag and re-verify. Keep the synthetic tests above green.

- [ ] **Step 5: Build and run — verify all shp tests pass.**

- [ ] **Step 6: Commit** `feat(shp): TS/RA2 SHP header + uncompressed/RLE-zero decode + palettize`.

---

## Task 8: `ra2extract` CLI (end-to-end)

**Files:**
- Create: `tools/ra2extract.cpp`
- Modify: `CMakeLists.txt` (add `ra2extract` executable)
- Vendor: `third_party/stb_image_write.h`

**Interfaces:**
- Consumes: `MixFile`, `Palette`, `ShpFile`.
- Produces: a CLI `ra2extract <mix> <shp-path> <pal-path> <out.png>` and `ra2extract <mix> --list [nested-path]`, where asset paths may descend nested mixes (`conquer.mix/e1.shp`). Nested paths are required to demonstrate the pipeline on stock assets: unit SHPs live in `conquer.mix` and palettes in `cache.mix`, sibling mixes nested inside `ra2.mix`.

- [ ] **Step 1: Vendor stb_image_write**

```bash
curl -L -o /d/ra2-android/third_party/stb_image_write.h \
  https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

- [ ] **Step 2: Write the CLI**

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/shp.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ra2assets;

static std::vector<uint8_t> slurp(const std::string& p){
    std::ifstream f(p,std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});
}

// Descend a nested-mix chain: "cache.mix" or "a.mix/b.mix".
static MixFile descend(MixFile m, std::string path){
    while (!path.empty()){
        size_t s = path.find('/');
        std::string head = (s==std::string::npos) ? path : path.substr(0,s);
        path = (s==std::string::npos) ? "" : path.substr(s+1);
        m = m.open_nested(head);
    }
    return m;
}

// Resolve "name.shp" or "inner.mix/name.shp" (any depth) to entry bytes.
static std::vector<uint8_t> read_path(const MixFile& m, const std::string& path){
    size_t s = path.find('/');
    if (s == std::string::npos){
        const MixEntry* e = m.find(path);
        if (!e) throw std::runtime_error("not found: " + path);
        return m.read(*e);
    }
    MixFile inner = m.open_nested(path.substr(0,s));
    return read_path(inner, path.substr(s+1));
}

int main(int argc, char** argv){
    if (argc<3){
        std::cerr<<"usage: ra2extract <mix> --list [nested-path]\n"
                 <<"       ra2extract <mix> <shp-path> <pal-path> <out.png>\n"
                 <<"       asset paths may descend nested mixes: conquer.mix/e1.shp\n";
        return 2;
    }
    try {
        auto bytes=slurp(argv[1]);
        if (bytes.empty()){ std::cerr<<"cannot read "<<argv[1]<<"\n"; return 1; }
        MixFile m = MixFile::open(std::move(bytes));
        if (std::string(argv[2])=="--list"){
            MixFile t = (argc>=4) ? descend(std::move(m), argv[3]) : std::move(m);
            std::cout<<"entries: "<<t.count()<<"\n";
            for (auto& e : t.entries())
                std::printf("  %08X  offset=%-10u size=%u\n", e.id, e.offset, e.size);
            for (auto& n : t.local_names()) std::cout<<"  "<<n<<"\n";
            return 0;
        }
        if (argc<5){ std::cerr<<"need <shp-path> <pal-path> <out.png>\n"; return 2; }
        ShpFile s=ShpFile::load(read_path(m, argv[2]));
        auto praw=read_path(m, argv[3]);
        Palette pal=Palette::load(praw.data(),praw.size());
        auto idx=s.decode_indexed(0);
        auto rgba=ShpFile::palettize(idx,pal);
        auto fi=s.frame_info(0);
        stbi_write_png(argv[4], fi.w, fi.h, 4, rgba.data(), fi.w*4);
        std::cout<<"wrote "<<argv[4]<<" ("<<fi.w<<"x"<<fi.h<<", "<<s.count()<<" frames)\n";
        return 0;
    } catch (const std::exception& ex){
        std::cerr<<"error: "<<ex.what()<<"\n";
        return 1;
    }
}
```

- [ ] **Step 3: Add to CMake**

```cmake
add_executable(ra2extract tools/ra2extract.cpp)
target_include_directories(ra2extract PRIVATE third_party)
target_link_libraries(ra2extract PRIVATE ra2assets)
```

- [ ] **Step 4: Build and smoke-test end-to-end** against a real nested mix

```bash
cmake --build build
# Top-level stock archives in this install: ra2.mix, ra2md.mix, ecache01.mix, ...
./build/ra2extract "D:\Red Alert 2\ra2.mix" --list
./build/ra2extract "D:\Red Alert 2\ra2.mix" --list cache.mix
# Stock layout: unit/building SHPs live in conquer.mix, palettes in cache.mix,
# both nested inside ra2.mix. Confirm exact asset names via XCC Mixer, then:
./build/ra2extract "D:\Red Alert 2\ra2.mix" conquer.mix/e1.shp cache.mix/unittem.pal e1.png
```

Expected: `--list` prints entry counts and hex IDs (cross-check a few IDs against XCC Mixer's ID column); the extract writes a non-empty PNG of a recognizable GI infantry frame. (`e1.shp` / `unittem.pal` are the expected stock names — verify via XCC Mixer and substitute if this install differs.)

- [ ] **Step 5: Commit** `feat(cli): ra2extract MIX→SHP→PAL→PNG end to end`.

---

## Task 9: Oracle test harness (byte-for-byte)

**Files:**
- Create: `tests/test_oracle.cpp`, `scripts/make_oracle.md`, `scripts/oracle_to_raw.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: full pipeline.
- Produces: automated tests asserting `ra2assets` indexed output equals oracle-exported ground truth for a fixed asset set. Tests **skip gracefully** when the git-ignored `oracle/` directory is absent (so asset-free CI still builds and links — satisfying spec success criterion #3).

- [ ] **Step 1: Document oracle generation** (`scripts/make_oracle.md`)

Written steps for generating ground truth with **XCC Mixer** (chosen oracle: it is the community-standard tool, exposes exact IDs, and exports frames as indexed data):
1. Open the target MIX in XCC Mixer.
2. Select a fixed asset list (record names in the doc, e.g. a small infantry SHP + its palette).
3. Export each SHP frame as an **8-bit indexed** image (BMP/PCX/PNG — whatever XCC offers) into `D:\ra2-android\oracle\<mixname>\<shpname>\`.
4. Convert every exported image to a **headerless raw index dump** (`frameNN.raw`: row-major `w*h` bytes, top-down) with `scripts/oracle_to_raw.py`. The test compares raw bytes and must not have to parse BMP headers, row padding, or bottom-up row order — Pillow normalizes all of that on load.
5. Also export the reference **RGBA** PNG for one frame to calibrate the palette-scaling constant (Task 6). If XCC's rendered RGBA differs from `(v<<2)|(v>>4)`, record the difference; if byte-for-byte RGBA parity with XCC is required, switch the single `expand6` constant to match XCC (`v<<2`) and note the divergence from the game's own scaling (to be confirmed by RE in Task 11).

Create `scripts/oracle_to_raw.py` (requires `pip install pillow`):

```python
#!/usr/bin/env python3
"""Convert XCC-exported indexed images (BMP/PCX/PNG) to headerless .raw index dumps."""
import sys
from PIL import Image

for path in sys.argv[1:]:
    im = Image.open(path)
    if im.mode != "P":
        raise SystemExit(f"{path}: not an indexed image (mode={im.mode})")
    out = path.rsplit(".", 1)[0] + ".raw"
    with open(out, "wb") as f:
        f.write(im.tobytes())      # top-down, row-major w*h palette indices
    print(f"{out}: {im.width}x{im.height}")
```

- [ ] **Step 2: Write the oracle test** (indexed byte-for-byte; the palette-independent gate)

```cpp
#include "doctest.h"
#include "ra2assets/mix.hpp"
#include "ra2assets/shp.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
using namespace ra2assets;

static std::vector<uint8_t> slurp_file(const fs::path& p){
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});
}

// Same nested-path resolver as tools/ra2extract.cpp ("inner.mix/name.shp").
static std::vector<uint8_t> read_path(const MixFile& m, const std::string& path){
    size_t s = path.find('/');
    if (s == std::string::npos){
        const MixEntry* e = m.find(path);
        REQUIRE(e != nullptr);
        return m.read(*e);
    }
    MixFile inner = m.open_nested(path.substr(0,s));
    return read_path(inner, path.substr(s+1));
}

TEST_CASE("indexed output matches XCC oracle byte-for-byte") {
    const char* od = std::getenv("RA2_ORACLE_DIR");
    fs::path base = od ? od : "D:\\ra2-android\\oracle";
    if (!fs::exists(base)) { MESSAGE("oracle dir absent; skipped"); return; }
    const char* id = std::getenv("RA2_INSTALL_DIR");
    fs::path install = id ? id : "D:\\Red Alert 2";
    // manifest.txt: one asset per line, whitespace-separated fields:
    //   <mix-name-relative-to-install> <shp-path-within-mix> <frame-dir-under-oracle>
    // e.g.:  ra2.mix  conquer.mix/e1.shp  ra2/e1
    // (Mix names are install-relative so fields never contain spaces.)
    fs::path manifest = base / "manifest.txt";
    if (!fs::exists(manifest)) { MESSAGE("no manifest; skipped"); return; }
    std::ifstream mf(manifest);
    std::string mixname, shppath, framedir;
    int frames_checked = 0;
    while (mf >> mixname >> shppath >> framedir) {
        auto mixbytes = slurp_file(install / mixname);
        REQUIRE_FALSE(mixbytes.empty());
        MixFile m = MixFile::open(std::move(mixbytes));
        ShpFile s = ShpFile::load(read_path(m, shppath));
        for (int i = 0; i < s.count(); i++) {
            char fn[32]; std::snprintf(fn, sizeof fn, "frame%02d.raw", i);
            fs::path fp = base / framedir / fn;
            if (!fs::exists(fp)) continue;      // only exported frames are compared
            auto want = slurp_file(fp);
            auto got  = s.decode_indexed(i);
            REQUIRE(got.size() == want.size());
            CHECK(std::equal(got.begin(), got.end(), want.begin()));
            frames_checked++;
        }
    }
    CHECK(frames_checked > 0);   // a manifest that exercises zero frames is a failure
}
```

> The gate is `CHECK(std::equal(...))` per frame on the indexed buffers. Indexed comparison is chosen as the primary gate because it is independent of the palette-scaling debate and therefore unambiguous. The final `frames_checked > 0` assertion closes the silent-skip failure mode where a malformed manifest lets the suite pass while comparing nothing.

- [ ] **Step 3: Add to CMake, build, run** (skips cleanly with no oracle dir).

```bash
cmake --build build && ./build/ra2assets_tests -tc="indexed output*"
```
Expected with no oracle dir: `oracle dir absent; skipped`, test passes.

- [ ] **Step 4: Generate a real oracle set and confirm byte-for-byte parity.** Record results in `docs/re/shp.md`.

- [ ] **Step 5: Commit** `test(oracle): byte-for-byte indexed comparison harness + docs`.

---

## Task 10: Android arm64-v8a cross-compile

**Files:**
- Modify: `CMakeLists.txt` (shared-lib option), `README.md` (NDK build instructions)

(No toolchain wrapper file is needed — Step 3 uses the NDK's own `android.toolchain.cmake` directly.)

**Interfaces:**
- Produces: `libra2assets.a`/`.so` built for `arm64-v8a`, proving portability (spec success criterion #3). No app; on-device is at most a smoke test.

- [ ] **Step 1: Install the NDK** (prerequisite). Set `ANDROID_NDK_HOME`.

- [ ] **Step 2: Add a shared-library option** so we can inspect an `.so`

In `CMakeLists.txt`:
```cmake
option(RA2_BUILD_SHARED "Build ra2assets as a shared lib (for Android .so check)" OFF)
if (RA2_BUILD_SHARED)
    set_target_properties(ra2assets PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
```

- [ ] **Step 3: Configure + build for arm64 using the NDK toolchain**

```bash
cmake -S . -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DRA2_BUILD_SHARED=ON
cmake --build build-android --target ra2assets
```

Expected: builds without error.

- [ ] **Step 4: Assert the artifact is ARM64 ELF**

```bash
python - <<'PY'
import struct, glob
for f in glob.glob("build-android/*ra2assets*"):
    with open(f,'rb') as fh:
        m=fh.read(20)
    if m[:4]==b'\x7fELF':
        machine=struct.unpack('<H',m[18:20])[0]
        print(f, "machine=0x%x"%machine, "AArch64" if machine==0xB7 else "NOT-ARM64")
PY
```

Expected: prints `machine=0xb7 AArch64` for the built library.

- [ ] **Step 5: Commit** `build(android): arm64-v8a cross-compile via NDK toolchain`.

---

## Task 11: Ghidra methodology — locate, decompile, confidence-rate

This delivers the second half of the sub-project: a **proven, documented `decompile → reimplement → verify` workflow**. It runs against the pinned `gamemd.exe`.

**Files:**
- Create: `docs/re/workflow.md`, `docs/re/mix.md`, `docs/re/pal.md`, `docs/re/shp.md`

**Interfaces:**
- Consumes: the reimplementations from Tasks 1–7 (the "known answer" to calibrate RE against).
- Produces: documented located functions with confidence ratings and any divergence from our reimplementation.

- [ ] **Step 1: Confirm the toolchain** (already installed). Ghidra 11.2.1 lives at `C:\work\DentrixDocumentsExtractor\ghidra_11.2.1_PUBLIC\`; JDK 21 at `C:\Program Files\Java\jdk-21`. Verify headless launches and picks JDK 21 (11.2.1 will refuse JDK 11):

```bash
export GHIDRA_HOME="/c/work/DentrixDocumentsExtractor/ghidra_11.2.1_PUBLIC"
export JAVA_HOME="/c/Program Files/Java/jdk-21"
"$GHIDRA_HOME/support/analyzeHeadless" -help >/dev/null && echo "ghidra headless OK"
```

Create the project directory `D:\ra2-re\` (outside the repo).

- [ ] **Step 2: Import and auto-analyze `gamemd.exe`** (headless)

```bash
"$GHIDRA_HOME/support/analyzeHeadless" "D:\ra2-re" ra2 \
  -import "D:\Red Alert 2\gamemd.exe" -analysisTimeoutPerFile 3600
```

> Note: `D:\ra2-re\` is the shared Ghidra workspace root. If it already holds unrelated projects, use a distinct project name (`ra2`) so nothing collides.

- [ ] **Step 3: Load YRpp/Ares symbol maps.** Clone YRpp and Ares locally under `D:\ra2-re\reference\`. Use their published class layouts and addresses (`MixFileClass`, palette load, `SHPStruct`/`SHPReference`) to name functions in Ghidra. Document which YRpp header/address each maps to.

- [ ] **Step 4: Locate and decompile the MIX loader.** In `docs/re/mix.md`: paste the Ghidra decompilation of the MIX open/index-parse and the id-hash routine; compare against our `src/mix.cpp` + `mix_id`; confirm the id routine is the CRC32 scheme with count-byte-then-repeat padding (not the TD/RA1 rotate-add) that Task 5 implements; rate confidence (High/Med/Low) with justification.

- [ ] **Step 5: Locate and decompile the palette conversion.** In `docs/re/pal.md`: find where 6-bit palette entries are expanded for the display surface; **record the exact scaling operation** (`<<2` vs `(v<<2)|(v>>4)` vs other). This authoritatively resolves the palette-scaling rule for the whole program. If it differs from Task 6's default, update `expand6` and re-run oracle RGBA checks; document the change.

- [ ] **Step 6: Locate and decompile the SHP decoder.** In `docs/re/shp.md`: identify the frame decompressors, confirm which compression flags map to RLE-zero vs LCW, cross-check against `src/shp.cpp`, and rate confidence.

- [ ] **Step 7: Write `docs/re/workflow.md`** — the reusable machine: how to import, how to apply YRpp maps, the decompile→reimplement→verify→confidence loop, and lessons learned (what Ghidra got wrong about `thiscall`/vtables, how oracle equality closed the loop). This is the deliverable later phases depend on.

- [ ] **Step 8: Commit** `docs(re): Ghidra methodology + located MIX/PAL/SHP loaders with confidence ratings`.

---

## Self-Review Notes (author checklist — completed)

- **Spec coverage:** `ra2assets` lib (Tasks 1–7) ✓; `ra2extract` CLI (Task 8) ✓; oracle byte-for-byte tests (Task 9) ✓; host + arm64 builds (Tasks 0, 10) ✓; Ghidra workflow documented with ≥3 located functions + confidence (Task 11) ✓; all four success criteria mapped. Legal/no-assets-committed enforced via `.gitignore` + `RA2_INSTALL_DIR` (Task 0, Global Constraints) ✓.
- **Open items settled:** repo at `D:\ra2-android\` (Global Constraints); `gamemd.exe` = 1.001 pinned by SHA1 (Task 0 preflight); oracle = XCC Mixer with indexed comparison as the palette-independent primary gate (Task 9); palette scaling default `(v<<2)|(v>>4)`, authoritatively confirmed/adjusted by RE in Task 11 Step 5 (Task 6, 9, 11 cross-referenced).
- **Known deferred-to-executing-agent bodies (intentional, not placeholders):** the verbatim big-int port in Task 2 Step 4 (flagged loudly; correctness gated by Task 4's real-archive test) and the oracle comparison loop in Task 9 Step 2 (gated by a generated manifest). Both are external-reference ports that must not be paraphrased; each has a functional test as its correctness gate.
- **Type consistency:** `MixEntry`, `MixFile`, `Palette`, `Rgba`, `ShpFile`, `mix_id`, `expand6` names are consistent across Tasks 1–8. `Rgba` defined once in `pal.hpp`, consumed by `shp.hpp`.

---

## Review Corrections (applied 2026-07-08)

A pre-execution review against the actual install and the reference format documentation corrected the following (details live in each task):

1. **Task 5 — `mix_id` algorithm replaced.** The draft used the TD/RA1 rotate-add scheme. TS/RA2/YR IDs are a **standard CRC32** (reflected poly `0xEDB88320`) over the uppercased name with count-byte-then-repeat padding (XCC `get_id`, OpenRA `PackageHashType.CRC32`). The draft's recovery guidance ("the variants differ only in padding") was also wrong — the variants differ in the hash function itself — and was replaced.
2. **Task 5 — primary gate replaced.** Stock Westwood mixes do not contain `local mix database.dat` (it is an XCC-tooling artifact), so the draft's `language.mix` test would have silently skipped and let a wrong hash through. The primary gate is now nested-mix-name resolution against `ra2.mix`/`ra2md.mix` (cannot skip); the local-db check moved to `ecache01.mix` as a secondary.
3. **Task 7 — RLE-zero scanline length prefix added.** TS/RA2 format-3 scanlines each begin with a uint16 total line length (XCC `decode3`, OpenRA ShpTS). The draft decoder and its synthetic test omitted it and would have decoded garbage from every real asset while passing their own tests.
4. **Task 4 — bounds checks added** to the encrypted-header path so a wrong Blowfish key (a botched Task 2 port) fails with a diagnostic instead of an out-of-bounds read. Task 4 is the only correctness gate for Task 2, so it must fail loudly, not crash.
5. **Task 8 — CLI gained nested-mix asset paths** (`conquer.mix/e1.shp`), and the smoke test now targets files that exist (the draft referenced a nonexistent `cache01.mix`). Stock SHPs and palettes live in sibling mixes nested inside `ra2.mix`, so a single-level CLI could not demonstrate the pipeline end-to-end.
6. **Task 9 — oracle format pipeline defined.** XCC exports indexed BMP/PCX/PNG but the test reads raw dumps; added `scripts/oracle_to_raw.py` (Pillow) plus a concrete manifest format (mix names install-relative, so fields never contain spaces — `D:\Red Alert 2` would break whitespace-tokenized parsing). The comparison-loop sketch was completed and now fails if zero frames were compared.
7. **Task 1 — cleanups.** Removed the malformed `V vs[]` test fragment, the dead `zero`/`out` variables in `set_key`, and the pi-digit generation script (it printed nothing, so it would have produced an empty `.inc` and a build break); the tables are now pasted verbatim from a canonical source.
8. **Task 0 / Task 10 — structural tidy-ups.** The spec and plan are copied into the repo (`D:\ra2-android\docs\`) so they version with the code; preflight now warns on all four test-relevant mixes; the unused `cmake/android-arm64.cmake` wrapper was dropped (Task 10 uses the NDK's own toolchain file); stale task-number cross-references in the file tree were fixed.

Environmental assumptions verified during review: `gamemd.exe` SHA1 = `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`, 4,808,976 bytes (YR 1.001 — matches the pin); `ra2.mix`, `ra2md.mix`, `language.mix`, `ecache01.mix` all present in `D:\Red Alert 2`.

9. **Task 11 / Prerequisites — Ghidra confirmed installed.** Ghidra 11.2.1 is present at `C:\work\DentrixDocumentsExtractor\ghidra_11.2.1_PUBLIC\` and JDK 21 at `C:\Program Files\Java\jdk-21`; the "install Ghidra" step became a "confirm toolchain and point `JAVA_HOME` at JDK 21" step (11.2.1 rejects the also-installed JDK 11). The Android NDK prerequisite was misattributed to Task 8 and now correctly points to Task 10.
