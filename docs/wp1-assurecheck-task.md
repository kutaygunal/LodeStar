# WP-1 Test Contract — AssureCheck standards + checklist data model

> Written by the scrum-master BEFORE the WP-1 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.
>
> **Naming note:** this is the AssureCheck WP-1 (Phase 11). The existing
> `docs/wp1-task.md` / `docs/wp1-test.md` and the `lodestar_wp1_tests` CMake
> target belong to the Phase-10 suspect-link workflow, so this contract uses a
> distinct file name and a distinct test target to avoid clobbering them.

## Test file
- **File:** `core/test/wp1_assurecheck_tests.cpp`
- **CMake target:** `lodestar_wp1_assurecheck_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_assurecheck`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp1_assurecheck_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-1 engineer must provide

### (A) Migration 019
`core/persistence/migrations/019_assurecheck_standards.sql` creates the standards
registry and checklist-item tables so the five assurance standards and all 136
checklist items from `docs/assurecheck-standards-checklist.md` can be stored and
seeded. Append-only and idempotent (`IF NOT EXISTS`). Suggested:

```sql
CREATE TABLE IF NOT EXISTS assurance_standards (
    id          TEXT PRIMARY KEY,             -- UUID
    code        TEXT NOT NULL UNIQUE,         -- DO-178C | DO-254 | ARP4754A | ARP4761 | DO-278A
    name        TEXT NOT NULL,                -- full standard name
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS assurance_checklist_items (
    id          TEXT PRIMARY KEY,             -- UUID
    standard_id TEXT NOT NULL,
    item_code   TEXT NOT NULL,                -- A1-1 | D254-1 | A4754-1 | A4761-1 | D278-1
    seq         INTEGER NOT NULL DEFAULT 0,   -- order within the standard
    objective   TEXT NOT NULL,                -- the objective text
    dal_level   TEXT NOT NULL DEFAULT 'A-D',  -- A | A-B | A-C | A-D (range of applicable DALs)
    evidence    TEXT NOT NULL DEFAULT '',     -- evidence required
    FOREIGN KEY (standard_id) REFERENCES assurance_standards(id)
);
CREATE INDEX IF NOT EXISTS idx_assurance_items_standard
    ON assurance_checklist_items(standard_id, seq);
```

> **Migration number:** the task brief said "Migration 018", but `018_matrix_views.sql`
> (WP-8) already exists. The AssureCheck migration is therefore **019** to keep the
> append-only migration sequence collision-free.

### (B) `AssureCheckService` (new, `core/assurecheck/AssureCheckService.h`)
```cpp
struct AssuranceStandard {
    std::string id;
    std::string code;      // DO-178C | DO-254 | ARP4754A | ARP4761 | DO-278A
    std::string name;      // full standard name
    std::string description;
};

struct AssuranceChecklistItem {
    std::string id;
    std::string standardId;
    std::string itemCode;  // A1-1 | D254-1 | A4754-1 | A4761-1 | D278-1
    int seq = 0;
    std::string objective;
    std::string dalLevel;  // A | A-B | A-C | A-D
    std::string evidence;
};

class AssureCheckService {
public:
    explicit AssureCheckService(persistence::Database& db);

    // Seeds the five standards (DO-178C, DO-254, ARP4754A, ARP4761, DO-278A)
    // with all 136 checklist items from the standards checklist doc. Idempotent.
    common::Result<void> seedStandards();

    // All standards, ordered by code.
    common::Result<std::vector<AssuranceStandard>> listStandards();

    // A standard by code; nullopt if missing.
    common::Result<std::optional<AssuranceStandard>> getStandard(
        const std::string& code);

    // Checklist items for a standard (by code), ordered by seq.
    common::Result<std::vector<AssuranceChecklistItem>> checklistFor(
        const std::string& standardCode);

    // Total number of checklist items across all standards.
    common::Result<int> totalItemCount();

    // Number of checklist items whose DAL range includes the given level
    // (e.g. "A", "B", "C", "D", "E").
    common::Result<int> countForDal(const std::string& dalLevel);
};
```

## Test cases & expected behavior

### T1. Migration 019 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `assurance_standards` and
  `assurance_checklist_items` tables exist.

### T2. seedStandards + listStandards returns the five standards
- `seedStandards()`.
- **Expect:** `listStandards()` returns exactly 5 standards with codes
  ARP4754A, ARP4761, DO-178C, DO-254, DO-278A (ordered by code).

### T3. seedStandards seeds all 136 items
- `seedStandards()`.
- **Expect:** `totalItemCount()` == 136.

### T4. Per-standard item counts match the checklist doc
- For each standard, `checklistFor(code)` returns the documented count:
  - DO-178C == 82
  - DO-254 == 16
  - ARP4754A == 16
  - ARP4761 == 11
  - DO-278A == 11
- **Expect:** all five counts match (sum == 136).

### T5. checklistFor returns non-empty ordered items with full fields
- For each standard, `checklistFor` returns a non-empty list ordered by `seq`
  (seq strictly increasing), and every item has a non-empty `itemCode`,
  `objective`, `dalLevel`, and `evidence`.
- **Expect:** every standard has at least one item; items are ordered; fields
  are populated.

### T6. DAL applicability
- `countForDal("A")` == 136 (every item's DAL range starts at A).
- `countForDal("E")` == 0 (no item applies to DAL E).
- **Expect:** both counts match.

### T7. Known-item spot checks
- `checklistFor("DO-178C")` contains an item with `itemCode == "A1-1"`,
  `dalLevel == "A-D"`, and `evidence == "PSAC (Plan for Software Aspects of Certification)"`.
- `checklistFor("DO-178C")` contains an item with `itemCode == "A6-10"`,
  `dalLevel == "A"`, and `evidence == "Coverage analysis"`.
- **Expect:** both spot checks pass.

### T8. Idempotent seeding
- Call `seedStandards()` twice.
- **Expect:** `listStandards()` still returns exactly 5 standards and
  `totalItemCount()` still == 136 (no duplicates).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- Phase 11 WP-1: AssureCheck standards + checklist data model (migration 019) ---
add_executable(lodestar_wp1_assurecheck_tests
    test/wp1_assurecheck_tests.cpp)
target_link_libraries(lodestar_wp1_assurecheck_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_assurecheck)
target_compile_definitions(lodestar_wp1_assurecheck_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp1_assurecheck_tests` (not
> `lodestar_wp1_tests`) to avoid clobbering the existing Phase-1 tracelink
> `lodestar_wp1_tests` regression target.
