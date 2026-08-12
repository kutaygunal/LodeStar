// core/scenario/orbit/Keplerian.cpp
// IS-GPS-200 two-body propagation (Item 1.2).

#include "core/scenario/orbit/Keplerian.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::scenario {

namespace {
constexpr double kMu = 3.986005e14;   // WGS-84 GM, m^3/s^2 (IS-GPS-200)
constexpr double kOmegaE = 7.2921151467e-5;  // Earth rotation, rad/s
constexpr double kC = 299792458.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;
}  // namespace

Keplerian::Keplerian(const BroadcastEphemeris& eph) : eph_(eph) {}

Result<SvState> Keplerian::propagate(double t) const {
    const BroadcastEphemeris& e = eph_;

    // Validate elements (R8).
    if (!(e.sqrtA > 0.0)) {
        return Result<SvState>::err(
            "Keplerian: sqrtA must be positive (got " + std::to_string(e.sqrtA) + ")");
    }
    if (e.e < 0.0 || e.e >= 1.0) {
        return Result<SvState>::err(
            "Keplerian: eccentricity out of range [0,1) (got " + std::to_string(e.e) + ")");
    }
    if (!std::isfinite(t)) {
        return Result<SvState>::err("Keplerian: non-finite time");
    }

    double a = e.sqrtA * e.sqrtA;
    double n0 = std::sqrt(kMu / (a * a * a));
    double n = n0 + e.deltaN;
    double tk = t - e.toe;
    if (tk > 302400.0) tk -= 604800.0;
    if (tk < -302400.0) tk += 604800.0;

    double Mk = e.M0 + n * tk;

    // Solve Kepler's equation E - e*sin(E) = M by Newton-Raphson.
    double E = Mk;
    for (int i = 0; i < 20; ++i) {
        double f = E - e.e * std::sin(E) - Mk;
        double fp = 1.0 - e.e * std::cos(E);
        double dE = f / fp;
        E -= dE;
        if (std::fabs(dE) < 1e-13) break;
        if (i == 19) {
            return Result<SvState>::err("Keplerian: Kepler iteration did not converge");
        }
    }

    double sinE = std::sin(E), cosE = std::cos(E);
    double v = std::atan2(std::sqrt(1.0 - e.e * e.e) * sinE, cosE - e.e);
    double phi = v + e.argp;

    // Harmonic corrections.
    double sin2phi = std::sin(2.0 * phi), cos2phi = std::cos(2.0 * phi);
    double uk = phi + e.cuc * cos2phi + e.cus * sin2phi;
    double rk = a * (1.0 - e.e * cosE) + e.crc * cos2phi + e.crs * sin2phi;
    double ik = e.i0 + e.idot * tk + e.cic * cos2phi + e.cis * sin2phi;

    double xp = rk * std::cos(uk);
    double yp = rk * std::sin(uk);

    // Corrected longitude of ascending node (with Earth rotation).
    double omegak = e.omega0 + (e.omegadot - kOmegaE) * tk - kOmegaE * e.toe;

    double cosO = std::cos(omegak), sinO = std::sin(omegak);
    double cosI = std::cos(ik), sinI = std::sin(ik);

    SvState s;
    s.posEcef = Vec3(xp * cosO - yp * cosI * sinO,
                     xp * sinO + yp * cosI * cosO,
                     yp * sinI);

    // Velocity (time derivatives).
    double Edot = n / (1.0 - e.e * cosE);
    double vdot = std::sqrt(1.0 - e.e * e.e) * Edot / (1.0 - e.e * cosE);
    double phidot = vdot;
    double ukdot = phidot + 2.0 * (e.cus * cos2phi - e.cuc * sin2phi) * phidot;
    double rkdot = e.e * a * sinE * Edot +
                   2.0 * (e.crs * cos2phi - e.crc * sin2phi) * phidot;
    double ikdot = e.idot + 2.0 * (e.cis * cos2phi - e.cic * sin2phi) * phidot;
    double omegakdot = e.omegadot - kOmegaE;

    double xpdot = rkdot * std::cos(uk) - yp * ukdot;
    double ypdot = rkdot * std::sin(uk) + xp * ukdot;

    s.velEcef = Vec3(
        xpdot * cosO - yp * cosI * sinO + yp * sinI * sinO * ikdot -
            yp * cosI * cosO * omegakdot,
        xpdot * sinO + yp * cosI * cosO + yp * sinI * cosO * ikdot -
            yp * cosI * sinO * omegakdot,
        ypdot * sinI + yp * cosI * ikdot);

    // Clock correction: dt = af0 + af1*(t-toe) + af2*(t-toe)^2 + relativistic.
    double dt = e.af0 + e.af1 * tk + e.af2 * tk * tk;
    double rel = -2.0 * std::sqrt(kMu * a) * e.e * sinE / (kC * kC);
    s.clockBias = dt + rel;
    s.clockDrift = e.af1 + 2.0 * e.af2 * tk;

    return Result<SvState>::ok(s);
}

}  // namespace lodestar::scenario
