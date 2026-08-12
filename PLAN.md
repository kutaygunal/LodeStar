# Plan — AssureCheck (Phase 11)

Purpose: Build the **AssureCheck** module (currently a stub) into a commercial-grade
compliance-checking engine covering **ARP4754A, ARP4761, DO-178C, DO-254, DO-278A** as much
as possible. Performant, certification-ready, integrated with TraceLink + TestForge.

Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config
Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. AssureCheck
is a stub at `core/assurecheck/stub.cpp` (only `module_version()`). It must become a real
CMake lib target `lodestar_assurecheck`. TraceLink (requirements/design/test/trace graph,
compliance rules) and TestForge (test runs) are DONE and available for integration. Schema:
append-only migrations in `core/persistence/migrations/` (001-017 exist; new start at 018).
Qt 6.8.2 at `/c/Qt/6.8.2/msvc2022_64`. Tests: `core/test/wp*_tests.cpp`.

## Reference
- **Standards checklists:** `docs/assurecheck-standards-checklist.md` (136 items across
  DO-178C A-1..A-7, DO-254, ARP4754A, ARP4761, DO-278A). Seed the engine from this.
- **Architecture:** `docs/architecture.md` — AssureCheck = compliance checks for the
  assurance standards; `assurance_checks` table.

## Scope (commercial grade)
Standards registry + DAL levels, checklist engine (136 objectives), compliance engine
(PASS/FAIL/NA/WARNING + evidence), DAL applicability, evidence collection from
TraceLink/TestForge, certification-ready reports, objective coverage, performance at scale,
REST API + Qt compliance dashboard.

## Work packages

| # | WP | Tasks | Priority | Status | Assigned to | Tests | Committed |
|---|-----|-------|----------|--------|-------------|-------|-----------|
| 1 | Standards + checklist data model | Migration 019: standards registry, checklist items, DAL levels, objectives, evidence requirements. Seed all 136 items from the checklist doc | High | DONE | - | wp1_assurecheck_tests | cca2cf3 |
| 2 | Compliance engine | Run checks against project data; PASS/FAIL/NA/WARNING; DAL applicability; evidence links; assurance_checks storage | High | DONE | - | wp2_assurecheck_tests | e501a77 |
| 3 | Evidence + integration | Pull requirements/design/test/trace data from TraceLink; test-run results from TestForge as verification evidence | High | DONE | - | wp3_assurecheck_tests | cd32045 |
| 4 | Compliance reporting | Certification-ready reports per standard/DAL; objective coverage %; export HTML/CSV/JSON | Med | DONE | - | wp4_assurecheck_tests | e20a89b |
| 5 | Performance + hardening | Indexed checks, batched evaluation, incremental re-check, 10k+ scale, WAL/transactions | Med | TODO | - | - | - |
| 6 | REST API + Qt UI | /assurecheck endpoints; compliance dashboard view (objective coverage, per-standard status) | Med | TODO | - | - | - |

## Dependency order
WP-1 -> WP-2 -> WP-3 -> WP-4 -> WP-5 -> WP-6. WP-1 is the foundation (data model + seed).
WP-2 depends on WP-1. WP-3 depends on WP-2 + TraceLink/TestForge. WP-4 depends on WP-2/3.
WP-5 can run parallel with WP-4. WP-6 depends on WP-1..WP-5.

## Working rules
Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops-<wp> commits/pushes. Update Status/Committed columns and commit as chore(...) after
each WP. Every WP commercial grade: full tests, no regressions, smoke passes. UI WP must
build with Qt 6.8.2 (CMAKE_PREFIX_PATH + LODESTAR_BUILD_UI=ON).
