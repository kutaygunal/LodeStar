# S2 Phase 14 Test Contract — ScenarioForge trajectory + multipath/interference

> Written by the scrum-master BEFORE the Phase 14 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase14_tests
timeout 120 ./build/core/Release/lodestar_s2_phase14_tests.exe
```

## Test file
- **File:** `core/test/s2_phase14_tests.cpp`
- **CMake target:** `lodestar_s2_phase14_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_scenario`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase14_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 14 engineer must provide

### (A) Trajectory engine (waypoints / 6-DOF)
- `buildTrajectory(waypoints)` produces a trajectory; `positionAt(t)` returns the
  interpolated position (and optionally velocity/attitude) at time t.

### (B) RF impairments
- `applyMultipath(samples, delay, gain)` returns samples with a delayed, attenuated copy
  added.
- `applyInterference(samples, amplitude)` returns samples with additive interference.

## Test cases & expected behavior

### T1. trajectory interpolates between waypoints
- With two waypoints at t=0 and t=10, `positionAt(5)` returns a position between them
  (not equal to either endpoint).

### T2. trajectory respects waypoint endpoints
- `positionAt(0)` equals the first waypoint; `positionAt(10)` equals the second.

### T3. multipath adds a delayed copy
- `applyMultipath(samples, delay, gain)` returns samples different from the input (a
  delayed copy was added).

### T4. interference adds signal
- `applyInterference(samples, amplitude)` returns samples different from the input (the
  interference was added).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase14_tests
    test/s2_phase14_tests.cpp)
target_link_libraries(lodestar_s2_phase14_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_scenario)
target_compile_definitions(lodestar_s2_phase14_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
