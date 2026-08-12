# Senior Engineer Task — S2 Phase 5 (TestForge test-case design intelligence)

You are **senior-engineer-phase5**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 5 per the test contract in **`docs/s2-phase5-test.md`** (read it fully).
Deliverable: TestForge test-case design intelligence — equivalence + boundary derivation
from a requirement/objective (replacing the thin "checks → steps" copier).

## Background (read these first)
- `PLAN.md` Phase 5 section.
- `core/testforge/PlanGenerator.h/.cpp` — currently a thin "checks → steps" copier.
- `core/testforge/Models.h` — test plan/case models.
- `core/testforge/TestForgeDao.h/.cpp` — persistence.

## What to do
1. Read `docs/s2-phase5-test.md` and `PLAN.md` (Phase 5).
2. Add test-case design intelligence to `PlanGenerator`: given a requirement/objective
   (with input ranges/constraints), derive:
   - **Equivalence classes** (valid/invalid partitions of the input domain).
   - **Boundary values** (min, min+1, nominal, max-1, max, and just-outside for each
     equivalence class boundary).
   - Optionally **decision-table / MC/DC-style** combinations for boolean conditions.
3. Produce concrete test cases (steps + expected results) from the derived classes/boundaries.
4. Add the test file `core/test/s2_phase5_tests.cpp` and register the
   `lodestar_s2_phase5_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase5_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase5_tests.exe`
6. Make all T1–T5 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase5'`.
