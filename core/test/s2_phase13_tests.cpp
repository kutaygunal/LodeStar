// core/test/s2_phase13_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 13: AI quality scoring on requirements tests.
//
// Test contract: docs/s2-phase13-test.md (written by the scrum-master BEFORE the
// Phase 13 engineer implements the feature). The engineer must implement the
// contract so a requirement is scored on quality dimensions (clarity,
// testability, atomicity, completeness, ambiguity) each 0-100 plus an overall
// score, using the LLM adapter with a deterministic fallback. Do NOT weaken the
// assertions to make them pass; implement the feature to satisfy them.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// No DB required. All tests are deterministic and must always pass.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/adapters/Adapter.h"
#include "core/adapters/MockAdapter.h"
#include "core/riskai/RequirementQualityService.h"
#include "core/tracelink/Types.h"

namespace ad = lodestar::adapters;
namespace ra = lodestar::riskai;
namespace tl = lodestar::tracelink;

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

// Build a requirement entity with the given body text.
tl::Entity makeRequirement(const std::string& text) {
    tl::Entity e;
    e.id = "req-1";
    e.externalId = "REQ-1";
    e.type = tl::EntityType::Requirement;
    e.name = text;
    e.text = text;
    return e;
}

// Validate every dimension is present and in [0,100].
bool scoreValid(const ra::QualityScore& s) {
    auto inRange = [](int v) { return v >= 0 && v <= 100; };
    return inRange(s.clarity) && inRange(s.testability) &&
           inRange(s.atomicity) && inRange(s.completeness) &&
           inRange(s.ambiguity) && inRange(s.overall);
}

// ---------------------------------------------------------------------------
// T1. scoreRequirement returns all dimensions.
// ---------------------------------------------------------------------------
void testT1(Harness& h) {
    h.section("T1 scoreRequirement returns all dimensions in [0,100]");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RequirementQualityService svc(mock, cfg);

    auto s = svc.scoreRequirement(makeRequirement(
        "The system shall display the speed in km/h."));
    h.check(scoreValid(s), "all 5 dimensions + overall present and in [0,100]");
}

// ---------------------------------------------------------------------------
// T2. overall score is in range.
// ---------------------------------------------------------------------------
void testT2(Harness& h) {
    h.section("T2 overall score is in [0,100]");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RequirementQualityService svc(mock, cfg);

    auto s = svc.scoreRequirement(makeRequirement(
        "The system shall display the speed in km/h."));
    h.check(s.overall >= 0 && s.overall <= 100,
            "overall in [0,100]");
}

// ---------------------------------------------------------------------------
// T3. a well-formed requirement scores higher than a vague one.
// ---------------------------------------------------------------------------
void testT3(Harness& h) {
    h.section("T3 well-formed requirement scores higher than vague one");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RequirementQualityService svc(mock, cfg);

    auto good = svc.scoreRequirement(makeRequirement(
        "The system shall display the speed in km/h."));
    auto vague = svc.scoreRequirement(makeRequirement("Handle stuff."));

    std::printf("  [INFO] well-formed overall=%d, vague overall=%d\n",
                good.overall, vague.overall);
    h.check(good.overall > vague.overall,
            "well-formed overall > vague overall");
}

// ---------------------------------------------------------------------------
// T4. deterministic fallback works without LLM.
// ---------------------------------------------------------------------------
void testT4(Harness& h) {
    h.section("T4 deterministic fallback works without LLM");
    // MockAdapter throws AdapterError on the "complete" op, so the LLM path is
    // unavailable and the service must fall back to the deterministic heuristic.
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RequirementQualityService svc(mock, cfg);

    auto s = svc.scoreRequirement(makeRequirement(
        "The system shall display the speed in km/h."));
    h.check(scoreValid(s),
            "fallback returns valid score (all dimensions in [0,100])");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    (void)argc;
    (void)argv;

    Harness h("S2 Phase 13 AI quality scoring");
    std::printf("S2 PHASE 13 AI QUALITY SCORING TESTS\n");

    testT1(h);
    testT2(h);
    testT3(h);
    testT4(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
