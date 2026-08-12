# Plan — Sprint 1: "Make it run"

Purpose: **Turn the verified Lodestar engineering core into a runnable, demoable product.**
This is Sprint 1 of the 4-sprint plan (see `docs/reports/sprint-plan.html`). It delivers the
P0 "make it run" layer: a runnable desktop app, functional adapters, and the two headline
differentiators (RiskAI, IntegrateHub) no longer stubs.

Status: **IN PROGRESS**
Context: Lodestar C++17 CMake monorepo (MSVC/Windows). Build: `cmake --build build --config
Release` (HARD TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`. AssureCheck
WP-1..WP-6 are DONE and committed. TraceLink, ScenarioForge, AssureCheck are the mature core.

## Sprint 1 scope (from sprint-plan.html)

| # | Work item | Module | Priority | Effort |
|---|-----------|--------|----------|--------|
| S1.1 | Build the desktop Qt app (enable `LODESTAR_BUILD_UI`, wire MainWindow to service API) | Platform | P0 | 6–10 wks |
| S1.2 | Make adapters functional (real `invoke()` for Skydel + LLM) | Platform | P0 | 6–10 wks |
| S1.3 | Implement RiskAI first slice (LLM-assisted FMEA/hazard) | Platform | P0 | 6–8 wks |
| S1.4 | Implement IntegrateHub first slice (cross-disciplinary hub) | Platform | P0 | 4–6 wks |
| S1.5 | Validate real-time / determinism (benchmarks + HIL smoke) | Platform | P0 | 4–6 wks |

## Phase breakdown (loop phases)

| Phase | Work item | Status | Committed | Depends on |
|-------|-----------|--------|-----------|------------|
| 1 | Desktop app — enable `LODESTAR_BUILD_UI=ON`, wire `MainWindow` to service API so the app opens and shows TraceLink data. Deliverable: runnable desktop app. | NOT STARTED | — | none (core service API exists) |
| 2 | Functional adapters — real `invoke()` for Skydel + LLM. Deliverable: one end-to-end RF injection (or simulated) + a real LLM call. | NOT STARTED | — | none |
| 3 | RiskAI first slice — hazard input → LLM call → FMEA table. Deliverable: working LLM-assisted FMEA. | NOT STARTED | — | Phase 2 (LLM adapter) |
| 4 | IntegrateHub first slice — cross-disciplinary issue/coordination model. Deliverable: working issue/coordination model. | NOT STARTED | — | none |
| 5 | Real-time / determinism validation — real-time benchmarks + HIL smoke test. Deliverable: recorded benchmark numbers. | NOT STARTED | — | Phase 2 (Skydel adapter) |

## Dependency / Parallelization

**Dependency graph (edges = must finish before dependent starts):**

```
Phase 1 (Desktop app)   ── independent
Phase 2 (Adapters)      ── independent  ──┬──▶ Phase 3 (RiskAI, needs LLM adapter)
Phase 4 (IntegrateHub)  ── independent  └──▶ Phase 5 (RT/determinism, needs Skydel adapter)
```

**Parallelization groups (run concurrently):**

- **Group A (parallel, independent):** Phase 1, Phase 2, Phase 4. All three touch disjoint
  areas (UI shell, adapters, IntegrateHub model) and can run in parallel immediately.
- **Group B (after Phase 2 LLM part):** Phase 3. Cannot start until the LLM adapter has a real
  `invoke()`/`doComplete()`.
- **Group C (after Phase 2 Skydel part):** Phase 5. Cannot start until the Skydel adapter has a
  real `invoke()` for RF injection.

**Critical path:** Phase 2 → Phase 3 and Phase 2 → Phase 5. Phase 2 is the single bottleneck;
its LLM and Skydel halves can be developed in parallel within the phase. Phase 1 and Phase 4
are fully off the critical path and can be scheduled anytime.

**Recommended scheduling:** start Phase 1, Phase 2, and Phase 4 together. Kick off Phase 3 as
soon as the LLM adapter lands, and Phase 5 as soon as the Skydel adapter lands. This keeps all
engineers busy and minimizes total wall-clock time.

## Definition of done (Sprint 1)

- A runnable desktop app opens and shows TraceLink data.
- RiskAI produces an FMEA table from a hazard via a real LLM call.
- One vendor adapter drives a real (or simulated) RF injection.
- IntegrateHub has a working issue/coordination model.
- Real-time benchmark numbers are recorded.

## Working rules

Follow docs/working-rules.md. Build with HARD TIMEOUT, run tests ONE AT A TIME. Only
devops commits/pushes. Commit as chore(...).
