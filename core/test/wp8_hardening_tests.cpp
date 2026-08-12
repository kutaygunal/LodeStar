// core/test/wp8_hardening_tests.cpp
// ---------------------------------------------------------------------------
// WP-8 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-8 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-8 / section 7.2 / 7.3 / 8):
//   1. WAL mode enabled       -> PRAGMA journal_mode returns "wal"
//   2. BEGIN IMMEDIATE tx     -> a failing mutation rolls back with no partial write
//   3. Performance indexes    -> required indexes exist in sqlite_master
//   4. Perf test              -> load 10,000 entities + 50,000 links, then
//                                coverage() + impactAnalysis() + validate all
//                                complete within a generous finite budget (< 60s)
//   5. WP-8 acceptance        -> the 10k/50k load + traverse + validate completes
//
// Uses the same lightweight self-contained harness as WP-1..WP-7.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-8 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Database additions (core/persistence/Database.h):
//   common::Result<void> beginImmediate();   // BEGIN IMMEDIATE
//   common::Result<void> commit();           // COMMIT
//   common::Result<void> rollback();         // ROLLBACK
//   std::string queryScalar(const std::string& sql); // first col of first row
//
//   WAL is already enabled in Database::open; verify journal_mode == "wal".
//
// (B) Performance indexes (migration or schema) that MUST exist:
//   idx_requirements_external_id
//   idx_requirements_status
//   idx_trace_links_source    (source_type, source_id)
//   idx_trace_links_target    (target_type, target_id)
//   idx_trace_links_relation
//   idx_audit_entity          (entity_type, entity_id)
//   idx_audit_timestamp
//
//   Any failing mutation inside a BEGIN IMMEDIATE block must roll back the whole
//   transaction (no partial writes). The service layer already enforces this.
// ---------------------------------------------------------------------------

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
// (Same helper pattern as the WP-1..WP-7 test suites.)
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
// 1. WAL mode
// ---------------------------------------------------------------------------
void testWalMode(Harness& h) {
    h.section("1. WAL mode enabled");

    p::Database db;
    if (db.open("lodestar_wp8_wal.db").failed()) {
        h.check(false, "open db");
        return;
    }
    std::string mode = db.queryScalar("PRAGMA journal_mode;");
    h.check(mode == "wal", "journal_mode returns 'wal'");
    db.close();
}

// ---------------------------------------------------------------------------
// 2. BEGIN IMMEDIATE transaction rollback / commit
// ---------------------------------------------------------------------------
void testTransactions(Harness& h) {
    h.section("2. BEGIN IMMEDIATE transaction rollback + commit");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_tx.db")) {
        h.check(false, "open fresh db");
        return;
    }
    p::RequirementDao dao(db);

    // Failing mutation inside a transaction: write a row, then roll back.
    h.check(db.beginImmediate().isOk(), "beginImmediate ok");
    p::Requirement r;
    r.id = "tx-req";
    r.name = "TX-REQ";
    r.description = "rollback candidate";
    r.status = "Draft";
    h.check(dao.create(r).isOk(), "insert inside transaction ok");
    h.check(db.rollback().isOk(), "rollback ok");

    std::string count = db.queryScalar(
        "SELECT count(*) FROM requirements WHERE id = 'tx-req';");
    h.check(count == "0", "rolled-back row leaves no partial write");

    // Commit path: write a row, commit, it persists.
    h.check(db.beginImmediate().isOk(), "beginImmediate (commit path) ok");
    p::Requirement kept;
    kept.id = "kept-req";
    kept.name = "KEPT-REQ";
    kept.description = "commit candidate";
    kept.status = "Draft";
    h.check(dao.create(kept).isOk(), "insert (commit path) ok");
    h.check(db.commit().isOk(), "commit ok");
    std::string keptCount = db.queryScalar(
        "SELECT count(*) FROM requirements WHERE id = 'kept-req';");
    h.check(keptCount == "1", "committed row persists");
}

// ---------------------------------------------------------------------------
// 3. Performance indexes
// ---------------------------------------------------------------------------
void testIndexes(Harness& h) {
    h.section("3. Performance indexes exist");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_idx.db")) {
        h.check(false, "open fresh db");
        return;
    }
    const char* expected[] = {
        "idx_requirements_external_id",
        "idx_requirements_status",
        "idx_trace_links_source",
        "idx_trace_links_target",
        "idx_trace_links_relation",
        "idx_audit_entity",
        "idx_audit_timestamp",
    };
    bool allPresent = true;
    for (const char* name : expected) {
        std::string sql = "SELECT count(*) FROM sqlite_master WHERE type='index' AND name='"
                          + std::string(name) + "';";
        if (db.queryScalar(sql) != "1") allPresent = false;
    }
    h.check(allPresent, "all required performance indexes exist");
}

// ---------------------------------------------------------------------------
// 4 + 5. Perf test: 10k entities + 50k links, coverage + impact + validate
// ---------------------------------------------------------------------------
void testPerf(Harness& h) {
    h.section("4+5. Perf: 10k entities + 50k links, load + traverse + validate");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_perf.db")) {
        h.check(false, "open fresh db");
        return;
    }

    const int kEntities = 10000;
    const int kLinks = 50000;
    const double kBudgetMs = 60000.0;  // generous but finite (< 60s)

    auto start = std::chrono::steady_clock::now();

    // Bulk load inside a single BEGIN IMMEDIATE transaction (fast path).
    h.check(db.beginImmediate().isOk(), "beginImmediate for bulk load");
    p::RequirementDao reqDao(db);
    p::TraceLinkDao linkDao(db);
    for (int i = 0; i < kEntities; ++i) {
        p::Requirement r;
        r.id = "req-" + std::to_string(i);
        r.name = "REQ-" + std::to_string(i);
        r.description = "perf requirement";
        r.status = "Approved";
        if (reqDao.create(r).failed()) {
            h.check(false, "bulk entity insert");
            db.rollback();
            db.close();
            return;
        }
    }
    for (int i = 0; i < kLinks; ++i) {
        p::TraceLink l;
        l.id = "link-" + std::to_string(i);
        l.sourceType = "requirement";
        l.sourceId = "req-" + std::to_string(i % kEntities);
        l.targetType = "requirement";
        l.targetId = "req-" + std::to_string((i + 1) % kEntities);
        l.relation = "derives";
        l.status = "Active";
        if (linkDao.create(l).failed()) {
            h.check(false, "bulk link insert");
            db.rollback();
            db.close();
            return;
        }
    }
    h.check(db.commit().isOk(), "commit bulk load");

    // Verify load counts.
    std::string eCount = db.queryScalar("SELECT count(*) FROM requirements;");
    std::string lCount = db.queryScalar("SELECT count(*) FROM trace_links;");
    h.check(eCount == "10000", "10,000 entities loaded");
    h.check(lCount == "50000", "50,000 links loaded");

    // Traverse: coverage().
    tl::GraphEngine graph(db);
    auto cov = graph.coverage();
    h.check(cov.isOk() && static_cast<int>(cov.value().rows.size()) >= kEntities,
            "coverage() over 10k requirements completes");

    // Traverse: impactAnalysis() on one node.
    auto impact = graph.impactAnalysis(tl::EntityType::Requirement, "req-0");
    h.check(impact.isOk(), "impactAnalysis() completes");

    // Validate: rules engine over the whole graph.
    tl::RulesEngine rules(db);
    tl::Rule noSelf;
    noSelf.name = "NO_SELF_LINKS";
    noSelf.ruleType = "NO_SELF_LINKS";
    noSelf.severity = tl::Severity::Error;
    noSelf.enabled = true;
    rules.defineRule(noSelf);
    auto run = rules.runValidation();
    h.check(run.isOk(), "validate (rules engine) completes");

    auto end = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    h.check(elapsedMs < kBudgetMs,
            "10k/50k load + traverse + validate completes within budget");
    std::printf("  [PERF] 10k entities + 50k links: %.1f ms (budget %.0f ms)\n",
                elapsedMs, kBudgetMs);

    db.close();
    std::remove("lodestar_wp8_perf.db");
    std::remove("lodestar_wp8_perf.db-wal");
    std::remove("lodestar_wp8_perf.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-8 hardening");
    std::printf("WP-8 HARDENING TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testWalMode(h);
    testTransactions(h);
    testIndexes(h);
    testPerf(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
