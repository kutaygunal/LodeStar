# S2 Phase 16 Test Contract — Variants / branching

> Written by the scrum-master BEFORE the Phase 16 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase16_tests
timeout 120 ./build/core/Release/lodestar_s2_phase16_tests.exe
```

## Test file
- **File:** `core/test/s2_phase16_tests.cpp`
- **CMake target:** `lodestar_s2_phase16_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase16_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 16 engineer must provide

### (A) Variant model
- `createVariant(name)` creates a product variant.
- `addToVariant(variantId, requirementId)` / `removeFromVariant(variantId, requirementId)`
  manage which requirements belong to a variant.

### (B) Branching
- `createBranch(baseVariantId, name)` creates a branch of a variant.
- `mergeBranch(branchId, targetVariantId)` merges branch changes back, detecting conflicts
  when the same requirement was changed differently in both.

## Test cases & expected behavior

### T1. createVariant + addToVariant
- `createVariant("Pro")` succeeds; `addToVariant(proId, reqId)` succeeds; the variant
  contains the requirement.

### T2. removeFromVariant
- `removeFromVariant(proId, reqId)` removes the requirement from the variant.

### T3. createBranch
- `createBranch(baseId, "feature-x")` succeeds and returns a branch id.

### T4. mergeBranch detects conflict
- When a requirement is changed differently on the branch and the target, `mergeBranch`
  returns a **conflict** result (does not silently overwrite).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase16_tests
    test/s2_phase16_tests.cpp)
target_link_libraries(lodestar_s2_phase16_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_s2_phase16_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
