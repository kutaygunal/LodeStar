// core/scenario/orbit/Tle.h
// Two-Line Element set parsing (Item 1.3).

#pragma once

#include <string>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Tle {
public:
    // Parse a TLE from a name line + two element lines. Validates checksums
    // and field ranges. Returns an error for malformed input.
    static Result<Tle> parse(const std::string& name,
                             const std::string& line1,
                             const std::string& line2);

    double epochJulian() const { return epochJd_; }
    double inclinationRad() const { return inclRad_; }
    double raanRad() const { return raanRad_; }
    double eccentricity() const { return ecc_; }
    double argPerigeeRad() const { return argpRad_; }
    double meanAnomalyRad() const { return meanAnomalyRad_; }
    double meanMotionRevPerDay() const { return meanMotion_; }
    double bstar() const { return bstar_; }
    int catalogNumber() const { return catNum_; }
    int epochYear() const { return epochYear_; }
    double epochDay() const { return epochDay_; }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    int catNum_ = 0;
    int epochYear_ = 0;
    double epochDay_ = 0.0;
    double inclRad_ = 0.0;
    double raanRad_ = 0.0;
    double ecc_ = 0.0;
    double argpRad_ = 0.0;
    double meanAnomalyRad_ = 0.0;
    double meanMotion_ = 0.0;  // rev/day
    double bstar_ = 0.0;
    double epochJd_ = 0.0;
};

}  // namespace lodestar::scenario
