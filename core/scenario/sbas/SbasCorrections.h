// core/scenario/sbas/SbasCorrections.h
// SBAS fast & long-term corrections, and ionospheric grid (Items 6.2, 6.3).

#pragma once

#include <map>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class SbasCorrections {
public:
    // Fast correction for a PRN at time t.
    void applyFast(int prn, double prc, double udre, double t);
    // Long-term (slow) correction for a PRN.
    void applyLongTerm(int prn, const LongTermCorrection& ltc);

    // Apply all corrections to a satellite state. Returns an error if a required
    // correction is missing or stale for the given PRN.
    Result<SvState> correct(const SvState& sv, int prn, double t) const;

private:
    struct Fast {
        double prc = 0.0, udre = 0.0, t = 0.0;
        bool present = false;
    };
    struct Slow {
        LongTermCorrection ltc;
        bool present = false;
    };
    std::map<int, Fast> fast_;
    std::map<int, Slow> slow_;
};

class SbasIonoGrid {
public:
    void addIgp(const IgpData& igp);

    // Ionospheric delay (m) interpolated over the IGP mesh at the pierce point.
    Result<double> delay(const Vec3& rxEcef, const SvState& sv, const GpsTime& t) const;
    Result<double> give(const Vec3& rxEcef, const SvState& sv) const;

private:
    std::vector<IgpData> igps_;
};

}  // namespace lodestar::scenario
