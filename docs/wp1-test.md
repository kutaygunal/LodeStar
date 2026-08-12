# WP-1 Test Contract — Suspect-link workflow

> Written by the scrum-master BEFORE the WP-1 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp1_suspect_tests.cpp`
- **CMake target:** `lodestar_wp1_suspect_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp1_suspect_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-1 engineer must provide

### (A) Migration 013
`core/persistence/migrations/013_*.sql` creates a `suspect_flags` table so a
downstream artifact can be flagged `suspect` when an upstream requirement changes.
Append-only and idempotent (`IF NOT EXISTS`). Suggested columns:

```sql
CREATE TABLE IF NOT EXISTS suspect_flags (
    id                 TEXT PRIMARY KEY,             -- UUID
    entity_type        TEXT NOT NULL,                -- requirement|design|test_case|...
    entity_id          TEXT NOT NULL,                -- flagged artifact UUID
    reason             TEXT NOT NULL DEFAULT '',
    source_type        TEXT NOT NULL DEFAULT '',     -- the changed upstream entity type
    source_id          TEXT NOT NULL DEFAULT '',     -- the changed upstream entity UUID
    created_at         TEXT NOT NULL DEFAULT '',
    cleared_at         TEXT NOT NULL DEFAULT '',     -- '' while active
    cleared_by         TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_suspect_active ON suspect_flags(entity_type, entity_id);
```

### (B) `SuspectService` (new, `core/tracelink/SuspectService.h`)
```cpp
// One active (uncleared) suspect flag.
struct SuspectFlag {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string reason;
    std::string sourceType;
    std::string sourceId;
    std::string createdAt;
};

class SuspectService {
public:
    explicit SuspectService(persistence::Database& db);

    // Flags an artifact as suspect. Assigns a UUID if id is empty.
    common::Result<SuspectFlag> flagSuspect(
        const std::string& entityType, const std::string& entityId,
        const std::string& reason,
        const std::string& sourceType, const std::string& sourceId);

    // All ACTIVE (uncleared) suspect flags, newest first.
    common::Result<std::vector<SuspectFlag>> suspectQueue();

    // True if the artifact has at least one active suspect flag.
    common::Result<bool> isSuspect(const std::string& entityType,
                                   const std::string& entityId);

    // Clears a flag (records cleared_at/cleared_by). No-op if already cleared.
    common::Result<void> clearSuspect(const std::string& flagId,
                                      const std::string& clearedBy);

    // Auto-flags every downstream artifact of `entityType`/`entityId` as suspect
    // (designs that satisfy it, tests that verify it, and their transitive
    // downstream closure). Used when a requirement changes. Returns the flags.
    common::Result<std::vector<SuspectFlag>> autoFlagDownstream(
        const std::string& entityType, const std::string& entityId,
        const std::string& reason);
};
```

## Test cases & expected behavior

### T1. Migration 013 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `suspect_flags` table exists (a flag can be inserted
  and read back).

### T2. flagSuspect + isSuspect
- `flagSuspect("design", dId, "req changed", "requirement", rId)`.
- **Expect:** returns a flag with a non-empty id; `isSuspect("design", dId)` is true;
  `isSuspect("design", otherId)` is false.

### T3. suspectQueue returns active flags newest first
- Flag two artifacts (A then B).
- **Expect:** `suspectQueue()` returns both; B (newest) is first; both are active.

### T4. clearSuspect removes from queue
- Clear flag A.
- **Expect:** `suspectQueue()` no longer contains A; `isSuspect` for A is false;
  B remains active. Clearing an already-cleared flag is a no-op (still ok).

### T5. autoFlagDownstream flags downstream artifacts
- Build: requirement R, design D (satisfies R), test TC (verifies R).
- `autoFlagDownstream("requirement", rId, "req changed")`.
- **Expect:** D and TC are both flagged suspect; `isSuspect("design", dId)` and
  `isSuspect("test_case", tcId)` are true; the flags carry `source_type=requirement`
  and `source_id=rId`.

### T6. Suspect status on links
- After a requirement changes, the Active links from downstream artifacts to the
  changed requirement are reported as suspect (the service exposes the affected
  link ids, e.g. via `suspectQueue` reason or a `suspectLinks()` helper).
- **Expect:** the verifies/satisfies links incident to the changed requirement are
  flagged; clearing the flags removes them from the queue.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp1_suspect_tests
    test/wp1_suspect_tests.cpp)
target_link_libraries(lodestar_wp1_suspect_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp1_suspect_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp1_suspect_tests` (not `lodestar_wp1_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp1_tests` regression target.
