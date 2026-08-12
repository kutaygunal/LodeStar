# S2 Phase 6 Test Contract — Wire functional RF adapters into TestForge execution

> Written by the scrum-master BEFORE the Phase 6 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase6_tests
timeout 120 ./build/core/Release/lodestar_s2_phase6_tests.exe
```

## Test file
- **File:** `core/test/s2_phase6_tests.cpp`
- **CMake target:** `lodestar_s2_phase6_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_testforge`, `lodestar_adapters`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase6_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 6 engineer must provide

### (A) IMeasurementProvider backed by SkydelAdapter
- An `IMeasurementProvider` implementation that uses `SkydelAdapter::invoke()` (simulate
  mode for CI) to produce a measurement.

### (B) TestForge runner drives the adapter
- The TestForge runner can execute a test case that invokes the adapter-backed provider
  and records the measurement result.

## Test cases & expected behavior

### T1. provider produces a measurement via the adapter
- The adapter-backed `IMeasurementProvider` returns a non-empty measurement when invoked
  (in simulate mode).

### T2. runner executes a test case through the provider
- A TestForge test case that uses the provider executes and records a measurement result.

### T3. measurement result is persisted
- After execution, the run/result is persisted (queryable) with the measurement value.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase6_tests
    test/s2_phase6_tests.cpp)
target_link_libraries(lodestar_s2_phase6_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_testforge
    lodestar_adapters)
target_compile_definitions(lodestar_s2_phase6_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
