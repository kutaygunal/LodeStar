// core/testforge/CoverageIngestMain.cpp
// S3 Phase 3: CLI entry point that imports a real Cobertura coverage report
// (produced by OpenCppCoverage - see ci/run_coverage.ps1) into a Lodestar
// SQLite database's coverage_results table.
//
// Usage:
//   lodestar_coverage_ingest <cobertura.xml> <db_path> <run_id>
//
// Exits 0 and prints a per-run aggregate statement-coverage percentage on
// success; exits 1 with an error message otherwise.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/CoberturaImport.h"
#include "core/testforge/Coverage.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr,
                     "usage: lodestar_coverage_ingest <cobertura.xml> <db_path> "
                     "<run_id>\n");
        return 1;
    }
    const std::string xmlPath = argv[1];
    const std::string dbPath = argv[2];
    const std::string runId = argv[3];

    lodestar::persistence::Database db;
    auto opened = db.open(dbPath);
    if (opened.failed()) {
        std::fprintf(stderr, "failed to open db '%s': %s\n", dbPath.c_str(),
                     opened.error().c_str());
        return 1;
    }
    lodestar::persistence::MigrationRunner runner(db);
    auto migrated = runner.run(LODESTAR_MIGRATIONS_DIR);
    if (migrated.failed()) {
        std::fprintf(stderr, "migration failed: %s\n", migrated.error().c_str());
        return 1;
    }

    lodestar::testforge::CoverageDao dao(db);
    auto imported =
        lodestar::testforge::importCoberturaReport(xmlPath, runId, dao);
    if (imported.failed()) {
        std::fprintf(stderr, "import failed: %s\n", imported.error().c_str());
        return 1;
    }

    // Aggregate + print a per-file breakdown and the overall percentage so a
    // human (or CI log) can see real numbers immediately, not just "N files
    // imported".
    auto all = dao.list();
    if (all.failed()) {
        std::fprintf(stderr, "list failed: %s\n", all.error().c_str());
        return 1;
    }
    long long execSum = 0, totalSum = 0;
    int filesForRun = 0;
    for (const auto& r : all.value()) {
        if (r.runId != runId) continue;
        ++filesForRun;
        execSum += r.statementsExecuted;
        totalSum += r.statementsTotal;
        double pct = lodestar::testforge::computeStatementCoverage(
            r.statementsExecuted, r.statementsTotal);
        std::printf("  %-70s %6.1f%%  (%d/%d)\n", r.scope.c_str(), pct,
                    r.statementsExecuted, r.statementsTotal);
    }
    double overall = lodestar::testforge::computeStatementCoverage(
        static_cast<int>(execSum), static_cast<int>(totalSum));
    std::printf("\nImported %d file(s) for run '%s'.\n", filesForRun,
               runId.c_str());
    std::printf("Overall statement coverage: %.2f%% (%lld/%lld statements)\n",
               overall, execSum, totalSum);
    return 0;
}
