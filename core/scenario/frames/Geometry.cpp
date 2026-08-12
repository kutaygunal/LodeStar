// core/scenario/frames/Geometry.cpp
// Elevation/azimuth/slant-range/ENU geometry (Item 1.1).

#include "core/scenario/frames/Geometry.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::scenario {

Vec3 Geometry::topocentricEnu(const Vec3& rxEcef, const Vec3& svEcef) {
    if (!rxEcef.isFinite() || !svEcef.isFinite()) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Geometry::topocentricEnu: non-finite input");
    }
    double lat, lon, h;
    Frames::ecefToGeodetic(rxEcef, lat, lon, h);
    double slat = std::sin(lat), clat = std::cos(lat);
    double slon = std::sin(lon), clon = std::cos(lon);

    Vec3 d = svEcef - rxEcef;
    // Rotation from ECEF to ENU.
    double e = -slon * d.x + clon * d.y;
    double n = -slat * clon * d.x - slat * slon * d.y + clat * d.z;
    double u = clat * clon * d.x + clat * slon * d.y + slat * d.z;
    return Vec3(e, n, u);
}

double Geometry::elevationRad(const Vec3& rxEcef, const Vec3& svEcef) {
    Vec3 enu = topocentricEnu(rxEcef, svEcef);
    double horiz = std::sqrt(enu.x * enu.x + enu.y * enu.y);
    return std::atan2(enu.z, horiz);
}

double Geometry::azimuthRad(const Vec3& rxEcef, const Vec3& svEcef) {
    Vec3 enu = topocentricEnu(rxEcef, svEcef);
    double az = std::atan2(enu.x, enu.y);  // east / north
    if (az < 0.0) az += 2.0 * 3.14159265358979323846;
    return az;
}

double Geometry::slantRange(const Vec3& rxEcef, const Vec3& svEcef) {
    return (svEcef - rxEcef).norm();
}

}  // namespace lodestar::scenario
