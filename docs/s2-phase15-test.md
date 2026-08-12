# S2 Phase 15 Test Contract — Guided compliance templates/checklists

> Written by the scrum-master BEFORE the Phase 15 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase15_tests
timeout 120 ./build/core/Release/lodestar_s2_phase15_tests.exe
```

## Test file
- **File:** `core/test/s2_phase15_tests.cpp`
- **CMake target:** `lodestar_s2_phase15_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase15_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.

## Contract the Phase 15 engineer must provide

### (A) OOTB templates
- `listTemplates()` returns at least the ARP4754A and DO-178C templates.

### (B) Guided checklist
- `templateChecklist(templateId)` returns a guided sequence of checklist items for the
  template, each with a status (e.g. pending/in-progress/complete).

### (C) Progress tracking
- `templateProgress(templateId)` returns the fraction of items complete (0–100).

## Test cases & expected behavior

### T1. ARP4754A and DO-178C templates exist
- `listTemplates()` includes both `ARP4754A` and `DO-178C`.

### T2. template checklist is non-empty
- `templateChecklist(do178cId)` returns a non-empty list of checklist items.

### T3. checklist items have status
- Each item in the checklist has a status field (pending/in-progress/complete).

### T4. progress reflects completed items
- After marking some items complete, `templateProgress` returns a value > 0 and ≤ 100.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase15_tests
    test/s2_phase15_tests.cpp)
target_link_libraries(lodestar_s2_phase15_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_s2_phase15_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
