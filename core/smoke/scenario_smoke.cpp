// core/smoke/scenario_smoke.cpp
// Self-verifying smoke path for Phase 4 (ScenarioForge real GNSS math).
//
// Exercises: frame/time conversion, Keplerian propagation, SGP4 propagation,
// RINEX parse, NMEA generation (checksum-verified), pseudorange + Doppler,
// error-model corrections, and an SBAS protection-level computation. Returns
// non-zero on failure.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "core/scenario/ScenarioError.h"
#include "core/scenario/errors/ClockModel.h"
#include "core/scenario/errors/ErrorModelConfig.h"
#include "core/scenario/errors/Ionosphere.h"
#include "core/scenario/errors/Troposphere.h"
#include "core/scenario/frames/Frames.h"
#include "core/scenario/frames/Geometry.h"
#include "core/scenario/frames/TimeSystem.h"
#include "core/scenario/nmea/NmeaGenerator.h"
#include "core/scenario/nmea/NmeaSentence.h"
#include "core/scenario/orbit/Constellation.h"
#include "core/scenario/orbit/Keplerian.h"
#include "core/scenario/orbit/Sgp4.h"
#include "core/scenario/orbit/Tle.h"
#include "core/scenario/pvt/Doppler.h"
#include "core/scenario/pvt/Pseudorange.h"
#include "core/scenario/Scenario.h"
#include "core/scenario/rinex/RinexNav.h"
#include "core/scenario/sbas/Integrity.h"
#include "core/scenario/sbas/SbasMessage.h"

namespace lodestar::scenario {

namespace {
int failures = 0;

bool close(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

void check(bool ok, const std::string& name) {
    if (ok) {
        std::printf("  [PASS] %s\n", name.c_str());
    } else {
        std::printf("  [FAIL] %s\n", name.c_str());
        ++failures;
    }
}

// Reference values (hand-computed / published; tolerances per plan R6).
}  // namespace

int runScenarioSmoke() {
    std::printf("ScenarioForge Phase 4 smoke path\n");

    // --- 1. Frame & time: ECEF <-> geodetic round trip ---------------------
    {
        double lat = 40.0 * 3.14159265358979323846 / 180.0;
        double lon = -105.0 * 3.14159265358979323846 / 180.0;
        double h = 1600.0;
        Vec3 ecef = Frames::geodeticToEcef(lat, lon, h);
        double rlat, rlon, rh;
        Frames::ecefToGeodetic(ecef, rlat, rlon, rh);
        check(close(lat, rlat, 1e-9) && close(lon, rlon, 1e-9) &&
                  close(h, rh, 1e-3),
              "ECEF <-> geodetic round trip");
    }

    // --- 2. Keplerian propagation ------------------------------------------
    {
        BroadcastEphemeris eph;
        eph.sqrtA = 5153.6556;
        eph.e = 0.003;
        eph.i0 = 0.959931;         // ~55 deg
        eph.omega0 = -1.5707963;
        eph.argp = 0.0;
        eph.M0 = 0.0;
        eph.toe = 0.0;
        eph.deltaN = 0.0;
        eph.idot = 0.0;
        eph.omegadot = 0.0;
        eph.af0 = 0.0; eph.af1 = 0.0; eph.af2 = 0.0;
        Keplerian k(eph);
        auto s = k.propagate(0.0);
        check(s.isOk(), "Keplerian propagate at toe");
        if (s.isOk()) {
            double a = eph.sqrtA * eph.sqrtA;
            // At t=toe with M0=0 the satellite is near perigee: r = a(1-e).
            double expected = a * (1.0 - eph.e);
            check(close(s.value().posEcef.norm(), expected, 1.0),
                  "Keplerian radius matches a(1-e) at perigee (within 1 m)");
        }
        // Invalid eccentricity -> error.
        BroadcastEphemeris bad = eph;
        bad.e = 1.5;
        Keplerian kb(bad);
        check(kb.propagate(0.0).failed(), "Keplerian rejects e>=1");
    }

    // --- 3. SGP4 propagation + TLE parsing ---------------------------------
    {
        // ISS TLE (published).
        const char* name = "ISS (ZARYA)";
        const char* l1 = "1 25544U 98067A   20334.51635445  .00016717  00000-0  10270-3 0  9002";
        const char* l2 = "2 25544  51.6431 236.7993 0003709  73.7848 286.3923 15.49403545232423";
        auto tle = Tle::parse(name, l1, l2);
        check(tle.isOk(), "TLE parse (ISS)");
        if (tle.isOk()) {
            auto sgp4 = Sgp4::create(tle.value());
            check(sgp4.isOk(), "Sgp4::create");
            if (sgp4.isOk()) {
                auto st = sgp4.value().propagate(0.0);
                check(st.isOk() && st.value().posTeme.isFinite(),
                      "Sgp4 propagate at epoch");
            }
        }
        // Malformed TLE -> error.
        std::string bad = l1;
        bad[0] = '9';
        check(Tle::parse(name, bad, l2).failed(), "TLE rejects malformed line");
    }

    // --- 4. RINEX navigation parse -----------------------------------------
    {
        std::istringstream ss(
            "     3.03                                                 RINEX VERSION / TYPE\n"
            "                                                            END OF HEADER\n"
            "G01 2020 11 29 12 00 00 0.123456789E-05 0.123456789E-11 0.000000000E+00\n"
            "    0.100000000E+01  0.500000000E+01  0.000000000E+00  0.100000000E+00\n"
            "    0.100000000E+01  0.300000000E-02  0.000000000E+00  0.515300000E+04\n"
            "    0.000000000E+00  0.000000000E+00 -0.150000000E+01  0.000000000E+00\n"
            "    0.900000000E+00  0.000000000E+00  0.000000000E+00  0.000000000E+00\n"
            "    0.000000000E+00  0.000000000E+00  0.000000000E+00  0.000000000E+00\n"
            "    0.000000000E+00  0.000000000E+00  0.000000000E+00  0.000000000E+00\n"
            "    0.000000000E+00  0.000000000E+00  0.000000000E+00  0.000000000E+00\n");
        RinexNavParser parser(ss);
        auto hdr = parser.readHeader();
        check(hdr.isOk(), "RINEX NAV header read");
        auto eph = parser.nextEphemeris();
        check(eph.isOk(), "RINEX NAV ephemeris parsed");
        if (eph.isOk()) {
            check(eph.value().prn == 1, "RINEX NAV PRN parsed");
        }
        auto eof = parser.nextEphemeris();
        check(eof.isEof(), "RINEX NAV end-of-file signaled");
    }

    // --- 5. NMEA generation + checksum --------------------------------------
    {
        NmeaConfig cfg;
        NmeaGenerator gen(cfg);
        PvtResult pvt;
        pvt.valid = true;
        pvt.posEcef = Frames::geodeticToEcef(0.5, 0.2, 100.0);
        pvt.velEcef = Vec3(0, 0, 0);
        pvt.numSats = 5;
        pvt.pdop = 2.0; pvt.hdop = 1.5; pvt.vdop = 1.2;
        GpsTime t{2100, 10000.0};
        std::vector<SatelliteView> views(3);
        for (int i = 0; i < 3; ++i) {
            views[i].prn = i + 1;
            views[i].elevationRad = 0.5;
            views[i].azimuthRad = 0.5;
            views[i].slantRange = 20000000.0 + i;
            views[i].visible = true;
            views[i].state.posEcef = Frames::geodeticToEcef(0.3 + i * 0.1, 0.1, 20000000.0);
        }
        std::string gga = gen.gga(pvt, views);
        check(NmeaSentence::verify(gga), "GGA checksum verified");
        std::string rmc = gen.rmc(pvt, t);
        check(NmeaSentence::verify(rmc), "RMC checksum verified");
        std::string zda = gen.zda(t);
        check(NmeaSentence::verify(zda), "ZDA checksum verified");
        std::string gsa = gen.gsa(pvt, views);
        check(NmeaSentence::verify(gsa), "GSA checksum verified");
        std::string gsv = gen.gsv(views);
        check(NmeaSentence::verify(gsv), "GSV checksum verified");
    }

    // --- 6. Pseudorange + Doppler -------------------------------------------
    {
        SvState sv;
        sv.posEcef = Vec3(20000000.0, 5000000.0, 10000000.0);
        sv.velEcef = Vec3(2000.0, -1000.0, 500.0);
        sv.clockBias = 0.0;
        Vec3 rx(0, 0, 0);
        AtmosphericCorrections atm;  // all zero
        auto rho = Pseudorange::compute(sv, rx, 0.0, atm);
        check(rho.isOk() && close(rho.value(), sv.posEcef.norm(), 1e-3),
              "Pseudorange geometric range (within 1e-3 m)");
        auto dop = Doppler::compute(sv, rx, Vec3(0, 0, 0), 0.0);
        check(dop.isOk() && std::isfinite(dop.value()),
              "Doppler finite");
        // Non-finite input -> error.
        SvState bad;
        bad.posEcef = Vec3(0.0, 0.0, 0.0);  // zero range to co-located receiver
        // A zero-range pseudorange is degenerate and must be rejected (not crash).
        auto zeroRange = Pseudorange::compute(bad, Vec3(0, 0, 0), 0.0, atm);
        check(zeroRange.failed(), "Pseudorange rejects degenerate zero range");
        // A normal positive range computes successfully.
        SvState ok2;
        ok2.posEcef = Vec3(2.5e7, 0.0, 0.0);
        ok2.clockBias = 1.0e-6;
        auto pos = Pseudorange::compute(ok2, Vec3(0, 0, 0), 0.0, atm);
        check(pos.isOk() && pos.value() > 2.4e7 && pos.value() < 2.6e7,
              "Pseudorange positive range within bounds");
    }

    // --- 7. Error models (clock/ionosphere/troposphere) ----------------------
    {
        ClockModel cm;
        ClockModel::Config cfg{1.0e-4, 1.0e-10, 0.0};
        cm.setReceiver(cfg);
        check(close(cm.receiverBias(100.0), cfg.bias + cfg.drift * 100.0, 1e-9),
              "Clock model polynomial");

        Ionosphere iono;
        Ionosphere::KlobucharParams kp;
        kp.alpha[0] = 0.8382e-08; kp.alpha[1] = -0.7451e-08;
        kp.alpha[2] = -0.5960e-07; kp.alpha[3] = 0.1192e-06;
        kp.beta[0] = 0.8808e+05; kp.beta[1] = -0.3277e+05;
        kp.beta[2] = -0.1966e+06; kp.beta[3] = 0.1966e+06;
        iono.setKlobuchar(kp);
        SvState sv;
        sv.posEcef = Vec3(20000000.0, 0.0, 10000000.0);
        Vec3 rx = Frames::geodeticToEcef(0.6, 0.0, 0.0);
        GpsTime t{2100, 30000.0};
        auto ionoDelay = iono.delay(sv, rx, t);
        check(ionoDelay.isOk() && ionoDelay.value() >= 0.0 &&
                  ionoDelay.value() < 50.0,
              "Klobuchar iono delay in expected range");

        Troposphere tropo;
        tropo.setModel(Troposphere::Model::Saastamoinen);
        auto tropoDelay = tropo.delay(sv, rx);
        check(tropoDelay.isOk() && tropoDelay.value() > 0.0 &&
                  tropoDelay.value() < 50.0,
              "Saastamoinen tropo delay in expected range");

        ErrorModelConfig emc;
        emc.enableIonosphere(true);
        emc.setIonosphereModel(kp);
        emc.enableTroposphere(false);
        auto c = emc.compute(sv, rx, t);
        check(c.isOk() && c.value().iono >= 0.0 && c.value().tropo == 0.0,
              "Error model config toggling");
    }

    // --- 8. SBAS message + integrity -----------------------------------------
    {
        // Build a synthetic frame with a deliberately corrupted CRC field so the
        // parser must reject it (bits 226-249 are the CRC; set them non-zero
        // while the message bits stay zero, making expected != computed).
        std::vector<uint8_t> bytes(32, 0);
        bytes[28] = 0x3F;  // non-zero in the 24-bit CRC region
        auto m = SbasMessage::parse(bytes);
        check(m.failed(), "SBAS parse rejects bad CRC");

        // Integrity protection levels from a simple, non-degenerate geometry.
        // Vary elevation and azimuth so the geometry matrix is full rank.
        std::vector<SatelliteView> views;
        Vec3 rx(0, 0, 0);
        for (int i = 0; i < 6; ++i) {
            SatelliteView v;
            v.prn = i + 1;
            v.visible = true;
            double a = i * 6.28318530717958647692 / 6.0;
            double el = 0.35 + 0.1 * i;  // 20..55 deg, varied
            v.state.posEcef = Vec3(std::cos(el) * std::cos(a) * 2.0e7,
                                   std::cos(el) * std::sin(a) * 2.0e7,
                                   std::sin(el) * 2.0e7);
            views.push_back(v);
        }
        Integrity integrity;
        std::vector<double> udres(6, 1.0);
        auto pl = integrity.compute(views, udres, 1.0);
        check(pl.isOk() && pl.value().valid && pl.value().hpl > 0.0 &&
                  pl.value().vpl > 0.0,
              "Protection levels (HPL/VPL) computed");

        // Insufficient satellites -> error.
        std::vector<SatelliteView> few;
        for (int i = 0; i < 3; ++i) {
            SatelliteView v;
            v.prn = i + 1;
            v.visible = true;
            v.state.posEcef = Vec3(1.0e7, 0.0, 1.0e7);
            few.push_back(v);
        }
        check(integrity.compute(few, std::vector<double>(3, 1.0), 1.0).failed(),
              "Protection levels reject <4 sats");
    }

    // --- 9. Scenario facade integration (Item 7) ----------------------------
    {
        ScenarioConfig cfg;
        cfg.elevationMaskRad = 0.1;
        Scenario scen(cfg);
        // Add two GPS satellites from broadcast ephemeris.
        BroadcastEphemeris eph1;
        eph1.sqrtA = 5153.6556; eph1.e = 0.003; eph1.i0 = 0.959931;
        eph1.omega0 = -1.5707963; eph1.argp = 0.0; eph1.M0 = 0.0; eph1.toe = 0.0;
        BroadcastEphemeris eph2 = eph1;
        eph2.argp = 1.0; eph2.M0 = 2.0;
        scen.addGps(eph1, 1);
        scen.addGps(eph2, 2);
        Vec3 rx = Frames::geodeticToEcef(0.6, 0.0, 0.0);
        scen.setReceiver(rx, Vec3(0, 0, 0));
        GpsTime t{2100, 1000.0};
        auto epoch = scen.step(t);
        check(epoch.isOk(), "Scenario::step produces an epoch");
        if (epoch.isOk()) {
            check(!epoch.value().views.empty(), "Scenario epoch has satellite views");
            // With only 2 satellites PVT is expected to fail gracefully (>=4 needed),
            // but the epoch itself must still be valid.
            check(epoch.value().pvt.failed(), "Scenario handles <4 sats gracefully");
        }
    }

    if (failures == 0) {
        std::printf("SCENARIO SMOKE OK\n");
        return 0;
    }
    std::printf("SCENARIO SMOKE FAIL: %d check(s) failed\n", failures);
    return 1;
}

}  // namespace lodestar::scenario
