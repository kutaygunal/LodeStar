# WP-3 Test Contract — AssureCheck evidence + integration

> Written by the scrum-master BEFORE the WP-3 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-3 (Phase 11). The existing
> `docs/wp3-task.md` / `docs/wp3-test.md` and the `lodestar_wp3_tests` CMake
> target belong to the Phase-10 compliance-templates workflow, so this contract
> uses a distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp3_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp3_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`,
  `lodestar_tracelink`, `lodestar_testforge`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp3_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Overview
WP-3 wires the WP-2 `ComplianceEngine` to real project data. It pulls
requirements / design / test / trace entities from **TraceLink**
(`core/tracelink/TraceLinkService`) and test-run results from **TestForge**
(`core/testforge/TestForgeDao`) into an `EvidenceSnapshot`, then evaluates the
checklist against that snapshot. No new migration is required — WP-3 reuses the
existing `requirements`, `design_items`, `test_cases`, `trace_links`,
`test_runs` tables and the WP-2 `assurance_checks` table.

## Contract the WP-3 engineer must provide

### (A) `EvidenceSnapshot` (added to `core/assurecheck/ComplianceEngine.h`)
```cpp
// Project data + test-run results gathered from TraceLink and TestForge.
struct EvidenceSnapshot {
    std::vector<std::string> requirementIds;  // TraceLink requirements
    std::vector<std::string> designIds;       // TraceLink design items
    std::vector<std::string> testCaseIds;     // TraceLink test cases
    std::vector<std::string> traceLinkIds;    // TraceLink links
    std::vector<std::string> passedRunIds;    // TestForge runs, status Passed
    std::vector<std::string> failedRunIds;    // TestForge runs, status Failed
    std::vector<std::string> blockedRunIds;   // TestForge runs, status Blocked
};
```

### (B) `ComplianceEngine` addition (`core/assurecheck/ComplianceEngine.h`)
Add one method to the existing WP-2 `ComplianceEngine`:
```cpp
// Evaluates every checklist item of the given standard against an explicit
// evidence snapshot (instead of reading raw DB tables). Same DAL-applicability
// and status rules as runChecks. Does NOT persist.
common::Result<std::vector<CheckResult>> runChecksWithEvidence(
    const std::string& standardCode, const std::string& dalLevel,
    const EvidenceSnapshot& evidence);
```

### (C) `EvidenceService` (new, `core/assurecheck/EvidenceService.h`)
```cpp
class EvidenceService {
public:
    EvidenceService(persistence::Database& db,
                    tracelink::TraceLinkService& tl,
                    testforge::TestForgeDao& tf);

    // Collects evidence from TraceLink (entities + links) and TestForge
    // (test runs) into a snapshot.
    common::Result<EvidenceSnapshot> collect();

    // Collects evidence, then runs checks for a standard against it.
    common::Result<std::vector<CheckResult>> runChecks(
        const std::string& standardCode, const std::string& dalLevel);

    // Persists results into assurance_checks (idempotent per standard).
    common::Result<void> storeResults(
        const std::vector<CheckResult>& results);

    // Retrieves stored results for a standard, ordered by item seq.
    common::Result<std::vector<CheckResult>> resultsFor(
        const std::string& standardCode);
};
```

### (D) `TestForgeDao` addition (`core/testforge/TestForgeDao.h`)
Add one method so the evidence collector can enumerate saved runs:
```cpp
// Returns all saved test runs (any status).
common::Result<std::vector<TestRun>> listRuns();
```

### (E) Evaluation rule for `runChecksWithEvidence` (deterministic)
For each checklist item of the standard, in `seq` order:

1. **DAL applicability.** Parse the item's `dalLevel` range (e.g. `"A"`, `"A-B"`,
   `"A-C"`, `"A-D"`). If the project `dalLevel` letter is NOT within
   `[first, last]` of the range, the item is **NA** and is skipped (no evidence).
2. **Evidence source.** Map the item's `evidence` text to a source using
   case-insensitive substring matching, in this order:
   - contains `"test"` (e.g. "Test results", "Test procedure", "Test result",
     "Coverage analysis") → **test_cases**
   - else contains `"traceab"` (e.g. "Traceability matrix", "Build traceability")
     → **trace_links**
   - else contains `"design"` OR `"architecture"` OR `"source code"` OR `"code"`
     OR `"build"` OR `"implementation"` OR `"partitioning"` → **design_items**
   - else → **requirements**
3. **Status.** Evaluate the mapped source against the snapshot:
   - **test_cases:** PASS if `passedRunIds` is non-empty; WARNING if there are
     test runs (`passedRunIds` + `failedRunIds` + `blockedRunIds` non-empty) but
     none passed; FAIL if there are no test runs.
   - **trace_links:** PASS if `traceLinkIds` is non-empty; FAIL otherwise.
   - **design_items:** PASS if `designIds` is non-empty; FAIL otherwise.
   - **requirements:** PASS if `requirementIds` is non-empty; FAIL otherwise.
4. **Evidence links.** On PASS, populate `evidence` with the entity id(s) that
   satisfied the objective (requirement id, design id, trace_link id, or a
   passed run id for test items). On FAIL/NA/WARNING, `evidence` is empty.

### (F) `EvidenceService::collect()` behavior
- `requirementIds` = ids of all TraceLink `Requirement` entities.
- `designIds` = ids of all TraceLink `Design` entities.
- `testCaseIds` = ids of all TraceLink `TestCase` entities.
- `traceLinkIds` = ids of all TraceLink links (`allLinks()`).
- `passedRunIds` / `failedRunIds` / `blockedRunIds` = ids of TestForge runs
  (`listRuns()`) classified by `RunStatus` (Passed / Failed / Blocked).

## Test cases & expected behavior

### T1. collect() pulls TraceLink entities
- Fresh DB, run migrations, `seedStandards()`.
- Via `TraceLinkService`, add one `Requirement`, one `Design`, one `TestCase`,
  and one `Link` (requirement → design, relation `satisfies`).
- `EvidenceService::collect()`.
- **Expect:** `requirementIds.size()==1`, `designIds.size()==1`,
  `testCaseIds.size()==1`, `traceLinkIds.size()==1`.

### T2. collect() pulls TestForge test-run results
- Via `TestForgeDao`, save a `TestRun` with `status == RunStatus::Passed`.
- `collect()`.
- **Expect:** `passedRunIds.size()==1`; `failedRunIds` and `blockedRunIds` empty.

### T3. runChecks uses TraceLink evidence → PASS for requirements/design/trace
- Add one `Requirement`, one `Design`, one `Link` (requirement → design) via
  `TraceLinkService`. No test runs.
- `runChecks("DO-178C", "A")`.
- **Expect:** 82 results, `na == 0`; `A2-1` (requirements) → **Pass**,
  `A2-9` (design) → **Pass**, `A2-4` (trace) → **Pass**.

### T4. runChecks uses TestForge passed runs → PASS for test items
- Save a `TestRun` with `status == RunStatus::Passed` via `TestForgeDao`.
- `runChecks("DO-178C", "A")`.
- **Expect:** `A6-4` (test) → **Pass**.

### T5. failed runs (no passed) → WARNING for test items
- Save a `TestRun` with `status == RunStatus::Failed` (no passed runs).
- `runChecks("DO-178C", "A")`.
- **Expect:** `A6-4` → **Warning**.

### T6. no test runs → FAIL for test items
- No TestForge runs saved.
- `runChecks("DO-178C", "A")`.
- **Expect:** `A6-4` → **Fail**.

### T7. Evidence links populated on PASS + storeResults/resultsFor round-trip
- Add one `Requirement` (id `req1`) via `TraceLinkService`.
- `runChecks("DO-178C", "A")`; find the result for `A2-1`.
- **Expect:** its `evidence` contains an `EvidenceLink` with
  `entityType == "requirement"` and `entityId == "req1"`.
- `storeResults(results)`; `resultsFor("DO-178C")`.
- **Expect:** returns 82 results; the result for `A2-1` has `status == Pass`.

### T8. DAL applicability still holds with evidence
- Add a `Requirement` and a passed `TestRun`.
- `runChecks("DO-178C", "E")`.
- **Expect:** 82 results; every result `status == Na` (no DO-178C item applies
  to DAL E, regardless of evidence).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-3: AssureCheck evidence + integration (TraceLink/TestForge) ---
add_executable(lodestar_wp3_assurecheck_tests
    test/wp3_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp3_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck
    lodestar_tracelink
    lodestar_testforge)
target_compile_definitions(lodestar_wp3_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp3_assurecheck_tests` (not
> `lodestar_wp3_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp3_tests` regression target.
