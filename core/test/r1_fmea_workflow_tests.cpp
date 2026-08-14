// core/test/r1_fmea_workflow_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill RiskAI 1.1 + 1.2: FMEA workflow engine + RPN/AP scoring tests.
//
// Test contract: docs/gap-fill-plan.md (Module 1).
//   (A) Migration 028 creates riskai_fmea / riskai_fmea_function /
//       riskai_fmea_row tables.
//   (B) core/riskai/FmeaWorkflowService.h (+ .cpp) provides the AIAG/VDA
//       seven-stage workflow state machine (CRUD, stage gating, required-field
//       rules) plus deterministic RPN (S*O*D) and Action Priority (H/M/L).
//
// Uses the same lightweight self-contained harness as the rest of the suite.
// Deterministic: no live LLM required.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/riskai/FmeaWorkflowService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ra = lodestar::riskai;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

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

void closeAndRemove(p::Database& db, const char* file) {
    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. Migration 028 + workflow CRUD round-trip
// ---------------------------------------------------------------------------
void testMigrationAndCrud(Harness& h) {
    h.section("T1. migration 028 + workflow CRUD round-trip");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r1_crud.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    h.check(tableExists(db, "riskai_fmea"), "riskai_fmea table exists");
    h.check(tableExists(db, "riskai_fmea_function"),
            "riskai_fmea_function table exists");
    h.check(tableExists(db, "riskai_fmea_row"), "riskai_fmea_row table exists");

    ra::FmeaWorkflowService svc(db);
    ra::FmeaWorkflow wf;
    wf.name = "Receiver FMEA";
    wf.system = "GNSS receiver";
    auto created = svc.createWorkflow(wf);
    h.check(created.isOk(), "createWorkflow() ok");
    if (!created.isOk()) { closeAndRemove(db, "lodestar_r1_crud.db"); return; }
    const std::string id = created.value();
    h.check(!id.empty(), "createWorkflow() returns a non-empty id");

    auto found = svc.findWorkflow(id);
    h.check(found.isOk(), "findWorkflow() ok");
    if (found.isOk() && found.value().has_value()) {
        h.check(found.value()->name == "Receiver FMEA",
                "found workflow name == \"Receiver FMEA\"");
        h.check(found.value()->stage == ra::FmeaStage::Planning,
                "new workflow starts at Planning");
        h.check(found.value()->system == "GNSS receiver",
                "found workflow system == \"GNSS receiver\"");
    }

    auto list = svc.listWorkflows();
    h.check(list.isOk() && list.value().size() == 1,
            "listWorkflows() returns 1 workflow");

    h.check(svc.deleteWorkflow(id).isOk(), "deleteWorkflow() ok");
    auto after = svc.findWorkflow(id);
    h.check(after.isOk() && !after.value().has_value(),
            "workflow gone after delete");
    closeAndRemove(db, "lodestar_r1_crud.db");
}

// ---------------------------------------------------------------------------
// T2. Stage gating: required-field rules
// ---------------------------------------------------------------------------
void testStageGating(Harness& h) {
    h.section("T2. stage gating (required-field rules)");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r1_gating.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::FmeaWorkflowService svc(db);

    ra::FmeaWorkflow wf;
    wf.name = "Wing FMEA";
    // No system set -> cannot leave Planning.
    auto created = svc.createWorkflow(wf);
    h.check(created.isOk(), "createWorkflow() ok");
    if (!created.isOk()) { closeAndRemove(db, "lodestar_r1_gating.db"); return; }
    const std::string id = created.value();

    // Without a system (focus element), Planning is not complete.
    auto adv = svc.advanceStage(id);
    h.check(adv.failed(), "advanceStage() blocked (no system set)");
    h.check(adv.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "blocked advance reports ValidationFailed");

    // Set the system, then Planning -> Structure.
    // Create with system set, then verify Structure requires structure elements.
    ra::FmeaWorkflow wf2;
    wf2.name = "Wing FMEA";
    wf2.system = "Wing";
    auto created2 = svc.createWorkflow(wf2);
    h.check(created2.isOk(), "createWorkflow() with system ok");
    const std::string id2 = created2.value();

    auto toStruct = svc.advanceStage(id2);
    h.check(toStruct.isOk() && toStruct.value() == ra::FmeaStage::Structure,
            "advance Planning -> Structure ok");

    // Structure requires next-higher + next-lower.
    auto toFunction = svc.advanceStage(id2);
    h.check(toFunction.failed(), "advance Structure -> Function blocked (no structure)");
    h.check(toFunction.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "blocked Structure advance reports ValidationFailed");

    closeAndRemove(db, "lodestar_r1_gating.db");
}

// ---------------------------------------------------------------------------
// T3. Full staged progression through the 7 AIAG-VDA steps
// ---------------------------------------------------------------------------
void testFullProgression(Harness& h) {
    h.section("T3. full progression through all 7 stages");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r1_prog.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::FmeaWorkflowService svc(db);

    ra::FmeaWorkflow wf;
    wf.name = "Actuator FMEA";
    wf.system = "Flight control actuator";
    auto created = svc.createWorkflow(wf);
    if (!created.isOk()) { closeAndRemove(db, "lodestar_r1_prog.db"); return; }
    const std::string id = created.value();

    // Planning -> Structure.
    auto s2 = svc.advanceStage(id);
    h.check(s2.isOk() && s2.value() == ra::FmeaStage::Structure,
            "Planning -> Structure");

    // Complete Structure (next-higher + next-lower) then advance to Function.
    ra::FmeaWorkflow wfFound = *svc.findWorkflow(id).value();
    wfFound.nextHigher = "Actuator assembly";
    wfFound.nextLower = "Servo motor";
    h.check(svc.updateWorkflow(wfFound).isOk(), "updateWorkflow() ok");
    auto s3 = svc.advanceStage(id);
    h.check(s3.isOk() && s3.value() == ra::FmeaStage::Function,
            "Structure -> Function");

    // Add a function, then advance to Failure.
    auto fn = svc.addFunction(id, "Convert electrical command to mechanical motion",
                              "The actuator shall move the control surface");
    h.check(fn.isOk(), "addFunction() ok");
    auto s4 = svc.advanceStage(id);
    h.check(s4.isOk() && s4.value() == ra::FmeaStage::Failure,
            "Function -> Failure");

    // Add a failure row, advance to Risk.
    ra::FmeaRow row;
    row.fmeaId = id;
    row.failureMode = "Servo stalls";
    row.effect = "Control surface frozen";
    row.cause = "Motor winding short";
    auto rowId = svc.addRow(row);
    h.check(rowId.isOk(), "addRow() ok");
    auto s5 = svc.advanceStage(id);
    h.check(s5.isOk() && s5.value() == ra::FmeaStage::Risk,
            "Failure -> Risk");

    // Risk -> Optimization requires the row to be rated.
    auto s6 = svc.advanceStage(id);
    h.check(s6.failed(), "Risk -> Optimization blocked (row not rated)");
    h.check(s6.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "blocked Risk advance reports ValidationFailed");

    closeAndRemove(db, "lodestar_r1_prog.db");
}

// ---------------------------------------------------------------------------
// T4. RPN + Action Priority boundary correctness (RiskAI 1.2)
// ---------------------------------------------------------------------------
void testScoring(Harness& h) {
    h.section("T4. RPN + Action Priority boundary correctness");

    // RPN = S*O*D.
    auto a = ra::FmeaWorkflowService::computeScore(5, 4, 3);
    h.check(a.rpn == 60, "RPN 5*4*3 == 60");
    auto b = ra::FmeaWorkflowService::computeScore(10, 10, 10);
    h.check(b.rpn == 1000, "RPN 10*10*10 == 1000");
    h.check(b.actionPriority == "High", "S=10 => High AP");

    // Clamp out-of-range inputs.
    auto c = ra::FmeaWorkflowService::computeScore(0, 99, -3);
    h.check(c.rpn == 10, "out-of-range clamps to 1..10 (1*10*1 == 10)");

    // High boundaries.
    h.check(ra::FmeaWorkflowService::computeScore(9, 1, 1).actionPriority == "High",
            "S>=9 => High regardless of O/D");
    h.check(ra::FmeaWorkflowService::computeScore(7, 6, 6).actionPriority == "High",
            "S=7,O=6,D=6 => High");
    h.check(ra::FmeaWorkflowService::computeScore(7, 5, 6).actionPriority == "Medium",
            "S=7,O=5,D=6 => Medium (not High)");
    h.check(ra::FmeaWorkflowService::computeScore(5, 7, 8).actionPriority == "High",
            "S=5,O=7,D=8 => High");
    h.check(ra::FmeaWorkflowService::computeScore(5, 7, 7).actionPriority == "Medium",
            "S=5,O=7,D=7 => Medium");

    // Low boundaries.
    h.check(ra::FmeaWorkflowService::computeScore(4, 5, 5).actionPriority == "Low",
            "S<=4,O<=5,D<=5 => Low");
    h.check(ra::FmeaWorkflowService::computeScore(4, 6, 5).actionPriority == "Medium",
            "S=4,O=6,D=5 => Medium");
    h.check(ra::FmeaWorkflowService::computeScore(6, 3, 4).actionPriority == "Low",
            "S<=6,O<=3,D<=4 => Low");
    h.check(ra::FmeaWorkflowService::computeScore(6, 4, 4).actionPriority == "Medium",
            "S=6,O=4,D=4 => Medium");

    // rowIsRated.
    ra::FmeaRow rated;
    rated.severity = 5; rated.occurrence = 4; rated.detection = 3;
    rated.actionPriority = "Medium";
    h.check(ra::FmeaWorkflowService::rowIsRated(rated),
            "rated row is recognized as rated");
    ra::FmeaRow unrated;
    unrated.severity = 0; unrated.occurrence = 4; unrated.detection = 3;
    h.check(!ra::FmeaWorkflowService::rowIsRated(unrated),
            "unrated row (S=0) is not rated");
    ra::FmeaRow noAp = rated; noAp.actionPriority = "";
    h.check(!ra::FmeaWorkflowService::rowIsRated(noAp),
            "row without computed AP is not rated");
}

// ---------------------------------------------------------------------------
// T5. Row scoring integrates into the workflow (Risk -> rated advance)
// ---------------------------------------------------------------------------
void testRatedRowAdvance(Harness& h) {
    h.section("T5. rated row unblocks Risk -> Optimization");
    p::Database db;
    if (!openFreshDb(db, "lodestar_r1_rated.db")) {
        h.check(false, "open fresh db");
        return;
    }
    ra::FmeaWorkflowService svc(db);

    ra::FmeaWorkflow wf;
    wf.name = "Actuator FMEA";
    wf.system = "Flight control actuator";
    auto created = svc.createWorkflow(wf);
    if (!created.isOk()) { closeAndRemove(db, "lodestar_r1_rated.db"); return; }
    const std::string id = created.value();

    // Drive to Risk with a rated row.
    svc.advanceStage(id);  // -> Structure
    ra::FmeaWorkflow wf2 = *svc.findWorkflow(id).value();
    wf2.nextHigher = "A"; wf2.nextLower = "B";
    svc.updateWorkflow(wf2);
    svc.advanceStage(id);  // -> Function
    svc.addFunction(id, "Fn", "Req");
    svc.advanceStage(id);  // -> Failure

    ra::FmeaRow row;
    row.fmeaId = id;
    row.failureMode = "Stall";
    row.effect = "Frozen";
    row.cause = "Short";
    auto score = ra::FmeaWorkflowService::computeScore(7, 6, 6);
    row.severity = 7; row.occurrence = 6; row.detection = 6;
    row.actionPriority = score.actionPriority;
    auto rowId = svc.addRow(row);
    h.check(rowId.isOk(), "addRow() with ratings ok");
    svc.advanceStage(id);  // -> Risk
    auto s6 = svc.advanceStage(id);  // Risk -> Optimization
    h.check(s6.isOk() && s6.value() == ra::FmeaStage::Optimization,
            "rated row allows Risk -> Optimization");

    // Verify AP persisted on the row.
    auto rows = svc.rowsFor(id);
    h.check(rows.isOk() && rows.value().size() == 1, "1 row persisted");
    if (rows.isOk() && rows.value().size() == 1) {
        h.check(rows.value()[0].actionPriority == "High",
                "persisted AP == High (S=7,O=6,D=6)");
        h.check(rows.value()[0].severity == 7, "persisted severity == 7");
    }

    closeAndRemove(db, "lodestar_r1_rated.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill RiskAI 1.1/1.2 FMEA workflow + scoring");
    std::printf("RISKAI R1 FMEA WORKFLOW + SCORING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testMigrationAndCrud(h);
    testStageGating(h);
    testFullProgression(h);
    testScoring(h);
    testRatedRowAdvance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
