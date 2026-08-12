# Senior Engineer Task — S1 Phase 5 (Real-time / determinism validation)

You are **senior-engineer-phase5**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S1 Phase 5 per the test contract in **`docs/s1-phase5-test.md`** (read it fully).
Deliverable: recorded benchmark numbers + determinism validation of the core.

## What to do
1. Read `docs/s1-phase5-test.md` and `PLAN.md` (Phase 5 section).
2. Add the test `core/test/s1_phase5_tests.cpp` with a benchmark harness that times a core
   operation (e.g. `TraceGraph` query) using a monotonic clock, runs it M times, records
   min/avg/max microseconds, and appends a dated section to
   `docs/reports/s1-phase5-benchmarks.md` (create the file/dir if needed). Include a
   determinism check (same input → byte-identical output).
3. Register the `lodestar_s1_phase5_tests` target in `core/CMakeLists.txt` (inside
   `if(LODESTAR_BUILD_TESTS)`) exactly as the contract specifies.
4. Build and run ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s1_phase5_tests`
   - `timeout 120 ./build/core/Release/lodestar_s1_phase5_tests.exe`
5. Make T1–T5 pass. Ensure `docs/reports/s1-phase5-benchmarks.md` is created with the
   recorded numbers.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase5'`.
