// core/scenario/RfImpairments.h
// ScenarioForge RF impairments (S2 Phase 14).
//
// Multipath and interference applied to the baseband I/Q sample stream. These
// model real-world RF impairments a GNSS receiver must tolerate.

#pragma once

#include <vector>

#include "core/scenario/Baseband.h"

namespace lodestar::scenario {

// Add a delayed, attenuated copy of the signal to model multipath. `delay` is
// the delay in samples; `gain` is the attenuation of the delayed copy
// (0 < gain < 1 for a real reflected path). The output has the same length as
// the input: out[i] = samples[i] + gain * samples[i - delay] (for i >= delay).
std::vector<IqSample> applyMultipath(const std::vector<IqSample>& samples,
                                     int delay,
                                     double gain);

// Add additive interference (noise/jammer) to the sample stream. `amplitude`
// scales the injected interference. The output has the same length as the
// input: out[i] = samples[i] + interference[i].
std::vector<IqSample> applyInterference(const std::vector<IqSample>& samples,
                                        double amplitude);

}  // namespace lodestar::scenario
