// RA2 engine desktop entry point.
//
// Default: opens an interactive SDL window and renders the real temperate map
// terrain (arrow keys / drag to pan, Esc/close to quit).
//
// `--screenshot <out.png>`: renders exactly one frame with the camera centered
// on the map, reads the framebuffer back, writes it to a PNG, and exits.
//
// Asset chain (all under $RA2_INSTALL_DIR, default "D:\Red Alert 2"):
//   * map     : mapsmd03.mix, entry id 0x133a0cb2 (temperate, 103x110)
//   * palette : ra2.mix -> cache.mix -> isotem.pal (768-byte 6-bit iso pal)
//   * tilecfg : ra2.mix -> local.mix -> temperat.ini  ([TileSetNNNN] mapping)
//   * tiles   : ra2.mix -> isotemp.mix -> <FileName><NN>.tem templates
// See engine/iso_renderer.hpp (Theater) for the tile-index -> template mapping.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio_mixer.hpp"
#include "audio_sdl.hpp"
#include "facing_cache.hpp"
#include "input.hpp"
#include "iso_project.hpp"
#include "iso_renderer.hpp"
#include "platform.hpp"
#include "texture_cache.hpp"
#include "voxel_raster.hpp"
#include "world.hpp"

#include "ra2assets/art.hpp"
#include "ra2assets/audio_bag.hpp"
#include "ra2assets/fileio.hpp"
#include "ra2assets/hva.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/rules.hpp"
#include "ra2assets/shp.hpp"
#include "ra2assets/sounds.hpp"
#include "ra2assets/vpl.hpp"
#include "ra2assets/vxl.hpp"
#include "ra2assets/wav.hpp"

using namespace ra2assets;

namespace {

std::string env_install_dir() {
#ifdef __ANDROID__
    // Android has no env vars / CLI. The user pushes the game files to the
    // app's external files dir (see README "Android": adb push ... ra2.mix,
    // mapsmd03.mix, ...). SDL_AndroidGetExternalStoragePath() resolves that to
    // /storage/emulated/0/Android/data/org.ra2.engine/files (no runtime
    // permission needed for the app's own dir); we read the mix chain from its
    // `ra2/` subfolder. Assets are NOT bundled in the APK (copyright).
    const char* ext = SDL_AndroidGetExternalStoragePath();
    std::string dir = ext ? ext : "/sdcard/Android/data/org.ra2.engine/files";
    dir += "/ra2/";
    // std::cerr/std::cout are not captured on Android; SDL_Log routes to
    // logcat (tag "SDL"). Log the resolved asset dir so a missing/misplaced
    // game-file set is diagnosable from `adb logcat` instead of the app
    // silently exiting.
    SDL_Log("[assets] android asset dir = %s", dir.c_str());
    return dir;
#else
    const char* e = std::getenv("RA2_INSTALL_DIR");
    std::string dir = e ? e : "D:\\Red Alert 2";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '\\';
    return dir;
#endif
}

// Everything the renderer needs, loaded once from the mix chain.
struct Assets {
    MapFile map;
    Palette pal;
    ra2engine::Theater theater;
};

// `ra2` is the already-open ra2.mix (holds the nested theater/config/palette
// mixes) -- opened ONCE by each mode entry point and shared with
// load_unit_assets(), instead of each loader slurping the 268 MB file
// independently (was 79 s cold start; see docs/superpowers/specs/
// 2026-07-09-foundation-batch-design.md, sub-project 4).
Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2) {
    // Map: read the temperate reference map INI out of mapsmd03.mix.
    MixFile maps = MixFile::open(ra2assets::read_file(dir + "mapsmd03.mix"));
    const MixEntry* me = maps.find(0x133a0cb2u);
    if (!me) throw std::runtime_error("map entry 0x133a0cb2 not found in mapsmd03.mix");
    auto map_raw = maps.read(*me);
    MapFile map = MapFile::parse(std::string(map_raw.begin(), map_raw.end()));

    const std::string theater = map.theater();
    if (theater != "TEMPERATE") {
        // Milestone 1 wires temperate only; other theaters map the same way
        // (isosnow.mix/snow.ini/.sno, isourb.mix/urban.ini/.urb).
        throw std::runtime_error("unsupported theater (only TEMPERATE wired): " + theater);
    }

    // Palette: isotem.pal lives in cache.mix.
    MixFile cache = ra2.open_nested("cache.mix");
    const MixEntry* pe = cache.find(std::string("isotem.pal"));
    if (!pe) throw std::runtime_error("isotem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    Palette pal = Palette::load(pal_raw.data(), pal_raw.size());

    // Tile-set control INI: temperat.ini lives in local.mix.
    MixFile local = ra2.open_nested("local.mix");
    const MixEntry* ce = local.find(std::string("temperat.ini"));
    if (!ce) throw std::runtime_error("temperat.ini not found in ra2.mix/local.mix");
    auto ini_raw = local.read(*ce);
    std::string ini_text(ini_raw.begin(), ini_raw.end());

    // Tiles: the .tem templates live in isotemp.mix.
    MixFile isotemp = ra2.open_nested("isotemp.mix");

    ra2engine::Theater th(std::move(isotemp), ini_text, "tem");

    std::cerr << "[assets] theater=" << theater << " size=" << map.width() << "x"
              << map.height() << " cells=" << map.cells().size()
              << " templates=" << th.template_count() << "\n";

    return Assets{std::move(map), pal, std::move(th)};
}

// Everything needed to draw + fight both squads: one decoded SHP per
// distinct image ("GI", "CONS", both keyed by Rules::image() output), the
// shared (temperate UNIT, not terrain) palette, and each unit-type's
// Rules-resolved combat stats + walk speed. Generalizes the single-unit-type
// version of this struct (Phase 2/6) to the two-squad combat vertical slice
// (Task 8): render_world/draw calls pick the SHP by `unit.image` out of
// `shps` instead of assuming there is only one.
struct UnitAssets {
    std::map<std::string, ShpFile> shps;  // image -> decoded SHP ("GI", "CONS")
    Palette pal;                          // shared unittem.pal
    ra2engine::UnitCombat gi;             // E1-resolved combat stats (image "GI")
    int gi_speed = 0;                     // rulesmd.ini [E1] Speed=
    ra2engine::UnitCombat cons;           // E2-resolved combat stats (image "CONS")
    int cons_speed = 0;                   // rulesmd.ini [E2] Speed=
    std::map<std::string, ra2engine::UnitAnimSeq> anims;  // image -> artmd.ini-resolved anim seq
};

// Everything needed to draw + fight one voxel vehicle type (Rhino/HTNK): six
// pre-rendered facing caches (hull/turret/barrel x house 0/1 -- see
// facing_cache.hpp), its Rules-resolved combat/locomotion stats, the
// turret-space PrimaryFireFLH muzzle offset (leptons; docs/re/voxel-render.md
// §6), and the death-explosion SHP + its palette. `ok` is false whenever ANY
// required asset failed to load (logged once); the game then runs infantry-only
// and never touches this struct's caches. Move-only (unique_ptr member); loaded
// once per mode entry point, held by reference thereafter.
struct VehicleAssets {
    ra2engine::FacingSet hull[2], turret[2], barrel[2];  // index = house (0 Allied, 1 Soviet)
    bool has_turret = false, has_barrel = false;
    ra2engine::UnitCombat combat;
    int flh[3] = {0, 0, 0};                        // PrimaryFireFLH F,L,H (leptons, turret space)
    std::string explosion_image;                   // Explosion= first entry (e.g. "TWLT070")
    std::unique_ptr<ra2assets::ShpFile> explosion;  // death anim SHP; null if unavailable
    ra2assets::Palette explosion_pal;              // anim.pal (or unit pal fallback)
    bool ok = false;
};

// Loads `name` (e.g. "rulesmd.ini", "artmd.ini") from ra2md.mix ONCE per mode
// entry point (nested fallback chain -- these INIs are not always a
// top-level ra2md.mix entry in a stock install; mirrors the exact fallback
// chain proven against the real install in tests/test_rules.cpp's real-asset
// test). The returned Ini must outlive any Rules/Art built from it (both wrap
// their Ini by reference -- see rules.hpp/art.hpp) -- callers keep both as
// locals in the same function scope.
Ini load_ini_from_ra2md(const std::string& dir, const std::string& name) {
    MixFile top = MixFile::open(ra2assets::read_file(dir + "ra2md.mix"));

    std::vector<uint8_t> ini_bytes;
    if (const MixEntry* e = top.find(mix_id(name))) {
        ini_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",  "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix",  "conqmd.mix",
        };
        for (const char* cand : candidates) {
            try {
                MixFile nested = top.open_nested(cand);
                if (const MixEntry* e2 = nested.find(mix_id(name))) {
                    ini_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
                // Nested mix not present in this install/layout; try the next.
            }
        }
    }
    if (ini_bytes.empty())
        throw std::runtime_error(name + " not found in ra2md.mix (top level or nested)");
    return Ini::parse(std::string(ini_bytes.begin(), ini_bytes.end()));
}

// --- Audio Playback phase, Task 6: SDL sink + game audio wiring ---------
//
// One unit-type's soundmd.ini section names for its voice/report lines.
// VoiceSelect/VoiceMove/VoiceAttack/DieSound come straight from Rules;
// `report` is the unit's PRIMARY WEAPON's Report= (docs/re/audio-playback.md:
// Report lives on the weapon, not the unit). Any field may be "" if
// rulesmd.ini omits it -- looking that up in AudioSystem::voices and playing
// it is then just a silent no-op (Mixer::play on an empty/unregistered key
// returns false).
struct UnitVoices { std::string select, move, attack, die, report; };

// Everything the audio wiring needs at runtime. `mixer` is a unique_ptr (not
// a plain Mixer member) because Mixer stores its SoundBank BY REFERENCE (see
// engine/audio_mixer.hpp): init_audio() fully populates `bank` first (every
// voice/report/die clip's variants, then the EVA clip), and only THEN
// constructs the Mixer over the now-stable bank -- so the reference the
// Mixer holds is never invalidated by an earlier/later bank.add() call, and
// AudioSystem itself never needs to move once init_audio returns.
// `enabled` gates ALL per-frame audio work (event collection/acks/pump in
// run_interactive) -- stays false when --no-audio was passed (init_audio is
// then never called at all) or the SDL device failed to open, giving the
// "fully silent game, no crash" robustness guarantee.
struct AudioSystem {
    ra2engine::SoundBank bank;
    std::unique_ptr<ra2engine::Mixer> mixer;
    ra2engine::AudioOutput out;
    std::map<std::string, UnitVoices> voices;  // by image ("GI","CONS")
    int eva_unitlost = -1;                     // bank clip id, -1 if unavailable
    bool enabled = false;
};

// Audio-local RNG seed for Mixer's variant-selection RNG (docs/re/
// audio-playback.md's RNG-determinism caveat: audio must draw from its OWN
// RNG, never the sim's Randomizer, so --demo-combat's byte-identity gate
// stays unaffected by audio; any fixed constant is fine here since nothing
// about audio playback needs to be bit-reproducible across runs).
constexpr uint32_t kAudioRngSeed = 0x52413221u;

bool has_token(const std::vector<std::string>& tokens, const char* tok) {
    return std::find(tokens.begin(), tokens.end(), tok) != tokens.end();
}

// Resolves every variant name of one soundmd.ini `[key]` section to decoded
// PCM (via `idx`/`bag`, the audio.mix BAG chain), adding each clip to
// `audio.bank` as it goes, and returns the MixerSound to register for `key`.
// NOT registered here -- init_audio collects these into a temporary map and
// registers them only after audio.mixer is constructed (see AudioSystem's
// mixer comment: the bank must be complete before the Mixer is built over
// it). A missing idx entry is logged once and that variant is skipped
// (soundmd.ini's own documented "missing sample" tolerance, docs/re/
// audio-playback.md finding 1); a key with zero resolved variants still
// gets a MixerSound back (empty `variants`) -- Mixer::play() already treats
// that as an unconditional no-op, so there is no separate "unregistered"
// state to implement here.
ra2engine::MixerSound resolve_sound_event(AudioSystem& audio, const ra2assets::Sounds& sounds,
                                          const ra2assets::AudioIdx& idx,
                                          const std::vector<uint8_t>& bag, const std::string& key) {
    const ra2assets::SoundEntry e = sounds.entry(key);
    ra2engine::MixerSound ms;
    for (const std::string& variant : e.sounds) {
        const ra2assets::BagClip* clip = idx.find(variant);
        if (!clip) {
            std::cerr << "[audio] missing clip " << variant << " (key " << key << ")\n";
            continue;
        }
        ra2assets::AudioClipInfo info;
        std::vector<int16_t> pcm = ra2assets::decode_bag_clip(bag.data(), bag.size(), *clip, info);
        ms.variants.push_back(audio.bank.add(std::move(pcm)));
    }
    ms.volume = e.volume;
    // AMENDED (docs/re/audio-playback.md "Decisions for ra2engine"): EVERY
    // event attenuates by distance, voice acks included -- they inherit
    // [Defaults] Type=SCREEN and are positional too, just usually near full
    // volume since they play near an on-screen unit. GLOBAL only adds the
    // MinVolume floor inside gain_permille; it does not mean "non-positional".
    ms.priority = ra2engine::priority_from_string(e.priority);
    ms.limit = e.limit;
    ms.interrupt = has_token(e.control, "interrupt");
    ms.global = has_token(e.type, "global");
    ms.range = e.range;
    ms.min_volume = e.min_volume;
    return ms;
}

// Loads soundmd.ini + the audio.mix BAG chain (language.mix -> audio.mix ->
// audio.idx/audio.bag, the exact chain tests/test_audio.cpp:305-311 uses),
// resolves E1 (GI)/E2 (CONS)'s five voice-line keys apiece (select/move/
// attack/die/report), decodes every variant into audio.bank, builds the
// Mixer over the now-complete bank, and registers every key. Also loads the
// EVA "Unit lost" clip ("ceva064.wav", same audio.mix, WAV/IMA path -- Task
// 3 -- not the BAG path) and opens the SDL sink. NOT called for --no-audio
// (main() skips this call entirely -- see run_interactive/mode_demo_audio)
// or for --demo-combat (its byte-identity gate stays provably unaffected by
// audio).
void init_audio(AudioSystem& audio, const std::string& dir, const Rules& rules) {
    Ini snd_ini = load_ini_from_ra2md(dir, "soundmd.ini");
    ra2assets::Sounds sounds = ra2assets::Sounds::load(snd_ini);

    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    ra2assets::MixFile audio_mix = language.open_nested("audio.mix");
    const ra2assets::MixEntry* ie = audio_mix.find("audio.idx");
    if (!ie) throw std::runtime_error("audio.idx not found in language.mix/audio.mix");
    auto idx_bytes = audio_mix.read(*ie);
    ra2assets::AudioIdx idx = ra2assets::AudioIdx::load(idx_bytes.data(), idx_bytes.size());
    const ra2assets::MixEntry* be = audio_mix.find("audio.bag");
    if (!be) throw std::runtime_error("audio.bag not found in language.mix/audio.mix");
    std::vector<uint8_t> bag = audio_mix.read(*be);

    // Resolve both squads' voice/report keys, decoding every referenced clip
    // into audio.bank as we go; collect the MixerSounds in `pending` and
    // register them only once the bank -- and thus the Mixer built over it --
    // is complete (see AudioSystem's mixer comment above).
    std::map<std::string, ra2engine::MixerSound> pending;
    // HTNK (Rhino) added for the Voxel Vehicles phase: the per-image UnitVoices
    // machinery below handles it exactly like the infantry ids (image "HTNK").
    for (const char* unit_id : {"E1", "E2", "HTNK"}) {
        const std::string image = rules.image(unit_id);
        UnitVoices v;
        v.select = rules.voice_select(unit_id);
        v.move = rules.voice_move(unit_id);
        v.attack = rules.voice_attack(unit_id);
        v.die = rules.die_sound(unit_id);
        v.report = rules.weapon_report(rules.primary(unit_id));
        audio.voices[image] = v;

        for (const std::string& key : {v.select, v.move, v.attack, v.die, v.report}) {
            if (key.empty() || pending.count(key)) continue;
            pending[key] = resolve_sound_event(audio, sounds, idx, bag, key);
        }
    }

    // EVA: "ceva064.wav" ("Unit lost") lives in the SAME audio.mix, decoded
    // via load_wav_ima (the WAV/IMA container, Task 3), not decode_bag_clip.
    try {
        const ra2assets::MixEntry* we = audio_mix.find("ceva064.wav");
        if (!we) throw std::runtime_error("ceva064.wav not found in language.mix/audio.mix");
        std::vector<uint8_t> wav_bytes = audio_mix.read(*we);
        ra2assets::WavInfo info;
        std::vector<int16_t> pcm = ra2assets::load_wav_ima(wav_bytes.data(), wav_bytes.size(), info);
        audio.eva_unitlost = audio.bank.add(std::move(pcm));
    } catch (const std::exception& ex) {
        std::cerr << "[audio] EVA unavailable: " << ex.what() << "\n";
        audio.eva_unitlost = -1;
    }

    // The bank is now complete -- safe to build the Mixer over it and
    // register every resolved key.
    audio.mixer = std::make_unique<ra2engine::Mixer>(audio.bank, kAudioRngSeed);
    for (auto& [key, ms] : pending) audio.mixer->register_sound(key, ms);

    audio.out.open();
    audio.enabled = audio.out.ok();
    std::cerr << "[audio] device=" << (audio.out.ok() ? "ok" : "none")
              << " clips=" << audio.bank.count() << "\n";
}

// One unit id's full combat + movement profile, resolved from an
// already-loaded Rules: Image= (SHP base name), Speed=, Strength=, Armor=,
// then the Primary= weapon's Damage=/ROF=/Range=/Warhead=, then that
// warhead's Verses=.
struct ResolvedUnit {
    ra2engine::UnitCombat combat;
    int speed = 0;
};

// Resolves one unit id and logs it (extends the old load_unit_assets [unit]
// log line with the new combat fields) so a real-install run visibly
// confirms which numbers got pinned for E1/E2 (Task 8 ground truth: E1 GI
// M60 15/20/4 SA armor=none str=125; E2 CONS M1Carbine 15/25/4 SA armor=flak
// str=125).
ResolvedUnit resolve_combat_unit(const Rules& rules, const std::string& unit_id) {
    ResolvedUnit r;
    r.combat.image = rules.image(unit_id);
    r.speed = rules.speed(unit_id);
    r.combat.strength = rules.strength(unit_id);
    r.combat.armor = rules.armor(unit_id);
    const std::string primary = rules.primary(unit_id);
    r.combat.damage = rules.weapon_damage(primary);
    r.combat.rof = rules.weapon_rof(primary);
    r.combat.range = rules.weapon_range(primary);
    const std::string warhead = rules.weapon_warhead(primary);
    r.combat.verses = rules.verses(warhead);

    std::cerr << "[unit] id=" << unit_id << " image=" << r.combat.image << " speed=" << r.speed
              << " strength=" << r.combat.strength << " armor=" << r.combat.armor
              << " primary=" << primary << " damage=" << r.combat.damage
              << " rof=" << r.combat.rof << " range=" << r.combat.range
              << " warhead=" << warhead << "\n";
    return r;
}

// Resolves one image's stand/walk/fire/die animation sequence from an
// already-loaded Art (artmd.ini's [image] Sequence= -> that [SequenceName]
// section's Ready/Walk/FireUp/Die1 entries -- docs/re/infantry-anim.md).
// Logs the fire/die entries (mirrors resolve_combat_unit's [unit] log line)
// so a real-install run visibly confirms the real artmd.ini numbers got
// pinned (ground truth: GI/CONS fireup=164,6,6 die1=134,15).
ra2engine::UnitAnimSeq anim_from_art(const ra2assets::Art& art, const std::string& image) {
    auto conv = [&](const char* name) {
        const ra2assets::ArtSequenceEntry e = art.entry(image, name);
        return ra2engine::SeqEntry{e.start, e.count, e.facing_mult};
    };
    ra2engine::UnitAnimSeq seq{conv("Ready"), conv("Walk"), conv("FireUp"), conv("Die1")};
    std::cerr << "[anim] image=" << image << " fireup=" << seq.fire.first << ","
              << seq.fire.count << "," << seq.fire.fmult << " die1=" << seq.die.first
              << "," << seq.die.count << "\n";
    return seq;
}

// Loads one image's SHP from ra2.mix/conquer.mix ("<Image>.shp", falling
// back to the lowercased form -- conquer.mix's stock entry names are
// lowercase, e.g. "gi.shp"/"cons.shp"; try as-is first in case a mod/layout
// differs).
ShpFile load_unit_shp(const MixFile& conquer, const std::string& image) {
    std::string shp_name = image + ".shp";
    const MixEntry* se = conquer.find(shp_name);
    if (!se) {
        std::string lower = shp_name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        se = conquer.find(lower);
        shp_name = lower;
    }
    if (!se)
        throw std::runtime_error("unit SHP not found in ra2.mix/conquer.mix: " + image + ".shp");
    ShpFile shp = ShpFile::load(conquer.read(*se));
    std::cerr << "[shp] image=" << image << " file=" << shp_name << " frames=" << shp.count()
              << " canvas=" << shp.width() << "x" << shp.height() << "\n";
    return shp;
}

// Loads everything both squads need: rulesmd.ini once -> ONE Rules -> E1
// (GI) + E2 (Conscript) resolved combat/speed profiles, their two SHPs
// (GI.SHP, CONS.SHP), and the shared unittem.pal. `ra2` is the already-open
// ra2.mix shared with load_assets() (opened once by the mode entry points --
// avoids slurping the 268 MB file again).
UnitAssets load_unit_assets(const std::string& dir, const ra2assets::MixFile& ra2) {
    Ini ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(ini);

    const ResolvedUnit gi = resolve_combat_unit(rules, "E1");
    const ResolvedUnit cons = resolve_combat_unit(rules, "E2");

    MixFile conquer = ra2.open_nested("conquer.mix");
    UnitAssets ua;
    ua.shps.emplace(gi.combat.image, load_unit_shp(conquer, gi.combat.image));
    ua.shps.emplace(cons.combat.image, load_unit_shp(conquer, cons.combat.image));

    MixFile cache = ra2.open_nested("cache.mix");
    const MixEntry* pe = cache.find(std::string("unittem.pal"));
    if (!pe) throw std::runtime_error("unittem.pal not found in ra2.mix/cache.mix");
    auto pal_raw = cache.read(*pe);
    ua.pal = Palette::load(pal_raw.data(), pal_raw.size());

    // artmd.ini: resolve both images' animation sequences. The Ini/Art locals
    // stay alive only for this scope (Art wraps its Ini by reference -- see
    // art.hpp), but anim_from_art returns plain UnitAnimSeq values, so storing
    // them in ua.anims after art/art_ini go out of scope is safe.
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    ua.anims.emplace(gi.combat.image, anim_from_art(art, gi.combat.image));
    ua.anims.emplace(cons.combat.image, anim_from_art(art, cons.combat.image));

    ua.gi = gi.combat;
    ua.gi_speed = gi.speed;
    ua.cons = cons.combat;
    ua.cons_speed = cons.speed;
    return ua;
}

// Constructs the World (places the unit on a passable Clear cell -- see
// world.hpp/pick_start_cell) and tags its Unit with the resolved image.
// Walk pace tuning: iso cells/sec = rules Speed * this. rulesmd Speed for E1
// is 4, so 4 * 0.75 = 3 cells/sec -- fast enough to visibly stride between
// tiles, slow enough to read as walking (not teleporting) and to cross a
// ~25-cell demo path in well under the 15s cap. Falls back to World's own
// default if Speed is missing (0).
constexpr double kCellsPerSecPerSpeed = 0.75;

// Spawns `count` combat units of the given type/house on distinct walkable,
// unoccupied cells, spiralling ring-by-ring out from (origin_x, origin_y).
// Shared by both squads in build_world so their spawn geometry -- and thus
// --demo-combat's bit-identical replay, which depends on that geometry being
// reproduced exactly every run -- comes from one code path.
void spawn_squad(ra2engine::World& world, int origin_x, int origin_y, int count, int house,
                 const ra2engine::UnitCombat& combat) {
    int placed = 0;
    for (int r = 0; r <= 12 && placed < count; ++r)
        for (int dy = -r; dy <= r && placed < count; ++dy)
            for (int dx = -r; dx <= r && placed < count; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // ring border only
                const int x = origin_x + dx, y = origin_y + dy;
                if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                    world.add_combat_unit(x, y, house, combat);
                    ++placed;
                }
            }
}

// Loads everything the Rhino (or any single voxel vehicle) needs to render +
// fight: its combat/locomotion profile from rulesmd.ini, the hull/turret/barrel
// VXL+HVA parts + voxels.vpl from ra2.mix -> local.mix, six load-time facing
// caches (hull/turret/barrel x house 0/1, palette per house via pal_for_house
// over the shared unit palette `unit_pal` == ua.pal), the PrimaryFireFLH muzzle
// offset from artmd.ini, and the death-explosion SHP + its palette. EVERY step
// is guarded: on ANY failure the whole load is abandoned (logged once) and `ok`
// stays false, so the game runs infantry-only and never crashes. `ra2` is the
// shared already-open ra2.mix. Turret/barrel absence is not an error for a
// non-turreted vehicle (Rules::has_turret gates the attempt).
VehicleAssets load_vehicle_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                                  const ra2assets::Palette& unit_pal, const std::string& unit_id) {
    VehicleAssets va;
    try {
        Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
        Rules rules = Rules::load(rules_ini);
        Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
        Art art = Art::load(art_ini);

        const ResolvedUnit ru = resolve_combat_unit(rules, unit_id);
        va.combat = ru.combat;
        va.combat.is_vehicle = true;
        va.combat.rot = rules.rot(unit_id);
        va.combat.turret = rules.has_turret(unit_id);
        va.combat.speed_cps = ru.speed * kCellsPerSecPerSpeed;  // Speed 6 -> 4.5 cps

        const std::string image = va.combat.image;  // "HTNK"
        std::string base = image;
        for (char& ch : base) ch = static_cast<char>(std::tolower((unsigned char)ch));  // "htnk"

        MixFile local = ra2.open_nested("local.mix");
        auto load_part = [&](const std::string& part_base, ra2assets::VxlModel& m,
                             ra2assets::HvaModel& h) -> bool {
            const MixEntry* ve = local.find(part_base + ".vxl");
            const MixEntry* he = local.find(part_base + ".hva");
            if (!ve || !he) return false;
            auto vr = local.read(*ve);
            auto hr = local.read(*he);
            m = ra2assets::load_vxl(vr.data(), vr.size());
            h = ra2assets::load_hva(hr.data(), hr.size());
            return true;
        };

        ra2assets::VxlModel hull_m, tur_m, barl_m;
        ra2assets::HvaModel hull_h, tur_h, barl_h;
        if (!load_part(base, hull_m, hull_h))
            throw std::runtime_error("hull vxl/hva missing (" + base + ")");

        va.has_turret = rules.has_turret(unit_id);
        if (va.has_turret) {
            if (!load_part(base + "tur", tur_m, tur_h))
                throw std::runtime_error("turret vxl/hva missing (" + base + "tur)");
            va.has_barrel = load_part(base + "barl", barl_m, barl_h);  // barrel absence OK
        }

        const MixEntry* pe = local.find("voxels.vpl");
        if (!pe) throw std::runtime_error("voxels.vpl missing");
        auto vpl_raw = local.read(*pe);
        ra2assets::Vpl vpl = ra2assets::Vpl::load(vpl_raw.data(), vpl_raw.size());

        art.fire_flh(image, va.flh);  // leaves {0,0,0} if absent/malformed

        for (int house = 0; house <= 1; ++house) {
            const ra2assets::Palette hp = ra2engine::pal_for_house(unit_pal, house);
            va.hull[house] =
                ra2engine::build_facing_set(hull_m, hull_h, ra2engine::kVehicleFacings, vpl, hp);
            if (va.has_turret)
                va.turret[house] =
                    ra2engine::build_facing_set(tur_m, tur_h, ra2engine::kVehicleFacings, vpl, hp);
            if (va.has_barrel)
                va.barrel[house] =
                    ra2engine::build_facing_set(barl_m, barl_h, ra2engine::kVehicleFacings, vpl, hp);
        }

        // Death explosion (Rules::explosion_first -> SHP from conquer.mix,
        // drawn with anim.pal from cache.mix if present, else the unit pal).
        // Individually guarded: a missing explosion downgrades the death visual
        // (tank simply vanishes) but does NOT fail the whole vehicle load.
        va.explosion_image = rules.explosion_first(unit_id);
        va.explosion_pal = unit_pal;
        if (!va.explosion_image.empty()) {
            try {
                MixFile conquer = ra2.open_nested("conquer.mix");
                va.explosion =
                    std::make_unique<ShpFile>(load_unit_shp(conquer, va.explosion_image));
                try {
                    MixFile cache = ra2.open_nested("cache.mix");
                    const MixEntry* ape = cache.find(std::string("anim.pal"));
                    if (ape) {
                        auto apal = cache.read(*ape);
                        va.explosion_pal = Palette::load(apal.data(), apal.size());
                    }
                } catch (const std::exception&) { /* keep unit pal */ }
            } catch (const std::exception& ex) {
                std::cerr << "[vehicle] explosion SHP unavailable: " << ex.what() << "\n";
                va.explosion.reset();
            }
        }

        va.ok = true;
        const auto& hs = hull_m.sections.empty() ? ra2assets::VxlSection{} : hull_m.sections[0];
        std::cerr << "[vehicle] image=" << image << " facings=" << ra2engine::kVehicleFacings
                  << " hull=" << hs.size_x << "x" << hs.size_y << "x" << hs.size_z
                  << " turret=" << (va.has_turret ? "yes" : "no")
                  << " barrel=" << (va.has_barrel ? "yes" : "no")
                  << " speed_cps=" << va.combat.speed_cps << " rot=" << va.combat.rot << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "[vehicle] assets unavailable (" << ex.what()
                  << "); running infantry-only\n";
        va.ok = false;
    }
    return va;
}

// Spawns `n` Rhino tanks per side (docs/re/voxel-render.md spawn rules): house 0
// 4 cells WEST of the GI origin (pick_start_cell), house 1 4 cells EAST of the
// CONS origin (GI origin + 8 east). Reuses spawn_squad verbatim with va.combat.
// Called ONLY when va.ok, and NEVER from the byte-identity-gated demos
// (--demo-combat/move/retarget/group) -- interactive/screenshot/demo-tank only.
void spawn_tank_squads(ra2engine::World& world, const Assets& a, const VehicleAssets& va, int n) {
    const ra2assets::MapCell* c = ra2engine::pick_start_cell(a.map);
    if (!c) return;
    spawn_squad(world, c->x - 4, c->y, n, /*house=*/0, va.combat);
    spawn_squad(world, c->x + 12, c->y, n, /*house=*/1, va.combat);
    std::cerr << "[world] spawned " << n << " tank(s)/side; total units now "
              << world.units().size() << "\n";
}

// Spawn `count` GI units (house 0, player/Allied) on distinct walkable cells
// spiralling out from the map's start cell (pick_start_cell), plus `count`
// Conscript units (house 1, hostile/Soviet) spiralling out from 8 cells east
// of it. count==1 reproduces the old single-GI slice (used by the
// single-unit demos, which now also get one hostile Conscript alongside --
// harmless for movement-only demos); the interactive/screenshot/group/combat
// modes pass a small squad. Hostiles get no orders here -- they auto-acquire
// and retaliate once combat starts (see engine/combat.cpp step_combat).
ra2engine::World build_world(const Assets& a, const UnitAssets& ua, int count) {
    ra2engine::World world(a.map);
    if (ua.gi_speed > 0) world.set_move_speed(ua.gi_speed * kCellsPerSecPerSpeed);

    // Register each image's real artmd.ini-resolved anim sequence BEFORE any
    // units are spawned (set_anim/anim_for_image, world.hpp) -- units spawned
    // below immediately animate with these instead of falling back to
    // default_anim().
    for (const auto& [image, seq] : ua.anims) world.set_anim(image, seq);

    const ra2assets::MapCell* c = ra2engine::pick_start_cell(a.map);
    spawn_squad(world, c->x, c->y, count, /*house=*/0, ua.gi);
    spawn_squad(world, c->x + 8, c->y, count, /*house=*/1, ua.cons);

    std::cerr << "[world] spawned " << world.units().size() << " unit(s) near (" << c->x << ","
              << c->y << ") grid=" << world.grid_w() << "x" << world.grid_h()
              << " move_cps=" << world.move_speed() << "\n";
    return world;
}

// Draws one voxel vehicle in render_world's back-to-front loop: a dying tank
// shows the death-explosion SHP (frame = min(die_time*15, count-1), the same
// 15 fps as infantry deaths); a live tank draws its hull, then (if turreted)
// turret + barrel facing sprites via the cached FacingSets, then -- while
// firing -- a muzzle flash at the PrimaryFireFLH point (docs/re/voxel-render.md
// §6: rotate (F, L) by the quantized turret yaw, leptons->cells /256, project
// with the same 60x30 diamond shear draw_unit uses; H raises the flash).
void draw_vehicle_unit(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::Unit& u,
                       const VehicleAssets& va, const ra2engine::Camera& cam, int W, int H) {
    if (u.die_time >= 0.0) {
        if (va.explosion && va.explosion->count() > 0) {
            int frame = static_cast<int>(u.die_time * 15.0);
            if (frame >= va.explosion->count()) frame = va.explosion->count() - 1;
            if (frame < 0) frame = 0;
            // Reuse draw_unit's SHP canvas-center anchor. house 1 -> identity
            // pal_for_house remap, so the explosion keeps its own fire colors
            // (a house-0 remap would recolor any 16..31 indices to blue).
            ra2engine::Unit tmp = u;
            tmp.frame = frame;
            tmp.image = va.explosion_image;
            tmp.house = 1;
            ra2engine::draw_unit(ren, tc, tmp, *va.explosion, va.explosion_pal, cam, W, H);
        }
        return;  // dying vehicle: explosion instead of the tank
    }

    const std::string& img = va.combat.image;
    const int bi = ra2engine::facing_to_index(u.body_facing, ra2engine::kVehicleFacings);
    ra2engine::draw_voxel_sprite(
        ren, tc, "vxl:" + img + ":h:" + std::to_string(u.house) + ":" + std::to_string(bi),
        va.hull[u.house].sprites[bi], u.fx, u.fy, u.level, cam, W, H);
    if (va.has_turret) {
        const int ti = ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings);
        ra2engine::draw_voxel_sprite(
            ren, tc, "vxl:" + img + ":t:" + std::to_string(u.house) + ":" + std::to_string(ti),
            va.turret[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H);
        if (va.has_barrel)
            ra2engine::draw_voxel_sprite(
                ren, tc, "vxl:" + img + ":b:" + std::to_string(u.house) + ":" + std::to_string(ti),
                va.barrel[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H);
    }

    // Muzzle flash at the FLH point while firing (voxel-render.md §6).
    if (u.fire_flash > 0.0) {
        const double yaw = ra2engine::index_to_yaw(
            ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings),
            ra2engine::kVehicleFacings);
        const double fc = va.flh[0] / 256.0, lc = va.flh[1] / 256.0;  // leptons -> cells
        const double mx = fc * std::cos(yaw) - lc * std::sin(yaw);    // cells, model X
        const double my = fc * std::sin(yaw) + lc * std::cos(yaw);    // cells, model Y
        const int flash_dx = static_cast<int>(std::lround((mx + my) * 30.0));
        const int flash_dy =
            static_cast<int>(std::lround((mx - my) * 15.0 - (va.flh[2] / 256.0) * 30.0));
        const int cx = static_cast<int>(std::lround((u.fx - u.fy) * 30.0)) + cam.x + 30;
        const int cy =
            static_cast<int>(std::lround((u.fx + u.fy) * 15.0 - u.level * 15)) + cam.y + 15;
        SDL_Rect fr{cx + flash_dx - 3, cy + flash_dy - 3, 6, 6};
        if (!(fr.x + fr.w <= 0 || fr.y + fr.h <= 0 || fr.x >= W || fr.y >= H)) {
            SDL_SetRenderDrawColor(ren, 255, 230, 120, 255);
            SDL_RenderFillRect(ren, &fr);
        }
    }
}

// Draws one full frame of the world into the current render target (does NOT
// clear or present): terrain, the ordered path + selection diamond (alive,
// non-dying units only), the unit sprites (back-to-front; dying units
// included so their die anim plays, tombstoned units excluded -- see
// unit_draw_order), HP bars (alive, non-dying, selected-or-damaged units),
// then the live selection box on top. Shared by every mode so they stay
// visually consistent. House color (pal_for_house) is applied inside
// draw_unit itself via its house-keyed texture cache entry -- no extra work
// needed here. `va` (optional): when non-null AND va->ok, units whose image
// matches the vehicle's draw via voxel sprites (draw_vehicle_unit) instead of
// SHP, and their selection diamond / HP bar size to the hull sprite width.
// Passed nullptr by the SHP-only modes (including --demo-combat, whose
// byte-identity gate requires tanks NEVER spawn there).
void render_world(SDL_Renderer* ren, ra2engine::TextureCache& tc, Assets& a,
                  UnitAssets& ua, ra2engine::World& world, const ra2engine::Camera& cam,
                  int W, int H, const ra2engine::Input* input = nullptr,
                  const VehicleAssets* va = nullptr) {
    ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H);
    const std::vector<ra2engine::Unit>& us = world.units();
    // Hull sprite width for a vehicle unit (0 for infantry) -- sizes the
    // selection diamond + HP bar to the wider voxel sprite.
    auto veh_width = [&](const ra2engine::Unit& u) -> int {
        if (!(va && va->ok && u.image == va->combat.image)) return 0;
        const int bi = ra2engine::facing_to_index(u.body_facing, ra2engine::kVehicleFacings);
        return va->hull[u.house].sprites[bi].w;
    };
    // Paths + selection markers first (under the sprites); skip dead/dying
    // units entirely (no path, no selection marker for a corpse-in-progress).
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0) continue;
        ra2engine::draw_path(ren, u, cam, W, H);
        if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H, veh_width(u));
    }
    // Sprites back-to-front so nearer units overlap farther ones. Vehicles
    // (an image with a VehicleAssets entry) draw via voxel sprites; otherwise
    // pick the SHP by the unit's own image ("GI"/"CONS"). An unrecognized
    // image is logged and skipped (not thrown) so one bad unit can't take
    // down the whole frame.
    for (int idx : ra2engine::unit_draw_order(us)) {
        const ra2engine::Unit& u = us[idx];
        if (va && va->ok && u.image == va->combat.image) {
            draw_vehicle_unit(ren, tc, u, *va, cam, W, H);
            continue;
        }
        auto it = ua.shps.find(u.image);
        if (it == ua.shps.end()) {
            std::cerr << "[render] no SHP loaded for image=" << u.image << " (unit " << idx
                      << "); skipping\n";
            continue;
        }
        ra2engine::draw_unit(ren, tc, u, it->second, ua.pal, cam, W, H);
    }
    // HP bars after sprites, before the selection box: alive, non-dying units
    // that are selected or currently damaged.
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0) continue;
        if (u.selected || u.hp < u.max_hp) ra2engine::draw_hp_bar(ren, u, cam, W, H, veh_width(u));
    }
    // Live selection box on top (interactive only).
    if (input) {
        int x0, y0, x1, y1;
        if (input->selection_box(x0, y0, x1, y1))
            ra2engine::draw_selection_box(ren, x0, y0, x1, y1);
    }
}

// Read the framebuffer back and write it to a PNG. Throws on failure.
void capture_png(SDL_Renderer* ren, const std::string& out_path, int W, int H) {
    std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4);
    if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), W * 4) != 0)
        throw std::runtime_error(std::string("RenderReadPixels: ") + SDL_GetError());
    if (!stbi_write_png(out_path.c_str(), W, H, 4, px.data(), W * 4))
        throw std::runtime_error("stbi_write_png failed: " + out_path);
}

// Center the camera so the map's projected bounding-box center lands in the
// middle of the view (guarantees a populated region is shown).
ra2engine::Camera centered_camera(const MapFile& map, int view_w, int view_h) {
    ra2engine::Camera cam;
    int mnx, mny, mxx, mxy;
    if (ra2engine::terrain_screen_bounds(map, mnx, mny, mxx, mxy)) {
        cam.x = view_w / 2 - (mnx + mxx) / 2;
        cam.y = view_h / 2 - (mny + mxy) / 2;
    }
    return cam;
}

int run_screenshot(const std::string& out_path) {
    const int W = 1280, H = 720;
    // ra2.mix opened ONCE and shared by both loaders; the buffer stays
    // resident for the run (accepted tradeoff, see the load_assets comment).
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, "HTNK");
    ra2engine::World world = build_world(a, ua, 5);
    if (va.ok) spawn_tank_squads(world, a, va, 3);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (screenshot)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    // Scope the TextureCache so all SDL_Texture objects are destroyed BEFORE
    // the renderer / SDL_Quit below (destroying textures after SDL teardown is
    // an access violation).
    {
        ra2engine::TextureCache tc(ren);
        ra2engine::Camera cam = centered_camera(a.map, W, H);

        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, tc, a, ua, world, cam, W, H, /*input=*/nullptr, va.ok ? &va : nullptr);
        capture_png(ren, out_path, W, H);

        std::cerr << "[screenshot] wrote " << out_path << " (" << W << "x" << H << ")\n";
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

int run_interactive(bool no_audio) {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, "HTNK");
    ra2engine::World world = build_world(a, ua, 5);
    if (va.ok) spawn_tank_squads(world, a, va, 3);
    ra2engine::Platform platform = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(platform.renderer());

    ra2engine::Camera cam = centered_camera(a.map, platform.width(), platform.height());
    ra2engine::Input input(world, cam, platform.width(), platform.height());
    const int kPan = 40;

    // Audio wiring (Task 6): --no-audio skips init_audio entirely, leaving
    // `audio` default-constructed (enabled=false) -- same silent-fallback
    // outcome as a failed device open, just without ever touching the mix
    // chain. rulesmd.ini is re-parsed here (load_unit_assets already parsed
    // it once for combat/anim stats) purely for the Rules voice getters --
    // a one-time startup cost, not per-frame.
    AudioSystem audio;
    if (!no_audio) {
        Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
        Rules rules = Rules::load(rules_ini);
        init_audio(audio, dir, rules);
    }
    // fire_flash/die_time seen last frame, per unit index (resized as units()
    // grows; never shrinks, matching World's own tombstone-not-erase model).
    std::vector<std::pair<double, double>> prev_audio_state;

    Uint32 prev = SDL_GetTicks();
    while (!input.wants_quit()) {
        ra2engine::InputEvent ev;
        while (platform.poll(ev)) input.handle(ev);

        // Camera pan: arrow keys + screen-edge scroll (left-drag now box-selects).
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int pan_dx = 0, pan_dy = 0;
        if (ks[SDL_SCANCODE_LEFT]) pan_dx += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) pan_dx -= kPan;
        if (ks[SDL_SCANCODE_UP]) pan_dy += kPan;
        if (ks[SDL_SCANCODE_DOWN]) pan_dy -= kPan;
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        const int kEdge = 24;  // px band at the viewport edge that scrolls
        if (mx <= kEdge) pan_dx += kPan;
        else if (mx >= platform.width() - kEdge) pan_dx -= kPan;
        if (my <= kEdge) pan_dy += kPan;
        else if (my >= platform.height() - kEdge) pan_dy -= kPan;
        if (pan_dx || pan_dy) input.pan(pan_dx, pan_dy);

        const Uint32 now = SDL_GetTicks();
        const double dt = (now - prev) / 1000.0;
        prev = now;
        world.step(dt);

        // Audio event collection + acks (Task 6, Step 3). Gated on
        // audio.enabled so a --no-audio run or a failed device open touches
        // none of this (and, per input.hpp, Input::take_actions() -- the
        // ONE place per frame the accumulated select/move/attack flags are
        // consumed -- is only called here; nothing else in the engine reads
        // InputActions today).
        if (audio.enabled) {
            const double view_center_px_x = platform.width() / 2.0 - cam.x;
            const double view_center_px_y = platform.height() / 2.0 - cam.y;
            const auto& units = world.units();
            prev_audio_state.resize(units.size(), {0.0, -1.0});
            for (std::size_t i = 0; i < units.size(); ++i) {
                const auto& u = units[i];
                // Game metric (docs/re/audio-playback.md, High): Chebyshev
                // max(|dx|, 2|dy|) in SCREEN pixels from the view centre,
                // /60 to cells (the game multiplies Range by 60 px/cell;
                // dividing here keeps Mixer::gain_permille in cell units).
                // Same iso projection the renderer uses (iso_renderer.cpp
                // draw_unit / iso_project.hpp cell_to_screen(60,30,15)):
                // screen = (fx-fy)*30 + cam.x, (fx+fy)*15 + cam.y (level
                // ignored, per the brief's simplified formula).
                const double sx = (u.fx - u.fy) * 30.0 - view_center_px_x;
                const double sy = (u.fx + u.fy) * 15.0 - view_center_px_y;
                const double dist = std::max(std::abs(sx), 2.0 * std::abs(sy)) / 60.0;
                const auto& v = audio.voices[u.image];
                if (u.fire_flash > prev_audio_state[i].first)  // rising edge: fired
                    audio.mixer->play(v.report, dist);
                if (u.die_time >= 0.0 && prev_audio_state[i].second < 0.0) {  // started dying
                    audio.mixer->play(v.die, dist);
                    if (u.house == 0 && audio.eva_unitlost >= 0)
                        audio.mixer->play_eva(audio.eva_unitlost);
                }
                prev_audio_state[i] = {u.fire_flash, u.die_time};
            }
            const auto act = input.take_actions();  // ONCE/frame, consumes
            if (act.actor >= 0) {
                // Acks attenuate like every other event (docs/re/
                // audio-playback.md, AMENDED: positional = !GLOBAL for ALL
                // events -- acks inherit [Defaults] Type=SCREEN): same
                // Chebyshev metric as the report/die path above, on the
                // ACTOR unit's position. Usually near full volume, since
                // the actor was just clicked on-screen.
                const auto& au = world.units()[act.actor];
                const double asx = (au.fx - au.fy) * 30.0 - view_center_px_x;
                const double asy = (au.fx + au.fy) * 15.0 - view_center_px_y;
                const double adist = std::max(std::abs(asx), 2.0 * std::abs(asy)) / 60.0;
                const auto& av = audio.voices[au.image];
                if (act.selected) audio.mixer->play(av.select, adist);
                else if (act.attack_ordered) audio.mixer->play(av.attack, adist);
                else if (act.move_ordered) audio.mixer->play(av.move, adist);
            }
            audio.out.pump(*audio.mixer);
        }

        platform.begin_frame();
        render_world(static_cast<SDL_Renderer*>(platform.renderer()), tc, a, ua, world, cam,
                     platform.width(), platform.height(), &input, va.ok ? &va : nullptr);
        platform.present();
        SDL_Delay(16);  // ~60fps
    }
    return 0;
}

// Non-interactive verification of the whole select -> path -> walk pipeline:
// programmatically select the unit, order a move to (goal_x, goal_y), then
// run the sim (fixed 60Hz dt) until arrival or a 15s cap, rendering each
// frame. Captures a START screenshot (unit at origin, selected, path drawn)
// and an END screenshot (unit arrived), and prints start/target/path-len/
// final. Screenshots land in the scratchpad, never the repo.
int run_demo_move(int goal_x, int goal_y) {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "task12_start.png";
    const std::string end_png = scratch + "task12_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2engine::World world = build_world(a, ua, 1);

    const int start_x = world.units()[0].cell_x;
    const int start_y = world.units()[0].cell_y;

    // SELECT, then order the move.
    world.units()[0].selected = true;
    world.order_move_selected(goal_x, goal_y);
    const std::size_t path_len = world.units()[0].path.size();

    std::cerr << "[demo] start=(" << start_x << "," << start_y << ") target=(" << goal_x << ","
              << goal_y << ") path_len=" << path_len << " goal_walkable="
              << (world.walkable(goal_x, goal_y) ? "yes" : "no") << "\n";
    if (path_len == 0) {
        std::cerr << "[demo] no path to target (unreachable / off-grid / == start); aborting.\n";
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-move)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    int final_x = start_x, final_y = start_y;
    double sim_time = 0.0;
    {
        ra2engine::TextureCache tc(ren);
        // Center the (fixed) demo camera on the MIDPOINT of start->goal so
        // both endpoints stay comfortably in frame -- the map-centered camera
        // used elsewhere can leave a distant destination off-screen.
        ra2engine::Camera cam;
        {
            const ra2engine::ScreenPt mid = ra2engine::cell_to_screen(
                (start_x + goal_x) / 2, (start_y + goal_y) / 2, 0, 60, 30, 15);
            cam.x = W / 2 - mid.x;
            cam.y = H / 2 - mid.y;
        }

        auto draw = [&]() {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, tc, a, ua, world, cam, W, H);
        };

        // START frame: unit at origin, selected, path visible.
        draw();
        capture_png(ren, start_png, W, H);

        // Sim loop: fixed 60Hz steps until arrival or 15s of sim time.
        const double dt = 1.0 / 60.0;
        const double kMaxSim = 15.0;
        while (world.units()[0].moving() && sim_time < kMaxSim) {
            world.step(dt);
            sim_time += dt;
            draw();  // render every frame (exercises the render path)
        }

        final_x = world.units()[0].cell_x;
        final_y = world.units()[0].cell_y;

        // END frame: unit arrived (or stopped at the cap).
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cerr << "[demo] finished sim_time=" << sim_time << "s final=(" << final_x << ","
              << final_y << ") arrived=" << (!world.units()[0].moving() ? "yes" : "no (hit cap)")
              << "\n";
    std::cout << "start_cell=(" << start_x << "," << start_y << ") target=(" << goal_x << ","
              << goal_y << ") path_len=" << path_len << " final_cell=(" << final_x << ","
              << final_y << ")\n";
    std::cout << "start_png=" << start_png << "\n";
    std::cout << "end_png=" << end_png << "\n";
    return 0;
}

// Headless (no window/rendering) verification of the Task 12 review's
// Finding 1 fix: order a move to (x1,y1), let the sim run PARTWAY (not to
// completion -- so fx/fy is genuinely fractional/mid-walk), then retarget to
// a DIFFERENT cell (x2,y2) exactly as a live reorder would, and confirm (a)
// the retarget did NOT snap fx/fy backward to the last committed cell, and
// (b) the unit still ends up at the SECOND target, not stuck partway or
// stranded. Prints the numbers; exits 0 only if both checks hold.
int run_demo_retarget(int x1, int y1, int x2, int y2) {
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2engine::World world = build_world(a, ua, 1);

    const int start_x = world.units()[0].cell_x;
    const int start_y = world.units()[0].cell_y;

    world.units()[0].selected = true;
    world.order_move_selected(x1, y1);
    const std::size_t len1 = world.units()[0].path.size();
    std::cerr << "[demo-retarget] start=(" << start_x << "," << start_y << ") first_target=("
              << x1 << "," << y1 << ") path_len1=" << len1 << "\n";
    if (len1 == 0) {
        std::cerr << "[demo-retarget] no path to first target; aborting.\n";
        return 2;
    }

    // Step partway into the first order -- long enough to be well off the
    // starting cell, short enough (checked below) to still be mid-walk.
    const double dt = 1.0 / 60.0;
    double sim_time = 0.0;
    const double kRetargetAt = 1.5;  // seconds
    while (world.units()[0].moving() && sim_time < kRetargetAt) {
        world.step(dt);
        sim_time += dt;
    }
    const bool was_moving_mid = world.units()[0].moving();
    const double fx_mid = world.units()[0].fx;
    const double fy_mid = world.units()[0].fy;
    const int cell_mid_x = world.units()[0].cell_x;
    const int cell_mid_y = world.units()[0].cell_y;
    const bool genuinely_fractional = (fx_mid != cell_mid_x || fy_mid != cell_mid_y);

    // Retarget mid-walk.
    world.order_move_selected(x2, y2);
    const std::size_t len2 = world.units()[0].path.size();
    const bool no_backward_snap = (world.units()[0].fx == fx_mid && world.units()[0].fy == fy_mid);
    std::cerr << "[demo-retarget] retargeted at t=" << sim_time << "s (moving=" << was_moving_mid
              << " fractional=" << genuinely_fractional << ") mid_pos=(" << fx_mid << ","
              << fy_mid << ") second_target=(" << x2 << "," << y2 << ") path_len2=" << len2
              << " no_backward_snap=" << no_backward_snap << "\n";
    if (len2 == 0) {
        std::cerr << "[demo-retarget] no path to second target; aborting.\n";
        return 3;
    }

    // Run to arrival or a generous cap.
    const double kMaxSim = 30.0;
    while (world.units()[0].moving() && sim_time < kMaxSim) {
        world.step(dt);
        sim_time += dt;
    }
    const int final_x = world.units()[0].cell_x;
    const int final_y = world.units()[0].cell_y;
    const bool arrived_second = (final_x == x2 && final_y == y2);

    std::cout << "start=(" << start_x << "," << start_y << ") first_target=(" << x1 << "," << y1
              << ") second_target=(" << x2 << "," << y2 << ") mid_pos=(" << fx_mid << "," << fy_mid
              << ") no_backward_snap=" << (no_backward_snap ? "yes" : "no") << " final_cell=("
              << final_x << "," << final_y
              << ") arrived_at_second=" << (arrived_second ? "yes" : "no")
              << " sim_time=" << sim_time << "s\n";

    return (was_moving_mid && genuinely_fractional && no_backward_snap && arrived_second) ? 0 : 1;
}

// Non-interactive verification of the multi-unit pipeline: spawn `n` GI units,
// select them all, group-move to (gx, gy), run the sim to arrival (or a cap),
// and write before/after screenshots. Asserts the no-overlap invariant every
// step and prints per-unit arrival cells. Screenshots -> scratchpad only.
int run_demo_group(int n, int gx, int gy) {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_group_start.png";
    const std::string end_png = scratch + "demo_group_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2engine::World world = build_world(a, ua, n);
    for (auto& u : world.units()) u.selected = true;
    world.order_move_selected(gx, gy);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-group)", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
    if (!win) throw std::runtime_error(SDL_GetError());
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) throw std::runtime_error(SDL_GetError());

    bool overlap_seen = false;
    double sim_time = 0.0;
    {
        ra2engine::TextureCache tc(ren);
        ra2engine::Camera cam;
        {
            const ra2engine::ScreenPt mid = ra2engine::cell_to_screen(
                gx, gy, 0, 60, 30, 15);
            cam.x = W / 2 - mid.x;
            cam.y = H / 2 - mid.y;
        }
        auto draw = [&]() {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, tc, a, ua, world, cam, W, H);
        };
        auto any_moving = [&]() {
            for (const auto& u : world.units()) if (u.moving()) return true;
            return false;
        };
        auto check_overlap = [&]() {
            const auto& us = world.units();
            for (size_t i = 0; i < us.size(); ++i)
                for (size_t j = i + 1; j < us.size(); ++j)
                    if (us[i].cell_x == us[j].cell_x && us[i].cell_y == us[j].cell_y)
                        overlap_seen = true;
        };

        draw();
        capture_png(ren, start_png, W, H);

        const double dt = 1.0 / 60.0;
        const double kMaxSim = 30.0;
        while (any_moving() && sim_time < kMaxSim) {
            world.step(dt);
            check_overlap();
            sim_time += dt;
            draw();
        }
        draw();
        capture_png(ren, end_png, W, H);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::cout << "units=" << world.units().size() << " target=(" << gx << "," << gy
              << ") sim_time=" << sim_time << "s overlap_seen=" << (overlap_seen ? "YES" : "no")
              << "\n";
    for (size_t i = 0; i < world.units().size(); ++i)
        std::cout << "  unit " << i << " final=(" << world.units()[i].cell_x << ","
                  << world.units()[i].cell_y << ")\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    return overlap_seen ? 1 : 0;  // fail if the invariant was ever violated
}

// One deterministic run of the 5v5 skirmish (see run_demo_combat). Builds a
// FRESH World every call (so two calls from the same Assets/UnitAssets are
// truly independent sims, not two views of shared state), gives it the same
// orders, and steps it at a fixed 60Hz dt until one side is wiped out or the
// 120s cap is hit. `render` controls whether each frame is actually drawn
// (into the caller's already-created hidden window/renderer/TextureCache/
// camera) and whether start/end screenshots are captured -- render_world
// only READS Unit/World state to draw, so whether this flag is true or false
// must never change the resulting log; that invariant is exactly what
// run_demo_combat checks by diffing a render=true run against a render=false
// one.
struct DemoCombatResult {
    std::vector<std::string> log;  // per-frame hp-change + death lines, in order
    int shot_count = 0;            // number of hp-change lines (one per landed hit)
    int death_count = 0;           // number of "died" lines
    int gi_survivors = 0;          // house-0 alive & non-dying at sim end
    int cons_survivors = 0;        // house-1 alive & non-dying at sim end
    double sim_time = 0.0;
    bool overlap = false;          // two alive, non-dying units shared a cell at end
    bool dangling_target = false;  // some unit's target index was out of [-1, n)
    // --demo-tank only (unused by --demo-combat, which leaves them false): a
    // victim hp drop of exactly 90 on a Rhino (HTNK, heavy 100% of 120mm's
    // Damage=90 -- only a Rhino can inflict it) proves a Rhino-vs-Rhino hit;
    // exactly 22 on an infantry victim (none 25% -> trunc(90*0.25)) proves a
    // Rhino-vs-infantry hit.
    bool hit90_rhino = false;
    bool hit22_infantry = false;
};

DemoCombatResult simulate_combat(Assets& a, UnitAssets& ua, bool render, SDL_Renderer* ren,
                                 ra2engine::TextureCache* tc, const ra2engine::Camera* cam, int W,
                                 int H, const std::string& start_png, const std::string& end_png) {
    ra2engine::World world = build_world(a, ua, 5);

    // Select all house-0 (GI) units and order an attack on the single
    // hostile nearest their centroid (euclidean over spawn cells -- squared
    // distance is monotonic with euclidean, so no sqrt is needed just to
    // compare). Deterministic: build_world's spiral spawn reproduces the
    // exact same geometry every call, so this picks the same target index
    // every run.
    double cx = 0.0, cy = 0.0;
    int n_allied = 0;
    for (const auto& u : world.units())
        if (u.house == 0) { cx += u.cell_x; cy += u.cell_y; ++n_allied; }
    if (n_allied > 0) { cx /= n_allied; cy /= n_allied; }

    int nearest = -1;
    double best_d2 = 0.0;
    for (std::size_t i = 0; i < world.units().size(); ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (u.house == 0) continue;
        const double dx = u.cell_x - cx, dy = u.cell_y - cy;
        const double d2 = dx * dx + dy * dy;
        if (nearest < 0 || d2 < best_d2) { nearest = static_cast<int>(i); best_d2 = d2; }
    }
    for (auto& u : world.units())
        if (u.house == 0) u.selected = true;
    if (nearest >= 0) world.order_attack(nearest);
    // Hostiles (house 1) get NO orders -- they auto-acquire/retaliate once a
    // GI closes into range (engine/combat.cpp step_combat).

    DemoCombatResult result;
    const std::size_t nunits = world.units().size();
    std::vector<int> prev_hp(nunits);
    for (std::size_t i = 0; i < nunits; ++i) prev_hp[i] = world.units()[i].hp;
    std::vector<bool> prev_alive(nunits, true);

    auto alive_non_dying = [&](int house) {
        int n = 0;
        for (const auto& u : world.units())
            if (u.house == house && u.alive && u.die_time < 0.0) ++n;
        return n;
    };
    auto draw = [&]() {
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H);
    };

    if (render) {
        draw();
        capture_png(ren, start_png, W, H);
    }

    const double dt = 1.0 / 60.0;
    const double kMaxSim = 120.0;
    double t = 0.0;
    while (alive_non_dying(0) > 0 && alive_non_dying(1) > 0 && t < kMaxSim) {
        // Player-micro emulation (demo only): re-target idle units at the
        // nearest living hostile -- a pure function of world state, so the
        // bit-determinism gate still holds. Without this the demo faithfully
        // reproduces guard semantics (hold position when the ordered target
        // dies) and stalls at the cap.
        std::vector<int> idle_no_target;
        for (std::size_t i = 0; i < world.units().size(); ++i) {
            const ra2engine::Unit& u = world.units()[i];
            if (u.house == 0 && u.alive && u.die_time < 0.0 && !u.moving() && u.target == -1)
                idle_no_target.push_back(static_cast<int>(i));
        }
        for (int i : idle_no_target) {
            const double ux = world.units()[i].fx, uy = world.units()[i].fy;
            int nearest_h = -1;
            double best_d2 = 0.0;
            for (std::size_t j = 0; j < world.units().size(); ++j) {
                const ra2engine::Unit& h = world.units()[j];
                if (h.house != 1 || !h.alive || h.die_time >= 0.0) continue;
                const double dx = ux - h.fx, dy = uy - h.fy;
                const double d2 = dx * dx + dy * dy;
                if (nearest_h < 0 || d2 < best_d2) { nearest_h = static_cast<int>(j); best_d2 = d2; }
            }
            if (nearest_h < 0) break;  // no living hostiles left; nothing to re-order
            world.clear_selection();
            world.units()[i].selected = true;
            world.order_attack(nearest_h);
        }
        // Restore the "whole squad selected" visual the demo starts with --
        // the per-unit re-order passes above touch selection one unit at a
        // time via clear_selection()/order_attack().
        for (auto& u : world.units())
            if (u.house == 0 && u.alive && u.die_time < 0.0) u.selected = true;

        world.step(dt);
        t += dt;
        // Victim-centric per-hit log (plan amended): attacker attribution
        // would require instrumenting the locked combat core.
        for (std::size_t i = 0; i < nunits; ++i) {
            const ra2engine::Unit& u = world.units()[i];
            if (u.hp != prev_hp[i]) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "t=%.3f victim=%zu(%s) hp=%d->%d", t, i,
                              u.image.c_str(), prev_hp[i], u.hp);
                result.log.emplace_back(buf);
                ++result.shot_count;
                prev_hp[i] = u.hp;
            }
            if (prev_alive[i] && !u.alive) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "t=%.3f unit %zu(%s) died", t, i,
                              u.image.c_str());
                result.log.emplace_back(buf);
                ++result.death_count;
                prev_alive[i] = false;
            }
        }
        if (render) draw();
    }
    result.sim_time = t;
    if (render) {
        draw();
        capture_png(ren, end_png, W, H);
    }

    result.gi_survivors = alive_non_dying(0);
    result.cons_survivors = alive_non_dying(1);

    // End-state invariants: no dangling target indices, no two live units
    // sharing a cell (occupancy is released the instant a unit starts dying,
    // so the no-overlap check only applies to alive, non-dying units).
    for (std::size_t i = 0; i < nunits; ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (u.target < -1 || u.target >= static_cast<int>(nunits)) result.dangling_target = true;
    }
    for (std::size_t i = 0; i < nunits; ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (!u.alive || u.die_time >= 0.0) continue;
        for (std::size_t j = i + 1; j < nunits; ++j) {
            const ra2engine::Unit& v = world.units()[j];
            if (!v.alive || v.die_time >= 0.0) continue;
            if (u.cell_x == v.cell_x && u.cell_y == v.cell_y) result.overlap = true;
        }
    }
    return result;
}

// Tank-aware pattern-copy of simulate_combat (see that function's comments):
// builds the same 5 GI (house 0) vs 5 CONS (house 1) world, then adds 3 Rhinos
// per side (spawn_tank_squads), orders house-0 to attack the nearest hostile,
// re-targets idle units identically, and runs to wipe-out or the 120s cap.
// `render` draws every frame (with the vehicle draw path, &va) + start/end
// screenshots. Additionally records the two Rhino-hit witnesses: a victim hp
// drop of exactly 90 on a Rhino (Rhino-vs-Rhino) and exactly 22 on an infantry
// unit (Rhino-vs-infantry). Determinism holds for the same reason as
// simulate_combat (no combat RNG); render must not perturb the log.
DemoCombatResult simulate_tank_combat(Assets& a, UnitAssets& ua, const VehicleAssets& va,
                                      bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                      const ra2engine::Camera* cam, int W, int H,
                                      const std::string& start_png, const std::string& end_png) {
    ra2engine::World world = build_world(a, ua, 5);
    spawn_tank_squads(world, a, va, 3);

    double cx = 0.0, cy = 0.0;
    int n_allied = 0;
    for (const auto& u : world.units())
        if (u.house == 0) { cx += u.cell_x; cy += u.cell_y; ++n_allied; }
    if (n_allied > 0) { cx /= n_allied; cy /= n_allied; }

    int nearest = -1;
    double best_d2 = 0.0;
    for (std::size_t i = 0; i < world.units().size(); ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (u.house == 0) continue;
        const double dx = u.cell_x - cx, dy = u.cell_y - cy;
        const double d2 = dx * dx + dy * dy;
        if (nearest < 0 || d2 < best_d2) { nearest = static_cast<int>(i); best_d2 = d2; }
    }
    for (auto& u : world.units())
        if (u.house == 0) u.selected = true;
    if (nearest >= 0) world.order_attack(nearest);

    DemoCombatResult result;
    const std::size_t nunits = world.units().size();
    std::vector<int> prev_hp(nunits);
    for (std::size_t i = 0; i < nunits; ++i) prev_hp[i] = world.units()[i].hp;
    std::vector<bool> prev_alive(nunits, true);

    auto alive_non_dying = [&](int house) {
        int n = 0;
        for (const auto& u : world.units())
            if (u.house == house && u.alive && u.die_time < 0.0) ++n;
        return n;
    };
    auto draw = [&]() {
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, /*input=*/nullptr, &va);
    };

    if (render) {
        draw();
        capture_png(ren, start_png, W, H);
    }

    const double dt = 1.0 / 60.0;
    const double kMaxSim = 120.0;
    double t = 0.0;
    while (alive_non_dying(0) > 0 && alive_non_dying(1) > 0 && t < kMaxSim) {
        std::vector<int> idle_no_target;
        for (std::size_t i = 0; i < world.units().size(); ++i) {
            const ra2engine::Unit& u = world.units()[i];
            if (u.house == 0 && u.alive && u.die_time < 0.0 && !u.moving() && u.target == -1)
                idle_no_target.push_back(static_cast<int>(i));
        }
        for (int i : idle_no_target) {
            const double ux = world.units()[i].fx, uy = world.units()[i].fy;
            int nearest_h = -1;
            double best_dh2 = 0.0;
            for (std::size_t j = 0; j < world.units().size(); ++j) {
                const ra2engine::Unit& h = world.units()[j];
                if (h.house != 1 || !h.alive || h.die_time >= 0.0) continue;
                const double dx = ux - h.fx, dy = uy - h.fy;
                const double d2 = dx * dx + dy * dy;
                if (nearest_h < 0 || d2 < best_dh2) { nearest_h = static_cast<int>(j); best_dh2 = d2; }
            }
            if (nearest_h < 0) break;
            world.clear_selection();
            world.units()[i].selected = true;
            world.order_attack(nearest_h);
        }
        for (auto& u : world.units())
            if (u.house == 0 && u.alive && u.die_time < 0.0) u.selected = true;

        world.step(dt);
        t += dt;
        for (std::size_t i = 0; i < nunits; ++i) {
            const ra2engine::Unit& u = world.units()[i];
            if (u.hp != prev_hp[i]) {
                const int drop = prev_hp[i] - u.hp;
                const bool is_rhino = (u.image == va.combat.image);  // "HTNK"
                // A 90-drop can ONLY be a Rhino's 120mm (Damage=90) at heavy
                // 100%; a 22-drop is 90*25% (none) -- both are Rhino hits.
                if (drop == 90 && is_rhino) result.hit90_rhino = true;
                if (drop == 22 && !is_rhino) result.hit22_infantry = true;
                char buf[128];
                std::snprintf(buf, sizeof(buf), "t=%.3f victim=%zu(%s) hp=%d->%d", t, i,
                              u.image.c_str(), prev_hp[i], u.hp);
                result.log.emplace_back(buf);
                ++result.shot_count;
                prev_hp[i] = u.hp;
            }
            if (prev_alive[i] && !u.alive) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "t=%.3f unit %zu(%s) died", t, i, u.image.c_str());
                result.log.emplace_back(buf);
                ++result.death_count;
                prev_alive[i] = false;
            }
        }
        if (render) draw();
    }
    result.sim_time = t;
    if (render) {
        draw();
        capture_png(ren, end_png, W, H);
    }

    result.gi_survivors = alive_non_dying(0);
    result.cons_survivors = alive_non_dying(1);

    for (std::size_t i = 0; i < nunits; ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (u.target < -1 || u.target >= static_cast<int>(nunits)) result.dangling_target = true;
    }
    for (std::size_t i = 0; i < nunits; ++i) {
        const ra2engine::Unit& u = world.units()[i];
        if (!u.alive || u.die_time >= 0.0) continue;
        for (std::size_t j = i + 1; j < nunits; ++j) {
            const ra2engine::Unit& v = world.units()[j];
            if (!v.alive || v.die_time >= 0.0) continue;
            if (u.cell_x == v.cell_x && u.cell_y == v.cell_y) result.overlap = true;
        }
    }
    return result;
}

// --demo-tank: the voxel-vehicle counterpart of --demo-combat. 5 GI + 3 Rhinos
// (house 0) vs 5 CONS + 3 Rhinos (house 1); house 0 attacks the nearest
// hostile, idle units re-target exactly like simulate_combat. Runs the sim
// rendered (hidden window + start/end screenshots via the voxel draw path) and
// headless, byte-compares the two logs, and passes (exit 0) only if: logs
// identical, >=1 death, no overlap, no dangling target, AND both Rhino-hit
// witnesses fired (a 90-hp Rhino-vs-Rhino drop and a 22-hp Rhino-vs-infantry
// drop). Never touched by the byte-identity-gated demos.
int run_demo_tank() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_tank_start.png";
    const std::string end_png = scratch + "demo_tank_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, "HTNK");
    if (!va.ok) {
        std::cerr << "[demo-tank] FAILED: vehicle assets unavailable (see [vehicle] log)\n";
        return 1;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoCombatResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-tank)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_tank_combat(a, ua, va, /*render=*/true, ren, &tc, &cam, W, H, start_png,
                                      end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical spawn/orders from a FRESH World.
    DemoCombatResult r2 = simulate_tank_combat(a, ua, va, /*render=*/false, nullptr, nullptr,
                                               nullptr, W, H, "", "");

    const bool logs_identical = (r1.log == r2.log);
    const bool someone_died = (r1.death_count > 0 && r2.death_count > 0);
    const bool hits_present = r1.hit90_rhino && r1.hit22_infantry;
    const bool bad = !logs_identical || !someone_died || r1.overlap || r2.overlap ||
                     r1.dangling_target || r2.dangling_target || !hits_present;

    std::cerr << "[demo-tank] run1 lines=" << r1.log.size() << " run2 lines=" << r2.log.size()
              << " identical=" << (logs_identical ? "yes" : "NO") << " deaths(r1/r2)="
              << r1.death_count << "/" << r2.death_count << " overlap(r1/r2)="
              << (r1.overlap ? "Y" : "n") << "/" << (r2.overlap ? "Y" : "n") << " dangling(r1/r2)="
              << (r1.dangling_target ? "Y" : "n") << "/" << (r2.dangling_target ? "Y" : "n")
              << " hit90_rhino=" << (r1.hit90_rhino ? "Y" : "n")
              << " hit22_infantry=" << (r1.hit22_infantry ? "Y" : "n") << "\n";

    std::cout << "survivors: gi=" << r1.gi_survivors << " cons=" << r1.cons_survivors
              << " sim_time=" << r1.sim_time << "s\n";
    std::cout << "--- first 5 log lines ---\n";
    {
        const std::size_t first_n = r1.log.size() < 5 ? r1.log.size() : 5;
        for (std::size_t i = 0; i < first_n; ++i) std::cout << r1.log[i] << "\n";
    }
    std::cout << "--- last 5 log lines ---\n";
    {
        const std::size_t from = r1.log.size() > 5 ? r1.log.size() - 5 : 0;
        for (std::size_t i = from; i < r1.log.size(); ++i) std::cout << r1.log[i] << "\n";
    }
    std::cout << "shots=" << r1.shot_count << " deaths=" << r1.death_count
              << " hit90_rhino=" << (r1.hit90_rhino ? "yes" : "no")
              << " hit22_infantry=" << (r1.hit22_infantry ? "yes" : "no") << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (logs_identical ? "yes" : "no") << " logs=" << r1.log.size()
              << " lines\n";

    return bad ? 1 : 0;
}

// Bit-deterministic proof of the whole combat pipeline: spawn 5 GIs (house 0)
// vs 5 Conscripts (house 1), order the GIs to attack the nearest hostile, and
// run the sim TWICE from identical starting state (a fresh World each time,
// built from the same Assets/UnitAssets) -- once rendering every frame to a
// hidden window (plus start/end screenshots), once fully headless -- then
// byte-compare the two per-shot logs. Combat itself has no RNG (see
// docs/re/combat.md section 5: confirmed no ROF jitter/randomness anywhere in
// vanilla combat), so identical spawn geometry + identical orders must
// produce identical logs; `seed` is accepted only for interface stability
// (some future mode might need to seed something, e.g. randomized spawn
// placement -- this vertical slice's combat has nothing to seed) and is
// otherwise ignored.
int run_demo_combat(int seed) {
    (void)seed;  // see function comment: combat is deterministic, nothing to seed here.

    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_combat_start.png";
    const std::string end_png = scratch + "demo_combat_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);

    // Run 1: rendered every frame (hidden window; exercises the full render
    // path) + start/end screenshots.
    DemoCombatResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-combat)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_combat(a, ua, /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless (no window/renderer/texture cache at all) -- identical
    // spawn/orders from a FRESH World; logs must still be byte-identical
    // (rendering must not perturb sim state; if it does, that's a bug to
    // surface here, not paper over).
    DemoCombatResult r2 =
        simulate_combat(a, ua, /*render=*/false, nullptr, nullptr, nullptr, W, H, "", "");

    const bool logs_identical = (r1.log == r2.log);
    const bool nobody_died = (r1.death_count == 0 || r2.death_count == 0);
    const bool bad = !logs_identical || nobody_died || r1.overlap || r2.overlap ||
                     r1.dangling_target || r2.dangling_target;

    std::cerr << "[demo-combat] run1 lines=" << r1.log.size() << " run2 lines=" << r2.log.size()
              << " identical=" << (logs_identical ? "yes" : "NO") << " deaths(r1/r2)="
              << r1.death_count << "/" << r2.death_count << " overlap(r1/r2)="
              << (r1.overlap ? "Y" : "n") << "/" << (r2.overlap ? "Y" : "n") << " dangling(r1/r2)="
              << (r1.dangling_target ? "Y" : "n") << "/" << (r2.dangling_target ? "Y" : "n")
              << "\n";

    std::cout << "survivors: gi=" << r1.gi_survivors << " cons=" << r1.cons_survivors
              << " sim_time=" << r1.sim_time << "s\n";
    std::cout << "--- first 5 log lines ---\n";
    {
        const std::size_t first_n = r1.log.size() < 5 ? r1.log.size() : 5;
        for (std::size_t i = 0; i < first_n; ++i) std::cout << r1.log[i] << "\n";
    }
    std::cout << "--- last 5 log lines ---\n";
    {
        const std::size_t from = r1.log.size() > 5 ? r1.log.size() - 5 : 0;
        for (std::size_t i = from; i < r1.log.size(); ++i) std::cout << r1.log[i] << "\n";
    }
    std::cout << "shots=" << r1.shot_count << " deaths=" << r1.death_count << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (logs_identical ? "yes" : "no") << " logs=" << r1.log.size()
              << " lines\n";

    return bad ? 1 : 0;
}

// --demo-audio: headless-safe proof that the audio pipeline actually
// produces sound (no window is created -- CI/dev-box safe either way).
// Loads rulesmd.ini -> Rules -> init_audio exactly like run_interactive does,
// then:
//  1) A DEVICELESS proof: play "GIAttack" and render straight into a plain
//     buffer (bypassing AudioOutput/SDL entirely) -- FAILS (exit 1) if every
//     sample comes back zero, which would mean the whole load -> decode ->
//     register -> play chain silently produced nothing.
//  2) IF a real device opened (audio.enabled), an audible scripted sequence
//     -- GIAttack -> GISelect -> GIDie -> EVA "Unit lost" -- paced over ~4s
//     of wall time so a human at the keyboard can actually hear it play.
//     Device absence is NOT a failure (CI/headless boxes have none).
// Always prints the summary line and exits 0 once past check 1.
int mode_demo_audio() {
    const std::string dir = env_install_dir();
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);

    AudioSystem audio;
    init_audio(audio, dir, rules);

    // 1) Deviceless render proof.
    audio.mixer->play("GIAttack");
    std::vector<int16_t> buf(22050, 0);
    audio.mixer->render(buf.data(), buf.size());
    const bool all_zero = std::all_of(buf.begin(), buf.end(), [](int16_t s) { return s == 0; });
    if (all_zero) {
        std::cerr << "[demo-audio] FAILED: rendered buffer is all-zero (GIAttack produced no "
                     "audible samples)\n";
        return 1;
    }

    // 2) Audible scripted sequence, only if a real device opened.
    if (audio.enabled) {
        // The deviceless proof above already consumed the first GIAttack voice
        // via render(), so re-trigger it here to make it audible over SDL_QueueAudio.
        audio.mixer->play("GIAttack");
        const Uint32 t0 = SDL_GetTicks();
        bool did_select = false, did_die = false, did_eva = false;
        while (SDL_GetTicks() - t0 < 4000) {
            const Uint32 t = SDL_GetTicks() - t0;
            if (!did_select && t >= 1000) { audio.mixer->play("GISelect"); did_select = true; }
            if (!did_die && t >= 2000) { audio.mixer->play("GIDie"); did_die = true; }
            if (!did_eva && t >= 3000 && audio.eva_unitlost >= 0) {
                audio.mixer->play_eva(audio.eva_unitlost);
                did_eva = true;
            }
            audio.out.pump(*audio.mixer);
            SDL_Delay(16);
        }
    }

    std::cout << "[demo-audio] clips=" << audio.bank.count()
              << " device=" << (audio.enabled ? "yes" : "no") << " mix=nonzero\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
#ifdef __ANDROID__
        // Android launches straight into the interactive engine: there is no
        // CLI, so the desktop-only verification modes (--screenshot/--demo-*)
        // are unreachable here. Touch drives select/move (see platform.cpp
        // SDL_FINGER* handling + input.cpp).
        (void)argc;
        (void)argv;
        return run_interactive(/*no_audio=*/false);
#else
        std::string screenshot;
        bool demo_move = false;
        int demo_x = 0, demo_y = 0;
        bool demo_retarget = false;
        int retarget_x1 = 0, retarget_y1 = 0, retarget_x2 = 0, retarget_y2 = 0;
        bool demo_group = false;
        int group_n = 5, group_x = 0, group_y = 0;
        bool demo_combat = false;
        int combat_seed = 0;
        bool demo_tank = false;
        bool demo_audio = false;
        bool no_audio = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--screenshot" && i + 1 < argc) {
                screenshot = argv[++i];
            } else if (arg == "--demo-move" && i + 2 < argc) {
                demo_move = true;
                demo_x = std::atoi(argv[++i]);
                demo_y = std::atoi(argv[++i]);
            } else if (arg == "--demo-retarget" && i + 4 < argc) {
                demo_retarget = true;
                retarget_x1 = std::atoi(argv[++i]);
                retarget_y1 = std::atoi(argv[++i]);
                retarget_x2 = std::atoi(argv[++i]);
                retarget_y2 = std::atoi(argv[++i]);
            } else if (arg == "--demo-group" && i + 3 < argc) {
                demo_group = true;
                group_n = std::atoi(argv[++i]);
                group_x = std::atoi(argv[++i]);
                group_y = std::atoi(argv[++i]);
            } else if (arg == "--demo-combat") {
                demo_combat = true;
                // Optional int seed: only consume the next arg if it actually
                // parses as one (so a following unrelated flag isn't
                // swallowed). Accepted and ignored -- see run_demo_combat's
                // comment (combat has no RNG to seed; this is here purely for
                // interface stability).
                if (i + 1 < argc) {
                    char* end = nullptr;
                    const long v = std::strtol(argv[i + 1], &end, 10);
                    if (end != argv[i + 1] && *end == '\0') {
                        combat_seed = static_cast<int>(v);
                        ++i;
                    }
                }
            } else if (arg == "--demo-tank") {
                demo_tank = true;
            } else if (arg == "--demo-audio") {
                demo_audio = true;
            } else if (arg == "--no-audio") {
                no_audio = true;
            }
        }
        // --demo-tank dispatched BEFORE --demo-combat (brief Step 6 ordering).
        if (demo_tank) return run_demo_tank();
        if (demo_combat) return run_demo_combat(combat_seed);
        if (demo_audio) return mode_demo_audio();
        if (demo_group) return run_demo_group(group_n, group_x, group_y);
        if (demo_retarget)
            return run_demo_retarget(retarget_x1, retarget_y1, retarget_x2, retarget_y2);
        if (demo_move) return run_demo_move(demo_x, demo_y);
        if (!screenshot.empty()) return run_screenshot(screenshot);
        return run_interactive(no_audio);
#endif
    } catch (const std::exception& ex) {
        // SDL_Log reaches Android logcat (tag "SDL"); std::cerr does not. Emit
        // both so the failure reason is visible on-device and on desktop
        // instead of the app just vanishing.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fatal: %s", ex.what());
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}
