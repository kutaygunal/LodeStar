// core/scenario/MultiGnssBaseband.h
// Gap-Fill ScenarioForge 5.5: first-party baseband / multi-GNSS synthesis.
//
// Extends the software-defined baseband to full multi-GNSS: GLONASS and BeiDou
// in addition to GPS, with per-constellation carrier, chip rate, and code. This
// is a software baseband / SDR-ready signal model — real RF emission remains
// adapter-driven (vendor hardware). Clearly scoped: do not overclaim RF output.
//
// Produces valid I/Q frames per constellation.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/scenario/Baseband.h"

namespace lodestar::scenario {

// GNSS constellations supported by the software baseband.
enum class GnssSystem {
    Gps,
    Glonass,
    BeiDou
};

std::string gnssSystemName(GnssSystem s);

// One synthesized constellation channel.
struct GnssChannel {
    GnssSystem system;
    int prn = 0;
    double carrierHz = 0.0;   // RF carrier for this constellation
    double chipRateHz = 0.0;  // code chip rate
    double dopplerHz = 0.0;   // doppler shift applied
};

// A synthesized I/Q frame for one constellation.
struct ConstellationFrame {
    GnssSystem system;
    double carrierHz = 0.0;
    std::vector<IqSample> samples;
};

// Software-defined multi-GNSS baseband synthesizer.
class MultiGnssBaseband {
public:
    // Synthesize an I/Q frame for `system` over `durationSec` at `sampleRate`
    // for `numChannels` channels. Deterministic. `carrierOverride` (Hz) selects
    // a specific carrier (0 = default per constellation).
    static ConstellationFrame synthesize(GnssSystem system, double sampleRate,
                                         double durationSec, int numChannels,
                                         double carrierOverride = 0.0);

    // The default RF carrier for a constellation (GPS L1, GLONASS L1, BeiDou B1).
    static double defaultCarrierHz(GnssSystem s);

    // The default code chip rate for a constellation.
    static double defaultChipRateHz(GnssSystem s);

    // Whether the synthesized frame for a constellation is valid (non-empty and
    // every sample finite).
    static bool frameIsValid(const ConstellationFrame& f);
};

}  // namespace lodestar::scenario
