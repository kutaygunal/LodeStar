# Senior Engineer Task — S1 Phase 3 (RiskAI first slice)

You are **senior-engineer-phase3**. The orchestrator is agent `orchestrator`. Project:
`/c/Users/kutay/Desktop/Projects/Lodestar`.

## Your job
Implement S1 Phase 3 per the test contract in **`docs/s1-phase3-test.md`** (read it fully).
Deliverable: working LLM-assisted FMEA (hazard → LLM call → FMEA table). Depends on the
Phase 2 LLM adapter (DONE, committed).

## What to do
1. Read `docs/s1-phase3-test.md` and `PLAN.md` (Phase 3 section).
2. Replace `core/riskai/stub.cpp` with a real `RiskAiService` (new
   `core/riskai/RiskAiService.h`) with the `FmeaRow` struct and `analyze(hazard)` method
   exactly as the contract specifies. It must call `llm.invoke("complete", ...)`, parse the
   reply into FMEA rows, compute `risk = severity * likelihood`, and fall back to a
   deterministic canned table on LLM failure.
3. Register the `RiskAiService` sources in the `lodestar_riskai` module in
   `core/CMakeLists.txt` (replacing `stub.cpp`), linking `lodestar_adapters`.
4. Add the test `core/test/s1_phase3_tests.cpp` and register the `lodestar_s1_phase3_tests`
   target in `core/CMakeLists.txt` (inside `if(LODESTAR_BUILD_TESTS)`) exactly as the
   contract specifies (link `ws2_32` on Windows).
5. Build and run ONE AT A TIME with HARD TIMEOUTS:
   - `cmake -S . -B build -DLODESTAR_BUILD_TESTS=ON`
   - `timeout 600 cmake --build build --config Release --target lodestar_s1_phase3_tests`
   - `timeout 120 ./build/core/Release/lodestar_s1_phase3_tests.exe`
6. Make T1–T6 pass. T3 is `[SKIP]` when no live Ollama server; T1/T2/T4/T5/T6 must always
   pass.

## Rules
- Do NOT commit/push. Only the devops agent commits.
- Run commands with HARD TIMEOUTS, ONE AT A TIME. Never `find /`.
- When done, notify the orchestrator: `herdr agent prompt orchestrator 'DONE senior-engineer-phase3'`.
