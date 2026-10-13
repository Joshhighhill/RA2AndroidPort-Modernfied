# w7sus-misc: suspect audit (master 9bf5ccd)

STATUS: PARTIAL. Suspects 2 and 4 are done. Suspects 1 and 3 were handed to two opus sub-auditors, which were still running when this report had to be handed back. Their scratch dirs are D:\ra2-re\w7sus-misc-out\amp and \a09. They share an engine lock at D:\ra2-re\w7sus-misc-out\engine.lock, which they remove when they finish.

---

## Suspect 2: --demo-projectile "WITNESS FAILED: miss"

**Verdict: FAITHFUL (the engine is right; the gate's witness expectation is stale).** The cause is the wave-7 speed model (commit 0a50188, W7-11-01). OpportunityFire and the threat scan are not involved: the gate's synthetic World never calls `arm_combat_w7`, so the `w7` targeting block in combat.cpp:1463 is off.

### Evidence
I made one gate run, which is in D:\ra2-re\w7sus-misc-out\demo-proj.out. It exited 1 and the hash still matches the pin:
```
[proj] miss: old_cell=(114,112) impact_cell=(114,112) victim=1 damage=22 target_hp=103/125
[proj] WITNESS FAILED: miss
[demo-projectile] deterministic=yes projectile_hash=d1cd26e6fe1a6d4c
```
The other four witnesses (firewall, arc, kirov, flak) pass.

### What the witness checks
engine/main.cpp:16916-16947. A Rhino (HTNK, 120mm, Damage 90, AP CellSpread .3) fires at a Conscript 4 cells away. On the launch frame the gate calls `order_unit_move(1, ox, oy+3)`. The witness then requires `det.victim == -1 && det.damage == 0 && target hp unchanged`.

### Why it fails now
- **Conscript speed.** The gate World is `build_world` (main.cpp:2429). It calls `set_move_speed(speed_to_cps(gi_speed))`, which works out to 10 leptons/frame. Since W7-11-01 (main.cpp:848-863: `Speed=4 -> trunc(4*256/100)=10` leptons/frame, no GameSpeedBias), Walk moves raw leptons. Before wave 7 it was x1.6, about 16 leptons/frame.
- **Not enough movement.** The shell flies about 12-13 frames, and `step_projectiles` runs after `step_units` in the same `World::step` (world.cpp:5531 vs 5631). So the Conscript moves at most about 120-130 leptons from its cell centre. It needs at least 128 to leave the cell, so in practice it is still inside the impact cell. The old speed gave about 190-200 leptons, so it left the cell and the shell missed.
- **Hit at the old cell.** When the target is still in the impact cell, the shell hits. This happens either through §7 collision (world.cpp:6383-6390, first hostile in the cell within 127 leptons) or through the close-enough re-aim (world.cpp:6436-6447). The re-aim snaps onto the target when `dist/3 <= max(2|V|,128)`. The recorded damage of 22 is the full 90 x AP Verses 25% (infantry), which matches a snap onto the target.

### Original behaviour
- **Speed.** Speed is stored as `min(255, clamp(Speed,0,100)*256/100)` leptons/frame. Sources: TechnoType INI FUN_00711510 (+0x678) and FootClass::GetCurrentSpeed FUN_004DB0B0. GameSpeedBias (house+0x190) is written by FUN_004F6DD0 and never read. The re-audit is in docs/gap/wave7/11-locomotion.md W7-11-01.
- **Re-aim.** BulletClass::Update FUN_004666D0, tail 0x467D60..0x467E3E, with DAT_007E1808 = 128.0. A non-Inaccurate shell that detonates in its target's cell re-aims onto the target. See docs/re/combat.md §10 and the decompile in D:\ra2-re\out\proj\bul_4666D0.c.
- **Conclusion.** A Conscript walking at 10 leptons/frame cannot clear its cell during a roughly 12-frame Rhino shell flight, so in the original the shell hits it as well.

### Clone behaviour
It matches the original:
- engine/world.cpp:6383-6390 and :6436-6447 (detonation and re-aim);
- engine/world_loco.cpp:942-953 (Walk raw leptons);
- engine/main.cpp:860-864 (`speed_to_cps`).

### Root cause
The gate scenario was written when infantry moved at x1.6. The W7 re-pin (README.md:2785, commit 50f770f) says "same witnesses", which is not true: the miss witness has failed since 0a50188, and d1cd26e6fe1a6d4c was pinned with a failing witness.

### Suggested fix (gate only; no engine change)
Change scenario 3 in engine/main.cpp:16916 so that the dodger leaves the impact cell before the shell arrives, while keeping faithful physics. The simplest option is to give the dodger a speed that clears a cell in about 12 frames. For example, after the spawn:
```
world.set_move_speed(speed_to_cps(10.0));   // 25 lep/frame; only the dodger moves (the Rhino is in range and stationary)
```
Two alternatives would also work:
- set a per-unit `speed_cps` on unit 1 (world_loco.cpp:215 honours `u.speed_cps`);
- keep the Conscript at faithful speed and rename the witness to "reaim", asserting `victim==1 && damage==22`. This pins the 0x467D60 behaviour, which is itself worth a witness. Ideally there would be both a "miss" with a fast dodger and a "reaim" with the Conscript.

After the change, re-pin `projectile_hash` in README.md:2785 and correct the "same witnesses" note.

### Test
Run `--demo-projectile`. It should exit 0, the miss line should show `impact_cell != current cell of unit 1`, `victim=-1 damage=0 target_hp=125/125`, and the run should be deterministic. A doctest in the projectile suite (a two-unit World, the dodger at 25 leptons/frame, then assert no damage) would cover the same thing without the gate.

---

## Suspect 4: `missile_holds` leftover

**Verdict: CONFIRMED dead field (cleanup, no behaviour impact).**

### Evidence
- `git grep missile_holds` on master finds only two hits:
  - engine/unit_w7.hpp:93 (the declaration, `std::vector<int>`);
  - engine/save_fields.hpp:830 (`io(ar, v.missile_holds)` in `fields(Ar&, UnitW7&)`).
- Nothing reads or writes it. Its only writer and reader were in w7fix-units commit e0915ab:
  - engine/world_an.cpp:277-280, the decrement/erase;
  - engine/world_an.cpp:347, `push_back(pause_frames + tilt_frames)`, W7-04-08.
- The air/naval merge replaced that logic with the spawn-manager node state:
  - engine/world_an.cpp:598-604 sets `sp_state[K] = kSpawnRocketWait` and `sp_timer[K] = pause_frames + tilt_frames`, per SpawnManagerClass::AI FUN_006B68B0 / 0x6B72E0;
  - the state and timer are serialized at save_fields.hpp:421.
- So the vector is always empty, and every save writes a zero-length vector for each unit. It is harmless, except that it costs a few bytes per unit.

### Original behaviour
The PauseFrames+TiltFrames hold lives in the SpawnManager node (state 1 with a timer). The clone now models it in `AnUnit::sp_state` and `sp_timer`, so there is no per-unit list.

### Suggested fix
At the next save-format bump (engine/savegame.hpp:39 `kFormatVersion`, currently 39):
- remove `missile_holds` from engine/unit_w7.hpp:89-93, including its comment, or repoint the comment to `sp_timer`;
- drop `io(ar, v.missile_holds)` from engine/save_fields.hpp:830;
- add 40 to the version comment.

Removing it without a bump would break v39 save compatibility, so the two changes have to go together. `sizeof(World)` is not affected, because units live in a vector.

### Test
Save/load round-trip doctest (existing saveload suite) at the new version, plus the saveload gate re-pin. `git grep missile_holds` should then return nothing.

---

## Suspect 1: Amplifier self-repair (ALL04)

PENDING. The opus sub-auditor (scratch D:\ra2-re\w7sus-misc-out\amp) is checking:
- the AI repair rules RepairStep, RepairRate, [IQ] RepairSell and credits;
- ALL04 trigger actions;
- the `hp_lost(h)` probe semantics. That probe counts damage on LIVE buildings only, so a building that dies, is captured or is handed over drops out and reads as "back to 0";
- a reproduction run.

No verdict yet.

## Suspect 3: m-all09-opening

PENDING. The opus sub-auditor (scratch D:\ra2-re\w7sus-misc-out\a09) is running the script, reading result.txt and play.log, and bisecting by history to classify the failure as script or engine. No verdict yet.
