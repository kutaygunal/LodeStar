// core/test/s2_interference_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill ScenarioForge 5.4: advanced interference & multipath depth tests.
//
// Test contract: docs/gap-fill-plan.md (Module 5.4).
//   (A) core/scenario/RfImpairments.h (+ .cpp) extends RF-impairment modeling:
//       matched-spectrum interferer, CW/AWGN/jamming/spoofing scenarios, and
//       data-driven environment models (urban canyon, ground reflection).
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <vector>

#include "core/scenario/RfImpairments.h"

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

std::vector<s::IqSample> makeSignal(std::size_t n) {
    std::vector<s::IqSample> s(n);
    for (std::size_t i = 0; i < n; ++i) {
        s[i].i = static_cast<double>(i % 7) * 0.1;
        s[i].q = static_cast<double>(i % 5) * 0.1;
    }
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// T1. Matched-spectrum interferer preserves length and alters the signal
// ---------------------------------------------------------------------------
static void testMatched(Harness& h) {
    h.section("T1. matched-spectrum interferer");
    auto in = makeSignal(20);
    s::InterferenceSource src;
    src.kind = s::InterferenceKind::MatchedSpectrum;
    src.amplitude = 0.5;
    src.startSample = 0;
    src.endSample = 19;
    auto out = s::applyInterferenceSource(in, src);
    h.check(out.size() == in.size(), "output length preserved");
    // With matched spectrum, sample 1 is scaled by (1+0.5).
    h.check(out[1].i > in[1].i, "matched-spectrum boosts the signal");
}

// ---------------------------------------------------------------------------
// T2. CW tone at a carrier offset
// ---------------------------------------------------------------------------
static void testCw(Harness& h) {
    h.section("T2. continuous-wave tone");
    auto in = makeSignal(20);
    s::InterferenceSource src;
    src.kind = s::InterferenceKind::ContinuousWave;
    src.amplitude = 0.8;
    src.frequencyHz = 1000.0;
    src.startSample = 0;
    src.endSample = 19;
    auto out = s::applyInterferenceSource(in, src);
    h.check(out.size() == in.size(), "output length preserved");
    bool altered = false;
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i].i != in[i].i || out[i].q != in[i].q) altered = true;
    h.check(altered, "CW tone alters the sample stream");
}

// ---------------------------------------------------------------------------
// T3. AWGN + jamming are deterministic and bounded by the amplitude window
// ---------------------------------------------------------------------------
static void testNoiseAndJam(Harness& h) {
    h.section("T3. AWGN + jamming determinism + amplitude");
    auto in = makeSignal(400);
    s::InterferenceSource awgn;
    awgn.kind = s::InterferenceKind::Awgn;
    awgn.amplitude = 0.2;
    awgn.startSample = 0;
    awgn.endSample = 399;
    auto a = s::applyInterferenceSource(in, awgn);
    auto b = s::applyInterferenceSource(in, awgn);
    bool same = true;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].i != b[i].i || a[i].q != b[i].q) same = false;
    h.check(same, "AWGN is deterministic");

    // Jamming injects higher relative power than AWGN at the same amplitude
    // (compare the injected delta energy, i.e. |out - in|, which is the
    // interference contribution independent of the base signal).
    s::InterferenceSource jam;
    jam.kind = s::InterferenceKind::Jamming;
    jam.amplitude = 0.2;
    jam.startSample = 0;
    jam.endSample = 399;
    auto j = s::applyInterferenceSource(in, jam);
    double awgnDelta = 0.0, jamDelta = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        awgnDelta += (a[i].i - in[i].i) * (a[i].i - in[i].i) +
                     (a[i].q - in[i].q) * (a[i].q - in[i].q);
        jamDelta += (j[i].i - in[i].i) * (j[i].i - in[i].i) +
                    (j[i].q - in[i].q) * (j[i].q - in[i].q);
    }
    h.check(jamDelta >= awgnDelta,
            "jamming injected energy >= AWGN at same amplitude");
}

// ---------------------------------------------------------------------------
// T4. Spoofing injects a false (delayed) signal
// ---------------------------------------------------------------------------
static void testSpoofing(Harness& h) {
    h.section("T4. spoofing false signal injection");
    auto in = makeSignal(20);
    s::InterferenceSource src;
    src.kind = s::InterferenceKind::Spoofing;
    src.amplitude = 0.9;
    src.startSample = 10;
    src.endSample = 19;
    auto out = s::applyInterferenceSource(in, src);
    // At sample 10, the spoofing copy is drawn from sample 5 (delayed by 5).
    h.check(out[10].i != in[10].i, "spoofing alters the sample after the start");
    // Before startSample, unchanged.
    h.check(out[5].i == in[5].i, "signal unchanged before the spoofing window");
}

// ---------------------------------------------------------------------------
// T5. Environment models (urban canyon / ground reflection)
// ---------------------------------------------------------------------------
static void testEnvironment(Harness& h) {
    h.section("T5. data-driven environment models");
    auto urban = s::urbanCanyonModel(0.5);
    h.check(urban.size() == 3, "urban canyon model has 3 reflection paths");
    h.check(urban[0].gain == 0.5, "primary path gain == ground gain");

    auto ground = s::groundReflectionModel(0.4);
    h.check(ground.size() == 1 && ground[0].gain == 0.4,
            "ground reflection model has 1 path");

    auto in = makeSignal(50);
    auto out = s::applyEnvironment(in, urban);
    h.check(out.size() == in.size(), "environment model output length preserved");
    bool altered = false;
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i].i != in[i].i) altered = true;
    h.check(altered, "urban canyon multipath alters the signal");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill ScenarioForge 5.4 advanced interference/multipath");
    testMatched(h);
    testCw(h);
    testNoiseAndJam(h);
    testSpoofing(h);
    testEnvironment(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
