// core/test/s2_phase9_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 9 (Full CI/CD) unit tests.
//
// Written by the scrum-master BEFORE the Phase 9 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions; implement the feature
// to satisfy them.
//
// Covers (docs/s2-phase9-test.md): the CI/CD test gate. The phase target links
// lodestar_common + lodestar_persistence and acts as a regression gate that
// verifies the persistence layer (the foundation every CI test target depends
// on) is healthy: migrations apply cleanly, DAO CRUD round-trips, and
// backup/restore works. This is the same lightweight self-contained harness as
// WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 9 engineer must provide (in core/persistence/):
//   Database::open / execute / beginImmediate / commit / rollback / queryScalar
//   MigrationRunner::run / currentVersion / dryRun / checksum / verify
//   RequirementDao::create / findById / update / softDelete / findAll
//   Database::backup / restore
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace p = lodestar::persistence;

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
// T1. migrations apply cleanly to the latest schema version
// ---------------------------------------------------------------------------
void testMigrations(Harness& h) {
    h.section("T1. migrations apply cleanly to the latest schema version");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p9_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }

    p::MigrationRunner runner(db);
    int version = runner.currentVersion();
    h.check(version > 0, "schema version is non-zero after migrations");
    h.check(version >= 24, "schema version is at least 24 (all migrations applied)");

    // The migration runner must report the DB is up to date (no pending files).
    auto dry = runner.dryRun(g_migrationsDir);
    h.check(dry.isOk() && !dry.value(), "dryRun reports no pending migrations");

    // verify() must confirm the applied set matches the files on disk.
    auto ver = runner.verify(g_migrationsDir);
    h.check(ver.isOk() && ver.value(), "verify() confirms applied set matches files");

    // checksum() must be stable and non-empty.
    std::string c1 = runner.checksum();
    std::string c2 = runner.checksum();
    h.check(!c1.empty(), "checksum is non-empty");
    h.check(c1 == c2, "checksum is stable across calls");

    db.close();
    cleanup("lodestar_s2p9_mig.db");
}

// ---------------------------------------------------------------------------
// T2. DAO CRUD round-trip (create / findById / update / softDelete)
// ---------------------------------------------------------------------------
void testDaoCrud(Harness& h) {
    h.section("T2. DAO CRUD round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p9_crud.db")) {
        h.check(false, "open fresh db");
        return;
    }

    p::RequirementDao dao(db);

    p::Requirement r;
    r.id = "req-ci-001";
    r.externalId = "REQ-CI-001";
    r.name = "CI gate requirement";
    r.description = "The pipeline must run all test targets as a gate.";
    r.status = "Draft";
    r.priority = "High";

    auto created = dao.create(r);
    h.check(created.isOk(), "RequirementDao::create ok");
    h.check(!r.id.empty(), "created requirement has a non-empty id");

    auto found = dao.findById(r.id);
    h.check(found.isOk() && found.value().has_value(), "findById returns the row");
    h.check(found.isOk() && found.value().has_value() &&
                found.value().value().name == "CI gate requirement",
            "findById returns the correct name");
    h.check(found.isOk() && found.value().has_value() &&
                found.value().value().externalId == "REQ-CI-001",
            "findById returns the correct externalId");

    // Update the status and confirm it persists.
    r.status = "Approved";
    auto updated = dao.update(r);
    h.check(updated.isOk(), "RequirementDao::update ok");

    auto after = dao.findById(r.id);
    h.check(after.isOk() && after.value().has_value() &&
                after.value().value().status == "Approved",
            "updated status persisted");

    // Soft-delete removes it from findAll but keeps the row.
    auto del = dao.softDelete(r.id);
    h.check(del.isOk(), "RequirementDao::softDelete ok");

    auto all = dao.findAll();
    h.check(all.isOk(), "findAll ok after soft delete");
    bool stillPresent = false;
    if (all.isOk()) {
        for (const auto& row : all.value()) {
            if (row.id == r.id) stillPresent = true;
        }
    }
    h.check(!stillPresent, "soft-deleted row no longer returned by findAll");

    db.close();
    cleanup("lodestar_s2p9_crud.db");
}

// ---------------------------------------------------------------------------
// T3. backup / restore round-trip
// ---------------------------------------------------------------------------
void testBackupRestore(Harness& h) {
    h.section("T3. backup / restore round-trip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p9_bk.db")) {
        h.check(false, "open fresh db");
        return;
    }

    // Seed a row so the backup has content.
    p::RequirementDao dao(db);
    p::Requirement r;
    r.externalId = "REQ-BK-001";
    r.name = "Backup me";
    r.description = "This row must survive a backup/restore round-trip.";
    auto created = dao.create(r);
    h.check(created.isOk(), "seed a requirement row");

    const char* snap = "lodestar_s2p9_snapshot.db";
    std::remove(snap);
    auto bk = db.backup(snap);
    h.check(bk.isOk(), "Database::backup ok");

    // Mutate the live DB after the snapshot.
    r.status = "Changed";
    auto upd = dao.update(r);
    h.check(upd.isOk(), "mutate live db after backup");

    // Restore the snapshot into a fresh DB and confirm the original row is back.
    p::Database restored;
    auto ro = restored.open("lodestar_s2p9_restored.db");
    h.check(ro.isOk(), "open restored db");
    if (ro.isOk()) {
        auto rs = restored.restore(snap);
        h.check(rs.isOk(), "Database::restore ok");

        p::RequirementDao rdao(restored);
        auto found = rdao.findByExternalId("REQ-BK-001");
        h.check(found.isOk() && found.value().has_value(),
                "restored db contains the seeded row");
        h.check(found.isOk() && found.value().has_value() &&
                    found.value().value().status == "Draft",
                "restored row has the pre-mutation status (Draft)");
    }

    restored.close();
    db.close();
    cleanup("lodestar_s2p9_bk.db");
    cleanup("lodestar_s2p9_snapshot.db");
    cleanup("lodestar_s2p9_restored.db");
}

// ---------------------------------------------------------------------------
// T4. transaction atomicity (beginImmediate / commit / rollback)
// ---------------------------------------------------------------------------
void testTransactions(Harness& h) {
    h.section("T4. transaction atomicity");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p9_tx.db")) {
        h.check(false, "open fresh db");
        return;
    }

    auto begin = db.beginImmediate();
    h.check(begin.isOk(), "beginImmediate ok");

    auto ins = db.execute(
        "INSERT INTO requirements (id, external_id, name, description, status) "
        "VALUES ('tx-1', 'REQ-TX-001', 'Tx row', 'inside transaction', 'Draft');");
    h.check(ins.isOk(), "insert inside transaction ok");

    auto commit = db.commit();
    h.check(commit.isOk(), "commit ok");

    std::string count = db.queryScalar(
        "SELECT COUNT(*) FROM requirements WHERE id='tx-1';");
    h.check(count == "1", "committed row is visible");

    // A rolled-back transaction must not persist.
    auto begin2 = db.beginImmediate();
    h.check(begin2.isOk(), "beginImmediate (2) ok");
    auto ins2 = db.execute(
        "INSERT INTO requirements (id, external_id, name, description, status) "
        "VALUES ('tx-2', 'REQ-TX-002', 'Rolled back', 'inside tx', 'Draft');");
    h.check(ins2.isOk(), "insert inside tx (2) ok");
    auto rb = db.rollback();
    h.check(rb.isOk(), "rollback ok");

    std::string count2 = db.queryScalar(
        "SELECT COUNT(*) FROM requirements WHERE id='tx-2';");
    h.check(count2 == "0", "rolled-back row is not visible");

    db.close();
    cleanup("lodestar_s2p9_tx.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 9 Full CI/CD (persistence regression gate)");
    std::printf("S2 PHASE 9 TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testMigrations(h);
    testDaoCrud(h);
    testBackupRestore(h);
    testTransactions(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
