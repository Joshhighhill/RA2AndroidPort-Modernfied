#pragma once
// Pure CB-2 flight-locomotor logic [RE: docs/re/aircraft.md]: the altitude
// climb/descend integrator (step_altitude), the straight-line cruise advance
// (cruise_step -- never consults passable_/occ_, they are not even parameters),
// the shortest-arc heading turn (heading_toward -- no-op on OmniFire), the
// altitude->screen pixel offset (altitude_to_screen_dy), and the altitude-
// driven state transitions (advance_flight_state). Layered on Unit exactly
// like cargo.hpp/harvest.hpp (world.hpp defines Unit + FlightState; this module
// includes world.hpp; world.hpp does NOT include this back). No SDL/OS.
#include "world.hpp"  // Unit, FlightState, facing_u16_from_delta, turn_toward

namespace ra2engine {

// Integrate `altitude` (leptons) toward `target` (leptons) by at most
// `max_delta` (leptons this call = rate_per_sec * dt), clamped so it NEVER
// overshoots [RE-GATE APPLIED: aircraft.md §2 -- JumpjetClimb=6 ascend /
// JumpjetCrash=12 descend leptons/frame (CONFIRMED offsets +0xD78/+0xD7C)].
// Returns the new altitude; lands exactly on `target`.
double step_altitude(double altitude, double target, double max_delta);

// Advance (fx, fy) (cell space) in a STRAIGHT LINE toward (gx, gy) by at most
// `max_step` cells this call (= flight_speed_cps * dt). Writes fx/fy in place;
// snaps exactly onto the goal and returns true on arrival, else false. NEVER
// consults passable_/occ_ -- they are not parameters [RE-GATE APPLIED:
// aircraft.md §1 -- straight-line, unpathed flight CONFIRMED (Process is
// atan2 + cos/sin translate, no A*/passable_/cell walk)].
bool cruise_step(double& fx, double& fy, double gx, double gy, double max_step);

// Turn `body_facing` (u16) toward the (dx, dy) travel direction by at most
// `max_turn` u16 this call (= flight_turn_per_sec * dt), shortest arc, exact
// landing (reuses turn_toward + facing_u16_from_delta -- the engine's one
// facing convention). Returns `body_facing` unchanged when `omni_fire` (the
// OmniFire weapon needs no heading -- [RE-GATE APPLIED: aircraft.md §1 -- the
// caller-supplied max_turn = flight_turn*dt where flight_turn is ROT x2, NOT
// JumpJetTurnRate; Process turns AND translates the same frame]) or the
// direction is degenerate (dx == 0 && dy == 0).
double heading_toward(double body_facing, double dx, double dy,
                      double max_turn, bool omni_fire);

// The §5 raised-sprite vertical pixel offset for a given `altitude` (leptons):
// 0 at altitude 0, monotone increasing [RE-GATE APPLIED: aircraft.md §3 --
// [RE-FALLBACK] (the loco pixel-offset formula was NOT unwound): dy =
// round((altitude/256) * 36.74) (voxel-render.md §6 up/z term); ~107 px at
// JumpjetHeight=750].
int altitude_to_screen_dy(double altitude);

// Altitude-DRIVEN state transitions only (order-driven ones are set by World):
// TakingOff -> Cruising once altitude >= cruise_altitude; Descending ->
// Grounded once altitude <= 0; Grounded/Cruising pass through unchanged.
FlightState advance_flight_state(FlightState state, double altitude,
                                 double cruise_altitude);

}  // namespace ra2engine
