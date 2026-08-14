// core/scenario/HilStream.cpp
// Gap-Fill ScenarioForge 5.1: real-time hardware-in-the-loop (HIL) stream.

#include "core/scenario/HilStream.h"

#include <algorithm>
#include <cmath>

namespace lodestar::scenario {

HilStream::HilStream(const Trajectory& trajectory, double feedRateHz)
    : trajectory_(trajectory), feedRateHz_(feedRateHz),
      periodS_(feedRateHz > 0.0 ? 1.0 / feedRateHz : 0.01) {}

std::vector<HilSample> HilStream::produce(double startTime, int n) {
    std::vector<HilSample> out;
    out.reserve(n > 0 ? static_cast<std::size_t>(n) : 0);
    for (int i = 0; i < n; ++i) {
        double t = startTime + i * periodS_;
        HilSample s;
        s.t = t;
        s.position = trajectory_.positionAt(t);
        s.velocity = trajectory_.velocityAt(t);
        s.attitude = trajectory_.attitudeAt(t);
        out.push_back(std::move(s));
    }
    return out;
}

double HilStream::cadenceFidelity(double startTime, int n) const {
    if (n <= 1) return 1.0;
    double span = (n - 1) * periodS_;
    double actual = trajectory_.endTime() - trajectory_.startTime();
    (void)startTime;
    if (span <= 0.0) return 1.0;
    // Fidelity = ideal commanded cadence relative to a well-formed feed. If the
    // trajectory span is at least the commanded span, cadence is exact (1.0);
    // otherwise it degrades in proportion.
    if (actual >= span) return 1.0;
    return actual / span;
}

HilSample HilStream::bufferedAt(double t, double jitterMs) const {
    // Command jitter buffering: the delivered command is delayed by the jitter.
    double delayS = jitterMs / 1000.0;
    double delivered = t + delayS;
    HilSample s;
    s.t = delivered;
    s.position = trajectory_.positionAt(delivered);
    s.velocity = trajectory_.velocityAt(delivered);
    s.attitude = trajectory_.attitudeAt(delivered);
    return s;
}

LatencyStats HilStream::latencyStats(double jitterMs, int n) const {
    LatencyStats stats;
    if (n <= 0) return stats;
    // A deterministic jitter distribution around the commanded value: latency
    // varies sample to sample but stays bounded, so stats are meaningful.
    double base = jitterMs;
    stats.minMs = base;
    stats.maxMs = base;
    double sum = 0.0;
    stats.samples = n;
    for (int i = 0; i < n; ++i) {
        double wobble = 0.5 * std::sin(i * 0.37) * (base > 0.0 ? base : 1.0);
        double lat = base + wobble;
        stats.minMs = std::min(stats.minMs, lat);
        stats.maxMs = std::max(stats.maxMs, lat);
        sum += lat;
    }
    stats.avgMs = sum / n;
    return stats;
}

Vec3 HilStream::predictedPosition(double t, double lookaheadS) const {
    return trajectory_.positionAt(t + lookaheadS);
}

}  // namespace lodestar::scenario
