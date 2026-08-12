// core/scenario/errors/ErrorModelConfig.cpp
// Error model orchestration (Item 5.4).

#include "core/scenario/errors/ErrorModelConfig.h"

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

void ErrorModelConfig::setIonosphereModel(const Ionosphere::KlobucharParams& p) {
    iono_.setKlobuchar(p);
}

void ErrorModelConfig::setTroposphereModel(Troposphere::Model m) {
    tropo_.setModel(m);
}

void ErrorModelConfig::setMeteo(double p, double t, double h) {
    tropo_.setMeteo(p, t, h);
}

void ErrorModelConfig::setClock(const ClockModel::Config& rx,
                                const ClockModel::Config& sv) {
    clock_.setReceiver(rx);
    clock_.setSatellite(sv);
}

Result<AtmosphericCorrections> ErrorModelConfig::compute(
    const SvState& sv, const Vec3& rxEcef, const GpsTime& t) const {
    AtmosphericCorrections c;
    if (ionoOn_) {
        auto iono = iono_.delay(sv, rxEcef, t);
        if (iono.failed()) {
            return Result<AtmosphericCorrections>::err(
                "ErrorModelConfig: ionosphere: " + iono.error());
        }
        c.iono = iono.value();
    }
    if (tropoOn_) {
        auto tropo = tropo_.delay(sv, rxEcef);
        if (tropo.failed()) {
            return Result<AtmosphericCorrections>::err(
                "ErrorModelConfig: troposphere: " + tropo.error());
        }
        c.tropo = tropo.value();
    }
    if (clockOn_) {
        // Relativistic contribution is already embedded in the satellite clock
        // bias; we report it as the satellite clock bias in meters for clarity.
        c.rel = sv.clockBias * 299792458.0;
    }
    return Result<AtmosphericCorrections>::ok(c);
}

}  // namespace lodestar::scenario
