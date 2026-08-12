# S2 Phase 2 Test Contract — Web / browser layer over the REST API

> Written by the scrum-master BEFORE the Phase 2 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase2_tests
timeout 120 ./build/core/Release/lodestar_s2_phase2_tests.exe
```

## Test file
- **File:** `core/test/s2_phase2_tests.cpp`
- **CMake target:** `lodestar_s2_phase2_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_api`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase2_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 2 engineer must provide

### (A) Web layer over the REST API
- A web server (reusing `core/api/HttpServer`) serves:
  - `GET /web/` → an HTML page.
  - `GET /web/requirements` → requirements as HTML/JSON.
  - `GET /web/trace` → the trace matrix as HTML/JSON.
  - `GET /web/assure` → AssureCheck compliance summary as HTML/JSON.

### (B) Auth-aware
- The web layer honors Phase 1 auth: a `viewer` can read; an `editor` can review.

## Test cases & expected behavior

### T1. web root serves an HTML page
- `GET /web/` returns a 200 with an HTML body (contains `<html` or `<!DOCTYPE`).

### T2. requirements endpoint returns data
- `GET /web/requirements` returns a 200 with a body containing the seeded requirement's
  external id (e.g. `REQ-001`).

### T3. trace endpoint returns the matrix
- `GET /web/trace` returns a 200 with a body containing the trace link data.

### T4. assure endpoint returns compliance summary
- `GET /web/assure` returns a 200 with a body containing a compliance summary (e.g. a
  pass count or percentage).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase2_tests
    test/s2_phase2_tests.cpp)
target_link_libraries(lodestar_s2_phase2_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_api)
target_compile_definitions(lodestar_s2_phase2_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
