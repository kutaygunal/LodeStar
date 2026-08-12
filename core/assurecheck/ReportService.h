#pragma once
// core/assurecheck/ReportService.h
// Phase 11 WP-4 (AssureCheck): compliance reporting.
//
// Turns WP-2/3 compliance results into certification-ready reports per standard
// and per DAL, with objective-coverage percentages, and exports them as HTML,
// CSV, and JSON. Reads the existing `assurance_checklist_items` table for
// objective text and consumes `CheckResult` vectors produced by the engine.
//
// Contract written by the scrum-master in docs/wp4-assurecheck-task.md and
// core/test/wp4_assurecheck_tests.cpp.

#include <string>
#include <vector>

#include "core/adapters/Json.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One row of a report: a single checklist item's result.
struct ReportRow {
    std::string itemCode;
    std::string objective;   // objective text from the checklist
    std::string dalLevel;    // item's DAL range (A | A-B | A-C | A-D)
    std::string status;      // PASS | FAIL | NA | WARNING
    std::string evidence;    // evidence links summary
};

// Objective-coverage summary for a report.
struct CoverageSummary {
    int total = 0;        // number of checklist items
    int applicable = 0;   // total - na
    int pass = 0;
    int fail = 0;
    int na = 0;
    int warning = 0;
    int percent = 0;      // applicable>0 ? pass*100/applicable : 0
};

// A full report for one standard at one project DAL.
struct ComplianceReport {
    std::string standardCode;
    std::string standardName;
    std::string dalLevel;   // project DAL
    CoverageSummary coverage;
    std::vector<ReportRow> rows;   // ordered by item seq
};

class ReportService {
public:
    explicit ReportService(persistence::Database& db);

    // Builds a report from results for a standard + project DAL. Rows are
    // ordered by item seq; objective text is looked up from the checklist.
    common::Result<ComplianceReport> buildReport(
        const std::string& standardCode, const std::string& dalLevel,
        const std::vector<CheckResult>& results);

    // Computes the coverage summary for a report.
    CoverageSummary coverage(const ComplianceReport& report);

    // Exports a report to a self-contained HTML document.
    common::Result<std::string> toHtml(const ComplianceReport& report);

    // Exports a report to CSV (header row + one row per item).
    common::Result<std::string> toCsv(const ComplianceReport& report);

    // Exports a report to a structured JSON object.
    common::Result<Json> toJson(const ComplianceReport& report);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
