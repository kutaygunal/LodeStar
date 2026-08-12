# WP-6 Engineer Task — Fix 5 Failing AssureCheck REST API Tests

You are `senior-engineer-wp6`. Project: `/c/Users/kutay/Desktop/Projects/Lodestar`.
Run `pwd` first to confirm you are in the repo. When done, notify the orchestrator:
`herdr agent prompt orchestrator 'DONE senior-engineer-wp6'`.

## Goal
Fix the 5 failing REST API tests in `lodestar_wp6_assurecheck_tests` so ALL 6
AssureCheck suites pass (wp1..wp6), smoke passes, and no regressions. Do NOT weaken
any test assertion — make the feature satisfy the tests.

## Current state
- Build succeeds: `cmake --build build --config Release` (HARD TIMEOUT ~600s).
- Service-layer tests PASS (runChecks, storeResults, dashboard).
- 5 REST API tests FAIL in `lodestar_wp6_assurecheck_tests`:
  - `POST /assurecheck/checks -> 200` (FAIL)
  - `POST /assurecheck/checks missing standard -> 400` (FAIL)
  - `GET /assurecheck/summary?standard=DO-178C -> 200` (FAIL)
  - `GET /assurecheck/summary without standard -> 400` (FAIL)
  - `GET /assurecheck/dashboard -> 200` (FAIL)
- `GET /assurecheck/standards` and `GET /assurecheck/standards/{code}` PASS.

## Files
- Tests: `core/test/wp6_assurecheck_tests.cpp` (read the CONTRACT at top).
- Server impl: `core/api/AssureCheckApiServer.cpp/h`
- HTTP server: `core/api/HttpServer.cpp/h`
- HTTP client: `core/adapters/HttpClient.cpp`
- Dashboard: `core/assurecheck/DashboardService.cpp/h`

## How to run the failing test
Run ONE test at a time with a HARD TIMEOUT (working rules). From the repo root:
```bash
timeout 300 ./build/core/Release/lodestar_wp6_assurecheck_tests.exe core/persistence/migrations
```
(Adjust the exe path if the build layout differs — check `build/core/`.)

## Likely root causes to investigate (in order)
1. **Server lifecycle / port reuse.** Each test starts its own `HttpServer` on an
   ephemeral port (`start(0)`). T1/T2 pass but T3/T4/T5 fail. The common factor in
   T3/T4/T5 is the `POST /assurecheck/checks` call. Investigate whether the POST
   request (JSON body) is received correctly, and whether the server's accept thread
   / port is left in a bad state after a POST. Check `HttpServer::stop()` and the
   accept loop for races.
2. **POST body parsing.** Verify `POST /assurecheck/checks` receives the JSON body
   (`{"standard":"DO-178C","dal":"A"}`) and returns 200 with a `results` array of 82
   entries. If the body is empty, `Json::parse("")` throws -> 400. Check
   Content-Length handling in `HttpServer::handleClient` and the HttpClient send.
3. **Query param parsing.** Verify `GET /assurecheck/summary?standard=DO-178C` parses
   the `standard` query param into `req.params` and returns 200 with `summary.total==82`.
4. **Route dispatch.** Verify POST /checks, GET /summary, GET /dashboard are correctly
   registered and dispatched (params preserved through `dispatch`).

## Debugging
The code already has `[DBG]` fprintf(stderr) lines in `AssureCheckApiServer.cpp`,
`HttpServer.cpp`, and `HttpClient.cpp`. Run the test and read stderr to see the actual
status/body. Add more debug output if needed, but REMOVE or keep it minimal before
finalizing (it goes to stderr, not the response body, so it won't break assertions).

## Acceptance
- `lodestar_wp6_assurecheck_tests` -> 0 failures.
- All 6 AssureCheck suites (wp1..wp6) -> 0 failures each.
- Smoke passes: `./build/core/Release/lodestar_smoke.exe`.
- No regressions in existing suites.
- Do NOT weaken test assertions.

## Rules
- Do NOT commit or push (that is devops's job).
- Build with HARD TIMEOUT. Run tests ONE AT A TIME.
- If a test hangs, stop and fix rather than re-running it.
