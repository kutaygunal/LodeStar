// core/test/s4_multignss_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.5: first-party baseband / multi-GNSS synthesis tests.
//
// Test contract: docs/gap-fill-plan.md (Module 5.5). Full multi-GNSS baseband
// (GPS / GLONASS / BeiDou) as a software baseband / SDR-ready signal model with
// per-constellation carrier, chip rate, and code — clearly scoped (no RF
// overclaim). Produces valid I/Q frames per constellation.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cmath>

#include "core/scenario/MultiGnssBaseband.h"

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
// T1. Per-constellation default parameters
// ---------------------------------------------------------------------------
static void testDefaults(Harness& h) {
    h.section("T1. per-constellation defaults");
    h.check(s::gnssSystemName(s::GnssSystem::Gps) == "GPS", "GPS name");
    h.check(s::gnssSystemName(s::GnssSystem::Glonass) == "GLONASS", "GLONASS name");
    h.check(s::gnssSystemName(s::GnssSystem::BeiDou) == "BeiDou", "BeiDou name");

    double gps = s::MultiGnssBaseband::defaultCarrierHz(s::GnssSystem::Gps);
    double glo = s::MultiGnssBaseband::defaultCarrierHz(s::GnssSystem::Glonass);
    double bds = s::MultiGnssBaseband::defaultCarrierHz(s::GnssSystem::BeiDou);
    h.check(gps > 1.5e9 && gps < 1.6e9, "GPS L1 ~1.575 GHz");
    h.check(glo > 1.5e9 && glo < 1.7e9, "GLONASS L1 ~1.6 GHz");
    h.check(bds > 1.5e9 && bds < 1.6e9, "BeiDou B1 ~1.56 GHz");
    // Carriers are distinct per constellation.
    h.check(gps != glo && glo != bds, "carriers differ across constellations");

    h.check(s::MultiGnssBaseband::defaultChipRateHz(s::GnssSystem::Gps) == 1.023e6,
            "GPS C/A chip rate 1.023 MHz");
    h.check(s::MultiGnssBaseband::defaultChipRateHz(s::GnssSystem::BeiDou) == 2.046e6,
            "BeiDou B1I chip rate 2.046 MHz");
}

// ---------------------------------------------------------------------------
// T2. Synthesize valid I/Q frames per constellation
// ---------------------------------------------------------------------------
static void testSynthesize(Harness& h) {
    h.section("T2. valid I/Q frames per constellation");
    for (auto sys : {s::GnssSystem::Gps, s::GnssSystem::Glonass,
                     s::GnssSystem::BeiDou}) {
        auto frame = s::MultiGnssBaseband::synthesize(sys, 8000.0, 0.01, 3);
        h.check(s::MultiGnssBaseband::frameIsValid(frame),
                (std::string(s::gnssSystemName(sys)) + " frame is valid").c_str());
        h.check(frame.samples.size() == 80,
                (std::string(s::gnssSystemName(sys)) +
                 " produces ~80 samples at 8 kHz over 10 ms").c_str());
        h.check(frame.system == sys, "frame system matches");
    }
}

// ---------------------------------------------------------------------------
// T3. Determinism
// ---------------------------------------------------------------------------
static void testDeterminism(Harness& h) {
    h.section("T3. determinism");
    auto a = s::MultiGnssBaseband::synthesize(s::GnssSystem::Gps, 8000.0, 0.01, 2);
    auto b = s::MultiGnssBaseband::synthesize(s::GnssSystem::Gps, 8000.0, 0.01, 2);
    bool same = a.samples.size() == b.samples.size();
    for (std::size_t i = 0; same && i < a.samples.size() && i < b.samples.size(); ++i) {
        if (a.samples[i].i != b.samples[i].i || a.samples[i].q != b.samples[i].q)
            same = false;
    }
    h.check(same, "synthesis is deterministic (identical frames)");
}

// ---------------------------------------------------------------------------
// T4. Invalid input -> empty frame (scoped, no overclaim)
// ---------------------------------------------------------------------------
static void testInvalid(Harness& h) {
    h.section("T4. invalid input + scoping (no RF overclaim)");
    auto bad = s::MultiGnssBaseband::synthesize(s::GnssSystem::Gps, 0.0, 1.0, 1);
    h.check(bad.samples.empty(), "zero sample rate -> empty frame");
    auto zero = s::MultiGnssBaseband::synthesize(s::GnssSystem::Gps, 8000.0, 0.0, 1);
    h.check(zero.samples.empty(), "zero duration -> empty frame");
    // No channels -> empty frame.
    auto none = s::MultiGnssBaseband::synthesize(s::GnssSystem::Gps, 8000.0, 0.01, 0);
    h.check(none.samples.empty(), "zero channels -> empty frame");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill ScenarioForge 5.5 multi-GNSS baseband");
    testDefaults(h);
    testSynthesize(h);
    testDeterminism(h);
    testInvalid(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
