# WP-E Task — API + Duplicates (senior-engineer-wpe)

You are `senior-engineer-wpe`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-E to commercial grade.

## Scope (PLAN.md WP-E)
- **A8 REST API auth / API keys**
- **A9 duplicate/similarity detection**

## Contract
The test contract is in `core/test/wpE_tests.cpp` (written by scrum-master). Make it pass.
It is already registered in `core/CMakeLists.txt`.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `core/test/wpE_tests.cpp` passes (all assertions green).
2. No regressions in existing wp1..wp8 + wpA/wpC/wpF test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wpe'`
