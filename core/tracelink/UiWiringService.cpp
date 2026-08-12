#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/UiWiringService.cpp
// WP-G Qt-independent wiring layer implementation. Delegates to the WP-7
// ViewModelFactory to build the four view models in one pass, guaranteeing the
// cross-model consistency invariant (matrix rows == coverage items == number of
// requirements; graph nodes == all active entities).

#include "core/tracelink/UiWiringService.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::tracelink {

namespace {

// Orders a list of sibling entities by sortOrder then id (same rule the
// service uses for hierarchy children).
void sortSiblings(std::vector<Entity>& v) {
    std::sort(v.begin(), v.end(), [](const Entity& a, const Entity& b) {
        if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
        return a.id < b.id;
    });
}

// Deduplicates a child list by entity id (a node may be reachable both via a
// parentId and via an Active link; it must still appear only once).
void dedupeChildren(std::vector<Entity>& v) {
    std::set<std::string> seen;
    std::vector<Entity> out;
    for (auto& e : v) {
        if (seen.insert(e.id).second) out.push_back(e);
    }
    v = std::move(out);
}

}  // namespace

UiWiringService::UiWiringService(persistence::Database& db) : db_(db) {}

common::Result<UiSnapshot> UiWiringService::refreshAll() {
    ViewModelFactory factory(db_);

    UiSnapshot snap;

    auto matrix = factory.matrix();
    if (matrix.failed()) {
        return common::Result<UiSnapshot>::err(matrix.error());
    }
    snap.matrix = std::move(matrix.value());

    auto coverage = factory.coverageDashboard();
    if (coverage.failed()) {
        return common::Result<UiSnapshot>::err(coverage.error());
    }
    snap.coverage = std::move(coverage.value());

    auto graph = factory.graph();
    if (graph.failed()) {
        return common::Result<UiSnapshot>::err(graph.error());
    }
    snap.graph = std::move(graph.value());

    // Impact tab: one impact model per requirement (the same focus the
    // MainWindow uses when it defaults to the first requirement).
    snap.impacts.reserve(snap.matrix.rows.size());
    for (const auto& row : snap.matrix.rows) {
        auto imp = factory.impact(EntityType::Requirement, row.requirementId);
        if (imp.failed()) {
            return common::Result<UiSnapshot>::err(imp.error());
        }
        snap.impacts.push_back(std::move(imp.value()));
    }

    return common::Result<UiSnapshot>::ok(std::move(snap));
}

common::Result<ImpactViewModel> UiWiringService::impact(EntityType type,
                                                        const std::string& id) {
    // The ImpactView path must fail cleanly for a nonexistent entity rather
    // than returning an empty impact model (the factory builds a tree rooted
    // at the requested node even when it does not exist).
    TraceLinkService svc(db_);
    auto ent = svc.getEntity(type, id);
    if (ent.failed()) {
        return common::Result<ImpactViewModel>::err(ent.error());
    }
    if (!ent.value().has_value()) {
        return common::Result<ImpactViewModel>::err(
            common::ErrorCode::NotFound, "entity not found: " + id);
    }

    ViewModelFactory factory(db_);
    return factory.impact(type, id);
}

common::Result<std::vector<ProjectTreeNode>> UiWiringService::projectTree() {
    TraceLinkService svc(db_);

    // Gather every active entity across all types, keyed by id.
    std::map<std::string, Entity> byId;
    const std::vector<EntityType> kTypes = {
        EntityType::Requirement, EntityType::Design,   EntityType::Interface,
        EntityType::TestCase,    EntityType::Hazard,  EntityType::Decision,
        EntityType::Assumption};
    for (auto type : kTypes) {
        auto ents = svc.listEntities(type, EntityFilter{});
        if (ents.failed()) {
            return common::Result<std::vector<ProjectTreeNode>>::err(ents.error());
        }
        for (auto& e : ents.value()) {
            if (e.status == "Obsolete") continue;
            byId[e.id] = e;
        }
    }

    // childMap: parent id -> ordered children. A node X is a child of Y when
    // X.parentId == Y.id (setParent hierarchy) OR there is an Active link from
    // X to Y (cross-type nesting, e.g. design/test under a requirement).
    std::map<std::string, std::vector<Entity>> childMap;
    std::set<std::string> hasParent;  // entities that are a child of something
    for (auto& kv : byId) {
        const Entity& e = kv.second;
        if (!e.parentId.empty()) {
            childMap[e.parentId].push_back(e);
            hasParent.insert(e.id);
        }
    }

    auto links = svc.allLinks();
    if (links.failed()) {
        return common::Result<std::vector<ProjectTreeNode>>::err(links.error());
    }
    for (const auto& l : links.value()) {
        if (l.status != "Active") continue;
        if (l.sourceId == l.targetId) continue;
        auto srcIt = byId.find(l.sourceId);
        auto tgtIt = byId.find(l.targetId);
        if (srcIt == byId.end() || tgtIt == byId.end()) continue;
        childMap[l.targetId].push_back(srcIt->second);
        hasParent.insert(l.sourceId);
    }

    // Roots: active entities with no parent and no incoming Active link.
    std::vector<Entity> roots;
    for (auto& kv : byId) {
        const Entity& e = kv.second;
        if (e.parentId.empty() && hasParent.find(e.id) == hasParent.end()) {
            roots.push_back(e);
        }
    }
    sortSiblings(roots);

    // Recursively build the tree. A shared visited set guarantees every active
    // entity appears exactly once and guards against link cycles.
    std::set<std::string> visited;
    std::function<ProjectTreeNode(const Entity&)> build =
        [&](const Entity& e) {
            ProjectTreeNode node;
            node.id = e.id;
            node.externalId = e.externalId;
            node.type = toString(e.type);
            node.name = e.name;
            visited.insert(e.id);

            auto it = childMap.find(e.id);
            if (it != childMap.end()) {
                auto kids = it->second;
                dedupeChildren(kids);
                sortSiblings(kids);
                for (auto& k : kids) {
                    if (visited.find(k.id) != visited.end()) continue;
                    node.children.push_back(build(k));
                }
            }
            return node;
        };

    std::vector<ProjectTreeNode> out;
    out.reserve(roots.size());
    for (auto& r : roots) {
        if (visited.find(r.id) != visited.end()) continue;
        out.push_back(build(r));
    }
    return common::Result<std::vector<ProjectTreeNode>>::ok(std::move(out));
}

common::Result<DetailPanelModel> UiWiringService::detail(EntityType type,
                                                          const std::string& id) {
    TraceLinkService svc(db_);
    auto entRes = svc.getEntity(type, id);
    if (entRes.failed()) {
        return common::Result<DetailPanelModel>::err(entRes.error());
    }
    if (!entRes.value().has_value()) {
        return common::Result<DetailPanelModel>::err(
            common::ErrorCode::NotFound, "entity not found: " + id);
    }
    const Entity& e = entRes.value().value();

    DetailPanelModel m;
    m.id = e.id;
    m.externalId = e.externalId;
    m.type = toString(e.type);
    m.name = e.name;
    m.status = e.status;
    m.owner = e.owner;
    m.priority = e.priority;
    m.verificationMethod = e.verificationMethod;
    m.safetyLevel = e.safetyLevel;
    m.version = e.version;

    // Incoming links: Active links where this entity is the target.
    auto inRes = svc.linksTo(type, id);
    if (inRes.failed()) {
        return common::Result<DetailPanelModel>::err(inRes.error());
    }
    for (const auto& l : inRes.value()) {
        if (l.status != "Active") continue;
        auto src = svc.getEntity(l.sourceType, l.sourceId);
        if (src.failed() || !src.value().has_value()) continue;
        m.incomingLinks.push_back(l.relation + ": " + src.value().value().externalId);
    }

    // Outgoing links: Active links where this entity is the source.
    auto outRes = svc.linksFrom(type, id);
    if (outRes.failed()) {
        return common::Result<DetailPanelModel>::err(outRes.error());
    }
    for (const auto& l : outRes.value()) {
        if (l.status != "Active") continue;
        auto tgt = svc.getEntity(l.targetType, l.targetId);
        if (tgt.failed() || !tgt.value().has_value()) continue;
        m.outgoingLinks.push_back(l.relation + ": " + tgt.value().value().externalId);
    }

    return common::Result<DetailPanelModel>::ok(std::move(m));
}

}  // namespace lodestar::tracelink
