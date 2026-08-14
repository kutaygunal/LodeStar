// core/test/t1_collab_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TraceLink 3.2: real-time multi-user collaboration tests.
//
// Test contract: docs/gap-fill-plan.md (Module 3.2).
//   (A) Migration 032 creates collab_operation_log / collab_vector.
//   (B) core/tracelink/CollaborationService.h (+ .cpp): an append-only
//       operation log with per-entity version vectors; optimistic concurrency
//       with conflict detection; merge + conflict resolution.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/CollaborationService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

bool tableExists(p::Database& db, const std::string& table) {
    return db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
        table + "';") == "1";
}

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

void closeAndRemove(p::Database& db, const char* file) {
    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. Operation log + version increments + vector update
// ---------------------------------------------------------------------------
void testOperationLog(Harness& h) {
    h.section("T1. operation log + per-entity version + vector");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t1_log.db")) {
        h.check(false, "open fresh db");
        return;
    }
    h.check(tableExists(db, "collab_operation_log"),
            "collab_operation_log table exists");
    h.check(tableExists(db, "collab_vector"), "collab_vector table exists");

    tl::CollaborationService svc(db);
    auto o1 = svc.recordOperation("requirement", "REQ-1", "create", "alice", "{}");
    h.check(o1.isOk() && o1.value().version == 1, "first op has version 1");
    auto o2 = svc.recordOperation("requirement", "REQ-1", "update", "alice", "{\"n\":2}");
    h.check(o2.isOk() && o2.value().version == 2, "second op has version 2");
    auto o3 = svc.recordOperation("requirement", "REQ-1", "update", "bob", "{\"n\":3}");
    h.check(o3.isOk() && o3.value().version == 3, "third op has version 3");

    auto ver = svc.currentVersion("REQ-1");
    h.check(ver.isOk() && ver.value() == 3, "currentVersion == 3");

    // Vector tracks both actors.
    auto vec = svc.vectorFor("REQ-1");
    h.check(vec.isOk(), "vectorFor() ok");
    if (vec.isOk()) {
        h.check(vec.value().size() == 2, "vector has 2 actors (alice, bob)");
    }

    // Changes feed: after version 1, returns 2 and 3.
    auto changes = svc.changesSince("REQ-1", 1);
    h.check(changes.isOk() && changes.value().size() == 2,
            "changesSince(after 1) returns 2 ops");
    if (changes.isOk() && changes.value().size() == 2) {
        h.check(changes.value()[0].version == 2, "first change is version 2");
        h.check(changes.value()[1].version == 3, "second change is version 3");
    }

    // Invalid op rejected.
    auto bad = svc.recordOperation("requirement", "REQ-1", "bogus", "a", "");
    h.check(bad.failed(), "invalid operation rejected");

    closeAndRemove(db, "lodestar_t1_log.db");
}

// ---------------------------------------------------------------------------
// T2. Optimistic concurrency: applied when base matches, NotCurrent when stale
// ---------------------------------------------------------------------------
void testOptimistic(Harness& h) {
    h.section("T2. optimistic concurrency (base-version match)");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t1_opt.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::CollaborationService svc(db);
    svc.recordOperation("requirement", "REQ-2", "create", "alice", "{}");  // v1

    // Correct base (1) -> applied -> v2.
    auto ok = svc.optimisticUpdate("requirement", "REQ-2", "alice", "{\"x\":1}", 1);
    h.check(ok.isOk(), "optimisticUpdate() ok");
    if (ok.isOk()) {
        h.check(ok.value().status == tl::EditStatus::Applied,
                "correct base applies");
        h.check(ok.value().currentVersion == 2, "version bumped to 2");
    }

    // Stale base (1, but current is 2) -> NotCurrent, not applied.
    auto stale = svc.optimisticUpdate("requirement", "REQ-2", "bob", "{\"y\":2}", 1);
    h.check(stale.isOk(), "stale optimisticUpdate() ok");
    if (stale.isOk()) {
        h.check(stale.value().status == tl::EditStatus::NotCurrent,
                "stale base reports NotCurrent");
        h.check(stale.value().currentVersion == 2,
                "NotCurrent carries current version");
        h.check(!stale.value().currentVector.empty(),
                "NotCurrent carries the current vector");
    }
    // The stale edit must NOT have advanced the version.
    auto ver = svc.currentVersion("REQ-2");
    h.check(ver.isOk() && ver.value() == 2, "version unchanged after rejected edit");

    closeAndRemove(db, "lodestar_t1_opt.db");
}

// ---------------------------------------------------------------------------
// T3. Merge detects a concurrent divergence (conflict)
// ---------------------------------------------------------------------------
void testMergeConflict(Harness& h) {
    h.section("T3. merge + conflict detection");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t1_merge.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::CollaborationService svc(db);
    // alice creates + edits (v1, v2).
    svc.recordOperation("requirement", "REQ-3", "create", "alice", "{}");  // v1
    svc.recordOperation("requirement", "REQ-3", "update", "alice", "{}");  // v2

    // Bob also edits the same entity (v3) -- concurrent divergence.
    svc.recordOperation("requirement", "REQ-3", "update", "bob", "{}");  // v3

    // Alice merges a remote vector (from bob) that advanced beyond alice's
    // seen version -> conflict.
    std::vector<tl::VectorElement> remote;
    remote.push_back({"alice", 2});
    remote.push_back({"bob", 3});
    auto res = svc.merge("REQ-3", "alice", remote);
    h.check(res.isOk(), "merge() ok");
    if (res.isOk()) {
        h.check(res.value() == tl::EditStatus::Conflict,
                "merge detects conflict (bob advanced beyond alice)");
    }

    // No divergence -> applied.
    auto vec = svc.vectorFor("REQ-3");
    // Construct a remote where bob has not exceeded alice.
    std::vector<tl::VectorElement> same;
    same.push_back({"alice", 2});
    same.push_back({"bob", 2});
    auto ok = svc.merge("REQ-3", "alice", same);
    h.check(ok.isOk() && ok.value() == tl::EditStatus::Applied,
            "merge applied when no divergence");

    closeAndRemove(db, "lodestar_t1_merge.db");
}

// ---------------------------------------------------------------------------
// T4. Conflict resolution: reconcile and record
// ---------------------------------------------------------------------------
void testResolution(Harness& h) {
    h.section("T4. conflict resolution via base-version retry");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t1_res.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::CollaborationService svc(db);
    svc.recordOperation("requirement", "REQ-4", "create", "alice", "{}");  // v1

    // Alice edits (v2).
    auto a = svc.optimisticUpdate("requirement", "REQ-4", "alice", "{\"alice\":1}", 1);
    h.check(a.isOk() && a.value().status == tl::EditStatus::Applied, "alice edit applied");

    // Bob edits concurrently with stale base (v2 current) -> NotCurrent.
    auto b = svc.optimisticUpdate("requirement", "REQ-4", "bob", "{\"bob\":1}", 1);
    h.check(b.isOk() && b.value().status == tl::EditStatus::NotCurrent,
            "bob concurrent edit flagged NotCurrent");

    // Bob resolves by rebasing to the current version (2) -> applied (v3).
    auto b2 = svc.optimisticUpdate("requirement", "REQ-4", "bob", "{\"bob\":2}", 2);
    h.check(b2.isOk() && b2.value().status == tl::EditStatus::Applied,
            "bob rebased edit applied");
    auto ver = svc.currentVersion("REQ-4");
    h.check(ver.isOk() && ver.value() == 3, "version advanced to 3 after resolution");

    closeAndRemove(db, "lodestar_t1_res.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill TraceLink 3.2 real-time collaboration");
    testOperationLog(h);
    testOptimistic(h);
    testMergeConflict(h);
    testResolution(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
