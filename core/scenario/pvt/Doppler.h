// core/scenario/pvt/Doppler.h
// Doppler measurement model (Item 4.2).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Doppler {
public:
    // Range rate (m/s): rate of change of geometric range.
    static double rangeRate(const SvState& sv, const Vec3& rxEcef,
                            const Vec3& rxVelEcef);

    // Doppler shift (Hz) for the given carrier frequency (Hz). Defaults to L1.
    static Result<double> compute(const SvState& sv, const Vec3& rxEcef,
                                  const Vec3& rxVelEcef, double rxClockDrift,
                                  double carrierHz = 1575.42e6);
};

}  // namespace lodestar::scenario
