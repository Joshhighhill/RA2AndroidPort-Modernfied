# Audio Playback — SFX, unit voices, EVA

**Date:** 2026-07-10 · **Status:** approved (design sections 1–3 approved in session)
**Calibration authority:** `docs/re/audio.md` (BAG/ADPCM, complete) + `docs/re/audio-playback.md` (NEW — Task 1 of the plan produces it; playback semantics recovered from gamemd.exe)

## Goal

Wire the bit-exact audio decoder (Phase 2.5: `AudioIdx` + `decode_bag_clip`,
byte-parity vs XCC) to actual sound output in `ra2engine`: weapon fire,
unit voice acknowledgements, death screams, and EVA speech — with playback
behavior (volume, attenuation, limits, priority, variant choice) recovered
from the original binary, per the standing calibrate-first directive.

## Scope

**In:** weapon `Report=` on fire; `DieSound=` on death; `VoiceSelect=` /
`VoiceMove=` / `VoiceAttack=` acks (house-0, one speaker per command, like
the original); EVA "unit lost" (queued, never overlaps itself). Desktop SDL
output; Android compiles via the same SDL backend (arm64 gate as usual).

**Out (documented carry-forwards):** music/theme streaming; ambient
`Control=loop` sounds; stereo IMA-ADPCM (`uslide1/2` — decoder still
throws); stereo panning (we output mono; if the original pans positionally,
document the divergence in the RE doc).

## Ground truth already extracted (this session, from the real install)

- rulesmd.ini: `[M60] Report=GIAttack`, `[M1Carbine] Report=ConscriptAttack`;
  `[E1] VoiceSelect=GISelect VoiceMove=GIMove VoiceAttack=GIAttackCommand
  DieSound=GIDie` (+`VoiceFeedback=GIFear`, unused this phase);
  `[E2]` = Conscript equivalents.
- soundmd.ini: `[GIAttack] Sounds=igiat1a igiat1b igiat1c, Control=random
  interrupt, VShift=15, Volume=65`; `[ConscriptAttack]` 7 variants (with
  repeats), `Limit=3`, Volume=65; voice entries (`[GISelect]` etc.)
  `$`-prefixed variants, `Control=random`, Volume=85; `[GIDie]`/
  `[ConscriptDie]` 5 variants, `Priority=low`, `FShift=-10 10`, Volume=85.
- audio.idx (real, 1153 entries, all 22,050 Hz): every needed clip present;
  `$igisea` in soundmd ↔ bare `igisea` in the idx ($ stripped for lookup).
- Load path (proven by tests/test_audio.cpp): `language.mix → audio.mix →
  audio.idx + audio.bag`. EVA speech and music are RIFF WAV (`fmt` 0x0011,
  same IMA math — docs/re/audio.md "second, generic IMA codec",
  content-located at FUN_007c50ac).

## Task 1 — Ghidra playback-semantics hunt (BEFORE any playback code)

Recover, content-anchored, into `docs/re/audio-playback.md` with a
confidence table (ModEnc/XCC fallback tagged Low where the hunt fails):

1. Distance attenuation: volume vs distance from the tactical view;
   `Range=` / `MinVolume=` semantics and defaults.
2. `Limit=` cap behavior (reject vs steal) and `Priority=` stealing rules;
   engine defaults for absent keys.
3. `Control=random` variant selection (which RNG; uniform vs no-repeat
   shuffle) and `interrupt` semantics.
4. Token semantics: `$` prefix, `VShift=`, `FShift=a b`, `Control=loop/
   attack/decay` (loop is out of playback scope but document the flag).
5. EVA: queue rules (one at a time), volume class relative to SFX/voices.

## Architecture (approach approved: pure mixer + thin SDL sink, no new deps)

1. **`ra2assets::Sounds`** (`include/ra2assets/sounds.hpp`, `src/sounds.cpp`)
   — thin soundmd.ini lookup mirroring `Rules`/`Art` (wraps parsed `Ini` by
   reference): `entry(name)` → `{variant names ($ stripped), volume,
   priority, limit, control flags, range, min_volume}`; defaults per the
   RE doc.
2. **`ra2assets::load_wav_ima`** (`include/ra2assets/wav.hpp`,
   `src/wav.cpp`) — RIFF WAV fmt-0x0011 IMA decoder for EVA (container
   parse over the already-proven nibble math).
3. **`engine/audio_mixer.{hpp,cpp}`** — PURE (no SDL; compiled into tests +
   engine via `cmake/ra2_sources.cmake` pattern): `SoundBank` (id → decoded
   PCM, preloaded at startup), `Mixer::play(...)` honoring the recovered
   volume/limit/priority/interrupt semantics with positional attenuation,
   `play_eva` one-at-a-time queue, `render(int16*, n)` summing voices with
   clamp. Variant RNG is audio-local and seeded — NEVER touches sim state
   (`--demo-combat` byte-determinism must be provably unaffected).
4. **`engine/audio_sdl.{hpp,cpp}`** — the only SDL-aware piece: opens the
   device (22,050 Hz mono S16), SDL audio callback pulls `Mixer::render`
   under a lock. Excluded from the test target.
5. **Wiring:** a `collect_audio_events` pass in the render loop observes
   World state transitions (fire_flash set → Report; die_time started →
   DieSound, with the unit's cell for attenuation); input handlers trigger
   select/move/attack acks; EVA "unit lost" on house-0 death. `main.cpp`
   loads soundmd.ini via `load_ini_from_ra2md`, opens the language.mix
   chain, preloads the needed clips + EVA file.

**Error handling:** missing clip/entry → log once, mute that sound; audio
device open failure or `--no-audio` → fully silent, game unaffected. Never
crash over audio.

## Testing

- **Mixer (pure, no assets):** hand-computed vectors — overlap summing with
  clamp; Limit/priority behavior per recovered rules; EVA queue order;
  deterministic variant sequence from fixed seed; attenuation pinned to
  hand-computed values from the recovered formula.
- **`Sounds` reader:** synthetic INI + install-guarded real-soundmd pins
  ([GIAttack] 3 variants/vol 65/random+interrupt; [ConscriptAttack]
  Limit=3; [GIDie] priority low, FShift −10..10, vol 85).
- **WAV decoder:** install-guarded decode of the real EVA clip (format
  invariants + length arithmetic; XCC oracle if cheap).
- **Integration gates:** `--demo-combat` byte-identity UNCHANGED; new
  `--demo-audio` scripted sequence (report → voice ack → die → EVA) exits 0
  (audible smoke test on desktop); suites green with AND without install
  (clean skips); arm64 NDK compiles all new sources.

## Constraints (standing, binding)

GPL-clean (XCC/YRpp/ModEnc read off-repo, prose only); no game assets or
Ghidra project committed; tests skip cleanly without `RA2_INSTALL_DIR`
(default `D:\Red Alert 2`); pure modules shared via `cmake/ra2_sources.cmake`;
test exes from PowerShell; work on `master`, frequent commits.
