# Senior Engineer Task — S2 Phase 1 (User model + RBAC + concurrent editing)

You are **senior-engineer-phase1**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 1 per the test contract in **`docs/s2-phase1-test.md`** (read it fully).
Deliverable: surface the existing RBAC service as real user accounts with login, roles,
permissions, and concurrent-editing conflict handling.

## Background (read these first)
- `PLAN.md` Phase 1 section.
- `core/tracelink/RbacService.h/.cpp` — RBAC already exists (users/roles/user_permissions,
  migration 016). It is NOT surfaced as accounts with login or conflict handling.
- `core/api/ApiServer.h/.cpp` — REST API with API-key auth. Add user login + role endpoints.
- `core/persistence/migrations/` — add a new migration for password hashes + sessions +
  optimistic-lock/conflict columns.

## What to do
1. Read `docs/s2-phase1-test.md` and `PLAN.md` (Phase 1).
2. Extend the user model: add password hash (e.g. salted SHA-256), login/session support,
   and an `updated_at`/`version` optimistic-lock column for concurrent-editing conflict
   detection. Add a new migration (e.g. `023_user_sessions.sql`).
3. Add a `UserService` (or extend `RbacService`) with: `registerUser`, `login`, `logout`,
   `currentUser`, `listUsers`, `changeRole`, and `updateEntity` that detects concurrent
   edits via the version column (returns a conflict error if the version is stale).
4. Surface these in the REST API (`ApiServer`): `POST /auth/register`, `POST /auth/login`,
   `POST /auth/logout`, `GET /auth/me`, `GET /users`, `PATCH /users/{id}/role`,
   `PUT /entities/{type}/{id}` with optimistic-lock conflict handling.
5. Add the test file `core/test/s2_phase1_tests.cpp` and register the
   `lodestar_s2_phase1_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
6. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase1_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase1_tests.exe`
7. Make all T1–T6 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase1'`.
