#pragma once
// core/assurecheck/WorkflowService.h
// S2 Phase 3 (AssureCheck): review/approval/sign-off workflow + audit trail +
// objective->evidence package.
//
// A stored check result (assurance_checks row) can be submitted for review,
// approved, or rejected by a named actor. Every transition records the real
// actor and a real timestamp (never the literal placeholder "now") and is
// appended to an audit log (who, what, when, from->to). buildEvidencePackage
// collects the evidence links for an objective (checklist item) into an
// exportable package.
//
// Contract written by the scrum-master in docs/s2-phase3-test.md and
// core/test/s2_phase3_tests.cpp.

#include <string>
#include <vector>

#include "core/assurecheck/ComplianceEngine.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One audit-log entry for a workflow transition.
struct AuditEntry {
    std::string id;         // audit row UUID
    std::string resultId;   // target check result
    std::string actor;      // who performed the transition
    std::string action;     // submit | approve | reject
    std::string target;     // result id (same as resultId)
    std::string timestamp;  // real date/time
    std::string fromState;  // draft | in_review | approved | rejected
    std::string toState;
};

// An objective->evidence package: the evidence links that satisfy an objective.
struct EvidencePackage {
    std::string objectiveId;              // checklist item id
    std::vector<EvidenceLink> links;      // {entityType, entityId}
};

class WorkflowService {
public:
    explicit WorkflowService(persistence::Database& db);

    // Submits a check result for review (draft -> in_review). Records the actor
    // and a real timestamp.
    common::Result<void> submitForReview(const std::string& resultId,
                                         const std::string& actor);

    // Approves a check result (in_review -> approved). Records the actor and a
    // real timestamp.
    common::Result<void> approve(const std::string& resultId,
                                 const std::string& actor);

    // Rejects a check result (in_review -> rejected). Records the actor and a
    // real timestamp.
    common::Result<void> reject(const std::string& resultId,
                                const std::string& actor);

    // Current workflow state of a result: draft | in_review | approved |
    // rejected. Empty string if the result does not exist.
    common::Result<std::string> stateFor(const std::string& resultId);

    // Audit log for a result, ordered by timestamp.
    common::Result<std::vector<AuditEntry>> auditLog(
        const std::string& resultId);

    // Collects the evidence links for an objective (checklist item id) into an
    // exportable package.
    common::Result<EvidencePackage> buildEvidencePackage(
        const std::string& objectiveId);

private:
    // Applies a single workflow transition and appends an audit entry.
    common::Result<void> transition(const std::string& resultId,
                                    const std::string& actor,
                                    const std::string& action,
                                    const std::string& fromState,
                                    const std::string& toState);

    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
