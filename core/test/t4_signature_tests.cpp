// core/test/t4_signature_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TraceLink 3.4: electronic signatures tests.
//
// Test contract: docs/gap-fill-plan.md (Module 3.4).
//   (A) Migration 033 creates esignature.
//   (B) core/tracelink/SignatureService.h (+ .cpp): approval-signature model
//       (signer, role, timestamp, hash of approved content) on the review/
//       approval workflow; immutable persistence; validity on content change.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/SignatureService.h"

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

void seedApprovedReview(p::Database& db, const std::string& entityType,
                        const std::string& entityId) {
    // Create the reviews table (migration 014) and an approving review.
    db.execute(
        "CREATE TABLE IF NOT EXISTS reviews ("
        " id TEXT PRIMARY KEY, entity_type TEXT, entity_id TEXT, "
        " reviewer TEXT, verdict TEXT, comment TEXT, created_at TEXT);");
    db.execute("INSERT INTO reviews (id, entity_type, entity_id, reviewer, "
               "verdict, comment, created_at) VALUES "
               "('RV-1','" + entityType + "','" + entityId +
               "','engineer','Approve','ok','now');");
}

// ---------------------------------------------------------------------------
// T1. Sign requires an approved review + persists signature
// ---------------------------------------------------------------------------
void testSign(Harness& h) {
    h.section("T1. sign + immutable persistence");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t4_sign.db")) {
        h.check(false, "open fresh db");
        return;
    }
    h.check(tableExists(db, "esignature"), "esignature table exists");
    tl::SignatureService svc(db);

    // No approved review -> cannot sign.
    auto noApproval = svc.sign("requirement", "REQ-1", "alice", "engineer", "content v1");
    h.check(noApproval.failed(), "sign() rejected without an approved review");
    h.check(noApproval.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "rejected sign reports ValidationFailed");

    // Add an approving review, then sign.
    seedApprovedReview(db, "requirement", "REQ-1");
    auto sig = svc.sign("requirement", "REQ-1", "alice", "certifying engineer", "content v1");
    h.check(sig.isOk(), "sign() ok after approval");
    if (!sig.isOk()) { closeAndRemove(db, "lodestar_t4_sign.db"); return; }
    h.check(!sig.value().id.empty(), "signature has an id");
    h.check(sig.value().signer == "alice", "signer recorded");
    h.check(sig.value().role == "certifying engineer", "role recorded");
    h.check(!sig.value().signedAt.empty(), "timestamp recorded");
    h.check(!sig.value().contentHash.empty(), "content hash recorded");
    h.check(sig.value().contentHash == tl::SignatureService::hashContent("content v1"),
            "content hash == SHA-256 of approved content");

    auto last = svc.lastSignature("requirement", "REQ-1");
    h.check(last.isOk() && last.value().has_value(),
            "lastSignature() returns the signature");
    if (last.isOk() && last.value().has_value()) {
        h.check(last.value()->signer == "alice", "persisted signature signer == alice");
    }

    closeAndRemove(db, "lodestar_t4_sign.db");
}

// ---------------------------------------------------------------------------
// T2. Signature validity on content change
// ---------------------------------------------------------------------------
void testValidity(Harness& h) {
    h.section("T2. validity on content change");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t4_valid.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::SignatureService svc(db);
    seedApprovedReview(db, "requirement", "REQ-2");
    auto sig = svc.sign("requirement", "REQ-2", "alice", "engineer", "approved text");
    h.check(sig.isOk(), "sign() ok");

    // Same content -> valid.
    auto valid = svc.isValid("requirement", "REQ-2", "approved text");
    h.check(valid.isOk() && valid.value() == true,
            "signature valid when content unchanged");

    // Changed content -> invalid.
    auto invalid = svc.isValid("requirement", "REQ-2", "approved text CHANGED");
    h.check(invalid.isOk() && invalid.value() == false,
            "signature invalid when content changed");

    // No signature -> invalid.
    auto none = svc.isValid("requirement", "no-such", "anything");
    h.check(none.isOk() && none.value() == false,
            "no signature => invalid");

    closeAndRemove(db, "lodestar_t4_valid.db");
}

// ---------------------------------------------------------------------------
// T3. Multiple signatures + certification export surface
// ---------------------------------------------------------------------------
void testMultiple(Harness& h) {
    h.section("T3. multiple signatures + export surface");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t4_multi.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::SignatureService svc(db);
    seedApprovedReview(db, "requirement", "REQ-3");
    h.check(svc.sign("requirement", "REQ-3", "alice", "engineer", "v1").isOk(),
            "alice signs");
    h.check(svc.sign("requirement", "REQ-3", "bob", "reviewer", "v1").isOk(),
            "bob signs");

    auto sigs = svc.signaturesFor("requirement", "REQ-3");
    h.check(sigs.isOk() && sigs.value().size() == 2,
            "signaturesFor() returns 2 signatures");
    if (sigs.isOk() && sigs.value().size() == 2) {
        h.check(sigs.value()[0].signer == "alice", "oldest signature is alice");
        h.check(sigs.value()[1].signer == "bob", "newest signature is bob");
        h.check(sigs.value()[0].contentHash == sigs.value()[1].contentHash,
                "both sign the same content hash (same approved text)");
    }

    // The last signature is the newest (bob).
    auto last = svc.lastSignature("requirement", "REQ-3");
    h.check(last.isOk() && last.value().has_value() &&
                last.value()->signer == "bob",
            "lastSignature() is the newest (bob)");

    closeAndRemove(db, "lodestar_t4_multi.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill TraceLink 3.4 electronic signatures");
    testSign(h);
    testValidity(h);
    testMultiple(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
