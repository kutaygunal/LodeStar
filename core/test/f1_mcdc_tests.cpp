// core/test/f1_mcdc_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TestForge 4.1: decision / MC-DC coverage via clang/llvm-cov tests.
//
// Test contract: docs/gap-fill-plan.md (Module 4.1).
//   (A) core/testforge/LlvmCovImport.h (+ .cpp) accepts a real llvm-cov JSON
//       report and populates decisions_total/conditions_total honestly (known
//       branch structures produce expected counts).
//   (B) Decision + MC/DC percentages are computed correctly from the counts.
//
// Deterministic (fixture llvm-cov JSON).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/testforge/Coverage.h"
#include "core/testforge/LlvmCovImport.h"

namespace tf = lodestar::testforge;

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

// A minimal llvm-cov JSON report with known branch counts.
// file1: 2 branches, both taken. file2: 2 branches, 1 taken.
const char* kReport =
    "{ \"files\": [\n"
    "  {\"filename\":\"a.c\", \"segments\":[[1,1,1],[2,1,1]], \"branches\":[\n"
    "    {\"line\":1,\"count\":1,\"taken\":true,\"unconditional\":false},\n"
    "    {\"line\":2,\"count\":1,\"taken\":true,\"unconditional\":false}]},\n"
    "  {\"filename\":\"b.c\", \"segments\":[[1,1,1],[2,1,0]], \"branches\":[\n"
    "    {\"line\":1,\"count\":1,\"taken\":true,\"unconditional\":false},\n"
    "    {\"line\":2,\"count\":0,\"taken\":false,\"unconditional\":false}]}\n"
    "] }";

}  // namespace

// ---------------------------------------------------------------------------
// T1. Parse llvm-cov JSON -> per-file decision/MC-DC counts
// ---------------------------------------------------------------------------
static void testParse(Harness& h) {
    h.section("T1. parse llvm-cov JSON -> decision/MC-DC counts");
    tf::LlvmCovImport imp;
    std::vector<tf::CoverageResult> rows;
    h.check(imp.parseJson(kReport, "RUN-1", rows), "parseJson() returns true");
    h.check(rows.size() == 2, "parses 2 files");
    if (rows.size() == 2) {
        // file a.c: 2 branches, 2 taken.
        h.check(rows[0].decisionsTotal == 2, "a.c decisions_total == 2");
        h.check(rows[0].decisionsTaken == 2, "a.c decisions_taken == 2");
        h.check(rows[0].conditionsTotal == 2, "a.c conditions_total == 2");
        h.check(rows[0].conditionsSatisfied == 2, "a.c conditions_satisfied == 2");
        // file b.c: 2 branches, 1 taken.
        h.check(rows[1].decisionsTotal == 2, "b.c decisions_total == 2");
        h.check(rows[1].decisionsTaken == 1, "b.c decisions_taken == 1");
        h.check(rows[1].conditionsSatisfied == 1, "b.c conditions_satisfied == 1");
        h.check(rows[0].runId == "RUN-1", "runId stamped on rows");
    }

    // Unparseable -> false.
    std::vector<tf::CoverageResult> none;
    h.check(!imp.parseJson("not json", "RUN-1", none),
            "parseJson() returns false on unparseable input");
}

// ---------------------------------------------------------------------------
// T2. Unconditional branches are excluded (decision coverage honesty)
// ---------------------------------------------------------------------------
static void testUnconditional(Harness& h) {
    h.section("T2. unconditional branches excluded");
    const char* rep =
        "{ \"files\": [\n"
        "  {\"filename\":\"c.c\", \"segments\":[[1,1,1]], \"branches\":[\n"
        "    {\"line\":1,\"count\":1,\"taken\":true,\"unconditional\":true},\n"
        "    {\"line\":2,\"count\":1,\"taken\":true,\"unconditional\":false}]}\n"
        "] }";
    tf::LlvmCovImport imp;
    std::vector<tf::CoverageResult> rows;
    h.check(imp.parseJson(rep, "R", rows), "parseJson() ok");
    if (rows.size() == 1) {
        h.check(rows[0].decisionsTotal == 1,
                "unconditional branch excluded (decisions_total == 1)");
    }
}

// ---------------------------------------------------------------------------
// T3. Percentage computation from counts
// ---------------------------------------------------------------------------
static void testPercentages(Harness& h) {
    h.section("T3. decision + MC/DC percentages");
    // 100% decision coverage.
    auto d1 = tf::computeDecisionCoverage(4, 4);
    h.check(d1 == 100.0, "decision 4/4 == 100%");
    // 50% decision coverage.
    auto d2 = tf::computeDecisionCoverage(2, 4);
    h.check(d2 == 50.0, "decision 2/4 == 50%");
    // 0 total -> 0.
    auto d3 = tf::computeDecisionCoverage(0, 0);
    h.check(d3 == 0.0, "decision 0/0 == 0%");
    // MC/DC.
    auto m1 = tf::computeMcdcCoverage(3, 4);
    h.check(m1 == 75.0, "mcdc 3/4 == 75%");
    auto m0 = tf::computeMcdcCoverage(0, 0);
    h.check(m0 == 0.0, "mcdc 0/0 == 0%");
}

// ---------------------------------------------------------------------------
// T4. Honest MC-DC: satisfied == taken branches (no overclaim)
// ---------------------------------------------------------------------------
static void testMcdcHonesty(Harness& h) {
    h.section("T4. MC-DC honestly = satisfied taken branches");
    tf::LlvmCovImport imp;
    std::vector<tf::CoverageResult> rows;
    imp.parseJson(kReport, "RUN-1", rows);
    if (rows.size() == 2) {
        // b.c has 1 of 2 conditions satisfied -> 50% MC-DC, honestly derived.
        auto pct = tf::computeMcdcCoverage(rows[1].conditionsSatisfied,
                                           rows[1].conditionsTotal);
        h.check(pct == 50.0, "b.c MC-DC == 50% (1/2 conditions)");
        h.check(rows[1].conditionsSatisfied <= rows[1].conditionsTotal,
                "conditions_satisfied never exceeds total (no overclaim)");
    }
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill TestForge 4.1 decision/MC-DC coverage");
    testParse(h);
    testUnconditional(h);
    testPercentages(h);
    testMcdcHonesty(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
