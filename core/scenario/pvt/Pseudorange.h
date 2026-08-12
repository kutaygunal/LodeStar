// core/scenario/pvt/Pseudorange.h
// Pseudorange measurement model (Item 4.1).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Pseudorange {
public:
    // Compute measured pseudorange including clock + atmospheric + relativistic
    // corrections.
    static Result<double> compute(const SvState& sv, const Vec3& rxEcef,
                                  double rxClockBias,
                                  const AtmosphericCorrections& atm);

    // Geometric range (meters) with Sagnac/Earth-rotation correction.
    static double geometricRange(const SvState& sv, const Vec3& rxEcef);

    // Relativistic correction (meters) per IS-GPS-200. Takes the already
    // computed satellite clock bias which includes the relativistic term.
    static double relativisticCorrection(double svClockBiasSec);
};

}  // namespace lodestar::scenario
