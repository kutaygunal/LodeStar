// core/test/wp7_view_models_tests.cpp
// ---------------------------------------------------------------------------
// WP-7 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-7 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// The view-model layer is QT-INDEPENDENT (pure C++). It feeds the Qt views but
// has no Qt dependency, so it is unit-testable without a Qt build.
//
// Covers (docs/tracelink-plan.md, WP-7 / section 6):
//   1. MatrixViewModel    -> rows/columns/cells + CSV/HTML export
//   2. CoverageDashboardModel -> % designed/verified + rule violations
//   3. ImpactViewModel    -> affected tree + blocked transitions
//   4. GraphViewModel     -> nodes + edges
//   5. WP-7 acceptance    -> from a loaded graph all four models produce
//                            correct data and the matrix model exports
//
// Uses the same lightweight self-contained harness as WP-1..WP-6.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-7 engineer must provide (in core/tracelink/ViewModelFactory.h,
// namespace lodestar::tracelink). Reuses EntityType/Entity/Link/Violation
// (from RulesEngine.h) and TraceLinkService/RulesEngine.
// ---------------------------------------------------------------------------
//
// namespace lodestar::tracelink {
//
// // --- Matrix view: rows = requirements, columns = design + test ------------
// struct MatrixViewModel {
//     struct Column { std::string id; std::string type; std::string name; };
//     struct Row {
//         std::string requirementId;
//         std::string requirementExternalId;
//         std::vector<std::string> cellRelations;  // one per column; "" = none
//     };
//     std::vector<Column> columns;
//     std::vector<Row> rows;
//
//     int rowCount() const;     // == rows.size()
//     int columnCount() const;  // == columns.size()
//     std::string cell(int row, int col) const;    // relation or ""
//     common::Result<std::string> toCsv() const;   // matrix CSV export
//     common::Result<std::string> toHtml() const;  // matrix HTML export
// };
//
// // --- Coverage / compliance dashboard --------------------------------------
// struct CoverageDashboardModel {
//     struct Item {
//         std::string requirementId;
//         std::string requirementExternalId;
//         int percentDesigned = 0;   // has satisfies ? 100 : 0
//         int percentVerified = 0;   // has verifies   ? 100 : 0
//         int percentSatisfied = 0;  // (hasSatisfies?1:0 + hasVerifies?1:0) * 50
//     };
//     std::vector<Item> items;              // one per requirement
//     double overallPercentDesigned = 0.0;  // mean of designed
//     double overallPercentVerified = 0.0;  // mean of verified
//     int violationCount = 0;               // active rule violations
//     std::vector<Violation> violations;    // from the last validation run
// };
//
// // --- Impact view: affected tree + blocked transitions ---------------------
// struct ImpactViewModel {
//     struct Node {
//         std::string id;
//         std::string externalId;
//         std::string type;   // "requirement" | "design" | "test_case" | ...
//         int depth = 0;      // 0 = the changed node
//         bool affected = true;
//     };
//     std::vector<Node> affected;                // affected tree (flat, with depth)
//     std::vector<std::string> blockedTransitions;   // e.g. "REQ-X -> Verified"
//     std::vector<std::string> downstreamTests;      // external ids of affected tests
// };
//
// // --- Graph view: node-link diagram data ------------------------------------
// struct GraphViewModel {
//     struct Node { std::string id; std::string externalId; std::string type; };
//     struct Edge { std::string sourceId; std::string targetId; std::string relation; };
//     std::vector<Node> nodes;
//     std::vector<Edge> edges;
// };
//
// class ViewModelFactory {
// public:
//     explicit ViewModelFactory(persistence::Database& db);
//
//     common::Result<MatrixViewModel> matrix();
//     common::Result<CoverageDashboardModel> coverageDashboard();
//     common::Result<ImpactViewModel> impact(EntityType type, const std::string& id);
//     common::Result<GraphViewModel> graph();
// };
// }  // namespace lodestar::tracelink
// ---------------------------------------------------------------------------

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/ViewModelFactory.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
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
// Factories (same contract as WP-1).
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

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
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

const tl::CoverageDashboardModel::Item* dashItem(
    const std::vector<tl::CoverageDashboardModel::Item>& items, const std::string& extId) {
    for (const auto& it : items) {
        if (it.requirementExternalId == extId) return &it;
    }
    return nullptr;
}

bool impactHasExtId(const tl::ImpactViewModel& m, const std::string& extId) {
    for (const auto& n : m.affected) {
        if (n.externalId == extId) return true;
    }
    return false;
}

bool hasString(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

bool graphNodeExtId(const tl::GraphViewModel& g, const std::string& extId) {
    for (const auto& n : g.nodes) {
        if (n.externalId == extId) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 1. MatrixViewModel
// ---------------------------------------------------------------------------
void testMatrix(Harness& h) {
    h.section("1. MatrixViewModel (rows / columns / cells / export)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_matrix.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::ViewModelFactory vf(db);
    Graph g = buildGraph(svc);

    auto m = vf.matrix();
    h.check(m.isOk(), "matrix() ok");
    const tl::MatrixViewModel& mm = m.value();

    h.check(mm.rowCount() == 3, "matrix has 3 requirement rows");
    h.check(mm.columnCount() == 3, "matrix has 3 design/test columns");

    std::string rel;
    h.check(matrixCell(mm, g.derReqId, g.desId, rel) && rel == "satisfies",
            "REQ-DER x DES-1 cell = satisfies");
    h.check(matrixCell(mm, g.derReqId, g.tcId, rel) && rel == "verifies",
            "REQ-DER x TC-1 cell = verifies");
    h.check(matrixCell(mm, g.unvReqId, g.desUId, rel) && rel == "satisfies",
            "REQ-UNV x DES-U cell = satisfies");
    h.check(matrixCell(mm, g.sysReqId, g.desId, rel) && rel.empty(),
            "REQ-SYS x DES-1 cell empty");

    auto csv = mm.toCsv();
    auto html = mm.toHtml();
    h.check(csv.isOk() && !csv.value().empty(), "matrix toCsv() produces content");
    h.check(csv.isOk() && csv.value().find("REQ-SYS") != std::string::npos,
            "matrix CSV contains a requirement row");
    h.check(html.isOk() && html.value().find("<html") != std::string::npos,
            "matrix toHtml() is an HTML document");
}

// ---------------------------------------------------------------------------
// 2. CoverageDashboardModel
// ---------------------------------------------------------------------------
void testCoverageDashboard(Harness& h) {
    h.section("2. CoverageDashboardModel (% designed/verified + violations)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_dash.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine rules(db);
    tl::ViewModelFactory vf(db);
    Graph g = buildGraph(svc);

    // Produce a validation run with a violation (REQ-UNV is unverified).
    tl::Rule r;
    r.name = "REQ_MUST_BE_VERIFIED";
    r.ruleType = "REQ_MUST_BE_VERIFIED";
    r.severity = tl::Severity::Error;
    r.standards = {"ARP4754A", "DO-178C"};
    r.enabled = true;
    rules.defineRule(r);
    rules.runValidation();

    auto d = vf.coverageDashboard();
    h.check(d.isOk(), "coverageDashboard() ok");
    const tl::CoverageDashboardModel& dm = d.value();

    const tl::CoverageDashboardModel::Item* der = dashItem(dm.items, "REQ-DER");
    const tl::CoverageDashboardModel::Item* unv = dashItem(dm.items, "REQ-UNV");
    const tl::CoverageDashboardModel::Item* sys = dashItem(dm.items, "REQ-SYS");
    h.check(der != nullptr && unv != nullptr && sys != nullptr,
            "dashboard has items for all requirements");

    h.check(der->percentDesigned == 100 && der->percentVerified == 100 &&
                der->percentSatisfied == 100,
            "REQ-DER fully covered (100% designed + verified)");
    h.check(unv->percentDesigned == 100 && unv->percentVerified == 0 &&
                unv->percentSatisfied == 50,
            "REQ-UNV designed but unverified (50% satisfied)");
    h.check(sys->percentDesigned == 0 && sys->percentVerified == 0,
            "REQ-SYS 0% coverage");

    h.check(dm.violationCount >= 1, "dashboard reports at least one violation");
    bool unvViolation = false;
    for (const auto& v : dm.violations) {
        if (v.ruleType == "REQ_MUST_BE_VERIFIED" && v.entityId == g.unvReqId)
            unvViolation = true;
    }
    h.check(unvViolation, "dashboard lists the unverified-requirement violation");
}

// ---------------------------------------------------------------------------
// 3. ImpactViewModel
// ---------------------------------------------------------------------------
void testImpact(Harness& h) {
    h.section("3. ImpactViewModel (affected tree + blocked transitions)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_impact.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::ViewModelFactory vf(db);
    Graph g = buildGraph(svc);

    auto imp = vf.impact(tl::EntityType::Requirement, g.sysReqId);
    h.check(imp.isOk(), "impact() ok");
    const tl::ImpactViewModel& im = imp.value();

    h.check(impactHasExtId(im, "REQ-SYS") && impactHasExtId(im, "REQ-DER") &&
                impactHasExtId(im, "REQ-UNV") && impactHasExtId(im, "DES-1") &&
                impactHasExtId(im, "TC-1") && impactHasExtId(im, "DES-U"),
            "affected tree includes the whole branch under REQ-SYS");

    h.check(hasString(im.blockedTransitions, "REQ-UNV -> Verified"),
            "blocked transition: REQ-UNV -> Verified (no test)");
    h.check(!hasString(im.blockedTransitions, "REQ-UNV -> Implemented"),
            "REQ-UNV NOT blocked from Implemented (has design)");

    h.check(hasString(im.downstreamTests, "TC-1"),
            "downstream tests include TC-1");
}

// ---------------------------------------------------------------------------
// 4. GraphViewModel
// ---------------------------------------------------------------------------
void testGraph(Harness& h) {
    h.section("4. GraphViewModel (nodes + edges)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_graph.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::ViewModelFactory vf(db);
    Graph g = buildGraph(svc);

    auto gm = vf.graph();
    h.check(gm.isOk(), "graph() ok");
    const tl::GraphViewModel& gg = gm.value();

    h.check(gg.nodes.size() == 6, "graph has 6 nodes");
    h.check(gg.edges.size() == 5, "graph has 5 edges");
    h.check(graphNodeExtId(gg, "REQ-SYS") && graphNodeExtId(gg, "DES-1") &&
                graphNodeExtId(gg, "TC-1"),
            "graph nodes include requirements, designs, and tests");
}

// ---------------------------------------------------------------------------
// 7. WP-7 acceptance
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("7. WP-7 acceptance: all four models from a loaded graph + matrix export");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp7_accept.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine rules(db);
    tl::ViewModelFactory vf(db);
    Graph g = buildGraph(svc);

    // one validation run so the dashboard has violations
    tl::Rule r;
    r.name = "REQ_MUST_BE_VERIFIED";
    r.ruleType = "REQ_MUST_BE_VERIFIED";
    r.severity = tl::Severity::Error;
    r.standards = {"ARP4754A"};
    r.enabled = true;
    rules.defineRule(r);
    rules.runValidation();

    // matrix
    auto m = vf.matrix();
    h.check(m.isOk() && m.value().rowCount() == 3, "matrix model: 3 rows");
    std::string rel;
    h.check(matrixCell(m.value(), g.derReqId, g.tcId, rel) && rel == "verifies",
            "matrix model: correct cell");
    h.check(m.value().toCsv().isOk() && m.value().toHtml().isOk(),
            "matrix model exports CSV + HTML");

    // coverage dashboard
    auto d = vf.coverageDashboard();
    h.check(d.isOk() && d.value().violationCount >= 1,
            "coverage dashboard model: reports violations");

    // impact
    auto imp = vf.impact(tl::EntityType::Requirement, g.sysReqId);
    h.check(imp.isOk() && impactHasExtId(imp.value(), "REQ-UNV"),
            "impact model: affected tree correct");

    // graph
    auto gm = vf.graph();
    h.check(gm.isOk() && gm.value().nodes.size() == 6 && gm.value().edges.size() == 5,
            "graph model: nodes + edges correct");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-7 view models");
    std::printf("WP-7 VIEW MODEL TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testMatrix(h);
    testCoverageDashboard(h);
    testImpact(h);
    testGraph(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
