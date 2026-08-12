# Scrum-Master Task — S1 Phase 3 & Phase 5 test specs

You are the **scrum-master** for the Lodestar Sprint 1 loop. The orchestrator is agent
`orchestrator`. Project: `/c/Users/kutay/Desktop/Projects/Lodestar`.

## Context
Group A (Phases 1, 2, 4) is DONE and committed. Phase 2 (functional adapters) landed the real
LLM and Skydel `invoke()`. Now write test contracts for the two remaining phases so their
engineers can implement them.

## Your job
Write two test-spec files (test contracts, NOT implementation):

1. **`docs/s1-phase3-test.md`** — RiskAI first slice. Deliverable: hazard input → real LLM
   call → FMEA table. It depends on the LLM adapter (Phase 2). Define a `RiskAiService` that
   takes a hazard description, calls the LLM adapter (`invoke("complete", ...)`), and returns
   a structured FMEA table (rows: failure mode, effect, severity, likelihood, risk). Include
   a deterministic fallback so the suite passes even when no live LLM server is running
   (e.g. `[SKIP]` for the live-LLM assertion, but a deterministic canned-FMEA path always
   runs). Specify the test file `core/test/s1_phase3_tests.cpp`, target
   `lodestar_s1_phase3_tests`, links (`lodestar_common`, `lodestar_adapters`), and the CMake
   registration snippet. Give 4–6 concrete test cases (T1..T6) with expected behavior.

2. **`docs/s1-phase5-test.md`** — Real-time / determinism validation. Deliverable: recorded
   benchmark numbers. Define a benchmark harness that measures a core operation (e.g.
   TraceLink graph query or a determinism check) and records numbers to a file
   (`docs/reports/s1-phase5-benchmarks.md`). Specify the test file
   `core/test/s1_phase5_tests.cpp`, target `lodestar_s1_phase5_tests`, links, and CMake
   registration. Give 3–5 concrete test cases (T1..T5) including a determinism check (same
   input → same output) and a benchmark that prints/records timing.

## Rules
- Do NOT implement. Test specs only.
- Do NOT commit. Only the devops agent commits.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE scrum-master'`.
