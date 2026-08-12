// core/scenario/Scenario.cpp
// Scenario facade implementation (Item 7).

#include "core/scenario/Scenario.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/TimeSystem.h"
#include "core/scenario/pvt/Pseudorange.h"
#include "core/scenario/pvt/Solver.h"

namespace lodestar::scenario {

Scenario::Scenario(const ScenarioConfig& cfg)
    : cfg_(cfg), nmea_(std::make_unique<NmeaGenerator>(cfg.nmea)) {
    models_ = std::make_unique<ErrorModelConfig>();
}

void Scenario::addGps(const BroadcastEphemeris& eph, int prn) {
    constellation_.addGps(eph, prn);
}

void Scenario::addTle(const Tle& tle, int prn) {
    constellation_.addTle(tle, prn);
}

void Scenario::setReceiver(const Vec3& rxEcef, const Vec3& rxVelEcef) {
    if (!rxEcef.isFinite() || !rxVelEcef.isFinite()) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Scenario::setReceiver: non-finite receiver state");
    }
    rxEcef_ = rxEcef;
    rxVelEcef_ = rxVelEcef;
    receiverSet_ = true;
}

void Scenario::setErrorModels(const ErrorModelConfig& models) {
    *models_ = models;
}

void Scenario::addSbasIgp(const IgpData& igp) { igps_.push_back(igp); }

Result<ScenarioEpoch> Scenario::step(const GpsTime& t) {
    if (!receiverSet_) {
        return Result<ScenarioEpoch>::err("Scenario: receiver not set");
    }
    if (t.sow < 0.0 || t.sow >= 604800.0) {
        return Result<ScenarioEpoch>::err("Scenario: out-of-range GPS time");
    }

    ScenarioEpoch epoch;

    // 1. Compute satellite views.
    auto viewsRes = constellation_.computeViews(rxEcef_, t, cfg_.elevationMaskRad);
    if (viewsRes.failed()) {
        return Result<ScenarioEpoch>::err("Scenario: " + viewsRes.error());
    }
    epoch.views = viewsRes.value();

    // 2. Compute pseudoranges and solve PVT.
    std::vector<PvSolver::Measurement> meas;
    for (const auto& v : epoch.views) {
        if (!v.visible) continue;
        AtmosphericCorrections atm;
        auto atmRes = models_->compute(v.state, rxEcef_, t);
        if (atmRes.failed()) {
            // Skip satellites whose atmospheric model fails.
            continue;
        }
        atm = atmRes.value();
        auto rho = Pseudorange::compute(v.state, rxEcef_, 0.0, atm);
        if (rho.failed()) continue;
        PvSolver::Measurement m;
        m.sv = v.state;
        m.pseudorange = rho.value();
        meas.push_back(m);
    }

    if (meas.size() >= 4) {
        auto pvt = PvSolver::solve(meas);
        epoch.pvt = pvt;
        if (pvt.isOk()) {
            // 3. Generate NMEA stream.
            try {
                epoch.nmea.push_back(nmea_->zda(t));
                epoch.nmea.push_back(nmea_->gga(pvt.value(), epoch.views));
                epoch.nmea.push_back(nmea_->rmc(pvt.value(), t));
                epoch.nmea.push_back(nmea_->gsa(pvt.value(), epoch.views));
                epoch.nmea.push_back(nmea_->gsv(epoch.views));
            } catch (const ScenarioError& e) {
                // If NMEA generation fails, report the epoch PVT without NMEA.
            }
        }
    } else {
        epoch.pvt = Result<PvtResult>::err(
            "Scenario: insufficient visible satellites for PVT (<4)");
    }

    // 4. Protection levels (if SBAS enabled).
    if (cfg_.enableSbas && !epoch.views.empty()) {
        Integrity integrity;
        std::vector<double> udres;
        for (const auto& v : epoch.views) {
            double udre = 1.0;  // default UDRE
            integrity.setUdre(v.prn, udre);
            udres.push_back(udre);
        }
        auto pl = integrity.compute(epoch.views, udres, 1.0);
        if (pl.isOk()) epoch.protection = pl.value();
    }

    return Result<ScenarioEpoch>::ok(epoch);
}

}  // namespace lodestar::scenario
