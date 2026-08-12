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

class TraceLinkService {
public:
    explicit TraceLinkService(persistence::Database& db);

    // --- Entity CRUD -------------------------------------------------------
    // Assigns UUID + external_id, validates required fields and a legal
    // initial status, rejects a duplicate external id for the type, persists.
    common::Result<Entity> addEntity(const Entity& e);

    // Bumps version, validates any status transition, persists changes.
    common::Result<Entity> updateEntity(const Entity& e);

    // Soft delete: marks the entity Obsolete. Never hard-deletes.
    common::Result<void> removeEntity(EntityType type, const std::string& id);

    common::Result<std::optional<Entity>> getEntity(EntityType type,
                                                    const std::string& id);
    common::Result<std::vector<Entity>> listEntities(EntityType type,
                                                     const EntityFilter& filter);
    // Full-text search across name + description.
    common::Result<std::vector<Entity>> search(EntityType type, const std::string& text);

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
