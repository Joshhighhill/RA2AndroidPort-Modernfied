# RA2 Clone — Completion Roadmap

**Standing goal:** finish the Red Alert 2 (Allied vs Soviet, no Yuri) clone at
`D:\ra2-android` — a complete, playable game, built calibrate-first from the
original game data / `gamemd.exe` / `game.exe`, one byte-identity-preserving
phase at a time.

**Method (unchanged, proven across 25+ phases):** RE hunt (`docs/re/*.md`) →
spec (`docs/superpowers/specs/`) → plan (`docs/superpowers/plans/`) →
subagent-driven execution with per-task + whole-branch review → finalize
(README + ledger). Every phase must keep the **18 byte-identity gates**
bit-identical (or add its own new gate) and must be additive/firewalled.

**Model assignment:** hunts/specs/plans/reviews → fable; implementation → opus;
mechanical single-file tasks → sonnet.

## Status at roadmap creation (commit 54359c8)

Done: MIX/SHP/PAL/VXL/HVA/TMP/CSF/BAG/WAV/PCX/FNT/**Bink** decoders; iso render;
A* movement; infantry+vehicle+air+naval combat; buildings/construction; economy
(ore, harvesters, refineries); unit production (4 queues); AI-1 base / AI-2 teams
/ AI-3 triggers; transports; superweapon (nuke); tech-tree; engineer/spy capture;
shroud/fog; multi-house alliances + victory; skirmish shell; faithful main menu
with the real Bink CRT animation; 24 buildings + ~20 units; Android arm64 build.

## Phase queue (ordered; each is a full spec→plan→SDD cycle)

| # | Phase | Why it's next | Gate |
|---|-------|---------------|------|
| P1 | **Repair & Sell** | The sidebar's two remaining inert buttons; core base management the player reaches for constantly. Tight scope. | `--demo-repair` |
| P2 | **Projectile flight (real ballistics)** | Every weapon is currently instant-hit. The single most-visible fidelity gap in normal play; unblocks V3/Kirov/Dreadnought abilities. | `--demo-projectile` |
| P3 | **Map browser / multi-map** | One hardcoded map today. Enumerate + parse the stock map set so skirmish is replayable. | `--demo-maps` |
| P4 | **Unit special abilities (batch 1)** | Terror Drone, Desolator, Chrono Legionnaire, SEAL C4, Terrorist, IFV modes — the roster has stats but no identity. | `--demo-abilities` |
| P5 | **Veterancy + crates** | Kills-based promotion/elite/chevrons (minimal rank-1 exists) and crate pickups. | `--demo-veterancy` |
| P6 | **Roster breadth + remaining producers** | Naval Yard, Airfield/rearm, walls, the rest of the Allied/Soviet cast. | `--demo-roster2` |
| P7 | **Save / Load** | Serialize + restore world state; prerequisite for campaign. | `--demo-saveload` |
| P8 | **Options + persistence** | Sound/keyboard/display screens, settings file. | (interactive) |
| P9 | **Map triggers / scripting** | The campaign substrate: `[Triggers]`/`[Events]`/`[Actions]`/`[Tags]` from the map. | `--demo-triggers` |
| P10 | **Campaign missions** | Mission list, briefing, loadscreens, win/lose conditions. | `--demo-campaign` |

Ordering rationale: P1–P2 finish the *combat/base* loop the player already
touches; P3 makes it replayable; P4–P6 fill content; P7–P8 finish the shell;
P9–P10 are the campaign, which depends on save/load and triggers.

## Non-goals (unchanged)

Yuri faction; multiplayer netcode; map editor; mod support.

## Ledger

Durable progress lives in `.superpowers/sdd/progress.md` (gitignored). This file
records the plan; the ledger records what actually happened.
