# Senior Engineer Task — S2 Phase 7 (Structural code coverage)

You are **senior-engineer-phase7**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 7 per the test contract in **`docs/s2-phase7-test.md`** (read it fully).
Deliverable: structural code coverage — statement/decision first, MC/DC next (or integrate
an external engine). Depends on Phase 5 (TestForge design) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 7 section.
- `core/testforge/` — TestForge models + runner.
- `core/persistence/migrations/` — add a migration for coverage tables.

## What to do
1. Read `docs/s2-phase7-test.md` and `PLAN.md` (Phase 7).
2. Add a **structural coverage** module that computes:
   - **Statement coverage**: fraction of statements executed by a test run.
   - **Decision coverage**: fraction of decision outcomes (true/false branches) exercised.
   - **MC/DC** (if feasible): each condition in a decision independently affects the outcome.
3. Persist coverage results (add a migration, e.g. `026_coverage.sql`).
4. Add the test file `core/test/s2_phase7_tests.cpp` and register the
   `lodestar_s2_phase7_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase7_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase7_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase7'`.
