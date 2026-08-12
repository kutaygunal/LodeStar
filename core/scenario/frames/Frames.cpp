// core/scenario/frames/Frames.cpp
// WGS-84 datum, GMST, ECEF<->ECI, geodetic<->ECEF (Item 1.1).

#include "core/scenario/frames/Frames.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
constexpr double kJ2000Jd = 2451545.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;
}  // namespace

double Frames::gmstRad(const JulianDate& jd) {
    // IAU-1982 GMST formula. T = Julian centuries of UT1 from J2000.
    double t = (jd.jd - kJ2000Jd) / 36525.0;
    // GMST in seconds.
    double gmstSec = 67310.54841 +
                     (876600.0 * 3600.0 + 8640184.812866) * t +
                     0.093104 * t * t -
                     6.2e-6 * t * t * t;
    double gmst = std::fmod(gmstSec * kTwoPi / 86400.0, kTwoPi);
    if (gmst < 0.0) gmst += kTwoPi;
    return gmst;
}

Mat3 Frames::ecefToEciRotation(const JulianDate& jd) {
    return Mat3::rotZ(gmstRad(jd));
}

Vec3 Frames::ecefToEci(const Vec3& ecef, const JulianDate& jd) {
    // ECI = Rz(-GMST) * ECEF.
    return Mat3::rotZ(-gmstRad(jd)) * ecef;
}

Vec3 Frames::eciToEcef(const Vec3& eci, const JulianDate& jd) {
    // ECEF = Rz(GMST) * ECI.
    return Mat3::rotZ(gmstRad(jd)) * eci;
}

Vec3 Frames::geodeticToEcef(double lat, double lon, double h) {
    if (lat < -1.5707963267948966 || lat > 1.5707963267948966) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Frames::geodeticToEcef: latitude out of range");
    }
    double sinLat = std::sin(lat), cosLat = std::cos(lat);
    double sinLon = std::sin(lon), cosLon = std::cos(lon);
    double n = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
    double x = (n + h) * cosLat * cosLon;
    double y = (n + h) * cosLat * sinLon;
    double z = (n * (1.0 - WGS84_E2) + h) * sinLat;
    return Vec3(x, y, z);
}

void Frames::ecefToGeodetic(const Vec3& ecef, double& lat, double& lon, double& h) {
    if (!ecef.isFinite()) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Frames::ecefToGeodetic: non-finite ECEF input");
    }
    double x = ecef.x, y = ecef.y, z = ecef.z;
    double p = std::sqrt(x * x + y * y);
    lon = std::atan2(y, x);

    // Bowring's closed-form inverse.
    double b = WGS84_A * (1.0 - WGS84_F);
    double ep2 = (WGS84_A * WGS84_A - b * b) / (b * b);
    double theta = std::atan2(z * WGS84_A, p * b);
    double sinT = std::sin(theta), cosT = std::cos(theta);
    lat = std::atan2(z + ep2 * b * sinT * sinT * sinT,
                     p - WGS84_E2 * WGS84_A * cosT * cosT * cosT);
    double sinLat = std::sin(lat);
    double n = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
    h = (p / std::cos(lat)) - n;
    if (std::fabs(lat) > 1.5707963267948966) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Frames::ecefToGeodetic: latitude out of range");
    }
}

}  // namespace lodestar::scenario
