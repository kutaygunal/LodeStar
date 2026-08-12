// core/test/s2_phase7_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 7 (Structural code coverage) unit tests.
//
// Written by the scrum-master BEFORE the Phase 7 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions; implement the feature
// to satisfy them.
//
// Covers (docs/s2-phase7-test.md): statement coverage, decision coverage,
// MC/DC coverage, and persistence of coverage results (migration 026).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 7 engineer must provide (in core/testforge/Coverage.h):
//   double computeStatementCoverage(int executed, int total);   // 0-100
//   double computeDecisionCoverage(int decisionsTaken, int decisionsTotal);
//   double computeMcdcCoverage(int conditionsSatisfied, int conditionsTotal);
//   struct CoverageResult { id; runId; scope; statementsExecuted;
//                           statementsTotal; decisionsTaken; decisionsTotal;
//                           conditionsSatisfied; conditionsTotal; recordedAt; }
//   CoverageDao::save(CoverageResult) / load(id) / list()
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/Coverage.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace p = lodestar::persistence;
namespace tf = lodestar::testforge;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

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

bool near(double a, double b, double eps = 0.0001) {
    return std::fabs(a - b) <= eps;
}

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    auto mig = runner.run(g_migrationsDir);
    return mig.isOk();
}

void cleanup(const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. statement coverage percentage
// ---------------------------------------------------------------------------
void testStatementCoverage(Harness& h) {
    h.section("T1. statement coverage percentage");

    double pct = tf::computeStatementCoverage(5, 10);
    h.check(near(pct, 50.0), "5 of 10 statements executed -> 50%");

    h.check(near(tf::computeStatementCoverage(10, 10), 100.0),
            "10 of 10 statements executed -> 100%");
    h.check(near(tf::computeStatementCoverage(0, 10), 0.0),
            "0 of 10 statements executed -> 0%");
    h.check(near(tf::computeStatementCoverage(3, 0), 0.0),
            "zero total statements -> 0% (no divide-by-zero)");
}

// ---------------------------------------------------------------------------
// T2. decision coverage percentage
// ---------------------------------------------------------------------------
void testDecisionCoverage(Harness& h) {
    h.section("T2. decision coverage percentage");

    double pct = tf::computeDecisionCoverage(3, 4);
    h.check(near(pct, 75.0), "3 of 4 decision outcomes exercised -> 75%");

    h.check(near(tf::computeDecisionCoverage(4, 4), 100.0),
            "4 of 4 decision outcomes exercised -> 100%");
    h.check(near(tf::computeDecisionCoverage(0, 4), 0.0),
            "0 of 4 decision outcomes exercised -> 0%");
    h.check(near(tf::computeDecisionCoverage(2, 0), 0.0),
            "zero total decisions -> 0% (no divide-by-zero)");
}

// ---------------------------------------------------------------------------
// T3. MC/DC coverage percentage
// ---------------------------------------------------------------------------
void testMcdcCoverage(Harness& h) {
    h.section("T3. MC/DC coverage percentage");

    double pct = tf::computeMcdcCoverage(2, 4);
    h.check(near(pct, 50.0), "2 of 4 conditions independently affecting outcome -> 50%");

    h.check(near(tf::computeMcdcCoverage(4, 4), 100.0),
            "4 of 4 conditions independently affecting outcome -> 100%");
    h.check(near(tf::computeMcdcCoverage(0, 4), 0.0),
            "0 of 4 conditions independently affecting outcome -> 0%");
    h.check(near(tf::computeMcdcCoverage(1, 0), 0.0),
            "zero total conditions -> 0% (no divide-by-zero)");
}

// ---------------------------------------------------------------------------
// T4. coverage results persist
// ---------------------------------------------------------------------------
void testCoveragePersistence(Harness& h) {
    h.section("T4. coverage results persist");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p7_cov.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }

    tf::CoverageDao dao(db);

    tf::CoverageResult r;
    r.id = "cov-001";
    r.runId = "run-42";
    r.scope = "module:core/testforge/Coverage.cpp";
    r.statementsExecuted = 5;
    r.statementsTotal = 10;
    r.decisionsTaken = 3;
    r.decisionsTotal = 4;
    r.conditionsSatisfied = 2;
    r.conditionsTotal = 4;
    r.recordedAt = "2025-01-01T00:00:00Z";

    auto saved = dao.save(r);
    h.check(saved.isOk(), "CoverageDao::save ok");

    auto loaded = dao.load(r.id);
    h.check(loaded.isOk() && loaded.value().has_value(),
            "CoverageDao::load returns the stored result");
    if (loaded.isOk() && loaded.value().has_value()) {
        const tf::CoverageResult& got = loaded.value().value();
        h.check(got.id == r.id, "retrieved id matches");
        h.check(got.runId == r.runId, "retrieved runId matches");
        h.check(got.scope == r.scope, "retrieved scope matches");
        h.check(got.statementsExecuted == r.statementsExecuted,
                "retrieved statementsExecuted matches");
        h.check(got.statementsTotal == r.statementsTotal,
                "retrieved statementsTotal matches");
        h.check(got.decisionsTaken == r.decisionsTaken,
                "retrieved decisionsTaken matches");
        h.check(got.decisionsTotal == r.decisionsTotal,
                "retrieved decisionsTotal matches");
        h.check(got.conditionsSatisfied == r.conditionsSatisfied,
                "retrieved conditionsSatisfied matches");
        h.check(got.conditionsTotal == r.conditionsTotal,
                "retrieved conditionsTotal matches");
        h.check(got.recordedAt == r.recordedAt, "retrieved recordedAt matches");

        // The retrieved value must recompute to the same percentage.
        h.check(near(tf::computeStatementCoverage(got.statementsExecuted,
                                                  got.statementsTotal),
                     50.0),
                "retrieved counts recompute to 50% statement coverage");
    }

    // A missing id returns nullopt (not an error).
    auto missing = dao.load("cov-does-not-exist");
    h.check(missing.isOk() && !missing.value().has_value(),
            "load of unknown id returns nullopt");

    // list() returns the stored result.
    auto all = dao.list();
    h.check(all.isOk(), "CoverageDao::list ok");
    bool found = false;
    if (all.isOk()) {
        for (const auto& row : all.value()) {
            if (row.id == r.id) found = true;
        }
    }
    h.check(found, "list() contains the stored result");

    db.close();
    cleanup("lodestar_s2p7_cov.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 7 Structural code coverage");
    std::printf("S2 PHASE 7 STRUCTURAL CODE COVERAGE TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testStatementCoverage(h);
    testDecisionCoverage(h);
    testMcdcCoverage(h);
    testCoveragePersistence(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
