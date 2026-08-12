// core/test/s2_phase11_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 11 tests (test contract): ScenarioForge software-defined baseband
// (I/Q sample generation) + automation API (SCPI-style remote control).
//
// Written by the scrum-master BEFORE the Phase 11 engineer implements the
// feature. The engineer must implement the contract below so these tests
// compile and pass. Do NOT weaken the assertions; implement the feature to
// satisfy them.
//
// Covers (PLAN.md, S2 Phase 11):
//   (A) I/Q baseband generator: generateBaseband(scenario, carrierHz,
//       sampleRate, durationSec) -> vector of complex I/Q samples.
//   (B) Automation API: startScenario / stopScenario / configure / query.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 11 engineer must provide.
// ---------------------------------------------------------------------------
// (A) core/scenario/Baseband.h:
//   struct IqSample { double i; double q; };
//   std::vector<IqSample> generateBaseband(const Scenario& scenario,
//                                          double carrierHz,
//                                          double sampleRate,
//                                          double durationSec);
//   The number of returned samples is approximately sampleRate * durationSec.
//
// (B) core/scenario/AutomationApi.h:
//   class AutomationApi {
//   public:
//       common::Result<std::string> startScenario(const std::string& scenarioId);
//       common::Result<void> stopScenario(const std::string& handle);
//       common::Result<void> configure(const std::string& handle,
//                                      const std::string& key,
//                                      const std::string& value);
//       common::Result<std::string> query(const std::string& scpi);
//       bool isRunning(const std::string& handle) const;
//   };
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/scenario/AutomationApi.h"
#include "core/scenario/Baseband.h"
#include "core/scenario/Scenario.h"
#include "core/scenario/frames/Frames.h"

using lodestar::scenario::AutomationApi;
using lodestar::scenario::BroadcastEphemeris;
using lodestar::scenario::Frames;
using lodestar::scenario::generateBaseband;
using lodestar::scenario::Scenario;
using lodestar::scenario::ScenarioConfig;
using lodestar::scenario::Vec3;

namespace {

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// Builds a small two-satellite GPS scenario with a receiver (mirrors the
// Phase 4 smoke path) so the baseband has real satellite geometry.
Scenario makeScenario() {
    ScenarioConfig cfg;
    cfg.elevationMaskRad = 0.1;
    Scenario scen(cfg);

    BroadcastEphemeris eph1;
    eph1.sqrtA = 5153.6556;
    eph1.e = 0.003;
    eph1.i0 = 0.959931;
    eph1.omega0 = -1.5707963;
    eph1.argp = 0.0;
    eph1.M0 = 0.0;
    eph1.toe = 0.0;
    BroadcastEphemeris eph2 = eph1;
    eph2.argp = 1.0;
    eph2.M0 = 2.0;
    scen.addGps(eph1, 1);
    scen.addGps(eph2, 2);

    Vec3 rx = Frames::geodeticToEcef(0.6, 0.0, 0.0);
    scen.setReceiver(rx, Vec3(0, 0, 0));
    return scen;
}

// ---------------------------------------------------------------------------
// T1. baseband produces the expected number of samples
// ---------------------------------------------------------------------------
void testBasebandSampleCount(Harness& h) {
    h.section("T1. baseband produces the expected number of samples");

    Scenario scen = makeScenario();
    auto samples = generateBaseband(scen, 1575.42e6, 10e6, 0.001);

    const std::size_t expected = 10000;  // 10e6 * 0.001
    const std::size_t tol = 100;
    h.check(samples.size() >= expected - tol && samples.size() <= expected + tol,
            "generateBaseband(..., 1575.42e6, 10e6, 0.001) size ~= 10000");
    h.check(!samples.empty(), "sample vector is non-empty");
}

// ---------------------------------------------------------------------------
// T2. baseband samples are non-trivial (not all zeros)
// ---------------------------------------------------------------------------
void testBasebandNonTrivial(Harness& h) {
    h.section("T2. baseband samples are non-trivial (not all zeros)");

    Scenario scen = makeScenario();
    auto samples = generateBaseband(scen, 1575.42e6, 10e6, 0.001);

    double energy = 0.0;
    for (const auto& s : samples) energy += s.i * s.i + s.q * s.q;
    h.check(energy > 0.0, "baseband has non-zero signal energy");

    bool anyNonZero = false;
    for (const auto& s : samples) {
        if (s.i != 0.0 || s.q != 0.0) {
            anyNonZero = true;
            break;
        }
    }
    h.check(anyNonZero, "at least one I/Q sample is non-zero");
}

// ---------------------------------------------------------------------------
// T3. automation API can start a scenario
// ---------------------------------------------------------------------------
void testStartScenario(Harness& h) {
    h.section("T3. automation API can start a scenario");

    AutomationApi api;
    auto handle = api.startScenario("scen-1");
    h.check(handle.isOk(), "startScenario(scen-1) ok");
    h.check(handle.isOk() && !handle.value().empty(),
            "startScenario returns a non-empty handle");
    h.check(handle.isOk() && api.isRunning(handle.value()),
            "scenario is running after start");

    auto status = api.query("SYST:STAT?");
    h.check(status.isOk() && status.value() == "RUN",
            "SYST:STAT? reports RUN after start");
}

// ---------------------------------------------------------------------------
// T4. automation API can stop a scenario
// ---------------------------------------------------------------------------
void testStopScenario(Harness& h) {
    h.section("T4. automation API can stop a scenario");

    AutomationApi api;
    auto handle = api.startScenario("scen-2");
    h.check(handle.isOk(), "startScenario(scen-2) ok");
    if (!handle.isOk()) return;

    auto stopped = api.stopScenario(handle.value());
    h.check(stopped.isOk(), "stopScenario(handle) ok");
    h.check(!api.isRunning(handle.value()),
            "scenario marked stopped after stop");

    auto status = api.query("SYST:STAT?");
    h.check(status.isOk() && status.value() == "STOP",
            "SYST:STAT? reports STOP after stop");

    auto bad = api.stopScenario("nope");
    h.check(bad.failed(), "stopScenario(unknown handle) fails");
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    Harness h("S2 Phase 11 ScenarioForge baseband + automation API");
    std::printf("S2 PHASE 11 TESTS\n");

    testBasebandSampleCount(h);
    testBasebandNonTrivial(h);
    testStartScenario(h);
    testStopScenario(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
