// core/test/wp5_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-5 (AssureCheck) performance + hardening tests (test-first).
//
// Written by the scrum-master BEFORE the WP-5 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp5-assurecheck-task.md): migration 021 (performance indexes),
// the PerformanceService (batched transactional evaluation + incremental
// re-check), WAL mode, and a 10k+ entity performance budget.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-5 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 021 (core/persistence/migrations/021_assurecheck_perf.sql)
//     adds idx_assurance_checks_standard and idx_assurance_items_code.
// (B) core/assurecheck/PerformanceService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/PerformanceService.h"
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

bool indexExists(p::Database& db, const std::string& name) {
    auto rows = db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='" +
        name + "';");
    return rows == "1";
}

// Seeds the five standards and returns true on success.
bool seed(p::Database& db, Harness& h) {
    ac::AssureCheckService svc(db);
    auto seed = svc.seedStandards();
    h.check(seed.isOk(), "seedStandards() ok");
    return seed.isOk();
}

// Inserts the T3 project data: one requirements row, one design_items row,
// one test_cases row (result_status='Passed'), and one trace_links row.
void insertT3Data(p::Database& db) {
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO design_items (id, name) VALUES ('des1', 'Des 1');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id) "
               "VALUES ('tl1', 'requirement', 'req1', 'design', 'des1');");
}

// Returns the result for the given itemCode, or nullptr if absent.
const ac::CheckResult* findResult(const std::vector<ac::CheckResult>& results,
                                 const std::string& itemCode) {
    for (const auto& r : results) {
        if (r.itemCode == itemCode) return &r;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// T1. WAL mode enabled
// ---------------------------------------------------------------------------
void testWalMode(Harness& h) {
    h.section("T1. WAL mode enabled");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_wal.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    std::string mode = db.queryScalar("PRAGMA journal_mode;");
    h.check(mode == "wal", "PRAGMA journal_mode returns \"wal\"");

    db.close();
    std::remove("lodestar_wp5_ac_wal.db");
    std::remove("lodestar_wp5_ac_wal.db-wal");
    std::remove("lodestar_wp5_ac_wal.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Performance indexes exist
// ---------------------------------------------------------------------------
void testIndexes(Harness& h) {
    h.section("T2. Performance indexes exist");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_idx.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(indexExists(db, "idx_assurance_checks_standard"),
            "idx_assurance_checks_standard exists");
    h.check(indexExists(db, "idx_assurance_items_code"),
            "idx_assurance_items_code exists");
    h.check(indexExists(db, "idx_assurance_checks_item"),
            "idx_assurance_checks_item exists");
    h.check(indexExists(db, "idx_assurance_checks_status"),
            "idx_assurance_checks_status exists");
    h.check(indexExists(db, "idx_assurance_items_standard"),
            "idx_assurance_items_standard exists");

    db.close();
    std::remove("lodestar_wp5_ac_idx.db");
    std::remove("lodestar_wp5_ac_idx.db-wal");
    std::remove("lodestar_wp5_ac_idx.db-shm");
}

// ---------------------------------------------------------------------------
// T3. Batched evaluation returns + stores 82 results
// ---------------------------------------------------------------------------
void testBatched(Harness& h) {
    h.section("T3. Batched evaluation returns + stores 82 results");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_batch.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT3Data(db);

    ac::PerformanceService perf(db);
    auto res = perf.evaluateBatched("DO-178C", "A");
    h.check(res.isOk(), "evaluateBatched(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "evaluateBatched returns 82 results");

    ac::ComplianceEngine engine(db);
    auto got = engine.resultsFor("DO-178C");
    h.check(got.isOk(), "resultsFor(\"DO-178C\") ok");
    if (got.isOk()) {
        h.check(got.value().size() == 82,
                "resultsFor(\"DO-178C\") returns 82 results");
        const ac::CheckResult* a2_1 = findResult(got.value(), "A2-1");
        h.check(a2_1 != nullptr && a2_1->status == ac::CheckStatus::Pass,
                "A2-1 has status Pass");
    }

    db.close();
    std::remove("lodestar_wp5_ac_batch.db");
    std::remove("lodestar_wp5_ac_batch.db-wal");
    std::remove("lodestar_wp5_ac_batch.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Incremental re-check returns only requirement-source items
// ---------------------------------------------------------------------------
void testIncrementalRequirement(Harness& h) {
    h.section("T4. Incremental re-check returns only requirement-source items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_incr_req.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT3Data(db);

    ac::PerformanceService perf(db);
    auto batched = perf.evaluateBatched("DO-178C", "A");
    h.check(batched.isOk(), "evaluateBatched(\"DO-178C\", \"A\") ok");
    if (!batched.isOk()) {
        db.close();
        return;
    }

    auto subset = perf.recheckIncremental("DO-178C", "A", {"requirement"});
    h.check(subset.isOk(), "recheckIncremental({\"requirement\"}) ok");
    if (!subset.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a2_1 = findResult(subset.value(), "A2-1");
    const ac::CheckResult* a6_4 = findResult(subset.value(), "A6-4");
    h.check(a2_1 != nullptr, "subset contains A2-1 (requirements)");
    h.check(a6_4 == nullptr, "subset does NOT contain A6-4 (test)");

    db.close();
    std::remove("lodestar_wp5_ac_incr_req.db");
    std::remove("lodestar_wp5_ac_incr_req.db-wal");
    std::remove("lodestar_wp5_ac_incr_req.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Incremental re-check returns only test-source items
// ---------------------------------------------------------------------------
void testIncrementalTest(Harness& h) {
    h.section("T5. Incremental re-check returns only test-source items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_incr_test.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT3Data(db);

    ac::PerformanceService perf(db);
    auto batched = perf.evaluateBatched("DO-178C", "A");
    h.check(batched.isOk(), "evaluateBatched(\"DO-178C\", \"A\") ok");
    if (!batched.isOk()) {
        db.close();
        return;
    }

    auto subset = perf.recheckIncremental("DO-178C", "A", {"test_run"});
    h.check(subset.isOk(), "recheckIncremental({\"test_run\"}) ok");
    if (!subset.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a6_4 = findResult(subset.value(), "A6-4");
    const ac::CheckResult* a2_1 = findResult(subset.value(), "A2-1");
    h.check(a6_4 != nullptr, "subset contains A6-4 (test)");
    h.check(a2_1 == nullptr, "subset does NOT contain A2-1 (requirements)");

    db.close();
    std::remove("lodestar_wp5_ac_incr_test.db");
    std::remove("lodestar_wp5_ac_incr_test.db-wal");
    std::remove("lodestar_wp5_ac_incr_test.db-shm");
}

// ---------------------------------------------------------------------------
// T6. 10k scale: batched evaluation completes within budget
// ---------------------------------------------------------------------------
void testScale(Harness& h) {
    h.section("T6. 10k scale: batched evaluation completes within budget");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_ac_scale.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }

    // Load 10k requirements + 10k trace_links (+ one design + one passed test)
    // inside a single BEGIN IMMEDIATE transaction.
    auto begin = db.beginImmediate();
    h.check(begin.isOk(), "BEGIN IMMEDIATE ok");
    if (begin.failed()) {
        db.close();
        return;
    }
    for (int i = 0; i < 10000; ++i) {
        std::string rid = "req" + std::to_string(i);
        db.execute("INSERT INTO requirements (id, name) VALUES ('" + rid +
                   "', 'Req " + std::to_string(i) + "');");
        std::string tlid = "tl" + std::to_string(i);
        db.execute("INSERT INTO trace_links (id, source_type, source_id, "
                   "target_type, target_id) VALUES ('" + tlid +
                   "', 'requirement', '" + rid + "', 'design', 'des1');");
    }
    db.execute("INSERT INTO design_items (id, name) VALUES ('des1', 'Des 1');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    auto commit = db.commit();
    h.check(commit.isOk(), "COMMIT ok");
    if (commit.failed()) {
        db.close();
        return;
    }

    auto t0 = std::chrono::steady_clock::now();
    ac::PerformanceService perf(db);
    auto res = perf.evaluateBatched("DO-178C", "A");
    auto t1 = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                       .count();

    h.check(res.isOk(), "evaluateBatched(\"DO-178C\", \"A\") ok at 10k scale");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "returns 82 results at 10k scale");
    bool allPass = true;
    for (const auto& r : res.value()) {
        if (r.status != ac::CheckStatus::Pass) {
            allPass = false;
            break;
        }
    }
    h.check(allPass, "every result status == Pass at 10k scale");
    h.check(ms <= 60000, "load + evaluate completes within 60,000 ms budget");

    db.close();
    std::remove("lodestar_wp5_ac_scale.db");
    std::remove("lodestar_wp5_ac_scale.db-wal");
    std::remove("lodestar_wp5_ac_scale.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-5 AssureCheck performance + hardening");
    std::printf("WP-5 ASSURECHECK PERFORMANCE + HARDENING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testWalMode(h);
    testIndexes(h);
    testBatched(h);
    testIncrementalRequirement(h);
    testIncrementalTest(h);
    testScale(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
