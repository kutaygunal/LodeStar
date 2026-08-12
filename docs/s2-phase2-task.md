# Senior Engineer Task — S2 Phase 2 (Web / browser layer over the REST API)

You are **senior-engineer-phase2**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 2 per the test contract in **`docs/s2-phase2-test.md`** (read it fully).
Deliverable: a web / browser read-review layer over the REST API (fastest path to
collaboration). Depends on Phase 1 (RBAC) which is DONE.

## Background (read these first)
- `PLAN.md` Phase 2 section.
- `core/api/ApiServer.h/.cpp` — REST API with API-key auth + Phase 1 auth endpoints.
- `ui/` — the Qt desktop app (reference for what data to expose).
- `python/` — existing Python tooling (reference for a web server approach).

## What to do
1. Read `docs/s2-phase2-test.md` and `PLAN.md` (Phase 2).
2. Add a **web layer**: a lightweight HTTP server (reuse `core/api/HttpServer.h/.cpp`) that
   serves a browser-based read/review UI over the REST API. At minimum:
   - `GET /web/` serves an HTML page.
   - `GET /web/requirements` returns requirements as HTML/JSON for review.
   - `GET /web/trace` returns the trace matrix as HTML/JSON.
   - `GET /web/assure` returns AssureCheck compliance summary as HTML/JSON.
3. The web layer must honor the Phase 1 auth (login/roles) — a viewer can read, an editor
   can review.
4. Add the test file `core/test/s2_phase2_tests.cpp` and register the
   `lodestar_s2_phase2_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase2_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase2_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase2'`.
