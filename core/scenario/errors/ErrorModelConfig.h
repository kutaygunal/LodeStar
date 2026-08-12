// core/scenario/errors/ErrorModelConfig.h
// Central configuration selecting active error models (Item 5.4).

#pragma once

#include "core/scenario/Types.h"
#include "core/scenario/errors/ClockModel.h"
#include "core/scenario/errors/Ionosphere.h"
#include "core/scenario/errors/Troposphere.h"

namespace lodestar::scenario {

class ErrorModelConfig {
public:
    void enableClock(bool on) { clockOn_ = on; }
    void enableIonosphere(bool on) { ionoOn_ = on; }
    void enableTroposphere(bool on) { tropoOn_ = on; }
    void setIonosphereModel(const Ionosphere::KlobucharParams& p);
    void setTroposphereModel(Troposphere::Model m);
    void setMeteo(double p, double t, double h);
    void setClock(const ClockModel::Config& rx, const ClockModel::Config& sv);

    bool clockEnabled() const { return clockOn_; }
    bool ionoEnabled() const { return ionoOn_; }
    bool tropoEnabled() const { return tropoOn_; }

    // Compute the aggregate atmospheric/relativistic corrections for a satellite
    // at GPS time t. Satellite clock bias (including relativistic) is taken from
    // the SvState; receiver clock bias is provided separately by the caller.
    Result<AtmosphericCorrections> compute(const SvState& sv, const Vec3& rxEcef,
                                           const GpsTime& t) const;

    ClockModel& clock() { return clock_; }

private:
    bool clockOn_ = true;
    bool ionoOn_ = true;
    bool tropoOn_ = true;
    ClockModel clock_;
    Ionosphere iono_;
    Troposphere tropo_;
};

}  // namespace lodestar::scenario
