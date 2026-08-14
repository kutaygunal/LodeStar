// core/scenario/RfImpairments.h
// ScenarioForge RF impairments (S2 Phase 14).
//
// Multipath and interference applied to the baseband I/Q sample stream. These
// model real-world RF impairments a GNSS receiver must tolerate.

#pragma once

#include <string>
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

// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.4: advanced interference & multipath depth.
// ---------------------------------------------------------------------------

// Interference scenario kinds.
enum class InterferenceKind {
    MatchedSpectrum,  // interferer with the same spectral shape as the signal
    ContinuousWave,   // CW tone at a carrier offset
    Awgn,             // broadband noise
    Jamming,          // high-power denial
    Spoofing          // false signal injection
};

// A single interference source applied to the sample stream.
struct InterferenceSource {
    InterferenceKind kind = InterferenceKind::Awgn;
    double amplitude = 0.0;   // relative power
    double frequencyHz = 0.0; // CW/jamming carrier offset
    int startSample = 0;      // first sample affected
    int endSample = 0;        // last sample affected (inclusive)
};

// Apply an interference source to the sample stream. Matched-spectrum
// re-modulates the input (scaled) onto itself; CW injects a tone; AWGN injects
// deterministic noise; jamming injects high-power noise; spoofing injects a
// scaled, phase-delayed copy (a false signal). Deterministic.
std::vector<IqSample> applyInterferenceSource(
    const std::vector<IqSample>& samples, const InterferenceSource& src);

// A data-driven environment model for multipath: a set of reflection paths,
// each with a delay (samples) and gain.
struct ReflectionPath {
    int delaySamples = 0;
    double gain = 0.0;
};

// Apply a set of reflection paths (urban canyon / ground reflection) to the
// sample stream. Each path adds a delayed, attenuated copy. Deterministic.
std::vector<IqSample> applyEnvironment(const std::vector<IqSample>& samples,
                                       const std::vector<ReflectionPath>& paths);

// Built-in environment models.
std::vector<ReflectionPath> urbanCanyonModel(double groundGain);
std::vector<ReflectionPath> groundReflectionModel(double gain);

}  // namespace lodestar::scenario
