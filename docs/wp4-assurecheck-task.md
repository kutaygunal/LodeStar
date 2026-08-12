# WP-4 Test Contract — AssureCheck compliance reporting

> Written by the scrum-master BEFORE the WP-4 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-4 (Phase 11). The existing
> `docs/wp4-task.md` / `docs/wp4-test.md` and the `lodestar_wp4_tests` CMake
> target belong to the Phase-10 audit/baseline workflow, so this contract uses
> a distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp4_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp4_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`,
  `lodestar_adapters`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp4_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Overview
WP-4 turns WP-2/3 compliance results into certification-ready reports per
standard and per DAL, with objective-coverage percentages, and exports them as
HTML, CSV, and JSON. It builds on the WP-2 `CheckResult` / `CheckSummary` types
and the `lodestar::Json` value type (`core/adapters/Json.h`). No new migration
is required — WP-4 reads the existing `assurance_checklist_items` table for
objective text and consumes `CheckResult` vectors produced by the engine.

## Contract the WP-4 engineer must provide

### (A) `ReportService` (new, `core/assurecheck/ReportService.h`)
```cpp
// One row of a report: a single checklist item's result.
struct ReportRow {
    std::string itemCode;
    std::string objective;   // objective text from the checklist
    std::string dalLevel;    // item's DAL range (A | A-B | A-C | A-D)
    std::string status;      // PASS | FAIL | NA | WARNING
    std::string evidence;    // evidence links summary
};

// Objective-coverage summary for a report.
struct CoverageSummary {
    int total = 0;        // number of checklist items
    int applicable = 0;   // total - na
    int pass = 0;
    int fail = 0;
    int na = 0;
    int warning = 0;
    int percent = 0;      // applicable>0 ? pass*100/applicable : 0
};

// A full report for one standard at one project DAL.
struct ComplianceReport {
    std::string standardCode;
    std::string standardName;
    std::string dalLevel;   // project DAL
    CoverageSummary coverage;
    std::vector<ReportRow> rows;   // ordered by item seq
};

class ReportService {
public:
    explicit ReportService(persistence::Database& db);

    // Builds a report from results for a standard + project DAL. Rows are
    // ordered by item seq; objective text is looked up from the checklist.
    common::Result<ComplianceReport> buildReport(
        const std::string& standardCode, const std::string& dalLevel,
        const std::vector<CheckResult>& results);

    // Computes the coverage summary for a report.
    CoverageSummary coverage(const ComplianceReport& report);

    // Exports a report to a self-contained HTML document.
    common::Result<std::string> toHtml(const ComplianceReport& report);

    // Exports a report to CSV (header row + one row per item).
    common::Result<std::string> toCsv(const ComplianceReport& report);

    // Exports a report to a structured JSON object.
    common::Result<Json> toJson(const ComplianceReport& report);
};
```

### (B) Behavior
- **buildReport:** for each `CheckResult`, produce a `ReportRow` with `itemCode`,
  `status` (uppercase string: `PASS`/`FAIL`/`NA`/`WARNING`), `dalLevel` (the
  item's DAL range), and `evidence` (a compact summary of the evidence links,
  e.g. `"requirement:req1"` joined by `;`). Look up `objective` from
  `assurance_checklist_items` by `item_code`. Order rows by item `seq`. Compute
  `coverage` from the rows.
- **coverage:** `total` = number of rows; `na` = count of `NA` rows;
  `applicable` = `total - na`; `pass`/`fail`/`warning` = counts of the
  corresponding statuses; `percent` = `applicable>0 ? pass*100/applicable : 0`.
- **toHtml:** a self-contained HTML document (starts with `<!DOCTYPE html>` or
  `<html`) containing the standard code, the project DAL, the coverage percent,
  and a per-item table with the statuses.
- **toCsv:** a CSV document whose first line is exactly
  `item_code,objective,dal_level,status,evidence` followed by one line per row
  (fields comma-separated, no quoting required).
- **toJson:** a `Json` object with keys `standard`, `standard_name`, `dal`,
  `coverage` (object with `total`, `applicable`, `pass`, `fail`, `na`,
  `warning`, `percent`), and `rows` (array of objects with `item_code`,
  `objective`, `dal_level`, `status`, `evidence`).

## Test cases & expected behavior

### T1. buildReport computes coverage (all PASS)
- Fresh DB, run migrations, `seedStandards()`.
- Insert one `requirements` row, one `design_items` row, one `test_cases` row
  (`result_status='Passed'`), and one `trace_links` row.
- `ComplianceEngine::runChecks("DO-178C", "A")` → results.
- `ReportService::buildReport("DO-178C", "A", results)`.
- **Expect:** `coverage.total == 82`, `coverage.na == 0`, `coverage.pass == 82`,
  `coverage.fail == 0`, `coverage.percent == 100`.

### T2. Coverage with NA (DAL B)
- Same data as T1; `runChecks("DO-178C", "B")` → results.
- `buildReport("DO-178C", "B", results)`.
- **Expect:** `coverage.total == 82`, `coverage.na == 1` (item `A6-10`),
  `coverage.applicable == 81`, `coverage.pass == 81`, `coverage.percent == 100`.

### T3. Report rows include objective text
- `buildReport("DO-178C", "A", results)` from T1 data.
- **Expect:** the row with `itemCode == "A2-1"` has
  `objective == "High-level requirements are developed"` and `status == "PASS"`.

### T4. toHtml produces a valid HTML report
- `buildReport` then `toHtml`.
- **Expect:** output contains `"<html"` (case-insensitive), `"DO-178C"`, `"PASS"`,
  and `"100"` (the coverage percent).

### T5. toCsv produces CSV with header + rows
- `buildReport` then `toCsv`.
- **Expect:** the first line is exactly
  `item_code,objective,dal_level,status,evidence`; the output contains a line
  containing `"A2-1"` and `"PASS"`.

### T6. toJson produces structured JSON
- `buildReport` then `toJson`.
- **Expect:** `json["standard"].asString() == "DO-178C"`,
  `json["dal"].asString() == "A"`,
  `json["coverage"]["total"].asNumber() == 82`,
  `json["rows"].size() == 82`.

### T7. Report per DAL (DAL B has an NA row)
- `buildReport("DO-178C", "B", results)` from T2 data.
- **Expect:** `rows` contains a row with `itemCode == "A6-10"` and
  `status == "NA"`.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-4: AssureCheck compliance reporting (HTML/CSV/JSON) ------
add_executable(lodestar_wp4_assurecheck_tests
    test/wp4_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp4_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck
    lodestar_adapters)
target_compile_definitions(lodestar_wp4_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp4_assurecheck_tests` (not
> `lodestar_wp4_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp4_tests` regression target.
