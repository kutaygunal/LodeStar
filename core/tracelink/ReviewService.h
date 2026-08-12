#pragma once
// core/tracelink/ReviewService.h
// WP-2 (Phase 10): general artifact review / comment / approval.
//
// Any artifact can carry general review comments and an approval verdict,
// beyond the change-request workflow. Comments are free-form notes attached to
// an artifact; reviews record an approval verdict (Approve | Reject |
// RequestChanges) from a reviewer. The current approval status of an artifact
// is governed by its most recent review.
//
// Contract written by the scrum-master in core/test/wp2_review_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One general review comment attached to an artifact.
struct Comment {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string author;
    std::string body;
    std::string createdAt;
};

// One review verdict submitted for an artifact.
struct Review {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string reviewer;
    std::string verdict;   // Approve | Reject | RequestChanges
    std::string comment;
    std::string createdAt;
};

class ReviewService {
public:
    explicit ReviewService(persistence::Database& db);

    // Adds a comment to an artifact. Assigns a UUID if id is empty.
    common::Result<Comment> addComment(const std::string& entityType,
                                       const std::string& entityId,
                                       const std::string& author,
                                       const std::string& body);

    // All comments for an artifact, oldest first.
    common::Result<std::vector<Comment>> commentsFor(
        const std::string& entityType, const std::string& entityId);

    // Submits a review verdict for an artifact.
    common::Result<Review> submitReview(const std::string& entityType,
                                        const std::string& entityId,
                                        const std::string& reviewer,
                                        const std::string& verdict,
                                        const std::string& comment);

    // All reviews for an artifact, newest first.
    common::Result<std::vector<Review>> reviewsFor(
        const std::string& entityType, const std::string& entityId);

    // Current approval status: "Approved" if the latest review is Approve,
    // "Rejected" if Reject, "RequestChanges" if RequestChanges, else "None".
    common::Result<std::string> approvalStatus(
        const std::string& entityType, const std::string& entityId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
