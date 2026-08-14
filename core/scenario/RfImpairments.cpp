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

// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.4: advanced interference & environment models.
// ---------------------------------------------------------------------------

std::vector<IqSample> applyInterferenceSource(
    const std::vector<IqSample>& samples, const InterferenceSource& src) {
    if (samples.empty() || src.amplitude == 0.0) return samples;
    std::vector<IqSample> out = samples;

    for (std::size_t i = 0; i < out.size(); ++i) {
        if ((int)i < src.startSample || (int)i > src.endSample) continue;
        const double t = static_cast<double>(i);

        switch (src.kind) {
            case InterferenceKind::MatchedSpectrum: {
                // Re-modulate the signal onto itself (scaled) -> spectral match.
                out[i].i += src.amplitude * samples[i].i;
                out[i].q += src.amplitude * samples[i].q;
                break;
            }
            case InterferenceKind::ContinuousWave: {
                const double phase = 2.0 * kPi * src.frequencyHz * t;
                out[i].i += src.amplitude * std::cos(phase);
                out[i].q += src.amplitude * std::sin(phase);
                break;
            }
            case InterferenceKind::Awgn: {
                uint32_t st = 0xBEEF + static_cast<uint32_t>(i);
                const double ni = (static_cast<double>(nextRand(st)) / 4294967295.0) * 2.0 - 1.0;
                const double nq = (static_cast<double>(nextRand(st)) / 4294967295.0) * 2.0 - 1.0;
                out[i].i += src.amplitude * ni;
                out[i].q += src.amplitude * nq;
                break;
            }
            case InterferenceKind::Jamming: {
                // High-power broadband denial.
                uint32_t st = 0xABCD + static_cast<uint32_t>(i);
                const double nj = (static_cast<double>(nextRand(st)) / 4294967295.0) * 2.0 - 1.0;
                out[i].i += src.amplitude * 3.0 * nj;
                out[i].q += src.amplitude * 3.0 * nj;
                break;
            }
            case InterferenceKind::Spoofing: {
                // A scaled, phase-delayed copy of the true signal (false signal).
                const std::size_t d = 5;
                const std::size_t srcIdx = i >= d ? i - d : i;
                out[i].i += src.amplitude * samples[srcIdx].i;
                out[i].q += src.amplitude * samples[srcIdx].q;
                break;
            }
        }
    }
    return out;
}

std::vector<IqSample> applyEnvironment(
    const std::vector<IqSample>& samples,
    const std::vector<ReflectionPath>& paths) {
    std::vector<IqSample> out = samples;
    for (const auto& p : paths) {
        out = applyMultipath(out, p.delaySamples, p.gain);
    }
    return out;
}

std::vector<ReflectionPath> urbanCanyonModel(double groundGain) {
    // Urban canyon: a ground reflection plus nearby building reflections.
    std::vector<ReflectionPath> p;
    p.push_back({8, groundGain});       // ground bounce
    p.push_back({20, groundGain * 0.6}); // building wall
    p.push_back({35, groundGain * 0.3}); // far building
    return p;
}

std::vector<ReflectionPath> groundReflectionModel(double gain) {
    return { {6, gain} };
}

}  // namespace lodestar::scenario
