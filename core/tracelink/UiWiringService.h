#pragma once
// core/tracelink/UiWiringService.h
// WP-G Qt-independent wiring layer. Assembles the four view models from the
// service in one pass and is the single entry point the Qt MainWindow calls on
// refresh. Reuses the WP-7 ViewModelFactory types (MatrixViewModel,
// CoverageDashboardModel, ImpactViewModel, GraphViewModel) so the Qt views
// consume exactly the same data whether they are driven by the factory
// directly or through this wiring service.
//
// Contract written by the scrum-master in core/test/wpG_tests.cpp.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::tracelink {

// The complete set of view models produced by one refresh.
struct UiSnapshot {
    MatrixViewModel matrix;
    CoverageDashboardModel coverage;
    GraphViewModel graph;
    std::vector<ImpactViewModel> impacts;  // one per requirement (impact tab)
};

// One node of the left-nav project tree (nested hierarchy).
struct ProjectTreeNode {
    std::string id;
    std::string externalId;
    std::string type;      // "requirement" | "design" | "test_case" | ...
    std::string name;
    std::vector<ProjectTreeNode> children;  // ordered by sortOrder then id
};

// The right-side detail/properties panel for one selected entity.
struct DetailPanelModel {
    std::string id;
    std::string externalId;
    std::string type;
    std::string name;
    std::string status;
    std::string owner;
    std::string priority;
    std::string verificationMethod;
    std::string safetyLevel;
    int version = 0;
    std::vector<std::string> incomingLinks;  // "relation: sourceExternalId"
    std::vector<std::string> outgoingLinks;  // "relation: targetExternalId"
};

// Assembles all four view models from the current graph in one pass. The
// models are mutually consistent: matrix rows == coverage items == number of
// requirements; graph nodes == all active entities.
class UiWiringService {
public:
    explicit UiWiringService(persistence::Database& db);

    // Builds all four view models from the current graph in one pass (the
    // exact path the Qt MainWindow::refreshAll() calls).
    common::Result<UiSnapshot> refreshAll();

    // Builds the impact view model for one entity (the path the ImpactView
    // uses when a user selects a node).
    common::Result<ImpactViewModel> impact(EntityType type,
                                            const std::string& id);

    // Builds the full left-nav project tree: every root entity (no parent)
    // with its ordered nested children (recursive). Roots ordered by sortOrder
    // then id. A node's children come from the parent/child hierarchy
    // (setParent) AND from Active links that point at the node (source ->
    // target), so cross-type nesting (design/test under a requirement) is
    // represented. Every active entity appears exactly once.
    common::Result<std::vector<ProjectTreeNode>> projectTree();

    // Builds the right-side detail/properties panel for one entity, including
    // its Active incoming/outgoing links. Fails cleanly if the entity is
    // missing.
    common::Result<DetailPanelModel> detail(EntityType type,
                                            const std::string& id);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
