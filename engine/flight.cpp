#include "flight.hpp"

#include <cmath>

namespace ra2engine {

double step_altitude(double altitude, double target, double max_delta) {
    const double d = target - altitude;
    if (std::fabs(d) <= max_delta) return target;      // no overshoot: land exactly
    return altitude + (d > 0.0 ? max_delta : -max_delta);
}

bool cruise_step(double& fx, double& fy, double gx, double gy, double max_step) {
    const double dx = gx - fx;
    const double dy = gy - fy;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= max_step || dist == 0.0) {             // arrival: snap exactly
        fx = gx;
        fy = gy;
        return true;
    }
    fx += dx / dist * max_step;                        // straight-line advance
    fy += dy / dist * max_step;
    return false;
}

double heading_toward(double body_facing, double dx, double dy,
                      double max_turn, bool omni_fire) {
    if (omni_fire) return body_facing;                 // OmniFire: never turns
    if (dx == 0.0 && dy == 0.0) return body_facing;    // degenerate direction
    const double want = facing_u16_from_delta(dx, dy);
    return turn_toward(body_facing, want, max_turn);   // shortest arc, exact land
}

int altitude_to_screen_dy(double altitude) {
    // [RE-GATE APPLIED: aircraft.md §3 -- [RE-FALLBACK] (loco pixel-offset not
    // unwound): the voxel up/z term is ~36.74 px per cell-of-z (voxel-render.md
    // §6), so dy = (altitude/256) * 36.74.]
    if (altitude <= 0.0) return 0;
    return static_cast<int>(std::lround(altitude / 256.0 * 36.74));
}

FlightState advance_flight_state(FlightState state, double altitude,
                                 double cruise_altitude) {
    if (state == FlightState::TakingOff && altitude >= cruise_altitude)
        return FlightState::Cruising;
    if (state == FlightState::Descending && altitude <= 0.0)
        return FlightState::Grounded;
    return state;
}

}  // namespace ra2engine
