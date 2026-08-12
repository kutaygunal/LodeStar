# S2 Phase 13 Test Contract — AI quality scoring on requirements

> Written by the scrum-master BEFORE the Phase 13 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase13_tests
timeout 120 ./build/core/Release/lodestar_s2_phase13_tests.exe
```

## Test file
- **File:** `core/test/s2_phase13_tests.cpp`
- **CMake target:** `lodestar_s2_phase13_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`, `lodestar_riskai`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase13_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 13 engineer must provide

### (A) Quality scorer
- `scoreRequirement(requirement)` returns a score object with per-dimension scores
  (clarity, testability, atomicity, completeness, ambiguity) each 0–100, plus an overall
  score (0–100).

### (B) LLM + deterministic fallback
- Uses the LLM adapter when available; falls back to a deterministic heuristic when the
  LLM is unavailable (so the tests pass without a live LLM).

## Test cases & expected behavior

### T1. scoreRequirement returns all dimensions
- `scoreRequirement(req)` returns a score with all 5 dimensions present, each in [0,100].

### T2. overall score is in range
- The overall score is in [0,100].

### T3. a well-formed requirement scores higher than a vague one
- A clear, testable requirement (e.g. "The system shall display the speed in km/h.") gets
  a higher overall score than a vague one (e.g. "Handle stuff.").

### T4. deterministic fallback works without LLM
- With the LLM adapter unavailable, `scoreRequirement` still returns a valid score object
  (all dimensions in [0,100]) via the deterministic fallback.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase13_tests
    test/s2_phase13_tests.cpp)
target_link_libraries(lodestar_s2_phase13_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink
    lodestar_riskai)
target_compile_definitions(lodestar_s2_phase13_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
