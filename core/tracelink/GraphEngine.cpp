// core/tracelink/GraphEngine.cpp
// WP-2 graph engine: transitive closure, impact analysis, coverage, and the
// trace matrix, built on top of TraceLinkService.

#include "core/tracelink/GraphEngine.h"

#include <deque>
#include <map>
#include <utility>

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Bijective reverse-relation mapping (WP-2 contract).
// ---------------------------------------------------------------------------
std::string GraphEngine::reverseRelation(const std::string& relation) {
    static const std::vector<std::pair<std::string, std::string>> kMap = {
        {"verifies",    "is_verified_by"},
        {"satisfies",   "is_satisfied_by"},
        {"derives",     "is_derived_from"},
        {"allocates",   "is_allocated_to"},
        {"refines",     "is_refined_by"},
        {"decomposes",  "is_decomposed_into"},
        {"depends_on",  "is_dependency_for"},
        {"traces_to",   "is_traced_by"},
        {"validates",   "is_validated_by"},
        {"conflicts",   "is_conflicted_with"},
    };
    for (const auto& p : kMap) {
        if (relation == p.first) return p.second;
        if (relation == p.second) return p.first;
    }
    return relation;  // unknown -> identity
}

GraphEngine::GraphEngine(persistence::Database& db) : db_(db), service_(db) {}

GraphNode GraphEngine::toNode(const Entity& e) const {
    GraphNode n;
    n.type = e.type;
    n.id = e.id;
    n.externalId = e.externalId;
    n.name = e.name;
    return n;
}

common::Result<std::optional<Entity>> GraphEngine::getEntity(EntityType type,
                                                             const std::string& id) {
    return service_.getEntity(type, id);
}

// ---------------------------------------------------------------------------
// Core BFS traversal. Out follows stored edges source->target; In reverses.
// The start node is never included in the result.
// ---------------------------------------------------------------------------
common::Result<std::vector<GraphNode>> GraphEngine::traverse(
    EntityType type, const std::string& id, const std::string& relation,
    Direction direction, int depth) {
    auto linksRes = service_.allLinks();
    if (linksRes.failed()) {
        return common::Result<std::vector<GraphNode>>::err(linksRes.error());
    }

    // Adjacency: out (source->target) and in (target->source).
    std::map<NodeKey, std::vector<std::pair<NodeKey, std::string>>> outEdges;
    std::map<NodeKey, std::vector<std::pair<NodeKey, std::string>>> inEdges;
    for (const auto& l : linksRes.value()) {
        if (l.status == "Superseded") continue;  // ignore historical links
        NodeKey src{l.sourceType, l.sourceId};
        NodeKey tgt{l.targetType, l.targetId};
        outEdges[src].emplace_back(tgt, l.relation);
        inEdges[tgt].emplace_back(src, l.relation);
    }

    // Relation filter: "" / "*" / "any" matches all; otherwise accept either
    // the forward or the reverse name (bijective map).
    const bool matchAny = relation.empty() || relation == "*" || relation == "any";
    auto relMatch = [&](const std::string& rel) -> bool {
        if (matchAny) return true;
        if (rel == relation) return true;
        return reverseRelation(relation) == rel;
    };

    NodeKey start{type, id};
    std::map<NodeKey, bool> visited;
    std::deque<std::pair<NodeKey, int>> frontier;
    frontier.emplace_back(start, 0);
    visited[start] = true;

    std::vector<NodeKey> collected;
    while (!frontier.empty()) {
        auto [cur, level] = frontier.front();
        frontier.pop_front();
        if (depth > 0 && level >= depth) continue;

        const auto* edges = (direction == Direction::Out) ? &outEdges : &inEdges;
        auto it = edges->find(cur);
        if (it == edges->end()) continue;
        for (const auto& [nb, rel] : it->second) {
            if (!relMatch(rel)) continue;
            if (visited[nb]) continue;
            visited[nb] = true;
            frontier.emplace_back(nb, level + 1);
            collected.push_back(nb);
        }
    }

    std::vector<GraphNode> out;
    for (const auto& nk : collected) {
        auto e = getEntity(nk.type, nk.id);
        if (e.isOk() && e.value().has_value()) out.push_back(toNode(*e.value()));
    }
    return common::Result<std::vector<GraphNode>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Closure.
// ---------------------------------------------------------------------------
common::Result<std::vector<GraphNode>> GraphEngine::downstreamClosure(
    EntityType type, const std::string& id, int depth) {
    return traverse(type, id, "", Direction::Out, depth);
}

common::Result<std::vector<GraphNode>> GraphEngine::upstreamClosure(
    EntityType type, const std::string& id, int depth) {
    return traverse(type, id, "", Direction::In, depth);
}

common::Result<std::vector<GraphNode>> GraphEngine::graphQuery(
    EntityType type, const std::string& id, const std::string& relation,
    Direction direction, int depth) {
    return traverse(type, id, relation, direction, depth);
}

// ---------------------------------------------------------------------------
// Impact analysis.
// ---------------------------------------------------------------------------
common::Result<ImpactAnalysis> GraphEngine::impactAnalysis(EntityType type,
                                                           const std::string& id) {
    ImpactAnalysis result;

    auto downRes = traverse(type, id, "", Direction::Out, 0);
    if (downRes.failed()) return common::Result<ImpactAnalysis>::err(downRes.error());
    auto upRes = traverse(type, id, "", Direction::In, 0);
    if (upRes.failed()) return common::Result<ImpactAnalysis>::err(upRes.error());

    // affectedEntities = union(upstream, downstream) + self node.
    std::map<std::string, bool> seen;  // key = type:id
    auto addNode = [&](const GraphNode& n) {
        const std::string key = toString(n.type) + ":" + n.id;
        if (seen[key]) return;
        seen[key] = true;
        result.affectedEntities.push_back(n);
    };
    for (const auto& n : downRes.value()) addNode(n);
    for (const auto& n : upRes.value()) addNode(n);
    auto selfRes = getEntity(type, id);
    if (selfRes.isOk() && selfRes.value().has_value()) {
        addNode(toNode(*selfRes.value()));
    }

    // Downstream test cases among affected entities.
    for (const auto& n : result.affectedEntities) {
        if (n.type == EntityType::TestCase) result.downstreamTestCases.push_back(n);
    }

    auto linksRes = service_.allLinks();
    if (linksRes.failed()) return common::Result<ImpactAnalysis>::err(linksRes.error());

    // blocked transitions for affected requirements.
    auto isAffected = [&](const std::string& ntype, const std::string& nid) {
        const std::string key = ntype + ":" + nid;
        return seen.find(key) != seen.end();
    };

    // affectedLinks: Active links whose source AND target are both affected.
    for (const auto& l : linksRes.value()) {
        if (l.status != "Active") continue;
        const std::string st = toString(l.sourceType);
        const std::string tt = toString(l.targetType);
        if (isAffected(st, l.sourceId) && isAffected(tt, l.targetId)) {
            GraphLink gl;
            gl.id = l.id;
            gl.sourceId = l.sourceId;
            gl.targetId = l.targetId;
            gl.relation = l.relation;
            result.affectedLinks.push_back(gl);
        }
    }

    // blockedTransitions: for each affected requirement, add the transitions it
    // cannot reach because required Active evidence is missing.
    std::map<std::string, bool> hasSatisfies;  // reqId -> has Active satisfies
    std::map<std::string, bool> hasVerifies;   // reqId -> has Active verifies
    for (const auto& l : linksRes.value()) {
        if (l.status != "Active") continue;
        if (l.targetType != EntityType::Requirement) continue;
        if (l.relation == "satisfies") hasSatisfies[l.targetId] = true;
        if (l.relation == "verifies") hasVerifies[l.targetId] = true;
    }
    for (const auto& n : result.affectedEntities) {
        if (n.type != EntityType::Requirement) continue;
        if (!hasSatisfies[n.id]) {
            result.blockedTransitions.push_back(n.externalId + " -> Implemented");
        }
        if (!hasVerifies[n.id]) {
            result.blockedTransitions.push_back(n.externalId + " -> Verified");
        }
    }

    return common::Result<ImpactAnalysis>::ok(std::move(result));
}

// ---------------------------------------------------------------------------
// Coverage: one row per requirement (designs satisfying, tests verifying).
// ---------------------------------------------------------------------------
common::Result<CoverageReport> GraphEngine::coverage() {
    auto reqsRes = service_.listEntities(EntityType::Requirement, EntityFilter{});
    if (reqsRes.failed()) {
        return common::Result<CoverageReport>::err(reqsRes.error());
    }
    auto linksRes = service_.allLinks();
    if (linksRes.failed()) {
        return common::Result<CoverageReport>::err(linksRes.error());
    }

    CoverageReport report;
    for (const auto& req : reqsRes.value()) {
        CoverageRow row;
        row.requirementId = req.id;
        row.requirementExternalId = req.externalId;
        for (const auto& l : linksRes.value()) {
            if (l.status != "Active") continue;
            if (l.targetType != EntityType::Requirement || l.targetId != req.id) continue;
            if (l.relation == "satisfies") {
                bool dup = false;
                for (const auto& d : row.satisfyingDesignIds) if (d == l.sourceId) dup = true;
                if (!dup) row.satisfyingDesignIds.push_back(l.sourceId);
            } else if (l.relation == "verifies") {
                bool dup = false;
                for (const auto& t : row.verifyingTestIds) if (t == l.sourceId) dup = true;
                if (!dup) row.verifyingTestIds.push_back(l.sourceId);
            }
        }
        row.designedCount = static_cast<int>(row.satisfyingDesignIds.size());
        row.verifiedCount = static_cast<int>(row.verifyingTestIds.size());
        row.percentDesigned = row.designedCount > 0 ? 100 : 0;
        row.percentVerified = row.verifiedCount > 0 ? 100 : 0;
        row.percentSatisfied = (row.designedCount > 0 && row.verifiedCount > 0) ? 100 : 0;
        report.rows.push_back(row);
    }
    return common::Result<CoverageReport>::ok(std::move(report));
}

// ---------------------------------------------------------------------------
// Coverage gaps: listed for every requirement; flags indicate missing coverage.
// ---------------------------------------------------------------------------
common::Result<std::vector<CoverageGap>> GraphEngine::coverageGap() {
    auto covRes = coverage();
    if (covRes.failed()) {
        return common::Result<std::vector<CoverageGap>>::err(covRes.error());
    }
    std::vector<CoverageGap> gaps;
    for (const auto& row : covRes.value().rows) {
        CoverageGap gap;
        gap.requirementId = row.requirementId;
        gap.requirementExternalId = row.requirementExternalId;
        gap.hasNoDesign = row.designedCount == 0;
        gap.hasNoTest = row.verifiedCount == 0;
        gaps.push_back(gap);
    }
    return common::Result<std::vector<CoverageGap>>::ok(std::move(gaps));
}

// ---------------------------------------------------------------------------
// Trace matrix: rows = requirements, columns = design then test, cell relation.
// ---------------------------------------------------------------------------
common::Result<TraceMatrix> GraphEngine::traceMatrix() {
    auto reqsRes = service_.listEntities(EntityType::Requirement, EntityFilter{});
    if (reqsRes.failed()) {
        return common::Result<TraceMatrix>::err(reqsRes.error());
    }
    auto linksRes = service_.allLinks();
    if (linksRes.failed()) {
        return common::Result<TraceMatrix>::err(linksRes.error());
    }

    TraceMatrix matrix;
    std::vector<std::string> designCols;
    std::vector<std::string> testCols;
    auto pushUnique = [](std::vector<std::string>& v, const std::string& s) {
        for (const auto& x : v) if (x == s) return;
        v.push_back(s);
    };

    for (const auto& l : linksRes.value()) {
        if (l.status != "Active") continue;
        if (l.targetType != EntityType::Requirement) continue;
        if (l.relation == "satisfies") pushUnique(designCols, l.sourceId);
        else if (l.relation == "verifies") pushUnique(testCols, l.sourceId);
    }

    for (const auto& id_ : designCols) matrix.columnIds.push_back(id_);
    for (const auto& id_ : testCols) matrix.columnIds.push_back(id_);

    for (const auto& req : reqsRes.value()) {
        TraceMatrixRow row;
        row.requirementId = req.id;
        row.requirementExternalId = req.externalId;

        std::map<std::string, std::string> relByCol;  // colId -> relation
        for (const auto& l : linksRes.value()) {
            if (l.status != "Active") continue;
            if (l.targetType != EntityType::Requirement || l.targetId != req.id) continue;
            if (l.relation == "satisfies") relByCol[l.sourceId] = "satisfies";
            else if (l.relation == "verifies") relByCol[l.sourceId] = "verifies";
        }

        for (const auto& cid : designCols) {
            MatrixCell c;
            c.columnId = cid;
            c.columnType = "design";
            c.relation = relByCol[cid];
            row.cells.push_back(c);
        }
        for (const auto& cid : testCols) {
            MatrixCell c;
            c.columnId = cid;
            c.columnType = "test_case";
            c.relation = relByCol[cid];
            row.cells.push_back(c);
        }
        matrix.rows.push_back(row);
    }

    return common::Result<TraceMatrix>::ok(std::move(matrix));
}

}  // namespace lodestar::tracelink
