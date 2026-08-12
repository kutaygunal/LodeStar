# WP-3 Test Contract — Compliance templates / checklists

> Written by the scrum-master BEFORE the WP-3 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp3_compliance_tests.cpp`
- **CMake target:** `lodestar_wp3_compliance_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp3_compliance_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-3 engineer must provide

### (A) Migration 015
`core/persistence/migrations/015_*.sql` creates `compliance_templates` and
`compliance_checklist_items` tables so guided OOTB templates/checklists for
ARP4754A / ARP4761 / DO-178C / DO-254 can be stored and tracked. Append-only and
idempotent (`IF NOT EXISTS`). Suggested:

```sql
CREATE TABLE IF NOT EXISTS compliance_templates (
    id          TEXT PRIMARY KEY,             -- UUID
    name        TEXT NOT NULL,                -- ARP4754A | ARP4761 | DO-178C | DO-254
    description TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS compliance_checklist_items (
    id          TEXT PRIMARY KEY,             -- UUID
    template_id TEXT NOT NULL,
    seq         INTEGER NOT NULL DEFAULT 0,
    title       TEXT NOT NULL DEFAULT '',
    guidance    TEXT NOT NULL DEFAULT '',
    checked     INTEGER NOT NULL DEFAULT 0,   -- 0/1 progress state
    FOREIGN KEY (template_id) REFERENCES compliance_templates(id)
);
CREATE INDEX IF NOT EXISTS idx_checklist_template ON compliance_checklist_items(template_id, seq);
```

### (B) `ComplianceService` (new, `core/tracelink/ComplianceService.h`)
```cpp
struct ComplianceTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string createdAt;
};

struct ChecklistItem {
    std::string id;
    std::string templateId;
    int seq = 0;
    std::string title;
    std::string guidance;
    bool checked = false;
};

struct ComplianceStatus {
    int total = 0;
    int checked = 0;
    int percent = 0;   // checked>0 ? (checked*100/total) : 0
};

class ComplianceService {
public:
    explicit ComplianceService(persistence::Database& db);

    // Seeds the four OOTB templates (ARP4754A, ARP4761, DO-178C, DO-254) with
    // their checklist items. Idempotent: safe to call any time.
    common::Result<void> seedTemplates();

    // All templates, ordered by name.
    common::Result<std::vector<ComplianceTemplate>> listTemplates();

    // A template with its checklist items (ordered by seq); nullopt if missing.
    common::Result<std::optional<ComplianceTemplate>> getTemplate(
        const std::string& id);

    // Checklist items for a template, ordered by seq.
    common::Result<std::vector<ChecklistItem>> checklistFor(
        const std::string& templateId);

    // Sets the checked state of one checklist item.
    common::Result<void> setChecked(const std::string& itemId, bool checked);

    // Progress for a template (checked/total).
    common::Result<ComplianceStatus> complianceStatus(
        const std::string& templateId);
};
```

## Test cases & expected behavior

### T1. Migration 015 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `compliance_templates` and
  `compliance_checklist_items` tables exist.

### T2. seedTemplates + listTemplates returns the four OOTB templates
- `seedTemplates()`.
- **Expect:** `listTemplates()` returns exactly 4 templates with names
  ARP4754A, ARP4761, DO-178C, DO-254 (ordered by name).

### T3. getTemplate + checklistFor returns non-empty ordered items
- For each template, `checklistFor` returns a non-empty list ordered by `seq`
  (seq strictly increasing).
- **Expect:** every OOTB template has at least one checklist item.

### T4. setChecked + complianceStatus reflects progress
- On a template with N items, check the first M items.
- **Expect:** `complianceStatus` reports `total == N`, `checked == M`,
  `percent == M*100/N`.

### T5. Idempotent seeding
- Call `seedTemplates()` twice.
- **Expect:** `listTemplates()` still returns exactly 4 templates (no duplicates).

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp3_compliance_tests
    test/wp3_compliance_tests.cpp)
target_link_libraries(lodestar_wp3_compliance_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp3_compliance_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp3_compliance_tests` (not `lodestar_wp3_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp3_tests` regression target.
