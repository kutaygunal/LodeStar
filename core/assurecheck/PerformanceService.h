#pragma once
// core/assurecheck/PerformanceService.h
// Phase 11 WP-5 (AssureCheck): performance + hardening.
//
// Hardens the AssureCheck engine for scale: batched (transactional)
// evaluation of a whole standard, and incremental re-check of only the
// affected items. Builds on the WP-2 ComplianceEngine / CheckResult types and
// the WP-2 assurance_checks table.
//
// Contract written by the scrum-master in docs/wp5-assurecheck-task.md and
// core/test/wp5_assurecheck_tests.cpp.

#include <string>
#include <vector>

#include "core/assurecheck/ComplianceEngine.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

class PerformanceService {
public:
    explicit PerformanceService(persistence::Database& db);

    // Batched evaluation: evaluates every checklist item of the given standard
    // against the project data and stores the results atomically in a single
    // transaction (BEGIN IMMEDIATE ... COMMIT). Returns the results.
    common::Result<std::vector<CheckResult>> evaluateBatched(
        const std::string& standardCode, const std::string& dalLevel);

    // Incremental re-check: re-evaluates only the checklist items whose
    // evidence source is among changedSources. changedSources values are
    // "requirement", "design", "test_case", "trace_link", "test_run".
    // Returns the affected subset (only those items). Does NOT persist.
    common::Result<std::vector<CheckResult>> recheckIncremental(
        const std::string& standardCode, const std::string& dalLevel,
        const std::vector<std::string>& changedSources);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
