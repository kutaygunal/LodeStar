// core/test/wp3_rules_engine_tests.cpp
// ---------------------------------------------------------------------------
// WP-3 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-3 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-3 / section 4.4 / 7.5):
//   1. Rule lifecycle      -> defineRule / listRules / enableRule / disableRule
//   2. Built-in templates  -> REQ_MUST_BE_VERIFIED, REQ_MUST_BE_SATISFIED,
//                             NO_DANGLING_LINKS, NO_DUPLICATE_LINKS,
//                             NO_SELF_LINKS, COVERAGE_MIN, NO_ORPHAN_DESIGN,
//                             STATUS_VALID
//   3. Standard tagging    -> each rule maps to assurance standards
//   4. runValidation       -> writes validation_runs + compliance_violations
//   5. WP-3 acceptance     -> an unverified requirement triggers
//                             REQ_MUST_BE_VERIFIED and appears in a run report
//
// Uses the same lightweight self-contained harness as WP-1/WP-2.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-3 engineer must provide (in core/tracelink/RulesEngine.h,
// namespace lodestar::tracelink). Reuses WP-1 types (Entity, Link, EntityType,
// TraceLinkService) and the relation set.
// ---------------------------------------------------------------------------
//
// namespace lodestar::tracelink {
//
// enum class Severity { Info, Warning, Error, Critical };
//
// // A compliance rule. ruleType names the built-in template to evaluate.
// struct Rule {
//     std::string id;                          // UUID; assigned if empty
//     std::string name;                        // display name
//     std::string ruleType;                    // built-in template key
//     std::map<std::string, std::string> params;   // e.g. COVERAGE_MIN min_percent
//     Severity severity = Severity::Error;
//     std::vector<std::string> standards;      // ARP4754A / ARP4761 / DO-178C / DO-254
//     bool enabled = true;
// };
//
// // One recorded rule violation.
// struct Violation {
//     std::string id;
//     std::string runId;
//     std::string ruleId;
//     std::string ruleName;
//     std::string ruleType;
//     std::vector<std::string> standards;      // copied from the rule (tagging)
//     EntityType entityType;
//     std::string entityId;                    // offending entity
//     std::string entityExternalId;
//     std::string message;
//     Severity severity;
// };
//
// // Result of one runValidation() invocation.
// struct ValidationRun {
//     std::string id;
//     std::string name;
//     std::string status;        // "ok" | "violations"
//     std::string summary;
//     int violationCount = 0;
//     std::vector<Violation> violations;   // every violation written this run
// };
//
// class RulesEngine {
// public:
//     explicit RulesEngine(persistence::Database& db);
//
//     // Persists a rule. Assigns a UUID to rule.id if empty. Returns the rule.
//     common::Result<Rule> defineRule(const Rule& rule);
//
//     common::Result<std::vector<Rule>> listRules();
//
//     // Toggle a rule on/off. Disabled rules are skipped by runValidation.
//     common::Result<void> enableRule(const std::string& ruleId, bool enabled);
//
//     // Evaluates every ENABLED rule against the current graph, writes a
//     // validation_run plus compliance_violations, and returns the report.
//     common::Result<ValidationRun> runValidation();
// };
// }  // namespace lodestar::tracelink
//
// Built-in template semantics (evaluate only against NON-Obsolete / active
// entities unless stated otherwise):
//   REQ_MUST_BE_VERIFIED  : a requirement with zero Active "verifies" links ->
//                           violation (entity = the requirement).
//   REQ_MUST_BE_SATISFIED : a requirement with zero Active "satisfies" links ->
//                           violation (entity = the requirement).
//   NO_DANGLING_LINKS     : a link whose source or target entity is absent or
//                           Obsolete -> violation (entity = dangling endpoint).
//   NO_DUPLICATE_LINKS    : two links with the same (srcType, srcId, tgtType,
//                           tgtId, relation) -> violation (entity = source).
//   NO_SELF_LINKS         : a link with srcType==tgtType && srcId==tgtId ->
//                           violation (entity = the entity).
//   COVERAGE_MIN          : per active requirement, coveragePercent =
//                           (hasDesign ? 1 : 0) + (hasVerified ? 1 : 0)) * 50;
//                           violation if coveragePercent < params["min_percent"]
//                           (default 100). Entity = the requirement.
//   NO_ORPHAN_DESIGN      : a design item with no outgoing "satisfies" link ->
//                           violation (entity = the design).
//   STATUS_VALID          : an entity whose status is not one of the legal
//                           statuses for its type -> violation (entity = it).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"

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

tl::Rule makeRule(const std::string& ruleType,
                  const std::vector<std::string>& standards,
                  std::map<std::string, std::string> params = {}) {
    tl::Rule r;
    r.name = ruleType;
    r.ruleType = ruleType;
    r.standards = standards;
    r.params = std::move(params);
    r.severity = tl::Severity::Error;
    r.enabled = true;
    return r;
}

// ---------------------------------------------------------------------------
// Result-set helpers.
// ---------------------------------------------------------------------------
const tl::Violation* violationByRule(const std::vector<tl::Violation>& vs,
                                     const std::string& ruleType) {
    for (const auto& v : vs) {
        if (v.ruleType == ruleType) return &v;
    }
    return nullptr;
}

bool violationForEntity(const std::vector<tl::Violation>& vs,
                        const std::string& ruleType, const std::string& entityId) {
    for (const auto& v : vs) {
        if (v.ruleType == ruleType && v.entityId == entityId) return true;
    }
    return false;
}

bool containsStandard(const tl::Violation& v, const std::string& standard) {
    for (const auto& s : v.standards) {
        if (s == standard) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 1. Rule lifecycle + standard tagging
// ---------------------------------------------------------------------------
void testRuleLifecycle(Harness& h) {
    h.section("1. Rule lifecycle (define/list/enable/disable) + standard tagging");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_rules.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::RulesEngine engine(db);

    auto r1 = engine.defineRule(makeRule("REQ_MUST_BE_VERIFIED", {"ARP4754A", "DO-178C"}));
    auto r2 = engine.defineRule(makeRule("NO_SELF_LINKS", {"ARP4754A"}));
    h.check(r1.isOk() && !r1.value().id.empty(), "defineRule assigns a UUID id");
    h.check(r2.isOk(), "second defineRule ok");

    auto rules = engine.listRules();
    h.check(rules.isOk() && rules.value().size() == 2, "listRules returns 2 rules");

    // standard tagging round-trips through define/list
    bool foundVerified = false;
    bool tagged178c = false;
    for (const auto& r : rules.value()) {
        if (r.ruleType == "REQ_MUST_BE_VERIFIED") {
            foundVerified = true;
            for (const auto& s : r.standards) {
                if (s == "DO-178C") tagged178c = true;
            }
        }
    }
    h.check(foundVerified, "listRules includes REQ_MUST_BE_VERIFIED");
    h.check(tagged178c, "rule standard tag (DO-178C) round-trips");

    // disable -> skipped by runValidation; enable -> evaluated again
    h.check(engine.enableRule(r1.value().id, false).isOk(), "enableRule(off) ok");

    auto offRules = engine.listRules();
    bool offFound = false;
    for (const auto& r : offRules.value()) {
        if (r.ruleType == "REQ_MUST_BE_VERIFIED" && !r.enabled) offFound = true;
    }
    h.check(offFound, "disabled rule persists enabled=false");

    engine.enableRule(r1.value().id, true);
    auto onRules = engine.listRules();
    bool onFound = false;
    for (const auto& r : onRules.value()) {
        if (r.ruleType == "REQ_MUST_BE_VERIFIED" && r.enabled) onFound = true;
    }
    h.check(onFound, "re-enabled rule persists enabled=true");
}

// ---------------------------------------------------------------------------
// 2. Built-in templates: verification / satisfaction
// ---------------------------------------------------------------------------
void testRequirementRules(Harness& h) {
    h.section("2. REQ_MUST_BE_VERIFIED + REQ_MUST_BE_SATISFIED");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_ver.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    // REQ-V: fully covered (design + test). REQ-U: nothing (unverified).
    auto v = svc.addEntity(makeReq("REQ-V", "Approved"));
    auto u = svc.addEntity(makeReq("REQ-U", "Approved"));
    auto des = svc.addEntity(makeDesign("DES-V"));
    auto tc = svc.addEntity(makeTc("TC-V"));
    const std::string vId = v.value().id, uId = u.value().id;
    const std::string desId = des.value().id, tcId = tc.value().id;
    svc.addLink(makeLink(tl::EntityType::Design, desId, tl::EntityType::Requirement,
                         vId, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, tcId, tl::EntityType::Requirement,
                         vId, "verifies"));

    engine.defineRule(makeRule("REQ_MUST_BE_VERIFIED", {"ARP4754A", "DO-178C"}));
    engine.defineRule(makeRule("REQ_MUST_BE_SATISFIED", {"ARP4754A"}));

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");
    h.check(run.value().status == "violations", "run status reports violations");

    // REQ-U is unverified AND unsatisfied -> flagged by both rules.
    h.check(violationForEntity(run.value().violations, "REQ_MUST_BE_VERIFIED", uId),
            "REQ-U flagged unverified");
    h.check(violationForEntity(run.value().violations, "REQ_MUST_BE_SATISFIED", uId),
            "REQ-U flagged unsatisfied");
    // REQ-V is fully covered -> not flagged.
    h.check(!violationForEntity(run.value().violations, "REQ_MUST_BE_VERIFIED", vId),
            "REQ-V NOT flagged unverified");
    h.check(!violationForEntity(run.value().violations, "REQ_MUST_BE_SATISFIED", vId),
            "REQ-V NOT flagged unsatisfied");

    const tl::Violation* vu = violationByRule(run.value().violations, "REQ_MUST_BE_VERIFIED");
    h.check(vu != nullptr && vu->entityExternalId == "REQ-U", "violation carries external id");
    h.check(vu != nullptr && containsStandard(*vu, "DO-178C"), "violation carries standard tag");
}

// ---------------------------------------------------------------------------
// 3. Integrity rules: dangling / duplicate / self-loop
// ---------------------------------------------------------------------------
void testIntegrityRules(Harness& h) {
    h.section("3. NO_DANGLING_LINKS / NO_DUPLICATE_LINKS / NO_SELF_LINKS");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_integrity.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    auto req = svc.addEntity(makeReq("REQ-I", "Approved"));
    auto tc = svc.addEntity(makeTc("TC-I"));
    const std::string reqId = req.value().id, tcId = tc.value().id;

    // Insert integrity-violating links DIRECTLY via the DAO (addLink would reject
    // them at write time; the rules must still detect them if they ever exist).
    p::TraceLinkDao dao(db);
    p::TraceLink dangling;
    dangling.id = "link-dangling";
    dangling.sourceType = "test_case"; dangling.sourceId = tcId;
    dangling.targetType = "requirement"; dangling.targetId = "ghost-req";
    dangling.relation = "verifies"; dangling.status = "Active";
    dao.create(dangling);

    p::TraceLink dupA;
    dupA.id = "link-dup-a";
    dupA.sourceType = "test_case"; dupA.sourceId = tcId;
    dupA.targetType = "requirement"; dupA.targetId = reqId;
    dupA.relation = "verifies"; dupA.status = "Active";
    dao.create(dupA);
    p::TraceLink dupB = dupA;
    dupB.id = "link-dup-b";
    dao.create(dupB);

    p::TraceLink selfLoop;
    selfLoop.id = "link-self";
    selfLoop.sourceType = "requirement"; selfLoop.sourceId = reqId;
    selfLoop.targetType = "requirement"; selfLoop.targetId = reqId;
    selfLoop.relation = "refines"; selfLoop.status = "Active";
    dao.create(selfLoop);

    engine.defineRule(makeRule("NO_DANGLING_LINKS", {"ARP4754A", "ARP4761", "DO-178C", "DO-254"}));
    engine.defineRule(makeRule("NO_DUPLICATE_LINKS", {"ARP4754A"}));
    engine.defineRule(makeRule("NO_SELF_LINKS", {"ARP4754A"}));

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");

    // dangling -> endpoint "ghost-req" does not exist.
    bool danglingHit = false;
    for (const auto& v : run.value().violations) {
        if (v.ruleType == "NO_DANGLING_LINKS" && v.entityId == "ghost-req") danglingHit = true;
    }
    h.check(danglingHit, "NO_DANGLING_LINKS flags link to nonexistent target");

    // duplicate pair -> at least one violation.
    bool dupHit = false;
    for (const auto& v : run.value().violations) {
        if (v.ruleType == "NO_DUPLICATE_LINKS") dupHit = true;
    }
    h.check(dupHit, "NO_DUPLICATE_LINKS flags duplicate (src,tgt,relation) pair");

    // self-loop -> violation on REQ-I.
    h.check(violationForEntity(run.value().violations, "NO_SELF_LINKS", reqId),
            "NO_SELF_LINKS flags self-loop");
}

// ---------------------------------------------------------------------------
// 4. COVERAGE_MIN (numeric percent)
// ---------------------------------------------------------------------------
void testCoverageMin(Harness& h) {
    h.section("4. COVERAGE_MIN");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_covmin.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    // REQ-FULL: design + test -> 100%.
    // REQ-HALF: design only     -> 50%.
    // REQ-ZERO: nothing         -> 0%.
    auto full = svc.addEntity(makeReq("REQ-FULL", "Approved"));
    auto half = svc.addEntity(makeReq("REQ-HALF", "Approved"));
    auto zero = svc.addEntity(makeReq("REQ-ZERO", "Approved"));
    auto desFull = svc.addEntity(makeDesign("DES-FULL"));
    auto desHalf = svc.addEntity(makeDesign("DES-HALF"));
    auto tcFull = svc.addEntity(makeTc("TC-FULL"));
    const std::string fullId = full.value().id, halfId = half.value().id, zeroId = zero.value().id;
    const std::string desFullId = desFull.value().id, desHalfId = desHalf.value().id;
    const std::string tcFullId = tcFull.value().id;

    svc.addLink(makeLink(tl::EntityType::Design, desFullId, tl::EntityType::Requirement,
                         fullId, "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, tcFullId, tl::EntityType::Requirement,
                         fullId, "verifies"));
    svc.addLink(makeLink(tl::EntityType::Design, desHalfId, tl::EntityType::Requirement,
                         halfId, "satisfies"));

    engine.defineRule(makeRule("COVERAGE_MIN", {"DO-178C", "DO-254"},
                               {{"min_percent", "100"}}));

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");
    h.check(!violationForEntity(run.value().violations, "COVERAGE_MIN", fullId),
            "COVERAGE_MIN 100 passes REQ-FULL (100%)");
    h.check(violationForEntity(run.value().violations, "COVERAGE_MIN", halfId),
            "COVERAGE_MIN 100 flags REQ-HALF (50%)");
    h.check(violationForEntity(run.value().violations, "COVERAGE_MIN", zeroId),
            "COVERAGE_MIN 100 flags REQ-ZERO (0%)");
}

// ---------------------------------------------------------------------------
// 5. NO_ORPHAN_DESIGN
// ---------------------------------------------------------------------------
void testNoOrphanDesign(Harness& h) {
    h.section("5. NO_ORPHAN_DESIGN");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_orphan.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    auto req = svc.addEntity(makeReq("REQ-O", "Approved"));
    auto linked = svc.addEntity(makeDesign("DES-LINKED"));
    auto orphan = svc.addEntity(makeDesign("DES-ORPHAN"));
    const std::string reqId = req.value().id;
    const std::string linkedId = linked.value().id, orphanId = orphan.value().id;
    svc.addLink(makeLink(tl::EntityType::Design, linkedId, tl::EntityType::Requirement,
                         reqId, "satisfies"));

    engine.defineRule(makeRule("NO_ORPHAN_DESIGN", {"ARP4754A"}));

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");
    h.check(violationForEntity(run.value().violations, "NO_ORPHAN_DESIGN", orphanId),
            "orphan design flagged");
    h.check(!violationForEntity(run.value().violations, "NO_ORPHAN_DESIGN", linkedId),
            "linked design not flagged");
}

// ---------------------------------------------------------------------------
// 6. STATUS_VALID
// ---------------------------------------------------------------------------
void testStatusValid(Harness& h) {
    h.section("6. STATUS_VALID");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_status.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    auto good = svc.addEntity(makeReq("REQ-GOOD", "Verified"));
    const std::string goodId = good.value().id;

    // Insert an entity with an illegal status directly via the DAO.
    p::RequirementDao dao(db);
    p::Requirement bad;
    bad.id = "req-bad";
    bad.name = "REQ-BAD";
    bad.description = "invalid status";
    bad.status = "Bogus";
    dao.create(bad);

    engine.defineRule(makeRule("STATUS_VALID", {"ARP4754A", "DO-178C", "DO-254"}));

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");
    h.check(violationForEntity(run.value().violations, "STATUS_VALID", "req-bad"),
            "STATUS_VALID flags illegal status");
    h.check(!violationForEntity(run.value().violations, "STATUS_VALID", goodId),
            "STATUS_VALID passes legal status");
}

// ---------------------------------------------------------------------------
// 7. WP-3 acceptance
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("7. WP-3 acceptance: unverified requirement triggers REQ_MUST_BE_VERIFIED");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp3_accept.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::RulesEngine engine(db);

    // Graph with an unverified active requirement.
    auto req = svc.addEntity(makeReq("REQ-ACCEPT", "Approved"));
    const std::string reqId = req.value().id;

    engine.defineRule(makeRule("REQ_MUST_BE_VERIFIED", {"ARP4754A", "DO-178C"}));
    engine.defineRule(makeRule("NO_SELF_LINKS", {"ARP4754A"}));  // unrelated, stays silent

    auto run = engine.runValidation();
    h.check(run.isOk(), "runValidation ok");

    const tl::ValidationRun& rr = run.value();
    h.check(rr.violationCount >= 1, "run report reports at least one violation");

    const tl::Violation* hit = violationByRule(rr.violations, "REQ_MUST_BE_VERIFIED");
    h.check(hit != nullptr, "REQ_MUST_BE_VERIFIED appears in the run report");
    h.check(hit != nullptr && hit->entityId == reqId,
            "the flagged entity is the unverified requirement");
    h.check(hit != nullptr && hit->entityExternalId == "REQ-ACCEPT",
            "violation carries the requirement external id");
    h.check(hit != nullptr && hit->ruleId.size() > 8, "violation references a rule id");
    h.check(hit != nullptr && containsStandard(*hit, "DO-178C"),
            "violation carries the rule's assurance standard tag");
    h.check(rr.status == "violations", "run status is 'violations'");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-3 rules engine");
    std::printf("WP-3 RULES ENGINE TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testRuleLifecycle(h);
    testRequirementRules(h);
    testIntegrityRules(h);
    testCoverageMin(h);
    testNoOrphanDesign(h);
    testStatusValid(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
