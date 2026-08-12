# WP-3 Task — Compliance templates / checklists (senior-engineer-wp3)

You are `senior-engineer-wp3`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-3 to commercial grade.

## Scope (PLAN.md WP-3)
Guided OOTB templates/checklists for ARP4754A / ARP4761 / DO-178C / DO-254; migration 015.

## Contract
The test contract is in `docs/wp3-test.md` (written by scrum-master). Follow it exactly:
- Create migration `core/persistence/migrations/015_*.sql`
  (compliance_templates + compliance_checklist_items tables).
- Create `core/tracelink/ComplianceService.h` (+ .cpp) with the exact API in the contract.
- Create `core/test/wp3_compliance_tests.cpp` implementing the contract's test cases T1..T5.
- The CMake target `lodestar_wp3_compliance_tests` is already registered in
  `core/CMakeLists.txt`. Do NOT weaken assertions; implement the feature to satisfy them.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp3_compliance_tests.exe` passes (all assertions green).
2. No regressions in existing wp1..wp8, wpA..wpG test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp3'`
