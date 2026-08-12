// core/testforge/ReportGenerator.cpp
#include "core/testforge/ReportGenerator.h"

#include <cstdio>

namespace lodestar::testforge {

namespace {

std::string fmt(double v, bool measured) {
    if (!measured) return "n/a";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}

}  // namespace

TestReport ReportGenerator::summarize(const TestRun& run) const {
    TestReport rep;
    rep.runId = run.id;
    rep.procedureName = run.procedureName;
    rep.scenarioId = run.scenarioId;
    rep.total = static_cast<int>(run.results.size());
    for (const StepResult& r : run.results) {
        if (r.status == StepStatus::Passed) ++rep.passed;
        if (r.status == StepStatus::Failed) ++rep.failed;
        if (r.status == StepStatus::Blocked) ++rep.blocked;
    }
    rep.results = run.results;
    if (run.status == RunStatus::Passed) {
        rep.conclusion = "ALL PASSED";
    } else if (run.status == RunStatus::Failed) {
        rep.conclusion = std::to_string(rep.failed) + " STEP(S) FAILED";
    } else if (run.status == RunStatus::Blocked) {
        rep.conclusion = std::to_string(rep.blocked) + " STEP(S) BLOCKED";
    } else {
        rep.conclusion = "INCOMPLETE";
    }
    return rep;
}

std::string ReportGenerator::toMarkdown(const TestRun& run) const {
    TestReport rep = summarize(run);

    std::string out;
    out += "# Test Report\n\n";
    out += "- **Run:** " + run.id + "\n";
    out += "- **Procedure:** " + run.procedureName + "\n";
    out += "- **Scenario:** " + run.scenarioId + "\n";
    out += "- **Status:** " + std::string(toString(run.status)) + "\n";
    out += "- **Result:** " + rep.conclusion + "\n\n";

    char counts[128];
    std::snprintf(counts, sizeof(counts), "**%d passed** | **%d failed** | **%d blocked**"
                  " | **%d total**\n\n",
                  rep.passed, rep.failed, rep.blocked, rep.total);
    out += counts;

    out += "| # | Step | Status | Actual | Expected | Tolerance |\n";
    out += "|---|------|--------|--------|----------|-----------|\n";
    for (const StepResult& r : rep.results) {
        out += "| " + std::to_string(r.seq) + " | " + r.name + " | "
             + toString(r.status) + " | " + fmt(r.actualValue, r.measured) + " | "
             + fmt(r.expectedValue, true) + " | " + fmt(r.tolerance, true) + " |\n";
    }
    return out;
}

Json ReportGenerator::toJson(const TestRun& run) const {
    TestReport rep = summarize(run);

    Json root = Json::object();
    root["runId"] = Json::string(run.id);
    root["procedureId"] = Json::string(run.procedureId);
    root["procedureName"] = Json::string(run.procedureName);
    root["scenarioId"] = Json::string(run.scenarioId);
    root["status"] = Json::string(toString(run.status));
    root["conclusion"] = Json::string(rep.conclusion);
    root["total"] = Json::number(rep.total);
    root["passed"] = Json::number(rep.passed);
    root["failed"] = Json::number(rep.failed);
    root["blocked"] = Json::number(rep.blocked);

    Json steps = Json::array();
    for (const StepResult& r : rep.results) {
        Json item = Json::object();
        item["seq"] = Json::number(r.seq);
        item["name"] = Json::string(r.name);
        item["status"] = Json::string(toString(r.status));
        item["actual"] = Json::number(r.actualValue);
        item["expected"] = Json::number(r.expectedValue);
        item["tolerance"] = Json::number(r.tolerance);
        item["measured"] = Json::boolean(r.measured);
        item["message"] = Json::string(r.message);
        steps.push(item);
    }
    root["steps"] = steps;
    return root;
}

}  // namespace lodestar::testforge
