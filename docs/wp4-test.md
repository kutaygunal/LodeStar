# WP-4 Test Contract — Roles / permissions (RBAC) + concurrency

> Written by the scrum-master BEFORE the WP-4 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp4_rbac_tests.cpp`
- **CMake target:** `lodestar_wp4_rbac_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp4_rbac_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-4 engineer must provide

### (A) Migration 016
`core/persistence/migrations/016_*.sql` creates `users`, `roles`, and
`user_permissions` tables so user roles + permissions (RBAC) can be enforced on
entities/links. Append-only and idempotent (`IF NOT EXISTS`). Suggested:

```sql
CREATE TABLE IF NOT EXISTS users (
    id       TEXT PRIMARY KEY,             -- UUID
    username TEXT NOT NULL UNIQUE,
    role     TEXT NOT NULL DEFAULT 'viewer'  -- admin|editor|reviewer|viewer
);

CREATE TABLE IF NOT EXISTS user_permissions (
    id         TEXT PRIMARY KEY,          -- UUID
    user_id    TEXT NOT NULL,
    permission TEXT NOT NULL,             -- e.g. "edit", "approve", "delete"
    entity_type TEXT NOT NULL DEFAULT '', -- '' = all types
    FOREIGN KEY (user_id) REFERENCES users(id)
);
CREATE INDEX IF NOT EXISTS idx_user_perm ON user_permissions(user_id, permission);
```

### (B) `RbacService` (new, `core/tracelink/RbacService.h`)
```cpp
struct User {
    std::string id;
    std::string username;
    std::string role;   // admin|editor|reviewer|viewer
};

class RbacService {
public:
    explicit RbacService(persistence::Database& db);

    // Creates a user (assigns UUID if id empty). Fails on duplicate username.
    common::Result<User> createUser(const std::string& username,
                                    const std::string& role);

    // Grants a permission to a user (optionally scoped to one entity type).
    common::Result<void> grantPermission(const std::string& userId,
                                         const std::string& permission,
                                         const std::string& entityType = "");

    // True if the user has the permission (admin always has all permissions).
    common::Result<bool> hasPermission(const std::string& userId,
                                       const std::string& permission,
                                       const std::string& entityType = "");

    // Enforces a permission; fails with an error if the user lacks it.
    common::Result<void> requirePermission(const std::string& userId,
                                           const std::string& permission,
                                           const std::string& entityType = "");
};
```

### (C) Optimistic locking on `TraceLinkService`
Add a version-checked update so concurrent edits are detected:

```cpp
// Updates the entity ONLY if its current version equals expectedVersion.
// Fails (concurrent-edit conflict) if the stored version differs.
common::Result<Entity> updateEntityIfVersion(const Entity& e, int expectedVersion);
```

## Test cases & expected behavior

### T1. Migration 016 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `users`, `user_permissions` tables exist.

### T2. createUser + duplicate rejection
- Create user "alice" (role editor).
- **Expect:** returns a user with non-empty id; creating "alice" again fails.

### T3. grantPermission + hasPermission
- Grant "edit" to alice.
- **Expect:** `hasPermission(alice, "edit")` is true; `hasPermission(alice, "delete")`
  is false; `hasPermission(bob, "edit")` is false.

### T4. Admin has all permissions
- Create user "root" with role admin.
- **Expect:** `hasPermission(root, "edit")` and `hasPermission(root, "delete")` are
  both true without explicit grants.

### T5. requirePermission enforces
- `requirePermission(alice, "edit")` succeeds; `requirePermission(alice, "delete")`
  fails (returns an error).

### T6. Optimistic locking detects concurrent edit
- Add entity R (version 1). Update it once (version 2).
- `updateEntityIfVersion(R, 1)` (stale expected version).
- **Expect:** fails (conflict). `updateEntityIfVersion(R, 2)` succeeds and bumps
  version to 3.

### T7. Version increments on each update
- Add entity, then update twice.
- **Expect:** version is 1 after add, 2 after first update, 3 after second.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp4_rbac_tests
    test/wp4_rbac_tests.cpp)
target_link_libraries(lodestar_wp4_rbac_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp4_rbac_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp4_rbac_tests` (not `lodestar_wp4_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp4_tests` regression target.
