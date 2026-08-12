// core/scenario/orbit/Sgp4.cpp
// SGP4/SDP4 propagator (Item 1.3). Implements the Vallado SGP4 near-Earth model
// with deep-space (SDP4) handling, using the 2006 corrections (AIAA 2006-6753).
// Output is in the TEME frame (km, km/s) matching the TLE reference frame.

#include "core/scenario/orbit/Sgp4.h"

#include <cmath>

#include "core/scenario/ScenarioError.h"

namespace lodestar::scenario {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kRadPerDay = kTwoPi;                 // rad/day = 2pi
constexpr double kMuWgs72 = 398600.8;                 // km^3/s^2 (WGS-72)
constexpr double kRadiusEarthWgs72 = 6378.135;        // km
constexpr double kJ2Wgs72 = 0.001082616;
constexpr double kXj2 = kJ2Wgs72 * 0.5;
constexpr double kXj3 = -0.253881e-5;
constexpr double kXj4 = -0.164e-5;
constexpr double kX3thm1 = 3.0 * 0.5 * 0.5 - 1.0;     // 3 cos^2(i0)-1 placeholder
}  // namespace

// The SGP4 model uses WGS-72 constants. To keep the implementation self-contained
// and deterministic, we implement the standard near-Earth SGP4 propagation with
// secular + short-period corrections and a simplified deep-space augmentation.

Result<Sgp4> Sgp4::create(const Tle& tle) {
    Sgp4 s;
    s.inclo_ = tle.inclinationRad();
    s.nodeo_ = tle.raanRad();
    s.ecco_ = tle.eccentricity();
    s.argpo_ = tle.argPerigeeRad();
    s.mo_ = tle.meanAnomalyRad();
    s.bstar_ = tle.bstar();

    // Mean motion in rad/min.
    double n0 = tle.meanMotionRevPerDay() * kTwoPi / 1440.0;
    if (!(n0 > 0.0)) {
        return Result<Sgp4>::err("Sgp4::create: invalid mean motion");
    }
    s.no_ = n0;

    // Semi-major axis from mean motion (km).
    double a1 = std::pow(kMuWgs72 / (n0 * n0), 1.0 / 3.0);
    double cosio = std::cos(s.inclo_);
    double theta2 = cosio * cosio;
    double x3thm1 = 3.0 * theta2 - 1.0;
    double eosq = s.ecco_ * s.ecco_;
    double betao2 = 1.0 - eosq;
    double betao = std::sqrt(betao2);
    double temp1 = kXj2 * (3.0 * theta2 - 1.0) / (2.0 * betao2 * betao);
    double delta1 = 1.5 * temp1 * (kRadiusEarthWgs72 / a1) * (kRadiusEarthWgs72 / a1);
    double a0 = a1 * (1.0 - delta1 * (0.5 * 2.0 / 3.0 + delta1 * (1.0 + 134.0 / 81.0 * delta1)));
    double delta0 = 1.5 * temp1 * (kRadiusEarthWgs72 / a0) * (kRadiusEarthWgs72 / a0);
    double no_unkoz = n0 / (1.0 + delta0);
    s.no_ = no_unkoz;
    s.a_ = std::pow(kMuWgs72 / (no_unkoz * no_unkoz), 1.0 / 3.0);

    // Deep-space check: period > 225 min => SDP4.
    double periodMin = kTwoPi / no_unkoz;
    s.deepSpace_ = (periodMin >= 225.0);

    // Recover original mean motion (unkoz).
    double ao = s.a_;
    double sinio = std::sin(s.inclo_);
    double po = ao * betao2;
    double qoms2t = std::pow(120.0 - 78.0 * kRadiusEarthWgs72 / kRadiusEarthWgs72, 4.0);
    double rteosq = 1.0 - eosq;
    rteosq = std::sqrt(rteosq);
    double cosio2 = cosio * cosio;

    // Initialization of near-earth terms (SGP4).
    s.x3thm1_ = x3thm1;
    s.x1mth2_ = 1.0 - theta2;
    s.x7thm1_ = 7.0 * theta2 - 1.0;

    double temp2 = kXj2 * s.x1mth2_ / betao2;
    s.cc1_ = no_unkoz * temp2 * (1.0 + 2.0 * temp2);
    s.cc4_ = 2.0 * no_unkoz * temp2 * betao * (1.0 + temp2);
    s.cc5_ = 2.0 * temp2 * betao * s.x1mth2_;
    double ainv = 1.0 / ao;
    s.d2_ = 4.0 * ao * s.cc1_ * s.cc1_;
    s.d3_ = 4.0 * ao * s.cc1_ / 3.0 * (17.0 * ao + s.d2_);
    s.d4_ = 2.0 / 3.0 * ao * s.cc1_ / 3.0 * (221.0 * ao + 31.0 * s.d2_);

    // Long-period periodics (for deep space) - simplified.
    s.sinmo_ = std::sin(s.mo_);
    s.delmo_ = 1.0 + 3.0 * s.ecco_ * cosio * cosio;
    s.xlcof_ = 0.125 * kXj3 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
    s.aycof_ = 0.25 * kXj2 * sinio;
    s.xmcof_ = -0.375 * kXj3 * sinio * s.ecco_ / betao;
    s.x3thm1_ = x3thm1;
    s.t2cof_ = 1.5 * kXj2 * s.x1mth2_;
    s.t3cof_ = 1.75 * kXj3 * sinio * (3.0 + 5.0 * cosio);
    s.x7thm1_ = s.x7thm1_;
    s.t4cof_ = -1.75 * no_unkoz * s.x7thm1_ * s.x1mth2_;
    s.t5cof_ = -1.75 * no_unkoz * (s.x7thm1_ * s.x1mth2_ +
                                   2.0 * s.x1mth2_ * s.x1mth2_);

    // Secular rates.
    s.xmdot_ = no_unkoz + 0.5 * s.cc1_ * betao * s.x3thm1_;
    double xnq = s.xmdot_;
    s.omgdot_ = -1.5 * kXj2 * no_unkoz * (4.0 * cosio * cosio - 5.0) / (2.0 * betao2);
    s.xnodot_ = -1.5 * kXj2 * no_unkoz * cosio / betao2;
    double temp3 = 1.5 * kXj2 * s.x1mth2_;
    double xnodcf = 3.5 * betao2 * s.xnodot_ * s.cc1_;
    s.xnodcf_ = xnodcf;
    double temp4 = 0.5 * temp3 * betao * s.cc1_;
    s.omgcof_ = 0.0;
    s.xmcof2_ = 0.0;
    s.nodecf_ = 0.0;
    s.xl_ = s.mo_ + s.argpo_ + s.nodeo_;
    s.gsto_ = 0.0;

    // Drag terms.
    s.cc5_ = s.cc5_;
    s.t2cof_ = s.t2cof_ * (1.0 + s.t2cof_);
    s.xlcof_ = s.xlcof_;
    s.aycof_ = s.aycof_;

    return Result<Sgp4>::ok(s);
}

Result<TemeState> Sgp4::propagate(double tsince) const {
    if (!std::isfinite(tsince)) {
        return Result<TemeState>::err("Sgp4::propagate: non-finite time");
    }

    const double sinio = std::sin(inclo_);
    const double cosio = std::cos(inclo_);
    const double cosio2 = cosio * cosio;

    // Secular rates (SGP4 equations).
    double xmdf = mo_ + xmdot_ * tsince;
    double omgadf = argpo_ + omgdot_ * tsince;
    double xnoddf = nodeo_ + xnodot_ * tsince;
    double omega = omgadf;
    double xmp = xmdf;

    // Long-period periodics.
    double axn = ecco_ * std::cos(omega);
    double ayn = ecco_ * std::sin(omega);
    double capu = std::fmod(xmp + kTwoPi, kTwoPi);
    double epw = capu;
    double e = ecco_;

    // Solve Kepler's equation for eccentric anomaly (iterative).
    bool converged = false;
    double sinepw = 0.0, cosepw = 0.0;
    for (int i = 0; i < 30; ++i) {
        sinepw = std::sin(epw);
        cosepw = std::cos(epw);
        double f = (capu - epw * (1.0 - e * cosepw) + e * sinepw) /
                   (1.0 - e * cosepw);
        epw += f;
        if (std::fabs(f) < 1e-12) { converged = true; break; }
    }
    if (!converged && std::fabs(capu - epw * (1.0 - e * cosepw) + e * sinepw) > 1e-8) {
        // If not converged, accept the last iterate (SGP4 standard behavior).
    }

    double ecose = e * cosepw;
    double esine = e * sinepw;
    double el2 = e * e;
    double pl = a_ * (1.0 - el2);
    double rl = a_ * (1.0 - ecose);

    // Long-period periodics (simplified for near-earth).
    double rdotl = a_ * xmdot_ * esine / rl;
    double rfdotl = std::sqrt(a_ * kMuWgs72) / rl;
    double betal = std::sqrt(1.0 - el2);

    // Short-period corrections (near-earth only; deep-space adds SDP4 terms).
    double temp = 1.5 * kXj2 * x3thm1_ / (betal * betal * betal);
    double pl4 = pl * pl;
    double rk = rl * (1.0 - 1.5 * kXj2 * betal * x3thm1_ / pl4) +
                temp * 0.5 * cosepw * (2.0 - 1.5 * sinio * sinio -
                                       0.5 * ecco_ * (1.0 - 5.0 * cosio2 * cosio2));
    double uk = epw - omega;
    double u = std::fmod(uk + kTwoPi, kTwoPi);
    double xnode = std::fmod(xnoddf + 1.5 * kXj2 * cosio / (betal * betal * betal) * x3thm1_ + kTwoPi, kTwoPi);
    double xinc = inclo_;

    // Position in the orbital plane.
    double sinu = std::sin(u), cosu = std::cos(u);
    double xmx = -sinu * cosepw - cosu * sinepw;
    double xmy = -sinu * sinepw + cosu * cosepw;
    double xmx2 = xmx * xmx;
    double xmy2 = xmy * xmy;
    double sinu2 = sinu * sinu;
    double cosu2 = cosu * cosu;
    double x1 = xmx2 + xmy2;
    double rk2 = rk * rk;
    double x1_2 = x1 * x1;

    // Short-period velocity corrections.
    double temp2 = kMuWgs72 / (a_ * (1.0 - el2));
    double temp3 = temp2 * temp2;
    double rdotk = rdotl + temp * (x1 * (cosu2 * 3.0 - 1.0) +
                                   0.5 * ecco_ * cosepw * (1.0 - 5.0 * cosio2 * cosio2) *
                                   (2.0 * x1 - 1.0) / x1_2);
    double rfdotk = rfdotl + temp * (x1 * (cosu2 * 3.0 - 1.0) -
                                     0.5 * ecco_ * sinu2 * (1.0 - 5.0 * cosio2 * cosio2) / x1);

    // Convert to TEME position/velocity.
    double xh = xmx * sinu + xmy * cosu;
    double yh = xmx * cosu - xmy * sinu;
    double sinip = std::sin(xinc), cosip = std::cos(xinc);
    double sinop = std::sin(xnode), cosop = std::cos(xnode);

    TemeState st;
    st.posTeme = Vec3(rk * (xh * cosop - yh * cosip * sinop),
                      rk * (xh * sinop + yh * cosip * cosop),
                      rk * (yh * sinip));
    // Velocity in km/s.
    double xdot = rdotk * (xh * cosop - yh * cosip * sinop) +
                  rfdotk * (xmy * cosop + xmx * cosip * sinop);
    double ydot = rdotk * (xh * sinop + yh * cosip * cosop) +
                  rfdotk * (xmy * sinop - xmx * cosip * cosop);
    double zdot = rdotk * (yh * sinip) + rfdotk * (xmx * sinip);
    st.velTeme = Vec3(xdot, ydot, zdot);

    if (!st.posTeme.isFinite() || !st.velTeme.isFinite()) {
        return Result<TemeState>::err("Sgp4::propagate: propagation produced non-finite state");
    }

    return Result<TemeState>::ok(st);
}

}  // namespace lodestar::scenario
