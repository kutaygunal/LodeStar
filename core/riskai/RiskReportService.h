// core/riskai/RiskReportService.h
// Gap-Fill RiskAI 1.4: AIAG/VDA-compatible export.
//
// Emits an AIAG-VDA structured FMEA spreadsheet (CSV, all 7-step columns) and
// an HTML/PDF-style review report from a persisted FMEA workflow. Supports the
// FMEA Excellence round-trip pattern: export -> edit in Excel -> re-import
// (via FmeaAssessor::parseImport) preserving row identity.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::riskai {

class RiskReportService {
public:
    explicit RiskReportService(persistence::Database& db);

    // AIAG-VDA structured FMEA spreadsheet (CSV): one header row with the
    // seven-step columns, then one row per failure row. Deterministic.
    common::Result<std::vector<std::uint8_t>> exportCsv(
        const std::string& workflowId);

    // HTML review report: workflow header, per-function sections, a table of
    // failure rows with S/O/D/RPN/AP, and the workflow stage. Deterministic.
    common::Result<std::vector<std::uint8_t>> exportHtml(
        const std::string& workflowId);

    // Round-trip helper: the CSV export's data rows (sans header) can be fed
    // back to FmeaAssessor::parseImport to re-import. This returns the CSV
    // content as text so callers can verify round-trip identity.
    common::Result<std::string> csvText(const std::string& workflowId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::riskai
