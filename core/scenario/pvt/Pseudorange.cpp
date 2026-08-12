// core/scenario/pvt/Pseudorange.cpp
// Pseudorange computation (Item 4.1).

#include "core/scenario/pvt/Pseudorange.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::scenario {

namespace {
constexpr double kC = 299792458.0;  // m/s
}  // namespace

double Pseudorange::geometricRange(const SvState& sv, const Vec3& rxEcef) {
    Vec3 d = sv.posEcef - rxEcef;
    double range = d.norm();
    // Sagnac / Earth-rotation correction over signal propagation time.
    double tau = range / kC;
    Vec3 corr = sv.posEcef;
    double sinR = std::sin(Frames::OMEGA_E * tau);
    double cosR = std::cos(Frames::OMEGA_E * tau);
    // Rotate satellite position backward by omega*tau (ECEF was sampled at
    // emission; the receiver ECEF is at reception).
    double corrX = cosR * corr.x + sinR * corr.y;
    double corrY = -sinR * corr.x + cosR * corr.y;
    Vec3 d2(corrX - rxEcef.x, corrY - rxEcef.y, corr.z - rxEcef.z);
    return d2.norm();
}

double Pseudorange::relativisticCorrection(double svClockBiasSec) {
    // The satellite clock bias from Keplerian already includes the relativistic
    // term (-2*sqrt(mu*a)*e*sinE/c^2). As a standalone correction we return the
    // bias in meters.
    return svClockBiasSec * kC;
}

Result<double> Pseudorange::compute(const SvState& sv, const Vec3& rxEcef,
                                    double rxClockBias,
                                    const AtmosphericCorrections& atm) {
    if (!sv.posEcef.isFinite() || !rxEcef.isFinite()) {
        return Result<double>::err("Pseudorange: non-finite input");
    }
    if (!std::isfinite(rxClockBias)) {
        return Result<double>::err("Pseudorange: non-finite receiver clock bias");
    }
    double geo = geometricRange(sv, rxEcef);
    // rho = geo + c*(rxBias - svBias) + iono + tropo + rel
    double svBiasSec = sv.clockBias;
    double relM = relativisticCorrection(svBiasSec);
    // Note: sv.clockBias already includes relativistic; to avoid double-counting
    // we treat rel as the relativistic component of the satellite clock.
    double rho = geo + kC * (rxClockBias - svBiasSec) + atm.iono + atm.tropo + relM;
    if (!(rho > 0.0) || rho > 1e8) {
        return Result<double>::err(
            "Pseudorange: out-of-range value " + std::to_string(rho));
    }
    return Result<double>::ok(rho);
}

}  // namespace lodestar::scenario
