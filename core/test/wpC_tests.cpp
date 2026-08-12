// core/test/wpC_tests.cpp
// ---------------------------------------------------------------------------
// WP-C unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-C engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-C / A2): requirement hierarchy tree — parent/child
// navigation, subtree operations, and reorder.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-C engineer must provide.
// ---------------------------------------------------------------------------
// TraceLinkService additions (core/tracelink/TraceLinkService.h):
//
//   // A nested hierarchy node (entity + its ordered children).
//   struct HierarchyNode {
//       Entity entity;
//       std::vector<HierarchyNode> children;
//   };
//
//   // Sets the parent of `id` to `parentId`. Both must exist and be the same
//   // entity type. Rejects a cycle (parentId must not be `id` or a descendant
//   // of `id`). Pass an empty parentId to detach (make it a root).
//   common::Result<void> setParent(EntityType type, const std::string& id,
//                                  const std::string& parentId);
//
//   // Direct children of parentId ("" = roots), ordered by sortOrder then id.
//   common::Result<std::vector<Entity>> children(EntityType type,
//                                                const std::string& parentId);
//
//   // All descendants of `id` (recursive, excludes `id` itself).
//   common::Result<std::vector<Entity>> subtree(EntityType type,
//                                               const std::string& id);
//
//   // The ancestor chain of `id` from its immediate parent up to the root.
//   common::Result<std::vector<Entity>> ancestors(EntityType type,
//                                                 const std::string& id);
//
//   // Reorders the direct children of parentId to the given id order,
//   // assigning sortOrder 0..N-1 in that order.
//   common::Result<void> reorder(EntityType type, const std::string& parentId,
//                                const std::vector<std::string>& orderedIds);
//
//   // All entities of `type` with no parent (roots), ordered by sortOrder.
//   common::Result<std::vector<Entity>> rootNodes(EntityType type);
//
//   // Builds the full nested tree rooted at `id` (recursive).
//   common::Result<HierarchyNode> buildTree(EntityType type,
//                                           const std::string& rootId);
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
// Helpers.
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Requirement " + extId;
    e.status = "Draft";
    return e;
}

bool containsId(const std::vector<tl::Entity>& v, const std::string& id) {
    for (const auto& e : v) {
        if (e.id == id) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// A2. Parent/child navigation + subtree + reorder
// ---------------------------------------------------------------------------
void testHierarchy(Harness& h) {
    h.section("A2. Hierarchy: parent/child, subtree, ancestors, reorder");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpC_hier.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Build a 3-level tree:
    //   root (REQ-ROOT)
    //     child1 (REQ-C1)
    //       grandchild (REQ-G1)
    //     child2 (REQ-C2)
    auto root = svc.addEntity(makeReq("REQ-ROOT"));
    auto c1 = svc.addEntity(makeReq("REQ-C1"));
    auto c2 = svc.addEntity(makeReq("REQ-C2"));
    auto g1 = svc.addEntity(makeReq("REQ-G1"));
    h.check(root.isOk() && c1.isOk() && c2.isOk() && g1.isOk(),
            "seed 4 requirements ok");
    const std::string rootId = root.value().id;
    const std::string c1Id = c1.value().id;
    const std::string c2Id = c2.value().id;
    const std::string g1Id = g1.value().id;

    // Initially all are roots.
    auto roots0 = svc.rootNodes(tl::EntityType::Requirement);
    h.check(roots0.isOk() && roots0.value().size() == 4,
            "all 4 requirements are roots before parenting");

    // setParent: c1, c2 under root; g1 under c1.
    h.check(svc.setParent(tl::EntityType::Requirement, c1Id, rootId).isOk(),
            "setParent c1 under root ok");
    h.check(svc.setParent(tl::EntityType::Requirement, c2Id, rootId).isOk(),
            "setParent c2 under root ok");
    h.check(svc.setParent(tl::EntityType::Requirement, g1Id, c1Id).isOk(),
            "setParent g1 under c1 ok");

    // children of root -> c1, c2 (ordered by sortOrder then id).
    auto kids = svc.children(tl::EntityType::Requirement, rootId);
    h.check(kids.isOk() && kids.value().size() == 2,
            "root has exactly 2 direct children");
    h.check(containsId(kids.value(), c1Id) && containsId(kids.value(), c2Id),
            "root children are c1 and c2");

    // children of c1 -> g1.
    auto c1kids = svc.children(tl::EntityType::Requirement, c1Id);
    h.check(c1kids.isOk() && c1kids.value().size() == 1 &&
                c1kids.value().front().id == g1Id,
            "c1 has exactly 1 child (g1)");

    // children of a leaf -> empty.
    auto leafKids = svc.children(tl::EntityType::Requirement, g1Id);
    h.check(leafKids.isOk() && leafKids.value().empty(),
            "leaf has no children");

    // subtree of root -> c1, c2, g1 (all descendants, excludes root).
    auto sub = svc.subtree(tl::EntityType::Requirement, rootId);
    h.check(sub.isOk() && sub.value().size() == 3,
            "subtree of root has 3 descendants");
    h.check(containsId(sub.value(), c1Id) && containsId(sub.value(), c2Id) &&
                containsId(sub.value(), g1Id),
            "subtree contains c1, c2, g1");

    // subtree of c1 -> g1 only.
    auto subC1 = svc.subtree(tl::EntityType::Requirement, c1Id);
    h.check(subC1.isOk() && subC1.value().size() == 1 &&
                subC1.value().front().id == g1Id,
            "subtree of c1 is g1 only");

    // ancestors of g1 -> c1, root (immediate parent first).
    auto anc = svc.ancestors(tl::EntityType::Requirement, g1Id);
    h.check(anc.isOk() && anc.value().size() == 2,
            "ancestors of g1 has 2 entries");
    if (anc.isOk() && anc.value().size() == 2) {
        h.check(anc.value()[0].id == c1Id && anc.value()[1].id == rootId,
                "ancestors ordered immediate-parent-first");
    }

    // rootNodes now -> only root.
    auto roots = svc.rootNodes(tl::EntityType::Requirement);
    h.check(roots.isOk() && roots.value().size() == 1 &&
                roots.value().front().id == rootId,
            "only root remains a root node");

    // Cycle rejection: cannot set root's parent to its own descendant g1.
    auto cycle = svc.setParent(tl::EntityType::Requirement, rootId, g1Id);
    h.check(cycle.failed(), "setParent to own descendant rejected (cycle)");

    // Cannot set parent to itself.
    auto self = svc.setParent(tl::EntityType::Requirement, c1Id, c1Id);
    h.check(self.failed(), "setParent to itself rejected");

    // Cannot set parent to a nonexistent node.
    auto ghost = svc.setParent(tl::EntityType::Requirement, c1Id, "ghost");
    h.check(ghost.failed(), "setParent to nonexistent parent rejected");

    // Reorder: make c2 sort before c1 under root.
    std::vector<std::string> order{c2Id, c1Id};
    h.check(svc.reorder(tl::EntityType::Requirement, rootId, order).isOk(),
            "reorder root children ok");
    auto reordered = svc.children(tl::EntityType::Requirement, rootId);
    h.check(reordered.isOk() && reordered.value().size() == 2 &&
                reordered.value()[0].id == c2Id &&
                reordered.value()[1].id == c1Id,
            "children reflect the new sort order");

    // Detach: setParent(c1, "") makes c1 a root again.
    h.check(svc.setParent(tl::EntityType::Requirement, c1Id, "").isOk(),
            "detach c1 (setParent to empty) ok");
    auto rootsAfter = svc.rootNodes(tl::EntityType::Requirement);
    h.check(rootsAfter.isOk() && rootsAfter.value().size() == 2,
            "c1 becomes a root after detach");

    db.close();
    std::remove("lodestar_wpC_hier.db");
    std::remove("lodestar_wpC_hier.db-wal");
    std::remove("lodestar_wpC_hier.db-shm");
}

// ---------------------------------------------------------------------------
// A2. buildTree nested structure
// ---------------------------------------------------------------------------
void testBuildTree(Harness& h) {
    h.section("A2. buildTree nested structure");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpC_tree.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    auto root = svc.addEntity(makeReq("REQ-ROOT"));
    auto c1 = svc.addEntity(makeReq("REQ-C1"));
    auto c2 = svc.addEntity(makeReq("REQ-C2"));
    auto g1 = svc.addEntity(makeReq("REQ-G1"));
    h.check(root.isOk() && c1.isOk() && c2.isOk() && g1.isOk(), "seed ok");
    const std::string rootId = root.value().id;
    const std::string c1Id = c1.value().id;
    const std::string c2Id = c2.value().id;
    const std::string g1Id = g1.value().id;

    h.check(svc.setParent(tl::EntityType::Requirement, c1Id, rootId).isOk(), "c1->root");
    h.check(svc.setParent(tl::EntityType::Requirement, c2Id, rootId).isOk(), "c2->root");
    h.check(svc.setParent(tl::EntityType::Requirement, g1Id, c1Id).isOk(), "g1->c1");

    auto tree = svc.buildTree(tl::EntityType::Requirement, rootId);
    h.check(tree.isOk(), "buildTree ok");
    if (tree.isOk()) {
        h.check(tree.value().entity.id == rootId, "tree root is the requested node");
        h.check(tree.value().children.size() == 2, "root has 2 children in tree");
        // Find c1 among root's children and check it has g1 as a child.
        bool c1HasG1 = false;
        for (const auto& child : tree.value().children) {
            if (child.entity.id == c1Id) {
                c1HasG1 = child.children.size() == 1 &&
                          child.children[0].entity.id == g1Id;
            }
        }
        h.check(c1HasG1, "c1 node in tree has g1 as its child");
    }

    db.close();
    std::remove("lodestar_wpC_tree.db");
    std::remove("lodestar_wpC_tree.db-wal");
    std::remove("lodestar_wpC_tree.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-C hierarchy tree");
    std::printf("WP-C HIERARCHY TREE TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testHierarchy(h);
    testBuildTree(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
