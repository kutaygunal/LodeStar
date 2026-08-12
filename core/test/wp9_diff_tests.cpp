// core/test/wp9_diff_tests.cpp
// ---------------------------------------------------------------------------
// WP-9 Baseline visual diff + rollback tests (test-first).
//
// Written by the scrum-master BEFORE the WP-9 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-9): baseline visual compare view + per-item rollback UI.
// Builds on the existing BaselineService (diffBaseline, restoreBaseline,
// entityAtBaseline).
//
// Qt is NOT installed in this build, so the Qt view class (BaselineDiffView)
// cannot be compiled or instantiated here. This contract therefore covers the
// QT-INDEPENDENT wiring that the Qt view consumes (the visual diff rows and
// the per-item rollback it renders). This wiring is pure C++ and fully
// testable without a display. The Qt UI build itself is verified separately
// with LODESTAR_BUILD_UI=ON (see "UI build acceptance").
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-9 engineer must provide.
// ---------------------------------------------------------------------------
// (A) A Qt-independent wiring layer (core/tracelink/UiWiringService.h,
//     namespace lodestar::tracelink) extended with:
//
//   // One row of the visual compare view between two baselines.
//   struct VisualDiffRow {
//       std::string entityId;
//       std::string entityExternalId;
//       std::string kind;   // "added" | "removed" | "modified"
//       std::vector<FieldChange> fieldChanges;  // non-empty for "modified"
//   };
//
//   // Result of a per-item rollback.
//   struct RollbackResult {
//       std::string entityId;
//       std::string entityExternalId;
//       bool restored = false;
//   };
//
//   class UiWiringService {
//       // ... existing refreshAll(), impact(), projectTree(), detail(),
//       //     liveCoverage(), coverageCharts(), matrixFiltered(), ...
//
//       // Visual diff of baseline a (older) against b (newer): one row per
//       // changed entity/link, with field changes for modified items.
//       common::Result<std::vector<VisualDiffRow>> visualDiff(
//           const std::string& aId, const std::string& bId);
//
//       // Rolls a single entity back to its state in `baselineId`. Fails
//       // cleanly if the entity is missing from the baseline.
//       common::Result<RollbackResult> rollbackEntity(
//           EntityType type, const std::string& id, const std::string& baselineId);
//   };
//
// (B) The Qt view (ui/BaselineDiffView) renders the visual compare from
//     visualDiff() and calls rollbackEntity() for per-item rollback. Not
//     compiled here (Qt absent); the wiring it calls is what this contract
//     verifies.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UiWiringService.h"

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

// ---------------------------------------------------------------------------
// Factories (same contract as WP-1 / WP-4 / WP-6).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Draft") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Body of " + extId;
    e.status = status;
    e.owner = "engineer";
    e.verificationMethod = "test";
    e.safetyLevel = "Level A";
    return e;
}

tl::Entity makeDesign(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = extId;
    e.text = "Design body of " + extId;
    return e;
}

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
int countKind(const std::vector<tl::VisualDiffRow>& rows, const std::string& kind) {
    int n = 0;
    for (const auto& r : rows) {
        if (r.kind == kind) ++n;
    }
    return n;
}

const tl::VisualDiffRow* rowFor(const std::vector<tl::VisualDiffRow>& rows,
                                const std::string& entityId) {
    for (const auto& r : rows) {
        if (r.entityId == entityId) return &r;
    }
    return nullptr;
}

bool hasFieldChange(const tl::VisualDiffRow& row, const std::string& field,
                    const std::string& oldV, const std::string& newV) {
    for (const auto& f : row.fieldChanges) {
        if (f.field == field && f.oldValue == oldV && f.newValue == newV) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// T1. visualDiff() reports added/removed/modified
// ---------------------------------------------------------------------------
void testAddedRemovedModified(Harness& h) {
    h.section("T1. visualDiff() reports added/removed/modified");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp9_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);
    tl::UiWiringService wiring(db);

    // Baseline A: R1 (req), D1 (design), R2 (req).
    auto R1 = svc.addEntity(makeReq("R1"));
    auto D1 = svc.addEntity(makeDesign("D1"));
    auto R2 = svc.addEntity(makeReq("R2"));
    h.check(R1.isOk() && D1.isOk() && R2.isOk(), "add R1, D1, R2 ok");
    const std::string r2Id = R2.value().id;
    const std::string d1Id = D1.value().id;

    auto a = bl.createBaseline("A", "before");
    h.check(a.isOk(), "create baseline A ok");

    // After A: add a requirement, remove a design, modify a requirement's name.
    auto R3 = svc.addEntity(makeReq("R3"));
    h.check(R3.isOk(), "add R3 ok");
    h.check(svc.removeEntity(tl::EntityType::Design, d1Id).isOk(), "remove D1 ok");

    auto got = svc.getEntity(tl::EntityType::Requirement, r2Id);
    auto upd = *got.value();
    upd.name = "R2 renamed";
    h.check(svc.updateEntity(upd).isOk(), "modify R2 name ok");

    auto b = bl.createBaseline("B", "after");
    h.check(b.isOk(), "create baseline B ok");

    auto diff = wiring.visualDiff(a.value().id, b.value().id);
    h.check(diff.isOk(), "visualDiff(A, B) ok");
    if (!diff.isOk()) {
        db.close();
        return;
    }
    const auto& rows = diff.value();
    h.check(countKind(rows, "added") == 1, "one 'added' row");
    h.check(countKind(rows, "removed") == 1, "one 'removed' row");
    h.check(countKind(rows, "modified") == 1, "one 'modified' row");

    db.close();
    std::remove("lodestar_wp9_t1.db");
    std::remove("lodestar_wp9_t1.db-wal");
    std::remove("lodestar_wp9_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. visualDiff() includes field changes for modified items
// ---------------------------------------------------------------------------
void testFieldChanges(Harness& h) {
    h.section("T2. visualDiff() includes field changes for modified items");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp9_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("R"));
    h.check(R.isOk(), "add R ok");
    const std::string rId = R.value().id;

    auto a = bl.createBaseline("A", "before");
    h.check(a.isOk(), "create baseline A ok");

    auto got = svc.getEntity(tl::EntityType::Requirement, rId);
    auto upd = *got.value();
    upd.name = "R new name";
    h.check(svc.updateEntity(upd).isOk(), "modify R name ok");

    auto b = bl.createBaseline("B", "after");
    h.check(b.isOk(), "create baseline B ok");

    auto diff = wiring.visualDiff(a.value().id, b.value().id);
    h.check(diff.isOk(), "visualDiff(A, B) ok");
    if (!diff.isOk()) {
        db.close();
        return;
    }
    const auto& rows = diff.value();
    const tl::VisualDiffRow* row = rowFor(rows, rId);
    h.check(row != nullptr, "modified row present for R");
    if (row) {
        h.check(row->kind == "modified", "R row kind is 'modified'");
        h.check(!row->fieldChanges.empty(), "R row has non-empty fieldChanges");
        h.check(hasFieldChange(*row, "name", "R", "R new name"),
                "fieldChanges contains name old -> new");
    }

    db.close();
    std::remove("lodestar_wp9_t2.db");
    std::remove("lodestar_wp9_t2.db-wal");
    std::remove("lodestar_wp9_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. rollbackEntity() restores a single entity
// ---------------------------------------------------------------------------
void testRollbackRestores(Harness& h) {
    h.section("T3. rollbackEntity() restores a single entity");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp9_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);
    tl::UiWiringService wiring(db);

    auto R = makeReq("R");
    R.name = "original name";
    auto r = svc.addEntity(R);
    h.check(r.isOk(), "add R ok");
    const std::string rId = r.value().id;

    auto a = bl.createBaseline("A", "before");
    h.check(a.isOk(), "create baseline A ok");

    // Modify R after baseline A.
    auto got = svc.getEntity(tl::EntityType::Requirement, rId);
    auto upd = *got.value();
    upd.name = "changed name";
    h.check(svc.updateEntity(upd).isOk(), "modify R after A ok");

    auto rb = wiring.rollbackEntity(tl::EntityType::Requirement, rId, a.value().id);
    h.check(rb.isOk(), "rollbackEntity ok");
    if (rb.isOk()) {
        h.check(rb.value().restored == true, "rollback result restored == true");
        h.check(rb.value().entityId == rId, "rollback result entityId matches");
    }

    auto after = svc.getEntity(tl::EntityType::Requirement, rId);
    h.check(after.isOk() && after.value().has_value(), "getEntity after rollback ok");
    if (after.isOk() && after.value().has_value()) {
        h.check(after.value()->name == "original name",
                "R name restored to baseline-A value");
        h.check(after.value()->version == 1,
                "R version restored to baseline-A version");
    }

    db.close();
    std::remove("lodestar_wp9_t3.db");
    std::remove("lodestar_wp9_t3.db-wal");
    std::remove("lodestar_wp9_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. rollbackEntity() on a missing entity fails cleanly
// ---------------------------------------------------------------------------
void testRollbackMissing(Harness& h) {
    h.section("T4. rollbackEntity() on a missing entity fails cleanly");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp9_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);
    tl::UiWiringService wiring(db);

    auto R = svc.addEntity(makeReq("R"));
    h.check(R.isOk(), "add R ok");
    auto a = bl.createBaseline("A", "before");
    h.check(a.isOk(), "create baseline A ok");

    auto rb = wiring.rollbackEntity(tl::EntityType::Requirement, "does-not-exist",
                                    a.value().id);
    h.check(rb.failed(), "rollbackEntity on missing entity fails (not false success)");

    db.close();
    std::remove("lodestar_wp9_t4.db");
    std::remove("lodestar_wp9_t4.db-wal");
    std::remove("lodestar_wp9_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Acceptance: diff then rollback roundtrip
// ---------------------------------------------------------------------------
void testRoundtrip(Harness& h) {
    h.section("T5. Acceptance: diff then rollback roundtrip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp9_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);
    tl::UiWiringService wiring(db);

    auto R = makeReq("R");
    R.name = "original";
    auto r = svc.addEntity(R);
    h.check(r.isOk(), "add R ok");
    const std::string rId = r.value().id;

    auto a = bl.createBaseline("A", "before");
    h.check(a.isOk(), "create baseline A ok");

    auto got = svc.getEntity(tl::EntityType::Requirement, rId);
    auto upd = *got.value();
    upd.name = "changed";
    h.check(svc.updateEntity(upd).isOk(), "modify R ok");

    auto b = bl.createBaseline("B", "after");
    h.check(b.isOk(), "create baseline B ok");

    // visualDiff(A, B) shows R modified.
    auto diff = wiring.visualDiff(a.value().id, b.value().id);
    h.check(diff.isOk(), "visualDiff(A, B) ok");
    if (diff.isOk()) {
        const tl::VisualDiffRow* row = rowFor(diff.value(), rId);
        h.check(row != nullptr && row->kind == "modified",
                "visualDiff(A, B) reports R modified");
    }

    // Roll R back to baseline A.
    auto rb = wiring.rollbackEntity(tl::EntityType::Requirement, rId, a.value().id);
    h.check(rb.isOk() && rb.value().restored, "rollbackEntity(R, A) restores R");

    // Live entity now matches A.
    auto after = svc.getEntity(tl::EntityType::Requirement, rId);
    h.check(after.isOk() && after.value().has_value(), "getEntity after rollback ok");
    if (after.isOk() && after.value().has_value()) {
        h.check(after.value()->name == "original", "live R matches baseline A");
    }

    // A fresh visualDiff(A, B) still reports the change (snapshots immutable).
    auto diff2 = wiring.visualDiff(a.value().id, b.value().id);
    h.check(diff2.isOk(), "fresh visualDiff(A, B) ok");
    if (diff2.isOk()) {
        const tl::VisualDiffRow* row = rowFor(diff2.value(), rId);
        h.check(row != nullptr && row->kind == "modified",
                "fresh visualDiff(A, B) still reports R modified (snapshots immutable)");
    }

    db.close();
    std::remove("lodestar_wp9_t5.db");
    std::remove("lodestar_wp9_t5.db-wal");
    std::remove("lodestar_wp9_t5.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-9 baseline visual diff + rollback");
    std::printf("WP-9 BASELINE VISUAL DIFF + ROLLBACK TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testAddedRemovedModified(h);
    testFieldChanges(h);
    testRollbackRestores(h);
    testRollbackMissing(h);
    testRoundtrip(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
