#pragma once
// core/assurecheck/ComplianceEngine.h
// Phase 11 WP-2 (AssureCheck): compliance engine.
//
// Evaluates every checklist item of an assurance standard against the project
// data currently in the DB (requirements, design_items, test_cases,
// trace_links) for a given project DAL level, and persists the results into
// the `assurance_checks` table (migration 020).
//
// Contract written by the scrum-master in docs/wp2-assurecheck-task.md and
// core/test/wp2_assurecheck_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

enum class CheckStatus { Pass, Fail, Na, Warning };

// One evidence link: a project entity that satisfies an objective.
struct EvidenceLink {
    std::string entityType;  // requirement | design | test_case | trace_link
    std::string entityId;
};

// The result of evaluating one checklist item.
struct CheckResult {
    std::string id;          // result id (UUID)
    std::string standardCode;
    std::string itemCode;
    std::string itemId;
    CheckStatus status;
    std::string dalLevel;    // item's DAL range
    std::vector<EvidenceLink> evidence;
    std::string detail;
};

// Counts of results by status for a standard.
struct CheckSummary {
    int total = 0;
    int pass = 0;
    int fail = 0;
    int na = 0;
    int warning = 0;
    int percent = 0;  // pass>0 ? (pass*100/total) : 0
};

class ComplianceEngine {
public:
    explicit ComplianceEngine(persistence::Database& db);

    // Evaluates every checklist item of the given standard against the project
    // data currently in the DB, for the given project DAL level. Returns one
    // CheckResult per item (applicable or NA). Does NOT persist.
    common::Result<std::vector<CheckResult>> runChecks(
        const std::string& standardCode, const std::string& dalLevel);

    // Persists a set of results into assurance_checks. Idempotent: replaces
    // any previously stored results for the same standard (no duplicates).
    common::Result<void> storeResults(
        const std::vector<CheckResult>& results);

    // Retrieves stored results for a standard, ordered by item seq.
    common::Result<std::vector<CheckResult>> resultsFor(
        const std::string& standardCode);

    // Summary counts (by status) for a standard from stored results.
    common::Result<CheckSummary> summaryFor(const std::string& standardCode);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
