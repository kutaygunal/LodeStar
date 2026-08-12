// core/test/s2_phase6_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 6 (wire functional RF adapters into TestForge execution) tests.
//
// Written by the scrum-master BEFORE the Phase 6 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions; implement the feature
// to satisfy them.
//
// Covers (docs/s2-phase6-test.md): an IMeasurementProvider backed by
// SkydelAdapter::invoke() (simulate mode for CI), the TestForge runner driving
// that provider, and persistence of the resulting measurement.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 6 engineer must provide:
//   - An IMeasurementProvider implementation (SkydelMeasurementProvider) that
//     uses SkydelAdapter::invoke() (simulate mode) to produce a measurement.
//   - The TestForge runner (TestRunner) executes a test case through that
//     provider and records the measurement result.
//   - The run/result is persisted via TestForgeDao and is queryable.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/adapters/SkydelAdapter.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/SkydelMeasurementProvider.h"
#include "core/testforge/TestForgeDao.h"
#include "core/testforge/TestRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ad = lodestar::adapters;
namespace p = lodestar::persistence;
namespace tf = lodestar::testforge;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

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

// Builds a connected SkydelAdapter in simulate mode (CI).
ad::SkydelAdapter makeSimulatedSkydel() {
    ad::SkydelAdapter skydel("skydel");
    ad::AdapterConfig cfg("127.0.0.1", 8081);
    cfg.params["simulate"] = "1";
    skydel.connect(cfg);
    return skydel;
}

// Builds a single-step test procedure whose step expects the simulated
// measurement value (1.5 m) within a small tolerance.
tf::TestProcedure makeProcedure() {
    tf::TestProcedure proc;
    proc.id = "proc-rf-001";
    proc.name = "RF position accuracy";
    proc.version = "1.0";
    proc.objective = "Verify the RF adapter reports position accuracy within tolerance.";
    proc.scenarioId = "scn-001";

    tf::TestStep step;
    step.id = "step-001";
    step.seq = 1;
    step.name = "Measure position accuracy";
    step.description = "Drive the Skydel adapter and record the measured accuracy.";
    step.metric = "position_accuracy_m";
    step.expectedValue = 1.5;
    step.tolerance = 0.1;
    proc.steps.push_back(std::move(step));
    return proc;
}

void cleanup(const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. provider produces a measurement via the adapter
// ---------------------------------------------------------------------------
void testProviderMeasurement(Harness& h) {
    h.section("T1. provider produces a measurement via the adapter");

    ad::SkydelAdapter skydel = makeSimulatedSkydel();
    tf::SkydelMeasurementProvider provider(skydel);

    auto result = provider.measure("position_accuracy_m");
    h.check(result.isOk(), "measure() returns a successful Result");
    h.check(result.isOk() && result.value().has_value(),
            "measure() returns a non-empty measurement (simulate mode)");
    if (result.isOk() && result.value().has_value()) {
        h.check(result.value().value() == 1.5,
                "measured value matches the simulated adapter value (1.5)");
    }
}

// ---------------------------------------------------------------------------
// T2. runner executes a test case through the provider
// ---------------------------------------------------------------------------
void testRunnerThroughProvider(Harness& h) {
    h.section("T2. runner executes a test case through the provider");

    ad::SkydelAdapter skydel = makeSimulatedSkydel();
    tf::SkydelMeasurementProvider provider(skydel);
    tf::TestRunner runner(provider);

    tf::TestProcedure proc = makeProcedure();
    auto runResult = runner.run(proc, "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z");
    h.check(runResult.isOk(), "runner.run() returns a successful Result");
    if (!runResult.isOk()) return;

    const tf::TestRun& run = runResult.value();
    h.check(run.status == tf::RunStatus::Passed,
            "run reaches terminal status Passed");
    h.check(run.results.size() == 1, "run records one step result");
    if (run.results.empty()) return;

    const tf::StepResult& r = run.results[0];
    h.check(r.measured, "step result is marked as measured");
    h.check(r.status == tf::StepStatus::Passed, "step result status is Passed");
    h.check(r.actualValue == 1.5, "step result records the measured value (1.5)");
}

// ---------------------------------------------------------------------------
// T3. measurement result is persisted
// ---------------------------------------------------------------------------
void testPersistence(Harness& h) {
    h.section("T3. measurement result is persisted");

    const char* file = "lodestar_s2p6.db";
    p::Database db;
    std::remove(file);
    if (db.open(file).failed()) {
        h.check(false, "open fresh db");
        return;
    }
    p::MigrationRunner mig(db);
    if (mig.run(g_migrationsDir).failed()) {
        h.check(false, "run migrations");
        db.close();
        cleanup(file);
        return;
    }

    // Execute a run through the provider-backed runner.
    ad::SkydelAdapter skydel = makeSimulatedSkydel();
    tf::SkydelMeasurementProvider provider(skydel);
    tf::TestRunner runner(provider);
    tf::TestProcedure proc = makeProcedure();
    auto runResult = runner.run(proc, "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z");
    h.check(runResult.isOk(), "runner.run() ok");
    if (!runResult.isOk()) {
        db.close();
        cleanup(file);
        return;
    }

    // Persist the run and its step results.
    tf::TestForgeDao dao(db);
    auto saved = dao.saveRun(runResult.value());
    h.check(saved.isOk(), "saveRun() persists the run");

    // Query it back and confirm the measurement value is present.
    auto loaded = dao.loadRun(runResult.value().id);
    h.check(loaded.isOk() && loaded.value().has_value(),
            "loadRun() returns the persisted run");
    if (loaded.isOk() && loaded.value().has_value()) {
        const tf::TestRun& back = loaded.value().value();
        h.check(back.status == tf::RunStatus::Passed,
                "persisted run status is Passed");
        h.check(back.results.size() == 1, "persisted run has one step result");
        if (!back.results.empty()) {
            h.check(back.results[0].measured, "persisted step result is measured");
            h.check(back.results[0].actualValue == 1.5,
                    "persisted step result carries the measurement value (1.5)");
        }
    }

    db.close();
    cleanup(file);
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 6 wire functional RF adapters into TestForge execution");
    std::printf("S2 PHASE 6 TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testProviderMeasurement(h);
    testRunnerThroughProvider(h);
    testPersistence(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
