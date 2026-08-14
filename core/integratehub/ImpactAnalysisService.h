// core/integratehub/ImpactAnalysisService.h
// Gap-Fill IntegrateHub 6.1: Problem Report -> Change Request -> impact analysis.
//
// Formalizes a PR workflow (fields, states, approval authority) and a CR model
// linked to it, computes impact analysis on a CR (affected requirements, design
// items, tests, baselines), flags risk of unverified impact, and links PR/CR
// rows into the TraceLink traceability graph (reusing its link types).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::integratehub {

// --- Problem Report -------------------------------------------------------
struct ProblemReport {
    std::string id;
    std::string title;
    std::string description;
    std::string severity = "medium";  // low | medium | high | critical
    std::string status = "open";      // open | under_investigation | resolved | closed
    std::string reportedBy;
    std::string approvalAuthority;
    std::string createdAt;
    std::string updatedAt;
};

// --- Change Request -------------------------------------------------------
struct ChangeRequest {
    std::string id;
    std::string prId;          // linked problem report (optional)
    std::string title;
    std::string description;
    std::string status = "Open";  // Open | InReview | Approved | Rejected | Implemented
    std::string entityType;       // requirement | design | test_case | ...
    std::string entityId;
    std::string proposedChange;
    std::string createdBy;
    std::string createdAt;
};

// One impacted item computed by impact analysis.
struct ImpactItem {
    std::string targetType;  // requirement | design | test_case | baseline
    std::string targetId;
    bool riskUnverified = false;  // true if impact not verifiable
    std::string detail;
};

class ImpactAnalysisService {
public:
    explicit ImpactAnalysisService(persistence::Database& db);

    // --- Problem Report workflow -------------------------------------------
    // status transitions: open -> under_investigation -> resolved -> closed.
    // Approval authority is required before a PR may be closed.
    common::Result<std::string> createPr(const ProblemReport& pr);
    common::Result<std::vector<ProblemReport>> listPrs();
    common::Result<ProblemReport> transitionPr(const std::string& id,
                                               const std::string& to);

    // --- Change Request ----------------------------------------------------
    common::Result<std::string> createCr(const ChangeRequest& cr);
    common::Result<std::vector<ChangeRequest>> listCrs();

    // --- Impact analysis ---------------------------------------------------
    // Computes the impact set for a CR: the direct entity plus linked
    // requirements/design/tests/baselines reachable via the TraceLink graph.
    // Sets riskUnverified on impact items that cannot be verified.
    common::Result<std::vector<ImpactItem>> analyzeImpact(const std::string& crId);

    // Approval gating: a CR with unresolved (unverified) high-risk impact may
    // not be approved. Returns ok if approvable, else a ValidationFailed error.
    common::Result<void> checkApprovalGate(const std::string& crId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::integratehub
