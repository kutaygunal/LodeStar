# Senior Engineer Task — S2 Phase 14 (ScenarioForge trajectory + multipath/interference)

You are **senior-engineer-phase14**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 14 per the test contract in **`docs/s2-phase14-test.md`** (read it fully).
Deliverable: ScenarioForge trajectory (waypoints/6-DOF) + multipath/interference (RF
impairments). Depends on Phase 11 (baseband) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 14 section.
- `core/scenario/` — ScenarioForge (Phase 11 added I/Q baseband + automation API).
- `core/scenario/orbit/`, `core/scenario/pvt/` — existing trajectory math.

## What to do
1. Read `docs/s2-phase14-test.md` and `PLAN.md` (Phase 14).
2. Add a **trajectory engine**: waypoint-based 6-DOF (position, velocity, attitude) motion
   for a receiver/vehicle, interpolating between waypoints.
3. Add **RF impairments**: multipath (delayed copies of the signal) and interference
   (additive noise/jammer) applied to the baseband I/Q samples.
4. Add the test file `core/test/s2_phase14_tests.cpp` and register the
   `lodestar_s2_phase14_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase14_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase14_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase14'`.
