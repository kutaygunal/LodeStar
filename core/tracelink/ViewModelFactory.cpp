#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/ViewModelFactory.cpp
// WP-7 Qt-independent view-model layer implementation. Reuses GraphEngine and
// RulesEngine to produce the data backing the four Qt views.

#include "core/tracelink/ViewModelFactory.h"

#include <cstdio>
#include <map>
#include <optional>
#include <string>

#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/TraceLinkService.h"

namespace lodestar::tracelink {

namespace {

// A node key used for BFS adjacency during impact tree construction.
struct NodeKey {
    EntityType type;
    std::string id;
    bool operator<(const NodeKey& o) const {
        if (type != o.type) return static_cast<int>(type) < static_cast<int>(o.type);
        return id < o.id;
    }
    bool operator==(const NodeKey& o) const {
        return type == o.type && id == o.id;
    }
};

std::string escHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default:  out.push_back(c); break;
        }
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// MatrixViewModel
// ---------------------------------------------------------------------------
std::string MatrixViewModel::cell(int row, int col) const {
    if (row < 0 || row >= static_cast<int>(rows.size())) return "";
    if (col < 0 || col >= static_cast<int>(columns.size())) return "";
    if (col >= static_cast<int>(rows[static_cast<size_t>(row)].cellRelations.size()))
        return "";
    return rows[static_cast<size_t>(row)].cellRelations[static_cast<size_t>(col)];
}

common::Result<std::string> MatrixViewModel::toCsv() const {
    std::string out;
    // Header: first cell is the requirement column label.
    out += "Requirement";
    for (const auto& c : columns) out += "," + c.name;
    out += "\n";

    for (const auto& r : rows) {
        out += r.requirementExternalId;
        for (size_t i = 0; i < columns.size(); ++i) {
            out += ",";
            if (i < r.cellRelations.size() && !r.cellRelations[i].empty())
                out += r.cellRelations[i];
        }
        out += "\n";
    }
    return common::Result<std::string>::ok(std::move(out));
}

common::Result<std::string> MatrixViewModel::toHtml() const {
    std::string out;
    out += "<html><head><meta charset=\"utf-8\">"
           "<title>Trace Matrix</title>"
           "<style>table{border-collapse:collapse}th,td{border:1px solid #888;"
           "padding:4px 8px;font-size:12px}th{background:#eef}</style></head>";
    out += "<body><h1>Trace Matrix</h1><table><thead><tr><th>Requirement</th>";
    for (const auto& c : columns) out += "<th>" + escHtml(c.name) + "</th>";
    out += "</tr></thead><tbody>";

    for (const auto& r : rows) {
        out += "<tr><td>" + escHtml(r.requirementExternalId) + "</td>";
        for (size_t i = 0; i < columns.size(); ++i) {
            out += "<td>";
            if (i < r.cellRelations.size() && !r.cellRelations[i].empty())
                out += escHtml(r.cellRelations[i]);
            out += "</td>";
        }
        out += "</tr>";
    }
    out += "</tbody></table></body></html>";
    return common::Result<std::string>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// ViewModelFactory
// ---------------------------------------------------------------------------
ViewModelFactory::ViewModelFactory(persistence::Database& db) : db_(db) {}

common::Result<MatrixViewModel> ViewModelFactory::matrix() {
    GraphEngine engine(db_);
    TraceLinkService svc(db_);

    auto tm = engine.traceMatrix();
    if (tm.failed()) {
        return common::Result<MatrixViewModel>::err(tm.error());
    }

    // Resolve each design/test column's type + external name.
    std::map<std::string, Entity> byId;
    auto collect = [&](EntityType t) {
        auto res = svc.listEntities(t, EntityFilter{});
        if (res.isOk()) {
            for (const auto& e : res.value()) byId[e.id] = e;
        }
    };
    collect(EntityType::Design);
    collect(EntityType::TestCase);

    MatrixViewModel vm;
    for (const auto& cid : tm.value().columnIds) {
        MatrixViewModel::Column col;
        col.id = cid;
        auto it = byId.find(cid);
        if (it != byId.end()) {
            col.name = it->second.externalId;
            col.type = (it->second.type == EntityType::Design) ? "design" : "test_case";
        } else {
            col.name = cid;
            col.type = "test_case";
        }
        vm.columns.push_back(std::move(col));
    }

    for (const auto& tr : tm.value().rows) {
        MatrixViewModel::Row r;
        r.requirementId = tr.requirementId;
        r.requirementExternalId = tr.requirementExternalId;
        for (const auto& c : tr.cells) r.cellRelations.push_back(c.relation);
        vm.rows.push_back(std::move(r));
    }
    return common::Result<MatrixViewModel>::ok(std::move(vm));
}

common::Result<CoverageDashboardModel> ViewModelFactory::coverageDashboard() {
    GraphEngine engine(db_);
    RulesEngine rules(db_);

    auto cov = engine.coverage();
    if (cov.failed()) {
        return common::Result<CoverageDashboardModel>::err(cov.error());
    }

    CoverageDashboardModel dm;
    double sumDesigned = 0.0;
    double sumVerified = 0.0;
    for (const auto& cr : cov.value().rows) {
        CoverageDashboardModel::Item it;
        it.requirementId = cr.requirementId;
        it.requirementExternalId = cr.requirementExternalId;
        it.percentDesigned = cr.percentDesigned;
        it.percentVerified = cr.percentVerified;
        it.percentSatisfied = (it.percentDesigned + it.percentVerified) / 2;
        sumDesigned += static_cast<double>(it.percentDesigned);
        sumVerified += static_cast<double>(it.percentVerified);
        dm.items.push_back(std::move(it));
    }

    const size_t n = dm.items.empty() ? 1 : dm.items.size();
    dm.overallPercentDesigned = sumDesigned / static_cast<double>(n);
    dm.overallPercentVerified = sumVerified / static_cast<double>(n);

    // Run the enabled compliance rules to surface active violations.
    auto run = rules.runValidation();
    if (run.isOk()) {
        dm.violationCount = run.value().violationCount;
        dm.violations = run.value().violations;
    }
    return common::Result<CoverageDashboardModel>::ok(std::move(dm));
}

common::Result<ImpactViewModel> ViewModelFactory::impact(EntityType type,
                                                         const std::string& id) {
    GraphEngine engine(db_);
    TraceLinkService svc(db_);

    // blocked transitions + downstream test cases from the engine.
    auto ia = engine.impactAnalysis(type, id);
    if (ia.failed()) {
        return common::Result<ImpactViewModel>::err(ia.error());
    }

    ImpactViewModel vm;
    vm.blockedTransitions = ia.value().blockedTransitions;
    for (const auto& t : ia.value().downstreamTestCases) {
        vm.downstreamTests.push_back(t.externalId.empty() ? t.id : t.externalId);
    }

    // Build the affected tree (flat, with depth) via bidirectional BFS so each
    // node records its distance from the changed node (depth 0 = the node).
    auto linksRes = svc.allLinks();
    if (linksRes.failed()) {
        return common::Result<ImpactViewModel>::err(linksRes.error());
    }
    std::map<NodeKey, std::vector<NodeKey>> outEdges;
    std::map<NodeKey, std::vector<NodeKey>> inEdges;
    for (const auto& l : linksRes.value()) {
        if (l.status != "Active") continue;
        NodeKey src{l.sourceType, l.sourceId};
        NodeKey tgt{l.targetType, l.targetId};
        outEdges[src].push_back(tgt);
        inEdges[tgt].push_back(src);
    }

    std::map<NodeKey, int> depth;
    std::vector<NodeKey> queue;
    NodeKey start{type, id};
    depth[start] = 0;
    queue.push_back(start);
    for (size_t i = 0; i < queue.size(); ++i) {
        NodeKey cur = queue[i];
        int d = depth[cur];
        const auto process = [&](const std::map<NodeKey, std::vector<NodeKey>>& adj) {
            auto it = adj.find(cur);
            if (it == adj.end()) return;
            for (const auto& nb : it->second) {
                if (depth.count(nb)) continue;
                depth[nb] = d + 1;
                queue.push_back(nb);
            }
        };
        process(outEdges);
        process(inEdges);
    }

    // Populate affected nodes in BFS order (start first, then by depth).
    auto getEntity = [&](NodeKey k) -> std::optional<Entity> {
        auto r = svc.getEntity(k.type, k.id);
        if (r.isOk() && r.value().has_value()) return *r.value();
        return std::nullopt;
    };
    for (const auto& k : queue) {
        auto e = getEntity(k);
        ImpactViewModel::Node node;
        node.id = k.id;
        node.depth = depth[k];
        node.affected = true;
        node.type = toString(k.type);
        if (e.has_value()) {
            node.externalId = e->externalId;
        }
        vm.affected.push_back(std::move(node));
    }

    return common::Result<ImpactViewModel>::ok(std::move(vm));
}

common::Result<GraphViewModel> ViewModelFactory::graph() {
    TraceLinkService svc(db_);

    GraphViewModel gm;
    const EntityType allTypes[] = {
        EntityType::Requirement, EntityType::Design,    EntityType::Interface,
        EntityType::TestCase,    EntityType::Hazard,    EntityType::Decision,
        EntityType::Assumption};
    for (auto t : allTypes) {
        auto res = svc.listEntities(t, EntityFilter{});
        if (res.failed()) {
            return common::Result<GraphViewModel>::err(res.error());
        }
        for (const auto& e : res.value()) {
            if (e.status == "Obsolete") continue;
            GraphViewModel::Node n;
            n.id = e.id;
            n.externalId = e.externalId;
            n.type = toString(e.type);
            gm.nodes.push_back(std::move(n));
        }
    }

    auto links = svc.allLinks();
    if (links.failed()) {
        return common::Result<GraphViewModel>::err(links.error());
    }
    for (const auto& l : links.value()) {
        if (l.status != "Active") continue;
        GraphViewModel::Edge e;
        e.sourceId = l.sourceId;
        e.targetId = l.targetId;
        e.relation = l.relation;
        gm.edges.push_back(std::move(e));
    }
    return common::Result<GraphViewModel>::ok(std::move(gm));
}

}  // namespace lodestar::tracelink
