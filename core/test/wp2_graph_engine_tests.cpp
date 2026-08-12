// core/test/wp2_graph_engine_tests.cpp
// ---------------------------------------------------------------------------
// WP-2 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-2 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-2 / section 4.3):
//   1. Reverse-relation mapping
//   2. upstream / downstream closure (incl. depth limit)
//   3. impactAnalysis (affected entities + links, blocked transitions,
//      downstream test cases)
//   4. coverage() math (% designed / verified / satisfied)
//   5. coverageGap()
//   6. traceMatrix()
//   7. WP-2 acceptance: a 3-level graph (system req -> derived req -> design
//      -> test) yields correct coverage and a correct impact set when a leaf
//      (the test) changes.
//
// Uses the same lightweight self-contained harness as WP-1. Each check reports
// [PASS]/[FAIL]; the process exits non-zero if any check fails.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-2 engineer must provide (in core/tracelink/GraphEngine.h,
// namespace lodestar::tracelink). Reuses WP-1 types:
//   enum class EntityType { Requirement, Design, Interface, TestCase };
//   struct Entity { id, externalId, type, name, text, status, ... };
//   struct Link   { id, sourceType, sourceId, targetType, targetId, relation,
//                   rationale, status /*Active|Superseded|Proposed*/, version };
//   class TraceLinkService { addEntity, getEntity, addLink, linksFrom/linksTo,
//                            isLegalTransition, transition, ... };
// ---------------------------------------------------------------------------
//
// namespace lodestar::tracelink {
//
// struct GraphNode {
//     EntityType type;
//     std::string id;
//     std::string externalId;
//     std::string name;
// };
//
// struct GraphLink {
//     std::string id;
//     std::string sourceId;
//     std::string targetId;
//     std::string relation;
// };
//
// enum class Direction { Out, In };  // Out=follow source->target; In=reverse
//
// // One requirement's coverage row.
// struct CoverageRow {
//     std::string requirementId;
//     std::string requirementExternalId;
//     std::vector<std::string> satisfyingDesignIds;  // designs that satisfies->it
//     std::vector<std::string> verifyingTestIds;     // tests that verifies->it
//     int designedCount = 0;
//     int verifiedCount = 0;
//     int percentDesigned = 0;   // designedCount>0 ? 100 : 0
//     int percentVerified = 0;   // verifiedCount>0 ? 100 : 0
//     int percentSatisfied = 0;  // (designedCount>0 && verifiedCount>0) ? 100 : 0
// };
//
// struct CoverageReport { std::vector<CoverageRow> rows; };
//
// struct CoverageGap {
//     std::string requirementId;
//     std::string requirementExternalId;
//     bool hasNoDesign = false;
//     bool hasNoTest = false;
// };
//
// struct MatrixCell {
//     std::string columnId;
//     std::string columnType;  // "design" | "test_case"
//     std::string relation;    // relation present, "" when none
// };
//
// struct TraceMatrixRow {
//     std::string requirementId;
//     std::string requirementExternalId;
//     std::vector<MatrixCell> cells;
// };
//
// struct TraceMatrix {
//     std::vector<std::string> columnIds;  // ordered unique design+test ids
//     std::vector<TraceMatrixRow> rows;
// };
//
// struct ImpactAnalysis {
//     std::vector<GraphNode> affectedEntities;   // both-direction closure + self
//     std::vector<GraphLink> affectedLinks;      // Active links whose source AND
//                                               // target are both in affectedEntities
//     std::vector<GraphNode> downstreamTestCases;
//     std::vector<std::string> blockedTransitions; // e.g. "REQ-X -> Verified"
// };
//
// class GraphEngine {
// public:
//     explicit GraphEngine(persistence::Database& db);
//
//     // Bijective reverse-relation mapping; reverseRelation(reverseRelation(x))==x.
//     static std::string reverseRelation(const std::string& relation);
//
//     // Closure. depth<=0 means unlimited. Result EXCLUDES the start node.
//     // downstreamClosure = BFS following links source->target.
//     // upstreamClosure   = BFS following links target->source.
//     common::Result<std::vector<GraphNode>>
//         downstreamClosure(EntityType, const std::string& id, int depth = 0);
//     common::Result<std::vector<GraphNode>>
//         upstreamClosure(EntityType, const std::string& id, int depth = 0);
//
//     // General traversal along links of one relation ("" = all).
//     common::Result<std::vector<GraphNode>>
//         graphQuery(EntityType, const std::string& id, const std::string& relation,
//                    Direction direction, int depth = 0);
//
//     // affectedEntities = union(downstreamClosure, upstreamClosure) + the node.
//     // blockedTransitions: for each affected requirement with no Active satisfies
//     //   link add "<extId> -> Implemented"; with no Active verifies link add
//     //   "<extId> -> Verified".
//     common::Result<ImpactAnalysis>
//         impactAnalysis(EntityType, const std::string& id);
//
//     common::Result<CoverageReport> coverage();
//     common::Result<std::vector<CoverageGap>> coverageGap();
//     common::Result<TraceMatrix> traceMatrix();
// };
// }  // namespace lodestar::tracelink
//
// Reverse-relation map (bijective):
//   verifies<->is_verified_by  satisfies<->is_satisfied_by  derives<->is_derived_from
//   allocates<->is_allocated_to  refines<->is_refined_by  decomposes<->is_decomposed_into
//   depends_on<->is_dependency_for  traces_to<->is_traced_by
//   validates<->is_validated_by  conflicts<->is_conflicted_with
// ---------------------------------------------------------------------------

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/TraceLinkService.h"

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
// Factories (same contract as WP-1).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Draft") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "The system shall provide GNSS position output.";
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
    e.text = "Verify GNSS position output accuracy.";
    return e;
}

tl::Entity makeDesign(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = extId;
    e.text = "Position solver component.";
    return e;
}

tl::Entity makeIface(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Interface;
    e.name = extId;
    e.text = "RS-422 data interface.";
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
// Result-set helpers.
// ---------------------------------------------------------------------------
bool containsId(const std::vector<tl::GraphNode>& nodes, const std::string& id) {
    for (const auto& n : nodes) {
        if (n.id == id) return true;
    }
    return false;
}

bool containsExtId(const std::vector<tl::GraphNode>& nodes, const std::string& extId) {
    for (const auto& n : nodes) {
        if (n.externalId == extId) return true;
    }
    return false;
}

const tl::GraphNode* findByExtId(const std::vector<tl::GraphNode>& nodes,
                                 const std::string& extId) {
    for (const auto& n : nodes) {
        if (n.externalId == extId) return &n;
    }
    return nullptr;
}

bool containsString(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

const tl::CoverageRow* rowFor(const std::vector<tl::CoverageRow>& rows,
                              const std::string& requirementId) {
    for (const auto& r : rows) {
        if (r.requirementId == requirementId) return &r;
    }
    return nullptr;
}

const tl::TraceMatrixRow* matrixRowFor(const std::vector<tl::TraceMatrixRow>& rows,
                                       const std::string& requirementId) {
    for (const auto& r : rows) {
        if (r.requirementId == requirementId) return &r;
    }
    return nullptr;
}

const tl::MatrixCell* cellFor(const tl::TraceMatrixRow& row, const std::string& columnId) {
    for (const auto& c : row.cells) {
        if (c.columnId == columnId) return &c;
    }
    return nullptr;
}

bool columnPresent(const std::vector<std::string>& cols, const std::string& id) {
    for (const auto& c : cols) {
        if (c == id) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// The WP-2 acceptance 3-level chain:
//   system req (REQ-SYS) -> derived req (REQ-DER) -> design (DES-1) -> test (TC-1)
// Links (all Active):
//   REQ-DER derives REQ-SYS      (requirement -> requirement)
//   DES-1   satisfies REQ-DER    (design      -> requirement)
//   TC-1    verifies REQ-DER     (test_case   -> requirement)
// ---------------------------------------------------------------------------
struct Chain {
    std::string sysReqId;
    std::string derReqId;
    std::string desId;
    std::string tcId;
};

Chain buildChain(tl::TraceLinkService& svc) {
    Chain c;
    auto sys = svc.addEntity(makeReq("REQ-SYS", "Approved"));
    auto der = svc.addEntity(makeReq("REQ-DER", "Draft"));
    auto des = svc.addEntity(makeDesign("DES-1"));
    auto tc  = svc.addEntity(makeTc("TC-1"));
    c.sysReqId = sys.value().id;
    c.derReqId = der.value().id;
    c.desId = des.value().id;
    c.tcId = tc.value().id;

    svc.addLink(makeLink(tl::EntityType::Requirement, c.derReqId,
                         tl::EntityType::Requirement, c.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, c.desId,
                         tl::EntityType::Requirement, c.derReqId, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, c.tcId,
                         tl::EntityType::Requirement, c.derReqId, "verifies"));
    return c;
}

// ---------------------------------------------------------------------------
// 1. Reverse-relation mapping
// ---------------------------------------------------------------------------
void testReverseMapping(Harness& h) {
    h.section("1. Reverse-relation mapping");

    struct Pair { const char* fwd; const char* rev; };
    const Pair pairs[] = {
        {"verifies",    "is_verified_by"},
        {"satisfies",   "is_satisfied_by"},
        {"derives",     "is_derived_from"},
        {"allocates",   "is_allocated_to"},
        {"refines",     "is_refined_by"},
        {"decomposes",  "is_decomposed_into"},
        {"depends_on",  "is_dependency_for"},
        {"traces_to",   "is_traced_by"},
        {"validates",   "is_validated_by"},
        {"conflicts",   "is_conflicted_with"},
    };

    for (const auto& p : pairs) {
        h.check(tl::GraphEngine::reverseRelation(p.fwd) == p.rev,
                (std::string("reverseRelation(") + p.fwd + ")").c_str());
        h.check(tl::GraphEngine::reverseRelation(p.rev) == p.fwd,
                (std::string("reverseRelation(") + p.rev + ") is bijective").c_str());
    }
}

// ---------------------------------------------------------------------------
// 2. Upstream / downstream closure
// ---------------------------------------------------------------------------
void testClosure(Harness& h) {
    h.section("2. upstream / downstream closure");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_closure.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);
    Chain c = buildChain(svc);

    // downstream follows source->target
    auto dsSys = engine.downstreamClosure(tl::EntityType::Requirement, c.sysReqId);
    h.check(dsSys.isOk() && dsSys.value().empty(), "downstream(root) empty");

    auto dsDer = engine.downstreamClosure(tl::EntityType::Requirement, c.derReqId);
    h.check(dsDer.isOk() && dsDer.value().size() == 1 &&
                containsId(dsDer.value(), c.sysReqId),
            "downstream(derived) = {system}");

    auto dsTc = engine.downstreamClosure(tl::EntityType::TestCase, c.tcId);
    h.check(dsTc.isOk() && dsTc.value().size() == 2 &&
                containsId(dsTc.value(), c.derReqId) &&
                containsId(dsTc.value(), c.sysReqId),
            "downstream(test) = {derived, system}");

    auto dsDes = engine.downstreamClosure(tl::EntityType::Design, c.desId);
    h.check(dsDes.isOk() && dsDes.value().size() == 2 &&
                containsId(dsDes.value(), c.derReqId) &&
                containsId(dsDes.value(), c.sysReqId),
            "downstream(design) = {derived, system}");

    // upstream follows target->source (reverse traversal)
    auto usSys = engine.upstreamClosure(tl::EntityType::Requirement, c.sysReqId);
    h.check(usSys.isOk() && usSys.value().size() == 3 &&
                containsId(usSys.value(), c.derReqId) &&
                containsId(usSys.value(), c.desId) &&
                containsId(usSys.value(), c.tcId),
            "upstream(root) = {derived, design, test}");

    auto usDer = engine.upstreamClosure(tl::EntityType::Requirement, c.derReqId);
    h.check(usDer.isOk() && usDer.value().size() == 2 &&
                containsId(usDer.value(), c.desId) &&
                containsId(usDer.value(), c.tcId),
            "upstream(derived) = {design, test}");

    auto usTc = engine.upstreamClosure(tl::EntityType::TestCase, c.tcId);
    h.check(usTc.isOk() && usTc.value().empty(), "upstream(leaf test) empty");

    // nodes carry external ids (useful for reporting)
    const tl::GraphNode* derNode = findByExtId(usSys.value(), "REQ-DER");
    h.check(derNode != nullptr && derNode->name == "REQ-DER",
            "upstream result carries external id + name");

    // depth limiting
    auto depth1 = engine.downstreamClosure(tl::EntityType::TestCase, c.tcId, 1);
    h.check(depth1.isOk() && depth1.value().size() == 1 &&
                containsId(depth1.value(), c.derReqId) &&
                !containsId(depth1.value(), c.sysReqId),
            "downstream(test, depth=1) stops at one hop");

    auto depth2 = engine.downstreamClosure(tl::EntityType::TestCase, c.tcId, 2);
    h.check(depth2.isOk() && depth2.value().size() == 2 &&
                containsId(depth2.value(), c.sysReqId),
            "downstream(test, depth=2) reaches system");

    // general graphQuery
    auto qOut = engine.graphQuery(tl::EntityType::TestCase, c.tcId, "verifies",
                                  tl::Direction::Out, 0);
    h.check(qOut.isOk() && qOut.value().size() == 1 &&
                containsId(qOut.value(), c.derReqId),
            "graphQuery verifies Out from test -> derived");

    auto qIn = engine.graphQuery(tl::EntityType::Requirement, c.derReqId, "verifies",
                                 tl::Direction::In, 0);
    h.check(qIn.isOk() && qIn.value().size() == 1 &&
                containsId(qIn.value(), c.tcId),
            "graphQuery verifies In to derived -> test");

    auto qDerivesIn = engine.graphQuery(tl::EntityType::Requirement, c.sysReqId,
                                        "derives", tl::Direction::In, 0);
    h.check(qDerivesIn.isOk() && qDerivesIn.value().size() == 1 &&
                containsId(qDerivesIn.value(), c.derReqId),
            "graphQuery derives In to system -> derived");
}

// ---------------------------------------------------------------------------
// 3. Impact analysis
// ---------------------------------------------------------------------------
void testImpact(Harness& h) {
    h.section("3. impactAnalysis (blocked transitions + downstream tests)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_impact.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);

    // Chain plus an unverified derived requirement branch:
    //   REQ-UNV derives REQ-SYS, DES-UNV satisfies REQ-UNV, but NO test verifies it.
    Chain c = buildChain(svc);
    auto unv = svc.addEntity(makeReq("REQ-UNV", "Approved"));
    auto desUnv = svc.addEntity(makeDesign("DES-UNV"));
    const std::string unvId = unv.value().id;
    const std::string desUnvId = desUnv.value().id;
    svc.addLink(makeLink(tl::EntityType::Requirement, unvId,
                         tl::EntityType::Requirement, c.sysReqId, "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, desUnvId,
                         tl::EntityType::Requirement, unvId, "satisfies"));

    // Change the root system requirement -> whole traceability branch affected.
    auto impRoot = engine.impactAnalysis(tl::EntityType::Requirement, c.sysReqId);
    h.check(impRoot.isOk(), "impactAnalysis(root) ok");
    const tl::ImpactAnalysis& ri = impRoot.value();
    h.check(containsId(ri.affectedEntities, c.sysReqId) &&
                containsId(ri.affectedEntities, c.derReqId) &&
                containsId(ri.affectedEntities, c.desId) &&
                containsId(ri.affectedEntities, c.tcId) &&
                containsId(ri.affectedEntities, unvId) &&
                containsId(ri.affectedEntities, desUnvId),
            "impact(root) includes the whole branch");
    h.check(containsExtId(ri.downstreamTestCases, "TC-1"),
            "impact(root) downstream test cases include TC-1");
    h.check(ri.affectedLinks.size() == 5,
            "impact(root) includes all 5 incident links");

    // blocked transitions: REQ-UNV is designed but not verified.
    h.check(containsString(ri.blockedTransitions, "REQ-UNV -> Verified"),
            "REQ-UNV blocked from Verified (no test)");
    h.check(!containsString(ri.blockedTransitions, "REQ-UNV -> Implemented"),
            "REQ-UNV NOT blocked from Implemented (has design)");
    h.check(containsString(ri.blockedTransitions, "REQ-SYS -> Verified"),
            "REQ-SYS blocked from Verified (no test)");
    h.check(containsString(ri.blockedTransitions, "REQ-SYS -> Implemented"),
            "REQ-SYS blocked from Implemented (no design)");
}

// ---------------------------------------------------------------------------
// 4. Coverage math
// ---------------------------------------------------------------------------
void testCoverage(Harness& h) {
    h.section("4. coverage() math (% designed / verified / satisfied)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_coverage.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);
    Chain c = buildChain(svc);

    auto rep = engine.coverage();
    h.check(rep.isOk(), "coverage() ok");

    const tl::CoverageRow* der = rowFor(rep.value().rows, c.derReqId);
    const tl::CoverageRow* sys = rowFor(rep.value().rows, c.sysReqId);
    h.check(der != nullptr && sys != nullptr, "coverage has rows for both requirements");

    // REQ-DER: one satisfying design + one verifying test.
    h.check(der->designedCount == 1 && der->satisfyingDesignIds.size() == 1 &&
                der->satisfyingDesignIds[0] == c.desId,
            "REQ-DER designed by DES-1");
    h.check(der->verifiedCount == 1 && der->verifyingTestIds.size() == 1 &&
                der->verifyingTestIds[0] == c.tcId,
            "REQ-DER verified by TC-1");
    h.check(der->percentDesigned == 100, "REQ-DER percentDesigned = 100");
    h.check(der->percentVerified == 100, "REQ-DER percentVerified = 100");
    h.check(der->percentSatisfied == 100, "REQ-DER percentSatisfied = 100");

    // REQ-SYS: not designed and not verified -> 0%.
    h.check(sys->designedCount == 0 && sys->verifiedCount == 0,
            "REQ-SYS has zero design and zero test");
    h.check(sys->percentDesigned == 0 && sys->percentVerified == 0 &&
                sys->percentSatisfied == 0,
            "REQ-SYS all percentages 0");
}

// ---------------------------------------------------------------------------
// 5. Coverage gap
// ---------------------------------------------------------------------------
void testCoverageGap(Harness& h) {
    h.section("5. coverageGap()");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_gap.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);
    Chain c = buildChain(svc);

    auto gaps = engine.coverageGap();
    h.check(gaps.isOk(), "coverageGap() ok");

    bool foundSys = false;
    bool foundDer = false;
    for (const auto& g : gaps.value()) {
        if (g.requirementId == c.sysReqId) {
            foundSys = true;
            h.check(g.hasNoDesign && g.hasNoTest, "REQ-SYS flagged no design + no test");
        }
        if (g.requirementId == c.derReqId) {
            foundDer = true;
            h.check(!g.hasNoDesign && !g.hasNoTest, "REQ-DER not flagged (fully covered)");
        }
    }
    h.check(foundSys, "coverageGap lists REQ-SYS");
    h.check(foundDer, "coverageGap lists REQ-DER (as a row, without gaps)");
}

// ---------------------------------------------------------------------------
// 6. Trace matrix
// ---------------------------------------------------------------------------
void testTraceMatrix(Harness& h) {
    h.section("6. traceMatrix()");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_matrix.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);
    Chain c = buildChain(svc);

    auto m = engine.traceMatrix();
    h.check(m.isOk(), "traceMatrix() ok");
    h.check(columnPresent(m.value().columnIds, c.desId),
            "matrix has a design column (DES-1)");
    h.check(columnPresent(m.value().columnIds, c.tcId),
            "matrix has a test column (TC-1)");

    const tl::TraceMatrixRow* der = matrixRowFor(m.value().rows, c.derReqId);
    const tl::TraceMatrixRow* sys = matrixRowFor(m.value().rows, c.sysReqId);
    h.check(der != nullptr && sys != nullptr, "matrix has rows for both requirements");

    const tl::MatrixCell* derDes = cellFor(*der, c.desId);
    const tl::MatrixCell* derTc = cellFor(*der, c.tcId);
    h.check(derDes != nullptr && derDes->relation == "satisfies",
            "REQ-DER x DES-1 cell = satisfies");
    h.check(derTc != nullptr && derTc->relation == "verifies",
            "REQ-DER x TC-1 cell = verifies");
    h.check(derDes->columnType == "design" && derTc->columnType == "test_case",
            "matrix cell column types set");

    const tl::MatrixCell* sysDes = cellFor(*sys, c.desId);
    const tl::MatrixCell* sysTc = cellFor(*sys, c.tcId);
    h.check(sysDes != nullptr && sysDes->relation.empty(),
            "REQ-SYS x DES-1 cell empty");
    h.check(sysTc != nullptr && sysTc->relation.empty(),
            "REQ-SYS x TC-1 cell empty");
}

// ---------------------------------------------------------------------------
// 7. WP-2 acceptance
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("7. WP-2 acceptance: 3-level graph coverage + leaf-change impact");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp2_accept.db")) {
        h.check(false, "open fresh db");
        return;
    }

    tl::TraceLinkService svc(db);
    tl::GraphEngine engine(db);
    Chain c = buildChain(svc);

    // (a) correct coverage for the 3-level chain.
    auto rep = engine.coverage();
    h.check(rep.isOk(), "coverage() ok");
    const tl::CoverageRow* der = rowFor(rep.value().rows, c.derReqId);
    h.check(der != nullptr && der->designedCount == 1 && der->verifiedCount == 1 &&
                der->percentDesigned == 100 && der->percentVerified == 100 &&
                der->percentSatisfied == 100,
            "REQ-DER fully covered (designed + verified, 100%)");

    // (b) a leaf (the test) changes -> correct impact set ripples up to the
    //     requirements it verifies. DES-1 is unrelated and must NOT appear.
    auto imp = engine.impactAnalysis(tl::EntityType::TestCase, c.tcId);
    h.check(imp.isOk(), "impactAnalysis(leaf test) ok");
    const tl::ImpactAnalysis& a = imp.value();
    h.check(a.affectedEntities.size() == 3,
            "leaf change affects exactly 3 entities");
    h.check(containsId(a.affectedEntities, c.tcId) &&
                containsId(a.affectedEntities, c.derReqId) &&
                containsId(a.affectedEntities, c.sysReqId),
            "leaf change affects {test, derived, system}");
    h.check(!containsId(a.affectedEntities, c.desId),
            "leaf change does NOT affect the unrelated design DES-1");
    h.check(containsExtId(a.downstreamTestCases, "TC-1"),
            "impact lists the changed test case as a downstream test");
    h.check(a.affectedLinks.size() == 2,
            "leaf change touches exactly 2 links (verifies + derives)");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-2 graph engine");
    std::printf("WP-2 GRAPH ENGINE TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testReverseMapping(h);
    testClosure(h);
    testImpact(h);
    testCoverage(h);
    testCoverageGap(h);
    testTraceMatrix(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
