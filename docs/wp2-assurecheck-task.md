# WP-2 Test Contract — AssureCheck compliance engine

> Written by the scrum-master BEFORE the WP-2 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-2 (Phase 11). The existing
> `docs/wp2-task.md` / `docs/wp2-test.md` and the `lodestar_wp2_tests` CMake
> target belong to the Phase-10 review/comment workflow, so this contract uses a
> distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp2_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp2_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp2_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-2 engineer must provide

### (A) Migration 020
`core/persistence/migrations/020_assurecheck_checks.sql` creates the
`assurance_checks` table that stores compliance-check results. Append-only and
idempotent (`IF NOT EXISTS`). Suggested:

```sql
CREATE TABLE IF NOT EXISTS assurance_checks (
    id          TEXT PRIMARY KEY,             -- UUID
    standard_id TEXT NOT NULL,
    item_id     TEXT NOT NULL,
    item_code   TEXT NOT NULL DEFAULT '',     -- A1-1 | D254-1 | ...
    status      TEXT NOT NULL DEFAULT 'NA',   -- PASS | FAIL | NA | WARNING
    dal_level   TEXT NOT NULL DEFAULT '',     -- item's DAL range (A | A-B | A-C | A-D)
    evidence    TEXT NOT NULL DEFAULT '',     -- "type:id;type:id" evidence links
    detail      TEXT NOT NULL DEFAULT '',
    checked_at  TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (standard_id) REFERENCES assurance_standards(id),
    FOREIGN KEY (item_id) REFERENCES assurance_checklist_items(id)
);
CREATE INDEX IF NOT EXISTS idx_assurance_checks_item
    ON assurance_checks(item_id);
CREATE INDEX IF NOT EXISTS idx_assurance_checks_status
    ON assurance_checks(status);
```

### (B) `ComplianceEngine` (new, `core/assurecheck/ComplianceEngine.h`)
```cpp
enum class CheckStatus { Pass, Fail, Na, Warning };

// One evidence link: a project entity that satisfies an objective.
struct EvidenceLink {
    std::string entityType;  // requirement | design | test_case | trace_link
    std::string entityId;
};

// The result of evaluating one checklist item.
struct CheckResult {
    std::string id;          // result id (UUID)
    std::string standardCode;
    std::string itemCode;
    std::string itemId;
    CheckStatus status;
    std::string dalLevel;    // item's DAL range
    std::vector<EvidenceLink> evidence;
    std::string detail;
};

// Counts of results by status for a standard.
struct CheckSummary {
    int total = 0;
    int pass = 0;
    int fail = 0;
    int na = 0;
    int warning = 0;
    int percent = 0;  // pass>0 ? (pass*100/total) : 0
};

class ComplianceEngine {
public:
    explicit ComplianceEngine(persistence::Database& db);

    // Evaluates every checklist item of the given standard against the project
    // data currently in the DB, for the given project DAL level. Returns one
    // CheckResult per item (applicable or NA). Does NOT persist.
    common::Result<std::vector<CheckResult>> runChecks(
        const std::string& standardCode, const std::string& dalLevel);

    // Persists a set of results into assurance_checks. Idempotent: replaces
    // any previously stored results for the same standard (no duplicates).
    common::Result<void> storeResults(
        const std::vector<CheckResult>& results);

    // Retrieves stored results for a standard, ordered by item seq.
    common::Result<std::vector<CheckResult>> resultsFor(
        const std::string& standardCode);

    // Summary counts (by status) for a standard from stored results.
    common::Result<CheckSummary> summaryFor(const std::string& standardCode);
};
```

### (C) Evaluation rule (deterministic — the engineer must implement exactly this)
For each checklist item of the standard, in `seq` order:

1. **DAL applicability.** Parse the item's `dalLevel` range (e.g. `"A"`, `"A-B"`,
   `"A-C"`, `"A-D"`). If the project `dalLevel` letter is NOT within `[first, last]`
   of the range, the item is **NA** (not applicable) and is skipped (no evidence).
2. **Evidence source.** For applicable items, map the item's `evidence` text to a
   project-data source using case-insensitive substring matching, in this order:
   - contains `"test"` (e.g. "Test results", "Test procedure", "Test result",
     "Coverage analysis") → **test_cases**
   - else contains `"traceab"` (e.g. "Traceability matrix", "Build traceability")
     → **trace_links**
   - else contains `"design"` OR `"architecture"` OR `"source code"` OR `"code"`
     OR `"build"` OR `"implementation"` OR `"partitioning"` → **design_items**
   - else → **requirements**
3. **Status.** Evaluate the mapped source:
   - **test_cases:** PASS if at least one `test_cases` row has
     `result_status = 'Passed'`; WARNING if there are test_cases but none passed;
     FAIL if there are no test_cases.
   - **trace_links:** PASS if at least one `trace_links` row exists; FAIL otherwise.
   - **design_items:** PASS if at least one `design_items` row exists; FAIL otherwise.
   - **requirements:** PASS if at least one `requirements` row exists; FAIL otherwise.
4. **Evidence links.** On PASS, populate `evidence` with the entity id(s) that
   satisfied the objective (e.g. the requirement id, design id, test_case id, or
   trace_link id). On FAIL/NA/WARNING, `evidence` is empty.

## Test cases & expected behavior

### T1. Migration 020 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `assurance_checks` table exists.

### T2. Empty project data → all applicable items FAIL
- `seedStandards()`; run `runChecks("DO-178C", "A")` on an empty DB.
- **Expect:** 82 results; every result `status == Fail`; `na == 0` (all DO-178C
  items apply to DAL A).

### T3. DAL applicability → NA for out-of-range DAL
- `runChecks("DO-178C", "E")` on an empty DB.
- **Expect:** 82 results; every result `status == Na` (no DO-178C item applies
  to DAL E).
- `runChecks("DO-178C", "B")` on an empty DB.
- **Expect:** 82 results; exactly 1 result is `Na` (item `A6-10`, whose DAL range
  is `"A"` only) and 81 are `Fail`.

### T4. Project data → PASS/FAIL/WARNING mix
- `seedStandards()`. Insert exactly: one `requirements` row (id `req1`), one
  `test_cases` row (id `tc1`, `result_status='Passed'`). Insert NO `design_items`
  and NO `trace_links`.
- `runChecks("DO-178C", "A")`.
- **Expect:** 82 results, `na == 0`; `pass > 0` and `fail > 0`; and these specific
  items have these statuses:
  - `A2-1` (evidence "HL requirements" → requirements) → **Pass**
  - `A2-4` (evidence "Traceability matrix" → trace_links) → **Fail**
  - `A2-9` (evidence "Architecture description" → design_items) → **Fail**
  - `A6-4` (evidence "Test results" → test_cases) → **Pass**

### T5. Evidence links populated on PASS
- Using the T4 data, find the result for `A2-1`.
- **Expect:** its `evidence` contains an `EvidenceLink` with
  `entityType == "requirement"` and `entityId == "req1"`.

### T6. storeResults + resultsFor round-trip
- Run `runChecks("DO-178C", "A")` on the T4 data; `storeResults(results)`.
- `resultsFor("DO-178C")`.
- **Expect:** returns 82 results; the result for `A2-1` has `status == Pass` and
  `itemCode == "A2-1"`.

### T7. summaryFor counts
- After T6, `summaryFor("DO-178C")`.
- **Expect:** `total == 82`, `na == 0`, `pass > 0`, `fail > 0`,
  `percent == pass*100/82`.

### T8. Idempotent storage
- Run `runChecks("DO-178C", "A")` on the T4 data; call `storeResults` twice.
- **Expect:** `resultsFor("DO-178C")` still returns exactly 82 results (no
  duplicates).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-2: AssureCheck compliance engine (migration 020) ---------
add_executable(lodestar_wp2_assurecheck_tests
    test/wp2_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp2_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_wp2_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp2_assurecheck_tests` (not
> `lodestar_wp2_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp2_tests` regression target.
