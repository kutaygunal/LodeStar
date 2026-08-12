# WP-1 Task — Suspect-link workflow (senior-engineer-wp1)

You are `senior-engineer-wp1`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-1 to commercial grade.

## Scope (PLAN.md WP-1)
Auto-flag downstream artifacts as `suspect` when a requirement changes; review/clear
queue; suspect status on links/entities; migration 013.

## Contract
The test contract is in `docs/wp1-test.md` (written by scrum-master). Follow it exactly:
- Create migration `core/persistence/migrations/013_*.sql` (suspect_flags table).
- Create `core/tracelink/SuspectService.h` (+ .cpp) with the exact API in the contract.
- Create `core/test/wp1_suspect_tests.cpp` implementing the contract's test cases T1..T6.
- The CMake target `lodestar_wp1_suspect_tests` is already registered in
  `core/CMakeLists.txt`. Do NOT weaken assertions; implement the feature to satisfy them.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp1_suspect_tests.exe` passes (all assertions green).
2. No regressions in existing wp1..wp8, wpA..wpG test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp1'`
