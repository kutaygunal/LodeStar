// core/test/wp6_ui_tests.cpp
// ---------------------------------------------------------------------------
// WP-6 Qt UI shell tests (test-first).
//
// Written by the scrum-master BEFORE the WP-6 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-6): Qt UI shell — left-nav project tree + right-side
// detail/properties panel, with LODESTAR_BUILD_UI=ON enabled against Qt 6.8.2.
//
// Qt is NOT installed in this build, so the Qt view classes (ProjectTreeView,
// DetailPanelView, MainWindow) cannot be compiled or instantiated here. This
// contract therefore covers the QT-INDEPENDENT wiring that the Qt views consume
// (the tree/detail data they render). This wiring is pure C++ and fully
// testable without a display. The Qt UI build itself is verified separately
// with LODESTAR_BUILD_UI=ON (see "UI build acceptance").
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-6 engineer must provide.
// ---------------------------------------------------------------------------
// (A) A Qt-independent wiring layer (core/tracelink/UiWiringService.h,
//     namespace lodestar::tracelink) extended with:
//
//   // One node of the left-nav project tree (nested hierarchy).
//   struct ProjectTreeNode {
//       std::string id;
//       std::string externalId;
//       std::string type;      // "requirement" | "design" | "test_case" | ...
//       std::string name;
//       std::vector<ProjectTreeNode> children;  // ordered by sortOrder then id
//   };
//
//   // The right-side detail/properties panel for one selected entity.
//   struct DetailPanelModel {
//       std::string id;
//       std::string externalId;
//       std::string type;
//       std::string name;
//       std::string status;
//       std::string owner;
//       std::string priority;
//       std::string verificationMethod;
//       std::string safetyLevel;
//       int version = 0;
//       std::vector<std::string> incomingLinks;  // "relation: sourceExternalId"
//       std::vector<std::string> outgoingLinks;  // "relation: targetExternalId"
//   };
//
//   class UiWiringService {
//       // ... existing refreshAll(), impact() ...
//
//       // Builds the full left-nav project tree: every root entity (no parent)
//       // with its ordered nested children (recursive). Roots ordered by
//       // sortOrder then id.
//       common::Result<std::vector<ProjectTreeNode>> projectTree();
//
//       // Builds the right-side detail/properties panel for one entity,
//       // including its Active incoming/outgoing links. Fails cleanly if the
//       // entity is missing.
//       common::Result<DetailPanelModel> detail(EntityType type,
//                                               const std::string& id);
//   };
//
// (B) The Qt views (ui/*.h): ProjectTreeView (QTreeView), DetailPanelView
//     (QWidget), and MainWindow assembles the left-nav tree + right-side detail
//     panel alongside the existing tabs, exposing refreshAll() and
//     showDetail(type, id). These are NOT exercised here (Qt absent); the
//     wiring they call is what this contract verifies.
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

bool hasString(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

// Recursively counts the nodes in a project tree.
int countNodes(const tl::ProjectTreeNode& n) {
    int c = 1;
    for (const auto& ch : n.children) c += countNodes(ch);
    return c;
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
// T1. projectTree() returns the full nested hierarchy
// ---------------------------------------------------------------------------
void testTreeHierarchy(Harness& h) {
    h.section("T1. projectTree() returns the full nested hierarchy");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    // Root requirement R; C derives R; D satisfies C; TC verifies C.
    auto R = svc.addEntity(makeReq("R"));
    auto C = svc.addEntity(makeReq("C"));
    auto D = svc.addEntity(makeDesign("D"));
    auto TC = svc.addEntity(makeTc("TC"));
    h.check(R.isOk() && C.isOk() && D.isOk() && TC.isOk(), "add entities ok");

    svc.addLink(makeLink(tl::EntityType::Requirement, C.value().id,
                         tl::EntityType::Requirement, R.value().id, "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, D.value().id,
                         tl::EntityType::Requirement, C.value().id, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, TC.value().id,
                         tl::EntityType::Requirement, C.value().id, "verifies"));

    auto tree = wiring.projectTree();
    h.check(tree.isOk(), "projectTree() ok");
    if (!tree.isOk()) {
        db.close();
        return;
    }
    const auto& roots = tree.value();
    h.check(roots.size() == 1, "one root");
    if (roots.size() != 1) {
        db.close();
        return;
    }
    const auto& root = roots[0];
    h.check(root.externalId == "R", "root is R");
    h.check(root.children.size() == 1, "R has one child");
    if (root.children.size() != 1) {
        db.close();
        return;
    }
    const auto& c = root.children[0];
    h.check(c.externalId == "C", "R's child is C");
    h.check(c.children.size() == 2, "C has two children");
    bool hasD = false, hasTC = false;
    for (const auto& ch : c.children) {
        if (ch.externalId == "D") hasD = true;
        if (ch.externalId == "TC") hasTC = true;
    }
    h.check(hasD && hasTC, "C's children contain D and TC");

    // Every entity appears exactly once (4 total).
    h.check(countNodes(root) == 4, "every entity appears exactly once");

    db.close();
    std::remove("lodestar_wp6_t1.db");
    std::remove("lodestar_wp6_t1.db-wal");
    std::remove("lodestar_wp6_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. projectTree() reflects parent/child relationships
// ---------------------------------------------------------------------------
void testParentChild(Harness& h) {
    h.section("T2. projectTree() reflects parent/child relationships");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto A = svc.addEntity(makeReq("A"));
    auto B = svc.addEntity(makeReq("B"));
    h.check(A.isOk() && B.isOk(), "add A and B ok");

    // B is a root initially.
    auto before = wiring.projectTree();
    h.check(before.isOk() && before.value().size() == 2, "two roots before setParent");

    auto sp = svc.setParent(tl::EntityType::Requirement, B.value().id, A.value().id);
    h.check(sp.isOk(), "setParent(B, A) ok");

    auto tree = wiring.projectTree();
    h.check(tree.isOk(), "projectTree() ok");
    if (!tree.isOk()) {
        db.close();
        return;
    }
    const auto& roots = tree.value();
    h.check(roots.size() == 1, "one root after setParent");
    if (roots.size() != 1) {
        db.close();
        return;
    }
    h.check(roots[0].externalId == "A", "root is A");
    h.check(roots[0].children.size() == 1, "A has one child");
    if (roots[0].children.size() == 1) {
        h.check(roots[0].children[0].externalId == "B", "A's child is B");
    }

    db.close();
    std::remove("lodestar_wp6_t2.db");
    std::remove("lodestar_wp6_t2.db-wal");
    std::remove("lodestar_wp6_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. projectTree() orders children by sortOrder
// ---------------------------------------------------------------------------
void testOrdering(Harness& h) {
    h.section("T3. projectTree() orders children by sortOrder");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto C = svc.addEntity(makeReq("C"));
    auto X = makeReq("X");
    X.sortOrder = 0;
    auto Y = makeReq("Y");
    Y.sortOrder = 1;
    auto x = svc.addEntity(X);
    auto y = svc.addEntity(Y);
    h.check(C.isOk() && x.isOk() && y.isOk(), "add C, X, Y ok");

    svc.setParent(tl::EntityType::Requirement, x.value().id, C.value().id);
    svc.setParent(tl::EntityType::Requirement, y.value().id, C.value().id);

    auto tree = wiring.projectTree();
    h.check(tree.isOk(), "projectTree() ok");
    if (!tree.isOk()) {
        db.close();
        return;
    }
    const auto& roots = tree.value();
    h.check(roots.size() == 1, "one root");
    if (roots.size() != 1) {
        db.close();
        return;
    }
    const auto& c = roots[0];
    h.check(c.externalId == "C", "root is C");
    h.check(c.children.size() == 2, "C has two children");
    if (c.children.size() == 2) {
        h.check(c.children[0].externalId == "X", "X appears before Y (sortOrder 0)");
        h.check(c.children[1].externalId == "Y", "Y appears after X (sortOrder 1)");
    }

    db.close();
    std::remove("lodestar_wp6_t3.db");
    std::remove("lodestar_wp6_t3.db-wal");
    std::remove("lodestar_wp6_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. detail() returns the selected entity's properties
// ---------------------------------------------------------------------------
void testDetailProperties(Harness& h) {
    h.section("T4. detail() returns the selected entity's properties");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto R = makeReq("R");
    R.owner = "alice";
    R.priority = "High";
    R.verificationMethod = "analysis";
    R.safetyLevel = "Level A";
    auto r = svc.addEntity(R);
    h.check(r.isOk(), "add R ok");

    // Bump the version via an update.
    auto e = r.value();
    e.name = "R updated";
    auto upd = svc.updateEntity(e);
    h.check(upd.isOk(), "update R ok");

    auto d = wiring.detail(tl::EntityType::Requirement, r.value().id);
    h.check(d.isOk(), "detail() ok");
    if (!d.isOk()) {
        db.close();
        return;
    }
    const auto& m = d.value();
    h.check(m.externalId == "R", "externalId is R");
    h.check(m.type == "requirement", "type is requirement");
    h.check(m.owner == "alice", "owner is alice");
    h.check(m.priority == "High", "priority is High");
    h.check(m.verificationMethod == "analysis", "verificationMethod is analysis");
    h.check(m.safetyLevel == "Level A", "safetyLevel is Level A");
    h.check(m.version == 2, "version is current (2 after one update)");

    db.close();
    std::remove("lodestar_wp6_t4.db");
    std::remove("lodestar_wp6_t4.db-wal");
    std::remove("lodestar_wp6_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. detail() returns incoming/outgoing links
// ---------------------------------------------------------------------------
void testDetailLinks(Harness& h) {
    h.section("T5. detail() returns incoming/outgoing links");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("R"));
    auto D = svc.addEntity(makeDesign("D"));
    auto S = svc.addEntity(makeReq("S"));
    h.check(R.isOk() && D.isOk() && S.isOk(), "add R, D, S ok");

    // D satisfies R (incoming to R); R derives S (outgoing from R).
    svc.addLink(makeLink(tl::EntityType::Design, D.value().id,
                         tl::EntityType::Requirement, R.value().id, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::Requirement, R.value().id,
                         tl::EntityType::Requirement, S.value().id, "derives"));

    auto d = wiring.detail(tl::EntityType::Requirement, R.value().id);
    h.check(d.isOk(), "detail() ok");
    if (!d.isOk()) {
        db.close();
        return;
    }
    const auto& m = d.value();
    h.check(hasString(m.incomingLinks, "satisfies: D"), "incoming contains 'satisfies: D'");
    h.check(hasString(m.outgoingLinks, "derives: S"), "outgoing contains 'derives: S'");

    db.close();
    std::remove("lodestar_wp6_t5.db");
    std::remove("lodestar_wp6_t5.db-wal");
    std::remove("lodestar_wp6_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. detail() on a missing entity fails cleanly
// ---------------------------------------------------------------------------
void testDetailMissing(Harness& h) {
    h.section("T6. detail() on a missing entity fails cleanly");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto d = wiring.detail(tl::EntityType::Requirement, "does-not-exist");
    h.check(d.failed(), "detail() on missing entity fails");

    db.close();
    std::remove("lodestar_wp6_t6.db");
    std::remove("lodestar_wp6_t6.db-wal");
    std::remove("lodestar_wp6_t6.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Acceptance: tree + detail reflect a live service change
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("T7. Acceptance: tree + detail reflect a live service change");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_t7.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("R"));
    auto C = svc.addEntity(makeReq("C"));
    h.check(R.isOk() && C.isOk(), "add R, C ok");
    svc.setParent(tl::EntityType::Requirement, C.value().id, R.value().id);

    // Snapshot before the change.
    auto before = wiring.projectTree();
    h.check(before.isOk() && before.value().size() == 1, "initial tree has one root");
    auto beforeDetail = wiring.detail(tl::EntityType::Requirement, C.value().id);
    h.check(beforeDetail.isOk(), "initial detail for C ok");

    // Add a new requirement N under R, then re-query.
    auto N = svc.addEntity(makeReq("N"));
    h.check(N.isOk(), "add N ok");
    svc.setParent(tl::EntityType::Requirement, N.value().id, R.value().id);

    auto after = wiring.projectTree();
    h.check(after.isOk(), "tree after change ok");
    if (after.isOk() && after.value().size() == 1) {
        const auto* n = findNode(after.value()[0], "N");
        h.check(n != nullptr, "N appears under R in the tree");
        if (n) {
            h.check(n->type == "requirement", "N node type is requirement");
        }
    }

    auto d = wiring.detail(tl::EntityType::Requirement, N.value().id);
    h.check(d.isOk(), "detail() for N ok");
    if (d.isOk()) {
        h.check(d.value().externalId == "N", "detail for N returns its properties");
    }

    // The tree is stable across repeated calls (idempotent).
    auto again = wiring.projectTree();
    h.check(again.isOk() && again.value().size() == after.value().size(),
            "tree is stable across repeated calls");

    db.close();
    std::remove("lodestar_wp6_t7.db");
    std::remove("lodestar_wp6_t7.db-wal");
    std::remove("lodestar_wp6_t7.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-6 Qt UI shell");
    std::printf("WP-6 QT UI SHELL TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testTreeHierarchy(h);
    testParentChild(h);
    testOrdering(h);
    testDetailProperties(h);
    testDetailLinks(h);
    testDetailMissing(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
