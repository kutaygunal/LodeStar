# S2 Phase 4 Test Contract — AssureCheck semantic evidence evaluation

> Written by the scrum-master BEFORE the Phase 4 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase4_tests
timeout 120 ./build/core/Release/lodestar_s2_phase4_tests.exe
```

## Test file
- **File:** `core/test/s2_phase4_tests.cpp`
- **CMake target:** `lodestar_s2_phase4_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase4_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 4 engineer must provide

### (A) Objective-specific evaluation
- `evaluateObjective(objective, evidence)` returns a status based on the objective's
  semantic type, NOT just "any row exists". Different objective types require different
  evidence:
  - traceability → a trace link must exist.
  - verification → a passed test run must exist.
  - coverage → coverage evidence must exist.
  - review → an approved review must exist.

## Test cases & expected behavior

### T1. traceability objective requires a trace link
- A traceability objective with NO trace link → FAIL.
- The same objective WITH a trace link → PASS.

### T2. verification objective requires a passed run
- A verification objective with only a failed run → FAIL.
- With a passed run → PASS.

### T3. coverage objective requires coverage evidence
- A coverage objective with no coverage evidence → FAIL.
- With coverage evidence → PASS.

### T4. review objective requires an approved review
- A review objective with no approved review → FAIL.
- With an approved review → PASS.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase4_tests
    test/s2_phase4_tests.cpp)
target_link_libraries(lodestar_s2_phase4_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_s2_phase4_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
