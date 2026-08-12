// core/test/wp8_matrix_tests.cpp
// ---------------------------------------------------------------------------
// WP-8 interactive traceability matrix tests (test-first).
//
// Written by the scrum-master BEFORE the WP-8 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-8): interactive traceability matrix — search, filter,
// saved views, relationship toggling, and export.
//
// WP-8 is a Qt Widgets UI work package. Following the WP-6/WP-G precedent, this
// contract verifies the QT-INDEPENDENT wiring the Qt views consume (pure C++,
// testable without a display) and documents the UI build acceptance step.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-8 engineer must provide (in core/tracelink/UiWiringService.h,
// namespace lodestar::tracelink). Reuses MatrixViewModel (with toCsv()/toHtml())
// from WP-7.
// ---------------------------------------------------------------------------
//   // Filtering / view configuration for the interactive matrix.
//   struct MatrixViewConfig {
//       std::string search;                        // substring on name/externalId
//       std::string statusFilter;                  // "" = all, else a status
//       std::vector<std::string> hiddenRelations;  // relations to hide (toggle off)
//   };
//
//   // A saved matrix view (persisted).
//   struct SavedMatrixView {
//       std::string id;
//       std::string name;
//       MatrixViewConfig config;
//   };
//
//   class UiWiringService {
//       // ... existing refreshAll(), impact(), projectTree(), detail(),
//       //     liveCoverage(), coverageCharts() ...
//
//       // Builds the matrix honoring the config: rows filtered by search/status,
//       // and any cell whose relation is in hiddenRelations is shown as "".
//       common::Result<MatrixViewModel> matrixFiltered(const MatrixViewConfig& cfg);
//
//       // Persists a named matrix view.
//       common::Result<void> saveMatrixView(const std::string& name,
//                                           const MatrixViewConfig& cfg);
//
//       // All saved matrix views, ordered by name.
//       common::Result<std::vector<SavedMatrixView>> listMatrixViews();
//
//       // Applies a saved view and returns the filtered matrix.
//       common::Result<MatrixViewModel> applyMatrixView(const std::string& viewId);
//   };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UiWiringService.h"
#include "core/tracelink/ViewModelFactory.h"

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
// Factories (same contract as WP-1 / WP-7 / WP-G).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Approved") {
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

tl::Entity makeTc(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = extId;
    e.text = "Test body of " + extId;
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

tl::Link makeLink(tl::EntityType srcT, const std::string& srcId, tl::EntityType tgtT,
                  const std::string& tgtId, const std::string& rel) {
    tl::Link l;
    l.sourceType = srcT;
    l.sourceId = srcId;
    l.targetType = tgtT;
    l.targetId = tgtId;
    l.relation = rel;
    return l;
}

// ---------------------------------------------------------------------------
// The WP-7 fixture graph:
//   REQ-SYS (Approved)                          <-- root
//   |-- REQ-DER (Draft)  derives REQ-SYS        fully covered (DES-1 + TC-1)
//   |-- REQ-UNV (Approved) derives REQ-SYS      designed (DES-U) but unverified
//   DES-1 satisfies REQ-DER
//   TC-1  verifies   REQ-DER
//   DES-U satisfies REQ-UNV
// ---------------------------------------------------------------------------
struct Graph {
    std::string sysReqId;
    std::string derReqId;
    std::string unvReqId;
    std::string desId;
    std::string tcId;
    std::string desUId;
};

Graph buildGraph(tl::TraceLinkService& svc) {
    Graph g;
    auto sys = svc.addEntity(makeReq("REQ-SYS", "Approved"));
    auto der = svc.addEntity(makeReq("REQ-DER", "Draft"));
    auto unv = svc.addEntity(makeReq("REQ-UNV", "Approved"));
    auto des = svc.addEntity(makeDesign("DES-1"));
    auto tc = svc.addEntity(makeTc("TC-1"));
    auto desU = svc.addEntity(makeDesign("DES-U"));
    g.sysReqId = sys.value().id;
    g.derReqId = der.value().id;
    g.unvReqId = unv.value().id;
    g.desId = des.value().id;
    g.tcId = tc.value().id;
    g.desUId = desU.value().id;

    svc.addLink(makeLink(tl::EntityType::Requirement, g.derReqId,
                         tl::EntityType::Requirement, g.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Requirement, g.unvReqId,
                         tl::EntityType::Requirement, g.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, g.desId,
                         tl::EntityType::Requirement, g.derReqId, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, g.tcId,
                         tl::EntityType::Requirement, g.derReqId, "verifies"));
    svc.addLink(makeLink(tl::EntityType::Design, g.desUId,
                         tl::EntityType::Requirement, g.unvReqId, "satisfies"));
    return g;
}

bool matrixCell(const tl::MatrixViewModel& m, const std::string& requirementId,
                const std::string& columnId, std::string& out) {
    int row = -1, col = -1;
    for (int i = 0; i < m.rowCount(); ++i) {
        if (m.rows[i].requirementId == requirementId) { row = i; break; }
    }
    for (int j = 0; j < m.columnCount(); ++j) {
        if (m.columns[j].id == columnId) { col = j; break; }
    }
    if (row < 0 || col < 0) return false;
    out = m.cell(row, col);
    return true;
}

bool rowHasExtId(const tl::MatrixViewModel& m, const std::string& extId) {
    for (const auto& r : m.rows) {
        if (r.requirementExternalId == extId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// T1. matrixFiltered() filters rows by search text
// ---------------------------------------------------------------------------
void testSearch(Harness& h) {
    h.section("T1. matrixFiltered() filters rows by search text");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig cfg;
    cfg.search = "REQ-DER";
    auto m = wiring.matrixFiltered(cfg);
    h.check(m.isOk(), "matrixFiltered(search=REQ-DER) ok");
    if (m.isOk()) {
        h.check(m.value().rowCount() == 1, "only the REQ-DER row remains");
        h.check(rowHasExtId(m.value(), "REQ-DER"), "remaining row is REQ-DER");
    }

    db.close();
    std::remove("lodestar_wp8_t1.db");
    std::remove("lodestar_wp8_t1.db-wal");
    std::remove("lodestar_wp8_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. matrixFiltered() filters by status
// ---------------------------------------------------------------------------
void testStatus(Harness& h) {
    h.section("T2. matrixFiltered() filters by status");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig cfg;
    cfg.statusFilter = "Approved";
    auto m = wiring.matrixFiltered(cfg);
    h.check(m.isOk(), "matrixFiltered(status=Approved) ok");
    if (m.isOk()) {
        h.check(m.value().rowCount() == 2, "two Approved requirements remain");
        h.check(rowHasExtId(m.value(), "REQ-SYS"), "REQ-SYS remains (Approved)");
        h.check(rowHasExtId(m.value(), "REQ-UNV"), "REQ-UNV remains (Approved)");
        h.check(!rowHasExtId(m.value(), "REQ-DER"), "REQ-DER excluded (Draft)");
    }

    db.close();
    std::remove("lodestar_wp8_t2.db");
    std::remove("lodestar_wp8_t2.db-wal");
    std::remove("lodestar_wp8_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. matrixFiltered() toggles relations off
// ---------------------------------------------------------------------------
void testToggle(Harness& h) {
    h.section("T3. matrixFiltered() toggles relations off");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig cfg;
    cfg.hiddenRelations = {"verifies"};
    auto m = wiring.matrixFiltered(cfg);
    h.check(m.isOk(), "matrixFiltered(hide verifies) ok");
    if (m.isOk()) {
        std::string rel;
        h.check(matrixCell(m.value(), g.derReqId, g.tcId, rel) && rel.empty(),
                "REQ-DER x TC-1 cell is empty (verifies hidden)");
        h.check(matrixCell(m.value(), g.derReqId, g.desId, rel) && rel == "satisfies",
                "REQ-DER x DES-1 cell still shows satisfies");
    }

    db.close();
    std::remove("lodestar_wp8_t3.db");
    std::remove("lodestar_wp8_t3.db-wal");
    std::remove("lodestar_wp8_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. saveMatrixView + listMatrixViews roundtrip
// ---------------------------------------------------------------------------
void testSaveList(Harness& h) {
    h.section("T4. saveMatrixView + listMatrixViews roundtrip");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig v1;
    v1.search = "REQ-DER";
    v1.statusFilter = "Draft";
    v1.hiddenRelations = {"verifies"};

    tl::MatrixViewConfig v2;
    v2.search = "REQ-UNV";

    h.check(wiring.saveMatrixView("V1", v1).isOk(), "saveMatrixView(V1) ok");
    h.check(wiring.saveMatrixView("V2", v2).isOk(), "saveMatrixView(V2) ok");

    auto list = wiring.listMatrixViews();
    h.check(list.isOk(), "listMatrixViews() ok");
    if (list.isOk()) {
        h.check(list.value().size() == 2, "two saved views returned");
        h.check(list.value()[0].name == "V1" && list.value()[1].name == "V2",
                "views ordered by name (V1, V2)");
        h.check(list.value()[0].config.search == "REQ-DER" &&
                    list.value()[0].config.statusFilter == "Draft" &&
                    list.value()[0].config.hiddenRelations.size() == 1 &&
                    list.value()[0].config.hiddenRelations[0] == "verifies",
                "V1 config intact");
        h.check(list.value()[1].config.search == "REQ-UNV",
                "V2 config intact");
    }

    db.close();
    std::remove("lodestar_wp8_t4.db");
    std::remove("lodestar_wp8_t4.db-wal");
    std::remove("lodestar_wp8_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. applyMatrixView() restores a saved view
// ---------------------------------------------------------------------------
void testApply(Harness& h) {
    h.section("T5. applyMatrixView() restores a saved view");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig cfg;
    cfg.search = "REQ-DER";
    h.check(wiring.saveMatrixView("DER-Only", cfg).isOk(), "saveMatrixView ok");

    auto list = wiring.listMatrixViews();
    h.check(list.isOk() && list.value().size() == 1, "one saved view");
    if (!list.isOk() || list.value().empty()) {
        db.close();
        return;
    }
    const std::string viewId = list.value()[0].id;

    auto m = wiring.applyMatrixView(viewId);
    h.check(m.isOk(), "applyMatrixView(viewId) ok");
    if (m.isOk()) {
        h.check(m.value().rowCount() == 1, "applied view returns only REQ-DER row");
        h.check(rowHasExtId(m.value(), "REQ-DER"), "applied row is REQ-DER");
    }

    // Applying a nonexistent view fails cleanly.
    auto missing = wiring.applyMatrixView("does-not-exist");
    h.check(missing.failed(), "applyMatrixView on missing view fails");

    db.close();
    std::remove("lodestar_wp8_t5.db");
    std::remove("lodestar_wp8_t5.db-wal");
    std::remove("lodestar_wp8_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. Export still works on a filtered matrix
// ---------------------------------------------------------------------------
void testExport(Harness& h) {
    h.section("T6. Export still works on a filtered matrix");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp8_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    Graph g = buildGraph(svc);

    tl::MatrixViewConfig cfg;
    cfg.search = "REQ-DER";
    auto m = wiring.matrixFiltered(cfg);
    h.check(m.isOk(), "matrixFiltered(search=REQ-DER) ok");
    if (!m.isOk()) {
        db.close();
        return;
    }

    auto csv = m.value().toCsv();
    auto html = m.value().toHtml();
    h.check(csv.isOk() && !csv.value().empty(), "toCsv() succeeds on filtered matrix");
    h.check(csv.isOk() && csv.value().find("REQ-DER") != std::string::npos,
            "CSV contains the REQ-DER row");
    h.check(html.isOk() && html.value().find("<html") != std::string::npos,
            "toHtml() succeeds on filtered matrix");
    h.check(html.isOk() && html.value().find("REQ-DER") != std::string::npos,
            "HTML contains the REQ-DER row");

    db.close();
    std::remove("lodestar_wp8_t6.db");
    std::remove("lodestar_wp8_t6.db-wal");
    std::remove("lodestar_wp8_t6.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-8 interactive matrix");
    std::printf("WP-8 INTERACTIVE MATRIX TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testSearch(h);
    testStatus(h);
    testToggle(h);
    testSaveList(h);
    testApply(h);
    testExport(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
