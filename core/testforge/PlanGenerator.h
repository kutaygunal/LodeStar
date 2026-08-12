#pragma once
// core/testforge/PlanGenerator.h
// TestForge plan generation: auto-builds a TestProcedure (IT&V plan) from a
// scenario id and a list of measurement checks. This is the automation layer
// that removes manual test-procedure authoring.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/testforge/Models.h"

namespace lodestar::testforge {

// One named check to include in a generated procedure.
struct Check {
    std::string name;
    std::string description;
    std::string metric;
    double expectedValue = 0.0;
    double tolerance = 0.0;
};

// ---------------------------------------------------------------------------
// S2 Phase 5: test-case design intelligence.
//
// Given a requirement/objective with an input range (and optional boolean
// conditions), derive equivalence classes, boundary values, and concrete test
// cases. This replaces the thin "checks -> steps" copier with real black-box
// test design (equivalence partitioning + boundary-value analysis).
// ---------------------------------------------------------------------------

// One partition of the input domain.
struct EquivalenceClass {
    std::string label;  // e.g. "valid", "below-min", "above-max"
    double min = 0.0;
    double max = 0.0;
    bool valid = true;  // false => invalid partition (input rejected)
};

// A single boundary value to exercise.
struct BoundaryValue {
    double value = 0.0;
    std::string label;  // e.g. "min", "min+1", "nominal", "max-1", "max", "min-1", "max+1"
};

// A requirement/objective with an input range and optional boolean conditions.
struct Requirement {
    std::string name;
    std::string objective;
    double min = 0.0;
    double max = 0.0;
    bool hasRange = false;
    std::vector<std::string> booleanConditions;  // optional MC/DC-style conditions
};

// One concrete step inside a generated test case.
struct TestCaseStep {
    std::string id;
    int seq = 0;
    std::string name;
    std::string description;
    std::string metric;
    double inputValue = 0.0;
    double expectedValue = 0.0;
    std::string expectedResult;
};

// A concrete test case derived from equivalence classes / boundaries.
struct TestCase {
    std::string id;
    std::string name;
    std::string objective;
    std::string expectedResult;
    std::vector<TestCaseStep> steps;
};

class PlanGenerator {
public:
    // Builds a procedure from a scenario and a set of checks. Each check becomes
    // one ordered TestStep. Assigns a UUID id and Draft/Running planning status.
    common::Result<TestProcedure> generate(const std::string& name,
                                           const std::string& version,
                                           const std::string& objective,
                                           const std::string& scenarioId,
                                           const std::vector<Check>& checks);

    // Convenience: turn a plain-text objective into a single behavioral check
    // (metric "behavioral", no tolerance) for quick smoke / ad-hoc use.
    static Check checksFromObjective(const std::string& name,
                                     const std::string& objective);

    // --- S2 Phase 5: test-case design intelligence -------------------------

    // Derives the equivalence classes for an input range [min, max]: one valid
    // class and the invalid classes below-min and above-max.
    std::vector<EquivalenceClass> deriveEquivalenceClasses(double min,
                                                           double max) const;

    // Derives boundary values for [min, max]: min, min+1, nominal, max-1, max,
    // and just-outside (min-1, max+1).
    std::vector<BoundaryValue> deriveBoundaryValues(double min,
                                                    double max) const;

    // Generates concrete test cases (steps + expected result) from the derived
    // equivalence classes and boundary values. Each case has a unique id and a
    // non-empty steps list. Invalid-class cases are included.
    std::vector<TestCase> generateTestCases(const Requirement& req) const;
};

// Helper: does a generated test case reference the given input value in any of
// its steps? Used by tests to verify boundary coverage.
bool testCaseReferences(const TestCase& tc, double value);

}  // namespace lodestar::testforge
