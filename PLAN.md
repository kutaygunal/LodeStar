# Plan

Purpose: Execute the TraceLink commercial-grade plan (docs/tracelink-plan.md) in the existing
Lodestar C++ core. Implement WP-1 through WP-8 in dependency order. RiskAI is deferred to the
very last phase (out of scope for this loop).

Context: Lodestar is a C++17 CMake monorepo (MSVC/Windows) with an existing working core
(common, persistence, tracelink, scenario, adapters, api, testforge). Build: `cmake --build
build --config Release`. Self-verify: `./build/core/Release/lodestar_smoke.exe`. Each module
is a CMake static lib target (`lodestar_*`). Schema is append-only migrations in
`core/persistence/migrations/`. The thin REST API lives in `core/api/ApiServer.cpp`.

| # | WP | Description | Priority | Status | Assigned to | Tests | Committed |
|---|-----|-------------|----------|--------|-------------|-------|-----------|
| 1 | Domain model + schema | Rich typed entities + typed links + metadata; migrations 003/004; DAO CRUD/update/soft-delete/search; status state machines; integrity on write (dangling/duplicate/self-loop/relation-type) | High | TODO | - | - | - |
| 2 | Graph engine | upstream/downstream closure, impactAnalysis, coverage/coverageGap, traceMatrix, reverse-relation mapping | High | TODO | - | - | - |
| 3 | Rules engine + validation | rule data model, evaluation, built-in templates (REQ_MUST_BE_VERIFIED etc.), validation_runs + compliance_violations, standard tagging | High | TODO | - | - | - |
| 4 | Audit + baselines + diff | audit_log writes on every mutation; Baselines snapshots; diffBaseline(a,b); history; entityAtBaseline | Medium | TODO | - | - | - |
| 5 | Import/Export | CSV matrix+entities export, HTML report, ReqIF import+export, non-destructive import with batch+log | Medium | TODO | - | - | - |
| 6 | REST API | all /tracelink routes in ApiServer; smoke each endpoint | Medium | TODO | - | - | - |
| 7 | Qt UI views | matrix view, graph view, impact view, coverage/compliance dashboard | Medium | TODO | - | - | - |
| 8 | Commercial hardening | WAL mode, BEGIN IMMEDIATE transactions, performance indexes, 10k-node perf, docs | Low | TODO | - | - | - |

## Dependency order
WP-1 -> WP-2 -> WP-3 -> WP-4 -> WP-5/6 -> WP-7/8. WP-4 audit can start parallel with WP-3.

## Working rules
Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops-<phase> commits/pushes. Update Status/Committed columns and commit as chore(...)
after each phase.
