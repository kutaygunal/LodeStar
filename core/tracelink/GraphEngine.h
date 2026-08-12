#pragma once
// core/tracelink/GraphEngine.h
// WP-2 graph engine: transitive closure, impact analysis, coverage, and the
// trace matrix. Built on top of TraceLinkService (typed entities + typed,
// directed edges). Traversal walks the canonical directed graph; a link
// source->target is followed "outward" (Out) or reversed (In) so a single
// stored edge serves both directions.
//
// Contract written by the scrum-master in core/test/wp2_graph_engine_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Traversal direction. Out follows a stored link source->target; In follows it
// in reverse (target->source).
// ---------------------------------------------------------------------------
enum class Direction { Out, In };

// A lightweight view of an entity node for graph results.
struct GraphNode {
    EntityType  type = EntityType::Requirement;
    std::string id;
    std::string externalId;
    std::string name;
};

// A lightweight view of a link for graph results.
struct GraphLink {
    std::string id;
    std::string sourceId;
    std::string targetId;
    std::string relation;
};

// One requirement's coverage row.
struct CoverageRow {
    std::string requirementId;
    std::string requirementExternalId;
    std::vector<std::string> satisfyingDesignIds;  // designs that satisfies->it
    std::vector<std::string> verifyingTestIds;     // tests that verifies->it
    int designedCount = 0;
    int verifiedCount = 0;
    int percentDesigned = 0;   // designedCount>0 ? 100 : 0
    int percentVerified = 0;   // verifiedCount>0 ? 100 : 0
    int percentSatisfied = 0;  // (designedCount>0 && verifiedCount>0) ? 100 : 0
};

struct CoverageReport {
    std::vector<CoverageRow> rows;
};

// A requirement coverage gap (listed for every requirement, flags indicate gaps).
struct CoverageGap {
    std::string requirementId;
    std::string requirementExternalId;
    bool hasNoDesign = false;
    bool hasNoTest = false;
};

// One cell of the trace matrix (a requirement x a column).
struct MatrixCell {
    std::string columnId;
    std::string columnType;  // "design" | "test_case"
    std::string relation;    // relation present, "" when none
};

// One requirement row of the trace matrix.
struct TraceMatrixRow {
    std::string requirementId;
    std::string requirementExternalId;
    std::vector<MatrixCell> cells;
};

// The trace matrix: ordered unique design+test columns and one row per req.
struct TraceMatrix {
    std::vector<std::string> columnIds;  // ordered unique design+test ids
    std::vector<TraceMatrixRow> rows;
};

// impactAnalysis result.
struct ImpactAnalysis {
    std::vector<GraphNode> affectedEntities;   // union(upstream, downstream) + self
    std::vector<GraphLink> affectedLinks;      // Active links incident to affected
    std::vector<GraphNode> downstreamTestCases;
    std::vector<std::string> blockedTransitions;  // e.g. "REQ-X -> Verified"
};

// ---------------------------------------------------------------------------
// GraphEngine: closure, impact, coverage, matrix, and general traversal.
// ---------------------------------------------------------------------------
class GraphEngine {
public:
    explicit GraphEngine(persistence::Database& db);

    // Bijective reverse-relation mapping; reverseRelation(reverseRelation(x))==x.
    // e.g. "verifies" <-> "is_verified_by".
    static std::string reverseRelation(const std::string& relation);

    // Closure. depth<=0 means unlimited. Result EXCLUDES the start node.
    // downstreamClosure = BFS following links source->target.
    // upstreamClosure   = BFS following links target->source.
    common::Result<std::vector<GraphNode>> downstreamClosure(
        EntityType type, const std::string& id, int depth = 0);
    common::Result<std::vector<GraphNode>> upstreamClosure(
        EntityType type, const std::string& id, int depth = 0);

    // General traversal along links of one relation ("" = all).
    common::Result<std::vector<GraphNode>> graphQuery(
        EntityType type, const std::string& id, const std::string& relation,
        Direction direction, int depth = 0);

    // affectedEntities = union(downstreamClosure, upstreamClosure) + the node.
    // blockedTransitions: for each affected requirement with no Active satisfies
    // link add "<extId> -> Implemented"; with no Active verifies link add
    // "<extId> -> Verified".
    common::Result<ImpactAnalysis> impactAnalysis(EntityType type,
                                                  const std::string& id);

    common::Result<CoverageReport> coverage();
    common::Result<std::vector<CoverageGap>> coverageGap();
    common::Result<TraceMatrix> traceMatrix();

private:
    struct NodeKey {
        EntityType type;
        std::string id;
        bool operator<(const NodeKey& o) const {
            if (type != o.type) return static_cast<int>(type) < static_cast<int>(o.type);
            return id < o.id;
        }
        bool operator==(const NodeKey& o) const { return type == o.type && id == o.id; }
    };

    GraphNode toNode(const Entity& e) const;
    common::Result<std::optional<Entity>> getEntity(EntityType type,
                                                    const std::string& id);
    // BFS traversal; returns GraphNodes excluding the start node.
    common::Result<std::vector<GraphNode>> traverse(EntityType type,
                                                    const std::string& id,
                                                    const std::string& relation,
                                                    Direction direction,
                                                    int depth);

    persistence::Database& db_;
    TraceLinkService service_;
};

}  // namespace lodestar::tracelink
