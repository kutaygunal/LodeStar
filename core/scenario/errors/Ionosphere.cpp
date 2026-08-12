// core/scenario/errors/Ionosphere.cpp
// Klobuchar ionospheric model (Item 5.2). Implements the IS-GPS-200 broadcast
// model: geomagnetic latitude, local time, obliquity factor, and alpha/beta fit.

#include "core/scenario/errors/Ionosphere.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/frames/Frames.h"
#include "core/scenario/frames/Geometry.h"

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kC = 299792458.0;
// Ionosphere height and earth radius.
constexpr double kIonoH = 350000.0;   // m
constexpr double kRe = 6378137.0;     // m (WGS-84 a)
// L1 frequency.
constexpr double kL1Freq = 1575.42e6;
}  // namespace

void Ionosphere::setKlobuchar(const KlobucharParams& p) {
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(p.alpha[i]) || !std::isfinite(p.beta[i])) {
            throw ScenarioError(ErrorCode::InvalidArgument,
                                "Ionosphere::setKlobuchar: non-finite coefficient");
        }
    }
    params_ = p;
    configured_ = true;
}

Result<double> Ionosphere::delay(const SvState& sv, const Vec3& rxEcef,
                                 const GpsTime& t) const {
    if (!configured_) {
        return Result<double>::err("Ionosphere: Klobuchar parameters not configured");
    }
    if (!sv.posEcef.isFinite() || !rxEcef.isFinite()) {
        return Result<double>::err("Ionosphere: non-finite input");
    }

    // Elevation angle (radians).
    double el = Geometry::elevationRad(rxEcef, sv.posEcef);
    if (el <= 0.0) {
        return Result<double>::err("Ionosphere: satellite below horizon");
    }
    double az = Geometry::azimuthRad(rxEcef, sv.posEcef);

    // Receiver geodetic latitude/longitude.
    double lat, lon, h;
    Frames::ecefToGeodetic(rxEcef, lat, lon, h);

    // Earth-centered angle psi.
    double psi = kPi / 2.0 - el - std::asin(kRe / (kRe + kIonoH) * std::cos(el));
    // Sub-ionospheric latitude.
    double phiI = lat + psi * std::cos(az);
    if (phiI > 0.416) phiI = 0.416;
    if (phiI < -0.416) phiI = -0.416;
    // Sub-ionospheric longitude.
    double lamI = lon + psi * std::sin(az) / std::cos(phiI);
    // Geomagnetic latitude.
    double phiM = phiI + 0.064 * std::cos(lamI - 1.617);

    // Local time (seconds).
    double utcSec = t.sow;
    double localTime = utcSec + lamI * 43200.0 / kPi;
    localTime = std::fmod(localTime, 86400.0);
    if (localTime < 0.0) localTime += 86400.0;

    // Obliquity factor. The Klobuchar model uses elevation in semicircles
    // (1 semicircle = pi radians); 0.53 is the standard breakpoint in that unit.
    double elSemi = el / kPi;
    double F = 1.0 + 16.0 * std::pow(0.53 - elSemi, 3.0);

    // Polynomial amplitudes (alpha) and periods (beta).
    double amp = params_.alpha[0];
    double per = params_.beta[0];
    double p = 1.0;
    for (int i = 1; i <= 3; ++i) {
        p *= phiM;
        amp += params_.alpha[i] * p;
        per += params_.beta[i] * p;
    }
    if (amp < 0.0) amp = 0.0;
    if (per < 72000.0) per = 72000.0;

    // Ionospheric delay (x in radians from local time in seconds).
    double x = kTwoPi * (localTime - 50400.0) / per;
    double I = F * (amp < 0.0 ? 0.0 : amp);
    if (std::fabs(x) < kPi / 2.0) {
        I = F * (5.0e-9 + amp * (1.0 - x * x / 2.0 + x * x * x * x / 24.0));
    }
    // Delay in meters at L1.
    double delayM = I * kC;
    // Scale to actual frequency (assume L1; if not L1, scale by (fL1/f)^2).
    // Here we assume L1, so no scaling.
    if (!std::isfinite(delayM) || delayM < 0.0) {
        return Result<double>::err("Ionosphere: invalid delay computed");
    }
    return Result<double>::ok(delayM);
}

}  // namespace lodestar::scenario
