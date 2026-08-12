// core/test/wpG_tests.cpp
// ---------------------------------------------------------------------------
// WP-G unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-G engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-G): Real Qt UI — install Qt, enable LODESTAR_BUILD_UI=ON,
// and wire the four views (matrix / graph / impact / coverage) to the service.
//
// Qt is NOT installed in this build, so the Qt view classes (MatrixView,
// GraphView, ImpactView, CoverageDashboardView, MainWindow) cannot be compiled
// or instantiated here. This contract therefore covers the QT-INDEPENDENT
// view-model/service WIRING logic that the Qt views consume — the exact
// "refresh all" path MainWindow::refreshAll() calls and the per-entity impact
// path the ImpactView uses. This wiring is pure C++ and fully testable without
// a display.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-G engineer must provide.
// ---------------------------------------------------------------------------
// (A) A Qt-independent wiring layer (core/tracelink/UiWiringService.h,
//     namespace lodestar::tracelink) that assembles the four view models from
//     the service in one pass. It reuses the WP-7 ViewModelFactory types
//     (MatrixViewModel, CoverageDashboardModel, ImpactViewModel, GraphViewModel)
//     and is the single entry point the Qt MainWindow calls on refresh.
//
//   // The complete set of view models produced by one refresh.
//   struct UiSnapshot {
//       MatrixViewModel matrix;
//       CoverageDashboardModel coverage;
//       GraphViewModel graph;
//       std::vector<ImpactViewModel> impacts;  // one per requirement (impact tab)
//   };
//
//   class UiWiringService {
//   public:
//       explicit UiWiringService(persistence::Database& db);
//
//       // Builds all four view models from the current graph in one pass
//       // (the exact path the Qt MainWindow::refreshAll() calls). The models
//       // are mutually consistent: matrix rows == coverage items == number of
//       // requirements; graph nodes == all active entities.
//       common::Result<UiSnapshot> refreshAll();
//
//       // Builds the impact view model for one entity (the path the ImpactView
//       // uses when a user selects a node).
//       common::Result<ImpactViewModel> impact(EntityType type,
//                                              const std::string& id);
//   };
//
// (B) The Qt views (ui/*.h) each expose `void setModel(const <Model>&)` and
//     MainWindow exposes `void refreshAll()`. These are NOT exercised here
//     (Qt absent); the wiring they call is what this contract verifies.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UiWiringService.h"
#include "core/tracelink/ViewModelFactory.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    auto mig = runner.run(g_migrationsDir);
    return mig.isOk();
}

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// ---------------------------------------------------------------------------
// Factories (same contract as WP-1 / WP-7).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Approved") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Body of " + extId;
    e.status = status;
    e.owner = "engineer";
    e.verificationMethod = "test";
    e.safetyLevel = "Level A";
    return e;
}

tl::Entity makeTc(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = extId;
    e.text = "Test body of " + extId;
    return e;
}

tl::Entity makeDesign(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = extId;
    e.text = "Design body of " + extId;
    return e;
}

tl::Link makeLink(tl::EntityType srcT, const std::string& srcId, tl::EntityType tgtT,
                  const std::string& tgtId, const std::string& rel) {
    tl::Link l;
    l.sourceType = srcT;
    l.sourceId = srcId;
    l.targetType = tgtT;
    l.targetId = tgtId;
    l.relation = rel;
    return l;
}

// ---------------------------------------------------------------------------
// The WP-G fixture graph (same shape as WP-7):
//   REQ-SYS (Approved)                          <-- root
//   |-- REQ-DER (Draft)  derives REQ-SYS        fully covered (DES-1 + TC-1)
//   |-- REQ-UNV (Approved) derives REQ-SYS      designed (DES-U) but unverified
//   DES-1 satisfies REQ-DER
//   TC-1  verifies   REQ-DER
//   DES-U satisfies REQ-UNV
// ---------------------------------------------------------------------------
struct Graph {
    std::string sysReqId;
    std::string derReqId;
    std::string unvReqId;
    std::string desId;
    std::string tcId;
    std::string desUId;
};

Graph buildGraph(tl::TraceLinkService& svc) {
    Graph g;
    auto sys = svc.addEntity(makeReq("REQ-SYS", "Approved"));
    auto der = svc.addEntity(makeReq("REQ-DER", "Draft"));
    auto unv = svc.addEntity(makeReq("REQ-UNV", "Approved"));
    auto des = svc.addEntity(makeDesign("DES-1"));
    auto tc = svc.addEntity(makeTc("TC-1"));
    auto desU = svc.addEntity(makeDesign("DES-U"));
    g.sysReqId = sys.value().id;
    g.derReqId = der.value().id;
    g.unvReqId = unv.value().id;
    g.desId = des.value().id;
    g.tcId = tc.value().id;
    g.desUId = desU.value().id;

    svc.addLink(makeLink(tl::EntityType::Requirement, g.derReqId,
                         tl::EntityType::Requirement, g.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Requirement, g.unvReqId,
                         tl::EntityType::Requirement, g.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, g.desId,
                         tl::EntityType::Requirement, g.derReqId, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, g.tcId,
                         tl::EntityType::Requirement, g.derReqId, "verifies"));
    svc.addLink(makeLink(tl::EntityType::Design, g.desUId,
                         tl::EntityType::Requirement, g.unvReqId, "satisfies"));
    return g;
}

bool hasString(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

bool impactHasExtId(const tl::ImpactViewModel& m, const std::string& extId) {
    for (const auto& n : m.affected) {
        if (n.externalId == extId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 1. refreshAll() wires all four view models from the service
// ---------------------------------------------------------------------------
void testRefreshAll(Harness& h) {
    h.section("1. refreshAll() builds all four view models");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpG_refresh.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    auto snap = wiring.refreshAll();
    h.check(snap.isOk(), "refreshAll() ok");
    if (!snap.isOk()) {
        db.close();
        return;
    }
    const tl::UiSnapshot& s = snap.value();

    // Matrix: 3 requirement rows, 3 design/test columns.
    h.check(s.matrix.rowCount() == 3, "matrix has 3 requirement rows");
    h.check(s.matrix.columnCount() == 3, "matrix has 3 design/test columns");

    // Coverage: one item per requirement.
    h.check(static_cast<int>(s.coverage.items.size()) == 3,
            "coverage dashboard has 3 items");

    // Graph: 6 nodes, 5 edges.
    h.check(s.graph.nodes.size() == 6, "graph has 6 nodes");
    h.check(s.graph.edges.size() == 5, "graph has 5 edges");

    // Impact tab: one impact model per requirement.
    h.check(s.impacts.size() == 3, "impact tab has one model per requirement");

    db.close();
    std::remove("lodestar_wpG_refresh.db");
    std::remove("lodestar_wpG_refresh.db-wal");
    std::remove("lodestar_wpG_refresh.db-shm");
}

// ---------------------------------------------------------------------------
// 2. Cross-model consistency (the wiring invariant)
// ---------------------------------------------------------------------------
void testConsistency(Harness& h) {
    h.section("2. Cross-model consistency of the refresh snapshot");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpG_consist.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    auto snap = wiring.refreshAll();
    h.check(snap.isOk(), "refreshAll() ok");
    if (!snap.isOk()) {
        db.close();
        return;
    }
    const tl::UiSnapshot& s = snap.value();

    // matrix rows == coverage items == number of requirements.
    h.check(s.matrix.rowCount() == static_cast<int>(s.coverage.items.size()),
            "matrix rows == coverage items");
    h.check(s.matrix.rowCount() == 3, "matrix rows == 3 requirements");

    // graph nodes == all active entities (3 req + 2 design + 1 test = 6).
    h.check(s.graph.nodes.size() == 6, "graph nodes == all active entities");

    // Every matrix row has a matching coverage item (same requirement id).
    bool allMatched = true;
    for (const auto& row : s.matrix.rows) {
        bool found = false;
        for (const auto& item : s.coverage.items) {
            if (item.requirementId == row.requirementId) found = true;
        }
        if (!found) allMatched = false;
    }
    h.check(allMatched, "every matrix row has a matching coverage item");

    // refreshAll() is stable across calls (idempotent wiring).
    auto snap2 = wiring.refreshAll();
    h.check(snap2.isOk() && snap2.value().matrix.rowCount() == s.matrix.rowCount() &&
                snap2.value().graph.nodes.size() == s.graph.nodes.size(),
            "refreshAll() is stable across calls");

    db.close();
    std::remove("lodestar_wpG_consist.db");
    std::remove("lodestar_wpG_consist.db-wal");
    std::remove("lodestar_wpG_consist.db-shm");
}

// ---------------------------------------------------------------------------
// 3. Per-entity impact wiring (the ImpactView path)
// ---------------------------------------------------------------------------
void testImpactWiring(Harness& h) {
    h.section("3. Per-entity impact wiring (ImpactView path)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpG_impact.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    // Impact on the root requirement.
    auto imp = wiring.impact(tl::EntityType::Requirement, g.sysReqId);
    h.check(imp.isOk(), "wiring.impact() ok");
    if (imp.isOk()) {
        const tl::ImpactViewModel& im = imp.value();
        h.check(impactHasExtId(im, "REQ-SYS") && impactHasExtId(im, "REQ-DER") &&
                    impactHasExtId(im, "REQ-UNV") && impactHasExtId(im, "DES-1") &&
                    impactHasExtId(im, "TC-1") && impactHasExtId(im, "DES-U"),
                "impact tree includes the whole branch under REQ-SYS");
        h.check(hasString(im.blockedTransitions, "REQ-UNV -> Verified"),
                "blocked transition: REQ-UNV -> Verified (no test)");
        h.check(hasString(im.downstreamTests, "TC-1"),
                "downstream tests include TC-1");
    }

    // The wiring impact path agrees with the ViewModelFactory impact path.
    tl::ViewModelFactory vf(db);
    auto factoryImp = vf.impact(tl::EntityType::Requirement, g.sysReqId);
    h.check(factoryImp.isOk() && imp.isOk() &&
                factoryImp.value().affected.size() == imp.value().affected.size(),
            "wiring impact matches the ViewModelFactory impact");

    // Impact on a nonexistent entity fails cleanly.
    auto missing = wiring.impact(tl::EntityType::Requirement, "does-not-exist");
    h.check(missing.failed(), "impact on missing entity fails");

    db.close();
    std::remove("lodestar_wpG_impact.db");
    std::remove("lodestar_wpG_impact.db-wal");
    std::remove("lodestar_wpG_impact.db-shm");
}

// ---------------------------------------------------------------------------
// 4. WP-G acceptance: refresh reflects a live change to the service
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("4. WP-G acceptance: refresh reflects a live service change");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpG_accept.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    auto before = wiring.refreshAll();
    h.check(before.isOk() && before.value().matrix.rowCount() == 3,
            "initial refresh shows 3 requirements");

    // Add a new requirement + a verifying test case, then refresh.
    auto newReq = svc.addEntity(makeReq("REQ-NEW", "Approved"));
    auto newTc = svc.addEntity(makeTc("TC-NEW"));
    h.check(newReq.isOk() && newTc.isOk(), "add new requirement + test ok");
    svc.addLink(makeLink(tl::EntityType::TestCase, newTc.value().id,
                         tl::EntityType::Requirement, newReq.value().id, "verifies"));

    auto after = wiring.refreshAll();
    h.check(after.isOk(), "refresh after change ok");
    h.check(after.value().matrix.rowCount() == 4,
            "matrix now shows 4 requirements");
    h.check(after.value().graph.nodes.size() == 8,
            "graph now has 8 nodes (6 + 2 new)");
    h.check(after.value().coverage.items.size() == 4,
            "coverage dashboard now has 4 items");

    db.close();
    std::remove("lodestar_wpG_accept.db");
    std::remove("lodestar_wpG_accept.db-wal");
    std::remove("lodestar_wpG_accept.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-G Qt UI wiring");
    std::printf("WP-G QT UI WIRING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testRefreshAll(h);
    testConsistency(h);
    testImpactWiring(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
