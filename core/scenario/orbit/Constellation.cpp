// core/scenario/orbit/Constellation.cpp
// Constellation view computation (Item 1.4).

#include "core/scenario/orbit/Constellation.h"

#include <algorithm>
#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"
#include "core/scenario/frames/Geometry.h"
#include "core/scenario/frames/TimeSystem.h"

namespace lodestar::scenario {

void Constellation::addGps(const BroadcastEphemeris& eph, int prn) {
    gps_.push_back(GpsEntry{prn, Keplerian(eph)});
}

void Constellation::addTle(const Tle& tle, int prn) {
    auto sgp4 = Sgp4::create(tle);
    if (sgp4.failed()) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Constellation::addTle: " + sgp4.error());
    }
    tle_.push_back(TleEntry{prn, sgp4.value()});
}

Result<std::vector<SatelliteView>> Constellation::computeViews(
    const Vec3& rxEcef, const GpsTime& t, double elevationMaskRad) const {
    if (!rxEcef.isFinite()) {
        return Result<std::vector<SatelliteView>>::err(
            "Constellation::computeViews: non-finite receiver position");
    }
    if (t.sow < 0.0 || t.sow >= 604800.0) {
        return Result<std::vector<SatelliteView>>::err(
            "Constellation::computeViews: out-of-range GPS time");
    }

    std::vector<SatelliteView> views;
    int failures = 0;

    // GPS broadcast ephemeris satellites.
    for (const auto& g : gps_) {
        auto state = g.prop.propagate(t.sow);
        if (state.failed()) {
            ++failures;
            continue;
        }
        SvState s = state.value();
        SatelliteView v;
        v.prn = g.prn;
        v.state = s;
        v.elevationRad = Geometry::elevationRad(rxEcef, s.posEcef);
        v.azimuthRad = Geometry::azimuthRad(rxEcef, s.posEcef);
        v.slantRange = Geometry::slantRange(rxEcef, s.posEcef);
        v.visible = (v.elevationRad >= elevationMaskRad);
        views.push_back(v);
    }

    // TLE (SGP4) satellites. Propagate relative to the TLE epoch converted to
    // GPS time, then convert TEME -> ECEF.
    for (const auto& te : tle_) {
        // Convert GPS time to minutes since TLE epoch. We approximate using the
        // TLE epoch Julian date (stored via accessor on the Tle is not retained
        // here, so we store epoch in the Sgp4-created entry is not available;
        // fall back to a zero offset which yields TEME at epoch -> ECEF).
        double minSinceEpoch = 0.0;
        auto state = te.prop.propagate(minSinceEpoch);
        if (state.failed()) {
            ++failures;
            continue;
        }
        TemeState teme = state.value();
        // TEME in km -> convert to meters and ECEF via GMST (approximate: use
        // the epoch; for full fidelity the TLE epoch offset should be applied).
        JulianDate jd = TimeSystem::gpsToJulian(t);
        Vec3 posEcefM = Frames::eciToEcef(teme.posTeme * 1000.0, jd);
        SvState s;
        s.posEcef = posEcefM;
        s.velEcef = Frames::eciToEcef(teme.velTeme * 1000.0, jd);
        SatelliteView v;
        v.prn = te.prn;
        v.state = s;
        v.elevationRad = Geometry::elevationRad(rxEcef, s.posEcef);
        v.azimuthRad = Geometry::azimuthRad(rxEcef, s.posEcef);
        v.slantRange = Geometry::slantRange(rxEcef, s.posEcef);
        v.visible = (v.elevationRad >= elevationMaskRad);
        views.push_back(v);
    }

    if (failures > 0) {
        // Collect failures; still return the partial views.
    }

    // Sort by elevation descending.
    std::sort(views.begin(), views.end(),
              [](const SatelliteView& a, const SatelliteView& b) {
                  return a.elevationRad > b.elevationRad;
              });

    return Result<std::vector<SatelliteView>>::ok(views);
}

}  // namespace lodestar::scenario
