// core/test/i1_impact_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill IntegrateHub 6.1: Problem Report -> Change Request -> impact analysis.
//
// Test contract: docs/gap-fill-plan.md (Module 6.1).
//   (A) Migration 031 creates integratehub_pr / integratehub_cr /
//       integratehub_impact.
//   (B) core/integratehub/ImpactAnalysisService.h (+ .cpp) formalizes a PR
//       workflow (fields, states, approval authority), a CR model linked to it,
//       impact analysis (affected requirements/design/tests/baselines with risk
//       of unverified impact), and approval gating.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/integratehub/ImpactAnalysisService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ih = lodestar::integratehub;
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

// ---------------------------------------------------------------------------
// T1. Migration 031 + PR workflow
// ---------------------------------------------------------------------------
void testPrWorkflow(Harness& h) {
    h.section("T1. migration 031 + PR workflow");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i1_pr.db")) {
        h.check(false, "open fresh db");
        return;
    }
    h.check(tableExists(db, "integratehub_pr"), "integratehub_pr table exists");
    h.check(tableExists(db, "integratehub_cr"), "integratehub_cr table exists");
    h.check(tableExists(db, "integratehub_impact"), "integratehub_impact exists");

    ih::ImpactAnalysisService svc(db);
    ih::ProblemReport pr;
    pr.title = "GPS lock lost during approach";
    pr.severity = "high";
    pr.reportedBy = "alice";
    auto id = svc.createPr(pr);
    h.check(id.isOk(), "createPr() ok");
    if (!id.isOk()) { closeAndRemove(db, "lodestar_i1_pr.db"); return; }

    // Legal transitions.
    auto s1 = svc.transitionPr(id.value(), "under_investigation");
    h.check(s1.isOk() && s1.value().status == "under_investigation",
            "open -> under_investigation ok");
    auto s2 = svc.transitionPr(id.value(), "resolved");
    h.check(s2.isOk() && s2.value().status == "resolved",
            "under_investigation -> resolved ok");

    // Illegal transition (skip).
    auto illegal = svc.transitionPr(id.value(), "open");
    h.check(illegal.failed() &&
                illegal.errorCode() == lodestar::common::ErrorCode::IllegalTransition,
            "illegal transition rejected");

    // Closing requires an approval authority.
    auto closeNoAuth = svc.transitionPr(id.value(), "closed");
    h.check(closeNoAuth.failed(), "close rejected without approval authority");
    h.check(closeNoAuth.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "close-without-authority reports ValidationFailed");

    // Re-open with authority then close.
    auto prs = svc.listPrs();
    // Patch approval authority by re-creating with it; simplest honest path:
    // create a new PR that has authority and run it to closed.
    ih::ProblemReport pr2;
    pr2.title = "PR with authority";
    pr2.approvalAuthority = "safety-reviewer";
    auto id2 = svc.createPr(pr2);
    svc.transitionPr(id2.value(), "under_investigation");
    svc.transitionPr(id2.value(), "resolved");
    auto closeOk = svc.transitionPr(id2.value(), "closed");
    h.check(closeOk.isOk() && closeOk.value().status == "closed",
            "PR with approval authority can close");

    closeAndRemove(db, "lodestar_i1_pr.db");
}

// ---------------------------------------------------------------------------
// T2. CR creation + impact analysis on a linked entity
// ---------------------------------------------------------------------------
void testCrAndImpact(Harness& h) {
    h.section("T2. CR creation + impact analysis");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i1_cr.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService svc(db);

    ih::ChangeRequest cr;
    cr.title = "Change acquisition threshold";
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    cr.proposedChange = "Change threshold to -140 dBm";
    auto id = svc.createCr(cr);
    h.check(id.isOk(), "createCr() ok");
    if (!id.isOk()) { closeAndRemove(db, "lodestar_i1_cr.db"); return; }

    auto impact = svc.analyzeImpact(id.value());
    h.check(impact.isOk(), "analyzeImpact() ok");
    if (!impact.isOk()) { closeAndRemove(db, "lodestar_i1_cr.db"); return; }
    // Direct target is always in the impact set.
    bool hasDirect = false;
    for (const auto& it : impact.value()) {
        if (it.targetType == "requirement" && it.targetId == "REQ-100")
            hasDirect = true;
    }
    h.check(hasDirect, "direct target in the impact set");

    // No verified test run -> unverified risk flagged.
    bool unverified = false;
    for (const auto& it : impact.value()) if (it.riskUnverified) unverified = true;
    h.check(unverified, "unverified impact risk flagged (no verified test run)");

    // Approval gate blocks.
    auto gate = svc.checkApprovalGate(id.value());
    h.check(gate.failed(), "approval gate blocks unverified-impact CR");
    h.check(gate.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "blocked gate reports ValidationFailed");

    // Missing CR -> NotFound.
    auto missing = svc.analyzeImpact("no-such-cr");
    h.check(missing.failed() && missing.errorCode() == lodestar::common::ErrorCode::NotFound,
            "analyzeImpact() on missing CR reports NotFound");

    closeAndRemove(db, "lodestar_i1_cr.db");
}

// ---------------------------------------------------------------------------
// T3. Impact set correctness with trace links
// ---------------------------------------------------------------------------
void testImpactSetWithLinks(Harness& h) {
    h.section("T3. impact set includes linked items via the trace graph");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i1_links.db")) {
        h.check(false, "open fresh db");
        return;
    }
    // Insert a trace link in the graph from REQ-100 -> DESIGN-5.
    db.execute(
        "CREATE TABLE IF NOT EXISTS trace_links ("
        " id TEXT PRIMARY KEY, source_type TEXT, source_id TEXT, "
        " target_type TEXT, target_id TEXT, relation TEXT, status TEXT, "
        " rationale TEXT, created_by TEXT, created_at TEXT, updated_at TEXT, "
        " version INTEGER, superseded_by TEXT, valid_from TEXT, valid_to TEXT);");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, target_type, "
               "target_id, relation, status) VALUES "
               "('L1','requirement','REQ-100','design','DESIGN-5','derives','Active');");

    ih::ImpactAnalysisService svc(db);
    ih::ChangeRequest cr;
    cr.title = "Change requirement";
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    auto id = svc.createCr(cr);
    auto impact = svc.analyzeImpact(id.value());
    h.check(impact.isOk(), "analyzeImpact() ok");
    if (!impact.isOk()) { closeAndRemove(db, "lodestar_i1_links.db"); return; }

    bool hasLinkedDesign = false;
    for (const auto& it : impact.value()) {
        if (it.targetType == "design" && it.targetId == "DESIGN-5")
            hasLinkedDesign = true;
    }
    h.check(hasLinkedDesign,
            "impact set includes linked design item via trace graph");

    closeAndRemove(db, "lodestar_i1_links.db");
}

// ---------------------------------------------------------------------------
// T4. Approval gate passes when impact is verified
// ---------------------------------------------------------------------------
void testApprovalGatePass(Harness& h) {
    h.section("T4. approval gate passes with verified impact");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i1_ok.db")) {
        h.check(false, "open fresh db");
        return;
    }
    // A passing test run makes the impact verified (test_run_coverage).
    db.execute("CREATE TABLE IF NOT EXISTS test_run_coverage ("
               " id TEXT PRIMARY KEY, run_id TEXT NOT NULL, test_case_id TEXT NOT NULL, "
               " passed INTEGER NOT NULL DEFAULT 0, executed_at TEXT NOT NULL DEFAULT '');");
    db.execute("INSERT INTO test_run_coverage (id, run_id, test_case_id, passed, executed_at) "
               "VALUES ('RC-1','RUN-1','REQ-100',1,'now');");

    ih::ImpactAnalysisService svc(db);
    ih::ChangeRequest cr;
    cr.title = "Verified change";
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    auto id = svc.createCr(cr);

    auto impact = svc.analyzeImpact(id.value());
    h.check(impact.isOk(), "analyzeImpact() ok");
    if (!impact.isOk()) { closeAndRemove(db, "lodestar_i1_ok.db"); return; }

    bool unverified = false;
    for (const auto& it : impact.value()) if (it.riskUnverified) unverified = true;
    h.check(!unverified, "impact verified (passing test run exists)");

    auto gate = svc.checkApprovalGate(id.value());
    h.check(gate.isOk(), "approval gate passes when impact is verified");

    closeAndRemove(db, "lodestar_i1_ok.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill IntegrateHub 6.1 PR/CR/impact analysis");
    testPrWorkflow(h);
    testCrAndImpact(h);
    testImpactSetWithLinks(h);
    testApprovalGatePass(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
