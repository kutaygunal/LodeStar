// core/test/a2_live_coverage_view_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill AssureCheck 2.3: unified live coverage view tests.
//
// Test contract: docs/gap-fill-plan.md (Module 2.3).
//   (A) CoverageComplianceView aggregates measured coverage (statement today;
//       decision/MC-DC when tooling lands) into a single dashboard.
//   (B) Objective aggregation correctness: maps coverage -> DO-178C
//       verification objectives (A-3..A-7) with per-objective status
//       compliant / partial / no-evidence, honestly (no fabricated measured
//       values where tooling is absent).
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/assurecheck/CoverageComplianceView.h"
#include "core/assurecheck/StandardsContentService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

#ifndef LODESTAR_ASSURECHECK_DATA_DIR
#define LODESTAR_ASSURECHECK_DATA_DIR "core/assurecheck/data"
#endif

namespace ac = lodestar::assurecheck;
namespace p  = lodestar::persistence;

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
// T1. Aggregation correctness
// ---------------------------------------------------------------------------
static void testAggregate(Harness& h) {
    h.section("T1. coverage aggregation");
    std::vector<ac::CoverageMeasurement> m;
    ac::CoverageMeasurement a;
    a.scope = "m1"; a.statementsExecuted = 80; a.statementsTotal = 100;
    a.decisionsTaken = 30; a.decisionsTotal = 40;
    ac::CoverageMeasurement b;
    b.scope = "m2"; b.statementsExecuted = 20; b.statementsTotal = 50;
    a.conditionsSatisfied = 10; a.conditionsTotal = 20;
    m.push_back(a); m.push_back(b);

    auto agg = ac::CoverageComplianceView::aggregate(m);
    h.check(agg.statementsExecuted == 100, "statements executed summed (80+20)");
    h.check(agg.statementsTotal == 150, "statements total summed (100+50)");
    h.check(agg.decisionsTaken == 30, "decisions taken summed");
    h.check(agg.decisionsTotal == 40, "decisions total summed");
    h.check(agg.conditionsSatisfied == 10, "conditions satisfied summed");
    h.check(agg.conditionsTotal == 20, "conditions total summed");

    // Empty aggregation.
    auto empty = ac::CoverageComplianceView::aggregate({});
    h.check(empty.statementsTotal == 0, "empty aggregate has 0 total");
}

// ---------------------------------------------------------------------------
// T2. Honest objective mapping (no tooling => no-evidence)
// ---------------------------------------------------------------------------
static void testNoTooling(Harness& h) {
    h.section("T2. no tooling => no-evidence (honest)");
    ac::StandardsContentService content;
    auto b = content.loadBundle(
        std::string(LODESTAR_ASSURECHECK_DATA_DIR) + "/do178c_standards.json");
    if (!b.isOk()) { h.check(false, "loadBundle() ok"); return; }

    ac::CoverageAggregate none;  // all zero
    auto obj = ac::CoverageComplianceView::mapToObjectives(b.value(), none, "A");
    h.check(!obj.empty(), "maps verification objectives for DAL A");
    // Every mapped objective must be "no-evidence" (nothing measured).
    bool allNoEvidence = true;
    for (const auto& o : obj) if (o.status != "no-evidence") allNoEvidence = false;
    h.check(allNoEvidence, "all objectives no-evidence with no measurements");
    // Evidence links empty (no fabricated measured lines).
    bool anyLink = false;
    for (const auto& o : obj) if (!o.evidenceLinks.empty()) anyLink = true;
    h.check(!anyLink, "no fabricated evidence links when nothing measured");
}

// ---------------------------------------------------------------------------
// T3. High coverage => compliant; low => partial
// ---------------------------------------------------------------------------
static void testCompliantPartial(Harness& h) {
    h.section("T3. compliant vs partial status");
    ac::StandardsContentService content;
    auto b = content.loadBundle(
        std::string(LODESTAR_ASSURECHECK_DATA_DIR) + "/do178c_standards.json");
    if (!b.isOk()) { h.check(false, "loadBundle() ok"); return; }

    // 90% statement coverage, decision tooling present and 90%.
    ac::CoverageAggregate good;
    good.statementsExecuted = 90; good.statementsTotal = 100;
    good.decisionsTaken = 36; good.decisionsTotal = 40;
    auto goodObj = ac::CoverageComplianceView::mapToObjectives(b.value(), good, "A");
    h.check(!goodObj.empty(), "maps objectives for good coverage");
    bool anyCompliant = false, anyNoEvidence = false;
    for (const auto& o : goodObj) {
        if (o.status == "compliant") anyCompliant = true;
        if (o.status == "no-evidence") anyNoEvidence = true;
    }
    h.check(anyCompliant, "high coverage yields compliant objectives");
    h.check(!anyNoEvidence, "high coverage yields no no-evidence objectives");
    // Evidence link present describing measured percentage.
    bool hasLink = false;
    for (const auto& o : goodObj)
        if (!o.evidenceLinks.empty() &&
            o.evidenceLinks[0].find("statement coverage 90%") != std::string::npos)
            hasLink = true;
    h.check(hasLink, "evidence link states the measured statement percentage");

    // Low coverage => partial.
    ac::CoverageAggregate low;
    low.statementsExecuted = 10; low.statementsTotal = 100;
    auto lowObj = ac::CoverageComplianceView::mapToObjectives(b.value(), low, "A");
    bool anyPartial = false;
    for (const auto& o : lowObj) if (o.status == "partial") anyPartial = true;
    h.check(anyPartial, "low coverage yields partial objectives");
}

// ---------------------------------------------------------------------------
// T4. DAL applicability: A-3 objectives present for A, absent for E
// ---------------------------------------------------------------------------
static void testDalCoverage(Harness& h) {
    h.section("T4. DAL applicability in the coverage view");
    ac::StandardsContentService content;
    auto b = content.loadBundle(
        std::string(LODESTAR_ASSURECHECK_DATA_DIR) + "/do178c_standards.json");
    if (!b.isOk()) { h.check(false, "loadBundle() ok"); return; }

    ac::CoverageAggregate some;
    some.statementsExecuted = 50; some.statementsTotal = 100;

    auto forA = ac::CoverageComplianceView::mapToObjectives(b.value(), some, "A");
    auto forE = ac::CoverageComplianceView::mapToObjectives(b.value(), some, "E");
    // DO-178C verification objectives (A-3) are DAL A-D, so for a project at
    // DAL E they do not apply -> empty view (honest: no applicable objectives).
    h.check(!forA.empty(), "coverage view maps objectives for DAL A");
    h.check(forE.empty(), "coverage view has no A-3 objectives for DAL E");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill AssureCheck 2.3 unified live coverage view");
    testAggregate(h);
    testNoTooling(h);
    testCompliantPartial(h);
    testDalCoverage(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
