# Planner Task — Sprint 1 "Make it run"

You are the **planner** for the Lodestar Sprint 1 loop. The orchestrator is agent
`orchestrator`. Project: `/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Review and finalize the phased plan in `PLAN.md` (already seeded with the Sprint 1 spec).
Confirm the phase breakdown, the Definition of Done, and — critically — the **dependency
graph** so the orchestrator can run independent phases in PARALLEL.

## Context (verified facts)
- C++17 CMake monorepo, MSVC/Windows. Build: `cmake --build build --config Release` (HARD
  TIMEOUT). Self-verify: `./build/core/Release/lodestar_smoke.exe`.
- `LODESTAR_BUILD_UI` defaults OFF in top-level `CMakeLists.txt` (line 12). Qt6/Qt5
  auto-detected in `ui/CMakeLists.txt`. `ui/` has MainWindow + Qt Widgets views.
- `core/adapters/` has SkydelAdapter, LlmAdapter (connect-only stubs, no real `invoke()`).
- `core/riskai/stub.cpp` and `core/integratehub/stub.cpp` are 5-line placeholders.

## The 5 phases (from PLAN.md)
1. Desktop app — enable `LODESTAR_BUILD_UI=ON`, wire MainWindow to service API.
2. Functional adapters — real `invoke()` for Skydel + LLM.
3. RiskAI first slice — hazard -> LLM call -> FMEA table (depends on LLM adapter).
4. IntegrateHub first slice — cross-disciplinary issue/coordination model.
5. Real-time / determinism validation — benchmarks + HIL smoke.

## Deliverable
Update `PLAN.md` so each phase has a clear `Status` and `Committed` column, and add a short
**Dependency / Parallelization** section stating which phases are independent (can run
concurrently) and which depend on others. Then reply to the orchestrator with a one-line
summary of the parallelization groups.

## Rules
- Do NOT implement anything. Planning only.
- Do NOT commit. Only the devops agent commits.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE planner'`.
