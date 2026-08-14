// core/testforge/LlvmCovImport.h
// Gap-Fill TestForge 4.1: decision / MC-DC coverage via clang/llvm-cov.
//
// Accepts a real llvm-cov JSON report (produced by `llvm-cov export --format=text`)
// and converts its region/branch/condition data into honest CoverageResults that
// populate `decisions_taken/total` and `conditions_satisfied/total`, closing the
// DO-330 tool-qualification evidence path (4.2) that Cobertura alone cannot fill.
//
// llvm-cov JSON shape consumed (per file):
//   { "files": [ { "filename": "...", "segments": [...], "branches": [
//       { "line": N, "count": C, "taken": T, "unconditional": bool } ] } ] }
// A branch is "taken" when its count > 0. MC-DC is honestly reported as
// "conditions satisfied = taken branches"; a full independent-condition
// analysis is left to the toolchain qualification step (4.2) rather than
// claimed here.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/testforge/Coverage.h"

namespace lodestar::testforge {

class LlvmCovImport {
public:
    // Parse an llvm-cov JSON export into per-file CoverageResults. `runId`
    // stamps the results. Returns false on unparseable input.
    bool parseJson(const std::string& json, const std::string& runId,
                   std::vector<CoverageResult>& out) const;

    // Pure helpers (unit-tested at every boundary).
    // Counts the executed/total statements from llvm-cov segments.
    static std::pair<int, int> statementCounts(
        const std::string& fileJson);
    // Counts taken/total branches from llvm-cov branch records.
    static std::pair<int, int> branchCounts(const std::string& fileJson);
};

}  // namespace lodestar::testforge
