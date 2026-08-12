# S2 Phase 11 Test Contract — ScenarioForge baseband + automation API

> Written by the scrum-master BEFORE the Phase 11 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase11_tests
timeout 120 ./build/core/Release/lodestar_s2_phase11_tests.exe
```

## Test file
- **File:** `core/test/s2_phase11_tests.cpp`
- **CMake target:** `lodestar_s2_phase11_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_scenario`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase11_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 11 engineer must provide

### (A) I/Q baseband generator
- `generateBaseband(scenario, carrierHz, sampleRate, durationSec)` produces a vector of
  complex I/Q samples. The number of samples ≈ `sampleRate * durationSec`.

### (B) Automation API
- A remote-control interface (Python binding, REST endpoint, or SCPI-style command set)
  to start/stop/configure scenario generation. At minimum a `startScenario`/`stopScenario`
  control.

## Test cases & expected behavior

### T1. baseband produces the expected number of samples
- `generateBaseband(scenario, 1575.42e6, 10e6, 0.001)` returns a vector whose size is
  approximately `10e6 * 0.001` (≈ 10000 samples, within tolerance).

### T2. baseband samples are non-trivial (not all zeros)
- The generated I/Q samples are not all zero (there is actual signal content).

### T3. automation API can start a scenario
- `startScenario(scenarioId)` succeeds and returns a handle/status.

### T4. automation API can stop a scenario
- `stopScenario(handle)` succeeds and marks the scenario stopped.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase11_tests
    test/s2_phase11_tests.cpp)
target_link_libraries(lodestar_s2_phase11_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_scenario)
target_compile_definitions(lodestar_s2_phase11_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
