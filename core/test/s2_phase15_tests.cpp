// core/test/s2_phase15_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 15 (AssureCheck) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 15 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase15-test.md): guided compliance templates/checklists —
// OOTB ARP4754A and DO-178C templates, a guided checklist with per-item status,
// and progress tracking. Uses migration 027 (guided_templates +
// guided_template_items).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 15 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 027 (core/persistence/migrations/027_templates.sql) creates the
//     guided_templates and guided_template_items tables.
// (B) core/assurecheck/TemplateService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/TemplateService.h"
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

// Seeds the OOTB templates and returns the DO-178C template id. Returns false
// on failure.
bool seedAndGetDo178c(p::Database& db, std::string& do178cId) {
    ac::TemplateService svc(db);
    if (svc.seedTemplates().failed()) return false;
    auto templates = svc.listTemplates();
    if (templates.failed()) return false;
    for (const auto& t : templates.value()) {
        if (t.name == "DO-178C") {
            do178cId = t.id;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// T0. Migration 027 applies (guided template tables)
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T0. Migration 027 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p15_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "guided_templates"),
            "guided_templates table exists");
    h.check(tableExists(db, "guided_template_items"),
            "guided_template_items table exists");

    db.close();
    std::remove("lodestar_s2p15_mig.db");
    std::remove("lodestar_s2p15_mig.db-wal");
    std::remove("lodestar_s2p15_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T1. ARP4754A and DO-178C templates exist
// ---------------------------------------------------------------------------
void testTemplatesExist(Harness& h) {
    h.section("T1. ARP4754A and DO-178C templates exist");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p15_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::TemplateService svc(db);
    h.check(svc.seedTemplates().isOk(), "seedTemplates() ok");

    auto templates = svc.listTemplates();
    h.check(templates.isOk(), "listTemplates() ok");
    if (templates.isOk()) {
        bool hasArp = false;
        bool hasDo = false;
        for (const auto& t : templates.value()) {
            if (t.name == "ARP4754A") hasArp = true;
            if (t.name == "DO-178C") hasDo = true;
        }
        h.check(hasArp, "listTemplates() includes ARP4754A");
        h.check(hasDo, "listTemplates() includes DO-178C");
    }

    db.close();
    std::remove("lodestar_s2p15_t1.db");
    std::remove("lodestar_s2p15_t1.db-wal");
    std::remove("lodestar_s2p15_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. template checklist is non-empty
// ---------------------------------------------------------------------------
void testChecklistNonEmpty(Harness& h) {
    h.section("T2. template checklist is non-empty");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p15_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string do178cId;
    if (!seedAndGetDo178c(db, do178cId)) {
        h.check(false, "seed templates + get DO-178C id");
        db.close();
        return;
    }
    ac::TemplateService svc(db);
    auto checklist = svc.templateChecklist(do178cId);
    h.check(checklist.isOk(), "templateChecklist(do178cId) ok");
    if (checklist.isOk()) {
        h.check(!checklist.value().empty(),
                "DO-178C checklist is non-empty");
    }

    db.close();
    std::remove("lodestar_s2p15_t2.db");
    std::remove("lodestar_s2p15_t2.db-wal");
    std::remove("lodestar_s2p15_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. checklist items have status
// ---------------------------------------------------------------------------
void testItemStatus(Harness& h) {
    h.section("T3. checklist items have status");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p15_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string do178cId;
    if (!seedAndGetDo178c(db, do178cId)) {
        h.check(false, "seed templates + get DO-178C id");
        db.close();
        return;
    }
    ac::TemplateService svc(db);
    auto checklist = svc.templateChecklist(do178cId);
    h.check(checklist.isOk(), "templateChecklist(do178cId) ok");
    if (checklist.isOk()) {
        bool allHaveStatus = true;
        bool allValid = true;
        for (const auto& item : checklist.value()) {
            if (item.status.empty()) allHaveStatus = false;
            if (item.status != "pending" && item.status != "in_progress" &&
                item.status != "complete") {
                allValid = false;
            }
        }
        h.check(allHaveStatus, "every item has a status field");
        h.check(allValid,
                "every status is pending/in_progress/complete");
    }

    db.close();
    std::remove("lodestar_s2p15_t3.db");
    std::remove("lodestar_s2p15_t3.db-wal");
    std::remove("lodestar_s2p15_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. progress reflects completed items
// ---------------------------------------------------------------------------
void testProgress(Harness& h) {
    h.section("T4. progress reflects completed items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p15_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    std::string do178cId;
    if (!seedAndGetDo178c(db, do178cId)) {
        h.check(false, "seed templates + get DO-178C id");
        db.close();
        return;
    }
    ac::TemplateService svc(db);
    auto checklist = svc.templateChecklist(do178cId);
    h.check(checklist.isOk() && !checklist.value().empty(),
            "DO-178C checklist non-empty");
    if (!checklist.isOk() || checklist.value().empty()) {
        db.close();
        return;
    }

    // Mark the first two items complete.
    const auto& items = checklist.value();
    h.check(svc.markComplete(do178cId, items[0].id).isOk(),
            "markComplete(item0) ok");
    h.check(svc.markComplete(do178cId, items[1].id).isOk(),
            "markComplete(item1) ok");

    auto progress = svc.templateProgress(do178cId);
    h.check(progress.isOk(), "templateProgress(do178cId) ok");
    if (progress.isOk()) {
        h.check(progress.value() > 0, "progress > 0 after completing items");
        h.check(progress.value() <= 100, "progress <= 100");
    }

    db.close();
    std::remove("lodestar_s2p15_t4.db");
    std::remove("lodestar_s2p15_t4.db-wal");
    std::remove("lodestar_s2p15_t4.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 15 Guided compliance templates");
    std::printf("S2 PHASE 15 GUIDED COMPLIANCE TEMPLATES TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testTemplatesExist(h);
    testChecklistNonEmpty(h);
    testItemStatus(h);
    testProgress(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
