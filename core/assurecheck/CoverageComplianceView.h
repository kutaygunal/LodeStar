// core/assurecheck/CoverageComplianceView.h
// Gap-Fill AssureCheck 2.3: unified live coverage view.
//
// Maps measured structural coverage (statement today; decision/MC-DC when the
// tooling lands) from TestForge into a single compliance dashboard that maps
// coverage -> DO-178C verification objectives (A-3..A-7). Shows per-objective
// status: compliant / partial / no-evidence, with links to the measured lines.
//
// Honest statuses: no fabricated "measured" values where tooling is absent.

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/assurecheck/StandardsContentService.h"

namespace lodestar::assurecheck {

// One coverage measurement fed into the compliance view.
struct CoverageMeasurement {
    std::string scope;               // module / run id
    int statementsExecuted = 0;
    int statementsTotal = 0;
    int decisionsTaken = 0;
    int decisionsTotal = 0;          // 0 when decision tooling absent
    int conditionsSatisfied = 0;
    int conditionsTotal = 0;         // 0 when MC-DC tooling absent
};

// Aggregated coverage for the view.
struct CoverageAggregate {
    int statementsExecuted = 0;
    int statementsTotal = 0;
    int decisionsTaken = 0;
    int decisionsTotal = 0;
    int conditionsSatisfied = 0;
    int conditionsTotal = 0;
};

// Per-objective compliance status in the dashboard.
struct ObjectiveCompliance {
    std::string subCode;    // e.g. A3-5
    std::string objective;
    std::string dal;
    std::string status;     // compliant | partial | no-evidence
    std::string reason;     // why the status was assigned
    std::vector<std::string> evidenceLinks;  // measured scopes/links
};

class CoverageComplianceView {
public:
    explicit CoverageComplianceView(persistence::Database& db);

    // Aggregate the measurements (single source of truth for the view).
    static CoverageAggregate aggregate(
        const std::vector<CoverageMeasurement>& measurements);

    // Map coverage aggregates to DO-178C verification objectives (A-3..A-7).
    // statement coverage below `statementThreshold` (default 70) is "partial";
    // with no measured statements at all it is "no-evidence". Decision/MC-DC
    // objectives are "no-evidence" when decision tooling is absent (honest:
    // not claimed as measured).
    static std::vector<ObjectiveCompliance> mapToObjectives(
        const StandardBundle& bundle, const CoverageAggregate& agg,
        const std::string& projectDal, int statementThreshold = 70);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
