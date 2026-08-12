// core/scenario/Baseband.h
// ScenarioForge software-defined baseband (S2 Phase 11).
//
// Given a scenario, a carrier frequency, a sample rate, and a duration,
// synthesize a vector of complex I/Q samples. This is the software-defined
// baseband: the raw IF/RF sample stream a real GNSS simulator would emit.

#pragma once

#include <cstddef>
#include <vector>

#include "core/scenario/Scenario.h"

namespace lodestar::scenario {

// A single complex I/Q sample (in-phase and quadrature components).
struct IqSample {
    double i = 0.0;
    double q = 0.0;
};

// Generate a vector of complex I/Q samples for `scenario` at `carrierHz`
// (Hz), `sampleRate` (samples/sec), over `durationSec` (seconds). The number
// of returned samples is approximately `sampleRate * durationSec`.
//
// The signal is a composite of the scenario's visible satellites (carrier +
// Doppler + PRN code content). If no usable satellite geometry is available it
// falls back to a single deterministic channel so the baseband is always
// well-defined and non-trivial.
std::vector<IqSample> generateBaseband(Scenario& scenario,
                                       double carrierHz,
                                       double sampleRate,
                                       double durationSec);

}  // namespace lodestar::scenario
