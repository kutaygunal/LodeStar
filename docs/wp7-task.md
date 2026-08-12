# WP-7 Task — Coverage dashboard + charts (senior-engineer-wp7)

You are `senior-engineer-wp7`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-7 to commercial grade.

## Scope (PLAN.md WP-7)
Live coverage dashboard (red/green gaps) + status/priority/coverage charts. Depends on
WP-5 CoverageService (executed results) and WP-6 (tree/detail).

## Contract
The test contract is in `docs/wp7-test.md` (written by scrum-master). Follow it exactly:
- Extend `core/tracelink/UiWiringService.h` (+ .cpp) with `LiveCoverageRow`,
  `CoverageCharts`, `liveCoverage()`, and `coverageCharts()` (Qt-independent wiring).
- Create `core/test/wp7_dashboard_tests.cpp` implementing the contract's test cases T1..T7.
- The CMake target `lodestar_wp7_dashboard_tests` is already registered in
  `core/CMakeLists.txt` (links lodestar_testforge). Do NOT weaken assertions.
- ALSO update `ui/CoverageDashboardView` to render the live dashboard + charts from
  `liveCoverage()`/`coverageCharts()`. Enable LODESTAR_BUILD_UI=ON.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- For the UI build, configure with:
  `cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON`
  then `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp7_dashboard_tests.exe` passes (all assertions green).
2. The UI shell builds with Qt 6.8.2 (LODESTAR_BUILD_UI=ON).
3. No regressions in existing wp1..wp8, wpA..wpG, and Phase-10 suites.
4. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
5. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp7'`
