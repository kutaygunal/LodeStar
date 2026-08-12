// core/scenario/orbit/Keplerian.h
// Two-body Keplerian propagator for GPS/Galileo broadcast ephemeris (Item 1.2).
// Implements IS-GPS-200 Section 20.3.3.4.3.

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Keplerian {
public:
    explicit Keplerian(const BroadcastEphemeris& eph);

    // Propagate to GPS time t (seconds of week). Returns ECEF position/velocity
    // and clock bias/drift. Returns an error for invalid elements or
    // non-convergence of Kepler's equation.
    Result<SvState> propagate(double t) const;

    const BroadcastEphemeris& ephemeris() const { return eph_; }

private:
    BroadcastEphemeris eph_;
};

}  // namespace lodestar::scenario
