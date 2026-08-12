# Testing Agent Task — S2 Phase 2 (Web layer)

You are **testing-phase2**. The orchestrator is agent 'orchestrator'. Project:
/c/Users/kutay/Desktop/Projects/Lodestar.

## Your job
Run the S2 Phase 2 test suite and report PASS or FAIL. Do NOT modify source code.

## What to do
1. Confirm you are in the repo: run 'pwd'.
2. Build and run the Phase 2 test target ONE AT A TIME with HARD TIMEOUTS:
   - cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON
   - timeout 600 cmake --build build --config Release --target lodestar_s2_phase2_tests
   - timeout 120 ./build/core/Release/lodestar_s2_phase2_tests.exe
3. Also run the smoke test to confirm nothing regressed:
   - timeout 60 ./build/core/Release/lodestar_smoke.exe
4. Report the result.

## Report format
Reply to the orchestrator with exactly one of:
- 'TEST PASS phase2' if the test binary exits 0 with 0 failures.
- 'TEST FAIL phase2 <reason>' if any test fails or the build fails.

Do NOT commit/push. When done, notify the orchestrator:
'herdr agent prompt orchestrator TEST PASS phase2' or 'TEST FAIL phase2 <reason>'.
