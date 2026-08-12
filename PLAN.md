# Plan — TraceLink v2 (Phase 9)

Purpose: Add all requested new functionalities and robustness hardening to the TraceLink
module in ONE phase, commercial grade. Builds on the completed WP-1..WP-8 (all committed and
pushed). RiskAI remains OUT OF SCOPE.

Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config
Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. TraceLink
module: `core/tracelink/` (TraceLinkService, GraphEngine, RulesEngine, BaselineService,
IoService, StateMachine, ViewModelFactory, Types). REST API: `core/api/TraceLinkApiServer.cpp`.
Tests: `core/test/wp*_tests.cpp` (all currently pass). Schema: append-only migrations in
`core/persistence/migrations/` (001-009 exist). Qt UI views exist in `ui/` but are NOT built
(LODESTAR_BUILD_UI=OFF, Qt not installed).

## Work packages (all in this phase)

| # | WP | Tasks (from options) | Priority | Status | Assigned to | Tests | Committed |
|---|-----|----------------------|----------|--------|-------------|-------|-----------|
| A | Search + pagination | A1 FTS5 full-text search (ranked); B1 pagination (limit/offset) on all list endpoints | High | DONE | senior-engineer-wpa | PASS (0 failures) | 7dc71d2 |
| B | Change management | A3 baseline restore/rollback; A4 change-request + review workflow (approve/reject, review queues, link CRs to audit) | High | TODO | - | - | - |
| C | Hierarchy tree | A2 requirement hierarchy tree: parent/child navigation, subtree ops, reorder | High | DONE | senior-engineer-wpc | PASS (30/30) | 3d5e4b7 |
| D | Coverage + evidence | A5 coverage by verification method; A6 DO-178C evidence package export (matrix+coverage+validation+audit bundle) | Med | DONE | senior-engineer-wpd | PASS (0 failures) | 2fb8451 |
| E | API + duplicates | A8 REST API auth / API keys; A9 duplicate/similarity detection | Med | TODO | - | - | - |
| F | Robustness hardening | B2 typed error codes; B3 input validation & size limits; B4 DB backup/restore; B5 migration safety (dry-run/rollback/checksum); B6 fuzz/edge-case tests; B7 concurrency stress test; B8 structured logging | High | DONE | senior-engineer-wpf | PASS (37/37) | bf45a16 |
| G | Real Qt UI | A7 install Qt, enable LODESTAR_BUILD_UI=ON, wire the 4 views (matrix/graph/impact/coverage) to the service | Med | TODO | - | - | - |

## Dependency order
WP-A, WP-C, WP-F are independent and can run in parallel. WP-B depends on WP-A (pagination
for review queues) lightly; WP-D depends on WP-C (hierarchy for evidence) and WP-A. WP-E
depends on WP-A (pagination) and WP-F (error codes). WP-G depends on WP-A..WP-F (needs the
full service surface). Recommended: run A, C, F in parallel first; then B, D, E; then G.

## Working rules
Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops-<wp> commits/pushes. Update Status/Committed columns and commit as chore(...) after
each WP. Every WP must be commercial grade: full tests, no regressions, smoke passes.
