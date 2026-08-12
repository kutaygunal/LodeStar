# WP-6 Test Contract — AssureCheck REST API + compliance dashboard

> Written by the scrum-master BEFORE the WP-6 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-6 (Phase 11). The existing
> `docs/wp6-task.md` / `docs/wp6-test.md` and the `lodestar_wp6_tests` CMake
> target belong to the Phase-10 Qt UI shell workflow, so this contract uses a
> distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp6_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp6_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`,
  `lodestar_api`, `lodestar_adapters` (+ `ws2_32` on Windows)
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp6_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures). The API tests drive a real
  in-process HTTP server over localhost with `HttpClient`, exactly like the
  Phase-10 `wp6_api_tests.cpp`.

## Overview
WP-6 exposes AssureCheck over REST (`/assurecheck` endpoints) and provides the
Qt-independent dashboard data (per-standard objective coverage + status) that
the Qt compliance dashboard consumes. It builds on the WP-1 `AssureCheckService`,
WP-2 `ComplianceEngine`, WP-4 `ReportService` / `CoverageSummary`, and the
`lodestar::Json` value type. No new migration is required.

## Contract the WP-6 engineer must provide

### (A) `DashboardService` (new, `core/assurecheck/DashboardService.h`)
```cpp
// One standard's row on the compliance dashboard.
struct DashboardStandard {
    std::string code;
    std::string name;
    CoverageSummary coverage;   // from WP-4
};

class DashboardService {
public:
    explicit DashboardService(persistence::Database& db);

    // Per-standard objective coverage computed from stored assurance_checks
    // results. Standards with no stored results are omitted. Ordered by code.
    common::Result<std::vector<DashboardStandard>> dashboard();
};
```

### (B) `AssureCheckApiServer` (new, `core/api/AssureCheckApiServer.h`)
```cpp
class AssureCheckApiServer {
public:
    AssureCheckApiServer(assurecheck::AssureCheckService& standards,
                         assurecheck::ComplianceEngine& engine,
                         assurecheck::ReportService& reports,
                         assurecheck::DashboardService& dashboard);

    // Register every /assurecheck route on the server.
    void setup(HttpServer& server);
};
```

### (C) Routes (method, path, success status, request body, response shape)
```
GET    /assurecheck/standards
       -> 200 {"standards":[{"code","name","description"}]}
GET    /assurecheck/standards/{code}
       -> 200 {"code","name","description"} | 404
POST   /assurecheck/checks
       body {"standard","dal"}
       -> 200 {"results":[...]} | 400 missing standard/dal
GET    /assurecheck/summary?standard=<code>
       -> 200 {"summary":{"total","pass","fail","na","warning","percent"}}
       -> 400 missing standard
GET    /assurecheck/dashboard
       -> 200 {"standards":[{"code","name","coverage":{...}}]}
```

- **POST /assurecheck/checks:** runs `ComplianceEngine::runChecks(standard, dal)`
  and persists via `storeResults`. Returns the results array.
- **GET /assurecheck/summary:** returns `ComplianceEngine::summaryFor(standard)`.
- **GET /assurecheck/dashboard:** returns `DashboardService::dashboard()`.
- **Error model** for every 400/404/500:
  `{"error":{"code":<int>,"message":"..."}}`.

## Test cases & expected behavior

### T1. GET /assurecheck/standards returns the five standards
- Fresh DB, run migrations, `seedStandards()`.
- Start the server; `GET /assurecheck/standards`.
- **Expect:** `200`; body has `standards` array of size 5.

### T2. GET /assurecheck/standards/{code} returns a standard; missing → 404
- `GET /assurecheck/standards/DO-178C`.
- **Expect:** `200`; `code == "DO-178C"`.
- `GET /assurecheck/standards/NOPE`.
- **Expect:** `404` with `error.code` present.

### T3. POST /assurecheck/checks runs + stores results; missing standard → 400
- Insert one `requirements` row, one `design_items` row, one `test_cases` row
  (`result_status='Passed'`), and one `trace_links` row.
- `POST /assurecheck/checks` body `{"standard":"DO-178C","dal":"A"}`.
- **Expect:** `200`; body `results` array of size 82.
- `POST /assurecheck/checks` body `{"dal":"A"}` (no standard).
- **Expect:** `400` with `error.code` present.

### T4. GET /assurecheck/summary?standard= returns summary counts
- After T3's POST, `GET /assurecheck/summary?standard=DO-178C`.
- **Expect:** `200`; `summary.total == 82`, `summary.na == 0`, `summary.pass == 82`.
- `GET /assurecheck/summary` (no standard).
- **Expect:** `400` with `error.code` present.

### T5. GET /assurecheck/dashboard returns per-standard coverage
- After T3's POST, `GET /assurecheck/dashboard`.
- **Expect:** `200`; `standards` array contains an entry with
  `code == "DO-178C"` and `coverage.total == 82`.

### T6. DashboardService.dashboard() returns per-standard coverage from stored results
- Fresh DB, run migrations, `seedStandards()`.
- Insert the T3 data; run `ComplianceEngine::runChecks("DO-178C","A")` and
  `storeResults`.
- `DashboardService::dashboard()`.
- **Expect:** returns one entry with `code == "DO-178C"`,
  `coverage.total == 82`, `coverage.pass == 82`, `coverage.percent == 100`.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-6: AssureCheck REST API + compliance dashboard -----------
add_executable(lodestar_wp6_assurecheck_tests
    test/wp6_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp6_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck
    lodestar_api
    lodestar_adapters)
if(WIN32)
    target_link_libraries(lodestar_wp6_assurecheck_tests PRIVATE ws2_32)
endif()
target_compile_definitions(lodestar_wp6_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp6_assurecheck_tests` (not
> `lodestar_wp6_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp6_tests` regression target.
