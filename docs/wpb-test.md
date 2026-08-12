# WP-B Testing Task (testing-wpb)

You are `testing-wpb`. Project: /c/Users/kutay/Desktop/Projects/Lodestar.
Read PLAN.md and docs/working-rules.md. Verify WP-B (change management) to commercial grade.

## Steps
1. Build with HARD TIMEOUT: `timeout 600 cmake --build build --config Release`.
2. Run the WP-B test suite ONE AT A TIME: `timeout 300 ./build/core/Release/lodestar_wpB_tests.exe` (or the correct binary name — check core/CMakeLists.txt).
3. Run regression suites wp1..wp8 + wpA/wpC/wpF (one at a time).
4. Run smoke: `./build/core/Release/lodestar_smoke.exe`.

## Report
- PASS: report counts and notify orchestrator.
- FAIL: report the exact failing assertion and notify orchestrator with FAIL.

Do NOT commit/push. Do NOT run `find /`.

When done, run: `herdr agent prompt orchestrator 'DONE testing-wpb'` (or 'FAIL testing-wpb <reason>')
