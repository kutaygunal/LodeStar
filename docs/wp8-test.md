# WP-8 Test Contract — Interactive traceability matrix

> Written by the scrum-master BEFORE the WP-8 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp8_matrix_tests.cpp`
- **CMake target:** `lodestar_wp8_matrix_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp8_matrix_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Scope & approach (Qt UI WP)

WP-8 is a **Qt Widgets** UI work package: an **interactive traceability matrix** with
search, filter, saved views, relationship toggling, and export. Following the WP-6
precedent, this contract verifies the **Qt-independent wiring** the Qt views consume
(pure C++, testable without a display) and documents the **UI build acceptance** step.

## Contract the WP-8 engineer must provide

### (A) Qt-independent wiring layer — extend `UiWiringService`
Add to `core/tracelink/UiWiringService.h` (namespace `lodestar::tracelink`):

```cpp
// Filtering / view configuration for the interactive matrix.
struct MatrixViewConfig {
    std::string search;                       // substring on name/externalId
    std::string statusFilter;                 // "" = all, else a status
    std::vector<std::string> hiddenRelations; // relations to hide (toggle off)
};

// A saved matrix view (persisted).
struct SavedMatrixView {
    std::string id;
    std::string name;
    MatrixViewConfig config;
};

class UiWiringService {
    // ... existing refreshAll(), impact(), projectTree(), detail(),
    //     liveCoverage(), coverageCharts() ...

    // Builds the matrix honoring the config: rows filtered by search/status,
    // and any cell whose relation is in hiddenRelations is shown as "".
    common::Result<MatrixViewModel> matrixFiltered(const MatrixViewConfig& cfg);

    // Persists a named matrix view.
    common::Result<void> saveMatrixView(const std::string& name,
                                        const MatrixViewConfig& cfg);

    // All saved matrix views, ordered by name.
    common::Result<std::vector<SavedMatrixView>> listMatrixViews();

    // Applies a saved view and returns the filtered matrix.
    common::Result<MatrixViewModel> applyMatrixView(const std::string& viewId);
};
```

### (B) Qt views (not exercised here — Qt absent)
- `ui/MatrixView` renders the interactive matrix, calls `matrixFiltered()` on
  search/filter/toggle, and `saveMatrixView()`/`applyMatrixView()` for saved views.
  Not compiled here; the wiring it calls is what this contract verifies.

### (C) UI build acceptance (testing step, not this binary)
The UI shell must build with:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON
cmake --build build --config Release
```
**Expect:** `lodestar_ui` compiles and links against Qt 6.8.2.

## Test cases & expected behavior

### T1. matrixFiltered() filters rows by search text
- Build the WP-7 fixture graph (REQ-SYS, REQ-DER, REQ-UNV + designs/tests).
- `matrixFiltered({search="REQ-DER"})`.
- **Expect:** only the REQ-DER row remains (rowCount == 1).

### T2. matrixFiltered() filters by status
- `matrixFiltered({statusFilter="Approved"})`.
- **Expect:** only Approved requirements remain (REQ-SYS, REQ-UNV; REQ-DER Draft
  excluded).

### T3. matrixFiltered() toggles relations off
- `matrixFiltered({hiddenRelations={"verifies"}})`.
- **Expect:** the REQ-DER x TC-1 cell is now `""` (verifies hidden); the
  REQ-DER x DES-1 cell still shows `"satisfies"`.

### T4. saveMatrixView + listMatrixViews roundtrip
- Save two views "V1" and "V2".
- **Expect:** `listMatrixViews()` returns both, ordered by name, with configs intact.

### T5. applyMatrixView() restores a saved view
- Save a view with `search="REQ-DER"`, then `applyMatrixView(viewId)`.
- **Expect:** returns a matrix with only the REQ-DER row.

### T6. Export still works on a filtered matrix
- `matrixFiltered({search="REQ-DER"})` then `toCsv()`/`toHtml()`.
- **Expect:** both exports succeed and contain the REQ-DER row.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp8_matrix_tests
    test/wp8_matrix_tests.cpp)
target_link_libraries(lodestar_wp8_matrix_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp8_matrix_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp8_matrix_tests` (not `lodestar_wp8_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp8_tests` (hardening)
> regression target. This test is Qt-independent and lives in `core/test/`; the Qt
> UI build is verified separately with `LODESTAR_BUILD_UI=ON`.
