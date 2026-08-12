#pragma once
// core/testforge/CoberturaImport.h
// S3 Phase 3: import REAL, instrumentation-measured statement coverage into
// CoverageDao, replacing the caller-supplied-percentage model (S2 Phase 7)
// with data produced by an actual coverage tool.
//
// The report is a Cobertura XML file as produced by OpenCppCoverage
// (https://github.com/OpenCppCoverage/OpenCppCoverage) against a Debug build
// of Lodestar's own test suite - see ci/run_coverage.ps1, which is the
// supported way to produce one. Only statement (line) coverage is imported:
// no tool in this project's current toolchain measures decision or MC/DC
// coverage for native MSVC binaries (OpenCppCoverage 0.9.9 is line-coverage
// only - verified via `OpenCppCoverage --help`, no branch/decision option
// exists). decisions_taken/decisions_total and conditions_satisfied/
// conditions_total are therefore left at 0 (honestly "not measured"), not
// fabricated. See PLAN.md S3.3 for what real decision/MC-DC would require
// (clang-cl + llvm-cov, or a commercial engine).

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/testforge/Coverage.h"

namespace lodestar::testforge {

// One <class filename="..."> element's aggregated line coverage.
struct FileCoverage {
    std::string filename;  // normalized to a repo-relative forward-slash path
    int statementsExecuted = 0;
    int statementsTotal = 0;
};

// Parses a Cobertura XML report (as produced by
// `OpenCppCoverage --export_type cobertura:<path>`) into one FileCoverage
// entry per source file. Files with zero <line> entries are omitted.
common::Result<std::vector<FileCoverage>> parseCoberturaReport(
    const std::string& xmlPath);

// Imports a Cobertura report into CoverageDao: one CoverageResult row per
// source file (scope = "module:<filename>"). The row id is derived
// deterministically from (runId, scope), so re-importing the same report
// under the same runId is an idempotent update, not a growing set of
// duplicate rows. Returns the number of files imported.
common::Result<int> importCoberturaReport(const std::string& xmlPath,
                                          const std::string& runId,
                                          CoverageDao& dao);

}  // namespace lodestar::testforge
