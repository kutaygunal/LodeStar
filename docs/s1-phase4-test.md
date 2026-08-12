# S1 Phase 4 Test Contract — IntegrateHub first slice (issue/coordination model)

> Written by the scrum-master BEFORE the Phase 4 engineer implements the feature.
> The engineer must implement the contract below so the IntegrateHub module has a
> working cross-disciplinary issue/coordination model. Do NOT weaken the assertions to
> make them pass; implement the feature to satisfy them. This is a TEST CONTRACT, not a
> testing task.
>
> **Scope:** Sprint 1 Phase 4 (PLAN.md). Deliverable = a working issue/coordination
> model. `core/integratehub/stub.cpp` is currently a 5-line placeholder; this phase
> replaces it with a real model. Phase 4 is independent and runs in parallel with
> Phase 1 and Phase 2.

## Build / run commands (HARD TIMEOUT, ONE AT A TIME)

```bash
# 1. Configure with tests enabled.
cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON

# 2. Build the IntegrateHub tests (HARD TIMEOUT).
timeout 600 cmake --build build --config Release --target lodestar_s1_phase4_tests

# 3. Run the IntegrateHub tests (HARD TIMEOUT).
timeout 120 ./build/core/Release/lodestar_s1_phase4_tests.exe
```

## Test file
- **File:** `core/test/s1_phase4_tests.cpp`
- **CMake target:** `lodestar_s1_phase4_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_integratehub`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_s1_phase4_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the Phase 4 engineer must provide

### (A) `IntegrateHubService` (new, `core/integratehub/IntegrateHubService.h`)
A service backed by `persistence::Database` that models cross-disciplinary issues and
coordination. It must provide:

```cpp
namespace lodestar::integratehub {

enum class Discipline { Systems, Software, Hardware, Test, Safety };

struct Issue {
    std::string id;            // stable id
    std::string title;
    std::string description;
    Discipline owner;          // owning discipline
    std::string status;        // "open" | "in_progress" | "resolved"
    std::string createdAt;
};

struct Coordination {
    std::string id;
    std::string issueId;       // the issue being coordinated
    std::string note;
    std::string createdAt;
};

class IntegrateHubService {
public:
    explicit IntegrateHubService(persistence::Database& db);

    // Create an issue; returns its id.
    common::Result<std::string> createIssue(const Issue& issue);

    // List issues, optionally filtered by discipline.
    common::Result<std::vector<Issue>> listIssues(Discipline d);

    // Update an issue's status.
    common::Result<void> setStatus(const std::string& issueId,
                                   const std::string& status);

    // Add a coordination note to an issue.
    common::Result<std::string> addCoordination(const std::string& issueId,
                                                const std::string& note);

    // List coordination notes for an issue, oldest first.
    common::Result<std::vector<Coordination>> coordinationFor(const std::string& issueId);
};

}
```

- The model must be **persisted** (SQLite) so issues and coordination survive a reopen.
- A new migration (e.g. `022_integratehub.sql`) creates the `integratehub_issues` and
  `integratehub_coordination` tables.

## Test cases & expected behavior

### T1. createIssue + listIssues round-trip
- Fresh DB, run migrations.
- `createIssue({title:"GPS outage", owner:Software, status:"open"})`.
- `listIssues(Software)`.
- **Expect:** returns 1 issue with `title == "GPS outage"`, `status == "open"`.

### T2. listIssues filters by discipline
- Create one issue owned by `Software` and one owned by `Test`.
- `listIssues(Software)`.
- **Expect:** returns only the Software issue (size 1).

### T3. setStatus updates an issue
- Create an issue; `setStatus(id, "resolved")`.
- `listIssues(Software)`.
- **Expect:** the issue's `status == "resolved"`.

### T4. addCoordination + coordinationFor
- Create an issue; `addCoordination(id, "Need RF data")`; `addCoordination(id, "Data ready")`.
- `coordinationFor(id)`.
- **Expect:** 2 notes, oldest first, with the given texts.

### T5. Persistence survives reopen
- Create an issue + one coordination note; close the DB.
- Reopen the same DB file; `listIssues(Software)`.
- **Expect:** the issue is still present with its coordination note intact.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
# --- S1 Phase 4: IntegrateHub first slice (issue/coordination model) ------
add_executable(lodestar_s1_phase4_tests
    test/s1_phase4_tests.cpp)
target_link_libraries(lodestar_s1_phase4_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_integratehub)
target_compile_definitions(lodestar_s1_phase4_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the engineer must add the new migration file to `persistence/migrations/` and
> register the `IntegrateHubService` sources in the `lodestar_integratehub` module in
> `core/CMakeLists.txt` (replacing the `stub.cpp` placeholder).
