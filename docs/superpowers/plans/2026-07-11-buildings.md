# Buildings & Construction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Allied and Soviet core building sets (8 structures) rendered from real SHP art with multi-cell foundations, plus the full ConYard construction loop — real RA2 sidebar (credits, power meter, tabs, cameo grid), BuildTime countdown from a fixed credit pool, placement cursor with validity feedback, buildup anims, and a power grid with a low-power build penalty.

**Architecture:** Task 1 recovers rendering + construction semantics from gamemd.exe (Ghidra, content-anchored) into two RE docs. Then: ra2assets building accessors + a pure theater-name resolver → a pure `engine/building` module (foundations, validity, frame selection) → World/combat integration (placement, passability, attack-building) → a pure `engine/production` module (queue/credits/power) → a pure `engine/sidebar` layout module → `main.cpp`/`input.cpp` wiring + `--demo-base`.

**Tech Stack:** C++17, CMake+Ninja+MinGW (host), doctest, SDL2 (engine only), Ghidra headless at `D:\ra2-re`.

## Global Constraints

- GPL-clean: XCC/OpenRA/YRpp/ModEnc are read OFF-REPO (`D:\ra2-re\reference`) and only prose conclusions may be committed. No GPL code in the repo. Numeric constants extracted from the binary are factual data and MAY be committed.
- No game assets, no `gamemd.exe`, no Ghidra project files committed. RE docs are prose + short decompiler excerpts.
- Tests must skip cleanly (doctest `MESSAGE` + `return`) when `RA2_INSTALL_DIR` is unset AND the default `D:\Red Alert 2` is absent (pattern: `tests/test_vpl.cpp`'s guarded real-file test).
- Pure modules (no SDL/OS) are compiled into BOTH `ra2assets_tests` and `ra2engine`: library sources go in `cmake/ra2_sources.cmake` `RA2_LIB_SOURCES`; engine-pure sources are added to both targets (pattern: `engine/combat.cpp` in `CMakeLists.txt` + `RA2_ENGINE_SOURCES`).
- Run test/demo executables from **PowerShell**, never Git Bash.
- Work directly on `master`; commit after every test+impl step-pair; every commit message ends with `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- **`--demo-combat` AND `--demo-tank` byte-identity must remain unchanged by every task** (no buildings spawn there; infantry/vehicle code paths' behavior untouched).
- Sim determinism: nothing in this phase introduces RNG into the sim.
- Build dir: `D:\ra2-android\build`. Rebuild: `cmake --build build` from repo root. Full suite: `.\build\ra2assets_tests.exe` from PowerShell.
- Ghidra/gradle need `JAVA_HOME=C:\Program Files\Java\jdk-21`.
- Scratch files (screenshots, demo outputs) go to the session scratchpad, never the repo.

## Ground truth (extracted from the real install this session — cite, don't re-derive)

rulesmd.ini:

| id | Strength | Armor | TechLevel | Adjacent | Cost | Power | Prerequisite | notes |
|---|---|---|---|---|---|---|---|---|
| GACNST | 1000 | concrete | -1 | 2 | 3000 | 0 | — | ConstructionYard=yes, Factory=BuildingType |
| GAPOWR | 750 | wood | 1 | 2 | 800 | **+200** | GACNST | BuildCat=Power |
| GAPILE | 500 | steel | 2 | 2 | 500 | −10 | POWER,GACNST | Factory=InfantryType |
| GAWEAP | 1000 | wood | 2 | 2 | 2000 | −25 | PROC,GAPILE,GACNST | WeaponsFactory=yes, Bib=yes |
| NACNST | 1000 | concrete | -1 | 2 | 3000 | 0 | — | ConstructionYard=yes |
| NAPOWR | 750 | wood | 1 | 2 | 600 | **+150** | NACNST | |
| NAHAND | 500 | steel | 2 | 2 | 500 | −10 | POWER,NACNST | |
| NAWEAP | 1000 | wood | 2 | 2 | 2000 | −25 | PROC,NAHAND,NACNST | |

All eight have `Explosion=TWLT070,...` (first entry TWLT070). [General]: `BuildSpeed=.7`, `RefundPercent=50%`, `LowPowerPenaltyModifier=1`, `ConditionYellow=50%`, `ConditionRed=25%`; prerequisite groups `PrerequisitePower=GAPOWR,NAPOWR,NANRCT,YAPOWR`, `PrerequisiteFactory=GAWEAP,NAWEAP,YAWEAP`, `PrerequisiteBarracks=NAHAND,GAPILE,YABRCK`, `PrerequisiteRadar=...`, `PrerequisiteTech=...`, `PrerequisiteProc=GAREFN,NAREFN,YAREFN`. `[MultiplayerDialogSettings] Money=10000`.

artmd.ini (all eight: `Remapable=yes`, `NewTheater=yes`):

| id | Foundation | Buildup | Cameo |
|---|---|---|---|
| GACNST | 4x4 | GACNSTMK | (none — never buildable) |
| GAPOWR | 2x2 | GAPOWRMK | POWRICON |
| GAPILE | 3x2 | GAPILEMK | BRRKICON |
| GAWEAP | 5x3 | GAWEAPMK | gwepicon |
| NACNST | 4x4 | NACNSTMK | (none) |
| NAPOWR | 3x2 | NAPOWRMK | NPWRICON |
| NAHAND | 2x2 | NAHANDMK | HANDICON |
| NAWEAP | 5x3 | NAWEAPMK | NWEPICON |

Verified asset locations (ra2extract probes this session):
- Temperate main building art is the **generic-'G' fallback**: `ra2.mix/generic.mix/ggcnst.shp` (63,760 B), `ggpowr.shp` (16,416 B), `ggweap.shp` (43,008 B), `ggpile.shp` (25,712 B), `ggrefn.shp` (27,328 B), `ggcnst_a.shp` (6,344 B). **No `gtcnst.shp` exists anywhere.**
- Snow-specific art (theater letter 'A'): `ra2.mix/snow.mix/gacnst.shp` (63,736 B), `gapowr.shp` (16,416 B), `gapile.shp` (25,712 B).
- Buildup anims ARE theater-specific: `ra2.mix/isotemp.mix/gtcnstmk.shp` (428,080 B).
- Sidebar chrome: `ra2.mix/sidec01.mix` (Allied) and `sidec02.mix` (Soviet), both contain `top.shp`, `side1.shp`, `side2.shp`, `tab00.shp`, `tab01.shp`, `powerp.shp`, `credits.shp`.
- Cameos: `language.mix/cameo.mix/powricon.shp` (2,912 B), `brrkicon.shp`. Palettes: `ra2.mix/cache.mix/cameo.pal` (768 B) and `unittem.pal` both verified present.

## [RE-GATE] Plan amendment gate (same mechanism as the audio + voxel phases)

Tasks 2–7 contain constants and semantic choices marked `[RE: building-render.md §N]` or `[RE: construction.md §N]`. They are written from pre-hunt assumptions so the plan is complete, but **the controller MUST reconcile them against Task 1's RE docs and commit plan amendments BEFORE dispatching each affected task**. If the hunt contradicts an assumption, the plan text is amended (and committed) first; the implementer never sees the stale version. Items the hunt cannot resolve get an `[RE-FALLBACK]` tag in the RE doc naming the off-repo source used.

**[RE-GATE STATUS: RECONCILED 2026-07-11 against 5ac594c.]** Amended (marked
`[RE-GATE APPLIED]` in place): Task 2 theater resolver (pattern-gated munge,
[themed, 'g'] names × 3-mix walk, no literal 4th candidate); Task 3
buildup_frame (period-stepped, trunc(time/period)); Task 5
build_speed_multiplier (clamp form, .5 floor/.8 low-cap) + the 54-step
FactoryClass economy (QueueSlot stage/step_ticks, stall-and-retry, no
catch-up burst); Task 6 SidebarArtDims (real art sizes, four 28×27 tab SHPs
at bar.w/4 pitch); Task 7 (buildup_total from BuildupTime×900, center–center
canvas registration, (f−1)/2 depth key, gclock2.shp build clock, §8 power-bar
pip model, sidebar art list + gclock2 + sidebar.pal probe); Task 8 README
divergence additions. Remaining `[RE: …§N]` markers were CONFIRMED as
written by the hunt (foundation origin/min-corner §2, expanded-rect
adjacency ⇒ gap ≤ Adjacent §9, frame-0/1 + shadow-half §4, build_frames
trunc formula §7 with 504/1890 pins, Buildup-contributes-nothing §8,
ANDed prereq tokens + groups §10, 168-px sidebar structure §11) — no text
change needed; implementers treat them as recovered fact, citing the docs.

---

### Task 1: Ghidra buildings hunt → docs/re/building-render.md + docs/re/construction.md

**Files:**
- Create: `docs/re/building-render.md`, `docs/re/construction.md` (the ONLY repo deliverables)
- Create (off-repo, not committed): `D:\ra2-re\scripts\building_hunt*.py`

**Interfaces:**
- Produces: two RE docs with numbered sections (below), a confidence row per item, and a **"Decisions for ra2engine"** section in EACH doc explicitly resolving every `[RE: …§N]` marker in Tasks 2–7 (grep this plan for `[RE:` before writing).

**Context.** `D:\ra2-re` holds a Ghidra 11 project (`ra2.gpr`) over YR 1.001 `gamemd.exe`, with `analyzeHeadless.bat` workflow scripts from prior hunts in `D:\ra2-re\scripts` (see `audio_hunt*.py` / `voxel_render_hunt*.py` for the pattern: block-read string search → `getReferencesTo` → targeted `decompile` → dump to `D:\ra2-re\out`). Content-anchor ONLY — YRpp/Ares hardcoded addresses are unreliable on this binary. Set `JAVA_HOME=C:\Program Files\Java\jdk-21` before running. Useful string anchors present in the binary: `"BuildSpeed"`, `"LowPowerPenaltyModifier"`, `"Adjacent"`, `"Foundation"`, `"Buildup"`, theater mix names (`"generic.mix"`, `"isotemp.mix"`, `"snow.mix"`), sidebar art names (`"side1.shp"`, `"tab00.shp"`, `"powerp.shp"`, `"credits.shp"`), `"cameo.pal"`, `"unittem.pal"`, and CSF label `"TXT_..."` sidebar strings. The verified on-disk layout in "Ground truth" above is the empirical check every §1 conclusion must explain.

**Recover into `docs/re/building-render.md`, with a confidence row per item:**

- **§1 NewTheater resolution.** The filename transform for `NewTheater=yes` art: which character position is substituted, the theater letter table (expected T=temperate, A=snow, U=urban), the fallback order (expected: theater letter → 'G' generic → literal), and WHICH mixes are searched for building SHPs vs buildup SHPs (empirical: main art in `generic.mix`/`snow.mix`, buildups in `isotemp.mix`). Deliverable: an exact candidate-list algorithm the Task 2 resolver implements verbatim, one that explains why `ggcnst.shp` exists but `gtcnst.shp` does not.
- **§2 Foundation semantics.** How `Foundation=WxH` maps to occupied cells relative to the placement/origin cell in the map's cell space (which corner is the origin), and the SHP canvas registration: where frame (0,0) is anchored relative to the foundation's cells at the 60×30 (our 30-half-width) cell metric — the exact pixel offset formula `screen(px,py) = f(origin_cell, canvas_w, canvas_h, fw, fh)`.
- **§3 Draw order.** How a multi-cell building sorts against units for painter's order — the sort key the original uses (foundation center? front edge? per the render list insertion), so buildings slot into `unit_draw_order`'s `(fx+fy)` depth space without seams. Also whether ZAdjust/YSort art keys matter for the MAIN building image (we render no ActiveAnims this phase).
- **§4 Damage states + frame layout.** The main building SHP frame layout (how many image frames, whether the second half is shadow frames like unit SHPs, which frame is "damaged"), the HP threshold that switches to damaged art (expected ConditionYellow=50%), and what renders below ConditionRed. Empirical check: `ggcnst.shp`'s actual frame count/canvas via ra2extract.
- **§5 Buildup anim.** The MK SHP's playback rate (frames per 15 Hz logic frame), whether it plays once fully before the building becomes operational, its frame layout (shadow half?), and the placement sound (expected `[AudioVisual]` BuildingSlam or per-building key — name the soundmd entry).
- **§6 Palette + remap.** Confirm buildings draw with the theater unit palette (`unittem.pal` from `ra2.mix/cache.mix`, as SHP units/vehicles do) and house remap = palette indices 16–31 (the existing `pal_for_house` scheme); confirm cameos draw with `cameo.pal`.

**Recover into `docs/re/construction.md`, with a confidence row per item:**

- **§7 BuildTime math.** The Cost → build duration formula. Pre-hunt expectation from the BuildSpeed comment ("time in minutes to produce a 1000 credit cost item"): `frames = Cost/1000 × BuildSpeed × 900` at 15 Hz (GAPOWR 800 → 504 frames = 33.6 s). Confirm the 900-frames-per-minute constant, truncation/rounding, and whether BuildTime= overrides exist for our 8 (none have the key — confirm the Cost fallback path).
- **§8 Power model + low-power penalty.** Output vs drain accumulation; the low-power condition (drain > output); the penalty applied to BUILDING production speed when low (`LowPowerPenaltyModifier=1` — recover the base penalty formula it modifies, e.g. `penalty = shortage_ratio × modifier`, and the resulting speed multiplier); the sidebar power-bar thresholds (green/yellow/red pip colors and the drain indicator).
- **§9 Placement validity.** The Adjacent=N measurement: expected = candidate foundation rect expanded by N cells must overlap an own building's foundation rect (i.e. Chebyshev gap ≤ N); confirm exactly, including which buildings count (own house, alive, fully constructed?) and terrain requirements per foundation cell (passable/clear, no units, no overlap). Also the green/red per-cell feedback rule.
- **§10 Queue rules.** Credits deducted incrementally over the build vs up-front; stall behavior when credits hit 0 mid-build; cancel refund (all spent so far?); one-item-at-a-time per BuildingType queue in 1.001; READY hold (indefinite); what happens when the ConYard dies mid-build. Prerequisite semantics: how tokens resolve (plain id = own live building of that id; group tokens POWER/BARRACKS/PROC/FACTORY/RADAR/TECH = any id in the [General] Prerequisite<Group> list). Confirm GAWEAP is therefore unbuildable without a refinery.
- **§11 Sidebar metrics + credits font.** The sidebar layout constants: total width (expected 168 px), the vertical stacking of top.shp (radar) / credits.shp / tab strip / cameo grid, cameo cell size (expected 60×48) and 2-column grid geometry, powerp.shp pip geometry and the meter's screen position, tab00/tab01 frame meanings (which SHP frame = which tab state). Also: the credits digits — which font file renders them (language.mix .fnt?) and whether extracting it is trivial; if not, recommend the built-in digit-strip divergence.

**"Decisions for ra2engine"** (one section per doc) must explicitly resolve each `[RE: …§N]` marker in Tasks 2–7.

- [ ] **Step 1:** Read `docs/re/workflow.md`, `docs/re/voxel-render.md`'s hunt provenance section, and two prior hunt scripts in `D:\ra2-re\scripts` for the headless pattern. **FOREGROUND ONLY — never background an analyzeHeadless run, never start a monitor; run each script synchronously and read its output when it returns.**
- [ ] **Step 2:** Empirical pre-checks with ra2extract (no Ghidra): dump `ggcnst.shp` / `gtcnstmk.shp` frame counts + canvas sizes, `side1.shp`/`top.shp`/`tab00.shp`/`powerp.shp`/`credits.shp` dimensions, `powricon.shp` dimensions. Record in the docs — these are §4/§5/§11 ground truth.
- [ ] **Step 3:** Write + run the hunt scripts (iterate; dump decompiled candidates to `D:\ra2-re\out`).
- [ ] **Step 4:** Write both RE docs (§1–§6, §7–§11, confidence tables, Decisions sections).
- [ ] **Step 5:** Commit:

```bash
git add docs/re/building-render.md docs/re/construction.md
git commit -m "docs(re): building rendering + construction semantics from gamemd.exe

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: ra2assets — building Rules/Art accessors + theater name resolver

**Files:**
- Modify: `include/ra2assets/rules.hpp`, `src/rules.cpp`
- Modify: `include/ra2assets/art.hpp`, `src/art.cpp`
- Create: `include/ra2assets/theater_name.hpp`, `src/theater_name.cpp`
- Modify: `cmake/ra2_sources.cmake` (add `src/theater_name.cpp` to `RA2_LIB_SOURCES`)
- Test: `tests/test_rules.cpp`, `tests/test_art.cpp`, `tests/test_theater_name.cpp` (new; add `target_sources(ra2assets_tests PRIVATE tests/test_theater_name.cpp)` to `CMakeLists.txt` next to the other test registrations)

**Interfaces:**
- Consumes: existing `Ini`, `Rules`, `Art` patterns (thin by-reference wrappers; see rules.hpp header comment).
- Produces (later tasks rely on these EXACT signatures):
  - `int Rules::cost(const std::string& id) const;` — `[id] Cost=`, 0 if absent.
  - `int Rules::power(const std::string& id) const;` — `[id] Power=`, 0 if absent (negative = drain).
  - `int Rules::adjacent(const std::string& id) const;` — `[id] Adjacent=`, 0 if absent.
  - `int Rules::tech_level(const std::string& id) const;` — `[id] TechLevel=`, 0 if absent.
  - `bool Rules::is_construction_yard(const std::string& id) const;` — `[id] ConstructionYard=` yes/true.
  - `std::vector<std::string> Rules::prerequisite(const std::string& id) const;` — comma-split `[id] Prerequisite=` tokens, whitespace-trimmed, UPPERCASED; empty if absent.
  - `std::vector<std::string> Rules::prerequisite_group(const std::string& group) const;` — `[General] Prerequisite<Group>=` list (group is e.g. "Power"/"Proc"/"Barracks"/"Factory"/"Radar"/"Tech"), comma-split, trimmed, UPPERCASED; empty if absent.
  - `double Rules::general_double(const std::string& key, double fallback) const;` — `[General] key=` parsed as double, tolerating a trailing `%` (percent values are returned as given, e.g. "50%" → 50.0). Used for BuildSpeed / RefundPercent / LowPowerPenaltyModifier / ConditionYellow / ConditionRed.
  - `int Rules::mp_money() const;` — `[MultiplayerDialogSettings] Money=`, fallback 10000.
  - `bool Art::foundation(const std::string& image, int& fw, int& fh) const;` — parses `[image] Foundation=WxH` (case-insensitive 'x'); false and untouched outs if absent/malformed.
  - `std::string Art::buildup(const std::string& image) const;` — `[image] Buildup=`, "" if absent.
  - `std::string Art::cameo(const std::string& image) const;` — `[image] Cameo=`, "" if absent.
  - `bool Art::new_theater(const std::string& image) const;` — `[image] NewTheater=` yes/true.
  - `theater_name.hpp`:

```cpp
#pragma once
#include <string>
#include <vector>

namespace ra2assets {

// A single candidate for resolving a NewTheater art name: try `file` inside
// nested mix `mix` (both lowercase, mix name relative to ra2.mix).
struct TheaterCandidate {
    std::string mix;   // e.g. "generic.mix"
    std::string file;  // e.g. "ggcnst.shp"
};

// NewTheater=yes filename resolution [RE-GATE APPLIED: building-render.md §1,
// recovered from FUN_005F8860/FUN_005F8EA0/FUN_005F8F00]. The munge applies
// ONLY when name[0] is one of {g,n,c,y} AND name[1] is one of {a,t} (case-
// insensitive): name[1] is replaced by the theater letter ('T' temperate,
// 'A' snow, 'U' urban); the lookup is a GLOBAL search over the loaded mixes;
// on miss, ONE retry with name[1]='G' (the generic art). There is NO third
// literal candidate for munged names (snow's letter IS 'A', so a literal
// spelling like "gacnst" is already the snow-themed name). Names that fail
// the pattern check get the literal name only.
// `image` is the artmd image/section name (e.g. "GACNST"); `ext` includes the
// dot (".shp"). iso_mix/theater_mix define the engine's walk ORDER over its
// open nested mixes (the binary searches all loaded mixes; we search these
// three per name): for each candidate NAME in order [themed, generic-'g']
// (or just [literal] for non-matching names), emit
//   { iso_mix, name+ext }, { theater_mix, name+ext }, { "generic.mix", name+ext }
// i.e. 6 entries for munged names, 3 for literal ones. Pinned real
// locations this must reproduce: ggcnst->generic.mix, gacnst->snow.mix,
// gtcnstmk->isotemp.mix.
// theater_mix: "temperat.mix" | "snow.mix" | "urban.mix".
std::vector<TheaterCandidate> theater_candidates(const std::string& image,
                                                 char theater_letter,
                                                 const std::string& iso_mix,
                                                 const std::string& theater_mix,
                                                 const std::string& ext);

} // namespace ra2assets
```

  All returned filenames are lowercase.

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_rules.cpp` (synthetic ini section + the guarded real-rulesmd pins) and `tests/test_art.cpp`; create `tests/test_theater_name.cpp`:

```cpp
// tests/test_rules.cpp — append. Uses the file's existing synthetic-Ini helpers.
TEST_CASE("rules: building accessors (synthetic)") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[General]\nBuildSpeed=.7\nRefundPercent=50%\nPrerequisitePower=GAPOWR,NAPOWR\n"
        "[MultiplayerDialogSettings]\nMoney=10000\n"
        "[GAPOWR]\nCost=800\nPower=200\nAdjacent=2\nTechLevel=1\nPrerequisite=GACNST\n"
        "[GACNST]\nConstructionYard=yes\nCost=3000\n"
        "[GAPILE]\nPrerequisite= power , GACNST\n");
    ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.cost("GAPOWR") == 800);
    CHECK(r.power("GAPOWR") == 200);
    CHECK(r.power("MISSING") == 0);
    CHECK(r.adjacent("GAPOWR") == 2);
    CHECK(r.tech_level("GAPOWR") == 1);
    CHECK(r.is_construction_yard("GACNST"));
    CHECK_FALSE(r.is_construction_yard("GAPOWR"));
    auto pre = r.prerequisite("GAPILE");
    REQUIRE(pre.size() == 2);
    CHECK(pre[0] == "POWER");   // trimmed + uppercased
    CHECK(pre[1] == "GACNST");
    auto grp = r.prerequisite_group("Power");
    REQUIRE(grp.size() == 2);
    CHECK(grp[0] == "GAPOWR");
    CHECK(r.general_double("BuildSpeed", 0.0) == doctest::Approx(0.7));
    CHECK(r.general_double("RefundPercent", 0.0) == doctest::Approx(50.0));
    CHECK(r.general_double("Absent", 3.5) == doctest::Approx(3.5));
    CHECK(r.mp_money() == 10000);
}

TEST_CASE("rules: real rulesmd building pins") {
    // Guarded like the file's other real-install tests (same load helper).
    // Load rulesmd.ini via the existing real-rules fixture in this file, then:
    //   CHECK(cost GAPOWR==800, NAPOWR==600, GAPILE==500, GAWEAP==2000, GACNST==3000)
    //   CHECK(power GAPOWR==200, NAPOWR==150, GAWEAP==-25, NAHAND==-10, GACNST==0)
    //   CHECK(adjacent GACNST==2); CHECK(tech_level GACNST==-1)
    //   CHECK(is_construction_yard GACNST && NACNST); CHECK_FALSE(GAPOWR)
    //   prerequisite("GAWEAP") == {"PROC","GAPILE","GACNST"}
    //   prerequisite_group("Proc") first entry "GAREFN"
    //   general_double("BuildSpeed",0)==0.7; ("RefundPercent",0)==50
    //   ("LowPowerPenaltyModifier",0)==1; ("ConditionYellow",0)==50; ("ConditionRed",0)==25
    //   mp_money()==10000
}
```

```cpp
// tests/test_art.cpp — append.
TEST_CASE("art: building accessors (synthetic)") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[GAPOWR]\nFoundation=2x2\nBuildup=GAPOWRMK\nCameo=POWRICON\nNewTheater=yes\n"
        "[BAD]\nFoundation=huge\n");
    ra2assets::Art a = ra2assets::Art::load(ini);
    int fw = -1, fh = -1;
    REQUIRE(a.foundation("GAPOWR", fw, fh));
    CHECK(fw == 2); CHECK(fh == 2);
    int ow = 9, oh = 9;
    CHECK_FALSE(a.foundation("BAD", ow, oh));
    CHECK(ow == 9);                       // untouched on failure
    CHECK_FALSE(a.foundation("MISSING", ow, oh));
    CHECK(a.buildup("GAPOWR") == "GAPOWRMK");
    CHECK(a.buildup("MISSING") == "");
    CHECK(a.cameo("GAPOWR") == "POWRICON");
    CHECK(a.new_theater("GAPOWR"));
    CHECK_FALSE(a.new_theater("MISSING"));
}

TEST_CASE("art: real artmd building pins") {
    // Guarded real-artmd fixture (same pattern as this file's existing pins):
    //   foundation GACNST==4x4, GAPOWR==2x2, GAPILE==3x2, GAWEAP==5x3,
    //   NAPOWR==3x2, NAHAND==2x2, NAWEAP==5x3
    //   buildup GACNST=="GACNSTMK", NAWEAP=="NAWEAPMK"
    //   cameo GAPOWR=="POWRICON", GAWEAP=="gwepicon", GACNST==""
    //   new_theater(GACNST) == true
}
```

```cpp
// tests/test_theater_name.cpp (new file)
#include "doctest.h"
#include "ra2assets/theater_name.hpp"
using ra2assets::theater_candidates;

TEST_CASE("theater_candidates: temperate GACNST order") {
    auto c = theater_candidates("GACNST", 'T', "isotemp.mix", "temperat.mix", ".shp");
    REQUIRE(c.size() == 6);            // [gtcnst x3 mixes, ggcnst x3 mixes]
    CHECK(c[0].mix == "isotemp.mix");   CHECK(c[0].file == "gtcnst.shp");
    CHECK(c[1].mix == "temperat.mix");  CHECK(c[1].file == "gtcnst.shp");
    CHECK(c[2].mix == "generic.mix");   CHECK(c[2].file == "gtcnst.shp");
    CHECK(c[3].mix == "isotemp.mix");   CHECK(c[3].file == "ggcnst.shp");
    CHECK(c[5].mix == "generic.mix");   CHECK(c[5].file == "ggcnst.shp");
}

TEST_CASE("theater_candidates: snow letter A themes to the literal spelling") {
    auto c = theater_candidates("GACNST", 'A', "isosnow.mix", "snow.mix", ".shp");
    REQUIRE(c.size() == 6);
    CHECK(c[0].file == "gacnst.shp");   // letter A == literal spelling
    CHECK(c[1].mix == "snow.mix");      // where the real file lives
    CHECK(c[3].file == "ggcnst.shp");
}

TEST_CASE("theater_candidates: non-NewTheater-pattern name is literal only") {
    // First-two-chars pattern {g,n,c,y}x{a,t} not matched -> no munge
    // [RE-GATE: building-render.md §1 FUN_005F8EA0 pattern check].
    auto c = theater_candidates("XXICON", 'T', "isotemp.mix", "temperat.mix", ".shp");
    REQUIRE(c.size() == 3);
    CHECK(c[0].file == "xxicon.shp");
    CHECK(c[2].mix == "generic.mix");
}

TEST_CASE("theater_candidates: real-install resolution pins") {
    // Guarded (install_dir pattern from test_vpl.cpp). Open ra2.mix; walk the
    // temperate candidates for GACNST(.shp): the FIRST candidate that
    // resolves must be generic.mix/ggcnst.shp (63,760 bytes), proving both
    // the fallback order and the empirical layout. Then GACNSTMK: first hit
    // must be isotemp.mix/gtcnstmk.shp (428,080 bytes).
}
```

- [ ] **Step 2:** `cmake --build build` then `.\build\ra2assets_tests.exe -ts="rules*,art*,theater*"` (PowerShell) — new cases FAIL to compile / fail.
- [ ] **Step 3:** Implement. rules.cpp/art.cpp additions follow the file's existing get-string/get-int helpers exactly (see `Rules::speed` / `Art::remapable` for the idiom). `theater_name.cpp`:

```cpp
#include "ra2assets/theater_name.hpp"
#include <cctype>

namespace ra2assets {

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<TheaterCandidate> theater_candidates(const std::string& image,
                                                 char theater_letter,
                                                 const std::string& iso_mix,
                                                 const std::string& theater_mix,
                                                 const std::string& ext) {
    std::vector<TheaterCandidate> out;
    if (image.empty()) return out;
    std::string base = lower(image);
    // [RE-GATE: building-render.md §1] munge only when name[0] in {g,n,c,y}
    // and name[1] in {a,t} (FUN_005F8EA0's pattern check); candidate NAMES
    // are [themed, generic-'g'] (FUN_005F8F00's one 'G' retry), each searched
    // across the walk-order mixes; non-matching names stay literal.
    auto push_name = [&](const std::string& name) {
        out.push_back({iso_mix, name + ext});
        out.push_back({theater_mix, name + ext});
        out.push_back({"generic.mix", name + ext});
    };
    bool munge = base.size() >= 2 &&
                 (base[0] == 'g' || base[0] == 'n' || base[0] == 'c' || base[0] == 'y') &&
                 (base[1] == 'a' || base[1] == 't');
    if (munge) {
        std::string themed = base;
        themed[1] = (char)std::tolower((unsigned char)theater_letter);
        std::string generic = base;
        generic[1] = 'g';
        push_name(themed);
        push_name(generic);   // real theater letters T/A/U never equal 'g'
    } else {
        push_name(base);
    }
    return out;
}

} // namespace ra2assets
```

- [ ] **Step 4:** Rebuild + rerun the three test files — PASS. Run the FULL suite.
- [ ] **Step 5:** Commit:

```bash
git add include/ra2assets/rules.hpp src/rules.cpp include/ra2assets/art.hpp src/art.cpp \
        include/ra2assets/theater_name.hpp src/theater_name.cpp cmake/ra2_sources.cmake \
        CMakeLists.txt tests/test_rules.cpp tests/test_art.cpp tests/test_theater_name.cpp
git commit -m "feat(assets): building rules/art accessors + NewTheater name resolver

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: engine/building — pure foundations, validity, frames, stepping

**Files:**
- Create: `engine/building.hpp`, `engine/building.cpp`
- Modify: `cmake/ra2_sources.cmake` (`RA2_ENGINE_SOURCES` += `engine/building.cpp`), `CMakeLists.txt` (`target_sources(ra2assets_tests PRIVATE tests/test_building.cpp engine/building.cpp)`)
- Test: `tests/test_building.cpp`

**Interfaces:**
- Consumes: `Occupancy` (occupancy.hpp), `Cellxy` (pathfind.hpp).
- Produces (Tasks 4–7 rely on these EXACT names):

```cpp
#pragma once
// Pure building core: type defs resolved from Rules/Art by the caller,
// foundation cell math, placement validity, damage/frame selection, and
// per-frame stepping (buildup progression, death anim timing). No SDL/OS.
// Calibration: docs/re/building-render.md (§ refs inline).
#include <string>
#include <vector>
#include "occupancy.hpp"
#include "pathfind.hpp"  // Cellxy

namespace ra2engine {

// One building type's stats, resolved ONCE by the caller (main.cpp from
// Rules/Art, or a test by hand). Index into the roster vector is the
// Building::type key used everywhere.
struct BuildingTypeDef {
    std::string id;                  // "GAPOWR"
    int fw = 1, fh = 1;              // Art Foundation=WxH
    int strength = 1;
    int armor_ix = 0;                // armor_index(Rules armor)
    int cost = 0;
    int power = 0;                   // + output, - drain
    int adjacent = 2;
    int tech_level = 0;
    bool conyard = false;
    std::vector<std::string> prerequisite;  // uppercased tokens (may be groups)
    std::string image;               // SHP image name (== id for all 8)
    std::string buildup;             // Art Buildup= ("" = place instantly Idle)
    std::string cameo;               // Art Cameo= ("" = never in sidebar)
    std::string explosion;           // Rules explosion_first(id)
};

// A placed building instance.
struct Building {
    int type = -1;                   // index into the roster
    int cx = 0, cy = 0;              // foundation ORIGIN cell [RE: building-render.md §2]
    int fw = 1, fh = 1;              // copied from the type at placement
    int house = 0;
    int hp = 1, max_hp = 1;
    enum class State { Buildup, Idle, Dead };
    State state = State::Buildup;
    double buildup_time = 0.0;       // seconds elapsed in Buildup
    double buildup_total = 0.0;      // seconds Buildup lasts (0 = instant)
    double die_time = -1.0;          // >=0: death anim playing
    bool alive = true;               // tombstone after death anim (never erased)
};

// Foundation origin convention [RE: building-render.md §2, pre-hunt]: the
// origin cell (cx, cy) is the foundation's minimum-(x, y) corner in raw iso
// cell space; the building occupies (cx+i, cy+j) for i in [0,fw), j in [0,fh).
std::vector<Cellxy> foundation_cells(int cx, int cy, int fw, int fh);

// Per-cell placement feedback for the cursor grid: one flag per foundation
// cell in foundation_cells order; true = that cell individually OK
// (in-bounds + passable + unoccupied + no building). The ADJACENCY test is
// all-or-nothing and reported separately.
struct PlacementCheck {
    bool ok = false;                  // all cells OK AND adjacency satisfied
    bool adjacency = false;           // within Adjacent= range of an own building
    std::vector<bool> cell_ok;        // per foundation cell
};

// Validity [RE: construction.md §9, pre-hunt]: every foundation cell must be
// in-grid, passable (grid value 1), free in `occ` (no unit), and not overlap
// any ALIVE building's foundation. Adjacency: the candidate foundation rect
// expanded by `adjacent` cells (Chebyshev) must intersect the foundation
// rect of at least one alive, same-house, non-Dead building. `bypass_adjacency`
// (scripted spawns: the very first ConYard has no neighbors) skips ONLY the
// adjacency test, never the cell tests.
PlacementCheck placement_check(int cx, int cy, int fw, int fh, int house, int adjacent,
                               const std::vector<std::uint8_t>& passable, int grid_w, int grid_h,
                               const Occupancy& occ,
                               const std::vector<Building>& buildings,
                               bool bypass_adjacency);

// Frame selection for the MAIN building SHP [RE: building-render.md §4,
// pre-hunt]: image frames are the first half (second half = shadows, like
// unit SHPs); frame 0 = healthy, frame 1 = damaged, shown when
// hp/max_hp <= condition_yellow (0..1 fraction). Returns 0 when the SHP has
// fewer than 2 image frames. Only meaningful in State::Idle.
int building_frame(const Building& b, int shp_frame_count, double condition_yellow);

// Frame selection for the BUILDUP SHP during State::Buildup
// [RE-GATE APPLIED: building-render.md §5]: image frames = first half; the
// anim plays ONCE, one frame per fixed PERIOD, period = buildup_total /
// image_frames seconds (the original's per-frame delay is
// trunc(BuildupTime*900/image_frames) ticks; buildup_total is period *
// image_frames, so dividing recovers the period). frame =
// trunc(buildup_time / period), clamped to [0, image_frames-1].
int buildup_frame(const Building& b, int buildup_shp_frame_count);

// Death anim duration (the explosion SHP is drawn by main.cpp; this is the
// sim-side tombstone timer, matching the unit kDieAnimSecs pattern).
constexpr double kBuildingDieSecs = 1.0;

// Advance every building by dt: Buildup accumulates buildup_time and flips
// to Idle at buildup_total; Dead accumulates die_time and tombstones
// (alive=false) at kBuildingDieSecs. Idle does nothing. Pure; deterministic;
// ascending index. Returns indices that flipped Buildup->Idle this call (the
// caller re-computes power on those) -- Dead->tombstone needs no report (the
// caller released blocking when the building DIED, not when it tombstoned).
std::vector<int> step_buildings(std::vector<Building>& buildings, double dt);

// Kill helper shared by combat and tests: marks Dead + die_time=0 (idempotent
// -- already-Dead/dead buildings are untouched). hp clamped to 0.
void kill_building(Building& b);

}  // namespace ra2engine
```

- [ ] **Step 1: Write the failing tests** (`tests/test_building.cpp`):

```cpp
#include "doctest.h"
#include "../engine/building.hpp"
#include "../engine/occupancy.hpp"
using namespace ra2engine;

static std::vector<std::uint8_t> open_grid(int w, int h) {
    return std::vector<std::uint8_t>((size_t)w * h, 1);
}
static Building mk(int cx, int cy, int fw, int fh, int house = 0) {
    Building b; b.cx = cx; b.cy = cy; b.fw = fw; b.fh = fh; b.house = house;
    b.state = Building::State::Idle; b.hp = b.max_hp = 100; return b;
}

TEST_CASE("foundation_cells: 3x2 enumerates row-major from origin") {
    auto c = foundation_cells(5, 7, 3, 2);
    REQUIRE(c.size() == 6);
    CHECK(c[0].x == 5); CHECK(c[0].y == 7);
    CHECK(c[2].x == 7); CHECK(c[2].y == 7);
    CHECK(c[5].x == 7); CHECK(c[5].y == 8);
}

TEST_CASE("placement_check: cell tests") {
    auto pass = open_grid(20, 20);
    Occupancy occ(20, 20);
    std::vector<Building> bs = {mk(2, 2, 4, 4)};   // own ConYard at (2,2)

    // valid: adjacent to the ConYard (gap 1 <= Adjacent 2)
    auto ok = placement_check(7, 2, 2, 2, 0, 2, pass, 20, 20, occ, bs, false);
    CHECK(ok.ok); CHECK(ok.adjacency);
    REQUIRE(ok.cell_ok.size() == 4);
    CHECK(ok.cell_ok[0]);

    // out of bounds cell -> that cell flagged, ok false
    auto oob = placement_check(19, 19, 2, 2, 0, 2, pass, 20, 20, occ, bs, true);
    CHECK_FALSE(oob.ok);
    CHECK(oob.cell_ok[0]);          // (19,19) in bounds
    CHECK_FALSE(oob.cell_ok[3]);    // (20,20) out

    // impassable terrain
    auto pass2 = pass; pass2[3 * 20 + 8] = 0;   // cell (8,3)
    auto imp = placement_check(8, 3, 1, 1, 0, 2, pass2, 20, 20, occ, bs, true);
    CHECK_FALSE(imp.ok); CHECK_FALSE(imp.cell_ok[0]);

    // unit in the way
    occ.set(7, 3, /*unit*/ 0);
    auto unit = placement_check(7, 2, 2, 2, 0, 2, pass, 20, 20, occ, bs, false);
    CHECK_FALSE(unit.ok); CHECK_FALSE(unit.cell_ok[2]);  // (7,3) is index 2 of 2x2
    occ.clear(7, 3);

    // overlap an existing building
    auto ovl = placement_check(4, 4, 2, 2, 0, 2, pass, 20, 20, occ, bs, true);
    CHECK_FALSE(ovl.ok); CHECK_FALSE(ovl.cell_ok[0]);
}

TEST_CASE("placement_check: adjacency [RE: construction.md §9]") {
    auto pass = open_grid(30, 30);
    Occupancy occ(30, 30);
    std::vector<Building> bs = {mk(2, 2, 4, 4)};   // own, cells x/y in [2,5]

    // gap exactly Adjacent (2): candidate at x=8 -> gap = 8-5-1 = 2 -> OK
    CHECK(placement_check(8, 2, 2, 2, 0, 2, pass, 30, 30, occ, bs, false).adjacency);
    // gap 3 -> fails
    auto far = placement_check(9, 2, 2, 2, 0, 2, pass, 30, 30, occ, bs, false);
    CHECK_FALSE(far.adjacency); CHECK_FALSE(far.ok);
    CHECK(far.cell_ok[0]);   // cells individually fine; adjacency is the failure
    // enemy building doesn't grant adjacency
    std::vector<Building> enemy = {mk(2, 2, 4, 4, /*house*/ 1)};
    CHECK_FALSE(placement_check(8, 2, 2, 2, 0, 2, pass, 30, 30, occ, enemy, false).adjacency);
    // dead building doesn't grant adjacency
    std::vector<Building> dead = {mk(2, 2, 4, 4)};
    kill_building(dead[0]);
    CHECK_FALSE(placement_check(8, 2, 2, 2, 0, 2, pass, 30, 30, occ, dead, false).adjacency);
    // bypass skips adjacency only
    CHECK(placement_check(20, 20, 2, 2, 0, 2, pass, 30, 30, occ, bs, true).ok);
}

TEST_CASE("building_frame: healthy/damaged at ConditionYellow") {
    Building b = mk(0, 0, 2, 2);
    b.hp = 100; b.max_hp = 100;
    // 4-frame SHP: frames 0,1 image; 2,3 shadow [RE: building-render.md §4]
    CHECK(building_frame(b, 4, 0.5) == 0);
    b.hp = 51; CHECK(building_frame(b, 4, 0.5) == 0);
    b.hp = 50; CHECK(building_frame(b, 4, 0.5) == 1);   // <= threshold
    b.hp = 1;  CHECK(building_frame(b, 4, 0.5) == 1);
    CHECK(building_frame(b, 2, 0.5) == 0);              // 1 image frame only
}

TEST_CASE("buildup_frame + step_buildings: plays once then Idle") {
    Building b = mk(0, 0, 2, 2);
    b.state = Building::State::Buildup;
    b.buildup_total = 2.0;
    // 20-frame MK SHP -> 10 image frames, period = 2.0/10 = 0.2 s
    // [RE-GATE: building-render.md §5 — frame = trunc(time/period), clamped]
    b.buildup_time = 0.0;  CHECK(buildup_frame(b, 20) == 0);
    b.buildup_time = 0.19; CHECK(buildup_frame(b, 20) == 0);
    b.buildup_time = 1.0;  CHECK(buildup_frame(b, 20) == 5);   // trunc(1.0/0.2)
    b.buildup_time = 1.9;  CHECK(buildup_frame(b, 20) == 9);
    b.buildup_time = 9.9;  CHECK(buildup_frame(b, 20) == 9);   // clamped

    std::vector<Building> v = {b};
    auto done = step_buildings(v, 1.0);
    CHECK(done.empty());
    CHECK(v[0].state == Building::State::Buildup);
    done = step_buildings(v, 1.0);
    REQUIRE(done.size() == 1); CHECK(done[0] == 0);
    CHECK(v[0].state == Building::State::Idle);
}

TEST_CASE("kill_building + death tombstone") {
    std::vector<Building> v = {mk(0, 0, 2, 2)};
    kill_building(v[0]);
    CHECK(v[0].state == Building::State::Dead);
    CHECK(v[0].hp == 0);
    CHECK(v[0].die_time == doctest::Approx(0.0));
    kill_building(v[0]);                       // idempotent
    CHECK(v[0].die_time == doctest::Approx(0.0));
    step_buildings(v, kBuildingDieSecs + 0.01);
    CHECK_FALSE(v[0].alive);
    CHECK(v[0].state == Building::State::Dead);
}
```

- [ ] **Step 2:** Build; new tests fail to compile (building.hpp absent).
- [ ] **Step 3:** Implement `engine/building.cpp`:

```cpp
#include "building.hpp"
#include <algorithm>

namespace ra2engine {

std::vector<Cellxy> foundation_cells(int cx, int cy, int fw, int fh) {
    std::vector<Cellxy> out;
    out.reserve((size_t)fw * fh);
    for (int j = 0; j < fh; ++j)
        for (int i = 0; i < fw; ++i)
            out.push_back({cx + i, cy + j});
    return out;
}

// Chebyshev gap between two axis-aligned cell rects (0 = touching/overlap).
static int rect_gap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    int dx = std::max({bx - (ax + aw - 1) - 1, ax - (bx + bw - 1) - 1, 0});
    int dy = std::max({by - (ay + ah - 1) - 1, ay - (by + bh - 1) - 1, 0});
    return std::max(dx, dy);
}

PlacementCheck placement_check(int cx, int cy, int fw, int fh, int house, int adjacent,
                               const std::vector<std::uint8_t>& passable, int grid_w, int grid_h,
                               const Occupancy& occ,
                               const std::vector<Building>& buildings,
                               bool bypass_adjacency) {
    PlacementCheck r;
    auto cells = foundation_cells(cx, cy, fw, fh);
    r.cell_ok.assign(cells.size(), false);
    bool all = true;
    for (size_t k = 0; k < cells.size(); ++k) {
        int x = cells[k].x, y = cells[k].y;
        bool ok = x >= 0 && y >= 0 && x < grid_w && y < grid_h &&
                  passable[(size_t)y * grid_w + x] != 0 && occ.is_free(x, y);
        if (ok) {   // no overlap with any alive building's foundation
            for (const Building& b : buildings) {
                if (!b.alive || b.state == Building::State::Dead) continue;
                if (x >= b.cx && x < b.cx + b.fw && y >= b.cy && y < b.cy + b.fh) {
                    ok = false;
                    break;
                }
            }
        }
        r.cell_ok[k] = ok;
        all = all && ok;
    }
    // [RE: construction.md §9] expanded-rect adjacency vs own alive buildings.
    r.adjacency = bypass_adjacency;
    if (!bypass_adjacency) {
        for (const Building& b : buildings) {
            if (!b.alive || b.state == Building::State::Dead || b.house != house) continue;
            if (rect_gap(cx, cy, fw, fh, b.cx, b.cy, b.fw, b.fh) <= adjacent) {
                r.adjacency = true;
                break;
            }
        }
    }
    r.ok = all && r.adjacency;
    return r;
}

int building_frame(const Building& b, int shp_frame_count, double condition_yellow) {
    int image_frames = shp_frame_count / 2;   // [RE: building-render.md §4] shadows = 2nd half
    if (image_frames < 2) return 0;
    double frac = b.max_hp > 0 ? (double)b.hp / b.max_hp : 0.0;
    return frac <= condition_yellow ? 1 : 0;
}

int buildup_frame(const Building& b, int buildup_shp_frame_count) {
    int image_frames = buildup_shp_frame_count / 2;   // [RE-GATE: building-render.md §5]
    if (image_frames < 1) return 0;
    if (b.buildup_total <= 0.0) return image_frames - 1;
    double period = b.buildup_total / image_frames;   // one frame per period
    int f = (int)(b.buildup_time / period);
    return std::clamp(f, 0, image_frames - 1);
}

std::vector<int> step_buildings(std::vector<Building>& buildings, double dt) {
    std::vector<int> completed;
    for (size_t i = 0; i < buildings.size(); ++i) {
        Building& b = buildings[i];
        if (!b.alive) continue;
        if (b.state == Building::State::Buildup) {
            b.buildup_time += dt;
            if (b.buildup_time >= b.buildup_total) {
                b.state = Building::State::Idle;
                completed.push_back((int)i);
            }
        } else if (b.state == Building::State::Dead) {
            b.die_time += dt;
            if (b.die_time >= kBuildingDieSecs) b.alive = false;
        }
    }
    return completed;
}

void kill_building(Building& b) {
    if (!b.alive || b.state == Building::State::Dead) return;
    b.hp = 0;
    b.state = Building::State::Dead;
    b.die_time = 0.0;
}

}  // namespace ra2engine
```

- [ ] **Step 4:** Build; run `-ts="*building*"` then the full suite — PASS.
- [ ] **Step 5:** Commit (`feat(engine): pure building module — foundations, placement validity, frames`).

---

### Task 4: World + combat integration — place/remove, passability, attack-building

**Files:**
- Modify: `engine/world.hpp`, `engine/world.cpp`
- Modify: `engine/combat.hpp`, `engine/combat.cpp`
- Test: `tests/test_world.cpp`, `tests/test_combat.cpp` (append)

**Interfaces:**
- Consumes: Task 3's `Building`/`BuildingTypeDef`/`foundation_cells`/`kill_building`/`step_buildings`.
- Produces:
  - `Unit` gains `int target_building = -1;` (an explicit-order building target; mutually exclusive with `target` — setting one clears the other).
  - `World` gains:
    - `std::vector<Building>& buildings();` (+ const overload)
    - `int add_building(int type, int cx, int cy, int fw, int fh, int house, int strength, int armor_ix, double buildup_total, bool instant);` — appends a Building (State::Buildup, or Idle when `instant`; hp = max_hp = strength; armor_ix stored on the Building for combat), clears `passable_` for its foundation cells, returns its index. (Scripted spawns pass `instant=true`.) Precondition: caller validated via `placement_check`.
    - `int building_at_cell(int x, int y) const;` — index of the alive, non-Dead building whose foundation contains (x,y), else -1.
    - `void order_attack_building(int b_idx);` — every SELECTED house-0, alive, non-dying unit: `target_building = b_idx; target = -1; attack_order = true;` (+ the same occupancy-release/path-clear dance as `order_attack`). No-op for out-of-range/dead/Dead-state/house-0 targets.
    - `World::step` additionally: calls `step_buildings(buildings_, dt)`; when a building DIES this frame (state flipped to Dead — detected by kill inside step_combat, see below) or completes buildup, nothing extra is needed for passability EXCEPT death restores it: World restores `passable_ = 1` for the foundation cells of any building whose state is Dead and whose passability hasn't been restored yet (track with a `bool blocking_released` field added to `Building` — set by World, not building.cpp).
  - `step_combat` signature changes to `std::vector<MoveRequest> step_combat(std::vector<Unit>& units, std::vector<Building>& buildings, Occupancy& occ, double dt);` — the OLD 3-arg call sites are all inside World::step (one place) and tests; an empty `buildings` vector reproduces the old behavior EXACTLY (byte-identity gates depend on this). New behavior, for a unit with `target_building >= 0`:
    - target validity: building alive && state != Dead, else clear target_building/attack_order and fall back to normal targeting;
    - range check: `hypot(min over foundation cells of (cell.x - u.fx, cell.y - u.fy)) <= w_range` (nearest-foundation-cell distance — a 4x4 building is in range of its EDGE, not its center);
    - in range: fire on the existing rof cadence — damage `damage_vs(u.w_damage, u.w_verses, b.armor_ix)` (`Building` gains `int armor_ix = 0;`, set by add_building); hp -= dmg; hp <= 0 → `kill_building`;
    - out of range with attack_order: emit `MoveRequest{unit, nearest_cell.x, nearest_cell.y, /*approach*/ true}`;
    - units NEVER auto-acquire buildings (explicit orders only — divergence already documented in the spec);
    - buildings never retaliate (no weapons in this roster).

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_combat.cpp`:

```cpp
TEST_CASE("combat: attack-building — range to nearest foundation cell, kill, tombstone") {
    // Grid-free: step_combat needs only units+buildings+occ.
    std::vector<Unit> units(1);
    Unit& u = units[0];
    u.alive = true; u.house = 0; u.hp = u.max_hp = 100;
    u.fx = u.cell_x = 0; u.fy = u.cell_y = 0;
    u.w_damage = 90; u.w_rof = 15; u.w_range = 4.0;
    u.w_verses = {100,100,100,100,100,100,100,100,100,100,100};
    u.attack_order = true; u.target_building = 0;

    std::vector<Building> bs(1);
    Building& b = bs[0];
    b.state = Building::State::Idle; b.alive = true; b.house = 1;
    b.cx = 3; b.cy = 0; b.fw = 4; b.fh = 4;       // nearest cell (3,0), dist 3 <= 4
    b.hp = b.max_hp = 180; b.armor_ix = 0;

    Occupancy occ(32, 32);
    occ.set(0, 0, 0);
    auto reqs = step_combat(units, bs, occ, 1.0 / 15.0);
    CHECK(reqs.empty());                            // in range -> no pursuit
    CHECK(b.hp == 90);                              // one 90-damage hit
    step_combat(units, bs, occ, 1.0);               // rof elapses, second hit
    CHECK(b.hp <= 0);
    CHECK(b.state == Building::State::Dead);
    CHECK(units[0].target_building == -1);          // target cleared on death
}

TEST_CASE("combat: attack-building out of range emits approach MoveRequest") {
    std::vector<Unit> units(1);
    Unit& u = units[0];
    u.alive = true; u.house = 0; u.hp = u.max_hp = 100;
    u.fx = u.cell_x = 0; u.fy = u.cell_y = 0;
    u.w_damage = 10; u.w_rof = 15; u.w_range = 2.0;
    u.w_verses = {100};
    u.attack_order = true; u.target_building = 0;
    std::vector<Building> bs(1);
    bs[0].state = Building::State::Idle; bs[0].alive = true; bs[0].house = 1;
    bs[0].cx = 6; bs[0].cy = 0; bs[0].fw = 2; bs[0].fh = 2; bs[0].hp = bs[0].max_hp = 50;
    Occupancy occ(32, 32);
    auto reqs = step_combat(units, bs, occ, 1.0 / 15.0);
    REQUIRE(reqs.size() == 1);
    CHECK(reqs[0].unit == 0);
    CHECK(reqs[0].goal_x == 6); CHECK(reqs[0].goal_y == 0);   // nearest foundation cell
    CHECK(reqs[0].approach);
    CHECK(bs[0].hp == 50);
}

TEST_CASE("combat: empty buildings vector == legacy behavior") {
    // Byte-identity guard: with no buildings, the 4-arg step_combat must be
    // indistinguishable from the old 3-arg one. Re-run one existing scenario
    // from this file (copy the exact setup of the first infantry-vs-infantry
    // TEST_CASE above) passing an empty buildings vector, and assert the same
    // outcomes that test asserts.
}
```

Append to `tests/test_world.cpp`:

```cpp
TEST_CASE("world: add_building blocks passability, death restores it") {
    // Build the minimal in-memory MapFile fixture this file already uses
    // (see its existing World tests for the helper), sized >= 12x12 cells.
    // Then:
    //   World w(map);
    //   REQUIRE(w.walkable(4, 4));
    //   int bi = w.add_building(/*type*/0, 4, 4, 2, 2, /*house*/1,
    //                           /*strength*/100, /*armor_ix*/0,
    //                           /*buildup_total*/0.0, /*instant*/true);
    //   CHECK_FALSE(w.walkable(4, 4)); CHECK_FALSE(w.walkable(5, 5));
    //   CHECK(w.walkable(6, 4));
    //   CHECK(w.building_at_cell(5, 4) == bi);
    //   CHECK(w.building_at_cell(6, 4) == -1);
    //   kill_building(w.buildings()[bi]);
    //   w.step(0.001);                       // death detected -> passability restored
    //   CHECK(w.walkable(4, 4));
    //   CHECK(w.building_at_cell(4, 4) == -1);
    //   w.step(kBuildingDieSecs + 0.1);
    //   CHECK_FALSE(w.buildings()[bi].alive);   // tombstoned, never erased
}

TEST_CASE("world: order_attack_building sets exclusive target") {
    // Same fixture; spawn one house-0 combat unit (existing UnitCombat helper),
    // select it, add an enemy building, order_attack_building(bi):
    //   CHECK(u.target_building == bi); CHECK(u.target == -1);
    //   CHECK(u.attack_order);
    // Then order_attack(<enemy unit>) and CHECK(u.target_building == -1).
    // Ordering against a house-0 building: no-op (target_building stays -1).
}
```

- [ ] **Step 2:** Build → fails (signatures absent).
- [ ] **Step 3:** Implement. Key edits:
  - `world.hpp`: `#include "building.hpp"`; Unit `+ int target_building = -1;`; World `+ std::vector<Building> buildings_;` + the four methods above. `add_building` sets `passable_[y*grid_w_+x] = 0` per foundation cell; `Building` gains `bool blocking_released = false;` (declared in building.hpp, used only by World).
  - `world.cpp` `World::step`: after `step_units`, call `step_buildings(buildings_, dt)`; then for each building with `state == Dead && !blocking_released`, restore `passable_ = 1` on its cells and set `blocking_released = true`; pass `buildings_` into `step_combat`.
  - `order_attack` additionally clears `target_building` on the ordered units; `order_attack_building` mirrors `order_attack` (read its body first — reuse the release/re-assert lambda verbatim).
  - `combat.cpp`: at the top of the per-unit loop, the building-target branch (validity → range via nearest-foundation-cell → fire/pursue as specced above). Firing uses the SAME rof_cool/fire_flash fields; on kill also clears every OTHER unit's `target_building` pointing at the dead building. **Do not touch any existing branch** — the unit-vs-unit path must remain bit-identical when `buildings` is empty.
- [ ] **Step 4:** Build; run `-ts="*combat*,*world*"`, then FULL suite. Then the byte-identity gates from PowerShell (both demos print a final hash line; compare to the pinned values recorded in `.superpowers/sdd/progress.md`):

```powershell
.\build\ra2engine.exe --demo-combat
.\build\ra2engine.exe --demo-tank
```

- [ ] **Step 5:** Commit (`feat(engine): buildings in World — blocking, attack-building combat`).

---

### Task 5: engine/production — queue, credits, power (pure)

**Files:**
- Create: `engine/production.hpp`, `engine/production.cpp`
- Modify: `cmake/ra2_sources.cmake` (`RA2_ENGINE_SOURCES` += `engine/production.cpp`), `CMakeLists.txt` (`target_sources(ra2assets_tests PRIVATE tests/test_production.cpp engine/production.cpp)`)
- Test: `tests/test_production.cpp`

**Interfaces:**
- Consumes: Task 3's `Building`/`BuildingTypeDef`.
- Produces:

```cpp
#pragma once
// Pure construction economy: the single BuildingType queue, credits, power.
// Calibration: docs/re/construction.md (§ refs inline). No SDL/OS.
#include <string>
#include <vector>
#include "building.hpp"

namespace ra2engine {

// [RE: construction.md §7, pre-hunt] BuildSpeed comment in rulesmd: "time in
// minutes to produce a 1000 credit cost item"; RA2 logic runs 900 frames/min
// at 15 Hz. Truncated toward zero like the damage formula.
int build_frames(int cost, double build_speed);   // = (int)(cost/1000.0 * build_speed * 900.0)

// Power totals over ALIVE, non-Dead buildings of `house`. Buildings still in
// Buildup do NOT contribute output or drain [RE: construction.md §8,
// pre-hunt: power applies when operational].
struct PowerTotals { int output = 0; int drain = 0; };   // drain stored positive
PowerTotals power_totals(const std::vector<Building>& buildings,
                         const std::vector<BuildingTypeDef>& types, int house);

// [RE-GATE APPLIED: construction.md §8] low power = drain > output. Recovered
// rule (FUN_004FCC50 + the production-speed chain): with f = output/drain,
//   speed = 1 - low_power_penalty_modifier * (1 - f)        (0 output -> 0)
//   speed = max(speed, kMinLowPowerSpeed)                    (= .5, rulesmd)
//   speed = min(speed, kMaxLowPowerSpeed)   [low power only] (= .8, rulesmd)
// Full power -> exactly 1.0. With modifier=1 and any shortage, the result is
// therefore always in [.5, .8].
constexpr double kMinLowPowerSpeed = 0.5;   // rulesmd MinLowPowerProductionSpeed
constexpr double kMaxLowPowerSpeed = 0.8;   // rulesmd MaxLowPowerProductionSpeed
double build_speed_multiplier(const PowerTotals& p, double low_power_penalty_modifier);

// One-slot BuildingType queue [RE-GATE APPLIED: construction.md §10 —
// FactoryClass's 54-step incremental economy]. Production advances in
// exactly kProductionSteps stages; each stage lasts `step_ticks` 15 Hz
// frames (= clamp(trunc(total_frames/54), 1, 255)); entering stage s+1
// charges `(cost - spent) / (54 - s)` credits — equal cost/54 chunks under
// uninterrupted funding. When a stage's charge is unaffordable the build
// STALLS (no progress, no charge, retried every tick).
constexpr int kProductionSteps = 54;
struct QueueSlot {
    int type = -1;              // roster index; -1 = idle
    int stage = 0;              // 0..kProductionSteps
    int step_ticks = 1;         // 15 Hz frames per stage
    double frames_accum = 0.0;  // fractional frames toward the next stage
    int total_frames = 1;       // Time_To_Build (the 504-frame pin) — display/witness
    double spent = 0.0;         // credits deducted so far
    bool ready = false;         // finished, waiting for placement
};

struct ProductionState {
    double credits = 0.0;
    QueueSlot slot;
};

// Prerequisite check [RE: construction.md §10]: every token must be satisfied.
// A plain token = an ALIVE, IDLE (fully built) building of that id owned by
// `house` exists. Group tokens (POWER/BARRACKS/PROC/FACTORY/RADAR/TECH) =
// any id in `groups[token]` satisfied the same way. Unknown tokens fail.
bool prerequisites_met(const std::vector<std::string>& tokens,
                       const std::vector<Building>& buildings,
                       const std::vector<BuildingTypeDef>& types, int house,
                       const std::vector<std::pair<std::string, std::vector<std::string>>>& groups);

// Can `type` start building? conyard_alive = an own alive Idle ConYard exists
// (the BuildingType factory); slot must be idle; prereqs met; TechLevel > 0.
// Affordability is NOT gated here — RA2 starts a build with any credits and
// stalls at 0 [RE: construction.md §10].
bool can_start(const ProductionState& ps, int type,
               const std::vector<BuildingTypeDef>& types,
               const std::vector<Building>& buildings, int house,
               const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
               bool conyard_alive);

// Start building `type` (caller checked can_start): total_frames =
// build_frames(cost, build_speed); step_ticks = clamp(total_frames /
// kProductionSteps, 1, 255) (integer division = the binary's trunc)
// [RE-GATE: construction.md §10 FUN_004C9DC0]. GAPOWR: 504 -> step_ticks 9
// (wall duration 54*9 = 486 frames = 32.4 s).
void start(ProductionState& ps, int type, const std::vector<BuildingTypeDef>& types,
           double build_speed);

// Advance by dt seconds [RE-GATE: construction.md §10]:
//   frames_accum += dt * 15 * speed_mult;
//   while (frames_accum >= step_ticks && stage < kProductionSteps):
//     charge = (cost - spent) / (kProductionSteps - stage);
//     if (credits < charge) { frames_accum = step_ticks; break; }  // stall+retry
//     credits -= charge; spent += charge; ++stage; frames_accum -= step_ticks;
//   stage == kProductionSteps -> spent settles to exactly cost, ready = true.
// The stall clamp (frames_accum = step_ticks) mirrors the original's
// rolled-back stage retried each tick — a funding gap never produces a
// catch-up burst. No-op when idle/ready.
void tick(ProductionState& ps, double dt, double speed_mult,
          const std::vector<BuildingTypeDef>& types);

// Cancel: refund everything spent so far, clear the slot
// [RE-GATE: construction.md §10 — Abandon refunds Cost_Of minus balance,
// i.e. ALL spent].
void cancel(ProductionState& ps);

// Placement succeeded: clear the slot (credits already fully spent).
void placed(ProductionState& ps);

}  // namespace ra2engine
```

- [ ] **Step 1: Write the failing tests** (`tests/test_production.cpp`):

```cpp
#include "doctest.h"
#include "../engine/production.hpp"
using namespace ra2engine;

static std::vector<BuildingTypeDef> roster() {
    std::vector<BuildingTypeDef> t(3);
    t[0].id = "GACNST"; t[0].conyard = true; t[0].cost = 3000; t[0].tech_level = -1;
    t[1].id = "GAPOWR"; t[1].cost = 800; t[1].power = 200; t[1].tech_level = 1;
    t[1].prerequisite = {"GACNST"};
    t[2].id = "GAPILE"; t[2].cost = 500; t[2].power = -10; t[2].tech_level = 2;
    t[2].prerequisite = {"POWER", "GACNST"};
    return t;
}
static Building live(int type, int house = 0) {
    Building b; b.type = type; b.house = house; b.alive = true;
    b.state = Building::State::Idle; b.hp = b.max_hp = 100; return b;
}
static const std::vector<std::pair<std::string, std::vector<std::string>>> kGroups =
    {{"POWER", {"GAPOWR", "NAPOWR"}}};

TEST_CASE("build_frames: BuildSpeed formula [RE: construction.md §7]") {
    CHECK(build_frames(1000, 0.7) == 630);   // 0.7 min * 900
    CHECK(build_frames(800, 0.7) == 504);
    CHECK(build_frames(500, 0.7) == 315);
    CHECK(build_frames(2000, 0.7) == 1260);
}

TEST_CASE("power_totals + build_speed_multiplier [RE-GATE: construction.md §8]") {
    auto t = roster();
    std::vector<Building> bs = {live(1), live(2)};       // +200, -10
    auto p = power_totals(bs, t, 0);
    CHECK(p.output == 200); CHECK(p.drain == 10);
    CHECK(build_speed_multiplier(p, 1.0) == doctest::Approx(1.0));
    bs[0].state = Building::State::Buildup;              // powerplant not up yet
    p = power_totals(bs, t, 0);
    CHECK(p.output == 0); CHECK(p.drain == 10);
    // f = 0/10 = 0 -> speed 1-1*(1-0) = 0 -> max(.5) -> min(.8) = 0.5 exactly
    CHECK(build_speed_multiplier(p, 1.0) == doctest::Approx(kMinLowPowerSpeed));
    // partial shortage: out 90 / drain 100 -> 1-1*(1-.9) = .9 -> capped at .8
    PowerTotals part; part.output = 90; part.drain = 100;
    CHECK(build_speed_multiplier(part, 1.0) == doctest::Approx(kMaxLowPowerSpeed));
    // heavy modifier: out 90/drain 100, mod 4 -> 1-4*.1 = .6 (within [.5,.8])
    CHECK(build_speed_multiplier(part, 4.0) == doctest::Approx(0.6));
    kill_building(bs[1]);
    p = power_totals(bs, t, 0);
    CHECK(p.drain == 0);                                 // dead building drains nothing
    // enemy buildings don't count
    std::vector<Building> enemy = {live(1, 1)};
    CHECK(power_totals(enemy, t, 0).output == 0);
}

TEST_CASE("prerequisites_met: plain ids, groups, Buildup doesn't count") {
    auto t = roster();
    std::vector<Building> bs = {live(0)};                // ConYard only
    CHECK(prerequisites_met({"GACNST"}, bs, t, 0, kGroups));
    CHECK_FALSE(prerequisites_met({"POWER", "GACNST"}, bs, t, 0, kGroups));
    bs.push_back(live(1));                               // + power plant
    CHECK(prerequisites_met({"POWER", "GACNST"}, bs, t, 0, kGroups));
    bs[1].state = Building::State::Buildup;              // not finished
    CHECK_FALSE(prerequisites_met({"POWER"}, bs, t, 0, kGroups));
    CHECK_FALSE(prerequisites_met({"PROC"}, bs, t, 0, kGroups));   // unknown group
}

TEST_CASE("can_start: slot, conyard, tech level") {
    auto t = roster();
    std::vector<Building> bs = {live(0)};
    ProductionState ps; ps.credits = 10000;
    CHECK(can_start(ps, 1, t, bs, 0, kGroups, true));
    CHECK_FALSE(can_start(ps, 1, t, bs, 0, kGroups, false));   // no ConYard
    CHECK_FALSE(can_start(ps, 0, t, bs, 0, kGroups, true));    // TechLevel -1
    CHECK_FALSE(can_start(ps, 2, t, bs, 0, kGroups, true));    // POWER missing
    start(ps, 1, t, 0.7);
    CHECK_FALSE(can_start(ps, 1, t, bs, 0, kGroups, true));    // slot busy
}

TEST_CASE("tick: 54-step economy [RE-GATE: construction.md §10]") {
    auto t = roster();
    ProductionState ps; ps.credits = 10000;
    start(ps, 1, t, 0.7);                    // GAPOWR: 504 frames, 800 credits
    CHECK(ps.slot.total_frames == 504);
    CHECK(ps.slot.step_ticks == 9);          // trunc(504/54)
    // 27 stages = half the steps = 27*9 = 243 frames = 16.2 s
    tick(ps, 243.0 / 15.0, 1.0, t);
    CHECK(ps.slot.stage == 27);
    CHECK(ps.credits == doctest::Approx(10000 - 400.0));   // equal cost/54 chunks
    CHECK_FALSE(ps.slot.ready);
    tick(ps, 504.0, 1.0, t);                 // way past the end
    CHECK(ps.slot.ready);
    CHECK(ps.slot.stage == kProductionSteps);
    CHECK(ps.credits == doctest::Approx(9200.0));           // exactly cost deducted
    placed(ps);
    CHECK(ps.slot.type == -1);

    // stall: 100 credits fund 6 chunks of 800/54 (= 88.9); the 7th (14.8) is
    // unaffordable -> stalls with ~11.1 left, no progress past stage 6
    ps.credits = 100;
    start(ps, 1, t, 0.7);
    tick(ps, 1000.0, 1.0, t);
    CHECK(ps.slot.stage == 6);
    CHECK(ps.credits == doctest::Approx(100.0 - 800.0 * 6 / 54).epsilon(0.001));
    CHECK_FALSE(ps.slot.ready);
    ps.credits += 5000;                       // funds arrive -> resumes
    tick(ps, 1000.0, 1.0, t);
    CHECK(ps.slot.ready);
    CHECK(ps.credits == doctest::Approx(100.0 + 5000.0 - 800.0));  // total spend == cost
    placed(ps);

    // stall never produces a catch-up burst: broke for a long time, then
    // funded -> at most ONE stage per step_ticks afterwards
    ProductionState nb; nb.credits = 0;
    start(nb, 1, t, 0.7);
    tick(nb, 100.0, 1.0, t);                  // long broke stretch
    CHECK(nb.slot.stage == 0);
    nb.credits = 10000;
    tick(nb, 9.0 / 15.0, 1.0, t);             // exactly one step_ticks worth
    CHECK(nb.slot.stage == 1);                // NOT a burst to stage 11

    // cancel refunds spent
    ProductionState c; c.credits = 1000;
    start(c, 2, t, 0.7);                      // GAPILE: 315 frames, step_ticks 5
    CHECK(c.slot.step_ticks == 5);            // trunc(315/54)
    tick(c, 90.0 / 15.0, 1.0, t);             // 90 frames = 18 stages
    CHECK(c.slot.stage == 18);
    CHECK(c.credits < 1000.0);
    cancel(c);
    CHECK(c.credits == doctest::Approx(1000.0));
    CHECK(c.slot.type == -1);

    // half-speed multiplier halves stage cadence
    ProductionState s; s.credits = 10000;
    start(s, 1, t, 0.7);                      // step_ticks 9
    tick(s, 1.0, 0.5, t);                     // 7.5 frames < 9 -> no stage yet
    CHECK(s.slot.stage == 0);
    tick(s, 1.0, 0.5, t);                     // 15 frames -> one stage
    CHECK(s.slot.stage == 1);
}
```

- [ ] **Step 2:** Build → fails.
- [ ] **Step 3:** Implement `engine/production.cpp`:

```cpp
#include "production.hpp"
#include <algorithm>
#include <cmath>

namespace ra2engine {

int build_frames(int cost, double build_speed) {
    return (int)(cost / 1000.0 * build_speed * 900.0);   // [RE: construction.md §7]
}

PowerTotals power_totals(const std::vector<Building>& buildings,
                         const std::vector<BuildingTypeDef>& types, int house) {
    PowerTotals p;
    for (const Building& b : buildings) {
        if (!b.alive || b.house != house || b.state != Building::State::Idle) continue;
        if (b.type < 0 || b.type >= (int)types.size()) continue;
        int pw = types[b.type].power;
        if (pw >= 0) p.output += pw;
        else p.drain -= pw;
    }
    return p;
}

double build_speed_multiplier(const PowerTotals& p, double low_power_penalty_modifier) {
    if (p.drain <= p.output) return 1.0;
    // [RE-GATE: construction.md §8] f = output/drain; 1 - mod*(1-f), then
    // floor .5 and (low power only) cap .8.
    double f = (double)p.output / p.drain;
    double s = 1.0 - low_power_penalty_modifier * (1.0 - f);
    s = std::max(s, kMinLowPowerSpeed);
    s = std::min(s, kMaxLowPowerSpeed);
    return s;
}

bool prerequisites_met(const std::vector<std::string>& tokens,
                       const std::vector<Building>& buildings,
                       const std::vector<BuildingTypeDef>& types, int house,
                       const std::vector<std::pair<std::string, std::vector<std::string>>>& groups) {
    auto have = [&](const std::string& id) {
        for (const Building& b : buildings) {
            if (!b.alive || b.house != house || b.state != Building::State::Idle) continue;
            if (b.type >= 0 && b.type < (int)types.size() && types[b.type].id == id) return true;
        }
        return false;
    };
    for (const std::string& tok : tokens) {
        bool ok = have(tok);
        if (!ok) {
            for (const auto& g : groups) {
                if (g.first != tok) continue;
                for (const std::string& id : g.second)
                    if (have(id)) { ok = true; break; }
                break;
            }
        }
        if (!ok) return false;
    }
    return true;
}

bool can_start(const ProductionState& ps, int type,
               const std::vector<BuildingTypeDef>& types,
               const std::vector<Building>& buildings, int house,
               const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
               bool conyard_alive) {
    if (type < 0 || type >= (int)types.size()) return false;
    if (!conyard_alive || ps.slot.type != -1) return false;
    if (types[type].tech_level <= 0) return false;       // -1 = never buildable
    return prerequisites_met(types[type].prerequisite, buildings, types, house, groups);
}

void start(ProductionState& ps, int type, const std::vector<BuildingTypeDef>& types,
           double build_speed) {
    ps.slot = QueueSlot{};
    ps.slot.type = type;
    ps.slot.total_frames = std::max(1, build_frames(types[type].cost, build_speed));
    // [RE-GATE: construction.md §10 FUN_004C9DC0] frames per production step.
    ps.slot.step_ticks = std::clamp(ps.slot.total_frames / kProductionSteps, 1, 255);
}

void tick(ProductionState& ps, double dt, double speed_mult,
          const std::vector<BuildingTypeDef>& types) {
    if (ps.slot.type == -1 || ps.slot.ready) return;
    const BuildingTypeDef& t = types[ps.slot.type];
    ps.slot.frames_accum += dt * 15.0 * speed_mult;
    while (ps.slot.frames_accum >= ps.slot.step_ticks &&
           ps.slot.stage < kProductionSteps) {
        // [RE-GATE: construction.md §10] entering the next stage charges the
        // remaining balance split over the remaining steps.
        double charge = ((double)t.cost - ps.slot.spent) /
                        (kProductionSteps - ps.slot.stage);
        if (ps.credits < charge) {
            // Stall: no progress, no charge; retried next tick. Clamp the
            // accumulator so a funding gap never bursts through many stages.
            ps.slot.frames_accum = (double)ps.slot.step_ticks;
            return;
        }
        ps.credits -= charge;
        ps.slot.spent += charge;
        ps.slot.stage += 1;
        ps.slot.frames_accum -= ps.slot.step_ticks;
    }
    if (ps.slot.stage >= kProductionSteps) {
        // settle rounding: total spend is exactly cost
        ps.credits -= (double)t.cost - ps.slot.spent;
        ps.slot.spent = (double)t.cost;
        ps.slot.ready = true;
    }
}

void cancel(ProductionState& ps) {
    ps.credits += ps.slot.spent;                          // [RE: construction.md §10]
    ps.slot = QueueSlot{};
}

void placed(ProductionState& ps) { ps.slot = QueueSlot{}; }

}  // namespace ra2engine
```

- [ ] **Step 4:** Build; `-ts="*production*"` then FULL suite — PASS.
- [ ] **Step 5:** Commit (`feat(engine): production — build queue, credits, power (pure)`).

---

### Task 6: engine/sidebar — layout + hit-testing (pure)

**Files:**
- Create: `engine/sidebar.hpp`, `engine/sidebar.cpp`
- Modify: `cmake/ra2_sources.cmake` (`RA2_ENGINE_SOURCES` += `engine/sidebar.cpp`), `CMakeLists.txt` (`target_sources(ra2assets_tests PRIVATE tests/test_sidebar.cpp engine/sidebar.cpp)`)
- Test: `tests/test_sidebar.cpp`

**Interfaces:**
- Consumes: nothing engine-side (pure geometry).
- Produces:

```cpp
#pragma once
// Pure RA2 sidebar geometry + interaction states. All pixel metrics
// [RE: construction.md §11] — the constants below are pre-hunt assumptions
// from the art dimensions; the RE-GATE amends them before this task runs.
#include <string>
#include <vector>

namespace ra2engine {

struct SbRect { int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

// Input art dimensions, measured from the loaded SHPs at wire-up time (so
// the layout adapts to the real art rather than hardcoding every number).
struct SidebarArtDims {
    // Defaults = the REAL measured art [RE-GATE: construction.md §11]:
    // top 168x32, credits 168x16, four separate 28x27 tab SHPs (tab00-tab03,
    // 5 state frames each — NOT one strip divided by 4), side1 168x69,
    // side2 168x50, cameo 60x48, powerp pips 12x2 at 3 px pitch.
    int top_w = 168, top_h = 32;       // top.shp (radar frame)
    int credits_w = 168, credits_h = 16;
    int tab_w = 28, tab_h = 27;        // ONE tab button (tab00..tab03 each)
    int side1_h = 69, side2_h = 50;    // strip background segments
    int cameo_w = 60, cameo_h = 48;
    int power_w = 12;                  // powerp.shp pip width (12x2, 3 px pitch)
};

enum class CameoState { Available, Disabled, Building, Ready };

// One sidebar cameo cell: which building type it shows + its rect.
struct CameoCell { int type = -1; SbRect rect; };

struct SidebarLayout {
    SbRect bar;              // whole sidebar (right edge strip)
    SbRect radar;            // top.shp area
    SbRect credits;          // credits readout
    SbRect tabs[4];          // Building, Defense, Infantry, Vehicle
    SbRect power;            // vertical power meter strip
    std::vector<CameoCell> cameos;   // 2-column grid, row-major

    // view_w/view_h = window size; `types` = roster indices to show, already
    // filtered (cameo != "" AND tech_level > 0) and ordered by tech_level
    // then roster order.
    static SidebarLayout compute(int view_w, int view_h,
                                 const SidebarArtDims& art,
                                 const std::vector<int>& types);

    int cameo_at(int px, int py) const;   // index into cameos, or -1
    int tab_at(int px, int py) const;     // 0..3, or -1
    bool contains(int px, int py) const { return bar.contains(px, py); }
};

}  // namespace ra2engine
```

`compute` [RE-GATE: construction.md §11]: `bar = {view_w - art.top_w, 0, art.top_w, view_h}`; `radar` at bar top; `credits` directly below radar; the 4 `tabs` in one row below credits — four SEPARATE buttons at pitch `bar.w / 4` (= 42), each `tab_w`(28) wide centered in its slot: `tabs[i].x = bar.x + i*(bar.w/4) + (bar.w/4 - art.tab_w)/2`; `power` = `{bar.x, tabs_bottom, art.power_w, view_h - tabs_bottom}` (pip column at the strip's left edge, 3 px pitch); cameo grid 2 columns starting right of the power strip (`x = bar.x + art.power_w + 2`), rows descending from tabs_bottom + 2, cell pitch = cameo size + 1px gutter, one row-major cell per entry of `types`.

- [ ] **Step 1: Write the failing tests** (`tests/test_sidebar.cpp`):

```cpp
#include "doctest.h"
#include "../engine/sidebar.hpp"
using namespace ra2engine;

TEST_CASE("sidebar layout: geometry from art dims") {
    SidebarArtDims art;   // defaults
    auto lay = SidebarLayout::compute(1280, 720, art, {1, 2, 3});
    CHECK(lay.bar.x == 1280 - art.top_w);
    CHECK(lay.bar.w == art.top_w);
    CHECK(lay.bar.h == 720);
    CHECK(lay.radar.y == 0);
    CHECK(lay.credits.y == lay.radar.y + lay.radar.h);
    CHECK(lay.tabs[0].y == lay.credits.y + lay.credits.h);
    CHECK(lay.tabs[1].x == lay.tabs[0].x + lay.bar.w / 4);   // 42 px pitch, 28 wide
    CHECK(lay.tabs[0].w == art.tab_w);
    REQUIRE(lay.cameos.size() == 3);
    CHECK(lay.cameos[0].type == 1);
    // 2-column grid: cell 0 and 1 same row, cell 2 next row
    CHECK(lay.cameos[0].rect.y == lay.cameos[1].rect.y);
    CHECK(lay.cameos[2].rect.y > lay.cameos[0].rect.y);
    CHECK(lay.cameos[2].rect.x == lay.cameos[0].rect.x);
    // power strip hugs the bar's left edge below the tabs
    CHECK(lay.power.x == lay.bar.x);
    CHECK(lay.power.y == lay.tabs[0].y + lay.tabs[0].h);
}

TEST_CASE("sidebar hit-testing") {
    SidebarArtDims art;
    auto lay = SidebarLayout::compute(1280, 720, art, {1, 2});
    auto c0 = lay.cameos[0].rect;
    CHECK(lay.cameo_at(c0.x + 1, c0.y + 1) == 0);
    CHECK(lay.cameo_at(c0.x - 200, c0.y) == -1);         // in the world, not the bar
    CHECK(lay.tab_at(lay.tabs[2].x + 1, lay.tabs[2].y + 1) == 2);
    CHECK(lay.tab_at(0, 0) == -1);
    CHECK(lay.contains(1280 - 1, 0));
    CHECK_FALSE(lay.contains(1280 - art.top_w - 1, 0));
}
```

- [ ] **Step 2:** Build → fails.
- [ ] **Step 3:** Implement `engine/sidebar.cpp` exactly per the compute rule above (straightforward rect arithmetic; `cameo_at`/`tab_at` are linear scans).
- [ ] **Step 4:** Build; `-ts="*sidebar*"` + FULL suite — PASS.
- [ ] **Step 5:** Commit (`feat(engine): sidebar layout + hit-testing (pure)`).

---

### Task 7: Wiring — assets, draw, placement input, pre-built base, --demo-base

**Files:**
- Modify: `engine/main.cpp`, `engine/input.hpp`, `engine/input.cpp`
- Test: `tests/test_input.cpp` (append placement-mode cases); the integration gate is `--demo-base`

**Interfaces:**
- Consumes: everything from Tasks 2–6.
- Produces: `BuildingAssets` (main.cpp struct), `--demo-base`, placement mode on `Input`.

**7a. Input placement mode** (`input.hpp` additions):

```cpp
// --- Buildings phase: placement mode -----------------------------------
// While active, left-click confirms placement at the hovered foundation
// origin cell (reported via InputActions), right-click/Esc cancels back to
// READY. Selection/attack/move gestures are suppressed while placing.
void begin_placement(int roster_type, int fw, int fh);
void cancel_placement();
bool placement_active() const { return place_type_ >= 0; }
int placement_type() const { return place_type_; }
// Hovered foundation ORIGIN cell (screen_to_cell of the cursor, minus half
// the foundation so the cursor sits near the foundation's center).
void placement_cell(int& cx, int& cy) const;
```

`InputActions` gains `bool place_confirmed = false; bool place_cancelled = false; int place_cx = 0, place_cy = 0;`. In `Input::handle`: when `placement_active()`, a Left Down+Up (no drag) sets `place_confirmed` + the cell; Right click or Escape key sets `place_cancelled` and clears placement state; all other gestures are swallowed. main.cpp routes sidebar-rect clicks BEFORE `Input::handle` (the sidebar consumes them), so Input never needs to know the sidebar exists.

Placement-cell math: `screen_to_cell(mouse - camera offset)` exactly like the existing right-click order path (copy its two-pass level-aware resolution), then `cx -= fw/2; cy -= fh/2;`.

Append to `tests/test_input.cpp` (this file already builds Worlds + fake InputEvents — reuse its helpers):

```cpp
TEST_CASE("input: placement mode confirms at hovered origin and suppresses orders") {
    // fixture: world + camera at origin, view 800x600 (existing helper)
    // input.begin_placement(1, 2, 2);
    // CHECK(input.placement_active());
    // feed Left Down + Up at a pixel whose screen_to_cell is (10, 10)
    // auto acts = input.take_actions();
    // CHECK(acts.place_confirmed);
    // CHECK(acts.place_cx == 10 - 1); CHECK(acts.place_cy == 10 - 1); // -fw/2
    // CHECK_FALSE(acts.selected);         // click did NOT select
    // input.begin_placement(1, 2, 2);
    // feed Right Down + Up -> take_actions().place_cancelled == true
    // CHECK_FALSE(input.placement_active());
}
```

**7b. Building assets** (main.cpp, after `VehicleAssets` — same guarded-load shape):

```cpp
// All SHP + palette assets for one building type, resolved through the
// NewTheater candidate list [RE: building-render.md §1] over the already-
// open ra2.mix nested mixes. ok=false drops the type from the roster
// (guarded degrade, never crash).
struct BuildingArt {
    ra2assets::ShpFile shp;        // main image (frame 0 healthy, 1 damaged)
    ra2assets::ShpFile buildup;    // MK anim ("" buildup -> unused)
    ra2assets::ShpFile cameo;      // 60x48 icon (may be absent for ConYards)
    bool has_buildup = false, has_cameo = false, ok = false;
};

struct BuildingAssets {
    std::vector<ra2engine::BuildingTypeDef> roster;   // index = Building::type
    std::vector<BuildingArt> art;                     // parallel to roster
    ra2assets::Palette unit_pal;                      // unittem.pal (cache.mix)
    ra2assets::Palette cameo_pal;                     // cameo.pal (cache.mix)
    // sidebar chrome per house side (0 = sidec01 Allied, 1 = sidec02 Soviet)
    // [RE-GATE: construction.md §11] four SEPARATE tab SHPs + the 55-frame
    // gclock2.shp build clock (frame = 1 + production stage).
    ra2assets::ShpFile sb_top, sb_side1, sb_side2, sb_tabs[4],
                       sb_powerp, sb_credits, sb_gclock;
    bool sidebar_ok = false;
    bool ok = false;                                  // any usable building
};
```

`load_building_assets(MixFile& ra2, MixFile& language, const Rules& rules, const Art& art)`:
roster = the 8 ids in this fixed order: `{"GACNST","GAPOWR","GAPILE","GAWEAP","NACNST","NAPOWR","NAHAND","NAWEAP"}`; per id fill `BuildingTypeDef` from Rules/Art (foundation, strength, `armor_index(rules.armor(id))`, cost, power, adjacent, tech_level, is_construction_yard, prerequisite, buildup, cameo, explosion_first). Per id resolve the main SHP via `theater_candidates(id, 'T', "isotemp.mix", "temperat.mix", ".shp")` walked over ra2.mix's nested mixes (open each candidate mix once, `find` the file; first hit wins); buildup likewise with the MK name. Cameo from `language.mix/cameo.mix/<lowercase cameo>.shp`. Sidebar chrome from `ra2.mix/sidec01.mix` (player side; Soviet sidec02 is NOT loaded this phase — the player is Allied): `top.shp`, `side1.shp`, `side2.shp`, `tab00.shp`–`tab03.shp` (four separate 28×27 buttons), `powerp.shp`, `credits.shp`, and `gclock2.shp` (60×48, 55 frames — the build clock) [RE-GATE: construction.md §11]. Every load individually try/caught → log one `[building] <id> ... missing` line, `art[i].ok = false`. Palettes: `unittem.pal` + `cameo.pal` from `ra2.mix/cache.mix` (both verified present); sidebar chrome + gclock draw with `cameo.pal`'s sidebar sibling — use `sidebar.pal` if present in cache.mix, else `cameo.pal` (the hunt's §11 pixel-decoding used sidebar.pal; probe at load, guarded).

Prerequisite groups resolved once: `groups = {{"POWER", rules.prerequisite_group("Power")}, {"BARRACKS", ...("Barracks")}, {"PROC", ...("Proc")}, {"FACTORY", ...("Factory")}, {"RADAR", ...("Radar")}, {"TECH", ...("Tech")}}`.

**7c. Spawning.** `spawn_base(World&, BuildingAssets&, int house, int cx, int cy)`: ConYard (GACNST/NACNST per house) at (cx, cy) instant + validity-bypassed; for house 1 additionally NAPOWR at (cx+6, cy), NAHAND at (cx+6, cy+4), NAWEAP at (cx-1, cy+6) — each via `placement_check(..., bypass_adjacency=false)` against the growing base, `instant=true`; if a scripted cell fails validity, probe (cx±k) eastward up to k=6 and log. `buildup_total` at real placement [RE-GATE APPLIED: building-render.md §5] = `image_frames * max(1, (int)(kBuildupTimeMinutes * 900.0 / image_frames)) / 15.0` seconds, with `kBuildupTimeMinutes = 0.06` (rulesmd `[General] BuildupTime=.06`; read it via `rules.general_double("BuildupTime", 0.06)`). GACNSTMK (29 image frames) ≈ 1.93 s; a 25-frame MK ≈ 3.33 s. Interactive/screenshot + `--demo-base` spawn: player ConYard + the existing GI squad; enemy full base + Conscript squad. **`--demo-combat`, `--demo-tank`, and the other demos spawn NO buildings.**

**7d. Draw.** Buildings render in the world pass with units: build a merged draw list — for each alive building, depth key `(cx + (fw-1)/2.0) + (cy + (fh-1)/2.0)` [RE-GATE APPLIED: building-render.md §3 — Center_Coord uses (f−1)/2, NOT f/2] merged into `unit_draw_order`'s `(fx+fy)` space (std::merge two sorted lists by key; buildings before units on ties). Per building: texture = `TextureCache::get("bld:<id>:<house>:<frame>", indices, w, h, pal_for_house(unit_pal, house))`; screen anchor [RE-GATE APPLIED: building-render.md §2 — CENTER–center registration, replacing the pre-hunt bottom-center rule]: compute the foundation's screen bbox as the union of `cell_to_screen(cx+i, cy+j, level, 60, 30, kLevelPx)`-anchored 60×30 tile rects over all foundation cells, then `px = bbox_center_x - canvas_w/2`, `py = bbox_center_y - canvas_h/2`. State::Buildup draws the buildup SHP frame (`buildup_frame`); Dead draws the explosion anim (reuse the vehicle death-anim path: TWLT070 frames over `kBuildingDieSecs` via the existing explosion texture helper). Damaged frame via `building_frame(b, count, cond_yellow/100.0)`. HP bar + selection box reuse the unit pattern, sized to the foundation's screen extent (buildings are right-click targets, not selectable).

**7e. Sidebar draw + interaction** (main.cpp, screen-space pass after the world):
- `SidebarArtDims` filled from the loaded SHPs' `width()`/`height()` (tab_w = tab00 width / 4 if the strip is one image [RE: construction.md §11 — the hunt says which]; pre-hunt: tab00.shp frame per tab).
- Buildable list: roster indices with `cameo != "" && tech_level > 0 && art[i].ok`, ordered tech_level then roster order → `{GAPOWR, GAPILE, GAWEAP}` for the Allied player.
- Per frame: draw side1/side2 tiled down the bar, top.shp radar frame (static frame 0), credits number (right-aligned in the credits rect), the four tab buttons (tab00–tab03, frame 0; the active Building tab drawn full-bright, the other three shaded with the existing shade helper — the per-state frame mapping beyond idle is the §11 [RE-FALLBACK], so frame 0 + shade is the documented choice), power meter [RE-GATE APPLIED: construction.md §8 pip model]: `total_pips = power.h / 3` (3 px pitch); `pips_used = total_pips - (int)(total_pips * 400.0 / (output + drain + 400.0))` (the soft 400 capacity scale); from the BOTTOM up draw `red = round(pips_used * drain / max(output+drain,1))` pips with powerp frame 3, then yellow = the pips covering surplus up to 100 power (frame 2), then green for the rest of pips_used (frame 1), empty (frame 0) above; while `drain > output` redraw the used pips with flash frame 4 on alternating 3-tick periods. Cameo grid per-state overlay [RE-GATE APPLIED: construction.md §11 — gclock2.shp replaces the pre-hunt fill bar]: Disabled = 55% black shade (existing shade helper); Building = the gclock2.shp frame `1 + slot.stage` drawn over the cameo (60×48, aligned; its palette-index-0 transparency shows the cameo through the swept region) + the shade under it; Ready = 2 Hz brightness flash + "READY" text.
- Credits digits: **built-in 5×7 digit strip** (a `static const uint8_t kDigits[10][7]` bitmap rendered via SDL rects, white) — documented divergence UNLESS Task 1 §11 found the game font trivially extractable, in which case the RE-GATE amends this step. The DISPLAYED number rolls toward the true credit total (RA2's tally animation): `shown += clamp(true - shown, -rate*dt, rate*dt)` with rate = max(500/s, |true−shown|/0.5s); display-only, never read by the sim.
- Event routing in the main loop: for each mouse event, if `lay.contains(px, py)` → sidebar handling (cameo click: Available → `start`; Building → `cancel`; Ready → `begin_placement`; tab clicks: only tab 0 does anything (no-op — it's already active)); else forward to `Input::handle`. Keyboard Escape while placing → `cancel_placement` (input handles).
- Placement cursor: while `placement_active()`, per frame run `placement_check(...)` at `placement_cell` and draw the fw×fh cell diamonds green/red (per-cell `cell_ok` AND overall adjacency → all-red tint when adjacency fails); on `place_confirmed`: re-run check, if ok → `World::add_building(type, ..., buildup_total, /*instant*/false)` + `placed(ps)` + play the placement sound [RE: building-render.md §5 sound name — pre-hunt "PlaceBuilding"] via the existing AudioSystem; if not ok → keep placing (RA2 behavior: invalid click just bonks).
- Per-frame sim glue (headless-safe, NO SDL): `ps_tick`: `auto pt = power_totals(world.buildings(), ba.roster, 0); tick(ps, dt, build_speed_multiplier(pt, low_mod), ba.roster);` ConYard-death check: when no alive Idle own ConYard exists and `slot.type != -1` → `cancel(ps)`.

**7f. `--demo-base`** (new mode, cloned from `--demo-tank`'s skeleton): fixed seed-free script driven by frame count at 15 Hz fixed dt; player ConYard at the start cell, enemy base across the map, 3 GI + 3 Conscripts for scenery. Script: frame 15 `start(GAPOWR)`; on `slot.ready` → place at ConYard east offset via `placement_check` (assert ok); then `start(GAPILE)` → place; then `start(GAWEAP)` — **assert `can_start == false`** (PROC gate); then order the GIs to attack the enemy NAPOWR (`order_attack_building`) and run until it dies. Witness lines (printed in BOTH rendered and headless runs, byte-compared like demo-tank):

```
[base] roster=8 buildable=GAPOWR,GAPILE sidebar=ok
[base] t=1.0 start GAPOWR frames=504 step_ticks=9
[base] placed GAPOWR cell=(x,y) credits=9200 power=+200/-0
[base] placed GAPILE credits=8700 power=+200/-10
[base] GAWEAP blocked (PROC unmet)
[base] NAPOWR destroyed t=...
[base] hash=...
```

(exact cell numbers come from the map start cell — print, don't hardcode). The final `hash=` line reuses the demo-tank world-state FNV hash helper extended with building `(type,cx,cy,hp,state)` tuples.

- [ ] **Step 1:** Input placement mode + `tests/test_input.cpp` cases (write test → fail → implement → pass). Commit (`feat(engine): placement mode input`).
- [ ] **Step 2:** `load_building_assets` + startup log line `[building] roster=8 ok=<n> sidebar=<ok|missing>`; verify interactively from PowerShell: `.\build\ra2engine.exe --screenshot` shows the log and still renders. Commit.
- [ ] **Step 3:** `spawn_base` + building draw pass (buildings visible in `--screenshot`; take a scratchpad screenshot and EYEBALL against the original game's look before proceeding). Commit.
- [ ] **Step 4:** Sidebar draw + interaction + placement cursor + sim glue; interactive sanity run (build a power plant end-to-end by hand). Commit.
- [ ] **Step 5:** `--demo-base` (rendered vs headless byte-compare loop from demo-tank, both run from PowerShell); pin the witness output. Commit.
- [ ] **Step 6:** Byte-identity gates: `--demo-combat` and `--demo-tank` hashes unchanged. FULL suite both modes (with and without `RA2_INSTALL_DIR`). Commit any fixes.

---

### Task 8: Finalize — arm64, README, gates, ledger

**Files:**
- Modify: `README.md` (divergences + phase paragraph), `.superpowers/sdd/progress.md`

- [ ] **Step 1:** arm64: `$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"; cd android; .\gradlew assembleDebug` → BUILD SUCCESSFUL; `llvm-nm --demangle` spot-check that `placement_check` and `SidebarLayout::compute` symbols are in `libmain.so`.
- [ ] **Step 2:** README: buildings paragraph + divergence list verbatim from the spec §Documented divergences (idle anims, repair/sell, rubble, shroud, radar, credits font digit-strip, inert tabs, GAWEAP PROC-blocked, no auto-acquire of buildings), PLUS the RE-GATE additions: power output does NOT scale with building health (vanilla scales output by hp ratio — construction.md §8 optional-fidelity item); Bib=yes bib art/row omitted (GAWEAP renders without its bib — building-render.md §2); tab buttons drawn frame-0 + shade (per-state frames are the §11 [RE-FALLBACK]); main-SHP damaged frame 2 (second damaged variant) unused.
- [ ] **Step 3:** FULL suite with and without `RA2_INSTALL_DIR` (PowerShell); `--demo-combat` + `--demo-tank` + `--demo-base` all green/pinned.
- [ ] **Step 4:** Ledger entry + commit (`docs: buildings phase README + ledger`).

---

## Self-review notes (spec coverage)

- Spec "full ConYard loop" → Tasks 5 (queue/credits/power) + 7e (sidebar interaction) + 7f (demo).
- Spec "8-building roster both sides" → Task 7b roster; Soviet art exercised via the pre-built base (7c).
- Spec "theater resolver + verified locations" → Task 2 (+ real-install pins).
- Spec "foundations/occupancy/damage states/buildup" → Tasks 3–4, draw in 7d.
- Spec "full RA2 sidebar, core live" → Task 6 (geometry) + 7e (draw/interaction); radar static, repair/sell inert (drawn as part of side art, no hit targets — matches "Core" scope).
- Spec "pre-built enemy base" → 7c. Spec "placement cursor green/red" → 7e. Spec "GAWEAP PROC-blocked" → Task 5 tests + 7f witness.
- Spec error handling → 7b guarded loads. Spec testing matrix → Tasks 2–7 tests + 7f gates + Task 8.
- Type consistency check done: `placement_check`/`foundation_cells`/`kill_building`/`step_buildings` (T3) consumed by T4/T7; `ProductionState`/`start`/`tick`/`cancel`/`placed`/`can_start`/`power_totals`/`build_speed_multiplier` (T5) consumed by 7e/7f; `SidebarLayout::compute`/`cameo_at`/`tab_at` (T6) consumed by 7e; `theater_candidates` (T2) consumed by 7b; `add_building(type, cx, cy, fw, fh, house, strength, armor_ix, buildup_total, instant)` (T4) consumed by 7c/7e.
- Known deliberate deferrals to the RE-GATE: §2 SHP registration formula (7d), §5 buildup rate + placement sound name, §7 build_frames constant, §8 penalty formula + kLowPowerFloor, §9 adjacency measurement, §11 sidebar metrics + clock-sweep vs fill-bar + credits font.
