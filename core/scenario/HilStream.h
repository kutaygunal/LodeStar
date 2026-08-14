// core/scenario/HilStream.h
// Gap-Fill ScenarioForge 5.1: real-time hardware-in-the-loop (HIL) stream.
//
// A first-party software HIL position/attitude feed modeled after the R&S
// SMW-K109 behavior, with a latency-control loop: latency measurement, command
// jitter buffering, and trajectory prediction. Vendor adapters remain the
// alternative RF path; this adds a software-first feed. (Real RF emission stays
// adapter-driven; this is the software feed/positioning slice.)

#pragma once

#include <vector>

#include "core/scenario/Trajectory.h"
#include "core/scenario/Types.h"

namespace lodestar::scenario {

// A single HIL feed sample at 100 Hz.
struct HilSample {
    double t = 0.0;      // time (s)
    Vec3 position;       // ECEF (m)
    Vec3 velocity;       // m/s
    Vec3 attitude;       // rad
};

// Latency statistics for the feed (deterministic, unit-tested).
struct LatencyStats {
    double minMs = 0.0;
    double maxMs = 0.0;
    double avgMs = 0.0;
    int samples = 0;
};

// A 100 Hz HIL feed driven by a trajectory. Produces position/attitude samples
// at a fixed rate with a latency-control loop and jitter buffering.
class HilStream {
public:
    // feedRateHz is the commanded sample rate (default 100).
    explicit HilStream(const Trajectory& trajectory, double feedRateHz = 100.0);

    // Produce the next `n` feed samples starting at `startTime` (seconds), at
    // the configured rate. Uses trajectory prediction / interpolation.
    std::vector<HilSample> produce(double startTime, int n);

    // Feed-rate fidelity: how close the delivered cadence is to the commanded
    // rate (1.0 = exact). Deterministic from the requested count and duration.
    double cadenceFidelity(double startTime, int n) const;

    // Jitter buffering: simulates command jitter and returns a latency-corrected
    // sample for a commanded time. `jitterMs` is the injected command delay.
    HilSample bufferedAt(double t, double jitterMs) const;

    // Latency statistics over the last produce() call (command jitter, ms).
    LatencyStats latencyStats(double jitterMs, int n) const;

    // Predicted position ahead of a commanded time (trajectory prediction).
    Vec3 predictedPosition(double t, double lookaheadS) const;

private:
    const Trajectory& trajectory_;
    double feedRateHz_;
    double periodS_;
};

}  // namespace lodestar::scenario
