#pragma once
// core/tracelink/TraceGraph.h
// Facade over TraceLinkService (rich, integrity-enforcing domain service).

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/Models.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

class TraceGraph {
public:
    explicit TraceGraph(persistence::Database& db);

    // --- Entity creation (legacy convenience wrappers) ---------------------
    common::Result<void> addRequirement(persistence::Requirement& r);
    common::Result<void> addDesignItem(persistence::DesignItem& d);
    common::Result<void> addInterface(persistence::InterfaceDef& i);
    common::Result<void> addTestCase(persistence::TestCase& t);

    // --- Rich entity API ---------------------------------------------------
    common::Result<Entity> addEntity(const Entity& e);
    common::Result<std::optional<Entity>> getEntity(EntityType type, const std::string& id);
    common::Result<std::vector<Entity>> listEntities(EntityType type,
                                                     const EntityFilter& filter);
    common::Result<Entity> updateEntity(const Entity& e);
    common::Result<void> removeEntity(EntityType type, const std::string& id);
    common::Result<std::vector<Entity>> search(EntityType type, const std::string& text);

    // --- Links -------------------------------------------------------------
    common::Result<void> addLink(persistence::TraceLink& link);
    common::Result<persistence::TraceLink> updateLink(const std::string& id,
                                                      const std::string& rationale,
                                                      const std::string& status);
    common::Result<persistence::TraceLink> removeLink(const std::string& id);
    common::Result<std::vector<persistence::TraceLink>> linksFrom(
        const std::string& type, const std::string& id);
    common::Result<std::vector<persistence::TraceLink>> linksTo(
        const std::string& type, const std::string& id);
    common::Result<std::vector<persistence::TraceLink>> allLinks();

    // --- Queries (legacy convenience) --------------------------------------
    common::Result<std::vector<persistence::Requirement>> requirements();
    common::Result<std::vector<persistence::TestCase>> testCases();

private:
    persistence::Database& db_;
    TraceLinkService service_;
};

}  // namespace lodestar::tracelink
