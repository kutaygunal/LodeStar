// core/tracelink/TraceGraph.cpp
// Facade over TraceLinkService (rich, integrity-enforcing).

#include "core/tracelink/TraceGraph.h"

#include "core/common/Uuid.h"

namespace lodestar::tracelink {

TraceGraph::TraceGraph(persistence::Database& db) : db_(db), service_(db) {}

namespace {
// Legacy wrappers historically auto-generated an external id when the caller
// left it empty. The service now rejects empty external ids (WP-F / B3), so the
// facade supplies a default here to keep the legacy convenience path working.
std::string defaultExternalId(EntityType type) {
    const char* prefix = "ENT";
    switch (type) {
        case EntityType::Requirement: prefix = "REQ"; break;
        case EntityType::Design:      prefix = "DES"; break;
        case EntityType::Interface:   prefix = "IF"; break;
        case EntityType::TestCase:    prefix = "TC"; break;
        case EntityType::Hazard:      prefix = "HAZ"; break;
        case EntityType::Decision:    prefix = "DEC"; break;
        case EntityType::Assumption:  prefix = "ASS"; break;
    }
    return std::string(prefix) + "-" + lodestar::common::newUuid().substr(0, 8);
}
}  // namespace

// ---------------------------------------------------------------------------
// Legacy entity creation wrappers.
// ---------------------------------------------------------------------------
common::Result<void> TraceGraph::addRequirement(persistence::Requirement& r) {
    Entity e;
    e.type = EntityType::Requirement;
    e.id = r.id;
    e.externalId = r.externalId.empty() ? defaultExternalId(EntityType::Requirement)
                                        : r.externalId;
    e.name = r.name;
    e.text = r.description;
    e.status = r.status;
    auto res = service_.addEntity(e);
    if (res.failed()) return common::Result<void>::err(res.error());
    r.id = res.value().id;
    r.externalId = res.value().externalId;
    r.version = res.value().version;
    r.createdAt = res.value().createdAt;
    r.updatedAt = res.value().updatedAt;
    return common::Result<void>::ok();
}

common::Result<void> TraceGraph::addDesignItem(persistence::DesignItem& d) {
    Entity e;
    e.type = EntityType::Design;
    e.id = d.id;
    e.externalId = d.externalId.empty() ? defaultExternalId(EntityType::Design)
                                        : d.externalId;
    e.name = d.name;
    e.text = d.description;
    e.status = d.status;
    auto res = service_.addEntity(e);
    if (res.failed()) return common::Result<void>::err(res.error());
    d.id = res.value().id;
    return common::Result<void>::ok();
}

common::Result<void> TraceGraph::addInterface(persistence::InterfaceDef& i) {
    Entity e;
    e.type = EntityType::Interface;
    e.id = i.id;
    e.externalId = i.externalId.empty() ? defaultExternalId(EntityType::Interface)
                                        : i.externalId;
    e.name = i.name;
    e.text = i.description;
    e.status = i.status;
    auto res = service_.addEntity(e);
    if (res.failed()) return common::Result<void>::err(res.error());
    i.id = res.value().id;
    return common::Result<void>::ok();
}

common::Result<void> TraceGraph::addTestCase(persistence::TestCase& t) {
    Entity e;
    e.type = EntityType::TestCase;
    e.id = t.id;
    e.externalId = t.externalId.empty() ? defaultExternalId(EntityType::TestCase)
                                        : t.externalId;
    e.name = t.name;
    e.text = t.description;
    e.status = t.status;
    auto res = service_.addEntity(e);
    if (res.failed()) return common::Result<void>::err(res.error());
    t.id = res.value().id;
    t.externalId = res.value().externalId;
    t.version = res.value().version;
    return common::Result<void>::ok();
}

// ---------------------------------------------------------------------------
// Rich entity API.
// ---------------------------------------------------------------------------
common::Result<Entity> TraceGraph::addEntity(const Entity& e) {
    return service_.addEntity(e);
}

common::Result<std::optional<Entity>> TraceGraph::getEntity(EntityType type,
                                                            const std::string& id) {
    return service_.getEntity(type, id);
}

common::Result<std::vector<Entity>> TraceGraph::listEntities(EntityType type,
                                                             const EntityFilter& filter) {
    return service_.listEntities(type, filter);
}

common::Result<Entity> TraceGraph::updateEntity(const Entity& e) {
    return service_.updateEntity(e);
}

common::Result<void> TraceGraph::removeEntity(EntityType type, const std::string& id) {
    return service_.removeEntity(type, id);
}

common::Result<std::vector<Entity>> TraceGraph::search(EntityType type,
                                                       const std::string& text) {
    return service_.search(type, text);
}

// ---------------------------------------------------------------------------
// Links.
// ---------------------------------------------------------------------------
common::Result<void> TraceGraph::addLink(persistence::TraceLink& link) {
    Link l;
    l.id = link.id;
    l.sourceType = entityTypeFromString(link.sourceType).value_or(EntityType::Requirement);
    l.sourceId = link.sourceId;
    l.targetType = entityTypeFromString(link.targetType).value_or(EntityType::Requirement);
    l.targetId = link.targetId;
    l.relation = link.relation;
    l.rationale = link.rationale;
    l.status = link.status;
    auto res = service_.addLink(l);
    if (res.failed()) return common::Result<void>::err(res.error());
    // Map the returned typed link back onto the persistence model.
    link.id = res.value().id;
    link.status = res.value().status;
    link.rationale = res.value().rationale;
    link.version = res.value().version;
    link.createdAt = res.value().createdAt;
    link.updatedAt = res.value().updatedAt;
    return common::Result<void>::ok();
}

common::Result<persistence::TraceLink> TraceGraph::updateLink(
    const std::string& id, const std::string& rationale, const std::string& status) {
    auto found = service_.allLinks();
    if (found.failed()) return common::Result<persistence::TraceLink>::err(found.error());
    for (const auto& l : found.value()) {
        if (l.id != id) continue;
        Link upd = l;
        if (!rationale.empty()) upd.rationale = rationale;
        if (!status.empty()) upd.status = status;
        auto res = service_.updateLink(upd);
        if (res.failed()) return common::Result<persistence::TraceLink>::err(res.error());
        persistence::TraceLink pl;
        pl.id = res.value().id;
        pl.sourceType = toString(res.value().sourceType);
        pl.sourceId = res.value().sourceId;
        pl.targetType = toString(res.value().targetType);
        pl.targetId = res.value().targetId;
        pl.relation = res.value().relation;
        pl.rationale = res.value().rationale;
        pl.status = res.value().status;
        pl.version = res.value().version;
        pl.createdAt = res.value().createdAt;
        pl.updatedAt = res.value().updatedAt;
        return common::Result<persistence::TraceLink>::ok(pl);
    }
    return common::Result<persistence::TraceLink>::err("link not found: " + id);
}

common::Result<persistence::TraceLink> TraceGraph::removeLink(const std::string& id) {
    auto res = service_.removeLink(id);
    if (res.failed()) return common::Result<persistence::TraceLink>::err(res.error());
    return common::Result<persistence::TraceLink>::ok(persistence::TraceLink{});
}

common::Result<std::vector<persistence::TraceLink>> TraceGraph::linksFrom(
    const std::string& type, const std::string& id) {
    auto typeOpt = entityTypeFromString(type);
    if (!typeOpt) return common::Result<std::vector<persistence::TraceLink>>::err("unknown type");
    auto res = service_.linksFrom(*typeOpt, id);
    if (res.failed()) return common::Result<std::vector<persistence::TraceLink>>::err(res.error());
    std::vector<persistence::TraceLink> out;
    for (const auto& l : res.value()) {
        persistence::TraceLink pl;
        pl.id = l.id;
        pl.sourceType = toString(l.sourceType);
        pl.sourceId = l.sourceId;
        pl.targetType = toString(l.targetType);
        pl.targetId = l.targetId;
        pl.relation = l.relation;
        pl.rationale = l.rationale;
        pl.status = l.status;
        pl.version = l.version;
        out.push_back(pl);
    }
    return common::Result<std::vector<persistence::TraceLink>>::ok(std::move(out));
}

common::Result<std::vector<persistence::TraceLink>> TraceGraph::linksTo(
    const std::string& type, const std::string& id) {
    auto typeOpt = entityTypeFromString(type);
    if (!typeOpt) return common::Result<std::vector<persistence::TraceLink>>::err("unknown type");
    auto res = service_.linksTo(*typeOpt, id);
    if (res.failed()) return common::Result<std::vector<persistence::TraceLink>>::err(res.error());
    std::vector<persistence::TraceLink> out;
    for (const auto& l : res.value()) {
        persistence::TraceLink pl;
        pl.id = l.id;
        pl.sourceType = toString(l.sourceType);
        pl.sourceId = l.sourceId;
        pl.targetType = toString(l.targetType);
        pl.targetId = l.targetId;
        pl.relation = l.relation;
        pl.rationale = l.rationale;
        pl.status = l.status;
        pl.version = l.version;
        out.push_back(pl);
    }
    return common::Result<std::vector<persistence::TraceLink>>::ok(std::move(out));
}

common::Result<std::vector<persistence::TraceLink>> TraceGraph::allLinks() {
    auto res = service_.allLinks();
    if (res.failed()) return common::Result<std::vector<persistence::TraceLink>>::err(res.error());
    std::vector<persistence::TraceLink> out;
    for (const auto& l : res.value()) {
        persistence::TraceLink pl;
        pl.id = l.id;
        pl.sourceType = toString(l.sourceType);
        pl.sourceId = l.sourceId;
        pl.targetType = toString(l.targetType);
        pl.targetId = l.targetId;
        pl.relation = l.relation;
        pl.rationale = l.rationale;
        pl.status = l.status;
        pl.version = l.version;
        out.push_back(pl);
    }
    return common::Result<std::vector<persistence::TraceLink>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Legacy queries.
// ---------------------------------------------------------------------------
common::Result<std::vector<persistence::Requirement>> TraceGraph::requirements() {
    persistence::RequirementDao dao(db_);
    return dao.findAll();
}

common::Result<std::vector<persistence::TestCase>> TraceGraph::testCases() {
    persistence::TestCaseDao dao(db_);
    return dao.findAll();
}

}  // namespace lodestar::tracelink
