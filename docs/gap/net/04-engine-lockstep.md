# N4: Engine readiness for lockstep multiplayer (our builds, cross-platform)

Research only. No engine code was changed. Base: `master` @ `43f4128`. Branch `net4`.

Scope: can two or more of OUR builds (Windows x86-64 MinGW/GCC, Android arm64 clang/bionic,
Android x86_64 emulator) run one game in lockstep? We check the input → command path,
determinism, multi-human support, sync hashing / state transfer, and performance.
Compatibility with the original game's protocol is covered by N1-N3.

## TL;DR

| Area | Verdict |
|---|---|
| Fixed-tick sim | **Mostly ready.** The sim already advances in fixed 1/15 s logic ticks (`game_scene_controls.inc:359`, `main.cpp:6368-6380`). It is driven by a wall-clock accumulator that drops ticks under load (`kMaxLogicSteps = 4`), so it needs a network-driven tick source. |
| Same-binary determinism | **Ready.** Gate hashes, save→load→continue (`--demo-saveload`) and input replay all reproduce. I re-checked this: two 10-minute `--ai-soak --ai-vs-ai` runs gave identical output (0 differing lines). |
| Cross-platform determinism | **NOT ready (blocker).** The sim calls libm transcendentals (`sin/cos/atan2/acos/hypot/pow`). **I measured these on Windows (UCRT) and the Android x86_64 emulator (bionic). They differ by 1 ulp on 0.04-17 % of inputs. `hypot` differs on 17 %.** `sqrt` is bit-identical. Positions are `double` and are hashed, so a Windows-vs-Android game would desync within seconds of the first projectile or aircraft. |
| Command path | **NOT ready (blocker).** Player actions mutate `World` directly, at frame time (not tick time), from `Input::handle`, hotkeys, sidebar clicks and the per-frame tail of `GameScene::update`. About 35 distinct call sites. No command object or queue exists. |
| Multi-human | **NOT ready (blocker).** The human economy (wallet, 6 production queues, backlog, hold) lives in `GameScene`, not in `World`, and is hard-wired to house 0. Selection is a sim field (`Unit::selected`) that is serialized and hashed. `World::controllable()` means "house 0 or a PlayerControl house" (one local player). |
| Sync hash / state transfer | **Good foundation.** `save_world` serializes the complete World with bit-exact doubles (`savegame.hpp:13-14,135`). `fnv1a(save_world(w))` is already the saveload gate's full-state hash. Saves are 0.5-0.67 MB and gzip to 9-19 KB, so late join and resync by state transfer is cheap. Scene-held state must move into the sim first. |
| Shell | **Nothing exists.** The Network and Internet menu buttons are disabled stubs (`main.cpp:8297,8317-8318`). There is no socket code in the tree. The Android manifest declares no permissions at all, so `INTERNET` is missing (`AndroidManifest.xml:31`). The skirmish lobby is one human plus 7 AI rows (`skirmish_lobby.hpp:30`). |
| Performance | **OK on desktop. Marginal on low-end Android in big fights.** About 1.2-1.6 ms per tick for a 1v1 AI-vs-AI soak on desktop (measured). 8 players run at 290-400 ticks/s headless (R-172). The emulator shows `update` at 25-41 ms per frame in the ALL06 battle (W6-06-18). Lockstep runs at the speed of the slowest peer. |

**Biggest risks, in order:**
1. libm divergence across platforms (measured).
2. The human economy lives outside the World and is tied to house 0.
3. UI mutates the World directly at frame time.
4. Selection is part of the sim state.
5. Android backgrounding stalls the lockstep for everyone.
6. A pointer-keyed path memo could serve a stale path after a resync load in the same process.
7. Float→int conversion of out-of-range values behaves differently on arm64 and x86.

---

## (a) Input → command path

### What exists

* **Driver** `run_scene_driver` (`main.cpp:5366-5490`). Each frame it polls events into `scene->handle(ev)`, then calls `scene->update(dt)` with wall-clock dt (`main.cpp:5443-5446`), then renders.
* **Fixed logic tick** inside `GameScene::update` (`main.cpp:6290-6460`). `logic_acc_ += dt * speed_mult`. While the accumulator holds at least `kLogicDt` (1/15 s), it runs one logic body: campaign triggers, `world_.step(dt)`, the presentation logic tick, `prod_tick` and the unit exit handoff. At most `kMaxLogicSteps = 4` steps run per frame, and the remainder is **dropped** (`main.cpp:6375`, constants at `game_scene_controls.inc:359-360`).
* **Input replay (R-184 / W5-F)** `InputRecorder` / `InputReplay` (`app_perf.inc:159-241`). This records **raw SDL events plus per-frame dt**, not commands. Replay depends on canvas size, camera and hit-testing. Arrow-key scrolling reads `SDL_GetKeyboardState` (`main.cpp:6308-6312`) and is not recorded. So it proves same-binary determinism but is **not portable across devices**. The end-of-run fingerprint is `replay_report` → FNV over `scene->probe()` (`app_perf.inc:244`).
* **`--play` harness**: it scripts events through the same `EventSource`, so it uses the same UI path.
* **Save/load**: `capture_save`/`apply_save` (`main.cpp:6774+`) and `save_world`/`load_world` (`savegame.cpp:23-40`).

### Where player actions become world changes (bypass list)

None of these go through a command object. All of them run at **frame** time, which is not the same as tick time: before, between or after 0-4 logic ticks, depending on the machine's frame rate.

| # | Action | Call site | Mutation |
|---|---|---|---|
| 1 | Select / box select / deselect / type-select / group recall / next-prev object | `input.cpp` (8× `.selected =`, 6× `clear_selection`), `input_original.cpp` (4+4), `game_scene_controls.inc:134-150` (`cycle_object`) | writes `Unit::selected` (a **sim** field) |
| 2 | Move (ground right-click, radar click) | `input.cpp:573,781`, `input_original.cpp:335`, `game_scene_controls.inc:574` | `World::order_move_selected` |
| 3 | Attack unit / building / cell (force-fire) | `input.cpp:505,770`, `input_original.cpp:309,315,316` | `order_attack`, `order_attack_building`, `order_attack_cell_selected` |
| 4 | Harvest / return to refinery | `input.cpp:546,592` | `order_harvest`, `order_return` |
| 5 | Enter transport / occupy garrison | `input.cpp:638,670` | `order_enter_transport`, `order_occupy_selected` |
| 6 | Engineer capture / defuse / spy infiltrate | `input.cpp:746,617,728,687` | `order_engineer_capture`, `order_defuse`, `order_spy_infiltrate` |
| 7 | Set primary factory / rally point | `input.cpp:367,483` | `set_primary`, `set_rally` |
| 8 | Stop / Guard / Scatter hotkeys | `input_original.cpp:479-492` | `order_*_selected` |
| 9 | Deploy / unload (key D, click on self) | `game_scene_controls.inc:115-131` | `unload_occupants`, `order_unload_selected`, `order_deploy` |
| 10 | Repair / Sell / Power toggle click | `main.cpp:6492-6494` | `toggle_repair(b,0)`, `begin_sell(b,0)`, `set_building_power` |
| 11 | Place building | `main.cpp:6496-6505` → `realplay_scene.inc:47-80` `place_building` | `add_building(...,house 0,...)`, wall line fill |
| 12 | Superweapon target | `main.cpp:6507` → `w2air_play.inc:358-375` | `fire_superweapon_at` |
| 13 | Start / queue / hold / resume / cancel production (cameo L/R/Shift) | `game_scene_controls.inc:397-475` | scene-owned `queues_`, `backlog_`, `held_`, `player_credits_` |
| 14 | Unit exit from factory | `realplay_scene.inc:294-318` `rp_launch_ready_units` | `launch_unit`, `make_harvester` (scene-driven, house 0) |
| 15 | Free units on building open | `main.cpp:2292` `spawn_free_units_on_open` (scene `miners_`, `bstates_`) | spawns units |
| 16 | Victory cheer | `main.cpp:6518-6520` | `make_house_cheer(0)` from presentation logic (each client would cheer *its own* house) |
| 17 | "Player ordered" release from script teams | `input*.cpp` `mark_player_order` | `player_orders_` |
| 18 | Repair wallet bridge / spy wallet hint | `main.cpp:6386-6392,6408` | `deposit_credits(0, …)`, `set_wallet_hint(0, …)` |
| 19 | Pause via options menu, briefing, movie | `main.cpp:6296-6305` | the sim does not advance (local only) |
| 20 | Cheats / probes (`--play`) | `pres_play_cheats.inc`, `play_art_cheats.inc`, `w2air_play.inc:408` | direct `add_building` etc. (debug only; must be compiled out of netplay) |

`ToggleAlliance`, `PlaceBeacon`, `PageUser` and taunts are explicitly unimplemented "network-only" commands (`game_scene_controls.inc:200-208`).

**Conclusion.** Every action *can* be expressed as a serializable command. The `World` order API already takes plain ints (unit/building indices, cells), and there are no pointers in arguments. What is missing:

1. A command type and a queue keyed by tick.
2. Commands that carry an explicit **issuer house plus a unit-id list** instead of "the current selection".
3. Moving the scene-held production and economy logic into the sim, so production commands have something deterministic to act on.

Unit and building references are vector indices. Dead units are never compacted (wave2/03 notes "dead units are never removed from World vectors"), so indices are stable ids for the life of a game. That is good for commands, but each command must check `alive` and ownership at apply time.

## (b) Determinism

### Already good (evidence)

* **Compiler flags.** No `-ffast-math` anywhere. `-ffp-contract=off` is set on both builds (`CMakeLists.txt:363`, `android/app/jni/CMakeLists.txt:66-70`, "Q3"). x86-64 uses SSE2 (no x87 excess precision). arm64 is `FLT_EVAL_METHOD 0`. The CMake comment says gate hashes are identical across -O0/-O2/-O3 (`CMakeLists.txt:8-9`).
* **RNGs** are seeded members of `World` and serialized: `proj_rng_`, `combat_rng_{0x2545F491}`, `crate_rng_`, `ore_rng_`, `bridge_rng_` (`world.hpp:1499,2154,2494,2567,2608`). There is no `rand()`, `<random>` or `random_device` in sim code. Presentation has no RNG path into the sim.
* **Presentation is read-only.** The `pres_*` entry points take `const World&` (`pres_game.inc:142,217`, `pres_audio_events.inc:110,474,487`). `World` has no `extern` globals and no `std::function` hooks.
* **Ordering.** Sorts use total orders with an index tie-break: `OpenNodeGreater` for A* (`pathfind.cpp:34-40`), `world.cpp:5428-5434`, `combat.cpp:184` (pair with index), `ai_base.cpp:71` (`stable_sort`). That makes them safe across libstdc++ and libc++. `unordered_map` appears only in `aimd_parse.cpp` (lookup only) and the path memo (lookup only).
* **Threads** (`std::thread`: voxel prewarm `main.cpp:7434-7445`, audio mixer, music stream, Bink worker `bink_async.hpp`, os_profiler) never touch `World`.
* **Uninitialised data.** A scan of sim headers found only aggregate-initialised event structs (`world.hpp:2062-2065`, `combat.hpp:103-105`). Any stray padding or uninitialised field would show up in `save_world` bytes, and the saveload gate already catches that.

### Cross-platform risks

| Risk | Evidence | Severity |
|---|---|---|
| **libm transcendentals differ between UCRT (MinGW) and bionic** | Sim uses: `world.cpp:59,82` (atan2, fmod), `:368` (sinf table), `:3171` (pow), `:5851-5852` (sin/cos); `world_an.cpp:329,387,639-640`; `combat.cpp:166-167`; `projectile.cpp:68` (acos), `:91,164-165,200-202,269,279-310`; `rocket.cpp:133-140,219-239`; `ai_team.cpp:55-56`; ~40 `std::hypot` uses. **Measured** with `docs/gap/net/04-libm-probe.cpp` (1,000,000 inputs per function in sim ranges, `-O2 -ffp-contract=off`; GCC 16.1 UCRT vs NDK r27c clang, API 24, emulator Android 11 x86_64): see the table below. | **BLOCKER** |
| GCC vs clang constant folding of libm calls | GCC folds `sin(const)` with MPFR (correctly rounded). Clang/bionic may fold differently or not at all, so constant expressions can disagree too. | high (covered by the same fix) |
| arm64 vs x86_64 *bionic* | Same msun C source, but arm64 libm may use FMA internally. **Not measured: there is no arm64 device on this box.** Run the probe on a phone: `aarch64-linux-android24-clang++ -O2 -ffp-contract=off 04-libm-probe.cpp`. | unknown |
| Float→int of out-of-range / NaN | x86 `cvttsd2si` gives `0x80000000`; arm64 `fcvtzs` saturates and NaN gives 0. `static_cast<int>(double)` appears 133× in `world.cpp`, 26× in `combat.cpp`, 29× in `world_an.cpp`, plus `ability_ftol` (`abilities.hpp:17`). Harmless only while values stay in range. | medium |
| Pointer-keyed path memo | `MemoKey` uses `leak`/`level`/`deck` **pointers** as identity (`pathfind.cpp:302-345`), and the memo is `thread_local` and outlives a World (`pathfind.cpp:347`). After a resync `load_world` or a rematch in the same process, a freed-then-reused address with different content could give a stale hit. The grid itself is memcmp-validated; the leak mask and terrain level are not. This also makes the peer that loaded state differ from peers that did not. | medium |
| Wall-clock seeds | Crate RNG seed is `SDL_GetTicks() ^ 0x52413252` for skirmish (`main.cpp:5529`). The lobby random country/colour/start uses `LobbyRng(ctx.seed)` (`skirmish_lobby.cpp:315`). Both must be a host-chosen session seed. | high (easy fix) |
| Frame-time application of orders | See (a). Orders land between ticks at frame boundaries, which vary per device. | **BLOCKER** (fixed by the command queue) |
| Tick drop under load | `logic_acc_ = 0.0` after 4 steps (`main.cpp:6375`). In lockstep, ticks must never be dropped; the game slows down instead. | high |
| Local-only pause | Options, save dialog, briefing and movies freeze the sim (`main.cpp:6296-6305`). An Android background event opens the menu and autosaves (`game_scene_lifecycle.inc:19-28`). | high |
| Data mismatch | Rules, art and AI come from each user's install (Android imports via SAF). Different `rulesmd.ini`/`artmd.ini`/`aimd.ini`/map bytes or engine version means instant desync. | high (handshake hash) |
| `inf_human_` single house | Infantry fear-drop exemption is for one "human" house (`world.hpp:820-824`, `world.cpp:5335`). | low (needs a mask) |

#### libm probe results: Windows (UCRT) vs Android x86_64 (bionic)

Each value is the fraction of 1,000,000 inputs whose result differs. The maximum difference is 1 ulp for every function except fmod.

| Function | Inputs that differ | Max difference |
|---|---|---|
| sin | 1.57 % | 1 ulp |
| cos | 1.70 % | 1 ulp |
| atan2 | 0.22 % | 1 ulp |
| acos | 7.71 % | 1 ulp |
| hypot | **17.05 %** | 1 ulp |
| pow | 0.06 % | 1 ulp |
| fmod(atan2·k) | 0.04 % | 32 ulp (propagated) |
| sinf | 1.35 % | 1 float ulp |
| sqrt | **0 %** | identical |

Basic IEEE operations (`+ - * /`, `sqrt`, `floor`/`trunc`/`lround`) are bit-exact on both platforms with contraction off.

### Recommended fixes (determinism)

1. **Deterministic math library (required, small).** Add `engine/dmath.hpp` with `dm::sin/cos/atan2/acos/hypot/pow/sinf` written in plain double arithmetic. Options:
   * Port the FreeBSD msun kernels (`k_sin`, `k_cos`, `e_atan2`, `e_acos`, `e_pow`). This is BSD-licensed and is what bionic runs. Compiled into our binary with `-ffp-contract=off`, it gives identical bits everywhere.
   * Or use the original game's table approach. RA2 uses fixed-point facings and lookup tables; N3 may recover these, which would also improve fidelity.

   Replace every sim call. Replace `hypot(x,y)` with `sqrt(x*x+y*y)`, which is exact-IEEE and portable, unless overflow matters (it does not at map scale). Enforce it with a CI grep: no `std::sin|cos|tan|atan|acos|asin|pow|exp|log|hypot` in `engine/world*.cpp`, `combat`, `projectile`, `rocket`, `ai_*`, `harvest`, `flight`, `cargo`, `building_combat`, `pathfind`. Presentation code may keep libm. Note that this **changes gate hashes once** (expected; re-pin).
2. **Keep doubles. Do not port to fixed point now.** Once libm is removed, IEEE double with SSE2/NEON and no contraction is reproducible. Fixed point is a large rewrite with little extra benefit. Keep `-ffp-contract=off` and add `-fno-fast-math -fexcess-precision=standard` defensively. On GCC also add `-fno-builtin-sin` (etc.) or rely on dmath, so that constant folding cannot differ.
3. **Checked conversions.** Add `dm::to_int(double)` that clamps and maps NaN to 0, and use it in sim code where the input can be out of range. A debug assert lets an x86 build catch out-of-range values.
4. **Path memo.** Key by content (World id + version counters) instead of addresses, or clear all memos in `World` construction and `load_world`.
5. **Session seed.** The host picks a `uint32 session_seed` in the lobby, used for crates, lobby random resolution and any future random start positions.
6. **Cross-platform CI gate.** Build the headless gates as an Android x86_64 executable (NDK, no Java), push to the emulator with `adb`, run `--demo-*` and `--ai-soak`, and compare hashes with Windows. The emulator was attached during this audit (`emulator-5554`), so it is feasible here. Add an arm64 phone run before release.

## (c) Multi-human support in the sim

| Item | Today | Gap |
|---|---|---|
| Houses | Up to 8 combatants plus neutral from `SkirmishConfig::slots`, `slots[0]` = "the human" (`skirmish.hpp:42`). `HouseRec` has side, colour, country, team, combatant and player_control (`world.hpp:1343-1353`). Human-ness in the sim is `!ai_enabled(h)` (`world.cpp:2260`), which already generalises. | Add `HouseRec::controller = {AI, Human(peer id), Observer}`. |
| Who may order a unit | `World::controllable(h)` = `h == 0 \|\| houses_[h].player_control` (`world.hpp:1355-1357`). Used by every `*_selected` order (`world.cpp:1395-1399`, `unit_orders.cpp:116-127`). `world.cpp:1147,1163` and `world_depth.cpp:107` still test `house != 0`. | Becomes `can_command(issuer_house, unit_house)`. PlayerControl and mind control belong to the *issuer*, not to "the local player". |
| Selection | `Unit::selected` is in the sim struct (`world.hpp:96`), **serialized** (`save_fields.hpp:944`) and therefore **hashed** by `fnv1a(save_world)`. | Move selection into a per-client UI structure. Commands carry unit-id lists. Exclude it from the sync hash. |
| Economy and production | The human wallet `player_credits_`, `ProductionQueues queues_` (`main.cpp:7411-7412`), `backlog_`, `held_` (`game_scene_controls.inc:378-379`), `bstates_` (`main.cpp:7480`) and `prod_tick(... house 0)` (`main.cpp:4381-4430`) are scene-side. They are bridged into `World` with `take_income(0)`, `deposit_credits(0)` and `set_wallet_hint(0)` (`main.cpp:6386-6440`). AI houses keep money in `World::house_credits_` (`world.hpp:2517`). | **Largest refactor.** Move one `PlayerEcon` per house (wallet, 6 queues, backlog, hold, factory lists, placement-pending) into `World` (or a `Sim` wrapper that owns World + PlayerEcons), stepped inside the tick for every human house. Saves gain these fields; the `SceneSave` economy fields move. |
| Shroud | Per-house in `World` (`explored_at(house,…)`, `world.hpp:1696-1734`). Ally reveal exists (`world.hpp:1426-1430`). Presentation reads house 0 about 70× (`main.cpp` 38, `pres_game.inc` 10, `pres_radar.inc` 4, …). | Add `local_house_` in the scene and replace the literal 0 in presentation. Mechanical work. |
| Sidebar, buildable sets, superweapons | `sw_types(0)`, `factory_category_lost(ft,0)`, `conyard_alive(world_,ba_,0)`, `primary_factory(…,0)`, `house_side(0)`; about 25 call sites in `main.cpp` / `game_scene_controls.inc` / `gf_scene.inc` / `w2air_play.inc`. | Parameterise by house. The UI uses `local_house_`; the sim tick loops over all human houses. |
| Alliances / teams | Lobby teams map to `alliance_team`. `set_pair_alliance` / `set_ally_view` exist (`world.hpp:1395-1400`). Skirmish "AIs ally against the human" rule (`skirmish.cpp:40-65`). | Co-op vs AI = humans on one team plus AIs on another, which already works through `alliance_team`. The "all AIs ally when no teams" rule must treat *all* humans as humans (it already loops over slots). The in-game `ToggleAlliance` command is not implemented. |
| AI fill-ins | `enable_ai_house` per slot. The AI runs inside `World::step`, so it is deterministic. | Dropped player → `enable_ai_house(h, diff)` via a scheduled command (deterministic). |
| Spectators | Not present. | Observer = a peer with no house that runs the full sim and sends no commands except chat; presentation shows all houses (shroud off). |
| Victory and game-over | `gf_update` (scene) / `winning_team()` (World). The local cheer is a world mutation (`main.cpp:6518`). | Make the cheer a sim rule for every winning house, or presentation-only. |

## (d) State hashing, reconnect / resync, shell

* **Hashes available.** `world_hash` reads only unit cell/hp/alive and building tuples (`main.cpp:9800-9815`); it is too weak to localise drift. The full-state hash is `fnv1a(save_world(w))` (+ triggers + scene), used by the saveload gate (`main.cpp:17549-17555,17712,17818`). Doubles are stored as exact 8-byte patterns (`savegame.hpp:13-14,135-137`), so the full hash is bit-sensitive, which is what we want.
* **Cost.** A save is 0.5-0.67 MB (10 real saves in `%APPDATA%\ra2engine\RA2\Saved Games`). FNV-1a byte-wise at about 4 cycles/byte is roughly 1 ms on desktop and 3-5 ms on a phone, plus serialisation (estimate, not measured). Recommendation:
  * A **cheap per-tick hash** from a few counters: RNG states, unit count, sum of cells/hp, credits.
  * The **full hash every 30 ticks** (2 s), computed on the logic thread right after the tick.
  * Per-section sub-hashes (units, buildings, houses, AI, projectiles) so a desync report names the subsystem. Dump `save_world` on mismatch for offline diff.
  * Exclude transient UI fields (`selected`, event queues) from the hash, or move them out of the World.
* **Resync / late join / reconnect by state transfer.** It works in principle:
  * `load_world` overwrites every serialized member (`savegame.hpp:221-225`).
  * `--demo-saveload` proves save → load → continue gives a hash identical to running straight through.
  * The compressed state is 9-19 KB (gzip -9; zlib level 1 gives 31 KB), so it fits a relay easily.

  Prerequisites:
  1. All sim state is inside the serialized set. Today the scene-side economy is captured by `SceneSave`; it should move into the World (see (c)).
  2. A fresh peer builds the World from the same map and data. `load_world` "must have been constructed from the same map".
  3. Clear the path memo (risk above).
  4. Presentation state (anims, smudges, particles, audio) is rebuilt locally; it does not need to match.
* **Shell.** There is no networking code and no lobby. `SkirmishSetupScene` / `skirmish_page.inc` plus `skirmish_lobby.{hpp,cpp}` (8 rows; row 0 is the editable human, rows 1-7 AI combos) are the natural base for a "Network game" lobby: rows become {Open, Closed, Human(peer), AI(diff)}, with chat, a ready flag, the host-authoritative config, the session seed, and the data/engine hash check. The main menu already has the Network and Internet buttons (disabled, `main.cpp:8297,8317-8318`). Android needs the `INTERNET` permission (manifest currently has none, line 31) and, for LAN discovery, `CHANGE_WIFI_MULTICAST_STATE` plus a `MulticastLock`.

## (e) Performance and fixed logic rate

* **Measured (this audit).** Release build. `--ai-soak --minutes 10 --ai-vs-ai` (xmp31s2, 2 AI houses, 9,000 ticks) ran in **14.2 s wall including load**, which is about 1.2-1.6 ms per tick on desktop. Two runs gave identical output.
* **Prior data.**
  * 8 players headless run at 290-400 ticks/s (R-172).
  * 226 tanks fighting took 28 ms per tick in wave 2 (before later optimisations).
  * Emulator ALL03: `update` 4-7 ms. Emulator ALL06 battle: `update` 25-41 ms per frame, logic falls to 15/s with no headroom (W6-06-18).
  * No real arm64 numbers exist yet (R-170).
* **Fixed rate independent of rendering.** Logic is already decoupled from rendering by the accumulator. For lockstep:
  1. The tick source becomes "network says tick N is complete". Ticks are never dropped. If a peer falls behind, everyone waits (adaptive latency) or the host lowers the session speed.
  2. On slow phones, let the sim catch up with several ticks per frame and skip *rendering* instead (the opposite of today's `kMaxLogicSteps` drop).
  3. Optional later step: run the sim on its own thread. Presentation reads `const World&`, so this needs a double-buffer or snapshot; it is not needed for v1.
* At 15 ticks/s (GameSpeed 4) the budget is 66 ms per tick. A fast session speed (GameSpeed 1 = 60 ticks/s) gives a 16 ms budget, which phones will not sustain in large fights. The lobby should cap the speed for mixed-device sessions.

---

## Gap list

Effort: S < 2 days, M ≈ 1 week, L ≥ 2 weeks.

| ID | Gap | Severity | Effort |
|---|---|---|---|
| N4-01 | libm transcendentals in the sim differ across platforms (measured) → deterministic `dmath`, CI grep rule, re-pin gates | BLOCKER | M |
| N4-02 | No command layer: ~35 UI sites mutate World at frame time → `Command` structs, a tick-keyed queue, apply at tick start | BLOCKER | M |
| N4-03 | Human economy and production are scene-side and house-0-only → per-house `PlayerEcon` inside the sim, stepped for all humans | BLOCKER | L |
| N4-04 | Selection is sim state (serialized and hashed) → move to client UI; commands carry unit ids | BLOCKER | M |
| N4-05 | `controllable()` = local player → `can_command(issuer, house)`; remove the literal `house != 0` in `world.cpp:1147,1163`, `world_depth.cpp:107` | HIGH | S |
| N4-06 | ~70 presentation reads and ~25 scene calls with literal house 0 → `local_house_` | HIGH | M (mechanical) |
| N4-07 | Tick drop (`kMaxLogicSteps`) and local pause (menu, briefing, background) → network-driven ticks, pause as a command, Android background = disconnect then resync | HIGH | S-M |
| N4-08 | Wall-clock crate seed and lobby RNG seed → host session seed | HIGH | S |
| N4-09 | No data/version handshake → hash of engine version + rules/art/ai/map bytes + YR data set | HIGH | S |
| N4-10 | Weak `world_hash`; full hash not wired for runtime → periodic sectioned sync hash plus desync dump | MEDIUM | S |
| N4-11 | Path memo keyed by pointers, `thread_local`, outlives a World → content keys or clear on World build/load | MEDIUM | S |
| N4-12 | Float→int UB differs between arm64 and x86 → `dm::to_int` in sim; debug assert | MEDIUM | S-M |
| N4-13 | Victory cheer and other local-house sim mutations → sim rule or presentation | LOW | S |
| N4-14 | `inf_human_` single house → bitmask | LOW | S |
| N4-15 | No observer mode | LOW | M |
| N4-16 | No network lobby, no sockets, no `INTERNET` permission; menu buttons disabled | BLOCKER (for shipping) | L (with N5) |
| N4-17 | Raw-input replay is not portable → command-log replay (same format as the network stream). This also gives replays and desync repro. | MEDIUM | S (after N4-02) |
| N4-18 | arm64 libm and sim behaviour not verified on hardware | HIGH (unknown) | S |
| N4-19 | Mixed-device performance: slowest peer sets the pace; phones at 25-41 ms update in big fights | MEDIUM | M (profiling) |

## Phased implementation plan (our own lockstep)

The design follows the original's model (N1 has the details): deterministic sim at 15 Hz, inputs as events scheduled for `current_tick + latency`, every peer applies the same event list at the start of a tick, periodic CRC compare. The transport and relay come from N5.

**Phase L0: groundwork, no networking (M, can run in parallel)**
1. N4-01 dmath, plus the CI grep and gate re-pin.
2. N4-11 memo fix, N4-12 conversions, N4-08 session seed.
3. Cross-platform gate runner: headless gates as an NDK x86_64 executable on the emulator, compared with Windows (N4-18 on an arm64 phone).
4. Exit criterion: `--demo-*`, `--demo-saveload` and a 30-minute `--ai-soak --ai-vs-ai` give **identical hashes on Windows, emulator x86_64 and arm64**.

**Phase L1: command layer, single player (M-L)**
1. Define `struct Command { uint32 tick; uint8 house; uint16 type; payload }`. Types:
   * unit orders: Move, Attack(unit/bldg/cell), Harvest, Return, Enter, Occupy, Capture, Defuse, Infiltrate, Stop, Guard, Scatter, Deploy, Unload
   * production: SetPrimary, Rally, Produce, Hold, Resume, Cancel, CancelAll
   * buildings: PlaceBuilding, Repair, Sell, Power
   * other: FireSW, Cheer, Alliance, Surrender, Chat (non-sim), Pause/Resume, SetAI(house, diff)

   Each unit order carries a unit-id list (≤ 255 ids; larger selections split across commands).
2. A `CommandQueue` with an `apply_commands(tick)` call **inside** the tick loop, before `campaign_->update` / `world_.step`. Apply in a fixed order: sorted by (house, sequence). Validate ownership and liveness at apply time.
3. Reroute all bypass sites (table in (a)) to `enqueue(cmd)`. Locally, commands are scheduled for `tick + 1`.
4. N4-04: selection out of `Unit` (client-side `std::vector<int>`). `World::*_selected` becomes `World::order_*(house, ids)`.
5. Command-log record/replay (N4-17), plus a hidden replay gate that matches the existing `probe_hash`.
6. Exit criterion: the game plays as before, and a command log replays identically across platforms.

**Phase L2: multi-human sim (L)**
1. N4-03 `PlayerEcon` per house in the sim (wallet, queues, backlog, hold, exit handoff, free units, repair bridge removed). AI houses unchanged.
2. N4-05/06/13/14 house parameterisation; `local_house_` in the scene.
3. Save format bump: economy moves from `SceneSave` into the World.
4. Exit criterion: a hot-seat test harness runs 2+ human houses from scripted command logs on one machine and reaches the same hashes as two separate processes.

**Phase L3: lockstep netcode (M, with N5 transport)**
1. Peer session: host-authoritative lobby config, session seed, data hash (N4-09), start tick barrier.
2. Per tick, each peer sends `{tick, commands[]}` (empty allowed) for `tick + L`. A tick runs when all peers' packets for it are present. Adaptive `L`: 2-6 ticks at 15 Hz (133-400 ms), sized from RTT, the way the original sizes its frame-send rate.
3. Sync hash (N4-10): cheap hash every tick, full sectioned hash every 30 ticks, exchanged in the tick packets. On mismatch: dump state, pause, offer a resync.
4. Never drop ticks (N4-07). Render skipping on slow peers. Pause and the options menu become non-pausing or a `Pause` command.

**Phase L4: robustness (M)**
1. Reconnect / late join / resync by state transfer: host serialises `save_world` + econ + triggers at tick T (gzip ≈ 10-30 KB), the joiner loads it and receives buffered commands T..now, then fast-forwards.
2. Android background is treated as a disconnect: hold the slot for N seconds, then AI takeover via a scheduled `SetAI` command, then resync on return.
3. Drop or kick handling, AI fill-in, observers (N4-15).

**Phase L5: shell and UX (L, overlaps L3)**
1. Network lobby page derived from the skirmish page: open/closed/human/AI slots, teams (co-op vs AI), colours, start positions, chat, ready, map transfer or hash check.
2. LAN discovery and relay lobby (N5).
3. In-game chat and allied beacons; `ToggleAlliance`, `PlaceBeacon`, `PageUser`.
4. Android `INTERNET` permission.

## Reproduction

* Same-binary determinism: run `ra2engine --ai-soak --minutes 10 --ai-vs-ai` twice and diff the output (identical here).
* libm probe:
  * Windows: `g++ -O2 -ffp-contract=off -std=c++17 docs/gap/net/04-libm-probe.cpp -o probe.exe && probe.exe 1000000 win.bin`
  * Android: `<ndk>/x86_64-linux-android24-clang++ -O2 -ffp-contract=off -std=c++17 -static-libstdc++ 04-libm-probe.cpp -o probe_x64`, then `adb push` it to `/data/local/tmp` and run with `1000000 /data/local/tmp/x64.bin`.
  * Diff the two dumps; each record is 9 doubles.

  Per-function hashes printed in this audit:

  | Function | Windows | Android x86_64 |
  |---|---|---|
  | sin | 8b51e00f84b99bf7 | 8b2cb53ac2de4d89 |
  | sqrt | 8ad959ebefb194a5 | 8ad959ebefb194a5 (identical) |
