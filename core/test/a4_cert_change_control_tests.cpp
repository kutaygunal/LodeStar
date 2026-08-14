// core/test/a4_cert_change_control_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill AssureCheck 2.4: certification change/impact control tests.
//
// Test contract: docs/gap-fill-plan.md (Module 2.4).
//   (A) CertChangeControlService wires IntegrateHub CR/impact into a DO-178C
//       configuration-control view (every PR/CR shows impacted HLRs/LLRs/tests
//       and its approval state per baseline).
//   (B) Emits the change-impact report as a certification artifact.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/CertChangeControlService.h"
#include "core/common/Result.h"
#include "core/integratehub/ImpactAnalysisService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
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

// Seed one PR + CR + impact (requirement impact, unverified).
std::string seedCr(p::Database& db, ih::ImpactAnalysisService& impact,
                   const std::string& title) {
    ih::ChangeRequest cr;
    cr.title = title;
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    auto id = impact.createCr(cr);
    if (id.isOk()) impact.analyzeImpact(id.value());
    return id.isOk() ? id.value() : "";
}

// ---------------------------------------------------------------------------
// T1. Configuration-control view shows impacted HLR/LLR + approval state
// ---------------------------------------------------------------------------
void testView(Harness& h) {
    h.section("T1. configuration-control view");
    p::Database db;
    if (!openFreshDb(db, "lodestar_a4_view.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService impact(db);
    ac::CertChangeControlService cert(db);

    std::string crId = seedCr(db, impact, "Change threshold");
    h.check(!crId.empty(), "seeded a CR");

    auto view = cert.configurationControlView();
    h.check(view.isOk(), "configurationControlView() ok");
    if (!view.isOk()) { closeAndRemove(db, "lodestar_a4_view.db"); return; }
    h.check(!view.value().empty(), "view has rows");
    if (!view.value().empty()) {
        const auto& r = view.value()[0];
        h.check(r.crId == crId, "CR id in view");
        h.check(r.crTitle == "Change threshold", "CR title in view");
        h.check(r.impactedType == "HLR/LLR", "requirement classified as HLR/LLR");
        h.check(r.impactedId == "REQ-100", "impacted entity id shown");
        h.check(r.crStatus == "Open", "CR status shown");
        // Unverified impact + Open status -> Pending approval.
        h.check(r.approvalState == "Pending", "unapproved CR approval state = Pending");
        h.check(r.unverifiedImpact >= 1, "unverified impact risk shown");
    }

    closeAndRemove(db, "lodestar_a4_view.db");
}

// ---------------------------------------------------------------------------
// T2. Change-impact report artifact
// ---------------------------------------------------------------------------
void testReport(Harness& h) {
    h.section("T2. change-impact report artifact");
    p::Database db;
    if (!openFreshDb(db, "lodestar_a4_report.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService impact(db);
    ac::CertChangeControlService cert(db);
    seedCr(db, impact, "Change threshold");

    auto report = cert.emitChangeImpactReport();
    h.check(report.isOk(), "emitChangeImpactReport() ok");
    if (report.isOk()) {
        h.check(report.value().find("DO-178C CHANGE IMPACT REPORT") != std::string::npos,
                "report has the DO-178C title");
        h.check(report.value().find("Change threshold") != std::string::npos,
                "report contains the CR title");
        h.check(report.value().find("impacted HLR/LLR") != std::string::npos,
                "report shows the impacted HLR/LLR");
        h.check(report.value().find("REQ-100") != std::string::npos,
                "report shows the impacted entity id");
        h.check(report.value().find("approval=Pending") != std::string::npos,
                "report shows the approval state");
    }

    closeAndRemove(db, "lodestar_a4_report.db");
}

// ---------------------------------------------------------------------------
// T3. Approval state reflects an Approved CR with verified impact
// ---------------------------------------------------------------------------
void testApprovalState(Harness& h) {
    h.section("T3. approval state from CR status + verified impact");
    p::Database db;
    if (!openFreshDb(db, "lodestar_a4_approve.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ih::ImpactAnalysisService impact(db);
    ac::CertChangeControlService cert(db);

    ih::ChangeRequest cr;
    cr.title = "Approved change";
    cr.entityType = "requirement";
    cr.entityId = "REQ-200";
    auto id = impact.createCr(cr);
    impact.analyzeImpact(id.value());

    // Mark the CR approved AND verify the impact by inserting a passing test run
    // so unverifiedImpact becomes 0.
    db.execute("CREATE TABLE IF NOT EXISTS test_run_coverage ("
               " id TEXT PRIMARY KEY, run_id TEXT NOT NULL, test_case_id TEXT NOT NULL, "
               " passed INTEGER NOT NULL DEFAULT 0, executed_at TEXT NOT NULL DEFAULT '');");
    db.execute("INSERT INTO test_run_coverage (id, run_id, test_case_id, passed, executed_at) "
               "VALUES ('RC-1','RUN-1','REQ-200',1,'now');");
    // Re-analyze with the verified run present (new impact rows, verified).
    impact.analyzeImpact(id.value());
    db.execute("UPDATE integratehub_cr SET status='Approved' WHERE id='" +
               id.value() + "';");

    auto view = cert.configurationControlView();
    h.check(view.isOk(), "configurationControlView() ok");
    if (view.isOk() && !view.value().empty()) {
        bool foundApproved = false;
        for (const auto& r : view.value()) {
            if (r.crId == id.value() && r.approvalState == "Approved")
                foundApproved = true;
        }
        h.check(foundApproved,
                "approved CR with verified impact shows approval state Approved");
    }

    closeAndRemove(db, "lodestar_a4_approve.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill AssureCheck 2.4 certification change/impact control");
    testView(h);
    testReport(h);
    testApprovalState(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
