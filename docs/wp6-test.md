# WP-6 Test Contract — Qt UI shell (left-nav tree + detail panel)

> Written by the scrum-master BEFORE the WP-6 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp6_ui_tests.cpp`
- **CMake target:** `lodestar_wp6_ui_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp6_ui_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Scope & approach (Qt UI WP)

WP-6 is a **Qt Widgets** UI work package: a left-nav **project tree** plus a
right-side **detail/properties panel**, with `LODESTAR_BUILD_UI=ON` enabled against
Qt 6.8.2. Following the WP-G precedent, this contract verifies the **Qt-independent
wiring** that the Qt views consume (pure C++, fully testable without a display), and
documents the **UI build verification** as a separate acceptance step.

Two acceptance layers:
1. **Wiring (this test file):** the tree/detail data the Qt views render is correct.
2. **Build (testing step):** the UI shell compiles and links with
   `-DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON`.

## Contract the WP-6 engineer must provide

### (A) Qt-independent wiring layer — extend `UiWiringService`
Add to `core/tracelink/UiWiringService.h` (namespace `lodestar::tracelink`):

```cpp
// One node of the left-nav project tree (nested hierarchy).
struct ProjectTreeNode {
    std::string id;
    std::string externalId;
    std::string type;      // "requirement" | "design" | "test_case" | ...
    std::string name;
    std::vector<ProjectTreeNode> children;  // ordered by sortOrder then id
};

// The right-side detail/properties panel for one selected entity.
struct DetailPanelModel {
    std::string id;
    std::string externalId;
    std::string type;
    std::string name;
    std::string status;
    std::string owner;
    std::string priority;
    std::string verificationMethod;
    std::string safetyLevel;
    int version = 0;
    std::vector<std::string> incomingLinks;  // "relation: sourceExternalId"
    std::vector<std::string> outgoingLinks;  // "relation: targetExternalId"
};

class UiWiringService {
    // ... existing refreshAll(), impact() ...

    // Builds the full left-nav project tree: every root entity (no parent) with
    // its ordered nested children (recursive). Roots ordered by sortOrder then id.
    common::Result<std::vector<ProjectTreeNode>> projectTree();

    // Builds the right-side detail/properties panel for one entity, including
    // its Active incoming/outgoing links. Fails cleanly if the entity is missing.
    common::Result<DetailPanelModel> detail(EntityType type, const std::string& id);
};
```

### (B) Qt views (not exercised here — Qt absent)
- `ui/ProjectTreeView` (a `QTreeView`) renders the left-nav tree from `projectTree()`.
- `ui/DetailPanelView` (a `QWidget`) renders the right-side panel from `detail()`.
- `MainWindow` assembles the left-nav tree + right-side detail panel (alongside the
  existing tabs), exposes `refreshAll()` and `showDetail(type, id)`. These are NOT
  compiled/instantiated in this test; the wiring they call is what this contract
  verifies.

### (C) UI build acceptance (testing step, not this binary)
The UI shell must build with:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON
cmake --build build --config Release
```
**Expect:** `lodestar_ui` (MatrixView, GraphView, ImpactView, CoverageDashboardView,
ProjectTreeView, DetailPanelView, MainWindow) compiles and links against Qt 6.8.2.

## Test cases & expected behavior

### T1. projectTree() returns the full nested hierarchy
- Build a graph: root requirement R, child requirement C (derives R), design D
  (satisfies C), test TC (verifies C).
- **Expect:** `projectTree()` returns one root (R) whose children contain C; C's
  children contain D and TC. Every entity appears exactly once.

### T2. projectTree() reflects parent/child relationships
- Add two roots A, B; set B's parent to A via `setParent`.
- **Expect:** `projectTree()` returns one root (A) with B as a child; B is no longer
  a root.

### T3. projectTree() orders children by sortOrder
- Give C two children X, Y with sortOrder 0 and 1.
- **Expect:** X appears before Y in C's children.

### T4. detail() returns the selected entity's properties
- For requirement R with owner/priority/verificationMethod/safetyLevel set and
  version bumped.
- **Expect:** `detail(Requirement, rId)` returns those fields and the current version.

### T5. detail() returns incoming/outgoing links
- R has an incoming `satisfies` link from D and an outgoing `derives` link to S.
- **Expect:** `detail()` reports `incomingLinks` containing `"satisfies: D"` and
  `outgoingLinks` containing `"derives: S"`.

### T6. detail() on a missing entity fails cleanly
- `detail(Requirement, "does-not-exist")`.
- **Expect:** returns an error (not an empty model).

### T7. Acceptance: tree + detail reflect a live service change
- Build the graph, snapshot `projectTree()` and `detail()`.
- Add a new requirement N, set its parent to R, then re-query.
- **Expect:** `projectTree()` now shows N under R; `detail()` for N returns its
  properties; the tree is stable across repeated calls (idempotent).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp6_ui_tests
    test/wp6_ui_tests.cpp)
target_link_libraries(lodestar_wp6_ui_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp6_ui_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp6_ui_tests` (not `lodestar_wp6_tests`) to
> avoid clobbering the existing Phase-1 `lodestar_wp6_tests` (API) regression target.
> This test is Qt-independent and lives in `core/test/`; the Qt UI build is verified
> separately with `LODESTAR_BUILD_UI=ON` (see "UI build acceptance" above).
