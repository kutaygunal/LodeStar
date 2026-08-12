// core/scenario/RfImpairments.cpp
// ScenarioForge RF impairments implementation (S2 Phase 14).

#include "core/scenario/RfImpairments.h"

#include <cmath>
#include <cstdint>

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Deterministic pseudo-random generator so the interference is reproducible
// (same input -> same output) while still being non-trivial.
uint32_t nextRand(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}
}  // namespace

std::vector<IqSample> applyMultipath(const std::vector<IqSample>& samples,
                                     int delay,
                                     double gain) {
    if (samples.empty()) return {};
    std::vector<IqSample> out = samples;
    if (delay <= 0 || gain == 0.0) return out;

    const std::size_t d = static_cast<std::size_t>(delay);
    for (std::size_t i = d; i < out.size(); ++i) {
        out[i].i += gain * samples[i - d].i;
        out[i].q += gain * samples[i - d].q;
    }
    return out;
}

std::vector<IqSample> applyInterference(const std::vector<IqSample>& samples,
                                        double amplitude) {
    if (samples.empty()) return {};
    std::vector<IqSample> out = samples;
    if (amplitude == 0.0) return out;

    uint32_t state = 0x5EEDu;
    for (std::size_t i = 0; i < out.size(); ++i) {
        // Deterministic additive noise/jammer on both I and Q.
        const double ni = (static_cast<double>(nextRand(state)) / 4294967295.0) * 2.0 - 1.0;
        const double nq = (static_cast<double>(nextRand(state)) / 4294967295.0) * 2.0 - 1.0;
        out[i].i += amplitude * ni;
        out[i].q += amplitude * nq;
    }
    return out;
}

}  // namespace lodestar::scenario
