# Senior Engineer Task — S2 Phase 11 (ScenarioForge baseband + automation API)

You are **senior-engineer-phase11**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 11 per the test contract in **`docs/s2-phase11-test.md`** (read it fully).
Deliverable: ScenarioForge software-defined baseband (I/Q sample generation) + automation
API (Python/REST/SCPI-style remote control).

## Background (read these first)
- `PLAN.md` Phase 11 section.
- `core/scenario/` — ScenarioForge currently produces numbers (data-level GNSS math), not
  signals. No RF/IF baseband, no I/Q samples.
- `core/adapters/` — existing adapters (Skydel, Spirent, Rs) for reference on automation.

## What to do
1. Read `docs/s2-phase11-test.md` and `PLAN.md` (Phase 11).
2. Add an **I/Q baseband generator**: given a scenario (satellites, carrier frequency,
   sample rate), produce I/Q sample data (e.g. a vector of complex samples) for a short
   duration. This is the software-defined baseband.
3. Add an **automation API**: a remote-control interface (Python binding, REST endpoint, or
   SCPI-style command set) to start/stop/configure scenario generation.
4. Add the test file `core/test/s2_phase11_tests.cpp` and register the
   `lodestar_s2_phase11_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase11_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase11_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase11'`.
