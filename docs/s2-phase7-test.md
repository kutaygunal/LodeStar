# S2 Phase 7 Test Contract — Structural code coverage

> Written by the scrum-master BEFORE the Phase 7 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase7_tests
timeout 120 ./build/core/Release/lodestar_s2_phase7_tests.exe
```

## Test file
- **File:** `core/test/s2_phase7_tests.cpp`
- **CMake target:** `lodestar_s2_phase7_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_testforge`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase7_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 7 engineer must provide

### (A) Statement coverage
- `computeStatementCoverage(executed, total)` returns a percentage (0–100).

### (B) Decision coverage
- `computeDecisionCoverage(decisionsTaken, decisionsTotal)` returns a percentage (0–100).

### (C) MC/DC coverage
- `computeMcdcCoverage(conditionsSatisfied, conditionsTotal)` returns a percentage (0–100).

### (D) Persistence
- Coverage results can be stored and retrieved (add a migration).

## Test cases & expected behavior

### T1. statement coverage percentage
- 5 of 10 statements executed → 50%.

### T2. decision coverage percentage
- 3 of 4 decision outcomes exercised → 75%.

### T3. MC/DC coverage percentage
- 2 of 4 conditions independently affecting outcome → 50%.

### T4. coverage results persist
- Store a coverage result, retrieve it, and the retrieved value matches.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase7_tests
    test/s2_phase7_tests.cpp)
target_link_libraries(lodestar_s2_phase7_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_testforge)
target_compile_definitions(lodestar_s2_phase7_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
