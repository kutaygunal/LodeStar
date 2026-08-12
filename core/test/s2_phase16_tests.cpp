// core/test/s2_phase16_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 16 tests (test contract): Variants / branching.
//
// Written by the scrum-master BEFORE the Phase 16 engineer implements the
// feature. The engineer must implement the contract below so these tests
// compile and pass. Do NOT weaken the assertions; implement the feature to
// satisfy them.
//
// Covers (PLAN.md, S2 Phase 16):
//   (A) Variant model: createVariant / addToVariant / removeFromVariant.
//   (B) Branching: createBranch / mergeBranch with conflict detection.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each DB-dependent test opens its own fresh throwaway DB.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 16 engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 025 (core/persistence/migrations/025_variants.sql) adds the
//     `variants`, `variant_requirements`, `variant_branches` and
//     `branch_requirements` tables.
//
// (B) New VariantService (core/tracelink/VariantService.h):
//
//   struct Variant { std::string id; std::string name; std::string createdAt; };
//   struct VariantBranch { std::string id; std::string baseVariantId;
//                          std::string name; std::string createdAt; };
//   enum class MergeStatus { Merged, Conflict };
//   struct MergeResult { MergeStatus status; std::vector<std::string> conflicts; };
//
//   class VariantService {
//   public:
//       explicit VariantService(persistence::Database& db);
//       common::Result<Variant> createVariant(const std::string& name);
//       common::Result<void> addToVariant(const std::string& variantId,
//                                        const std::string& requirementId);
//       common::Result<void> removeFromVariant(const std::string& variantId,
//                                              const std::string& requirementId);
//       common::Result<bool> variantContains(const std::string& variantId,
//                                             const std::string& requirementId);
//       common::Result<VariantBranch> createBranch(const std::string& baseVariantId,
//                                                  const std::string& name);
//       common::Result<void> addToBranch(const std::string& branchId,
//                                        const std::string& requirementId);
//       common::Result<void> removeFromBranch(const std::string& branchId,
//                                             const std::string& requirementId);
//       common::Result<MergeResult> mergeBranch(const std::string& branchId,
//                                               const std::string& targetVariantId);
//   };
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/VariantService.h"

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

// Seeds a requirement and returns its id (or "" on failure).
std::string seedRequirement(tl::TraceLinkService& tls, const char* extId,
                            const char* text) {
    tl::Entity r;
    r.externalId = extId;
    r.type = tl::EntityType::Requirement;
    r.name = extId;
    r.text = text;
    r.status = "Draft";
    auto added = tls.addEntity(r);
    return added.isOk() ? added.value().id : std::string();
}

// ---------------------------------------------------------------------------
// T1. createVariant + addToVariant
// ---------------------------------------------------------------------------
void testCreateAndAdd(Harness& h) {
    h.section("T1. createVariant + addToVariant");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p16_add.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::VariantService vs(db);

    auto pro = vs.createVariant("Pro");
    h.check(pro.isOk(), "createVariant(Pro) succeeds");
    h.check(pro.isOk() && !pro.value().id.empty(), "Pro has a non-empty id");
    h.check(pro.isOk() && pro.value().name == "Pro", "Pro name set");

    std::string reqId = seedRequirement(tls, "REQ-P", "Pro requirement");
    h.check(!reqId.empty(), "seed a requirement");

    auto add = vs.addToVariant(pro.value().id, reqId);
    h.check(add.isOk(), "addToVariant(proId, reqId) succeeds");

    auto contains = vs.variantContains(pro.value().id, reqId);
    h.check(contains.isOk() && contains.value(),
            "variant contains the requirement after add");

    db.close();
    std::remove("lodestar_s2p16_add.db");
    std::remove("lodestar_s2p16_add.db-wal");
    std::remove("lodestar_s2p16_add.db-shm");
}

// ---------------------------------------------------------------------------
// T2. removeFromVariant
// ---------------------------------------------------------------------------
void testRemove(Harness& h) {
    h.section("T2. removeFromVariant");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p16_remove.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::VariantService vs(db);

    auto pro = vs.createVariant("Pro");
    h.check(pro.isOk(), "createVariant(Pro)");

    std::string reqId = seedRequirement(tls, "REQ-R", "R");
    h.check(!reqId.empty(), "seed a requirement");

    auto add = vs.addToVariant(pro.value().id, reqId);
    h.check(add.isOk(), "addToVariant(proId, reqId)");

    auto before = vs.variantContains(pro.value().id, reqId);
    h.check(before.isOk() && before.value(), "contains before remove");

    auto rem = vs.removeFromVariant(pro.value().id, reqId);
    h.check(rem.isOk(), "removeFromVariant(proId, reqId) succeeds");

    auto after = vs.variantContains(pro.value().id, reqId);
    h.check(after.isOk() && !after.value(),
            "variant no longer contains the requirement after remove");

    db.close();
    std::remove("lodestar_s2p16_remove.db");
    std::remove("lodestar_s2p16_remove.db-wal");
    std::remove("lodestar_s2p16_remove.db-shm");
}

// ---------------------------------------------------------------------------
// T3. createBranch
// ---------------------------------------------------------------------------
void testCreateBranch(Harness& h) {
    h.section("T3. createBranch");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p16_branch.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::VariantService vs(db);

    auto base = vs.createVariant("Base");
    h.check(base.isOk(), "createVariant(Base)");

    std::string reqId = seedRequirement(tls, "REQ-B", "B");
    h.check(!reqId.empty(), "seed a requirement");
    auto add = vs.addToVariant(base.value().id, reqId);
    h.check(add.isOk(), "addToVariant(base, req)");

    auto br = vs.createBranch(base.value().id, "feature-x");
    h.check(br.isOk(), "createBranch(baseId, feature-x) succeeds");
    h.check(br.isOk() && !br.value().id.empty(), "branch has a non-empty id");
    h.check(br.isOk() && br.value().baseVariantId == base.value().id,
            "branch points at the base variant");
    h.check(br.isOk() && br.value().name == "feature-x", "branch name set");

    db.close();
    std::remove("lodestar_s2p16_branch.db");
    std::remove("lodestar_s2p16_branch.db-wal");
    std::remove("lodestar_s2p16_branch.db-shm");
}

// ---------------------------------------------------------------------------
// T4. mergeBranch detects conflict
// ---------------------------------------------------------------------------
void testMergeConflict(Harness& h) {
    h.section("T4. mergeBranch detects conflict");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s2p16_merge.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::VariantService vs(db);

    auto base = vs.createVariant("Base");
    h.check(base.isOk(), "createVariant(Base)");

    std::string reqId = seedRequirement(tls, "REQ-M", "M");
    h.check(!reqId.empty(), "seed a requirement");
    auto add = vs.addToVariant(base.value().id, reqId);
    h.check(add.isOk(), "addToVariant(base, req)");

    auto br = vs.createBranch(base.value().id, "feature-x");
    h.check(br.isOk(), "createBranch(base, feature-x)");
    std::string branchId = br.value().id;

    // Change the same requirement differently on the branch and the target.
    // Branch removes it; the target keeps it (bumps its version again).
    auto brChange = vs.removeFromBranch(branchId, reqId);
    h.check(brChange.isOk(), "removeFromBranch(branch, req)");
    auto tgtChange = vs.addToVariant(base.value().id, reqId);
    h.check(tgtChange.isOk(), "addToVariant(base, req) again (target changed)");

    auto merge = vs.mergeBranch(branchId, base.value().id);
    h.check(merge.isOk(), "mergeBranch(branchId, baseId) succeeds");
    h.check(merge.isOk() && merge.value().status == tl::MergeStatus::Conflict,
            "merge returns a Conflict result");
    h.check(merge.isOk() && merge.value().status == tl::MergeStatus::Conflict &&
                std::find(merge.value().conflicts.begin(),
                          merge.value().conflicts.end(), reqId) !=
                    merge.value().conflicts.end(),
            "conflict lists the requirement");

    // The target must NOT be silently overwritten by the branch's removal.
    auto contains = vs.variantContains(base.value().id, reqId);
    h.check(contains.isOk() && contains.value(),
            "target still contains the requirement (not overwritten)");

    db.close();
    std::remove("lodestar_s2p16_merge.db");
    std::remove("lodestar_s2p16_merge.db-wal");
    std::remove("lodestar_s2p16_merge.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 16 Variants / branching");
    std::printf("S2 PHASE 16 TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testCreateAndAdd(h);
    testRemove(h);
    testCreateBranch(h);
    testMergeConflict(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
