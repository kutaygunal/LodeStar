# WP-D Task — Coverage + Evidence (senior-engineer-wpd)

You are `senior-engineer-wpd`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-D to commercial grade.

## Scope (PLAN.md WP-D)
- **A5 coverage by verification method**
- **A6 DO-178C evidence package export** (matrix + coverage + validation + audit bundle)

## Contract
The test contract is in `core/test/wpD_tests.cpp` (written by scrum-master). Make it pass.
It is already registered in `core/CMakeLists.txt`.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `core/test/wpD_tests.cpp` passes (all assertions green).
2. No regressions in existing wp1..wp8 + wpA/wpC/wpF test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wpd'`
