# WP-5 Task — TestForge coverage wiring (senior-engineer-wp5)

You are `senior-engineer-wp5`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-5 to commercial grade.

## Scope (PLAN.md WP-5)
Wire TestForge test runs into live coverage (coverage reflects executed results).

## Contract
The test contract is in `docs/wp5-test.md` (written by scrum-master). Follow it exactly:
- Create migration `core/persistence/migrations/017_*.sql` (test_run_coverage table) if a
  mapping table is required; may be omitted if coverage can be wired from existing
  TestForge run data + traceability links.
- Create `core/tracelink/CoverageService.h` (+ .cpp) with the exact API in the contract.
- Create `core/test/wp5_coverage_tests.cpp` implementing the contract's test cases T1..T6.
- The CMake target `lodestar_wp5_coverage_tests` is already registered in
  `core/CMakeLists.txt` (links lodestar_testforge). Do NOT weaken assertions; implement
  the feature to satisfy them.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp5_coverage_tests.exe` passes (all assertions green).
2. No regressions in existing wp1..wp8, wpA..wpG test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp5'`
