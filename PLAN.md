# Plan — TraceLink Phase 10 (Competitive Gaps)

Purpose: Close all **top + medium** competitive gaps identified in
`docs/tracelink-gap-analysis.md`, **excluding AI and web/browser** (per user). Commercial
grade. Builds on WP-1..WP-8 + v2 WP-A..WP-G (all committed, all tests green).

Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config
Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. TraceLink
module: `core/tracelink/` (TraceLinkService, GraphEngine, RulesEngine, BaselineService,
IoService, ChangeRequestService, StateMachine, ViewModelFactory, Types). REST API:
`core/api/TraceLinkApiServer.cpp` + `ApiKeyService`. Tests: `core/test/wp*_tests.cpp` (all
pass). Schema: append-only migrations in `core/persistence/migrations/` (001-017 exist; new
start at 018). Qt 6.8.2 installed at `/c/Qt/6.8.2/msvc2022_64`. Qt UI views exist in `ui/`
but are NOT built (LODESTAR_BUILD_UI=OFF). To build UI: configure with
`-DCMAKE_PREFIX_PATH=/c/Qt/6.8.2/msvc2022_64 -DLODESTAR_BUILD_UI=ON`.

## Scope (included)
Top: suspect-link workflow, live coverage dashboard. Medium: interactive matrix, baseline
visual diff+rollback, left-nav tree+detail panel, dashboards/charts, document-style
authoring, general review/comment/approval, compliance templates/checklists, TestForge
coverage wiring, user roles/permissions+concurrent editing.

## Scope (EXCLUDED per user)
- **AI** (quality scoring, AI suggestions) — NOT in this phase.
- **Web/browser** presentation — NOT in this phase. UI is the Qt desktop app.

## Work packages

| # | WP | Tasks | Priority | Status | Assigned to | Tests | Committed |
|---|-----|-------|----------|--------|-------------|-------|-----------|
| 1 | Suspect-link workflow | Auto-flag downstream artifacts as `suspect` when a requirement changes; review/clear queue; suspect status on links/entities; migration 013 | High | DONE | senior-engineer-wp1 | wp1_suspect_tests | 19ca1aa |
| 2 | Review/comment/approval | General artifact review + comments + approval (beyond change requests); migration 014 | Med | DONE | senior-engineer-wp2 | wp2_review_tests | b0942f8 |
| 3 | Compliance templates/checklists | Guided OOTB templates/checklists for ARP4754A/ARP4761/DO-178C/DO-254; migration 015 | Med | DONE | senior-engineer-wp3 | wp3_compliance_tests | b0942f8 |
| 4 | Roles/permissions + concurrency | User roles + permissions (RBAC) on entities/links; concurrent-edit safety (optimistic locking/version check); migration 016 | Med | DONE | senior-engineer-wp4 | wp4_rbac_tests | 1297129 |
| 5 | TestForge coverage wiring | Wire TestForge test runs into live coverage (coverage reflects executed results) | Med | DONE | senior-engineer-wp5 | wp5_coverage_tests | fdb42a9 |
| 6 | Qt UI shell | Left-nav project tree + right-side detail/properties panel; enable LODESTAR_BUILD_UI=ON with Qt 6.8.2 | Med | TODO | - | - | - |
| 7 | Coverage dashboard + charts | Live coverage dashboard (red/green gaps) + status/priority/coverage charts | High | TODO | - | - | - |
| 8 | Interactive traceability matrix | Search, filter, saved views, relationship toggling, export | Med | TODO | - | - | - |
| 9 | Baseline visual diff + rollback | Visual compare view + per-item rollback UI | Med | TODO | - | - | - |
| 10 | Document-style authoring | Author requirements in a document context with atomic traceability | Med | TODO | - | - | - |

## Dependency order
- **Engine WPs 1-4 are independent** — run in parallel first.
- WP-5 depends on WP-1 (suspect) lightly + TestForge; can run with engine batch.
- **UI WPs 6-10 depend on the engine WPs** (consume the service surface) and on Qt config.
  WP-6 (UI shell) first, then WP-7 (needs WP-5 + WP-6), WP-8/9/10 (need WP-6).
- Recommended batches: Batch 1 = WP-1,2,3,4,5 (engine, parallel). Batch 2 = WP-6 (UI shell).
  Batch 3 = WP-7,8,9,10 (UI views, parallel after shell).

## Working rules
Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops-<wp> commits/pushes. Update Status/Committed columns and commit as chore(...) after
each WP. Every WP commercial grade: full tests, no regressions, smoke passes. UI WPs must
build with Qt 6.8.2 (CMAKE_PREFIX_PATH + LODESTAR_BUILD_UI=ON) and pass their tests.
