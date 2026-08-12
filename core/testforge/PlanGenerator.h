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
};

}  // namespace lodestar::testforge
