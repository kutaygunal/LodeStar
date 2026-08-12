# S2 Phase 3 Test Contract — AssureCheck workflow + audit + evidence package

> Written by the scrum-master BEFORE the Phase 3 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase3_tests
timeout 120 ./build/core/Release/lodestar_s2_phase3_tests.exe
```

## Test file
- **File:** `core/test/s2_phase3_tests.cpp`
- **CMake target:** `lodestar_s2_phase3_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase3_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 3 engineer must provide

### (A) Review/approval/sign-off workflow
- A check result (or checklist item) can be `submitForReview`, `approve`, or `reject`.
- Each transition records a **real actor** (user id/name) and a **real timestamp**
  (not the literal string `"now"`).

### (B) Audit trail
- Every workflow transition is appended to an audit log: actor, action, target,
  timestamp, and from→to state.

### (C) Evidence package
- `buildEvidencePackage(objectiveId)` collects the evidence links for that objective
  into a package (list of {entityType, entityId}) that can be exported.

## Test cases & expected behavior

### T1. submitForReview records a real actor + timestamp
- `submitForReview(resultId, "alice")` succeeds.
- The stored `reviewed_by` == `"alice"` and `reviewed_at` is a real timestamp
  (NOT the literal string `"now"`; it parses as a date/time).

### T2. approve transitions state to approved
- After `approve(resultId, "bob")`, the result state is `approved` with `approved_by == "bob"`.

### T3. reject transitions state to rejected
- After `reject(resultId, "carol")`, the result state is `rejected`.

### T4. audit trail records each transition
- After submit→approve, the audit log has at least 2 entries, each with an actor,
  an action, and a timestamp. The actions include `submit` and `approve`.

### T5. evidence package collects evidence links
- With a check result that has 2 evidence links, `buildEvidencePackage(objectiveId)`
  returns a package containing those 2 links ({entityType, entityId}).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase3_tests
    test/s2_phase3_tests.cpp)
target_link_libraries(lodestar_s2_phase3_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_s2_phase3_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
