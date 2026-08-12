#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/UiWiringService.cpp
// WP-G Qt-independent wiring layer implementation. Delegates to the WP-7
// ViewModelFactory to build the four view models in one pass, guaranteeing the
// cross-model consistency invariant (matrix rows == coverage items == number of
// requirements; graph nodes == all active entities).

#include "core/tracelink/UiWiringService.h"

#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/ViewModelFactory.h"

namespace lodestar::tracelink {

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

}  // namespace lodestar::tracelink
