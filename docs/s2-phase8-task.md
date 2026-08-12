# Senior Engineer Task — S2 Phase 8 (Certification-ready reporting + traceability)

You are **senior-engineer-phase8**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 8 per the test contract in **`docs/s2-phase8-test.md`** (read it fully).
Deliverable: certification-ready reporting + traceability — PDF/Word/ReQIF export +
result→requirement traceability. Depends on Phase 5 (TestForge design) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 8 section.
- `core/assurecheck/ReportService.h/.cpp` — existing HTML/CSV/JSON export.
- `core/testforge/ReportGenerator.h/.cpp` — TestForge report generation.
- `core/tracelink/` — requirements + trace links for traceability.

## What to do
1. Read `docs/s2-phase8-test.md` and `PLAN.md` (Phase 8).
2. Add **certification-ready export**:
   - **PDF** export of a compliance/test report.
   - **Word** (docx) export.
   - **ReQIF** export of requirements + traceability.
3. Add **result→requirement traceability**: for each test result, show which requirement(s)
   it verifies (via trace links).
4. Add the test file `core/test/s2_phase8_tests.cpp` and register the
   `lodestar_s2_phase8_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase8_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase8_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase8'`.
