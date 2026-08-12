#pragma once
// core/tracelink/CoverageService.h
// WP-5: wires TestForge test runs into live traceability coverage. A
// requirement is `verified` only when it has an Active verifies link to a test
// case AND that test case has a PASSING recorded run (the latest executed
// result governs). `executed` means at least one run was recorded for a
// verifying test case. `designed` means at least one Active satisfies link.
//
// Contract written by the scrum-master in core/test/wp5_coverage_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One requirement's coverage row reflecting EXECUTED TestForge results.
struct ExecutedCoverageRow {
    std::string requirementId;
    std::string requirementExternalId;
    bool designed = false;  // has >=1 Active satisfies link
    bool verified = false;  // has >=1 Active verifies link AND a passing run
    bool executed = false;  // has at least one recorded test run
};

class CoverageService {
public:
    explicit CoverageService(persistence::Database& db);

    // Records that a TestForge run executed a traceability test case.
    // passed = (run.status == Passed).
    common::Result<void> recordRun(const std::string& runId,
                                   const std::string& testCaseId,
                                   bool passed);

    // Live coverage: a requirement is `verified` only when it has an Active
    // verifies link to a test case AND that test case has a passing recorded
    // run (the latest executed result governs).
    common::Result<std::vector<ExecutedCoverageRow>> executedCoverage();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
