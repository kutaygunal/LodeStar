// core/test/s2_phase3_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 3 (AssureCheck) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 3 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase3-test.md): the review/approval/sign-off workflow with
// real actors + real timestamps (fixing the "now" placeholder), the audit
// trail, and the objective->evidence package. Uses migration 024
// (assurance_workflow_audit + workflow columns on assurance_checks).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 3 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 024 (core/persistence/migrations/024_assurecheck_workflow.sql)
//     adds workflow columns to assurance_checks and creates the audit table.
// (B) core/assurecheck/WorkflowService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/WorkflowService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
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

bool tableExists(p::Database& db, const std::string& table) {
    auto rows = db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
        table + "';");
    return rows == "1";
}

// Seeds the five standards and returns the first DO-178C checklist item's
// standard_id and item_id. Returns false on failure.
bool seedAndGetItem(p::Database& db, std::string& standardId,
                    std::string& itemId) {
    ac::AssureCheckService svc(db);
    if (svc.seedStandards().failed()) return false;
    auto items = svc.checklistFor("DO-178C");
    if (items.failed() || items.value().empty()) return false;
    standardId = items.value().front().standardId;
    itemId = items.value().front().id;
    return true;
}

// Inserts one check result row into assurance_checks.
void insertResult(p::Database& db, const std::string& resultId,
                  const std::string& standardId, const std::string& itemId,
                  const std::string& evidence) {
    db.execute("INSERT INTO assurance_checks "
               "(id, standard_id, item_id, item_code, status, dal_level, "
               "evidence, detail, checked_at) VALUES ('" +
               resultId + "','" + standardId + "','" + itemId +
               "','A1-1','PASS','A-D','" + evidence +
               "','','2024-01-01T00:00:00');");
}

// Returns true if `s` looks like a real date/time (not the literal "now").
bool isRealTimestamp(const std::string& s) {
    if (s.empty() || s == "now") return false;
    // Expect at least "YYYY-MM-DD" (10 chars) with a '-' separator.
    if (s.size() < 10) return false;
    return s.find('-') != std::string::npos;
}

// ---------------------------------------------------------------------------
// T0. Migration 024 applies (workflow columns + audit table)
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T0. Migration 024 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "assurance_workflow_audit"),
            "assurance_workflow_audit table exists");
    // workflow_state column exists on assurance_checks.
    auto col = db.queryScalar(
        "SELECT COUNT(*) FROM pragma_table_info('assurance_checks') "
        "WHERE name='workflow_state';");
    h.check(col == "1", "assurance_checks.workflow_state column exists");

    db.close();
    std::remove("lodestar_s2p3_mig.db");
    std::remove("lodestar_s2p3_mig.db-wal");
    std::remove("lodestar_s2p3_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T1. submitForReview records a real actor + timestamp
// ---------------------------------------------------------------------------
void testSubmit(Harness& h) {
    h.section("T1. submitForReview records a real actor + timestamp");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_submit.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string stdId, itemId;
    if (!seedAndGetItem(db, stdId, itemId)) {
        h.check(false, "seed standards + get item");
        db.close();
        return;
    }
    insertResult(db, "res1", stdId, itemId, "");

    ac::WorkflowService wf(db);
    auto sub = wf.submitForReview("res1", "alice");
    h.check(sub.isOk(), "submitForReview(\"res1\", \"alice\") ok");

    auto reviewedBy = db.queryScalar(
        "SELECT reviewed_by FROM assurance_checks WHERE id='res1';");
    auto reviewedAt = db.queryScalar(
        "SELECT reviewed_at FROM assurance_checks WHERE id='res1';");
    h.check(reviewedBy == "alice", "reviewed_by == \"alice\"");
    h.check(isRealTimestamp(reviewedAt),
            "reviewed_at is a real timestamp (not \"now\")");

    db.close();
    std::remove("lodestar_s2p3_submit.db");
    std::remove("lodestar_s2p3_submit.db-wal");
    std::remove("lodestar_s2p3_submit.db-shm");
}

// ---------------------------------------------------------------------------
// T2. approve transitions state to approved
// ---------------------------------------------------------------------------
void testApprove(Harness& h) {
    h.section("T2. approve transitions state to approved");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_approve.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string stdId, itemId;
    if (!seedAndGetItem(db, stdId, itemId)) {
        h.check(false, "seed standards + get item");
        db.close();
        return;
    }
    insertResult(db, "res1", stdId, itemId, "");

    ac::WorkflowService wf(db);
    h.check(wf.submitForReview("res1", "alice").isOk(),
            "submitForReview ok");
    auto app = wf.approve("res1", "bob");
    h.check(app.isOk(), "approve(\"res1\", \"bob\") ok");

    auto state = wf.stateFor("res1");
    h.check(state.isOk() && state.value() == "approved",
            "state is approved");
    auto approvedBy = db.queryScalar(
        "SELECT approved_by FROM assurance_checks WHERE id='res1';");
    h.check(approvedBy == "bob", "approved_by == \"bob\"");

    db.close();
    std::remove("lodestar_s2p3_approve.db");
    std::remove("lodestar_s2p3_approve.db-wal");
    std::remove("lodestar_s2p3_approve.db-shm");
}

// ---------------------------------------------------------------------------
// T3. reject transitions state to rejected
// ---------------------------------------------------------------------------
void testReject(Harness& h) {
    h.section("T3. reject transitions state to rejected");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_reject.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string stdId, itemId;
    if (!seedAndGetItem(db, stdId, itemId)) {
        h.check(false, "seed standards + get item");
        db.close();
        return;
    }
    insertResult(db, "res1", stdId, itemId, "");

    ac::WorkflowService wf(db);
    h.check(wf.submitForReview("res1", "alice").isOk(),
            "submitForReview ok");
    auto rej = wf.reject("res1", "carol");
    h.check(rej.isOk(), "reject(\"res1\", \"carol\") ok");

    auto state = wf.stateFor("res1");
    h.check(state.isOk() && state.value() == "rejected",
            "state is rejected");

    db.close();
    std::remove("lodestar_s2p3_reject.db");
    std::remove("lodestar_s2p3_reject.db-wal");
    std::remove("lodestar_s2p3_reject.db-shm");
}

// ---------------------------------------------------------------------------
// T4. audit trail records each transition
// ---------------------------------------------------------------------------
void testAudit(Harness& h) {
    h.section("T4. audit trail records each transition");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_audit.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string stdId, itemId;
    if (!seedAndGetItem(db, stdId, itemId)) {
        h.check(false, "seed standards + get item");
        db.close();
        return;
    }
    insertResult(db, "res1", stdId, itemId, "");

    ac::WorkflowService wf(db);
    h.check(wf.submitForReview("res1", "alice").isOk(),
            "submitForReview ok");
    h.check(wf.approve("res1", "bob").isOk(), "approve ok");

    auto log = wf.auditLog("res1");
    h.check(log.isOk(), "auditLog(\"res1\") ok");
    if (log.isOk()) {
        h.check(log.value().size() >= 2,
                "audit log has at least 2 entries");
        bool hasSubmit = false;
        bool hasApprove = false;
        bool allComplete = true;
        for (const auto& e : log.value()) {
            if (e.actor.empty()) allComplete = false;
            if (e.action.empty()) allComplete = false;
            if (!isRealTimestamp(e.timestamp)) allComplete = false;
            if (e.action == "submit") hasSubmit = true;
            if (e.action == "approve") hasApprove = true;
        }
        h.check(hasSubmit, "audit log includes action \"submit\"");
        h.check(hasApprove, "audit log includes action \"approve\"");
        h.check(allComplete,
                "every entry has an actor, an action, and a real timestamp");
    }

    db.close();
    std::remove("lodestar_s2p3_audit.db");
    std::remove("lodestar_s2p3_audit.db-wal");
    std::remove("lodestar_s2p3_audit.db-shm");
}

// ---------------------------------------------------------------------------
// T5. evidence package collects evidence links
// ---------------------------------------------------------------------------
void testEvidencePackage(Harness& h) {
    h.section("T5. evidence package collects evidence links");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p3_evpkg.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string stdId, itemId;
    if (!seedAndGetItem(db, stdId, itemId)) {
        h.check(false, "seed standards + get item");
        db.close();
        return;
    }
    // A check result with 2 evidence links for this objective.
    insertResult(db, "res1", stdId, itemId,
                 "requirement:req1;test_case:tc1");

    ac::WorkflowService wf(db);
    auto pkg = wf.buildEvidencePackage(itemId);
    h.check(pkg.isOk(), "buildEvidencePackage(objectiveId) ok");
    if (pkg.isOk()) {
        h.check(pkg.value().objectiveId == itemId,
                "package objectiveId matches");
        h.check(pkg.value().links.size() == 2,
                "package contains 2 evidence links");
        bool hasReq = false;
        bool hasTc = false;
        for (const auto& l : pkg.value().links) {
            if (l.entityType == "requirement" && l.entityId == "req1")
                hasReq = true;
            if (l.entityType == "test_case" && l.entityId == "tc1")
                hasTc = true;
        }
        h.check(hasReq, "package contains {requirement, req1}");
        h.check(hasTc, "package contains {test_case, tc1}");
    }

    db.close();
    std::remove("lodestar_s2p3_evpkg.db");
    std::remove("lodestar_s2p3_evpkg.db-wal");
    std::remove("lodestar_s2p3_evpkg.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 3 AssureCheck workflow");
    std::printf("S2 PHASE 3 ASSURECHECK WORKFLOW TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testSubmit(h);
    testApprove(h);
    testReject(h);
    testAudit(h);
    testEvidencePackage(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
