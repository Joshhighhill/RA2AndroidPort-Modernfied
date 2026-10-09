#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ai_base.hpp"  // AI-1 base-planning: AiBaseState/BaseView/next_build/...
#include "ai_team.hpp"  // AI-2 offensive-teams: AiTeamState/TeamInstance/TeamView/...
#include "ai_trigger.hpp"  // AI-3 trigger engine: AiTriggerState/TriggerView/TriggerDecision
#include "building.hpp"
#include "occupancy.hpp"
#include "ore.hpp"  // OreField/OreKind/OreRng (Economy phase)
#include "pathfind.hpp"  // Cellxy (path storage / find_path)
#include "production.hpp"  // AI-1 base-planning: World owns per-house ProductionQueues
#include "inf_anim.hpp"               // W3-B infantry sequence machine (Unit::ia)
#include "ra2assets/land_type.hpp"  // CB-3 Naval: LandType (build_water_passability)
#include "repair_sell.hpp"  // Repair & Sell: RepairSellRules + the pure tick arithmetic
#include "crate.hpp"  // P5 Crates: GameRandom/CrateRules/Powerup (pure)
#include "abilities.hpp"    // P4 abilities: AbilityProfile/AbilityRules/RadField
#include "superweapon.hpp"  // CB-4 Superweapons: Superweapon/SuperweaponType lifecycle
#include "visibility.hpp"  // Shroud/Fog: cell_in_sight/reveal_disc (pure reveal math)
#include "ra2assets/map.hpp"
#include "ra2assets/pal.hpp"  // ra2assets::Palette (house_palette render entry)
#include "unit_combat.hpp"  // UnitCombat (moved here to break the include cycle
                            // production.hpp -> unit_type.hpp -> world.hpp)

namespace ra2engine {

// Forward declarations only: OreValues and HarvestEvent are DEFINED in
// harvest.hpp, which itself includes world.hpp (harvest.hpp needs Unit,
// defined below) -- a real #include cycle, so world.hpp cannot include
// harvest.hpp back. These two structs appear here ONLY in function
// declaration signatures (set_economy's by-value parameter, take_harvest_
// events' by-value return), which is legal with just a forward declaration
// (no instantiation is forced by a mere declaration); World stores NO member
// of either type directly -- set_economy copies OreValues's two ints into
// private int fields, and take_harvest_events converts a private POD event
// buffer (World::PendingHarvestEvent, fully defined inline, see below) to/
// from real HarvestEvent objects, entirely inside world.cpp where
// harvest.hpp IS included. Any caller of these methods must include
// harvest.hpp itself to get the complete types (main.cpp / tests already do,
// or will).
struct OreValues;
struct HarvestEvent;

// Harvester mission states [RE: economy.md §1]. Lives here (not harvest.hpp)
// because Unit carries one; harvest.hpp includes world.hpp.
enum class HarvState : std::uint8_t {
    None,          // not a harvester / no mission
    SeekOre,       // choosing an ore cell (rescan)
    MoveToOre,     // pathing to harv_cell
    Harvesting,    // parked on ore, gulping
    ReturnToRef,   // driving (HARV) or warping (CMIN) home
    WaitDock,      // at the refinery, dock busy
    Unloading,     // draining bales into credits
};

// CB-2 Aircraft [RE: aircraft.md]: the flight-locomotor state machine. A
// Grounded unit is on the map (holds its cell); TakingOff ascends to cruise
// (ground cell released); Cruising translates straight-line at altitude
// holding no cell; Descending lowers toward the ground. Order-driven
// transitions (Grounded->TakingOff, Cruising->Descending) are set by World;
// altitude-driven ones (TakingOff->Cruising, Descending->Grounded) by
// flight::advance_flight_state. Default Grounded is INERT (spec §8).
enum class FlightState { Grounded, TakingOff, Cruising, Descending };

// CB-3 Naval [RE: naval.md §1]: MovementDomain { Land=0, Water=1 } is defined
// in unit_combat.hpp (UnitCombat carries a field of it, and that header is
// included above at line 17). world.hpp sees it transitively -- Land (the
// DEFAULT, every existing unit) resolves to the UNCHANGED passable_ grid via
// grid_for; Water resolves to water_passable_.

// A single visible/controllable actor in the world: its map cell + height
// level (raw IsoMapPack5 coordinate space -- see ra2assets::MapCell), the
// SHP base image name (a `Rules::image()` result, e.g. unit id "E1" ->
// "GI" -> GI.SHP), and the current animation frame index into that SHP.
//
// Task 12 adds selection + movement: `path` is the A* result (from the
// unit's cell to the ordered goal, inclusive of both endpoints), `path_idx`
// is the index of the NEXT cell being advanced toward (path[0] is always the
// cell the unit started the order on, so movement begins at index 1), and
// `fx`/`fy` are the CONTINUOUS iso-cell position used for smooth
// interpolated rendering between cells (integer `cell_x`/`cell_y` only
// update as each cell is reached, so game logic/hit-testing stays on a
// clean grid). `anim_time` drives the walk-frame cycle. `selected` is the
// UI selection flag.
struct Unit {
    int cell_x = 0;
    int cell_y = 0;
    int level = 0;
    int frame = 0;
    std::string image;

    // --- Task 12: selection + movement/animation state ---
    bool selected = false;
    std::vector<Cellxy> path;  // full A* path (start..goal); empty when idle
    std::size_t path_idx = 0;  // index of the next path cell to advance toward
    double fx = 0.0, fy = 0.0;  // continuous iso-cell position (interp render)
    double anim_time = 0.0;     // accumulated time for walk-frame cycling
    double wait_time = 0.0;     // seconds blocked at current cell (collision re-path timer)

    // 8-way facing block index, 0=N going COUNTERCLOCKWISE on screen to 7=NE
    // (docs/re/infantry-anim.md §4-6); spawns facing S (4).
    int facing = 4;

    bool moving() const { return !path.empty() && path_idx < path.size(); }

    // --- Combat (this phase). Stats resolved once at spawn from Rules. ---
    int house = 0;                 // 0 = player/Allied, 1 = hostile/Soviet
    int hp = 1, max_hp = 1;
    int armor_ix = 0;              // armor_index(Armor=)
    int w_damage = 0;              // weapon Damage=
    int w_rof = 0;                 // weapon ROF= (15Hz logic frames)
    double w_range = 0.0;          // weapon Range= (cells)
    std::vector<int> w_verses;     // warhead Verses percents (11 entries)
    double w_cell_spread = 0.0;    // warhead CellSpread (cells) [aircraft.md §4]
    double w_percent_at_max = 1.0; // warhead PercentAtMax (edge damage scale)
    bool w_affects_allies = true;  // W5-B R-060: warhead AffectsAllies= (default yes)
    // Campaign mind control [RE: BulletClass::Detonate FUN_004690A0 -- a
    // warhead with MindControl (+0x155) skips the whole DamageArea
    // FUN_00489190 branch and only calls the owner's CaptureManager
    // (+0x2BC) Capture FUN_00471C80 @0x4692C0: the victim is taken, never
    // damaged]: set every frame by CampaignRuntime
    // (poll_mind_control, BEFORE World::step) on a live MindControl=yes
    // controller; try_fire then deals no damage and the runtime captures the
    // victim (same 3-D range rule). Re-armed each frame; saved (v24) only
    // because every Unit member is (the save tripwire).
    bool w_mind_control = false;
    // W5-B R-061 infantry sub-cells [RE: FindInfantrySubposition FUN_00481090,
    // spot coords DAT_0089D960]: the spot (2 right, 3 left, 4 down; 0 centre =
    // vehicles / sub-cells off) the unit holds in its cell, and its lepton
    // offset from the cell centre -- interpolated between the old and the
    // reserved spot while walking. Derived from the Occupancy every step
    // (World::update_sub_offsets); render / damage use fx*256 + sub_x.
    int spot = 0;
    double sub_x = 0.0, sub_y = 0.0;
    // W5-B R-062 [RE: DriveLocomotionClass FUN_004B2550 @0x4B3AE0..]: the
    // GroundType Cost column (SpeedType) and the speed multiplier the
    // locomotor set when it committed to the current/next cell (land cost of
    // that cell capped at 1, 0 -> .5, x uphill/downhill, x .75 at ConditionRed).
    int speed_type = -1;
    double speed_mult = 1.0;
    // Projectile phase (P2) [RE: projectile.md]: the primary weapon's projectile
    // (default kind=Instant = the pre-P2 instant-hit path). Read ONLY when the
    // World's projectiles_enabled_ firewall is open; NOT read by world_hash.
    ProjectileSpec w_proj;
    int target = -1;               // unit index being attacked, -1 none
    int target_building = -1;      // building index being attacked, -1 none;
                                    // mutually exclusive with `target` --
                                    // setting one clears the other (Buildings
                                    // & Construction Task 4).
    bool attack_order = false;     // explicit order (pursues) vs auto-acquire
    double rof_cool = 0.0;         // seconds until the weapon may fire again
    double fire_flash = 0.0;       // seconds of fire-anim left (renderer)
    // W3-B: the active weapon's warhead InfDeath= / ProneDamage= (set with the
    // other w_* fields) and the infantry sequence state. Inert unless
    // World::enable_infantry_anims armed the unit; never hashed.
    int w_inf_death = 0;
    double w_prone_damage = 1.0;
    InfAnimState ia;
    int guard_x = 0, guard_y = 0;  // guard anchor (cell when target auto-acquired)
    bool alive = true;             // TOMBSTONE: false after die anim; never erased
    double die_time = -1.0;        // >=0: dying (die anim playing); alive stays true until it ends

    // --- Voxel Vehicles phase: continuous-facing vehicle state ---
    // Facings live in the original's 16-bit space [0, 65536); 32768 faces
    // screen-South like the infantry spawn (docs/re/voxel-render.md §2/§5).
    bool is_vehicle = false;
    double body_facing = 32768.0;
    double turret_facing = 32768.0;
    double rot_u16_per_sec = 0.0;  // ROT * kRotU16PerRotPerSec
    double speed_cps = 0.0;        // per-unit speed; 0 = use World move_cps

    // --- Economy phase: harvester state [RE: economy.md §1-§3] ---
    bool is_harvester = false;
    bool chrono_miner = false;       // CMIN: warps home instead of driving
    int storage_bales = 0;           // Rules Storage= (HARV 40, CMIN 20)
    double bales_ore = 0.0;          // current load, ore bales
    double bales_gems = 0.0;         // current load, gem bales
    HarvState harv = HarvState::None;
    int harv_cell_x = 0, harv_cell_y = 0;   // target/last ore cell
    // True once harv_cell_x/y have been stamped with a REAL cell (SeekOre
    // finding ore, or a player's order_harvest click) -- Review Important-3:
    // a freshly spawned CMIN manually returned before ever finding ore has
    // harv_cell_x/y at their (0,0) default, which is not a place worth
    // warping back to. Deliberately never cleared back to false once set
    // (a miner that has found ore once always has a legitimate fallback
    // cell to return to).
    bool has_harv_cell = false;
    int dock_refinery = -1;          // target/docked refinery Building index:
                                     // stamped when ReturnToRef picks it, held
                                     // through WaitDock/Unloading (-1 = none,
                                     // or CMIN post-unload warp in transit)
    double harv_timer = 0.0;         // gulp/unload/rescan/warp accumulator
    bool manual_hold = false;        // player move order: pause auto-resume until arrival
    int harv_skip_x = -1, harv_skip_y = -1;   // W2-SK-04: ore cell the last move failed to reach

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---------------------
    // ALL defaults INERT (spec §8): no existing demo issues a load/unload
    // order and no existing spawn path sets passengers_max > 0, so every
    // field below holds its default in every existing demo; world_hash reads
    // only cell/hp/alive and is untouched. cargo/cargo_of/enter_target store
    // unit INDICES -- stable for a World's lifetime (units are tombstoned,
    // never erased; the same invariant `target` relies on).
    int passengers_max = 0;      // capacity COUNT (Rules Passengers= bodies);
                                 // 0 = not a transport [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- NumPassengers <
                                 // Passengers, a body count, NOT a Size pool]
    int size_limit = 0;          // largest admissible passenger Size
                                 // (SizeLimit=) -- a PER-PASSENGER cap [§1]
    int size = 1;                // own bulk (Size=); feeds ONLY the pip display
                                 // (cargo_total_size) [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- absent-key defaults
                                 // CONFIRMED: Passengers/SizeLimit -> 0,
                                 // Size -> 1; Task 4 resolves real values]
    std::vector<int> cargo;      // carried unit indices in boarding order;
                                 // ejects pop the BACK (LIFO [RE-GATE APPLIED:
                                 // transport-cargo.md §6])
    int cargo_of = -1;           // >=0: index of the transport carrying this
                                 // unit -- the unit is OFF-MAP (spec §4)
    int enter_target = -1;       // pending enter order -> transport index
    bool unload_pending = false; // pending unload order on this transport
    int eject_counter = 0;       // eject_cell scatter-side alternator (+/-90 deg
                                 // by low bit), bumped once per eject
                                 // [transport-cargo.md §4]

    // --- CB-2 Aircraft/Flight [RE: aircraft.md] -----------------------------
    // ALL defaults INERT (spec §8): no existing demo spawns an aircraft and no
    // existing spawn path sets is_aircraft=true, so every field holds its
    // default in every existing demo; world_hash reads only cell/hp/alive and
    // is untouched (strategy (a)). altitude is DERIVED into a screen offset at
    // draw time -- no stored screen offset.
    bool is_aircraft = false;                          // flight-locomotor unit
    double altitude = 0.0;                             // leptons; 0 = on the ground
    FlightState flight_state = FlightState::Grounded;  // Grounded/TakingOff/Cruising/Descending
    double cruise_altitude = 0.0;                      // JumpjetHeight (leptons)
    double climb_rate = 0.0;                           // JumpjetClimb (leptons/sec)
    double descend_rate = 0.0;                         // JumpjetCrash (leptons/sec)
    double flight_speed = 0.0;                         // cruise speed (cells/sec)
    double flight_turn = 0.0;                          // heading turn (u16/sec)
    bool balloon_hover = false;                        // never lands
    bool omni_fire = false;                            // fire without turning
    bool has_flight_goal = false;                      // a cruise goal is set
    int flight_goal_x = 0, flight_goal_y = 0;          // cruise goal cell (NOT A*)

    // --- CB-3 Naval/Water [RE: naval.md §1] ---------------------------------
    // ALL defaults INERT (spec §8): no existing demo spawns a naval unit and no
    // existing spawn path sets is_naval=true, so every field holds its default
    // in every existing demo; world_hash reads only cell/hp/alive and is
    // untouched (strategy (a)). A naval unit reuses path/path_idx/fx/fy/
    // cell_x/cell_y/occ_ exactly like a ground vehicle; only the GRID its A*
    // reads differs (movement_domain -> grid_for).
    bool is_naval = false;                                // naval-locomotor unit
    MovementDomain movement_domain = MovementDomain::Land;  // Water for a naval unit

    // --- Engineer/Spy Capture phase [RE: capture.md] ------------------------
    // Default INERT (spec §8): no existing demo issues an engineer capture order
    // and no spawn path sets it, so this field holds -1 in every existing demo;
    // world_hash reads only cell/hp/alive and is untouched. Mirrors enter_target
    // (a pending-order building INDEX, -1 = none). The step_capture arrival pass
    // is a strict no-op for every unit with capture_target < 0.
    int capture_target = -1;     // pending engineer capture/repair order ->
                                 // building index (-1 = none)
    // Spy infiltration order [RE: capture.md §B]: same deferred move-adjacent-
    // then-act model as capture_target, but the effect (infiltrate_building) is
    // resolved-purpose-driven -- the InfiltratePurpose is decided at the gesture
    // layer (World is asset-free) and stored here PRE-RESOLVED, so step_capture
    // acts on arrival without the roster. Default INERT: -1 target + None(0)
    // purpose -> the step_capture spy pass is a strict no-op in every existing
    // demo; world_hash reads none of these. infiltrate_purpose holds the int
    // value of World::InfiltratePurpose (defined below Unit; 0 = None).
    int infiltrate_target = -1;         // pending spy infiltrate -> building idx
    int infiltrate_purpose = 0;         // resolved InfiltratePurpose as int (0=None)
    double infiltrate_steal_pct = 0.5;  // SpyMoneyStealPercent (arrives pre-resolved)
    int infiltrate_blackout = 1000;     // SpyPowerBlackout (arrives pre-resolved)

    // --- Engineer/Spy Capture phase [RE: capture.md §B5] --------------------
    // Instance veteran rank (copied from UnitCombat at add_combat_unit). Inert
    // default (0) -> NOT read by world_hash (cell/hp/alive only); hashed only by
    // capture_hash. Recorded so the gate can fold a produced unit's rank.
    int veteran_rank = 0;

    // --- Shroud/Fog phase [RE: shroud.md §2/§3] ---
    int sight = 0;                 // reveal radius (cells); copied from UnitCombat
    std::uint8_t seen_by_mask = 0xFF;  // bit h set iff this unit's cell is visible
                                       // to house h. DEFAULT 0xFF = seen by all =
                                       // INERT (the combat.cpp:362 clause never
                                       // fires). Overwritten only by step_visibility
                                       // (behind shroud_any_). NOT read by world_hash.

    // --- Engineer/Spy Capture phase: disguise [RE: capture.md §C] ---
    // A disguised unit RENDERS as `disguised_as_type` (a SHP image id) in
    // `disguised_as_house`'s palette; a still-disguised enemy spy is treated as
    // FRIENDLY -- the step_combat idle auto-acquire scan (combat.cpp) skips any
    // candidate with disguised==true, so no enemy unit auto-fires on it (an
    // explicit order_attack is unaffected -- the skip lives only in the auto-
    // acquire scan, not valid_target). detect_disguise units (dogs) clear the
    // flag (step_tech_structures), restoring targetability. ALL inert defaults
    // -> NOT read by world_hash; hashed only by capture_hash.
    bool disguised = false;
    std::string disguised_as_type;   // SHP image id shown while disguised (e.g. "E1")
    int disguised_as_house = 0;      // palette house shown while disguised
    bool detect_disguise = false;    // DetectDisguise=yes (dogs) -> reveals spies

    // --- P5 Veterancy + Crates [RE: veterancy.md] ---------------------------
    // Experience, rank memory, crate multipliers, the combat receive-hook flag
    // and the type data copied from UnitCombat. ALL defaults inert (see
    // veterancy.hpp UnitVet): read only past World's vet_any_ firewall, except
    // vet.killed_by, a stamp combat.cpp writes at death and nothing else reads.
    // NOT read by world_hash or any existing *_hash.
    UnitVet vet;
    // --- P4 Unit special abilities [RE: abilities.md] -----------------------
    // ALL defaults INERT (spec §Global constraints): a profile reaches a unit
    // ONLY through World::register_ability_profile (bound by image in
    // step_abilities), which no pre-P4 gate calls -- so every field below
    // holds its default in every existing demo and world_hash (cell/hp/alive)
    // is untouched. Indices are unit/building indices (stable: tombstoned,
    // never erased).
    AbilityProfile abil;             // the bound per-type profile (inert = none)
    bool abil_bound = false;         // step_abilities looked this unit's image up
    // Frozen = cannot move, fire, acquire or be pathed: teleport warp-in
    // (§C1 +0x271), being erased (§C2 +0x270), deployed (§A). Read as a guard
    // by step_units / step_combat / the MoveRequest dispatch.
    bool frozen = false;
    std::int64_t warp_until = -1;    // teleport: frozen until this frame (§C1)
    int ability_fired_unit = -1;     // combat fire intercepted this frame (victim)
    int ability_fired_building = -1; // ... or building victim
    int parasite_on_me = -1;         // drone riding this unit (§B1 +0x694)
    std::int64_t parasite_next_tick = 0;  // drone: DamageDeliveryTimer expiry (§B2)
    std::int64_t suppress_until = -1;     // drone: SuppressionTimer expiry (§B3)
    int last_hp = -1;                // host: HP at the end of the previous pass (§B3)
    int temporal_target = -1;        // CLEG: unit being erased (§C2)
    int temporal_target_building = -1;  // CLEG: building being erased
    int warp_remaining = 0;          // victim: WarpRemaining (§C2); 0 = not warped
    std::int64_t bomb_frame = -1;    // Ivan bomb DetonationFrame on this unit (§E)
    int bomb_owner = -1;             // the planting unit
    int bomb_house = -1;             // its house
    bool deployed = false;           // desolator deployed (§A)
    std::int64_t deploy_next_fire = 0;  // next eruption frame
    int ifv_current = 0;             // gunner: current weapon index (§G)
    bool death_done = false;         // death weapon / bomb / parasite handled (§F)

    // --- Gap fix C: weapon slots [RE: docs/re/weapons.md] -------------------
    // w_* above always hold the ACTIVE weapon. Slot 0 (primary) is the legacy
    // w_*/vet.base data; slot 1 (secondary) lives here and is swapped in by
    // activate_weapon_slot when choose_weapon picks it for a target. Inert for
    // every unit without a secondary (w2.valid false -> slot 0 forever).
    WeaponSlot w2;                   // Secondary= (slot 1)
    WeaponSlot w2_elite;             // EliteSecondary= (valid only if defined)
    bool p_aa = false, p_ag = true;  // primary projectile AA= / AG=
    ProjectileSpec p_proj;           // primary projectile (base)
    ProjectileSpec p_proj_elite;     // ElitePrimary/EliteWeapon1 projectile
    bool p_proj_elite_set = false;
    int w_active = 0;                // 0 = primary in w_*, 1 = secondary
    bool w_aa = false, w_ag = true;  // ACTIVE weapon's AA/AG
    // Deployed infantry fire weapon (GI Para, GGI MissileLauncher):
    // DeployFireWeapon slot resolved; deploy_fire=true makes the deployed unit
    // immobile and fire this slot through the normal step_combat path (G-45).
    bool deploy_fire = false;        // Deployer=yes DeployFire=yes, no RadLevel
    int deploy_fire_slot = 1;        // DeployFireWeapon= (def 1)
    // Original behaviour switches (G-50 / G-42 / G-47): set per World by
    // World::enable_original_combat; see combat.cpp.
    bool crusher = false;            // Crusher=yes (vehicles)
    bool auto_crush = false;         // W5-B R-050: AutoCrush=yes (the player's harvesters)
    bool is_infantry = false;        // W5-B R-061: InfantryTypes type -> sub-cell spots
    bool land_speed_loco = false;    // W5-B R-062: Drive/Ship locomotor
    bool crushing = false;           // W5-B R-050: driving onto its crushable target
    bool crushable = false;          // Crushable=yes (infantry)
    // --- gF MCV deploy [RE: docs/re/mcv-deploy.md] --- inert defaults.
    int deploys_into = -1;           // DeploysInto= building roster index (-1 none)
    int deploy_mission = 0;          // 0 none, 1 turning to DeployFacing, 2 driving to a spot
    int deploy_goal_x = 0, deploy_goal_y = 0;   // mission 2 destination
    bool deployed_away = false;      // consumed by a deploy (not a death)
    // --- G16 world depth (agent G, world_depth.cpp) [RE: docs/re/world-depth.md] ---
    // A garrisoned infantry is off the map like a passenger: cargo_of holds
    // kGarrisonCargo (so every "cargo_of >= 0 -> off-map" test skips it) and
    // garrison_of the building index. Inert defaults; NOT read by world_hash.
    bool p_wall = false;             // primary warhead Wall= (bridge damage)
    bool occupier = false;           // Occupier=
    WeaponSlot occupy_w;             // OccupyWeapon=
    WeaponSlot occupy_w_elite;       // EliteOccupyWeapon=
    int garrison_of = -1;            // building index while garrisoned
    int occupy_target = -1;          // pending "enter garrison" order -> building
    int attack_cell_x = -1, attack_cell_y = -1;   // force-fire ground target (-1 none)
    bool cell_shot = false;          // fired at attack_cell this step (World resolves it)
    // --- W2-SK-22 (save v13) ---
    int shrapnel_count = 0;
    int shrapnel_damage = 0;
    double shrapnel_range = 0.0;
    std::vector<int> shrapnel_verses;
    bool disguise_when_still = false;
    int still_frames = 0;            // Mirage: frames stopped and not firing
    AnUnit an;                       // W2-AN: naval/air/SW runtime state (air_naval.hpp)
    UnitW5 w5;                       // wave-5 abilities (abil_w5.hpp)
};
// G16: the cargo_of sentinel of a garrisoned infantry (never a unit index).
constexpr int kGarrisonCargo = 0x3FFFFFFF;
// W4-SQ-06: the cargo_of sentinel of a paratrooper still under its parachute
// (off the map for every system; AnUnit::para_alt holds its height).
constexpr int kParachuteCargo = 0x3FFFFFFE;

// One cargo event (board / capacity-refusal / eject), drained by callers per
// frame via World::take_cargo_events -- the take_harvest_events pattern:
// main.cpp turns these into demo witness lines and the guarded Enter/
// LeaveTransportSound hook [RE: transport-cargo.md; spec §6/§7]. Defined
// HERE (not cargo.hpp) so World can store a vector without an include cycle.
struct CargoEvent {
    enum class Kind { Board, Refuse, Eject };
    Kind kind = Kind::Board;
    int unit = -1;       // the passenger
    int transport = -1;  // the transport
    // Board/Refuse: x = the passenger COUNT after the event resolved
    // (capacity is a COUNT [RE-GATE APPLIED: transport-cargo.md §1]), y =
    // the transport's passengers_max (the "K/5" witness pair). Eject:
    // (x, y) = the eject cell.
    int x = 0, y = 0;
};

// Per-image (per-unit-type), facing-aware animation data -- REPLACES the
// Task 7 empirically-eyeballed table, which had two confirmed frame bugs
// (CONS fire played half of Die2's frames; die played Idle2/Prone frames).
// Calibrated against the ORIGINAL game's artmd.ini GISequence/ConSequence
// data; docs/re/infantry-anim.md is the calibration authority -- see it for
// the extracted values, the frame-selection formula's derivation, and the
// facing-block-order evidence (§1-5).
//
// A SeqEntry mirrors one artmd.ini `Name=StartFrame,FrameCount,
// FacingMultiplier` sequence entry (§1); frame selection is
// `first + facing*fmult + phase` (see seq_frame below). `fmult == 0` means
// facing-independent (deaths use one shared animation, no per-facing block).
struct SeqEntry {
    int first = 0;
    int count = 1;
    int fmult = 0;
};

// One unit-type's stand/walk/fire/die sequences.
struct UnitAnimSeq {
    SeqEntry stand, walk, fire, die;
};

// Per-image animation table: Unit::image ("GI", "CONS", ...) -> its
// UnitAnimSeq. Populated from artmd.ini by main.cpp (Task 4 wires
// ra2assets::Art in); declared here (not file-static) so both World and
// tests/test_world.cpp's pure anim-selection tests can build/pass one.
using AnimTable = std::map<std::string, UnitAnimSeq>;

// The real GI/CONS artmd.ini values (docs/re/infantry-anim.md §2). Serves as
// the fallback for any image not registered in an AnimTable -- including
// every test that builds a World without wiring artmd.ini at all -- so
// unregistered images still animate with the REAL values instead of a
// crash/UB or stale placeholder.
const UnitAnimSeq& default_anim();

// Looks up `image` in `t`; falls back to default_anim() on a miss (documented
// -- see default_anim() above). Defined in world.cpp; declared here so both
// step_units/World::step and tests/test_world.cpp can call it.
const UnitAnimSeq& anim_for_image(const AnimTable& t, const std::string& image);

// frame = e.first + facing*e.fmult + phase, with phase clamped to
// [0, e.count-1] (docs/re/infantry-anim.md §1's
// `StartFrame + facing*FacingMultiplier + phase` formula; `phase` here is
// the doc's "frame offset within the sequence", renamed to avoid confusion
// with the FrameCount field).
int seq_frame(const SeqEntry& e, int facing, int phase);

// Cell-space direction (dx, dy) -> facing block index (0=N, counterclockwise
// on screen to 7=NE; docs/re/infantry-anim.md §4/§5). Returns -1 for a zero
// vector (no direction -- caller should keep the unit's current facing).
// Takes continuous (double) input so a move segment's ray direction resolves
// the same way regardless of its magnitude.
int facing_from_delta(double dx, double dy);

// --- Voxel Vehicles phase: continuous 16-bit-space facing helpers ---
// (docs/re/voxel-render.md §5). Facings are in [0, 65536); the 8 infantry
// blocks sit at k*8192.

// ROT rate scale: Set_ROT applies ROT*256 u16 per 15 Hz logic frame, i.e.
// ROT*256*15 u16/sec (voxel-render.md §5). ROT=5 -> 19,200 u16/sec ->
// ~3.4 s per full circle.
constexpr double kRotU16PerRotPerSec = 256.0 * 15.0;

// Aim tolerance for rotation-complete fire gating (voxel-render.md §5):
// vanilla holds fire until the timed turret rotation COMPLETES (exact facing,
// via Is_Rotating). turn_toward clamp-lands exactly on its target and
// step_turrets/step_combat compute `want` from the same positions in the same
// frame, so exact double comparison (`<= 0.0` on the shortest-arc magnitude)
// is well-defined. Same rule gates the hull turn-then-move.
constexpr double kAimToleranceU16 = 0.0;

// Continuous facing (u16) from a cell-space direction, consistent with
// facing_from_delta's 8-block convention (block k spans k*8192 +/- 4096).
double facing_u16_from_delta(double dx, double dy);

// Shortest-arc step in u16 space with wraparound, clamped to max_step;
// returns the new facing (== target when the arc magnitude <= max_step),
// always normalized to [0, 65536).
double turn_toward(double facing, double target, double max_step);

// True when the shortest arc from u.turret_facing to target_facing is within
// kAimToleranceU16 (rotation-complete gate).
bool vehicle_aimed(const Unit& u, double target_facing);

// Advance every moving unit toward its `path` by `dt` seconds at `move_cps`
// cells/sec, using `occ` to enforce one-unit-per-cell: a unit reserves the next
// path cell before entering it. While that cell is held by another unit the
// unit waits (resuming the instant the cell frees) and attempts a re-path
// around occupied cells on a seniority-staggered cadence -- a unit blocked by
// a LOWER-index unit tries at kBlockedWait, one blocked by a HIGHER-index
// unit one interval later (breaks the mirror-symmetric re-path livelock); it
// abandons the order only after kGiveUpAfter seconds of continuous block
// (boxed-in / sealed corridor).
// Idle units re-assert ownership of their current cell and stand. Units are
// processed in ascending index order (deterministic). `anims` resolves each
// unit's per-image stand/walk sequence (anim_for_image) and its facing
// (Unit::facing, updated here as each walk segment is selected --
// docs/re/infantry-anim.md §5) drives frame selection (seq_frame). Does NOT
// modify Unit::level -- rendering height is patched separately from the map
// by World::step. Pure: takes a passability grid + occupancy, no MapFile/SDL.
// CB-3 Naval [RE-GATE APPLIED: naval.md §1/§3]: `water_passable` is the SECOND
// grid (immediately after `passable`) -- the blocked re-path selects it per unit
// (u.is_naval ? water_passable : passable). Every existing caller passes the
// (empty in every existing demo) water grid and no existing (Land) unit reads
// it, so ground re-pathing is byte-identical.
// W5-B R-062 [RE: DriveLocomotionClass FUN_004B2550; GroundType table
// DAT_0089D9B0 read by FUN_00673A40]: the per-cell land speed inputs. Null /
// unarmed -> every unit keeps its flat speed (the pre-W5 behaviour).
struct MoveSpeedCtx {
    bool armed = false;
    const std::vector<std::uint8_t>* land = nullptr;   // LandType per cell (tile + overlay)
    const std::vector<std::int8_t>* level = nullptr;   // height level per cell
    float cost[12][8] = {};                            // [LandType][SpeedType], capped at 1
    double up[2] = {1.0, 1.0};                         // Tracked/Wheeled Uphill
    double down[2] = {1.0, 1.0};                       // Tracked/Wheeled Downhill
    double condition_red = 0.25;
    // Ore on a flat cell turns its land Tiberium (FUN_0047D1C0); overlay
    // lookup supplied by World (null = none).
    const OreField* ore = nullptr;
    const std::vector<std::uint8_t>* ramp = nullptr;   // TMP ramp type per cell
    // W5-B R-050 [RE-FALLBACK: CellClass::Incoming scatter not unwound]: idle
    // friendly infantry blocking a vehicle's next cell, asked to step aside.
    std::vector<int>* scatter_out = nullptr;
};
// The speed multiplier for unit u entering cell (nx, ny) from its cell.
double land_speed_mult(const MoveSpeedCtx& c, const Unit& u, int w, int h, int nx, int ny);

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable,
                const std::vector<std::uint8_t>& water_passable, int w, int h,
                Occupancy& occ, double dt, double move_cps, const AnimTable& anims,
                const std::vector<std::uint8_t>* amphib_passable = nullptr,
                const MoveSpeedCtx* speed = nullptr,
                const std::vector<std::uint8_t>* wall_leak = nullptr,
                const std::vector<std::uint8_t>* crush_passable = nullptr,
                const std::vector<std::uint8_t>* crush_leak = nullptr);

// Slew each alive, non-dying vehicle's turret_facing toward its target (via
// turn_toward at rot_u16_per_sec*dt) when it has a live target, else toward
// its body_facing (docs/re/voxel-render.md §5). Non-vehicles are skipped.
// Called from World::step between step_units and step_combat so the fire
// gate (vehicle_aimed) sees the turret's post-slew facing this frame. Pure.
//
// Buildings & Construction Task 4 fix: a unit with a live `target_building`
// (alive, non-Dead) also slews, toward the NEAREST foundation cell of that
// building -- same nearest-cell rule combat.cpp uses for range -- mirroring
// the unit-target branch above it. With `buildings` empty (every call site
// predating this fix, and any test that doesn't attack a building) this new
// branch is unreachable, so behavior is byte-identical to the old 2-arg
// signature.
void step_turrets(std::vector<Unit>& units, const std::vector<Building>& buildings, double dt);

// Picks a passable (Clear/grass, `tile == 0` -- confirmed in Task 9 /
// task-9-report.md: "tile index 0 is Clear") cell close to the centroid of
// every Clear cell on the map. That centroid sits in the map's open,
// populated interior (biased away from edges/water/cliffs), and -- because
// `cell_to_screen` is linear in (x, y) -- projects close to the same region
// `terrain_screen_bounds`'s bounding-box center does, so a unit placed here
// lands inside whatever viewport `main.cpp` centers on the map (Task 9's
// `centered_camera`). Full tile-type passability comes later; this is a
// deliberately simple "any Clear cell near the middle" heuristic.
// Returns nullptr if the map has no cells at all.
const ra2assets::MapCell* pick_start_cell(const ra2assets::MapFile& map);

// Builds the passability grid in RAW IsoMapPack5 (x, y) space -- the SAME
// coordinate space MapCell.x/y and iso_project's cell_to_screen /
// screen_to_cell live in (see map.hpp + iso_project.hpp). The grid is sized
// to cover every cell: grid_w = max(cell.x)+1, grid_h = max(cell.y)+1. A
// cell is passable (1) iff the map materialized a record at that (x, y);
// every un-materialized coordinate stays 0 (impassable / off-map). For this
// vertical slice ALL materialized ground cells are treated walkable (water /
// cliffs / land-type refinement comes later). Returns the flat
// grid_w*grid_h vector; sets grid_w/grid_h to 0 and returns empty if there
// are no cells. Free function (not a World member) so it is unit-testable
// with a hand-built cell vector without constructing a full MapFile.
std::vector<std::uint8_t> build_passability(const std::vector<ra2assets::MapCell>& cells,
                                            int& grid_w, int& grid_h);

// Builds the WATER passability grid in the SAME raw IsoMapPack5 (x, y) space
// and the SAME grid_w*grid_h shape as build_passability -- a cell is water-
// passable (1) iff its resolved LandType is Water AND it is materialized
// (`passable[cell]`), else 0 [RE-GATE APPLIED: naval.md §1/§2 -- BINARY
// passability (the game's Ground[LandType][SpeedType]==0.0 gate), intersected
// with the Land grid so a ship never paths on a non-materialized water cell
// ("Land grid filtered by Float passability"); a speed-weighted cost is a
// carry-forward]. `land_types` is the per-cell LandType array main.cpp resolves
// from the Theater (resolve_land_types), indexed cell = y*grid_w + x, same as
// passable_. `passable` is the UNCHANGED Land grid (build_passability output).
// Cells beyond either array's size stay 0. Free function (not a World member) so
// it is unit-testable with hand-built LandType/passable vectors. Does NOT touch
// passable_ / build_passability -- the Land grid is literally unchanged (spec §8).
std::vector<std::uint8_t> build_water_passability(
    const std::vector<ra2assets::LandType>& land_types,
    const std::vector<std::uint8_t>& passable, int grid_w, int grid_h);

// --- AI-1 base-planning phase [RE: ai-base.md] -----------------------------
// Everything an AI house needs, resolved ONCE by main.cpp (assets/rules) and
// handed to World::enable_ai_house. World stays asset-free: the roster here
// is the same BuildingTypeDef list main.cpp already resolves (ba.roster),
// buildup_totals mirrors ba.art[t].buildup_total, groups mirrors ba.groups.
// --- AI-2 offensive-teams phase [RE: ai-team.md] ----------------------------
// Everything an AI house's TEAM brain needs, resolved ONCE by main.cpp
// (aimd.ini + unit roster + waypoint table) and handed inside AiHouseConfig.
// BYTE-IDENTITY FIREWALL: `enabled` defaults FALSE — every existing
// enable_ai_house caller (including --demo-ai-base) gets a team-less config,
// so step_ai_teams skips the house and all six gates stay bit-identical.
// Building class bits for the attack quarry types [G-61].
enum : std::uint8_t { kAiBldFactory = 1, kAiBldDefense = 2, kAiBldPower = 4, kAiBldTech = 8 };
struct AiTeamConfig {
    bool enabled = false;                 // the per-house team firewall
    AimdData aimd;                        // parsed TaskForce/Script/TeamType tables
    std::vector<UnitTypeDef> unit_roster; // producible/recruitable combat types
    std::vector<int> inf_factory_types;   // BUILDING roster indices (barracks)
    std::vector<int> veh_factory_types;   // BUILDING roster indices (war factory)
    std::vector<int> nav_factory_types;   // W2-AN-16: BUILDING roster indices (naval yard)
    std::vector<Cellxy> waypoints;        // waypoint index -> cell (main.cpp table)
    std::string house_name;               // aimd House= token (e.g. "Russians")
    // [G-61] rulesmd [BuildingTypes] index -> building-roster index (-1 = not
    // in the roster): resolves the 46/47/58 argument's low word.
    std::vector<int> building_type_roster;
    // [G-61] per building-roster index: kAiBld* class bits for the quarry
    // types 6/7/9/11 (factories / base defenses / power / tech).
    std::vector<std::uint8_t> building_class;
    // [G-68] IQ Harvester (=2 <= skirmish IQ 5): keep refineries x
    // HarvestersPerRefinery (2,2,1 by house+0x184) miners alive. 0 = off
    // (the AI gates' synthetic configs).
    int harvesters_per_refinery = 0;
    AiTeamRules rules;                    // recovered team knobs

    // --- AI-3 AITriggerTypes [RE: ai-trigger.md] -----------------------------
    // BYTE-IDENTITY FIREWALL: triggers_enabled defaults FALSE — every existing
    // caller (including --demo-ai-team) gets a trigger-less brain, the trigger
    // step never runs, and all SEVEN gates stay bit-identical. Only the
    // interactive path and --demo-ai-trigger set it true.
    bool triggers_enabled = false;  // the per-house trigger firewall
    int trigger_side = 0;           // the house's side for the SideIndex
                                    // filter (1=Allied, 2=Soviet, 3=Yuri;
                                    // triggers with side 0 match any house)
                                    // [RE-GATE APPLIED: ai-trigger.md §5 —
                                    // CONFIRMED 1-based encoding]
    int trigger_tech_level = 10;    // house tech level for the TechLevel
                                    // filter (house >= field passes)
                                    // [RE-FALLBACK: skirmish ceiling — no
                                    // recovered per-house tech input]
};

struct AiHouseConfig {
    AiDifficulty difficulty = AiDifficulty::Normal;  // default group [RE-FALLBACK: ai-base.md §4]
    AiCatalog catalog;
    AiRules ai;
    DifficultyMods mods;  // difficulty_mods(difficulty, groups)
    std::vector<BuildingTypeDef> roster;
    std::vector<double> buildup_totals;  // per roster index (0.0 = instant flip)
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    double build_speed = 0.7;            // rules [General] BuildSpeed
    double low_power_penalty = 1.0;      // rules LowPowerPenaltyModifier
    double start_credits = 0.0;          // [RE-GATE APPLIED: ai-base.md §4 —
                                         // NO starting-credit multiplier
                                         // exists in the difficulty sections;
                                         // this is the lobby/MPlayer money,
                                         // passed through unmodified]
    AiTeamConfig team;  // AI-2 [RE: ai-team.md]; enabled=false = no team brain
    // W2-SK-06 [RE: production.md §1.2 -- MultipleFactory applies to every
    // house]: each extra same-class factory multiplies build time by this.
    // 1.0 = off (the synthetic gate configs); real play passes rules 0.8.
    double multiple_factory = 1.0;
    // W5-A R-028 [RE: HouseClass AI FUN_004FD320 -- the superweapon pass
    // FUN_00509710 runs only when Rules [IQ] SuperWeapons <= house IQ
    // (+0x24C, the map's [<House>] IQ=); skirmish AI houses get MaxIQLevels]:
    // false = this house never fires its superweapons by itself.
    bool fire_superweapons = true;
    // W6-03-03 [RE: FUN_00522a80 / FUN_0073c560 @0x73D30B -- an unload adds
    // n * PurifierBonus * value where n = own purifiers (house+0x538c) PLUS,
    // for a computer house outside a campaign, Rules AIVirtualPurifiers
    // [house+0x184] (Rules+0x1324; rulesmd 4,2,0 = h,m,e)]. 0 = none (the
    // campaign and every synthetic gate config).
    int virtual_purifiers = 0;
};

// Holds a reference to the (caller-owned, long-lived) MapFile plus the placed
// Units, the passability grid, and a cell-occupancy grid derived from the
// map's cells. Does not copy the map's cell data -- the caller must keep the
// MapFile alive at least as long as the World. Pure engine-layer logic; no SDL.
class World {
public:
    // P7 Save/Load: the serializer (engine/save_fields.hpp) visits private state.
    friend struct SaveAccess;

    explicit World(const ra2assets::MapFile& map);

    const ra2assets::MapFile& map() const { return *map_; }
    // W5-B: terrain height level of a cell (0 off-map) -- the Z of 3-D
    // distances (level * 104 leptons, Unsorted::LevelHeight).
    int cell_level(int x, int y) const;
    // W5-B R-061: up to three infantry per cell (sub-cell spots). Called by
    // the real game (GameScene, w5_arm_combat); off = one unit per cell.
    void enable_subcells();
    bool subcells_enabled() const { return occ_.subcells(); }
    // Re-register unit idx's occupancy as infantry (spot) or whole-cell.
    void subcell_register(int idx);
    // Derive Unit::spot / sub_x / sub_y from the occupancy and the walk.
    void update_sub_offsets();
    // Put infantry idx into spot `spot` (2..4) of its current cell.
    void place_in_spot(int idx, int spot);
    // W5-B R-050 [RE: UnitClass ShouldCrush FUN_00742720, Mission_Attack
    // FUN_007402F0]: a Crusher whose crushable enemy target is within
    // [CombatDamage] Crush (1.8 cells = 460 leptons, 3-D) drives onto it
    // instead of firing -- the computer always (house IQ >= [IQ] AutoCrush),
    // the player only with PlayerAutoCrush=yes or an AutoCrush=yes type.
    struct CrushRules {
        bool armed = false;
        int crush_leptons = 460;
        bool player_auto_crush = false;
        bool ai_any_building = false;   // R-064 (kept here to share the save block)
    };
    void arm_crush(const CrushRules& r) { crush_rules_ = r; }
    // W5-B R-063 [RE: TechnoClass in-range FUN_006F6750, elevation bonus
    // FUN_006F6490, cliff trace FUN_004CC020/FUN_004CC280]: 3-D weapon range,
    // [ElevationModel] range bonus and the SubjectToCliffs line trace.
    struct ElevationRules {
        bool armed = false;
        int increment = 4;        // ElevationIncrement (levels)
        double bonus = 2.0;       // ElevationIncrementBonus (cells per increment)
        double cap = 2.0;         // ElevationBonusCap (cells)
    };
    void arm_elevation(const ElevationRules& r) { elev_rules_ = r; }
    const ElevationRules& elevation_rules() const { return elev_rules_; }
    const CrushRules& crush_rules() const { return crush_rules_; }
    // W5-B: true for a computer-controlled (AI) house.
    bool house_is_computer(int h) const { return ai_enabled(h); }
    // W5-B R-064 switch (armed by w5_arm_combat): computer units' idle threat
    // scan includes unarmed buildings.
    bool ai_threat_any_building() const { return crush_rules_.ai_any_building; }
    // W5-B R-062: arm per-cell land speed. `land` = LandType per cell from the
    // TMP terrain-type byte (DAT_008278F4 table), `ramp` = TMP ramp type;
    // `cost` = rulesmd [LandType] Foot..FloatBeach fractions (capped at 1).
    struct LandSpeedRules {
        float cost[12][8] = {};
        double tracked_up = 1.0, tracked_down = 1.0, wheeled_up = 1.0, wheeled_down = 1.0;
        double condition_red = 0.25;
    };
    void arm_land_speed(const std::vector<std::uint8_t>& land, const std::vector<std::uint8_t>& ramp,
                        const LandSpeedRules& r);
    bool land_speed_armed() const { return land_speed_armed_; }
    int speed_land_at(int x, int y) const;   // effective LandType (ore -> Tiberium)
    // Cells per second unit i moves at right now (flat speed x speed_mult).
    double unit_speed_cps(int i) const;

    std::vector<Unit>& units() { return units_; }
    const std::vector<Unit>& units() const { return units_; }

    // Append a unit at iso cell (cell_x, cell_y) with SHP image name + frame;
    // seeds its level from the map and claims its cell in the occupancy grid.
    // Returns a reference to the new unit (valid until units() is reallocated).
    // Precondition: (cell_x, cell_y) is walkable and unoccupied -- callers
    // check walkable() and unit_at_cell() first; seeding two units onto one
    // cell silently breaks the one-unit-per-cell invariant.
    Unit& add_unit(int cell_x, int cell_y, const std::string& image, int frame = 0);

    // Append a combat-capable unit at (cell_x, cell_y): mirrors add_unit
    // (image = c.image, frame 0, level seed, occupancy claim) and additionally
    // resolves `house` plus hp/max_hp (from c.strength), armor_ix
    // (armor_index(c.armor)), and the weapon fields (w_damage/w_rof/w_range
    // from c.damage/c.rof/c.range, w_verses = c.verses). Same precondition as
    // add_unit: (cell_x, cell_y) must be walkable and unoccupied -- callers
    // check walkable() and unit_at_cell() first; seeding two units onto one
    // cell silently breaks the one-unit-per-cell invariant.
    Unit& add_combat_unit(int cell_x, int cell_y, int house, const UnitCombat& c);

    // Registers (or replaces) the animation sequence used for Unit::image ==
    // `image`; anim_for_image falls back to default_anim() for any image
    // never registered here (main.cpp/Task 4 wires this from artmd.ini).
    void set_anim(const std::string& image, const UnitAnimSeq& seq);
    // W3-B: the live game's infantry DoType machine (inf_anim.hpp). Off in
    // every gate/test World (frames then come from set_anim's 4 entries).
    // `human_house` = the player's house (no fear-drop while it is moving),
    // `idle_frequency` = rules [AudioVisual] IdleActionFrequency.
    void enable_infantry_anims(int human_house, double idle_frequency) {
        inf_anims_ = true;
        inf_human_ = human_house;
        inf_idle_freq_ = idle_frequency;
    }
    bool infantry_anims() const { return inf_anims_; }
    void set_inf_table(const std::string& image, const InfTable& t) { inf_tables_[image] = t; }
    const InfTable* inf_table(const std::string& image) const {
        auto it = inf_tables_.find(image);
        return it == inf_tables_.end() ? nullptr : &it->second;
    }
    // Trigger action 113 MakeHouseCheer / the victory cheer: every idle
    // infantry of `house` plays its Cheer sequence.
    void make_house_cheer(int house) {
        for (Unit& u : units_)
            if (u.alive && u.die_time < 0.0 && u.house == house && !u.is_vehicle) u.ia.cheer = true;
    }

    // Deselect every unit.
    void clear_selection();
    // Index of the unit whose CURRENT cell is (x, y), or -1 if none -- skips
    // dying and dead units (a corpse is unpickable; its cell was already
    // released by the time it started dying, so it never occupies anyway).
    int unit_at_cell(int x, int y) const;

    // Passability grid accessors (iso space; see build_passability).
    int grid_w() const { return grid_w_; }
    int grid_h() const { return grid_h_; }
    const std::vector<std::uint8_t>& passable() const { return passable_; }
    // G-87 (presentation/map dressing): a map [Terrain] object, wall overlay or
    // civilian building blocks its cell for ground movement. Live game only.
    void block_cell(int x, int y) {
        if (x >= 0 && y >= 0 && x < grid_w_ && y < grid_h_ && !passable_.empty())
            passable_[static_cast<std::size_t>(y) * grid_w_ + x] = 0;
    }
    // W5-A: a wall overlay cell -- blocked like block_cell, and flagged in the
    // diagonal leak mask (pathfind.hpp: two walls touching only at a corner
    // leave a gap units walk through). Derived from the map, never saved.
    void block_wall_cell(int x, int y) {
        if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || passable_.empty()) return;
        if (wall_crushed(x, y)) return;   // P4: crushed by a tank (a reload re-blocks the dressing)
        block_cell(x, y);
        if (wall_leak_.size() != passable_.size()) wall_leak_.assign(passable_.size(), 0);
        wall_leak_[static_cast<std::size_t>(y) * grid_w_ + x] = 1;
    }
    const std::vector<std::uint8_t>* wall_leak() const {
        return wall_leak_.size() == passable_.size() && !wall_leak_.empty() ? &wall_leak_ : nullptr;
    }
    // P4 crushable walls [RE-FALLBACK: RA1 UnitClass::Can_Enter_Cell (a
    // Crusher entering a cell whose overlay is Crushable= gets MOVE_OK) and the
    // per-cell crush of that overlay; the data is RA2's OverlayType
    // Crushable=yes (GASAND sandbags, CAFNCP/CAFNCB/CAFNCW fences, ...)]: a
    // Crusher land vehicle paths through a standing crushable wall cell and
    // flattens it when it drives in. 0 none / 1 standing / 2 crushed (saved).
    void mark_crushable_wall(int x, int y) {
        if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || passable_.empty()) return;
        if (crush_wall_.size() != passable_.size()) crush_wall_.assign(passable_.size(), 0);
        std::uint8_t& c = crush_wall_[static_cast<std::size_t>(y) * grid_w_ + x];
        if (c == 0) c = 1;
    }
    bool wall_crushed(int x, int y) const {
        if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || crush_wall_.size() != passable_.size()) return false;
        return crush_wall_[static_cast<std::size_t>(y) * grid_w_ + x] == 2;
    }
    bool crush_paths(const Unit& u) const {
        return !crush_wall_.empty() && u.crusher && u.is_vehicle && u.movement_domain == MovementDomain::Land;
    }
    const std::vector<std::uint8_t>& crush_grid() const;           // passable_ + standing crushable walls
    const std::vector<std::uint8_t>* crush_leak() const;           // wall_leak_ minus them
    // CB-3 Naval [RE: naval.md §1]: the passability grid for a movement domain.
    // Land (the DEFAULT -- every existing unit) returns the SAME passable_
    // object every existing A* site already reads (byte-identical); Water
    // returns the new water_passable_ grid. Structural byte-identity: a
    // default-Land unit resolves to the identical grid object (spec §8).
    const std::vector<std::uint8_t>& grid_for(MovementDomain d) const {
        if (d == MovementDomain::Amphibious) return amphib_grid();
        return d == MovementDomain::Water ? water_passable_ : passable_;
    }
    // W2-AN-10: Land OR Water, rebuilt on every call into a cached buffer
    // (derived state, never saved; both source grids mutate in many places).
    const std::vector<std::uint8_t>& amphib_grid() const;
    // W2-AN-09: can unit `idx` stand on cell (x, y) by its movement domain
    // (aircraft: any in-bounds cell)?
    bool unit_can_enter_cell(const Unit& u, int x, int y) const;
    // W2-AN-09: is (x, y) enterable by ANY selected live house-`house` unit?
    // No selection -> the Land grid (the pre-fix answer).
    bool selection_can_enter(int house, int x, int y) const;
    // CB-3 Naval [RE-GATE APPLIED: naval.md §1/§2]: build the WATER grid from a
    // per-cell LandType array (main.cpp's resolve_land_types, indexed y*grid_w+x),
    // intersected with the Land grid passable_ (a ship never paths on a non-
    // materialized water cell). Opt-in -- called ONLY by the naval demo; no
    // existing demo calls it, so water_passable_ stays empty and no naval path is
    // ever computed elsewhere (spec §8). World stays asset-free: it takes the
    // resolved array, not a map.
    void set_water_terrain(const std::vector<ra2assets::LandType>& land_types);
    // Gap fix C (G-43) [RE: naval.md §2]: set_water_terrain PLUS close the Land
    // grid (passable_) on Water / Rock (cliffs, water cliffs) / Wall cells --
    // Foot/Track/Wheel are 0% there. Called by launch_skirmish and the
    // campaign world builder (the real game); gates that never call it keep
    // the all-walkable grid.
    void set_land_terrain(const std::vector<ra2assets::LandType>& land_types);
    // P6 [RE: roster2.md §B]: a WaterBound building (GAYARD/NAYARD) sits ON the
    // water grid -- add_building only blocks the Land grid, so the caller also
    // clears its foundation from water_passable_ (ships never path through the
    // yard; NumberImpassableRows not modelled). No-op when the water grid is
    // empty. Called ONLY by --demo-roster2.
    void block_water_footprint(int cx, int cy, int fw, int fh);
    bool walkable(int x, int y) const;
    const Occupancy& occupancy() const { return occ_; }

    // --- Economy phase [RE: economy.md] ---
    // Ore state built from the ctor map's overlay (empty if the map has
    // none, or the map's has_overlay() is false).
    OreField& ore() { return ore_; }
    const OreField& ore() const { return ore_; }

    // Roster copy + refinery type ids so step() can run harvest logic.
    // `types_.empty()` (never called) gates OFF ore_.step/step_harvest in
    // step() entirely -- a World that never calls this (every pre-economy-
    // phase caller, including demo-base) steps with zero ore/harvest
    // activity, byte-identical to before this phase landed.
    void set_economy(std::vector<BuildingTypeDef> types, std::vector<int> refinery_types,
                     OreValues values, double growth_interval_secs,
                     double growth_pct, double spread_pct);

    // Give the World a building-type roster for TYPE LOOKUPS ONLY, without
    // arming ore growth / harvest stepping.
    //
    // Why this is a SEPARATE vector and not just `types_`: `types_.empty()` IS
    // the economy firewall (see its declaration below) -- step() skips
    // ore_.step and step_harvest precisely when it is empty. The interactive
    // launcher (launch_skirmish) never calls set_economy, so types_ is empty
    // there, and every repair/sell type lookup silently refused: the wrench did
    // nothing in the real game while all 19 gates stayed green, because gates
    // build their worlds through wire_economy instead [RE: repair-sell.md §A4;
    // Task 4 report §6]. Writing types_ here would fix the lookup but
    // simultaneously switch the economy passes ON in the interactive/skirmish
    // world, moving skirmish_hash and maps_hash. So the roster lands in its own
    // field and `btypes()` prefers whichever is populated.
    void set_building_types(std::vector<BuildingTypeDef> types) { lookup_types_ = std::move(types); }
    // G16 (agent G): the roster grew AFTER set_economy (map civilians / bridge
    // huts registered by the live game) -- refresh whichever copy btypes() reads.
    void refresh_building_types(const std::vector<BuildingTypeDef>& types) {
        lookup_types_ = types;
        if (!types_.empty()) types_ = types;
    }
    // Gap owner A (G-27) [RE: economy.md §2]: rulesmd [General] PurifierBonus.
    void set_purifier_bonus(double b) { purifier_bonus_ = b; }
    // W2-SK-02: rules [General] TiberiumLongScan / TiberiumShortScan (cells).
    void set_harvest_scan(int long_scan, int short_scan) {
        harvest_scan_.long_scan = long_scan > 0 ? long_scan : kOreScanRadius;
        harvest_scan_.short_scan = short_scan > 0 ? short_scan : 6;
    }
    const HarvestScan& harvest_scan() const { return harvest_scan_; }
    // W2-SK: total refinery unload income a house has earned (probe/soak).
    double harvest_income(int house) const {
        return house >= 0 && house < (int)harvest_income_.size() ? harvest_income_[(std::size_t)house] : 0.0;
    }
    // W2-SK-01: register a map ore mine (TerrainType SpawnsTiberium=yes).
    void add_ore_mine(int x, int y, int half_frames, int anim_rate, double anim_prob) {
        ore_.add_mine(x, y, half_frames, anim_rate, anim_prob);
    }

    // The type roster for non-economy consumers (repair/sell). set_economy's
    // roster wins when present so gate worlds behave exactly as before.
    const std::vector<BuildingTypeDef>& btypes() const {
        return types_.empty() ? lookup_types_ : types_;
    }

    // Harvest income accumulated (both houses' unload drains summed since
    // the last call to take_income for THAT house) since the last call,
    // then zeroed for `house`. House index must be 0 or 1; anything else
    // returns 0 untouched.
    double take_income(int house);

    // Spawn a harvester: add_combat_unit (same placement precondition: the
    // caller already checked walkable()/unit_at_cell()) plus the harvester
    // fields, starting the mission at HarvState::SeekOre (a freshly built
    // harvester starts hunting immediately, matching the original).
    Unit& add_harvester(int cell_x, int cell_y, int house, const UnitCombat& c,
                        bool chrono, int storage_bales);
    // Gap owner A (G-22): a War Factory-built HARV/CMIN leaves through the
    // ordinary launch_unit exit, then becomes a miner hunting ore exactly like
    // add_harvester's (SeekOre). No-op on a bad index.
    void make_harvester(int unit_idx, bool chrono, int storage_bales) {
        if (unit_idx < 0 || unit_idx >= static_cast<int>(units_.size())) return;
        Unit& u = units_[unit_idx];
        u.is_harvester = true;
        u.chrono_miner = chrono;
        u.storage_bales = storage_bales;
        u.harv = HarvState::SeekOre;
    }

    // Player orders (selected house-0 harvesters only). Simplified per
    // economy.md §1: no separate "pinned" flag -- just pin harv_cell_* to
    // (x, y), set the mission to MoveToOre, and emit the path immediately
    // via order_unit_move (mirrors step()'s own HarvestMove handling).
    void order_harvest(int x, int y);
    // ReturnToRef now (resets harv_timer -- every transition into
    // ReturnToRef resets it; see harvest.cpp's ChronoOut-fires-once comment).
    void order_return();

    // Drained by main.cpp each frame for audio (chrono/unload sounds); empty
    // if set_economy was never called (step() never populates it then).
    std::vector<HarvestEvent> take_harvest_events();

    // Movement speed in iso cells/sec (see main.cpp for the Rules-derived value).
    void set_move_speed(double cells_per_sec) { move_cps_ = cells_per_sec; }
    double move_speed() const { return move_cps_; }

    // Order every SELECTED unit to walk toward (goal_x, goal_y): assigns each a
    // distinct formation cell spiralling out from the goal, then paths it there.
    // Units retargeted mid-walk keep their interpolated fx/fy (no backward
    // snap). Dying/dead selected units are skipped. No-op if nothing eligible
    // is selected.
    // Economy phase addition [RE: economy.md §1 auto-resume]: a selected
    // is_harvester unit also gets manual_hold = true, so step_harvest parks
    // its mission FSM for the duration of this player-directed move and
    // resumes (at SeekOre) the instant it arrives -- a ground right-click on
    // a harvester keeps this existing move path rather than a dedicated one.
    void order_move_selected(int goal_x, int goal_y);

    // Order every SELECTED house-0, alive, non-dying unit to attack
    // `target_idx`: sets target=target_idx and attack_order=true (pursues
    // indefinitely -- step_combat's next MoveRequest paths it there via
    // step()), and releases/re-asserts its occupancy claim and clears its
    // path the same way order_move_selected does (a fresh pursuit path is
    // computed later by step_combat's request, not here). No-op (touches no
    // unit) if target_idx is out of range, dead, dying, or itself house 0 --
    // a house-0 (player) unit can't be ordered to attack a friendly.
    void order_attack(int target_idx);

    // Placed buildings (Buildings & Construction Task 4).
    std::vector<Building>& buildings() { return buildings_; }
    const std::vector<Building>& buildings() const { return buildings_; }

    // Appends a Building at foundation origin (cx, cy), sized fw x fh, owned
    // by `house`, with hp = max_hp = strength and the given armor_ix (used by
    // combat's damage_vs). State starts Buildup unless `instant` (scripted
    // spawns pass true), in which case it starts Idle. Clears passable_ for
    // every foundation cell (foundation_cells) so units can no longer walk
    // through it -- restored automatically by World::step when the building
    // dies. Returns the new building's index (stable -- buildings are never
    // erased, only tombstoned). Precondition: the caller already validated
    // the placement via placement_check (this method does not re-check it).
    int add_building(int type, int cx, int cy, int fw, int fh, int house, int strength,
                     int armor_ix, double buildup_total, bool instant, int sight = 0,
                     bool grants_map_reveal = false);

    // Index of the alive, non-Dead building whose foundation contains (x, y),
    // or -1 if none (ascending index; foundations never overlap while both
    // are alive, so at most one can ever match).
    int building_at_cell(int x, int y) const;

    // Orders every SELECTED house-0, alive, non-dying unit to attack building
    // `b_idx`: target_building = b_idx, target = -1 (mutually exclusive with
    // order_attack's unit target), attack_order = true, plus the same
    // occupancy-release/path-clear dance as order_attack (a fresh pursuit
    // path is computed later by step_combat's request, not here). No-op
    // (touches no unit) if b_idx is out of range, dead, in State::Dead, or
    // owned by house 0 (a house-0 unit can't be ordered to attack a
    // friendly building).
    void order_attack_building(int b_idx);

    // --- Production phase [RE: production.md §1.3/§1.4] ---
    // Set factory `b_idx`'s rally cell (finished units path here after exiting).
    // No-op if b_idx is out of range or not an alive building.
    void set_rally(int b_idx, int x, int y);

    // Make `b_idx` its house's primary factory of its category (factory::
    // set_primary over `factory_types`, the roster-index list for ONE category).
    // No-op if b_idx is out of range.
    void set_primary(int b_idx, const std::vector<int>& factory_types);

    // Launch a finished COMBAT unit from `factory_idx`: exit_cell() walks the
    // 8-ring from the factory's anchor to the first FREE (walkable, unoccupied)
    // cell [RE-GATE APPLIED: production.md §1.3]; add_combat_unit there (house
    // = factory's house, stats from `c`), then order it to the factory's rally
    // cell (has_rally) else the DEFAULT GATHER CELL — the nearest free cell
    // from the building's own coordinate [RE-GATE APPLIED: production.md §1.4,
    // FUN_00500020: the exit-adjacent gather cell, NOT the map center] —
    // computed by re-running the ring walk now that the unit occupies the door
    // cell. order_unit_move cancels cleanly if unreachable. Returns the new
    // unit index; -1 if NO ring cell was free (caller HOLDS the queue ready)
    // or factory_idx is invalid. Asset-free: the caller resolves c + factory_def.
    int launch_unit(int factory_idx, const BuildingTypeDef& factory_def, const UnitCombat& c);

    // True iff `house` owns NO alive Idle factory whose type is in
    // factory_types (the caller then cancels+refunds that queue; a survivor ->
    // false, the queue continues). Mirrors ps_tick's ConYard-died cancel gate.
    bool factory_category_lost(const std::vector<int>& factory_types, int house) const;

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Enter order: every SELECTED house-0, alive, non-dying unit that passes
    // can_load NOW (capacity is re-checked at boarding -- orders may
    // oversubscribe) gets enter_target = t_idx, drops any engagement, and
    // paths toward the transport through assign_formation around its cell
    // (a raw move to the transport's own occupied cell would cancel as
    // unreachable) [spec §3]. No-op if t_idx is not an own, alive, non-dying
    // transport. Sets the cargo_any_ firewall.
    void order_enter_transport(int t_idx);
    // Unload order: sets unload_pending on every SELECTED house-0, alive,
    // non-dying transport with non-empty cargo [spec §5]. Any new move/
    // attack order on the transport clears unload_pending (an order
    // supersedes -- our order-model convention, not overturned by the hunt).
    // Sets the cargo_any_ firewall.
    void order_unload_selected();
    // W2-CA-17: the same unload order for one transport of any house (team
    // script action 8). No-op unless it is alive with cargo.
    void order_unload(int idx);
    // W4-SQ-01 [RE: FUN_0065d770 -> FUN_004732d0 AddPassenger]: a campaign
    // reinforcement created INSIDE its transport. The passenger never touches
    // the map (no occupancy) and the original adds it without a capacity
    // test. Returns the new unit index, -1 when the transport is not alive.
    int add_unit_in_transport(int t_idx, int house, const UnitCombat& c);
    // W4-SQ-01 [RE: FUN_006ec760 team Load]: one unit of ANY house walks to
    // and boards transport t_idx (can_load re-checked on arrival).
    bool order_board(int p_idx, int t_idx);
    // W4-SQ-06 [RE: FUN_0065d320 Droppod branch]: a paradrop plane (an AN
    // type registered as `plane_id`) enters at (sx,sy) flying, drops its CARGO
    // over (dx,dy) and leaves over (ex,ey). -1 when the plane type is unknown.
    // W4-SQ-03: a fixed-wing aircraft with no dock leaves over the nearest
    // map edge and is removed (not a loss: deployed_away).
    bool order_leave_map(int idx);
    int spawn_drop_plane(const std::string& plane_id, int house, int sx, int sy, int dx, int dy,
                         int ex, int ey);
    // Drain the board/refuse/eject events accumulated by step_cargo since
    // the last call (the take_harvest_events pattern). Empty if no cargo
    // activity ever happened.
    std::vector<CargoEvent> take_cargo_events();

    // --- AI-1 base-planning phase [RE: ai-base.md] ---
    // Flag `house` as AI-driven with the given config. The planner then runs
    // inside step() (step_ai_houses), driving the EXISTING ProductionQueues /
    // placement_check / add_building machinery — no new production or
    // placement code. BYTE-IDENTITY FIREWALL: a World that never calls this
    // steps EXACTLY as before (step_ai_houses early-returns on a single
    // bool), and none of the AI fields is read by any hash.
    void enable_ai_house(int house, AiHouseConfig cfg);
    // [G-69] Campaign houses: install an explicit base-node plan (the map's
    // [<House>] NodeCount/NNN=TYPE,X,Y list) instead of the lazily built
    // catalog plan, and link every standing own building to the node of its
    // type (nearest node cell first). Call right after enable_ai_house.
    void set_ai_base_nodes(int house, const std::vector<PlannedNode>& nodes);
    bool ai_enabled(int house) const;
    // Test/demo accessors (read-only views of the per-house AI record;
    // nullptr / 0 / -1 when `house` is not AI-flagged).
    const AiBaseState* ai_state(int house) const;
    // Read-only view of an AI house's resolved config (diagnostics/harness).
    const AiHouseConfig* ai_config(int house) const {
        return ai_enabled(house) ? &ai_houses_[house].cfg : nullptr;
    }
    double ai_credits(int house) const;
    // Roster type building in the AI house's Structure (cat 0) or Defense
    // (cat 1) queue; -1 = idle.
    int ai_producing(int house, int cat01) const;

    // --- AI-2 offensive-teams phase [RE: ai-team.md] ---
    // Read-only team state (nullptr when `house` is not AI-flagged). NOT read
    // by world_hash — the new gate hashes it separately (ai_team_hash).
    const AiTeamState* ai_team_state(int house) const;
    // Unit-roster type building in the AI house's Infantry (0) or Vehicle (1)
    // queue; -1 = idle.
    int ai_unit_producing(int house, int iv01) const;

    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    // Read-only trigger state (nullptr when `house` is not AI-flagged). NOT
    // read by world_hash — the 8th gate hashes it separately (ai_trigger_hash).
    const AiTriggerState* ai_trigger_state(int house) const;

    // --- CB-4 Superweapons [RE: superweapon.md] -----------------------------
    // A NEW per-house superweapon registry, INDEPENDENT of the AI registry so
    // it works for the PLAYER house 0 (which is never in ai_houses_). Default
    // INERT: no house has a SW until grant_superweapon, which no existing demo
    // calls -> sw_houses_ empty, sw_any_ false, step_superweapon a strict no-op
    // (spec §8). Unit and combat.cpp behavior are untouched.
    //
    // Grant `house` a superweapon: grow sw_houses_ to cover it, store the pure
    // lifecycle (type + recharge_secs already in SECONDS) and the resolved
    // detonation params ([NukePayload] Damage + [NUKE] CellSpread/PercentAtMax/
    // Verses), and open the sw_any_ firewall. verses is the 11-slot armor array
    // (int percents, matching Unit::w_verses / damage_vs).
    void grant_superweapon(int house, SuperweaponType type, double recharge_secs,
                           int dmg, double cell_spread, double pmax,
                           const std::array<int, 11>& verses);
    // Fire `house`'s superweapon at cell (cx, cy): no-op returning false unless
    // that house's SW is READY; on fire it detonates at the cell (§5) and resets
    // the timer (§4). Returns whether it fired (the wedge guard reads this).
    // [Task 4 lands the detonation body; Task 3 lands the registry it uses.]
    bool fire_superweapon(int house, int cx, int cy);
    // Read-only SW state for the sidebar (§6) and the gate (§7). false / 0 /
    // nullptr when `house` has no granted SW.
    bool sw_ready(int house) const;
    double sw_charge_fraction(int house) const;
    const Superweapon* sw_state(int house) const;
    // Gap owner A (G-25/G-29) [RE: superweapon.md §1 -- a house holds a SW
    // while it owns the SuperWeapon= building; IsPowered=true pauses the
    // charge under low power; construction.md §8 -- low power = output <
    // drain]. revoke drops the grant (building lost); set_superweapon_powered
    // records the SW type's IsPowered flag.
    // W2-AN-04: a house holds ONE record per granted SW type (sw_houses_[h]
    // is a list). The single-SW calls above/below act on the house's FIRST
    // record (the historical one-SW behaviour every gate/test uses); the
    // typed calls further below address one type.
    void revoke_superweapon(int house) {
        if (house >= 0 && house < static_cast<int>(sw_houses_.size())) sw_houses_[house].clear();
    }
    void set_superweapon_powered(int house, bool is_powered) {
        if (house >= 0 && house < static_cast<int>(sw_houses_.size()) && !sw_houses_[house].empty())
            sw_houses_[house].front().is_powered = is_powered;
    }
    // Low power for `house`: Idle buildings' output (after a spy blackout) <
    // their drain, over btypes(). The query every low-power consumer shares
    // (SW charge here; radar, Powered=yes defences).
    bool house_low_power(int house) const;

    // ===== Wave-2 W2-AN: superweapons v2 (world_sw.cpp) =====================
    // Grant `house` a SW of `type` (a fresh full charge) unless it already
    // holds one; Nuke records carry the [NukePayload]/[NUKE] params given.
    void grant_superweapon_type(int house, SuperweaponType type, double recharge_secs,
                                bool is_powered, int dmg = 0, double cell_spread = 0.0,
                                double pmax = 1.0, const std::array<int, 11>& verses = {});
    void revoke_superweapon_type(int house, SuperweaponType type);
    const Superweapon* sw_state(int house, SuperweaponType type) const;
    bool sw_ready(int house, SuperweaponType type) const;
    double sw_charge_fraction(int house, SuperweaponType type) const;
    // The SW types `house` holds, in grant order (the sidebar slot order).
    std::vector<SuperweaponType> sw_types(int house) const;
    // W5-A: the house's AI superweapon quarry (map action 35 PreferredTarget,
    // YRpp QuarryType; 1 Anything = the default scoring). Derived state the
    // campaign runtime re-pushes every frame (it saves it), never saved here.
    void set_sw_quarry(int house, int quarry) {
        if (house < 0) return;
        if ((int)sw_quarry_.size() <= house) sw_quarry_.resize((std::size_t)house + 1, 1);
        sw_quarry_[(std::size_t)house] = quarry;
    }
    // Fire `house`'s `type` at (cx, cy) [Chronosphere: (cx,cy) = source,
    // (cx2,cy2) = destination]. Refused (false) unless that SW is READY.
    // Nuke: a timed two-leg flight (SwStrike) then the blast; Iron Curtain:
    // 3x3 invulnerability; Chronosphere: 3x3 teleport; Lightning Storm: a
    // storm (SwStrike); (Amer)ParaDrop: a paradrop plane.
    bool fire_superweapon_at(int house, SuperweaponType type, int cx, int cy, int cx2 = -1,
                             int cy2 = -1);
    // W3-FX map trigger actions (campaign host). 90 LightningStrikeAt [RE:
    // FUN_00539e70 -- a WeatherConClouds cloud at the cell whose bolt strikes
    // it]: one storm bolt (damage + the Bolt presentation event) at (cx, cy)
    // [RE-FALLBACK: the cloud's manifest delay before the bolt is not
    // modelled -- the bolt lands at once]. 102 [RE: Execute 0x66 -> unless
    // LightningStorm::IsActive (FUN_00539e30), LightningStorm::Start(cell,
    // house) FUN_00539be0]: a full storm owned by `house`, outside any SW
    // charge. Both return false off the map (102: also while a storm rages).
    bool trigger_lightning_strike(int house, int cx, int cy);
    bool trigger_lightning_storm(int house, int cx, int cy);
    void set_sw_params(const SwParams& p) { an_.sw = p; }
    // Building roster index -> the SuperweaponType (int) it grants (0 none):
    // finds the Nuke's silo for the launch point.
    void set_sw_building_types(const std::vector<int>& t) { an_.sw_btype = t; }
    // UnitRepair=yes (Naval Yard) and Helipad=yes (Airforce Command) flags per
    // building roster index.
    void set_an_building_flags(const std::vector<int>& repair, const std::vector<int>& airfield) {
        an_.repair_btype = repair;
        an_.airfield_btype = airfield;
    }
    const SwParams& sw_params() const { return an_.sw; }
    const std::vector<SwStrike>& sw_strikes() const { return an_.strikes; }
    // The presentation feed (append-only, capped; read by sequence number).
    const std::vector<SwEvent>& sw_events() const { return an_.events; }
    // Paradrop / spawner / missile unit types (rules id -> resolved combat),
    // registered by the scene from the roster's Spawns= / the SW params.
    void register_an_type(const std::string& id, const UnitCombat& c) { an_.types[id] = c; }
    bool has_an_type(const std::string& id) const { return an_.types.count(id) != 0; }
    // ===== Wave-2 W2-AN: naval / aircraft (world_an.cpp) =====================
    // A cloaked (submerged) enemy sub that `house` has no sonar contact on.
    bool unit_hidden_from(int house, const Unit& v) const {
        // W4: a rocket still on its launcher (RocketLocomotion pause / tilt)
        // is not in the air and not a target [PROBABLE: ALL01's Liberty
        // Island Patriots sit 7 cells from the Dreadnoughts and never blow up
        // the fleet's own missiles on deck in the original].
        if (rocket_on_launcher(v)) return true;
        return v.an.submerged && house >= 0 && house < 8 && v.house != house &&
               !(v.an.detected_mask & (1u << house));
    }
    static bool rocket_on_launcher(const Unit& v) {
        return v.an.t.missile && v.an.rocket_end == 0 && v.an.rocket.phase < kRocketClimb;
    }
    // Dock pads of an airfield building (cells), GAAIRC DockingOffset0..3.
    void set_dock_offsets(const std::vector<std::pair<int, int>>& offs) { an_.dock_offsets = offs; }
    // A fixed-wing/landing aircraft order: strike unit/building/cell, or fly to
    // a cell. Returns false when `idx` is not a fixed-wing aircraft.
    bool order_fixed_wing(int idx, int unit_target, int building_target, int gx, int gy);

    // --- Multi-house / Alliances phase [RE: multihouse.md §1/§2, D1] ---------
    // On-screen sides (the [Sides] legacy GDI=Allied / Nod=Soviet / ThirdSide=Yuri
    // are resolved to these). Neutral = Civilian/Special (non-combatant).
    enum class Side { Allied, Soviet, Yuri, Neutral };
    // One record per house (including player 0). Defaults reproduce today's world
    // 0=player Allied / 1=enemy Soviet / 2=neutral EXACTLY. NEVER read by
    // world_hash. color_ix indexes the 8 MP colors (or a default). alliance_team
    // sentinel -1 resolves to the house's own id (self-only team = FFA).
    struct HouseRec {
        Side        side          = Side::Allied;
        int         color_ix      = 0;      // 0..7 into the 8 MP colors
        std::string country       = "";
        int         alliance_team = -1;     // sentinel; -1 -> own id on read
        bool        combatant     = true;
        // W2-CA-02 [RE: HouseClass::ReadINI FUN_00500960 -> house+0x1ED]:
        // [<House>] PlayerControl=yes -- the local player selects and
        // orders this house's units as its own.
        bool        player_control = false;
    };
    // True for house 0 and every PlayerControl house (W2-CA-02).
    bool controllable(int h) const {
        return h == 0 || (h > 0 && h < (int)houses_.size() && houses_[h].player_control);
    }
    // Number of houses (default 3). The six per-house vectors + house_defeated_
    // are sized to this. Grows via set_house.
    int  house_count() const { return houses_.empty() ? 3 : (int)houses_.size(); }
    // Side / color for house h. With houses_configured_ FALSE -> today's exact
    // derivation (house 1 -> Soviet/red, else Allied/blue) -> byte-identical.
    Side house_side(int h) const;
    int  house_color(int h) const;
    // Render entry [RE: multihouse.md §2/D6]: with houses_configured_ FALSE ->
    // pal_for_house(unit_pal, house) EXACTLY (today's blue/red) -> pixel-identical.
    // Configured -> the 8-MP-color spread from house_color(house). NOT hashed.
    ra2assets::Palette house_palette(const ra2assets::Palette& unit_pal, int house) const;
    // Alliance team for h (sentinel -1 -> own id). Non-combatant houses keep their
    // own team but never ally/enemy anyone (see is_enemy/are_allied, Task 4).
    int  house_team(int h) const;
    // W6-03-01: Wall=/Insignificant= types never count for victory; walls are
    // never an AI quarry (overlays in the original).
    bool building_insignificant(const Building& b) const;
    bool building_is_wall(const Building& b) const;
    bool house_combatant(int h) const;
    // Configure house h. Grows houses_ (and the per-house vectors, once T3 lands)
    // to h+1 with defaults if needed, sets houses_[h]=rec, and OPENS the firewall
    // (houses_configured_ = true). No existing gate calls this -> byte-identical.
    void set_house(int h, HouseRec rec);
    bool houses_configured() const { return houses_configured_; }

    // --- Multi-house / Alliances phase [RE: multihouse.md §3/§4, D3] ---
    // Pairwise relationship predicates. With houses_configured_ FALSE ->
    // is_enemy(a,b) == (a != b) and are_allied(a,b) == (a == b) -> EXACTLY today's
    // behavior -> byte-identical in every gate. Neutral/non-combatant is NEVER an
    // enemy of anyone (both directions) and never allied to a house it doesn't
    // share a team with. Self is always allied to self. NOT read by world_hash.
    bool are_allied(int a, int b) const;
    bool is_enemy(int a, int b) const;
    // W2-CA-10 [RE: MakeAlly/MakeEnemy FUN_004f9990/FUN_004f9db0 are PAIRWISE,
    // campaign.md §2/§10]: the first call snapshots the team relation into a
    // per-pair matrix; from then on are_allied() reads the matrix, so
    // allying house b with house a never drags b's other relations along.
    // Inert (empty matrix) until a campaign trigger calls it.
    void set_pair_alliance(int a, int b, bool allied);
    // W4-SQ: one house's OWN view of another (HouseClass::IsAlliedWith reads
    // the house's own Allies= bits, so the relation can be one-sided); only
    // [a][b] changes. Same lazily-built matrix as set_pair_alliance.
    void set_ally_view(int a, int b, bool allied);
    // W4-SQ / W4-MC-06: the player just ordered the current selection (Input
    // calls this beside every selected-unit order). The campaign runtime
    // drains it and releases those units from their script teams. Transient
    // (not saved, not hashed).
    // W4-SQ-12 [RE-FALLBACK, RE-W4-SQ-2 not traced: the TS-lineage
    // HouseClass::AI_Unit trains what its under-strength teams still miss]:
    // the campaign hands each computer house the members its scripted
    // (CreateTeam) teams still need; step_ai_teams adds them to the AI-2 team
    // shortfall it trains for. Recomputed by the campaign every frame.
    void set_ai_script_demand(int house, std::vector<std::string> ids, std::vector<int> counts) {
        if (house < 0) return;
        if ((int)ai_demand_ids_.size() <= house) {
            ai_demand_ids_.resize((std::size_t)house + 1);
            ai_demand_counts_.resize((std::size_t)house + 1);
        }
        ai_demand_ids_[(std::size_t)house] = std::move(ids);
        ai_demand_counts_[(std::size_t)house] = std::move(counts);
    }
    void mark_player_order() {
        for (int i = 0; i < static_cast<int>(units_.size()); ++i)
            if (units_[i].selected && units_[i].alive) player_orders_.push_back(i);
    }
    std::vector<int> take_player_orders() { std::vector<int> o; o.swap(player_orders_); return o; }

    // AllyReveal [RE: multihouse.md §3, D4]: allied houses share revealed shroud.
    // Firewalled behind houses_configured_ + shroud_any_ -> OFF in every gate.
    // Default true (matches rules AllyReveal=yes) but only ACTIVE when alliances
    // are configured AND shroud is enabled. NOT hashed.
    void enable_ally_reveal(bool on) { ally_reveal_ = on; }
    bool ally_reveal_ = true;

    // --- Victory / defeat [RE: multihouse.md §5, D5] ---
    // Enable the greenfield last-team-standing victory pass. short_game=true
    // (ShortGame default) -> a combatant house is defeated when its last building
    // falls; false -> zero buildings AND zero units. Default OFF -> step_victory
    // early-returns -> gates run their fixed frame counts. NOT hashed.
    void enable_victory(bool on, bool short_game = true);
    bool victory_enabled() const { return victory_enabled_; }  // read-only; not hashed
    bool house_defeated(int h) const;   // sticky; false if h out of range / neutral
    int  winning_team() const;          // the sole surviving combatant team, or -1
    void step_victory();                // firewalled per-frame pass (public for the gate)

    // --- Engineer/Spy Capture phase [RE: capture.md] ------------------------
    // Ownership transfer (Approach A) [RE: capture.md §A3 CONFIRMED]: flip
    // buildings_[b_idx].house to new_house -> power/economy/buildability/sidebar
    // follow for free (all per-frame derived). Then the targeted fix-ups:
    //  (1) SW move: if the building grants_superweapon, transfer_superweapon
    //      old_house -> new_house;
    //  (2) AI base-node resync BOTH sides (the AI-1 mechanism): the losing AI
    //      house drops the node that pointed at this building; a capturing AI
    //      house adopts it into a matching unclaimed node;
    //  (3) primary-flag reset: the captured factory's `primary` flag is cleared
    //      (neither house treats a just-captured building as its established
    //      primary; the caller's set_primary re-elects one);
    //  (4) ProduceCashStartup: if captured FROM the neutral house, grant the
    //      new owner `startup` credits once.
    // Opens capture_any_. No-op returning false if b_idx is invalid, the
    // building is Dead, or new_house == the current owner. `startup` is the
    // captured building type's ProduceCashStartup (0 for a normal building).
    bool capture_building(int b_idx, int new_house, int startup);

    // Spy infiltration [RE: capture.md §B CONFIRMED]. Per building purpose, keyed
    // off `purpose` (resolved by the caller from the building type -- see
    // InfiltratePurpose): Refinery -> steal cash_steal_amount(victim credits,
    // steal_pct) into the spy house; PowerPlant -> arm blackout_frames_[victim]
    // = blackout (1000); Superweapon -> reset_superweapon(victim); Factory ->
    // set produced_veteran_[spy house] (Task 6); Radar/Lab -> inert (no shroud,
    // [RE-FALLBACK] documented). No-op returning false if the spy already owns
    // the building (self-spy) or b_idx is invalid/Dead. Opens capture_any_. The
    // caller consumes the spy on true. steal_pct = SpyMoneyStealPercent (0.5),
    // blackout = SpyPowerBlackout (1000).
    // Wave-5 (agent C) [RE: abilities-w5.md §1]: StolenAllied/Soviet/Third =
    // a [AI] BuildTech type (GATECH/NATECH/YATECH) by its AIBasePlanningSide.
    enum class InfiltratePurpose { None, Refinery, PowerPlant, Superweapon, Factory, RadarLab,
                                   StolenAllied, StolenSoviet, StolenThird };
    bool infiltrate_building(int b_idx, int spy_house, InfiltratePurpose purpose,
                             double steal_pct, int blackout);
    // Engineer repair [RE-GATE APPLIED: capture.md §A5]: heal building b_idx to
    // full HP (a damaged OWN building) and open capture_any_. No-op returning
    // false if b_idx is invalid, Dead, not owned by `by_house`, or already at
    // full HP. The caller consumes the engineer on true.
    bool repair_building(int b_idx, int by_house);
    // W2-SK-12: may an engineer capture this building (BuildingType Capturable=)?
    bool building_capturable(int b_idx) const {
        if (b_idx < 0 || b_idx >= (int)buildings_.size()) return false;
        const int t = buildings_[(std::size_t)b_idx].type;
        if (t < 0 || t >= (int)btypes().size()) return true;   // untyped test buildings
        // A bridge repair hut is "entered", not captured (world-depth.md §B4).
        return btypes()[(std::size_t)t].capturable || btypes()[(std::size_t)t].bridge_repair_hut;
    }

    // --- Projectile phase (P2) [RE: projectile.md] -------------------------
    // ALL inert by default. projectiles_enabled_ is opened ONLY by
    // enable_projectiles; step_projectiles' first line is the early return, and
    // World::step passes a launch sink to step_combat only when it is open. None
    // of this state is read by world_hash.
    bool projectiles_enabled_ = false;          // the firewall
    ProjectileRules proj_rules_;                // Gravity / BallisticScatter
    ProjRng proj_rng_;                          // dedicated scatter RNG (spec D3)
    std::vector<Projectile> projectiles_;       // in flight, launch order
    std::vector<ProjectileEvent> proj_events_;  // pending take_projectile_events
    double proj_accum_ = 0.0;                   // dt accumulator -> 15 Hz frames
    int proj_frame_ = 0;                        // logic-frame counter
    int proj_seq_ = 0;                          // W4: bullet ID source (homing wobble phase)
    // W4: fill a launched homing bullet's state (uid, ROT, keys, lock).
    void init_homing(Projectile& p, const ProjectileSpec& spec, double launch_speed);
    // W4: the homing target point this frame; false when the target is gone.
    bool homing_target(Projectile& p, Vec3i& out) const;
    // Firewalled World::step pass (pure insertion before step_combat): advance
    // every projectile one logic frame per accumulated 1/15 s.
    void step_projectiles(double dt);
    void step_projectiles_frame();
    // Turn one step_combat launch into a Projectile (muzzle via FLH, target
    // coords, compute_launch incl. scatter draws). Refused shots log an event.
    void launch_projectile(int attacker, int target_unit, int target_building);
    // Detonate `p` at its position: Explode snap, then the EXISTING damage layer
    // (apply_spread_damage / deliver_unit_hit / deliver_building_hit).
    void detonate_projectile(Projectile& p, int victim);

    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // The four [General] scalars + the three survivor divisors. The struct's
    // defaults ARE this install's real values, so a World that never calls
    // this already behaves like the original; main.cpp overrides from rulesmd.
    void set_repair_sell_rules(const RepairSellRules& r) { repair_rules_ = r; }
    const RepairSellRules& repair_sell_rules() const { return repair_rules_; }

    // BuildingClass::Repair(control) [RE: repair-sell.md §A4, vtbl+0x19C].
    // control: -1 toggle / 0 off / 1 on -- the classic Westwood convention.
    // Returns true iff the flag CHANGED. Refuses (false, no change) unless:
    //   * b_idx names an alive, non-Dead building,
    //   * b.house == by_house (the network REPAIR/SELL event is authoritative
    //     about ownership, §C1 -- you cannot repair someone else's building),
    //   * types_ covers b.type (set_economy must have run; repair needs Cost
    //     and Strength to price a step), and
    //   * turning ON additionally passes can_be_click_repaired (§A4).
    // Turning ON opens repair_sell_any_. Turning OFF is always allowed on a
    // live building (it is off-ramp 1/2/5 -- see step_repair_sell for 3/4).
    bool set_repair(int b_idx, int by_house, int control);
    bool toggle_repair(int b_idx, int by_house) { return set_repair(b_idx, by_house, -1); }
    // W5-E2 [RE: FUN_00452350 GoOffline / FUN_00452250 GoOnline, guarded by
    // type PowerDrain > 0 (+0xEE4); the EventClass POWERON(1) / POWEROFF(2)
    // cases (FUN_004C6BD0) and trigger actions TurnOn / TurnOff
    // (FUN_006DCE20) reach them]: `on` = power up. control -1 toggles.
    // False when the building cannot be powered down (no drain) or nothing
    // changed. NOT read by world_hash.
    bool set_building_power(int b_idx, int control);
    bool building_repairing(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].being_repaired;
    }
    // The GLOBAL repair cadence counter [RE: §A2]. It advances EVERY frame,
    // above the firewall, so `frame % interval == 0` is an ABSOLUTE phase --
    // all repairing buildings step together and a newly-armed one waits
    // 0..interval-1 frames, exactly like the original's `Frame % interval`.
    std::uint64_t repair_frame() const { return repair_frame_; }
    // The firewalled per-frame Repair & Sell pass. Public so a gate/test can
    // drive it directly; normally called from World::step.
    void step_repair_sell(double dt);

    // BuildingClass::Sell_Back(-1) [RE: repair-sell.md §B2/§B5, vtbl+0x1A0].
    // Starts mission 0x13 (Selling). Returns true iff the mission STARTED (or,
    // for a type with no buildup art, completed instantly -- §B5 item 6).
    // Refuses per can_be_sold PLUS ownership (b.house == by_house -- the SELL
    // event is authoritative, §C1) PLUS btypes() coverage. Calls
    // set_repair(b_idx, by_house, 0) FIRST -- selling always cancels repair
    // (§B2 entry, 0x00449B81). Opens repair_sell_any_.
    //
    // c4_applied is passed through to can_be_sold; the clone has no C4 mechanic
    // until roadmap P4, so every caller passes false today.
    bool begin_sell(int b_idx, int by_house, bool c4_applied = false);
    bool building_selling(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].selling;
    }
    // The crew UnitCombat spawned as a survivor for a given side [RE: §B3].
    // World is ASSET-FREE, so the CALLER resolves the infantry (E1 Allied /
    // E2 Soviet, [General] AlliedCrew/SovietCrew -- §D1) and registers it here.
    // side_ix is World::Side's underlying value. UNREGISTERED (the default for
    // every existing World) -> that side spawns ZERO survivors.
    // [RE-GATE APPLIED: repair-sell.md §D item 1] the 25%-chance special crew
    // type for building purpose-class 7 is CUT: Rules+0xF70 was never
    // string-anchored, and it is an RNG draw that would break gate determinism.
    void set_survivor_crew(int side_ix, const UnitCombat& c);
    // Add `amount` to house's world-side credit pool (the pool repair charges
    // and sell refunds land in). The interactive GameScene keeps the player's
    // wallet outside the World and drains this pool via take_income every
    // frame, so it deposits the wallet here before a step that may charge a
    // repair. No-op for an out-of-range house. No gate calls it.
    void deposit_credits(int house, double amount) {
        if (house >= 0 && house < house_count()) house_credits_[house] += amount;
    }
    // True iff any alive building of `house` has its wrench on.
    bool house_repairing(int house) const {
        for (const Building& b : buildings_)
            if (b.alive && b.house == house && b.being_repaired) return true;
        return depot_servicing(house);   // W2-SK-19: the depot charges the wallet too
    }
    // W2-SK-19 Service Depot [RE: rulesmd GADEPT/NADEPT UnitRepair=yes;
    // [General] URepairRate=.016 RepairStep=8 RepairPercent=15%]: the vehicle
    // standing at an own depot (the lowest-index one beside its foundation,
    // stopped) heals RepairStep HP on the shared repair phase for
    // repair_step_cost(Cost, Strength) credits, and an attached Terror Drone is
    // destroyed. [RE-FALLBACK: the pad is part of the foundation in the
    // original; here the docked cell is any cell touching the footprint.]
    int depot_client(int b_idx) const;
    bool depot_servicing(int house) const;
    void step_service_depots();
    void step_disguises();   // W2-SK-22 Mirage disguise-when-still

    // Consume a unit (the capture/repair gesture spends its engineer, and any
    // future "removed from map" consumer): tombstone it directly (no die anim --
    // it walked INTO the building) and release its occupancy cell, reusing the
    // combat.cpp death path (alive=false + occ_.release_all_of). No-op if idx is
    // out of range or the unit is already tombstoned. Interactive-only -- no
    // demo calls it, so every gate stays byte-identical.
    void consume_unit(int idx);
    // Move a house's whole superweapon grant old_house -> new_house (the SW
    // record + params); no-op if old_house has none. Reuses the CB-4 registry.
    void transfer_superweapon(int old_house, int new_house);
    // Reset a house's superweapon recharge to 0/not-ready [RE: capture.md §B4
    // CONFIRMED - spy SW-building infiltrate resets the timer]; reuses
    // Superweapon::on_fired(). No-op if the house has no granted SW.
    void reset_superweapon(int house);
    // Engineer capture/repair ORDER [RE: capture.md §A3/§A5]: record building
    // b_idx as unit eng_idx's capture_target and path it to a free cell adjacent
    // to the building's footprint (the transport/attack-building adjacency-
    // approach analogue -- a raw move onto the occupied foundation would cancel
    // as unreachable). It does NOT capture or consume here: step_capture performs
    // the capture_building/repair_building + consume_unit when the engineer
    // reaches adjacency (§A5: move-adjacent-then-act, like enter-transport).
    // Supersedes any engagement/enter order on the engineer. Opens capture_any_
    // (the step_capture firewall). No-op if eng_idx is not an alive, non-dying
    // unit or b_idx is not an alive, non-Dead building. Interactive-only -- no
    // demo issues it, so every gate stays byte-identical.
    void order_engineer_capture(int eng_idx, int b_idx);

    // Spy infiltrate ORDER [RE: capture.md §B]: the deferred sibling of
    // order_engineer_capture. Records building b_idx as unit spy_idx's
    // infiltrate_target and paths it to a free cell adjacent to the footprint;
    // the PRE-RESOLVED purpose + steal_pct/blackout are stored on the unit so
    // step_capture calls infiltrate_building (and consume_unit) on Chebyshev
    // arrival WITHOUT the roster (World is asset-free -- the caller resolved the
    // purpose from the building type at the gesture layer). It does NOT
    // infiltrate or consume here -- the effect fires only on arrival, so a spy
    // no longer infiltrates across the map on a single click. Supersedes any
    // engagement/enter order. Opens capture_any_. No-op if spy_idx is not an
    // alive, non-dying unit or b_idx is not an alive, non-Dead building.
    // Interactive-only -- no demo issues it, so every gate stays byte-identical.
    void order_spy_infiltrate(int spy_idx, int b_idx, InfiltratePurpose purpose,
                              double steal_pct, int blackout);

    // Register a captured/owned income building (an oil derrick) [RE: capture.md
    // §D1]. Opens capture_any_. amount/delay are the resolved [CAOILD]
    // ProduceCashAmount/ProduceCashDelay. Idempotent per building index; ignores
    // a non-positive amount/delay or a negative b_idx.
    void register_tech_income(int b_idx, int amount, int delay);

    // Set/clear a unit's disguise [RE: capture.md §C1/§C2]. set_disguise opens
    // capture_any_ (the step_tech_structures detection firewall). Both no-op on
    // an out-of-range index. clear_disguise leaves the type/house fields alone
    // (only the flag matters for render/detection).
    void set_disguise(int unit_idx, const std::string& as_type, int as_house);
    void clear_disguise(int unit_idx);
    // Read-only Capture state for the gate (§gate) and the power path.
    double house_credits(int house) const {
        return (house >= 0 && house < house_count()) ? house_credits_[house] : 0.0;
    }
    int blackout_frames(int house) const {
        return (house >= 0 && house < house_count()) ? blackout_frames_[house] : 0;
    }
    bool house_blacked_out(int house) const { return blackout_frames(house) > 0; }
    // Produced-unit veterancy flag [RE: capture.md §B5]. Set by a factory
    // infiltration; consumed by add_combat_unit/launch_unit in Task 6 (units
    // this house produces thereafter spawn at veteran_rank 1). Index 2 inert.
    bool produced_veteran(int house) const {
        return house >= 0 && house < house_count() && produced_veteran_[house];
    }
    // W2-SK-20: per factory category (BuildingTypeDef::factory_kind).
    bool produced_veteran_kind(int house, int kind) const {
        if (house < 0 || house >= house_count()) return false;
        const std::uint8_t m = produced_veteran_[house];
        return kind > 0 ? (m & (1u << kind)) != 0 : m != 0;
    }
    // W2-SK-20: the GameScene publishes the human wallet so a refinery spy
    // steals the real balance (the World only sees the per-frame income pool).
    void set_wallet_hint(int house, double credits) {
        if (house < 0) return;
        if ((int)wallet_hint_.size() <= house) wallet_hint_.resize((std::size_t)house + 1, 0.0);
        wallet_hint_[(std::size_t)house] = credits;
    }
    double wallet_hint(int house) const {
        return house >= 0 && house < (int)wallet_hint_.size() ? wallet_hint_[(std::size_t)house] : 0.0;
    }

    // --- Shroud/Fog phase [RE: shroud.md] ---------------------------------
    // Enable the per-house visibility layer: opens shroud_any_ and allocates
    // explored_[h]/visible_[h] (grid_w_*grid_h_, zero-filled) for h in 0..2.
    // Default OFF -> every existing gate leaves the grids EMPTY and step_visibility
    // is a strict no-op. Idempotent (re-enabling with grids already sized is a
    // no-op). enable(false) is not needed by any gate (left as a hard reset).
    void enable_shroud(bool on);
    bool shroud_enabled() const { return shroud_any_; }
    // W2-GA-03 FogOfWar [RE: presentation.md §4 FUN_00480100 draws FOG.SHP only
    // when Scen->Flags & 0x1000; rulesmd [General] FogOfWar=no, MP default no;
    // shroud.md §4]. OFF by default: an explored cell keeps showing everything
    // in it. When ON, objects need a currently-VISIBLE cell. NOT hashed.
    void set_fog_of_war(bool on) { fog_of_war_ = on; }
    bool fog_of_war() const { return fog_of_war_; }
    // The ONE object-visibility query (drawing, cursor, targeting, AI): can
    // house h see an object standing in cell (x,y)? Fog off -> the cell is
    // explored (mapped); fog on -> currently visible. Inert (true) with the
    // shroud off, like explored_at/visible_at.
    bool object_visible_at(int house, int x, int y) const {
        return fog_of_war_ ? visible_at(house, x, y) : explored_at(house, x, y);
    }

    // --- Projectile phase (P2) [RE: projectile.md] ---------------------------
    // Open the projectile firewall: from now on a shot whose Unit::w_proj
    // flies() launches a real projectile (ballistic arc / falling bomb) whose
    // damage lands at detonation, instead of the instant hit. Called ONLY by the
    // interactive game (GameScene) and --demo-projectile; no byte-identity gate
    // calls it, so every existing gate keeps the instant path bit-for-bit.
    void enable_projectiles(const ProjectileRules& rules);
    const ProjectileRules& projectile_rules() const { return proj_rules_; }   // W5-B R-057
    bool projectiles_enabled() const { return projectiles_enabled_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    int projectile_frame() const { return proj_frame_; }
    // Drain the projectile event log (launch/refused/detonate/removed).
    std::vector<ProjectileEvent> take_projectile_events();
    // Is cell (x,y) currently VISIBLE / ever EXPLORED to house h? EMPTY grid
    // (shroud off, or h out of range) -> true (all-visible / all-explored),
    // so every consult is inert with shroud off. Out-of-bounds cell -> false.
    bool visible_at(int house, int x, int y) const;
    bool explored_at(int house, int x, int y) const;
    // Stamp a near-circular sight disc for house h around (cx,cy) into
    // visible_[h] + explored_[h] (delegates to reveal_disc). No-op if the grid
    // is empty or h is out of range. The mutating entry for buildings/units.
    void reveal_source(int house, int cx, int cy, int sight);
    // The firewalled per-frame visibility pass (see world.cpp). Public so the
    // gate can drive it directly if needed; normally called from World::step.
    void step_visibility();
    // Re-shroud house h [RE: shroud.md §7B]: clear BOTH explored_[h] and
    // visible_[h] (fully re-shrouded -- the victim must re-scout). No-op if the
    // grid is empty (shroud off) or h out of range. shroud_any_ is NOT needed
    // (only meaningful once enabled). The classic spy radar-sabotage.
    void reshroud_house(int h);
    // Reveal the whole map for house h [RE: shroud.md §7A]: set explored_[h]
    // fully (and visible_[h] fully). Driven by owning a grants_map_reveal
    // building; also the "reveal map / no-shroud" start option. No-op if empty.
    void reveal_all_house(int h);

    // --- P4 Unit special abilities [RE: abilities.md] ----------------------
    // Bind `p` to every unit whose Unit::image == image (looked up lazily by
    // step_abilities, so spawn paths stay untouched). OPENS abilities_any_ --
    // the single firewall of the whole phase. No pre-P4 gate calls it.
    void register_ability_profile(const std::string& image, const AbilityProfile& p);
    // The recovered globals; the struct's defaults ARE this install's values.
    void set_ability_rules(const AbilityRules& r) { ability_rules_ = r; }
    const AbilityRules& ability_rules() const { return ability_rules_; }
    bool abilities_enabled() const { return abilities_any_; }
    // One step() = one logic frame; advances every step, above the firewall.
    std::uint64_t ability_frame() const { return ability_frame_; }
    const RadField& rad_field() const { return rad_; }
    // Building roster types with CanC4=no (rulesmd [type] CanC4=no).
    void set_no_c4_types(const std::vector<int>& types) { no_c4_types_ = types; }
    // Toggle a Deployer's deployed state (the desolator 'D'). Returns true iff
    // the unit is a live deployer (state flipped).
    bool order_deploy(int idx);
    // The per-frame ability pass (appended to World::step). Public so tests
    // can drive it; `if (!abilities_any_) return;` after the frame tick.
    void step_abilities(double dt);
    // Witness events for the gate / audio (drained like take_cargo_events).
    struct AbilityEvent {
        enum class Kind {
            Infect, ParasiteTick, DroneExit, DroneLost, Teleport, Unfreeze, TemporalAttach,
            Erase, BombPlant, BombBlast, C4Plant, C4Blast, Suicide, DeathWeapon, Eruption,
            RadDamage, IfvSwap, Deploy
        };
        Kind kind = Kind::Infect;
        int a = -1;          // the acting unit (or victim for damage events)
        int b = -1;          // the other party (unit, or building for *_bld)
        int x = 0, y = 0;    // event-specific payload (damage, cell, frames)
        bool building = false;  // b is a building index
    };
    std::vector<AbilityEvent> take_ability_events();

    // Test seam: directly seed a house's credits without running the full
    // harvest/unload pipeline (the cash-steal victim in the infiltrate tests).
    // Index 2 (neutral) inert. Interactive/demo code never calls it.
    void grant_credits_for_test(int house, double amount) {
        if (house >= 0 && house < house_count()) house_credits_[house] = amount;
    }

    // --- P5 Veterancy + Crates [RE: docs/re/veterancy.md] --------------------
    // BYTE-IDENTITY FIREWALL: nothing below runs unless enable_veterancy (and,
    // for crates, enable_crates) was called. No existing gate calls either --
    // only GameScene (the interactive game) and --demo-veterancy do -- so
    // step_crates/step_veterancy cost every existing gate one counter
    // increment and two branches, and none of this state is read by
    // world_hash or any existing *_hash.
    //
    // Open the veterancy system: the rank/kill/self-heal pass, the combat
    // receive-damage hook on every unit + building, and the vet-aware spawn.
    // `building_costs[type]` = Cost of building roster type (kill value of a
    // destroyed building, V3); a type outside it is worth 0.
    void enable_veterancy(const VeterancyRules& r, std::vector<int> building_costs = {});
    bool veterancy_enabled() const { return vet_any_; }
    const VeterancyRules& veterancy_rules() const { return vet_rules_; }
    // Rank-change events (V12), drained by the caller (EVA/sound hook, witness).
    struct VetEvent { int unit = -1; int old_rank = 0; int new_rank = 0; };
    std::vector<VetEvent> take_vet_events();
    // Recompute unit `idx`'s effective weapon/ROF/speed/sight from its base or
    // elite weapon, rank abilities and crate multipliers (V5/V7/V8/V9/V10).
    // No-op unless veterancy is enabled. Public: the crate effects + tests.
    void refresh_vet_stats(int idx);
    // Frames stepped so far (the global Frame for the self-heal phase, V11).
    std::uint64_t vet_frame() const { return vet_frame_; }

    // Crates (C1-C8). Requires veterancy (the multipliers live on its path);
    // returns false (and opens nothing) when veterancy is off.
    struct CrateConfig {
        CrateRules rules;
        std::uint32_t seed = 0;               // ScenarioClass::Random seed
        int players = 1;                      // session human count (C2)
        std::vector<UnitCombat> unit_pool;    // CrateGoodie=yes vehicles (Unit crate)
        bool place_initial = true;            // drop the C2 initial crates now
    };
    // One Crates[256] slot. x < 0 = empty. powerup -1 = random (the overlay
    // OverlayData >= 0x13 case); >= 0 = a typed crate (PlaceCrate(cell,type)).
    struct Crate {
        int x = -1, y = -1;
        int powerup = -1;
        std::uint64_t start = 0;              // vet_frame() at placement
        int lifetime = 0;                     // frames (C4)
    };
    struct CrateEvent {
        int picked = -1;      // powerup drawn/typed
        int applied = -1;     // after the conversion rules (§3.3)
        int house = -1, unit = -1, x = -1, y = -1;
        int amount = 0;       // Money credits / spawned unit index
        int affected = 0;     // area effects: objects changed
    };
    bool enable_crates(const CrateConfig& cfg);
    bool crates_enabled() const { return crates_any_; }
    // PlaceCrate(cell, type) (FUN_0056B9F0): nearest valid cell to (x,y), first
    // free slot, lifetime drawn. powerup -1 = random. Returns success.
    bool place_crate(int x, int y, int powerup = -1);
    // PlaceRandomCrate (FUN_0056B870): up to 1000 random tries.
    bool place_random_crate();
    const std::vector<Crate>& crates() const { return crates_; }
    std::vector<CrateEvent> take_crate_events();

    // --- Gap fix C: base defenses + combat events [RE: docs/re/defenses.md] ---
    // One shot fired this step (a unit or a base defense) for the renderer /
    // audio owner (muzzle flash, report sound, tracer). Drained per frame.
    struct FireEvent {
        int unit = -1;             // firing unit, or -1
        int building = -1;         // firing base defense, or -1
        int target_unit = -1;
        int target_building = -1;
        int slot = 0;              // weapon slot fired (0 primary / 1 secondary)
        std::string weapon;        // weapon id when known (defenses), else ""
    };
    std::vector<FireEvent> take_fire_events();
    // W3-B: the not-yet-taken events (the presentation's per-tick observer).
    const std::vector<FireEvent>& pending_fire_events() const { return fire_events_; }
    // Stop / Guard / Scatter (unit_orders.cpp). B binds S / G / X to the
    // *_selected forms (house-0 selection); the per-unit forms serve AI/tests.
    void order_stop(int idx);
    void order_guard(int idx);
    void order_scatter(int idx);
    void order_stop_selected();
    void order_guard_selected();
    void order_scatter_selected();
    // Powered=yes defenses work only while the house's power output covers its
    // drain (BuildingClass power check FUN_004555c0, ratio < 1.0 -> offline).
    bool defense_power_ok(int house) const;
    // The per-frame defense pass (public for tests; called from step()).
    void step_building_combat(double dt);

    // Advance all units by dt seconds: step_units (collision-aware movement)
    // -> step_combat (targeting/firing/deaths, returns pursuit/return-to-
    // anchor MoveRequests) -> path each request via order_unit_move (skipping
    // any request for a unit that died/started dying this same step) -> patch
    // each still-alive unit's terrain level from the map for rendering height.
    void step(double dt);

    // Per-unit body shared by order_move_selected (per selected unit, given
    // its formation-assigned cell) and step's MoveRequest execution (per
    // pursuing/returning unit, given step_combat's requested cell): paths
    // unit `idx` directly to (gx, gy) -- no formation, since a pursuit/
    // return-to-anchor request already names one specific goal cell --
    // releasing/re-asserting its occupancy claim first. Unreachable/self-goal
    // cancels cleanly (fx/fy snap to the current cell, mirroring the
    // single-unit Finding-1 cancel branch); otherwise fx/fy are left alone
    // (no backward snap) and the unit starts walking. No-op if idx is out of
    // range.
    // CB-2 [RE: aircraft.md §1/§6]: for an is_aircraft unit this instead sets a
    // FLIGHT GOAL and initiates takeoff/re-launch (no A*, no occupancy). Public
    // as the direct single-unit order seam the flight World tests drive (a
    // compile-time access change only -- no gate reads it).
    void order_unit_move(int idx, int gx, int gy);

    // --- P10 Campaign [RE: campaign.md §7] -- per-unit entry points for the
    // map-team scripts / trigger actions of ANY house. ADDITIVE forwarding
    // wrappers over the existing private bodies (AI-2 / P4 paths); nothing
    // that exists calls them, so every gate is unaffected.
    void order_unit_attack(int unit_idx, int target_idx) { ai_order_attack(unit_idx, target_idx); }
    void order_unit_attack_building(int unit_idx, int b_idx) {
        ai_order_attack_building(unit_idx, b_idx);
    }
    void campaign_kill_unit(int idx) { ability_kill_unit(idx); }
    void campaign_erase_unit(int idx) { ability_erase_unit(idx); }
    void campaign_damage_unit(int idx, int dmg) { ability_damage_unit(idx, dmg); }
    void campaign_damage_building(int b_idx, int dmg) { ability_damage_building(b_idx, dmg); }

    // --- gF MCV deploy / undeploy [RE: docs/re/mcv-deploy.md] ---------------
    // A unit whose type has DeploysInto= (AMCV/SMCV) unfolds into that building.
    // UnitClass deploy mission FUN_007383e0 / try-to-deploy FUN_00737d50:
    // foundation origin = MCV cell + AdjacentCell[NW] (-1,-1) when the
    // foundation is wider or taller than 2 (else the MCV cell itself); legality
    // = every foundation cell clear (BuildingType vtbl+0xA8 -> FUN_007154F0,
    // NO adjacency); the unit turns to the type's DeployFacing (default 0x80)
    // and only then becomes the building (mission Construction = buildup anim,
    // health ratio carried, min 1). If the spot is illegal at order time the
    // unit searches the recovered 36-offset ring for a legal spot and drives
    // there; a deploy that fails on the spot raises EVA_CannotDeployHere for a
    // human house. Firewalled by deploy_any_ (no gate issues the order).
    struct DeployEvent {
        enum class Kind { Deployed, CannotDeploy, Undeployed };
        Kind kind = Kind::Deployed;
        int unit = -1;       // the MCV (consumed for Deployed, new for Undeployed)
        int building = -1;   // the new building (Deployed) / the packed one (Undeployed)
        int house = -1;
    };
    // Order unit `idx` to deploy (any house: player 'D', AI, campaign script 9).
    // false = not a deployer / dead / no legal spot anywhere in the ring.
    bool order_deploy_mcv(int idx);
    bool is_mcv(int idx) const;
    // Would the unit's DeploysInto building be legal with the MCV at (x, y)?
    // (the MCV itself does not block its own spot). Out: foundation origin.
    bool can_deploy_at(int idx, int x, int y, int* ox = nullptr, int* oy = nullptr) const;
    // Buildup seconds per roster type (art Buildup timing) so a deployed
    // building plays its unfold animation; unset -> instant.
    void set_buildup_totals(std::vector<double> t) { buildup_totals_ = std::move(t); }
    // UndeploysInto [RE: repair-sell.md §B6]: the unit a ConstructionYard packs
    // back into when sold by a human in a skirmish session with MCVRedeploys.
    void set_undeploy_unit(int btype, const UnitCombat& c);
    void set_mcv_redeploys(bool on) { mcv_redeploys_ = on; }
    bool mcv_redeploys() const { return mcv_redeploys_; }
    void set_skirmish_session(bool on) { skirmish_session_ = on; }
    std::vector<DeployEvent> take_deploy_events();
    void step_deploy(double dt);
    // gF G-75: trigger action 101 ReshroudMapAtWaypoint (TAction FUN_006E0FE0
    // -> MapClass reshroud of radius RevealTriggerRadius around the waypoint
    // for the player; a no-op while the house has the whole map revealed).
    void reshroud_area(int house, int cx, int cy, int radius);
    // --- G16 world depth: garrisons (G-48) + bridges (G-49) [agent G,
    // world_depth.cpp; RE: docs/re/world-depth.md]. Everything is inert until
    // a caller opens it (set_depth_rules / set_bridges / an order), so every
    // byte-identity gate is unaffected.
    struct DepthRules {
        double occupy_damage_mult = 1.0;   // [CombatDamage] OccupyDamageMultiplier (Rules+0xF40)
        double occupy_rof_mult = 1.0;      // OccupyROFMultiplier (+0xF44)
        int occupy_weapon_range = 0;       // OccupyWeaponRange (+0xF48), cells
        int bridge_strength = 1500;        // BridgeStrength (+0x1740)
        bool bridge_destruction = true;    // [CombatDamage] DestroyableBridges && BridgeDestruction
        bool armed = false;                // set by the real game only (gates never)
    };
    void set_depth_rules(const DepthRules& r) { depth_rules_ = r; }
    const DepthRules& depth_rules() const { return depth_rules_; }
    // Garrisons.
    bool can_occupy(int unit_idx, int b_idx) const;          // BuildingClass::CanBeOccupiedBy
    void order_occupy(int unit_idx, int b_idx);              // walk adjacent, then enter
    int order_occupy_selected(int b_idx);                    // house-0 selection; returns #ordered
    void unload_occupants(int b_idx);                        // BuildingClass::UnloadOccupants
    // W4-SQ-02: a campaign reinforcement / garrison team member enters a
    // building at once (FUN_0065d320 all-infantry branch; the script 46
    // arrival). campaign_can_occupy: the type CanBeOccupied, live, idle,
    // not full (no unit test).
    bool campaign_can_occupy(int b_idx) const;
    // W4-SQ-02 [RE: FUN_00457CE0 tests the owner's HouseType MultiplayPassive
    // (+0x1A6)]: a campaign house whose country is passive (the civilian
    // houses) lends its occupiable buildings to anyone's Occupier.
    void set_passive_house(int h, bool on) {
        if (h < 0) return;
        if ((int)passive_houses_.size() <= h) passive_houses_.resize((std::size_t)h + 1, 0);
        passive_houses_[(std::size_t)h] = on ? 1 : 0;
    }
    bool passive_house(int h) const {
        return h >= 0 && h < (int)passive_houses_.size() && passive_houses_[(std::size_t)h] != 0;
    }
    bool campaign_occupy_now(int unit_idx, int b_idx);
    bool garrison_firing(int b_idx) const;                   // CanOccupyFire && occupants
    double garrison_range(int b_idx) const;                  // cells
    struct GarrisonEvent { enum Kind { Entered, Garrisoned, Abandoned, Unloaded } kind; int building; int house; };
    std::vector<GarrisonEvent> take_garrison_events();
    // W2-CA-01 [RE: FUN_00519449 raises event 1 EnteredBy on the TARGET
    // building's tag at every "infantry arrived into a building" site
    // (capture, repair, infiltrate, occupy, bridge hut), then 48 on the
    // consumed infantry]. Recorded only once a consumer opted in (the
    // campaign runtime), so no other mode ever accumulates them.
    struct EnteredEvent {
        enum Kind { Capture, Repair, Infiltrate, Occupy, BridgeHut } kind;
        int unit; int building; int house; bool consumed;
    };
    void set_track_entered(bool on) { track_entered_ = on; }
    std::vector<EnteredEvent> take_entered_events() {
        std::vector<EnteredEvent> out;
        out.swap(entered_events_);
        return out;
    }
    // Bridges. `overlay` = per-cell RUNTIME overlay index (-1 none), `tile_land`
    // = the cells' tile land types BEFORE overlay overrides (y*grid_w+x).
    void set_bridges(std::vector<int> overlay, std::vector<ra2assets::LandType> tile_land);
    bool bridges_enabled() const { return !bridge_ovl_.empty(); }
    int bridge_overlay_at(int x, int y) const;               // current runtime index, -1 none
    int bridge_explosion(int cx, int cy, int damage, bool wall_warhead);   // FUN_00489190 bridge part
    int repair_bridge_near(int b_idx);                       // engineer entered a BridgeRepairHut
    // W4-SQ-10 high bridges [RE: FUN_00586BB0 dispatch -> FUN_005766D0
    // (BRIDGE1/2 concrete) / FUN_00570FC0 (BRIDGEB1/B2 wood)]. `cells`: per
    // cell 0 none, 1 deck over water/cliff (passable only while intact), 2
    // deck over land (always passable).
    void set_high_bridges(std::vector<std::uint8_t> cells);
    bool high_bridge_cell(int x, int y) const;               // any high-bridge cell (the CellFlags stand-in)
    bool high_bridge_destroyed(int x, int y) const;
    int break_high_bridge(int x, int y);                     // the whole span through (x,y) (or a neighbour)
    int repair_high_bridge_near(int b_idx);                  // hut repair (FUN_0056FB80)
    // Trigger Apply100Damage bridge call (FUN_006DFA00 -> FUN_00586BB0, up to
    // 4 tries): breaks the low section / high span at (x,y) with no roll.
    int force_bridge_break(int x, int y);
    std::uint32_t bridge_version() const { return bridge_version_; }
    long long garrison_shots() const { return garrison_shots_; }
    int bridge_cells_destroyed() const;
    // Force-fire at a ground cell (Ctrl+click, G-49 bridge demolition).
    void order_attack_cell(int unit_idx, int cx, int cy);
    int order_attack_cell_selected(int cx, int cy);
    // The per-frame G16 pass (called from step()).
    void step_depth(double dt);

    // ===== Wave-5 unit and building abilities (world_w5.cpp, agent C) =====
    // [RE: docs/re/abilities-w5.md]. All state lives in w5_ / Unit::w5 /
    // Building::w5 / BuildingTypeDef::w5; inert until a roster fills the type
    // data or a spy steals tech (w5_.any firewall).
    // Stolen tech (HouseClass Side0/1/2TechInfiltrated): kStolen* bits.
    std::uint8_t stolen_tech(int house) const {
        return house >= 0 && house < (int)w5_.stolen_tech.size() ? w5_.stolen_tech[(std::size_t)house] : 0;
    }
    void grant_stolen_tech(int house, std::uint8_t bits);
    // HouseClass::HasAllStolenTech: a RequiresStolen* type keeps TechLevel 0
    // (never buildable) until `house` holds every bit it needs.
    void apply_stolen_tech_gate(std::vector<UnitTypeDef>& defs, int house) const;
    // The per-frame wave-5 pass (called from step()).
    void step_w5(double dt);
    // Arm the real-play wave-5 mechanics (recovered globals + per-image unit
    // profiles). Never called by a gate.
    void arm_w5(const RulesW5& rules, const std::map<std::string, UnitProfileW5>& profiles);
    bool w5_armed() const { return w5_.armed; }
    const WorldW5& w5_state() const { return w5_; }
    // IsPowerOnline with the Tesla overpower exception.
    bool w5_power_online(int b_idx) const;
    // Engineer bomb defusal (R-046): walk next to the bombed unit/building and
    // disarm it (DefuseKit, BombDisarm warhead). false when nothing to defuse.
    bool order_defuse(int eng, int unit_target, int b_target);
    // Cells an enemy gap currently keeps black for house h (probe witness).
    int w5_gap_hidden(int h) const;

private:
    // --- P5 Veterancy + Crates [RE: veterancy.md] (all inert until enabled) ---
    bool vet_any_ = false;                    // the veterancy firewall
    VeterancyRules vet_rules_;
    std::vector<int> vet_building_costs_;     // roster type -> Cost (V3)
    std::uint64_t vet_frame_ = 0;             // global Frame (advances every step)
    std::vector<VetEvent> vet_events_;
    void init_unit_vet(Unit& u);              // arm recv + rank memory + refresh
    void credit_kill(int killer, int victim_house, int victim_cost, float victim_vet,
                     bool dont_score);
    void step_veterancy();                    // firewalled pass after step_combat
    bool crates_any_ = false;                 // the crate firewall
    CrateRules crate_rules_;
    GameRandom crate_rng_;
    std::vector<UnitCombat> crate_pool_;
    std::vector<Crate> crates_;               // Crates[256]
    std::vector<CrateEvent> crate_events_;
    bool cell_is_water(int x, int y) const;
    bool crate_nearby_cell(int x, int y, bool water, int& ox, int& oy) const;
    bool crate_cell_valid(int x, int y) const;
    bool crate_slot_place(int slot, int x, int y, int powerup);
    void collect_crate(int slot, int unit_idx);
    void step_crates();                       // firewalled pass after step_units

    // --- AI-1 base-planning phase [RE: ai-base.md] ---
    struct HouseAiRec {
        bool is_ai = false;
        AiHouseConfig cfg;
        AiBaseState state;
        ProductionQueues queues;  // ONLY the Structure/Defense slots are used
        double credits = 0.0;
        AiTeamState team_state;  // AI-2 [RE: ai-team.md]
        AiTriggerState trigger_state;  // AI-3 [RE: ai-trigger.md]
        int enemy_house = -1;  // W2-SK-08: the ONE current enemy (house+0x5600), v9
    };
    std::vector<HouseAiRec> ai_houses_;  // index = house id (grown on demand)
    bool ai_any_ = false;                // the step() firewall bool
    void step_ai_houses(double dt);
    void ai_pick_enemy_house(int house, HouseAiRec& rec);   // W2-SK-08
    bool ai_house_alive(int house) const;                   // W2-SK-08
public:
    int ai_enemy_house(int house) const {                   // W2-SK-08 (probe)
        return ai_enabled(house) ? ai_houses_[house].enemy_house : -1;
    }
private:
    void ai_try_fire_superweapon(int house, HouseAiRec& rec);   // [G-66]
    // W6-03-05 [RE: FUN_0050ca10]: the best-valued object of `enemy`; scores.
    bool ai_sw_pick_target(int house, HouseAiRec& rec, int enemy, int quarry, Cellxy& out);
    int ai_sw_score_building(const HouseAiRec& rec, const Building& b) const;
    int ai_sw_score_unit(const HouseAiRec& rec, const Unit& u) const;
    bool ai_base_centre(int house, int& x, int& y) const;   // house+0x5490
    BaseView make_ai_base_view(int house, const HouseAiRec& rec) const;
    bool ai_place_ready(int house, HouseAiRec& rec, QueueCat cat);
    // W2-AN-16: an AI Naval Yard spot -- every foundation cell water, free, no
    // building, within AINavalYardAdjacency of the house's Construction Yard
    // (world_an.cpp); and the full scan used when the base-ring scan finds none.
    bool ai_naval_spot_ok(int house, const HouseAiRec& rec, const BuildingTypeDef& t, int cx,
                          int cy) const;
    Cellxy ai_naval_spot_scan(int house, const HouseAiRec& rec, const BuildingTypeDef& t) const;

    // --- AI-2 offensive-teams phase [RE: ai-team.md] ---
    void step_ai_teams(double dt);
    int ai_unit_type_of(const HouseAiRec& rec, const Unit& u) const;
    std::vector<IdleUnitView> ai_idle_units(int house, const HouseAiRec& rec) const;
    void ai_fill_team_view(int house, const HouseAiRec& rec, const TeamInstance& team,
                           TeamView& v) const;
    bool ai_quarry_exists(int house, const HouseAiRec& rec, int quarry) const;
    // Actions 46/47/58: the building of rulesmd [BuildingTypes] index arg&0xffff
    // picked by selector arg>>16, among enemy (or own) buildings; -1 none [G-61].
    int ai_find_typed_building(int house, const HouseAiRec& rec, int arg, bool enemy,
                               int nx, int ny) const;
    // Pickers score by distance^2 x value x seeded-RNG jitter (ai_rand over the
    // passed rng) and keep the nearest-reachable [RE-GATE APPLIED: ai-team.md §5].
    int ai_pick_quarry_unit(int house, int quarry, int nx, int ny, std::uint32_t& rng,
                            bool can_aa = false) const;
    int ai_pick_quarry_building(int house, const HouseAiRec& rec, int quarry, int nx, int ny,
                                std::uint32_t& rng) const;
    // Shared mutation tail extracted from order_attack/order_attack_building so
    // both the player selection-loops and the AI per-unit entry points commit a
    // target identically [RE-GATE APPLIED: ai-team.md §5 — de-duplication].
    void commit_attack_target(int unit_idx, int target_unit, int target_building);
    void ai_order_attack(int unit_idx, int target_idx);
    void ai_order_attack_building(int unit_idx, int b_idx);
    void ai_execute_team_command(int house, HouseAiRec& rec, TeamInstance& team,
                                 const TeamCommand& c);
    bool ai_launch_unit_ready(int house, HouseAiRec& rec, QueueCat cat);
    // W2-SK-06: how many more of unit type `ut` the team's TaskForce owes.
    static int ai_team_need(const TeamInstance& t, const AimdData& aimd, int ut);
    int ai_team_transport(const TeamInstance& t) const;        // W2-SK-09
    InfiltratePurpose ai_infiltrate_purpose(int b_idx) const;  // W2-SK-09

    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    void ai_fill_trigger_view(int house, const HouseAiRec& rec, TriggerView& v) const;

    // --- CB-4 Superweapons [RE: superweapon.md] ---
    // Per-house SW record: the pure lifecycle + the resolved detonation params.
    // Mirrors HouseAiRec's shape but INDEPENDENT of ai_houses_ (spec §2). All
    // defaults INERT (has_sw=false). eff_verses is the [NUKE] 11-slot armor
    // array as int percents (assigned straight into the transient detonator's
    // Unit::w_verses in fire_superweapon -- damage_vs takes std::vector<int>).
    struct HouseSuperweapon {
        bool has_sw = false;
        Superweapon sw;
        int eff_damage = 0;                 // [NukePayload] Damage=600
        double eff_cell_spread = 0.0;       // [NUKE] CellSpread=10
        double eff_percent_at_max = 1.0;    // [NUKE] PercentAtMax=.02
        std::array<int, 11> eff_verses{};   // [NUKE] Verses (armor percents)
        bool is_powered = false;            // SW type IsPowered= (gap owner A, G-29)
    };
    // index = house id (grown on demand); W2-AN-04: one record per granted SW type.
    std::vector<std::vector<HouseSuperweapon>> sw_houses_;
    HouseSuperweapon* sw_rec(int house, SuperweaponType type);
    const HouseSuperweapon* sw_rec(int house, SuperweaponType type) const;
    // --- W2-AN state (world_an.cpp / world_sw.cpp) ---
    struct AnWorld {
        bool any = false;                        // step_air_naval firewall
        SwParams sw;
        std::vector<SwStrike> strikes;
        std::vector<SwEvent> events;
        int event_seq = 0;
        std::map<std::string, UnitCombat> types; // spawned / paradrop types
        std::vector<std::pair<int, int>> dock_offsets;   // GAAIRC pads (leptons x,y)
        std::uint32_t rng = 0x2545F491u;
        int frame = 0;
        std::vector<int> sw_btype;               // building roster idx -> SuperweaponType (int)
        std::vector<int> repair_btype;           // building roster idx -> UnitRepair=yes (1/0)
        std::vector<int> airfield_btype;         // building roster idx -> Helipad=yes airfield
        bool nuke_set = false;                   // the last Nuke grant's blast params
        int nuke_damage = 600;
        double nuke_spread = 10.0;
        double nuke_pmax = 0.02;
        std::array<int, 11> nuke_verses{};
    };
    AnWorld an_;
    void an_event(int kind, int house, SuperweaponType type, int x, int y, int x2 = -1, int y2 = -1);
    std::uint32_t an_rand();
    void step_air_naval(double dt);
    void an_step_naval(double dt);
    void an_step_spawners(double dt);
    void an_step_fixed_wing(double dt);
    void an_step_deaths(double dt);
    void an_step_strikes(double dt);
    void an_nuke_detonate(const SwStrike& s);
    void an_storm_bolt(SwStrike& s, int x, int y);
    void an_paradrop(int house, SuperweaponType type, int cx, int cy);
    bool an_drop_passenger(int plane);   // W4-SQ-06: one carried paratrooper out
    void an_step_parachutes(double dt);  // W4-SQ-06: falling paratroopers
    void an_chrono(int house, int sx, int sy, int dx, int dy);
    void an_iron_curtain(int house, int cx, int cy);
    int an_spawn(const std::string& id, int house, int x, int y);
    void an_damage_unit(int idx, int dmg, int attacker);
    void an_damage_building(int b, int dmg);
    void an_area_damage(int house, double cx, double cy, int damage, double spread, double pmax,
                        const std::array<int, 11>& verses, bool hit_air, bool spare_house,
                        double z = 0.0);   // W4: explosion height (0 = the 2-D test)
    // W4 RocketLocomotion (world_an.cpp): target point, explosion.
    bool rocket_target_point(int tu, int tb, double& x, double& y, double& z) const;
    void rocket_explode(int i, int end);
    bool sw_any_ = false;                      // the step() firewall bool
    void step_superweapon(double dt);          // firewalled World::step pass (§3)

    // --- Engineer/Spy Capture phase [RE: capture.md] ---
    bool capture_any_ = false;   // the step() firewall bool (opened by any
                                 // capture/infiltrate/neutral-spawn/disguise)
    // Resync AI base-node ownership after a capture [RE: capture.md §A3 -
    // reuses the AI-1 standing-building<->node sync]. For the LOSING house (if
    // AI): drop the node whose `building == b_idx` (n.building=-1, n.dead=true)
    // -- a captured building is still ALIVE so node-death detection never fires.
    // For the GAINING house (if AI): claim b_idx into the FIRST unclaimed plan
    // node of the same type (the standing-building sync rule). The player
    // (house 0) is never AI -> no node work when the player captures.
    void ai_resync_captured_building(int b_idx, int old_house, int new_house);
    // Engineer capture-arrival pass [RE: capture.md §A5], called from step()
    // beside step_cargo (a pure insertion). Early-returns unless capture_any_
    // (the firewall: false until the first capture/repair/order). For each unit
    // with a live capture_target that has reached Chebyshev adjacency to its
    // target building's footprint, performs capture_building (enemy/neutral) or
    // repair_building (own damaged), consumes the engineer on a successful act,
    // and clears the intent; a vanished/dead target clears the intent cleanly.
    void step_capture(double dt);

    // Tech-structure periodic income [RE: capture.md §D1 CONFIRMED - CAOILD
    // ProduceCashAmount=20 every ProduceCashDelay=100 frames]. One record per
    // captured income building; the income accrues to whoever CURRENTLY owns
    // the building (buildings_[b].house), so capturing an oil derrick transfers
    // its future income for free (per-frame derived, Approach A).
    struct TechIncome { int building = -1; int amount = 0; int delay = 0; int frames = 0; };
    std::vector<TechIncome> tech_income_;
    // Power blackout per house [RE: capture.md §B2 CONFIRMED - SpyPowerBlackout
    // =1000 frames]. >0 -> that house's power OUTPUT is forced to 0 (see
    // apply_blackout). Index 2 inert. Decays one per logic frame in
    // step_tech_structures.
    // T3 [RE: multihouse.md §D2]: [3] -> std::vector sized house_count() (default
    // 3 -> indices 0/1/2 identical). NOT hashed. Sized in ensure_houses_default /
    // grow_house_arrays.
    std::vector<int> blackout_frames_;
    // Produced-unit veterancy flag [RE: capture.md §B5]. Set by a factory
    // infiltration; consumed by add_combat_unit/launch_unit in Task 6 (units
    // this house produces thereafter spawn at veteran_rank 1). Index 2 inert.
    // Defaults false (INERT): no existing demo infiltrates a factory, so every
    // gate is byte-identical.
    // T3 [RE: multihouse.md §D2]: bool[3] -> std::vector<uint8_t> sized
    // house_count(). NOT hashed. Sized in ensure_houses_default/grow_house_arrays.
    std::vector<std::uint8_t> produced_veteran_;   // W2-SK-20: bit (1 << factory_kind)
    std::vector<double> wallet_hint_;              // W2-SK-20 (v13)
    // The single firewalled Capture per-frame pass: tech-structure income +
    // blackout decay (+ disguise detection, added in Task 7). First line is
    // `if (!capture_any_) return;` -> a strict no-op in every existing demo.
    void step_tech_structures(double dt);

    // --- Shroud/Fog phase [RE: shroud.md §1/D1] ---
    // Two independent per-house grids, sized grid_w_*grid_h_, index y*grid_w_+x
    // (same as passable_). EMPTY (size 0) until enable_shroud allocates them ->
    // accessors return all-visible -> inert with shroud off (the water_passable_
    // precedent). explored_ is sticky (fog no-regrow §4): set on reveal, cleared
    // ONLY by reshroud_house (Task 7). visible_ is cleared+recomputed each tick.
    // House index 0=player, 1=enemy, 2=neutral (kNeutralHouse, left inert). NONE
    // read by world_hash.
    // T3 [RE: multihouse.md §D2]: outer [3] -> std::vector sized house_count()
    // (each inner grid still grid_w_*grid_h_). NOT hashed. Sized in
    // ensure_houses_default/grow_house_arrays (inner grids stay EMPTY until
    // enable_shroud).
    std::vector<std::vector<std::uint8_t>> explored_;
    std::vector<std::vector<std::uint8_t>> visible_;
    bool fog_of_war_ = false;   // W2-GA-03: FogOfWar scenario flag (default off)
    bool shroud_any_ = false;   // the step_visibility firewall (opened by
                                // enable_shroud); false in every existing gate.
    // SpySat/GPS latch per house [RE: shroud.md §7A]. Recomputed each tick from
    // the house's alive grants_map_reveal buildings; when set, step_visibility
    // keeps the whole map revealed and reshroud_house(h) is SUPPRESSED for h
    // (the §7B "unless SpySat-protected" guard). Index 2 inert. Not hashed.
    // T3 [RE: multihouse.md §D2]: bool[3] latch -> std::vector<uint8_t> sized
    // house_count(). NOT hashed. Sized in ensure_houses_default/grow_house_arrays.
    std::vector<std::uint8_t> map_reveal_;

    // --- Multi-house / Alliances phase [RE: multihouse.md §1/D1] ---
    // The single source of truth for every house. Populated with 3 defaults at
    // construction (0=Allied/team0/comb, 1=Soviet/team1/comb, 2=Neutral/team2/
    // non-comb) so house_count()==3 and today's world is reproduced. NOT hashed.
    std::vector<HouseRec> houses_;
    std::vector<std::uint8_t> ally_pairs_;   // W2-CA-10: n*n, empty = team model
    bool houses_configured_ = false;   // the side/color/enmity/AllyReveal firewall;
                                       // false in every existing gate.

    // Victory / defeat state [RE: multihouse.md §5]. house_defeated_ sized
    // house_count(); sticky. victory_enabled_ (the step_victory firewall) is
    // false in every existing gate -> step_victory early-returns. NONE hashed.
    bool victory_enabled_ = false;
    bool short_game_      = true;
    std::vector<std::uint8_t> house_defeated_;
    int  winning_team_    = -1;
    // Populate houses_ with the 3 defaults if empty (idempotent). Called from the
    // constructor; harmless if invoked again.
    void ensure_houses_default();
    // T3: resize the six per-house vectors + house_defeated_ to n. Stub in Task 2
    // (declared here so set_house compiles).
    void grow_house_arrays(int n);

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Boarding / carry / eject / transport-death, called between step_units
    // and step_turrets (a pure insertion -- spec §8). Early-returns unless
    // cargo_any_ (the firewall: false until the first enter/unload order).
    void step_cargo(double dt);
    bool cargo_any_ = false;
    std::vector<int> player_orders_;   // W4-SQ: transient, see mark_player_order
    std::vector<std::uint8_t> passive_houses_;   // W4-SQ-02 (save v16)
    std::vector<std::vector<std::string>> ai_demand_ids_;   // W4-SQ-12 (save v16)
    std::vector<std::vector<int>> ai_demand_counts_;
    std::vector<CargoEvent> cargo_events_;

    // CB-2 Aircraft [RE: aircraft.md §3/§6]: the airborne movement pass --
    // takeoff/cruise/descend + altitude integration. A NEW pass inserted
    // between step_cargo and step_turrets in step(); firewalled behind
    // aircraft_any_ (false until the first aircraft is ordered) so every
    // existing demo pays only the early return [spec §8].
    void step_flight(double dt);
    bool aircraft_any_ = false;

    // Fully-defined-inline POD mirror of HarvestEvent (kind stored as int,
    // via static_cast to/from HarvestEvent::Kind) -- see the forward-
    // declaration comment near the top of this header for why World cannot
    // store a real HarvestEvent (or a container of one) as a member.
    struct PendingHarvestEvent { int kind; int unit; };

    const ra2assets::MapFile* map_;
    std::vector<Unit> units_;
    std::vector<Building> buildings_;
    std::vector<std::uint8_t> passable_;
    std::vector<int> sw_quarry_;            // W5-A set_sw_quarry (transient)
    std::vector<std::uint8_t> wall_leak_;   // W5-A: block_wall_cell cells (pathfind leak mask)
    std::vector<std::uint8_t> crush_wall_;  // P4: crushable wall cells 0/1 standing/2 crushed (save v23)
    mutable std::vector<std::uint8_t> crush_grid_, crush_leak_;   // P4: derived per call (never saved)
    void step_wall_crush();                 // P4: crushers flatten the crushable wall they drive into
    // CB-3 Naval [RE: naval.md §1]: the WATER passability grid, empty by default
    // (built only by set_water_terrain -- the naval demo). Same grid_w_*grid_h_
    // shape as passable_. naval_any_ is a diagnostics/guard flag opened by the
    // first naval order (naval movement reuses step_units -- no separate step to
    // firewall, unlike flight's step_flight).
    std::vector<std::uint8_t> water_passable_;
    mutable std::vector<std::uint8_t> amphib_passable_;   // W2-AN-10 scratch (amphib_grid)
    bool naval_any_ = false;
    Occupancy occ_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;
    // W5-B R-062 land speed (arming state; rebuilt by w5_arm_combat, not saved).
    bool land_speed_armed_ = false;
    std::vector<std::uint8_t> speed_land_, ramp_;
    std::vector<std::int8_t> level_grid_;
    LandSpeedRules land_rules_;
    CrushRules crush_rules_;   // W5-B R-050
    ElevationRules elev_rules_;   // W5-B R-063
    AnimTable anims_;  // Unit::image -> UnitAnimSeq (set_anim); unregistered -> default_anim()
    bool inf_anims_ = false;                       // W3-B
    int inf_human_ = 0;
    double inf_idle_freq_ = 0.15;
    std::map<std::string, InfTable> inf_tables_;

    // --- Economy phase [RE: economy.md] ---
    OreField ore_;
    OreRng ore_rng_;
    // Roster copy + refinery type ids (set_economy). EMPTY types_ (the
    // default -- set_economy never called) is the byte-identity gate:
    // step() skips ore_.step and step_harvest entirely in that case.
    std::vector<BuildingTypeDef> types_;
    std::vector<int> refinery_types_;
    // Type roster for lookups that must NOT arm the economy passes above
    // (set_building_types / btypes()). Read only through btypes().
    std::vector<BuildingTypeDef> lookup_types_;
    // OreValues's two fields, copied by set_economy (see the forward-
    // declaration comment above for why OreValues itself isn't stored).
    int ore_value_ore_ = 25;
    int ore_value_gems_ = 50;
    double growth_interval_ = 0.0;
    double growth_pct_ = 0.0;
    double spread_pct_ = 0.0;
    double purifier_bonus_ = 0.0;   // rules [General] PurifierBonus (gap owner A, G-27)
    HarvestScan harvest_scan_;      // W2-SK-02 (save v13)
    std::vector<double> harvest_income_;   // W2-SK: cumulative unload income per house (v13)
    // T3 [RE: multihouse.md §D2]: double[3] -> std::vector sized house_count().
    // per-house income; index 2 (kNeutralHouse) inert -- neutral holds no credits
    // [RE: capture.md §D1]. NOT hashed. Sized in ensure_houses_default/
    // grow_house_arrays.
    std::vector<double> house_credits_;
    std::vector<PendingHarvestEvent> harvest_events_;  // pending take_harvest_events() drain

    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // ALL inert by default. repair_sell_any_ is opened ONLY by set_repair
    // (turning ON) and begin_sell, neither of which ANY existing demo calls --
    // so step_repair_sell costs every existing gate one increment and one
    // branch, and none of this state is read by world_hash.
    RepairSellRules repair_rules_;              // recovered defaults [§A1]
    bool            repair_sell_any_ = false;   // the firewall
    std::uint64_t   repair_frame_ = 0;          // the global cadence phase [§A2]
    std::vector<UnitCombat>    survivor_crew_;      // by side_ix; empty = none registered
    std::vector<std::uint8_t>  survivor_crew_set_;  // parallel "was registered" flags
    // Finish a sell mission: free the footprint, spawn survivors, pay the FLAT
    // refund, then kill + mark sold. See world.cpp for the recovered ordering.
    void complete_sell(int b_idx);

    // --- P4 Unit special abilities [RE: abilities.md] (world_abilities.cpp) ---
    bool abilities_any_ = false;                      // THE firewall
    std::uint64_t ability_frame_ = 0;                 // logic frame counter
    AbilityRules ability_rules_;
    std::map<std::string, AbilityProfile> ability_profiles_;  // by image
    RadField rad_;
    std::vector<int> no_c4_types_;
    std::vector<AbilityEvent> ability_events_;
    // Teleport `idx` to the nearest free cell at/around (gx,gy) [§C1].
    void teleport_unit(int idx, int gx, int gy);
    // Start a unit's death the combat.cpp way (die_time=0, release occupancy).
    void ability_kill_unit(int idx);
    // Remove a unit from the game without dying (erase, §C2).
    void ability_erase_unit(int idx);
    // DamageArea [§E/§F]: all houses, units + buildings, linear falloff.
    void ability_area_damage(int src_unit, double cx, double cy, const WeaponSpec& w, int damage);
    void ability_delay_kill(int b_idx, const WeaponSpec& w, double dist_leptons);   // P4
public:
    // P4: arm the ability pass for building death weapons (GameScene, real play).
    void arm_building_deaths() { abilities_any_ = true; }
private:
    void ability_fire_event(int idx);
    void ability_death_pass();
    // Weapon application to a unit/building with verses (no splash).
    void ability_damage_unit(int victim, int dmg);
    void ability_damage_building(int b_idx, int dmg);
    void ability_apply_weapon_fields(Unit& u, const WeaponSpec& w);
    void ability_bind_unit(Unit& u);

    // --- Gap fix C (building_combat.cpp) ---
    std::vector<FireEvent> fire_events_;
    // G-50: the sync-RNG stand-in for the ROF Random(0,2) draw (xorshift32
    // with FUN_0065C220's range mapping, like ProjRng; fixed seed).
    ProjRng combat_rng_{0x2545F491u};
    void launch_defense_projectile(int b_idx, int target_unit, int slot);
    Unit defense_shooter(int b_idx, int slot) const;
    // gF MCV deploy state (inert until order_deploy_mcv / set_undeploy_unit).
    bool deploy_any_ = false;
    std::vector<double> buildup_totals_;
    std::vector<DeployEvent> deploy_events_;
    std::vector<int> undeploy_types_;          // roster building index per entry
    std::vector<UnitCombat> undeploy_units_;   // parallel: the unit to spawn
    bool mcv_redeploys_ = false;
    bool skirmish_session_ = false;
    bool try_undeploy(int b_idx);              // called by complete_sell
    void finish_deploy(int idx);

    // --- Wave-5 abilities (world_w5.cpp) ---
    WorldW5 w5_;
    std::vector<std::uint8_t> w5_hide_, w5_own_;   // gap scratch (per frame, not saved)
    void w5_gap_pass(int phase);
    void w5_step_defuse();
    void w5_step_repair_ifv(double dt);
    void w5_ai_pass();
    void w5_step_airstrike();
    bool w5_prism_fire(int bi, int target_unit, int slot);   // building_combat.cpp
    // --- G16 world depth (world_depth.cpp) ---
    DepthRules depth_rules_;
    bool garrison_any_ = false;                    // an occupy order was ever given
    long long garrison_shots_ = 0;                 // shots fired out of garrisons
    std::vector<GarrisonEvent> garrison_events_;
    std::vector<EnteredEvent> entered_events_;   // W2-CA-01 (campaign only)
    bool track_entered_ = false;
    void note_entered(EnteredEvent::Kind k, int ui, int bi, int house, bool consumed) {
        if (track_entered_) entered_events_.push_back({k, ui, bi, house, consumed});
    }
    std::vector<std::int16_t> bridge_ovl_;         // current runtime overlay per cell (-1)
    std::vector<std::uint8_t> bridge_tile_land_;   // the tile's LandType under each cell
    std::uint32_t bridge_version_ = 0;
    std::vector<std::uint8_t> hbridge_;   // W4-SQ-10: 1/2 per set_high_bridges, +4 destroyed (save v16)
    ProjRng bridge_rng_{0x42524447u};              // Random(1, BridgeStrength) stand-in
    void occupy_now(int unit_idx, int b_idx);      // InfantryClass::Occupy (FUN_00522640)
    void sync_garrison_owner(int b_idx);           // FUN_004581F0
    void step_garrison_combat(double dt);
    void set_bridge_cell(int x, int y, int idx);   // overlay + Recalc_Attributes analogue
    int neutral_house() const;
};

// Indices of `units` sorted back-to-front for painter's-order drawing: by
// screen depth (fx + fy) ascending, tie-break fx ascending then index.
// Includes dying units (die_time >= 0 -- the die anim must still render);
// excludes tombstoned (!alive) units. Pure.
std::vector<int> unit_draw_order(const std::vector<Unit>& units);

}  // namespace ra2engine
