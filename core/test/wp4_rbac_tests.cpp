// core/test/wp4_rbac_tests.cpp
// ---------------------------------------------------------------------------
// WP-4 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-4 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-4):
//   User roles + permissions (RBAC) on entities/links.
//   Concurrent-edit safety (optimistic locking / version check).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-4 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 016 (core/persistence/migrations/016_*.sql) creates `users`,
//     `roles` and `user_permissions` tables, append-only and idempotent.
//
// (B) New RbacService (core/tracelink/RbacService.h):
//
//   struct User { std::string id; std::string username; std::string role; };
//
//   class RbacService {
//   public:
//       explicit RbacService(persistence::Database& db);
//       common::Result<User> createUser(const std::string& username,
//                                       const std::string& role);
//       common::Result<void> grantPermission(const std::string& userId,
//                                            const std::string& permission,
//                                            const std::string& entityType = "");
//       common::Result<bool> hasPermission(const std::string& userId,
//                                          const std::string& permission,
//                                          const std::string& entityType = "");
//       common::Result<void> requirePermission(const std::string& userId,
//                                              const std::string& permission,
//                                              const std::string& entityType = "");
//   };
//
// (C) TraceLinkService addition (core/tracelink/TraceLinkService.h):
//
//   // Updates the entity ONLY if its current version equals expectedVersion.
//   // Fails (concurrent-edit conflict) if the stored version differs.
//   common::Result<Entity> updateEntityIfVersion(const Entity& e,
//                                                int expectedVersion);
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/RbacService.h"
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
// T1. Migration 016 applies
// ---------------------------------------------------------------------------
void testMigration016(Harness& h) {
    h.section("T1. Migration 016 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    // The users and user_permissions tables must exist after migration.
    h.check(!db.queryScalar(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='users';")
                 .empty(),
            "users table exists");
    h.check(!db.queryScalar("SELECT name FROM sqlite_master WHERE type='table' "
                            "AND name='user_permissions';")
                 .empty(),
            "user_permissions table exists");
    h.check(!db.queryScalar(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='roles';")
                 .empty(),
            "roles table exists");

    db.close();
    std::remove("lodestar_wp4_rbac_mig.db");
    std::remove("lodestar_wp4_rbac_mig.db-wal");
    std::remove("lodestar_wp4_rbac_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T2. createUser + duplicate rejection
// ---------------------------------------------------------------------------
void testCreateUser(Harness& h) {
    h.section("T2. createUser + duplicate rejection");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_user.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::RbacService rbac(db);

    auto alice = rbac.createUser("alice", "editor");
    h.check(alice.isOk(), "createUser(alice, editor) ok");
    h.check(alice.isOk() && !alice.value().id.empty(), "alice has a non-empty id");
    h.check(alice.isOk() && alice.value().username == "alice", "alice username set");
    h.check(alice.isOk() && alice.value().role == "editor", "alice role set");

    auto dup = rbac.createUser("alice", "viewer");
    h.check(dup.failed(), "duplicate username rejected");

    db.close();
    std::remove("lodestar_wp4_rbac_user.db");
    std::remove("lodestar_wp4_rbac_user.db-wal");
    std::remove("lodestar_wp4_rbac_user.db-shm");
}

// ---------------------------------------------------------------------------
// T3. grantPermission + hasPermission
// ---------------------------------------------------------------------------
void testGrantHasPermission(Harness& h) {
    h.section("T3. grantPermission + hasPermission");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_perm.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::RbacService rbac(db);

    auto alice = rbac.createUser("alice", "editor");
    auto bob = rbac.createUser("bob", "viewer");
    h.check(alice.isOk() && bob.isOk(), "create alice + bob");

    auto grant = rbac.grantPermission(alice.value().id, "edit");
    h.check(grant.isOk(), "grant edit to alice");

    auto hasEdit = rbac.hasPermission(alice.value().id, "edit");
    h.check(hasEdit.isOk() && hasEdit.value(), "alice has edit");

    auto hasDelete = rbac.hasPermission(alice.value().id, "delete");
    h.check(hasDelete.isOk() && !hasDelete.value(), "alice does not have delete");

    auto bobEdit = rbac.hasPermission(bob.value().id, "edit");
    h.check(bobEdit.isOk() && !bobEdit.value(), "bob does not have edit");

    db.close();
    std::remove("lodestar_wp4_rbac_perm.db");
    std::remove("lodestar_wp4_rbac_perm.db-wal");
    std::remove("lodestar_wp4_rbac_perm.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Admin has all permissions
// ---------------------------------------------------------------------------
void testAdminAllPermissions(Harness& h) {
    h.section("T4. Admin has all permissions");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_admin.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::RbacService rbac(db);

    auto root = rbac.createUser("root", "admin");
    h.check(root.isOk(), "create root (admin)");

    auto hasEdit = rbac.hasPermission(root.value().id, "edit");
    auto hasDelete = rbac.hasPermission(root.value().id, "delete");
    h.check(hasEdit.isOk() && hasEdit.value(), "admin has edit without grant");
    h.check(hasDelete.isOk() && hasDelete.value(), "admin has delete without grant");

    db.close();
    std::remove("lodestar_wp4_rbac_admin.db");
    std::remove("lodestar_wp4_rbac_admin.db-wal");
    std::remove("lodestar_wp4_rbac_admin.db-shm");
}

// ---------------------------------------------------------------------------
// T5. requirePermission enforces
// ---------------------------------------------------------------------------
void testRequirePermission(Harness& h) {
    h.section("T5. requirePermission enforces");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_req.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::RbacService rbac(db);

    auto alice = rbac.createUser("alice", "editor");
    h.check(alice.isOk(), "create alice");
    h.check(rbac.grantPermission(alice.value().id, "edit").isOk(),
            "grant edit to alice");

    auto okEdit = rbac.requirePermission(alice.value().id, "edit");
    h.check(okEdit.isOk(), "requirePermission(alice, edit) succeeds");

    auto failDelete = rbac.requirePermission(alice.value().id, "delete");
    h.check(failDelete.failed(), "requirePermission(alice, delete) fails");

    db.close();
    std::remove("lodestar_wp4_rbac_req.db");
    std::remove("lodestar_wp4_rbac_req.db-wal");
    std::remove("lodestar_wp4_rbac_req.db-shm");
}

// ---------------------------------------------------------------------------
// T6. Optimistic locking detects concurrent edit
// ---------------------------------------------------------------------------
void testOptimisticLocking(Harness& h) {
    h.section("T6. Optimistic locking detects concurrent edit");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_lock.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    tl::Entity r;
    r.externalId = "REQ-R";
    r.type = tl::EntityType::Requirement;
    r.name = "R";
    r.text = "v1";
    r.status = "Draft";
    auto added = svc.addEntity(r);
    h.check(added.isOk(), "add entity R");
    h.check(added.isOk() && added.value().version == 1, "R starts at version 1");

    // Update it once -> version 2.
    tl::Entity upd = added.value();
    upd.text = "v2";
    auto first = svc.updateEntity(upd);
    h.check(first.isOk() && first.value().version == 2, "first update -> version 2");

    // A stale expected version (1) must be rejected as a concurrent-edit conflict.
    tl::Entity stale = first.value();
    stale.text = "stale write";
    auto conflict = svc.updateEntityIfVersion(stale, 1);
    h.check(conflict.failed(), "stale expected version rejected");
    h.check(conflict.failed() &&
                conflict.errorCode() == lodestar::common::ErrorCode::ConcurrencyError,
            "conflict carries ConcurrencyError");

    // The current expected version (2) succeeds and bumps to 3.
    tl::Entity fresh = first.value();
    fresh.text = "v3";
    auto ok = svc.updateEntityIfVersion(fresh, 2);
    h.check(ok.isOk(), "current expected version accepted");
    h.check(ok.isOk() && ok.value().version == 3, "successful update bumps to version 3");

    db.close();
    std::remove("lodestar_wp4_rbac_lock.db");
    std::remove("lodestar_wp4_rbac_lock.db-wal");
    std::remove("lodestar_wp4_rbac_lock.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Version increments on each update
// ---------------------------------------------------------------------------
void testVersionIncrements(Harness& h) {
    h.section("T7. Version increments on each update");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_rbac_ver.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    tl::Entity e;
    e.externalId = "REQ-V";
    e.type = tl::EntityType::Requirement;
    e.name = "V";
    e.text = "base";
    e.status = "Draft";
    auto added = svc.addEntity(e);
    h.check(added.isOk() && added.value().version == 1, "version 1 after add");

    auto u1 = added.value();
    u1.text = "one";
    auto r1 = svc.updateEntity(u1);
    h.check(r1.isOk() && r1.value().version == 2, "version 2 after first update");

    auto u2 = r1.value();
    u2.text = "two";
    auto r2 = svc.updateEntity(u2);
    h.check(r2.isOk() && r2.value().version == 3, "version 3 after second update");

    db.close();
    std::remove("lodestar_wp4_rbac_ver.db");
    std::remove("lodestar_wp4_rbac_ver.db-wal");
    std::remove("lodestar_wp4_rbac_ver.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-4 RBAC + concurrency");
    std::printf("WP-4 RBAC + CONCURRENCY TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration016(h);
    testCreateUser(h);
    testGrantHasPermission(h);
    testAdminAllPermissions(h);
    testRequirePermission(h);
    testOptimisticLocking(h);
    testVersionIncrements(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
