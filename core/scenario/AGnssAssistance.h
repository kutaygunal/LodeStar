// core/scenario/AGnssAssistance.h
// Gap-Fill ScenarioForge 5.3: A-GNSS assistance-data generation.
//
// Generates almanac / navigation / acquisition assistance files for TTFF
// (time-to-first-fix) testing, and validates that the files parse and match
// the simulated constellation.

#pragma once

#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

// One assisted satellite entry.
struct AssistedSatellite {
    int prn = 0;                 // satellite PRN
    std::string constellation;   // GPS | GLONASS | BeiDou | Galileo
    double ecefX = 0.0;          // approximate position (m)
    double ecefY = 0.0;
    double ecefZ = 0.0;
    double clockBias = 0.0;      // s
    double clockDrift = 0.0;     // s/s
};

// A-GNSS assistance data set (almanac + nav + acquisition).
class AGnssAssistance {
public:
    explicit AGnssAssistance(const std::vector<AssistedSatellite>& sats);

    // Generate the almanac file (RINEX-like text). Deterministic.
    std::string generateAlmanac() const;

    // Generate the navigation (ephemeris) file. Deterministic.
    std::string generateNavigation() const;

    // Generate the acquisition-assistance (position/clock) file. Deterministic.
    std::string generateAcquisition() const;

    // Parse a generated file back and validate every satellite present in the
    // source data is represented. Returns the count parsed.
    int validateAlmanac(const std::string& almanac) const;

    // The satellite list.
    const std::vector<AssistedSatellite>& satellites() const { return sats_; }

private:
    std::vector<AssistedSatellite> sats_;
};

}  // namespace lodestar::scenario
