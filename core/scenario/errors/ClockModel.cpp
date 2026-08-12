// core/scenario/errors/ClockModel.cpp
// Polynomial clock model (Item 5.1). bias + drift*t + driftRate*t^2.

#include "core/scenario/errors/ClockModel.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
double poly(const ClockModel::Config& c, double t, bool drift) {
    if (drift) {
        return c.drift + 2.0 * c.driftRate * t;
    }
    return c.bias + c.drift * t + c.driftRate * t * t;
}
}  // namespace

void ClockModel::setReceiver(const Config& c) {
    if (!std::isfinite(c.bias) || !std::isfinite(c.drift) ||
        !std::isfinite(c.driftRate)) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "ClockModel::setReceiver: non-finite config");
    }
    rx_ = c;
}

void ClockModel::setSatellite(const Config& c) {
    if (!std::isfinite(c.bias) || !std::isfinite(c.drift) ||
        !std::isfinite(c.driftRate)) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "ClockModel::setSatellite: non-finite config");
    }
    sv_ = c;
}

double ClockModel::receiverBias(double t) const { return poly(rx_, t, false); }
double ClockModel::satelliteBias(double t) const { return poly(sv_, t, false); }
double ClockModel::receiverDrift(double t) const { return poly(rx_, t, true); }
double ClockModel::satelliteDrift(double t) const { return poly(sv_, t, true); }

}  // namespace lodestar::scenario
