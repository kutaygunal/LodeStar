// core/test/wpB_tests.cpp
// ---------------------------------------------------------------------------
// WP-B unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-B engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-B):
//   A3. Baseline restore / rollback.
//   A4. Change-request + review workflow (approve/reject, review queues,
//       link CRs to audit).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-B engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 010 (core/persistence/migrations/010_*.sql) creates a
//     `change_requests` table (append-only, idempotent):
//       id, title, description, status, entity_type, entity_id,
//       proposed_change (JSON), created_by, created_at, reviewed_by,
//       reviewed_at, review_comment.
//
// (B) BaselineService addition (core/tracelink/BaselineService.h):
//
//   // Restores the current database to the exact state captured in
//   // `baselineId` (entity fields/status/version and the link set are
//   // reverted to the snapshot). Returns the number of entities restored.
//   common::Result<int> restoreBaseline(const std::string& baselineId);
//
// (C) New ChangeRequestService (core/tracelink/ChangeRequestService.h):
//
//   struct ChangeRequest {
//       std::string id;
//       std::string title;
//       std::string description;
//       std::string status;          // Open | InReview | Approved | Rejected | Implemented
//       std::string entityType;      // "requirement" | "design" | ...
//       std::string entityId;
//       std::string proposedChange;  // JSON of proposed field changes, e.g. {"name":"X"}
//       std::string createdBy;
//       std::string createdAt;
//       std::string reviewedBy;
//       std::string reviewedAt;
//       std::string reviewComment;
//   };
//
//   class ChangeRequestService {
//   public:
//       explicit ChangeRequestService(persistence::Database& db);
//
//       // Creates a CR in status "Open". Assigns a UUID if id is empty.
//       common::Result<ChangeRequest> create(const ChangeRequest& cr);
//
//       // All CRs awaiting review (status Open or InReview), newest first.
//       common::Result<std::vector<ChangeRequest>> reviewQueue();
//
//       // Open -> InReview.
//       common::Result<ChangeRequest> submitForReview(const std::string& id);
//
//       // InReview -> Approved (records reviewer + comment).
//       common::Result<ChangeRequest> approve(const std::string& id,
//                                              const std::string& reviewer,
//                                              const std::string& comment);
//
//       // InReview -> Rejected (records reviewer + comment).
//       common::Result<ChangeRequest> reject(const std::string& id,
//                                            const std::string& reviewer,
//                                            const std::string& comment);
//
//       // Applies an APPROVED CR's proposed change to the target entity,
//       // stamping every audit row with the CR id. Marks the CR "Implemented".
//       // Fails if the CR is not Approved. Returns the updated entity.
//       common::Result<Entity> applyChangeRequest(const std::string& crId);
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
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/ChangeRequestService.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

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

tl::Entity makeReq(const std::string& extId, const std::string& name) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = name;
    e.text = "Body of " + extId;
    e.status = "Draft";
    return e;
}

// ---------------------------------------------------------------------------
// A3. Baseline restore / rollback
// ---------------------------------------------------------------------------
void testRestoreBaseline(Harness& h) {
    h.section("A3. Baseline restore / rollback");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpB_restore.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService base(db);

    auto r = svc.addEntity(makeReq("REQ-R1", "Original"));
    h.check(r.isOk(), "seed entity ok");
    const std::string reqId = r.value().id;

    // Baseline A captures the "Original" state.
    auto bA = base.createBaseline("A", "original");
    h.check(bA.isOk(), "create baseline A ok");
    const std::string aId = bA.value().id;

    // Modify the entity.
    auto upd = svc.updateEntity([&] {
        tl::Entity e = r.value();
        e.name = "Changed";
        e.status = "Proposed";
        return e;
    }());
    h.check(upd.isOk(), "update entity ok");

    // Baseline B captures the "Changed" state.
    auto bB = base.createBaseline("B", "changed");
    h.check(bB.isOk(), "create baseline B ok");

    // Restore to A -> entity reverts to "Original".
    auto restored = base.restoreBaseline(aId);
    h.check(restored.isOk(), "restoreBaseline(A) ok");
    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    h.check(got.isOk() && got.value().has_value() &&
                got.value()->name == "Original",
            "entity name reverted to baseline A value");
    h.check(got.value()->status == "Draft",
            "entity status reverted to baseline A value");

    // Restore to B -> entity reverts to "Changed".
    auto restoredB = base.restoreBaseline(bB.value().id);
    h.check(restoredB.isOk(), "restoreBaseline(B) ok");
    auto gotB = svc.getEntity(tl::EntityType::Requirement, reqId);
    h.check(gotB.isOk() && gotB.value().has_value() &&
                gotB.value()->name == "Changed",
            "entity name reverted to baseline B value");

    // Restore to a nonexistent baseline fails.
    auto bad = base.restoreBaseline("does-not-exist");
    h.check(bad.failed(), "restoreBaseline to missing baseline fails");

    db.close();
    std::remove("lodestar_wpB_restore.db");
    std::remove("lodestar_wpB_restore.db-wal");
    std::remove("lodestar_wpB_restore.db-shm");
}

// ---------------------------------------------------------------------------
// A4. Change-request + review workflow
// ---------------------------------------------------------------------------
void testChangeRequestWorkflow(Harness& h) {
    h.section("A4. Change-request + review workflow");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpB_cr.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService base(db);
    tl::ChangeRequestService crs(db);

    auto r = svc.addEntity(makeReq("REQ-CR1", "Before"));
    h.check(r.isOk(), "seed entity ok");
    const std::string reqId = r.value().id;

    // Create a change request proposing a name change.
    tl::ChangeRequest cr;
    cr.title = "Rename REQ-CR1";
    cr.description = "Propose a new name.";
    cr.entityType = "requirement";
    cr.entityId = reqId;
    cr.proposedChange = "{\"name\":\"After\"}";
    cr.createdBy = "engineer";
    auto created = crs.create(cr);
    h.check(created.isOk(), "create change request ok");
    const std::string crId = created.value().id;
    h.check(created.value().status == "Open", "new CR starts in Open status");

    // It appears in the review queue.
    auto q0 = crs.reviewQueue();
    h.check(q0.isOk() && q0.value().size() == 1, "review queue has the new CR");

    // Submit for review.
    auto submitted = crs.submitForReview(crId);
    h.check(submitted.isOk() && submitted.value().status == "InReview",
            "submitForReview -> InReview");

    // Approve.
    auto approved = crs.approve(crId, "reviewer", "looks good");
    h.check(approved.isOk() && approved.value().status == "Approved",
            "approve -> Approved");
    h.check(approved.value().reviewedBy == "reviewer",
            "approve records the reviewer");

    // Apply the approved change.
    auto applied = crs.applyChangeRequest(crId);
    h.check(applied.isOk(), "applyChangeRequest ok");
    h.check(applied.value().name == "After", "entity name updated by CR");

    // The CR is now Implemented and out of the review queue.
    auto q1 = crs.reviewQueue();
    h.check(q1.isOk() && q1.value().empty(), "implemented CR leaves the review queue");

    // Audit rows for the entity carry the CR id (link CR to audit).
    auto hist = base.history(tl::EntityType::Requirement, reqId);
    h.check(hist.isOk(), "history ok");
    bool linked = false;
    for (const auto& a : hist.value()) {
        if (a.changeRequestId == crId) linked = true;
    }
    h.check(linked, "audit rows are linked to the change request id");

    // Reject path: a second CR is rejected and cannot be applied.
    tl::ChangeRequest cr2;
    cr2.title = "Reject me";
    cr2.entityType = "requirement";
    cr2.entityId = reqId;
    cr2.proposedChange = "{\"name\":\"Nope\"}";
    cr2.createdBy = "engineer";
    auto created2 = crs.create(cr2);
    h.check(created2.isOk(), "create second CR ok");
    const std::string cr2Id = created2.value().id;
    h.check(crs.submitForReview(cr2Id).isOk(), "submit second CR ok");
    auto rejected = crs.reject(cr2Id, "reviewer", "not needed");
    h.check(rejected.isOk() && rejected.value().status == "Rejected",
            "reject -> Rejected");
    auto applyRejected = crs.applyChangeRequest(cr2Id);
    h.check(applyRejected.failed(), "applyChangeRequest on rejected CR fails");

    // Applying a nonexistent CR fails.
    auto applyMissing = crs.applyChangeRequest("does-not-exist");
    h.check(applyMissing.failed(), "applyChangeRequest on missing CR fails");

    db.close();
    std::remove("lodestar_wpB_cr.db");
    std::remove("lodestar_wpB_cr.db-wal");
    std::remove("lodestar_wpB_cr.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-B change management");
    std::printf("WP-B CHANGE MANAGEMENT TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testRestoreBaseline(h);
    testChangeRequestWorkflow(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
