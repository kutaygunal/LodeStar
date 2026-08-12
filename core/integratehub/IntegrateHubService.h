#pragma once
// core/integratehub/IntegrateHubService.h
// Sprint 1 Phase 4 (IntegrateHub): cross-disciplinary issue/coordination model.
//
// Backed by persistence::Database (SQLite). Models issues owned by a single
// discipline and coordination notes attached to those issues. Persisted so
// issues and coordination survive a reopen.
//
// Contract written by the scrum-master in docs/s1-phase4-task.md and
// core/test/s1_phase4_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::integratehub {

// Owning discipline of an issue.
enum class Discipline { Systems, Software, Hardware, Test, Safety };

// A cross-disciplinary issue.
struct Issue {
    std::string id;            // stable id
    std::string title;
    std::string description;
    Discipline owner;          // owning discipline
    std::string status;        // "open" | "in_progress" | "resolved"
    std::string createdAt;
};

// A coordination note attached to an issue.
struct Coordination {
    std::string id;
    std::string issueId;       // the issue being coordinated
    std::string note;
    std::string createdAt;
};

class IntegrateHubService {
public:
    explicit IntegrateHubService(persistence::Database& db);

    // Create an issue; returns its id.
    common::Result<std::string> createIssue(const Issue& issue);

    // List issues, optionally filtered by discipline.
    common::Result<std::vector<Issue>> listIssues(Discipline d);

    // Update an issue's status.
    common::Result<void> setStatus(const std::string& issueId,
                                   const std::string& status);

    // Add a coordination note to an issue.
    common::Result<std::string> addCoordination(const std::string& issueId,
                                                const std::string& note);

    // List coordination notes for an issue, oldest first.
    common::Result<std::vector<Coordination>> coordinationFor(
        const std::string& issueId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::integratehub
