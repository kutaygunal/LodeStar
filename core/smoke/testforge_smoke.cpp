// core/smoke/testforge_smoke.cpp
// Self-verifying smoke path for Phase 6 (TestForge: IT&V plan generation,
// execution, reporting, persistence).
//
// Exercises: PlanGenerator (step count + expected values), TestRunner
// (Passed / Blocked / Failed via MockMeasurementProvider), ReportGenerator
// (markdown + JSON), and TestForgeDao (procedure + run round-trip through
// SQLite). Returns non-zero on any failure.

#include <cstdio>
#include <string>
#include <vector>

#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/Models.h"
#include "core/testforge/PlanGenerator.h"
#include "core/testforge/ReportGenerator.h"
#include "core/testforge/TestForgeDao.h"
#include "core/testforge/TestRunner.h"

namespace lodestar::testforge {

namespace {
int failures = 0;

void check(bool cond, const char* what) {
    if (cond) {
        std::printf("  [PASS] %s\n", what);
    } else {
        std::printf("  [FAIL] %s\n", what);
        ++failures;
    }
}
}  // namespace

int runTestForgeSmoke() {
    std::printf("TESTFORGE PHASE 6 SMOKE\n");

    // --- Plan generation ---------------------------------------------------
    PlanGenerator generator;
    std::vector<Check> checks;
    checks.push_back({"Position accuracy", "Horizontal position within spec",
                      "position_accuracy_m", 2.0, 0.5});
    checks.push_back({"Signal acquisition", "Lock acquired within window",
                      "acquisition_time_s", 30.0, 5.0});

    auto procResult = generator.generate("GNSS Position Accuracy IT&V",
                                         "1.0", "Verify position accuracy and "
                                         "signal acquisition",
                                         "SCN-001", checks);
    check(procResult.isOk(), "PlanGenerator::generate succeeds");
    if (!procResult.isOk()) return 1;

    TestProcedure proc = procResult.value();
    check(proc.id.size() > 8, "procedure got a UUID id");
    check(proc.steps.size() == 2, "procedure has 2 steps");
    check(proc.steps[0].seq == 1 && proc.steps[1].seq == 2, "steps are sequentially numbered");
    check(proc.steps[0].metric == "position_accuracy_m", "step 0 metric set");
    check(proc.steps[0].expectedValue == 2.0, "step 0 expected value copied");
    check(proc.steps[0].tolerance == 0.5, "step 0 tolerance copied");

    // --- Execution ---------------------------------------------------------
    MockMeasurementProvider provider;
    provider.set("position_accuracy_m", 2.2);   // within tolerance 0.5 -> Pass
    provider.remove("acquisition_time_s");      // unavailable -> Blocked

    TestRunner runner(provider);
    auto runResult = runner.run(proc, "t0", "t1");
    check(runResult.isOk(), "TestRunner::run succeeds");
    if (!runResult.isOk()) return 1;

    TestRun run = runResult.value();
    check(run.results.size() == 2, "run has 2 step results");
    check(run.results[0].status == StepStatus::Passed, "step 0 measured within tolerance -> Passed");
    check(run.results[1].status == StepStatus::Blocked, "step 1 unavailable -> Blocked");
    check(run.status == RunStatus::Blocked, "run terminal status Blocked (no failures)");

    // Now supply the second metric -> all pass -> run Passed.
    provider.set("acquisition_time_s", 31.0);
    auto run2Result = runner.run(proc, "t0", "t1");
    check(run2Result.isOk(), "second run succeeds");
    TestRun run2 = run2Result.value();
    check(run2.status == RunStatus::Passed, "second run all-pass -> Passed");

    // Force a failure on step 0.
    provider.set("position_accuracy_m", 99.0);
    auto run3Result = runner.run(proc, "t0", "t1");
    TestRun run3 = run3Result.value();
    check(run3.results[0].status == StepStatus::Failed, "out-of-tolerance -> Failed");
    check(run3.status == RunStatus::Failed, "failure -> run Failed");

    // --- Reporting ---------------------------------------------------------
    ReportGenerator reporter;
    std::string md = reporter.toMarkdown(run2);
    check(md.find("Test Report") != std::string::npos, "markdown has header");
    check(md.find("ALL PASSED") != std::string::npos, "markdown has conclusion");

    Json js = reporter.toJson(run2);
    check(js.has("runId") && js.has("steps"), "json has runId and steps");
    check(js["total"].asNumber() == 2.0, "json total == 2");
    check(js["passed"].asNumber() == 2.0, "json passed == 2");
    check(js["steps"].size() == 2, "json has 2 step entries");

    TestReport rep = reporter.summarize(run2);
    check(rep.conclusion == "ALL PASSED", "summary conclusion ALL PASSED");

    // --- Persistence -------------------------------------------------------
    {
        persistence::Database db;
        auto open = db.open("lodestar_testforge_smoke.db");
        check(open.isOk(), "opened throwaway db");
        if (!open.isOk()) return 1;

        persistence::MigrationRunner mig(db);
        auto migrated = mig.run(LODESTAR_TESTFORGE_MIGRATIONS_DIR);
        check(migrated.isOk(), "migration runs");
        if (!migrated.isOk()) return 1;
        check(migrated.value() >= 4, "schema migrated to v4 (WP-1 migrations applied)");

        TestForgeDao dao(db);

        auto saved = dao.saveProcedure(proc);
        check(saved.isOk(), "saveProcedure succeeds");

        auto loaded = dao.loadProcedure(proc.id);
        check(loaded.isOk(), "loadProcedure succeeds");
        if (loaded.isOk() && loaded.value().has_value()) {
            TestProcedure lp = loaded.value().value();
            check(lp.id == proc.id, "loaded procedure id matches");
            check(lp.name == proc.name, "loaded procedure name matches");
            check(lp.steps.size() == 2, "loaded procedure has 2 steps");
            check(lp.steps[0].metric == proc.steps[0].metric, "loaded step metric matches");
            check(lp.steps[0].expectedValue == proc.steps[0].expectedValue,
                  "loaded step expected value matches");
        } else {
            check(false, "loadProcedure returned a procedure");
        }

        auto savedRun = dao.saveRun(run2);
        check(savedRun.isOk(), "saveRun succeeds");

        auto loadedRun = dao.loadRun(run2.id);
        check(loadedRun.isOk(), "loadRun succeeds");
        if (loadedRun.isOk() && loadedRun.value().has_value()) {
            TestRun lr = loadedRun.value().value();
            check(lr.id == run2.id, "loaded run id matches");
            check(lr.status == RunStatus::Passed, "loaded run status Passed");
            check(lr.results.size() == 2, "loaded run has 2 step results");
            check(lr.results[0].status == StepStatus::Passed, "loaded step 0 Passed");
            check(lr.results[1].status == StepStatus::Passed, "loaded step 1 Passed");
        } else {
            check(false, "loadRun returned a run");
        }

        db.close();
        std::remove("lodestar_testforge_smoke.db");
        std::remove("lodestar_testforge_smoke.db-wal");
        std::remove("lodestar_testforge_smoke.db-shm");
    }

    if (failures == 0) {
        std::printf("TESTFORGE SMOKE OK\n");
        return 0;
    }
    std::printf("TESTFORGE SMOKE FAILED: %d check(s)\n", failures);
    return 1;
}

}  // namespace lodestar::testforge
