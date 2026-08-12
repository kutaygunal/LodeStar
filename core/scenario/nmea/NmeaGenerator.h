// core/scenario/nmea/NmeaGenerator.h
// High-level NMEA sentence emitters (GGA/RMC/GSA/GSV/ZDA) from PVT (Item 3.2).

#pragma once

#include <string>
#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class NmeaGenerator {
public:
    explicit NmeaGenerator(const NmeaConfig& cfg);

    std::string gga(const PvtResult& pvt, const std::vector<SatelliteView>& views) const;
    std::string rmc(const PvtResult& pvt, const GpsTime& t) const;
    std::string gsa(const PvtResult& pvt, const std::vector<SatelliteView>& views) const;
    std::string gsv(const std::vector<SatelliteView>& views, int msgPerSentence = 4) const;
    std::string zda(const GpsTime& t) const;
    // Concatenate all five sentence types for an epoch.
    std::string stream(const PvtResult& pvt, const std::vector<SatelliteView>& views,
                       const GpsTime& t) const;

private:
    NmeaConfig cfg_;
};

}  // namespace lodestar::scenario
