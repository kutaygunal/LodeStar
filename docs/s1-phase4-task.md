# Senior Engineer Task — S1 Phase 4 (IntegrateHub first slice)

You are **senior-engineer-phase4**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S1 Phase 4 per the test contract in **`docs/s1-phase4-test.md`** (read it fully).
Deliverable: a working cross-disciplinary issue/coordination model in IntegrateHub.

## What to do
1. Read `docs/s1-phase4-test.md` and `PLAN.md` (Phase 4 section).
2. Replace `core/integratehub/stub.cpp` with a real `IntegrateHubService` (new
   `core/integratehub/IntegrateHubService.h`) backed by `persistence::Database`, with the
   `Discipline`, `Issue`, `Coordination` types and the `createIssue` / `listIssues` /
   `setStatus` / `addCoordination` / `coordinationFor` methods exactly as the contract
   specifies.
3. Add a new migration `persistence/migrations/022_integratehub.sql` creating the
   `integratehub_issues` and `integratehub_coordination` tables.
4. Register the `IntegrateHubService` sources in the `lodestar_integratehub` module in
   `core/CMakeLists.txt` (replacing the `stub.cpp` placeholder).
5. Add the test `core/test/s1_phase4_tests.cpp` and register the `lodestar_s1_phase4_tests`
   target in `core/CMakeLists.txt` (inside `if(LODESTAR_BUILD_TESTS)`) exactly as the
   contract specifies.
6. Build and run ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s1_phase4_tests`
   - `timeout 120 ./build/core/Release/lodestar_s1_phase4_tests.exe`
7. Make T1–T5 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase4'`.
