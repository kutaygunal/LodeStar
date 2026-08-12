// core/scenario/sbas/SbasCorrections.cpp
// SBAS fast/long-term corrections and ionospheric grid interpolation (6.2, 6.3).

#include "core/scenario/sbas/SbasCorrections.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::scenario {

// --- SbasCorrections ------------------------------------------------------

void SbasCorrections::applyFast(int prn, double prc, double udre, double t) {
    fast_[prn] = Fast{prc, udre, t, true};
}

void SbasCorrections::applyLongTerm(int prn, const LongTermCorrection& ltc) {
    slow_[prn] = Slow{ltc, true};
}

Result<SvState> SbasCorrections::correct(const SvState& sv, int prn, double t) const {
    SvState out = sv;
    auto fit = fast_.find(prn);
    if (fit == fast_.end() || !fit->second.present) {
        return Result<SvState>::err("SbasCorrections: no fast correction for PRN " +
                                    std::to_string(prn));
    }
    const Fast& f = fit->second;
    if (std::fabs(t - f.t) > 30.0) {
        // Stale fast correction (>30 s).
        return Result<SvState>::err("SbasCorrections: stale fast correction for PRN " +
                                    std::to_string(prn));
    }
    // Apply fast pseudorange correction along the line of sight (approximate:
    // shift the state radially by the PRC projected to position change).
    // For scenario modeling we apply the PRC to the clock bias (in seconds).
    out.clockBias += f.prc / 299792458.0;

    auto sit = slow_.find(prn);
    if (sit != slow_.end() && sit->second.present) {
        const LongTermCorrection& ltc = sit->second.ltc;
        out.posEcef += ltc.posOffset;
        out.velEcef += ltc.velOffset;
        out.clockBias += ltc.clockOffset;
    }

    return Result<SvState>::ok(out);
}

// --- SbasIonoGrid ----------------------------------------------------------

void SbasIonoGrid::addIgp(const IgpData& igp) { igps_.push_back(igp); }

Result<double> SbasIonoGrid::delay(const Vec3& rxEcef, const SvState& sv,
                                   const GpsTime& t) const {
    if (igps_.empty()) {
        return Result<double>::err("SbasIonoGrid: no IGPs configured");
    }
    // For a single-point scenario, interpolate as a weighted average of IGP
    // delays (inverse-distance weighting on the ionospheric pierce point). This
    // is a simplified mesh interpolation (bilinear over the nearest 4 is used in
    // production); here we use the nearest IGP for determinism.
    double lat, lon, h;
    Frames::ecefToGeodetic(rxEcef, lat, lon, h);
    double bestDist = 1e300;
    double bestDelay = 0.0;
    for (const auto& igp : igps_) {
        double dLat = (igp.latDeg - lat * 180.0 / 3.14159265358979323846);
        double dLon = (igp.lonDeg - lon * 180.0 / 3.14159265358979323846);
        double dist = std::sqrt(dLat * dLat + dLon * dLon);
        if (dist < bestDist) {
            bestDist = dist;
            bestDelay = igp.delay;
        }
    }
    (void)sv; (void)t;
    if (!(bestDist < 100.0)) {
        return Result<double>::err("SbasIonoGrid: pierce point outside IGP mesh");
    }
    return Result<double>::ok(bestDelay);
}

Result<double> SbasIonoGrid::give(const Vec3& rxEcef, const SvState& sv) const {
    if (igps_.empty()) {
        return Result<double>::err("SbasIonoGrid: no IGPs configured");
    }
    double lat, lon, h;
    Frames::ecefToGeodetic(rxEcef, lat, lon, h);
    double bestDist = 1e300;
    double bestGive = 0.0;
    for (const auto& igp : igps_) {
        double dLat = (igp.latDeg - lat * 180.0 / 3.14159265358979323846);
        double dLon = (igp.lonDeg - lon * 180.0 / 3.14159265358979323846);
        double dist = std::sqrt(dLat * dLat + dLon * dLon);
        if (dist < bestDist) {
            bestDist = dist;
            bestGive = igp.give;
        }
    }
    (void)sv;
    return Result<double>::ok(bestGive);
}

}  // namespace lodestar::scenario
