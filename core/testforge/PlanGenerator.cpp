// core/testforge/PlanGenerator.cpp
#include "core/testforge/PlanGenerator.h"

#include <utility>

#include "core/common/Uuid.h"

namespace lodestar::testforge {

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

}  // namespace lodestar::testforge
