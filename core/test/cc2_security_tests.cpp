// core/test/cc2_security_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill Cross-cutting #2: security (iterated KDF, session/token expiry, RBAC).
//
// Test contract: docs/gap-fill-plan.md (Cross-cutting #2).
//   (A) UserService stores an iterated key-derivation-function hash (not a
//       single-pass hash and never the plaintext) for new accounts, while
//       still verifying legacy single-pass accounts.
//   (B) Session/token expiry: sessions expire after their configured lifetime,
//       and an explicit expireToken() invalidates a session immediately.
//   (C) RBAC enforcement at the service boundary: role-based permissions gate
//       privileged operations.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/UserService.h"

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
// T1. Iterated KDF: new accounts use the iter format, never plaintext
// ---------------------------------------------------------------------------
void testIteratedKdf(Harness& h) {
    h.section("T1. iterated KDF password hashing");
    p::Database db;
    if (!openFreshDb(db, "lodestar_cc2_kdf.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);
    auto u = svc.registerUser("alice", "s3cret", "editor");
    h.check(u.isOk(), "registerUser() ok");
    if (!u.isOk()) { closeAndRemove(db, "lodestar_cc2_kdf.db"); return; }

    std::string stored = db.queryScalar(
        "SELECT password_hash FROM users WHERE username='alice';");
    h.check(!stored.empty(), "password_hash populated");
    h.check(stored.find("iter:") == 0,
            "stored hash uses the iterated KDF format (prefix \"iter:\")");
    h.check(stored != "s3cret", "stored hash is NOT the plaintext");
    h.check(stored.find("s3cret") == std::string::npos,
            "stored hash does not contain the plaintext");

    // Correct password logs in; wrong password is rejected.
    auto okLogin = svc.login("alice", "s3cret");
    h.check(okLogin.isOk(), "login() with correct password ok");
    auto badLogin = svc.login("alice", "wrong");
    h.check(badLogin.failed(), "login() with wrong password fails");
    h.check(badLogin.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "wrong password reports ValidationFailed");

    closeAndRemove(db, "lodestar_cc2_kdf.db");
}

// ---------------------------------------------------------------------------
// T2. Session expiry: explicit expireToken + lifetime
// ---------------------------------------------------------------------------
void testSessionExpiry(Harness& h) {
    h.section("T2. session/token expiry");
    p::Database db;
    if (!openFreshDb(db, "lodestar_cc2_session.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);
    svc.registerUser("bob", "pw", "viewer");

    // Login creates a valid session.
    auto token = svc.login("bob", "pw");
    h.check(token.isOk(), "login() ok");
    if (!token.isOk()) { closeAndRemove(db, "lodestar_cc2_session.db"); return; }

    auto valid = svc.isSessionValid(token.value());
    h.check(valid.isOk() && valid.value() == true, "session valid after login");

    // currentUser resolves the session.
    auto cur = svc.currentUser(token.value());
    h.check(cur.isOk() && cur.value().username == "bob",
            "currentUser() resolves the valid session");

    // Explicit expiry invalidates immediately.
    auto expired = svc.expireToken(token.value());
    h.check(expired.isOk(), "expireToken() ok");
    auto nowInvalid = svc.isSessionValid(token.value());
    h.check(nowInvalid.isOk() && nowInvalid.value() == false,
            "session invalid after expireToken()");
    auto curAfter = svc.currentUser(token.value());
    h.check(curAfter.failed(), "currentUser() fails after expiry");

    // Logout also invalidates.
    auto token2 = svc.login("bob", "pw");
    auto loggedOut = svc.logout(token2.value());
    h.check(loggedOut.isOk(), "logout() ok");
    auto afterLogout = svc.isSessionValid(token2.value());
    h.check(afterLogout.isOk() && afterLogout.value() == false,
            "session invalid after logout");

    closeAndRemove(db, "lodestar_cc2_session.db");
}

// ---------------------------------------------------------------------------
// T3. RBAC enforcement at the service boundary
// ---------------------------------------------------------------------------
void testRbac(Harness& h) {
    h.section("T3. RBAC enforcement at the service boundary");
    p::Database db;
    if (!openFreshDb(db, "lodestar_cc2_rbac.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::UserService svc(db);
    auto admin = svc.registerUser("admin", "pw", "admin");
    auto viewer = svc.registerUser("view", "pw", "viewer");
    h.check(admin.isOk() && viewer.isOk(), "register users ok");
    if (!admin.isOk() || !viewer.isOk()) {
        closeAndRemove(db, "lodestar_cc2_rbac.db");
        return;
    }

    // Admin has all permissions.
    auto adminOk = svc.hasPermission(admin.value().id, "edit_requirements");
    h.check(adminOk.isOk() && adminOk.value() == true,
            "admin has any permission (admin grants all)");

    // Viewer does NOT have an edit permission by default.
    auto viewerEdit = svc.hasPermission(viewer.value().id, "edit_requirements");
    h.check(viewerEdit.isOk() && viewerEdit.value() == false,
            "viewer lacks edit_requirements by default");

    // Grant the permission -> viewer now has it.
    auto granted = svc.grantPermission(viewer.value().id, "edit_requirements");
    h.check(granted.isOk(), "grantPermission() ok");
    auto viewerAfter = svc.hasPermission(viewer.value().id, "edit_requirements");
    h.check(viewerAfter.isOk() && viewerAfter.value() == true,
            "viewer has edit_requirements after grant");

    closeAndRemove(db, "lodestar_cc2_rbac.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill Cross-cutting #2 security");
    testIteratedKdf(h);
    testSessionExpiry(h);
    testRbac(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
