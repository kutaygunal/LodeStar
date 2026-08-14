// core/scenario/AGnssAssistance.cpp
// Gap-Fill ScenarioForge 5.3: A-GNSS assistance-data generation.

#include "core/scenario/AGnssAssistance.h"

#include <cstdio>
#include <sstream>

namespace lodestar::scenario {

AGnssAssistance::AGnssAssistance(const std::vector<AssistedSatellite>& sats)
    : sats_(sats) {}

std::string AGnssAssistance::generateAlmanac() const {
    std::ostringstream o;
    o << "     2  A-GNSS ALMANAC FILE                    RINEX 3\n";
    for (const auto& s : sats_) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "%2d %-8s %14.4f %14.4f %14.4f\n",
                      s.prn, s.constellation.c_str(), s.ecefX, s.ecefY, s.ecefZ);
        o << buf;
    }
    return o.str();
}

std::string AGnssAssistance::generateNavigation() const {
    std::ostringstream o;
    o << "     2  A-GNSS NAVIGATION FILE                 RINEX 3\n";
    for (const auto& s : sats_) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "%2d %-8s %14.4f %14.4f\n",
                      s.prn, s.constellation.c_str(), s.clockBias, s.clockDrift);
        o << buf;
    }
    return o.str();
}

std::string AGnssAssistance::generateAcquisition() const {
    std::ostringstream o;
    o << "A-GNSS ACQUISITION ASSISTANCE\n";
    for (const auto& s : sats_) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "PRN %2d %-8s POS %.4f %.4f %.4f\n",
                      s.prn, s.constellation.c_str(), s.ecefX, s.ecefY, s.ecefZ);
        o << buf;
    }
    return o.str();
}

int AGnssAssistance::validateAlmanac(const std::string& almanac) const {
    std::istringstream ss(almanac);
    std::string line;
    int parsed = 0;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        // Skip the RINEX header line (contains "RINEX").
        if (line.find("RINEX") != std::string::npos) continue;
        ++parsed;
    }
    return parsed;
}

}  // namespace lodestar::scenario
