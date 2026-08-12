#pragma once
// core/assurecheck/AssureCheckService.h
// Phase 11 WP-1 (AssureCheck): standards registry + checklist data model.
//
// Seeds the five assurance standards (DO-178C, DO-254, ARP4754A, ARP4761,
// DO-278A) with all 136 checklist items from
// docs/assurecheck-standards-checklist.md (migration 019). Idempotent.
//
// Contract written by the scrum-master in docs/wp1-assurecheck-task.md and
// core/test/wp1_assurecheck_tests.cpp.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One assurance standard (e.g. DO-178C).
struct AssuranceStandard {
    std::string id;
    std::string code;      // DO-178C | DO-254 | ARP4754A | ARP4761 | DO-278A
    std::string name;      // full standard name
    std::string description;
};

// One checklist item within a standard.
struct AssuranceChecklistItem {
    std::string id;
    std::string standardId;
    std::string itemCode;  // A1-1 | D254-1 | A4754-1 | A4761-1 | D278-1
    int seq = 0;
    std::string objective;
    std::string dalLevel;  // A | A-B | A-C | A-D
    std::string evidence;
};

class AssureCheckService {
public:
    explicit AssureCheckService(persistence::Database& db);

    // Seeds the five standards (DO-178C, DO-254, ARP4754A, ARP4761, DO-278A)
    // with all 136 checklist items from the standards checklist doc. Idempotent.
    common::Result<void> seedStandards();

    // All standards, ordered by code.
    common::Result<std::vector<AssuranceStandard>> listStandards();

    // A standard by code; nullopt if missing.
    common::Result<std::optional<AssuranceStandard>> getStandard(
        const std::string& code);

    // Checklist items for a standard (by code), ordered by seq.
    common::Result<std::vector<AssuranceChecklistItem>> checklistFor(
        const std::string& standardCode);

    // Total number of checklist items across all standards.
    common::Result<int> totalItemCount();

    // Number of checklist items whose DAL range includes the given level
    // (e.g. "A", "B", "C", "D", "E").
    common::Result<int> countForDal(const std::string& dalLevel);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
