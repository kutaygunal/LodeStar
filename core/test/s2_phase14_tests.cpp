// core/test/s2_phase14_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 14 tests (test contract): ScenarioForge trajectory (waypoints /
// 6-DOF) + multipath / interference (RF impairments).
//
// Written by the scrum-master BEFORE the Phase 14 engineer implements the
// feature. The engineer must implement the contract below so these tests
// compile and pass. Do NOT weaken the assertions; implement the feature to
// satisfy them.
//
// Covers (PLAN.md, S2 Phase 14):
//   (A) Trajectory engine: buildTrajectory(waypoints) -> trajectory;
//       positionAt(t) returns the interpolated position (and optionally
//       velocity/attitude) at time t.
//   (B) RF impairments: applyMultipath(samples, delay, gain) adds a delayed,
//       attenuated copy; applyInterference(samples, amplitude) adds additive
//       interference.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 14 engineer must provide.
// ---------------------------------------------------------------------------
// (A) core/scenario/Trajectory.h:
//   struct Waypoint {
//       double t;        // time (s)
//       Vec3 position;   // ECEF position (m)
//       Vec3 velocity;   // m/s
//       Vec3 attitude;   // Euler angles (rad)
//   };
//   class Trajectory {
//   public:
//       Vec3 positionAt(double t) const;   // interpolated position at t
//       Vec3 velocityAt(double t) const;   // interpolated velocity at t
//       Vec3 attitudeAt(double t) const;   // interpolated attitude at t
//       double startTime() const;
//       double endTime() const;
//       bool empty() const;
//   };
//   Trajectory buildTrajectory(const std::vector<Waypoint>& waypoints);
//
// (B) core/scenario/RfImpairments.h:
//   std::vector<IqSample> applyMultipath(
//       const std::vector<IqSample>& samples, int delay, double gain);
//   std::vector<IqSample> applyInterference(
//       const std::vector<IqSample>& samples, double amplitude);
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <vector>

#include "core/scenario/Baseband.h"
#include "core/scenario/RfImpairments.h"
#include "core/scenario/Trajectory.h"
#include "core/scenario/Types.h"

using lodestar::scenario::applyInterference;
using lodestar::scenario::applyMultipath;
using lodestar::scenario::buildTrajectory;
using lodestar::scenario::IqSample;
using lodestar::scenario::Trajectory;
using lodestar::scenario::Vec3;
using lodestar::scenario::Waypoint;

namespace {

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// ---------------------------------------------------------------------------
// T1. trajectory interpolates between waypoints
// ---------------------------------------------------------------------------
void testTrajectoryInterpolates(Harness& h) {
    h.section("T1. trajectory interpolates between waypoints");

    std::vector<Waypoint> wps;
    wps.push_back({0.0, Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 0, 0)});
    wps.push_back({10.0, Vec3(100, 0, 0), Vec3(1, 0, 0), Vec3(0, 0, 0)});

    Trajectory traj = buildTrajectory(wps);
    h.check(!traj.empty(), "trajectory is non-empty");

    Vec3 mid = traj.positionAt(5.0);
    h.check(mid.x > 0.0 && mid.x < 100.0,
            "positionAt(5) x is strictly between the two endpoints");
    h.check(mid.y == 0.0 && mid.z == 0.0,
            "positionAt(5) y/z match the straight-line path");
    h.check(mid.x != 0.0 && mid.x != 100.0,
            "positionAt(5) is not equal to either endpoint");
}

// ---------------------------------------------------------------------------
// T2. trajectory respects waypoint endpoints
// ---------------------------------------------------------------------------
void testTrajectoryEndpoints(Harness& h) {
    h.section("T2. trajectory respects waypoint endpoints");

    std::vector<Waypoint> wps;
    wps.push_back({0.0, Vec3(10, 20, 30), Vec3(0, 0, 0), Vec3(0.1, 0.2, 0.3)});
    wps.push_back({10.0, Vec3(50, 60, 70), Vec3(0, 0, 0), Vec3(0.4, 0.5, 0.6)});

    Trajectory traj = buildTrajectory(wps);

    Vec3 p0 = traj.positionAt(0.0);
    h.check(p0.x == 10.0 && p0.y == 20.0 && p0.z == 30.0,
            "positionAt(0) equals the first waypoint position");

    Vec3 p1 = traj.positionAt(10.0);
    h.check(p1.x == 50.0 && p1.y == 60.0 && p1.z == 70.0,
            "positionAt(10) equals the second waypoint position");

    Vec3 a0 = traj.attitudeAt(0.0);
    h.check(a0.x == 0.1 && a0.y == 0.2 && a0.z == 0.3,
            "attitudeAt(0) equals the first waypoint attitude");

    Vec3 a1 = traj.attitudeAt(10.0);
    h.check(a1.x == 0.4 && a1.y == 0.5 && a1.z == 0.6,
            "attitudeAt(10) equals the second waypoint attitude");
}

// ---------------------------------------------------------------------------
// T3. multipath adds a delayed copy
// ---------------------------------------------------------------------------
void testMultipath(Harness& h) {
    h.section("T3. multipath adds a delayed copy");

    std::vector<IqSample> samples;
    for (int i = 0; i < 64; ++i) {
        samples.push_back({std::cos(0.1 * i), std::sin(0.1 * i)});
    }

    auto out = applyMultipath(samples, 4, 0.5);
    h.check(out.size() == samples.size(),
            "applyMultipath preserves the sample count");

    bool differs = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i].i != samples[i].i || out[i].q != samples[i].q) {
            differs = true;
            break;
        }
    }
    h.check(differs, "multipath output differs from the input (delayed copy added)");

    // The delayed copy must be attenuated by `gain` and shifted by `delay`.
    // out[i] = samples[i] + gain * samples[i - delay] (for i >= delay).
    const int delay = 4;
    const double gain = 0.5;
    bool delayedCopyCorrect = true;
    for (std::size_t i = static_cast<std::size_t>(delay); i < out.size(); ++i) {
        const double expI = samples[i].i + gain * samples[i - delay].i;
        const double expQ = samples[i].q + gain * samples[i - delay].q;
        if (std::fabs(out[i].i - expI) > 1e-9 ||
            std::fabs(out[i].q - expQ) > 1e-9) {
            delayedCopyCorrect = false;
            break;
        }
    }
    h.check(delayedCopyCorrect,
            "multipath adds an attenuated, delayed copy of the signal");
}

// ---------------------------------------------------------------------------
// T4. interference adds signal
// ---------------------------------------------------------------------------
void testInterference(Harness& h) {
    h.section("T4. interference adds signal");

    std::vector<IqSample> samples;
    for (int i = 0; i < 64; ++i) {
        samples.push_back({std::cos(0.1 * i), std::sin(0.1 * i)});
    }

    auto out = applyInterference(samples, 0.3);
    h.check(out.size() == samples.size(),
            "applyInterference preserves the sample count");

    bool differs = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i].i != samples[i].i || out[i].q != samples[i].q) {
            differs = true;
            break;
        }
    }
    h.check(differs, "interference output differs from the input (signal added)");

    // The interference must be additive: out[i] = samples[i] + interference.
    bool additive = true;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i].i == samples[i].i && out[i].q == samples[i].q) {
            additive = false;
            break;
        }
    }
    h.check(additive, "interference is added to every sample");
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    Harness h("S2 Phase 14 ScenarioForge trajectory + multipath/interference");
    std::printf("S2 PHASE 14 TESTS\n");

    testTrajectoryInterpolates(h);
    testTrajectoryEndpoints(h);
    testMultipath(h);
    testInterference(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
