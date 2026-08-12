# WP-6 Devops Task — Commit AssureCheck WP-6

You are `devops-wp6`. Project: `/c/Users/kutay/Desktop/Projects/Lodestar`.
Run `pwd` first to confirm you are in the repo. When done, notify the orchestrator:
`herdr agent prompt orchestrator 'DONE devops-wp6'`.

## Goal
Commit the WP-6 AssureCheck REST API + compliance dashboard work. All 6 AssureCheck
suites pass (0 failures), smoke OK, no regressions. This is the ONLY agent that commits.

## What to commit
The WP-6 partial work (currently uncommitted in the working tree):
- New: `core/api/AssureCheckApiServer.cpp/h`, `core/assurecheck/DashboardService.cpp/h`,
  `core/test/wp6_assurecheck_tests.cpp`
- Modified: `core/CMakeLists.txt`, `core/adapters/HttpClient.cpp` (Windows socket timeout
  fix), `core/api/HttpServer.cpp`
- Also update `PLAN.md` (mark WP-6 Status=DONE, Committed=<sha>) and
  `docs/loop-state.md` (Status: WP-6 DONE).

## Steps
1. `git status --short` to see the working tree.
2. Stage the WP-6 files (the ones listed above). Do NOT commit unrelated untracked
   research/docs/scripts unless they are part of WP-6. Focus on the WP-6 code + PLAN.md
   + docs/loop-state.md.
3. Commit as: `chore(wp-6): AssureCheck REST API + compliance dashboard — AssureCheckApiServer, DashboardService, wp6 tests, Windows HttpClient socket-timeout fix`
4. Push: `git push` (remote is present).
5. Report the commit SHA.

## Rules
- Only you commit/push. Do NOT weaken tests.
- Commit as `chore(...)`.
