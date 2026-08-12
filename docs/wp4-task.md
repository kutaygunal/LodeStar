# WP-4 Task — Roles / permissions (RBAC) + concurrency (senior-engineer-wp4)

You are `senior-engineer-wp4`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-4 to commercial grade.

## Scope (PLAN.md WP-4)
User roles + permissions (RBAC) on entities/links; concurrent-edit safety (optimistic
locking/version check); migration 016.

## Contract
The test contract is in `docs/wp4-test.md` (written by scrum-master). Follow it exactly:
- Create migration `core/persistence/migrations/016_*.sql` (users + user_permissions tables).
- Create `core/tracelink/RbacService.h` (+ .cpp) with the exact API in the contract.
- Add `updateEntityIfVersion` optimistic-locking method to `TraceLinkService`.
- Create `core/test/wp4_rbac_tests.cpp` implementing the contract's test cases T1..T7.
- The CMake target `lodestar_wp4_rbac_tests` is already registered in
  `core/CMakeLists.txt`. Do NOT weaken assertions; implement the feature to satisfy them.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp4_rbac_tests.exe` passes (all assertions green).
2. No regressions in existing wp1..wp8, wpA..wpG test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp4'`
