// core/test/s2_phase1_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 1 tests (test contract): User model + RBAC + concurrent editing.
//
// Written by the scrum-master BEFORE the Phase 1 engineer implements the
// feature. The engineer must implement the contract below so these tests
// compile and pass. Do NOT weaken the assertions; implement the feature to
// satisfy them.
//
// Covers (PLAN.md, S2 Phase 1):
//   (A) User accounts with login: registerUser/login/logout/currentUser.
//   (B) Roles + permissions: changeRole, hasPermission, grantPermission.
//   (C) Concurrent-editing conflict handling: optimistic-lock updateEntity.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each DB-dependent test opens its own fresh throwaway DB.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 1 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 023 (core/persistence/migrations/023_*.sql) adds a salted
//     password_hash + optimistic-lock version to `users` and creates a
//     `sessions` table.
//
// (B) New UserService (core/tracelink/UserService.h):
//
//   struct UserAccount { std::string id; std::string username; std::string role; };
//
//   class UserService {
//   public:
//       explicit UserService(persistence::Database& db);
//       common::Result<UserAccount> registerUser(const std::string& username,
//                                                const std::string& password,
//                                                const std::string& role);
//       common::Result<std::string> login(const std::string& username,
//                                         const std::string& password);
//       common::Result<void> logout(const std::string& token);
//       common::Result<UserAccount> currentUser(const std::string& token);
//       common::Result<std::vector<UserAccount>> listUsers();
//       common::Result<void> changeRole(const std::string& userId,
//                                       const std::string& newRole);
//       common::Result<void> grantPermission(const std::string& userId,
//                                            const std::string& permission,
//                                            const std::string& entityType = "");
//       common::Result<bool> hasPermission(const std::string& userId,
//                                          const std::string& permission,
//                                          const std::string& entityType = "");
//       common::Result<void> updateEntity(const std::string& type,
//                                         const std::string& id,
//                                         const std::string& newData,
//                                         int expectedVersion);
//   };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UserService.h"

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
// T1. registerUser stores a salted hash, not plaintext
// ---------------------------------------------------------------------------
void testRegisterUser(Harness& h) {
    h.section("T1. registerUser stores a salted hash, not plaintext");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_register.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tl::UserService svc(db);

    auto alice = svc.registerUser("alice", "s3cret", "editor");
    h.check(alice.isOk(), "registerUser(alice, s3cret, editor) ok");
    h.check(alice.isOk() && !alice.value().id.empty(), "alice has a non-empty id");
    h.check(alice.isOk() && alice.value().username == "alice", "alice username set");
    h.check(alice.isOk() && alice.value().role == "editor", "alice role set");

    // The stored password column must NOT equal the plaintext.
    std::string stored = db.queryScalar(
        "SELECT password_hash FROM users WHERE username='alice';");
    h.check(!stored.empty(), "password_hash column is populated");
    h.check(stored != "s3cret", "stored password is NOT the plaintext");
    h.check(stored.find("s3cret") == std::string::npos,
            "stored password does not contain the plaintext");

    // Duplicate username fails.
    auto dup = svc.registerUser("alice", "other", "viewer");
    h.check(dup.failed(), "duplicate username rejected");

    db.close();
    std::remove("lodestar_s2p1_register.db");
    std::remove("lodestar_s2p1_register.db-wal");
    std::remove("lodestar_s2p1_register.db-shm");
}

// ---------------------------------------------------------------------------
// T2. login/logout/currentUser round-trip
// ---------------------------------------------------------------------------
void testLoginLogout(Harness& h) {
    h.section("T2. login/logout/currentUser round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_login.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);

    auto alice = svc.registerUser("alice", "s3cret", "editor");
    h.check(alice.isOk(), "register alice");

    auto token = svc.login("alice", "s3cret");
    h.check(token.isOk(), "login(alice, s3cret) ok");
    h.check(token.isOk() && !token.value().empty(), "login returns a non-empty token");

    auto me = svc.currentUser(token.value());
    h.check(me.isOk(), "currentUser(token) ok");
    h.check(me.isOk() && me.value().username == "alice", "currentUser is alice");
    h.check(me.isOk() && me.value().role == "editor", "currentUser role is editor");

    auto out = svc.logout(token.value());
    h.check(out.isOk(), "logout(token) ok");

    auto after = svc.currentUser(token.value());
    h.check(after.failed(), "currentUser(token) fails after logout");

    db.close();
    std::remove("lodestar_s2p1_login.db");
    std::remove("lodestar_s2p1_login.db-wal");
    std::remove("lodestar_s2p1_login.db-shm");
}

// ---------------------------------------------------------------------------
// T3. wrong password rejected
// ---------------------------------------------------------------------------
void testWrongPassword(Harness& h) {
    h.section("T3. wrong password rejected");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_wrong.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);

    auto alice = svc.registerUser("alice", "s3cret", "editor");
    h.check(alice.isOk(), "register alice");

    auto bad = svc.login("alice", "wrong");
    h.check(bad.failed(), "login(alice, wrong) fails");

    auto unknown = svc.login("nobody", "s3cret");
    h.check(unknown.failed(), "login(unknown user) fails");

    db.close();
    std::remove("lodestar_s2p1_wrong.db");
    std::remove("lodestar_s2p1_wrong.db-wal");
    std::remove("lodestar_s2p1_wrong.db-shm");
}

// ---------------------------------------------------------------------------
// T4. changeRole updates the role
// ---------------------------------------------------------------------------
void testChangeRole(Harness& h) {
    h.section("T4. changeRole updates the role");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_role.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);

    auto alice = svc.registerUser("alice", "s3cret", "editor");
    h.check(alice.isOk(), "register alice (editor)");

    auto token = svc.login("alice", "s3cret");
    h.check(token.isOk(), "login alice");

    auto changed = svc.changeRole(alice.value().id, "admin");
    h.check(changed.isOk(), "changeRole(aliceId, admin) ok");

    auto me = svc.currentUser(token.value());
    h.check(me.isOk() && me.value().role == "admin", "currentUser role is now admin");

    db.close();
    std::remove("lodestar_s2p1_role.db");
    std::remove("lodestar_s2p1_role.db-wal");
    std::remove("lodestar_s2p1_role.db-shm");
}

// ---------------------------------------------------------------------------
// T5. hasPermission honors role + grant
// ---------------------------------------------------------------------------
void testHasPermission(Harness& h) {
    h.section("T5. hasPermission honors role + grant");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_perm.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);

    auto viewer = svc.registerUser("viewer1", "pw", "viewer");
    auto admin = svc.registerUser("root", "pw", "admin");
    h.check(viewer.isOk() && admin.isOk(), "register viewer + admin");

    // A viewer lacks 'edit' on requirement by default.
    auto before = svc.hasPermission(viewer.value().id, "edit", "requirement");
    h.check(before.isOk() && !before.value(), "viewer lacks edit before grant");

    // Grant 'edit' scoped to requirement.
    auto grant = svc.grantPermission(viewer.value().id, "edit", "requirement");
    h.check(grant.isOk(), "grantPermission(viewer, edit, requirement) ok");

    auto after = svc.hasPermission(viewer.value().id, "edit", "requirement");
    h.check(after.isOk() && after.value(), "viewer has edit after grant");

    // Admin has all permissions without a grant.
    auto adminEdit = svc.hasPermission(admin.value().id, "edit", "requirement");
    auto adminDelete = svc.hasPermission(admin.value().id, "delete", "requirement");
    h.check(adminEdit.isOk() && adminEdit.value(), "admin has edit");
    h.check(adminDelete.isOk() && adminDelete.value(), "admin has delete");

    db.close();
    std::remove("lodestar_s2p1_perm.db");
    std::remove("lodestar_s2p1_perm.db-wal");
    std::remove("lodestar_s2p1_perm.db-shm");
}

// ---------------------------------------------------------------------------
// T6. concurrent edit conflict detected
// ---------------------------------------------------------------------------
void testConcurrentEdit(Harness& h) {
    h.section("T6. concurrent edit conflict detected");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p1_conflict.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);

    // Seed an entity at version 1.
    tl::TraceLinkService tls(db);
    tl::Entity r;
    r.externalId = "REQ-C";
    r.type = tl::EntityType::Requirement;
    r.name = "C";
    r.text = "v1";
    r.status = "Draft";
    auto added = tls.addEntity(r);
    h.check(added.isOk() && added.value().version == 1, "seed entity at version 1");
    std::string id = added.value().id;

    // First update with expectedVersion=1 succeeds and bumps to 2.
    auto first = svc.updateEntity("requirement", id, "v2", 1);
    h.check(first.isOk(), "updateEntity(..., expectedVersion=1) ok");

    // A stale expectedVersion=1 must be rejected as a conflict.
    auto conflict = svc.updateEntity("requirement", id, "stale write", 1);
    h.check(conflict.failed(), "stale expectedVersion rejected");
    h.check(conflict.failed() &&
                conflict.errorCode() == lodestar::common::ErrorCode::ConcurrencyError,
            "conflict carries ConcurrencyError");

    // The stored data must NOT be overwritten by the stale write.
    auto current = tls.getEntity(tl::EntityType::Requirement, id);
    h.check(current.isOk() && current.value().has_value(), "entity still present");
    h.check(current.isOk() && current.value().has_value() &&
                current.value().value().text == "v2",
            "stored text is still v2 (stale write did not overwrite)");

    // A fresh expectedVersion=2 succeeds.
    auto ok = svc.updateEntity("requirement", id, "v3", 2);
    h.check(ok.isOk(), "updateEntity(..., expectedVersion=2) ok");

    db.close();
    std::remove("lodestar_s2p1_conflict.db");
    std::remove("lodestar_s2p1_conflict.db-wal");
    std::remove("lodestar_s2p1_conflict.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 1 User model + RBAC + concurrency");
    std::printf("S2 PHASE 1 TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testRegisterUser(h);
    testLoginLogout(h);
    testWrongPassword(h);
    testChangeRole(h);
    testHasPermission(h);
    testConcurrentEdit(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
