// core/scenario/errors/ClockModel.h
// Clock error model (receiver + satellite) (Item 5.1).

#pragma once

#include "core/scenario/Types.h"

namespace lodestar::scenario {

class ClockModel {
public:
    struct Config {
        double bias = 0.0;      // s
        double drift = 0.0;     // s/s
        double driftRate = 0.0; // s/s^2
    };

    void setReceiver(const Config& c);
    void setSatellite(const Config& c);

    // Polynomial clock bias at time t (seconds): bias + drift*t + driftRate*t^2.
    double receiverBias(double t) const;
    double satelliteBias(double t) const;
    // Polynomial clock drift at time t (s/s).
    double receiverDrift(double t) const;
    double satelliteDrift(double t) const;

private:
    Config rx_;
    Config sv_;
};

}  // namespace lodestar::scenario
