// core/scenario/sbas/Integrity.h
// SBAS integrity: UDRE/GIVE aggregation and protection levels HPL/VPL (Item 6.4).

#pragma once

#include <map>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Integrity {
public:
    void setUdre(int prn, double udre);  // m
    void setGive(double give);           // m (user ionospheric error)

    // Compute HPL/VPL from the satellite geometry and per-PRN UDREs using a
    // weighted least-squares geometry matrix. Returns an error for insufficient
    // satellites or a singular geometry matrix.
    Result<ProtectionLevels> compute(const std::vector<SatelliteView>& views,
                                     const std::vector<double>& udrePerPrn,
                                     double give) const;

private:
    std::map<int, double> udre_;
    double give_ = 0.0;
    bool haveGive_ = false;
};

}  // namespace lodestar::scenario
