# S1 Phase 1 Test Contract — Desktop Qt App (runnable + shows TraceLink data)

> Written by the scrum-master BEFORE the Phase 1 engineer implements the feature.
> The engineer must implement the contract below so the app builds, launches, and
> shows TraceLink data. Do NOT weaken the assertions to make them pass; implement
> the feature to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Scope:** Sprint 1 Phase 1 (PLAN.md). Deliverable = a runnable desktop app that
> opens and shows TraceLink data. The Qt UI shell already exists in `ui/`
> (MainWindow + WP-7 views) but is gated behind `LODESTAR_BUILD_UI=OFF`. This phase
> enables the flag, wires MainWindow to the service API, and proves the app runs.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)

```bash
# 1. Configure with the UI enabled (Qt6 auto-detected, falls back to Qt5).
cmake -S . -B build -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON

# 2. Build the UI + the Qt-independent wiring test (HARD TIMEOUT).
timeout 600 cmake --build build --config Release --target lodestar_ui lodestar_s1_phase1_tests

# 3. Run the Qt-independent wiring test (HARD TIMEOUT).
timeout 120 ./build/core/Release/lodestar_s1_phase1_tests.exe

# 4. Launch the app headless (offscreen) to prove it constructs + refreshes.
timeout 60 ./build/ui/Release/lodestar_app.exe --platform offscreen
```

## Test file
- **File:** `core/test/s1_phase1_tests.cpp`
- **CMake target:** `lodestar_s1_phase1_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s1_phase1_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures). This test is **Qt-independent**
  (no Qt headers) and verifies the exact data the Qt views consume, so it runs in the
  core test harness without needing a Qt runtime.

## Contract the Phase 1 engineer must provide

### (A) Enable the UI build
- Top-level `CMakeLists.txt` keeps `option(LODESTAR_BUILD_UI ... OFF)` as the default,
  but the Phase 1 engineer must ensure `ui/CMakeLists.txt` builds a **runnable app**
  (an `add_executable` target named `lodestar_app`) in addition to the existing
  `lodestar_ui` static library. The app must:
  - Construct `lodestar::ui::MainWindow` against a real `persistence::Database`
    (open a DB, run migrations, seed a small TraceLink graph).
  - Call `refreshAll()` and `show()`.
  - Exit cleanly on window close.

### (B) MainWindow wiring (already present; must keep working)
- `MainWindow(db)` builds the left-nav `ProjectTreeView`, the tabbed views
  (Trace Matrix, Graph, Impact, Coverage Dashboard, Baseline Diff, Document), and the
  detail panel.
- `refreshAll()` populates the matrix, graph, coverage dashboard, and project tree from
  `UiWiringService::refreshAll()`.
- `showDetail(type, id)` populates the detail panel from `UiWiringService::detail()`.

### (C) Qt-independent data contract (what the views consume)
The test verifies the service layer that feeds the views. The engineer must ensure the
following produce the stated results on a fresh DB with one requirement, one test case,
and one `verifies` link:

1. `UiWiringService::refreshAll()` returns a snapshot whose `matrix` has exactly **1 row**
   (one requirement) and whose `graph` contains the requirement + test case nodes.
2. `UiWiringService::projectTree()` returns a tree containing the requirement node.
3. `UiWiringService::detail(requirement, id)` returns the requirement's detail.

## Test cases & expected behavior

### T1. UI target is configured and builds
- `cmake -S . -B build -DLODESTAR_BUILD_UI=ON` succeeds.
- `cmake --build build --config Release --target lodestar_ui lodestar_app` succeeds.
- **Expect:** both `lodestar_ui` and `lodestar_app` build without error.

### T2. App binary exists and launches headless
- `./build/ui/Release/lodestar_app.exe --platform offscreen` runs and exits 0.
- **Expect:** the app constructs `MainWindow`, calls `refreshAll()`, and exits cleanly
  (no crash, no uncaught exception). A `--platform offscreen` run must not require a
  display.

### T3. refreshAll() produces a 1-row matrix + graph nodes
- Fresh DB, run migrations, insert one requirement + one test case + one `verifies` link.
- `UiWiringService::refreshAll()`.
- **Expect:** `matrix` has size 1; `graph` contains the requirement node and the test
  case node.

### T4. projectTree() contains the requirement
- After T3's data, `UiWiringService::projectTree()`.
- **Expect:** the tree contains a node for the requirement (by external id `REQ-001`).

### T5. detail() returns the requirement
- After T3's data, `UiWiringService::detail(requirement, reqId)`.
- **Expect:** returns the requirement with `externalId == "REQ-001"`.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- S1 Phase 1: desktop app wiring (Qt-independent data contract) --------
add_executable(lodestar_s1_phase1_tests
    test/s1_phase1_tests.cpp)
target_link_libraries(lodestar_s1_phase1_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_s1_phase1_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

And in `ui/CMakeLists.txt`, add the runnable app target (inside the `if(QT_PKG)` branch):

```cmake
add_executable(lodestar_app app/main.cpp)
target_link_libraries(lodestar_app PRIVATE lodestar_ui ${QT_PKG}::Widgets)
```

> Note: the app entry point `ui/app/main.cpp` is new and must construct the DB, run
> migrations, seed a small graph, build `MainWindow`, and `show()` it. The Qt-independent
> wiring test (`lodestar_s1_phase1_tests`) does NOT require Qt and runs in the core
> harness; the app launch check (T2) is the only step that needs the Qt runtime.
