#include "doctest.h"

#include <cmath>

#include "../engine/flight.hpp"
using namespace ra2engine;

TEST_CASE("flight: step_altitude ascends to exactly cruise, no overshoot") {
    // Climb 6 leptons/call toward 750; the last partial step lands EXACTLY on
    // 750 (no overshoot) [RE-GATE APPLIED: aircraft.md §2 -- JumpjetClimb=6].
    double alt = 0.0;
    int calls = 0;
    while (alt < 750.0 && calls < 1000) { alt = step_altitude(alt, 750.0, 6.0); ++calls; }
    CHECK(alt == doctest::Approx(750.0));
    CHECK(calls == 125);                      // ceil(750/6) = 125
    CHECK(step_altitude(750.0, 750.0, 6.0) == doctest::Approx(750.0));  // stays put
}

TEST_CASE("flight: step_altitude descends to exactly 0, no undershoot") {
    double alt = 750.0;
    int calls = 0;
    while (alt > 0.0 && calls < 1000) { alt = step_altitude(alt, 0.0, 12.0); ++calls; }
    CHECK(alt == doctest::Approx(0.0));
    CHECK(calls == 63);                        // ceil(750/12) = 63
}

TEST_CASE("flight: cruise_step advances straight-line and arrives exactly") {
    double fx = 10.0, fy = 10.0;
    // Toward (10, 20): 10 cells at 0.5/call -> arrival after 20 calls, exact.
    int calls = 0;
    bool arrived = false;
    while (!arrived && calls < 100) { arrived = cruise_step(fx, fy, 10.0, 20.0, 0.5); ++calls; }
    CHECK(arrived);
    CHECK(calls == 20);
    CHECK(fx == doctest::Approx(10.0));
    CHECK(fy == doctest::Approx(20.0));
    // A single big step that overshoots snaps exactly onto the goal.
    fx = 0.0; fy = 0.0;
    CHECK(cruise_step(fx, fy, 3.0, 4.0, 100.0));   // dist 5 <= 100 -> arrive
    CHECK(fx == doctest::Approx(3.0));
    CHECK(fy == doctest::Approx(4.0));
}

TEST_CASE("flight: cruise_step midpoint is on the straight line toward the goal") {
    double fx = 0.0, fy = 0.0;
    CHECK_FALSE(cruise_step(fx, fy, 6.0, 8.0, 5.0));  // dist 10, step 5 -> halfway
    CHECK(fx == doctest::Approx(3.0));                // 6 * 5/10
    CHECK(fy == doctest::Approx(4.0));                // 8 * 5/10
}

TEST_CASE("flight: heading_toward turns shortest-arc, exact land, OmniFire no-op") {
    // Toward cell +Y (South): facing_u16_from_delta(0,1) == 32768.
    const double want = facing_u16_from_delta(0.0, 1.0);
    double f = 0.0;                                   // start North
    f = heading_toward(f, 0.0, 1.0, 1000.0, /*omni=*/false);
    CHECK(f == doctest::Approx(1000.0));              // stepped toward want
    // Exact landing when within max_turn.
    f = heading_toward(want - 100.0, 0.0, 1.0, 1000.0, false);
    CHECK(f == doctest::Approx(want));
    // OmniFire: never turns, whatever the direction.
    CHECK(heading_toward(12345.0, 1.0, 0.0, 1000.0, /*omni=*/true) == doctest::Approx(12345.0));
    // Degenerate direction: unchanged.
    CHECK(heading_toward(4242.0, 0.0, 0.0, 1000.0, false) == doctest::Approx(4242.0));
}

TEST_CASE("flight: altitude_to_screen_dy is 0 at 0, monotone, ~107 px at cruise") {
    CHECK(altitude_to_screen_dy(0.0) == 0);
    CHECK(altitude_to_screen_dy(-5.0) == 0);
    CHECK(altitude_to_screen_dy(256.0) == 37);        // round(1 * 36.74)
    CHECK(altitude_to_screen_dy(750.0) == 108);       // round((750/256)*36.74)=107.6
    CHECK(altitude_to_screen_dy(750.0) > altitude_to_screen_dy(375.0));  // monotone
    CHECK(altitude_to_screen_dy(375.0) > altitude_to_screen_dy(0.0));
}

TEST_CASE("flight: advance_flight_state transitions on the altitude predicates") {
    CHECK(advance_flight_state(FlightState::TakingOff, 100.0, 750.0) == FlightState::TakingOff);
    CHECK(advance_flight_state(FlightState::TakingOff, 750.0, 750.0) == FlightState::Cruising);
    CHECK(advance_flight_state(FlightState::TakingOff, 800.0, 750.0) == FlightState::Cruising);
    CHECK(advance_flight_state(FlightState::Descending, 10.0, 750.0) == FlightState::Descending);
    CHECK(advance_flight_state(FlightState::Descending, 0.0, 750.0) == FlightState::Grounded);
    CHECK(advance_flight_state(FlightState::Cruising, 750.0, 750.0) == FlightState::Cruising);
    CHECK(advance_flight_state(FlightState::Grounded, 0.0, 750.0) == FlightState::Grounded);
}
