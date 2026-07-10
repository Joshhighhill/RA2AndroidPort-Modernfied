# Audio Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the proven BAG/ADPCM decoder to real SDL sound output — weapon reports, unit voice acks, death screams, EVA "Unit lost" — with playback semantics recovered from gamemd.exe.

**Architecture:** Pure software mixer (`engine/audio_mixer`, no SDL, hand-computed-vector-testable) + thin SDL queue-push sink (`engine/audio_sdl`). New thin readers in ra2assets: `Sounds` (soundmd.ini, mirrors Rules/Art) and `load_wav_ima` (RIFF fmt-0x0011 for EVA). Render loop observes World transitions (fire/death) and Input actions (select/move/attack) — audio NEVER mutates sim state.

**Tech Stack:** C++17, SDL2 (already vendored), doctest, CMake+Ninja+MinGW; Ghidra headless at D:\ra2-re for Task 1.

## Global Constraints

- Ground truth: `docs/re/audio.md` (BAG, complete) + `docs/re/audio-playback.md` (Task 1 output). Content-anchoring only — hardcoded addresses unreliable in this binary even at matching SHA1.
- Real soundmd.ini `[Defaults]` (extracted this session, binding test pins): `MinVolume=50 Range=10 Volume=80 Limit=5 Type=NORMAL SCREEN UNSHROUD Priority=NORMAL`.
- Real entry pins: `[GIAttack] Sounds=igiat1a igiat1b igiat1c / Control=random interrupt / VShift=15 / Volume=65`; `[ConscriptAttack]` 7 names (with duplicates) `Limit=3 Volume=65`; `[GIDie]` 5 `$`-variants `Priority=low FShift=-10 10 Volume=85`; `[GISelect]` 6 `$`-variants `Volume=85`.
- rulesmd pins: `[M60] Report=GIAttack`, `[M1Carbine] Report=ConscriptAttack`, `[E1] VoiceSelect=GISelect VoiceMove=GIMove VoiceAttack=GIAttackCommand DieSound=GIDie`, `[E2]` → `ConscriptSelect/ConscriptMove/ConscriptAttackCommand/ConscriptDie`.
- EVA: `evamd.ini [EVA_UnitLost] Allied=ceva064`; file at `language.mix → audio.mix → ceva064.wav` (RIFF fmt 0x0011, mono, 22050 Hz, block_align 512, samples/block 1017, fact=18559 samples, first block predictor = −13).
- BAG load path (proven, tests/test_audio.cpp:305): `language.mix → audio.mix → audio.idx + audio.bag`; all clips 22,050 Hz; `AudioIdx::find` case-insensitive; soundmd `$` prefix is NOT in idx names (strip it).
- Audio-local seeded RNG only; `--demo-combat` must stay byte-identical (it never initializes audio).
- Tests skip cleanly (MESSAGE + return) without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`); doctest `CHECK_FALSE((a && b))` parens; run exes from PowerShell.
- Pure sources → `cmake/ra2_sources.cmake` (shared with Android jni): `src/sounds.cpp`, `src/wav.cpp` beside `src/rules.cpp`; `engine/audio_mixer.cpp` beside `engine/combat.cpp`. SDL-aware `engine/audio_sdl.cpp` goes ONLY where other SDL-aware engine files (e.g. `engine/iso_renderer.cpp`? — no, iso_renderer is SDL-aware: follow wherever `engine/main.cpp`'s SDL companions like `engine/platform*.cpp` are listed for the desktop engine target AND the Android app target, NOT the test target).
- No assets/GPL/Ghidra artifacts committed. Commits on `master`.
- **Controller amendment gate:** Tasks 4–6 contain fallback constants/formulas marked `[RE-FALLBACK]`. After Task 1's doc lands, the controller diffs those against the doc's findings and commits plan amendments BEFORE dispatching the affected task.
- **AMENDED post-Task-1 (docs/re/audio-playback.md "Decisions for ra2engine" — all now binding):** attenuation linear `1 − dist/range` CONFIRMED (High); distance metric = Chebyshev `max(|dx_px|, 2·|dy_px|)` in screen pixels from the view centre, ÷60 to cells; positional rule = EVERY event attenuates, `Type` containing `GLOBAL` adds a `MinVolume` floor (the old "acks non-positional" guess is DEAD); priority scale is 5 levels `lowest=0 low=1 normal=2 high=3 critical=4`; full voice pool DROPS the new sound (vanilla has no priority-steal of playing SFX voices); `Limit` reject-at-trigger kept as a documented approximation of vanilla's start-then-cull-to-cap; `interrupt` = steal oldest same-key (kept); `$` AND `#` prefixes both strip; `Control=random` = uniform with replacement (no shuffle-bag); VShift/FShift deferred with formulas recorded in the RE doc (carry-forward).

---

### Task 1: Ghidra hunt — playback semantics → `docs/re/audio-playback.md`

**Files:**
- Create: `docs/re/audio-playback.md` (prose + confidence table; NO Ghidra artifacts committed)
- Off-repo workspace: hunt scripts in `D:\ra2-re\scripts\` (e.g. `sndplay_dump.py`, numbered variants), outputs to `D:\ra2-re\out\`

**Interfaces:**
- Consumes: Ghidra project `D:\ra2-re\ra2.gpr` (YR 1.001 gamemd.exe, already analyzed), headless workflow per `docs/re/workflow.md` (analyzeHeadless.bat, JDK 21, Jython scripts).
- Produces: the calibration doc Tasks 4–6 bind to.

- [ ] **Step 1: Read the prior art.** `docs/re/audio.md` (esp. `FUN_00406b10` audio bootstrap, `FUN_004011c0` GABA loader, `FUN_004015c0` name→index bsearch, `FUN_00409c40` format dispatch — these are the entry points into the playback layer) and `docs/re/workflow.md` (content-anchoring techniques, function-gap handling).

- [ ] **Step 2: Anchor on the sound-INI vocabulary.** The strings `"Sounds"`, `"Volume"`, `"Priority"`, `"Limit"`, `"Range"`, `"MinVolume"`, `"Control"`, `"VShift"`, `"FShift"`, `"Delay"`, `"Type"`, and token literals `"random"`, `"interrupt"`, `"loop"`, `"ambient"`, `"GLOBAL"`, `"SCREEN"`, `"LOCAL"`, `"PLAYER"`, `"UNSHROUD"` must exist for the soundmd parser. Script: enumerate defined strings, xref each key to its parse site(s); the parse function reveals the in-memory sound-entry struct (field offsets + defaults written before parsing). Record the struct layout and every default.

- [ ] **Step 3: Trace the play call path.** From the parse struct's consumers, locate (a) the "play sound event by index/name" entry point; (b) **variant selection** for `Control=random` — which RNG function, uniform-vs-no-repeat, and whether `$` changes lookup (also check the `$`-strip site — where the leading char is tested); (c) **`Limit=` enforcement** — reject vs steal when the count is at limit, and what `interrupt` changes; (d) **`Priority=` stealing** — comparison rule when the voice pool is full.

- [ ] **Step 4: Recover the attenuation math.** Find where `Range`/`MinVolume` fields are read at play time: the function converting an event coordinate + tactical view position into a final volume. Recover the exact curve (linear/quadratic/step), what distance metric it uses (leptons? cells? screen px?), what `Type=SCREEN` vs `GLOBAL` vs `LOCAL` changes, and the fate of `VShift`/`FShift` (volume/pitch jitter — formula + RNG source).

- [ ] **Step 5: EVA queue.** Anchor: `"eva"`/`"EVA"` strings, evamd key strings (`"Priority"` value literals `"IMPORTANT"`/`"CRITICAL"`), the speech queue push/pop. Record: queue depth/order rules, whether a playing EVA is ever interrupted, volume class vs SFX.

- [ ] **Step 6: Write `docs/re/audio-playback.md`.** Same shape as `docs/re/combat.md`: per-item located function(s) + verdict + confidence (High/Med/Low), honest **Low + ModEnc/XCC fallback** rows for anything not located within budget. MUST include a "Decisions for ra2engine" section listing each `[RE-FALLBACK]` item from Tasks 4–6 of this plan with its confirmed-or-fallback resolution, so the controller's amendment diff is mechanical.

- [ ] **Step 7: Commit** — `git add docs/re/audio-playback.md && git commit -m "docs(re): audio playback semantics recovered from gamemd (attenuation, limit/priority, variants, EVA)"`.

---

### Task 2: `ra2assets::Sounds` + Rules voice getters

**Files:**
- Create: `include/ra2assets/sounds.hpp`, `src/sounds.cpp`, `tests/test_sounds.cpp`
- Modify: `include/ra2assets/rules.hpp` + `src/rules.cpp` (4 voice getters), `cmake/ra2_sources.cmake` (add `src/sounds.cpp` beside `src/rules.cpp`), `CMakeLists.txt` (add `tests/test_sounds.cpp` beside `tests/test_rules.cpp`)

**Interfaces:**
- Consumes: `ra2assets::Ini` (`get`, case-insensitive), Rules' thin by-reference pattern.
- Produces (Tasks 4/6 rely on these EXACT names):

```cpp
// include/ra2assets/sounds.hpp
struct SoundEntry {
    std::vector<std::string> sounds;   // variant clip names, leading '$' stripped
    int volume = 80;                    // [Defaults]-seeded at load
    std::string priority = "normal";    // lower-cased
    int limit = 5;
    double range = 10.0;                // cells
    int min_volume = 50;
    std::vector<std::string> control;   // lower-cased tokens ("random","interrupt",...)
    std::vector<std::string> type;      // lower-cased tokens ("normal","screen",...)
    int vshift = 0;
    int fshift_lo = 0, fshift_hi = 0;
};
class Sounds {
public:
    static Sounds load(const Ini& ini);            // reads [Defaults] once
    bool has(const std::string& name) const;
    SoundEntry entry(const std::string& name) const; // absent -> the [Defaults] entry
private:
    explicit Sounds(const Ini& ini);
    const Ini* ini_;
    SoundEntry defaults_;
};
// rules.hpp additions (all: "[unit_id] Key=", "" if absent):
std::string voice_select(const std::string& unit_id) const;
std::string voice_move(const std::string& unit_id) const;
std::string voice_attack(const std::string& unit_id) const;
std::string die_sound(const std::string& unit_id) const;
```

- [ ] **Step 1: Write the failing tests** (`tests/test_sounds.cpp`):

```cpp
#include "doctest.h"
#include "ra2assets/ini.hpp"
#include "ra2assets/sounds.hpp"

using namespace ra2assets;

static const char* kSyntheticSound =
    "[Defaults]\n"
    "MinVolume=50\nRange=10\nVolume=80\nLimit=5\n"
    "Type= NORMAL SCREEN UNSHROUD\nPriority=NORMAL\n"
    "\n"
    "[GIAttack]\n"
    "Sounds=igiat1a igiat1b igiat1c\n"
    "Control= random interrupt\n"
    "VShift=15\nVolume=65\n"
    "\n"
    "[GIDie]\n"
    "Sounds= $igidia $igidib\n"
    "Priority=low\nControl= random\nFShift= -10 10\nVolume=85\n";

TEST_CASE("sounds: entry inherits [Defaults] for absent keys") {
    Ini ini = Ini::parse(kSyntheticSound);
    Sounds snd = Sounds::load(ini);
    SoundEntry e = snd.entry("GIAttack");
    CHECK(e.volume == 65);      // overridden
    CHECK(e.limit == 5);        // from [Defaults]
    CHECK(e.range == doctest::Approx(10.0));
    CHECK(e.min_volume == 50);
    CHECK(e.priority == "normal");
    REQUIRE(e.type.size() == 3);
    CHECK(e.type[0] == "normal");
    CHECK(e.type[1] == "screen");
    CHECK(e.type[2] == "unshroud");
}

TEST_CASE("sounds: variant list, $ stripping, control tokens, shifts") {
    Ini ini = Ini::parse(kSyntheticSound);
    Sounds snd = Sounds::load(ini);
    SoundEntry a = snd.entry("GIAttack");
    REQUIRE(a.sounds.size() == 3);
    CHECK(a.sounds[0] == "igiat1a");
    CHECK(a.sounds[2] == "igiat1c");
    REQUIRE(a.control.size() == 2);
    CHECK(a.control[0] == "random");
    CHECK(a.control[1] == "interrupt");
    CHECK(a.vshift == 15);
    SoundEntry d = snd.entry("GIDie");
    REQUIRE(d.sounds.size() == 2);
    CHECK(d.sounds[0] == "igidia");   // '$' stripped
    CHECK(d.priority == "low");
    CHECK(d.fshift_lo == -10);
    CHECK(d.fshift_hi == 10);
    // '#' strips exactly like '$' (docs/re/audio-playback.md): extend the
    // synthetic text with "[HashCase]\nSounds= #habc $hdef\n" and assert
    // entry("HashCase").sounds == {"habc","hdef"}.
    CHECK(snd.has("GIDie"));
    CHECK_FALSE(snd.has("NoSuchSound"));
    SoundEntry missing = snd.entry("NoSuchSound");   // -> defaults verbatim
    CHECK(missing.sounds.empty());
    CHECK(missing.volume == 80);
    CHECK(missing.limit == 5);
}

TEST_CASE("sounds: real soundmd.ini pins") {
    // Install guard + ra2md.mix nested-candidate loading: copy the exact
    // shape from tests/test_art.cpp's real-artmd test, but for
    // "soundmd.ini" (same localmd.mix location).
    // ... guard: RA2_INSTALL_DIR default D:\Red Alert 2, MESSAGE+return ...
    // Ini ini = Ini::parse(...soundmd bytes...);
    Sounds snd = Sounds::load(ini);
    SoundEntry def = snd.entry("NoSuchSound__");
    CHECK(def.volume == 80);
    CHECK(def.limit == 5);
    CHECK(def.range == doctest::Approx(10.0));
    CHECK(def.min_volume == 50);
    SoundEntry gia = snd.entry("GIAttack");
    REQUIRE(gia.sounds.size() == 3);
    CHECK(gia.sounds[0] == "igiat1a");
    CHECK(gia.volume == 65);
    CHECK(gia.vshift == 15);
    REQUIRE(gia.control.size() == 2);
    CHECK(gia.control[1] == "interrupt");
    SoundEntry cona = snd.entry("ConscriptAttack");
    CHECK(cona.sounds.size() == 7);
    CHECK(cona.limit == 3);
    SoundEntry gid = snd.entry("GIDie");
    CHECK(gid.sounds.size() == 5);
    CHECK(gid.sounds[0] == "igidia");
    CHECK(gid.priority == "low");
    CHECK(gid.fshift_lo == -10);
    CHECK(gid.fshift_hi == 10);
    SoundEntry gis = snd.entry("GISelect");
    CHECK(gis.sounds.size() == 6);
    CHECK(gis.volume == 85);
}
```

And in `tests/test_rules.cpp`, extend the existing synthetic + real-rulesmd cases:

```cpp
// synthetic (append to an existing synthetic-Ini rules case):
CHECK(rules.voice_select("E9") == "");   // absent -> ""
// real-install case additions:
CHECK(rules.voice_select("E1") == "GISelect");
CHECK(rules.voice_move("E1") == "GIMove");
CHECK(rules.voice_attack("E1") == "GIAttackCommand");
CHECK(rules.die_sound("E1") == "GIDie");
CHECK(rules.voice_select("E2") == "ConscriptSelect");
CHECK(rules.die_sound("E2") == "ConscriptDie");
```

- [ ] **Step 2: Run to verify failure** — `cmake --build build` fails (sounds.hpp missing).

- [ ] **Step 3: Implement.** `src/sounds.cpp` — parsing helpers (whitespace-split; lower-case; leading-`$` strip; two-int FShift). The four Rules getters are one-liners following `rules.cpp`'s `armor()` pattern (`ini_->get(unit_id, "VoiceSelect", "")` etc.).

```cpp
#include "ra2assets/sounds.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ra2assets {
namespace {
std::vector<std::string> split_ws(const std::string& v) {
    std::vector<std::string> out;
    std::istringstream ss(v);
    std::string tok;
    while (ss >> tok) out.push_back(tok);
    return out;
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}  // namespace

Sounds::Sounds(const Ini& ini) : ini_(&ini) {}

Sounds Sounds::load(const Ini& ini) {
    Sounds s(ini);
    // Seed the shared defaults from [Defaults] (the file's own mechanism --
    // "the default ratings" per soundmd.ini's header comment). Absent
    // section/keys leave the C++ member initializers (which mirror the real
    // file's values) in place.
    SoundEntry& d = s.defaults_;
    d.volume = ini.get_int("Defaults", "Volume", d.volume);
    d.limit = ini.get_int("Defaults", "Limit", d.limit);
    d.min_volume = ini.get_int("Defaults", "MinVolume", d.min_volume);
    d.range = std::atof(ini.get("Defaults", "Range", "10").c_str());
    d.priority = lower(ini.get("Defaults", "Priority", d.priority));
    const std::string ty = ini.get("Defaults", "Type", "");
    if (!ty.empty()) {
        d.type.clear();
        for (const std::string& t : split_ws(ty)) d.type.push_back(lower(t));
    }
    return s;
}

bool Sounds::has(const std::string& name) const { return ini_->has(name); }

SoundEntry Sounds::entry(const std::string& name) const {
    SoundEntry e = defaults_;
    if (!ini_->has(name)) return e;
    for (const std::string& raw : split_ws(ini_->get(name, "Sounds", "")))
        // gamemd's add_sound strips a leading '$' OR '#' (docs/re/
        // audio-playback.md, High confidence).
        e.sounds.push_back((raw[0] == '$' || raw[0] == '#') ? raw.substr(1) : raw);
    e.volume = ini_->get_int(name, "Volume", e.volume);
    e.limit = ini_->get_int(name, "Limit", e.limit);
    e.min_volume = ini_->get_int(name, "MinVolume", e.min_volume);
    const std::string rg = ini_->get(name, "Range", "");
    if (!rg.empty()) e.range = std::atof(rg.c_str());
    e.priority = lower(ini_->get(name, "Priority", e.priority));
    e.vshift = ini_->get_int(name, "VShift", e.vshift);
    const std::vector<std::string> fs = split_ws(ini_->get(name, "FShift", ""));
    if (fs.size() >= 2) {
        e.fshift_lo = std::atoi(fs[0].c_str());
        e.fshift_hi = std::atoi(fs[1].c_str());
    }
    for (const std::string& t : split_ws(ini_->get(name, "Control", "")))
        e.control.push_back(lower(t));
    const std::string ty = ini_->get(name, "Type", "");
    if (!ty.empty()) {
        e.type.clear();
        for (const std::string& t : split_ws(ty)) e.type.push_back(lower(t));
    }
    return e;
}
}  // namespace ra2assets
```

  (Note: `defaults_.type` starts as `{"normal","screen","unshroud"}` via the member initializer in the header? No — keep the struct's `type` default EMPTY in the header and set the three tokens inside `Sounds::load`'s fallback when `[Defaults]` has no `Type=`: `if (d.type.empty()) d.type = {"normal","screen","unshroud"};`. The header struct stays neutral.)

- [ ] **Step 4: Run** — `powershell -Command "D:\ra2-android\build\ra2assets_tests.exe -tc='sounds*'"` green (and the rules additions via `-tc='rules*'`); re-run the sounds real-pin case with `RA2_INSTALL_DIR='C:\nonexistent'` → clean skip.
- [ ] **Step 5: Commit** — `git commit -m "feat(assets): Sounds reader for soundmd.ini + Rules voice getters"`.

---

### Task 3: `ra2assets::load_wav_ima` (EVA RIFF decoder)

**Files:**
- Create: `include/ra2assets/wav.hpp`, `src/wav.cpp`, `src/ima_adpcm.hpp` (internal shared nibble math), `tests/test_wav.cpp`
- Modify: `src/audio_bag.cpp` (use the shared `src/ima_adpcm.hpp` instead of its private copy of the tables/cascade — mechanical extraction, byte-identical behavior), `cmake/ra2_sources.cmake` (`src/wav.cpp`), `CMakeLists.txt` (`tests/test_wav.cpp`)

**Interfaces:**
- Consumes: nothing new.
- Produces: `struct WavInfo { int sample_rate=0; int channels=1; int bits=16; };` and `std::vector<int16_t> load_wav_ima(const uint8_t* data, std::size_t len, WavInfo& out_info);` — throws `std::runtime_error` on non-RIFF, non-0x0011 fmt, or channels != 1 (all real EVA speech is mono; message points at docs/re/audio.md's stereo note).
- `src/ima_adpcm.hpp` (internal): `namespace ra2assets::ima { extern const int kStep[89]; extern const int kIndexAdjust[16]; int decode_nibble(int code, int& sample, int& index); }` — the exact cascade from docs/re/audio.md (step>>3 base, +step>>2/step>>1/step on bits 0–2, subtract on bit 3, clamp ±32767, index clamp 0..88).

- [ ] **Step 1: Write the failing tests** (`tests/test_wav.cpp`):

```cpp
#include "doctest.h"
#include "ra2assets/wav.hpp"
#include <cstring>
#include <vector>

using namespace ra2assets;

// Build a minimal in-memory fmt-0x0011 mono WAV: one 12-byte block
// (4-byte header {pred=0,idx=0,rsvd=0} + bytes {0x07,0x80} + 6 pad bytes of
// 0x00 nibbles), fact = 5 samples. The nibble stream 7,0,0,8 must decode to
// the docs/re/audio.md worked vector {0, 11, 13, 14, 13}; fact caps output
// at 5 so the pad nibbles are dropped.
static std::vector<uint8_t> tiny_ima_wav() {
    const uint16_t block_align = 12, channels = 1, tag = 0x0011, bits = 4;
    const uint32_t rate = 22050;
    const uint16_t samples_per_block = (block_align - 4) * 2 + 1;  // 17
    std::vector<uint8_t> v;
    auto u16 = [&](uint16_t x) { v.push_back(x & 0xff); v.push_back(x >> 8); };
    auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 0xff); };
    auto tag4 = [&](const char* t) { v.insert(v.end(), t, t + 4); };
    tag4("RIFF"); u32(4 + 26 + 12 + 20);  // riff payload size (WAVE + chunks)
    tag4("WAVE");
    tag4("fmt "); u32(20); u16(tag); u16(channels); u32(rate); u32(11100);
    u16(block_align); u16(bits); u16(2); u16(samples_per_block);
    tag4("fact"); u32(4); u32(5);        // 5 valid samples
    tag4("data"); u32(block_align);
    u16(0); v.push_back(0); v.push_back(0);          // block header pred=0 idx=0 rsvd=0
    v.push_back(0x07); v.push_back(0x80);            // nibbles 7,0 then 0,8
    for (int i = 0; i < 6; ++i) v.push_back(0x00);   // pad to block_align
    return v;
}

TEST_CASE("wav: tiny synthetic IMA block decodes to the documented vector") {
    std::vector<uint8_t> w = tiny_ima_wav();
    WavInfo info;
    std::vector<int16_t> pcm = load_wav_ima(w.data(), w.size(), info);
    CHECK(info.sample_rate == 22050);
    CHECK(info.channels == 1);
    REQUIRE(pcm.size() == 5);            // fact cap
    CHECK(pcm[0] == 0);                  // block-header predictor emitted first
    CHECK(pcm[1] == 11);
    CHECK(pcm[2] == 13);
    CHECK(pcm[3] == 14);
    CHECK(pcm[4] == 13);
}

TEST_CASE("wav: rejects non-IMA and stereo") {
    std::vector<uint8_t> w = tiny_ima_wav();
    w[20] = 0x01;                        // fmt tag -> 0x0001 PCM
    WavInfo info;
    CHECK_THROWS(load_wav_ima(w.data(), w.size(), info));
    std::vector<uint8_t> w2 = tiny_ima_wav();
    w2[22] = 2;                          // channels -> 2
    CHECK_THROWS(load_wav_ima(w2.data(), w2.size(), info));
    CHECK_THROWS(load_wav_ima(w2.data(), 8, info));  // truncated
}

TEST_CASE("wav: real ceva064 (EVA 'Unit lost') pins") {
    // Install guard (MESSAGE+return) exactly like tests/test_audio.cpp's
    // real-asset cases; then: language.mix -> audio.mix -> "ceva064.wav".
    // ...
    WavInfo info;
    std::vector<int16_t> pcm = load_wav_ima(bytes.data(), bytes.size(), info);
    CHECK(info.sample_rate == 22050);
    CHECK(info.channels == 1);
    CHECK(pcm.size() == 18559);          // fact chunk
    CHECK(pcm[0] == -13);                // first block predictor (hex f3 ff)
}
```

- [ ] **Step 2: Run to verify failure** — compile error (wav.hpp missing).

- [ ] **Step 3: Implement.** `src/ima_adpcm.hpp`: move the step/index tables + nibble cascade out of `src/audio_bag.cpp` verbatim (keep that file's behavior byte-identical — its oracle tests are the proof). `src/wav.cpp`:

```cpp
#include "ra2assets/wav.hpp"
#include "ima_adpcm.hpp"
#include <cstring>
#include <stdexcept>

namespace ra2assets {
namespace {
uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
}  // namespace

std::vector<int16_t> load_wav_ima(const uint8_t* data, std::size_t len, WavInfo& out_info) {
    if (len < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
        throw std::runtime_error("wav: not a RIFF WAVE");
    std::size_t pos = 12;
    uint16_t tag = 0, channels = 0, block_align = 0;
    uint32_t rate = 0, fact_samples = 0;
    const uint8_t* body = nullptr;
    uint32_t body_len = 0;
    bool have_fmt = false, have_fact = false;
    while (pos + 8 <= len) {
        const uint32_t csz = rd32(data + pos + 4);
        const uint8_t* c = data + pos + 8;
        if (pos + 8 + csz > len) throw std::runtime_error("wav: truncated chunk");
        if (!std::memcmp(data + pos, "fmt ", 4)) {
            if (csz < 16) throw std::runtime_error("wav: short fmt");
            tag = rd16(c); channels = rd16(c + 2); rate = rd32(c + 4);
            block_align = rd16(c + 12);
            have_fmt = true;
        } else if (!std::memcmp(data + pos, "fact", 4) && csz >= 4) {
            fact_samples = rd32(c);
            have_fact = true;
        } else if (!std::memcmp(data + pos, "data", 4)) {
            body = c; body_len = csz;
        }
        pos += 8 + csz + (csz & 1);  // chunks are word-aligned
    }
    if (!have_fmt || !body) throw std::runtime_error("wav: missing fmt/data");
    if (tag != 0x0011) throw std::runtime_error("wav: fmt tag not IMA-ADPCM (0x0011)");
    if (channels != 1)
        throw std::runtime_error("wav: only mono IMA supported (docs/re/audio.md stereo note)");
    if (block_align <= 4) throw std::runtime_error("wav: bad block align");

    std::vector<int16_t> pcm;
    for (uint32_t off = 0; off + 4 <= body_len; off += block_align) {
        const uint32_t blk = std::min<uint32_t>(block_align, body_len - off);
        const uint8_t* b = body + off;
        int sample = static_cast<int16_t>(rd16(b));   // predictor, emitted verbatim
        int index = b[2];
        if (index > 88) throw std::runtime_error("wav: bad block step index");
        pcm.push_back(static_cast<int16_t>(sample));
        for (uint32_t i = 4; i < blk; ++i) {
            pcm.push_back(static_cast<int16_t>(ima::decode_nibble(b[i] & 0xf, sample, index)));
            pcm.push_back(static_cast<int16_t>(ima::decode_nibble(b[i] >> 4, sample, index)));
        }
    }
    if (have_fact && fact_samples < pcm.size()) pcm.resize(fact_samples);
    out_info.sample_rate = static_cast<int>(rate);
    out_info.channels = 1;
    out_info.bits = 16;
    return pcm;
}
}  // namespace ra2assets
```

- [ ] **Step 4: Run** — `-tc='wav*'` green with install; skip-clean without. ALSO run `-tc='audio*'` (the BAG oracle suite) to prove the ima_adpcm.hpp extraction changed nothing.
- [ ] **Step 5: Commit** — `git commit -m "feat(assets): RIFF fmt-0x0011 IMA WAV decoder for EVA (shared nibble core with BAG)"`.

---

### Task 4: `engine/audio_mixer` — pure mixer

**Files:**
- Create: `engine/audio_mixer.hpp`, `engine/audio_mixer.cpp`, `tests/test_audio_mixer.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `engine/audio_mixer.cpp` beside `engine/combat.cpp`), `CMakeLists.txt` (add `tests/test_audio_mixer.cpp`)

**Interfaces:**
- Consumes: nothing from SDL/assets — pure. (Task 6 feeds it decoded PCM + `SoundEntry`-derived `MixerSound`s.)
- Produces (Task 6 relies on these EXACT names):

```cpp
// engine/audio_mixer.hpp  -- pure, 22050 Hz mono S16 throughout
struct MixerSound {
    std::vector<int> variants;   // SoundBank clip ids (>=1 to be playable)
    int volume = 80;             // 0..100
    int priority = 1;            // 0 low, 1 normal, 2 high, 3 critical
    int limit = 5;               // max simultaneous instances; <=0 unlimited
    bool interrupt = false;      // Control=interrupt
    bool global = false;         // Type contains GLOBAL: MinVolume floor (never silent)
    double range = 10.0;         // cells
    int min_volume = 50;
};
// AMENDED: real 5-level scale (docs/re/audio-playback.md, table @0x815018):
// lowest=0 low=1 normal=2 high=3 critical=4, unknown -> 2.
int priority_from_string(const std::string& p);
class SoundBank {
public:
    int add(std::vector<int16_t> pcm);           // -> clip id
    const std::vector<int16_t>& pcm(int id) const;
    int count() const;
};
class Mixer {
public:
    static constexpr int kMaxVoices = 16;
    Mixer(const SoundBank& bank, uint32_t seed);
    void register_sound(const std::string& key, const MixerSound& s);
    bool play(const std::string& key, double dist_cells = 0.0);  // false: rejected/unknown
    void play_eva(int clip_id);                   // queued, one at a time, full volume
    void render(int16_t* out, std::size_t frames);  // sum+clamp, advances state
    int active_voices() const;                    // non-EVA voices playing
    bool eva_active() const;
    std::size_t eva_queued() const;               // waiting behind the active one
    // CONFIRMED linear falloff (docs/re/audio-playback.md, FUN_0074f8e0,
    // High): gain = 1 - dist/range, full at 0, silent at range. Type=GLOBAL
    // events are floored at min_volume (never silent) instead. The caller
    // supplies dist in CELLS (the game divides its pixel metric by 60 --
    // Task 6 computes the exact Chebyshev metric). Returns 0..1000.
    static int gain_permille(const MixerSound& s, double dist_cells);
};
```

- [ ] **Step 1: Write the failing tests** (`tests/test_audio_mixer.cpp`) — hand-computed vectors:

```cpp
#include "doctest.h"
#include "audio_mixer.hpp"
#include <vector>

using namespace ra2engine;

namespace {
// bank with two tiny clips: A = {1000,1000,1000,1000}, B = {-300,-300}
SoundBank two_clip_bank(int& a, int& b) {
    SoundBank bank;
    a = bank.add({1000, 1000, 1000, 1000});
    b = bank.add({-300, -300});
    return bank;
}
MixerSound plain(int clip, int volume = 100) {
    MixerSound s;
    s.variants = {clip};
    s.volume = volume;
    s.limit = 0;
    return s;   // plays at dist 0 -> full gain
}
}  // namespace

TEST_CASE("mixer: two overlapping clips sum sample-by-sample") {
    int a, b;
    SoundBank bank = two_clip_bank(a, b);
    Mixer m(bank, 1);
    m.register_sound("A", plain(a));
    m.register_sound("B", plain(b));
    CHECK(m.play("A"));
    CHECK(m.play("B"));
    int16_t out[6] = {};
    m.render(out, 6);
    CHECK(out[0] == 700);   // 1000 - 300
    CHECK(out[1] == 700);
    CHECK(out[2] == 1000);  // B finished
    CHECK(out[3] == 1000);
    CHECK(out[4] == 0);     // both finished
    CHECK(m.active_voices() == 0);
}

TEST_CASE("mixer: summing clamps at int16 extremes") {
    SoundBank bank;
    int loud = bank.add({30000, -30000});
    Mixer m(bank, 1);
    m.register_sound("L", plain(loud));
    CHECK(m.play("L"));
    CHECK(m.play("L"));
    int16_t out[2] = {};
    m.render(out, 2);
    CHECK(out[0] == 32767);    // 60000 clamped
    CHECK(out[1] == -32768);   // -60000 clamped
}

TEST_CASE("mixer: volume scales linearly (permille math)") {
    SoundBank bank;
    int c = bank.add({10000});
    Mixer m(bank, 1);
    m.register_sound("H", plain(c, 65));   // GIAttack's real volume
    CHECK(m.play("H"));
    int16_t out[1] = {};
    m.render(out, 1);
    CHECK(out[0] == 6500);
}

TEST_CASE("mixer: Limit rejects the (limit+1)-th instance; interrupt steals instead") {
    SoundBank bank;
    int c = bank.add(std::vector<int16_t>(100, 100));
    Mixer m(bank, 1);
    MixerSound s = plain(c);
    s.limit = 3;                       // ConscriptAttack's real limit
    m.register_sound("S", s);
    CHECK(m.play("S")); CHECK(m.play("S")); CHECK(m.play("S"));
    CHECK_FALSE(m.play("S"));          // 4th rejected  [RE-FALLBACK]
    CHECK(m.active_voices() == 3);
    MixerSound si = s;
    si.interrupt = true;
    Mixer m2(bank, 1);
    m2.register_sound("S", si);
    CHECK(m2.play("S")); CHECK(m2.play("S")); CHECK(m2.play("S"));
    CHECK(m2.play("S"));               // interrupt: steals oldest same-key  [RE-FALLBACK]
    CHECK(m2.active_voices() == 3);
}

TEST_CASE("mixer: full voice pool DROPS the new sound (vanilla: no priority steal)") {
    // docs/re/audio-playback.md FUN_00405190: the SFX allocator reclaims
    // finished voices only, then fails -- it never evicts a playing voice.
    SoundBank bank;
    int c = bank.add(std::vector<int16_t>(100, 100));
    Mixer m(bank, 1);
    MixerSound s = plain(c);
    s.priority = 4;                    // even critical does not steal
    m.register_sound("S", s);
    for (int i = 0; i < Mixer::kMaxVoices; ++i) REQUIRE(m.play("S"));
    CHECK_FALSE(m.play("S"));          // pool full -> dropped
    CHECK(m.active_voices() == Mixer::kMaxVoices);
}

TEST_CASE("mixer: priority_from_string uses the real 5-level scale") {
    CHECK(priority_from_string("lowest") == 0);
    CHECK(priority_from_string("low") == 1);
    CHECK(priority_from_string("normal") == 2);
    CHECK(priority_from_string("high") == 3);
    CHECK(priority_from_string("critical") == 4);
    CHECK(priority_from_string("bogus") == 2);
}

TEST_CASE("mixer: attenuation linear (CONFIRMED) with GLOBAL MinVolume floor") {
    MixerSound s;
    s.volume = 100; s.range = 10.0; s.min_volume = 50;
    CHECK(Mixer::gain_permille(s, 0.0) == 1000);
    CHECK(Mixer::gain_permille(s, 5.0) == 500);
    CHECK(Mixer::gain_permille(s, 10.0) == 0);
    CHECK(Mixer::gain_permille(s, 25.0) == 0);
    s.global = true;                   // Type=GLOBAL: floored, never silent
    CHECK(Mixer::gain_permille(s, 25.0) == 500);   // max(gain, min_volume*10)
    CHECK(Mixer::gain_permille(s, 0.0) == 1000);   // floor never raises full
}

TEST_CASE("mixer: seeded variant selection is deterministic") {
    SoundBank bank;
    int c0 = bank.add({1});
    int c1 = bank.add({2});
    int c2 = bank.add({3});
    MixerSound s;
    s.variants = {c0, c1, c2};
    s.positional = false; s.limit = 0; s.volume = 100;
    Mixer a(bank, 42), b(bank, 42);
    a.register_sound("V", s);
    b.register_sound("V", s);
    for (int i = 0; i < 8; ++i) {
        REQUIRE(a.play("V")); REQUIRE(b.play("V"));
        int16_t oa[1] = {}, ob[1] = {};
        a.render(oa, 1); b.render(ob, 1);
        CHECK(oa[0] == ob[0]);   // same seed -> same variant sequence
    }
}

TEST_CASE("mixer: EVA queues one-at-a-time in FIFO order") {
    SoundBank bank;
    int e1 = bank.add({11, 11});
    int e2 = bank.add({22, 22});
    Mixer m(bank, 1);
    m.play_eva(e1);
    m.play_eva(e2);
    CHECK(m.eva_active());
    CHECK(m.eva_queued() == 1);
    int16_t out[2] = {};
    m.render(out, 2);
    CHECK(out[0] == 11);           // e1 plays alone first
    m.render(out, 2);
    CHECK(out[0] == 22);           // then e2
    CHECK(m.eva_queued() == 0);
    m.render(out, 2);
    CHECK_FALSE(m.eva_active());
    CHECK(out[0] == 0);
}

TEST_CASE("mixer: unknown key or empty variants -> false, no crash") {
    SoundBank bank;
    Mixer m(bank, 1);
    CHECK_FALSE(m.play("nope"));
    MixerSound empty;
    m.register_sound("E", empty);
    CHECK_FALSE(m.play("E"));
}
```

- [ ] **Step 2: Run to verify failure** — compile error.

- [ ] **Step 3: Implement** (`engine/audio_mixer.cpp`). Core shapes:

```cpp
#include "audio_mixer.hpp"
#include <algorithm>

namespace ra2engine {

int priority_from_string(const std::string& p) {
    if (p == "low") return 0;
    if (p == "high") return 2;
    if (p == "critical") return 3;
    return 1;
}

int SoundBank::add(std::vector<int16_t> pcm) {
    clips_.push_back(std::move(pcm));
    return static_cast<int>(clips_.size()) - 1;
}
// pcm(): bounds-checked at(); count(): size.

// CONFIRMED linear falloff (docs/re/audio-playback.md FUN_0074f8e0, High):
// gain = 1 - dist/range; Type=GLOBAL floors the result at MinVolume
// (max(vol, MinVolume) branch is GLOBAL-only in gamemd; MinVolume is
// ignored for every other Type).
int Mixer::gain_permille(const MixerSound& s, double dist_cells) {
    int g = 1000;
    if (s.range > 0.0) {
        const double f = 1.0 - dist_cells / s.range;
        g = f <= 0.0 ? 0 : (f >= 1.0 ? 1000 : static_cast<int>(f * 1000.0));
    }
    if (s.global) g = std::max(g, s.min_volume * 10);
    return g;
}

// xorshift32 -- audio-local, never the sim's state.
uint32_t Mixer::next_rand_() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }

bool Mixer::play(const std::string& key, double dist_cells) {
    auto it = sounds_.find(key);
    if (it == sounds_.end() || it->second.variants.empty()) return false;
    const MixerSound& s = it->second;
    const int gain = gain_permille(s, dist_cells);
    if (gain <= 0) return false;                       // inaudible: don't burn a voice
    int same = 0;
    for (const Voice& v : voices_)
        if (v.key == key) ++same;
    if (s.limit > 0 && same >= s.limit) {
        // Reject-at-trigger: documented approximation of vanilla's
        // start-then-cull-to-cap -- both hold the <=Limit invariant
        // (docs/re/audio-playback.md FUN_004041d0, Decisions).
        if (!s.interrupt) return false;
        kill_oldest_with_key_(key);   // interrupt: steal same-event channel (Medium)
    } else if (static_cast<int>(voices_.size()) >= kMaxVoices) {
        // Vanilla drops the new sound when the pool is full -- it never
        // evicts a playing voice (docs/re/audio-playback.md FUN_00405190).
        return false;
    }
    const int clip = s.variants[next_rand_() % s.variants.size()];
    voices_.push_back({key, clip, 0, s.volume * gain, s.priority, age_counter_++});
    return true;
}
// render(): int32 acc per frame: for each voice acc += pcm[pos] * (volume_x_gain) / 100000;
// plus the EVA slot (gain 1000, volume 100 -> pcm as-is); clamp to
// [-32768,32767]; ++pos; erase finished voices; pop EVA queue when the
// active EVA finishes.
}  // namespace ra2engine
```

  The header carries `struct Voice { std::string key; int clip; std::size_t pos; int vol_x_gain; uint64_t age; };`, `std::map<std::string, MixerSound> sounds_;`, `std::vector<Voice> voices_;`, `int eva_clip_ = -1; std::size_t eva_pos_ = 0; std::deque<int> eva_queue_;`, `uint32_t rng_; uint64_t age_counter_ = 0;`. Scaling: `vol_x_gain = s.volume * gain` (0..100000); per-sample contribution `pcm * vol_x_gain / 100000` in int64/int32 — the volume-65 test pins `10000*65*1000/100000 = 6500` exactly. Steal rule: `kill_oldest_with_key_` removes the same-key voice with the smallest `age` (interrupt only; the full pool never steals — vanilla drops).

- [ ] **Step 4: Run** — `-tc='mixer*'` green; then the FULL suite once (new shared sources touch cmake).
- [ ] **Step 5: Commit** — `git commit -m "feat(engine): pure audio mixer (limit/priority/attenuation/variants/EVA queue)"`.

---

### Task 5: `Input` action feedback (voice-ack hooks)

**Files:**
- Modify: `engine/input.hpp` (InputActions struct + accessor), `engine/input.cpp` (fill it in commit_click/commit_box/commit_double and the right-click handler), `tests/test_input.cpp` (new cases)

**Interfaces:**
- Consumes: existing Input internals (commit_click/commit_box/commit_double, right-click handling in `handle()`).
- Produces (Task 6 relies on):

```cpp
struct InputActions {
    bool selected = false;       // a commit produced a non-empty selection
    bool move_ordered = false;   // right-click group move issued
    bool attack_ordered = false; // right-click attack issued
    int actor = -1;              // representative unit index (lowest selected)
};
const InputActions& last_actions() const;  // state produced by the MOST RECENT handle()
```

- [ ] **Step 1: Write the failing tests** (append to `tests/test_input.cpp`, using its existing World/Input fixture and synthetic-event helpers — follow the file's current click/box/attack cases):
  - click-select a unit → `last_actions().selected == true`, `actor` == that unit's index; click empty ground → all false, actor −1.
  - right-click ground with a selection → `move_ordered == true`, actor == lowest selected index; `selected == false`.
  - right-click a hostile with a selection → `attack_ordered == true`.
  - each `handle()` RESETS the struct: after the select-click case, feeding a mouse-move event yields all-false.
  - box-select over two units → `selected == true`, actor == lowest boxed index.

- [ ] **Step 2: Run to verify failure** — `-tc='input*'`: new cases fail to compile (no last_actions).
- [ ] **Step 3: Implement** — `InputActions actions_;` member; `handle()` starts with `actions_ = {};`; `commit_click/commit_box/commit_double` set `selected=true` + lowest selected index when their commit results in ≥1 selected unit; the right-click branch sets `move_ordered`/`attack_ordered` + actor (lowest selected, BEFORE the order mutates anything) only when the selection is non-empty (matching the existing guards). No behavior change to selection/orders themselves.
- [ ] **Step 4: Run** — `-tc='input*'` green; full suite once.
- [ ] **Step 5: Commit** — `git commit -m "feat(engine): Input reports select/move/attack actions (audio ack hooks)"`.

---

### Task 6: SDL sink + game wiring + demos

**Files:**
- Create: `engine/audio_sdl.hpp`, `engine/audio_sdl.cpp`
- Modify: `engine/main.cpp` (audio load + AudioSystem + event collection + acks + `--no-audio` + `--demo-audio`), desktop engine target in `CMakeLists.txt` + Android app jni sources (add `engine/audio_sdl.cpp` wherever `engine/main.cpp`'s SDL companions are listed — NOT the test target, NOT `cmake/ra2_sources.cmake`)

**Interfaces:**
- Consumes: `Sounds`/`SoundEntry` + Rules voice getters (Task 2), `load_wav_ima` (Task 3), `SoundBank`/`Mixer`/`MixerSound`/`priority_from_string` (Task 4), `Input::last_actions()` (Task 5), `load_ini_from_ra2md(dir, name)` (existing, engine/main.cpp), `AudioIdx`/`decode_bag_clip` (existing), the World accessors (`units()`, camera center already used by render_world).
- Produces: the audible game. No new interfaces consumed later.

- [ ] **Step 1: `engine/audio_sdl.{hpp,cpp}`** — queue-push sink, no callback/locks:

```cpp
// audio_sdl.hpp
#pragma once
#include <cstdint>
namespace ra2engine {
class Mixer;
// Thin SDL audio sink: opens the default device at 22050 Hz mono S16 and
// keeps ~kTargetQueuedMs of mixed audio queued via SDL_QueueAudio (pumped
// once per frame from the main loop -- no audio-thread callback, no locks).
class AudioOutput {
public:
    bool open();                 // false: no device (game continues silent)
    void pump(Mixer& mixer);     // render+queue until the target depth is met
    void close();
    bool ok() const { return dev_ != 0; }
private:
    static constexpr int kTargetQueuedMs = 200;
    static constexpr int kChunkFrames = 1024;
    uint32_t dev_ = 0;
};
}  // namespace ra2engine
```

  `open()`: `SDL_InitSubSystem(SDL_INIT_AUDIO)` + `SDL_OpenAudioDevice(nullptr,0,&want,&have,0)` with want = {22050, AUDIO_S16SYS, 1 channel, 1024 samples}, require exact format (`allowed_changes=0`); unpause. `pump()`: `while (SDL_GetQueuedAudioSize(dev_) < 22050*2*kTargetQueuedMs/1000) { int16_t buf[kChunkFrames]; mixer.render(buf, kChunkFrames); SDL_QueueAudio(dev_, buf, sizeof buf); }`.

- [ ] **Step 2: main.cpp — AudioSystem assembly.** New struct + loader (near UnitAssets):

```cpp
struct UnitVoices { std::string select, move, attack, die, report; };
struct AudioSystem {
    ra2engine::SoundBank bank;
    std::unique_ptr<ra2engine::Mixer> mixer;
    ra2engine::AudioOutput out;
    std::map<std::string, UnitVoices> voices;  // by image ("GI","CONS")
    int eva_unitlost = -1;                     // bank clip id, -1 if unavailable
    bool enabled = false;
};
```

  Loader steps (one function `init_audio(AudioSystem&, const std::string& dir, const Rules& rules)` called by the interactive mode entry — NOT by --demo-combat):
  1. `Ini snd_ini = load_ini_from_ra2md(dir, "soundmd.ini"); Sounds sounds = Sounds::load(snd_ini);`
  2. Open `language.mix → audio.mix`, load `audio.idx` bytes → `AudioIdx`, slurp `audio.bag` bytes (this is the exact chain in tests/test_audio.cpp:305–311).
  3. For each unit id {E1, E2}: fill `audio.voices[rules.image(id)] = UnitVoices{rules.voice_select(id), rules.voice_move(id), rules.voice_attack(id), rules.die_sound(id), rules.weapon_report(rules.primary(id))}` (note: Report is on the WEAPON). Then for each of those five keys: for each KEY: `SoundEntry e = sounds.entry(key)`; for each variant name: `AudioIdx::find` → `decode_bag_clip` → `bank.add`, collect ids; build `MixerSound{ids, e.volume, priority_from_string(e.priority), e.limit, /*interrupt=*/ e.control contains "interrupt", /*global=*/ e.type contains "global", e.range, e.min_volume}`; `mixer->register_sound(key, ...)`. AMENDED per docs/re/audio-playback.md: EVERY event attenuates by distance (voice acks included — they inherit `[Defaults]` `Type=SCREEN` and are positional; they just usually play near the view centre); `GLOBAL` only adds the MinVolume floor inside `gain_permille`. A missing idx name → `std::cerr << "[audio] missing clip ...\n"` once, skip variant; a key with zero resolved variants stays unregistered (play() returns false silently).
  4. EVA: read `"ceva064.wav"` from the same audio.mix → `load_wav_ima` → `bank.add` → `eva_unitlost`. On any exception: log `[audio] EVA unavailable: <what>` and leave −1.
  5. `out.open()`; `enabled = out.ok() && !no_audio_flag;` log one `[audio] device=<ok/none> clips=<bank.count()>` line.

- [ ] **Step 3: main.cpp — event collection in the interactive loop.** Keep a `std::vector<std::pair<double,double>> prev_audio_state;` (fire_flash, die_time per unit). After `world.step(dt)` each frame:

```cpp
if (audio.enabled) {
    const auto& units = world.units();
    prev_audio_state.resize(units.size(), {0.0, -1.0});
    for (std::size_t i = 0; i < units.size(); ++i) {
        const auto& u = units[i];
        // Game metric (docs/re/audio-playback.md, High): Chebyshev
        // max(|dx|, 2*|dy|) in SCREEN pixels from the view centre, /60 to
        // cells (the game multiplies Range by 60 px/cell; dividing here
        // keeps Mixer::gain_permille in cell units). dx/dy from the same
        // iso projection the renderer uses: sx=(fx-fy)*30, sy=(fx+fy)*15.
        const double sx = (u.fx - u.fy) * 30.0 - view_center_px_x;
        const double sy = (u.fx + u.fy) * 15.0 - view_center_px_y;
        const double dist = std::max(std::abs(sx), 2.0 * std::abs(sy)) / 60.0;
        const auto& v = audio.voices[u.image];
        if (u.fire_flash > prev_audio_state[i].first)          // rising edge: fired
            audio.mixer->play(v.report, dist);
        if (u.die_time >= 0.0 && prev_audio_state[i].second < 0.0) {  // started dying
            audio.mixer->play(v.die, dist);
            if (u.house == 0 && audio.eva_unitlost >= 0)
                audio.mixer->play_eva(audio.eva_unitlost);
        }
        prev_audio_state[i] = {u.fire_flash, u.die_time};
    }
    const auto& act = input.last_actions();   // after the event-poll loop
    if (act.actor >= 0) {
        const auto& av = audio.voices[world.units()[act.actor].image];
        if (act.selected) audio.mixer->play(av.select);
        else if (act.attack_ordered) audio.mixer->play(av.attack);
        else if (act.move_ordered) audio.mixer->play(av.move);
    }
    audio.out.pump(*audio.mixer);
}
```

  `view_center_px_x/y`: the camera's centre in the same world-pixel space as the projection above (camera offset + half the viewport — exactly the values render_world already uses to place tiles; lift them from there).
  Note ack precedence: `handle()` produces at most one action per event, but a click that both selects and… cannot happen — the three flags are mutually exclusive per event by construction (Task 5).

- [ ] **Step 4: `--no-audio` + `--demo-audio`.** `--no-audio`: skip init_audio entirely. `--demo-audio`: init assets + audio (no window), then scripted proof:

```cpp
// mode_demo_audio(): load rules+audio as above. Then:
// 1) deviceless render proof: play "GIAttack", render 22050 frames into a
//    buffer, FAIL (exit 1) if every sample is zero.
// 2) play sequence with real pacing IF a device opened: GIAttack ->
//    GISelect -> GIDie -> EVA; pump ~4s of wall time so it is audible.
// 3) print "[demo-audio] clips=<n> device=<yes/no> mix=<nonzero>" and exit 0
//    (device absence is NOT a failure -- CI/headless boxes).
```

  `--demo-combat` is untouched and must NOT call init_audio (its byte-identity gate then can't be affected by audio at all).

- [ ] **Step 5: Verify live.** Build; `powershell`: run `ra2engine.exe --demo-audio` (expect the [audio]/[demo-audio] lines, exit 0, and hear M60 fire → "yes sir" → scream → "Unit lost" if a device exists); run `ra2engine.exe --demo-combat` → still exit 0 byte-identical; run the interactive game and click/move/attack for a manual smoke test.
- [ ] **Step 6: Run suites** — full, both modes (with/without install).
- [ ] **Step 7: Commit** — `git commit -m "feat(engine): SDL audio sink + game audio wiring (reports, acks, deaths, EVA)"`.

---

### Task 7: Finalize — suites, arm64, README, carry-forwards

**Files:**
- Modify: `README.md` (audio paragraph), plus verification only.

- [ ] **Step 1: Full suite** with install (expect prior count + new sounds/wav/mixer/input cases, all green) and without install (clean skips) — PowerShell.
- [ ] **Step 2: `--demo-combat`** — byte-identical, exit 0 (unchanged from pre-phase).
- [ ] **Step 3: arm64 gate** — NDK r27c build (command recorded in `.superpowers/sdd/progress.md`); confirms `src/sounds.cpp`, `src/wav.cpp`, `engine/audio_mixer.cpp` compile for Android and `engine/audio_sdl.cpp` links in the app target.
- [ ] **Step 4: README** — extend the engine feature list: sound effects, unit voices, and EVA driven by the real soundmd.ini/evamd.ini + audio.bag via the in-house decoders; playback semantics per `docs/re/audio-playback.md`; `--demo-audio`, `--no-audio` documented.
- [ ] **Step 5: Commit** — `git commit -m "feat(engine): audio playback phase finalized (suites, arm64, README)"`.
