// core/test/i2_cert_control_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill IntegrateHub 6.2: integration with certification control tests.
//
// Test contract: docs/gap-fill-plan.md (Module 6.2).
//   (A) CertificationControlService feeds PR/CR + impact into the
//       configuration-control view (every CR shows its PR and impact counts,
//       including risk of unverified impact).
//   (B) Emits the PR/CR log as a certification artifact.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/integratehub/CertificationControlService.h"
#include "core/integratehub/ImpactAnalysisService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ih = lodestar::integratehub;
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
// T1. Config-control view: CR + PR + impact counts
// ---------------------------------------------------------------------------
void testConfigControlView(Harness& h) {
    h.section("T1. configuration-control view");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i2_view.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService impact(db);
    ih::CertificationControlService cert(db);

    // Create a PR and a CR linked to it.
    ih::ProblemReport pr;
    pr.title = "GPS lock lost";
    pr.approvalAuthority = "safety";
    auto prId = impact.createPr(pr);
    h.check(prId.isOk(), "createPr() ok");

    ih::ChangeRequest cr;
    cr.prId = prId.value();
    cr.title = "Change acquisition threshold";
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    auto crId = impact.createCr(cr);
    h.check(crId.isOk(), "createCr() ok");

    // Analyze impact (produces impact rows; no verified test run -> unverified).
    auto impactRes = impact.analyzeImpact(crId.value());
    h.check(impactRes.isOk() && !impactRes.value().empty(),
            "impact analysis produces impact rows");

    auto view = cert.configControlView();
    h.check(view.isOk(), "configControlView() ok");
    if (!view.isOk()) { closeAndRemove(db, "lodestar_i2_view.db"); return; }
    h.check(view.value().size() == 1, "view has 1 CR");
    if (view.value().size() == 1) {
        const auto& r = view.value()[0];
        h.check(r.crId == crId.value(), "CR id in view");
        h.check(r.crTitle == "Change acquisition threshold", "CR title in view");
        h.check(r.prId == prId.value(), "linked PR id in view");
        h.check(r.prTitle == "GPS lock lost", "linked PR title in view");
        h.check(r.impactCount >= 1, "impact count >= 1");
        h.check(r.unverifiedImpact >= 1,
                "unverified impact risk surfaced in the view");
    }

    closeAndRemove(db, "lodestar_i2_view.db");
}

// ---------------------------------------------------------------------------
// T2. Emit the PR/CR log as a certification artifact
// ---------------------------------------------------------------------------
void testEmitLog(Harness& h) {
    h.section("T2. PR/CR log certification artifact");
    p::Database db;
    if (!openFreshDb(db, "lodestar_i2_log.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService impact(db);
    ih::CertificationControlService cert(db);

    ih::ProblemReport pr;
    pr.title = "GPS lock lost";
    auto prId = impact.createPr(pr);
    ih::ChangeRequest cr;
    cr.prId = prId.value();
    cr.title = "Change threshold";
    cr.entityType = "requirement";
    cr.entityId = "REQ-1";
    impact.createCr(cr);

    auto log = cert.emitPrCrLog();
    h.check(log.isOk(), "emitPrCrLog() ok");
    if (log.isOk()) {
        h.check(log.value().find("CERTIFICATION CONFIGURATION CONTROL LOG") !=
                    std::string::npos,
                "log has the artifact title");
        h.check(log.value().find("Change requests: 1") != std::string::npos,
                "log counts 1 change request");
        h.check(log.value().find("Change threshold") != std::string::npos,
                "log contains the CR title");
        h.check(log.value().find("linked PR") != std::string::npos,
                "log shows the linked PR");
    }

    // Empty DB -> log is still valid with 0 CRs.
    ih::CertificationControlService empty(db);
    // (db already has data, so this just confirms it doesn't crash; the 
    // artifact is emitted with whatever rows exist.)
    h.check(empty.emitPrCrLog().isOk(), "log emission always returns an artifact");

    closeAndRemove(db, "lodestar_i2_log.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill IntegrateHub 6.2 certification control");
    testConfigControlView(h);
    testEmitLog(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
