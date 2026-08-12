# S2 Phase 1 Test Contract — User model + RBAC + concurrent editing

> Written by the scrum-master BEFORE the Phase 1 engineer implements the feature.
> The engineer must implement the contract below so the tests pass. Do NOT weaken
> the assertions; implement the feature to satisfy them. This is a TEST CONTRACT.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)
```bash
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
timeout 600 cmake --build build --config Release --target lodestar_s2_phase1_tests
timeout 120 ./build/core/Release/lodestar_s2_phase1_tests.exe
```

## Test file
- **File:** `core/test/s2_phase1_tests.cpp`
- **CMake target:** `lodestar_s2_phase1_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s2_phase1_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / S1 phases
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the Phase 1 engineer must provide

### (A) User accounts with login
- `registerUser(username, password, role)` creates a user with a salted password hash
  (never stored in plaintext). Duplicate username fails.
- `login(username, password)` returns a session token on success; wrong password fails.
- `logout(token)` invalidates the session.
- `currentUser(token)` returns the user for a valid session; fails for invalid/expired.

### (B) Roles + permissions
- `changeRole(userId, newRole)` updates the role (admin|editor|reviewer|viewer).
- `hasPermission(userId, permission, entityType)` still works; admin has all.

### (C) Concurrent-editing conflict handling
- Entities carry a `version` (optimistic lock). `updateEntity(type, id, newData, expectedVersion)`
  succeeds when `expectedVersion` matches the current version and bumps the version.
- When `expectedVersion` is stale, it returns a **conflict** error (does NOT overwrite).

## Test cases & expected behavior

### T1. registerUser stores a salted hash, not plaintext
- `registerUser("alice", "s3cret", "editor")` succeeds.
- The stored password column does NOT equal `"s3cret"` (it is a hash).

### T2. login/logout/currentUser round-trip
- `login("alice", "s3cret")` returns a non-empty token.
- `currentUser(token)` returns alice with role `editor`.
- `logout(token)`; then `currentUser(token)` fails (session invalid).

### T3. wrong password rejected
- `login("alice", "wrong")` fails.

### T4. changeRole updates the role
- `changeRole(aliceId, "admin")`; `currentUser(token)` now shows role `admin`.

### T5. hasPermission honors role + grant
- A `viewer` user lacks `edit`; after `grantPermission(viewerId, "edit", "requirement")`,
  `hasPermission(viewerId, "edit", "requirement")` is true; admin has all.

### T6. concurrent edit conflict detected
- `updateEntity("requirement", id, data, version=1)` succeeds (version becomes 2).
- `updateEntity("requirement", id, data, version=1)` again returns a **conflict** error
  (stale version), and the stored data is NOT overwritten by the stale write.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:
```cmake
add_executable(lodestar_s2_phase1_tests
    test/s2_phase1_tests.cpp)
target_link_libraries(lodestar_s2_phase1_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_s2_phase1_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```
