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
#include "core/tracelink/BaselineService.h"
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

// One live coverage dashboard row (red/green gap).
struct LiveCoverageRow {
    std::string requirementId;
    std::string requirementExternalId;
    bool designed = false;   // has >=1 Active satisfies link
    bool verified = false;   // has >=1 Active verifies link AND a passing run
    bool executed = false;   // has at least one recorded test run
    bool gapNoDesign = false;  // red: no design
    bool gapNoTest = false;    // red: no passing test
};

// Chart data for the dashboard.
struct CoverageCharts {
    struct Slice { std::string label; int count = 0; };
    std::vector<Slice> byStatus;    // Draft / Approved / ... counts
    std::vector<Slice> byPriority;  // High / Medium / Low / ... counts
    std::vector<Slice> byCoverage;  // Full / Partial / None counts
};

// Filtering / view configuration for the interactive traceability matrix.
// WP-8: search text, status filter, and toggled-off relations.
struct MatrixViewConfig {
    std::string search;                        // substring on name/externalId
    std::string statusFilter;                  // "" = all, else a status
    std::vector<std::string> hiddenRelations;   // relations to hide (toggle off)
};

// A saved matrix view (persisted). WP-8.
struct SavedMatrixView {
    std::string id;
    std::string name;
    MatrixViewConfig config;
};

// One section of a document (a container of ordered requirements).
struct DocumentSection {
    std::string id;
    std::string title;
    std::vector<Entity> requirements;  // ordered by sortOrder then id
};

// A document: a root container with ordered sections.
struct DocumentModel {
    std::string id;
    std::string title;
    std::vector<DocumentSection> sections;
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

// One row of the visual compare view between two baselines.
struct VisualDiffRow {
    std::string entityId;
    std::string entityExternalId;
    std::string kind;   // "added" | "removed" | "modified"
    std::vector<FieldChange> fieldChanges;  // non-empty for "modified"
};

// Result of a per-item rollback.
struct RollbackResult {
    std::string entityId;
    std::string entityExternalId;
    bool restored = false;
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

    // Visual diff of baseline a (older) against b (newer): one row per changed
    // entity/link, with field changes for modified items.
    common::Result<std::vector<VisualDiffRow>> visualDiff(
        const std::string& aId, const std::string& bId);

    // Rolls a single entity back to its state in `baselineId`. Fails cleanly
    // if the entity is missing from the baseline.
    common::Result<RollbackResult> rollbackEntity(
        EntityType type, const std::string& id, const std::string& baselineId);

    // Resolves the entity type of an entity by id (UI helper so the diff view
    // can call rollbackEntity for a row). Fails cleanly if no entity with that
    // id exists.
    common::Result<EntityType> entityTypeOf(const std::string& id);

    // Live coverage: a requirement is `verified` only when it has an Active
    // verifies link AND a passing executed run (WP-5 CoverageService).
    common::Result<std::vector<LiveCoverageRow>> liveCoverage();

    // Chart data: status / priority / coverage distributions across all
    // requirements. byCoverage: Full = designed+verified, Partial = one of
    // the two, None = neither.
    common::Result<CoverageCharts> coverageCharts();

    // Builds the matrix honoring the config: rows filtered by search/status,
    // and any cell whose relation is in hiddenRelations is shown as "".
    common::Result<MatrixViewModel> matrixFiltered(const MatrixViewConfig& cfg);

    // Persists a named matrix view.
    common::Result<void> saveMatrixView(const std::string& name,
                                        const MatrixViewConfig& cfg);

    // All saved matrix views, ordered by name.
    common::Result<std::vector<SavedMatrixView>> listMatrixViews();

    // Applies a saved view and returns the filtered matrix.
    common::Result<MatrixViewModel> applyMatrixView(const std::string& viewId);

    // Builds a document model from the hierarchy rooted at `docId` (a
    // requirement-type root whose children are sections, whose children are
    // requirements). Fails cleanly if the document root is missing.
    common::Result<DocumentModel> document(const std::string& docId);

    // Creates a requirement and attaches it to a section with atomic
    // traceability (the requirement is created AND linked to the section in one
    // operation). Returns the created requirement.
    common::Result<Entity> addRequirementToDocument(
        const std::string& docId, const std::string& sectionId, const Entity& req);

    // Reorders the requirements within a section to the given id order.
    common::Result<void> reorderRequirements(
        const std::string& docId, const std::string& sectionId,
        const std::vector<std::string>& orderedIds);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
