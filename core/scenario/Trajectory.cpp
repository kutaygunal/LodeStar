// core/scenario/Trajectory.cpp
// ScenarioForge trajectory engine implementation (S2 Phase 14).

#include "core/scenario/Trajectory.h"

#include <algorithm>

namespace lodestar::scenario {

namespace {
// Linear interpolation between two waypoints for a given field at time t.
// Returns the interpolated value, or the nearest endpoint if t is outside the
// segment. `a` and `b` are the surrounding waypoints; `t` is the query time.
template <typename Getter>
Vec3 lerpField(const std::vector<Waypoint>& wps, double t, Getter get) {
    if (wps.empty()) return Vec3(0, 0, 0);
    if (wps.size() == 1) return get(wps[0]);

    // Clamp to the trajectory time span.
    if (t <= wps.front().t) return get(wps.front());
    if (t >= wps.back().t) return get(wps.back());

    // Find the segment [i, i+1] containing t.
    std::size_t i = 0;
    while (i + 1 < wps.size() && wps[i + 1].t < t) ++i;
    const Waypoint& a = wps[i];
    const Waypoint& b = wps[i + 1];

    const double span = b.t - a.t;
    const double f = (span > 0.0) ? (t - a.t) / span : 0.0;

    const Vec3 va = get(a);
    const Vec3 vb = get(b);
    return va + (vb - va) * f;
}
}  // namespace

Vec3 Trajectory::positionAt(double t) const {
    return lerpField(waypoints_, t,
                     [](const Waypoint& w) { return w.position; });
}

Vec3 Trajectory::velocityAt(double t) const {
    return lerpField(waypoints_, t,
                     [](const Waypoint& w) { return w.velocity; });
}

Vec3 Trajectory::attitudeAt(double t) const {
    return lerpField(waypoints_, t,
                     [](const Waypoint& w) { return w.attitude; });
}

double Trajectory::startTime() const {
    return waypoints_.empty() ? 0.0 : waypoints_.front().t;
}

double Trajectory::endTime() const {
    return waypoints_.empty() ? 0.0 : waypoints_.back().t;
}

bool Trajectory::empty() const { return waypoints_.empty(); }

Trajectory buildTrajectory(const std::vector<Waypoint>& waypoints) {
    Trajectory traj;
    traj.waypoints_ = waypoints;
    // Ensure waypoints are ordered by time so interpolation is well-defined.
    std::sort(traj.waypoints_.begin(), traj.waypoints_.end(),
              [](const Waypoint& a, const Waypoint& b) { return a.t < b.t; });
    return traj;
}

}  // namespace lodestar::scenario
