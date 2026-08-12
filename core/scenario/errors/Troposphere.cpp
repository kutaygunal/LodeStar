// core/scenario/errors/Troposphere.cpp
// Tropospheric delay: Saastamoinen and Hopfield models (Item 5.3).

#include "core/scenario/errors/Troposphere.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"
#include "core/scenario/frames/Geometry.h"

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

void Troposphere::setModel(Model m) { model_ = m; }

void Troposphere::setMeteo(double pressureHpa, double tempC, double humidityPct) {
    if (!(pressureHpa > 300.0) || pressureHpa > 1200.0) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Troposphere::setMeteo: pressure out of range");
    }
    if (tempC < -60.0 || tempC > 60.0) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Troposphere::setMeteo: temperature out of range");
    }
    if (humidityPct < 0.0 || humidityPct > 100.0) {
        throw ScenarioError(ErrorCode::InvalidArgument,
                            "Troposphere::setMeteo: humidity out of range");
    }
    p_ = pressureHpa;
    T_ = tempC;
    H_ = humidityPct;
}

double Troposphere::saastamoinen(double elRad) const {
    // Saastamoinen: ZTD = 0.002277/sin(el) * (P + (1255/T + 0.05)*e - a/tan^2(el))
    double e = H_ / 100.0 * 6.1078 * std::exp((17.27 * T_) / (T_ + 237.3));
    double sinEl = std::sin(elRad);
    double m = 1.0 / sinEl;
    double ztd = 0.002277 / sinEl *
                 (p_ + (1255.0 / (T_ + 273.15) + 0.05) * e - 0.0 / std::tan(elRad));
    (void)m;
    return ztd;
}

double Troposphere::hopfield(double elRad, double latRad) const {
    // Hopfield dry + wet refractivity integral.
    double hd = 40136.0 + 148.72 * (T_ - 273.15);
    double hw = 11000.0;
    double Tk = T_ + 273.15;
    double e = H_ / 100.0 * 6.1078 * std::exp((17.27 * T_) / (T_ + 237.3));
    double Nd = 77.6 * p_ / Tk;
    double Nw = 3.73e5 * e / (Tk * Tk);
    double sinEl = std::sin(elRad);
    double f = 1.0 / std::sin(std::sqrt(elRad * elRad + 6.25e-5));
    double dd = Nd * 1e-6 * std::sqrt(hd) * f;
    double dw = Nw * 1e-6 * std::sqrt(hw) * f;
    (void)latRad;
    return dd + dw;
}

Result<double> Troposphere::delay(const SvState& sv, const Vec3& rxEcef) const {
    if (!sv.posEcef.isFinite() || !rxEcef.isFinite()) {
        return Result<double>::err("Troposphere: non-finite input");
    }
    double el = Geometry::elevationRad(rxEcef, sv.posEcef);
    if (el <= 0.0) {
        return Result<double>::err("Troposphere: satellite below horizon");
    }
    double lat, lon, h;
    Frames::ecefToGeodetic(rxEcef, lat, lon, h);
    double d;
    switch (model_) {
        case Model::Saastamoinen: d = saastamoinen(el); break;
        case Model::Hopfield: d = hopfield(el, lat); break;
        default: return Result<double>::err("Troposphere: unknown model");
    }
    if (!std::isfinite(d) || d < 0.0) {
        return Result<double>::err("Troposphere: invalid delay computed");
    }
    return Result<double>::ok(d);
}

}  // namespace lodestar::scenario
