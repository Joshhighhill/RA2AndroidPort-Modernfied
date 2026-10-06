#include "projectile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ra2engine {

namespace {

constexpr double kPi = 3.14159265358979323846;
// 16-bit binary-angle conversions read out of the image (§1 constants table):
// 0x7E1818 = -65534/2pi (radians -> u16), 0x7E1810 = -2pi/65534 (u16 -> radians),
// 0x7E1820 = pi/2 (the pitch datum); 0x3FFF is the level datum.
constexpr double kRadToU16 = -65534.0 / (2.0 * kPi);
constexpr double kU16ToRad = -2.0 * kPi / 65534.0;
constexpr double kHalfPi = kPi / 2.0;
constexpr int kLevelDatum = 0x3FFF;

// Fire's pitch quantisation: radians -> 16-bit binary angle -> radians.
double quantise_pitch(double theta) {
    int p16 = ftol((theta - kHalfPi) * kRadToU16) & 0xFFFF;  // stored in a 16-bit word
    if (p16 >= 0x8000) p16 -= 0x10000;                        // (short) reinterpretation
    return static_cast<double>(p16 - kLevelDatum) * kU16ToRad;
}

}  // namespace

int ftol(double v) { return static_cast<int>(std::trunc(v)); }

int lepton_to_cell(int l) {
    return static_cast<int>(std::floor((static_cast<double>(l) + 128.0) / 256.0));
}

int weapon_speed_leptons(int ini_speed) {
    // FUN_00474740 (§1/§6.5): min(255, clamp(Speed,0,100) * 256 / 100), ftol.
    const double s = std::clamp(static_cast<double>(ini_speed), 0.0, 100.0) * 256.0 / 100.0;
    return ftol(std::min(255.0, s));
}

int arcing_launch_speed(int dist2d, double gravity) {
    // FUN_0048AAA0 (§0.2): FILD d; FMUL g; FMUL 1.2 (DAT_007E4190); sqrt; ftol.
    const double v = static_cast<double>(dist2d) * gravity * 1.2;
    return v > 0.0 ? ftol(std::sqrt(v)) : 0;
}

bool arc_solve_core(bool lofted, int v, int d, int z, double g, double& theta) {
    // FUN_0048A8E0, unwound in §12.1.
    if (z > 0) {
        theta = kPi / 4.0;  // literal 0x3FE921D9F4D37C12
        return true;
    }
    const double vv = static_cast<double>(v) * v;
    const double dz = static_cast<double>(z);
    const double dd = static_cast<double>(d) * d;
    const double a = vv - dz * g;
    const double disc = vv * vv - 2.0 * vv * dz * g - g * g * dd;
    double den = (dz * dz) / dd + 1.0;
    den = den + den;
    if (!(disc >= 0.0) || den == 0.0) return false;
    const double root = std::sqrt(disc);
    const double t = (lofted ? (a - root) : (a + root)) / den;
    if (!(t >= 0.0) || v == 0) return false;
    double c = std::sqrt(t) / static_cast<double>(v);
    c = std::clamp(c, -1.0, 1.0);  // FUN_004CACD0 is a table acos; guard the domain
    theta = std::acos(c);
    return true;
}

bool arc_pitch(int v, int d, int z, double g, double& phi) {
    // FUN_0048A7E0 (§12.1), low root: core(d), then core(d+1) and negate when
    // the angle FALLS with distance (the downward branch acos cannot express).
    double th = 0.0;
    if (!arc_solve_core(false, v, d, z, g, th)) return false;
    double th1 = th;  // original: uninitialised stack slot if this call fails
    arc_solve_core(false, v, d + 1, z, g, th1);
    if (th1 < th) th = -th;
    phi = quantise_pitch(th);
    return true;
}

double straight_pitch(int horiz, int dz) {
    // Fire, non-arcing branch (§4 step 7): level datum unless |dz| > 200, then
    // atan2(|dz| - 20, horiz). The 20.0 is dropped only for a building target
    // within tolerance (not modelled). Sign toward the target: [PROBABLE].
    const int adz = std::abs(dz);
    if (adz <= 200) return quantise_pitch(0.0);
    const double h = static_cast<double>(std::max(horiz, 1));
    double th = std::atan2(static_cast<double>(adz) - 20.0, h);
    if (dz < 0) th = -th;
    return quantise_pitch(th);
}

std::uint32_t ProjRng::next() {
    std::uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (x == 0) x = 0x9E3779B9u;
    s = x;
    return x;
}

int ProjRng::random(int lo, int hi) {
    // FUN_0065C220 (§12.6): exact range mapping over our bit source.
    if (lo == hi) return lo;
    if (hi < lo) std::swap(lo, hi);
    const std::uint32_t range = static_cast<std::uint32_t>(hi) - static_cast<std::uint32_t>(lo);
    int b = 31;
    while (b > 0 && !(range & (1u << b))) --b;
    const std::uint64_t mask = (std::uint64_t{1} << (b + 1)) - 1;
    std::uint32_t v;
    do {
        v = static_cast<std::uint32_t>(next() & mask);
    } while (v > range);
    return static_cast<int>(static_cast<std::uint32_t>(lo) + v);
}

double ProjRng::angle() {
    // §5: r = Random(0,0x7FFFFFFE); ang16 = ftol((r/2^31*2pi - pi/2) * (-65534/2pi))
    // (0x7E2570 = 1/2^31); theta = ((short)ang16 - 0x3FFF) * (-2pi/65534).
    const int r = random(0, 0x7FFFFFFE);
    const double a = (static_cast<double>(r) / 2147483648.0) * (2.0 * kPi);
    int a16 = ftol((a - kHalfPi) * kRadToU16) & 0xFFFF;
    if (a16 >= 0x8000) a16 -= 0x10000;
    return static_cast<double>(a16 - kLevelDatum) * kU16ToRad;
}

LaunchResult compute_launch(const ProjectileSpec& spec, const ProjectileRules& rules,
                            const Vec3i& src, const Vec3i& tgt, ProjRng& rng) {
    LaunchResult out;
    double g = rules.gravity;
    if (spec.floater) g = rules.gravity * 0.5;  // FUN_0048AC00 (§6.1)

    const int tdx = tgt.x - src.x;
    const int tdy = tgt.y - src.y;
    const int dz = tgt.z - src.z;

    // Step 2 (§0.2): GetSpeed(distance) -- 2-D muzzle->target distance, pre-scatter.
    const int d2 = ftol(std::sqrt(static_cast<double>(tdx) * tdx + static_cast<double>(tdy) * tdy));
    int speed = (spec.rot == 0) ? arcing_launch_speed(d2, g) : spec.speed_cap;

    // Step 4 (§5.1/§5.2): scatter perturbs the working delta. Radius drawn FIRST,
    // then the angle (the order the two FUN_0065C220 calls appear in Fire).
    int dx = tdx, dy = tdy;
    if (spec.inaccurate && spec.arcing) {
        double r = 0.0;
        if (!spec.flak_scatter || spec.inviso) {
            // §5.1: Random(BallisticScatter/2, BallisticScatter).
            r = static_cast<double>(rng.random(rules.ballistic_scatter / 2, rules.ballistic_scatter));
        } else {
            // §5.2 [FlakTProj]: Random(0, BS) * ftol(|delta|_3d) / WeaponRange (integer).
            const int dist = ftol(std::sqrt(static_cast<double>(dx) * dx +
                                            static_cast<double>(dy) * dy +
                                            static_cast<double>(dz) * dz));
            const int pick = rng.random(0, rules.ballistic_scatter);
            r = spec.weapon_range > 0
                    ? static_cast<double>((static_cast<long long>(pick) * dist) / spec.weapon_range)
                    : 0.0;
        }
        const double th = rng.angle();
        dx = ftol(static_cast<double>(dx) + std::cos(th) * r);
        dy = ftol(static_cast<double>(dy) - std::sin(th) * r);
    }
    out.aim_dx = dx;
    out.aim_dy = dy;
    out.scatter_dx = dx - tdx;
    out.scatter_dy = dy - tdy;

    // Step 3 (§4): never cross more than half the remaining distance in frame 1.
    const int d3 = ftol(std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy +
                                  static_cast<double>(dz) * dz));
    if (d3 / 2 < speed) speed = d3 / 2;
    // §12.3: Vertical (and homing) projectiles leave the launcher at 1 lept/frame.
    if (spec.kind == ProjKind::Vertical || spec.rot > 0) speed = 1;

    // Step 7: pitch.
    const int horiz = ftol(std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));
    double phi = 0.0;
    if (spec.arcing) {
        if (!arc_pitch(speed, horiz, dz, g, phi)) {
            out.ok = false;  // §4: "the bullet is destroyed and the shot does not happen"
            out.speed = speed;
            return out;
        }
    } else {
        phi = straight_pitch(horiz, dz);
    }

    // Steps 6/8: |V| = speed along the yaw, then pitched (V.xy *= cos, V.z = sin*|V|).
    double ux = 1.0, uy = 0.0;  // degenerate (target straight below): any yaw
    const double hl = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
    if (hl > 0.0) {
        ux = dx / hl;
        uy = dy / hl;
    }
    const double s = static_cast<double>(speed);
    out.vx = ux * s * std::cos(phi);
    out.vy = uy * s * std::cos(phi);
    out.vz = s * std::sin(phi);
    out.speed = speed;
    out.ok = true;
    return out;
}

void integrate_ballistic(Projectile& p, double gravity) {
    // FUN_004666D0 non-homing !Vertical branch (§6.1, dis_arcgrav_4673F2.txt).
    p.slow = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz) < kMinSpeed;
    const double g = p.floater ? gravity * 0.5 : gravity;
    p.vz -= g;
    p.x = ftol(static_cast<double>(p.x) + p.vx);
    p.y = ftol(static_cast<double>(p.y) + p.vy);
    p.z = ftol(static_cast<double>(p.z) + p.vz);
    p.max_z = std::max(p.max_z, p.z);
}

void integrate_vertical(Projectile& p) {
    // FUN_004666D0 Vertical branch (§6.2).
    const double mag = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
    const int s = ftol(mag);
    if (s < p.speed_cap) {
        const int ns = p.acceleration + s;
        if (p.vx == 0.0 && p.vy == 0.0 && p.vz == 0.0) p.vx = 100.0;  // 0x40590000 = 100.0
        const double m = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
        const double f = static_cast<double>(ns) / m;
        p.vx *= f;
        p.vy *= f;
        p.vz *= f;
    }
    p.x = ftol(static_cast<double>(p.x) + p.vx);
    p.y = ftol(static_cast<double>(p.y) + p.vy);
    p.z = ftol(static_cast<double>(p.z) + p.vz);
    p.max_z = std::max(p.max_z, p.z);
}

}  // namespace ra2engine
