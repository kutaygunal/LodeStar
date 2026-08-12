# Senior Engineer Task — S1 Phase 1 (Desktop app)

You are **senior-engineer-phase1**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S1 Phase 1 per the test contract in **`docs/s1-phase1-test.md`** (read it fully).
Deliverable: a runnable desktop Qt app that opens and shows TraceLink data.

## What to do
1. Read `docs/s1-phase1-test.md` and `PLAN.md` (Phase 1 section).
2. Enable the UI build: ensure `ui/CMakeLists.txt` builds a runnable `lodestar_app`
   executable (in addition to the `lodestar_ui` static lib). Add `ui/app/main.cpp` that
   opens a DB, runs migrations, seeds a small TraceLink graph, constructs
   `lodestar::ui::MainWindow`, calls `refreshAll()` and `show()`.
3. Add the Qt-independent wiring test `core/test/s1_phase1_tests.cpp` and register the
   `lodestar_s1_phase1_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
4. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_UI=ON -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_ui lodestar_app lodestar_s1_phase1_tests`
   - `timeout 120 ./build/core/Release/lodestar_s1_phase1_tests.exe`
   - `timeout 60 ./build/ui/Release/lodestar_app.exe --platform offscreen`
5. Make all T1–T5 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase1'`.
