#pragma once
// core/assurecheck/DashboardService.h
// Phase 11 WP-6 (AssureCheck): compliance dashboard data.
//
// Provides the Qt-independent per-standard objective coverage + status that the
// Qt compliance dashboard consumes. Computed from stored assurance_checks
// results (WP-2 ComplianceEngine::storeResults). No new migration is required.
//
// Contract written by the scrum-master in docs/wp6-assurecheck-task.md and
// core/test/wp6_assurecheck_tests.cpp.

#include <string>
#include <vector>

#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One standard's row on the compliance dashboard.
struct DashboardStandard {
    std::string code;
    std::string name;
    CoverageSummary coverage;   // from WP-4
};

class DashboardService {
public:
    explicit DashboardService(persistence::Database& db);

    // Per-standard objective coverage computed from stored assurance_checks
    // results. Standards with no stored results are omitted. Ordered by code.
    common::Result<std::vector<DashboardStandard>> dashboard();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
