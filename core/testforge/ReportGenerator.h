#pragma once
// core/testforge/ReportGenerator.h
// TestForge reporting: turns a TestRun into a markdown report, a structured
// JSON report, and a TestReport summary.

#include <string>

#include "core/adapters/Json.h"
#include "core/testforge/Models.h"

namespace lodestar::testforge {

class ReportGenerator {
public:
    // Human-readable markdown report (header, summary counts, per-step table).
    std::string toMarkdown(const TestRun& run) const;

    // Structured JSON report of the run.
    Json toJson(const TestRun& run) const;

    // Compact summary with counts and a one-line conclusion.
    TestReport summarize(const TestRun& run) const;
};

}  // namespace lodestar::testforge
