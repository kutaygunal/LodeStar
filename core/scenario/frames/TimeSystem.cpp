// core/scenario/frames/TimeSystem.cpp
// Implementation of GPS/UTC/Julian time conversions with a leap-second table.

#include "core/scenario/frames/TimeSystem.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
// GPS epoch: 1980-01-06 00:00:00 UTC.
constexpr double kGpsEpochJd = 2444244.5;
constexpr double kSecondsPerDay = 86400.0;
constexpr double kSecondsPerWeek = 604800.0;
constexpr double kJ2000Jd = 2451545.0;

// Leap-second table: (MJD, TAI-UTC seconds). Sorted ascending by MJD.
// Source: IERS Bulletin C (through 2017-01-01, the last leap second).
constexpr struct { double mjd; double taiUtc; } kLeapTable[] = {
    { 41317.0, 10.0 },  // 1972-01-01
    { 41499.0, 11.0 },  // 1972-07-01
    { 41683.0, 12.0 },  // 1973-01-01
    { 42048.0, 13.0 },  // 1974-01-01
    { 42413.0, 14.0 },  // 1975-01-01
    { 42778.0, 15.0 },  // 1976-01-01
    { 43144.0, 16.0 },  // 1977-01-01
    { 43509.0, 17.0 },  // 1978-01-01
    { 43874.0, 18.0 },  // 1979-01-01
    { 44239.0, 19.0 },  // 1980-01-01
    { 44786.0, 20.0 },  // 1981-07-01
    { 45151.0, 21.0 },  // 1982-07-01
    { 45516.0, 22.0 },  // 1983-07-01
    { 46247.0, 23.0 },  // 1985-07-01
    { 47161.0, 24.0 },  // 1988-01-01
    { 47892.0, 25.0 },  // 1990-01-01
    { 48257.0, 26.0 },  // 1991-01-01
    { 48804.0, 27.0 },  // 1992-07-01
    { 49169.0, 28.0 },  // 1993-07-01
    { 49534.0, 29.0 },  // 1994-07-01
    { 50083.0, 30.0 },  // 1996-01-01
    { 50630.0, 31.0 },  // 1997-07-01
    { 51179.0, 32.0 },  // 1999-01-01
    { 53736.0, 33.0 },  // 2006-01-01
    { 54832.0, 34.0 },  // 2009-01-01
    { 56109.0, 35.0 },  // 2012-07-01
    { 57204.0, 36.0 },  // 2015-07-01
    { 57754.0, 37.0 },  // 2017-01-01
};
constexpr int kLeapTableSize =
    static_cast<int>(sizeof(kLeapTable) / sizeof(kLeapTable[0]));
}  // namespace

double TimeSystem::taiUtcOffsetSec(double mjd) {
    double offset = kLeapTable[0].taiUtc;
    for (int i = 0; i < kLeapTableSize; ++i) {
        if (mjd >= kLeapTable[i].mjd) offset = kLeapTable[i].taiUtc;
    }
    return offset;
}

JulianDate TimeSystem::gpsToJulian(const GpsTime& t) {
    if (t.week < 0 || t.sow < 0.0 || t.sow >= kSecondsPerWeek) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "TimeSystem::gpsToJulian: out-of-range GPS time");
    }
    double jd = kGpsEpochJd + (static_cast<double>(t.week) * kSecondsPerWeek +
                               t.sow) / kSecondsPerDay;
    return JulianDate{jd, jd - 2400000.5};
}

GpsTime TimeSystem::julianToGps(const JulianDate& jd) {
    double days = jd.jd - kGpsEpochJd;
    double totalSec = days * kSecondsPerDay;
    if (totalSec < 0.0) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "TimeSystem::julianToGps: date before GPS epoch");
    }
    int week = static_cast<int>(std::floor(totalSec / kSecondsPerWeek));
    double sow = totalSec - static_cast<double>(week) * kSecondsPerWeek;
    return GpsTime{week, sow};
}

double TimeSystem::gpsToUtcOffsetSec(const GpsTime& t) {
    double mjd = gpsToJulian(t).mjd;
    // GPS-UTC = (TAI-UTC) - 19 (GPS is 19 s behind TAI, constant).
    return taiUtcOffsetSec(mjd) - 19.0;
}

double TimeSystem::julianFromTm(const std::tm& utc) {
    int y = utc.tm_year + 1900;
    int m = utc.tm_mon + 1;
    int d = utc.tm_mday;
    double h = static_cast<double>(utc.tm_hour) +
               static_cast<double>(utc.tm_min) / 60.0 +
               static_cast<double>(utc.tm_sec) / 3600.0;
    if (m <= 2) { y -= 1; m += 12; }
    int a = y / 100;
    int b = 2 - a + a / 4;
    double jd = std::floor(365.25 * (y + 4716)) +
                std::floor(30.6001 * (m + 1)) + d + b - 1524.5 + h / 24.0;
    return jd;
}

GpsTime TimeSystem::utcToGps(const std::tm& utc) {
    double jd = julianFromTm(utc);
    // GPS time = UTC + leap seconds (GPS ahead of UTC).
    double mjd = jd - 2400000.5;
    double gpsOffset = taiUtcOffsetSec(mjd) - 19.0;
    double gpsJd = jd + gpsOffset / kSecondsPerDay;
    return julianToGps(JulianDate{gpsJd, gpsJd - 2400000.5});
}

}  // namespace lodestar::scenario
