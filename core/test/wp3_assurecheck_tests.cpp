// core/test/wp3_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-3 (AssureCheck) evidence + integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-3 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp3-assurecheck-task.md): EvidenceSnapshot, the
// ComplianceEngine::runChecksWithEvidence method, the EvidenceService
// (collect / runChecks / storeResults / resultsFor), and the
// TestForgeDao::listRuns addition. Wires the WP-2 ComplianceEngine to real
// TraceLink + TestForge data.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-3 engineer must provide.
// ---------------------------------------------------------------------------
// (A) EvidenceSnapshot struct in core/assurecheck/ComplianceEngine.h.
// (B) ComplianceEngine::runChecksWithEvidence(standardCode, dalLevel, evidence).
// (C) core/assurecheck/EvidenceService.h (+ .cpp).
// (D) TestForgeDao::listRuns() in core/testforge/TestForgeDao.h (+ .cpp).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/EvidenceService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/TestForgeDao.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
namespace p  = lodestar::persistence;
namespace tl = lodestar::tracelink;
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

// Seeds the five standards and returns true on success.
bool seed(p::Database& db, Harness& h) {
    ac::AssureCheckService svc(db);
    auto seed = svc.seedStandards();
    h.check(seed.isOk(), "seedStandards() ok");
    return seed.isOk();
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
// TraceLink / TestForge fixtures.
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& id, const std::string& extId) {
    tl::Entity e;
    e.id = id;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "The system shall provide GNSS position output.";
    return e;
}

tl::Entity makeDesign(const std::string& id, const std::string& extId) {
    tl::Entity e;
    e.id = id;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = extId;
    e.text = "Position solver component.";
    return e;
}

tl::Entity makeTc(const std::string& id, const std::string& extId) {
    tl::Entity e;
    e.id = id;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = extId;
    e.text = "Verify GNSS position output accuracy.";
    return e;
}

// A valid link: Design -> Requirement with relation "satisfies".
tl::Link makeLink(const std::string& srcId, const std::string& tgtId) {
    tl::Link l;
    l.sourceType = tl::EntityType::Design;
    l.sourceId = srcId;
    l.targetType = tl::EntityType::Requirement;
    l.targetId = tgtId;
    l.relation = "satisfies";
    return l;
}

tf::TestRun makeRun(const std::string& id, tf::RunStatus status) {
    tf::TestRun run;
    run.id = id;
    run.procedureId = "proc-1";
    run.procedureName = "Proc 1";
    run.scenarioId = "scen-1";
    run.status = status;
    run.startedAt = "2024-01-01T00:00:00Z";
    run.finishedAt = "2024-01-01T00:01:00Z";
    return run;
}

// ---------------------------------------------------------------------------
// T1. collect() pulls TraceLink entities
// ---------------------------------------------------------------------------
void testCollectTraceLink(Harness& h) {
    h.section("T1. collect() pulls TraceLink entities");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_tl.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tls.addEntity(makeReq("req1", "REQ-1")).isOk(), "add Requirement ok");
    h.check(tls.addEntity(makeDesign("des1", "DES-1")).isOk(), "add Design ok");
    h.check(tls.addEntity(makeTc("tc1", "TC-1")).isOk(), "add TestCase ok");
    h.check(tls.addLink(makeLink("des1", "req1")).isOk(), "add Link ok");

    auto snap = svc.collect();
    h.check(snap.isOk(), "collect() ok");
    if (snap.isOk()) {
        h.check(snap.value().requirementIds.size() == 1,
                "requirementIds.size() == 1");
        h.check(snap.value().designIds.size() == 1, "designIds.size() == 1");
        h.check(snap.value().testCaseIds.size() == 1, "testCaseIds.size() == 1");
        h.check(snap.value().traceLinkIds.size() == 1, "traceLinkIds.size() == 1");
    }

    db.close();
    std::remove("lodestar_wp3_tl.db");
    std::remove("lodestar_wp3_tl.db-wal");
    std::remove("lodestar_wp3_tl.db-shm");
}

// ---------------------------------------------------------------------------
// T2. collect() pulls TestForge test-run results
// ---------------------------------------------------------------------------
void testCollectTestForge(Harness& h) {
    h.section("T2. collect() pulls TestForge test-run results");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_tf.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tfd.saveRun(makeRun("run1", tf::RunStatus::Passed)).isOk(),
            "saveRun(Passed) ok");

    auto snap = svc.collect();
    h.check(snap.isOk(), "collect() ok");
    if (snap.isOk()) {
        h.check(snap.value().passedRunIds.size() == 1,
                "passedRunIds.size() == 1");
        h.check(snap.value().failedRunIds.empty(), "failedRunIds empty");
        h.check(snap.value().blockedRunIds.empty(), "blockedRunIds empty");
    }

    db.close();
    std::remove("lodestar_wp3_tf.db");
    std::remove("lodestar_wp3_tf.db-wal");
    std::remove("lodestar_wp3_tf.db-shm");
}

// ---------------------------------------------------------------------------
// T3. runChecks uses TraceLink evidence -> PASS for requirements/design/trace
// ---------------------------------------------------------------------------
void testRunChecksTraceLink(Harness& h) {
    h.section("T3. runChecks uses TraceLink evidence -> PASS");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_rt.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tls.addEntity(makeReq("req1", "REQ-1")).isOk(), "add Requirement ok");
    h.check(tls.addEntity(makeDesign("des1", "DES-1")).isOk(), "add Design ok");
    h.check(tls.addLink(makeLink("des1", "req1")).isOk(), "add Link ok");

    auto res = svc.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "82 results for DO-178C");
    int na = 0;
    for (const auto& r : res.value())
        if (r.status == ac::CheckStatus::Na) ++na;
    h.check(na == 0, "na == 0 (all DO-178C items apply to DAL A)");

    const ac::CheckResult* a2_1 = findResult(res.value(), "A2-1");
    const ac::CheckResult* a2_9 = findResult(res.value(), "A2-9");
    const ac::CheckResult* a2_4 = findResult(res.value(), "A2-4");
    h.check(a2_1 != nullptr && a2_1->status == ac::CheckStatus::Pass,
            "A2-1 (requirements) is Pass");
    h.check(a2_9 != nullptr && a2_9->status == ac::CheckStatus::Pass,
            "A2-9 (design) is Pass");
    h.check(a2_4 != nullptr && a2_4->status == ac::CheckStatus::Pass,
            "A2-4 (trace) is Pass");

    db.close();
    std::remove("lodestar_wp3_rt.db");
    std::remove("lodestar_wp3_rt.db-wal");
    std::remove("lodestar_wp3_rt.db-shm");
}

// ---------------------------------------------------------------------------
// T4. runChecks uses TestForge passed runs -> PASS for test items
// ---------------------------------------------------------------------------
void testRunChecksPassed(Harness& h) {
    h.section("T4. runChecks uses TestForge passed runs -> PASS");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_pass.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tfd.saveRun(makeRun("run1", tf::RunStatus::Passed)).isOk(),
            "saveRun(Passed) ok");

    auto res = svc.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a6_4 = findResult(res.value(), "A6-4");
    h.check(a6_4 != nullptr && a6_4->status == ac::CheckStatus::Pass,
            "A6-4 (test) is Pass");

    db.close();
    std::remove("lodestar_wp3_pass.db");
    std::remove("lodestar_wp3_pass.db-wal");
    std::remove("lodestar_wp3_pass.db-shm");
}

// ---------------------------------------------------------------------------
// T5. failed runs (no passed) -> WARNING for test items
// ---------------------------------------------------------------------------
void testRunChecksWarning(Harness& h) {
    h.section("T5. failed runs (no passed) -> WARNING");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_warn.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tfd.saveRun(makeRun("run1", tf::RunStatus::Failed)).isOk(),
            "saveRun(Failed) ok");

    auto res = svc.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a6_4 = findResult(res.value(), "A6-4");
    h.check(a6_4 != nullptr && a6_4->status == ac::CheckStatus::Warning,
            "A6-4 (test) is Warning");

    db.close();
    std::remove("lodestar_wp3_warn.db");
    std::remove("lodestar_wp3_warn.db-wal");
    std::remove("lodestar_wp3_warn.db-shm");
}

// ---------------------------------------------------------------------------
// T6. no test runs -> FAIL for test items
// ---------------------------------------------------------------------------
void testRunChecksFail(Harness& h) {
    h.section("T6. no test runs -> FAIL");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_fail.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    auto res = svc.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a6_4 = findResult(res.value(), "A6-4");
    h.check(a6_4 != nullptr && a6_4->status == ac::CheckStatus::Fail,
            "A6-4 (test) is Fail");

    db.close();
    std::remove("lodestar_wp3_fail.db");
    std::remove("lodestar_wp3_fail.db-wal");
    std::remove("lodestar_wp3_fail.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Evidence links populated on PASS + storeResults/resultsFor round-trip
// ---------------------------------------------------------------------------
void testEvidenceAndRoundTrip(Harness& h) {
    h.section("T7. Evidence links + storeResults/resultsFor round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_ev.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tls.addEntity(makeReq("req1", "REQ-1")).isOk(), "add Requirement ok");

    auto res = svc.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const ac::CheckResult* a2_1 = findResult(res.value(), "A2-1");
    h.check(a2_1 != nullptr, "A2-1 result present");
    if (a2_1 != nullptr) {
        bool found = false;
        for (const auto& e : a2_1->evidence) {
            if (e.entityType == "requirement" && e.entityId == "req1") {
                found = true;
            }
        }
        h.check(found,
                "A2-1 evidence contains EvidenceLink{requirement, req1}");
    }

    h.check(svc.storeResults(res.value()).isOk(), "storeResults() ok");
    auto got = svc.resultsFor("DO-178C");
    h.check(got.isOk(), "resultsFor(\"DO-178C\") ok");
    if (got.isOk()) {
        h.check(got.value().size() == 82, "resultsFor returns 82 results");
        const ac::CheckResult* stored = findResult(got.value(), "A2-1");
        h.check(stored != nullptr && stored->status == ac::CheckStatus::Pass,
                "stored A2-1 has status Pass");
    }

    db.close();
    std::remove("lodestar_wp3_ev.db");
    std::remove("lodestar_wp3_ev.db-wal");
    std::remove("lodestar_wp3_ev.db-shm");
}

// ---------------------------------------------------------------------------
// T8. DAL applicability still holds with evidence
// ---------------------------------------------------------------------------
void testDalWithEvidence(Harness& h) {
    h.section("T8. DAL applicability still holds with evidence");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_dal.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    tl::TraceLinkService tls(db);
    tf::TestForgeDao tfd(db);
    ac::EvidenceService svc(db, tls, tfd);

    h.check(tls.addEntity(makeReq("req1", "REQ-1")).isOk(), "add Requirement ok");
    h.check(tfd.saveRun(makeRun("run1", tf::RunStatus::Passed)).isOk(),
            "saveRun(Passed) ok");

    auto res = svc.runChecks("DO-178C", "E");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"E\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "82 results for DAL E");
    int na = 0;
    for (const auto& r : res.value())
        if (r.status == ac::CheckStatus::Na) ++na;
    h.check(na == 82, "all 82 results are Na for DAL E");

    db.close();
    std::remove("lodestar_wp3_dal.db");
    std::remove("lodestar_wp3_dal.db-wal");
    std::remove("lodestar_wp3_dal.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-3 AssureCheck evidence + integration");
    std::printf("WP-3 ASSURECHECK EVIDENCE + INTEGRATION TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testCollectTraceLink(h);
    testCollectTestForge(h);
    testRunChecksTraceLink(h);
    testRunChecksPassed(h);
    testRunChecksWarning(h);
    testRunChecksFail(h);
    testEvidenceAndRoundTrip(h);
    testDalWithEvidence(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
