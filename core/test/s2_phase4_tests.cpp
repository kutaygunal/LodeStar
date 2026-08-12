// core/test/s2_phase4_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 4 (AssureCheck) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 4 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase4-test.md): objective-specific semantic evidence
// evaluation. Instead of the heuristic "any row exists" PASS, each checklist
// item is evaluated based on its objective's semantic type:
//   - traceability -> a trace link must exist.
//   - verification -> a passed test run must exist.
//   - coverage     -> coverage evidence must exist.
//   - review       -> an approved review must exist.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 4 engineer must provide.
// ---------------------------------------------------------------------------
// (A) ComplianceEngine::evaluateObjective(objective, evidence) returns a status
//     based on the objective's semantic type, NOT just "any row exists".
// (B) EvidenceSnapshot carries coverage evidence and approved-review evidence
//     (coverageEvidenceIds, approvedReviewIds).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/ComplianceEngine.h"
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

// ---------------------------------------------------------------------------
// T1. traceability objective requires a trace link
// ---------------------------------------------------------------------------
void testTraceability(Harness& h) {
    h.section("T1. traceability objective requires a trace link");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p4_trace.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    ac::ComplianceEngine engine(db);

    ac::EvidenceSnapshot ev;  // no trace link
    auto r1 = engine.evaluateObjective(
        "High-level requirements are traceable to system requirements", ev);
    h.check(r1.isOk(), "evaluateObjective(traceability, no link) ok");
    if (r1.isOk()) {
        h.check(r1.value() == ac::CheckStatus::Fail,
                "traceability with NO trace link -> FAIL");
    }

    ev.traceLinkIds.push_back("link1");  // with a trace link
    auto r2 = engine.evaluateObjective(
        "High-level requirements are traceable to system requirements", ev);
    h.check(r2.isOk(), "evaluateObjective(traceability, with link) ok");
    if (r2.isOk()) {
        h.check(r2.value() == ac::CheckStatus::Pass,
                "traceability WITH a trace link -> PASS");
    }

    db.close();
    std::remove("lodestar_s2p4_trace.db");
    std::remove("lodestar_s2p4_trace.db-wal");
    std::remove("lodestar_s2p4_trace.db-shm");
}

// ---------------------------------------------------------------------------
// T2. verification objective requires a passed run
// ---------------------------------------------------------------------------
void testVerification(Harness& h) {
    h.section("T2. verification objective requires a passed run");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p4_ver.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    ac::ComplianceEngine engine(db);

    ac::EvidenceSnapshot ev;
    ev.failedRunIds.push_back("run1");  // only a failed run
    auto r1 = engine.evaluateObjective(
        "High-level requirements are tested", ev);
    h.check(r1.isOk(), "evaluateObjective(verification, failed only) ok");
    if (r1.isOk()) {
        h.check(r1.value() == ac::CheckStatus::Fail,
                "verification with ONLY a failed run -> FAIL");
    }

    ev.passedRunIds.push_back("run2");  // add a passed run
    auto r2 = engine.evaluateObjective(
        "High-level requirements are tested", ev);
    h.check(r2.isOk(), "evaluateObjective(verification, with passed) ok");
    if (r2.isOk()) {
        h.check(r2.value() == ac::CheckStatus::Pass,
                "verification WITH a passed run -> PASS");
    }

    db.close();
    std::remove("lodestar_s2p4_ver.db");
    std::remove("lodestar_s2p4_ver.db-wal");
    std::remove("lodestar_s2p4_ver.db-shm");
}

// ---------------------------------------------------------------------------
// T3. coverage objective requires coverage evidence
// ---------------------------------------------------------------------------
void testCoverage(Harness& h) {
    h.section("T3. coverage objective requires coverage evidence");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p4_cov.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    ac::ComplianceEngine engine(db);

    ac::EvidenceSnapshot ev;  // no coverage evidence
    auto r1 = engine.evaluateObjective("Statement coverage is achieved", ev);
    h.check(r1.isOk(), "evaluateObjective(coverage, none) ok");
    if (r1.isOk()) {
        h.check(r1.value() == ac::CheckStatus::Fail,
                "coverage with NO coverage evidence -> FAIL");
    }

    ev.coverageEvidenceIds.push_back("cov1");  // with coverage evidence
    auto r2 = engine.evaluateObjective("Statement coverage is achieved", ev);
    h.check(r2.isOk(), "evaluateObjective(coverage, with evidence) ok");
    if (r2.isOk()) {
        h.check(r2.value() == ac::CheckStatus::Pass,
                "coverage WITH coverage evidence -> PASS");
    }

    db.close();
    std::remove("lodestar_s2p4_cov.db");
    std::remove("lodestar_s2p4_cov.db-wal");
    std::remove("lodestar_s2p4_cov.db-shm");
}

// ---------------------------------------------------------------------------
// T4. review objective requires an approved review
// ---------------------------------------------------------------------------
void testReview(Harness& h) {
    h.section("T4. review objective requires an approved review");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p4_rev.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    ac::ComplianceEngine engine(db);

    ac::EvidenceSnapshot ev;  // no approved review
    auto r1 = engine.evaluateObjective(
        "Software plans are reviewed and approved", ev);
    h.check(r1.isOk(), "evaluateObjective(review, none) ok");
    if (r1.isOk()) {
        h.check(r1.value() == ac::CheckStatus::Fail,
                "review with NO approved review -> FAIL");
    }

    ev.approvedReviewIds.push_back("rev1");  // with an approved review
    auto r2 = engine.evaluateObjective(
        "Software plans are reviewed and approved", ev);
    h.check(r2.isOk(), "evaluateObjective(review, with approved) ok");
    if (r2.isOk()) {
        h.check(r2.value() == ac::CheckStatus::Pass,
                "review WITH an approved review -> PASS");
    }

    db.close();
    std::remove("lodestar_s2p4_rev.db");
    std::remove("lodestar_s2p4_rev.db-wal");
    std::remove("lodestar_s2p4_rev.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 4 AssureCheck semantic evidence evaluation");
    std::printf("S2 PHASE 4 ASSURECHECK SEMANTIC EVALUATION TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testTraceability(h);
    testVerification(h);
    testCoverage(h);
    testReview(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
