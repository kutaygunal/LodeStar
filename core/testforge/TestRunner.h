#pragma once
// core/testforge/TestRunner.h
// TestForge execution: runs a TestProcedure step-by-step against a measurement
// source and evaluates each step as Passed/Failed/Blocked. Produces a TestRun.

#include <map>
#include <optional>
#include <string>

#include "core/common/Result.h"
#include "core/testforge/Models.h"

namespace lodestar::testforge {

// Abstraction over where measured values come from. Implementations may wrap a
// Scenario, an adapter (vendor RF tool / LLM), or return canned values for
// offline self-verification (MockMeasurementProvider).
class IMeasurementProvider {
public:
    virtual ~IMeasurementProvider() = default;

    // Returns the measured value for a named metric, or nullopt if the metric
    // is unavailable (which marks the step Blocked).
    virtual common::Result<std::optional<double>> measure(const std::string& metric) = 0;
};

// Deterministic provider used by the smoke path and offline runs.
class MockMeasurementProvider : public IMeasurementProvider {
public:
    void set(const std::string& metric, double value) { values_[metric] = value; }
    void remove(const std::string& metric) { values_.erase(metric); }

    common::Result<std::optional<double>> measure(const std::string& metric) override {
        auto it = values_.find(metric);
        if (it == values_.end()) {
            return common::Result<std::optional<double>>::ok(std::nullopt);
        }
        return common::Result<std::optional<double>>::ok(it->second);
    }

private:
    std::map<std::string, double> values_;
};

class TestRunner {
public:
    explicit TestRunner(IMeasurementProvider& provider) : provider_(provider) {}

    // Executes every step of the procedure, populating a TestRun. The run is
    // Pending before execution, Running during, and reaches a terminal status:
    // Passed (all passed), Blocked (some blocked, none failed), else Failed.
    common::Result<TestRun> run(const TestProcedure& procedure,
                                const std::string& startedAt = "",
                                const std::string& finishedAt = "");

private:
    IMeasurementProvider& provider_;
};

}  // namespace lodestar::testforge
