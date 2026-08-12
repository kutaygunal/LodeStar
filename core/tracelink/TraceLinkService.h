#pragma once
// core/tracelink/TraceLinkService.h
// The canonical TraceLink service: rich entity CRUD, typed link CRUD with
// integrity-on-write, and status state-machine enforcement. This is the
// contract implemented for WP-1 and exercised by the scrum-master's tests.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/daos.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// One ranked full-text search hit (WP-A, A1).
struct SearchHit {
    EntityType type = EntityType::Requirement;
    std::string id;
    std::string externalId;
    std::string name;
    double rank = 0.0;  // FTS5 bm25 score; LOWER is a better match
};

// A nested hierarchy node: an entity plus its ordered children.
struct HierarchyNode {
    Entity entity;
    std::vector<HierarchyNode> children;
};

// WP-E (A9): a group of similar entities (by name/text similarity).
struct DuplicateGroup {
    std::vector<Entity> entities;   // >= 2 similar entities
    double similarity = 0.0;        // max pairwise similarity (0..1)
};

class TraceLinkService {
public:
    explicit TraceLinkService(persistence::Database& db);

    // --- Entity CRUD -------------------------------------------------------
    // Assigns UUID + external_id, validates required fields and a legal
    // initial status, rejects a duplicate external id for the type, persists.
    common::Result<Entity> addEntity(const Entity& e);

    // Bumps version, validates any status transition, persists changes.
    common::Result<Entity> updateEntity(const Entity& e);

    // WP-4: optimistic-locking update. Updates the entity ONLY if its current
    // stored version equals expectedVersion. Fails with a ConcurrencyError
    // (concurrent-edit conflict) if the stored version differs. On success the
    // version is bumped by one, exactly like updateEntity.
    common::Result<Entity> updateEntityIfVersion(const Entity& e,
                                                 int expectedVersion);

    // Soft delete: marks the entity Obsolete. Never hard-deletes.
    common::Result<void> removeEntity(EntityType type, const std::string& id);

    common::Result<std::optional<Entity>> getEntity(EntityType type,
                                                    const std::string& id);
    common::Result<std::vector<Entity>> listEntities(EntityType type,
                                                     const EntityFilter& filter);
    // Full-text search across name + description.
    common::Result<std::vector<Entity>> search(EntityType type, const std::string& text);

    // --- WP-A: FTS5 ranked full-text search --------------------------------
    // Rebuilds the FTS5 index from every entity table. Safe to call any time.
    common::Result<void> rebuildSearchIndex();

    // Ranked full-text search across name + body for one entity type.
    // text is a plain term/phrase (the service escapes it for FTS5 MATCH).
    // limit/offset paginate the ranked result set (0 = no limit).
    // Results are ordered best-match-first (ascending bm25 rank).
    // The name column is weighted HIGHER than body, so a name match always
    // ranks above a body-only match for the same term.
    common::Result<std::vector<SearchHit>> searchRanked(
        EntityType type, const std::string& text, int limit = 0, int offset = 0);

    // --- Links -------------------------------------------------------------
    // Validates nodes exist (no dangling), no self-loop, no duplicate, and the
    // relation is legal for the source/target pair.
    common::Result<Link> addLink(const Link& link);
    common::Result<Link> updateLink(const Link& link);
    common::Result<void> removeLink(const std::string& id);  // marks Superseded
    common::Result<std::vector<Link>> linksFrom(EntityType type, const std::string& id);
    common::Result<std::vector<Link>> linksTo(EntityType type, const std::string& id);
    common::Result<std::vector<Link>> allLinks();

    // --- Audit context -----------------------------------------------------
    // Stamps the actor + change request id on subsequent mutations until reset
    // (pass empty strings to clear). Every mutation writes an audit_log row
    // regardless of context; the context only enriches those rows.
    void setAuditContext(const std::string& actor, const std::string& changeRequestId);

    // --- Status state machine ----------------------------------------------
    bool isLegalTransition(EntityType type, const std::string& from,
                           const std::string& to);
    common::Result<void> transition(EntityType type, const std::string& id,
                                    const std::string& to);

    // --- Hierarchy tree (WP-C / A2) ----------------------------------------
    // Sets the parent of `id` to `parentId`. Both must exist and be the same
    // entity type. Rejects a cycle (parentId must not be `id` or a descendant
    // of `id`). Pass an empty parentId to detach (make it a root).
    common::Result<void> setParent(EntityType type, const std::string& id,
                                   const std::string& parentId);

    // Direct children of parentId ("" = roots), ordered by sortOrder then id.
    common::Result<std::vector<Entity>> children(EntityType type,
                                                 const std::string& parentId);

    // All descendants of `id` (recursive, excludes `id` itself).
    common::Result<std::vector<Entity>> subtree(EntityType type,
                                                const std::string& id);

    // The ancestor chain of `id` from its immediate parent up to the root.
    common::Result<std::vector<Entity>> ancestors(EntityType type,
                                                  const std::string& id);

    // Reorders the direct children of parentId to the given id order,
    // assigning sortOrder 0..N-1 in that order.
    common::Result<void> reorder(EntityType type, const std::string& parentId,
                                 const std::vector<std::string>& orderedIds);

    // All entities of `type` with no parent (roots), ordered by sortOrder.
    common::Result<std::vector<Entity>> rootNodes(EntityType type);

    // Builds the full nested tree rooted at `id` (recursive).
    common::Result<HierarchyNode> buildTree(EntityType type,
                                            const std::string& rootId);

    // --- WP-E (A9): duplicate / similarity detection -----------------------
    // Groups entities of `type` whose pairwise similarity >= threshold.
    // Exact duplicates (identical name+text) always group. Returns groups
    // with at least 2 members; each group's `similarity` is the maximum
    // pairwise similarity among its members.
    common::Result<std::vector<DuplicateGroup>> findDuplicates(
        EntityType type, double threshold = 0.8);

private:
    bool nodeExists(EntityType type, const std::string& id);
    common::Result<std::optional<Entity>> dispatchGet(EntityType type,
                                                      const std::string& id);
    common::Result<Entity> dispatchCreate(EntityType type, const Entity& e);
    common::Result<Entity> dispatchUpdate(EntityType type, const Entity& e);

    // Appends one audit_log row (inside the caller's transaction).
    common::Result<void> writeAudit(const std::string& entityType,
                                    const std::string& entityId,
                                    const std::string& action,
                                    const std::string& field,
                                    const std::string& oldValue,
                                    const std::string& newValue);
    void beginTx();
    common::Result<void> commitTx();
    void rollbackTx();

    std::string actor_;
    std::string changeRequestId_;

    persistence::Database& db_;
    persistence::RequirementDao reqDao_;
    persistence::DesignItemDao designDao_;
    persistence::InterfaceDao ifaceDao_;
    persistence::TestCaseDao testDao_;
    persistence::HazardDao hazardDao_;
    persistence::DecisionDao decisionDao_;
    persistence::AssumptionDao assumptionDao_;
    persistence::TraceLinkDao linkDao_;
};

}  // namespace lodestar::tracelink
