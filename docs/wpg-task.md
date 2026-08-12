# WP-G Task — Real Qt UI (senior-engineer-wpg)

You are `senior-engineer-wpg`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-G to commercial grade.

## Scope (PLAN.md WP-G)
- **A7 install Qt, enable LODESTAR_BUILD_UI=ON, wire the 4 views** (matrix/graph/impact/coverage) to the service.

## Context
- Qt is NOT currently installed. The UI views exist in `ui/` (MatrixView, GraphView, ImpactView, CoverageDashboardView, MainWindow) but are NOT built (LODESTAR_BUILD_UI=OFF).
- The test contract is in `core/test/wpG_tests.cpp` (written by scrum-master). It covers the view-model/service wiring logic that can be tested WITHOUT a display. Make it pass.
- It is already registered in `core/CMakeLists.txt`.

## Approach
1. First make `core/test/wpG_tests.cpp` pass (the wiring logic — this is the primary contract).
2. Then attempt to install Qt and enable LODESTAR_BUILD_UI=ON. If Qt cannot be installed in this environment (no network/package manager), document the exact steps needed and ensure the UI code compiles cleanly against the service surface (syntax-check the ui/ sources). Do NOT let a Qt install failure block the WP-G test contract.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `core/test/wpG_tests.cpp` passes (all assertions green).
2. No regressions in existing wp1..wp8 + wpA/wpC/wpF/wpB/wpD/wpE test suites.
3. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
4. Report a concise summary: what you implemented, whether Qt was installed, and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wpg'`
