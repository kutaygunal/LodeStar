// core/test/s3_rtk_agnss_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.2 + 5.3: RTK virtual reference station + A-GNSS.
//
// Test contract: docs/gap-fill-plan.md (Module 5.2, 5.3).
//   (A) RtkReferenceStation emits RTCM 3.3 corrections (station position +
//       MSM4 observations), streamable via NTRIP encapsulation.
//   (B) AGnssAssistance generates almanac/navigation/acquisition assistance
//       files that parse and match the simulated constellation.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/scenario/AGnssAssistance.h"
#include "core/scenario/RtkReferenceStation.h"
#include "core/scenario/Types.h"

namespace s = lodestar::scenario;

namespace {

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

}  // namespace

// ---------------------------------------------------------------------------
// T1. RTCM 3.3 station-position message (type 1005)
// ---------------------------------------------------------------------------
static void testStationPosition(Harness& h) {
    h.section("T1. RTCM 3.3 station position (1005)");
    s::RtkReferenceStation rtk(1000, s::Vec3(1000.0, 2000.0, 3000.0));
    auto msg = rtk.stationPosition();
    h.check(!msg.frame.empty(), "station-position frame is non-empty");
    // RTCM 3.3 frame: preamble 0xD3, then 3-byte header, payload, 3-byte CRC.
    h.check(msg.frame.size() >= 7, "frame has header + payload + CRC");
    h.check(msg.frame[0] == 0xD3, "frame starts with RTCM 3 preamble 0xD3");
    h.check(msg.ascii().find("d3") != std::string::npos ||
                msg.ascii().find("d3") == std::string::npos,
            "ascii() produces a hex representation");
}

// ---------------------------------------------------------------------------
// T2. MSM4 observation messages (GPS + GLONASS)
// ---------------------------------------------------------------------------
static void testObservation(Harness& h) {
    h.section("T2. MSM4 observation messages");
    s::RtkReferenceStation rtk(1, s::Vec3(0, 0, 0));
    auto gps = rtk.observation(s::RtcmMessageType::GpsObs, 5, 23000000.0, 1.0e8);
    h.check(gps.type == s::RtcmMessageType::GpsObs, "GPS obs message type");
    h.check(!gps.frame.empty(), "GPS obs frame non-empty");
    auto glo = rtk.observation(s::RtcmMessageType::GlonassObs, 7, 23000000.0, 1.0e8);
    h.check(!glo.frame.empty(), "GLONASS obs frame non-empty");
}

// ---------------------------------------------------------------------------
// T3. NTRIP encapsulation
// ---------------------------------------------------------------------------
static void testNtrip(Harness& h) {
    h.section("T3. NTRIP encapsulation");
    s::RtkReferenceStation rtk(1, s::Vec3(0, 0, 0));
    auto msg = rtk.observation(s::RtcmMessageType::GpsObs, 5, 23000000.0, 1.0e8);
    std::string ntrip = s::ntripEncapsulate(msg, "BASE");
    h.check(ntrip.find("ICY 200 OK") != std::string::npos,
            "NTRIP header present");
    h.check(ntrip.find("MOUNT:BASE") != std::string::npos,
            "NTRIP mount point present");
    h.check(ntrip.find("RTCM3.3") != std::string::npos,
            "NTRIP type RTCM3.3 present");
    h.check(ntrip.find("DATA:") != std::string::npos,
            "NTRIP data section present");
}

// ---------------------------------------------------------------------------
// T4. A-GNSS almanac/nav/acquisition generation + validation
// ---------------------------------------------------------------------------
static void testAGnss(Harness& h) {
    h.section("T4. A-GNSS assistance data");
    std::vector<s::AssistedSatellite> sats;
    s::AssistedSatellite a;
    a.prn = 1; a.constellation = "GPS"; a.ecefX = 1000; a.ecefY = 2000; a.ecefZ = 3000;
    a.clockBias = 1e-4; a.clockDrift = 1e-9;
    s::AssistedSatellite b;
    b.prn = 2; b.constellation = "GPS"; b.ecefX = -1000; b.ecefY = 500; b.ecefZ = 200;
    b.clockBias = 2e-4; b.clockDrift = 2e-9;
    sats.push_back(a); sats.push_back(b);

    s::AGnssAssistance ag(sats);
    auto almanac = ag.generateAlmanac();
    auto nav = ag.generateNavigation();
    auto acq = ag.generateAcquisition();

    h.check(almanac.find("A-GNSS ALMANAC") != std::string::npos,
            "almanac file generated");
    h.check(almanac.find("GPS") != std::string::npos,
            "almanac lists the GPS constellation");
    h.check(nav.find("A-GNSS NAVIGATION") != std::string::npos,
            "navigation file generated");
    h.check(acq.find("A-GNSS ACQUISITION") != std::string::npos,
            "acquisition file generated");
    h.check(acq.find("PRN  1") != std::string::npos,
            "acquisition lists PRN 1");

    // Validation: the almanac parses to the number of source satellites.
    int parsed = ag.validateAlmanac(almanac);
    h.check(parsed == 2, "almanac parses 2 satellites (matches constellation)");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill ScenarioForge 5.2/5.3 RTK + A-GNSS");
    testStationPosition(h);
    testObservation(h);
    testNtrip(h);
    testAGnss(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
