// core/scenario/pvt/Doppler.cpp
// Doppler computation from range rate (Item 4.2).

#include "core/scenario/pvt/Doppler.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
constexpr double kC = 299792458.0;  // m/s
}  // namespace

double Doppler::rangeRate(const SvState& sv, const Vec3& rxEcef,
                          const Vec3& rxVelEcef) {
    Vec3 los = sv.posEcef - rxEcef;
    double r = los.norm();
    if (r <= 0.0) return 0.0;
    Vec3 u = los.normalized();
    Vec3 relVel = sv.velEcef - rxVelEcef;
    return dot(relVel, u);
}

Result<double> Doppler::compute(const SvState& sv, const Vec3& rxEcef,
                                const Vec3& rxVelEcef, double rxClockDrift,
                                double carrierHz) {
    if (!sv.posEcef.isFinite() || !rxEcef.isFinite() || !rxVelEcef.isFinite()) {
        return Result<double>::err("Doppler: non-finite input");
    }
    if (!(carrierHz > 0.0)) {
        return Result<double>::err("Doppler: non-positive carrier frequency");
    }
    double rr = rangeRate(sv, rxEcef, rxVelEcef);
    // Include receiver clock drift contribution.
    double lambda = kC / carrierHz;
    double fd = -rr / lambda + rxClockDrift * carrierHz;
    if (!std::isfinite(fd) || std::fabs(fd) > 1e7) {
        return Result<double>::err("Doppler: out-of-range value " + std::to_string(fd));
    }
    return Result<double>::ok(fd);
}

}  // namespace lodestar::scenario
