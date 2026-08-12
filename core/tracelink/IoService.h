#pragma once
// core/tracelink/IoService.h
// WP-5 import/export: CSV (trace matrix, entities, links), auditor-ready HTML
// report, and ReqIF XML. Imports are non-destructive: every import writes one
// import_batches row plus per-record import_log rows so partial failures are
// reported without corrupting existing data.
//
// Contract written by the scrum-master in core/test/wp5_import_export_tests.cpp
// (see docs/tracelink-plan.md WP-5 / sections 4.6, 3.6).

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::tracelink {

// One line from an import batch log.
struct ImportLogEntry {
    int line = 0;           // 1-based source line / record
    std::string severity;   // "info" | "warning" | "error"
    std::string message;
};

// Result of one import call.
struct ImportReport {
    std::string batchId;    // UUID of the import_batches row
    std::string status;     // "ok" | "partial"
    int imported = 0;       // entities + links successfully imported
    int errors = 0;         // failed records
    std::vector<ImportLogEntry> log;
};

class IoService {
public:
    explicit IoService(persistence::Database& db);

    // --- Exports (non-empty content on success) ----------------------------
    common::Result<std::string> matrixCsv();    // trace matrix CSV
    common::Result<std::string> matrixHtml();   // auditor-ready HTML report
    common::Result<std::string> entitiesCsv();  // entity rows (see format)
    common::Result<std::string> linksCsv();     // link rows (see format)
    common::Result<std::string> reqif();        // ReqIF XML

    // Exports a DO-178C evidence package to `dir` (created if missing):
    //   matrix.csv, coverage.csv, coverage_by_method.csv, validation.json,
    //   audit.csv, manifest.json
    // Each file is non-empty on success. manifest.json lists the files (WP-D A6).
    common::Result<void> exportEvidencePackage(const std::string& dir);

    // --- Imports (non-destructive; always write a batch + log) -------------
    common::Result<ImportReport> importCsv(const std::string& content);
    common::Result<ImportReport> importReqif(const std::string& content);

private:
    // Resolves an entity's external id -> internal id ("" if missing).
    std::string resolveEntityId(const std::string& type,
                                const std::string& externalId);

    // Persists one import_batches row + import_log rows inside a transaction.
    common::Result<void> persistBatch(const std::string& format,
                                      const ImportReport& report,
                                      const std::vector<ImportLogEntry>& log);

    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
