// core/scenario/frames/Frames.h
// ECEF <-> ECI rotation (GMST), WGS-84 datum, geodetic <-> ECEF (Item 1.1).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Frames {
public:
    // WGS-84 datum constants (sourced from NGA WGS-84).
    static constexpr double WGS84_A = 6378137.0;              // semi-major axis, m
    static constexpr double WGS84_F = 1.0 / 298.257223563;   // flattening
    static constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F); // first ecc^2
    static constexpr double GM = 3.986004418e14;              // Earth grav const, m^3/s^2
    static constexpr double OMEGA_E = 7.2921151467e-5;        // Earth rotation, rad/s
    static constexpr double C = 299792458.0;                  // speed of light, m/s

    // GMST (radians) from a Julian Date (IAU-1982 formula).
    static double gmstRad(const JulianDate& jd);

    // Rotation matrix ECEF = R * ECI (Rz(GMST)).
    static Mat3 ecefToEciRotation(const JulianDate& jd);

    static Vec3 ecefToEci(const Vec3& ecef, const JulianDate& jd);
    static Vec3 eciToEcef(const Vec3& eci, const JulianDate& jd);

    // WGS-84 geodetic <-> ECEF. lat/lon in radians, h in meters.
    static Vec3 geodeticToEcef(double lat, double lon, double h);
    static void ecefToGeodetic(const Vec3& ecef, double& lat, double& lon, double& h);
};

}  // namespace lodestar::scenario
