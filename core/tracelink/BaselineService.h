#pragma once
// core/tracelink/BaselineService.h
// WP-4 versioning: append-only audit read, baseline snapshots, field-level
// diff between baselines, per-entity history, entity reconstruction at a
// baseline, and change-impact analysis.
//
// Contract written by the scrum-master in core/test/wp4_audit_baseline_tests.cpp
// (see docs/tracelink-plan.md WP-4 / sections 4.5, 7.4, schema 3.3/3.4).

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Audit / baseline value types.
// ---------------------------------------------------------------------------

// One row of the append-only audit trail.
struct AuditEntry {
    std::string id;
    std::string entityType;      // "requirement" | "design" | ... | "link"
    std::string entityId;
    std::string action;          // create/update/soft_delete/add_link/...
    std::string field;           // set only for field-level updates
    std::string oldValue;
    std::string newValue;
    std::string actor;
    std::string timestamp;
    std::string changeRequestId;
};

// A named point-in-time snapshot.
struct Baseline {
    std::string id;
    std::string name;
    std::string description;
    std::string createdAt;
};

// A single field-level change observed when diffing two baselines.
struct FieldChange {
    std::string field;
    std::string oldValue;
    std::string newValue;
};

enum class DiffKind { Added, Removed, Modified };

// One entity or link difference between two baselines.
struct DiffEntry {
    DiffKind kind = DiffKind::Added;
    EntityType entityType = EntityType::Requirement;
    std::string entityId;
    std::string entityExternalId;
    std::vector<FieldChange> fieldChanges;  // non-empty for Modified only
};

// Full diff result: entity deltas and link deltas.
struct DiffResult {
    std::vector<DiffEntry> entities;
    std::vector<DiffEntry> links;
};

// Change-impact result: audit entries tagged to a change request, plus the
// downstream entities affected by the change.
struct ImpactResult {
    std::vector<AuditEntry> changes;           // audit rows tagged to the change
    std::vector<GraphNode> downstreamAffected; // downstream closure of the entity
};

// ---------------------------------------------------------------------------
// BaselineService.
// ---------------------------------------------------------------------------
class BaselineService {
public:
    explicit BaselineService(persistence::Database& db);

    // --- Audit read --------------------------------------------------------
    common::Result<std::vector<AuditEntry>> history(EntityType type, const std::string& id);
    common::Result<std::vector<AuditEntry>> allHistory();

    // --- Baselines ---------------------------------------------------------
    common::Result<Baseline> createBaseline(const std::string& name, const std::string& description);
    common::Result<std::vector<Baseline>> listBaselines();
    common::Result<std::optional<Baseline>> getBaseline(const std::string& id);

    // Diff a(older) against b(newer). Added/Removed/Modified for entities and links.
    common::Result<DiffResult> diffBaseline(const std::string& aId, const std::string& bId);

    // Reconstruct the entity exactly as snapshotted in a baseline.
    common::Result<std::optional<Entity>>
        entityAtBaseline(EntityType type, const std::string& id, const std::string& baselineId);

    // Audit entries tagged to a change request + downstream affected entities.
    common::Result<ImpactResult>
        changeImpact(EntityType type, const std::string& id, const std::string& changeRequestId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
