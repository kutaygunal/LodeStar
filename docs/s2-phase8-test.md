# S2 Phase 8 Test Contract — Certification-ready reporting + traceability

> Written by the scrum-master BEFORE the Phase 8 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase8_tests
timeout 120 ./build/core/Release/lodestar_s2_phase8_tests.exe
```

## Test file
- **File:** `core/test/s2_phase8_tests.cpp`
- **CMake target:** `lodestar_s2_phase8_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`, `lodestar_testforge`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase8_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 8 engineer must provide

### (A) PDF export
- `exportPdf(report)` produces a PDF file (bytes) with a non-empty body.

### (B) Word export
- `exportWord(report)` produces a Word (docx) file (bytes) with a non-empty body.

### (C) ReQIF export
- `exportReqif(requirements, links)` produces a ReQIF file (bytes) containing the
  requirements and trace links.

### (D) Result→requirement traceability
- `traceResultToRequirements(resultId)` returns the requirement(s) the result verifies.

## Test cases & expected behavior

### T1. PDF export produces non-empty bytes
- `exportPdf(report)` returns a non-empty byte vector.

### T2. Word export produces non-empty bytes
- `exportWord(report)` returns a non-empty byte vector.

### T3. ReQIF export contains requirements + links
- `exportReqif(reqs, links)` returns a non-empty byte vector whose content references the
  requirement ids and the trace link.

### T4. result→requirement traceability
- `traceResultToRequirements(resultId)` returns the requirement(s) the result verifies
  (via trace links).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase8_tests
    test/s2_phase8_tests.cpp)
target_link_libraries(lodestar_s2_phase8_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck
    lodestar_testforge
    lodestar_tracelink)
target_compile_definitions(lodestar_s2_phase8_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
