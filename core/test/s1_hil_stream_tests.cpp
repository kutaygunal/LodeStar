// core/test/s1_hil_stream_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.1: real-time HIL stream tests.
//
// Test contract: docs/gap-fill-plan.md (Module 5.1).
//   (A) core/scenario/HilStream.h (+ .cpp): a first-party 100 Hz HIL
//       position/attitude feed over UDP/SCPI-style generation with a
//       latency-control loop (latency measurement, jitter buffering,
//       trajectory prediction).
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <vector>

#include "core/scenario/HilStream.h"
#include "core/scenario/Trajectory.h"
#include "core/scenario/Types.h"

namespace s = lodestar::scenario;

namespace {

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

s::Trajectory makeTrajectory() {
    std::vector<s::Waypoint> wp;
    s::Waypoint a;
    a.t = 0.0;
    a.position = s::Vec3(0, 0, 0);
    a.velocity = s::Vec3(10, 0, 0);
    s::Waypoint b;
    b.t = 1.0;
    b.position = s::Vec3(10, 0, 0);
    b.velocity = s::Vec3(10, 0, 0);
    wp.push_back(a);
    wp.push_back(b);
    return s::buildTrajectory(wp);
}

}  // namespace

// ---------------------------------------------------------------------------
// T1. Feed rate: 100 Hz over 1 second -> ~100 samples
// ---------------------------------------------------------------------------
static void testFeedRate(Harness& h) {
    h.section("T1. 100 Hz feed rate");
    s::Trajectory traj = makeTrajectory();
    s::HilStream stream(traj, 100.0);

    auto samples = stream.produce(0.0, 100);
    h.check(samples.size() == 100, "produces 100 samples at 100 Hz over 1 s");
    if (samples.size() == 100) {
        // Sample cadence is 10 ms.
        h.check(samples[1].t - samples[0].t > 0.009 && samples[1].t - samples[0].t < 0.011,
                "sample cadence ~10 ms (100 Hz)");
    }
    h.check(stream.cadenceFidelity(0.0, 100) == 1.0,
            "cadence fidelity is exact (trajectory covers the span)");
}

// ---------------------------------------------------------------------------
// T2. Interpolation of position/velocity/attitude
// ---------------------------------------------------------------------------
static void testInterpolation(Harness& h) {
    h.section("T2. trajectory interpolation");
    s::Trajectory traj = makeTrajectory();
    s::HilStream stream(traj, 100.0);

    auto samples = stream.produce(0.5, 1);
    h.check(samples.size() == 1, "produces 1 sample");
    if (samples.size() == 1) {
        // At t=0.5, position interpolates to x=5.
        h.check(samples[0].position.x > 4.9 && samples[0].position.x < 5.1,
                "position interpolated at t=0.5 (~x=5)");
        h.check(samples[0].velocity.x > 9.9 && samples[0].velocity.x < 10.1,
                "velocity interpolated (~10 m/s)");
    }
}

// ---------------------------------------------------------------------------
// T3. Latency statistics + jitter buffering
// ---------------------------------------------------------------------------
static void testLatency(Harness& h) {
    h.section("T3. latency stats + jitter buffering");
    s::Trajectory traj = makeTrajectory();
    s::HilStream stream(traj, 100.0);

    auto stats = stream.latencyStats(5.0, 100);
    h.check(stats.samples == 100, "latency stats over 100 samples");
    h.check(stats.avgMs > 0.0, "average latency is positive");
    h.check(stats.maxMs >= stats.minMs, "max latency >= min latency");

    // Jitter buffering shifts the delivered sample by the jitter delay.
    auto buffered = stream.bufferedAt(0.5, 10.0);  // 10 ms jitter
    h.check(buffered.t >= 0.51,
            "jitter-buffered sample delivered at/after the 10 ms delay");

    // Prediction looks ahead.
    auto pred = stream.predictedPosition(0.5, 0.1);
    h.check(pred.x > 5.0, "predicted position ahead of commanded time");
}

// ---------------------------------------------------------------------------
// T4. Determinism
// ---------------------------------------------------------------------------
static void testDeterminism(Harness& h) {
    h.section("T4. determinism");
    s::Trajectory traj = makeTrajectory();
    s::HilStream s1(traj, 100.0), s2(traj, 100.0);
    auto a = s1.produce(0.0, 10);
    auto b = s2.produce(0.0, 10);
    h.check(a.size() == b.size(), "same size");
    bool same = true;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i].position.x != b[i].position.x) same = false;
    }
    h.check(same, "identical outputs (deterministic)");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill ScenarioForge 5.1 HIL stream");
    testFeedRate(h);
    testInterpolation(h);
    testLatency(h);
    testDeterminism(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
