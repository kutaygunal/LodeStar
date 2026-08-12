// core/test/wp3_compliance_tests.cpp
// ---------------------------------------------------------------------------
// WP-3 (Phase 10) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-3 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-3): guided OOTB compliance templates/checklists for
// ARP4754A / ARP4761 / DO-178C / DO-254; migration 015.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-3 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 015 (core/persistence/migrations/015_*.sql) creates
//     `compliance_templates` and `compliance_checklist_items` tables.
// (B) core/tracelink/ComplianceService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/ComplianceService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
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
// T1. Migration 015 applies
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T1. Migration 015 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "compliance_templates"),
            "compliance_templates table exists");
    h.check(tableExists(db, "compliance_checklist_items"),
            "compliance_checklist_items table exists");

    db.close();
    std::remove("lodestar_wp3_mig.db");
    std::remove("lodestar_wp3_mig.db-wal");
    std::remove("lodestar_wp3_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T2. seedTemplates + listTemplates returns the four OOTB templates
// ---------------------------------------------------------------------------
void testSeedAndList(Harness& h) {
    h.section("T2. seedTemplates + listTemplates returns the four OOTB templates");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_seed.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ComplianceService svc(db);
    auto seed = svc.seedTemplates();
    h.check(seed.isOk(), "seedTemplates() ok");

    auto list = svc.listTemplates();
    h.check(list.isOk(), "listTemplates() ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 4, "exactly 4 templates returned");

    // Names present and ordered by name.
    std::vector<std::string> names;
    for (const auto& t : list.value()) names.push_back(t.name);
    h.check(names == std::vector<std::string>({"ARP4754A", "ARP4761", "DO-178C",
                                               "DO-254"}),
            "names are ARP4754A, ARP4761, DO-178C, DO-254 ordered by name");

    db.close();
    std::remove("lodestar_wp3_seed.db");
    std::remove("lodestar_wp3_seed.db-wal");
    std::remove("lodestar_wp3_seed.db-shm");
}

// ---------------------------------------------------------------------------
// T3. getTemplate + checklistFor returns non-empty ordered items
// ---------------------------------------------------------------------------
void testGetAndChecklist(Harness& h) {
    h.section("T3. getTemplate + checklistFor returns non-empty ordered items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_check.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ComplianceService svc(db);
    svc.seedTemplates();

    auto list = svc.listTemplates();
    h.check(list.isOk() && list.value().size() == 4, "listTemplates() ok");
    if (!list.isOk() || list.value().size() != 4) {
        db.close();
        return;
    }

    bool allNonEmpty = true;
    bool allOrdered = true;
    for (const auto& t : list.value()) {
        auto got = svc.getTemplate(t.id);
        h.check(got.isOk() && got.value().has_value() &&
                    got.value()->name == t.name,
                ("getTemplate(" + t.name + ") returns the template").c_str());

        auto items = svc.checklistFor(t.id);
        if (items.failed() || items.value().empty()) {
            allNonEmpty = false;
            continue;
        }
        for (size_t i = 1; i < items.value().size(); ++i) {
            if (items.value()[i].seq <= items.value()[i - 1].seq) {
                allOrdered = false;
            }
        }
    }
    h.check(allNonEmpty, "every OOTB template has at least one checklist item");
    h.check(allOrdered, "checklist items are ordered by seq (strictly increasing)");

    // getTemplate on a missing id returns nullopt.
    auto missing = svc.getTemplate("does-not-exist");
    h.check(missing.isOk() && !missing.value().has_value(),
            "getTemplate(missing) returns nullopt");

    db.close();
    std::remove("lodestar_wp3_check.db");
    std::remove("lodestar_wp3_check.db-wal");
    std::remove("lodestar_wp3_check.db-shm");
}

// ---------------------------------------------------------------------------
// T4. setChecked + complianceStatus reflects progress
// ---------------------------------------------------------------------------
void testProgress(Harness& h) {
    h.section("T4. setChecked + complianceStatus reflects progress");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_prog.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ComplianceService svc(db);
    svc.seedTemplates();

    auto list = svc.listTemplates();
    h.check(list.isOk() && !list.value().empty(), "listTemplates() ok");
    if (!list.isOk() || list.value().empty()) {
        db.close();
        return;
    }

    const std::string tid = list.value().front().id;
    auto items = svc.checklistFor(tid);
    h.check(items.isOk() && !items.value().empty(), "checklistFor() non-empty");
    if (!items.isOk() || items.value().empty()) {
        db.close();
        return;
    }

    const int N = static_cast<int>(items.value().size());
    const int M = N / 2;  // check the first M items
    for (int i = 0; i < M; ++i) {
        auto res = svc.setChecked(items.value()[i].id, true);
        h.check(res.isOk(), "setChecked(item, true) ok");
    }

    auto st = svc.complianceStatus(tid);
    h.check(st.isOk(), "complianceStatus() ok");
    if (st.isOk()) {
        h.check(st.value().total == N, "total == N");
        h.check(st.value().checked == M, "checked == M");
        h.check(st.value().percent == M * 100 / N,
                "percent == M*100/N");
    }

    // Unchecking one item reduces progress.
    if (M > 0) {
        svc.setChecked(items.value()[0].id, false);
        auto st2 = svc.complianceStatus(tid);
        h.check(st2.isOk() && st2.value().checked == M - 1,
                "unchecking an item reduces checked count");
    }

    db.close();
    std::remove("lodestar_wp3_prog.db");
    std::remove("lodestar_wp3_prog.db-wal");
    std::remove("lodestar_wp3_prog.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Idempotent seeding
// ---------------------------------------------------------------------------
void testIdempotent(Harness& h) {
    h.section("T5. Idempotent seeding");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_idem.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::ComplianceService svc(db);
    h.check(svc.seedTemplates().isOk(), "first seedTemplates() ok");
    h.check(svc.seedTemplates().isOk(), "second seedTemplates() ok");

    auto list = svc.listTemplates();
    h.check(list.isOk() && list.value().size() == 4,
            "listTemplates() still returns exactly 4 templates (no duplicates)");

    db.close();
    std::remove("lodestar_wp3_idem.db");
    std::remove("lodestar_wp3_idem.db-wal");
    std::remove("lodestar_wp3_idem.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-3 compliance templates/checklists");
    std::printf("WP-3 COMPLIANCE TEMPLATES/CHECKLISTS TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testSeedAndList(h);
    testGetAndChecklist(h);
    testProgress(h);
    testIdempotent(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
