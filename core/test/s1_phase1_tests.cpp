// core/test/s1_phase1_tests.cpp
// ---------------------------------------------------------------------------
// S1 Phase 1 desktop-app wiring tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 1 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (PLAN.md, Phase 1): a runnable desktop Qt app that opens and shows
// TraceLink data. The Qt UI shell already exists in ui/ (MainWindow + WP-7
// views) but is gated behind LODESTAR_BUILD_UI=OFF. This phase enables the
// flag, wires MainWindow to the service API, and proves the app runs.
//
// Qt is NOT installed in this build, so the Qt view classes (MainWindow,
// MatrixView, ...) cannot be compiled or instantiated here. This contract
// therefore covers the QT-INDEPENDENT wiring that the Qt views consume (the
// exact data refreshAll()/projectTree()/detail() produce). This wiring is pure
// C++ and fully testable without a display. The Qt app launch itself is
// verified separately with LODESTAR_BUILD_UI=ON (see "App launch acceptance").
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the Phase 1 engineer must provide.
// ---------------------------------------------------------------------------
// (A) A runnable Qt app target `lodestar_app` (ui/app/main.cpp) that opens a
//     DB, runs migrations, seeds a small TraceLink graph, constructs
//     lodestar::ui::MainWindow, calls refreshAll() and show(), and exits
//     cleanly on window close.
//
// (B) The Qt-independent wiring layer (core/tracelink/UiWiringService.h,
//     namespace lodestar::tracelink) already provides:
//       refreshAll() -> UiSnapshot { matrix, coverage, graph, impacts }
//       projectTree() -> vector<ProjectTreeNode>
//       detail(type, id) -> DetailPanelModel
//     This contract verifies those produce the stated results on a fresh DB
//     with one requirement, one test case, and one `verifies` link.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UiWiringService.h"

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
// Factories (same contract as WP-1 / WP-7 / WP-G).
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

// Recursively searches for a node by external id.
const tl::ProjectTreeNode* findNode(const tl::ProjectTreeNode& n,
                                    const std::string& extId) {
    if (n.externalId == extId) return &n;
    for (const auto& ch : n.children) {
        auto* found = findNode(ch, extId);
        if (found) return found;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// T3. refreshAll() produces a 1-row matrix + graph nodes
// ---------------------------------------------------------------------------
void testRefreshAll(Harness& h) {
    h.section("T3. refreshAll() produces a 1-row matrix + graph nodes");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p1_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    // One requirement + one test case + one `verifies` link.
    auto R = svc.addEntity(makeReq("REQ-001"));
    auto TC = svc.addEntity(makeTc("TC-001"));
    h.check(R.isOk() && TC.isOk(), "add requirement + test case ok");
    if (!R.isOk() || !TC.isOk()) {
        db.close();
        return;
    }
    auto lk = svc.addLink(makeLink(tl::EntityType::TestCase, TC.value().id,
                                   tl::EntityType::Requirement, R.value().id,
                                   "verifies"));
    h.check(lk.isOk(), "add verifies link ok");

    auto snap = wiring.refreshAll();
    h.check(snap.isOk(), "refreshAll() ok");
    if (!snap.isOk()) {
        db.close();
        return;
    }
    const auto& s = snap.value();
    h.check(s.matrix.rows.size() == 1, "matrix has exactly 1 row");

    // Graph contains the requirement node and the test case node.
    bool hasReq = false, hasTc = false;
    for (const auto& n : s.graph.nodes) {
        if (n.externalId == "REQ-001") hasReq = true;
        if (n.externalId == "TC-001") hasTc = true;
    }
    h.check(hasReq, "graph contains the requirement node");
    h.check(hasTc, "graph contains the test case node");

    db.close();
    std::remove("lodestar_s1p1_t3.db");
    std::remove("lodestar_s1p1_t3.db-wal");
    std::remove("lodestar_s1p1_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. projectTree() contains the requirement
// ---------------------------------------------------------------------------
void testProjectTree(Harness& h) {
    h.section("T4. projectTree() contains the requirement");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p1_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("REQ-001"));
    auto TC = svc.addEntity(makeTc("TC-001"));
    h.check(R.isOk() && TC.isOk(), "add requirement + test case ok");
    if (!R.isOk() || !TC.isOk()) {
        db.close();
        return;
    }
    svc.addLink(makeLink(tl::EntityType::TestCase, TC.value().id,
                         tl::EntityType::Requirement, R.value().id, "verifies"));

    auto tree = wiring.projectTree();
    h.check(tree.isOk(), "projectTree() ok");
    if (!tree.isOk()) {
        db.close();
        return;
    }
    const auto& roots = tree.value();
    h.check(!roots.empty(), "tree has at least one root");
    bool found = false;
    for (const auto& root : roots) {
        if (findNode(root, "REQ-001") != nullptr) {
            found = true;
            break;
        }
    }
    h.check(found, "tree contains a node for REQ-001");

    db.close();
    std::remove("lodestar_s1p1_t4.db");
    std::remove("lodestar_s1p1_t4.db-wal");
    std::remove("lodestar_s1p1_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. detail() returns the requirement
// ---------------------------------------------------------------------------
void testDetail(Harness& h) {
    h.section("T5. detail() returns the requirement");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p1_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("REQ-001"));
    auto TC = svc.addEntity(makeTc("TC-001"));
    h.check(R.isOk() && TC.isOk(), "add requirement + test case ok");
    if (!R.isOk() || !TC.isOk()) {
        db.close();
        return;
    }
    svc.addLink(makeLink(tl::EntityType::TestCase, TC.value().id,
                         tl::EntityType::Requirement, R.value().id, "verifies"));

    auto d = wiring.detail(tl::EntityType::Requirement, R.value().id);
    h.check(d.isOk(), "detail(requirement, reqId) ok");
    if (!d.isOk()) {
        db.close();
        return;
    }
    h.check(d.value().externalId == "REQ-001", "detail returns externalId REQ-001");

    db.close();
    std::remove("lodestar_s1p1_t5.db");
    std::remove("lodestar_s1p1_t5.db-wal");
    std::remove("lodestar_s1p1_t5.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S1 Phase 1 desktop app wiring");
    std::printf("S1 PHASE 1 DESKTOP APP WIRING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testRefreshAll(h);
    testProjectTree(h);
    testDetail(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
