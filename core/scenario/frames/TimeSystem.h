// core/scenario/frames/TimeSystem.h
// GPS time, UTC, Julian Date, Modified Julian Date, leap seconds (Item 1.1).

#pragma once

#include <ctime>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

// Single source of truth for time systems used across ScenarioForge.
class TimeSystem {
public:
    // GPS <-> Julian Date. GPS epoch is 1980-01-06 00:00:00 UTC.
    static JulianDate gpsToJulian(const GpsTime& t);
    static GpsTime julianToGps(const JulianDate& jd);

    // GPS - UTC offset in seconds (leap seconds). GPS is ahead of UTC.
    static double gpsToUtcOffsetSec(const GpsTime& t);

    // UTC (broken-down) -> GPS time.
    static GpsTime utcToGps(const std::tm& utc);

    // Julian Date from a broken-down UTC time.
    static double julianFromTm(const std::tm& utc);

    // Leap-second table: (MJD, TAI-UTC seconds). GPS-UTC = (TAI-UTC) - 19.
    static double taiUtcOffsetSec(double mjd);
};

}  // namespace lodestar::scenario
