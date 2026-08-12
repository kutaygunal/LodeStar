// core/scenario/frames/Geometry.h
// Ground-station/satellite geometry: elevation, azimuth, slant range, ENU (Item 1.1).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class Geometry {
public:
    // Elevation angle (radians) of a satellite as seen from a receiver.
    static double elevationRad(const Vec3& rxEcef, const Vec3& svEcef);
    // Azimuth (radians, clockwise from north).
    static double azimuthRad(const Vec3& rxEcef, const Vec3& svEcef);
    // Slant range (meters).
    static double slantRange(const Vec3& rxEcef, const Vec3& svEcef);
    // Topocentric ENU (east, north, up) of the satellite relative to receiver.
    static Vec3 topocentricEnu(const Vec3& rxEcef, const Vec3& svEcef);
};

}  // namespace lodestar::scenario
