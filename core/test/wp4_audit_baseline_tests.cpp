// core/test/wp4_audit_baseline_tests.cpp
// ---------------------------------------------------------------------------
// WP-4 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-4 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-4 / section 4.5 / 7.4 / 3.3 / 3.4):
//   1. Audit on every mutation (create / update / soft_delete / link ops)
//   2. BaselineService create / list / get
//   3. diffBaseline (field-level added/removed/modified)
//   4. history (ordered per-entity audit trail)
//   5. entityAtBaseline (reconstruct an entity as it was)
//   6. changeImpact (audit entries tagged to a change + downstream entities)
//   7. WP-4 acceptance: modify an entity -> two baselines -> diff shows the
//      exact field change; history lists every action.
//
// Uses the same lightweight self-contained harness as WP-1..WP-3.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-4 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Audit: `TraceLinkService` (core/tracelink/TraceLinkService.h) gains an
//     optional audit context, and EVERY mutation writes an audit_log row.
//
//   // Stamps actor + changeRequestId on subsequent mutations until reset.
//   void setAuditContext(const std::string& actor, const std::string& changeRequestId);
//
//   Audit rows are written with:
//     entity_type/entity_id = the mutated entity, or "link"/<linkId> for link ops;
//     action = "create" | "update" | "soft_delete" | "add_link" |
//              "update_link" | "remove_link";
//     field / old_value / new_value = set for "update" (the changed field only).
//   Every mutation writes an audit row regardless of context (context may be empty).
//
// ---------------------------------------------------------------------------
// (B) BaselineService (core/tracelink/BaselineService.h,
//     namespace lodestar::tracelink). Reuses EntityType/Entity/Link/GraphNode.
//
// namespace lodestar::tracelink {
//
// struct AuditEntry {
//     std::string id;
//     std::string entityType;      // "requirement" | "design" | ... | "link"
//     std::string entityId;
//     std::string action;          // create/update/soft_delete/add_link/...
//     std::string field;           // set only for field-level updates
//     std::string oldValue;
//     std::string newValue;
//     std::string actor;
//     std::string timestamp;
//     std::string changeRequestId;
// };
//
// struct Baseline {
//     std::string id;
//     std::string name;
//     std::string description;
//     std::string createdAt;
// };
//
// struct FieldChange { std::string field; std::string oldValue; std::string newValue; };
//
// enum class DiffKind { Added, Removed, Modified };
//
// struct DiffEntry {
//     DiffKind kind;
//     EntityType entityType;
//     std::string entityId;
//     std::string entityExternalId;
//     std::vector<FieldChange> fieldChanges;   // non-empty for Modified only
// };
//
// struct DiffResult {
//     std::vector<DiffEntry> entities;
//     std::vector<DiffEntry> links;
// };
//
// struct ImpactResult {
//     std::vector<AuditEntry> changes;            // audit rows tagged to the change
//     std::vector<GraphNode> downstreamAffected;  // downstream closure of the entity
// };
//
// class BaselineService {
// public:
//     explicit BaselineService(persistence::Database& db);
//
//     // Audit read
//     common::Result<std::vector<AuditEntry>> history(EntityType type, const std::string& id);
//     common::Result<std::vector<AuditEntry>> allHistory();
//
//     // Baselines
//     common::Result<Baseline> createBaseline(const std::string& name, const std::string& description);
//     common::Result<std::vector<Baseline>> listBaselines();
//     common::Result<std::optional<Baseline>> getBaseline(const std::string& id);
//
//     // Diff a(older) against b(newer). Added/Removed/Modified for entities and links.
//     common::Result<DiffResult> diffBaseline(const std::string& aId, const std::string& bId);
//
//     // Reconstruct the entity exactly as snapshotted in a baseline.
//     common::Result<std::optional<Entity>>
//         entityAtBaseline(EntityType type, const std::string& id, const std::string& baselineId);
//
//     // Audit entries tagged to a change request + downstream affected entities.
//     common::Result<ImpactResult>
//         changeImpact(EntityType type, const std::string& id, const std::string& changeRequestId);
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
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/GraphEngine.h"
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

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
bool hasAction(const std::vector<tl::AuditEntry>& entries, const std::string& action) {
    for (const auto& e : entries) {
        if (e.action == action) return true;
    }
    return false;
}

const tl::AuditEntry* entryFor(const std::vector<tl::AuditEntry>& entries,
                               const std::string& action, const std::string& entityId) {
    for (const auto& e : entries) {
        if (e.action == action && e.entityId == entityId) return &e;
    }
    return nullptr;
}

bool hasFieldAudit(const std::vector<tl::AuditEntry>& entries, const std::string& action,
                   const std::string& entityId, const std::string& field,
                   const std::string& oldV, const std::string& newV) {
    for (const auto& e : entries) {
        if (e.action == action && e.entityId == entityId && e.field == field &&
            e.oldValue == oldV && e.newValue == newV) return true;
    }
    return false;
}

bool hasFieldChange(const tl::DiffEntry& e, const std::string& field,
                    const std::string& oldV, const std::string& newV) {
    for (const auto& f : e.fieldChanges) {
        if (f.field == field && f.oldValue == oldV && f.newValue == newV) return true;
    }
    return false;
}

const tl::DiffEntry* entityDiffFor(const std::vector<tl::DiffEntry>& entries,
                                   const std::string& entityId) {
    for (const auto& e : entries) {
        if (e.entityId == entityId) return &e;
    }
    return nullptr;
}

bool containsNodeId(const std::vector<tl::GraphNode>& nodes, const std::string& id) {
    for (const auto& n : nodes) {
        if (n.id == id) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 1. Audit on every mutation
// ---------------------------------------------------------------------------
void testAuditOnMutation(Harness& h) {
    h.section("1. Audit writes on every mutation");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_audit.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);

    auto req = svc.addEntity(makeReq("REQ-A"));
    const std::string reqId = req.value().id;

    auto hist1 = bl.history(tl::EntityType::Requirement, reqId);
    h.check(hist1.isOk() && hasAction(hist1.value(), "create"),
            "addEntity writes a 'create' audit entry");

    // update a field
    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    auto upd = *got.value();
    upd.name = "REQ-A2";
    upd.status = "Approved";
    h.check(svc.updateEntity(upd).isOk(), "updateEntity ok");
    auto hist2 = bl.history(tl::EntityType::Requirement, reqId);
    h.check(hasAction(hist2.value(), "update"), "updateEntity writes an 'update' audit entry");
    h.check(hasFieldAudit(hist2.value(), "update", reqId, "name", "REQ-A", "REQ-A2"),
            "update audit records exact field change (name)");
    h.check(hasFieldAudit(hist2.value(), "update", reqId, "status", "Draft", "Approved"),
            "update audit records exact field change (status)");

    // link operation is audited (entity_type 'link')
    auto tc = svc.addEntity(makeTc("TC-A"));
    const std::string tcId = tc.value().id;
    auto link = svc.addLink(makeLink(tl::EntityType::TestCase, tcId,
                                     tl::EntityType::Requirement, reqId, "verifies"));
    h.check(link.isOk(), "addLink ok");
    auto all = bl.allHistory();
    bool linkAudited = false;
    for (const auto& e : all.value()) {
        if (e.entityType == "link" && e.action == "add_link") linkAudited = true;
    }
    h.check(linkAudited, "addLink writes an 'add_link' audit entry");

    // soft delete is audited
    h.check(svc.removeEntity(tl::EntityType::Requirement, reqId).isOk(), "removeEntity ok");
    auto hist3 = bl.history(tl::EntityType::Requirement, reqId);
    h.check(hasAction(hist3.value(), "soft_delete"), "removeEntity writes 'soft_delete' audit");
}

// ---------------------------------------------------------------------------
// 2. Baseline lifecycle
// ---------------------------------------------------------------------------
void testBaselineLifecycle(Harness& h) {
    h.section("2. BaselineService create / list / get");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_baseline.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::BaselineService bl(db);

    auto b1 = bl.createBaseline("BL1", "first snapshot");
    h.check(b1.isOk() && !b1.value().id.empty(), "createBaseline assigns an id");
    auto b2 = bl.createBaseline("BL2", "second snapshot");
    h.check(b2.isOk(), "second createBaseline ok");

    auto list = bl.listBaselines();
    h.check(list.isOk() && list.value().size() == 2, "listBaselines returns 2 baselines");

    auto got = bl.getBaseline(b1.value().id);
    h.check(got.isOk() && got.value().has_value(), "getBaseline finds BL1");
    h.check(got.value()->name == "BL1" && got.value()->description == "first snapshot",
            "getBaseline returns name + description");
}

// ---------------------------------------------------------------------------
// 7. WP-4 acceptance: modify -> two baselines -> diff + history
// ---------------------------------------------------------------------------
void testDiffAndHistory(Harness& h) {
    h.section("7. WP-4 acceptance: diff shows exact field change; history lists every action");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_accept.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);

    auto e = makeReq("REQ-1", "Draft");
    e.text = "v1";
    auto req = svc.addEntity(e);
    const std::string reqId = req.value().id;

    // Baseline A: before the change.
    auto a = bl.createBaseline("A", "before");

    // Modify the entity (two fields: text and status).
    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    auto upd = *got.value();
    upd.text = "v2";
    upd.status = "Approved";
    h.check(svc.updateEntity(upd).isOk(), "updateEntity ok");

    // Baseline B: after the change.
    auto b = bl.createBaseline("B", "after");

    // diff shows exactly the modified fields.
    auto diff = bl.diffBaseline(a.value().id, b.value().id);
    h.check(diff.isOk(), "diffBaseline ok");
    const tl::DiffEntry* de = entityDiffFor(diff.value().entities, reqId);
    h.check(de != nullptr && de->kind == tl::DiffKind::Modified,
            "REQ-1 diff kind = Modified");
    h.check(de != nullptr && hasFieldChange(*de, "text", "v1", "v2"),
            "diff shows exact text change v1 -> v2");
    h.check(de != nullptr && hasFieldChange(*de, "status", "Draft", "Approved"),
            "diff shows exact status change Draft -> Approved");

    // history lists every action in order (create, then update).
    auto hist = bl.history(tl::EntityType::Requirement, reqId);
    h.check(hist.isOk(), "history ok");
    const tl::AuditEntry* create = entryFor(hist.value(), "create", reqId);
    const tl::AuditEntry* update = entryFor(hist.value(), "update", reqId);
    h.check(create != nullptr && update != nullptr,
            "history lists both create and update actions");
}

// ---------------------------------------------------------------------------
// 5. entityAtBaseline
// ---------------------------------------------------------------------------
void testEntityAtBaseline(Harness& h) {
    h.section("5. entityAtBaseline reconstructs entity as it was");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_atbase.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);

    auto e = makeReq("REQ-X", "Draft");
    e.text = "v1";
    auto req = svc.addEntity(e);
    const std::string reqId = req.value().id;
    auto a = bl.createBaseline("A", "before");

    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    auto upd = *got.value();
    upd.text = "changed";
    upd.status = "Approved";
    svc.updateEntity(upd);
    auto b = bl.createBaseline("B", "after");

    auto atA = bl.entityAtBaseline(tl::EntityType::Requirement, reqId, a.value().id);
    auto atB = bl.entityAtBaseline(tl::EntityType::Requirement, reqId, b.value().id);
    h.check(atA.isOk() && atA.value().has_value(), "entityAtBaseline(A) returns the entity");
    h.check(atA.value()->text == "v1" && atA.value()->status == "Draft",
            "at baseline A the entity holds its original values");
    h.check(atB.value()->text == "changed" && atB.value()->status == "Approved",
            "at baseline B the entity holds the modified values");
}

// ---------------------------------------------------------------------------
// 6. changeImpact
// ---------------------------------------------------------------------------
void testChangeImpact(Harness& h) {
    h.section("6. changeImpact (audit entries tagged to a change + downstream)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_impact.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::BaselineService bl(db);

    // Tag all mutations to change request CR-1.
    svc.setAuditContext("alice", "CR-1");
    auto req = svc.addEntity(makeReq("REQ-CR", "Draft"));
    const std::string reqId = req.value().id;
    auto des = svc.addEntity(makeDesign("DES-CR"));
    const std::string desId = des.value().id;
    // allocates: requirement -> design, so DES-CR is DOWNSTREAM of REQ-CR.
    svc.addLink(makeLink(tl::EntityType::Requirement, reqId, tl::EntityType::Design,
                         desId, "allocates"));
    auto got = svc.getEntity(tl::EntityType::Requirement, reqId);
    auto upd = *got.value();
    upd.text = "updated";
    svc.updateEntity(upd);
    svc.setAuditContext("", "");  // reset

    auto impact = bl.changeImpact(tl::EntityType::Requirement, reqId, "CR-1");
    h.check(impact.isOk(), "changeImpact ok");

    const tl::ImpactResult& r = impact.value();
    h.check(!r.changes.empty(), "changeImpact returns tagged audit entries");
    bool allTagged = true;
    for (const auto& e : r.changes) {
        if (e.changeRequestId != "CR-1") allTagged = false;
    }
    h.check(allTagged, "every returned entry is tagged CR-1");
    h.check(entryFor(r.changes, "create", reqId) != nullptr,
            "tagged entries include the create action for REQ-CR");
    h.check(entryFor(r.changes, "update", reqId) != nullptr,
            "tagged entries include the update action for REQ-CR");
    h.check(containsNodeId(r.downstreamAffected, desId),
            "downstream affected includes the allocated design DES-CR");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-4 audit + baseline");
    std::printf("WP-4 AUDIT + BASELINE TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testAuditOnMutation(h);
    testBaselineLifecycle(h);
    testDiffAndHistory(h);
    testEntityAtBaseline(h);
    testChangeImpact(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
