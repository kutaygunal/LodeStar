# Senior Engineer Task — S2 Phase 15 (Guided compliance templates/checklists)

You are **senior-engineer-phase15**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 15 per the test contract in **`docs/s2-phase15-test.md`** (read it fully).
Deliverable: guided compliance templates/checklists — OOTB ARP4754A/DO-178C templates.
Depends on Phase 3 (AssureCheck workflow) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 15 section.
- `core/assurecheck/AssureCheckService.h/.cpp` — standards registry (DO-178C, ARP4754A, etc.).
- `docs/assurecheck-standards-checklist.md` — the 136 checklist items.
- `core/persistence/migrations/` — add a migration for templates.

## What to do
1. Read `docs/s2-phase15-test.md` and `PLAN.md` (Phase 15).
2. Add **guided compliance templates**: OOTB templates for ARP4754A and DO-178C that guide
   a user through the compliance process (a guided checklist with steps/status).
3. Each template is tied to a standard and provides a guided sequence of checklist items
   with progress tracking.
4. Add a new migration (e.g. `027_templates.sql`) for the template tables.
5. Add the test file `core/test/s2_phase15_tests.cpp` and register the
   `lodestar_s2_phase15_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
6. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase15_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase15_tests.exe`
7. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase15'`.
