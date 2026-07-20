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
#include <array>
#include <cctype>
#include <climits>
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
#include "building.hpp"
#include "cargo.hpp"        // cargo_total_size (pip fill) [CB-1 Transports/Cargo]
#include "capture.hpp"      // kNeutralHouse (Engineer/Spy Capture phase)
#include "combat.hpp"       // armor_index
#include "facing_cache.hpp"
#include "factory.hpp"      // primary_factory (unit exit handoff)
#include "flight.hpp"       // altitude_to_screen_dy (CB-2 raised-sprite draw)
#include "harvest.hpp"     // OreValues/HarvestEvent/dock_cell + step_harvest (Economy phase)
#include "input.hpp"
#include "iso_project.hpp"
#include "iso_renderer.hpp"
#include "platform.hpp"
#include "production.hpp"
#include "rectfont.hpp"    // glyph_rows (5x7 rect-font for menu labels) [Task 5]
#include "sidebar.hpp"
#include "texture_cache.hpp"
#include "skirmish.hpp"
#include "voxel_raster.hpp"
#include "world.hpp"

#include "ra2assets/art.hpp"
#include "ra2assets/audio_bag.hpp"
#include "ra2assets/fileio.hpp"
#include "ra2assets/hva.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/land_type.hpp"  // CB-3 Naval: ra2assets::LandType (resolve_land_types)
#include "ra2assets/map.hpp"
#include "ra2assets/mix.hpp"
#include "ra2assets/pal.hpp"
#include "ra2assets/rules.hpp"
#include "ra2assets/shp.hpp"
#include "ra2assets/sounds.hpp"
#include "ra2assets/theater_name.hpp"
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

// Ore/gem overlay art [RE: economy.md §5]: one SHP per [OverlayTypes] name,
// loaded from the temperate theater chain (.tem, then a generic .shp
// fallback) via the existing theater name resolver. Indexed [kind][slot]:
// kind 0 = Ore (tibNN), 1 = Gems (gemNN); slot = the 0-based art index within
// the set (OreField::variant_at). Each SHP's first half is the density
// frames (0..11), second half shadows (verified: tib01.tem = SHP(TS) 60x60,
// 24 frames, frame 12+ empty). Individually guarded: a name missing in every
// candidate leaves present=false and that cell is simply not drawn -- never
// a crash.
//
// PALETTE (task-7 fix round, root-caused from raw-byte dumps): overlays
// palettize with temperat.pal -- the THEATER palette -- NOT isotem.pal (the
// iso TILE palette). The same decoded indices render as multicolored
// confetti under isotem.pal but as the original's golden-amber ore clumps /
// blue-green gem crystals under temperat.pal (scratchpad
// tib01_f11_isotem.png vs tib01_f11_temperat.png).
struct OreArt {
    ra2assets::ShpFile shp[2][20];
    bool present[2][20] = {};
    ra2assets::Palette pal;   // temperat.pal (see PALETTE note above)
    bool has_pal = false;     // false -> draw_ore falls back to the terrain pal
    bool any = false;
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

// CB-3 Naval [RE-GATE APPLIED: naval.md §2]: resolve every materialized map
// cell's LandType into a grid_w*grid_h array (indexed y*grid_w + x, the SAME
// space build_water_passability/passable_ use). For each MapCell, classify its
// global tile index against the theater [General] tile-set indices via
// Theater::land_type_of_cell (tile -> owning [TileSetNNNN] set -> LandType). The
// .tmp TerrainType byte is NOT used (it was FALSIFIED for LandType). Cells with
// no map entry stay Clear (land, never water). land_type_of_cell is const; `th`
// is by ref only because the Assets member is non-const.
std::vector<ra2assets::LandType> resolve_land_types(const ra2assets::MapFile& map,
                                                    ra2engine::Theater& th,
                                                    int grid_w, int grid_h) {
    std::vector<ra2assets::LandType> lt(
        static_cast<std::size_t>(grid_w) * grid_h, ra2assets::LandType::Clear);
    for (const ra2assets::MapCell& c : map.cells()) {
        if (c.x < 0 || c.y < 0 || c.x >= grid_w || c.y >= grid_h) continue;
        lt[static_cast<std::size_t>(c.y) * grid_w + c.x] =
            th.land_type_of_cell(c.tile);   // tile -> owning tile-set -> LandType
    }
    return lt;
}

// Loads the ore/gem overlay SHPs from the temperate theater chain [RE:
// economy.md §5]. Ore uses the TIB set (tib01..tib20), gems use gem01..gem12;
// each name is resolved through theater_candidates (.tem preferred, .shp
// fallback) over the isotemp/temperat/generic nested mixes -- exactly the
// walk load_building_assets uses. Every name is individually guarded (a miss
// leaves present=false; the cell just doesn't draw). Logs a small probe of
// frame counts so a real-install run confirms the density-frame layout.
OreArt load_ore_art(const ra2assets::MixFile& ra2) {
    OreArt oa;
    std::map<std::string, std::unique_ptr<MixFile>> mix_cache;
    auto get_mix = [&](const std::string& name) -> MixFile* {
        auto it = mix_cache.find(name);
        if (it != mix_cache.end()) return it->second.get();
        std::unique_ptr<MixFile> m;
        try {
            m = std::make_unique<MixFile>(ra2.open_nested(name));
        } catch (const std::exception&) { m.reset(); }
        MixFile* raw = m.get();
        mix_cache.emplace(name, std::move(m));
        return raw;
    };
    auto try_load = [&](const std::string& name, ra2assets::ShpFile& out) -> bool {
        for (const char* ext : {".tem", ".shp"})
            for (const auto& c : ra2assets::theater_candidates(name, 'T', "isotemp.mix",
                                                               "temperat.mix", ext)) {
                MixFile* m = get_mix(c.mix);
                if (!m) continue;
                if (const MixEntry* e = m->find(c.file)) {
                    try {
                        out = ShpFile::load(m->read(*e));
                        return true;
                    } catch (const std::exception&) { return false; }
                }
            }
        return false;
    };
    int ok = 0;
    for (int i = 0; i < 20; ++i) {
        char nm[16];
        std::snprintf(nm, sizeof nm, "tib%02d", i + 1);
        if (try_load(nm, oa.shp[0][i])) { oa.present[0][i] = true; ++ok; }
    }
    for (int i = 0; i < 12; ++i) {
        char nm[16];
        std::snprintf(nm, sizeof nm, "gem%02d", i + 1);
        if (try_load(nm, oa.shp[1][i])) { oa.present[1][i] = true; ++ok; }
    }
    // The overlay palette: temperat.pal from cache.mix (see OreArt's PALETTE
    // note). Guarded: if absent, draw_ore falls back to the terrain palette
    // (wrong colors, but still renders rather than crashing).
    try {
        MixFile cache = ra2.open_nested("cache.mix");
        if (const MixEntry* pe = cache.find(std::string("temperat.pal"))) {
            auto raw = cache.read(*pe);
            oa.pal = Palette::load(raw.data(), raw.size());
            oa.has_pal = true;
        }
    } catch (const std::exception&) { oa.has_pal = false; }
    oa.any = ok > 0;
    std::cerr << "[ore] art files loaded=" << ok << " pal=" << (oa.has_pal ? "temperat" : "FALLBACK")
              << " tib01=" << (oa.present[0][0] ? std::to_string(oa.shp[0][0].count()) : "absent")
              << "f tib02=" << (oa.present[0][1] ? std::to_string(oa.shp[0][1].count()) : "absent")
              << "f gem01=" << (oa.present[1][0] ? std::to_string(oa.shp[1][0].count()) : "absent")
              << "f\n";
    return oa;
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

// All SHP + palette assets for one building type, resolved through the
// NewTheater candidate list [RE: building-render.md §1] over the already-open
// ra2.mix nested mixes. ok=false drops the type from the roster (guarded
// degrade, never crash).
struct BuildingArt {
    ra2assets::ShpFile shp;      // main image (frame 0 healthy, 1 damaged; 2nd half shadows)
    ra2assets::ShpFile buildup;  // MK anim ("" buildup -> unused)
    ra2assets::ShpFile cameo;    // 60x48 icon (may be absent for ConYards)
    double buildup_total = 0.0;  // seconds the buildup anim lasts [RE: building-render.md §5]
    bool has_buildup = false, has_cameo = false, ok = false;
};

// Everything the buildings + sidebar draw/interaction needs, loaded once.
struct BuildingAssets {
    std::vector<ra2engine::BuildingTypeDef> roster;  // index = Building::type
    std::vector<BuildingArt> art;                    // parallel to roster
    // [RE-GATE APPLIED: production.md §1.1] per-roster tab/queue classifier:
    // 1 if BuildCat==Combat (Defense queue, tab 1), 0 otherwise (Structure
    // queue, tab 0). Index-aligned with `roster`.
    std::vector<char> defense;
    ra2assets::Palette unit_pal;                     // unittem.pal (cache.mix)
    ra2assets::Palette cameo_pal;                    // cameo.pal (cache.mix)
    // sidebar chrome (player side = Allied sidec01) [RE: construction.md
    // §11a]: four SEPARATE tab SHPs + the 55-frame gclock2.shp build clock,
    // plus the full recovered composition set — radar.shp (168x110x33; frame
    // 0 = pre-radar eagle logo), side3 bottom cap, addon tail, scroll
    // buttons, repair/sell and options/diplomacy button chrome.
    ra2assets::ShpFile sb_top, sb_side1, sb_side2, sb_tabs[4], sb_powerp, sb_credits, sb_gclock;
    // CB-4 Superweapons [RE: superweapon.md; spec §6]: the SW cameo (NukeIcon),
    // loaded through the EXISTING cameo.mix chain. Art-guarded -- sw_has_cameo
    // is false if NukeIcon is absent (the SW slot then draws a placeholder shade).
    ra2assets::ShpFile sb_nukeicon;
    bool sw_has_cameo = false;
    ra2assets::ShpFile sb_radar, sb_side3, sb_addon, sb_rup, sb_rdn, sb_repair, sb_sell,
                       sb_optbtn, sb_diplobtn;
    bool has_radar = false, has_side3 = false, has_addon = false, has_scroll = false,
         has_repair_sell = false, has_optbtns = false;
    ra2assets::Palette sb_pal;                       // sidebar.pal if present, else cameo.pal
    bool sidebar_ok = false;
    // Shared building death explosion (TWLT070) [RE: building-render.md §4] --
    // render-only; null if unavailable (building just vanishes on death).
    std::unique_ptr<ra2assets::ShpFile> explosion;
    ra2assets::Palette explosion_pal;
    // Prerequisite groups resolved once [RE: construction.md §10].
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    bool ok = false;                                 // any usable building
    // rulesmd [General] ConditionYellow=50 (percent HP threshold for the
    // damaged building frame) [RE: task-7 review "ConditionYellow"], read via
    // rules.general_double in load_building_assets; 0.5 is only the fallback.
    double condition_yellow = 0.5;
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
    // Chrono Miner teleport sounds [RE: economy.md §3]: soundmd section names
    // from rulesmd [CMIN] ChronoOut/InSound ("" -> silent no-op on play).
    std::string chrono_out, chrono_in;
    // CB-1 transport sounds [RE: transport-cargo.md; spec §6]: soundmd
    // section names from rulesmd [HTK] EnterTransportSound/
    // LeaveTransportSound, falling back to the [AudioVisual] defaults
    // ("" -> silent no-op on play, the chrono pattern).
    std::string enter_transport, leave_transport;
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

    // Building placement sound [RE: building-render.md §5]: rules [AudioVisual]
    // BuildingSlam=PlaceBuilding -> soundmd [PlaceBuilding] Sounds=uplace.
    // Registered globally (a missing section resolves to a silent no-op).
    if (!pending.count("PlaceBuilding"))
        pending["PlaceBuilding"] = resolve_sound_event(audio, sounds, idx, bag, "PlaceBuilding");

    // Chrono Miner teleport sounds [RE: economy.md §3]: rulesmd [CMIN]
    // ChronoOut/InSound name soundmd sections; resolve + register so the
    // interactive loop can play them on ChronoOut/ChronoIn harvest events.
    {
        Ini cmin_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
        audio.chrono_out = cmin_ini.get("CMIN", "ChronoOutSound", "");
        audio.chrono_in = cmin_ini.get("CMIN", "ChronoInSound", "");
        for (const std::string& k : {audio.chrono_out, audio.chrono_in})
            if (!k.empty() && !pending.count(k))
                pending[k] = resolve_sound_event(audio, sounds, idx, bag, k);
        audio.enter_transport = cmin_ini.get("HTK", "EnterTransportSound", "");
        if (audio.enter_transport.empty())
            audio.enter_transport = cmin_ini.get("AudioVisual", "EnterTransportSound", "");
        audio.leave_transport = cmin_ini.get("HTK", "LeaveTransportSound", "");
        if (audio.leave_transport.empty())
            audio.leave_transport = cmin_ini.get("AudioVisual", "LeaveTransportSound", "");
        for (const std::string& k : {audio.enter_transport, audio.leave_transport})
            if (!k.empty() && !pending.count(k))
                pending[k] = resolve_sound_event(audio, sounds, idx, bag, k);
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

// Movement pace tuning: iso cells/sec = rules Speed * this. rulesmd Speed for
// E1 is 4, so 4 * 0.75 = 3 cells/sec -- fast enough to visibly stride between
// tiles, slow enough to read as walking (not teleporting) and to cross a
// ~25-cell demo path in well under the 15s cap. Falls back to World's own
// default if Speed is missing (0). Defined HERE (ahead of resolve_combat_unit)
// because CB-2's JumpjetSpeed fallback converts base Speed with the SAME scale.
constexpr double kCellsPerSecPerSpeed = 0.75;

// Case-insensitive lowercasing (CB-2 Locomotor CLSID compare). File-local.
inline std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

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
    r.combat.sight = rules.sight(unit_id);  // [RE: shroud.md §2] reveal radius
    r.combat.armor = rules.armor(unit_id);
    const std::string primary = rules.primary(unit_id);
    r.combat.damage = rules.weapon_damage(primary);
    r.combat.rof = rules.weapon_rof(primary);
    r.combat.range = rules.weapon_range(primary);
    const std::string warhead = rules.weapon_warhead(primary);
    r.combat.verses = rules.verses(warhead);

    // CB-2 air-to-ground splash [RE-GATE APPLIED: aircraft.md §4 -> combat.md
    // §7]: the warhead's splash radius + edge scale (BlimpBomb -> BlimpHE
    // CellSpread=2 PercentAtMax=.5). Fed to combat.cpp's apply_spread_damage,
    // GATED on cell_spread >= 1 -- so every existing sub-1-cell warhead (tank
    // AP CellSpread=.3, infantry) keeps its EXACT single-target path; only
    // CellSpread>=1 warheads (BlimpHE=2) splash. Default 0 = single-target.
    r.combat.cell_spread = rules.section_double(warhead, "CellSpread", 0.0);
    r.combat.percent_at_max = rules.section_double(warhead, "PercentAtMax", 1.0);

    // CB-1 cargo stats [RE-GATE APPLIED: transport-cargo.md §1 -- absent-key
    // defaults CONFIRMED: Passengers/SizeLimit -> 0, Size -> 1. In the
    // binary Size/SizeLimit are doubles (type +0x380/+0x388); int is exact
    // for the roster values (1/2/3). Capacity gates on the COUNT
    // (NumPassengers < Passengers); Size/SizeLimit form the per-passenger
    // cap and Size otherwise feeds only the pip display]. Real rulesmd
    // ground truth: [HTK] Passengers=5 SizeLimit=2 Size=3, [FV] Passengers=1
    // SizeLimit=1 Size=3, infantry Size=1.
    r.combat.passengers = (int)rules.section_double(unit_id, "Passengers", 0.0);
    r.combat.size_limit = (int)rules.section_double(unit_id, "SizeLimit", 0.0);
    r.combat.size = (int)rules.section_double(unit_id, "Size", 1.0);

    // CB-2 flight stats [RE-GATE APPLIED: aircraft.md §1/§2/§5]. is_aircraft iff
    // the type carries the Jumpjet locomotor CLSID (CONFIRMED from [ZEP]
    // Locomotor=). D1: flight_turn = ROT<<1 (ROT x2) -- the FacingClass rate
    // Link_To_Object arms from TechnoType+0x71C; JumpJetTurnRate (+0xD70) is
    // VESTIGIAL and NOT read. D2: flight_speed = JumpjetSpeed (fallback base
    // Speed) via the engine's EXISTING vehicle-speed conversion
    // (kCellsPerSecPerSpeed -- the SAME scale ground vehicles use; the vanilla
    // 12 leptons/frame is the documented divergence, voxel-render.md §5).
    // climb/crash are per-frame Jumpjet floats x15 -> leptons/sec (the
    // rot_u16_per_sec precedent). All default INERT for a ground unit.
    static const std::string kJumpjetClsid = "{92612C46-F71F-11d1-AC9F-006008055BB5}";
    const std::string loco = rules.locomotor(unit_id);
    r.combat.is_aircraft =
        (!loco.empty() && to_lower(loco) == to_lower(kJumpjetClsid));
    if (r.combat.is_aircraft) {
        // D2: JumpjetSpeed, falling back to base Speed (r.speed) when absent.
        const double jjspeed = rules.section_double(unit_id, "JumpjetSpeed",
                                                    static_cast<double>(r.speed));
        const double jjclimb = rules.section_double(unit_id, "JumpjetClimb", 6.0);
        const double jjcrash = rules.section_double(unit_id, "JumpjetCrash", 12.0);
        // D1: ROT is the turn source -- reuse the SAME resolver the voxel
        // vehicle path uses (rules.rot), NOT JumpJetTurnRate.
        const double rot = static_cast<double>(rules.rot(unit_id));
        r.combat.cruise_altitude = rules.section_double(unit_id, "JumpjetHeight", 500.0);
        // D2: same vehicle-speed conversion ground vehicles use (Speed 6 ->
        // 4.5 cps); ZEP JumpjetSpeed=5 -> 3.75 cells/sec in engine scale.
        r.combat.flight_speed = jjspeed * kCellsPerSecPerSpeed;   // cells/sec
        r.combat.climb_rate   = jjclimb * 15.0;                   // leptons/sec
        r.combat.descend_rate = jjcrash * 15.0;
        // D1: FacingClass rate = (ROT<<1)*256 u16/frame; x15 -> u16/sec.
        r.combat.flight_turn  = (rot * 2.0) * 256.0 * 15.0;      // u16/sec
        r.combat.balloon_hover = rules.balloon_hover(unit_id);
        r.combat.omni_fire = rules.weapon_omnifire(primary);
    }

    // CB-3 naval stats [RE-GATE APPLIED: naval.md §1/§5]. is_naval iff the type
    // carries the naval locomotor CLSID (CONFIRMED from [HYD]/[DEST]/[SUB]/[DRED]
    // Locomotor={2BEA74E1-...}). The [HYD] combat stats are resolved by the SAME
    // ground-vehicle conversions with NO naval special-casing [RE-GATE APPLIED:
    // naval.md §4/§5]: Primary=FlakTrackGun -> damage=25, rof=25 frames,
    // range=5 cells (1280 leptons), verses from Warhead=FlakTWH
    // (150,125,100,60,10,10,30,20,10,100,100); Speed=8/ROT=6 via voxel-render.md
    // §5; Strength=400 -> hp. FlakTWH CellSpread=1.0 (>= 1.0) flows through CB-2's
    // existing apply_spread_damage one-ring splash (PercentAtMax=1.0 -> no
    // falloff) -- NO new code; the instant single-target-cell hit drops the
    // vanilla FlakScatter/Inaccurate RNG (documented deviation). Naval adds only
    // the domain flags. All default INERT for a ground unit (movement_domain=Land).
    static const std::string kNavalClsid = "{2BEA74E1-7CCA-11d3-BE14-00104B62A16C}";
    const std::string nloco = rules.locomotor(unit_id);
    r.combat.is_naval =
        (!nloco.empty() && to_lower(nloco) == to_lower(kNavalClsid));
    if (r.combat.is_naval)
        r.combat.movement_domain = ra2engine::MovementDomain::Water;

    std::cerr << "[unit] id=" << unit_id << " image=" << r.combat.image << " speed=" << r.speed
              << " strength=" << r.combat.strength << " armor=" << r.combat.armor
              << " primary=" << primary << " damage=" << r.combat.damage
              << " rof=" << r.combat.rof << " range=" << r.combat.range
              << " warhead=" << warhead << "\n";
    return r;
}

// CB-4 Superweapons [RE-GATE APPLIED: superweapon.md §1/§2]: resolve the Nuclear
// Missile grant params from the real rulesmd. RechargeTime is MINUTES -- CONFIRMED
// (single FMUL [0x7F3100]=900.0 + _ftol: frames = trunc(RechargeTime*900); this
// engine keeps SECONDS, so recharge_secs = RechargeTime*60, the same value as
// *900 frames / 15 fps) [RE-GATE APPLIED: superweapon.md §1]. The DAMAGING leg is
// [NukePayload]/[NUKE] (the down-strike), NOT the SW's [NukeCarrier]/NukeMaker
// up-launch (which carries no Damage/Verses -- zero area damage) -- CONFIRMED
// [RE-GATE APPLIED: superweapon.md §2]. Verses is the [NUKE] 11-slot armor array
// (int percents). All values cite docs/re/superweapon.md.
struct SuperweaponParams {
    double recharge_secs = 0.0;
    int damage = 0;
    double cell_spread = 0.0;
    double percent_at_max = 1.0;
    std::array<int, 11> verses{};
    bool ok = false;
};

SuperweaponParams resolve_superweapon(const ra2assets::Rules& rules) {
    SuperweaponParams p;
    const double recharge_min = rules.section_double("NukeSpecial", "RechargeTime", 10.0);
    p.recharge_secs = recharge_min * 60.0;                        // minutes -> seconds
    p.damage = (int)rules.section_double("NukePayload", "Damage", 600.0);
    p.cell_spread = rules.section_double("NUKE", "CellSpread", 10.0);
    p.percent_at_max = rules.section_double("NUKE", "PercentAtMax", 0.02);
    const std::vector<int> v = rules.verses("NUKE");             // [NUKE] Verses (combat.md §1)
    for (std::size_t i = 0; i < p.verses.size(); ++i)
        p.verses[i] = i < v.size() ? v[i] : 100;                 // absent slot -> 100% (no mod)
    p.ok = true;
    std::cerr << "[sw] resolve nuke recharge_secs=" << p.recharge_secs
              << " damage=" << p.damage << " cell_spread=" << p.cell_spread
              << " percent_at_max=" << p.percent_at_max << "\n";
    return p;
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

// The two harvester types the economy needs [RE: economy.md §1-§3], plus each
// type's Storage= (bales) and chrono flag. HARV/CMIN are voxel units loaded
// exactly like the Rhino (load_vehicle_assets: HARV has a turret, CMIN none).
// A type whose voxel assets fail to load keeps ok=false and is simply never
// spawned (economy silently degrades; a demo prints the [vehicle] failure).
struct MinerAssets {
    VehicleAssets harv;   // Soviet War Miner (drives home)
    VehicleAssets cmin;   // Allied Chrono Miner (warps home)
    int harv_storage = 40;
    int cmin_storage = 20;
};

MinerAssets load_miner_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                              const ra2assets::Palette& unit_pal, const Rules& rules) {
    MinerAssets m;
    m.harv = load_vehicle_assets(dir, ra2, unit_pal, "HARV");
    m.cmin = load_vehicle_assets(dir, ra2, unit_pal, "CMIN");
    const int hs = rules.storage("HARV"), cs = rules.storage("CMIN");
    m.harv_storage = hs > 0 ? hs : 40;
    m.cmin_storage = cs > 0 ? cs : 20;
    std::cerr << "[miner] harv ok=" << (m.harv.ok ? "y" : "n") << " storage=" << m.harv_storage
              << " cmin ok=" << (m.cmin.ok ? "y" : "n") << " storage=" << m.cmin_storage << "\n";
    return m;
}

// Resolves a FreeUnit id (GAREFN->"CMIN", NAREFN->"HARV") to the loaded
// VehicleAssets + its chrono flag + storage; nullptr if the id isn't a known
// miner or its assets failed to load.
const VehicleAssets* miner_for_id(const MinerAssets& m, const std::string& id, bool& chrono,
                                  int& storage) {
    if (id == "HARV") { chrono = false; storage = m.harv_storage; return m.harv.ok ? &m.harv : nullptr; }
    if (id == "CMIN") { chrono = true;  storage = m.cmin_storage; return m.cmin.ok ? &m.cmin : nullptr; }
    return nullptr;
}

// CB-1: the demo/interactive multi-capacity transport [spec: Transport
// choice; RE: transport-cargo.md §1]. HTK (Soviet Flak Track: Passengers=5
// SizeLimit=2 Size=3, ground Drive, art PRESENT on this install) with FV
// (IFV, Passengers=1, already producible) as the art-guard fallback;
// multi-passenger logic is fully implemented regardless (capacity>1 is
// covered host-only by the pure tests). load_vehicle_assets already
// resolves the cargo stats via resolve_combat_unit, so a candidate is
// usable iff its art loaded AND it is actually a transport.
struct TransportAssets {
    VehicleAssets va;      // voxel render assets + combat (cargo stats included)
    std::string id;        // "HTK" or "FV"
    bool ok = false;       // art present AND Passengers > 0
};

TransportAssets load_transport_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                                      const ra2assets::Palette& unit_pal) {
    TransportAssets ta;
    for (const char* id : {"HTK", "FV"}) {
        VehicleAssets va = load_vehicle_assets(dir, ra2, unit_pal, id);
        if (!va.ok || va.combat.passengers <= 0) continue;  // art-guard-drop ->
        ta.va = std::move(va);                              // try the fallback
        ta.id = id;
        ta.ok = true;
        break;
    }
    if (ta.ok)
        std::cerr << "[transport] id=" << ta.id
                  << " passengers=" << ta.va.combat.passengers
                  << " size_limit=" << ta.va.combat.size_limit
                  << " size=" << ta.va.combat.size << "\n";
    else
        std::cerr << "[transport] unavailable (HTK and FV art both missing); "
                     "cargo gestures degrade to no-ops\n";
    return ta;
}

// CB-2 Aircraft render assets [RE: aircraft.md]. load_vehicle_assets already
// resolves the flight stats via resolve_combat_unit, so a candidate is usable
// iff its voxel art loaded AND it is actually a flight-locomotor unit.
struct AircraftAssets {
    VehicleAssets va;      // voxel render assets + combat (flight stats included)
    std::string id;        // "ZEP", "ORCA", or "BEAG"
    bool ok = false;       // art present AND is_aircraft
};

AircraftAssets load_aircraft_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                                    const ra2assets::Palette& unit_pal) {
    AircraftAssets aa;
    for (const char* id : {"ZEP", "ORCA", "BEAG"}) {
        VehicleAssets va = load_vehicle_assets(dir, ra2, unit_pal, id);
        if (!va.ok || !va.combat.is_aircraft) continue;   // art-guard-drop -> fallback
        aa.va = std::move(va);
        aa.id = id;
        aa.ok = true;
        break;
    }
    if (aa.ok)
        std::cerr << "[aircraft] id=" << aa.id
                  << " cruise_alt=" << aa.va.combat.cruise_altitude
                  << " speed=" << aa.va.combat.flight_speed
                  << " climb=" << aa.va.combat.climb_rate
                  << " omni=" << (aa.va.combat.omni_fire ? "yes" : "no") << "\n";
    else
        std::cerr << "[aircraft] unavailable (ZEP/ORCA/BEAG art missing)\n";
    return aa;
}

// CB-3 Naval render assets [RE: naval.md §5]. A ship is a voxel vehicle at
// altitude 0 -- load_vehicle_assets already resolves the naval stats via
// resolve_combat_unit, so a candidate is usable iff its voxel art loaded AND it
// is actually a naval-locomotor unit. No new render work: the ship draws
// through the EXISTING draw_vehicle_unit/draw_voxel_sprite + facing_cache path.
struct NavalAssets {
    VehicleAssets va;      // voxel render assets + combat (naval flags included)
    std::string id;        // "HYD", "DRED", or "DEST"
    bool ok = false;       // art present AND is_naval
};

NavalAssets load_naval_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                              const ra2assets::Palette& unit_pal) {
    NavalAssets na;
    for (const char* id : {"HYD", "DRED", "DEST"}) {
        VehicleAssets va = load_vehicle_assets(dir, ra2, unit_pal, id);
        if (!va.ok || !va.combat.is_naval) continue;   // art-guard-drop -> fallback
        na.va = std::move(va);
        na.id = id;
        na.ok = true;
        break;
    }
    if (na.ok)
        std::cerr << "[naval] id=" << na.id
                  << " strength=" << na.va.combat.strength
                  << " speed=" << na.va.combat.speed_cps
                  << " range=" << na.va.combat.range
                  << " domain=" << (na.va.combat.movement_domain ==
                                    ra2engine::MovementDomain::Water ? "water" : "land")
                  << "\n";
    else
        std::cerr << "[naval] unavailable (HYD/DRED/DEST art missing or not naval)\n";
    return na;
}

// The Allied producible-unit roster + its render/cameo assets [RE-GATE
// APPLIED: production.md §1.6]. `defs` holds each UnitTypeDef (index = the
// unit-queue slot type); `vehicles` holds one VehicleAssets per is_vehicle def
// (appended in def order, so the k-th vehicle def maps to vehicles[k]); the
// cameo SHP + present flag are parallel to `defs`. Infantry render/combat come
// from the existing UnitAssets pipeline + UnitTypeDef::combat, so no per-def
// infantry SHP store is needed here.
struct UnitRoster {
    std::vector<ra2engine::UnitTypeDef> defs;
    std::vector<VehicleAssets> vehicles;
    std::vector<ra2assets::ShpFile> cameos;  // parallel to defs
    std::vector<char> has_cameo;             // parallel to defs
};

// Every entry is opt-guarded: a def whose cost/tech/cameo is unusable, whose vehicle
// voxel assets fail to load, or whose combat can't resolve is DROPPED from
// `defs` (never a crash — mirrors Task 6's defense guard). Cameo SHPs load
// from cameo.mix (language.mix) exactly like building cameos, keyed by unit id.
// Hoisted from load_unit_roster so the AI-2 Soviet table reuses the loop.
struct UnitRosterEnt { const char* id; bool vehicle; const char* prereq; };

// Shared roster-loading loop [RE-GATE APPLIED: production.md §1.1/§1.6]. Loads
// the given (ents, n) into a UnitRoster with EVERY guard identical to the
// former load_unit_roster body (cost/tech/cameo drop, vehicle-asset drop,
// per-entry catch/log) — the GATOW/art-guard lesson. Consumed by the player
// roster (load_unit_roster) and the AI-2 Soviet roster (load_soviet_unit_roster).
UnitRoster load_unit_roster_table(const std::string& dir, const ra2assets::MixFile& ra2,
                                  const UnitAssets& ua, const Rules& rules, const Art& art,
                                  const UnitRosterEnt* ents, std::size_t n, bool full_parse) {
    UnitRoster ur;
    // cameo.mix lives inside language.mix (same chain load_building_assets uses
    // for building cameos); opened once here, guarded so a missing archive just
    // leaves every unit cameo absent (defs still gate on the cameo NAME string).
    std::unique_ptr<MixFile> cameo_mix;
    try {
        MixFile language = MixFile::open(ra2assets::read_file(dir + "language.mix"));
        cameo_mix = std::make_unique<MixFile>(language.open_nested("cameo.mix"));
    } catch (const std::exception&) { cameo_mix.reset(); }

    for (std::size_t i = 0; i < n; ++i) {
        const UnitRosterEnt& e = ents[i];
        try {
            ra2engine::UnitTypeDef d;
            d.id = e.id;
            d.cost = rules.cost(e.id);
            d.tech_level = rules.tech_level(e.id);
            d.is_vehicle = e.vehicle;
            // CB-5 [RE-GATE APPLIED: techtree.md §1/§5]: the PLAYER roster full-parses
            // the real rulesmd [<id>] Prerequisite= (the same list buildings already
            // use, main.cpp:1435) so multi-token + group-token (TECH) unit prereqs
            // resolve through the EXISTING prerequisites_met. The SOVIET AI roster
            // keeps its hardcoded proxy token (APOC->NAWEAP) UNCHANGED to protect
            // ai_team_hash (spec §8; real [APOC] Prerequisite=NAWEAP,NATECH -- the
            // NATECH conjunct the AI base never builds, so dropping it in the proxy
            // never makes APOC buildable where the real rule would not; techtree.md
            // §5 CONFIRMED). Byte-identity anchor CONFIRMED (techtree.md §5):
            // [E1]=GAPILE, [GGI]=GAPILE, [MTNK]=GAWEAP, [FV]=GAWEAP -- ALL single
            // plain building tokens (no group token), so the full-parse resolves them
            // BYTE-IDENTICALLY to the retired hardcoded tokens. Defensive fallback to
            // the proxy token if rulesmd has no Prerequisite= for this id.
            if (full_parse) {
                d.prerequisite = rules.prerequisite(e.id);
                if (d.prerequisite.empty()) d.prerequisite = {e.prereq};
            } else {
                d.prerequisite = {e.prereq};
            }
            // DEVIATION from brief (`art.cameo(e.id)`): the cameo must come from
            // the unit's IMAGE art section (the one it actually renders as), not
            // the rules id. In this artmd.ini E1's section is [GI]
            // (Cameo=GIICON) and, critically, rules [MTNK] Image=GTNK — the
            // Grizzly — while the legacy [MTNK] art section is the *Apocalypse*
            // (Cameo=MTNKICON). Keying on rules.image(id) yields GTNK->GTNKICON
            // (Grizzly), the correct cameo. Fall back to the id-keyed section if
            // the image section has none.
            const std::string image = rules.image(e.id);
            d.cameo = art.cameo(image);
            if (d.cameo.empty()) d.cameo = art.cameo(e.id);
            if (d.cost <= 0 || d.tech_level <= 0 || d.cameo.empty()) continue;  // not producible

            // Cameo SHP (guarded; absence only downgrades the sidebar icon).
            ra2assets::ShpFile cshp;
            bool have_cameo = false;
            if (cameo_mix) {
                std::string cn = d.cameo;
                for (char& ch : cn) ch = (char)std::tolower((unsigned char)ch);
                if (const MixEntry* ce = cameo_mix->find(cn + ".shp")) {
                    try { cshp = ShpFile::load(cameo_mix->read(*ce)); have_cameo = true; }
                    catch (const std::exception&) { have_cameo = false; }
                }
            }

            if (e.vehicle) {
                VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, e.id);
                if (!va.ok) continue;                 // assets missing -> drop
                d.combat = va.combat;
                ur.vehicles.push_back(std::move(va));
            } else {
                const ResolvedUnit ru = resolve_combat_unit(rules, e.id);  // infantry combat + SHP image
                d.combat = ru.combat;
                d.combat.is_vehicle = false;
                d.combat.speed_cps = ru.speed * kCellsPerSecPerSpeed;
            }
            ur.defs.push_back(std::move(d));
            ur.cameos.push_back(std::move(cshp));
            ur.has_cameo.push_back(have_cameo ? 1 : 0);
        } catch (const std::exception& ex) {
            std::cerr << "[unit-roster] " << e.id << " unavailable (" << ex.what() << ")\n";
        }
    }
    std::cerr << "[unit-roster] producible=" << ur.defs.size() << "\n";
    return ur;
}

// Loads the curated Allied producibles + assets [RE-GATE APPLIED:
// production.md §1.1/§1.6 — recovered per-unit prereqs are PLAIN building ids:
// E1/GGI=GAPILE, MTNK/FV=GAWEAP, not the Barracks/Factory group tokens].
UnitRoster load_unit_roster(const std::string& dir, const ra2assets::MixFile& ra2,
                            const UnitAssets& ua, const Rules& rules, const Art& art) {
    static const UnitRosterEnt kUnits[] = {
        {"E1",  false, "GAPILE"}, {"GGI", false, "GAPILE"},
        {"MTNK", true, "GAWEAP"}, {"FV",  true, "GAWEAP"},
        // CB-5 [RE-GATE APPLIED: techtree.md §4]: the Prism Tank (advanced tier-3
        // unit), appended at index 4 so E1/GGI/MTNK/FV (0..3) stay stable ->
        // --demo-prod's mtnk_slot/gi_slot lookups + prod_hash unaffected.
        // full_parse=true (Task 2) resolves its CONFIRMED real Prerequisite=GAWEAP,
        // GATECH (techtree.md §4; TechLevel=8, Cost=1200); the "GAWEAP" token here is
        // only the Task-2 defensive fallback. SREF art is PRESENT on this install
        // (sref.vxl+sref.hva in local.mix, cameo sreficon.shp in cameo.mix -- the
        // Prism Tank renders; techtree.md §4/§5) so NO synthetic unit fallback is
        // needed, but the art-guard stays defensively: were SREF SHP/cameo absent it
        // would be dropped from ur.defs and --demo-techtree would synthesize a probe
        // def (spec §6).
        {"SREF", true, "GAWEAP"},
    };
    return load_unit_roster_table(dir, ra2, ua, rules, art, kUnits, 5, /*full_parse=*/true);
}

// The Soviet combat roster the exemplar aimd teams field [RE-GATE APPLIED:
// ai-team.md §I exemplars]. These are exactly the UNITIDs the CONFIRMED stock
// Soviet autocreate offensive TaskForces name: E2 (Conscript), HTNK (Rhino),
// APOC (Apocalypse), HTK (Flak Track). Prereqs are the Soviet factories
// (recovered ids from ai-base.md §2: NAHAND barracks, NAWEAP war factory; APOC
// also needs the tech path — proxied to NAWEAP pre-map [RE-FALLBACK]). Any entry
// whose assets fail to load is dropped (art-guard) and parse_aimd then drops the
// teams needing it. Consumed ONLY by the AI config — the player's `ur` roster
// (E1/GGI/MTNK/FV) is untouched, so the sidebar/demo-prod slots/witnesses stay
// byte-identical.
UnitRoster load_soviet_unit_roster(const std::string& dir, const ra2assets::MixFile& ra2,
                                   const UnitAssets& ua, const Rules& rules, const Art& art) {
    static const UnitRosterEnt kSoviet[] = {
        {"E2",   false, "NAHAND"},   // Conscript
        {"HTNK", true,  "NAWEAP"},   // Rhino Tank
        {"APOC", true,  "NAWEAP"},   // Apocalypse
        {"HTK",  true,  "NAWEAP"},   // Flak Track
    };
    return load_unit_roster_table(dir, ra2, ua, rules, art, kSoviet, 4, /*full_parse=*/false);
}

// Spawns a harvester of `va`'s type at/next to (tx,ty): tries the target cell,
// then rings outward, for the first walkable + unoccupied cell (add_harvester's
// placement precondition). Returns the new unit index or -1 if none is free.
// Starts the miner in SeekOre (add_harvester).
int spawn_harvester(ra2engine::World& world, const VehicleAssets& va, bool chrono, int storage,
                    int house, int tx, int ty) {
    for (int r = 0; r <= 8; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // ring border only
                const int x = tx + dx, y = ty + dy;
                if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                    world.add_harvester(x, y, house, va.combat, chrono, storage);
                    return static_cast<int>(world.units().size()) - 1;
                }
            }
    return -1;
}

// Refinery roster indices = types that ship a FreeUnit harvester
// (GAREFN/NAREFN); the World needs these to route unload docking.
std::vector<int> refinery_type_indices(const BuildingAssets& ba, const Rules& rules) {
    std::vector<int> v;
    for (int i = 0; i < static_cast<int>(ba.roster.size()); ++i)
        if (!rules.free_unit(ba.roster[i].id).empty()) v.push_back(i);
    return v;
}

// Engineer/Spy Capture [RE: capture.md §B]: map every roster building TYPE to its
// spy-infiltration purpose (the interactive spy right-click gesture; input.cpp).
// Resolved ONCE from the id + power field, priority Refinery > PowerPlant >
// Superweapon > Factory > RadarLab, else None (spy still enters, inert). Both
// sides' ids are matched (a spy targets ENEMY buildings), unlike the player-only
// factory lists. No demo selects a spy -> no gate exercises this.
std::vector<ra2engine::World::InfiltratePurpose>
infiltrate_purposes_for(const BuildingAssets& ba, const Rules& rules) {
    using P = ra2engine::World::InfiltratePurpose;
    const auto in = [](const std::string& id, std::initializer_list<const char*> set) {
        for (const char* s : set)
            if (id == s) return true;
        return false;
    };
    std::vector<P> out(ba.roster.size(), P::None);
    for (std::size_t i = 0; i < ba.roster.size(); ++i) {
        const ra2engine::BuildingTypeDef& d = ba.roster[i];
        const std::string& id = d.id;
        if (!rules.free_unit(id).empty()) { out[i] = P::Refinery; continue; }  // GAREFN/NAREFN
        if (d.power > 0) { out[i] = P::PowerPlant; continue; }                 // GAPOWR/NAPOWR
        if (in(id, {"NAMISL", "GACHRO", "NAIRON", "GAWEAT", "GAWEATH", "GAAIRC",
                    "YAPSYT", "YAGNTC"})) { out[i] = P::Superweapon; continue; }
        if (in(id, {"GAPILE", "GAWEAP", "GAYARD", "NAHAND", "NAWEAP", "NAYARD",
                    "YABRCK", "YAWEAP"})) { out[i] = P::Factory; continue; }
        if (in(id, {"NARADR", "GARADR", "GATECH", "NATECH", "GACOMM", "NACOMM",
                    "GASPYSAT"})) { out[i] = P::RadarLab; continue; }
    }
    return out;
}

// Wire the World's economy from Rules [RE: economy.md §4]: roster copy,
// refinery type indices, ore/gem Values (Riparius 25 / Cruentus 50), and the
// GrowthRate cadence (minutes->seconds) + Riparius growth/spread percentages.
void wire_economy(ra2engine::World& world, const BuildingAssets& ba, const Rules& rules) {
    world.set_economy(ba.roster, refinery_type_indices(ba, rules),
                      ra2engine::OreValues{rules.value("Riparius"), rules.value("Cruentus")},
                      rules.growth_rate_minutes() * 60.0,
                      rules.growth_percentage("Riparius"),
                      rules.spread_percentage("Riparius"));
}

// FreeUnit grand-opening spawn [RE: economy.md §6]: for every building that
// flipped Buildup->Idle since the previous frame (tracked in `prev_states`,
// grown as buildings are added), if its type ships a FreeUnit and the miner
// assets are loaded, spawn that miner at the refinery's dock cell. Call once
// per frame AFTER world.step(). Instant (scripted) buildings start Idle and so
// never trigger this -- their free units are spawned explicitly by the caller.
//
// Review Important-2: the Buildup->Idle edge detection itself (the part with
// zero automated coverage) is extracted into the pure, directly-testable
// ra2engine::free_unit_grand_openings (harvest.hpp/cpp, covered by
// tests/test_harvest.cpp); this function is now a thin wrapper resolving the
// FreeUnit id/asset/spawn-cell side effects that DO need World/Rules/assets.
void spawn_free_units_on_open(ra2engine::World& world, const BuildingAssets& ba,
                              const Rules& rules, const MinerAssets& miners,
                              std::vector<int>& prev_states) {
    std::vector<bool> free_unit_of_type(ba.roster.size());
    for (std::size_t t = 0; t < ba.roster.size(); ++t)
        free_unit_of_type[t] = !rules.free_unit(ba.roster[t].id).empty();
    for (std::size_t i :
        ra2engine::free_unit_grand_openings(world.buildings(), free_unit_of_type, prev_states)) {
        const ra2engine::Building b = world.buildings()[i];  // copy: dock cell math only
        const std::string fu = rules.free_unit(ba.roster[b.type].id);
        bool chrono = false;
        int storage = 0;
        const VehicleAssets* va = miner_for_id(miners, fu, chrono, storage);
        if (!va) continue;
        spawn_harvester(world, *va, chrono, storage, b.house, ra2engine::dock_cell_x(b),
                        ra2engine::dock_cell_y(b));
        prev_states.resize(world.buildings().size(), -1);  // units added, not buildings; safe
    }
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

// Loads the roster of 8 buildings + their SHP/cameo/sidebar assets through
// the theater candidate walk [RE: building-render.md §1/§6]. Every load is
// individually guarded: a missing main SHP drops that type (art[i].ok=false)
// but never crashes. `ra2`/`language` are already-open archives; `rules`/`art`
// already loaded. Temperate only (letter 'T', isotemp/temperat/generic walk).
BuildingAssets load_building_assets(const ra2assets::MixFile& ra2,
                                    const ra2assets::MixFile& language, const Rules& rules,
                                    const Art& art) {
    BuildingAssets ba;
    // Economy phase adds GAREFN/NAREFN (appended so the existing 0..7 indices
    // -- and thus demo-base's world_hash -- stay byte-identical). Both ship a
    // FreeUnit harvester (GAREFN->CMIN, NAREFN->HARV), driving the charge math
    // below [RE: economy.md §6a].
    // GAPILL (pillbox) / GATOW (patriot) are Allied base defenses (BuildCat=
    // Combat) appended at indices 10/11 [RE-GATE APPLIED: production.md §1.1].
    // They land ONLY on tab 1's separate Defense list; spawn_base never places
    // them by name, so demo-base's world_hash is unchanged.
    // AI-1 phase appends the recovered Soviet build set at indices 12..17
    // [RE-GATE APPLIED: ai-base.md §2 -- SovietBaseDefenses = NALASR,NABNKR,
    // TESLA,NAFLAK; ConcreteWalls -> NAWALL; BuildRadar -> NARADR]. Appended
    // AFTER the existing 12 so every prior index -- and demo-base's
    // world_hash -- is unchanged; spawn_base never places them by name; all
    // six are NA*/T* so the GA*-filtered player sidebar lists are unchanged
    // too. Missing SHPs just leave art[i].ok=false (existing guard) and
    // make_ai_config drops them from the AI catalog (never a crash) -- the
    // GATOW lesson.
    // CB-5 [RE-GATE APPLIED: techtree.md §4]: GATECH (Allied Battle Lab, the
    // TECH-group member) appended LAST so every prior index -- and demo-base's
    // world_hash -- is unchanged; spawn_base never places it by name; it is a GA* id
    // so it does NOT enter the Soviet AI catalog (make_ai_config names NA* ids).
    // Content-only roster growth: the [base] roster= witness grows 18->19, the gated
    // hash is unaffected. Buildings are never DROPPED from ba.roster on art failure
    // (unlike units) -- a missing SHP just leaves ba.art[i].ok=false (placeholder
    // render). CONFIRMED values (techtree.md §4): [GATECH] Prerequisite=GAWEAP,RADAR,
    // GACNST, TechLevel=8, Cost=2000. GATECH is itself radar-gated, BUT the
    // --demo-techtree gate PLACES GATECH DIRECTLY via spawn_base/place_player_building
    // (the base-build path bypasses the buildability predicate), so GATECH's own
    // radar-gated prereq is NEVER evaluated and the demo base needs NO radar.
    // GATECH building ART is art-guarded, NOT hard-required: it loads through the
    // same Allied-building-art path as the other GA* buildings (which may resolve
    // via the NewTheater generic-art munge). On this install it RESOLVES (ok=true,
    // cameo present); on a partial install missing the Allied SHP set it would leave
    // ba.art[i].ok=false + placeholder. Either way roster=19 is UNCONDITIONAL and
    // GATECH stays placeable/roster-findable (buildings are never dropped on art
    // failure), so the gate proves buildability art-independent. SREF art likewise
    // resolves on this install -- no unit fallback needed.
    // Engineer/Spy Capture [RE: capture.md §D1]: the neutral Tech Oil Derrick
    // (CAOILD), appended LAST so every prior index -- and demo-base's world_hash
    // -- is unchanged. It is a NEUTRAL (house 2) building placed only by the
    // capture demo/helper -> no existing gate places it; the [base] roster=
    // witness grows 19->20 (content-only, the CB-5/GATECH precedent). Real values
    // (capture.md §D1): ProduceCashStartup=1000, ProduceCashAmount=20,
    // ProduceCashDelay=100. CAOILD has TechLevel=-1 and a "CA" id (not "GA"), so
    // it is excluded from every buildable/sidebar closure. Art [RE-FALLBACK]: if
    // the CAOILD SHP is absent it stays in ba.roster with art.ok=false +
    // placeholder (buildings are never dropped on art failure).
    static const char* kRoster[24] = {"GACNST", "GAPOWR", "GAPILE", "GAWEAP",
                                       "NACNST", "NAPOWR", "NAHAND", "NAWEAP",
                                       "GAREFN", "NAREFN", "GAPILL", "GATOW",
                                       "NALASR", "NABNKR", "TESLA",  "NAFLAK",
                                       "NAWALL", "NARADR", "GATECH", "CAOILD",
                                       // [RE: roster.md §2/§4, D1] APPEND-ONLY at indices
                                       // 20-23. All four art-present (generic.mix);
                                       // resolved generically by the loop below from
                                       // Rules/Art (stats/prereq/power/cost/armor/sight +
                                       // the defensive weapon for ATESLA/NASAM). Buildings
                                       // are KEPT on art-fail (slot.ok=false). Every prior
                                       // index 0-19 is preserved -> world_hash + all 17
                                       // gate hashes UNCHANGED; only demo-base/techtree's
                                       // roster= WITNESS grows 20->24 (re-pinned in README).
                                       "NANRCT", "ATESLA", "NASAM", "GAAIRC"};
    // rulesmd [General] BuildupTime=.06 (minutes) [RE: building-render.md §5].
    const double build_time_min = rules.general_double("BuildupTime", 0.06);
    // rulesmd [General] ConditionYellow=50 (percent HP threshold for the
    // damaged-vs-healthy building frame) [RE: task-7 review "ConditionYellow"].
    ba.condition_yellow = rules.general_double("ConditionYellow", 50.0) / 100.0;

    // Palettes (both verified present in cache.mix): buildings draw with
    // unittem.pal (+ house remap), cameos with cameo.pal [RE: building-render.md §6].
    MixFile cache = ra2.open_nested("cache.mix");
    auto load_pal = [&](const char* name) -> Palette {
        const MixEntry* pe = cache.find(std::string(name));
        if (!pe) throw std::runtime_error(std::string(name) + " not found in ra2.mix/cache.mix");
        auto raw = cache.read(*pe);
        return Palette::load(raw.data(), raw.size());
    };
    ba.unit_pal = load_pal("unittem.pal");
    ba.cameo_pal = load_pal("cameo.pal");
    // Sidebar chrome palette: sidebar.pal if present (the §11 pixel-decode
    // used it), else cameo.pal (guarded probe).
    ba.sb_pal = ba.cameo_pal;
    if (const MixEntry* se = cache.find(std::string("sidebar.pal"))) {
        auto raw = cache.read(*se);
        ba.sb_pal = Palette::load(raw.data(), raw.size());
    }

    // Theater art mix cache: open each nested mix once, lazily; a mix that
    // fails to open is remembered as null so later candidates just skip it.
    std::map<std::string, std::unique_ptr<MixFile>> mix_cache;
    auto get_mix = [&](const std::string& name) -> MixFile* {
        auto it = mix_cache.find(name);
        if (it != mix_cache.end()) return it->second.get();
        std::unique_ptr<MixFile> m;
        try {
            m = std::make_unique<MixFile>(ra2.open_nested(name));
        } catch (const std::exception&) { m.reset(); }
        MixFile* raw = m.get();
        mix_cache.emplace(name, std::move(m));
        return raw;
    };
    // Resolve an image name to a decoded SHP over the theater candidate walk
    // (first hit wins); throws if no candidate hits.
    auto resolve_shp = [&](const std::string& image) -> ShpFile {
        for (const auto& c : ra2assets::theater_candidates(image, 'T', "isotemp.mix",
                                                           "temperat.mix", ".shp")) {
            MixFile* m = get_mix(c.mix);
            if (!m) continue;
            if (const MixEntry* e = m->find(c.file)) return ShpFile::load(m->read(*e));
        }
        throw std::runtime_error("no theater candidate resolved for " + image);
    };

    ba.roster.reserve(24);
    ba.art.reserve(24);
    int ok_n = 0;
    for (const char* id_c : kRoster) {
        const std::string id = id_c;
        ra2engine::BuildingTypeDef d;
        d.id = id;
        d.fw = d.fh = 1;
        art.foundation(id, d.fw, d.fh);
        d.strength = rules.strength(id);
        d.sight = rules.sight(id);  // [RE: shroud.md §2] reveal radius
        d.grants_map_reveal = rules.spy_sat(id);  // [RE: shroud.md §7A] SpySat= whole-map reveal (inert: no rostered SpySat)
        d.armor_ix = ra2engine::armor_index(rules.armor(id));
        d.cost = rules.cost(id);
        // [RE: economy.md §6a] charge basis: cost normally; the FreeUnit-
        // subtracted amount for a refinery (GAREFN/NAREFN ship a HARV/CMIN,
        // so the queue charges cost - FreeUnit.Cost = 2000-1400 = 600). Build
        // TIME still uses raw `cost` (production.cpp start()).
        const std::string free_id = rules.free_unit(id);
        d.charge = free_id.empty() ? d.cost : std::max(0, d.cost - rules.cost(free_id));
        d.power = rules.power(id);
        d.adjacent = rules.adjacent(id);
        d.tech_level = rules.tech_level(id);
        d.conyard = rules.is_construction_yard(id);
        d.prerequisite = rules.prerequisite(id);
        d.image = id;
        d.buildup = art.buildup(id);
        d.cameo = art.cameo(id);
        d.explosion = rules.explosion_first(id);
        // Engineer/Spy Capture [RE: capture.md §D1]: neutral tech-structure
        // periodic income. Absent from every non-tech section -> 0 default keeps
        // all prior types byte-identical ([CAOILD]: 1000 / 20 / 100).
        d.produce_cash_startup = (int)rules.section_double(id, "ProduceCashStartup", 0.0);
        d.produce_cash_amount = (int)rules.section_double(id, "ProduceCashAmount", 0.0);
        d.produce_cash_delay = (int)rules.section_double(id, "ProduceCashDelay", 0.0);

        BuildingArt slot;
        try {
            slot.shp = resolve_shp(id);
            slot.ok = true;
            ++ok_n;
            if (!d.buildup.empty()) {
                try {
                    slot.buildup = resolve_shp(d.buildup);
                    slot.has_buildup = true;
                    // [RE-GATE APPLIED: building-render.md §5] buildup_total =
                    // image_frames * max(1, trunc(BuildupTime*900/image_frames)) / 15.
                    const int image_frames = slot.buildup.count() / 2;
                    if (image_frames > 0) {
                        const int rate =
                            std::max(1, (int)(build_time_min * 900.0 / image_frames));
                        slot.buildup_total = image_frames * rate / 15.0;
                    }
                } catch (const std::exception&) { slot.has_buildup = false; }
            }
            if (!d.cameo.empty()) {
                try {
                    MixFile cameo_mix = language.open_nested("cameo.mix");
                    std::string cn = d.cameo;
                    for (char& ch : cn) ch = (char)std::tolower((unsigned char)ch);
                    if (const MixEntry* ce = cameo_mix.find(cn + ".shp")) {
                        slot.cameo = ShpFile::load(cameo_mix.read(*ce));
                        slot.has_cameo = true;
                    }
                } catch (const std::exception&) { slot.has_cameo = false; }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[building] " << id << " main image missing (" << ex.what() << ")\n";
            slot.ok = false;
        }
        std::cerr << "[building] " << id << " fnd=" << d.fw << "x" << d.fh << " str=" << d.strength
                  << " cost=" << d.cost << " power=" << d.power << " adj=" << d.adjacent
                  << " tech=" << d.tech_level << " conyard=" << (d.conyard ? "y" : "n")
                  << " buildup=" << (slot.has_buildup ? "y" : "n")
                  << " cameo=" << (slot.has_cameo ? "y" : "n") << " ok=" << (slot.ok ? "y" : "n")
                  << " prereq=[";
        for (size_t k = 0; k < d.prerequisite.size(); ++k)
            std::cerr << (k ? "," : "") << d.prerequisite[k];
        std::cerr << "]\n";
        ba.roster.push_back(std::move(d));
        ba.art.push_back(std::move(slot));
        // [RE-GATE APPLIED: production.md §1.1 — the tab/queue classifier is
        // BuildCat==Combat, NOT IsBaseDefense]
        ba.defense.push_back(rules.is_buildcat_combat(id) ? 1 : 0);
    }
    ba.ok = ok_n > 0;

    // Sidebar chrome (player side = Allied sidec01) [RE-GATE: construction.md
    // §11]: four SEPARATE 28x27 tab SHPs + the 55-frame gclock2.shp clock.
    try {
        MixFile sb = ra2.open_nested("sidec01.mix");
        auto load_sb = [&](const char* fn) -> ShpFile {
            const MixEntry* e = sb.find(std::string(fn));
            if (!e) throw std::runtime_error(std::string("sidebar SHP missing: ") + fn);
            return ShpFile::load(sb.read(*e));
        };
        ba.sb_top = load_sb("top.shp");
        ba.sb_side1 = load_sb("side1.shp");
        ba.sb_side2 = load_sb("side2.shp");
        ba.sb_tabs[0] = load_sb("tab00.shp");
        ba.sb_tabs[1] = load_sb("tab01.shp");
        ba.sb_tabs[2] = load_sb("tab02.shp");
        ba.sb_tabs[3] = load_sb("tab03.shp");
        ba.sb_powerp = load_sb("powerp.shp");
        ba.sb_credits = load_sb("credits.shp");
        ba.sb_gclock = load_sb("gclock2.shp");
        ba.sidebar_ok = true;
        // Optional composition pieces [RE: construction.md §11a] — each
        // individually guarded so a missing decorative SHP degrades to a
        // plain gap, never a dead sidebar.
        auto opt = [&](ra2assets::ShpFile& dst, const char* fn, bool& flag) {
            try { dst = load_sb(fn); flag = true; } catch (const std::exception&) {}
        };
        opt(ba.sb_radar, "radar.shp", ba.has_radar);
        opt(ba.sb_side3, "side3.shp", ba.has_side3);
        opt(ba.sb_addon, "addon.shp", ba.has_addon);
        bool up = false, dn = false;
        opt(ba.sb_rup, "r-up.shp", up);
        opt(ba.sb_rdn, "r-dn.shp", dn);
        ba.has_scroll = up && dn;
        bool rep = false, sel = false;
        opt(ba.sb_repair, "repair.shp", rep);
        opt(ba.sb_sell, "sell.shp", sel);
        ba.has_repair_sell = rep && sel;
        bool ob = false, db = false;
        opt(ba.sb_optbtn, "optbtn.shp", ob);
        opt(ba.sb_diplobtn, "diplobtn.shp", db);
        ba.has_optbtns = ob && db;
        if (!ba.has_radar)
            std::cerr << "[building] radar.shp missing — radar area will be blank\n";
    } catch (const std::exception& ex) {
        std::cerr << "[building] sidebar chrome missing (" << ex.what() << ")\n";
        ba.sidebar_ok = false;
    }

    // CB-4 Superweapons [RE: superweapon.md; spec §6]: the SW cameo, loaded
    // through the EXISTING cameo.mix chain (reusing the `language` handle this
    // function was handed). Art-guarded -- if NukeIcon is absent the SW slot
    // draws a placeholder shade and the sim is unaffected [RE-GATE APPLIED:
    // NukeIcon = language.mix/cameo.mix/nukeicon.shp, CONFIRMED present on this
    // install; still guarded defensively]. sw_has_cameo==false signals "no art".
    ba.sw_has_cameo = false;
    try {
        MixFile cameo_mix = language.open_nested("cameo.mix");
        if (const MixEntry* ce = cameo_mix.find(std::string("nukeicon.shp"))) {
            ba.sb_nukeicon = ShpFile::load(cameo_mix.read(*ce));
            ba.sw_has_cameo = ba.sb_nukeicon.count() > 0;
        }
    } catch (const std::exception&) { ba.sw_has_cameo = false; }
    std::cerr << "[sw] NukeIcon cameo " << (ba.sw_has_cameo ? "loaded" : "absent") << "\n";

    // Shared death explosion (TWLT070) from conquer.mix + anim.pal (guarded;
    // render-only). Mirrors the vehicle death-anim path [RE: building-render.md §4].
    ba.explosion_pal = ba.unit_pal;
    try {
        MixFile conquer = ra2.open_nested("conquer.mix");
        const MixEntry* ee = conquer.find(std::string("twlt070.shp"));
        if (ee) {
            ba.explosion = std::make_unique<ShpFile>(ShpFile::load(conquer.read(*ee)));
            if (const MixEntry* ape = cache.find(std::string("anim.pal"))) {
                auto apal = cache.read(*ape);
                ba.explosion_pal = Palette::load(apal.data(), apal.size());
            }
        }
    } catch (const std::exception&) { ba.explosion.reset(); }

    ba.groups = {
        {"POWER", rules.prerequisite_group("Power")},
        {"BARRACKS", rules.prerequisite_group("Barracks")},
        {"PROC", rules.prerequisite_group("Proc")},
        {"FACTORY", rules.prerequisite_group("Factory")},
        {"RADAR", rules.prerequisite_group("Radar")},
        {"TECH", rules.prerequisite_group("Tech")},
    };

    std::cerr << "[building] roster=" << ba.roster.size() << " ok=" << ok_n
              << " sidebar=" << (ba.sidebar_ok ? "ok" : "missing") << "\n";
    return ba;
}

// Roster index of a building id, or -1.
int roster_index(const BuildingAssets& ba, const std::string& id) {
    for (std::size_t i = 0; i < ba.roster.size(); ++i)
        if (ba.roster[i].id == id) return static_cast<int>(i);
    return -1;
}

// Resolve everything an AI house needs from the loaded assets + rules
// [RE: ai-base.md §2/§4]. Art-guarded: a catalog entry whose SHP failed to
// load is dropped (-1 / omitted) so the planner skips that need.
ra2engine::AiHouseConfig make_ai_config(const BuildingAssets& ba, const Rules& rules,
                                        ra2engine::AiDifficulty difficulty,
                                        double start_credits) {
    ra2engine::AiHouseConfig cfg;
    cfg.difficulty = difficulty;
    cfg.roster = ba.roster;  // the same defs the player machinery uses
    cfg.buildup_totals.resize(ba.art.size(), 0.0);
    for (std::size_t i = 0; i < ba.art.size(); ++i)
        cfg.buildup_totals[i] = ba.art[i].buildup_total;
    cfg.groups = ba.groups;
    auto ok_index = [&](const char* id) -> int {
        const int t = roster_index(ba, id);
        return (t >= 0 && ba.art[t].ok) ? t : -1;  // art-guard-drop
    };
    // Soviet build set [RE-GATE APPLIED: ai-base.md §2 — the RECOVERED [AI]
    // build lists: BuildConst=NACNST, BuildPower=NAPOWR, BuildRefinery=
    // NAREFN, BuildBarracks=NAHAND, BuildWeapons=NAWEAP, BuildRadar=NARADR;
    // SovietBaseDefenses = NALASR,NABNKR,TESLA,NAFLAK (per-Side list — the
    // [AI] BuildDefense/BuildAA/BuildPDefense/BuildHelipad lists are DEAD);
    // ConcreteWalls -> NAWALL (first Soviet-ownable of GAWALL,NAWALL,GAFWLL).
    // Any SHP absent in this install is art-guard-dropped (the GATOW lesson).]
    cfg.catalog.conyard = ok_index("NACNST");
    cfg.catalog.power = ok_index("NAPOWR");
    cfg.catalog.refinery = ok_index("NAREFN");
    cfg.catalog.barracks = ok_index("NAHAND");
    cfg.catalog.war_factory = ok_index("NAWEAP");
    cfg.catalog.radar = ok_index("NARADR");
    for (const char* d : {"NALASR", "NABNKR", "TESLA", "NAFLAK"}) {
        const int t = ok_index(d);
        if (t >= 0) cfg.catalog.defenses.push_back(t);
    }
    cfg.catalog.wall = ok_index("NAWALL");
    // [AI] knobs from the real rulesmd [RE: ai-base.md §2] (constants are the
    // committed pins; the file read keeps us honest if a mod changes them).
    cfg.ai.power_surplus =
        rules.section_double("AI", "PowerSurplus", ra2engine::kAiPowerSurplus);
    // The key is PowerEmergency (NOT PowerEmergencyFraction); the file says
    // "75%" -> strtod yields 75 -> normalize to the 0.75 fraction
    // [RE-GATE APPLIED: ai-base.md §2].
    double pe = rules.section_double("AI", "PowerEmergency",
                                     ra2engine::kAiPowerEmergency * 100.0);
    cfg.ai.power_emergency = pe > 1.0 ? pe / 100.0 : pe;
    cfg.ai.base_size_add = (int)rules.section_double(
        "AI", "BaseSizeAdd", (double)ra2engine::kAiBaseSizeAdd);
    cfg.ai.base_spacing = (int)rules.section_double(
        "AI", "AIBaseSpacing", (double)ra2engine::kAiBaseSpacing);
    cfg.ai.refinery_ratio =
        rules.section_double("AI", "RefineryRatio", ra2engine::kAiRefineryRatio);
    cfg.ai.refinery_limit = (int)rules.section_double(
        "AI", "RefineryLimit", (double)ra2engine::kAiRefineryLimit);
    cfg.ai.war_ratio = rules.section_double("AI", "WarRatio", ra2engine::kAiWarRatio);
    cfg.ai.war_limit =
        (int)rules.section_double("AI", "WarLimit", (double)ra2engine::kAiWarLimit);
    cfg.ai.defense_ratio =
        rules.section_double("AI", "DefenseRatio", ra2engine::kAiDefenseRatio);
    cfg.ai.defense_limit = (int)rules.section_double(
        "AI", "DefenseLimit", (double)ra2engine::kAiDefenseLimit);
    cfg.ai.credit_reserve =
        rules.section_double("AI", "CreditReserve", ra2engine::kAiCreditReserve);
    // Difficulty groups [RE-GATE APPLIED: ai-base.md §4 — the RECOVERED
    // section names Easy/Normal/Difficult (NOT Medium/Hard); BuildDelay is
    // MINUTES; BuildSlowdown is yes/no so it comes from the pinned constant].
    ra2engine::DifficultyGroupRaw g[3];
    const char* kGroups[3] = {"Easy", "Normal", "Difficult"};  // [RE: ai-base.md §4]
    for (int i = 0; i < 3; ++i) {
        g[i].cost = rules.section_double(kGroups[i], "Cost", ra2engine::kDifficultyCost[i]);
        g[i].build_time =
            rules.section_double(kGroups[i], "BuildTime", ra2engine::kDifficultyBuildTime[i]);
        g[i].build_delay = rules.section_double(kGroups[i], "BuildDelay",
                                                ra2engine::kDifficultyBuildDelayMin[i]);
        g[i].build_slowdown = ra2engine::kDifficultyBuildSlowdown[i];  // yes/no key [RE: ai-base.md §4]
    }
    cfg.mods = ra2engine::difficulty_mods(difficulty, g);
    cfg.build_speed = rules.general_double("BuildSpeed", 0.7);
    cfg.low_power_penalty = rules.general_double("LowPowerPenaltyModifier", 1.0);
    cfg.start_credits = start_credits;  // lobby money, UNMODIFIED — no
                                        // starting-credit multiplier exists
                                        // in the difficulty sections
                                        // [RE-GATE APPLIED: ai-base.md §4]
    return cfg;
}

// Spawns a base for `house` with foundation origin near (cx, cy) [RE:
// building-render.md §2 origin, construction.md §9 adjacency]. The ConYard
// (GACNST/NACNST) is placed instant + validity-bypassed; a full Soviet base
// (house 1) additionally gets NAPOWR/NAHAND/NAWEAP, each validity-checked
// (adjacency enforced) against the GROWING base and probed eastward up to k=6
// on a miss. All scripted spawns are instant (start Idle immediately).
void spawn_base(ra2engine::World& world, const BuildingAssets& ba, int house, int cx, int cy,
                bool with_refinery = false, const VehicleAssets* harv_va = nullptr,
                bool harv_chrono = false, int harv_storage = 0) {
    auto place = [&](int type, int px, int py, bool bypass) -> int {
        if (type < 0 || type >= static_cast<int>(ba.art.size()) || !ba.art[type].ok) return -1;
        const ra2engine::BuildingTypeDef& d = ba.roster[type];
        for (int k = 0; k <= 6; ++k) {
            const int tx = px + k;
            const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                tx, py, d.fw, d.fh, house, d.adjacent, world.passable(), world.grid_w(),
                world.grid_h(), world.occupancy(), world.buildings(), bypass);
            if (chk.ok) {
                const int idx =
                    world.add_building(type, tx, py, d.fw, d.fh, house, d.strength, d.armor_ix,
                                       ba.art[type].buildup_total, /*instant=*/true, d.sight,
                                       d.grants_map_reveal);
                std::cerr << "[base] spawned " << d.id << " house=" << house << " at (" << tx << ","
                          << py << ")" << (k ? " probed+" + std::to_string(k) : "") << "\n";
                return idx;
            }
        }
        std::cerr << "[base] FAILED to place " << d.id << " near (" << px << "," << py << ")\n";
        return -1;
    };

    // Multi-house [RE: multihouse.md §1/D6]: side drives the ConYard/base TYPE
    // (Building{type} feeds world_hash). Default house_side(1)==Soviet, else
    // Allied -> reproduces `house == 1 ? NACNST : GACNST` and the house-1 base
    // set EXACTLY -> the spawned types are byte-identical in every gate.
    const bool soviet = (world.house_side(house) == ra2engine::World::Side::Soviet);
    place(roster_index(ba, soviet ? "NACNST" : "GACNST"), cx, cy, /*bypass=*/true);
    if (soviet) {
        place(roster_index(ba, "NAPOWR"), cx + 6, cy, /*bypass=*/false);
        place(roster_index(ba, "NAHAND"), cx + 6, cy + 4, /*bypass=*/false);
        place(roster_index(ba, "NAWEAP"), cx - 1, cy + 6, /*bypass=*/false);
        // Economy phase [RE: economy.md §6]: a refinery east of NAHAND plus its
        // free War Miner adjacent to the dock. Instant + validity-bypassed like
        // the ConYard. demo-base never sets with_refinery (default false), so
        // its spawn_base output -- and world_hash -- is byte-identical.
        if (with_refinery) {
            const int refn = place(roster_index(ba, "NAREFN"), cx + 9, cy + 4, /*bypass=*/true);
            if (refn >= 0 && harv_va) {
                const ra2engine::Building& rb = world.buildings()[refn];
                spawn_harvester(world, *harv_va, harv_chrono, harv_storage, house,
                                ra2engine::dock_cell_x(rb), ra2engine::dock_cell_y(rb));
            }
        }
    }
}

// Draws one building in render_world's merged back-to-front pass. Anchors the
// SHP canvas CENTER at the foundation's screen bbox center [RE-GATE APPLIED:
// building-render.md §2 center-center registration, replacing the pre-hunt
// bottom-center rule]. State selects SHP + frame: Buildup -> buildup MK frame;
// Idle -> main frame 0 (healthy) / 1 (damaged, hp/max<=ConditionYellow); Dead
// -> explosion anim (TWLT070) over kBuildingDieSecs. `level` = the foundation
// origin cell's terrain level (buildings flatten their footprint).
void draw_building(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::Building& b,
                   const BuildingAssets& ba, int level, double condition_yellow,
                   const ra2engine::Camera& cam, int W, int H) {
    if (b.type < 0 || b.type >= static_cast<int>(ba.art.size())) return;
    const BuildingArt& art = ba.art[b.type];
    if (!art.ok) return;
    const ra2engine::BuildingTypeDef& def = ba.roster[b.type];

    // Foundation screen bbox = union of the fw*fh cell diamonds (60x30 rects).
    int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
    for (int j = 0; j < b.fh; ++j)
        for (int i = 0; i < b.fw; ++i) {
            const ra2engine::ScreenPt p =
                ra2engine::cell_to_screen(b.cx + i, b.cy + j, level, 60, 30, 15);
            const int x0 = p.x + cam.x, y0 = p.y + cam.y;
            minx = std::min(minx, x0);
            miny = std::min(miny, y0);
            maxx = std::max(maxx, x0 + 60);
            maxy = std::max(maxy, y0 + 30);
        }
    const int fcx = (minx + maxx) / 2, fcy = (miny + maxy) / 2;

    // Dead: explosion anim centered on the foundation (render-only nicety).
    if (b.state == ra2engine::Building::State::Dead) {
        if (ba.explosion && ba.explosion->count() > 0) {
            int frame = static_cast<int>(b.die_time * 15.0);
            frame = std::clamp(frame, 0, ba.explosion->count() - 1);
            const ra2assets::ShpFile::Frame f = ba.explosion->frame_info(frame);
            SDL_Rect dst{fcx - ba.explosion->width() / 2 + f.x,
                         fcy - ba.explosion->height() / 2 + f.y, f.w, f.h};
            if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H) return;
            char key[96];
            std::snprintf(key, sizeof key, "bldx:%s:%d", def.id.c_str(), frame);
            SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
            if (!tex) {
                std::vector<uint8_t> idx = ba.explosion->decode_indexed(frame);
                tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, ba.explosion_pal));
            }
            SDL_RenderCopy(ren, tex, nullptr, &dst);
        }
        return;
    }

    const bool buildup = (b.state == ra2engine::Building::State::Buildup && art.has_buildup);
    const ra2assets::ShpFile& shp = buildup ? art.buildup : art.shp;
    const int frame = buildup ? ra2engine::buildup_frame(b, art.buildup.count())
                              : ra2engine::building_frame(b, art.shp.count(), condition_yellow);
    if (frame < 0 || frame >= shp.count()) return;
    const ra2assets::ShpFile::Frame f = shp.frame_info(frame);
    SDL_Rect dst{fcx - shp.width() / 2 + f.x, fcy - shp.height() / 2 + f.y, f.w, f.h};
    if (!(dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H)) {
        char key[96];
        std::snprintf(key, sizeof key, "bld:%s:%c:h%d:%d", def.id.c_str(), buildup ? 'b' : 'm',
                      b.house, frame);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            std::vector<uint8_t> idx = shp.decode_indexed(frame);
            const ra2assets::Palette hp = ra2engine::pal_for_house(ba.unit_pal, b.house);
            tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, hp));
        }
        SDL_RenderCopy(ren, tex, nullptr, &dst);
    }

    // Foundation-width HP bar when damaged (render-only; buildings aren't
    // selectable so there's no selection marker).
    if (b.hp < b.max_hp && b.max_hp > 0) {
        const int bw = std::max(24, maxx - minx - 8);
        const int bx = fcx - bw / 2, by = miny - 6;
        SDL_Rect bg{bx - 1, by - 1, bw + 2, 5};
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &bg);
        const double frac = static_cast<double>(b.hp) / b.max_hp;
        SDL_Rect fg{bx, by, static_cast<int>(bw * frac), 3};
        if (frac > 2.0 / 3) SDL_SetRenderDrawColor(ren, 0, 220, 0, 255);
        else if (frac > 1.0 / 3) SDL_SetRenderDrawColor(ren, 220, 220, 0, 255);
        else SDL_SetRenderDrawColor(ren, 220, 0, 0, 255);
        SDL_RenderFillRect(ren, &fg);
    }
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

    // CB-2 Aircraft [RE-GATE APPLIED: aircraft.md §3]: an airborne unit is drawn RAISED
    // by altitude_to_screen_dy, with a ground shadow at the un-raised cell.
    // Guarded on is_aircraft && altitude > 0 -- no existing demo spawns an
    // aircraft, so no existing screenshot changes [spec §8].
    const int extra_dy =
        (u.is_aircraft && u.altitude > 0.0) ? ra2engine::altitude_to_screen_dy(u.altitude) : 0;
    if (extra_dy > 0)
        ra2engine::draw_ground_shadow(ren, u.fx, u.fy, u.level,
                                      va.hull[u.house].sprites[bi].w, cam, W, H);

    ra2engine::draw_voxel_sprite(
        ren, tc, "vxl:" + img + ":h:" + std::to_string(u.house) + ":" + std::to_string(bi),
        va.hull[u.house].sprites[bi], u.fx, u.fy, u.level, cam, W, H, extra_dy);
    if (va.has_turret) {
        const int ti = ra2engine::facing_to_index(u.turret_facing, ra2engine::kVehicleFacings);
        ra2engine::draw_voxel_sprite(
            ren, tc, "vxl:" + img + ":t:" + std::to_string(u.house) + ":" + std::to_string(ti),
            va.turret[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H, extra_dy);
        if (va.has_barrel)
            ra2engine::draw_voxel_sprite(
                ren, tc, "vxl:" + img + ":b:" + std::to_string(u.house) + ":" + std::to_string(ti),
                va.barrel[u.house].sprites[ti], u.fx, u.fy, u.level, cam, W, H, extra_dy);
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
        // H (up) term: the recovered vertical scale is 0.866·60/√2 ≈ 36.74 px
        // per cell-of-z (docs/re/voxel-render.md §6 FLH via §1's buf_y = ... −
        // 0.866·v.z coefficient, in 256√2/60 px-per-lepton units), NOT the
        // 30 px/cell diamond half-height that applies to the F/L ground plane.
        const int flash_dy =
            static_cast<int>(std::lround((mx - my) * 15.0 - (va.flh[2] / 256.0) * 36.74));
        const int cx = static_cast<int>(std::lround((u.fx - u.fy) * 30.0)) + cam.x + 30;
        const int cy =
            static_cast<int>(std::lround((u.fx + u.fy) * 15.0 - u.level * 15)) + cam.y + 15 -
            extra_dy;   // CB-2: track the raised hull when airborne
        SDL_Rect fr{cx + flash_dx - 3, cy + flash_dy - 3, 6, 6};
        if (!(fr.x + fr.w <= 0 || fr.y + fr.h <= 0 || fr.x >= W || fr.y >= H)) {
            SDL_SetRenderDrawColor(ren, 255, 230, 120, 255);
            SDL_RenderFillRect(ren, &fr);
        }
    }
}

// Ore/overlay render pass [RE: economy.md §5]: drawn immediately after the
// terrain tiles and below all objects (units/buildings). For every visible
// map cell that has_ore, pick the tiberium set's art by the cell's variant
// (OreField::variant_at) and select the density frame within the SHP's IMAGE
// half -- min(density, count/2 - 1); the second half is shadow frames, empty
// for ore (verified frame 12 of tib01.tem decodes 0x0). Anchor it exactly
// like a terrain tile (cell_to_screen top-left + the SHP frame's own bbox
// offset) and palettize with the THEATER palette temperat.pal (OreArt::pal;
// see OreArt's PALETTE root-cause note -- isotem.pal renders confetti).
// Cells whose variant art is absent simply don't draw.
void draw_ore(SDL_Renderer* ren, ra2engine::TextureCache& tc, const OreArt& oa,
              const ra2engine::OreField& ore, const ra2assets::MapFile& map,
              const ra2assets::Palette& pal, const ra2engine::Camera& cam, int W, int H) {
    if (!oa.any) return;
    const ra2assets::Palette& ore_pal = oa.has_pal ? oa.pal : pal;
    for (const ra2assets::MapCell& c : map.cells()) {
        if (!ore.has_ore(c.x, c.y)) continue;
        const int kind = (ore.kind_at(c.x, c.y) == ra2engine::OreKind::Gems) ? 1 : 0;
        int slot = ore.variant_at(c.x, c.y);
        if (slot < 0 || slot >= 20) slot = 0;
        // Variant picks the SHP; density picks the frame within it. Fall back
        // to the set's base art (slot 0) when this exact variant is absent.
        if (!oa.present[kind][slot]) {
            slot = 0;
            if (!oa.present[kind][slot]) continue;
        }
        const ra2assets::ShpFile& shp = oa.shp[kind][slot];
        if (shp.count() <= 0) continue;
        // Image frames = first half (second half shadows), like every TS SHP.
        const int image_frames = std::max(1, shp.count() / 2);
        int frame = ore.density_at(c.x, c.y);
        if (frame >= image_frames) frame = image_frames - 1;
        if (frame < 0) frame = 0;
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(c.x, c.y, static_cast<int>(c.level), 60, 30, 15);
        const ra2assets::ShpFile::Frame f = shp.frame_info(frame);
        SDL_Rect dst{p.x + cam.x + f.x, p.y + cam.y + f.y, f.w, f.h};
        if (dst.x + dst.w <= 0 || dst.y + dst.h <= 0 || dst.x >= W || dst.y >= H) continue;
        char key[48];
        std::snprintf(key, sizeof key, "ore:%d:%d:%d", kind, slot, frame);
        SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
        if (!tex) {
            std::vector<uint8_t> idx = shp.decode_indexed(frame);
            tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, ore_pal));
        }
        SDL_RenderCopy(ren, tex, nullptr, &dst);
    }
}

// Finds the loaded VehicleAssets whose resolved image matches `image` (Rhino
// HTNK and the two miners HARV/CMIN all render through the voxel path); nullptr
// if none matches or the list is null. Lets render_world draw several distinct
// voxel types in one scene instead of the old single-`va` special case.
const VehicleAssets* va_for_image(const std::vector<const VehicleAssets*>* vlist,
                                  const std::string& image) {
    if (!vlist) return nullptr;
    for (const VehicleAssets* v : *vlist)
        if (v && v->ok && v->combat.image == image) return v;
    return nullptr;
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
                  const std::vector<const VehicleAssets*>* vlist = nullptr,
                  const BuildingAssets* ba = nullptr, const OreArt* ore_art = nullptr) {
    ra2engine::draw_terrain(ren, tc, a.map, a.theater, a.pal, cam, W, H, &world);
    // Ore/overlay layer: above terrain tiles, below every object [economy.md §5].
    if (ore_art) draw_ore(ren, tc, *ore_art, world.ore(), a.map, a.pal, cam, W, H);
    const std::vector<ra2engine::Unit>& us = world.units();
    // Hull sprite width for a vehicle unit (0 for infantry) -- sizes the
    // selection diamond + HP bar to the wider voxel sprite.
    auto veh_width = [&](const ra2engine::Unit& u) -> int {
        const VehicleAssets* va = va_for_image(vlist, u.image);
        if (!va) return 0;
        const int bi = ra2engine::facing_to_index(u.body_facing, ra2engine::kVehicleFacings);
        return va->hull[u.house].sprites[bi].w;
    };
    // Paths + selection markers first (under the sprites); skip dead/dying
    // units entirely (no path, no selection marker for a corpse-in-progress).
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;  // CB-1:
        ra2engine::draw_path(ren, u, cam, W, H);       // carried units draw nothing
        if (u.selected) ra2engine::draw_selection(ren, u, cam, W, H, veh_width(u));
    }
    // Sprites back-to-front so nearer units overlap farther ones. Vehicles
    // (an image with a VehicleAssets entry) draw via voxel sprites; otherwise
    // pick the SHP by the unit's own image ("GI"/"CONS"). An unrecognized
    // image is logged and skipped (not thrown) so one bad unit can't take
    // down the whole frame.
    auto draw_one_unit = [&](int idx) {
        const ra2engine::Unit& u = us[idx];
        // Shroud/Fog [RE: shroud.md §5]: hide mobile objects in cells not
        // currently VISIBLE to the local player (house 0). Inert with shroud
        // off (visible_at returns true on empty grids) -> every existing render
        // is pixel-identical. Own units (u.house == 0) are always drawn.
        if (u.house != 0 && !world.visible_at(0, u.cell_x, u.cell_y)) return;
        // Engineer/Spy Capture [RE: capture.md §C1]: a disguised unit selects the
        // SHP for its disguise type (draw_unit then blits it in the disguised
        // house's palette). When NOT disguised draw_image == u.image -> this is
        // byte-identical to the pre-Task-7 lookup.
        const std::string& draw_image = u.disguised ? u.disguised_as_type : u.image;
        if (const VehicleAssets* va = va_for_image(vlist, draw_image)) {
            draw_vehicle_unit(ren, tc, u, *va, cam, W, H);
            return;
        }
        auto it = ua.shps.find(draw_image);
        if (it == ua.shps.end()) {
            std::cerr << "[render] no SHP loaded for image=" << draw_image << " (unit " << idx
                      << "); skipping\n";
            return;
        }
        ra2engine::draw_unit(ren, tc, u, it->second, ua.pal, cam, W, H);
    };
    // Merge alive buildings (depth key = center x+y with (f-1)/2, [RE-GATE
    // APPLIED: building-render.md §3]) into the unit draw order (fx+fy);
    // buildings precede units on ties (std::merge takes the first range when
    // keys tie). With no BuildingAssets (or no buildings) this is exactly the
    // old unit-only pass -- the byte-identity-gated demos spawn no buildings.
    const std::vector<int> uorder = ra2engine::unit_draw_order(us);
    struct DrawItem { double key; int kind; int idx; };  // kind 0=building, 1=unit
    std::vector<DrawItem> merged;
    if (ba) {
        const std::vector<ra2engine::Building>& bs = world.buildings();
        auto bkey = [&](int i) {
            const ra2engine::Building& b = bs[i];
            return (b.cx + (b.fw - 1) / 2.0) + (b.cy + (b.fh - 1) / 2.0);
        };
        std::vector<DrawItem> A, B;
        for (int i = 0; i < static_cast<int>(bs.size()); ++i)
            if (bs[i].alive) A.push_back({bkey(i), 0, i});
        std::sort(A.begin(), A.end(), [](const DrawItem& x, const DrawItem& y) { return x.key < y.key; });
        for (int i : uorder) B.push_back({us[i].fx + us[i].fy, 1, i});
        merged.resize(A.size() + B.size());
        std::merge(A.begin(), A.end(), B.begin(), B.end(), merged.begin(),
                   [](const DrawItem& x, const DrawItem& y) { return x.key < y.key; });
    } else {
        for (int i : uorder) merged.push_back({0.0, 1, i});
    }
    for (const DrawItem& d : merged) {
        if (d.kind == 0) {
            const ra2engine::Building& b = world.buildings()[d.idx];
            const ra2assets::MapCell* mc = a.map.at(b.cx, b.cy);
            draw_building(ren, tc, b, *ba, mc ? mc->level : 0, ba->condition_yellow, cam, W, H);
        } else {
            draw_one_unit(d.idx);
        }
    }
    // HP bars after sprites, before the selection box: alive, non-dying,
    // NON-CARRIED units that are selected or currently damaged. A selected
    // transport also shows its loaded pip row -- filled with the SIZE-SUM
    // (cargo_total_size), the pip-only metric [RE-GATE APPLIED:
    // transport-cargo.md §1]; capacity itself is the COUNT [spec §6].
    for (const ra2engine::Unit& u : us) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        if (u.selected || u.hp < u.max_hp)
            ra2engine::draw_hp_bar(ren, u, cam, W, H, veh_width(u));
        if (u.selected && u.passengers_max > 0)
            ra2engine::draw_cargo_pips(ren, u, ra2engine::cargo_total_size(us, u),
                                       u.passengers_max, cam, W, H);
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

// Sidebar helpers (defined below, before run_interactive) used by both the
// screenshot HUD and the interactive loop.
std::vector<int> sidebar_buildable(const BuildingAssets& ba);
void draw_sidebar(SDL_Renderer* ren, ra2engine::TextureCache& tc, const BuildingAssets& ba,
                  const ra2engine::SidebarLayout& lay, const ra2engine::ProductionState& ps,
                  const ra2engine::World& world, int credits_shown, double t_now, int active_tab,
                  const UnitRoster* ur = nullptr, bool unit_factory_alive = true);

int run_screenshot(const std::string& out_path) {
    const int W = 1280, H = 720;
    // ra2.mix opened ONCE and shared by both loaders; the buffer stays
    // resident for the run (accepted tradeoff, see the load_assets comment).
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    VehicleAssets va = load_vehicle_assets(dir, ra2, ua.pal, "HTNK");
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    OreArt ore_art = load_ore_art(ra2);
    // Producible-unit roster (logs [unit-roster] producible=N) so the HUD
    // screenshot can render a unit tab with the real unit cameos.
    UnitRoster ur = load_unit_roster(dir, ra2, ua, rules, art);
    const std::vector<const VehicleAssets*> vlist{&va, &miners.harv, &miners.cmin};
    ra2engine::World world = build_world(a, ua, 5);
    if (va.ok) spawn_tank_squads(world, a, va, 3);
    // Player ConYard + enemy full base flanking the existing squads (visual
    // gate: buildings on their foundations, blue player vs red enemy remap).
    if (ba.ok) {
        const ra2assets::MapCell* sc = ra2engine::pick_start_cell(a.map);
        if (sc) {
            spawn_base(world, ba, /*house=*/0, sc->x, sc->y + 4);
            spawn_base(world, ba, /*house=*/1, sc->x + 14, sc->y, /*with_refinery=*/true,
                       miners.harv.ok ? &miners.harv : nullptr, /*harv_chrono=*/false,
                       miners.harv_storage);
        }
    }

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
        // With bases present, frame the player ConYard region so the visual
        // gate can verify foundation registration + house remap directly.
        if (ba.ok) {
            const ra2assets::MapCell* sc = ra2engine::pick_start_cell(a.map);
            if (sc) {
                const ra2engine::ScreenPt p =
                    ra2engine::cell_to_screen(sc->x + 4, sc->y + 4, 0, 60, 30, 15);
                cam.x = W / 2 - p.x;
                cam.y = H / 2 - p.y;
            }
        }

        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, tc, a, ua, world, cam, W, H, /*input=*/nullptr, &vlist,
                     ba.ok ? &ba : nullptr, ore_art.any ? &ore_art : nullptr);
        // Sidebar HUD so --screenshot shows the full UI. Render the Vehicle tab
        // (tab 3) so the wired unit-roster cameos (MTNK/FV) are visible, with an
        // idle unit queue's ProductionState.
        if (ba.ok && ba.sidebar_ok) {
            std::vector<int> buildable;   // tab 3 (Vehicle): unit-roster indices
            for (int i = 0; i < static_cast<int>(ur.defs.size()); ++i)
                if (ur.defs[i].is_vehicle) buildable.push_back(i);
            const int active_tab = buildable.empty() ? 0 : 3;
            if (buildable.empty()) buildable = sidebar_buildable(ba);
            const ra2engine::SidebarLayout lay =
                ra2engine::SidebarLayout::compute(W, H, buildable);
            ra2engine::ProductionState ps;
            ps.credits = rules.mp_money();
            draw_sidebar(ren, tc, ba, lay, ps, world, static_cast<int>(ps.credits), 0.0,
                         active_tab, &ur);
        }
        capture_png(ren, out_path, W, H);

        std::cerr << "[screenshot] wrote " << out_path << " (" << W << "x" << H << ")\n";
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

// ---- Buildings & Construction Task 7e: sidebar + production glue ----------

// True if `house` owns an alive, Idle ConYard (the BuildingType factory).
bool conyard_alive(const ra2engine::World& world, const BuildingAssets& ba, int house) {
    for (const ra2engine::Building& b : world.buildings()) {
        if (!b.alive || b.state != ra2engine::Building::State::Idle || b.house != house) continue;
        if (b.type >= 0 && b.type < static_cast<int>(ba.roster.size()) && ba.roster[b.type].conyard)
            return true;
    }
    return false;
}

// Per-frame construction glue [RE: construction.md §8/§10], headless-safe (no
// SDL): recompute power, advance the queue, and cancel if the ConYard died.
void ps_tick(ra2engine::ProductionState& ps, const ra2engine::World& world,
             const BuildingAssets& ba, double dt, double low_mod) {
    const ra2engine::PowerTotals pt = ra2engine::power_totals(world.buildings(), ba.roster, 0);
    ra2engine::tick(ps, dt, ra2engine::build_speed_multiplier(pt, low_mod), ba.roster);
    if (ps.slot.type != -1 && !conyard_alive(world, ba, 0)) ra2engine::cancel(ps);
}

// The roster-index list of buildings that are factories for one queue category
// (a single id here — GAPILE for Infantry, GAWEAP for Vehicle, GACNST for the
// building queues). Empty if the id isn't in the roster.
std::vector<int> factory_types_for(const BuildingAssets& ba, const char* id) {
    std::vector<int> v;
    const int t = roster_index(ba, id);
    if (t >= 0) v.push_back(t);
    return v;
}

// Waypoint index -> cell table [RE-FALLBACK: ai-team.md deviations — the map
// [Waypoints] section is not parsed yet; scripted move-to-waypoint arguments
// resolve into this deterministic ring around the PLAYER base, so every
// recovered script marches at the player regardless of which waypoint id it
// names. Replace with real map waypoints in a later phase.]
std::vector<ra2engine::Cellxy> make_ai_waypoints(int px, int py) {
    static const int dx[8] = {0, 4, 4, 4, 0, -4, -4, -4};
    static const int dy[8] = {-4, -4, 0, 4, 4, 4, 0, -4};
    std::vector<ra2engine::Cellxy> w;
    w.reserve(32);
    for (int i = 0; i < 32; ++i)
        w.push_back(ra2engine::Cellxy{px + dx[i % 8], py + dy[i % 8]});
    return w;
}

// Resolve the AI-2 team config from the real aimd.ini + the Soviet unit
// roster [RE: ai-team.md]. (player_x, player_y) anchors the waypoint table.
ra2engine::AiTeamConfig make_ai_team_config(const BuildingAssets& ba, const UnitRoster& ur_ai,
                                            const Rules& rules, const ra2assets::Ini& aimd_ini,
                                            ra2engine::AiDifficulty difficulty,
                                            int player_x, int player_y,
                                            bool enable_triggers = false) {
    ra2engine::AiTeamConfig tc;
    tc.enabled = true;
    tc.unit_roster = ur_ai.defs;
    // UNITID -> roster index; unknown ids -1 -> parse_aimd art-guard-drops
    // the TaskForce and every team on it [RE: ai-team.md §1].
    auto unit_ix = [&](const std::string& id) -> int {
        for (int i = 0; i < (int)ur_ai.defs.size(); ++i)
            if (ur_ai.defs[i].id == id) return i;
        return -1;
    };
    tc.aimd = ra2engine::parse_aimd(aimd_ini, unit_ix);
    // AI-3 [RE: ai-trigger.md §1]: parse [AITriggerTypes] HOUSE-SCOPED through
    // the SAME resolver scoping as the teams above — UNIT condition objects
    // resolve in this house's (Soviet) unit roster, BUILDING objects in the
    // full shared building roster; an unresolvable object or a dropped/missing
    // team1 art-guard-drops the trigger whole, so only fieldable/resolvable
    // triggers survive. Parsed unconditionally (cheap, deterministic, cerr
    // only); the FIREWALL is triggers_enabled below.
    auto bld_ix = [&](const std::string& id) -> int {
        const int i = roster_index(ba, id);
        return (i >= 0 && i < (int)ba.art.size() && ba.art[i].ok) ? i : -1;
    };
    ra2engine::parse_ai_triggers(aimd_ini, tc.aimd, unit_ix, bld_ix);
    tc.triggers_enabled = enable_triggers;
    tc.trigger_side = 2;         // Soviet [RE-GATE APPLIED: ai-trigger.md §5 —
                                 // CONFIRMED SideIndex encoding 0=any,
                                 // 1=Allied, 2=Soviet, 3=Yuri]
    tc.trigger_tech_level = 10;  // skirmish tech ceiling [RE-FALLBACK: no
                                 // recovered per-house tech input; TechLevel
                                 // gates pass at 10]
    std::cerr << "[ai-trigger] aimd triggers parsed=" << tc.aimd.triggers.size()
              << " enabled=" << (enable_triggers ? "yes" : "no") << "\n";
    tc.inf_factory_types = factory_types_for(ba, "NAHAND");   // tab2 [RE: ai-base.md §2]
    tc.veh_factory_types = factory_types_for(ba, "NAWEAP");   // tab3
    tc.house_name = "Russians";  // [RE-GATE APPLIED: ai-team.md §1/§4] — the
                                 // OPTIONAL country restriction only; stock teams
                                 // are House=<none> so this does NOT filter them.
                                 // The real eligibility gate is that the Soviet
                                 // roster can field the TaskForce (parse_aimd
                                 // art-guard-drop). Interactive path is skirmish:
                                 // tc.rules.multiplay stays false (no dissolve).
    tc.waypoints = make_ai_waypoints(player_x, player_y);
    // Team knobs from the real rulesmd, constants as fallbacks
    // [RE: ai-team.md §2/§4]. TeamDelays= is a CSV and section_double would
    // read only its head, so the per-difficulty value comes from the PINNED
    // constant array (the Task 3 install-gated test keeps constant and file
    // head consistent) [RE-FALLBACK: index binding]:
    tc.rules.team_delay_frames = ra2engine::kAiTeamDelayFrames[(int)difficulty];
    tc.rules.autocreate_time_min = rules.section_double(
        "AI", "AutocreateTime", ra2engine::kAiAutocreateTimeMin);   // spawn cadence [§4]
    tc.rules.total_team_cap = (int)rules.section_double(
        "General", "TotalAITeamCap", (double)ra2engine::kAiTotalTeamCap);  // hard cap [§4]
    tc.rules.dissolve_frames = (int)rules.section_double(
        "General", "DissolveUnfilledTeamDelay",
        (double)ra2engine::kAiDissolveUnfilledDelayFrames);
    tc.rules.credit_reserve =
        rules.section_double("AI", "CreditReserve", ra2engine::kAiCreditReserve);
    tc.rules.infantry_reserve = (int)rules.section_double(
        "AI", "InfantryReserve", (double)ra2engine::kAiInfantryReserve);
    // BuildSlowdown is a difficulty BYTE (Easy off / Normal+Difficult on) — the
    // over-queue damp, NOT a time multiplier [RE-GATE APPLIED: ai-team.md §6].
    tc.rules.build_slowdown = ra2engine::kDifficultyBuildSlowdown[(int)difficulty];
    std::cerr << "[ai-team] aimd parsed: tf=" << tc.aimd.taskforces.size()
              << " sc=" << tc.aimd.scripts.size()
              << " teams=" << tc.aimd.teams.size() << "\n";
    return tc;
}

// MultipleFactory build-SPEED multiplier for one category [RE-GATE APPLIED:
// production.md §1.2 — CONFIRMED MultipleFactory=0.8: each EXTRA same-class
// alive Idle owned factory multiplies build TIME by 0.8 (2->x0.8, 3->x0.64),
// so the tick SPEED multiplier is mf^-(n-1)]. n<=1 -> 1.0.
double factory_speedup(const ra2engine::World& world, const std::vector<int>& ftypes, double mf) {
    int n = 0;
    for (const ra2engine::Building& b : world.buildings()) {
        if (!b.alive || b.state != ra2engine::Building::State::Idle || b.house != 0) continue;
        for (int t : ftypes) if (b.type == t) { ++n; break; }
    }
    if (n <= 1 || mf <= 0.0) return 1.0;
    return std::pow(1.0 / mf, n - 1);
}

// Per-frame production glue for the FOUR concurrent queues [RE-GATE APPLIED:
// production.md §1.1/§1.2/§1.5], headless-safe. Recompute power, advance every
// queue (each category's speed = power_mult x its MultipleFactory speedup;
// Structure+Defense share the ONE ConYard factory-count slot), then cancel a
// queue whose factory group was lost — buildings share the ConYard-alive gate
// (both building queues cancel together), Infantry/Vehicle each cancel on their
// own factory group. reconcile_refund settles the shared pool after a cancel.
void prod_tick(ra2engine::ProductionQueues& q, ra2engine::World& world, const BuildingAssets& ba,
               double dt, double low_mod, double mf, const std::vector<int>& inf_ft,
               const std::vector<int>& veh_ft) {
    // Engineer/Spy Capture [RE: capture.md §B2]: a spied power plant forces this
    // house's power OUTPUT to 0 while the blackout timer runs (drain unaffected)
    // -> the existing low-power clamp + offline defenses follow. apply_blackout
    // is IDENTITY when !house_blacked_out, so every existing demo (never blacked
    // out) is byte-identical (production.cpp untouched).
    const ra2engine::PowerTotals pt =
        ra2engine::apply_blackout(ra2engine::power_totals(world.buildings(), ba.roster, 0),
                                  world.house_blacked_out(0));
    const double pmul = ra2engine::build_speed_multiplier(pt, low_mod);
    const std::vector<int> bld_ft = factory_types_for(ba, "GACNST");
    const double bsp = factory_speedup(world, bld_ft, mf);
    const double sp[4] = {bsp, bsp,
                          factory_speedup(world, inf_ft, mf), factory_speedup(world, veh_ft, mf)};
    q.tick(dt, pmul, sp, ba.roster);
    // ConYard-died cancels BOTH building queues (Structure AND Defense share the
    // ConYard-alive factory group) [RE: production.md §1.1].
    if (!conyard_alive(world, ba, 0)) {
        // Both building queues must cancel through the SAME broadcast path
        // (ProductionQueues::cancel), so each refund lands in the shared pool
        // and ACCUMULATES. A raw ra2engine::cancel(q.structure()) refunds only
        // q_[0].credits without broadcasting; the following q.cancel(Defense)
        // then broadcasts q_[1]'s balance over all four queues, OVERWRITING the
        // Structure refund (reconcile_refund can't recover it — all four are
        // already equal). Cancel Structure FIRST so its refund is in the pool
        // before the Defense cancel builds on top of it.
        if (q.producing(ra2engine::QueueCat::Structure) != -1) q.cancel(ra2engine::QueueCat::Structure);
        if (q.producing(ra2engine::QueueCat::Defense) != -1) q.cancel(ra2engine::QueueCat::Defense);
    }
    // Factory-died cancels+refunds the matching unit queue (mirrors ConYard).
    if (q.producing(ra2engine::QueueCat::Infantry) != -1 && world.factory_category_lost(inf_ft, 0))
        q.cancel(ra2engine::QueueCat::Infantry);
    if (q.producing(ra2engine::QueueCat::Vehicle) != -1 && world.factory_category_lost(veh_ft, 0))
        q.cancel(ra2engine::QueueCat::Vehicle);
    q.reconcile_refund();   // a refund raised one queue above the pool tail
}

// Building-roster indices for a building tab: GA* player buildings with a
// cameo, tech>0, art ok, split by the recovered BuildCat==Combat classifier
// [RE-GATE APPLIED: production.md §1.1]. tab 0 (Structures) = BuildCat!=Combat,
// tab 1 (Defenses) = BuildCat==Combat — tab 1 drives its OWN concurrent
// Defense queue (wired in Task 7), NOT a filtered view of the Structure queue.
// Ordered by tech_level then roster order.
std::vector<int> building_buildable_tab(const BuildingAssets& ba, bool defenses) {
    std::vector<int> v;
    for (int i = 0; i < static_cast<int>(ba.roster.size()); ++i) {
        const ra2engine::BuildingTypeDef& d = ba.roster[i];
        if (!ba.art[i].ok || d.cameo.empty() || d.tech_level <= 0) continue;
        if (d.id.size() < 2 || d.id[0] != 'G' || d.id[1] != 'A') continue;  // Allied player side
        const bool def = i < static_cast<int>(ba.defense.size()) && ba.defense[i] != 0;
        if (def != defenses) continue;
        v.push_back(i);
    }
    std::stable_sort(v.begin(), v.end(),
                     [&](int a, int b) { return ba.roster[a].tech_level < ba.roster[b].tech_level; });
    return v;
}

// Structures tab (tab 0) — the DEMO-facing list of the player's non-defense
// buildable buildings. With the Economy phase's GAREFN appended to the roster
// it is {GAPOWR,GAREFN,GAPILE,GAWEAP} (tech-ordered), so it is not the bare
// pre-economy trio; the byte-identity demos build specific ids by roster_index,
// not this list, so their hashes are unaffected.
std::vector<int> sidebar_buildable(const BuildingAssets& ba) {
    return building_buildable_tab(ba, /*defenses=*/false);
}

// Built-in 5x7 digit strip (documented divergence [RE: construction.md §11 —
// the game font is not trivially extractable]); white, right-aligned.
static const uint8_t kDigits[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

void draw_number(SDL_Renderer* ren, int value, int right_x, int y, int scale) {
    const std::string s = std::to_string(value < 0 ? 0 : value);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    int x = right_x - static_cast<int>(s.size()) * 6 * scale;
    for (char ch : s) {
        const int d = ch - '0';
        if (d >= 0 && d <= 9)
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (kDigits[d][row] & (1 << (4 - col))) {
                        SDL_Rect px{x + col * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(ren, &px);
                    }
        x += 6 * scale;
    }
}

// Centered variant: the original's credits tally prints "%d" centered at
// bar x+84 [RE: construction.md §11a, FUN_004A2280]. draw_number is
// right-aligned, so pass center + half the glyph run's width.
void draw_number_centered(SDL_Renderer* ren, int value, int center_x, int y, int scale) {
    const std::string s = std::to_string(value < 0 ? 0 : value);
    const int total_w = static_cast<int>(s.size()) * 6 * scale;
    draw_number(ren, value, center_x + total_w / 2, y, scale);
}

// [RE: skirmish.md §UI, D5] Draw a string as filled rects, same technique as
// draw_number. scale = pixel size of one glyph cell; 6px advance per glyph
// (5 wide + 1 space). Render-only; unhashed -- consumed only by the menu scenes
// (no byte-identity gate calls it), so it cannot affect any gate hash.
void draw_text(SDL_Renderer* ren, const std::string& s, int x, int y, int scale,
               std::uint8_t r = 255, std::uint8_t g = 255, std::uint8_t b = 255) {
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    int cx = x;
    for (char ch : s) {
        const std::array<std::uint8_t, 7>& rows = ra2engine::glyph_rows(ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (rows[row] & (1 << (4 - col))) {
                    SDL_Rect px{cx + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(ren, &px);
                }
        cx += 6 * scale;
    }
}

// Blit one SHP frame at screen (x,y) (frame bbox offset applied), palettized
// via tc under `key` (position-independent, so the same key caches once).
void blit_shp(SDL_Renderer* ren, ra2engine::TextureCache& tc, const std::string& key,
              const ra2assets::ShpFile& shp, int frame, const ra2assets::Palette& pal, int x, int y) {
    if (frame < 0 || frame >= shp.count()) return;
    const ra2assets::ShpFile::Frame f = shp.frame_info(frame);
    // An empty frame (0-dim, e.g. gclock2.shp frame 0 -- the build/recharge clock
    // starts its progress art at frame 1) draws nothing; skip it rather than let
    // TextureCache::get throw on a 0x0 SDL_CreateTexture. Render-only guard; no
    // gate folds textures (they fold world state), and any passing render path
    // already avoids empty frames, so gate outputs are unaffected.
    if (f.w <= 0 || f.h <= 0) return;
    SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
    if (!tex) {
        std::vector<uint8_t> idx = shp.decode_indexed(frame);
        tex = static_cast<SDL_Texture*>(tc.get(key, idx.data(), f.w, f.h, pal));
    }
    SDL_Rect dst{x + f.x, y + f.y, f.w, f.h};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
}

void shade_rect(SDL_Renderer* ren, const ra2engine::SbRect& r, Uint8 alpha, Uint8 rr = 0,
                Uint8 gg = 0, Uint8 bb = 0) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, rr, gg, bb, alpha);
    SDL_Rect rc{r.x, r.y, r.w, r.h};
    SDL_RenderFillRect(ren, &rc);
}

// CB-4 Superweapons [spec §6]: draw the SW cameo + recharge indicator in a
// sidebar slot, reusing the EXISTING gclock2.shp 55-frame chrome + the flashing-
// green READY tint. Drawn ONLY when house 0 has a granted SW, so no existing
// demo (which grants none) changes its sidebar. `slot` is the target rect.
void draw_sw_slot(SDL_Renderer* ren, ra2engine::TextureCache& tc, const BuildingAssets& ba,
                  const ra2engine::World& world, const ra2engine::SbRect& slot, double t_now) {
    const ra2engine::Superweapon* sw = world.sw_state(0);
    if (!sw) return;                                    // no SW granted -> nothing drawn
    // Cameo (art-guarded): NukeIcon, else a placeholder shade.
    if (ba.sw_has_cameo)
        blit_shp(ren, tc, "swcam:nukeicon", ba.sb_nukeicon, 0, ba.cameo_pal, slot.x, slot.y);
    else
        shade_rect(ren, slot, 140);                     // placeholder tile
    if (world.sw_ready(0)) {
        // READY: 2 Hz flashing-green tint (the existing production-ready state).
        if (static_cast<int>(t_now * 2) % 2 == 0) shade_rect(ren, slot, 90, 0, 200, 0);
    } else {
        // Charging: gclock2 frame over a shade (the build-clock chrome, §6).
        shade_rect(ren, slot, 90);
        int gframe = (int)std::lround(world.sw_charge_fraction(0) * 54.0);
        if (gframe < 0) gframe = 0;
        if (gframe >= ba.sb_gclock.count()) gframe = ba.sb_gclock.count() - 1;
        blit_shp(ren, tc, "sw:gclk" + std::to_string(gframe), ba.sb_gclock, gframe, ba.sb_pal,
                 slot.x, slot.y);
    }
}

// Shroud/Fog [RE: shroud.md §5 minimap]: the `Radar=yes` roster ids. A house
// "owns an operational radar" if it has an alive, non-Dead building of one of
// these types (a simple ownership scan; power gating is out of scope this
// phase — the interactive/demo paths that flip shroud on also own a radar).
static bool is_radar_id(const std::string& id) {
    return id == "GAAIRC" || id == "AMRADR" || id == "NARADR" || id == "NAPSIS";
}

bool house_owns_operational_radar(const ra2engine::World& world, const BuildingAssets& ba,
                                  int house) {
    for (const ra2engine::Building& b : world.buildings()) {
        if (b.house != house || !b.alive) continue;
        if (b.state == ra2engine::Building::State::Dead) continue;
        if (b.type < 0 || b.type >= static_cast<int>(ba.roster.size())) continue;
        if (is_radar_id(ba.roster[b.type].id)) return true;
    }
    return false;
}

// Shroud/Fog [RE: shroud.md §5 minimap]: draw a minimap into lay.radar. Paint
// ONLY cells EXPLORED by house 0 (terrain color); overlay unit blips ONLY for
// cells VISIBLE to house 0 (own house-0 units always shown). Gated on house 0
// owning an operational radar. Render-only; NOT part of any gate hash. With
// shroud off the grids are empty (all-explored) -> this is only reached when a
// radar is owned, matching today's behavior of showing the static logo
// otherwise. Falls back to the static "sb:radar" blit when no radar is owned.
void draw_radar_minimap(SDL_Renderer* ren, const ra2engine::SbRect& radar_rect,
                        const ra2engine::World& world, const BuildingAssets& ba) {
    const int gw = world.grid_w(), gh = world.grid_h();
    if (gw <= 0 || gh <= 0) return;
    // Map grid -> the 168x110 radar box (integer scale, letterboxed).
    const double sx = static_cast<double>(radar_rect.w) / gw;
    const double sy = static_cast<double>(radar_rect.h) / gh;
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            if (!world.explored_at(0, x, y)) continue;         // unexplored -> black (skip)
            SDL_Rect px{radar_rect.x + static_cast<int>(x * sx),
                        radar_rect.y + static_cast<int>(y * sy),
                        std::max(1, static_cast<int>(sx)), std::max(1, static_cast<int>(sy))};
            // terrain tint (dim if fogged, brighter if visible)
            Uint8 g = world.visible_at(0, x, y) ? 120 : 60;
            SDL_SetRenderDrawColor(ren, 30, g, 30, 255);
            SDL_RenderFillRect(ren, &px);
        }
    // Unit blips: only in currently-VISIBLE cells (own units always shown).
    // pal_for_house returns a full house-remapped palette (world.cpp:290); the
    // house remap ramp lives at indices 16..31, so a mid-ramp entry (24) is the
    // representative house color (blue=Allied, red=Soviet) — the same recolor
    // the building/unit sprites use.
    for (const ra2engine::Unit& u : world.units()) {
        if (!u.alive) continue;
        if (u.house != 0 && !world.visible_at(0, u.cell_x, u.cell_y)) continue;
        SDL_Rect bp{radar_rect.x + static_cast<int>(u.cell_x * sx),
                    radar_rect.y + static_cast<int>(u.cell_y * sy), 2, 2};
        const ra2assets::Rgba col = world.house_palette(ba.unit_pal, u.house)[24];
        SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(ren, &bp);
    }
}

// Draws the whole sidebar per the recovered composition [RE: construction.md
// §11a — credits(0,0) -> top cap(0,16) -> radar(0,48) -> side1(0,158) w/
// repair+sell -> tabs y=197 -> side2 x N rows from 227 -> side3 -> addon ->
// scroll buttons; §8 power pips]. `t_now` drives the Ready + outage flashes.
void draw_sidebar(SDL_Renderer* ren, ra2engine::TextureCache& tc, const BuildingAssets& ba,
                  const ra2engine::SidebarLayout& lay, const ra2engine::ProductionState& ps,
                  const ra2engine::World& world, int credits_shown, double t_now, int active_tab,
                  const UnitRoster* ur, bool unit_factory_alive) {
    // Opaque backdrop behind any gaps (missing optional chrome), then the
    // recovered top-to-bottom stack.
    shade_rect(ren, lay.bar, 255, 24, 24, 28);
    // Credits strip at the very top; tally centered at x+84 [§11a].
    blit_shp(ren, tc, "sb:credits", ba.sb_credits, 0, ba.sb_pal, lay.credits.x, lay.credits.y);
    draw_number_centered(ren, credits_shown, lay.credits.x + 84, lay.credits.y + 2, 2);
    // Bevel cap.
    blit_shp(ren, tc, "sb:top", ba.sb_top, 0, ba.sb_pal, lay.top_cap.x, lay.top_cap.y);
    // Options / diplomacy buttons over the cap (frame 0, inert).
    if (ba.has_optbtns) {
        blit_shp(ren, tc, "sb:optb", ba.sb_optbtn, 0, ba.sb_pal, lay.bar.x + 11, 20);
        blit_shp(ren, tc, "sb:dipb", ba.sb_diplobtn, 0, ba.sb_pal, lay.bar.x + 83, 20);
    }
    // Radar area: a minimap when house 0 owns an operational radar (Shroud/Fog
    // [RE: shroud.md §5]); else radar.shp frame 0 = the pre-radar side logo
    // (today's behavior, byte-identical with shroud off / no radar) [§11a]. The
    // gate is behind world.shroud_enabled() (false in every existing gate), so
    // the static-logo branch runs exactly as today.
    if (world.shroud_enabled() && house_owns_operational_radar(world, ba, 0)) {
        draw_radar_minimap(ren, lay.radar, world, ba);
    } else if (ba.has_radar) {
        blit_shp(ren, tc, "sb:radar", ba.sb_radar, 0, ba.sb_pal, lay.radar.x, lay.radar.y);
    }
    // side1 band (carries the repair/sell socket art), then the buttons.
    blit_shp(ren, tc, "sb:side1", ba.sb_side1, 0, ba.sb_pal, lay.side1.x, lay.side1.y);
    if (ba.has_repair_sell) {
        blit_shp(ren, tc, "sb:rep", ba.sb_repair, 0, ba.sb_pal, lay.bar.x + 20, 166);
        blit_shp(ren, tc, "sb:sell", ba.sb_sell, 0, ba.sb_pal, lay.bar.x + 84, 166);
    }
    // Four tab buttons (frame 0); the ACTIVE tab full-bright, the others shaded.
    for (int i = 0; i < 4; ++i) {
        blit_shp(ren, tc, "sb:tab" + std::to_string(i), ba.sb_tabs[i], 0, ba.sb_pal, lay.tabs[i].x,
                 lay.tabs[i].y);
        if (i != active_tab) shade_rect(ren, lay.tabs[i], 110);
    }
    // Cameo strip: side2 tiled once per visible row (sockets are baked into
    // the art), side3 bottom cap, addon tail (clips at the window bottom),
    // scroll buttons frame 0 (inert) [§11a].
    for (int k = 0; k < lay.side2_rows; ++k)
        blit_shp(ren, tc, "sb:side2", ba.sb_side2, 0, ba.sb_pal, lay.bar.x,
                 lay.strip_top + k * 50);
    const int strip_bottom = lay.strip_top + lay.side2_rows * 50;
    if (ba.has_side3)
        blit_shp(ren, tc, "sb:side3", ba.sb_side3, 0, ba.sb_pal, lay.bar.x, strip_bottom);
    if (ba.has_addon)
        blit_shp(ren, tc, "sb:addon", ba.sb_addon, 0, ba.sb_pal, lay.bar.x,
                 strip_bottom + (ba.has_side3 ? ba.sb_side3.height() : 0));
    if (ba.has_scroll) {
        blit_shp(ren, tc, "sb:rdn", ba.sb_rdn, 0, ba.sb_pal, lay.scroll_dn.x, lay.scroll_dn.y);
        blit_shp(ren, tc, "sb:rup", ba.sb_rup, 0, ba.sb_pal, lay.scroll_up.x, lay.scroll_up.y);
    }
    // Power meter [RE: construction.md §8 pip model, §11a geometry — the
    // column runs from the strip top at x=+5, capacity (strip_h+3)/3
    // precomputed by SidebarLayout].
    // Engineer/Spy Capture [RE: capture.md §B2]: the HUD visibly blacks out while
    // a spied power plant's timer runs (identity when !house_blacked_out(0), so
    // every existing demo's readout is byte-identical).
    const ra2engine::PowerTotals pt =
        ra2engine::apply_blackout(ra2engine::power_totals(world.buildings(), ba.roster, 0),
                                  world.house_blacked_out(0));
    const int output = pt.output, drain = pt.drain;
    int total_pips = lay.power_pips;
    if (total_pips < 1) total_pips = 1;
    // Capacity scale (FUN_0063F2F0): total_power = Sum output + Sum drain;
    // pips_used = total_pips - clamp(ftol(total_pips*400/(total_power+400)),
    // 0, total_pips-1) -- the clamp upper bound of total_pips-1 (not
    // total_pips) is what guarantees at least 1 pip whenever there is any
    // power at all (and, per the literal formula, even at zero power).
    const int total_power = output + drain;
    const int inner = std::clamp(
        static_cast<int>(total_pips * 400.0 / (total_power + 400.0)), 0, total_pips - 1);
    int pips_used = total_pips - inner;
    if ((output + drain) > 0 && pips_used < 1) pips_used = 1;
    // Color split (FUN_0063F400): surplus = output - drain; surplus<0 -> no
    // green/yellow (red_amt=drain only); else yellow_amt=min(surplus,100),
    // green_amt=surplus-yellow_amt, red_amt=drain. Pips are divided
    // proportionally by amt/(drain+yellow_amt+green_amt) -- for surplus>=0
    // that denominator collapses to `output` (drain+yellow+green = drain +
    // surplus = output), NOT output+drain.
    const int surplus = output - drain;
    int yellow_amt = 0, green_amt = 0;
    if (surplus >= 0) {
        yellow_amt = std::min(surplus, 100);
        green_amt = surplus - yellow_amt;
    }
    const int red_amt = drain;
    const int denom = red_amt + yellow_amt + green_amt;
    int red = 0, yellow = 0;
    if (denom > 0) {
        red = static_cast<int>(std::lround((double)pips_used * red_amt / denom));
        yellow = static_cast<int>(std::lround((double)pips_used * yellow_amt / denom));
    }
    int green = pips_used - red - yellow;
    if (green < 0) green = 0;
    const bool outage = drain > output;
    for (int i = 0; i < total_pips; ++i) {
        int frame = (i < red) ? 3 : (i < red + yellow) ? 2 : (i < pips_used) ? 1 : 0;
        if (outage && i < pips_used && (static_cast<int>(t_now / 0.2) % 2 == 0)) frame = 4;
        const int py = lay.power.y + lay.power.h - (i + 1) * 3;
        blit_shp(ren, tc, "sb:pp" + std::to_string(frame), ba.sb_powerp, frame, ba.sb_pal,
                 lay.power.x, py);
    }
    // Cameo grid + per-state overlay. On unit tabs (2/3) `c.type` indexes the
    // UnitRoster (not ba.art); the blit uses the unit cameo cache and the
    // overlay tracks THIS tab's own queue (`ps` = queues.state(cat)). The
    // disabled-shade can_start call is building-roster-based, so it is guarded
    // to building tabs only (unit tabs skip the cosmetic disabled shade).
    const bool unit_tab = (active_tab >= 2 && ur);
    for (const ra2engine::CameoCell& c : lay.cameos) {
        const int type = c.type;
        if (unit_tab) {
            if (type < 0 || type >= static_cast<int>(ur->defs.size())) continue;
            if (type < static_cast<int>(ur->has_cameo.size()) && ur->has_cameo[type])
                blit_shp(ren, tc, "ucam:" + ur->defs[type].id, ur->cameos[type], 0, ba.cameo_pal,
                         c.rect.x, c.rect.y);
        } else {
            if (type < 0 || type >= static_cast<int>(ba.art.size())) continue;
            if (ba.art[type].has_cameo)
                blit_shp(ren, tc, "cam:" + ba.roster[type].id, ba.art[type].cameo, 0, ba.cameo_pal,
                         c.rect.x, c.rect.y);
        }
        // CB-5 [RE: techtree.md §1]: unit tabs now compute buildability too (was
        // building-tabs-only). Unit cameo availability reuses can_start_unit
        // against the SAME predicate; building cameos keep can_start. Render-only;
        // no gate renders an interactive unit tab (spec §8).
        bool can;
        if (unit_tab)
            can = ra2engine::can_start_unit(ps, type, ur->defs, world.buildings(), ba.roster, 0,
                                            ba.groups, unit_factory_alive);
        else
            can = ra2engine::can_start(ps, type, ba.roster, world.buildings(), 0, ba.groups,
                                       conyard_alive(world, ba, 0));
        if (ps.slot.type == type && ps.slot.ready) {
            // Ready: 2 Hz brightness flash (green tint overlay; READY text is a
            // documented divergence -- no game font in this phase).
            if (static_cast<int>(t_now * 2) % 2 == 0) shade_rect(ren, c.rect, 90, 0, 200, 0);
        } else if (ps.slot.type == type) {
            // Building/unit: gclock2 frame (1 + stage) over a shade [§11].
            shade_rect(ren, c.rect, 90);
            int gframe = 1 + ps.slot.stage;
            if (gframe >= ba.sb_gclock.count()) gframe = ba.sb_gclock.count() - 1;
            blit_shp(ren, tc, "sb:gclk" + std::to_string(gframe), ba.sb_gclock, gframe, ba.sb_pal,
                     c.rect.x, c.rect.y);
        } else if (!can) {
            shade_rect(ren, c.rect, 140);  // Disabled: ~55% black (building AND unit tabs, CB-5)
        }
    }
    // CB-4 Superweapons [spec §6]: the SW cameo + recharge clock in an unused
    // slot (the radar-area top-left). draw_sw_slot no-ops unless house 0 has a
    // granted SW, so no existing demo (which grants none) changes its sidebar.
    const ra2engine::SbRect sw_slot{lay.bar.x + ra2engine::kCameoX, lay.radar.y + 2,
                                    ra2engine::kCameoW, ra2engine::kCameoH};
    draw_sw_slot(ren, tc, ba, world, sw_slot, t_now);
}

// Placement cursor: draw the fw*fh foundation cell diamonds green (cell OK) /
// red (cell blocked OR adjacency fails -> whole grid red) [RE: construction.md §9].
void draw_placement_cursor(SDL_Renderer* ren, const ra2engine::World& world,
                           const BuildingAssets& ba, const ra2engine::Input& input,
                           const Assets& a, const ra2engine::Camera& cam, int W, int H) {
    if (!input.placement_active()) return;
    const int type = input.placement_type();
    if (type < 0 || type >= static_cast<int>(ba.roster.size())) return;
    const ra2engine::BuildingTypeDef& d = ba.roster[type];
    int cx = 0, cy = 0;
    input.placement_cell(cx, cy);
    const ra2engine::PlacementCheck chk = ra2engine::placement_check(
        cx, cy, d.fw, d.fh, 0, d.adjacent, world.passable(), world.grid_w(), world.grid_h(),
        world.occupancy(), world.buildings(), /*bypass_adjacency=*/false);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    const std::vector<ra2engine::Cellxy> cells = ra2engine::foundation_cells(cx, cy, d.fw, d.fh);
    for (std::size_t k = 0; k < cells.size(); ++k) {
        const ra2assets::MapCell* mc = a.map.at(cells[k].x, cells[k].y);
        const int level = mc ? mc->level : 0;
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(cells[k].x, cells[k].y, level, 60, 30, 15);
        const int sx = p.x + cam.x, sy = p.y + cam.y;
        const bool ok = chk.adjacency && (k < chk.cell_ok.size() ? chk.cell_ok[k] : false);
        if (ok) SDL_SetRenderDrawColor(ren, 0, 220, 0, 160);
        else SDL_SetRenderDrawColor(ren, 220, 0, 0, 160);
        SDL_Point diamond[5] = {{sx + 30, sy},      {sx + 60, sy + 15}, {sx + 30, sy + 30},
                                {sx, sy + 15},       {sx + 30, sy}};
        SDL_RenderDrawLines(ren, diamond, 5);
    }
}

// Production chrome [RE: production.md §1.2/§1.4] — cosmetic, never mutates the
// sim. A small filled yellow triangle marks each own (house 0) alive PRIMARY
// unit factory at its foundation screen center; a cyan X marks the selected
// factory's rally cell. Both are clipped to the playfield (left of the sidebar)
// and tagged like the divergent READY overlay. `playfield_w` is the world
// viewport width (window minus the sidebar strip).
void draw_production_chrome(SDL_Renderer* ren, const ra2engine::World& world,
                            const ra2engine::Input& input, const Assets& a,
                            const ra2engine::Camera& cam, int playfield_w) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    auto cell_center_px = [&](int cx, int cy, int& sx, int& sy) {
        const ra2assets::MapCell* mc = a.map.at(cx, cy);
        const int level = mc ? mc->level : 0;
        const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cx, cy, level, 60, 30, 15);
        sx = p.x + cam.x + 30;   // + tile_w/2 (diamond center)
        sy = p.y + cam.y + 15;   // + tile_h/2
    };
    // Primary-factory markers.
    SDL_SetRenderDrawColor(ren, 255, 220, 40, 220);
    for (const ra2engine::Building& b : world.buildings()) {
        if (!b.alive || b.house != 0 || !b.primary) continue;
        int sx = 0, sy = 0;
        cell_center_px(b.cx + b.fw / 2, b.cy + b.fh / 2, sx, sy);
        if (sx < 0 || sx >= playfield_w) continue;
        SDL_Point tri[4] = {{sx, sy - 10}, {sx - 7, sy + 4}, {sx + 7, sy + 4}, {sx, sy - 10}};
        SDL_RenderDrawLines(ren, tri, 4);
        for (int yy = -8; yy <= 3; ++yy) {  // crude fill
            const int half = (yy + 8) * 7 / 11;
            SDL_RenderDrawLine(ren, sx - half, sy + yy, sx + half, sy + yy);
        }
    }
    // Rally marker for the selected factory.
    const int sb = input.selected_building();
    if (sb >= 0 && sb < static_cast<int>(world.buildings().size())) {
        const ra2engine::Building& b = world.buildings()[sb];
        if (b.alive && b.has_rally) {
            int sx = 0, sy = 0;
            cell_center_px(b.rally_x, b.rally_y, sx, sy);
            if (sx >= 0 && sx < playfield_w) {
                SDL_SetRenderDrawColor(ren, 40, 230, 230, 230);
                SDL_RenderDrawLine(ren, sx - 8, sy - 8, sx + 8, sy + 8);
                SDL_RenderDrawLine(ren, sx - 8, sy + 8, sx + 8, sy - 8);
            }
        }
    }
}

// [RE: skirmish.md §2, D4] launch_skirmish is DEFINED far below (near the demo
// gates); forward-declare it here so GameScene's ctor can build its World from a
// SkirmishConfig. Purely additive: the 16 gates never touch this path.
ra2engine::World launch_skirmish(const ra2engine::SkirmishConfig& cfg,
                                 Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                 const Rules& rules);

// [RE: skirmish.md §2, D4] Minimal polymorphic scene interface. Additive: driven
// ONLY at the two non-gate main() seams (Android + desktop fallthrough); no gate
// constructs or enters it. NOT hashed.
struct Scene {
    virtual void   handle(const ra2engine::InputEvent& ev) { (void)ev; }
    virtual void   update(double dt)                        { (void)dt; }
    virtual void   render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) {
        (void)plat; (void)tc;
    }
    virtual Scene* next()                                   { return nullptr; }  // non-null => transition (takes ownership)
    virtual bool   wants_quit() const                       { return false; }
    virtual ~Scene()                                        = default;
};

// The scene-loop driver: owns the active Scene*, pumps input -> handle, ticks
// update(dt) with the real wall-clock dt, calls render, and on a non-null next()
// swaps scenes (deleting the old). Returns 0 on clean quit.
int run_scene_driver(ra2engine::Platform& plat, ra2engine::TextureCache& tc, Scene* initial) {
    std::unique_ptr<Scene> scene(initial);
    Uint32 prev = SDL_GetTicks();
    while (scene && !scene->wants_quit()) {
        ra2engine::InputEvent ev;
        while (plat.poll(ev)) {
            if (ev.kind == ra2engine::InputEvent::Quit) return 0;
            scene->handle(ev);
        }
        const Uint32 now = SDL_GetTicks();
        const double dt = (now - prev) / 1000.0;
        prev = now;
        scene->update(dt);
        plat.begin_frame();
        scene->render(plat, tc);
        plat.present();
        if (Scene* nx = scene->next()) scene.reset(nx);   // transition (variable dt continues)
        SDL_Delay(16);
    }
    return 0;
}

// [RE: skirmish.md §2, D4] The playable game as a Scene. Its ctor builds the World
// via launch_skirmish(config) (replacing run_interactive's hardcoded 2964-3132
// setup block) and initializes assets/audio/camera/input/sidebar; update(dt) runs
// ONE iteration of the former run_interactive loop body at the interactive
// VARIABLE dt (NOT a gate); render(plat,tc) draws render_world + draw_sidebar. All
// loop-local state of the old run_interactive is hoisted to members. Additive: no
// gate constructs GameScene. NOT hashed.
class GameScene : public Scene {
public:
    GameScene(bool no_audio, ra2engine::SkirmishConfig cfg)
        : no_audio_(no_audio),
          dir_(env_install_dir()),
          ra2_(ra2assets::MixFile::open(ra2assets::read_file(dir_ + "ra2.mix"))),
          a_(load_assets(dir_, ra2_)),
          ua_(load_unit_assets(dir_, ra2_)),
          va_(load_vehicle_assets(dir_, ra2_, ua_.pal, "HTNK")),
          language_(ra2assets::MixFile::open(ra2assets::read_file(dir_ + "language.mix"))),
          // Rules/Art wrap their Ini BY REFERENCE -- the Ini must outlive them, so
          // rules_ini_/art_ini_ are members constructed before rules_/art_ (mirrors
          // the old run_interactive's named-local Inis kept alive for the whole run).
          rules_ini_(load_ini_from_ra2md(dir_, "rulesmd.ini")),
          rules_(Rules::load(rules_ini_)),
          art_ini_(load_ini_from_ra2md(dir_, "artmd.ini")),
          art_(Art::load(art_ini_)),
          ba_(load_building_assets(ra2_, language_, rules_, art_)),
          miners_(load_miner_assets(dir_, ra2_, ua_.pal, rules_)),
          transport_(load_transport_assets(dir_, ra2_, ua_.pal)),
          ore_art_(load_ore_art(ra2_)),
          ur_(load_unit_roster(dir_, ra2_, ua_, rules_, art_)),
          ur_ai_(load_soviet_unit_roster(dir_, ra2_, ua_, rules_, art_)),
          // Hardcoded 2964-3132 scenario REPLACED by launch_skirmish [D4].
          world_(launch_skirmish(cfg, a_, ua_, ba_, rules_)),
          buildable_(building_buildable_tab(ba_, /*defenses=*/false)),   // active_tab_ starts at 0
          lay_(ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_)),
          sidebar_up_(ba_.ok && ba_.sidebar_ok),
          playfield_w_(kWinW - (sidebar_up_ ? lay_.bar.w : 0)),
          cam_(centered_camera(a_.map, playfield_w_, kWinH)),
          input_(world_, cam_, playfield_w_, kWinH) {
        // --- vehicle render list (voxel hulls for launched MTNK/FV/etc.) ---
        vlist_ = {&va_, &miners_.harv, &miners_.cmin};
        for (const VehicleAssets& v : ur_.vehicles) vlist_.push_back(&v);
        for (const VehicleAssets& v : ur_ai_.vehicles) vlist_.push_back(&v);
        if (transport_.ok) vlist_.push_back(&transport_.va);

        // Register each infantry def's SHP + anim into the shared UnitAssets store
        // so launched infantry (player + AI-2 Soviet rosters) render. Guarded: a
        // missing SHP just leaves that unit unrenderable, never a crash.
        try {
            MixFile conquer = ra2_.open_nested("conquer.mix");
            for (const std::vector<ra2engine::UnitTypeDef>* defs : {&ur_.defs, &ur_ai_.defs})
                for (const ra2engine::UnitTypeDef& d : *defs) {
                    if (d.is_vehicle || ua_.shps.count(d.combat.image)) continue;
                    try {
                        ua_.shps.emplace(d.combat.image, load_unit_shp(conquer, d.combat.image));
                        ua_.anims.emplace(d.combat.image, anim_from_art(art_, d.combat.image));
                    } catch (const std::exception& ex) {
                        std::cerr << "[unit-roster] infantry SHP " << d.combat.image
                                  << " unavailable (" << ex.what() << ")\n";
                    }
                }
        } catch (const std::exception&) { /* conquer.mix missing: infantry render degrades */ }

        // Production math constants (read every frame in update()).
        build_speed_ = rules_.general_double("BuildSpeed", 0.7);
        low_mod_ = rules_.general_double("LowPowerPenaltyModifier", 1.0);
        mf_ = rules_.general_double("MultipleFactory", 0.8);   // [RE-GATE §1.2: 0.8]

        // Input wiring: refinery-return + factory-select/rally + spy-infiltrate.
        if (ba_.ok) input_.set_refinery_types(refinery_type_indices(ba_, rules_));
        inf_ft_ = factory_types_for(ba_, "GAPILE");
        veh_ft_ = factory_types_for(ba_, "GAWEAP");
        {
            std::vector<int> unit_ft = inf_ft_;
            unit_ft.insert(unit_ft.end(), veh_ft_.begin(), veh_ft_.end());
            input_.set_unit_factory_types(unit_ft);
            input_.set_infantry_factory_types(inf_ft_);
            input_.set_vehicle_factory_types(veh_ft_);
        }
        if (ba_.ok)
            input_.set_infiltrate_params(
                infiltrate_purposes_for(ba_, rules_),
                rules_.general_double("SpyMoneyStealPercent", 0.5),
                static_cast<int>(std::llround(rules_.general_double("SpyPowerBlackout", 1000.0))));

        // Production queues + rolling credits display.
        player_credits_ = rules_.mp_money();
        queues_.set_credits(player_credits_);
        credits_shown_ = player_credits_;

        // Audio (--no-audio skips init_audio entirely; rules_ reused for voices).
        if (!no_audio_) init_audio(audio_, dir_, rules_);
    }

    // The former poll-loop body (one event): sidebar consume-in-rect-before-world,
    // then Input. Escape-cancels-placement is retained here for fidelity (the
    // driver intercepts Quit before handle, so it is dormant on that path).
    void handle(const ra2engine::InputEvent& ev) override {
        if (sidebar_up_ && ev.kind == ra2engine::InputEvent::PointerDown &&
            lay_.contains(ev.x, ev.y)) {
            if (ev.button == ra2engine::InputEvent::Left) {
                // SW cameo slot: left-click arms SW-targeting when house 0's SW is READY.
                if (world_.sw_state(0)) {
                    const ra2engine::SbRect sw_slot{lay_.bar.x + ra2engine::kCameoX,
                                                    lay_.radar.y + 2, ra2engine::kCameoW,
                                                    ra2engine::kCameoH};
                    if (sw_slot.contains(ev.x, ev.y)) {
                        if (world_.sw_ready(0)) input_.arm_superweapon();
                        return;  // consumed by the SW slot
                    }
                }
                // Tab-click switching: recompute the buildable list + layout.
                const int tab = lay_.tab_at(ev.x, ev.y);
                if (tab >= 0 && tab != active_tab_) {
                    active_tab_ = tab;
                    buildable_ = tab_buildable(active_tab_);
                    lay_ = ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_);
                    return;  // consumed: switched tab
                }
                // Cameo start/cancel/place dispatched by the active tab.
                const int ci = lay_.cameo_at(ev.x, ev.y);
                if (ci >= 0 && ci < static_cast<int>(buildable_.size())) {
                    if (active_tab_ >= 2) {
                        const ra2engine::QueueCat cat = (active_tab_ == 3)
                            ? ra2engine::QueueCat::Vehicle
                            : ra2engine::QueueCat::Infantry;
                        const int uslot = buildable_[ci];   // unit-roster index
                        const std::vector<int>& ft =
                            (cat == ra2engine::QueueCat::Vehicle) ? veh_ft_ : inf_ft_;
                        if (queues_.producing(cat) == uslot) {
                            queues_.set_credits(player_credits_);
                            queues_.cancel(cat);
                            queues_.reconcile_refund();
                            player_credits_ = queues_.structure().credits;
                        } else if (ra2engine::can_start_unit(
                                       queues_.state(cat), uslot, ur_.defs, world_.buildings(),
                                       ba_.roster, 0, ba_.groups,
                                       !world_.factory_category_lost(ft, 0))) {
                            queues_.set_credits(player_credits_);
                            queues_.start_unit(cat, uslot, ur_.defs, build_speed_);
                        }
                    } else {
                        const ra2engine::QueueCat bcat = (active_tab_ == 1)
                            ? ra2engine::QueueCat::Defense
                            : ra2engine::QueueCat::Structure;
                        ra2engine::ProductionState& bq = queues_.state(bcat);
                        const int type = buildable_[ci];
                        if (bq.slot.type == type && bq.slot.ready) {
                            place_cat_ = bcat;
                            input_.begin_placement(type, ba_.roster[type].fw, ba_.roster[type].fh);
                        } else if (bq.slot.type == type) {
                            queues_.set_credits(player_credits_);
                            queues_.cancel(bcat);
                            queues_.reconcile_refund();
                            player_credits_ = queues_.structure().credits;
                        } else if (ra2engine::can_start(bq, type, ba_.roster, world_.buildings(),
                                                        0, ba_.groups,
                                                        conyard_alive(world_, ba_, 0))) {
                            queues_.set_credits(player_credits_);
                            ra2engine::start(bq, type, ba_.roster, build_speed_);
                        }
                    }
                }
            }
            return;  // consumed by the sidebar (right-click = no-op)
        }
        // Escape (mapped to Quit) cancels placement instead of quitting.
        if (ev.kind == ra2engine::InputEvent::Quit && input_.placement_active()) {
            input_.cancel_placement();
            return;
        }
        input_.handle(ev);
    }

    // One step of the former loop body: camera pan, edge-detects, world.step,
    // production, exit handoff, free units, audio events, placement/SW confirm.
    void update(double dt) override {
        const int kPan = 40;
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int pan_dx = 0, pan_dy = 0;
        if (ks[SDL_SCANCODE_LEFT]) pan_dx += kPan;
        if (ks[SDL_SCANCODE_RIGHT]) pan_dx -= kPan;
        if (ks[SDL_SCANCODE_UP]) pan_dy += kPan;
        if (ks[SDL_SCANCODE_DOWN]) pan_dy -= kPan;
        SDL_GetMouseState(&mx_, &my_);
        const int kEdge = 24;   // window-edge scroll band (true screen edge, incl. sidebar)
        if (mx_ <= kEdge) pan_dx += kPan;
        else if (mx_ >= kWinW - kEdge) pan_dx -= kPan;
        if (my_ <= kEdge) pan_dy += kPan;
        else if (my_ >= kWinH - kEdge) pan_dy -= kPan;
        if (pan_dx || pan_dy) input_.pan(pan_dx, pan_dy);

        // CB-1 unload key (D, edge-detected) -> unload selected own transports.
        const bool unload_key = ks[SDL_SCANCODE_D] != 0;
        if (unload_key && !prev_unload_key_) world_.order_unload_selected();
        prev_unload_key_ = unload_key;

        // CB-4 Superweapons: N arms SW-targeting when house 0's SW is READY.
        const bool sw_key = ks[SDL_SCANCODE_N] != 0;
        if (sw_key && !prev_sw_key_ && world_.sw_ready(0)) input_.arm_superweapon();
        prev_sw_key_ = sw_key;

        now_ = SDL_GetTicks();   // frame clock for draw_sidebar's animation timing
        world_.step(dt);
        // Advance all four queues from the shared pool, then read the pool back.
        queues_.set_credits(player_credits_);
        prod_tick(queues_, world_, ba_, dt, low_mod_, mf_, inf_ft_, veh_ft_);
        player_credits_ = queues_.structure().credits;
        player_credits_ += world_.take_income(0);   // harvest income drain

        // Exit handoff: any ready unit queue launches from its primary factory.
        for (ra2engine::QueueCat cat :
             {ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle}) {
            if (!queues_.ready(cat)) continue;
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Infantry) ? inf_ft_ : veh_ft_;
            const int fi = ra2engine::primary_factory(world_.buildings(), ft, 0);
            if (fi < 0) continue;
            const int uslot = queues_.producing(cat);
            if (uslot < 0 || uslot >= static_cast<int>(ur_.defs.size())) {
                queues_.placed(cat);
                continue;
            }
            const ra2engine::BuildingTypeDef& fdef = ba_.roster[world_.buildings()[fi].type];
            const int uidx = world_.launch_unit(fi, fdef, ur_.defs[uslot].combat);
            if (uidx >= 0) {
                world_.set_anim(ur_.defs[uslot].combat.image,
                               ua_.anims.count(ur_.defs[uslot].combat.image)
                                   ? ua_.anims.at(ur_.defs[uslot].combat.image)
                                   : ra2engine::default_anim());
                queues_.placed(cat);
            }
        }
        // FreeUnit grand-opening spawns (a just-built refinery drops its miner).
        if (ba_.ok) spawn_free_units_on_open(world_, ba_, rules_, miners_, bstates_);
        // Chrono Miner teleport sounds.
        {
            std::vector<ra2engine::HarvestEvent> hev = world_.take_harvest_events();
            if (audio_.enabled)
                for (const ra2engine::HarvestEvent& e : hev) {
                    if (e.kind == ra2engine::HarvestEvent::Kind::ChronoOut)
                        audio_.mixer->play(audio_.chrono_out);
                    else if (e.kind == ra2engine::HarvestEvent::Kind::ChronoIn)
                        audio_.mixer->play(audio_.chrono_in);
                }
        }
        // CB-1 Enter/LeaveTransportSound (non-gating).
        {
            const std::vector<ra2engine::CargoEvent> cev = world_.take_cargo_events();
            if (audio_.enabled)
                for (const ra2engine::CargoEvent& e : cev) {
                    if (e.kind == ra2engine::CargoEvent::Kind::Board)
                        audio_.mixer->play(audio_.enter_transport);
                    else if (e.kind == ra2engine::CargoEvent::Kind::Eject)
                        audio_.mixer->play(audio_.leave_transport);
                }
        }
        // Rolling credits tally (display-only).
        {
            const double target = player_credits_;
            const double rate = std::max(500.0, std::abs(target - credits_shown_) / 0.5);
            const double step = rate * dt;
            if (credits_shown_ < target) credits_shown_ = std::min(target, credits_shown_ + step);
            else credits_shown_ = std::max(target, credits_shown_ - step);
        }
        // Consume input actions ONCE per frame.
        const ra2engine::InputActions act = input_.take_actions();
        // Placement confirm.
        if (act.place_confirmed && input_.placement_active()) {
            const int type = input_.placement_type();
            if (type >= 0 && type < static_cast<int>(ba_.roster.size())) {
                const ra2engine::BuildingTypeDef& d = ba_.roster[type];
                const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                    act.place_cx, act.place_cy, d.fw, d.fh, 0, d.adjacent, world_.passable(),
                    world_.grid_w(), world_.grid_h(), world_.occupancy(), world_.buildings(),
                    /*bypass_adjacency=*/false);
                if (chk.ok) {
                    world_.add_building(type, act.place_cx, act.place_cy, d.fw, d.fh, 0, d.strength,
                                       d.armor_ix, ba_.art[type].buildup_total, /*instant=*/false,
                                       d.sight, d.grants_map_reveal);
                    queues_.placed(place_cat_);
                    input_.cancel_placement();
                    if (audio_.enabled) audio_.mixer->play("PlaceBuilding");
                }
            }
        }
        // CB-4 Superweapons: a committed SW target cell -> fire.
        if (act.sw_fire_confirmed) world_.fire_superweapon(0, act.sw_cx, act.sw_cy);

        // Audio event collection + acks (gated on audio_.enabled).
        if (audio_.enabled) {
            const double view_center_px_x = kWinW / 2.0 - cam_.x;
            const double view_center_px_y = kWinH / 2.0 - cam_.y;
            const auto& units = world_.units();
            prev_audio_state_.resize(units.size(), {0.0, -1.0});
            for (std::size_t i = 0; i < units.size(); ++i) {
                const auto& u = units[i];
                const double sx = (u.fx - u.fy) * 30.0 - view_center_px_x;
                const double sy = (u.fx + u.fy) * 15.0 - view_center_px_y;
                const double dist = std::max(std::abs(sx), 2.0 * std::abs(sy)) / 60.0;
                const auto& v = audio_.voices[u.image];
                if (u.fire_flash > prev_audio_state_[i].first)
                    audio_.mixer->play(v.report, dist);
                if (u.die_time >= 0.0 && prev_audio_state_[i].second < 0.0) {
                    audio_.mixer->play(v.die, dist);
                    if (u.house == 0 && audio_.eva_unitlost >= 0)
                        audio_.mixer->play_eva(audio_.eva_unitlost);
                }
                prev_audio_state_[i] = {u.fire_flash, u.die_time};
            }
            if (act.actor >= 0) {
                const auto& au = world_.units()[act.actor];
                const double asx = (au.fx - au.fy) * 30.0 - view_center_px_x;
                const double asy = (au.fx + au.fy) * 15.0 - view_center_px_y;
                const double adist = std::max(std::abs(asx), 2.0 * std::abs(asy)) / 60.0;
                const auto& av = audio_.voices[au.image];
                if (act.selected) audio_.mixer->play(av.select, adist);
                else if (act.attack_ordered) audio_.mixer->play(av.attack, adist);
                else if (act.move_ordered) audio_.mixer->play(av.move, adist);
            }
            audio_.out.pump(*audio_.mixer);
        }
    }

    // render_world + placement cursor + sidebar chrome + production chrome.
    // begin_frame/present are the driver's responsibility.
    void render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override {
        SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
        render_world(ren, tc, a_, ua_, world_, cam_, kWinW, kWinH, &input_,
                     &vlist_, ba_.ok ? &ba_ : nullptr, ore_art_.any ? &ore_art_ : nullptr);
        if (!(sidebar_up_ && lay_.contains(mx_, my_)))
            draw_placement_cursor(ren, world_, ba_, input_, a_, cam_, kWinW, kWinH);
        if (ba_.ok && ba_.sidebar_ok) {
            const ra2engine::QueueCat vis_cat =
                (active_tab_ == 1) ? ra2engine::QueueCat::Defense
                : (active_tab_ == 2) ? ra2engine::QueueCat::Infantry
                : (active_tab_ == 3) ? ra2engine::QueueCat::Vehicle
                                     : ra2engine::QueueCat::Structure;
            const bool unit_fac_alive =
                (active_tab_ == 3) ? !world_.factory_category_lost(veh_ft_, 0)
              : (active_tab_ == 2) ? !world_.factory_category_lost(inf_ft_, 0)
                                   : true;
            draw_sidebar(ren, tc, ba_, lay_, queues_.state(vis_cat), world_,
                         static_cast<int>(std::llround(credits_shown_)), now_ / 1000.0,
                         active_tab_, &ur_, unit_fac_alive);
        }
        draw_production_chrome(ren, world_, input_, a_, cam_, playfield_w_);
    }

    bool wants_quit() const override { return input_.wants_quit(); }

private:
    // The window is always created at 1280x720 (see run_interactive); the sidebar
    // layout/camera/input all key off these, so the driver's plat matches.
    static constexpr int kWinW = 1280;
    static constexpr int kWinH = 720;

    // Active sidebar tab (0=Structures,1=Defenses,2=Infantry,3=Vehicles).
    std::vector<int> tab_buildable(int tab) const {
        if (tab == 0) return building_buildable_tab(ba_, /*defenses=*/false);
        if (tab == 1) return building_buildable_tab(ba_, /*defenses=*/true);
        std::vector<int> v;   // tabs 2/3: unit-roster indices
        const bool want_veh = (tab == 3);
        for (int i = 0; i < static_cast<int>(ur_.defs.size()); ++i)
            if (ur_.defs[i].is_vehicle == want_veh) v.push_back(i);
        return v;
    }

    // --- loaded assets (constructed in declaration order in the init list) ---
    bool                 no_audio_;
    std::string          dir_;
    ra2assets::MixFile   ra2_;
    Assets               a_;
    UnitAssets           ua_;
    VehicleAssets        va_;
    ra2assets::MixFile   language_;
    Ini                  rules_ini_;   // kept alive: rules_ references it
    Rules                rules_;
    Ini                  art_ini_;     // kept alive: art_ references it
    Art                  art_;
    BuildingAssets       ba_;
    MinerAssets          miners_;
    TransportAssets      transport_;
    OreArt               ore_art_;
    UnitRoster           ur_;
    UnitRoster           ur_ai_;
    ra2engine::World     world_;
    // --- sidebar / camera / input ---
    int                      active_tab_ = 0;
    std::vector<int>         buildable_;
    ra2engine::SidebarLayout lay_;
    bool                     sidebar_up_ = false;
    int                      playfield_w_ = 0;
    ra2engine::Camera        cam_;
    ra2engine::Input         input_;
    // --- production / economy ---
    ra2engine::ProductionQueues queues_;
    double               player_credits_ = 0.0;
    double               credits_shown_  = 0.0;
    ra2engine::QueueCat  place_cat_ = ra2engine::QueueCat::Structure;
    std::vector<int>     inf_ft_;
    std::vector<int>     veh_ft_;
    double               build_speed_ = 0.7;
    double               low_mod_ = 1.0;
    double               mf_ = 0.8;
    // --- audio ---
    AudioSystem          audio_;
    std::vector<std::pair<double, double>> prev_audio_state_;
    bool                 prev_unload_key_ = false;
    bool                 prev_sw_key_ = false;
    // --- render helpers + FreeUnit tracker + frame-transient state ---
    std::vector<const VehicleAssets*> vlist_;
    std::vector<int>     bstates_;
    int                  mx_ = 0, my_ = 0;
    Uint32               now_ = 0;
};

// Thin wrapper [D4]: default-config quick-start. Behavior-preserving relative to
// the old inline scenario (default_skirmish_config reproduces the 1v1). SAME
// signature so every existing reference (Android + desktop main() seams) still
// compiles. Creates the Platform + TextureCache and drives a single GameScene
// through run_scene_driver.
int run_interactive(bool no_audio) {
    const std::string dir = env_install_dir();
    // Named local: Rules wraps the Ini by reference, so it must outlive `rules`
    // (which default_skirmish_config reads before GameScene construction).
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    ra2engine::Platform plat = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(plat.renderer());
    return run_scene_driver(plat, tc,
                            new GameScene(no_audio, ra2engine::default_skirmish_config(rules)));
}

// ============================================================================
// [RE: skirmish.md §1/§2, D6] Shell scenes: MainMenuScene + SkirmishSetupScene.
// Additive + NOT hashed: reachable ONLY via run_scene_driver from the two
// non-gate main() seams (Android + desktop fallthrough). The 17 gates never
// construct or enter these -- they return from their dispatch arms first.
// ============================================================================

// Human-readable enum names for the setup rows.
static const char* menu_side_name(ra2engine::World::Side s) {
    switch (s) {
        case ra2engine::World::Side::Allied:  return "ALLIED";
        case ra2engine::World::Side::Soviet:  return "SOVIET";
        case ra2engine::World::Side::Yuri:    return "YURI";
        default:                              return "NEUTRAL";
    }
}
static const char* menu_diff_name(ra2engine::AiDifficulty d) {
    switch (d) {
        case ra2engine::AiDifficulty::Easy:      return "EASY";
        case ra2engine::AiDifficulty::Normal:    return "NORMAL";
        default:                                 return "HARD";
    }
}

// Fwd decl: MainMenuScene::handle constructs a SkirmishSetupScene.
class SkirmishSetupScene;

// [RE: skirmish.md §2, D6] Title + buttons: Skirmish [active], Quit [active];
// Campaign/Multiplayer/Load/Options greyed stubs. Hit-test via SbRect::contains.
class MainMenuScene : public Scene {
public:
    explicit MainMenuScene(const Rules& rules, bool no_audio = false)
        : rules_(rules), no_audio_(no_audio) {}
    void   handle(const ra2engine::InputEvent& ev) override;   // out-of-line: needs SkirmishSetupScene
    void   render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override;
    Scene* next() override { Scene* n = next_; next_ = nullptr; return n; }
    bool   wants_quit() const override { return quit_; }

private:
    static constexpr int kWinW = 1280, kWinH = 720;
    // Button rects, laid out identically in render() and handle() (deterministic).
    struct Buttons { ra2engine::SbRect skirmish, campaign, mp, load, options, quit; };
    Buttons layout() const {
        Buttons b;
        const int bw = 380, bh = 50, gap = 14, x = (kWinW - bw) / 2;
        int y = 210;
        b.skirmish = {x, y, bw, bh}; y += bh + gap;
        b.campaign = {x, y, bw, bh}; y += bh + gap;
        b.mp       = {x, y, bw, bh}; y += bh + gap;
        b.load     = {x, y, bw, bh}; y += bh + gap;
        b.options  = {x, y, bw, bh}; y += bh + gap;
        b.quit     = {x, y, bw, bh};
        return b;
    }
    const Rules& rules_;
    bool         no_audio_ = false;
    Scene*       next_ = nullptr;
    bool         quit_ = false;
};

void MainMenuScene::render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) {
    (void)tc;
    SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
    // Title (draw_text is a 5x7 bitmap font; 6*scale px per char).
    {
        const char* title = "RED ALERT 2";
        const int scale = 6;
        const int tw = static_cast<int>(std::string(title).size()) * 6 * scale;
        draw_text(ren, title, (kWinW - tw) / 2, 90, scale, 220, 60, 50);
    }
    auto button = [&](const ra2engine::SbRect& r, const char* label, bool active) {
        SDL_Rect fr{r.x, r.y, r.w, r.h};
        SDL_SetRenderDrawColor(ren, active ? 34 : 20, active ? 52 : 26, active ? 84 : 34, 255);
        SDL_RenderFillRect(ren, &fr);
        SDL_SetRenderDrawColor(ren, active ? 120 : 60, active ? 160 : 72, active ? 210 : 90, 255);
        SDL_RenderDrawRect(ren, &fr);
        const int scale = 3;
        const int tw = static_cast<int>(std::string(label).size()) * 6 * scale;
        const int tx = r.x + (r.w - tw) / 2;
        const int ty = r.y + (r.h - 7 * scale) / 2;
        if (active) draw_text(ren, label, tx, ty, scale, 235, 240, 250);
        else        draw_text(ren, label, tx, ty, scale, 96, 100, 116);
    };
    const Buttons b = layout();
    button(b.skirmish, "SKIRMISH",     true);
    button(b.campaign, "CAMPAIGN",     false);
    button(b.mp,       "MULTIPLAYER",  false);
    button(b.load,     "LOAD GAME",    false);
    button(b.options,  "OPTIONS",      false);
    button(b.quit,     "QUIT",         true);
}

// [RE: skirmish.md §1/§3, D6] Exposes the recovered lobby subset (T1 item 1):
// AI player count + difficulty, per-slot side/color/alliance-team, cash slider
// (5000-10000), unit count (0-10), tech level (1-10), toggles (crates/
// superweapons/short-game). HIDES the 5 DESUPPORTED keys (Bases/ShadowGrow/
// CaptureTheFlag/HarvesterTruce/MultiEngineer). Map = the single mapsmd03.
// GameSpeed OMITTED (T1 item 2: presentation-only, no sim/hash effect).
// Start -> GameScene(launch_skirmish(cfg_)); Back -> MainMenuScene.
class SkirmishSetupScene : public Scene {
public:
    explicit SkirmishSetupScene(const Rules& rules, bool no_audio = false)
        : rules_(rules), no_audio_(no_audio), cfg_(ra2engine::default_skirmish_config(rules)) {}
    void   handle(const ra2engine::InputEvent& ev) override;   // out-of-line: needs MainMenuScene + GameScene
    void   render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override;
    Scene* next() override { Scene* n = next_; next_ = nullptr; return n; }

private:
    static constexpr int kWinW = 1280, kWinH = 720;
    // Global option rows (indices into the labels/values switch below).
    enum GOpt { G_AIPLAYERS, G_DIFFICULTY, G_CASH, G_UNITS, G_TECH,
                G_CRATES, G_SUPERW, G_SHORT, G_COUNT };

    // --- deterministic geometry (shared by render + handle) ---
    ra2engine::SbRect global_rect(int i) const { return {60, 150 + i * 46, 520, 38}; }
    ra2engine::SbRect slot_rect(int row) const { return {640, 190 + row * 46, 600, 38}; }
    ra2engine::SbRect slot_cell(int row, int cell) const {
        const ra2engine::SbRect r = slot_rect(row);
        switch (cell) {
            case 0: return {r.x,       r.y, 110, r.h};  // name (display only)
            case 1: return {r.x + 115, r.y, 150, r.h};  // side
            case 2: return {r.x + 275, r.y, 120, r.h};  // color
            default: return {r.x + 405, r.y, 130, r.h}; // team
        }
    }
    ra2engine::SbRect start_rect() const { return {60,  630, 220, 56}; }
    ra2engine::SbRect back_rect()  const { return {320, 630, 220, 56}; }

    // --- derived state over cfg_.slots ---
    int ai_players() const {
        int n = 0;
        for (const auto& s : cfg_.slots) if (s.combatant && s.is_ai) ++n;
        return n;
    }
    ra2engine::AiDifficulty cur_diff() const {
        for (const auto& s : cfg_.slots) if (s.combatant && s.is_ai) return s.diff;
        return ra2engine::AiDifficulty::Normal;
    }
    std::vector<int> combatant_slots() const {
        std::vector<int> v;
        for (int i = 0; i < static_cast<int>(cfg_.slots.size()); ++i)
            if (cfg_.slots[i].combatant) v.push_back(i);
        return v;
    }
    void set_difficulty(int dir) {
        int d = static_cast<int>(cur_diff()) + dir;
        d = std::max(0, std::min(2, d));
        for (auto& s : cfg_.slots)
            if (s.combatant && s.is_ai) s.diff = static_cast<ra2engine::AiDifficulty>(d);
    }
    // Rebuild the slot table around a new AI count (>=1). Keeps the human (slot 0)
    // and the trailing Neutral civilian; preserves existing AI slot settings.
    void set_ai_players(int n) {
        n = std::max(1, std::min(7, n));
        ra2engine::SkirmishSlot human = cfg_.slots.front();
        ra2engine::SkirmishSlot neutral;
        bool has_neutral = false;
        std::vector<ra2engine::SkirmishSlot> old_ai;
        for (const auto& s : cfg_.slots) {
            if (!s.combatant && !has_neutral) { neutral = s; has_neutral = true; }
            else if (s.combatant && s.is_ai)  { old_ai.push_back(s); }
        }
        const ra2engine::AiDifficulty diff =
            old_ai.empty() ? ra2engine::AiDifficulty::Normal : old_ai.front().diff;
        std::vector<ra2engine::SkirmishSlot> ns;
        ns.push_back(human);
        for (int i = 0; i < n; ++i) {
            if (i < static_cast<int>(old_ai.size())) {
                ns.push_back(old_ai[i]);
            } else {
                ra2engine::SkirmishSlot s;
                s.side          = ra2engine::World::Side::Soviet;
                s.color_ix      = (i + 1) % 8;
                s.country       = "Russians";
                s.alliance_team = i + 1;
                s.combatant     = true;
                s.is_ai         = true;
                s.diff          = diff;
                ns.push_back(s);
            }
        }
        if (has_neutral) ns.push_back(neutral);
        cfg_.slots = ns;
    }
    void adjust_global(int gopt, int dir) {
        switch (gopt) {
            case G_AIPLAYERS:  set_ai_players(ai_players() + dir); break;
            case G_DIFFICULTY: set_difficulty(dir); break;
            case G_CASH:
                cfg_.starting_cash = std::max(5000.0, std::min(10000.0,
                                        cfg_.starting_cash + dir * 100.0));
                break;
            case G_UNITS: cfg_.unit_count = std::max(0, std::min(10, cfg_.unit_count + dir)); break;
            case G_TECH:  cfg_.tech_level = std::max(1, std::min(10, cfg_.tech_level + dir)); break;
            case G_CRATES: cfg_.crates       = !cfg_.crates; break;
            case G_SUPERW: cfg_.superweapons = !cfg_.superweapons; break;
            case G_SHORT:  cfg_.short_game   = !cfg_.short_game; break;
            default: break;
        }
    }
    static const char* glabel(int i) {
        static const char* L[G_COUNT] = {"AI PLAYERS", "DIFFICULTY", "STARTING CASH",
                                         "UNIT COUNT", "TECH LEVEL", "CRATES",
                                         "SUPERWEAPONS", "SHORT GAME"};
        return (i >= 0 && i < G_COUNT) ? L[i] : "";
    }
    std::string gvalue(int i) const {
        switch (i) {
            case G_AIPLAYERS:  return std::to_string(ai_players());
            case G_DIFFICULTY: return menu_diff_name(cur_diff());
            case G_CASH:       return "$" + std::to_string(
                                   static_cast<int>(std::llround(cfg_.starting_cash)));
            case G_UNITS:      return std::to_string(cfg_.unit_count);
            case G_TECH:       return std::to_string(cfg_.tech_level);
            case G_CRATES:     return cfg_.crates       ? "ON" : "OFF";
            case G_SUPERW:     return cfg_.superweapons ? "ON" : "OFF";
            case G_SHORT:      return cfg_.short_game   ? "ON" : "OFF";
            default:           return "";
        }
    }

    const Rules&           rules_;
    bool                   no_audio_ = false;
    ra2engine::SkirmishConfig cfg_;       // seeded from shipped defaults
    Scene*                 next_ = nullptr;
};

void SkirmishSetupScene::render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) {
    (void)tc;
    SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
    // Small helper to draw a bordered box + label + value.
    auto box = [&](const ra2engine::SbRect& r, bool active) {
        SDL_Rect fr{r.x, r.y, r.w, r.h};
        SDL_SetRenderDrawColor(ren, active ? 30 : 22, active ? 44 : 30, active ? 70 : 40, 255);
        SDL_RenderFillRect(ren, &fr);
        SDL_SetRenderDrawColor(ren, active ? 110 : 64, active ? 140 : 76, active ? 190 : 96, 255);
        SDL_RenderDrawRect(ren, &fr);
    };
    // Title + fixed single map.
    draw_text(ren, "SKIRMISH SETUP", 60, 60, 4, 220, 220, 235);
    draw_text(ren, "MAP: MAPSMD03", 640, 60, 3, 180, 200, 180);
    // Global option rows: label (left), value (right of row).
    for (int i = 0; i < G_COUNT; ++i) {
        const ra2engine::SbRect r = global_rect(i);
        box(r, true);
        draw_text(ren, glabel(i), r.x + 8, r.y + 11, 2, 200, 205, 220);
        const std::string v = gvalue(i);
        const int vw = static_cast<int>(v.size()) * 6 * 2;
        draw_text(ren, v, r.x + r.w - vw - 10, r.y + 11, 2, 250, 240, 160);
    }
    // Slot rows header + per-slot side/color/team cells.
    draw_text(ren, "PLAYERS", 640, 160, 2, 200, 205, 220);
    const std::vector<int> cs = combatant_slots();
    for (int row = 0; row < static_cast<int>(cs.size()); ++row) {
        const int idx = cs[row];
        const ra2engine::SkirmishSlot& s = cfg_.slots[idx];
        // name cell (display only)
        const ra2engine::SbRect nr = slot_cell(row, 0);
        const std::string nm = "P" + std::to_string(idx) + (idx == 0 ? " YOU" : " CPU");
        draw_text(ren, nm, nr.x + 4, nr.y + 11, 2, 200, 205, 220);
        // side
        const ra2engine::SbRect sr = slot_cell(row, 1);
        box(sr, true);
        draw_text(ren, menu_side_name(s.side), sr.x + 8, sr.y + 11, 2, 235, 235, 245);
        // color
        const ra2engine::SbRect cr = slot_cell(row, 2);
        box(cr, true);
        draw_text(ren, "COL " + std::to_string(s.color_ix), cr.x + 8, cr.y + 11, 2, 235, 235, 245);
        // team
        const ra2engine::SbRect tr = slot_cell(row, 3);
        box(tr, true);
        const std::string tv = (s.alliance_team < 0) ? "FFA"
                                                      : "TEAM " + std::to_string(s.alliance_team);
        draw_text(ren, tv, tr.x + 8, tr.y + 11, 2, 235, 235, 245);
    }
    // Start / Back.
    auto action = [&](const ra2engine::SbRect& r, const char* label) {
        SDL_Rect fr{r.x, r.y, r.w, r.h};
        SDL_SetRenderDrawColor(ren, 34, 52, 84, 255);
        SDL_RenderFillRect(ren, &fr);
        SDL_SetRenderDrawColor(ren, 120, 160, 210, 255);
        SDL_RenderDrawRect(ren, &fr);
        const int scale = 3;
        const int tw = static_cast<int>(std::string(label).size()) * 6 * scale;
        draw_text(ren, label, r.x + (r.w - tw) / 2, r.y + (r.h - 7 * scale) / 2, scale,
                  235, 240, 250);
    };
    action(start_rect(), "START");
    action(back_rect(),  "BACK");
}

// out-of-line handles (both scene types now complete; GameScene defined above) ---

void MainMenuScene::handle(const ra2engine::InputEvent& ev) {
    if (ev.kind != ra2engine::InputEvent::PointerDown ||
        ev.button != ra2engine::InputEvent::Left)
        return;
    const Buttons b = layout();
    if (b.skirmish.contains(ev.x, ev.y)) { next_ = new SkirmishSetupScene(rules_, no_audio_); return; }
    if (b.quit.contains(ev.x, ev.y))     { quit_ = true; return; }
    // Campaign / Multiplayer / Load / Options are greyed stubs: no-op.
}

void SkirmishSetupScene::handle(const ra2engine::InputEvent& ev) {
    if (ev.kind != ra2engine::InputEvent::PointerDown ||
        ev.button != ra2engine::InputEvent::Left)
        return;
    // Start / Back first.
    if (start_rect().contains(ev.x, ev.y)) {
        next_ = new GameScene(no_audio_, cfg_);   // GameScene ctor runs launch_skirmish(cfg_)
        return;
    }
    if (back_rect().contains(ev.x, ev.y)) {
        next_ = new MainMenuScene(rules_, no_audio_);
        return;
    }
    // Global option rows: left half = decrement/cycle-back, right half = increment.
    for (int i = 0; i < G_COUNT; ++i) {
        const ra2engine::SbRect r = global_rect(i);
        if (r.contains(ev.x, ev.y)) {
            const int dir = (ev.x < r.x + r.w / 2) ? -1 : +1;
            adjust_global(i, dir);
            return;
        }
    }
    // Per-slot cells: click cycles side / color / team forward.
    const std::vector<int> cs = combatant_slots();
    for (int row = 0; row < static_cast<int>(cs.size()); ++row) {
        ra2engine::SkirmishSlot& s = cfg_.slots[cs[row]];
        if (slot_cell(row, 1).contains(ev.x, ev.y)) {         // side: Allied->Soviet->Yuri->...
            s.side = (s.side == ra2engine::World::Side::Allied) ? ra2engine::World::Side::Soviet
                   : (s.side == ra2engine::World::Side::Soviet) ? ra2engine::World::Side::Yuri
                                                                : ra2engine::World::Side::Allied;
            return;
        }
        if (slot_cell(row, 2).contains(ev.x, ev.y)) {         // color 0..7
            s.color_ix = (s.color_ix + 1) % 8;
            return;
        }
        if (slot_cell(row, 3).contains(ev.x, ev.y)) {         // team -1(FFA)..7
            s.alliance_team = (s.alliance_team >= 7) ? -1 : s.alliance_team + 1;
            return;
        }
    }
}

// [RE: skirmish.md §2, D6] The default desktop/Android entry: create the
// Platform/TextureCache, parse Rules (kept alive for the scene lifetime since
// the scenes hold it by reference), and drive from MainMenuScene. run_interactive
// STAYS as the thin quick-start wrapper; this is the new menu-first entry.
int run_menu(bool no_audio) {
    const std::string dir = env_install_dir();
    // Named local: Rules wraps the Ini by reference, so it must outlive `rules`
    // (which the menu scenes hold by reference for the whole driver loop).
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    ra2engine::Platform plat = ra2engine::Platform::create("RA2 Engine", 1280, 720);
    ra2engine::TextureCache tc(plat.renderer());
    // GameScene owns its own audio init; MainMenuScene/SkirmishSetupScene are silent.
    return run_scene_driver(plat, tc, new MainMenuScene(rules, no_audio));
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
    const std::vector<const VehicleAssets*> vlist{&va};
    auto draw = [&]() {
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, /*input=*/nullptr, &vlist);
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

// --- --demo-base helpers ---------------------------------------------------

// FNV-1a 64-bit world-state hash (the --demo-tank pattern, extended with
// building tuples): units (cell/hp/alive) + buildings (type,cx,cy,hp,state).
uint64_t fnv_mix(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v & 0xffull);
        h *= 0x100000001b3ull;
        v >>= 8;
    }
    return h;
}
uint64_t world_hash(const ra2engine::World& world) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
    }
    for (const ra2engine::Building& b : world.buildings()) {
        h = fnv_mix(h, static_cast<uint64_t>(b.type));
        h = fnv_mix(h, static_cast<uint64_t>(b.cx));
        h = fnv_mix(h, static_cast<uint64_t>(b.cy));
        h = fnv_mix(h, static_cast<uint64_t>(b.hp));
        h = fnv_mix(h, static_cast<uint64_t>(b.state));
    }
    return h;
}

// True if `prereq` is satisfiable by owning exactly the buildings named in
// `have_ids` (reuses the tested prerequisites_met over a synthetic Idle base).
bool names_satisfy(const std::vector<std::string>& prereq,
                   const std::vector<std::string>& have_ids, const BuildingAssets& ba) {
    std::vector<ra2engine::Building> fake;
    for (const std::string& id : have_ids) {
        const int t = roster_index(ba, id);
        if (t < 0) continue;
        ra2engine::Building b;
        b.type = t;
        b.house = 0;
        b.state = ra2engine::Building::State::Idle;
        b.alive = true;
        fake.push_back(b);
    }
    return ra2engine::prerequisites_met(prereq, fake, ba.roster, 0, ba.groups);
}

// Demo "buildable" = the sidebar-buildable types reachable from a lone ConYard
// through the tech tree (no refinery, so PROC-gated GAWEAP is excluded) ->
// {GAPOWR, GAPILE}, in sidebar order.
std::vector<int> demo_buildable_closure(const BuildingAssets& ba) {
    const std::vector<int> sb = sidebar_buildable(ba);
    std::vector<std::string> have = {"GACNST"};
    std::vector<int> got;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int t : sb) {
            if (std::find(got.begin(), got.end(), t) != got.end()) continue;
            if (names_satisfy(ba.roster[t].prerequisite, have, ba)) {
                got.push_back(t);
                have.push_back(ba.roster[t].id);
                changed = true;
            }
        }
    }
    std::vector<int> ordered;
    for (int t : sb)
        if (std::find(got.begin(), got.end(), t) != got.end()) ordered.push_back(t);
    return ordered;
}

// Place a player building instant (Idle) at (px,py), probing eastward on a
// validity miss. Returns the placed cell (or false).
bool place_player_building(ra2engine::World& world, const BuildingAssets& ba, int type, int px,
                           int py, int& out_x, int& out_y) {
    if (type < 0 || type >= static_cast<int>(ba.roster.size())) return false;
    const ra2engine::BuildingTypeDef& d = ba.roster[type];
    for (int k = 0; k <= 6; ++k) {
        const int tx = px + k;
        const ra2engine::PlacementCheck chk = ra2engine::placement_check(
            tx, py, d.fw, d.fh, 0, d.adjacent, world.passable(), world.grid_w(), world.grid_h(),
            world.occupancy(), world.buildings(), /*bypass_adjacency=*/false);
        if (chk.ok) {
            world.add_building(type, tx, py, d.fw, d.fh, 0, d.strength, d.armor_ix,
                               ba.art[type].buildup_total, /*instant=*/true, d.sight,
                               d.grants_map_reveal);
            out_x = tx;
            out_y = py;
            return true;
        }
    }
    return false;
}

struct DemoBaseResult {
    std::vector<std::string> witness;  // order-pinned [base] lines
    uint64_t hash = 0;
    bool ok = true;  // all placements succeeded, GAWEAP blocked, NAPOWR died
};

// One scripted run of the base-building demo (fixed 15 Hz dt, frame-driven).
// `render` draws every frame (world + sidebar) into the caller's hidden
// window; the witness log + hash are pure sim state, so rendered and headless
// runs must be byte-identical (the --demo-tank determinism guarantee).
DemoBaseResult simulate_base(Assets& a, UnitAssets& ua, BuildingAssets& ba, double build_speed,
                             double low_mod, int mp_money, bool render, SDL_Renderer* ren,
                             ra2engine::TextureCache* tc, const ra2engine::Camera* cam, int W, int H,
                             const std::string& start_png, const std::string& end_png) {
    DemoBaseResult r;
    ra2engine::World world = build_world(a, ua, 3);  // 3 GI (house 0) + 3 CONS (house 1) scenery
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    // Player ConYard south of the GI squad; enemy full base to the NORTH so the
    // GIs march away from the scenery Conscripts (east) to reach NAPOWR.
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 6);
    spawn_base(world, ba, /*house=*/1, pc->x, pc->y - 16);

    int conyard_bi = -1, napowr_bi = -1;
    const int napowr_type = roster_index(ba, "NAPOWR");
    for (int i = 0; i < static_cast<int>(world.buildings().size()); ++i) {
        const ra2engine::Building& b = world.buildings()[i];
        if (b.house == 0 && ba.roster[b.type].conyard) conyard_bi = i;
        if (b.house == 1 && b.type == napowr_type) napowr_bi = i;
    }

    const std::vector<int> buildable = sidebar_buildable(ba);
    const ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(W, H, buildable);
    ra2engine::ProductionState ps;
    ps.credits = mp_money;

    const int gapowr = roster_index(ba, "GAPOWR");
    const int gapile = roster_index(ba, "GAPILE");
    const int gaweap = roster_index(ba, "GAWEAP");

    const double dt = 1.0 / 15.0;
    long frame = 0;
    double t = 0.0;
    auto draw = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, /*input=*/nullptr, /*va=*/nullptr, &ba);
        if (ba.sidebar_ok)
            draw_sidebar(ren, *tc, ba, lay, ps, world,
                         static_cast<int>(std::llround(ps.credits)), t, /*active_tab=*/0);
    };
    auto step_one = [&]() {
        world.step(dt);
        ps_tick(ps, world, ba, dt, low_mod);
        ++frame;
        t += dt;
        draw();
    };

    // Witness 1: header.
    std::vector<int> closure = demo_buildable_closure(ba);
    std::string blist;
    for (std::size_t i = 0; i < closure.size(); ++i)
        blist += (i ? "," : "") + ba.roster[closure[i]].id;
    {
        char buf[128];
        std::snprintf(buf, sizeof buf, "[base] roster=%d buildable=%s sidebar=%s",
                      static_cast<int>(ba.roster.size()), blist.c_str(),
                      ba.sidebar_ok ? "ok" : "missing");
        r.witness.emplace_back(buf);
    }

    if (render) {
        draw();
        capture_png(ren, start_png, W, H);
    }

    // Frame 15 (t=1.0s): start GAPOWR.
    while (frame < 15) step_one();
    ra2engine::start(ps, gapowr, ba.roster, build_speed);
    {
        char buf[128];
        std::snprintf(buf, sizeof buf, "[base] t=%.1f start GAPOWR frames=%d step_ticks=%d", t,
                      ps.slot.total_frames, ps.slot.step_ticks);
        r.witness.emplace_back(buf);
    }
    while (ps.slot.type == gapowr && !ps.slot.ready) step_one();
    int gx = 0, gy = 0;
    const ra2engine::Building& cy_b = world.buildings()[conyard_bi];
    if (!place_player_building(world, ba, gapowr, cy_b.cx + cy_b.fw, cy_b.cy, gx, gy)) r.ok = false;
    ra2engine::placed(ps);
    {
        const ra2engine::PowerTotals pt = ra2engine::power_totals(world.buildings(), ba.roster, 0);
        char buf[160];
        std::snprintf(buf, sizeof buf, "[base] placed GAPOWR cell=(%d,%d) credits=%d power=+%d/-%d",
                      gx, gy, static_cast<int>(std::llround(ps.credits)), pt.output, pt.drain);
        r.witness.emplace_back(buf);
    }

    // GAPILE (needs POWER -> satisfied now that GAPOWR is Idle).
    ra2engine::start(ps, gapile, ba.roster, build_speed);
    while (ps.slot.type == gapile && !ps.slot.ready) step_one();
    int px2 = 0, py2 = 0;
    if (!place_player_building(world, ba, gapile, cy_b.cx, cy_b.cy + cy_b.fh, px2, py2)) r.ok = false;
    ra2engine::placed(ps);
    {
        const ra2engine::PowerTotals pt = ra2engine::power_totals(world.buildings(), ba.roster, 0);
        char buf[160];
        std::snprintf(buf, sizeof buf, "[base] placed GAPILE credits=%d power=+%d/-%d",
                      static_cast<int>(std::llround(ps.credits)), pt.output, pt.drain);
        r.witness.emplace_back(buf);
    }

    // GAWEAP: prerequisite-blocked without a refinery (PROC).
    const bool can_weap = ra2engine::can_start(ps, gaweap, ba.roster, world.buildings(), 0,
                                               ba.groups, conyard_alive(world, ba, 0));
    if (!can_weap) r.witness.emplace_back("[base] GAWEAP blocked (PROC unmet)");
    else { r.witness.emplace_back("[base] GAWEAP NOT blocked (unexpected)"); r.ok = false; }

    // Order the GIs to attack the enemy NAPOWR; run until it dies (or a cap).
    for (ra2engine::Unit& u : world.units())
        if (u.house == 0 && u.alive && u.die_time < 0.0) u.selected = true;
    world.order_attack_building(napowr_bi);
    const double cap = 400.0;  // sim seconds
    auto napowr_dead = [&]() {
        const ra2engine::Building& b = world.buildings()[napowr_bi];
        return !b.alive || b.state == ra2engine::Building::State::Dead;
    };
    while (!napowr_dead() && t < cap) step_one();
    r.ok = r.ok && napowr_dead();
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "[base] NAPOWR destroyed t=%.1f", t);
        r.witness.emplace_back(buf);
    }

    // Let the death anim tombstone, then hash.
    for (int k = 0; k < 20; ++k) step_one();
    r.hash = world_hash(world);
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "[base] hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(buf);
    }

    if (render) {
        draw();
        capture_png(ren, end_png, W, H);
    }
    return r;
}

// --demo-base: the Buildings & Construction integration gate. Builds a player
// ConYard + a Soviet base, scripts GAPOWR -> place -> GAPILE -> place -> GAWEAP
// (asserted blocked), then destroys the enemy NAPOWR. Runs rendered (hidden
// window + start/end screenshots via the building draw path) and headless, and
// byte-compares the two witness logs + world hashes.
int run_demo_base() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_base_start.png";
    const std::string end_png = scratch + "demo_base_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-base] FAILED: building assets unavailable\n";
        return 1;
    }
    const double build_speed = rules.general_double("BuildSpeed", 0.7);
    const double low_mod = rules.general_double("LowPowerPenaltyModifier", 1.0);
    const int mp_money = rules.mp_money();

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoBaseResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-base)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_base(a, ua, ba, build_speed, low_mod, mp_money, /*render=*/true, ren, &tc,
                               &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoBaseResult r2 = simulate_base(a, ua, ba, build_speed, low_mod, mp_money, /*render=*/false,
                                      nullptr, nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-base] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (identical ? "yes" : "no") << " lines=" << r1.witness.size()
              << "\n";
    return bad ? 1 : 0;
}

// --- --demo-econ helpers ---------------------------------------------------

// FNV extension of world_hash with the ore field + credits, for --demo-econ's
// two-run byte-identity gate. world_hash itself stays UNTOUCHED (demo-base's
// pin is unaffected).
uint64_t econ_hash(const ra2engine::World& world, double player_credits, double enemy_income) {
    uint64_t h = world_hash(world);
    const ra2engine::OreField& ore = world.ore();
    for (int y = 0; y < ore.h(); ++y)
        for (int x = 0; x < ore.w(); ++x) {
            if (!ore.has_ore(x, y)) continue;
            h = fnv_mix(h, static_cast<uint64_t>(x));
            h = fnv_mix(h, static_cast<uint64_t>(y));
            h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(ore.kind_at(x, y))));
            h = fnv_mix(h, static_cast<uint64_t>(ore.density_at(x, y)));
        }
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(player_credits)));
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(enemy_income * 100.0)));
    return h;
}

// Seeds a solid ore patch (density d, variant 0) on walkable, ore-free cells
// within Chebyshev `radius` of (cx,cy); returns cells seeded. Guarantees the
// demo's harvesters have deterministic ore next to their refinery regardless
// of the reference map's natural ore locations.
int seed_ore_patch(ra2engine::World& world, int cx, int cy, int radius, int d) {
    int n = 0;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const int x = cx + dx, y = cy + dy;
            if (!world.walkable(x, y) || world.ore().has_ore(x, y)) continue;
            world.ore().set(x, y, ra2engine::OreKind::Ore, d, 0);
            ++n;
        }
    return n;
}

// Index of the first alive building of roster `type` owned by `house`, or -1.
int find_building(const ra2engine::World& world, int type, int house) {
    const std::vector<ra2engine::Building>& bs = world.buildings();
    for (int i = 0; i < static_cast<int>(bs.size()); ++i)
        if (bs[i].alive && bs[i].type == type && bs[i].house == house) return i;
    return -1;
}

struct DemoEconResult {
    std::vector<std::string> witness;
    uint64_t hash = 0;
    bool ok = true;
};

// One scripted run of the economy demo (fixed 15 Hz dt, frame-driven, like
// simulate_base). Player Allied base (+ instant GAPOWR/GAPILE so GAWEAP's only
// missing prereq is PROC) vs a Soviet base with a refinery + War Miner. Script:
// build+place GAREFN (its CMIN FreeUnit spawns), assert GAWEAP unblocks, run
// until the player's CMIN and the enemy's HARV each complete an unload, then
// past the first ore-growth firing, and hash. Rendered and headless runs must
// be byte-identical (witness + econ_hash are pure sim state).
DemoEconResult simulate_econ(Assets& a, UnitAssets& ua, BuildingAssets& ba, MinerAssets& miners,
                             const Rules& rules, double build_speed, double low_mod, int mp_money,
                             const std::vector<const VehicleAssets*>& vlist, const OreArt& ore_art,
                             bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                             const ra2engine::Camera* cam, int W, int H,
                             const std::string& start_png, const std::string& end_png) {
    DemoEconResult r;
    ra2engine::World world = build_world(a, ua, 0);  // no scenery infantry
    wire_economy(world, ba, rules);

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 6);
    spawn_base(world, ba, /*house=*/1, pc->x, pc->y - 16, /*with_refinery=*/true,
               miners.harv.ok ? &miners.harv : nullptr, /*harv_chrono=*/false, miners.harv_storage);

    const int gacnst = roster_index(ba, "GACNST");
    const int gapowr = roster_index(ba, "GAPOWR");
    const int gapile = roster_index(ba, "GAPILE");
    const int gaweap = roster_index(ba, "GAWEAP");
    const int garefn = roster_index(ba, "GAREFN");
    const int narefn = roster_index(ba, "NAREFN");

    // Player POWER + BARRACKS (instant, no charge): GAWEAP prereq is
    // [PROC, GAPILE, GACNST], so with these placed only PROC (the refinery
    // built below) is still missing -- the inverse of demo-base.
    const int cy_bi = find_building(world, gacnst, 0);
    if (cy_bi < 0) r.ok = false;
    const ra2engine::Building cy_b =
        cy_bi >= 0 ? world.buildings()[cy_bi] : ra2engine::Building{};
    {
        int ox = 0, oy = 0;
        if (!place_player_building(world, ba, gapowr, cy_b.cx + cy_b.fw, cy_b.cy, ox, oy)) r.ok = false;
        if (!place_player_building(world, ba, gapile, cy_b.cx, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false;
    }

    // Enemy ore patch beside its refinery dock (the War Miner starts hunting on
    // the first step).
    const int narefn_bi = find_building(world, narefn, 1);
    if (narefn_bi >= 0) {
        const ra2engine::Building nb = world.buildings()[narefn_bi];
        seed_ore_patch(world, ra2engine::dock_cell_x(nb), ra2engine::dock_cell_y(nb) + 3, 3,
                       ra2engine::kMaxOreDensity);
    }

    const ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(W, H, sidebar_buildable(ba));
    ra2engine::ProductionState ps;
    ps.credits = mp_money;

    const double dt = 1.0 / 15.0;
    const double cap = 400.0;
    long frame = 0;
    double t = 0.0;
    double enemy_income = 0.0;
    bool cmin_unloaded = false;
    double cmin_credits = 0.0;
    bool enemy_unloaded = false;
    double enemy_income_at = 0.0;

    auto draw = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, /*input=*/nullptr, &vlist, &ba,
                     ore_art.any ? &ore_art : nullptr);
        if (ba.sidebar_ok)
            draw_sidebar(ren, *tc, ba, lay, ps, world, static_cast<int>(std::llround(ps.credits)),
                         t, /*active_tab=*/0);
    };
    auto step_one = [&]() {
        world.step(dt);
        ps_tick(ps, world, ba, dt, low_mod);
        ps.credits += world.take_income(0);
        enemy_income += world.take_income(1);
        for (const ra2engine::HarvestEvent& e : world.take_harvest_events()) {
            if (e.kind != ra2engine::HarvestEvent::Kind::UnloadEnd) continue;
            const int hs = (e.unit >= 0 && e.unit < static_cast<int>(world.units().size()))
                               ? world.units()[e.unit].house
                               : -1;
            if (hs == 0 && !cmin_unloaded) { cmin_unloaded = true; cmin_credits = ps.credits; }
            if (hs == 1 && !enemy_unloaded) { enemy_unloaded = true; enemy_income_at = enemy_income; }
        }
        ++frame;
        t += dt;
        draw();
    };

    if (render) {
        draw();
        capture_png(ren, start_png, W, H);
    }

    // t=1.0s: start GAREFN (frames=1260 from raw Cost 2000; charge=600).
    while (frame < 15) step_one();
    ra2engine::start(ps, garefn, ba.roster, build_speed);
    {
        char buf[128];
        std::snprintf(buf, sizeof buf, "[econ] start GAREFN frames=%d step_ticks=%d charge=%d",
                      ps.slot.total_frames, ps.slot.step_ticks, ps.slot.charge_basis);
        r.witness.emplace_back(buf);
    }
    while (ps.slot.type == garefn && !ps.slot.ready && t < cap) step_one();

    // Place east of the ConYard; the queue already spent exactly `charge`.
    int gx = 0, gy = 0;
    const ra2engine::Building cyb2 = (cy_bi >= 0) ? world.buildings()[cy_bi] : ra2engine::Building{};
    if (!place_player_building(world, ba, garefn, cyb2.cx + cyb2.fw, cyb2.cy, gx, gy)) r.ok = false;
    ra2engine::placed(ps);
    {
        const ra2engine::PowerTotals pt = ra2engine::power_totals(world.buildings(), ba.roster, 0);
        char buf[160];
        std::snprintf(buf, sizeof buf, "[econ] placed GAREFN cell=(%d,%d) credits=%d power=+%d/-%d",
                      gx, gy, static_cast<int>(std::llround(ps.credits)), pt.output, pt.drain);
        r.witness.emplace_back(buf);
    }

    // Seed the player ore patch by the GAREFN dock, then spawn the CMIN FreeUnit
    // (GAREFN->CMIN) at the dock -- the refinery's grand opening.
    int cmin_unit = -1;
    const int garefn_bi = find_building(world, garefn, 0);
    if (garefn_bi >= 0) {
        const ra2engine::Building gb = world.buildings()[garefn_bi];
        seed_ore_patch(world, ra2engine::dock_cell_x(gb), ra2engine::dock_cell_y(gb) + 3, 3,
                       ra2engine::kMaxOreDensity);
        bool chrono = false;
        int storage = 0;
        const VehicleAssets* mva = miner_for_id(miners, rules.free_unit("GAREFN"), chrono, storage);
        if (mva)
            cmin_unit = spawn_harvester(world, *mva, chrono, storage, 0,
                                        ra2engine::dock_cell_x(gb), ra2engine::dock_cell_y(gb));
    }
    if (cmin_unit >= 0) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "[econ] CMIN spawned cell=(%d,%d)",
                      world.units()[cmin_unit].cell_x, world.units()[cmin_unit].cell_y);
        r.witness.emplace_back(buf);
    } else {
        r.witness.emplace_back("[econ] CMIN spawn FAILED");
        r.ok = false;
    }

    // GAWEAP now buildable (PROC met by the refinery) -- inverse of demo-base.
    const bool can_weap = ra2engine::can_start(ps, gaweap, ba.roster, world.buildings(), 0,
                                               ba.groups, conyard_alive(world, ba, 0));
    if (can_weap) r.witness.emplace_back("[econ] GAWEAP can_start=yes (PROC met)");
    else { r.witness.emplace_back("[econ] GAWEAP can_start=NO (unexpected)"); r.ok = false; }

    // Run until the player's CMIN completes its first unload (20 bales * 25 = 500).
    while (!cmin_unloaded && t < cap) step_one();
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "[econ] CMIN unloaded credits=%d",
                      static_cast<int>(std::llround(cmin_credits)));
        r.witness.emplace_back(buf);
    }
    if (!cmin_unloaded) r.ok = false;

    // Run until the enemy War Miner completes its first unload (40 bales * 25 = 1000).
    while (!enemy_unloaded && t < cap) step_one();
    {
        char buf[96];
        std::snprintf(buf, sizeof buf, "[econ] enemy income=%d",
                      static_cast<int>(std::llround(enemy_unloaded ? enemy_income_at : enemy_income)));
        r.witness.emplace_back(buf);
    }
    if (!enemy_unloaded) r.ok = false;

    // Growth witness: bracket the first growth firing after t=250 (the ore
    // timer resets when it fires) and report the first ore cell whose density
    // rose across that single tick.
    while (t < 250.0 && t < cap) step_one();
    const ra2engine::OreField& oref = world.ore();
    const int ow = oref.w(), oh = oref.h();
    std::vector<int> before(static_cast<size_t>(ow) * oh, 0);
    bool fired = false;
    int gcx = -1, gcy = -1, gbv = 0, gav = 0;
    while (t < cap && !fired) {
        for (int y = 0; y < oh; ++y)
            for (int x = 0; x < ow; ++x)
                before[static_cast<size_t>(y) * ow + x] = world.ore().density_at(x, y);
        const double prev_timer = world.ore().timer();
        step_one();
        if (world.ore().timer() < prev_timer) {
            fired = true;
            const ra2engine::OreField& of = world.ore();
            for (int y = 0; y < oh && gcx < 0; ++y)
                for (int x = 0; x < ow; ++x) {
                    const int bf = before[static_cast<size_t>(y) * ow + x];
                    const int af = of.density_at(x, y);
                    if (bf > 0 && af > bf) { gcx = x; gcy = y; gbv = bf; gav = af; break; }
                }
        }
    }
    if (gcx >= 0) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "[econ] growth cell=(%d,%d) density=%d->%d", gcx, gcy, gbv,
                      gav);
        r.witness.emplace_back(buf);
    } else {
        r.witness.emplace_back("[econ] growth NONE (no cell rose)");
        r.ok = false;
    }

    // Run out to t=320 (past the pinned growth firing).
    while (t < 320.0 && t < cap) step_one();
    if (t >= cap) r.ok = false;

    r.hash = econ_hash(world, ps.credits, enemy_income);
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "[econ] econ_hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(buf);
    }

    if (render) {
        draw();
        capture_png(ren, end_png, W, H);
    }
    return r;
}

// --demo-econ: the Economy integration gate. Runs simulate_econ rendered
// (hidden window + start/end screenshots through the ore/miner draw path) and
// headless, and byte-compares the two witness logs + econ hashes. Exit 0 iff
// both runs are byte-identical AND both `ok`.
int run_demo_econ() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_econ_start.png";
    const std::string end_png = scratch + "demo_econ_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-econ] FAILED: building assets unavailable\n";
        return 1;
    }
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    if (!miners.harv.ok || !miners.cmin.ok) {
        std::cerr << "[demo-econ] FAILED: miner (HARV/CMIN) assets unavailable\n";
        return 1;
    }
    OreArt ore_art = load_ore_art(ra2);
    const std::vector<const VehicleAssets*> vlist{&miners.harv, &miners.cmin};
    const double build_speed = rules.general_double("BuildSpeed", 0.7);
    const double low_mod = rules.general_double("LowPowerPenaltyModifier", 1.0);
    const int mp_money = rules.mp_money();

    // Camera centered on the player base region (ConYard + dock + seeded ore).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 4, pc->y + 9, 0, 60, 30, 15);
        cam.x = (W - 168) / 2 - p.x;  // playfield left of the sidebar strip
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoEconResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-econ)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_econ(a, ua, ba, miners, rules, build_speed, low_mod, mp_money, vlist,
                               ore_art, /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoEconResult r2 = simulate_econ(a, ua, ba, miners, rules, build_speed, low_mod, mp_money,
                                      vlist, ore_art, /*render=*/false, nullptr, nullptr, nullptr, W,
                                      H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-econ] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (identical ? "yes" : "no") << " lines=" << r1.witness.size()
              << "\n";
    return bad ? 1 : 0;
}

// --- --demo-prod helpers ---------------------------------------------------

// FNV extension of world_hash with each queue's state + spawned unit types +
// credits, for --demo-prod's two-run byte-identity gate. world_hash UNTOUCHED.
uint64_t prod_hash(const ra2engine::World& world, const ra2engine::ProductionQueues& q,
                   double credits) {
    uint64_t h = world_hash(world);
    for (ra2engine::QueueCat c : {ra2engine::QueueCat::Structure, ra2engine::QueueCat::Defense,
                                  ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle}) {
        const ra2engine::ProductionState& ps = q.state(c);
        h = fnv_mix(h, static_cast<uint64_t>(ps.slot.type));
        h = fnv_mix(h, static_cast<uint64_t>(ps.slot.stage));
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(ps.slot.spent)));
    }
    for (const ra2engine::Unit& u : world.units())   // spawned unit types (cells: world_hash)
        for (char ch : u.image) h = fnv_mix(h, static_cast<uint64_t>((unsigned char)ch));
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(credits)));
    return h;
}

struct DemoProdResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the unit-production demo (fixed 15 Hz dt, frame-driven,
// like simulate_econ). Player Allied base + instant GAPOWR/GAPILE/GAWEAP (so
// both unit factories exist and PROC is met). Script + witnesses: queue a
// Grizzly (MTNK) in GAWEAP (frames from raw Cost, charge==Cost, §1.5), set a
// rally + start a GI (E1) concurrently, run the tank to its 8-ring exit cell,
// then add a SECOND war factory + set_primary and prove the NEXT tank exits
// from it AND builds in the recovered MultipleFactory x0.8 of the single-
// factory frames (§1.2). Rendered and headless runs must be byte-identical
// (witness + prod_hash are pure sim state).
DemoProdResult simulate_prod(Assets& a, UnitAssets& ua, BuildingAssets& ba, UnitRoster& ur,
                             const Rules& /*rules*/, double build_speed, double low_mod, int mp_money,
                             const std::vector<const VehicleAssets*>& vlist, const OreArt& ore_art,
                             double mf, bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                             const ra2engine::Camera* cam, int W, int H,
                             const std::string& start_png, const std::string& end_png) {
    DemoProdResult r;
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 6);

    const int gacnst = roster_index(ba, "GACNST");
    const int gapowr = roster_index(ba, "GAPOWR");
    const int gapile = roster_index(ba, "GAPILE");
    const int gaweap = roster_index(ba, "GAWEAP");
    const int cy_bi = find_building(world, gacnst, 0);
    const ra2engine::Building cy_b = cy_bi >= 0 ? world.buildings()[cy_bi] : ra2engine::Building{};
    { int ox = 0, oy = 0;
      if (!place_player_building(world, ba, gapowr, cy_b.cx + cy_b.fw, cy_b.cy, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gapile, cy_b.cx, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gaweap, cy_b.cx + cy_b.fw, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false; }

    const std::vector<int> inf_ft = factory_types_for(ba, "GAPILE");
    const std::vector<int> veh_ft = factory_types_for(ba, "GAWEAP");
    // Unit-roster slots for MTNK (Grizzly) and E1 (GI).
    int mtnk_slot = -1, gi_slot = -1;
    for (int i = 0; i < (int)ur.defs.size(); ++i) {
        if (ur.defs[i].id == "MTNK") mtnk_slot = i;
        if (ur.defs[i].id == "E1") gi_slot = i;
    }
    if (mtnk_slot < 0 || gi_slot < 0) { r.ok = false; r.witness.emplace_back("[prod] roster MISSING MTNK/E1"); }

    ra2engine::ProductionQueues q;
    double credits = mp_money;
    q.set_credits(credits);
    const ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(W, H, sidebar_buildable(ba));

    const double dt = 1.0 / 15.0, cap = 600.0;
    long frame = 0; double t = 0.0;
    auto step_one = [&]() {
        world.step(dt);
        q.set_credits(credits);
        prod_tick(q, world, ba, dt, low_mod, mf, inf_ft, veh_ft);
        credits = q.structure().credits;
        credits += world.take_income(0);
        (void)world.take_harvest_events();
        // Exit handoff (same as run_interactive).
        for (ra2engine::QueueCat cat : {ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle}) {
            if (!q.ready(cat)) continue;
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Infantry) ? inf_ft : veh_ft;
            const int fi = ra2engine::primary_factory(world.buildings(), ft, 0);
            if (fi < 0) continue;
            const int uslot = q.producing(cat);
            if (uslot < 0 || uslot >= (int)ur.defs.size()) { q.placed(cat); continue; }
            const ra2engine::BuildingTypeDef& fdef = ba.roster[world.buildings()[fi].type];
            if (world.launch_unit(fi, fdef, ur.defs[uslot].combat) >= 0) q.placed(cat);
        }
        ++frame; t += dt;
        if (render) { SDL_SetRenderDrawColor(ren, 10, 20, 60, 255); SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
            if (ba.sidebar_ok) draw_sidebar(ren, *tc, ba, lay, q.structure(), world,
                                            static_cast<int>(std::llround(credits)), t, 0); }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }

    // W1: queue a Grizzly in the Vehicle queue (frames from raw cost, charge==cost).
    while (frame < 15) step_one();
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Vehicle, mtnk_slot, ur.defs, build_speed);
    { char b[128]; std::snprintf(b, sizeof b, "[prod] start MTNK frames=%d step_ticks=%d charge=%d",
        q.state(ra2engine::QueueCat::Vehicle).slot.total_frames,
        q.state(ra2engine::QueueCat::Vehicle).slot.step_ticks,
        q.state(ra2engine::QueueCat::Vehicle).slot.charge_basis); r.witness.emplace_back(b); }
    const int mtnk_step_ticks = q.state(ra2engine::QueueCat::Vehicle).slot.step_ticks;

    // MultipleFactory speed multiplier with a SINGLE war factory (baseline x1.0).
    const double sp1 = factory_speedup(world, veh_ft, mf);

    // 8-ring exit cell the tank WILL take, captured now while the door is free
    // (recomputing after it spawns would walk past the now-occupied door).
    // Match launch_unit's engine-side predicate EXACTLY: occupancy().is_free
    // sees committed cells AND mid-hop next-cell reservations (unit_at_cell does
    // not), so the demo's expected exit cell tracks what the engine will pick.
    auto free_cell = [&](int x, int y) { return world.walkable(x, y) && world.occupancy().is_free(x, y); };
    // Set a rally for the war factory before the tank completes.
    const int weap_bi = find_building(world, gaweap, 0);
    ra2engine::Cellxy expected_exit{-1, -1};
    if (weap_bi >= 0) {
        const ra2engine::Building wb = world.buildings()[weap_bi];
        world.set_rally(weap_bi, wb.cx + wb.fw + 5, wb.cy + wb.fh);
        expected_exit = ra2engine::exit_cell(wb, ba.roster[wb.type], free_cell);
    }
    // Also queue a GI concurrently -> witness both queues advancing.
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Infantry, gi_slot, ur.defs, build_speed);
    { char b[96]; std::snprintf(b, sizeof b, "[prod] concurrent GI stage=%d MTNK stage=%d",
        q.state(ra2engine::QueueCat::Infantry).slot.stage,
        q.state(ra2engine::QueueCat::Vehicle).slot.stage); r.witness.emplace_back(b); }

    const int units_before = (int)world.units().size();
    const long f_single0 = frame;
    while (q.producing(ra2engine::QueueCat::Vehicle) != -1 && t < cap) step_one();  // to exit
    const long single_frames = frame - f_single0;
    // The freshly launched Grizzly is the last unit; witness its exit cell + rally path.
    if ((int)world.units().size() > units_before) {
        const ra2engine::Unit& tank = world.units().back();
        const ra2engine::Cellxy ex = expected_exit;
        char b[160]; std::snprintf(b, sizeof b,
            "[prod] MTNK exit cell=(%d,%d) expected=(%d,%d) moving=%d credits=%d",
            tank.cell_x, tank.cell_y, ex.x, ex.y, tank.moving() ? 1 : 0,
            static_cast<int>(std::llround(credits)));
        r.witness.emplace_back(b);
        if (tank.cell_x != ex.x || tank.cell_y != ex.y) r.ok = false;
    } else { r.witness.emplace_back("[prod] MTNK never exited"); r.ok = false; }

    // Second war factory + set_primary -> the NEXT tank exits from it.
    int gx = 0, gy = 0;
    // Guard the 2nd GAWEAP placement like the first three: a failure (broken
    // install) would leave w2=-1, skip set_primary, fall expected_exit2 back to
    // factory 1, and let the "2nd MTNK from primary" check re-test factory 1
    // against itself (match=1) -- a false pass. Fail the gate + diagnose.
    if (!place_player_building(world, ba, gaweap, cy_b.cx + cy_b.fw + 4, cy_b.cy + cy_b.fh, gx, gy)) {
        r.ok = false; r.witness.emplace_back("[prod] 2nd GAWEAP placement FAILED");
    }
    int w2 = -1;   // locate the SECOND war factory (any alive GAWEAP != weap_bi)
    for (int i = 0; i < (int)world.buildings().size(); ++i)
        if (world.buildings()[i].alive && world.buildings()[i].type == gaweap && i != weap_bi) w2 = i;
    if (w2 >= 0) world.set_primary(w2, veh_ft);
    const double sp2 = factory_speedup(world, veh_ft, mf);   // TWO war factories -> x1/mf
    { char b[96]; std::snprintf(b, sizeof b, "[prod] primary factory now idx=%d (2nd war factory=%d)",
        ra2engine::primary_factory(world.buildings(), veh_ft, 0), w2); r.witness.emplace_back(b); }
    { char b[96]; std::snprintf(b, sizeof b, "[prod] MF veh speedup 1fac=%.3f 2fac=%.3f",
        sp1, sp2); r.witness.emplace_back(b); }
    if (std::llround(sp1 * 1000) != 1000 || std::llround(sp2 * 1000) != std::llround(1000.0 / mf))
        r.ok = false;   // recovered MultipleFactory 0.8: 1fac=1.000, 2fac=1.250 EXACTLY

    // Exit cell of the NEW primary (2nd war factory), captured before the build.
    // Guard the index: if BOTH w2 and weap_bi are -1 (all GAWEAP placements
    // failed on a broken install), dereferencing world.buildings()[-1] is UB.
    // Report the verdict and skip the build rather than execute UB.
    const int primary_bi = (w2 >= 0) ? w2 : weap_bi;
    long double_frames = 0;
    if (primary_bi < 0) {
        r.ok = false; r.witness.emplace_back("[prod] 2nd MTNK no war factory (all placements failed)");
    } else {
    const ra2engine::Building w2b = world.buildings()[primary_bi];
    const ra2engine::Cellxy expected_exit2 = ra2engine::exit_cell(w2b, ba.roster[w2b.type], free_cell);
    const int u2_before = (int)world.units().size();
    q.set_credits(credits);
    q.start_unit(ra2engine::QueueCat::Vehicle, mtnk_slot, ur.defs, build_speed);
    const long f_double0 = frame;
    while (q.producing(ra2engine::QueueCat::Vehicle) != -1 && t < cap) step_one();
    double_frames = frame - f_double0;
    if ((int)world.units().size() > u2_before) {
        const ra2engine::Unit& tank2 = world.units().back();
        const ra2engine::Cellxy ex2 = expected_exit2;
        char b[160]; std::snprintf(b, sizeof b,
            "[prod] 2nd MTNK exit cell=(%d,%d) from primary=(%d,%d) match=%d",
            tank2.cell_x, tank2.cell_y, ex2.x, ex2.y,
            (tank2.cell_x == ex2.x && tank2.cell_y == ex2.y) ? 1 : 0); r.witness.emplace_back(b);
        if (tank2.cell_x != ex2.x || tank2.cell_y != ex2.y) r.ok = false;
    } else { r.witness.emplace_back("[prod] 2nd MTNK never exited"); r.ok = false; }
    }   // end guarded 2nd-factory build (primary_bi >= 0)
    // PIN the MultipleFactory x0.8: the two-factory build must take EXACTLY the
    // recovered fraction of the single-factory frames (not merely "faster").
    // The 54-stage timer is free-running, so completion = ceil(54*step_ticks /
    // (pmul*speedup)); with pmul constant across both builds the double-factory
    // count is ceil(single / (sp2/sp1)) == ceil(single * mf) EXACTLY.
    const long expected_double = static_cast<long>(std::ceil(single_frames * mf));
    { char b[160]; std::snprintf(b, sizeof b,
        "[prod] MTNK build_frames 1fac=%ld 2fac=%ld expected2=%ld step_ticks=%d",
        single_frames, double_frames, expected_double, mtnk_step_ticks); r.witness.emplace_back(b); }
    if (double_frames != expected_double) r.ok = false;

    // Finish the GI queue too, so both categories produced.
    while (q.producing(ra2engine::QueueCat::Infantry) != -1 && t < cap) step_one();
    for (int k = 0; k < 30; ++k) step_one();   // settle motion

    r.hash = prod_hash(world, q, credits);
    { char b[64]; std::snprintf(b, sizeof b, "[prod] prod_hash=%016llx",
        static_cast<unsigned long long>(r.hash)); r.witness.emplace_back(b); }
    if (t >= cap) r.ok = false;
    if (render) { step_one(); capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-prod: the Unit-Production integration gate. Runs simulate_prod
// rendered (hidden window + start/end screenshots) then headless, and
// byte-compares the two witness logs + prod hashes. Exit 0 iff both runs are
// byte-identical AND both `ok`.
int run_demo_prod() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_prod_start.png";
    const std::string end_png = scratch + "demo_prod_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-prod] FAILED: building assets unavailable\n";
        return 1;
    }
    UnitRoster ur = load_unit_roster(dir, ra2, ua, rules, art);
    bool have_mtnk = false, have_gi = false;
    for (const ra2engine::UnitTypeDef& d : ur.defs) {
        if (d.id == "MTNK") have_mtnk = true;
        if (d.id == "E1") have_gi = true;
    }
    if (!have_mtnk || !have_gi) {
        std::cerr << "[demo-prod] FAILED: unit roster missing MTNK/E1 (both must be producible)\n";
        return 1;
    }
    OreArt ore_art = load_ore_art(ra2);
    // Render vlist = the roster's vehicle voxels, so launched MTNKs draw as voxels.
    std::vector<const VehicleAssets*> vlist;
    for (const VehicleAssets& v : ur.vehicles) vlist.push_back(&v);
    const double build_speed = rules.general_double("BuildSpeed", 0.7);
    const double low_mod = rules.general_double("LowPowerPenaltyModifier", 1.0);
    const int mp_money = rules.mp_money();
    const double mf = rules.general_double("MultipleFactory", 0.8);

    // Camera centered on the player base region (ConYard + unit factories).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 4, pc->y + 9, 0, 60, 30, 15);
        cam.x = (W - 168) / 2 - p.x;  // playfield left of the sidebar strip
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoProdResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-prod)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_prod(a, ua, ba, ur, rules, build_speed, low_mod, mp_money, vlist,
                               ore_art, mf, /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoProdResult r2 = simulate_prod(a, ua, ba, ur, rules, build_speed, low_mod, mp_money, vlist,
                                      ore_art, mf, /*render=*/false, nullptr, nullptr, nullptr, W, H,
                                      "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-prod] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (identical ? "yes" : "no") << " lines=" << r1.witness.size()
              << "\n";
    return bad ? 1 : 0;
}

// FNV fold of the tech-tree phase state [spec §6]: every unit's + building's
// (cell/alive/hp/state) PLUS the BUILDABLE-SET snapshot -- can_start_unit for
// {MTNK, SREF} and can_start for {GATECH, GAWEAP} at a fixed idle probe queue.
// This is exactly the tech-tree state the phase adds. A NEW accumulator;
// world_hash and every prior *_hash UNTOUCHED. udefs is the probe unit roster
// (ur.defs, or ur.defs + a synthetic SREF when SREF art is absent, §6).
uint64_t techtree_fold(uint64_t h, const ra2engine::World& world, const BuildingAssets& ba,
                       const std::vector<ra2engine::UnitTypeDef>& udefs, int mtnk_slot,
                       int sref_slot, int gatech_type, int gaweap_type, bool conyard_ok,
                       bool veh_factory_alive) {
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
    }
    for (const ra2engine::Building& b : world.buildings()) {
        h = fnv_mix(h, static_cast<uint64_t>(b.type));
        h = fnv_mix(h, static_cast<uint64_t>(b.cx));
        h = fnv_mix(h, static_cast<uint64_t>(b.cy));
        h = fnv_mix(h, static_cast<uint64_t>(b.hp));
        h = fnv_mix(h, static_cast<uint64_t>(b.state));
    }
    const ra2engine::ProductionState idle;   // slot.type == -1 (a fresh idle queue)
    auto fold_unit = [&](int slot) {
        const bool ok = slot >= 0 &&
            ra2engine::can_start_unit(idle, slot, udefs, world.buildings(), ba.roster, 0,
                                      ba.groups, veh_factory_alive);
        h = fnv_mix(h, static_cast<uint64_t>(ok ? 1 : 0));
    };
    auto fold_bldg = [&](int type) {
        const bool ok = type >= 0 &&
            ra2engine::can_start(idle, type, ba.roster, world.buildings(), 0, ba.groups, conyard_ok);
        h = fnv_mix(h, static_cast<uint64_t>(ok ? 1 : 0));
    };
    fold_unit(mtnk_slot);    // MTNK (tier-2 control)
    fold_unit(sref_slot);    // SREF (tier-3, the gated unit)
    fold_bldg(gatech_type);  // GATECH (the tech building)
    fold_bldg(gaweap_type);  // GAWEAP (the factory)
    return h;
}

struct DemoTechtreeResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the tech-tree demo (fixed 15 Hz dt, frame-driven, like
// simulate_prod). Build a fresh Allied base (GACNST/GAPOWR/GAPILE/GAWEAP, NO
// radar), resolve the probe roster with a synthetic-SREF fallback, fold the
// buildable-set at BASELINE -> UNLOCK (place GATECH DIRECTLY, bypassing its
// radar-gated Prerequisite) -> END. Wedge guards: SREF UNBUILDABLE before GATECH
// AND BUILDABLE after. Rendered and headless runs must be byte-identical.
DemoTechtreeResult simulate_techtree(Assets& a, UnitAssets& ua, BuildingAssets& ba, UnitRoster& ur,
                                     const Rules& rules,
                                     const std::vector<const VehicleAssets*>& vlist, bool render,
                                     SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                     const ra2engine::Camera* cam, int W, int H,
                                     const std::string& start_png, const std::string& end_png) {
    DemoTechtreeResult r;
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 6);

    const int gacnst = roster_index(ba, "GACNST");
    const int gapowr = roster_index(ba, "GAPOWR");
    const int gapile = roster_index(ba, "GAPILE");
    const int gaweap = roster_index(ba, "GAWEAP");
    const int gatech = roster_index(ba, "GATECH");   // always present (buildings never dropped;
                                                     // art.ok may be false [RE-FALLBACK] but the
                                                     // roster entry + placement are art-independent)
    const ra2engine::Building cy_b =
        [&]{ int i = find_building(world, gacnst, 0);
             return i >= 0 ? world.buildings()[i] : ra2engine::Building{}; }();
    { int ox = 0, oy = 0;
      if (!place_player_building(world, ba, gapowr, cy_b.cx + cy_b.fw, cy_b.cy, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gapile, cy_b.cx, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false;
      if (!place_player_building(world, ba, gaweap, cy_b.cx + cy_b.fw, cy_b.cy + cy_b.fh, ox, oy)) r.ok = false; }

    // Probe unit roster: ur.defs, plus a SYNTHETIC SREF if its art was dropped
    // (spec §6 -- the sim/predicate is art-independent). MTNK is always present.
    std::vector<ra2engine::UnitTypeDef> udefs = ur.defs;
    int mtnk_slot = -1, sref_slot = -1;
    for (int i = 0; i < (int)udefs.size(); ++i) {
        if (udefs[i].id == "MTNK") mtnk_slot = i;
        if (udefs[i].id == "SREF") sref_slot = i;
    }
    bool sref_synth = false;
    if (sref_slot < 0) {                              // SREF art absent -> synthesize the probe def
        ra2engine::UnitTypeDef s;
        s.id = "SREF";
        s.is_vehicle = true;
        s.cost = std::max(1, rules.cost("SREF"));
        s.tech_level = std::max(1, rules.tech_level("SREF"));
        s.prerequisite = rules.prerequisite("SREF");
        if (s.prerequisite.empty()) s.prerequisite = {"GAWEAP", "GATECH"};
        udefs.push_back(std::move(s));
        sref_slot = (int)udefs.size() - 1;
        sref_synth = true;
    }
    if (mtnk_slot < 0) { r.ok = false; r.witness.emplace_back("[techtree] roster MISSING MTNK"); }

    const std::vector<int> veh_ft = factory_types_for(ba, "GAWEAP");
    auto veh_alive = [&]{ return !world.factory_category_lost(veh_ft, 0); };
    auto cy_ok = [&]{ return conyard_alive(world, ba, 0); };
    auto sref_buildable = [&]{
        const ra2engine::ProductionState idle;
        return ra2engine::can_start_unit(idle, sref_slot, udefs, world.buildings(), ba.roster, 0,
                                         ba.groups, veh_alive());
    };
    auto mtnk_buildable = [&]{
        const ra2engine::ProductionState idle;
        return mtnk_slot >= 0 &&
               ra2engine::can_start_unit(idle, mtnk_slot, udefs, world.buildings(), ba.roster, 0,
                                         ba.groups, veh_alive());
    };

    const ra2engine::SidebarLayout lay =
        ra2engine::SidebarLayout::compute(W, H, sidebar_buildable(ba));
    const double dt = 1.0 / 15.0;
    long frame = 0; double t = 0.0;
    uint64_t h = 0xcbf29ce484222325ull;              // FNV-1a offset basis (world_hash's)
    auto ps_idle = []{ return ra2engine::ProductionState{}; };
    auto step_one = [&]() {
        world.step(dt); ++frame; t += dt;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255); SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba, nullptr);
            if (ba.sidebar_ok) draw_sidebar(ren, *tc, ba, lay, ps_idle(), world, 10000, t, 0);
        }
    };

    {
        char b[160];
        std::snprintf(b, sizeof b, "[techtree] roster=%d sref_slot=%d sref_synth=%s gatech_idx=%d",
                      (int)ba.roster.size(), sref_slot, sref_synth ? "yes" : "no", gatech);
        r.witness.emplace_back(b);
    }
    if (render) { step_one(); capture_png(ren, start_png, W, H); }

    // 1) BASELINE: {GACNST,GAPOWR,GAPILE,GAWEAP} owned -> SREF UNBUILDABLE (GATECH
    //    unmet), MTNK BUILDABLE (the tier-2 control -- the base is productive).
    while (frame < 15) step_one();
    const bool base_sref = sref_buildable();
    const bool base_mtnk = mtnk_buildable();
    h = techtree_fold(h, world, ba, udefs, mtnk_slot, sref_slot, gatech, gaweap, cy_ok(),
                      veh_alive());
    {
        char b[160];
        std::snprintf(b, sizeof b, "[techtree] SREF %s (GATECH unmet) can_start=%d ; MTNK can_start=%d",
                      base_sref ? "buildable" : "unbuildable", base_sref ? 1 : 0, base_mtnk ? 1 : 0);
        r.witness.emplace_back(b);
    }

    // 2) UNLOCK: place the Battle Lab (instant-Idle via place_player_building) so
    //    the TECH group is satisfied. [RE-GATE APPLIED: techtree.md §4] DIRECT
    //    placement: place_player_building does NOT go through the buildability
    //    predicate (can_start), so GATECH's own radar-gated Prerequisite=GAWEAP,
    //    RADAR,GACNST is bypassed and the demo base needs NO radar (the base above
    //    places only GACNST/GAPOWR/GAPILE/GAWEAP). GATECH art may be absent
    //    [RE-FALLBACK] -> it renders as the placeholder; placement + the TECH-group
    //    satisfaction are art-independent. (If some future change makes placement
    //    itself gated, add a RADAR-group building -- GAAIRC/NARADR/AMRADR/NAPSIS --
    //    to the base so GATECH is placeable.)
    int gx = 0, gy = 0;
    const bool placed = place_player_building(world, ba, gatech, cy_b.cx, cy_b.cy - 1, gx, gy);
    if (!placed) { r.ok = false; r.witness.emplace_back("[techtree] FAILED to place GATECH"); }
    while (frame < 30) step_one();                   // settle frames (already Idle)
    {
        char b[128];
        std::snprintf(b, sizeof b, "[techtree] placed GATECH cell=(%d,%d) TECH group satisfied",
                      gx, gy);
        r.witness.emplace_back(b);
    }

    // 3) BUILDABLE: SREF now BUILDABLE (GATECH met).
    const bool unlocked_sref = sref_buildable();
    h = techtree_fold(h, world, ba, udefs, mtnk_slot, sref_slot, gatech, gaweap, cy_ok(),
                      veh_alive());
    {
        char b[128];
        std::snprintf(b, sizeof b, "[techtree] SREF %s (GATECH met) can_start=%d",
                      unlocked_sref ? "buildable" : "unbuildable", unlocked_sref ? 1 : 0);
        r.witness.emplace_back(b);
    }

    // 4) END fold (a few more settle frames; state is stable).
    while (frame < 45) step_one();
    h = techtree_fold(h, world, ba, udefs, mtnk_slot, sref_slot, gatech, gaweap, cy_ok(),
                      veh_alive());
    r.hash = h;
    { char b[64]; std::snprintf(b, sizeof b, "[techtree] techtree_hash=%016llx",
                                static_cast<unsigned long long>(r.hash)); r.witness.emplace_back(b); }

    // Wedge guards [spec §6]: the gate can never pass vacuously -- SREF must be
    // UNBUILDABLE at baseline AND BUILDABLE after GATECH is placed. Both hold or exit 1.
    if (base_sref)      { r.ok = false; r.witness.emplace_back("[techtree] WEDGE: SREF buildable BEFORE GATECH"); }
    if (!unlocked_sref) { r.ok = false; r.witness.emplace_back("[techtree] WEDGE: SREF NOT buildable after GATECH"); }
    if (!base_mtnk)     { r.ok = false; r.witness.emplace_back("[techtree] WEDGE: MTNK not buildable at baseline"); }
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-techtree: the CB-5 tech-tree integration gate (13th permanent
// byte-identity gate). Runs simulate_techtree rendered (hidden window +
// start/end screenshots) then headless, and byte-compares the two witness logs
// + techtree hashes. NO hardcoded hash literal -- deterministic self-compare.
// Exit 0 iff both runs are byte-identical AND both `ok`.
int run_demo_techtree() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_techtree_start.png";
    const std::string end_png = scratch + "demo_techtree_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) { std::cerr << "[demo-techtree] FAILED: building assets\n"; return 1; }
    UnitRoster ur = load_unit_roster(dir, ra2, ua, rules, art);
    std::vector<const VehicleAssets*> vlist;
    for (const VehicleAssets& va : ur.vehicles) vlist.push_back(&va);

    DemoTechtreeResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-techtree)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_techtree(a, ua, ba, ur, rules, vlist, /*render=*/true, ren, &tc, &cam,
                                   W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    }

    DemoTechtreeResult r2 = simulate_techtree(a, ua, ba, ur, rules, vlist, /*render=*/false,
                                              nullptr, nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;
    std::cerr << "[demo-techtree] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d techtree_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// FNV hash of the AI-1 end state for --demo-ai-base's two-run byte-identity
// gate [RE: ai-base.md]: the SORTED alive-building list [type, cell, hp] +
// credits + power totals + the planned-node list. A NEW function; world_hash
// / econ_hash / prod_hash UNTOUCHED.
uint64_t ai_base_hash(const ra2engine::World& world, int house,
                      const std::vector<ra2engine::BuildingTypeDef>& roster) {
    uint64_t h = 1469598103934665603ULL;  // FNV offset basis
    std::vector<std::array<int, 4>> rows;
    for (const ra2engine::Building& b : world.buildings()) {
        if (!b.alive || b.house != house) continue;
        rows.push_back({b.type, b.cx, b.cy, b.hp});
    }
    std::sort(rows.begin(), rows.end());
    for (const auto& r : rows)
        for (int x : r) h = fnv_mix(h, static_cast<uint64_t>(x));
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(world.ai_credits(house))));
    const ra2engine::PowerTotals pt =
        ra2engine::power_totals(world.buildings(), roster, house);
    h = fnv_mix(h, static_cast<uint64_t>(pt.output));
    h = fnv_mix(h, static_cast<uint64_t>(pt.drain));
    if (const ra2engine::AiBaseState* st = world.ai_state(house)) {
        for (const ra2engine::PlannedNode& n : st->plan) {
            h = fnv_mix(h, static_cast<uint64_t>(n.type));
            h = fnv_mix(h, static_cast<uint64_t>(n.cx));
            h = fnv_mix(h, static_cast<uint64_t>(n.cy));
            h = fnv_mix(h, static_cast<uint64_t>(n.dead ? 1 : 0));
            h = fnv_mix(h, static_cast<uint64_t>(n.building >= 0 ? 1 : 0));
        }
    }
    return h;
}

struct DemoAiBaseResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the AI-base demo (fixed 15 Hz dt, frame-driven, rendered
// + headless byte-identical). Spawns a LONE Soviet ConYard (NACNST) for house
// 1, AI-flags it at a FIXED difficulty / credits / seed, and lets the existing
// base-planning machinery build out over kAiBaseFrames. Witnesses record queue
// starts + placements; ai_base_hash pins the end state. This is the ONLY
// headless path that flags an AI house (intentional -- it IS the AI gate).
DemoAiBaseResult simulate_ai_base(Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                  MinerAssets& miners, const Rules& rules,
                                  const std::vector<const VehicleAssets*>& vlist,
                                  const OreArt& ore_art, bool render, SDL_Renderer* ren,
                                  ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                  int W, int H, const std::string& start_png,
                                  const std::string& end_png) {
    DemoAiBaseResult r;
    ra2engine::World world = build_world(a, ua, 0);
    wire_economy(world, ba, rules);  // ore + harvest stepping for the AI's income
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    // Lone Soviet ConYard, house 1, validity-bypassed like spawn_base's
    // ConYard branch (the very first building has no neighbors).
    const int nacnst = roster_index(ba, "NACNST");
    if (nacnst < 0 || !ba.art[nacnst].ok) { r.ok = false; return r; }
    {
        const ra2engine::BuildingTypeDef& d = ba.roster[nacnst];
        const ra2engine::PlacementCheck chk = ra2engine::placement_check(
            pc->x, pc->y, d.fw, d.fh, /*house=*/1, d.adjacent, world.passable(),
            world.grid_w(), world.grid_h(), world.occupancy(), world.buildings(),
            /*bypass=*/true);
        if (!chk.ok) { r.ok = false; r.witness.emplace_back("[ai-base] ConYard placement FAILED"); return r; }
        world.add_building(nacnst, pc->x, pc->y, d.fw, d.fh, 1, d.strength, d.armor_ix,
                           ba.art[nacnst].buildup_total, /*instant=*/true, d.sight,
                           d.grants_map_reveal);
    }

    // Fixed difficulty + fixed credits + fixed seed (AiBaseState's rng seed
    // is a compile-time constant) -> fully deterministic [RE: ai-base.md].
    world.enable_ai_house(1, make_ai_config(ba, rules, ra2engine::AiDifficulty::Normal,
                                            (double)rules.mp_money()));

    std::vector<int> bstates;  // FreeUnit grand-opening tracker (AI refinery miner)
    const double dt = 1.0 / 15.0;
    const long kAiBaseFrames = 5400;  // fixed run length: 6 min at 15 Hz
    long frame = 0;
    std::size_t known_buildings = world.buildings().size();
    int last_prod[2] = {-1, -1};

    auto step_one = [&]() {
        world.step(dt);
        spawn_free_units_on_open(world, ba, rules, miners, bstates);
        // Witness: queue starts (build-sequence authenticity eyeball).
        for (int q = 0; q < 2; ++q) {
            const int now = world.ai_producing(1, q);
            if (now != -1 && now != last_prod[q]) {
                char b[128];
                std::snprintf(b, sizeof b, "[ai-base] start %s queue=%s frame=%ld",
                              ba.roster[now].id.c_str(), q == 0 ? "structure" : "defense",
                              frame);
                r.witness.emplace_back(b);
            }
            last_prod[q] = now;
        }
        // Witness: placements.
        while (known_buildings < world.buildings().size()) {
            const ra2engine::Building& nb = world.buildings()[known_buildings];
            char b[160];
            std::snprintf(b, sizeof b,
                          "[ai-base] placed %s at (%d,%d) frame=%ld credits=%d",
                          ba.roster[nb.type].id.c_str(), nb.cx, nb.cy, frame,
                          (int)std::llround(world.ai_credits(1)));
            r.witness.emplace_back(b);
            ++known_buildings;
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
        }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < kAiBaseFrames) step_one();

    const ra2engine::PowerTotals pt =
        ra2engine::power_totals(world.buildings(), ba.roster, 1);
    {
        char b[160];
        std::snprintf(b, sizeof b,
                      "[ai-base] final buildings=%d credits=%d power=%d/%d plan=%d",
                      (int)known_buildings, (int)std::llround(world.ai_credits(1)),
                      pt.output, pt.drain,
                      world.ai_state(1) ? (int)world.ai_state(1)->plan.size() : -1);
        r.witness.emplace_back(b);
    }
    r.hash = ai_base_hash(world, 1, ba.roster);
    {
        char b[64];
        std::snprintf(b, sizeof b, "[ai-base] ai_base_hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(b);
    }
    // Sanity: the AI actually built out (>= power + barracks + war factory,
    // the recovered plan head [RE: ai-base.md §2]).
    if (known_buildings < 4) { r.ok = false; r.witness.emplace_back("[ai-base] built too little"); }
    if (render) { step_one(); capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-ai-base: the AI-1 base-planning integration gate (6th permanent
// byte-identity gate). Runs simulate_ai_base rendered (hidden window +
// start/end screenshots) then headless from a FRESH World, and byte-compares
// the two witness logs + ai_base hashes. Exit 0 iff both runs are byte-
// identical AND both `ok`.
int run_demo_ai_base() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_ai_base_start.png";
    const std::string end_png = scratch + "demo_ai_base_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-ai-base] FAILED: building assets unavailable\n";
        return 1;
    }
    const int nacnst = roster_index(ba, "NACNST");
    if (nacnst < 0 || !ba.art[nacnst].ok) {
        std::cerr << "[demo-ai-base] FAILED: NACNST (Soviet ConYard) unavailable\n";
        return 1;
    }
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    OreArt ore_art = load_ore_art(ra2);
    // Render vlist = the War Miner voxel, so the AI's harvester draws.
    std::vector<const VehicleAssets*> vlist;
    if (miners.harv.ok) vlist.push_back(&miners.harv);

    // Camera centered on the enemy ConYard region (same math as run_demo_prod).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 4, pc->y + 6, 0, 60, 30, 15);
        cam.x = (W - 168) / 2 - p.x;  // playfield left of the sidebar strip
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoAiBaseResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-ai-base)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_ai_base(a, ua, ba, miners, rules, vlist, ore_art,
                                  /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoAiBaseResult r2 = simulate_ai_base(a, ua, ba, miners, rules, vlist, ore_art,
                                           /*render=*/false, nullptr, nullptr, nullptr, W, H,
                                           "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-ai-base] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    std::cout << "deterministic=" << (identical ? "yes" : "no") << " lines=" << r1.witness.size()
              << "\n";
    return bad ? 1 : 0;
}

// FNV hash of the AI-2 end state for --demo-ai-team's two-run byte-identity
// gate [RE: ai-team.md]: the SORTED live-team rows {teamtype, state, pc,
// member count} + each team's SORTED member {cell, hp} rows + the
// produced-unit list (house units: image chars, cell, hp, alive) + credits +
// teams_created. A NEW function; world_hash/econ_hash/prod_hash/ai_base_hash
// UNTOUCHED.
uint64_t ai_team_hash(const ra2engine::World& world, int house) {
    uint64_t h = 1469598103934665603ULL;  // FNV offset basis
    if (const ra2engine::AiTeamState* st = world.ai_team_state(house)) {
        h = fnv_mix(h, static_cast<uint64_t>(st->teams_created));
        std::vector<std::array<int, 4>> rows;
        for (const ra2engine::TeamInstance& t : st->teams)
            rows.push_back({t.teamtype, (int)t.state, t.script_pc, (int)t.members.size()});
        std::sort(rows.begin(), rows.end());
        for (const auto& r : rows)
            for (int x : r) h = fnv_mix(h, static_cast<uint64_t>(x));
        for (const ra2engine::TeamInstance& t : st->teams) {
            std::vector<std::array<int, 3>> mem;
            for (int m : t.members) {
                if (m < 0 || m >= (int)world.units().size()) continue;
                const ra2engine::Unit& u = world.units()[m];
                mem.push_back({u.cell_x, u.cell_y, u.hp});
            }
            std::sort(mem.begin(), mem.end());
            for (const auto& r : mem)
                for (int x : r) h = fnv_mix(h, static_cast<uint64_t>(x));
        }
    }
    for (const ra2engine::Unit& u : world.units()) {   // produced-unit list
        if (u.house != house) continue;
        for (char ch : u.image) h = fnv_mix(h, static_cast<uint64_t>((unsigned char)ch));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
    }
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(world.ai_credits(house))));
    return h;
}

// FNV hash of the AI-3 end state for --demo-ai-trigger's two-run byte-identity
// gate [RE: ai-trigger.md; spec §5]: per-trigger CURRENT weights
// (trigger-table order, x1000 fixed-point) + triggers_fired/adjustments + the
// created-team list {teamtype, state, member count} (creation/live order) +
// credits. A NEW function; world_hash/econ_hash/prod_hash/ai_base_hash/
// ai_team_hash UNTOUCHED.
uint64_t ai_trigger_hash(const ra2engine::World& world, int house) {
    uint64_t h = 1469598103934665603ULL;  // FNV offset basis
    if (const ra2engine::AiTriggerState* ts = world.ai_trigger_state(house)) {
        for (double w : ts->weights)
            h = fnv_mix(h, static_cast<uint64_t>(std::llround(w * 1000.0)));
        h = fnv_mix(h, static_cast<uint64_t>(ts->triggers_fired));
        h = fnv_mix(h, static_cast<uint64_t>(ts->adjustments));
    }
    if (const ra2engine::AiTeamState* st = world.ai_team_state(house)) {
        h = fnv_mix(h, static_cast<uint64_t>(st->teams_created));
        for (const ra2engine::TeamInstance& t : st->teams) {
            h = fnv_mix(h, static_cast<uint64_t>(t.teamtype));
            h = fnv_mix(h, static_cast<uint64_t>((int)t.state));
            h = fnv_mix(h, static_cast<uint64_t>(t.members.size()));
        }
    }
    h = fnv_mix(h, static_cast<uint64_t>(std::llround(world.ai_credits(house))));
    return h;
}

struct DemoAiTeamResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the AI-team demo (fixed 15 Hz dt, frame-driven, rendered
// + headless byte-identical). Spawns a FULL Soviet base (house 1, refinery +
// War Miner, barracks & war factory standing) + a small player target base
// (house 0), AI-flags house 1 at FIXED difficulty / credits / seed with teams
// ENABLED from the REAL parsed aimd.ini (Soviet-roster-scoped via
// make_ai_team_config -- the same path the interactive session uses), and lets
// the offensive-team brain form/recruit/produce/gather/execute over the fixed
// kAiTeamFrames run. Witnesses record team lifecycle transitions + the FIRST
// attack order at the player; ai_team_hash pins the end state. This demo (and
// the interactive session) are the ONLY producers of team.enabled=true.
DemoAiTeamResult simulate_ai_team(Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                  MinerAssets& miners, const Rules& rules,
                                  const std::vector<const VehicleAssets*>& vlist,
                                  const OreArt& ore_art, const ra2assets::Ini& aimd_ini,
                                  const UnitRoster& ur_ai, bool render, SDL_Renderer* ren,
                                  ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                  int W, int H, const std::string& start_png,
                                  const std::string& end_png) {
    DemoAiTeamResult r;
    ra2engine::World world = build_world(a, ua, 0);
    wire_economy(world, ba, rules);  // ore + harvest stepping for the AI's income
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    // Bases: enemy Soviet FULL base (house 1, with refinery + miner) +
    // player target base (house 0) -- the interactive layout, fixed cells.
    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 4);
    spawn_base(world, ba, /*house=*/1, pc->x + 14, pc->y, /*with_refinery=*/true,
               miners.harv.ok ? &miners.harv : nullptr, /*harv_chrono=*/false,
               miners.harv_storage);
    ra2engine::AiHouseConfig acfg =
        make_ai_config(ba, rules, ra2engine::AiDifficulty::Normal,
                       (double)rules.mp_money());
    acfg.team = make_ai_team_config(ba, ur_ai, rules, aimd_ini,
                                    ra2engine::AiDifficulty::Normal,
                                    pc->x, pc->y + 4);
    world.enable_ai_house(1, acfg);
    if (acfg.team.aimd.teams.empty()) {
        r.ok = false;
        r.witness.emplace_back("[ai-team] NO teams parsed from aimd.ini");
        return r;
    }

    std::vector<int> bstates;  // FreeUnit grand-opening tracker (AI refinery miner)
    const double dt = 1.0 / 15.0;
    const long kAiTeamFrames = 5400;   // fixed run: 6 min at 15 Hz -- covers the
                                       // Normal TeamDelays check pass (2500
                                       // frames) + AutocreateTime spawn (900
                                       // frames) + recruit/produce/gather/march
    long frame = 0;
    // Witness trackers: team lifecycle transitions + first attack order.
    int last_created = 0;
    std::vector<std::array<int, 3>> last_rows;   // {teamtype, state, pc} per live team
    bool attack_seen = false;

    auto step_one = [&]() {
        world.step(dt);
        spawn_free_units_on_open(world, ba, rules, miners, bstates);
        const ra2engine::AiTeamState* st = world.ai_team_state(1);
        if (st) {
            if (st->teams_created > last_created) {
                char b[160];
                std::snprintf(b, sizeof b, "[ai-team] created teamtype=%s frame=%ld",
                              acfg.team.aimd.teams[st->teams.back().teamtype].id.c_str(),
                              frame);
                r.witness.emplace_back(b);
                last_created = st->teams_created;
            }
            std::vector<std::array<int, 3>> rows;
            for (const ra2engine::TeamInstance& t : st->teams)
                rows.push_back({t.teamtype, (int)t.state, t.script_pc});
            if (rows != last_rows) {
                for (const ra2engine::TeamInstance& t : st->teams) {
                    char b[200];
                    std::snprintf(b, sizeof b,
                                  "[ai-team] team=%s state=%d pc=%d members=%d frame=%ld",
                                  acfg.team.aimd.teams[t.teamtype].id.c_str(),
                                  (int)t.state, t.script_pc, (int)t.members.size(),
                                  frame);
                    r.witness.emplace_back(b);
                }
                last_rows = rows;
            }
        }
        if (!attack_seen) {
            for (const ra2engine::Unit& u : world.units()) {
                if (u.house == 1 && u.alive && u.attack_order &&
                    (u.target >= 0 || u.target_building >= 0)) {
                    char b[96];
                    std::snprintf(b, sizeof b,
                                  "[ai-team] FIRST ATTACK ORDER frame=%ld", frame);
                    r.witness.emplace_back(b);
                    attack_seen = true;
                    break;
                }
            }
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
        }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < kAiTeamFrames) step_one();

    {
        const ra2engine::AiTeamState* st = world.ai_team_state(1);
        char b[200];
        std::snprintf(b, sizeof b,
                      "[ai-team] final teams_created=%d live=%d units=%d credits=%d attack=%s",
                      st ? st->teams_created : -1, st ? (int)st->teams.size() : -1,
                      (int)world.units().size(),
                      (int)std::llround(world.ai_credits(1)),
                      attack_seen ? "yes" : "no");
        r.witness.emplace_back(b);
    }
    r.hash = ai_team_hash(world, 1);
    {
        char b[64];
        std::snprintf(b, sizeof b, "[ai-team] ai_team_hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(b);
    }
    // The witness chain the spec demands: created -> recruited (members>0)
    // -> gathered -> executing -> first attack order. Wedge = NOT ok.
    if (last_created < 1) { r.ok = false; r.witness.emplace_back("[ai-team] NO team created"); }
    if (!attack_seen)     { r.ok = false; r.witness.emplace_back("[ai-team] NO attack order -- team logic wedged"); }
    // Capture the ALREADY-rendered final frame WITHOUT stepping/witnessing again:
    // an extra step_one() here (rendered run only) could append a witness line if
    // a transition lands on that final frame, diverging the rendered vs headless
    // logs and turning the byte-identity gate false-RED [FIX 3].
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-ai-team: the AI-2 offensive-teams integration gate (7th permanent
// byte-identity gate). Runs simulate_ai_team rendered (hidden window +
// start/end screenshots) then headless from a FRESH World, and byte-compares
// the two witness logs + ai_team hashes. Exit 0 iff both runs are byte-
// identical AND both `ok`. NO hardcoded hash literal -- compute-and-self-compare
// (the --demo-ai-base precedent); the controller pins the observed value.
int run_demo_ai_team() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_ai_team_start.png";
    const std::string end_png = scratch + "demo_ai_team_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-ai-team] FAILED: building assets unavailable\n";
        return 1;
    }
    const int nacnst = roster_index(ba, "NACNST");
    if (nacnst < 0 || !ba.art[nacnst].ok) {
        std::cerr << "[demo-ai-team] FAILED: NACNST (Soviet ConYard) unavailable\n";
        return 1;
    }
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    OreArt ore_art = load_ore_art(ra2);

    // aimd.ini drives the enemy team brain [RE: ai-team.md]. A missing/broken
    // aimd.ini is a HARD failure for THIS gate (the gate's whole point is the
    // real parsed teams) -- unlike the interactive path's graceful degrade.
    Ini aimd_ini = Ini::parse("");
    try { aimd_ini = load_ini_from_ra2md(dir, "aimd.ini"); }
    catch (const std::exception& ex) {
        std::cerr << "[demo-ai-team] FAILED: aimd.ini unavailable (" << ex.what() << ")\n";
        return 1;
    }
    // The Soviet combat roster the enemy's aimd teams field.
    UnitRoster ur_ai = load_soviet_unit_roster(dir, ra2, ua, rules, art);
    if (ur_ai.defs.empty()) {
        std::cerr << "[demo-ai-team] FAILED: Soviet unit roster empty\n";
        return 1;
    }
    // Render vlist = War Miner voxel + the AI's combat vehicles, so the AI's
    // harvester and launched squad draw.
    std::vector<const VehicleAssets*> vlist;
    if (miners.harv.ok) vlist.push_back(&miners.harv);
    for (const VehicleAssets& v : ur_ai.vehicles) vlist.push_back(&v);

    // Camera centered on the enemy base region (same math as run_demo_ai_base).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 4, pc->y + 6, 0, 60, 30, 15);
        cam.x = (W - 168) / 2 - p.x;  // playfield left of the sidebar strip
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoAiTeamResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-ai-team)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_ai_team(a, ua, ba, miners, rules, vlist, ore_art, aimd_ini, ur_ai,
                                  /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoAiTeamResult r2 = simulate_ai_team(a, ua, ba, miners, rules, vlist, ore_art, aimd_ini,
                                           ur_ai, /*render=*/false, nullptr, nullptr, nullptr,
                                           W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-ai-team] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[80];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d ai_team_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

struct DemoAiTriggerResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the AI-trigger demo. Spawns the ai-team demo's base
// layout, enables teams + TRIGGERS on house 1 from the REAL parsed aimd.ini
// (the same make_ai_team_config path the interactive session uses), hands the
// PLAYER a building count satisfying a stock EnemyHouseOwns trigger condition
// (deterministic runtime exemplar scan -- NO hardcoded trigger id), and lets
// the trigger engine fire/create/attribute over kAiTriggerFrames. Witnesses:
// condition met -> trigger selected w/ weight -> team(s) created (Team1 +
// Team2 when set) -> outcome -> weight adjusted old->new. Attribution is by
// Team1 identity [RE: ai-trigger.md §6], so autocreate-created teams sharing
// a trigger's Team1 also feed the weights -- exactly the original's disband
// scan; every such outcome is witnessed deterministically.
// ai_trigger_hash pins the end state.
DemoAiTriggerResult simulate_ai_trigger(Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                        MinerAssets& miners, const Rules& rules,
                                        const std::vector<const VehicleAssets*>& vlist,
                                        const OreArt& ore_art, const ra2assets::Ini& aimd_ini,
                                        const UnitRoster& ur_ai, bool render, SDL_Renderer* ren,
                                        ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                        int W, int H, const std::string& start_png,
                                        const std::string& end_png) {
    DemoAiTriggerResult r;
    ra2engine::World world = build_world(a, ua, 0);
    wire_economy(world, ba, rules);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    spawn_base(world, ba, /*house=*/0, pc->x, pc->y + 4);
    spawn_base(world, ba, /*house=*/1, pc->x + 14, pc->y, /*with_refinery=*/true,
               miners.harv.ok ? &miners.harv : nullptr, /*harv_chrono=*/false,
               miners.harv_storage);
    ra2engine::AiHouseConfig acfg =
        make_ai_config(ba, rules, ra2engine::AiDifficulty::Normal,
                       (double)rules.mp_money());
    acfg.team = make_ai_team_config(ba, ur_ai, rules, aimd_ini,
                                    ra2engine::AiDifficulty::Normal,
                                    pc->x, pc->y + 4, /*enable_triggers=*/true);
    if (acfg.team.aimd.triggers.empty()) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO triggers parsed from aimd.ini");
        return r;
    }

    // Exemplar scan [RE-GATE APPLIED: ai-trigger.md §6 -- CONFIRMED: the
    // TRIGGER path is proven by the witness chain, not an autocreate-flag
    // distinction; ALL stock TeamTypes are Autocreate=yes]. The gate's VALUE
    // is an ADAPTIVE weight change across create->disband, so the exemplar
    // MUST be adaptive (min_weight < max_weight -- a start==min==max trigger
    // can never move) [controller directive]. Deterministic runtime scan --
    // NO hardcoded trigger id. `filters_ok` is the Normal-difficulty
    // ConditionMet filter set this house runs under [RE: ai-trigger.md §5]:
    // skirmish, per-trigger enable, NOT base-defense, the Normal enable slot
    // enabled[1] (difficulty index 1 -> enabled[2-1]), Soviet/any side,
    // <all>/Russians owner, adaptive weight band, and a finishable squad
    // (taskforce total <= 6).
    auto filters_ok = [&](const ra2engine::AITriggerType& t) -> bool {
        if (!t.is_for_skirmish || !t.is_enabled || t.base_defense) return false;
        if (!t.enabled[1]) return false;             // Normal slot [RE: §5]
        if (t.side != 0 && t.side != 2) return false;   // Soviet house
        if (!(t.owner_house == "<all>" || t.owner_house == "Russians"))
            return false;                            // "<none>"/"" reject
        if (t.min_weight >= t.max_weight) return false;   // ADAPTIVE only --
                                                          // the weight must be
                                                          // able to MOVE
        if (t.team1 < 0 || t.team1 >= (int)acfg.team.aimd.teams.size())
            return false;
        const ra2engine::TeamType& t1 = acfg.team.aimd.teams[t.team1];
        if (t1.taskforce < 0 ||
            t1.taskforce >= (int)acfg.team.aimd.taskforces.size())
            return false;
        int total = 0;
        for (const ra2engine::TaskForceMember& m :
             acfg.team.aimd.taskforces[t1.taskforce].members)
            total += m.count;
        return total <= 6;                           // finishable within the run
    };
    // PASS 1 (the brief's chosen seed): an EnemyHouseOwns (ct=0) BUILDING
    // trigger we can make TRUE by handing the PLAYER the required building
    // count. PASS 2 (the controller's named adaptive exemplar -- this roster's
    // surviving triggers are all ct=4/ct=5, so PASS 1 finds none here): an
    // intrinsically-TRUE condition needing no seed -- ct=-1 (always) or ct=4
    // HouseCredits with an always-satisfied comparator (>= a non-positive N;
    // the AI house credits are always >= 0). The recovered `Soviet Armor Hard
    // [0C8C56FC-G]` (ct=4, 70/40/70) is the archetype but is enabled ONLY on
    // the Hard on-disk slot; at the brief's Normal difficulty the scan lands
    // deterministically on its Normal-slot ct=4 adaptive sibling.
    int exemplar = -1;
    for (int i = 0; i < (int)acfg.team.aimd.triggers.size() && exemplar < 0; ++i) {
        const ra2engine::AITriggerType& t = acfg.team.aimd.triggers[i];
        if (t.condition_type == ra2engine::kCondEnemyOwns &&
            t.condition_object.kind == ra2engine::TechnoKind::Building &&
            filters_ok(t))
            exemplar = i;
    }
    for (int i = 0; i < (int)acfg.team.aimd.triggers.size() && exemplar < 0; ++i) {
        const ra2engine::AITriggerType& t = acfg.team.aimd.triggers[i];
        const bool always_true =
            t.condition_type == ra2engine::kCondNone ||
            (t.condition_type == ra2engine::kCondHouseCredits &&
             t.comparator.op == ra2engine::kCompGreaterEq &&
             t.comparator.value <= 0);
        if (always_true && filters_ok(t)) exemplar = i;
    }
    if (exemplar < 0) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO suitable adaptive stock trigger");
        return r;
    }
    const ra2engine::AITriggerType& ex = acfg.team.aimd.triggers[exemplar];
    // Seed the PLAYER a count satisfying the exemplar's comparator ONLY for the
    // EnemyHouseOwns BUILDING case (PASS 1); an intrinsically-true condition
    // (PASS 2) needs no seed and carries no building object.
    int need = 0;
    std::string objname = "<none>";
    if (ex.condition_object.kind == ra2engine::TechnoKind::Building &&
        ex.condition_object.index >= 0 &&
        ex.condition_object.index < (int)ba.roster.size()) {
        if (ex.comparator.op == ra2engine::kCompGreaterEq ||
            ex.comparator.op == ra2engine::kCompEq)
            need = ex.comparator.value;
        else if (ex.comparator.op == ra2engine::kCompGreater)
            need = ex.comparator.value + 1;
        else if (ex.comparator.op == ra2engine::kCompNotEq && ex.comparator.value == 0)
            need = 1;   // kCompLess/kCompLessEq: 0 owned already satisfies
        const int bt = ex.condition_object.index;
        const ra2engine::BuildingTypeDef& bd = ba.roster[bt];
        objname = bd.id;
        for (int k = 0; k < need; ++k)
            world.add_building(bt, pc->x + 1 + k * (bd.fw + 1), pc->y + 9,
                               bd.fw, bd.fh, /*house=*/0, std::max(1, bd.strength),
                               bd.armor_ix, 0.0, /*instant=*/true, bd.sight,
                               bd.grants_map_reveal);
    }
    world.enable_ai_house(1, acfg);
    {
        char b[240];
        std::snprintf(b, sizeof b,
                      "[ai-trigger] exemplar=%s ct=%d object=%s need=%d weight=%.1f[%.0f..%.0f]",
                      ex.id.c_str(), ex.condition_type, objname.c_str(), need,
                      ex.starting_weight, ex.min_weight, ex.max_weight);
        r.witness.emplace_back(b);
    }

    std::vector<int> bstates;  // FreeUnit grand-opening tracker
    const double dt = 1.0 / 15.0;
    const long kAiTriggerFrames = 13500;  // DEMO frame budget (NOT an engine
                                          // cadence -- trigger eval runs inside
                                          // the AI-2 team pass, no standalone
                                          // clock [RE: ai-trigger.md §5]):
                                          // 15 min at 15 Hz: ~5 Normal
                                          // TeamDelays passes (2500 frames) +
                                          // recruit/produce/gather/march +
                                          // disband margin for the outcome
    long frame = 0;
    int last_fired_seen = 0;
    int last_adjust_seen = 0;

    auto step_one = [&]() {
        world.step(dt);
        spawn_free_units_on_open(world, ba, rules, miners, bstates);
        const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
        if (ts) {
            if (ts->triggers_fired > last_fired_seen && ts->last_fired >= 0) {
                const ra2engine::AITriggerType& t =
                    acfg.team.aimd.triggers[ts->last_fired];
                const ra2engine::TeamType& t1 = acfg.team.aimd.teams[t.team1];
                char b[240];
                std::snprintf(b, sizeof b,
                              "[ai-trigger] condition met -> trigger=%s selected weight=%.1f frame=%ld",
                              t.id.c_str(), ts->weights[ts->last_fired], frame);
                r.witness.emplace_back(b);
                // BOTH Team1 and Team2 are created for the owner [RE-GATE
                // APPLIED: ai-trigger.md §3]; the second create may be
                // cap-skipped, so the FINAL teams_created line is the count
                // of record.
                std::snprintf(b, sizeof b,
                              "[ai-trigger] team(s) created team1=%s team2=%s frame=%ld",
                              t1.id.c_str(),
                              t.team2 >= 0
                                  ? acfg.team.aimd.teams[t.team2].id.c_str()
                                  : "<none>",
                              frame);
                r.witness.emplace_back(b);
                last_fired_seen = ts->triggers_fired;
            }
            if (ts->adjustments > last_adjust_seen && ts->last_adjusted >= 0) {
                char b[240];
                std::snprintf(b, sizeof b,
                              "[ai-trigger] outcome=%s trigger=%s weight %.1f -> %.1f frame=%ld",
                              ts->last_success ? "success" : "failure",
                              acfg.team.aimd.triggers[ts->last_adjusted].id.c_str(),
                              ts->last_old_weight, ts->last_new_weight, frame);
                r.witness.emplace_back(b);
                last_adjust_seen = ts->adjustments;
            }
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist, &ba,
                         ore_art.any ? &ore_art : nullptr);
        }
    };

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < kAiTriggerFrames) step_one();

    {
        const ra2engine::AiTriggerState* ts = world.ai_trigger_state(1);
        const ra2engine::AiTeamState* st = world.ai_team_state(1);
        char b[240];
        std::snprintf(b, sizeof b,
                      "[ai-trigger] final fired=%d adjustments=%d teams_created=%d live=%d credits=%d",
                      ts ? ts->triggers_fired : -1, ts ? ts->adjustments : -1,
                      st ? st->teams_created : -1, st ? (int)st->teams.size() : -1,
                      (int)std::llround(world.ai_credits(1)));
        r.witness.emplace_back(b);
    }
    r.hash = ai_trigger_hash(world, 1);
    {
        char b[80];
        std::snprintf(b, sizeof b, "[ai-trigger] ai_trigger_hash=%016llx",
                      static_cast<unsigned long long>(r.hash));
        r.witness.emplace_back(b);
    }
    // The witness chain the spec demands (spec §5, as amended by
    // ai-trigger.md §6: the TRIGGER path is proven by fire -> create ->
    // outcome -> adjust, NOT by an autocreate-flag distinction). Wedge = NOT ok.
    if (last_fired_seen < 1) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO trigger fired");
    }
    if (last_adjust_seen < 1) {
        r.ok = false;
        r.witness.emplace_back("[ai-trigger] NO outcome/weight adjustment -- trigger loop wedged");
    }
    // Capture the ALREADY-rendered final frame WITHOUT stepping/witnessing
    // again (the --demo-ai-team FIX-3 rule: an extra rendered-only step could
    // append a witness line and false-RED the byte-compare).
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-ai-trigger: the AI-3 AITriggerTypes weighted-engine integration gate
// (8th permanent byte-identity gate). Mirrors run_demo_ai_team exactly: runs
// simulate_ai_trigger rendered (hidden window + start/end screenshots) then
// headless from a FRESH World, and byte-compares the two witness logs +
// ai_trigger hashes. Exit 0 iff both runs are byte-identical AND both `ok`.
// NO hardcoded hash literal -- compute-and-self-compare; the controller pins
// the observed value.
int run_demo_ai_trigger() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_ai_trigger_start.png";
    const std::string end_png = scratch + "demo_ai_trigger_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-ai-trigger] FAILED: building assets unavailable\n";
        return 1;
    }
    const int nacnst = roster_index(ba, "NACNST");
    if (nacnst < 0 || !ba.art[nacnst].ok) {
        std::cerr << "[demo-ai-trigger] FAILED: NACNST (Soviet ConYard) unavailable\n";
        return 1;
    }
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    OreArt ore_art = load_ore_art(ra2);

    // aimd.ini drives the enemy trigger brain [RE: ai-trigger.md]. A
    // missing/broken aimd.ini is a HARD failure for THIS gate (the gate's
    // whole point is the real parsed triggers).
    Ini aimd_ini = Ini::parse("");
    try { aimd_ini = load_ini_from_ra2md(dir, "aimd.ini"); }
    catch (const std::exception& ex) {
        std::cerr << "[demo-ai-trigger] FAILED: aimd.ini unavailable (" << ex.what() << ")\n";
        return 1;
    }
    // The Soviet combat roster the enemy's aimd teams field.
    UnitRoster ur_ai = load_soviet_unit_roster(dir, ra2, ua, rules, art);
    if (ur_ai.defs.empty()) {
        std::cerr << "[demo-ai-trigger] FAILED: Soviet unit roster empty\n";
        return 1;
    }
    // Render vlist = War Miner voxel + the AI's combat vehicles.
    std::vector<const VehicleAssets*> vlist;
    if (miners.harv.ok) vlist.push_back(&miners.harv);
    for (const VehicleAssets& v : ur_ai.vehicles) vlist.push_back(&v);

    // Camera centered on the enemy base region (same math as run_demo_ai_team).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 4, pc->y + 6, 0, 60, 30, 15);
        cam.x = (W - 168) / 2 - p.x;  // playfield left of the sidebar strip
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoAiTriggerResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-ai-trigger)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_ai_trigger(a, ua, ba, miners, rules, vlist, ore_art, aimd_ini, ur_ai,
                                     /*render=*/true, ren, &tc, &cam, W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoAiTriggerResult r2 = simulate_ai_trigger(a, ua, ba, miners, rules, vlist, ore_art,
                                                 aimd_ini, ur_ai, /*render=*/false, nullptr,
                                                 nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-ai-trigger] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d ai_trigger_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// --- --demo-transport helpers ------------------------------------------------

// FNV fold of the cargo-phase state [spec §7]: every unit's (cell_x, cell_y,
// alive, cargo_of), each transport's cargo list (in order) + the derived
// pip total-size (cargo_total_size -- capacity's COUNT is the folded list
// length) + its cell, plus the draw-order size and a carried-units-own-
// no-cells occupancy audit -- folded at the demo's four fixed checkpoints
// (post-load, post-carry, post-unload, end). A NEW accumulator; world_hash/
// econ_hash/prod_hash/ai_base_hash/ai_team_hash/ai_trigger_hash UNTOUCHED.
uint64_t transport_fold(uint64_t h, const ra2engine::World& world) {
    const std::vector<ra2engine::Unit>& us = world.units();
    for (const ra2engine::Unit& u : us) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.cargo_of + 1));
    }
    for (const ra2engine::Unit& u : us) {
        if (u.passengers_max <= 0) continue;   // transports only
        h = fnv_mix(h, static_cast<uint64_t>(u.cargo.size()));
        for (int p : u.cargo) h = fnv_mix(h, static_cast<uint64_t>(p));
        h = fnv_mix(h, static_cast<uint64_t>(ra2engine::cargo_total_size(us, u)));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
    }
    // No carried unit is rendered or owns a cell [spec §7 step 2].
    h = fnv_mix(h, static_cast<uint64_t>(ra2engine::unit_draw_order(us).size()));
    uint64_t carried_cells = 0;
    const ra2engine::Occupancy& occ = world.occupancy();
    for (int y = 0; y < occ.height(); ++y)
        for (int x = 0; x < occ.width(); ++x) {
            const int o = occ.at(x, y);
            if (o >= 0 && o < static_cast<int>(us.size()) && us[o].cargo_of >= 0)
                ++carried_cells;
        }
    h = fnv_mix(h, carried_cells);
    return h;
}

struct DemoTransportResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the transport demo [spec §7] (headless or rendered --
// bit-identical): on the standard demo map, spawn one transport (HTK on this
// install; the FV fallback scales the script to capacity 1) + capacity+1 GIs
// for house 0, all scripted (no player input):
//   1. LOAD: order all GIs to enter -- capacity board (witnessed per board;
//      capacity is the passenger COUNT [RE-GATE APPLIED: transport-cargo.md
//      §1]), the last is REFUSED at capacity (the exact-fill boundary AND
//      the refusal both witnessed).
//   2. CARRY: order the loaded transport to a distant cell; run to arrival;
//      no passenger is rendered/occupying (folded into the hash).
//   3. UNLOAD: eject one per logic frame onto facing-scatter cells
//      (eject_cell; ejection begins immediately -- the DeployTime deploy-
//      START delay is unpinned and NOT implemented, a documented
//      carry-forward), LIFO (reverse boarding) witnessed [RE-GATE APPLIED:
//      transport-cargo.md §4/§6].
//   4. RESUME: run fixed frames; ejected GIs stand idle/guard normally.
// transport_hash = FNV folded at the four checkpoints. Wedge guards: >=1
// board AND >=1 eject AND >=1 capacity refusal AND LIFO eject order, else
// ok=false (exit 1 -- the gate can never pass vacuously).
DemoTransportResult simulate_transport(Assets& a, UnitAssets& ua, const TransportAssets& ta,
                                       const std::vector<const VehicleAssets*>& vlist,
                                       bool render, SDL_Renderer* ren,
                                       ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                       int W, int H, const std::string& start_png,
                                       const std::string& end_png) {
    DemoTransportResult r;
    if (!ta.ok) { r.ok = false; return r; }
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    // Spawn the transport (unit 0) + capacity+1 GIs (units 1..n).
    const int cap = ta.va.combat.passengers;
    const int n_pass = cap + 1;
    world.add_combat_unit(pc->x, pc->y, /*house=*/0, ta.va.combat);
    const int t_idx = 0;
    spawn_squad(world, pc->x - 4, pc->y, n_pass, /*house=*/0, ua.gi);
    {
        char b[160];
        std::snprintf(b, sizeof b,
                      "[transport] variant=%s passengers=%d size_limit=%d size=%d gis=%d",
                      ta.id.c_str(), cap, ta.va.combat.size_limit, ta.va.combat.size,
                      n_pass);
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    int boards = 0, refusals = 0, ejects = 0;
    std::vector<int> board_seq, eject_seq;   // LIFO witness material
    uint64_t h = 0xcbf29ce484222325ull;      // FNV-1a offset basis (world_hash's)

    auto step_one = [&]() {
        world.step(dt);
        for (const ra2engine::CargoEvent& e : world.take_cargo_events()) {
            char b[140];
            if (e.kind == ra2engine::CargoEvent::Kind::Board) {
                ++boards;
                board_seq.push_back(e.unit);
                std::snprintf(b, sizeof b, "[cargo] board unit=%d count=%d/%d frame=%ld",
                              e.unit, e.x, e.y, frame);
            } else if (e.kind == ra2engine::CargoEvent::Kind::Refuse) {
                ++refusals;
                std::snprintf(b, sizeof b,
                              "[cargo] refuse unit=%d (full) count=%d/%d frame=%ld",
                              e.unit, e.x, e.y, frame);
            } else {
                ++ejects;
                eject_seq.push_back(e.unit);
                std::snprintf(b, sizeof b,
                              "[cargo] eject unit=%d cell=(%d,%d) order=%d frame=%ld",
                              e.unit, e.x, e.y, ejects, frame);
            }
            r.witness.emplace_back(b);
        }
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };

    // 1) LOAD: select every GI and issue the enter order (oversubscribed by
    //    one -- the capacity refusal is part of the script).
    for (int i = 1; i <= n_pass; ++i) world.units()[i].selected = true;
    world.order_enter_transport(t_idx);

    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < 900 && boards + refusals < n_pass) step_one();
    h = transport_fold(h, world);                     // checkpoint 1: post-load
    {
        char b[120];
        std::snprintf(b, sizeof b, "[cargo] loaded boards=%d refusals=%d cargo=%d",
                      boards, refusals, (int)world.units()[t_idx].cargo.size());
        r.witness.emplace_back(b);
    }

    // 2) CARRY: drive the loaded transport to a distant cell.
    world.clear_selection();
    world.units()[t_idx].selected = true;
    world.order_move_selected(pc->x + 12, pc->y + 8);
    while (frame < 2400 && world.units()[t_idx].moving()) step_one();
    h = transport_fold(h, world);                     // checkpoint 2: post-carry
    {
        char b[120];
        std::snprintf(b, sizeof b, "[cargo] carry arrived=(%d,%d) cargo=%d frame=%ld",
                      world.units()[t_idx].cell_x, world.units()[t_idx].cell_y,
                      (int)world.units()[t_idx].cargo.size(), frame);
        r.witness.emplace_back(b);
    }

    // 3) UNLOAD: one eject per frame via the facing-scatter eject_cell, LIFO.
    world.clear_selection();
    world.units()[t_idx].selected = true;
    world.order_unload_selected();
    while (frame < 3000 && !world.units()[t_idx].cargo.empty()) step_one();
    h = transport_fold(h, world);                     // checkpoint 3: post-unload

    // 4) RESUME: fixed frames; ejected GIs stand idle/guard normally.
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = transport_fold(h, world);                     // checkpoint 4: end
    r.hash = h;

    {
        char b[140];
        std::snprintf(b, sizeof b,
                      "[transport] final boards=%d refusals=%d ejects=%d drawn=%d frame=%ld",
                      boards, refusals, ejects,
                      (int)ra2engine::unit_draw_order(world.units()).size(), frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[transport] transport_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously.
    if (boards < 1) { r.ok = false; r.witness.emplace_back("[transport] WEDGE: no board"); }
    if (ejects < 1) { r.ok = false; r.witness.emplace_back("[transport] WEDGE: no eject"); }
    if (refusals < 1) {
        r.ok = false;
        r.witness.emplace_back("[transport] WEDGE: no capacity refusal");
    }
    const std::vector<int> lifo_expect(board_seq.rbegin(), board_seq.rend());
    if (eject_seq != lifo_expect) { // LIFO: eject order == REVERSE boarding order
        r.ok = false;               // [RE-GATE APPLIED: transport-cargo.md §6 --
                                    // CONFIRMED head-push/head-pop stack]
        r.witness.emplace_back(
            "[transport] WEDGE: eject order != LIFO (reverse boarding) order");
    }
    // Capture the ALREADY-rendered final frame WITHOUT stepping again (the
    // --demo-ai-team FIX-3 rule: an extra rendered-only step could append a
    // witness line and false-RED the byte-compare).
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-transport: the CB-1 cargo integration gate (9th permanent
// byte-identity gate). Mirrors run_demo_ai_trigger exactly: runs
// simulate_transport rendered (hidden window + start/end screenshots) then
// headless from a FRESH World, and byte-compares the two witness logs +
// hashes. Exit 0 iff both runs are byte-identical AND both ok. NO hardcoded
// hash literal -- compute-and-self-compare; the controller pins the observed
// value in the README gate table.
int run_demo_transport() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_transport_start.png";
    const std::string end_png = scratch + "demo_transport_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    TransportAssets ta = load_transport_assets(dir, ra2, ua.pal);
    if (!ta.ok) {
        std::cerr << "[demo-transport] FAILED: transport art unavailable (HTK and FV)\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist{&ta.va};

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 6, pc->y + 4, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    // Run 1: rendered (hidden window) + start/end screenshots.
    DemoTransportResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-transport)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_transport(a, ua, ta, vlist, /*render=*/true, ren, &tc, &cam,
                                    W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Run 2: headless -- identical script from a FRESH World.
    DemoTransportResult r2 = simulate_transport(a, ua, ta, vlist, /*render=*/false, nullptr,
                                                nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-transport] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d transport_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// --- --demo-aircraft helpers -------------------------------------------------

// FNV fold of the flight-phase state [spec §7]: every unit's (cell_x, cell_y,
// alive, hp) PLUS each aircraft's (round(altitude), flight_state, target) and
// the occupancy-held count for airborne units (must be 0). Folded at the
// demo's four fixed checkpoints (post-takeoff, post-fly, post-attack, end). A
// NEW accumulator; world_hash and every prior *_hash UNTOUCHED.
uint64_t aircraft_fold(uint64_t h, const ra2engine::World& world) {
    const std::vector<ra2engine::Unit>& us = world.units();
    for (const ra2engine::Unit& u : us) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
    }
    for (const ra2engine::Unit& u : us) {
        if (!u.is_aircraft) continue;
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(u.altitude)));
        h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(u.flight_state)));
        h = fnv_mix(h, static_cast<uint64_t>(u.target + 1));
    }
    // Airborne units own NO cell [spec §7 step 2].
    uint64_t airborne_cells = 0;
    const ra2engine::Occupancy& occ = world.occupancy();
    for (int y = 0; y < occ.height(); ++y)
        for (int x = 0; x < occ.width(); ++x) {
            const int o = occ.at(x, y);
            if (o >= 0 && o < static_cast<int>(us.size()) && us[o].is_aircraft &&
                us[o].flight_state != ra2engine::FlightState::Grounded)
                ++airborne_cells;
        }
    h = fnv_mix(h, airborne_cells);
    return h;
}

struct DemoAircraftResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the aircraft demo [spec §7] (headless or rendered --
// bit-identical): on the standard demo map, spawn one ZEP (house 0) + TWO
// adjacent hostile ground GI targets (house 1, one cell apart), all scripted
// (no player input):
//   1. TAKEOFF: order a move -> release the ground cell, ascend to cruise.
//   2. FLY: cruise straight-line to a distant cell over any obstacle
//      (occ_held=0 while airborne, folded into the hash).
//   3. ATTACK: order the aircraft to attack target A; it flies to range and
//      drops ONE bomb whose BlimpHE 2-cell splash must kill BOTH adjacent
//      targets (A at the impact cell + B one cell away) [D5].
//   4. RESUME: run fixed frames; the Kirov holds cruise (BalloonHover).
// aircraft_hash = FNV folded at the four checkpoints. Wedge guards: takeoff
// observed AND airborne cruise (occ_held=0) observed AND both splash targets
// dead, else ok=false (exit 1 -- the gate can never pass vacuously).
DemoAircraftResult simulate_aircraft(Assets& a, UnitAssets& ua, const AircraftAssets& aa,
                                     const std::vector<const VehicleAssets*>& vlist,
                                     bool render, SDL_Renderer* ren,
                                     ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                     int W, int H, const std::string& start_png,
                                     const std::string& end_png) {
    DemoAircraftResult r;
    if (!aa.ok) { r.ok = false; return r; }
    ra2engine::World world = build_world(a, ua, 0);
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    if (!pc) { r.ok = false; return r; }

    // Spawn the aircraft (unit 0) + TWO adjacent hostile ground GI targets
    // (units 1 & 2, one cell apart) so ONE BlimpHE 2-cell splash kills BOTH
    // [RE-GATE APPLIED: aircraft.md §4 -> combat.md §7, D5]. ua.gi is a
    // UnitCombat; its warhead CellSpread<1 does not matter -- the ATTACKER's
    // (ZEP) CellSpread=2 drives the splash.
    world.add_combat_unit(pc->x, pc->y, /*house=*/0, aa.va.combat);
    const int z_idx = 0;
    world.add_combat_unit(pc->x + 8, pc->y + 6, /*house=*/1, ua.gi);  // target A (splash center)
    world.add_combat_unit(pc->x + 9, pc->y + 6, /*house=*/1, ua.gi);  // target B (1 cell away)
    const int tgt_idx = 1;
    const int tgt2_idx = 2;
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[aircraft] variant=%s cruise_alt=%.0f speed=%.3f climb=%.0f omni=%s",
                      aa.id.c_str(), aa.va.combat.cruise_altitude, aa.va.combat.flight_speed,
                      aa.va.combat.climb_rate, aa.va.combat.omni_fire ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    bool took_off = false, cruised_airborne = false;
    int hits = 0;
    int splash_kills = 0;                        // D5: how many of the 2 targets died
    int prev_hp = world.units()[tgt_idx].hp;
    uint64_t h = 0xcbf29ce484222325ull;         // FNV-1a offset basis

    auto dead = [&](int i) {
        return !world.units()[i].alive || world.units()[i].die_time >= 0.0;
    };

    auto airborne_occ = [&]() {
        int held = 0;
        const ra2engine::Occupancy& occ = world.occupancy();
        for (int y = 0; y < occ.height(); ++y)
            for (int x = 0; x < occ.width(); ++x) {
                const int o = occ.at(x, y);
                if (o == z_idx && world.units()[z_idx].flight_state != ra2engine::FlightState::Grounded)
                    ++held;
            }
        return held;
    };
    auto step_one = [&]() {
        world.step(dt);
        const ra2engine::Unit& z = world.units()[z_idx];
        if (z.flight_state != ra2engine::FlightState::Grounded && z.altitude > 0.0)
            took_off = true;
        if (z.flight_state == ra2engine::FlightState::Cruising && z.altitude > 0.0 &&
            airborne_occ() == 0)
            cruised_airborne = true;
        const int hp = world.units()[tgt_idx].hp;
        if (hp < prev_hp) { ++hits; }
        prev_hp = hp;
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };

    // 1) TAKEOFF: order a move -> release cell, ascend to cruise.
    world.order_unit_move(z_idx, pc->x, pc->y);        // takeoff in place
    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < 600 && world.units()[z_idx].flight_state != ra2engine::FlightState::Cruising)
        step_one();
    h = aircraft_fold(h, world);                        // checkpoint 1: post-takeoff
    {
        char b[120];
        std::snprintf(b, sizeof b, "[air] takeoff alt=%.0f/%.0f cell_released=%s frame=%ld",
                      world.units()[z_idx].altitude, aa.va.combat.cruise_altitude,
                      airborne_occ() == 0 ? "yes" : "no", frame);
        r.witness.emplace_back(b);
    }

    // 2) FLY: cruise straight to a distant cell (over any obstacle).
    world.order_unit_move(z_idx, pc->x + 14, pc->y + 10);
    while (frame < 2400 && world.units()[z_idx].has_flight_goal) step_one();
    h = aircraft_fold(h, world);                        // checkpoint 2: post-fly
    {
        char b[128];
        std::snprintf(b, sizeof b, "[air] cruise arrived=(%d,%d) alt=%.0f occ_held=%d frame=%ld",
                      world.units()[z_idx].cell_x, world.units()[z_idx].cell_y,
                      world.units()[z_idx].altitude, airborne_occ(), frame);
        r.witness.emplace_back(b);
    }

    // 3) ATTACK: order the aircraft to attack ground target A; it flies to
    //    range and drops ONE bomb whose BlimpHE 2-cell splash must kill BOTH
    //    adjacent targets (A at the impact cell + B one cell away) [D5].
    world.clear_selection();
    world.units()[z_idx].selected = true;
    world.order_attack(tgt_idx);
    while (frame < 5000 && !(dead(tgt_idx) && dead(tgt2_idx)))
        step_one();
    h = aircraft_fold(h, world);                        // checkpoint 3: post-attack
    splash_kills = (dead(tgt_idx) ? 1 : 0) + (dead(tgt2_idx) ? 1 : 0);
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[air] hit target=%d dead=%s | target=%d dead=%s | splash_kills=%d hits=%d frame=%ld",
                      tgt_idx, dead(tgt_idx) ? "yes" : "no",
                      tgt2_idx, dead(tgt2_idx) ? "yes" : "no",
                      splash_kills, hits, frame);
        r.witness.emplace_back(b);
    }

    // 4) RESUME: fixed window; the Kirov holds cruise (BalloonHover).
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = aircraft_fold(h, world);                        // checkpoint 4: end
    r.hash = h;

    {
        char b[144];
        std::snprintf(b, sizeof b,
                      "[aircraft] final took_off=%s cruised=%s hits=%d splash_kills=%d frame=%ld",
                      took_off ? "yes" : "no", cruised_airborne ? "yes" : "no",
                      hits, splash_kills, frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[aircraft] aircraft_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously.
    if (!took_off) { r.ok = false; r.witness.emplace_back("[aircraft] WEDGE: no takeoff"); }
    if (!cruised_airborne) {
        r.ok = false;
        r.witness.emplace_back("[aircraft] WEDGE: no airborne cruise with occ_held=0");
    }
    // D5: strengthened -- ONE bomb's BlimpHE 2-cell splash must kill BOTH
    // adjacent targets, proving the splash (not just a single hit).
    if (splash_kills < 2) {
        r.ok = false;
        r.witness.emplace_back(
            "[aircraft] WEDGE: BlimpHE splash did not kill BOTH adjacent targets (need 2 kills from one bomb)");
    }
    if (render) { capture_png(ren, end_png, W, H); }   // capture the ALREADY-rendered frame
    return r;
}

// --demo-aircraft: the CB-2 flight integration gate (10th permanent
// byte-identity gate). Mirrors run_demo_transport exactly: runs
// simulate_aircraft rendered (hidden window + start/end screenshots) then
// headless from a FRESH World, and byte-compares the two witness logs +
// hashes. Exit 0 iff both runs are byte-identical AND both ok. NO hardcoded
// hash literal -- compute-and-self-compare; the controller pins the observed
// value in the README gate table.
int run_demo_aircraft() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_aircraft_start.png";
    const std::string end_png = scratch + "demo_aircraft_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    AircraftAssets aa = load_aircraft_assets(dir, ra2, ua.pal);
    if (!aa.ok) {
        std::cerr << "[demo-aircraft] FAILED: aircraft art unavailable (ZEP/ORCA/BEAG)\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist{&aa.va};

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 7, pc->y + 5, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    DemoAircraftResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-aircraft)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_aircraft(a, ua, aa, vlist, /*render=*/true, ren, &tc, &cam,
                                   W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    DemoAircraftResult r2 = simulate_aircraft(a, ua, aa, vlist, /*render=*/false, nullptr,
                                              nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-aircraft] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d aircraft_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// --- --demo-naval helpers ----------------------------------------------------

// CB-3 Naval [RE-GATE APPLIED: naval.md §2]: pick a start Water cell and a
// distant Water cell in the SAME 4-connected water region (the 252-cell main
// body on mapsmd03), so the demo ship has a real path to sail. Returns false if
// no water region of >= min_cells exists -- a defensive wedge guard only
// (CONFIRMED not to fire on mapsmd03; the caller then errors out visibly).
bool pick_water_cell(const std::vector<ra2assets::LandType>& lt, int gw, int gh,
                     int min_cells, ra2engine::Cellxy& start, ra2engine::Cellxy& far_) {
    const std::size_t n = static_cast<std::size_t>(gw) * gh;
    std::vector<int> comp(n, -1);
    int best_comp = -1, best_size = 0;
    ra2engine::Cellxy best_a{0, 0}, best_b{0, 0};
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * gw + x;
            if (lt[i] != ra2assets::LandType::Water || comp[i] != -1) continue;
            const int id = static_cast<int>(i);
            std::vector<ra2engine::Cellxy> stack{{x, y}};
            comp[i] = id;
            int size = 0;
            ra2engine::Cellxy a{x, y}, b{x, y};   // extremes for a "distant" pair
            while (!stack.empty()) {
                const ra2engine::Cellxy c = stack.back(); stack.pop_back();
                ++size;
                if (c.x + c.y < a.x + a.y) a = c;
                if (c.x + c.y > b.x + b.y) b = c;
                const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = c.x + dx[k], ny = c.y + dy[k];
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    const std::size_t j = static_cast<std::size_t>(ny) * gw + nx;
                    if (lt[j] == ra2assets::LandType::Water && comp[j] == -1) {
                        comp[j] = id;
                        stack.push_back({nx, ny});
                    }
                }
            }
            if (size > best_size) { best_size = size; best_comp = id; best_a = a; best_b = b; }
        }
    if (best_comp < 0 || best_size < min_cells) return false;
    start = best_a;
    far_ = best_b;
    return true;
}

// FNV fold of the naval-phase state [spec §7]: every unit's (cell_x, cell_y,
// alive, hp) PLUS each naval unit's (movement_domain, path.size(), target) and
// a running count of DISTINCT water cells the ship has occupied. Folded at the
// demo's four fixed checkpoints (post-move, post-land-block, post-attack, end).
// A NEW accumulator; world_hash and every prior *_hash UNTOUCHED.
uint64_t naval_fold(uint64_t h, const ra2engine::World& world, uint64_t water_cells) {
    const std::vector<ra2engine::Unit>& us = world.units();
    for (const ra2engine::Unit& u : us) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
    }
    for (const ra2engine::Unit& u : us) {
        if (!u.is_naval) continue;
        h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(u.movement_domain)));
        h = fnv_mix(h, static_cast<uint64_t>(u.path.size()));
        h = fnv_mix(h, static_cast<uint64_t>(u.target + 1));
    }
    h = fnv_mix(h, water_cells);
    return h;
}

struct DemoNavalResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

DemoNavalResult simulate_naval(Assets& a, UnitAssets& ua, const NavalAssets& na,
                               const std::vector<const VehicleAssets*>& vlist,
                               bool render, SDL_Renderer* ren,
                               ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                               int W, int H, const std::string& start_png,
                               const std::string& end_png) {
    DemoNavalResult r;
    if (!na.ok) { r.ok = false; return r; }
    ra2engine::World world = build_world(a, ua, 0);
    const int gw = world.grid_w(), gh = world.grid_h();

    // Resolve the map's per-cell LandType and hand World the water grid.
    std::vector<ra2assets::LandType> lt = resolve_land_types(a.map, a.theater, gw, gh);
    world.set_water_terrain(lt);

    // Find a water region: a start cell + a distant cell to sail to in the
    // 252-cell main body [RE-GATE APPLIED: naval.md §2 -- mapsmd03 has 468 Water
    // cells, bbox [71,11]-[95,40]]. The false path is a defensive wedge guard
    // only (CONFIRMED not to fire on mapsmd03) -> fail visibly if it ever does.
    ra2engine::Cellxy w_start{0, 0}, w_far{0, 0};
    if (!pick_water_cell(lt, gw, gh, /*min_cells=*/12, w_start, w_far)) {
        r.ok = false;
        r.witness.emplace_back("[naval] WEDGE: no materialized contiguous water region");
        return r;
    }
    // The target sits on a water cell near w_far (in-range of the ship's Range=5
    // once it sails over); use w_far itself as the enemy cell and a cell a few
    // steps in from w_start as the ship's mid-approach goal.
    const ra2engine::Cellxy ship_cell = w_start;
    const ra2engine::Cellxy enemy_cell = w_far;

    world.add_combat_unit(ship_cell.x, ship_cell.y, /*house=*/0, na.va.combat);   // idx 0
    const int s_idx = 0;
    ra2engine::UnitCombat tgt = na.va.combat;    // a second Sea Scorpion (naval duel)
    ra2engine::Unit& enemy = world.add_combat_unit(enemy_cell.x, enemy_cell.y, /*house=*/1, tgt);
    const int t_idx = 1;
    enemy.hp = enemy.max_hp;
    {
        char b[192];
        std::snprintf(b, sizeof b,
                      "[naval] variant=%s ship=(%d,%d) enemy=(%d,%d) range=%.1f",
                      na.id.c_str(), ship_cell.x, ship_cell.y, enemy_cell.x, enemy_cell.y,
                      na.va.combat.range);
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    bool moved_on_water = false, land_blocked = false;
    int hits = 0;
    int prev_hp = world.units()[t_idx].hp;
    uint64_t water_cells = 0;
    int last_cx = ship_cell.x, last_cy = ship_cell.y;
    uint64_t h = 0xcbf29ce484222325ull;          // FNV-1a offset basis

    auto is_water = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= gw || y >= gh) return false;
        return lt[static_cast<std::size_t>(y) * gw + x] == ra2assets::LandType::Water;
    };
    auto dead = [&](int i) {
        return !world.units()[i].alive || world.units()[i].die_time >= 0.0;
    };
    auto step_one = [&]() {
        world.step(dt);
        const ra2engine::Unit& s = world.units()[s_idx];
        if ((s.cell_x != last_cx || s.cell_y != last_cy) && is_water(s.cell_x, s.cell_y)) {
            ++water_cells;
            moved_on_water = true;
        }
        last_cx = s.cell_x; last_cy = s.cell_y;
        const int hp = world.units()[t_idx].hp;
        if (hp < prev_hp) ++hits;
        prev_hp = hp;
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };

    // 1) MOVE: sail across the water toward a mid-water cell short of the enemy.
    const int mid_x = (ship_cell.x + enemy_cell.x) / 2;
    const int mid_y = (ship_cell.y + enemy_cell.y) / 2;
    world.order_unit_move(s_idx, mid_x, mid_y);
    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    while (frame < 4000 && world.units()[s_idx].moving()) step_one();
    h = naval_fold(h, world, water_cells);       // checkpoint 1: post-move
    {
        char b[144];
        std::snprintf(b, sizeof b,
                      "[naval] moved cells=%llu path_water=%s arrived=(%d,%d) frame=%ld",
                      static_cast<unsigned long long>(water_cells),
                      moved_on_water ? "yes" : "no",
                      world.units()[s_idx].cell_x, world.units()[s_idx].cell_y, frame);
        r.witness.emplace_back(b);
    }

    // 2) LAND-BLOCK: order the ship to a LAND cell; it must refuse (no water
    //    path -> empty/stay-put), the inverse of a ground unit.
    ra2engine::Cellxy land{0, 0};
    for (int y = 0; y < gh && (land.x == 0 && land.y == 0); ++y)
        for (int x = 0; x < gw; ++x)
            if (!is_water(x, y) &&
                lt[static_cast<std::size_t>(y) * gw + x] == ra2assets::LandType::Clear) {
                land = {x, y}; break;
            }
    const int before_x = world.units()[s_idx].cell_x, before_y = world.units()[s_idx].cell_y;
    world.order_unit_move(s_idx, land.x, land.y);
    const bool empty_path = world.units()[s_idx].path.empty();
    for (int f = 0; f < 60; ++f) step_one();
    if (empty_path && world.units()[s_idx].cell_x == before_x &&
        world.units()[s_idx].cell_y == before_y)
        land_blocked = true;
    h = naval_fold(h, world, water_cells);       // checkpoint 2: post-land-block
    {
        char b[144];
        std::snprintf(b, sizeof b, "[naval] land-block ordered=(%d,%d) moved=%s frame=%ld",
                      land.x, land.y, land_blocked ? "no" : "YES-BUG", frame);
        r.witness.emplace_back(b);
    }

    // 3) ATTACK: order the ship to attack the hostile ship; it approaches over
    //    water (the combat-approach naval branch) and fires FlakTrackGun.
    world.clear_selection();
    world.units()[s_idx].selected = true;
    world.order_attack(t_idx);
    while (frame < 9000 && !dead(t_idx)) step_one();
    h = naval_fold(h, world, water_cells);       // checkpoint 3: post-attack
    {
        char b[144];
        std::snprintf(b, sizeof b, "[naval] hit target=%d dmg-events=%d target_dead=%s frame=%ld",
                      t_idx, hits, dead(t_idx) ? "yes" : "no", frame);
        r.witness.emplace_back(b);
    }

    // 4) RESUME: fixed window; the ship holds station on the water.
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = naval_fold(h, world, water_cells);       // checkpoint 4: end
    r.hash = h;

    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[naval] final moved_on_water=%s land_blocked=%s hits=%d water_cells=%llu frame=%ld",
                      moved_on_water ? "yes" : "no", land_blocked ? "yes" : "no", hits,
                      static_cast<unsigned long long>(water_cells), frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[naval] naval_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously -- the ship must
    // MOVE on >=1 water cell AND be BLOCKED from a land cell AND land >=1 hit.
    if (!moved_on_water) { r.ok = false; r.witness.emplace_back("[naval] WEDGE: no move on water"); }
    if (!land_blocked) { r.ok = false; r.witness.emplace_back("[naval] WEDGE: not blocked from land"); }
    if (hits < 1) { r.ok = false; r.witness.emplace_back("[naval] WEDGE: no hit landed on the target"); }
    if (render) { capture_png(ren, end_png, W, H); }   // capture the ALREADY-rendered frame
    return r;
}

// --demo-naval: the CB-3 naval integration gate (11th permanent byte-identity
// gate). Mirrors run_demo_aircraft exactly: runs simulate_naval rendered
// (hidden window + start/end screenshots) then headless from a FRESH World, and
// byte-compares the two witness logs + hashes. Exit 0 iff both runs are
// byte-identical AND both ok. NO hardcoded hash literal -- compute-and-self-
// compare; the controller pins the observed value in the README gate table.
int run_demo_naval() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_naval_start.png";
    const std::string end_png = scratch + "demo_naval_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    NavalAssets na = load_naval_assets(dir, ra2, ua.pal);
    if (!na.ok) {
        std::cerr << "[demo-naval] FAILED: naval art unavailable (HYD/DRED/DEST)\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist{&na.va};

    ra2engine::Camera cam;   // centered on the water region so the ship is framed
    {
        // Center on the middle of the ship's water approach (start<->far midpoint
        // of the same 252-cell body simulate_naval sails) so the start/end
        // screenshots actually show the Sea Scorpion on water. The exact camera
        // does NOT affect the hash (rendered/headless runs fold identical state);
        // it only frames the rendered PNGs. Falls back to the map interior if the
        // water pick ever fails (defensive -- CONFIRMED not to on mapsmd03).
        ra2engine::World probe = build_world(a, ua, 0);
        const int gw = probe.grid_w(), gh = probe.grid_h();
        std::vector<ra2assets::LandType> lt = resolve_land_types(a.map, a.theater, gw, gh);
        ra2engine::Cellxy ws{0, 0}, wf{0, 0};
        int cx, cy;
        if (pick_water_cell(lt, gw, gh, /*min_cells=*/12, ws, wf)) {
            cx = (ws.x + wf.x) / 2;
            cy = (ws.y + wf.y) / 2;
        } else {
            const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
            cx = pc ? pc->x : gw / 2;
            cy = pc ? pc->y : gh / 2;
        }
        const ra2engine::ScreenPt p = ra2engine::cell_to_screen(cx, cy, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    DemoNavalResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-naval)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_naval(a, ua, na, vlist, /*render=*/true, ren, &tc, &cam,
                                W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    DemoNavalResult r2 = simulate_naval(a, ua, na, vlist, /*render=*/false, nullptr,
                                        nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-naval] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d naval_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// --- --demo-superweapon helpers ----------------------------------------------

// FNV fold of the superweapon-phase state [spec §7]: every unit's (cell_x,
// cell_y, alive, hp) PLUS the firing house's SW (quantized timer, ready, type)
// and the target cell (cx, cy). The timer is folded as an integer frame count
// (round(timer*15)) to stay bit-stable across the dt accumulation. Folded at
// the demo's four fixed checkpoints (post-charge, post-fire, post-detonate,
// end). A NEW accumulator; world_hash and every prior *_hash UNTOUCHED.
uint64_t superweapon_fold(uint64_t h, const ra2engine::World& world, int house,
                          int cx, int cy) {
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
    }
    if (const ra2engine::Superweapon* sw = world.sw_state(house)) {
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(sw->timer * 15.0)));  // quantized frames
        h = fnv_mix(h, static_cast<uint64_t>(sw->ready ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(sw->type)));
    }
    h = fnv_mix(h, static_cast<uint64_t>(cx));
    h = fnv_mix(h, static_cast<uint64_t>(cy));
    return h;
}

struct DemoSuperweaponResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the superweapon demo [spec §7] (headless or rendered --
// bit-identical): grant house 0 the Nuclear Missile (resolved params), spawn a
// tight enemy cluster (house 1) within the nuke radius + one OWN (house 0) unit
// in-radius (to prove enemies-only sparing) + one FAR enemy out-of-radius (to
// prove out-of-radius is untouched), all scripted (no player input):
//   1. CHARGE: step until the SW reaches READY (the real recharge countdown).
//   2. FIRE: fire_superweapon(0, cx, cy) returns true; the timer resets.
//   3. DETONATE: the in-radius enemies are damaged/killed; own spared, far
//      untouched.
//   4. RECHARGE: a fixed window; the SW begins re-charging from 0.
// superweapon_hash = FNV folded at the four checkpoints. Wedge guards: reached
// READY AND fire returned true AND >=1 in-radius kill, else ok=false (exit 1 --
// the gate can never pass vacuously). The SW cameo is sidebar-only, so the gate
// hashes pure sim state (art-independent).
DemoSuperweaponResult simulate_superweapon(Assets& a, UnitAssets& ua,
                                           const std::vector<const VehicleAssets*>& vlist,
                                           bool render, SDL_Renderer* ren,
                                           ra2engine::TextureCache* tc, const ra2engine::Camera* cam,
                                           int W, int H, const std::string& start_png,
                                           const std::string& end_png,
                                           const ra2assets::Rules& rules) {
    DemoSuperweaponResult r;
    ra2engine::World world = build_world(a, ua, 0);

    // A hand-built target stat block (the sim is exemplar-art-independent -- the
    // gate hashes pure state, not art; targets render if E1 anim is available).
    auto target_stats = []() {
        ra2engine::UnitCombat c; c.image = "E1"; c.strength = 100; c.armor = "none"; return c;
    };
    if (ua.anims.count("E1")) world.set_anim("E1", ua.anims.at("E1"));

    // Target cell + an enemy cluster within the nuke radius (CellSpread=10), one
    // OWN (house 0) unit in-radius (to prove enemies-only sparing), and one FAR
    // enemy well outside the radius (to prove out-of-radius is untouched).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int cx = pc ? pc->x + 6 : 30;
    const int cy = pc ? pc->y + 6 : 30;
    std::vector<int> in_radius;                       // enemy indices expected to die
    for (int d = 0; d < 6; ++d) {
        const int ex = cx + (d % 3) - 1;              // a tight 3-wide cluster on the cell
        const int ey = cy + (d / 3) - 1;
        ra2engine::Unit& e = world.add_combat_unit(ex, ey, /*house=*/1, target_stats());
        (void)e;
        in_radius.push_back((int)world.units().size() - 1);
    }
    world.add_combat_unit(cx + 2, cy, /*house=*/0, target_stats());       // OWN unit in-radius
    const int own_idx = (int)world.units().size() - 1;
    world.add_combat_unit(cx, cy + 40, /*house=*/1, target_stats());      // FAR enemy (out of radius)
    const int far_idx = (int)world.units().size() - 1;
    const int own_hp0 = world.units()[own_idx].hp;
    const int far_hp0 = world.units()[far_idx].hp;

    // Grant house 0 the Nuclear Missile with the recovered params. The framework
    // charges the REAL recharge value; resolve_superweapon supplied it.
    SuperweaponParams p = resolve_superweapon(rules);
    std::array<int, 11> verses = p.verses;
    world.grant_superweapon(0, ra2engine::SuperweaponType::Nuke, p.recharge_secs,
                            p.damage, p.cell_spread, p.percent_at_max, verses);
    {
        char b[192];
        std::snprintf(b, sizeof b,
                      "[sw] granted recharge_secs=%.1f damage=%d cell_spread=%.1f pmax=%.3f "
                      "target=(%d,%d) enemies=%d", p.recharge_secs, p.damage, p.cell_spread,
                      p.percent_at_max, cx, cy, (int)in_radius.size());
        r.witness.emplace_back(b);
    }

    const double dt = 1.0 / 15.0;
    long frame = 0;
    uint64_t h = 0xcbf29ce484222325ull;               // FNV-1a offset basis
    auto step_one = [&]() {
        world.step(dt);
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };
    if (render) { step_one(); capture_png(ren, start_png, W, H); }

    // 1) CHARGE: step until READY (the real recharge; RechargeTime*60*15 frames
    //    is a headless-fast countdown -- the --demo-naval 9000-frame precedent).
    bool reached_ready = false;
    const long charge_cap = (long)std::llround(p.recharge_secs * 15.0) + 100;
    while (frame < charge_cap && !world.sw_ready(0)) step_one();
    reached_ready = world.sw_ready(0);
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 1: post-charge
    {
        char b[144];
        std::snprintf(b, sizeof b, "[sw] charging recharge_secs=%.1f frames_to_ready=%ld ready=%s",
                      p.recharge_secs, frame, reached_ready ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 2) FIRE at the target cell; assert it fires + resets.
    const bool fired = world.fire_superweapon(0, cx, cy);
    const bool reset_ok = !world.sw_ready(0) && world.sw_charge_fraction(0) < 1.0;
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 2: post-fire
    {
        char b[128];
        std::snprintf(b, sizeof b, "[sw] fired target=(%d,%d) returned=%s reset_timer=%s",
                      cx, cy, fired ? "yes" : "no", reset_ok ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 3) DETONATE: count the in-radius kills + confirm own spared / far untouched.
    int killed = 0, damaged = 0;
    for (int idx : in_radius) {
        const ra2engine::Unit& u = world.units()[idx];
        if (u.hp <= 0 || !u.alive) ++killed;
        else if (u.hp < 100) ++damaged;
    }
    const bool own_spared = world.units()[own_idx].hp == own_hp0;
    const bool far_untouched = world.units()[far_idx].hp == far_hp0;
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 3: post-detonate
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[sw] detonate killed=%d damaged=%d in_radius=%d own_spared=%s far_untouched=%s",
                      killed, damaged, (int)in_radius.size(), own_spared ? "yes" : "no",
                      far_untouched ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 4) RECHARGE: a fixed window; the SW begins re-charging from 0.
    const long resume_until = frame + 150;
    while (frame < resume_until) step_one();
    h = superweapon_fold(h, world, 0, cx, cy);         // checkpoint 4: end
    r.hash = h;
    {
        char b[128];
        std::snprintf(b, sizeof b, "[sw] recharging charge_fraction=%.3f frame=%ld",
                      world.sw_charge_fraction(0), frame);
        r.witness.emplace_back(b);
    }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[sw] superweapon_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);

    // Wedge guards [spec §7]: the gate can never pass vacuously -- the SW must
    // reach READY AND fire_superweapon must return true AND >=1 in-radius kill.
    if (!reached_ready) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: never reached READY"); }
    if (!fired) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: fire_superweapon refused"); }
    if (killed < 1) { r.ok = false; r.witness.emplace_back("[sw] WEDGE: no in-radius kill"); }
    if (render) { capture_png(ren, end_png, W, H); }   // capture the already-rendered frame
    return r;
}

// --demo-superweapon: the CB-4 superweapon integration gate (12th permanent
// byte-identity gate). Mirrors run_demo_naval exactly: runs simulate_superweapon
// rendered (hidden window + start/end screenshots) then headless from a FRESH
// World, and byte-compares the two witness logs + hashes. Exit 0 iff both runs
// are byte-identical AND both ok. NO hardcoded hash literal -- compute-and-self-
// compare; the controller pins the observed value in the README gate table.
int run_demo_superweapon() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_superweapon_start.png";
    const std::string end_png = scratch + "demo_superweapon_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    std::vector<const VehicleAssets*> vlist;           // no voxel units in this gate

    // Assets carries no Rules; load rulesmd.ini here so resolve_superweapon can
    // read [NukeSpecial]/[NukePayload]/[NUKE] (the same source load_unit_assets
    // uses). Resolved once and passed to both sim runs for determinism.
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    ra2engine::Camera cam;
    if (pc) {
        const ra2engine::ScreenPt p =
            ra2engine::cell_to_screen(pc->x + 6, pc->y + 6, 0, 60, 30, 15);
        cam.x = W / 2 - p.x;
        cam.y = H / 2 - p.y;
    }

    DemoSuperweaponResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-superweapon)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            r1 = simulate_superweapon(a, ua, vlist, /*render=*/true, ren, &tc, &cam,
                                      W, H, start_png, end_png, rules);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    DemoSuperweaponResult r2 = simulate_superweapon(a, ua, vlist, /*render=*/false, nullptr,
                                                    nullptr, nullptr, W, H, "", "", rules);

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;

    std::cerr << "[demo-superweapon] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d superweapon_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// --- --demo-capture helpers (Engineer/Spy Capture phase) ---------------------

// FNV fold of the capture-phase state [spec §7]: per building (house, hp, alive,
// state); both houses' credits (index 2 neutral folded too); both houses'
// blackout_frames; the enemy house's SW ready flag (spy-reset proof); every
// unit's (cell_x, cell_y, alive, hp, veteran_rank, disguised). A NEW
// accumulator; world_hash + every prior *_hash UNTOUCHED. Folded at the demo's
// fixed checkpoints.
uint64_t capture_fold(uint64_t h, const ra2engine::World& world) {
    for (const ra2engine::Building& b : world.buildings()) {
        h = fnv_mix(h, static_cast<uint64_t>(b.house));
        h = fnv_mix(h, static_cast<uint64_t>(b.hp));
        h = fnv_mix(h, static_cast<uint64_t>(b.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(static_cast<int>(b.state)));
    }
    for (int hh = 0; hh < 3; ++hh) {
        h = fnv_mix(h, static_cast<uint64_t>(std::llround(world.house_credits(hh))));
        h = fnv_mix(h, static_cast<uint64_t>(world.blackout_frames(hh)));
    }
    h = fnv_mix(h, static_cast<uint64_t>(world.sw_ready(1) ? 1 : 0));
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_x));
        h = fnv_mix(h, static_cast<uint64_t>(u.cell_y));
        h = fnv_mix(h, static_cast<uint64_t>(u.alive ? 1 : 0));
        h = fnv_mix(h, static_cast<uint64_t>(u.hp));
        h = fnv_mix(h, static_cast<uint64_t>(u.veteran_rank));
        h = fnv_mix(h, static_cast<uint64_t>(u.disguised ? 1 : 0));
    }
    return h;
}

struct DemoCaptureResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the capture demo [spec §gate] (headless or rendered --
// bit-identical; no RNG in any capture path). Exercises the whole Engineer/Spy
// chain via DIRECT World:: calls (the --demo-* scripted precedent): engineer
// capture of an enemy building + a neutral CAOILD, the four spy infiltration
// effects, the produced-veteran chain, and the disguise/dog reveal. capture_fold
// is FNV-folded at fixed checkpoints; wedge guards force exit 1 if any effect
// fails to occur (the gate can never pass vacuously).
DemoCaptureResult simulate_capture(Assets& a, UnitAssets& ua,
                                   const std::vector<const VehicleAssets*>& vlist,
                                   bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                   const ra2engine::Camera* cam, int W, int H,
                                   const std::string& start_png, const std::string& end_png) {
    using P = ra2engine::World::InfiltratePurpose;
    DemoCaptureResult r;
    ra2engine::World world = build_world(a, ua, 0);   // count=0 -> no pre-spawned units

    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int bx = pc ? pc->x : 20;
    const int by = pc ? pc->y : 20;

    // 1) Base fragments. All instant (state Idle immediately). 1x1 foundations.
    //    Player (house 0): a ConYard + a launchable factory. Enemy (house 1): a
    //    refinery to capture, a second refinery to spy (cash), a power plant, a
    //    SW-granting building, a factory. Neutral (house 2): a CAOILD.
    world.add_building(0, bx,      by,     1, 1, /*house=*/0, 1000, 0, 0.0, true); // player ConYard
    const int player_fac  = world.add_building(0, bx, by + 3, 1, 1, /*house=*/0, 1000, 0, 0.0, true);
    const int enemy_refA  = world.add_building(0, bx + 2,  by, 1, 1, /*house=*/1, 500, 0, 0.0, true);
    const int enemy_refB  = world.add_building(0, bx + 4,  by, 1, 1, /*house=*/1, 500, 0, 0.0, true);
    const int enemy_power = world.add_building(0, bx + 6,  by, 1, 1, /*house=*/1, 500, 0, 0.0, true);
    const int enemy_sw    = world.add_building(0, bx + 8,  by, 1, 1, /*house=*/1, 500, 0, 0.0, true);
    world.buildings()[enemy_sw].grants_superweapon = true;
    const int enemy_fac   = world.add_building(0, bx + 10, by, 1, 1, /*house=*/1, 500, 0, 0.0, true);
    const int oil         = world.add_building(0, bx + 12, by, 1, 1, /*house=*/ra2engine::kNeutralHouse,
                                               500, 0, 0.0, true);
    world.register_tech_income(oil, /*amount=*/20, /*delay=*/100);   // [CAOILD]

    // Grant house 1 a Nuke (the spy-reset victim) BEFORE any stepping so the
    // rendered run's pre-step charges it identically to the headless run's first
    // charge-loop step (the superweapon-gate ordering -- keeps the reach-READY
    // frame count byte-identical across the two runs).
    std::array<int, 11> verses; verses.fill(100);
    world.grant_superweapon(1, ra2engine::SuperweaponType::Nuke, /*recharge_secs=*/10.0,
                            /*dmg=*/600, /*cell_spread=*/10.0, /*pmax=*/0.02, verses);

    auto count_owned = [&](int house) {
        int n = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.alive && b.state != ra2engine::Building::State::Dead && b.house == house) ++n;
        return n;
    };

    const double dt = 1.0 / 15.0;
    long frame = 0;
    uint64_t h = 0xcbf29ce484222325ull;               // FNV-1a offset basis
    auto step_one = [&]() {
        world.step(dt);
        ++frame;
        if (render) {
            SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
            SDL_RenderClear(ren);
            render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
        }
    };
    if (render) { step_one(); capture_png(ren, start_png, W, H); }
    h = capture_fold(h, world);                        // checkpoint 0: baseline

    // 2) Charge the granted SW to READY. No RNG; the oil is still neutral so tech
    //    income does NOT accrue yet (step_tech_structures skips a neutral owner).
    const long charge_cap = (long)std::llround(10.0 * 15.0) + 100;
    while (frame < charge_cap && !world.sw_ready(1)) step_one();
    const bool sw_charged = world.sw_ready(1);
    h = capture_fold(h, world);                        // checkpoint 1: SW charged
    {
        char b[128];
        std::snprintf(b, sizeof b, "[cap] enemy SW charged ready=%s frames=%ld",
                      sw_charged ? "yes" : "no", frame);
        r.witness.emplace_back(b);
    }

    // 3) Engineer captures an ENEMY building (ownership flip 1->0 + owned-count
    //    delta = buildability/power re-home proof).
    const int owned0_before = count_owned(0);
    const bool cap_ret = world.capture_building(enemy_refA, /*new_house=*/0, /*startup=*/0);
    const int owned0_after = count_owned(0);
    const bool captured_enemy = cap_ret && world.buildings()[enemy_refA].house == 0;
    const bool power_delta = owned0_after == owned0_before + 1;
    h = capture_fold(h, world);                        // checkpoint 2: enemy capture
    {
        char b[144];
        std::snprintf(b, sizeof b, "[cap] engineer captured enemy: house=%d owned0 %d->%d delta=%s",
                      world.buildings()[enemy_refA].house, owned0_before, owned0_after,
                      power_delta ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 4) Engineer captures the NEUTRAL CAOILD: ownership 2->0 + ProduceCashStartup
    //    1000 grant, then a +20 income tick after 100 frames.
    const double cred0_pre_oil = world.house_credits(0);
    const bool oil_ret = world.capture_building(oil, /*new_house=*/0, /*startup=*/1000);
    const double cred0_post_startup = world.house_credits(0);
    const bool captured_neutral = oil_ret && world.buildings()[oil].house == 0 &&
                                  std::llround(cred0_post_startup - cred0_pre_oil) == 1000;
    const long income_until = frame + 100;
    while (frame < income_until) step_one();           // exactly one delay period
    const double cred0_post_income = world.house_credits(0);
    const bool income_ticked = std::llround(cred0_post_income - cred0_post_startup) == 20;
    h = capture_fold(h, world);                        // checkpoint 3: neutral capture + income
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[cap] captured CAOILD: house=%d startup +%ld income +%ld (100 frames)",
                      world.buildings()[oil].house,
                      (long)std::llround(cred0_post_startup - cred0_pre_oil),
                      (long)std::llround(cred0_post_income - cred0_post_startup));
        r.witness.emplace_back(b);
    }

    // 5a) Spy infiltrates an ENEMY refinery -> cash moves 0.5x victim credits.
    world.grant_credits_for_test(1, 1000.0);           // seed the victim's coffers
    const double spy_cred0_pre = world.house_credits(0);
    const double spy_cred1_pre = world.house_credits(1);
    world.infiltrate_building(enemy_refB, /*spy_house=*/0, P::Refinery, 0.5, 1000);
    const double spy_cred0_post = world.house_credits(0);
    const double spy_cred1_post = world.house_credits(1);
    const bool cash_moved = std::llround(spy_cred1_pre - spy_cred1_post) == 500 &&
                            std::llround(spy_cred0_post - spy_cred0_pre) == 500;
    h = capture_fold(h, world);                        // checkpoint 4: cash steal
    {
        char b[160];
        std::snprintf(b, sizeof b, "[cap] spy refinery: house1 %ld->%ld house0 %ld->%ld (0.5x)",
                      (long)std::llround(spy_cred1_pre), (long)std::llround(spy_cred1_post),
                      (long)std::llround(spy_cred0_pre), (long)std::llround(spy_cred0_post));
        r.witness.emplace_back(b);
    }

    // 5b) Spy infiltrates an ENEMY power plant -> blackout timer armed to 1000.
    world.infiltrate_building(enemy_power, /*spy_house=*/0, P::PowerPlant, 0.5, 1000);
    const bool blackout_armed = world.blackout_frames(1) == 1000;
    h = capture_fold(h, world);                        // checkpoint 5: blackout
    {
        char b[96];
        std::snprintf(b, sizeof b, "[cap] spy power plant: blackout_frames(1)=%d",
                      world.blackout_frames(1));
        r.witness.emplace_back(b);
    }

    // 5c) Spy infiltrates the ENEMY SW building -> the charged SW resets.
    const bool sw_was_ready = world.sw_ready(1);
    world.infiltrate_building(enemy_sw, /*spy_house=*/0, P::Superweapon, 0.5, 1000);
    const bool sw_reset = sw_was_ready && !world.sw_ready(1);
    h = capture_fold(h, world);                        // checkpoint 6: SW reset
    {
        char b[112];
        std::snprintf(b, sizeof b, "[cap] spy SW building: ready %s->%s (reset)",
                      sw_was_ready ? "yes" : "no", world.sw_ready(1) ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // 5d) Spy infiltrates the ENEMY factory -> produced-veteran flag for house 0;
    //     the next unit house 0 launches spawns at veteran_rank 1.
    world.infiltrate_building(enemy_fac, /*spy_house=*/0, P::Factory, 0.5, 1000);
    const bool vet_flag = world.produced_veteran(0);
    ra2engine::BuildingTypeDef fdef; fdef.id = "GAWEAP"; fdef.fw = fdef.fh = 1;
    ra2engine::UnitCombat pc_stats;
    pc_stats.image = "E1"; pc_stats.strength = 100; pc_stats.armor = "none"; pc_stats.damage = 40;
    const int vet_idx = world.launch_unit(player_fac, fdef, pc_stats);
    const bool produced_vet = vet_flag && vet_idx >= 0 && world.units()[vet_idx].veteran_rank == 1;
    h = capture_fold(h, world);                        // checkpoint 7: produced veteran
    {
        char b[160];
        std::snprintf(b, sizeof b, "[cap] spy factory: produced_veteran(0)=%s launched idx=%d rank=%d",
                      vet_flag ? "yes" : "no", vet_idx,
                      vet_idx >= 0 ? world.units()[vet_idx].veteran_rank : -1);
        r.witness.emplace_back(b);
    }

    // 6) Disguise: a fresh weaponless house-0 spy disguised as an enemy E2 passes
    //    a non-detector enemy (stays disguised), then a DOG reveals it. Placed
    //    well away from the veteran/base so no cross auto-acquire perturbs it.
    ra2engine::UnitCombat spyc;
    spyc.image = "SPY"; spyc.strength = 100; spyc.armor = "flak"; spyc.damage = 0; spyc.range = 0.0;
    ra2engine::Unit& spy2 = world.add_combat_unit(bx + 6, by + 10, /*house=*/0, spyc);
    const int spy2_idx = (int)world.units().size() - 1;
    (void)spy2;
    world.set_disguise(spy2_idx, "E2", /*as_house=*/1);
    const bool disguised_set = world.units()[spy2_idx].disguised;
    // Non-detector enemy adjacent (within kDetectDisguiseRange, but never reveals).
    ra2engine::UnitCombat gic;
    gic.image = "E1"; gic.strength = 100; gic.armor = "none"; gic.damage = 0; gic.range = 0.0;
    world.add_combat_unit(bx + 6, by + 11, /*house=*/1, gic);
    step_one();                                        // step_tech_structures detection pass
    const bool stayed_disguised = world.units()[spy2_idx].disguised;
    h = capture_fold(h, world);                        // checkpoint 8: non-detector passed
    // A DOG (DetectDisguise) within range reveals the spy.
    ra2engine::UnitCombat dogc;
    dogc.image = "DOG"; dogc.strength = 100; dogc.armor = "none"; dogc.damage = 0; dogc.range = 0.0;
    world.add_combat_unit(bx + 6, by + 9, /*house=*/1, dogc);
    world.units().back().detect_disguise = true;
    step_one();                                        // detection pass with the dog present
    const bool dog_revealed = disguised_set && stayed_disguised && !world.units()[spy2_idx].disguised;
    h = capture_fold(h, world);                        // checkpoint 9: dog reveal
    {
        char b[144];
        std::snprintf(b, sizeof b, "[cap] disguise: set=%s past_nondetector=%s dog_revealed=%s",
                      disguised_set ? "yes" : "no", stayed_disguised ? "yes" : "no",
                      world.units()[spy2_idx].disguised ? "still-disguised" : "revealed");
        r.witness.emplace_back(b);
    }

    r.hash = h;

    // Wedge guards [spec §gate]: the gate can never pass vacuously.
    if (!captured_enemy)   { r.ok = false; r.witness.emplace_back("[cap] WEDGE: enemy capture did not flip ownership"); }
    if (!captured_neutral) { r.ok = false; r.witness.emplace_back("[cap] WEDGE: neutral CAOILD capture/startup failed"); }
    if (!income_ticked)    { r.ok = false; r.witness.emplace_back("[cap] WEDGE: CAOILD income did not tick +20"); }
    if (!power_delta)      { r.ok = false; r.witness.emplace_back("[cap] WEDGE: capture produced no owned-count delta"); }
    if (!cash_moved)       { r.ok = false; r.witness.emplace_back("[cap] WEDGE: cash-steal moved no credits"); }
    if (!blackout_armed)   { r.ok = false; r.witness.emplace_back("[cap] WEDGE: blackout did not arm"); }
    if (!sw_reset)         { r.ok = false; r.witness.emplace_back("[cap] WEDGE: SW did not reset"); }
    if (!produced_vet)     { r.ok = false; r.witness.emplace_back("[cap] WEDGE: produced unit is not a veteran"); }
    if (!dog_revealed)     { r.ok = false; r.witness.emplace_back("[cap] WEDGE: dog did not reveal the spy"); }

    char hb[64];
    std::snprintf(hb, sizeof hb, "[cap] capture_hash=%016llx",
                  static_cast<unsigned long long>(r.hash));
    r.witness.emplace_back(hb);
    if (render) { capture_png(ren, end_png, W, H); }   // capture the already-rendered frame
    return r;
}

// --demo-capture: the Engineer/Spy Capture integration gate (14th permanent
// byte-identity gate). Runs simulate_capture rendered (hidden window + start/end
// screenshots) then headless from a FRESH World, and byte-compares the two
// witness logs + capture hashes. NO hardcoded hash literal -- compute-and-self-
// compare; the controller pins the observed value in the README gate table.
int run_demo_capture() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_capture_start.png";
    const std::string end_png = scratch + "demo_capture_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    std::vector<const VehicleAssets*> vlist;            // no voxel units in this gate

    DemoCaptureResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-capture)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_capture(a, ua, vlist, /*render=*/true, ren, &tc, &cam,
                                  W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    }

    DemoCaptureResult r2 = simulate_capture(a, ua, vlist, /*render=*/false, nullptr,
                                            nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;
    std::cerr << "[demo-capture] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d capture_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// FNV fold of the shroud-phase state [spec §Testing]: per house h in 0..2, the
// COUNT of explored cells and the COUNT of visible cells (order-stable scalar
// summaries of the grids); plus a seen_by_mask sample -- the mask of every
// alive unit (order = units_ order). A NEW accumulator; world_hash + every prior
// *_hash UNTOUCHED. Folded at the demo's fixed checkpoints.
uint64_t shroud_fold(uint64_t h, const ra2engine::World& world) {
    const int gw = world.grid_w(), gh = world.grid_h();
    for (int hh = 0; hh < 3; ++hh) {
        uint64_t nexp = 0, nvis = 0;
        for (int y = 0; y < gh; ++y)
            for (int x = 0; x < gw; ++x) {
                // NOTE: with shroud ENABLED, explored_at/visible_at read the real
                // grid; a fully-shrouded cell returns false. (Empty-grid all-true
                // only applies with shroud OFF, which this gate never uses.)
                if (world.explored_at(hh, x, y)) ++nexp;
                if (world.visible_at(hh, x, y))  ++nvis;
            }
        h = fnv_mix(h, nexp);
        h = fnv_mix(h, nvis);
    }
    for (const ra2engine::Unit& u : world.units()) {
        if (!u.alive) continue;
        h = fnv_mix(h, (uint64_t)u.seen_by_mask);
    }
    return h;
}

struct DemoShroudResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the shroud/fog demo [spec §Testing] (headless or rendered
// -- bit-identical; no RNG in any shroud path). Drives the whole Shroud/Fog
// feature via DIRECT World:: calls (the --demo-* scripted precedent): initial
// reveal around a starting unit, movement growing the explored map, sticky fog
// after the source leaves, a fogged enemy that is not seen_by house 0, spy
// radar-sabotage clearing the victim's map, and a SpySat building revealing the
// whole map. shroud_fold is FNV-folded at six fixed checkpoints; wedge guards
// force exit 1 if any invariant fails (the gate can never pass vacuously).
DemoShroudResult simulate_shroud(Assets& a, UnitAssets& ua,
                                 const std::vector<const VehicleAssets*>& vlist,
                                 bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                 const ra2engine::Camera* cam, int W, int H,
                                 const std::string& start_png, const std::string& end_png) {
    using P = ra2engine::World::InfiltratePurpose;
    DemoShroudResult r;
    ra2engine::World world = build_world(a, ua, 0);    // count=0 -> no pre-spawned units
    world.enable_shroud(true);

    const int gw = world.grid_w(), gh = world.grid_h();
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

    // Player start cell (clamped one cell inside the border so a full sight disc
    // and an 8-cell march both stay on the grid).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int bx = clampi(pc ? pc->x : 20, 1, gw - 2);
    const int by = clampi(pc ? pc->y : 20, 1, gh - 2);
    // March toward the interior (whichever horizontal half has room). Far corner
    // for the enemy is chosen on the OPPOSITE side so it is well beyond sight 5.
    const int mvdir = (bx < gw / 2) ? +1 : -1;
    const int steps = 8;
    const int ex = (bx < gw / 2) ? clampi(gw - 3, 1, gw - 2) : clampi(2, 1, gw - 2);
    const int ey = (by < gh / 2) ? clampi(gh - 3, 1, gh - 2) : clampi(2, 1, gh - 2);
    const int sticky_x = clampi(bx + mvdir * steps, 0, gw - 1);
    const int sticky_y = by;

    auto explored_count = [&](int house) {
        int n = 0;
        for (int y = 0; y < gh; ++y)
            for (int x = 0; x < gw; ++x)
                if (world.explored_at(house, x, y)) ++n;
        return n;
    };
    auto visible_count = [&](int house) {
        int n = 0;
        for (int y = 0; y < gh; ++y)
            for (int x = 0; x < gw; ++x)
                if (world.visible_at(house, x, y)) ++n;
        return n;
    };

    auto render_frame = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
    };

    uint64_t h = 0xcbf29ce484222325ull;               // FNV-1a offset basis

    // --- Stage 1: initial reveal around a starting unit -----------------------
    ra2engine::UnitCombat pc_stats;
    pc_stats.image = "GI"; pc_stats.strength = 100; pc_stats.armor = "none";
    pc_stats.damage = 0; pc_stats.range = 0.0; pc_stats.sight = 5;
    world.add_combat_unit(bx, by, /*house=*/0, pc_stats);
    const int player_idx = (int)world.units().size() - 1;
    ra2engine::UnitCombat en_stats;
    en_stats.image = "CONS"; en_stats.strength = 100; en_stats.armor = "none";
    en_stats.damage = 0; en_stats.range = 0.0; en_stats.sight = 5;
    world.add_combat_unit(ex, ey, /*house=*/1, en_stats);
    const int enemy_idx = (int)world.units().size() - 1;
    world.step_visibility();                           // stamp initial discs + masks
    const int exp0_initial = explored_count(0);
    const bool initial_revealed = exp0_initial > 0;
    h = shroud_fold(h, world);                          // checkpoint A: initial reveal
    render_frame();
    if (render) capture_png(ren, start_png, W, H);      // start screenshot
    {
        char b[128];
        std::snprintf(b, sizeof b, "[shroud] stage1 initial reveal: house0 explored=%d visible=%d",
                      exp0_initial, visible_count(0));
        r.witness.emplace_back(b);
    }

    // --- Stage 2: movement grows the explored map -----------------------------
    const int exp0_before_move = explored_count(0);
    for (int k = 1; k <= steps; ++k) {
        const int nx = clampi(bx + mvdir * k, 0, gw - 1);
        world.units()[player_idx].cell_x = nx;
        world.units()[player_idx].cell_y = by;
        world.units()[player_idx].fx = (double)nx;
        world.units()[player_idx].fy = (double)by;
        world.step_visibility();
    }
    const int exp0_after_move = explored_count(0);
    const bool movement_grew = exp0_after_move > exp0_before_move;
    h = shroud_fold(h, world);                          // checkpoint B: movement growth
    {
        char b[128];
        std::snprintf(b, sizeof b, "[shroud] stage2 movement: house0 explored %d->%d grew=%s",
                      exp0_before_move, exp0_after_move, movement_grew ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 3: fog stickiness (source leaves -> explored persists, visible clears)
    world.units()[player_idx].cell_x = bx;
    world.units()[player_idx].cell_y = by;
    world.units()[player_idx].fx = (double)bx;
    world.units()[player_idx].fy = (double)by;
    world.step_visibility();
    const int exp0_after_return = explored_count(0);
    const bool sticky_explored = world.explored_at(0, sticky_x, sticky_y);
    const bool sticky_not_visible = !world.visible_at(0, sticky_x, sticky_y);
    const bool fog_sticky = (exp0_after_return >= exp0_after_move) &&
                            sticky_explored && sticky_not_visible;
    h = shroud_fold(h, world);                          // checkpoint C: fog stickiness
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[shroud] stage3 fog sticky: house0 explored %d->%d sticky(%d,%d) explored=%s visible=%s",
                      exp0_after_move, exp0_after_return, sticky_x, sticky_y,
                      sticky_explored ? "yes" : "no",
                      world.visible_at(0, sticky_x, sticky_y) ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 4: fogged enemy is not seen_by house 0 -------------------------
    world.units()[enemy_idx].cell_x = ex;              // (re)place the enemy in a fogged cell
    world.units()[enemy_idx].cell_y = ey;
    world.units()[enemy_idx].fx = (double)ex;
    world.units()[enemy_idx].fy = (double)ey;
    world.step_visibility();                            // restamp seen_by_mask
    const std::uint8_t enemy_mask = world.units()[enemy_idx].seen_by_mask;
    const bool enemy_unseen = (enemy_mask & 0x01) == 0;
    h = shroud_fold(h, world);                          // checkpoint D: fogged enemy mask
    {
        char b[144];
        std::snprintf(b, sizeof b,
                      "[shroud] stage4 fogged enemy: enemy_idx=%d seen_by_mask=0x%02x house0_bit=%s",
                      enemy_idx, (unsigned)enemy_mask, enemy_unseen ? "clear" : "SET");
        r.witness.emplace_back(b);
    }

    // --- Stage 5: spy-radar sabotage clears the victim's map -------------------
    // The enemy already explored around its scout (stage 4 step_visibility). A
    // house-0 spy infiltrates the ENEMY radar (RadarLab) -> reshroud_house(1).
    const int enemy_radar = world.add_building(0, ex, clampi(ey + 2, 0, gh - 1), 1, 1,
                                               /*house=*/1, 500, 0, 0.0, /*instant=*/true);
    const int exp1_before_sabotage = explored_count(1);
    world.infiltrate_building(enemy_radar, /*spy_house=*/0, P::RadarLab, 0.5, 1000);
    const int exp1_after_sabotage = explored_count(1);
    const bool sabotage_cleared = exp1_after_sabotage == 0;
    h = shroud_fold(h, world);                          // checkpoint E: sabotage
    {
        char b[144];
        std::snprintf(b, sizeof b,
                      "[shroud] stage5 spy-radar sabotage: house1 explored %d->%d cleared=%s",
                      exp1_before_sabotage, exp1_after_sabotage, sabotage_cleared ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 6: SpySat reveal-all -------------------------------------------
    // House 0 owns a grants_map_reveal building -> step_visibility latches the
    // whole map explored (and visible) for house 0.
    world.add_building(0, bx, clampi(by + 2, 0, gh - 1), 1, 1, /*house=*/0, 500, 0, 0.0,
                       /*instant=*/true, /*sight=*/4, /*grants_map_reveal=*/true);
    world.step_visibility();
    const int exp0_reveal = explored_count(0);
    const bool reveal_all_full = exp0_reveal == gw * gh;
    h = shroud_fold(h, world);                          // checkpoint F: reveal-all
    {
        char b[144];
        std::snprintf(b, sizeof b,
                      "[shroud] stage6 reveal-all: house0 explored=%d full=%d (%dx%d) full=%s",
                      exp0_reveal, gw * gh, gw, gh, reveal_all_full ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    r.hash = h;

    // Wedge guards [spec §Testing]: exit 1 unless each invariant holds.
    if (!initial_revealed)   { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: initial reveal marked 0 cells explored"); }
    if (!movement_grew)      { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: movement did not grow explored"); }
    if (!fog_sticky)         { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: fog did not persist after the source left"); }
    if (!enemy_unseen)       { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: fogged enemy was still seen_by house 0"); }
    if (!sabotage_cleared)   { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: spy-radar sabotage did not clear the victim's map"); }
    if (!reveal_all_full)    { r.ok = false; r.witness.emplace_back("[shroud] WEDGE: reveal-all did not set explored to full"); }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[shroud] shroud_hash=%016llx", (unsigned long long)r.hash);
    r.witness.emplace_back(hb);
    render_frame();
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-shroud: the Shroud/Fog-of-War integration gate (15th permanent byte-
// identity gate). Runs simulate_shroud rendered (hidden window + start/end
// screenshots) then headless from a FRESH World, and byte-compares the two
// witness logs + shroud hashes. NO hardcoded hash literal -- compute-and-self-
// compare; the controller pins the observed value in the README gate table.
int run_demo_shroud() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_shroud_start.png";
    const std::string end_png = scratch + "demo_shroud_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    std::vector<const VehicleAssets*> vlist;            // no voxel units in this gate

    DemoShroudResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-shroud)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_shroud(a, ua, vlist, /*render=*/true, ren, &tc, &cam,
                                 W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    }

    DemoShroudResult r2 = simulate_shroud(a, ua, vlist, /*render=*/false, nullptr,
                                          nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;
    std::cerr << "[demo-shroud] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d shroud_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// FNV fold of the multi-house state [spec §Testing]: for each house h -> alive
// building count, alive unit count, house_defeated flag; for each ordered pair
// (a,b) -> is_enemy(a,b) sampled; plus winning_team(). A NEW accumulator;
// world_hash + every prior *_hash UNTOUCHED. Folded at the demo's fixed checkpoints.
uint64_t multihouse_fold(uint64_t h, const ra2engine::World& world) {
    const int n = world.house_count();
    for (int hh = 0; hh < n; ++hh) {
        uint64_t nb = 0, nu = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.alive && b.state != ra2engine::Building::State::Dead && b.house == hh) ++nb;
        for (const ra2engine::Unit& u : world.units())
            if (u.alive && u.die_time < 0.0 && u.house == hh) ++nu;
        h = fnv_mix(h, nb);
        h = fnv_mix(h, nu);
        h = fnv_mix(h, (uint64_t)(world.house_defeated(hh) ? 1 : 0));
    }
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b)
            h = fnv_mix(h, (uint64_t)(world.is_enemy(a, b) ? 1 : 0));
    h = fnv_mix(h, (uint64_t)(int64_t)world.winning_team());
    return h;
}

struct DemoMultihouseResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the multi-house / Alliances demo [spec §Testing] (headless
// or rendered -- bit-identical; no RNG in any path). Drives the whole feature via
// DIRECT World:: calls (the --demo-* scripted precedent): three combatant houses
// in two alliance-teams (houses 0 + 2 on team 0 vs house 1 on team 1) plus a
// neutral house (3) owning a structure. The scenario demonstrates, at five fixed
// checkpoints folded into multihouse_fold: (A) configured alliances,
// (B) allies/neutrals are NOT auto-acquired while an enemy IS, (C) AllyReveal
// shares an ally scout's revealed sight, (D) an eliminated house is marked
// defeated, and (E) the last team standing wins while the neutral never wins or
// loses. Wedge guards force exit 1 if any invariant fails (the gate can never
// pass vacuously). All combat units carry Damage=0 so auto-acquire happens but
// nobody dies -> the only elimination is the scripted kill_building, keeping the
// two runs byte-identical.
// [RE: skirmish.md §3, D2] Build a ready-to-run World from a SkirmishConfig.
// Modeled 1:1 on simulate_multihouse + run_interactive's setup block. Calls the
// pure configure_skirmish_houses (skirmish.cpp) for the house table + toggles,
// then does the asset-dependent spawn + full AI config. PURELY ADDITIVE: no gate
// calls it (the 16 gates build worlds directly); world_hash untouched.
ra2engine::World launch_skirmish(const ra2engine::SkirmishConfig& cfg,
                                 Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                 const Rules& rules) {
    ra2engine::World world = build_world(a, ua, 0);          // count=0 -> launcher spawns everything
    ra2engine::configure_skirmish_houses(world, cfg);       // house table + toggles + credit seed + minimal AI
    const ra2assets::MapCell* sc = ra2engine::pick_start_cell(a.map);
    const int gw = world.grid_w(), gh = world.grid_h();
    (void)gh;
    // Per-combatant spawn: a base at a fanned-out cell + unit_count combat units
    // (+ MCV placeholder) ring-scanned like the existing transport/engineer spawns.
    for (int i = 0; i < (int)cfg.slots.size(); ++i) {
        const ra2engine::SkirmishSlot& s = cfg.slots[i];
        if (!s.combatant) continue;
        const int bx = sc ? sc->x + (i * 14) % std::max(1, gw - 14) : 6 + i * 6;
        const int by = sc ? sc->y : 6 + i * 4;
        if (ba.ok) spawn_base(world, ba, /*house=*/i, bx, by);
        // unit_count starting units (ring-scan for free walkable cells).
        ra2engine::UnitCombat uc = resolve_combat_unit(
            rules, s.side == ra2engine::World::Side::Soviet ? "E2" : "E1").combat;
        int placed = 0;
        for (int r = 1; r <= 8 && placed < cfg.unit_count; ++r)
            for (int dy = -r; dy <= r && placed < cfg.unit_count; ++dy)
                for (int dx = -r; dx <= r && placed < cfg.unit_count; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    const int x = bx + dx, y = by + dy;
                    if (world.walkable(x, y) && world.unit_at_cell(x, y) < 0) {
                        world.add_combat_unit(x, y, /*house=*/i, uc);
                        ++placed;
                    }
                }
        // Full roster-backed AI config OVER-WRITES the minimal one from the core.
        if (s.is_ai && ba.ok)
            world.enable_ai_house(i, make_ai_config(ba, rules, s.diff, cfg.starting_cash));
        // Superweapon grant per slot.
        if (cfg.superweapons) {
            const SuperweaponParams swp = resolve_superweapon(rules);
            world.grant_superweapon(i, ra2engine::SuperweaponType::Nuke, swp.recharge_secs,
                                    swp.damage, swp.cell_spread, swp.percent_at_max, swp.verses);
        }
    }
    return world;
}

DemoMultihouseResult simulate_multihouse(Assets& a, UnitAssets& ua,
                                         const std::vector<const VehicleAssets*>& vlist,
                                         bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                         const ra2engine::Camera* cam, int W, int H,
                                         const std::string& start_png, const std::string& end_png) {
    DemoMultihouseResult r;
    ra2engine::World world = build_world(a, ua, 0);    // count=0 -> no pre-spawned units

    // --- Stage 1: configure houses + alliances; enable shroud/AllyReveal/victory
    world.set_house(0, {ra2engine::World::Side::Allied,  5, "Americans", 0, true});
    world.set_house(1, {ra2engine::World::Side::Soviet,  1, "Russians",  1, true});
    world.set_house(2, {ra2engine::World::Side::Allied,  6, "British",   0, true});   // ally of 0
    world.set_house(3, {ra2engine::World::Side::Neutral, 3, "Civilian",  3, false});  // neutral
    world.enable_shroud(true);
    world.enable_ally_reveal(true);
    world.enable_victory(true, /*short_game=*/true);

    const int gw = world.grid_w(), gh = world.grid_h();
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

    // Base cell (clamped well inside the border so all five unit cells + four
    // building cells stay on the grid).
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int bx = clampi(pc ? pc->x : 20, 6, gw - 7);
    const int by = clampi(pc ? pc->y : 20, 6, gh - 7);
    // House-2 far scout cell: the OPPOSITE side of the map so neither house 0's
    // nor house 1's own sight reaches it -- house 0 sees it ONLY via AllyReveal.
    const int fx = (bx < gw / 2) ? clampi(gw - 4, 0, gw - 1) : clampi(3, 0, gw - 1);
    const int fy = (by < gh / 2) ? clampi(gh - 4, 0, gh - 1) : clampi(3, 0, gh - 1);

    auto mk = [](const char* img, int strength, double range, int sight) {
        ra2engine::UnitCombat c;
        c.image = img; c.strength = strength; c.armor = "none";
        c.damage = 0; c.rof = 10; c.range = range; c.sight = sight;   // Damage=0 -> no deaths
        return c;
    };

    // Combat units. u0 (house 0) sees + ranges the ally (house 2), the enemy
    // (house 1) and the neutral (house 3); only the enemy is a valid auto-target.
    world.add_combat_unit(bx,     by,     /*house=*/0, mk("GI",   100, 5.0, 5));
    const int u0_idx = (int)world.units().size() - 1;
    world.add_combat_unit(bx - 2, by,     /*house=*/2, mk("GI",   100, 3.0, 3));  // ally, in u0 range
    world.add_combat_unit(bx + 3, by,     /*house=*/1, mk("CONS", 100, 3.0, 3));  // enemy, in u0 range
    world.add_combat_unit(bx,     by - 2, /*house=*/3, mk("GI",   100, 0.0, 2));  // neutral, in u0 range
    world.add_combat_unit(fx,     fy,     /*house=*/2, mk("GI",   100, 0.0, 5));  // ally far scout (AllyReveal)

    // A ConYard-ish building per combatant + the neutral's structure.
    world.add_building(0, bx,     by + 2, 1, 1, /*house=*/0, 500, 0, 0.0, /*instant=*/true);
    world.add_building(0, bx - 2, by + 2, 1, 1, /*house=*/2, 500, 0, 0.0, /*instant=*/true);
    const int b1_idx =
        world.add_building(0, bx + 3, by + 2, 1, 1, /*house=*/1, 500, 0, 0.0, /*instant=*/true);
    world.add_building(0, bx,     by - 4, 1, 1, /*house=*/3, 500, 0, 0.0, /*instant=*/true);  // neutral

    auto render_frame = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
    };

    uint64_t h = 0xcbf29ce484222325ull;               // FNV-1a offset basis

    world.step_visibility();                           // stamp initial discs + masks
    h = multihouse_fold(h, world);                     // checkpoint A: configured + spawned
    render_frame();
    if (render) capture_png(ren, start_png, W, H);      // start screenshot
    {
        char b[192];
        std::snprintf(b, sizeof b,
                      "[mh] stage1 configure: houses=%d team(0,1,2,3)=(%d,%d,%d,%d) allied(0,2)=%s enemy(0,1)=%s house3=%s",
                      world.house_count(), world.house_team(0), world.house_team(1),
                      world.house_team(2), world.house_team(3),
                      world.are_allied(0, 2) ? "yes" : "no",
                      world.is_enemy(0, 1) ? "yes" : "no",
                      world.house_combatant(3) ? "combatant" : "noncombatant");
        r.witness.emplace_back(b);
    }

    // --- Stage 2: allies/neutrals are NOT auto-acquired; the enemy IS ----------
    bool enemy_acquired = false;
    bool ally_or_neutral_targeted = false;
    const int steps = 30;
    for (int k = 0; k < steps; ++k) {
        world.step(1.0 / 15.0);                         // 15Hz logic frames
        for (const ra2engine::Unit& u : world.units()) {
            if (!u.alive || u.house != 0 || u.target < 0) continue;
            const int th = world.units()[u.target].house;
            if (th == 1) enemy_acquired = true;
            if (th == 2 || th == 3) ally_or_neutral_targeted = true;
        }
    }
    const bool allies_dont_fight = !ally_or_neutral_targeted;
    h = multihouse_fold(h, world);                     // checkpoint B: acquisition
    {
        char b[192];
        const int u0t = world.units()[u0_idx].target;
        std::snprintf(b, sizeof b,
                      "[mh] stage2 acquire: house0 u%d target=%d target_house=%d enemy_acquired=%s allies_dont_fight=%s",
                      u0_idx, u0t, u0t >= 0 ? world.units()[u0t].house : -1,
                      enemy_acquired ? "yes" : "no", allies_dont_fight ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 3: AllyReveal shares house 2's far-scout sight with house 0 -----
    world.step_visibility();
    const bool see0 = world.visible_at(0, fx, fy);      // ally -> shared
    const bool see1 = world.visible_at(1, fx, fy);      // enemy -> NOT shared
    const bool see2 = world.visible_at(2, fx, fy);      // scout's own house
    const bool ally_reveal_shared = see2 && see0 && !see1;
    h = multihouse_fold(h, world);                     // checkpoint C: ally reveal
    {
        char b[192];
        std::snprintf(b, sizeof b,
                      "[mh] stage3 ally-reveal: scout(%d,%d) vis house2=%s house0(ally)=%s house1(enemy)=%s shared=%s",
                      fx, fy, see2 ? "yes" : "no", see0 ? "yes" : "no", see1 ? "yes" : "no",
                      ally_reveal_shared ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 4: destroy house 1's last building -> defeat detected -----------
    if (b1_idx >= 0 && b1_idx < (int)world.buildings().size())
        ra2engine::kill_building(world.buildings()[b1_idx]);
    world.step_victory();
    const bool defeat_detected = world.house_defeated(1);
    h = multihouse_fold(h, world);                     // checkpoint D: defeat
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[mh] stage4 defeat: killed b%d house1_defeated=%s house0_defeated=%s house2_defeated=%s",
                      b1_idx, defeat_detected ? "yes" : "no",
                      world.house_defeated(0) ? "yes" : "no", world.house_defeated(2) ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    // --- Stage 5: last team standing (team 0) wins; neutral never wins/loses ---
    world.step_victory();
    const int wteam = world.winning_team();
    const bool neutral_safe = !world.house_defeated(3);
    const bool team_wins = (wteam == 0) && neutral_safe;
    h = multihouse_fold(h, world);                     // checkpoint E: team wins
    {
        char b[176];
        std::snprintf(b, sizeof b,
                      "[mh] stage5 victory: winning_team=%d neutral(3)_defeated=%s team_wins=%s",
                      wteam, world.house_defeated(3) ? "yes" : "no", team_wins ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    r.hash = h;

    // Wedge guards [spec §Testing]: exit 1 unless each invariant holds.
    if (!allies_dont_fight)  { r.ok = false; r.witness.emplace_back("[mh] WEDGE: an allied/neutral unit was auto-acquired"); }
    if (!enemy_acquired)     { r.ok = false; r.witness.emplace_back("[mh] WEDGE: the enemy team was never auto-acquired"); }
    if (!ally_reveal_shared) { r.ok = false; r.witness.emplace_back("[mh] WEDGE: AllyReveal did not share the scout's sight"); }
    if (!defeat_detected)    { r.ok = false; r.witness.emplace_back("[mh] WEDGE: eliminated house was not marked defeated"); }
    if (!team_wins)          { r.ok = false; r.witness.emplace_back("[mh] WEDGE: last team standing did not win"); }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[mh] multihouse_hash=%016llx", (unsigned long long)r.hash);
    r.witness.emplace_back(hb);
    render_frame();
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}

// --demo-multihouse: the Multi-house / Alliances integration gate (16th
// permanent byte-identity gate). Runs simulate_multihouse rendered (hidden
// window + start/end screenshots) then headless from a FRESH World, and byte-
// compares the two witness logs + multihouse hashes. NO hardcoded hash literal
// -- compute-and-self-compare; the controller pins the observed value in the
// README gate table.
int run_demo_multihouse() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_multihouse_start.png";
    const std::string end_png = scratch + "demo_multihouse_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    std::vector<const VehicleAssets*> vlist;            // no voxel units in this gate

    DemoMultihouseResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-multihouse)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_multihouse(a, ua, vlist, /*render=*/true, ren, &tc, &cam,
                                     W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    }

    DemoMultihouseResult r2 = simulate_multihouse(a, ua, vlist, /*render=*/false, nullptr,
                                                  nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;
    std::cerr << "[demo-multihouse] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d multihouse_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
    return bad ? 1 : 0;
}

// FNV fold of the skirmish-launch state [spec §Testing]: for each house h ->
// alive building count, alive unit count, ai_enabled flag, house_credits; for
// each ordered pair (a,b) -> is_enemy(a,b); plus house_count + winning_team. A
// NEW accumulator; world_hash + every prior *_hash UNTOUCHED. Order-stable
// scalars only (no pointers/RNG/wall-clock/unordered iteration) -> rendered ==
// headless. Folded at the demo's fixed checkpoints.
uint64_t skirmish_fold(uint64_t h, const ra2engine::World& world) {
    const int n = world.house_count();
    h = fnv_mix(h, (uint64_t)n);
    for (int hh = 0; hh < n; ++hh) {
        uint64_t nb = 0, nu = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.alive && b.state != ra2engine::Building::State::Dead && b.house == hh) ++nb;
        for (const ra2engine::Unit& u : world.units())
            if (u.alive && u.die_time < 0.0 && u.house == hh) ++nu;
        h = fnv_mix(h, nb);
        h = fnv_mix(h, nu);
        h = fnv_mix(h, (uint64_t)(world.ai_enabled(hh) ? 1 : 0));
        h = fnv_mix(h, (uint64_t)(int64_t)std::llround(world.house_credits(hh)));
    }
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b)
            h = fnv_mix(h, (uint64_t)(world.is_enemy(a, b) ? 1 : 0));
    h = fnv_mix(h, (uint64_t)(int64_t)world.winning_team());
    return h;
}

struct DemoSkirmishResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the Skirmish-launch demo [RE-GATE: skirmish.md
// §Adjudication item 4] (headless or rendered -- bit-identical; no RNG in any
// path). Drives the whole SkirmishConfig -> launch_skirmish -> World path with
// the ADJUDICATED fixed config: 3 combatant slots in 2 alliance teams -- slot 0
// (Allied, team 0, human) + slot 2 (Allied, team 0, AI Easy) allied vs slot 1
// (Soviet, team 1, AI Normal). NO neutral house (keeps clear of the
// capture/infiltrate <2/<3 writer-guard carry). shroud + short_game +
// superweapons on; crates off; starting_cash 10000, unit_count 10, tech_level
// 10. It launches the world, folds at two fixed checkpoints, steps N=300 fixed
// dt=1/15 frames, records the wedge bools, and does the rendered/headless self-
// compare via run_demo_skirmish. PURELY ADDITIVE: no gate calls it; world_hash
// untouched; NO pinned hash literal (Task 7 pins the observed value in README).
DemoSkirmishResult simulate_skirmish(Assets& a, UnitAssets& ua, BuildingAssets& ba,
                                     const Rules& rules,
                                     const std::vector<const VehicleAssets*>& vlist,
                                     bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                     const ra2engine::Camera* cam, int W, int H,
                                     const std::string& start_png, const std::string& end_png) {
    DemoSkirmishResult r;
    // FIXED adjudicated config [RE-GATE: skirmish.md §Adjudication item 4].
    ra2engine::SkirmishConfig cfg;
    cfg.starting_cash = 10000.0;
    cfg.unit_count    = 10;
    cfg.tech_level    = 10;
    cfg.shroud = true; cfg.short_game = true; cfg.superweapons = true; cfg.crates = false;
    cfg.slots = {
        {ra2engine::World::Side::Allied, 5, "Americans", 0, true, false, ra2engine::AiDifficulty::Normal},
        {ra2engine::World::Side::Soviet, 1, "Russians",  1, true, true,  ra2engine::AiDifficulty::Normal},
        {ra2engine::World::Side::Allied, 6, "British",   0, true, true,  ra2engine::AiDifficulty::Easy},
    };
    ra2engine::World world = launch_skirmish(cfg, a, ua, ba, rules);

    uint64_t h = 0xcbf29ce484222325ull;    // FNV-1a offset basis (world_hash's)
    auto render_frame = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
    };

    // A deterministic per-house (alive building count, alive unit count,
    // credits) signature -> the liveness/determinism witness. Order-stable
    // scalars only, so it is identical in the rendered and headless runs.
    auto state_sig = [&]() -> uint64_t {
        uint64_t s = 0xcbf29ce484222325ull;
        const int n = world.house_count();
        for (int hh = 0; hh < n; ++hh) {
            uint64_t nb = 0, nu = 0;
            for (const ra2engine::Building& b : world.buildings())
                if (b.alive && b.state != ra2engine::Building::State::Dead && b.house == hh) ++nb;
            for (const ra2engine::Unit& u : world.units())
                if (u.alive && u.die_time < 0.0 && u.house == hh) ++nu;
            s = fnv_mix(s, nb);
            s = fnv_mix(s, nu);
            s = fnv_mix(s, (uint64_t)(int64_t)std::llround(world.house_credits(hh)));
        }
        return s;
    };

    // Checkpoint A: launched world shape.
    world.step_visibility();
    h = skirmish_fold(h, world);
    render_frame();
    if (render) capture_png(ren, start_png, W, H);

    const bool house_count_ok    = (world.house_count() >= 3);
    // slot 0 human, slots 1 + 2 AI.
    const bool ai_ok             = world.ai_enabled(1) && world.ai_enabled(2) && !world.ai_enabled(0);
    // 0 + 2 allied on team 0; 0 vs 1 enemies across teams.
    const bool relationship_ok   = world.are_allied(0, 2) && world.is_enemy(0, 1);
    // Both firewalls opened by the config launch (configure_skirmish_houses).
    const bool victory_shroud_ok = world.victory_enabled() && world.shroud_enabled();
    const double cash_before     = world.house_credits(0);
    const uint64_t sig_before    = state_sig();
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      "[sk] stage1 launch: houses=%d ai(0)=%s ai(1)=%s ai(2)=%s team(0,2)=%s enemy(0,1)=%s victory=%s shroud=%s cash0=%.0f",
                      world.house_count(), world.ai_enabled(0) ? "yes" : "no",
                      world.ai_enabled(1) ? "yes" : "no", world.ai_enabled(2) ? "yes" : "no",
                      world.are_allied(0, 2) ? "yes" : "no", world.is_enemy(0, 1) ? "yes" : "no",
                      world.victory_enabled() ? "yes" : "no", world.shroud_enabled() ? "yes" : "no",
                      cash_before);
        r.witness.emplace_back(b);
    }

    // Checkpoint B: advance N fixed frames -> deterministic state change.
    const int N = 300;
    for (int k = 0; k < N; ++k) world.step(1.0 / 15.0);
    h = skirmish_fold(h, world);
    const double cash_after = world.house_credits(0);
    const uint64_t sig_after = state_sig();
    const bool advanced_ok  = (sig_after != sig_before);
    {
        char b[224];
        std::snprintf(b, sizeof b,
                      "[sk] stage2 stepped: N=%d cash0=%.0f->%.0f sig=%016llx->%016llx advanced=%s",
                      N, cash_before, cash_after, (unsigned long long)sig_before,
                      (unsigned long long)sig_after, advanced_ok ? "yes" : "no");
        r.witness.emplace_back(b);
    }

    r.hash = h;
    // Wedge guards [spec §Testing]: exit 1 unless each invariant holds.
    if (!house_count_ok)     { r.ok = false; r.witness.emplace_back("[sk] WEDGE: launch_skirmish did not produce the expected house_count"); }
    if (!ai_ok)              { r.ok = false; r.witness.emplace_back("[sk] WEDGE: AI slots (1,2) not enabled / human slot 0 wrongly AI"); }
    if (!relationship_ok)    { r.ok = false; r.witness.emplace_back("[sk] WEDGE: alliance(0,2) / enemy(0,1) relationships not configured"); }
    if (!victory_shroud_ok)  { r.ok = false; r.witness.emplace_back("[sk] WEDGE: victory/shroud not enabled by the config launch"); }
    if (!advanced_ok)        { r.ok = false; r.witness.emplace_back("[sk] WEDGE: the sim did not advance deterministically"); }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[sk] skirmish_hash=%016llx", (unsigned long long)r.hash);
    r.witness.emplace_back(hb);
    render_frame();
    if (render) capture_png(ren, end_png, W, H);
    return r;
}

// --demo-skirmish: the Skirmish-launch integration gate (17th permanent byte-
// identity gate). Runs simulate_skirmish rendered (hidden window + start/end
// screenshots) then headless from a FRESH World, and byte-compares the two
// witness logs + skirmish hashes. NO hardcoded hash literal -- compute-and-
// self-compare; the controller pins the observed value in the README gate table.
int run_demo_skirmish() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_skirmish_start.png";
    const std::string end_png = scratch + "demo_skirmish_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok || !ba.sidebar_ok) {
        std::cerr << "[demo-skirmish] FAILED: building assets unavailable\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist;            // no voxel units in this gate

    DemoSkirmishResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-skirmish)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_skirmish(a, ua, ba, rules, vlist, /*render=*/true, ren, &tc, &cam,
                                   W, H, start_png, end_png);
        }
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    }

    DemoSkirmishResult r2 = simulate_skirmish(a, ua, ba, rules, vlist, /*render=*/false, nullptr,
                                              nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    const bool bad = !identical || !r1.ok || !r2.ok;
    std::cerr << "[demo-skirmish] run1 lines=" << r1.witness.size() << " run2 lines="
              << r2.witness.size() << " identical=" << (identical ? "yes" : "NO")
              << " ok(r1/r2)=" << (r1.ok ? "Y" : "n") << "/" << (r2.ok ? "Y" : "n") << "\n";
    for (const std::string& line : r1.witness) std::cout << line << "\n";
    std::cout << "start_png=" << start_png << "\nend_png=" << end_png << "\n";
    char hb[96];
    std::snprintf(hb, sizeof hb, "deterministic=%s lines=%d skirmish_hash=%016llx",
                  identical ? "yes" : "no", (int)r1.witness.size(),
                  static_cast<unsigned long long>(r1.hash));
    std::cout << hb << "\n";
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
        return run_menu(/*no_audio=*/false);
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
        bool demo_base = false;
        bool demo_econ = false;
        bool demo_prod = false;
        bool demo_ai_base = false;
        bool demo_ai_team = false;
        bool demo_ai_trigger = false;
        bool demo_transport = false;
        bool demo_aircraft = false;
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
            } else if (arg == "--demo-base") {
                demo_base = true;
            } else if (arg == "--demo-econ") {
                demo_econ = true;
            } else if (arg == "--demo-prod") {
                demo_prod = true;
            } else if (arg == "--demo-ai-base") {
                demo_ai_base = true;
            } else if (arg == "--demo-ai-team") {
                demo_ai_team = true;
            } else if (arg == "--demo-ai-trigger") {
                demo_ai_trigger = true;
            } else if (arg == "--demo-transport") {
                demo_transport = true;
            } else if (arg == "--demo-aircraft") {
                demo_aircraft = true;
            } else if (arg == "--demo-naval") {
                return run_demo_naval();
            } else if (arg == "--demo-superweapon") {
                return run_demo_superweapon();
            } else if (arg == "--demo-techtree") {
                return run_demo_techtree();
            } else if (arg == "--demo-capture") {
                return run_demo_capture();
            } else if (arg == "--demo-shroud") {
                return run_demo_shroud();
            } else if (arg == "--demo-multihouse") {
                return run_demo_multihouse();
            } else if (arg == "--demo-skirmish") {
                return run_demo_skirmish();
            } else if (arg == "--demo-audio") {
                demo_audio = true;
            } else if (arg == "--no-audio") {
                no_audio = true;
            }
        }
        // --demo-tank dispatched BEFORE --demo-combat (brief Step 6 ordering).
        if (demo_tank) return run_demo_tank();
        if (demo_base) return run_demo_base();
        if (demo_econ) return run_demo_econ();
        if (demo_prod) return run_demo_prod();
        if (demo_ai_base) return run_demo_ai_base();
        if (demo_ai_team) return run_demo_ai_team();
        if (demo_ai_trigger) return run_demo_ai_trigger();
        if (demo_transport) return run_demo_transport();
        if (demo_aircraft) return run_demo_aircraft();
        if (demo_combat) return run_demo_combat(combat_seed);
        if (demo_audio) return mode_demo_audio();
        if (demo_group) return run_demo_group(group_n, group_x, group_y);
        if (demo_retarget)
            return run_demo_retarget(retarget_x1, retarget_y1, retarget_x2, retarget_y2);
        if (demo_move) return run_demo_move(demo_x, demo_y);
        if (!screenshot.empty()) return run_screenshot(screenshot);
        return run_menu(no_audio);
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
