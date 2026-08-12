# Plan — Sprint 2: "Make it collaborative & trustworthy"

Purpose: **Close ALL remaining gaps from the post-Sprint-1 PM recheck** (`docs/reports/sprint1-gap-recheck.html`). This is Sprint 2 of the 4-sprint plan. It covers every item the recheck marked NOT FIXED or PARTIAL, plus the P2 differentiators, and ends with a fresh PM re-assessment.

Status: **IN PROGRESS**
Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. Sprint 1 DONE (runnable app, functional Skydel+LLM adapters, RiskAI, IntegrateHub, RT/determinism benchmarks). TraceLink, ScenarioForge, AssureCheck are the mature core.

## Sprint 2 scope — ALL remaining gaps (from sprint1-gap-recheck.html)

| # | Work item | Module | Priority | Effort |
|---|-----------|--------|----------|--------|
| S2.1 | User model + roles + permissions + concurrent editing | Platform | P0 | 8–12 wks |
| S2.2 | Web / browser layer over the REST API | Platform | P0 | 8–12 wks |
| S2.3 | AssureCheck workflow + audit + evidence package | AssureCheck | P0 | 6–10 wks |
| S2.4 | AssureCheck semantic evidence evaluation | AssureCheck | P0 | 4–6 wks |
| S2.5 | TestForge test-case design intelligence | TestForge | P0 | 4–6 wks |
| S2.6 | Wire functional RF adapters into TestForge execution | TestForge | P1 | 2–3 wks |
| S2.7 | Structural code coverage (statement/decision/MC/DC) | TestForge | P1 | 6–10 wks |
| S2.8 | Certification-ready reporting + traceability | TestForge | P1 | 4–6 wks |
| S2.9 | Full CI/CD (test agents, matrix builds, coverage) | Platform | P1 | 2–4 wks |
| S2.10 | Commercial packaging (licensing, installers, docs, support) | Platform | P1 | 4–6 wks |
| S2.11 | ScenarioForge software-defined baseband (I/Q) + automation API | ScenarioForge | P1 | 6–10 wks |
| S2.12 | OSLC integration | Platform | P2 | 2–3 wks |
| S2.13 | AI quality scoring on requirements | Platform | P2 | 3–4 wks |
| S2.14 | ScenarioForge trajectory + multipath/interference | ScenarioForge | P2 | 4–6 wks |
| S2.15 | Guided compliance templates/checklists | AssureCheck | P2 | 2–3 wks |
| S2.16 | Variants / branching | Platform | P2 | 4–6 wks |
| S2.17 | **Fresh PM re-assessment** (new PM, no prior report, then given previous report → latest gap report) | Docs | — | — |

## Phase breakdown (loop phases)

- **Phase 1 — User model + RBAC.** Surface the existing RBAC service as real accounts with login, roles, permissions, and concurrent-editing conflict handling. Prerequisite for web layer.
- **Phase 2 — Web / browser layer.** Read/review layer over the REST API (fastest path to collaboration).
- **Phase 3 — AssureCheck workflow + audit + evidence package.** Review/approval/sign-off with real timestamps/actors (fix the `"now"` placeholder), objective→evidence package (reuse TraceLink's).
- **Phase 4 — AssureCheck semantic evidence evaluation.** Replace "row exists" with objective-specific evaluation rules.
- **Phase 5 — TestForge test-case design intelligence.** Equivalence + boundary derivation from a requirement/objective.
- **Phase 6 — Wire RF adapters into TestForge execution.** Connect SkydelAdapter::invoke() to IMeasurementProvider.
- **Phase 7 — Structural code coverage.** Statement/decision first, MC/DC next (or integrate external engine).
- **Phase 8 — Certification-ready reporting + traceability.** PDF/Word/ReQIF export + result→requirement traceability.
- **Phase 9 — Full CI/CD.** Wire per-phase test targets into the pipeline with a test gate + matrix.
- **Phase 10 — Commercial packaging.** Licensing, installers, end-user docs, support model.
- **Phase 11 — ScenarioForge baseband + automation API.** I/Q sample generation + Python/REST/SCPI-style remote control.
- **Phase 12 — OSLC integration.** Expose/consume OSLC.
- **Phase 13 — AI quality scoring on requirements.** Builds on duplicate detection + functional LLM adapter.
- **Phase 14 — ScenarioForge trajectory + multipath/interference.** Waypoints/6-DOF + RF impairments.
- **Phase 15 — Guided compliance templates/checklists.** OOTB ARP4754A/DO-178C templates.
- **Phase 16 — Variants / branching.** Product-line engineering.
- **Phase 17 — Fresh PM re-assessment.** Spawn a NEW PM subagent with NO knowledge of prior reports. It does its own independent gap analysis of the current source. THEN give it the previous report (sprint1-gap-recheck.html) and have it produce the latest report of what is still missing. Deliverable: `docs/reports/sprint2-gap-recheck.html`.

## Definition of done (Sprint 2)

- All 16 implementation phases DONE + committed, tests 0 failures, smoke OK.
- A fresh PM (no prior context) independently assesses the product, then compares against the previous report and produces `docs/reports/sprint2-gap-recheck.html` listing what is still missing.

## Working rules

Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only devops commits/pushes. Commit as chore(...).
