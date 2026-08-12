# S2 Phase 5 Test Contract — TestForge test-case design intelligence

> Written by the scrum-master BEFORE the Phase 5 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase5_tests
timeout 120 ./build/core/Release/lodestar_s2_phase5_tests.exe
```

## Test file
- **File:** `core/test/s2_phase5_tests.cpp`
- **CMake target:** `lodestar_s2_phase5_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_testforge`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase5_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 5 engineer must provide

### (A) Equivalence-class derivation
- Given a requirement with an input range (e.g. `speed` in `[0, 120]`), derive the
  valid equivalence class(es) and invalid classes (below min, above max).

### (B) Boundary-value derivation
- For each equivalence-class boundary, derive boundary values: min, min+1, nominal,
  max-1, max, and just-outside (min-1, max+1).

### (C) Test-case generation
- Produce concrete test cases (steps + expected result) from the derived classes/boundaries.
- Each generated test case has a unique id and a non-empty list of steps.

## Test cases & expected behavior

### T1. equivalence classes derived for a range
- For a requirement with input range `[0, 120]`, `deriveEquivalenceClasses` returns
  at least 3 classes: valid `[0,120]`, invalid `<0`, invalid `>120`.

### T2. boundary values derived
- For the `[0, 120]` range, `deriveBoundaryValues` returns values including
  `0`, `1`, `60` (nominal), `119`, `120`, and `-1`/`121` (just-outside).

### T3. test cases generated from classes
- `generateTestCases(requirement)` returns a non-empty list of test cases.
- Each test case has a unique id and a non-empty `steps` list.

### T4. generated cases cover the boundaries
- The generated test cases collectively reference the boundary values from T2
  (each boundary value appears in at least one generated case's steps/inputs).

### T5. invalid inputs produce invalid-class cases
- At least one generated test case targets an invalid class (input below min or above max).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase5_tests
    test/s2_phase5_tests.cpp)
target_link_libraries(lodestar_s2_phase5_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_testforge)
target_compile_definitions(lodestar_s2_phase5_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
