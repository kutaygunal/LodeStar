#pragma once
// core/tracelink/ViewModelFactory.h
// WP-7 Qt-independent view-model layer. Produces the data that feeds the four
// Qt views (trace matrix, coverage dashboard, impact view, graph view) WITHOUT
// any Qt dependency, so it is fully unit-testable in a non-Qt build.
//
// Built on top of TraceLinkService / GraphEngine / RulesEngine:
//   - MatrixViewModel            <- GraphEngine::traceMatrix
//   - CoverageDashboardModel     <- GraphEngine::coverage + RulesEngine run
//   - ImpactViewModel            <- GraphEngine::impactAnalysis
//   - GraphViewModel             <- closure / all active entities + links
//
// Contract written by the scrum-master in core/test/wp7_view_models_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Matrix view: rows = requirements, columns = design + test.
// ---------------------------------------------------------------------------
struct MatrixViewModel {
    struct Column {
        std::string id;    // entity id
        std::string type;  // "design" | "test_case"
        std::string name;  // external id (human readable)
    };
    struct Row {
        std::string requirementId;
        std::string requirementExternalId;
        std::vector<std::string> cellRelations;  // one per column; "" = none
    };

    std::vector<Column> columns;
    std::vector<Row> rows;

    int rowCount() const { return static_cast<int>(rows.size()); }
    int columnCount() const { return static_cast<int>(columns.size()); }

    // Relation at (row,col), or "" when out of range / no relation.
    std::string cell(int row, int col) const;

    common::Result<std::string> toCsv() const;   // matrix CSV export
    common::Result<std::string> toHtml() const;  // matrix HTML export
};

// ---------------------------------------------------------------------------
// Coverage / compliance dashboard.
// ---------------------------------------------------------------------------
struct CoverageDashboardModel {
    struct Item {
        std::string requirementId;
        std::string requirementExternalId;
        int percentDesigned = 0;
        int percentVerified = 0;
        int percentSatisfied = 0;  // mean of designed + verified
    };

    std::vector<Item> items;              // one per requirement
    double overallPercentDesigned = 0.0;  // mean of designed
    double overallPercentVerified = 0.0;  // mean of verified
    int violationCount = 0;               // active rule violations
    std::vector<Violation> violations;    // from the latest validation run
};

// ---------------------------------------------------------------------------
// Impact view: affected tree + blocked transitions.
// ---------------------------------------------------------------------------
struct ImpactViewModel {
    struct Node {
        std::string id;
        std::string externalId;
        std::string type;  // "requirement" | "design" | "test_case" | ...
        int depth = 0;     // 0 = the changed node
        bool affected = true;
    };

    std::vector<Node> affected;                  // affected tree (flat, with depth)
    std::vector<std::string> blockedTransitions; // e.g. "REQ-X -> Verified"
    std::vector<std::string> downstreamTests;    // external ids of affected tests
};

// ---------------------------------------------------------------------------
// Graph view: node-link diagram data.
// ---------------------------------------------------------------------------
struct GraphViewModel {
    struct Node {
        std::string id;
        std::string externalId;
        std::string type;
    };
    struct Edge {
        std::string sourceId;
        std::string targetId;
        std::string relation;
    };

    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

// ---------------------------------------------------------------------------
// ViewModelFactory: builds the four view models from a loaded graph.
// ---------------------------------------------------------------------------
class ViewModelFactory {
public:
    explicit ViewModelFactory(persistence::Database& db);

    common::Result<MatrixViewModel> matrix();
    common::Result<CoverageDashboardModel> coverageDashboard();
    common::Result<ImpactViewModel> impact(EntityType type, const std::string& id);
    common::Result<GraphViewModel> graph();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
