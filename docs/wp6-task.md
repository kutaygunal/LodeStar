# WP-6 Task — Qt UI shell (senior-engineer-wp6)

You are `senior-engineer-wp6`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/reborn-brief.md first. Implement WP-6 to commercial grade.

## Scope (PLAN.md WP-6)
Qt UI shell: left-nav project tree + right-side detail/properties panel; enable
LODESTAR_BUILD_UI=ON with Qt 6.8.2.

## Contract
The test contract is in `docs/wp6-test.md` (written by scrum-master). Follow it exactly:
- Extend `core/tracelink/UiWiringService.h` (+ .cpp) with `ProjectTreeNode`,
  `DetailPanelModel`, `projectTree()`, and `detail()` (Qt-independent wiring).
- Create `core/test/wp6_ui_tests.cpp` implementing the contract's test cases T1..T7.
- The CMake target `lodestar_wp6_ui_tests` is already registered in `core/CMakeLists.txt`.
  Do NOT weaken assertions; implement the feature to satisfy them.
- ALSO create the Qt views in `ui/`: `ProjectTreeView` (QTreeView), `DetailPanelView`
  (QWidget), and wire them into `MainWindow` (left-nav tree + right-side detail panel,
  alongside existing tabs). Enable `LODESTAR_BUILD_UI=ON`.

## Working rules (docs/working-rules.md)
- Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
- For the UI build, configure with:
  `cmake -S . -B build -DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON`
  then `timeout 600 cmake --build build --config Release`.
- Run tests ONE AT A TIME with timeouts.
- Do NOT commit/push — that is devops's job.
- Do NOT run `find /`.

## Definition of done
1. `./build/core/Release/lodestar_wp6_ui_tests.exe` passes (all assertions green).
2. The UI shell builds with Qt 6.8.2 (LODESTAR_BUILD_UI=ON) — `lodestar_ui` compiles/links.
3. No regressions in existing wp1..wp8, wpA..wpG test suites.
4. Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
5. Report a concise summary of what you implemented and the test results.

When done, run: `herdr agent prompt orchestrator 'DONE senior-engineer-wp6'`
