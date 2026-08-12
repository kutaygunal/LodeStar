# Senior Engineer Task — S2 Phase 3 (AssureCheck workflow + audit + evidence package)

You are **senior-engineer-phase3**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 3 per the test contract in **`docs/s2-phase3-test.md`** (read it fully).
Deliverable: AssureCheck review/approval/sign-off workflow with real timestamps/actors
(fix the `"now"` placeholder), plus an objective→evidence package.

## Background (read these first)
- `PLAN.md` Phase 3 section.
- `core/assurecheck/AssureCheckService.h/.cpp` — standards registry + checklist.
- `core/assurecheck/ComplianceEngine.h/.cpp` — evaluates checklist items; the `checked_at`
  value is currently the literal string `"now"` (a placeholder). Fix this to a real timestamp.
- `core/assurecheck/EvidenceService.h/.cpp` — evidence handling.
- `core/persistence/migrations/` — add a new migration for workflow/audit tables.
- TraceLink already has an evidence package concept (reuse it).

## What to do
1. Read `docs/s2-phase3-test.md` and `PLAN.md` (Phase 3).
2. Add a review/approval/sign-off workflow: a checklist item (or check result) can be
   submitted for review, approved, or rejected by a named actor with a real timestamp.
   Persist the actor + timestamp (no more `"now"` placeholder).
3. Add an audit trail: every workflow transition is recorded (who, what, when, from→to).
4. Build an objective→evidence package: for a given objective/checklist item, collect the
   evidence links (from ComplianceEngine's EvidenceLink) into a package that can be exported.
5. Add a new migration (e.g. `024_assurecheck_workflow.sql`) for the workflow + audit tables.
6. Add the test file `core/test/s2_phase3_tests.cpp` and register the
   `lodestar_s2_phase3_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
7. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase3_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase3_tests.exe`
8. Make all T1–T5 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase3'`.
