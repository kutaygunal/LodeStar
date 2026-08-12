// core/test/wp5_coverage_tests.cpp
// ---------------------------------------------------------------------------
// WP-5 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-5 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-5): wire TestForge test runs into live coverage so that
// coverage reflects EXECUTED results (a requirement is `verified` only when a
// verifying test case has a passing recorded run).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-5 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 017 (core/persistence/migrations/017_*.sql) records which
//     TestForge test run executed a given traceability test case, so live
//     coverage reflects executed results. Append-only and idempotent.
// (B) core/tracelink/CoverageService.h (+ .cpp):
//     struct ExecutedCoverageRow { requirementId, requirementExternalId,
//         designed, verified, executed };
//     class CoverageService {
//         explicit CoverageService(persistence::Database& db);
//         common::Result<void> recordRun(runId, testCaseId, passed);
//         common::Result<std::vector<ExecutedCoverageRow>> executedCoverage();
//     };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/testforge/TestForgeDao.h"
#include "core/testforge/TestRunner.h"
#include "core/tracelink/CoverageService.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;
namespace tf = lodestar::testforge;

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
// Factories (same contract as WP-1 / WP-7).
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

// Builds requirement R + test case TC with an Active "verifies" link.
// Returns the requirement id and test case id.
struct ReqTc {
    std::string reqId;
    std::string tcId;
};

ReqTc buildReqVerifiedByTc(tl::TraceLinkService& svc, const std::string& reqExt,
                           const std::string& tcExt) {
    ReqTc out;
    auto req = svc.addEntity(makeReq(reqExt));
    auto tc = svc.addEntity(makeTc(tcExt));
    out.reqId = req.value().id;
    out.tcId = tc.value().id;
    svc.addLink(makeLink(tl::EntityType::TestCase, out.tcId,
                         tl::EntityType::Requirement, out.reqId, "verifies"));
    return out;
}

// Finds a coverage row by requirement id; returns nullptr if absent.
const tl::ExecutedCoverageRow* findRow(
    const std::vector<tl::ExecutedCoverageRow>& rows, const std::string& reqId) {
    for (const auto& r : rows) {
        if (r.requirementId == reqId) return &r;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// T1. Migration applies (including 017).
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T1. Migration applies (including 017)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t1.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(true, "migrations succeed (including 017)");

    // The mapping table must exist after migrations.
    std::string table = db.queryScalar(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='test_run_coverage';");
    h.check(table == "test_run_coverage", "test_run_coverage table exists");

    db.close();
    std::remove("lodestar_wp5_t1.db");
    std::remove("lodestar_wp5_t1.db-wal");
    std::remove("lodestar_wp5_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Unverified before any run.
// ---------------------------------------------------------------------------
void testUnverifiedBeforeRun(Harness& h) {
    h.section("T2. Unverified before any run");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T2", "TC-T2");

    auto res = cov.executedCoverage();
    h.check(res.isOk(), "executedCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::ExecutedCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R reported");
    if (row) {
        h.check(!row->designed, "R designed=false (no satisfies link)");
        h.check(!row->verified, "R verified=false (no passing run)");
        h.check(!row->executed, "R executed=false (no recorded run)");
    }

    db.close();
    std::remove("lodestar_wp5_t2.db");
    std::remove("lodestar_wp5_t2.db-wal");
    std::remove("lodestar_wp5_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. Passing run makes requirement verified.
// ---------------------------------------------------------------------------
void testPassingRunVerifies(Harness& h) {
    h.section("T3. Passing run makes requirement verified");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T3", "TC-T3");

    auto rec = cov.recordRun("run-1", rt.tcId, true);
    h.check(rec.isOk(), "recordRun(passing) ok");

    auto res = cov.executedCoverage();
    h.check(res.isOk(), "executedCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::ExecutedCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R reported");
    if (row) {
        h.check(row->executed, "R executed=true");
        h.check(row->verified, "R verified=true (passing run)");
        h.check(!row->designed, "R designed=false (no satisfies link)");
    }

    db.close();
    std::remove("lodestar_wp5_t3.db");
    std::remove("lodestar_wp5_t3.db-wal");
    std::remove("lodestar_wp5_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Failed run does not verify.
// ---------------------------------------------------------------------------
void testFailedRunDoesNotVerify(Harness& h) {
    h.section("T4. Failed run does not verify");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T4", "TC-T4");

    auto rec = cov.recordRun("run-2", rt.tcId, false);
    h.check(rec.isOk(), "recordRun(failing) ok");

    auto res = cov.executedCoverage();
    h.check(res.isOk(), "executedCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::ExecutedCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R2 reported");
    if (row) {
        h.check(row->executed, "R2 executed=true");
        h.check(!row->verified, "R2 verified=false (failing run)");
    }

    db.close();
    std::remove("lodestar_wp5_t4.db");
    std::remove("lodestar_wp5_t4.db-wal");
    std::remove("lodestar_wp5_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Coverage reflects executed results (latest run governs).
// ---------------------------------------------------------------------------
void testLiveCoverage(Harness& h) {
    h.section("T5. Coverage reflects executed results (latest run governs)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T5", "TC-T5");

    // Passing run first.
    h.check(cov.recordRun("run-pass", rt.tcId, true).isOk(), "recordRun(passing) ok");
    auto res1 = cov.executedCoverage();
    h.check(res1.isOk(), "executedCoverage() after passing ok");
    if (res1.isOk()) {
        const tl::ExecutedCoverageRow* row = findRow(res1.value(), rt.reqId);
        h.check(row && row->verified, "R verified after passing run");
        h.check(row && row->executed, "R executed after passing run");
    }

    // Later failing run for the SAME test case.
    h.check(cov.recordRun("run-fail", rt.tcId, false).isOk(), "recordRun(failing) ok");
    auto res2 = cov.executedCoverage();
    h.check(res2.isOk(), "executedCoverage() after failing ok");
    if (res2.isOk()) {
        const tl::ExecutedCoverageRow* row = findRow(res2.value(), rt.reqId);
        h.check(row && !row->verified, "R no longer verified (latest result governs)");
        h.check(row && row->executed, "R executed stays true");
    }

    db.close();
    std::remove("lodestar_wp5_t5.db");
    std::remove("lodestar_wp5_t5.db-wal");
    std::remove("lodestar_wp5_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. End-to-end with TestRunner.
// ---------------------------------------------------------------------------
void testEndToEnd(Harness& h) {
    h.section("T6. End-to-end with TestRunner");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    tf::TestForgeDao dao(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T6", "TC-T6");

    // A procedure with one step measuring "position_accuracy_m" (expected 1.0,
    // tolerance 0.1).
    tf::TestProcedure proc;
    proc.id = "proc-1";
    proc.name = "Accuracy check";
    proc.version = "1.0";
    proc.objective = "Verify position accuracy";
    proc.scenarioId = "scn-1";
    tf::TestStep step;
    step.id = "step-1";
    step.seq = 1;
    step.name = "Position accuracy";
    step.metric = "position_accuracy_m";
    step.expectedValue = 1.0;
    step.tolerance = 0.1;
    proc.steps.push_back(step);

    // Passing run: provider returns a value within tolerance.
    tf::MockMeasurementProvider provider;
    provider.set("position_accuracy_m", 1.02);
    tf::TestRunner runner(provider);
    auto passRun = runner.run(proc, "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z");
    h.check(passRun.isOk(), "TestRunner produced a run");
    if (!passRun.isOk()) {
        db.close();
        return;
    }
    h.check(passRun.value().status == tf::RunStatus::Passed,
            "passing run status == Passed");
    h.check(dao.saveRun(passRun.value()).isOk(), "saveRun(passing) ok");
    h.check(cov.recordRun(passRun.value().id, rt.tcId,
                          passRun.value().status == tf::RunStatus::Passed)
                .isOk(),
            "recordRun from passing run status ok");

    auto res1 = cov.executedCoverage();
    h.check(res1.isOk(), "executedCoverage() after passing run ok");
    if (res1.isOk()) {
        const tl::ExecutedCoverageRow* row = findRow(res1.value(), rt.reqId);
        h.check(row && row->verified, "passing run verifies the linked requirement");
        h.check(row && row->executed, "requirement executed=true");
    }

    // Failing run: provider returns a value outside tolerance.
    provider.set("position_accuracy_m", 5.0);
    auto failRun = runner.run(proc, "2024-01-01T00:01:00Z", "2024-01-01T00:01:01Z");
    h.check(failRun.isOk(), "TestRunner produced a failing run");
    if (failRun.isOk()) {
        h.check(failRun.value().status == tf::RunStatus::Failed,
                "failing run status == Failed");
        h.check(dao.saveRun(failRun.value()).isOk(), "saveRun(failing) ok");
        h.check(cov.recordRun(failRun.value().id, rt.tcId,
                              failRun.value().status == tf::RunStatus::Passed)
                    .isOk(),
                "recordRun from failing run status ok");
    }

    auto res2 = cov.executedCoverage();
    h.check(res2.isOk(), "executedCoverage() after failing run ok");
    if (res2.isOk()) {
        const tl::ExecutedCoverageRow* row = findRow(res2.value(), rt.reqId);
        h.check(row && !row->verified, "failing run does not verify the requirement");
        h.check(row && row->executed, "requirement executed stays true");
    }

    db.close();
    std::remove("lodestar_wp5_t6.db");
    std::remove("lodestar_wp5_t6.db-wal");
    std::remove("lodestar_wp5_t6.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-5 TestForge coverage wiring");
    std::printf("WP-5 TESTFORGE COVERAGE WIRING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testUnverifiedBeforeRun(h);
    testPassingRunVerifies(h);
    testFailedRunDoesNotVerify(h);
    testLiveCoverage(h);
    testEndToEnd(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
