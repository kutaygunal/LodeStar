// core/test/wp1_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-1 (AssureCheck) unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-1 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp1-assurecheck-task.md): standards registry + checklist data
// model for the five assurance standards (DO-178C, DO-254, ARP4754A, ARP4761,
// DO-278A) and all 136 checklist items; migration 019.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-1 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 019 (core/persistence/migrations/019_assurecheck_standards.sql)
//     creates `assurance_standards` and `assurance_checklist_items` tables.
// (B) core/assurecheck/AssureCheckService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/AssureCheckService.h"
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

// ---------------------------------------------------------------------------
// T1. Migration 019 applies
// ---------------------------------------------------------------------------
void testMigration(Harness& h) {
    h.section("T1. Migration 019 applies");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_mig.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "assurance_standards"),
            "assurance_standards table exists");
    h.check(tableExists(db, "assurance_checklist_items"),
            "assurance_checklist_items table exists");

    db.close();
    std::remove("lodestar_wp1_ac_mig.db");
    std::remove("lodestar_wp1_ac_mig.db-wal");
    std::remove("lodestar_wp1_ac_mig.db-shm");
}

// ---------------------------------------------------------------------------
// T2. seedStandards + listStandards returns the five standards
// ---------------------------------------------------------------------------
void testSeedAndList(Harness& h) {
    h.section("T2. seedStandards + listStandards returns the five standards");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_seed.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    auto seed = svc.seedStandards();
    h.check(seed.isOk(), "seedStandards() ok");

    auto list = svc.listStandards();
    h.check(list.isOk(), "listStandards() ok");
    if (!list.isOk()) {
        db.close();
        return;
    }
    h.check(list.value().size() == 5, "exactly 5 standards returned");

    // Codes present and ordered by code.
    std::vector<std::string> codes;
    for (const auto& s : list.value()) codes.push_back(s.code);
    h.check(codes == std::vector<std::string>({"ARP4754A", "ARP4761",
                                               "DO-178C", "DO-254", "DO-278A"}),
            "codes are ARP4754A, ARP4761, DO-178C, DO-254, DO-278A ordered by code");

    db.close();
    std::remove("lodestar_wp1_ac_seed.db");
    std::remove("lodestar_wp1_ac_seed.db-wal");
    std::remove("lodestar_wp1_ac_seed.db-shm");
}

// ---------------------------------------------------------------------------
// T3. seedStandards seeds all 136 items
// ---------------------------------------------------------------------------
void testTotalCount(Harness& h) {
    h.section("T3. seedStandards seeds all 136 items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_total.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    h.check(svc.seedStandards().isOk(), "seedStandards() ok");

    auto total = svc.totalItemCount();
    h.check(total.isOk(), "totalItemCount() ok");
    if (total.isOk()) {
        h.check(total.value() == 136, "totalItemCount() == 136");
    }

    db.close();
    std::remove("lodestar_wp1_ac_total.db");
    std::remove("lodestar_wp1_ac_total.db-wal");
    std::remove("lodestar_wp1_ac_total.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Per-standard item counts match the checklist doc
// ---------------------------------------------------------------------------
void testPerStandardCounts(Harness& h) {
    h.section("T4. Per-standard item counts match the checklist doc");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_counts.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    svc.seedStandards();

    struct Expect {
        const char* code;
        int count;
    };
    const Expect expects[] = {
        {"DO-178C", 82}, {"DO-254", 16}, {"ARP4754A", 16},
        {"ARP4761", 11}, {"DO-278A", 11},
    };
    int sum = 0;
    for (const auto& e : expects) {
        auto items = svc.checklistFor(e.code);
        bool ok = items.isOk() && static_cast<int>(items.value().size()) == e.count;
        h.check(ok, ("checklistFor(" + std::string(e.code) + ") == " +
                     std::to_string(e.count)).c_str());
        if (items.isOk()) sum += static_cast<int>(items.value().size());
    }
    h.check(sum == 136, "sum of per-standard counts == 136");

    db.close();
    std::remove("lodestar_wp1_ac_counts.db");
    std::remove("lodestar_wp1_ac_counts.db-wal");
    std::remove("lodestar_wp1_ac_counts.db-shm");
}

// ---------------------------------------------------------------------------
// T5. checklistFor returns non-empty ordered items with full fields
// ---------------------------------------------------------------------------
void testOrderedAndFields(Harness& h) {
    h.section("T5. checklistFor returns non-empty ordered items with full fields");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_ordered.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    svc.seedStandards();

    auto list = svc.listStandards();
    h.check(list.isOk() && list.value().size() == 5, "listStandards() ok");
    if (!list.isOk() || list.value().size() != 5) {
        db.close();
        return;
    }

    bool allNonEmpty = true;
    bool allOrdered = true;
    bool allFields = true;
    for (const auto& s : list.value()) {
        auto items = svc.checklistFor(s.code);
        if (items.failed() || items.value().empty()) {
            allNonEmpty = false;
            continue;
        }
        for (size_t i = 1; i < items.value().size(); ++i) {
            if (items.value()[i].seq <= items.value()[i - 1].seq) {
                allOrdered = false;
            }
        }
        for (const auto& it : items.value()) {
            if (it.itemCode.empty() || it.objective.empty() ||
                it.dalLevel.empty() || it.evidence.empty()) {
                allFields = false;
            }
        }
    }
    h.check(allNonEmpty, "every standard has at least one checklist item");
    h.check(allOrdered, "checklist items are ordered by seq (strictly increasing)");
    h.check(allFields, "every item has non-empty itemCode, objective, dalLevel, evidence");

    db.close();
    std::remove("lodestar_wp1_ac_ordered.db");
    std::remove("lodestar_wp1_ac_ordered.db-wal");
    std::remove("lodestar_wp1_ac_ordered.db-shm");
}

// ---------------------------------------------------------------------------
// T6. DAL applicability
// ---------------------------------------------------------------------------
void testDal(Harness& h) {
    h.section("T6. DAL applicability");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_dal.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    svc.seedStandards();

    auto a = svc.countForDal("A");
    h.check(a.isOk(), "countForDal(\"A\") ok");
    if (a.isOk()) h.check(a.value() == 136, "countForDal(\"A\") == 136");

    auto e = svc.countForDal("E");
    h.check(e.isOk(), "countForDal(\"E\") ok");
    if (e.isOk()) h.check(e.value() == 0, "countForDal(\"E\") == 0");

    db.close();
    std::remove("lodestar_wp1_ac_dal.db");
    std::remove("lodestar_wp1_ac_dal.db-wal");
    std::remove("lodestar_wp1_ac_dal.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Known-item spot checks
// ---------------------------------------------------------------------------
void testSpotChecks(Harness& h) {
    h.section("T7. Known-item spot checks");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_spot.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    svc.seedStandards();

    auto items = svc.checklistFor("DO-178C");
    h.check(items.isOk(), "checklistFor(\"DO-178C\") ok");
    if (!items.isOk()) {
        db.close();
        return;
    }

    bool foundA1_1 = false;
    bool foundA6_10 = false;
    for (const auto& it : items.value()) {
        if (it.itemCode == "A1-1") {
            foundA1_1 = (it.dalLevel == "A-D" &&
                         it.evidence ==
                             "PSAC (Plan for Software Aspects of Certification)");
        }
        if (it.itemCode == "A6-10") {
            foundA6_10 = (it.dalLevel == "A" && it.evidence == "Coverage analysis");
        }
    }
    h.check(foundA1_1,
            "A1-1 has dalLevel A-D and evidence PSAC (Plan for Software Aspects of Certification)");
    h.check(foundA6_10, "A6-10 has dalLevel A and evidence Coverage analysis");

    db.close();
    std::remove("lodestar_wp1_ac_spot.db");
    std::remove("lodestar_wp1_ac_spot.db-wal");
    std::remove("lodestar_wp1_ac_spot.db-shm");
}

// ---------------------------------------------------------------------------
// T8. Idempotent seeding
// ---------------------------------------------------------------------------
void testIdempotent(Harness& h) {
    h.section("T8. Idempotent seeding");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp1_ac_idem.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ac::AssureCheckService svc(db);
    h.check(svc.seedStandards().isOk(), "first seedStandards() ok");
    h.check(svc.seedStandards().isOk(), "second seedStandards() ok");

    auto list = svc.listStandards();
    h.check(list.isOk() && list.value().size() == 5,
            "listStandards() still returns exactly 5 standards (no duplicates)");

    auto total = svc.totalItemCount();
    h.check(total.isOk() && total.value() == 136,
            "totalItemCount() still == 136 (no duplicates)");

    db.close();
    std::remove("lodestar_wp1_ac_idem.db");
    std::remove("lodestar_wp1_ac_idem.db-wal");
    std::remove("lodestar_wp1_ac_idem.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-1 AssureCheck standards + checklist data model");
    std::printf("WP-1 ASSURECHECK STANDARDS/CHECKLIST TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigration(h);
    testSeedAndList(h);
    testTotalCount(h);
    testPerStandardCounts(h);
    testOrderedAndFields(h);
    testDal(h);
    testSpotChecks(h);
    testIdempotent(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
