// core/test/wp2_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-2 (AssureCheck) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-2 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp2-assurecheck-task.md): the ComplianceEngine (PASS/FAIL/NA/
// WARNING, DAL applicability, evidence links) and migration 020
// (assurance_checks storage).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-2 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 020 (core/persistence/migrations/020_assurecheck_checks.sql)
//     creates the `assurance_checks` table.
// (B) core/assurecheck/ComplianceEngine.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
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

bool tableExists(p::Database& db, const std::string& table) {
    auto rows = db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
        table + "';");
    return rows == "1";
}

// Seeds the five standards and returns true on success.
bool seed(p::Database& db, Harness& h) {
    ac::AssureCheckService svc(db);
    auto seed = svc.seedStandards();
    h.check(seed.isOk(), "seedStandards() ok");
    return seed.isOk();
}

// Inserts the T4 project data: one requirements row (req1) and one test_cases
// row (tc1, result_status='Passed'). No design_items, no trace_links.
void insertT4Data(p::Database& db) {
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
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
// T1. Migration 020 applies
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T1. Migration 020 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "assurance_checks"),
            "assurance_checks table exists");

    db.close();
    std::remove("lodestar_wp2_ac_mig.db");
    std::remove("lodestar_wp2_ac_mig.db-wal");
    std::remove("lodestar_wp2_ac_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Empty project data -> all applicable items FAIL
// ---------------------------------------------------------------------------
void testEmptyFails(Harness& h) {
    h.section("T2. Empty project data -> all applicable items FAIL");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_empty.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "82 results for DO-178C");
    int na = 0;
    int fail = 0;
    for (const auto& r : res.value()) {
        if (r.status == ac::CheckStatus::Na) ++na;
        if (r.status == ac::CheckStatus::Fail) ++fail;
    }
    h.check(na == 0, "na == 0 (all DO-178C items apply to DAL A)");
    h.check(fail == 82, "all 82 results are Fail on empty project data");

    db.close();
    std::remove("lodestar_wp2_ac_empty.db");
    std::remove("lodestar_wp2_ac_empty.db-wal");
    std::remove("lodestar_wp2_ac_empty.db-shm");
}

// ---------------------------------------------------------------------------
// T3. DAL applicability -> NA for out-of-range DAL
// ---------------------------------------------------------------------------
void testDalApplicability(Harness& h) {
    h.section("T3. DAL applicability -> NA for out-of-range DAL");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_dal.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    ac::ComplianceEngine engine(db);

    // DAL E: no DO-178C item applies.
    auto e = engine.runChecks("DO-178C", "E");
    h.check(e.isOk(), "runChecks(\"DO-178C\", \"E\") ok");
    if (e.isOk()) {
        h.check(e.value().size() == 82, "82 results for DAL E");
        int na = 0;
        for (const auto& r : e.value())
            if (r.status == ac::CheckStatus::Na) ++na;
        h.check(na == 82, "all 82 results are Na for DAL E");
    }

    // DAL B: exactly one NA (A6-10, DAL range "A" only), 81 Fail.
    auto b = engine.runChecks("DO-178C", "B");
    h.check(b.isOk(), "runChecks(\"DO-178C\", \"B\") ok");
    if (b.isOk()) {
        h.check(b.value().size() == 82, "82 results for DAL B");
        int na = 0;
        int fail = 0;
        for (const auto& r : b.value()) {
            if (r.status == ac::CheckStatus::Na) ++na;
            if (r.status == ac::CheckStatus::Fail) ++fail;
        }
        h.check(na == 1, "exactly 1 result is Na for DAL B");
        h.check(fail == 81, "81 results are Fail for DAL B");
        const ac::CheckResult* a6_10 = findResult(b.value(), "A6-10");
        h.check(a6_10 != nullptr && a6_10->status == ac::CheckStatus::Na,
                "A6-10 (DAL range A) is Na for DAL B");
    }

    db.close();
    std::remove("lodestar_wp2_ac_dal.db");
    std::remove("lodestar_wp2_ac_dal.db-wal");
    std::remove("lodestar_wp2_ac_dal.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Project data -> PASS/FAIL/WARNING mix
// ---------------------------------------------------------------------------
void testStatusMix(Harness& h) {
    h.section("T4. Project data -> PASS/FAIL/WARNING mix");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_mix.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT4Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(res.value().size() == 82, "82 results");
    int na = 0, pass = 0, fail = 0;
    for (const auto& r : res.value()) {
        if (r.status == ac::CheckStatus::Na) ++na;
        if (r.status == ac::CheckStatus::Pass) ++pass;
        if (r.status == ac::CheckStatus::Fail) ++fail;
    }
    h.check(na == 0, "na == 0");
    h.check(pass > 0, "pass > 0");
    h.check(fail > 0, "fail > 0");

    const ac::CheckResult* a2_1 = findResult(res.value(), "A2-1");
    const ac::CheckResult* a2_4 = findResult(res.value(), "A2-4");
    const ac::CheckResult* a2_9 = findResult(res.value(), "A2-9");
    const ac::CheckResult* a6_4 = findResult(res.value(), "A6-4");
    h.check(a2_1 != nullptr && a2_1->status == ac::CheckStatus::Pass,
            "A2-1 (HL requirements -> requirements) is Pass");
    h.check(a2_4 != nullptr && a2_4->status == ac::CheckStatus::Fail,
            "A2-4 (Traceability matrix -> trace_links) is Fail");
    h.check(a2_9 != nullptr && a2_9->status == ac::CheckStatus::Fail,
            "A2-9 (Architecture description -> design_items) is Fail");
    h.check(a6_4 != nullptr && a6_4->status == ac::CheckStatus::Pass,
            "A6-4 (Test results -> test_cases) is Pass");

    db.close();
    std::remove("lodestar_wp2_ac_mix.db");
    std::remove("lodestar_wp2_ac_mix.db-wal");
    std::remove("lodestar_wp2_ac_mix.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Evidence links populated on PASS
// ---------------------------------------------------------------------------
void testEvidenceLinks(Harness& h) {
    h.section("T5. Evidence links populated on PASS");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_ev.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT4Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
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

    db.close();
    std::remove("lodestar_wp2_ac_ev.db");
    std::remove("lodestar_wp2_ac_ev.db-wal");
    std::remove("lodestar_wp2_ac_ev.db-shm");
}

// ---------------------------------------------------------------------------
// T6. storeResults + resultsFor round-trip
// ---------------------------------------------------------------------------
void testStoreRoundTrip(Harness& h) {
    h.section("T6. storeResults + resultsFor round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_rt.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT4Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    auto store = engine.storeResults(res.value());
    h.check(store.isOk(), "storeResults() ok");

    auto got = engine.resultsFor("DO-178C");
    h.check(got.isOk(), "resultsFor(\"DO-178C\") ok");
    if (!got.isOk()) {
        db.close();
        return;
    }
    h.check(got.value().size() == 82, "resultsFor returns 82 results");
    const ac::CheckResult* a2_1 = findResult(got.value(), "A2-1");
    h.check(a2_1 != nullptr && a2_1->status == ac::CheckStatus::Pass &&
                a2_1->itemCode == "A2-1",
            "A2-1 has status Pass and itemCode A2-1");

    db.close();
    std::remove("lodestar_wp2_ac_rt.db");
    std::remove("lodestar_wp2_ac_rt.db-wal");
    std::remove("lodestar_wp2_ac_rt.db-shm");
}

// ---------------------------------------------------------------------------
// T7. summaryFor counts
// ---------------------------------------------------------------------------
void testSummary(Harness& h) {
    h.section("T7. summaryFor counts");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_sum.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT4Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(engine.storeResults(res.value()).isOk(), "storeResults() ok");

    auto sum = engine.summaryFor("DO-178C");
    h.check(sum.isOk(), "summaryFor(\"DO-178C\") ok");
    if (sum.isOk()) {
        h.check(sum.value().total == 82, "summary total == 82");
        h.check(sum.value().na == 0, "summary na == 0");
        h.check(sum.value().pass > 0, "summary pass > 0");
        h.check(sum.value().fail > 0, "summary fail > 0");
        h.check(sum.value().percent == sum.value().pass * 100 / 82,
                "summary percent == pass*100/82");
    }

    db.close();
    std::remove("lodestar_wp2_ac_sum.db");
    std::remove("lodestar_wp2_ac_sum.db-wal");
    std::remove("lodestar_wp2_ac_sum.db-shm");
}

// ---------------------------------------------------------------------------
// T8. Idempotent storage
// ---------------------------------------------------------------------------
void testIdempotent(Harness& h) {
    h.section("T8. Idempotent storage");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_ac_idem.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT4Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    h.check(engine.storeResults(res.value()).isOk(), "first storeResults() ok");
    h.check(engine.storeResults(res.value()).isOk(), "second storeResults() ok");

    auto got = engine.resultsFor("DO-178C");
    h.check(got.isOk(), "resultsFor(\"DO-178C\") ok");
    if (got.isOk()) {
        h.check(got.value().size() == 82,
                "resultsFor still returns exactly 82 results (no duplicates)");
    }

    db.close();
    std::remove("lodestar_wp2_ac_idem.db");
    std::remove("lodestar_wp2_ac_idem.db-wal");
    std::remove("lodestar_wp2_ac_idem.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-2 AssureCheck compliance engine");
    std::printf("WP-2 ASSURECHECK COMPLIANCE ENGINE TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testEmptyFails(h);
    testDalApplicability(h);
    testStatusMix(h);
    testEvidenceLinks(h);
    testStoreRoundTrip(h);
    testSummary(h);
    testIdempotent(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
