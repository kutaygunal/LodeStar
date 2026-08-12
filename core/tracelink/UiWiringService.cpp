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
#include <cctype>
#include <functional>
#include <map>
#include <set>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/ViewModelFactory.h"
#include "core/tracelink/CoverageService.h"

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

common::Result<std::vector<LiveCoverageRow>> UiWiringService::liveCoverage() {
    // Reuse the WP-5 CoverageService for the executed coverage semantics
    // (designed / verified / executed), then derive the red/green gap flags.
    CoverageService cov(db_);
    auto exec = cov.executedCoverage();
    if (exec.failed()) {
        return common::Result<std::vector<LiveCoverageRow>>::err(exec.error());
    }

    std::vector<LiveCoverageRow> rows;
    rows.reserve(exec.value().size());
    for (const auto& e : exec.value()) {
        LiveCoverageRow r;
        r.requirementId = e.requirementId;
        r.requirementExternalId = e.requirementExternalId;
        r.designed = e.designed;
        r.verified = e.verified;
        r.executed = e.executed;
        r.gapNoDesign = !e.designed;  // red: no design
        r.gapNoTest = !e.verified;    // red: no passing test
        rows.push_back(std::move(r));
    }
    return common::Result<std::vector<LiveCoverageRow>>::ok(std::move(rows));
}

common::Result<CoverageCharts> UiWiringService::coverageCharts() {
    CoverageService cov(db_);
    auto exec = cov.executedCoverage();
    if (exec.failed()) {
        return common::Result<CoverageCharts>::err(exec.error());
    }

    TraceLinkService svc(db_);
    auto reqs = svc.listEntities(EntityType::Requirement, EntityFilter{});
    if (reqs.failed()) {
        return common::Result<CoverageCharts>::err(reqs.error());
    }

    CoverageCharts charts;

    // byStatus / byPriority: aggregate across all requirements.
    std::map<std::string, int> statusCounts;
    std::map<std::string, int> priorityCounts;
    for (const auto& req : reqs.value()) {
        statusCounts[req.status]++;
        if (!req.priority.empty()) priorityCounts[req.priority]++;
    }
    for (const auto& kv : statusCounts) {
        charts.byStatus.push_back({kv.first, kv.second});
    }
    for (const auto& kv : priorityCounts) {
        charts.byPriority.push_back({kv.first, kv.second});
    }

    // byCoverage: Full = designed+verified, Partial = one of the two,
    // None = neither. Always emit all three slices for a deterministic chart.
    int full = 0, partial = 0, none = 0;
    for (const auto& e : exec.value()) {
        if (e.designed && e.verified) {
            ++full;
        } else if (e.designed || e.verified) {
            ++partial;
        } else {
            ++none;
        }
    }
    charts.byCoverage.push_back({"Full", full});
    charts.byCoverage.push_back({"Partial", partial});
    charts.byCoverage.push_back({"None", none});

    return common::Result<CoverageCharts>::ok(std::move(charts));
}

common::Result<std::vector<VisualDiffRow>> UiWiringService::visualDiff(
    const std::string& aId, const std::string& bId) {
    BaselineService bl(db_);
    auto diff = bl.diffBaseline(aId, bId);
    if (diff.failed()) {
        return common::Result<std::vector<VisualDiffRow>>::err(diff.error());
    }

    std::vector<VisualDiffRow> out;
    out.reserve(diff.value().entities.size());
    for (const auto& de : diff.value().entities) {
        VisualDiffRow row;
        row.entityId = de.entityId;
        row.entityExternalId = de.entityExternalId;
        switch (de.kind) {
            case DiffKind::Added:    row.kind = "added"; break;
            case DiffKind::Removed:  row.kind = "removed"; break;
            case DiffKind::Modified: row.kind = "modified"; break;
        }
        row.fieldChanges = de.fieldChanges;
        out.push_back(std::move(row));
    }
    return common::Result<std::vector<VisualDiffRow>>::ok(std::move(out));
}

common::Result<RollbackResult> UiWiringService::rollbackEntity(
    EntityType type, const std::string& id, const std::string& baselineId) {
    BaselineService bl(db_);

    // The entity must exist in the baseline snapshot.
    auto at = bl.entityAtBaseline(type, id, baselineId);
    if (at.failed()) {
        return common::Result<RollbackResult>::err(at.error());
    }
    if (!at.value().has_value()) {
        return common::Result<RollbackResult>::err(
            common::ErrorCode::NotFound,
            "entity not found in baseline " + baselineId + ": " + id);
    }

    auto res = bl.restoreEntity(type, id, baselineId);
    if (res.failed()) {
        return common::Result<RollbackResult>::err(res.error());
    }

    RollbackResult r;
    r.entityId = id;
    r.entityExternalId = at.value()->externalId;
    r.restored = true;
    return common::Result<RollbackResult>::ok(std::move(r));
}

common::Result<EntityType> UiWiringService::entityTypeOf(const std::string& id) {
    TraceLinkService svc(db_);
    const std::vector<EntityType> kTypes = {
        EntityType::Requirement, EntityType::Design,   EntityType::Interface,
        EntityType::TestCase,    EntityType::Hazard,  EntityType::Decision,
        EntityType::Assumption};
    for (auto type : kTypes) {
        auto ent = svc.getEntity(type, id);
        if (ent.failed()) {
            return common::Result<EntityType>::err(ent.error());
        }
        if (ent.value().has_value()) {
            return common::Result<EntityType>::ok(type);
        }
    }
    return common::Result<EntityType>::err(
        common::ErrorCode::NotFound, "entity not found: " + id);
}

// ---------------------------------------------------------------------------
// WP-8: interactive traceability matrix (search / filter / toggle / saved views)
// ---------------------------------------------------------------------------
namespace {

// Case-insensitive substring match helper.
bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    std::string h = haystack;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(n.begin(), n.end(), n.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return h.find(n) != std::string::npos;
}

// Serializes hidden relations as a '|'-delimited string for persistence.
std::string joinRelations(const std::vector<std::string>& rels) {
    std::string out;
    for (size_t i = 0; i < rels.size(); ++i) {
        if (i) out += "|";
        out += rels[i];
    }
    return out;
}

// Splits a '|'-delimited string back into a vector of relations.
std::vector<std::string> splitRelations(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '|') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

}  // namespace

common::Result<MatrixViewModel> UiWiringService::matrixFiltered(
    const MatrixViewConfig& cfg) {
    ViewModelFactory factory(db_);
    TraceLinkService svc(db_);

    auto base = factory.matrix();
    if (base.failed()) {
        return common::Result<MatrixViewModel>::err(base.error());
    }
    MatrixViewModel vm = std::move(base.value());

    // Build a set of hidden relations for O(1) lookup.
    std::set<std::string> hidden(cfg.hiddenRelations.begin(),
                                 cfg.hiddenRelations.end());

    // Filter rows by search text and status, and blank hidden-relation cells.
    std::vector<MatrixViewModel::Row> kept;
    kept.reserve(vm.rows.size());
    for (auto& row : vm.rows) {
        // Resolve the requirement entity for name/status matching.
        std::string name = row.requirementExternalId;
        std::string status;
        auto ent = svc.getEntity(EntityType::Requirement, row.requirementId);
        if (ent.isOk() && ent.value().has_value()) {
            name = ent.value().value().name;
            status = ent.value().value().status;
        }

        if (!cfg.search.empty()) {
            bool hit = containsIgnoreCase(name, cfg.search) ||
                       containsIgnoreCase(row.requirementExternalId, cfg.search);
            if (!hit) continue;
        }
        if (!cfg.statusFilter.empty() && status != cfg.statusFilter) continue;

        // Blank any cell whose relation is hidden.
        for (auto& rel : row.cellRelations) {
            if (hidden.count(rel)) rel.clear();
        }
        kept.push_back(std::move(row));
    }
    vm.rows = std::move(kept);

    return common::Result<MatrixViewModel>::ok(std::move(vm));
}

common::Result<void> UiWiringService::saveMatrixView(
    const std::string& name, const MatrixViewConfig& cfg) {
    if (name.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "matrix view name must not be empty");
    }
    std::string id = common::newUuid();
    std::string hidden = joinRelations(cfg.hiddenRelations);

    // Escape single quotes for safe SQL embedding.
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "''";
            else out.push_back(c);
        }
        return out;
    };

    std::string sql =
        "INSERT INTO matrix_views (id, name, search, status_filter, hidden_relations) "
        "VALUES ('" + esc(id) + "', '" + esc(name) + "', '" + esc(cfg.search) +
        "', '" + esc(cfg.statusFilter) + "', '" + esc(hidden) + "');";
    auto res = db_.execute(sql);
    if (res.failed()) {
        return common::Result<void>::err(common::ErrorCode::DatabaseError,
                                         "failed to save matrix view: " + res.error());
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<SavedMatrixView>> UiWiringService::listMatrixViews() {
    std::vector<SavedMatrixView> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, name, search, status_filter, hidden_relations "
        "FROM matrix_views ORDER BY name;";
    int rc = sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return common::Result<std::vector<SavedMatrixView>>::err(
            common::ErrorCode::DatabaseError, "failed to list matrix views");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedMatrixView v;
        v.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        v.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        v.config.search = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v.config.statusFilter =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        v.config.hiddenRelations =
            splitRelations(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        out.push_back(std::move(v));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<SavedMatrixView>>::ok(std::move(out));
}

common::Result<MatrixViewModel> UiWiringService::applyMatrixView(
    const std::string& viewId) {
    // Load the saved view config.
    MatrixViewConfig cfg;
    bool found = false;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT search, status_filter, hidden_relations "
        "FROM matrix_views WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return common::Result<MatrixViewModel>::err(
            common::ErrorCode::DatabaseError, "failed to load matrix view");
    }
    sqlite3_bind_text(stmt, 1, viewId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cfg.search = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        cfg.statusFilter =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        cfg.hiddenRelations =
            splitRelations(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        found = true;
    }
    sqlite3_finalize(stmt);

    if (!found) {
        return common::Result<MatrixViewModel>::err(
            common::ErrorCode::NotFound, "matrix view not found: " + viewId);
    }
    return matrixFiltered(cfg);
}

// ---------------------------------------------------------------------------
// WP-10: document-style authoring
// ---------------------------------------------------------------------------
common::Result<DocumentModel> UiWiringService::document(const std::string& docId) {
    TraceLinkService svc(db_);

    // The document root must exist and be a requirement.
    auto rootRes = svc.getEntity(EntityType::Requirement, docId);
    if (rootRes.failed()) {
        return common::Result<DocumentModel>::err(rootRes.error());
    }
    if (!rootRes.value().has_value()) {
        return common::Result<DocumentModel>::err(
            common::ErrorCode::NotFound, "document root not found: " + docId);
    }

    DocumentModel model;
    model.id = docId;
    model.title = rootRes.value().value().name;

    // Sections are the direct children of the document root.
    auto sections = svc.children(EntityType::Requirement, docId);
    if (sections.failed()) {
        return common::Result<DocumentModel>::err(sections.error());
    }
    for (const auto& sec : sections.value()) {
        DocumentSection ds;
        ds.id = sec.id;
        ds.title = sec.name;

        // Requirements are the direct children of each section.
        auto reqs = svc.children(EntityType::Requirement, sec.id);
        if (reqs.failed()) {
            return common::Result<DocumentModel>::err(reqs.error());
        }
        ds.requirements = std::move(reqs.value());
        model.sections.push_back(std::move(ds));
    }
    return common::Result<DocumentModel>::ok(std::move(model));
}

common::Result<Entity> UiWiringService::addRequirementToDocument(
    const std::string& docId, const std::string& sectionId, const Entity& req) {
    TraceLinkService svc(db_);

    // Validate the document root and the target section exist.
    auto docRes = svc.getEntity(EntityType::Requirement, docId);
    if (docRes.failed()) {
        return common::Result<Entity>::err(docRes.error());
    }
    if (!docRes.value().has_value()) {
        return common::Result<Entity>::err(
            common::ErrorCode::NotFound, "document root not found: " + docId);
    }
    auto secRes = svc.getEntity(EntityType::Requirement, sectionId);
    if (secRes.failed()) {
        return common::Result<Entity>::err(secRes.error());
    }
    if (!secRes.value().has_value()) {
        return common::Result<Entity>::err(
            common::ErrorCode::NotFound, "section not found: " + sectionId);
    }

    // Create the requirement, attach it to the section, and link it to the
    // section for atomic traceability. All three steps must succeed together.
    auto created = svc.addEntity(req);
    if (created.failed()) {
        return created;
    }
    const std::string newId = created.value().id;

    auto parent = svc.setParent(EntityType::Requirement, newId, sectionId);
    if (parent.failed()) {
        return common::Result<Entity>::err(parent.error());
    }

    // A requirement -> section "refines" link records the traceability.
    Link link;
    link.sourceType = EntityType::Requirement;
    link.sourceId = newId;
    link.targetType = EntityType::Requirement;
    link.targetId = sectionId;
    link.relation = "refines";
    auto linkRes = svc.addLink(link);
    if (linkRes.failed()) {
        return common::Result<Entity>::err(linkRes.error());
    }

    return created;
}

common::Result<void> UiWiringService::reorderRequirements(
    const std::string& docId, const std::string& sectionId,
    const std::vector<std::string>& orderedIds) {
    TraceLinkService svc(db_);

    // Validate the document root and the section exist.
    auto docRes = svc.getEntity(EntityType::Requirement, docId);
    if (docRes.failed()) {
        return common::Result<void>::err(docRes.error());
    }
    if (!docRes.value().has_value()) {
        return common::Result<void>::err(
            common::ErrorCode::NotFound, "document root not found: " + docId);
    }
    auto secRes = svc.getEntity(EntityType::Requirement, sectionId);
    if (secRes.failed()) {
        return common::Result<void>::err(secRes.error());
    }
    if (!secRes.value().has_value()) {
        return common::Result<void>::err(
            common::ErrorCode::NotFound, "section not found: " + sectionId);
    }

    return svc.reorder(EntityType::Requirement, sectionId, orderedIds);
}

}  // namespace lodestar::tracelink
