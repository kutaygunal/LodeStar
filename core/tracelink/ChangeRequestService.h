#pragma once
// core/tracelink/ChangeRequestService.h
// WP-B (A4): change-request + review workflow.
//
// A change request (CR) proposes a set of field changes to a target entity.
// It flows through a review lifecycle (Open -> InReview -> Approved/Rejected
// -> Implemented). Once approved, its proposed change is applied to the target
// entity and every audit row written for that change is stamped with the CR id
// (linking CRs to the audit trail for change-impact analysis).
//
// Contract written by the scrum-master in core/test/wpB_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// One change request row.
struct ChangeRequest {
    std::string id;
    std::string title;
    std::string description;
    std::string status;          // Open | InReview | Approved | Rejected | Implemented
    std::string entityType;      // "requirement" | "design" | ...
    std::string entityId;
    std::string proposedChange;  // JSON of proposed field changes, e.g. {"name":"X"}
    std::string createdBy;
    std::string createdAt;
    std::string reviewedBy;
    std::string reviewedAt;
    std::string reviewComment;
};

class ChangeRequestService {
public:
    explicit ChangeRequestService(persistence::Database& db);

    // Creates a CR in status "Open". Assigns a UUID if id is empty.
    common::Result<ChangeRequest> create(const ChangeRequest& cr);

    // All CRs awaiting review (status Open or InReview), newest first.
    common::Result<std::vector<ChangeRequest>> reviewQueue();

    // Open -> InReview.
    common::Result<ChangeRequest> submitForReview(const std::string& id);

    // InReview -> Approved (records reviewer + comment).
    common::Result<ChangeRequest> approve(const std::string& id,
                                          const std::string& reviewer,
                                          const std::string& comment);

    // InReview -> Rejected (records reviewer + comment).
    common::Result<ChangeRequest> reject(const std::string& id,
                                         const std::string& reviewer,
                                         const std::string& comment);

    // Applies an APPROVED CR's proposed change to the target entity,
    // stamping every audit row with the CR id. Marks the CR "Implemented".
    // Fails if the CR is not Approved. Returns the updated entity.
    common::Result<Entity> applyChangeRequest(const std::string& crId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
