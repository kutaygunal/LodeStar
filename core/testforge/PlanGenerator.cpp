// core/testforge/PlanGenerator.cpp
#include "core/testforge/PlanGenerator.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/common/Uuid.h"

namespace lodestar::testforge {

namespace {

// Rounds a double to a clean integer-like value when it is within epsilon of an
// integer, so boundary values like 60.0 print/compare as 60.
double tidy(double v) {
    const double r = std::round(v);
    if (std::fabs(v - r) < 1e-9) return r;
    return v;
}

bool contains(const std::vector<double>& values, double v) {
    return std::find(values.begin(), values.end(), v) != values.end();
}

}  // namespace

common::Result<TestProcedure> PlanGenerator::generate(const std::string& name,
                                                      const std::string& version,
                                                      const std::string& objective,
                                                      const std::string& scenarioId,
                                                      const std::vector<Check>& checks) {
    if (name.empty()) {
        return common::Result<TestProcedure>::err("procedure name must not be empty");
    }
    if (scenarioId.empty()) {
        return common::Result<TestProcedure>::err("scenario id must not be empty");
    }

    TestProcedure p;
    p.id = common::newUuid();
    p.name = name;
    p.version = version.empty() ? "1.0" : version;
    p.objective = objective;
    p.scenarioId = scenarioId;
    p.status = RunStatus::Pending;

    int seq = 1;
    p.steps.reserve(checks.size());
    for (const Check& c : checks) {
        TestStep s;
        s.id = common::newUuid();
        s.seq = seq++;
        s.name = c.name;
        s.description = c.description;
        s.metric = c.metric;
        s.expectedValue = c.expectedValue;
        s.tolerance = c.tolerance;
        p.steps.push_back(std::move(s));
    }

    return common::Result<TestProcedure>::ok(std::move(p));
}

Check PlanGenerator::checksFromObjective(const std::string& name,
                                         const std::string& objective) {
    Check c;
    c.name = name.empty() ? "Verify objective" : name;
    c.description = objective;
    c.metric = "behavioral";
    c.expectedValue = 0.0;
    c.tolerance = 0.0;
    return c;
}

// ---------------------------------------------------------------------------
// S2 Phase 5: test-case design intelligence.
// ---------------------------------------------------------------------------

std::vector<EquivalenceClass> PlanGenerator::deriveEquivalenceClasses(double min,
                                                                     double max) const {
    std::vector<EquivalenceClass> classes;
    if (max < min) {
        // Degenerate/invalid range: no meaningful partitions.
        return classes;
    }

    // Valid partition: the whole in-range domain.
    EquivalenceClass valid;
    valid.label = "valid";
    valid.min = min;
    valid.max = max;
    valid.valid = true;
    classes.push_back(valid);

    // Invalid partition: below the minimum.
    EquivalenceClass below;
    below.label = "below-min";
    below.min = min - 1.0;
    below.max = min - 1.0;
    below.valid = false;
    classes.push_back(below);

    // Invalid partition: above the maximum.
    EquivalenceClass above;
    above.label = "above-max";
    above.min = max + 1.0;
    above.max = max + 1.0;
    above.valid = false;
    classes.push_back(above);

    return classes;
}

std::vector<BoundaryValue> PlanGenerator::deriveBoundaryValues(double min,
                                                               double max) const {
    std::vector<BoundaryValue> values;
    if (max < min) {
        return values;
    }

    const double nominal = tidy((min + max) / 2.0);

    // Just-outside (invalid) boundaries first, then the in-range set.
    values.push_back(BoundaryValue{min - 1.0, "min-1"});
    values.push_back(BoundaryValue{min, "min"});
    values.push_back(BoundaryValue{min + 1.0, "min+1"});
    values.push_back(BoundaryValue{nominal, "nominal"});
    values.push_back(BoundaryValue{max - 1.0, "max-1"});
    values.push_back(BoundaryValue{max, "max"});
    values.push_back(BoundaryValue{max + 1.0, "max+1"});

    return values;
}

std::vector<TestCase> PlanGenerator::generateTestCases(const Requirement& req) const {
    std::vector<TestCase> cases;
    if (!req.hasRange) {
        return cases;
    }

    const double min = req.min;
    const double max = req.max;
    const std::vector<EquivalenceClass> classes = deriveEquivalenceClasses(min, max);
    const std::vector<BoundaryValue> boundaries = deriveBoundaryValues(min, max);

    // One test case per boundary value. Each case carries a single step whose
    // inputValue is the boundary value, so boundary coverage is explicit.
    for (const BoundaryValue& b : boundaries) {
        TestCase tc;
        tc.id = common::newUuid();
        tc.name = req.name.empty() ? "Boundary test" : req.name + " @ " + b.label;
        tc.objective = req.objective;

        const bool invalid = (b.value < min) || (b.value > max);
        tc.expectedResult = invalid ? "reject input (invalid class)"
                                    : "accept input (valid class)";

        TestCaseStep step;
        step.id = common::newUuid();
        step.seq = 1;
        step.name = "Apply " + b.label + " boundary";
        step.description = "Feed input value " + std::to_string(b.value) +
                           " (" + b.label + ") to the unit under test.";
        step.metric = "input";
        step.inputValue = b.value;
        step.expectedValue = b.value;
        step.expectedResult = tc.expectedResult;
        tc.steps.push_back(std::move(step));

        cases.push_back(std::move(tc));
    }

    // Ensure at least one explicit invalid-class case exists even if the range
    // is degenerate (min == max), so T5 always holds.
    bool hasInvalid = false;
    for (const EquivalenceClass& c : classes) {
        if (!c.valid) hasInvalid = true;
    }
    if (!hasInvalid) {
        TestCase tc;
        tc.id = common::newUuid();
        tc.name = req.name.empty() ? "Invalid input test" : req.name + " invalid";
        tc.objective = req.objective;
        tc.expectedResult = "reject input (invalid class)";

        TestCaseStep step;
        step.id = common::newUuid();
        step.seq = 1;
        step.name = "Apply out-of-range input";
        step.description = "Feed an input value outside the valid range.";
        step.metric = "input";
        step.inputValue = min - 1.0;
        step.expectedValue = min - 1.0;
        step.expectedResult = tc.expectedResult;
        tc.steps.push_back(std::move(step));

        cases.push_back(std::move(tc));
    }

    return cases;
}

bool testCaseReferences(const TestCase& tc, double value) {
    for (const TestCaseStep& s : tc.steps) {
        if (s.inputValue == value) return true;
    }
    return false;
}

}  // namespace lodestar::testforge
