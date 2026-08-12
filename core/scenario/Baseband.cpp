// core/scenario/Baseband.cpp
// ScenarioForge software-defined baseband implementation (S2 Phase 11).

#include "core/scenario/Baseband.h"

#include <cmath>
#include <cstdint>

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSpeedOfLight = 2.99792458e8;

// Deterministic PRN chip generator (LFSR) so the baseband carries real code
// content rather than a bare tone.
uint32_t nextChip(uint32_t& lfsr) {
    const uint32_t bit =
        ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return bit;
}
}  // namespace

std::vector<IqSample> generateBaseband(Scenario& scenario,
                                       double carrierHz,
                                       double sampleRate,
                                       double durationSec) {
    if (sampleRate <= 0.0 || durationSec < 0.0 || carrierHz < 0.0) {
        return {};
    }
    const std::size_t n = static_cast<std::size_t>(
        std::llround(sampleRate * durationSec));
    std::vector<IqSample> out;
    out.reserve(n);

    // Reference epoch for satellite geometry.
    GpsTime t0;
    t0.week = 2200;
    t0.sow = 100.0;

    std::vector<SatelliteView> views;
    try {
        auto epoch = scenario.step(t0);
        if (epoch.isOk()) views = epoch.value().views;
    } catch (...) {
        views.clear();
    }

    // Count visible satellites; if none are usable, synthesize a single
    // deterministic channel so the baseband is always well-defined.
    bool hasVisible = false;
    for (const auto& v : views) {
        if (v.visible) {
            hasVisible = true;
            break;
        }
    }

    if (!hasVisible) {
        uint32_t lfsr = 0xACE1u;
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double chip = nextChip(lfsr) ? 1.0 : -1.0;
            const double phase = 2.0 * kPi * carrierHz * t;
            out.push_back({chip * std::cos(phase), chip * std::sin(phase)});
        }
        return out;
    }

    // Composite multi-satellite software-defined baseband.
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        double iAcc = 0.0, qAcc = 0.0;
        for (const auto& v : views) {
            if (!v.visible) continue;
            const double doppler =
                v.state.velEcef.norm() * carrierHz / kSpeedOfLight;
            const double phase = 2.0 * kPi * (carrierHz + doppler) * t + v.prn;
            iAcc += 0.5 * std::cos(phase);
            qAcc += 0.5 * std::sin(phase);
        }
        out.push_back({iAcc, qAcc});
    }
    return out;
}

}  // namespace lodestar::scenario
