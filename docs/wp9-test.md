# WP-9 Test Contract — Baseline visual diff + rollback

> Written by the scrum-master BEFORE the WP-9 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp9_diff_tests.cpp`
- **CMake target:** `lodestar_wp9_diff_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp9_diff_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Scope & approach (Qt UI WP)

WP-9 is a **Qt Widgets** UI work package: a **baseline visual compare view** plus
**per-item rollback**. It builds on the existing `BaselineService` (`diffBaseline`,
`restoreBaseline`, `entityAtBaseline`). Following the WP-6 precedent, this contract
verifies the **Qt-independent wiring** the Qt views consume (pure C++, testable
without a display) and documents the **UI build acceptance** step.

## Contract the WP-9 engineer must provide

### (A) Qt-independent wiring layer — extend `UiWiringService`
Add to `core/tracelink/UiWiringService.h` (namespace `lodestar::tracelink`):

```cpp
// One row of the visual compare view between two baselines.
struct VisualDiffRow {
    std::string entityId;
    std::string entityExternalId;
    std::string kind;   // "added" | "removed" | "modified"
    std::vector<FieldChange> fieldChanges;  // non-empty for "modified"
};

// Result of a per-item rollback.
struct RollbackResult {
    std::string entityId;
    std::string entityExternalId;
    bool restored = false;
};

class UiWiringService {
    // ... existing refreshAll(), impact(), projectTree(), detail(),
    //     liveCoverage(), coverageCharts(), matrixFiltered(), ... 

    // Visual diff of baseline a (older) against b (newer): one row per changed
    // entity/link, with field changes for modified items.
    common::Result<std::vector<VisualDiffRow>> visualDiff(
        const std::string& aId, const std::string& bId);

    // Rolls a single entity back to its state in `baselineId`. Fails cleanly
    // if the entity is missing from the baseline.
    common::Result<RollbackResult> rollbackEntity(
        EntityType type, const std::string& id, const std::string& baselineId);
};
```

### (B) Qt views (not exercised here — Qt absent)
- `ui/BaselineDiffView` renders the visual compare from `visualDiff()` and calls
  `rollbackEntity()` for per-item rollback. Not compiled here; the wiring it calls
  is what this contract verifies.

### (C) UI build acceptance (testing step, not this binary)
The UI shell must build with:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON
cmake --build build --config Release
```
**Expect:** `lodestar_ui` compiles and links against Qt 6.8.2.

## Test cases & expected behavior

### T1. visualDiff() reports added/removed/modified
- Create baseline A. Add a requirement, remove a design, modify a requirement's
  name. Create baseline B.
- **Expect:** `visualDiff(A, B)` has one `added`, one `removed`, and one `modified`
  row.

### T2. visualDiff() includes field changes for modified items
- For the modified requirement, the row's `fieldChanges` is non-empty and contains
  the changed field (e.g. `name`) with old/new values.

### T3. rollbackEntity() restores a single entity
- Modify requirement R after baseline A, then `rollbackEntity(Requirement, rId, A)`.
- **Expect:** `restored=true`; `getEntity` returns R with the baseline-A name/version.

### T4. rollbackEntity() on a missing entity fails cleanly
- `rollbackEntity(Requirement, "does-not-exist", A)`.
- **Expect:** returns an error (not a false success).

### T5. Acceptance: diff then rollback roundtrip
- Baseline A, modify R, baseline B. `visualDiff(A,B)` shows R modified.
- `rollbackEntity(Requirement, rId, A)`.
- **Expect:** R is restored; a fresh `visualDiff(A, B)` still reports the change
  (the baseline snapshots are immutable), but the live entity now matches A.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp9_diff_tests
    test/wp9_diff_tests.cpp)
target_link_libraries(lodestar_wp9_diff_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp9_diff_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp9_diff_tests` (no existing Phase-1 target
> conflicts). This test is Qt-independent and lives in `core/test/`; the Qt UI build
> is verified separately with `LODESTAR_BUILD_UI=ON`.
