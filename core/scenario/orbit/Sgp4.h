// core/scenario/orbit/Sgp4.h
// SGP4/SDP4 propagator for TLE-based LEO/deep-space satellites (Item 1.3).
//
// Implements the standard SGP4/SDP4 model (Vallado, AIAA 2006-6753) with the
// 2006 corrections. Near-Earth satellites use SGP4; deep-space satellites
// (period > 225 min) use SDP4 with lunar/solar and resonance terms.
//
// Output is in the TEME frame (km, km/s), matching the TLE reference frame.
// Convert TEME -> ECEF via Frames::eciToEcef (GMST) for downstream use.

#pragma once

#include "core/scenario/Types.h"
#include "core/scenario/orbit/Tle.h"

namespace lodestar::scenario {

class Sgp4 {
public:
    // Build the propagator from a parsed TLE. Returns an error if the TLE
    // cannot be initialized (e.g. invalid mean motion).
    static Result<Sgp4> create(const Tle& tle);

    // Propagate to `minutesSinceEpoch` (minutes after the TLE epoch).
    // Returns TEME position (km) and velocity (km/s).
    Result<TemeState> propagate(double minutesSinceEpoch) const;

    bool isDeepSpace() const { return deepSpace_; }

private:
    // Internal state (initialized by create()).
    bool deepSpace_ = false;
    double bstar_ = 0.0;
    double inclo_ = 0.0;   // rad
    double nodeo_ = 0.0;   // rad
    double ecco_ = 0.0;
    double argpo_ = 0.0;   // rad
    double mo_ = 0.0;      // rad
    double no_ = 0.0;      // rad/min (mean motion)

    // SGP4 constants.
    double a_ = 0.0, cc1_ = 0.0, cc4_ = 0.0, cc5_ = 0.0, d2_ = 0.0, d3_ = 0.0,
           d4_ = 0.0, t2cof_ = 0.0, t3cof_ = 0.0, t4cof_ = 0.0, t5cof_ = 0.0,
           x1mth2_ = 0.0, x7thm1_ = 0.0, xlcof_ = 0.0, aycof_ = 0.0,
           xmcof_ = 0.0, delmo_ = 0.0, sinmo_ = 0.0, x3thm1_ = 0.0,
           xmdot_ = 0.0, omgdot_ = 0.0, xnodot_ = 0.0, xnodcf_ = 0.0,
           xl_ = 0.0, omgcof_ = 0.0, xmcof2_ = 0.0,
           nodecf_ = 0.0, isimp_ = 0.0, irez_ = 0.0, d2201_ = 0.0,
           d2211_ = 0.0, d3210_ = 0.0, d3222_ = 0.0, d4410_ = 0.0,
           d4422_ = 0.0, d5220_ = 0.0, d5232_ = 0.0, d5421_ = 0.0,
           d5433_ = 0.0, dedt_ = 0.0, del1_ = 0.0, del2_ = 0.0, del3_ = 0.0,
           didt_ = 0.0, dmdt_ = 0.0, dnodt_ = 0.0, domdt_ = 0.0,
           e3_ = 0.0, ee2_ = 0.0, peo_ = 0.0, pgho_ = 0.0, pho_ = 0.0,
           pinco_ = 0.0, plo_ = 0.0, se2_ = 0.0, se3_ = 0.0, sgh2_ = 0.0,
           sgh3_ = 0.0, sgh4_ = 0.0, sh2_ = 0.0, sh3_ = 0.0, si2_ = 0.0,
           si3_ = 0.0, sl2_ = 0.0, sl3_ = 0.0, sl4_ = 0.0, gsto_ = 0.0,
           xfact_ = 0.0, xgh2_ = 0.0, xgh3_ = 0.0, xgh4_ = 0.0, xh2_ = 0.0,
           xh3_ = 0.0, xi2_ = 0.0, xi3_ = 0.0, xl2_ = 0.0, xl3_ = 0.0,
           xl4_ = 0.0, xlamo_ = 0.0, zmol_ = 0.0, zmos_ = 0.0, atime_ = 0.0,
           xli_ = 0.0, xni_ = 0.0;
};

}  // namespace lodestar::scenario
