// core/test/t3_variant_override_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TraceLink 3.3: variant / module reuse (attribute inheritance/override).
//
// Test contract: docs/gap-fill-plan.md (Module 3.3).
//   (A) Migration 034 creates variant_attribute_override.
//   (B) VariantService models variants with inheritance/override of requirement
//       attributes: without an override a variant inherits the base value; with
//       one the variant value wins; clearing restores inheritance.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/VariantService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

bool tableExists(p::Database& db, const std::string& table) {
    return db.queryScalar(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
        table + "';") == "1";
}

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

void closeAndRemove(p::Database& db, const char* file) {
    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. Inherit base value when no override; override wins; clear restores
// ---------------------------------------------------------------------------
void testOverride(Harness& h) {
    h.section("T1. inheritance + override + clear");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t3_ovr.db")) {
        h.check(false, "open fresh db");
        return;
    }
    h.check(tableExists(db, "variant_attribute_override"),
            "variant_attribute_override table exists");

    tl::VariantService svc(db);
    auto var = svc.createVariant("Pro");
    h.check(var.isOk(), "createVariant() ok");
    if (!var.isOk()) { closeAndRemove(db, "lodestar_t3_ovr.db"); return; }
    const std::string vid = var.value().id;

    // No override -> inherit the base value.
    auto inherited = svc.effectiveAttribute(vid, "REQ-1", "priority", "Medium");
    h.check(inherited.isOk() && inherited.value() == "Medium",
            "no override -> inherits base value \"Medium\"");

    // Set an override -> the variant value wins.
    auto set = svc.setAttributeOverride(vid, "REQ-1", "priority", "High");
    h.check(set.isOk(), "setAttributeOverride() ok");
    auto overridden = svc.effectiveAttribute(vid, "REQ-1", "priority", "Medium");
    h.check(overridden.isOk() && overridden.value() == "High",
            "override wins: effective priority == \"High\"");

    // Another requirement without override still inherits.
    auto other = svc.effectiveAttribute(vid, "REQ-2", "priority", "Low");
    h.check(other.isOk() && other.value() == "Low",
            "different requirement without override inherits \"Low\"");

    // Clear the override -> inherit again.
    auto clear = svc.clearAttributeOverride(vid, "REQ-1", "priority");
    h.check(clear.isOk(), "clearAttributeOverride() ok");
    auto restored = svc.effectiveAttribute(vid, "REQ-1", "priority", "Medium");
    h.check(restored.isOk() && restored.value() == "Medium",
            "after clear, inherits base value again");

    // Validation: empty args rejected.
    auto bad = svc.setAttributeOverride("", "REQ-1", "priority", "High");
    h.check(bad.failed(), "setAttributeOverride() with empty variant rejected");

    closeAndRemove(db, "lodestar_t3_ovr.db");
}

// ---------------------------------------------------------------------------
// T2. Overrides are per-variant (reusable module in multiple variants)
// ---------------------------------------------------------------------------
void testPerVariant(Harness& h) {
    h.section("T2. overrides are per-variant (reusable module)");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t3_pv.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::VariantService svc(db);
    auto base = svc.createVariant("Base");
    auto pro = svc.createVariant("Pro");
    if (!base.isOk() || !pro.isOk()) {
        h.check(false, "createVariant() ok");
        closeAndRemove(db, "lodestar_t3_pv.db");
        return;
    }

    // A reusable requirement REQ-10 (module) in both variants.
    h.check(svc.addToVariant(base.value().id, "REQ-10").isOk(),
            "add REQ-10 to Base");
    h.check(svc.addToVariant(pro.value().id, "REQ-10").isOk(),
            "add REQ-10 to Pro");

    // Override safety level only on Pro.
    svc.setAttributeOverride(pro.value().id, "REQ-10", "safety_level", "A");
    auto baseLevel = svc.effectiveAttribute(base.value().id, "REQ-10", "safety_level", "C");
    auto proLevel = svc.effectiveAttribute(pro.value().id, "REQ-10", "safety_level", "C");
    h.check(baseLevel.isOk() && baseLevel.value() == "C",
            "Base inherits safety_level C");
    h.check(proLevel.isOk() && proLevel.value() == "A",
            "Pro overrides safety_level to A");

    closeAndRemove(db, "lodestar_t3_pv.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill TraceLink 3.3 variant attribute inheritance/override");
    testOverride(h);
    testPerVariant(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
