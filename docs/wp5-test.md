# WP-5 Test Contract — TestForge coverage wiring

> Written by the scrum-master BEFORE the WP-5 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp5_coverage_tests.cpp`
- **CMake target:** `lodestar_wp5_coverage_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`,
  `lodestar_testforge`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`, `LODESTAR_TESTFORGE_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp5_coverage_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-5 engineer must provide

### (A) Migration 017 (if a mapping table is required)
`core/persistence/migrations/017_*.sql` records which TestForge test run executed a
given traceability test case, so live coverage reflects executed results. If the
engineer can wire coverage purely from existing TestForge run data + traceability
links, this migration may be omitted. Append-only and idempotent (`IF NOT EXISTS`).
Suggested:

```sql
CREATE TABLE IF NOT EXISTS test_run_coverage (
    id          TEXT PRIMARY KEY,          -- UUID
    run_id      TEXT NOT NULL,             -- TestForge TestRun id
    test_case_id TEXT NOT NULL,            -- traceability test_case entity UUID
    passed      INTEGER NOT NULL DEFAULT 0, -- 1 if the run passed
    executed_at TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_run_coverage_tc ON test_run_coverage(test_case_id);
```

### (B) `CoverageService` (new, `core/tracelink/CoverageService.h`)
```cpp
// One requirement's coverage row reflecting EXECUTED TestForge results.
struct ExecutedCoverageRow {
    std::string requirementId;
    std::string requirementExternalId;
    bool designed = false;      // has >=1 Active satisfies link
    bool verified = false;      // has >=1 Active verifies link AND a passing run
    bool executed = false;      // has at least one recorded test run
};

class CoverageService {
public:
    explicit CoverageService(persistence::Database& db);

    // Records that a TestForge run executed a traceability test case.
    // passed = (run.status == Passed).
    common::Result<void> recordRun(const std::string& runId,
                                   const std::string& testCaseId,
                                   bool passed);

    // Live coverage: a requirement is `verified` only when it has an Active
    // verifies link to a test case AND that test case has a passing recorded run.
    common::Result<std::vector<ExecutedCoverageRow>> executedCoverage();
};
```

## Test cases & expected behavior

### T1. Migration applies
- Open a fresh DB and run migrations.
- **Expect:** migrations succeed (including 017 if present).

### T2. Unverified before any run
- Build: requirement R, test case TC, link TC verifies R.
- **Expect:** `executedCoverage()` reports R with `designed=false`, `verified=false`,
  `executed=false`.

### T3. Passing run makes requirement verified
- `recordRun("run-1", tcId, true)`.
- **Expect:** `executedCoverage()` reports R with `verified=true`, `executed=true`.

### T4. Failed run does not verify
- On a fresh requirement R2 + TC2 (verifies), `recordRun("run-2", tc2Id, false)`.
- **Expect:** R2 has `executed=true` but `verified=false`.

### T5. Coverage reflects executed results (live)
- Record a passing run for TC, then a later failing run for the same TC.
- **Expect:** R is no longer `verified` (latest executed result governs), but
  `executed` stays true.

### T6. End-to-end with TestRunner
- Build a `TestProcedure` with a step, run it via `testforge::TestRunner` with a
  `MockMeasurementProvider` that satisfies the step, save the run, then
  `recordRun` from the run's status.
- **Expect:** a passing run verifies the linked requirement; a failing run does not.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp5_coverage_tests
    test/wp5_coverage_tests.cpp)
target_link_libraries(lodestar_wp5_coverage_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink
    lodestar_testforge)
target_compile_definitions(lodestar_wp5_coverage_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations"
    LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp5_coverage_tests` (not `lodestar_wp5_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp5_tests` regression target.
