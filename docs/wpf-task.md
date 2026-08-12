# WP-F Task — Robustness Hardening (senior-engineer-wpf)

You are `senior-engineer-wpf`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-F to commercial grade.

## Scope (PLAN.md WP-F)
- **B2 typed error codes**
- **B3 input validation & size limits**
- **B4 DB backup/restore**
- **B5 migration safety (dry-run/rollback/checksum)**
- **B6 fuzz/edge-case tests**
- **B7 concurrency stress test**
- **B8 structured logging**

## Contract
The test contract is in `core/test/wpF_tests.cpp` (written by scrum-master). Make it pass.
It is already registered in `core/CMakeLists.txt`.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `core/test/wpF_tests.cpp` passes (all assertions green).
2. No regressions in existing wp1..wp8 test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wpf'`
