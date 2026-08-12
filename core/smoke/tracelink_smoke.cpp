// core/smoke/tracelink_smoke.cpp
// Self-verifying smoke path for WP-1 (rich typed domain model + schema
// migrations 003/004).
//
// Exercises: schema migration to v4, rich typed requirement creation with full
// attributes, a typed `verifies` link, and integrity-on-write (rejecting a
// duplicate link, an illegal status transition, a self-loop, and a dangling
// link). Returns non-zero on any failure.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/StateMachine.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace lodestar::tracelink {

namespace {
int failures = 0;

void check(bool ok, const char* what) {
    if (ok) {
        std::printf("  [PASS] %s\n", what);
    } else {
        std::printf("  [FAIL] %s\n", what);
        ++failures;
    }
}
}  // namespace

int runTraceLinkSmoke() {
    std::printf("TRACELINK WP-1 SMOKE\n");

    persistence::Database db;
    auto open = db.open("lodestar_tracelink_smoke.db");
    check(open.isOk(), "open throwaway db");
    if (open.failed()) return 1;

    persistence::MigrationRunner runner(db);
    auto migrated = runner.run(LODESTAR_MIGRATIONS_DIR);
    check(migrated.isOk(), "migration runs");
    if (migrated.failed()) return 1;
    check(migrated.value() == 17, "schema migrated to v17 (001..017 applied)");

    TraceLinkService svc(db);

    // --- Rich typed requirement -------------------------------------------
    Entity req;
    req.type = EntityType::Requirement;
    req.externalId = "REQ-100";
    req.name = "REQ-100 GNSS position output";
    req.text = "The system shall provide GNSS position output at 1 Hz.";
    req.status = "Draft";
    req.typeAttr = "functional";
    req.priority = "High";
    req.source = "Customer";
    req.owner = "alice";
    req.rationale = "Primary navigation output.";
    req.verificationMethod = "Test";
    req.safetyLevel = "Catastrophic";
    req.sortOrder = 1;
    req.tags = "nav,gnss";

    auto addReq = svc.addEntity(req);
    check(addReq.isOk(), "addEntity(requirement) succeeds");
    if (addReq.failed()) return 1;
    Entity storedReq = addReq.value();
    check(!storedReq.id.empty(), "requirement got an internal UUID id");
    check(storedReq.externalId == "REQ-100", "requirement external_id preserved");
    check(storedReq.version == 1, "requirement initial version == 1");
    check(storedReq.priority == "High", "requirement priority preserved");
    check(storedReq.verificationMethod == "Test", "verification_method preserved");
    check(storedReq.safetyLevel == "Catastrophic", "safety_level preserved");
    check(!storedReq.createdAt.empty(), "requirement got a created_at timestamp");

    // --- Typed test case ----------------------------------------------------
    Entity tc;
    tc.type = EntityType::TestCase;
    tc.externalId = "TC-100";
    tc.name = "TC-100 Position accuracy";
    tc.text = "Verify GNSS position output accuracy.";
    tc.status = "Draft";
    tc.verificationMethod = "Analysis";
    auto addTc = svc.addEntity(tc);
    check(addTc.isOk(), "addEntity(test_case) succeeds");
    if (addTc.failed()) return 1;
    Entity storedTc = addTc.value();

    // --- Typed link: test_case verifies requirement -------------------------
    Link link;
    link.sourceType = EntityType::TestCase;
    link.sourceId = storedTc.id;
    link.targetType = EntityType::Requirement;
    link.targetId = storedReq.id;
    link.relation = "verifies";
    link.rationale = "Dynamic accuracy test proves the requirement.";
    link.status = "Active";

    auto addLink = svc.addLink(link);
    check(addLink.isOk(), "addLink(verifies) succeeds");
    if (addLink.failed()) return 1;
    check(addLink.value().status == "Active", "link status Active");
    check(addLink.value().rationale == "Dynamic accuracy test proves the requirement.",
          "link rationale preserved");

    // Link round-trips through both directions.
    auto toReq = svc.linksTo(EntityType::Requirement, storedReq.id);
    check(toReq.isOk() && toReq.value().size() == 1, "linksTo(requirement) finds 1 link");
    auto fromTc = svc.linksFrom(EntityType::TestCase, storedTc.id);
    check(fromTc.isOk() && fromTc.value().size() == 1, "linksFrom(test_case) finds 1 link");

    // --- Reject a duplicate link -------------------------------------------
    Link dup = link;
    auto dupResult = svc.addLink(dup);
    check(dupResult.failed(), "rejects duplicate link");
    if (dupResult.isOk()) return 1;

    // --- Reject an illegal status transition (Draft -> Verified) ------------
    Entity badStatus = storedReq;
    badStatus.status = "Verified";
    auto badTransition = svc.updateEntity(badStatus);
    check(badTransition.failed(), "rejects illegal Draft -> Verified transition");

    // Legal transition (Draft -> Proposed) succeeds and bumps version.
    auto okTransition = svc.transition(EntityType::Requirement, storedReq.id, "Proposed");
    check(okTransition.isOk(), "accepts legal Draft -> Proposed transition");
    auto got = svc.getEntity(EntityType::Requirement, storedReq.id);
    check(got.isOk() && got.value().has_value() && got.value()->status == "Proposed",
          "status persisted after transition");

    // --- Reject a self-loop --------------------------------------------------
    Link selfLoop;
    selfLoop.sourceType = EntityType::Requirement;
    selfLoop.sourceId = storedReq.id;
    selfLoop.targetType = EntityType::Requirement;
    selfLoop.targetId = storedReq.id;
    selfLoop.relation = "derives";
    auto selfResult = svc.addLink(selfLoop);
    check(selfResult.failed(), "rejects self-loop link");

    // --- Reject a dangling link (target does not exist) ----------------------
    Link dangling;
    dangling.sourceType = EntityType::TestCase;
    dangling.sourceId = storedTc.id;
    dangling.targetType = EntityType::Requirement;
    dangling.targetId = "no-such-id";
    dangling.relation = "verifies";
    auto danglingResult = svc.addLink(dangling);
    check(danglingResult.failed(), "rejects dangling (nonexistent target) link");

    // --- Canonical relation map + reverse mapping ----------------------------
    check(reverseRelationName(Relation::Verifies) == "is_verified_by",
          "reverse relation map: verifies -> is_verified_by");
    check(isRelationAllowed(EntityType::TestCase, EntityType::Requirement,
                            Relation::Verifies),
          "verifies is allowed test_case -> requirement");
    check(!isRelationAllowed(EntityType::Requirement, EntityType::TestCase,
                             Relation::Verifies),
          "verifies is not allowed requirement -> test_case");

    // --- Soft delete marks Obsolete -----------------------------------------
    auto removed = svc.removeEntity(EntityType::Requirement, storedReq.id);
    check(removed.isOk(), "soft delete requirement ok");
    auto afterDelete = svc.getEntity(EntityType::Requirement, storedReq.id);
    check(afterDelete.isOk() && afterDelete.value().has_value() &&
              afterDelete.value()->status == "Obsolete",
          "soft delete marks requirement Obsolete");

    db.close();
    std::remove("lodestar_tracelink_smoke.db");
    std::remove("lodestar_tracelink_smoke.db-wal");
    std::remove("lodestar_tracelink_smoke.db-shm");

    if (failures == 0) {
        std::printf("TRACELINK WP-1 SMOKE OK\n");
        return 0;
    }
    std::printf("TRACELINK WP-1 SMOKE FAILED: %d check(s)\n", failures);
    return 1;
}

}  // namespace lodestar::tracelink
