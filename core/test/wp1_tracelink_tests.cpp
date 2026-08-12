// core/test/wp1_tracelink_tests.cpp
// ---------------------------------------------------------------------------
// WP-1 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-1 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-1 / section 4.1-4.2 / 7.1):
//   1. DAO CRUD          -> persistence::RequirementDao / TraceLinkDao round-trip
//   2. Status transition -> TraceLinkService::isLegalTransition / transition
//   3. Integrity         -> dangling / self-loop / duplicate / relation-type
//   4. WP-1 acceptance   -> typed requirement + verifies link + reject duplicate
//
// Uses a lightweight self-contained harness (matching the existing smoke path)
// because Catch2 is not yet a vcpkg dependency. Each check reports [PASS]/[FAIL]
// and the process exits non-zero if any check fails.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-1 engineer must provide (in lodestar::tracelink).
// ---------------------------------------------------------------------------
// namespace lodestar::tracelink {
//
// enum class EntityType { Requirement, Design, Interface, TestCase };
//
// // Enriched entity (migration 003). The persistence models gain these fields.
// struct Entity {
//     std::string id;             // internal UUID
//     std::string externalId;     // human id, e.g. "REQ-100"; unique per type
//     EntityType  type   = EntityType::Requirement;
//     std::string name;
//     std::string text;
//     std::string status = "Draft";
//     std::string priority;
//     std::string owner;
//     std::string rationale;
//     std::string verificationMethod;
//     std::string safetyLevel;
//     std::string parentId;
//     int         version = 1;
// };
//
// // Enriched link (migration 004).
// struct Link {
//     std::string id;
//     EntityType  sourceType;
//     std::string sourceId;
//     EntityType  targetType;
//     std::string targetId;
//     std::string relation;      // satisfies|verifies|derives|allocates|refines|
//                                // decomposes|depends_on|traces_to|validates|conflicts
//     std::string rationale;
//     std::string status = "Active";   // Active | Superseded | Proposed
//     int         version = 1;
// };
//
// struct EntityFilter {
//     std::optional<std::string> status;
// };
//
// class TraceLinkService {
// public:
//     explicit TraceLinkService(persistence::Database& db);
//
//     // --- entity CRUD -----------------------------------------------------
//     common::Result<Entity> addEntity(const Entity& e);             // assigns UUID + externalId
//     common::Result<Entity> updateEntity(const Entity& e);          // bumps version
//     common::Result<void>   removeEntity(EntityType, const std::string& id); // soft delete
//     common::Result<std::optional<Entity>> getEntity(EntityType, const std::string& id);
//     common::Result<std::vector<Entity>> listEntities(EntityType, const EntityFilter&);
//
//     // --- links ------------------------------------------------------------
//     // addLink validates nodes exist (no dangling), no self-loop, no duplicate,
//     // and relation is legal for the source/target pair.
//     common::Result<Link> addLink(const Link& link);
//     common::Result<Link> updateLink(const Link& link);
//     common::Result<void> removeLink(const std::string& id);        // marks Superseded
//     common::Result<std::vector<Link>> linksFrom(EntityType, const std::string& id);
//     common::Result<std::vector<Link>> linksTo(EntityType, const std::string& id);
//
//     // --- status state machine --------------------------------------------
//     bool isLegalTransition(EntityType, const std::string& from, const std::string& to);
//     common::Result<void> transition(EntityType, const std::string& id,
//                                     const std::string& to);
// };
// }  // namespace lodestar::tracelink
//
// Status machines (plan section 2.3); any state may move to "Obsolete":
//   Requirement: Draft -> Proposed -> Approved -> Validated -> Implemented -> Verified
//   Design:      Draft -> Reviewed -> Released
//   Test case:   Draft -> Ready -> Executed -> Passed -> Failed
//   Interface:   Draft -> Agreed -> Released -> Changed
//
// Allowed relation types (plan section 2.4); "traces_to" and "conflicts" allow
// any pair, everything else is restricted:
//   satisfies:   design      -> requirement
//   verifies:    test_case   -> requirement
//   derives:     requirement -> requirement
//   allocates:   requirement -> design
//   refines:     requirement -> requirement
//   decomposes:  design      -> design
//   depends_on:  design      -> interface
//   validates:   test_case   -> design
//
// persistence DAO additions: RequirementDao::update/softDelete/search,
// TraceLinkDao::update/softDelete; softDelete marks the row Obsolete/Superseded
// and the *findAll* queries return only non-deleted rows.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

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
// Test fixtures / factories.
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
// 1. DAO CRUD
// ---------------------------------------------------------------------------
void testDaoCrd(p::Database& db, Harness& h) {
    h.section("1. DAO CRUD (RequirementDao / TraceLinkDao)");

    p::RequirementDao reqDao(db);

    // create + findById round-trip
    p::Requirement r;
    r.id = "req-1";
    r.name = "REQ-1";
    r.description = "desc";
    r.status = "Draft";
    h.check(reqDao.create(r).isOk(), "RequirementDao.create ok");
    auto found = reqDao.findById(r.id);
    h.check(found.isOk() && found.value().has_value(), "RequirementDao.findById returns row");
    h.check(found.value()->name == "REQ-1", "findById round-trips name");
    h.check(found.value()->status == "Draft", "findById round-trips status");

    // update persists changes
    r.name = "REQ-1-v2";
    r.status = "Approved";
    h.check(reqDao.update(r).isOk(), "RequirementDao.update ok");
    auto upd = reqDao.findById(r.id);
    h.check(upd.value()->name == "REQ-1-v2", "update persisted name");
    h.check(upd.value()->status == "Approved", "update persisted status");

    // soft delete removes the row from findAll
    h.check(reqDao.softDelete(r.id).isOk(), "RequirementDao.softDelete ok");
    auto all = reqDao.findAll();
    h.check(all.isOk(), "findAll ok after delete");
    bool gone = true;
    for (const auto& it : all.value()) {
        if (it.id == r.id) gone = false;
    }
    h.check(gone, "deleted row excluded from findAll");

    // links CRUD
    p::TraceLinkDao linkDao(db);
    p::TraceLink l;
    l.id = "link-1";
    l.sourceType = "test_case";
    l.sourceId = "tc-1";
    l.targetType = "requirement";
    l.targetId = "req-1";
    l.relation = "verifies";
    h.check(linkDao.create(l).isOk(), "TraceLinkDao.create ok");
    auto bySrc = linkDao.findBySource("test_case", "tc-1");
    h.check(bySrc.isOk() && bySrc.value().size() == 1, "findBySource returns the link");
    auto byTgt = linkDao.findByTarget("requirement", "req-1");
    h.check(byTgt.isOk() && byTgt.value().size() == 1, "findByTarget returns the link");

    // link update + soft delete
    l.rationale = "proves REQ-1";
    h.check(linkDao.update(l).isOk(), "TraceLinkDao.update ok");
    auto updatedLinks = linkDao.findBySource("test_case", "tc-1");
    h.check(updatedLinks.value().size() == 1 &&
                updatedLinks.value().front().rationale == "proves REQ-1",
            "link update persisted rationale");
    h.check(linkDao.softDelete(l.id).isOk(), "TraceLinkDao.softDelete ok");
}

// ---------------------------------------------------------------------------
// 2. Status transition rules
// ---------------------------------------------------------------------------
void testStatusRules(p::Database& db, Harness& h) {
    h.section("2. Status transition rules");

    tl::TraceLinkService svc(db);

    // legal chains
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Draft", "Proposed"),
            "req Draft->Proposed legal");
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Proposed", "Approved"),
            "req Proposed->Approved legal");
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Approved", "Validated"),
            "req Approved->Validated legal");
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Validated", "Implemented"),
            "req Validated->Implemented legal");
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Implemented", "Verified"),
            "req Implemented->Verified legal");
    h.check(svc.isLegalTransition(tl::EntityType::Requirement, "Draft", "Obsolete"),
            "req any->Obsolete legal");

    // illegal
    h.check(!svc.isLegalTransition(tl::EntityType::Requirement, "Draft", "Verified"),
            "req Draft->Verified illegal (skip)");
    h.check(!svc.isLegalTransition(tl::EntityType::Requirement, "Approved", "Draft"),
            "req Approved->Draft illegal (reverse)");
    h.check(!svc.isLegalTransition(tl::EntityType::Requirement, "Verified", "Proposed"),
            "req Verified->Proposed illegal");

    // other types
    h.check(svc.isLegalTransition(tl::EntityType::Design, "Draft", "Reviewed"),
            "design Draft->Reviewed legal");
    h.check(!svc.isLegalTransition(tl::EntityType::Design, "Draft", "Released"),
            "design Draft->Released illegal");
    h.check(svc.isLegalTransition(tl::EntityType::TestCase, "Draft", "Ready"),
            "tc Draft->Ready legal");
    h.check(!svc.isLegalTransition(tl::EntityType::TestCase, "Draft", "Passed"),
            "tc Draft->Passed illegal");
    h.check(svc.isLegalTransition(tl::EntityType::Interface, "Draft", "Agreed"),
            "iface Draft->Agreed legal");
    h.check(!svc.isLegalTransition(tl::EntityType::Interface, "Draft", "Released"),
            "iface Draft->Released illegal");

    // real transition persists and illegal is rejected
    auto added = svc.addEntity(makeReq("REQ-T1", "Draft"));
    h.check(added.isOk(), "addEntity ok for transition test");
    auto okT = svc.transition(tl::EntityType::Requirement, added.value().id, "Proposed");
    h.check(okT.isOk(), "transition Draft->Proposed applied");
    auto got = svc.getEntity(tl::EntityType::Requirement, added.value().id);
    h.check(got.value().has_value() && got.value()->status == "Proposed",
            "status persisted after transition");
    auto badT = svc.transition(tl::EntityType::Requirement, added.value().id, "Verified");
    h.check(badT.failed(), "transition Draft->Verified rejected");
    auto got2 = svc.getEntity(tl::EntityType::Requirement, added.value().id);
    h.check(got2.value()->status == "Proposed", "rejected transition left status unchanged");
}

// ---------------------------------------------------------------------------
// 3. Integrity (dangling / self-loop / duplicate / relation-type)
// ---------------------------------------------------------------------------
void testIntegrity(p::Database& db, Harness& h) {
    h.section("3. Integrity (dangling / self-loop / duplicate / relation-type)");

    tl::TraceLinkService svc(db);

    auto reqRes = svc.addEntity(makeReq("REQ-I1"));
    auto tcRes = svc.addEntity(makeTc("TC-I1"));
    h.check(reqRes.isOk() && tcRes.isOk(), "setup entities ok");
    const std::string reqId = reqRes.value().id;
    const std::string tcId = tcRes.value().id;

    // dangling target
    auto dangling = makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                             "does-not-exist", "verifies");
    h.check(svc.addLink(dangling).failed(), "dangling target rejected");

    // dangling source
    auto danglingSrc = makeLink(tl::EntityType::TestCase, "does-not-exist",
                                tl::EntityType::Requirement, reqId, "verifies");
    h.check(svc.addLink(danglingSrc).failed(), "dangling source rejected");

    // self-loop (valid relation 'refines' between a requirement and itself)
    auto selfLoop = makeLink(tl::EntityType::Requirement, reqId, tl::EntityType::Requirement,
                             reqId, "refines");
    h.check(svc.addLink(selfLoop).failed(), "self-loop rejected");

    // duplicate
    auto v1 = makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                       reqId, "verifies");
    h.check(svc.addLink(v1).isOk(), "first verifies link accepted");
    auto v2 = makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                       reqId, "verifies");
    h.check(svc.addLink(v2).failed(), "duplicate verifies link rejected");

    // relation-type: only test_case may 'verifies'; a design must not
    auto dRes = svc.addEntity(makeDesign("DES-I1"));
    const std::string desId = dRes.value().id;
    auto badRel = makeLink(tl::EntityType::Design, desId, tl::EntityType::Requirement,
                           reqId, "verifies");
    h.check(svc.addLink(badRel).failed(), "relation type not allowed for pair rejected");

    // legal relation-type is accepted
    auto goodRel = makeLink(tl::EntityType::Design, desId, tl::EntityType::Requirement,
                            reqId, "satisfies");
    h.check(svc.addLink(goodRel).isOk(), "legal relation type (satisfies) accepted");
}

// ---------------------------------------------------------------------------
// 4. WP-1 acceptance: typed requirement + verifies link + reject duplicate
// ---------------------------------------------------------------------------
void testAcceptance(p::Database& db, Harness& h) {
    h.section("4. WP-1 acceptance: typed requirement + verifies link + reject duplicate");

    tl::TraceLinkService svc(db);

    // typed requirement with full attributes
    tl::Entity req = makeReq("REQ-100");
    req.verificationMethod = "test";
    req.safetyLevel = "Level A";
    req.priority = "High";
    req.owner = "systems";
    req.rationale = "Certification evidence for position accuracy.";
    auto reqRes = svc.addEntity(req);
    h.check(reqRes.isOk(), "add typed requirement ok");
    h.check(!reqRes.value().id.empty(), "entity got an internal UUID");
    h.check(!reqRes.value().externalId.empty(), "entity got an external id");

    // external id unique per type
    auto dupReq = svc.addEntity(makeReq("REQ-100"));
    h.check(dupReq.failed(), "duplicate external id rejected");

    auto tcRes = svc.addEntity(makeTc("TC-200"));
    h.check(tcRes.isOk(), "add test case ok");
    const std::string reqId = reqRes.value().id;
    const std::string tcId = tcRes.value().id;

    // verifies link accepted
    auto v1 = makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                       reqId, "verifies");
    auto linkRes = svc.addLink(v1);
    h.check(linkRes.isOk(), "test_case verifies requirement accepted");

    // duplicate verifies link rejected
    auto v2 = makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                       reqId, "verifies");
    h.check(svc.addLink(v2).failed(), "duplicate verifies link rejected");

    // the typed data round-trips
    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    h.check(got.isOk() && got.value().has_value(), "getEntity ok");
    h.check(got.value()->type == tl::EntityType::Requirement, "type round-trips");
    h.check(got.value()->externalId == "REQ-100", "external id round-trips");
    h.check(got.value()->verificationMethod == "test", "verification method round-trips");
    h.check(got.value()->safetyLevel == "Level A", "safety level round-trips");
    h.check(got.value()->priority == "High", "priority round-trips");

    // link visible from the requirement side
    auto to = svc.linksTo(tl::EntityType::Requirement, reqId);
    h.check(to.isOk() && to.value().size() == 1, "requirement has exactly one incoming link");
    h.check(to.value().front().relation == "verifies", "incoming link relation is verifies");
    h.check(to.value().front().sourceId == tcId, "incoming link source is the test case");
}

}  // namespace

int main(int argc, char** argv) {
    std::string migrationsDir = LODESTAR_MIGRATIONS_DIR;
    if (argc > 1) {
        migrationsDir = argv[1];
    }

    // Throwaway DB in the current working directory (mirrors the smoke path).
    const std::string dbPath = "lodestar_wp1_tests.db";
    std::remove(dbPath.c_str());

    p::Database db;
    auto open = db.open(dbPath);
    if (open.failed()) {
        std::fprintf(stderr, "WP1 TESTS FAIL: open db: %s\n", open.error().c_str());
        return 1;
    }

    p::MigrationRunner runner(db);
    auto mig = runner.run(migrationsDir);
    if (mig.failed()) {
        std::fprintf(stderr, "WP1 TESTS FAIL: migrate: %s\n", mig.error().c_str());
        db.close();
        std::remove(dbPath.c_str());
        return 1;
    }

    Harness h("WP-1 tracelink");
    std::printf("WP-1 TRACELINK TESTS (schema v%d)\n", mig.value());

    testDaoCrd(db, h);
    testStatusRules(db, h);
    testIntegrity(db, h);
    testAcceptance(db, h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());

    db.close();
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    return h.failures() == 0 ? 0 : 1;
}
