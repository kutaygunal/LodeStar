# Senior Engineer Task — S2 Phase 6 (Wire functional RF adapters into TestForge execution)

You are **senior-engineer-phase6**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 6 per the test contract in **`docs/s2-phase6-test.md`** (read it fully).
Deliverable: connect `SkydelAdapter::invoke()` to `IMeasurementProvider` so TestForge
execution uses the functional RF adapters. Depends on Phase 5 (TestForge design) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 6 section.
- `core/adapters/SkydelAdapter.h/.cpp` — real `invoke()` (HTTP POST, with simulate mode).
- `core/testforge/TestRunner.h/.cpp` — TestForge execution runner.
- `core/testforge/Models.h` — IMeasurementProvider interface.

## What to do
1. Read `docs/s2-phase6-test.md` and `PLAN.md` (Phase 6).
2. Wire the functional RF adapters into TestForge execution: implement
   `IMeasurementProvider` using `SkydelAdapter::invoke()` (with the simulate mode for CI)
   so a TestForge test run can drive the RF adapter and collect measurements.
3. Ensure the runner can execute a test case that invokes the adapter and records the
   measurement result.
4. Add the test file `core/test/s2_phase6_tests.cpp` and register the
   `lodestar_s2_phase6_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase6_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase6_tests.exe`
6. Make all T1–T3 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase6'`.
