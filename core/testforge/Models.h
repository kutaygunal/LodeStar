#pragma once
// core/testforge/Models.h
// TestForge domain model: test procedures (IT&V plans), steps, runs, and reports.
// These are plain data structures used by PlanGenerator, TestRunner, and
// ReportGenerator. Persistence lives in TestForgeDao.

#include <string>
#include <vector>

namespace lodestar::testforge {

// Status of an individual test step after execution.
enum class StepStatus {
    Pending,  // not yet executed
    Passed,
    Failed,
    Blocked   // could not be measured / precondition unmet
};

// Terminal status of a whole run.
enum class RunStatus {
    Pending,  // created, not started
    Running,  // in progress
    Passed,   // every step passed
    Failed,   // at least one step failed
    Blocked   // at least one step blocked (and none failed)
};

const char* toString(StepStatus s);
const char* toString(RunStatus s);

// One executable check inside a test procedure.
struct TestStep {
    std::string id;
    int seq = 0;
    std::string name;
    std::string description;
    std::string metric;   // what is measured, e.g. "position_accuracy_m"
    double expectedValue = 0.0;
    double tolerance = 0.0;
    StepStatus status = StepStatus::Pending;
};

// A generated IT&V test plan (collection of ordered steps against a scenario).
struct TestProcedure {
    std::string id;
    std::string name;
    std::string version;
    std::string objective;
    std::string scenarioId;
    RunStatus status = RunStatus::Pending;  // draft planning status
    std::vector<TestStep> steps;
};

// Result of executing a single step within a run.
struct StepResult {
    std::string stepId;
    int seq = 0;
    std::string name;
    StepStatus status = StepStatus::Pending;
    double actualValue = 0.0;
    double expectedValue = 0.0;
    double tolerance = 0.0;
    bool measured = false;   // true if a value was obtained from the provider
    std::string message;
};

// A recorded execution of a procedure.
struct TestRun {
    std::string id;
    std::string procedureId;
    std::string procedureName;
    std::string scenarioId;
    RunStatus status = RunStatus::Pending;
    std::string startedAt;
    std::string finishedAt;
    std::vector<StepResult> results;
};

// Summarized result for reporting.
struct TestReport {
    std::string runId;
    std::string procedureName;
    std::string scenarioId;
    int total = 0;
    int passed = 0;
    int failed = 0;
    int blocked = 0;
    std::string conclusion;
    std::vector<StepResult> results;
};

}  // namespace lodestar::testforge
