#pragma once
// core/assurecheck/EvidenceService.h
// Phase 11 WP-3 (AssureCheck): evidence + integration.
//
// Wires the WP-2 ComplianceEngine to real project data. Collects requirements /
// design / test / trace entities from TraceLink and test-run results from
// TestForge into an EvidenceSnapshot, then evaluates the checklist against that
// snapshot. Persists results into assurance_checks (idempotent per standard).
//
// Contract written by the scrum-master in docs/wp3-assurecheck-task.md and
// core/test/wp3_assurecheck_tests.cpp.

#include <string>
#include <vector>

#include "core/assurecheck/ComplianceEngine.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/testforge/TestForgeDao.h"
#include "core/tracelink/TraceLinkService.h"

namespace lodestar::assurecheck {

class EvidenceService {
public:
    EvidenceService(persistence::Database& db,
                    tracelink::TraceLinkService& tl,
                    testforge::TestForgeDao& tf);

    // Collects evidence from TraceLink (entities + links) and TestForge
    // (test runs) into a snapshot.
    common::Result<EvidenceSnapshot> collect();

    // Collects evidence, then runs checks for a standard against it.
    common::Result<std::vector<CheckResult>> runChecks(
        const std::string& standardCode, const std::string& dalLevel);

    // Persists results into assurance_checks (idempotent per standard).
    common::Result<void> storeResults(
        const std::vector<CheckResult>& results);

    // Retrieves stored results for a standard, ordered by item seq.
    common::Result<std::vector<CheckResult>> resultsFor(
        const std::string& standardCode);

private:
    persistence::Database& db_;
    tracelink::TraceLinkService& tl_;
    testforge::TestForgeDao& tf_;
};

}  // namespace lodestar::assurecheck
