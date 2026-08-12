# Senior Engineer Task — S2 Phase 10 (Commercial packaging)

You are **senior-engineer-phase10**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 10 per the test contract in **`docs/s2-phase10-test.md`** (read it fully).
Deliverable: commercial packaging — licensing, installers, end-user docs, support model.

## Background (read these first)
- `PLAN.md` Phase 10 section.
- `README.md` — current product positioning.
- `CMakeLists.txt` — build config.
- `ui/` — the desktop app to package.

## What to do
1. Read `docs/s2-phase10-test.md` and `PLAN.md` (Phase 10).
2. Add a **license** file (e.g. `LICENSE` / `LICENSE.md`) describing the commercial license
   model (e.g. proprietary, per-seat, evaluation).
3. Add an **installer** script/config (e.g. `packaging/installer.ps1` or a CPack config in
   `CMakeLists.txt`) that packages the built app + DLLs into an installable bundle.
4. Add **end-user documentation** (e.g. `docs/user-guide.md`) covering install, run, and
   the main features.
5. Add a **support model** doc (e.g. `docs/support.md`) describing support tiers/contact.
6. Add the test file `core/test/s2_phase10_tests.cpp` and register the
   `lodestar_s2_phase10_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
7. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase10_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase10_tests.exe`
8. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase10'`.
