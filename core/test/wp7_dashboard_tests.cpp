// core/test/wp7_dashboard_tests.cpp
// ---------------------------------------------------------------------------
// WP-7 coverage dashboard + charts tests (test-first).
//
// Written by the scrum-master BEFORE the WP-7 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-7): live coverage dashboard (red/green gaps) +
// status/priority/coverage charts. Depends on WP-5 CoverageService (executed
// results) and WP-6 (tree/detail).
//
// WP-7 is a Qt Widgets UI work package. Following the WP-6 precedent, this
// contract verifies the QT-INDEPENDENT wiring the Qt views consume (pure C++,
// testable without a display). The Qt UI build is verified separately with
// LODESTAR_BUILD_UI=ON.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-7 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Extend core/tracelink/UiWiringService.h (namespace lodestar::tracelink):
//
//   struct LiveCoverageRow {
//       std::string requirementId;
//       std::string requirementExternalId;
//       bool designed = false;   // has >=1 Active satisfies link
//       bool verified = false;   // has >=1 Active verifies link AND a passing run
//       bool executed = false;   // has at least one recorded test run
//       bool gapNoDesign = false;  // red: no design
//       bool gapNoTest = false;    // red: no passing test
//   };
//
//   struct CoverageCharts {
//       struct Slice { std::string label; int count = 0; };
//       std::vector<Slice> byStatus;    // Draft / Approved / ... counts
//       std::vector<Slice> byPriority;  // High / Medium / Low / ... counts
//       std::vector<Slice> byCoverage;  // Full / Partial / None counts
//   };
//
//   class UiWiringService {
//       // ... existing refreshAll(), impact(), projectTree(), detail() ...
//       common::Result<std::vector<LiveCoverageRow>> liveCoverage();
//       common::Result<CoverageCharts> coverageCharts();
//   };
//
// (B) ui/CoverageDashboardView renders the live dashboard (red/green gaps)
//     from liveCoverage() and the charts from coverageCharts(). Not compiled
//     here; the wiring it calls is what this contract verifies.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/CoverageService.h"
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
// Factories (same contract as WP-1 / WP-5 / WP-6 / WP-G).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Approved",
                   const std::string& priority = "") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Body of " + extId;
    e.status = status;
    e.priority = priority;
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

// Builds requirement R + test case TC with an Active "verifies" link.
struct ReqTc {
    std::string reqId;
    std::string tcId;
};

ReqTc buildReqVerifiedByTc(tl::TraceLinkService& svc, const std::string& reqExt,
                           const std::string& tcExt) {
    ReqTc out;
    auto req = svc.addEntity(makeReq(reqExt));
    auto tc = svc.addEntity(makeTc(tcExt));
    out.reqId = req.value().id;
    out.tcId = tc.value().id;
    svc.addLink(makeLink(tl::EntityType::TestCase, out.tcId,
                         tl::EntityType::Requirement, out.reqId, "verifies"));
    return out;
}

// Finds a live coverage row by requirement id; returns nullptr if absent.
const tl::LiveCoverageRow* findRow(const std::vector<tl::LiveCoverageRow>& rows,
                                   const std::string& reqId) {
    for (const auto& r : rows) {
        if (r.requirementId == reqId) return &r;
    }
    return nullptr;
}

// Finds a chart slice by label; returns nullptr if absent.
const tl::CoverageCharts::Slice* findSlice(
    const std::vector<tl::CoverageCharts::Slice>& slices, const std::string& label) {
    for (const auto& s : slices) {
        if (s.label == label) return &s;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// T1. liveCoverage() reflects executed results
// ---------------------------------------------------------------------------
void testUnverifiedBeforeRun(Harness& h) {
    h.section("T1. liveCoverage() reflects executed results");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t1.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T1", "TC-T1");

    auto res = wiring.liveCoverage();
    h.check(res.isOk(), "liveCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::LiveCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R reported");
    if (row) {
        h.check(!row->designed, "R designed=false (no satisfies link)");
        h.check(!row->verified, "R verified=false (no passing run)");
        h.check(!row->executed, "R executed=false (no recorded run)");
        h.check(row->gapNoDesign, "R gapNoDesign=true (red: no design)");
        h.check(row->gapNoTest, "R gapNoTest=true (red: no passing test)");
    }

    db.close();
    std::remove("lodestar_wp7_t1.db");
    std::remove("lodestar_wp7_t1.db-wal");
    std::remove("lodestar_wp7_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Passing run makes requirement verified (green)
// ---------------------------------------------------------------------------
void testPassingRunVerifies(Harness& h) {
    h.section("T2. Passing run makes requirement verified (green)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t2.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    tl::UiWiringService wiring(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T2", "TC-T2");

    auto rec = cov.recordRun("run-1", rt.tcId, true);
    h.check(rec.isOk(), "recordRun(passing) ok");

    auto res = wiring.liveCoverage();
    h.check(res.isOk(), "liveCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::LiveCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R reported");
    if (row) {
        h.check(row->verified, "R verified=true (passing run)");
        h.check(row->executed, "R executed=true");
        h.check(!row->gapNoTest, "R gapNoTest=false (green: passing test)");
    }

    db.close();
    std::remove("lodestar_wp7_t2.db");
    std::remove("lodestar_wp7_t2.db-wal");
    std::remove("lodestar_wp7_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. Failed run does not verify (red)
// ---------------------------------------------------------------------------
void testFailedRunDoesNotVerify(Harness& h) {
    h.section("T3. Failed run does not verify (red)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t3.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    tl::UiWiringService wiring(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T3", "TC-T3");

    auto rec = cov.recordRun("run-2", rt.tcId, false);
    h.check(rec.isOk(), "recordRun(failing) ok");

    auto res = wiring.liveCoverage();
    h.check(res.isOk(), "liveCoverage() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const tl::LiveCoverageRow* row = findRow(res.value(), rt.reqId);
    h.check(row != nullptr, "requirement R2 reported");
    if (row) {
        h.check(row->executed, "R2 executed=true");
        h.check(!row->verified, "R2 verified=false (failing run)");
        h.check(row->gapNoTest, "R2 gapNoTest=true (red: no passing test)");
    }

    db.close();
    std::remove("lodestar_wp7_t3.db");
    std::remove("lodestar_wp7_t3.db-wal");
    std::remove("lodestar_wp7_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. coverageCharts() byStatus distribution
// ---------------------------------------------------------------------------
void testByStatus(Harness& h) {
    h.section("T4. coverageCharts() byStatus distribution");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t4.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    h.check(svc.addEntity(makeReq("R1", "Draft")).isOk(), "add Draft requirement");
    h.check(svc.addEntity(makeReq("R2", "Approved")).isOk(), "add Approved requirement");
    h.check(svc.addEntity(makeReq("R3", "Approved")).isOk(), "add Approved requirement");

    auto res = wiring.coverageCharts();
    h.check(res.isOk(), "coverageCharts() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const auto& charts = res.value();
    const tl::CoverageCharts::Slice* draft = findSlice(charts.byStatus, "Draft");
    const tl::CoverageCharts::Slice* approved = findSlice(charts.byStatus, "Approved");
    h.check(draft != nullptr, "byStatus has a Draft slice");
    h.check(approved != nullptr, "byStatus has an Approved slice");
    if (draft) h.check(draft->count == 1, "Draft count == 1");
    if (approved) h.check(approved->count == 2, "Approved count == 2");

    db.close();
    std::remove("lodestar_wp7_t4.db");
    std::remove("lodestar_wp7_t4.db-wal");
    std::remove("lodestar_wp7_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. coverageCharts() byPriority distribution
// ---------------------------------------------------------------------------
void testByPriority(Harness& h) {
    h.section("T5. coverageCharts() byPriority distribution");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t5.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::UiWiringService wiring(db);

    h.check(svc.addEntity(makeReq("R1", "Approved", "High")).isOk(), "add High requirement");
    h.check(svc.addEntity(makeReq("R2", "Approved", "High")).isOk(), "add High requirement");
    h.check(svc.addEntity(makeReq("R3", "Approved", "Medium")).isOk(), "add Medium requirement");

    auto res = wiring.coverageCharts();
    h.check(res.isOk(), "coverageCharts() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const auto& charts = res.value();
    const tl::CoverageCharts::Slice* high = findSlice(charts.byPriority, "High");
    const tl::CoverageCharts::Slice* medium = findSlice(charts.byPriority, "Medium");
    h.check(high != nullptr, "byPriority has a High slice");
    h.check(medium != nullptr, "byPriority has a Medium slice");
    if (high) h.check(high->count == 2, "High count == 2");
    if (medium) h.check(medium->count == 1, "Medium count == 1");

    db.close();
    std::remove("lodestar_wp7_t5.db");
    std::remove("lodestar_wp7_t5.db-wal");
    std::remove("lodestar_wp7_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. coverageCharts() byCoverage distribution
// ---------------------------------------------------------------------------
void testByCoverage(Harness& h) {
    h.section("T6. coverageCharts() byCoverage distribution");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t6.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    tl::UiWiringService wiring(db);

    // Fully covered: designed (satisfies) + verified (verifies + passing run).
    auto fullReq = svc.addEntity(makeReq("FULL"));
    auto fullD = svc.addEntity(makeDesign("FULL-D"));
    auto fullTc = svc.addEntity(makeTc("FULL-TC"));
    h.check(fullReq.isOk() && fullD.isOk() && fullTc.isOk(), "add full-coverage entities");
    svc.addLink(makeLink(tl::EntityType::Design, fullD.value().id,
                         tl::EntityType::Requirement, fullReq.value().id, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, fullTc.value().id,
                         tl::EntityType::Requirement, fullReq.value().id, "verifies"));
    h.check(cov.recordRun("run-full", fullTc.value().id, true).isOk(),
            "recordRun(passing) for full coverage");

    // Partial: designed only (satisfies, no verifies).
    auto partReq = svc.addEntity(makeReq("PART"));
    auto partD = svc.addEntity(makeDesign("PART-D"));
    h.check(partReq.isOk() && partD.isOk(), "add partial-coverage entities");
    svc.addLink(makeLink(tl::EntityType::Design, partD.value().id,
                         tl::EntityType::Requirement, partReq.value().id, "satisfies"));

    // None: no links at all.
    h.check(svc.addEntity(makeReq("NONE")).isOk(), "add no-coverage requirement");

    auto res = wiring.coverageCharts();
    h.check(res.isOk(), "coverageCharts() ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    const auto& charts = res.value();
    const tl::CoverageCharts::Slice* full = findSlice(charts.byCoverage, "Full");
    const tl::CoverageCharts::Slice* partial = findSlice(charts.byCoverage, "Partial");
    const tl::CoverageCharts::Slice* none = findSlice(charts.byCoverage, "None");
    h.check(full != nullptr, "byCoverage has a Full slice");
    h.check(partial != nullptr, "byCoverage has a Partial slice");
    h.check(none != nullptr, "byCoverage has a None slice");
    if (full) h.check(full->count == 1, "Full count == 1");
    if (partial) h.check(partial->count == 1, "Partial count == 1");
    if (none) h.check(none->count == 1, "None count == 1");

    db.close();
    std::remove("lodestar_wp7_t6.db");
    std::remove("lodestar_wp7_t6.db-wal");
    std::remove("lodestar_wp7_t6.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Acceptance: live change flips the dashboard
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("T7. Acceptance: live change flips the dashboard");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_t7.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::CoverageService cov(db);
    tl::UiWiringService wiring(db);
    ReqTc rt = buildReqVerifiedByTc(svc, "REQ-T7", "TC-T7");

    // Snapshot before the change: R unverified.
    auto before = wiring.liveCoverage();
    h.check(before.isOk(), "liveCoverage() before ok");
    if (before.isOk()) {
        const tl::LiveCoverageRow* row = findRow(before.value(), rt.reqId);
        h.check(row != nullptr, "R reported before");
        if (row) h.check(!row->verified, "R unverified before the run");
    }

    // Record a passing run for TC, then re-query.
    h.check(cov.recordRun("run-7", rt.tcId, true).isOk(), "recordRun(passing) ok");

    auto after = wiring.liveCoverage();
    h.check(after.isOk(), "liveCoverage() after ok");
    if (after.isOk()) {
        const tl::LiveCoverageRow* row = findRow(after.value(), rt.reqId);
        h.check(row != nullptr, "R reported after");
        if (row) {
            h.check(row->verified, "R flips to verified=true");
            h.check(!row->gapNoTest, "R gapNoTest=false (green)");
        }
    }

    // coverageCharts() byCoverage reflects the flip. R is verified but has no
    // satisfies link, so it is Partial (one of the two), not Full.
    auto charts = wiring.coverageCharts();
    h.check(charts.isOk(), "coverageCharts() after ok");
    if (charts.isOk()) {
        const tl::CoverageCharts::Slice* partial =
            findSlice(charts.value().byCoverage, "Partial");
        const tl::CoverageCharts::Slice* none =
            findSlice(charts.value().byCoverage, "None");
        h.check(partial != nullptr && partial->count == 1,
                "byCoverage Partial == 1 after the run");
        h.check(none != nullptr && none->count == 0,
                "byCoverage None == 0 after the run");
    }

    db.close();
    std::remove("lodestar_wp7_t7.db");
    std::remove("lodestar_wp7_t7.db-wal");
    std::remove("lodestar_wp7_t7.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-7 coverage dashboard + charts");
    std::printf("WP-7 COVERAGE DASHBOARD + CHARTS TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testUnverifiedBeforeRun(h);
    testPassingRunVerifies(h);
    testFailedRunDoesNotVerify(h);
    testByStatus(h);
    testByPriority(h);
    testByCoverage(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
