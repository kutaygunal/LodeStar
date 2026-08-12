// core/scenario/Scenario.h
// Top-level ScenarioForge facade (Item 7). Drives constellation, PVT, NMEA,
// and protection-level output for an epoch.

#pragma once

#include <memory>
#include <vector>

#include "core/scenario/Types.h"
#include "core/scenario/errors/ErrorModelConfig.h"
#include "core/scenario/nmea/NmeaGenerator.h"
#include "core/scenario/orbit/Constellation.h"
#include "core/scenario/sbas/SbasCorrections.h"
#include "core/scenario/sbas/Integrity.h"

namespace lodestar::scenario {

class Scenario {
public:
    explicit Scenario(const ScenarioConfig& cfg);

    void addGps(const BroadcastEphemeris& eph, int prn);
    void addTle(const Tle& tle, int prn);
    void setReceiver(const Vec3& rxEcef, const Vec3& rxVelEcef);
    void setErrorModels(const ErrorModelConfig& models);
    void addSbasIgp(const IgpData& igp);

    // Compute one epoch's output: satellite views, PVT, NMEA stream, protection
    // levels.
    Result<ScenarioEpoch> step(const GpsTime& t);

private:
    ScenarioConfig cfg_;
    Constellation constellation_;
    Vec3 rxEcef_{0, 0, 0};
    Vec3 rxVelEcef_{0, 0, 0};
    bool receiverSet_ = false;
    std::unique_ptr<ErrorModelConfig> models_;
    std::unique_ptr<NmeaGenerator> nmea_;
    std::vector<IgpData> igps_;
};

}  // namespace lodestar::scenario
