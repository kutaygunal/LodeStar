#pragma once
// core/tracelink/SuspectService.h
// Phase 10 WP-1 (A1): suspect-link workflow.
//
// When an upstream requirement changes, every downstream artifact that depends
// on it (designs that satisfy it, tests that verify it, and their transitive
// downstream closure) is auto-flagged `suspect`. Suspect flags form a review
// queue that an engineer inspects and clears once the impact is understood.
//
// Contract written by the scrum-master in core/test/wp1_suspect_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One active (uncleared) suspect flag.
struct SuspectFlag {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string reason;
    std::string sourceType;
    std::string sourceId;
    std::string createdAt;
};

class SuspectService {
public:
    explicit SuspectService(persistence::Database& db);

    // Flags an artifact as suspect. Assigns a UUID if id is empty.
    common::Result<SuspectFlag> flagSuspect(
        const std::string& entityType, const std::string& entityId,
        const std::string& reason,
        const std::string& sourceType, const std::string& sourceId);

    // All ACTIVE (uncleared) suspect flags, newest first.
    common::Result<std::vector<SuspectFlag>> suspectQueue();

    // True if the artifact has at least one active suspect flag.
    common::Result<bool> isSuspect(const std::string& entityType,
                                   const std::string& entityId);

    // Clears a flag (records cleared_at/cleared_by). No-op if already cleared.
    common::Result<void> clearSuspect(const std::string& flagId,
                                      const std::string& clearedBy);

    // Auto-flags every downstream artifact of `entityType`/`entityId` as suspect
    // (designs that satisfy it, tests that verify it, and their transitive
    // downstream closure). Used when a requirement changes. Returns the flags.
    common::Result<std::vector<SuspectFlag>> autoFlagDownstream(
        const std::string& entityType, const std::string& entityId,
        const std::string& reason);

    // T6: ids of every Active link incident to a currently-suspect artifact
    // (i.e. the links that are themselves reported suspect). Used to surface
    // the affected verifies/satisfies links in the review queue.
    common::Result<std::vector<std::string>> suspectLinks();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
