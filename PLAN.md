# Plan — AssureCheck WP-6 Finish (Phase 11b)

Purpose: **Finish AssureCheck WP-6** (REST API + compliance dashboard) which was left
incomplete when the previous loop was killed. Fix the 5 failing REST API tests, commit the
partial work, and leave AssureCheck fully complete.

Status: **WP-6 DONE** — all 6 AssureCheck suites pass (0 failures), smoke OK, no regressions.
Committed: 7487b67

Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config
Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. AssureCheck
WP-1..WP-6 are DONE and committed (cca2cf3, e501a77, cd32045, 9ed8398, deb215e, 7487b67).
AssureCheck is COMPLETE.

## WP-6 state (committed)
- New: `core/api/AssureCheckApiServer.cpp/h`, `core/assurecheck/DashboardService.cpp/h`,
  `core/test/wp6_assurecheck_tests.cpp`
- Modified: `core/CMakeLists.txt`, `core/adapters/HttpClient.cpp`, `core/api/HttpServer.cpp`
- The build succeeds. All 6 AssureCheck suites pass (0 failures each):
  `lodestar_wp1..wp6_assurecheck_tests`. Smoke passes; no regressions.
- All REST API tests PASS: GET /standards, GET /standards/{code}, POST /checks,
  GET /summary?standard=, GET /dashboard, plus DashboardService unit tests.

## Task (single WP)
Fix the 5 failing REST API tests. Likely causes to investigate (in order):
1. **Server lifecycle / port reuse** — each test starts its own `HttpServer` on an ephemeral
   port (`start(0)`). T1/T2 pass but T3/T4/T5 fail, which suggests the first server's port is
   not released before the next server starts, so the client connects to a stale/closed port.
   Check `HttpServer::stop()` and ephemeral port allocation.
2. **Route handling** — verify POST /checks (body parsing), GET /summary (query param
   `?standard=` parsed into `req.params`), and GET /dashboard are correctly registered and
   dispatched.
3. **HttpClient** — verify POST with a JSON body and query-string GET work over the wire.

Do NOT weaken the test assertions. Make the feature satisfy them.

## Acceptance (MET)
- All 6 AssureCheck suites pass: `lodestar_wp1..wp6_assurecheck_tests` (0 failures each).
- Smoke passes; no regressions in existing suites.
- Commit the WP-6 work (AssureCheckApiServer, DashboardService, wp6 tests, CMake,
  HttpClient/HttpServer changes) as `chore(wp-6): ...`.
- Update PLAN.md + docs/loop-state.md to mark AssureCheck COMPLETE.

## Working rules
Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops commits/pushes. Commit as chore(...).
