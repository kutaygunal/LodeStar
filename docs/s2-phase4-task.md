# Senior Engineer Task — S2 Phase 4 (AssureCheck semantic evidence evaluation)

You are **senior-engineer-phase4**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 4 per the test contract in **`docs/s2-phase4-test.md`** (read it fully).
Deliverable: replace the heuristic "row exists" PASS with objective-specific semantic
evidence evaluation rules.

## Background (read these first)
- `PLAN.md` Phase 4 section.
- `core/assurecheck/ComplianceEngine.h/.cpp` — currently PASS = "row exists in mapped
  table". Replace with objective-specific evaluation rules.
- `core/assurecheck/AssureCheckService.h/.cpp` — checklist items with objectives.
- `core/assurecheck/EvidenceService.h/.cpp` — evidence handling.

## What to do
1. Read `docs/s2-phase4-test.md` and `PLAN.md` (Phase 4).
2. Add **objective-specific evaluation rules**: instead of "row exists", evaluate each
   checklist item based on its objective type. For example:
   - A "traceability" objective requires a trace link (requirement→test) to exist.
   - A "verification" objective requires a passed test run.
   - A "coverage" objective requires coverage evidence.
   - A "review" objective requires an approved review (from Phase 3 workflow).
3. The evaluation must be **semantic** (based on the objective's meaning), not just
   "any row exists".
4. Add the test file `core/test/s2_phase4_tests.cpp` and register the
   `lodestar_s2_phase4_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase4_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase4_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase4'`.
