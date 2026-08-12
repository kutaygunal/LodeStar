# WP-7 Test Contract — Coverage dashboard + charts

> Written by the scrum-master BEFORE the WP-7 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp7_dashboard_tests.cpp`
- **CMake target:** `lodestar_wp7_dashboard_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`,
  `lodestar_testforge`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`, `LODESTAR_TESTFORGE_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp7_dashboard_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Scope & approach (Qt UI WP)

WP-7 is a **Qt Widgets** UI work package: a **live coverage dashboard** (red/green
gaps) plus **status/priority/coverage charts**. It depends on WP-5 `CoverageService`
(executed results) and WP-6 (tree/detail). Following the WP-6 precedent, this
contract verifies the **Qt-independent wiring** the Qt views consume (pure C++,
testable without a display) and documents the **UI build acceptance** step.

## Contract the WP-7 engineer must provide

### (A) Qt-independent wiring layer — extend `UiWiringService`
Add to `core/tracelink/UiWiringService.h` (namespace `lodestar::tracelink`):

```cpp
// One live coverage dashboard row (red/green gap).
struct LiveCoverageRow {
    std::string requirementId;
    std::string requirementExternalId;
    bool designed = false;   // has >=1 Active satisfies link
    bool verified = false;   // has >=1 Active verifies link AND a passing run
    bool executed = false;   // has at least one recorded test run
    bool gapNoDesign = false;  // red: no design
    bool gapNoTest = false;    // red: no passing test
};

// Chart data for the dashboard.
struct CoverageCharts {
    struct Slice { std::string label; int count = 0; };
    std::vector<Slice> byStatus;    // Draft / Approved / ... counts
    std::vector<Slice> byPriority;  // High / Medium / Low / ... counts
    std::vector<Slice> byCoverage;  // Full / Partial / None counts
};

class UiWiringService {
    // ... existing refreshAll(), impact(), projectTree(), detail() ...

    // Live coverage: a requirement is `verified` only when it has an Active
    // verifies link AND a passing executed run (WP-5 CoverageService).
    common::Result<std::vector<LiveCoverageRow>> liveCoverage();

    // Chart data: status / priority / coverage distributions across all
    // requirements. byCoverage: Full = designed+verified, Partial = one of the
    // two, None = neither.
    common::Result<CoverageCharts> coverageCharts();
};
```

### (B) Qt views (not exercised here — Qt absent)
- `ui/CoverageDashboardView` renders the live dashboard (red/green gaps) from
  `liveCoverage()` and the charts from `coverageCharts()`. Not compiled here; the
  wiring it calls is what this contract verifies.

### (C) UI build acceptance (testing step, not this binary)
The UI shell must build with:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON
cmake --build build --config Release
```
**Expect:** `lodestar_ui` compiles and links against Qt 6.8.2.

## Test cases & expected behavior

### T1. liveCoverage() reflects executed results
- Build: requirement R, test TC (verifies R). No run recorded.
- **Expect:** R has `designed=false`, `verified=false`, `executed=false`,
  `gapNoDesign=true`, `gapNoTest=true`.

### T2. Passing run makes requirement verified (green)
- `recordRun("run-1", tcId, true)` (via WP-5 `CoverageService`).
- **Expect:** R has `verified=true`, `executed=true`, `gapNoTest=false`.

### T3. Failed run does not verify (red)
- On a fresh requirement R2 + TC2 (verifies), `recordRun("run-2", tc2Id, false)`.
- **Expect:** R2 has `executed=true` but `verified=false`, `gapNoTest=true`.

### T4. coverageCharts() byStatus distribution
- Create requirements with statuses Draft, Approved, Approved.
- **Expect:** `byStatus` has a Draft slice of count 1 and an Approved slice of
  count 2.

### T5. coverageCharts() byPriority distribution
- Create requirements with priorities High, High, Medium.
- **Expect:** `byPriority` has High=2, Medium=1.

### T6. coverageCharts() byCoverage distribution
- One fully covered (designed+verified), one partial (designed only), one none.
- **Expect:** `byCoverage` has Full=1, Partial=1, None=1.

### T7. Acceptance: live change flips the dashboard
- Build the graph, snapshot `liveCoverage()` (R unverified).
- Record a passing run for TC, re-query.
- **Expect:** R flips to `verified=true`; `coverageCharts()` byCoverage reflects it.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp7_dashboard_tests
    test/wp7_dashboard_tests.cpp)
target_link_libraries(lodestar_wp7_dashboard_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink
    lodestar_testforge)
target_compile_definitions(lodestar_wp7_dashboard_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations"
    LODESTAR_TESTFORGE_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp7_dashboard_tests` (not `lodestar_wp7_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp7_tests` (view models)
> regression target. This test is Qt-independent and lives in `core/test/`; the Qt
> UI build is verified separately with `LODESTAR_BUILD_UI=ON`.
