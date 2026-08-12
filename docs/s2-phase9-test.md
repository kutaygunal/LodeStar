# S2 Phase 9 Test Contract — Full CI/CD

> Written by the scrum-master BEFORE the Phase 9 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase9_tests
timeout 120 ./build/core/Release/lodestar_s2_phase9_tests.exe
```

## Test file
- **File:** `core/test/s2_phase9_tests.cpp`
- **CMake target:** `lodestar_s2_phase9_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase9_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 9 engineer must provide

### (A) CI test-gate script
- A script `ci/run_all_tests.sh` (or `.ps1`) that enumerates every `*_tests.exe` in the
  build tree and runs each with a timeout, failing (non-zero exit) if any fails.

### (B) CI pipeline with matrix + gate
- The CI config (`ci/Jenkinsfile` or `.github/workflows/*.yml`) builds with
  `LODESTAR_BUILD_TESTS=ON`, runs the test-gate script, and defines a matrix of at least
  2 configurations.

## Test cases & expected behavior

### T1. run_all_tests script exists and is executable
- `ci/run_all_tests.sh` (or `.ps1`) exists and is non-empty.

### T2. CI config references the test gate + matrix
- The CI config file references the test-gate script and defines a matrix with at least
  2 configurations (e.g. Release/Debug).

### T3. the phase test target builds and passes
- `lodestar_s2_phase9_tests` builds and runs with 0 failures.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase9_tests
    test/s2_phase9_tests.cpp)
target_link_libraries(lodestar_s2_phase9_tests PRIVATE
    lodestar_common
    lodestar_persistence)
target_compile_definitions(lodestar_s2_phase9_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
