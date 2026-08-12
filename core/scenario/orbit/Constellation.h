// core/scenario/orbit/Constellation.h
// Aggregate per-satellite states into a constellation snapshot and compute
// visibility from a ground station (Item 1.4).

#pragma once

#include <map>
#include <vector>

#include "core/scenario/Types.h"
#include "core/scenario/orbit/Keplerian.h"
#include "core/scenario/orbit/Sgp4.h"

namespace lodestar::scenario {

class Constellation {
public:
    // Add a GPS/Galileo satellite from broadcast ephemeris.
    void addGps(const BroadcastEphemeris& eph, int prn);

    // Add a satellite from a TLE (SGP4).
    void addTle(const Tle& tle, int prn);

    // Compute per-satellite views from a receiver ECEF position at GPS time t,
    // applying an elevation mask (radians). Per-satellite failures are
    // collected and reported; they do not abort the whole constellation.
    Result<std::vector<SatelliteView>> computeViews(const Vec3& rxEcef,
                                                    const GpsTime& t,
                                                    double elevationMaskRad) const;

    size_t size() const { return gps_.size() + tle_.size(); }

private:
    struct GpsEntry {
        int prn;
        Keplerian prop;
    };
    struct TleEntry {
        int prn;
        Sgp4 prop;
    };

    std::vector<GpsEntry> gps_;
    std::vector<TleEntry> tle_;
};

}  // namespace lodestar::scenario
