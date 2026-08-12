# WP-10 Test Contract — Document-style authoring

> Written by the scrum-master BEFORE the WP-10 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp10_doc_tests.cpp`
- **CMake target:** `lodestar_wp10_doc_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp10_doc_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Scope & approach (Qt UI WP)

WP-10 is a **Qt Widgets** UI work package: **document-style authoring** — author
requirements in a document context with atomic traceability. Following the WP-6
precedent, this contract verifies the **Qt-independent wiring** the Qt views consume
(pure C++, testable without a display) and documents the **UI build acceptance** step.

## Contract the WP-10 engineer must provide

### (A) Qt-independent wiring layer — extend `UiWiringService`
Add to `core/tracelink/UiWiringService.h` (namespace `lodestar::tracelink`):

```cpp
// One section of a document (a container of ordered requirements).
struct DocumentSection {
    std::string id;
    std::string title;
    std::vector<Entity> requirements;  // ordered by sortOrder then id
};

// A document: a root container with ordered sections.
struct DocumentModel {
    std::string id;
    std::string title;
    std::vector<DocumentSection> sections;
};

class UiWiringService {
    // ... existing refreshAll(), impact(), projectTree(), detail(),
    //     liveCoverage(), coverageCharts(), matrixFiltered(), visualDiff(), ...

    // Builds a document model from the hierarchy rooted at `docId` (a
    // requirement-type root whose children are sections, whose children are
    // requirements). Fails cleanly if the document root is missing.
    common::Result<DocumentModel> document(const std::string& docId);

    // Creates a requirement and attaches it to a section with atomic
    // traceability (the requirement is created AND linked to the section in one
    // operation). Returns the created requirement.
    common::Result<Entity> addRequirementToDocument(
        const std::string& docId, const std::string& sectionId, const Entity& req);

    // Reorders the requirements within a section to the given id order.
    common::Result<void> reorderRequirements(
        const std::string& docId, const std::string& sectionId,
        const std::vector<std::string>& orderedIds);
};
```

### (B) Qt views (not exercised here — Qt absent)
- `ui/DocumentView` renders the document (sections + requirements) from `document()`
  and calls `addRequirementToDocument()` / `reorderRequirements()`. Not compiled
  here; the wiring it calls is what this contract verifies.

### (C) UI build acceptance (testing step, not this binary)
The UI shell must build with:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON
cmake --build build --config Release
```
**Expect:** `lodestar_ui` compiles and links against Qt 6.8.2.

## Test cases & expected behavior

### T1. document() builds the model from the hierarchy
- Build: document root D, section S (child of D), requirements R1, R2 (children of S).
- **Expect:** `document(D)` returns one section S containing R1 and R2 in order.

### T2. document() on a missing root fails cleanly
- `document("does-not-exist")`.
- **Expect:** returns an error (not an empty model).

### T3. addRequirementToDocument() creates + links atomically
- `addRequirementToDocument(D, S, req)`.
- **Expect:** returns the created requirement; `document(D)` now shows it in section
  S; the requirement is traceable to the section (a link exists).

### T4. reorderRequirements() changes the order
- Reorder S's requirements to [R2, R1].
- **Expect:** `document(D)` shows R2 before R1 in section S.

### T5. Acceptance: authoring flow
- Create document D + section S, add two requirements, reorder them, re-query.
- **Expect:** `document(D)` reflects the final order; each added requirement is
  traceable to S; the model is stable across repeated calls (idempotent).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp10_doc_tests
    test/wp10_doc_tests.cpp)
target_link_libraries(lodestar_wp10_doc_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp10_doc_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp10_doc_tests` (no existing Phase-1 target
> conflicts). This test is Qt-independent and lives in `core/test/`; the Qt UI build
> is verified separately with `LODESTAR_BUILD_UI=ON`.
