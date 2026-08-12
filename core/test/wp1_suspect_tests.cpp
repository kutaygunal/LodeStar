// core/test/wp1_suspect_tests.cpp
// ---------------------------------------------------------------------------
// Phase 10 WP-1 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-1 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-1):
//   A1. Suspect-link workflow: auto-flag downstream artifacts as `suspect`
//       when a requirement changes; review/clear queue; suspect status on
//       links/entities; migration 013.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-1 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 013 (core/persistence/migrations/013_*.sql) creates a
//     `suspect_flags` table (append-only, idempotent):
//       id, entity_type, entity_id, reason, source_type, source_id,
//       created_at, cleared_at, cleared_by.
//
// (B) New SuspectService (core/tracelink/SuspectService.h):
//
//   struct SuspectFlag {
//       std::string id;
//       std::string entityType;
//       std::string entityId;
//       std::string reason;
//       std::string sourceType;
//       std::string sourceId;
//       std::string createdAt;
//   };
//
//   class SuspectService {
//   public:
//       explicit SuspectService(persistence::Database& db);
//
//       common::Result<SuspectFlag> flagSuspect(
//           const std::string& entityType, const std::string& entityId,
//           const std::string& reason,
//           const std::string& sourceType, const std::string& sourceId);
//
//       common::Result<std::vector<SuspectFlag>> suspectQueue();
//
//       common::Result<bool> isSuspect(const std::string& entityType,
//                                      const std::string& entityId);
//
//       common::Result<void> clearSuspect(const std::string& flagId,
//                                         const std::string& clearedBy);
//
//       common::Result<std::vector<SuspectFlag>> autoFlagDownstream(
//           const std::string& entityType, const std::string& entityId,
//           const std::string& reason);
//   };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/SuspectService.h"
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

tl::Entity makeReq(const std::string& extId, const std::string& name) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = name;
    e.text = "Body of " + extId;
    e.status = "Draft";
    return e;
}

tl::Entity makeDesign(const std::string& extId, const std::string& name) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = name;
    e.text = "Body of " + extId;
    e.status = "Draft";
    return e;
}

tl::Entity makeTest(const std::string& extId, const std::string& name) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = name;
    e.text = "Body of " + extId;
    e.status = "Draft";
    return e;
}

// ---------------------------------------------------------------------------
// T1. Migration 013 applies
// ---------------------------------------------------------------------------
void testMigration013(Harness& h) {
    h.section("T1. Migration 013 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }

    // The suspect_flags table exists: a flag can be inserted and read back.
    auto ins = db.execute(
        "INSERT INTO suspect_flags (id, entity_type, entity_id, reason, "
        "source_type, source_id, created_at, cleared_at, cleared_by) "
        "VALUES ('f1','design','d1','req changed','requirement','r1',"
        "'2024-01-01T00:00:00Z','','');");
    h.check(ins.isOk(), "insert into suspect_flags ok");

    auto got = db.queryScalar(
        "SELECT entity_id FROM suspect_flags WHERE id='f1';");
    h.check(got == "d1", "suspect_flags row read back ok");

    db.close();
    std::remove("lodestar_wp1_suspect_t1.db");
    std::remove("lodestar_wp1_suspect_t1.db-wal");
    std::remove("lodestar_wp1_suspect_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. flagSuspect + isSuspect
// ---------------------------------------------------------------------------
void testFlagAndIsSuspect(Harness& h) {
    h.section("T2. flagSuspect + isSuspect");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::SuspectService svc(db);

    auto flag = svc.flagSuspect("design", "dId", "req changed", "requirement", "rId");
    h.check(flag.isOk(), "flagSuspect ok");
    h.check(!flag.value().id.empty(), "flag has a non-empty id");

    auto isD = svc.isSuspect("design", "dId");
    h.check(isD.isOk() && isD.value(), "isSuspect(design, dId) is true");

    auto isOther = svc.isSuspect("design", "otherId");
    h.check(isOther.isOk() && !isOther.value(), "isSuspect(design, otherId) is false");

    db.close();
    std::remove("lodestar_wp1_suspect_t2.db");
    std::remove("lodestar_wp1_suspect_t2.db-wal");
    std::remove("lodestar_wp1_suspect_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. suspectQueue returns active flags newest first
// ---------------------------------------------------------------------------
void testQueueNewestFirst(Harness& h) {
    h.section("T3. suspectQueue returns active flags newest first");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::SuspectService svc(db);

    auto a = svc.flagSuspect("design", "A", "req changed", "requirement", "r1");
    auto b = svc.flagSuspect("design", "B", "req changed", "requirement", "r1");
    h.check(a.isOk() && b.isOk(), "flag A and B ok");

    auto q = svc.suspectQueue();
    h.check(q.isOk() && q.value().size() == 2, "suspectQueue returns both flags");

    // B (newest) is first.
    h.check(q.value().size() == 2 && q.value()[0].entityId == "B",
            "newest flag (B) is first in the queue");

    // Both are active.
    bool bothActive = true;
    for (const auto& f : q.value()) {
        if (f.entityId != "A" && f.entityId != "B") bothActive = false;
    }
    h.check(bothActive, "both flags are active");

    db.close();
    std::remove("lodestar_wp1_suspect_t3.db");
    std::remove("lodestar_wp1_suspect_t3.db-wal");
    std::remove("lodestar_wp1_suspect_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. clearSuspect removes from queue
// ---------------------------------------------------------------------------
void testClearSuspect(Harness& h) {
    h.section("T4. clearSuspect removes from queue");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::SuspectService svc(db);

    auto a = svc.flagSuspect("design", "A", "req changed", "requirement", "r1");
    auto b = svc.flagSuspect("design", "B", "req changed", "requirement", "r1");
    h.check(a.isOk() && b.isOk(), "flag A and B ok");
    const std::string aId = a.value().id;

    auto cleared = svc.clearSuspect(aId, "engineer");
    h.check(cleared.isOk(), "clearSuspect(A) ok");

    auto q = svc.suspectQueue();
    h.check(q.isOk(), "suspectQueue ok");
    bool aGone = true;
    for (const auto& f : q.value()) {
        if (f.id == aId) aGone = false;
    }
    h.check(aGone, "suspectQueue no longer contains A");

    auto isA = svc.isSuspect("design", "A");
    h.check(isA.isOk() && !isA.value(), "isSuspect(A) is false after clear");

    auto isB = svc.isSuspect("design", "B");
    h.check(isB.isOk() && isB.value(), "B remains active");

    // Clearing an already-cleared flag is a no-op (still ok).
    auto clearAgain = svc.clearSuspect(aId, "engineer");
    h.check(clearAgain.isOk(), "clearing an already-cleared flag is a no-op");

    db.close();
    std::remove("lodestar_wp1_suspect_t4.db");
    std::remove("lodestar_wp1_suspect_t4.db-wal");
    std::remove("lodestar_wp1_suspect_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. autoFlagDownstream flags downstream artifacts
// ---------------------------------------------------------------------------
void testAutoFlagDownstream(Harness& h) {
    h.section("T5. autoFlagDownstream flags downstream artifacts");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::SuspectService suspects(db);

    auto r = svc.addEntity(makeReq("REQ-R", "Requirement R"));
    h.check(r.isOk(), "add requirement R ok");
    const std::string rId = r.value().id;

    auto d = svc.addEntity(makeDesign("DES-D", "Design D"));
    h.check(d.isOk(), "add design D ok");
    const std::string dId = d.value().id;

    auto tc = svc.addEntity(makeTest("TC-T", "Test TC"));
    h.check(tc.isOk(), "add test TC ok");
    const std::string tcId = tc.value().id;

    // D satisfies R; TC verifies R.
    tl::Link l1;
    l1.sourceType = tl::EntityType::Design;
    l1.sourceId = dId;
    l1.targetType = tl::EntityType::Requirement;
    l1.targetId = rId;
    l1.relation = "satisfies";
    auto add1 = svc.addLink(l1);
    h.check(add1.isOk(), "add satisfies link D->R ok");

    tl::Link l2;
    l2.sourceType = tl::EntityType::TestCase;
    l2.sourceId = tcId;
    l2.targetType = tl::EntityType::Requirement;
    l2.targetId = rId;
    l2.relation = "verifies";
    auto add2 = svc.addLink(l2);
    h.check(add2.isOk(), "add verifies link TC->R ok");

    auto flags = suspects.autoFlagDownstream("requirement", rId, "req changed");
    h.check(flags.isOk(), "autoFlagDownstream ok");

    auto isD = suspects.isSuspect("design", dId);
    h.check(isD.isOk() && isD.value(), "isSuspect(design, dId) is true");

    auto isTC = suspects.isSuspect("test_case", tcId);
    h.check(isTC.isOk() && isTC.value(), "isSuspect(test_case, tcId) is true");

    // The flags carry source_type=requirement and source_id=rId.
    bool srcOk = true;
    for (const auto& f : flags.value()) {
        if (f.sourceType != "requirement" || f.sourceId != rId) srcOk = false;
    }
    h.check(srcOk, "flags carry source_type=requirement and source_id=rId");

    db.close();
    std::remove("lodestar_wp1_suspect_t5.db");
    std::remove("lodestar_wp1_suspect_t5.db-wal");
    std::remove("lodestar_wp1_suspect_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. Suspect status on links
// ---------------------------------------------------------------------------
void testSuspectLinks(Harness& h) {
    h.section("T6. Suspect status on links");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_suspect_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::SuspectService suspects(db);

    auto r = svc.addEntity(makeReq("REQ-R", "Requirement R"));
    const std::string rId = r.value().id;
    auto d = svc.addEntity(makeDesign("DES-D", "Design D"));
    const std::string dId = d.value().id;
    auto tc = svc.addEntity(makeTest("TC-T", "Test TC"));
    const std::string tcId = tc.value().id;

    tl::Link l1;
    l1.sourceType = tl::EntityType::Design;
    l1.sourceId = dId;
    l1.targetType = tl::EntityType::Requirement;
    l1.targetId = rId;
    l1.relation = "satisfies";
    auto add1 = svc.addLink(l1);
    h.check(add1.isOk(), "add satisfies link D->R ok");
    const std::string l1Id = add1.value().id;

    tl::Link l2;
    l2.sourceType = tl::EntityType::TestCase;
    l2.sourceId = tcId;
    l2.targetType = tl::EntityType::Requirement;
    l2.targetId = rId;
    l2.relation = "verifies";
    auto add2 = svc.addLink(l2);
    h.check(add2.isOk(), "add verifies link TC->R ok");
    const std::string l2Id = add2.value().id;

    // Before the change, no links are suspect.
    auto before = suspects.suspectLinks();
    h.check(before.isOk() && before.value().empty(),
            "no suspect links before the change");

    // After the requirement changes, the incident links are reported suspect.
    auto flags = suspects.autoFlagDownstream("requirement", rId, "req changed");
    h.check(flags.isOk(), "autoFlagDownstream ok");

    auto links = suspects.suspectLinks();
    h.check(links.isOk(), "suspectLinks ok");
    bool hasL1 = false, hasL2 = false;
    for (const auto& id : links.value()) {
        if (id == l1Id) hasL1 = true;
        if (id == l2Id) hasL2 = true;
    }
    h.check(hasL1 && hasL2, "verifies/satisfies links incident to R are flagged");

    // Clearing the flags removes the links from the queue.
    for (const auto& f : flags.value()) {
        auto c = suspects.clearSuspect(f.id, "engineer");
        h.check(c.isOk(), "clear flag ok");
    }
    auto after = suspects.suspectLinks();
    h.check(after.isOk() && after.value().empty(),
            "clearing the flags removes the links from the queue");

    db.close();
    std::remove("lodestar_wp1_suspect_t6.db");
    std::remove("lodestar_wp1_suspect_t6.db-wal");
    std::remove("lodestar_wp1_suspect_t6.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-1 suspect-link workflow");
    std::printf("WP-1 SUSPECT-LINK WORKFLOW TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration013(h);
    testFlagAndIsSuspect(h);
    testQueueNewestFirst(h);
    testClearSuspect(h);
    testAutoFlagDownstream(h);
    testSuspectLinks(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
