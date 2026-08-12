// core/test/s1_phase4_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 1 Phase 4 (IntegrateHub) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 4 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s1-phase4-task.md): cross-disciplinary issue/coordination model
// backed by persistence::Database; migration 022.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 4 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 022 (core/persistence/migrations/022_integratehub.sql) creates
//     `integratehub_issues` and `integratehub_coordination` tables.
// (B) core/integratehub/IntegrateHubService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/integratehub/IntegrateHubService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ih = lodestar::integratehub;
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

// ---------------------------------------------------------------------------
// T1. createIssue + listIssues round-trip
// ---------------------------------------------------------------------------
void testCreateAndList(Harness& h) {
    h.section("T1. createIssue + listIssues round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p4_roundtrip.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    ih::IntegrateHubService svc(db);

    ih::Issue issue;
    issue.title = "GPS outage";
    issue.owner = ih::Discipline::Software;
    issue.status = "open";
    auto created = svc.createIssue(issue);
    h.check(created.isOk(), "createIssue() ok");
    h.check(created.isOk() && !created.value().empty(),
            "createIssue() returns a non-empty id");

    auto list = svc.listIssues(ih::Discipline::Software);
    h.check(list.isOk(), "listIssues(Software) ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 1, "listIssues(Software) returns 1 issue");
    if (list.value().size() == 1) {
        h.check(list.value()[0].title == "GPS outage",
                "issue title == \"GPS outage\"");
        h.check(list.value()[0].status == "open",
                "issue status == \"open\"");
        h.check(list.value()[0].owner == ih::Discipline::Software,
                "issue owner == Software");
    }

    db.close();
    std::remove("lodestar_s1p4_roundtrip.db");
    std::remove("lodestar_s1p4_roundtrip.db-wal");
    std::remove("lodestar_s1p4_roundtrip.db-shm");
}

// ---------------------------------------------------------------------------
// T2. listIssues filters by discipline
// ---------------------------------------------------------------------------
void testFilterByDiscipline(Harness& h) {
    h.section("T2. listIssues filters by discipline");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p4_filter.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::IntegrateHubService svc(db);

    ih::Issue sw;
    sw.title = "Software issue";
    sw.owner = ih::Discipline::Software;
    sw.status = "open";
    ih::Issue test;
    test.title = "Test issue";
    test.owner = ih::Discipline::Test;
    test.status = "open";
    h.check(svc.createIssue(sw).isOk(), "createIssue(Software) ok");
    h.check(svc.createIssue(test).isOk(), "createIssue(Test) ok");

    auto list = svc.listIssues(ih::Discipline::Software);
    h.check(list.isOk(), "listIssues(Software) ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 1,
            "listIssues(Software) returns only the Software issue (size 1)");
    if (list.value().size() == 1) {
        h.check(list.value()[0].title == "Software issue",
                "returned issue is the Software one");
    }

    db.close();
    std::remove("lodestar_s1p4_filter.db");
    std::remove("lodestar_s1p4_filter.db-wal");
    std::remove("lodestar_s1p4_filter.db-shm");
}

// ---------------------------------------------------------------------------
// T3. setStatus updates an issue
// ---------------------------------------------------------------------------
void testSetStatus(Harness& h) {
    h.section("T3. setStatus updates an issue");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p4_status.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::IntegrateHubService svc(db);

    ih::Issue issue;
    issue.title = "GPS outage";
    issue.owner = ih::Discipline::Software;
    issue.status = "open";
    auto created = svc.createIssue(issue);
    h.check(created.isOk(), "createIssue() ok");
    if (!created.isOk()) {
        db.close();
        return;
    }

    auto upd = svc.setStatus(created.value(), "resolved");
    h.check(upd.isOk(), "setStatus(id, \"resolved\") ok");

    auto list = svc.listIssues(ih::Discipline::Software);
    h.check(list.isOk(), "listIssues(Software) ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 1, "listIssues(Software) returns 1 issue");
    if (list.value().size() == 1) {
        h.check(list.value()[0].status == "resolved",
                "issue status == \"resolved\"");
    }

    db.close();
    std::remove("lodestar_s1p4_status.db");
    std::remove("lodestar_s1p4_status.db-wal");
    std::remove("lodestar_s1p4_status.db-shm");
}

// ---------------------------------------------------------------------------
// T4. addCoordination + coordinationFor
// ---------------------------------------------------------------------------
void testCoordination(Harness& h) {
    h.section("T4. addCoordination + coordinationFor");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p4_coord.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::IntegrateHubService svc(db);

    ih::Issue issue;
    issue.title = "GPS outage";
    issue.owner = ih::Discipline::Software;
    issue.status = "open";
    auto created = svc.createIssue(issue);
    h.check(created.isOk(), "createIssue() ok");
    if (!created.isOk()) {
        db.close();
        return;
    }
    const std::string id = created.value();

    h.check(svc.addCoordination(id, "Need RF data").isOk(),
            "addCoordination(id, \"Need RF data\") ok");
    h.check(svc.addCoordination(id, "Data ready").isOk(),
            "addCoordination(id, \"Data ready\") ok");

    auto notes = svc.coordinationFor(id);
    h.check(notes.isOk(), "coordinationFor(id) ok");
    if (!notes.isOk()) {
        db.close();
        return;
    }
    h.check(notes.value().size() == 2, "coordinationFor(id) returns 2 notes");
    if (notes.value().size() == 2) {
        h.check(notes.value()[0].note == "Need RF data",
                "first note == \"Need RF data\" (oldest first)");
        h.check(notes.value()[1].note == "Data ready",
                "second note == \"Data ready\"");
        h.check(notes.value()[0].issueId == id,
                "note issueId matches the issue");
    }

    db.close();
    std::remove("lodestar_s1p4_coord.db");
    std::remove("lodestar_s1p4_coord.db-wal");
    std::remove("lodestar_s1p4_coord.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Persistence survives reopen
// ---------------------------------------------------------------------------
void testPersistence(Harness& h) {
    h.section("T5. Persistence survives reopen");

    const char* file = "lodestar_s1p4_reopen.db";
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());

    std::string issueId;
    {
        p::Database db;
        if (!openFreshDb(db, file)) {
            h.check(false, "open fresh db");
            return;
        }
        ih::IntegrateHubService svc(db);
        ih::Issue issue;
        issue.title = "GPS outage";
        issue.owner = ih::Discipline::Software;
        issue.status = "open";
        auto created = svc.createIssue(issue);
        h.check(created.isOk(), "createIssue() ok");
        if (!created.isOk()) {
            db.close();
            return;
        }
        issueId = created.value();
        h.check(svc.addCoordination(issueId, "Need RF data").isOk(),
                "addCoordination() ok");
        db.close();
    }

    // Reopen the same DB file.
    p::Database db;
    h.check(db.open(file).isOk(), "reopen db file ok");
    if (!db.isOpen()) {
        h.check(false, "db reopened");
        return;
    }
    ih::IntegrateHubService svc(db);
    auto list = svc.listIssues(ih::Discipline::Software);
    h.check(list.isOk(), "listIssues(Software) after reopen ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 1,
            "issue still present after reopen (size 1)");
    if (list.value().size() == 1) {
        h.check(list.value()[0].title == "GPS outage",
                "reopened issue title == \"GPS outage\"");
        h.check(list.value()[0].status == "open",
                "reopened issue status == \"open\"");
    }

    auto notes = svc.coordinationFor(issueId);
    h.check(notes.isOk(), "coordinationFor(id) after reopen ok");
    if (notes.isOk()) {
        h.check(notes.value().size() == 1,
                "coordination note intact after reopen (size 1)");
        if (notes.value().size() == 1) {
            h.check(notes.value()[0].note == "Need RF data",
                    "reopened note == \"Need RF data\"");
        }
    }

    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S1 Phase 4 IntegrateHub issue/coordination model");
    std::printf("S1 PHASE 4 INTEGRATEHUB TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testCreateAndList(h);
    testFilterByDiscipline(h);
    testSetStatus(h);
    testCoordination(h);
    testPersistence(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
