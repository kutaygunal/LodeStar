# WP-B Task — Change Management (senior-engineer-wpb)

You are `senior-engineer-wpb`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-B to commercial grade.

## Scope (PLAN.md WP-B)
- **A3 baseline restore/rollback**
- **A4 change-request + review workflow** (approve/reject, review queues, link CRs to audit)

## Contract
The test contract is in `core/test/wpB_tests.cpp` (written by scrum-master). Make it pass.
It is already registered in `core/CMakeLists.txt`.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `core/test/wpB_tests.cpp` passes (all assertions green).
2. No regressions in existing wp1..wp8 + wpA/wpC/wpF test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wpb'`
