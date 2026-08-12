// core/test/s2_phase5_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 5 (TestForge test-case design intelligence) unit tests.
//
// Written by the scrum-master BEFORE the Phase 5 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase5-test.md): equivalence-class + boundary-value
// derivation from a requirement/objective, and concrete test-case generation.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 5 engineer must provide (in core/testforge/PlanGenerator.h):
//   struct EquivalenceClass { label; min; max; valid; }
//   struct BoundaryValue    { value; label; }
//   struct Requirement      { name; objective; min; max; hasRange; booleanConditions; }
//   struct TestCaseStep     { id; seq; name; description; metric; inputValue;
//                             expectedValue; expectedResult; }
//   struct TestCase         { id; name; objective; expectedResult; steps; }
//   PlanGenerator::deriveEquivalenceClasses(min, max) -> vector<EquivalenceClass>
//   PlanGenerator::deriveBoundaryValues(min, max)     -> vector<BoundaryValue>
//   PlanGenerator::generateTestCases(req)             -> vector<TestCase>
//   testCaseReferences(tc, value)                    -> bool
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/testforge/PlanGenerator.h"

namespace tf = lodestar::testforge;

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

bool hasClass(const std::vector<tf::EquivalenceClass>& classes,
              const char* label, bool valid) {
    for (const auto& c : classes) {
        if (c.label == label && c.valid == valid) return true;
    }
    return false;
}

bool hasValue(const std::vector<tf::BoundaryValue>& values, double v) {
    for (const auto& b : values) {
        if (b.value == v) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// T1. equivalence classes derived for a range
// ---------------------------------------------------------------------------
void testEquivalenceClasses(Harness& h) {
    h.section("T1. equivalence classes derived for a range");

    tf::PlanGenerator gen;
    auto classes = gen.deriveEquivalenceClasses(0.0, 120.0);

    h.check(classes.size() >= 3, "at least 3 equivalence classes derived");
    h.check(hasClass(classes, "valid", true),
            "a valid class [0,120] is present");
    h.check(hasClass(classes, "below-min", false),
            "an invalid below-min class is present");
    h.check(hasClass(classes, "above-max", false),
            "an invalid above-max class is present");

    // The valid class must span the full range.
    for (const auto& c : classes) {
        if (c.label == "valid" && c.valid) {
            h.check(c.min == 0.0 && c.max == 120.0,
                    "valid class spans [0,120]");
        }
    }
}

// ---------------------------------------------------------------------------
// T2. boundary values derived
// ---------------------------------------------------------------------------
void testBoundaryValues(Harness& h) {
    h.section("T2. boundary values derived");

    tf::PlanGenerator gen;
    auto values = gen.deriveBoundaryValues(0.0, 120.0);

    h.check(hasValue(values, 0.0), "boundary value 0 (min) present");
    h.check(hasValue(values, 1.0), "boundary value 1 (min+1) present");
    h.check(hasValue(values, 60.0), "boundary value 60 (nominal) present");
    h.check(hasValue(values, 119.0), "boundary value 119 (max-1) present");
    h.check(hasValue(values, 120.0), "boundary value 120 (max) present");
    h.check(hasValue(values, -1.0), "boundary value -1 (min-1, just-outside) present");
    h.check(hasValue(values, 121.0), "boundary value 121 (max+1, just-outside) present");
}

// ---------------------------------------------------------------------------
// T3. test cases generated from classes
// ---------------------------------------------------------------------------
void testGenerateTestCases(Harness& h) {
    h.section("T3. test cases generated from classes");

    tf::PlanGenerator gen;
    tf::Requirement req;
    req.name = "Speed limit";
    req.objective = "Vehicle speed must stay within [0,120] km/h.";
    req.min = 0.0;
    req.max = 120.0;
    req.hasRange = true;

    auto cases = gen.generateTestCases(req);
    h.check(!cases.empty(), "generateTestCases returns a non-empty list");

    // Every case has a unique id and a non-empty steps list.
    bool allUnique = true;
    bool allHaveSteps = true;
    for (size_t i = 0; i < cases.size(); ++i) {
        if (cases[i].id.empty()) allUnique = false;
        if (cases[i].steps.empty()) allHaveSteps = false;
        for (size_t j = i + 1; j < cases.size(); ++j) {
            if (cases[i].id == cases[j].id) allUnique = false;
        }
    }
    h.check(allUnique, "every generated test case has a unique id");
    h.check(allHaveSteps, "every generated test case has a non-empty steps list");
}

// ---------------------------------------------------------------------------
// T4. generated cases cover the boundaries
// ---------------------------------------------------------------------------
void testBoundaryCoverage(Harness& h) {
    h.section("T4. generated cases cover the boundaries");

    tf::PlanGenerator gen;
    tf::Requirement req;
    req.name = "Speed limit";
    req.objective = "Vehicle speed must stay within [0,120] km/h.";
    req.min = 0.0;
    req.max = 120.0;
    req.hasRange = true;

    auto cases = gen.generateTestCases(req);
    auto boundaries = gen.deriveBoundaryValues(0.0, 120.0);

    bool allCovered = true;
    for (const auto& b : boundaries) {
        bool covered = false;
        for (const auto& tc : cases) {
            if (testCaseReferences(tc, b.value)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            allCovered = false;
            std::printf("  [INFO] boundary value %g not covered by any case\n",
                        b.value);
        }
    }
    h.check(allCovered, "every boundary value appears in at least one generated case");
}

// ---------------------------------------------------------------------------
// T5. invalid inputs produce invalid-class cases
// ---------------------------------------------------------------------------
void testInvalidClassCases(Harness& h) {
    h.section("T5. invalid inputs produce invalid-class cases");

    tf::PlanGenerator gen;
    tf::Requirement req;
    req.name = "Speed limit";
    req.objective = "Vehicle speed must stay within [0,120] km/h.";
    req.min = 0.0;
    req.max = 120.0;
    req.hasRange = true;

    auto cases = gen.generateTestCases(req);

    bool hasBelowMin = false;
    bool hasAboveMax = false;
    for (const auto& tc : cases) {
        for (const auto& s : tc.steps) {
            if (s.inputValue < req.min) hasBelowMin = true;
            if (s.inputValue > req.max) hasAboveMax = true;
        }
    }
    h.check(hasBelowMin, "at least one case targets an input below min (invalid class)");
    h.check(hasAboveMax, "at least one case targets an input above max (invalid class)");
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    Harness h("S2 Phase 5 TestForge test-case design intelligence");
    std::printf("S2 PHASE 5 TESTFORGE TEST-CASE DESIGN TESTS\n");

    testEquivalenceClasses(h);
    testBoundaryValues(h);
    testGenerateTestCases(h);
    testBoundaryCoverage(h);
    testInvalidClassCases(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
