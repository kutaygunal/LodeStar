// core/test/r3_agentic_pipeline_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill RiskAI 1.6: agentic / self-validating pipeline tests.
//
// Test contract: docs/gap-fill-plan.md (Module 1.6).
//   (A) core/riskai/AgenticPipeline.h (+ .cpp) runs ANALYZE -> RATE ->
//       VALIDATE -> CORRECT -> FINALIZE stages, each with a quality gate
//       (chain consistency, rating-range validity, AP consistency).
//   (B) On gate failure, a bounded CORRECT pass loops; corrections are logged
//       as audit events. Forced-invalid inputs converge to valid output.
//
// Deterministic: no live LLM.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/riskai/AgenticPipeline.h"
#include "core/riskai/FmeaWorkflowService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ra = lodestar::riskai;
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

// Seed a workflow at the Failure stage (ready to receive rows).
std::string seedWorkflow(p::Database& db, ra::FmeaWorkflowService& svc) {
    ra::FmeaWorkflow wf;
    wf.name = "Receiver FMEA";
    wf.system = "GNSS receiver";
    auto c = svc.createWorkflow(wf);
    if (c.failed()) return "";
    const std::string id = c.value();
    svc.advanceStage(id);  // -> Structure
    ra::FmeaWorkflow wf2 = *svc.findWorkflow(id).value();
    wf2.nextHigher = "A"; wf2.nextLower = "B";
    svc.updateWorkflow(wf2);
    svc.advanceStage(id);  // -> Function
    svc.addFunction(id, "Acquire signals", "R-1");
    svc.advanceStage(id);  // -> Failure
    return id;
}

// ---------------------------------------------------------------------------
// T1. Pure stage gates: ANALYZE + RATE + VALIDATE
// ---------------------------------------------------------------------------
void testStageGates(Harness& h) {
    h.section("T1. pure stage gates (ANALYZE/RATE/VALIDATE)");

    // ANALYZE produces an unrated chain row.
    auto analyzed = ra::AgenticPipeline::analyze("Loss of lock",
                                                 "Position error", "RF interference");
    h.check(analyzed.failureMode == "Loss of lock", "ANALYZE sets failure mode");
    h.check(analyzed.severity == 0, "ANALYZE leaves ratings unrated");

    // RATE assigns valid ratings + AP.
    auto rated = ra::AgenticPipeline::rate(analyzed);
    h.check(rated.severity >= 1 && rated.severity <= 10, "RATE assigns valid severity");
    h.check(rated.occurrence >= 1 && rated.occurrence <= 10, "RATE assigns valid occurrence");
    h.check(rated.detection >= 1 && rated.detection <= 10, "RATE assigns valid detection");
    h.check(!rated.actionPriority.empty(), "RATE computes action priority");

    // A fully rated row validates clean.
    h.check(ra::AgenticPipeline::validate(rated).empty(),
            "VALIDATE passes a correctly-rated row");

    // VALIDATE flags missing fields.
    ra::FmeaRow incomplete;
    incomplete.failureMode = "";
    incomplete.severity = 11;  // out of range
    auto gates = ra::AgenticPipeline::validate(incomplete);
    h.check(!gates.empty(), "VALIDATE flags an invalid row");
    bool hasMissing = false, hasRange = false;
    for (const auto& g : gates) {
        if (g == "failure_mode_missing") hasMissing = true;
        if (g == "severity_out_of_range") hasRange = true;
    }
    h.check(hasMissing, "VALIDATE flags missing failure mode");
    h.check(hasRange, "VALIDATE flags out-of-range severity");
}

// ---------------------------------------------------------------------------
// T2. AP consistency gate
// ---------------------------------------------------------------------------
void testApConsistency(Harness& h) {
    h.section("T2. AP consistency gate");
    ra::FmeaRow row;
    row.failureMode = "Loss of lock";
    row.effect = "Position error";
    row.cause = "RF interference";
    row.severity = 7; row.occurrence = 6; row.detection = 6;
    row.actionPriority = "Low";  // WRONG: S=7,O=6,D=6 => High
    auto gates = ra::AgenticPipeline::validate(row);
    bool hasInconsistent = false;
    for (const auto& g : gates) if (g == "ap_inconsistent") hasInconsistent = true;
    h.check(hasInconsistent, "VALIDATE flags AP inconsistent with matrix");

    // Correct the AP -> validates clean.
    row.actionPriority = "High";
    h.check(ra::AgenticPipeline::validate(row).empty(),
            "VALIDATE passes once AP matches the matrix");
}

// ---------------------------------------------------------------------------
// T3. CORRECT: bounded corrective pass on forced-invalid input
// ---------------------------------------------------------------------------
void testCorrect(Harness& h) {
    h.section("T3. bounded CORRECT pass");
    ra::FmeaRow invalid;
    invalid.failureMode = "";          // missing
    invalid.effect = "";
    invalid.cause = "";
    invalid.severity = 99;             // out of range
    invalid.actionPriority = "";       // missing

    std::vector<ra::CorrectionEvent> audit;
    int budget = 3;
    auto corrected = ra::AgenticPipeline::correct(invalid, audit, budget);
    h.check(!corrected.failureMode.empty(), "CORRECT fills missing failure mode");
    h.check(!corrected.effect.empty(), "CORRECT fills missing effect");
    h.check(!corrected.cause.empty(), "CORRECT fills missing cause");
    h.check(corrected.severity >= 1 && corrected.severity <= 10,
            "CORRECT clamps severity into range");
    h.check(!corrected.actionPriority.empty(), "CORRECT computes AP");
    h.check(!audit.empty(), "CORRECT logs audit events");
    // After corrections, the row must validate.
    h.check(ra::AgenticPipeline::validate(corrected).empty(),
            "corrected row validates clean");
}

// ---------------------------------------------------------------------------
// T4. End-to-end run: forced-invalid seeds converge + persist + audit
// ---------------------------------------------------------------------------
void testPipelineRun(Harness& h) {
    h.section("T4. end-to-end pipeline converges + persists + audits");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r3_run.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::FmeaWorkflowService svc(db);
    const std::string id = seedWorkflow(db, svc);
    h.check(!id.empty(), "seeded workflow");

    // Forced-invalid seed rows.
    std::vector<ra::FmeaRow> seeds(2);
    seeds[0].failureMode = "";      // invalid: missing chain + ratings
    seeds[1].failureMode = "Loss of lock";
    seeds[1].effect = "Position error";
    seeds[1].cause = "RF interference";
    seeds[1].severity = 7; seeds[1].occurrence = 6; seeds[1].detection = 6;
    seeds[1].actionPriority = "Low";  // inconsistent with matrix

    ra::AgenticPipeline pipe;
    auto result = pipe.run(db, id, seeds, 3);
    h.check(result.isOk(), "run() ok");
    if (!result.isOk()) { closeAndRemove(db, "lodestar_r3_run.db"); return; }

    h.check(result.value().first.size() == 2,
            "pipeline converged 2 valid rows");
    // Both persisted into the workflow.
    auto rows = svc.rowsFor(id);
    h.check(rows.isOk() && rows.value().size() == 2,
            "both validated rows persisted into workflow");
    if (rows.isOk() && rows.value().size() == 2) {
        h.check(rows.value()[0].actionPriority == "High" ||
                rows.value()[0].actionPriority == "Medium" ||
                rows.value()[0].actionPriority == "Low",
                "persisted row has a valid AP");
    }
    // Audit events recorded for the corrections.
    h.check(!result.value().second.empty(), "audit events recorded");

    closeAndRemove(db, "lodestar_r3_run.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill RiskAI 1.6 agentic/self-validating pipeline");
    testStageGates(h);
    testApConsistency(h);
    testCorrect(h);
    testPipelineRun(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
