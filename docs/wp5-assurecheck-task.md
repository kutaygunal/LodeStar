# WP-5 Test Contract — AssureCheck performance + hardening

> Written by the scrum-master BEFORE the WP-5 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-5 (Phase 11). The existing
> `docs/wp5-task.md` / `docs/wp5-test.md` and the `lodestar_wp5_tests` CMake
> target belong to the Phase-10 import/export workflow, so this contract uses a
> distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp5_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp5_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp5_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Overview
WP-5 hardens the AssureCheck engine for scale: indexed lookups, batched
(transactional) evaluation, incremental re-check of only the affected items, and
a 10k+ entity performance budget. It builds on the WP-2 `ComplianceEngine` /
`CheckResult` types and the WP-2 `assurance_checks` table.

## Contract the WP-5 engineer must provide

### (A) Migration 021
`core/persistence/migrations/021_assurecheck_perf.sql` adds the performance
indexes that make compliance lookups fast. Append-only and idempotent
(`IF NOT EXISTS`). Suggested:

```sql
CREATE INDEX IF NOT EXISTS idx_assurance_checks_standard
    ON assurance_checks(standard_id);
CREATE INDEX IF NOT EXISTS idx_assurance_items_code
    ON assurance_checklist_items(item_code);
```

> The WP-1 migration 019 already creates `idx_assurance_items_standard`; the
> WP-2 migration 020 already creates `idx_assurance_checks_item` and
> `idx_assurance_checks_status`. WP-5 adds the two indexes above.

### (B) `PerformanceService` (new, `core/assurecheck/PerformanceService.h`)
```cpp
class PerformanceService {
public:
    explicit PerformanceService(persistence::Database& db);

    // Batched evaluation: evaluates every checklist item of the given standard
    // against the project data and stores the results atomically in a single
    // transaction (BEGIN IMMEDIATE ... COMMIT). Returns the results.
    common::Result<std::vector<CheckResult>> evaluateBatched(
        const std::string& standardCode, const std::string& dalLevel);

    // Incremental re-check: re-evaluates only the checklist items whose
    // evidence source is among changedSources. changedSources values are
    // "requirement", "design", "test_case", "trace_link", "test_run".
    // Returns the affected subset (only those items). Does NOT persist.
    common::Result<std::vector<CheckResult>> recheckIncremental(
        const std::string& standardCode, const std::string& dalLevel,
        const std::vector<std::string>& changedSources);
};
```

### (C) Behavior
- **evaluateBatched:** equivalent to the WP-2 `ComplianceEngine::runChecks`
  (same DAL-applicability and status rules) but runs the whole standard in one
  transaction and persists the results via `storeResults` semantics (idempotent
  per standard). Returns the 82 (or per-standard) results.
- **recheckIncremental:** maps each checklist item to an evidence source using
  the WP-2/3 rule (case-insensitive substring):
  - contains `"test"` → **test_cases**
  - else contains `"traceab"` → **trace_links**
  - else contains `"design"`/`"architecture"`/`"source code"`/`"code"`/`"build"`/
    `"implementation"`/`"partitioning"` → **design_items**
  - else → **requirements**
  Then maps `changedSources` to sources: `"test_run"`/`"test_case"` →
  test_cases; `"trace_link"` → trace_links; `"design"` → design_items;
  `"requirement"` → requirements. An item is re-evaluated iff its source is in
  the changed set. Returns only the affected items (evaluated with the current
  project data), ordered by seq.

## Test cases & expected behavior

### T1. WAL mode enabled
- Open a fresh DB.
- **Expect:** `PRAGMA journal_mode` returns `"wal"`.

### T2. Performance indexes exist
- Open a fresh DB and run migrations.
- **Expect:** all of these indexes exist in `sqlite_master`:
  `idx_assurance_checks_standard`, `idx_assurance_items_code`,
  `idx_assurance_checks_item`, `idx_assurance_checks_status`,
  `idx_assurance_items_standard`.

### T3. Batched evaluation returns + stores 82 results
- Fresh DB, run migrations, `seedStandards()`.
- Insert one `requirements` row, one `design_items` row, one `test_cases` row
  (`result_status='Passed'`), and one `trace_links` row.
- `PerformanceService::evaluateBatched("DO-178C", "A")`.
- **Expect:** returns 82 results; `resultsFor("DO-178C")` returns 82 results;
  the result for `A2-1` has `status == Pass`.

### T4. Incremental re-check returns only requirement-source items
- Same data as T3; `evaluateBatched("DO-178C", "A")`.
- `recheckIncremental("DO-178C", "A", {"requirement"})`.
- **Expect:** the returned subset contains a result for `A2-1` (requirements)
  and does NOT contain a result for `A6-4` (test).

### T5. Incremental re-check returns only test-source items
- `recheckIncremental("DO-178C", "A", {"test_run"})`.
- **Expect:** the returned subset contains a result for `A6-4` (test) and does
  NOT contain a result for `A2-1` (requirements).

### T6. 10k scale: batched evaluation completes within budget
- Fresh DB, run migrations, `seedStandards()`.
- Inside a single `BEGIN IMMEDIATE` transaction, insert 10,000 `requirements`
  rows and 10,000 `trace_links` rows (plus one `design_items` row and one
  `test_cases` row with `result_status='Passed'`), then commit.
- `PerformanceService::evaluateBatched("DO-178C", "A")`.
- **Expect:** returns 82 results; every result `status == Pass`; the whole
  load + evaluate completes within a finite budget of 60,000 ms.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-5: AssureCheck performance + hardening (migration 021) ----
add_executable(lodestar_wp5_assurecheck_tests
    test/wp5_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp5_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_wp5_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp5_assurecheck_tests` (not
> `lodestar_wp5_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp5_tests` regression target.
