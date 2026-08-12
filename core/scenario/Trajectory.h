// core/scenario/Trajectory.h
// ScenarioForge trajectory engine (S2 Phase 14).
//
// Waypoint-based 6-DOF motion for a receiver/vehicle: position, velocity, and
// attitude are interpolated between waypoints over time. This drives the
// receiver/vehicle through a scenario so the baseband reflects real motion.

#pragma once

#include <vector>

#include "core/scenario/Types.h"

namespace lodestar::scenario {

// A single 6-DOF waypoint: time plus position, velocity, and attitude.
struct Waypoint {
    double t = 0.0;      // time (s)
    Vec3 position;      // ECEF position (m)
    Vec3 velocity;      // m/s
    Vec3 attitude;      // Euler angles (rad)
};

// A piecewise-linear trajectory built from waypoints. positionAt / velocityAt /
// attitudeAt interpolate linearly between the surrounding waypoints.
class Trajectory {
public:
    // Interpolated position at time t (m).
    Vec3 positionAt(double t) const;
    // Interpolated velocity at time t (m/s).
    Vec3 velocityAt(double t) const;
    // Interpolated attitude at time t (rad).
    Vec3 attitudeAt(double t) const;

    double startTime() const;
    double endTime() const;
    bool empty() const;

private:
    friend Trajectory buildTrajectory(const std::vector<Waypoint>& waypoints);
    std::vector<Waypoint> waypoints_;
};

// Build a trajectory from an ordered list of waypoints (sorted by time).
Trajectory buildTrajectory(const std::vector<Waypoint>& waypoints);

}  // namespace lodestar::scenario
