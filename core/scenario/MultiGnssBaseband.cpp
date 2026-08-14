// core/scenario/MultiGnssBaseband.cpp
// Gap-Fill ScenarioForge 5.5: first-party baseband / multi-GNSS synthesis.

#include "core/scenario/MultiGnssBaseband.h"

#include <cmath>
#include <cstdint>

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Deterministic PRN chip generator (LFSR) — same family as the GPS baseband so
// each constellation channel carries real code content.
uint32_t nextChip(uint32_t& lfsr) {
    const uint32_t bit =
        ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return bit;
}
}  // namespace

std::string gnssSystemName(GnssSystem s) {
    switch (s) {
        case GnssSystem::Gps: return "GPS";
        case GnssSystem::Glonass: return "GLONASS";
        case GnssSystem::BeiDou: return "BeiDou";
    }
    return "GPS";
}

double MultiGnssBaseband::defaultCarrierHz(GnssSystem s) {
    switch (s) {
        case GnssSystem::Gps: return 1575.42e6;   // L1
        case GnssSystem::Glonass: return 1602.0e6; // L1 (approx)
        case GnssSystem::BeiDou: return 1561.098e6; // B1I
    }
    return 1575.42e6;
}

double MultiGnssBaseband::defaultChipRateHz(GnssSystem s) {
    switch (s) {
        case GnssSystem::Gps: return 1.023e6;     // C/A
        case GnssSystem::Glonass: return 0.511e6; // L1 C/A-like
        case GnssSystem::BeiDou: return 2.046e6;  // B1I
    }
    return 1.023e6;
}

bool MultiGnssBaseband::frameIsValid(const ConstellationFrame& f) {
    if (f.samples.empty()) return false;
    for (const auto& s : f.samples) {
        if (!std::isfinite(s.i) || !std::isfinite(s.q)) return false;
    }
    return true;
}

ConstellationFrame MultiGnssBaseband::synthesize(
    GnssSystem system, double sampleRate, double durationSec, int numChannels,
    double carrierOverride) {
    ConstellationFrame f;
    f.system = system;
    f.carrierHz = carrierOverride > 0.0 ? carrierOverride
                                        : defaultCarrierHz(system);
    if (sampleRate <= 0.0 || durationSec < 0.0 || numChannels < 1) {
        return f;  // invalid -> empty frame
    }

    const std::size_t n = static_cast<std::size_t>(
        std::llround(sampleRate * durationSec));
    const double chipRate = defaultChipRateHz(system);

    // Sum `numChannels` channels, each with its own PRN chip stream and a small
    // per-channel phase offset. Deterministic.
    std::vector<IqSample> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        double iAcc = 0.0, qAcc = 0.0;
        for (int c = 0; c < numChannels; ++c) {
            uint32_t lfsr = 0xACE1u + static_cast<uint32_t>(c) * 7919u;
            // Advance the LFSR to sample i (deterministic per channel).
            for (std::size_t k = 0; k < i; ++k) nextChip(lfsr);
            const double chip = nextChip(lfsr) ? 1.0 : -1.0;
            const double phase =
                2.0 * kPi * (f.carrierHz * t + chipRate * t * 0.5) +
                c * 0.3;
            iAcc += 0.5 * chip * std::cos(phase);
            qAcc += 0.5 * chip * std::sin(phase);
        }
        out.push_back({iAcc, qAcc});
    }
    f.samples = std::move(out);
    return f;
}

}  // namespace lodestar::scenario
