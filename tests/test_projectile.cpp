#include "doctest.h"

#include <cmath>
#include <set>

#include "projectile.hpp"

using namespace ra2engine;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST_CASE("projectile: weapon Speed= -> leptons/frame (FUN_00474740)") {
    CHECK(weapon_speed_leptons(20) == 51);    // [BlimpBomb] -> Kirov terminal speed (§6.2)
    CHECK(weapon_speed_leptons(40) == 102);   // [120mm]
    CHECK(weapon_speed_leptons(25) == 64);    // [SubTorpedo] (§6.5 table)
    CHECK(weapon_speed_leptons(100) == 255);  // 256 clamped to 255
    CHECK(weapon_speed_leptons(120) == 255);  // clamp(.,0,100) first
    CHECK(weapon_speed_leptons(-5) == 0);
}

TEST_CASE("projectile: ROT==0 launch speed = ftol(sqrt(d*g*1.2)) (§0.2 table)") {
    CHECK(arcing_launch_speed(1472, 6.0) == 102);  // Rhino at 5.75 cells
    CHECK(arcing_launch_speed(768, 6.0) == 74);    // Rhino at 3 cells
    CHECK(arcing_launch_speed(1280, 6.0) == 96);   // Grizzly / Flak Track at 5 cells
    CHECK(arcing_launch_speed(2048, 6.0) == 121);  // Destroyer at 8 cells
    CHECK(arcing_launch_speed(0, 6.0) == 0);
}

TEST_CASE("projectile: lepton_to_cell rounds to the clone's cell-centre convention") {
    CHECK(lepton_to_cell(0) == 0);
    CHECK(lepton_to_cell(127) == 0);
    CHECK(lepton_to_cell(128) == 1);
    CHECK(lepton_to_cell(-128) == 0);
    CHECK(lepton_to_cell(-129) == -1);
    CHECK(lepton_to_cell(5 * 256) == 5);
}

TEST_CASE("projectile: arc solver flat ground matches sin(2theta) = d*g/v^2 (§12.1)") {
    double th = 0.0;
    REQUIRE(arc_solve_core(false, 102, 1472, 0, 6.0, th));
    CHECK(std::sin(2.0 * th) == doctest::Approx(1472.0 * 6.0 / (102.0 * 102.0)).epsilon(1e-9));
    CHECK(th < kPi / 4.0);  // the plus root is the LOW arc
    double hi = 0.0;
    REQUIRE(arc_solve_core(true, 102, 1472, 0, 6.0, hi));
    CHECK(hi > kPi / 4.0);  // the minus root is the lofted arc
    CHECK(th + hi == doctest::Approx(kPi / 2.0).epsilon(1e-9));  // complementary on flat ground
}

TEST_CASE("projectile: arc solver - target above the muzzle is a flat 45 degrees") {
    double th = 0.0;
    REQUIRE(arc_solve_core(false, 50, 1000, 1, 6.0, th));
    CHECK(th == doctest::Approx(kPi / 4.0));
    double phi = 0.0;
    REQUIRE(arc_pitch(50, 1000, 300, 6.0, phi));
    CHECK(phi == doctest::Approx(kPi / 4.0).epsilon(1e-3));
}

TEST_CASE("projectile: arc solver refuses a shot with no solution") {
    double th = 0.0;
    CHECK_FALSE(arc_solve_core(false, 10, 2000, 0, 6.0, th));  // far too slow
    double phi = 0.0;
    CHECK_FALSE(arc_pitch(10, 2000, -100, 6.0, phi));
}

TEST_CASE("projectile: arc_pitch derivative sign test makes a far-below low shot point DOWN") {
    // Muzzle 1000 leptons above the target, short range: the textbook low root is
    // below the horizontal; acos alone would lose that sign (§12.1 wrapper).
    double phi = 0.0;
    REQUIRE(arc_pitch(80, 300, -1000, 6.0, phi));
    CHECK(phi < 0.0);
    // Ordinary flat tank shot points up.
    REQUIRE(arc_pitch(102, 1472, -100, 6.0, phi));
    CHECK(phi > 0.0);
    CHECK(phi < kPi / 4.0);
}

TEST_CASE("projectile: straight_pitch is level within 200 leptons of height") {
    CHECK(straight_pitch(1000, 150) == doctest::Approx(0.0).epsilon(1e-3));
    CHECK(straight_pitch(1000, -199) == doctest::Approx(0.0).epsilon(1e-3));
    const double down = straight_pitch(300, -610);  // Kirov over its target
    CHECK(down < -1.0);                              // steeply down
    CHECK(down == doctest::Approx(-std::atan2(590.0, 300.0)).epsilon(1e-3));
}

TEST_CASE("projectile: ProjRng.random reproduces FUN_0065C220's inclusive range mapping") {
    ProjRng r;
    std::set<int> seen;
    for (int i = 0; i < 2000; ++i) {
        const int v = r.random(3, 7);
        CHECK(v >= 3);
        CHECK(v <= 7);
        seen.insert(v);
    }
    CHECK(seen.size() == 5u);       // inclusive both ends
    CHECK(r.random(9, 9) == 9);     // lo == hi -> lo, no draw needed
    for (int i = 0; i < 200; ++i) {  // swapped bounds behave like (lo, hi)
        const int v = r.random(256, 128);
        CHECK(v >= 128);
        CHECK(v <= 256);
    }
    ProjRng a, b;  // deterministic: same seed, same stream
    for (int i = 0; i < 50; ++i) CHECK(a.random(0, 0x7FFFFFFE) == b.random(0, 0x7FFFFFFE));
}

TEST_CASE("projectile: ProjRng.angle covers the circle") {
    ProjRng r;
    double mn = 10.0, mx = -10.0;
    for (int i = 0; i < 4000; ++i) {
        const double a = r.angle();
        mn = std::min(mn, a);
        mx = std::max(mx, a);
    }
    CHECK(mx - mn > 1.9 * kPi);  // spans (nearly) a full turn
}

TEST_CASE("projectile: ballistic integrator one step (§6.1 semi-implicit Euler, ftol)") {
    Projectile p;
    p.x = 100; p.y = 200; p.z = 50;
    p.vx = 10.7; p.vy = -3.2; p.vz = 20.0;
    integrate_ballistic(p, 6.0);
    CHECK(p.vz == doctest::Approx(14.0));  // gravity BEFORE the position step
    CHECK(p.x == 110);                      // ftol(110.7)
    CHECK(p.y == 196);                      // ftol(196.8)
    CHECK(p.z == 64);                       // 50 + 14
    CHECK_FALSE(p.slow);
    Projectile q;
    q.vx = 3.0;
    integrate_ballistic(q, 6.0);
    CHECK(q.slow);  // |V| < 8.0 sampled before gravity
}

TEST_CASE("projectile: vertical integrator accelerates +Acceleration to the Speed cap, direction frozen") {
    Projectile p;
    p.kind = ProjKind::Vertical;
    p.z = 610;
    p.vz = -1.0;  // launched at 1 lepton/frame (§12.3)
    p.speed_cap = 51;
    p.acceleration = 1;
    int frames = 0;
    while (p.z > 0 && frames < 100) {
        integrate_vertical(p);
        ++frames;
        CHECK(p.vx == 0.0);
        CHECK(p.vy == 0.0);
    }
    // n(n+1)/2 >= 610 -> ~34 frames with the first step at speed 2 (§6.2 worked example).
    CHECK(frames >= 30);
    CHECK(frames <= 40);
    CHECK(std::fabs(p.vz) < 51.0);  // the cap is not reached from Kirov height
    // From high enough the speed saturates at the weapon Speed cap (51).
    Projectile q;
    q.kind = ProjKind::Vertical;
    q.z = 5000;
    q.vz = -1.0;
    q.speed_cap = 51;
    q.acceleration = 1;
    for (int i = 0; i < 80; ++i) integrate_vertical(q);
    CHECK(std::fabs(q.vz) == doctest::Approx(51.0));
}

TEST_CASE("projectile: compute_launch - Rhino at 5.75 cells, flat, no scatter") {
    ProjectileSpec spec;
    spec.kind = ProjKind::Ballistic;
    spec.arcing = true;
    ProjectileRules rules;
    ProjRng rng;
    const ProjRng before = rng;
    const LaunchResult lr = compute_launch(spec, rules, Vec3i{0, 0, 0}, Vec3i{1472, 0, 0}, rng);
    REQUIRE(lr.ok);
    CHECK(lr.speed == 102);
    CHECK(rng.s == before.s);  // no scatter -> no RNG draw
    CHECK(std::sqrt(lr.vx * lr.vx + lr.vy * lr.vy + lr.vz * lr.vz) ==
          doctest::Approx(102.0).epsilon(1e-6));
    CHECK(lr.vx > 0.0);
    CHECK(lr.vy == doctest::Approx(0.0));
    CHECK(lr.vz > 0.0);
}

TEST_CASE("projectile: a flat Rhino shot flies ~16 frames and comes down in the target cell") {
    ProjectileSpec spec;
    spec.kind = ProjKind::Ballistic;
    spec.arcing = true;
    ProjectileRules rules;
    ProjRng rng;
    const LaunchResult lr = compute_launch(spec, rules, Vec3i{0, 0, 0}, Vec3i{1472, 0, 0}, rng);
    REQUIRE(lr.ok);
    Projectile p;
    p.vx = lr.vx; p.vy = lr.vy; p.vz = lr.vz;
    int frames = 0, apex = 0;
    bool arrived = false;
    while (frames < 60) {
        integrate_ballistic(p, rules.gravity);
        ++frames;
        apex = std::max(apex, p.z);
        if (lepton_to_cell(p.x) == lepton_to_cell(1472) && lepton_to_cell(p.y) == 0 &&
            p.z < kArrivalHeight) { arrived = true; break; }
        if (p.z < 0) break;
    }
    CHECK(arrived);
    CHECK(frames >= 13);   // §4.1: T ~ 0.4226*sqrt(1472) = 16.2 frames to landing
    CHECK(frames <= 17);
    CHECK(apex > 150);
}

TEST_CASE("projectile: speed cap - a shot never crosses more than half the distance in frame 1") {
    ProjectileSpec spec;
    spec.kind = ProjKind::Ballistic;  // straight (non-arcing) ROT==0
    ProjectileRules rules;
    ProjRng rng;
    const LaunchResult lr = compute_launch(spec, rules, Vec3i{0, 0, 0}, Vec3i{20, 0, 0}, rng);
    REQUIRE(lr.ok);
    CHECK(lr.speed == 10);  // sqrt(20*7.2)=12 capped to 20/2
}

TEST_CASE("projectile: compute_launch - Vertical launches at speed 1 toward the target") {
    ProjectileSpec spec;
    spec.kind = ProjKind::Vertical;
    spec.speed_cap = 51;
    spec.acceleration = 1;
    ProjectileRules rules;
    ProjRng rng;
    const LaunchResult lr = compute_launch(spec, rules, Vec3i{0, 0, 610}, Vec3i{200, 0, 0}, rng);
    REQUIRE(lr.ok);
    CHECK(lr.speed == 1);
    CHECK(std::sqrt(lr.vx * lr.vx + lr.vy * lr.vy + lr.vz * lr.vz) ==
          doctest::Approx(1.0).epsilon(1e-9));
    CHECK(lr.vz < -0.9);  // nearly straight down
    CHECK(lr.vx > 0.0);
}

TEST_CASE("projectile: §5.1 scatter radius in [BS/2, BS]; §5.2 FlakScatter scales with distance") {
    ProjectileRules rules;
    ProjectileSpec s1;
    s1.kind = ProjKind::Ballistic;
    s1.arcing = true;
    s1.inaccurate = true;  // no FlakScatter -> §5.1
    ProjRng rng;
    for (int i = 0; i < 100; ++i) {
        const LaunchResult lr = compute_launch(s1, rules, Vec3i{0, 0, 100}, Vec3i{1280, 0, 0}, rng);
        const double r = std::hypot(lr.scatter_dx, lr.scatter_dy);
        CHECK(r >= 128.0 - 2.0);
        CHECK(r <= 256.0 + 2.0);
    }
    ProjectileSpec s2 = s1;
    s2.flak_scatter = true;  // [FlakTProj] -> §5.2
    s2.weapon_range = 1280;
    double near_max = 0.0, far_max = 0.0;
    for (int i = 0; i < 200; ++i) {
        const LaunchResult a = compute_launch(s2, rules, Vec3i{0, 0, 100}, Vec3i{256, 0, 0}, rng);
        const LaunchResult b = compute_launch(s2, rules, Vec3i{0, 0, 100}, Vec3i{1280, 0, 0}, rng);
        near_max = std::max(near_max, std::hypot(a.scatter_dx, a.scatter_dy));
        far_max = std::max(far_max, std::hypot(b.scatter_dx, b.scatter_dy));
    }
    CHECK(near_max <= 256.0 * 280.0 / 1280.0 + 2.0);  // ~ BS * d/range at 1 cell
    CHECK(far_max > near_max);
    CHECK(far_max <= 256.0 + 2.0);
}

// ---- Task 3: rules resolution ------------------------------------------------
#include "asset_fixture.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/rules.hpp"

TEST_CASE("projectile: ini_bool_value follows the game's first-char bool with inline comments") {
    CHECK(ini_bool_value("yes ;can't turn or do much of anything", false));
    CHECK(ini_bool_value("true", false));
    CHECK(ini_bool_value("Yes", false));
    CHECK(ini_bool_value("1", false));
    CHECK_FALSE(ini_bool_value("no", true));
    CHECK_FALSE(ini_bool_value("false ; x", true));
    CHECK(ini_bool_value("", true));        // absent -> fallback
    CHECK(ini_bool_value("  ; only", true));
}

TEST_CASE("projectile: resolve_projectile_spec classifies a synthetic roster") {
    ra2assets::Ini ini = ra2assets::Ini::parse(
        "[120mm]\nDamage=90\nRange=5.75\nProjectile=Cannon\nSpeed=40\n"
        "[Cannon]\nArcing=true\n"
        "[BlimpBomb]\nRange=1.5\nProjectile=BlimpBombP\nSpeed=20\n"
        "[BlimpBombP]\nArm=10\nAcceleration=1\nVertical=yes ;can't turn\nDetonationAltitude=20000 ; nuke\n"
        "[M60]\nProjectile=InvisibleLow\nSpeed=100\n[InvisibleLow]\nInviso=yes\n"
        "[SubTorpedo]\nProjectile=Torpedo\nSpeed=25\n[Torpedo]\nROT=12 ;4\n"
        "[FlakTrackGun]\nRange=5\nProjectile=FlakTProj\nSpeed=50\n"
        "[FlakTProj]\nArcing=true\nInaccurate=yes  ; no snap\nFlakScatter=yes ; scatter\n"
        "[AudioVisual]\nGravity=6               ; gravity\n"
        "[CombatDamage]\nBallisticScatter=1.0 ; cells\n");
    const ra2assets::Rules rules = ra2assets::Rules::load(ini);

    const ProjectileSpec c = resolve_projectile_spec(rules, "120mm");
    CHECK(c.kind == ProjKind::Ballistic);
    CHECK(c.arcing);
    CHECK_FALSE(c.inaccurate);
    CHECK(c.weapon_range == 1472);
    CHECK(c.speed_cap == 102);

    const ProjectileSpec k = resolve_projectile_spec(rules, "BlimpBomb");
    CHECK(k.kind == ProjKind::Vertical);  // inline comment tolerated
    CHECK(k.speed_cap == 51);
    CHECK(k.acceleration == 1);
    CHECK(k.detonation_altitude == 20000);

    CHECK(resolve_projectile_spec(rules, "M60").kind == ProjKind::Instant);        // Inviso
    CHECK(resolve_projectile_spec(rules, "SubTorpedo").kind == ProjKind::Instant); // homing deferred
    CHECK(resolve_projectile_spec(rules, "SubTorpedo").rot == 12);
    CHECK(resolve_projectile_spec(rules, "").kind == ProjKind::Instant);
    CHECK(resolve_projectile_spec(rules, "NoSuchWeapon").kind == ProjKind::Instant);

    const ProjectileSpec f = resolve_projectile_spec(rules, "FlakTrackGun");
    CHECK(f.kind == ProjKind::Ballistic);
    CHECK(f.inaccurate);
    CHECK(f.flak_scatter);
    CHECK(f.weapon_range == 1280);

    const ProjectileRules pr = resolve_projectile_rules(rules);
    CHECK(pr.gravity == doctest::Approx(6.0));
    CHECK(pr.ballistic_scatter == 256);
}

TEST_CASE("projectile: real rulesmd pins (Cannon/BlimpBombP/FlakTProj/InvisibleLow, Gravity in [AudioVisual])") {
    if (!ra2fixture::assets_available()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    if (ra2fixture::rulesmd_bytes().empty()) { MESSAGE("skipped: rulesmd.ini not found"); return; }
    const ra2assets::Rules& rules = ra2fixture::rulesmd();
    const ProjectileRules pr = resolve_projectile_rules(rules);
    CHECK(pr.gravity == doctest::Approx(6.0));   // [AudioVisual] Gravity=6 (§0.1)
    CHECK(pr.ballistic_scatter == 256);          // [CombatDamage] BallisticScatter=1.0

    const ProjectileSpec rhino = resolve_projectile_spec(rules, rules.primary("HTNK"));
    CHECK(rhino.kind == ProjKind::Ballistic);
    CHECK(rhino.arcing);
    CHECK(rhino.weapon_range == 1472);

    const ProjectileSpec kirov = resolve_projectile_spec(rules, rules.primary("ZEP"));
    CHECK(kirov.kind == ProjKind::Vertical);
    CHECK(kirov.speed_cap == 51);
    CHECK(kirov.acceleration == 1);
    CHECK(kirov.detonation_altitude == 20000);

    const ProjectileSpec flak = resolve_projectile_spec(rules, rules.primary("HTK"));
    CHECK(flak.kind == ProjKind::Ballistic);
    CHECK(flak.inaccurate);
    CHECK(flak.flak_scatter);

    CHECK(resolve_projectile_spec(rules, rules.primary("E1")).kind == ProjKind::Instant);
    CHECK(resolve_projectile_spec(rules, rules.primary("E2")).kind == ProjKind::Instant);
}
