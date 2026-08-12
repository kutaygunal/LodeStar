// core/scenario/errors/Ionosphere.h
// Klobuchar ionospheric delay model (Item 5.2).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Ionosphere {
public:
    struct KlobucharParams {
        double alpha[4] = {0.0, 0.0, 0.0, 0.0};
        double beta[4] = {0.0, 0.0, 0.0, 0.0};
    };

    void setKlobuchar(const KlobucharParams& p);

    // Ionospheric delay (meters) along the path from receiver to satellite.
    Result<double> delay(const SvState& sv, const Vec3& rxEcef,
                         const GpsTime& t) const;

private:
    KlobucharParams params_;
    bool configured_ = false;
};

}  // namespace lodestar::scenario
