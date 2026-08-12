#pragma once
// core/testforge/Coverage.h
// Structural code coverage (S2 Phase 7): statement, decision, and MC/DC
// coverage percentages, plus persistence of coverage results.
//
// Statement coverage: fraction of statements executed by a test run.
// Decision coverage: fraction of decision outcomes (true/false branches)
//   exercised by a test run.
// MC/DC coverage: fraction of conditions that independently affect a
//   decision's outcome.
//
// Persistence lives in CoverageDao against the coverage_results table
// (migration 026_coverage.sql).
//
// S3 Phase 3: rows can now come from two sources. save()/load()/list() below
// accept a caller-supplied CoverageResult regardless of origin (unchanged
// API), but the actually-recommended way to populate real coverage is
// core/testforge/CoberturaImport.h + ci/run_coverage.ps1, which measure
// statement coverage with a real instrumentation tool (OpenCppCoverage)
// against the test suite, rather than a hand-supplied count.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::testforge {

// Returns the statement coverage percentage in [0,100]. If total is zero the
// result is 0.0 (nothing to cover).
double computeStatementCoverage(int executed, int total);

// Returns the decision coverage percentage in [0,100]. If total is zero the
// result is 0.0.
double computeDecisionCoverage(int decisionsTaken, int decisionsTotal);

// Returns the MC/DC coverage percentage in [0,100]. If total is zero the
// result is 0.0.
double computeMcdcCoverage(int conditionsSatisfied, int conditionsTotal);

// A persisted structural coverage result for one scope (e.g. a module or a
// test run). Stores the raw counts so percentages can be recomputed.
struct CoverageResult {
    std::string id;
    std::string runId;   // TestForge TestRun id this coverage belongs to
    std::string scope;   // e.g. "module:core/testforge/Coverage.cpp"
    int statementsExecuted = 0;
    int statementsTotal = 0;
    int decisionsTaken = 0;
    int decisionsTotal = 0;
    int conditionsSatisfied = 0;
    int conditionsTotal = 0;
    std::string recordedAt;
};

// Persistence for coverage results (migration 026_coverage.sql).
class CoverageDao {
public:
    explicit CoverageDao(persistence::Database& db) : db_(db) {}

    // Stores a coverage result (upsert by id).
    common::Result<void> save(const CoverageResult& r);

    // Loads a coverage result by id; returns nullopt if not found.
    common::Result<std::optional<CoverageResult>> load(const std::string& id);

    // Returns all stored coverage results, ordered by id.
    common::Result<std::vector<CoverageResult>> list();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::testforge
