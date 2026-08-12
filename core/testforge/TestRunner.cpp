// core/testforge/TestRunner.cpp
#include "core/testforge/TestRunner.h"

#include <cmath>
#include <utility>

#include "core/common/Uuid.h"

namespace lodestar::testforge {

common::Result<TestRun> TestRunner::run(const TestProcedure& procedure,
                                        const std::string& startedAt,
                                        const std::string& finishedAt) {
    TestRun run;
    run.id = common::newUuid();
    run.procedureId = procedure.id;
    run.procedureName = procedure.name;
    run.scenarioId = procedure.scenarioId;
    run.status = RunStatus::Running;
    run.startedAt = startedAt;
    run.finishedAt = finishedAt;

    run.results.reserve(procedure.steps.size());
    for (const TestStep& step : procedure.steps) {
        StepResult r;
        r.stepId = step.id;
        r.seq = step.seq;
        r.name = step.name;
        r.expectedValue = step.expectedValue;
        r.tolerance = step.tolerance;

        auto measured = provider_.measure(step.metric);
        if (measured.failed()) {
            r.status = StepStatus::Blocked;
            r.message = "measurement error: " + measured.error();
            run.results.push_back(std::move(r));
            continue;
        }

        std::optional<double> value = measured.value();
        if (!value.has_value()) {
            r.status = StepStatus::Blocked;
            r.message = "metric '" + step.metric + "' unavailable";
            run.results.push_back(std::move(r));
            continue;
        }

        r.measured = true;
        r.actualValue = value.value();
        const double deviation = std::fabs(r.actualValue - step.expectedValue);
        if (deviation <= step.tolerance) {
            r.status = StepStatus::Passed;
            r.message = "within tolerance";
        } else {
            r.status = StepStatus::Failed;
            r.message = "outside tolerance (deviation " + std::to_string(deviation) + ")";
        }
        run.results.push_back(std::move(r));
    }

    // Terminal status.
    bool anyFailed = false;
    bool anyBlocked = false;
    for (const StepResult& r : run.results) {
        if (r.status == StepStatus::Failed) anyFailed = true;
        if (r.status == StepStatus::Blocked) anyBlocked = true;
    }
    if (anyFailed) {
        run.status = RunStatus::Failed;
    } else if (anyBlocked) {
        run.status = RunStatus::Blocked;
    } else {
        run.status = RunStatus::Passed;
    }

    return common::Result<TestRun>::ok(std::move(run));
}

}  // namespace lodestar::testforge
