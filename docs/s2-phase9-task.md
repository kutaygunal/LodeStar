# Senior Engineer Task — S2 Phase 9 (Full CI/CD)

You are **senior-engineer-phase9**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 9 per the test contract in **`docs/s2-phase9-test.md`** (read it fully).
Deliverable: full CI/CD — wire the per-phase test targets into the pipeline with a test
gate and matrix builds.

## Background (read these first)
- `PLAN.md` Phase 9 section.
- `ci/` — the existing Jenkinsfile is checkout→build→self-verify only.
- `core/CMakeLists.txt` — per-phase test targets already exist (s1_phase*, wp*).

## What to do
1. Read `docs/s2-phase9-test.md` and `PLAN.md` (Phase 9).
2. Update the CI pipeline (`ci/Jenkinsfile` or add a GitHub Actions workflow) to:
   - Build with `LODESTAR_BUILD_TESTS=ON`.
   - Run ALL test targets (each with a timeout) as a **test gate** — the build FAILS if
     any test target fails.
   - Run a **matrix** of configurations (e.g. Release/Debug, or MSVC x64/x86).
   - Optionally collect coverage.
3. Add a CI smoke script (e.g. `ci/run_all_tests.sh` or `.ps1`) that enumerates and runs
   every `*_tests.exe` in the build tree and fails on any non-zero exit.
4. Add the test file `core/test/s2_phase9_tests.cpp` and register the
   `lodestar_s2_phase9_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase9_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase9_tests.exe`
6. Make all T1–T3 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase9'`.
