// core/tracelink/CollaborationService.h
// Gap-Fill TraceLink 3.2: real-time multi-user collaboration.
//
// Adds a change-notification layer over the TraceLink model: an append-only
// operation log with per-entity version vectors, and optimistic concurrency
// with conflict detection on conflicting edits.
//
// Every mutation goes through recordOperation() which bumps the entity's
// version and appends a log row. Concurrent edits are detected via version
// vectors: an edit is "safe" if its base version matches the current version;
// otherwise the caller must resolve the conflict.

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One collaboration operation in the log.
struct CollabOperation {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string op;        // create | update | delete
    std::string actor;
    int version = 0;
    std::string payload;   // JSON snapshot/change
    std::string createdAt;
};

// One version-vector element (per actor).
struct VectorElement {
    std::string actor;
    int version = 0;
};

// Result of attempting a concurrent edit.
enum class EditStatus { Applied, Conflict, NotCurrent };

struct EditResult {
    EditStatus status = EditStatus::Applied;
    int currentVersion = 0;   // the entity's latest version
    std::vector<VectorElement> currentVector;
};

class CollaborationService {
public:
    explicit CollaborationService(persistence::Database& db);

    // Appends a collab operation, bumps the entity's version, and updates the
    // actor's vector element. Returns the operation.
    common::Result<CollabOperation> recordOperation(
        const std::string& entityType, const std::string& entityId,
        const std::string& op, const std::string& actor,
        const std::string& payload);

    // Optimistic concurrency: apply an update ONLY if the caller's base version
    // (the version they last read) matches the entity's current version.
    //  - matches  -> applied (bumps version, records operation)
    //  - stale    -> NotCurrent with the current version/vector for resolution
    common::Result<EditResult> optimisticUpdate(
        const std::string& entityType, const std::string& entityId,
        const std::string& actor, const std::string& payload,
        int baseVersion);

    // The current version-vector for an entity (all actors' seen versions).
    common::Result<std::vector<VectorElement>> vectorFor(
        const std::string& entityId);

    // Change-notification feed: operations for an entity after `afterVersion`.
    common::Result<std::vector<CollabOperation>> changesSince(
        const std::string& entityId, int afterVersion);

    // Merge a remote vector into the local one; detects a conflict when a
    // concurrent actor advanced the same entity without this actor's base.
    common::Result<EditStatus> merge(const std::string& entityId,
                                     const std::string& actor,
                                     const std::vector<VectorElement>& remote);

    // The current version of an entity (0 if none).
    common::Result<int> currentVersion(const std::string& entityId);

private:
    // The entity's current version (MAX(version) in the op log; 0 if none).
    int currentVersionInternal(const std::string& entityId);

    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
