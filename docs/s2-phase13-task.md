# Senior Engineer Task — S2 Phase 13 (AI quality scoring on requirements)

You are **senior-engineer-phase13**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S2 Phase 13 per the test contract in **`docs/s2-phase13-test.md`** (read it fully).
Deliverable: AI quality scoring on requirements — builds on duplicate detection + the
functional LLM adapter.

## Background (read these first)
- `PLAN.md` Phase 13 section.
- `core/riskai/RiskAiService.h/.cpp` — LLM-assisted analysis (reference for LLM usage).
- `core/adapters/LlmAdapter.h/.cpp` — functional LLM adapter (real HTTP to Ollama).
- `core/tracelink/` — requirements model + existing duplicate detection.

## What to do
1. Read `docs/s2-phase13-test.md` and `PLAN.md` (Phase 13).
2. Add a **requirement quality scorer**: given a requirement, score it on quality
   dimensions (clarity, testability, atomicity, completeness, absence of ambiguity) —
   each 0–100, plus an overall score.
3. Use the LLM adapter for semantic quality assessment, with a deterministic fallback
   (e.g. heuristic based on length, presence of "shall", single-verb, etc.) when the LLM
   is unavailable.
4. Add the test file `core/test/s2_phase13_tests.cpp` and register the
   `lodestar_s2_phase13_tests` target in `core/CMakeLists.txt` (inside the
   `if(LODESTAR_BUILD_TESTS)` block) exactly as the contract specifies.
5. Build and run the tests ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s2_phase13_tests`
   - `timeout 120 ./build/core/Release/lodestar_s2_phase13_tests.exe`
6. Make all T1–T4 pass. Do NOT weaken the assertions.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase13'`.
